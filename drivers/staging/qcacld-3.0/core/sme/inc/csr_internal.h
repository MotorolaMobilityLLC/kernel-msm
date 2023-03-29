/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
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
 *   \file csr_internal.h
 *
 *   Define internal data structure for MAC.
 */
#ifndef CSRINTERNAL_H__
#define CSRINTERNAL_H__

#include "qdf_status.h"
#include "qdf_lock.h"

#include "qdf_mc_timer.h"
#include "csr_support.h"
#include "cds_reg_service.h"
#include "wlan_scan_public_structs.h"
#include "csr_neighbor_roam.h"

#include "sir_types.h"
#include "wlan_mlme_public_struct.h"
#include "csr_host_scan_roam.h"

#define CSR_ROAM_SCAN_CHANNEL_SWITCH_TIME        3

/* No of sessions to be supported, and a session is for Infra, BT-AMP */
#define CSR_IS_SESSION_VALID(mac, sessionId) \
	((sessionId) < WLAN_MAX_VDEVS && \
	 (mac != NULL) && \
	 ((mac)->roam.roamSession != NULL) && \
	 (mac)->roam.roamSession[(sessionId)].sessionActive)

#define CSR_GET_SESSION(mac, sessionId) \
	(sessionId < WLAN_MAX_VDEVS ? \
	 &(mac)->roam.roamSession[(sessionId)] : NULL)

#define CSR_IS_SESSION_ANY(sessionId) (sessionId == SME_SESSION_ID_ANY)
#define CSR_IS_DFS_CH_ROAM_ALLOWED(mac_ctx) \
	( \
	  ((((mac_ctx)->mlme_cfg->lfr.roaming_dfs_channel) != \
	    ROAMING_DFS_CHANNEL_DISABLED) ? true : false) \
	)
#define CSR_IS_SELECT_5GHZ_MARGIN(mac) \
	( \
	  (((mac)->roam.configParam.nSelect5GHzMargin) ? true : false) \
	)
#define CSR_IS_ROAM_PREFER_5GHZ(mac)	\
	( \
	  ((mac)->mlme_cfg->lfr.roam_prefer_5ghz) \
	)
#define CSR_IS_ROAM_INTRA_BAND_ENABLED(mac) \
	( \
	  ((mac)->mlme_cfg->lfr.roam_intra_band) \
	)
#define CSR_IS_FASTROAM_IN_CONCURRENCY_INI_FEATURE_ENABLED(mac) \
	( \
	  ((mac)->mlme_cfg->lfr.enable_fast_roam_in_concurrency) \
	)
#define CSR_IS_CHANNEL_24GHZ(chnNum) \
	(((chnNum) > 0) && ((chnNum) <= 14))
/* Support for "Fast roaming" (i.e., ESE, LFR, or 802.11r.) */
#define CSR_BG_SCAN_OCCUPIED_CHANNEL_LIST_LEN 15

#define QDF_ROAM_INVOKE_TIMEOUT 10000 /* in msec */
/* Used to determine what to set to the MLME_DOT11_MODE */
enum csr_cfgdot11mode {
	eCSR_CFG_DOT11_MODE_ABG,
	eCSR_CFG_DOT11_MODE_11A,
	eCSR_CFG_DOT11_MODE_11B,
	eCSR_CFG_DOT11_MODE_11G,
	eCSR_CFG_DOT11_MODE_11N,
	eCSR_CFG_DOT11_MODE_11AC,
	eCSR_CFG_DOT11_MODE_11G_ONLY,
	eCSR_CFG_DOT11_MODE_11N_ONLY,
	eCSR_CFG_DOT11_MODE_11AC_ONLY,
	/* This value can never set to CFG. Its for CSR's internal use */
	eCSR_CFG_DOT11_MODE_AUTO,
	eCSR_CFG_DOT11_MODE_11AX,
	eCSR_CFG_DOT11_MODE_11AX_ONLY,
};

enum csr_scan_reason {
	eCsrScanForSsid,
};

enum csr_roam_reason {
	/* Roaming because we've not established the initial connection. */
	eCsrNoConnection,
	/* roaming because LIM reported a cap change in the associated AP. */
	eCsrCapsChange,
	/* roaming because someone asked us to Disassoc & stay disassociated. */
	eCsrForcedDisassoc,
	/* roaming because an 802.11 request was issued to the driver. */
	eCsrHddIssued,
	/* roaming because we need to force a Disassoc due to MIC failure */
	eCsrForcedDisassocMICFailure,
	eCsrHddIssuedReassocToSameAP,
	eCsrSmeIssuedReassocToSameAP,
	/* roaming because someone asked us to deauth and stay disassociated. */
	eCsrForcedDeauth,
	/* will be issued by Handoff logic to disconect from current AP */
	eCsrSmeIssuedDisassocForHandoff,
	/* will be issued by Handoff logic to join a new AP with same profile */
	eCsrSmeIssuedAssocToSimilarAP,
	eCsrStopBss,
	eCsrSmeIssuedFTReassoc,
	eCsrForcedDisassocSta,
	eCsrForcedDeauthSta,
	eCsrPerformPreauth,
	/* Roaming disabled from driver during connect/start BSS */
	ecsr_driver_disabled,
};

enum csr_roam_substate {
	eCSR_ROAM_SUBSTATE_NONE = 0,
	eCSR_ROAM_SUBSTATE_START_BSS_REQ,
	eCSR_ROAM_SUBSTATE_JOIN_REQ,
	eCSR_ROAM_SUBSTATE_REASSOC_REQ,
	eCSR_ROAM_SUBSTATE_DISASSOC_REQ,
	eCSR_ROAM_SUBSTATE_STOP_BSS_REQ,
	/* Continue the current roam command after disconnect */
	eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING,
	eCSR_ROAM_SUBSTATE_AUTH_REQ,
	eCSR_ROAM_SUBSTATE_CONFIG,
	eCSR_ROAM_SUBSTATE_DEAUTH_REQ,
	eCSR_ROAM_SUBSTATE_DISASSOC_NOTHING_TO_JOIN,
	eCSR_ROAM_SUBSTATE_DISASSOC_REASSOC_FAILURE,
	eCSR_ROAM_SUBSTATE_DISASSOC_FORCED,
	eCSR_ROAM_SUBSTATE_WAIT_FOR_KEY,
	eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF,
	eCSR_ROAM_SUBSTATE_JOINED_NO_TRAFFIC,
	eCSR_ROAM_SUBSTATE_JOINED_NON_REALTIME_TRAFFIC,
	eCSR_ROAM_SUBSTATE_JOINED_REALTIME_TRAFFIC,
	eCSR_ROAM_SUBSTATE_DISASSOC_STA_HAS_LEFT,
	/*  max is 15 unless the bitfield is expanded... */
};

enum csr_roam_state {
	eCSR_ROAMING_STATE_STOP = 0,
	eCSR_ROAMING_STATE_IDLE,
	eCSR_ROAMING_STATE_JOINING,
	eCSR_ROAMING_STATE_JOINED,
};

enum csr_join_state {
	eCsrContinueRoaming,
	eCsrStopRoaming,
	eCsrReassocToSelfNoCapChange,
	eCsrStopRoamingDueToConcurrency,

};

enum csr_roaming_reason {
	eCsrNotRoaming,
	eCsrLostlinkRoamingDisassoc,
	eCsrLostlinkRoamingDeauth,
	eCsrDynamicRoaming,
	eCsrReassocRoaming,
};

enum csr_roam_wmstatus_changetypes {
	eCsrDisassociated,
	eCsrDeauthenticated
};

enum csr_diagwlan_status_eventsubtype {
	eCSR_WLAN_STATUS_CONNECT = 0,
	eCSR_WLAN_STATUS_DISCONNECT
};

enum csr_diagwlan_status_eventreason {
	eCSR_REASON_UNSPECIFIED = 0,
	eCSR_REASON_USER_REQUESTED,
	eCSR_REASON_MIC_ERROR,
	eCSR_REASON_DISASSOC,
	eCSR_REASON_DEAUTH,
	eCSR_REASON_HANDOFF,
	eCSR_REASON_ROAM_SYNCH_IND,
	eCSR_REASON_ROAM_SYNCH_CNF,
	eCSR_REASON_ROAM_HO_FAIL,
};

struct csr_channel {
	uint8_t numChannels;
	uint32_t channel_freq_list[CFG_VALID_CHANNEL_LIST_LEN];
};

struct bss_config_param {
	eCsrMediaAccessType qosType;
	tSirMacSSid SSID;
	enum csr_cfgdot11mode uCfgDot11Mode;
	enum reg_wifi_band band;
	tAniAuthType authType;
	eCsrEncryptionType encType;
	uint32_t uShortSlotTime;
	uint32_t uHTSupport;
	uint32_t uPowerLimit;
	uint32_t uHeartBeatThresh;
	uint32_t uJoinTimeOut;
	tSirMacCapabilityInfo BssCap;
	bool f11hSupport;
	ePhyChanBondState cbMode;
};

struct csr_roamstart_bssparams {
	tSirMacSSid ssId;

	/*
	 * This is the BSSID for the party we want to
	 * join (only use for WDS).
	 */
	struct qdf_mac_addr bssid;
	tSirNwType sirNwType;
	ePhyChanBondState cbMode;
	tSirMacRateSet operationalRateSet;
	tSirMacRateSet extendedRateSet;
	uint32_t operation_chan_freq;
	struct ch_params ch_params;
	enum csr_cfgdot11mode uCfgDot11Mode;
	uint8_t privacy;
	bool fwdWPSPBCProbeReq;
	bool protEnabled;
	bool obssProtEnabled;
	tAniAuthType authType;
	uint16_t beaconInterval; /* If this is 0, SME'll fill in for caller */
	uint16_t ht_protection;
	uint32_t dtimPeriod;
	uint8_t ApUapsdEnable;
	uint8_t ssidHidden;
	uint8_t wps_state;
	enum QDF_OPMODE bssPersona;
	uint16_t nRSNIELength;  /* If 0, pRSNIE is ignored. */
	uint8_t *pRSNIE;        /* If not null, it has IE byte stream for RSN */
	/* Flag used to indicate update beaconInterval */
	bool updatebeaconInterval;
#ifdef WLAN_FEATURE_11W
	bool mfpCapable;
	bool mfpRequired;
#endif
	struct add_ie_params add_ie_params;
	uint8_t sap_dot11mc;
	uint16_t beacon_tx_rate;
	uint32_t cac_duration_ms;
	uint32_t dfs_regdomain;
};

struct roam_cmd {
	uint32_t roamId;
	enum csr_roam_reason roamReason;
	struct csr_roam_profile roamProfile;
	tScanResultHandle hBSSList;       /* BSS list fits the profile */
	/*
	 * point to the current BSS in the list that is roaming.
	 * It starts from head to tail
	 * */
	tListElem *pRoamBssEntry;

	/* the last BSS we try and failed */
	struct bss_description *pLastRoamBss;
	bool fReleaseBssList;             /* whether to free hBSSList */
	bool fReleaseProfile;             /* whether to free roamProfile */
	bool fReassoc;                    /* whether this cmd is for reassoc */
	/* whether mac->roam.pCurRoamProfile needs to be updated */
	bool fUpdateCurRoamProfile;
	/*
	 * this is for CSR internal used only. And it should not be assigned
	 * when creating the command. This causes the roam cmd not todo anything
	 */
	bool fReassocToSelfNoCapChange;

	bool fStopWds;
	tSirMacAddr peerMac;
	enum wlan_reason_code reason;
	enum wlan_reason_code disconnect_reason;
	qdf_time_t connect_active_time;
};

struct setkey_cmd {
	uint32_t roamId;
	eCsrEncryptionType encType;
	enum csr_akm_type authType;
	tAniKeyDirection keyDirection;  /* Tx, Rx or Tx-and-Rx */
	struct qdf_mac_addr peermac;    /* Peer's MAC address. ALL 1's for group key */
	uint8_t paeRole;        /* 0 for supplicant */
	uint8_t keyId;          /* Kye index */
	uint8_t keyLength;      /* Number of bytes containing the key in pKey */
	uint8_t Key[CSR_MAX_KEY_LEN];
	uint8_t keyRsc[WLAN_CRYPTO_RSC_SIZE];
};

struct wmstatus_changecmd {
	enum csr_roam_wmstatus_changetypes Type;
	union {
		struct deauth_ind DeauthIndMsg;
		struct disassoc_ind DisassocIndMsg;
	} u;

};

struct delstafor_sessionCmd {
	/* Session self mac addr */
	tSirMacAddr self_mac_addr;
	csr_session_close_cb session_close_cb;
	void *context;
};

/*
 * Neighbor Report Params Bitmask
 */
#define NEIGHBOR_REPORT_PARAMS_TIME_OFFSET            0x01
#define NEIGHBOR_REPORT_PARAMS_LOW_RSSI_OFFSET        0x02
#define NEIGHBOR_REPORT_PARAMS_BMISS_COUNT_TRIGGER    0x04
#define NEIGHBOR_REPORT_PARAMS_PER_THRESHOLD_OFFSET   0x08
#define NEIGHBOR_REPORT_PARAMS_CACHE_TIMEOUT          0x10
#define NEIGHBOR_REPORT_PARAMS_MAX_REQ_CAP            0x20
#define NEIGHBOR_REPORT_PARAMS_ALL                    0x3F

struct csr_config {
	uint32_t channelBondingMode24GHz;
	uint32_t channelBondingMode5GHz;
	eCsrPhyMode phyMode;
	enum csr_cfgdot11mode uCfgDot11Mode;
	uint32_t HeartbeatThresh50;
	uint32_t HeartbeatThresh24;
	eCsrRoamWmmUserModeType WMMSupportMode;
	bool Is11eSupportEnabled;
	bool ProprietaryRatesEnabled;
	bool fenableMCCMode;
	bool mcc_rts_cts_prot_enable;
	bool mcc_bcast_prob_resp_enable;
	uint8_t fAllowMCCGODiffBI;
	uint8_t bCatRssiOffset; /* to set RSSI difference for each category */
	bool nRoamScanControl;

	/*
	 * Remove this code once SLM_Sessionization is supported
	 * BMPS_WORKAROUND_NOT_NEEDED
	 */
	bool doBMPSWorkaround;
	uint32_t nVhtChannelWidth;
	bool send_smps_action;
#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	uint8_t cc_switch_mode;
#endif
	bool obssEnabled;
	uint8_t conc_custom_rule1;
	uint8_t conc_custom_rule2;
	uint8_t is_sta_connection_in_5gz_enabled;
	struct roam_ext_params roam_params;
	struct csr_sta_roam_policy_params sta_roam_policy;
	enum force_1x1_type is_force_1x1;
	uint32_t offload_11k_enable_bitmask;
	bool wep_tkip_in_he;
	struct csr_neighbor_report_offload_params neighbor_report_offload;
};

struct csr_channel_powerinfo {
	tListElem link;
	uint32_t first_chan_freq;
	uint8_t numChannels;
	uint8_t txPower;
	uint8_t interChannelOffset;
};

struct csr_roam_joinstatus {
	tSirResultCodes status_code;
	/*
	 * this is set to unspecified if status_code indicates timeout.
	 * Or it is the failed reason from the other BSS(per 802.11 spec)
	 */
	uint32_t reasonCode;
	tSirMacAddr bssId;
};

struct csr_scanstruct {
	tSirScanType curScanType;
	struct csr_channel channels11d;
	struct channel_power defaultPowerTable[CFG_VALID_CHANNEL_LIST_LEN];
	uint32_t numChannelsDefault;
	struct csr_channel base_channels;  /* The channel base to work on */
	tDblLinkList channelPowerInfoList24;
	tDblLinkList channelPowerInfoList5G;
	uint32_t nLastAgeTimeOut;
	uint32_t nAgingCountDown;
	uint8_t countryCodeDefault[REG_ALPHA2_LEN + 1];
	uint8_t countryCodeCurrent[REG_ALPHA2_LEN + 1];
	uint8_t countryCode11d[REG_ALPHA2_LEN + 1];
	v_REGDOMAIN_t domainIdDefault;  /* default regulatory domain */
	v_REGDOMAIN_t domainIdCurrent;  /* current regulatory domain */

	/*
	 * in 11d IE from probe rsp or beacons of neighboring APs
	 * will use the most popular one (max count)
	 */
	uint8_t countryCodeElected[REG_ALPHA2_LEN + 1];
	/*
	 * Customer wants to optimize the scan time. Avoiding scans(passive)
	 * on DFS channels while swipping through both bands can save some time
	 * (apprx 1.3 sec)
	 */
	uint8_t fEnableDFSChnlScan;
	bool fDropScanCmd;      /* true means we don't accept scan commands */

	/* This includes all channels on which candidate APs are found */
	struct csr_channel occupiedChannels[WLAN_MAX_VDEVS];
	int8_t roam_candidate_count[WLAN_MAX_VDEVS];
	int8_t inScanResultBestAPRssi;
	bool fcc_constraint;
	bool pending_channel_list_req;
	wlan_scan_requester requester_id;
};

/*
 * Save the connected information. This structure + connectedProfile
 * should contain all information about the connection
 */
struct csr_roam_connectedinfo {
	uint32_t nBeaconLength;
	uint32_t nAssocReqLength;
	uint32_t nAssocRspLength;
	/* len of the parsed RIC resp IEs received in reassoc response */
	uint32_t nRICRspLength;
#ifdef FEATURE_WLAN_ESE
	uint32_t nTspecIeLength;
#endif
	/*
	 * Point to a buffer contain the beacon, assoc req, assoc rsp frame, in
	 * that order user needs to use nBeaconLength, nAssocReqLength,
	 * nAssocRspLength to desice where each frame starts and ends.
	 */
	uint8_t *pbFrames;
	uint8_t staId;
};

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
enum csr_roamoffload_authstatus {
	/* reassociation is done but couldn't finish security handshake */
	eSIR_ROAM_AUTH_STATUS_CONNECTED = 1,
	/* roam successfully completed by firmware */
	eSIR_ROAM_AUTH_STATUS_AUTHENTICATED = 2,
	/* unknown error */
	eSIR_ROAM_AUTH_STATUS_UNKNOWN = 0xff
};
#endif

struct csr_roam_stored_profile {
	uint32_t session_id;
	struct csr_roam_profile profile;
	tScanResultHandle bsslist_handle;
	enum csr_roam_reason reason;
	uint32_t roam_id;
	bool imediate_flag;
	bool clear_flag;
};

/**
 * struct scan_cmd_info - Scan cache entry node
 * @scan_id: scan id
 * @scan_reason: scan reason
 * @profile: roam profile
 * @roam_id: Roam id
 * @roambssentry: scan entries
 */
struct scan_cmd_info {
	wlan_scan_id scan_id;
	enum csr_scan_reason scan_reason;
	struct csr_roam_profile *profile;
	uint32_t roam_id;
	tListElem *roambssentry;
};

/**
 * struct csr_disconnect_stats - Disconnect Stats per session
 * @disconnection_cnt: total no. of disconnections
 * @disconnection_by_app: diconnections triggered by application
 * @disassoc_by_peer: disassoc sent by peer
 * @deauth_by_peer: deauth sent by peer
 * @bmiss: disconnect triggered by beacon miss
 * @peer_kickout: disconnect triggered by peer kickout
 */
struct csr_disconnect_stats {
	uint32_t disconnection_cnt;
	uint32_t disconnection_by_app;
	uint32_t disassoc_by_peer;
	uint32_t deauth_by_peer;
	uint32_t bmiss;
	uint32_t peer_kickout;
};

/**
 * struct csr_roam_session - CSR per-vdev context
 * @vdev_id: ID of the vdev for which this entry is applicable
 * @is_bcn_recv_start: Allow to process bcn recv indication
 * @beacon_report_do_not_resume: Do not resume the beacon reporting after scan
 */
struct csr_roam_session {
	union {
		uint8_t sessionId;
		uint8_t vdev_id;
	};
	bool sessionActive;     /* true if it is used */

	/* For BT-AMP station, this serve as BSSID for self-BSS. */
	struct qdf_mac_addr self_mac_addr;

	eCsrConnectState connectState;
	tCsrRoamConnectedProfile connectedProfile;
	struct csr_roam_connectedinfo connectedInfo;
	struct csr_roam_connectedinfo prev_assoc_ap_info;
	struct csr_roam_profile *pCurRoamProfile;
	struct bss_description *pConnectBssDesc;
	uint16_t NumPmkidCache; /* valid number of pmkid in the cache*/
	uint16_t curr_cache_idx; /* the index in pmkidcache to write next to */
	tPmkidCacheInfo PmkidCacheInfo[CSR_MAX_PMKID_ALLOWED];
	uint8_t cJoinAttemps;
	int32_t sPendingCommands;   /* 0 means CSR is ok to low power */
#ifdef FEATURE_WLAN_WAPI
	uint16_t NumBkidCache;
	tBkidCacheInfo BkidCacheInfo[CSR_MAX_BKID_ALLOWED];
#endif /* FEATURE_WLAN_WAPI */
	/*
	 * indicate whether CSR is roaming
	 * (either via lostlink or dynamic roaming)
	 */
	bool fRoaming;
	/*
	 * to remember some parameters needed for START_BSS.
	 * All member must be set every time we try to join
	 */
	struct csr_roamstart_bssparams bssParams;
	/* the byte count of pWpaRsnIE; */
	uint32_t nWpaRsnReqIeLength;
	/* contain the WPA/RSN IE in assoc req */
	uint8_t *pWpaRsnReqIE;
#ifdef FEATURE_WLAN_WAPI
	/* the byte count of pWapiReqIE; */
	uint32_t nWapiReqIeLength;
	/* this contain the WAPI IE in assoc req */
	uint8_t *pWapiReqIE;
#endif /* FEATURE_WLAN_WAPI */
	uint32_t nAddIEScanLength;      /* the byte count of pAddIeScanIE; */
	/* contains the additional IE in (unicast) probe req at time of join */
	uint8_t *pAddIEScan;
	uint32_t nAddIEAssocLength;     /* the byte count for pAddIeAssocIE */
	uint8_t *pAddIEAssoc;
	struct csr_timer_info roamingTimerInfo;
	enum csr_roaming_reason roamingReason;
	bool fCancelRoaming;
	qdf_mc_timer_t hTimerRoaming;
#ifdef WLAN_BCN_RECV_FEATURE
	bool is_bcn_recv_start;
	bool beacon_report_do_not_resume;
#endif
	/* the roamResult that is used when the roaming timer fires */
	eCsrRoamResult roamResult;
	/* This is the reason code for join(assoc) failure */
	struct csr_roam_joinstatus joinFailStatusCode;
	/* status from PE for deauth/disassoc(lostlink) or our own dyn roam */
	uint32_t roamingStatusCode;
	uint16_t NumPmkidCandidate;
	tPmkidCandidateInfo PmkidCandidateInfo[CSR_MAX_PMKID_ALLOWED];
	bool fWMMConnection;
	bool fQOSConnection;
#ifdef FEATURE_WLAN_ESE
	tCsrEseCckmInfo eseCckmInfo;
	bool isPrevApInfoValid;
	tSirMacSSid prevApSSID;
	struct qdf_mac_addr prevApBssid;
	uint16_t clientDissSecs;
	uint32_t roamTS1;
	tCsrEseCckmIe suppCckmIeInfo;
#endif
	uint8_t bRefAssocStartCnt;      /* Tracking assoc start indication */
	struct ht_config ht_config;
	struct sir_vht_config vht_config;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	uint8_t psk_pmk[SIR_ROAM_SCAN_PSK_SIZE];
	size_t pmk_len;
	struct roam_offload_synch_ind *roam_synch_data;
	struct pmkid_mode_bits pmkid_modes;
#endif
	tftSMEContext ftSmeContext;
	/* This count represents the number of bssid's we try to join. */
	uint8_t join_bssid_count;
	struct csr_roam_stored_profile stored_roam_profile;
	bool ch_switch_in_progress;
	bool supported_nss_1x1;
	uint8_t vdev_nss;
	uint8_t nss;
	bool nss_forced_1x1;
	bool disable_hi_rssi;
	bool dhcp_done;
	enum wlan_reason_code disconnect_reason;
	uint8_t uapsd_mask;
	struct scan_cmd_info scan_info;
	qdf_mc_timer_t roaming_offload_timer;
	bool is_fils_connection;
	uint16_t fils_seq_num;
	bool discon_in_progress;
	bool is_adaptive_11r_connection;
	struct csr_disconnect_stats disconnect_stats;
	qdf_mc_timer_t join_retry_timer;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	qdf_mc_timer_t roam_invoke_timer;
	struct csr_timer_info roam_invoke_timer_info;
#endif
};

struct csr_roamstruct {
	uint32_t nextRoamId;
	struct csr_config configParam;
	enum csr_roam_state curState[WLAN_MAX_VDEVS];
	enum csr_roam_substate curSubState[WLAN_MAX_VDEVS];
	/*
	 * This may or may not have the up-to-date valid channel list. It is
	 * used to get CFG_VALID_CHANNEL_LIST and not alloc mem all time
	 */
	uint32_t valid_ch_freq_list[CFG_VALID_CHANNEL_LIST_LEN];
	uint32_t numValidChannels;       /* total number of channels in CFG */
	int32_t sPendingCommands;
	qdf_mc_timer_t hTimerWaitForKey; /* support timeout for WaitForKey */
	struct csr_timer_info WaitForKeyTimerInfo;
	struct csr_roam_session *roamSession;
	tCsrNeighborRoamControlInfo neighborRoamInfo[WLAN_MAX_VDEVS];
	uint8_t isFastRoamIniFeatureEnabled;
#ifdef FEATURE_WLAN_ESE
	uint8_t isEseIniFeatureEnabled;
#endif
	uint8_t RoamRssiDiff;
	bool isWESModeEnabled;
	uint32_t deauthRspStatus;
#if defined(WLAN_LOGGING_SOCK_SVC_ENABLE) && \
	defined(FEATURE_PKTLOG) && !defined(REMOVE_PKT_LOG)
	qdf_mc_timer_t packetdump_timer;
#endif
	spinlock_t roam_state_lock;
};

#define GET_NEXT_ROAM_ID(pRoamStruct)  (((pRoamStruct)->nextRoamId + 1 == 0) ? \
			1 : (pRoamStruct)->nextRoamId)
#define CSR_IS_ROAM_STATE(mac, state, sessionId) \
			((state) == (mac)->roam.curState[sessionId])
#define CSR_IS_ROAM_STOP(mac, sessionId) \
		CSR_IS_ROAM_STATE((mac), eCSR_ROAMING_STATE_STOP, sessionId)
#define CSR_IS_ROAM_INIT(mac, sessionId) \
		 CSR_IS_ROAM_STATE((mac), eCSR_ROAMING_STATE_INIT, sessionId)
#define CSR_IS_ROAM_JOINING(mac, sessionId)  \
		CSR_IS_ROAM_STATE(mac, eCSR_ROAMING_STATE_JOINING, sessionId)
#define CSR_IS_ROAM_IDLE(mac, sessionId) \
		CSR_IS_ROAM_STATE(mac, eCSR_ROAMING_STATE_IDLE, sessionId)
#define CSR_IS_ROAM_JOINED(mac, sessionId) \
		CSR_IS_ROAM_STATE(mac, eCSR_ROAMING_STATE_JOINED, sessionId)
#define CSR_IS_ROAM_SUBSTATE(mac, subState, sessionId) \
		((subState) == (mac)->roam.curSubState[sessionId])
#define CSR_IS_ROAM_SUBSTATE_JOIN_REQ(mac, sessionId) \
	CSR_IS_ROAM_SUBSTATE((mac), eCSR_ROAM_SUBSTATE_JOIN_REQ, sessionId)
#define CSR_IS_ROAM_SUBSTATE_AUTH_REQ(mac, sessionId) \
	CSR_IS_ROAM_SUBSTATE((mac), eCSR_ROAM_SUBSTATE_AUTH_REQ, sessionId)
#define CSR_IS_ROAM_SUBSTATE_REASSOC_REQ(mac, sessionId) \
	CSR_IS_ROAM_SUBSTATE((mac), eCSR_ROAM_SUBSTATE_REASSOC_REQ, sessionId)
#define CSR_IS_ROAM_SUBSTATE_DISASSOC_REQ(mac, sessionId) \
	CSR_IS_ROAM_SUBSTATE((mac), eCSR_ROAM_SUBSTATE_DISASSOC_REQ, sessionId)
#define CSR_IS_ROAM_SUBSTATE_DISASSOC_NO_JOIN(mac, sessionId) \
	CSR_IS_ROAM_SUBSTATE((mac), \
		eCSR_ROAM_SUBSTATE_DISASSOC_NOTHING_TO_JOIN, sessionId)
#define CSR_IS_ROAM_SUBSTATE_REASSOC_FAIL(mac, sessionId) \
		CSR_IS_ROAM_SUBSTATE((mac), \
			eCSR_ROAM_SUBSTATE_DISASSOC_REASSOC_FAILURE, sessionId)
#define CSR_IS_ROAM_SUBSTATE_DISASSOC_FORCED(mac, sessionId) \
		CSR_IS_ROAM_SUBSTATE((mac), \
			eCSR_ROAM_SUBSTATE_DISASSOC_FORCED, sessionId)
#define CSR_IS_ROAM_SUBSTATE_DEAUTH_REQ(mac, sessionId) \
		CSR_IS_ROAM_SUBSTATE((mac), \
			eCSR_ROAM_SUBSTATE_DEAUTH_REQ, sessionId)
#define CSR_IS_ROAM_SUBSTATE_START_BSS_REQ(mac, sessionId) \
		CSR_IS_ROAM_SUBSTATE((mac), \
			eCSR_ROAM_SUBSTATE_START_BSS_REQ, sessionId)
#define CSR_IS_ROAM_SUBSTATE_STOP_BSS_REQ(mac, sessionId) \
		CSR_IS_ROAM_SUBSTATE((mac), \
			eCSR_ROAM_SUBSTATE_STOP_BSS_REQ, sessionId)
#define CSR_IS_ROAM_SUBSTATE_DISCONNECT_CONTINUE(mac, sessionId) \
		CSR_IS_ROAM_SUBSTATE((mac), \
		eCSR_ROAM_SUBSTATE_DISCONNECT_CONTINUE_ROAMING, sessionId)
#define CSR_IS_ROAM_SUBSTATE_CONFIG(mac, sessionId) \
		CSR_IS_ROAM_SUBSTATE((mac), \
		eCSR_ROAM_SUBSTATE_CONFIG, sessionId)
#define CSR_IS_ROAM_SUBSTATE_WAITFORKEY(mac, sessionId) \
		CSR_IS_ROAM_SUBSTATE((mac), \
			eCSR_ROAM_SUBSTATE_WAIT_FOR_KEY, sessionId)
#define CSR_IS_ROAM_SUBSTATE_DISASSOC_HO(mac, sessionId) \
		CSR_IS_ROAM_SUBSTATE((mac), \
			eCSR_ROAM_SUBSTATE_DISASSOC_HANDOFF, sessionId)
#define CSR_IS_ROAM_SUBSTATE_HO_NT(mac, sessionId) \
		CSR_IS_ROAM_SUBSTATE((mac), \
			eCSR_ROAM_SUBSTATE_JOINED_NO_TRAFFIC, sessionId)
#define CSR_IS_ROAM_SUBSTATE_HO_NRT(mac, sessionId)  \
			CSR_IS_ROAM_SUBSTATE((mac), \
				eCSR_ROAM_SUBSTATE_JOINED_NON_REALTIME_TRAFFIC,\
					sessionId)
#define CSR_IS_ROAM_SUBSTATE_HO_RT(mac, sessionId) \
			CSR_IS_ROAM_SUBSTATE((mac),\
			eCSR_ROAM_SUBSTATE_JOINED_REALTIME_TRAFFIC, sessionId)
#define CSR_IS_PHY_MODE_B_ONLY(mac) \
	((eCSR_DOT11_MODE_11b == (mac)->roam.configParam.phyMode) || \
	 (eCSR_DOT11_MODE_11b_ONLY == (mac)->roam.configParam.phyMode))

#define CSR_IS_PHY_MODE_G_ONLY(mac) \
	(eCSR_DOT11_MODE_11g == (mac)->roam.configParam.phyMode \
		|| eCSR_DOT11_MODE_11g_ONLY == (mac)->roam.configParam.phyMode)

#define CSR_IS_PHY_MODE_A_ONLY(mac) \
	(eCSR_DOT11_MODE_11a == (mac)->roam.configParam.phyMode)

#define CSR_IS_PHY_MODE_DUAL_BAND(phyMode) \
	((eCSR_DOT11_MODE_abg & (phyMode)) || \
	 (eCSR_DOT11_MODE_11n & (phyMode)) || \
	 (eCSR_DOT11_MODE_11ac & (phyMode)) || \
	 (eCSR_DOT11_MODE_11ax & (phyMode)) || \
	 (eCSR_DOT11_MODE_AUTO & (phyMode)))

#define CSR_IS_PHY_MODE_11n(phy_mode) \
	((eCSR_DOT11_MODE_11n == phy_mode) || \
	 (eCSR_DOT11_MODE_11n_ONLY == phy_mode) || \
	 (eCSR_DOT11_MODE_11ac == phy_mode) || \
	 (eCSR_DOT11_MODE_11ac_ONLY == phy_mode))

#define CSR_IS_PHY_MODE_11ac(phy_mode) \
	((eCSR_DOT11_MODE_11ac == phy_mode) || \
	 (eCSR_DOT11_MODE_11ac_ONLY == phy_mode))

#define CSR_IS_DOT11_MODE_11N(dot11mode) \
	((dot11mode == eCSR_CFG_DOT11_MODE_AUTO) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11N) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11AC) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11N_ONLY) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11AC_ONLY) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11AX) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11AX_ONLY))

#define CSR_IS_DOT11_MODE_11AC(dot11mode) \
	((dot11mode == eCSR_CFG_DOT11_MODE_AUTO) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11AC) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11AC_ONLY) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11AX) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11AX_ONLY))

#define CSR_IS_DOT11_MODE_11AX(dot11mode) \
	((dot11mode == eCSR_CFG_DOT11_MODE_AUTO) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11AX) || \
	 (dot11mode == eCSR_CFG_DOT11_MODE_11AX_ONLY))
/*
 * this function returns true if the NIC is operating exclusively in
 * the 2.4 GHz band, meaning. it is NOT operating in the 5.0 GHz band.
 */
#define CSR_IS_24_BAND_ONLY(mac) \
	(BIT(REG_BAND_2G) == (mac)->mlme_cfg->gen.band)

#define CSR_IS_5G_BAND_ONLY(mac) \
	(BIT(REG_BAND_5G) == (mac)->mlme_cfg->gen.band)

#define CSR_IS_RADIO_DUAL_BAND(mac) \
	((BIT(REG_BAND_2G) | BIT(REG_BAND_5G)) == \
		(mac)->mlme_cfg->gen.band_capability)

#define CSR_IS_RADIO_BG_ONLY(mac) \
	(BIT(REG_BAND_2G) == (mac)->mlme_cfg->gen.band_capability)

/*
 * this function returns true if the NIC is operating exclusively in the 5.0 GHz
 * band, meaning. it is NOT operating in the 2.4 GHz band
 */
#define CSR_IS_RADIO_A_ONLY(mac) \
	(BAND_5G == (mac)->mlme_cfg->gen.band_capability)
/* this function returns true if the NIC is operating in both bands. */
#define CSR_IS_OPEARTING_DUAL_BAND(mac) \
	((BAND_ALL == (mac)->mlme_cfg->gen.band_capability) && \
		(BAND_ALL == (mac)->mlme_cfg->gen.band))
/*
 * this function returns true if the NIC can operate in the 5.0 GHz band
 * (could operate in the 2.4 GHz band also)
 */
#define CSR_IS_OPERATING_A_BAND(mac) \
	(CSR_IS_OPEARTING_DUAL_BAND((mac)) || \
		CSR_IS_RADIO_A_ONLY((mac)) || CSR_IS_5G_BAND_ONLY((mac)))

/*
 * this function returns true if the NIC can operate in the 2.4 GHz band
 * (could operate in the 5.0 GHz band also).
 */
#define CSR_IS_OPERATING_BG_BAND(mac) \
	(CSR_IS_OPEARTING_DUAL_BAND((mac)) || \
		CSR_IS_RADIO_BG_ONLY((mac)) || CSR_IS_24_BAND_ONLY((mac)))

#define CSR_IS_ROAMING(pSession) \
	((CSR_IS_LOSTLINK_ROAMING((pSession)->roamingReason)) || \
		(eCsrDynamicRoaming == (pSession)->roamingReason)  ||	\
		(eCsrReassocRoaming == (pSession)->roamingReason))
#define CSR_IS_ADDTS_WHEN_ACMOFF_SUPPORTED(mac) \
	(mac->mlme_cfg->wmm_params.wmm_tspec_element.ts_acm_is_off)
#define CSR_IS_LOSTLINK_ROAMING(reason) \
	((eCsrLostlinkRoamingDisassoc == (reason)) || \
		(eCsrLostlinkRoamingDeauth == (reason)))

#ifdef FEATURE_LFR_SUBNET_DETECTION
/* bit-4 and bit-5 indicate the subnet status */
#define CSR_GET_SUBNET_STATUS(roam_reason) (((roam_reason) & 0x30) >> 4)
#else
#define CSR_GET_SUBNET_STATUS(roam_reason) (0)
#endif

/**
 * csr_get_vdev_dot11_mode() - get the supported dot11mode by vdev
 * @mac_ctx:  pointer to global mac structure
 * @device_mode: vdev mode
 * @curr_dot11_mode: Current dot11 mode
 *
 * The function return the min of supported dot11 mode and vdev type dot11mode
 * for given vdev type.
 *
 * Return:csr_cfgdot11mode
 */
enum csr_cfgdot11mode
csr_get_vdev_dot11_mode(struct mac_context *mac,
			enum QDF_OPMODE device_mode,
			enum csr_cfgdot11mode curr_dot11_mode);

QDF_STATUS csr_get_channel_and_power_list(struct mac_context *mac);

QDF_STATUS csr_scan_filter_results(struct mac_context *mac);

QDF_STATUS csr_set_modify_profile_fields(struct mac_context *mac,
		uint32_t sessionId, tCsrRoamModifyProfileFields *
		pModifyProfileFields);
QDF_STATUS csr_get_modify_profile_fields(struct mac_context *mac,
		uint32_t sessionId, tCsrRoamModifyProfileFields *
		pModifyProfileFields);
void csr_set_global_cfgs(struct mac_context *mac);
void csr_set_default_dot11_mode(struct mac_context *mac);
bool csr_is_conn_state_disconnected(struct mac_context *mac,
						   uint32_t sessionId);
bool csr_is_conn_state_connected_infra(struct mac_context *mac,
							uint32_t sessionId);
bool csr_is_conn_state_connected(struct mac_context *mac,
					       uint32_t sessionId);
bool csr_is_conn_state_infra(struct mac_context *mac,
					uint32_t sessionId);

bool csr_is_conn_state_wds(struct mac_context *mac, uint32_t sessionId);
bool csr_is_conn_state_connected_wds(struct mac_context *mac,
						    uint32_t sessionId);
bool csr_is_conn_state_disconnected_wds(struct mac_context *mac,
		uint32_t sessionId);
bool csr_is_any_session_in_connect_state(struct mac_context *mac);
bool csr_is_all_session_disconnected(struct mac_context *mac);

/**
 * csr_get_connected_infra() - get the session id of the connected infra
 * @mac_ctx:  pointer to global mac structure
 *
 * The function check if any infra is present in connected state and if present
 * return the session id of the connected infra else if no infra is in connected
 * state return WLAN_UMAC_VDEV_ID_MAX
 *
 * Return: session id of the connected infra
 */
uint8_t csr_get_connected_infra(struct mac_context *mac_ctx);
bool csr_is_concurrent_session_running(struct mac_context *mac);
bool csr_is_infra_ap_started(struct mac_context *mac);
bool csr_is_conn_state_connected_infra_ap(struct mac_context *mac,
		uint32_t sessionId);
QDF_STATUS csr_get_snr(struct mac_context *mac, tCsrSnrCallback callback,
			  struct qdf_mac_addr bssId, void *pContext);
QDF_STATUS csr_get_config_param(struct mac_context *mac,
					  struct csr_config_params *pParam);
QDF_STATUS csr_change_default_config_param(struct mac_context *mac,
		struct csr_config_params *pParam);
QDF_STATUS csr_msg_processor(struct mac_context *mac, void *msg_buf);
QDF_STATUS csr_open(struct mac_context *mac);
QDF_STATUS csr_init_chan_list(struct mac_context *mac, uint8_t *alpha2);
QDF_STATUS csr_close(struct mac_context *mac);
QDF_STATUS csr_start(struct mac_context *mac);
QDF_STATUS csr_stop(struct mac_context *mac);
QDF_STATUS csr_ready(struct mac_context *mac);

#ifdef FEATURE_WLAN_WAPI
uint8_t csr_construct_wapi_ie(struct mac_context *mac, uint32_t sessionId,
		struct csr_roam_profile *pProfile,
		struct bss_description *pSirBssDesc,
		tDot11fBeaconIEs *pIes, tCsrWapiIe *pWapiIe);
#endif /* FEATURE_WLAN_WAPI */

void csr_set_cfg_privacy(struct mac_context *mac,
			 struct csr_roam_profile *pProfile,
			 bool fPrivacy);

/**
 * csr_get_infra_operation_chan_freq() - get operating chan freq of
 * given vdev id
 * @mac_ctx: Pointer to mac context
 * @vdev_id: vdev id
 *
 * Return: chan freq of given vdev id
 */
uint32_t csr_get_infra_operation_chan_freq(
	struct mac_context *mac, uint8_t vdev_id);

bool csr_is_session_client_and_connected(struct mac_context *mac,
		uint8_t sessionId);
/**
 * csr_get_concurrent_operation_freq() - To get concurrent operating freq
 * @mac_ctx: Pointer to mac context
 *
 * This routine will return operating freq on FIRST BSS that is
 * active/operating to be used for concurrency mode.
 * If other BSS is not up or not connected it will return 0
 *
 * Return: uint32_t
 */
uint32_t csr_get_concurrent_operation_freq(struct mac_context *mac_ctx);

/**
 * csr_get_beaconing_concurrent_channel() - To get concurrent operating channel
 * frequency of beaconing interface
 * @mac_ctx: Pointer to mac context
 * @vdev_id_to_skip: channel of which vdev id to skip
 *
 * This routine will return operating channel of active AP/GO channel
 * and will skip the channel of vdev_id_to_skip.
 * If other no reqested mode is active it will return 0
 *
 * Return: uint32_t
 */
uint32_t csr_get_beaconing_concurrent_channel(struct mac_context *mac_ctx,
					      uint8_t vdev_id_to_skip);

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
uint16_t csr_check_concurrent_channel_overlap(
		struct mac_context *mac,
		uint32_t sap_ch_freq, eCsrPhyMode sap_phymode,
		uint8_t cc_switch_mode);
#endif
QDF_STATUS csr_roam_copy_connect_profile(struct mac_context *mac,
		uint32_t sessionId, tCsrRoamConnectedProfile *pProfile);
bool csr_is_set_key_allowed(struct mac_context *mac, uint32_t sessionId);

/* Returns whether the current association is a 11r assoc or not */
bool csr_roam_is11r_assoc(struct mac_context *mac, uint8_t sessionId);

#ifdef FEATURE_WLAN_ESE
/* Returns whether the current association is a ESE assoc or not */
bool csr_roam_is_ese_assoc(struct mac_context *mac, uint32_t sessionId);
bool csr_roam_is_ese_ini_feature_enabled(struct mac_context *mac);
QDF_STATUS csr_get_tsm_stats(struct mac_context *mac,
		tCsrTsmStatsCallback callback,
		struct qdf_mac_addr bssId,
		void *pContext, uint8_t tid);
#endif

/* Returns whether "Legacy Fast Roaming" is enabled...or not */
bool csr_roam_is_fast_roam_enabled(struct mac_context *mac,
						uint32_t sessionId);
bool csr_roam_is_roam_offload_scan_enabled(
	struct mac_context *mac);
bool csr_is_channel_present_in_list(uint32_t *pChannelList,
				    int numChannels, uint32_t chan_freq);
QDF_STATUS csr_add_to_channel_list_front(uint32_t *pChannelList,
					 int numChannels, uint32_t chan_freq);
#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)
QDF_STATUS csr_roam_offload_scan_rsp_hdlr(struct mac_context *mac,
		struct roam_offload_scan_rsp *scanOffloadRsp);
#else
static inline QDF_STATUS csr_roam_offload_scan_rsp_hdlr(
		struct mac_context *mac,
		struct roam_offload_scan_rsp *scanOffloadRsp)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif
QDF_STATUS csr_handoff_request(struct mac_context *mac, uint8_t sessionId,
		tCsrHandoffRequest
		*pHandoffInfo);
bool csr_roam_is_sta_mode(struct mac_context *mac, uint32_t sessionId);

/* Post Channel Change Indication */
QDF_STATUS csr_roam_channel_change_req(struct mac_context *mac,
					struct qdf_mac_addr
				       bssid, struct ch_params *ch_params,
				       struct csr_roam_profile *profile);

/* Post Beacon Tx Start Indication */
QDF_STATUS csr_roam_start_beacon_req(struct mac_context *mac,
		struct qdf_mac_addr bssid, uint8_t dfsCacWaitStatus);

QDF_STATUS csr_roam_send_chan_sw_ie_request(struct mac_context *mac,
					    struct qdf_mac_addr bssid,
					    uint32_t target_chan_freq,
					    uint8_t csaIeReqd,
					    struct ch_params *ch_params);
QDF_STATUS csr_roam_modify_add_ies(struct mac_context *mac,
					tSirModifyIE *pModifyIE,
				   eUpdateIEsType updateType);
QDF_STATUS
csr_roam_update_add_ies(struct mac_context *mac,
		tSirUpdateIE *pUpdateIE, eUpdateIEsType updateType);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * csr_scan_save_roam_offload_ap_to_scan_cache() - This function parses the
 * received beacon/probe response from the firmware as part of the roam synch
 * indication. The beacon or the probe response is parsed and is also
 * saved into the scan cache
 *
 * @mac:  mac Pointer to Global Mac
 * @roam_sync_ind_ptr:  Roam Synch Indication from firmware
 *
 * @Return QDF_STATUS
 */
QDF_STATUS
csr_rso_save_ap_to_scan_cache(struct mac_context *mac,
			      struct roam_offload_synch_ind *roam_synch_ind,
			      struct bss_description *bss_desc_ptr);

/**
 * csr_process_ho_fail_ind  - This function will process the Hand Off Failure
 * indication received from the firmware. It will trigger a disconnect on
 * the session which the firmware reported a hand off failure.
 * @mac:     Pointer to global Mac
 * @msg_buf: Pointer to wma Ho fail indication message
 *
 * Return: None
 */
void csr_process_ho_fail_ind(struct mac_context *mac, void *msg_buf);
#endif
#ifdef FEATURE_WLAN_DIAG_SUPPORT_CSR
void csr_roaming_report_diag_event(struct mac_context *mac_ctx,
		struct roam_offload_synch_ind *roam_synch_ind_ptr,
		enum csr_diagwlan_status_eventreason reason);
#else
static inline void csr_roaming_report_diag_event(
		struct mac_context *mac_ctx,
		struct roam_offload_synch_ind *roam_synch_ind_ptr,
		enum csr_diagwlan_status_eventreason reason)
{}
#endif

QDF_STATUS csr_get_channels_and_power(struct mac_context *mac);

bool csr_nonscan_active_ll_remove_entry(
			struct mac_context *mac_ctx,
			tListElem *pEntryToRemove, bool inter_locked);
tListElem *csr_nonscan_active_ll_peek_head(
			struct mac_context *mac_ctx,
			bool inter_locked);
tListElem *csr_nonscan_pending_ll_peek_head(
			struct mac_context *mac_ctx,
			bool inter_locked);
tListElem *csr_nonscan_pending_ll_next(
			struct mac_context *mac_ctx,
		tListElem *entry, bool inter_locked);

/**
 * csr_purge_vdev_pending_ser_cmd_list() - purge all scan and non-scan
 * pending cmds for the vdev id
 * @mac_ctx: pointer to global MAC context
 * @vdev_id : vdev id for which the pending cmds need to be purged
 *
 * Return : none
 */
void csr_purge_vdev_pending_ser_cmd_list(struct mac_context *mac_ctx,
					 uint32_t vdev_id);

/**
 * csr_purge_vdev_all_scan_ser_cmd_list() - purge all scan active and pending
 * cmds for the vdev id
 * @mac_ctx: pointer to global MAC context
 * @vdev_id : vdev id for which cmds need to be purged
 *
 * Return : none
 */
void csr_purge_vdev_all_scan_ser_cmd_list(struct mac_context *mac_ctx,
					  uint32_t vdev_id);

/**
 * csr_purge_pdev_all_ser_cmd_list() - purge all scan and non-scan
 * active and pending cmds for all vdevs in pdev
 * @mac_ctx: pointer to global MAC context
 *
 * Return : none
 */
void csr_purge_pdev_all_ser_cmd_list(struct mac_context *mac_ctx);

bool csr_wait_for_connection_update(struct mac_context *mac,
		bool do_release_reacquire_lock);
enum QDF_OPMODE csr_get_session_persona(struct mac_context *pmac,
					uint32_t session_id);
void csr_roam_substate_change(
			struct mac_context *mac, enum csr_roam_substate
					NewSubstate, uint32_t sessionId);

bool csr_is_ndi_started(struct mac_context *mac_ctx, uint32_t session_id);

QDF_STATUS csr_roam_update_config(
			struct mac_context *mac_ctx, uint8_t session_id,
				  uint16_t capab, uint32_t value);

/**
 * csr_is_mcc_channel() - check if using the channel results into MCC
 * @mac_ctx: pointer to global MAC context
 * @chan_freq: channel frequency to check for MCC scenario
 *
 * Return : true if channel causes MCC, else false
 */
bool csr_is_mcc_channel(struct mac_context *mac_ctx, uint32_t chan_freq);
#endif
