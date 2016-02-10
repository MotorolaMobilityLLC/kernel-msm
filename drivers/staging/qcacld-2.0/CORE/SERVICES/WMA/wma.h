/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
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

/**========================================================================

  \file     wma.h
  \brief    Implementation of WMA

  ========================================================================*/
/**=========================================================================
  EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

  $Header:$   $DateTime: $ $Author: $


  when              who           what, where, why
  --------          ---           -----------------------------------------
  12/03/2013        Ganesh        Created module for WMA
                    Kondabattini
  27/03/2013        Ganesh        Rx Mgmt Related added
                    Babu
  ==========================================================================*/
#ifndef WMA_H
#define WMA_H

#include "a_types.h"
#include "vos_types.h"
#include "osapi_linux.h"
#include "htc_packet.h"
#include "i_vos_event.h"
#include "wmi_services.h"
#include "wmi_unified.h"
#include "wmi_version.h"
#include "halTypes.h"
#include "cfgApi.h"
#include "vos_status.h"
#include "vos_sched.h"
#include "wlan_hdd_tgt_cfg.h"
#include "ol_txrx_api.h"
#include "sirMacProtDef.h"
#include "wlan_qct_wda.h"
#include "ol_txrx_types.h"
#include "wlan_qct_wda.h"
#include <linux/workqueue.h>

/* Platform specific configuration for max. no. of fragments */
#define QCA_OL_11AC_TX_MAX_FRAGS            2

/** Private **/
#define WMA_CFG_NV_DNLD_TIMEOUT            500
#define WMA_READY_EVENTID_TIMEOUT          2000
#define WMA_TGT_SUSPEND_COMPLETE_TIMEOUT   6000
#define WMA_WAKE_LOCK_TIMEOUT              1000
#define WMA_MAX_RESUME_RETRY               100
#define WMA_RESUME_TIMEOUT                 6000
#define WMA_TGT_WOW_TX_COMPLETE_TIMEOUT    2000
#define MAX_MEM_CHUNKS                     32
#define WMA_CRASH_INJECT_TIMEOUT           5000

/*
   In prima 12 HW stations are supported including BCAST STA(staId 0)
   and SELF STA(staId 1) so total ASSOC stations which can connect to Prima
   SoftAP = 12 - 1(Self STa) - 1(Bcast Sta) = 10 Stations. */

#ifdef WLAN_SOFTAP_VSTA_FEATURE
#define WMA_MAX_SUPPORTED_STAS    38
#else
#define WMA_MAX_SUPPORTED_STAS    12
#endif
#define WMA_MAX_SUPPORTED_BSS     5

#define FRAGMENT_SIZE 3072

#define WMA_INVALID_VDEV_ID				0xFF
#define MAX_MEM_CHUNKS					32
#define WMA_MAX_VDEV_SIZE				20
#define WMA_VDEV_TBL_ENTRY_ADD				1
#define WMA_VDEV_TBL_ENTRY_DEL				0


/* 11A/G channel boundary */
#define WMA_11A_CHANNEL_BEGIN           34
#define WMA_11A_CHANNEL_END             165
#define WMA_11G_CHANNEL_BEGIN           1
#define WMA_11G_CHANNEL_END             14

#define WMA_11P_CHANNEL_BEGIN           (170)
#define WMA_11P_CHANNEL_END             (184)

#define WMA_LOGD(args...) \
	VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_DEBUG, ## args)
#define WMA_LOGI(args...) \
	VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_INFO, ## args)
#define WMA_LOGW(args...) \
	VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_WARN, ## args)
#define WMA_LOGE(args...) \
	VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR, ## args)
#define WMA_LOGP(args...) \
	VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_FATAL, ## args)

#define WMA_DEBUG_ALWAYS

#ifdef WMA_DEBUG_ALWAYS
#define WMA_LOGA(args...) \
	VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_FATAL, ## args)
#else
#define WMA_LOGA(args...)
#endif

#define     ALIGNED_WORD_SIZE       4
#define WLAN_HAL_MSG_TYPE_MAX_ENUM_SIZE    0x7FFF

/* Prefix used by scan req ids generated on the host */
#define WMA_HOST_SCAN_REQID_PREFIX	 0xA000
/* Prefix used by roam scan req ids generated on the host */
#define WMA_HOST_ROAM_SCAN_REQID_PREFIX  0xA800
/* Prefix used by scan requestor id on host */
#define WMA_HOST_SCAN_REQUESTOR_ID_PREFIX 0xA000
#define WMA_HW_DEF_SCAN_MAX_DURATION      30000 /* 30 secs */

/* Max offchannel duration */
#define WMA_BURST_SCAN_MAX_NUM_OFFCHANNELS  (3)
#define WMA_SCAN_NPROBES_DEFAULT            (2)
#define WMA_SCAN_IDLE_TIME_DEFAULT          (25)
#define WMA_P2P_SCAN_MAX_BURST_DURATION     (180)
#define WMA_CTS_DURATION_MS_MAX             (32)
#define WMA_GO_MIN_ACTIVE_SCAN_BURST_DURATION   (40)
#define WMA_GO_MAX_ACTIVE_SCAN_BURST_DURATION   (120)
#define WMA_DWELL_TIME_PASSIVE_DEFAULT          (110)
#define WMA_DWELL_TIME_PROBE_TIME_MAP_SIZE      (11)
#define WMA_3PORT_CONC_SCAN_MAX_BURST_DURATION  (25)

#define WMA_SEC_TO_USEC                     (1000000)

#define BEACON_TX_BUFFER_SIZE               (512)

/* WMA_ETHER_TYPE_OFFSET = sa(6) + da(6) */
#define WMA_ETHER_TYPE_OFFSET (6 + 6)
/* WMA_ICMP_V6_HEADER_OFFSET = sa(6) + da(6) + eth_type(2) + icmp_v6_hdr(6)*/
#define WMA_ICMP_V6_HEADER_OFFSET (6 + 6 + 2 + 6)
/* WMA_ICMP_V6_TYPE_OFFSET = sa(6) + da(6) + eth_type(2) + 40 */
#define WMA_ICMP_V6_TYPE_OFFSET (6 + 6 + 2 + 40)

#define WMA_ICMP_V6_HEADER_TYPE (0x3A)
#define WMA_ICMP_V6_RA_TYPE (0x86)
#define WMA_ICMP_V6_NS_TYPE (0x87)
#define WMA_ICMP_V6_NA_TYPE (0x88)
#define WMA_BCAST_MAC_ADDR (0xFF)
#define WMA_MCAST_IPV4_MAC_ADDR (0x01)
#define WMA_MCAST_IPV6_MAC_ADDR (0x33)

typedef struct probeTime_dwellTime {
	u_int8_t dwell_time;
	u_int8_t probe_time;
} t_probeTime_dwellTime;

static const t_probeTime_dwellTime
	probeTime_dwellTime_map[WMA_DWELL_TIME_PROBE_TIME_MAP_SIZE] = {
	{28, 0}, /* 0 SSID */
	{28, 20}, /* 1 SSID */
	{28, 20}, /* 2 SSID */
	{28, 20}, /* 3 SSID */
	{28, 20}, /* 4 SSID */
	{28, 20}, /* 5 SSID */
	{28, 20}, /* 6 SSID */
	{28, 11}, /* 7 SSID */
	{28, 11}, /* 8 SSID */
	{28, 11}, /* 9 SSID */
	{28, 8}   /* 10 SSID */
};

/* Roaming default values
 * All time and period values are in milliseconds.
 * All rssi values are in dB except for WMA_NOISE_FLOOR_DBM_DEFAULT.
 */

#define WMA_ROAM_SCAN_CHANNEL_SWITCH_TIME    (4)
#define WMA_NOISE_FLOOR_DBM_DEFAULT          (-96)
#define WMA_ROAM_RSSI_DIFF_DEFAULT           (5)
#define WMA_ROAM_DWELL_TIME_ACTIVE_DEFAULT   (100)
#define WMA_ROAM_DWELL_TIME_PASSIVE_DEFAULT  (110)
#define WMA_ROAM_MIN_REST_TIME_DEFAULT       (50)
#define WMA_ROAM_MAX_REST_TIME_DEFAULT       (500)
#define WMA_ROAM_LOW_RSSI_TRIGGER_DEFAULT    (20)
#define WMA_ROAM_LOW_RSSI_TRIGGER_VERYLOW    (10)
#define WMA_ROAM_BEACON_WEIGHT_DEFAULT       (14)
#define WMA_ROAM_OPP_SCAN_PERIOD_DEFAULT     (120000)
#define WMA_ROAM_OPP_SCAN_AGING_PERIOD_DEFAULT (WMA_ROAM_OPP_SCAN_PERIOD_DEFAULT * 5)
#define WMA_ROAM_PREAUTH_SCAN_TIME           (50)
#define WMA_ROAM_PREAUTH_REST_TIME           (0)
#define WMA_ROAM_PREAUTH_MAX_SCAN_TIME       (10000)
#define WMA_ROAM_BMISS_FIRST_BCNT_DEFAULT    (10)
#define WMA_ROAM_BMISS_FINAL_BCNT_DEFAULT    (10)
#define WMA_ROAM_BMISS_FIRST_BCNT_DEFAULT_P2P (15)
#define WMA_ROAM_BMISS_FINAL_BCNT_DEFAULT_P2P (45)

#define WMA_INVALID_KEY_IDX	0xff
#define WMA_DFS_RADAR_FOUND   1

#define WMA_MAX_RF_CHAINS(x)	((1 << x) - 1)
#define WMA_MIN_RF_CHAINS		(1)

#ifdef FEATURE_WLAN_EXTSCAN
#define WMA_MAX_EXTSCAN_MSG_SIZE        1536
#define WMA_EXTSCAN_REST_TIME           100
#define WMA_EXTSCAN_MAX_SCAN_TIME       50000
#define WMA_EXTSCAN_BURST_DURATION      150
#endif

typedef void (*txFailIndCallback)(u_int8_t *peer_mac, u_int8_t seqNo);

typedef struct {
	HTC_ENDPOINT_ID endpoint_id;
}t_cfg_nv_param;

typedef enum
{
	WMA_DRIVER_TYPE_PRODUCTION  = 0,
	WMA_DRIVER_TYPE_MFG         = 1,
	WMA_DRIVER_TYPE_DVT         = 2,
	WMA_DRIVER_TYPE_INVALID     = 0x7FFFFFFF
}t_wma_drv_type;

typedef enum {
	WMA_STATE_OPEN,
	WMA_STATE_START,
	WMA_STATE_STOP,
	WMA_STATE_CLOSE
}t_wma_state;

#ifdef FEATURE_WLAN_TDLS
typedef enum {
	WMA_TDLS_SUPPORT_NOT_ENABLED = 0,
	WMA_TDLS_SUPPORT_DISABLED, /* suppress implicit trigger and not respond to the peer */
	WMA_TDLS_SUPPORT_EXPLICIT_TRIGGER_ONLY, /* suppress implicit trigger, but respond to the peer */
	WMA_TDLS_SUPPORT_ENABLED, /* implicit trigger */
	/* External control means implicit trigger
	 * but only to a peer mac configured by user space.
	 */
	WMA_TDLS_SUPPORT_ACTIVE_EXTERNAL_CONTROL,
}t_wma_tdls_mode;

/** TDLS EVENTS */
enum wma_tdls_peer_notification {
	/** tdls discovery recommended for peer (always based
	 * on tx bytes per second > tx_discover threshold
	 * NB: notification will be re-sent after
	 *     discovery_request_interval_ms */
	WMA_TDLS_SHOULD_DISCOVER,
	/** tdls link tear down recommended for peer
	 * due to tx bytes per second below tx_teardown_threshold
	 * NB: this notification sent once */
	WMA_TDLS_SHOULD_TEARDOWN,
	/** tx peer TDLS link tear down complete */
	WMA_TDLS_PEER_DISCONNECTED,
};

enum wma_tdls_peer_reason {
	/** tdls teardown recommended due to low transmits */
	WMA_TDLS_TEARDOWN_REASON_TX,
	/** tdls tear down recommended due to packet rates < AP rates */
	WMA_TDLS_TEARDOWN_REASON_RATE,
	/** tdls link tear down recommended due to poor RSSI */
	WMA_TDLS_TEARDOWN_REASON_RSSI,
	/** tdls link tear down recommended due to offchannel scan */
	WMA_TDLS_TEARDOWN_REASON_SCAN,
	/** tdls peer disconnected due to peer deletion */
	WMA_TDLS_DISCONNECTED_REASON_PEER_DELETE,
};
#endif /* FEATURE_WLAN_TDLS */

typedef enum {
        /* Roaming preauth channel state */
        WMA_ROAM_PREAUTH_CHAN_NONE,
        WMA_ROAM_PREAUTH_CHAN_REQUESTED,
        WMA_ROAM_PREAUTH_ON_CHAN,
        WMA_ROAM_PREAUTH_CHAN_CANCEL_REQUESTED,
        WMA_ROAM_PREAUTH_CHAN_COMPLETED
} t_wma_roam_preauth_chan_state_t;
/*
 * memory chunck allocated by Host to be managed by FW
 * used only for low latency interfaces like pcie
 */
struct wma_mem_chunk {
    u_int32_t *vaddr;
    u_int32_t paddr;
    adf_os_dma_mem_context(memctx);
    u_int32_t len;
    u_int32_t req_id;
};

typedef struct s_vdev_tbl {
	u_int8_t vdev_id;
	u_int8_t sta_mac[ETH_ALEN];
	ol_txrx_vdev_handle tx_rx_vdev_handle;
	u_int32_t vdev_type;
	bool used;
}t_vdev_tbl;

struct scan_param{
	u_int32_t scan_id;
	u_int32_t scan_requestor_id;
	tSirP2pScanType p2p_scan_type;
};


#define WMA_BCN_BUF_MAX_SIZE 2500
#define WMA_NOA_IE_SIZE(num_desc) (2 + (13 * (num_desc)))
#define WMA_MAX_NOA_DESCRIPTORS 4
struct beacon_info {
	adf_nbuf_t buf;
	u_int32_t len;
	u_int8_t dma_mapped;
	u_int32_t tim_ie_offset;
	u_int8_t dtim_count;
	u_int16_t seq_no;
	u_int8_t noa_sub_ie[2 + WMA_NOA_IE_SIZE(WMA_MAX_NOA_DESCRIPTORS)];
	u_int16_t noa_sub_ie_len;
	u_int8_t *noa_ie;
	u_int16_t p2p_ie_offset;
	adf_os_spinlock_t lock;
};

struct beacon_tim_ie {
	u_int8_t tim_ie;
	u_int8_t tim_len;
	u_int8_t dtim_count;
	u_int8_t dtim_period;
	u_int8_t tim_bitctl;
	u_int8_t tim_bitmap[1];
} __ATTRIB_PACK;

#define WMA_TIM_SUPPORTED_PVB_LENGTH (HAL_NUM_STA / 8) + 1


struct pps {
	v_BOOL_t paid_match_enable;
	v_BOOL_t gid_match_enable;
	v_BOOL_t tim_clear;
	v_BOOL_t dtim_clear;
	v_BOOL_t eof_delim;
	v_BOOL_t mac_match;
	v_BOOL_t delim_fail;
	v_BOOL_t nsts_zero;
	v_BOOL_t rssi_chk;
	v_BOOL_t ebt_5g;
};

struct qpower_params {
	u_int32_t max_ps_poll_cnt;
	u_int32_t max_tx_before_wake;
	u_int32_t spec_ps_poll_wake_interval;
	u_int32_t max_spec_nodata_ps_poll;
};

typedef struct {
	u_int32_t gtxRTMask[2]; /* for HT and VHT rate masks */
	u_int32_t gtxUsrcfg; /* host request for GTX mask */
	u_int32_t gtxPERThreshold; /* default: 10% */
	u_int32_t gtxPERMargin; /* default: 2% */
	u_int32_t gtxTPCstep; /* default: 1 */
	u_int32_t gtxTPCMin; /* default: 5 */
	u_int32_t gtxBWMask; /* 20/40/80/160 Mhz */
}gtx_config_t;

typedef struct {
	u_int32_t ani_enable;
	u_int32_t ani_poll_len;
	u_int32_t ani_listen_len;
	u_int32_t ani_ofdm_level;
	u_int32_t ani_cck_level;
	u_int32_t cwmenable;
	u_int32_t cts_cbw;
	u_int32_t txchainmask;
	u_int32_t rxchainmask;
	u_int32_t txpow2g;
	u_int32_t txpow5g;
	u_int32_t burst_enable;
	u_int32_t burst_dur;
	u_int32_t chainmask_2g;
	u_int32_t chainmask_5g;
} pdev_cli_config_t;

typedef struct {
	u_int32_t nss;
	u_int32_t ldpc;
	u_int32_t tx_stbc;
	u_int32_t rx_stbc;
	u_int32_t shortgi;
	u_int32_t rtscts_en;
	u_int32_t chwidth;
	u_int32_t tx_rate;
	u_int32_t ampdu;
	u_int32_t amsdu;
	u_int32_t erx_adjust;
	u_int32_t erx_bmiss_num;
	u_int32_t erx_bmiss_cycle;
	u_int32_t erx_slop_step;
	u_int32_t erx_init_slop;
	u_int32_t erx_adj_pause;
	u_int32_t erx_dri_sample;
        struct pps pps_params;
	struct qpower_params qpower_params;
	gtx_config_t gtx_info;
} vdev_cli_config_t;

#define WMA_WOW_PTRN_MASK_VALID     0xFF
#define WMA_NUM_BITS_IN_BYTE           8

#define WMA_AP_WOW_DEFAULT_PTRN_MAX    4
#define WMA_STA_WOW_DEFAULT_PTRN_MAX   4

struct wma_wow_ptrn_cache {
	u_int8_t vdev_id;
	u_int8_t *ptrn;
	u_int8_t ptrn_len;
	u_int8_t ptrn_offset;
	u_int8_t *mask;
	u_int8_t mask_len;
};

struct wma_wow {
	struct wma_wow_ptrn_cache *cache[WOW_MAX_BITMAP_FILTERS];
	u_int8_t no_of_ptrn_cached;

	u_int8_t free_ptrn_id[WOW_MAX_BITMAP_FILTERS];
	u_int8_t total_free_ptrn_id;
	u_int8_t used_free_ptrn_id;

	v_BOOL_t magic_ptrn_enable;
	v_BOOL_t wow_enable;
	v_BOOL_t wow_enable_cmd_sent;
	v_BOOL_t deauth_enable;
	v_BOOL_t disassoc_enable;
	v_BOOL_t bmiss_enable;
	v_BOOL_t gtk_pdev_enable;
	v_BOOL_t gtk_err_enable[WMA_MAX_SUPPORTED_BSS];
#ifdef FEATURE_WLAN_LPHB
	/* currently supports only vdev 0.
	 * cache has two entries: one for TCP and one for UDP.
	 */
	tSirLPHBReq lphb_cache[2];
#endif
};
#ifdef WLAN_FEATURE_11W
#define CMAC_IPN_LEN         (6)
#define WMA_IGTK_KEY_INDEX_4 (4)
#define WMA_IGTK_KEY_INDEX_5 (5)

typedef struct {
	u_int8_t  ipn[CMAC_IPN_LEN];
} wma_igtk_ipn_t;

typedef struct {
	u_int16_t key_length;
	u_int8_t  key[CSR_AES_KEY_LEN];

	/* IPN is maintained per iGTK keyID
	 * 0th index for iGTK keyID = 4;
	 * 1st index for iGTK KeyID = 5
	*/
        wma_igtk_ipn_t key_id[2];
} wma_igtk_key_t;
#endif

#define WMA_BSS_STATUS_STARTED 0x1
#define WMA_BSS_STATUS_STOPPED 0x2

typedef struct {
	A_UINT32 vdev_id;
	wmi_ssid ssid;
	A_UINT32 flags;
	A_UINT32 requestor_id;
	A_UINT32  disable_hw_ack;
	wmi_channel chan;
	adf_os_atomic_t hidden_ssid_restart_in_progress;
	tANI_U8 ssidHidden;
} vdev_restart_params_t;

struct wma_txrx_node {
	u_int8_t addr[ETH_ALEN];
	u_int8_t bssid[ETH_ALEN];
	void *handle;
	struct beacon_info *beacon;
	vdev_restart_params_t vdev_restart_params;
	vdev_cli_config_t config;
	struct scan_param scan_info;
	u_int32_t type;
	u_int32_t sub_type;
#ifdef FEATURE_WLAN_SCAN_PNO
	v_BOOL_t nlo_match_evt_received;
	v_BOOL_t pno_in_progress;
#endif
#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
	v_BOOL_t plm_in_progress;
#endif
	v_BOOL_t ptrn_match_enable;
	v_BOOL_t conn_state;
	/* BSS parameters cached for use in WDA_ADD_STA */
	tSirMacBeaconInterval   beaconInterval;
	tANI_U8                 llbCoexist;
	tANI_U8                 shortSlotTimeSupported;
	tANI_U8                 dtimPeriod;
	WLAN_PHY_MODE           chanmode;
	tANI_U8                 vht_capable;
	tANI_U8                 ht_capable;
	A_UINT32                mhz; /* channel frequency  in KHZ */
	v_BOOL_t vdev_up;
	u_int64_t tsfadjust;
	void     *addBssStaContext;
	tANI_U8 aid;
	/* Robust Management Frame (RMF) enabled/disabled */
	tANI_U8 rmfEnabled;
#ifdef WLAN_FEATURE_11W
	wma_igtk_key_t key;
#endif /* WLAN_FEATURE_11W */
	u_int32_t uapsd_cached_val;
	tAniGetPEStatsRsp       *stats_rsp;
	tANI_U8                 fw_stats_set;
	void *del_staself_req;
	adf_os_atomic_t bss_status;
	tANI_U8 rate_flags;
	tANI_U8 nss;
	v_BOOL_t is_channel_switch;
	u_int16_t pause_bitmap;
	tPowerdBm  tx_power; /* TX power in dBm */
	tPowerdBm  max_tx_power; /* max Tx power in dBm */
        u_int32_t  nwType;
#if defined WLAN_FEATURE_VOWIFI_11R
        void    *staKeyParams;
#endif
	v_BOOL_t ps_enabled;
	u_int32_t dtim_policy;
	u_int32_t peer_count;
	v_BOOL_t roam_synch_in_progress;
	void *plink_status_req;
	void *psnr_req;
	u_int8_t delay_before_vdev_stop;
#ifdef FEATURE_WLAN_EXTSCAN
	bool extscan_in_progress;
#endif
	uint32_t alt_modulated_dtim;
	bool alt_modulated_dtim_enabled;
	uint8_t wps_state;
	uint8_t nss_2g;
	uint8_t nss_5g;

	uint8_t wep_default_key_idx;
};

#if defined(QCA_WIFI_FTM)
#define MAX_UTF_EVENT_LENGTH	2048
#define MAX_WMI_UTF_LEN		252
#define SYS_MSG_COOKIE		(0xFACE)

typedef struct {
	A_UINT32 len;
	A_UINT32 msgref;
	A_UINT32 segmentInfo;
	A_UINT32 pad;
} SEG_HDR_INFO_STRUCT;

struct utf_event_info {
	u_int8_t *data;
	u_int32_t length;
	adf_os_size_t offset;
	u_int8_t currentSeq;
	u_int8_t expectedSeq;
};
#endif

typedef struct {
	u_int8_t vdev_id;
	u_int32_t scan_id;
}scan_timer_info;

typedef struct {
	u_int32_t atimWindowLength;
	u_int32_t isPowerSaveAllowed;
	u_int32_t isPowerCollapseAllowed;
	u_int32_t isAwakeonTxRxEnabled;
	u_int32_t inactivityCount;
	u_int32_t txSPEndInactivityTime;
	u_int32_t ibssPsWarmupTime;
	u_int32_t ibssPs1RxChainInAtimEnable;
}ibss_power_save_params;

#define MAX_REQUEST_HANDLERS 20

struct wma_handle;
typedef VOS_STATUS (*wma_request_handler)(struct wma_handle *wma_handle,
			void *request, void **response);

typedef struct request_handler_entry {
	int request_type;
	wma_request_handler handler;
} request_handler_entry_t;

struct wma_runtime_pm_context {
	void *ap;
	void *resume;
};

typedef struct wma_handle {
	void *wmi_handle;
	void *htc_handle;
	void *vos_context;
	void *mac_context;

	vos_event_t wma_ready_event;
	vos_event_t wma_resume_event;
	vos_event_t target_suspend;
	vos_event_t wow_tx_complete;
	vos_event_t runtime_suspend;
	vos_event_t recovery_event;

	t_cfg_nv_param cfg_nv;

	v_U16_t max_station;
	v_U16_t max_bssid;
	v_U32_t frame_xln_reqd;
	t_wma_drv_type driver_type;

	/* TODO: Check below 2 parameters are required for ROME/PRONTO ? */
	u_int8_t myaddr[ETH_ALEN]; /* current mac address */
	u_int8_t hwaddr[ETH_ALEN]; /* mac address from EEPROM */

	wmi_abi_version target_abi_vers; /* The target firmware version */
	/* The final negotiated ABI version to be used for communicating */
	wmi_abi_version final_abi_vers;
	v_U32_t target_fw_version; /* Target f/w build version */
#ifdef WLAN_FEATURE_LPSS
	v_U8_t lpss_support; /* LPSS feature is supported in target or not */
#endif
	uint8_t ap_arpns_support;
#ifdef FEATURE_GREEN_AP
	bool egap_support;
#endif
	bool wmi_ready;
	u_int32_t wlan_init_status;
	adf_os_device_t adf_dev;
	u_int32_t phy_capability; /* PHY Capability from Target*/
	u_int32_t max_frag_entry; /* Max number of Fragment entry */
	u_int32_t wmi_service_bitmap[WMI_SERVICE_BM_SIZE]; /* wmi services bitmap received from Target */
	wmi_resource_config   wlan_resource_config;
	u_int32_t frameTransRequired;
	tBssSystemRole       wmaGlobalSystemRole;

	/* Tx Frame Compl Cb registered by umac */
	pWDATxRxCompFunc tx_frm_download_comp_cb;

	/* Event to wait for tx download completion */
	vos_event_t tx_frm_download_comp_event;

	/*
	 * Dummy event to wait for draining MSDUs left in hardware tx
	 * queue and before requesting VDEV_STOP. Nobody will set this
	 * and wait will timeout, and code will poll the pending tx
	 * descriptors number to be zero.
	 */
	vos_event_t tx_queue_empty_event;

	/* Ack Complete Callback registered by umac */
	pWDAAckFnTxComp umac_ota_ack_cb[SIR_MAC_MGMT_RESERVED15];
	pWDAAckFnTxComp umac_data_ota_ack_cb;

	/* timestamp when OTA of last umac data was done */
	v_TIME_t last_umac_data_ota_timestamp;
	/* cache nbuf ptr for the last umac data buf */
	adf_nbuf_t last_umac_data_nbuf;

	v_BOOL_t needShutdown;
	u_int32_t num_mem_chunks;
	struct wma_mem_chunk mem_chunks[MAX_MEM_CHUNKS];
	wda_tgt_cfg_cb tgt_cfg_update_cb;
   /*Callback to indicate radar to HDD*/
   wda_dfs_radar_indication_cb dfs_radar_indication_cb;
	HAL_REG_CAPABILITIES reg_cap;
	u_int32_t scan_id;
	struct wma_txrx_node *interfaces;
	pdev_cli_config_t pdevconfig;
	struct list_head vdev_resp_queue;
	adf_os_spinlock_t vdev_respq_lock;
        adf_os_spinlock_t vdev_detach_lock;
	u_int32_t ht_cap_info;
#ifdef WLAN_FEATURE_11AC
	u_int32_t vht_cap_info;
	u_int32_t  vht_supp_mcs;
#endif
	u_int32_t num_rf_chains;

#if defined(QCA_WIFI_FTM)
	/* UTF event information */
	struct utf_event_info utf_event_info;
#endif
	u_int8_t is_fw_assert;
	struct wma_wow wow;
	u_int8_t no_of_suspend_ind;
	u_int8_t no_of_resume_ind;

	/* Have a back up of arp info to send along
	 * with ns info suppose if ns also enabled
	 */
	tSirHostOffloadReq mArpInfo;
	struct wma_tx_ack_work_ctx *ack_work_ctx;
	u_int8_t powersave_mode;
	v_BOOL_t ptrn_match_enable_all_vdev;
	void* pGetRssiReq;
	v_S7_t first_rssi;
	bool get_sta_rssi;
	v_MACADDR_t peer_macaddr;
	t_thermal_mgmt thermal_mgmt_info;
        v_BOOL_t  roam_offload_enabled;
        t_wma_roam_preauth_chan_state_t roam_preauth_scan_state;
        u_int32_t roam_preauth_scan_id;
        u_int16_t roam_preauth_chanfreq;
        void *roam_preauth_chan_context;
	adf_os_spinlock_t roam_preauth_lock;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	adf_os_spinlock_t roam_synch_lock;
#endif
	/* Here ol_ini_info is used to store ini
	 * status of arp offload, ns offload
	 * and others. Currently 1st bit is used
	 * for arp off load and 2nd bit for ns
	 * offload currently, rest bits are unused
	 */
	u_int8_t ol_ini_info;
	v_BOOL_t ssdp;
	bool enable_bcst_ptrn;
#ifdef FEATURE_RUNTIME_PM
	v_BOOL_t runtime_pm;
	u_int32_t auto_time;
#endif
        u_int8_t ibss_started;
        tSetBssKeyParams ibsskey_info;

   /*DFS umac interface information*/
   struct ieee80211com *dfs_ic;
#ifdef FEATURE_WLAN_SCAN_PNO
	vos_wake_lock_t pno_wake_lock;
#endif
#ifdef FEATURE_WLAN_EXTSCAN
	vos_wake_lock_t extscan_wake_lock;
#endif
	vos_wake_lock_t wow_wake_lock;
	int wow_nack;
	u_int32_t ap_client_cnt;
	adf_os_atomic_t is_wow_bus_suspended;

	vos_timer_t wma_scan_comp_timer;
	scan_timer_info wma_scan_timer_info;

	u_int8_t dfs_phyerr_filter_offload;
	v_BOOL_t suitable_ap_hb_failure;
	/* record the RSSI when suitable_ap_hb_failure for later usage to
	 * report RSSI at beacon miss scenario
	 */
	uint32_t suitable_ap_hb_failure_rssi;

	/* IBSS Power Save config Parameters */
	ibss_power_save_params wma_ibss_power_save_params;
#ifdef FEATURE_WLAN_RA_FILTERING
	v_BOOL_t IsRArateLimitEnabled;
	u_int16_t RArateLimitInterval;
#endif
#ifdef WLAN_FEATURE_LPSS
	bool is_lpass_enabled;
#endif

#ifdef WLAN_FEATURE_NAN
	bool is_nan_enabled;
#endif

	/* Powersave Configuration Parameters */
	u_int8_t staMaxLIModDtim;
	u_int8_t staModDtim;
	u_int8_t staDynamicDtim;

	int32_t dfs_pri_multiplier;

	u_int32_t hw_bd_id;
	u_int32_t hw_bd_info[HW_BD_INFO_SIZE];

#ifdef FEATURE_WLAN_D0WOW
	atomic_t in_d0wow;
#endif

	/* OCB request contexts */
	struct sir_ocb_config *ocb_config_req;

	uint32_t miracast_value;
	vos_timer_t log_completion_timer;
	uint32_t txrx_chainmask;
	uint8_t per_band_chainmask_supp;
	uint16_t self_gen_frm_pwr;
	bool tx_chain_mask_cck;

	uint32_t num_of_diag_events_logs;
	uint32_t *events_logs_list;

	uint32_t wow_pno_match_wake_up_count;
	uint32_t wow_pno_complete_wake_up_count;
	uint32_t wow_gscan_wake_up_count;
	uint32_t wow_low_rssi_wake_up_count;
	uint32_t wow_rssi_breach_wake_up_count;
	uint32_t wow_ucast_wake_up_count;
	uint32_t wow_bcast_wake_up_count;
	uint32_t wow_ipv4_mcast_wake_up_count;
	uint32_t wow_ipv6_mcast_wake_up_count;
	uint32_t wow_ipv6_mcast_ra_stats;
	uint32_t wow_ipv6_mcast_ns_stats;
	uint32_t wow_ipv6_mcast_na_stats;
	uint32_t wow_wakeup_enable_mask;
	uint32_t wow_wakeup_disable_mask;
	uint16_t max_mgmt_tx_fail_count;

	struct wma_runtime_pm_context runtime_context;
}t_wma_handle, *tp_wma_handle;

struct wma_target_cap {
	u_int32_t wmi_service_bitmap[WMI_SERVICE_BM_SIZE]; /* wmi services bitmap received from Target */
	wmi_resource_config wlan_resource_config; /* default resource config,the os shim can overwrite it */
};

/********** The following structures are referenced from legacy prima code *********/
typedef enum {
	QWLAN_ISOC_START_CMDID = 0x4000,
	QWLAN_ISOC_END_CMDID   = 0x4FFF,

	FW_CFG_DOWNLOAD_REQ = QWLAN_ISOC_START_CMDID,
	FW_NV_DOWNLOAD_REQ,
	FW_WLAN_HAL_STOP_REQ,
	/* Add additional commands here */

	QWLAN_ISOC_MAX_CMDID = QWLAN_ISOC_END_CMDID - 1
}QWLAN_CMD_ID;
/* The shared memory between WDI and HAL is 4K so maximum data can be transferred
from WDI to HAL is 4K.This 4K should also include the Message header so sending 4K
of NV fragment is nt possbile.The next multiple of 1Kb is 3K */

typedef struct
{
  v_VOID_t *pConfigBuffer;

  /*Length of the config buffer above*/
  v_U16_t usConfigBufferLen;

  /*Production or FTM driver*/
  t_wma_drv_type driver_type;

  /*The user data passed in by UMAC, it will be sent back when the above
    function pointer will be called */
  v_VOID_t *pUserData;

  /*The user data passed in by UMAC, it will be sent back when the indication
    function pointer will be called */
  v_VOID_t *pIndUserData;
}t_wma_start_req;

/* Message types for messages exchanged between WDI and HAL */
typedef enum
{
   //Init/De-Init
   WLAN_HAL_START_REQ = 0,
   WLAN_HAL_START_RSP = 1,
   WLAN_HAL_STOP_REQ  = 2,
   WLAN_HAL_STOP_RSP  = 3,

   //Scan
   WLAN_HAL_INIT_SCAN_REQ    = 4,
   WLAN_HAL_INIT_SCAN_RSP    = 5,
   WLAN_HAL_START_SCAN_REQ   = 6,
   WLAN_HAL_START_SCAN_RSP   = 7 ,
   WLAN_HAL_END_SCAN_REQ     = 8,
   WLAN_HAL_END_SCAN_RSP     = 9,
   WLAN_HAL_FINISH_SCAN_REQ  = 10,
   WLAN_HAL_FINISH_SCAN_RSP  = 11,

   // HW STA configuration/deconfiguration
   WLAN_HAL_CONFIG_STA_REQ   = 12,
   WLAN_HAL_CONFIG_STA_RSP   = 13,
   WLAN_HAL_DELETE_STA_REQ   = 14,
   WLAN_HAL_DELETE_STA_RSP   = 15,
   WLAN_HAL_CONFIG_BSS_REQ   = 16,
   WLAN_HAL_CONFIG_BSS_RSP   = 17,
   WLAN_HAL_DELETE_BSS_REQ   = 18,
   WLAN_HAL_DELETE_BSS_RSP   = 19,

   //Infra STA asscoiation
   WLAN_HAL_JOIN_REQ         = 20,
   WLAN_HAL_JOIN_RSP         = 21,
   WLAN_HAL_POST_ASSOC_REQ   = 22,
   WLAN_HAL_POST_ASSOC_RSP   = 23,

   //Security
   WLAN_HAL_SET_BSSKEY_REQ   = 24,
   WLAN_HAL_SET_BSSKEY_RSP   = 25,
   WLAN_HAL_SET_STAKEY_REQ   = 26,
   WLAN_HAL_SET_STAKEY_RSP   = 27,
   WLAN_HAL_RMV_BSSKEY_REQ   = 28,
   WLAN_HAL_RMV_BSSKEY_RSP   = 29,
   WLAN_HAL_RMV_STAKEY_REQ   = 30,
   WLAN_HAL_RMV_STAKEY_RSP   = 31,

   //Qos Related
   WLAN_HAL_ADD_TS_REQ          = 32,
   WLAN_HAL_ADD_TS_RSP          = 33,
   WLAN_HAL_DEL_TS_REQ          = 34,
   WLAN_HAL_DEL_TS_RSP          = 35,
   WLAN_HAL_UPD_EDCA_PARAMS_REQ = 36,
   WLAN_HAL_UPD_EDCA_PARAMS_RSP = 37,
   WLAN_HAL_ADD_BA_REQ          = 38,
   WLAN_HAL_ADD_BA_RSP          = 39,
   WLAN_HAL_DEL_BA_REQ          = 40,
   WLAN_HAL_DEL_BA_RSP          = 41,

   WLAN_HAL_CH_SWITCH_REQ       = 42,
   WLAN_HAL_CH_SWITCH_RSP       = 43,
   WLAN_HAL_SET_LINK_ST_REQ     = 44,
   WLAN_HAL_SET_LINK_ST_RSP     = 45,
   WLAN_HAL_GET_STATS_REQ       = 46,
   WLAN_HAL_GET_STATS_RSP       = 47,
   WLAN_HAL_UPDATE_CFG_REQ      = 48,
   WLAN_HAL_UPDATE_CFG_RSP      = 49,

   WLAN_HAL_MISSED_BEACON_IND           = 50,
   WLAN_HAL_UNKNOWN_ADDR2_FRAME_RX_IND  = 51,
   WLAN_HAL_MIC_FAILURE_IND             = 52,
   WLAN_HAL_FATAL_ERROR_IND             = 53,
   WLAN_HAL_SET_KEYDONE_MSG             = 54,

   //NV Interface
   WLAN_HAL_DOWNLOAD_NV_REQ             = 55,
   WLAN_HAL_DOWNLOAD_NV_RSP             = 56,

   WLAN_HAL_ADD_BA_SESSION_REQ          = 57,
   WLAN_HAL_ADD_BA_SESSION_RSP          = 58,
   WLAN_HAL_TRIGGER_BA_REQ              = 59,
   WLAN_HAL_TRIGGER_BA_RSP              = 60,
   WLAN_HAL_UPDATE_BEACON_REQ           = 61,
   WLAN_HAL_UPDATE_BEACON_RSP           = 62,
   WLAN_HAL_SEND_BEACON_REQ             = 63,
   WLAN_HAL_SEND_BEACON_RSP             = 64,

   WLAN_HAL_SET_BCASTKEY_REQ               = 65,
   WLAN_HAL_SET_BCASTKEY_RSP               = 66,
   WLAN_HAL_DELETE_STA_CONTEXT_IND         = 67,
   WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_REQ  = 68,
   WLAN_HAL_UPDATE_PROBE_RSP_TEMPLATE_RSP  = 69,

  // PTT interface support
   WLAN_HAL_PROCESS_PTT_REQ   = 70,
   WLAN_HAL_PROCESS_PTT_RSP   = 71,

   WLAN_HAL_TL_HAL_FLUSH_AC_REQ     = 74,
   WLAN_HAL_TL_HAL_FLUSH_AC_RSP     = 75,

   WLAN_HAL_ENTER_IMPS_REQ           = 76,
   WLAN_HAL_EXIT_IMPS_REQ            = 77,
   WLAN_HAL_ENTER_BMPS_REQ           = 78,
   WLAN_HAL_EXIT_BMPS_REQ            = 79,
   WLAN_HAL_ENTER_UAPSD_REQ          = 80,
   WLAN_HAL_EXIT_UAPSD_REQ           = 81,
   WLAN_HAL_UPDATE_UAPSD_PARAM_REQ   = 82,
   WLAN_HAL_CONFIGURE_RXP_FILTER_REQ = 83,
   WLAN_HAL_ADD_BCN_FILTER_REQ       = 84,
   WLAN_HAL_REM_BCN_FILTER_REQ       = 85,
   WLAN_HAL_ADD_WOWL_BCAST_PTRN      = 86,
   WLAN_HAL_DEL_WOWL_BCAST_PTRN      = 87,
   WLAN_HAL_ENTER_WOWL_REQ           = 88,
   WLAN_HAL_EXIT_WOWL_REQ            = 89,
   WLAN_HAL_HOST_OFFLOAD_REQ         = 90,
   WLAN_HAL_SET_RSSI_THRESH_REQ      = 91,
   WLAN_HAL_GET_RSSI_REQ             = 92,
   WLAN_HAL_SET_UAPSD_AC_PARAMS_REQ  = 93,
   WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_REQ = 94,

   WLAN_HAL_ENTER_IMPS_RSP           = 95,
   WLAN_HAL_EXIT_IMPS_RSP            = 96,
   WLAN_HAL_ENTER_BMPS_RSP           = 97,
   WLAN_HAL_EXIT_BMPS_RSP            = 98,
   WLAN_HAL_ENTER_UAPSD_RSP          = 99,
   WLAN_HAL_EXIT_UAPSD_RSP           = 100,
   WLAN_HAL_SET_UAPSD_AC_PARAMS_RSP  = 101,
   WLAN_HAL_UPDATE_UAPSD_PARAM_RSP   = 102,
   WLAN_HAL_CONFIGURE_RXP_FILTER_RSP = 103,
   WLAN_HAL_ADD_BCN_FILTER_RSP       = 104,
   WLAN_HAL_REM_BCN_FILTER_RSP       = 105,
   WLAN_HAL_SET_RSSI_THRESH_RSP      = 106,
   WLAN_HAL_HOST_OFFLOAD_RSP         = 107,
   WLAN_HAL_ADD_WOWL_BCAST_PTRN_RSP  = 108,
   WLAN_HAL_DEL_WOWL_BCAST_PTRN_RSP  = 109,
   WLAN_HAL_ENTER_WOWL_RSP           = 110,
   WLAN_HAL_EXIT_WOWL_RSP            = 111,
   WLAN_HAL_RSSI_NOTIFICATION_IND    = 112,
   WLAN_HAL_GET_RSSI_RSP             = 113,
   WLAN_HAL_CONFIGURE_APPS_CPU_WAKEUP_STATE_RSP = 114,

   //11k related events
   WLAN_HAL_SET_MAX_TX_POWER_REQ   = 115,
   WLAN_HAL_SET_MAX_TX_POWER_RSP   = 116,

   //11R related msgs
   WLAN_HAL_AGGR_ADD_TS_REQ        = 117,
   WLAN_HAL_AGGR_ADD_TS_RSP        = 118,

   //P2P  WLAN_FEATURE_P2P
   WLAN_HAL_SET_P2P_GONOA_REQ      = 119,
   WLAN_HAL_SET_P2P_GONOA_RSP      = 120,

   //WLAN Dump commands
   WLAN_HAL_DUMP_COMMAND_REQ       = 121,
   WLAN_HAL_DUMP_COMMAND_RSP       = 122,

   //OEM_DATA FEATURE SUPPORT
   WLAN_HAL_START_OEM_DATA_REQ   = 123,
   WLAN_HAL_START_OEM_DATA_RSP   = 124,

   //ADD SELF STA REQ and RSP
   WLAN_HAL_ADD_STA_SELF_REQ       = 125,
   WLAN_HAL_ADD_STA_SELF_RSP       = 126,

   //DEL SELF STA SUPPORT
   WLAN_HAL_DEL_STA_SELF_REQ       = 127,
   WLAN_HAL_DEL_STA_SELF_RSP       = 128,

   // Coex Indication
   WLAN_HAL_COEX_IND               = 129,

   // Tx Complete Indication
   WLAN_HAL_OTA_TX_COMPL_IND       = 130,

   //Host Suspend/resume messages
   WLAN_HAL_HOST_SUSPEND_IND       = 131,
   WLAN_HAL_HOST_RESUME_REQ        = 132,
   WLAN_HAL_HOST_RESUME_RSP        = 133,

   WLAN_HAL_SET_TX_POWER_REQ       = 134,
   WLAN_HAL_SET_TX_POWER_RSP       = 135,
   WLAN_HAL_GET_TX_POWER_REQ       = 136,
   WLAN_HAL_GET_TX_POWER_RSP       = 137,

   WLAN_HAL_P2P_NOA_ATTR_IND       = 138,

   WLAN_HAL_ENABLE_RADAR_DETECT_REQ  = 139,
   WLAN_HAL_ENABLE_RADAR_DETECT_RSP  = 140,
   WLAN_HAL_GET_TPC_REPORT_REQ       = 141,
   WLAN_HAL_GET_TPC_REPORT_RSP       = 142,
   WLAN_HAL_RADAR_DETECT_IND         = 143,
   WLAN_HAL_RADAR_DETECT_INTR_IND    = 144,
   WLAN_HAL_KEEP_ALIVE_REQ           = 145,
   WLAN_HAL_KEEP_ALIVE_RSP           = 146,

   /*PNO messages*/
   WLAN_HAL_SET_PREF_NETWORK_REQ     = 147,
   WLAN_HAL_SET_PREF_NETWORK_RSP     = 148,
   WLAN_HAL_SET_RSSI_FILTER_REQ      = 149,
   WLAN_HAL_SET_RSSI_FILTER_RSP      = 150,
   WLAN_HAL_UPDATE_SCAN_PARAM_REQ    = 151,
   WLAN_HAL_UPDATE_SCAN_PARAM_RSP    = 152,
   WLAN_HAL_PREF_NETW_FOUND_IND      = 153,

   WLAN_HAL_SET_TX_PER_TRACKING_REQ  = 154,
   WLAN_HAL_SET_TX_PER_TRACKING_RSP  = 155,
   WLAN_HAL_TX_PER_HIT_IND           = 156,

   WLAN_HAL_8023_MULTICAST_LIST_REQ   = 157,
   WLAN_HAL_8023_MULTICAST_LIST_RSP   = 158,

   WLAN_HAL_SET_PACKET_FILTER_REQ     = 159,
   WLAN_HAL_SET_PACKET_FILTER_RSP     = 160,
   WLAN_HAL_PACKET_FILTER_MATCH_COUNT_REQ   = 161,
   WLAN_HAL_PACKET_FILTER_MATCH_COUNT_RSP   = 162,
   WLAN_HAL_CLEAR_PACKET_FILTER_REQ         = 163,
   WLAN_HAL_CLEAR_PACKET_FILTER_RSP         = 164,
   /*This is temp fix. Should be removed once
    * Host and Riva code is in sync*/
   WLAN_HAL_INIT_SCAN_CON_REQ               = 165,

   WLAN_HAL_SET_POWER_PARAMS_REQ            = 166,
   WLAN_HAL_SET_POWER_PARAMS_RSP            = 167,

   WLAN_HAL_TSM_STATS_REQ                   = 168,
   WLAN_HAL_TSM_STATS_RSP                   = 169,

   // wake reason indication (WOW)
   WLAN_HAL_WAKE_REASON_IND                 = 170,
   // GTK offload support
   WLAN_HAL_GTK_OFFLOAD_REQ                 = 171,
   WLAN_HAL_GTK_OFFLOAD_RSP                 = 172,
   WLAN_HAL_GTK_OFFLOAD_GETINFO_REQ         = 173,
   WLAN_HAL_GTK_OFFLOAD_GETINFO_RSP         = 174,

   WLAN_HAL_FEATURE_CAPS_EXCHANGE_REQ       = 175,
   WLAN_HAL_FEATURE_CAPS_EXCHANGE_RSP       = 176,
   WLAN_HAL_EXCLUDE_UNENCRYPTED_IND         = 177,

   WLAN_HAL_SET_THERMAL_MITIGATION_REQ      = 178,
   WLAN_HAL_SET_THERMAL_MITIGATION_RSP      = 179,

  WLAN_HAL_UPDATE_VHT_OP_MODE_REQ          = 182,
  WLAN_HAL_UPDATE_VHT_OP_MODE_RSP          = 183,

   WLAN_HAL_P2P_NOA_START_IND               = 184,

   WLAN_HAL_CLASS_B_STATS_IND               = 187,
   WLAN_HAL_DEL_BA_IND                      = 188,
   WLAN_HAL_DHCP_START_IND                  = 189,
   WLAN_HAL_DHCP_STOP_IND                   = 190,

  WLAN_HAL_MSG_MAX = WLAN_HAL_MSG_TYPE_MAX_ENUM_SIZE
}tHalHostMsgType;

/* Enumeration for Version */
typedef enum
{
   WLAN_HAL_MSG_VERSION0 = 0,
   WLAN_HAL_MSG_VERSION1 = 1,
   WLAN_HAL_MSG_WCNSS_CTRL_VERSION = 0x7FFF, /*define as 2 bytes data*/
   WLAN_HAL_MSG_VERSION_MAX_FIELD  = WLAN_HAL_MSG_WCNSS_CTRL_VERSION
} tHalHostMsgVersion;

/* 4-byte control message header used by HAL*/
typedef PACKED_PRE struct PACKED_POST
{
   tHalHostMsgType  msgType:16;
   tHalHostMsgVersion msgVersion:16;
   tANI_U32         msgLen;
} tHalMsgHeader, *tpHalMsgHeader;

/*---------------------------------------------------------------------------
  WLAN_HAL_START_REQ
  ---------------------------------------------------------------------------*/

typedef PACKED_PRE struct PACKED_POST sHalMacStartParameter
{
	/* Drive Type - Production or FTM etc */
	tDriverType  driverType;

	/*Length of the config buffer*/
	tANI_U32  uConfigBufferLen;

	/* Following this there is a TLV formatted buffer of length
	 * "uConfigBufferLen" bytes containing all config values.
	 * The TLV is expected to be formatted like this:
	 * 0           15            31           31+CFG_LEN-1        length-1
	 * |   CFG_ID   |   CFG_LEN   |   CFG_BODY    |  CFG_ID  |......|
	 */
} tHalMacStartParameter, *tpHalMacStartParameter;

typedef PACKED_PRE struct PACKED_POST
{
   /* Note: The length specified in tHalMacStartReqMsg messages should be
    * header.msgLen = sizeof(tHalMacStartReqMsg) + uConfigBufferLen */
   tHalMsgHeader header;
   tHalMacStartParameter startReqParams;
}  tHalMacStartReqMsg, *tpHalMacStartReqMsg;

extern v_BOOL_t sys_validateStaConfig(void *pImage, unsigned long cbFile,
                               void **ppStaConfig, v_SIZE_t *pcbStaConfig);
extern void vos_WDAComplete_cback(v_PVOID_t pVosContext);
extern void wma_send_regdomain_info(u_int32_t reg_dmn, u_int16_t regdmn2G,
				    u_int16_t regdmn5G, int8_t ctl2G,
				    int8_t ctl5G);

void wma_get_modeselect(tp_wma_handle wma, u_int32_t *modeSelect);

void wma_set_dfs_regdomain(tp_wma_handle wma, uint8_t dfs_region);

/**
  * Frame index
  */
enum frame_index {
	GENERIC_NODOWNLD_NOACK_COMP_INDEX,
	GENERIC_DOWNLD_COMP_NOACK_COMP_INDEX,
	GENERIC_DOWNLD_COMP_ACK_COMP_INDEX,
	GENERIC_NODOWLOAD_ACK_COMP_INDEX,
	FRAME_INDEX_MAX
};

VOS_STATUS wma_update_vdev_tbl(tp_wma_handle wma_handle, u_int8_t vdev_id,
		ol_txrx_vdev_handle tx_rx_vdev_handle, u_int8_t *mac,
		u_int32_t vdev_type, bool add_del);

int32_t regdmn_get_regdmn_for_country(u_int8_t *alpha2);
void regdmn_get_ctl_info(struct regulatory *reg, u_int32_t modesAvail,
			 u_int32_t modeSelect);

/*get the ctl from regdomain*/
u_int8_t regdmn_get_ctl_for_regdmn(u_int32_t reg_dmn);
u_int16_t get_regdmn_5g(u_int32_t reg_dmn);

#define WMA_FW_PHY_STATS	0x1
#define WMA_FW_RX_REORDER_STATS 0x2
#define WMA_FW_RX_RC_STATS	0x3
#define WMA_FW_TX_PPDU_STATS	0x4
#define WMA_FW_TX_CONCISE_STATS 0x5
#define WMA_FW_TX_RC_STATS	0x6
#define WMA_FW_RX_REM_RING_BUF 0xc
#define WMA_FW_RX_TXBF_MUSU_NDPA 0xf
#define WMA_FW_TXRX_FWSTATS_RESET 0x1f

/*
 * Setting the Tx Comp Timeout to 1 secs.
 * TODO: Need to Revist the Timing
 */
#define WMA_TX_FRAME_COMPLETE_TIMEOUT  1000
#define WMA_TX_FRAME_BUFFER_NO_FREE    0
#define WMA_TX_FRAME_BUFFER_FREE       1

struct wma_tx_ack_work_ctx {
	tp_wma_handle wma_handle;
	u_int16_t sub_type;
	int32_t status;
	struct work_struct ack_cmp_work;
};

#define WMA_TARGET_REQ_TYPE_VDEV_START 0x1
#define WMA_TARGET_REQ_TYPE_VDEV_STOP  0x2
#define WMA_TARGET_REQ_TYPE_VDEV_DEL   0x3

#define WMA_VDEV_START_REQUEST_TIMEOUT (3000) /* 3 seconds */
#define WMA_VDEV_STOP_REQUEST_TIMEOUT  (3000) /* 3 seconds */

struct wma_target_req {
	vos_timer_t event_timeout;
	struct list_head node;
	void *user_data;
	u_int32_t msg_type;
	u_int8_t vdev_id;
	u_int8_t type;
};

struct wma_vdev_start_req {
	u_int32_t beacon_intval;
	u_int32_t dtim_period;
	int32_t max_txpow;
	ePhyChanBondState chan_offset;
	bool is_dfs;
	u_int8_t vdev_id;
	u_int8_t chan;
	u_int8_t oper_mode;
	tSirMacSSid ssid;
	u_int8_t hidden_ssid;
	u_int8_t pmf_enabled;
	u_int8_t vht_capable;
	u_int8_t ht_capable;
	int32_t dfs_pri_multiplier;
	u_int8_t dot11_mode;
	bool is_half_rate;
	bool is_quarter_rate;
};

struct wma_set_key_params {
	u_int8_t vdev_id;
	/* def_key_idx can be used to see if we have to read the key from cfg */
	u_int32_t def_key_idx;
	u_int16_t key_len;
	u_int8_t peer_mac[ETH_ALEN];
	u_int8_t singl_tid_rc;
	enum eAniEdType key_type;
	u_int32_t key_idx;
	bool unicast;
	u_int8_t key_data[SIR_MAC_MAX_KEY_LENGTH];
};

typedef struct {
	u_int16_t minTemp;
	u_int16_t maxTemp;
	u_int8_t thermalEnable;
} t_thermal_cmd_params, *tp_thermal_cmd_params;

/* Powersave Related */
/* Default InActivity Time is 200 ms */
#define POWERSAVE_DEFAULT_INACTIVITY_TIME 200

/* Default Listen Interval */
#define POWERSAVE_DEFAULT_LISTEN_INTERVAL 1

/*
 * TODO: Add WMI_CMD_ID_MAX as part of WMI_CMD_ID
 * instead of assigning it to the last valid wmi
 * cmd+1 to avoid updating this when a command is
 * added/deleted.
 */
#define WMI_CMDID_MAX (WMI_TXBF_CMDID + 1)

/*
 * wma cmd ids for configuration request which
 * does not involve sending a wmi command.
 */
enum wma_cfg_cmd_id {
	WMA_VDEV_TXRX_FWSTATS_ENABLE_CMDID = WMI_CMDID_MAX,
	WMA_VDEV_TXRX_FWSTATS_RESET_CMDID,
	/* Set time latency and time quota for MCC home channels */
	WMA_VDEV_MCC_SET_TIME_LATENCY,
	WMA_VDEV_MCC_SET_TIME_QUOTA,

	/* IBSS Power Save Parameters */
	WMA_VDEV_IBSS_SET_ATIM_WINDOW_SIZE,
	WMA_VDEV_IBSS_SET_POWER_SAVE_ALLOWED,
	WMA_VDEV_IBSS_SET_POWER_COLLAPSE_ALLOWED,
	WMA_VDEV_IBSS_SET_AWAKE_ON_TX_RX,
	WMA_VDEV_IBSS_SET_INACTIVITY_TIME,
	WMA_VDEV_IBSS_SET_TXSP_END_INACTIVITY_TIME,
	WMA_VDEV_IBSS_PS_SET_WARMUP_TIME_SECS,
	WMA_VDEV_IBSS_PS_SET_1RX_CHAIN_IN_ATIM_WINDOW,

	/* dfs control interface */
	WMA_VDEV_DFS_CONTROL_CMDID,
	WMA_VDEV_TXRX_GET_IPA_UC_FW_STATS_CMDID,

	/* Add any new command before this */
	WMA_CMD_ID_MAX
};

typedef struct wma_trigger_uapsd_params
{
	u_int32_t wmm_ac;
	u_int32_t user_priority;
	u_int32_t service_interval;
	u_int32_t suspend_interval;
	u_int32_t delay_interval;
}t_wma_trigger_uapsd_params, *tp_wma_trigger_uapsd_params;

VOS_STATUS wma_trigger_uapsd_params(tp_wma_handle wma_handle, u_int32_t vdev_id,
			tp_wma_trigger_uapsd_params trigger_uapsd_params);

/* added to get average snr for both data and beacon */
VOS_STATUS wma_send_snr_request(tp_wma_handle wma_handle, void *pGetRssiReq,
				v_S7_t first_rssi);


#define WMA_NLO_FREQ_THRESH          1000         /* in MHz */
#define WMA_SEC_TO_MSEC(sec)         (sec * 1000) /* sec to msec */
#define WMA_MSEC_TO_USEC(msec)       (msec * 1000) /* msec to usec */

/* Default rssi threshold defined in CFG80211 */
#define WMA_RSSI_THOLD_DEFAULT   -300

#ifdef FEATURE_WLAN_SCAN_PNO
#define WMA_PNO_MATCH_WAKE_LOCK_TIMEOUT		(5 * 1000) /* in msec */
#define WMA_PNO_SCAN_COMPLETE_WAKE_LOCK_TIMEOUT	(2 * 1000) /* in msec */
#endif
#define WMA_AUTH_REQ_RECV_WAKE_LOCK_TIMEOUT	(5 * 1000) /* in msec */
#define WMA_ASSOC_REQ_RECV_WAKE_LOCK_DURATION	(5 * 1000) /* in msec */
#define WMA_DEAUTH_RECV_WAKE_LOCK_DURATION	(5 * 1000) /* in msec */
#define WMA_DISASSOC_RECV_WAKE_LOCK_DURATION	(5 * 1000) /* in msec */
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
#define WMA_AUTO_SHUTDOWN_WAKE_LOCK_DURATION    (5 * 1000) /* in msec */
#endif
#define WMA_BMISS_EVENT_WAKE_LOCK_DURATION      (4 * 1000) /* in msec */

/* U-APSD maximum service period of peer station */
enum uapsd_peer_param_max_sp {
	UAPSD_MAX_SP_LEN_UNLIMITED = 0,
	UAPSD_MAX_SP_LEN_2 = 2,
	UAPSD_MAX_SP_LEN_4 = 4,
	UAPSD_MAX_SP_LEN_6 = 6
};

/* U-APSD Enabled AC's of peer station */
enum uapsd_peer_param_enabled_ac {
	UAPSD_VO_ENABLED = 0x01,
	UAPSD_VI_ENABLED = 0x02,
	UAPSD_BK_ENABLED = 0x04,
	UAPSD_BE_ENABLED = 0x08
};

#define WMA_TXMIC_LEN 8
#define WMA_RXMIC_LEN 8

/*
 * Length = (2 octets for Index and CTWin/Opp PS) and
 * (13 octets for each NOA Descriptors)
 */

#define WMA_P2P_NOA_IE_OPP_PS_SET (0x80)
#define WMA_P2P_NOA_IE_CTWIN_MASK (0x7F)

#define WMA_P2P_IE_ID 0xdd
#define WMA_P2P_WFA_OUI { 0x50,0x6f,0x9a }
#define WMA_P2P_WFA_VER 0x09                 /* ver 1.0 */
#define WMA_WSC_OUI { 0x00,0x50,0xF2 }       /* Microsoft WSC OUI byte */

/* P2P Sub element defintions (according to table 5 of Wifi's P2P spec) */
#define WMA_P2P_SUB_ELEMENT_STATUS                    0
#define WMA_P2P_SUB_ELEMENT_MINOR_REASON              1
#define WMA_P2P_SUB_ELEMENT_CAPABILITY                2
#define WMA_P2P_SUB_ELEMENT_DEVICE_ID                 3
#define WMA_P2P_SUB_ELEMENT_GO_INTENT                 4
#define WMA_P2P_SUB_ELEMENT_CONFIGURATION_TIMEOUT     5
#define WMA_P2P_SUB_ELEMENT_LISTEN_CHANNEL            6
#define WMA_P2P_SUB_ELEMENT_GROUP_BSSID               7
#define WMA_P2P_SUB_ELEMENT_EXTENDED_LISTEN_TIMING    8
#define WMA_P2P_SUB_ELEMENT_INTENDED_INTERFACE_ADDR   9
#define WMA_P2P_SUB_ELEMENT_MANAGEABILITY             10
#define WMA_P2P_SUB_ELEMENT_CHANNEL_LIST              11
#define WMA_P2P_SUB_ELEMENT_NOA                       12
#define WMA_P2P_SUB_ELEMENT_DEVICE_INFO               13
#define WMA_P2P_SUB_ELEMENT_GROUP_INFO                14
#define WMA_P2P_SUB_ELEMENT_GROUP_ID                  15
#define WMA_P2P_SUB_ELEMENT_INTERFACE                 16
#define WMA_P2P_SUB_ELEMENT_OP_CHANNEL                17
#define WMA_P2P_SUB_ELEMENT_INVITATION_FLAGS          18
#define WMA_P2P_SUB_ELEMENT_VENDOR                    221

/* Macros for handling unaligned memory accesses */
#define P2PIE_PUT_LE16(a, val)          \
	do {                    \
		(a)[1] = ((u16) (val)) >> 8;    \
		(a)[0] = ((u16) (val)) & 0xff;  \
	} while (0)

#define P2PIE_PUT_LE32(a, val)                  \
	do {                            \
		(a)[3] = (u8) ((((u32) (val)) >> 24) & 0xff);   \
		(a)[2] = (u8) ((((u32) (val)) >> 16) & 0xff);   \
		(a)[1] = (u8) ((((u32) (val)) >> 8) & 0xff);    \
		(a)[0] = (u8) (((u32) (val)) & 0xff);       \
	} while (0)

/*
 * P2P IE structural definition.
 */
struct p2p_ie {
	u_int8_t  p2p_id;
	u_int8_t  p2p_len;
	u_int8_t  p2p_oui[3];
	u_int8_t  p2p_oui_type;
} __packed;

struct p2p_noa_descriptor {
	u_int8_t   type_count; /* 255: continuous schedule, 0: reserved */
	u_int32_t  duration ;  /* Absent period duration in micro seconds */
	u_int32_t  interval;   /* Absent period interval in micro seconds */
	u_int32_t  start_time; /* 32 bit tsf time when in starts */
} __packed;

struct p2p_sub_element_noa {
	u_int8_t p2p_sub_id;
	u_int8_t p2p_sub_len;
	u_int8_t index;           /* identifies instance of NOA su element */
	u_int8_t oppPS:1,         /* oppPS state of the AP */
		 ctwindow:7;      /* ctwindow in TUs */
	u_int8_t num_descriptors; /* number of NOA descriptors */
	struct p2p_noa_descriptor noa_descriptors[WMA_MAX_NOA_DESCRIPTORS];
};

struct wma_decap_info_t {
	u_int8_t hdr[sizeof(struct ieee80211_qosframe_addr4)];
	int32_t hdr_len;
};


#define WMA_DEFAULT_MAX_PSPOLL_BEFORE_WAKE 1

typedef enum {
	/* set packet power save */
	WMI_VDEV_PPS_PAID_MATCH = 0,
	WMI_VDEV_PPS_GID_MATCH = 1,
	WMI_VDEV_PPS_EARLY_TIM_CLEAR = 2,
	WMI_VDEV_PPS_EARLY_DTIM_CLEAR = 3,
	WMI_VDEV_PPS_EOF_PAD_DELIM = 4,
	WMI_VDEV_PPS_MACADDR_MISMATCH = 5,
	WMI_VDEV_PPS_DELIM_CRC_FAIL = 6,
	WMI_VDEV_PPS_GID_NSTS_ZERO = 7,
	WMI_VDEV_PPS_RSSI_CHECK = 8,
	WMI_VDEV_VHT_SET_GID_MGMT = 9,
	WMI_VDEV_PPS_5G_EBT = 10
} packet_power_save;

typedef enum {
    WMI_VDEV_PARAM_GTX_HT_MCS,
    WMI_VDEV_PARAM_GTX_VHT_MCS,
    WMI_VDEV_PARAM_GTX_USR_CFG,
    WMI_VDEV_PARAM_GTX_THRE,
    WMI_VDEV_PARAM_GTX_MARGIN,
    WMI_VDEV_PARAM_GTX_STEP,
    WMI_VDEV_PARAM_GTX_MINTPC,
    WMI_VDEV_PARAM_GTX_BW_MASK,
}green_tx_param;

#define WMA_DEFAULT_QPOWER_MAX_PSPOLL_BEFORE_WAKE 1
#define WMA_DEFAULT_QPOWER_TX_WAKE_THRESHOLD 2
#define WMA_DEFAULT_SIFS_BURST_DURATION      8160

#define WMA_VHT_PPS_PAID_MATCH 1
#define WMA_VHT_PPS_GID_MATCH 2
#define WMA_VHT_PPS_DELIM_CRC_FAIL 3

#ifdef FEATURE_WLAN_TDLS
typedef struct wma_tdls_params
{
	tANI_U32    vdev_id;
	tANI_U32    tdls_state;
	tANI_U32    notification_interval_ms;
	tANI_U32    tx_discovery_threshold;
	tANI_U32    tx_teardown_threshold;
	tANI_S32    rssi_teardown_threshold;
	tANI_S32    rssi_delta;
	tANI_U32    tdls_options;
	tANI_U32    peer_traffic_ind_window;
	tANI_U32    peer_traffic_response_timeout;
	tANI_U32    puapsd_mask;
	tANI_U32    puapsd_inactivity_time;
	tANI_U32    puapsd_rx_frame_threshold;
	uint32_t    teardown_notification_ms;
	uint32_t    tdls_peer_kickout_threshold;
} t_wma_tdls_params;

typedef struct {
	/** unique id identifying the VDEV */
	A_UINT32        vdev_id;
	/** peer MAC address */
	wmi_mac_addr    peer_macaddr;
	/** TDLS peer status (wma_tdls_peer_notification)*/
	A_UINT32        peer_status;
	/** TDLS peer reason (wma_tdls_peer_reason) */
	A_UINT32        peer_reason;
} wma_tdls_peer_event;

#endif /* FEATURE_WLAN_TDLS */

#define WMA_DFS_MAX_20M_SUB_CH 8

struct wma_dfs_radar_channel_list {
	A_UINT32	nchannels;
	/*Channel number including bonded channels on which the RADAR is present */
	u_int8_t	channels[WMA_DFS_MAX_20M_SUB_CH];
};

/*
 * Structure to indicate RADAR
 */

struct wma_dfs_radar_indication {
	/* unique id identifying the VDEV */
	A_UINT32        vdev_id;
	/* Channel list on which RADAR is detected */
	struct wma_dfs_radar_channel_list chan_list;
	/* Flag to Indicate RADAR presence on the
	 * current operating channel
	 */
	u_int32_t       dfs_radar_status;
	/* Flag to indicate use NOL */
	int             use_nol;
};

/*
 * WMA-DFS Hooks
 */
int ol_if_dfs_attach(struct ieee80211com *ic, void *ptr, void *radar_info);
u_int64_t ol_if_get_tsf64(struct ieee80211com *ic);
int ol_if_dfs_disable(struct ieee80211com *ic);
struct ieee80211_channel * ieee80211_find_channel(struct ieee80211com *ic,
                                     int freq, u_int32_t flags);
int ol_if_dfs_enable(struct ieee80211com *ic, int *is_fastclk, void *pe);
u_int32_t ieee80211_ieee2mhz(u_int32_t chan, u_int32_t flags);
int ol_if_dfs_get_ext_busy(struct ieee80211com *ic);
int ol_if_dfs_get_mib_cycle_counts_pct(struct ieee80211com *ic,
          u_int32_t *rxc_pcnt, u_int32_t *rxf_pcnt, u_int32_t *txf_pcnt);
u_int16_t ol_if_dfs_usenol(struct ieee80211com *ic);
void ieee80211_mark_dfs(struct ieee80211com *ic,
                               struct ieee80211_channel *ichan);
int  wma_dfs_indicate_radar(struct ieee80211com *ic,
                               struct ieee80211_channel *ichan);
u_int16_t   dfs_usenol(struct ieee80211com *ic);

#define WMA_SMPS_MASK_LOWER_16BITS 0xFF
#define WMA_SMPS_MASK_UPPER_3BITS 0x7
#define WMA_SMPS_PARAM_VALUE_S 29

#define WMA_MAX_SCAN_ID        0x00FF

/* U-APSD Access Categories */
enum uapsd_ac {
	UAPSD_BE,
	UAPSD_BK,
	UAPSD_VI,
	UAPSD_VO
};

VOS_STATUS wma_disable_uapsd_per_ac(tp_wma_handle wma_handle,
					u_int32_t vdev_id,
					enum uapsd_ac ac);

/* U-APSD User Priorities */
enum uapsd_up {
	UAPSD_UP_BE,
	UAPSD_UP_BK,
	UAPSD_UP_RESV,
	UAPSD_UP_EE,
	UAPSD_UP_CL,
	UAPSD_UP_VI,
	UAPSD_UP_VO,
	UAPSD_UP_NC,
	UAPSD_UP_MAX
};

#ifdef FEATURE_WLAN_D0WOW
void wma_set_d0wow_flag(tp_wma_handle wma_handle, A_BOOL flag);
A_BOOL wma_read_d0wow_flag(tp_wma_handle wma_handle);
#endif

A_UINT32 eCsrAuthType_to_rsn_authmode (eCsrAuthType authtype,
                                       eCsrEncryptionType encr);
A_UINT32 eCsrEncryptionType_to_rsn_cipherset (eCsrEncryptionType encr);

/*
 * The firmware value has been changed recently to 0x127
 * But, to maintain backward compatibility, the old
 * value is also preserved.
 */
#define WMA_TGT_INVALID_SNR_OLD (-1)
#define WMA_TGT_INVALID_SNR_NEW 0x127

#define WMA_TX_Q_RECHECK_TIMER_WAIT      2    // 2 ms
#define WMA_TX_Q_RECHECK_TIMER_MAX_WAIT  20   // 20 ms
#define WMA_MAX_NUM_ARGS 8
typedef struct wma_unit_test_cmd
{
    v_UINT_t vdev_id;
    WLAN_MODULE_ID module_id;
    v_U32_t num_args;
    v_U32_t args[WMA_MAX_NUM_ARGS];
}t_wma_unit_test_cmd;

typedef struct wma_roam_invoke_cmd
{
    v_UINT_t vdev_id;
    u_int8_t bssid[6];
    v_U32_t channel;
}t_wma_roam_invoke_cmd;

struct wma_target_req *wma_fill_vdev_req(tp_wma_handle wma, u_int8_t vdev_id,
	u_int32_t msg_type, u_int8_t type, void *params, u_int32_t timeout);

VOS_STATUS wma_vdev_start(tp_wma_handle wma, struct wma_vdev_start_req *req,
	v_BOOL_t isRestart);

void wma_remove_vdev_req(tp_wma_handle wma, u_int8_t vdev_id, u_int8_t type);

#ifdef REMOVE_PKT_LOG
static inline void wma_set_wifi_start_packet_stats(void *wma_handle,
					struct sir_wifi_start_log *start_log)
{
	return;
}
#endif

/* API's to enable HDD to suspsend FW.
 * This are active only if Bus Layer aggreed to suspend.
 * This will be called for only for SDIO driver, for others
 * by default HIF return failure, as we suspend FW in bus
 * suspend callbacks
 */
int wma_suspend_fw(void);
int wma_resume_fw(void);

void wma_send_flush_logs_to_fw(tp_wma_handle wma_handle);
struct wma_txrx_node *wma_get_interface_by_vdev_id(uint8_t vdev_id);
bool wma_is_vdev_up(uint8_t vdev_id);

int wma_crash_inject(tp_wma_handle wma_handle, uint32_t type,
			uint32_t delay_time_ms);

uint32_t wma_get_vht_ch_width(void);

#endif
