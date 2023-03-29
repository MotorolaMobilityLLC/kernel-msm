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
 * DOC: csr_api_roam.c
 *
 * Implementation for the Common Roaming interfaces.
 */
#include "ani_global.h"          /* for struct mac_context **/
#include "wma_types.h"
#include "wma_if.h"          /* for STA_INVALID_IDX. */
#include "csr_inside_api.h"
#include <include/wlan_psoc_mlme.h>
#include "sme_trace.h"
#include "sme_qos_internal.h"
#include "sme_inside.h"
#include "host_diag_core_event.h"
#include "host_diag_core_log.h"
#include "csr_api.h"
#include "csr_internal.h"
#include "cds_reg_service.h"
#include "mac_trace.h"
#include "csr_neighbor_roam.h"
#include "cds_regdomain.h"
#include "cds_utils.h"
#include "sir_types.h"
#include "cfg_ucfg_api.h"
#include "sme_power_save_api.h"
#include "wma.h"
#include "wlan_policy_mgr_api.h"
#include "sme_nan_datapath.h"
#include "pld_common.h"
#include "wlan_reg_services_api.h"
#include "qdf_crypto.h"
#include <wlan_logging_sock_svc.h>
#include "wlan_objmgr_psoc_obj.h"
#include <wlan_scan_ucfg_api.h>
#include <wlan_cp_stats_mc_ucfg_api.h>
#include <wlan_tdls_tgt_api.h>
#include <wlan_cfg80211_scan.h>
#include <wlan_scan_public_structs.h>
#include <wlan_action_oui_public_struct.h>
#include <wlan_action_oui_ucfg_api.h>
#include "wlan_mlme_api.h"
#include "wlan_mlme_ucfg_api.h"
#include <wlan_utility.h>
#include "cfg_mlme.h"
#include "wlan_mlme_public_struct.h"
#include <wlan_crypto_global_api.h>
#include "wlan_qct_sys.h"
#include "wlan_blm_api.h"
#include "wlan_policy_mgr_i.h"
#include "wlan_scan_utils_api.h"
#include "wlan_p2p_cfg_api.h"
#include "cfg_nan_api.h"
#include "nan_ucfg_api.h"
#include "wlan_reg_ucfg_api.h"

#include <ol_defines.h>
#include "wlan_pkt_capture_ucfg_api.h"
#include "wlan_psoc_mlme_api.h"
#include "wlan_cm_roam_api.h"
#include "wlan_if_mgr_public_struct.h"
#include "wlan_if_mgr_ucfg_api.h"
#include "wlan_if_mgr_roam.h"
#include "wlan_roam_debug.h"
#include "wlan_cm_roam_public_struct.h"
#include "wlan_mlme_twt_api.h"
#include "wlan_cmn_ieee80211.h"

#define RSN_AUTH_KEY_MGMT_SAE           WLAN_RSN_SEL(WLAN_AKM_SAE)
#define MAX_PWR_FCC_CHAN_12 8
#define MAX_PWR_FCC_CHAN_13 2

/* 70 seconds, for WPA, WPA2, CCKM */
#define CSR_WAIT_FOR_KEY_TIMEOUT_PERIOD     \
	(SIR_INSTALL_KEY_TIMEOUT_SEC * QDF_MC_TIMER_TO_SEC_UNIT)
/* 120 seconds, for WPS */
#define CSR_WAIT_FOR_WPS_KEY_TIMEOUT_PERIOD (120 * QDF_MC_TIMER_TO_SEC_UNIT)

#define CSR_SINGLE_PMK_OUI               "\x00\x40\x96\x03"
#define CSR_SINGLE_PMK_OUI_SIZE          4

/* Flag to send/do not send disassoc frame over the air */
#define CSR_DONT_SEND_DISASSOC_OVER_THE_AIR 1
#define RSSI_HACK_BMPS (-40)
#define MAX_CB_VALUE_IN_INI (2)

#define MAX_SOCIAL_CHANNELS  3

/* packet dump timer duration of 60 secs */
#define PKT_DUMP_TIMER_DURATION 60

/* Choose the largest possible value that can be accommodated in 8 bit signed */
/* variable. */
#define SNR_HACK_BMPS                         (127)

/*
 * ROAMING_OFFLOAD_TIMER_START - Indicates the action to start the timer
 * ROAMING_OFFLOAD_TIMER_STOP - Indicates the action to stop the timer
 * CSR_ROAMING_OFFLOAD_TIMEOUT_PERIOD - Timeout value for roaming offload timer
 */
#define ROAMING_OFFLOAD_TIMER_START	1
#define ROAMING_OFFLOAD_TIMER_STOP	2
#define CSR_ROAMING_OFFLOAD_TIMEOUT_PERIOD    (5 * QDF_MC_TIMER_TO_SEC_UNIT)

/*
 * Neighbor report offload needs to send 0xFFFFFFFF if a particular
 * parameter is disabled from the ini
 */
#define NEIGHBOR_REPORT_PARAM_INVALID (0xFFFFFFFFU)

/*
 * To get 4 LSB of roam reason of roam_synch_data
 * received from firmware
 */
#define ROAM_REASON_MASK 0x0F
/**
 * csr_get_ielen_from_bss_description() - to get IE length
 *             from struct bss_description structure
 * @pBssDescr: pBssDescr
 *
 * This function is called in various places to get IE length
 * from struct bss_description structure
 *
 * @Return: total IE length
 */
static inline uint16_t
csr_get_ielen_from_bss_description(struct bss_description *pBssDescr)
{
	uint16_t ielen;

	if (!pBssDescr)
		return 0;

	/*
	 * Length of BSS desription is without length of
	 * length itself and length of pointer
	 * that holds ieFields
	 *
	 * <------------sizeof(struct bss_description)-------------------->
	 * +--------+---------------------------------+---------------+
	 * | length | other fields                    | pointer to IEs|
	 * +--------+---------------------------------+---------------+
	 *                                            ^
	 *                                            ieFields
	 */

	ielen = (uint16_t)(pBssDescr->length + sizeof(pBssDescr->length) -
			   GET_FIELD_OFFSET(struct bss_description, ieFields));

	return ielen;
}

#ifdef WLAN_FEATURE_SAE
/**
 * csr_sae_callback - Update SAE info to CSR roam session
 * @mac_ctx: MAC context
 * @msg_ptr: pointer to SAE message
 *
 * API to update SAE info to roam csr session
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS csr_sae_callback(struct mac_context *mac_ctx,
		tSirSmeRsp *msg_ptr)
{
	struct csr_roam_info *roam_info;
	uint32_t session_id;
	struct sir_sae_info *sae_info;

	sae_info = (struct sir_sae_info *) msg_ptr;
	if (!sae_info) {
		sme_err("SAE info is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("vdev_id %d "QDF_MAC_ADDR_FMT,
		sae_info->vdev_id,
		QDF_MAC_ADDR_REF(sae_info->peer_mac_addr.bytes));

	session_id = sae_info->vdev_id;
	if (session_id == WLAN_UMAC_VDEV_ID_MAX)
		return QDF_STATUS_E_INVAL;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_FAILURE;

	roam_info->sae_info = sae_info;

	csr_roam_call_callback(mac_ctx, session_id, roam_info,
				   0, eCSR_ROAM_SAE_COMPUTE,
				   eCSR_ROAM_RESULT_NONE);
	qdf_mem_free(roam_info);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS csr_sae_callback(struct mac_context *mac_ctx,
		tSirSmeRsp *msg_ptr)
{
	return QDF_STATUS_SUCCESS;
}
#endif


#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
enum mgmt_auth_type diag_auth_type_from_csr_type(enum csr_akm_type authtype)
{
	int n = AUTH_OPEN;

	switch (authtype) {
	case eCSR_AUTH_TYPE_SHARED_KEY:
		n = AUTH_SHARED;
		break;
	case eCSR_AUTH_TYPE_WPA:
		n = AUTH_WPA_EAP;
		break;
	case eCSR_AUTH_TYPE_WPA_PSK:
		n = AUTH_WPA_PSK;
		break;
	case eCSR_AUTH_TYPE_RSN:
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA256:
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA384:
#ifdef WLAN_FEATURE_11W
	case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
#endif
		n = AUTH_WPA2_EAP;
		break;
	case eCSR_AUTH_TYPE_RSN_PSK:
#ifdef WLAN_FEATURE_11W
	case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
#endif
		n = AUTH_WPA2_PSK;
		break;
#ifdef FEATURE_WLAN_WAPI
	case eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE:
		n = AUTH_WAPI_CERT;
		break;
	case eCSR_AUTH_TYPE_WAPI_WAI_PSK:
		n = AUTH_WAPI_PSK;
		break;
#endif /* FEATURE_WLAN_WAPI */
	default:
		break;
	}
	return n;
}

enum mgmt_encrypt_type diag_enc_type_from_csr_type(eCsrEncryptionType enctype)
{
	int n = ENC_MODE_OPEN;

	switch (enctype) {
	case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP40:
		n = ENC_MODE_WEP40;
		break;
	case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP104:
		n = ENC_MODE_WEP104;
		break;
	case eCSR_ENCRYPT_TYPE_TKIP:
		n = ENC_MODE_TKIP;
		break;
	case eCSR_ENCRYPT_TYPE_AES:
		n = ENC_MODE_AES;
		break;
	case eCSR_ENCRYPT_TYPE_AES_GCMP:
		n = ENC_MODE_AES_GCMP;
		break;
	case eCSR_ENCRYPT_TYPE_AES_GCMP_256:
		n = ENC_MODE_AES_GCMP_256;
		break;
#ifdef FEATURE_WLAN_WAPI
	case eCSR_ENCRYPT_TYPE_WPI:
		n = ENC_MODE_SMS4;
		break;
#endif /* FEATURE_WLAN_WAPI */
	default:
		break;
	}
	return n;
}

enum mgmt_dot11_mode
diag_dot11_mode_from_csr_type(enum csr_cfgdot11mode dot11mode)
{
	switch (dot11mode) {
	case eCSR_CFG_DOT11_MODE_ABG:
		return DOT11_MODE_ABG;
	case eCSR_CFG_DOT11_MODE_11A:
		return DOT11_MODE_11A;
	case eCSR_CFG_DOT11_MODE_11B:
		return DOT11_MODE_11B;
	case eCSR_CFG_DOT11_MODE_11G:
		return DOT11_MODE_11G;
	case eCSR_CFG_DOT11_MODE_11N:
		return DOT11_MODE_11N;
	case eCSR_CFG_DOT11_MODE_11AC:
		return DOT11_MODE_11AC;
	case eCSR_CFG_DOT11_MODE_11G_ONLY:
		return DOT11_MODE_11G_ONLY;
	case eCSR_CFG_DOT11_MODE_11N_ONLY:
		return DOT11_MODE_11N_ONLY;
	case eCSR_CFG_DOT11_MODE_11AC_ONLY:
		return DOT11_MODE_11AC_ONLY;
	case eCSR_CFG_DOT11_MODE_AUTO:
		return DOT11_MODE_AUTO;
	case eCSR_CFG_DOT11_MODE_11AX:
		return DOT11_MODE_11AX;
	case eCSR_CFG_DOT11_MODE_11AX_ONLY:
		return DOT11_MODE_11AX_ONLY;
	default:
		return DOT11_MODE_MAX;
	}
}

enum mgmt_ch_width diag_ch_width_from_csr_type(enum phy_ch_width ch_width)
{
	switch (ch_width) {
	case CH_WIDTH_20MHZ:
		return BW_20MHZ;
	case CH_WIDTH_40MHZ:
		return BW_40MHZ;
	case CH_WIDTH_80MHZ:
		return BW_80MHZ;
	case CH_WIDTH_160MHZ:
		return BW_160MHZ;
	case CH_WIDTH_80P80MHZ:
		return BW_80P80MHZ;
	case CH_WIDTH_5MHZ:
		return BW_5MHZ;
	case CH_WIDTH_10MHZ:
		return BW_10MHZ;
	default:
		return BW_MAX;
	}
}

enum mgmt_bss_type diag_persona_from_csr_type(enum QDF_OPMODE persona)
{
	switch (persona) {
	case QDF_STA_MODE:
		return STA_PERSONA;
	case QDF_SAP_MODE:
		return SAP_PERSONA;
	case QDF_P2P_CLIENT_MODE:
		return P2P_CLIENT_PERSONA;
	case QDF_P2P_GO_MODE:
		return P2P_GO_PERSONA;
	case QDF_FTM_MODE:
		return FTM_PERSONA;
	case QDF_MONITOR_MODE:
		return MONITOR_PERSONA;
	case QDF_P2P_DEVICE_MODE:
		return P2P_DEVICE_PERSONA;
	case QDF_OCB_MODE:
		return OCB_PERSONA;
	case QDF_EPPING_MODE:
		return EPPING_PERSONA;
	case QDF_QVIT_MODE:
		return QVIT_PERSONA;
	case QDF_NDI_MODE:
		return NDI_PERSONA;
	case QDF_WDS_MODE:
		return WDS_PERSONA;
	case QDF_BTAMP_MODE:
		return BTAMP_PERSONA;
	case QDF_AHDEMO_MODE:
		return AHDEMO_PERSONA;
	default:
		return MAX_PERSONA;
	}
}
#endif /* #ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR */

static const uint32_t
social_channel_freq[MAX_SOCIAL_CHANNELS] = { 2412, 2437, 2462 };

static void init_config_param(struct mac_context *mac);
static bool csr_roam_process_results(struct mac_context *mac, tSmeCmd *pCommand,
				     enum csr_roamcomplete_result Result,
				     void *Context);
static ePhyChanBondState csr_get_cb_mode_from_ies(struct mac_context *mac,
						  uint32_t primary_ch_freq,
						  tDot11fBeaconIEs *pIes);

static void csr_roaming_state_config_cnf_processor(struct mac_context *mac,
			tSmeCmd *pCommand, uint8_t session_id);
static QDF_STATUS csr_roam_open(struct mac_context *mac);
static QDF_STATUS csr_roam_close(struct mac_context *mac);
static bool csr_roam_is_same_profile_keys(struct mac_context *mac,
				   tCsrRoamConnectedProfile *pConnProfile,
				   struct csr_roam_profile *pProfile2);

static QDF_STATUS csr_roam_start_roaming_timer(struct mac_context *mac,
					       uint32_t vdev_id,
					       uint32_t interval);
static QDF_STATUS csr_roam_stop_roaming_timer(struct mac_context *mac,
					      uint32_t sessionId);
static void csr_roam_roaming_timer_handler(void *pv);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void csr_roam_roaming_offload_timer_action(struct mac_context *mac_ctx,
		uint32_t interval, uint8_t session_id, uint8_t action);
#endif
static void csr_roam_roaming_offload_timeout_handler(void *timer_data);
static QDF_STATUS csr_roam_start_wait_for_key_timer(struct mac_context *mac,
						uint32_t interval);
static void csr_roam_wait_for_key_time_out_handler(void *pv);
static QDF_STATUS csr_init11d_info(struct mac_context *mac, tCsr11dinfo *ps11dinfo);
static QDF_STATUS csr_init_channel_power_list(struct mac_context *mac,
					      tCsr11dinfo *ps11dinfo);
static QDF_STATUS csr_roam_free_connected_info(struct mac_context *mac,
					       struct csr_roam_connectedinfo *
					       pConnectedInfo);
static void csr_roam_link_up(struct mac_context *mac, struct qdf_mac_addr bssid);
static void csr_roam_link_down(struct mac_context *mac, uint32_t sessionId);
static enum csr_cfgdot11mode
csr_roam_get_phy_mode_band_for_bss(struct mac_context *mac,
				   struct csr_roam_profile *pProfile,
				   uint32_t bss_op_ch_freq,
				   enum reg_wifi_band *pBand);
static QDF_STATUS csr_roam_get_qos_info_from_bss(
struct mac_context *mac, struct bss_description *bss_desc);
static uint32_t csr_find_session_by_type(struct mac_context *,
					enum QDF_OPMODE);
static bool csr_is_conn_allow_2g_band(struct mac_context *mac,
						uint32_t chnl);
static bool csr_is_conn_allow_5g_band(struct mac_context *mac,
						uint32_t chnl);
static QDF_STATUS csr_roam_start_wds(struct mac_context *mac,
						uint32_t sessionId,
				     struct csr_roam_profile *pProfile,
				     struct bss_description *bss_desc);
static void csr_init_session(struct mac_context *mac, uint32_t sessionId);

static QDF_STATUS
csr_roam_get_qos_info_from_bss(struct mac_context *mac,
			       struct bss_description *bss_desc);

static void csr_init_operating_classes(struct mac_context *mac);

static void csr_add_len_of_social_channels(struct mac_context *mac,
		uint8_t *num_chan);
static void csr_add_social_channels(struct mac_context *mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan);

#ifdef WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY
static struct csr_roam_session *csr_roam_roam_session;

/* Allocate and initialize global variables */
static QDF_STATUS csr_roam_init_globals(struct mac_context *mac)
{
	uint32_t buf_size;
	QDF_STATUS status;

	buf_size = WLAN_MAX_VDEVS * sizeof(struct csr_roam_session);

	csr_roam_roam_session = qdf_mem_malloc(buf_size);
	if (csr_roam_roam_session) {
		mac->roam.roamSession = csr_roam_roam_session;
		status = QDF_STATUS_SUCCESS;
	} else {
		status = QDF_STATUS_E_NOMEM;
	}

	return status;
}

/* Free memory allocated dynamically */
static inline void csr_roam_free_globals(void)
{
	qdf_mem_free(csr_roam_roam_session);
	csr_roam_roam_session = NULL;
}

#else /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */
static struct csr_roam_session csr_roam_roam_session[WLAN_MAX_VDEVS];

/* Initialize global variables */
static QDF_STATUS csr_roam_init_globals(struct mac_context *mac)
{
	qdf_mem_zero(&csr_roam_roam_session,
		     sizeof(csr_roam_roam_session));
	mac->roam.roamSession = csr_roam_roam_session;

	return QDF_STATUS_SUCCESS;
}

static inline void csr_roam_free_globals(void)
{
}
#endif /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */

/* Returns whether handoff is currently in progress or not */
static
bool csr_roam_is_handoff_in_progress(struct mac_context *mac, uint8_t sessionId)
{
	return csr_neighbor_roam_is_handoff_in_progress(mac, sessionId);
}

static QDF_STATUS
csr_roam_issue_disassociate(struct mac_context *mac, uint32_t sessionId,
			    enum csr_roam_substate NewSubstate,
			    bool fMICFailure)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr bssId = QDF_MAC_ADDR_BCAST_INIT;
	uint16_t reasonCode;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	tpCsrNeighborRoamControlInfo p_nbr_roam_info;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (fMICFailure) {
		reasonCode = REASON_MIC_FAILURE;
	} else if (NewSubstate == eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF) {
		reasonCode = REASON_AUTHORIZED_ACCESS_LIMIT_REACHED;
	} else if (eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT == NewSubstate) {
		reasonCode = REASON_DISASSOC_NETWORK_LEAVING;
		NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_FORCED;
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "set to reason code eSIR_MAC_DISASSOC_LEAVING_BSS_REASON and set back NewSubstate");
	} else {
		reasonCode = REASON_UNSPEC_FAILURE;
	}

	p_nbr_roam_info = &mac->roam.neighborRoamInfo[sessionId];
	if (csr_roam_is_handoff_in_progress(mac, sessionId) &&
	    NewSubstate != eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF &&
	    p_nbr_roam_info->csrNeighborRoamProfile.BSSIDs.numOfBSSIDs) {
		qdf_copy_macaddr(
			&bssId,
			p_nbr_roam_info->csrNeighborRoamProfile.BSSIDs.bssid);
	} else if (pSession->pConnectBssDesc) {
		qdf_mem_copy(&bssId.bytes, pSession->pConnectBssDesc->bssId,
			     sizeof(struct qdf_mac_addr));
	}

	sme_debug("Disassociate Bssid " QDF_MAC_ADDR_FMT " subState: %s reason: %d",
		  QDF_MAC_ADDR_REF(bssId.bytes),
		  mac_trace_getcsr_roam_sub_state(NewSubstate),
		  reasonCode);

	csr_roam_substate_change(mac, NewSubstate, sessionId);

	status = csr_send_mb_disassoc_req_msg(mac, sessionId, bssId.bytes,
					      reasonCode);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		csr_roam_link_down(mac, sessionId);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		/* no need to tell QoS that we are disassociating, it will be
		 * taken care off in assoc req for HO
		 */
		if (eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF != NewSubstate) {
			/* notify QoS module that disassoc happening */
			sme_qos_csr_event_ind(mac, (uint8_t)sessionId,
					      SME_QOS_CSR_DISCONNECT_REQ, NULL);
		}
#endif
	} else {
		sme_warn("csr_send_mb_disassoc_req_msg failed status: %d",
			 status);
	}

	return status;
}

static void csr_roam_de_init_globals(struct mac_context *mac)
{
	uint8_t i;

	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		if (mac->roam.roamSession[i].pCurRoamProfile)
			csr_release_profile(mac,
					    mac->roam.roamSession[i].
					    pCurRoamProfile);
		csr_release_profile(mac,
				    &mac->roam.roamSession[i].
				    stored_roam_profile.profile);
	}
	csr_roam_free_globals();
	mac->roam.roamSession = NULL;
}

QDF_STATUS csr_open(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t i;

	do {
		/* Initialize CSR Roam Globals */
		status = csr_roam_init_globals(mac);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		for (i = 0; i < WLAN_MAX_VDEVS; i++)
			csr_roam_state_change(mac, eCSR_ROAMING_STATE_STOP, i);

		init_config_param(mac);
		status = csr_scan_open(mac);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			csr_roam_free_globals();
			break;
		}
		status = csr_roam_open(mac);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			csr_roam_free_globals();
			break;
		}
		mac->roam.nextRoamId = 1;      /* Must not be 0 */
	} while (0);

	return status;
}

QDF_STATUS csr_init_chan_list(struct mac_context *mac, uint8_t *alpha2)
{
	QDF_STATUS status;

	mac->scan.countryCodeDefault[0] = alpha2[0];
	mac->scan.countryCodeDefault[1] = alpha2[1];
	mac->scan.countryCodeDefault[2] = alpha2[2];

	sme_debug("init time country code %.2s", mac->scan.countryCodeDefault);

	mac->scan.domainIdDefault = 0;
	mac->scan.domainIdCurrent = 0;

	qdf_mem_copy(mac->scan.countryCodeCurrent,
		     mac->scan.countryCodeDefault, REG_ALPHA2_LEN + 1);
	qdf_mem_copy(mac->scan.countryCodeElected,
		     mac->scan.countryCodeDefault, REG_ALPHA2_LEN + 1);
	status = csr_get_channel_and_power_list(mac);

	return status;
}

QDF_STATUS csr_set_channels(struct mac_context *mac,
			    struct csr_config_params *pParam)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t index = 0;

	qdf_mem_copy(pParam->Csr11dinfo.countryCode,
		     mac->scan.countryCodeCurrent, REG_ALPHA2_LEN + 1);
	for (index = 0; index < mac->scan.base_channels.numChannels;
	     index++) {
		pParam->Csr11dinfo.Channels.channel_freq_list[index] =
			mac->scan.base_channels.channel_freq_list[index];
		pParam->Csr11dinfo.ChnPower[index].first_chan_freq =
			mac->scan.base_channels.channel_freq_list[index];
		pParam->Csr11dinfo.ChnPower[index].numChannels = 1;
		pParam->Csr11dinfo.ChnPower[index].maxtxPower =
			mac->scan.defaultPowerTable[index].tx_power;
	}
	pParam->Csr11dinfo.Channels.numChannels =
		mac->scan.base_channels.numChannels;

	return status;
}

QDF_STATUS csr_close(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	csr_roam_close(mac);
	csr_scan_close(mac);
	/* DeInit Globals */
	csr_roam_de_init_globals(mac);
	return status;
}

static int8_t
csr_find_channel_pwr(struct channel_power *pdefaultPowerTable,
		     uint32_t chan_freq)
{
	uint8_t i;
	/* TODO: if defaultPowerTable is guaranteed to be in ascending */
	/* order of channel numbers, we can employ binary search */
	for (i = 0; i < CFG_VALID_CHANNEL_LIST_LEN; i++) {
		if (pdefaultPowerTable[i].center_freq == chan_freq)
			return pdefaultPowerTable[i].tx_power;
	}
	/* could not find the channel list in default list */
	/* this should not have occurred */
	QDF_ASSERT(0);
	return 0;
}

/**
 * csr_roam_arrange_ch_list() - Updates the channel list modified with greedy
 * order for 5 Ghz preference and DFS channels.
 * @mac_ctx: pointer to mac context.
 * @chan_list:    channel list updated with greedy channel order.
 * @num_channel:  Number of channels in list
 *
 * To allow Early Stop Roaming Scan feature to co-exist with 5G preference,
 * this function moves 5G channels ahead of 2G channels. This function can
 * also move 2G channels, ahead of DFS channel or vice versa. Order is
 * maintained among same category channels
 *
 * Return: None
 */
static void csr_roam_arrange_ch_list(struct mac_context *mac_ctx,
			tSirUpdateChanParam *chan_list, uint8_t num_channel)
{
	bool prefer_5g = CSR_IS_ROAM_PREFER_5GHZ(mac_ctx);
	bool prefer_dfs = CSR_IS_DFS_CH_ROAM_ALLOWED(mac_ctx);
	int i, j = 0;
	tSirUpdateChanParam *tmp_list = NULL;

	if (!prefer_5g)
		return;

	tmp_list = qdf_mem_malloc(sizeof(tSirUpdateChanParam) * num_channel);
	if (!tmp_list)
		return;

	/* Fist copy Non-DFS 5g channels */
	for (i = 0; i < num_channel; i++) {
		if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_list[i].freq) &&
			!wlan_reg_is_dfs_for_freq(mac_ctx->pdev,
						  chan_list[i].freq)) {
			qdf_mem_copy(&tmp_list[j++],
				&chan_list[i], sizeof(tSirUpdateChanParam));
			chan_list[i].freq = 0;
		}
	}
	if (prefer_dfs) {
		/* next copy DFS channels (remaining channels in 5G) */
		for (i = 0; i < num_channel; i++) {
			if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_list[i].freq)) {
				qdf_mem_copy(&tmp_list[j++], &chan_list[i],
					sizeof(tSirUpdateChanParam));
				chan_list[i].freq = 0;
			}
		}
	} else {
		/* next copy 2G channels */
		for (i = 0; i < num_channel; i++) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_list[i].freq)) {
				qdf_mem_copy(&tmp_list[j++], &chan_list[i],
					sizeof(tSirUpdateChanParam));
				chan_list[i].freq = 0;
			}
		}
	}
	/* copy rest of the channels in same order to tmp list */
	for (i = 0; i < num_channel; i++) {
		if (chan_list[i].freq) {
			qdf_mem_copy(&tmp_list[j++], &chan_list[i],
				sizeof(tSirUpdateChanParam));
			chan_list[i].freq = 0;
		}
	}
	/* copy tmp list to original channel list buffer */
	qdf_mem_copy(chan_list, tmp_list,
				 sizeof(tSirUpdateChanParam) * num_channel);
	qdf_mem_free(tmp_list);
}

/**
 * csr_roam_sort_channel_for_early_stop() - Sort the channels
 * @mac_ctx:        mac global context
 * @chan_list:      Original channel list from the upper layers
 * @num_channel:    Number of original channels
 *
 * For Early stop scan feature, the channel list should be in an order,
 * where-in there is a maximum chance to detect an AP in the initial
 * channels in the list so that the scanning can be stopped early as the
 * feature demands.
 * Below fixed greedy channel list has been provided
 * based on most of the enterprise wifi installations across the globe.
 *
 * Identify all the greedy channels within the channel list from user space.
 * Identify all the non-greedy channels in the user space channel list.
 * Merge greedy channels followed by non-greedy channels back into the
 * chan_list.
 *
 * Return: None
 */
static void csr_roam_sort_channel_for_early_stop(struct mac_context *mac_ctx,
			tSirUpdateChanList *chan_list, uint8_t num_channel)
{
	tSirUpdateChanList *chan_list_greedy, *chan_list_non_greedy;
	uint8_t i, j;
	static const uint32_t fixed_greedy_freq_list[] = {2412, 2437, 2462,
		5180, 5240, 5200, 5220, 2457, 2417, 2452, 5745, 5785, 5805,
		2422, 2427, 2447, 5765, 5825, 2442, 2432, 5680, 5700, 5260,
		5580, 5280, 5520, 5320, 5300, 5500, 5600, 2472, 2484, 5560,
		5660, 5755, 5775};
	uint8_t num_fixed_greedy_chan;
	uint8_t num_greedy_chan = 0;
	uint8_t num_non_greedy_chan = 0;
	uint8_t match_found = false;
	uint32_t buf_size;

	buf_size = sizeof(tSirUpdateChanList) +
		(sizeof(tSirUpdateChanParam) * num_channel);
	chan_list_greedy = qdf_mem_malloc(buf_size);
	chan_list_non_greedy = qdf_mem_malloc(buf_size);
	if (!chan_list_greedy || !chan_list_non_greedy)
		goto scan_list_sort_error;
	/*
	 * fixed_greedy_freq_list is an evaluated freq list based on most of
	 * the enterprise wifi deployments and the order of the channels
	 * determines the highest possibility of finding an AP.
	 * chan_list is the channel list provided by upper layers based on the
	 * regulatory domain.
	 */
	num_fixed_greedy_chan = sizeof(fixed_greedy_freq_list) /
							sizeof(uint32_t);
	/*
	 * Browse through the chan_list and put all the non-greedy channels
	 * into a separate list by name chan_list_non_greedy
	 */
	for (i = 0; i < num_channel; i++) {
		for (j = 0; j < num_fixed_greedy_chan; j++) {
			if (chan_list->chanParam[i].freq ==
					 fixed_greedy_freq_list[j]) {
				match_found = true;
				break;
			}
		}
		if (!match_found) {
			qdf_mem_copy(
			  &chan_list_non_greedy->chanParam[num_non_greedy_chan],
			  &chan_list->chanParam[i],
			  sizeof(tSirUpdateChanParam));
			num_non_greedy_chan++;
		} else {
			match_found = false;
		}
	}
	/*
	 * Browse through the fixed_greedy_chan_list and put all the greedy
	 * channels in the chan_list into a separate list by name
	 * chan_list_greedy
	 */
	for (i = 0; i < num_fixed_greedy_chan; i++) {
		for (j = 0; j < num_channel; j++) {
			if (fixed_greedy_freq_list[i] ==
				chan_list->chanParam[j].freq) {
				qdf_mem_copy(
				  &chan_list_greedy->chanParam[num_greedy_chan],
				  &chan_list->chanParam[j],
				  sizeof(tSirUpdateChanParam));
				num_greedy_chan++;
				break;
			}
		}
	}
	QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_DEBUG,
		"greedy=%d, non-greedy=%d, tot=%d",
		num_greedy_chan, num_non_greedy_chan, num_channel);
	if ((num_greedy_chan + num_non_greedy_chan) != num_channel) {
		QDF_TRACE(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_ERROR,
			"incorrect sorting of channels");
		goto scan_list_sort_error;
	}
	/* Copy the Greedy channels first */
	i = 0;
	qdf_mem_copy(&chan_list->chanParam[i],
		&chan_list_greedy->chanParam[i],
		num_greedy_chan * sizeof(tSirUpdateChanParam));
	/* Copy the remaining Non Greedy channels */
	i = num_greedy_chan;
	j = 0;
	qdf_mem_copy(&chan_list->chanParam[i],
		&chan_list_non_greedy->chanParam[j],
		num_non_greedy_chan * sizeof(tSirUpdateChanParam));

	/* Update channel list for 5g preference and allow DFS roam */
	csr_roam_arrange_ch_list(mac_ctx, chan_list->chanParam, num_channel);
scan_list_sort_error:
	qdf_mem_free(chan_list_greedy);
	qdf_mem_free(chan_list_non_greedy);
}

/**
 * csr_emu_chan_req() - update the required channel list for emulation
 * @channel: channel number to check
 *
 * To reduce scan time during emulation platforms, this function
 * restricts the scanning to be done on selected channels
 *
 * Return: QDF_STATUS enumeration
 */
#ifdef QCA_WIFI_NAPIER_EMULATION
#define SCAN_CHAN_LIST_5G_LEN 6
#define SCAN_CHAN_LIST_2G_LEN 3
static const uint16_t
csr_scan_chan_list_5g[SCAN_CHAN_LIST_5G_LEN] = { 5180, 5220, 5260, 5280, 5700, 5745 };
static const uint16_t
csr_scan_chan_list_2g[SCAN_CHAN_LIST_2G_LEN] = { 2412, 2437, 2462 };
static QDF_STATUS csr_emu_chan_req(uint32_t channel)
{
	int i;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(channel)) {
		for (i = 0; i < QDF_ARRAY_SIZE(csr_scan_chan_list_2g); i++) {
			if (csr_scan_chan_list_2g[i] == channel)
				return QDF_STATUS_SUCCESS;
		}
	} else if (WLAN_REG_IS_5GHZ_CH_FREQ(channel)) {
		for (i = 0; i < QDF_ARRAY_SIZE(csr_scan_chan_list_5g); i++) {
			if (csr_scan_chan_list_5g[i] == channel)
				return QDF_STATUS_SUCCESS;
		}
	}
	return QDF_STATUS_E_FAILURE;
}
#else
static QDF_STATUS csr_emu_chan_req(uint32_t channel_num)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_ENABLE_SOCIAL_CHANNELS_5G_ONLY
static void csr_add_len_of_social_channels(struct mac_context *mac,
		uint8_t *num_chan)
{
	uint8_t i;
	uint8_t no_chan = *num_chan;

	sme_debug("add len of social channels, before adding - num_chan:%hu",
			*num_chan);
	if (CSR_IS_5G_BAND_ONLY(mac)) {
		for (i = 0; i < MAX_SOCIAL_CHANNELS; i++) {
			if (wlan_reg_get_channel_state_for_freq(
				mac->pdev, social_channel_freq[i]) ==
					CHANNEL_STATE_ENABLE)
				no_chan++;
		}
	}
	*num_chan = no_chan;
	sme_debug("after adding - num_chan:%hu", *num_chan);
}

static void csr_add_social_channels(struct mac_context *mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan)
{
	uint8_t i;
	uint8_t no_chan = *num_chan;

	sme_debug("add social channels chan_list %pK, num_chan %hu", chan_list,
			*num_chan);
	if (CSR_IS_5G_BAND_ONLY(mac)) {
		for (i = 0; i < MAX_SOCIAL_CHANNELS; i++) {
			if (wlan_reg_get_channel_state_for_freq(
					mac->pdev, social_channel_freq[i]) !=
					CHANNEL_STATE_ENABLE)
				continue;
			chan_list->chanParam[no_chan].freq =
				social_channel_freq[i];
			chan_list->chanParam[no_chan].pwr =
				csr_find_channel_pwr(pScan->defaultPowerTable,
						social_channel_freq[i]);
			chan_list->chanParam[no_chan].dfsSet = false;
			if (cds_is_5_mhz_enabled())
				chan_list->chanParam[no_chan].quarter_rate
					= 1;
			else if (cds_is_10_mhz_enabled())
				chan_list->chanParam[no_chan].half_rate = 1;
			no_chan++;
		}
		sme_debug("after adding -num_chan %hu", no_chan);
	}
	*num_chan = no_chan;
}
#else
static void csr_add_len_of_social_channels(struct mac_context *mac,
		uint8_t *num_chan)
{
	sme_debug("skip adding len of social channels");
}
static void csr_add_social_channels(struct mac_context *mac,
		tSirUpdateChanList *chan_list, struct csr_scanstruct *pScan,
		uint8_t *num_chan)
{
	sme_debug("skip social channels");
}
#endif

/**
 * csr_scan_event_handler() - callback for scan event
 * @vdev: wlan objmgr vdev pointer
 * @event: scan event
 * @arg: global mac context pointer
 *
 * Return: void
 */
static void csr_scan_event_handler(struct wlan_objmgr_vdev *vdev,
					    struct scan_event *event,
					    void *arg)
{
	bool success = false;
	QDF_STATUS lock_status;
	struct mac_context *mac = arg;

	if (!mac)
		return;

	if (!util_is_scan_completed(event, &success))
		return;

	lock_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(lock_status))
		return;

	if (mac->scan.pending_channel_list_req)
		csr_update_channel_list(mac);
	sme_release_global_lock(&mac->sme);
}

QDF_STATUS csr_update_channel_list(struct mac_context *mac)
{
	tSirUpdateChanList *pChanList;
	struct csr_scanstruct *pScan = &mac->scan;
	uint8_t numChan = pScan->base_channels.numChannels;
	uint8_t num_channel = 0;
	uint32_t bufLen;
	struct scheduler_msg msg = {0};
	uint8_t i;
	uint8_t channel_state;
	uint16_t unsafe_chan[NUM_CHANNELS];
	uint16_t unsafe_chan_cnt = 0;
	uint16_t cnt = 0;
	uint32_t  channel_freq;
	bool is_unsafe_chan;
	bool is_same_band;
	bool is_5mhz_enabled;
	bool is_10mhz_enabled;
	enum scm_scan_status scan_status;
	QDF_STATUS lock_status;

	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx) {
		sme_err("qdf_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	lock_status = sme_acquire_global_lock(&mac->sme);
	if (QDF_IS_STATUS_ERROR(lock_status))
		return lock_status;

	if (mac->mlme_cfg->reg.enable_pending_chan_list_req) {
		scan_status = ucfg_scan_get_pdev_status(mac->pdev);
		if (scan_status == SCAN_IS_ACTIVE ||
		    scan_status == SCAN_IS_ACTIVE_AND_PENDING) {
			mac->scan.pending_channel_list_req = true;
			sme_release_global_lock(&mac->sme);
			sme_debug("scan in progress postpone channel list req ");
			return QDF_STATUS_SUCCESS;
		}
		mac->scan.pending_channel_list_req = false;
	}
	sme_release_global_lock(&mac->sme);

	pld_get_wlan_unsafe_channel(qdf_ctx->dev, unsafe_chan,
		    &unsafe_chan_cnt,
		    sizeof(unsafe_chan));

	csr_add_len_of_social_channels(mac, &numChan);

	bufLen = sizeof(tSirUpdateChanList) +
		 (sizeof(tSirUpdateChanParam) * (numChan));

	csr_init_operating_classes(mac);
	pChanList = qdf_mem_malloc(bufLen);
	if (!pChanList)
		return QDF_STATUS_E_NOMEM;

	is_5mhz_enabled = cds_is_5_mhz_enabled();
	if (is_5mhz_enabled)
		sme_nofl_debug("quarter_rate enabled");
	is_10mhz_enabled = cds_is_10_mhz_enabled();
	if (is_10mhz_enabled)
		sme_nofl_debug("half_rate enabled");

	for (i = 0; i < pScan->base_channels.numChannels; i++) {
		struct csr_sta_roam_policy_params *roam_policy =
			&mac->roam.configParam.sta_roam_policy;
		if (QDF_STATUS_SUCCESS !=
			csr_emu_chan_req(pScan->base_channels.channel_freq_list[i]))
			continue;

		channel_freq = pScan->base_channels.channel_freq_list[i];
		/* Scan is not performed on DSRC channels*/
		if (wlan_reg_is_dsrc_freq(channel_freq))
			continue;

		channel_state =
			wlan_reg_get_channel_state_for_freq(
				mac->pdev, channel_freq);
		if ((CHANNEL_STATE_ENABLE == channel_state) ||
		    mac->scan.fEnableDFSChnlScan) {
			if ((mac->roam.configParam.sta_roam_policy.dfs_mode ==
				CSR_STA_ROAM_POLICY_DFS_DISABLED) &&
				(channel_state == CHANNEL_STATE_DFS)) {
				QDF_TRACE(QDF_MODULE_ID_SME,
					QDF_TRACE_LEVEL_DEBUG,
					FL("skip dfs channel frequency %d"),
					channel_freq);
				continue;
			}
			if (mac->roam.configParam.sta_roam_policy.
					skip_unsafe_channels &&
					unsafe_chan_cnt) {
				is_unsafe_chan = false;
				for (cnt = 0; cnt < unsafe_chan_cnt; cnt++) {
					if (unsafe_chan[cnt] == channel_freq) {
						is_unsafe_chan = true;
						break;
					}
				}
				is_same_band =
					(WLAN_REG_IS_24GHZ_CH_FREQ(
							channel_freq) &&
					roam_policy->sap_operating_band ==
							BAND_2G) ||
					(WLAN_REG_IS_5GHZ_CH_FREQ(
							channel_freq) &&
					roam_policy->sap_operating_band ==
							BAND_5G);
				if (is_unsafe_chan && is_same_band) {
					QDF_TRACE(QDF_MODULE_ID_SME,
					QDF_TRACE_LEVEL_DEBUG,
					FL("ignoring unsafe channel freq %d"),
					channel_freq);
					continue;
				}
			}
			pChanList->chanParam[num_channel].freq =
				pScan->base_channels.channel_freq_list[i];
			pChanList->chanParam[num_channel].pwr =
				csr_find_channel_pwr(
				pScan->defaultPowerTable,
				pScan->base_channels.channel_freq_list[i]);

			if (pScan->fcc_constraint) {
				if (2467 ==
					pScan->base_channels.channel_freq_list[i]) {
					pChanList->chanParam[num_channel].pwr =
						MAX_PWR_FCC_CHAN_12;
					QDF_TRACE(QDF_MODULE_ID_SME,
						  QDF_TRACE_LEVEL_DEBUG,
						  "txpow for channel 12 is %d",
						  MAX_PWR_FCC_CHAN_12);
				}
				if (2472 ==
					pScan->base_channels.channel_freq_list[i]) {
					pChanList->chanParam[num_channel].pwr =
						MAX_PWR_FCC_CHAN_13;
					QDF_TRACE(QDF_MODULE_ID_SME,
						  QDF_TRACE_LEVEL_DEBUG,
						  "txpow for channel 13 is %d",
						  MAX_PWR_FCC_CHAN_13);
				}
			}

			if (!ucfg_is_nan_allowed_on_freq(mac->pdev,
				pChanList->chanParam[num_channel].freq))
				pChanList->chanParam[num_channel].nan_disabled =
					true;

			if (CHANNEL_STATE_ENABLE != channel_state)
				pChanList->chanParam[num_channel].dfsSet =
					true;

			pChanList->chanParam[num_channel].quarter_rate =
							is_5mhz_enabled;

			pChanList->chanParam[num_channel].half_rate =
							is_10mhz_enabled;

			num_channel++;
		}
	}

	csr_add_social_channels(mac, pChanList, pScan, &num_channel);

	if (mac->mlme_cfg->lfr.early_stop_scan_enable)
		csr_roam_sort_channel_for_early_stop(mac, pChanList,
						     num_channel);
	else
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("Early Stop Scan Feature not supported"));

	if ((mac->roam.configParam.uCfgDot11Mode ==
				eCSR_CFG_DOT11_MODE_AUTO) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11AC) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11AC_ONLY)) {
		pChanList->vht_en = true;
		if (mac->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band)
			pChanList->vht_24_en = true;
	}
	if ((mac->roam.configParam.uCfgDot11Mode ==
				eCSR_CFG_DOT11_MODE_AUTO) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11N) ||
			(mac->roam.configParam.uCfgDot11Mode ==
			 eCSR_CFG_DOT11_MODE_11N_ONLY)) {
		pChanList->ht_en = true;
	}
	if ((mac->roam.configParam.uCfgDot11Mode == eCSR_CFG_DOT11_MODE_AUTO) ||
	    (mac->roam.configParam.uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (mac->roam.configParam.uCfgDot11Mode ==
	     eCSR_CFG_DOT11_MODE_11AX_ONLY))
		pChanList->he_en = true;

	pChanList->numChan = num_channel;
	mlme_store_fw_scan_channels(mac->psoc, pChanList);

	msg.type = WMA_UPDATE_CHAN_LIST_REQ;
	msg.reserved = 0;
	msg.bodyptr = pChanList;
	MTRACE(qdf_trace(QDF_MODULE_ID_SME, TRACE_CODE_SME_TX_WMA_MSG,
			 NO_SESSION, msg.type));
	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		qdf_mem_free(pChanList);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_start(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t i;

	do {
		for (i = 0; i < WLAN_MAX_VDEVS; i++)
			csr_roam_state_change(mac, eCSR_ROAMING_STATE_IDLE, i);

		status = csr_roam_start(mac);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		mac->roam.sPendingCommands = 0;
		for (i = 0; i < WLAN_MAX_VDEVS; i++)
			status = csr_neighbor_roam_init(mac, i);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_warn("Couldn't Init HO control blk");
			break;
		}
		/* Register with scan component */
		mac->scan.requester_id = ucfg_scan_register_requester(
						mac->psoc,
						"CSR", csr_scan_callback, mac);

		if (mac->mlme_cfg->reg.enable_pending_chan_list_req) {
			status = ucfg_scan_register_event_handler(mac->pdev,
					csr_scan_event_handler, mac);

			if (QDF_IS_STATUS_ERROR(status))
				sme_err("scan event registration failed ");
		}
	} while (0);
	return status;
}

QDF_STATUS csr_stop(struct mac_context *mac)
{
	uint32_t sessionId;

	if (mac->mlme_cfg->reg.enable_pending_chan_list_req)
		ucfg_scan_unregister_event_handler(mac->pdev,
						   csr_scan_event_handler,
						   mac);
	ucfg_scan_psoc_set_disable(mac->psoc, REASON_SYSTEM_DOWN);
	ucfg_scan_unregister_requester(mac->psoc, mac->scan.requester_id);

	/*
	 * purge all serialization commnad if there are any pending to make
	 * sure memory and vdev ref are freed.
	 */
	csr_purge_pdev_all_ser_cmd_list(mac);
	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++)
		csr_prepare_vdev_delete(mac, sessionId, true);

	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++)
		csr_neighbor_roam_close(mac, sessionId);
	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++)
		if (CSR_IS_SESSION_VALID(mac, sessionId))
			ucfg_scan_flush_results(mac->pdev, NULL);

	/* Reset the domain back to the deault */
	mac->scan.domainIdCurrent = mac->scan.domainIdDefault;

	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++) {
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_STOP, sessionId);
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_ready(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	/* If the gScanAgingTime is set to '0' then scan results aging timeout
	 * based  on timer feature is not enabled
	 */
	status = csr_apply_channel_and_power_list(mac);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("csr_apply_channel_and_power_list failed status: %d",
			status);

	return status;
}

void csr_set_default_dot11_mode(struct mac_context *mac)
{
	mac->mlme_cfg->dot11_mode.dot11_mode =
			csr_translate_to_wni_cfg_dot11_mode(mac,
					  mac->roam.configParam.uCfgDot11Mode);
}

void csr_set_global_cfgs(struct mac_context *mac)
{
	wlan_mlme_set_frag_threshold(mac->psoc, csr_get_frag_thresh(mac));
	wlan_mlme_set_rts_threshold(mac->psoc, csr_get_rts_thresh(mac));
	/* For now we will just use the 5GHz CB mode ini parameter to decide
	 * whether CB supported or not in Probes when there is no session
	 * Once session is established we will use the session related params
	 * stored in PE session for CB mode
	 */
	if (cfg_in_range(CFG_CHANNEL_BONDING_MODE_5GHZ,
			 mac->roam.configParam.channelBondingMode5GHz))
		ucfg_mlme_set_channel_bonding_5ghz(mac->psoc,
						   mac->roam.configParam.
						   channelBondingMode5GHz);
	if (cfg_in_range(CFG_CHANNEL_BONDING_MODE_24GHZ,
			 mac->roam.configParam.channelBondingMode24GHz))
		ucfg_mlme_set_channel_bonding_24ghz(mac->psoc,
						    mac->roam.configParam.
						    channelBondingMode24GHz);

	if (cfg_in_range(CFG_HEART_BEAT_THRESHOLD,
			 mac->roam.configParam.HeartbeatThresh24))
		mac->mlme_cfg->timeouts.heart_beat_threshold =
			mac->roam.configParam.HeartbeatThresh24;
	else
		mac->mlme_cfg->timeouts.heart_beat_threshold =
			cfg_default(CFG_HEART_BEAT_THRESHOLD);

	/* Update the operating mode to configured value during
	 *  initialization, So that client can advertise full
	 *  capabilities in Probe request frame.
	 */
	csr_set_default_dot11_mode(mac);
}

#if defined(WLAN_LOGGING_SOCK_SVC_ENABLE) && \
	defined(FEATURE_PKTLOG) && !defined(REMOVE_PKT_LOG)
/**
 * csr_packetdump_timer_handler() - packet dump timer
 * handler
 * @pv: user data
 *
 * This function is used to handle packet dump timer
 *
 * Return: None
 *
 */
static void csr_packetdump_timer_handler(void *pv)
{
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"%s Invoking packetdump deregistration API", __func__);
	wlan_deregister_txrx_packetdump(OL_TXRX_PDEV_ID);
}

void csr_packetdump_timer_start(void)
{
	QDF_STATUS status;
	mac_handle_t mac_handle;
	struct mac_context *mac;
	QDF_TIMER_STATE cur_state;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac = MAC_CONTEXT(mac_handle);
	if (!mac) {
		QDF_ASSERT(0);
		return;
	}
	cur_state = qdf_mc_timer_get_current_state(&mac->roam.packetdump_timer);
	if (cur_state == QDF_TIMER_STATE_STARTING ||
	    cur_state == QDF_TIMER_STATE_STARTING) {
		sme_debug("packetdump_timer is already started: %d", cur_state);
		return;
	}

	status = qdf_mc_timer_start(&mac->roam.packetdump_timer,
				    (PKT_DUMP_TIMER_DURATION *
				     QDF_MC_TIMER_TO_SEC_UNIT) /
				    QDF_MC_TIMER_TO_MS_UNIT);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_debug("cannot start packetdump timer status: %d", status);
}

void csr_packetdump_timer_stop(void)
{
	QDF_STATUS status;
	mac_handle_t mac_handle;
	struct mac_context *mac;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac = MAC_CONTEXT(mac_handle);
	if (!mac) {
		QDF_ASSERT(0);
		return;
	}

	status = qdf_mc_timer_stop(&mac->roam.packetdump_timer);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_err("cannot stop packetdump timer");
}

static QDF_STATUS csr_packetdump_timer_init(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!mac) {
		QDF_ASSERT(0);
		return -EINVAL;
	}

	status = qdf_mc_timer_init(&mac->roam.packetdump_timer,
				   QDF_TIMER_TYPE_SW,
				   csr_packetdump_timer_handler,
				   mac);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("cannot allocate memory for packetdump timer");
		return status;
	}

	return status;
}

static void csr_packetdump_timer_deinit(struct mac_context *mac)
{
	if (!mac) {
		QDF_ASSERT(0);
		return;
	}

	qdf_mc_timer_stop(&mac->roam.packetdump_timer);
	qdf_mc_timer_destroy(&mac->roam.packetdump_timer);
}
#else
static inline QDF_STATUS csr_packetdump_timer_init(struct mac_context *mac)
{
	return QDF_STATUS_SUCCESS;
}

static inline void csr_packetdump_timer_deinit(struct mac_context *mac) {}
#endif

static QDF_STATUS csr_roam_open(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t i;
	struct csr_roam_session *pSession;

	do {
		for (i = 0; i < WLAN_MAX_VDEVS; i++) {
			pSession = CSR_GET_SESSION(mac, i);
			pSession->roamingTimerInfo.mac = mac;
			pSession->roamingTimerInfo.vdev_id =
						WLAN_UMAC_VDEV_ID_MAX;
		}
		mac->roam.WaitForKeyTimerInfo.mac = mac;
		mac->roam.WaitForKeyTimerInfo.vdev_id =
						WLAN_UMAC_VDEV_ID_MAX;
		status = qdf_mc_timer_init(&mac->roam.hTimerWaitForKey,
					  QDF_TIMER_TYPE_SW,
					 csr_roam_wait_for_key_time_out_handler,
					  &mac->roam.WaitForKeyTimerInfo);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("cannot allocate memory for WaitForKey time out timer");
			break;
		}
		status = csr_packetdump_timer_init(mac);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		spin_lock_init(&mac->roam.roam_state_lock);
	} while (0);
	return status;
}

static QDF_STATUS csr_roam_close(struct mac_context *mac)
{
	uint32_t sessionId;

	/*
	 * purge all serialization commnad if there are any pending to make
	 * sure memory and vdev ref are freed.
	 */
	csr_purge_pdev_all_ser_cmd_list(mac);
	for (sessionId = 0; sessionId < WLAN_MAX_VDEVS; sessionId++)
		csr_prepare_vdev_delete(mac, sessionId, true);

	qdf_mc_timer_stop(&mac->roam.hTimerWaitForKey);
	qdf_mc_timer_destroy(&mac->roam.hTimerWaitForKey);
	csr_packetdump_timer_deinit(mac);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_roam_start(struct mac_context *mac)
{
	(void)mac;
	return QDF_STATUS_SUCCESS;
}

void csr_roam_stop(struct mac_context *mac, uint32_t sessionId)
{
	csr_roam_stop_roaming_timer(mac, sessionId);
}

QDF_STATUS csr_roam_copy_connect_profile(struct mac_context *mac,
			uint32_t sessionId, tCsrRoamConnectedProfile *pProfile)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t size = 0;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	tCsrRoamConnectedProfile *connected_prof;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	if (!pProfile) {
		sme_err("profile not found");
		return QDF_STATUS_E_FAILURE;
	}

	if (pSession->pConnectBssDesc) {
		size = pSession->pConnectBssDesc->length +
			sizeof(pSession->pConnectBssDesc->length);
		if (size) {
			pProfile->bss_desc = qdf_mem_malloc(size);
			if (pProfile->bss_desc) {
				qdf_mem_copy(pProfile->bss_desc,
					     pSession->pConnectBssDesc,
					     size);
				status = QDF_STATUS_SUCCESS;
			} else {
				return QDF_STATUS_E_FAILURE;
			}
		} else {
			pProfile->bss_desc = NULL;
		}
		connected_prof = &(pSession->connectedProfile);
		pProfile->AuthType = connected_prof->AuthType;
		pProfile->akm_list = connected_prof->akm_list;
		pProfile->EncryptionType = connected_prof->EncryptionType;
		pProfile->mcEncryptionType = connected_prof->mcEncryptionType;
		pProfile->BSSType = connected_prof->BSSType;
		pProfile->op_freq = connected_prof->op_freq;
		qdf_mem_copy(&pProfile->bssid, &connected_prof->bssid,
			sizeof(struct qdf_mac_addr));
		qdf_mem_copy(&pProfile->SSID, &connected_prof->SSID,
			sizeof(tSirMacSSid));
		pProfile->mdid = connected_prof->mdid;
#ifdef FEATURE_WLAN_ESE
		pProfile->isESEAssoc = connected_prof->isESEAssoc;
		if (csr_is_auth_type_ese(connected_prof->AuthType)) {
			qdf_mem_copy(pProfile->eseCckmInfo.krk,
				connected_prof->eseCckmInfo.krk,
				SIR_KRK_KEY_LEN);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
			qdf_mem_copy(pProfile->eseCckmInfo.btk,
				connected_prof->eseCckmInfo.btk,
				SIR_BTK_KEY_LEN);
#endif
			pProfile->eseCckmInfo.reassoc_req_num =
				connected_prof->eseCckmInfo.reassoc_req_num;
			pProfile->eseCckmInfo.krk_plumbed =
				connected_prof->eseCckmInfo.krk_plumbed;
		}
#endif
#ifdef WLAN_FEATURE_11W
		pProfile->MFPEnabled = connected_prof->MFPEnabled;
		pProfile->MFPRequired = connected_prof->MFPRequired;
		pProfile->MFPCapable = connected_prof->MFPCapable;
#endif
	}
	return status;
}

QDF_STATUS csr_roam_get_connect_profile(struct mac_context *mac, uint32_t sessionId,
					tCsrRoamConnectedProfile *pProfile)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if ((csr_is_conn_state_connected(mac, sessionId))) {
		if (pProfile) {
			status =
				csr_roam_copy_connect_profile(mac, sessionId,
							      pProfile);
		}
	}
	return status;
}

void csr_roam_free_connect_profile(tCsrRoamConnectedProfile *profile)
{
	if (profile->bss_desc)
		qdf_mem_free(profile->bss_desc);
	if (profile->pAddIEAssoc)
		qdf_mem_free(profile->pAddIEAssoc);
	qdf_mem_zero(profile, sizeof(tCsrRoamConnectedProfile));
	profile->AuthType = eCSR_AUTH_TYPE_UNKNOWN;
}

static QDF_STATUS csr_roam_free_connected_info(struct mac_context *mac,
					       struct csr_roam_connectedinfo *
					       pConnectedInfo)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (pConnectedInfo->pbFrames) {
		qdf_mem_free(pConnectedInfo->pbFrames);
		pConnectedInfo->pbFrames = NULL;
	}
	pConnectedInfo->nBeaconLength = 0;
	pConnectedInfo->nAssocReqLength = 0;
	pConnectedInfo->nAssocRspLength = 0;
	pConnectedInfo->staId = 0;
	pConnectedInfo->nRICRspLength = 0;
#ifdef FEATURE_WLAN_ESE
	pConnectedInfo->nTspecIeLength = 0;
#endif
	return status;
}

void csr_release_command_roam(struct mac_context *mac, tSmeCmd *pCommand)
{
	csr_reinit_roam_cmd(mac, pCommand);
}

void csr_release_command_wm_status_change(struct mac_context *mac,
					  tSmeCmd *pCommand)
{
	csr_reinit_wm_status_change_cmd(mac, pCommand);
}

static void csr_release_command_set_hw_mode(struct mac_context *mac,
					    tSmeCmd *cmd)
{
	struct csr_roam_session *session;
	uint32_t session_id;

	if (cmd->u.set_hw_mode_cmd.reason ==
	    POLICY_MGR_UPDATE_REASON_HIDDEN_STA) {
		session_id = cmd->u.set_hw_mode_cmd.session_id;
		session = CSR_GET_SESSION(mac, session_id);
		if (session)
			csr_saved_scan_cmd_free_fields(mac, session);
	}
}

void csr_roam_substate_change(struct mac_context *mac,
		enum csr_roam_substate NewSubstate, uint32_t sessionId)
{
	if (sessionId >= WLAN_MAX_VDEVS) {
		sme_err("Invalid no of concurrent sessions %d",
			  sessionId);
		return;
	}
	if (mac->roam.curSubState[sessionId] == NewSubstate)
		return;
	sme_nofl_debug("CSR RoamSubstate: [ %s <== %s ]",
		       mac_trace_getcsr_roam_sub_state(NewSubstate),
		       mac_trace_getcsr_roam_sub_state(mac->roam.
		       curSubState[sessionId]));
	spin_lock(&mac->roam.roam_state_lock);
	mac->roam.curSubState[sessionId] = NewSubstate;
	spin_unlock(&mac->roam.roam_state_lock);
}

enum csr_roam_state csr_roam_state_change(struct mac_context *mac,
				    enum csr_roam_state NewRoamState,
				uint8_t sessionId)
{
	enum csr_roam_state PreviousState;

	PreviousState = mac->roam.curState[sessionId];

	if (NewRoamState == mac->roam.curState[sessionId])
		return PreviousState;

	sme_nofl_debug("CSR RoamState[%d]: [ %s <== %s ]", sessionId,
		       mac_trace_getcsr_roam_state(NewRoamState),
		       mac_trace_getcsr_roam_state(
		       mac->roam.curState[sessionId]));
	/*
	 * Whenever we transition OUT of the Roaming state,
	 * clear the Roaming substate.
	 */
	if (CSR_IS_ROAM_JOINING(mac, sessionId)) {
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
	}

	mac->roam.curState[sessionId] = NewRoamState;

	return PreviousState;
}

static void init_config_param(struct mac_context *mac)
{
	mac->roam.configParam.channelBondingMode24GHz =
		WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
	mac->roam.configParam.channelBondingMode5GHz =
		WNI_CFG_CHANNEL_BONDING_MODE_ENABLE;

	mac->roam.configParam.phyMode = eCSR_DOT11_MODE_AUTO;
	mac->roam.configParam.uCfgDot11Mode = eCSR_CFG_DOT11_MODE_AUTO;
	mac->roam.configParam.HeartbeatThresh24 = 40;
	mac->roam.configParam.HeartbeatThresh50 = 40;
	mac->roam.configParam.Is11eSupportEnabled = true;
	mac->roam.configParam.WMMSupportMode = eCsrRoamWmmAuto;
	mac->roam.configParam.ProprietaryRatesEnabled = true;
	mac->roam.configParam.bCatRssiOffset = CSR_DEFAULT_RSSI_DB_GAP;

	mac->roam.configParam.nVhtChannelWidth =
		WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ + 1;
	/* Remove this code once SLM_Sessionization is supported */
	/* BMPS_WORKAROUND_NOT_NEEDED */
	mac->roam.configParam.doBMPSWorkaround = 0;
}

void csr_flush_cfg_bg_scan_roam_channel_list(tCsrChannelInfo *channel_info)
{
	/* Free up the memory first (if required) */
	if (channel_info->freq_list) {
		qdf_mem_free(channel_info->freq_list);
		channel_info->freq_list = NULL;
		channel_info->numOfChannels = 0;
	}
}

/**
 * csr_flush_roam_scan_chan_lists() - Flush the roam channel lists
 * @mac: Global MAC context
 * @vdev_id: vdev id
 *
 * Flush the roam channel lists pref_chan_info and specific_chan_info.
 *
 * Return: None
 */
static void
csr_flush_roam_scan_chan_lists(struct mac_context *mac, uint8_t vdev_id)
{
	tCsrChannelInfo *chan_info;

	chan_info =
	&mac->roam.neighborRoamInfo[vdev_id].cfgParams.pref_chan_info;
	csr_flush_cfg_bg_scan_roam_channel_list(chan_info);

	chan_info =
	&mac->roam.neighborRoamInfo[vdev_id].cfgParams.specific_chan_info;
	csr_flush_cfg_bg_scan_roam_channel_list(chan_info);
}

QDF_STATUS csr_create_bg_scan_roam_channel_list(struct mac_context *mac,
						tCsrChannelInfo *channel_info,
						const uint32_t *chan_freq_list,
						const uint8_t num_chan)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t i;

	channel_info->freq_list = qdf_mem_malloc(sizeof(uint32_t) * num_chan);
	if (!channel_info->freq_list)
		return QDF_STATUS_E_NOMEM;

	channel_info->numOfChannels = num_chan;
	for (i = 0; i < num_chan; i++)
		channel_info->freq_list[i] = chan_freq_list[i];

	return status;
}

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_check_band_freq_match() - check if passed band and ch freq match
 * @band: band to match with channel frequency
 * @freq: freq to match with band
 *
 * Return: bool if match else false
 */
static bool
csr_check_band_freq_match(enum band_info band, uint32_t freq)
{
	if (band == BAND_ALL)
		return true;

	if (band == BAND_2G && WLAN_REG_IS_24GHZ_CH_FREQ(freq))
		return true;

	if (band == BAND_5G && WLAN_REG_IS_5GHZ_CH_FREQ(freq))
		return true;

	return false;
}

/**
 * is_dfs_unsafe_extra_band_chan() - check if dfs unsafe or extra band channel
 * @mac_ctx: MAC context
 * @freq: channel freq to check
 * @band: band for intra band check
.*.
 * Return: bool if match else false
 */
static bool
is_dfs_unsafe_extra_band_chan(struct mac_context *mac_ctx, uint32_t freq,
			      enum band_info band)
{
	uint16_t  unsafe_chan[NUM_CHANNELS];
	uint16_t  unsafe_chan_cnt = 0;
	uint16_t  cnt = 0;
	bool      is_unsafe_chan;
	qdf_device_t qdf_ctx = cds_get_context(QDF_MODULE_ID_QDF_DEVICE);

	if (!qdf_ctx) {
		cds_err("qdf_ctx is NULL");
		return true;
	}

	if ((mac_ctx->mlme_cfg->lfr.roaming_dfs_channel ==
	     ROAMING_DFS_CHANNEL_DISABLED ||
	     mac_ctx->roam.configParam.sta_roam_policy.dfs_mode ==
	     CSR_STA_ROAM_POLICY_DFS_DISABLED) &&
	    (wlan_reg_is_dfs_for_freq(mac_ctx->pdev, freq)))
		return true;

	pld_get_wlan_unsafe_channel(qdf_ctx->dev, unsafe_chan,
				    &unsafe_chan_cnt,
				    sizeof(unsafe_chan));
	if (mac_ctx->roam.configParam.sta_roam_policy.skip_unsafe_channels &&
	    unsafe_chan_cnt) {
		is_unsafe_chan = false;
		for (cnt = 0; cnt < unsafe_chan_cnt; cnt++) {
			if (unsafe_chan[cnt] == freq) {
				is_unsafe_chan = true;
				QDF_TRACE(QDF_MODULE_ID_SME,
					  QDF_TRACE_LEVEL_DEBUG,
					  ("ignoring unsafe channel freq %d"),
					  freq);
				return true;
			}
		}
	}
	if (!csr_check_band_freq_match(band, freq)) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  ("ignoring non-intra band freq %d"),
			freq);
		return true;
	}

	return false;
}
#endif

#ifdef FEATURE_WLAN_ESE
/**
 * csr_create_roam_scan_channel_list() - create roam scan channel list
 * @mac: Global mac pointer
 * @sessionId: session id
 * @chan_freq_list: pointer to channel list
 * @numChannels: number of channels
 * @band: band enumeration
 *
 * This function modifies the roam scan channel list as per AP neighbor
 * report; AP neighbor report may be empty or may include only other AP
 * channels; in any case, we merge the channel list with the learned occupied
 * channels list.
 * if the band is 2.4G, then make sure channel list contains only 2.4G
 * valid channels if the band is 5G, then make sure channel list contains
 * only 5G valid channels
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS csr_create_roam_scan_channel_list(struct mac_context *mac,
					     uint8_t sessionId,
					     uint32_t *chan_freq_list,
					     uint8_t numChannels,
					     const enum band_info band)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	tpCsrNeighborRoamControlInfo pNeighborRoamInfo
		= &mac->roam.neighborRoamInfo[sessionId];
	uint8_t out_num_chan = 0;
	uint8_t inNumChannels = numChannels;
	uint32_t *in_ptr = chan_freq_list;
	uint8_t i = 0;
	uint32_t csr_freq_list[CFG_VALID_CHANNEL_LIST_LEN] = { 0 };
	uint32_t tmp_chan_freq_list[CFG_VALID_CHANNEL_LIST_LEN] = { 0 };
	uint8_t mergedOutputNumOfChannels = 0;

	tpCsrChannelInfo currChannelListInfo
		= &pNeighborRoamInfo->roamChannelInfo.currentChannelListInfo;
	/*
	 * Create a Union of occupied channel list learnt by the DUT along
	 * with the Neighbor report Channels. This increases the chances of
	 * the DUT to get a candidate AP while roaming even if the Neighbor
	 * Report is not able to provide sufficient information.
	 */
	if (mac->scan.occupiedChannels[sessionId].numChannels) {
		csr_neighbor_roam_merge_channel_lists(mac, &mac->scan.
						occupiedChannels[sessionId].
						channel_freq_list[0], mac->scan.
						occupiedChannels[sessionId].
						numChannels, in_ptr,
						inNumChannels,
						&mergedOutputNumOfChannels);
		inNumChannels = mergedOutputNumOfChannels;
	}
	if (BAND_2G == band) {
		for (i = 0; i < inNumChannels; i++) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(in_ptr[i]) &&
			    csr_roam_is_channel_valid(mac, in_ptr[i])) {
				csr_freq_list[out_num_chan++] = in_ptr[i];
			}
		}
	} else if (BAND_5G == band) {
		for (i = 0; i < inNumChannels; i++) {
			/* Add 5G Non-DFS channel */
			if (WLAN_REG_IS_5GHZ_CH_FREQ(in_ptr[i]) &&
			    csr_roam_is_channel_valid(mac, in_ptr[i]) &&
			    !wlan_reg_is_dfs_for_freq(mac->pdev, in_ptr[i])) {
				csr_freq_list[out_num_chan++] = in_ptr[i];
			}
		}
	} else if (BAND_ALL == band) {
		for (i = 0; i < inNumChannels; i++) {
			if (csr_roam_is_channel_valid(mac, in_ptr[i]) &&
			    !wlan_reg_is_dfs_for_freq(mac->pdev, in_ptr[i])) {
				csr_freq_list[out_num_chan++] = in_ptr[i];
			}
		}
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_WARN,
			  "Invalid band, No operation carried out (Band %d)",
			  band);
		return QDF_STATUS_E_INVAL;
	}
	/*
	 * if roaming within band is enabled, then select only the
	 * in band channels .
	 * This is required only if the band capability is set to ALL,
	 * E.g., if band capability is only 2.4G then all the channels in the
	 * list are already filtered for 2.4G channels, hence ignore this check
	 */
	if ((BAND_ALL == band) && CSR_IS_ROAM_INTRA_BAND_ENABLED(mac)) {
		csr_neighbor_roam_channels_filter_by_current_band(
						mac,
						sessionId,
						csr_freq_list,
						out_num_chan,
						tmp_chan_freq_list,
						&out_num_chan);
	}
	/* Prepare final roam scan channel list */
	if (out_num_chan) {
		/* Clear the channel list first */
		if (currChannelListInfo->freq_list) {
			qdf_mem_free(currChannelListInfo->freq_list);
			currChannelListInfo->freq_list = NULL;
			currChannelListInfo->numOfChannels = 0;
		}
		currChannelListInfo->freq_list =
			qdf_mem_malloc(out_num_chan * sizeof(uint32_t));
		if (!currChannelListInfo->freq_list) {
			currChannelListInfo->numOfChannels = 0;
			return QDF_STATUS_E_NOMEM;
		}
		for (i = 0; i < out_num_chan; i++)
			currChannelListInfo->freq_list[i] =
				tmp_chan_freq_list[i];

		currChannelListInfo->numOfChannels = out_num_chan;
	}
	return status;
}

/**
 * csr_roam_is_ese_assoc() - is this ese association
 * @mac_ctx: Global MAC context
 * @session_id: session identifier
 *
 * Returns whether the current association is a ESE assoc or not.
 *
 * Return: true if ese association; false otherwise
 */
bool csr_roam_is_ese_assoc(struct mac_context *mac_ctx, uint32_t session_id)
{
	return mac_ctx->roam.neighborRoamInfo[session_id].isESEAssoc;
}


/**
 * csr_roam_is_ese_ini_feature_enabled() - is ese feature enabled
 * @mac_ctx: Global MAC context
 *
 * Return: true if ese feature is enabled; false otherwise
 */
bool csr_roam_is_ese_ini_feature_enabled(struct mac_context *mac)
{
	return mac->mlme_cfg->lfr.ese_enabled;
}

/**
 * csr_tsm_stats_rsp_processor() - tsm stats response processor
 * @mac: Global MAC context
 * @pMsg: Message pointer
 *
 * Return: None
 */
static void csr_tsm_stats_rsp_processor(struct mac_context *mac, void *pMsg)
{
	tAniGetTsmStatsRsp *pTsmStatsRsp = (tAniGetTsmStatsRsp *) pMsg;

	if (pTsmStatsRsp) {
		/*
		 * Get roam Rssi request is backed up and passed back
		 * to the response, Extract the request message
		 * to fetch callback.
		 */
		tpAniGetTsmStatsReq reqBkp
			= (tAniGetTsmStatsReq *) pTsmStatsRsp->tsmStatsReq;

		if (reqBkp) {
			if (reqBkp->tsmStatsCallback) {
				((tCsrTsmStatsCallback)
				 (reqBkp->tsmStatsCallback))(pTsmStatsRsp->
							     tsmMetrics,
							     reqBkp->
							     pDevContext);
				reqBkp->tsmStatsCallback = NULL;
			}
			qdf_mem_free(reqBkp);
			pTsmStatsRsp->tsmStatsReq = NULL;
		} else {
			if (reqBkp) {
				qdf_mem_free(reqBkp);
				pTsmStatsRsp->tsmStatsReq = NULL;
			}
		}
	} else {
		sme_err("pTsmStatsRsp is NULL");
	}
}

/**
 * csr_send_ese_adjacent_ap_rep_ind() - ese send adjacent ap report
 * @mac: Global MAC context
 * @pSession: Session pointer
 *
 * Return: None
 */
static void csr_send_ese_adjacent_ap_rep_ind(struct mac_context *mac,
					struct csr_roam_session *pSession)
{
	uint32_t roamTS2 = 0;
	struct csr_roam_info *roam_info;
	struct pe_session *pe_session = NULL;
	uint8_t sessionId = WLAN_UMAC_VDEV_ID_MAX;

	if (!pSession) {
		sme_err("pSession is NULL");
		return;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	roamTS2 = qdf_mc_timer_get_system_time();
	roam_info->tsmRoamDelay = roamTS2 - pSession->roamTS1;
	sme_debug("Bssid(" QDF_MAC_ADDR_FMT ") Roaming Delay(%u ms)",
		QDF_MAC_ADDR_REF(pSession->connectedProfile.bssid.bytes),
		roam_info->tsmRoamDelay);

	pe_session = pe_find_session_by_bssid(mac,
					 pSession->connectedProfile.bssid.bytes,
					 &sessionId);
	if (!pe_session) {
		sme_err("session %d not found", sessionId);
		qdf_mem_free(roam_info);
		return;
	}

	pe_session->eseContext.tsm.tsmMetrics.RoamingDly
		= roam_info->tsmRoamDelay;

	csr_roam_call_callback(mac, pSession->sessionId, roam_info,
			       0, eCSR_ROAM_ESE_ADJ_AP_REPORT_IND, 0);
	qdf_mem_free(roam_info);
}

/**
 * csr_get_tsm_stats() - get tsm stats
 * @mac: Global MAC context
 * @callback: TSM stats callback
 * @staId: Station id
 * @bssId: bssid
 * @pContext: pointer to context
 * @tid: traffic id
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS csr_get_tsm_stats(struct mac_context *mac,
			     tCsrTsmStatsCallback callback,
			     struct qdf_mac_addr bssId,
			     void *pContext, uint8_t tid)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tAniGetTsmStatsReq *pMsg = NULL;

	pMsg = qdf_mem_malloc(sizeof(tAniGetTsmStatsReq));
	if (!pMsg) {
		return QDF_STATUS_E_NOMEM;
	}
	/* need to initiate a stats request to PE */
	pMsg->msgType = eWNI_SME_GET_TSM_STATS_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniGetTsmStatsReq);
	pMsg->tid = tid;
	qdf_copy_macaddr(&pMsg->bssId, &bssId);
	pMsg->tsmStatsCallback = callback;
	pMsg->pDevContext = pContext;
	status = umac_send_mb_message_to_mac(pMsg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Failed to send down the TSM req (status=%d)", status);
		/* pMsg is freed by cds_send_mb_message_to_mac */
		status = QDF_STATUS_E_FAILURE;
	}
	return status;
}

/**
 * csr_set_cckm_ie() - set CCKM IE
 * @mac: Global MAC context
 * @sessionId: session identifier
 * @pCckmIe: Pointer to input CCKM IE data
 * @ccKmIeLen: Length of @pCckmIe
 *
 * This function stores the CCKM IE passed by the supplicant
 * in a place holder data structure and this IE will be packed inside
 * reassociation request
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS csr_set_cckm_ie(struct mac_context *mac, const uint8_t sessionId,
			   const uint8_t *pCckmIe, const uint8_t ccKmIeLen)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	qdf_mem_copy(pSession->suppCckmIeInfo.cckmIe, pCckmIe, ccKmIeLen);
	pSession->suppCckmIeInfo.cckmIeLen = ccKmIeLen;
	return status;
}

/**
 * csr_roam_read_tsf() - read TSF
 * @mac: Global MAC context
 * @sessionId: session identifier
 * @pTimestamp: output TSF timestamp
 *
 * This function reads the TSF; and also add the time elapsed since
 * last beacon or probe response reception from the hand off AP to arrive at
 * the latest TSF value.
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS csr_roam_read_tsf(struct mac_context *mac, uint8_t *pTimestamp,
			     uint8_t sessionId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tCsrNeighborRoamBSSInfo handoffNode = {{0} };
	uint64_t timer_diff = 0;
	uint32_t timeStamp[2];
	struct bss_description *pBssDescription = NULL;

	csr_neighbor_roam_get_handoff_ap_info(mac, &handoffNode, sessionId);
	if (!handoffNode.pBssDescription) {
		sme_err("Invalid BSS Description");
		return QDF_STATUS_E_INVAL;
	}
	pBssDescription = handoffNode.pBssDescription;
	/* Get the time diff in nano seconds */
	timer_diff = (qdf_get_monotonic_boottime_ns()  -
				pBssDescription->scansystimensec);
	/* Convert msec to micro sec timer */
	timer_diff = do_div(timer_diff, SYSTEM_TIME_NSEC_TO_USEC);
	timeStamp[0] = pBssDescription->timeStamp[0];
	timeStamp[1] = pBssDescription->timeStamp[1];
	update_cckmtsf(&(timeStamp[0]), &(timeStamp[1]), &timer_diff);
	qdf_mem_copy(pTimestamp, (void *)&timeStamp[0], sizeof(uint32_t) * 2);
	return status;
}

#endif /* FEATURE_WLAN_ESE */

/**
 * csr_roam_is_roam_offload_scan_enabled() - is roam offload enabled
 * @mac_ctx: Global MAC context
 *
 * Returns whether firmware based background scan is currently enabled or not.
 *
 * Return: true if roam offload scan enabled; false otherwise
 */
bool csr_roam_is_roam_offload_scan_enabled(struct mac_context *mac_ctx)
{
	return mac_ctx->mlme_cfg->lfr.roam_scan_offload_enabled;
}

/* The funcns csr_convert_cb_ini_value_to_phy_cb_state and
 * csr_convert_phy_cb_state_to_ini_value have been introduced
 * to convert the ini value to the ENUM used in csr and MAC for CB state
 * Ideally we should have kept the ini value and enum value same and
 * representing the same cb values as in 11n standard i.e.
 * Set to 1 (SCA) if the secondary channel is above the primary channel
 * Set to 3 (SCB) if the secondary channel is below the primary channel
 * Set to 0 (SCN) if no secondary channel is present
 * However, since our driver is already distributed we will keep the ini
 * definition as it is which is:
 * 0 - secondary none
 * 1 - secondary LOW
 * 2 - secondary HIGH
 * and convert to enum value used within the driver in
 * csr_change_default_config_param using this funcn
 * The enum values are as follows:
 * PHY_SINGLE_CHANNEL_CENTERED          = 0
 * PHY_DOUBLE_CHANNEL_LOW_PRIMARY   = 1
 * PHY_DOUBLE_CHANNEL_HIGH_PRIMARY  = 3
 */
ePhyChanBondState csr_convert_cb_ini_value_to_phy_cb_state(uint32_t cbIniValue)
{

	ePhyChanBondState phyCbState;

	switch (cbIniValue) {
	/* secondary none */
	case eCSR_INI_SINGLE_CHANNEL_CENTERED:
		phyCbState = PHY_SINGLE_CHANNEL_CENTERED;
		break;
	/* secondary LOW */
	case eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY:
		phyCbState = PHY_DOUBLE_CHANNEL_HIGH_PRIMARY;
		break;
	/* secondary HIGH */
	case eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY:
		phyCbState = PHY_DOUBLE_CHANNEL_LOW_PRIMARY;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
		phyCbState =
			PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
		break;
	case eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
		phyCbState = PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH;
		break;
	default:
		/* If an invalid value is passed, disable CHANNEL BONDING */
		phyCbState = PHY_SINGLE_CHANNEL_CENTERED;
		break;
	}
	return phyCbState;
}

static
uint32_t csr_convert_phy_cb_state_to_ini_value(ePhyChanBondState phyCbState)
{
	uint32_t cbIniValue;

	switch (phyCbState) {
	/* secondary none */
	case PHY_SINGLE_CHANNEL_CENTERED:
		cbIniValue = eCSR_INI_SINGLE_CHANNEL_CENTERED;
		break;
	/* secondary LOW */
	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		cbIniValue = eCSR_INI_DOUBLE_CHANNEL_HIGH_PRIMARY;
		break;
	/* secondary HIGH */
	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		cbIniValue = eCSR_INI_DOUBLE_CHANNEL_LOW_PRIMARY;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
		cbIniValue =
			eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
		cbIniValue =
		eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
		cbIniValue =
			eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH;
		break;
	case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
		cbIniValue = eCSR_INI_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH;
		break;
	default:
		/* return some invalid value */
		cbIniValue = eCSR_INI_CHANNEL_BONDING_STATE_MAX;
		break;
	}
	return cbIniValue;
}

#ifdef WLAN_FEATURE_11AX
void csr_update_session_he_cap(struct mac_context *mac_ctx,
			       struct csr_roam_session *session)
{
	enum QDF_OPMODE persona;
	tDot11fIEhe_cap *he_cap;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;
	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	qdf_mem_copy(&mlme_priv->he_config,
		     &mac_ctx->mlme_cfg->he_caps.dot11_he_cap,
		     sizeof(mlme_priv->he_config));
	he_cap = &mlme_priv->he_config;
	he_cap->present = true;
	/*
	 * Do not advertise requester role for SAP & responder role
	 * for STA
	 */
	persona = wlan_vdev_mlme_get_opmode(vdev);
	if (persona == QDF_SAP_MODE || persona == QDF_P2P_GO_MODE) {
		he_cap->twt_request = 0;
	} else if (persona == QDF_STA_MODE || persona == QDF_P2P_CLIENT_MODE) {
		he_cap->twt_responder = 0;
	}

	if (he_cap->ppet_present) {
		/* till now operating channel is not decided yet, use 5g cap */
		qdf_mem_copy(he_cap->ppet.ppe_threshold.ppe_th,
			     mac_ctx->mlme_cfg->he_caps.he_ppet_5g,
			     WNI_CFG_HE_PPET_LEN);
		he_cap->ppet.ppe_threshold.num_ppe_th =
			lim_truncate_ppet(he_cap->ppet.ppe_threshold.ppe_th,
					  WNI_CFG_HE_PPET_LEN);
	} else {
		he_cap->ppet.ppe_threshold.num_ppe_th = 0;
	}
	mlme_priv->he_sta_obsspd = mac_ctx->mlme_cfg->he_caps.he_sta_obsspd;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#endif

/**
 * csr_set_11k_offload_config_param() - Update 11k neighbor report config
 *
 * @csr_config: pointer to csr_config in MAC context
 * @pParam: pointer to config params from HDD
 *
 * Return: none
 */
static
void csr_set_11k_offload_config_param(struct csr_config *csr_config,
					struct csr_config_params *param)
{
	csr_config->offload_11k_enable_bitmask =
		param->offload_11k_enable_bitmask;
	csr_config->neighbor_report_offload.params_bitmask =
		param->neighbor_report_offload.params_bitmask;
	csr_config->neighbor_report_offload.time_offset =
		param->neighbor_report_offload.time_offset;
	csr_config->neighbor_report_offload.low_rssi_offset =
		param->neighbor_report_offload.low_rssi_offset;
	csr_config->neighbor_report_offload.bmiss_count_trigger =
		param->neighbor_report_offload.bmiss_count_trigger;
	csr_config->neighbor_report_offload.per_threshold_offset =
		param->neighbor_report_offload.per_threshold_offset;
	csr_config->neighbor_report_offload.
		neighbor_report_cache_timeout =
		param->neighbor_report_offload.
		neighbor_report_cache_timeout;
	csr_config->neighbor_report_offload.
		max_neighbor_report_req_cap =
		param->neighbor_report_offload.
		max_neighbor_report_req_cap;
}

QDF_STATUS csr_change_default_config_param(struct mac_context *mac,
					   struct csr_config_params *pParam)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (pParam) {
		mac->roam.configParam.is_force_1x1 =
			pParam->is_force_1x1;
		mac->roam.configParam.WMMSupportMode = pParam->WMMSupportMode;
		mac->mlme_cfg->wmm_params.wme_enabled =
			(pParam->WMMSupportMode == eCsrRoamWmmNoQos) ? 0 : 1;
		mac->roam.configParam.Is11eSupportEnabled =
			pParam->Is11eSupportEnabled;

		mac->roam.configParam.fenableMCCMode = pParam->fEnableMCCMode;
		mac->roam.configParam.mcc_rts_cts_prot_enable =
						pParam->mcc_rts_cts_prot_enable;
		mac->roam.configParam.mcc_bcast_prob_resp_enable =
					pParam->mcc_bcast_prob_resp_enable;
		mac->roam.configParam.fAllowMCCGODiffBI =
			pParam->fAllowMCCGODiffBI;

		/* channelBondingMode5GHz plays a dual role right now
		 * INFRA STA will use this non zero value as CB enabled
		 * and SOFTAP will use this non-zero value to determine
		 * the secondary channel offset. This is how
		 * channelBondingMode5GHz works now and this is kept intact
		 * to avoid any cfg.ini change.
		 */
		if (pParam->channelBondingMode24GHz > MAX_CB_VALUE_IN_INI)
			sme_warn("Invalid CB value from ini in 2.4GHz band %d, CB DISABLED",
				pParam->channelBondingMode24GHz);
		mac->roam.configParam.channelBondingMode24GHz =
			csr_convert_cb_ini_value_to_phy_cb_state(pParam->
						channelBondingMode24GHz);
		if (pParam->channelBondingMode5GHz > MAX_CB_VALUE_IN_INI)
			sme_warn("Invalid CB value from ini in 5GHz band %d, CB DISABLED",
				pParam->channelBondingMode5GHz);
		mac->roam.configParam.channelBondingMode5GHz =
			csr_convert_cb_ini_value_to_phy_cb_state(pParam->
							channelBondingMode5GHz);
		mac->roam.configParam.phyMode = pParam->phyMode;
		mac->roam.configParam.HeartbeatThresh24 =
			mac->mlme_cfg->timeouts.heart_beat_threshold;
		mac->roam.configParam.HeartbeatThresh50 =
			pParam->HeartbeatThresh50;
		mac->roam.configParam.ProprietaryRatesEnabled =
			pParam->ProprietaryRatesEnabled;

		mac->roam.configParam.wep_tkip_in_he = pParam->wep_tkip_in_he;

		mac->roam.configParam.uCfgDot11Mode =
			csr_get_cfg_dot11_mode_from_csr_phy_mode(NULL,
							mac->roam.configParam.
							phyMode,
							mac->roam.configParam.
						ProprietaryRatesEnabled);

		if (pParam->bCatRssiOffset)
			mac->roam.configParam.bCatRssiOffset =
							pParam->bCatRssiOffset;
		/* Assign this before calling csr_init11d_info */
		if (wlan_reg_11d_enabled_on_host(mac->psoc))
			status = csr_init11d_info(mac, &pParam->Csr11dinfo);
		else
			mac->scan.curScanType = eSIR_ACTIVE_SCAN;

		/* Initialize the power + channel information if 11h is
		 * enabled. If 11d is enabled this information has already
		 * been initialized
		 */
		if (csr_is11h_supported(mac) &&
				!wlan_reg_11d_enabled_on_host(mac->psoc))
			csr_init_channel_power_list(mac, &pParam->Csr11dinfo);

		mac->scan.fEnableDFSChnlScan = pParam->fEnableDFSChnlScan;
		/* This parameter is not available in cfg and not passed from
		 * upper layers. Instead it is initialized here This parametere
		 * is used in concurrency to determine if there are concurrent
		 * active sessions. Is used as a temporary fix to disconnect
		 * all active sessions when BMPS enabled so the active session
		 * if Infra STA will automatically connect back and resume BMPS
		 * since resume BMPS is not working when moving from concurrent
		 * to single session
		 */
		/* Remove this code once SLM_Sessionization is supported */
		/* BMPS_WORKAROUND_NOT_NEEDED */
		mac->roam.configParam.doBMPSWorkaround = 0;
		mac->roam.configParam.send_smps_action =
			pParam->send_smps_action;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		mac->roam.configParam.cc_switch_mode = pParam->cc_switch_mode;
#endif
		mac->roam.configParam.obssEnabled = pParam->obssEnabled;
		mac->roam.configParam.conc_custom_rule1 =
			pParam->conc_custom_rule1;
		mac->roam.configParam.conc_custom_rule2 =
			pParam->conc_custom_rule2;
		mac->roam.configParam.is_sta_connection_in_5gz_enabled =
			pParam->is_sta_connection_in_5gz_enabled;

		/* update interface configuration */
		mac->sme.max_intf_count = pParam->max_intf_count;

		mac->f_sta_miracast_mcc_rest_time_val =
			pParam->f_sta_miracast_mcc_rest_time_val;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
		mac->sap.sap_channel_avoidance =
			pParam->sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
		mac->roam.configParam.sta_roam_policy.dfs_mode =
			pParam->sta_roam_policy_params.dfs_mode;
		mac->roam.configParam.sta_roam_policy.skip_unsafe_channels =
			pParam->sta_roam_policy_params.skip_unsafe_channels;
		mac->roam.configParam.sta_roam_policy.sap_operating_band =
			pParam->sta_roam_policy_params.sap_operating_band;

		csr_set_11k_offload_config_param(&mac->roam.configParam,
						 pParam);
	}
	return status;
}

/**
 * csr_get_11k_offload_config_param() - Get 11k neighbor report config
 *
 * @csr_config: pointer to csr_config in MAC context
 * @pParam: pointer to config params from HDD
 *
 * Return: none
 */
static
void csr_get_11k_offload_config_param(struct csr_config *csr_config,
					struct csr_config_params *param)
{
	param->offload_11k_enable_bitmask =
		csr_config->offload_11k_enable_bitmask;
	param->neighbor_report_offload.params_bitmask =
		csr_config->neighbor_report_offload.params_bitmask;
	param->neighbor_report_offload.time_offset =
		csr_config->neighbor_report_offload.time_offset;
	param->neighbor_report_offload.low_rssi_offset =
		csr_config->neighbor_report_offload.low_rssi_offset;
	param->neighbor_report_offload.bmiss_count_trigger =
		csr_config->neighbor_report_offload.bmiss_count_trigger;
	param->neighbor_report_offload.per_threshold_offset =
		csr_config->neighbor_report_offload.per_threshold_offset;
	param->neighbor_report_offload.neighbor_report_cache_timeout =
		csr_config->neighbor_report_offload.
		neighbor_report_cache_timeout;
	param->neighbor_report_offload.max_neighbor_report_req_cap =
		csr_config->neighbor_report_offload.
		max_neighbor_report_req_cap;
}

QDF_STATUS csr_get_config_param(struct mac_context *mac,
				struct csr_config_params *pParam)
{
	struct csr_config *cfg_params = &mac->roam.configParam;

	if (!pParam)
		return QDF_STATUS_E_INVAL;

	pParam->is_force_1x1 = cfg_params->is_force_1x1;
	pParam->WMMSupportMode = cfg_params->WMMSupportMode;
	pParam->Is11eSupportEnabled = cfg_params->Is11eSupportEnabled;
	pParam->channelBondingMode24GHz = csr_convert_phy_cb_state_to_ini_value(
					cfg_params->channelBondingMode24GHz);
	pParam->channelBondingMode5GHz = csr_convert_phy_cb_state_to_ini_value(
					cfg_params->channelBondingMode5GHz);
	pParam->phyMode = cfg_params->phyMode;
	pParam->HeartbeatThresh50 = cfg_params->HeartbeatThresh50;
	pParam->ProprietaryRatesEnabled = cfg_params->ProprietaryRatesEnabled;
	pParam->bCatRssiOffset = cfg_params->bCatRssiOffset;
	pParam->fEnableDFSChnlScan = mac->scan.fEnableDFSChnlScan;
	pParam->fEnableMCCMode = cfg_params->fenableMCCMode;
	pParam->fAllowMCCGODiffBI = cfg_params->fAllowMCCGODiffBI;

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	pParam->cc_switch_mode = cfg_params->cc_switch_mode;
#endif
	pParam->wep_tkip_in_he = cfg_params->wep_tkip_in_he;
	csr_set_channels(mac, pParam);
	pParam->obssEnabled = cfg_params->obssEnabled;
	pParam->roam_dense_min_aps =
			cfg_params->roam_params.dense_min_aps_cnt;

	pParam->roam_bg_scan_bad_rssi_thresh =
		cfg_params->roam_params.bg_scan_bad_rssi_thresh;
	pParam->roam_bad_rssi_thresh_offset_2g =
		cfg_params->roam_params.roam_bad_rssi_thresh_offset_2g;
	pParam->roam_data_rssi_threshold_triggers =
		cfg_params->roam_params.roam_data_rssi_threshold_triggers;
	pParam->roam_data_rssi_threshold =
		cfg_params->roam_params.roam_data_rssi_threshold;
	pParam->rx_data_inactivity_time =
		cfg_params->roam_params.rx_data_inactivity_time;

	pParam->conc_custom_rule1 = cfg_params->conc_custom_rule1;
	pParam->conc_custom_rule2 = cfg_params->conc_custom_rule2;
	pParam->is_sta_connection_in_5gz_enabled =
		cfg_params->is_sta_connection_in_5gz_enabled;
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
	pParam->sap_channel_avoidance = mac->sap.sap_channel_avoidance;
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */
	pParam->max_intf_count = mac->sme.max_intf_count;
	pParam->f_sta_miracast_mcc_rest_time_val =
		mac->f_sta_miracast_mcc_rest_time_val;
	pParam->send_smps_action = mac->roam.configParam.send_smps_action;
	pParam->sta_roam_policy_params.dfs_mode =
		mac->roam.configParam.sta_roam_policy.dfs_mode;
	pParam->sta_roam_policy_params.skip_unsafe_channels =
		mac->roam.configParam.sta_roam_policy.skip_unsafe_channels;

	csr_get_11k_offload_config_param(&mac->roam.configParam, pParam);

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_prune_ch_list() - prunes the channel list to keep only a type of channels
 * @ch_lst:        existing channel list
 * @is_24_GHz:     indicates if 2.5 GHz or 5 GHz channels are required
 *
 * Return: void
 */
static void csr_prune_ch_list(struct csr_channel *ch_lst, bool is_24_GHz)
{
	uint8_t idx = 0, num_channels = 0;

	for ( ; idx < ch_lst->numChannels; idx++) {
		if (is_24_GHz) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_lst->channel_freq_list[idx])) {
				ch_lst->channel_freq_list[num_channels] =
					ch_lst->channel_freq_list[idx];
				num_channels++;
			}
		} else {
			if (WLAN_REG_IS_5GHZ_CH_FREQ(ch_lst->channel_freq_list[idx])) {
				ch_lst->channel_freq_list[num_channels] =
					ch_lst->channel_freq_list[idx];
				num_channels++;
			}
		}
	}
	/*
	 * Cleanup the rest of channels. Note we only need to clean up the
	 * channels if we had to trim the list. Calling qdf_mem_set() with a 0
	 * size is going to throw asserts on the debug builds so let's be a bit
	 * smarter about that. Zero out the reset of the channels only if we
	 * need to. The amount of memory to clear is the number of channesl that
	 * we trimmed (ch_lst->numChannels - num_channels) times the size of a
	 * channel in the structure.
	 */
	if (ch_lst->numChannels > num_channels) {
		qdf_mem_zero(&ch_lst->channel_freq_list[num_channels],
			     sizeof(ch_lst->channel_freq_list[0]) *
			     (ch_lst->numChannels - num_channels));
	}
	ch_lst->numChannels = num_channels;
}

/**
 * csr_prune_channel_list_for_mode() - prunes the channel list
 * @mac_ctx:       global mac context
 * @ch_lst:        existing channel list
 *
 * Prunes the channel list according to band stored in mac_ctx
 *
 * Return: void
 */
void csr_prune_channel_list_for_mode(struct mac_context *mac_ctx,
				     struct csr_channel *ch_lst)
{
	/* for dual band NICs, don't need to trim the channel list.... */
	if (CSR_IS_OPEARTING_DUAL_BAND(mac_ctx))
		return;
	/*
	 * 2.4 GHz band operation requires the channel list to be trimmed to
	 * the 2.4 GHz channels only
	 */
	if (CSR_IS_24_BAND_ONLY(mac_ctx))
		csr_prune_ch_list(ch_lst, true);
	else if (CSR_IS_5G_BAND_ONLY(mac_ctx))
		csr_prune_ch_list(ch_lst, false);
}

#define INFRA_AP_DEFAULT_CHAN_FREQ 2437

QDF_STATUS csr_get_channel_and_power_list(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t num20MHzChannelsFound = 0;
	QDF_STATUS qdf_status;
	uint8_t Index = 0;

	qdf_status = wlan_reg_get_channel_list_with_power_for_freq(
				mac->pdev,
				mac->scan.defaultPowerTable,
				&num20MHzChannelsFound);

	if ((QDF_STATUS_SUCCESS != qdf_status) ||
	    (num20MHzChannelsFound == 0)) {
		sme_err("failed to get channels");
		status = QDF_STATUS_E_FAILURE;
	} else {
		if (num20MHzChannelsFound > CFG_VALID_CHANNEL_LIST_LEN)
			num20MHzChannelsFound = CFG_VALID_CHANNEL_LIST_LEN;
		mac->scan.numChannelsDefault = num20MHzChannelsFound;
		/* Move the channel list to the global data */
		/* structure -- this will be used as the scan list */
		for (Index = 0; Index < num20MHzChannelsFound; Index++)
			mac->scan.base_channels.channel_freq_list[Index] =
				mac->scan.defaultPowerTable[Index].center_freq;
		mac->scan.base_channels.numChannels =
			num20MHzChannelsFound;
	}
	return status;
}

QDF_STATUS csr_apply_channel_and_power_list(struct mac_context *mac)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	csr_prune_channel_list_for_mode(mac, &mac->scan.base_channels);
	csr_save_channel_power_for_band(mac, false);
	csr_save_channel_power_for_band(mac, true);
	csr_apply_channel_power_info_to_fw(mac,
					   &mac->scan.base_channels,
					   mac->scan.countryCodeCurrent);

	csr_init_operating_classes(mac);
	return status;
}

static QDF_STATUS csr_init11d_info(struct mac_context *mac, tCsr11dinfo *ps11dinfo)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t index;
	uint32_t count = 0;
	tSirMacChanInfo *pChanInfo;
	tSirMacChanInfo *pChanInfoStart;
	bool applyConfig = true;

	if (!ps11dinfo)
		return status;

	if (ps11dinfo->Channels.numChannels
	    && (CFG_VALID_CHANNEL_LIST_LEN >=
		ps11dinfo->Channels.numChannels)) {
		mac->scan.base_channels.numChannels =
			ps11dinfo->Channels.numChannels;
		qdf_mem_copy(mac->scan.base_channels.channel_freq_list,
			     ps11dinfo->Channels.channel_freq_list,
			     ps11dinfo->Channels.numChannels);
	} else {
		/* No change */
		return QDF_STATUS_SUCCESS;
	}
	/* legacy maintenance */

	qdf_mem_copy(mac->scan.countryCodeDefault, ps11dinfo->countryCode,
		     REG_ALPHA2_LEN + 1);

	/* Tush: at csropen get this initialized with default,
	 * during csr reset if this already set with some value
	 * no need initilaize with default again
	 */
	if (0 == mac->scan.countryCodeCurrent[0]) {
		qdf_mem_copy(mac->scan.countryCodeCurrent,
			     ps11dinfo->countryCode, REG_ALPHA2_LEN + 1);
	}
	/* need to add the max power channel list */
	pChanInfo =
		qdf_mem_malloc(sizeof(tSirMacChanInfo) *
			       CFG_VALID_CHANNEL_LIST_LEN);
	if (pChanInfo) {
		pChanInfoStart = pChanInfo;
		for (index = 0; index < ps11dinfo->Channels.numChannels;
		     index++) {
			pChanInfo->first_freq = ps11dinfo->ChnPower[index].first_chan_freq;
			pChanInfo->numChannels =
				ps11dinfo->ChnPower[index].numChannels;
			pChanInfo->maxTxPower =
				ps11dinfo->ChnPower[index].maxtxPower;
			pChanInfo++;
			count++;
		}
		if (count) {
			status = csr_save_to_channel_power2_g_5_g(mac,
							 count *
							sizeof(tSirMacChanInfo),
							 pChanInfoStart);
		}
		qdf_mem_free(pChanInfoStart);
	}
	/* Only apply them to CFG when not in STOP state.
	 * Otherwise they will be applied later
	 */
	if (QDF_IS_STATUS_SUCCESS(status)) {
		for (index = 0; index < WLAN_MAX_VDEVS; index++) {
			if ((CSR_IS_SESSION_VALID(mac, index))
			    && CSR_IS_ROAM_STOP(mac, index)) {
				applyConfig = false;
			}
		}

		if (true == applyConfig) {
			/* Apply the base channel list, power info,
			 * and set the Country code.
			 */
			csr_apply_channel_power_info_to_fw(mac,
							   &mac->scan.
							   base_channels,
							   mac->scan.
							   countryCodeCurrent);
		}
	}
	return status;
}

/* Initialize the Channel + Power List in the local cache and in the CFG */
QDF_STATUS csr_init_channel_power_list(struct mac_context *mac,
					tCsr11dinfo *ps11dinfo)
{
	uint8_t index;
	uint32_t count = 0;
	tSirMacChanInfo *pChanInfo;
	tSirMacChanInfo *pChanInfoStart;

	if (!ps11dinfo || !mac)
		return QDF_STATUS_E_FAILURE;

	pChanInfo =
		qdf_mem_malloc(sizeof(tSirMacChanInfo) *
			       CFG_VALID_CHANNEL_LIST_LEN);
	if (pChanInfo) {
		pChanInfoStart = pChanInfo;

		for (index = 0; index < ps11dinfo->Channels.numChannels;
		     index++) {
			pChanInfo->first_freq = ps11dinfo->ChnPower[index].first_chan_freq;
			pChanInfo->numChannels =
				ps11dinfo->ChnPower[index].numChannels;
			pChanInfo->maxTxPower =
				ps11dinfo->ChnPower[index].maxtxPower;
			pChanInfo++;
			count++;
		}
		if (count) {
			csr_save_to_channel_power2_g_5_g(mac,
							 count *
							sizeof(tSirMacChanInfo),
							 pChanInfoStart);
		}
		qdf_mem_free(pChanInfoStart);
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_roam_remove_duplicate_cmd_from_list()- Remove duplicate roam cmd from
 * list
 *
 * @mac_ctx: pointer to global mac
 * @vdev_id: vdev_id for the cmd
 * @list: pending list from which cmd needs to be removed
 * @command: cmd to be removed, can be NULL
 * @roam_reason: cmd with reason to be removed
 *
 * Remove duplicate command from the pending list.
 *
 * Return: void
 */
static void csr_roam_remove_duplicate_pending_cmd_from_list(
			struct mac_context *mac_ctx,
			uint32_t vdev_id,
			tSmeCmd *command, enum csr_roam_reason roam_reason)
{
	tListElem *entry, *next_entry;
	tSmeCmd *dup_cmd;
	tDblLinkList local_list;

	qdf_mem_zero(&local_list, sizeof(tDblLinkList));
	if (!QDF_IS_STATUS_SUCCESS(csr_ll_open(&local_list))) {
		sme_err("failed to open list");
		return;
	}
	entry = csr_nonscan_pending_ll_peek_head(mac_ctx, LL_ACCESS_NOLOCK);
	while (entry) {
		next_entry = csr_nonscan_pending_ll_next(mac_ctx, entry,
						LL_ACCESS_NOLOCK);
		dup_cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		/*
		 * If command is not NULL remove the similar duplicate cmd for
		 * same reason as command. If command is NULL then check if
		 * roam_reason is eCsrForcedDisassoc (disconnect) and remove
		 * all roam command for the sessionId, else if roam_reason is
		 * eCsrHddIssued (connect) remove all connect (non disconenct)
		 * commands.
		 */
		if ((command && (command->vdev_id == dup_cmd->vdev_id) &&
			((command->command == dup_cmd->command) &&
			/*
			 * This peermac check is required for Softap/GO
			 * scenarios. for STA scenario below OR check will
			 * suffice as command will always be NULL for
			 * STA scenarios
			 */
			(!qdf_mem_cmp(dup_cmd->u.roamCmd.peerMac,
				command->u.roamCmd.peerMac,
					sizeof(QDF_MAC_ADDR_SIZE))) &&
				((command->u.roamCmd.roamReason ==
					dup_cmd->u.roamCmd.roamReason) ||
				(eCsrForcedDisassoc ==
					command->u.roamCmd.roamReason) ||
				(eCsrHddIssued ==
					command->u.roamCmd.roamReason)))) ||
			/* OR if pCommand is NULL */
			((vdev_id == dup_cmd->vdev_id) &&
			(eSmeCommandRoam == dup_cmd->command) &&
			((eCsrForcedDisassoc == roam_reason) ||
			(eCsrHddIssued == roam_reason &&
			!CSR_IS_DISCONNECT_COMMAND(dup_cmd))))) {
			sme_debug("RoamReason: %d",
					dup_cmd->u.roamCmd.roamReason);
			/* Insert to local_list and remove later */
			csr_ll_insert_tail(&local_list, entry,
					   LL_ACCESS_NOLOCK);
		}
		entry = next_entry;
	}

	while ((entry = csr_ll_remove_head(&local_list, LL_ACCESS_NOLOCK))) {
		dup_cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		/* Tell caller that the command is cancelled */
		csr_roam_call_callback(mac_ctx, dup_cmd->vdev_id, NULL,
				dup_cmd->u.roamCmd.roamId,
				eCSR_ROAM_CANCELLED, eCSR_ROAM_RESULT_NONE);
		csr_release_command(mac_ctx, dup_cmd);
	}
	csr_ll_close(&local_list);
}

/**
 * csr_roam_remove_duplicate_command()- Remove duplicate roam cmd
 * from pending lists.
 *
 * @mac_ctx: pointer to global mac
 * @session_id: session id for the cmd
 * @command: cmd to be removed, can be null
 * @roam_reason: cmd with reason to be removed
 *
 * Remove duplicate command from the sme and roam pending list.
 *
 * Return: void
 */
void csr_roam_remove_duplicate_command(struct mac_context *mac_ctx,
			uint32_t session_id, tSmeCmd *command,
			enum csr_roam_reason roam_reason)
{
	csr_roam_remove_duplicate_pending_cmd_from_list(mac_ctx,
		session_id, command, roam_reason);
}

/**
 * csr_roam_populate_channels() - Helper function to populate channels
 * @beacon_ies: pointer to beacon ie
 * @roam_info: Roaming related information
 * @chan1: center freq 1
 * @chan2: center freq2
 *
 * This function will issue populate chan1 and chan2 based on beacon ie
 *
 * Return: none.
 */
static void csr_roam_populate_channels(tDot11fBeaconIEs *beacon_ies,
			struct csr_roam_info *roam_info,
			uint8_t *chan1, uint8_t *chan2)
{
	ePhyChanBondState phy_state;

	if (beacon_ies->VHTOperation.present) {
		*chan1 = beacon_ies->VHTOperation.chan_center_freq_seg0;
		*chan2 = beacon_ies->VHTOperation.chan_center_freq_seg1;
		roam_info->chan_info.info = MODE_11AC_VHT80;
	} else if (beacon_ies->HTInfo.present) {
		if (beacon_ies->HTInfo.recommendedTxWidthSet ==
			eHT_CHANNEL_WIDTH_40MHZ) {
			phy_state = beacon_ies->HTInfo.secondaryChannelOffset;
			if (phy_state == PHY_DOUBLE_CHANNEL_LOW_PRIMARY)
				*chan1 = beacon_ies->HTInfo.primaryChannel +
						CSR_CB_CENTER_CHANNEL_OFFSET;
			else if (phy_state == PHY_DOUBLE_CHANNEL_HIGH_PRIMARY)
				*chan1 = beacon_ies->HTInfo.primaryChannel -
						CSR_CB_CENTER_CHANNEL_OFFSET;
			else
				*chan1 = beacon_ies->HTInfo.primaryChannel;

			roam_info->chan_info.info = MODE_11NA_HT40;
		} else {
			*chan1 = beacon_ies->HTInfo.primaryChannel;
			roam_info->chan_info.info = MODE_11NA_HT20;
		}
		*chan2 = 0;
	} else {
		*chan1 = 0;
		*chan2 = 0;
		roam_info->chan_info.info = MODE_11A;
	}
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static const char *csr_get_ch_width_str(uint8_t ch_width)
{
	switch (ch_width) {
	CASE_RETURN_STRING(BW_20MHZ);
	CASE_RETURN_STRING(BW_40MHZ);
	CASE_RETURN_STRING(BW_80MHZ);
	CASE_RETURN_STRING(BW_160MHZ);
	CASE_RETURN_STRING(BW_80P80MHZ);
	CASE_RETURN_STRING(BW_5MHZ);
	CASE_RETURN_STRING(BW_10MHZ);
	default:
		return "Unknown";
	}
}

static const char *csr_get_persona(enum mgmt_bss_type persona)
{
	switch (persona) {
	CASE_RETURN_STRING(STA_PERSONA);
	CASE_RETURN_STRING(SAP_PERSONA);
	CASE_RETURN_STRING(P2P_CLIENT_PERSONA);
	CASE_RETURN_STRING(P2P_GO_PERSONA);
	CASE_RETURN_STRING(FTM_PERSONA);
	CASE_RETURN_STRING(MONITOR_PERSONA);
	CASE_RETURN_STRING(P2P_DEVICE_PERSONA);
	CASE_RETURN_STRING(NDI_PERSONA);
	CASE_RETURN_STRING(WDS_PERSONA);
	default:
		return "Unknown";
	}
}

static const char *csr_get_dot11_mode_str(enum mgmt_dot11_mode dot11mode)
{
	switch (dot11mode) {
	CASE_RETURN_STRING(DOT11_MODE_AUTO);
	CASE_RETURN_STRING(DOT11_MODE_ABG);
	CASE_RETURN_STRING(DOT11_MODE_11A);
	CASE_RETURN_STRING(DOT11_MODE_11B);
	CASE_RETURN_STRING(DOT11_MODE_11G);
	CASE_RETURN_STRING(DOT11_MODE_11N);
	CASE_RETURN_STRING(DOT11_MODE_11AC);
	CASE_RETURN_STRING(DOT11_MODE_11G_ONLY);
	CASE_RETURN_STRING(DOT11_MODE_11N_ONLY);
	CASE_RETURN_STRING(DOT11_MODE_11AC_ONLY);
	CASE_RETURN_STRING(DOT11_MODE_11AX);
	CASE_RETURN_STRING(DOT11_MODE_11AX_ONLY);
	default:
		return "Unknown";
	}
}

static const char *csr_get_auth_type_str(uint8_t auth_type)
{
	switch (auth_type) {
	CASE_RETURN_STRING(AUTH_OPEN);
	CASE_RETURN_STRING(AUTH_SHARED);
	CASE_RETURN_STRING(AUTH_WPA_EAP);
	CASE_RETURN_STRING(AUTH_WPA_PSK);
	CASE_RETURN_STRING(AUTH_WPA2_EAP);
	CASE_RETURN_STRING(AUTH_WPA2_PSK);
	CASE_RETURN_STRING(AUTH_WAPI_CERT);
	CASE_RETURN_STRING(AUTH_WAPI_PSK);
	default:
		return "Unknown";
	}
}

static const char *csr_get_encr_type_str(uint8_t encr_type)
{
	switch (encr_type) {
	CASE_RETURN_STRING(ENC_MODE_OPEN);
	CASE_RETURN_STRING(ENC_MODE_WEP40);
	CASE_RETURN_STRING(ENC_MODE_WEP104);
	CASE_RETURN_STRING(ENC_MODE_TKIP);
	CASE_RETURN_STRING(ENC_MODE_AES);
	CASE_RETURN_STRING(ENC_MODE_AES_GCMP);
	CASE_RETURN_STRING(ENC_MODE_AES_GCMP_256);
	CASE_RETURN_STRING(ENC_MODE_SMS4);
	default:
		return "Unknown";
	}
}

static const uint8_t *csr_get_akm_str(uint8_t akm)
{
	switch (akm) {
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		return "Open";
	case eCSR_AUTH_TYPE_SHARED_KEY:
		return "Shared Key";
	case eCSR_AUTH_TYPE_SAE:
		return "SAE";
	case eCSR_AUTH_TYPE_WPA:
		return "WPA";
	case eCSR_AUTH_TYPE_WPA_PSK:
		return "WPA-PSK";
	case eCSR_AUTH_TYPE_WPA_NONE:
		return "WPA-NONE";
	case eCSR_AUTH_TYPE_RSN:
		return "EAP 802.1x";
	case eCSR_AUTH_TYPE_RSN_PSK:
		return "WPA2-PSK";
	case eCSR_AUTH_TYPE_FT_RSN:
		return "FT-802.1x";
	case eCSR_AUTH_TYPE_FT_RSN_PSK:
		return "FT-PSK";
	case eCSR_AUTH_TYPE_CCKM_WPA:
		return "WPA-CCKM";
	case eCSR_AUTH_TYPE_CCKM_RSN:
		return "RSN-CCKM";
	case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
		return "PSK-SHA256";
	case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
		return "EAP 802.1x-SHA256";
	case eCSR_AUTH_TYPE_FILS_SHA256:
		return "FILS-SHA256";
	case eCSR_AUTH_TYPE_FILS_SHA384:
		return "FILS-SHA384";
	case eCSR_AUTH_TYPE_FT_FILS_SHA256:
		return "FILS-SHA256";
	case eCSR_AUTH_TYPE_FT_FILS_SHA384:
		return "FILS-SHA384";
	case eCSR_AUTH_TYPE_DPP_RSN:
		return "DPP";
	case eCSR_AUTH_TYPE_OWE:
		return "OWE";
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA256:
		return "EAP Suite-B SHA256";
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA384:
		return "EAP Suite-B SHA384";
	case eCSR_AUTH_TYPE_OSEN:
		return "OSEN";
	case eCSR_AUTH_TYPE_FT_SAE:
		return "FT-SAE";
	case eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384:
		return "FT-Suite-B SHA384";
	default:
		return "NONE";
	}
}

/**
 * csr_get_sta_ap_intersected_nss  - Get the intersected NSS capability between
 * sta and connected AP.
 * @mac_ctx: Pointer to mac context
 * @vdev_id: Vdev id
 *
 * Return: NSS value
 */
static uint8_t
csr_get_sta_ap_intersected_nss(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	uint8_t intrsct_nss;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		pe_warn("vdev not found for id: %d", vdev_id);
		return 0;
	}
	wlan_vdev_obj_lock(vdev);
	intrsct_nss = wlan_vdev_mlme_get_nss(vdev);
	wlan_vdev_obj_unlock(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	return intrsct_nss;
}

static void
csr_connect_info(struct mac_context *mac_ctx,
		 struct csr_roam_session *session,
		 struct csr_roam_info *roam_info,
		 eCsrRoamResult u2)
{
	struct tagCsrRoamConnectedProfile *conn_profile;
	struct csr_roam_profile *profile;
	WLAN_HOST_DIAG_EVENT_DEF(conn_stats,
				 struct host_event_wlan_connection_stats);

	if (!session || !session->pCurRoamProfile || !roam_info)
		return;

	conn_profile = roam_info->u.pConnectedProfile;
	if (!conn_profile)
		return;
	profile = session->pCurRoamProfile;
	qdf_mem_zero(&conn_stats,
		    sizeof(struct host_event_wlan_connection_stats));
	qdf_mem_copy(conn_stats.bssid, conn_profile->bssid.bytes,
		     QDF_MAC_ADDR_SIZE);
	conn_stats.ssid_len = conn_profile->SSID.length;
	if (conn_stats.ssid_len > WLAN_SSID_MAX_LEN)
		conn_stats.ssid_len = WLAN_SSID_MAX_LEN;
	qdf_mem_copy(conn_stats.ssid, conn_profile->SSID.ssId,
		     conn_stats.ssid_len);
	sme_get_rssi_snr_by_bssid(MAC_HANDLE(mac_ctx),
				  session->pCurRoamProfile,
				  &conn_stats.bssid[0],
				  &conn_stats.rssi, NULL, session->vdev_id);
	conn_stats.est_link_speed = 0;
	conn_stats.chnl_bw =
		diag_ch_width_from_csr_type(conn_profile->vht_channel_width);
	conn_stats.dot11mode =
		diag_dot11_mode_from_csr_type(conn_profile->dot11Mode);
	conn_stats.bss_type =
	     diag_persona_from_csr_type(session->pCurRoamProfile->csrPersona);
	if (conn_profile->op_freq)
		conn_stats.operating_channel =
			wlan_reg_freq_to_chan(mac_ctx->pdev,
					      conn_profile->op_freq);

	conn_stats.qos_capability = conn_profile->qosConnection;
	conn_stats.auth_type =
	     diag_auth_type_from_csr_type(conn_profile->AuthType);
	conn_stats.encryption_type =
	     diag_enc_type_from_csr_type(conn_profile->EncryptionType);
	conn_stats.result_code = (u2 == eCSR_ROAM_RESULT_ASSOCIATED) ? 1 : 0;
	conn_stats.reason_code = 0;
	conn_stats.op_freq = conn_profile->op_freq;
	sme_nofl_debug("+---------CONNECTION INFO START------------+");
	sme_nofl_debug("VDEV-ID: %d self_mac:"QDF_MAC_ADDR_FMT, session->vdev_id,
		       QDF_MAC_ADDR_REF(session->self_mac_addr.bytes));
	sme_nofl_debug("ssid: %.*s bssid: "QDF_MAC_ADDR_FMT" RSSI: %d dBm",
		       conn_stats.ssid_len, conn_stats.ssid,
		       QDF_MAC_ADDR_REF(conn_stats.bssid), conn_stats.rssi);
	sme_nofl_debug("Channel Freq: %d channel_bw: %s dot11Mode: %s", conn_stats.op_freq,
		       csr_get_ch_width_str(conn_stats.chnl_bw),
		       csr_get_dot11_mode_str(conn_stats.dot11mode));
	sme_nofl_debug("AKM: %s Encry-type: %s",
		       csr_get_akm_str(conn_profile->AuthType),
		       csr_get_encr_type_str(conn_stats.encryption_type));
	sme_nofl_debug("DUT_NSS: %d | Intersected NSS:%d", session->vdev_nss,
		  csr_get_sta_ap_intersected_nss(mac_ctx, session->vdev_id));
	sme_nofl_debug("Qos enable: %d | Associated: %s",
		       conn_stats.qos_capability,
		       (conn_stats.result_code ? "yes" : "no"));
	sme_nofl_debug("+---------CONNECTION INFO END------------+");

	WLAN_HOST_DIAG_EVENT_REPORT(&conn_stats, EVENT_WLAN_CONN_STATS_V2);
}

void csr_get_sta_cxn_info(struct mac_context *mac_ctx,
			  struct csr_roam_session *session,
			  struct tagCsrRoamConnectedProfile *conn_profile,
			  char *buf, uint32_t buf_sz)
{
	int8_t rssi = 0;
	uint32_t nss, hw_mode;
	struct policy_mgr_conc_connection_info *conn_info;
	struct wma_txrx_node *wma_conn_table_entry = NULL;
	uint32_t i = 0, len = 0, max_cxn = 0;
	enum mgmt_ch_width ch_width;
	enum mgmt_dot11_mode dot11mode;
	enum mgmt_bss_type type;
	enum mgmt_auth_type authtype;
	enum mgmt_encrypt_type enctype;

	if (!session || !session->pCurRoamProfile || !conn_profile)
		return;
	if (!conn_profile->op_freq)
		return;
	qdf_mem_set(buf, buf_sz, '\0');
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\tbssid: "QDF_MAC_ADDR_FMT,
			     QDF_MAC_ADDR_REF(conn_profile->bssid.bytes));
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\tssid: %.*s", conn_profile->SSID.length,
			     conn_profile->SSID.ssId);
	sme_get_rssi_snr_by_bssid(MAC_HANDLE(mac_ctx),
				  session->pCurRoamProfile,
				  conn_profile->bssid.bytes,
				  &rssi, NULL, session->vdev_id);
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\trssi: %d", rssi);
	ch_width = diag_ch_width_from_csr_type(conn_profile->vht_channel_width);
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\tbw: %s", csr_get_ch_width_str(ch_width));
	dot11mode = diag_dot11_mode_from_csr_type(conn_profile->dot11Mode);
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\tdot11mode: %s",
			     csr_get_dot11_mode_str(dot11mode));
	type = diag_persona_from_csr_type(session->pCurRoamProfile->csrPersona);
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\tbss_type: %s", csr_get_persona(type));
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\tch_freq: %d", conn_profile->op_freq);
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\tQoS: %d", conn_profile->qosConnection);
	authtype = diag_auth_type_from_csr_type(conn_profile->AuthType);
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\tauth_type: %s",
			     csr_get_auth_type_str(authtype));
	enctype = diag_enc_type_from_csr_type(conn_profile->EncryptionType);
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\tencry_type: %s",
			     csr_get_encr_type_str(enctype));
	conn_info = policy_mgr_get_conn_info(&max_cxn);
	for (i = 0; i < max_cxn; i++)
		if ((conn_info->vdev_id == session->sessionId) &&
		    (conn_info->in_use))
			break;
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\tmac: %d", conn_info->mac);
	wma_conn_table_entry = wma_get_interface_by_vdev_id(session->sessionId);
	if (wma_conn_table_entry)
		nss = wma_conn_table_entry->nss;
	else
		nss = 0;
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\torig_nss: %dx%d neg_nss: %dx%d",
			     conn_info->original_nss, conn_info->original_nss,
			     nss, nss);
	hw_mode = policy_mgr_is_current_hwmode_dbs(mac_ctx->psoc);
	len += qdf_scnprintf(buf + len, buf_sz - len,
			     "\n\tis_current_hw_mode_dbs: %s",
			     ((hw_mode != 0) ? "yes" : "no"));
}
#else
static void csr_connect_info(struct mac_context *mac_ctx,
			     struct csr_roam_session *session,
			     struct csr_roam_info *roam_info,
			     eCsrRoamResult u2)
{}

#endif

QDF_STATUS csr_roam_call_callback(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_info *roam_info,
				  uint32_t roamId,
				  eRoamCmdStatus u1, eCsrRoamResult u2)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR

	WLAN_HOST_DIAG_EVENT_DEF(connectionStatus,
			host_event_wlan_status_payload_type);
#endif
	struct csr_roam_session *pSession;
	tDot11fBeaconIEs *beacon_ies = NULL;
	uint8_t chan1, chan2;
	uint32_t chan_freq;

	if (!CSR_IS_SESSION_VALID(mac, sessionId)) {
		sme_err("Session ID: %d is not valid", sessionId);
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}
	pSession = CSR_GET_SESSION(mac, sessionId);

	if (false == pSession->sessionActive) {
		sme_debug("Session is not Active");
		return QDF_STATUS_E_FAILURE;
	}

	if (eCSR_ROAM_ASSOCIATION_COMPLETION == u1 &&
			eCSR_ROAM_RESULT_ASSOCIATED == u2 && roam_info) {
		sme_debug("Assoc complete result: %d status: %d reason: %d",
			u2, roam_info->status_code, roam_info->reasonCode);
		beacon_ies = qdf_mem_malloc(sizeof(tDot11fBeaconIEs));
		if ((beacon_ies) && (roam_info->bss_desc)) {
			status = csr_parse_bss_description_ies(
					mac, roam_info->bss_desc,
					beacon_ies);
			csr_roam_populate_channels(beacon_ies, roam_info,
					&chan1, &chan2);
			if (0 != chan1)
				roam_info->chan_info.band_center_freq1 =
					cds_chan_to_freq(chan1);
			else
				roam_info->chan_info.band_center_freq1 = 0;
			if (0 != chan2)
				roam_info->chan_info.band_center_freq2 =
					cds_chan_to_freq(chan2);
			else
				roam_info->chan_info.band_center_freq2 = 0;
		} else {
			roam_info->chan_info.band_center_freq1 = 0;
			roam_info->chan_info.band_center_freq2 = 0;
			roam_info->chan_info.info = 0;
		}
		roam_info->chan_info.mhz = roam_info->u.pConnectedProfile->op_freq;
		chan_freq = roam_info->chan_info.mhz;
		roam_info->chan_info.reg_info_1 =
			(csr_get_cfg_max_tx_power(mac, chan_freq) << 16);
		roam_info->chan_info.reg_info_2 =
			(csr_get_cfg_max_tx_power(mac, chan_freq) << 8);
		qdf_mem_free(beacon_ies);
	} else if ((u1 == eCSR_ROAM_FT_REASSOC_FAILED)
			&& (pSession->bRefAssocStartCnt)) {
		/*
		 * Decrement bRefAssocStartCnt for FT reassoc failure.
		 * Reason: For FT reassoc failures, we first call
		 * csr_roam_call_callback before notifying a failed roam
		 * completion through csr_roam_complete. The latter in
		 * turn calls csr_roam_process_results which tries to
		 * once again call csr_roam_call_callback if bRefAssocStartCnt
		 * is non-zero. Since this is redundant for FT reassoc
		 * failure, decrement bRefAssocStartCnt.
		 */
		pSession->bRefAssocStartCnt--;
	} else if (roam_info && (u1 == eCSR_ROAM_SET_CHANNEL_RSP)
		   && (u2 == eCSR_ROAM_RESULT_CHANNEL_CHANGE_SUCCESS)) {
		pSession->connectedProfile.op_freq =
			roam_info->channelChangeRespEvent->new_op_freq;
	}

	if (eCSR_ROAM_ASSOCIATION_COMPLETION == u1)
		csr_connect_info(mac, pSession, roam_info, u2);

	if (mac->session_roam_complete_cb) {
		if (roam_info)
			roam_info->sessionId = (uint8_t) sessionId;
		status = mac->session_roam_complete_cb(mac->psoc, sessionId, roam_info,
						       roamId, u1, u2);
	}
	/*
	 * EVENT_WLAN_STATUS_V2: eCSR_ROAM_ASSOCIATION_COMPLETION,
	 *                    eCSR_ROAM_LOSTLINK,
	 *                    eCSR_ROAM_DISASSOCIATED,
	 */
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	qdf_mem_zero(&connectionStatus,
			sizeof(host_event_wlan_status_payload_type));

	if ((eCSR_ROAM_ASSOCIATION_COMPLETION == u1)
			&& (eCSR_ROAM_RESULT_ASSOCIATED == u2) && roam_info) {
		connectionStatus.eventId = eCSR_WLAN_STATUS_CONNECT;
		connectionStatus.bssType =
			roam_info->u.pConnectedProfile->BSSType;

		if (roam_info->bss_desc) {
			connectionStatus.rssi =
				roam_info->bss_desc->rssi * (-1);
			connectionStatus.channel =
				wlan_reg_freq_to_chan(
					mac->pdev,
					roam_info->bss_desc->chan_freq);
		}
		mac->mlme_cfg->sta.current_rssi = connectionStatus.rssi;

		connectionStatus.qosCapability =
			roam_info->u.pConnectedProfile->qosConnection;
		connectionStatus.authType =
			(uint8_t) diag_auth_type_from_csr_type(
				roam_info->u.pConnectedProfile->AuthType);
		connectionStatus.encryptionType =
			(uint8_t) diag_enc_type_from_csr_type(
				roam_info->u.pConnectedProfile->EncryptionType);
		qdf_mem_copy(connectionStatus.ssid,
				roam_info->u.pConnectedProfile->SSID.ssId,
				roam_info->u.pConnectedProfile->SSID.length);

		connectionStatus.reason = eCSR_REASON_UNSPECIFIED;
		qdf_mem_copy(&mac->sme.eventPayload, &connectionStatus,
				sizeof(host_event_wlan_status_payload_type));
		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
	if ((eCSR_ROAM_MIC_ERROR_IND == u1)
			|| (eCSR_ROAM_RESULT_MIC_FAILURE == u2)) {
		qdf_mem_copy(&connectionStatus, &mac->sme.eventPayload,
				sizeof(host_event_wlan_status_payload_type));
		connectionStatus.rssi = mac->mlme_cfg->sta.current_rssi;
		connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = eCSR_REASON_MIC_ERROR;
		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
	if (eCSR_ROAM_RESULT_FORCED == u2) {
		qdf_mem_copy(&connectionStatus, &mac->sme.eventPayload,
				sizeof(host_event_wlan_status_payload_type));
		connectionStatus.rssi = mac->mlme_cfg->sta.current_rssi;
		connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = eCSR_REASON_USER_REQUESTED;
		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
	if (eCSR_ROAM_RESULT_DISASSOC_IND == u2) {
		qdf_mem_copy(&connectionStatus, &mac->sme.eventPayload,
				sizeof(host_event_wlan_status_payload_type));
		connectionStatus.rssi = mac->mlme_cfg->sta.current_rssi;
		connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = eCSR_REASON_DISASSOC;
		if (roam_info)
			connectionStatus.reasonDisconnect =
				roam_info->reasonCode;

		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
	if (eCSR_ROAM_RESULT_DEAUTH_IND == u2) {
		qdf_mem_copy(&connectionStatus, &mac->sme.eventPayload,
				sizeof(host_event_wlan_status_payload_type));
		connectionStatus.rssi = mac->mlme_cfg->sta.current_rssi;
		connectionStatus.eventId = eCSR_WLAN_STATUS_DISCONNECT;
		connectionStatus.reason = eCSR_REASON_DEAUTH;
		if (roam_info)
			connectionStatus.reasonDisconnect =
				roam_info->reasonCode;
		WLAN_HOST_DIAG_EVENT_REPORT(&connectionStatus,
				EVENT_WLAN_STATUS_V2);
	}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

	return status;
}

static bool csr_peer_mac_match_cmd(tSmeCmd *sme_cmd,
				   struct qdf_mac_addr peer_macaddr,
				   uint8_t vdev_id)
{
	if (sme_cmd->command == eSmeCommandRoam &&
	    (sme_cmd->u.roamCmd.roamReason == eCsrForcedDisassocSta ||
	     sme_cmd->u.roamCmd.roamReason == eCsrForcedDeauthSta) &&
	    !qdf_mem_cmp(peer_macaddr.bytes, sme_cmd->u.roamCmd.peerMac,
			 QDF_MAC_ADDR_SIZE))
		return true;

	/*
	 * For STA/CLI mode for NB disconnect peer mac is not stored, so check
	 * vdev id as there is only one bssid/peer for STA/CLI.
	 */
	if (CSR_IS_DISCONNECT_COMMAND(sme_cmd) && sme_cmd->vdev_id == vdev_id)
		return true;

	if (sme_cmd->command == eSmeCommandWmStatusChange) {
		struct wmstatus_changecmd *wms_cmd;

		wms_cmd = &sme_cmd->u.wmStatusChangeCmd;
		if (wms_cmd->Type == eCsrDisassociated &&
		    !qdf_mem_cmp(peer_macaddr.bytes,
				 wms_cmd->u.DisassocIndMsg.peer_macaddr.bytes,
				 QDF_MAC_ADDR_SIZE))
			return true;

		if (wms_cmd->Type == eCsrDeauthenticated &&
		    !qdf_mem_cmp(peer_macaddr.bytes,
				 wms_cmd->u.DeauthIndMsg.peer_macaddr.bytes,
				 QDF_MAC_ADDR_SIZE))
			return true;
	}

	return false;
}

static bool
csr_is_deauth_disassoc_in_pending_q(struct mac_context *mac_ctx,
				    uint8_t vdev_id,
				    struct qdf_mac_addr peer_macaddr)
{
	tListElem *entry = NULL;
	tSmeCmd *sme_cmd;

	entry = csr_nonscan_pending_ll_peek_head(mac_ctx, LL_ACCESS_NOLOCK);
	while (entry) {
		sme_cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		if ((sme_cmd->vdev_id == vdev_id) &&
		    csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
			return true;
		entry = csr_nonscan_pending_ll_next(mac_ctx, entry,
						    LL_ACCESS_NOLOCK);
	}

	return false;
}

static bool
csr_is_deauth_disassoc_in_active_q(struct mac_context *mac_ctx,
				   uint8_t vdev_id,
				   struct qdf_mac_addr peer_macaddr)
{
	tSmeCmd *sme_cmd;

	sme_cmd = wlan_serialization_get_active_cmd(mac_ctx->psoc, vdev_id,
						WLAN_SER_CMD_FORCE_DEAUTH_STA);

	if (sme_cmd && csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
		return true;

	sme_cmd = wlan_serialization_get_active_cmd(mac_ctx->psoc, vdev_id,
					WLAN_SER_CMD_FORCE_DISASSOC_STA);

	if (sme_cmd && csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
		return true;

	/*
	 * WLAN_SER_CMD_WM_STATUS_CHANGE is of two type, the handling
	 * should take care of both the types.
	 */
	sme_cmd = wlan_serialization_get_active_cmd(mac_ctx->psoc, vdev_id,
						WLAN_SER_CMD_WM_STATUS_CHANGE);
	if (sme_cmd && csr_peer_mac_match_cmd(sme_cmd, peer_macaddr, vdev_id))
		return true;

	return false;
}

/*
 * csr_is_deauth_disassoc_already_active() - Function to check if deauth or
 *  disassoc is already in progress.
 * @mac_ctx: Global MAC context
 * @vdev_id: vdev id
 * @peer_macaddr: Peer MAC address
 *
 * Return: True if deauth/disassoc indication can be dropped
 *  else false
 */
static bool
csr_is_deauth_disassoc_already_active(struct mac_context *mac_ctx,
				      uint8_t vdev_id,
				      struct qdf_mac_addr peer_macaddr)
{
	bool ret = csr_is_deauth_disassoc_in_pending_q(mac_ctx,
						      vdev_id,
						      peer_macaddr);
	if (!ret)
		/**
		 * commands are not found in pending queue, check in active
		 * queue as well
		 */
		ret = csr_is_deauth_disassoc_in_active_q(mac_ctx,
							  vdev_id,
							  peer_macaddr);

	if (ret)
		sme_debug("Deauth/Disassoc already in progress for "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(peer_macaddr.bytes));

	return ret;
}

static void csr_roam_issue_disconnect_stats(struct mac_context *mac,
					    uint32_t session_id,
					    struct qdf_mac_addr peer_mac)
{
	tSmeCmd *cmd;

	cmd = csr_get_command_buffer(mac);
	if (!cmd) {
		sme_err(" fail to get command buffer");
		return;
	}

	cmd->command = eSmeCommandGetdisconnectStats;
	cmd->vdev_id = session_id;
	qdf_mem_copy(&cmd->u.disconnect_stats_cmd.peer_mac_addr, &peer_mac,
		     QDF_MAC_ADDR_SIZE);
	if (QDF_IS_STATUS_ERROR(csr_queue_sme_command(mac, cmd, true)))
		sme_err("fail to queue get disconnect stats");
}

/**
 * csr_roam_issue_disassociate_sta_cmd() - disassociate a associated station
 * @sessionId:     Session Id for Soft AP
 * @p_del_sta_params: Pointer to parameters of the station to disassoc
 *
 * CSR function that HDD calls to delete a associated station
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_* on error
 */
QDF_STATUS csr_roam_issue_disassociate_sta_cmd(struct mac_context *mac,
					       uint32_t sessionId,
					       struct csr_del_sta_params
					       *p_del_sta_params)

{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	do {
		if (csr_is_deauth_disassoc_already_active(mac, sessionId,
					      p_del_sta_params->peerMacAddr))
			break;
		pCommand = csr_get_command_buffer(mac);
		if (!pCommand) {
			sme_err("fail to get command buffer");
			status = QDF_STATUS_E_RESOURCES;
			break;
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamReason = eCsrForcedDisassocSta;
		qdf_mem_copy(pCommand->u.roamCmd.peerMac,
				p_del_sta_params->peerMacAddr.bytes,
				sizeof(pCommand->u.roamCmd.peerMac));
		pCommand->u.roamCmd.reason = p_del_sta_params->reason_code;

		csr_roam_issue_disconnect_stats(
					mac, sessionId,
					p_del_sta_params->peerMacAddr);

		status = csr_queue_sme_command(mac, pCommand, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} while (0);

	return status;
}

/**
 * csr_roam_issue_deauthSta() - delete a associated station
 * @sessionId:     Session Id for Soft AP
 * @pDelStaParams: Pointer to parameters of the station to deauthenticate
 *
 * CSR function that HDD calls to delete a associated station
 *
 * Return: QDF_STATUS_SUCCESS on success or another QDF_STATUS_** on error
 */
QDF_STATUS csr_roam_issue_deauth_sta_cmd(struct mac_context *mac,
		uint32_t sessionId,
		struct csr_del_sta_params *pDelStaParams)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	do {
		if (csr_is_deauth_disassoc_already_active(mac, sessionId,
					      pDelStaParams->peerMacAddr))
			break;
		pCommand = csr_get_command_buffer(mac);
		if (!pCommand) {
			sme_err("fail to get command buffer");
			status = QDF_STATUS_E_RESOURCES;
			break;
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamReason = eCsrForcedDeauthSta;
		qdf_mem_copy(pCommand->u.roamCmd.peerMac,
			     pDelStaParams->peerMacAddr.bytes,
			     sizeof(tSirMacAddr));
		pCommand->u.roamCmd.reason = pDelStaParams->reason_code;

		csr_roam_issue_disconnect_stats(mac, sessionId,
						pDelStaParams->peerMacAddr);

		status = csr_queue_sme_command(mac, pCommand, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} while (0);

	return status;
}

static
QDF_STATUS csr_roam_issue_deauth(struct mac_context *mac, uint32_t sessionId,
				 enum csr_roam_substate NewSubstate)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct qdf_mac_addr bssId = QDF_MAC_ADDR_BCAST_INIT;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (pSession->pConnectBssDesc) {
		qdf_mem_copy(bssId.bytes, pSession->pConnectBssDesc->bssId,
			     sizeof(struct qdf_mac_addr));
	}
	sme_debug("Deauth to Bssid " QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(bssId.bytes));
	csr_roam_substate_change(mac, NewSubstate, sessionId);

	status =
		csr_send_mb_deauth_req_msg(mac, sessionId, bssId.bytes,
					   REASON_DEAUTH_NETWORK_LEAVING);
	if (QDF_IS_STATUS_SUCCESS(status))
		csr_roam_link_down(mac, sessionId);
	else {
		sme_err("csr_send_mb_deauth_req_msg failed with status %d Session ID: %d"
			QDF_MAC_ADDR_FMT, status, sessionId,
			QDF_MAC_ADDR_REF(bssId.bytes));
	}

	return status;
}

QDF_STATUS csr_roam_save_connected_bss_desc(struct mac_context *mac,
					    uint32_t sessionId,
					    struct bss_description *bss_desc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	uint32_t size;

	if (!pSession) {
		sme_err("  session %d not found ", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* If no BSS description was found in this connection
	 * , then nix the BSS description
	 * that we keep around for the connected BSS) and get out.
	 */
	if (!bss_desc) {
		csr_free_connect_bss_desc(mac, sessionId);
	} else {
		size = bss_desc->length + sizeof(bss_desc->length);
		if (pSession->pConnectBssDesc) {
			if (((pSession->pConnectBssDesc->length) +
			     sizeof(pSession->pConnectBssDesc->length)) <
			    size) {
				/* not enough room for the new BSS,
				 * mac->roam.pConnectBssDesc is freed inside
				 */
				csr_free_connect_bss_desc(mac, sessionId);
			}
		}
		if (!pSession->pConnectBssDesc)
			pSession->pConnectBssDesc = qdf_mem_malloc(size);

		if (!pSession->pConnectBssDesc)
			status = QDF_STATUS_E_NOMEM;
		else
			qdf_mem_copy(pSession->pConnectBssDesc, bss_desc, size);
	}
	return status;
}

static
QDF_STATUS csr_roam_prepare_bss_config(struct mac_context *mac,
				       struct csr_roam_profile *pProfile,
				       struct bss_description *bss_desc,
				       struct bss_config_param *pBssConfig,
				       tDot11fBeaconIEs *pIes)
{
	enum csr_cfgdot11mode cfgDot11Mode;
	uint32_t join_timeout;

	QDF_ASSERT(pIes);
	if (!pIes)
		return QDF_STATUS_E_FAILURE;
	if (!pProfile) {
		sme_debug("Profile is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (!bss_desc) {
		sme_debug("bss_desc is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(&pBssConfig->BssCap, &bss_desc->capabilityInfo,
		     sizeof(tSirMacCapabilityInfo));
	/* get qos */
	pBssConfig->qosType = csr_get_qos_from_bss_desc(mac, bss_desc, pIes);
	/* Take SSID always from profile */
	qdf_mem_copy(&pBssConfig->SSID.ssId,
		     pProfile->SSIDs.SSIDList->SSID.ssId,
		     pProfile->SSIDs.SSIDList->SSID.length);
	pBssConfig->SSID.length = pProfile->SSIDs.SSIDList->SSID.length;

	if (csr_is_nullssid(pBssConfig->SSID.ssId, pBssConfig->SSID.length)) {
		sme_warn("BSS desc SSID is a wild card");
		/* Return failed if profile doesn't have an SSID either. */
		if (pProfile->SSIDs.numOfSSIDs == 0) {
			sme_warn("BSS desc and profile doesn't have SSID");
			return QDF_STATUS_E_FAILURE;
		}
	}

	if (WLAN_REG_IS_5GHZ_CH_FREQ(bss_desc->chan_freq))
		pBssConfig->band = REG_BAND_5G;
	else if (WLAN_REG_IS_24GHZ_CH_FREQ(bss_desc->chan_freq))
		pBssConfig->band = REG_BAND_2G;
	else if (WLAN_REG_IS_6GHZ_CHAN_FREQ(bss_desc->chan_freq))
		pBssConfig->band = REG_BAND_6G;
	else
		return QDF_STATUS_E_FAILURE;

		/* phymode */
	if (csr_is_phy_mode_match(mac, pProfile->phyMode, bss_desc,
				  pProfile, &cfgDot11Mode, pIes)) {
		pBssConfig->uCfgDot11Mode = cfgDot11Mode;
	} else {
		/*
		 * No matching phy mode found, force to 11b/g based on INI for
		 * 2.4Ghz and to 11a mode for 5Ghz
		 */
		sme_warn("Can not find match phy mode");
		if (REG_BAND_2G == pBssConfig->band) {
			if (mac->roam.configParam.phyMode &
			    (eCSR_DOT11_MODE_11b | eCSR_DOT11_MODE_11b_ONLY)) {
				pBssConfig->uCfgDot11Mode =
						eCSR_CFG_DOT11_MODE_11B;
			} else {
				pBssConfig->uCfgDot11Mode =
						eCSR_CFG_DOT11_MODE_11G;
			}
		} else if (pBssConfig->band == REG_BAND_5G) {
			pBssConfig->uCfgDot11Mode = eCSR_CFG_DOT11_MODE_11A;
		} else if (pBssConfig->band == REG_BAND_6G) {
			pBssConfig->uCfgDot11Mode =
						eCSR_CFG_DOT11_MODE_11AX_ONLY;
		}
	}

	sme_debug("phyMode=%d, uCfgDot11Mode=%d negotiatedAuthType %d",
		   pProfile->phyMode, pBssConfig->uCfgDot11Mode,
		   pProfile->negotiatedAuthType);

	/* Qos */
	if ((pBssConfig->uCfgDot11Mode != eCSR_CFG_DOT11_MODE_11N) &&
	    (mac->roam.configParam.WMMSupportMode == eCsrRoamWmmNoQos)) {
		/*
		 * Joining BSS is not 11n capable and WMM is disabled on client.
		 * Disable QoS and WMM
		 */
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_DCF;
	}

	if (((pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11N)
	    || (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AC))
		&& ((pBssConfig->qosType != eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP)
		    || (pBssConfig->qosType != eCSR_MEDIUM_ACCESS_11e_HCF)
		    || (pBssConfig->qosType != eCSR_MEDIUM_ACCESS_11e_eDCF))) {
		/* Joining BSS is 11n capable and WMM is disabled on AP. */
		/* Assume all HT AP's are QOS AP's and enable WMM */
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP;
	}
	/* auth type */
	switch (pProfile->negotiatedAuthType) {
	default:
	case eCSR_AUTH_TYPE_WPA:
	case eCSR_AUTH_TYPE_WPA_PSK:
	case eCSR_AUTH_TYPE_WPA_NONE:
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		pBssConfig->authType = eSIR_OPEN_SYSTEM;
		break;
	case eCSR_AUTH_TYPE_SHARED_KEY:
		pBssConfig->authType = eSIR_SHARED_KEY;
		break;
	case eCSR_AUTH_TYPE_AUTOSWITCH:
		pBssConfig->authType = eSIR_AUTO_SWITCH;
		break;
	case eCSR_AUTH_TYPE_SAE:
	case eCSR_AUTH_TYPE_FT_SAE:
		pBssConfig->authType = eSIR_AUTH_TYPE_SAE;
		break;
	}
	/* short slot time */
	if (eCSR_CFG_DOT11_MODE_11B != cfgDot11Mode)
		pBssConfig->uShortSlotTime =
			mac->mlme_cfg->ht_caps.short_slot_time_enabled;
	else
		pBssConfig->uShortSlotTime = 0;

	pBssConfig->f11hSupport =
			mac->mlme_cfg->gen.enabled_11h;
	/* power constraint */
	pBssConfig->uPowerLimit =
		csr_get11h_power_constraint(mac, &pIes->PowerConstraints);
	/* heartbeat */
	if (CSR_IS_11A_BSS(bss_desc))
		pBssConfig->uHeartBeatThresh =
			mac->roam.configParam.HeartbeatThresh50;
	else
		pBssConfig->uHeartBeatThresh =
			mac->roam.configParam.HeartbeatThresh24;

	/*
	 * Join timeout: if we find a BeaconInterval in the BssDescription,
	 * then set the Join Timeout to be 10 x the BeaconInterval.
	 */
	pBssConfig->uJoinTimeOut = cfg_default(CFG_JOIN_FAILURE_TIMEOUT);
	if (bss_desc->beaconInterval) {
		/* Make sure it is bigger than the minimal */
		join_timeout = QDF_MAX(10 * bss_desc->beaconInterval,
				       cfg_min(CFG_JOIN_FAILURE_TIMEOUT));
		if (join_timeout < pBssConfig->uJoinTimeOut)
			pBssConfig->uJoinTimeOut = join_timeout;
	}

	/* validate CB */
	if ((pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11N) ||
	    (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11N_ONLY) ||
	    (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AC) ||
	    (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AC_ONLY) ||
	    (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AX) ||
	    (pBssConfig->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11AX_ONLY))
		pBssConfig->cbMode = csr_get_cb_mode_from_ies(
					mac, bss_desc->chan_freq, pIes);
	else
		pBssConfig->cbMode = PHY_SINGLE_CHANNEL_CENTERED;

	if (WLAN_REG_IS_24GHZ_CH_FREQ(bss_desc->chan_freq) &&
	    pProfile->force_24ghz_in_ht20) {
		pBssConfig->cbMode = PHY_SINGLE_CHANNEL_CENTERED;
		sme_debug("force_24ghz_in_ht20 is set so set cbmode to 0");
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_roam_prepare_bss_config_from_profile(
	struct mac_context *mac, struct csr_roam_profile *pProfile,
					struct bss_config_param *pBssConfig,
					struct bss_description *bss_desc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t bss_op_ch_freq = 0;
	uint8_t qAPisEnabled = false;
	/* SSID */
	pBssConfig->SSID.length = 0;
	if (pProfile->SSIDs.numOfSSIDs) {
		/* only use the first one */
		qdf_mem_copy(&pBssConfig->SSID,
			     &pProfile->SSIDs.SSIDList[0].SSID,
			     sizeof(tSirMacSSid));
	} else {
		/* SSID must present */
		return QDF_STATUS_E_FAILURE;
	}
	/* Settomg up the capabilities */
	pBssConfig->BssCap.ess = 1;

	if (eCSR_ENCRYPT_TYPE_NONE !=
	    pProfile->EncryptionType.encryptionType[0])
		pBssConfig->BssCap.privacy = 1;

	/* Update when 6G support is added for NDI */
	pBssConfig->band = (mac->mlme_cfg->gen.band == BAND_2G ?
			    REG_BAND_2G : REG_BAND_5G);
	/* phymode */
	if (pProfile->ChannelInfo.freq_list)
		bss_op_ch_freq = pProfile->ChannelInfo.freq_list[0];
	pBssConfig->uCfgDot11Mode = csr_roam_get_phy_mode_band_for_bss(
						mac, pProfile, bss_op_ch_freq,
						&pBssConfig->band);
	/* QOS */
	/* Is this correct to always set to this // *** */
	if (pBssConfig->BssCap.ess == 1) {
		/*For Softap case enable WMM */
		if (CSR_IS_INFRA_AP(pProfile)
		    && (eCsrRoamWmmNoQos !=
			mac->roam.configParam.WMMSupportMode)) {
			qAPisEnabled = true;
		} else
		if (csr_roam_get_qos_info_from_bss(mac, bss_desc) ==
		    QDF_STATUS_SUCCESS) {
			qAPisEnabled = true;
		} else {
			qAPisEnabled = false;
		}
	} else {
		qAPisEnabled = true;
	}
	if ((eCsrRoamWmmNoQos != mac->roam.configParam.WMMSupportMode &&
	     qAPisEnabled) ||
	    ((eCSR_CFG_DOT11_MODE_11N == pBssConfig->uCfgDot11Mode &&
	      qAPisEnabled))) {
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP;
	} else {
		pBssConfig->qosType = eCSR_MEDIUM_ACCESS_DCF;
	}

	/* auth type */
	/* Take the preferred Auth type. */
	switch (pProfile->AuthType.authType[0]) {
	default:
	case eCSR_AUTH_TYPE_WPA:
	case eCSR_AUTH_TYPE_WPA_PSK:
	case eCSR_AUTH_TYPE_WPA_NONE:
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		pBssConfig->authType = eSIR_OPEN_SYSTEM;
		break;
	case eCSR_AUTH_TYPE_SHARED_KEY:
		pBssConfig->authType = eSIR_SHARED_KEY;
		break;
	case eCSR_AUTH_TYPE_AUTOSWITCH:
		pBssConfig->authType = eSIR_AUTO_SWITCH;
		break;
	case eCSR_AUTH_TYPE_SAE:
	case eCSR_AUTH_TYPE_FT_SAE:
		pBssConfig->authType = eSIR_AUTH_TYPE_SAE;
		break;
	}
	/* short slot time */
	if (WNI_CFG_PHY_MODE_11B != pBssConfig->uCfgDot11Mode) {
		pBssConfig->uShortSlotTime =
			mac->mlme_cfg->ht_caps.short_slot_time_enabled;
	} else {
		pBssConfig->uShortSlotTime = 0;
	}
	pBssConfig->f11hSupport = false;
	pBssConfig->uPowerLimit = 0;
	/* heartbeat */
	if (REG_BAND_5G == pBssConfig->band ||
	    REG_BAND_6G == pBssConfig->band) {
		pBssConfig->uHeartBeatThresh =
			mac->roam.configParam.HeartbeatThresh50;
	} else {
		pBssConfig->uHeartBeatThresh =
			mac->roam.configParam.HeartbeatThresh24;
	}
	/* Join timeout */
	pBssConfig->uJoinTimeOut = cfg_default(CFG_JOIN_FAILURE_TIMEOUT);

	return status;
}

static QDF_STATUS
csr_roam_get_qos_info_from_bss(struct mac_context *mac,
			       struct bss_description *bss_desc)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tDot11fBeaconIEs *pIes = NULL;

	do {
		if (!QDF_IS_STATUS_SUCCESS(
			csr_get_parsed_bss_description_ies(
				mac, bss_desc, &pIes))) {
			sme_err("csr_get_parsed_bss_description_ies() failed");
			break;
		}
		/* check if the AP is QAP & it supports APSD */
		if (CSR_IS_QOS_BSS(pIes))
			status = QDF_STATUS_SUCCESS;
	} while (0);

	if (pIes)
		qdf_mem_free(pIes);

	return status;
}

static void csr_reset_cfg_privacy(struct mac_context *mac)
{
	uint8_t Key0[WLAN_CRYPTO_KEY_WEP104_LEN] = {0};
	uint8_t Key1[WLAN_CRYPTO_KEY_WEP104_LEN] = {0};
	uint8_t Key2[WLAN_CRYPTO_KEY_WEP104_LEN] = {0};
	uint8_t Key3[WLAN_CRYPTO_KEY_WEP104_LEN] = {0};
	struct wlan_mlme_wep_cfg *wep_params = &mac->mlme_cfg->wep_params;

	mlme_set_wep_key(wep_params, MLME_WEP_DEFAULT_KEY_1, Key0,
			 WLAN_CRYPTO_KEY_WEP104_LEN);
	mlme_set_wep_key(wep_params, MLME_WEP_DEFAULT_KEY_2, Key1,
			 WLAN_CRYPTO_KEY_WEP104_LEN);
	mlme_set_wep_key(wep_params, MLME_WEP_DEFAULT_KEY_3, Key2,
			 WLAN_CRYPTO_KEY_WEP104_LEN);
	mlme_set_wep_key(wep_params, MLME_WEP_DEFAULT_KEY_4, Key3,
			 WLAN_CRYPTO_KEY_WEP104_LEN);
}

void csr_set_cfg_privacy(struct mac_context *mac, struct csr_roam_profile *pProfile,
			 bool fPrivacy)
{
	/*
	 * the only difference between this function and
	 * the csr_set_cfg_privacyFromProfile() is the setting of the privacy
	 * CFG based on the advertised privacy setting from the AP for WPA
	 * associations. See note below in this function...
	 */
	uint32_t privacy_enabled = 0, rsn_enabled = 0, wep_default_key_id = 0;
	uint32_t WepKeyLength = WLAN_CRYPTO_KEY_WEP40_LEN;
	uint32_t Key0Length = 0, Key1Length = 0, Key2Length = 0, Key3Length = 0;

	/* Reserve for the biggest key */
	uint8_t Key0[WLAN_CRYPTO_KEY_WEP104_LEN];
	uint8_t Key1[WLAN_CRYPTO_KEY_WEP104_LEN];
	uint8_t Key2[WLAN_CRYPTO_KEY_WEP104_LEN];
	uint8_t Key3[WLAN_CRYPTO_KEY_WEP104_LEN];

	struct wlan_mlme_wep_cfg *wep_params = &mac->mlme_cfg->wep_params;

	switch (pProfile->negotiatedUCEncryptionType) {
	case eCSR_ENCRYPT_TYPE_NONE:
		/* for NO encryption, turn off Privacy and Rsn. */
		privacy_enabled = 0;
		rsn_enabled = 0;
		/* clear out the WEP keys that may be hanging around. */
		Key0Length = 0;
		Key1Length = 0;
		Key2Length = 0;
		Key3Length = 0;
		break;

	case eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP40:

		/* Privacy is ON.  NO RSN for Wep40 static key. */
		privacy_enabled = 1;
		rsn_enabled = 0;
		/* Set the Wep default key ID. */
		wep_default_key_id = pProfile->Keys.defaultIndex;
		/* Wep key size if 5 bytes (40 bits). */
		WepKeyLength = WLAN_CRYPTO_KEY_WEP40_LEN;
		/*
		 * set encryption keys in the CFG database or
		 * clear those that are not present in this profile.
		 */
		if (pProfile->Keys.KeyLength[0]) {
			qdf_mem_copy(Key0,
				pProfile->Keys.KeyMaterial[0],
				WLAN_CRYPTO_KEY_WEP40_LEN);
			Key0Length = WLAN_CRYPTO_KEY_WEP40_LEN;
		} else {
			Key0Length = 0;
		}

		if (pProfile->Keys.KeyLength[1]) {
			qdf_mem_copy(Key1,
				pProfile->Keys.KeyMaterial[1],
				WLAN_CRYPTO_KEY_WEP40_LEN);
			Key1Length = WLAN_CRYPTO_KEY_WEP40_LEN;
		} else {
			Key1Length = 0;
		}

		if (pProfile->Keys.KeyLength[2]) {
			qdf_mem_copy(Key2,
				pProfile->Keys.KeyMaterial[2],
				WLAN_CRYPTO_KEY_WEP40_LEN);
			Key2Length = WLAN_CRYPTO_KEY_WEP40_LEN;
		} else {
			Key2Length = 0;
		}

		if (pProfile->Keys.KeyLength[3]) {
			qdf_mem_copy(Key3,
				pProfile->Keys.KeyMaterial[3],
				WLAN_CRYPTO_KEY_WEP40_LEN);
			Key3Length = WLAN_CRYPTO_KEY_WEP40_LEN;
		} else {
			Key3Length = 0;
		}
		break;

	case eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
	case eCSR_ENCRYPT_TYPE_WEP104:

		/* Privacy is ON.  NO RSN for Wep40 static key. */
		privacy_enabled = 1;
		rsn_enabled = 0;
		/* Set the Wep default key ID. */
		wep_default_key_id = pProfile->Keys.defaultIndex;
		/* Wep key size if 13 bytes (104 bits). */
		WepKeyLength = WLAN_CRYPTO_KEY_WEP104_LEN;
		/*
		 * set encryption keys in the CFG database or clear
		 * those that are not present in this profile.
		 */
		if (pProfile->Keys.KeyLength[0]) {
			qdf_mem_copy(Key0,
				pProfile->Keys.KeyMaterial[0],
				WLAN_CRYPTO_KEY_WEP104_LEN);
			Key0Length = WLAN_CRYPTO_KEY_WEP104_LEN;
		} else {
			Key0Length = 0;
		}

		if (pProfile->Keys.KeyLength[1]) {
			qdf_mem_copy(Key1,
				pProfile->Keys.KeyMaterial[1],
				WLAN_CRYPTO_KEY_WEP104_LEN);
			Key1Length = WLAN_CRYPTO_KEY_WEP104_LEN;
		} else {
			Key1Length = 0;
		}

		if (pProfile->Keys.KeyLength[2]) {
			qdf_mem_copy(Key2,
				pProfile->Keys.KeyMaterial[2],
				WLAN_CRYPTO_KEY_WEP104_LEN);
			Key2Length = WLAN_CRYPTO_KEY_WEP104_LEN;
		} else {
			Key2Length = 0;
		}

		if (pProfile->Keys.KeyLength[3]) {
			qdf_mem_copy(Key3,
				pProfile->Keys.KeyMaterial[3],
				WLAN_CRYPTO_KEY_WEP104_LEN);
			Key3Length = WLAN_CRYPTO_KEY_WEP104_LEN;
		} else {
			Key3Length = 0;
		}
		break;

	case eCSR_ENCRYPT_TYPE_TKIP:
	case eCSR_ENCRYPT_TYPE_AES:
	case eCSR_ENCRYPT_TYPE_AES_GCMP:
	case eCSR_ENCRYPT_TYPE_AES_GCMP_256:
#ifdef FEATURE_WLAN_WAPI
	case eCSR_ENCRYPT_TYPE_WPI:
#endif /* FEATURE_WLAN_WAPI */
		/*
		 * this is the only difference between this function and
		 * the csr_set_cfg_privacyFromProfile().
		 * (setting of the privacy CFG based on the advertised
		 *  privacy setting from AP for WPA/WAPI associations).
		 */
		privacy_enabled = (0 != fPrivacy);
		/* turn on RSN enabled for WPA associations */
		rsn_enabled = 1;
		/* clear static WEP keys that may be hanging around. */
		Key0Length = 0;
		Key1Length = 0;
		Key2Length = 0;
		Key3Length = 0;
		break;
	default:
		privacy_enabled = 0;
		rsn_enabled = 0;
		break;
	}

	mac->mlme_cfg->wep_params.is_privacy_enabled = privacy_enabled;
	mac->mlme_cfg->feature_flags.enable_rsn = rsn_enabled;
	mlme_set_wep_key(wep_params, MLME_WEP_DEFAULT_KEY_1, Key0, Key0Length);
	mlme_set_wep_key(wep_params, MLME_WEP_DEFAULT_KEY_2, Key1, Key1Length);
	mlme_set_wep_key(wep_params, MLME_WEP_DEFAULT_KEY_3, Key2, Key2Length);
	mlme_set_wep_key(wep_params, MLME_WEP_DEFAULT_KEY_4, Key3, Key3Length);
	mac->mlme_cfg->wep_params.wep_default_key_id = wep_default_key_id;
}

static void csr_set_cfg_ssid(struct mac_context *mac, tSirMacSSid *pSSID)
{
	uint32_t len = 0;

	if (pSSID->length <= WLAN_SSID_MAX_LEN)
		len = pSSID->length;
	else
		len = WLAN_SSID_MAX_LEN;

	qdf_mem_copy(mac->mlme_cfg->sap_cfg.cfg_ssid,
		     (uint8_t *)pSSID->ssId, len);
	mac->mlme_cfg->sap_cfg.cfg_ssid_len = len;

}

static QDF_STATUS csr_set_qos_to_cfg(struct mac_context *mac, uint32_t sessionId,
				     eCsrMediaAccessType qosType)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t QoSEnabled;
	uint32_t WmeEnabled;
	/* set the CFG enable/disable variables based on the
	 * qosType being configured.
	 */
	switch (qosType) {
	case eCSR_MEDIUM_ACCESS_WMM_eDCF_802dot1p:
		QoSEnabled = false;
		WmeEnabled = true;
		break;
	case eCSR_MEDIUM_ACCESS_WMM_eDCF_DSCP:
		QoSEnabled = false;
		WmeEnabled = true;
		break;
	case eCSR_MEDIUM_ACCESS_WMM_eDCF_NoClassify:
		QoSEnabled = false;
		WmeEnabled = true;
		break;
	case eCSR_MEDIUM_ACCESS_11e_eDCF:
		QoSEnabled = true;
		WmeEnabled = false;
		break;
	case eCSR_MEDIUM_ACCESS_11e_HCF:
		QoSEnabled = true;
		WmeEnabled = false;
		break;
	default:
	case eCSR_MEDIUM_ACCESS_DCF:
		QoSEnabled = false;
		WmeEnabled = false;
		break;
	}
	/* save the WMM setting for later use */
	mac->roam.roamSession[sessionId].fWMMConnection = (bool) WmeEnabled;
	mac->roam.roamSession[sessionId].fQOSConnection = (bool) QoSEnabled;
	return status;
}

static bool is_ofdm_rates(uint16_t rate)
{
	uint16_t n = BITS_OFF(rate, CSR_DOT11_BASIC_RATE_MASK);

	switch (n) {
	case SIR_MAC_RATE_6:
	case SIR_MAC_RATE_9:
	case SIR_MAC_RATE_12:
	case SIR_MAC_RATE_18:
	case SIR_MAC_RATE_24:
	case SIR_MAC_RATE_36:
	case SIR_MAC_RATE_48:
	case SIR_MAC_RATE_54:
		return true;
	default:
		break;
	}

	return false;
}

static QDF_STATUS csr_get_rate_set(struct mac_context *mac,
				   struct csr_roam_profile *pProfile,
				   eCsrPhyMode phyMode,
				   struct bss_description *bss_desc,
				   tDot11fBeaconIEs *pIes,
				   tSirMacRateSet *pOpRateSet,
				   tSirMacRateSet *pExRateSet)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	int i;
	enum csr_cfgdot11mode cfgDot11Mode;
	uint8_t *pDstRate;
	uint16_t rateBitmap = 0;
	bool is_5ghz_freq;

	qdf_mem_zero(pOpRateSet, sizeof(tSirMacRateSet));
	qdf_mem_zero(pExRateSet, sizeof(tSirMacRateSet));
	QDF_ASSERT(pIes);

	if (!pIes) {
		sme_err("failed to parse BssDesc");
		return status;
	}

	csr_is_phy_mode_match(mac, phyMode, bss_desc, pProfile,
			      &cfgDot11Mode, pIes);
	is_5ghz_freq = wlan_reg_is_5ghz_ch_freq(bss_desc->chan_freq);
	/*
	 * Originally, we thought that for 11a networks, the 11a rates
	 * are always in the Operational Rate set & for 11b and 11g
	 * networks, the 11b rates appear in the Operational Rate set.
	 * Consequently, in either case, we would blindly put the rates
	 * we support into our Operational Rate set.
	 * (including the basic rates, which we've already verified are
	 * supported earlier in the roaming decision).
	 * However, it turns out that this is not always the case.
	 * Some AP's (e.g. D-Link DI-784) ram 11g rates into the
	 * Operational Rate set too.  Now, we're a little more careful.
	 */
	pDstRate = pOpRateSet->rate;
	if (pIes->SuppRates.present) {
		for (i = 0; i < pIes->SuppRates.num_rates; i++) {
			if (is_5ghz_freq &&
			    !is_ofdm_rates(pIes->SuppRates.rates[i]))
				continue;

			if (csr_rates_is_dot11_rate_supported(mac,
				pIes->SuppRates.rates[i]) &&
				!csr_check_rate_bitmap(
					pIes->SuppRates.rates[i],
					rateBitmap)) {
				csr_add_rate_bitmap(pIes->SuppRates.
						    rates[i], &rateBitmap);
				*pDstRate++ = pIes->SuppRates.rates[i];
				pOpRateSet->numRates++;
			}
		}
	}
	/*
	 * If there are Extended Rates in the beacon, we will reflect the
	 * extended rates that we support in our Extended Operational Rate
	 * set.
	 */
	if (pIes->ExtSuppRates.present) {
		pDstRate = pExRateSet->rate;
		for (i = 0; i < pIes->ExtSuppRates.num_rates; i++) {
			if (csr_rates_is_dot11_rate_supported(mac,
				pIes->ExtSuppRates.rates[i]) &&
				!csr_check_rate_bitmap(
					pIes->ExtSuppRates.rates[i],
					rateBitmap)) {
				*pDstRate++ = pIes->ExtSuppRates.rates[i];
				pExRateSet->numRates++;
			}
		}
	}
	if (pOpRateSet->numRates > 0 || pExRateSet->numRates > 0)
		status = QDF_STATUS_SUCCESS;
	return status;
}

static void csr_set_cfg_rate_set(struct mac_context *mac, eCsrPhyMode phyMode,
				 struct csr_roam_profile *pProfile,
				 struct bss_description *bss_desc,
				 tDot11fBeaconIEs *pIes,
				 uint32_t session_id)
{
	int i;
	uint8_t *pDstRate;
	enum csr_cfgdot11mode cfgDot11Mode;
	/* leave enough room for the max number of rates */
	uint8_t OperationalRates[CSR_DOT11_SUPPORTED_RATES_MAX];
	qdf_size_t OperationalRatesLength = 0;
	/* leave enough room for the max number of rates */
	uint8_t ExtendedOperationalRates
				[CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX];
	qdf_size_t ExtendedOperationalRatesLength = 0;
	uint8_t MCSRateIdxSet[SIZE_OF_SUPPORTED_MCS_SET];
	qdf_size_t MCSRateLength = 0;
	struct wlan_objmgr_vdev *vdev;

	QDF_ASSERT(pIes);
	if (pIes) {
		csr_is_phy_mode_match(mac, phyMode, bss_desc, pProfile,
				      &cfgDot11Mode, pIes);
		/* Originally, we thought that for 11a networks, the 11a rates
		 * are always in the Operational Rate set & for 11b and 11g
		 * networks, the 11b rates appear in the Operational Rate set.
		 * Consequently, in either case, we would blindly put the rates
		 * we support into our Operational Rate set (including the basic
		 * rates, which we have already verified are supported earlier
		 * in the roaming decision). However, it turns out that this is
		 * not always the case.  Some AP's (e.g. D-Link DI-784) ram 11g
		 * rates into the Operational Rate set, too.  Now, we're a
		 * little more careful:
		 */
		pDstRate = OperationalRates;
		if (pIes->SuppRates.present) {
			for (i = 0; i < pIes->SuppRates.num_rates; i++) {
				if (csr_rates_is_dot11_rate_supported
					    (mac, pIes->SuppRates.rates[i])
				    && (OperationalRatesLength <
					CSR_DOT11_SUPPORTED_RATES_MAX)) {
					*pDstRate++ = pIes->SuppRates.rates[i];
					OperationalRatesLength++;
				}
			}
		}
		if (eCSR_CFG_DOT11_MODE_11G == cfgDot11Mode ||
		    eCSR_CFG_DOT11_MODE_11N == cfgDot11Mode ||
		    eCSR_CFG_DOT11_MODE_ABG == cfgDot11Mode) {
			/* If there are Extended Rates in the beacon, we will
			 * reflect those extended rates that we support in out
			 * Extended Operational Rate set:
			 */
			pDstRate = ExtendedOperationalRates;
			if (pIes->ExtSuppRates.present) {
				for (i = 0; i < pIes->ExtSuppRates.num_rates;
				     i++) {
					if (csr_rates_is_dot11_rate_supported
						    (mac, pIes->ExtSuppRates.
							rates[i])
					    && (ExtendedOperationalRatesLength <
						CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX)) {
						*pDstRate++ =
							pIes->ExtSuppRates.
							rates[i];
						ExtendedOperationalRatesLength++;
					}
				}
			}
		}
		/* Enable proprietary MAC features if peer node is Airgo node
		 * and STA user wants to use them For ANI network companions,
		 * we need to populate the proprietary rate set with any
		 * proprietary rates we found in the beacon, only if user allows
		 * them.
		 */
		/* No proprietary modes... */
		/* Get MCS Rate */
		pDstRate = MCSRateIdxSet;
		if (pIes->HTCaps.present) {
			for (i = 0; i < VALID_MAX_MCS_INDEX; i++) {
				if ((unsigned int)pIes->HTCaps.
				    supportedMCSSet[0] & (1 << i)) {
					MCSRateLength++;
					*pDstRate++ = i;
				}
			}
		}
		/* Set the operational rate set CFG variables... */
		vdev = wlan_objmgr_get_vdev_by_id_from_pdev(
						mac->pdev, session_id,
						WLAN_LEGACY_SME_ID);
		if (vdev) {
			mlme_set_opr_rate(vdev, OperationalRates,
					  OperationalRatesLength);
			mlme_set_ext_opr_rate(vdev, ExtendedOperationalRates,
					      ExtendedOperationalRatesLength);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		} else {
			sme_err("null vdev");
		}

		wlan_mlme_set_cfg_str(MCSRateIdxSet,
				      &mac->mlme_cfg->rates.current_mcs_set,
				      MCSRateLength);
	} /* Parsing BSSDesc */
	else
		sme_err("failed to parse BssDesc");
}

static void csr_set_cfg_rate_set_from_profile(struct mac_context *mac,
					      struct csr_roam_profile *pProfile,
					      uint32_t session_id)
{
	tSirMacRateSetIE DefaultSupportedRates11a = { WLAN_ELEMID_RATES,
						      {8,
						       {SIR_MAC_RATE_6,
							SIR_MAC_RATE_9,
							SIR_MAC_RATE_12,
							SIR_MAC_RATE_18,
							SIR_MAC_RATE_24,
							SIR_MAC_RATE_36,
							SIR_MAC_RATE_48,
							SIR_MAC_RATE_54} } };
	tSirMacRateSetIE DefaultSupportedRates11b = { WLAN_ELEMID_RATES,
						      {4,
						       {SIR_MAC_RATE_1,
							SIR_MAC_RATE_2,
							SIR_MAC_RATE_5_5,
							SIR_MAC_RATE_11} } };
	enum csr_cfgdot11mode cfgDot11Mode;
	enum reg_wifi_band band;
	/* leave enough room for the max number of rates */
	uint8_t OperationalRates[CSR_DOT11_SUPPORTED_RATES_MAX];
	qdf_size_t OperationalRatesLength = 0;
	/* leave enough room for the max number of rates */
	uint8_t ExtendedOperationalRates
				[CSR_DOT11_EXTENDED_SUPPORTED_RATES_MAX];
	qdf_size_t ExtendedOperationalRatesLength = 0;
	uint32_t bss_op_ch_freq = 0;
	struct wlan_objmgr_vdev *vdev;

	if (pProfile->ChannelInfo.freq_list)
		bss_op_ch_freq = pProfile->ChannelInfo.freq_list[0];
	cfgDot11Mode = csr_roam_get_phy_mode_band_for_bss(mac, pProfile,
							  bss_op_ch_freq,
							  &band);
	/* For 11a networks, the 11a rates go into the Operational Rate set.
	 * For 11b and 11g networks, the 11b rates appear in the Operational
	 * Rate set. In either case, we can blindly put the rates we support
	 * into our Operational Rate set (including the basic rates, which we
	 * have already verified are supported earlier in the roaming decision).
	 */
	if (REG_BAND_5G == band) {
		/* 11a rates into the Operational Rate Set. */
		OperationalRatesLength =
			DefaultSupportedRates11a.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11a.supportedRateSet.rate);
		qdf_mem_copy(OperationalRates,
			     DefaultSupportedRates11a.supportedRateSet.rate,
			     OperationalRatesLength);

		/* Nothing in the Extended rate set. */
		ExtendedOperationalRatesLength = 0;
	} else if (eCSR_CFG_DOT11_MODE_11B == cfgDot11Mode) {
		/* 11b rates into the Operational Rate Set. */
		OperationalRatesLength =
			DefaultSupportedRates11b.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11b.supportedRateSet.rate);
		qdf_mem_copy(OperationalRates,
			     DefaultSupportedRates11b.supportedRateSet.rate,
			     OperationalRatesLength);
		/* Nothing in the Extended rate set. */
		ExtendedOperationalRatesLength = 0;
	} else {
		/* 11G */

		/* 11b rates into the Operational Rate Set. */
		OperationalRatesLength =
			DefaultSupportedRates11b.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11b.supportedRateSet.rate);
		qdf_mem_copy(OperationalRates,
			     DefaultSupportedRates11b.supportedRateSet.rate,
			     OperationalRatesLength);

		/* 11a rates go in the Extended rate set. */
		ExtendedOperationalRatesLength =
			DefaultSupportedRates11a.supportedRateSet.numRates *
			sizeof(*DefaultSupportedRates11a.supportedRateSet.rate);
		qdf_mem_copy(ExtendedOperationalRates,
			     DefaultSupportedRates11a.supportedRateSet.rate,
			     ExtendedOperationalRatesLength);
	}

	/* Set the operational rate set CFG variables... */
	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac->pdev,
						    session_id,
						    WLAN_LEGACY_SME_ID);
	if (vdev) {
		mlme_set_opr_rate(vdev, OperationalRates,
				  OperationalRatesLength);
		mlme_set_ext_opr_rate(vdev, ExtendedOperationalRates,
				      ExtendedOperationalRatesLength);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	} else {
		sme_err("null vdev");
	}
}

static void csr_roam_ccm_cfg_set_callback(struct mac_context *mac,
					  uint8_t session_id)
{
	tListElem *pEntry =
		csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	uint32_t sessionId;
	tSmeCmd *pCommand = NULL;

	if (!pEntry) {
		sme_err("CFG_CNF with active list empty");
		return;
	}
	pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
	sessionId = pCommand->vdev_id;

	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac->psoc, sessionId))
		sme_debug("LFR3: Set ccm vdev_id:%d", session_id);

	if (CSR_IS_ROAM_JOINING(mac, sessionId)
	    && CSR_IS_ROAM_SUBSTATE_CONFIG(mac, sessionId)) {
		csr_roaming_state_config_cnf_processor(mac, pCommand,
						       session_id);
	}
}

/* pIes may be NULL */
QDF_STATUS csr_roam_set_bss_config_cfg(struct mac_context *mac, uint32_t sessionId,
				       struct csr_roam_profile *pProfile,
				       struct bss_description *bss_desc,
				       struct bss_config_param *pBssConfig,
				       struct sDot11fBeaconIEs *pIes,
				       bool resetCountry)
{
	uint32_t cfgCb = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	uint32_t chan_freq = 0;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	/* Make sure we have the domain info for the BSS we try to connect to.
	 * Do we need to worry about sequence for OSs that are not Windows??
	 */
	if (bss_desc) {
		if ((QDF_SAP_MODE !=
			csr_get_session_persona(mac, sessionId)) &&
			csr_learn_11dcountry_information(
					mac, bss_desc, pIes, true)) {
			csr_apply_country_information(mac);
		}
		if ((wlan_reg_11d_enabled_on_host(mac->psoc)) && pIes) {
			if (!pIes->Country.present)
				csr_apply_channel_power_info_wrapper(mac);
		}
	}
	/* Qos */
	csr_set_qos_to_cfg(mac, sessionId, pBssConfig->qosType);
	/* SSID */
	csr_set_cfg_ssid(mac, &pBssConfig->SSID);

	/* Auth type */
	mac->mlme_cfg->wep_params.auth_type = pBssConfig->authType;

	/* encryption type */
	csr_set_cfg_privacy(mac, pProfile, (bool) pBssConfig->BssCap.privacy);
	/* short slot time */
	mac->mlme_cfg->feature_flags.enable_short_slot_time_11g =
						pBssConfig->uShortSlotTime;

	mac->mlme_cfg->power.local_power_constraint = pBssConfig->uPowerLimit;
	/* CB */

	if (CSR_IS_INFRA_AP(pProfile))
		chan_freq = pProfile->op_freq;
	else if (bss_desc)
		chan_freq = bss_desc->chan_freq;
	if (0 != chan_freq) {
		/* for now if we are on 2.4 Ghz, CB will be always disabled */
		if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq))
			cfgCb = WNI_CFG_CHANNEL_BONDING_MODE_DISABLE;
		else
			cfgCb = pBssConfig->cbMode;
	}
	/* Rate */
	/* Fixed Rate */
	if (bss_desc)
		csr_set_cfg_rate_set(mac, (eCsrPhyMode) pProfile->phyMode,
				     pProfile, bss_desc, pIes, sessionId);
	else
		csr_set_cfg_rate_set_from_profile(mac, pProfile, sessionId);

	mac->mlme_cfg->timeouts.join_failure_timeout =
		pBssConfig->uJoinTimeOut;

	/* Any roaming related changes should be above this line */
	if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac->psoc, sessionId)) {
		sme_debug("LFR3: Roam synch is in progress Session_id: %d",
			  sessionId);
		return QDF_STATUS_SUCCESS;
	}
	/* Make this the last CFG to set. The callback will trigger a
	 * join_req Join time out
	 */
	csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_CONFIG, sessionId);

	csr_roam_ccm_cfg_set_callback(mac, sessionId);

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_check_for_hidden_ssid_match() - Check if the current connected SSID
 * is hidden ssid and if it matches with the roamed AP ssid.
 * @mac: Global mac context pointer
 * @session: csr session pointer
 * @roamed_bss_desc: pointer to bss descriptor of roamed bss
 * @roamed_bss_ies: Roamed AP beacon/probe IEs pointer
 *
 * Return: True if the SSID is hidden and matches with roamed SSID else false
 */
static bool
csr_check_for_hidden_ssid_match(struct mac_context *mac,
				struct csr_roam_session *session,
				struct bss_description *roamed_bss_desc,
				tDot11fBeaconIEs *roamed_bss_ies)
{
	QDF_STATUS status;
	bool is_null_ssid_match = false;
	tDot11fBeaconIEs *connected_profile_ies = NULL;

	if (!session || !roamed_bss_ies)
		return false;

	status = csr_get_parsed_bss_description_ies(mac,
						    session->pConnectBssDesc,
						    &connected_profile_ies);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Unable to get IES");
		goto error;
	}

	if (!csr_is_nullssid(connected_profile_ies->SSID.ssid,
			     connected_profile_ies->SSID.num_ssid))
		goto error;

	/*
	 * After roam synch indication is received, the driver compares
	 * the SSID of the current AP and SSID of the roamed AP. If
	 * there is a mismatch, driver issues disassociate to current
	 * connected AP. This causes data path queues to be stopped and
	 * M2 to the roamed AP from userspace will fail if EAPOL is
	 * offloaded to userspace. The SSID of the current AP is
	 * parsed from the beacon IEs stored in the connected bss
	 * description. In hidden ssid case the SSID IE has 0 length
	 * and the host receives unicast probe with SSID of the
	 * AP in the roam synch indication. So SSID mismatch happens
	 * and validation fails. So fetch if the connected bss
	 * description has hidden ssid, fill the ssid from the
	 * csr_session connected_profile structure which will
	 * have the SSID.
	 */
	if (!roamed_bss_ies->SSID.present)
		goto error;

	if (roamed_bss_ies->SSID.num_ssid !=
	    session->connectedProfile.SSID.length)
		goto error;

	is_null_ssid_match = !qdf_mem_cmp(session->connectedProfile.SSID.ssId,
					  roamed_bss_ies->SSID.ssid,
					  roamed_bss_ies->SSID.num_ssid);
error:
	if (connected_profile_ies)
		qdf_mem_free(connected_profile_ies);

	return is_null_ssid_match;
}

/**
 * csr_check_for_allowed_ssid() - Function to check if the roamed
 * SSID is present in the configured Allowed SSID list
 * @mac: Pointer to global mac_ctx
 * @bss_desc: bss descriptor pointer
 * @roamed_bss_ies: Pointer to roamed BSS IEs
 *
 * Return: True if SSID match found else False
 */
static bool
csr_check_for_allowed_ssid(struct mac_context *mac,
			   struct bss_description *bss_desc,
			   tDot11fBeaconIEs *roamed_bss_ies)
{
	uint8_t i;
	tSirMacSSid *ssid_list =
		mac->roam.configParam.roam_params.ssid_allowed_list;
	uint8_t num_ssid_allowed_list =
		mac->roam.configParam.roam_params.num_ssid_allowed_list;

	if (!roamed_bss_ies) {
		sme_debug(" Roamed BSS IEs NULL");
		return false;
	}

	for (i = 0; i < num_ssid_allowed_list; i++) {
		if (ssid_list[i].length == roamed_bss_ies->SSID.num_ssid &&
		    !qdf_mem_cmp(ssid_list[i].ssId, roamed_bss_ies->SSID.ssid,
				 ssid_list[i].length))
			return true;
	}

	return false;
}

static
QDF_STATUS csr_roam_stop_network(struct mac_context *mac, uint32_t sessionId,
				 struct csr_roam_profile *roam_profile,
				 struct bss_description *bss_desc,
				 tDot11fBeaconIEs *pIes)
{
	QDF_STATUS status;
	struct bss_config_param *pBssConfig;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	bool ssid_match;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pBssConfig = qdf_mem_malloc(sizeof(struct bss_config_param));
	if (!pBssConfig)
		return QDF_STATUS_E_NOMEM;

	sme_debug("session id: %d", sessionId);

	status = csr_roam_prepare_bss_config(mac, roam_profile, bss_desc,
					     pBssConfig, pIes);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		enum csr_roam_substate substate;

		substate = eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING;
		pSession->bssParams.uCfgDot11Mode = pBssConfig->uCfgDot11Mode;
		/* This will allow to pass cbMode during join req */
		pSession->bssParams.cbMode = pBssConfig->cbMode;

		if (CSR_IS_INFRA_AP(roam_profile))
			csr_roam_prepare_bss_params(mac, sessionId,
						    roam_profile, bss_desc,
						    pBssConfig, pIes);
		if (csr_is_conn_state_infra(mac, sessionId)) {
			/*
			 * we are roaming from
			 * Infra to Infra across SSIDs
			 * (roaming to a new SSID)...
			 * Not worry about WDS connection for now
			 */
			ssid_match =
			    (csr_check_for_allowed_ssid(mac, bss_desc, pIes) ||
			     csr_check_for_hidden_ssid_match(mac, pSession,
							     bss_desc, pIes));
			if (!ssid_match)
				ssid_match = csr_is_ssid_equal(
						mac, pSession->pConnectBssDesc,
						bss_desc, pIes);
			if (bss_desc && !ssid_match)
				status = csr_roam_issue_disassociate(mac,
						sessionId, substate, false);
			else if (bss_desc)
				/*
				 * In an infra & going to an infra network with
				 * the same SSID.  This calls for a reassoc seq.
				 * So issue the CFG sets for this new AP. Set
				 * parameters for this Bss.
				 */
				status = csr_roam_set_bss_config_cfg(
						mac, sessionId, roam_profile,
						bss_desc, pBssConfig, pIes,
						false);
		} else if (bss_desc || CSR_IS_INFRA_AP(roam_profile)) {
			/*
			 * Not in Infra. We can go ahead and set
			 * the cfg for tne new network... nothing to stop.
			 */
			bool is_11r_roaming = false;

			is_11r_roaming = csr_roam_is11r_assoc(mac, sessionId);
			/* Set parameters for this Bss. */
			status = csr_roam_set_bss_config_cfg(mac, sessionId,
							     roam_profile,
							     bss_desc,
							     pBssConfig, pIes,
							     is_11r_roaming);
		}
	} /* Success getting BSS config info */
	qdf_mem_free(pBssConfig);
	return status;
}

/**
 * csr_roam_state_for_same_profile() - Determine roam state for same profile
 * @mac_ctx: pointer to mac context
 * @profile: Roam profile
 * @session: Roam session
 * @session_id: session id
 * @ies_local: local ies
 * @bss_descr: bss description
 *
 * This function will determine the roam state for same profile
 *
 * Return: Roaming state.
 */
static enum csr_join_state csr_roam_state_for_same_profile(
	struct mac_context *mac_ctx, struct csr_roam_profile *profile,
			struct csr_roam_session *session,
			uint32_t session_id, tDot11fBeaconIEs *ies_local,
			struct bss_description *bss_descr)
{
	QDF_STATUS status;
	struct bss_config_param bssConfig;

	if (csr_roam_is_same_profile_keys(mac_ctx, &session->connectedProfile,
				profile))
		return eCsrReassocToSelfNoCapChange;
	/* The key changes */
	qdf_mem_zero(&bssConfig, sizeof(bssConfig));
	status = csr_roam_prepare_bss_config(mac_ctx, profile, bss_descr,
				&bssConfig, ies_local);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		session->bssParams.uCfgDot11Mode =
				bssConfig.uCfgDot11Mode;
		session->bssParams.cbMode =
				bssConfig.cbMode;
		/* reapply the cfg including keys so reassoc happens. */
		status = csr_roam_set_bss_config_cfg(mac_ctx, session_id,
				profile, bss_descr, &bssConfig,
				ies_local, false);
		if (QDF_IS_STATUS_SUCCESS(status))
			return eCsrContinueRoaming;
	}

	return eCsrStopRoaming;

}

static enum csr_join_state csr_roam_join(struct mac_context *mac,
	uint32_t sessionId, tCsrScanResultInfo *pScanResult,
				   struct csr_roam_profile *pProfile)
{
	enum csr_join_state eRoamState = eCsrContinueRoaming;
	struct bss_description *bss_desc = &pScanResult->BssDescriptor;
	tDot11fBeaconIEs *pIesLocal = (tDot11fBeaconIEs *) (pScanResult->pvIes);
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return eCsrStopRoaming;
	}

	if (!pIesLocal &&
		!QDF_IS_STATUS_SUCCESS(csr_get_parsed_bss_description_ies(mac,
				bss_desc, &pIesLocal))) {
		sme_err("fail to parse IEs");
		return eCsrStopRoaming;
	}
	if (csr_is_infra_bss_desc(bss_desc)) {
		/*
		 * If we are connected in infra mode and the join bss descr is
		 * for the same BssID, then we are attempting to join the AP we
		 * are already connected with.  In that case, see if the Bss or
		 * sta capabilities have changed and handle the changes
		 * without disturbing the current association
		 */

		if (csr_is_conn_state_connected_infra(mac, sessionId) &&
			csr_is_bss_id_equal(bss_desc,
					    pSession->pConnectBssDesc) &&
			csr_is_ssid_equal(mac, pSession->pConnectBssDesc,
				bss_desc, pIesLocal)) {
			/*
			 * Check to see if the Auth type has changed in the
			 * profile.  If so, we don't want to reassociate with
			 * authenticating first.  To force this, stop the
			 * current association (Disassociate) and then re 'Join'
			 * the AP, wihch will force an Authentication (with the
			 * new Auth type) followed by a new Association.
			 */
			if (csr_is_same_profile(mac,
				&pSession->connectedProfile, pProfile)) {
				sme_warn("detect same profile");
				eRoamState =
					csr_roam_state_for_same_profile(mac,
						pProfile, pSession, sessionId,
						pIesLocal, bss_desc);
			} else if (!QDF_IS_STATUS_SUCCESS(
						csr_roam_issue_disassociate(
						mac,
						sessionId,
						eCSR_ROAM_SUBSTATE_DISASSOC_REQ,
						false))) {
				sme_err("fail disassoc session %d",
						sessionId);
				eRoamState = eCsrStopRoaming;
			}
		} else if (!QDF_IS_STATUS_SUCCESS(csr_roam_stop_network(mac,
				sessionId, pProfile, bss_desc, pIesLocal)))
			/* we used to pre-auth here with open auth
			 * networks but that wasn't working so well.
			 * stop the existing network before attempting
			 * to join the new network.
			 */
			eRoamState = eCsrStopRoaming;
	} else if (!QDF_IS_STATUS_SUCCESS(csr_roam_stop_network(mac, sessionId,
						pProfile, bss_desc,
						pIesLocal)))
		eRoamState = eCsrStopRoaming;

	if (pIesLocal && !pScanResult->pvIes)
		qdf_mem_free(pIesLocal);
	return eRoamState;
}

static QDF_STATUS
csr_roam_should_roam(struct mac_context *mac, uint32_t sessionId,
		     struct bss_description *bss_desc, uint32_t roamId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;
	roam_info->bss_desc = bss_desc;
	status = csr_roam_call_callback(mac, sessionId, roam_info, roamId,
					eCSR_ROAM_SHOULD_ROAM,
					eCSR_ROAM_RESULT_NONE);
	qdf_mem_free(roam_info);
	return status;
}

/* In case no matching BSS is found, use whatever default we can find */
static void csr_roam_assign_default_param(struct mac_context *mac,
					tSmeCmd *pCommand)
{
	/* Need to get all negotiated types in place first */
	/* auth type */
	/* Take the preferred Auth type. */
	switch (pCommand->u.roamCmd.roamProfile.AuthType.authType[0]) {
	default:
	case eCSR_AUTH_TYPE_WPA:
	case eCSR_AUTH_TYPE_WPA_PSK:
	case eCSR_AUTH_TYPE_WPA_NONE:
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_OPEN_SYSTEM;
		break;

	case eCSR_AUTH_TYPE_SHARED_KEY:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_SHARED_KEY;
		break;

	case eCSR_AUTH_TYPE_AUTOSWITCH:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_AUTOSWITCH;
		break;

	case eCSR_AUTH_TYPE_SAE:
	case eCSR_AUTH_TYPE_FT_SAE:
		pCommand->u.roamCmd.roamProfile.negotiatedAuthType =
			eCSR_AUTH_TYPE_SAE;
		break;
	}
	pCommand->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
		pCommand->u.roamCmd.roamProfile.EncryptionType.
		encryptionType[0];
	/* In this case, the multicast encryption needs to follow the
	 * uncast ones.
	 */
	pCommand->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
		pCommand->u.roamCmd.roamProfile.EncryptionType.
		encryptionType[0];
}

/**
 * csr_roam_select_bss() - Handle join scenario based on profile
 * @mac_ctx:             Global MAC Context
 * @roam_bss_entry:      The next BSS to join
 * @csr_result_info:     Result of join
 * @csr_scan_result:     Global scan result
 * @vdev_id:             SME Session ID
 * @roam_id:             Roaming ID
 * @roam_state:          Current roaming state
 * @bss_list:            BSS List
 *
 * Return: true if the entire BSS list is done, false otherwise.
 */
static bool csr_roam_select_bss(struct mac_context *mac_ctx,
		tListElem **roam_bss_entry, tCsrScanResultInfo **csr_result_info,
		struct tag_csrscan_result **csr_scan_result,
		uint32_t vdev_id, uint32_t roam_id,
		enum csr_join_state *roam_state,
		struct scan_result_list *bss_list)
{
	uint32_t chan_freq;
	bool status = false;
	struct tag_csrscan_result *scan_result = NULL;
	tCsrScanResultInfo *result = NULL;
	enum QDF_OPMODE op_mode;
	struct wlan_objmgr_vdev *vdev;
	QDF_STATUS qdf_status;
	struct if_mgr_event_data event_data;
	struct validate_bss_data candidate_info;

	vdev = wlan_objmgr_get_vdev_by_id_from_pdev(mac_ctx->pdev,
						    vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Vdev ref error");
		return true;
	}
	op_mode = wlan_vdev_mlme_get_opmode(vdev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	while (*roam_bss_entry) {
		scan_result = GET_BASE_ADDR(*roam_bss_entry, struct
				tag_csrscan_result, Link);
		/*
		 * If concurrency enabled take the
		 * concurrent connected channel first.
		 * Valid multichannel concurrent
		 * sessions exempted
		 */
		result = &scan_result->Result;
		chan_freq = result->BssDescriptor.chan_freq;

		qdf_mem_copy(candidate_info.peer_addr.bytes,
			result->BssDescriptor.bssId,
			sizeof(tSirMacAddr));
		candidate_info.chan_freq = result->BssDescriptor.chan_freq;
		candidate_info.beacon_interval =
			result->BssDescriptor.beaconInterval;
		candidate_info.chan_freq = result->BssDescriptor.chan_freq;
		event_data.validate_bss_info = candidate_info;
		qdf_status = ucfg_if_mgr_deliver_event(vdev,
						WLAN_IF_MGR_EV_VALIDATE_CANDIDATE,
						&event_data);

		result->BssDescriptor.beaconInterval =
						candidate_info.beacon_interval;

		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			*roam_state = eCsrStopRoamingDueToConcurrency;
			status = true;
			*roam_bss_entry = csr_ll_next(&bss_list->List,
						      *roam_bss_entry,
						      LL_ACCESS_LOCK);
			continue;
		}

		if (QDF_IS_STATUS_SUCCESS(csr_roam_should_roam(mac_ctx, vdev_id,
					  &result->BssDescriptor, roam_id))) {
			status = false;
			break;
		}

		*roam_state = eCsrStopRoamingDueToConcurrency;
		status = true;
		*roam_bss_entry = csr_ll_next(&bss_list->List, *roam_bss_entry,
					LL_ACCESS_LOCK);
	}
	*csr_result_info = result;
	*csr_scan_result = scan_result;
	return status;
}

/**
 * csr_roam_join_handle_profile() - Handle join scenario based on profile
 * @mac_ctx:             Global MAC Context
 * @session_id:          SME Session ID
 * @cmd:                 Command
 * @roam_info_ptr:       Pointed to the roaming info for join
 * @roam_state:          Current roaming state
 * @result:              Result of join
 * @scan_result:         Global scan result
 *
 * Return: None
 */
static void csr_roam_join_handle_profile(struct mac_context *mac_ctx,
		uint32_t session_id, tSmeCmd *cmd,
		struct csr_roam_info *roam_info_ptr,
		enum csr_join_state *roam_state, tCsrScanResultInfo *result,
		struct tag_csrscan_result *scan_result)
{
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
	uint8_t acm_mask = 0;
#endif
	QDF_STATUS status;
	struct csr_roam_session *session;
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	tDot11fBeaconIEs *ies_local = NULL;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (CSR_IS_INFRASTRUCTURE(profile) && roam_info_ptr) {
		if (session->bRefAssocStartCnt) {
			session->bRefAssocStartCnt--;
			roam_info_ptr->pProfile = profile;
			/*
			 * Complete the last assoc attempt as a
			 * new one is about to be tried
			 */
			csr_roam_call_callback(mac_ctx, session_id,
				roam_info_ptr, cmd->u.roamCmd.roamId,
				eCSR_ROAM_ASSOCIATION_COMPLETION,
				eCSR_ROAM_RESULT_NOT_ASSOCIATED);
		}

		qdf_mem_zero(roam_info_ptr, sizeof(struct csr_roam_info));
		if (!scan_result)
			cmd->u.roamCmd.roamProfile.uapsd_mask = 0;
		else
			ies_local = scan_result->Result.pvIes;

		if (!result) {
			sme_err(" cannot parse IEs");
			*roam_state = eCsrStopRoaming;
			return;
		} else if (scan_result && !ies_local &&
				(!QDF_IS_STATUS_SUCCESS(
					csr_get_parsed_bss_description_ies(
						mac_ctx, &result->BssDescriptor,
						&ies_local)))) {
			sme_err(" cannot parse IEs");
			*roam_state = eCsrStopRoaming;
			return;
		}
		roam_info_ptr->bss_desc = &result->BssDescriptor;
		cmd->u.roamCmd.pLastRoamBss = roam_info_ptr->bss_desc;
		/* dont put uapsd_mask if BSS doesn't support uAPSD */
		if (scan_result && cmd->u.roamCmd.roamProfile.uapsd_mask
				&& CSR_IS_QOS_BSS(ies_local)
				&& CSR_IS_UAPSD_BSS(ies_local)) {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
			acm_mask = sme_qos_get_acm_mask(mac_ctx,
					&result->BssDescriptor, ies_local);
#endif /* WLAN_MDM_CODE_REDUCTION_OPT */
		} else {
			cmd->u.roamCmd.roamProfile.uapsd_mask = 0;
		}
		if (ies_local && !scan_result->Result.pvIes)
			qdf_mem_free(ies_local);
		roam_info_ptr->pProfile = profile;
		session->bRefAssocStartCnt++;
		csr_roam_call_callback(mac_ctx, session_id, roam_info_ptr,
			cmd->u.roamCmd.roamId, eCSR_ROAM_ASSOCIATION_START,
			eCSR_ROAM_RESULT_NONE);
	}
	if (cmd->u.roamCmd.pRoamBssEntry) {
		/*
		 * We have BSS
		 * Need to assign these value because
		 * they are used in csr_is_same_profile
		 */
		scan_result = GET_BASE_ADDR(cmd->u.roamCmd.pRoamBssEntry,
				struct tag_csrscan_result, Link);
		/*
		 * The OSEN IE doesn't provide the cipher suite.Therefore set
		 * to constant value of AES
		 */
		if (cmd->u.roamCmd.roamProfile.bOSENAssociation) {
			cmd->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
				eCSR_ENCRYPT_TYPE_AES;
			cmd->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
				eCSR_ENCRYPT_TYPE_AES;
		} else {
			/* Negotiated while building scan result. */
			cmd->u.roamCmd.roamProfile.negotiatedUCEncryptionType =
				scan_result->ucEncryptionType;
			cmd->u.roamCmd.roamProfile.negotiatedMCEncryptionType =
				scan_result->mcEncryptionType;
		}

		cmd->u.roamCmd.roamProfile.negotiatedAuthType =
			scan_result->authType;
		if (cmd->u.roamCmd.fReassocToSelfNoCapChange) {
			/* trying to connect to the one already connected */
			cmd->u.roamCmd.fReassocToSelfNoCapChange = false;
			*roam_state = eCsrReassocToSelfNoCapChange;
			return;
		}
		/* Attempt to Join this Bss... */
		*roam_state = csr_roam_join(mac_ctx, session_id,
				&scan_result->Result, profile);
		return;
	}

	if (CSR_IS_INFRA_AP(profile)) {
		/* Attempt to start this WDS... */
		csr_roam_assign_default_param(mac_ctx, cmd);
		/* For AP WDS, we dont have any BSSDescription */
		status = csr_roam_start_wds(mac_ctx, session_id, profile, NULL);
		if (QDF_IS_STATUS_SUCCESS(status))
			*roam_state = eCsrContinueRoaming;
		else
			*roam_state = eCsrStopRoaming;
	} else if (CSR_IS_NDI(profile)) {
		csr_roam_assign_default_param(mac_ctx, cmd);
		status = csr_roam_start_ndi(mac_ctx, session_id, profile);
		if (QDF_IS_STATUS_SUCCESS(status))
			*roam_state = eCsrContinueRoaming;
		else
			*roam_state = eCsrStopRoaming;
	} else {
		/* Nothing we can do */
		sme_warn("cannot continue without BSS list");
		*roam_state = eCsrStopRoaming;
		return;
	}

}
/**
 * csr_roam_join_next_bss() - Pick the next BSS for join
 * @mac_ctx:             Global MAC Context
 * @cmd:                 Command
 * @use_same_bss:        Use Same BSS to Join
 *
 * Return: The Join State
 */
static enum csr_join_state csr_roam_join_next_bss(struct mac_context *mac_ctx,
		tSmeCmd *cmd, bool use_same_bss)
{
	struct tag_csrscan_result *scan_result = NULL;
	enum csr_join_state roam_state = eCsrStopRoaming;
	struct scan_result_list *bss_list =
		(struct scan_result_list *) cmd->u.roamCmd.hBSSList;
	bool done = false;
	struct csr_roam_info *roam_info = NULL;
	uint32_t session_id = cmd->vdev_id;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	struct csr_roam_joinstatus *join_status;
	tCsrScanResultInfo *result = NULL;

	if (!session) {
		sme_err("session %d not found", session_id);
		return eCsrStopRoaming;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return eCsrStopRoaming;

	qdf_mem_copy(&roam_info->bssid, &session->joinFailStatusCode.bssId,
			sizeof(tSirMacAddr));
	/*
	 * When handling AP's capability change, continue to associate
	 * to same BSS and make sure pRoamBssEntry is not Null.
	 */
	if (bss_list) {
		if (!cmd->u.roamCmd.pRoamBssEntry) {
			/* Try the first BSS */
			cmd->u.roamCmd.pLastRoamBss = NULL;
			cmd->u.roamCmd.pRoamBssEntry =
				csr_ll_peek_head(&bss_list->List,
					LL_ACCESS_LOCK);
		} else if (!use_same_bss) {
			cmd->u.roamCmd.pRoamBssEntry =
				csr_ll_next(&bss_list->List,
					cmd->u.roamCmd.pRoamBssEntry,
					LL_ACCESS_LOCK);
			/*
			 * Done with all the BSSs.
			 * In this case, will tell HDD the
			 * completion
			 */
			if (!cmd->u.roamCmd.pRoamBssEntry)
				goto end;
			/*
			 * We need to indicate to HDD that we
			 * are done with this one.
			 */
			roam_info->bss_desc = cmd->u.roamCmd.pLastRoamBss;
			join_status = &session->joinFailStatusCode;
			roam_info->status_code = join_status->status_code;
			roam_info->reasonCode = join_status->reasonCode;
		} else {
			/*
			 * Try connect to same BSS again. Fill roam_info for the
			 * last attemp to indicate to HDD.
			 */
			roam_info->bss_desc = cmd->u.roamCmd.pLastRoamBss;
			join_status = &session->joinFailStatusCode;
			roam_info->status_code = join_status->status_code;
			roam_info->reasonCode = join_status->reasonCode;
		}

		done = csr_roam_select_bss(mac_ctx,
					   &cmd->u.roamCmd.pRoamBssEntry,
					   &result, &scan_result, session_id,
					   cmd->u.roamCmd.roamId,
					   &roam_state, bss_list);
		if (done)
			goto end;
	}

	roam_info->u.pConnectedProfile = &session->connectedProfile;

	csr_roam_join_handle_profile(mac_ctx, session_id, cmd, roam_info,
		&roam_state, result, scan_result);
end:
	if ((eCsrStopRoaming == roam_state) && CSR_IS_INFRASTRUCTURE(profile) &&
		(session->bRefAssocStartCnt > 0)) {
		/*
		 * Need to indicate association_completion if association_start
		 * has been done
		 */
		session->bRefAssocStartCnt--;
		/*
		 * Complete the last assoc attempte as a
		 * new one is about to be tried
		 */
		roam_info->pProfile = profile;
		csr_roam_call_callback(mac_ctx, session_id,
			roam_info, cmd->u.roamCmd.roamId,
			eCSR_ROAM_ASSOCIATION_COMPLETION,
			eCSR_ROAM_RESULT_NOT_ASSOCIATED);
	}
	qdf_mem_free(roam_info);

	return roam_state;
}

static QDF_STATUS csr_roam(struct mac_context *mac, tSmeCmd *pCommand,
			   bool use_same_bss)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum csr_join_state RoamState;
	enum csr_roam_substate substate;
	uint32_t sessionId = pCommand->vdev_id;

	/* Attept to join a Bss... */
	RoamState = csr_roam_join_next_bss(mac, pCommand, use_same_bss);

	/* if nothing to join.. */
	if ((eCsrStopRoaming == RoamState) ||
		(eCsrStopRoamingDueToConcurrency == RoamState)) {
		bool fComplete = false;
		/* and if connected in Infrastructure mode... */
		if (csr_is_conn_state_infra(mac, sessionId)) {
			/* ... then we need to issue a disassociation */
			substate = eCSR_ROAM_SUBSTATE_DISASSOC_NOTHING_TO_JOIN;
			status = csr_roam_issue_disassociate(mac, sessionId,
					substate, false);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_warn("fail issuing disassoc status = %d",
					status);
				/*
				 * roam command is completed by caller in the
				 * failed case
				 */
				fComplete = true;
			}
		} else if (csr_is_conn_state_connected_infra_ap(mac,
					sessionId)) {
			substate = eCSR_ROAM_SUBSTATE_STOP_BSS_REQ;
			status = csr_roam_issue_stop_bss(mac, sessionId,
						substate);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_warn("fail issuing stop bss status = %d",
					status);
				/*
				 * roam command is completed by caller in the
				 * failed case
				 */
				fComplete = true;
			}
		} else {
			fComplete = true;
		}

		if (fComplete) {
			/* otherwise, we can complete the Roam command here. */
			if (eCsrStopRoamingDueToConcurrency == RoamState)
				csr_roam_complete(mac,
					eCsrJoinFailureDueToConcurrency, NULL,
					sessionId);
			else
				csr_roam_complete(mac,
					eCsrNothingToJoin, NULL, sessionId);
		}
	} else if (eCsrReassocToSelfNoCapChange == RoamState) {
		csr_roam_complete(mac, eCsrSilentlyStopRoamingSaveState,
				NULL, sessionId);
	}

	return status;
}

static
QDF_STATUS csr_process_ft_reassoc_roam_command(struct mac_context *mac,
					       tSmeCmd *pCommand)
{
	uint32_t sessionId;
	struct csr_roam_session *pSession;
	struct tag_csrscan_result *pScanResult = NULL;
	struct bss_description *bss_desc = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	sessionId = pCommand->vdev_id;
	pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (CSR_IS_ROAMING(pSession) && pSession->fCancelRoaming) {
		/* the roaming is cancelled. Simply complete the command */
		sme_debug("Roam command canceled");
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	if (pCommand->u.roamCmd.pRoamBssEntry) {
		pScanResult =
			GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry,
				      struct tag_csrscan_result, Link);
		bss_desc = &pScanResult->Result.BssDescriptor;
	} else {
		/* the roaming is cancelled. Simply complete the command */
		sme_debug("Roam command canceled");
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	status = csr_roam_issue_reassociate(mac, sessionId, bss_desc,
					    (tDot11fBeaconIEs *) (pScanResult->
								  Result.pvIes),
					    &pCommand->u.roamCmd.roamProfile);
	return status;
}

/**
 * csr_roam_trigger_reassociate() - Helper function to trigger reassociate
 * @mac_ctx: pointer to mac context
 * @cmd: sme command
 * @session_ptr: session pointer
 * @session_id: session id
 *
 * This function will trigger reassociate.
 *
 * Return: QDF_STATUS for success or failure.
 */
static QDF_STATUS
csr_roam_trigger_reassociate(struct mac_context *mac_ctx, tSmeCmd *cmd,
			     struct csr_roam_session *session_ptr,
			     uint32_t session_id)
{
	tDot11fBeaconIEs *pIes = NULL;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	if (session_ptr->pConnectBssDesc) {
		status = csr_get_parsed_bss_description_ies(mac_ctx,
				session_ptr->pConnectBssDesc, &pIes);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("fail to parse IEs");
		} else {
			roam_info->reasonCode =
					eCsrRoamReasonStaCapabilityChanged;
			csr_roam_call_callback(mac_ctx, session_ptr->sessionId,
					roam_info, 0, eCSR_ROAM_ROAMING_START,
					eCSR_ROAM_RESULT_NONE);
			session_ptr->roamingReason = eCsrReassocRoaming;
			roam_info->bss_desc = session_ptr->pConnectBssDesc;
			roam_info->pProfile = &cmd->u.roamCmd.roamProfile;
			session_ptr->bRefAssocStartCnt++;
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
				cmd->u.roamCmd.roamId,
				eCSR_ROAM_ASSOCIATION_START,
				eCSR_ROAM_RESULT_NONE);

			sme_debug("calling csr_roam_issue_reassociate");
			status = csr_roam_issue_reassociate(mac_ctx, session_id,
					session_ptr->pConnectBssDesc, pIes,
					&cmd->u.roamCmd.roamProfile);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_err("failed status %d", status);
				csr_release_command(mac_ctx, cmd);
			} else {
				csr_neighbor_roam_state_transition(mac_ctx,
					eCSR_NEIGHBOR_ROAM_STATE_REASSOCIATING,
					session_id);
			}


			qdf_mem_free(pIes);
			pIes = NULL;
		}
	} else {
		sme_err("reassoc to same AP failed as connected BSS is NULL");
		status = QDF_STATUS_E_FAILURE;
	}
	qdf_mem_free(roam_info);
	return status;
}

/**
 * csr_allow_concurrent_sta_connections() - Wrapper for policy_mgr api
 * @mac: mac context
 * @vdev_id: vdev id
 *
 * This function invokes policy mgr api to check for support of
 * simultaneous connections on concurrent STA interfaces.
 *
 *  Return: If supports return true else false.
 */
static
bool csr_allow_concurrent_sta_connections(struct mac_context *mac,
					  uint32_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	enum QDF_OPMODE vdev_mode;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("vdev object not found for vdev_id %u", vdev_id);
		return false;
	}
	vdev_mode = wlan_vdev_mlme_get_opmode(vdev);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	/* If vdev mode is STA then proceed further */
	if (vdev_mode != QDF_STA_MODE)
		return true;

	if (policy_mgr_allow_concurrency(mac->psoc, PM_STA_MODE, 0,
					 HW_MODE_20_MHZ))
		return true;

	return false;
}

static void csr_get_peer_rssi_cb(struct stats_event *ev, void *cookie)
{
	struct mac_context *mac = (struct mac_context *)cookie;

	if (!mac) {
		sme_err("Invalid mac ctx");
		return;
	}

	if (!ev->peer_stats) {
		sme_debug("no peer stats");
		goto disconnect_stats_complete;
	}

	mac->peer_rssi = ev->peer_stats->peer_rssi;
	mac->peer_txrate = ev->peer_stats->tx_rate;
	mac->peer_rxrate = ev->peer_stats->rx_rate;
	if (!ev->peer_extended_stats) {
		sme_debug("no peer extended stats");
		goto disconnect_stats_complete;
	}
	mac->rx_mc_bc_cnt = ev->peer_extended_stats->rx_mc_bc_cnt;

disconnect_stats_complete:
	csr_roam_get_disconnect_stats_complete(mac);
}

static void csr_get_peer_rssi(struct mac_context *mac, uint32_t session_id,
			      struct qdf_mac_addr peer_mac)
{
	struct wlan_objmgr_vdev *vdev;
	struct request_info info = {0};
	QDF_STATUS status;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
						mac->psoc,
						session_id,
						WLAN_LEGACY_SME_ID);
	if (vdev) {
		info.cookie = mac;
		info.u.get_peer_rssi_cb = csr_get_peer_rssi_cb;
		info.vdev_id = wlan_vdev_get_id(vdev);
		info.pdev_id = wlan_objmgr_pdev_get_pdev_id(
					wlan_vdev_get_pdev(vdev));
		qdf_mem_copy(info.peer_mac_addr, &peer_mac, QDF_MAC_ADDR_SIZE);
		status = ucfg_mc_cp_stats_send_stats_request(
						vdev,
						TYPE_PEER_STATS,
						&info);
		sme_debug("peer_mac" QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(peer_mac.bytes));
		if (QDF_IS_STATUS_ERROR(status))
			sme_err("stats req failed: %d", status);

		wma_get_rx_retry_cnt(mac, session_id, info.peer_mac_addr);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	}
}

QDF_STATUS csr_roam_process_command(struct mac_context *mac, tSmeCmd *pCommand)
{
	QDF_STATUS lock_status, status = QDF_STATUS_SUCCESS;
	uint32_t sessionId = pCommand->vdev_id;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	sme_debug("Roam Reason: %d sessionId: %d",
		pCommand->u.roamCmd.roamReason, sessionId);

	pSession->disconnect_reason = pCommand->u.roamCmd.disconnect_reason;

	switch (pCommand->u.roamCmd.roamReason) {
	case eCsrForcedDisassoc:
		status = csr_roam_process_disassoc_deauth(mac, pCommand,
				true, false);
		lock_status = sme_acquire_global_lock(&mac->sme);
		if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
			csr_roam_complete(mac, eCsrNothingToJoin, NULL,
					  sessionId);
			return lock_status;
		}
		csr_free_roam_profile(mac, sessionId);
		sme_release_global_lock(&mac->sme);
		break;
	case eCsrSmeIssuedDisassocForHandoff:
		/* Not to free mac->roam.pCurRoamProfile (via
		 * csr_free_roam_profile) because its needed after disconnect
		 */
		status = csr_roam_process_disassoc_deauth(mac, pCommand,
				true, false);

		break;
	case eCsrForcedDisassocMICFailure:
		status = csr_roam_process_disassoc_deauth(mac, pCommand,
				true, true);
		lock_status = sme_acquire_global_lock(&mac->sme);
		if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
			csr_roam_complete(mac, eCsrNothingToJoin, NULL,
					  sessionId);
			return lock_status;
		}
		csr_free_roam_profile(mac, sessionId);
		sme_release_global_lock(&mac->sme);
		break;
	case eCsrForcedDeauth:
		status = csr_roam_process_disassoc_deauth(mac, pCommand,
				false, false);
		lock_status = sme_acquire_global_lock(&mac->sme);
		if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
			csr_roam_complete(mac, eCsrNothingToJoin, NULL,
					  sessionId);
			return lock_status;
		}
		csr_free_roam_profile(mac, sessionId);
		sme_release_global_lock(&mac->sme);
		break;
	case eCsrHddIssuedReassocToSameAP:
	case eCsrSmeIssuedReassocToSameAP:
		status = csr_roam_trigger_reassociate(mac, pCommand,
						      pSession, sessionId);
		break;
	case eCsrCapsChange:
		sme_err("received eCsrCapsChange ");
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING,
				sessionId);
		status = csr_roam_issue_disassociate(mac, sessionId,
				eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING,
				false);
		break;
	case eCsrSmeIssuedFTReassoc:
		sme_debug("received FT Reassoc Req");
		status = csr_process_ft_reassoc_roam_command(mac, pCommand);
		break;

	case eCsrStopBss:
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING,
				sessionId);
		status = csr_roam_issue_stop_bss(mac, sessionId,
				eCSR_ROAM_SUBSTATE_STOP_BSS_REQ);
		break;

	case eCsrForcedDisassocSta:
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING,
				sessionId);
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_DISASSOC_REQ,
				sessionId);
		sme_debug("Disassociate issued with reason: %d",
			pCommand->u.roamCmd.reason);

		status = csr_send_mb_disassoc_req_msg(mac, sessionId,
				pCommand->u.roamCmd.peerMac,
				pCommand->u.roamCmd.reason);
		break;

	case eCsrForcedDeauthSta:
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING,
				sessionId);
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_DEAUTH_REQ,
				sessionId);
		sme_debug("Deauth issued with reason: %d",
			  pCommand->u.roamCmd.reason);

		status = csr_send_mb_deauth_req_msg(mac, sessionId,
				pCommand->u.roamCmd.peerMac,
				pCommand->u.roamCmd.reason);
		break;

	case eCsrPerformPreauth:
		sme_debug("Attempting FT PreAuth Req");
		status = csr_roam_issue_ft_preauth_req(mac, sessionId,
				pCommand->u.roamCmd.pLastRoamBss);
		break;

	case eCsrHddIssued:
		/*
		 * Check for simultaneous connection support on
		 * multiple STA interfaces.
		 */
		if (!csr_allow_concurrent_sta_connections(mac, sessionId)) {
			sme_err("No support of conc STA con");
			csr_roam_complete(mac, eCsrNothingToJoin, NULL,
					  sessionId);
			status = QDF_STATUS_E_FAILURE;
			break;
		}
		/* for success case */
		/* fallthrough */
	default:
		csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING,
				sessionId);

		if (pCommand->u.roamCmd.fUpdateCurRoamProfile) {
			/* Remember the roaming profile */
			lock_status = sme_acquire_global_lock(&mac->sme);
			if (!QDF_IS_STATUS_SUCCESS(lock_status)) {
				csr_roam_complete(mac, eCsrNothingToJoin, NULL,
						  sessionId);
				return lock_status;
			}
			csr_free_roam_profile(mac, sessionId);
			pSession->pCurRoamProfile =
				qdf_mem_malloc(sizeof(struct csr_roam_profile));
			if (pSession->pCurRoamProfile) {
				csr_roam_copy_profile(mac,
					pSession->pCurRoamProfile,
					&pCommand->u.roamCmd.roamProfile,
					sessionId);
			}
			sme_release_global_lock(&mac->sme);
		}
		pCommand->u.roamCmd.connect_active_time =
					qdf_mc_timer_get_system_time();
		/*
		 * At this point original uapsd_mask is saved in
		 * pCurRoamProfile. uapsd_mask in the pCommand may change from
		 * this point on. Attempt to roam with the new scan results
		 * (if we need to..)
		 */
		status = csr_roam(mac, pCommand, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_warn("csr_roam() failed with status = 0x%08X",
				status);
		break;
	}
	return status;
}

void csr_reinit_roam_cmd(struct mac_context *mac, tSmeCmd *pCommand)
{
	if (pCommand->u.roamCmd.fReleaseBssList) {
		csr_scan_result_purge(mac, pCommand->u.roamCmd.hBSSList);
		pCommand->u.roamCmd.fReleaseBssList = false;
		pCommand->u.roamCmd.hBSSList = CSR_INVALID_SCANRESULT_HANDLE;
	}
	if (pCommand->u.roamCmd.fReleaseProfile) {
		csr_release_profile(mac, &pCommand->u.roamCmd.roamProfile);
		pCommand->u.roamCmd.fReleaseProfile = false;
	}
	pCommand->u.roamCmd.pLastRoamBss = NULL;
	pCommand->u.roamCmd.pRoamBssEntry = NULL;
	/* Because u.roamCmd is union and share with scanCmd and StatusChange */
	qdf_mem_zero(&pCommand->u.roamCmd, sizeof(struct roam_cmd));
}

void csr_reinit_wm_status_change_cmd(struct mac_context *mac,
			tSmeCmd *pCommand)
{
	qdf_mem_zero(&pCommand->u.wmStatusChangeCmd,
			sizeof(struct wmstatus_changecmd));
}

void csr_roam_complete(struct mac_context *mac_ctx,
		       enum csr_roamcomplete_result Result,
		       void *Context, uint8_t session_id)
{
	tSmeCmd *sme_cmd;
	struct wlan_serialization_command *cmd;

	cmd = wlan_serialization_peek_head_active_cmd_using_psoc(
				mac_ctx->psoc, false);
	if (!cmd) {
		sme_err("Roam completion called but cmd is not active");
		return;
	}
	sme_cmd = cmd->umac_cmd;
	if (!sme_cmd) {
		sme_err("sme_cmd is NULL");
		return;
	}
	if (eSmeCommandRoam == sme_cmd->command) {
		csr_roam_process_results(mac_ctx, sme_cmd, Result, Context);
		csr_release_command(mac_ctx, sme_cmd);
	}
}


void csr_reset_pmkid_candidate_list(struct mac_context *mac,
						uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session: %d not found", sessionId);
		return;
	}
	qdf_mem_zero(&(pSession->PmkidCandidateInfo[0]),
		    sizeof(tPmkidCandidateInfo) * CSR_MAX_PMKID_ALLOWED);
	pSession->NumPmkidCandidate = 0;
}

/* Returns whether the current association is a 11r assoc or not */
bool csr_roam_is11r_assoc(struct mac_context *mac, uint8_t sessionId)
{
	return csr_neighbor_roam_is11r_assoc(mac, sessionId);
}

/* Returns whether "Legacy Fast Roaming" is currently enabled...or not */
bool csr_roam_is_fast_roam_enabled(struct mac_context *mac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = NULL;

	if (CSR_IS_SESSION_VALID(mac, sessionId)) {
		pSession = CSR_GET_SESSION(mac, sessionId);
		if (pSession->pCurRoamProfile) {
			if (pSession->pCurRoamProfile->csrPersona !=
			    QDF_STA_MODE) {
				return false;
			}
		}
	}
	if (true == CSR_IS_FASTROAM_IN_CONCURRENCY_INI_FEATURE_ENABLED(mac)) {
		return mac->mlme_cfg->lfr.lfr_enabled;
	} else {
		return mac->mlme_cfg->lfr.lfr_enabled &&
			(!csr_is_concurrent_session_running(mac));
	}
}

static
void csr_update_scan_entry_associnfo(struct mac_context *mac_ctx,
				     struct csr_roam_session *session,
				     enum scan_entry_connection_state state)
{
	QDF_STATUS status;
	struct mlme_info mlme;
	struct bss_info bss;
	tCsrRoamConnectedProfile *conn_profile;

	if (!session) {
		sme_debug("session is NULL");
		return;
	}
	conn_profile = &session->connectedProfile;
	if (!CSR_IS_INFRASTRUCTURE(conn_profile)) {
		sme_debug("not infra return");
		return;
	}

	qdf_copy_macaddr(&bss.bssid, &conn_profile->bssid);
	bss.freq = conn_profile->op_freq;
	bss.ssid.length = conn_profile->SSID.length;
	qdf_mem_copy(&bss.ssid.ssid, &conn_profile->SSID.ssId,
		     bss.ssid.length);

	sme_debug("Update MLME info in scan database. bssid "QDF_MAC_ADDR_FMT" ssid:%.*s freq %d state: %d",
		  QDF_MAC_ADDR_REF(bss.bssid.bytes), bss.ssid.length,
		  bss.ssid.ssid, bss.freq, state);
	mlme.assoc_state = state;
	status = ucfg_scan_update_mlme_by_bssinfo(mac_ctx->pdev, &bss, &mlme);
	if (QDF_IS_STATUS_ERROR(status))
		sme_debug("Failed to update the MLME info in scan entry");
}

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
static eCsrPhyMode csr_roamdot11mode_to_phymode(uint8_t dot11mode)
{
	eCsrPhyMode phymode = eCSR_DOT11_MODE_abg;

	switch (dot11mode) {
	case MLME_DOT11_MODE_ALL:
		phymode = eCSR_DOT11_MODE_abg;
		break;
	case MLME_DOT11_MODE_11A:
		phymode = eCSR_DOT11_MODE_11a;
		break;
	case MLME_DOT11_MODE_11B:
		phymode = eCSR_DOT11_MODE_11b;
		break;
	case MLME_DOT11_MODE_11G:
		phymode = eCSR_DOT11_MODE_11g;
		break;
	case MLME_DOT11_MODE_11N:
		phymode = eCSR_DOT11_MODE_11n;
		break;
	case MLME_DOT11_MODE_11G_ONLY:
		phymode = eCSR_DOT11_MODE_11g_ONLY;
		break;
	case MLME_DOT11_MODE_11N_ONLY:
		phymode = eCSR_DOT11_MODE_11n_ONLY;
		break;
	case MLME_DOT11_MODE_11AC:
		phymode = eCSR_DOT11_MODE_11ac;
		break;
	case MLME_DOT11_MODE_11AC_ONLY:
		phymode = eCSR_DOT11_MODE_11ac_ONLY;
		break;
	case MLME_DOT11_MODE_11AX:
		phymode = eCSR_DOT11_MODE_11ax;
		break;
	case MLME_DOT11_MODE_11AX_ONLY:
		phymode = eCSR_DOT11_MODE_11ax_ONLY;
		break;
	default:
		break;
	}

	return phymode;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void csr_roam_synch_clean_up(struct mac_context *mac, uint8_t session_id)
{
	struct scheduler_msg msg = {0};
	struct roam_offload_synch_fail *roam_offload_failed = NULL;

	/* Clean up the roam synch in progress for LFR3 */
	sme_err("Roam Synch Failed, Clean Up");

	roam_offload_failed = qdf_mem_malloc(
				sizeof(struct roam_offload_synch_fail));
	if (!roam_offload_failed)
		return;

	roam_offload_failed->session_id = session_id;
	msg.type     = WMA_ROAM_OFFLOAD_SYNCH_FAIL;
	msg.reserved = 0;
	msg.bodyptr  = roam_offload_failed;
	if (!QDF_IS_STATUS_SUCCESS(scheduler_post_message(QDF_MODULE_ID_SME,
							  QDF_MODULE_ID_WMA,
							  QDF_MODULE_ID_WMA,
							  &msg))) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"%s: Unable to post WMA_ROAM_OFFLOAD_SYNCH_FAIL to WMA",
			__func__);
		qdf_mem_free(roam_offload_failed);
	}
}
#endif

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
/**
 * csr_roam_copy_ht_profile() - Copy from src to dst
 * @dst_profile:          Destination HT profile
 * @src_profile:          Source HT profile
 *
 * Copy the HT profile from the given source to destination
 *
 * Return: None
 */
static void csr_roam_copy_ht_profile(tCsrRoamHTProfile *dst_profile,
		struct ht_profile *src_profile)
{
	dst_profile->phymode =
		csr_roamdot11mode_to_phymode(src_profile->dot11mode);
	dst_profile->htCapability = src_profile->htCapability;
	dst_profile->htSupportedChannelWidthSet =
		src_profile->htSupportedChannelWidthSet;
	dst_profile->htRecommendedTxWidthSet =
		src_profile->htRecommendedTxWidthSet;
	dst_profile->htSecondaryChannelOffset =
		src_profile->htSecondaryChannelOffset;
	dst_profile->vhtCapability = src_profile->vhtCapability;
	dst_profile->apCenterChan = src_profile->apCenterChan;
	dst_profile->apChanWidth = src_profile->apChanWidth;
}
#endif

#if defined(WLAN_FEATURE_FILS_SK)
/**
 * csr_update_fils_seq_number() - Copy FILS sequence number to roam info
 * @session: CSR Roam Session
 * @roam_info: Roam info
 *
 * Return: None
 */
static void csr_update_fils_seq_number(struct csr_roam_session *session,
					 struct csr_roam_info *roam_info)
{
	roam_info->is_fils_connection = true;
	roam_info->fils_seq_num = session->fils_seq_num;
	pe_debug("FILS sequence number %x", session->fils_seq_num);
}
#else
static inline void csr_update_fils_seq_number(struct csr_roam_session *session,
						struct csr_roam_info *roam_info)
{}
#endif

/**
 * csr_roam_process_results_default() - Process the result for start bss
 * @mac_ctx:          Global MAC Context
 * @cmd:              Command to be processed
 * @context:          Additional data in context of the cmd
 *
 * Return: None
 */
static void csr_roam_process_results_default(struct mac_context *mac_ctx,
		     tSmeCmd *cmd, void *context, enum csr_roamcomplete_result
			res)
{
	uint32_t session_id = cmd->vdev_id;
	struct csr_roam_session *session;
	struct csr_roam_info *roam_info;
	QDF_STATUS status;
	struct csr_roam_connectedinfo *prev_connect_info = NULL;

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	session = CSR_GET_SESSION(mac_ctx, session_id);

	prev_connect_info = &session->prev_assoc_ap_info;

	sme_debug("Assoc ref count: %d", session->bRefAssocStartCnt);

	/* Update AP's assoc info in scan before removing connectedProfile */
	switch (cmd->u.roamCmd.roamReason) {
	case eCsrSmeIssuedDisassocForHandoff:
	case eCsrForcedDisassoc:
	case eCsrForcedDeauth:
	case eCsrForcedDisassocMICFailure:
		csr_update_scan_entry_associnfo(mac_ctx, session,
						SCAN_ENTRY_CON_STATE_NONE);
		break;
	default:
		break;
	}
	if (CSR_IS_INFRASTRUCTURE(&session->connectedProfile)
		|| CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ(mac_ctx, session_id)) {
		/*
		 * do not free for the other profiles as we need
		 * to send down stop BSS later
		 */
		csr_free_connect_bss_desc(mac_ctx, session_id);
		csr_roam_free_connect_profile(&session->connectedProfile);
		csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
		csr_set_default_dot11_mode(mac_ctx);
	}

	/* Copy FILS sequence number used to be updated to userspace */
	if (session->is_fils_connection)
		csr_update_fils_seq_number(session, roam_info);

	switch (cmd->u.roamCmd.roamReason) {
	/*
	 * If this transition is because of an 802.11 OID, then we
	 * transition back to INIT state so we sit waiting for more
	 * OIDs to be issued and we don't start the IDLE timer.
	 */
	case eCsrSmeIssuedFTReassoc:
	case eCsrSmeIssuedAssocToSimilarAP:
	case eCsrHddIssued:
	case eCsrSmeIssuedDisassocForHandoff:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);
		roam_info->bss_desc = cmd->u.roamCmd.pLastRoamBss;
		roam_info->pProfile = &cmd->u.roamCmd.roamProfile;
		roam_info->status_code =
			session->joinFailStatusCode.status_code;
		roam_info->reasonCode = session->joinFailStatusCode.reasonCode;
		qdf_mem_copy(&roam_info->bssid,
			     &session->joinFailStatusCode.bssId,
			     sizeof(struct qdf_mac_addr));
		if (prev_connect_info->pbFrames) {
			roam_info->nAssocReqLength =
					prev_connect_info->nAssocReqLength;
			roam_info->nAssocRspLength =
					prev_connect_info->nAssocRspLength;
			roam_info->nBeaconLength =
				prev_connect_info->nBeaconLength;
			roam_info->pbFrames = prev_connect_info->pbFrames;
		}
		/*
		 * If Join fails while Handoff is in progress, indicate
		 * disassociated event to supplicant to reconnect
		 */
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)) {
			csr_neighbor_roam_indicate_connect(mac_ctx,
				(uint8_t)session_id, QDF_STATUS_E_FAILURE);
		}
		if (session->bRefAssocStartCnt > 0) {
			session->bRefAssocStartCnt--;
			if (eCsrJoinFailureDueToConcurrency == res)
				csr_roam_call_callback(mac_ctx, session_id,
						       roam_info,
						       cmd->u.roamCmd.roamId,
				       eCSR_ROAM_ASSOCIATION_COMPLETION,
				       eCSR_ROAM_RESULT_ASSOC_FAIL_CON_CHANNEL);
			else
				csr_roam_call_callback(mac_ctx, session_id,
						       roam_info,
						       cmd->u.roamCmd.roamId,
					       eCSR_ROAM_ASSOCIATION_COMPLETION,
					       eCSR_ROAM_RESULT_FAILURE);
		} else {
			/*
			 * bRefAssocStartCnt is not incremented when
			 * eRoamState == eCsrStopRoamingDueToConcurrency
			 * in csr_roam_join_next_bss API. so handle this in
			 * else case by sending assoc failure
			 */
			csr_roam_call_callback(mac_ctx, session_id,
					       roam_info,
					       cmd->u.roamCmd.roamId,
					       eCSR_ROAM_ASSOCIATION_FAILURE,
					       eCSR_ROAM_RESULT_FAILURE);
		}
		sme_debug("roam(reason %d) failed", cmd->u.roamCmd.roamReason);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_update_hand_off((uint8_t) session_id, false);
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
			SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
			eCSR_ROAM_RESULT_FAILURE, false);
		break;
	case eCsrHddIssuedReassocToSameAP:
	case eCsrSmeIssuedReassocToSameAP:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);

		csr_roam_call_callback(mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_DISASSOCIATED,
			eCSR_ROAM_RESULT_FORCED);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
			SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
			eCSR_ROAM_RESULT_FAILURE, false);
		break;
	case eCsrForcedDisassoc:
	case eCsrForcedDeauth:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);
		csr_roam_call_callback(
			mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_DISASSOCIATED,
			eCSR_ROAM_RESULT_FORCED);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
				SME_QOS_CSR_DISCONNECT_IND,
				NULL);
#endif
		csr_roam_link_down(mac_ctx, session_id);

		if (mac_ctx->roam.deauthRspStatus == eSIR_SME_DEAUTH_STATUS) {
			sme_warn("FW still in connected state");
			break;
		}
		break;
	case eCsrForcedDisassocMICFailure:
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
			session_id);

		csr_roam_call_callback(mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_DISASSOCIATED,
			eCSR_ROAM_RESULT_MIC_FAILURE);
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
			SME_QOS_CSR_DISCONNECT_REQ, NULL);
#endif
		break;
	case eCsrStopBss:
		csr_roam_call_callback(mac_ctx, session_id, NULL,
			cmd->u.roamCmd.roamId, eCSR_ROAM_INFRA_IND,
			eCSR_ROAM_RESULT_INFRA_STOPPED);
		break;
	case eCsrForcedDisassocSta:
	case eCsrForcedDeauthSta:
		roam_info->rssi = mac_ctx->peer_rssi;
		roam_info->tx_rate = mac_ctx->peer_txrate;
		roam_info->rx_rate = mac_ctx->peer_rxrate;
		roam_info->rx_mc_bc_cnt = mac_ctx->rx_mc_bc_cnt;
		roam_info->rx_retry_cnt = mac_ctx->rx_retry_cnt;

		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED,
			session_id);
		session = CSR_GET_SESSION(mac_ctx, session_id);
		if (CSR_IS_SESSION_VALID(mac_ctx, session_id) &&
			CSR_IS_INFRA_AP(&session->connectedProfile)) {
			roam_info->u.pConnectedProfile =
				&session->connectedProfile;
			qdf_mem_copy(roam_info->peerMac.bytes,
				     cmd->u.roamCmd.peerMac,
				     sizeof(tSirMacAddr));
			roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
			/* Update the MAC reason code */
			roam_info->disassoc_reason = cmd->u.roamCmd.reason;
			roam_info->status_code = eSIR_SME_SUCCESS;
			status = csr_roam_call_callback(mac_ctx, session_id,
							roam_info,
							cmd->u.roamCmd.roamId,
							eCSR_ROAM_LOSTLINK,
						       eCSR_ROAM_RESULT_FORCED);
		}
		break;
	default:
		csr_roam_state_change(mac_ctx,
			eCSR_ROAMING_STATE_IDLE, session_id);
		break;
	}
	qdf_mem_free(roam_info);
}

/**
 * csr_roam_process_start_bss_success() - Process the result for start bss
 * @mac_ctx:          Global MAC Context
 * @cmd:              Command to be processed
 * @context:          Additional data in context of the cmd
 *
 * Return: None
 */
static void csr_roam_process_start_bss_success(struct mac_context *mac_ctx,
		     tSmeCmd *cmd, void *context)
{
	uint32_t session_id = cmd->vdev_id;
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	struct csr_roam_session *session;
	struct bss_description *bss_desc = NULL;
	struct csr_roam_info *roam_info;
	struct start_bss_rsp *start_bss_rsp = NULL;
	eRoamCmdStatus roam_status = eCSR_ROAM_INFRA_IND;
	eCsrRoamResult roam_result = eCSR_ROAM_RESULT_INFRA_STARTED;
	tSirMacAddr bcast_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	QDF_STATUS status;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	struct ht_profile *src_profile = NULL;
	tCsrRoamHTProfile *dst_profile = NULL;
#endif

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);

	sme_debug("receives start BSS ok indication");
	status = QDF_STATUS_E_FAILURE;
	start_bss_rsp = (struct start_bss_rsp *) context;
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	if (CSR_IS_INFRA_AP(profile))
		session->connectState =
			eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED;
	else if (CSR_IS_NDI(profile))
		session->connectState = eCSR_CONNECT_STATE_TYPE_NDI_STARTED;
	else
		session->connectState = eCSR_ASSOC_STATE_TYPE_WDS_DISCONNECTED;

	bss_desc = &start_bss_rsp->bssDescription;
	if (CSR_IS_NDI(profile)) {
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED,
			session_id);
		csr_roam_save_ndi_connected_info(mac_ctx, session_id, profile,
						bss_desc);
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		qdf_mem_copy(&roam_info->bssid, &bss_desc->bssId,
			     sizeof(struct qdf_mac_addr));
	} else {
		csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_JOINED,
				session_id);
	}

	csr_roam_save_connected_bss_desc(mac_ctx, session_id, bss_desc);
	csr_roam_free_connect_profile(&session->connectedProfile);
	csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
	csr_roam_save_connected_information(mac_ctx, session_id,
			profile, bss_desc, NULL);
	qdf_mem_copy(&roam_info->bssid, &bss_desc->bssId,
		     sizeof(struct qdf_mac_addr));
	/* We are done with the IEs so free it */
	/*
	 * Only set context for non-WDS_STA. We don't even need it for
	 * WDS_AP. But since the encryption.
	 * is WPA2-PSK so it won't matter.
	 */
	if (session->pCurRoamProfile &&
	    !CSR_IS_INFRA_AP(session->pCurRoamProfile)) {
		if (CSR_IS_ENC_TYPE_STATIC(
				profile->negotiatedUCEncryptionType)) {
			/* NO keys. these key parameters don't matter */
			csr_issue_set_context_req_helper(mac_ctx,
						profile, session_id,
						&bcast_mac, false,
						false, eSIR_TX_RX,
						0, 0, NULL);
		}
	}
	/*
	 * Only tell upper layer is we start the BSS because Vista doesn't like
	 * multiple connection indications. If we don't start the BSS ourself,
	 * handler of eSIR_SME_JOINED_NEW_BSS will trigger the connection start
	 * indication in Vista
	 */
	roam_info->staId = (uint8_t)start_bss_rsp->staId;
	if (CSR_IS_NDI(profile)) {
		csr_roam_update_ndp_return_params(mac_ctx,
						  eCsrStartBssSuccess,
						  &roam_status,
						  &roam_result,
						  roam_info);
	}
	/*
	 * Only tell upper layer is we start the BSS because Vista
	 * doesn't like multiple connection indications. If we don't
	 * start the BSS ourself, handler of eSIR_SME_JOINED_NEW_BSS
	 * will trigger the connection start indication in Vista
	 */
	roam_info->status_code =
			session->joinFailStatusCode.status_code;
	roam_info->reasonCode = session->joinFailStatusCode.reasonCode;
	roam_info->bss_desc = bss_desc;
	if (bss_desc)
		qdf_mem_copy(roam_info->bssid.bytes, bss_desc->bssId,
			     sizeof(struct qdf_mac_addr));
	if (!IS_FEATURE_SUPPORTED_BY_FW(SLM_SESSIONIZATION) &&
			(csr_is_concurrent_session_running(mac_ctx))) {
		mac_ctx->roam.configParam.doBMPSWorkaround = 1;
	}
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	dst_profile = &session->connectedProfile.ht_profile;
	src_profile = &start_bss_rsp->ht_profile;
	if (mac_ctx->roam.configParam.cc_switch_mode
			!= QDF_MCC_TO_SCC_SWITCH_DISABLE)
		csr_roam_copy_ht_profile(dst_profile, src_profile);
#endif
	csr_roam_call_callback(mac_ctx, session_id, roam_info,
			       cmd->u.roamCmd.roamId,
			       roam_status, roam_result);
	qdf_mem_free(roam_info);
}

#ifdef WLAN_FEATURE_FILS_SK
/**
 * populate_fils_params_join_rsp() - Copy FILS params from JOIN rsp
 * @mac_ctx: Global MAC Context
 * @roam_info: CSR Roam Info
 * @join_rsp: SME Join response
 *
 * Copy the FILS params from the join results
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS populate_fils_params_join_rsp(struct mac_context *mac_ctx,
						struct csr_roam_info *roam_info,
						struct join_rsp *join_rsp)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct fils_join_rsp_params *roam_fils_info,
				    *fils_join_rsp = join_rsp->fils_join_rsp;

	if (!fils_join_rsp->fils_pmk_len ||
			!fils_join_rsp->fils_pmk || !fils_join_rsp->tk_len ||
			!fils_join_rsp->kek_len || !fils_join_rsp->gtk_len) {
		sme_err("fils join rsp err: pmk len %d tk len %d kek len %d gtk len %d",
			fils_join_rsp->fils_pmk_len,
			fils_join_rsp->tk_len,
			fils_join_rsp->kek_len,
			fils_join_rsp->gtk_len);
		status = QDF_STATUS_E_FAILURE;
		goto free_fils_join_rsp;
	}

	roam_info->fils_join_rsp = qdf_mem_malloc(sizeof(*fils_join_rsp));
	if (!roam_info->fils_join_rsp) {
		status = QDF_STATUS_E_FAILURE;
		goto free_fils_join_rsp;
	}

	roam_fils_info = roam_info->fils_join_rsp;
	roam_fils_info->fils_pmk = qdf_mem_malloc(fils_join_rsp->fils_pmk_len);
	if (!roam_fils_info->fils_pmk) {
		qdf_mem_free(roam_info->fils_join_rsp);
		roam_info->fils_join_rsp = NULL;
		status = QDF_STATUS_E_FAILURE;
		goto free_fils_join_rsp;
	}

	roam_info->fils_seq_num = join_rsp->fils_seq_num;
	roam_fils_info->fils_pmk_len = fils_join_rsp->fils_pmk_len;
	qdf_mem_copy(roam_fils_info->fils_pmk,
		     fils_join_rsp->fils_pmk, roam_fils_info->fils_pmk_len);

	qdf_mem_copy(roam_fils_info->fils_pmkid,
		     fils_join_rsp->fils_pmkid, PMKID_LEN);

	roam_fils_info->kek_len = fils_join_rsp->kek_len;
	qdf_mem_copy(roam_fils_info->kek,
		     fils_join_rsp->kek, roam_fils_info->kek_len);

	roam_fils_info->tk_len = fils_join_rsp->tk_len;
	qdf_mem_copy(roam_fils_info->tk,
		     fils_join_rsp->tk, fils_join_rsp->tk_len);

	roam_fils_info->gtk_len = fils_join_rsp->gtk_len;
	qdf_mem_copy(roam_fils_info->gtk,
		     fils_join_rsp->gtk, roam_fils_info->gtk_len);

	cds_copy_hlp_info(&fils_join_rsp->dst_mac, &fils_join_rsp->src_mac,
			  fils_join_rsp->hlp_data_len, fils_join_rsp->hlp_data,
			  &roam_fils_info->dst_mac, &roam_fils_info->src_mac,
			  &roam_fils_info->hlp_data_len,
			  roam_fils_info->hlp_data);
	sme_debug("FILS connect params copied to CSR!");

free_fils_join_rsp:
	qdf_mem_free(fils_join_rsp->fils_pmk);
	qdf_mem_free(fils_join_rsp);
	return status;
}

/**
 * csr_process_fils_join_rsp() - Process join rsp for FILS connection
 * @mac_ctx: Global MAC Context
 * @profile: CSR Roam Profile
 * @session_id: Session ID
 * @roam_info: CSR Roam Info
 * @bss_desc: BSS description
 * @join_rsp: SME Join rsp
 *
 * Process SME join response for FILS connection
 *
 * Return: None
 */
static void csr_process_fils_join_rsp(struct mac_context *mac_ctx,
					struct csr_roam_profile *profile,
					uint32_t session_id,
					struct csr_roam_info *roam_info,
					struct bss_description *bss_desc,
					struct join_rsp *join_rsp)
{
	QDF_STATUS status;

	if (!join_rsp || !join_rsp->fils_join_rsp) {
		sme_err("Join rsp doesn't have FILS info");
		goto process_fils_join_rsp_fail;
	}

	/* Copy FILS params */
	status = populate_fils_params_join_rsp(mac_ctx, roam_info, join_rsp);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Copy FILS params join rsp fails");
		goto process_fils_join_rsp_fail;
	}

	status = csr_issue_set_context_req_helper(mac_ctx, profile,
				session_id, &bss_desc->bssId, true,
				true, eSIR_TX_RX, 0,
				roam_info->fils_join_rsp->tk_len,
				roam_info->fils_join_rsp->tk);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Set context for unicast fail");
		goto process_fils_join_rsp_fail;
	}

	status = csr_issue_set_context_req_helper(mac_ctx, profile,
			session_id, &bss_desc->bssId, true, false,
			eSIR_RX_ONLY, 2, roam_info->fils_join_rsp->gtk_len,
			roam_info->fils_join_rsp->gtk);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Set context for bcast fail");
		goto process_fils_join_rsp_fail;
	}
	return;

process_fils_join_rsp_fail:
	csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE, session_id);
}
#else

static inline void csr_process_fils_join_rsp(struct mac_context *mac_ctx,
					     struct csr_roam_profile *profile,
					     uint32_t session_id,
					     struct csr_roam_info *roam_info,
					     struct bss_description *bss_desc,
					     struct join_rsp *join_rsp)
{}
#endif

#ifdef WLAN_FEATURE_11AX
static void csr_roam_process_he_info(struct join_rsp *sme_join_rsp,
				     struct csr_roam_info *roam_info)
{
	roam_info->he_operation = sme_join_rsp->he_operation;
}
#else
static inline void csr_roam_process_he_info(struct join_rsp *sme_join_rsp,
					    struct csr_roam_info *roam_info)
{
}
#endif

static void csr_update_rsn_intersect_to_fw(struct mac_context *mac_ctx,
					   uint8_t vdev_id,
					   struct bss_description *bss_desc)
{
	int32_t rsn_val;
	struct wlan_objmgr_vdev *vdev;
	const uint8_t *ie = NULL;
	uint16_t ie_len;
	tDot11fIERSN rsn_ie = {0};

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);

	if (!vdev) {
		sme_err("Invalid vdev obj for vdev id %d", vdev_id);
		return;
	}

	rsn_val = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_RSN_CAP);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	if (rsn_val < 0) {
		sme_err("Invalid mgmt cipher");
		return;
	}

	if (bss_desc) {
		ie_len = csr_get_ielen_from_bss_description(bss_desc);

		ie = wlan_get_ie_ptr_from_eid(WLAN_EID_RSN,
					      (uint8_t *)bss_desc->ieFields,
					      ie_len);

		if (ie && ie[0] == WLAN_EID_RSN &&
		    !dot11f_unpack_ie_rsn(mac_ctx, (uint8_t *)ie + 2, ie[1],
					  &rsn_ie, false)) {
			if (!(*(uint16_t *)&rsn_ie.RSN_Cap &
			    WLAN_CRYPTO_RSN_CAP_OCV_SUPPORTED))
				rsn_val &= ~WLAN_CRYPTO_RSN_CAP_OCV_SUPPORTED;
		}
	}

	if (wma_cli_set2_command(vdev_id, WMI_VDEV_PARAM_RSN_CAPABILITY,
				 rsn_val, 0, VDEV_CMD))
		sme_err("Failed to update WMI_VDEV_PARAM_RSN_CAPABILITY for vdev id %d",
			vdev_id);
}

/**
 * csr_roam_process_join_res() - Process the Join results
 * @mac_ctx:          Global MAC Context
 * @result:           Result after the command was processed
 * @cmd:              Command to be processed
 * @context:          Additional data in context of the cmd
 *
 * Process the join results which are obtained in a successful join
 *
 * Return: None
 */
static void csr_roam_process_join_res(struct mac_context *mac_ctx,
	enum csr_roamcomplete_result res, tSmeCmd *cmd, void *context)
{
	tSirMacAddr bcast_mac = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff };
	sme_QosAssocInfo assoc_info;
	uint32_t key_timeout_interval = 0;
	uint8_t acm_mask = 0;   /* HDD needs ACM mask in assoc rsp callback */
	uint32_t session_id = cmd->vdev_id;
	struct csr_roam_profile *profile = &cmd->u.roamCmd.roamProfile;
	struct csr_roam_session *session;
	struct bss_description *bss_desc = NULL;
	struct tag_csrscan_result *scan_res = NULL;
	sme_qos_csr_event_indType ind_qos;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	struct ht_profile *src_profile = NULL;
	tCsrRoamHTProfile *dst_profile = NULL;
#endif
	tCsrRoamConnectedProfile *conn_profile = NULL;
	tDot11fBeaconIEs *ies_ptr = NULL;
	struct csr_roam_info *roam_info;
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct join_rsp *join_rsp = context;
	uint32_t len;
	enum csr_akm_type akm_type;
	uint8_t mdie_present;

	if (!join_rsp) {
		sme_err("join_rsp is NULL");
		return;
	}

	if (!CSR_IS_SESSION_VALID(mac_ctx, session_id)) {
		sme_err("Invalid session id %d", session_id);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);

	conn_profile = &session->connectedProfile;
	sme_debug("receives association indication");

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	if (eCsrReassocSuccess == res) {
		roam_info->reassoc = true;
		ind_qos = SME_QOS_CSR_REASSOC_COMPLETE;
	} else {
		roam_info->reassoc = false;
		ind_qos = SME_QOS_CSR_ASSOC_COMPLETE;
	}

	/*
	 * Reset remain_in_power_active_till_dhcp as
	 * it might have been set by last failed secured connection.
	 * It should be set only for secured connection.
	 */
	ps_global_info->remain_in_power_active_till_dhcp = false;
	if (CSR_IS_INFRASTRUCTURE(profile))
		session->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED;
	else
		session->connectState = eCSR_ASSOC_STATE_TYPE_WDS_CONNECTED;
	/*
	 * Use the last connected bssdesc for reassoc-ing to the same AP.
	 * NOTE: What to do when reassoc to a different AP???
	 */
	if ((eCsrHddIssuedReassocToSameAP == cmd->u.roamCmd.roamReason)
		|| (eCsrSmeIssuedReassocToSameAP ==
			cmd->u.roamCmd.roamReason)) {
		bss_desc = session->pConnectBssDesc;
		if (bss_desc)
			qdf_mem_copy(&roam_info->bssid, &bss_desc->bssId,
				     sizeof(struct qdf_mac_addr));
	} else {
		if (cmd->u.roamCmd.pRoamBssEntry) {
			scan_res = GET_BASE_ADDR(cmd->u.roamCmd.pRoamBssEntry,
					struct tag_csrscan_result, Link);
			if (scan_res) {
				bss_desc = &scan_res->Result.BssDescriptor;
				ies_ptr = (tDot11fBeaconIEs *)
					(scan_res->Result.pvIes);
				qdf_mem_copy(&roam_info->bssid,
					     &bss_desc->bssId,
					     sizeof(struct qdf_mac_addr));
			}
		}
	}
	if (bss_desc) {
		roam_info->staId = STA_INVALID_IDX;
		csr_roam_save_connected_information(mac_ctx, session_id,
			profile, bss_desc, ies_ptr);
#ifdef FEATURE_WLAN_ESE
		roam_info->isESEAssoc = conn_profile->isESEAssoc;
#endif

		/*
		 * csr_roam_state_change also affects sub-state.
		 * Hence, csr_roam_state_change happens first and then
		 * substate change.
		 * Moving even save profile above so that below
		 * mentioned conditon is also met.
		 * JEZ100225: Moved to after saving the profile.
		 * Fix needed in main/latest
		 */
		csr_roam_state_change(mac_ctx,
			eCSR_ROAMING_STATE_JOINED, session_id);

		/*
		 * Make sure the Set Context is issued before link
		 * indication to NDIS.  After link indication is
		 * made to NDIS, frames could start flowing.
		 * If we have not set context with LIM, the frames
		 * will be dropped for the security context may not
		 * be set properly.
		 *
		 * this was causing issues in the 2c_wlan_wep WHQL test
		 * when the SetContext was issued after the link
		 * indication. (Link Indication happens in the
		 * profFSMSetConnectedInfra call).
		 *
		 * this reordering was done on titan_prod_usb branch
		 * and is being replicated here.
		 */

		if (CSR_IS_ENC_TYPE_STATIC
			(profile->negotiatedUCEncryptionType) &&
			!profile->bWPSAssociation) {
			/*
			 * Issue the set Context request to LIM to establish
			 * the Unicast STA context
			 */
			if (QDF_IS_STATUS_ERROR(
				csr_issue_set_context_req_helper(mac_ctx,
					profile, session_id, &bss_desc->bssId,
					false, true, eSIR_TX_RX, 0, 0, NULL))) {
				/* NO keys. these key parameters don't matter */
				sme_debug("Set context for unicast fail");
				csr_roam_substate_change(mac_ctx,
					eCSR_ROAM_SUBSTATE_NONE, session_id);
			}
			/*
			 * Issue the set Context request to LIM
			 * to establish the Broadcast STA context
			 * NO keys. these key parameters don't matter
			 */
			csr_issue_set_context_req_helper(mac_ctx, profile,
							 session_id, &bcast_mac,
							 false, false,
							 eSIR_TX_RX, 0, 0,
							 NULL);
		} else if (CSR_IS_AUTH_TYPE_FILS(profile->negotiatedAuthType)
				&& join_rsp->is_fils_connection) {
			roam_info->is_fils_connection = true;
			csr_process_fils_join_rsp(mac_ctx, profile, session_id,
						  roam_info, bss_desc,
						  join_rsp);
		} else {
			/* Need to wait for supplicant authtication */
			roam_info->fAuthRequired = true;
			/*
			 * Set the substate to WaitForKey in case
			 * authentiation is needed
			 */
			csr_roam_substate_change(mac_ctx,
					eCSR_ROAM_SUBSTATE_WAIT_FOR_KEY,
					session_id);

			/*
			 * Set remain_in_power_active_till_dhcp to make
			 * sure we wait for until keys are set before
			 * going into BMPS.
			 */
			ps_global_info->remain_in_power_active_till_dhcp
				= true;

			if (profile->bWPSAssociation)
				key_timeout_interval =
					CSR_WAIT_FOR_WPS_KEY_TIMEOUT_PERIOD;
			else
				key_timeout_interval =
					CSR_WAIT_FOR_KEY_TIMEOUT_PERIOD;

			/* Save session_id in case of timeout */
			mac_ctx->roam.WaitForKeyTimerInfo.vdev_id =
				(uint8_t) session_id;
			/*
			 * This time should be long enough for the rest
			 * of the process plus setting key
			 */
			if (!QDF_IS_STATUS_SUCCESS
					(csr_roam_start_wait_for_key_timer(
					   mac_ctx, key_timeout_interval))
			   ) {
				/* Reset state so nothing is blocked. */
				sme_err("Failed preauth timer start");
				csr_roam_substate_change(mac_ctx,
						eCSR_ROAM_SUBSTATE_NONE,
						session_id);
			}
		}

		assoc_info.bss_desc = bss_desc;       /* could be NULL */
		assoc_info.pProfile = profile;
		if (context) {
			if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac_ctx->psoc,
							   session_id))
				sme_debug("LFR3: Clear Connected info");

			csr_roam_free_connected_info(mac_ctx,
						     &session->connectedInfo);

			akm_type = session->connectedProfile.AuthType;
			mdie_present =
				session->connectedProfile.mdid.mdie_present;
			if (akm_type == eCSR_AUTH_TYPE_FT_SAE && mdie_present) {
				sme_debug("Update the MDID in PMK cache for FT-SAE case");
				csr_update_pmk_cache_ft(mac_ctx, session_id,
							NULL, NULL);
			}

			len = join_rsp->assocReqLength +
				join_rsp->assocRspLength +
				join_rsp->beaconLength;
			len += join_rsp->parsedRicRspLen;
#ifdef FEATURE_WLAN_ESE
			len += join_rsp->tspecIeLen;
#endif
			if (len) {
				session->connectedInfo.pbFrames =
					qdf_mem_malloc(len);
				if (session->connectedInfo.pbFrames !=
						NULL) {
					qdf_mem_copy(
						session->connectedInfo.pbFrames,
						join_rsp->frames, len);
					session->connectedInfo.nAssocReqLength =
						join_rsp->assocReqLength;
					session->connectedInfo.nAssocRspLength =
						join_rsp->assocRspLength;
					session->connectedInfo.nBeaconLength =
						join_rsp->beaconLength;
					session->connectedInfo.nRICRspLength =
						join_rsp->parsedRicRspLen;
#ifdef FEATURE_WLAN_ESE
					session->connectedInfo.nTspecIeLength =
						join_rsp->tspecIeLen;
#endif
					roam_info->nAssocReqLength =
						join_rsp->assocReqLength;
					roam_info->nAssocRspLength =
						join_rsp->assocRspLength;
					roam_info->nBeaconLength =
						join_rsp->beaconLength;
					roam_info->pbFrames =
						session->connectedInfo.pbFrames;
				}
			}
			if (cmd->u.roamCmd.fReassoc)
				roam_info->fReassocReq =
					roam_info->fReassocRsp = true;
			conn_profile->vht_channel_width =
				join_rsp->vht_channel_width;
			session->connectedInfo.staId =
				(uint8_t)join_rsp->staId;
			roam_info->staId = (uint8_t)join_rsp->staId;
			roam_info->timingMeasCap = join_rsp->timingMeasCap;
			roam_info->chan_info.nss = join_rsp->nss;
			roam_info->chan_info.rate_flags =
				join_rsp->max_rate_flags;
			roam_info->chan_info.ch_width =
				join_rsp->vht_channel_width;
#ifdef FEATURE_WLAN_TDLS
			roam_info->tdls_prohibited = join_rsp->tdls_prohibited;
			roam_info->tdls_chan_swit_prohibited =
				join_rsp->tdls_chan_swit_prohibited;
			sme_debug("tdls:prohibit: %d chan_swit_prohibit: %d",
				roam_info->tdls_prohibited,
				roam_info->tdls_chan_swit_prohibited);
#endif
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
			src_profile = &join_rsp->ht_profile;
			dst_profile = &conn_profile->ht_profile;
			if (mac_ctx->roam.configParam.cc_switch_mode
				!= QDF_MCC_TO_SCC_SWITCH_DISABLE)
				csr_roam_copy_ht_profile(dst_profile,
						src_profile);
#endif
			roam_info->vht_caps = join_rsp->vht_caps;
			roam_info->ht_caps = join_rsp->ht_caps;
			roam_info->hs20vendor_ie = join_rsp->hs20vendor_ie;
			roam_info->ht_operation = join_rsp->ht_operation;
			roam_info->vht_operation = join_rsp->vht_operation;
			csr_roam_process_he_info(join_rsp, roam_info);
		} else {
			if (cmd->u.roamCmd.fReassoc) {
				roam_info->fReassocReq =
					roam_info->fReassocRsp = true;
				roam_info->nAssocReqLength =
					session->connectedInfo.nAssocReqLength;
				roam_info->nAssocRspLength =
					session->connectedInfo.nAssocRspLength;
				roam_info->nBeaconLength =
					session->connectedInfo.nBeaconLength;
				roam_info->pbFrames =
					session->connectedInfo.pbFrames;
			}
		}
		/*
		 * Update the staId from the previous connected profile info
		 * as the reassociation is triggred at SME/HDD
		 */

		if ((eCsrHddIssuedReassocToSameAP ==
				cmd->u.roamCmd.roamReason) ||
			(eCsrSmeIssuedReassocToSameAP ==
				cmd->u.roamCmd.roamReason))
			roam_info->staId = session->connectedInfo.staId;

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		/*
		 * Indicate SME-QOS with reassoc success event,
		 * only after copying the frames
		 */
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id, ind_qos,
				&assoc_info);
#endif
		roam_info->bss_desc = bss_desc;
		roam_info->status_code =
			session->joinFailStatusCode.status_code;
		roam_info->reasonCode =
			session->joinFailStatusCode.reasonCode;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		acm_mask = sme_qos_get_acm_mask(mac_ctx, bss_desc, NULL);
#endif
		conn_profile->acm_mask = acm_mask;
		conn_profile->modifyProfileFields.uapsd_mask =
						join_rsp->uapsd_mask;
		/*
		 * start UAPSD if uapsd_mask is not 0 because HDD will
		 * configure for trigger frame It may be better to let QoS do
		 * this????
		 */
		if (conn_profile->modifyProfileFields.uapsd_mask) {
			sme_err(
				" uapsd_mask (0x%X) set, request UAPSD now",
				conn_profile->modifyProfileFields.uapsd_mask);
			sme_ps_start_uapsd(MAC_HANDLE(mac_ctx), session_id);
		}
		conn_profile->dot11Mode = session->bssParams.uCfgDot11Mode;
		roam_info->u.pConnectedProfile = conn_profile;

		if (session->bRefAssocStartCnt > 0) {
			session->bRefAssocStartCnt--;
			if (!IS_FEATURE_SUPPORTED_BY_FW
				(SLM_SESSIONIZATION) &&
				(csr_is_concurrent_session_running(mac_ctx))) {
				mac_ctx->roam.configParam.doBMPSWorkaround = 1;
			}
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
					       cmd->u.roamCmd.roamId,
					       eCSR_ROAM_ASSOCIATION_COMPLETION,
					       eCSR_ROAM_RESULT_ASSOCIATED);
		}

		csr_update_scan_entry_associnfo(mac_ctx, session,
						SCAN_ENTRY_CON_STATE_ASSOC);
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
				eCSR_ROAM_RESULT_NONE, true);
		csr_reset_pmkid_candidate_list(mac_ctx, session_id);
	} else {
		sme_warn("Roam command doesn't have a BSS desc");
	}

	if (csr_roam_is_sta_mode(mac_ctx, session_id))
		csr_post_roam_state_change(mac_ctx, session_id, WLAN_ROAM_INIT,
					   REASON_CONNECT);

	/* Not to signal link up because keys are yet to be set.
	 * The linkup function will overwrite the sub-state that
	 * we need to keep at this point.
	 */
	if (!CSR_IS_WAIT_FOR_KEY(mac_ctx, session_id)) {
		if (MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac_ctx->psoc, session_id))
			sme_debug("LFR3: NO WAIT_FOR_KEY do csr_roam_link_up");

		csr_roam_link_up(mac_ctx, conn_profile->bssid);
	}

	csr_update_rsn_intersect_to_fw(mac_ctx, session_id, bss_desc);
	sme_free_join_rsp_fils_params(roam_info);
	qdf_mem_free(roam_info);
}

/**
 * csr_roam_process_results() - Process the Roam Results
 * @mac_ctx:      Global MAC Context
 * @cmd:          Command that has been processed
 * @res:          Results available after processing the command
 * @context:      Context
 *
 * Process the available results and make an appropriate decision
 *
 * Return: true if the command can be released, else not.
 */
static bool csr_roam_process_results(struct mac_context *mac_ctx, tSmeCmd *cmd,
				     enum csr_roamcomplete_result res,
					void *context)
{
	bool release_cmd = true;
	struct bss_description *bss_desc = NULL;
	struct csr_roam_info *roam_info;
	uint32_t session_id = cmd->vdev_id;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	struct csr_roam_profile *profile;
	eRoamCmdStatus roam_status = eCSR_ROAM_INFRA_IND;
	eCsrRoamResult roam_result = eCSR_ROAM_RESULT_INFRA_START_FAILED;
	struct start_bss_rsp  *start_bss_rsp = NULL;

	profile = &cmd->u.roamCmd.roamProfile;
	if (!session) {
		sme_err("session %d not found ", session_id);
		return false;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return false;

	sme_debug("Result %d", res);
	switch (res) {
	case eCsrJoinSuccess:
	case eCsrReassocSuccess:
		csr_roam_process_join_res(mac_ctx, res, cmd, context);
		break;
	case eCsrStartBssSuccess:
		csr_roam_process_start_bss_success(mac_ctx, cmd, context);
		break;
	case eCsrStartBssFailure:
		start_bss_rsp = (struct start_bss_rsp *)context;

		if (CSR_IS_NDI(profile)) {
			csr_roam_update_ndp_return_params(mac_ctx,
							  eCsrStartBssFailure,
							  &roam_status,
							  &roam_result,
							  roam_info);
		}

		if (context)
			bss_desc = (struct bss_description *) context;
		else
			bss_desc = NULL;
		roam_info->bss_desc = bss_desc;
		csr_roam_call_callback(mac_ctx, session_id, roam_info,
				       cmd->u.roamCmd.roamId, roam_status,
				       roam_result);
		csr_set_default_dot11_mode(mac_ctx);
		break;
	case eCsrSilentlyStopRoamingSaveState:
		/* We are here because we try to connect to the same AP */
		/* No message to PE */
		sme_debug("receives silently stop roaming indication");
		qdf_mem_zero(roam_info, sizeof(*roam_info));

		/* to aviod resetting the substate to NONE */
		mac_ctx->roam.curState[session_id] = eCSR_ROAMING_STATE_JOINED;
		/*
		 * No need to change substate to wai_for_key because there
		 * is no state change
		 */
		roam_info->bss_desc = session->pConnectBssDesc;
		if (roam_info->bss_desc)
			qdf_mem_copy(&roam_info->bssid,
				     &roam_info->bss_desc->bssId,
				     sizeof(struct qdf_mac_addr));
		roam_info->status_code =
			session->joinFailStatusCode.status_code;
		roam_info->reasonCode = session->joinFailStatusCode.reasonCode;
		roam_info->nBeaconLength = session->connectedInfo.nBeaconLength;
		roam_info->nAssocReqLength =
			session->connectedInfo.nAssocReqLength;
		roam_info->nAssocRspLength =
			session->connectedInfo.nAssocRspLength;
		roam_info->pbFrames = session->connectedInfo.pbFrames;
		roam_info->staId = session->connectedInfo.staId;
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		if (0 == roam_info->staId)
			QDF_ASSERT(0);

		session->bRefAssocStartCnt--;
		csr_roam_call_callback(mac_ctx, session_id, roam_info,
				       cmd->u.roamCmd.roamId,
				       eCSR_ROAM_ASSOCIATION_COMPLETION,
				       eCSR_ROAM_RESULT_ASSOCIATED);
		csr_roam_completion(mac_ctx, session_id, NULL, cmd,
				eCSR_ROAM_RESULT_ASSOCIATED, true);
		break;
	case eCsrReassocFailure:
		/*
		 * Currently Reassoc failure is handled through eCsrJoinFailure
		 * Need to revisit for eCsrReassocFailure handling
		 */
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		sme_qos_csr_event_ind(mac_ctx, (uint8_t) session_id,
				SME_QOS_CSR_REASSOC_FAILURE, NULL);
#endif
		break;
	case eCsrStopBssSuccess:
		if (CSR_IS_NDI(profile)) {
			qdf_mem_zero(roam_info, sizeof(*roam_info));
			csr_roam_update_ndp_return_params(mac_ctx, res,
							  &roam_status,
							  &roam_result,
							  roam_info);
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
					       cmd->u.roamCmd.roamId,
					       roam_status, roam_result);
		}
		break;
	case eCsrStopBssFailure:
		if (CSR_IS_NDI(profile)) {
			qdf_mem_zero(roam_info, sizeof(*roam_info));
			csr_roam_update_ndp_return_params(mac_ctx, res,
							  &roam_status,
							  &roam_result,
							  roam_info);
			csr_roam_call_callback(mac_ctx, session_id, roam_info,
					       cmd->u.roamCmd.roamId,
					       roam_status, roam_result);
		}
		break;
	case eCsrJoinFailure:
	case eCsrNothingToJoin:
	case eCsrJoinFailureDueToConcurrency:
	default:
		csr_roam_process_results_default(mac_ctx, cmd, context, res);
		break;
	}
	qdf_mem_free(roam_info);
	return release_cmd;
}

#ifdef WLAN_FEATURE_FILS_SK
/*
 * update_profile_fils_info: API to update FILS info from
 * source profile to destination profile.
 * @des_profile: pointer to destination profile
 * @src_profile: pointer to souce profile
 *
 * Return: None
 */
static void update_profile_fils_info(struct mac_context *mac,
				     struct csr_roam_profile *des_profile,
				     struct csr_roam_profile *src_profile,
				     uint8_t vdev_id)
{
	if (!src_profile || !src_profile->fils_con_info)
		return;

	sme_debug("is fils %d", src_profile->fils_con_info->is_fils_connection);

	if (!src_profile->fils_con_info->is_fils_connection)
		return;

	if (des_profile->fils_con_info)
		qdf_mem_free(des_profile->fils_con_info);

	des_profile->fils_con_info =
		qdf_mem_malloc(sizeof(struct wlan_fils_connection_info));
	if (!des_profile->fils_con_info)
		return;

	qdf_mem_copy(des_profile->fils_con_info,
		     src_profile->fils_con_info,
		     sizeof(struct wlan_fils_connection_info));

	wlan_cm_update_mlme_fils_connection_info(mac->psoc,
						 des_profile->fils_con_info,
						 vdev_id);

	if (src_profile->hlp_ie_len) {
		if (des_profile->hlp_ie)
			qdf_mem_free(des_profile->hlp_ie);

		des_profile->hlp_ie =
			qdf_mem_malloc(src_profile->hlp_ie_len);
		if (!des_profile->hlp_ie) {
			qdf_mem_free(des_profile->fils_con_info);
			des_profile->fils_con_info = NULL;
			return;
		}
		qdf_mem_copy(des_profile->hlp_ie, src_profile->hlp_ie,
			     src_profile->hlp_ie_len);
		des_profile->hlp_ie_len = src_profile->hlp_ie_len;
	}
}
#else
static inline
void update_profile_fils_info(struct mac_context *mac,
			      struct csr_roam_profile *des_profile,
			      struct csr_roam_profile *src_profile,
			      uint8_t vdev_id)
{}
#endif

QDF_STATUS csr_roam_copy_profile(struct mac_context *mac,
				 struct csr_roam_profile *pDstProfile,
				 struct csr_roam_profile *pSrcProfile,
				 uint8_t vdev_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint32_t size = 0;

	qdf_mem_zero(pDstProfile, sizeof(struct csr_roam_profile));
	if (pSrcProfile->BSSIDs.numOfBSSIDs) {
		size = sizeof(struct qdf_mac_addr) * pSrcProfile->BSSIDs.
								numOfBSSIDs;
		pDstProfile->BSSIDs.bssid = qdf_mem_malloc(size);
		if (!pDstProfile->BSSIDs.bssid) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->BSSIDs.numOfBSSIDs =
			pSrcProfile->BSSIDs.numOfBSSIDs;
		qdf_mem_copy(pDstProfile->BSSIDs.bssid,
			pSrcProfile->BSSIDs.bssid, size);
	}
	if (pSrcProfile->SSIDs.numOfSSIDs) {
		size = sizeof(tCsrSSIDInfo) * pSrcProfile->SSIDs.numOfSSIDs;
		pDstProfile->SSIDs.SSIDList = qdf_mem_malloc(size);
		if (!pDstProfile->SSIDs.SSIDList) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->SSIDs.numOfSSIDs =
			pSrcProfile->SSIDs.numOfSSIDs;
		qdf_mem_copy(pDstProfile->SSIDs.SSIDList,
			pSrcProfile->SSIDs.SSIDList, size);
	}
	if (pSrcProfile->nWPAReqIELength) {
		pDstProfile->pWPAReqIE =
			qdf_mem_malloc(pSrcProfile->nWPAReqIELength);
		if (!pDstProfile->pWPAReqIE) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nWPAReqIELength =
			pSrcProfile->nWPAReqIELength;
		qdf_mem_copy(pDstProfile->pWPAReqIE, pSrcProfile->pWPAReqIE,
			pSrcProfile->nWPAReqIELength);
	}
	if (pSrcProfile->nRSNReqIELength) {
		pDstProfile->pRSNReqIE =
			qdf_mem_malloc(pSrcProfile->nRSNReqIELength);
		if (!pDstProfile->pRSNReqIE) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nRSNReqIELength =
			pSrcProfile->nRSNReqIELength;
		qdf_mem_copy(pDstProfile->pRSNReqIE, pSrcProfile->pRSNReqIE,
			pSrcProfile->nRSNReqIELength);
	}
#ifdef FEATURE_WLAN_WAPI
	if (pSrcProfile->nWAPIReqIELength) {
		pDstProfile->pWAPIReqIE =
			qdf_mem_malloc(pSrcProfile->nWAPIReqIELength);
		if (!pDstProfile->pWAPIReqIE) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nWAPIReqIELength =
			pSrcProfile->nWAPIReqIELength;
		qdf_mem_copy(pDstProfile->pWAPIReqIE, pSrcProfile->pWAPIReqIE,
			pSrcProfile->nWAPIReqIELength);
	}
#endif /* FEATURE_WLAN_WAPI */
	if (pSrcProfile->nAddIEScanLength && pSrcProfile->pAddIEScan) {
		pDstProfile->pAddIEScan =
			qdf_mem_malloc(pSrcProfile->nAddIEScanLength);
		if (!pDstProfile->pAddIEScan) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nAddIEScanLength =
			pSrcProfile->nAddIEScanLength;
		qdf_mem_copy(pDstProfile->pAddIEScan, pSrcProfile->pAddIEScan,
			pSrcProfile->nAddIEScanLength);
	}
	if (pSrcProfile->nAddIEAssocLength) {
		pDstProfile->pAddIEAssoc =
			qdf_mem_malloc(pSrcProfile->nAddIEAssocLength);
		if (!pDstProfile->pAddIEAssoc) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nAddIEAssocLength =
			pSrcProfile->nAddIEAssocLength;
		qdf_mem_copy(pDstProfile->pAddIEAssoc, pSrcProfile->pAddIEAssoc,
			pSrcProfile->nAddIEAssocLength);
	}
	if (pSrcProfile->ChannelInfo.freq_list) {
		pDstProfile->ChannelInfo.freq_list =
			qdf_mem_malloc(sizeof(uint32_t) *
				       pSrcProfile->ChannelInfo.numOfChannels);
		if (!pDstProfile->ChannelInfo.freq_list) {
			pDstProfile->ChannelInfo.numOfChannels = 0;
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->ChannelInfo.numOfChannels =
			pSrcProfile->ChannelInfo.numOfChannels;
		qdf_mem_copy(pDstProfile->ChannelInfo.freq_list,
			     pSrcProfile->ChannelInfo.freq_list,
			     sizeof(uint32_t) *
			     pSrcProfile->ChannelInfo.numOfChannels);
	}
	pDstProfile->AuthType = pSrcProfile->AuthType;
	pDstProfile->akm_list = pSrcProfile->akm_list;
	pDstProfile->EncryptionType = pSrcProfile->EncryptionType;
	pDstProfile->mcEncryptionType = pSrcProfile->mcEncryptionType;
	pDstProfile->negotiatedUCEncryptionType =
		pSrcProfile->negotiatedUCEncryptionType;
	pDstProfile->negotiatedMCEncryptionType =
		pSrcProfile->negotiatedMCEncryptionType;
	pDstProfile->negotiatedAuthType = pSrcProfile->negotiatedAuthType;
#ifdef WLAN_FEATURE_11W
	pDstProfile->MFPEnabled = pSrcProfile->MFPEnabled;
	pDstProfile->MFPRequired = pSrcProfile->MFPRequired;
	pDstProfile->MFPCapable = pSrcProfile->MFPCapable;
#endif
	pDstProfile->BSSType = pSrcProfile->BSSType;
	pDstProfile->phyMode = pSrcProfile->phyMode;
	pDstProfile->csrPersona = pSrcProfile->csrPersona;

#ifdef FEATURE_WLAN_WAPI
	if (csr_is_profile_wapi(pSrcProfile))
		if (pDstProfile->phyMode & eCSR_DOT11_MODE_11n)
			pDstProfile->phyMode &= ~eCSR_DOT11_MODE_11n;
#endif /* FEATURE_WLAN_WAPI */
	pDstProfile->ch_params.ch_width = pSrcProfile->ch_params.ch_width;
	pDstProfile->ch_params.center_freq_seg0 =
		pSrcProfile->ch_params.center_freq_seg0;
	pDstProfile->ch_params.center_freq_seg1 =
		pSrcProfile->ch_params.center_freq_seg1;
	pDstProfile->ch_params.sec_ch_offset =
		pSrcProfile->ch_params.sec_ch_offset;
	/*Save the WPS info */
	pDstProfile->bWPSAssociation = pSrcProfile->bWPSAssociation;
	pDstProfile->bOSENAssociation = pSrcProfile->bOSENAssociation;
	pDstProfile->force_24ghz_in_ht20 = pSrcProfile->force_24ghz_in_ht20;
	pDstProfile->uapsd_mask = pSrcProfile->uapsd_mask;
	pDstProfile->beaconInterval = pSrcProfile->beaconInterval;
	pDstProfile->privacy = pSrcProfile->privacy;
	pDstProfile->fwdWPSPBCProbeReq = pSrcProfile->fwdWPSPBCProbeReq;
	pDstProfile->csr80211AuthType = pSrcProfile->csr80211AuthType;
	pDstProfile->dtimPeriod = pSrcProfile->dtimPeriod;
	pDstProfile->ApUapsdEnable = pSrcProfile->ApUapsdEnable;
	pDstProfile->SSIDs.SSIDList[0].ssidHidden =
		pSrcProfile->SSIDs.SSIDList[0].ssidHidden;
	pDstProfile->protEnabled = pSrcProfile->protEnabled;
	pDstProfile->obssProtEnabled = pSrcProfile->obssProtEnabled;
	pDstProfile->cfg_protection = pSrcProfile->cfg_protection;
	pDstProfile->wps_state = pSrcProfile->wps_state;
	pDstProfile->ieee80211d = pSrcProfile->ieee80211d;
	qdf_mem_copy(&pDstProfile->Keys, &pSrcProfile->Keys,
		sizeof(pDstProfile->Keys));
#ifdef WLAN_FEATURE_11W
	pDstProfile->MFPEnabled = pSrcProfile->MFPEnabled;
	pDstProfile->MFPRequired = pSrcProfile->MFPRequired;
	pDstProfile->MFPCapable = pSrcProfile->MFPCapable;
#endif
	pDstProfile->mdid = pSrcProfile->mdid;
	pDstProfile->add_ie_params = pSrcProfile->add_ie_params;

	update_profile_fils_info(mac, pDstProfile, pSrcProfile, vdev_id);

	pDstProfile->beacon_tx_rate = pSrcProfile->beacon_tx_rate;

	if (pSrcProfile->supported_rates.numRates) {
		qdf_mem_copy(pDstProfile->supported_rates.rate,
				pSrcProfile->supported_rates.rate,
				pSrcProfile->supported_rates.numRates);
		pDstProfile->supported_rates.numRates =
			pSrcProfile->supported_rates.numRates;
	}
	if (pSrcProfile->extended_rates.numRates) {
		qdf_mem_copy(pDstProfile->extended_rates.rate,
				pSrcProfile->extended_rates.rate,
				pSrcProfile->extended_rates.numRates);
		pDstProfile->extended_rates.numRates =
			pSrcProfile->extended_rates.numRates;
	}
	pDstProfile->require_h2e = pSrcProfile->require_h2e;
	pDstProfile->cac_duration_ms = pSrcProfile->cac_duration_ms;
	pDstProfile->dfs_regdomain   = pSrcProfile->dfs_regdomain;
	pDstProfile->chan_switch_hostapd_rate_enabled  =
		pSrcProfile->chan_switch_hostapd_rate_enabled;
	pDstProfile->force_rsne_override = pSrcProfile->force_rsne_override;
end:
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		csr_release_profile(mac, pDstProfile);
		pDstProfile = NULL;
	}

	return status;
}

QDF_STATUS csr_roam_copy_connected_profile(struct mac_context *mac,
					   uint32_t sessionId,
					   struct csr_roam_profile *pDstProfile)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tCsrRoamConnectedProfile *pSrcProfile =
		&mac->roam.roamSession[sessionId].connectedProfile;

	qdf_mem_zero(pDstProfile, sizeof(struct csr_roam_profile));

	pDstProfile->BSSIDs.bssid = qdf_mem_malloc(sizeof(struct qdf_mac_addr));
	if (!pDstProfile->BSSIDs.bssid) {
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	pDstProfile->BSSIDs.numOfBSSIDs = 1;
	qdf_copy_macaddr(pDstProfile->BSSIDs.bssid, &pSrcProfile->bssid);

	if (pSrcProfile->SSID.length > 0) {
		pDstProfile->SSIDs.SSIDList =
			qdf_mem_malloc(sizeof(tCsrSSIDInfo));
		if (!pDstProfile->SSIDs.SSIDList) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->SSIDs.numOfSSIDs = 1;
		pDstProfile->SSIDs.SSIDList[0].handoffPermitted =
			pSrcProfile->handoffPermitted;
		pDstProfile->SSIDs.SSIDList[0].ssidHidden =
			pSrcProfile->ssidHidden;
		qdf_mem_copy(&pDstProfile->SSIDs.SSIDList[0].SSID,
			&pSrcProfile->SSID, sizeof(tSirMacSSid));
	}
	if (pSrcProfile->nAddIEAssocLength) {
		pDstProfile->pAddIEAssoc =
			qdf_mem_malloc(pSrcProfile->nAddIEAssocLength);
		if (!pDstProfile->pAddIEAssoc) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}
		pDstProfile->nAddIEAssocLength = pSrcProfile->nAddIEAssocLength;
		qdf_mem_copy(pDstProfile->pAddIEAssoc, pSrcProfile->pAddIEAssoc,
			pSrcProfile->nAddIEAssocLength);
	}
	pDstProfile->ChannelInfo.freq_list = qdf_mem_malloc(sizeof(uint32_t));
	if (!pDstProfile->ChannelInfo.freq_list) {
		pDstProfile->ChannelInfo.numOfChannels = 0;
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	pDstProfile->ChannelInfo.numOfChannels = 1;
	pDstProfile->ChannelInfo.freq_list[0] = pSrcProfile->op_freq;
	pDstProfile->AuthType.numEntries = 1;
	pDstProfile->AuthType.authType[0] = pSrcProfile->AuthType;
	pDstProfile->negotiatedAuthType = pSrcProfile->AuthType;
	pDstProfile->akm_list = pSrcProfile->akm_list;
	pDstProfile->EncryptionType.numEntries = 1;
	pDstProfile->EncryptionType.encryptionType[0] =
		pSrcProfile->EncryptionType;
	pDstProfile->negotiatedUCEncryptionType =
		pSrcProfile->EncryptionType;
	pDstProfile->mcEncryptionType.numEntries = 1;
	pDstProfile->mcEncryptionType.encryptionType[0] =
		pSrcProfile->mcEncryptionType;
	pDstProfile->negotiatedMCEncryptionType =
		pSrcProfile->mcEncryptionType;
	pDstProfile->BSSType = pSrcProfile->BSSType;
	qdf_mem_copy(&pDstProfile->Keys, &pSrcProfile->Keys,
		sizeof(pDstProfile->Keys));
	pDstProfile->mdid = pSrcProfile->mdid;
#ifdef WLAN_FEATURE_11W
	pDstProfile->MFPEnabled = pSrcProfile->MFPEnabled;
	pDstProfile->MFPRequired = pSrcProfile->MFPRequired;
	pDstProfile->MFPCapable = pSrcProfile->MFPCapable;
#endif

end:
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		csr_release_profile(mac, pDstProfile);
		pDstProfile = NULL;
	}

	return status;
}

QDF_STATUS csr_roam_issue_connect(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_profile *pProfile,
				  tScanResultHandle hBSSList,
				  enum csr_roam_reason reason, uint32_t roamId,
				  bool fImediate, bool fClearScan)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	pCommand = csr_get_command_buffer(mac);
	if (!pCommand) {
		csr_scan_result_purge(mac, hBSSList);
		sme_err(" fail to get command buffer");
		status = QDF_STATUS_E_RESOURCES;
	} else {
		if (fClearScan)
			csr_scan_abort_mac_scan(mac, sessionId, INVAL_SCAN_ID);

		pCommand->u.roamCmd.fReleaseProfile = false;
		if (!pProfile) {
			/* We can roam now
			 * Since pProfile is NULL, we need to build our own
			 * profile, set everything to default We can only
			 * support open and no encryption
			 */
			pCommand->u.roamCmd.roamProfile.AuthType.numEntries = 1;
			pCommand->u.roamCmd.roamProfile.AuthType.authType[0] =
				eCSR_AUTH_TYPE_OPEN_SYSTEM;
			pCommand->u.roamCmd.roamProfile.EncryptionType.
			numEntries = 1;
			pCommand->u.roamCmd.roamProfile.EncryptionType.
			encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
			pCommand->u.roamCmd.roamProfile.csrPersona =
				QDF_STA_MODE;
		} else {
			/* make a copy of the profile */
			status = csr_roam_copy_profile(mac, &pCommand->u.
						       roamCmd.roamProfile,
						       pProfile, sessionId);
			if (QDF_IS_STATUS_SUCCESS(status))
				pCommand->u.roamCmd.fReleaseProfile = true;
		}

		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.hBSSList = hBSSList;
		pCommand->u.roamCmd.roamId = roamId;
		pCommand->u.roamCmd.roamReason = reason;
		/* We need to free the BssList when the command is done */
		pCommand->u.roamCmd.fReleaseBssList = true;
		pCommand->u.roamCmd.fUpdateCurRoamProfile = true;
		status = csr_queue_sme_command(mac, pCommand, fImediate);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("fail to send message status: %d", status);
		}
	}

	return status;
}

QDF_STATUS csr_roam_issue_reassoc(struct mac_context *mac, uint32_t sessionId,
				  struct csr_roam_profile *pProfile,
				  tCsrRoamModifyProfileFields
				*pMmodProfileFields,
				  enum csr_roam_reason reason, uint32_t roamId,
				  bool fImediate)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	pCommand = csr_get_command_buffer(mac);
	if (!pCommand) {
		sme_err("fail to get command buffer");
		status = QDF_STATUS_E_RESOURCES;
	} else {
		csr_scan_abort_mac_scan(mac, sessionId, INVAL_SCAN_ID);
		if (pProfile) {
			/* This is likely trying to reassoc to
			 * different profile
			 */
			pCommand->u.roamCmd.fReleaseProfile = false;
			/* make a copy of the profile */
			status = csr_roam_copy_profile(mac, &pCommand->u.
							roamCmd.roamProfile,
						      pProfile, sessionId);
			pCommand->u.roamCmd.fUpdateCurRoamProfile = true;
		} else {
			status = csr_roam_copy_connected_profile(mac,
							sessionId,
							&pCommand->u.roamCmd.
							roamProfile);
			/* how to update WPA/WPA2 info in roamProfile?? */
			pCommand->u.roamCmd.roamProfile.uapsd_mask =
				pMmodProfileFields->uapsd_mask;
		}
		if (QDF_IS_STATUS_SUCCESS(status))
			pCommand->u.roamCmd.fReleaseProfile = true;
		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamId = roamId;
		pCommand->u.roamCmd.roamReason = reason;
		/* We need to free the BssList when the command is done */
		/* For reassoc there is no BSS list, so the bool set to false */
		pCommand->u.roamCmd.hBSSList = CSR_INVALID_SCANRESULT_HANDLE;
		pCommand->u.roamCmd.fReleaseBssList = false;
		pCommand->u.roamCmd.fReassoc = true;
		csr_roam_remove_duplicate_command(mac, sessionId, pCommand,
						  reason);
		status = csr_queue_sme_command(mac, pCommand, fImediate);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("fail to send message status = %d", status);
			csr_roam_completion(mac, sessionId, NULL, NULL,
					    eCSR_ROAM_RESULT_FAILURE, false);
		}
	}
	return status;
}

QDF_STATUS csr_dequeue_roam_command(struct mac_context *mac,
			enum csr_roam_reason reason,
					uint8_t session_id)
{
	tListElem *pEntry;
	tSmeCmd *pCommand;

	pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);

	if (pEntry) {
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
		if ((eSmeCommandRoam == pCommand->command) &&
		    (eCsrPerformPreauth == reason)) {
			sme_debug("DQ-Command = %d, Reason = %d",
				pCommand->command,
				pCommand->u.roamCmd.roamReason);
			if (csr_nonscan_active_ll_remove_entry(mac, pEntry,
				    LL_ACCESS_LOCK)) {
				csr_release_command(mac, pCommand);
			}
		} else if ((eSmeCommandRoam == pCommand->command) &&
			   (eCsrSmeIssuedFTReassoc == reason)) {
			sme_debug("DQ-Command = %d, Reason = %d",
				pCommand->command,
				pCommand->u.roamCmd.roamReason);
			if (csr_nonscan_active_ll_remove_entry(mac, pEntry,
				    LL_ACCESS_LOCK)) {
				csr_release_command(mac, pCommand);
			}
		} else {
			sme_err("Command = %d, Reason = %d ",
				pCommand->command,
				pCommand->u.roamCmd.roamReason);
		}
	} else {
		sme_err("pEntry NULL for eWNI_SME_FT_PRE_AUTH_RSP");
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_FILS_SK
/**
 * csr_is_fils_connection() - API to check if FILS connection
 * @profile: CSR Roam Profile
 *
 * Return: true, if fils connection, false otherwise
 */
static bool csr_is_fils_connection(struct csr_roam_profile *profile)
{
	if (!profile->fils_con_info)
		return false;

	return profile->fils_con_info->is_fils_connection;
}
#else
static bool csr_is_fils_connection(struct csr_roam_profile *pProfile)
{
	return false;
}
#endif

/**
 * csr_roam_print_candidate_aps() - print all candidate AP in sorted
 * score.
 * @results: scan result
 *
 * Return : void
 */
static void csr_roam_print_candidate_aps(tScanResultHandle results)
{
	tListElem *entry;
	struct tag_csrscan_result *bss_desc = NULL;
	struct scan_result_list *bss_list = NULL;

	if (!results)
		return;
	bss_list = (struct scan_result_list *)results;
	entry = csr_ll_peek_head(&bss_list->List, LL_ACCESS_NOLOCK);
	while (entry) {
		bss_desc = GET_BASE_ADDR(entry,
				struct tag_csrscan_result, Link);
		sme_nofl_debug(QDF_MAC_ADDR_FMT " score: %d",
			QDF_MAC_ADDR_REF(bss_desc->Result.BssDescriptor.bssId),
			bss_desc->bss_score);

		entry = csr_ll_next(&bss_list->List, entry,
				LL_ACCESS_NOLOCK);
	}
}

void csr_set_open_mode_in_scan_filter(struct scan_filter *filter)
{
	QDF_SET_PARAM(filter->authmodeset, WLAN_CRYPTO_AUTH_OPEN);
}

QDF_STATUS csr_roam_connect(struct mac_context *mac, uint32_t sessionId,
		struct csr_roam_profile *pProfile,
		uint32_t *pRoamId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tScanResultHandle hBSSList;
	struct scan_filter *filter;
	uint32_t roamId = 0;
	bool fCallCallback = false;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	struct bss_description *first_ap_profile;
	enum QDF_OPMODE opmode = QDF_STA_MODE;
	uint32_t ch_freq;

	if (!pSession) {
		sme_err("session does not exist for given sessionId: %d",
			sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (!pProfile) {
		sme_err("No profile specified");
		return QDF_STATUS_E_FAILURE;
	}

	first_ap_profile = qdf_mem_malloc(sizeof(*first_ap_profile));
	if (!first_ap_profile)
		return QDF_STATUS_E_NOMEM;

	/* Initialize the count before proceeding with the Join requests */
	pSession->join_bssid_count = 0;
	pSession->discon_in_progress = false;
	pSession->is_fils_connection = csr_is_fils_connection(pProfile);
	sme_debug("Persona %d authtype %d  encryType %d mc_encType %d",
		  pProfile->csrPersona, pProfile->AuthType.authType[0],
		  pProfile->EncryptionType.encryptionType[0],
		  pProfile->mcEncryptionType.encryptionType[0]);
	csr_roam_cancel_roaming(mac, sessionId);
	csr_scan_abort_mac_scan(mac, sessionId, INVAL_SCAN_ID);
	csr_roam_remove_duplicate_command(mac, sessionId, NULL, eCsrHddIssued);

	/* Check whether ssid changes */
	if (csr_is_conn_state_connected(mac, sessionId) &&
	    pProfile->SSIDs.numOfSSIDs &&
	    !csr_is_ssid_in_list(&pSession->connectedProfile.SSID,
				 &pProfile->SSIDs))
		csr_roam_issue_disassociate_cmd(mac, sessionId,
					eCSR_DISCONNECT_REASON_UNSPECIFIED,
					REASON_UNSPEC_FAILURE);
	/*
	 * If roamSession.connectState is disconnecting that mean
	 * disconnect was received with scan for ssid in progress
	 * and dropped. This state will ensure that connect will
	 * not be issued from scan for ssid completion. Thus
	 * if this fresh connect also issue scan for ssid the connect
	 * command will be dropped assuming disconnect is in progress.
	 * Thus reset connectState here
	 */
	if (eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTING ==
			mac->roam.roamSession[sessionId].connectState)
		mac->roam.roamSession[sessionId].connectState =
			eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;

	filter = qdf_mem_malloc(sizeof(*filter));
	if (!filter) {
		status = QDF_STATUS_E_NOMEM;
		goto error;
	}

	/* Here is the profile we need to connect to */
	status = csr_roam_get_scan_filter_from_profile(mac, pProfile,
						       filter, false,
						       sessionId);
	opmode = pProfile->csrPersona;
	roamId = GET_NEXT_ROAM_ID(&mac->roam);
	if (pRoamId)
		*pRoamId = roamId;
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_free(filter);
		goto error;
	}

	if (pProfile && CSR_IS_INFRA_AP(pProfile)) {
		/* This can be started right away */
		status = csr_roam_issue_connect(mac, sessionId, pProfile, NULL,
				 eCsrHddIssued, roamId, false, false);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("CSR failed to issue start BSS cmd with status: 0x%08X",
				status);
			fCallCallback = true;
		} else
			sme_debug("Connect request to proceed for sap mode");

		qdf_mem_free(filter);
		goto error;
	}
	status = csr_scan_get_result(mac, filter, &hBSSList,
				     opmode == QDF_STA_MODE ? true : false);
	qdf_mem_free(filter);
	csr_roam_print_candidate_aps(hBSSList);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		/* check if set hw mode needs to be done */
		if ((opmode == QDF_STA_MODE) ||
		    (opmode == QDF_P2P_CLIENT_MODE)) {
			bool ok;

			csr_get_bssdescr_from_scan_handle(hBSSList,
							  first_ap_profile);
			status = policy_mgr_is_chan_ok_for_dnbs(
					mac->psoc,
					first_ap_profile->chan_freq, &ok);
			if (QDF_IS_STATUS_ERROR(status)) {
				sme_debug("policy_mgr_is_chan_ok_for_dnbs():error:%d",
					  status);
				csr_scan_result_purge(mac, hBSSList);
				fCallCallback = true;
				goto error;
			}
			if (!ok) {
				sme_debug("freq:%d not ok for DNBS",
					  first_ap_profile->chan_freq);
				csr_scan_result_purge(mac, hBSSList);
				fCallCallback = true;
				status = QDF_STATUS_E_INVAL;
				goto error;
			}

			ch_freq = csr_get_channel_for_hw_mode_change
					(mac, hBSSList, sessionId);
			if (!ch_freq)
				ch_freq = first_ap_profile->chan_freq;

			status = policy_mgr_handle_conc_multiport(
					mac->psoc, sessionId, ch_freq,
					POLICY_MGR_UPDATE_REASON_NORMAL_STA,
					POLICY_MGR_DEF_REQ_ID);
			if ((QDF_IS_STATUS_SUCCESS(status)) &&
				(!csr_wait_for_connection_update(mac, true))) {
					sme_debug("conn update error");
					csr_scan_result_purge(mac, hBSSList);
					fCallCallback = true;
					status = QDF_STATUS_E_TIMEOUT;
					goto error;
			} else if (status == QDF_STATUS_E_FAILURE) {
				sme_debug("conn update error");
				csr_scan_result_purge(mac, hBSSList);
				fCallCallback = true;
				goto error;
			}
		}

		status = csr_roam_issue_connect(mac, sessionId, pProfile,
				hBSSList, eCsrHddIssued, roamId, false, false);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("CSR failed to issue connect cmd with status: 0x%08X",
				status);
			fCallCallback = true;
		}
	} else if (pProfile) {
		if (CSR_IS_NDI(pProfile)) {
			status = csr_roam_issue_connect(mac, sessionId,
					pProfile, NULL, eCsrHddIssued,
					roamId, false, false);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_err("Failed with status = 0x%08X",
					status);
				fCallCallback = true;
			}
		} else if (status == QDF_STATUS_E_EXISTS &&
			   pProfile->BSSIDs.numOfBSSIDs) {
			sme_debug("Scan entries removed either due to rssi reject or assoc disallowed");
			fCallCallback = true;
		} else {
			/* scan for this SSID */
			status = csr_scan_for_ssid(mac, sessionId, pProfile,
						roamId, true);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				sme_err("CSR failed to issue SSID scan cmd with status: 0x%08X",
					status);
				fCallCallback = true;
			}
		}
	} else {
		fCallCallback = true;
	}

error:
	/* tell the caller if we fail to trigger a join request */
	if (fCallCallback) {
		csr_roam_call_callback(mac, sessionId, NULL, roamId,
				eCSR_ROAM_FAILED, eCSR_ROAM_RESULT_FAILURE);
	}
	qdf_mem_free(first_ap_profile);

	return status;
}

/**
 * csr_roam_reassoc() - process reassoc command
 * @mac_ctx:       mac global context
 * @session_id:    session id
 * @profile:       roam profile
 * @mod_fields:    AC info being modified in reassoc
 * @roam_id:       roam id to be populated
 *
 * Return: status of operation
 */
QDF_STATUS
csr_roam_reassoc(struct mac_context *mac_ctx, uint32_t session_id,
		 struct csr_roam_profile *profile,
		 tCsrRoamModifyProfileFields mod_fields,
		 uint32_t *roam_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool fCallCallback = true;
	uint32_t roamId = 0;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!profile) {
		sme_err("No profile specified");
		return QDF_STATUS_E_FAILURE;
	}
	sme_debug(
		"called  BSSType = %s (%d) authtype = %d  encryType = %d",
		sme_bss_type_to_string(profile->BSSType),
		profile->BSSType, profile->AuthType.authType[0],
		profile->EncryptionType.encryptionType[0]);
	csr_roam_cancel_roaming(mac_ctx, session_id);
	csr_scan_abort_mac_scan(mac_ctx, session_id, INVAL_SCAN_ID);
	csr_roam_remove_duplicate_command(mac_ctx, session_id, NULL,
					  eCsrHddIssuedReassocToSameAP);
	if (csr_is_conn_state_connected(mac_ctx, session_id)) {
		if (profile) {
			if (profile->SSIDs.numOfSSIDs &&
			    csr_is_ssid_in_list(&session->connectedProfile.SSID,
						&profile->SSIDs)) {
				fCallCallback = false;
			} else {
				/*
				 * Connected SSID did not match with what is
				 * asked in profile
				 */
				sme_debug("SSID mismatch");
			}
		} else if (qdf_mem_cmp(&mod_fields,
				&session->connectedProfile.modifyProfileFields,
				sizeof(tCsrRoamModifyProfileFields))) {
			fCallCallback = false;
		} else {
			sme_debug(
				/*
				 * Either the profile is NULL or none of the
				 * fields in tCsrRoamModifyProfileFields got
				 * modified
				 */
				"Profile NULL or nothing to modify");
		}
	} else {
		sme_debug("Not connected! No need to reassoc");
	}
	if (!fCallCallback) {
		roamId = GET_NEXT_ROAM_ID(&mac_ctx->roam);
		if (roam_id)
			*roam_id = roamId;
		status = csr_roam_issue_reassoc(mac_ctx, session_id, profile,
				&mod_fields, eCsrHddIssuedReassocToSameAP,
				roamId, false);
	} else {
		status = csr_roam_call_callback(mac_ctx, session_id, NULL,
						roamId, eCSR_ROAM_FAILED,
						eCSR_ROAM_RESULT_FAILURE);
	}
	return status;
}

QDF_STATUS csr_roam_process_disassoc_deauth(struct mac_context *mac,
						tSmeCmd *pCommand,
					    bool fDisassoc, bool fMICFailure)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	bool fComplete = false;
	enum csr_roam_substate NewSubstate;
	uint32_t sessionId = pCommand->vdev_id;

	if (CSR_IS_WAIT_FOR_KEY(mac, sessionId)) {
		sme_debug("Stop Wait for key timer and change substate to eCSR_ROAM_SUBSTATE_NONE");
		csr_roam_stop_wait_for_key_timer(mac);
		csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
					sessionId);
	}
	/* change state to 'Roaming'... */
	csr_roam_state_change(mac, eCSR_ROAMING_STATE_JOINING, sessionId);
	mlme_set_discon_reason_n_from_ap(mac->psoc, pCommand->vdev_id, false,
					 pCommand->u.roamCmd.disconnect_reason);
	if (csr_is_conn_state_infra(mac, sessionId)) {
		/*
		 * in Infrastructure, we need to disassociate from the
		 * Infrastructure network...
		 */
		NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_FORCED;
		if (eCsrSmeIssuedDisassocForHandoff ==
		    pCommand->u.roamCmd.roamReason) {
			NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF;
		} else
		if ((eCsrForcedDisassoc == pCommand->u.roamCmd.roamReason)
		    && (pCommand->u.roamCmd.reason ==
			REASON_DISASSOC_NETWORK_LEAVING)) {
			NewSubstate = eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT;
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
					 "set to substate eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT");
		}
		if (eCsrSmeIssuedDisassocForHandoff !=
				pCommand->u.roamCmd.roamReason) {
			/*
			 * If we are in neighbor preauth done state then
			 * on receiving disassoc or deauth we dont roam
			 * instead we just disassoc from current ap and
			 * then go to disconnected state.
			 * This happens for ESE and 11r FT connections ONLY.
			 */
			if (csr_roam_is11r_assoc(mac, sessionId) &&
				(csr_neighbor_roam_state_preauth_done(mac,
							sessionId)))
				csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac, sessionId);
#ifdef FEATURE_WLAN_ESE
			if (csr_roam_is_ese_assoc(mac, sessionId) &&
				(csr_neighbor_roam_state_preauth_done(mac,
							sessionId)))
				csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac, sessionId);
#endif
			if (csr_roam_is_fast_roam_enabled(mac, sessionId) &&
				(csr_neighbor_roam_state_preauth_done(mac,
							sessionId)))
				csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac, sessionId);
		}
		if (fDisassoc)
			status = csr_roam_issue_disassociate(mac, sessionId,
								NewSubstate,
								fMICFailure);
		else
			status = csr_roam_issue_deauth(mac, sessionId,
						eCSR_ROAM_SUBSTATE_DEAUTH_REQ);
		fComplete = (!QDF_IS_STATUS_SUCCESS(status));
	} else {
		/* we got a dis-assoc request while not connected to any peer */
		/* just complete the command */
		fComplete = true;
		status = QDF_STATUS_E_FAILURE;
	}
	if (fComplete)
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, sessionId);

	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (csr_is_conn_state_infra(mac, sessionId)) {
			/* Set the state to disconnect here */
			mac->roam.roamSession[sessionId].connectState =
				eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
		}
	} else
		sme_warn(" failed with status %d", status);
	return status;
}

static void csr_abort_connect_request_timers(
	struct mac_context *mac, uint32_t vdev_id)
{
	struct scheduler_msg msg;
	QDF_STATUS status;
	enum QDF_OPMODE op_mode;

	op_mode = wlan_get_opmode_from_vdev_id(mac->pdev, vdev_id);
	if (op_mode != QDF_STA_MODE &&
	    op_mode != QDF_P2P_CLIENT_MODE)
		return;
	qdf_mem_zero(&msg, sizeof(msg));
	msg.bodyval = vdev_id;
	msg.type = eWNI_SME_ABORT_CONN_TIMER;
	status = scheduler_post_message(QDF_MODULE_ID_MLME,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_IS_STATUS_ERROR(status))
		sme_debug("msg eWNI_SME_ABORT_CONN_TIMER post fail");
}

QDF_STATUS csr_roam_issue_disassociate_cmd(struct mac_context *mac,
					uint32_t sessionId,
					eCsrRoamDisconnectReason reason,
					enum wlan_reason_code mac_reason)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	do {
		pCommand = csr_get_command_buffer(mac);
		if (!pCommand) {
			sme_err(" fail to get command buffer");
			status = QDF_STATUS_E_RESOURCES;
			break;
		}
		/* Change the substate in case it is wait-for-key */
		if (CSR_IS_WAIT_FOR_KEY(mac, sessionId)) {
			csr_roam_stop_wait_for_key_timer(mac);
			csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
						 sessionId);
		}
		csr_abort_connect_request_timers(mac, sessionId);

		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		sme_debug("Disassociate reason: %d, vdev_id: %d mac_reason %d",
			 reason, sessionId, mac_reason);
		switch (reason) {
		case eCSR_DISCONNECT_REASON_MIC_ERROR:
			pCommand->u.roamCmd.roamReason =
				eCsrForcedDisassocMICFailure;
			break;
		case eCSR_DISCONNECT_REASON_DEAUTH:
			pCommand->u.roamCmd.roamReason = eCsrForcedDeauth;
			break;
		case eCSR_DISCONNECT_REASON_HANDOFF:
			pCommand->u.roamCmd.roamReason =
				eCsrSmeIssuedDisassocForHandoff;
			break;
		case eCSR_DISCONNECT_REASON_UNSPECIFIED:
		case eCSR_DISCONNECT_REASON_DISASSOC:
			pCommand->u.roamCmd.roamReason = eCsrForcedDisassoc;
			break;
		case eCSR_DISCONNECT_REASON_ROAM_HO_FAIL:
			pCommand->u.roamCmd.roamReason = eCsrForcedDisassoc;
			break;
		case eCSR_DISCONNECT_REASON_STA_HAS_LEFT:
			pCommand->u.roamCmd.roamReason = eCsrForcedDisassoc;
			pCommand->u.roamCmd.reason =
				REASON_DISASSOC_NETWORK_LEAVING;
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				 "SME convert to internal reason code eCsrStaHasLeft");
			break;
		case eCSR_DISCONNECT_REASON_NDI_DELETE:
			pCommand->u.roamCmd.roamReason = eCsrStopBss;
			pCommand->u.roamCmd.roamProfile.BSSType =
				eCSR_BSS_TYPE_NDI;
		default:
			break;
		}
		pCommand->u.roamCmd.disconnect_reason = mac_reason;
		status = csr_queue_sme_command(mac, pCommand, true);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} while (0);
	return status;
}

QDF_STATUS csr_roam_issue_stop_bss_cmd(struct mac_context *mac, uint32_t sessionId,
				       bool fHighPriority)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSmeCmd *pCommand;

	pCommand = csr_get_command_buffer(mac);
	if (pCommand) {
		/* Change the substate in case it is wait-for-key */
		if (CSR_IS_WAIT_FOR_KEY(mac, sessionId)) {
			csr_roam_stop_wait_for_key_timer(mac);
			csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
						 sessionId);
		}
		pCommand->command = eSmeCommandRoam;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.roamCmd.roamReason = eCsrStopBss;
		status = csr_queue_sme_command(mac, pCommand, fHighPriority);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err("fail to send message status: %d", status);
	} else {
		sme_err("fail to get command buffer");
		status = QDF_STATUS_E_RESOURCES;
	}
	return status;
}

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void
csr_disable_roaming_offload(struct mac_context *mac_ctx, uint32_t vdev_id)
{
	sme_stop_roaming(MAC_HANDLE(mac_ctx), vdev_id,
			 REASON_DRIVER_DISABLED,
			 RSO_INVALID_REQUESTOR);
}
#else
static inline void
csr_disable_roaming_offload(struct mac_context *mac_ctx, uint32_t session_id)
{}
#endif

QDF_STATUS csr_roam_disconnect(struct mac_context *mac_ctx, uint32_t session_id,
			       eCsrRoamDisconnectReason reason,
			       enum wlan_reason_code mac_reason)
{
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!session) {
		sme_err("session: %d not found ", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	session->discon_in_progress = true;

	csr_roam_cancel_roaming(mac_ctx, session_id);
	csr_disable_roaming_offload(mac_ctx, session_id);

	csr_roam_remove_duplicate_command(mac_ctx, session_id, NULL,
					  eCsrForcedDisassoc);

	if (csr_is_conn_state_connected(mac_ctx, session_id)
	    || csr_is_roam_command_waiting_for_session(mac_ctx, session_id)
	    || CSR_IS_CONN_NDI(&session->connectedProfile)) {
		status = csr_roam_issue_disassociate_cmd(mac_ctx, session_id,
							 reason, mac_reason);
	} else if (csr_is_deauth_disassoc_already_active(mac_ctx, session_id,
					session->connectedProfile.bssid)) {
		/* Return success if Disconnect is already in progress */
		sme_debug("Disconnect already in queue return success");
		status = QDF_STATUS_SUCCESS;
	} else if (session->scan_info.profile) {
		mac_ctx->roam.roamSession[session_id].connectState =
			eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTING;
		csr_scan_abort_mac_scan(mac_ctx, session_id, INVAL_SCAN_ID);
		status = QDF_STATUS_CMD_NOT_QUEUED;
		sme_debug("Disconnect cmd not queued, Roam command is not present return with status %d",
			  status);
	}

	return status;
}

QDF_STATUS
csr_roam_save_connected_information(struct mac_context *mac,
				    uint32_t sessionId,
				    struct csr_roam_profile *pProfile,
				    struct bss_description *pSirBssDesc,
				    tDot11fBeaconIEs *pIes)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tDot11fBeaconIEs *pIesTemp = pIes;
	uint8_t index;
	struct csr_roam_session *pSession = NULL;
	tCsrRoamConnectedProfile *pConnectProfile = NULL;

	pSession = CSR_GET_SESSION(mac, sessionId);
	if (!pSession) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			 "session: %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pConnectProfile = &pSession->connectedProfile;
	if (pConnectProfile->pAddIEAssoc) {
		qdf_mem_free(pConnectProfile->pAddIEAssoc);
		pConnectProfile->pAddIEAssoc = NULL;
	}
	/*
	 * In case of LFR2.0, the connected profile is copied into a temporary
	 * profile and cleared and then is copied back. This is not needed for
	 * LFR3.0, since the profile is not cleared.
	 */
	if (!MLME_IS_ROAM_SYNCH_IN_PROGRESS(mac->psoc, sessionId)) {
		qdf_mem_zero(&pSession->connectedProfile,
				sizeof(tCsrRoamConnectedProfile));
		pConnectProfile->AuthType = pProfile->negotiatedAuthType;
		pConnectProfile->AuthInfo = pProfile->AuthType;
		pConnectProfile->akm_list = pProfile->akm_list;
		pConnectProfile->EncryptionType =
			pProfile->negotiatedUCEncryptionType;
		pConnectProfile->EncryptionInfo = pProfile->EncryptionType;
		pConnectProfile->mcEncryptionType =
			pProfile->negotiatedMCEncryptionType;
		pConnectProfile->mcEncryptionInfo = pProfile->mcEncryptionType;
		pConnectProfile->BSSType = pProfile->BSSType;
		pConnectProfile->modifyProfileFields.uapsd_mask =
			pProfile->uapsd_mask;
		qdf_mem_copy(&pConnectProfile->Keys, &pProfile->Keys,
				sizeof(tCsrKeys));
		if (pProfile->nAddIEAssocLength) {
			pConnectProfile->pAddIEAssoc =
				qdf_mem_malloc(pProfile->nAddIEAssocLength);
			if (!pConnectProfile->pAddIEAssoc)
				return QDF_STATUS_E_FAILURE;

			pConnectProfile->nAddIEAssocLength =
				pProfile->nAddIEAssocLength;
			qdf_mem_copy(pConnectProfile->pAddIEAssoc,
					pProfile->pAddIEAssoc,
					pProfile->nAddIEAssocLength);
		}
#ifdef WLAN_FEATURE_11W
		pConnectProfile->MFPEnabled = pProfile->MFPEnabled;
		pConnectProfile->MFPRequired = pProfile->MFPRequired;
		pConnectProfile->MFPCapable = pProfile->MFPCapable;
#endif
	}
	/* Save bssid */
	pConnectProfile->op_freq = pSirBssDesc->chan_freq;
	pConnectProfile->beaconInterval = pSirBssDesc->beaconInterval;
	if (!pConnectProfile->beaconInterval)
		sme_err("ERROR: Beacon interval is ZERO");
	csr_get_bss_id_bss_desc(pSirBssDesc, &pConnectProfile->bssid);
	if (pSirBssDesc->mdiePresent) {
		pConnectProfile->mdid.mdie_present = 1;
		pConnectProfile->mdid.mobility_domain =
			(pSirBssDesc->mdie[1] << 8) | (pSirBssDesc->mdie[0]);
	}
	if (!pIesTemp)
		status = csr_get_parsed_bss_description_ies(mac, pSirBssDesc,
							   &pIesTemp);
#ifdef FEATURE_WLAN_ESE
	if ((csr_is_profile_ese(pProfile) ||
	     (QDF_IS_STATUS_SUCCESS(status) &&
	      (pIesTemp->ESEVersion.present) &&
	      (pProfile->negotiatedAuthType == eCSR_AUTH_TYPE_OPEN_SYSTEM))) &&
	    (mac->mlme_cfg->lfr.ese_enabled)) {
		pConnectProfile->isESEAssoc = 1;
	}
#endif
	/* save ssid */
	if (QDF_IS_STATUS_SUCCESS(status)) {
		if (pIesTemp->SSID.present &&
		    !csr_is_nullssid(pIesTemp->SSID.ssid,
				     pIesTemp->SSID.num_ssid)) {
			pConnectProfile->SSID.length = pIesTemp->SSID.num_ssid;
			qdf_mem_copy(pConnectProfile->SSID.ssId,
				     pIesTemp->SSID.ssid,
				     pIesTemp->SSID.num_ssid);
		} else if (pProfile->SSIDs.numOfSSIDs) {
			pConnectProfile->SSID.length =
					pProfile->SSIDs.SSIDList[0].SSID.length;
			qdf_mem_copy(pConnectProfile->SSID.ssId,
				     pProfile->SSIDs.SSIDList[0].SSID.ssId,
				     pConnectProfile->SSID.length);
		}
		/* Save the bss desc */
		status = csr_roam_save_connected_bss_desc(mac, sessionId,
								pSirBssDesc);

		if (CSR_IS_QOS_BSS(pIesTemp) || pIesTemp->HTCaps.present)
			/* Some HT AP's dont send WMM IE so in that case we
			 * assume all HT Ap's are Qos Enabled AP's
			 */
			pConnectProfile->qap = true;
		else
			pConnectProfile->qap = false;

		if (pIesTemp->ExtCap.present) {
			struct s_ext_cap *p_ext_cap = (struct s_ext_cap *)
							pIesTemp->ExtCap.bytes;
			pConnectProfile->proxy_arp_service =
				p_ext_cap->proxy_arp_service;
		}

		qdf_mem_zero(pConnectProfile->country_code,
			     WNI_CFG_COUNTRY_CODE_LEN);
		if (pIesTemp->Country.present) {
			qdf_mem_copy(pConnectProfile->country_code,
				     pIesTemp->Country.country,
				     WNI_CFG_COUNTRY_CODE_LEN);
			sme_debug("Storing country in connected info, %c%c 0x%x",
				  pConnectProfile->country_code[0],
				  pConnectProfile->country_code[1],
				  pConnectProfile->country_code[2]);
		}

		if (!pIes)
			/* Free memory if it allocated locally */
			qdf_mem_free(pIesTemp);
	}
	/* Save Qos connection */
	pConnectProfile->qosConnection =
		mac->roam.roamSession[sessionId].fWMMConnection;

	if (!QDF_IS_STATUS_SUCCESS(status))
		csr_free_connect_bss_desc(mac, sessionId);

	for (index = 0; index < pProfile->SSIDs.numOfSSIDs; index++) {
		if ((pProfile->SSIDs.SSIDList[index].SSID.length ==
		     pConnectProfile->SSID.length)
		    && (!qdf_mem_cmp(pProfile->SSIDs.SSIDList[index].SSID.
				       ssId, pConnectProfile->SSID.ssId,
				       pConnectProfile->SSID.length))) {
			pConnectProfile->handoffPermitted = pProfile->SSIDs.
					SSIDList[index].handoffPermitted;
			break;
		}
		pConnectProfile->handoffPermitted = false;
	}

	return status;
}


bool is_disconnect_pending(struct mac_context *pmac, uint8_t vdev_id)
{
	tListElem *entry = NULL;
	tListElem *next_entry = NULL;
	tSmeCmd *command = NULL;
	bool disconnect_cmd_exist = false;

	entry = csr_nonscan_pending_ll_peek_head(pmac, LL_ACCESS_NOLOCK);
	while (entry) {
		next_entry = csr_nonscan_pending_ll_next(pmac,
					entry, LL_ACCESS_NOLOCK);

		command = GET_BASE_ADDR(entry, tSmeCmd, Link);
		if (command && CSR_IS_DISCONNECT_COMMAND(command) &&
				command->vdev_id == vdev_id){
			disconnect_cmd_exist = true;
			break;
		}
		entry = next_entry;
	}

	return disconnect_cmd_exist;
}

bool is_disconnect_pending_on_other_vdev(struct mac_context *pmac,
					 uint8_t vdev_id)
{
	tListElem *entry = NULL;
	tListElem *next_entry = NULL;
	tSmeCmd *command = NULL;
	bool disconnect_cmd_exist = false;

	entry = csr_nonscan_pending_ll_peek_head(pmac, LL_ACCESS_NOLOCK);
	while (entry) {
		next_entry = csr_nonscan_pending_ll_next(pmac, entry,
							 LL_ACCESS_NOLOCK);
		command = GET_BASE_ADDR(entry, tSmeCmd, Link);
		/*
		 * check if any other vdev NB disconnect or SB disconnect
		 * (eSmeCommandWmStatusChange) is pending
		 */
		if (command && (CSR_IS_DISCONNECT_COMMAND(command) ||
		    command->command == eSmeCommandWmStatusChange) &&
		    command->vdev_id != vdev_id) {
			sme_debug("disconnect is pending on vdev:%d, cmd:%d",
				  command->vdev_id, command->command);
			disconnect_cmd_exist = true;
			break;
		}
		entry = next_entry;
	}

	return disconnect_cmd_exist;
}

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void
csr_clear_other_bss_sae_single_pmk_entry(struct mac_context *mac,
					 struct bss_description *bss_desc,
					 uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	uint32_t akm;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return;
	}

	akm = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (!bss_desc->is_single_pmk ||
	    !QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_SAE)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	wlan_crypto_selective_clear_sae_single_pmk_entries(vdev,
			(struct qdf_mac_addr *)bss_desc->bssId);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

static void
csr_delete_current_bss_sae_single_pmk_entry(struct mac_context *mac,
					    struct bss_description *bss_desc,
					    uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_crypto_pmksa pmksa;
	uint32_t akm;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return;
	}

	akm = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (!bss_desc->is_single_pmk ||
	    !QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_SAE)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	qdf_copy_macaddr(&pmksa.bssid,
			 (struct qdf_mac_addr *)bss_desc->bssId);
	wlan_crypto_set_del_pmksa(vdev, &pmksa, false);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#else
static inline void
csr_clear_other_bss_sae_single_pmk_entry(struct mac_context *mac,
					 struct bss_description *bss_desc,
					 uint8_t vdev_id)
{}

static inline void
csr_delete_current_bss_sae_single_pmk_entry(struct mac_context *mac,
					    struct bss_description *bss_desc,
					    uint8_t vdev_id)
{}
#endif

/*
 * Do not allow last connect attempt after 20 sec, assuming a new attempt will
 * take max 10 sec, total connect time will not be more than 30 sec
 */
#define CSR_CONNECT_MAX_ACTIVE_TIME 20000

static bool
csr_is_time_allowed_for_connect_attempt(tSmeCmd *cmd, uint8_t vdev_id)
{
	qdf_time_t time_since_connect_active;

	if (!cmd)
		return false;

	if (cmd->command != eSmeCommandRoam ||
	    cmd->u.roamCmd.roamReason != eCsrHddIssued) {
		sme_err("vdev_id %d invalid command %d, reason %d", vdev_id,
			cmd->command, cmd->command == eSmeCommandRoam ?
			cmd->u.roamCmd.roamReason : 0);
		return false;
	}

	time_since_connect_active = qdf_mc_timer_get_system_time() -
					cmd->u.roamCmd.connect_active_time;
	if (time_since_connect_active >= CSR_CONNECT_MAX_ACTIVE_TIME) {
		sme_info("vdev_id %d Max time allocated (%d ms) for connect completed, cur time %lu, active time %lu and diff %lu",
			 vdev_id, CSR_CONNECT_MAX_ACTIVE_TIME,
			 qdf_mc_timer_get_system_time(),
			 cmd->u.roamCmd.connect_active_time,
			 time_since_connect_active);
		return false;
	}

	return true;
}

static void csr_roam_join_rsp_processor(struct mac_context *mac,
					struct join_rsp *pSmeJoinRsp)
{
	tListElem *pEntry = NULL;
	tSmeCmd *pCommand = NULL;
	mac_handle_t mac_handle = MAC_HANDLE(mac);
	struct csr_roam_session *session_ptr;
	struct csr_roam_profile *profile = NULL;
	struct csr_roam_connectedinfo *prev_connect_info;
	struct wlan_crypto_pmksa *pmksa;
	uint32_t len = 0, roamId = 0, reason_code = 0;
	bool is_dis_pending, is_dis_pending_on_other_vdev;
	bool use_same_bss = false;
	uint8_t max_retry_count = 1;
	bool retry_same_bss = false;
	bool attempt_next_bss = true;
	enum csr_akm_type auth_type = eCSR_AUTH_TYPE_NONE;
	bool is_time_allowed;

	if (!pSmeJoinRsp) {
		sme_err("Sme Join Response is NULL");
		return;
	}

	session_ptr = CSR_GET_SESSION(mac, pSmeJoinRsp->vdev_id);
	if (!session_ptr) {
		sme_err("session %d not found", pSmeJoinRsp->vdev_id);
		return;
	}

	prev_connect_info = &session_ptr->prev_assoc_ap_info;
	/* The head of the active list is the request we sent */
	pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (pEntry)
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);

	if (pSmeJoinRsp->is_fils_connection)
		sme_debug("Fils connection");
	/* Copy Sequence Number last used for FILS assoc failure case */
	if (session_ptr->is_fils_connection)
		session_ptr->fils_seq_num = pSmeJoinRsp->fils_seq_num;

	len = pSmeJoinRsp->assocReqLength +
	      pSmeJoinRsp->assocRspLength + pSmeJoinRsp->beaconLength;
	if (prev_connect_info->pbFrames)
		csr_roam_free_connected_info(mac, prev_connect_info);

	prev_connect_info->pbFrames = qdf_mem_malloc(len);
	if (prev_connect_info->pbFrames) {
		qdf_mem_copy(prev_connect_info->pbFrames, pSmeJoinRsp->frames,
			     len);
		prev_connect_info->nAssocReqLength =
					pSmeJoinRsp->assocReqLength;
		prev_connect_info->nAssocRspLength =
					pSmeJoinRsp->assocRspLength;
		prev_connect_info->nBeaconLength = pSmeJoinRsp->beaconLength;
	}

	if (eSIR_SME_SUCCESS == pSmeJoinRsp->status_code) {
		if (pCommand &&
		    eCsrSmeIssuedAssocToSimilarAP ==
		    pCommand->u.roamCmd.roamReason) {
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
			sme_qos_csr_event_ind(mac, pSmeJoinRsp->vdev_id,
					    SME_QOS_CSR_HANDOFF_COMPLETE, NULL);
#endif
		}

		if (pSmeJoinRsp->nss < session_ptr->nss) {
			session_ptr->nss = pSmeJoinRsp->nss;
			session_ptr->vdev_nss = pSmeJoinRsp->nss;
		}

		session_ptr->supported_nss_1x1 = pSmeJoinRsp->supported_nss_1x1;

		/*
		 * The join bssid count can be reset as soon as
		 * we are done with the join requests and returning
		 * the response to upper layers
		 */
		session_ptr->join_bssid_count = 0;

		/*
		 * On successful connection to sae single pmk AP,
		 * clear all the single pmk AP.
		 */
		if (pCommand && pCommand->u.roamCmd.pLastRoamBss)
			csr_clear_other_bss_sae_single_pmk_entry(mac,
					pCommand->u.roamCmd.pLastRoamBss,
					pSmeJoinRsp->vdev_id);

		csr_roam_complete(mac, eCsrJoinSuccess, (void *)pSmeJoinRsp,
				  pSmeJoinRsp->vdev_id);

		return;
	}

	/* The head of the active list is the request we sent
	 * Try to get back the same profile and roam again
	 */
	if (pCommand) {
		roamId = pCommand->u.roamCmd.roamId;
		profile = &pCommand->u.roamCmd.roamProfile;
		auth_type = profile->AuthType.authType[0];
	}

	reason_code = pSmeJoinRsp->protStatusCode;
	session_ptr->joinFailStatusCode.reasonCode = reason_code;
	session_ptr->joinFailStatusCode.status_code = pSmeJoinRsp->status_code;

	sme_warn("SmeJoinReq failed with status_code= 0x%08X [%d] reason:%d",
		 pSmeJoinRsp->status_code, pSmeJoinRsp->status_code,
		 reason_code);

	is_dis_pending = is_disconnect_pending(mac, session_ptr->sessionId);
	is_dis_pending_on_other_vdev =
		is_disconnect_pending_on_other_vdev(mac,
						    session_ptr->sessionId);
	is_time_allowed =
		csr_is_time_allowed_for_connect_attempt(pCommand,
							session_ptr->vdev_id);

	/*
	 * if userspace has issued disconnection or we have reached mac tries or
	 * max time, driver should not continue for next connection.
	 */
	if (is_dis_pending || is_dis_pending_on_other_vdev || !is_time_allowed ||
	    session_ptr->join_bssid_count >= CSR_MAX_BSSID_COUNT)
		attempt_next_bss = false;

	/*
	 * Delete the PMKID of the BSSID for which the assoc reject is
	 * received from the AP due to invalid PMKID reason.
	 * This will avoid the driver trying to connect to same AP with
	 * the same stale PMKID if multiple connection attempts to different
	 * bss fail and supplicant issues connect request back to the same
	 * AP.
	 */
	if (reason_code == STATUS_INVALID_PMKID) {
		pmksa = qdf_mem_malloc(sizeof(*pmksa));
		if (!pmksa)
			return;

		sme_warn("Assoc reject from BSSID:"QDF_MAC_ADDR_FMT" due to invalid PMKID",
			 QDF_MAC_ADDR_REF(session_ptr->joinFailStatusCode.bssId));
		qdf_mem_copy(pmksa->bssid.bytes,
			     &session_ptr->joinFailStatusCode.bssId,
			     sizeof(tSirMacAddr));
		sme_roam_del_pmkid_from_cache(mac_handle, session_ptr->vdev_id,
					      pmksa, false);
		qdf_mem_free(pmksa);
		retry_same_bss = true;
	}

	if (pSmeJoinRsp->messageType == eWNI_SME_JOIN_RSP &&
	    pSmeJoinRsp->status_code == eSIR_SME_ASSOC_TIMEOUT_RESULT_CODE &&
	    (mlme_get_reconn_after_assoc_timeout_flag(mac->psoc,
						     pSmeJoinRsp->vdev_id) ||
	    (auth_type == eCSR_AUTH_TYPE_SAE ||
	     auth_type == eCSR_AUTH_TYPE_FT_SAE))) {
		retry_same_bss = true;
		if (auth_type == eCSR_AUTH_TYPE_SAE ||
		    auth_type == eCSR_AUTH_TYPE_FT_SAE)
			wlan_mlme_get_sae_assoc_retry_count(mac->psoc,
							    &max_retry_count);
	}

	if (pSmeJoinRsp->messageType == eWNI_SME_JOIN_RSP &&
	    pSmeJoinRsp->status_code == eSIR_SME_JOIN_TIMEOUT_RESULT_CODE &&
	    pCommand && pCommand->u.roamCmd.hBSSList) {
		struct scan_result_list *bss_list =
		   (struct scan_result_list *)pCommand->u.roamCmd.hBSSList;

		if (csr_ll_count(&bss_list->List) == 1) {
			retry_same_bss = true;
			sme_err("retry_same_bss is set");
			wlan_mlme_get_sae_assoc_retry_count(mac->psoc,
							    &max_retry_count);
		}
	}

	if (attempt_next_bss && retry_same_bss &&
	    pCommand && pCommand->u.roamCmd.pRoamBssEntry) {
		struct tag_csrscan_result *scan_result;

		scan_result =
			GET_BASE_ADDR(pCommand->u.roamCmd.pRoamBssEntry,
				      struct tag_csrscan_result, Link);
		/* Retry with same BSSID without PMKID */
		if (scan_result->retry_count < max_retry_count) {
			sme_info("Retry once with same BSSID, status %d reason %d auth_type %d retry count %d max count %d",
				 pSmeJoinRsp->status_code, reason_code,
				 auth_type, scan_result->retry_count,
				 max_retry_count);
			scan_result->retry_count++;
			use_same_bss = true;
		}
	}

	/*
	 * If connection fails with Single PMK bssid, clear this pmk
	 * entry, Flush in case if we are not trying again with same AP
	 */
	if (!use_same_bss && pCommand && pCommand->u.roamCmd.pLastRoamBss)
		csr_delete_current_bss_sae_single_pmk_entry(
			mac, pCommand->u.roamCmd.pLastRoamBss,
			pSmeJoinRsp->vdev_id);

	/* If Join fails while Handoff is in progress, indicate
	 * disassociated event to supplicant to reconnect
	 */
	if (csr_roam_is_handoff_in_progress(mac, pSmeJoinRsp->vdev_id)) {
		csr_roam_call_callback(mac, pSmeJoinRsp->vdev_id, NULL,
				       roamId, eCSR_ROAM_DISASSOCIATED,
				       eCSR_ROAM_RESULT_FORCED);
		/* Should indicate neighbor roam algorithm about the
		 * connect failure here
		 */
		csr_neighbor_roam_indicate_connect(mac, pSmeJoinRsp->vdev_id,
						   QDF_STATUS_E_FAILURE);
	}

	if (pCommand && attempt_next_bss) {
			csr_roam(mac, pCommand, use_same_bss);
			return;
	}

	/*
	 * When the upper layers issue a connect command, there is a roam
	 * command with reason eCsrHddIssued that gets enqueued and an
	 * associated timer for the SME command timeout is started which is
	 * currently 120 seconds. This command would be dequeued only upon
	 * successful connections. In case of join failures, if there are too
	 * many BSS in the cache, and if we fail Join requests with all of them,
	 * there is a chance of timing out the above timer.
	 */
	if (session_ptr->join_bssid_count >= CSR_MAX_BSSID_COUNT)
		sme_err("Excessive Join Req Failures");

	if (is_dis_pending)
		sme_err("disconnect is pending, complete roam");

	if (is_dis_pending_on_other_vdev)
		sme_err("disconnect is pending on other vdev, complete roam");

	if (!is_time_allowed)
		sme_err("time can exceed the active timeout for connection attempt");

	if (session_ptr->bRefAssocStartCnt)
		session_ptr->bRefAssocStartCnt--;

	session_ptr->join_bssid_count = 0;
	csr_roam_call_callback(mac, session_ptr->sessionId, NULL, roamId,
			       eCSR_ROAM_ASSOCIATION_COMPLETION,
			       eCSR_ROAM_RESULT_NOT_ASSOCIATED);
	csr_roam_complete(mac, eCsrNothingToJoin, NULL, pSmeJoinRsp->vdev_id);
}

static QDF_STATUS csr_roam_issue_join(struct mac_context *mac, uint32_t sessionId,
				      struct bss_description *pSirBssDesc,
				      tDot11fBeaconIEs *pIes,
				      struct csr_roam_profile *pProfile,
				      uint32_t roamId)
{
	QDF_STATUS status;

	/* Set the roaming substate to 'join attempt'... */
	csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_JOIN_REQ, sessionId);
	/* attempt to Join this BSS... */
	status = csr_send_join_req_msg(mac, sessionId, pSirBssDesc, pProfile,
					pIes, eWNI_SME_JOIN_REQ);
	return status;
}

static void
csr_roam_reissue_roam_command(struct mac_context *mac, uint8_t session_id)
{
	tListElem *pEntry;
	tSmeCmd *pCommand;
	struct csr_roam_info *roam_info;
	uint32_t sessionId;
	struct csr_roam_session *pSession;

	pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (!pEntry) {
		sme_err("Disassoc rsp can't continue, no active CMD");
		return;
	}
	pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
	if (eSmeCommandRoam != pCommand->command) {
		sme_err("Active cmd, is not a roaming CMD");
		return;
	}
	sessionId = pCommand->vdev_id;
	pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}

	if (!pCommand->u.roamCmd.fStopWds) {
		if (pSession->bRefAssocStartCnt > 0) {
			/*
			 * bRefAssocStartCnt was incremented in
			 * csr_roam_join_next_bss when the roam command issued
			 * previously. As part of reissuing the roam command
			 * again csr_roam_join_next_bss is going increment
			 * RefAssocStartCnt. So make sure to decrement the
			 * bRefAssocStartCnt
			 */
			pSession->bRefAssocStartCnt--;
		}
		if (eCsrStopRoaming == csr_roam_join_next_bss(mac, pCommand,
							      true)) {
			sme_warn("Failed to reissue join command");
			csr_roam_complete(mac, eCsrNothingToJoin, NULL,
					session_id);
		}
		return;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	roam_info->bss_desc = pCommand->u.roamCmd.pLastRoamBss;
	roam_info->status_code = pSession->joinFailStatusCode.status_code;
	roam_info->reasonCode = pSession->joinFailStatusCode.reasonCode;
	pSession->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_DISCONNECTED;
	csr_roam_call_callback(mac, sessionId, roam_info,
			       pCommand->u.roamCmd.roamId,
			       eCSR_ROAM_INFRA_IND,
			       eCSR_ROAM_RESULT_INFRA_DISASSOCIATED);

	if (!QDF_IS_STATUS_SUCCESS(csr_roam_issue_stop_bss(mac, sessionId,
					eCSR_ROAM_SUBSTATE_STOP_BSS_REQ))) {
		sme_err("Failed to reissue stop_bss command for WDS");
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, session_id);
	}
	qdf_mem_free(roam_info);
}

bool csr_is_roam_command_waiting_for_session(struct mac_context *mac,
						uint32_t vdev_id)
{
	bool fRet = false;
	tListElem *pEntry;
	tSmeCmd *pCommand = NULL;

	pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_NOLOCK);
	if (pEntry) {
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
		if ((eSmeCommandRoam == pCommand->command)
		    && (vdev_id == pCommand->vdev_id)) {
			fRet = true;
		}
	}
	if (false == fRet) {
		pEntry = csr_nonscan_pending_ll_peek_head(mac,
					 LL_ACCESS_NOLOCK);
		while (pEntry) {
			pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
			if ((eSmeCommandRoam == pCommand->command)
			    && (vdev_id == pCommand->vdev_id)) {
				fRet = true;
				break;
			}
			pEntry = csr_nonscan_pending_ll_next(mac, pEntry,
							LL_ACCESS_NOLOCK);
		}
	}

	return fRet;
}

static void
csr_roaming_state_config_cnf_processor(struct mac_context *mac_ctx,
				       tSmeCmd *cmd, uint8_t vdev_id)
{
	struct tag_csrscan_result *scan_result = NULL;
	struct bss_description *bss_desc = NULL;
	uint32_t session_id;
	struct csr_roam_session *session;
	tDot11fBeaconIEs *local_ies = NULL;
	bool is_ies_malloced = false;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!cmd) {
		sme_err("given sme cmd is null");
		return;
	}
	session_id = cmd->vdev_id;
	session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("session %d not found", session_id);
		return;
	}

	if (CSR_IS_ROAMING(session) && session->fCancelRoaming) {
		/* the roaming is cancelled. Simply complete the command */
		sme_warn("Roam command canceled");
		csr_roam_complete(mac_ctx, eCsrNothingToJoin, NULL, vdev_id);
		return;
	}

	/*
	 * Successfully set the configuration parameters for the new Bss.
	 * Attempt to join the roaming Bss
	 */
	if (cmd->u.roamCmd.pRoamBssEntry) {
		scan_result = GET_BASE_ADDR(cmd->u.roamCmd.pRoamBssEntry,
					    struct tag_csrscan_result,
					    Link);
		bss_desc = &scan_result->Result.BssDescriptor;
	}
	if (CSR_IS_INFRA_AP(&cmd->u.roamCmd.roamProfile) ||
	    CSR_IS_NDI(&cmd->u.roamCmd.roamProfile)) {
		if (!QDF_IS_STATUS_SUCCESS(csr_roam_issue_start_bss(mac_ctx,
						session_id, &session->bssParams,
						&cmd->u.roamCmd.roamProfile,
						bss_desc,
						cmd->u.roamCmd.roamId))) {
			sme_err("CSR start BSS failed");
			/* We need to complete the command */
			csr_roam_complete(mac_ctx, eCsrStartBssFailure, NULL,
					  vdev_id);
		}
		return;
	}

	if (!cmd->u.roamCmd.pRoamBssEntry) {
		sme_err("pRoamBssEntry is NULL");
		/* We need to complete the command */
		csr_roam_complete(mac_ctx, eCsrJoinFailure, NULL, vdev_id);
		return;
	}

	if (!scan_result) {
		/* If we are roaming TO an Infrastructure BSS... */
		QDF_ASSERT(scan_result);
		csr_roam_complete(mac_ctx, eCsrJoinFailure, NULL, vdev_id);
		return;
	}

	if (!csr_is_infra_bss_desc(bss_desc)) {
		sme_warn("found BSSType mismatching the one in BSS descp");
		/* here we allow the connection even if ess is not set.
		 * previously it return directly which cause serialization cmd
		 * timeout.
		 */
	}

	local_ies = (tDot11fBeaconIEs *) scan_result->Result.pvIes;
	if (!local_ies) {
		status = csr_get_parsed_bss_description_ies(mac_ctx, bss_desc,
							    &local_ies);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			csr_roam(mac_ctx, cmd, false);
			return;
		}
		is_ies_malloced = true;
	}

	if (csr_is_conn_state_connected_infra(mac_ctx, session_id)) {
		if (csr_is_ssid_equal(mac_ctx, session->pConnectBssDesc,
				      bss_desc, local_ies)) {
			cmd->u.roamCmd.fReassoc = true;
			csr_roam_issue_reassociate(mac_ctx, session_id,
						   bss_desc, local_ies,
						   &cmd->u.roamCmd.roamProfile);
		} else {
			/*
			 * otherwise, we have to issue a new Join request to LIM
			 * because we disassociated from the previously
			 * associated AP.
			 */
			status = csr_roam_issue_join(mac_ctx, session_id,
					bss_desc, local_ies,
					&cmd->u.roamCmd.roamProfile,
					cmd->u.roamCmd.roamId);
			if (!QDF_IS_STATUS_SUCCESS(status)) {
				/* try something else */
				csr_roam(mac_ctx, cmd, false);
			}
		}
	} else {
		status = QDF_STATUS_SUCCESS;
		/*
		 * We need to come with other way to figure out that this is
		 * because of HO in BMP The below API will be only available for
		 * Android as it uses a different HO algorithm. Reassoc request
		 * will be used only for ESE and 11r handoff whereas other
		 * legacy roaming should use join request
		 */
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)
		    && csr_roam_is11r_assoc(mac_ctx, session_id)) {
			status = csr_roam_issue_reassociate(mac_ctx,
					session_id, bss_desc,
					local_ies,
					&cmd->u.roamCmd.roamProfile);
		} else
#ifdef FEATURE_WLAN_ESE
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)
		   && csr_roam_is_ese_assoc(mac_ctx, session_id)) {
			/* Now serialize the reassoc command. */
			status = csr_roam_issue_reassociate_cmd(mac_ctx,
								session_id);
		} else
#endif
		if (csr_roam_is_handoff_in_progress(mac_ctx, session_id)
		   && csr_roam_is_fast_roam_enabled(mac_ctx, session_id)) {
			/* Now serialize the reassoc command. */
			status = csr_roam_issue_reassociate_cmd(mac_ctx,
								session_id);
		} else {
			/*
			 * else we are not connected and attempting to Join.
			 * Issue the Join request.
			 */
			status = csr_roam_issue_join(mac_ctx, session_id,
						    bss_desc,
						    local_ies,
						    &cmd->u.roamCmd.roamProfile,
						    cmd->u.roamCmd.roamId);
		}
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			/* try something else */
			csr_roam(mac_ctx, cmd, false);
		}
	}
	if (is_ies_malloced) {
		/* Locally allocated */
		qdf_mem_free(local_ies);
	}
}

static void csr_roam_roaming_state_reassoc_rsp_processor(struct mac_context *mac,
						struct join_rsp *pSmeJoinRsp)
{
	enum csr_roamcomplete_result result;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo = NULL;
	struct csr_roam_session *csr_session;

	if (pSmeJoinRsp->vdev_id >= WLAN_MAX_VDEVS) {
		sme_err("Invalid session ID received %d", pSmeJoinRsp->vdev_id);
		return;
	}

	pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[pSmeJoinRsp->vdev_id];
	if (eSIR_SME_SUCCESS == pSmeJoinRsp->status_code) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			 "CSR SmeReassocReq Successful");
		result = eCsrReassocSuccess;
		csr_session = CSR_GET_SESSION(mac, pSmeJoinRsp->vdev_id);
		if (csr_session) {
			if (pSmeJoinRsp->nss < csr_session->nss) {
				csr_session->nss = pSmeJoinRsp->nss;
				csr_session->vdev_nss = pSmeJoinRsp->nss;
			}
			csr_session->supported_nss_1x1 =
				pSmeJoinRsp->supported_nss_1x1;
			sme_debug("SME session supported nss: %d",
				csr_session->supported_nss_1x1);
		}
		/*
		 * Since the neighbor roam algorithm uses reassoc req for
		 * handoff instead of join, we need the response contents while
		 * processing the result in csr_roam_process_results()
		 */
		if (csr_roam_is_handoff_in_progress(mac,
						pSmeJoinRsp->vdev_id)) {
			/* Need to dig more on indicating events to
			 * SME QoS module
			 */
			sme_qos_csr_event_ind(mac, pSmeJoinRsp->vdev_id,
					    SME_QOS_CSR_HANDOFF_COMPLETE, NULL);
			csr_roam_complete(mac, result, pSmeJoinRsp,
					pSmeJoinRsp->vdev_id);
		} else {
			csr_roam_complete(mac, result, NULL,
					pSmeJoinRsp->vdev_id);
		}
	}
	/* Should we handle this similar to handling the join failure? Is it ok
	 * to call csr_roam_complete() with state as CsrJoinFailure
	 */
	else {
		sme_warn(
			"CSR SmeReassocReq failed with status_code= 0x%08X [%d]",
			pSmeJoinRsp->status_code, pSmeJoinRsp->status_code);
		result = eCsrReassocFailure;
		cds_flush_logs(WLAN_LOG_TYPE_FATAL,
			WLAN_LOG_INDICATOR_HOST_DRIVER,
			WLAN_LOG_REASON_ROAM_FAIL,
			false, false);
		if ((eSIR_SME_FT_REASSOC_TIMEOUT_FAILURE ==
		     pSmeJoinRsp->status_code)
		    || (eSIR_SME_FT_REASSOC_FAILURE ==
			pSmeJoinRsp->status_code)
		    || (eSIR_SME_INVALID_PARAMETERS ==
			pSmeJoinRsp->status_code)) {
			/* Inform HDD to turn off FT flag in HDD */
			if (pNeighborRoamInfo) {
				struct csr_roam_info *roam_info;
				uint32_t roam_id = 0;

				roam_info = qdf_mem_malloc(sizeof(*roam_info));
				if (!roam_info)
					return;
				mlme_set_discon_reason_n_from_ap(mac->psoc,
					pSmeJoinRsp->vdev_id, false,
					REASON_HOST_TRIGGERED_ROAM_FAILURE);
				csr_roam_call_callback(mac,
						       pSmeJoinRsp->vdev_id,
						       roam_info, roam_id,
						    eCSR_ROAM_FT_REASSOC_FAILED,
						    eCSR_ROAM_RESULT_SUCCESS);
				/*
				 * Since the above callback sends a disconnect
				 * to HDD, we should clean-up our state
				 * machine as well to be in sync with the upper
				 * layers. There is no need to send a disassoc
				 * since: 1) we will never reassoc to the
				 * current AP in LFR, and 2) there is no need
				 * to issue a disassoc to the AP with which we
				 * were trying to reassoc.
				 */
				csr_roam_complete(mac, eCsrJoinFailure, NULL,
						pSmeJoinRsp->vdev_id);
				qdf_mem_free(roam_info);
				return;
			}
		}
		/* In the event that the Reassociation fails, then we need to
		 * Disassociate the current association and keep roaming. Note
		 * that we will attempt to Join the AP instead of a Reassoc
		 * since we may have attempted a 'Reassoc to self', which AP's
		 * that don't support Reassoc will force a Disassoc. The
		 * isassoc rsp message will remove the command from active list
		 */
		if (!QDF_IS_STATUS_SUCCESS
			    (csr_roam_issue_disassociate
				    (mac, pSmeJoinRsp->vdev_id,
				    eCSR_ROAM_SUBSTATE_DISASSOC_REASSOC_FAILURE,
				false))) {
			csr_roam_complete(mac, eCsrJoinFailure, NULL,
					pSmeJoinRsp->vdev_id);
		}
	}
}

static void csr_roam_roaming_state_stop_bss_rsp_processor(struct mac_context *mac,
							  tSirSmeRsp *pSmeRsp)
{
	enum csr_roamcomplete_result result_code = eCsrNothingToJoin;
	struct csr_roam_profile *profile;

	mac->roam.roamSession[pSmeRsp->vdev_id].connectState =
		eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	if (CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ(mac, pSmeRsp->vdev_id)) {
		profile =
		    mac->roam.roamSession[pSmeRsp->vdev_id].pCurRoamProfile;
		if (profile && CSR_IS_CONN_NDI(profile)) {
			result_code = eCsrStopBssSuccess;
			if (pSmeRsp->status_code != eSIR_SME_SUCCESS)
				result_code = eCsrStopBssFailure;
		}
		csr_roam_complete(mac, result_code, NULL, pSmeRsp->vdev_id);
	} else if (CSR_IS_ROAM_SUBSTATE_DISCONNECT_CONTINUE(mac,
			pSmeRsp->vdev_id)) {
		csr_roam_reissue_roam_command(mac, pSmeRsp->vdev_id);
	}
}

#ifdef WLAN_FEATURE_HOST_ROAM
/**
 * csr_dequeue_command() - removes a command from active cmd list
 * @mac:          mac global context
 *
 * Return: void
 */
static void
csr_dequeue_command(struct mac_context *mac_ctx)
{
	bool fRemoveCmd;
	tSmeCmd *cmd = NULL;
	tListElem *entry = csr_nonscan_active_ll_peek_head(mac_ctx,
					    LL_ACCESS_LOCK);
	if (!entry) {
		sme_err("NO commands are active");
		return;
	}

	cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
	/*
	 * If the head of the queue is Active and it is a given cmd type, remove
	 * and put this on the Free queue.
	 */
	if (eSmeCommandRoam != cmd->command) {
		sme_err("Roam command not active");
		return;
	}
	/*
	 * we need to process the result first before removing it from active
	 * list because state changes still happening insides
	 * roamQProcessRoamResults so no other roam command should be issued.
	 */
	fRemoveCmd = csr_nonscan_active_ll_remove_entry(mac_ctx, entry,
					 LL_ACCESS_LOCK);
	if (cmd->u.roamCmd.fReleaseProfile) {
		csr_release_profile(mac_ctx, &cmd->u.roamCmd.roamProfile);
		cmd->u.roamCmd.fReleaseProfile = false;
	}
	if (fRemoveCmd)
		csr_release_command(mac_ctx, cmd);
	else
		sme_err("fail to remove cmd reason %d",
			cmd->u.roamCmd.roamReason);
}

/**
 * csr_post_roam_failure() - post roam failure back to csr and issues a disassoc
 * @mac:               mac global context
 * @session_id:         session id
 * @roam_info:          roam info struct
 * @scan_filter:        scan filter to free
 * @cur_roam_profile:   current csr roam profile
 *
 * Return: void
 */
static void
csr_post_roam_failure(struct mac_context *mac_ctx,
		      uint32_t session_id,
		      struct csr_roam_info *roam_info,
		      struct csr_roam_profile *cur_roam_profile)
{
	QDF_STATUS status;

	if (cur_roam_profile)
		qdf_mem_free(cur_roam_profile);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
		csr_roam_synch_clean_up(mac_ctx, session_id);
#endif
	/* Inform the upper layers that the reassoc failed */
	qdf_mem_zero(roam_info, sizeof(struct csr_roam_info));
	csr_roam_call_callback(mac_ctx, session_id, roam_info, 0,
			       eCSR_ROAM_FT_REASSOC_FAILED,
			       eCSR_ROAM_RESULT_SUCCESS);
	/*
	 * Issue a disassoc request so that PE/LIM uses this to clean-up the FT
	 * session. Upon success, we would re-enter this routine after receiving
	 * the disassoc response and will fall into the reassoc fail sub-state.
	 * And, eventually call csr_roam_complete which would remove the roam
	 * command from SME active queue.
	 */
	status = csr_roam_issue_disassociate(mac_ctx, session_id,
			eCSR_ROAM_SUBSTATE_DISASSOC_REASSOC_FAILURE, false);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err(
			"csr_roam_issue_disassociate failed, status %d",
			status);
		csr_roam_complete(mac_ctx, eCsrJoinFailure, NULL, session_id);
	}
}

/**
 * csr_check_profile_in_scan_cache() - finds if roam profile is present in scan
 * cache or not
 * @mac:                  mac global context
 * @neighbor_roam_info:    roam info struct
 * @hBSSList:              scan result
 * @vdev_id: vdev id
 *
 * Return: true if found else false.
 */
static bool
csr_check_profile_in_scan_cache(struct mac_context *mac_ctx,
				tpCsrNeighborRoamControlInfo neighbor_roam_info,
				tScanResultHandle *hBSSList, uint8_t vdev_id)
{
	QDF_STATUS status;
	struct scan_filter *scan_filter;

	scan_filter = qdf_mem_malloc(sizeof(*scan_filter));
	if (!scan_filter)
		return false;

	status = csr_roam_get_scan_filter_from_profile(mac_ctx,
			&neighbor_roam_info->csrNeighborRoamProfile,
			scan_filter, true, vdev_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err(
			"failed to prepare scan filter, status %d",
			status);
		qdf_mem_free(scan_filter);
		return false;
	}
	status = csr_scan_get_result(mac_ctx, scan_filter, hBSSList, true);
	qdf_mem_free(scan_filter);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err(
			"csr_scan_get_result failed, status %d",
			status);
		return false;
	}
	return true;
}

static
QDF_STATUS csr_roam_lfr2_issue_connect(struct mac_context *mac,
				uint32_t session_id,
				struct scan_result_list *hbss_list,
				uint32_t roam_id)
{
	struct csr_roam_profile *cur_roam_profile = NULL;
	struct csr_roam_session *session;
	QDF_STATUS status;

	session = CSR_GET_SESSION(mac, session_id);
	if (!session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"session is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	/*
	 * Copy the connected profile to apply the same for this
	 * connection as well
	 */
	cur_roam_profile = qdf_mem_malloc(sizeof(*cur_roam_profile));
	if (cur_roam_profile) {
		/*
		 * notify sub-modules like QoS etc. that handoff
		 * happening
		 */
		sme_qos_csr_event_ind(mac, session_id,
				      SME_QOS_CSR_HANDOFF_ASSOC_REQ,
				      NULL);
		csr_roam_copy_profile(mac, cur_roam_profile,
				      session->pCurRoamProfile, session_id);
		/* make sure to put it at the head of the cmd queue */
		status = csr_roam_issue_connect(mac, session_id,
				cur_roam_profile, hbss_list,
				eCsrSmeIssuedAssocToSimilarAP,
				roam_id, true, false);
		if (!QDF_IS_STATUS_SUCCESS(status))
			sme_err(
				"issue_connect failed. status %d",
				status);

		csr_release_profile(mac, cur_roam_profile);
		qdf_mem_free(cur_roam_profile);
		return QDF_STATUS_SUCCESS;
	} else {
		sme_err("malloc failed");
		QDF_ASSERT(0);
		csr_dequeue_command(mac);
	}
	return QDF_STATUS_E_NOMEM;
}

QDF_STATUS csr_continue_lfr2_connect(struct mac_context *mac,
				      uint32_t session_id)
{
	uint32_t roam_id = 0;
	struct csr_roam_info *roam_info;
	struct scan_result_list *scan_handle_roam_ap;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	scan_handle_roam_ap =
		mac->roam.neighborRoamInfo[session_id].scan_res_lfr2_roam_ap;
	if (!scan_handle_roam_ap) {
		sme_err("no roam target ap");
		goto POST_ROAM_FAILURE;
	}
	if(is_disconnect_pending(mac, session_id)) {
		sme_err("disconnect pending");
		goto purge_scan_result;
	}

	status = csr_roam_lfr2_issue_connect(mac, session_id,
						scan_handle_roam_ap,
						roam_id);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_free(roam_info);
		return status;
	}

purge_scan_result:
	csr_scan_result_purge(mac, scan_handle_roam_ap);

POST_ROAM_FAILURE:
	csr_post_roam_failure(mac, session_id, roam_info, NULL);
	qdf_mem_free(roam_info);
	return status;
}

static
void csr_handle_disassoc_ho(struct mac_context *mac, uint32_t session_id)
{
	uint32_t roam_id = 0;
	struct csr_roam_info *roam_info;
	struct sCsrNeighborRoamControlInfo *neighbor_roam_info = NULL;
	struct scan_result_list *scan_handle_roam_ap;
	struct sCsrNeighborRoamBSSInfo *bss_node;
	QDF_STATUS status;

	csr_dequeue_command(mac);
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		 "CSR SmeDisassocReq due to HO on session %d", session_id);
	neighbor_roam_info = &mac->roam.neighborRoamInfo[session_id];

	/*
	 * First ensure if the roam profile is in the scan cache.
	 * If not, post a reassoc failure and disconnect.
	 */
	if (!csr_check_profile_in_scan_cache(mac, neighbor_roam_info,
			     (tScanResultHandle *)&scan_handle_roam_ap,
			     session_id))
		goto POST_ROAM_FAILURE;

	/* notify HDD about handoff and provide the BSSID too */
	roam_info->reasonCode = eCsrRoamReasonBetterAP;

	qdf_copy_macaddr(&roam_info->bssid,
		 neighbor_roam_info->csrNeighborRoamProfile.BSSIDs.bssid);

	/*
	 * For LFR2, removal of policy mgr entry for disassociated
	 * AP is handled in eCSR_ROAM_ROAMING_START.
	 * eCSR_ROAM_RESULT_NOT_ASSOCIATED is sent to differentiate
	 * eCSR_ROAM_ROAMING_START sent after FT preauth success
	 */
	csr_roam_call_callback(mac, session_id, roam_info, 0,
			       eCSR_ROAM_ROAMING_START,
			       eCSR_ROAM_RESULT_NOT_ASSOCIATED);

	bss_node = csr_neighbor_roam_next_roamable_ap(mac,
		&neighbor_roam_info->FTRoamInfo.preAuthDoneList,
		NULL);
	if (!bss_node) {
		sme_debug("LFR2DBG: bss_node is NULL");
		goto POST_ROAM_FAILURE;
	}

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		  "LFR2DBG: preauthed bss_node->pBssDescription BSSID"\
		  QDF_MAC_ADDR_FMT",freq:%d",
		  QDF_MAC_ADDR_REF(bss_node->pBssDescription->bssId),
		  bss_node->pBssDescription->chan_freq);

	status = policy_mgr_handle_conc_multiport(
			mac->psoc, session_id,
			bss_node->pBssDescription->chan_freq,
			POLICY_MGR_UPDATE_REASON_LFR2_ROAM,
			POLICY_MGR_DEF_REQ_ID);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		mac->roam.neighborRoamInfo[session_id].scan_res_lfr2_roam_ap =
							scan_handle_roam_ap;
		/*if hw_mode change is required then handle roam
		* issue connect in mode change response handler
		*/
		qdf_mem_free(roam_info);
		return;
	} else if (status == QDF_STATUS_E_FAILURE)
		goto POST_ROAM_FAILURE;

	status = csr_roam_lfr2_issue_connect(mac, session_id,
					     scan_handle_roam_ap,
					     roam_id);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_free(roam_info);
		return;
	}
	csr_scan_result_purge(mac, scan_handle_roam_ap);

POST_ROAM_FAILURE:
	mlme_set_discon_reason_n_from_ap(mac->psoc, session_id, false,
					 REASON_HOST_TRIGGERED_ROAM_FAILURE);
	csr_post_roam_failure(mac, session_id, roam_info, NULL);
	qdf_mem_free(roam_info);
}
#else
static
void csr_handle_disassoc_ho(struct mac_context *mac, uint32_t session_id)
{
	return;
}
#endif

static
void csr_roam_roaming_state_disassoc_rsp_processor(struct mac_context *mac,
						   struct disassoc_rsp *rsp)
{
	uint32_t sessionId;
	struct csr_roam_session *pSession;

	sessionId = rsp->sessionId;
	sme_debug("sessionId %d", sessionId);

	if (csr_is_conn_state_infra(mac, sessionId)) {
		mac->roam.roamSession[sessionId].connectState =
			eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	}

	pSession = CSR_GET_SESSION(mac, sessionId);
	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}


	if (CSR_IS_ROAM_SUBSTATE_DISASSOC_NO_JOIN(mac, sessionId)) {
		sme_debug("***eCsrNothingToJoin***");
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, sessionId);
	} else if (CSR_IS_ROAM_SUBSTATE_DISASSOC_FORCED(mac, sessionId) ||
		   CSR_IS_ROAM_SUBSTATE_DISASSOC_REQ(mac, sessionId)) {
		if (eSIR_SME_SUCCESS == rsp->status_code) {
			sme_debug("CSR force disassociated successful");
			/*
			 * A callback to HDD will be issued from
			 * csr_roam_complete so no need to do anything here
			 */
		}
		csr_roam_complete(mac, eCsrNothingToJoin, NULL, sessionId);
	} else if (CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(mac, sessionId)) {
		csr_handle_disassoc_ho(mac, sessionId);
	} /* else if ( CSR_IS_ROAM_SUBSTATE_DISASSOC_HO( mac ) ) */
	else if (CSR_IS_ROAM_SUBSTATE_REASSOC_FAIL(mac, sessionId)) {
		/* Disassoc due to Reassoc failure falls into this codepath */
		csr_roam_complete(mac, eCsrJoinFailure, NULL, sessionId);
	} else {
		if (eSIR_SME_SUCCESS == rsp->status_code) {
			/*
			 * Successfully disassociated from the 'old' Bss.
			 * We get Disassociate response in three conditions.
			 * Where we are doing an Infra to Infra roam between
			 *    networks with different SSIDs.
			 * In all cases, we set the new Bss configuration here
			 * and attempt to join
			 */
			sme_debug("Disassociated successfully");
		} else {
			sme_err("DisassocReq failed, status_code= 0x%08X",
				rsp->status_code);
		}
		/* We are not done yet. Get the data and continue roaming */
		csr_roam_reissue_roam_command(mac, sessionId);
	}
}

static void csr_roam_roaming_state_deauth_rsp_processor(struct mac_context *mac,
						struct deauth_rsp *pSmeRsp)
{
	tSirResultCodes status_code;
	status_code = csr_get_de_auth_rsp_status_code(pSmeRsp);
	mac->roam.deauthRspStatus = status_code;
	if (CSR_IS_ROAM_SUBSTATE_DEAUTH_REQ(mac, pSmeRsp->sessionId)) {
		csr_roam_complete(mac, eCsrNothingToJoin, NULL,
				pSmeRsp->sessionId);
	} else {
		if (eSIR_SME_SUCCESS == status_code) {
			/* Successfully deauth from the 'old' Bss... */
			/* */
			sme_debug(
				"CSR SmeDeauthReq disassociated Successfully");
		}
		/* We are not done yet. Get the data and continue roaming */
		csr_roam_reissue_roam_command(mac, pSmeRsp->sessionId);
	}
}

static void
csr_roam_roaming_state_start_bss_rsp_processor(struct mac_context *mac,
					struct start_bss_rsp *pSmeStartBssRsp)
{
	enum csr_roamcomplete_result result;

	if (eSIR_SME_SUCCESS == pSmeStartBssRsp->status_code) {
		sme_debug("SmeStartBssReq Successful");
		result = eCsrStartBssSuccess;
	} else {
		sme_warn("SmeStartBssReq failed with status_code= 0x%08X",
			 pSmeStartBssRsp->status_code);
		/* Let csr_roam_complete decide what to do */
		result = eCsrStartBssFailure;
	}
	csr_roam_complete(mac, result, pSmeStartBssRsp,
				pSmeStartBssRsp->sessionId);
}

/**
 * csr_roam_send_disconnect_done_indication() - Send disconnect ind to HDD.
 *
 * @mac_ctx: mac global context
 * @msg_ptr: incoming message
 *
 * This function gives final disconnect event to HDD after all cleanup in
 * lower layers is done.
 *
 * Return: None
 */
static void
csr_roam_send_disconnect_done_indication(struct mac_context *mac_ctx,
					 tSirSmeRsp *msg_ptr)
{
	struct sir_sme_discon_done_ind *discon_ind =
				(struct sir_sme_discon_done_ind *)(msg_ptr);
	struct csr_roam_info *roam_info;
	struct csr_roam_session *session;
	struct wlan_objmgr_vdev *vdev;
	uint8_t vdev_id;

	vdev_id = discon_ind->session_id;
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;

	sme_debug("DISCONNECT_DONE_IND RC:%d", discon_ind->reason_code);

	if (CSR_IS_SESSION_VALID(mac_ctx, vdev_id)) {
		roam_info->reasonCode = discon_ind->reason_code;
		roam_info->status_code = eSIR_SME_STA_NOT_ASSOCIATED;
		qdf_mem_copy(roam_info->peerMac.bytes, discon_ind->peer_mac,
			     ETH_ALEN);

		roam_info->rssi = mac_ctx->peer_rssi;
		roam_info->tx_rate = mac_ctx->peer_txrate;
		roam_info->rx_rate = mac_ctx->peer_rxrate;
		roam_info->disassoc_reason = discon_ind->reason_code;
		roam_info->rx_mc_bc_cnt = mac_ctx->rx_mc_bc_cnt;
		roam_info->rx_retry_cnt = mac_ctx->rx_retry_cnt;
		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
							    vdev_id,
							    WLAN_LEGACY_SME_ID);
		if (vdev)
			roam_info->disconnect_ies =
				mlme_get_peer_disconnect_ies(vdev);

		csr_roam_call_callback(mac_ctx, vdev_id,
				       roam_info, 0, eCSR_ROAM_LOSTLINK,
				       eCSR_ROAM_RESULT_DISASSOC_IND);
		if (vdev) {
			mlme_free_peer_disconnect_ies(vdev);
			wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		}
		session = CSR_GET_SESSION(mac_ctx, vdev_id);
		if (!CSR_IS_INFRA_AP(&session->connectedProfile))
			csr_roam_state_change(mac_ctx, eCSR_ROAMING_STATE_IDLE,
					      vdev_id);

		if (CSR_IS_INFRASTRUCTURE(&session->connectedProfile)) {
			csr_free_roam_profile(mac_ctx, vdev_id);
			csr_free_connect_bss_desc(mac_ctx, vdev_id);
			csr_roam_free_connect_profile(
						&session->connectedProfile);
			csr_roam_free_connected_info(mac_ctx,
						     &session->connectedInfo);
		}

	} else {
		sme_err("Inactive vdev_id %d", vdev_id);
	}

	/*
	 * Release WM status change command as eWNI_SME_DISCONNECT_DONE_IND
	 * has been sent to HDD and there is nothing else left to do.
	 */
	csr_roam_wm_status_change_complete(mac_ctx, vdev_id);
	qdf_mem_free(roam_info);
}

/**
 * csr_roaming_state_msg_processor() - process roaming messages
 * @mac:       mac global context
 * @msg_buf:    message buffer
 *
 * We need to be careful on whether to cast msg_buf (pSmeRsp) to other type of
 * strucutres. It depends on how the message is constructed. If the message is
 * sent by lim_send_sme_rsp, the msg_buf is only a generic response and can only
 * be used as pointer to tSirSmeRsp. For the messages where sender allocates
 * memory for specific structures, then it can be cast accordingly.
 *
 * Return: status of operation
 */
void csr_roaming_state_msg_processor(struct mac_context *mac, void *msg_buf)
{
	tSirSmeRsp *pSmeRsp;

	pSmeRsp = (tSirSmeRsp *)msg_buf;

	switch (pSmeRsp->messageType) {

	case eWNI_SME_JOIN_RSP:
		/* in Roaming state, process the Join response message... */
		if (CSR_IS_ROAM_SUBSTATE_JOIN_REQ(mac, pSmeRsp->vdev_id))
			/* We sent a JOIN_REQ */
			csr_roam_join_rsp_processor(mac,
						    (struct join_rsp *)pSmeRsp);
		break;
	case eWNI_SME_REASSOC_RSP:
		/* or the Reassociation response message... */
		if (CSR_IS_ROAM_SUBSTATE_REASSOC_REQ(mac, pSmeRsp->vdev_id))
			csr_roam_roaming_state_reassoc_rsp_processor(mac,
						(struct join_rsp *)pSmeRsp);
		break;
	case eWNI_SME_STOP_BSS_RSP:
		/* or the Stop Bss response message... */
		csr_roam_roaming_state_stop_bss_rsp_processor(mac, pSmeRsp);
		break;
	case eWNI_SME_DISASSOC_RSP:
		/* or the Disassociate response message... */
		if (CSR_IS_ROAM_SUBSTATE_DISASSOC_REQ(mac, pSmeRsp->vdev_id)
		    || CSR_IS_ROAM_SUBSTATE_DISASSOC_NO_JOIN(mac,
							pSmeRsp->vdev_id)
		    || CSR_IS_ROAM_SUBSTATE_REASSOC_FAIL(mac,
							pSmeRsp->vdev_id)
		    || CSR_IS_ROAM_SUBSTATE_DISASSOC_FORCED(mac,
							pSmeRsp->vdev_id)
		    || CSR_IS_ROAM_SUBSTATE_DISCONNECT_CONTINUE(mac,
							pSmeRsp->vdev_id)
		    || CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(mac,
							pSmeRsp->vdev_id)) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
				 "eWNI_SME_DISASSOC_RSP subState = %s",
				  mac_trace_getcsr_roam_sub_state(
				  mac->roam.curSubState[pSmeRsp->vdev_id]));
			csr_roam_roaming_state_disassoc_rsp_processor(mac,
						(struct disassoc_rsp *) pSmeRsp);
		}
		break;
	case eWNI_SME_DEAUTH_RSP:
		/* or the Deauthentication response message... */
		if (CSR_IS_ROAM_SUBSTATE_DEAUTH_REQ(mac, pSmeRsp->vdev_id))
			csr_roam_roaming_state_deauth_rsp_processor(mac,
						(struct deauth_rsp *) pSmeRsp);
		break;
	case eWNI_SME_START_BSS_RSP:
		/* or the Start BSS response message... */
		if (CSR_IS_ROAM_SUBSTATE_START_BSS_REQ(mac,
						       pSmeRsp->vdev_id))
			csr_roam_roaming_state_start_bss_rsp_processor(mac,
					(struct start_bss_rsp *)pSmeRsp);
		break;
	/* In case CSR issues STOP_BSS, we need to tell HDD about peer departed
	 * because PE is removing them
	 */
	case eWNI_SME_TRIGGER_SAE:
		sme_debug("Invoke SAE callback");
		csr_sae_callback(mac, pSmeRsp);
		break;

	case eWNI_SME_SETCONTEXT_RSP:
		csr_roam_check_for_link_status_change(mac, pSmeRsp);
		break;

	case eWNI_SME_DISCONNECT_DONE_IND:
		csr_roam_send_disconnect_done_indication(mac, pSmeRsp);
		break;

	case eWNI_SME_UPPER_LAYER_ASSOC_CNF:
		csr_roam_joined_state_msg_processor(mac, pSmeRsp);
		break;
	default:
		sme_debug("Unexpected message type: %d[0x%X] received in substate %s",
			pSmeRsp->messageType, pSmeRsp->messageType,
			mac_trace_getcsr_roam_sub_state(
				mac->roam.curSubState[pSmeRsp->vdev_id]));
		/* If we are connected, check the link status change */
		if (!csr_is_conn_state_disconnected(mac, pSmeRsp->vdev_id))
			csr_roam_check_for_link_status_change(mac, pSmeRsp);
		break;
	}
}

void csr_roam_joined_state_msg_processor(struct mac_context *mac, void *msg_buf)
{
	tSirSmeRsp *pSirMsg = (tSirSmeRsp *)msg_buf;

	switch (pSirMsg->messageType) {
	case eWNI_SME_UPPER_LAYER_ASSOC_CNF:
	{
		struct csr_roam_session *pSession;
		tSirSmeAssocIndToUpperLayerCnf *pUpperLayerAssocCnf;
		struct csr_roam_info *roam_info;
		uint32_t sessionId;
		QDF_STATUS status;

		sme_debug("ASSOCIATION confirmation can be given to upper layer ");
		pUpperLayerAssocCnf =
			(tSirSmeAssocIndToUpperLayerCnf *)msg_buf;
		status = csr_roam_get_session_id_from_bssid(mac,
							(struct qdf_mac_addr *)
							   pUpperLayerAssocCnf->
							   bssId, &sessionId);
		pSession = CSR_GET_SESSION(mac, sessionId);

		if (!pSession) {
			sme_err("session %d not found", sessionId);
			if (pUpperLayerAssocCnf->ies)
				qdf_mem_free(pUpperLayerAssocCnf->ies);
			return;
		}

		roam_info = qdf_mem_malloc(sizeof(*roam_info));
		if (!roam_info) {
			if (pUpperLayerAssocCnf->ies)
				qdf_mem_free(pUpperLayerAssocCnf->ies);
			return;
		}
		/* send the status code as Success */
		roam_info->status_code = eSIR_SME_SUCCESS;
		roam_info->u.pConnectedProfile =
			&pSession->connectedProfile;
		roam_info->staId = (uint8_t) pUpperLayerAssocCnf->aid;
		roam_info->rsnIELen =
			(uint8_t) pUpperLayerAssocCnf->rsnIE.length;
		roam_info->prsnIE =
			pUpperLayerAssocCnf->rsnIE.rsnIEdata;
#ifdef FEATURE_WLAN_WAPI
		roam_info->wapiIELen =
			(uint8_t) pUpperLayerAssocCnf->wapiIE.length;
		roam_info->pwapiIE =
			pUpperLayerAssocCnf->wapiIE.wapiIEdata;
#endif
		roam_info->addIELen =
			(uint8_t) pUpperLayerAssocCnf->addIE.length;
		roam_info->paddIE =
			pUpperLayerAssocCnf->addIE.addIEdata;
		qdf_mem_copy(roam_info->peerMac.bytes,
			     pUpperLayerAssocCnf->peerMacAddr,
			     sizeof(tSirMacAddr));
		qdf_mem_copy(&roam_info->bssid,
			     pUpperLayerAssocCnf->bssId,
			     sizeof(struct qdf_mac_addr));
		roam_info->wmmEnabledSta =
			pUpperLayerAssocCnf->wmmEnabledSta;
		roam_info->timingMeasCap =
			pUpperLayerAssocCnf->timingMeasCap;
		qdf_mem_copy(&roam_info->chan_info,
			     &pUpperLayerAssocCnf->chan_info,
			     sizeof(struct oem_channel_info));

		roam_info->ampdu = pUpperLayerAssocCnf->ampdu;
		roam_info->sgi_enable = pUpperLayerAssocCnf->sgi_enable;
		roam_info->tx_stbc = pUpperLayerAssocCnf->tx_stbc;
		roam_info->rx_stbc = pUpperLayerAssocCnf->rx_stbc;
		roam_info->ch_width = pUpperLayerAssocCnf->ch_width;
		roam_info->mode = pUpperLayerAssocCnf->mode;
		roam_info->max_supp_idx = pUpperLayerAssocCnf->max_supp_idx;
		roam_info->max_ext_idx = pUpperLayerAssocCnf->max_ext_idx;
		roam_info->max_mcs_idx = pUpperLayerAssocCnf->max_mcs_idx;
		roam_info->rx_mcs_map = pUpperLayerAssocCnf->rx_mcs_map;
		roam_info->tx_mcs_map = pUpperLayerAssocCnf->tx_mcs_map;
		roam_info->ecsa_capable = pUpperLayerAssocCnf->ecsa_capable;
		if (pUpperLayerAssocCnf->ht_caps.present)
			roam_info->ht_caps = pUpperLayerAssocCnf->ht_caps;
		if (pUpperLayerAssocCnf->vht_caps.present)
			roam_info->vht_caps = pUpperLayerAssocCnf->vht_caps;
		roam_info->capability_info =
					pUpperLayerAssocCnf->capability_info;
		roam_info->he_caps_present =
					pUpperLayerAssocCnf->he_caps_present;

		if (CSR_IS_INFRA_AP(roam_info->u.pConnectedProfile)) {
			if (pUpperLayerAssocCnf->ies_len > 0) {
				roam_info->assocReqLength =
						pUpperLayerAssocCnf->ies_len;
				roam_info->assocReqPtr =
						pUpperLayerAssocCnf->ies;
			}

			mac->roam.roamSession[sessionId].connectState =
				eCSR_ASSOC_STATE_TYPE_INFRA_CONNECTED;
			roam_info->fReassocReq =
				pUpperLayerAssocCnf->reassocReq;
			status = csr_roam_call_callback(mac, sessionId,
						       roam_info, 0,
						       eCSR_ROAM_INFRA_IND,
					eCSR_ROAM_RESULT_INFRA_ASSOCIATION_CNF);
		}
		if (pUpperLayerAssocCnf->ies)
			qdf_mem_free(pUpperLayerAssocCnf->ies);
		qdf_mem_free(roam_info);
	}
	break;
	default:
		csr_roam_check_for_link_status_change(mac, pSirMsg);
		break;
	}
}

/**
 * csr_update_wep_key_peer_macaddr() - Update wep key peer mac addr
 * @vdev: vdev object
 * @crypto_key: crypto key info
 * @unicast: uncast or broadcast
 * @mac_addr: peer mac address
 *
 * Update peer mac address to key context before set wep key to target.
 *
 * Return void
 */
static void
csr_update_wep_key_peer_macaddr(struct wlan_objmgr_vdev *vdev,
				struct wlan_crypto_key *crypto_key,
				bool unicast,
				struct qdf_mac_addr *mac_addr)
{
	if (!crypto_key || !vdev) {
		sme_err("vdev or crytpo_key null");
		return;
	}

	if (unicast) {
		qdf_mem_copy(&crypto_key->macaddr, mac_addr,
			     QDF_MAC_ADDR_SIZE);
	} else {
		if (vdev->vdev_mlme.vdev_opmode == QDF_STA_MODE ||
		    vdev->vdev_mlme.vdev_opmode == QDF_P2P_CLIENT_MODE)
			qdf_mem_copy(&crypto_key->macaddr, mac_addr,
				     QDF_MAC_ADDR_SIZE);
		else
			qdf_mem_copy(&crypto_key->macaddr,
				     vdev->vdev_mlme.macaddr,
				     QDF_MAC_ADDR_SIZE);
	}
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void
csr_roam_diag_set_ctx_rsp(struct mac_context *mac_ctx,
			  struct csr_roam_session *session,
			  struct set_context_rsp *pRsp)
{
	WLAN_HOST_DIAG_EVENT_DEF(setKeyEvent,
				 host_event_wlan_security_payload_type);
	if (eCSR_ENCRYPT_TYPE_NONE ==
		session->connectedProfile.EncryptionType)
		return;
	qdf_mem_zero(&setKeyEvent,
		     sizeof(host_event_wlan_security_payload_type));
	if (qdf_is_macaddr_group(&pRsp->peer_macaddr))
		setKeyEvent.eventId =
			WLAN_SECURITY_EVENT_SET_BCAST_RSP;
	else
		setKeyEvent.eventId =
			WLAN_SECURITY_EVENT_SET_UNICAST_RSP;
	setKeyEvent.encryptionModeMulticast =
		(uint8_t) diag_enc_type_from_csr_type(
				session->connectedProfile.mcEncryptionType);
	setKeyEvent.encryptionModeUnicast =
		(uint8_t) diag_enc_type_from_csr_type(
				session->connectedProfile.EncryptionType);
	qdf_mem_copy(setKeyEvent.bssid, session->connectedProfile.bssid.bytes,
			QDF_MAC_ADDR_SIZE);
	setKeyEvent.authMode =
		(uint8_t) diag_auth_type_from_csr_type(
					session->connectedProfile.AuthType);
	if (eSIR_SME_SUCCESS != pRsp->status_code)
		setKeyEvent.status = WLAN_SECURITY_STATUS_FAILURE;
	WLAN_HOST_DIAG_EVENT_REPORT(&setKeyEvent, EVENT_WLAN_SECURITY);
}
#else /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
static void
csr_roam_diag_set_ctx_rsp(struct mac_context *mac_ctx,
			  struct csr_roam_session *session,
			  struct set_context_rsp *pRsp)
{
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

static void
csr_roam_chk_lnk_set_ctx_rsp(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	eCsrRoamResult result = eCSR_ROAM_RESULT_NONE;
	struct set_context_rsp *pRsp = (struct set_context_rsp *)msg_ptr;

	if (!pRsp) {
		sme_err("set key response is NULL");
		return;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sessionId = pRsp->sessionId;
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		qdf_mem_free(roam_info);
		return;
	}

	csr_roam_diag_set_ctx_rsp(mac_ctx, session, pRsp);

	if (CSR_IS_WAIT_FOR_KEY(mac_ctx, sessionId)) {
		csr_roam_stop_wait_for_key_timer(mac_ctx);
		/* We are done with authentication, whethere succeed or not */
		csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE,
					 sessionId);
		/* We do it here because this linkup function is not called
		 * after association  when a key needs to be set.
		 */
		if (csr_is_conn_state_connected_infra(mac_ctx, sessionId))
			csr_roam_link_up(mac_ctx,
					 session->connectedProfile.bssid);
	}
	if (eSIR_SME_SUCCESS == pRsp->status_code) {
		qdf_copy_macaddr(&roam_info->peerMac, &pRsp->peer_macaddr);
		/* Make sure we install the GTK before indicating to HDD as
		 * authenticated. This is to prevent broadcast packets go out
		 * after PTK and before GTK.
		 */
		if (qdf_is_macaddr_broadcast(&pRsp->peer_macaddr)) {
			/*
			 * OBSS SCAN Indication will be sent to Firmware
			 * to start OBSS Scan
			 */
			if (mac_ctx->obss_scan_offload &&
			    wlan_reg_is_24ghz_ch_freq(
					session->connectedProfile.op_freq) &&
			    (session->connectState ==
			     eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED) &&
			     session->pCurRoamProfile &&
			    (QDF_P2P_CLIENT_MODE ==
			     session->pCurRoamProfile->csrPersona ||
			    (QDF_STA_MODE ==
			     session->pCurRoamProfile->csrPersona))) {
				struct sme_obss_ht40_scanind_msg *msg;

				msg = qdf_mem_malloc(sizeof(
					struct sme_obss_ht40_scanind_msg));
				if (!msg) {
					qdf_mem_free(roam_info);
					return;
				}

				msg->msg_type = eWNI_SME_HT40_OBSS_SCAN_IND;
				msg->length =
				      sizeof(struct sme_obss_ht40_scanind_msg);
				qdf_copy_macaddr(&msg->mac_addr,
					&session->connectedProfile.bssid);
				status = umac_send_mb_message_to_mac(msg);
			}
			result = eCSR_ROAM_RESULT_AUTHENTICATED;
		} else {
			result = eCSR_ROAM_RESULT_NONE;
		}
	} else {
		result = eCSR_ROAM_RESULT_FAILURE;
		sme_err(
			"CSR: setkey command failed(err=%d) PeerMac "
			QDF_MAC_ADDR_FMT,
			pRsp->status_code,
			QDF_MAC_ADDR_REF(pRsp->peer_macaddr.bytes));
	}
	/* keeping roam_id = 0 as nobody is using roam_id for set_key */
	csr_roam_call_callback(mac_ctx, sessionId, roam_info,
			       0, eCSR_ROAM_SET_KEY_COMPLETE, result);
	/* Indicate SME_QOS that the SET_KEY is completed, so that SME_QOS
	 * can go ahead and initiate the TSPEC if any are pending
	 */
	sme_qos_csr_event_ind(mac_ctx, (uint8_t)sessionId,
			      SME_QOS_CSR_SET_KEY_SUCCESS_IND, NULL);
#ifdef FEATURE_WLAN_ESE
	/* Send Adjacent AP repot to new AP. */
	if (result == eCSR_ROAM_RESULT_AUTHENTICATED &&
	    session->isPrevApInfoValid &&
	    session->connectedProfile.isESEAssoc) {
		csr_send_ese_adjacent_ap_rep_ind(mac_ctx, session);
		session->isPrevApInfoValid = false;
	}
#endif
	qdf_mem_free(roam_info);
}

static QDF_STATUS csr_roam_issue_set_context_req(struct mac_context *mac_ctx,
						 uint32_t session_id,
						 bool add_key, bool unicast,
						 uint8_t key_idx,
						 struct qdf_mac_addr *mac_addr)
{
	enum wlan_crypto_cipher_type cipher;
	struct wlan_crypto_key *crypto_key;
	uint8_t wep_key_idx = 0;
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, session_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("VDEV object not found for session_id %d", session_id);
		return QDF_STATUS_E_INVAL;
	}
	cipher = wlan_crypto_get_cipher(vdev, unicast, key_idx);
	if (IS_WEP_CIPHER(cipher)) {
		wep_key_idx = wlan_crypto_get_default_key_idx(vdev, !unicast);
		crypto_key = wlan_crypto_get_key(vdev, wep_key_idx);
		csr_update_wep_key_peer_macaddr(vdev, crypto_key, unicast,
						mac_addr);
	} else {
		crypto_key = wlan_crypto_get_key(vdev, key_idx);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	sme_debug("session:%d, cipher:%d, ucast:%d, idx:%d, wep:%d, add:%d",
		  session_id, cipher, unicast, key_idx, wep_key_idx, add_key);
	if (!IS_WEP_CIPHER(cipher) && !add_key)
		return QDF_STATUS_E_INVAL;

	return ucfg_crypto_set_key_req(vdev, crypto_key, (unicast ?
				       WLAN_CRYPTO_KEY_TYPE_UNICAST :
				       WLAN_CRYPTO_KEY_TYPE_GROUP));
}

static enum wlan_crypto_cipher_type
csr_encr_to_cipher_type(eCsrEncryptionType encr_type)
{
	switch (encr_type) {
	case eCSR_ENCRYPT_TYPE_WEP40:
		return WLAN_CRYPTO_CIPHER_WEP_40;
	case eCSR_ENCRYPT_TYPE_WEP104:
		return WLAN_CRYPTO_CIPHER_WEP_104;
	case eCSR_ENCRYPT_TYPE_TKIP:
		return WLAN_CRYPTO_CIPHER_TKIP;
	case eCSR_ENCRYPT_TYPE_AES:
		return WLAN_CRYPTO_CIPHER_AES_CCM;
	case eCSR_ENCRYPT_TYPE_AES_CMAC:
		return WLAN_CRYPTO_CIPHER_AES_CMAC;
	case eCSR_ENCRYPT_TYPE_AES_GMAC_128:
		return WLAN_CRYPTO_CIPHER_AES_GMAC;
	case eCSR_ENCRYPT_TYPE_AES_GMAC_256:
		return WLAN_CRYPTO_CIPHER_AES_GMAC_256;
	case eCSR_ENCRYPT_TYPE_AES_GCMP:
		return WLAN_CRYPTO_CIPHER_AES_GCM;
	case eCSR_ENCRYPT_TYPE_AES_GCMP_256:
		return WLAN_CRYPTO_CIPHER_AES_GCM_256;
	default:
		return WLAN_CRYPTO_CIPHER_NONE;
	}
}

#ifdef WLAN_FEATURE_FILS_SK
static QDF_STATUS
csr_roam_store_fils_key(struct wlan_objmgr_vdev *vdev,
			bool unicast, uint8_t key_id,
			uint16_t key_length, uint8_t *key,
			tSirMacAddr *bssid,
			eCsrEncryptionType encr_type)
{
	struct wlan_crypto_key *crypto_key = NULL;
	QDF_STATUS status;
	/*
	 * key index is the FW key index.
	 * Key_id is for host crypto component key storage index
	 */
	uint8_t key_index = 0;

	if (unicast)
		key_index = 0;
	else
		key_index = 2;

	crypto_key = wlan_crypto_get_key(vdev, key_id);
	if (!crypto_key) {
		crypto_key = qdf_mem_malloc(sizeof(*crypto_key));
		if (!crypto_key)
			return QDF_STATUS_E_INVAL;

		status = wlan_crypto_save_key(vdev, key_id, crypto_key);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("Failed to save key");
			qdf_mem_free(crypto_key);
			return QDF_STATUS_E_INVAL;
		}
	}
	qdf_mem_zero(crypto_key, sizeof(*crypto_key));
	/* TODO add support for FILS cipher translation in OSIF */
	crypto_key->cipher_type = csr_encr_to_cipher_type(encr_type);
	crypto_key->keylen = key_length;
	crypto_key->keyix = key_index;
	sme_debug("key_len %d, unicast %d", key_length, unicast);
	qdf_mem_copy(&crypto_key->keyval[0], key, key_length);
	qdf_mem_copy(crypto_key->macaddr, bssid, QDF_MAC_ADDR_SIZE);

	return QDF_STATUS_SUCCESS;
}
#else
static inline
QDF_STATUS csr_roam_store_fils_key(struct wlan_objmgr_vdev *vdev,
				   bool unicast, uint8_t key_id,
				   uint16_t key_length, uint8_t *key,
				   tSirMacAddr *bssid,
				   eCsrEncryptionType encr_type)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

QDF_STATUS
csr_issue_set_context_req_helper(struct mac_context *mac_ctx,
				 struct csr_roam_profile *profile,
				 uint32_t session_id, tSirMacAddr *bssid,
				 bool addkey, bool unicast,
				 tAniKeyDirection key_direction, uint8_t key_id,
				 uint16_t key_length, uint8_t *key)
{
	enum wlan_crypto_cipher_type cipher;
	struct wlan_objmgr_vdev *vdev;
	struct set_context_rsp install_key_rsp;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, session_id,
						    WLAN_LEGACY_MAC_ID);
	if (!vdev) {
		sme_err("VDEV object not found for session_id %d", session_id);
		return QDF_STATUS_E_INVAL;
	}

	cipher = wlan_crypto_get_cipher(vdev, unicast, key_id);
	if (addkey && !IS_WEP_CIPHER(cipher) &&
	    (profile && csr_is_fils_connection(profile)))
		csr_roam_store_fils_key(vdev, unicast, key_id, key_length,
					key, bssid,
					profile->negotiatedMCEncryptionType);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);

	/*
	 * For open mode authentication, send dummy install key response to
	 * send OBSS scan and QOS event.
	 */
	if (profile &&
	    profile->negotiatedUCEncryptionType == eCSR_ENCRYPT_TYPE_NONE &&
	    !unicast) {
		install_key_rsp.length = sizeof(install_key_rsp);
		install_key_rsp.status_code = eSIR_SME_SUCCESS;
		install_key_rsp.sessionId = session_id;
		qdf_mem_copy(install_key_rsp.peer_macaddr.bytes, bssid,
			     QDF_MAC_ADDR_SIZE);

		csr_roam_chk_lnk_set_ctx_rsp(mac_ctx,
					     (tSirSmeRsp *)&install_key_rsp);

		return QDF_STATUS_SUCCESS;
	}

	return csr_roam_issue_set_context_req(mac_ctx, session_id, addkey,
					      unicast, key_id,
					      (struct qdf_mac_addr *)bssid);
}

#ifdef WLAN_FEATURE_FILS_SK
/*
 * csr_create_fils_realm_hash: API to create hash using realm
 * @fils_con_info: fils connection info obtained from supplicant
 * @tmp_hash: pointer to new hash
 *
 * Return: None
 */
static bool
csr_create_fils_realm_hash(struct wlan_fils_connection_info *fils_con_info,
			   uint8_t *tmp_hash)
{
	uint8_t *hash;
	uint8_t *data[1];

	if (!fils_con_info->realm_len)
		return false;

	hash = qdf_mem_malloc(SHA256_DIGEST_SIZE);
	if (!hash)
		return false;

	data[0] = fils_con_info->realm;
	qdf_get_hash(SHA256_CRYPTO_TYPE, 1, data,
			&fils_con_info->realm_len, hash);
	qdf_trace_hex_dump(QDF_MODULE_ID_QDF, QDF_TRACE_LEVEL_DEBUG,
				   hash, SHA256_DIGEST_SIZE);
	qdf_mem_copy(tmp_hash, hash, 2);
	qdf_mem_free(hash);
	return true;
}

static void csr_update_fils_scan_filter(struct scan_filter *filter,
					struct csr_roam_profile *profile)
{
	if (profile->fils_con_info &&
	    profile->fils_con_info->is_fils_connection) {
		uint8_t realm_hash[2];

		sme_debug("creating realm based on fils info %d",
			profile->fils_con_info->is_fils_connection);
		filter->fils_scan_filter.realm_check =
			csr_create_fils_realm_hash(profile->fils_con_info,
						   realm_hash);
		if (filter->fils_scan_filter.realm_check)
			qdf_mem_copy(filter->fils_scan_filter.fils_realm,
				     realm_hash, REAM_HASH_LEN);
	}
}

#else
static void csr_update_fils_scan_filter(struct scan_filter *filter,
					struct csr_roam_profile *profile)
{ }
#endif

static inline void csr_copy_ssids(struct wlan_ssid *ssid, tSirMacSSid *from)
{
	ssid->length = from->length;
	if (ssid->length > WLAN_SSID_MAX_LEN)
		ssid->length = WLAN_SSID_MAX_LEN;
	qdf_mem_copy(ssid->ssid, from->ssId, ssid->length);
}

void csr_copy_ssids_from_roam_params(struct roam_ext_params *roam_params,
				     struct scan_filter *filter)
{
	uint8_t i;

	if (!roam_params->num_ssid_allowed_list)
		return;

	filter->num_of_ssid = roam_params->num_ssid_allowed_list;
	if (filter->num_of_ssid > WLAN_SCAN_FILTER_NUM_SSID)
		filter->num_of_ssid = WLAN_SCAN_FILTER_NUM_SSID;
	for  (i = 0; i < filter->num_of_ssid; i++)
		csr_copy_ssids(&filter->ssid_list[i],
			       &roam_params->ssid_allowed_list[i]);
}

static void csr_copy_ssids_from_profile(tCsrSSIDs *ssid_list,
					struct scan_filter *filter)
{
	uint8_t i;

	filter->num_of_ssid = ssid_list->numOfSSIDs;
	if (filter->num_of_ssid > WLAN_SCAN_FILTER_NUM_SSID)
		filter->num_of_ssid = WLAN_SCAN_FILTER_NUM_SSID;
	for (i = 0; i < filter->num_of_ssid; i++)
		csr_copy_ssids(&filter->ssid_list[i],
			       &ssid_list->SSIDList[i].SSID);
}

#ifdef WLAN_FEATURE_11W

/**
 * csr_update_pmf_cap_from_profile: Updates PMF cap
 * @profile: Source profile
 * @filter: scan filter
 *
 * Return: None
 */
static void csr_update_pmf_cap_from_profile(struct csr_roam_profile *profile,
					    struct scan_filter *filter)
{
	if (profile->MFPCapable || profile->MFPEnabled)
		filter->pmf_cap = WLAN_PMF_CAPABLE;
	if (profile->MFPRequired)
		filter->pmf_cap = WLAN_PMF_REQUIRED;
}

static void
csr_cm_roam_fill_11w_params(struct mac_context *mac_ctx,
			    uint8_t vdev_id,
			    struct ap_profile_params *req)
{
	uint32_t group_mgmt_cipher;
	bool peer_rmf_capable = false;
	uint32_t keymgmt;
	struct wlan_objmgr_vdev *vdev;
	uint16_t rsn_caps;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Invalid vdev");
		return;
	}

	/*
	 * Get rsn cap of link, intersection of self cap and bss cap,
	 * Only set PMF flags when both STA and current AP has MFP enabled
	 */
	rsn_caps = (uint16_t)wlan_crypto_get_param(vdev,
						   WLAN_CRYPTO_PARAM_RSN_CAP);
	if (wlan_crypto_vdev_has_mgmtcipher(vdev,
					(1 << WLAN_CRYPTO_CIPHER_AES_GMAC) |
					(1 << WLAN_CRYPTO_CIPHER_AES_GMAC_256) |
					(1 << WLAN_CRYPTO_CIPHER_AES_CMAC)) &&
					(rsn_caps &
					 WLAN_CRYPTO_RSN_CAP_MFP_ENABLED))
		peer_rmf_capable = true;

	keymgmt = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_MGMT_CIPHER);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	if (keymgmt & (1 << WLAN_CRYPTO_CIPHER_AES_CMAC))
		group_mgmt_cipher = WMI_CIPHER_AES_CMAC;
	else if (keymgmt & (1 << WLAN_CRYPTO_CIPHER_AES_GMAC))
		group_mgmt_cipher = WMI_CIPHER_AES_GMAC;
	else if (keymgmt & (1 << WLAN_CRYPTO_CIPHER_AES_GMAC_256))
		group_mgmt_cipher = WMI_CIPHER_BIP_GMAC_256;
	else
		group_mgmt_cipher = WMI_CIPHER_NONE;

	if (peer_rmf_capable) {
		req->profile.rsn_mcastmgmtcipherset = group_mgmt_cipher;
		req->profile.flags |= WMI_AP_PROFILE_FLAG_PMF;
	} else {
		req->profile.rsn_mcastmgmtcipherset = WMI_CIPHER_NONE;
	}
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void
csr_cm_roam_fill_rsn_caps(struct mac_context *mac, uint8_t vdev_id,
			  uint16_t *rsn_caps)
{
	tCsrRoamConnectedProfile *profile;

	/* Copy the self RSN capabilities in roam offload request */
	profile = &mac->roam.roamSession[vdev_id].connectedProfile;
	*rsn_caps &= ~WLAN_CRYPTO_RSN_CAP_MFP_ENABLED;
	*rsn_caps &= ~WLAN_CRYPTO_RSN_CAP_MFP_REQUIRED;
	if (profile->MFPRequired)
		*rsn_caps |= WLAN_CRYPTO_RSN_CAP_MFP_REQUIRED;
	if (profile->MFPCapable)
		*rsn_caps |= WLAN_CRYPTO_RSN_CAP_MFP_ENABLED;
}
#endif
#else
static inline
void csr_update_pmf_cap_from_profile(struct csr_roam_profile *profile,
				     struct scan_filter *filter)
{}

static inline
void csr_cm_roam_fill_11w_params(struct mac_context *mac_ctx,
				 uint8_t vdev_id,
				 struct ap_profile_params *req)
{}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static inline
void csr_cm_roam_fill_rsn_caps(struct mac_context *mac, uint8_t vdev_id,
			       uint16_t *rsn_caps)
{}
#endif
#endif

QDF_STATUS csr_fill_filter_from_vdev_crypto(struct mac_context *mac_ctx,
					    struct scan_filter *filter,
					    uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Invalid vdev");
		return QDF_STATUS_E_FAILURE;
	}

	filter->authmodeset =
		wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_AUTH_MODE);
	filter->mcastcipherset =
		wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_MCAST_CIPHER);
	filter->ucastcipherset =
		wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_UCAST_CIPHER);
	filter->key_mgmt =
		wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	filter->mgmtcipherset =
		wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_MGMT_CIPHER);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	return QDF_STATUS_SUCCESS;
}

static QDF_STATUS csr_fill_crypto_params(struct mac_context *mac_ctx,
					 struct csr_roam_profile *profile,
					 struct scan_filter *filter,
					 uint8_t vdev_id)
{
	if (profile->force_rsne_override) {
		sme_debug("force_rsne_override do not set authmode and set ignore pmf cap");
		filter->ignore_pmf_cap = true;
		return QDF_STATUS_SUCCESS;
	}

	return csr_fill_filter_from_vdev_crypto(mac_ctx, filter, vdev_id);
}

void csr_update_scan_filter_dot11mode(struct mac_context *mac_ctx,
				      struct scan_filter *filter)
{
	eCsrPhyMode phymode = mac_ctx->roam.configParam.phyMode;

	if (phymode == eCSR_DOT11_MODE_11n_ONLY)
		filter->dot11mode = ALLOW_11N_ONLY;
	else if (phymode == eCSR_DOT11_MODE_11ac_ONLY)
		filter->dot11mode = ALLOW_11AC_ONLY;
	else if (phymode == eCSR_DOT11_MODE_11ax_ONLY)
		filter->dot11mode = ALLOW_11AX_ONLY;
}

QDF_STATUS
csr_roam_get_scan_filter_from_profile(struct mac_context *mac_ctx,
				      struct csr_roam_profile *profile,
				      struct scan_filter *filter,
				      bool is_roam,
				      uint8_t vdev_id)
{
	tCsrChannelInfo *ch_info;
	struct roam_ext_params *roam_params;
	uint8_t i;
	QDF_STATUS status;

	if (!filter || !profile) {
		sme_err("filter or profile is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	roam_params = &mac_ctx->roam.configParam.roam_params;

	qdf_mem_zero(filter, sizeof(*filter));
	if (profile->BSSIDs.numOfBSSIDs) {
		filter->num_of_bssid = profile->BSSIDs.numOfBSSIDs;
		if (filter->num_of_bssid > WLAN_SCAN_FILTER_NUM_BSSID)
			filter->num_of_bssid = WLAN_SCAN_FILTER_NUM_BSSID;
		for (i = 0; i < filter->num_of_bssid; i++)
			qdf_mem_copy(filter->bssid_list[i].bytes,
				     profile->BSSIDs.bssid[i].bytes,
				     QDF_MAC_ADDR_SIZE);
	}

	if (profile->SSIDs.numOfSSIDs) {
		if (is_roam && roam_params->num_ssid_allowed_list)
			csr_copy_ssids_from_roam_params(roam_params, filter);
		else
			csr_copy_ssids_from_profile(&profile->SSIDs, filter);
	}

	ch_info = &profile->ChannelInfo;
	if (ch_info->numOfChannels && ch_info->freq_list &&
	    ch_info->freq_list[0]) {
		filter->num_of_channels = 0;
		for (i = 0; i < ch_info->numOfChannels; i++) {
			if (filter->num_of_channels >= NUM_CHANNELS) {
				sme_err("max allowed channel(%d) reached",
					filter->num_of_channels);
				break;
			}
			if (csr_roam_is_channel_valid(mac_ctx,
						      ch_info->freq_list[i]) &&
			    wlan_cm_dual_sta_is_freq_allowed(
				    mac_ctx->psoc, ch_info->freq_list[i],
				    profile->csrPersona)) {
				filter->chan_freq_list[filter->num_of_channels] =
							ch_info->freq_list[i];
				filter->num_of_channels++;
			} else {
				sme_debug("freq (%d) is invalid",
					  ch_info->freq_list[i]);
			}
		}
	} else {
		/*
		 * Channels allowed is not present in the roam_profile.
		 * Update the the channels for this connection if this is
		 * 2nd STA, with the channels other than the 1st connected
		 * STA, as dual sta roaming is supported only on one band.
		 */
		if (profile->csrPersona == QDF_STA_MODE)
			wlan_cm_dual_sta_roam_update_connect_channels(
							mac_ctx->psoc,
							filter);
	}

	status = csr_fill_crypto_params(mac_ctx, profile, filter, vdev_id);
	if (QDF_IS_STATUS_ERROR(status))
		return status;


	if (profile->bWPSAssociation || profile->bOSENAssociation)
		filter->ignore_auth_enc_type = true;

	filter->mobility_domain = profile->mdid.mobility_domain;
	qdf_mem_copy(filter->bssid_hint.bytes, profile->bssid_hint.bytes,
		     QDF_MAC_ADDR_SIZE);

	csr_update_pmf_cap_from_profile(profile, filter);

	csr_update_fils_scan_filter(filter, profile);

	filter->enable_adaptive_11r =
		wlan_mlme_adaptive_11r_enabled(mac_ctx->psoc);
	csr_update_scan_filter_dot11mode(mac_ctx, filter);

	return QDF_STATUS_SUCCESS;
}

static
bool csr_roam_issue_wm_status_change(struct mac_context *mac, uint32_t sessionId,
				     enum csr_roam_wmstatus_changetypes Type,
				     tSirSmeRsp *pSmeRsp)
{
	bool fCommandQueued = false;
	tSmeCmd *pCommand;
	struct qdf_mac_addr peer_mac;
	struct csr_roam_session *session;

	session = CSR_GET_SESSION(mac, sessionId);
	if (!session)
		return false;

	do {
		/* Validate the type is ok... */
		if ((eCsrDisassociated != Type)
		    && (eCsrDeauthenticated != Type))
			break;
		pCommand = csr_get_command_buffer(mac);
		if (!pCommand) {
			sme_err(" fail to get command buffer");
			break;
		}
		/* Change the substate in case it is waiting for key */
		if (CSR_IS_WAIT_FOR_KEY(mac, sessionId)) {
			csr_roam_stop_wait_for_key_timer(mac);
			csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_NONE,
						 sessionId);
		}
		pCommand->command = eSmeCommandWmStatusChange;
		pCommand->vdev_id = (uint8_t) sessionId;
		pCommand->u.wmStatusChangeCmd.Type = Type;
		if (eCsrDisassociated == Type) {
			qdf_mem_copy(&pCommand->u.wmStatusChangeCmd.u.
				     DisassocIndMsg, pSmeRsp,
				     sizeof(pCommand->u.wmStatusChangeCmd.u.
					    DisassocIndMsg));
			qdf_mem_copy(&peer_mac, &pCommand->u.wmStatusChangeCmd.
						u.DisassocIndMsg.peer_macaddr,
				     QDF_MAC_ADDR_SIZE);

		} else {
			qdf_mem_copy(&pCommand->u.wmStatusChangeCmd.u.
				     DeauthIndMsg, pSmeRsp,
				     sizeof(pCommand->u.wmStatusChangeCmd.u.
					    DeauthIndMsg));
			qdf_mem_copy(&peer_mac, &pCommand->u.wmStatusChangeCmd.
						u.DeauthIndMsg.peer_macaddr,
				     QDF_MAC_ADDR_SIZE);
		}

		if (CSR_IS_INFRA_AP(&session->connectedProfile))
			csr_roam_issue_disconnect_stats(mac, sessionId,
							peer_mac);

		if (QDF_IS_STATUS_SUCCESS
			    (csr_queue_sme_command(mac, pCommand, false)))
			fCommandQueued = true;
		else
			sme_err("fail to send message");

		/* AP has issued Dissac/Deauth, Set the operating mode
		 * value to configured value
		 */
		csr_set_default_dot11_mode(mac);
	} while (0);
	return fCommandQueued;
}

static void csr_update_snr(struct mac_context *mac, void *pMsg)
{
	tAniGetSnrReq *pGetSnrReq = (tAniGetSnrReq *) pMsg;

	if (pGetSnrReq) {
		if (QDF_STATUS_SUCCESS != wma_get_snr(pGetSnrReq)) {
			sme_err("Error in wma_get_snr");
			return;
		}

	} else
		sme_err("pGetSnrReq is NULL");
}

static QDF_STATUS csr_send_reset_ap_caps_changed(struct mac_context *mac,
				struct qdf_mac_addr *bssId)
{
	tpSirResetAPCapsChange pMsg;
	uint16_t len;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	/* Create the message and send to lim */
	len = sizeof(tSirResetAPCapsChange);
	pMsg = qdf_mem_malloc(len);
	if (!pMsg)
		status = QDF_STATUS_E_NOMEM;
	else
		status = QDF_STATUS_SUCCESS;

	if (QDF_IS_STATUS_SUCCESS(status)) {
		pMsg->messageType = eWNI_SME_RESET_AP_CAPS_CHANGED;
		pMsg->length = len;
		qdf_copy_macaddr(&pMsg->bssId, bssId);
		sme_debug(
			"CSR reset caps change for Bssid= " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(pMsg->bssId.bytes));
		status = umac_send_mb_message_to_mac(pMsg);
	} else {
		sme_err("Memory allocation failed");
	}
	return status;
}

#ifdef FEATURE_WLAN_ESE
/**
 * csr_convert_ese_akm_to_ani() - Convert enum csr_akm_type ESE akm value to
 * equivalent enum ani_akm_type value
 * @akm_type: value of type enum ani_akm_type
 *
 * Return: ani_akm_type value corresponding
 */
static enum ani_akm_type csr_convert_ese_akm_to_ani(enum csr_akm_type akm_type)
{
	switch (akm_type) {
	case eCSR_AUTH_TYPE_CCKM_RSN:
		return ANI_AKM_TYPE_CCKM;
	default:
		return ANI_AKM_TYPE_UNKNOWN;
	}
}
#else
static inline enum
ani_akm_type csr_convert_ese_akm_to_ani(enum csr_akm_type akm_type)
{
	return ANI_AKM_TYPE_UNKNOWN;
}
#endif

#ifdef WLAN_FEATURE_SAE
/**
 * csr_convert_sae_akm_to_ani() - Convert enum csr_akm_type SAE akm value to
 * equivalent enum ani_akm_type value
 * @akm_type: value of type enum ani_akm_type
 *
 * Return: ani_akm_type value corresponding
 */
static enum ani_akm_type csr_convert_sae_akm_to_ani(enum csr_akm_type akm_type)
{
	switch (akm_type) {
	case eCSR_AUTH_TYPE_SAE:
		return ANI_AKM_TYPE_SAE;
	case eCSR_AUTH_TYPE_FT_SAE:
		return ANI_AKM_TYPE_FT_SAE;
	default:
		return ANI_AKM_TYPE_UNKNOWN;
	}
}
#else
static inline
enum ani_akm_type csr_convert_sae_akm_to_ani(enum csr_akm_type akm_type)
{
	return ANI_AKM_TYPE_UNKNOWN;
}
#endif

/**
 * csr_convert_csr_to_ani_akm_type() - Convert enum csr_akm_type value to
 * equivalent enum ani_akm_type value
 * @akm_type: value of type enum ani_akm_type
 *
 * Return: ani_akm_type value corresponding
 */
static enum ani_akm_type
csr_convert_csr_to_ani_akm_type(enum csr_akm_type akm_type)
{
	enum ani_akm_type ani_akm;

	switch (akm_type) {
	case eCSR_AUTH_TYPE_OPEN_SYSTEM:
	case eCSR_AUTH_TYPE_NONE:
		return ANI_AKM_TYPE_NONE;
	case eCSR_AUTH_TYPE_WPA:
		return ANI_AKM_TYPE_WPA;
	case eCSR_AUTH_TYPE_WPA_PSK:
		return ANI_AKM_TYPE_WPA_PSK;
	case eCSR_AUTH_TYPE_RSN:
		return ANI_AKM_TYPE_RSN;
	case eCSR_AUTH_TYPE_RSN_PSK:
		return ANI_AKM_TYPE_RSN_PSK;
	case eCSR_AUTH_TYPE_FT_RSN:
		return ANI_AKM_TYPE_FT_RSN;
	case eCSR_AUTH_TYPE_FT_RSN_PSK:
		return ANI_AKM_TYPE_FT_RSN_PSK;
	case eCSR_AUTH_TYPE_RSN_PSK_SHA256:
		return ANI_AKM_TYPE_RSN_PSK_SHA256;
	case eCSR_AUTH_TYPE_RSN_8021X_SHA256:
		return ANI_AKM_TYPE_RSN_8021X_SHA256;
	case eCSR_AUTH_TYPE_FILS_SHA256:
		return ANI_AKM_TYPE_FILS_SHA256;
	case eCSR_AUTH_TYPE_FILS_SHA384:
		return ANI_AKM_TYPE_FILS_SHA384;
	case eCSR_AUTH_TYPE_FT_FILS_SHA256:
		return ANI_AKM_TYPE_FT_FILS_SHA256;
	case eCSR_AUTH_TYPE_FT_FILS_SHA384:
		return ANI_AKM_TYPE_FT_FILS_SHA384;
	case eCSR_AUTH_TYPE_DPP_RSN:
		return ANI_AKM_TYPE_DPP_RSN;
	case eCSR_AUTH_TYPE_OWE:
		return ANI_AKM_TYPE_OWE;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA256:
		return ANI_AKM_TYPE_SUITEB_EAP_SHA256;
	case eCSR_AUTH_TYPE_SUITEB_EAP_SHA384:
		return ANI_AKM_TYPE_SUITEB_EAP_SHA384;
	case eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384:
		return ANI_AKM_TYPE_FT_SUITEB_EAP_SHA384;
	case eCSR_AUTH_TYPE_OSEN:
		return ANI_AKM_TYPE_OSEN;
	default:
		ani_akm = ANI_AKM_TYPE_UNKNOWN;
	}

	if (ani_akm == ANI_AKM_TYPE_UNKNOWN)
		ani_akm = csr_convert_sae_akm_to_ani(akm_type);

	if (ani_akm == ANI_AKM_TYPE_UNKNOWN)
		ani_akm = csr_convert_ese_akm_to_ani(akm_type);

	return ani_akm;
}

/**
 * csr_translate_akm_type() - Convert ani_akm_type value to equivalent
 * enum csr_akm_type
 * @akm_type: value of type ani_akm_type
 *
 * Return: enum csr_akm_type value
 */
static enum csr_akm_type csr_translate_akm_type(enum ani_akm_type akm_type)
{
	enum csr_akm_type csr_akm_type;

	switch (akm_type)
	{
	case ANI_AKM_TYPE_NONE:
		csr_akm_type = eCSR_AUTH_TYPE_NONE;
		break;
#ifdef WLAN_FEATURE_SAE
	case ANI_AKM_TYPE_SAE:
		csr_akm_type = eCSR_AUTH_TYPE_SAE;
		break;
#endif
	case ANI_AKM_TYPE_WPA:
		csr_akm_type = eCSR_AUTH_TYPE_WPA;
		break;
	case ANI_AKM_TYPE_WPA_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_WPA_PSK;
		break;
	case ANI_AKM_TYPE_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_RSN;
		break;
	case ANI_AKM_TYPE_RSN_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_PSK;
		break;
	case ANI_AKM_TYPE_FT_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_FT_RSN;
		break;
	case ANI_AKM_TYPE_FT_RSN_PSK:
		csr_akm_type = eCSR_AUTH_TYPE_FT_RSN_PSK;
		break;
#ifdef FEATURE_WLAN_ESE
	case ANI_AKM_TYPE_CCKM:
		csr_akm_type = eCSR_AUTH_TYPE_CCKM_RSN;
		break;
#endif
	case ANI_AKM_TYPE_RSN_PSK_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_PSK_SHA256;
		break;
	case ANI_AKM_TYPE_RSN_8021X_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_RSN_8021X_SHA256;
		break;
	case ANI_AKM_TYPE_FILS_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_FILS_SHA256;
		break;
	case ANI_AKM_TYPE_FILS_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_FILS_SHA384;
		break;
	case ANI_AKM_TYPE_FT_FILS_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_FT_FILS_SHA256;
		break;
	case ANI_AKM_TYPE_FT_FILS_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_FT_FILS_SHA384;
		break;
	case ANI_AKM_TYPE_DPP_RSN:
		csr_akm_type = eCSR_AUTH_TYPE_DPP_RSN;
		break;
	case ANI_AKM_TYPE_OWE:
		csr_akm_type = eCSR_AUTH_TYPE_OWE;
		break;
	case ANI_AKM_TYPE_SUITEB_EAP_SHA256:
		csr_akm_type = eCSR_AUTH_TYPE_SUITEB_EAP_SHA256;
		break;
	case ANI_AKM_TYPE_SUITEB_EAP_SHA384:
		csr_akm_type = eCSR_AUTH_TYPE_SUITEB_EAP_SHA384;
		break;
	case ANI_AKM_TYPE_OSEN:
		csr_akm_type = eCSR_AUTH_TYPE_OSEN;
		break;
	default:
		csr_akm_type = eCSR_AUTH_TYPE_UNKNOWN;
	}

	return csr_akm_type;
}

static bool csr_is_sae_akm_present(tDot11fIERSN * const rsn_ie)
{
	uint16_t i;

	if (rsn_ie->akm_suite_cnt > 6) {
		sme_debug("Invalid akm_suite_cnt in Rx RSN IE");
		return false;
	}

	for (i = 0; i < rsn_ie->akm_suite_cnt; i++) {
		if (LE_READ_4(rsn_ie->akm_suite[i]) == RSN_AUTH_KEY_MGMT_SAE) {
			sme_debug("SAE AKM present");
			return true;
		}
	}
	return false;
}

static bool csr_is_sae_peer_allowed(struct mac_context *mac_ctx,
				    struct assoc_ind *assoc_ind,
				    struct csr_roam_session *session,
				    tSirMacAddr peer_mac_addr,
				    tDot11fIERSN *rsn_ie,
				    enum wlan_status_code *mac_status_code)
{
	bool is_allowed = false;

	/* Allow the peer if it's SAE authenticated */
	if (assoc_ind->is_sae_authenticated)
		return true;

	/* Allow the peer with valid PMKID */
	if (!rsn_ie->pmkid_count) {
		*mac_status_code = STATUS_NOT_SUPPORTED_AUTH_ALG;
		sme_debug("No PMKID present in RSNIE; Tried to use SAE AKM after non-SAE authentication");
	} else if (csr_is_pmkid_found_for_peer(mac_ctx, session, peer_mac_addr,
					       &rsn_ie->pmkid[0][0],
					       rsn_ie->pmkid_count)) {
		sme_debug("Valid PMKID found for SAE peer");
		is_allowed = true;
	} else {
		*mac_status_code = STATUS_INVALID_PMKID;
		sme_debug("No valid PMKID found for SAE peer");
	}

	return is_allowed;
}

static QDF_STATUS
csr_send_assoc_ind_to_upper_layer_cnf_msg(struct mac_context *mac,
					  struct assoc_ind *ind,
					  QDF_STATUS status,
					  uint8_t vdev_id)
{
	struct scheduler_msg msg = {0};
	tSirSmeAssocIndToUpperLayerCnf *cnf;

	cnf = qdf_mem_malloc(sizeof(*cnf));
	if (!cnf)
		return QDF_STATUS_E_NOMEM;

	cnf->messageType = eWNI_SME_UPPER_LAYER_ASSOC_CNF;
	cnf->length = sizeof(*cnf);
	cnf->sessionId = vdev_id;

	if (QDF_IS_STATUS_SUCCESS(status))
		cnf->status_code = eSIR_SME_SUCCESS;
	else
		cnf->status_code = eSIR_SME_ASSOC_REFUSED;
	qdf_mem_copy(&cnf->bssId, &ind->bssId, sizeof(cnf->bssId));
	qdf_mem_copy(&cnf->peerMacAddr, &ind->peerMacAddr,
		     sizeof(cnf->peerMacAddr));
	cnf->aid = ind->staId;
	cnf->wmmEnabledSta = ind->wmmEnabledSta;
	cnf->rsnIE = ind->rsnIE;
#ifdef FEATURE_WLAN_WAPI
	cnf->wapiIE = ind->wapiIE;
#endif
	cnf->addIE = ind->addIE;
	cnf->reassocReq = ind->reassocReq;
	cnf->timingMeasCap = ind->timingMeasCap;
	cnf->chan_info = ind->chan_info;
	cnf->ampdu = ind->ampdu;
	cnf->sgi_enable = ind->sgi_enable;
	cnf->tx_stbc = ind->tx_stbc;
	cnf->ch_width = ind->ch_width;
	cnf->mode = ind->mode;
	cnf->rx_stbc = ind->rx_stbc;
	cnf->max_supp_idx = ind->max_supp_idx;
	cnf->max_ext_idx = ind->max_ext_idx;
	cnf->max_mcs_idx = ind->max_mcs_idx;
	cnf->rx_mcs_map = ind->rx_mcs_map;
	cnf->tx_mcs_map = ind->tx_mcs_map;
	cnf->ecsa_capable = ind->ecsa_capable;
	if (ind->HTCaps.present)
		cnf->ht_caps = ind->HTCaps;
	if (ind->VHTCaps.present)
		cnf->vht_caps = ind->VHTCaps;
	cnf->capability_info = ind->capability_info;
	cnf->he_caps_present = ind->he_caps_present;
	if (ind->assocReqPtr) {
		if (ind->assocReqLength < MAX_ASSOC_REQ_IE_LEN) {
			cnf->ies = qdf_mem_malloc(ind->assocReqLength);
			if (!cnf->ies) {
				qdf_mem_free(cnf);
				return QDF_STATUS_E_NOMEM;
			}
			cnf->ies_len = ind->assocReqLength;
			qdf_mem_copy(cnf->ies, ind->assocReqPtr,
				     cnf->ies_len);
		} else {
			sme_err("Assoc Ie length is too long");
		}
	}

	msg.type = eWNI_SME_UPPER_LAYER_ASSOC_CNF;
	msg.bodyptr = cnf;
	sys_process_mmh_msg(mac, &msg);

	return QDF_STATUS_SUCCESS;
}

static void
csr_roam_chk_lnk_assoc_ind_upper_layer(
		struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t session_id = WLAN_UMAC_VDEV_ID_MAX;
	struct assoc_ind *assoc_ind;
	QDF_STATUS status;

	assoc_ind = (struct assoc_ind *)msg_ptr;
	status = csr_roam_get_session_id_from_bssid(
			mac_ctx, (struct qdf_mac_addr *)assoc_ind->bssId,
			&session_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Couldn't find session_id for given BSSID");
		goto free_mem;
	}
	csr_send_assoc_ind_to_upper_layer_cnf_msg(
					mac_ctx, assoc_ind, status, session_id);
	/*in the association response tx compete case,
	 *memory for assoc_ind->assocReqPtr will be malloced
	 *in the lim_assoc_rsp_tx_complete -> lim_fill_sme_assoc_ind_params
	 *and then assoc_ind will pass here, so after using it
	 *in the csr_send_assoc_ind_to_upper_layer_cnf_msg and
	 *then free the memroy here.
	 */
free_mem:
	if (assoc_ind->assocReqLength != 0 && assoc_ind->assocReqPtr)
		qdf_mem_free(assoc_ind->assocReqPtr);
}

static void
csr_roam_chk_lnk_assoc_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	struct assoc_ind *pAssocInd;
	enum wlan_status_code mac_status_code = STATUS_SUCCESS;
	enum csr_akm_type csr_akm_type;

	sme_debug("Receive WNI_SME_ASSOC_IND from SME");
	pAssocInd = (struct assoc_ind *) msg_ptr;
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
				(struct qdf_mac_addr *) pAssocInd->bssId,
				&sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Couldn't find session_id for given BSSID");
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	csr_akm_type = csr_translate_akm_type(pAssocInd->akm_type);

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	/* Required for indicating the frames to upper layer */
	roam_info->assocReqLength = pAssocInd->assocReqLength;
	roam_info->assocReqPtr = pAssocInd->assocReqPtr;
	roam_info->beaconPtr = pAssocInd->beaconPtr;
	roam_info->beaconLength = pAssocInd->beaconLength;
	roam_info->status_code = eSIR_SME_SUCCESS;
	roam_info->u.pConnectedProfile = &session->connectedProfile;
	roam_info->staId = (uint8_t)pAssocInd->staId;
	roam_info->rsnIELen = (uint8_t)pAssocInd->rsnIE.length;
	roam_info->prsnIE = pAssocInd->rsnIE.rsnIEdata;
#ifdef FEATURE_WLAN_WAPI
	roam_info->wapiIELen = (uint8_t)pAssocInd->wapiIE.length;
	roam_info->pwapiIE = pAssocInd->wapiIE.wapiIEdata;
#endif
	roam_info->addIELen = (uint8_t)pAssocInd->addIE.length;
	roam_info->paddIE = pAssocInd->addIE.addIEdata;
	qdf_mem_copy(roam_info->peerMac.bytes,
		     pAssocInd->peerMacAddr,
		     sizeof(tSirMacAddr));
	qdf_mem_copy(roam_info->bssid.bytes,
		     pAssocInd->bssId,
		     sizeof(struct qdf_mac_addr));
	roam_info->wmmEnabledSta = pAssocInd->wmmEnabledSta;
	roam_info->timingMeasCap = pAssocInd->timingMeasCap;
	roam_info->ecsa_capable = pAssocInd->ecsa_capable;
	qdf_mem_copy(&roam_info->chan_info,
		     &pAssocInd->chan_info,
		     sizeof(struct oem_channel_info));

	if (pAssocInd->HTCaps.present)
		qdf_mem_copy(&roam_info->ht_caps,
			     &pAssocInd->HTCaps,
			     sizeof(tDot11fIEHTCaps));
	if (pAssocInd->VHTCaps.present)
		qdf_mem_copy(&roam_info->vht_caps,
			     &pAssocInd->VHTCaps,
			     sizeof(tDot11fIEVHTCaps));
	roam_info->capability_info = pAssocInd->capability_info;
	roam_info->he_caps_present = pAssocInd->he_caps_present;

	if (CSR_IS_INFRA_AP(roam_info->u.pConnectedProfile)) {
		if (session->pCurRoamProfile &&
		    CSR_IS_ENC_TYPE_STATIC(
			session->pCurRoamProfile->negotiatedUCEncryptionType)) {
			/* NO keys... these key parameters don't matter. */
			csr_issue_set_context_req_helper(mac_ctx,
					session->pCurRoamProfile, sessionId,
					&roam_info->peerMac.bytes, false, true,
					eSIR_TX_RX, 0, 0, NULL);
			roam_info->fAuthRequired = false;
		} else {
			roam_info->fAuthRequired = true;
		}
		if (csr_akm_type == eCSR_AUTH_TYPE_OWE) {
			roam_info->owe_pending_assoc_ind = qdf_mem_malloc(
							    sizeof(*pAssocInd));
			if (roam_info->owe_pending_assoc_ind)
				qdf_mem_copy(roam_info->owe_pending_assoc_ind,
					     pAssocInd, sizeof(*pAssocInd));
		}
		status = csr_roam_call_callback(mac_ctx, sessionId,
					roam_info, 0, eCSR_ROAM_INFRA_IND,
					eCSR_ROAM_RESULT_INFRA_ASSOCIATION_IND);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			/* Refused due to Mac filtering */
			roam_info->status_code = eSIR_SME_ASSOC_REFUSED;
		} else if (pAssocInd->rsnIE.length && WLAN_ELEMID_RSN ==
			   pAssocInd->rsnIE.rsnIEdata[0]) {
			tDot11fIERSN rsn_ie = {0};

			if (dot11f_unpack_ie_rsn(mac_ctx,
						 pAssocInd->rsnIE.rsnIEdata + 2,
						 pAssocInd->rsnIE.length - 2,
						 &rsn_ie, false)
			    != DOT11F_PARSE_SUCCESS ||
			    (csr_is_sae_akm_present(&rsn_ie) &&
			     !csr_is_sae_peer_allowed(mac_ctx, pAssocInd,
						      session,
						      pAssocInd->peerMacAddr,
						      &rsn_ie,
						      &mac_status_code))) {
				status = QDF_STATUS_E_INVAL;
				roam_info->status_code =
						eSIR_SME_ASSOC_REFUSED;
				sme_debug("SAE peer not allowed: Status: %d",
					  mac_status_code);
			}
		}
	}

	if (csr_akm_type != eCSR_AUTH_TYPE_OWE) {
		if (CSR_IS_INFRA_AP(roam_info->u.pConnectedProfile) &&
		    roam_info->status_code != eSIR_SME_ASSOC_REFUSED)
			pAssocInd->need_assoc_rsp_tx_cb = true;
		/* Send Association completion message to PE */
		status = csr_send_assoc_cnf_msg(mac_ctx, pAssocInd, status,
						mac_status_code);
	}

	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_disassoc_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	struct disassoc_ind *pDisassocInd;
	tSmeCmd *cmd;

	cmd = qdf_mem_malloc(sizeof(*cmd));
	if (!cmd)
		return;

	/*
	 * Check if AP dis-associated us because of MIC failure. If so,
	 * then we need to take action immediately and not wait till the
	 * the WmStatusChange requests is pushed and processed
	 */
	pDisassocInd = (struct disassoc_ind *)msg_ptr;

	sessionId = pDisassocInd->vdev_id;
	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId)) {
		sme_err("Invalid session. Session Id: %d BSSID: " QDF_MAC_ADDR_FMT,
			sessionId, QDF_MAC_ADDR_REF(pDisassocInd->bssid.bytes));
		qdf_mem_free(cmd);
		return;
	}

	if (csr_is_deauth_disassoc_already_active(mac_ctx, sessionId,
	    pDisassocInd->peer_macaddr)) {
		qdf_mem_free(cmd);
		return;
	}

	sme_nofl_info("disassoc from peer " QDF_MAC_ADDR_FMT
		      "reason: %d status: %d vid %d",
		      QDF_MAC_ADDR_REF(pDisassocInd->peer_macaddr.bytes),
		      pDisassocInd->reasonCode,
		      pDisassocInd->status_code, sessionId);
	/*
	 * If we are in neighbor preauth done state then on receiving
	 * disassoc or deauth we dont roam instead we just disassoc
	 * from current ap and then go to disconnected state
	 * This happens for ESE and 11r FT connections ONLY.
	 */
	if (csr_roam_is11r_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#ifdef FEATURE_WLAN_ESE
	if (csr_roam_is_ese_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#endif
	if (csr_roam_is_fast_roam_enabled(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session: %d not found", sessionId);
		qdf_mem_free(cmd);
		return;
	}

	csr_update_scan_entry_associnfo(mac_ctx,
					session, SCAN_ENTRY_CON_STATE_NONE);

	/* Update the disconnect stats */
	session->disconnect_stats.disconnection_cnt++;
	session->disconnect_stats.disassoc_by_peer++;

	if (csr_is_conn_state_infra(mac_ctx, sessionId))
		session->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
	sme_qos_csr_event_ind(mac_ctx, (uint8_t) sessionId,
			      SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
	csr_roam_link_down(mac_ctx, sessionId);
	csr_roam_issue_wm_status_change(mac_ctx, sessionId,
					eCsrDisassociated, msg_ptr);
	if (CSR_IS_INFRA_AP(&session->connectedProfile)) {
		/*
		 * STA/P2P client got  disassociated so remove any pending
		 * deauth commands in sme pending list
		 */
		cmd->command = eSmeCommandRoam;
		cmd->vdev_id = (uint8_t) sessionId;
		cmd->u.roamCmd.roamReason = eCsrForcedDeauthSta;
		qdf_mem_copy(cmd->u.roamCmd.peerMac,
			     pDisassocInd->peer_macaddr.bytes,
			     QDF_MAC_ADDR_SIZE);
		csr_roam_remove_duplicate_command(mac_ctx, sessionId, cmd,
						  eCsrForcedDeauthSta);
	}
	qdf_mem_free(cmd);
}

static void
csr_roam_chk_lnk_deauth_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	struct deauth_ind *pDeauthInd;

	pDeauthInd = (struct deauth_ind *)msg_ptr;
	sme_debug("DEAUTH Indication from MAC for vdev_id %d bssid "QDF_MAC_ADDR_FMT,
		  pDeauthInd->vdev_id,
		  QDF_MAC_ADDR_REF(pDeauthInd->bssid.bytes));

	sessionId = pDeauthInd->vdev_id;
	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId))
		return;

	if (csr_is_deauth_disassoc_already_active(mac_ctx, sessionId,
	    pDeauthInd->peer_macaddr))
		return;
	/* If we are in neighbor preauth done state then on receiving
	 * disassoc or deauth we dont roam instead we just disassoc
	 * from current ap and then go to disconnected state
	 * This happens for ESE and 11r FT connections ONLY.
	 */
	if (csr_roam_is11r_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#ifdef FEATURE_WLAN_ESE
	if (csr_roam_is_ese_assoc(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
#endif
	if (csr_roam_is_fast_roam_enabled(mac_ctx, sessionId) &&
	    (csr_neighbor_roam_state_preauth_done(mac_ctx, sessionId)))
		csr_neighbor_roam_tranistion_preauth_done_to_disconnected(
							mac_ctx, sessionId);
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}

	csr_update_scan_entry_associnfo(mac_ctx,
					session, SCAN_ENTRY_CON_STATE_NONE);
	/* Update the disconnect stats */
	switch (pDeauthInd->reasonCode) {
	case REASON_DISASSOC_DUE_TO_INACTIVITY:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.peer_kickout++;
		break;
	case REASON_UNSPEC_FAILURE:
	case REASON_PREV_AUTH_NOT_VALID:
	case REASON_DEAUTH_NETWORK_LEAVING:
	case REASON_CLASS2_FRAME_FROM_NON_AUTH_STA:
	case REASON_CLASS3_FRAME_FROM_NON_ASSOC_STA:
	case REASON_STA_NOT_AUTHENTICATED:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.deauth_by_peer++;
		break;
	case REASON_BEACON_MISSED:
		session->disconnect_stats.disconnection_cnt++;
		session->disconnect_stats.bmiss++;
		break;
	default:
		/* Unknown reason code */
		break;
	}

	if (csr_is_conn_state_infra(mac_ctx, sessionId))
		session->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
	sme_qos_csr_event_ind(mac_ctx, (uint8_t) sessionId,
			      SME_QOS_CSR_DISCONNECT_IND, NULL);
#endif
	csr_roam_link_down(mac_ctx, sessionId);
	csr_roam_issue_wm_status_change(mac_ctx, sessionId,
					eCsrDeauthenticated,
					msg_ptr);
}

static void
csr_roam_chk_lnk_swt_ch_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	uint16_t ie_len;
	QDF_STATUS status;
	struct switch_channel_ind *pSwitchChnInd;
	struct csr_roam_info *roam_info;
	tSirMacDsParamSetIE *ds_params_ie;
	struct wlan_ie_htinfo *ht_info_ie;


	/* in case of STA, the SWITCH_CHANNEL originates from its AP */
	sme_debug("eWNI_SME_SWITCH_CHL_IND from SME");
	pSwitchChnInd = (struct switch_channel_ind *)msg_ptr;
	/* Update with the new channel id. The channel id is hidden in the
	 * status_code.
	 */
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
			&pSwitchChnInd->bssid, &sessionId);
	if (QDF_IS_STATUS_ERROR(status))
		return;

	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}

	if (QDF_IS_STATUS_ERROR(pSwitchChnInd->status)) {
		sme_err("Channel switch failed");
		csr_roam_disconnect(mac_ctx, sessionId,
				    eCSR_DISCONNECT_REASON_DEAUTH,
				    REASON_CHANNEL_SWITCH_FAILED);
		return;
	}
	session->connectedProfile.op_freq = pSwitchChnInd->freq;

	/* Update the occupied channel list with the new switched channel */
	csr_init_occupied_channels_list(mac_ctx, sessionId);

	if (session->pConnectBssDesc) {
		session->pConnectBssDesc->chan_freq = pSwitchChnInd->freq;

		ie_len = csr_get_ielen_from_bss_description(
						session->pConnectBssDesc);
		ds_params_ie = (tSirMacDsParamSetIE *)wlan_get_ie_ptr_from_eid(
				DOT11F_EID_DSPARAMS,
				(uint8_t *)session->pConnectBssDesc->ieFields,
				ie_len);
		if (ds_params_ie)
			ds_params_ie->channelNumber =
				wlan_reg_freq_to_chan(mac_ctx->pdev,
						      pSwitchChnInd->freq);

		ht_info_ie =
			(struct wlan_ie_htinfo *)wlan_get_ie_ptr_from_eid
				(DOT11F_EID_HTINFO,
				 (uint8_t *)session->pConnectBssDesc->ieFields,
				 ie_len);
		if (ht_info_ie) {
			ht_info_ie->hi_ie.hi_ctrlchannel =
				wlan_reg_freq_to_chan(mac_ctx->pdev,
						      pSwitchChnInd->freq);
			ht_info_ie->hi_ie.hi_extchoff =
				pSwitchChnInd->chan_params.sec_ch_offset;
		}
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	roam_info->chan_info.mhz = pSwitchChnInd->freq;
	roam_info->chan_info.ch_width = pSwitchChnInd->chan_params.ch_width;
	roam_info->chan_info.sec_ch_offset =
				pSwitchChnInd->chan_params.sec_ch_offset;
	roam_info->chan_info.band_center_freq1 =
				pSwitchChnInd->chan_params.mhz_freq_seg0;
	roam_info->chan_info.band_center_freq2 =
				pSwitchChnInd->chan_params.mhz_freq_seg1;

	switch (session->bssParams.uCfgDot11Mode) {
	case eCSR_CFG_DOT11_MODE_11N:
	case eCSR_CFG_DOT11_MODE_11N_ONLY:
		roam_info->mode = SIR_SME_PHY_MODE_HT;
		break;
	case eCSR_CFG_DOT11_MODE_11AC:
	case eCSR_CFG_DOT11_MODE_11AC_ONLY:
	case eCSR_CFG_DOT11_MODE_11AX:
	case eCSR_CFG_DOT11_MODE_11AX_ONLY:
		roam_info->mode = SIR_SME_PHY_MODE_VHT;
		break;
	default:
		roam_info->mode = SIR_SME_PHY_MODE_LEGACY;
		break;
	}

	status = csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
					eCSR_ROAM_STA_CHANNEL_SWITCH,
					eCSR_ROAM_RESULT_NONE);
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_deauth_rsp(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	struct deauth_rsp *pDeauthRsp = (struct deauth_rsp *) msg_ptr;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("eWNI_SME_DEAUTH_RSP from SME");
	sessionId = pDeauthRsp->sessionId;
	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId)) {
		qdf_mem_free(roam_info);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (CSR_IS_INFRA_AP(&session->connectedProfile)) {
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDeauthRsp->peer_macaddr);
		roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
		roam_info->status_code = pDeauthRsp->status_code;
		status = csr_roam_call_callback(mac_ctx, sessionId,
						roam_info, 0,
						eCSR_ROAM_LOSTLINK,
						eCSR_ROAM_RESULT_FORCED);
	}
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_disassoc_rsp(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	struct csr_roam_session *session;
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	/*
	 * session id is invalid here so cant use it to access the array
	 * curSubstate as index
	 */
	struct disassoc_rsp *pDisassocRsp = (struct disassoc_rsp *) msg_ptr;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("eWNI_SME_DISASSOC_RSP from SME ");
	sessionId = pDisassocRsp->sessionId;
	if (!CSR_IS_SESSION_VALID(mac_ctx, sessionId)) {
		qdf_mem_free(roam_info);
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, sessionId);
	if (CSR_IS_INFRA_AP(&session->connectedProfile)) {
		roam_info->u.pConnectedProfile = &session->connectedProfile;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDisassocRsp->peer_macaddr);
		roam_info->reasonCode = eCSR_ROAM_RESULT_FORCED;
		roam_info->status_code = pDisassocRsp->status_code;
		status = csr_roam_call_callback(mac_ctx, sessionId,
						roam_info, 0,
						eCSR_ROAM_LOSTLINK,
						eCSR_ROAM_RESULT_FORCED);
	}
	qdf_mem_free(roam_info);
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
static void
csr_roam_diag_mic_fail(struct mac_context *mac_ctx, uint32_t sessionId)
{
	WLAN_HOST_DIAG_EVENT_DEF(secEvent,
				 host_event_wlan_security_payload_type);
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, sessionId);

	if (!session) {
		sme_err("session %d not found", sessionId);
		return;
	}
	qdf_mem_zero(&secEvent, sizeof(host_event_wlan_security_payload_type));
	secEvent.eventId = WLAN_SECURITY_EVENT_MIC_ERROR;
	secEvent.encryptionModeMulticast =
		(uint8_t) diag_enc_type_from_csr_type(
				session->connectedProfile.mcEncryptionType);
	secEvent.encryptionModeUnicast =
		(uint8_t) diag_enc_type_from_csr_type(
				session->connectedProfile.EncryptionType);
	secEvent.authMode =
		(uint8_t) diag_auth_type_from_csr_type(
				session->connectedProfile.AuthType);
	qdf_mem_copy(secEvent.bssid, session->connectedProfile.bssid.bytes,
			QDF_MAC_ADDR_SIZE);
	WLAN_HOST_DIAG_EVENT_REPORT(&secEvent, EVENT_WLAN_SECURITY);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */

static void
csr_roam_chk_lnk_mic_fail_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	struct mic_failure_ind *mic_ind = (struct mic_failure_ind *)msg_ptr;
	eCsrRoamResult result = eCSR_ROAM_RESULT_MIC_ERROR_UNICAST;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	status = csr_roam_get_session_id_from_bssid(mac_ctx,
				&mic_ind->bssId, &sessionId);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		roam_info->u.pMICFailureInfo = &mic_ind->info;
		if (mic_ind->info.multicast)
			result = eCSR_ROAM_RESULT_MIC_ERROR_GROUP;
		else
			result = eCSR_ROAM_RESULT_MIC_ERROR_UNICAST;
		csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
				       eCSR_ROAM_MIC_ERROR_IND, result);
	}
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
	csr_roam_diag_mic_fail(mac_ctx, sessionId);
#endif /* FEATURE_WLAN_DIAG_SUPPORT_CSR */
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_pbs_probe_req_ind(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	tpSirSmeProbeReqInd pProbeReqInd = (tpSirSmeProbeReqInd) msg_ptr;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	sme_debug("WPS PBC Probe request Indication from SME");

	status = csr_roam_get_session_id_from_bssid(mac_ctx,
			&pProbeReqInd->bssid, &sessionId);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		roam_info->u.pWPSPBCProbeReq = &pProbeReqInd->WPSPBCProbeReq;
		csr_roam_call_callback(mac_ctx, sessionId, roam_info,
				       0, eCSR_ROAM_WPS_PBC_PROBE_REQ_IND,
				       eCSR_ROAM_RESULT_WPS_PBC_PROBE_REQ_IND);
	}
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_wm_status_change_ntf(struct mac_context *mac_ctx,
				      tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;
	struct csr_roam_info *roam_info;
	struct wm_status_change_ntf *pStatusChangeMsg;
	struct ap_new_caps *pApNewCaps;
	eCsrRoamResult result = eCSR_ROAM_RESULT_NONE;
	eRoamCmdStatus roamStatus = eCSR_ROAM_FAILED;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	pStatusChangeMsg = (struct wm_status_change_ntf *) msg_ptr;
	switch (pStatusChangeMsg->statusChangeCode) {
	/*
	 * detection by LIM that the capabilities of the associated
	 * AP have changed.
	 */
	case eSIR_SME_AP_CAPS_CHANGED:
		pApNewCaps = &pStatusChangeMsg->statusChangeInfo.apNewCaps;
		sme_debug("CSR handling eSIR_SME_AP_CAPS_CHANGED");
		status = csr_roam_get_session_id_from_bssid(mac_ctx,
					&pApNewCaps->bssId, &sessionId);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		if (eCSR_ROAMING_STATE_JOINED ==
		    sme_get_current_roam_state(MAC_HANDLE(mac_ctx), sessionId)
		    && ((eCSR_ROAM_SUBSTATE_JOINED_REALTIME_TRAFFIC
			== mac_ctx->roam.curSubState[sessionId])
		    || (eCSR_ROAM_SUBSTATE_NONE ==
			mac_ctx->roam.curSubState[sessionId])
		    || (eCSR_ROAM_SUBSTATE_JOINED_NON_REALTIME_TRAFFIC
			== mac_ctx->roam.curSubState[sessionId])
		    || (eCSR_ROAM_SUBSTATE_JOINED_NO_TRAFFIC ==
			 mac_ctx->roam.curSubState[sessionId]))) {
			sme_warn("Calling csr_roam_disconnect");
			csr_roam_disconnect(mac_ctx, sessionId,
					    eCSR_DISCONNECT_REASON_UNSPECIFIED,
					    REASON_UNSPEC_FAILURE);
		} else {
			sme_warn("Skipping the new scan as CSR is in state: %s and sub-state: %s",
				mac_trace_getcsr_roam_state(
					mac_ctx->roam.curState[sessionId]),
				mac_trace_getcsr_roam_sub_state(
					mac_ctx->roam.curSubState[sessionId]));
			/* We ignore the caps change event if CSR is not in full
			 * connected state. Send one event to PE to reset
			 * limSentCapsChangeNtf Once limSentCapsChangeNtf set
			 * 0, lim can send sub sequent CAPS change event
			 * otherwise lim cannot send any CAPS change events to
			 * SME
			 */
			csr_send_reset_ap_caps_changed(mac_ctx,
						       &pApNewCaps->bssId);
		}
		break;

	default:
		roamStatus = eCSR_ROAM_FAILED;
		result = eCSR_ROAM_RESULT_NONE;
		break;
	} /* end switch on statusChangeCode */
	if (eCSR_ROAM_RESULT_NONE != result) {
		csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
				       roamStatus, result);
	}
	qdf_mem_free(roam_info);
}

static void
csr_roam_chk_lnk_max_assoc_exceeded(struct mac_context *mac_ctx, tSirSmeRsp *msg_ptr)
{
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	tSmeMaxAssocInd *pSmeMaxAssocInd;
	struct csr_roam_info *roam_info;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return;
	pSmeMaxAssocInd = (tSmeMaxAssocInd *) msg_ptr;
	sme_debug(
		"max assoc have been reached, new peer cannot be accepted");
	sessionId = pSmeMaxAssocInd->sessionId;
	roam_info->sessionId = sessionId;
	qdf_copy_macaddr(&roam_info->peerMac, &pSmeMaxAssocInd->peer_mac);
	csr_roam_call_callback(mac_ctx, sessionId, roam_info, 0,
			       eCSR_ROAM_INFRA_IND,
			       eCSR_ROAM_RESULT_MAX_ASSOC_EXCEEDED);
	qdf_mem_free(roam_info);
}

void csr_roam_check_for_link_status_change(struct mac_context *mac,
						tSirSmeRsp *pSirMsg)
{
	if (!pSirMsg) {
		sme_err("pSirMsg is NULL");
		return;
	}
	switch (pSirMsg->messageType) {
	case eWNI_SME_ASSOC_IND:
		csr_roam_chk_lnk_assoc_ind(mac, pSirMsg);
		break;
	case eWNI_SME_ASSOC_IND_UPPER_LAYER:
		csr_roam_chk_lnk_assoc_ind_upper_layer(mac, pSirMsg);
		break;
	case eWNI_SME_DISASSOC_IND:
		csr_roam_chk_lnk_disassoc_ind(mac, pSirMsg);
		break;
	case eWNI_SME_DISCONNECT_DONE_IND:
		csr_roam_send_disconnect_done_indication(mac, pSirMsg);
		break;
	case eWNI_SME_DEAUTH_IND:
		csr_roam_chk_lnk_deauth_ind(mac, pSirMsg);
		break;
	case eWNI_SME_SWITCH_CHL_IND:
		csr_roam_chk_lnk_swt_ch_ind(mac, pSirMsg);
		break;
	case eWNI_SME_DEAUTH_RSP:
		csr_roam_chk_lnk_deauth_rsp(mac, pSirMsg);
		break;
	case eWNI_SME_DISASSOC_RSP:
		csr_roam_chk_lnk_disassoc_rsp(mac, pSirMsg);
		break;
	case eWNI_SME_MIC_FAILURE_IND:
		csr_roam_chk_lnk_mic_fail_ind(mac, pSirMsg);
		break;
	case eWNI_SME_WPS_PBC_PROBE_REQ_IND:
		csr_roam_chk_lnk_pbs_probe_req_ind(mac, pSirMsg);
		break;
	case eWNI_SME_WM_STATUS_CHANGE_NTF:
		csr_roam_chk_lnk_wm_status_change_ntf(mac, pSirMsg);
		break;
	case eWNI_SME_SETCONTEXT_RSP:
		csr_roam_chk_lnk_set_ctx_rsp(mac, pSirMsg);
		break;
#ifdef FEATURE_WLAN_ESE
	case eWNI_SME_GET_TSM_STATS_RSP:
		sme_debug("TSM Stats rsp from PE");
		csr_tsm_stats_rsp_processor(mac, pSirMsg);
		break;
#endif /* FEATURE_WLAN_ESE */
	case eWNI_SME_GET_SNR_REQ:
		sme_debug("GetSnrReq from self");
		csr_update_snr(mac, pSirMsg);
		break;
	case eWNI_SME_FT_PRE_AUTH_RSP:
		csr_roam_ft_pre_auth_rsp_processor(mac,
						(tpSirFTPreAuthRsp) pSirMsg);
		break;
	case eWNI_SME_MAX_ASSOC_EXCEEDED:
		csr_roam_chk_lnk_max_assoc_exceeded(mac, pSirMsg);
		break;
	case eWNI_SME_CANDIDATE_FOUND_IND:
		csr_neighbor_roam_candidate_found_ind_hdlr(mac, pSirMsg);
		break;
	case eWNI_SME_HANDOFF_REQ:
		sme_debug("Handoff Req from self");
		csr_neighbor_roam_handoff_req_hdlr(mac, pSirMsg);
		break;

	default:
		break;
	} /* end switch on message type */
}

void csr_call_roaming_completion_callback(struct mac_context *mac,
					  struct csr_roam_session *pSession,
					  struct csr_roam_info *roam_info,
					  uint32_t roamId,
					  eCsrRoamResult roamResult)
{
	if (pSession) {
		if (pSession->bRefAssocStartCnt) {
			pSession->bRefAssocStartCnt--;

			if (0 != pSession->bRefAssocStartCnt) {
				QDF_ASSERT(pSession->bRefAssocStartCnt == 0);
				return;
			}
			/* Need to call association_completion because there
			 * is an assoc_start pending.
			 */
			csr_roam_call_callback(mac, pSession->sessionId, NULL,
					       roamId,
					       eCSR_ROAM_ASSOCIATION_COMPLETION,
					       eCSR_ROAM_RESULT_FAILURE);
		}
		csr_roam_call_callback(mac, pSession->sessionId, roam_info,
				       roamId, eCSR_ROAM_ROAMING_COMPLETION,
				       roamResult);
	} else
		sme_err("pSession is NULL");
}

/* return a bool to indicate whether roaming completed or continue. */
bool csr_roam_complete_roaming(struct mac_context *mac, uint32_t sessionId,
			       bool fForce, eCsrRoamResult roamResult)
{
	bool fCompleted = true;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found ", sessionId);
		return false;
	}
	/* Check whether time is up */
	if (pSession->fCancelRoaming || fForce ||
	    eCsrReassocRoaming == pSession->roamingReason ||
	    eCsrDynamicRoaming == pSession->roamingReason) {
		sme_debug("indicates roaming completion");
		if (pSession->fCancelRoaming
		    && CSR_IS_LOSTLINK_ROAMING(pSession->roamingReason)) {
			/* roaming is cancelled, tell HDD to indicate disconnect
			 * Because LIM overload deauth_ind for both deauth frame
			 * and missed beacon we need to use this logic to
			 * detinguish it. For missed beacon, LIM set reason to
			 * be eSIR_BEACON_MISSED
			 */
			if (pSession->roamingStatusCode ==
			    REASON_BEACON_MISSED) {
				roamResult = eCSR_ROAM_RESULT_LOSTLINK;
			} else if (eCsrLostlinkRoamingDisassoc ==
				   pSession->roamingReason) {
				roamResult = eCSR_ROAM_RESULT_DISASSOC_IND;
			} else if (eCsrLostlinkRoamingDeauth ==
				   pSession->roamingReason) {
				roamResult = eCSR_ROAM_RESULT_DEAUTH_IND;
			} else {
				roamResult = eCSR_ROAM_RESULT_LOSTLINK;
			}
		}
		csr_call_roaming_completion_callback(mac, pSession, NULL, 0,
						     roamResult);
		pSession->roamingReason = eCsrNotRoaming;
	} else {
		pSession->roamResult = roamResult;
		if (!QDF_IS_STATUS_SUCCESS(csr_roam_start_roaming_timer(mac,
					sessionId, QDF_MC_TIMER_TO_SEC_UNIT))) {
			csr_call_roaming_completion_callback(mac, pSession,
							NULL, 0, roamResult);
			pSession->roamingReason = eCsrNotRoaming;
		} else {
			fCompleted = false;
		}
	}
	return fCompleted;
}

void csr_roam_cancel_roaming(struct mac_context *mac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session: %d not found", sessionId);
		return;
	}

	if (CSR_IS_ROAMING(pSession)) {
		sme_debug("Cancel roaming");
		pSession->fCancelRoaming = true;
		if (CSR_IS_ROAM_JOINING(mac, sessionId)
		    && CSR_IS_ROAM_SUBSTATE_CONFIG(mac, sessionId)) {
			/* No need to do anything in here because the handler
			 * takes care of it
			 */
		} else {
			eCsrRoamResult roamResult =
				CSR_IS_LOSTLINK_ROAMING(pSession->
							roamingReason) ?
				eCSR_ROAM_RESULT_LOSTLINK :
							eCSR_ROAM_RESULT_NONE;
			/* Roaming is stopped after here */
			csr_roam_complete_roaming(mac, sessionId, true,
						  roamResult);
			/* Since CSR may be in lostlink roaming situation,
			 * abort all roaming related activities
			 */
			csr_scan_abort_mac_scan(mac, sessionId, INVAL_SCAN_ID);
			csr_roam_stop_roaming_timer(mac, sessionId);
		}
	}
}

void csr_roam_roaming_timer_handler(void *pv)
{
	struct csr_timer_info *info = pv;
	struct mac_context *mac = info->mac;
	uint32_t vdev_id = info->vdev_id;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, vdev_id);

	if (!pSession) {
		sme_err("  session %d not found ", vdev_id);
		return;
	}

	if (false == pSession->fCancelRoaming) {
		csr_call_roaming_completion_callback(mac, pSession,
						NULL, 0,
						pSession->roamResult);
		pSession->roamingReason = eCsrNotRoaming;
	}
}

/**
 * csr_roam_roaming_offload_timeout_handler() - Handler for roaming failure
 * @timer_data: Carries the mac_ctx and session info
 *
 * This function would be invoked when the roaming_offload_timer expires.
 * The timer is waiting in anticipation of a related roaming event from
 * the firmware after receiving the ROAM_START event.
 *
 * Return: None
 */
void csr_roam_roaming_offload_timeout_handler(void *timer_data)
{
	struct csr_timer_info *timer_info = timer_data;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_roam_after_data_stall *vdev_roam_params;
	struct scheduler_msg wma_msg = {0};
	struct roam_sync_timeout_timer_info *req;
	QDF_STATUS status;

	if (timer_info) {
		sme_debug("LFR3:roaming offload timer expired, session: %d",
			  timer_info->vdev_id);
	} else {
		sme_err("Invalid Session");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(timer_info->mac->psoc,
						    timer_info->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL, aborting roam start");
		return;
	}

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);
	if (!vdev_roam_params) {
		sme_err("Invalid vdev roam params, aborting timeout handler");
		status = QDF_STATUS_E_NULL_VALUE;
		goto rel;
	}

	req = qdf_mem_malloc(sizeof(*req));
	if (!req)
		goto rel;

	req->vdev_id = timer_info->vdev_id;
	wma_msg.type = WMA_ROAM_SYNC_TIMEOUT;
	wma_msg.bodyptr = req;

	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_WMA,
					QDF_MODULE_ID_WMA, &wma_msg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Post roam offload timer fails, session id: %d",
			timer_info->vdev_id);
		qdf_mem_free(req);
		goto rel;
	}

	vdev_roam_params->roam_invoke_in_progress = false;

rel:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

static void csr_roam_invoke_timeout_handler(void *data)
{
	struct csr_timer_info *info = data;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_roam_after_data_stall *vdev_roam_params;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(info->mac->psoc,
						    info->vdev_id,
						    WLAN_LEGACY_SME_ID);

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);
	if (!vdev_roam_params) {
		sme_err("Invalid vdev roam params, aborting timeout handler");
		goto rel;
	}

	sme_debug("Roam invoke timer expired source %d nud behaviour %d",
		  vdev_roam_params->source, info->mac->nud_fail_behaviour);

	if (vdev_roam_params->source == USERSPACE_INITIATED ||
	    info->mac->nud_fail_behaviour == DISCONNECT_AFTER_ROAM_FAIL) {
		csr_roam_disconnect(info->mac, info->vdev_id,
				    eCSR_DISCONNECT_REASON_DEAUTH,
				    REASON_USER_TRIGGERED_ROAM_FAILURE);
	}

	vdev_roam_params->roam_invoke_in_progress = false;

rel:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

QDF_STATUS csr_roam_start_roaming_timer(struct mac_context *mac,
					uint32_t vdev_id,
					uint32_t interval)
{
	QDF_STATUS status;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, vdev_id);

	if (!pSession) {
		sme_err("session %d not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("csrScanStartRoamingTimer");
	pSession->roamingTimerInfo.vdev_id = (uint8_t) vdev_id;
	status = qdf_mc_timer_start(&pSession->hTimerRoaming,
				    interval / QDF_MC_TIMER_TO_MS_UNIT);

	return status;
}

QDF_STATUS csr_roam_stop_roaming_timer(struct mac_context *mac,
		uint32_t sessionId)
{
	return qdf_mc_timer_stop
			(&mac->roam.roamSession[sessionId].hTimerRoaming);
}

void csr_roam_wait_for_key_time_out_handler(void *pv)
{
	struct csr_timer_info *info = pv;
	struct mac_context *mac = info->mac;
	uint8_t vdev_id = info->vdev_id;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, vdev_id);
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!pSession) {
		sme_err("session not found");
		return;
	}

	sme_debug("WaitForKey timer expired in state: %s sub-state: %s",
		mac_trace_get_neighbour_roam_state(mac->roam.
					neighborRoamInfo[vdev_id].
						   neighborRoamState),
		mac_trace_getcsr_roam_sub_state(mac->roam.
						curSubState[vdev_id]));
	spin_lock(&mac->roam.roam_state_lock);
	if (CSR_IS_WAIT_FOR_KEY(mac, vdev_id)) {
		/* Change the substate so command queue is unblocked. */
		if (vdev_id < WLAN_MAX_VDEVS)
			mac->roam.curSubState[vdev_id] =
						eCSR_ROAM_SUBSTATE_NONE;
		spin_unlock(&mac->roam.roam_state_lock);

		if (csr_neighbor_roam_is_handoff_in_progress(mac, vdev_id)) {
			/*
			 * Enable heartbeat timer when hand-off is in progress
			 * and Key Wait timer expired.
			 */
			sme_debug("Enabling HB timer after WaitKey expiry nHBCount: %d)",
				mac->roam.configParam.HeartbeatThresh24);
			if (cfg_in_range(CFG_HEART_BEAT_THRESHOLD,
					 mac->roam.configParam.
					 HeartbeatThresh24))
				mac->mlme_cfg->timeouts.heart_beat_threshold =
				mac->roam.configParam.HeartbeatThresh24;
			else
				mac->mlme_cfg->timeouts.heart_beat_threshold =
					cfg_default(CFG_HEART_BEAT_THRESHOLD);
		}
		sme_debug("SME pre-auth state timeout");

		status = sme_acquire_global_lock(&mac->sme);
		if (csr_is_conn_state_connected_infra(mac, vdev_id)) {
			csr_roam_link_up(mac,
					 pSession->connectedProfile.bssid);
			if (QDF_IS_STATUS_SUCCESS(status)) {
				csr_roam_disconnect(mac, vdev_id,
					eCSR_DISCONNECT_REASON_UNSPECIFIED,
					REASON_KEY_TIMEOUT);
			}
		} else {
			sme_err("session not found");
		}
		sme_release_global_lock(&mac->sme);
	} else {
		spin_unlock(&mac->roam.roam_state_lock);
	}

}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * csr_roam_roaming_offload_timer_action() - API to start/stop the timer
 * @mac_ctx: MAC Context
 * @interval: Value to be set for the timer
 * @vdev_id: Session on which the timer should be operated
 * @action: Start/Stop action for the timer
 *
 * API to start/stop the roaming offload timer
 *
 * Return: None
 */
void csr_roam_roaming_offload_timer_action(
		struct mac_context *mac_ctx, uint32_t interval, uint8_t vdev_id,
		uint8_t action)
{
	struct csr_roam_session *csr_session = CSR_GET_SESSION(mac_ctx,
				vdev_id);
	QDF_TIMER_STATE tstate;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			("LFR3: timer action %d, session %d, intvl %d"),
			action, vdev_id, interval);
	if (mac_ctx) {
		csr_session = CSR_GET_SESSION(mac_ctx, vdev_id);
	} else {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				("LFR3: Invalid MAC Context"));
		return;
	}
	if (!csr_session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				("LFR3: session %d not found"), vdev_id);
		return;
	}
	csr_session->roamingTimerInfo.vdev_id = (uint8_t) vdev_id;

	tstate =
	   qdf_mc_timer_get_current_state(&csr_session->roaming_offload_timer);

	if (action == ROAMING_OFFLOAD_TIMER_START) {
		/*
		 * If timer is already running then re-start timer in order to
		 * process new ROAM_START with fresh timer.
		 */
		if (tstate == QDF_TIMER_STATE_RUNNING)
			qdf_mc_timer_stop(&csr_session->roaming_offload_timer);
		qdf_mc_timer_start(&csr_session->roaming_offload_timer,
				   interval / QDF_MC_TIMER_TO_MS_UNIT);
	}
	if (action == ROAMING_OFFLOAD_TIMER_STOP)
		qdf_mc_timer_stop(&csr_session->roaming_offload_timer);

}

static QDF_STATUS csr_roam_invoke_timer_init(
					struct csr_roam_session *csr_session)
{
	QDF_STATUS status;

	status = qdf_mc_timer_init(&csr_session->roam_invoke_timer,
				   QDF_TIMER_TYPE_WAKE_APPS,
				   csr_roam_invoke_timeout_handler,
				   &csr_session->roam_invoke_timer_info);

	if (QDF_IS_STATUS_ERROR(status))
		sme_err("timer init failed for roam invoke timer");

	return status;
}

static QDF_STATUS csr_roam_invoke_timer_destroy(
					struct csr_roam_session *csr_session)
{
	QDF_STATUS status;

	status = qdf_mc_timer_destroy(&csr_session->roam_invoke_timer);

	if (QDF_IS_STATUS_ERROR(status))
		sme_err("timer deinit failed for roam invoke timer");

	return status;
}

static QDF_STATUS csr_roam_invoke_timer_stop(struct mac_context *mac_ctx,
					     uint8_t vdev_id)
{
	struct csr_roam_session *csr_session;

	csr_session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!csr_session) {
		sme_err("LFR3: session %d not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (QDF_TIMER_STATE_RUNNING ==
	    qdf_mc_timer_get_current_state(&csr_session->roam_invoke_timer)) {
		sme_debug("Stop Roam invoke timer for session ID: %d", vdev_id);
		qdf_mc_timer_stop(&csr_session->roam_invoke_timer);
		return QDF_STATUS_SUCCESS;
	}

	return QDF_STATUS_E_FAILURE;
}

#else
static inline QDF_STATUS csr_roam_invoke_timer_init(
					struct csr_roam_session *csr_session)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS csr_roam_invoke_timer_destroy(
					struct csr_roam_session *session)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS csr_roam_invoke_timer_stop(struct mac_context *mac_ctx,
						    uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

static QDF_STATUS csr_roam_start_wait_for_key_timer(
		struct mac_context *mac, uint32_t interval)
{
	QDF_STATUS status;
	uint8_t session_id = mac->roam.WaitForKeyTimerInfo.vdev_id;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[session_id];

	if (csr_neighbor_roam_is_handoff_in_progress(mac, session_id)) {
		/* Disable heartbeat timer when hand-off is in progress */
		sme_debug("disabling HB timer in state: %s sub-state: %s",
			mac_trace_get_neighbour_roam_state(
				pNeighborRoamInfo->neighborRoamState),
			mac_trace_getcsr_roam_sub_state(
				mac->roam.curSubState[session_id]));
		mac->mlme_cfg->timeouts.heart_beat_threshold = 0;
	}
	sme_debug("csrScanStartWaitForKeyTimer");
	status = qdf_mc_timer_start(&mac->roam.hTimerWaitForKey,
				    interval / QDF_MC_TIMER_TO_MS_UNIT);

	return status;
}

QDF_STATUS csr_roam_stop_wait_for_key_timer(struct mac_context *mac)
{
	uint8_t vdev_id = mac->roam.WaitForKeyTimerInfo.vdev_id;
	tpCsrNeighborRoamControlInfo pNeighborRoamInfo =
		&mac->roam.neighborRoamInfo[vdev_id];

	sme_debug("WaitForKey timer stopped in state: %s sub-state: %s",
		mac_trace_get_neighbour_roam_state(pNeighborRoamInfo->
						   neighborRoamState),
		mac_trace_getcsr_roam_sub_state(mac->roam.
						curSubState[vdev_id]));
	if (csr_neighbor_roam_is_handoff_in_progress(mac, vdev_id)) {
		/*
		 * Enable heartbeat timer when hand-off is in progress
		 * and Key Wait timer got stopped for some reason
		 */
		sme_debug("Enabling HB timer after WaitKey stop nHBCount: %d",
			mac->roam.configParam.HeartbeatThresh24);
		if (cfg_in_range(CFG_HEART_BEAT_THRESHOLD,
				 mac->roam.configParam.HeartbeatThresh24))
			mac->mlme_cfg->timeouts.heart_beat_threshold =
				mac->roam.configParam.HeartbeatThresh24;
		else
			mac->mlme_cfg->timeouts.heart_beat_threshold =
				cfg_default(CFG_HEART_BEAT_THRESHOLD);
	}
	return qdf_mc_timer_stop(&mac->roam.hTimerWaitForKey);
}

void csr_roam_completion(struct mac_context *mac, uint32_t vdev_id,
			 struct csr_roam_info *roam_info, tSmeCmd *pCommand,
			 eCsrRoamResult roamResult, bool fSuccess)
{
	eRoamCmdStatus roamStatus = csr_get_roam_complete_status(mac,
								vdev_id);
	uint32_t roamId = 0;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, vdev_id);

	if (!pSession) {
		sme_err("session: %d not found ", vdev_id);
		return;
	}

	if (pCommand) {
		roamId = pCommand->u.roamCmd.roamId;
		if (vdev_id != pCommand->vdev_id) {
			QDF_ASSERT(vdev_id == pCommand->vdev_id);
			return;
		}
	}
	if (eCSR_ROAM_ROAMING_COMPLETION == roamStatus)
		/* if success, force roaming completion */
		csr_roam_complete_roaming(mac, vdev_id, fSuccess,
								roamResult);
	else {
		if (pSession->bRefAssocStartCnt != 0) {
			QDF_ASSERT(pSession->bRefAssocStartCnt == 0);
			return;
		}
		sme_debug("indicates association completion roamResult: %d",
			roamResult);
		csr_roam_call_callback(mac, vdev_id, roam_info, roamId,
				       roamStatus, roamResult);
	}
}

static
QDF_STATUS csr_roam_lost_link(struct mac_context *mac, uint32_t sessionId,
			      uint32_t type, tSirSmeRsp *pSirMsg)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct deauth_ind *pDeauthIndMsg = NULL;
	struct disassoc_ind *pDisassocIndMsg = NULL;
	eCsrRoamResult result = eCSR_ROAM_RESULT_LOSTLINK;
	struct csr_roam_info *roam_info;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	bool from_ap = false;

	if (!pSession) {
		sme_err("session: %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;
	if (eWNI_SME_DISASSOC_IND == type) {
		result = eCSR_ROAM_RESULT_DISASSOC_IND;
		pDisassocIndMsg = (struct disassoc_ind *)pSirMsg;
		pSession->roamingStatusCode = pDisassocIndMsg->status_code;
		pSession->joinFailStatusCode.reasonCode =
			pDisassocIndMsg->reasonCode;
		from_ap = pDisassocIndMsg->from_ap;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDisassocIndMsg->peer_macaddr);
	} else if (eWNI_SME_DEAUTH_IND == type) {
		result = eCSR_ROAM_RESULT_DEAUTH_IND;
		pDeauthIndMsg = (struct deauth_ind *)pSirMsg;
		pSession->roamingStatusCode = pDeauthIndMsg->status_code;
		pSession->joinFailStatusCode.reasonCode =
			pDeauthIndMsg->reasonCode;
		from_ap = pDeauthIndMsg->from_ap;
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDeauthIndMsg->peer_macaddr);

	} else {
		sme_warn("gets an unknown type (%d)", type);
		result = eCSR_ROAM_RESULT_NONE;
		pSession->joinFailStatusCode.reasonCode = 1;
	}

	mlme_set_discon_reason_n_from_ap(mac->psoc, sessionId, from_ap,
				      pSession->joinFailStatusCode.reasonCode);


	csr_roam_call_callback(mac, sessionId, NULL, 0,
			       eCSR_ROAM_LOSTLINK_DETECTED, result);

	if (eWNI_SME_DISASSOC_IND == type)
		status = csr_send_mb_disassoc_cnf_msg(mac, pDisassocIndMsg);
	else if (eWNI_SME_DEAUTH_IND == type)
		status = csr_send_mb_deauth_cnf_msg(mac, pDeauthIndMsg);

	/* prepare to tell HDD to disconnect */
	qdf_mem_zero(roam_info, sizeof(*roam_info));
	roam_info->status_code = (tSirResultCodes)pSession->roamingStatusCode;
	roam_info->reasonCode = pSession->joinFailStatusCode.reasonCode;
	if (eWNI_SME_DISASSOC_IND == type) {
		/* staMacAddr */
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDisassocIndMsg->peer_macaddr);
		roam_info->staId = (uint8_t)pDisassocIndMsg->staId;
		roam_info->reasonCode = pDisassocIndMsg->reasonCode;
	} else if (eWNI_SME_DEAUTH_IND == type) {
		/* staMacAddr */
		qdf_copy_macaddr(&roam_info->peerMac,
				 &pDeauthIndMsg->peer_macaddr);
		roam_info->staId = (uint8_t)pDeauthIndMsg->staId;
		roam_info->reasonCode = pDeauthIndMsg->reasonCode;
		roam_info->rxRssi = pDeauthIndMsg->rssi;
	}
	sme_debug("roamInfo.staId: %d", roam_info->staId);
/* Dont initiate internal driver based roaming after disconnection*/
	qdf_mem_free(roam_info);
	return status;
}

void csr_roam_get_disconnect_stats_complete(struct mac_context *mac)
{
	tListElem *entry;
	tSmeCmd *cmd;

	entry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (!entry) {
		sme_err("NO commands are ACTIVE ...");
		return;
	}

	cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
	if (cmd->command != eSmeCommandGetdisconnectStats) {
		sme_err("Get disconn stats cmd is not ACTIVE ...");
		return;
	}

	if (csr_nonscan_active_ll_remove_entry(mac, entry, LL_ACCESS_LOCK))
		csr_release_command(mac, cmd);
	else
		sme_err("Failed to release command");
}

void csr_roam_wm_status_change_complete(struct mac_context *mac,
					uint8_t session_id)
{
	tListElem *pEntry;
	tSmeCmd *pCommand;

	pEntry = csr_nonscan_active_ll_peek_head(mac, LL_ACCESS_LOCK);
	if (pEntry) {
		pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
		if (eSmeCommandWmStatusChange == pCommand->command) {
			/* Nothing to process in a Lost Link completion....  It just kicks off a */
			/* roaming sequence. */
			if (csr_nonscan_active_ll_remove_entry(mac, pEntry,
				    LL_ACCESS_LOCK)) {
				csr_release_command(mac, pCommand);
			} else {
				sme_err("Failed to release command");
			}
		} else {
			sme_warn("CSR: LOST LINK command is not ACTIVE ...");
		}
	} else {
		sme_warn("CSR: NO commands are ACTIVE ...");
	}
}

void csr_roam_process_get_disconnect_stats_command(struct mac_context *mac,
						   tSmeCmd *cmd)
{
	csr_get_peer_rssi(mac, cmd->vdev_id,
			  cmd->u.disconnect_stats_cmd.peer_mac_addr);
}

void csr_roam_process_wm_status_change_command(
		struct mac_context *mac, tSmeCmd *pCommand)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	tSirSmeRsp *pSirSmeMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac,
						pCommand->vdev_id);

	if (!pSession) {
		sme_err("session %d not found", pCommand->vdev_id);
		goto end;
	}
	sme_debug("session:%d, CmdType : %d",
		pCommand->vdev_id, pCommand->u.wmStatusChangeCmd.Type);

	switch (pCommand->u.wmStatusChangeCmd.Type) {
	case eCsrDisassociated:
		pSirSmeMsg =
			(tSirSmeRsp *) &pCommand->u.wmStatusChangeCmd.u.
			DisassocIndMsg;
		status =
			csr_roam_lost_link(mac, pCommand->vdev_id,
					   eWNI_SME_DISASSOC_IND, pSirSmeMsg);
		break;
	case eCsrDeauthenticated:
		pSirSmeMsg =
			(tSirSmeRsp *) &pCommand->u.wmStatusChangeCmd.u.
			DeauthIndMsg;
		status =
			csr_roam_lost_link(mac, pCommand->vdev_id,
					   eWNI_SME_DEAUTH_IND, pSirSmeMsg);
		break;
	default:
		sme_warn("gets an unknown command %d",
			pCommand->u.wmStatusChangeCmd.Type);
		break;
	}

end:
	if (status != QDF_STATUS_SUCCESS) {
		/*
		 * As status returned is not success, there is nothing else
		 * left to do so release WM status change command here.
		 */
		csr_roam_wm_status_change_complete(mac, pCommand->vdev_id);
	}
}

/**
 * csr_compute_mode_and_band() - computes dot11mode
 * @mac: mac global context
 * @dot11_mode: out param, do11 mode calculated
 * @band: out param, band caclculated
 * @opr_ch_freq: operating channel freq in MHz
 *
 * This function finds dot11 mode based on current mode, operating channel and
 * fw supported modes.
 *
 * Return: void
 */
static void
csr_compute_mode_and_band(struct mac_context *mac_ctx,
			  enum csr_cfgdot11mode *dot11_mode,
			  enum reg_wifi_band *band,
			  uint32_t opr_ch_freq)
{
	bool vht_24_ghz = mac_ctx->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band;

	switch (mac_ctx->roam.configParam.uCfgDot11Mode) {
	case eCSR_CFG_DOT11_MODE_11A:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
		*band = REG_BAND_5G;
		break;
	case eCSR_CFG_DOT11_MODE_11B:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
		*band = REG_BAND_2G;
		break;
	case eCSR_CFG_DOT11_MODE_11G:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11G;
		*band = REG_BAND_2G;
		break;
	case eCSR_CFG_DOT11_MODE_11N:
		*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	case eCSR_CFG_DOT11_MODE_11AC:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	case eCSR_CFG_DOT11_MODE_11AC_ONLY:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC_ONLY;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	case eCSR_CFG_DOT11_MODE_11AX:
	case eCSR_CFG_DOT11_MODE_11AX_ONLY:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AX)) {
			*dot11_mode = mac_ctx->roam.configParam.uCfgDot11Mode;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band, check
			 * for INI item to disable VHT operation in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	case eCSR_CFG_DOT11_MODE_AUTO:
		if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AX)) {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11AX;
		} else if (IS_FEATURE_SUPPORTED_BY_FW(DOT11AC)) {
			/*
			 * If the operating channel is in 2.4 GHz band,
			 * check for INI item to disable VHT operation
			 * in 2.4 GHz band
			 */
			if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq) &&
			    !vht_24_ghz)
				/* Disable 11AC operation */
				*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
			else
				*dot11_mode = eCSR_CFG_DOT11_MODE_11AC;
		} else {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11N;
		}
		*band = wlan_reg_freq_to_band(opr_ch_freq);
		break;
	default:
		/*
		 * Global dot11 Mode setting is 11a/b/g. use the channel number
		 * to determine the Mode setting.
		 */
		if (eCSR_OPERATING_CHANNEL_AUTO == opr_ch_freq) {
			*band = (mac_ctx->mlme_cfg->gen.band == BAND_2G ?
				REG_BAND_2G : REG_BAND_5G);
			if (REG_BAND_2G == *band) {
				*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
			} else {
				/* prefer 5GHz */
				*band = REG_BAND_5G;
				*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
			}
		} else if (WLAN_REG_IS_24GHZ_CH_FREQ(opr_ch_freq)) {
			*dot11_mode = eCSR_CFG_DOT11_MODE_11B;
			*band = REG_BAND_2G;
		} else {
			/* else, it's a 5.0GHz channel.  Set mode to 11a. */
			*dot11_mode = eCSR_CFG_DOT11_MODE_11A;
			*band = REG_BAND_5G;
		}
		break;
	} /* switch */
}

/**
 * csr_roam_get_phy_mode_band_for_bss() - This function returns band and mode
 * information.
 * @mac_ctx:       mac global context
 * @profile:       bss profile
 * @bss_op_ch_freq:operating channel freq in MHz
 * @band:          out param, band caclculated
 *
 * This function finds dot11 mode based on current mode, operating channel and
 * fw supported modes. The only tricky part is that if phyMode is set to 11abg,
 * this function may return eCSR_CFG_DOT11_MODE_11B instead of
 * eCSR_CFG_DOT11_MODE_11G if everything is set to auto-pick.
 *
 * Return: dot11mode
 */
static enum csr_cfgdot11mode
csr_roam_get_phy_mode_band_for_bss(struct mac_context *mac_ctx,
				   struct csr_roam_profile *profile,
				   uint32_t bss_op_ch_freq,
				   enum reg_wifi_band *p_band)
{
	enum reg_wifi_band band = REG_BAND_2G;
	uint8_t opr_freq = 0;
	enum csr_cfgdot11mode curr_mode =
		mac_ctx->roam.configParam.uCfgDot11Mode;
	enum csr_cfgdot11mode cfg_dot11_mode =
		csr_get_cfg_dot11_mode_from_csr_phy_mode(
			profile,
			(eCsrPhyMode) profile->phyMode,
			mac_ctx->roam.configParam.ProprietaryRatesEnabled);

	if (bss_op_ch_freq)
		opr_freq = bss_op_ch_freq;
	/*
	 * If the global setting for dot11Mode is set to auto/abg, we overwrite
	 * the setting in the profile.
	 */
	if ((!CSR_IS_INFRA_AP(profile)
	    && ((eCSR_CFG_DOT11_MODE_AUTO == curr_mode)
	    || (eCSR_CFG_DOT11_MODE_ABG == curr_mode)))
	    || (eCSR_CFG_DOT11_MODE_AUTO == cfg_dot11_mode)
	    || (eCSR_CFG_DOT11_MODE_ABG == cfg_dot11_mode)) {
		csr_compute_mode_and_band(mac_ctx, &cfg_dot11_mode,
					  &band, bss_op_ch_freq);
	} /* if( eCSR_CFG_DOT11_MODE_ABG == cfg_dot11_mode ) */
	else {
		/* dot11 mode is set, lets pick the band */
		if (0 == opr_freq) {
			/* channel is Auto also. */
			if (mac_ctx->mlme_cfg->gen.band == BAND_ALL) {
				/* prefer 5GHz */
				band = REG_BAND_5G;
			}
		} else{
			band = wlan_reg_freq_to_band(bss_op_ch_freq);
		}
	}
	if (p_band)
		*p_band = band;

	if (opr_freq == 2484 && wlan_reg_is_24ghz_ch_freq(bss_op_ch_freq)) {
		sme_err("Switching to Dot11B mode");
		cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11B;
	}

	if (wlan_reg_is_24ghz_ch_freq(bss_op_ch_freq) &&
	    !mac_ctx->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band &&
	    (eCSR_CFG_DOT11_MODE_11AC == cfg_dot11_mode ||
	    eCSR_CFG_DOT11_MODE_11AC_ONLY == cfg_dot11_mode))
		cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11N;
	/*
	 * Incase of WEP Security encryption type is coming as part of add key.
	 * So while STart BSS dont have information
	 */
	if ((!CSR_IS_11n_ALLOWED(profile->EncryptionType.encryptionType[0])
	    || ((profile->privacy == 1)
		&& (profile->EncryptionType.encryptionType[0] ==
		eCSR_ENCRYPT_TYPE_NONE)))
		&& ((eCSR_CFG_DOT11_MODE_11N == cfg_dot11_mode) ||
		    (eCSR_CFG_DOT11_MODE_11AC == cfg_dot11_mode) ||
		    (eCSR_CFG_DOT11_MODE_11AX == cfg_dot11_mode))) {
		/* We cannot do 11n here */
		if (wlan_reg_is_24ghz_ch_freq(bss_op_ch_freq))
			cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11G;
		else
			cfg_dot11_mode = eCSR_CFG_DOT11_MODE_11A;
	}
	sme_debug("dot11mode: %d phyMode %d fw sup AX %d", cfg_dot11_mode,
		  profile->phyMode, IS_FEATURE_SUPPORTED_BY_FW(DOT11AX));
	return cfg_dot11_mode;
}

QDF_STATUS csr_roam_issue_stop_bss(struct mac_context *mac,
		uint32_t sessionId, enum csr_roam_substate NewSubstate)
{
	QDF_STATUS status;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* Set the roaming substate to 'stop Bss request'... */
	csr_roam_substate_change(mac, NewSubstate, sessionId);

	/* attempt to stop the Bss (reason code is ignored...) */
	status = csr_send_mb_stop_bss_req_msg(mac, sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_warn(
			"csr_send_mb_stop_bss_req_msg failed with status %d",
			status);
	}
	return status;
}

QDF_STATUS csr_get_cfg_valid_channels(struct mac_context *mac,
				      uint32_t *ch_freq_list,
				      uint32_t *num_ch_freq)
{
	uint8_t num_chan_temp = 0;
	int i;
	uint32_t *valid_ch_freq_list =
				mac->mlme_cfg->reg.valid_channel_freq_list;

	*num_ch_freq = mac->mlme_cfg->reg.valid_channel_list_num;

	for (i = 0; i < *num_ch_freq; i++) {
		if (!wlan_reg_is_dsrc_freq(valid_ch_freq_list[i])) {
			ch_freq_list[num_chan_temp] = valid_ch_freq_list[i];
			num_chan_temp++;
		}
	}

	*num_ch_freq = num_chan_temp;
	return QDF_STATUS_SUCCESS;
}

int8_t csr_get_cfg_max_tx_power(struct mac_context *mac, uint32_t ch_freq)
{
	uint32_t cfg_length = 0;
	int8_t maxTxPwr = 0;
	tSirMacChanInfo *pCountryInfo = NULL;
	uint8_t count = 0;
	uint8_t maxChannels;
	int32_t rem_length = 0;

	if (WLAN_REG_IS_5GHZ_CH_FREQ(ch_freq)) {
		cfg_length = mac->mlme_cfg->power.max_tx_power_5.len;
	} else if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq)) {
		cfg_length = mac->mlme_cfg->power.max_tx_power_24.len;

	} else if (wlan_reg_is_6ghz_chan_freq(ch_freq)) {
		return wlan_reg_get_channel_reg_power_for_freq(mac->pdev,
							       ch_freq);
	} else {
		return maxTxPwr;
	}

	if (!cfg_length)
		goto error;

	pCountryInfo = qdf_mem_malloc(cfg_length);
	if (!pCountryInfo)
		goto error;

	if (WLAN_REG_IS_5GHZ_CH_FREQ(ch_freq)) {
		if (cfg_length > CFG_MAX_TX_POWER_5_LEN)
			goto error;
		qdf_mem_copy(pCountryInfo,
			     mac->mlme_cfg->power.max_tx_power_5.data,
			     cfg_length);
	} else if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq)) {
		if (cfg_length > CFG_MAX_TX_POWER_2_4_LEN)
			goto error;
		qdf_mem_copy(pCountryInfo,
			     mac->mlme_cfg->power.max_tx_power_24.data,
			     cfg_length);
	}

	/* Identify the channel and maxtxpower */
	rem_length = cfg_length;
	while (rem_length >= (sizeof(tSirMacChanInfo))) {
		maxChannels = pCountryInfo[count].numChannels;
		maxTxPwr = pCountryInfo[count].maxTxPower;
		count++;
		rem_length -= (sizeof(tSirMacChanInfo));

		if (ch_freq >= pCountryInfo[count].first_freq &&
		    ch_freq < (pCountryInfo[count].first_freq + maxChannels)) {
			break;
		}
	}

error:
	if (pCountryInfo)
		qdf_mem_free(pCountryInfo);

	return maxTxPwr;
}

bool csr_roam_is_channel_valid(struct mac_context *mac, uint32_t chan_freq)
{
	bool valid = false;
	uint32_t id_valid_ch;
	uint32_t len = sizeof(mac->roam.valid_ch_freq_list);

	if (QDF_IS_STATUS_SUCCESS(csr_get_cfg_valid_channels(mac,
					mac->roam.valid_ch_freq_list, &len))) {
		for (id_valid_ch = 0; (id_valid_ch < len);
		     id_valid_ch++) {
			if (chan_freq ==
			    mac->roam.valid_ch_freq_list[id_valid_ch]) {
				valid = true;
				break;
			}
		}
	}
	mac->roam.numValidChannels = len;
	return valid;
}

/* This function check and validate whether the NIC can do CB (40MHz) */
static ePhyChanBondState csr_get_cb_mode_from_ies(struct mac_context *mac,
						  uint32_t ch_freq,
						  tDot11fBeaconIEs *pIes)
{
	ePhyChanBondState eRet = PHY_SINGLE_CHANNEL_CENTERED;
	uint32_t sec_ch_freq = 0;
	uint32_t ChannelBondingMode;
	struct ch_params ch_params = {0};

	if (WLAN_REG_IS_24GHZ_CH_FREQ(ch_freq)) {
		ChannelBondingMode =
			mac->roam.configParam.channelBondingMode24GHz;
	} else {
		ChannelBondingMode =
			mac->roam.configParam.channelBondingMode5GHz;
	}

	if (WNI_CFG_CHANNEL_BONDING_MODE_DISABLE == ChannelBondingMode)
		return PHY_SINGLE_CHANNEL_CENTERED;

	/* Figure what the other side's CB mode */
	if (!(pIes->HTCaps.present && (eHT_CHANNEL_WIDTH_40MHZ ==
		pIes->HTCaps.supportedChannelWidthSet))) {
		return PHY_SINGLE_CHANNEL_CENTERED;
	}

	/* In Case WPA2 and TKIP is the only one cipher suite in Pairwise */
	if ((pIes->RSN.present && (pIes->RSN.pwise_cipher_suite_count == 1) &&
		!memcmp(&(pIes->RSN.pwise_cipher_suites[0][0]),
					"\x00\x0f\xac\x02", 4))
		/* In Case only WPA1 is supported and TKIP is
		 * the only one cipher suite in Unicast.
		 */
		|| (!pIes->RSN.present && (pIes->WPA.present &&
			(pIes->WPA.unicast_cipher_count == 1) &&
			!memcmp(&(pIes->WPA.unicast_ciphers[0][0]),
					"\x00\x50\xf2\x02", 4)))) {
		sme_debug("No channel bonding in TKIP mode");
		return PHY_SINGLE_CHANNEL_CENTERED;
	}

	if (!pIes->HTInfo.present)
		return PHY_SINGLE_CHANNEL_CENTERED;

	/*
	 * This is called during INFRA STA/CLIENT and should use the merged
	 * value of supported channel width and recommended tx width as per
	 * standard
	 */
	sme_debug("ch freq %d scws %u rtws %u sco %u", ch_freq,
		  pIes->HTCaps.supportedChannelWidthSet,
		  pIes->HTInfo.recommendedTxWidthSet,
		  pIes->HTInfo.secondaryChannelOffset);

	if (pIes->HTInfo.recommendedTxWidthSet == eHT_CHANNEL_WIDTH_40MHZ)
		eRet = (ePhyChanBondState)pIes->HTInfo.secondaryChannelOffset;
	else
		eRet = PHY_SINGLE_CHANNEL_CENTERED;

	switch (eRet) {
	case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		sec_ch_freq = ch_freq + CSR_SEC_CHANNEL_OFFSET;
		break;
	case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
		sec_ch_freq = ch_freq - CSR_SEC_CHANNEL_OFFSET;
		break;
	default:
		break;
	}

	if (eRet != PHY_SINGLE_CHANNEL_CENTERED) {
		ch_params.ch_width = CH_WIDTH_40MHZ;
		wlan_reg_set_channel_params_for_freq(mac->pdev, ch_freq,
						     sec_ch_freq, &ch_params);
		if (ch_params.ch_width == CH_WIDTH_20MHZ ||
		    ch_params.sec_ch_offset != eRet) {
			sme_err("ch freq %d :: Supported HT BW %d and cbmode %d, APs HT BW %d and cbmode %d, so switch to 20Mhz",
				ch_freq, ch_params.ch_width,
				ch_params.sec_ch_offset,
				pIes->HTInfo.recommendedTxWidthSet, eRet);
			eRet = PHY_SINGLE_CHANNEL_CENTERED;
		}
	}

	return eRet;
}

static bool csr_is_encryption_in_list(struct mac_context *mac,
				      tCsrEncryptionList *pCipherList,
				      eCsrEncryptionType encryptionType)
{
	bool fFound = false;
	uint32_t idx;

	for (idx = 0; idx < pCipherList->numEntries; idx++) {
		if (pCipherList->encryptionType[idx] == encryptionType) {
			fFound = true;
			break;
		}
	}
	return fFound;
}

static bool csr_is_auth_in_list(struct mac_context *mac, tCsrAuthList *pAuthList,
				enum csr_akm_type authType)
{
	bool fFound = false;
	uint32_t idx;

	for (idx = 0; idx < pAuthList->numEntries; idx++) {
		if (pAuthList->authType[idx] == authType) {
			fFound = true;
			break;
		}
	}
	return fFound;
}

bool csr_is_same_profile(struct mac_context *mac,
			 tCsrRoamConnectedProfile *pProfile1,
			 struct csr_roam_profile *pProfile2)
{
	uint32_t i;
	bool fCheck = false;

	if (!(pProfile1 && pProfile2))
		return fCheck;

	for (i = 0; i < pProfile2->SSIDs.numOfSSIDs; i++) {
		fCheck = csr_is_ssid_match(mac,
				pProfile2->SSIDs.SSIDList[i].SSID.ssId,
				pProfile2->SSIDs.SSIDList[i].SSID.length,
				pProfile1->SSID.ssId,
				pProfile1->SSID.length,
				false);
		if (fCheck)
			break;
	}
	if (!fCheck)
		goto exit;

	if (!csr_is_auth_in_list(mac, &pProfile2->AuthType,
				 pProfile1->AuthType)
	    || (pProfile2->BSSType != pProfile1->BSSType)
	    || !csr_is_encryption_in_list(mac, &pProfile2->EncryptionType,
					  pProfile1->EncryptionType)) {
		fCheck = false;
		goto exit;
	}
	if (pProfile1->mdid.mdie_present || pProfile2->mdid.mdie_present) {
		if (pProfile1->mdid.mobility_domain !=
		    pProfile2->mdid.mobility_domain) {
			fCheck = false;
			goto exit;
		}
	}
	/* Match found */
	fCheck = true;
exit:
	return fCheck;
}

static bool csr_roam_is_same_profile_keys(struct mac_context *mac,
				   tCsrRoamConnectedProfile *pConnProfile,
				   struct csr_roam_profile *pProfile2)
{
	bool fCheck = false;
	int i;

	do {
		/* Only check for static WEP */
		if (!csr_is_encryption_in_list
			    (mac, &pProfile2->EncryptionType,
			    eCSR_ENCRYPT_TYPE_WEP40_STATICKEY)
		    && !csr_is_encryption_in_list(mac,
				&pProfile2->EncryptionType,
				eCSR_ENCRYPT_TYPE_WEP104_STATICKEY)) {
			fCheck = true;
			break;
		}
		if (!csr_is_encryption_in_list
			    (mac, &pProfile2->EncryptionType,
			    pConnProfile->EncryptionType))
			break;
		if (pConnProfile->Keys.defaultIndex !=
		    pProfile2->Keys.defaultIndex)
			break;
		for (i = 0; i < CSR_MAX_NUM_KEY; i++) {
			if (pConnProfile->Keys.KeyLength[i] !=
			    pProfile2->Keys.KeyLength[i])
				break;
			if (qdf_mem_cmp(&pConnProfile->Keys.KeyMaterial[i],
					     &pProfile2->Keys.KeyMaterial[i],
					     pProfile2->Keys.KeyLength[i])) {
				break;
			}
		}
		if (i == CSR_MAX_NUM_KEY)
			fCheck = true;
	} while (0);
	return fCheck;
}

/**
 * csr_populate_basic_rates() - populates OFDM or CCK rates
 * @rates:         rate struct to populate
 * @is_ofdm_rates:          true: ofdm rates, false: cck rates
 * @is_basic_rates:        indicates if rates are to be masked with
 *                 CSR_DOT11_BASIC_RATE_MASK
 *
 * This function will populate OFDM or CCK rates
 *
 * Return: void
 */
static void
csr_populate_basic_rates(tSirMacRateSet *rate_set, bool is_ofdm_rates,
		bool is_basic_rates)
{
	int i = 0;
	uint8_t ofdm_rates[8] = {
		SIR_MAC_RATE_6,
		SIR_MAC_RATE_9,
		SIR_MAC_RATE_12,
		SIR_MAC_RATE_18,
		SIR_MAC_RATE_24,
		SIR_MAC_RATE_36,
		SIR_MAC_RATE_48,
		SIR_MAC_RATE_54
	};
	uint8_t cck_rates[4] = {
		SIR_MAC_RATE_1,
		SIR_MAC_RATE_2,
		SIR_MAC_RATE_5_5,
		SIR_MAC_RATE_11
	};

	if (is_ofdm_rates == true) {
		rate_set->numRates = 8;
		qdf_mem_copy(rate_set->rate, ofdm_rates, sizeof(ofdm_rates));
		if (is_basic_rates) {
			rate_set->rate[0] |= CSR_DOT11_BASIC_RATE_MASK;
			rate_set->rate[2] |= CSR_DOT11_BASIC_RATE_MASK;
			rate_set->rate[4] |= CSR_DOT11_BASIC_RATE_MASK;
		}
		for (i = 0; i < rate_set->numRates; i++)
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			("Default OFDM rate is %2x"), rate_set->rate[i]);
	} else {
		rate_set->numRates = 4;
		qdf_mem_copy(rate_set->rate, cck_rates, sizeof(cck_rates));
		if (is_basic_rates) {
			rate_set->rate[0] |= CSR_DOT11_BASIC_RATE_MASK;
			rate_set->rate[1] |= CSR_DOT11_BASIC_RATE_MASK;
			rate_set->rate[2] |= CSR_DOT11_BASIC_RATE_MASK;
			rate_set->rate[3] |= CSR_DOT11_BASIC_RATE_MASK;
		}
		for (i = 0; i < rate_set->numRates; i++)
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			("Default CCK rate is %2x"), rate_set->rate[i]);

	}
}

/**
 * csr_convert_mode_to_nw_type() - convert mode into network type
 * @dot11_mode:    dot11_mode
 * @band:          2.4 or 5 GHz
 *
 * Return: tSirNwType
 */
static tSirNwType
csr_convert_mode_to_nw_type(enum csr_cfgdot11mode dot11_mode,
			    enum reg_wifi_band band)
{
	switch (dot11_mode) {
	case eCSR_CFG_DOT11_MODE_11G:
		return eSIR_11G_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11B:
		return eSIR_11B_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11A:
		return eSIR_11A_NW_TYPE;
	case eCSR_CFG_DOT11_MODE_11N:
	default:
		/*
		 * Because LIM only verifies it against 11a, 11b or 11g, set
		 * only 11g or 11a here
		 */
		if (REG_BAND_2G == band)
			return eSIR_11G_NW_TYPE;
		else
			return eSIR_11A_NW_TYPE;
	}
	return eSIR_DONOT_USE_NW_TYPE;
}

/**
 * csr_populate_supported_rates_from_hostapd() - populates operational
 * and extended rates.
 * from hostapd.conf file
 * @opr_rates:		rate struct to populate operational rates
 * @ext_rates:		rate struct to populate extended rates
 * @profile:		bss profile
 *
 * Return: void
 */
static void csr_populate_supported_rates_from_hostapd(tSirMacRateSet *opr_rates,
		tSirMacRateSet *ext_rates,
		struct csr_roam_profile *profile)
{
	int i = 0;

	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("supported_rates: %d extended_rates: %d"),
			profile->supported_rates.numRates,
			profile->extended_rates.numRates);

	if (profile->supported_rates.numRates > WLAN_SUPPORTED_RATES_IE_MAX_LEN)
		profile->supported_rates.numRates =
			WLAN_SUPPORTED_RATES_IE_MAX_LEN;

	if (profile->extended_rates.numRates > WLAN_SUPPORTED_RATES_IE_MAX_LEN)
		profile->extended_rates.numRates =
			WLAN_SUPPORTED_RATES_IE_MAX_LEN;

	if (profile->supported_rates.numRates) {
		opr_rates->numRates = profile->supported_rates.numRates;
		qdf_mem_copy(opr_rates->rate,
				profile->supported_rates.rate,
				profile->supported_rates.numRates);
		for (i = 0; i < opr_rates->numRates; i++)
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("Supported Rate is %2x"), opr_rates->rate[i]);
	}
	if (profile->extended_rates.numRates) {
		ext_rates->numRates =
			profile->extended_rates.numRates;
		qdf_mem_copy(ext_rates->rate,
				profile->extended_rates.rate,
				profile->extended_rates.numRates);
		for (i = 0; i < ext_rates->numRates; i++)
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			FL("Extended Rate is %2x"), ext_rates->rate[i]);
	}
}

/**
 * csr_roam_get_bss_start_parms() - get bss start param from profile
 * @mac:          mac global context
 * @pProfile:      roam profile
 * @pParam:        out param, start bss params
 * @skip_hostapd_rate: to skip given hostapd's rate
 *
 * This function populates start bss param from roam profile
 *
 * Return: void
 */
static QDF_STATUS
csr_roam_get_bss_start_parms(struct mac_context *mac,
			     struct csr_roam_profile *pProfile,
			     struct csr_roamstart_bssparams *pParam,
			     bool skip_hostapd_rate)
{
	enum reg_wifi_band band;
	uint32_t opr_ch_freq = 0;
	tSirNwType nw_type;
	uint32_t tmp_opr_ch_freq = 0;
	uint8_t h2e;
	tSirMacRateSet *opr_rates = &pParam->operationalRateSet;
	tSirMacRateSet *ext_rates = &pParam->extendedRateSet;

	if (pProfile->ChannelInfo.numOfChannels &&
	    pProfile->ChannelInfo.freq_list)
		tmp_opr_ch_freq = pProfile->ChannelInfo.freq_list[0];

	pParam->uCfgDot11Mode =
		csr_roam_get_phy_mode_band_for_bss(mac, pProfile,
						   tmp_opr_ch_freq,
						   &band);

	if (((pProfile->csrPersona == QDF_P2P_CLIENT_MODE)
	    || (pProfile->csrPersona == QDF_P2P_GO_MODE))
	    && (pParam->uCfgDot11Mode == eCSR_CFG_DOT11_MODE_11B)) {
		/* This should never happen */
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_FATAL,
			 "For P2P (persona %d) dot11_mode is 11B",
			  pProfile->csrPersona);
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	nw_type = csr_convert_mode_to_nw_type(pParam->uCfgDot11Mode, band);
	ext_rates->numRates = 0;
	/*
	 * hostapd.conf will populate its basic and extended rates
	 * as per hw_mode, but if acs in ini is enabled driver should
	 * ignore basic and extended rates from hostapd.conf and should
	 * populate default rates.
	 */
	if (!cds_is_sub_20_mhz_enabled() && !skip_hostapd_rate &&
			(pProfile->supported_rates.numRates ||
			pProfile->extended_rates.numRates)) {
		csr_populate_supported_rates_from_hostapd(opr_rates,
				ext_rates, pProfile);
		pParam->operation_chan_freq = tmp_opr_ch_freq;
	} else {
		switch (nw_type) {
		default:
			sme_err(
				"sees an unknown pSirNwType (%d)",
				nw_type);
			return QDF_STATUS_E_INVAL;
		case eSIR_11A_NW_TYPE:
			csr_populate_basic_rates(opr_rates, true, true);
			if (eCSR_OPERATING_CHANNEL_ANY != tmp_opr_ch_freq) {
				opr_ch_freq = tmp_opr_ch_freq;
				break;
			}
			if (0 == opr_ch_freq &&
				CSR_IS_PHY_MODE_DUAL_BAND(pProfile->phyMode) &&
				CSR_IS_PHY_MODE_DUAL_BAND(
					mac->roam.configParam.phyMode)) {
				/*
				 * We could not find a 5G channel by auto pick,
				 * let's try 2.4G channels. We only do this here
				 * because csr_roam_get_phy_mode_band_for_bss
				 * always picks 11a  for AUTO
				 */
				nw_type = eSIR_11B_NW_TYPE;
				csr_populate_basic_rates(opr_rates, false, true);
			}
			break;
		case eSIR_11B_NW_TYPE:
			csr_populate_basic_rates(opr_rates, false, true);
			if (eCSR_OPERATING_CHANNEL_ANY != tmp_opr_ch_freq)
				opr_ch_freq = tmp_opr_ch_freq;
			break;
		case eSIR_11G_NW_TYPE:
			/* For P2P Client and P2P GO, disable 11b rates */
			if ((pProfile->csrPersona == QDF_P2P_CLIENT_MODE) ||
				(pProfile->csrPersona == QDF_P2P_GO_MODE) ||
				(eCSR_CFG_DOT11_MODE_11G_ONLY ==
					pParam->uCfgDot11Mode)) {
				csr_populate_basic_rates(opr_rates, true, true);
			} else {
				csr_populate_basic_rates(opr_rates, false,
								true);
				csr_populate_basic_rates(ext_rates, true,
								false);
			}
			if (eCSR_OPERATING_CHANNEL_ANY != tmp_opr_ch_freq)
				opr_ch_freq = tmp_opr_ch_freq;
			break;
		}
		pParam->operation_chan_freq = opr_ch_freq;
	}

	if (pProfile->require_h2e) {
		h2e = WLAN_BASIC_RATE_MASK |
			WLAN_BSS_MEMBERSHIP_SELECTOR_SAE_H2E;
		if (ext_rates->numRates < SIR_MAC_MAX_NUMBER_OF_RATES) {
			ext_rates->rate[ext_rates->numRates] = h2e;
			ext_rates->numRates++;
			sme_debug("H2E bss membership add to ext support rate");
		} else if (opr_rates->numRates < SIR_MAC_MAX_NUMBER_OF_RATES) {
			opr_rates->rate[opr_rates->numRates] = h2e;
			opr_rates->numRates++;
			sme_debug("H2E bss membership add to support rate");
		} else {
			sme_err("rates full, can not add H2E bss membership");
		}
	}

	pParam->sirNwType = nw_type;
	pParam->ch_params.ch_width = pProfile->ch_params.ch_width;
	pParam->ch_params.center_freq_seg0 =
		pProfile->ch_params.center_freq_seg0;
	pParam->ch_params.center_freq_seg1 =
		pProfile->ch_params.center_freq_seg1;
	pParam->ch_params.sec_ch_offset =
		pProfile->ch_params.sec_ch_offset;
	return QDF_STATUS_SUCCESS;
}

static void
csr_roam_get_bss_start_parms_from_bss_desc(
					struct mac_context *mac,
					struct bss_description *bss_desc,
					tDot11fBeaconIEs *pIes,
					struct csr_roamstart_bssparams *pParam)
{
	if (!pParam) {
		sme_err("BSS param's pointer is NULL");
		return;
	}

	pParam->sirNwType = bss_desc->nwType;
	pParam->cbMode = PHY_SINGLE_CHANNEL_CENTERED;
	pParam->operation_chan_freq = bss_desc->chan_freq;
	qdf_mem_copy(&pParam->bssid, bss_desc->bssId,
						sizeof(struct qdf_mac_addr));

	if (!pIes) {
		pParam->ssId.length = 0;
		pParam->operationalRateSet.numRates = 0;
		sme_err("IEs struct pointer is NULL");
		return;
	}

	if (pIes->SuppRates.present) {
		pParam->operationalRateSet.numRates = pIes->SuppRates.num_rates;
		if (pIes->SuppRates.num_rates > WLAN_SUPPORTED_RATES_IE_MAX_LEN) {
			sme_err(
				"num_rates: %d > max val, resetting",
				pIes->SuppRates.num_rates);
			pIes->SuppRates.num_rates =
				WLAN_SUPPORTED_RATES_IE_MAX_LEN;
		}
		qdf_mem_copy(pParam->operationalRateSet.rate,
			     pIes->SuppRates.rates,
			     sizeof(*pIes->SuppRates.rates) *
			     pIes->SuppRates.num_rates);
	}
	if (pIes->ExtSuppRates.present) {
		pParam->extendedRateSet.numRates = pIes->ExtSuppRates.num_rates;
		if (pIes->ExtSuppRates.num_rates > WLAN_SUPPORTED_RATES_IE_MAX_LEN) {
			sme_err(
				"num_rates: %d > max val, resetting",
				pIes->ExtSuppRates.num_rates);
			pIes->ExtSuppRates.num_rates =
				WLAN_SUPPORTED_RATES_IE_MAX_LEN;
		}
		qdf_mem_copy(pParam->extendedRateSet.rate,
			     pIes->ExtSuppRates.rates,
			     sizeof(*pIes->ExtSuppRates.rates) *
			     pIes->ExtSuppRates.num_rates);
	}
	if (pIes->SSID.present) {
		pParam->ssId.length = pIes->SSID.num_ssid;
		qdf_mem_copy(pParam->ssId.ssId, pIes->SSID.ssid,
			     pParam->ssId.length);
	}
	pParam->cbMode =
		csr_get_cb_mode_from_ies(mac, pParam->operation_chan_freq,
					 pIes);
}

static void csr_roam_determine_max_rate_for_ad_hoc(struct mac_context *mac,
						   tSirMacRateSet *pSirRateSet)
{
	uint8_t MaxRate = 0;
	uint32_t i;
	uint8_t *pRate;

	pRate = pSirRateSet->rate;
	for (i = 0; i < pSirRateSet->numRates; i++) {
		MaxRate = QDF_MAX(MaxRate, (pRate[i] &
					(~CSR_DOT11_BASIC_RATE_MASK)));
	}

	/* Save the max rate in the connected state information.
	 * modify LastRates variable as well
	 */

}

QDF_STATUS csr_roam_issue_start_bss(struct mac_context *mac, uint32_t sessionId,
				    struct csr_roamstart_bssparams *pParam,
				    struct csr_roam_profile *pProfile,
				    struct bss_description *bss_desc,
					uint32_t roamId)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	enum reg_wifi_band band;
	/* Set the roaming substate to 'Start BSS attempt'... */
	csr_roam_substate_change(mac, eCSR_ROAM_SUBSTATE_START_BSS_REQ,
				 sessionId);
	/* Put RSN information in for Starting BSS */
	pParam->nRSNIELength = (uint16_t) pProfile->nRSNReqIELength;
	pParam->pRSNIE = pProfile->pRSNReqIE;

	pParam->privacy = pProfile->privacy;
	pParam->fwdWPSPBCProbeReq = pProfile->fwdWPSPBCProbeReq;
	pParam->authType = pProfile->csr80211AuthType;
	pParam->beaconInterval = pProfile->beaconInterval;
	pParam->dtimPeriod = pProfile->dtimPeriod;
	pParam->ApUapsdEnable = pProfile->ApUapsdEnable;
	pParam->ssidHidden = pProfile->SSIDs.SSIDList[0].ssidHidden;
	if (CSR_IS_INFRA_AP(pProfile) && (pParam->operation_chan_freq != 0)) {
		if (!wlan_reg_is_freq_present_in_cur_chan_list(mac->pdev,
						pParam->operation_chan_freq)) {
			pParam->operation_chan_freq = INFRA_AP_DEFAULT_CHAN_FREQ;
			pParam->ch_params.ch_width = CH_WIDTH_20MHZ;
		}
	}
	pParam->protEnabled = pProfile->protEnabled;
	pParam->obssProtEnabled = pProfile->obssProtEnabled;
	pParam->ht_protection = pProfile->cfg_protection;
	pParam->wps_state = pProfile->wps_state;

	pParam->uCfgDot11Mode =
		csr_roam_get_phy_mode_band_for_bss(mac, pProfile,
						   pParam->operation_chan_freq,
						   &band);
	pParam->bssPersona = pProfile->csrPersona;

#ifdef WLAN_FEATURE_11W
	pParam->mfpCapable = (0 != pProfile->MFPCapable);
	pParam->mfpRequired = (0 != pProfile->MFPRequired);
#endif

	pParam->add_ie_params.probeRespDataLen =
		pProfile->add_ie_params.probeRespDataLen;
	pParam->add_ie_params.probeRespData_buff =
		pProfile->add_ie_params.probeRespData_buff;

	pParam->add_ie_params.assocRespDataLen =
		pProfile->add_ie_params.assocRespDataLen;
	pParam->add_ie_params.assocRespData_buff =
		pProfile->add_ie_params.assocRespData_buff;

	pParam->add_ie_params.probeRespBCNDataLen =
		pProfile->add_ie_params.probeRespBCNDataLen;
	pParam->add_ie_params.probeRespBCNData_buff =
		pProfile->add_ie_params.probeRespBCNData_buff;

	if (pProfile->csrPersona == QDF_SAP_MODE)
		pParam->sap_dot11mc = mac->mlme_cfg->gen.sap_dot11mc;
	else
		pParam->sap_dot11mc = 1;

	sme_debug("11MC Support Enabled : %d", pParam->sap_dot11mc);

	pParam->cac_duration_ms = pProfile->cac_duration_ms;
	pParam->dfs_regdomain = pProfile->dfs_regdomain;
	pParam->beacon_tx_rate = pProfile->beacon_tx_rate;

	status = csr_send_mb_start_bss_req_msg(mac, sessionId,
						pProfile->BSSType, pParam,
					      bss_desc);
	return status;
}

void csr_roam_prepare_bss_params(struct mac_context *mac, uint32_t sessionId,
					struct csr_roam_profile *pProfile,
					struct bss_description *bss_desc,
					struct bss_config_param *pBssConfig,
					tDot11fBeaconIEs *pIes)
{
	ePhyChanBondState cbMode = PHY_SINGLE_CHANNEL_CENTERED;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	bool skip_hostapd_rate = !pProfile->chan_switch_hostapd_rate_enabled;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}

	if (bss_desc) {
		csr_roam_get_bss_start_parms_from_bss_desc(mac, bss_desc, pIes,
							  &pSession->bssParams);
		if (CSR_IS_NDI(pProfile)) {
			qdf_copy_macaddr(&pSession->bssParams.bssid,
				&pSession->self_mac_addr);
		}
	} else {
		csr_roam_get_bss_start_parms(mac, pProfile,
					     &pSession->bssParams,
					     skip_hostapd_rate);
		/* Use the first SSID */
		if (pProfile->SSIDs.numOfSSIDs)
			qdf_mem_copy(&pSession->bssParams.ssId,
				     pProfile->SSIDs.SSIDList,
				     sizeof(tSirMacSSid));
		if (pProfile->BSSIDs.numOfBSSIDs)
			/* Use the first BSSID */
			qdf_mem_copy(&pSession->bssParams.bssid,
				     pProfile->BSSIDs.bssid,
				     sizeof(struct qdf_mac_addr));
		else
			qdf_mem_zero(&pSession->bssParams.bssid,
				    sizeof(struct qdf_mac_addr));
	}
	/* Set operating frequency in pProfile which will be used */
	/* in csr_roam_set_bss_config_cfg() to determine channel bonding */
	/* mode and will be configured in CFG later */
	pProfile->op_freq = pSession->bssParams.operation_chan_freq;

	if (pProfile->op_freq == 0)
		sme_err("CSR cannot find a channel to start");
	else {
		csr_roam_determine_max_rate_for_ad_hoc(mac,
						       &pSession->bssParams.
						       operationalRateSet);
		if (CSR_IS_INFRA_AP(pProfile)) {
			if (WLAN_REG_IS_24GHZ_CH_FREQ(pProfile->op_freq)) {
				cbMode =
					mac->roam.configParam.
					channelBondingMode24GHz;
			} else {
				cbMode =
					mac->roam.configParam.
					channelBondingMode5GHz;
			}
			sme_debug("## cbMode %d", cbMode);
			pBssConfig->cbMode = cbMode;
			pSession->bssParams.cbMode = cbMode;
		}
	}
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
void csr_get_pmk_info(struct mac_context *mac_ctx, uint8_t session_id,
			  tPmkidCacheInfo *pmk_cache)
{
	struct csr_roam_session *session = NULL;

	if (!mac_ctx) {
		sme_err("Mac_ctx is NULL");
		return;
	}
	session = CSR_GET_SESSION(mac_ctx, session_id);
	if (!session) {
		sme_err("session %d not found", session_id);
		return;
	}
	qdf_mem_copy(pmk_cache->pmk, session->psk_pmk,
					sizeof(session->psk_pmk));
	pmk_cache->pmk_len = session->pmk_len;
}

QDF_STATUS csr_roam_set_psk_pmk(struct mac_context *mac,
				struct wlan_crypto_pmksa *pmksa,
				uint8_t vdev_id, bool update_to_fw)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, vdev_id);
	struct qdf_mac_addr connected_bssid = {0};

	if (!pSession) {
		sme_err("session %d not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_copy_macaddr(&connected_bssid, &pSession->connectedProfile.bssid);
	if (csr_is_conn_state_connected_infra(mac, vdev_id) &&
	    !pmksa->ssid_len &&
	    !qdf_is_macaddr_equal(&connected_bssid, &pmksa->bssid)) {
		sme_debug("Set pmksa received for non-connected bss");
		return QDF_STATUS_E_INVAL;
	}

	pSession->pmk_len = pmksa->pmk_len;
	qdf_mem_copy(pSession->psk_pmk, pmksa->pmk, pSession->pmk_len);

	if (csr_is_auth_type_ese(mac->roam.roamSession[vdev_id].
				 connectedProfile.AuthType)) {
		sme_debug("PMK update is not required for ESE");
		return QDF_STATUS_SUCCESS;
	}

	if (update_to_fw)
		csr_roam_update_cfg(mac, vdev_id,
				    REASON_ROAM_PSK_PMK_CHANGED);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_set_pmk_cache_ft(struct mac_context *mac, uint32_t session_id,
				tPmkidCacheInfo *pmk_cache)
{
	struct csr_roam_session *session = CSR_GET_SESSION(mac, session_id);
	enum csr_akm_type akm_type;

	if (!CSR_IS_SESSION_VALID(mac, session_id)) {
		sme_err("session %d not found", session_id);
		return QDF_STATUS_E_INVAL;
	}

	akm_type = session->connectedProfile.AuthType;
	if ((akm_type == eCSR_AUTH_TYPE_FT_RSN ||
	     akm_type == eCSR_AUTH_TYPE_FT_FILS_SHA256 ||
	     akm_type == eCSR_AUTH_TYPE_FT_FILS_SHA384 ||
	     akm_type == eCSR_AUTH_TYPE_FT_SUITEB_EAP_SHA384) &&
	    session->connectedProfile.mdid.mdie_present) {
		sme_debug("Auth type: %d update the MDID in cache", akm_type);
		csr_update_pmk_cache_ft(mac, session_id, pmk_cache, NULL);
	} else {
		tCsrScanResultInfo *scan_res;
		QDF_STATUS status = QDF_STATUS_E_FAILURE;

		scan_res = qdf_mem_malloc(sizeof(tCsrScanResultInfo));
		if (!scan_res)
			return QDF_STATUS_E_NOMEM;

		status = csr_scan_get_result_for_bssid(mac, &pmk_cache->BSSID,
						       scan_res);
		if (QDF_IS_STATUS_SUCCESS(status) &&
		    scan_res->BssDescriptor.mdiePresent) {
			sme_debug("Update MDID in cache from scan_res");
			csr_update_pmk_cache_ft(mac, session_id,
						pmk_cache, scan_res);
		}
		qdf_mem_free(scan_res);
		scan_res = NULL;
	}
	return QDF_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void csr_mem_zero_psk_pmk(struct csr_roam_session *session)
{
	qdf_mem_zero(session->psk_pmk, sizeof(session->psk_pmk));
	session->pmk_len = 0;
}
#else
static void csr_mem_zero_psk_pmk(struct csr_roam_session *session)
{
}
#endif

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
void csr_clear_sae_single_pmk(struct wlan_objmgr_psoc *psoc,
			      uint8_t vdev_id, tPmkidCacheInfo *pmk_cache)
{
	struct wlan_objmgr_vdev *vdev;
	int32_t keymgmt;
	struct mlme_pmk_info pmk_info;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return;
	}

	keymgmt = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (keymgmt < 0) {
		sme_err("Invalid mgmt cipher");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	if (!(keymgmt & (1 << WLAN_CRYPTO_KEY_MGMT_SAE))) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	if (pmk_cache) {
		qdf_mem_copy(&pmk_info.pmk, pmk_cache->pmk, pmk_cache->pmk_len);
		pmk_info.pmk_len = pmk_cache->pmk_len;
		wlan_mlme_clear_sae_single_pmk_info(vdev, &pmk_info);
	} else {
		wlan_mlme_clear_sae_single_pmk_info(vdev, NULL);
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

void
csr_store_sae_single_pmk_to_global_cache(struct mac_context *mac,
					 struct csr_roam_session *session,
					 uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct wlan_crypto_pmksa *pmksa;
	struct mlme_pmk_info *pmk_info;
	uint32_t akm;

	if (!session->pConnectBssDesc)
		return;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return;
	}

	akm = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (!session->pConnectBssDesc->is_single_pmk ||
	    !QDF_HAS_PARAM(akm, WLAN_CRYPTO_KEY_MGMT_SAE)) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	/*
	 * Mark the AP as single PMK capable in Crypto Table
	 */
	wlan_crypto_set_sae_single_pmk_bss_cap(vdev,
			(struct qdf_mac_addr *)session->pConnectBssDesc->bssId,
			true);

	pmk_info = qdf_mem_malloc(sizeof(*pmk_info));
	if (!pmk_info) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	qdf_mem_copy(pmk_info->pmk, session->psk_pmk, session->pmk_len);
	pmk_info->pmk_len = session->pmk_len;

	pmksa = wlan_crypto_get_pmksa(vdev,
				      (struct qdf_mac_addr *)session->pConnectBssDesc->bssId);
	if (pmksa) {
		pmk_info->spmk_timeout_period =
			(pmksa->pmk_lifetime *
			 pmksa->pmk_lifetime_threshold) / 100;
		pmk_info->spmk_timestamp = pmksa->pmk_entry_ts;
		sme_debug("spmk_ts:%ld spmk_timeout_prd:%d secs",
			  pmk_info->spmk_timestamp,
			  pmk_info->spmk_timeout_period);
	} else {
		sme_debug("PMK entry not found for bss:" QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(session->pConnectBssDesc->bssId));
	}

	wlan_mlme_update_sae_single_pmk(vdev, pmk_info);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
	qdf_mem_zero(pmk_info, sizeof(*pmk_info));
	qdf_mem_free(pmk_info);
}
#endif

void csr_update_pmk_cache_ft(struct mac_context *mac, uint32_t vdev_id,
			     tPmkidCacheInfo *pmk_cache,
			     tCsrScanResultInfo *scan_res)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	struct wlan_objmgr_vdev *vdev;
	struct wlan_crypto_pmksa pmksa;
	enum QDF_OPMODE vdev_mode;
	struct csr_roam_session *session = CSR_GET_SESSION(mac, vdev_id);

	if (!session) {
		sme_err("session not found");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return;
	}

	vdev_mode = wlan_vdev_mlme_get_opmode(vdev);
	/* If vdev mode is STA then proceed further */
	if (vdev_mode != QDF_STA_MODE) {
		sme_err("vdev mode is not STA");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	/*
	 * In FT connection fetch the MDID from Session or scan result whichever
	 * is available and send it to crypto so that it will update the crypto
	 * PMKSA table with the MDID for the matching BSSID or SSID PMKSA entry.
	 * And delete the old/stale PMK cache entries for the same mobility
	 * domain as of the newly added entry to avoid multiple PMK cache
	 * entries for the same MDID.
	 */
	if (pmk_cache && pmk_cache->ssid_len) {
		pmksa.ssid_len = pmk_cache->ssid_len;
		qdf_mem_copy(&pmksa.ssid, &pmk_cache->ssid, pmksa.ssid_len);
		qdf_mem_copy(&pmksa.cache_id,
			     &pmk_cache->cache_id, CACHE_ID_LEN);
		sme_debug("copied the SSID from pmk_cache to PMKSA");
	}

	if (session->connectedProfile.mdid.mdie_present) {
		pmksa.mdid.mdie_present = 1;
		pmksa.mdid.mobility_domain =
				session->connectedProfile.mdid.mobility_domain;
		sme_debug("Session MDID:0x%x copied to PMKSA",
			  pmksa.mdid.mobility_domain);
		qdf_copy_macaddr(&pmksa.bssid,
				 &session->connectedProfile.bssid);

		status = wlan_crypto_update_pmk_cache_ft(vdev, &pmksa);
		if (QDF_IS_STATUS_ERROR(status))
			sme_debug("Failed to update the crypto table");
	} else if (scan_res && scan_res->BssDescriptor.mdiePresent) {
		pmksa.mdid.mdie_present = 1;
		pmksa.mdid.mobility_domain =
			(scan_res->BssDescriptor.mdie[0] |
			 (scan_res->BssDescriptor.mdie[1] << 8));
		sme_debug("Scan_res MDID:0x%x copied to PMKSA",
			  pmksa.mdid.mobility_domain);
		if (pmk_cache)
			qdf_copy_macaddr(&pmksa.bssid, &pmk_cache->BSSID);

		status = wlan_crypto_update_pmk_cache_ft(vdev, &pmksa);
		if (QDF_IS_STATUS_ERROR(status))
			sme_debug("Failed to update the crypto table");
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}

QDF_STATUS csr_roam_del_pmkid_from_cache(struct mac_context *mac,
					 uint32_t sessionId,
					 tPmkidCacheInfo *pmksa,
					 bool flush_cache)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	bool fMatchFound = false;
	uint32_t Index;
	uint32_t curr_idx;
	tPmkidCacheInfo *cached_pmksa;
	uint32_t i;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	/* Check if there are no entries to delete */
	if (0 == pSession->NumPmkidCache) {
		sme_debug("No entries to delete/Flush");
		return QDF_STATUS_SUCCESS;
	}

	if (flush_cache) {
		/* Flush the entire cache */
		qdf_mem_zero(pSession->PmkidCacheInfo,
			     sizeof(tPmkidCacheInfo) * CSR_MAX_PMKID_ALLOWED);
		/* flush single_pmk_info information */
		csr_clear_sae_single_pmk(mac->psoc, pSession->vdev_id, NULL);
		pSession->NumPmkidCache = 0;
		pSession->curr_cache_idx = 0;
		csr_mem_zero_psk_pmk(pSession);
		return QDF_STATUS_SUCCESS;
	}

	/* clear single_pmk_info information */
	csr_clear_sae_single_pmk(mac->psoc, pSession->vdev_id, pmksa);

	/* !flush_cache - so look up in the cache */
	for (Index = 0; Index < CSR_MAX_PMKID_ALLOWED; Index++) {
		cached_pmksa = &pSession->PmkidCacheInfo[Index];
		if ((!cached_pmksa->ssid_len) &&
			qdf_is_macaddr_equal(&cached_pmksa->BSSID,
				&pmksa->BSSID))
			fMatchFound = 1;

		else if (cached_pmksa->ssid_len &&
			(!qdf_mem_cmp(cached_pmksa->ssid,
			pmksa->ssid, pmksa->ssid_len)) &&
			(!qdf_mem_cmp(cached_pmksa->cache_id,
				pmksa->cache_id, CACHE_ID_LEN)))
			fMatchFound = 1;

		if (fMatchFound) {
			/* Clear this - matched entry */
			qdf_mem_zero(cached_pmksa,
				     sizeof(tPmkidCacheInfo));
			break;
		}
	}

	if (Index == CSR_MAX_PMKID_ALLOWED && !fMatchFound) {
		sme_debug("No such PMKSA entry exists");
		return QDF_STATUS_SUCCESS;
	}

	/* Match Found, Readjust the other entries */
	curr_idx = pSession->curr_cache_idx;
	if (Index < curr_idx) {
		for (i = Index; i < (curr_idx - 1); i++) {
			qdf_mem_copy(&pSession->PmkidCacheInfo[i],
				     &pSession->PmkidCacheInfo[i + 1],
				     sizeof(tPmkidCacheInfo));
		}

		pSession->curr_cache_idx--;
		qdf_mem_zero(&pSession->PmkidCacheInfo
			     [pSession->curr_cache_idx],
			     sizeof(tPmkidCacheInfo));
	} else if (Index > curr_idx) {
		for (i = Index; i > (curr_idx); i--) {
			qdf_mem_copy(&pSession->PmkidCacheInfo[i],
				     &pSession->PmkidCacheInfo[i - 1],
				     sizeof(tPmkidCacheInfo));
		}

		qdf_mem_zero(&pSession->PmkidCacheInfo
			     [pSession->curr_cache_idx],
			     sizeof(tPmkidCacheInfo));
	}

	/* Decrement the count since an entry has been deleted */
	pSession->NumPmkidCache--;
	sme_debug("PMKID at index=%d deleted, current index=%d cache count=%d",
		Index, pSession->curr_cache_idx, pSession->NumPmkidCache);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_roam_get_wpa_rsn_req_ie(struct mac_context *mac, uint32_t sessionId,
				       uint32_t *pLen, uint8_t *pBuf)
{
	QDF_STATUS status = QDF_STATUS_E_INVAL;
	uint32_t len;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (pLen) {
		len = *pLen;
		*pLen = pSession->nWpaRsnReqIeLength;
		if (pBuf) {
			if (len >= pSession->nWpaRsnReqIeLength) {
				qdf_mem_copy(pBuf, pSession->pWpaRsnReqIE,
					     pSession->nWpaRsnReqIeLength);
				status = QDF_STATUS_SUCCESS;
			}
		}
	}
	return status;
}

eRoamCmdStatus csr_get_roam_complete_status(struct mac_context *mac,
						uint32_t sessionId)
{
	eRoamCmdStatus retStatus = eCSR_ROAM_CONNECT_COMPLETION;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return retStatus;
	}

	if (CSR_IS_ROAMING(pSession)) {
		retStatus = eCSR_ROAM_ROAMING_COMPLETION;
		pSession->fRoaming = false;
	}
	return retStatus;
}

static QDF_STATUS csr_roam_start_wds(struct mac_context *mac, uint32_t sessionId,
				     struct csr_roam_profile *pProfile,
				     struct bss_description *bss_desc)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	struct bss_config_param bssConfig;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	if (csr_is_conn_state_connected_infra(mac, sessionId)) {
		/* Disassociate from the connected Infrastructure network.*/
		status = csr_roam_issue_disassociate(mac, sessionId,
				eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING,
						    false);
	} else {
		if (csr_is_conn_state_wds(mac, sessionId)) {
			QDF_ASSERT(0);
			return QDF_STATUS_E_FAILURE;
		}
		qdf_mem_zero(&bssConfig, sizeof(struct bss_config_param));
		/* Assume HDD provide bssid in profile */
		qdf_copy_macaddr(&pSession->bssParams.bssid,
				 pProfile->BSSIDs.bssid);
		/* there is no Bss description before we start an WDS so we
		 * need to adopt all Bss configuration parameters from the
		 * Profile.
		 */
		status = csr_roam_prepare_bss_config_from_profile(mac,
								pProfile,
								&bssConfig,
								bss_desc);
		if (QDF_IS_STATUS_SUCCESS(status)) {
			/* Save profile for late use */
			csr_free_roam_profile(mac, sessionId);
			pSession->pCurRoamProfile =
				qdf_mem_malloc(sizeof(struct csr_roam_profile));
			if (pSession->pCurRoamProfile) {
				csr_roam_copy_profile(mac,
						      pSession->pCurRoamProfile,
						      pProfile, sessionId);
			}
			/* Prepare some more parameters for this WDS */
			csr_roam_prepare_bss_params(mac, sessionId, pProfile,
						NULL, &bssConfig, NULL);
			status = csr_roam_set_bss_config_cfg(mac, sessionId,
							pProfile, NULL,
							&bssConfig, NULL,
							false);
		}
	}

	return status;
}

/**
 * csr_add_supported_5Ghz_channels()- Add valid 5Ghz channels
 * in Join req.
 * @mac_ctx: pointer to global mac structure
 * @chan_list: Pointer to channel list buffer to populate
 * @num_chan: Pointer to number of channels value to update
 * @supp_chan_ie: Boolean to check if we need to populate as IE
 *
 * This function is called to update valid 5Ghz channels
 * in Join req. If @supp_chan_ie is true, supported channels IE
 * format[chan num 1, num of channels 1, chan num 2, num of
 * channels 2, ..] is populated. Else, @chan_list would be a list
 * of supported channels[chan num 1, chan num 2..]
 *
 * Return: void
 */
static void csr_add_supported_5Ghz_channels(struct mac_context *mac_ctx,
						uint8_t *chan_list,
						uint8_t *num_chnl,
						bool supp_chan_ie)
{
	uint16_t i, j;
	uint32_t size = 0;

	if (!chan_list) {
		sme_err("chan_list buffer NULL");
		return;
	}

	size = sizeof(mac_ctx->roam.valid_ch_freq_list);
	if (QDF_IS_STATUS_SUCCESS
		(csr_get_cfg_valid_channels(mac_ctx,
					    mac_ctx->roam.valid_ch_freq_list,
					    &size))) {
		for (i = 0, j = 0; i < size; i++) {
			/* Only add 5ghz channels.*/
			if (WLAN_REG_IS_5GHZ_CH_FREQ
					(mac_ctx->roam.valid_ch_freq_list[i])) {
				chan_list[j] =
					wlan_reg_freq_to_chan(mac_ctx->pdev,
					  mac_ctx->roam.valid_ch_freq_list[i]);
				j++;

				if (supp_chan_ie) {
					chan_list[j] = 1;
					j++;
				}
			}
		}
		*num_chnl = (uint8_t)j;
	} else {
		sme_err("can not find any valid channel");
		*num_chnl = 0;
	}
}

/**
 * csr_set_ldpc_exception() - to set allow any LDPC exception permitted
 * @mac_ctx: Pointer to mac context
 * @session: Pointer to SME/CSR session
 * @channel: Given channel number where connection will go
 * @usr_cfg_rx_ldpc: User provided RX LDPC setting
 *
 * This API will check if hardware allows LDPC to be enabled for provided
 * channel and user has enabled the RX LDPC selection
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS csr_set_ldpc_exception(struct mac_context *mac_ctx,
			struct csr_roam_session *session, uint32_t ch_freq,
			bool usr_cfg_rx_ldpc)
{
	if (!mac_ctx) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"mac_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (!session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			"session is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	if (usr_cfg_rx_ldpc && wma_is_rx_ldpc_supported_for_channel(ch_freq)) {
		session->ht_config.ht_rx_ldpc = 1;
		session->vht_config.ldpc_coding = 1;
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"LDPC enable for ch freq[%d]", ch_freq);
	} else {
		session->ht_config.ht_rx_ldpc = 0;
		session->vht_config.ldpc_coding = 0;
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			"LDPC disable for ch freq[%d]", ch_freq);
	}
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_FILS_SK
/*
 * csr_validate_and_update_fils_info: Copy fils connection info to join request
 * @profile: pointer to profile
 * @csr_join_req: csr join request
 *
 * Return: None
 */
static QDF_STATUS
csr_validate_and_update_fils_info(struct mac_context *mac,
				  struct csr_roam_profile *profile,
				  struct bss_description *bss_desc,
				  struct join_req *csr_join_req,
				  uint8_t vdev_id)
{
	uint8_t cache_id[CACHE_ID_LEN] = {0};
	struct qdf_mac_addr bssid;

	if (!profile->fils_con_info) {
		wlan_cm_update_mlme_fils_connection_info(mac->psoc, NULL,
							 vdev_id);
		return QDF_STATUS_SUCCESS;
	}

	if (!profile->fils_con_info->is_fils_connection) {
		sme_debug("FILS_PMKSA: Not a FILS connection");
		return QDF_STATUS_SUCCESS;
	}

	if (bss_desc->fils_info_element.is_cache_id_present) {
		qdf_mem_copy(cache_id, bss_desc->fils_info_element.cache_id,
			     CACHE_ID_LEN);
		sme_debug("FILS_PMKSA: cache_id[0]:%d, cache_id[1]:%d",
			  cache_id[0], cache_id[1]);
	}

	qdf_mem_copy(bssid.bytes,
		     csr_join_req->bssDescription.bssId,
		     QDF_MAC_ADDR_SIZE);

	if ((!profile->fils_con_info->r_rk_length ||
	     !profile->fils_con_info->key_nai_length) &&
	    !bss_desc->fils_info_element.is_cache_id_present &&
	    !csr_lookup_fils_pmkid(mac, vdev_id, cache_id,
				   csr_join_req->ssId.ssId,
				   csr_join_req->ssId.length, &bssid))
		return QDF_STATUS_E_FAILURE;

	return wlan_cm_update_mlme_fils_connection_info(mac->psoc,
							profile->fils_con_info,
							vdev_id);
}
#else
static inline QDF_STATUS
csr_validate_and_update_fils_info(struct mac_context *mac,
				  struct csr_roam_profile *profile,
				  struct bss_description *bss_desc,
				  struct join_req *csr_join_req,
				  uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#ifdef WLAN_FEATURE_SAE
/*
 * csr_update_sae_config() - Copy SAE info to join request
 * @csr_join_req: csr join request
 * @mac: mac context
 * @session: sme session
 *
 * Return: None
 */
static void csr_update_sae_config(struct join_req *csr_join_req,
	struct mac_context *mac, struct csr_roam_session *session)
{
	tPmkidCacheInfo *pmkid_cache;

	pmkid_cache = qdf_mem_malloc(sizeof(*pmkid_cache));
	if (!pmkid_cache)
		return;

	qdf_mem_copy(pmkid_cache->BSSID.bytes,
		     csr_join_req->bssDescription.bssId,
		     QDF_MAC_ADDR_SIZE);

	csr_join_req->sae_pmk_cached =
	       csr_lookup_pmkid_using_bssid(mac, session, pmkid_cache);

	qdf_mem_free(pmkid_cache);

	if (!csr_join_req->sae_pmk_cached)
		return;

	sme_debug("Found for BSSID=" QDF_MAC_ADDR_FMT,
		  QDF_MAC_ADDR_REF(csr_join_req->bssDescription.bssId));
}
#else
static inline void csr_update_sae_config(struct join_req *csr_join_req,
	struct mac_context *mac, struct csr_roam_session *session)
{ }
#endif

/**
 * csr_get_nss_supported_by_sta_and_ap() - finds out nss from session
 * and beacon from AP
 * @vht_caps: VHT capabilities
 * @ht_caps: HT capabilities
 * @dot11_mode: dot11 mode
 *
 * Return: number of nss advertised by beacon
 */
static uint8_t csr_get_nss_supported_by_sta_and_ap(tDot11fIEVHTCaps *vht_caps,
						   tDot11fIEHTCaps *ht_caps,
						   tDot11fIEhe_cap *he_cap,
						   uint32_t dot11_mode)
{
	bool vht_capability, ht_capability, he_capability;

	vht_capability = IS_DOT11_MODE_VHT(dot11_mode);
	ht_capability = IS_DOT11_MODE_HT(dot11_mode);
	he_capability = IS_DOT11_MODE_HE(dot11_mode);

	if (he_capability && he_cap->present) {
		if ((he_cap->rx_he_mcs_map_lt_80 & 0xC0) != 0xC0)
			return NSS_4x4_MODE;

		if ((he_cap->rx_he_mcs_map_lt_80 & 0x30) != 0x30)
			return NSS_3x3_MODE;

		if ((he_cap->rx_he_mcs_map_lt_80 & 0x0C) != 0x0C)
			return NSS_2x2_MODE;
	} else if (vht_capability && vht_caps->present) {
		if ((vht_caps->rxMCSMap & 0xC0) != 0xC0)
			return NSS_4x4_MODE;

		if ((vht_caps->rxMCSMap & 0x30) != 0x30)
			return NSS_3x3_MODE;

		if ((vht_caps->rxMCSMap & 0x0C) != 0x0C)
			return NSS_2x2_MODE;
	} else if (ht_capability && ht_caps->present) {
		if (ht_caps->supportedMCSSet[3])
			return NSS_4x4_MODE;

		if (ht_caps->supportedMCSSet[2])
			return NSS_3x3_MODE;

		if (ht_caps->supportedMCSSet[1])
			return NSS_2x2_MODE;
	}

	return NSS_1x1_MODE;
}

/**
 * csr_check_vendor_ap_3_present() - Check if Vendor AP 3 is present
 * @mac_ctx: Pointer to Global MAC structure
 * @ie: Pointer to starting IE in Beacon/Probe Response
 * @ie_len: Length of all IEs combined
 *
 * For Vendor AP 3, the condition is that Vendor AP 3 IE should be present
 * and Vendor AP 4 IE should not be present.
 * If Vendor AP 3 IE is present and Vendor AP 4 IE is also present,
 * return false, else return true.
 *
 * Return: true or false
 */
static bool
csr_check_vendor_ap_3_present(struct mac_context *mac_ctx, uint8_t *ie,
			      uint16_t ie_len)
{
	bool ret = true;

	if ((wlan_get_vendor_ie_ptr_from_oui(SIR_MAC_VENDOR_AP_3_OUI,
	    SIR_MAC_VENDOR_AP_3_OUI_LEN, ie, ie_len)) &&
	    (wlan_get_vendor_ie_ptr_from_oui(SIR_MAC_VENDOR_AP_4_OUI,
	    SIR_MAC_VENDOR_AP_4_OUI_LEN, ie, ie_len))) {
		sme_debug("Vendor OUI 3 and Vendor OUI 4 found");
		ret = false;
	}

	return ret;
}

#if defined(WLAN_FEATURE_11AX) && defined(WLAN_SUPPORT_TWT)
/**
 * csr_enable_twt() - Check if its allowed to enable twt for this session
 * @ie: pointer to beacon/probe resp ie's
 *
 * TWT is allowed only if device is in 11ax mode and peer supports
 * TWT responder or if QCN ie present.
 *
 * Return: true or flase
 */
static bool csr_enable_twt(struct mac_context *mac_ctx, tDot11fBeaconIEs *ie)
{

	if (mac_ctx->mlme_cfg->he_caps.dot11_he_cap.twt_request && ie &&
	    (ie->qcn_ie.present || ie->he_cap.twt_responder)) {
		sme_debug("TWT is supported, hence disable UAPSD; twt req supp: %d,twt respon supp: %d, QCN_IE: %d",
			  mac_ctx->mlme_cfg->he_caps.dot11_he_cap.twt_request,
			  ie->he_cap.twt_responder, ie->qcn_ie.present);
		return true;
	}
	return false;
}
#else
static bool csr_enable_twt(struct mac_context *mac_ctx, tDot11fBeaconIEs *ie)
{
	return false;
}
#endif

#ifdef WLAN_FEATURE_11AX
static void
csr_update_he_caps_mcs(struct mac_context *mac
,		       struct wlan_mlme_cfg *mlme_cfg,
		       struct csr_roam_session *csr_session)
{
	uint32_t tx_mcs_map = 0;
	uint32_t rx_mcs_map = 0;
	uint32_t mcs_map = 0;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc,
						    csr_session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;
	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	rx_mcs_map = mlme_cfg->he_caps.dot11_he_cap.rx_he_mcs_map_lt_80;
	tx_mcs_map = mlme_cfg->he_caps.dot11_he_cap.tx_he_mcs_map_lt_80;
	mcs_map = rx_mcs_map & 0x3;

	if (csr_session->nss == 1) {
		tx_mcs_map = HE_SET_MCS_4_NSS(tx_mcs_map, HE_MCS_DISABLE, 2);
		rx_mcs_map = HE_SET_MCS_4_NSS(rx_mcs_map, HE_MCS_DISABLE, 2);
	} else {
		tx_mcs_map = HE_SET_MCS_4_NSS(tx_mcs_map, mcs_map, 2);
		rx_mcs_map = HE_SET_MCS_4_NSS(rx_mcs_map, mcs_map, 2);
	}
	sme_debug("new HE Nss MCS MAP: Rx 0x%0X, Tx: 0x%0X",
		  rx_mcs_map, tx_mcs_map);
	mlme_priv->he_config.tx_he_mcs_map_lt_80 = tx_mcs_map;
	mlme_priv->he_config.rx_he_mcs_map_lt_80 = rx_mcs_map;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#else
static void
csr_update_he_caps_mcs(struct mac_context *mac,
		       struct wlan_mlme_cfg *mlme_cfg,
		       struct csr_roam_session *csr_session)
{
}
#endif

#ifdef WLAN_ADAPTIVE_11R
/**
 * csr_get_adaptive_11r_enabled() - Function to check if adaptive 11r
 * ini is enabled or disabled
 * @mac: pointer to mac context
 *
 * Return: true if adaptive 11r is enabled
 */
static bool
csr_get_adaptive_11r_enabled(struct mac_context *mac)
{
	return mac->mlme_cfg->lfr.enable_adaptive_11r;
}
#else
static inline bool
csr_get_adaptive_11r_enabled(struct mac_context *mac)
{
	return false;
}
#endif

static QDF_STATUS csr_check_and_validate_6g_ap(struct mac_context *mac_ctx,
					       struct bss_description *bss,
					       struct join_req *csr_join_req,
					       tDot11fBeaconIEs *ie)
{
	tDot11fIEhe_op *he_op = &ie->he_op;

	if (!wlan_reg_is_6ghz_chan_freq(bss->chan_freq))
		return QDF_STATUS_SUCCESS;

	if (!he_op->oper_info_6g_present) {
		sme_err(QDF_MAC_ADDR_FMT" Invalid 6GHZ AP BSS description IE",
			QDF_MAC_ADDR_REF(bss->bssId));
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11AX
static void csr_handle_iot_ap_no_common_he_rates(struct mac_context *mac,
					struct csr_roam_session *session,
					tDot11fBeaconIEs *ies,
					uint32_t *dot11mode)
{
	uint16_t int_mcs;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_legacy_priv *mlme_priv;

	/* if the connection is not 11AX mode then return */
	if (*dot11mode != MLME_DOT11_MODE_11AX)
		return;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev)
		return;
	mlme_priv = wlan_vdev_mlme_get_ext_hdl(vdev);
	if (!mlme_priv) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}
	int_mcs = HE_INTERSECT_MCS(mlme_priv->he_config.tx_he_mcs_map_lt_80,
				   ies->he_cap.rx_he_mcs_map_lt_80);
	sme_debug("HE self rates %x AP rates %x int_mcs %x vendorIE %d",
		  mlme_priv->he_config.rx_he_mcs_map_lt_80,
		  ies->he_cap.rx_he_mcs_map_lt_80, int_mcs,
		  ies->vendor_vht_ie.present);
	if (ies->he_cap.present)
		if ((int_mcs == 0xFFFF) &&
		    (ies->vendor_vht_ie.present ||
		     ies->VHTCaps.present)) {
			*dot11mode = MLME_DOT11_MODE_11AC;
			sme_debug("No common 11AX rate. Force 11AC connection");
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#else
static void csr_handle_iot_ap_no_common_he_rates(struct mac_context *mac,
					struct csr_roam_session *session,
					tDot11fBeaconIEs *ies,
					uint32_t *dot11mode)
{
}
#endif

#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_update_sae_single_pmk_ap_cap() - Function to update sae single pmk ap ie
 * @mac: pointer to mac context
 * @bss_desc: BSS Descriptor
 * @vdev_id: Vdev id
 * @akm: Akm type
 *
 * Return: true if sae single pmk feature is enabled
 */
static void
csr_update_sae_single_pmk_ap_cap(struct mac_context *mac,
				 struct bss_description *bss_desc,
				 uint8_t vdev_id, enum csr_akm_type akm)
{
	if (akm == eCSR_AUTH_TYPE_SAE &&
	    mac->mlme_cfg->lfr.sae_single_pmk_feature_enabled)
		wlan_mlme_set_sae_single_pmk_bss_cap(mac->psoc, vdev_id,
						     bss_desc->is_single_pmk);
}
#else
static inline void
csr_update_sae_single_pmk_ap_cap(struct mac_context *mac,
				 struct bss_description *bss_desc,
				 uint8_t vdev_id, enum csr_akm_type akm)
{
}
#endif

static void csr_get_basic_rates(tSirMacRateSet *b_rates, uint32_t chan_freq)
{
	/*
	 * Some IOT APs don't send supported rates in
	 * probe resp, hence add BSS basic rates in
	 * supported rates IE of assoc request.
	 */
	if (WLAN_REG_IS_24GHZ_CH_FREQ(chan_freq))
		csr_populate_basic_rates(b_rates, false, true);
	else if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq))
		csr_populate_basic_rates(b_rates, true, true);
}

/*
 * csr_iterate_triplets() - Iterate the country IE to validate it
 * @country_ie: country IE to iterate through
 *
 * This function always returns success because connection should not be failed
 * in the case of missing elements in the country IE
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS csr_iterate_triplets(tDot11fIECountry country_ie)
{
	u_int8_t i;

	if (country_ie.first_triplet[0] > OP_CLASS_ID_200) {
		if (country_ie.more_triplets[0][0] <= OP_CLASS_ID_200)
			return QDF_STATUS_SUCCESS;
	}

	for (i = 0; i < country_ie.num_more_triplets; i++) {
		if ((country_ie.more_triplets[i][0] > OP_CLASS_ID_200) &&
		    (i < country_ie.num_more_triplets - 1)) {
			if (country_ie.more_triplets[i + 1][0] <=
			    OP_CLASS_ID_200)
				return QDF_STATUS_SUCCESS;
		}
	}
	sme_debug("No operating class triplet followed by sub-band triplet");
	return QDF_STATUS_SUCCESS;
}

/**
 * The communication between HDD and LIM is thru mailbox (MB).
 * Both sides will access the data structure "struct join_req".
 * The rule is, while the components of "struct join_req" can be accessed in the
 * regular way like struct join_req.assocType, this guideline stops at component
 * tSirRSNie;
 * any acces to the components after tSirRSNie is forbidden because the space
 * from tSirRSNie is squeezed with the component "struct bss_description" and
 * since the size of actual 'struct bss_description' varies, the receiving side
 * should keep in mind not to access the components DIRECTLY after tSirRSNie.
 */
QDF_STATUS csr_send_join_req_msg(struct mac_context *mac, uint32_t sessionId,
				 struct bss_description *pBssDescription,
				 struct csr_roam_profile *pProfile,
				 tDot11fBeaconIEs *pIes, uint16_t messageType)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	uint8_t acm_mask = 0, uapsd_mask;
	uint32_t bss_freq;
	uint16_t msgLen, ieLen;
	tSirMacRateSet OpRateSet;
	tSirMacRateSet ExRateSet;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);
	uint32_t dw_tmp, dot11mode = 0;
	uint8_t *wpaRsnIE = NULL;
	uint8_t txBFCsnValue = 0;
	struct join_req *csr_join_req;
	tSirMacCapabilityInfo *pAP_capabilityInfo;
	bool fTmp;
	int8_t pwr_limit = 0;
	struct ps_global_info *ps_global_info = &mac->sme.ps_global_info;
	struct ps_params *ps_param = &ps_global_info->ps_params[sessionId];
	uint8_t ese_config = 0;
	tpCsrNeighborRoamControlInfo neigh_roam_info;
	uint32_t value = 0, value1 = 0;
	bool is_vendor_ap_present;
	struct vdev_type_nss *vdev_type_nss;
	struct action_oui_search_attr vendor_ap_search_attr;
	tDot11fIEVHTCaps *vht_caps = NULL;
	bool bvalue = 0;
	enum csr_akm_type akm;
	bool force_max_nss;
	uint8_t ap_nss;
	struct wlan_objmgr_vdev *vdev;
	bool follow_ap_edca;
	bool reconn_after_assoc_timeout = false;
	uint8_t programmed_country[REG_ALPHA2_LEN + 1];
	enum reg_6g_ap_type power_type_6g;
	bool ctry_code_match;

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* To satisfy klockworks */
	if (!pBssDescription) {
		sme_err(" pBssDescription is NULL");
		return QDF_STATUS_E_FAILURE;
	}
	neigh_roam_info = &mac->roam.neighborRoamInfo[sessionId];
	bss_freq = pBssDescription->chan_freq;
	if ((eWNI_SME_REASSOC_REQ == messageType) ||
	    WLAN_REG_IS_5GHZ_CH_FREQ(bss_freq)) {
		pSession->disable_hi_rssi = true;
		sme_debug("Disabling HI_RSSI, AP freq=%d, rssi=%d",
			  pBssDescription->chan_freq, pBssDescription->rssi);
	} else {
		pSession->disable_hi_rssi = false;
	}

	do {
		pSession->joinFailStatusCode.status_code = eSIR_SME_SUCCESS;
		pSession->joinFailStatusCode.reasonCode = 0;
		qdf_mem_copy(&pSession->joinFailStatusCode.bssId,
		       &pBssDescription->bssId, sizeof(tSirMacAddr));
		/*
		 * the struct join_req which includes a single
		 * bssDescription. it includes a single uint32_t for the
		 * IE fields, but the length field in the bssDescription
		 * needs to be interpreted to determine length of IE fields
		 * So, take the size of the struct join_req, subtract  size of
		 * bssDescription, add the number of bytes indicated by the
		 * length field of the bssDescription, add the size of length
		 * field because it not included in the length field.
		 */
		msgLen = sizeof(struct join_req) - sizeof(*pBssDescription) +
				pBssDescription->length +
				sizeof(pBssDescription->length) +
				/*
				 * add in the size of the WPA IE that
				 * we may build.
				 */
				sizeof(tCsrWpaIe) + sizeof(tCsrWpaAuthIe) +
				sizeof(uint16_t);
		csr_join_req = qdf_mem_malloc(msgLen);
		if (!csr_join_req)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		wpaRsnIE = qdf_mem_malloc(DOT11F_IE_RSN_MAX_LEN);
		if (!wpaRsnIE)
			status = QDF_STATUS_E_NOMEM;

		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		status = csr_check_and_validate_6g_ap(mac, pBssDescription,
						      csr_join_req, pIes);
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;

		csr_join_req->messageType = messageType;
		csr_join_req->length = msgLen;
		csr_join_req->vdev_id = (uint8_t) sessionId;
		if (pIes->SSID.present &&
		    !csr_is_nullssid(pIes->SSID.ssid,
				     pIes->SSID.num_ssid)) {
			csr_join_req->ssId.length = pIes->SSID.num_ssid;
			qdf_mem_copy(&csr_join_req->ssId.ssId, pIes->SSID.ssid,
				     pIes->SSID.num_ssid);
		} else if (pProfile->SSIDs.numOfSSIDs) {
			csr_join_req->ssId.length =
					pProfile->SSIDs.SSIDList[0].SSID.length;
			qdf_mem_copy(&csr_join_req->ssId.ssId,
				     pProfile->SSIDs.SSIDList[0].SSID.ssId,
				     csr_join_req->ssId.length);
		} else {
			csr_join_req->ssId.length = 0;
		}
		qdf_mem_copy(&csr_join_req->self_mac_addr,
			     &pSession->self_mac_addr,
			     sizeof(tSirMacAddr));
		sme_nofl_info("vdev-%d: Connecting to %.*s " QDF_MAC_ADDR_FMT
			      " rssi: %d freq: %d akm %d cipher: uc %d mc %d, CC: %c%c",
			      sessionId, csr_join_req->ssId.length,
			      csr_join_req->ssId.ssId,
			      QDF_MAC_ADDR_REF(pBssDescription->bssId),
			      pBssDescription->rssi, pBssDescription->chan_freq,
			      pProfile->negotiatedAuthType,
			      pProfile->negotiatedUCEncryptionType,
			      pProfile->negotiatedMCEncryptionType,
			      mac->scan.countryCodeCurrent[0],
			      mac->scan.countryCodeCurrent[1]);
		wlan_rec_conn_info(sessionId, DEBUG_CONN_CONNECTING,
				   pBssDescription->bssId,
				   pProfile->negotiatedAuthType,
				   pBssDescription->chan_freq);
		/* bsstype */
		dw_tmp = csr_translate_bsstype_to_mac_type
						(pProfile->BSSType);
		csr_join_req->bsstype = dw_tmp;
		/* dot11mode */
		dot11mode =
			csr_translate_to_wni_cfg_dot11_mode(mac,
							    pSession->bssParams.
							    uCfgDot11Mode);
		akm = pProfile->negotiatedAuthType;
		csr_join_req->akm = csr_convert_csr_to_ani_akm_type(akm);

		csr_update_sae_single_pmk_ap_cap(mac, pBssDescription,
						 sessionId, akm);

		if (bss_freq <= 2484 &&
		    !mac->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band &&
		    dot11mode == MLME_DOT11_MODE_11AC) {
			/* Need to disable VHT operation in 2.4 GHz band */
			dot11mode = MLME_DOT11_MODE_11N;
		}
		/*
		 * FIX IOT AP:
		 * AP capable of HE but doesn't advertize MCS rates for 1x1/2x2.
		 * In such scenario, associate to AP in VHT mode
		 */
		csr_handle_iot_ap_no_common_he_rates(mac, pSession, pIes,
						     &dot11mode);

		ieLen = csr_get_ielen_from_bss_description(pBssDescription);

		/* Fill the Vendor AP search params */
		vendor_ap_search_attr.ie_data =
				(uint8_t *)&pBssDescription->ieFields[0];
		vendor_ap_search_attr.ie_length = ieLen;
		vendor_ap_search_attr.mac_addr = &pBssDescription->bssId[0];
		vendor_ap_search_attr.nss = csr_get_nss_supported_by_sta_and_ap(
						&pIes->VHTCaps, &pIes->HTCaps,
						&pIes->he_cap, dot11mode);
		vendor_ap_search_attr.ht_cap = pIes->HTCaps.present;
		vendor_ap_search_attr.vht_cap = pIes->VHTCaps.present;
		vendor_ap_search_attr.enable_2g =
					wlan_reg_is_24ghz_ch_freq(bss_freq);
		vendor_ap_search_attr.enable_5g =
					wlan_reg_is_5ghz_ch_freq(bss_freq);

		if (wlan_reg_is_5ghz_ch_freq(bss_freq))
			vdev_type_nss = &mac->vdev_type_nss_5g;
		else
			vdev_type_nss = &mac->vdev_type_nss_2g;
		if (pSession->pCurRoamProfile->csrPersona ==
		    QDF_P2P_CLIENT_MODE)
			pSession->vdev_nss = vdev_type_nss->p2p_cli;
		else
			pSession->vdev_nss = vdev_type_nss->sta;
		pSession->nss = pSession->vdev_nss;

		force_max_nss = ucfg_action_oui_search(mac->psoc,
						&vendor_ap_search_attr,
						ACTION_OUI_FORCE_MAX_NSS);

		if (!mac->mlme_cfg->vht_caps.vht_cap_info.enable2x2) {
			force_max_nss = false;
			pSession->nss = 1;
		}

		if (!force_max_nss)
			ap_nss = csr_get_nss_supported_by_sta_and_ap(
						&pIes->VHTCaps,
						&pIes->HTCaps,
						&pIes->he_cap,
						dot11mode);
		if (!force_max_nss && pSession->nss > ap_nss) {
			pSession->nss = ap_nss;
			pSession->vdev_nss = pSession->nss;
		}

		if (pSession->nss == 1)
			pSession->supported_nss_1x1 = true;

		follow_ap_edca = ucfg_action_oui_search(mac->psoc,
					    &vendor_ap_search_attr,
					    ACTION_OUI_DISABLE_AGGRESSIVE_EDCA);

		if (messageType == eWNI_SME_JOIN_REQ &&
		    ucfg_action_oui_search(mac->psoc, &vendor_ap_search_attr,
					   ACTION_OUI_HOST_RECONN))
			reconn_after_assoc_timeout = true;
		mlme_set_reconn_after_assoc_timeout_flag(
				mac->psoc, sessionId,
				reconn_after_assoc_timeout);

		vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac->psoc,
							sessionId,
							WLAN_LEGACY_MAC_ID);
		if (vdev) {
			mlme_set_follow_ap_edca_flag(vdev, follow_ap_edca);
			is_vendor_ap_present = wlan_get_vendor_ie_ptr_from_oui(
					SIR_MAC_BA_2K_JUMP_AP_VENDOR_OUI,
					SIR_MAC_BA_2K_JUMP_AP_VENDOR_OUI_LEN,
					vendor_ap_search_attr.ie_data,
					vendor_ap_search_attr.ie_length);
			wlan_mlme_set_ba_2k_jump_iot_ap(vdev,
							is_vendor_ap_present);

			is_vendor_ap_present = wlan_get_vendor_ie_ptr_from_oui
					(SIR_MAC_BAD_HTC_HE_VENDOR_OUI1,
					 SIR_MAC_BAD_HTC_HE_VENDOR_OUI_LEN,
					 vendor_ap_search_attr.ie_data,
					 vendor_ap_search_attr.ie_length);

			is_vendor_ap_present =
					is_vendor_ap_present &&
					wlan_get_vendor_ie_ptr_from_oui
					(SIR_MAC_BAD_HTC_HE_VENDOR_OUI2,
					 SIR_MAC_BAD_HTC_HE_VENDOR_OUI_LEN,
					 vendor_ap_search_attr.ie_data,
					 vendor_ap_search_attr.ie_length);
			/*
			 * For SAP with special OUI, if DUT STA connect with
			 * htc he enabled, SAP can't decode data pkt from DUT.
			 */
			wlan_mlme_set_bad_htc_he_iot_ap(vdev,
							is_vendor_ap_present);

			wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
		}

		is_vendor_ap_present =
				ucfg_action_oui_search(mac->psoc,
						       &vendor_ap_search_attr,
						       ACTION_OUI_CONNECT_1X1);

		if (is_vendor_ap_present) {
			is_vendor_ap_present = csr_check_vendor_ap_3_present(
						mac,
						vendor_ap_search_attr.ie_data,
						ieLen);
		}

		/*
		 * For WMI_ACTION_OUI_CONNECT_1x1_WITH_1_CHAIN, the host
		 * sends the NSS as 1 to the FW and the FW then decides
		 * after receiving the first beacon after connection to
		 * switch to 1 Tx/Rx Chain.
		 */

		if (!is_vendor_ap_present) {
			is_vendor_ap_present =
				ucfg_action_oui_search(mac->psoc,
					&vendor_ap_search_attr,
					ACTION_OUI_CONNECT_1X1_WITH_1_CHAIN);
			if (is_vendor_ap_present)
				sme_debug("1x1 with 1 Chain AP");
		}

		if (is_vendor_ap_present &&
		    !policy_mgr_is_hw_dbs_2x2_capable(mac->psoc) &&
		    ((mac->roam.configParam.is_force_1x1 ==
		    FORCE_1X1_ENABLED_FOR_AS &&
		    mac->mlme_cfg->gen.as_enabled) ||
		    mac->roam.configParam.is_force_1x1 ==
		    FORCE_1X1_ENABLED_FORCED)) {
			pSession->supported_nss_1x1 = true;
			pSession->vdev_nss = 1;
			pSession->nss = 1;
			pSession->nss_forced_1x1 = true;
			sme_debug("For special ap, NSS: %d force 1x1 %d",
				  pSession->nss,
				  mac->roam.configParam.is_force_1x1);
		}

		csr_update_he_caps_mcs(mac, mac->mlme_cfg, pSession);
		/*
		 * If CCK WAR is set for current AP, update to firmware via
		 * WMI_VDEV_PARAM_ABG_MODE_TX_CHAIN_NUM
		 */
		is_vendor_ap_present =
				ucfg_action_oui_search(mac->psoc,
						       &vendor_ap_search_attr,
						       ACTION_OUI_CCKM_1X1);
		if (is_vendor_ap_present) {
			sme_debug("vdev: %d WMI_VDEV_PARAM_ABG_MODE_TX_CHAIN_NUM 1",
				 pSession->sessionId);
			wma_cli_set_command(
				pSession->sessionId,
				(int)WMI_VDEV_PARAM_ABG_MODE_TX_CHAIN_NUM, 1,
				VDEV_CMD);
		}

		/*
		 * If Switch to 11N WAR is set for current AP, change dot11
		 * mode to 11N.
		 */
		is_vendor_ap_present =
			ucfg_action_oui_search(mac->psoc,
					       &vendor_ap_search_attr,
					       ACTION_OUI_SWITCH_TO_11N_MODE);
		if (mac->roam.configParam.is_force_1x1 &&
		    mac->mlme_cfg->gen.as_enabled &&
		    is_vendor_ap_present &&
		    (dot11mode == MLME_DOT11_MODE_ALL ||
		     dot11mode == MLME_DOT11_MODE_11AC ||
		     dot11mode == MLME_DOT11_MODE_11AC_ONLY))
			dot11mode = MLME_DOT11_MODE_11N;

		csr_join_req->supported_nss_1x1 = pSession->supported_nss_1x1;
		csr_join_req->vdev_nss = pSession->vdev_nss;
		csr_join_req->nss = pSession->nss;
		csr_join_req->nss_forced_1x1 = pSession->nss_forced_1x1;
		csr_join_req->dot11mode = (uint8_t)dot11mode;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
		csr_join_req->cc_switch_mode =
			mac->roam.configParam.cc_switch_mode;
#endif
		csr_join_req->staPersona = (uint8_t) pProfile->csrPersona;
		csr_join_req->wps_registration = pProfile->bWPSAssociation;
		csr_join_req->cbMode = (uint8_t) pSession->bssParams.cbMode;
		csr_join_req->force_24ghz_in_ht20 =
			pProfile->force_24ghz_in_ht20;
		pSession->uapsd_mask = pProfile->uapsd_mask;
		status =
			csr_get_rate_set(mac, pProfile,
					 (eCsrPhyMode) pProfile->phyMode,
					 pBssDescription, pIes, &OpRateSet,
					 &ExRateSet);
		if (!csr_enable_twt(mac, pIes))
			ps_param->uapsd_per_ac_bit_mask = pProfile->uapsd_mask;
		if (QDF_IS_STATUS_SUCCESS(status)) {
			/* OperationalRateSet */
			if (OpRateSet.numRates) {
				csr_join_req->operationalRateSet.numRates =
					OpRateSet.numRates;
				qdf_mem_copy(&csr_join_req->operationalRateSet.
						rate, OpRateSet.rate,
						OpRateSet.numRates);
			} else if (pProfile->phyMode == eCSR_DOT11_MODE_AUTO) {
				tSirMacRateSet b_rates = {0};

				csr_get_basic_rates(&b_rates,
						    pBssDescription->chan_freq);
				csr_join_req->operationalRateSet = b_rates;
			}
			/* ExtendedRateSet */
			if (ExRateSet.numRates) {
				csr_join_req->extendedRateSet.numRates =
					ExRateSet.numRates;
				qdf_mem_copy(&csr_join_req->extendedRateSet.
						rate, ExRateSet.rate,
						ExRateSet.numRates);
			} else {
				csr_join_req->extendedRateSet.numRates = 0;
			}
		} else if (pProfile->phyMode == eCSR_DOT11_MODE_AUTO) {
			tSirMacRateSet b_rates = {0};

			csr_get_basic_rates(&b_rates,
					    pBssDescription->chan_freq);
			csr_join_req->operationalRateSet = b_rates;
		} else {
			csr_join_req->operationalRateSet.numRates = 0;
			csr_join_req->extendedRateSet.numRates = 0;
		}

		if (pBssDescription->adaptive_11r_ap)
			pSession->is_adaptive_11r_connection =
				csr_get_adaptive_11r_enabled(mac);
		else
			pSession->is_adaptive_11r_connection = false;

		csr_join_req->is_adaptive_11r_connection =
				pSession->is_adaptive_11r_connection;

		/* rsnIE */
		if (csr_is_profile_wpa(pProfile)) {
			/* Insert the Wpa IE into the join request */
			ieLen = csr_retrieve_wpa_ie(mac, sessionId, pProfile,
						    pBssDescription, pIes,
						    (tCsrWpaIe *) (wpaRsnIE));
		} else if (csr_is_profile_rsn(pProfile)) {
			/* Insert the RSN IE into the join request */
			ieLen =
				csr_retrieve_rsn_ie(mac, sessionId, pProfile,
						    pBssDescription, pIes,
						    (tCsrRSNIe *) (wpaRsnIE));
			csr_join_req->force_rsne_override =
						pProfile->force_rsne_override;
		}
#ifdef FEATURE_WLAN_WAPI
		else if (csr_is_profile_wapi(pProfile)) {
			/* Insert the WAPI IE into the join request */
			ieLen =
				csr_retrieve_wapi_ie(mac, sessionId, pProfile,
						     pBssDescription, pIes,
						     (tCsrWapiIe *) (wpaRsnIE));
		}
#endif /* FEATURE_WLAN_WAPI */
		else
			ieLen = 0;
		/* remember the IE for future use */
		if (ieLen) {
			if (ieLen > DOT11F_IE_RSN_MAX_LEN) {
				sme_err("WPA RSN IE length :%d is more than DOT11F_IE_RSN_MAX_LEN, resetting to %d",
					ieLen, DOT11F_IE_RSN_MAX_LEN);
				ieLen = DOT11F_IE_RSN_MAX_LEN;
			}
#ifdef FEATURE_WLAN_WAPI
			if (csr_is_profile_wapi(pProfile)) {
				/* Check whether we need to allocate more mem */
				if (ieLen > pSession->nWapiReqIeLength) {
					if (pSession->pWapiReqIE
					    && pSession->nWapiReqIeLength) {
						qdf_mem_free(pSession->
							     pWapiReqIE);
					}
					pSession->pWapiReqIE =
						qdf_mem_malloc(ieLen);
					if (!pSession->pWapiReqIE)
						status = QDF_STATUS_E_NOMEM;
					else
						status = QDF_STATUS_SUCCESS;
					if (!QDF_IS_STATUS_SUCCESS(status))
						break;
				}
				pSession->nWapiReqIeLength = ieLen;
				qdf_mem_copy(pSession->pWapiReqIE, wpaRsnIE,
					     ieLen);
				csr_join_req->rsnIE.length = ieLen;
				qdf_mem_copy(&csr_join_req->rsnIE.rsnIEdata,
						 wpaRsnIE, ieLen);
			} else  /* should be WPA/WPA2 otherwise */
#endif /* FEATURE_WLAN_WAPI */
			{
				/* Check whether we need to allocate more mem */
				if (ieLen > pSession->nWpaRsnReqIeLength) {
					if (pSession->pWpaRsnReqIE
					    && pSession->nWpaRsnReqIeLength) {
						qdf_mem_free(pSession->
							     pWpaRsnReqIE);
					}
					pSession->pWpaRsnReqIE =
						qdf_mem_malloc(ieLen);
					if (!pSession->pWpaRsnReqIE)
						status = QDF_STATUS_E_NOMEM;
					else
						status = QDF_STATUS_SUCCESS;
					if (!QDF_IS_STATUS_SUCCESS(status))
						break;
				}
				pSession->nWpaRsnReqIeLength = ieLen;
				qdf_mem_copy(pSession->pWpaRsnReqIE, wpaRsnIE,
					     ieLen);
				csr_join_req->rsnIE.length = ieLen;
				qdf_mem_copy(&csr_join_req->rsnIE.rsnIEdata,
						 wpaRsnIE, ieLen);
			}
		} else {
			/* free whatever old info */
			pSession->nWpaRsnReqIeLength = 0;
			if (pSession->pWpaRsnReqIE) {
				qdf_mem_free(pSession->pWpaRsnReqIE);
				pSession->pWpaRsnReqIE = NULL;
			}
#ifdef FEATURE_WLAN_WAPI
			pSession->nWapiReqIeLength = 0;
			if (pSession->pWapiReqIE) {
				qdf_mem_free(pSession->pWapiReqIE);
				pSession->pWapiReqIE = NULL;
			}
#endif /* FEATURE_WLAN_WAPI */
			csr_join_req->rsnIE.length = 0;
		}
#ifdef FEATURE_WLAN_ESE
		if (eWNI_SME_JOIN_REQ == messageType)
			csr_join_req->cckmIE.length = 0;
		else if (eWNI_SME_REASSOC_REQ == messageType) {
			/* cckmIE */
			if (csr_is_profile_ese(pProfile)) {
				/* Insert the CCKM IE into the join request */
				ieLen = pSession->suppCckmIeInfo.cckmIeLen;
				qdf_mem_copy((void *)(wpaRsnIE),
						pSession->suppCckmIeInfo.cckmIe,
						ieLen);
			} else
				ieLen = 0;
			/*
			 * If present, copy the IE into the
			 * eWNI_SME_REASSOC_REQ message buffer
			 */
			if (ieLen) {
				/*
				 * Copy the CCKM IE over from the temp
				 * buffer (wpaRsnIE)
				 */
				csr_join_req->cckmIE.length = ieLen;
				qdf_mem_copy(&csr_join_req->cckmIE.cckmIEdata,
						wpaRsnIE, ieLen);
			} else
				csr_join_req->cckmIE.length = 0;
		}
#endif /* FEATURE_WLAN_ESE */
		/* addIEScan */
		if (pProfile->nAddIEScanLength && pProfile->pAddIEScan) {
			ieLen = pProfile->nAddIEScanLength;
			if (ieLen > pSession->nAddIEScanLength) {
				if (pSession->pAddIEScan
					&& pSession->nAddIEScanLength) {
					qdf_mem_free(pSession->pAddIEScan);
				}
				pSession->pAddIEScan = qdf_mem_malloc(ieLen);
				if (!pSession->pAddIEScan)
					status = QDF_STATUS_E_NOMEM;
				else
					status = QDF_STATUS_SUCCESS;
				if (!QDF_IS_STATUS_SUCCESS(status))
					break;
			}
			pSession->nAddIEScanLength = ieLen;
			qdf_mem_copy(pSession->pAddIEScan, pProfile->pAddIEScan,
					ieLen);
			csr_join_req->addIEScan.length = ieLen;
			qdf_mem_copy(&csr_join_req->addIEScan.addIEdata,
					pProfile->pAddIEScan, ieLen);
		} else {
			pSession->nAddIEScanLength = 0;
			if (pSession->pAddIEScan) {
				qdf_mem_free(pSession->pAddIEScan);
				pSession->pAddIEScan = NULL;
			}
			csr_join_req->addIEScan.length = 0;
		}
		/* addIEAssoc */
		if (pProfile->nAddIEAssocLength && pProfile->pAddIEAssoc) {
			ieLen = pProfile->nAddIEAssocLength;
			if (ieLen > pSession->nAddIEAssocLength) {
				if (pSession->pAddIEAssoc
				    && pSession->nAddIEAssocLength) {
					qdf_mem_free(pSession->pAddIEAssoc);
				}
				pSession->pAddIEAssoc = qdf_mem_malloc(ieLen);
				if (!pSession->pAddIEAssoc)
					status = QDF_STATUS_E_NOMEM;
				else
					status = QDF_STATUS_SUCCESS;
				if (!QDF_IS_STATUS_SUCCESS(status))
					break;
			}
			pSession->nAddIEAssocLength = ieLen;
			qdf_mem_copy(pSession->pAddIEAssoc,
				     pProfile->pAddIEAssoc, ieLen);
			csr_join_req->addIEAssoc.length = ieLen;
			qdf_mem_copy(&csr_join_req->addIEAssoc.addIEdata,
					 pProfile->pAddIEAssoc, ieLen);
		} else {
			pSession->nAddIEAssocLength = 0;
			if (pSession->pAddIEAssoc) {
				qdf_mem_free(pSession->pAddIEAssoc);
				pSession->pAddIEAssoc = NULL;
			}
			csr_join_req->addIEAssoc.length = 0;
		}

		if (eWNI_SME_REASSOC_REQ == messageType) {
			/* Unmask any AC in reassoc that is ACM-set */
			uapsd_mask = (uint8_t) pProfile->uapsd_mask;
			if (uapsd_mask && (pBssDescription)) {
				if (CSR_IS_QOS_BSS(pIes)
						&& CSR_IS_UAPSD_BSS(pIes))
#ifndef WLAN_MDM_CODE_REDUCTION_OPT
					acm_mask =
						sme_qos_get_acm_mask(mac,
								pBssDescription,
								pIes);
#endif /* WLAN_MDM_CODE_REDUCTION_OPT */
				else
					uapsd_mask = 0;
			}
		}

		if (!CSR_IS_11n_ALLOWED(pProfile->negotiatedUCEncryptionType))
			csr_join_req->he_with_wep_tkip =
				mac->roam.configParam.wep_tkip_in_he;

		csr_join_req->UCEncryptionType =
				csr_translate_encrypt_type_to_ed_type
					(pProfile->negotiatedUCEncryptionType);

#ifdef FEATURE_WLAN_ESE
		ese_config =  mac->mlme_cfg->lfr.ese_enabled;
#endif
		pProfile->mdid.mdie_present = pBssDescription->mdiePresent;
		if (csr_is_profile11r(mac, pProfile)
#ifdef FEATURE_WLAN_ESE
		    &&
		    !((pProfile->negotiatedAuthType ==
		       eCSR_AUTH_TYPE_OPEN_SYSTEM) && (pIes->ESEVersion.present)
		      && (ese_config))
#endif
			)
			csr_join_req->is11Rconnection = true;
		else
			csr_join_req->is11Rconnection = false;
#ifdef FEATURE_WLAN_ESE
		if (true == ese_config)
			csr_join_req->isESEFeatureIniEnabled = true;
		else
			csr_join_req->isESEFeatureIniEnabled = false;

		/* A profile can not be both ESE and 11R. But an 802.11R AP
		 * may be advertising support for ESE as well. So if we are
		 * associating Open or explicitly ESE then we will get ESE.
		 * If we are associating explicitly 11R only then we will get
		 * 11R.
		 */
		if ((csr_is_profile_ese(pProfile) ||
			((pIes->ESEVersion.present) &&
			(pProfile->negotiatedAuthType ==
				eCSR_AUTH_TYPE_OPEN_SYSTEM)))
			&& (ese_config))
			csr_join_req->isESEconnection = true;
		else
			csr_join_req->isESEconnection = false;

		if (eWNI_SME_JOIN_REQ == messageType) {
			tESETspecInfo eseTspec;
			/*
			 * ESE-Tspec IEs in the ASSOC request is presently not
			 * supported. so nullify the TSPEC parameters
			 */
			qdf_mem_zero(&eseTspec, sizeof(tESETspecInfo));
			qdf_mem_copy(&csr_join_req->eseTspecInfo,
					&eseTspec, sizeof(tESETspecInfo));
		} else if (eWNI_SME_REASSOC_REQ == messageType) {
			if ((csr_is_profile_ese(pProfile) ||
				((pIes->ESEVersion.present)
				&& (pProfile->negotiatedAuthType ==
					eCSR_AUTH_TYPE_OPEN_SYSTEM))) &&
				(ese_config)) {
				tESETspecInfo eseTspec;

				qdf_mem_zero(&eseTspec, sizeof(tESETspecInfo));
				eseTspec.numTspecs =
					sme_qos_ese_retrieve_tspec_info(mac,
						sessionId,
						(tTspecInfo *) &eseTspec.
							tspec[0]);
				csr_join_req->eseTspecInfo.numTspecs =
					eseTspec.numTspecs;
				if (eseTspec.numTspecs) {
					qdf_mem_copy(&csr_join_req->eseTspecInfo
						.tspec[0],
						&eseTspec.tspec[0],
						(eseTspec.numTspecs *
							sizeof(tTspecInfo)));
				}
			} else {
				tESETspecInfo eseTspec;
				/*
				 * ESE-Tspec IEs in the ASSOC request is
				 * presently not supported. so nullify the TSPEC
				 * parameters
				 */
				qdf_mem_zero(&eseTspec, sizeof(tESETspecInfo));
				qdf_mem_copy(&csr_join_req->eseTspecInfo,
						&eseTspec,
						sizeof(tESETspecInfo));
			}
		}
#endif /* FEATURE_WLAN_ESE */
		if (ese_config
		    || csr_roam_is_fast_roam_enabled(mac, sessionId))
			csr_join_req->isFastTransitionEnabled = true;
		else
			csr_join_req->isFastTransitionEnabled = false;

		if (csr_roam_is_fast_roam_enabled(mac, sessionId))
			csr_join_req->isFastRoamIniFeatureEnabled = true;
		else
			csr_join_req->isFastRoamIniFeatureEnabled = false;

		csr_join_req->txLdpcIniFeatureEnabled =
			(uint8_t)mac->mlme_cfg->ht_caps.tx_ldpc_enable;

		/*
		 * If RX LDPC has been disabled for 2.4GHz channels and enabled
		 * for 5Ghz for STA like persona then here is how to handle
		 * those cases (by now channel has been decided).
		 */
		if (eSIR_INFRASTRUCTURE_MODE == csr_join_req->bsstype ||
		    !policy_mgr_is_dbs_enable(mac->psoc))
			csr_set_ldpc_exception(mac, pSession,
					       bss_freq,
					       mac->mlme_cfg->ht_caps.
					       ht_cap_info.adv_coding_cap);
		csr_join_req->ht_config = pSession->ht_config;
		csr_join_req->vht_config = pSession->vht_config;
		sme_debug("ht capability 0x%x VHT capability 0x%x",
			(unsigned int)(*(uint32_t *) &csr_join_req->ht_config),
			(unsigned int)(*(uint32_t *) &csr_join_req->
			vht_config));

		value = mac->mlme_cfg->vht_caps.vht_cap_info.su_bformee;
		value1 = mac->mlme_cfg->vht_caps.vht_cap_info.tx_bfee_ant_supp;

		csr_join_req->vht_config.su_beam_formee = value;

		if (pIes->VHTCaps.present)
			vht_caps = &pIes->VHTCaps;
		else if (pIes->vendor_vht_ie.VHTCaps.present)
			vht_caps = &pIes->vendor_vht_ie.VHTCaps;
		/* Set BF CSN value only if SU Bformee is enabled */
		if (vht_caps && csr_join_req->vht_config.su_beam_formee) {
			txBFCsnValue = (uint8_t)value1;
			/*
			 * Certain commercial AP display a bad behavior when
			 * CSN value in  assoc request is more than AP's CSN.
			 * Sending absolute self CSN value with such AP leads to
			 * IOT issues. However this issue is observed only with
			 * CSN cap of less than 4. To avoid such issues, take a
			 * min of self and peer CSN while sending ASSOC request.
			 */
			if (pIes->Vendor1IE.present &&
					vht_caps->csnofBeamformerAntSup < 4) {
				if (vht_caps->csnofBeamformerAntSup)
					txBFCsnValue = QDF_MIN(txBFCsnValue,
					  vht_caps->csnofBeamformerAntSup);
			}
		}
		csr_join_req->vht_config.csnof_beamformer_antSup = txBFCsnValue;

		bvalue = mac->mlme_cfg->vht_caps.vht_cap_info.su_bformer;
		/*
		 * Set SU Bformer only if SU Bformer is enabled in INI
		 * and AP is SU Bformee capable
		 */
		if (bvalue && !((IS_BSS_VHT_CAPABLE(pIes->VHTCaps) &&
		    pIes->VHTCaps.suBeamformeeCap) ||
		    (IS_BSS_VHT_CAPABLE(pIes->vendor_vht_ie.VHTCaps) &&
		     pIes->vendor_vht_ie.VHTCaps.suBeamformeeCap)))
			bvalue = 0;

		csr_join_req->vht_config.su_beam_former = bvalue;

		/* Set num soundingdim value to 0 if SU Bformer is disabled */
		if (!csr_join_req->vht_config.su_beam_former)
			csr_join_req->vht_config.num_soundingdim = 0;

		value =
			mac->mlme_cfg->vht_caps.vht_cap_info.enable_mu_bformee;
		/*
		 * Set MU Bformee only if SU Bformee is enabled and
		 * MU Bformee is enabled in INI
		 */
		if (value && csr_join_req->vht_config.su_beam_formee &&
				pIes->VHTCaps.muBeamformerCap)
			csr_join_req->vht_config.mu_beam_formee = 1;
		else
			csr_join_req->vht_config.mu_beam_formee = 0;

		csr_join_req->enableVhtpAid =
			mac->mlme_cfg->vht_caps.vht_cap_info.enable_paid;

		csr_join_req->enableVhtGid =
			mac->mlme_cfg->vht_caps.vht_cap_info.enable_gid;

		csr_join_req->enableAmpduPs =
			(uint8_t)mac->mlme_cfg->ht_caps.enable_ampdu_ps;

		csr_join_req->enableHtSmps =
			(uint8_t)mac->mlme_cfg->ht_caps.enable_smps;

		csr_join_req->htSmps = (uint8_t)mac->mlme_cfg->ht_caps.smps;
		csr_join_req->send_smps_action =
			mac->roam.configParam.send_smps_action;

		csr_join_req->max_amsdu_num =
			(uint8_t)mac->mlme_cfg->ht_caps.max_num_amsdu;

		if (mac->roam.roamSession[sessionId].fWMMConnection)
			csr_join_req->isWMEenabled = true;
		else
			csr_join_req->isWMEenabled = false;

		if (mac->roam.roamSession[sessionId].fQOSConnection)
			csr_join_req->isQosEnabled = true;
		else
			csr_join_req->isQosEnabled = false;

		if (wlan_reg_is_6ghz_chan_freq(pBssDescription->chan_freq)) {
			if (!pIes->Country.present)
				sme_debug("Channel is 6G but country IE not present");
			wlan_reg_read_current_country(mac->psoc,
						      programmed_country);
			status = wlan_reg_get_6g_power_type_for_ctry(
					pIes->Country.country,
					programmed_country, &power_type_6g,
					&ctry_code_match);
			if (QDF_IS_STATUS_ERROR(status))
				break;
			csr_join_req->ap_power_type_6g = power_type_6g;
			csr_join_req->same_ctry_code = ctry_code_match;

			status = csr_iterate_triplets(pIes->Country);
		}

		if (wlan_reg_is_6ghz_chan_freq(pBssDescription->chan_freq)) {
			if (!pIes->num_transmit_power_env ||
			    !pIes->transmit_power_env[0].present)
				sme_debug("TPE not present for 6G channel");
		}

		if (pProfile->bOSENAssociation)
			csr_join_req->isOSENConnection = true;
		else
			csr_join_req->isOSENConnection = false;

		/* Fill rrm config parameters */
		qdf_mem_copy(&csr_join_req->rrm_config,
			     &mac->rrm.rrmConfig,
			     sizeof(struct rrm_config_param));

		pAP_capabilityInfo =
			(tSirMacCapabilityInfo *)
				&pBssDescription->capabilityInfo;
		/*
		 * tell the target AP my 11H capability only if both AP and STA
		 * support
		 * 11H and the channel being used is 11a
		 */
		if (csr_is11h_supported(mac) && pAP_capabilityInfo->spectrumMgt
			&& eSIR_11A_NW_TYPE == pBssDescription->nwType) {
			fTmp = true;
		} else
			fTmp = false;

		csr_join_req->spectrumMgtIndicator = fTmp;
		csr_join_req->powerCap.minTxPower = MIN_TX_PWR_CAP;
		/*
		 * This is required for 11k test VoWiFi Ent: Test 2.
		 * We need the power capabilities for Assoc Req.
		 * This macro is provided by the halPhyCfg.h. We pick our
		 * max and min capability by the halPhy provided macros
		 * Any change in this power cap IE should also be done
		 * in csr_update_driver_assoc_ies() which would send
		 * assoc IE's to FW which is used for LFR3 roaming
		 * ie. used in reassociation requests from FW.
		 */
		pwr_limit = csr_get_cfg_max_tx_power(mac, bss_freq);
		if (0 != pwr_limit && pwr_limit < MAX_TX_PWR_CAP)
			csr_join_req->powerCap.maxTxPower = pwr_limit;
		else
			csr_join_req->powerCap.maxTxPower = MAX_TX_PWR_CAP;

		csr_add_supported_5Ghz_channels(mac,
				csr_join_req->supportedChannels.channelList,
				&csr_join_req->supportedChannels.numChnl,
				false);
		/* Enable UAPSD only if TWT is not supported */
		if (!csr_enable_twt(mac, pIes))
			csr_join_req->uapsdPerAcBitmask = pProfile->uapsd_mask;
		/* Move the entire BssDescription into the join request. */
		qdf_mem_copy(&csr_join_req->bssDescription, pBssDescription,
				pBssDescription->length +
				sizeof(pBssDescription->length));

		status = csr_validate_and_update_fils_info(mac, pProfile,
							   pBssDescription,
							   csr_join_req,
							   sessionId);
		if (QDF_IS_STATUS_ERROR(status))
			return status;

		csr_update_sae_config(csr_join_req, mac, pSession);
		/*
		 * conc_custom_rule1:
		 * If SAP comes up first and STA comes up later then SAP
		 * need to follow STA's channel in 2.4Ghz. In following if
		 * condition we are adding sanity check, just to make sure that
		 * if this rule is enabled then don't allow STA to connect on
		 * 5gz channel and also by this time SAP's channel should be the
		 * same as STA's channel.
		 */
		if (mac->roam.configParam.conc_custom_rule1) {
			if ((0 == mac->roam.configParam.
				is_sta_connection_in_5gz_enabled) &&
				WLAN_REG_IS_5GHZ_CH_FREQ(bss_freq)) {
				QDF_TRACE(QDF_MODULE_ID_SME,
					  QDF_TRACE_LEVEL_ERROR,
					 "STA-conn on 5G isn't allowed");
				status = QDF_STATUS_E_FAILURE;
				break;
			}
			if (!WLAN_REG_IS_5GHZ_CH_FREQ(bss_freq) &&
			    csr_is_conn_allow_2g_band(mac, bss_freq) == false) {
				status = QDF_STATUS_E_FAILURE;
				break;
			}
		}

		/*
		 * conc_custom_rule2:
		 * If P2PGO comes up first and STA comes up later then P2PGO
		 * need to follow STA's channel in 5Ghz. In following if
		 * condition we are just adding sanity check to make sure that
		 * by this time P2PGO's channel is same as STA's channel.
		 */
		if (mac->roam.configParam.conc_custom_rule2 &&
			!WLAN_REG_IS_24GHZ_CH_FREQ(bss_freq) &&
			(!csr_is_conn_allow_5g_band(mac, bss_freq))) {
			status = QDF_STATUS_E_FAILURE;
			break;
		}

		if (pSession->pCurRoamProfile->csrPersona == QDF_STA_MODE)
			csr_join_req->enable_bcast_probe_rsp =
				mac->mlme_cfg->oce.enable_bcast_probe_rsp;

		csr_join_req->enable_session_twt_support = csr_enable_twt(mac,
									  pIes);
		status = umac_send_mb_message_to_mac(csr_join_req);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			/*
			 * umac_send_mb_message_to_mac would've released the mem
			 * allocated by csr_join_req. Let's make it defensive by
			 * assigning NULL to the pointer.
			 */
			csr_join_req = NULL;
			break;
		}

		if (pProfile->csrPersona == QDF_STA_MODE)
			wlan_register_txrx_packetdump(OL_TXRX_PDEV_ID);

#ifndef WLAN_MDM_CODE_REDUCTION_OPT
		if (eWNI_SME_JOIN_REQ == messageType) {
			/* Notify QoS module that join happening */
			pSession->join_bssid_count++;
			sme_qos_csr_event_ind(mac, (uint8_t) sessionId,
						SME_QOS_CSR_JOIN_REQ, NULL);
		} else if (eWNI_SME_REASSOC_REQ == messageType)
			/* Notify QoS module that reassoc happening */
			sme_qos_csr_event_ind(mac, (uint8_t) sessionId,
						SME_QOS_CSR_REASSOC_REQ,
						NULL);
#endif
	} while (0);

	/* Clean up the memory in case of any failure */
	if (!QDF_IS_STATUS_SUCCESS(status) && (csr_join_req))
		qdf_mem_free(csr_join_req);

	if (wpaRsnIE)
		qdf_mem_free(wpaRsnIE);

	return status;
}

/* */
QDF_STATUS csr_send_mb_disassoc_req_msg(struct mac_context *mac,
					uint32_t sessionId,
					tSirMacAddr bssId, uint16_t reasonCode)
{
	struct disassoc_req *pMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!CSR_IS_SESSION_VALID(mac, sessionId))
		return QDF_STATUS_E_FAILURE;

	pMsg = qdf_mem_malloc(sizeof(*pMsg));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_DISASSOC_REQ;
	pMsg->length = sizeof(*pMsg);
	pMsg->sessionId = sessionId;
	if ((pSession->pCurRoamProfile)
		&& (CSR_IS_INFRA_AP(pSession->pCurRoamProfile))) {
		qdf_mem_copy(&pMsg->bssid.bytes,
			     &pSession->self_mac_addr,
			     QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(&pMsg->peer_macaddr.bytes,
			     bssId,
			     QDF_MAC_ADDR_SIZE);
	} else {
		qdf_mem_copy(&pMsg->bssid.bytes,
			     bssId, QDF_MAC_ADDR_SIZE);
		qdf_mem_copy(&pMsg->peer_macaddr.bytes,
			     bssId, QDF_MAC_ADDR_SIZE);
	}
	pMsg->reasonCode = reasonCode;
	pMsg->process_ho_fail = (pSession->disconnect_reason ==
		REASON_FW_TRIGGERED_ROAM_FAILURE) ? true : false;

	/* Update the disconnect stats */
	pSession->disconnect_stats.disconnection_cnt++;
	pSession->disconnect_stats.disconnection_by_app++;

	/*
	 * The state will be DISASSOC_HANDOFF only when we are doing
	 * handoff. Here we should not send the disassoc over the air
	 * to the AP
	 */
	if ((CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(mac, sessionId)
			&& csr_roam_is11r_assoc(mac, sessionId)) ||
						pMsg->process_ho_fail) {
		/* Set DoNotSendOverTheAir flag to 1 only for handoff case */
		pMsg->doNotSendOverTheAir = CSR_DONT_SEND_DISASSOC_OVER_THE_AIR;
	}
	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_send_chng_mcc_beacon_interval(struct mac_context *mac,
						uint32_t sessionId)
{
	struct wlan_change_bi *pMsg;
	uint16_t len = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}
	/* NO need to update the Beacon Params if update beacon parameter flag
	 * is not set
	 */
	if (!mac->roam.roamSession[sessionId].bssParams.updatebeaconInterval)
		return QDF_STATUS_SUCCESS;

	mac->roam.roamSession[sessionId].bssParams.updatebeaconInterval =
		false;

	/* Create the message and send to lim */
	len = sizeof(*pMsg);
	pMsg = qdf_mem_malloc(len);
	if (!pMsg)
		status = QDF_STATUS_E_NOMEM;
	else
		status = QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_SUCCESS(status)) {
		pMsg->message_type = eWNI_SME_CHNG_MCC_BEACON_INTERVAL;
		pMsg->length = len;

		qdf_copy_macaddr(&pMsg->bssid, &pSession->self_mac_addr);
		sme_debug("CSR Attempting to change BI for Bssid= "
			  QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(pMsg->bssid.bytes));
		pMsg->session_id = sessionId;
		sme_debug("session %d BeaconInterval %d",
			sessionId,
			mac->roam.roamSession[sessionId].bssParams.
			beaconInterval);
		pMsg->beacon_interval =
			mac->roam.roamSession[sessionId].bssParams.beaconInterval;
		status = umac_send_mb_message_to_mac(pMsg);
	}
	return status;
}

#ifdef QCA_HT_2040_COEX
QDF_STATUS csr_set_ht2040_mode(struct mac_context *mac, uint32_t sessionId,
			       ePhyChanBondState cbMode, bool obssEnabled)
{
	struct set_ht2040_mode *pMsg;
	uint16_t len = 0;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	/* Create the message and send to lim */
	len = sizeof(struct set_ht2040_mode);
	pMsg = qdf_mem_malloc(len);
	if (!pMsg)
		status = QDF_STATUS_E_NOMEM;
	else
		status = QDF_STATUS_SUCCESS;
	if (QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_zero(pMsg, sizeof(struct set_ht2040_mode));
		pMsg->messageType = eWNI_SME_SET_HT_2040_MODE;
		pMsg->length = len;

		qdf_copy_macaddr(&pMsg->bssid, &pSession->self_mac_addr);
		sme_debug(
			"CSR Attempting to set HT20/40 mode for Bssid= "
			 QDF_MAC_ADDR_FMT,
			 QDF_MAC_ADDR_REF(pMsg->bssid.bytes));
		pMsg->sessionId = sessionId;
		sme_debug("  session %d HT20/40 mode %d",
			sessionId, cbMode);
		pMsg->cbMode = cbMode;
		pMsg->obssEnabled = obssEnabled;
		status = umac_send_mb_message_to_mac(pMsg);
	}
	return status;
}
#endif

QDF_STATUS csr_send_mb_deauth_req_msg(struct mac_context *mac,
				      uint32_t vdev_id,
				      tSirMacAddr bssId, uint16_t reasonCode)
{
	struct deauth_req *pMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, vdev_id);

	if (!CSR_IS_SESSION_VALID(mac, vdev_id))
		return QDF_STATUS_E_FAILURE;

	pMsg = qdf_mem_malloc(sizeof(*pMsg));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_DEAUTH_REQ;
	pMsg->length = sizeof(*pMsg);
	pMsg->vdev_id = vdev_id;

	if ((pSession->pCurRoamProfile)
	     && (CSR_IS_INFRA_AP(pSession->pCurRoamProfile))) {
		qdf_mem_copy(&pMsg->bssid,
			     &pSession->self_mac_addr,
			     QDF_MAC_ADDR_SIZE);
	} else {
		qdf_mem_copy(&pMsg->bssid,
			     bssId, QDF_MAC_ADDR_SIZE);
	}

	/* Set the peer MAC address before sending the message to LIM */
	qdf_mem_copy(&pMsg->peer_macaddr.bytes, bssId, QDF_MAC_ADDR_SIZE);
	pMsg->reasonCode = reasonCode;

	/* Update the disconnect stats */
	pSession->disconnect_stats.disconnection_cnt++;
	pSession->disconnect_stats.disconnection_by_app++;

	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_send_mb_disassoc_cnf_msg(struct mac_context *mac,
					struct disassoc_ind *pDisassocInd)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct disassoc_cnf *pMsg;

	do {
		pMsg = qdf_mem_malloc(sizeof(*pMsg));
		if (!pMsg)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		pMsg->messageType = eWNI_SME_DISASSOC_CNF;
		pMsg->status_code = eSIR_SME_SUCCESS;
		pMsg->length = sizeof(*pMsg);
		pMsg->vdev_id = pDisassocInd->vdev_id;
		qdf_copy_macaddr(&pMsg->peer_macaddr,
				 &pDisassocInd->peer_macaddr);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}

		qdf_copy_macaddr(&pMsg->bssid, &pDisassocInd->bssid);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}

		status = umac_send_mb_message_to_mac(pMsg);
	} while (0);
	return status;
}

QDF_STATUS csr_send_mb_deauth_cnf_msg(struct mac_context *mac,
				      struct deauth_ind *pDeauthInd)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct deauth_cnf *pMsg;

	do {
		pMsg = qdf_mem_malloc(sizeof(*pMsg));
		if (!pMsg)
			status = QDF_STATUS_E_NOMEM;
		else
			status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status))
			break;
		pMsg->messageType = eWNI_SME_DEAUTH_CNF;
		pMsg->status_code = eSIR_SME_SUCCESS;
		pMsg->length = sizeof(*pMsg);
		pMsg->vdev_id = pDeauthInd->vdev_id;
		qdf_copy_macaddr(&pMsg->bssid, &pDeauthInd->bssid);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}
		qdf_copy_macaddr(&pMsg->peer_macaddr,
				 &pDeauthInd->peer_macaddr);
		status = QDF_STATUS_SUCCESS;
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			qdf_mem_free(pMsg);
			break;
		}
		status = umac_send_mb_message_to_mac(pMsg);
	} while (0);
	return status;
}

QDF_STATUS csr_send_assoc_cnf_msg(struct mac_context *mac,
				  struct assoc_ind *pAssocInd,
				  QDF_STATUS Halstatus,
				  enum wlan_status_code mac_status_code)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct assoc_cnf *pMsg;
	struct scheduler_msg msg = { 0 };

	sme_debug("HalStatus: %d, mac_status_code %d",
		  Halstatus, mac_status_code);
	do {
		pMsg = qdf_mem_malloc(sizeof(*pMsg));
		if (!pMsg)
			return QDF_STATUS_E_NOMEM;
		pMsg->messageType = eWNI_SME_ASSOC_CNF;
		pMsg->length = sizeof(*pMsg);
		if (QDF_IS_STATUS_SUCCESS(Halstatus)) {
			pMsg->status_code = eSIR_SME_SUCCESS;
		} else {
			pMsg->status_code = eSIR_SME_ASSOC_REFUSED;
			pMsg->mac_status_code = mac_status_code;
		}
		/* bssId */
		qdf_mem_copy(pMsg->bssid.bytes, pAssocInd->bssId,
			     QDF_MAC_ADDR_SIZE);
		/* peerMacAddr */
		qdf_mem_copy(pMsg->peer_macaddr.bytes, pAssocInd->peerMacAddr,
			     QDF_MAC_ADDR_SIZE);
		/* aid */
		pMsg->aid = pAssocInd->aid;
		/* OWE IE */
		if (pAssocInd->owe_ie_len) {
			pMsg->owe_ie = qdf_mem_malloc(pAssocInd->owe_ie_len);
			if (!pMsg->owe_ie)
				return QDF_STATUS_E_NOMEM;
			qdf_mem_copy(pMsg->owe_ie, pAssocInd->owe_ie,
				     pAssocInd->owe_ie_len);
			pMsg->owe_ie_len = pAssocInd->owe_ie_len;
		}
		pMsg->need_assoc_rsp_tx_cb = pAssocInd->need_assoc_rsp_tx_cb;

		msg.type = pMsg->messageType;
		msg.bodyval = 0;
		msg.bodyptr = pMsg;
		/* pMsg is freed by umac_send_mb_message_to_mac in anycase*/
		status = scheduler_post_msg_by_priority(QDF_MODULE_ID_PE, &msg,
							true);
	} while (0);
	return status;
}

QDF_STATUS csr_send_mb_start_bss_req_msg(struct mac_context *mac, uint32_t
					sessionId, eCsrRoamBssType bssType,
					 struct csr_roamstart_bssparams *pParam,
					 struct bss_description *bss_desc)
{
	struct start_bss_req *pMsg;
	uint32_t value = 0;
	struct validate_bss_data candidate_info;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pSession->joinFailStatusCode.status_code = eSIR_SME_SUCCESS;
	pSession->joinFailStatusCode.reasonCode = 0;
	pMsg = qdf_mem_malloc(sizeof(*pMsg));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_START_BSS_REQ;
	pMsg->vdev_id = sessionId;
	pMsg->length = sizeof(*pMsg);
	qdf_copy_macaddr(&pMsg->bssid, &pParam->bssid);
	/* self_mac_addr */
	qdf_copy_macaddr(&pMsg->self_macaddr, &pSession->self_mac_addr);
	/* beaconInterval */
	if (bss_desc && bss_desc->beaconInterval)
		candidate_info.beacon_interval = bss_desc->beaconInterval;
	else if (pParam->beaconInterval)
		candidate_info.beacon_interval = pParam->beaconInterval;
	else
		candidate_info.beacon_interval = MLME_CFG_BEACON_INTERVAL_DEF;

	candidate_info.chan_freq = pParam->operation_chan_freq;
	if_mgr_is_beacon_interval_valid(mac->pdev, sessionId,
					&candidate_info);

	/* Update the beacon Interval */
	pParam->beaconInterval = candidate_info.beacon_interval;
	pMsg->beaconInterval = candidate_info.beacon_interval;
	pMsg->dot11mode =
		csr_translate_to_wni_cfg_dot11_mode(mac,
						    pParam->uCfgDot11Mode);
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	pMsg->cc_switch_mode = mac->roam.configParam.cc_switch_mode;
#endif
	pMsg->bssType = csr_translate_bsstype_to_mac_type(bssType);
	qdf_mem_copy(&pMsg->ssId, &pParam->ssId, sizeof(pParam->ssId));
	pMsg->oper_ch_freq = pParam->operation_chan_freq;
	/* What should we really do for the cbmode. */
	pMsg->cbMode = (ePhyChanBondState) pParam->cbMode;
	pMsg->vht_channel_width = pParam->ch_params.ch_width;
	pMsg->center_freq_seg0 = pParam->ch_params.center_freq_seg0;
	pMsg->center_freq_seg1 = pParam->ch_params.center_freq_seg1;
	pMsg->sec_ch_offset = pParam->ch_params.sec_ch_offset;
	pMsg->privacy = pParam->privacy;
	pMsg->apUapsdEnable = pParam->ApUapsdEnable;
	pMsg->ssidHidden = pParam->ssidHidden;
	pMsg->fwdWPSPBCProbeReq = (uint8_t) pParam->fwdWPSPBCProbeReq;
	pMsg->protEnabled = (uint8_t) pParam->protEnabled;
	pMsg->obssProtEnabled = (uint8_t) pParam->obssProtEnabled;
	/* set cfg related to protection */
	pMsg->ht_capab = pParam->ht_protection;
	pMsg->authType = pParam->authType;
	pMsg->dtimPeriod = pParam->dtimPeriod;
	pMsg->wps_state = pParam->wps_state;
	pMsg->bssPersona = pParam->bssPersona;
	pMsg->txLdpcIniFeatureEnabled = mac->mlme_cfg->ht_caps.tx_ldpc_enable;

	/*
	 * If RX LDPC has been disabled for 2.4GHz channels and enabled
	 * for 5Ghz for STA like persona then here is how to handle
	 * those cases (by now channel has been decided).
	 */
	if (!policy_mgr_is_dbs_enable(mac->psoc))
		csr_set_ldpc_exception(mac, pSession,
				       pParam->operation_chan_freq,
				       mac->mlme_cfg->ht_caps.
				       ht_cap_info.adv_coding_cap);

	pMsg->vht_config = pSession->vht_config;
	pMsg->ht_config = pSession->ht_config;

	value = mac->mlme_cfg->vht_caps.vht_cap_info.su_bformee;
	pMsg->vht_config.su_beam_formee =
		(uint8_t)value &&
		(uint8_t)mac->mlme_cfg->vht_caps.vht_cap_info.tx_bfee_sap;
	value = MLME_VHT_CSN_BEAMFORMEE_ANT_SUPPORTED_FW_DEF;
	pMsg->vht_config.csnof_beamformer_antSup = (uint8_t)value;
	pMsg->vht_config.mu_beam_formee = 0;

	sme_debug("ht capability 0x%x VHT capability 0x%x",
		  (*(uint32_t *) &pMsg->ht_config),
		  (*(uint32_t *) &pMsg->vht_config));
#ifdef WLAN_FEATURE_11W
	pMsg->pmfCapable = pParam->mfpCapable;
	pMsg->pmfRequired = pParam->mfpRequired;
#endif

	if (pParam->nRSNIELength > sizeof(pMsg->rsnIE.rsnIEdata)) {
		qdf_mem_free(pMsg);
		return QDF_STATUS_E_INVAL;
	}
	pMsg->rsnIE.length = pParam->nRSNIELength;
	qdf_mem_copy(pMsg->rsnIE.rsnIEdata,
		     pParam->pRSNIE,
		     pParam->nRSNIELength);
	pMsg->nwType = (tSirNwType)pParam->sirNwType;
	qdf_mem_copy(&pMsg->operationalRateSet,
		     &pParam->operationalRateSet,
		     sizeof(tSirMacRateSet));
	qdf_mem_copy(&pMsg->extendedRateSet,
		     &pParam->extendedRateSet,
		     sizeof(tSirMacRateSet));

	pMsg->add_ie_params = pParam->add_ie_params;
	pMsg->obssEnabled = mac->roam.configParam.obssEnabled;
	pMsg->sap_dot11mc = pParam->sap_dot11mc;
	pMsg->vendor_vht_sap =
		mac->mlme_cfg->vht_caps.vht_cap_info.vendor_24ghz_band;
	pMsg->cac_duration_ms = pParam->cac_duration_ms;
	pMsg->dfs_regdomain = pParam->dfs_regdomain;
	pMsg->beacon_tx_rate = pParam->beacon_tx_rate;

	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_send_mb_stop_bss_req_msg(struct mac_context *mac,
					uint32_t sessionId)
{
	struct stop_bss_req *pMsg;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return QDF_STATUS_E_FAILURE;
	}

	pMsg = qdf_mem_malloc(sizeof(*pMsg));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;
	pMsg->messageType = eWNI_SME_STOP_BSS_REQ;
	pMsg->sessionId = sessionId;
	pMsg->length = sizeof(*pMsg);
	pMsg->reasonCode = 0;
	qdf_copy_macaddr(&pMsg->bssid, &pSession->connectedProfile.bssid);
	return umac_send_mb_message_to_mac(pMsg);
}

QDF_STATUS csr_reassoc(struct mac_context *mac, uint32_t sessionId,
		       tCsrRoamModifyProfileFields *pModProfileFields,
		       uint32_t *pRoamId, bool fForce)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t roamId = 0;
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if ((csr_is_conn_state_connected(mac, sessionId)) &&
	    (fForce || (qdf_mem_cmp(&pModProfileFields,
				     &pSession->connectedProfile.
				     modifyProfileFields,
				     sizeof(tCsrRoamModifyProfileFields))))) {
		roamId = GET_NEXT_ROAM_ID(&mac->roam);
		if (pRoamId)
			*pRoamId = roamId;

		status =
			csr_roam_issue_reassoc(mac, sessionId, NULL,
					       pModProfileFields,
					       eCsrSmeIssuedReassocToSameAP,
					       roamId, false);
	}
	return status;
}

/**
 * csr_store_oce_cfg_flags_in_vdev() - fill OCE flags from ini
 * @mac: mac_context.
 * @vdev: Pointer to pdev obj
 * @vdev_id: vdev_id
 *
 * This API will store the oce flags in vdev mlme priv object
 *
 * Return: none
 */
static void csr_store_oce_cfg_flags_in_vdev(struct mac_context *mac,
					    struct wlan_objmgr_pdev *pdev,
					    uint8_t vdev_id)
{
	uint8_t *vdev_dynamic_oce;
	struct wlan_objmgr_vdev *vdev =
	wlan_objmgr_get_vdev_by_id_from_pdev(pdev, vdev_id, WLAN_LEGACY_MAC_ID);

	if (!vdev)
		return;

	vdev_dynamic_oce = mlme_get_dynamic_oce_flags(vdev);
	if (vdev_dynamic_oce)
		*vdev_dynamic_oce = mac->mlme_cfg->oce.feature_bitmap;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_MAC_ID);
}

void csr_send_set_ie(uint8_t type, uint8_t sub_type,
		     uint8_t vdev_id)
{
	struct send_extcap_ie *msg;
	QDF_STATUS status;

	sme_debug("send SET IE msg to PE");

	if (!(type == WLAN_VDEV_MLME_TYPE_STA ||
	      (type == WLAN_VDEV_MLME_TYPE_AP &&
	      sub_type == WLAN_VDEV_MLME_SUBTYPE_P2P_DEVICE))) {
		sme_debug("Failed to send set IE req for vdev_%d", vdev_id);
		return;
	}

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg)
		return;

	msg->msg_type = eWNI_SME_SET_IE_REQ;
	msg->session_id = vdev_id;
	msg->length = sizeof(*msg);
	status = umac_send_mb_message_to_mac(msg);
	if (!QDF_IS_STATUS_SUCCESS(status))
		sme_debug("Failed to send set IE req for vdev_%d", vdev_id);
}

void csr_get_vdev_type_nss(enum QDF_OPMODE dev_mode, uint8_t *nss_2g,
			   uint8_t *nss_5g)
{
	struct mac_context *mac_ctx = sme_get_mac_context();

	if (!mac_ctx) {
		sme_err("Invalid MAC context");
		return;
	}

	switch (dev_mode) {
	case QDF_STA_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.sta;
		*nss_5g = mac_ctx->vdev_type_nss_5g.sta;
		break;
	case QDF_SAP_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.sap;
		*nss_5g = mac_ctx->vdev_type_nss_5g.sap;
		break;
	case QDF_P2P_CLIENT_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_cli;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_cli;
		break;
	case QDF_P2P_GO_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_go;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_go;
		break;
	case QDF_P2P_DEVICE_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.p2p_dev;
		*nss_5g = mac_ctx->vdev_type_nss_5g.p2p_dev;
		break;
	case QDF_IBSS_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ibss;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ibss;
		break;
	case QDF_OCB_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ocb;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ocb;
		break;
	case QDF_NAN_DISC_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.nan;
		*nss_5g = mac_ctx->vdev_type_nss_5g.nan;
		break;
	case QDF_NDI_MODE:
		*nss_2g = mac_ctx->vdev_type_nss_2g.ndi;
		*nss_5g = mac_ctx->vdev_type_nss_5g.ndi;
		break;
	default:
		*nss_2g = 1;
		*nss_5g = 1;
		sme_err("Unknown device mode");
		break;
	}
	sme_debug("mode - %d: nss_2g - %d, 5g - %d",
		  dev_mode, *nss_2g, *nss_5g);
}

QDF_STATUS csr_setup_vdev_session(struct vdev_mlme_obj *vdev_mlme)
{
	QDF_STATUS status;
	uint32_t existing_session_id;
	struct mlme_ht_capabilities_info *ht_cap_info;
	struct csr_roam_session *session;
	struct mlme_vht_capabilities_info *vht_cap_info;
	u8 vdev_id;
	struct qdf_mac_addr *mac_addr;
	mac_handle_t mac_handle;
	struct mac_context *mac_ctx;
	struct wlan_objmgr_vdev *vdev;

	mac_handle = cds_get_context(QDF_MODULE_ID_SME);
	mac_ctx = MAC_CONTEXT(mac_handle);
	if (!mac_ctx) {
		QDF_ASSERT(0);
		return QDF_STATUS_E_INVAL;
	}

	if (!(mac_ctx->mlme_cfg)) {
		sme_err("invalid mlme cfg");
		return QDF_STATUS_E_FAILURE;
	}
	vht_cap_info = &mac_ctx->mlme_cfg->vht_caps.vht_cap_info;

	vdev = vdev_mlme->vdev;

	vdev_id = wlan_vdev_get_id(vdev);
	mac_addr = (struct qdf_mac_addr *)wlan_vdev_mlme_get_macaddr(vdev);

	/* check to see if the mac address already belongs to a session */
	status = csr_roam_get_session_id_from_bssid(mac_ctx, mac_addr,
						    &existing_session_id);
	if (QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Session %d exists with mac address "QDF_MAC_ADDR_FMT,
			existing_session_id,
			QDF_MAC_ADDR_REF(mac_addr->bytes));
		return QDF_STATUS_E_FAILURE;
	}

	/* attempt to retrieve session for Id */
	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("Session does not exist for interface %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	/* check to see if the session is already active */
	if (session->sessionActive) {
		sme_err("Cannot re-open active session with Id %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	session->sessionActive = true;
	session->sessionId = vdev_id;

	/* Initialize FT related data structures only in STA mode */
	sme_ft_open(MAC_HANDLE(mac_ctx), session->sessionId);


	qdf_mem_copy(&session->self_mac_addr, mac_addr,
		     sizeof(struct qdf_mac_addr));
	status = qdf_mc_timer_init(&session->hTimerRoaming,
				   QDF_TIMER_TYPE_SW,
				   csr_roam_roaming_timer_handler,
				   &session->roamingTimerInfo);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("cannot allocate memory for Roaming timer");
		return status;
	}

	status = qdf_mc_timer_init(&session->roaming_offload_timer,
				   QDF_TIMER_TYPE_SW,
				   csr_roam_roaming_offload_timeout_handler,
				   &session->roamingTimerInfo);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("mem fail for roaming timer");
		return status;
	}
	status = csr_roam_invoke_timer_init(session);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	ht_cap_info = &mac_ctx->mlme_cfg->ht_caps.ht_cap_info;
	session->ht_config.ht_rx_ldpc = ht_cap_info->adv_coding_cap;
	session->ht_config.ht_tx_stbc = ht_cap_info->tx_stbc;
	session->ht_config.ht_rx_stbc = ht_cap_info->rx_stbc;
	session->ht_config.ht_sgi20 = ht_cap_info->short_gi_20_mhz;
	session->ht_config.ht_sgi40 = ht_cap_info->short_gi_40_mhz;

	session->vht_config.max_mpdu_len = vht_cap_info->ampdu_len;
	session->vht_config.supported_channel_widthset =
			vht_cap_info->supp_chan_width;
	session->vht_config.ldpc_coding = vht_cap_info->ldpc_coding_cap;
	session->vht_config.shortgi80 = vht_cap_info->short_gi_80mhz;
	session->vht_config.shortgi160and80plus80 =
			vht_cap_info->short_gi_160mhz;
	session->vht_config.tx_stbc = vht_cap_info->tx_stbc;
	session->vht_config.rx_stbc = vht_cap_info->rx_stbc;
	session->vht_config.su_beam_former = vht_cap_info->su_bformer;
	session->vht_config.su_beam_formee = vht_cap_info->su_bformee;
	session->vht_config.csnof_beamformer_antSup =
			vht_cap_info->tx_bfee_ant_supp;
	session->vht_config.num_soundingdim = vht_cap_info->num_soundingdim;
	session->vht_config.mu_beam_former = vht_cap_info->mu_bformer;
	session->vht_config.mu_beam_formee = vht_cap_info->enable_mu_bformee;
	session->vht_config.vht_txops = vht_cap_info->txop_ps;
	session->vht_config.htc_vhtcap = vht_cap_info->htc_vhtc;
	session->vht_config.rx_antpattern = vht_cap_info->rx_antpattern;
	session->vht_config.tx_antpattern = vht_cap_info->tx_antpattern;

	session->vht_config.max_ampdu_lenexp =
			vht_cap_info->ampdu_len_exponent;

	csr_update_session_he_cap(mac_ctx, session);

	csr_send_set_ie(vdev_mlme->mgmt.generic.type,
			vdev_mlme->mgmt.generic.subtype,
			wlan_vdev_get_id(vdev));

	if (vdev_mlme->mgmt.generic.type == WLAN_VDEV_MLME_TYPE_STA) {
		csr_store_oce_cfg_flags_in_vdev(mac_ctx, mac_ctx->pdev,
						wlan_vdev_get_id(vdev));
		wlan_mlme_update_oce_flags(mac_ctx->pdev);
	}

	return status;
}

void csr_cleanup_vdev_session(struct mac_context *mac, uint8_t vdev_id)
{
	if (CSR_IS_SESSION_VALID(mac, vdev_id)) {
		struct csr_roam_session *pSession = CSR_GET_SESSION(mac,
								vdev_id);

		csr_roam_stop(mac, vdev_id);

		/* Clean up FT related data structures */
		sme_ft_close(MAC_HANDLE(mac), vdev_id);
		csr_free_connect_bss_desc(mac, vdev_id);
		sme_reset_key(MAC_HANDLE(mac), vdev_id);
		csr_reset_cfg_privacy(mac);
		csr_flush_roam_scan_chan_lists(mac, vdev_id);
		csr_roam_free_connect_profile(&pSession->connectedProfile);
		csr_roam_free_connected_info(mac, &pSession->connectedInfo);
		csr_roam_free_connected_info(mac,
					     &pSession->prev_assoc_ap_info);
		csr_roam_invoke_timer_destroy(pSession);
		qdf_mc_timer_destroy(&pSession->hTimerRoaming);
		qdf_mc_timer_destroy(&pSession->roaming_offload_timer);
		csr_init_session(mac, vdev_id);
	}
}

QDF_STATUS csr_prepare_vdev_delete(struct mac_context *mac_ctx,
				   uint8_t vdev_id, bool cleanup)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *session;

	if (!CSR_IS_SESSION_VALID(mac_ctx, vdev_id))
		return QDF_STATUS_E_INVAL;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	/* Vdev going down stop roaming */
	session->fCancelRoaming = true;
	if (cleanup) {
		csr_cleanup_vdev_session(mac_ctx, vdev_id);
		return status;
	}

	if (CSR_IS_WAIT_FOR_KEY(mac_ctx, vdev_id)) {
		sme_debug("Stop Wait for key timer and change substate to eCSR_ROAM_SUBSTATE_NONE");
		csr_roam_stop_wait_for_key_timer(mac_ctx);
		csr_roam_substate_change(mac_ctx, eCSR_ROAM_SUBSTATE_NONE,
					 vdev_id);
	}

	/* Flush all the commands for vdev */
	wlan_serialization_purge_all_cmd_by_vdev_id(mac_ctx->pdev, vdev_id);
	if (!mac_ctx->session_close_cb) {
		sme_err("no close session callback registered");
		return QDF_STATUS_E_FAILURE;
	}

	return status;
}

static void csr_init_session(struct mac_context *mac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession)
		return;

	pSession->sessionActive = false;
	pSession->sessionId = WLAN_UMAC_VDEV_ID_MAX;
	pSession->connectState = eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	csr_saved_scan_cmd_free_fields(mac, pSession);
	csr_free_roam_profile(mac, sessionId);
	csr_roam_free_connect_profile(&pSession->connectedProfile);
	csr_roam_free_connected_info(mac, &pSession->connectedInfo);
	csr_roam_free_connected_info(mac,
				     &pSession->prev_assoc_ap_info);
	csr_free_connect_bss_desc(mac, sessionId);
	qdf_mem_zero(&pSession->self_mac_addr, sizeof(struct qdf_mac_addr));
	if (pSession->pWpaRsnReqIE) {
		qdf_mem_free(pSession->pWpaRsnReqIE);
		pSession->pWpaRsnReqIE = NULL;
	}
	pSession->nWpaRsnReqIeLength = 0;
#ifdef FEATURE_WLAN_WAPI
	if (pSession->pWapiReqIE) {
		qdf_mem_free(pSession->pWapiReqIE);
		pSession->pWapiReqIE = NULL;
	}
	pSession->nWapiReqIeLength = 0;
#endif /* FEATURE_WLAN_WAPI */
	if (pSession->pAddIEScan) {
		qdf_mem_free(pSession->pAddIEScan);
		pSession->pAddIEScan = NULL;
	}
	pSession->nAddIEScanLength = 0;
	if (pSession->pAddIEAssoc) {
		qdf_mem_free(pSession->pAddIEAssoc);
		pSession->pAddIEAssoc = NULL;
	}
	pSession->nAddIEAssocLength = 0;
}

QDF_STATUS csr_roam_get_session_id_from_bssid(struct mac_context *mac,
					      struct qdf_mac_addr *bssid,
					      uint32_t *pSessionId)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint32_t i;

	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		if (CSR_IS_SESSION_VALID(mac, i)) {
			if (qdf_is_macaddr_equal(bssid,
				    &mac->roam.roamSession[i].connectedProfile.
				    bssid)) {
				/* Found it */
				status = QDF_STATUS_SUCCESS;
				*pSessionId = i;
				break;
			}
		}
	}
	return status;
}

static void csr_roam_link_up(struct mac_context *mac, struct qdf_mac_addr bssid)
{
	uint32_t sessionId = 0;

	/*
	 * Update the current BSS info in ho control block based on connected
	 * profile info from pmac global structure
	 */

	sme_debug("WLAN link UP with AP= " QDF_MAC_ADDR_FMT,
		QDF_MAC_ADDR_REF(bssid.bytes));
	/* Indicate the neighbor roal algorithm about the connect indication */
	csr_roam_get_session_id_from_bssid(mac, &bssid,
					   &sessionId);
	csr_neighbor_roam_indicate_connect(mac, sessionId,
					   QDF_STATUS_SUCCESS);
}

static void csr_roam_link_down(struct mac_context *mac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found", sessionId);
		return;
	}
	/* Only to handle the case for Handover on infra link */
	if (eCSR_BSS_TYPE_INFRASTRUCTURE != pSession->connectedProfile.BSSType)
		return;
	/*
	 * Incase of station mode, immediately stop data transmission whenever
	 * link down is detected.
	 */
	if (csr_roam_is_sta_mode(mac, sessionId)
	    && !CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(mac, sessionId)
	    && !csr_roam_is11r_assoc(mac, sessionId)) {
		sme_debug("Inform Link lost for session %d",
			sessionId);
		csr_roam_call_callback(mac, sessionId, NULL, 0,
				       eCSR_ROAM_LOSTLINK,
				       eCSR_ROAM_RESULT_LOSTLINK);
	}
	/* Indicate the neighbor roal algorithm about the disconnect
	 * indication
	 */
	csr_neighbor_roam_indicate_disconnect(mac, sessionId);

	/* Remove this code once SLM_Sessionization is supported */
	/* BMPS_WORKAROUND_NOT_NEEDED */
	if (!IS_FEATURE_SUPPORTED_BY_FW(SLM_SESSIONIZATION) &&
	    csr_is_infra_ap_started(mac) &&
	    mac->roam.configParam.doBMPSWorkaround) {
		mac->roam.configParam.doBMPSWorkaround = 0;
	}

}

QDF_STATUS csr_get_snr(struct mac_context *mac,
		       tCsrSnrCallback callback,
		       struct qdf_mac_addr bssId, void *pContext)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0};
	uint32_t sessionId = WLAN_UMAC_VDEV_ID_MAX;
	tAniGetSnrReq *pMsg;

	pMsg = qdf_mem_malloc(sizeof(tAniGetSnrReq));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	status = csr_roam_get_session_id_from_bssid(mac, &bssId, &sessionId);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		qdf_mem_free(pMsg);
		sme_err("Couldn't find session_id for given BSSID");
		return status;
	}

	pMsg->msgType = eWNI_SME_GET_SNR_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniGetSnrReq);
	pMsg->sessionId = sessionId;
	pMsg->snrCallback = callback;
	pMsg->pDevContext = pContext;
	msg.type = eWNI_SME_GET_SNR_REQ;
	msg.bodyptr = pMsg;
	msg.reserved = 0;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 &msg)) {
		qdf_mem_free((void *)pMsg);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * csr_roam_set_key_mgmt_offload() - enable/disable key mgmt offload
 * @mac_ctx: mac context.
 * @session_id: Session Identifier
 * @pmkid_modes: PMKID modes of PMKSA caching and OKC
 *
 * Return: QDF_STATUS_SUCCESS - CSR updated config successfully.
 * Other status means CSR is failed to update.
 */

QDF_STATUS csr_roam_set_key_mgmt_offload(struct mac_context *mac_ctx,
					 uint32_t session_id,
					 struct pmkid_mode_bits *pmkid_modes)
{
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	if (!session) {
		sme_err("session %d not found", session_id);
		return QDF_STATUS_E_FAILURE;
	}
	session->pmkid_modes.fw_okc = pmkid_modes->fw_okc;
	session->pmkid_modes.fw_pmksa_cache = pmkid_modes->fw_pmksa_cache;
	return QDF_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_FIPS
QDF_STATUS
csr_process_roam_pmkid_req_callback(struct mac_context *mac_ctx,
				    uint8_t vdev_id,
				    struct roam_pmkid_req_event *src_lst)
{
	struct csr_roam_info *roam_info;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, vdev_id);
	struct qdf_mac_addr *dst_list;
	QDF_STATUS status;
	uint32_t num_entries, i;

	if (!session)
		return QDF_STATUS_E_NULL_VALUE;

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_NOMEM;

	num_entries = src_lst->num_entries;
	for (i = 0; i < num_entries; i++) {
		dst_list = &src_lst->ap_bssid[i];
		qdf_copy_macaddr(&roam_info->bssid, dst_list);

		status = csr_roam_call_callback(mac_ctx, vdev_id, roam_info,
						0, eCSR_ROAM_FIPS_PMK_REQUEST,
						eCSR_ROAM_RESULT_NONE);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("Trigger pmkid fallback failed");
			qdf_mem_free(roam_info);
			return status;
		}
	}
	qdf_mem_free(roam_info);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
csr_roam_pmkid_req_callback(uint8_t vdev_id,
			    struct roam_pmkid_req_event *src_lst)
{
	QDF_STATUS status;
	struct mac_context *mac_ctx;

	mac_ctx = cds_get_context(QDF_MODULE_ID_PE);
	if (!mac_ctx) {
		sme_err("NULL mac ptr");
		QDF_ASSERT(0);
		return -EINVAL;
	}

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Locking failed, bailing out");
		return status;
	}

	status = csr_process_roam_pmkid_req_callback(mac_ctx, vdev_id,
						     src_lst);
	sme_release_global_lock(&mac_ctx->sme);

	return status;
}
#endif /* WLAN_FEATURE_FIPS */
#else
static inline void
csr_update_roam_scan_offload_request(struct mac_context *mac_ctx,
				     struct roam_offload_scan_req *req_buf,
				     struct csr_roam_session *session)
{
	req_buf->roam_force_rssi_trigger =
			mac_ctx->mlme_cfg->lfr.roam_force_rssi_trigger;
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)

/**
 * csr_update_btm_offload_config() - Update btm config param to fw
 * @mac_ctx: Global mac ctx
 * @command: Roam offload command
 * @btm_offload_config: btm offload config
 * @session: roam session
 *
 * Return: None
 */
static void csr_update_btm_offload_config(struct mac_context *mac_ctx,
					  uint8_t command,
					  uint32_t *btm_offload_config,
					  struct csr_roam_session *session)
{
	struct wlan_objmgr_peer *peer;
	struct wlan_objmgr_vdev *vdev;
	bool is_pmf_enabled, is_open_connection = false;
	int32_t cipher;

	*btm_offload_config =
			mac_ctx->mlme_cfg->btm.btm_offload_config;

	/* Return if INI is disabled */
	if (!(*btm_offload_config))
		return;

	/* For RSO Stop Disable BTM offload to firmware */
	if (command == ROAM_SCAN_OFFLOAD_STOP) {
		*btm_offload_config = 0;
		return;
	}

	if (!session->pConnectBssDesc) {
		sme_err("Connected Bss Desc is NULL");
		return;
	}

	peer = wlan_objmgr_get_peer(mac_ctx->psoc,
				    wlan_objmgr_pdev_get_pdev_id(mac_ctx->pdev),
				    session->pConnectBssDesc->bssId,
				    WLAN_LEGACY_SME_ID);
	if (!peer) {
		sme_debug("Peer of peer_mac "QDF_MAC_ADDR_FMT" not found",
			  QDF_MAC_ADDR_REF(session->pConnectBssDesc->bssId));
		return;
	}

	is_pmf_enabled = mlme_get_peer_pmf_status(peer);

	wlan_objmgr_peer_release_ref(peer, WLAN_LEGACY_SME_ID);

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev:%d is NULL", session->vdev_id);
		return;
	}

	cipher = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_UCAST_CIPHER);
	if (!cipher || QDF_HAS_PARAM(cipher, WLAN_CRYPTO_CIPHER_NONE))
		is_open_connection = true;

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	/*
	 * If peer does not support PMF in case of OCE/MBO
	 * Connection, Disable BTM offload to firmware.
	 */
	if (session->pConnectBssDesc->mbo_oce_enabled_ap &&
	    (!is_pmf_enabled && !is_open_connection))
		*btm_offload_config = 0;

	sme_debug("is_open:%d is_pmf_enabled %d btm_offload_cfg:%d for "QDF_MAC_ADDR_FMT,
		  is_open_connection, is_pmf_enabled,
		  *btm_offload_config,
		  QDF_MAC_ADDR_REF(session->pConnectBssDesc->bssId));
}

/**
 * csr_add_rssi_reject_ap_list() - add rssi reject AP list to the
 * roam params
 * @mac_ctx: mac ctx.
 * @roam_params: roam params in which reject AP list needs
 * to be populated.
 *
 * Return: None
 */
static void
csr_add_rssi_reject_ap_list(struct mac_context *mac_ctx,
			    struct roam_ext_params *roam_params)
{
	int i = 0;
	struct reject_ap_config_params *reject_list;

	reject_list = qdf_mem_malloc(sizeof(*reject_list) *
				     MAX_RSSI_AVOID_BSSID_LIST);
	if (!reject_list)
		return;

	roam_params->num_rssi_rejection_ap =
		wlan_blm_get_bssid_reject_list(mac_ctx->pdev, reject_list,
					       MAX_RSSI_AVOID_BSSID_LIST,
					       DRIVER_RSSI_REJECT_TYPE);
	if (!roam_params->num_rssi_rejection_ap) {
		qdf_mem_free(reject_list);
		return;
	}

	for (i = 0; i < roam_params->num_rssi_rejection_ap; i++) {
		roam_params->rssi_reject_bssid_list[i] = reject_list[i];
		sme_debug("BSSID "QDF_MAC_ADDR_FMT" expected rssi %d remaining duration %d",
			  QDF_MAC_ADDR_REF(roam_params->rssi_reject_bssid_list[i].bssid.bytes),
			  roam_params->rssi_reject_bssid_list[i].expected_rssi,
			  roam_params->rssi_reject_bssid_list[i].
							reject_duration);
	}

	qdf_mem_free(reject_list);
}

/**
 * csr_update_11k_offload_params - Update 11K offload params
 * @mac_ctx: MAC context
 * @session: Pointer to the CSR Roam Session
 * @params: Pointer to the roam 11k offload params
 * @enabled: 11k offload enabled/disabled.
 *
 * API to update 11k offload params
 *
 * Return: none
 */
static void
csr_update_11k_offload_params(struct mac_context *mac_ctx,
			      struct csr_roam_session *session,
			      struct wlan_roam_11k_offload_params *params,
			      bool enabled)
{
	struct csr_config *csr_config = &mac_ctx->roam.configParam;
	struct csr_neighbor_report_offload_params *neighbor_report_offload =
		&csr_config->neighbor_report_offload;

	params->vdev_id = session->sessionId;

	if (enabled) {
		params->offload_11k_bitmask =
				csr_config->offload_11k_enable_bitmask;
	} else {
		params->offload_11k_bitmask = 0;
		return;
	}

	/*
	 * If none of the parameters are enabled, then set the
	 * offload_11k_bitmask to 0, so that we don't send the command
	 * to the FW and drop it in WMA
	 */
	if ((neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_ALL) == 0) {
		sme_err("No valid neighbor report offload params %x",
			neighbor_report_offload->params_bitmask);
		params->offload_11k_bitmask = 0;
		return;
	}

	/*
	 * First initialize all params to NEIGHBOR_REPORT_PARAM_INVALID
	 * Then set the values that are enabled
	 */
	params->neighbor_report_params.time_offset =
		NEIGHBOR_REPORT_PARAM_INVALID;
	params->neighbor_report_params.low_rssi_offset =
		NEIGHBOR_REPORT_PARAM_INVALID;
	params->neighbor_report_params.bmiss_count_trigger =
		NEIGHBOR_REPORT_PARAM_INVALID;
	params->neighbor_report_params.per_threshold_offset =
		NEIGHBOR_REPORT_PARAM_INVALID;
	params->neighbor_report_params.neighbor_report_cache_timeout =
		NEIGHBOR_REPORT_PARAM_INVALID;
	params->neighbor_report_params.max_neighbor_report_req_cap =
		NEIGHBOR_REPORT_PARAM_INVALID;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_TIME_OFFSET)
		params->neighbor_report_params.time_offset =
			neighbor_report_offload->time_offset;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_LOW_RSSI_OFFSET)
		params->neighbor_report_params.low_rssi_offset =
			neighbor_report_offload->low_rssi_offset;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_BMISS_COUNT_TRIGGER)
		params->neighbor_report_params.bmiss_count_trigger =
			neighbor_report_offload->bmiss_count_trigger;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_PER_THRESHOLD_OFFSET)
		params->neighbor_report_params.per_threshold_offset =
			neighbor_report_offload->per_threshold_offset;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_CACHE_TIMEOUT)
		params->neighbor_report_params.neighbor_report_cache_timeout =
			neighbor_report_offload->neighbor_report_cache_timeout;

	if (neighbor_report_offload->params_bitmask &
	    NEIGHBOR_REPORT_PARAMS_MAX_REQ_CAP)
		params->neighbor_report_params.max_neighbor_report_req_cap =
			neighbor_report_offload->max_neighbor_report_req_cap;

	params->neighbor_report_params.ssid.length =
		session->connectedProfile.SSID.length;
	qdf_mem_copy(params->neighbor_report_params.ssid.ssid,
			session->connectedProfile.SSID.ssId,
			session->connectedProfile.SSID.length);
}

QDF_STATUS csr_invoke_neighbor_report_request(
				uint8_t session_id,
				struct sRrmNeighborReq *neighbor_report_req,
				bool send_resp_to_host)
{
	struct wmi_invoke_neighbor_report_params *invoke_params;
	struct scheduler_msg msg = {0};

	if (!neighbor_report_req) {
		sme_err("Invalid params");
		return QDF_STATUS_E_INVAL;
	}

	invoke_params = qdf_mem_malloc(sizeof(*invoke_params));
	if (!invoke_params)
		return QDF_STATUS_E_NOMEM;

	invoke_params->vdev_id = session_id;
	invoke_params->send_resp_to_host = send_resp_to_host;

	if (!neighbor_report_req->no_ssid) {
		invoke_params->ssid.length = neighbor_report_req->ssid.length;
		qdf_mem_copy(invoke_params->ssid.ssid,
			     neighbor_report_req->ssid.ssId,
			     neighbor_report_req->ssid.length);
	} else {
		invoke_params->ssid.length = 0;
	}

	sme_debug("Sending SIR_HAL_INVOKE_NEIGHBOR_REPORT");

	msg.type = SIR_HAL_INVOKE_NEIGHBOR_REPORT;
	msg.reserved = 0;
	msg.bodyptr = invoke_params;

	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_WMA,
							 QDF_MODULE_ID_WMA,
							 &msg)) {
		sme_err("Not able to post message to WMA");
		qdf_mem_free(invoke_params);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_cm_apend_assoc_ies() - Append specific IE to assoc IE's buffer
 * @req_buf: Pointer to Roam offload scan request
 * @ie_id: IE ID to be appended
 * @ie_len: IE length to be appended
 * @ie_data: IE data to be appended
 *
 * Return: None
 */
static void
csr_cm_append_assoc_ies(struct wlan_roam_scan_offload_params *rso_mode_cfg,
			uint8_t ie_id, uint8_t ie_len,
			const uint8_t *ie_data)
{
	uint32_t curr_length = rso_mode_cfg->assoc_ie_length;

	if ((SIR_MAC_MAX_ADD_IE_LENGTH - curr_length) < ie_len) {
		sme_err("Appending IE id: %d failed", ie_id);
		return;
	}

	rso_mode_cfg->assoc_ie[curr_length] = ie_id;
	rso_mode_cfg->assoc_ie[curr_length + 1] = ie_len;
	qdf_mem_copy(&rso_mode_cfg->assoc_ie[curr_length + 2], ie_data, ie_len);
	rso_mode_cfg->assoc_ie_length += (ie_len + 2);
}

#ifdef FEATURE_WLAN_ESE
static void csr_cm_esr_populate_version_ie(
			struct mac_context *mac_ctx,
			struct wlan_roam_scan_offload_params *rso_mode_cfg)
{
	static const uint8_t ese_ie[] = {0x0, 0x40, 0x96, 0x3,
					 ESE_VERSION_SUPPORTED};

	/* Append ESE version IE if isEseIniFeatureEnabled INI is enabled */
	if (mac_ctx->mlme_cfg->lfr.ese_enabled)
		csr_cm_append_assoc_ies(rso_mode_cfg, WLAN_ELEMID_VENDOR,
					sizeof(ese_ie), ese_ie);
}

/**
 * csr_cm_ese_populate_addtional_ies() - add IEs to reassoc frame
 * @mac_ctx: Pointer to global mac structure
 * @session: pointer to CSR session
 * @req_buf: Pointer to Roam offload scan request
 *
 * This function populates the TSPEC ie and appends the info
 * to assoc buffer.
 *
 * Return: None
 */
static void csr_cm_ese_populate_addtional_ies(
		struct mac_context *mac_ctx,
		struct csr_roam_session *session,
		struct wlan_roam_scan_offload_params *rso_mode_cfg)
{
	uint8_t tspec_ie_hdr[SIR_MAC_OUI_WME_HDR_MIN]
			= { 0x00, 0x50, 0xf2, 0x02, 0x02, 0x01 };
	uint8_t tspec_ie_buf[DOT11F_IE_WMMTSPEC_MAX_LEN], j;
	ese_wmm_tspec_ie *tspec_ie;
	tESETspecInfo ese_tspec;

	tspec_ie = (ese_wmm_tspec_ie *)(tspec_ie_buf + SIR_MAC_OUI_WME_HDR_MIN);
	if (csr_is_wmm_supported(mac_ctx) &&
	    mac_ctx->mlme_cfg->lfr.ese_enabled &&
	    csr_roam_is_ese_assoc(mac_ctx, session->sessionId)) {
		ese_tspec.numTspecs = sme_qos_ese_retrieve_tspec_info(
					mac_ctx, session->sessionId,
					(tTspecInfo *)&ese_tspec.tspec[0]);
		qdf_mem_copy(tspec_ie_buf, tspec_ie_hdr,
			     SIR_MAC_OUI_WME_HDR_MIN);
		for (j = 0; j < ese_tspec.numTspecs; j++) {
			/* Populate the tspec_ie */
			ese_populate_wmm_tspec(&ese_tspec.tspec[j].tspec,
					       tspec_ie);
			csr_cm_append_assoc_ies(rso_mode_cfg,
						WLAN_ELEMID_VENDOR,
						DOT11F_IE_WMMTSPEC_MAX_LEN,
						tspec_ie_buf);
		}
	}
}
#else
static inline void csr_cm_esr_populate_version_ie(
			struct mac_context *mac_ctx,
			struct wlan_roam_scan_offload_params *rso_mode_cfg)
{}

static inline void csr_cm_ese_populate_addtional_ies(
		struct mac_context *mac_ctx,
		struct csr_roam_session *session,
		struct wlan_roam_scan_offload_params *rso_mode_cfg)
{}
#endif

/**
 * csr_cm_update_driver_assoc_ies  - Append driver built IE's to assoc IE's
 * @mac_ctx: Pointer to global mac structure
 * @session: pointer to CSR session
 * @rso_mode_cfg: Pointer to Roam offload scan request
 *
 * Return: None
 */
static void csr_cm_update_driver_assoc_ies(
		struct mac_context *mac_ctx,
		struct csr_roam_session *session,
		struct wlan_roam_scan_offload_params *rso_mode_cfg)
{
	uint32_t csr_11henable;
	bool power_caps_populated = false;
	uint8_t *rrm_cap_ie_data =
		(uint8_t *)&mac_ctx->rrm.rrmPEContext.rrmEnabledCaps;
	uint8_t power_cap_ie_data[DOT11F_IE_POWERCAPS_MAX_LEN] = {
		MIN_TX_PWR_CAP, MAX_TX_PWR_CAP};
	uint8_t max_tx_pwr_cap = 0;
	uint8_t supp_chan_ie[DOT11F_IE_SUPPCHANNELS_MAX_LEN], supp_chan_ie_len;
	static const uint8_t qcn_ie[] = {0x8C, 0xFD, 0xF0, 0x1,
					 QCN_IE_VERSION_SUBATTR_ID,
					 QCN_IE_VERSION_SUBATTR_DATA_LEN,
					 QCN_IE_VERSION_SUPPORTED,
					 QCN_IE_SUBVERSION_SUPPORTED};

	/* Re-Assoc IE TLV parameters */
	rso_mode_cfg->assoc_ie_length = session->nAddIEAssocLength;
	qdf_mem_copy(rso_mode_cfg->assoc_ie, session->pAddIEAssoc,
		     rso_mode_cfg->assoc_ie_length);

	if (session->pConnectBssDesc)
		max_tx_pwr_cap = csr_get_cfg_max_tx_power(
					mac_ctx,
					session->pConnectBssDesc->chan_freq);

	if (max_tx_pwr_cap && max_tx_pwr_cap < MAX_TX_PWR_CAP)
		power_cap_ie_data[1] = max_tx_pwr_cap;
	else
		power_cap_ie_data[1] = MAX_TX_PWR_CAP;

	csr_11henable = mac_ctx->mlme_cfg->gen.enabled_11h;

	if (csr_11henable && csr_is11h_supported(mac_ctx)) {
		/* Append power cap IE */
		csr_cm_append_assoc_ies(rso_mode_cfg, WLAN_ELEMID_PWRCAP,
					DOT11F_IE_POWERCAPS_MAX_LEN,
					power_cap_ie_data);
		power_caps_populated = true;

		/* Append Supported channels IE */
		csr_add_supported_5Ghz_channels(mac_ctx, supp_chan_ie,
						&supp_chan_ie_len, true);

		csr_cm_append_assoc_ies(rso_mode_cfg,
					WLAN_ELEMID_SUPPCHAN,
					supp_chan_ie_len, supp_chan_ie);
	}

	csr_cm_esr_populate_version_ie(mac_ctx, rso_mode_cfg);

	if (mac_ctx->rrm.rrmPEContext.rrmEnable) {
		/* Append RRM IE */
		csr_cm_append_assoc_ies(rso_mode_cfg, WLAN_ELEMID_RRM,
					DOT11F_IE_RRMENABLEDCAP_MAX_LEN,
					rrm_cap_ie_data);

		/* Append Power cap IE if not appended already */
		if (!power_caps_populated)
			csr_cm_append_assoc_ies(rso_mode_cfg,
						WLAN_ELEMID_PWRCAP,
						DOT11F_IE_POWERCAPS_MAX_LEN,
						power_cap_ie_data);
	}

	csr_cm_ese_populate_addtional_ies(mac_ctx, session, rso_mode_cfg);

	/* Append QCN IE if g_support_qcn_ie INI is enabled */
	if (mac_ctx->mlme_cfg->sta.qcn_ie_support)
		csr_cm_append_assoc_ies(rso_mode_cfg, WLAN_ELEMID_VENDOR,
					sizeof(qcn_ie), qcn_ie);
}

#if defined(WLAN_FEATURE_FILS_SK)
QDF_STATUS csr_update_fils_config(struct mac_context *mac, uint8_t session_id,
				  struct csr_roam_profile *src_profile)
{
	struct csr_roam_session *session = CSR_GET_SESSION(mac, session_id);
	struct csr_roam_profile *dst_profile = NULL;

	if (!session) {
		sme_err("session NULL");
		return QDF_STATUS_E_FAILURE;
	}

	dst_profile = session->pCurRoamProfile;

	if (!dst_profile) {
		sme_err("Current Roam profile of SME session NULL");
		return QDF_STATUS_E_FAILURE;
	}
	update_profile_fils_info(mac, dst_profile, src_profile,
				 session_id);
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * csr_update_score_params() - API to update Score params in RSO
 * @mac_ctx: Mac context
 * @req_score_params: request score params
 *
 * Return: None
 */
static void csr_update_score_params(struct mac_context *mac_ctx,
				    struct scoring_param *req_score_params,
				    tpCsrNeighborRoamControlInfo roam_info)
{
	struct wlan_mlme_roam_scoring_cfg *roam_score_params;
	struct weight_cfg *weight_config;
	struct psoc_mlme_obj *mlme_psoc_obj;
	struct scoring_cfg *score_config;

	mlme_psoc_obj = wlan_psoc_mlme_get_cmpt_obj(mac_ctx->psoc);

	if (!mlme_psoc_obj)
		return;

	score_config = &mlme_psoc_obj->psoc_cfg.score_config;
	roam_score_params = &mac_ctx->mlme_cfg->roam_scoring;
	weight_config = &score_config->weight_config;

	if (!roam_info->cfgParams.enable_scoring_for_roam)
		req_score_params->disable_bitmap =
			WLAN_ROAM_SCORING_DISABLE_ALL;

	req_score_params->rssi_weightage = weight_config->rssi_weightage;
	req_score_params->ht_weightage = weight_config->ht_caps_weightage;
	req_score_params->vht_weightage = weight_config->vht_caps_weightage;
	req_score_params->he_weightage = weight_config->he_caps_weightage;
	req_score_params->bw_weightage = weight_config->chan_width_weightage;
	req_score_params->band_weightage = weight_config->chan_band_weightage;
	req_score_params->nss_weightage = weight_config->nss_weightage;
	req_score_params->esp_qbss_weightage =
		weight_config->channel_congestion_weightage;
	req_score_params->beamforming_weightage =
		weight_config->beamforming_cap_weightage;
	req_score_params->pcl_weightage =
		weight_config->pcl_weightage;
	req_score_params->oce_wan_weightage = weight_config->oce_wan_weightage;
	req_score_params->oce_ap_tx_pwr_weightage =
		weight_config->oce_ap_tx_pwr_weightage;
	req_score_params->oce_subnet_id_weightage =
		weight_config->oce_subnet_id_weightage;
	req_score_params->sae_pk_ap_weightage =
		weight_config->sae_pk_ap_weightage;

	req_score_params->bw_index_score =
		score_config->bandwidth_weight_per_index;
	req_score_params->band_index_score =
		score_config->band_weight_per_index;
	req_score_params->nss_index_score =
		score_config->nss_weight_per_index;

	req_score_params->vendor_roam_score_algorithm =
			score_config->vendor_roam_score_algorithm;


	req_score_params->roam_score_delta =
				roam_score_params->roam_score_delta;
	req_score_params->roam_trigger_bitmap =
				roam_score_params->roam_trigger_bitmap;

	qdf_mem_copy(&req_score_params->rssi_scoring, &score_config->rssi_score,
		     sizeof(struct rssi_config_score));
	qdf_mem_copy(&req_score_params->esp_qbss_scoring,
		     &score_config->esp_qbss_scoring,
		     sizeof(struct per_slot_score));
	qdf_mem_copy(&req_score_params->oce_wan_scoring,
		     &score_config->oce_wan_scoring,
		     sizeof(struct per_slot_score));
	req_score_params->cand_min_roam_score_delta =
					roam_score_params->min_roam_score_delta;
}

uint8_t
csr_get_roam_enabled_sta_sessionid(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	struct csr_roam_session *session;
	uint8_t i;

	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		session = CSR_GET_SESSION(mac_ctx, i);
		if (!session || !CSR_IS_SESSION_VALID(mac_ctx, i))
			continue;
		if (!session->pCurRoamProfile ||
		    session->pCurRoamProfile->csrPersona != QDF_STA_MODE)
			continue;

		if (vdev_id == i)
			continue;

		if (MLME_IS_ROAM_INITIALIZED(mac_ctx->psoc, i))
			return i;
	}

	return WLAN_UMAC_VDEV_ID_MAX;
}

#ifdef WLAN_ADAPTIVE_11R
static bool
csr_is_adaptive_11r_roam_supported(struct mac_context *mac_ctx,
				   struct csr_roam_session *session)
{
	if (session->is_adaptive_11r_connection)
		return mac_ctx->mlme_cfg->lfr.tgt_adaptive_11r_cap;

	return true;
}
#else
static bool
csr_is_adaptive_11r_roam_supported(struct mac_context *mac_ctx,
				   struct csr_roam_session *session)

{
	return true;
}
#endif

QDF_STATUS
csr_post_roam_state_change(struct mac_context *mac, uint8_t vdev_id,
			   enum roam_offload_state state, uint8_t reason)
{
	return wlan_cm_roam_state_change(mac->pdev, vdev_id, state, reason);
}

QDF_STATUS
csr_roam_offload_scan(struct mac_context *mac_ctx, uint8_t session_id,
		      uint8_t command, uint8_t reason)
{
	return wlan_cm_roam_send_rso_cmd(mac_ctx->psoc, session_id, command,
					 reason);
}

QDF_STATUS
cm_akm_roam_allowed(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	struct csr_roam_session *session;
	enum csr_akm_type roam_profile_akm = eCSR_AUTH_TYPE_UNKNOWN;
	uint32_t fw_akm_bitmap;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "session is null");
		return QDF_STATUS_E_FAILURE;
	}

	if (session->pCurRoamProfile)
		roam_profile_akm =
			session->pCurRoamProfile->AuthType.authType[0];
	else
		sme_info("Roam profile is NULL");

	if (CSR_IS_AKM_FILS(roam_profile_akm) &&
	    !mac_ctx->is_fils_roaming_supported) {
		sme_info("FILS Roaming not suppprted by fw");
		return QDF_STATUS_E_NOSUPPORT;
	}

	if (!csr_is_adaptive_11r_roam_supported(mac_ctx, session)) {
		sme_info("Adaptive 11r Roaming not suppprted by fw");
		return QDF_STATUS_E_NOSUPPORT;
	}

	fw_akm_bitmap = mac_ctx->mlme_cfg->lfr.fw_akm_bitmap;
	/* Roaming is not supported currently for OWE akm */
	if (roam_profile_akm == eCSR_AUTH_TYPE_OWE &&
	    !CSR_IS_FW_OWE_ROAM_SUPPORTED(fw_akm_bitmap)) {
		sme_info("OWE Roaming not suppprted by fw");
		return QDF_STATUS_E_NOSUPPORT;
	}

	/* Roaming is not supported for SAE authentication */
	if (CSR_IS_AUTH_TYPE_SAE(roam_profile_akm) &&
	    !CSR_IS_FW_SAE_ROAM_SUPPORTED(fw_akm_bitmap)) {
		sme_info("Roaming not suppprted for SAE connection");
		return QDF_STATUS_E_NOSUPPORT;
	}

	if ((roam_profile_akm == eCSR_AUTH_TYPE_SUITEB_EAP_SHA256 ||
	     roam_profile_akm == eCSR_AUTH_TYPE_SUITEB_EAP_SHA384) &&
	     !CSR_IS_FW_SUITEB_ROAM_SUPPORTED(fw_akm_bitmap)) {
		sme_info("Roaming not supported for SUITEB connection");
		return QDF_STATUS_E_NOSUPPORT;
	}

	/*
	 * If fw doesn't advertise FT SAE, FT-FILS or FT-Suite-B capability,
	 * don't support roaming to that profile
	 */
	if (CSR_IS_AKM_FT_SAE(roam_profile_akm)) {
		if (!CSR_IS_FW_FT_SAE_SUPPORTED(fw_akm_bitmap)) {
			sme_info("Roaming not suppprted for FT SAE akm");
			return QDF_STATUS_E_NOSUPPORT;
		}
	}

	if (CSR_IS_AKM_FT_SUITEB_SHA384(roam_profile_akm)) {
		if (!CSR_IS_FW_FT_SUITEB_SUPPORTED(fw_akm_bitmap)) {
			sme_info("Roaming not suppprted for FT Suite-B akm");
			return QDF_STATUS_E_NOSUPPORT;
		}
	}

	if (CSR_IS_AKM_FT_FILS(roam_profile_akm)) {
		if (!CSR_IS_FW_FT_FILS_SUPPORTED(fw_akm_bitmap)) {
			sme_info("Roaming not suppprted for FT FILS akm");
			return QDF_STATUS_E_NOSUPPORT;
		}
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_cm_roam_cmd_allowed(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			 uint8_t command, uint8_t reason)
{
	uint8_t *state = NULL;
	struct mac_context *mac_ctx;
	tpCsrNeighborRoamControlInfo roam_info;
	bool p2p_disable_sta_roaming = 0, nan_disable_sta_roaming = 0;
	QDF_STATUS status;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	sme_debug("RSO Command %d, vdev %d, Reason %d",
		  command, vdev_id, reason);

	roam_info = &mac_ctx->roam.neighborRoamInfo[vdev_id];

	if (roam_info->neighborRoamState !=
	    eCSR_NEIGHBOR_ROAM_STATE_CONNECTED &&
	    (command == ROAM_SCAN_OFFLOAD_UPDATE_CFG ||
	     command == ROAM_SCAN_OFFLOAD_START ||
	     command == ROAM_SCAN_OFFLOAD_RESTART)) {
		sme_debug("Session not in connected state, RSO not sent and state=%d ",
			  roam_info->neighborRoamState);
		return QDF_STATUS_E_FAILURE;
	}

	status = cm_akm_roam_allowed(mac_ctx, vdev_id);
	if (QDF_IS_STATUS_ERROR(status))
		return status;

	p2p_disable_sta_roaming =
		(cfg_p2p_is_roam_config_disabled(psoc) &&
		(policy_mgr_mode_specific_connection_count(
					psoc, PM_P2P_CLIENT_MODE, NULL) ||
		policy_mgr_mode_specific_connection_count(
					psoc, PM_P2P_GO_MODE, NULL)));
	nan_disable_sta_roaming =
	    (cfg_nan_is_roam_config_disabled(psoc) &&
	    policy_mgr_mode_specific_connection_count(psoc, PM_NDI_MODE, NULL));

	if ((command == ROAM_SCAN_OFFLOAD_START ||
	     command == ROAM_SCAN_OFFLOAD_UPDATE_CFG) &&
	     (p2p_disable_sta_roaming || nan_disable_sta_roaming)) {
		sme_info("roaming not supported for active %s connection",
			 p2p_disable_sta_roaming ? "p2p" : "ndi");
		return QDF_STATUS_E_FAILURE;
	}

	/*
	 * The Dynamic Config Items Update may happen even if the state is in
	 * INIT. It is important to ensure that the command is passed down to
	 * the FW only if the Infra Station is in a connected state. A connected
	 * station could also be in a PREAUTH or REASSOC states.
	 * 1) Block all CMDs that are not STOP in INIT State. For STOP always
	 *    inform firmware irrespective of state.
	 * 2) Block update cfg CMD if its for REASON_ROAM_SET_BLACKLIST_BSSID,
	 *    because we need to inform firmware of blacklisted AP for PNO in
	 *    all states.
	 */
	if ((roam_info->neighborRoamState == eCSR_NEIGHBOR_ROAM_STATE_INIT) &&
	    (command != ROAM_SCAN_OFFLOAD_STOP) &&
	    (reason != REASON_ROAM_SET_BLACKLIST_BSSID)) {
		state = mac_trace_get_neighbour_roam_state(
				roam_info->neighborRoamState);
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  FL("Scan Command not sent to FW state=%s and cmd=%d"),
			  state,  command);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_cm_roam_scan_offload_rssi_thresh() - set roam offload scan rssi
 * parameters
 * @mac_ctx: global mac ctx
 * @session: csr roam session
 * @params:  roam offload scan rssi related parameters
 *
 * This function is used to set roam offload scan rssi related parameters
 *
 * Return: None
 */
static void
csr_cm_roam_scan_offload_rssi_thresh(struct mac_context *mac_ctx,
			struct csr_roam_session *session,
			struct wlan_roam_offload_scan_rssi_params *params)
{
	struct roam_ext_params *roam_params;
		tpCsrNeighborRoamControlInfo roam_info =
		&mac_ctx->roam.neighborRoamInfo[session->vdev_id];

	roam_params = &mac_ctx->roam.configParam.roam_params;

	if (roam_params->alert_rssi_threshold)
		params->rssi_thresh = roam_params->alert_rssi_threshold;
	else
		params->rssi_thresh =
			(int8_t)roam_info->cfgParams.neighborLookupThreshold *
			(-1);

	params->vdev_id = session->vdev_id;
	params->rssi_thresh_diff =
		roam_info->cfgParams.nOpportunisticThresholdDiff & 0x000000ff;
	params->hi_rssi_scan_max_count =
		roam_info->cfgParams.hi_rssi_scan_max_count;
	/*
	 * If the current operation channel is 5G frequency band, then
	 * there is no need to enable the HI_RSSI feature. This feature
	 * is useful only if we are connected to a 2.4 GHz AP and we wish
	 * to connect to a better 5GHz AP is available.
	 */
	if (session->disable_hi_rssi)
		params->hi_rssi_scan_rssi_delta = 0;
	else
		params->hi_rssi_scan_rssi_delta =
			roam_info->cfgParams.hi_rssi_scan_rssi_delta;
	params->hi_rssi_scan_rssi_ub =
		roam_info->cfgParams.hi_rssi_scan_rssi_ub;
	params->raise_rssi_thresh_5g =
		mac_ctx->mlme_cfg->lfr.rssi_boost_threshold_5g;
	params->dense_rssi_thresh_offset =
		mac_ctx->mlme_cfg->lfr.roam_dense_rssi_thre_offset;
	params->dense_min_aps_cnt = mac_ctx->mlme_cfg->lfr.roam_dense_min_aps;
	params->traffic_threshold =
			mac_ctx->mlme_cfg->lfr.roam_dense_traffic_threshold;

	/* Set initial dense roam status */
	if (mac_ctx->scan.roam_candidate_count[params->vdev_id] >
	    params->dense_min_aps_cnt)
		params->initial_dense_status = true;

	params->bg_scan_bad_rssi_thresh =
		mac_ctx->mlme_cfg->lfr.roam_bg_scan_bad_rssi_threshold;
	params->bg_scan_client_bitmap =
		mac_ctx->mlme_cfg->lfr.roam_bg_scan_client_bitmap;
	params->roam_bad_rssi_thresh_offset_2g =
			mac_ctx->mlme_cfg->lfr.roam_bg_scan_bad_rssi_offset_2g;
	params->roam_data_rssi_threshold_triggers =
		mac_ctx->mlme_cfg->lfr.roam_data_rssi_threshold_triggers;
	params->roam_data_rssi_threshold =
		mac_ctx->mlme_cfg->lfr.roam_data_rssi_threshold;
	params->rx_data_inactivity_time =
		mac_ctx->mlme_cfg->lfr.rx_data_inactivity_time;

	params->drop_rssi_thresh_5g =
		mac_ctx->mlme_cfg->lfr.rssi_penalize_threshold_5g;

	params->raise_factor_5g = mac_ctx->mlme_cfg->lfr.rssi_boost_factor_5g;
	params->drop_factor_5g = mac_ctx->mlme_cfg->lfr.rssi_penalize_factor_5g;
	params->max_raise_rssi_5g = mac_ctx->mlme_cfg->lfr.max_rssi_boost_5g;
	params->max_drop_rssi_5g = mac_ctx->mlme_cfg->lfr.max_rssi_penalize_5g;

	if (roam_params->good_rssi_roam)
		params->good_rssi_threshold = NOISE_FLOOR_DBM_DEFAULT;
	else
		params->good_rssi_threshold = 0;

	params->early_stop_scan_enable =
		mac_ctx->mlme_cfg->lfr.early_stop_scan_enable;
	if (params->early_stop_scan_enable) {
		params->roam_earlystop_thres_min =
			mac_ctx->mlme_cfg->lfr.early_stop_scan_min_threshold;
		params->roam_earlystop_thres_max =
			mac_ctx->mlme_cfg->lfr.early_stop_scan_max_threshold;
	}

	params->rssi_thresh_offset_5g =
		roam_info->cfgParams.rssi_thresh_offset_5g;
}

/**
 * csr_cm_roam_scan_offload_scan_period() - set roam offload scan period
 * parameters
 * @mac_ctx: global mac ctx
 * @vdev_id: vdev id
 * @params:  roam offload scan period related parameters
 *
 * This function is used to set roam offload scan period related parameters
 *
 * Return: None
 */
static void
csr_cm_roam_scan_offload_scan_period(struct mac_context *mac_ctx,
				    uint8_t vdev_id,
				    struct wlan_roam_scan_period_params *params)
{
	tpCsrNeighborRoamControlInfo roam_info =
			&mac_ctx->roam.neighborRoamInfo[vdev_id];

	params->vdev_id = vdev_id;
	params->empty_scan_refresh_period =
				roam_info->cfgParams.emptyScanRefreshPeriod;
	params->scan_period = params->empty_scan_refresh_period;
	params->scan_age = (3 * params->empty_scan_refresh_period);
	params->roam_scan_inactivity_time =
				roam_info->cfgParams.roam_scan_inactivity_time;
	params->roam_inactive_data_packet_count =
			roam_info->cfgParams.roam_inactive_data_packet_count;
	params->roam_scan_period_after_inactivity =
			roam_info->cfgParams.roam_scan_period_after_inactivity;
	params->full_scan_period =
			roam_info->cfgParams.full_roam_scan_period;
}

static void
csr_cm_roam_fill_crypto_params(struct mac_context *mac_ctx,
			       struct csr_roam_session *session,
			       struct ap_profile *profile)
{
	struct wlan_objmgr_vdev *vdev;
	int32_t uccipher, authmode, mccipher, akm;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
						    session->vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Invalid vdev");
		return;
	}

	akm = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	authmode = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_AUTH_MODE);
	mccipher = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_MCAST_CIPHER);
	uccipher = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_UCAST_CIPHER);
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	profile->rsn_authmode =
		cm_crypto_authmode_to_wmi_authmode(authmode, akm, uccipher);

	/* Pairwise cipher suite */
	profile->rsn_ucastcipherset = cm_crypto_cipher_wmi_cipher(uccipher);

	/* Group cipher suite */
	profile->rsn_mcastcipherset = cm_crypto_cipher_wmi_cipher(mccipher);
}

/**
 * csr_cm_roam_scan_offload_ap_profile() - set roam ap profile parameters
 * @mac_ctx: global mac ctx
 * @session: sme session
 * @params:  roam ap profile related parameters
 *
 * This function is used to set roam ap profile related parameters
 *
 * Return: None
 */
static void
csr_cm_roam_scan_offload_ap_profile(struct mac_context *mac_ctx,
				    struct csr_roam_session *session,
				    struct ap_profile_params *params)
{
	struct ap_profile *profile = &params->profile;
	struct roam_ext_params *roam_params_src =
			&mac_ctx->roam.configParam.roam_params;
	tpCsrNeighborRoamControlInfo roam_info =
			&mac_ctx->roam.neighborRoamInfo[session->vdev_id];

	csr_cm_roam_fill_11w_params(mac_ctx, session->vdev_id, params);
	params->vdev_id = session->vdev_id;
	profile->ssid.length = session->connectedProfile.SSID.length;
	qdf_mem_copy(profile->ssid.ssid, session->connectedProfile.SSID.ssId,
		     profile->ssid.length);

	csr_cm_roam_fill_crypto_params(mac_ctx, session, profile);

	profile->rssi_threshold = roam_info->cfgParams.roam_rssi_diff;
	profile->bg_rssi_threshold =
			roam_info->cfgParams.bg_rssi_threshold;
	/*
	 * rssi_diff which is updated via framework is equivalent to the
	 * INI RoamRssiDiff parameter and hence should be updated.
	 */
	if (roam_params_src->rssi_diff)
		profile->rssi_threshold  = roam_params_src->rssi_diff;

	profile->rssi_abs_thresh =
			mac_ctx->mlme_cfg->lfr.roam_rssi_abs_threshold;

	csr_update_score_params(mac_ctx, &params->param, roam_info);

	params->min_rssi_params[DEAUTH_MIN_RSSI] =
			mac_ctx->mlme_cfg->trig_min_rssi[DEAUTH_MIN_RSSI];
	params->min_rssi_params[BMISS_MIN_RSSI] =
			mac_ctx->mlme_cfg->trig_min_rssi[BMISS_MIN_RSSI];
	params->min_rssi_params[MIN_RSSI_2G_TO_5G_ROAM] =
			mac_ctx->mlme_cfg->trig_min_rssi[MIN_RSSI_2G_TO_5G_ROAM];

	params->score_delta_param[IDLE_ROAM_TRIGGER] =
			mac_ctx->mlme_cfg->trig_score_delta[IDLE_ROAM_TRIGGER];
	params->score_delta_param[BTM_ROAM_TRIGGER] =
			mac_ctx->mlme_cfg->trig_score_delta[BTM_ROAM_TRIGGER];
}

/**
 * csr_cm_populate_roam_chan_list() - Populate roam channel list
 * parameters
 * @mac_ctx: global mac ctx
 * @dst: Destination roam channel buf to populate the roam chan list
 * @src: Source channel list
 *
 * Return: QDF_STATUS enumeration
 */
static QDF_STATUS
csr_cm_populate_roam_chan_list(struct mac_context *mac_ctx,
			       struct wlan_roam_scan_channel_list *dst,
			       tCsrChannelInfo *src)
{
	enum band_info band;
	uint32_t band_cap;
	uint8_t i = 0;
	uint8_t num_channels = 0;
	uint32_t *freq_lst = src->freq_list;

	/*
	 * The INI channels need to be filtered with respect to the current band
	 * that is supported.
	 */
	band_cap = mac_ctx->mlme_cfg->gen.band_capability;
	if (!band_cap) {
		sme_err("Invalid band_cap(%d), roam scan offload req aborted",
			band_cap);
		return QDF_STATUS_E_FAILURE;
	}

	band = wlan_reg_band_bitmap_to_band_info(band_cap);
	num_channels = dst->chan_count;
	for (i = 0; i < src->numOfChannels; i++) {
		if (csr_is_channel_present_in_list(dst->chan_freq_list,
						   num_channels, *freq_lst)) {
			freq_lst++;
			continue;
		}
		if (is_dfs_unsafe_extra_band_chan(mac_ctx, *freq_lst, band)) {
			freq_lst++;
			continue;
		}
		dst->chan_freq_list[num_channels++] = *freq_lst;
		freq_lst++;
	}
	dst->chan_count = num_channels;

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_ESE
static void csr_cm_fetch_ch_lst_from_received_list(
			struct mac_context *mac_ctx,
			tpCsrNeighborRoamControlInfo roam_info,
			tCsrChannelInfo *curr_ch_lst_info,
			struct wlan_roam_scan_channel_list *rso_chan_info)
{
	uint8_t i = 0;
	uint8_t num_channels = 0;
	uint32_t *freq_lst = NULL;
	enum band_info band = BAND_ALL;

	if (curr_ch_lst_info->numOfChannels == 0)
		return;

	freq_lst = curr_ch_lst_info->freq_list;
	for (i = 0; i < curr_ch_lst_info->numOfChannels; i++) {
		if (is_dfs_unsafe_extra_band_chan(mac_ctx, *freq_lst, band)) {
			freq_lst++;
			continue;
		}
		rso_chan_info->chan_freq_list[num_channels++] = *freq_lst;
		freq_lst++;
	}
	rso_chan_info->chan_count = num_channels;
	rso_chan_info->chan_cache_type = CHANNEL_LIST_DYNAMIC;
}
#else
static void csr_cm_fetch_ch_lst_from_received_list(
			struct mac_context *mac_ctx,
			tpCsrNeighborRoamControlInfo roam_info,
			tCsrChannelInfo *curr_ch_lst_info,
			struct wlan_roam_scan_channel_list *rso_chan_info)
{}
#endif

static void csr_cm_fetch_ch_lst_from_occupied_lst(
			struct mac_context *mac_ctx,
			tpCsrNeighborRoamControlInfo roam_info,
			struct wlan_roam_scan_channel_list *rso_chan_info,
			uint8_t vdev_id, uint8_t reason)
{
	uint8_t i = 0;
	uint8_t num_channels = 0;
	uint32_t op_freq;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, vdev_id);
	uint32_t *ch_lst;
	enum band_info band = BAND_ALL;

	if (!session) {
		sme_err("session NULL for vdev:%d", vdev_id);
		return;
	}

	ch_lst = mac_ctx->scan.occupiedChannels[vdev_id].channel_freq_list;
	op_freq = session->connectedProfile.op_freq;

	if (CSR_IS_ROAM_INTRA_BAND_ENABLED(mac_ctx)) {
		if (WLAN_REG_IS_5GHZ_CH_FREQ(op_freq))
			band = BAND_5G;
		else if (WLAN_REG_IS_24GHZ_CH_FREQ(op_freq))
			band = BAND_2G;
		else
			band = BAND_UNKNOWN;
	}

	for (i = 0; i < mac_ctx->scan.occupiedChannels[vdev_id].numChannels;
	     i++) {
		if (is_dfs_unsafe_extra_band_chan(mac_ctx, *ch_lst, band)) {
			ch_lst++;
			continue;
		}
		rso_chan_info->chan_freq_list[num_channels++] = *ch_lst;
		ch_lst++;
	}
	rso_chan_info->chan_count = num_channels;
	rso_chan_info->chan_cache_type = CHANNEL_LIST_DYNAMIC;
}

/**
 * csr_cm_add_ch_lst_from_roam_scan_list() - channel from roam scan chan list
 * parameters
 * @mac_ctx: Global mac ctx
 * @rso_chan_info: RSO channel info
 * @roam_info: roam info struct
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS csr_cm_add_ch_lst_from_roam_scan_list(
			struct mac_context *mac_ctx,
			struct wlan_roam_scan_channel_list *rso_chan_info,
			tpCsrNeighborRoamControlInfo roam_info)
{
	QDF_STATUS status;
	tCsrChannelInfo *pref_chan_info = &roam_info->cfgParams.pref_chan_info;

	if (!pref_chan_info->numOfChannels)
		return QDF_STATUS_SUCCESS;

	status = csr_cm_populate_roam_chan_list(mac_ctx,
					     rso_chan_info,
					     pref_chan_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to copy channels to roam list");
		return status;
	}
	sme_dump_freq_list(pref_chan_info);
	rso_chan_info->chan_cache_type = CHANNEL_LIST_DYNAMIC;

	return QDF_STATUS_SUCCESS;
}

/**
 * csr_cm_fetch_valid_ch_lst() - fetch channel list from valid channel list and
 * update rso req msg
 * parameters
 * @mac_ctx: global mac ctx
 * @rso_chan_buf: out param, roam offload scan request channel info buffer
 * @vdev_id: Vdev id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS
csr_cm_fetch_valid_ch_lst(struct mac_context *mac_ctx,
			  struct wlan_roam_scan_channel_list *rso_chan_info,
			  uint8_t vdev_id)
{
	QDF_STATUS status;
	uint32_t host_channels = 0;
	uint32_t *ch_freq_list = NULL;
	uint8_t i = 0, num_channels = 0;
	enum band_info band = BAND_ALL;
	uint32_t op_freq;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, vdev_id);

	if (!session) {
		sme_err("session NULL for vdev:%d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	op_freq = session->connectedProfile.op_freq;
	if (CSR_IS_ROAM_INTRA_BAND_ENABLED(mac_ctx)) {
		if (WLAN_REG_IS_5GHZ_CH_FREQ(op_freq))
			band = BAND_5G;
		else if (WLAN_REG_IS_24GHZ_CH_FREQ(op_freq))
			band = BAND_2G;
		else
			band = BAND_UNKNOWN;
	}
	host_channels = sizeof(mac_ctx->roam.valid_ch_freq_list);
	status = csr_get_cfg_valid_channels(mac_ctx,
					    mac_ctx->roam.valid_ch_freq_list,
					    &host_channels);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to get the valid channel list");
		return status;
	}

	ch_freq_list = mac_ctx->roam.valid_ch_freq_list;
	mac_ctx->roam.numValidChannels = host_channels;

	for (i = 0; i < mac_ctx->roam.numValidChannels; i++) {
		if (is_dfs_unsafe_extra_band_chan(mac_ctx, *ch_freq_list,
						  band)) {
			ch_freq_list++;
			continue;
		}
		rso_chan_info->chan_freq_list[num_channels++] =	*ch_freq_list;
		ch_freq_list++;
	}
	rso_chan_info->chan_count = num_channels;
	rso_chan_info->chan_cache_type = CHANNEL_LIST_DYNAMIC;

	return status;
}

/**
 * csr_cm_fetch_ch_lst_from_ini() - fetch channel list from ini and update req msg
 * parameters
 * @mac_ctx:      global mac ctx
 * @roam_info:    roam info struct
 * @rso_chan_info:
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS csr_cm_fetch_ch_lst_from_ini(
			struct mac_context *mac_ctx,
			tpCsrNeighborRoamControlInfo roam_info,
			struct wlan_roam_scan_channel_list *rso_chan_info)
{
	QDF_STATUS status;
	tCsrChannelInfo *specific_chan_info;

	specific_chan_info = &roam_info->cfgParams.specific_chan_info;

	status = csr_cm_populate_roam_chan_list(mac_ctx, rso_chan_info,
						specific_chan_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Failed to copy channels to roam list");
		return status;
	}
	rso_chan_info->chan_cache_type = CHANNEL_LIST_STATIC;

	return QDF_STATUS_SUCCESS;
}

static void
csr_cm_fill_rso_channel_list(struct mac_context *mac_ctx,
			     struct wlan_roam_scan_channel_list *rso_chan_info,
			     uint8_t vdev_id, uint8_t reason)
{
	tpCsrNeighborRoamControlInfo roam_info =
			&mac_ctx->roam.neighborRoamInfo[vdev_id];
	tCsrChannelInfo *specific_chan_info =
			&roam_info->cfgParams.specific_chan_info;
	tpCsrChannelInfo curr_ch_lst_info =
		&roam_info->roamChannelInfo.currentChannelListInfo;
	QDF_STATUS status;
	bool ese_neighbor_list_recvd = false;
	uint8_t ch_cache_str[128] = {0};
	uint8_t i, j;

#ifdef FEATURE_WLAN_ESE
	/*
	 * this flag will be true if connection is ESE and no neighbor
	 * list received or if the connection is not ESE
	 */
	ese_neighbor_list_recvd = ((roam_info->isESEAssoc)
		&& (roam_info->roamChannelInfo.IAPPNeighborListReceived
		    == false)) || (!roam_info->isESEAssoc);
#endif /* FEATURE_WLAN_ESE */

	rso_chan_info->vdev_id = vdev_id;
	if (ese_neighbor_list_recvd ||
	    curr_ch_lst_info->numOfChannels == 0) {
		/*
		 * Retrieve the Channel Cache either from ini or from
		 * the occupied channels list along with preferred
		 * channel list configured by the client.
		 * Give Preference to INI Channels
		 */
		if (specific_chan_info->numOfChannels) {
			status = csr_cm_fetch_ch_lst_from_ini(mac_ctx,
							      roam_info,
							      rso_chan_info);
			if (QDF_IS_STATUS_ERROR(status)) {
				sme_err("Fetch channel list from ini failed");
				return;
			}
		} else if (reason == REASON_FLUSH_CHANNEL_LIST) {
			rso_chan_info->chan_cache_type = CHANNEL_LIST_STATIC;
			rso_chan_info->chan_count = 0;
		} else {
			csr_cm_fetch_ch_lst_from_occupied_lst(mac_ctx,
							      roam_info,
							      rso_chan_info,
							      vdev_id, reason);
			/* Add the preferred channel list configured by
			 * client to the roam channel list along with
			 * occupied channel list.
			 */
			csr_cm_add_ch_lst_from_roam_scan_list(mac_ctx,
							      rso_chan_info,
							      roam_info);
		}
	} else {
		/*
		 * If ESE is enabled, and a neighbor Report is received,
		 * then Ignore the INI Channels or the Occupied Channel
		 * List. Consider the channels in the neighbor list sent
		 * by the ESE AP
		 */
		csr_cm_fetch_ch_lst_from_received_list(mac_ctx, roam_info,
						       curr_ch_lst_info,
						       rso_chan_info);
	}

	if (!rso_chan_info->chan_count &&
	    reason != REASON_FLUSH_CHANNEL_LIST) {
		/* Maintain the Valid Channels List */
		status = csr_cm_fetch_valid_ch_lst(mac_ctx, rso_chan_info,
						   vdev_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_err("Fetch channel list fail");
			return;
		}
	}

	for (i = 0, j = 0; i < rso_chan_info->chan_count; i++) {
		if (j < sizeof(ch_cache_str))
			j += snprintf(ch_cache_str + j,
				      sizeof(ch_cache_str) - j, " %d",
				      rso_chan_info->chan_freq_list[i]);
		else
			break;
	}

	sme_debug("ChnlCacheType:%d, No of Chnls:%d,Channels: %s",
		  rso_chan_info->chan_cache_type,
		  rso_chan_info->chan_count, ch_cache_str);
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#ifdef WLAN_SAE_SINGLE_PMK
static bool
csr_cm_fill_rso_sae_single_pmk_info(struct mac_context *mac_ctx,
				    struct wlan_roam_scan_offload_params *rso_cfg,
				    uint8_t vdev_id)
{
	struct wlan_mlme_sae_single_pmk single_pmk = {0};
	struct wlan_objmgr_vdev *vdev;
	struct wlan_rso_11i_params *rso_11i_info = &rso_cfg->rso_11i_info;
	uint64_t time_expired;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("vdev is NULL");
		return QDF_STATUS_E_NULL_VALUE;
	}

	wlan_mlme_get_sae_single_pmk_info(vdev, &single_pmk);

	if (single_pmk.pmk_info.pmk_len && single_pmk.sae_single_pmk_ap &&
	    mac_ctx->mlme_cfg->lfr.sae_single_pmk_feature_enabled) {

		rso_11i_info->pmk_len = single_pmk.pmk_info.pmk_len;
		/* Update sae same pmk info in rso */
		qdf_mem_copy(rso_11i_info->psk_pmk, single_pmk.pmk_info.pmk,
			     rso_11i_info->pmk_len);
		rso_11i_info->is_sae_same_pmk = single_pmk.sae_single_pmk_ap;

		/* get the time expired in seconds */
		time_expired = (qdf_get_system_timestamp() -
				single_pmk.pmk_info.spmk_timestamp) / 1000;

		rso_cfg->sae_offload_params.spmk_timeout = 0;
		if (time_expired < single_pmk.pmk_info.spmk_timeout_period)
			rso_cfg->sae_offload_params.spmk_timeout =
				(single_pmk.pmk_info.spmk_timeout_period  -
				 time_expired);

		sme_debug("Update spmk with len %d is_spmk_ap:%d time_exp:%lld time left:%d",
			  single_pmk.pmk_info.pmk_len,
			  single_pmk.sae_single_pmk_ap, time_expired,
			  rso_cfg->sae_offload_params.spmk_timeout);

		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return true;
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	return false;
}
#else
static inline bool
csr_cm_fill_rso_sae_single_pmk_info(struct mac_context *mac_ctx,
				    struct wlan_roam_scan_offload_params *rso_cfg,
				    uint8_t vdev_id)
{
	return false;
}
#endif
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
#define RSN_CAPS_SHIFT 16

#ifdef WLAN_ADAPTIVE_11R
static void
csr_cm_update_rso_adaptive_11r(struct wlan_rso_11r_params *dst,
			       struct csr_roam_session *session)
{
	dst->is_adaptive_11r = session->is_adaptive_11r_connection;
}
#else
static inline void
csr_cm_update_rso_adaptive_11r(struct wlan_rso_11r_params *dst,
			       struct csr_roam_session *session)
{}
#endif

#ifdef FEATURE_WLAN_ESE
static void
csr_cm_update_rso_ese_info(struct mac_context *mac,
			   struct wlan_roam_scan_offload_params *rso_config,
			   tpCsrNeighborRoamControlInfo roam_info,
			   struct csr_roam_session *session)
{
	enum csr_akm_type akm =
	   mac->roam.roamSession[session->vdev_id].connectedProfile.AuthType;

	rso_config->rso_ese_info.is_ese_assoc =
		(csr_roam_is_ese_assoc(mac, session->vdev_id) &&
		 akm == eCSR_AUTH_TYPE_OPEN_SYSTEM)  ||
		(csr_is_auth_type_ese(akm));
	rso_config->rso_11r_info.is_11r_assoc = roam_info->is11rAssoc;
	if (rso_config->rso_ese_info.is_ese_assoc) {
		qdf_mem_copy(rso_config->rso_ese_info.krk,
			     session->eseCckmInfo.krk, WMI_KRK_KEY_LEN);
		qdf_mem_copy(rso_config->rso_ese_info.btk,
			     session->eseCckmInfo.btk, WMI_BTK_KEY_LEN);
		rso_config->rso_11i_info.fw_okc = false;
		rso_config->rso_11i_info.fw_pmksa_cache = false;
		rso_config->rso_11i_info.pmk_len = 0;
		qdf_mem_zero(&rso_config->rso_11i_info.psk_pmk[0],
			     sizeof(rso_config->rso_11i_info.psk_pmk));
	}
}
#else
static inline void
csr_cm_update_rso_ese_info(struct mac_context *mac,
			   struct wlan_roam_scan_offload_params *rso_config,
			   tpCsrNeighborRoamControlInfo roam_info,
			   struct csr_roam_session *session)
{}
#endif

/**
 * csr_cm_roam_scan_offload_fill_lfr3_config  - Fill Roam scan offload
 * related configs for WMI_ROAM_SCAN_MODE command to firmware.
 * @mac: Pointer to mac context
 * @session: Pointer to csr_roam_session
 * @vdev_id: vdev_id
 *
 * Return: QDF_STATUS
 */
static QDF_STATUS csr_cm_roam_scan_offload_fill_lfr3_config(
			struct mac_context *mac,
			struct csr_roam_session *session,
			struct wlan_roam_scan_offload_params *rso_config,
			uint8_t command, uint32_t *mode)
{
	tSirMacCapabilityInfo self_caps;
	tSirMacQosInfoStation sta_qos_info;
	enum csr_akm_type akm;
	eCsrEncryptionType encr;
	uint16_t *final_caps_val;
	uint8_t *qos_cfg_val, temp_val;
	uint32_t pmkid_modes = mac->mlme_cfg->sta.pmkid_modes;
	uint32_t val = 0;
	uint16_t vdev_id = session->vdev_id;
	qdf_size_t val_len;
	QDF_STATUS status;
	uint16_t rsn_caps = 0;
	tpCsrNeighborRoamControlInfo roam_info =
		&mac->roam.neighborRoamInfo[vdev_id];

	rso_config->roam_offload_enabled =
		mac->mlme_cfg->lfr.lfr3_roaming_offload;
	if (!rso_config->roam_offload_enabled)
		return QDF_STATUS_SUCCESS;

	/* FILL LFR3 specific roam scan mode TLV parameters */
	rso_config->rso_lfr3_params.roam_rssi_cat_gap =
		mac->roam.configParam.bCatRssiOffset;
	rso_config->rso_lfr3_params.prefer_5ghz =
		(uint8_t)mac->mlme_cfg->lfr.roam_prefer_5ghz;
	rso_config->rso_lfr3_params.select_5ghz_margin =
		mac->mlme_cfg->gen.select_5ghz_margin;
	rso_config->rso_lfr3_params.reassoc_failure_timeout =
		mac->mlme_cfg->timeouts.reassoc_failure_timeout;
	rso_config->rso_lfr3_params.ho_delay_for_rx =
		mac->mlme_cfg->lfr.ho_delay_for_rx;
	rso_config->rso_lfr3_params.roam_retry_count =
		mac->mlme_cfg->lfr.roam_preauth_retry_count;
	rso_config->rso_lfr3_params.roam_preauth_no_ack_timeout =
		mac->mlme_cfg->lfr.roam_preauth_no_ack_timeout;
	rso_config->rso_lfr3_params.rct_validity_timer =
		mac->mlme_cfg->btm.rct_validity_timer;
	rso_config->rso_lfr3_params.disable_self_roam =
		!mac->mlme_cfg->lfr.enable_self_bss_roam;
	if (!roam_info->roam_control_enable &&
	    mac->mlme_cfg->lfr.roam_force_rssi_trigger)
		*mode |= WMI_ROAM_SCAN_MODE_RSSI_CHANGE;

	/* Fill LFR3 specific self capabilities for roam scan mode TLV */
	self_caps.ess = 1;
	self_caps.ibss = 0;

	val = mac->mlme_cfg->wep_params.is_privacy_enabled;
	if (val)
		self_caps.privacy = 1;

	if (mac->mlme_cfg->ht_caps.short_preamble)
		self_caps.shortPreamble = 1;

	self_caps.pbcc = 0;
	self_caps.channelAgility = 0;

	if (mac->mlme_cfg->feature_flags.enable_short_slot_time_11g)
		self_caps.shortSlotTime = 1;

	if (mac->mlme_cfg->gen.enabled_11h)
		self_caps.spectrumMgt = 1;

	if (mac->mlme_cfg->wmm_params.qos_enabled)
		self_caps.qos = 1;

	if (mac->mlme_cfg->roam_scoring.apsd_enabled)
		self_caps.apsd = 1;

	self_caps.rrm = mac->rrm.rrmConfig.rrm_enabled;

	val = mac->mlme_cfg->feature_flags.enable_block_ack;
	self_caps.delayedBA =
		(uint16_t)((val >> WNI_CFG_BLOCK_ACK_ENABLED_DELAYED) & 1);
	self_caps.immediateBA =
		(uint16_t)((val >> WNI_CFG_BLOCK_ACK_ENABLED_IMMEDIATE) & 1);
	final_caps_val = (uint16_t *)&self_caps;

	/*
	 * Self rsn caps aren't sent to firmware, so in case of PMF required,
	 * the firmware connects to a non PMF AP advertising PMF not required
	 * in the re-assoc request which violates protocol.
	 * So send self RSN caps to firmware in roam SCAN offload command to
	 * let it configure the params in the re-assoc request too.
	 * Instead of making another infra, send the RSN-CAPS in MSB of
	 * beacon Caps.
	 */
	csr_cm_roam_fill_rsn_caps(mac, vdev_id, &rsn_caps);
	rso_config->rso_lfr3_caps.capability =
		(rsn_caps << RSN_CAPS_SHIFT) | ((*final_caps_val) & 0xFFFF);

	rso_config->rso_lfr3_caps.ht_caps_info =
		*(uint16_t *)&mac->mlme_cfg->ht_caps.ht_cap_info;
	rso_config->rso_lfr3_caps.ampdu_param =
		*(uint8_t *)&mac->mlme_cfg->ht_caps.ampdu_params;
	rso_config->rso_lfr3_caps.ht_ext_cap =
		*(uint16_t *)&mac->mlme_cfg->ht_caps.ext_cap_info;

	temp_val = (uint8_t)mac->mlme_cfg->vht_caps.vht_cap_info.tx_bf_cap;
	rso_config->rso_lfr3_caps.ht_txbf = temp_val & 0xFF;
	temp_val = (uint8_t)mac->mlme_cfg->vht_caps.vht_cap_info.as_cap;
	rso_config->rso_lfr3_caps.asel_cap = temp_val & 0xFF;

	qdf_mem_zero(&sta_qos_info, sizeof(tSirMacQosInfoStation));
	sta_qos_info.maxSpLen =
		(uint8_t)mac->mlme_cfg->wmm_params.max_sp_length;
	sta_qos_info.moreDataAck = 0;
	sta_qos_info.qack = 0;
	sta_qos_info.acbe_uapsd = SIR_UAPSD_GET(ACBE, session->uapsd_mask);
	sta_qos_info.acbk_uapsd = SIR_UAPSD_GET(ACBK, session->uapsd_mask);
	sta_qos_info.acvi_uapsd = SIR_UAPSD_GET(ACVI, session->uapsd_mask);
	sta_qos_info.acvo_uapsd = SIR_UAPSD_GET(ACVO, session->uapsd_mask);
	qos_cfg_val = (uint8_t *)&sta_qos_info;
	rso_config->rso_lfr3_caps.qos_caps = (*qos_cfg_val) & 0xFF;
	if (rso_config->rso_lfr3_caps.qos_caps)
		rso_config->rso_lfr3_caps.qos_enabled = true;

	rso_config->rso_lfr3_caps.wmm_caps = 0x4;

	val_len = ROAM_OFFLOAD_NUM_MCS_SET;
	status =
	    wlan_mlme_get_cfg_str((uint8_t *)rso_config->rso_lfr3_caps.mcsset,
				  &mac->mlme_cfg->rates.supported_mcs_set,
				  &val_len);
	if (QDF_IS_STATUS_ERROR(status)) {
		mlme_err("Failed to get CFG_SUPPORTED_MCS_SET");
		return QDF_STATUS_E_FAILURE;
	}

	/* Update 11i TLV related Fields */
	rso_config->rso_11i_info.roam_key_mgmt_offload_enabled =
			mac->mlme_cfg->lfr.lfr3_roaming_offload;
	rso_config->rso_11i_info.fw_okc =
			(pmkid_modes & CFG_PMKID_MODES_OKC) ? 1 : 0;
	rso_config->rso_11i_info.fw_pmksa_cache =
			(pmkid_modes & CFG_PMKID_MODES_PMKSA_CACHING) ? 1 : 0;

	/* Check whether to send psk_pmk or sae_single pmk info */
	if (!csr_cm_fill_rso_sae_single_pmk_info(mac,
						 rso_config, vdev_id)) {
		rso_config->rso_11i_info.is_sae_same_pmk = false;
		qdf_mem_copy(rso_config->rso_11i_info.psk_pmk, session->psk_pmk,
			     sizeof(rso_config->rso_11i_info.psk_pmk));
		rso_config->rso_11i_info.pmk_len = session->pmk_len;
	}

	rso_config->rso_11r_info.enable_ft_im_roaming =
		mac->mlme_cfg->lfr.enable_ft_im_roaming;
	rso_config->rso_11r_info.mdid.mdie_present =
		session->connectedProfile.mdid.mdie_present;
	rso_config->rso_11r_info.mdid.mobility_domain =
		session->connectedProfile.mdid.mobility_domain;
	rso_config->rso_11r_info.r0kh_id_length =
			session->ftSmeContext.r0kh_id_len;
	qdf_mem_copy(rso_config->rso_11r_info.r0kh_id,
		     session->ftSmeContext.r0kh_id,
		     session->ftSmeContext.r0kh_id_len);

	qdf_mem_copy(rso_config->rso_11r_info.psk_pmk, session->psk_pmk,
		     session->pmk_len);
	rso_config->rso_11r_info.pmk_len = session->pmk_len;

	csr_cm_update_rso_adaptive_11r(&rso_config->rso_11r_info,
				       session);
	csr_cm_update_rso_ese_info(mac, rso_config, roam_info, session);

	akm = mac->roam.roamSession[vdev_id].connectedProfile.AuthType;
	encr =
	mac->roam.roamSession[vdev_id].connectedProfile.EncryptionType;
	rso_config->akm = e_csr_auth_type_to_rsn_authmode(akm, encr);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
csr_cm_roam_scan_offload_fill_lfr3_config(
			struct mac_context *mac,
			struct csr_roam_session *session,
			struct wlan_roam_scan_offload_params *rso_config,
			uint8_t command, uint32_t *mode)
{
	if (mac->mlme_cfg->lfr.roam_force_rssi_trigger)
		*mode |= WMI_ROAM_SCAN_MODE_RSSI_CHANGE;

	return QDF_STATUS_SUCCESS;
}
#endif

static void
csr_cm_roam_scan_offload_fill_scan_params(
		struct mac_context *mac,
		struct csr_roam_session *session,
		struct wlan_roam_scan_offload_params *rso_mode_cfg,
		struct wlan_roam_scan_channel_list *rso_chan_info,
		uint8_t command)
{
	struct wlan_roam_scan_params *scan_params =
			&rso_mode_cfg->rso_scan_params;
	tpCsrNeighborRoamControlInfo roam_info =
		&mac->roam.neighborRoamInfo[session->vdev_id];
	uint8_t channels_per_burst = 0;
	uint16_t roam_scan_home_away_time;
	eSirDFSRoamScanMode allow_dfs_ch_roam;

	qdf_mem_zero(scan_params, sizeof(*scan_params));
	if (command == ROAM_SCAN_OFFLOAD_STOP)
		return;

	/* Parameters updated after association is complete */
	wlan_scan_cfg_get_passive_dwelltime(mac->psoc,
					    &scan_params->dwell_time_passive);
	/*
	 * Here is the formula,
	 * T(HomeAway) = N * T(dwell) + (N+1) * T(cs)
	 * where N is number of channels scanned in single burst
	 */
	scan_params->dwell_time_active =
		roam_info->cfgParams.maxChannelScanTime;

	roam_scan_home_away_time =
		roam_info->cfgParams.roam_scan_home_away_time;
	if (roam_scan_home_away_time <
	    (scan_params->dwell_time_active +
	     (2 * ROAM_SCAN_CHANNEL_SWITCH_TIME))) {
		sme_debug("Disable Home away time(%d) as it is less than (2*RF switching time + channel max time)(%d)",
			  roam_scan_home_away_time,
			  (scan_params->dwell_time_active +
			   (2 * ROAM_SCAN_CHANNEL_SWITCH_TIME)));
		roam_scan_home_away_time = 0;
	}

	if (roam_scan_home_away_time < (2 * ROAM_SCAN_CHANNEL_SWITCH_TIME)) {
		/* clearly we can't follow home away time.
		 * Make it a split scan.
		 */
		scan_params->burst_duration = 0;
	} else {
		channels_per_burst =
		  (roam_scan_home_away_time - ROAM_SCAN_CHANNEL_SWITCH_TIME) /
		  (scan_params->dwell_time_active + ROAM_SCAN_CHANNEL_SWITCH_TIME);

		if (channels_per_burst < 1) {
			/* dwell time and home away time conflicts */
			/* we will override dwell time */
			scan_params->dwell_time_active =
				roam_scan_home_away_time -
				(2 * ROAM_SCAN_CHANNEL_SWITCH_TIME);
			scan_params->burst_duration =
				scan_params->dwell_time_active;
		} else {
			scan_params->burst_duration =
				channels_per_burst *
				scan_params->dwell_time_active;
		}
	}

	allow_dfs_ch_roam =
		(eSirDFSRoamScanMode)mac->mlme_cfg->lfr.roaming_dfs_channel;
	/* Roaming on DFS channels is supported and it is not
	 * app channel list. It is ok to override homeAwayTime
	 * to accommodate DFS dwell time in burst
	 * duration.
	 */
	if (allow_dfs_ch_roam == SIR_ROAMING_DFS_CHANNEL_ENABLED_NORMAL &&
	    roam_scan_home_away_time > 0  &&
	    rso_chan_info->chan_cache_type != CHANNEL_LIST_STATIC)
		scan_params->burst_duration =
			QDF_MAX(scan_params->burst_duration,
				scan_params->dwell_time_passive);

	scan_params->min_rest_time =
		roam_info->cfgParams.neighbor_scan_min_period;
	scan_params->max_rest_time = roam_info->cfgParams.neighborScanPeriod;
	scan_params->repeat_probe_time =
		(roam_info->cfgParams.roam_scan_n_probes > 0) ?
			QDF_MAX(scan_params->dwell_time_active /
				roam_info->cfgParams.roam_scan_n_probes, 1) : 0;
	scan_params->probe_spacing_time = 0;
	scan_params->probe_delay = 0;
	/* 30 seconds for full scan cycle */
	scan_params->max_scan_time = ROAM_SCAN_HW_DEF_SCAN_MAX_DURATION;
	scan_params->idle_time = scan_params->min_rest_time;
	scan_params->n_probes = roam_info->cfgParams.roam_scan_n_probes;

	if (allow_dfs_ch_roam == SIR_ROAMING_DFS_CHANNEL_DISABLED) {
		scan_params->scan_ctrl_flags |= WMI_SCAN_BYPASS_DFS_CHN;
	} else {
		/* Roaming scan on DFS channel is allowed.
		 * No need to change any flags for default
		 * allowDFSChannelRoam = 1.
		 * Special case where static channel list is given by\
		 * application that contains DFS channels.
		 * Assume that the application has knowledge of matching
		 * APs being active and that probe request transmission
		 * is permitted on those channel.
		 * Force active scans on those channels.
		 */

		if (allow_dfs_ch_roam ==
		    SIR_ROAMING_DFS_CHANNEL_ENABLED_ACTIVE &&
		    rso_chan_info->chan_cache_type == CHANNEL_LIST_STATIC &&
		    rso_chan_info->chan_count)
			scan_params->scan_ctrl_flags |=
				WMI_SCAN_FLAG_FORCE_ACTIVE_ON_DFS;
	}

	scan_params->rso_adaptive_dwell_mode =
		mac->mlme_cfg->lfr.adaptive_roamscan_dwell_mode;
}

/**
 * csr_cm_roam_scan_offload_fill_rso_configs  - Fill Roam scan offload related
 * configs for WMI_ROAM_SCAN_MODE command to firmware.
 * @mac: Pointer to mac context
 * @session: Pointer to csr_roam_session
 * @vdev_id: vdev_id
 */
static void csr_cm_roam_scan_offload_fill_rso_configs(
			struct mac_context *mac,
			struct csr_roam_session *session,
			struct wlan_roam_scan_offload_params *rso_mode_cfg,
			struct wlan_roam_scan_channel_list *rso_chan_info,
			uint8_t command, uint16_t reason)
{
	uint8_t vdev_id = session->vdev_id;
	tpCsrNeighborRoamControlInfo roam_info =
			&mac->roam.neighborRoamInfo[vdev_id];
	uint32_t mode = 0;

	qdf_mem_zero(rso_mode_cfg, sizeof(*rso_mode_cfg));

	rso_mode_cfg->vdev_id = session->vdev_id;
	rso_mode_cfg->is_rso_stop = (command == ROAM_SCAN_OFFLOAD_STOP);
	rso_mode_cfg->roaming_scan_policy =
		mac->mlme_cfg->lfr.roaming_scan_policy;

	/* Fill ROAM SCAN mode TLV parameters */
	if (roam_info->cfgParams.emptyScanRefreshPeriod)
		mode |= WMI_ROAM_SCAN_MODE_PERIODIC;

	rso_mode_cfg->rso_mode_info.min_delay_btw_scans =
			mac->mlme_cfg->lfr.min_delay_btw_roam_scans;
	rso_mode_cfg->rso_mode_info.min_delay_roam_trigger_bitmask =
			mac->mlme_cfg->lfr.roam_trigger_reason_bitmask;

	if (command == ROAM_SCAN_OFFLOAD_STOP) {
		if (reason == REASON_ROAM_STOP_ALL ||
		    reason == REASON_DISCONNECTED ||
		    reason == REASON_ROAM_SYNCH_FAILED) {
			mode = WMI_ROAM_SCAN_MODE_NONE;
		} else {
			if (csr_is_roam_offload_enabled(mac))
				mode = WMI_ROAM_SCAN_MODE_NONE |
					WMI_ROAM_SCAN_MODE_ROAMOFFLOAD;
			else
				mode = WMI_ROAM_SCAN_MODE_NONE;
		}
	}

	rso_mode_cfg->rso_mode_info.roam_scan_mode = mode;
	if (command == ROAM_SCAN_OFFLOAD_STOP)
		return;

	csr_cm_roam_scan_offload_fill_lfr3_config(mac, session, rso_mode_cfg,
						  command, &mode);
	rso_mode_cfg->rso_mode_info.roam_scan_mode = mode;
	csr_cm_roam_scan_offload_fill_scan_params(mac, session, rso_mode_cfg,
						  rso_chan_info,
						  command);
	csr_cm_update_driver_assoc_ies(mac, session, rso_mode_cfg);
	cm_roam_scan_offload_add_fils_params(mac->psoc, rso_mode_cfg,
					     vdev_id);
}

/**
 * csr_cm_roam_scan_filter() - set roam scan filter parameters
 * @mac_ctx: global mac ctx
 * @vdev_id: vdev id
 * @command: rso command
 * @reason:  reason to roam
 * @scan_filter_params:  roam scan filter related parameters
 *
 * There are filters such as whitelist, blacklist and preferred
 * list that need to be applied to the scan results to form the
 * probable candidates for roaming.
 *
 * Return: None
 */
static void
csr_cm_roam_scan_filter(struct mac_context *mac_ctx, uint8_t vdev_id,
			uint8_t command, uint8_t reason,
			struct wlan_roam_scan_filter_params *scan_filter_params)
{
	int i;
	uint32_t num_bssid_black_list = 0, num_ssid_white_list = 0,
	   num_bssid_preferred_list = 0,  num_rssi_rejection_ap = 0;
	uint32_t op_bitmap = 0;
	struct roam_ext_params *roam_params;
	struct roam_scan_filter_params *params;

	scan_filter_params->reason = reason;
	params = &scan_filter_params->filter_params;
	roam_params = &mac_ctx->roam.configParam.roam_params;
	/*
	 * If rssi disallow bssid list have any member
	 * fill it and send it to firmware so that firmware does not
	 * try to roam to these BSS until RSSI OR time condition are
	 * matched.
	 */
	csr_add_rssi_reject_ap_list(mac_ctx, roam_params);

	if (command != ROAM_SCAN_OFFLOAD_STOP) {
		switch (reason) {
		case REASON_ROAM_SET_BLACKLIST_BSSID:
			op_bitmap |= 0x1;
			num_bssid_black_list =
				roam_params->num_bssid_avoid_list;
			break;
		case REASON_ROAM_SET_SSID_ALLOWED:
			op_bitmap |= 0x2;
			num_ssid_white_list =
				roam_params->num_ssid_allowed_list;
			break;
		case REASON_ROAM_SET_FAVORED_BSSID:
			op_bitmap |= 0x4;
			num_bssid_preferred_list =
				roam_params->num_bssid_favored;
			break;
		case REASON_CTX_INIT:
			if (command == ROAM_SCAN_OFFLOAD_START) {
				params->lca_disallow_config_present = true;
				num_rssi_rejection_ap =
					roam_params->num_rssi_rejection_ap;
			} else {
				sme_debug("Roam Filter need not be sent, no need to fill parameters");
				return;
			}
			break;
		default:
			sme_debug("Roam Filter need not be sent, no need to fill parameters");
			return;
		}
	} else {
		/* In case of STOP command, reset all the variables
		 * except for blacklist BSSID which should be retained
		 * across connections.
		 */
		op_bitmap = 0x2 | 0x4;
		if (reason == REASON_ROAM_SET_SSID_ALLOWED)
			num_ssid_white_list =
					roam_params->num_ssid_allowed_list;
		num_bssid_preferred_list = roam_params->num_bssid_favored;
	}

	/* fill in fixed values */
	params->vdev_id = vdev_id;
	params->op_bitmap = op_bitmap;
	params->num_bssid_black_list = num_bssid_black_list;
	params->num_ssid_white_list = num_ssid_white_list;
	params->num_bssid_preferred_list = num_bssid_preferred_list;
	params->num_rssi_rejection_ap = num_rssi_rejection_ap;
	params->delta_rssi =
		wlan_blm_get_rssi_blacklist_threshold(mac_ctx->pdev);
	if (params->num_bssid_black_list)
		qdf_mem_copy(params->bssid_avoid_list,
			     roam_params->bssid_avoid_list,
			     MAX_BSSID_AVOID_LIST *
					sizeof(struct qdf_mac_addr));

	for (i = 0; i < num_ssid_white_list; i++) {
		qdf_mem_copy(params->ssid_allowed_list[i].ssid,
			     roam_params->ssid_allowed_list[i].ssId,
			     roam_params->ssid_allowed_list[i].length);
		params->ssid_allowed_list[i].length =
				roam_params->ssid_allowed_list[i].length;
		sme_debug("SSID %d: %.*s", i,
			  params->ssid_allowed_list[i].length,
			  params->ssid_allowed_list[i].ssid);
	}

	for (i = 0; i < params->num_bssid_black_list; i++)
		sme_debug("Blacklist bssid[%d]:" QDF_MAC_ADDR_FMT, i,
			  QDF_MAC_ADDR_REF(params->bssid_avoid_list[i].bytes));
	if (params->num_bssid_preferred_list) {
		qdf_mem_copy(params->bssid_favored, roam_params->bssid_favored,
			     MAX_BSSID_FAVORED * sizeof(struct qdf_mac_addr));
		qdf_mem_copy(params->bssid_favored_factor,
			     roam_params->bssid_favored_factor,
			     MAX_BSSID_FAVORED);
	}
	if (params->num_rssi_rejection_ap)
		qdf_mem_copy(params->rssi_rejection_ap,
			     roam_params->rssi_reject_bssid_list,
			     MAX_RSSI_AVOID_BSSID_LIST *
			     sizeof(struct reject_ap_config_params));

	for (i = 0; i < params->num_bssid_preferred_list; i++)
		sme_debug("Preferred Bssid[%d]:"QDF_MAC_ADDR_FMT" score: %d", i,
			  QDF_MAC_ADDR_REF(params->bssid_favored[i].bytes),
			  params->bssid_favored_factor[i]);

	if (params->lca_disallow_config_present) {
		params->disallow_duration
				= mac_ctx->mlme_cfg->lfr.lfr3_disallow_duration;
		params->rssi_channel_penalization
			= mac_ctx->mlme_cfg->lfr.lfr3_rssi_channel_penalization;
		params->num_disallowed_aps
			= mac_ctx->mlme_cfg->lfr.lfr3_num_disallowed_aps;
		sme_debug("disallow_dur %d rssi_chan_pen %d num_disallowed_aps %d",
			  params->disallow_duration,
			  params->rssi_channel_penalization,
			  params->num_disallowed_aps);
	}
}

/**
 * csr_cm_roam_scan_btm_offload() - set roam scan btm offload parameters
 * @mac_ctx: global mac ctx
 * @session: sme session
 * @params:  roam scan btm offload parameters
 *
 * This function is used to set roam scan btm offload related parameters
 *
 * Return: None
 */
static void
csr_cm_roam_scan_btm_offload(struct mac_context *mac_ctx,
			     struct csr_roam_session *session,
			     struct wlan_roam_btm_config *params)
{
	params->vdev_id = session->vdev_id;
	csr_update_btm_offload_config(mac_ctx, ROAM_SCAN_OFFLOAD_START,
				      &params->btm_offload_config, session);
	params->btm_solicited_timeout =
			mac_ctx->mlme_cfg->btm.btm_solicited_timeout;
	params->btm_max_attempt_cnt =
			mac_ctx->mlme_cfg->btm.btm_max_attempt_cnt;
	params->btm_sticky_time =
			mac_ctx->mlme_cfg->btm.btm_sticky_time;
	params->disassoc_timer_threshold =
			mac_ctx->mlme_cfg->btm.disassoc_timer_threshold;
	params->btm_query_bitmask =
			mac_ctx->mlme_cfg->btm.btm_query_bitmask;
	params->btm_candidate_min_score =
			mac_ctx->mlme_cfg->btm.btm_trig_min_candidate_score;
}

/**
 * csr_cm_roam_offload_11k_params() - set roam 11k offload parameters
 * @mac_ctx: global mac ctx
 * @session: sme session
 * @params:  roam 11k offload parameters
 * @enabled: 11k offload enabled/disabled
 *
 * This function is used to set roam 11k offload related parameters
 *
 * Return: None
 */
static void
csr_cm_roam_offload_11k_params(struct mac_context *mac_ctx,
			       struct csr_roam_session *session,
			       struct wlan_roam_11k_offload_params *params,
			       bool enabled)
{
	csr_update_11k_offload_params(mac_ctx, session, params, enabled);
}

/*
 * Below wlan_cm_roam_* and all csr_cm_roam_* APIs will move to component once
 * conenction manager is converged.
 */

QDF_STATUS
wlan_cm_roam_fill_start_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			    struct wlan_roam_start_config *req, uint8_t reason)
{
	struct csr_roam_session *session;
	struct mac_context *mac_ctx;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("session is null %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	csr_cm_roam_scan_offload_rssi_thresh(mac_ctx, session,
					     &req->rssi_params);

	csr_cm_roam_scan_offload_scan_period(mac_ctx, vdev_id,
					     &req->scan_period_params);

	csr_cm_roam_scan_offload_ap_profile(mac_ctx, session,
					    &req->profile_params);

	csr_cm_fill_rso_channel_list(mac_ctx, &req->rso_chan_info, vdev_id,
				     reason);
	csr_cm_roam_scan_filter(mac_ctx, vdev_id, ROAM_SCAN_OFFLOAD_START,
				reason, &req->scan_filter_params);

	csr_cm_roam_scan_offload_fill_rso_configs(mac_ctx, session,
						  &req->rso_config,
						  &req->rso_chan_info,
						  ROAM_SCAN_OFFLOAD_START,
						  reason);

	csr_cm_roam_scan_btm_offload(mac_ctx, session, &req->btm_config);

	/* 11k offload is enabled during RSO Start after connect indication */
	csr_cm_roam_offload_11k_params(mac_ctx, session,
				       &req->roam_11k_params, TRUE);
	/* fill other struct similar to wlan_roam_offload_scan_rssi_params */

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_cm_roam_fill_stop_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			   struct wlan_roam_stop_config *req, uint8_t reason)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac_ctx;
	struct csr_roam_session *session;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("session is null %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (reason == REASON_ROAM_SYNCH_FAILED)
		return status;
	else if (reason == REASON_DRIVER_DISABLED)
		req->reason = REASON_ROAM_STOP_ALL;
	else if (reason == REASON_SUPPLICANT_DISABLED_ROAMING)
		req->reason = REASON_SUPPLICANT_DISABLED_ROAMING;
	else if (reason == REASON_DISCONNECTED)
		req->reason = REASON_DISCONNECTED;
	else if (reason == REASON_OS_REQUESTED_ROAMING_NOW)
		req->reason = REASON_OS_REQUESTED_ROAMING_NOW;
	else
		req->reason = REASON_SME_ISSUED;

	if (csr_neighbor_middle_of_roaming(mac_ctx, vdev_id))
		req->middle_of_roaming = 1;
	else
		csr_roam_reset_roam_params(mac_ctx);

	/*
	 * Disable offload_11k_params for current vdev
	 */
	req->roam_11k_params.vdev_id = vdev_id;
	csr_cm_roam_scan_filter(mac_ctx, vdev_id, ROAM_SCAN_OFFLOAD_STOP,
				reason, &req->scan_filter_params);
	csr_cm_roam_scan_offload_fill_rso_configs(mac_ctx, session,
						  &req->rso_config,
						  NULL,
						  ROAM_SCAN_OFFLOAD_STOP,
						  req->reason);

	return status;
}

QDF_STATUS
wlan_cm_roam_fill_update_config_req(struct wlan_objmgr_psoc *psoc,
				    uint8_t vdev_id,
				    struct wlan_roam_update_config *req,
				    uint8_t reason)
{
	struct csr_roam_session *session;
	struct mac_context *mac_ctx;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  "session is null");
		return QDF_STATUS_E_FAILURE;
	}

	csr_cm_roam_scan_filter(mac_ctx, vdev_id, ROAM_SCAN_OFFLOAD_UPDATE_CFG,
				reason, &req->scan_filter_params);

	csr_cm_roam_scan_offload_rssi_thresh(mac_ctx, session,
					     &req->rssi_params);

	csr_cm_roam_scan_offload_scan_period(mac_ctx, vdev_id,
					     &req->scan_period_params);

	csr_cm_fill_rso_channel_list(mac_ctx, &req->rso_chan_info, vdev_id,
				     reason);

	csr_cm_roam_scan_offload_fill_rso_configs(mac_ctx, session,
						  &req->rso_config,
						  &req->rso_chan_info,
						  ROAM_SCAN_OFFLOAD_UPDATE_CFG,
						  reason);
	csr_cm_roam_scan_offload_ap_profile(mac_ctx, session,
					    &req->profile_params);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
wlan_cm_roam_scan_offload_rsp(uint8_t vdev_id, uint8_t reason)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg cds_msg = {0};
	struct roam_offload_scan_rsp *scan_offload_rsp;

	if (reason == REASON_OS_REQUESTED_ROAMING_NOW) {
		scan_offload_rsp = qdf_mem_malloc(sizeof(*scan_offload_rsp));
		if (!scan_offload_rsp)
			return QDF_STATUS_E_NOMEM;

		cds_msg.type = eWNI_SME_ROAM_SCAN_OFFLOAD_RSP;
		scan_offload_rsp->sessionId = vdev_id;
		scan_offload_rsp->reason = reason;
		cds_msg.bodyptr = scan_offload_rsp;
		/*
		 * Since REASSOC request is processed in
		 * Roam_Scan_Offload_Rsp post a dummy rsp msg back to
		 * SME with proper reason code.
		 */
		status = scheduler_post_message(QDF_MODULE_ID_MLME,
						QDF_MODULE_ID_SME,
						QDF_MODULE_ID_SME,
						&cds_msg);
		if (QDF_IS_STATUS_ERROR(status))
			qdf_mem_free(scan_offload_rsp);
	}

	return status;
}

QDF_STATUS
wlan_cm_roam_neighbor_proceed_with_handoff_req(uint8_t vdev_id)
{
	struct mac_context *mac_ctx;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return QDF_STATUS_E_FAILURE;
	}

	return csr_neighbor_roam_proceed_with_handoff_req(mac_ctx, vdev_id);
}

bool wlan_cm_is_sta_connected(uint8_t vdev_id)
{
	struct mac_context *mac_ctx;

	mac_ctx = sme_get_mac_context();
	if (!mac_ctx) {
		sme_err("mac_ctx is NULL");
		return false;
	}

	return CSR_IS_ROAM_JOINED(mac_ctx, vdev_id);
}

QDF_STATUS
csr_roam_offload_scan_rsp_hdlr(struct mac_context *mac,
			       struct roam_offload_scan_rsp *scanOffloadRsp)
{
	switch (scanOffloadRsp->reason) {
	case 0:
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "Rsp for Roam Scan Offload with failure status");
		break;
	case REASON_OS_REQUESTED_ROAMING_NOW:
		csr_neighbor_roam_proceed_with_handoff_req(mac,
						scanOffloadRsp->sessionId);
		break;

	default:
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
			  "Rsp for Roam Scan Offload with reason %d",
			  scanOffloadRsp->reason);
	}
	return QDF_STATUS_SUCCESS;
}
#endif

tSmeCmd *csr_get_command_buffer(struct mac_context *mac)
{
	tSmeCmd *pCmd = sme_get_command_buffer(mac);

	if (pCmd)
		mac->roam.sPendingCommands++;

	return pCmd;
}

static void csr_free_cmd_memory(struct mac_context *mac, tSmeCmd *pCommand)
{
	if (!pCommand) {
		sme_err("pCommand is NULL");
		return;
	}
	switch (pCommand->command) {
	case eSmeCommandRoam:
		csr_release_command_roam(mac, pCommand);
		break;
	case eSmeCommandWmStatusChange:
		csr_release_command_wm_status_change(mac, pCommand);
		break;
	case e_sme_command_set_hw_mode:
		csr_release_command_set_hw_mode(mac, pCommand);
	default:
		break;
	}
}

void csr_release_command_buffer(struct mac_context *mac, tSmeCmd *pCommand)
{
	if (mac->roam.sPendingCommands > 0) {
		/*
		 * All command allocated through csr_get_command_buffer
		 * need to decrement the pending count when releasing
		 */
		mac->roam.sPendingCommands--;
		csr_free_cmd_memory(mac, pCommand);
		sme_release_command(mac, pCommand);
	} else {
		sme_err("no pending commands");
		QDF_ASSERT(0);
	}
}

void csr_release_command(struct mac_context *mac_ctx, tSmeCmd *sme_cmd)
{
	struct wlan_serialization_queued_cmd_info cmd_info;
	struct wlan_serialization_command cmd;
	struct wlan_objmgr_vdev *vdev;

	if (!sme_cmd) {
		sme_err("sme_cmd is NULL");
		return;
	}
	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
			sme_cmd->vdev_id, WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("Invalid vdev");
		return;
	}
	qdf_mem_zero(&cmd_info,
			sizeof(struct wlan_serialization_queued_cmd_info));
	cmd_info.cmd_id = sme_cmd->cmd_id;
	cmd_info.req_type = WLAN_SER_CANCEL_NON_SCAN_CMD;
	cmd_info.cmd_type = csr_get_cmd_type(sme_cmd);
	cmd_info.vdev = vdev;
	qdf_mem_zero(&cmd, sizeof(struct wlan_serialization_command));
	cmd.cmd_id = cmd_info.cmd_id;
	cmd.cmd_type = cmd_info.cmd_type;
	cmd.vdev = cmd_info.vdev;
	if (wlan_serialization_is_cmd_present_in_active_queue(
				mac_ctx->psoc, &cmd)) {
		cmd_info.queue_type = WLAN_SERIALIZATION_ACTIVE_QUEUE;
		wlan_serialization_remove_cmd(&cmd_info);
	} else if (wlan_serialization_is_cmd_present_in_pending_queue(
				mac_ctx->psoc, &cmd)) {
		cmd_info.queue_type = WLAN_SERIALIZATION_PENDING_QUEUE;
		wlan_serialization_cancel_request(&cmd_info);
	} else {
		sme_debug("can't find cmd_id %d cmd_type %d", cmd_info.cmd_id,
			  cmd_info.cmd_type);
	}
	if (cmd_info.vdev)
		wlan_objmgr_vdev_release_ref(cmd_info.vdev, WLAN_LEGACY_SME_ID);
}


static enum wlan_serialization_cmd_type csr_get_roam_cmd_type(
		tSmeCmd *sme_cmd)
{
	enum wlan_serialization_cmd_type cmd_type = WLAN_SER_CMD_MAX;

	switch (sme_cmd->u.roamCmd.roamReason) {
	case eCsrForcedDisassoc:
	case eCsrForcedDeauth:
	case eCsrForcedDisassocMICFailure:
		cmd_type = WLAN_SER_CMD_VDEV_DISCONNECT;
		break;
	case eCsrHddIssued:
		if (CSR_IS_INFRA_AP(&sme_cmd->u.roamCmd.roamProfile) ||
		    CSR_IS_NDI(&sme_cmd->u.roamCmd.roamProfile))
			cmd_type = WLAN_SER_CMD_VDEV_START_BSS;
		else
			cmd_type = WLAN_SER_CMD_VDEV_CONNECT;
		break;
	case eCsrHddIssuedReassocToSameAP:
		cmd_type = WLAN_SER_CMD_HDD_ISSUE_REASSOC_SAME_AP;
		break;
	case eCsrSmeIssuedReassocToSameAP:
		cmd_type = WLAN_SER_CMD_SME_ISSUE_REASSOC_SAME_AP;
		break;
	case eCsrSmeIssuedDisassocForHandoff:
		cmd_type =
			WLAN_SER_CMD_SME_ISSUE_DISASSOC_FOR_HANDOFF;
		break;
	case eCsrSmeIssuedAssocToSimilarAP:
		cmd_type =
			WLAN_SER_CMD_SME_ISSUE_ASSOC_TO_SIMILAR_AP;
		break;
	case eCsrStopBss:
		cmd_type = WLAN_SER_CMD_VDEV_STOP_BSS;
		break;
	case eCsrSmeIssuedFTReassoc:
		cmd_type = WLAN_SER_CMD_SME_ISSUE_FT_REASSOC;
		break;
	case eCsrForcedDisassocSta:
		cmd_type = WLAN_SER_CMD_FORCE_DISASSOC_STA;
		break;
	case eCsrForcedDeauthSta:
		cmd_type = WLAN_SER_CMD_FORCE_DEAUTH_STA;
		break;
	case eCsrPerformPreauth:
		cmd_type = WLAN_SER_CMD_PERFORM_PRE_AUTH;
		break;
	default:
		break;
	}

	return cmd_type;
}

enum wlan_serialization_cmd_type csr_get_cmd_type(tSmeCmd *sme_cmd)
{
	enum wlan_serialization_cmd_type cmd_type = WLAN_SER_CMD_MAX;

	switch (sme_cmd->command) {
	case eSmeCommandRoam:
		cmd_type = csr_get_roam_cmd_type(sme_cmd);
		break;
	case eSmeCommandWmStatusChange:
		cmd_type = WLAN_SER_CMD_WM_STATUS_CHANGE;
		break;
	case eSmeCommandGetdisconnectStats:
		cmd_type =  WLAN_SER_CMD_GET_DISCONNECT_STATS;
		break;
	case eSmeCommandAddTs:
		cmd_type = WLAN_SER_CMD_ADDTS;
		break;
	case eSmeCommandDelTs:
		cmd_type = WLAN_SER_CMD_DELTS;
		break;
	case e_sme_command_set_hw_mode:
		cmd_type = WLAN_SER_CMD_SET_HW_MODE;
		break;
	case e_sme_command_nss_update:
		cmd_type = WLAN_SER_CMD_NSS_UPDATE;
		break;
	case e_sme_command_set_dual_mac_config:
		cmd_type = WLAN_SER_CMD_SET_DUAL_MAC_CONFIG;
		break;
	case e_sme_command_set_antenna_mode:
		cmd_type = WLAN_SER_CMD_SET_ANTENNA_MODE;
		break;
	default:
		break;
	}

	return cmd_type;
}

static uint32_t csr_get_monotonous_number(struct mac_context *mac_ctx)
{
	uint32_t cmd_id;
	uint32_t mask = 0x00FFFFFF, prefix = 0x0D000000;

	cmd_id = qdf_atomic_inc_return(&mac_ctx->global_cmd_id);
	cmd_id = (cmd_id & mask);
	cmd_id = (cmd_id | prefix);

	return cmd_id;
}

static void csr_fill_cmd_timeout(struct wlan_serialization_command *cmd)
{
	switch (cmd->cmd_type) {
	case WLAN_SER_CMD_VDEV_DISCONNECT:
	case WLAN_SER_CMD_WM_STATUS_CHANGE:
		cmd->cmd_timeout_duration = SME_CMD_VDEV_DISCONNECT_TIMEOUT;
		break;
	case WLAN_SER_CMD_VDEV_START_BSS:
		cmd->cmd_timeout_duration = SME_CMD_VDEV_START_BSS_TIMEOUT;
		break;
	case WLAN_SER_CMD_VDEV_STOP_BSS:
		cmd->cmd_timeout_duration = SME_CMD_STOP_BSS_CMD_TIMEOUT;
		break;
	case WLAN_SER_CMD_FORCE_DISASSOC_STA:
	case WLAN_SER_CMD_FORCE_DEAUTH_STA:
		cmd->cmd_timeout_duration = SME_CMD_PEER_DISCONNECT_TIMEOUT;
		break;
	case WLAN_SER_CMD_GET_DISCONNECT_STATS:
		cmd->cmd_timeout_duration =
					SME_CMD_GET_DISCONNECT_STATS_TIMEOUT;
		break;
	case WLAN_SER_CMD_HDD_ISSUE_REASSOC_SAME_AP:
	case WLAN_SER_CMD_SME_ISSUE_REASSOC_SAME_AP:
	case WLAN_SER_CMD_SME_ISSUE_DISASSOC_FOR_HANDOFF:
	case WLAN_SER_CMD_SME_ISSUE_ASSOC_TO_SIMILAR_AP:
	case WLAN_SER_CMD_SME_ISSUE_FT_REASSOC:
	case WLAN_SER_CMD_PERFORM_PRE_AUTH:
		cmd->cmd_timeout_duration = SME_CMD_ROAM_CMD_TIMEOUT;
		break;
	case WLAN_SER_CMD_ADDTS:
	case WLAN_SER_CMD_DELTS:
		cmd->cmd_timeout_duration = SME_CMD_ADD_DEL_TS_TIMEOUT;
		break;
	case WLAN_SER_CMD_SET_HW_MODE:
	case WLAN_SER_CMD_NSS_UPDATE:
	case WLAN_SER_CMD_SET_DUAL_MAC_CONFIG:
	case WLAN_SER_CMD_SET_ANTENNA_MODE:
		cmd->cmd_timeout_duration = SME_CMD_POLICY_MGR_CMD_TIMEOUT;
		break;
	case WLAN_SER_CMD_VDEV_CONNECT:
		/* fallthrough to use def MAX value */
	default:
		cmd->cmd_timeout_duration = SME_ACTIVE_LIST_CMD_TIMEOUT_VALUE;
		break;
	}
}

QDF_STATUS csr_set_serialization_params_to_cmd(struct mac_context *mac_ctx,
		tSmeCmd *sme_cmd, struct wlan_serialization_command *cmd,
		uint8_t high_priority)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!sme_cmd) {
		sme_err("Invalid sme_cmd");
		return status;
	}
	if (!cmd) {
		sme_err("Invalid serialization_cmd");
		return status;
	}

	/*
	 * no need to fill command id for non-scan as they will be
	 * zero always
	 */
	sme_cmd->cmd_id = csr_get_monotonous_number(mac_ctx);
	cmd->cmd_id = sme_cmd->cmd_id;

	cmd->cmd_type = csr_get_cmd_type(sme_cmd);
	if (cmd->cmd_type == WLAN_SER_CMD_MAX) {
		sme_err("serialization enum not found for sme_cmd type %d",
			sme_cmd->command);
		return status;
	}
	cmd->vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc,
				sme_cmd->vdev_id, WLAN_LEGACY_SME_ID);
	if (!cmd->vdev) {
		sme_err("vdev is NULL for vdev_id:%d", sme_cmd->vdev_id);
		return status;
	}
	cmd->umac_cmd = sme_cmd;

	csr_fill_cmd_timeout(cmd);

	cmd->source = WLAN_UMAC_COMP_MLME;
	cmd->cmd_cb = sme_ser_cmd_callback;
	cmd->is_high_priority = high_priority;
	cmd->is_blocking = true;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS csr_queue_sme_command(struct mac_context *mac_ctx, tSmeCmd *sme_cmd,
				 bool high_priority)
{
	struct wlan_serialization_command cmd;
	struct wlan_objmgr_vdev *vdev = NULL;
	enum wlan_serialization_status ser_cmd_status;
	QDF_STATUS status;

	if (!SME_IS_START(mac_ctx)) {
		sme_err("Sme in stop state");
		QDF_ASSERT(0);
		goto error;
	}

	if (CSR_IS_WAIT_FOR_KEY(mac_ctx, sme_cmd->vdev_id)) {
		if (!CSR_IS_DISCONNECT_COMMAND(sme_cmd)) {
			sme_err("Can't process cmd(%d), waiting for key",
				sme_cmd->command);
			goto error;
		}
	}

	qdf_mem_zero(&cmd, sizeof(struct wlan_serialization_command));
	status = csr_set_serialization_params_to_cmd(mac_ctx, sme_cmd,
					&cmd, high_priority);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("failed to set ser params");
		goto error;
	}

	vdev = cmd.vdev;
	ser_cmd_status = wlan_serialization_request(&cmd);

	switch (ser_cmd_status) {
	case WLAN_SER_CMD_PENDING:
	case WLAN_SER_CMD_ACTIVE:
		/* Command posted to active/pending list */
		status = QDF_STATUS_SUCCESS;
		break;
	default:
		sme_err("Failed to queue command %d with status:%d",
			  sme_cmd->command, ser_cmd_status);
		status = QDF_STATUS_E_FAILURE;
		goto error;
	}

	return status;

error:
	if (vdev)
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	csr_release_command_buffer(mac_ctx, sme_cmd);

	return QDF_STATUS_E_FAILURE;
}

QDF_STATUS csr_roam_update_config(struct mac_context *mac_ctx, uint8_t session_id,
				  uint16_t capab, uint32_t value)
{
	struct update_config *msg;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);

	sme_debug("update HT config requested");
	if (!session) {
		sme_err("Session does not exist for session id %d", session_id);
		return QDF_STATUS_E_FAILURE;
	}

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->messageType = eWNI_SME_UPDATE_CONFIG;
	msg->vdev_id = session_id;
	msg->capab = capab;
	msg->value = value;
	msg->length = sizeof(*msg);
	status = umac_send_mb_message_to_mac(msg);

	return status;
}

/* Returns whether a session is in QDF_STA_MODE...or not */
bool csr_roam_is_sta_mode(struct mac_context *mac, uint32_t sessionId)
{
	struct csr_roam_session *pSession = NULL;

	pSession = CSR_GET_SESSION(mac, sessionId);

	if (!pSession) {
		sme_err("session %d not found",	sessionId);
		return false;
	}
	if (!CSR_IS_SESSION_VALID(mac, sessionId)) {
		sme_err("Inactive session_id: %d", sessionId);
		return false;
	}
	if (eCSR_BSS_TYPE_INFRASTRUCTURE != pSession->connectedProfile.BSSType)
		return false;
	/* There is a possibility that the above check may fail,because
	 * P2P CLI also uses the same BSSType (eCSR_BSS_TYPE_INFRASTRUCTURE)
	 * when it is connected.So,we may sneak through the above check even
	 * if we are not a STA mode INFRA station. So, if we sneak through
	 * the above condition, we can use the following check if we are
	 * really in STA Mode.
	 */

	if (pSession->pCurRoamProfile) {
		if (pSession->pCurRoamProfile->csrPersona == QDF_STA_MODE)
			return true;
		else
			return false;
	}

	return false;
}

QDF_STATUS csr_handoff_request(struct mac_context *mac,
			       uint8_t sessionId,
			       tCsrHandoffRequest *pHandoffInfo)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct scheduler_msg msg = {0};

	tAniHandoffReq *pMsg;

	pMsg = qdf_mem_malloc(sizeof(tAniHandoffReq));
	if (!pMsg) {
		return QDF_STATUS_E_NOMEM;
	}
	pMsg->msgType = eWNI_SME_HANDOFF_REQ;
	pMsg->msgLen = (uint16_t) sizeof(tAniHandoffReq);
	pMsg->sessionId = sessionId;
	pMsg->ch_freq = pHandoffInfo->ch_freq;
	pMsg->handoff_src = pHandoffInfo->src;
	qdf_mem_copy(pMsg->bssid, pHandoffInfo->bssid.bytes, QDF_MAC_ADDR_SIZE);
	msg.type = eWNI_SME_HANDOFF_REQ;
	msg.bodyptr = pMsg;
	msg.reserved = 0;
	if (QDF_STATUS_SUCCESS != scheduler_post_message(QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 QDF_MODULE_ID_SME,
							 &msg)) {
		qdf_mem_free((void *)pMsg);
		status = QDF_STATUS_E_FAILURE;
	}
	return status;
}

/**
 * csr_roam_channel_change_req() - Post channel change request to LIM
 * @mac: mac context
 * @bssid: SAP bssid
 * @ch_params: channel information
 * @profile: CSR profile
 *
 * This API is primarily used to post Channel Change Req for SAP
 *
 * Return: QDF_STATUS
 */
QDF_STATUS csr_roam_channel_change_req(struct mac_context *mac,
				       struct qdf_mac_addr bssid,
				       struct ch_params *ch_params,
				       struct csr_roam_profile *profile)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirChanChangeRequest *msg;
	struct csr_roamstart_bssparams param;
	bool skip_hostapd_rate = !profile->chan_switch_hostapd_rate_enabled;

	/*
	 * while changing the channel, use basic rates given by driver
	 * and not by hostapd as there is a chance that hostapd might
	 * give us rates based on original channel which may not be
	 * suitable for new channel
	 */
	qdf_mem_zero(&param, sizeof(struct csr_roamstart_bssparams));

	status = csr_roam_get_bss_start_parms(mac, profile, &param,
					      skip_hostapd_rate);

	if (status != QDF_STATUS_SUCCESS) {
		sme_err("Failed to get bss parameters");
		return status;
	}

	msg = qdf_mem_malloc(sizeof(tSirChanChangeRequest));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->messageType = eWNI_SME_CHANNEL_CHANGE_REQ;
	msg->messageLen = sizeof(tSirChanChangeRequest);
	msg->target_chan_freq = profile->ChannelInfo.freq_list[0];
	msg->sec_ch_offset = ch_params->sec_ch_offset;
	msg->ch_width = profile->ch_params.ch_width;
	msg->dot11mode = csr_translate_to_wni_cfg_dot11_mode(mac,
					param.uCfgDot11Mode);
	if (WLAN_REG_IS_24GHZ_CH_FREQ(msg->target_chan_freq) &&
	    !mac->mlme_cfg->vht_caps.vht_cap_info.b24ghz_band &&
	    (msg->dot11mode == MLME_DOT11_MODE_11AC ||
	    msg->dot11mode == MLME_DOT11_MODE_11AC_ONLY))
		msg->dot11mode = MLME_DOT11_MODE_11N;
	msg->nw_type = param.sirNwType;
	msg->center_freq_seg_0 = ch_params->center_freq_seg0;
	msg->center_freq_seg_1 = ch_params->center_freq_seg1;
	msg->cac_duration_ms = profile->cac_duration_ms;
	msg->dfs_regdomain = profile->dfs_regdomain;
	qdf_mem_copy(msg->bssid, bssid.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(&msg->operational_rateset,
		     &param.operationalRateSet,
		     sizeof(msg->operational_rateset));
	qdf_mem_copy(&msg->extended_rateset, &param.extendedRateSet,
		     sizeof(msg->extended_rateset));

	status = umac_send_mb_message_to_mac(msg);

	return status;
}

/*
 * Post Beacon Tx Start request to LIM
 * immediately after SAP CAC WAIT is
 * completed without any RADAR indications.
 */
QDF_STATUS csr_roam_start_beacon_req(struct mac_context *mac,
				     struct qdf_mac_addr bssid,
				     uint8_t dfsCacWaitStatus)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirStartBeaconIndication *pMsg;

	pMsg = qdf_mem_malloc(sizeof(tSirStartBeaconIndication));
	if (!pMsg)
		return QDF_STATUS_E_NOMEM;

	pMsg->messageType = eWNI_SME_START_BEACON_REQ;
	pMsg->messageLen = sizeof(tSirStartBeaconIndication);
	pMsg->beaconStartStatus = dfsCacWaitStatus;
	qdf_mem_copy(pMsg->bssid, bssid.bytes, QDF_MAC_ADDR_SIZE);

	status = umac_send_mb_message_to_mac(pMsg);

	return status;
}

/*
 * csr_roam_modify_add_ies -
 * This function sends msg to modify the additional IE buffers in PE
 *
 * @mac: mac global structure
 * @pModifyIE: pointer to tSirModifyIE structure
 * @updateType: Type of buffer
 *
 *
 * Return: QDF_STATUS -  Success or failure
 */
QDF_STATUS
csr_roam_modify_add_ies(struct mac_context *mac,
			 tSirModifyIE *pModifyIE, eUpdateIEsType updateType)
{
	tpSirModifyIEsInd pModifyAddIEInd = NULL;
	uint8_t *pLocalBuffer = NULL;
	QDF_STATUS status;

	/* following buffer will be freed by consumer (PE) */
	pLocalBuffer = qdf_mem_malloc(pModifyIE->ieBufferlength);
	if (!pLocalBuffer)
		return QDF_STATUS_E_NOMEM;

	pModifyAddIEInd = qdf_mem_malloc(sizeof(tSirModifyIEsInd));
	if (!pModifyAddIEInd) {
		qdf_mem_free(pLocalBuffer);
		return QDF_STATUS_E_NOMEM;
	}

	/*copy the IE buffer */
	qdf_mem_copy(pLocalBuffer, pModifyIE->pIEBuffer,
		     pModifyIE->ieBufferlength);
	qdf_mem_zero(pModifyAddIEInd, sizeof(tSirModifyIEsInd));

	pModifyAddIEInd->msgType = eWNI_SME_MODIFY_ADDITIONAL_IES;
	pModifyAddIEInd->msgLen = sizeof(tSirModifyIEsInd);

	qdf_copy_macaddr(&pModifyAddIEInd->modifyIE.bssid, &pModifyIE->bssid);

	pModifyAddIEInd->modifyIE.vdev_id = pModifyIE->vdev_id;
	pModifyAddIEInd->modifyIE.notify = pModifyIE->notify;
	pModifyAddIEInd->modifyIE.ieID = pModifyIE->ieID;
	pModifyAddIEInd->modifyIE.ieIDLen = pModifyIE->ieIDLen;
	pModifyAddIEInd->modifyIE.pIEBuffer = pLocalBuffer;
	pModifyAddIEInd->modifyIE.ieBufferlength = pModifyIE->ieBufferlength;
	pModifyAddIEInd->modifyIE.oui_length = pModifyIE->oui_length;

	pModifyAddIEInd->updateType = updateType;

	status = umac_send_mb_message_to_mac(pModifyAddIEInd);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to send eWNI_SME_UPDATE_ADDTIONAL_IES msg status %d",
			status);
		qdf_mem_free(pLocalBuffer);
	}
	return status;
}

/*
 * csr_roam_update_add_ies -
 * This function sends msg to updates the additional IE buffers in PE
 *
 * @mac: mac global structure
 * @sessionId: SME session id
 * @bssid: BSSID
 * @additionIEBuffer: buffer containing addition IE from hostapd
 * @length: length of buffer
 * @updateType: Type of buffer
 * @append: append or replace completely
 *
 *
 * Return: QDF_STATUS -  Success or failure
 */
QDF_STATUS
csr_roam_update_add_ies(struct mac_context *mac,
			 tSirUpdateIE *pUpdateIE, eUpdateIEsType updateType)
{
	tpSirUpdateIEsInd pUpdateAddIEs = NULL;
	uint8_t *pLocalBuffer = NULL;
	QDF_STATUS status;

	if (pUpdateIE->ieBufferlength != 0) {
		/* Following buffer will be freed by consumer (PE) */
		pLocalBuffer = qdf_mem_malloc(pUpdateIE->ieBufferlength);
		if (!pLocalBuffer) {
			return QDF_STATUS_E_NOMEM;
		}
		qdf_mem_copy(pLocalBuffer, pUpdateIE->pAdditionIEBuffer,
			     pUpdateIE->ieBufferlength);
	}

	pUpdateAddIEs = qdf_mem_malloc(sizeof(tSirUpdateIEsInd));
	if (!pUpdateAddIEs) {
		qdf_mem_free(pLocalBuffer);
		return QDF_STATUS_E_NOMEM;
	}

	pUpdateAddIEs->msgType = eWNI_SME_UPDATE_ADDITIONAL_IES;
	pUpdateAddIEs->msgLen = sizeof(tSirUpdateIEsInd);

	qdf_copy_macaddr(&pUpdateAddIEs->updateIE.bssid, &pUpdateIE->bssid);

	pUpdateAddIEs->updateIE.vdev_id = pUpdateIE->vdev_id;
	pUpdateAddIEs->updateIE.append = pUpdateIE->append;
	pUpdateAddIEs->updateIE.notify = pUpdateIE->notify;
	pUpdateAddIEs->updateIE.ieBufferlength = pUpdateIE->ieBufferlength;
	pUpdateAddIEs->updateIE.pAdditionIEBuffer = pLocalBuffer;

	pUpdateAddIEs->updateType = updateType;

	status = umac_send_mb_message_to_mac(pUpdateAddIEs);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("Failed to send eWNI_SME_UPDATE_ADDTIONAL_IES msg status %d",
			status);
		qdf_mem_free(pLocalBuffer);
	}
	return status;
}

/**
 * csr_send_ext_change_channel()- function to post send ECSA
 * action frame to lim.
 * @mac_ctx: pointer to global mac structure
 * @channel: new channel to switch
 * @session_id: senssion it should be sent on.
 *
 * This function is called to post ECSA frame to lim.
 *
 * Return: success if msg posted to LIM else return failure
 */
QDF_STATUS csr_send_ext_change_channel(struct mac_context *mac_ctx, uint32_t channel,
					uint8_t session_id)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct sir_sme_ext_cng_chan_req *msg;

	msg = qdf_mem_malloc(sizeof(*msg));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->message_type = eWNI_SME_EXT_CHANGE_CHANNEL;
	msg->length = sizeof(*msg);
	msg->new_channel = channel;
	msg->vdev_id = session_id;
	status = umac_send_mb_message_to_mac(msg);
	return status;
}

QDF_STATUS csr_csa_restart(struct mac_context *mac_ctx, uint8_t session_id)
{
	QDF_STATUS status;
	struct scheduler_msg message = {0};

	/* Serialize the req through MC thread */
	message.bodyval = session_id;
	message.type    = eWNI_SME_CSA_RESTART_REQ;
	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("scheduler_post_msg failed!(err=%d)", status);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

/**
 * csr_roam_send_chan_sw_ie_request() - Request to transmit CSA IE
 * @mac_ctx:        Global MAC context
 * @bssid:          BSSID
 * @target_chan_freq: Channel frequency on which to send the IE
 * @csa_ie_reqd:    Include/Exclude CSA IE.
 * @ch_params:  operating Channel related information
 *
 * This function sends request to transmit channel switch announcement
 * IE to lower layers
 *
 * Return: success or failure
 **/
QDF_STATUS csr_roam_send_chan_sw_ie_request(struct mac_context *mac_ctx,
					    struct qdf_mac_addr bssid,
					    uint32_t target_chan_freq,
					    uint8_t csa_ie_reqd,
					    struct ch_params *ch_params)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tSirDfsCsaIeRequest *msg;

	msg = qdf_mem_malloc(sizeof(tSirDfsCsaIeRequest));
	if (!msg)
		return QDF_STATUS_E_NOMEM;

	msg->msgType = eWNI_SME_DFS_BEACON_CHAN_SW_IE_REQ;
	msg->msgLen = sizeof(tSirDfsCsaIeRequest);

	msg->target_chan_freq = target_chan_freq;
	msg->csaIeRequired = csa_ie_reqd;
	msg->ch_switch_beacon_cnt =
		 mac_ctx->sap.SapDfsInfo.sap_ch_switch_beacon_cnt;
	msg->ch_switch_mode = mac_ctx->sap.SapDfsInfo.sap_ch_switch_mode;
	msg->dfs_ch_switch_disable =
		mac_ctx->sap.SapDfsInfo.disable_dfs_ch_switch;
	qdf_mem_copy(msg->bssid, bssid.bytes, QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(&msg->ch_params, ch_params, sizeof(struct ch_params));

	status = umac_send_mb_message_to_mac(msg);

	return status;
}

QDF_STATUS csr_sta_continue_csa(struct mac_context *mac_ctx, uint8_t vdev_id)
{
	QDF_STATUS status;
	struct scheduler_msg message = {0};

	/* Serialize the req through MC thread */
	message.bodyval = vdev_id;
	message.type    = eWNI_SME_STA_CSA_CONTINUE_REQ;
	status = scheduler_post_message(QDF_MODULE_ID_SME, QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &message);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("eWNI_SME_STA_CSA_CONTINUE_REQ failed!(err=%d)",
			status);
		status = QDF_STATUS_E_FAILURE;
	}

	return status;
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
/**
 * csr_roaming_report_diag_event() - Diag events for LFR3
 * @mac_ctx:              MAC context
 * @roam_synch_ind_ptr:   Roam Synch Indication Pointer
 * @reason:               Reason for this event to happen
 *
 * The major events in the host for LFR3 roaming such as
 * roam synch indication, roam synch completion and
 * roam synch handoff fail will be indicated to the
 * diag framework using this API.
 *
 * Return: None
 */
void csr_roaming_report_diag_event(struct mac_context *mac_ctx,
		struct roam_offload_synch_ind *roam_synch_ind_ptr,
		enum csr_diagwlan_status_eventreason reason)
{
	WLAN_HOST_DIAG_EVENT_DEF(roam_connection,
		host_event_wlan_status_payload_type);
	qdf_mem_zero(&roam_connection,
		sizeof(host_event_wlan_status_payload_type));
	switch (reason) {
	case eCSR_REASON_ROAM_SYNCH_IND:
		roam_connection.eventId = eCSR_WLAN_STATUS_CONNECT;
		if (roam_synch_ind_ptr) {
			roam_connection.rssi = roam_synch_ind_ptr->rssi;
			roam_connection.channel =
				cds_freq_to_chan(roam_synch_ind_ptr->chan_freq);
		}
		break;
	case eCSR_REASON_ROAM_SYNCH_CNF:
		roam_connection.eventId = eCSR_WLAN_STATUS_CONNECT;
		break;
	case eCSR_REASON_ROAM_HO_FAIL:
		roam_connection.eventId = eCSR_WLAN_STATUS_DISCONNECT;
		break;
	default:
		sme_err("LFR3: Unsupported reason %d", reason);
		return;
	}
	roam_connection.reason = reason;
	WLAN_HOST_DIAG_EVENT_REPORT(&roam_connection, EVENT_WLAN_STATUS_V2);
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
void csr_process_ho_fail_ind(struct mac_context *mac_ctx, void *msg_buf)
{
	struct handoff_failure_ind *pSmeHOFailInd = msg_buf;
	struct mlme_roam_after_data_stall *vdev_roam_params;
	struct wlan_objmgr_vdev *vdev;
	struct reject_ap_info ap_info;
	uint32_t sessionId;

	if (!pSmeHOFailInd) {
		sme_err("LFR3: Hand-Off Failure Ind is NULL");
		return;
	}

	sessionId = pSmeHOFailInd->vdev_id;
	ap_info.bssid = pSmeHOFailInd->bssid;
	ap_info.reject_ap_type = DRIVER_AVOID_TYPE;
	ap_info.reject_reason = REASON_ROAM_HO_FAILURE;
	ap_info.source = ADDED_BY_DRIVER;
	wlan_blm_add_bssid_to_reject_list(mac_ctx->pdev, &ap_info);

	csr_roam_invoke_timer_stop(mac_ctx, sessionId);

	/* Roaming is supported only on Infra STA Mode. */
	if (!csr_roam_is_sta_mode(mac_ctx, sessionId)) {
		sme_err("LFR3:HO Fail cannot be handled for session %d",
			sessionId);
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, sessionId,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		sme_err("LFR3: vdev is NULL");
		return;
	}

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);
	if (vdev_roam_params)
		vdev_roam_params->roam_invoke_in_progress = false;
	else
		sme_err("LFR3: Vdev roam params is NULL");

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	mac_ctx->sme.set_connection_info_cb(false);
	csr_roam_roaming_offload_timer_action(mac_ctx, 0, sessionId,
			ROAMING_OFFLOAD_TIMER_STOP);
	csr_roam_call_callback(mac_ctx, sessionId, NULL, 0,
			eCSR_ROAM_NAPI_OFF, eCSR_ROAM_RESULT_FAILURE);
	csr_roam_synch_clean_up(mac_ctx, sessionId);
	csr_roaming_report_diag_event(mac_ctx, NULL,
			eCSR_REASON_ROAM_HO_FAIL);
	csr_roam_disconnect(mac_ctx, sessionId,
			eCSR_DISCONNECT_REASON_ROAM_HO_FAIL,
			REASON_FW_TRIGGERED_ROAM_FAILURE);
	if (mac_ctx->mlme_cfg->gen.fatal_event_trigger)
		cds_flush_logs(WLAN_LOG_TYPE_FATAL,
				WLAN_LOG_INDICATOR_HOST_DRIVER,
				WLAN_LOG_REASON_ROAM_HO_FAILURE,
				false, false);
}
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

/**
 * csr_update_op_class_array() - update op class for each band
 * @mac_ctx:          mac global context
 * @op_classes:       out param, operating class array to update
 * @channel_info:     channel info
 * @ch_name:          channel band name to display in debug messages
 * @i:                out param, stores number of operating classes
 *
 * Return: void
 */
static void
csr_update_op_class_array(struct mac_context *mac_ctx,
			  uint8_t *op_classes,
			  struct csr_channel *channel_info,
			  char *ch_name,
			  uint8_t *i)
{
	uint8_t j = 0, idx = 0, class = 0;
	bool found = false;
	uint8_t num_channels = channel_info->numChannels;
	uint8_t ch_num;

	sme_debug("Num %s channels,  %d", ch_name, num_channels);

	for (idx = 0; idx < num_channels &&
		*i < (REG_MAX_SUPP_OPER_CLASSES - 1); idx++) {
		wlan_reg_freq_to_chan_op_class(
			mac_ctx->pdev, channel_info->channel_freq_list[idx],
			true, BIT(BEHAV_NONE), &class, &ch_num);

		found = false;
		for (j = 0; j < REG_MAX_SUPP_OPER_CLASSES - 1; j++) {
			if (op_classes[j] == class) {
				found = true;
				break;
			}
		}

		if (!found) {
			op_classes[*i] = class;
			*i = *i + 1;
		}
	}
}

/**
 * csr_init_operating_classes() - update op class for all bands
 * @mac: pointer to mac context.
 *
 * Return: void
 */
static void csr_init_operating_classes(struct mac_context *mac)
{
	uint8_t i = 0;
	uint8_t j = 0;
	uint8_t swap = 0;
	uint8_t numClasses = 0;
	uint8_t opClasses[REG_MAX_SUPP_OPER_CLASSES] = {0,};

	sme_debug("Current Country = %c%c",
		  mac->scan.countryCodeCurrent[0],
		  mac->scan.countryCodeCurrent[1]);

	csr_update_op_class_array(mac, opClasses,
				  &mac->scan.base_channels, "20MHz", &i);
	numClasses = i;

	/* As per spec the operating classes should be in ascending order.
	 * Bubble sort is fine since we don't have many classes
	 */
	for (i = 0; i < (numClasses - 1); i++) {
		for (j = 0; j < (numClasses - i - 1); j++) {
			/* For decreasing order use < */
			if (opClasses[j] > opClasses[j + 1]) {
				swap = opClasses[j];
				opClasses[j] = opClasses[j + 1];
				opClasses[j + 1] = swap;
			}
		}
	}

	/* Set the ordered list of op classes in regdomain
	 * for use by other modules
	 */
	wlan_reg_dmn_set_curr_opclasses(numClasses, &opClasses[0]);
}

/**
 * csr_find_session_by_type() - This function will find given session type from
 * all sessions.
 * @mac_ctx: pointer to mac context.
 * @type:    session type
 *
 * Return: session id for give session type.
 **/
static uint32_t
csr_find_session_by_type(struct mac_context *mac_ctx, enum QDF_OPMODE type)
{
	uint32_t i, session_id = WLAN_UMAC_VDEV_ID_MAX;
	struct csr_roam_session *session_ptr;

	for (i = 0; i < WLAN_MAX_VDEVS; i++) {
		if (!CSR_IS_SESSION_VALID(mac_ctx, i))
			continue;

		session_ptr = CSR_GET_SESSION(mac_ctx, i);
		if (type == session_ptr->bssParams.bssPersona) {
			session_id = i;
			break;
		}
	}
	return session_id;
}

/**
 * csr_is_conn_allow_2g_band() - This function will check if station's conn
 * is allowed in 2.4Ghz band.
 * @mac_ctx: pointer to mac context.
 * @chnl: station's channel.
 *
 * This function will check if station's connection is allowed in 5Ghz band
 * after comparing it with SAP's operating channel. If SAP's operating
 * channel and Station's channel is different than this function will return
 * false else true.
 *
 * Return: true or false.
 **/
static bool csr_is_conn_allow_2g_band(struct mac_context *mac_ctx,
				      uint32_t ch_freq)
{
	uint32_t sap_session_id;
	struct csr_roam_session *sap_session;

	if (0 == ch_freq) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("channel is zero, connection not allowed"));

		return false;
	}

	sap_session_id = csr_find_session_by_type(mac_ctx, QDF_SAP_MODE);
	if (WLAN_UMAC_VDEV_ID_MAX != sap_session_id) {
		sap_session = CSR_GET_SESSION(mac_ctx, sap_session_id);
		if (0 != sap_session->bssParams.operation_chan_freq &&
		    sap_session->bssParams.operation_chan_freq != ch_freq) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"Can't allow STA to connect, chnls not same");
			return false;
		}
	}
	return true;
}

/**
 * csr_is_conn_allow_5g_band() - This function will check if station's conn
 * is allowed in 5Ghz band.
 * @mac_ctx: pointer to mac context.
 * @chnl: station's channel.
 *
 * This function will check if station's connection is allowed in 5Ghz band
 * after comparing it with P2PGO's operating channel. If P2PGO's operating
 * channel and Station's channel is different than this function will return
 * false else true.
 *
 * Return: true or false.
 **/
static bool csr_is_conn_allow_5g_band(struct mac_context *mac_ctx,
				      uint32_t ch_freq)
{
	uint32_t p2pgo_session_id;
	struct csr_roam_session *p2pgo_session;

	if (0 == ch_freq) {
		QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
			  FL("channel is zero, connection not allowed"));
		return false;
	}

	p2pgo_session_id = csr_find_session_by_type(mac_ctx, QDF_P2P_GO_MODE);
	if (WLAN_UMAC_VDEV_ID_MAX != p2pgo_session_id) {
		p2pgo_session = CSR_GET_SESSION(mac_ctx, p2pgo_session_id);
		if (0 != p2pgo_session->bssParams.operation_chan_freq &&
		    eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED !=
		    p2pgo_session->connectState &&
		    p2pgo_session->bssParams.operation_chan_freq != ch_freq) {
			QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				"Can't allow STA to connect, chnls not same");
			return false;
		}
	}
	return true;
}

/**
 * csr_process_set_hw_mode() - Set HW mode command to PE
 * @mac: Globacl MAC pointer
 * @command: Command received from SME
 *
 * Posts the set HW mode command to PE. This message passing
 * through PE is required for PE's internal management
 *
 * Return: None
 */
void csr_process_set_hw_mode(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct s_sir_set_hw_mode *cmd = NULL;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_set_hw_mode_resp *param;
	enum policy_mgr_hw_mode_change hw_mode;
	enum policy_mgr_conc_next_action action;
	enum set_hw_mode_status hw_mode_change_status =
						SET_HW_MODE_STATUS_ECANCELED;

	/* Setting HW mode is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set HW mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd)
		/* Probably the fail response will also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;

	action = command->u.set_hw_mode_cmd.action;
	/* For hidden SSID case, if there is any scan command pending
	 * it needs to be cleared before issuing set HW mode
	 */
	if (command->u.set_hw_mode_cmd.reason ==
		POLICY_MGR_UPDATE_REASON_HIDDEN_STA) {
		sme_err("clear any pending scan command");
		status = csr_scan_abort_mac_scan(mac,
			command->u.set_hw_mode_cmd.session_id, INVAL_SCAN_ID);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			sme_err("Failed to clear scan cmd");
			goto fail;
		}
	}

	status = policy_mgr_validate_dbs_switch(mac->psoc, action);

	if (QDF_IS_STATUS_ERROR(status)) {
		sme_debug("Hw mode change not sent to FW status = %d", status);
		if (status == QDF_STATUS_E_ALREADY)
			hw_mode_change_status = SET_HW_MODE_STATUS_ALREADY;
		goto fail;
	}

	hw_mode = policy_mgr_get_hw_mode_change_from_hw_mode_index(
			mac->psoc, command->u.set_hw_mode_cmd.hw_mode_index);

	if (POLICY_MGR_HW_MODE_NOT_IN_PROGRESS == hw_mode) {
		sme_err("hw_mode %d, failing", hw_mode);
		goto fail;
	}

	policy_mgr_set_hw_mode_change_in_progress(mac->psoc, hw_mode);

	if ((POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC ==
	     command->u.set_hw_mode_cmd.reason) &&
	    (true == mac->sme.get_connection_info_cb(NULL, NULL))) {
		sme_err("Set HW mode refused: conn in progress");
		policy_mgr_restart_opportunistic_timer(mac->psoc, false);
		goto reset_state;
	}

	if ((POLICY_MGR_UPDATE_REASON_OPPORTUNISTIC ==
	     command->u.set_hw_mode_cmd.reason) &&
	    (!command->u.set_hw_mode_cmd.hw_mode_index &&
	     !policy_mgr_need_opportunistic_upgrade(mac->psoc, NULL))) {
		sme_err("Set HW mode to SMM not needed anymore");
		goto reset_state;
	}

	cmd->messageType = eWNI_SME_SET_HW_MODE_REQ;
	cmd->length = len;
	cmd->set_hw.hw_mode_index = command->u.set_hw_mode_cmd.hw_mode_index;
	cmd->set_hw.reason = command->u.set_hw_mode_cmd.reason;
	/*
	 * Below callback and context info are not needed for PE as of now.
	 * Storing the passed value in the same s_sir_set_hw_mode format.
	 */
	cmd->set_hw.set_hw_mode_cb = command->u.set_hw_mode_cmd.set_hw_mode_cb;

	sme_debug(
		"Posting set hw mode req to PE session:%d reason:%d",
		command->u.set_hw_mode_cmd.session_id,
		command->u.set_hw_mode_cmd.reason);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("Posting to PE failed");
		cmd = NULL;
		goto reset_state;
	}
	return;

reset_state:
	policy_mgr_set_hw_mode_change_in_progress(mac->psoc,
			POLICY_MGR_HW_MODE_NOT_IN_PROGRESS);
fail:
	if (cmd)
		qdf_mem_free(cmd);
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_debug("Sending set HW fail response to SME");
	param->status = hw_mode_change_status;
	param->cfgd_hw_mode_index = 0;
	param->num_vdev_mac_entries = 0;
	msg.type = eWNI_SME_SET_HW_MODE_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_set_dual_mac_config() - Set HW mode command to PE
 * @mac: Global MAC pointer
 * @command: Command received from SME
 *
 * Posts the set dual mac config command to PE.
 *
 * Return: None
 */
void csr_process_set_dual_mac_config(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_set_dual_mac_cfg *cmd;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_dual_mac_config_resp *param;

	/* Setting MAC configuration is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set HW mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd)
		/* Probably the fail response will also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;

	cmd->message_type = eWNI_SME_SET_DUAL_MAC_CFG_REQ;
	cmd->length = len;
	cmd->set_dual_mac.scan_config = command->u.set_dual_mac_cmd.scan_config;
	cmd->set_dual_mac.fw_mode_config =
		command->u.set_dual_mac_cmd.fw_mode_config;
	/*
	 * Below callback and context info are not needed for PE as of now.
	 * Storing the passed value in the same sir_set_dual_mac_cfg format.
	 */
	cmd->set_dual_mac.set_dual_mac_cb =
		command->u.set_dual_mac_cmd.set_dual_mac_cb;

	sme_debug("Posting eWNI_SME_SET_DUAL_MAC_CFG_REQ to PE: %x %x",
		  cmd->set_dual_mac.scan_config,
		  cmd->set_dual_mac.fw_mode_config);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Posting to PE failed");
		goto fail;
	}
	return;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_err("Sending set dual mac fail response to SME");
	param->status = SET_HW_MODE_STATUS_ECANCELED;
	msg.type = eWNI_SME_SET_DUAL_MAC_CFG_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_set_antenna_mode() - Set antenna mode command to
 * PE
 * @mac: Global MAC pointer
 * @command: Command received from SME
 *
 * Posts the set dual mac config command to PE.
 *
 * Return: None
 */
void csr_process_set_antenna_mode(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_set_antenna_mode *cmd;
	QDF_STATUS status;
	struct scheduler_msg msg = {0};
	struct sir_antenna_mode_resp *param;

	/* Setting MAC configuration is for the entire system.
	 * So, no need to check session
	 */

	if (!command) {
		sme_err("Set antenna mode param is NULL");
		goto fail;
	}

	len = sizeof(*cmd);
	cmd = qdf_mem_malloc(len);
	if (!cmd)
		goto fail;

	cmd->message_type = eWNI_SME_SET_ANTENNA_MODE_REQ;
	cmd->length = len;
	cmd->set_antenna_mode = command->u.set_antenna_mode_cmd;

	sme_debug(
		"Posting eWNI_SME_SET_ANTENNA_MODE_REQ to PE: %d %d",
		cmd->set_antenna_mode.num_rx_chains,
		cmd->set_antenna_mode.num_tx_chains);

	status = umac_send_mb_message_to_mac(cmd);
	if (QDF_STATUS_SUCCESS != status) {
		sme_err("Posting to PE failed");
		/*
		 * umac_send_mb_message_to_mac would've released the mem
		 * allocated by cmd.
		 */
		goto fail;
	}

	return;
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_err("Sending set dual mac fail response to SME");
	param->status = SET_ANTENNA_MODE_STATUS_ECANCELED;
	msg.type = eWNI_SME_SET_ANTENNA_MODE_RESP;
	msg.bodyptr = param;
	msg.bodyval = 0;
	sys_process_mmh_msg(mac, &msg);
}

/**
 * csr_process_nss_update_req() - Update nss command to PE
 * @mac: Globacl MAC pointer
 * @command: Command received from SME
 *
 * Posts the nss update command to PE. This message passing
 * through PE is required for PE's internal management
 *
 * Return: None
 */
void csr_process_nss_update_req(struct mac_context *mac, tSmeCmd *command)
{
	uint32_t len;
	struct sir_nss_update_request *msg;
	QDF_STATUS status;
	struct scheduler_msg msg_return = {0};
	struct sir_bcn_update_rsp *param;
	struct csr_roam_session *session;


	if (!CSR_IS_SESSION_VALID(mac, command->vdev_id)) {
		sme_err("Invalid session id %d", command->vdev_id);
		goto fail;
	}
	session = CSR_GET_SESSION(mac, command->vdev_id);

	len = sizeof(*msg);
	msg = qdf_mem_malloc(len);
	if (!msg)
		/* Probably the fail response is also fail during malloc.
		 * Still proceeding to send response!
		 */
		goto fail;

	msg->msgType = eWNI_SME_NSS_UPDATE_REQ;
	msg->msgLen = sizeof(*msg);

	msg->new_nss = command->u.nss_update_cmd.new_nss;
	msg->ch_width = command->u.nss_update_cmd.ch_width;
	msg->vdev_id = command->u.nss_update_cmd.session_id;

	sme_debug("Posting eWNI_SME_NSS_UPDATE_REQ to PE");

	status = umac_send_mb_message_to_mac(msg);
	if (QDF_IS_STATUS_SUCCESS(status))
		return;

	sme_err("Posting to PE failed");
fail:
	param = qdf_mem_malloc(sizeof(*param));
	if (!param)
		return;

	sme_err("Sending nss update fail response to SME");
	param->status = QDF_STATUS_E_FAILURE;
	param->vdev_id = command->u.nss_update_cmd.session_id;
	param->reason = REASON_NSS_UPDATE;
	msg_return.type = eWNI_SME_NSS_UPDATE_RSP;
	msg_return.bodyptr = param;
	msg_return.bodyval = 0;
	sys_process_mmh_msg(mac, &msg_return);
}
#ifdef FEATURE_WLAN_TDLS
/**
 * csr_roam_fill_tdls_info() - Fill TDLS information
 * @roam_info: Roaming information buffer
 * @join_rsp: Join response which has TDLS info
 *
 * Return: None
 */
void csr_roam_fill_tdls_info(struct mac_context *mac_ctx,
			     struct csr_roam_info *roam_info,
			     struct join_rsp *join_rsp)
{
	roam_info->tdls_prohibited = join_rsp->tdls_prohibited;
	roam_info->tdls_chan_swit_prohibited =
		join_rsp->tdls_chan_swit_prohibited;
	sme_debug(
		"tdls:prohibit: %d, chan_swit_prohibit: %d",
		roam_info->tdls_prohibited,
		roam_info->tdls_chan_swit_prohibited);
}
#endif

#if defined(WLAN_FEATURE_FILS_SK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
static void csr_copy_fils_join_rsp_roam_info(struct csr_roam_info *roam_info,
				      struct roam_offload_synch_ind *roam_synch_data)
{
	struct fils_join_rsp_params *roam_fils_info;

	roam_info->fils_join_rsp = qdf_mem_malloc(sizeof(*roam_fils_info));
	if (!roam_info->fils_join_rsp)
		return;

	roam_fils_info = roam_info->fils_join_rsp;
	cds_copy_hlp_info(&roam_synch_data->dst_mac,
			&roam_synch_data->src_mac,
			roam_synch_data->hlp_data_len,
			roam_synch_data->hlp_data,
			&roam_fils_info->dst_mac,
			&roam_fils_info->src_mac,
			&roam_fils_info->hlp_data_len,
			roam_fils_info->hlp_data);
}

/*
 * csr_update_fils_erp_seq_num() - Update the FILS erp sequence number in
 * roaming profile after roam complete
 * @roam_info: roam_info pointer
 * @erp_next_seq_num: next erp sequence number from firmware
 *
 * Return: NONE
 */
static
void csr_update_fils_erp_seq_num(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id,
				 struct csr_roam_profile *roam_profile,
				 uint16_t erp_next_seq_num)
{
	struct wlan_fils_connection_info *fils_info;

	if (roam_profile->fils_con_info)
		roam_profile->fils_con_info->erp_sequence_number =
				erp_next_seq_num;

	fils_info = wlan_cm_get_fils_connection_info(psoc, vdev_id);
	if (fils_info)
		fils_info->erp_sequence_number = erp_next_seq_num;
}
#else
static inline
void csr_copy_fils_join_rsp_roam_info(struct csr_roam_info *roam_info,
				      struct roam_offload_synch_ind *roam_synch_data)
{}

static inline
void csr_update_fils_erp_seq_num(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id,
				 struct csr_roam_profile *roam_profile,
				 uint16_t erp_next_seq_num)
{}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
QDF_STATUS csr_fast_reassoc(mac_handle_t mac_handle,
			    struct csr_roam_profile *profile,
			    const tSirMacAddr bssid, uint32_t ch_freq,
			    uint8_t vdev_id, const tSirMacAddr connected_bssid)
{
	QDF_STATUS status;
	struct wma_roam_invoke_cmd *fastreassoc;
	struct scheduler_msg msg = {0};
	struct mac_context *mac_ctx = MAC_CONTEXT(mac_handle);
	struct csr_roam_session *session;
	struct wlan_objmgr_vdev *vdev;
	struct mlme_roam_after_data_stall *vdev_roam_params;
	bool roam_control_bitmap;

	session = CSR_GET_SESSION(mac_ctx, vdev_id);
	if (!session) {
		sme_err("session %d not found", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	if (!csr_is_conn_state_connected(mac_ctx, vdev_id)) {
		sme_debug("Not in connected state, Roam Invoke not sent");
		return QDF_STATUS_E_FAILURE;
	}

	roam_control_bitmap = mlme_get_operations_bitmap(mac_ctx->psoc,
							 vdev_id);
	if (roam_control_bitmap ||
	    !MLME_IS_ROAM_INITIALIZED(mac_ctx->psoc, vdev_id)) {
		sme_debug("ROAM: RSO Disabled internaly: vdev[%d] bitmap[0x%x]",
			  vdev_id, roam_control_bitmap);
		return QDF_STATUS_E_FAILURE;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);

	if (!vdev) {
		sme_err("vdev is NULL, aborting roam invoke");
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);

	if (!vdev_roam_params) {
		sme_err("Invalid vdev roam params, aborting roam invoke");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_NULL_VALUE;
	}

	if (vdev_roam_params->roam_invoke_in_progress) {
		sme_debug("Roaming already initiated by %d source",
			  vdev_roam_params->source);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_FAILURE;
	}

	fastreassoc = qdf_mem_malloc(sizeof(*fastreassoc));
	if (!fastreassoc) {
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_NOMEM;
	}
	/* if both are same then set the flag */
	if (!qdf_mem_cmp(connected_bssid, bssid, ETH_ALEN))
		fastreassoc->is_same_bssid = true;

	fastreassoc->vdev_id = vdev_id;
	fastreassoc->bssid[0] = bssid[0];
	fastreassoc->bssid[1] = bssid[1];
	fastreassoc->bssid[2] = bssid[2];
	fastreassoc->bssid[3] = bssid[3];
	fastreassoc->bssid[4] = bssid[4];
	fastreassoc->bssid[5] = bssid[5];

	status = sme_get_beacon_frm(mac_handle, profile, bssid,
				    &fastreassoc->frame_buf,
				    &fastreassoc->frame_len,
				    &ch_freq, vdev_id);

	if (!ch_freq) {
		sme_err("channel retrieval from BSS desc fails!");
		qdf_mem_free(fastreassoc->frame_buf);
		fastreassoc->frame_buf = NULL;
		fastreassoc->frame_len = 0;
		qdf_mem_free(fastreassoc);
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return QDF_STATUS_E_FAULT;
	}

	fastreassoc->ch_freq = ch_freq;
	if (QDF_STATUS_SUCCESS != status) {
		sme_warn("sme_get_beacon_frm failed");
		qdf_mem_free(fastreassoc->frame_buf);
		fastreassoc->frame_buf = NULL;
		fastreassoc->frame_len = 0;
	}

	if (csr_is_auth_type_ese(mac_ctx->roam.roamSession[vdev_id].
				connectedProfile.AuthType)) {
		sme_debug("Beacon is not required for ESE");
		if (fastreassoc->frame_len) {
			qdf_mem_free(fastreassoc->frame_buf);
			fastreassoc->frame_buf = NULL;
			fastreassoc->frame_len = 0;
		}
	}

	msg.type = eWNI_SME_ROAM_INVOKE;
	msg.reserved = 0;
	msg.bodyptr = fastreassoc;
	status = scheduler_post_message(QDF_MODULE_ID_SME,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		sme_err("Not able to post ROAM_INVOKE_CMD message to PE");
		qdf_mem_free(fastreassoc->frame_buf);
		fastreassoc->frame_buf = NULL;
		fastreassoc->frame_len = 0;
		qdf_mem_free(fastreassoc);
	} else {
		vdev_roam_params->roam_invoke_in_progress = true;
		vdev_roam_params->source = USERSPACE_INITIATED;
		session->roam_invoke_timer_info.mac = mac_ctx;
		session->roam_invoke_timer_info.vdev_id = vdev_id;
		qdf_mc_timer_start(&session->roam_invoke_timer,
				   QDF_ROAM_INVOKE_TIMEOUT);
	}

	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	return status;
}
#if defined(WLAN_SAE_SINGLE_PMK) && defined(WLAN_FEATURE_ROAM_OFFLOAD)
/**
 * csr_is_sae_single_pmk_vsie_ap() - check if the Roamed AP supports sae
 * roaming using single pmk connection
 * with same pmk or not
 * @bss_des: bss descriptor
 *
 * Return: True if same pmk IE is present
 */
static bool csr_is_sae_single_pmk_vsie_ap(struct bss_description *bss_des)
{
	uint16_t ie_len;
	const uint8_t *vendor_ie;

	if (!bss_des) {
		sme_debug("Invalid bss description");
		return false;
	}
	ie_len = csr_get_ielen_from_bss_description(bss_des);

	vendor_ie =
		wlan_get_vendor_ie_ptr_from_oui(CSR_SINGLE_PMK_OUI,
						CSR_SINGLE_PMK_OUI_SIZE,
						(uint8_t *)bss_des->ieFields,
						ie_len);

	if (!vendor_ie)
		return false;

	sme_debug("AP supports sae single pmk feature");

	return true;
}

/**
 * csr_check_and_set_sae_single_pmk_cap() - check if the Roamed AP support
 * roaming using single pmk
 * with same pmk or not
 * @mac_ctx: mac context
 * @session: Session
 * @vdev_id: session id
 *
 * Return: True if same pmk IE is present
 */
static void
csr_check_and_set_sae_single_pmk_cap(struct mac_context *mac_ctx,
				     struct csr_roam_session *session,
				     uint8_t vdev_id)
{
	struct wlan_objmgr_vdev *vdev;
	struct mlme_pmk_info *pmk_info;
	tPmkidCacheInfo *pmkid_cache;
	int32_t keymgmt;
	bool val, lookup_success;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, vdev_id,
						    WLAN_LEGACY_SME_ID);
	if (!vdev) {
		mlme_err("get vdev failed");
		return;
	}

	keymgmt = wlan_crypto_get_param(vdev, WLAN_CRYPTO_PARAM_KEY_MGMT);
	if (keymgmt < 0) {
		mlme_err("Invalid mgmt cipher");
		wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
		return;
	}

	if (keymgmt & (1 << WLAN_CRYPTO_KEY_MGMT_SAE)) {
		val = csr_is_sae_single_pmk_vsie_ap(session->pConnectBssDesc);
		wlan_mlme_set_sae_single_pmk_bss_cap(mac_ctx->psoc, vdev_id,
						     val);
		if (!val)
			goto end;

		wlan_crypto_set_sae_single_pmk_bss_cap(vdev,
		   (struct qdf_mac_addr *)session->pConnectBssDesc->bssId,
		   true);

		pmkid_cache = qdf_mem_malloc(sizeof(*pmkid_cache));
		if (!pmkid_cache)
			goto end;

		qdf_copy_macaddr(&pmkid_cache->BSSID,
				 &session->connectedProfile.bssid);
		/*
		 * In SAE single pmk roaming case, there will
		 * be no PMK entry found for the AP in pmk cache.
		 * So if the lookup is successful, then we have done
		 * a FULL sae here. In that case, clear all other
		 * single pmk entries.
		 */
		lookup_success = csr_lookup_pmkid_using_bssid(mac_ctx, session,
							      pmkid_cache);
		if (lookup_success) {
			wlan_crypto_selective_clear_sae_single_pmk_entries(vdev,
					&session->connectedProfile.bssid);

			pmk_info = qdf_mem_malloc(sizeof(*pmk_info));
			if (!pmk_info) {
				qdf_mem_zero(pmkid_cache, sizeof(*pmkid_cache));
				qdf_mem_free(pmkid_cache);
				goto end;
			}

			qdf_mem_copy(pmk_info->pmk, pmkid_cache->pmk,
				     session->pmk_len);
			pmk_info->pmk_len = pmkid_cache->pmk_len;
			pmk_info->spmk_timestamp = pmkid_cache->pmk_ts;
			pmk_info->spmk_timeout_period  =
				(pmkid_cache->pmk_lifetime *
				 pmkid_cache->pmk_lifetime_threshold / 100);

			wlan_mlme_update_sae_single_pmk(vdev, pmk_info);

			qdf_mem_zero(pmk_info, sizeof(*pmk_info));
			qdf_mem_free(pmk_info);
		}

		qdf_mem_zero(pmkid_cache, sizeof(*pmkid_cache));
		qdf_mem_free(pmkid_cache);
	}

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);
}
#else
static inline void
csr_check_and_set_sae_single_pmk_cap(struct mac_context *mac_ctx,
				     struct csr_roam_session *session,
				     uint8_t vdev_id)
{
}
#endif

#define IS_ROAM_REASON_STA_KICKOUT(reason) ((reason & 0xF) == \
	WMI_ROAM_TRIGGER_REASON_STA_KICKOUT)
#define IS_ROAM_REASON_DISCONNECTION(reason) ((reason & 0xF) == \
	WMI_ROAM_TRIGGER_REASON_DEAUTH)

static QDF_STATUS
csr_process_roam_sync_callback(struct mac_context *mac_ctx,
			       struct roam_offload_synch_ind *roam_synch_data,
			       struct bss_description *bss_desc,
			       enum sir_roam_op_code reason)
{
	uint8_t session_id = roam_synch_data->roamed_vdev_id;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, session_id);
	tDot11fBeaconIEs *ies_local = NULL;
	struct ps_global_info *ps_global_info = &mac_ctx->sme.ps_global_info;
	struct csr_roam_info *roam_info;
	tCsrRoamConnectedProfile *conn_profile = NULL;
	sme_QosAssocInfo assoc_info;
	struct bss_params *add_bss_params;
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	tPmkidCacheInfo *pmkid_cache;
	uint16_t len;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	struct ht_profile *src_profile = NULL;
	tCsrRoamHTProfile *dst_profile = NULL;
#endif
	struct wlan_objmgr_vdev *vdev;
	struct mlme_roam_after_data_stall *vdev_roam_params;
	bool abort_host_scan_cap = false;
	wlan_scan_id scan_id;
	struct wlan_crypto_pmksa *pmksa;
	uint8_t ssid_offset;
	enum csr_akm_type akm_type;
	uint8_t mdie_present;

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(mac_ctx->psoc, session_id,
						    WLAN_LEGACY_SME_ID);

	if (!vdev) {
		sme_err("vdev is NULL, aborting roam invoke");
		return QDF_STATUS_E_NULL_VALUE;
	}

	vdev_roam_params = mlme_get_roam_invoke_params(vdev);

	if (!vdev_roam_params) {
		sme_err("Invalid vdev roam params, aborting roam invoke");
		status = QDF_STATUS_E_NULL_VALUE;
		goto end;
	}

	if (!session) {
		sme_err("LFR3: Session not found");
		status = QDF_STATUS_E_NULL_VALUE;
		goto end;
	}

	sme_debug("LFR3: reason: %d roam invoke in progress %d, source %d",
		  reason, vdev_roam_params->roam_invoke_in_progress,
		  vdev_roam_params->source);
	if (reason == SIR_ROAM_SYNCH_PROPAGATION)
		wlan_rec_conn_info(session_id, DEBUG_CONN_ROAMING,
				   bss_desc ? bss_desc->bssId : NULL,
				   reason, session->connectState);
	switch (reason) {
	case SIR_ROAMING_DEREGISTER_STA:
		/*
		 * The following is the first thing done in CSR
		 * after receiving RSI. Hence stopping the timer here.
		 */
		csr_roam_roaming_offload_timer_action(mac_ctx,
				0, session_id, ROAMING_OFFLOAD_TIMER_STOP);
		csr_roam_invoke_timer_stop(mac_ctx, session_id);
		if (session->discon_in_progress) {
			sme_err("LFR3: vdev:%d Disconnect is in progress roam_synch is not allowed",
				session_id);
			status = QDF_STATUS_E_FAILURE;
			goto end;
		}

		status = csr_post_roam_state_change(mac_ctx, session_id,
						    WLAN_ROAM_SYNCH_IN_PROG,
						    REASON_ROAM_HANDOFF_DONE);
		if (QDF_IS_STATUS_ERROR(status))
			goto end;

		mlme_init_twt_context(mac_ctx->psoc, &roam_synch_data->bssid,
				      TWT_ALL_SESSIONS_DIALOG_ID);
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_FT_START, eCSR_ROAM_RESULT_SUCCESS);
		goto end;
	case SIR_ROAMING_START:
		status = csr_post_roam_state_change(
					mac_ctx, session_id,
					WLAN_ROAMING_IN_PROG,
					REASON_ROAM_CANDIDATE_FOUND);
		if (QDF_IS_STATUS_ERROR(status))
			goto end;

		csr_roam_roaming_offload_timer_action(mac_ctx,
				CSR_ROAMING_OFFLOAD_TIMEOUT_PERIOD, session_id,
				ROAMING_OFFLOAD_TIMER_START);
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_START, eCSR_ROAM_RESULT_SUCCESS);

		/*
		 * For emergency deauth roaming, firmware sends ROAM start
		 * instead of ROAM scan start notification as data path queues
		 * will be stopped only during roam start notification.
		 * This is because, for deauth/disassoc triggered roam, the
		 * AP has sent deauth, and packets shouldn't be sent to AP
		 * after that. Since firmware is sending roam start directly
		 * host sends scan abort during roam scan, but in other
		 * triggers, the host receives roam start after candidate
		 * selection and roam scan is complete. So when host sends
		 * roam abort for emergency deauth roam trigger, the firmware
		 * roam scan is also aborted. This results in roaming failure.
		 * So send scan_id as CANCEL_HOST_SCAN_ID to scan module to
		 * abort only host triggered scans.
		 */
		abort_host_scan_cap =
			wlan_mlme_get_host_scan_abort_support(mac_ctx->psoc);
		if (abort_host_scan_cap)
			scan_id = CANCEL_HOST_SCAN_ID;
		else
			scan_id = INVAL_SCAN_ID;

		wlan_abort_scan(mac_ctx->pdev, INVAL_PDEV_ID,
				session_id, scan_id, false);
		goto end;
	case SIR_ROAMING_ABORT:
		/*
		 * Roaming abort is received after roaming is started
		 * in firmware(after candidate selection) but re-assoc to
		 * the candidate was not successful.
		 * Connection to the previous AP is still valid in this
		 * case. So move to RSO_ENABLED state.
		 */
		csr_post_roam_state_change(mac_ctx, session_id,
					   WLAN_ROAM_RSO_ENABLED,
					   REASON_ROAM_ABORT);
		csr_roam_roaming_offload_timer_action(mac_ctx,
				0, session_id, ROAMING_OFFLOAD_TIMER_STOP);
		csr_roam_invoke_timer_stop(mac_ctx, session_id);
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_ABORT, eCSR_ROAM_RESULT_SUCCESS);
		goto end;
	case SIR_ROAM_SYNCH_NAPI_OFF:
		csr_roam_call_callback(mac_ctx, session_id, NULL, 0,
				eCSR_ROAM_NAPI_OFF, eCSR_ROAM_RESULT_SUCCESS);
		goto end;
	case SIR_ROAMING_INVOKE_FAIL:
		sme_debug("Roaming triggered failed source %d nud behaviour %d",
			  vdev_roam_params->source, mac_ctx->nud_fail_behaviour);
		status = csr_roam_invoke_timer_stop(mac_ctx, session_id);
		if (QDF_IS_STATUS_ERROR(status)) {
			sme_debug("Ignoring roam invoke fail event as host didn't send roam invoke request");
			goto end;
		}

		/* Userspace roam req fail, disconnect with AP */
		if (vdev_roam_params->source == USERSPACE_INITIATED ||
		    mac_ctx->nud_fail_behaviour == DISCONNECT_AFTER_ROAM_FAIL)
			csr_roam_disconnect(mac_ctx, session_id,
				    eCSR_DISCONNECT_REASON_DEAUTH,
				    REASON_USER_TRIGGERED_ROAM_FAILURE);

		vdev_roam_params->roam_invoke_in_progress = false;
		goto end;
	case SIR_ROAM_SYNCH_PROPAGATION:
		break;
	case SIR_ROAM_SYNCH_COMPLETE:
		/* Handle one race condition that if candidate is already
		 *selected & FW has gone ahead with roaming or about to go
		 * ahead when set_band comes, it will be complicated for FW
		 * to stop the current roaming. Instead, host will check the
		 * roam sync to make sure the new AP is on 2G, or disconnect
		 * the AP.
		 */
		if (wlan_reg_is_disable_for_freq(mac_ctx->pdev,
						 roam_synch_data->chan_freq)) {
			csr_roam_disconnect(mac_ctx, session_id,
					    eCSR_DISCONNECT_REASON_DEAUTH,
					    REASON_OPER_CHANNEL_BAND_CHANGE);
			sme_debug("Roaming Failed for disabled channel or band");
			vdev_roam_params->roam_invoke_in_progress = false;
			goto end;
		}
		/*
		 * Following operations need to be done once roam sync
		 * completion is sent to FW, hence called here:
		 * 1) Firmware has already updated DBS policy. Update connection
		 *    table in the host driver.
		 * 2) Force SCC switch if needed
		 * 3) Set connection in progress = false
		 */
		/* first update connection info from wma interface */
		policy_mgr_update_connection_info(mac_ctx->psoc, session_id);
		/* then update remaining parameters from roam sync ctx */
		sme_debug("Update DBS hw mode");
		policy_mgr_hw_mode_transition_cb(
			roam_synch_data->hw_mode_trans_ind.old_hw_mode_index,
			roam_synch_data->hw_mode_trans_ind.new_hw_mode_index,
			roam_synch_data->hw_mode_trans_ind.num_vdev_mac_entries,
			roam_synch_data->hw_mode_trans_ind.vdev_mac_map,
			mac_ctx->psoc);
		mac_ctx->sme.set_connection_info_cb(false);

		csr_check_and_set_sae_single_pmk_cap(mac_ctx, session,
						     session_id);

		if (ucfg_pkt_capture_get_pktcap_mode(mac_ctx->psoc))
			ucfg_pkt_capture_record_channel(vdev);

		if (WLAN_REG_IS_5GHZ_CH_FREQ(bss_desc->chan_freq)) {
			session->disable_hi_rssi = true;
			sme_debug("Disabling HI_RSSI, AP freq=%d, rssi=%d",
				  bss_desc->chan_freq, bss_desc->rssi);
		} else {
			session->disable_hi_rssi = false;
		}

		policy_mgr_check_n_start_opportunistic_timer(mac_ctx->psoc);
		policy_mgr_check_concurrent_intf_and_restart_sap(mac_ctx->psoc);
		vdev_roam_params->roam_invoke_in_progress = false;

		if (roam_synch_data->authStatus ==
		    CSR_ROAM_AUTH_STATUS_AUTHENTICATED) {
			csr_post_roam_state_change(mac_ctx, session_id,
						   WLAN_ROAM_RSO_ENABLED,
						   REASON_CONNECT);
		} else {
			/*
			 * STA is just in associated state here, RSO
			 * enable will be sent once EAP & EAPOL will be done by
			 * user-space and after set key response
			 * is received.
			 */
			csr_post_roam_state_change(mac_ctx, session_id,
						   WLAN_ROAM_INIT,
						   REASON_CONNECT);
		}
		goto end;
	case SIR_ROAMING_DEAUTH:
		csr_roam_roaming_offload_timer_action(
			mac_ctx, 0, session_id, ROAMING_OFFLOAD_TIMER_STOP);
		csr_roam_invoke_timer_stop(mac_ctx, session_id);
		goto end;
	default:
		status = QDF_STATUS_E_FAILURE;
		goto end;
	}
	session->roam_synch_data = roam_synch_data;
	status = csr_get_parsed_bss_description_ies(
			mac_ctx, bss_desc, &ies_local);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_err("LFR3: fail to parse IEs. Len Bcn : %d, Reassoc req : %d Reassoc rsp : %d",
			roam_synch_data->beaconProbeRespLength,
			roam_synch_data->reassoc_req_length,
			roam_synch_data->reassocRespLength);
		qdf_trace_hex_dump(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_ERROR,
				   (uint8_t *)roam_synch_data +
				   roam_synch_data->beaconProbeRespOffset,
				   roam_synch_data->beaconProbeRespLength);
		goto end;
	}

	conn_profile = &session->connectedProfile;
	status = csr_roam_stop_network(mac_ctx, session_id,
				       session->pCurRoamProfile,
				       bss_desc, ies_local);
	if (!QDF_IS_STATUS_SUCCESS(status))
		goto end;
	ps_global_info->remain_in_power_active_till_dhcp = false;
	session->connectState = eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED;

	/* Update the BLM that the previous profile has disconnected */
	wlan_blm_update_bssid_connect_params(mac_ctx->pdev,
					     session->connectedProfile.bssid,
					     BLM_AP_DISCONNECTED);

	if (IS_ROAM_REASON_STA_KICKOUT(roam_synch_data->roamReason)) {
		struct reject_ap_info ap_info;

		ap_info.bssid = session->connectedProfile.bssid;
		ap_info.reject_ap_type = DRIVER_AVOID_TYPE;
		ap_info.reject_reason = REASON_STA_KICKOUT;
		ap_info.source = ADDED_BY_DRIVER;
		wlan_blm_add_bssid_to_reject_list(mac_ctx->pdev, &ap_info);
	}

	/* Remove old BSSID mlme info from scan cache */
	csr_update_scan_entry_associnfo(mac_ctx, session,
					SCAN_ENTRY_CON_STATE_NONE);
	roam_info = qdf_mem_malloc(sizeof(struct csr_roam_info));
	if (!roam_info) {
		qdf_mem_free(ies_local);
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	csr_rso_save_ap_to_scan_cache(mac_ctx, roam_synch_data, bss_desc);
	roam_info->sessionId = session_id;

	qdf_mem_copy(&roam_info->bssid.bytes, &bss_desc->bssId,
			sizeof(struct qdf_mac_addr));
	csr_roam_save_connected_information(mac_ctx, session_id,
			session->pCurRoamProfile,
			bss_desc,
			ies_local);
	/* Add new mlme info to new BSSID after upting connectedProfile */
	csr_update_scan_entry_associnfo(mac_ctx, session,
					SCAN_ENTRY_CON_STATE_ASSOC);

#ifdef FEATURE_WLAN_ESE
	roam_info->isESEAssoc = conn_profile->isESEAssoc;
#endif

	/*
	 * Encryption keys for new connection are obtained as follows:
	 * authStatus = CSR_ROAM_AUTH_STATUS_AUTHENTICATED
	 * Open - No keys required.
	 * Static WEP - Firmware copies keys from old AP to new AP.
	 * Fast roaming authentications e.g. PSK, FT, CCKM - firmware
	 *      supplicant obtains them through 4-way handshake.
	 *
	 * authStatus = CSR_ROAM_AUTH_STATUS_CONNECTED
	 * All other authentications - Host supplicant performs EAPOL
	 *      with AP after this point and sends new keys to the driver.
	 *      Driver starts wait_for_key timer for that purpose.
	 * Allow csr_lookup_pmkid_using_bssid() if akm is SAE/OWE since
	 * SAE/OWE roaming uses hybrid model and eapol is offloaded to
	 * supplicant unlike in WPA2 – 802.1x case, after 8 way handshake
	 * the __wlan_hdd_cfg80211_keymgmt_set_key ->sme_roam_set_psk_pmk()
	 * will get called after roam synch complete to update the
	 * session->psk_pmk, but in SAE/OWE roaming this sequence is not
	 * present and set_pmksa will come before roam synch indication &
	 * eapol. So the session->psk_pmk will be stale in PMKSA cached
	 * SAE/OWE roaming case.
	 */
	if (roam_synch_data->authStatus == CSR_ROAM_AUTH_STATUS_AUTHENTICATED ||
	    session->pCurRoamProfile->negotiatedAuthType ==
	    eCSR_AUTH_TYPE_SAE ||
	    session->pCurRoamProfile->negotiatedAuthType ==
	    eCSR_AUTH_TYPE_OWE) {
		csr_roam_substate_change(mac_ctx,
				eCSR_ROAM_SUBSTATE_NONE, session_id);
		/*
		 * If authStatus is AUTHENTICATED, then we have done successful
		 * 4 way handshake in FW using the cached PMKID.
		 * However, the session->psk_pmk has the PMK of the older AP
		 * as set_key is not received from supplicant.
		 * When any RSO command is sent for the current AP, the older
		 * AP's PMK is sent to the FW which leads to incorrect PMK and
		 * leads to 4 way handshake failure when roaming happens to
		 * this AP again.
		 * Check if a PMK cache exists for the roamed AP and update
		 * it into the session pmk.
		 */
		pmkid_cache = qdf_mem_malloc(sizeof(*pmkid_cache));
		if (!pmkid_cache) {
			status = QDF_STATUS_E_NOMEM;
			goto end;
		}

		qdf_copy_macaddr(&pmkid_cache->BSSID,
				 &session->connectedProfile.bssid);
		sme_debug("Trying to find PMKID for " QDF_MAC_ADDR_FMT " AKM Type:%d",
			  QDF_MAC_ADDR_REF(pmkid_cache->BSSID.bytes),
			  session->pCurRoamProfile->negotiatedAuthType);
		akm_type = session->connectedProfile.AuthType;
		mdie_present = session->connectedProfile.mdid.mdie_present;

		if (csr_lookup_pmkid_using_bssid(mac_ctx, session,
						 pmkid_cache)) {
			session->pmk_len = pmkid_cache->pmk_len;
			qdf_mem_zero(session->psk_pmk,
				     sizeof(session->psk_pmk));
			qdf_mem_copy(session->psk_pmk, pmkid_cache->pmk,
				     session->pmk_len);
			/*
			 * Consider two APs: AP1, AP2
			 * Both APs configured with EAP 802.1x security mode
			 * and OKC is enabled in both APs by default. Initially
			 * DUT successfully associated with AP1, and generated
			 * PMK1 by performing full EAP and added an entry for
			 * AP1 in pmk table. At this stage, pmk table has only
			 * one entry for PMK1 (1. AP1-->PMK1).
			 * Now DUT try to roam to AP2 using PMK1 (as OKC is
			 * enabled) but key timeout happens on AP2 just before
			 * 4 way handshake completion in FW. At this point of
			 * time DUT not in authenticated state. Due to this DUT
			 * performs full EAP with AP2 and generates PMK2. As
			 * there is no previous entry of AP2 (AP2-->PMK1) in
			 * pmk table, When host gets pmk delete command for
			 * BSSID of AP2, BSSID match fails. This results host
			 * is unable to delete pmk entry for AP1. At this point
			 * of time PMK table has two entry
			 * 1. AP1-->PMK1 and 2. AP2 --> PMK2.
			 * Now security profile for both APs changed to FT-RSN.
			 * DUT first disassociate with AP2 and successfully
			 * associated with AP2 by performing full EAP and
			 * generates PMK2*. DUT first deletes PMK entry for AP2
			 * and then add new entry for AP2. At this point of time
			 * pmk table has two entry AP1-->PMK1 and PMK2-->PMK2*.
			 * Now DUT roamed to AP1 using PMK2* but sends stale
			 * entry of AP1 (PMK1) to fw via RSO command. This
			 * results fw override PMK for both APs with PMK1 (as FW
			 * maintains only one PMK for both APs in case of FT
			 * roaming) and next time when FW try to roam to AP2
			 * using PMK1, AP2 rejects PMK1 (As AP2 is expecting
			 * PMK2*) and initiates full EAP with AP2, which is
			 * wrong.
			 * To address this issue update pmk table entry for
			 * roamed AP with the pmk3 value comes to host via roam
			 * sync data. By this host override stale entry (if any)
			 * with latest valid pmk for that AP at a point of time.
			 */
			if (roam_synch_data->pmk_len) {
				pmksa = qdf_mem_malloc(sizeof(*pmksa));
				if (!pmksa) {
					qdf_mem_zero(pmkid_cache,
						     sizeof(pmkid_cache));
					qdf_mem_free(pmkid_cache);
					status = QDF_STATUS_E_NOMEM;
					goto end;
				}

				/* Update the session PMK also */
				session->pmk_len = roam_synch_data->pmk_len;
				qdf_mem_zero(session->psk_pmk,
					     sizeof(session->psk_pmk));
				qdf_mem_copy(session->psk_pmk,
					     roam_synch_data->pmk,
					     session->pmk_len);

				/* Update the crypto table */
				qdf_copy_macaddr(&pmksa->bssid,
						 &conn_profile->bssid);
				qdf_mem_copy(pmksa->pmkid,
					     roam_synch_data->pmkid, PMKID_LEN);
				qdf_mem_copy(pmksa->pmk,
					     roam_synch_data->pmk,
					     roam_synch_data->pmk_len);
				pmksa->pmk_len = roam_synch_data->pmk_len;
				if (wlan_crypto_set_del_pmksa(vdev, pmksa, true)
				    != QDF_STATUS_SUCCESS) {
					qdf_mem_zero(pmksa, sizeof(*pmksa));
					qdf_mem_free(pmksa);
				}
			}
			sme_debug("pmkid found for " QDF_MAC_ADDR_FMT " len %d",
				  QDF_MAC_ADDR_REF(pmkid_cache->BSSID.bytes),
				  (uint32_t)session->pmk_len);
		} else {
			sme_debug("PMKID Not found in cache for " QDF_MAC_ADDR_FMT,
				  QDF_MAC_ADDR_REF(pmkid_cache->BSSID.bytes));
			/*
			 * In FT roam when the CSR lookup fails then the PMK
			 * details from the roam sync indication will be
			 * updated to Session/PMK cache. This will result in
			 * having multiple PMK cache entries for the same MDID,
			 * So do not add the PMKSA cache entry in all the
			 * FT-Roam cases.
			 */
			if (!csr_is_auth_type11r(mac_ctx, akm_type,
						 mdie_present) &&
			    roam_synch_data->pmk_len) {
				pmksa = qdf_mem_malloc(sizeof(*pmksa));
				if (!pmksa) {
					qdf_mem_zero(pmkid_cache,
						     sizeof(pmkid_cache));
					qdf_mem_free(pmkid_cache);
					status = QDF_STATUS_E_NOMEM;
					goto end;
				}

				session->pmk_len = roam_synch_data->pmk_len;
				qdf_mem_zero(session->psk_pmk,
					     sizeof(session->psk_pmk));
				qdf_mem_copy(session->psk_pmk,
					     roam_synch_data->pmk,
					     session->pmk_len);

				qdf_copy_macaddr(&pmksa->bssid,
						 &session->
						 connectedProfile.bssid);
				qdf_mem_copy(pmksa->pmkid,
					     roam_synch_data->pmkid, PMKID_LEN);
				qdf_mem_copy(pmksa->pmk, roam_synch_data->pmk,
					     roam_synch_data->pmk_len);
				pmksa->pmk_len = roam_synch_data->pmk_len;

				if (wlan_crypto_set_del_pmksa(vdev, pmksa, true)
					!= QDF_STATUS_SUCCESS) {
					qdf_mem_zero(pmksa, sizeof(*pmksa));
					qdf_mem_free(pmksa);
				}
			}
		}
		qdf_mem_zero(pmkid_cache, sizeof(pmkid_cache));
		qdf_mem_free(pmkid_cache);
	}

	if (roam_synch_data->authStatus != CSR_ROAM_AUTH_STATUS_AUTHENTICATED) {
		roam_info->fAuthRequired = true;
		csr_roam_substate_change(mac_ctx,
				eCSR_ROAM_SUBSTATE_WAIT_FOR_KEY,
				session_id);

		ps_global_info->remain_in_power_active_till_dhcp = true;
		mac_ctx->roam.WaitForKeyTimerInfo.vdev_id = session_id;
		if (!QDF_IS_STATUS_SUCCESS(csr_roam_start_wait_for_key_timer(
				mac_ctx, CSR_WAIT_FOR_KEY_TIMEOUT_PERIOD))
		   ) {
			sme_err("Failed wait for key timer start");
			csr_roam_substate_change(mac_ctx,
					eCSR_ROAM_SUBSTATE_NONE,
					session_id);
		}
		csr_neighbor_roam_state_transition(mac_ctx,
				eCSR_NEIGHBOR_ROAM_STATE_INIT, session_id);
	}

	if (roam_synch_data->is_ft_im_roam) {
		ssid_offset = SIR_MAC_ASSOC_REQ_SSID_OFFSET;
	} else {
		ssid_offset = SIR_MAC_REASSOC_REQ_SSID_OFFSET;
	}

	roam_info->nBeaconLength = 0;
	roam_info->nAssocReqLength = roam_synch_data->reassoc_req_length -
		SIR_MAC_HDR_LEN_3A - ssid_offset;
	roam_info->nAssocRspLength = roam_synch_data->reassocRespLength -
		SIR_MAC_HDR_LEN_3A;
	roam_info->pbFrames = qdf_mem_malloc(roam_info->nBeaconLength +
		roam_info->nAssocReqLength + roam_info->nAssocRspLength);
	if (!roam_info->pbFrames) {
		if (roam_info)
			qdf_mem_free(roam_info);
		qdf_mem_free(ies_local);
		status = QDF_STATUS_E_NOMEM;
		goto end;
	}
	qdf_mem_copy(roam_info->pbFrames,
			(uint8_t *)roam_synch_data +
			roam_synch_data->reassoc_req_offset +
			SIR_MAC_HDR_LEN_3A + ssid_offset,
			roam_info->nAssocReqLength);

	qdf_mem_copy(roam_info->pbFrames + roam_info->nAssocReqLength,
			(uint8_t *)roam_synch_data +
			roam_synch_data->reassocRespOffset +
			SIR_MAC_HDR_LEN_3A,
			roam_info->nAssocRspLength);

	sme_debug("LFR3:Clear Connected info");
	csr_roam_free_connected_info(mac_ctx, &session->connectedInfo);
	len = roam_synch_data->join_rsp->parsedRicRspLen;

#ifdef FEATURE_WLAN_ESE
	len += roam_synch_data->join_rsp->tspecIeLen;
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		FL("LFR3: tspecLen %d"),
		roam_synch_data->join_rsp->tspecIeLen);
#endif

	sme_debug("LFR3: RIC length - %d",
		  roam_synch_data->join_rsp->parsedRicRspLen);
	if (len) {
		session->connectedInfo.pbFrames =
			qdf_mem_malloc(len);
		if (session->connectedInfo.pbFrames) {
			qdf_mem_copy(session->connectedInfo.pbFrames,
				roam_synch_data->join_rsp->frames, len);
			session->connectedInfo.nRICRspLength =
				roam_synch_data->join_rsp->parsedRicRspLen;

#ifdef FEATURE_WLAN_ESE
			session->connectedInfo.nTspecIeLength =
				roam_synch_data->join_rsp->tspecIeLen;
#endif
		}
	}
	conn_profile->vht_channel_width =
		roam_synch_data->join_rsp->vht_channel_width;
	add_bss_params = (struct bss_params *)roam_synch_data->add_bss_params;
	roam_info->staId = session->connectedInfo.staId;
	roam_info->timingMeasCap =
		roam_synch_data->join_rsp->timingMeasCap;
	roam_info->chan_info.nss = roam_synch_data->join_rsp->nss;
	roam_info->chan_info.rate_flags =
		roam_synch_data->join_rsp->max_rate_flags;
	roam_info->chan_info.ch_width =
		roam_synch_data->join_rsp->vht_channel_width;
	csr_roam_fill_tdls_info(mac_ctx, roam_info, roam_synch_data->join_rsp);
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	src_profile = &roam_synch_data->join_rsp->ht_profile;
	dst_profile = &conn_profile->ht_profile;
	if (mac_ctx->roam.configParam.cc_switch_mode
			!= QDF_MCC_TO_SCC_SWITCH_DISABLE)
		csr_roam_copy_ht_profile(dst_profile,
				src_profile);
#endif
	assoc_info.bss_desc = bss_desc;
	roam_info->status_code = eSIR_SME_SUCCESS;
	roam_info->reasonCode = eSIR_SME_SUCCESS;
	assoc_info.pProfile = session->pCurRoamProfile;
	mac_ctx->roam.roamSession[session_id].connectState =
		eCSR_ASSOC_STATE_TYPE_NOT_CONNECTED;
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_HANDOFF_ASSOC_REQ, NULL);
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_REASSOC_REQ, NULL);
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_HANDOFF_COMPLETE, NULL);
	mac_ctx->roam.roamSession[session_id].connectState =
		eCSR_ASSOC_STATE_TYPE_INFRA_ASSOCIATED;
	sme_qos_csr_event_ind(mac_ctx, session_id,
		SME_QOS_CSR_REASSOC_COMPLETE, &assoc_info);
	roam_info->bss_desc = bss_desc;
	conn_profile->acm_mask = sme_qos_get_acm_mask(mac_ctx,
			bss_desc, NULL);
	if (conn_profile->modifyProfileFields.uapsd_mask) {
		sme_debug(
				" uapsd_mask (0x%X) set, request UAPSD now",
				conn_profile->modifyProfileFields.uapsd_mask);
		sme_ps_start_uapsd(MAC_HANDLE(mac_ctx), session_id);
	}
	conn_profile->dot11Mode = session->bssParams.uCfgDot11Mode;
	roam_info->u.pConnectedProfile = conn_profile;

	sme_debug(
		"vht ch width %d staId %d nss %d rate_flag %d dot11Mode %d",
		conn_profile->vht_channel_width,
		roam_info->staId,
		roam_info->chan_info.nss,
		roam_info->chan_info.rate_flags,
		conn_profile->dot11Mode);

	if (!IS_FEATURE_SUPPORTED_BY_FW
			(SLM_SESSIONIZATION) &&
			(csr_is_concurrent_session_running(mac_ctx))) {
		mac_ctx->roam.configParam.doBMPSWorkaround = 1;
	}
	roam_info->roamSynchInProgress = true;
	roam_info->synchAuthStatus = roam_synch_data->authStatus;
	roam_info->kck_len = roam_synch_data->kck_len;
	roam_info->kek_len = roam_synch_data->kek_len;
	roam_info->pmk_len = roam_synch_data->pmk_len;
	qdf_mem_copy(roam_info->kck, roam_synch_data->kck, roam_info->kck_len);
	qdf_mem_copy(roam_info->kek, roam_synch_data->kek, roam_info->kek_len);

	if (roam_synch_data->pmk_len)
		qdf_mem_copy(roam_info->pmk, roam_synch_data->pmk,
			     roam_synch_data->pmk_len);

	qdf_mem_copy(roam_info->pmkid, roam_synch_data->pmkid, PMKID_LEN);
	roam_info->update_erp_next_seq_num =
			roam_synch_data->update_erp_next_seq_num;
	roam_info->next_erp_seq_num = roam_synch_data->next_erp_seq_num;
	csr_update_fils_erp_seq_num(mac_ctx->psoc, session_id,
				    session->pCurRoamProfile,
				    roam_info->next_erp_seq_num);
	sme_debug("Update ERP Seq Num : %d, Next ERP Seq Num : %d",
			roam_info->update_erp_next_seq_num,
			roam_info->next_erp_seq_num);
	qdf_mem_copy(roam_info->replay_ctr, roam_synch_data->replay_ctr,
			SIR_REPLAY_CTR_LEN);
	QDF_TRACE(QDF_MODULE_ID_SME, QDF_TRACE_LEVEL_DEBUG,
		FL("LFR3: Copy KCK, KEK(len %d) and Replay Ctr"),
		roam_info->kek_len);
	/* bit-4 and bit-5 indicate the subnet status */
	roam_info->subnet_change_status =
		CSR_GET_SUBNET_STATUS(roam_synch_data->roamReason);

	/* fetch 4 LSB to get roam reason */
	roam_info->roam_reason = roam_synch_data->roamReason &
				 ROAM_REASON_MASK;
	sme_debug("Update roam reason : %d", roam_info->roam_reason);
	csr_copy_fils_join_rsp_roam_info(roam_info, roam_synch_data);

	csr_roam_call_callback(mac_ctx, session_id, roam_info, 0,
		eCSR_ROAM_ASSOCIATION_COMPLETION, eCSR_ROAM_RESULT_ASSOCIATED);
	csr_reset_pmkid_candidate_list(mac_ctx, session_id);

	if (IS_ROAM_REASON_DISCONNECTION(roam_synch_data->roamReason))
		sme_qos_csr_event_ind(mac_ctx, session_id,
				      SME_QOS_CSR_DISCONNECT_ROAM_COMPLETE,
				      NULL);

	if (!CSR_IS_WAIT_FOR_KEY(mac_ctx, session_id)) {
		QDF_TRACE(QDF_MODULE_ID_SME,
				QDF_TRACE_LEVEL_DEBUG,
				FL
				("NO CSR_IS_WAIT_FOR_KEY -> csr_roam_link_up"));
		csr_roam_link_up(mac_ctx, conn_profile->bssid);
	}

	session->fRoaming = false;
	sme_free_join_rsp_fils_params(roam_info);
	qdf_mem_free(roam_info->pbFrames);
	qdf_mem_free(roam_info);
	qdf_mem_free(ies_local);

end:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_LEGACY_SME_ID);

	return status;
}

/**
 * csr_roam_synch_callback() - SME level callback for roam synch propagation
 * @mac_ctx: MAC Context
 * @roam_synch_data: Roam synch data buffer pointer
 * @bss_desc: BSS descriptor pointer
 * @reason: Reason for calling the callback
 *
 * This callback is registered with WMA and used after roaming happens in
 * firmware and the call to this routine completes the roam synch
 * propagation at both CSR and HDD levels. The HDD level propagation
 * is achieved through the already defined callback for assoc completion
 * handler.
 *
 * Return: Success or Failure.
 */
QDF_STATUS
csr_roam_synch_callback(struct mac_context *mac_ctx,
			struct roam_offload_synch_ind *roam_synch_data,
			struct bss_description *bss_desc,
			enum sir_roam_op_code reason)
{
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	status = csr_process_roam_sync_callback(mac_ctx, roam_synch_data,
					    bss_desc, reason);

	sme_release_global_lock(&mac_ctx->sme);

	return status;
}

#ifdef WLAN_FEATURE_SAE
/**
 * csr_process_roam_auth_sae_callback() - API to trigger the
 * WPA3 pre-auth event for candidate AP received from firmware.
 * @mac_ctx: Global mac context pointer
 * @vdev_id: vdev id
 * @roam_bssid: Candidate BSSID to roam
 *
 * This function calls the hdd_sme_roam_callback with reason
 * eCSR_ROAM_SAE_COMPUTE to trigger SAE auth to supplicant.
 */
static QDF_STATUS
csr_process_roam_auth_sae_callback(struct mac_context *mac_ctx,
				   uint8_t vdev_id,
				   struct qdf_mac_addr roam_bssid)
{
	struct csr_roam_info *roam_info;
	struct sir_sae_info sae_info;
	struct csr_roam_session *session = CSR_GET_SESSION(mac_ctx, vdev_id);

	if (!session) {
		sme_err("WPA3 Preauth event with invalid session id:%d",
			vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	roam_info = qdf_mem_malloc(sizeof(*roam_info));
	if (!roam_info)
		return QDF_STATUS_E_FAILURE;

	sae_info.msg_len = sizeof(sae_info);
	sae_info.vdev_id = vdev_id;

	sae_info.ssid.length = session->connectedProfile.SSID.length;
	qdf_mem_copy(sae_info.ssid.ssId, session->connectedProfile.SSID.ssId,
		     sae_info.ssid.length);

	qdf_mem_copy(sae_info.peer_mac_addr.bytes,
		     roam_bssid.bytes, QDF_MAC_ADDR_SIZE);

	roam_info->sae_info = &sae_info;

	csr_roam_call_callback(mac_ctx, vdev_id, roam_info, 0,
			       eCSR_ROAM_SAE_COMPUTE, eCSR_ROAM_RESULT_NONE);

	qdf_mem_free(roam_info);

	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
csr_process_roam_auth_sae_callback(struct mac_context *mac_ctx,
				   uint8_t vdev_id,
				   struct qdf_mac_addr roam_bssid)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

QDF_STATUS
csr_roam_auth_offload_callback(struct mac_context *mac_ctx,
			       uint8_t vdev_id, struct qdf_mac_addr bssid)
{
	QDF_STATUS status;

	status = sme_acquire_global_lock(&mac_ctx->sme);
	if (!QDF_IS_STATUS_SUCCESS(status))
		return status;

	status = csr_process_roam_auth_sae_callback(mac_ctx, vdev_id, bssid);

	sme_release_global_lock(&mac_ctx->sme);

	return status;

}
#endif

QDF_STATUS csr_update_owe_info(struct mac_context *mac,
			       struct assoc_ind *assoc_ind)
{
	uint32_t session_id = WLAN_UMAC_VDEV_ID_MAX;
	QDF_STATUS status;

	status = csr_roam_get_session_id_from_bssid(mac,
					(struct qdf_mac_addr *)assoc_ind->bssId,
					&session_id);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		sme_debug("Couldn't find session_id for given BSSID");
		return QDF_STATUS_E_FAILURE;
	}

	/* Send Association completion message to PE */
	if (assoc_ind->owe_status)
		status = QDF_STATUS_E_INVAL;
	status = csr_send_assoc_cnf_msg(mac, assoc_ind, status,
					assoc_ind->owe_status);
	/*
	 * send a message to CSR itself just to avoid the EAPOL frames
	 * going OTA before association response
	 */
	if (assoc_ind->owe_status == 0)
		status = csr_send_assoc_ind_to_upper_layer_cnf_msg(mac,
								   assoc_ind,
								   status,
								   session_id);

	return status;
}

QDF_STATUS
csr_roam_update_cfg(struct mac_context *mac, uint8_t vdev_id, uint8_t reason)
{
	if (!MLME_IS_ROAM_STATE_RSO_ENABLED(mac->psoc, vdev_id)) {
		sme_debug("Update cfg received while ROAM RSO not started");
		return QDF_STATUS_E_INVAL;
	}

	return csr_roam_offload_scan(mac, vdev_id, ROAM_SCAN_OFFLOAD_UPDATE_CFG,
				     reason);
}

QDF_STATUS
csr_enable_roaming_on_connected_sta(struct mac_context *mac, uint8_t vdev_id)
{
	uint32_t op_ch_freq_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint8_t vdev_id_list[MAX_NUMBER_OF_CONC_CONNECTIONS];
	uint32_t sta_vdev_id = WLAN_INVALID_VDEV_ID;
	struct csr_roam_session *session;
	uint32_t count;
	uint32_t idx;

	sta_vdev_id = csr_get_roam_enabled_sta_sessionid(mac, vdev_id);
	if (sta_vdev_id != WLAN_UMAC_VDEV_ID_MAX)
		return QDF_STATUS_E_FAILURE;

	count = policy_mgr_get_mode_specific_conn_info(mac->psoc,
						       op_ch_freq_list,
						       vdev_id_list,
						       PM_STA_MODE);

	if (!count)
		return QDF_STATUS_E_FAILURE;

	/*
	 * Loop through all connected STA vdevs and roaming will be enabled
	 * on the STA that has a different vdev id from the one passed as
	 * input and has non zero roam trigger value.
	 */
	for (idx = 0; idx < count; idx++) {
		session = CSR_GET_SESSION(mac, vdev_id_list[idx]);
		if (vdev_id_list[idx] != vdev_id &&
		    mlme_get_roam_trigger_bitmap(mac->psoc,
						 vdev_id_list[idx])) {
			sta_vdev_id = vdev_id_list[idx];
			break;
		}
	}

	if (sta_vdev_id == WLAN_INVALID_VDEV_ID)
		return QDF_STATUS_E_FAILURE;

	sme_debug("ROAM: Enabling roaming on vdev[%d]", sta_vdev_id);

	return csr_post_roam_state_change(mac, sta_vdev_id,
					  WLAN_ROAM_RSO_ENABLED,
					  REASON_CTX_INIT);
}
