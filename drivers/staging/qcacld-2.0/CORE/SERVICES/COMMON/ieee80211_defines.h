/*
 * Copyright (c) 2011 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */



#ifndef _IEEE80211_DEFINES_H_
#define _IEEE80211_DEFINES_H_

#include "ieee80211_common.h"
#ifndef EXTERNAL_USE_ONLY
#include "_ieee80211_common.h"        /* IEEE80211_ADDR_LEN, iee80211_phymode */
#endif

/*
 * Public defines for Atheros Upper MAC Layer
 */

/**
 * @brief Opaque handle of 802.11 protocal layer.
 */
struct ieee80211com;
typedef struct ieee80211com *wlan_dev_t;

/**
 * @brief Opaque handle to App IE module.
*/
struct wlan_mlme_app_ie;
typedef struct wlan_mlme_app_ie *wlan_mlme_app_ie_t;

/**
 * @brief Opaque handle of network instance (vap) in 802.11 protocal layer.
*/
struct ieee80211vap;
typedef struct ieee80211vap *wlan_if_t;

struct ieee80211vapprofile;
typedef struct ieee80211vapprofile *wlan_if_info_t;

/**
 * @brief Opaque handle of a node in the wifi network.
 */
struct ieee80211_node;
typedef struct ieee80211_node *wlan_node_t;

/**
 * @brief Opaque handle of OS interface (ifp in the case of unix ).
 */
struct _os_if_t;
typedef struct _os_if_t *os_if_t;

/**
 *
 * @brief Opaque handle.
 */
typedef void *os_handle_t;

/**
 * @brief Opaque handle of a channel.
 */
struct ieee80211_channel;
typedef struct ieee80211_channel *wlan_chan_t;

/**
 * @brief Opaque handle scan_entry.
 */
struct ieee80211_scan_entry;
typedef struct ieee80211_scan_entry *wlan_scan_entry_t;

/* AoW related defines */
#define AOW_MAX_RECEIVER_COUNT  10



#define IEEE80211_NWID_LEN                  32
#define IEEE80211_ISO_COUNTRY_LENGTH        3       /* length of 11d ISO country string */

typedef struct _ieee80211_ssid {
    int         len;
    u_int8_t    ssid[IEEE80211_NWID_LEN];
} ieee80211_ssid;

typedef struct ieee80211_tx_status {
    int         ts_flags;
#define IEEE80211_TX_ERROR          0x01
#define IEEE80211_TX_XRETRY         0x02

    int         ts_retries;     /* number of retries to successfully transmit this frame */
#ifdef ATH_SUPPORT_TxBF
    u_int8_t    ts_txbfstatus;
#define	AR_BW_Mismatch      0x1
#define	AR_Stream_Miss      0x2
#define	AR_CV_Missed        0x4
#define AR_Dest_Miss        0x8
#define AR_Expired          0x10
#define AR_TxBF_Valid_HW_Status    (AR_BW_Mismatch|AR_Stream_Miss|AR_CV_Missed|AR_Dest_Miss|AR_Expired)
#define TxBF_STATUS_Sounding_Complete   0x20
#define TxBF_STATUS_Sounding_Request    0x40
#define TxBF_Valid_SW_Status  (TxBF_STATUS_Sounding_Complete | TxBF_STATUS_Sounding_Request)
#define TxBF_Valid_Status  (AR_TxBF_Valid_HW_Status | TxBF_Valid_SW_Status)
    u_int32_t    ts_tstamp;     /* tx time stamp */
#endif
#ifdef ATH_SUPPORT_FLOWMAC_MODULE
    u_int8_t    ts_flowmac_flags;
#define IEEE80211_TX_FLOWMAC_DONE           0x01
#endif
    u_int32_t    ts_rateKbps;
} ieee80211_xmit_status;

#ifndef EXTERNAL_USE_ONLY
typedef struct ieee80211_rx_status {
    int         rs_numchains;
    int         rs_flags;
#define IEEE80211_RX_FCS_ERROR      0x01
#define IEEE80211_RX_MIC_ERROR      0x02
#define IEEE80211_RX_DECRYPT_ERROR  0x04
/* holes in flags here between, ATH_RX_XXXX to IEEE80211_RX_XXX */
#define IEEE80211_RX_KEYMISS        0x200
    int         rs_rssi;       /* RSSI (noise floor ajusted) */
    int         rs_abs_rssi;   /* absolute RSSI */
    int         rs_datarate;   /* data rate received */
    int         rs_rateieee;
    int         rs_ratephy;

#define IEEE80211_MAX_ANTENNA       3                /* Keep the same as ATH_MAX_ANTENNA */
    u_int8_t    rs_rssictl[IEEE80211_MAX_ANTENNA];   /* RSSI (noise floor ajusted) */
    u_int8_t    rs_rssiextn[IEEE80211_MAX_ANTENNA];  /* RSSI (noise floor ajusted) */
    u_int8_t    rs_isvalidrssi;                      /* rs_rssi is valid or not */

    enum ieee80211_phymode rs_phymode;
    int         rs_freq;

    union {
        u_int8_t            data[8];
        u_int64_t           tsf;
    } rs_tstamp;

    /*
     * Detail channel structure of recv frame.
     * It could be NULL if not available
     */
    struct ieee80211_channel *rs_full_chan;

    u_int8_t rs_isaggr;
    u_int8_t rs_isapsd;
    int16_t rs_noisefloor;
    u_int16_t  rs_channel;
#ifdef ATH_SUPPORT_TxBF
    u_int32_t   rs_rpttstamp;   /* txbf report time stamp*/
#endif

    /* The following counts are meant to assist in stats calculation.
       These variables are incremented only in specific situations, and
       should not be relied upon for any purpose other than the original
       stats related purpose they have been introduced for. */

    u_int16_t   rs_cryptodecapcount; /* Crypto bytes decapped/demic'ed. */
    u_int8_t    rs_padspace;         /* No. of padding bytes present after header
                                        in wbuf. */
    u_int8_t    rs_qosdecapcount;    /* QoS/HTC bytes decapped. */

    /* End of stats calculation related counts. */

    uint8_t     rs_lsig[IEEE80211_LSIG_LEN];
    uint8_t     rs_htsig[IEEE80211_HTSIG_LEN];
    uint8_t     rs_servicebytes[IEEE80211_SB_LEN];

} ieee80211_recv_status;
#endif /* EXTERNAL_USE_ONLY */

/*
 * flags to be passed to ieee80211_vap_create function .
 */
#define IEEE80211_CLONE_BSSID           0x0001  /* allocate unique mac/bssid */
#define IEEE80211_CLONE_NOBEACONS       0x0002  /* don't setup beacon timers */
#define IEEE80211_CLONE_WDS             0x0004  /* enable WDS processing */
#define IEEE80211_CLONE_WDSLEGACY       0x0008  /* legacy WDS operation */
#define IEEE80211_PRIMARY_VAP           0x0010  /* primary vap */
#define IEEE80211_P2PDEV_VAP            0x0020  /* p2pdev vap */
#define IEEE80211_P2PGO_VAP             0x0040  /* p2p-go vap */
#define IEEE80211_P2PCLI_VAP            0x0080  /* p2p-client vap */
#define IEEE80211_CLONE_MACADDR         0x0100  /* create vap w/ specified mac/bssid */
#define IEEE80211_CLONE_MATADDR         0x0200  /* create vap w/ specified MAT addr */
#define IEEE80211_WRAP_VAP              0x0400  /* wireless repeater ap vap */

/*
 * For the new multi-vap scan feature, there is a set of default priority tables
 * for each OpMode.
 * The following are the default list of the VAP Scan Priority Mapping based on OpModes.
 * NOTE: the following are only used when "#if ATH_SUPPORT_MULTIPLE_SCANS" is true.
 */
/* For IBSS opmode */
#define DEF_VAP_SCAN_PRI_MAP_OPMODE_IBSS_BASE               0
/* For STA opmode */
#define DEF_VAP_SCAN_PRI_MAP_OPMODE_STA_BASE                0
#define DEF_VAP_SCAN_PRI_MAP_OPMODE_STA_P2P_CLIENT          1
/* For HostAp opmode */
#define DEF_VAP_SCAN_PRI_MAP_OPMODE_AP_BASE                 0
#define DEF_VAP_SCAN_PRI_MAP_OPMODE_AP_P2P_GO               1
#define DEF_VAP_SCAN_PRI_MAP_OPMODE_AP_P2P_DEVICE           2
/* For BTAmp opmode */
#define DEF_VAP_SCAN_PRI_MAP_OPMODE_BTAMP_BASE              0

typedef enum _ieee80211_dev_vap_event {
    IEEE80211_VAP_CREATED = 1,
    IEEE80211_VAP_STOPPED,
    IEEE80211_VAP_DELETED
} ieee80211_dev_vap_event;

typedef struct _wlan_dev_event_handler_table {
    void (*wlan_dev_vap_event) (void *event_arg, wlan_dev_t, os_if_t, ieee80211_dev_vap_event);  /* callback to receive vap events*/
#ifdef ATH_SUPPORT_SPECTRAL
    void (*wlan_dev_spectral_indicate)(void*, void*, u_int32_t);
#endif
} wlan_dev_event_handler_table;

typedef enum _ieee80211_ap_stopped_reason {
    IEEE80211_AP_STOPPED_REASON_DUMMY = 0,          /* Dummy placeholder. Should not use */
    IEEE80211_AP_STOPPED_REASON_CHANNEL_DFS = 1,
} ieee80211_ap_stopped_reason;

typedef int IEEE80211_REASON_CODE;
typedef int IEEE80211_STATUS;

/*
 * scan API related structs.
 */
typedef enum _ieee80211_scan_type {
    IEEE80211_SCAN_BACKGROUND,
    IEEE80211_SCAN_FOREGROUND,
    IEEE80211_SCAN_SPECTRAL,
    IEEE80211_SCAN_REPEATER_BACKGROUND,
    IEEE80211_SCAN_REPEATER_EXT_BACKGROUND,
    IEEE80211_SCAN_RADIO_MEASUREMENTS,
} ieee80211_scan_type;

/*
 * Priority numbers must be sequential, starting with 0.
 */
typedef enum ieee80211_scan_priority_t {
    IEEE80211_SCAN_PRIORITY_VERY_LOW    = 0,
    IEEE80211_SCAN_PRIORITY_LOW,
    IEEE80211_SCAN_PRIORITY_MEDIUM,
    IEEE80211_SCAN_PRIORITY_HIGH,
    IEEE80211_SCAN_PRIORITY_VERY_HIGH,

    IEEE80211_SCAN_PRIORITY_COUNT   /* number of priorities supported */
} IEEE80211_SCAN_PRIORITY;

typedef u_int16_t    IEEE80211_SCAN_REQUESTOR;
typedef u_int32_t    IEEE80211_SCAN_ID;

#define IEEE80211_SCAN_ID_NONE                    0

/* All P2P scans currently use medium priority */
#define IEEE80211_P2P_DEFAULT_SCAN_PRIORITY       IEEE80211_SCAN_PRIORITY_MEDIUM
#define IEEE80211_P2P_SCAN_PRIORITY_HIGH          IEEE80211_SCAN_PRIORITY_HIGH

/* Masks identifying types/ID of scans */
#define IEEE80211_SPECIFIC_SCAN       0x00000000
#define IEEE80211_VAP_SCAN            0x01000000
#define IEEE80211_ALL_SCANS           0x04000000

/**
 * host scan bit. only relevant for host/target architecture.
 * do not reuse this bit definition. target uses this .
 *
 */
#define IEEE80211_HOST_SCAN           0x80000000
#define IEEE80211_SCAN_CLASS_MASK     0xFF000000

#define IEEE80211_SCAN_PASSIVE            0x0001 /* passively scan all the channels */
#define IEEE80211_SCAN_ACTIVE             0x0002 /* actively  scan all the channels (regdomain rules still apply) */
#define IEEE80211_SCAN_2GHZ               0x0004 /* scan 2GHz band */
#define IEEE80211_SCAN_5GHZ               0x0008 /* scan 5GHz band */
#define IEEE80211_SCAN_ALLBANDS           (IEEE80211_SCAN_5GHZ | IEEE80211_SCAN_2GHZ)
#define IEEE80211_SCAN_CONTINUOUS         0x0010 /* keep scanning until maxscantime expires */
#define IEEE80211_SCAN_FORCED             0x0020 /* forced scan (OS request) - should proceed even in the presence of data traffic */
#define IEEE80211_SCAN_NOW                0x0040 /* scan now (User request)  - should proceed even in the presence of data traffic */
#define IEEE80211_SCAN_ADD_BCAST_PROBE    0x0080 /* add wildcard ssid and broadcast probe request if there is none */
#define IEEE80211_SCAN_EXTERNAL           0x0100 /* scan requested by OS */
#define IEEE80211_SCAN_BURST              0x0200 /* scan multiple channels before returning to BSS channel */
#define IEEE80211_SCAN_CHAN_EVENT         0x0400 /* scan chan event for  offload architectures */
#define IEEE80211_SCAN_FILTER_PROBE_REQ   0x0800 /* Filter probe requests- applicable only for offload architectures*/

#define IEEE80211_SCAN_PARAMS_MAX_SSID     10
#define IEEE80211_SCAN_PARAMS_MAX_BSSID    10


/* flag definitions passed to scan_cancel API */

#define IEEE80211_SCAN_CANCEL_ASYNC 0x0 /* asynchronouly wait for scan SM to complete cancel */
#define IEEE80211_SCAN_CANCEL_WAIT  0x1 /* wait for scan SM to complete cancel */
#define IEEE80211_SCAN_CANCEL_SYNC  0x2 /* synchronously execute cancel scan */

#ifndef EXTERNAL_USE_ONLY
typedef bool (*ieee80211_scan_termination_check) (void *arg);

typedef struct _ieee80211_scan_params {
    ieee80211_scan_type                 type;
    int                                 min_dwell_time_active;  /* min time in msec on active channels */
    int                                 max_dwell_time_active;  /* max time in msec on active channels (if no response) */
    int                                 min_dwell_time_passive; /* min time in msec on passive channels */
    int                                 max_dwell_time_passive; /* max time in msec on passive channels (if no response) */
    int                                 min_rest_time;          /* min time in msec on the BSS channel, only valid for BG scan */
    int                                 max_rest_time;          /* max time in msec on the BSS channel, only valid for BG scan */
    int                                 max_offchannel_time;    /* max time away from BSS channel, in ms */
    int                                 repeat_probe_time;      /* time before sending second probe request */
    int                                 idle_time;              /* time in msec on bss channel before switching channel */
    int                                 max_scan_time;          /* maximum time in msec allowed for scan  */
    int                                 probe_delay;            /* delay in msec before sending probe request */
    int                                 offchan_retry_delay;    /* delay in msec before retrying off-channel switch */
    int                                 min_beacon_count;       /* number of home AP beacons to receive before leaving the home channel */
    int                                 max_offchan_retries;    /* maximum number of times to retry off-channel switch */
    int                                 beacon_timeout;         /* maximum time to wait for beacons */
    int                                 flags;                  /* scan flags */
    int                                 num_channels;           /* number of channels to scan */
    bool                                multiple_ports_active;  /* driver has multiple ports active in the home channel */
    bool                                restricted_scan;        /* Perform restricted scan */
    bool                                chan_list_allocated;
    IEEE80211_SCAN_PRIORITY             p2p_scan_priority;      /* indicates the scan priority if this is a P2P-related scan */
    u_int32_t                           *chan_list;             /* array of ieee channels (or) frequencies to scan */
    int                                 num_ssid;               /* number of desired ssids */
    ieee80211_ssid                      ssid_list[IEEE80211_SCAN_PARAMS_MAX_SSID];
    int                                 num_bssid;              /* number of desired bssids */
    u_int8_t                            bssid_list[IEEE80211_SCAN_PARAMS_MAX_BSSID][IEEE80211_ADDR_LEN];
    struct ieee80211_node               *bss_node;              /* BSS node */
    int                                 ie_len;                 /* length of the ie data to be added to probe req */
    u_int8_t                            *ie_data;               /* pointer to ie data */
    ieee80211_scan_termination_check    check_termination_function;  /* function checking for termination condition */
    void                                *check_termination_context;  /* context passed to function above */
} ieee80211_scan_params;

/* Data types used to specify scan priorities */
typedef u_int32_t IEEE80211_PRIORITY_MAPPING[IEEE80211_SCAN_PRIORITY_COUNT];

/**************************************
 * Called before attempting to roam.  Modifies the rssiAdder of a BSS
 * based on the preferred status of a BSS.
 *
 * According to CCX spec, AP in the neighbor list is not meant for giving extra
 * weightage in roaming. By doing so, roaming becomes sticky. See bug 21220.
 * Change the weightage to 0. Cisco may ask in future for a user control of
 * this weightage.
 */
#define PREFERRED_BSS_RANK                20
#define NEIGHBOR_BSS_RANK                  0    /* must be less than preferred BSS rank */

/*
 * The utility of the BSS is the metric used in the selection
 * of a BSS. The Utility of the BSS is reduced if we just left the BSS.
 * The Utility of the BSS is not reduced if we have left the
 * BSS for 8 seconds (8000ms) or more.
 * 2^13 milliseconds is a close approximation to avoid expensive division
 */
#define LAST_ASSOC_TIME_DELTA_REQUIREMENT (1 << 13) // 8192

#define QBSS_SCALE_MAX                   255  /* Qbss channel load Max value */
#define QBSS_SCALE_DOWN_FACTOR             2  /* scale factor to reduce Qbss channel load */
#define QBSS_HYST_ADJ                     60  /* Qbss Weightage factor for the current AP */

/*
 * Flags used to set field APState
 */
#define AP_STATE_GOOD    0x00
#define AP_STATE_BAD     0x01
#define AP_STATE_RETRY   0x10
#define BAD_AP_TIMEOUT   6000   // In milli seconds
/*
 * To disable BAD_AP status check on any scan entry
 */
#define BAD_AP_TIMEOUT_DISABLED             0

/*
 * BAD_AP timeout specified in seconds
 */
#define BAD_AP_TIMEOUT_IN_SECONDS           10

/*
 * State values used to represent our assoc_state with ap (discrete, not bitmasks)
 */
#define AP_ASSOC_STATE_NONE     0
#define AP_ASSOC_STATE_AUTH     1
#define AP_ASSOC_STATE_ASSOC    2

/*
 * Entries in the scan list are considered obsolete after 75 seconds.
 */
#define IEEE80211_SCAN_ENTRY_EXPIRE_TIME           75000

/*
 * idle time is only valid for scan type IEEE80211_SCAN_BACKGROUND.
 * if idle time is set then the scanner would change channel from BSS
 * channel to foreign channel only if both resttime is expired and
 * the theres was not traffic for idletime msec on the bss channel.
 * value of 0 for idletime would cause the channel to switch from BSS
 * channel to foreign channel as soon  as the resttime is expired.
 *
 * if maxscantime is nonzero and if the scanner can not complete the
 * scan in maxscantime msec then the scanner will cancel the scan and
 * post IEEE80211_SCAN_COMPLETED event with reason SCAN_TIMEDOUT.
 *
 */

/*
 * chanlist can be either ieee channels (or) frequencies.
 * if a value is less than 1000 implementation assumes it
 * as ieee channel # otherwise implementation assumes it
 * as frequency in Mhz.
 */

typedef enum _ieee80211_scan_event_type {
    IEEE80211_SCAN_STARTED,
    IEEE80211_SCAN_COMPLETED,
    IEEE80211_SCAN_RADIO_MEASUREMENT_START,
    IEEE80211_SCAN_RADIO_MEASUREMENT_END,
    IEEE80211_SCAN_RESTARTED,
    IEEE80211_SCAN_HOME_CHANNEL,
    IEEE80211_SCAN_FOREIGN_CHANNEL,
    IEEE80211_SCAN_BSSID_MATCH,
    IEEE80211_SCAN_FOREIGN_CHANNEL_GET_NF,
    IEEE80211_SCAN_DEQUEUED,
    IEEE80211_SCAN_PREEMPTED,

    IEEE80211_SCAN_EVENT_COUNT
} ieee80211_scan_event_type;

typedef enum ieee80211_scan_completion_reason {
    IEEE80211_REASON_NONE,
    IEEE80211_REASON_COMPLETED,
    IEEE80211_REASON_CANCELLED,
    IEEE80211_REASON_TIMEDOUT,
    IEEE80211_REASON_TERMINATION_FUNCTION,
    IEEE80211_REASON_MAX_OFFCHAN_RETRIES,
    IEEE80211_REASON_PREEMPTED,
    IEEE80211_REASON_RUN_FAILED,
    IEEE80211_REASON_INTERNAL_STOP,

    IEEE80211_REASON_COUNT
} ieee80211_scan_completion_reason;

typedef struct _ieee80211_scan_event {
    ieee80211_scan_event_type           type;
    ieee80211_scan_completion_reason    reason;
    wlan_chan_t                         chan;
    IEEE80211_SCAN_REQUESTOR            requestor; /* Requestor ID passed to the scan_start function */
    IEEE80211_SCAN_ID                   scan_id;   /* Specific ID of the scan reporting the event */
} ieee80211_scan_event;

typedef enum _ieee80211_scan_request_status {
    IEEE80211_SCAN_STATUS_QUEUED,
    IEEE80211_SCAN_STATUS_RUNNING,
    IEEE80211_SCAN_STATUS_PREEMPTED,
    IEEE80211_SCAN_STATUS_COMPLETED
} ieee80211_scan_request_status;

/*
 * the sentry field of tht ieee80211_scan_event is only valid if the
 * event type is IEEE80211_SCAN_BSSID_MATCH.
 */

typedef void (*ieee80211_scan_event_handler) (wlan_if_t vaphandle, ieee80211_scan_event *event, void *arg);

typedef struct _ieee80211_scan_info {
    ieee80211_scan_type                type;
    IEEE80211_SCAN_REQUESTOR           requestor;                   /* Originator ID passed to the scan_start function */
    IEEE80211_SCAN_ID                  scan_id;                     /* Specific ID of the scan reporting the event */
    IEEE80211_SCAN_PRIORITY            priority;                    /* Requested priority level (low/medium/high) */
    ieee80211_scan_request_status      scheduling_status;           /* Queued/running/preempted/completed */
    int                                min_dwell_time_active;       /* min time in msec on active channels */
    int                                max_dwell_time_active;       /* max time in msec on active channel (if no response) */
    int                                min_dwell_time_passive;      /* min time in msec on passive channels */
    int                                max_dwell_time_passive;      /* max time in msec on passive channel*/
    int                                min_rest_time;               /* min time in msec on the BSS channel, only valid for BG scan */
    int                                max_rest_time;               /* max time in msec on the BSS channel, only valid for BG scan */
    int                                max_offchannel_time;         /* max time away from BSS channel, in ms */
    int                                repeat_probe_time;           /* time before sending second probe request */
    int                                min_beacon_count;            /* number of home AP beacons to receive before leaving the home channel */
    int                                flags;                       /* scan flags */
    systime_t                          scan_start_time;             /* system time when last scani started */
    int                                scanned_channels;            /* number of scanned channels */
    int                                default_channel_list_length; /* number of channels in the default channel list */
    int                                channel_list_length;         /* number of channels in the channel list used for the current scan */
    u_int8_t                           in_progress            : 1,  /* if the scan is in progress */
                                       cancelled              : 1,  /* if the scan is cancelled */
                                       preempted              : 1,  /* if the scan is preempted */
                                       restricted             : 1;  /* if the scan is restricted */
} ieee80211_scan_info;

typedef struct _ieee80211_scan_request_info {
    wlan_if_t                        vaphandle;
    IEEE80211_SCAN_REQUESTOR         requestor;
    IEEE80211_SCAN_PRIORITY          requested_priority;
    IEEE80211_SCAN_PRIORITY          absolute_priority;
    IEEE80211_SCAN_ID                scan_id;
    ieee80211_scan_request_status    scheduling_status;
    ieee80211_scan_params            params;
    systime_t                        request_timestamp;
    u_int32_t                        maximum_duration;
} ieee80211_scan_request_info;

#endif /* EXTERNAL_USE_ONLY */

#ifndef EXTERNAL_USE_ONLY
typedef void (*ieee80211_acs_event_handler) (void *arg, wlan_chan_t channel);
#endif /* EXTERNAL_USE_ONLY */

#define MAX_CHAINS 3

typedef struct _wlan_rssi_info {
    int8_t      avg_rssi;     /* average rssi */
    u_int8_t    valid_mask;   /* bitmap of valid elements in rssi_ctrl/ext array */
    int8_t      rssi_ctrl[MAX_CHAINS];
    int8_t      rssi_ext[MAX_CHAINS];
} wlan_rssi_info;

typedef enum _wlan_rssi_type {
    WLAN_RSSI_TX,
    WLAN_RSSI_RX,
    WLAN_RSSI_BEACON,  /* rssi of the beacon, only valid for STA/IBSS vap */
    WLAN_RSSI_RX_DATA
} wlan_rssi_type;

typedef enum _ieee80211_rate_type {
    IEEE80211_RATE_TYPE_LEGACY,
    IEEE80211_RATE_TYPE_MCS,
} ieee80211_rate_type;

typedef struct _ieee80211_rate_info {
    ieee80211_rate_type    type;
    u_int32_t              rate;     /* average rate in kbps */
    u_int32_t              lastrate; /* last packet rate in kbps */
    u_int8_t               mcs;      /* mcs index . is valid if rate type is MCS20 or MCS40 */
    u_int8_t               maxrate_per_client;
} ieee80211_rate_info;

typedef enum _ieee80211_node_param_type {
    IEEE80211_NODE_PARAM_TX_POWER,
    IEEE80211_NODE_PARAM_ASSOCID,
    IEEE80211_NODE_PARAM_INACT,     /* inactivity timer value */
    IEEE80211_NODE_PARAM_AUTH_MODE, /* auth mode */
    IEEE80211_NODE_PARAM_CAP_INFO,  /* auth mode */
} ieee80211_node_param_type;

/*
 * Per/node (station) statistics available when operating as an AP.
 */
struct ieee80211_nodestats {
    u_int32_t    ns_rx_data;             /* rx data frames */
    u_int32_t    ns_rx_mgmt;             /* rx management frames */
    u_int32_t    ns_rx_ctrl;             /* rx control frames */
    u_int32_t    ns_rx_ucast;            /* rx unicast frames */
    u_int32_t    ns_rx_mcast;            /* rx multi/broadcast frames */
    u_int64_t    ns_rx_bytes;            /* rx data count (bytes) */
    u_int64_t    ns_rx_beacons;          /* rx beacon frames */
    u_int32_t    ns_rx_proberesp;        /* rx probe response frames */

    u_int32_t    ns_rx_dup;              /* rx discard 'cuz dup */
    u_int32_t    ns_rx_noprivacy;        /* rx w/ wep but privacy off */
    u_int32_t    ns_rx_wepfail;          /* rx wep processing failed */
    u_int32_t    ns_rx_demicfail;        /* rx demic failed */

    /* We log MIC and decryption failures against Transmitter STA stats.
       Though the frames may not actually be sent by STAs corresponding
       to TA, the stats are still valuable for some customers as a sort
       of rough indication.
       Also note that the mapping from TA to STA may fail sometimes. */
    u_int32_t    ns_rx_tkipmic;          /* rx TKIP MIC failure */
    u_int32_t    ns_rx_ccmpmic;          /* rx CCMP MIC failure */
    u_int32_t    ns_rx_wpimic;           /* rx WAPI MIC failure */
    u_int32_t    ns_rx_tkipicv;          /* rx ICV check failed (TKIP) */
    u_int32_t    ns_rx_decap;            /* rx decapsulation failed */
    u_int32_t    ns_rx_defrag;           /* rx defragmentation failed */
    u_int32_t    ns_rx_disassoc;         /* rx disassociation */
    u_int32_t    ns_rx_deauth;           /* rx deauthentication */
    u_int32_t    ns_rx_action;           /* rx action */
    u_int32_t    ns_rx_decryptcrc;       /* rx decrypt failed on crc */
    u_int32_t    ns_rx_unauth;           /* rx on unauthorized port */
    u_int32_t    ns_rx_unencrypted;      /* rx unecrypted w/ privacy */

    u_int32_t    ns_tx_data;             /* tx data frames */
    u_int32_t    ns_tx_data_success;     /* tx data frames successfully
                                            transmitted (unicast only) */
    u_int32_t    ns_tx_mgmt;             /* tx management frames */
    u_int32_t    ns_tx_ucast;            /* tx unicast frames */
    u_int32_t    ns_tx_mcast;            /* tx multi/broadcast frames */
    u_int64_t    ns_tx_bytes;            /* tx data count (bytes) */
    u_int64_t    ns_tx_bytes_success;    /* tx success data count - unicast only
                                            (bytes) */
    u_int32_t    ns_tx_probereq;         /* tx probe request frames */
    u_int32_t    ns_tx_uapsd;            /* tx on uapsd queue */
    u_int32_t    ns_tx_discard;          /* tx dropped by NIC */

    u_int32_t    ns_tx_novlantag;        /* tx discard 'cuz no tag */
    u_int32_t    ns_tx_vlanmismatch;     /* tx discard 'cuz bad tag */

    u_int32_t    ns_tx_eosplost;         /* uapsd EOSP retried out */

    u_int32_t    ns_ps_discard;          /* ps discard 'cuz of age */

    u_int32_t    ns_uapsd_triggers;      /* uapsd triggers */
    u_int32_t    ns_uapsd_duptriggers;    /* uapsd duplicate triggers */
    u_int32_t    ns_uapsd_ignoretriggers; /* uapsd duplicate triggers */
    u_int32_t    ns_uapsd_active;         /* uapsd duplicate triggers */
    u_int32_t    ns_uapsd_triggerenabled; /* uapsd duplicate triggers */


    /* MIB-related state */
    u_int32_t    ns_tx_assoc;            /* [re]associations */
    u_int32_t    ns_tx_assoc_fail;       /* [re]association failures */
    u_int32_t    ns_tx_auth;             /* [re]authentications */
    u_int32_t    ns_tx_auth_fail;        /* [re]authentication failures*/
    u_int32_t    ns_tx_deauth;           /* deauthentications */
    u_int32_t    ns_tx_deauth_code;      /* last deauth reason */
    u_int32_t    ns_tx_disassoc;         /* disassociations */
    u_int32_t    ns_tx_disassoc_code;    /* last disassociation reason */
    u_int32_t    ns_psq_drops;           /* power save queue drops */
};

/*
 * station power save mode.
 */
typedef enum ieee80211_psmode {
    IEEE80211_PWRSAVE_NONE = 0,          /* no power save */
    IEEE80211_PWRSAVE_LOW,
    IEEE80211_PWRSAVE_NORMAL,
    IEEE80211_PWRSAVE_MAXIMUM,
    IEEE80211_PWRSAVE_WNM                /* WNM-Sleep Mode */
} ieee80211_pwrsave_mode;

/* station power save pspoll handling */
typedef enum {
  IEEE80211_CONTINUE_PSPOLL_FOR_MORE_DATA,
  IEEE80211_WAKEUP_FOR_MORE_DATA,
} ieee80211_pspoll_moredata_handling;

/*
 * apps power save state.
 */
typedef enum {
     APPS_AWAKE = 0,
     APPS_PENDING_SLEEP,
     APPS_SLEEP,
     APPS_FAKE_SLEEP,           /* Pending blocking sleep */
     APPS_FAKING_SLEEP,         /* Blocking sleep */
     APPS_UNKNOWN_PWRSAVE,
} ieee80211_apps_pwrsave_state;

typedef enum _iee80211_mimo_powersave_mode {
    IEEE80211_MIMO_POWERSAVE_NONE,    /* no mimo power save */
    IEEE80211_MIMO_POWERSAVE_STATIC,  /* static mimo power save */
    IEEE80211_MIMO_POWERSAVE_DYNAMIC  /* dynamic mimo powersave */
} ieee80211_mimo_powersave_mode;

#ifdef ATH_COALESCING
typedef enum _ieee80211_coalescing_state {
    IEEE80211_COALESCING_DISABLED   = 0,        /* Coalescing is disabled*/
    IEEE80211_COALESCING_DYNAMIC    = 1,        /* Dynamically move to Enabled state based on Uruns*/
    IEEE80211_COALESCING_ENABLED    = 2,        /* Coalescing is enabled*/
} ieee80211_coalescing_state;

#define IEEE80211_TX_COALESCING_THRESHOLD     5 /* Number of underrun errors to trigger coalescing */
#endif

typedef enum _ieee80211_cap {
    IEEE80211_CAP_SHSLOT,                    /* CAPABILITY: short slot */
    IEEE80211_CAP_SHPREAMBLE,                /* CAPABILITY: short premable */
    IEEE80211_CAP_MULTI_DOMAIN,              /* CAPABILITY: multiple domain */
    IEEE80211_CAP_WMM,                       /* CAPABILITY: WMM */
    IEEE80211_CAP_HT,                        /* CAPABILITY: HT */
    IEEE80211_CAP_PERF_PWR_OFLD,             /* CAPABILITY: power performance offload support */
    IEEE80211_CAP_11AC,                      /* CAPABILITY: 11ac support */
} ieee80211_cap;

typedef enum _ieee80211_device_param {
	IEEE80211_DEVICE_RSSI_CTL,
    IEEE80211_DEVICE_NUM_TX_CHAIN,
    IEEE80211_DEVICE_NUM_RX_CHAIN,
    IEEE80211_DEVICE_TX_CHAIN_MASK,
    IEEE80211_DEVICE_RX_CHAIN_MASK,
    IEEE80211_DEVICE_TX_CHAIN_MASK_LEGACY,
    IEEE80211_DEVICE_RX_CHAIN_MASK_LEGACY,
    IEEE80211_DEVICE_BMISS_LIMIT,            /* # of beacon misses for HW to generate BMISS intr */
    IEEE80211_DEVICE_PROTECTION_MODE,        /* protection mode*/
    IEEE80211_DEVICE_BLKDFSCHAN,             /* block the use of DFS channels */
    IEEE80211_DEVICE_GREEN_AP_PS_ENABLE,
    IEEE80211_DEVICE_GREEN_AP_PS_TIMEOUT,
    IEEE80211_DEVICE_GREEN_AP_PS_ON_TIME,
    IEEE80211_DEVICE_CWM_EXTPROTMODE,
    IEEE80211_DEVICE_CWM_EXTPROTSPACING,
    IEEE80211_DEVICE_CWM_ENABLE,
    IEEE80211_DEVICE_CWM_EXTBUSYTHRESHOLD,
    IEEE80211_DEVICE_DOTH,
    IEEE80211_DEVICE_ADDBA_MODE,
    IEEE80211_DEVICE_COUNTRYCODE,
    IEEE80211_DEVICE_MULTI_CHANNEL,           /* turn on/off off channel support */
    IEEE80211_DEVICE_MAX_AMSDU_SIZE,          /* Size of AMSDU to be sent on the air */
    IEEE80211_DEVICE_P2P,                     /* Enable or Disable P2P */
    IEEE80211_DEVICE_OVERRIDE_SCAN_PROBERESPONSE_IE, /* Override scan Probe response IE, 0: Don't over-ride */
    IEEE80211_DEVICE_2G_CSA,
    IEEE80211_DEVICE_PWRTARGET,
    IEEE80211_DEVICE_OFF_CHANNEL_SUPPORT,
} ieee80211_device_param;

typedef enum _ieee80211_param {
    IEEE80211_BEACON_INTVAL,                 /* in TUs */
    IEEE80211_LISTEN_INTVAL,                 /* number of beacons */
    IEEE80211_DTIM_INTVAL,                   /* number of beacons */
    IEEE80211_BMISS_COUNT_RESET,             /* number of beacon miss intrs before reset */
    IEEE80211_BMISS_COUNT_MAX,               /* number of beacon miss intrs for bmiss notificationst */
    IEEE80211_ATIM_WINDOW,                   /* ATIM window */
    IEEE80211_SHORT_SLOT,                    /* short slot on/off */
    IEEE80211_SHORT_PREAMBLE,                /* short preamble on/off */
    IEEE80211_RTS_THRESHOLD,                 /* rts threshold, 0 means no rts threshold  */
    IEEE80211_FRAG_THRESHOLD,                /* fragmentation threshold, 0 means no rts threshold  */
    IEEE80211_FIXED_RATE,                    /*
                                              * rate code series(0: auto rate, 32 bit value:  rate
                                              * codes for 4 rate series. each byte for one rate series)
                                              */
    IEEE80211_MCAST_RATE,                    /* rate in Kbps */
    IEEE80211_TXPOWER,                       /* in 0.5db units */
    IEEE80211_AMPDU_DENCITY,                 /* AMPDU dencity*/
    IEEE80211_AMPDU_LIMIT,                   /* AMPDU limit*/
    IEEE80211_MAX_AMPDU,                     /* Max AMPDU Exp*/
    IEEE80211_VHT_MAX_AMPDU,                 /* VHT Max AMPDU Exp */
    IEEE80211_WPS_MODE,                      /* WPS mode*/
    IEEE80211_TSN_MODE,                      /* TSN mode*/
    IEEE80211_MULTI_DOMAIN,                  /* Multiple domain */
    IEEE80211_SAFE_MODE,                     /* Safe mode */
    IEEE80211_NOBRIDGE_MODE,                 /* No bridging done, all frames sent up the stack */
    IEEE80211_PERSTA_KEYTABLE_SIZE,          /* IBSS-only, read-only: persta key table size */
    IEEE80211_RECEIVE_80211,                 /* deliver std 802.11 frames 802.11 instead of ethernet frames on the rx */
    IEEE80211_SEND_80211,                    /* OS sends std 802.11 frames 802.11 instead of ethernet frames on tx side */
    IEEE80211_MIN_BEACON_COUNT,              /* minumum number beacons to tx/rx before vap can pause */
    IEEE80211_IDLE_TIME,                     /* minimun no activity time before vap can pause */
    IEEE80211_MIN_FRAMESIZE,                 /* smallest frame size we are allowed to receive */
                                             /* features. 0:feature is off. 1:feature is on. */
    IEEE80211_FEATURE_WMM,                   /* WMM */
    IEEE80211_FEATURE_WMM_PWRSAVE,           /* WMM Power Save */
    IEEE80211_FEATURE_UAPSD,                 /* UAPSD setting (BE/BK/VI/VO) */
    IEEE80211_FEATURE_WDS,                   /* dynamic WDS feature */
    IEEE80211_FEATURE_PRIVACY,               /* encryption */
    IEEE80211_FEATURE_DROP_UNENC,            /* drop un encrypted frames */
    IEEE80211_FEATURE_COUNTER_MEASURES ,     /* turn on couter measures */
    IEEE80211_FEATURE_HIDE_SSID,             /* turn on hide ssid feature */
    IEEE80211_FEATURE_APBRIDGE,              /* turn on internal mcast traffic bridging for AP */
    IEEE80211_FEATURE_PUREB,                 /* turn on pure B mode for AP */
    IEEE80211_FEATURE_PUREG,                 /* turn on pure G mode for AP */
    IEEE80211_FEATURE_REGCLASS,              /* add regulatory class IE in AP */
    IEEE80211_FEATURE_COUNTRY_IE,            /* add country IE for vap in AP */
    IEEE80211_FEATURE_IC_COUNTRY_IE,         /* add country IE for ic in AP */
    IEEE80211_FEATURE_DOTH,                  /* enable 802.11h */
    IEEE80211_FEATURE_PURE11N,               /* enable pure 11n  mode */
    IEEE80211_FEATURE_PRIVATE_RSNIE,         /* enable OS shim to setup RSN IE*/
    IEEE80211_FEATURE_COPY_BEACON,           /* keep a copy of beacon */
    IEEE80211_FEATURE_PSPOLL,                /* enable/disable pspoll mode in power save SM */
    IEEE80211_FEATURE_CONTINUE_PSPOLL_FOR_MOREDATA, /* enable/disable option to contunue sending ps polls when there is more data */
    IEEE80211_FEATURE_AMPDU,                 /* Enable or Disable Aggregation */
#ifdef ATH_COALESCING
    IEEE80211_FEATURE_TX_COALESCING,         /* enable tx coalescing */
#endif
    IEEE80211_FEATURE_VAP_IND,               /* Repeater independant VAP */
    IEEE80211_FIXED_RETRIES,                 /* fixed retries  0-4 */
    IEEE80211_SHORT_GI,                      /* short gi on/off */
    IEEE80211_HT40_INTOLERANT,
    IEEE80211_CHWIDTH,
    IEEE80211_CHEXTOFFSET,
    IEEE80211_DISABLE_2040COEXIST,
    IEEE80211_DISABLE_HTPROTECTION,
    IEEE80211_STA_QUICKKICKOUT,
    IEEE80211_CHSCANINIT,
    IEEE80211_FEATURE_STAFWD,                /* dynamic AP Client  feature */
    IEEE80211_DRIVER_CAPS,
    IEEE80211_UAPSD_MAXSP,                    /* UAPSD service period setting (0:unlimited, 2,4,6) */
    IEEE80211_WEP_MBSSID,
    IEEE80211_MGMT_RATE,                      /* ieee rate to be used for management*/
    IEEE80211_RESMGR_VAP_AIR_TIME_LIMIT,      /* When multi-channel enabled, restrict air-time allocated to a VAP */
    IEEE80211_TDLS_MACADDR1,                  /* Upper 4 bytes of device's MAC address */
    IEEE80211_TDLS_MACADDR2,                  /* Lower 2 bytes of device's MAC address */
    IEEE80211_TDLS_ACTION,                    /* TDLS action requested                 */
    IEEE80211_AUTO_ASSOC,
    IEEE80211_PROTECTION_MODE,                /* per VAP protection mode*/
    IEEE80211_AUTH_INACT_TIMEOUT,             /* inactivity time while waiting for 802.11x auth to complete */
    IEEE80211_INIT_INACT_TIMEOUT,             /* inactivity time while waiting for 802.11 auth/assoc to complete */
    IEEE80211_RUN_INACT_TIMEOUT,              /* inactivity time when fully authed*/
    IEEE80211_PROBE_INACT_TIMEOUT,            /* inactivity counter value below which starts probing */
    IEEE80211_QBSS_LOAD,
    IEEE80211_WNM_CAP,
    IEEE80211_WNM_BSS_CAP,
    IEEE80211_WNM_TFS_CAP,
    IEEE80211_WNM_TIM_CAP,
    IEEE80211_WNM_SLEEP_CAP,
    IEEE80211_WNM_FMS_CAP,
    IEEE80211_AP_REJECT_DFS_CHAN,             /* AP to reject resuming on DFS Channel */
	IEEE80211_ABOLT,
	IEEE80211_COMP,
	IEEE80211_FF,
	IEEE80211_TURBO,
	IEEE80211_BURST,
	IEEE80211_AR,
	IEEE80211_SLEEP,
	IEEE80211_EOSPDROP,
	IEEE80211_MARKDFS,
	IEEE80211_WDS_AUTODETECT,
	IEEE80211_WEP_TKIP_HT,
	IEEE80211_ATH_RADIO,
	IEEE80211_IGNORE_11DBEACON,
    /* Video debug feature */
    IEEE80211_VI_DBG_CFG,                     /* Video debug configuration - Bit0- enable dbg, Bit1 - enable stats log */
    IEEE80211_VI_DBG_NUM_STREAMS,             /* Total number of receive streams */
    IEEE80211_VI_STREAM_NUM,                  /* the stream number whose marker parameters are being set */
    IEEE80211_VI_DBG_NUM_MARKERS,             /* total number of markers used to filter pkts */
    IEEE80211_VI_MARKER_NUM,                  /* the marker number whose parameters (offset, size & match) are being set */
    IEEE80211_VI_MARKER_OFFSET_SIZE,          /* byte offset from skb start (upper 16 bits) & size in bytes(lower 16 bits) */
    IEEE80211_VI_MARKER_MATCH,                /* marker pattern match used in filtering */
    IEEE80211_VI_RXSEQ_OFFSET_SIZE,           /* Rx Seq num offset skb start (upper 16 bits) & size in bytes(lower 16 bits) */
    IEEE80211_VI_RX_SEQ_RSHIFT,               /* right-shift value in case field is not word aligned */
    IEEE80211_VI_RX_SEQ_MAX,                  /* maximum Rx Seq number (to check wrap around) */
    IEEE80211_VI_RX_SEQ_DROP,                 /* Indicator to the debug app that a particular seq num has been dropped */
    IEEE80211_VI_TIME_OFFSET_SIZE,            /* Timestamp offset skb start (upper 16 bits) & size in bytes(lower 16 bits) */
    IEEE80211_VI_RESTART,                     /* If set to 1 resets all internal variables/counters & restarts debug tool*/
    IEEE80211_VI_RXDROP_STATUS,               /* Total RX drops in wireless */
    IEEE80211_TRIGGER_MLME_RESP,              /* Option for App to trigger mlme response */
#ifdef ATH_SUPPORT_TxBF
    IEEE80211_TXBF_AUTO_CVUPDATE,             /* auto CV update enable */
    IEEE80211_TXBF_CVUPDATE_PER,              /* per threshold to initial CV update*/
#endif
    IEEE80211_MAX_CLIENT_NUMBERS,
	IEEE80211_SMARTNET,
    IEEE80211_FEATURE_MFP_TEST,               /* MFP test */
	IEEE80211_WEATHER_RADAR,				  /* weather radar channel skip */
    IEEE80211_WEP_KEYCACHE,                   /* WEP KEYCACHE is enable */
    IEEE80211_SEND_DEAUTH,                  /* send deauth instead of disassoc while doing interface down  */
    IEEE80211_SET_TXPWRADJUST,
    IEEE80211_RRM_CAP,
    IEEE80211_RRM_DEBUG,
    IEEE80211_RRM_STATS,
    IEEE80211_RRM_SLWINDOW,
    IEEE80211_FEATURE_OFF_CHANNEL_SUPPORT,
    IEEE80211_FIXED_VHT_MCS,                  /* VHT mcs index */
    IEEE80211_FIXED_NSS,                      /* Spatial Streams count */
    IEEE80211_SUPPORT_LDPC,                   /* LDPC Support */
    IEEE80211_SUPPORT_TX_STBC,                /* TX STBC enable/disable */
    IEEE80211_SUPPORT_RX_STBC,                /* RX STBC enable/disable */
    IEEE80211_DEFAULT_KEYID,                  /* XMIT default key */
    IEEE80211_OPMODE_NOTIFY_ENABLE,           /* Op mode notification enable/disable */
    IEEE80211_ENABLE_RTSCTS,                  /* Enable/Disable RTS-CTS */
    IEEE80211_VHT_MCSMAP,                     /* VHT MCS Map */
    IEEE80211_GET_ACS_STATE,                  /* get acs state */
    IEEE80211_GET_CAC_STATE,                  /* get cac state */
} ieee80211_param;

#define  IEEE80211_PROTECTION_NONE         0
#define  IEEE80211_PROTECTION_CTSTOSELF    1
#define  IEEE80211_PROTECTION_RTS_CTS      2

typedef enum _ieee80211_privacy_filter {
    IEEE80211_PRIVACY_FILTER_ALLWAYS,
    IEEE80211_PRIVACY_FILTER_KEY_UNAVAILABLE,
} ieee80211_privacy_filter ;

typedef enum _ieee80211_privacy_filter_packet_type {
    IEEE80211_PRIVACY_FILTER_PACKET_UNICAST,
    IEEE80211_PRIVACY_FILTER_PACKET_MULTICAST,
    IEEE80211_PRIVACY_FILTER_PACKET_BOTH
} ieee80211_privacy_filter_packet_type ;

typedef struct _ieee80211_privacy_excemption_filter {
    u_int16_t                               ether_type; /* type of ethernet to apply this filter, in host byte order*/
    ieee80211_privacy_filter                filter_type;
    ieee80211_privacy_filter_packet_type    packet_type;
} ieee80211_privacy_exemption;

/*
 * Authentication mode.
 * NB: the usage of auth modes NONE, AUTO are deprecated,
 * they are implemented through combinations of other auth modes
 * and cipher types. The deprecated values are preserved here to
 * maintain binary compatibility with applications like
 * wpa_supplicant and hostapd.
 */
typedef enum _ieee80211_auth_mode {
    IEEE80211_AUTH_NONE     = 0, /* deprecated */
    IEEE80211_AUTH_OPEN     = 1, /* open */
    IEEE80211_AUTH_SHARED   = 2, /* shared-key */
    IEEE80211_AUTH_8021X    = 3, /* 802.1x */
    IEEE80211_AUTH_AUTO     = 4, /* deprecated */
    IEEE80211_AUTH_WPA      = 5, /* WPA */
    IEEE80211_AUTH_RSNA     = 6, /* WPA2/RSNA */
    IEEE80211_AUTH_CCKM     = 7, /* CCK */
    IEEE80211_AUTH_WAPI     = 8, /* WAPI */
} ieee80211_auth_mode;

#define IEEE80211_AUTH_MAX      (IEEE80211_AUTH_WAPI+1)

/*
 * Cipher types.
 * NB: The values are preserved here to maintain binary compatibility
 * with applications like wpa_supplicant and hostapd.
 */
typedef enum _ieee80211_cipher_type {
    IEEE80211_CIPHER_WEP        = 0,
    IEEE80211_CIPHER_TKIP       = 1,
    IEEE80211_CIPHER_AES_OCB    = 2,
    IEEE80211_CIPHER_AES_CCM    = 3,
    IEEE80211_CIPHER_WAPI       = 4,
    IEEE80211_CIPHER_CKIP       = 5,
    IEEE80211_CIPHER_AES_CMAC   = 6,
    IEEE80211_CIPHER_NONE       = 7,
} ieee80211_cipher_type;

#define IEEE80211_CIPHER_MAX    (IEEE80211_CIPHER_NONE+1)

/* key direction */
typedef enum _ieee80211_key_direction {
    IEEE80211_KEY_DIR_TX,
    IEEE80211_KEY_DIR_RX,
    IEEE80211_KEY_DIR_BOTH
} ieee80211_key_direction;

#define IEEE80211_KEYIX_NONE    ((u_int16_t) -1)

typedef struct _ieee80211_keyval {
    ieee80211_cipher_type   keytype;
    ieee80211_key_direction keydir;
    u_int                   persistent:1,   /* persistent key */
                            mfp:1;          /* management frame protection */
    u_int16_t               keylen;         /* length of the key data fields */
    u_int8_t                *macaddr;       /* mac address of length IEEE80211_ADDR_LEN . all bytes are 0xff for multicast key */
    u_int64_t               keyrsc;
    u_int64_t               keytsc;
    u_int16_t               txmic_offset;   /* TKIP/SMS4 only: offset to tx mic key */
    u_int16_t               rxmic_offset;   /* TKIP/SMS4 only: offset to rx mic key */
    u_int8_t                *keydata;
#ifdef ATH_SUPPORT_WAPI
    u_int8_t                key_used;       /*index for WAPI rekey labeling*/
#endif
} ieee80211_keyval;

#define IEEE80211_AES_CMAC_LEN     128
typedef enum _ieee80211_rsn_param {
    IEEE80211_UCAST_CIPHER_LEN,
    IEEE80211_MCAST_CIPHER_LEN,
    IEEE80211_MCASTMGMT_CIPHER_LEN,
    IEEE80211_KEYMGT_ALGS,
    IEEE80211_RSN_CAPS
} ieee80211_rsn_param;

#define IEEE80211_PMKID_LEN     16

typedef struct _ieee80211_pmkid_entry {
    u_int8_t    bssid[IEEE80211_ADDR_LEN];
    u_int8_t    pmkid[IEEE80211_PMKID_LEN];
} ieee80211_pmkid_entry;

typedef enum _wlan_wme_param {
    WLAN_WME_CWMIN,
    WLAN_WME_CWMAX,
    WLAN_WME_AIFS,
    WLAN_WME_TXOPLIMIT,
    WLAN_WME_ACM,      /*bss only*/
    WLAN_WME_ACKPOLICY /*bss only*/
} wlan_wme_param;

typedef enum _ieee80211_frame_type {
    IEEE80211_FRAME_TYPE_PROBEREQ,
    IEEE80211_FRAME_TYPE_BEACON,
    IEEE80211_FRAME_TYPE_PROBERESP,
    IEEE80211_FRAME_TYPE_ASSOCREQ,
    IEEE80211_FRAME_TYPE_ASSOCRESP,
    IEEE80211_FRAME_TYPE_AUTH
} ieee80211_frame_type;

#define IEEE80211_FRAME_TYPE_MAX    (IEEE80211_FRAME_TYPE_AUTH+1)

typedef enum _ieee80211_ampdu_mode {
    IEEE80211_AMPDU_MODE_OFF,   /* disable AMPDU */
    IEEE80211_AMPDU_MODE_ON,    /* enable AMPDU */
    IEEE80211_AMPDU_MODE_WDSVAR /* enable AMPDU with 4addr WAR */
} ieee80211_ampdu_mode;

typedef enum _ieee80211_reset_type {
    IEEE80211_RESET_TYPE_DEVICE = 0,    /* device reset on error: tx timeout and etc. */
    IEEE80211_RESET_TYPE_DOT11_INTF,    /* dot11 reset: only reset one network interface (vap) */
    IEEE80211_RESET_TYPE_INTERNAL,      /* internal reset */
} ieee80211_reset_type;

typedef struct _ieee80211_reset_request {
    ieee80211_reset_type    type;

    u_int                   reset_hw:1,         /* reset the actual H/W */
                            /*
                             * The following fields are only valid for DOT11 reset, i.e.,
                             * IEEE80211_RESET_TYPE_DOT11_INTF
                             */
                            reset_phy:1,        /* reset PHY */
                            reset_mac:1,        /* reset MAC */
                            set_default_mib:1,  /* set default MIB variables */
                            no_flush:1;
    u_int8_t                macaddr[IEEE80211_ADDR_LEN];
    enum ieee80211_phymode  phy_mode;
} ieee80211_reset_request;

#define IEEE80211_MSG_MAX 63
#define IEEE80211_MSG_SMARTANT 7            /* Bit 7 (0x80)for Smart Antenna debug */
enum {
    /* IEEE80211_PARAM_DBG_LVL */
    IEEE80211_MSG_TDLS      = 0,            /* TDLS */
    IEEE80211_MSG_ACS,                      /* auto channel selection */
    IEEE80211_MSG_SCAN_SM,                  /* scan state machine */
    IEEE80211_MSG_SCANENTRY,                /* scan entry */
    IEEE80211_MSG_WDS,                      /* WDS handling */
    IEEE80211_MSG_ACTION,                   /* action management frames */
    IEEE80211_MSG_ROAM,                     /* sta-mode roaming */
    IEEE80211_MSG_INACT,                    /* inactivity handling */
    IEEE80211_MSG_DOTH      = 8,            /* 11.h */
    IEEE80211_MSG_IQUE,                     /* IQUE features */
    IEEE80211_MSG_WME,                      /* WME protocol */
    IEEE80211_MSG_ACL,                      /* ACL handling */
    IEEE80211_MSG_WPA,                      /* WPA/RSN protocol */
    IEEE80211_MSG_RADKEYS,                  /* dump 802.1x keys */
    IEEE80211_MSG_RADDUMP,                  /* dump 802.1x radius packets */
    IEEE80211_MSG_RADIUS,                   /* 802.1x radius client */
    IEEE80211_MSG_DOT1XSM   = 16,           /* 802.1x state machine */
    IEEE80211_MSG_DOT1X,                    /* 802.1x authenticator */
    IEEE80211_MSG_POWER,                    /* power save handling */
    IEEE80211_MSG_STATE,                    /* state machine */
    IEEE80211_MSG_OUTPUT,                   /* output handling */
    IEEE80211_MSG_SCAN,                     /* scanning */
    IEEE80211_MSG_AUTH,                     /* authentication handling */
    IEEE80211_MSG_ASSOC,                    /* association handling */
    IEEE80211_MSG_NODE      = 24,           /* node handling */
    IEEE80211_MSG_ELEMID,                   /* element id parsing */
    IEEE80211_MSG_XRATE,                    /* rate set handling */
    IEEE80211_MSG_INPUT,                    /* input handling */
    IEEE80211_MSG_CRYPTO,                   /* crypto work */
    IEEE80211_MSG_DUMPPKTS,                 /* IFF_LINK2 equivalant */
    IEEE80211_MSG_DEBUG,                    /* IFF_DEBUG equivalent */
    IEEE80211_MSG_MLME,                     /* MLME */
    /* IEEE80211_PARAM_DBG_LVL_HIGH */
    IEEE80211_MSG_RRM       = 32,           /* Radio resource measurement */
    IEEE80211_MSG_WNM,                      /* Wireless Network Management */
    IEEE80211_MSG_P2P_PROT,                 /* P2P Protocol driver */
    IEEE80211_MSG_PROXYARP,                 /* 11v Proxy ARP */
    IEEE80211_MSG_L2TIF,                    /* Hotspot 2.0 L2 TIF */
    IEEE80211_MSG_WIFIPOS,                  /* WifiPositioning Feature */
    IEEE80211_MSG_WRAP,                     /* WRAP or Wireless ProxySTA */
    IEEE80211_MSG_DFS,                      /* DFS debug mesg */

    IEEE80211_MSG_NUM_CATEGORIES,           /* total ieee80211 messages */
    IEEE80211_MSG_UNMASKABLE = IEEE80211_MSG_MAX,  /* anything */
    IEEE80211_MSG_ANY = IEEE80211_MSG_MAX,  /* anything */
};

/* verbosity levels */
#define     IEEE80211_VERBOSE_OFF                  100
#define     IEEE80211_VERBOSE_FORCE               1
#define     IEEE80211_VERBOSE_SERIOUS             2
#define     IEEE80211_VERBOSE_NORMAL              3
#define     IEEE80211_VERBOSE_LOUD                4
#define     IEEE80211_VERBOSE_DETAILED            5
#define     IEEE80211_VERBOSE_COMPLEX             6
#define     IEEE80211_VERBOSE_FUNCTION            7
#define     IEEE80211_VERBOSE_TRACE               8

#define IEEE80211_DEBUG_DEFAULT IEEE80211_MSG_DEBUG

/*
 * the lower 4 bits of the msg flags are used for extending the
 * debug flags.
 */

/*
 * flag defintions for wlan_mlme_stop_bss(vap) API.
 */
#define WLAN_MLME_STOP_BSS_F_SEND_DEAUTH                0x01
#define WLAN_MLME_STOP_BSS_F_CLEAR_ASSOC_STATE          0x02
#define WLAN_MLME_STOP_BSS_F_FORCE_STOP_RESET           0x04
#define WLAN_MLME_STOP_BSS_F_WAIT_RX_DONE               0x08
#define WLAN_MLME_STOP_BSS_F_NO_RESET                   0x10
#define WLAN_MLME_STOP_BSS_F_STANDBY                    0x20

/*
 * WAPI commands to authenticator
 */
#define WAPI_WAI_REQUEST            (u_int16_t)0x00F1
#define WAPI_UNICAST_REKEY          (u_int16_t)0x00F2
#define WAPI_STA_AGING              (u_int16_t)0x00F3
#define WAPI_MULTI_REKEY            (u_int16_t)0x00F4
#define WAPI_STA_STATS              (u_int16_t)0x00F5

/*
 * IEEE80211 PHY Statistics.
 */
struct ieee80211_phy_stats {
    u_int64_t   ips_tx_packets;      /* frames successfully transmitted */
    u_int64_t   ips_tx_multicast;    /* multicast/broadcast frames successfully transmitted */
    u_int64_t   ips_tx_fragments;    /* fragments successfully transmitted */
    u_int64_t   ips_tx_xretries;     /* frames that are xretried. NB: not number of retries */
    u_int64_t   ips_tx_retries;      /* frames transmitted after retries. NB: not number of retries */
    u_int64_t   ips_tx_multiretries; /* frames transmitted after more than one retry. */
    u_int64_t   ips_tx_timeout;      /* frames that expire the dot11MaxTransmitMSDULifetime */
    u_int64_t   ips_rx_packets;      /* frames successfully received */
    u_int64_t   ips_rx_multicast;    /* multicast/broadcast frames successfully received */
    u_int64_t   ips_rx_fragments;    /* fragments successfully received */
    u_int64_t   ips_rx_timeout;      /* frmaes that expired the dot11MaxReceiveLifetime */
    u_int64_t   ips_rx_dup;          /* duplicated fragments */
    u_int64_t   ips_rx_mdup;         /* multiple duplicated fragments */
    u_int64_t   ips_rx_promiscuous;  /* frames that are received only because promiscuous filter is on */
    u_int64_t   ips_rx_promiscuous_fragments; /* fragments that are received only because promiscuous filter is on */
    u_int64_t   ips_tx_rts;          /* RTS success count */
    u_int64_t   ips_tx_shortretry;   /* tx on-chip retries (short). RTSFailCnt */
    u_int64_t   ips_tx_longretry;    /* tx on-chip retries (long). DataFailCnt */
    u_int64_t   ips_rx_crcerr;       /* rx failed 'cuz of bad CRC */
    u_int64_t   ips_rx_fifoerr;      /* rx failed 'cuz of FIFO overrun */
    u_int64_t   ips_rx_decrypterr;   /* rx decryption error */
};

struct ieee80211_chan_stats {
    u_int32_t   chan_clr_cnt;
    u_int32_t   cycle_cnt;
    u_int32_t   phy_err_cnt;
};

struct ieee80211_mac_stats {
    u_int64_t   ims_tx_packets; /* frames successfully transmitted */
    u_int64_t   ims_rx_packets; /* frames successfully received */
    u_int64_t   ims_tx_bytes;	/* bytes successfully transmitted */
    u_int64_t	ims_rx_bytes;   /* bytes successfully received */

    /* TODO: For the byte counts below, we need to handle some scenarios
       such as encryption related decaps, etc */
    u_int64_t   ims_tx_data_packets;/* data frames successfully transmitted */
    u_int64_t   ims_rx_data_packets;/* data frames successfully received */
    u_int64_t   ims_tx_data_bytes;  /* data bytes successfully transmitted,
                                       inclusive of FCS. */
    u_int64_t   ims_rx_data_bytes;  /* data bytes successfully received,
                                       inclusive of FCS. */

    u_int64_t   ims_tx_datapyld_bytes;  /* data payload bytes successfully
                                           transmitted */
    u_int64_t   ims_rx_datapyld_bytes;  /* data payload successfully
                                           received */

    /* Decryption errors */
    u_int64_t   ims_rx_unencrypted; /* rx w/o wep and privacy on */
    u_int64_t   ims_rx_badkeyid;    /* rx w/ incorrect keyid */
    u_int64_t   ims_rx_decryptok;   /* rx decrypt okay */
    u_int64_t   ims_rx_decryptcrc;  /* rx decrypt failed on crc */
    u_int64_t   ims_rx_wepfail;     /* rx wep processing failed */
    u_int64_t   ims_rx_tkipreplay;  /* rx seq# violation (TKIP) */
    u_int64_t   ims_rx_tkipformat;  /* rx format bad (TKIP) */
    u_int64_t   ims_rx_tkipmic;     /* rx MIC check failed (TKIP) */
    u_int64_t   ims_rx_tkipicv;     /* rx ICV check failed (TKIP) */
    u_int64_t   ims_rx_ccmpreplay;  /* rx seq# violation (CCMP) */
    u_int64_t   ims_rx_ccmpformat;  /* rx format bad (CCMP) */
    u_int64_t   ims_rx_ccmpmic;     /* rx MIC check failed (CCMP) */
/*this file can be included by applications as 80211stats that has no such MACRO definition*/
//#if ATH_SUPPORT_WAPI
    u_int64_t   ims_rx_wpireplay;  /* rx seq# violation (WPI) */
    u_int64_t   ims_rx_wpimic;     /* rx MIC check failed (WPI) */
//#endif
    /* Other Tx/Rx errors */
    u_int64_t   ims_tx_discard;     /* tx dropped by NIC */
    u_int64_t   ims_rx_discard;     /* rx dropped by NIC */

    u_int64_t   ims_rx_countermeasure; /* rx TKIP countermeasure activation count */
};

/*
 * Summary statistics.
 */
struct ieee80211_stats {
    u_int32_t   is_rx_badversion;          /* rx frame with bad version */
    u_int32_t   is_rx_tooshort;            /* rx frame too short */
    u_int32_t   is_rx_wrongbss;            /* rx from wrong bssid */
    u_int32_t   is_rx_wrongdir;            /* rx w/ wrong direction */
    u_int32_t   is_rx_mcastecho;           /* rx discard 'cuz mcast echo */
    u_int32_t   is_rx_notassoc;            /* rx discard 'cuz sta !assoc */
    u_int32_t   is_rx_noprivacy;           /* rx w/ wep but privacy off */
    u_int32_t   is_rx_decap;               /* rx decapsulation failed */
    u_int32_t   is_rx_mgtdiscard;          /* rx discard mgt frames */
    u_int32_t   is_rx_ctl;                 /* rx discard ctrl frames */
    u_int32_t   is_rx_beacon;              /* rx beacon frames */
    u_int32_t   is_rx_rstoobig;            /* rx rate set truncated */
    u_int32_t   is_rx_elem_missing;        /* rx required element missing*/
    u_int32_t   is_rx_elem_toobig;         /* rx element too big */
    u_int32_t   is_rx_elem_toosmall;       /* rx element too small */
    u_int32_t   is_rx_elem_unknown;        /* rx element unknown */
    u_int32_t   is_rx_badchan;             /* rx frame w/ invalid chan */
    u_int32_t   is_rx_chanmismatch;        /* rx frame chan mismatch */
    u_int32_t   is_rx_nodealloc;           /* rx frame dropped */
    u_int32_t   is_rx_ssidmismatch;        /* rx frame ssid mismatch  */
    u_int32_t   is_rx_auth_unsupported;    /* rx w/ unsupported auth alg */
    u_int32_t   is_rx_auth_fail;           /* rx sta auth failure */
    u_int32_t   is_rx_auth_countermeasures;/* rx auth discard 'cuz CM */
    u_int32_t   is_rx_assoc_bss;           /* rx assoc from wrong bssid */
    u_int32_t   is_rx_assoc_notauth;       /* rx assoc w/o auth */
    u_int32_t   is_rx_assoc_capmismatch;   /* rx assoc w/ cap mismatch */
    u_int32_t   is_rx_assoc_norate;        /* rx assoc w/ no rate match */
    u_int32_t   is_rx_assoc_badwpaie;      /* rx assoc w/ bad WPA IE */
    u_int32_t   is_rx_deauth;              /* rx deauthentication */
    u_int32_t   is_rx_disassoc;            /* rx disassociation */
    u_int32_t   is_rx_action;              /* rx action mgt */
    u_int32_t   is_rx_badsubtype;          /* rx frame w/ unknown subtype*/
    u_int32_t   is_rx_nobuf;               /* rx failed for lack of buf */
    u_int32_t   is_rx_ahdemo_mgt;          /* rx discard ahdemo mgt frame*/
    u_int32_t   is_rx_bad_auth;            /* rx bad auth request */
    u_int32_t   is_rx_unauth;              /* rx on unauthorized port */
    u_int32_t   is_rx_badcipher;           /* rx failed 'cuz key type */
    u_int32_t   is_tx_nodefkey;            /* tx failed 'cuz no defkey */
    u_int32_t   is_tx_noheadroom;          /* tx failed 'cuz no space */
    u_int32_t   is_rx_nocipherctx;         /* rx failed 'cuz key !setup */
    u_int32_t   is_rx_acl;                 /* rx discard 'cuz acl policy */
    u_int32_t   is_rx_ffcnt;               /* rx fast frames */
    u_int32_t   is_rx_badathtnl;           /* driver key alloc failed */
    u_int32_t   is_rx_nowds;               /* 4-addr packets received with no wds enabled */
    u_int32_t   is_tx_nobuf;               /* tx failed for lack of buf */
    u_int32_t   is_tx_nonode;              /* tx failed for no node */
    u_int32_t   is_tx_unknownmgt;          /* tx of unknown mgt frame */
    u_int32_t   is_tx_badcipher;           /* tx failed 'cuz key type */
    u_int32_t   is_tx_ffokcnt;             /* tx fast frames sent success */
    u_int32_t   is_tx_fferrcnt;            /* tx fast frames sent success */
    u_int32_t   is_scan_active;            /* active scans started */
    u_int32_t   is_scan_passive;           /* passive scans started */
    u_int32_t   is_node_timeout;           /* nodes timed out inactivity */
    u_int32_t   is_crypto_nomem;           /* no memory for crypto ctx */
    u_int32_t   is_crypto_tkip;            /* tkip crypto done in s/w */
    u_int32_t   is_crypto_tkipenmic;       /* tkip en-MIC done in s/w */
    u_int32_t   is_crypto_tkipdemic;       /* tkip de-MIC done in s/w */
    u_int32_t   is_crypto_tkipcm;          /* tkip counter measures */
    u_int32_t   is_crypto_ccmp;            /* ccmp crypto done in s/w */
    u_int32_t   is_crypto_wep;             /* wep crypto done in s/w */
    u_int32_t   is_crypto_setkey_cipher;   /* cipher rejected key */
    u_int32_t   is_crypto_setkey_nokey;    /* no key index for setkey */
    u_int32_t   is_crypto_delkey;          /* driver key delete failed */
    u_int32_t   is_crypto_badcipher;       /* unknown cipher */
    u_int32_t   is_crypto_nocipher;        /* cipher not available */
    u_int32_t   is_crypto_attachfail;      /* cipher attach failed */
    u_int32_t   is_crypto_swfallback;      /* cipher fallback to s/w */
    u_int32_t   is_crypto_keyfail;         /* driver key alloc failed */
    u_int32_t   is_crypto_enmicfail;       /* en-MIC failed */
    u_int32_t   is_ibss_capmismatch;       /* merge failed-cap mismatch */
    u_int32_t   is_ibss_norate;            /* merge failed-rate mismatch */
    u_int32_t   is_ps_unassoc;             /* ps-poll for unassoc. sta */
    u_int32_t   is_ps_badaid;              /* ps-poll w/ incorrect aid */
    u_int32_t   is_ps_qempty;              /* ps-poll w/ nothing to send */
};

typedef enum _ieee80211_send_frame_type {
    IEEE80211_SEND_NULL,
    IEEE80211_SEND_QOSNULL,
} ieee80211_send_frame_type;

typedef struct _ieee80211_tspec_info {
    u_int8_t    traffic_type;
    u_int8_t    direction;
    u_int8_t    dot1Dtag;
    u_int8_t    tid;
    u_int8_t    acc_policy_edca;
    u_int8_t    acc_policy_hcca;
    u_int8_t    aggregation;
    u_int8_t    psb;
    u_int8_t    ack_policy;
    u_int16_t   norminal_msdu_size;
    u_int16_t   max_msdu_size;
    u_int32_t   min_srv_interval;
    u_int32_t   max_srv_interval;
    u_int32_t   inactivity_interval;
    u_int32_t   suspension_interval;
    u_int32_t   srv_start_time;
    u_int32_t   min_data_rate;
    u_int32_t   mean_data_rate;
    u_int32_t   peak_data_rate;
    u_int32_t   max_burst_size;
    u_int32_t   delay_bound;
    u_int32_t   min_phy_rate;
    u_int16_t   surplus_bw;
    u_int16_t   medium_time;
} ieee80211_tspec_info;

#ifndef EXTERNAL_USE_ONLY
/*
 * Manual ADDBA support
 */
enum {
    ADDBA_SEND     = 0,
    ADDBA_STATUS   = 1,
    DELBA_SEND     = 2,
    ADDBA_RESP     = 3,
    ADDBA_CLR_RESP = 4,
    SINGLE_AMSDU   = 5,
};

enum {
    ADDBA_MODE_AUTO   = 0,
    ADDBA_MODE_MANUAL = 1,
};

struct ieee80211_addba_delba_request {
    wlan_dev_t             ic;
    u_int8_t               action;
    u_int8_t               tid;
    u_int16_t              status;
    u_int16_t              aid;
    u_int32_t              arg1;
    u_int32_t              arg2;
};
#endif /* EXTERNAL_USE_ONLY */

#ifdef ATH_BT_COEX
typedef enum _ieee80211_bt_coex_info_type {
    IEEE80211_BT_COEX_INFO_SCHEME        = 0,
    IEEE80211_BT_COEX_INFO_BTBUSY        = 1,
} ieee80211_bt_coex_info_type;
#endif

struct tkip_countermeasure {
    u_int16_t   mic_count_in_60s;
    u_int32_t   timestamp;
} ;

enum _ieee80211_qos_frame_direction {
    IEEE80211_RX_QOS_FRAME = 0,
    IEEE80211_TX_QOS_FRAME = 1,
    IEEE80211_TX_COMPLETE_QOS_FRAME = 2
};

typedef struct ieee80211_vap_opmode_count {
    int    total_vaps;
    int    ibss_count;
    int    sta_count;
    int    wds_count;
    int    ahdemo_count;
    int    ap_count;
    int    monitor_count;
    int    btamp_count;
    int    unknown_count;
} ieee80211_vap_opmode_count;

struct ieee80211_app_ie_t {
        u_int32_t       length;
        u_int8_t        *ie;
};

/*
 * MAC ACL operations.
 */
enum {
    IEEE80211_MACCMD_POLICY_OPEN    = 0,  /* set policy: no ACL's */
    IEEE80211_MACCMD_POLICY_ALLOW   = 1,  /* set policy: allow traffic */
    IEEE80211_MACCMD_POLICY_DENY    = 2,  /* set policy: deny traffic */
    IEEE80211_MACCMD_FLUSH          = 3,  /* flush ACL database */
    IEEE80211_MACCMD_DETACH         = 4,  /* detach ACL policy */
    IEEE80211_MACCMD_POLICY_RADIUS  = 5,  /* set policy: RADIUS managed ACLs */
};

#endif
