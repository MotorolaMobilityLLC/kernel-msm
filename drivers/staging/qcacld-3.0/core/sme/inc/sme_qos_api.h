/*
 * Copyright (c) 2014-2020 The Linux Foundation. All rights reserved.
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

#if !defined(__SME_QOSAPI_H)
#define __SME_QOSAPI_H

/**
 * \file  sme_qos_api.h
 *
 * \brief prototype for SME QoS APIs
 */

/* Include Files */
#include "qdf_lock.h"
#include "qdf_trace.h"
#include "qdf_mem.h"
#include "qdf_types.h"
#include "ani_global.h"
#include "sir_api.h"

/* Pre-processor Definitions */
#define SME_QOS_UAPSD_VO      0x01
#define SME_QOS_UAPSD_VI      0x02
#define SME_QOS_UAPSD_BE      0x08
#define SME_QOS_UAPSD_BK      0x04

/* Enumeration of the various QoS status types that would be reported to HDD */
enum sme_qos_statustype {
	/*
	 * async: once PE notifies successful TSPEC negotiation, or CSR notifies
	 * for successful reassoc, notifies HDD with current QoS Params
	 */
	SME_QOS_STATUS_SETUP_SUCCESS_IND = 0,
	/* sync: only when App asked for APSD & it's already set with ACM = 0 */
	SME_QOS_STATUS_SETUP_SUCCESS_APSD_SET_ALREADY,
	/* sync or async: in case of async notify HDD with current QoS Params */
	SME_QOS_STATUS_SETUP_FAILURE_RSP,
	/* sync */
	SME_QOS_STATUS_SETUP_INVALID_PARAMS_RSP,
	/* sync: AP doesn't support QoS (WMM) */
	SME_QOS_STATUS_SETUP_NOT_QOS_AP_RSP,
	/* sync: either req has been sent down to PE or just buffered in SME */
	SME_QOS_STATUS_SETUP_REQ_PENDING_RSP,
	/*
	 * async: in case of flow aggregation, if the new TSPEC negotiation
	 * is successful, OR, notify existing flows that TSPEC is modified with
	 * current QoS Params
	 */
	SME_QOS_STATUS_SETUP_MODIFIED_IND,
	/* sync: no APSD asked for & ACM = 0 */
	SME_QOS_STATUS_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP,
	/*
	 * async: In case of UAPSD, once PE notifies successful TSPEC
	 * negotiation, or CSR notifies for successful reassoc to SME-QoS,
	 * notify HDD if PMC can't put the module in UAPSD mode right away
	 * (QDF_STATUS_PMC_PENDING)
	 */
	SME_QOS_STATUS_SETUP_SUCCESS_IND_APSD_PENDING,
	/*
	 * async: In case of UAPSD, once PE notifies successful TSPEC
	 * negotiation, or CSR notifies for successful reassoc to SME-QoS,
	 * notify HDD if PMC can't put the module in UAPSD mode at all
	 * (QDF_STATUS_E_FAILURE)
	 */
	SME_QOS_STATUS_SETUP_SUCCESS_IND_APSD_SET_FAILED,
	/*
	 * sync: req has been sent down to PE in case of delts or addts
	 * for remain flows, OR if the AC doesn't have APSD or ACM
	 * async: once the downgrade req for QoS params is successful
	 */
	SME_QOS_STATUS_RELEASE_SUCCESS_RSP = 100,
	/* sync or async: in case of async notify HDD with current QoS Param */
	SME_QOS_STATUS_RELEASE_FAILURE_RSP,
	/* async: AP sent DELTS indication */
	SME_QOS_STATUS_RELEASE_QOS_LOST_IND,
	/*
	 * sync: an addts req has been sent down to PE to downgrade the
	 * QoS params or just buffered in SME
	 */
	SME_QOS_STATUS_RELEASE_REQ_PENDING_RSP,
	/* sync */
	SME_QOS_STATUS_RELEASE_INVALID_PARAMS_RSP,
	/*
	 * async: for QoS modify request if modification is successful,
	 * notifies HDD with current QoS Params
	 */
	SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND = 200,
	/* sync: only when App asked for APSD & it's already set with ACM = 0 */
	SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_APSD_SET_ALREADY,
	/* sync or async: in case of async notify HDD with current QoS Param */
	SME_QOS_STATUS_MODIFY_SETUP_FAILURE_RSP,
	/* sync: either req has been sent down to PE or just buffered in SME */
	SME_QOS_STATUS_MODIFY_SETUP_PENDING_RSP,
	/* sync: no APSD asked for & ACM = 0 */
	SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_NO_ACM_NO_APSD_RSP,
	/* sync */
	SME_QOS_STATUS_MODIFY_SETUP_INVALID_PARAMS_RSP,
	/*
	 * async: In case of UAPSD, once PE notifies successful TSPEC
	 * negotiation or CSR notifies for successful reassoc to SME-QoS,
	 * notify HDD if PMC can't put the module in UAPSD mode right away
	 * (QDF_STATUS_PMC_PENDING)
	 */
	SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND_APSD_PENDING,
	/*
	 * async: In case of UAPSD, once PE notifies successful TSPEC
	 * negotiation, or CSR notifies for successful reassoc to SME-QoS,
	 * notify HDD if PMC can't put the module in UAPSD mode at all
	 * (QDF_STATUS_E_FAILURE)
	 */
	SME_QOS_STATUS_MODIFY_SETUP_SUCCESS_IND_APSD_SET_FAILED,
	/* sync: STA is handing off to a new AP */
	SME_QOS_STATUS_HANDING_OFF = 300,
	/* async:powersave mode changed by PMC from UAPSD to Full power */
	SME_QOS_STATUS_OUT_OF_APSD_POWER_MODE_IND = 400,
	/* async:powersave mode changed by PMC from Full power to UAPSD */
	SME_QOS_STATUS_INTO_APSD_POWER_MODE_IND,

};

#define WLAN_MAX_DSCP 0x3f

/*
 * Enumeration of the various User priority (UP) types
 * From 802.1D/802.11e/WMM specifications (all refer to same table)
 */
enum sme_qos_wmmuptype {
	SME_QOS_WMM_UP_BE = 0,
	SME_QOS_WMM_UP_BK = 1,
	SME_QOS_WMM_UP_RESV = 2,        /* Reserved */
	SME_QOS_WMM_UP_EE = 3,
	SME_QOS_WMM_UP_CL = 4,
	SME_QOS_WMM_UP_VI = 5,
	SME_QOS_WMM_UP_VO = 6,
	SME_QOS_WMM_UP_NC = 7,
	SME_QOS_WMM_UP_MAX
};

/*
 * Enumeration of the various TSPEC ack policies.
 * From 802.11 WMM specification
 */
enum sme_qos_wmmack_policytype {
	SME_QOS_WMM_TS_ACK_POLICY_NORMAL_ACK = 0,
	SME_QOS_WMM_TS_ACK_POLICY_RESV1 = 1,
	SME_QOS_WMM_TS_ACK_POLICY_RESV2 = 2,    /* Reserved */
	SME_QOS_WMM_TS_ACK_POLICY_HT_IMMEDIATE_BLOCK_ACK = 3,

};

/*
 * TS Info field in the WMM TSPEC
 * See suggestive values above
 */
struct sme_qos_wmmts_infotype {
	uint8_t burst_size_defn;
	enum sme_qos_wmmack_policytype ack_policy;
	enum sme_qos_wmmuptype up;    /* User priority */
	uint8_t psb;            /* power-save bit */
	enum sme_qos_wmm_dir_type direction;    /* Direction */
	uint8_t tid;            /* TID : To be filled up by SME-QoS */
};

/* The WMM TSPEC Element (from the WMM spec)*/
struct sme_qos_wmmtspecinfo {
	struct sme_qos_wmmts_infotype ts_info;
	uint16_t nominal_msdu_size;
	uint16_t maximum_msdu_size;
	uint32_t min_service_interval;
	uint32_t max_service_interval;
	uint32_t inactivity_interval;
	uint32_t suspension_interval;
	uint32_t svc_start_time;
	uint32_t min_data_rate;
	uint32_t mean_data_rate;
	uint32_t peak_data_rate;
	uint32_t max_burst_size;
	uint32_t delay_bound;
	uint32_t min_phy_rate;
	uint16_t surplus_bw_allowance;
	uint16_t medium_time;
};

static inline enum sme_qos_wmmuptype qca_wlan_ac_to_sme_qos(u8 priority)
{
	switch (priority) {
	case QCA_WLAN_AC_BE:
		return SME_QOS_WMM_UP_BE;
	case QCA_WLAN_AC_BK:
		return SME_QOS_WMM_UP_BK;
	case QCA_WLAN_AC_VI:
		return SME_QOS_WMM_UP_VI;
	case QCA_WLAN_AC_VO:
		return SME_QOS_WMM_UP_VO;
	default:
		return SME_QOS_WMM_UP_BE;
	}
}

/* External APIs */
typedef QDF_STATUS (*sme_QosCallback)(mac_handle_t mac_handle, void *HDDcontext,
		struct sme_qos_wmmtspecinfo *pCurrentQoSInfo,
		enum sme_qos_statustype status, uint32_t QosFlowID);
enum sme_qos_statustype sme_qos_setup_req(mac_handle_t mac_handle,
					  uint32_t sessionId,
					  struct sme_qos_wmmtspecinfo *pQoSInfo,
					  sme_QosCallback QoSCallback,
					  void *HDDcontext,
					  enum sme_qos_wmmuptype UPType,
					  uint32_t *pQosFlowID);
enum sme_qos_statustype sme_qos_modify_req(mac_handle_t mac_handle,
		struct sme_qos_wmmtspecinfo *pQoSInfo, uint32_t QosFlowID);
enum sme_qos_statustype sme_qos_release_req(mac_handle_t mac_handle,
					    uint8_t session_id,
					    uint32_t QosFlowID);
bool sme_qos_is_ts_info_ack_policy_valid(mac_handle_t mac_handle,
					 struct sme_qos_wmmtspecinfo *pQoSInfo,
					 uint8_t sessionId);
void sme_qos_update_hand_off(uint8_t sessionId, bool updateHandOff);
QDF_STATUS sme_update_dsc_pto_up_mapping(mac_handle_t mac_handle,
		enum sme_qos_wmmuptype *dscpmapping, uint8_t sessionId);

QDF_STATUS sme_offload_qos_process_out_of_uapsd_mode(struct mac_context *mac_ctx,
		uint32_t session_id);
QDF_STATUS sme_offload_qos_process_into_uapsd_mode(struct mac_context *mac_ctx,
		uint32_t session_id);


#endif /* #if !defined( __SME_QOSAPI_H ) */
