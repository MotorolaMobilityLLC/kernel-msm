/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

#if !defined(WLAN_HDD_MAIN_H)
#define WLAN_HDD_MAIN_H
/**
 * DOC: wlan_hdd_main.h
 *
 * Linux HDD Adapter Type
 */

/*
 * The following terms were in use in prior versions of the driver but
 * have now been replaced with terms that are aligned with the Linux
 * Coding style. Macros are defined to hopefully prevent new instances
 * from being introduced, primarily by code propagation.
 */
#define pHddCtx
#define pAdapter
#define pHostapdAdapter
#define pHddApCtx
#define pHddStaCtx
#define pHostapdState
#define pRoamInfo
#define pScanInfo
#define pBeaconIes

/*
 * Include files
 */

#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <net/cfg80211.h>
#include <linux/ieee80211.h>
#include <qdf_delayed_work.h>
#include <qdf_list.h>
#include <qdf_types.h>
#include "sir_mac_prot_def.h"
#include "csr_api.h"
#include "wlan_dsc.h"
#include <wlan_hdd_assoc.h>
#include <wlan_hdd_wmm.h>
#include <wlan_hdd_cfg.h>
#include <linux/spinlock.h>
#include <ani_system_defs.h>
#if defined(WLAN_OPEN_SOURCE) && defined(CONFIG_HAS_WAKELOCK)
#include <linux/wakelock.h>
#endif
#ifdef WLAN_FEATURE_TSF_PTP
#include <linux/ptp_classify.h>
#include <linux/ptp_clock_kernel.h>
#endif
#include <wlan_hdd_ftm.h>
#include "wlan_hdd_tdls.h"
#include "wlan_hdd_tsf.h"
#include "wlan_hdd_cfg80211.h"
#include "wlan_hdd_debugfs.h"
#include <qdf_defer.h>
#include "sap_api.h"
#include <wlan_hdd_lro.h>
#include "cdp_txrx_flow_ctrl_legacy.h"
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_misc.h>
#include "wlan_hdd_nan_datapath.h"
#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include "wlan_pmo_ucfg_api.h"
#ifdef WIFI_POS_CONVERGED
#include "os_if_wifi_pos.h"
#include "wifi_pos_api.h"
#else
#include "wlan_hdd_oemdata.h"
#endif
#include "wlan_hdd_he.h"

#include <net/neighbour.h>
#include <net/netevent.h>
#include "wlan_hdd_nud_tracking.h"
#include "wlan_hdd_twt.h"
#include "wma_sar_public_structs.h"
#include "wlan_mlme_ucfg_api.h"
#include "pld_common.h"
#include <dp_txrx.h>
#include "wlan_cm_roam_public_struct.h"

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
#include "qdf_periodic_work.h"
#endif

#if defined(CLD_PM_QOS) || defined(FEATURE_RUNTIME_PM)
#include <linux/pm_qos.h>
#endif

#include "wlan_hdd_sta_info.h"

/*
 * Preprocessor definitions and constants
 */

#ifdef FEATURE_WLAN_APF
/**
 * struct hdd_apf_context - hdd Context for apf
 * @magic: magic number
 * @qdf_apf_event: Completion variable for APF get operations
 * @capability_response: capabilities response received from fw
 * @apf_enabled: True: APF Interpreter enabled, False: Disabled
 * @cmd_in_progress: Flag that indicates an APF command is in progress
 * @buf: Buffer to accumulate read memory chunks
 * @buf_len: Length of the read memory requested
 * @offset: APF work memory offset to fetch from
 * @lock: APF Context lock
 */
struct hdd_apf_context {
	unsigned int magic;
	qdf_event_t qdf_apf_event;
	bool apf_enabled;
	bool cmd_in_progress;
	uint8_t *buf;
	uint32_t buf_len;
	uint32_t offset;
	qdf_spinlock_t lock;
};
#endif /* FEATURE_WLAN_APF */

/** Number of Tx Queues */
#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || \
	defined(QCA_HL_NETDEV_FLOW_CONTROL) || \
	defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
#define NUM_TX_QUEUES 5
#else
#define NUM_TX_QUEUES 4
#endif

/* HDD_IS_RATE_LIMIT_REQ: Macro helper to implement rate limiting
 * @flag: The flag to determine if limiting is required or not
 * @rate: The number of seconds within which if multiple commands come, the
 *	  flag will be set to true
 *
 * If the function in which this macro is used is called multiple times within
 * "rate" number of seconds, the "flag" will be set to true which can be used
 * to reject/take appropriate action.
 */
#define HDD_IS_RATE_LIMIT_REQ(flag, rate)\
	do {\
		static ulong __last_ticks;\
		ulong __ticks = jiffies;\
		flag = false; \
		if (!time_after(__ticks,\
		    __last_ticks + rate * HZ)) {\
			flag = true; \
		} \
		else { \
			__last_ticks = __ticks;\
		} \
	} while (0)

/*
 * API in_compat_syscall() is introduced in 4.6 kernel to check whether we're
 * in a compat syscall or not. It is a new way to query the syscall type, which
 * works properly on all architectures.
 *
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 6, 0))
static inline bool in_compat_syscall(void) { return is_compat_task(); }
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 7, 0)) || \
	defined(CFG80211_REMOVE_IEEE80211_BACKPORT)
#define HDD_NL80211_BAND_2GHZ   NL80211_BAND_2GHZ
#define HDD_NL80211_BAND_5GHZ   NL80211_BAND_5GHZ
#define HDD_NUM_NL80211_BANDS   NUM_NL80211_BANDS
#else
#define HDD_NL80211_BAND_2GHZ   IEEE80211_BAND_2GHZ
#define HDD_NL80211_BAND_5GHZ   IEEE80211_BAND_5GHZ
#define HDD_NUM_NL80211_BANDS   ((enum nl80211_band)IEEE80211_NUM_BANDS)
#endif

#if defined(CONFIG_BAND_6GHZ) && (defined(CFG80211_6GHZ_BAND_SUPPORTED) || \
	(KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE))
#define HDD_NL80211_BAND_6GHZ   NL80211_BAND_6GHZ
#endif

#define TSF_GPIO_PIN_INVALID 255

/** Length of the TX queue for the netdev */
#define HDD_NETDEV_TX_QUEUE_LEN (3000)

/** Hdd Tx Time out value */
#define HDD_TX_TIMEOUT          msecs_to_jiffies(5000)

#define HDD_TX_STALL_THRESHOLD 4

/** Hdd Default MTU */
#define HDD_DEFAULT_MTU         (1500)

#ifdef QCA_CONFIG_SMP
#define NUM_CPUS NR_CPUS
#else
#define NUM_CPUS 1
#endif

#define ACS_COMPLETE_TIMEOUT 3000

#define HDD_PSOC_IDLE_SHUTDOWN_SUSPEND_DELAY (1000)
/**
 * enum hdd_adapter_flags - event bitmap flags registered net device
 * @NET_DEVICE_REGISTERED: Adapter is registered with the kernel
 * @SME_SESSION_OPENED: Firmware vdev has been created
 * @INIT_TX_RX_SUCCESS: Adapter datapath is initialized
 * @WMM_INIT_DONE: Adapter is initialized
 * @SOFTAP_BSS_STARTED: Software Access Point (SAP) is running
 * @DEVICE_IFACE_OPENED: Adapter has been "opened" via the kernel
 * @SOFTAP_INIT_DONE: Software Access Point (SAP) is initialized
 * @VENDOR_ACS_RESPONSE_PENDING: Waiting for event for vendor acs
 */
enum hdd_adapter_flags {
	NET_DEVICE_REGISTERED,
	SME_SESSION_OPENED,
	INIT_TX_RX_SUCCESS,
	WMM_INIT_DONE,
	SOFTAP_BSS_STARTED,
	DEVICE_IFACE_OPENED,
	SOFTAP_INIT_DONE,
	VENDOR_ACS_RESPONSE_PENDING,
};

/**
 * enum hdd_nb_cmd_id - North bound command IDs received during SSR
 * @NO_COMMAND - No NB command received during SSR
 * @INTERFACE_DOWN - Received interface down during SSR
 */
enum hdd_nb_cmd_id {
	NO_COMMAND,
	INTERFACE_DOWN
};

#define WLAN_WAIT_DISCONNECT_ALREADY_IN_PROGRESS  1000
#define WLAN_WAIT_TIME_STOP_ROAM  4000
#define WLAN_WAIT_TIME_STATS       800
#define WLAN_WAIT_TIME_LINK_STATUS 800

/** Maximum time(ms) to wait for mc thread suspend **/
#define WLAN_WAIT_TIME_MCTHREAD_SUSPEND  1200

/** Maximum time(ms) to wait for target to be ready for suspend **/
#define WLAN_WAIT_TIME_READY_TO_SUSPEND  2000

/* Scan Req Timeout */
#define WLAN_WAIT_TIME_SCAN_REQ 100

#define WLAN_WAIT_TIME_APF     1000

#define WLAN_WAIT_TIME_FW_ROAM_STATS 1000

#define WLAN_WAIT_TIME_ANTENNA_ISOLATION 8000

/* Maximum time(ms) to wait for RSO CMD status event */
#define WAIT_TIME_RSO_CMD_STATUS 2000

/* rcpi request timeout in milli seconds */
#define WLAN_WAIT_TIME_RCPI 500

#define WLAN_WAIT_PEER_CLEANUP 5000

#define MAX_CFG_STRING_LEN  255

/* Maximum time(ms) to wait for external acs response */
#define WLAN_VENDOR_ACS_WAIT_TIME 1000

/* Maximum time(ms) to wait for monitor mode vdev up event completion*/
#define WLAN_MONITOR_MODE_VDEV_UP_EVT      SME_CMD_VDEV_START_BSS_TIMEOUT

/* Mac Address string length */
#define MAC_ADDRESS_STR_LEN 18  /* Including null terminator */
/* Max and min IEs length in bytes */
#define MAX_GENIE_LEN (512)
#define MIN_GENIE_LEN (2)

#define WPS_OUI_TYPE   "\x00\x50\xf2\x04"
#define WPS_OUI_TYPE_SIZE  4

#define P2P_OUI_TYPE   "\x50\x6f\x9a\x09"
#define P2P_OUI_TYPE_SIZE  4

#define HS20_OUI_TYPE   "\x50\x6f\x9a\x10"
#define HS20_OUI_TYPE_SIZE  4

#define OSEN_OUI_TYPE   "\x50\x6f\x9a\x12"
#define OSEN_OUI_TYPE_SIZE  4

#ifdef WLAN_FEATURE_WFD
#define WFD_OUI_TYPE   "\x50\x6f\x9a\x0a"
#define WFD_OUI_TYPE_SIZE  4
#endif

#define MBO_OUI_TYPE   "\x50\x6f\x9a\x16"
#define MBO_OUI_TYPE_SIZE  4

#define QCN_OUI_TYPE   "\x8c\xfd\xf0\x01"
#define QCN_OUI_TYPE_SIZE  4

#define wlan_hdd_get_wps_ie_ptr(ie, ie_len) \
	wlan_get_vendor_ie_ptr_from_oui(WPS_OUI_TYPE, WPS_OUI_TYPE_SIZE, \
	ie, ie_len)

#define hdd_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_HDD, params)
#define hdd_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_HDD, params)
#define hdd_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_HDD, params)
#define hdd_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_HDD, params)
#define hdd_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_HDD, params)

#define hdd_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_HDD, params)
#define hdd_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_HDD, params)
#define hdd_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_HDD, params)
#define hdd_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_HDD, params)
#define hdd_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_HDD, params)

#define hdd_alert_rl(params...) QDF_TRACE_FATAL_RL(QDF_MODULE_ID_HDD, params)
#define hdd_err_rl(params...) QDF_TRACE_ERROR_RL(QDF_MODULE_ID_HDD, params)
#define hdd_warn_rl(params...) QDF_TRACE_WARN_RL(QDF_MODULE_ID_HDD, params)
#define hdd_info_rl(params...) QDF_TRACE_INFO_RL(QDF_MODULE_ID_HDD, params)
#define hdd_debug_rl(params...) QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_HDD, params)

#define hdd_enter() QDF_TRACE_ENTER(QDF_MODULE_ID_HDD, "enter")
#define hdd_enter_dev(dev) \
	QDF_TRACE_ENTER(QDF_MODULE_ID_HDD, "enter(%s)", (dev)->name)
#define hdd_exit() QDF_TRACE_EXIT(QDF_MODULE_ID_HDD, "exit")

#define WLAN_HDD_GET_PRIV_PTR(__dev__) \
		(struct hdd_adapter *)(netdev_priv((__dev__)))

#define MAX_NO_OF_2_4_CHANNELS 14

#define WLAN_HDD_PUBLIC_ACTION_FRAME_OFFSET 24

#define WLAN_HDD_IS_SOCIAL_CHANNEL(center_freq)	\
	(((center_freq) == 2412) || ((center_freq) == 2437) || \
	((center_freq) == 2462))

#define WLAN_HDD_QOS_ACTION_FRAME 1
#define WLAN_HDD_QOS_MAP_CONFIGURE 4
#define HDD_SAP_WAKE_LOCK_DURATION WAKELOCK_DURATION_RECOMMENDED

/* SAP client disconnect wake lock duration in milli seconds */
#define HDD_SAP_CLIENT_DISCONNECT_WAKE_LOCK_DURATION \
	WAKELOCK_DURATION_RECOMMENDED

#define HDD_CFG_REQUEST_FIRMWARE_RETRIES (3)
#define HDD_CFG_REQUEST_FIRMWARE_DELAY (20)

#define MAX_USER_COMMAND_SIZE 4096
#define DNS_DOMAIN_NAME_MAX_LEN 255
#define ICMPv6_ADDR_LEN 16


#define HDD_MIN_TX_POWER (-100) /* minimum tx power */
#define HDD_MAX_TX_POWER (+100) /* maximum tx power */

/* If IPA UC data path is enabled, target should reserve extra tx descriptors
 * for IPA data path.
 * Then host data path should allow less TX packet pumping in case
 * IPA data path enabled
 */
#define WLAN_TFC_IPAUC_TX_DESC_RESERVE   100

/*
 * NET_NAME_UNKNOWN is only introduced after Kernel 3.17, to have a macro
 * here if the Kernel version is less than 3.17 to avoid the interleave
 * conditional compilation.
 */
#if !((LINUX_VERSION_CODE >= KERNEL_VERSION(3, 17, 0)) ||\
	defined(WITH_BACKPORTS))
#define NET_NAME_UNKNOWN	0
#endif

#define PRE_CAC_SSID "pre_cac_ssid"

#define SCAN_REJECT_THRESHOLD_TIME 300000 /* Time is in msec, equal to 5 mins */
#define SCAN_REJECT_THRESHOLD 15

/* Default Psoc id */
#define DEFAULT_PSOC_ID 1

/* wait time for nud stats in milliseconds */
#define WLAN_WAIT_TIME_NUD_STATS 800
/* nud stats skb max length */
#define WLAN_NUD_STATS_LEN 800
/* ARP packet type for NUD debug stats */
#define WLAN_NUD_STATS_ARP_PKT_TYPE 1
/* Assigned size of driver memory dump is 4096 bytes */
#define DRIVER_MEM_DUMP_SIZE    4096

/* MAX OS Q block time value in msec
 * Prevent from permanent stall, resume OS Q if timer expired
 */
#define WLAN_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME 1000
#define WLAN_SAP_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME 100
#define WLAN_HDD_TX_FLOW_CONTROL_MAX_24BAND_CH   14

#ifndef NUM_TX_RX_HISTOGRAM
#define NUM_TX_RX_HISTOGRAM 128
#endif

#define NUM_TX_RX_HISTOGRAM_MASK (NUM_TX_RX_HISTOGRAM - 1)

#define HDD_NOISE_FLOOR_DBM (-96)

/**
 * enum hdd_auth_key_mgmt - auth key mgmt protocols
 * @HDD_AUTH_KEY_MGMT_802_1X: 802.1x
 * @HDD_AUTH_KEY_MGMT_PSK: PSK
 * @HDD_AUTH_KEY_MGMT_CCKM: CCKM
 */
enum hdd_auth_key_mgmt {
	HDD_AUTH_KEY_MGMT_802_1X = BIT(0),
	HDD_AUTH_KEY_MGMT_PSK = BIT(1),
	HDD_AUTH_KEY_MGMT_CCKM = BIT(2)
};

/**
 * wlan_net_dev_ref_dbgid - Debug IDs to detect net device reference leaks
 */
/*
 * New value added to the enum must also be reflected in function
 *  net_dev_ref_debug_string_from_id()
 */
typedef enum {
	NET_DEV_HOLD_ID_RESERVED = 0,
	NET_DEV_HOLD_GET_STA_CONNECTION_IN_PROGRESS = 1,
	NET_DEV_HOLD_CHECK_DFS_CHANNEL_FOR_ADAPTER = 2,
	NET_DEV_HOLD_GET_SAP_OPERATING_BAND = 3,
	NET_DEV_HOLD_RECOVERY_NOTIFIER_CALL = 4,
	NET_DEV_HOLD_IS_ANY_STA_CONNECTING = 5,
	NET_DEV_HOLD_SAP_DESTROY_CTX_ALL = 6,
	NET_DEV_HOLD_DRV_CMD_MAX_TX_POWER = 7,
	NET_DEV_HOLD_IPA_SET_TX_FLOW_INFO = 8,
	NET_DEV_HOLD_SET_RPS_CPU_MASK = 9,
	NET_DEV_HOLD_DFS_INDICATE_RADAR = 10,
	NET_DEV_HOLD_MAX_STA_INTERFACE_UP_COUNT_REACHED = 11,
	NET_DEV_HOLD_IS_CHAN_SWITCH_IN_PROGRESS = 12,
	NET_DEV_HOLD_STA_DESTROY_CTX_ALL = 13,
	NET_DEV_HOLD_CHECK_FOR_EXISTING_MACADDR = 14,
	NET_DEV_HOLD_DEINIT_ALL_ADAPTERS = 15,
	NET_DEV_HOLD_STOP_ALL_ADAPTERS = 16,
	NET_DEV_HOLD_RESET_ALL_ADAPTERS = 17,
	NET_DEV_HOLD_IS_ANY_INTERFACE_OPEN = 18,
	NET_DEV_HOLD_START_ALL_ADAPTERS = 19,
	NET_DEV_HOLD_GET_ADAPTER_BY_RAND_MACADDR = 20,
	NET_DEV_HOLD_GET_ADAPTER_BY_MACADDR = 21,
	NET_DEV_HOLD_GET_ADAPTER_BY_VDEV = 22,
	NET_DEV_HOLD_ADAPTER_GET_BY_REFERENCE = 23,
	NET_DEV_HOLD_GET_ADAPTER_BY_IFACE_NAME = 24,
	NET_DEV_HOLD_GET_ADAPTER = 25,
	NET_DEV_HOLD_GET_OPERATING_CHAN_FREQ = 26,
	NET_DEV_HOLD_UNREGISTER_WEXT_ALL_ADAPTERS = 27,
	NET_DEV_HOLD_ABORT_MAC_SCAN_ALL_ADAPTERS = 28,
	NET_DEV_HOLD_ABORT_SCHED_SCAN_ALL_ADAPTERS = 29,
	NET_DEV_HOLD_GET_FIRST_VALID_ADAPTER = 30,
	NET_DEV_HOLD_CLEAR_RPS_CPU_MASK = 31,
	NET_DEV_HOLD_BUS_BW_WORK_HANDLER = 32,
	NET_DEV_HOLD_DISPLAY_NETIF_QUEUE_HISTORY_COMPACT = 33,
	NET_DEV_HOLD_DISPLAY_NETIF_QUEUE_HISTORY = 34,
	NET_DEV_HOLD_CLEAR_NETIF_QUEUE_HISTORY = 35,
	NET_DEV_HOLD_UNSAFE_CHANNEL_RESTART_SAP = 36,
	NET_DEV_HOLD_INDICATE_MGMT_FRAME = 37,
	NET_DEV_HOLD_STATE_INFO_DUMP = 38,
	NET_DEV_HOLD_DISABLE_ROAMING = 39,
	NET_DEV_HOLD_ENABLE_ROAMING = 40,
	NET_DEV_HOLD_AUTO_SHUTDOWN_ENABLE = 41,
	NET_DEV_HOLD_GET_CON_SAP_ADAPTER = 42,
	NET_DEV_HOLD_IS_ANY_ADAPTER_CONNECTED = 43,
	NET_DEV_HOLD_IS_ROAMING_IN_PROGRESS = 44,
	NET_DEV_HOLD_DEL_P2P_INTERFACE = 45,
	NET_DEV_HOLD_IS_NDP_ALLOWED = 46,
	NET_DEV_HOLD_NDI_OPEN = 47,
	NET_DEV_HOLD_SEND_OEM_REG_RSP_NLINK_MSG = 48,
	NET_DEV_HOLD_PERIODIC_STA_STATS_DISPLAY = 49,
	NET_DEV_HOLD_SUSPEND_WLAN = 50,
	NET_DEV_HOLD_RESUME_WLAN = 51,
	NET_DEV_HOLD_SSR_RESTART_SAP = 52,
	NET_DEV_HOLD_SEND_DEFAULT_SCAN_IES = 53,
	NET_DEV_HOLD_CFG80211_SUSPEND_WLAN = 54,
	NET_DEV_HOLD_COUNTRY_CHANGE_UPDATE_STA = 55,
	NET_DEV_HOLD_COUNTRY_CHANGE_UPDATE_SAP = 56,
	NET_DEV_HOLD_CACHE_STATION_STATS_CB = 57,
	NET_DEV_HOLD_DISPLAY_TXRX_STATS = 58,
	NET_DEV_HOLD_GET_MODE_SPECIFIC_IF_COUNT = 59,
	NET_DEV_HOLD_START_PRE_CAC_TRANS = 60,
	NET_DEV_HOLD_IS_ANY_STA_CONNECTED = 61,

	/* Keep it at the end */
	NET_DEV_HOLD_ID_MAX
} wlan_net_dev_ref_dbgid;

/**
 * struct hdd_tx_rx_histogram - structure to keep track of tx and rx packets
 *				received over 100ms intervals
 * @interval_rx:	# of rx packets received in the last 100ms interval
 * @interval_tx:	# of tx packets received in the last 100ms interval
 * @next_vote_level:	pld_bus_width_type voting level (high or low)
 *			determined on the basis of total tx and rx packets
 *			received in the last 100ms interval
 * @next_rx_level:	pld_bus_width_type voting level (high or low)
 *			determined on the basis of rx packets received in the
 *			last 100ms interval
 * @next_tx_level:	pld_bus_width_type voting level (high or low)
 *			determined on the basis of tx packets received in the
 *			last 100ms interval
 * @is_rx_pm_qos_high	Capture rx_pm_qos voting
 * @is_tx_pm_qos_high	Capture tx_pm_qos voting
 * @qtime		timestamp when the record is added
 *
 * The structure keeps track of throughput requirements of wlan driver.
 * An entry is added if either of next_vote_level, next_rx_level or
 * next_tx_level changes. An entry is not added for every 100ms interval.
 */
struct hdd_tx_rx_histogram {
	uint64_t interval_rx;
	uint64_t interval_tx;
	uint32_t next_vote_level;
	uint32_t next_rx_level;
	uint32_t next_tx_level;
	bool is_rx_pm_qos_high;
	bool is_tx_pm_qos_high;
	uint64_t qtime;
};

struct hdd_tx_rx_stats {
	/* start_xmit stats */
	__u32    tx_called;
	__u32    tx_dropped;
	__u32    tx_orphaned;
	__u32    tx_classified_ac[NUM_TX_QUEUES];
	__u32    tx_dropped_ac[NUM_TX_QUEUES];

	/* rx stats */
	__u32 rx_packets[NUM_CPUS];
	__u32 rx_dropped[NUM_CPUS];
	__u32 rx_delivered[NUM_CPUS];
	__u32 rx_refused[NUM_CPUS];
	qdf_atomic_t rx_usolict_arp_n_mcast_drp;

	/* rx gro */
	__u32 rx_aggregated;
	__u32 rx_gro_dropped;
	__u32 rx_non_aggregated;
	__u32 rx_gro_flush_skip;
	__u32 rx_gro_low_tput_flush;

	/* txflow stats */
	bool     is_txflow_paused;
	__u32    txflow_pause_cnt;
	__u32    txflow_unpause_cnt;
	__u32    txflow_timer_cnt;

	/*tx timeout stats*/
	__u32 tx_timeout_cnt;
	__u32 cont_txtimeout_cnt;
	u64 jiffies_last_txtimeout;
};

#ifdef WLAN_FEATURE_11W
/**
 * struct hdd_pmf_stats - Protected Management Frame statistics
 * @num_unprot_deauth_rx: Number of unprotected deauth frames received
 * @num_unprot_disassoc_rx: Number of unprotected disassoc frames received
 */
struct hdd_pmf_stats {
	uint8_t num_unprot_deauth_rx;
	uint8_t num_unprot_disassoc_rx;
};
#endif

/**
 * struct hdd_arp_stats_s - arp debug stats count
 * @tx_arp_req_count: no. of arp req received from network stack
 * @rx_arp_rsp_count: no. of arp res received from FW
 * @tx_dropped: no. of arp req dropped at hdd layer
 * @rx_dropped: no. of arp res dropped
 * @rx_delivered: no. of arp res delivered to network stack
 * @rx_refused: no of arp rsp refused (not delivered) to network stack
 * @tx_host_fw_sent: no of arp req sent by FW OTA
 * @rx_host_drop_reorder: no of arp res dropped by host
 * @rx_fw_cnt: no of arp res received by FW
 * @tx_ack_cnt: no of arp req acked by FW
 */
struct hdd_arp_stats_s {
	uint16_t tx_arp_req_count;
	uint16_t rx_arp_rsp_count;
	uint16_t tx_dropped;
	uint16_t rx_dropped;
	uint16_t rx_delivered;
	uint16_t rx_refused;
	uint16_t tx_host_fw_sent;
	uint16_t rx_host_drop_reorder;
	uint16_t rx_fw_cnt;
	uint16_t tx_ack_cnt;
};

/**
 * struct hdd_dns_stats_s - dns debug stats count
 * @tx_dns_req_count: no. of dns query received from network stack
 * @rx_dns_rsp_count: no. of dns res received from FW
 * @tx_dropped: no. of dns query dropped at hdd layer
 * @rx_delivered: no. of dns res delivered to network stack
 * @rx_refused: no of dns res refused (not delivered) to network stack
 * @tx_host_fw_sent: no of dns query sent by FW OTA
 * @rx_host_drop: no of dns res dropped by host
 * @tx_ack_cnt: no of dns req acked by FW
 */
struct hdd_dns_stats_s {
	uint16_t tx_dns_req_count;
	uint16_t rx_dns_rsp_count;
	uint16_t tx_dropped;
	uint16_t rx_delivered;
	uint16_t rx_refused;
	uint16_t tx_host_fw_sent;
	uint16_t rx_host_drop;
	uint16_t tx_ack_cnt;
};

/**
 * struct hdd_tcp_stats_s - tcp debug stats count
 * @tx_tcp_syn_count: no. of tcp syn received from network stack
 * @@tx_tcp_ack_count: no. of tcp ack received from network stack
 * @rx_tcp_syn_ack_count: no. of tcp syn ack received from FW
 * @tx_tcp_syn_dropped: no. of tcp syn dropped at hdd layer
 * @tx_tcp_ack_dropped: no. of tcp ack dropped at hdd layer
 * @rx_delivered: no. of tcp syn ack delivered to network stack
 * @rx_refused: no of tcp syn ack refused (not delivered) to network stack
 * @tx_tcp_syn_host_fw_sent: no of tcp syn sent by FW OTA
 * @@tx_tcp_ack_host_fw_sent: no of tcp ack sent by FW OTA
 * @rx_host_drop: no of tcp syn ack dropped by host
 * @tx_tcp_syn_ack_cnt: no of tcp syn acked by FW
 * @tx_tcp_syn_ack_cnt: no of tcp ack acked by FW
 * @is_tcp_syn_ack_rcv: flag to check tcp syn ack received or not
 * @is_tcp_ack_sent: flag to check tcp ack sent or not
 */
struct hdd_tcp_stats_s {
	uint16_t tx_tcp_syn_count;
	uint16_t tx_tcp_ack_count;
	uint16_t rx_tcp_syn_ack_count;
	uint16_t tx_tcp_syn_dropped;
	uint16_t tx_tcp_ack_dropped;
	uint16_t rx_delivered;
	uint16_t rx_refused;
	uint16_t tx_tcp_syn_host_fw_sent;
	uint16_t tx_tcp_ack_host_fw_sent;
	uint16_t rx_host_drop;
	uint16_t rx_fw_cnt;
	uint16_t tx_tcp_syn_ack_cnt;
	uint16_t tx_tcp_ack_ack_cnt;
	bool is_tcp_syn_ack_rcv;
	bool is_tcp_ack_sent;

};

/**
 * struct hdd_icmpv4_stats_s - icmpv4 debug stats count
 * @tx_icmpv4_req_count: no. of icmpv4 req received from network stack
 * @rx_icmpv4_rsp_count: no. of icmpv4 res received from FW
 * @tx_dropped: no. of icmpv4 req dropped at hdd layer
 * @rx_delivered: no. of icmpv4 res delivered to network stack
 * @rx_refused: no of icmpv4 res refused (not delivered) to network stack
 * @tx_host_fw_sent: no of icmpv4 req sent by FW OTA
 * @rx_host_drop: no of icmpv4 res dropped by host
 * @rx_fw_cnt: no of icmpv4 res received by FW
 * @tx_ack_cnt: no of icmpv4 req acked by FW
 */
struct hdd_icmpv4_stats_s {
	uint16_t tx_icmpv4_req_count;
	uint16_t rx_icmpv4_rsp_count;
	uint16_t tx_dropped;
	uint16_t rx_delivered;
	uint16_t rx_refused;
	uint16_t tx_host_fw_sent;
	uint16_t rx_host_drop;
	uint16_t rx_fw_cnt;
	uint16_t tx_ack_cnt;
};

/**
 * struct hdd_peer_stats - Peer stats at HDD level
 * @rx_count: RX count
 * @rx_bytes: RX bytes
 * @fcs_count: FCS err count
 */
struct hdd_peer_stats {
	uint32_t rx_count;
	uint64_t rx_bytes;
	uint32_t fcs_count;
};

#define MAX_SUBTYPES_TRACKED	4

/**
 * struct hdd_eapol_stats_s - eapol debug stats count
 * @eapol_m1_count: eapol m1 count
 * @eapol_m2_count: eapol m2 count
 * @eapol_m3_count: eapol m3 count
 * @eapol_m4_count: eapol m4 count
 * @tx_dropped: no of tx frames dropped by host
 * @tx_noack_cnt: no of frames for which there is no ack
 * @rx_delivered: no. of frames delivered to network stack
 * @rx_refused: no of frames not delivered to network stack
 */
struct hdd_eapol_stats_s {
	uint16_t eapol_m1_count;
	uint16_t eapol_m2_count;
	uint16_t eapol_m3_count;
	uint16_t eapol_m4_count;
	uint16_t tx_dropped[MAX_SUBTYPES_TRACKED];
	uint16_t tx_noack_cnt[MAX_SUBTYPES_TRACKED];
	uint16_t rx_delivered[MAX_SUBTYPES_TRACKED];
	uint16_t rx_refused[MAX_SUBTYPES_TRACKED];
};

/**
 * struct hdd_dhcp_stats_s - dhcp debug stats count
 * @dhcp_dis_count: dhcp discovery count
 * @dhcp_off_count: dhcp offer count
 * @dhcp_req_count: dhcp request count
 * @dhcp_ack_count: dhcp ack count
 * @tx_dropped: no of tx frames dropped by host
 * @tx_noack_cnt: no of frames for which there is no ack
 * @rx_delivered: no. of frames delivered to network stack
 * @rx_refused: no of frames not delivered to network stack
 */
struct hdd_dhcp_stats_s {
	uint16_t dhcp_dis_count;
	uint16_t dhcp_off_count;
	uint16_t dhcp_req_count;
	uint16_t dhcp_ack_count;
	uint16_t tx_dropped[MAX_SUBTYPES_TRACKED];
	uint16_t tx_noack_cnt[MAX_SUBTYPES_TRACKED];
	uint16_t rx_delivered[MAX_SUBTYPES_TRACKED];
	uint16_t rx_refused[MAX_SUBTYPES_TRACKED];
};

struct hdd_stats {
	tCsrSummaryStatsInfo summary_stat;
	tCsrGlobalClassAStatsInfo class_a_stat;
	tCsrGlobalClassDStatsInfo class_d_stat;
	struct csr_per_chain_rssi_stats_info  per_chain_rssi_stats;
	struct hdd_tx_rx_stats tx_rx_stats;
	struct hdd_arp_stats_s hdd_arp_stats;
	struct hdd_dns_stats_s hdd_dns_stats;
	struct hdd_tcp_stats_s hdd_tcp_stats;
	struct hdd_icmpv4_stats_s hdd_icmpv4_stats;
	struct hdd_peer_stats peer_stats;
#ifdef WLAN_FEATURE_11W
	struct hdd_pmf_stats hdd_pmf_stats;
#endif
	struct hdd_eapol_stats_s hdd_eapol_stats;
	struct hdd_dhcp_stats_s hdd_dhcp_stats;
	struct pmf_bcn_protect_stats bcn_protect_stats;

#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
	uint32_t sta_stats_cached_timestamp;
	bool is_ll_stats_req_in_progress;
#endif
};

/**
 * struct hdd_roaming_info - HDD Internal Roaming Information
 * @bssid: BSSID to which we are connected
 * @peer_mac: Peer MAC address for IBSS connection
 * @roam_id: Unique identifier for a roaming instance
 * @roam_status: Current roam command status
 * @defer_key_complete: Should key complete be deferred?
 *
 */
struct hdd_roaming_info {
	tSirMacAddr bssid;
	tSirMacAddr peer_mac;
	uint32_t roam_id;
	eRoamCmdStatus roam_status;
	bool defer_key_complete;

};

#ifdef FEATURE_WLAN_WAPI
/* Define WAPI macros for Length, BKID count etc*/
#define MAX_NUM_AKM_SUITES    16

/** WAPI AUTH mode definition */
enum wapi_auth_mode {
	WAPI_AUTH_MODE_OPEN = 0,
	WAPI_AUTH_MODE_PSK = 1,
	WAPI_AUTH_MODE_CERT
} __packed;

#define WPA_GET_LE16(a) ((u16) (((a)[1] << 8) | (a)[0]))
#define WPA_GET_BE24(a) ((u32) ((a[0] << 16) | (a[1] << 8) | a[2]))
#define WAPI_PSK_AKM_SUITE  0x02721400
#define WAPI_CERT_AKM_SUITE 0x01721400

/**
 * struct hdd_wapi_info - WAPI Information structure definition
 * @wapi_mode: Is WAPI enabled on this adapter?
 * @is_wapi_sta: Is the STA associated with WAPI?
 * @wapi_auth_mode: WAPI authentication mode used by this adapter
 */
struct hdd_wapi_info {
	bool wapi_mode;
	bool is_wapi_sta;
	enum wapi_auth_mode wapi_auth_mode;
};
#endif /* FEATURE_WLAN_WAPI */

struct hdd_beacon_data {
	u8 *head;
	u8 *tail;
	u8 *proberesp_ies;
	u8 *assocresp_ies;
	int head_len;
	int tail_len;
	int proberesp_ies_len;
	int assocresp_ies_len;
	int dtim_period;
};

/**
 * struct hdd_mon_set_ch_info - Holds monitor mode channel switch params
 * @freq: Channel frequency.
 * @cb_mode: Channel bonding
 * @channel_width: Channel width 0/1/2 for 20/40/80MHz respectively.
 * @phy_mode: PHY mode
 */
struct hdd_mon_set_ch_info {
	uint32_t freq;
	uint8_t cb_mode;
	uint32_t channel_width;
	eCsrPhyMode phy_mode;
};

/**
 * struct hdd_station_ctx -- STA-specific information
 * @roam_profile: current roaming profile
 * @security_ie: WPA or RSN IE used by the @roam_profile
 * @assoc_additional_ie: association additional IE used by the @roam_profile
 * @wpa_versions: bitmap of supported WPA versions
 * @auth_key_mgmt: bitmap of supported auth key mgmt protocols
 * @requested_bssid: Specific BSSID to which to connect
 * @conn_info: current connection information
 * @roam_info: current roaming information
 * @ft_carrier_on: is carrier on
 * @hdd_reassoc_scenario: is station in the middle of reassociation?
 * @sta_debug_state: STA context debug variable
 * @broadcast_sta_id: STA ID assigned for broadcast frames
 * @ch_info: monitor mode channel information
 * @ap_supports_immediate_power_save: Does the current AP allow our STA
 *    to immediately go into power save?
 */
struct hdd_station_ctx {
	struct csr_roam_profile roam_profile;
	uint8_t security_ie[WLAN_MAX_IE_LEN];
	tSirAddie assoc_additional_ie;
	enum nl80211_wpa_versions wpa_versions;
	enum hdd_auth_key_mgmt auth_key_mgmt;
	struct qdf_mac_addr requested_bssid;
	struct hdd_connection_info conn_info;
	struct hdd_connection_info cache_conn_info;
	bool ft_carrier_on;
	bool hdd_reassoc_scenario;
	int sta_debug_state;
	struct hdd_mon_set_ch_info ch_info;
	bool ap_supports_immediate_power_save;
};

/**
 * enum bss_state - current state of the BSS
 * @BSS_STOP: BSS is stopped
 * @BSS_START: BSS is started
 */
enum bss_state {
	BSS_STOP,
	BSS_START,
};

/**
 * struct hdd_hostapd_state - hostapd-related state information
 * @bss_state: Current state of the BSS
 * @qdf_event: Event to synchronize actions between hostapd thread and
 *    internal callback threads
 * @qdf_stop_bss_event: Event to synchronize Stop BSS. When Stop BSS
 *    is issued userspace thread can wait on this event. The event will
 *    be set when the Stop BSS processing in UMAC has completed.
 * @qdf_sta_disassoc_event: Event to synchronize STA Disassociation.
 *    When a STA is disassociated userspace thread can wait on this
 *    event. The event will be set when the STA Disassociation
 *    processing in UMAC has completed.
 * @qdf_sta_eap_frm_done_event: Event to synchronize P2P GO disassoc
 *    frame and EAP frame.
 * @qdf_status: Used to communicate state from other threads to the
 *    userspace thread.
 */
struct hdd_hostapd_state {
	enum bss_state bss_state;
	qdf_event_t qdf_event;
	qdf_event_t qdf_stop_bss_event;
	qdf_event_t qdf_sta_disassoc_event;
	qdf_event_t qdf_sta_eap_frm_done_event;
	QDF_STATUS qdf_status;
};

/**
 * enum bss_stop_reason - reasons why a BSS is stopped.
 * @BSS_STOP_REASON_INVALID: no reason specified explicitly.
 * @BSS_STOP_DUE_TO_MCC_SCC_SWITCH: BSS stopped due to host
 *  driver is trying to switch AP role to a different channel
 *  to maintain SCC mode with the STA role on the same card.
 *  this usually happens when STA is connected to an external
 *  AP that runs on a different channel
 * @BSS_STOP_DUE_TO_VENDOR_CONFIG_CHAN: BSS stopped due to
 *  vendor subcmd set sap config channel
 */
enum bss_stop_reason {
	BSS_STOP_REASON_INVALID = 0,
	BSS_STOP_DUE_TO_MCC_SCC_SWITCH = 1,
	BSS_STOP_DUE_TO_VENDOR_CONFIG_CHAN = 2,
};

/**
 * struct hdd_rate_info - rate_info in HDD
 * @rate: tx/rx rate (kbps)
 * @mode: 0->11abg legacy, 1->HT, 2->VHT (refer to sir_sme_phy_mode)
 * @nss: number of streams
 * @mcs: mcs index for HT/VHT mode
 * @rate_flags: rate flags for last tx/rx
 *
 * rate info in HDD
 */
struct hdd_rate_info {
	uint32_t rate;
	uint8_t mode;
	uint8_t nss;
	uint8_t mcs;
	enum tx_rate_info rate_flags;
};

/**
 * struct hdd_mic_info - mic error info in HDD
 * @ta_mac_addr: transmitter mac address
 * @multicast: Flag for multicast
 * @key_id: Key ID
 * @tsc: Sequence number
 * @vdev_id: vdev id
 *
 */
struct hdd_mic_error_info {
	struct qdf_mac_addr ta_mac_addr;
	bool multicast;
	uint8_t key_id;
	uint8_t tsc[SIR_CIPHER_SEQ_CTR_SIZE];
	uint16_t vdev_id;
};

enum hdd_mic_work_status {
	MIC_UNINITIALIZED,
	MIC_INITIALIZED,
	MIC_SCHEDULED,
	MIC_DISABLED
};

enum hdd_work_status {
	HDD_WORK_UNINITIALIZED,
	HDD_WORK_INITIALIZED,
};

/**
 * struct hdd_mic_work - mic work info in HDD
 * @mic_error_work: mic error work
 * @status: sattus of mic error work
 * @info: Pointer to mic error information
 * @lock: lock to synchronixe mic error work
 *
 */
struct hdd_mic_work {
	qdf_work_t work;
	enum hdd_mic_work_status status;
	struct hdd_mic_error_info *info;
	qdf_spinlock_t lock;
};

/**
 * struct hdd_fw_txrx_stats - fw txrx status in HDD
 *                            (refer to station_info struct in Kernel)
 * @tx_packets: packets transmitted to this station
 * @tx_bytes: bytes transmitted to this station
 * @rx_packets: packets received from this station
 * @rx_bytes: bytes received from this station
 * @tx_retries: cumulative retry counts
 * @tx_failed: the number of failed frames
 * @tx_succeed: the number of succeed frames
 * @rssi: The signal strength (dbm)
 * @tx_rate: last used tx rate info
 * @rx_rate: last used rx rate info
 *
 * fw txrx status in HDD
 */
struct hdd_fw_txrx_stats {
	uint32_t tx_packets;
	uint64_t tx_bytes;
	uint32_t rx_packets;
	uint64_t rx_bytes;
	uint32_t tx_retries;
	uint32_t tx_failed;
	uint32_t tx_succeed;
	int8_t rssi;
	struct hdd_rate_info tx_rate;
	struct hdd_rate_info rx_rate;
};

/**
 * struct hdd_ap_ctx - SAP/P2PGO specific information
 * @hostapd_state: state control information
 * @dfs_cac_block_tx: Is data tramsmission blocked due to DFS CAC?
 * @ap_active: Are any stations active?
 * @disable_intrabss_fwd: Prevent forwarding between stations
 * @broadcast_sta_id: Station ID assigned after BSS starts
 * @privacy: The privacy bits of configuration
 * @encryption_type: The encryption being used
 * @group_key: Group Encryption Key
 * @wep_key: WEP key array
 * @wep_def_key_idx: WEP default key index
 * @sap_context: Pointer to context maintained by SAP (opaque to HDD)
 * @sap_config: SAP configuration
 * @operating_chan_freq: channel upon which the SAP is operating
 * @beacon: Beacon information
 * @vendor_acs_timer: Timer for ACS
 * @vendor_acs_timer_initialized: Is @vendor_acs_timer initialized?
 * @bss_stop_reason: Reason why the BSS was stopped
 * @acs_in_progress: In progress acs flag for an adapter
 */
struct hdd_ap_ctx {
	struct hdd_hostapd_state hostapd_state;
	bool dfs_cac_block_tx;
	bool ap_active;
	bool disable_intrabss_fwd;
	uint8_t broadcast_sta_id;
	uint8_t privacy;
	eCsrEncryptionType encryption_type;
	tCsrRoamSetKey group_key;
	tCsrRoamSetKey wep_key[CSR_MAX_NUM_KEY];
	uint8_t wep_def_key_idx;
	struct sap_context *sap_context;
	struct sap_config sap_config;
	uint32_t operating_chan_freq;
	struct hdd_beacon_data *beacon;
	qdf_mc_timer_t vendor_acs_timer;
	bool vendor_acs_timer_initialized;
	enum bss_stop_reason bss_stop_reason;
	qdf_atomic_t acs_in_progress;
};

/**
 * struct hdd_scan_info - Per-adapter scan information
 * @scan_add_ie: Additional IE for scan
 * @default_scan_ies: Default scan IEs
 * @default_scan_ies_len: Length of @default_scan_ies
 * @scan_mode: Scan mode
 */
struct hdd_scan_info {
	tSirAddie scan_add_ie;
	uint8_t *default_scan_ies;
	uint16_t default_scan_ies_len;
	tSirScanType scan_mode;
};

#define WLAN_HDD_MAX_MC_ADDR_LIST CFG_TGT_MAX_MULTICAST_FILTER_ENTRIES

struct hdd_multicast_addr_list {
	uint8_t mc_cnt;
	uint8_t addr[WLAN_HDD_MAX_MC_ADDR_LIST][ETH_ALEN];
};

#define WLAN_HDD_MAX_HISTORY_ENTRY 25

/**
 * struct hdd_netif_queue_stats - netif queue operation statistics
 * @pause_count - pause counter
 * @unpause_count - unpause counter
 */
struct hdd_netif_queue_stats {
	u32 pause_count;
	u32 unpause_count;
	qdf_time_t total_pause_time;
};

/**
 * struct hdd_netif_queue_history - netif queue operation history
 * @time: timestamp
 * @netif_action: action type
 * @netif_reason: reason type
 * @pause_map: pause map
 * @tx_q_state: state of the netdev TX queues
 */
struct hdd_netif_queue_history {
	qdf_time_t time;
	uint16_t netif_action;
	uint16_t netif_reason;
	uint32_t pause_map;
	unsigned long tx_q_state[NUM_TX_QUEUES];
};

/**
 * struct hdd_chan_change_params - channel related information
 * @chan_freq: operating channel frequency
 * @chan_params: channel parameters
 */
struct hdd_chan_change_params {
	uint32_t chan_freq;
	struct ch_params chan_params;
};

/**
 * struct hdd_runtime_pm_context - context to prevent/allow runtime pm
 * @dfs: dfs context to prevent/allow runtime pm
 * @connect: connect context to prevent/allow runtime pm
 * @user: user context to prevent/allow runtime pm
 * @is_user_wakelock_acquired: boolean to check if user wakelock status
 * @monitor_mode: monitor mode context to prevent/allow runtime pm
 *
 * Runtime PM control for underlying activities
 */
struct hdd_runtime_pm_context {
	qdf_runtime_lock_t dfs;
	qdf_runtime_lock_t connect;
	qdf_runtime_lock_t user;
	bool is_user_wakelock_acquired;
	qdf_runtime_lock_t monitor_mode;
};

/*
 * WLAN_HDD_ADAPTER_MAGIC is a magic number used to identify net devices
 * belonging to this driver from net devices belonging to other devices.
 * Therefore, the magic number must be unique relative to the numbers for
 * other drivers in the system. If WLAN_HDD_ADAPTER_MAGIC is already defined
 * (e.g. by compiler argument), then use that. If it's not already defined,
 * then use the first 4 characters of MULTI_IF_NAME to construct the magic
 * number. If MULTI_IF_NAME is not defined, then use a default magic number.
 */
#ifndef WLAN_HDD_ADAPTER_MAGIC
#ifdef MULTI_IF_NAME
#define WLAN_HDD_ADAPTER_MAGIC                                          \
	(MULTI_IF_NAME[0] == 0 ? 0x574c414e :                           \
	(MULTI_IF_NAME[1] == 0 ? (MULTI_IF_NAME[0] << 24) :             \
	(MULTI_IF_NAME[2] == 0 ? (MULTI_IF_NAME[0] << 24) |             \
		(MULTI_IF_NAME[1] << 16) :                              \
	(MULTI_IF_NAME[0] << 24) | (MULTI_IF_NAME[1] << 16) |           \
	(MULTI_IF_NAME[2] << 8) | MULTI_IF_NAME[3])))
#else
#define WLAN_HDD_ADAPTER_MAGIC 0x574c414e       /* ASCII "WLAN" */
#endif
#endif

/**
 * struct rcpi_info - rcpi info
 * @rcpi: computed value in dB
 * @mac_addr: peer mac addr for which rcpi is computed
 */
struct rcpi_info {
	int32_t rcpi;
	struct qdf_mac_addr mac_addr;
};

struct hdd_context;

/**
 * struct hdd_adapter - hdd vdev/net_device context
 * @vdev: object manager vdev context
 * @vdev_lock: lock to protect vdev context access
 * @vdev_id: Unique identifier assigned to the vdev
 * @event_flags: a bitmap of hdd_adapter_flags
 * @mic_work: mic work information
 * @gpio_tsf_sync_work: work to sync send TSF CAP WMI command
 * @cache_sta_count: number of currently cached stations
 * @acs_complete_event: acs complete event
 * @latency_level: 0 - normal, 1 - moderate, 2 - low, 3 - ultralow
 * @last_disconnect_reason: Last disconnected internal reason code
 *                          as per enum qca_disconnect_reason_codes
 * @connect_req_status: Last disconnected internal status code
 *                          as per enum qca_sta_connect_fail_reason_codes
 * @upgrade_udp_qos_threshold: The threshold for user priority upgrade for
			       any UDP packet.
 * @gro_disallowed: Flag to check if GRO is enabled or disable for adapter
 * @gro_flushed: Flag to indicate if GRO explicit flush is done or not
 * @handle_feature_update: Handle feature update only if it is triggered
 *			   by hdd_netdev_feature_update
 * @netdev_features_update_work: work for handling the netdev features update
 *				 for the adapter.
 * @netdev_features_update_work_status: status for netdev_features_update_work
 * @delete_in_progress: Flag to indicate that the adapter delete is in
 *			progress, and any operation using rtnl lock inside
 *			the driver can be avoided/skipped.
 * @mon_adapter: hdd_adapter of monitor mode.
 */
struct hdd_adapter {
	/* Magic cookie for adapter sanity verification.  Note that this
	 * needs to be at the beginning of the private data structure so
	 * that it will exist at the beginning of dev->priv and hence
	 * will always be in mapped memory
	 */
	uint32_t magic;

	/* list node for membership in the adapter list */
	qdf_list_node_t node;

	struct hdd_context *hdd_ctx;
	struct wlan_objmgr_vdev *vdev;
	qdf_spinlock_t vdev_lock;
	uint8_t vdev_id;

	/** Handle to the network device */
	struct net_device *dev;

	enum QDF_OPMODE device_mode;

	/** IPv4 notifier callback for handling ARP offload on change in IP */
	struct work_struct ipv4_notifier_work;
#ifdef WLAN_NS_OFFLOAD
	/** IPv6 notifier callback for handling NS offload on change in IP */
	struct work_struct ipv6_notifier_work;
#endif

	/* TODO Move this to sta Ctx */
	struct wireless_dev wdev;

	/** ops checks if Opportunistic Power Save is Enable or Not
	 * ctw stores CT Window value once we receive Opps command from
	 * wpa_supplicant then using CT Window value we need to Enable
	 * Opportunistic Power Save
	 */
	uint8_t ops;
	uint32_t ctw;

	/** Current MAC Address for the adapter  */
	struct qdf_mac_addr mac_addr;

#ifdef WLAN_NUD_TRACKING
	struct hdd_nud_tracking_info nud_tracking;
#endif

	struct hdd_mic_work mic_work;
	bool disconnection_in_progress;
	qdf_mutex_t disconnection_status_lock;
	unsigned long event_flags;

	/**Device TX/RX statistics*/
	struct net_device_stats stats;
	/** HDD statistics*/
	struct hdd_stats hdd_stats;

	/* estimated link speed */
	uint32_t estimated_linkspeed;

	/**
	 * vdev_destroy_event is moved from the qdf_event to linux event
	 * consciously, Lets take example when sap interface is waiting on the
	 * session_close event and then there is a SSR the wait event is
	 * completed the interface down is returned and the next command to the
	 * driver will  hdd_hostapd_uinit-->vhdd_deinit_ap_mode-->
	 * hdd_hostapd_deinit_sap_session where in the sap_ctx would be freed.
	 * During the SSR if the same sap context is used it would result
	 * in null pointer de-reference.
	 */
	struct completion vdev_destroy_event;

	/* QDF event for session open */
	qdf_event_t qdf_session_open_event;

#ifdef FEATURE_MONITOR_MODE_SUPPORT
	/* QDF event for monitor mode vdev up */
	qdf_event_t qdf_monitor_mode_vdev_up_event;
#endif

	/* TODO: move these to sta ctx. These may not be used in AP */
	/** completion variable for disconnect callback */
	struct completion disconnect_comp_var;

	struct completion roaming_comp_var;

	/* completion variable for Linkup Event */
	struct completion linkup_event_var;

	/* completion variable for off channel  remain on channel Event */
	struct completion offchannel_tx_event;
	/* Completion variable for action frame */
	struct completion tx_action_cnf_event;

	struct completion sta_authorized_event;

	/* Track whether the linkup handling is needed  */
	bool is_link_up_service_needed;

	/* WMM Status */
	struct hdd_wmm_status hdd_wmm_status;

	/** Multiple station supports */
	/** Per-station structure */

	/* TODO: Will be removed as a part of next phase of clean up */
	struct hdd_station_info sta_info[WLAN_MAX_STA_COUNT];
	struct hdd_station_info cache_sta_info[WLAN_MAX_STA_COUNT];

	/* TODO: _list from name will be removed after clean up */
	struct hdd_sta_info_obj sta_info_list;
	struct hdd_sta_info_obj cache_sta_info_list;
	qdf_atomic_t cache_sta_count;

#ifdef FEATURE_WLAN_WAPI
	struct hdd_wapi_info wapi_info;
#endif

	int8_t rssi;
	int32_t rssi_on_disconnect;
#ifdef WLAN_FEATURE_LPSS
	bool rssi_send;
#endif

	uint8_t snr;

	struct work_struct  sap_stop_bss_work;

	union {
		struct hdd_station_ctx station;
		struct hdd_ap_ctx ap;
	} session;

	qdf_atomic_t ch_switch_in_progress;
	qdf_event_t acs_complete_event;

#ifdef WLAN_FEATURE_TSF
	/* tsf value received from firmware */
	uint64_t cur_target_time;
	uint64_t cur_tsf_sync_soc_time;
	uint64_t last_tsf_sync_soc_time;
	uint64_t cur_target_global_tsf_time;
	uint64_t last_target_global_tsf_time;
	qdf_mc_timer_t host_capture_req_timer;
#ifdef WLAN_FEATURE_TSF_PLUS
	/* spin lock for read/write timestamps */
	qdf_spinlock_t host_target_sync_lock;
	qdf_mc_timer_t host_target_sync_timer;
	uint64_t cur_host_time;
	uint64_t last_host_time;
	uint64_t last_target_time;
	/* to store the count of continuous invalid tstamp-pair */
	int continuous_error_count;
	/* to store the count of continuous capture retry */
	int continuous_cap_retry_count;
	/* to indicate whether tsf_sync has been initialized */
	qdf_atomic_t tsf_sync_ready_flag;
#ifdef WLAN_FEATURE_TSF_PLUS_EXT_GPIO_SYNC
	qdf_work_t gpio_tsf_sync_work;
#endif
#endif /* WLAN_FEATURE_TSF_PLUS */
#endif

	struct hdd_multicast_addr_list mc_addr_list;
	uint8_t addr_filter_pattern;

	struct hdd_scan_info scan_info;

	/* Flag to ensure PSB is configured through framework */
	uint8_t psb_changed;
	/* UAPSD psb value configured through framework */
	uint8_t configured_psb;
	/* Use delayed work for Sec AP ACS as Pri AP Startup need to complete
	 * since CSR (PMAC Struct) Config is same for both AP
	 */
	struct delayed_work acs_pending_work;

	struct work_struct scan_block_work;
	qdf_list_t blocked_scan_request_q;
	qdf_mutex_t blocked_scan_request_q_lock;

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
	unsigned long prev_rx_packets;
	unsigned long prev_tx_packets;
	unsigned long prev_tx_bytes;
	uint64_t prev_fwd_tx_packets;
	uint64_t prev_fwd_rx_packets;
#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/

#ifdef WLAN_FEATURE_MSCS
	unsigned long mscs_prev_tx_vo_pkts;
	uint32_t mscs_counter;
#endif /* WLAN_FEATURE_MSCS */

#if  defined(QCA_LL_LEGACY_TX_FLOW_CONTROL) || \
				defined(QCA_HL_NETDEV_FLOW_CONTROL)
	qdf_mc_timer_t tx_flow_control_timer;
	bool tx_flow_timer_initialized;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL || QCA_HL_NETDEV_FLOW_CONTROL */
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
	unsigned int tx_flow_low_watermark;
	unsigned int tx_flow_hi_watermark_offset;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

	bool offloads_configured;

	/* DSCP to UP QoS Mapping */
	enum sme_qos_wmmuptype dscp_to_up_map[WLAN_MAX_DSCP + 1];

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
	bool is_link_layer_stats_set;
#endif
	uint8_t link_status;
	uint8_t upgrade_udp_qos_threshold;

	/* variable for temperature in Celsius */
	int temperature;

#ifdef WLAN_FEATURE_DSRC
	/* MAC addresses used for OCB interfaces */
	struct qdf_mac_addr ocb_mac_address[QDF_MAX_CONCURRENCY_PERSONA];
	int ocb_mac_addr_count;
#endif

	/* BITMAP indicating pause reason */
	uint32_t pause_map;
	uint32_t subqueue_pause_map;
	spinlock_t pause_map_lock;
	qdf_time_t start_time;
	qdf_time_t last_time;
	qdf_time_t total_pause_time;
	qdf_time_t total_unpause_time;
	uint8_t history_index;
	struct hdd_netif_queue_history
		 queue_oper_history[WLAN_HDD_MAX_HISTORY_ENTRY];
	struct hdd_netif_queue_stats queue_oper_stats[WLAN_REASON_TYPE_MAX];
	ol_txrx_tx_fp tx_fn;
	/* debugfs entry */
	struct dentry *debugfs_phy;
	/*
	 * The pre cac channel frequency is saved here and will be used when
         * the SAP's channel needs to be moved from the existing 2.4GHz channel.
	 */
	uint32_t pre_cac_freq;

	/*
	 * Indicate if HO fails during disconnect so that
	 * disconnect is not initiated by HDD as its already
	 * initiated by CSR
	 */
	bool roam_ho_fail;
	struct lfr_firmware_status lfr_fw_status;
	bool con_status;
	bool dad;
	uint8_t active_ac;
	uint32_t pkt_type_bitmap;
	uint32_t track_arp_ip;
	uint8_t dns_payload[256];
	uint32_t track_dns_domain_len;
	uint32_t track_src_port;
	uint32_t track_dest_port;
	uint32_t track_dest_ipv4;
	uint32_t mon_chan_freq;
	uint32_t mon_bandwidth;
	uint16_t latency_level;
#ifdef FEATURE_MONITOR_MODE_SUPPORT
	bool monitor_mode_vdev_up_in_progress;
#endif
	/* rcpi information */
	struct rcpi_info rcpi;
	bool send_mode_change;
#ifdef FEATURE_WLAN_APF
	struct hdd_apf_context apf_context;
#endif /* FEATURE_WLAN_APF */

#ifdef WLAN_DEBUGFS
	struct hdd_debugfs_file_info csr_file[HDD_DEBUGFS_FILE_ID_MAX];
#endif /* WLAN_DEBUGFS */

#ifdef WLAN_FEATURE_MOTION_DETECTION
	bool motion_detection_mode;
	bool motion_det_cfg;
	bool motion_det_in_progress;
	uint32_t motion_det_baseline_value;
#endif /* WLAN_FEATURE_MOTION_DETECTION */
	enum qca_disconnect_reason_codes last_disconnect_reason;
	enum wlan_status_code connect_req_status;

#ifdef WLAN_FEATURE_PERIODIC_STA_STATS
	/* Indicate whether to display sta periodic stats */
	bool is_sta_periodic_stats_enabled;
	uint16_t periodic_stats_timer_count;
	uint32_t periodic_stats_timer_counter;
	qdf_mutex_t sta_periodic_stats_lock;
#endif /* WLAN_FEATURE_PERIODIC_STA_STATS */
	qdf_event_t peer_cleanup_done;
#ifdef FEATURE_OEM_DATA
	bool oem_data_in_progress;
	void *cookie;
	bool response_expected;
#endif
	uint8_t gro_disallowed[DP_MAX_RX_THREADS];
	uint8_t gro_flushed[DP_MAX_RX_THREADS];
	bool handle_feature_update;
	bool runtime_disable_rx_thread;
	ol_txrx_rx_fp rx_stack;

	qdf_work_t netdev_features_update_work;
	enum hdd_work_status netdev_features_update_work_status;
	/* Flag to indicate whether it is a pre cac adapter or not */
	bool is_pre_cac_adapter;
#ifdef WLAN_FEATURE_BIG_DATA_STATS
	struct big_data_stats_event big_data_stats;
#endif
	bool delete_in_progress;
	qdf_atomic_t net_dev_hold_ref_count[NET_DEV_HOLD_ID_MAX];
#ifdef WLAN_FEATURE_PKT_CAPTURE
	struct hdd_adapter *mon_adapter;
#endif
};

#define WLAN_HDD_GET_STATION_CTX_PTR(adapter) (&(adapter)->session.station)
#define WLAN_HDD_GET_AP_CTX_PTR(adapter) (&(adapter)->session.ap)
#define WLAN_HDD_GET_CTX(adapter) ((adapter)->hdd_ctx)
#define WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter) \
				(&(adapter)->session.ap.hostapd_state)
#define WLAN_HDD_GET_SAP_CTX_PTR(adapter) ((adapter)->session.ap.sap_context)

#ifdef WLAN_FEATURE_NAN
#define WLAN_HDD_IS_NDP_ENABLED(hdd_ctx) ((hdd_ctx)->nan_datapath_enabled)
#else
/* WLAN_HDD_GET_NDP_CTX_PTR and WLAN_HDD_GET_NDP_WEXT_STATE_PTR are not defined
 * intentionally so that all references to these must be within NDP code.
 * non-NDP code can call WLAN_HDD_IS_NDP_ENABLED(), and when it is enabled,
 * invoke NDP code to do all work.
 */
#define WLAN_HDD_IS_NDP_ENABLED(hdd_ctx) (false)
#endif

/* Set mac address locally administered bit */
#define WLAN_HDD_RESET_LOCALLY_ADMINISTERED_BIT(macaddr) (macaddr[0] &= 0xFD)

#define HDD_DEFAULT_MCC_P2P_QUOTA    70
#define HDD_RESET_MCC_P2P_QUOTA      50

/*
 * struct hdd_priv_data - driver ioctl private data payload
 * @buf: pointer to command buffer (may be in userspace)
 * @used_len: length of the command/data currently in @buf
 * @total_len: total length of the @buf memory allocation
 */
struct hdd_priv_data {
	uint8_t *buf;
	int used_len;
	int total_len;
};

#define  MAX_MOD_LOGLEVEL 10
struct fw_log_info {
	uint8_t enable;
	uint8_t dl_type;
	uint8_t dl_report;
	uint8_t dl_loglevel;
	uint8_t index;
	uint32_t dl_mod_loglevel[MAX_MOD_LOGLEVEL];

};

/**
 * enum antenna_mode - number of TX/RX chains
 * @HDD_ANTENNA_MODE_INVALID: Invalid mode place holder
 * @HDD_ANTENNA_MODE_1X1: Number of TX/RX chains equals 1
 * @HDD_ANTENNA_MODE_2X2: Number of TX/RX chains equals 2
 * @HDD_ANTENNA_MODE_MAX: Place holder for max mode
 */
enum antenna_mode {
	HDD_ANTENNA_MODE_INVALID,
	HDD_ANTENNA_MODE_1X1,
	HDD_ANTENNA_MODE_2X2,
	HDD_ANTENNA_MODE_MAX
};

/**
 * enum smps_mode - SM power save mode
 * @HDD_SMPS_MODE_STATIC: Static power save
 * @HDD_SMPS_MODE_DYNAMIC: Dynamic power save
 * @HDD_SMPS_MODE_RESERVED: Reserved
 * @HDD_SMPS_MODE_DISABLED: Disable power save
 * @HDD_SMPS_MODE_MAX: Place holder for max mode
 */
enum smps_mode {
	HDD_SMPS_MODE_STATIC,
	HDD_SMPS_MODE_DYNAMIC,
	HDD_SMPS_MODE_RESERVED,
	HDD_SMPS_MODE_DISABLED,
	HDD_SMPS_MODE_MAX
};

#ifdef WLAN_FEATURE_OFFLOAD_PACKETS
/**
 * struct hdd_offloaded_packets - request id to pattern id mapping
 * @request_id: request id
 * @pattern_id: pattern id
 *
 */
struct hdd_offloaded_packets {
	uint32_t request_id;
	uint8_t  pattern_id;
};

/**
 * struct hdd_offloaded_packets_ctx - offloaded packets context
 * @op_table: request id to pattern id table
 * @op_lock: mutex lock
 */
struct hdd_offloaded_packets_ctx {
	struct hdd_offloaded_packets op_table[MAXNUM_PERIODIC_TX_PTRNS];
	struct mutex op_lock;
};
#endif

/**
 * enum driver_status: Driver Modules status
 * @DRIVER_MODULES_UNINITIALIZED: Driver CDS modules uninitialized
 * @DRIVER_MODULES_ENABLED: Driver CDS modules opened
 * @DRIVER_MODULES_CLOSED: Driver CDS modules closed
 */
enum driver_modules_status {
	DRIVER_MODULES_UNINITIALIZED,
	DRIVER_MODULES_ENABLED,
	DRIVER_MODULES_CLOSED
};

/**
 * struct acs_dfs_policy - Define ACS policies
 * @acs_dfs_mode: Dfs mode enabled/disabled.
 * @acs_chan_freq: pre defined channel frequency to avoid ACS.
 */
struct acs_dfs_policy {
	enum dfs_mode acs_dfs_mode;
	uint32_t acs_chan_freq;
};

/**
 * enum suspend_fail_reason: Reasons a WLAN suspend might fail
 * SUSPEND_FAIL_IPA: IPA in progress
 * SUSPEND_FAIL_RADAR: radar scan in progress
 * SUSPEND_FAIL_ROAM: roaming in progress
 * SUSPEND_FAIL_SCAN: scan in progress
 * SUSPEND_FAIL_INITIAL_WAKEUP: received initial wakeup from firmware
 * SUSPEND_FAIL_MAX_COUNT: the number of wakeup reasons, always at the end
 */
enum suspend_fail_reason {
	SUSPEND_FAIL_IPA,
	SUSPEND_FAIL_RADAR,
	SUSPEND_FAIL_ROAM,
	SUSPEND_FAIL_SCAN,
	SUSPEND_FAIL_INITIAL_WAKEUP,
	SUSPEND_FAIL_MAX_COUNT
};

/**
 * suspend_resume_stats - Collection of counters for suspend/resume events
 * @suspends: number of suspends completed
 * @resumes: number of resumes completed
 * @suspend_fail: counters for failed suspend reasons
 */
struct suspend_resume_stats {
	uint32_t suspends;
	uint32_t resumes;
	uint32_t suspend_fail[SUSPEND_FAIL_MAX_COUNT];
};

/**
 * hdd_sta_smps_param  - SMPS parameters to configure from hdd
 * HDD_STA_SMPS_PARAM_UPPER_RSSI_THRESH: RSSI threshold to enter Dynamic SMPS
 * mode from inactive mode
 * HDD_STA_SMPS_PARAM_STALL_RSSI_THRESH:  RSSI threshold to enter
 * Stalled-D-SMPS mode from D-SMPS mode or to enter D-SMPS mode from
 * Stalled-D-SMPS mode
 * HDD_STA_SMPS_PARAM_LOWER_RSSI_THRESH:  RSSI threshold to disable SMPS modes
 * HDD_STA_SMPS_PARAM_UPPER_BRSSI_THRESH: Upper threshold for beacon-RSSI.
 * Used to reduce RX chainmask.
 * HDD_STA_SMPS_PARAM_LOWER_BRSSI_THRESH:  Lower threshold for beacon-RSSI.
 * Used to increase RX chainmask.
 * HDD_STA_SMPS_PARAM_DTIM_1CHRX_ENABLE: Enable/Disable DTIM 1chRx feature
 */
enum hdd_sta_smps_param {
	HDD_STA_SMPS_PARAM_UPPER_RSSI_THRESH = 0,
	HDD_STA_SMPS_PARAM_STALL_RSSI_THRESH = 1,
	HDD_STA_SMPS_PARAM_LOWER_RSSI_THRESH = 2,
	HDD_STA_SMPS_PARAM_UPPER_BRSSI_THRESH = 3,
	HDD_STA_SMPS_PARAM_LOWER_BRSSI_THRESH = 4,
	HDD_STA_SMPS_PARAM_DTIM_1CHRX_ENABLE = 5
};

/**
 * enum RX_OFFLOAD - Receive offload modes
 * @CFG_LRO_ENABLED: Large Rx offload
 * @CFG_GRO_ENABLED: Generic Rx Offload
 */
enum RX_OFFLOAD {
	CFG_LRO_ENABLED = 1,
	CFG_GRO_ENABLED,
};

/* One per STA: 1 for BCMC_STA_ID, 1 for each SAP_SELF_STA_ID,
 * 1 for WDS_STAID
 */
#define HDD_MAX_ADAPTERS (WLAN_MAX_STA_COUNT + QDF_MAX_NO_OF_SAP_MODE + 2)

#ifdef DISABLE_CHANNEL_LIST

/**
 * struct hdd_cache_channel_info - Structure of the channel info
 * which needs to be cached
 * @channel_num: channel number
 * @reg_status: Current regulatory status of the channel
 * Enable
 * Disable
 * DFS
 * Invalid
 * @wiphy_status: Current wiphy status
 */
struct hdd_cache_channel_info {
	uint32_t channel_num;
	enum channel_state reg_status;
	uint32_t wiphy_status;
};

/**
 * struct hdd_cache_channels - Structure of the channels to be cached
 * @num_channels: Number of channels to be cached
 * @channel_info: Structure of the channel info
 */
struct hdd_cache_channels {
	uint32_t num_channels;
	struct hdd_cache_channel_info *channel_info;
};
#endif

/**
 * struct hdd_dynamic_mac - hdd structure to handle dynamic mac address changes
 * @dynamic_mac: Dynamicaly configured mac, this contains the mac on which
 * current interface is up
 * @is_provisioned_mac: is this mac from provisioned list
 * @bit_position: holds the bit mask position from where this mac is assigned,
 * if mac is assigned from provisioned this field contains the position from
 * provisioned_intf_addr_mask else contains the position from
 * derived_intf_addr_mask
 */
struct hdd_dynamic_mac {
	struct qdf_mac_addr dynamic_mac;
	bool is_provisioned_mac;
	uint8_t bit_position;
};

/**
 * hdd_fw_ver_info - FW version info structure
 * @major_spid: FW version - major spid.
 * @minor_spid: FW version - minor spid
 * @siid:       FW version - siid
 * @sub_id:     FW version - sub id
 * @rel_id:     FW version - release id
 * @crmid:      FW version - crmid
 */

struct hdd_fw_ver_info {
	uint32_t major_spid;
	uint32_t minor_spid;
	uint32_t siid;
	uint32_t sub_id;
	uint32_t rel_id;
	uint32_t crmid;
};

/**
 * The logic for get current index of history is dependent on this
 * value being power of 2.
 */
#define WLAN_HDD_ADAPTER_OPS_HISTORY_MAX 4
QDF_COMPILE_TIME_ASSERT(adapter_ops_history_size,
			(WLAN_HDD_ADAPTER_OPS_HISTORY_MAX &
			 (WLAN_HDD_ADAPTER_OPS_HISTORY_MAX - 1)) == 0);

/**
 * enum hdd_adapter_ops_event - events for adapter ops history
 * @WLAN_HDD_ADAPTER_OPS_WORK_POST: adapter ops work posted
 * @WLAN_HDD_ADAPTER_OPS_WORK_SCHED: adapter ops work scheduled
 */
enum hdd_adapter_ops_event {
	WLAN_HDD_ADAPTER_OPS_WORK_POST,
	WLAN_HDD_ADAPTER_OPS_WORK_SCHED,
};

/**
 * struct hdd_adapter_ops_record - record of adapter ops history
 * @timestamp: time of the occurrence of event
 * @event: event
 * @vdev_id: vdev id corresponding to the event
 */
struct hdd_adapter_ops_record {
	uint64_t timestamp;
	enum hdd_adapter_ops_event event;
	int vdev_id;
};

/**
 * struct hdd_adapter_ops_history - history of adapter ops
 * @index: index to store the next event
 * @entry: array of events
 */
struct hdd_adapter_ops_history {
	qdf_atomic_t index;
	struct hdd_adapter_ops_record entry[WLAN_HDD_ADAPTER_OPS_HISTORY_MAX];
};

/**
 * struct hdd_context - hdd shared driver and psoc/device context
 * @psoc: object manager psoc context
 * @pdev: object manager pdev context
 * @iftype_data_2g: Interface data for 2g band
 * @iftype_data_5g: Interface data for 5g band
 * @num_latency_critical_clients: Number of latency critical clients connected
 * @bus_bw_work: work for periodically computing DDR bus bandwidth requirements
 * @g_event_flags: a bitmap of hdd_driver_flags
 * @psoc_idle_timeout_work: delayed work for psoc idle shutdown
 * @dynamic_nss_chains_support: Per vdev dynamic nss chains update capability
 * @sar_cmd_params: SAR command params to be configured to the FW
 * @country_change_work: work for updating vdev when country changes
 * @rx_aggregation: rx aggregation enable or disable state
 * @gro_force_flush: gro force flushed indication flag
 * @current_pcie_gen_speed: current pcie gen speed
 * @pm_qos_req: pm_qos request for all cpu cores
 * @qos_cpu_mask: voted cpu core mask
 * @adapter_ops_wq: High priority workqueue for handling adapter operations
 * @multi_client_thermal_mitigation: Multi client thermal mitigation by fw
 * @disconnect_for_sta_mon_conc: disconnect if sta monitor intf concurrency
 * @is_dual_mac_cfg_updated: indicate whether dual mac cfg has been updated
 * @twt_en_dis_work: work to send twt enable/disable cmd on MCC/SCC concurrency
 * @dump_in_progress: Stores value of dump in progress
 */
struct hdd_context {
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_pdev *pdev;
	mac_handle_t mac_handle;
	struct wiphy *wiphy;
	qdf_spinlock_t hdd_adapter_lock;
	qdf_list_t hdd_adapters; /* List of adapters */

	/** Pointer for firmware image data */
	const struct firmware *fw;

	/** Pointer for configuration data */
	const struct firmware *cfg;

	/** Pointer to the parent device */
	struct device *parent_dev;

	/** Config values read from qcom_cfg.ini file */
	struct hdd_config *config;

	/* Pointer for wiphy 2G/5G band channels */
	struct ieee80211_channel *channels_2ghz;
	struct ieee80211_channel *channels_5ghz;

#if defined(WLAN_FEATURE_11AX) && \
	(defined(CFG80211_SBAND_IFTYPE_DATA_BACKPORT) || \
	 (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)))
	struct ieee80211_sband_iftype_data *iftype_data_2g;
	struct ieee80211_sband_iftype_data *iftype_data_5g;
#if defined(CONFIG_BAND_6GHZ) && (defined(CFG80211_6GHZ_BAND_SUPPORTED) || \
		(KERNEL_VERSION(5, 4, 0) <= LINUX_VERSION_CODE))
	struct ieee80211_sband_iftype_data *iftype_data_6g;
#endif
#endif
	/* Completion  variable to indicate Mc Thread Suspended */
	struct completion mc_sus_event_var;

	bool is_scheduler_suspended;

#ifdef QCA_CONFIG_SMP
	bool is_ol_rx_thread_suspended;
#endif

	bool hdd_wlan_suspended;
	bool suspended;
	/* flag to start pktlog after SSR/PDR if previously enabled */
	bool is_pktlog_enabled;

	/* Lock to avoid race condition during start/stop bss */
	struct mutex sap_lock;

#ifdef FEATURE_OEM_DATA_SUPPORT
	/* OEM App registered or not */
	bool oem_app_registered;

	/* OEM App Process ID */
	int32_t oem_pid;
#endif

	qdf_atomic_t num_latency_critical_clients;
	/** Concurrency Parameters*/
	uint32_t concurrency_mode;

	uint8_t no_of_open_sessions[QDF_MAX_NO_OF_MODE];
	uint8_t no_of_active_sessions[QDF_MAX_NO_OF_MODE];

	/** P2P Device MAC Address for the adapter  */
	struct qdf_mac_addr p2p_device_address;

	qdf_wake_lock_t rx_wake_lock;
	qdf_wake_lock_t sap_wake_lock;

	/* Flag keeps track of wiphy suspend/resume */
	bool is_wiphy_suspended;

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
	struct qdf_periodic_work bus_bw_work;
	int cur_vote_level;
	qdf_spinlock_t bus_bw_lock;
	int cur_rx_level;
	uint64_t prev_no_rx_offload_pkts;
	uint64_t prev_rx_offload_pkts;
	/* Count of non TSO packets in previous bus bw delta time */
	uint64_t prev_no_tx_offload_pkts;
	/* Count of TSO packets in previous bus bw delta time */
	uint64_t prev_tx_offload_pkts;
	int cur_tx_level;
	uint64_t prev_tx;
	qdf_atomic_t low_tput_gro_enable;
	uint32_t bus_low_vote_cnt;
#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/

	struct completion ready_to_suspend;
	/* defining the solution type */
	uint32_t target_type;

	/* defining the firmware version */
	uint32_t target_fw_version;
	uint32_t target_fw_vers_ext;
	struct hdd_fw_ver_info fw_version_info;

	/* defining the chip/rom version */
	uint32_t target_hw_version;
	/* defining the chip/rom revision */
	uint32_t target_hw_revision;
	/* chip/rom name */
	char *target_hw_name;
	struct regulatory reg;
#ifdef FEATURE_WLAN_CH_AVOID
	uint16_t unsafe_channel_count;
	uint16_t unsafe_channel_list[NUM_CHANNELS];
#endif /* FEATURE_WLAN_CH_AVOID */

	uint8_t max_intf_count;
	uint8_t current_intf_count;
#ifdef WLAN_FEATURE_LPSS
	uint8_t lpss_support;
#endif
	uint8_t ap_arpns_support;
	tSirScanType ioctl_scan_mode;

#ifdef FEATURE_WLAN_MCC_TO_SCC_SWITCH
	qdf_work_t sta_ap_intf_check_work;
#endif

	uint8_t dev_dfs_cac_status;

	bool bt_coex_mode_set;
#ifdef FEATURE_WLAN_AP_AP_ACS_OPTIMIZE
	qdf_mc_timer_t skip_acs_scan_timer;
	uint8_t skip_acs_scan_status;
	uint32_t *last_acs_freq_list;
	uint8_t num_of_channels;
	qdf_spinlock_t acs_skip_lock;
#endif

	qdf_wake_lock_t sap_dfs_wakelock;
	atomic_t sap_dfs_ref_cnt;

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
	bool is_extwow_app_type1_param_set;
	bool is_extwow_app_type2_param_set;
#endif

	/* Time since boot up to extscan start (in micro seconds) */
	uint64_t ext_scan_start_since_boot;
	unsigned long g_event_flags;
	uint8_t miracast_value;

#ifdef WLAN_NS_OFFLOAD
	/* IPv6 notifier callback for handling NS offload on change in IP */
	struct notifier_block ipv6_notifier;
#endif
	bool ns_offload_enable;
	/* IPv4 notifier callback for handling ARP offload on change in IP */
	struct notifier_block ipv4_notifier;

#ifdef FEATURE_RUNTIME_PM
	struct notifier_block pm_qos_notifier;
	bool runtime_pm_prevented;
	qdf_spinlock_t pm_qos_lock;
#endif
	/* number of rf chains supported by target */
	uint32_t  num_rf_chains;
	/* Is htTxSTBC supported by target */
	uint8_t   ht_tx_stbc_supported;
#ifdef WLAN_FEATURE_OFFLOAD_PACKETS
	struct hdd_offloaded_packets_ctx op_ctx;
#endif
	bool mcc_mode;
	struct mutex memdump_lock;
	uint16_t driver_dump_size;
	uint8_t *driver_dump_mem;

	bool connection_in_progress;
	qdf_spinlock_t connection_status_lock;

	uint16_t hdd_txrx_hist_idx;
	struct hdd_tx_rx_histogram *hdd_txrx_hist;

	/*
	 * place to store FTM capab of target. This allows changing of FTM capab
	 * at runtime and intersecting it with target capab before updating.
	 */
	uint32_t fine_time_meas_cap_target;
	uint32_t rx_high_ind_cnt;
	/* For Rx thread non GRO/LRO packet accounting */
	uint64_t no_rx_offload_pkt_cnt;
	uint64_t no_tx_offload_pkt_cnt;
	/* Current number of TX X RX chains being used */
	enum antenna_mode current_antenna_mode;

	/* the radio index assigned by cnss_logger */
	int radio_index;
	qdf_work_t sap_pre_cac_work;
	bool hbw_requested;
	bool pm_qos_request;
	enum RX_OFFLOAD ol_enable;
#ifdef WLAN_FEATURE_NAN
	bool nan_datapath_enabled;
#endif
	/* Present state of driver cds modules */
	enum driver_modules_status driver_status;
	struct qdf_delayed_work psoc_idle_timeout_work;
	bool rps;
	bool dynamic_rps;
	bool enable_rxthread;
	/* support for DP RX threads */
	bool enable_dp_rx_threads;
	bool napi_enable;
	struct acs_dfs_policy acs_policy;
	uint16_t wmi_max_len;
	struct suspend_resume_stats suspend_resume_stats;
	struct hdd_runtime_pm_context runtime_context;
	struct scan_chan_info *chan_info;
	struct mutex chan_info_lock;
	/* bit map to set/reset TDLS by different sources */
	unsigned long tdls_source_bitmap;
	bool tdls_umac_comp_active;
	bool tdls_nap_active;
	uint8_t beacon_probe_rsp_cnt_per_scan;
	uint8_t last_scan_reject_vdev_id;
	enum scan_reject_states last_scan_reject_reason;
	unsigned long last_scan_reject_timestamp;
	uint8_t scan_reject_cnt;
	bool dfs_cac_offload;
	bool reg_offload;
	bool rcpi_enabled;
#ifdef FEATURE_WLAN_CH_AVOID
	struct ch_avoid_ind_type coex_avoid_freq_list;
	struct ch_avoid_ind_type dnbs_avoid_freq_list;
	/* Lock to control access to dnbs and coex avoid freq list */
	struct mutex avoid_freq_lock;
#endif
#ifdef WLAN_FEATURE_TSF
	/* indicate whether tsf has been initialized */
	qdf_atomic_t tsf_ready_flag;
	/* indicate whether it's now capturing tsf(updating tstamp-pair) */
	qdf_atomic_t cap_tsf_flag;
	/* the context that is capturing tsf */
	struct hdd_adapter *cap_tsf_context;
#endif
#ifdef WLAN_FEATURE_TSF_PTP
	struct ptp_clock_info ptp_cinfo;
	struct ptp_clock *ptp_clock;
#endif
	uint8_t bt_a2dp_active:1;
	uint8_t bt_vo_active:1;
	enum band_info curr_band;
	bool imps_enabled;
#ifdef WLAN_FEATURE_PACKET_FILTERING
	int user_configured_pkt_filter_rules;
#endif
	bool is_fils_roaming_supported;
	QDF_STATUS (*receive_offload_cb)(struct hdd_adapter *,
					 struct sk_buff *);
	qdf_atomic_t vendor_disable_lro_flag;

	/* disable RX offload (GRO/LRO) in concurrency scenarios */
	qdf_atomic_t disable_rx_ol_in_concurrency;
	/* disable RX offload (GRO/LRO) in low throughput scenarios */
	qdf_atomic_t disable_rx_ol_in_low_tput;
	bool en_tcp_delack_no_lro;
	bool force_rsne_override;
	qdf_wake_lock_t monitor_mode_wakelock;
	bool lte_coex_ant_share;
	bool obss_scan_offload;
	int sscan_pid;
	uint32_t track_arp_ip;

	/* defining the board related information */
	uint32_t hw_bd_id;
	struct board_info hw_bd_info;
#ifdef WLAN_SUPPORT_TWT
	enum twt_status twt_state;
	qdf_event_t twt_disable_comp_evt;
	qdf_event_t twt_enable_comp_evt;
#endif
#ifdef FEATURE_WLAN_APF
	uint32_t apf_version;
	bool apf_enabled_v2;
#endif

#ifdef DISABLE_CHANNEL_LIST
	struct hdd_cache_channels *original_channels;
	qdf_mutex_t cache_channel_lock;
#endif
	enum sar_version sar_version;
	struct hdd_dynamic_mac dynamic_mac_list[QDF_MAX_CONCURRENCY_PERSONA];
	bool dynamic_nss_chains_support;
	struct qdf_mac_addr hw_macaddr;
	struct qdf_mac_addr provisioned_mac_addr[QDF_MAX_CONCURRENCY_PERSONA];
	struct qdf_mac_addr derived_mac_addr[QDF_MAX_CONCURRENCY_PERSONA];
	uint32_t num_provisioned_addr;
	uint32_t num_derived_addr;
	unsigned long provisioned_intf_addr_mask;
	unsigned long derived_intf_addr_mask;

	struct sar_limit_cmd_params *sar_cmd_params;
#ifdef SAR_SAFETY_FEATURE
	qdf_mc_timer_t sar_safety_timer;
	qdf_mc_timer_t sar_safety_unsolicited_timer;
	qdf_event_t sar_safety_req_resp_event;
	qdf_atomic_t sar_safety_req_resp_event_in_progress;
#endif

	qdf_time_t runtime_resume_start_time_stamp;
	qdf_time_t runtime_suspend_done_time_stamp;
#if defined(CLD_PM_QOS) && defined(CLD_DEV_PM_QOS)
	struct dev_pm_qos_request pm_qos_req[NR_CPUS];
	struct cpumask qos_cpu_mask;
#elif defined(CLD_PM_QOS)
	struct pm_qos_request pm_qos_req;
#endif
#ifdef WLAN_FEATURE_PKT_CAPTURE
	/* enable packet capture support */
	bool enable_pkt_capture_support;
	/* value for packet capturte mode */
	uint8_t val_pkt_capture_mode;
#endif
	bool roam_ch_from_fw_supported;
#ifdef FW_THERMAL_THROTTLE_SUPPORT
	uint8_t dutycycle_off_percent;
#endif
	uint8_t pm_qos_request_flags;
	uint8_t high_bus_bw_request;
	qdf_work_t country_change_work;
	struct {
		qdf_atomic_t rx_aggregation;
		uint8_t gro_force_flush[DP_MAX_RX_THREADS];
	} dp_agg_param;
	int current_pcie_gen_speed;
	qdf_workqueue_t *adapter_ops_wq;
	struct hdd_adapter_ops_history adapter_ops_history;
	bool ll_stats_per_chan_rx_tx_time;
#ifdef FEATURE_CLUB_LL_STATS_AND_GET_STATION
	bool is_get_station_clubbed_in_ll_stats_req;
#endif
#ifdef FEATURE_WPSS_THERMAL_MITIGATION
	bool multi_client_thermal_mitigation;
#endif
	bool disconnect_for_sta_mon_conc;
	bool is_dual_mac_cfg_updated;
	bool is_regulatory_update_in_progress;
	qdf_event_t regulatory_update_event;
	qdf_mutex_t regulatory_status_lock;
	bool is_fw_dbg_log_levels_configured;
#ifdef WLAN_SUPPORT_TWT
	qdf_work_t twt_en_dis_work;
#endif
	bool dump_in_progress;
#ifdef THERMAL_STATS_SUPPORT
	bool is_therm_stats_in_progress;
#endif
};

/**
 * struct hdd_vendor_acs_chan_params - vendor acs channel parameters
 * @pcl_count: pcl list count
 * @vendor_pcl_list: pointer to pcl frequency (MHz) list
 * @vendor_weight_list: pointer to pcl weight list
 */
struct hdd_vendor_acs_chan_params {
	uint32_t pcl_count;
	uint32_t *vendor_pcl_list;
	uint8_t *vendor_weight_list;
};

/**
 * struct hdd_external_acs_timer_context - acs timer context
 * @reason: reason for acs trigger
 * @adapter: hdd adapter for acs
 */
struct hdd_external_acs_timer_context {
	int8_t reason;
	struct hdd_adapter *adapter;
};

/**
 * struct hdd_vendor_chan_info - vendor channel info
 * @band: channel operating band
 * @pri_chan_freq: primary channel freq in MHz
 * @ht_sec_chan_freq: secondary channel freq in MHz
 * @vht_seg0_center_chan_freq: segment0 for vht in MHz
 * @vht_seg1_center_chan_freq: vht segment 1 in MHz
 * @chan_width: channel width
 */
struct hdd_vendor_chan_info {
	uint8_t band;
	uint32_t pri_chan_freq;
	uint32_t ht_sec_chan_freq;
	uint32_t vht_seg0_center_chan_freq;
	uint32_t vht_seg1_center_chan_freq;
	uint8_t chan_width;
};

/**
 * struct  hdd_channel_info - standard channel info
 * @freq: Freq in Mhz
 * @flags: channel info flags
 * @flagext: extended channel info flags
 * @ieee_chan_number: channel number
 * @max_reg_power: max tx power according to regulatory
 * @max_radio_power: max radio power
 * @min_radio_power: min radio power
 * @reg_class_id: regulatory class
 * @max_antenna_gain: max antenna gain allowed on channel
 * @vht_center_freq_seg0: vht center freq segment 0
 * @vht_center_freq_seg1: vht center freq segment 1
 */
struct hdd_channel_info {
	u_int16_t freq;
	u_int32_t flags;
	u_int16_t flagext;
	u_int8_t ieee_chan_number;
	int8_t max_reg_power;
	int8_t max_radio_power;
	int8_t min_radio_power;
	u_int8_t reg_class_id;
	u_int8_t max_antenna_gain;
	u_int8_t vht_center_freq_seg0;
	u_int8_t vht_center_freq_seg1;
};

/*
 * Function declarations and documentation
 */

/**
 * wlan_hdd_history_get_next_index() - get next index to store the history
				       entry
 * @curr_idx: current index
 * @max_entries: max entries in the history
 *
 * Returns: The index at which record is to be stored in history
 */
static inline uint32_t wlan_hdd_history_get_next_index(qdf_atomic_t *curr_idx,
						       uint32_t max_entries)
{
	uint32_t idx = qdf_atomic_inc_return(curr_idx);

	return idx & (max_entries - 1);
}

/**
 * hdd_adapter_ops_record_event() - record an entry in the adapter ops history
 * @hdd_ctx: pointer to hdd context
 * @event: event
 * @vdev_id: vdev id corresponding to event
 *
 * Returns: None
 */
static inline void
hdd_adapter_ops_record_event(struct hdd_context *hdd_ctx,
			     enum hdd_adapter_ops_event event,
			     int vdev_id)
{
	struct hdd_adapter_ops_history *adapter_hist;
	struct hdd_adapter_ops_record *record;
	uint32_t idx;

	adapter_hist = &hdd_ctx->adapter_ops_history;

	idx = wlan_hdd_history_get_next_index(&adapter_hist->index,
					      WLAN_HDD_ADAPTER_OPS_HISTORY_MAX);

	record = &adapter_hist->entry[idx];
	record->event = event;
	record->vdev_id = vdev_id;
	record->timestamp = qdf_get_log_timestamp();
}

/**
 * hdd_validate_channel_and_bandwidth() - Validate the channel-bandwidth combo
 * @adapter: HDD adapter
 * @chan_freq: Channel frequency
 * @chan_bw: Bandwidth
 *
 * Checks if the given bandwidth is valid for the given channel number.
 *
 * Return: 0 for success, non-zero for failure
 */
int hdd_validate_channel_and_bandwidth(struct hdd_adapter *adapter,
				       qdf_freq_t chan_freq,
				       enum phy_ch_width chan_bw);

/**
 * hdd_get_front_adapter() - Get the first adapter from the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @current_adapter: pointer to the current adapter
 * @out_adapter: double pointer to pass the next adapter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_front_adapter(struct hdd_context *hdd_ctx,
				 struct hdd_adapter **out_adapter);

/**
 * hdd_get_next_adapter() - Get the next adapter from the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @current_adapter: pointer to the current adapter
 * @out_adapter: double pointer to pass the next adapter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_next_adapter(struct hdd_context *hdd_ctx,
				struct hdd_adapter *current_adapter,
				struct hdd_adapter **out_adapter);

/**
 * hdd_get_front_adapter_no_lock() - Get the first adapter from the adapter list
 * This API doesnot use any lock in it's implementation. It is the caller's
 * directive to ensure concurrency safety.
 * @hdd_ctx: pointer to the HDD context
 * @out_adapter: double pointer to pass the next adapter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_front_adapter_no_lock(struct hdd_context *hdd_ctx,
					 struct hdd_adapter **out_adapter);

/**
 * hdd_get_next_adapter_no_lock() - Get the next adapter from the adapter list
 * This API doesnot use any lock in it's implementation. It is the caller's
 * directive to ensure concurrency safety.
 * @hdd_ctx: pointer to the HDD context
 * @current_adapter: pointer to the current adapter
 * @out_adapter: double pointer to pass the next adapter
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_get_next_adapter_no_lock(struct hdd_context *hdd_ctx,
					struct hdd_adapter *current_adapter,
					struct hdd_adapter **out_adapter);

/**
 * hdd_remove_adapter() - Remove the adapter from the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @adapter: pointer to the adapter to be removed
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_remove_adapter(struct hdd_context *hdd_ctx,
			      struct hdd_adapter *adapter);

/**
 * hdd_remove_front_adapter() - Remove the first adapter from the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @out_adapter: pointer to the adapter to be removed
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_remove_front_adapter(struct hdd_context *hdd_ctx,
				    struct hdd_adapter **out_adapter);

/**
 * hdd_add_adapter_back() - Add an adapter to the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @adapter: pointer to the adapter to be added
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_add_adapter_back(struct hdd_context *hdd_ctx,
				struct hdd_adapter *adapter);

/**
 * hdd_add_adapter_front() - Add an adapter to the head of the adapter list
 * @hdd_ctx: pointer to the HDD context
 * @adapter: pointer to the adapter to be added
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_add_adapter_front(struct hdd_context *hdd_ctx,
				 struct hdd_adapter *adapter);

/**
 * typedef hdd_adapter_iterate_cb() – Iteration callback function
 * @adapter: current adapter of interest
 * @context: user context supplied to the iterator
 *
 * This specifies the type of a callback function to supply to
 * hdd_adapter_iterate().
 *
 * Return:
 * * QDF_STATUS_SUCCESS if further iteration should continue
 * * QDF_STATUS_E_ABORTED if further iteration should be aborted
 */
typedef QDF_STATUS (*hdd_adapter_iterate_cb)(struct hdd_adapter *adapter,
					     void *context);

/**
 * hdd_adapter_iterate() – Safely iterate over all adapters
 * @cb: callback function to invoke for each adapter
 * @context: user-supplied context to pass to @cb
 *
 * This function will iterate over all of the adapters known to the system in
 * a safe manner, invoking the callback function for each adapter.
 * The callback function will be invoked in the same context/thread as the
 * caller without any additional locks in force.
 * Iteration continues until either the callback has been invoked for all
 * adapters or a callback returns a value of QDF_STATUS_E_ABORTED to indicate
 * that further iteration should cease.
 *
 * Return:
 * * QDF_STATUS_E_ABORTED if any callback function returns that value
 * * QDF_STATUS_E_FAILURE if the callback was not invoked for all adapters due
 * to concurrency (i.e. adapter was deleted while iterating)
 * * QDF_STATUS_SUCCESS if @cb was invoked for each adapter and none returned
 * an error
 */
QDF_STATUS hdd_adapter_iterate(hdd_adapter_iterate_cb cb,
			       void *context);

/**
 * hdd_adapter_dev_hold_debug - Debug API to call dev_hold
 * @adapter: hdd_adapter pointer
 * @dbgid: Debug ID corresponding to API that is requesting the dev_hold
 *
 * Return: none
 */
void hdd_adapter_dev_hold_debug(struct hdd_adapter *adapter,
				wlan_net_dev_ref_dbgid dbgid);

/**
 * hdd_adapter_dev_put_debug - Debug API to call dev_put
 * @adapter: hdd_adapter pointer
 * @dbgid: Debug ID corresponding to API that is requesting the dev_put
 *
 * Return: none
 */
void hdd_adapter_dev_put_debug(struct hdd_adapter *adapter,
			       wlan_net_dev_ref_dbgid dbgid);

/**
 * __hdd_take_ref_and_fetch_front_adapter_safe - Helper macro to lock, fetch
 * front and next adapters, take ref and unlock.
 * @hdd_ctx: the global HDD context
 * @adapter: an hdd_adapter pointer to use as a cursor
 * @next_adapter: hdd_adapter pointer to next adapter
 * @dbgid: debug ID to detect reference leaks
 */
#define __hdd_take_ref_and_fetch_front_adapter_safe(hdd_ctx, adapter, \
						    next_adapter, dbgid) \
	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock), \
	hdd_get_front_adapter_no_lock(hdd_ctx, &adapter), \
	(adapter) ? hdd_adapter_dev_hold_debug(adapter, dbgid) : (false), \
	hdd_get_next_adapter_no_lock(hdd_ctx, adapter, &next_adapter), \
	(next_adapter) ? hdd_adapter_dev_hold_debug(next_adapter, dbgid) : \
			 (false), \
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock)

/**
 * __hdd_take_ref_and_fetch_next_adapter_safe - Helper macro to lock, fetch next
 * adapter, take ref and unlock.
 * @hdd_ctx: the global HDD context
 * @adapter: hdd_adapter pointer to use as a cursor
 * @next_adapter: hdd_adapter pointer to next adapter
 * @dbgid: debug ID to detect reference leaks
 */
#define __hdd_take_ref_and_fetch_next_adapter_safe(hdd_ctx, adapter, \
						   next_adapter, dbgid) \
	qdf_spin_lock_bh(&hdd_ctx->hdd_adapter_lock), \
	adapter = next_adapter, \
	hdd_get_next_adapter_no_lock(hdd_ctx, adapter, &next_adapter), \
	(next_adapter) ? hdd_adapter_dev_hold_debug(next_adapter, dbgid) : \
			 (false), \
	qdf_spin_unlock_bh(&hdd_ctx->hdd_adapter_lock)

/**
 * __hdd_is_adapter_valid - Helper macro to return true/false for valid adapter.
 * @adapter: an hdd_adapter pointer to use as a cursor
 */
#define __hdd_is_adapter_valid(_adapter) !!_adapter

/**
 * hdd_for_each_adapter_dev_held_safe - Adapter iterator with dev_hold called
 *                                      in a delete safe manner
 * @hdd_ctx: the global HDD context
 * @adapter: an hdd_adapter pointer to use as a cursor
 * @next_adapter: hdd_adapter pointer to the next adapter
 *
 * This iterator will take the reference of the netdev associated with the
 * given adapter so as to prevent it from being removed in other context. It
 * also takes the reference of the next adapter if exist. This avoids infinite
 * loop due to deletion of the adapter list entry inside the loop. Deletion of
 * list entry will make the list entry to point to self. If the control goes
 * inside the loop body then the dev_hold has been invoked.
 *
 *                           ***** NOTE *****
 * Before the end of each iteration, hdd_adapter_dev_put_debug(adapter, dbgid)
 * must be called. Not calling this will keep hold of a reference, thus
 * preventing unregister of the netdevice. If the loop is terminated in
 * between with return/goto/break statements,
 * hdd_adapter_dev_put_debug(next_adapter, dbgid) must be done along with
 * hdd_adapter_dev_put_debug(adapter, dbgid) before termination of the loop.
 *
 * Usage example:
 *  hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter, dbgid) {
 *        <work involving adapter>
 *        <some more work>
 *        hdd_adapter_dev_put_debug(adapter, dbgid)
 *  }
 */
#define hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter, \
					   dbgid) \
	for (__hdd_take_ref_and_fetch_front_adapter_safe(hdd_ctx, adapter, \
							 next_adapter, dbgid); \
	     __hdd_is_adapter_valid(adapter); \
	     __hdd_take_ref_and_fetch_next_adapter_safe(hdd_ctx, adapter, \
							next_adapter, dbgid))

/**
 * wlan_hdd_get_adapter_by_vdev_id_from_objmgr() - Fetch adapter from objmgr
 * using vdev_id.
 * @hdd_ctx: the global HDD context
 * @adapter: an hdd_adapter double pointer to store the address of the adapter
 * @vdev: the vdev whose corresponding adapter has to be fetched
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
wlan_hdd_get_adapter_by_vdev_id_from_objmgr(struct hdd_context *hdd_ctx,
					    struct hdd_adapter **adapter,
					    struct wlan_objmgr_vdev *vdev);

struct hdd_adapter *hdd_open_adapter(struct hdd_context *hdd_ctx,
				     uint8_t session_type,
				     const char *name, tSirMacAddr mac_addr,
				     unsigned char name_assign_type,
				     bool rtnl_held);

/**
 * hdd_close_adapter() - remove and free @adapter from the adapter list
 * @hdd_ctx: The Hdd context containing the adapter list
 * @adapter: the adapter to remove and free
 * @rtnl_held: if the caller is already holding the RTNL lock
 *
 * Return: None
 */
void hdd_close_adapter(struct hdd_context *hdd_ctx,
		       struct hdd_adapter *adapter,
		       bool rtnl_held);

/**
 * hdd_close_all_adapters() - remove and free all adapters from the adapter list
 * @hdd_ctx: The Hdd context containing the adapter list
 * @rtnl_held: if the caller is already holding the RTNL lock
 *
 * Return: None
 */
void hdd_close_all_adapters(struct hdd_context *hdd_ctx, bool rtnl_held);

QDF_STATUS hdd_stop_all_adapters(struct hdd_context *hdd_ctx);
void hdd_deinit_all_adapters(struct hdd_context *hdd_ctx, bool rtnl_held);
QDF_STATUS hdd_reset_all_adapters(struct hdd_context *hdd_ctx);
QDF_STATUS hdd_start_all_adapters(struct hdd_context *hdd_ctx);

/**
 * hdd_get_adapter_by_vdev() - Return adapter with the given vdev id
 * @hdd_ctx: hdd context.
 * @vdev_id: vdev id for the adapter to get.
 *
 * This function is used to get the adapter with provided vdev id
 *
 * Return: adapter pointer if found
 *
 */
struct hdd_adapter *hdd_get_adapter_by_vdev(struct hdd_context *hdd_ctx,
					    uint32_t vdev_id);

/**
 * hdd_adapter_get_by_reference() - Return adapter with the given reference
 * @hdd_ctx: hdd context
 * @reference: reference for the adapter to get
 *
 * This function is used to get the adapter with provided reference.
 * The adapter reference will be held until being released by calling
 * hdd_adapter_put().
 *
 * Return: adapter pointer if found
 *
 */
struct hdd_adapter *hdd_adapter_get_by_reference(struct hdd_context *hdd_ctx,
						 struct hdd_adapter *reference);

/**
 * hdd_adapter_put() - Release reference to adapter
 * @adapter: adapter reference
 *
 * Release reference to adapter previously acquired via
 * hdd_adapter_get_*() function
 */
void hdd_adapter_put(struct hdd_adapter *adapter);

struct hdd_adapter *hdd_get_adapter_by_macaddr(struct hdd_context *hdd_ctx,
					       tSirMacAddr mac_addr);

/**
 * hdd_get_adapter_home_channel() - return home channel of adapter
 * @adapter: hdd adapter of vdev
 *
 * This function returns operation channel of station/p2p-cli if
 * connected, returns opration channel of sap/p2p-go if started.
 *
 * Return: home channel if connected/started or invalid channel 0
 */
uint32_t hdd_get_adapter_home_channel(struct hdd_adapter *adapter);

/*
 * hdd_get_adapter_by_rand_macaddr() - find Random mac adapter
 * @hdd_ctx: hdd context
 * @mac_addr: random mac addr
 *
 * Find the Adapter based on random mac addr. Adapter's vdev
 * have active random mac list.
 *
 * Return: adapter ptr or null
 */
struct hdd_adapter *
hdd_get_adapter_by_rand_macaddr(struct hdd_context *hdd_ctx,
				tSirMacAddr mac_addr);

/**
 * hdd_is_vdev_in_conn_state() - Check whether the vdev is in
 * connected/started state.
 * @adapter: hdd adapter of the vdev
 *
 * This function will give whether the vdev in the adapter is in
 * connected/started state.
 *
 * Return: True/false
 */
bool hdd_is_vdev_in_conn_state(struct hdd_adapter *adapter);

/**
 * hdd_vdev_create() - Create the vdev in the firmware
 * @adapter: hdd adapter
 *
 * This function will create the vdev in the firmware
 *
 * Return: 0 when the vdev create is sent to firmware or -EINVAL when
 * there is a failure to send the command.
 */
int hdd_vdev_create(struct hdd_adapter *adapter);
int hdd_vdev_destroy(struct hdd_adapter *adapter);
int hdd_vdev_ready(struct hdd_adapter *adapter);

QDF_STATUS hdd_init_station_mode(struct hdd_adapter *adapter);
struct hdd_adapter *hdd_get_adapter(struct hdd_context *hdd_ctx,
			enum QDF_OPMODE mode);

/**
 * hdd_get_device_mode() - Get device mode
 * @vdev_id: vdev id
 *
 * Return: Device mode
 */
enum QDF_OPMODE hdd_get_device_mode(uint32_t vdev_id);

void hdd_deinit_adapter(struct hdd_context *hdd_ctx,
			struct hdd_adapter *adapter,
			bool rtnl_held);
QDF_STATUS hdd_stop_adapter(struct hdd_context *hdd_ctx,
			    struct hdd_adapter *adapter);

/**
 * hdd_set_station_ops() - update net_device ops
 * @dev: Handle to struct net_device to be updated.
 * Return: None
 */
void hdd_set_station_ops(struct net_device *dev);

/**
 * wlan_hdd_get_intf_addr() - Get address for the interface
 * @hdd_ctx: Pointer to hdd context
 * @interface_type: type of the interface for which address is queried
 *
 * This function is used to get mac address for every new interface
 *
 * Return: If addr is present then return pointer to MAC address
 *         else NULL
 */

uint8_t *wlan_hdd_get_intf_addr(struct hdd_context *hdd_ctx,
				enum QDF_OPMODE interface_type);
void wlan_hdd_release_intf_addr(struct hdd_context *hdd_ctx,
				uint8_t *releaseAddr);

/**
 * hdd_get_operating_chan_freq() - return operating channel of the device mode
 * @hdd_ctx:	Pointer to the HDD context.
 * @mode:	Device mode for which operating channel is required.
 *              Supported modes:
 *			QDF_STA_MODE,
 *			QDF_P2P_CLIENT_MODE,
 *			QDF_SAP_MODE,
 *			QDF_P2P_GO_MODE.
 *
 * This API returns the operating channel of the requested device mode
 *
 * Return: channel frequency, or
 *         0 if the requested device mode is not found.
 */
uint32_t hdd_get_operating_chan_freq(struct hdd_context *hdd_ctx,
				     enum QDF_OPMODE mode);

void hdd_set_conparam(int32_t con_param);
enum QDF_GLOBAL_MODE hdd_get_conparam(void);
void wlan_hdd_reset_prob_rspies(struct hdd_adapter *adapter);
void hdd_prevent_suspend(uint32_t reason);

/*
 * hdd_get_first_valid_adapter() - Get the first valid adapter from adapter list
 *
 * This function is used to fetch the first valid adapter from the adapter
 * list. If there is no valid adapter then it returns NULL
 *
 * @hdd_ctx: HDD context handler
 *
 * Return: NULL if no valid adapter found in the adapter list
 *
 */
struct hdd_adapter *hdd_get_first_valid_adapter(struct hdd_context *hdd_ctx);

void hdd_allow_suspend(uint32_t reason);
void hdd_prevent_suspend_timeout(uint32_t timeout, uint32_t reason);

/**
 * wlan_hdd_validate_context() - check the HDD context
 * @hdd_ctx: Global HDD context pointer
 *
 * Return: 0 if the context is valid. Error code otherwise
 */
#define wlan_hdd_validate_context(hdd_ctx) \
	__wlan_hdd_validate_context(hdd_ctx, __func__)

int __wlan_hdd_validate_context(struct hdd_context *hdd_ctx, const char *func);

/**
 * hdd_validate_adapter() - Validate the given adapter
 * @adapter: the adapter to validate
 *
 * This function validates the given adapter, and ensures that it is open.
 *
 * Return: Errno
 */
#define hdd_validate_adapter(adapter) \
	__hdd_validate_adapter(adapter, __func__)

int __hdd_validate_adapter(struct hdd_adapter *adapter, const char *func);

/**
 * wlan_hdd_validate_vdev_id() - ensure the given vdev Id is valid
 * @vdev_id: the vdev Id to validate
 *
 * Return: Errno
 */
#define wlan_hdd_validate_vdev_id(vdev_id) \
	__wlan_hdd_validate_vdev_id(vdev_id, __func__)

int __wlan_hdd_validate_vdev_id(uint8_t vdev_id, const char *func);

/**
 * hdd_is_valid_mac_address() - validate MAC address
 * @mac_addr:	Pointer to the input MAC address
 *
 * This function validates whether the given MAC address is valid or not
 * Expected MAC address is of the format XX:XX:XX:XX:XX:XX
 * where X is the hexa decimal digit character and separated by ':'
 * This algorithm works even if MAC address is not separated by ':'
 *
 * This code checks given input string mac contains exactly 12 hexadecimal
 * digits and a separator colon : appears in the input string only after
 * an even number of hex digits.
 *
 * Return: true for valid and false for invalid
 */
bool hdd_is_valid_mac_address(const uint8_t *mac_addr);

bool wlan_hdd_validate_modules_state(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_validate_mac_address() - Function to validate mac address
 * @mac_addr: input mac address
 *
 * Return QDF_STATUS
 */
#define wlan_hdd_validate_mac_address(mac_addr) \
	__wlan_hdd_validate_mac_address(mac_addr, __func__)

QDF_STATUS __wlan_hdd_validate_mac_address(struct qdf_mac_addr *mac_addr,
					   const char *func);

/**
 * hdd_is_any_adapter_connected() - Check if any adapter is in connected state
 * @hdd_ctx: the global hdd context
 *
 * Returns: true, if any of the adapters is in connected state,
 *	    false, if none of the adapters is in connected state.
 */
bool hdd_is_any_adapter_connected(struct hdd_context *hdd_ctx);

/**
 * hdd_add_latency_critical_client() - Add latency critical client
 * @adapter: adapter handle (Should not be NULL)
 * @phymode: the phymode of the connected adapter
 *
 * This function checks if the present connection is latency critical
 * and adds to the latency critical clients count and informs the
 * datapath about this connection being latency critical.
 *
 * Returns: None
 */
static inline void
hdd_add_latency_critical_client(struct hdd_adapter *adapter,
				enum qca_wlan_802_11_mode phymode)
{
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;

	switch (phymode) {
	case QCA_WLAN_802_11_MODE_11A:
	case QCA_WLAN_802_11_MODE_11G:
		qdf_atomic_inc(&hdd_ctx->num_latency_critical_clients);

		hdd_debug("Adding latency critical connection for vdev %d",
			  adapter->vdev_id);
		cdp_vdev_inform_ll_conn(cds_get_context(QDF_MODULE_ID_SOC),
					adapter->vdev_id,
					CDP_VDEV_LL_CONN_ADD);
		break;
	default:
		break;
	}
}

/**
 * hdd_del_latency_critical_client() - Add tlatency critical client
 * @adapter: adapter handle (Should not be NULL)
 * @phymode: the phymode of the connected adapter
 *
 * This function checks if the present connection was latency critical
 * and removes from the latency critical clients count and informs the
 * datapath about the removed connection being latency critical.
 *
 * Returns: None
 */
static inline void
hdd_del_latency_critical_client(struct hdd_adapter *adapter,
				enum qca_wlan_802_11_mode phymode)
{
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;

	switch (phymode) {
	case QCA_WLAN_802_11_MODE_11A:
	case QCA_WLAN_802_11_MODE_11G:
		qdf_atomic_dec(&hdd_ctx->num_latency_critical_clients);

		hdd_info("Removing latency critical connection for vdev %d",
			 adapter->vdev_id);
		cdp_vdev_inform_ll_conn(cds_get_context(QDF_MODULE_ID_SOC),
					adapter->vdev_id,
					CDP_VDEV_LL_CONN_DEL);
		break;
	default:
		break;
	}
}

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
/**
 * hdd_bus_bw_compute_prev_txrx_stats() - get tx and rx stats
 * @adapter: hdd adapter reference
 *
 * This function get the collected tx and rx stats before starting
 * the bus bandwidth timer.
 *
 * Return: None
 */
void hdd_bus_bw_compute_prev_txrx_stats(struct hdd_adapter *adapter);

/**
 * hdd_bus_bw_compute_reset_prev_txrx_stats() - reset previous tx and rx stats
 * @adapter: hdd adapter reference
 *
 * This function resets the adapter previous tx rx stats.
 *
 * Return: None
 */
void hdd_bus_bw_compute_reset_prev_txrx_stats(struct hdd_adapter *adapter);

/**
 * hdd_bus_bw_compute_timer_start() - start the bandwidth timer
 * @hdd_ctx: the global hdd context
 *
 * Return: None
 */
void hdd_bus_bw_compute_timer_start(struct hdd_context *hdd_ctx);

/**
 * hdd_bus_bw_compute_timer_try_start() - try to start the bandwidth timer
 * @hdd_ctx: the global hdd context
 *
 * This function ensures there is at least one adapter in the associated state
 * before starting the bandwidth timer.
 *
 * Return: None
 */
void hdd_bus_bw_compute_timer_try_start(struct hdd_context *hdd_ctx);

/**
 * hdd_bus_bw_compute_timer_stop() - stop the bandwidth timer
 * @hdd_ctx: the global hdd context
 *
 * Return: None
 */
void hdd_bus_bw_compute_timer_stop(struct hdd_context *hdd_ctx);

/**
 * hdd_bus_bw_compute_timer_try_stop() - try to stop the bandwidth timer
 * @hdd_ctx: the global hdd context
 *
 * This function ensures there are no adapters in the associated state before
 * stopping the bandwidth timer.
 *
 * Return: None
 */
void hdd_bus_bw_compute_timer_try_stop(struct hdd_context *hdd_ctx);

/**
 * hdd_bus_bandwidth_init() - Initialize bus bandwidth data structures.
 * @hdd_ctx: HDD context
 *
 * Initialize bus bandwidth related data structures like spinlock and timer.
 *
 * Return: None.
 */
int hdd_bus_bandwidth_init(struct hdd_context *hdd_ctx);

/**
 * hdd_bus_bandwidth_deinit() - De-initialize bus bandwidth data structures.
 * @hdd_ctx: HDD context
 *
 * De-initialize bus bandwidth related data structures like timer.
 *
 * Return: None.
 */
void hdd_bus_bandwidth_deinit(struct hdd_context *hdd_ctx);

static inline enum pld_bus_width_type
hdd_get_current_throughput_level(struct hdd_context *hdd_ctx)
{
	return hdd_ctx->cur_vote_level;
}

/**
 * hdd_set_current_throughput_level() - update the current vote
 * level
 * @hdd_ctx: the global hdd context
 * @next_vote_level: pld_bus_width_type voting level
 *
 * This function updates the current vote level to the new level
 * provided
 *
 * Return: None
 */
static inline void
hdd_set_current_throughput_level(struct hdd_context *hdd_ctx,
				 enum pld_bus_width_type next_vote_level)
{
	hdd_ctx->cur_vote_level = next_vote_level;
}

static inline bool
hdd_is_low_tput_gro_enable(struct hdd_context *hdd_ctx)
{
	return (qdf_atomic_read(&hdd_ctx->low_tput_gro_enable)) ? true : false;
}

#define GET_CUR_RX_LVL(config) ((config)->cur_rx_level)
#define GET_BW_COMPUTE_INTV(config) ((config)->bus_bw_compute_interval)
#else

static inline
void hdd_bus_bw_compute_prev_txrx_stats(struct hdd_adapter *adapter)
{
}

static inline
void hdd_bus_bw_compute_reset_prev_txrx_stats(struct hdd_adapter *adapter)
{
}

static inline
void hdd_bus_bw_compute_timer_start(struct hdd_context *hdd_ctx)
{
}

static inline
void hdd_bus_bw_compute_timer_try_start(struct hdd_context *hdd_ctx)
{
}

static inline
void hdd_bus_bw_compute_timer_stop(struct hdd_context *hdd_ctx)
{
}

static inline
void hdd_bus_bw_compute_timer_try_stop(struct hdd_context *hdd_ctx)
{
}

static inline
int hdd_bus_bandwidth_init(struct hdd_context *hdd_ctx)
{
	return 0;
}

static inline
void hdd_bus_bandwidth_deinit(struct hdd_context *hdd_ctx)
{
}

static inline enum pld_bus_width_type
hdd_get_current_throughput_level(struct hdd_context *hdd_ctx)
{
	return PLD_BUS_WIDTH_NONE;
}

static inline void
hdd_set_current_throughput_level(struct hdd_context *hdd_ctx,
				 enum pld_bus_width_type next_vote_level)
{
}

static inline bool
hdd_is_low_tput_gro_enable(struct hdd_context *hdd_ctx)
{
	return false;
}

#define GET_CUR_RX_LVL(config) 0
#define GET_BW_COMPUTE_INTV(config) 0

#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/

/**
 * hdd_init_adapter_ops_wq() - Init global workqueue for adapter operations.
 * @hdd_ctx: pointer to HDD context
 *
 * Return: QDF_STATUS_SUCCESS if workqueue is allocated,
 *	   QDF_STATUS_E_NOMEM if workqueue aloocation fails.
 */
QDF_STATUS hdd_init_adapter_ops_wq(struct hdd_context *hdd_ctx);

/**
 * hdd_deinit_adapter_ops_wq() - Deinit global workqueue for adapter operations.
 * @hdd_ctx: pointer to HDD context
 *
 * Return: None
 */
void hdd_deinit_adapter_ops_wq(struct hdd_context *hdd_ctx);

/**
 * hdd_adapter_feature_update_work_init() - Init per adapter work for netdev
 *					    feature update
 * @adapter: pointer to adapter structure
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_adapter_feature_update_work_init(struct hdd_adapter *adapter);

/**
 * hdd_adapter_feature_update_work_deinit() - Deinit per adapter work for
 *					      netdev feature update
 * @adapter: pointer to adapter structure
 *
 * Return: QDF_STATUS
 */
void hdd_adapter_feature_update_work_deinit(struct hdd_adapter *adapter);

int hdd_qdf_trace_enable(QDF_MODULE_ID module_id, uint32_t bitmask);

int hdd_init(void);
void hdd_deinit(void);

/**
 * hdd_wlan_startup() - HDD init function
 * hdd_ctx: the HDD context corresponding to the psoc to startup
 *
 * Return: Errno
 */
int hdd_wlan_startup(struct hdd_context *hdd_ctx);

/**
 * hdd_wlan_exit() - HDD WLAN exit function
 * @hdd_ctx: pointer to the HDD Context
 *
 * Return: None
 */
void hdd_wlan_exit(struct hdd_context *hdd_ctx);

/**
 * hdd_psoc_create_vdevs() - create the default vdevs for a psoc
 * @hdd_ctx: the HDD context for the psoc to operate against
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_psoc_create_vdevs(struct hdd_context *hdd_ctx);

/*
 * hdd_context_create() - Allocate and inialize HDD context.
 * @dev: Device Pointer to the underlying device
 *
 * Allocate and initialize HDD context. HDD context is allocated as part of
 * wiphy allocation and then context is initialized.
 *
 * Return: HDD context on success and ERR_PTR on failure
 */
struct hdd_context *hdd_context_create(struct device *dev);

/**
 * hdd_context_destroy() - Destroy HDD context
 * @hdd_ctx: HDD context to be destroyed.
 *
 * Free config and HDD context as well as destroy all the resources.
 *
 * Return: None
 */
void hdd_context_destroy(struct hdd_context *hdd_ctx);

int hdd_wlan_notify_modem_power_state(int state);

void wlan_hdd_send_svc_nlink_msg(int radio, int type, void *data, int len);
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
void wlan_hdd_auto_shutdown_enable(struct hdd_context *hdd_ctx, bool enable);
#endif

struct hdd_adapter *
hdd_get_con_sap_adapter(struct hdd_adapter *this_sap_adapter,
			bool check_start_bss);

bool hdd_is_5g_supported(struct hdd_context *hdd_ctx);

/**
 * hdd_is_2g_supported() - check if 2GHz channels are supported
 * @hdd_ctx:	Pointer to the hdd context
 *
 * HDD function to know if 2GHz channels are supported
 *
 * Return:  true if 2GHz channels are supported
 */
bool hdd_is_2g_supported(struct hdd_context *hdd_ctx);

int wlan_hdd_scan_abort(struct hdd_adapter *adapter);

/**
 * hdd_indicate_active_ndp_cnt() - Callback to indicate active ndp count to hdd
 * if ndp connection is on NDI established
 * @psoc: pointer to psoc object
 * @vdev_id: vdev id
 * @cnt: number of active ndp sessions
 *
 * This HDD callback registerd with policy manager to indicates number of active
 * ndp sessions to hdd.
 *
 * Return:  none
 */
void hdd_indicate_active_ndp_cnt(struct wlan_objmgr_psoc *psoc,
				 uint8_t vdev_id, uint8_t cnt);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static inline bool roaming_offload_enabled(struct hdd_context *hdd_ctx)
{
	bool is_roam_offload;

	ucfg_mlme_get_roaming_offload(hdd_ctx->psoc, &is_roam_offload);

	return is_roam_offload;
}
#else
static inline bool roaming_offload_enabled(struct hdd_context *hdd_ctx)
{
	return false;
}
#endif

#ifdef WLAN_FEATURE_HOST_ROAM
static inline bool hdd_driver_roaming_supported(struct hdd_context *hdd_ctx)
{
	bool lfr_enabled;

	ucfg_mlme_is_lfr_enabled(hdd_ctx->psoc, &lfr_enabled);

	return lfr_enabled;
}
#else
static inline bool hdd_driver_roaming_supported(struct hdd_context *hdd_ctx)
{
	return false;
}
#endif

static inline bool hdd_roaming_supported(struct hdd_context *hdd_ctx)
{
	bool val;

	val = hdd_driver_roaming_supported(hdd_ctx) ||
		roaming_offload_enabled(hdd_ctx);

	return val;
}

#ifdef CFG80211_SCAN_RANDOM_MAC_ADDR
static inline bool hdd_scan_random_mac_addr_supported(void)
{
	return true;
}
#else
static inline bool hdd_scan_random_mac_addr_supported(void)
{
	return false;
}
#endif

/**
 * hdd_start_vendor_acs(): Start vendor ACS procedure
 * @adapter: pointer to SAP adapter struct
 *
 * This function sends the ACS config to the ACS daemon and
 * starts the vendor ACS timer to wait for the next command.
 *
 * Return: Status of vendor ACS procedure
 */
int hdd_start_vendor_acs(struct hdd_adapter *adapter);

/**
 * hdd_acs_response_timeout_handler() - timeout handler for acs_timer
 * @context: timeout handler context
 *
 * Return: None
 */
void hdd_acs_response_timeout_handler(void *context);

/**
 * wlan_hdd_cfg80211_start_acs(): Start ACS Procedure for SAP
 * @adapter: pointer to SAP adapter struct
 *
 * This function starts the ACS procedure if there are no
 * constraints like MBSSID DFS restrictions.
 *
 * Return: Status of ACS Start procedure
 */
int wlan_hdd_cfg80211_start_acs(struct hdd_adapter *adapter);

/**
 * hdd_cfg80211_update_acs_config() - update acs config to application
 * @adapter: hdd adapter
 * @reason: channel change reason
 *
 * Return: 0 for success else error code
 */
int hdd_cfg80211_update_acs_config(struct hdd_adapter *adapter,
				   uint8_t reason);

/**
 * hdd_update_acs_timer_reason() - update acs timer start reason
 * @adapter: hdd adapter
 * @reason: channel change reason
 *
 * Return: 0 for success
 */
int hdd_update_acs_timer_reason(struct hdd_adapter *adapter, uint8_t reason);

/**
 * hdd_switch_sap_channel() - Move SAP to the given channel
 * @adapter: AP adapter
 * @channel: Channel
 * @forced: Force to switch channel, ignore SCC/MCC check
 *
 * Moves the SAP interface by invoking the function which
 * executes the callback to perform channel switch using (E)CSA.
 *
 * Return: None
 */
void hdd_switch_sap_channel(struct hdd_adapter *adapter, uint8_t channel,
			    bool forced);

/**
 * hdd_switch_sap_chan_freq() - Move SAP to the given channel
 * @adapter: AP adapter
 * @chan_freq: Channel frequency
 * @forced: Force to switch channel, ignore SCC/MCC check
 *
 * Moves the SAP interface by invoking the function which
 * executes the callback to perform channel switch using (E)CSA.
 *
 * Return: None
 */
void hdd_switch_sap_chan_freq(struct hdd_adapter *adapter, qdf_freq_t chan_freq,
			      bool forced);

#if defined(FEATURE_WLAN_CH_AVOID)
void hdd_unsafe_channel_restart_sap(struct hdd_context *hdd_ctx);

void hdd_ch_avoid_ind(struct hdd_context *hdd_ctxt,
		      struct unsafe_ch_list *unsafe_chan_list,
		      struct ch_avoid_ind_type *avoid_freq_list);
#else
static inline
void hdd_unsafe_channel_restart_sap(struct hdd_context *hdd_ctx)
{
}

static inline
void hdd_ch_avoid_ind(struct hdd_context *hdd_ctxt,
		      struct unsafe_ch_list *unsafe_chan_list,
		      struct ch_avoid_ind_type *avoid_freq_list)
{
}
#endif

/**
 * hdd_free_mac_address_lists() - Free both the MAC address lists
 * @hdd_ctx: HDD context
 *
 * This API clears/memset provisioned address list and
 * derived address list
 *
 */
void hdd_free_mac_address_lists(struct hdd_context *hdd_ctx);

/**
 * hdd_update_macaddr() - update mac address
 * @hdd_ctx:	hdd contxt
 * @hw_macaddr:	mac address
 * @generate_mac_auto: Indicates whether the first address is
 * provisioned address or derived address.
 *
 * Mac address for multiple virtual interface is found as following
 * i) The mac address of the first interface is just the actual hw mac address.
 * ii) MSM 3 or 4 bits of byte5 of the actual mac address are used to
 *     define the mac address for the remaining interfaces and locally
 *     admistered bit is set. INTF_MACADDR_MASK is based on the number of
 *     supported virtual interfaces, right now this is 0x07 (meaning 8
 *     interface).
 *     Byte[3] of second interface will be hw_macaddr[3](bit5..7) + 1,
 *     for third interface it will be hw_macaddr[3](bit5..7) + 2, etc.
 *
 * Return: None
 */
void hdd_update_macaddr(struct hdd_context *hdd_ctx,
			struct qdf_mac_addr hw_macaddr, bool generate_mac_auto);

/**
 * hdd_store_nss_chains_cfg_in_vdev() - Store the per vdev ini cfg in vdev_obj
 * @adapter: Current HDD adapter passed from caller
 *
 * This function will store the per vdev nss params to the particular mlme
 * vdev obj.
 *
 * Return: None
 */
void
hdd_store_nss_chains_cfg_in_vdev(struct hdd_adapter *adapter);

/**
 * wlan_hdd_disable_roaming() - disable roaming on all STAs except the input one
 * @cur_adapter: Current HDD adapter passed from caller
 * @rso_op_requestor: roam disable requestor
 *
 * This function loops through all adapters and disables roaming on each STA
 * mode adapter except the current adapter passed from the caller
 *
 * Return: None
 */
void
wlan_hdd_disable_roaming(struct hdd_adapter *cur_adapter,
			 enum wlan_cm_rso_control_requestor rso_op_requestor);

/**
 * wlan_hdd_enable_roaming() - enable roaming on all STAs except the input one
 * @cur_adapter: Current HDD adapter passed from caller
 * @rso_op_requestor: roam disable requestor
 *
 * This function loops through all adapters and enables roaming on each STA
 * mode adapter except the current adapter passed from the caller
 *
 * Return: None
 */
void
wlan_hdd_enable_roaming(struct hdd_adapter *cur_adapter,
			enum wlan_cm_rso_control_requestor rso_op_requestor);

QDF_STATUS hdd_post_cds_enable_config(struct hdd_context *hdd_ctx);

QDF_STATUS hdd_abort_mac_scan_all_adapters(struct hdd_context *hdd_ctx);

void wlan_hdd_stop_sap(struct hdd_adapter *ap_adapter);
void wlan_hdd_start_sap(struct hdd_adapter *ap_adapter, bool reinit);

#ifdef QCA_CONFIG_SMP
int wlan_hdd_get_cpu(void);
#else
static inline int wlan_hdd_get_cpu(void)
{
	return 0;
}
#endif

void wlan_hdd_sap_pre_cac_failure(void *data);
void hdd_clean_up_pre_cac_interface(struct hdd_context *hdd_ctx);

void wlan_hdd_txrx_pause_cb(uint8_t vdev_id,
	enum netif_action_type action, enum netif_reason_type reason);

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
void wlan_hdd_mod_fc_timer(struct hdd_adapter *adapter,
			   enum netif_action_type action);
#else
static inline void wlan_hdd_mod_fc_timer(struct hdd_adapter *adapter,
					 enum netif_action_type action)
{
}
#endif /* QCA_HL_NETDEV_FLOW_CONTROL */

/**
 * hdd_wlan_dump_stats() - display dump Stats
 * @adapter: adapter handle
 * @stats_id: stats id from user
 *
 * Return: 0 => success, error code on failure
 */
int hdd_wlan_dump_stats(struct hdd_adapter *adapter, int stats_id);

/**
 * hdd_wlan_clear_stats() - clear Stats
 * @adapter: adapter handle
 * @stats_id: stats id from user
 *
 * Return: 0 => success, error code on failure
 */
int hdd_wlan_clear_stats(struct hdd_adapter *adapter, int stats_id);

/**
 * wlan_hdd_display_tx_rx_histogram() - display tx rx histogram
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
void wlan_hdd_display_tx_rx_histogram(struct hdd_context *hdd_ctx);
void wlan_hdd_clear_tx_rx_histogram(struct hdd_context *hdd_ctx);

void
wlan_hdd_display_netif_queue_history(struct hdd_context *hdd_ctx,
				     enum qdf_stats_verbosity_level verb_lvl);

/**
 * wlan_hdd_display_adapter_netif_queue_history() - display adapter based netif
 * queue history
 * @adapter: hdd adapter
 *
 * Return: none
 */
void
wlan_hdd_display_adapter_netif_queue_history(struct hdd_adapter *adapter);

void wlan_hdd_clear_netif_queue_history(struct hdd_context *hdd_ctx);
const char *hdd_get_fwpath(void);
void hdd_indicate_mgmt_frame(tSirSmeMgmtFrameInd *frame_ind);

/**
 * hdd_get_adapter_by_iface_name() - Return adapter with given interface name
 * @hdd_ctx: hdd context.
 * @iface_name: interface name
 *
 * This function is used to get the adapter with given interface name
 *
 * Return: adapter pointer if found, NULL otherwise
 *
 */
struct hdd_adapter *hdd_get_adapter_by_iface_name(struct hdd_context *hdd_ctx,
					     const char *iface_name);
enum phy_ch_width hdd_map_nl_chan_width(enum nl80211_chan_width ch_width);

/**
 * hdd_nl_to_qdf_iface_type() - map nl80211_iftype to QDF_OPMODE
 * @nl_type: the input NL80211 interface type to map
 * @out_qdf_type: the output, equivalent QDF operating mode
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_nl_to_qdf_iface_type(enum nl80211_iftype nl_type,
				    enum QDF_OPMODE *out_qdf_type);

/**
 * wlan_hdd_find_opclass() - Find operating class for a channel
 * @mac_handle: global MAC handle
 * @channel: channel id
 * @bw_offset: bandwidth offset
 *
 * Function invokes sme api to find the operating class
 *
 * Return: operating class
 */
uint8_t wlan_hdd_find_opclass(mac_handle_t mac_handle, uint8_t channel,
			      uint8_t bw_offset);

int hdd_update_config(struct hdd_context *hdd_ctx);

/**
 * hdd_update_components_config() - Initialize driver per module ini parameters
 * @hdd_ctx: HDD Context
 *
 * API is used to initialize components configuration parameters
 * Return: 0 for success, errno for failure
 */
int hdd_update_components_config(struct hdd_context *hdd_ctx);

/**
 * hdd_chan_change_notify() - Function to notify hostapd about channel change
 * @hostapd_adapter:	hostapd adapter
 * @dev:		Net device structure
 * @chan_change:	New channel change parameters
 * @legacy_phymode:	is the phymode legacy
 *
 * This function is used to notify hostapd about the channel change
 *
 * Return: Success on intimating userspace
 *
 */
QDF_STATUS hdd_chan_change_notify(struct hdd_adapter *adapter,
		struct net_device *dev,
		struct hdd_chan_change_params chan_change,
		bool legacy_phymode);
int wlan_hdd_set_channel(struct wiphy *wiphy,
		struct net_device *dev,
		struct cfg80211_chan_def *chandef,
		enum nl80211_channel_type channel_type);
int wlan_hdd_cfg80211_start_bss(struct hdd_adapter *adapter,
		struct cfg80211_beacon_data *params,
		const u8 *ssid, size_t ssid_len,
		enum nl80211_hidden_ssid hidden_ssid,
		bool check_for_concurrency);

#if !defined(REMOVE_PKT_LOG)
int hdd_process_pktlog_command(struct hdd_context *hdd_ctx, uint32_t set_value,
			       int set_value2);
int hdd_pktlog_enable_disable(struct hdd_context *hdd_ctx, bool enable,
			      uint8_t user_triggered, int size);

#else
static inline
int hdd_pktlog_enable_disable(struct hdd_context *hdd_ctx, bool enable,
			      uint8_t user_triggered, int size)
{
	return 0;
}

static inline
int hdd_process_pktlog_command(struct hdd_context *hdd_ctx,
			       uint32_t set_value, int set_value2)
{
	return 0;
}
#endif /* REMOVE_PKT_LOG */

#if defined(FEATURE_SG) && !defined(CONFIG_HL_SUPPORT)
/**
 * hdd_set_sg_flags() - enable SG flag in the network device
 * @hdd_ctx: HDD context
 * @wlan_dev: network device structure
 *
 * This function enables the SG feature flag in the
 * given network device.
 *
 * Return: none
 */
static inline void hdd_set_sg_flags(struct hdd_context *hdd_ctx,
				struct net_device *wlan_dev)
{
	hdd_debug("SG Enabled");
	wlan_dev->features |= NETIF_F_SG;
}
#else
static inline void hdd_set_sg_flags(struct hdd_context *hdd_ctx,
				struct net_device *wlan_dev){}
#endif

/**
 * hdd_set_netdev_flags() - set netdev flags for adapter as per ini config
 * @adapter: hdd adapter context
 *
 * This function sets netdev feature flags for the adapter.
 *
 * Return: none
 */
void hdd_set_netdev_flags(struct hdd_adapter *adapter);

#ifdef FEATURE_TSO
/**
 * hdd_get_tso_csum_feature_flags() - Return TSO and csum flags if enabled
 *
 * Return: Enabled feature flags set, 0 on failure
 */
static inline netdev_features_t hdd_get_tso_csum_feature_flags(void)
{
	netdev_features_t netdev_features = 0;
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!soc) {
		hdd_err("soc handle is NULL");
		return 0;
	}

	if (cdp_cfg_get(soc, cfg_dp_enable_ip_tcp_udp_checksum_offload)) {
		netdev_features = NETIF_F_IP_CSUM | NETIF_F_IPV6_CSUM;

		if (cdp_cfg_get(soc, cfg_dp_tso_enable)) {
			/*
			 * Enable TSO only if IP/UDP/TCP TX checksum flag is
			 * enabled.
			 */
			netdev_features |= NETIF_F_TSO | NETIF_F_TSO6 |
					   NETIF_F_SG;
		}
	}
	return netdev_features;
}

/**
 * hdd_set_tso_flags() - enable TSO flags in the network device
 * @hdd_ctx: HDD context
 * @wlan_dev: network device structure
 *
 * This function enables the TSO related feature flags in the
 * given network device.
 *
 * Return: none
 */
static inline void hdd_set_tso_flags(struct hdd_context *hdd_ctx,
	 struct net_device *wlan_dev)
{
	hdd_debug("TSO Enabled");

	wlan_dev->features |= hdd_get_tso_csum_feature_flags();
}
#else
static inline void hdd_set_tso_flags(struct hdd_context *hdd_ctx,
	 struct net_device *wlan_dev)
{
	hdd_set_sg_flags(hdd_ctx, wlan_dev);
}

static inline netdev_features_t hdd_get_tso_csum_feature_flags(void)
{
	return 0;
}
#endif /* FEATURE_TSO */

/**
 * wlan_hdd_get_host_log_nl_proto() - Get host log netlink protocol
 * @hdd_ctx: HDD context
 *
 * This function returns with host log netlink protocol settings
 *
 * Return: none
 */
#ifdef WLAN_LOGGING_SOCK_SVC_ENABLE
static inline int wlan_hdd_get_host_log_nl_proto(struct hdd_context *hdd_ctx)
{
	return hdd_ctx->config->host_log_custom_nl_proto;
}
#else
static inline int wlan_hdd_get_host_log_nl_proto(struct hdd_context *hdd_ctx)
{
	return NETLINK_USERSOCK;
}
#endif

#ifdef CONFIG_CNSS_LOGGER
/**
 * wlan_hdd_nl_init() - wrapper function to CNSS_LOGGER case
 * @hdd_ctx:	the hdd context pointer
 *
 * The nl_srv_init() will call to cnss_logger_device_register() and
 * expect to get a radio_index from cnss_logger module and assign to
 * hdd_ctx->radio_index, then to maintain the consistency to original
 * design, adding the radio_index check here, then return the error
 * code if radio_index is not assigned correctly, which means the nl_init
 * from cnss_logger is failed.
 *
 * Return: 0 if successfully, otherwise error code
 */
static inline int wlan_hdd_nl_init(struct hdd_context *hdd_ctx)
{
	int proto;

	proto = wlan_hdd_get_host_log_nl_proto(hdd_ctx);
	hdd_ctx->radio_index = nl_srv_init(hdd_ctx->wiphy, proto);

	/* radio_index is assigned from 0, so only >=0 will be valid index  */
	if (hdd_ctx->radio_index >= 0)
		return 0;
	else
		return -EINVAL;
}
#else
/**
 * wlan_hdd_nl_init() - wrapper function to non CNSS_LOGGER case
 * @hdd_ctx:	the hdd context pointer
 *
 * In case of non CNSS_LOGGER case, the nl_srv_init() will initialize
 * the netlink socket and return the success or not.
 *
 * Return: the return value from  nl_srv_init()
 */
static inline int wlan_hdd_nl_init(struct hdd_context *hdd_ctx)
{
	int proto;

	proto = wlan_hdd_get_host_log_nl_proto(hdd_ctx);
	return nl_srv_init(hdd_ctx->wiphy, proto);
}
#endif
QDF_STATUS hdd_sme_open_session_callback(uint8_t vdev_id,
					 QDF_STATUS qdf_status);
QDF_STATUS hdd_sme_close_session_callback(uint8_t vdev_id);

/**
 * hdd_reassoc() - perform a userspace-directed reassoc
 * @adapter:    Adapter upon which the command was received
 * @bssid:      BSSID with which to reassociate
 * @ch_freq:    channel upon which to reassociate
 * @src:        The source for the trigger of this action
 *
 * This function performs a userspace-directed reassoc operation
 *
 * Return: 0 for success non-zero for failure
 */
int hdd_reassoc(struct hdd_adapter *adapter, const uint8_t *bssid,
		uint32_t ch_freq, const handoff_src src);

int hdd_register_cb(struct hdd_context *hdd_ctx);
void hdd_deregister_cb(struct hdd_context *hdd_ctx);
int hdd_start_station_adapter(struct hdd_adapter *adapter);
int hdd_start_ap_adapter(struct hdd_adapter *adapter);
int hdd_configure_cds(struct hdd_context *hdd_ctx);
int hdd_set_fw_params(struct hdd_adapter *adapter);

/**
 * hdd_wlan_start_modules() - Single driver state machine for starting modules
 * @hdd_ctx: HDD context
 * @reinit: flag to indicate from SSR or normal path
 *
 * This function maintains the driver state machine it will be invoked from
 * startup, reinit and change interface. Depending on the driver state shall
 * perform the opening of the modules.
 *
 * Return: Errno
 */
int hdd_wlan_start_modules(struct hdd_context *hdd_ctx, bool reinit);

/**
 * hdd_wlan_stop_modules - Single driver state machine for stoping modules
 * @hdd_ctx: HDD context
 * @ftm_mode: ftm mode
 *
 * This function maintains the driver state machine it will be invoked from
 * exit, shutdown and con_mode change handler. Depending on the driver state
 * shall perform the stopping/closing of the modules.
 *
 * Return: Errno
 */
int hdd_wlan_stop_modules(struct hdd_context *hdd_ctx, bool ftm_mode);

/**
 * hdd_psoc_idle_timer_start() - start the idle psoc detection timer
 * @hdd_ctx: the hdd context for which the timer should be started
 *
 * Return: None
 */
void hdd_psoc_idle_timer_start(struct hdd_context *hdd_ctx);

/**
 * hdd_psoc_idle_timer_stop() - stop the idle psoc detection timer
 * @hdd_ctx: the hdd context for which the timer should be stopped
 *
 * Return: None
 */
void hdd_psoc_idle_timer_stop(struct hdd_context *hdd_ctx);

/**
 * hdd_trigger_psoc_idle_restart() - trigger restart of a previously shutdown
 *                                   idle psoc, if needed
 * @hdd_ctx: the hdd context which should be restarted
 *
 * This API does nothing if the given psoc is already active.
 *
 * Return: Errno
 */
int hdd_trigger_psoc_idle_restart(struct hdd_context *hdd_ctx);

int hdd_start_adapter(struct hdd_adapter *adapter);
void hdd_populate_random_mac_addr(struct hdd_context *hdd_ctx, uint32_t num);
/**
 * hdd_is_interface_up()- Checkfor interface up before ssr
 * @hdd_ctx: HDD context
 *
 * check  if there are any wlan interfaces before SSR accordingly start
 * the interface.
 *
 * Return: 0 if interface was opened else false
 */
bool hdd_is_interface_up(struct hdd_adapter *adapter);

void hdd_connect_result(struct net_device *dev, const u8 *bssid,
			struct csr_roam_info *roam_info, const u8 *req_ie,
			size_t req_ie_len, const u8 *resp_ie,
			size_t resp_ie_len, u16 status, gfp_t gfp,
			bool connect_timeout,
			tSirResultCodes timeout_reason);

#ifdef WLAN_FEATURE_FASTPATH
void hdd_enable_fastpath(struct hdd_context *hdd_ctx,
			 void *context);
#else
static inline void hdd_enable_fastpath(struct hdd_context *hdd_ctx,
				       void *context)
{
}
#endif
void hdd_wlan_update_target_info(struct hdd_context *hdd_ctx, void *context);

enum  sap_acs_dfs_mode wlan_hdd_get_dfs_mode(enum dfs_mode mode);
void hdd_unsafe_channel_restart_sap(struct hdd_context *hdd_ctx);
/**
 * hdd_clone_local_unsafe_chan() - clone hdd ctx unsafe chan list
 * @hdd_ctx: hdd context pointer
 * @local_unsafe_list: copied unsafe chan list array
 * @local_unsafe_list_count: channel number in returned local_unsafe_list
 *
 * The function will allocate memory and make a copy the current unsafe
 * channels from hdd ctx. The caller need to free the local_unsafe_list
 * memory after use.
 *
 * Return: 0 if successfully clone unsafe chan list.
 */
int hdd_clone_local_unsafe_chan(struct hdd_context *hdd_ctx,
	uint16_t **local_unsafe_list, uint16_t *local_unsafe_list_count);

/**
 * hdd_local_unsafe_channel_updated() - check unsafe chan list same or not
 * @hdd_ctx: hdd context pointer
 * @local_unsafe_list: unsafe chan list to be compared with hdd_ctx's list
 * @local_unsafe_list_count: channel number in local_unsafe_list
 *
 * The function checked the input channel is same as current unsafe chan
 * list in hdd_ctx.
 *
 * Return: true if input channel list is same as the list in hdd_ctx
 */
bool hdd_local_unsafe_channel_updated(struct hdd_context *hdd_ctx,
	uint16_t *local_unsafe_list, uint16_t local_unsafe_list_count);

int hdd_enable_disable_ca_event(struct hdd_context *hddctx,
				uint8_t set_value);
void wlan_hdd_undo_acs(struct hdd_adapter *adapter);

#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0))
static inline int
hdd_wlan_nla_put_u64(struct sk_buff *skb, int attrtype, u64 value)
{
	return nla_put_u64(skb, attrtype, value);
}
#else
static inline int
hdd_wlan_nla_put_u64(struct sk_buff *skb, int attrtype, u64 value)
{
	return nla_put_u64_64bit(skb, attrtype, value, NL80211_ATTR_PAD);
}
#endif

/**
 * hdd_roam_profile() - Get adapter's roam profile
 * @adapter: The adapter being queried
 *
 * Given an adapter this function returns a pointer to its roam profile.
 *
 * NOTE WELL: Caller is responsible for ensuring this interface is only
 * invoked for STA-type interfaces
 *
 * Return: pointer to the adapter's roam profile
 */
static inline
struct csr_roam_profile *hdd_roam_profile(struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *sta_ctx;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	return &sta_ctx->roam_profile;
}

/**
 * hdd_security_ie() - Get adapter's security IE
 * @adapter: The adapter being queried
 *
 * Given an adapter this function returns a pointer to its security IE
 * buffer. Note that this buffer is maintained outside the roam
 * profile but, when in use, is referenced by a pointer within the
 * roam profile.
 *
 * NOTE WELL: Caller is responsible for ensuring this interface is only
 * invoked for STA-type interfaces
 *
 * Return: pointer to the adapter's roam profile security IE buffer
 */
static inline
uint8_t *hdd_security_ie(struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *sta_ctx;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	return sta_ctx->security_ie;
}

/**
 * hdd_assoc_additional_ie() - Get adapter's assoc additional IE
 * @adapter: The adapter being queried
 *
 * Given an adapter this function returns a pointer to its assoc
 * additional IE buffer. Note that this buffer is maintained outside
 * the roam profile but, when in use, is referenced by a pointer
 * within the roam profile.
 *
 * NOTE WELL: Caller is responsible for ensuring this interface is only
 * invoked for STA-type interfaces
 *
 * Return: pointer to the adapter's assoc additional IE buffer
 */
static inline
tSirAddie *hdd_assoc_additional_ie(struct hdd_adapter *adapter)
{
	struct hdd_station_ctx *sta_ctx;

	sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	return &sta_ctx->assoc_additional_ie;
}

/**
 * hdd_is_roaming_in_progress() - check if roaming is in progress
 * @hdd_ctx - Global HDD context
 *
 * Checks if roaming is in progress on any of the adapters
 *
 * Return: true if roaming is in progress else false
 */
bool hdd_is_roaming_in_progress(struct hdd_context *hdd_ctx);

/**
 * hdd_is_connection_in_progress() - check if connection is in progress
 * @out_vdev_id: id of vdev where connection is occurring
 * @out_reason: scan reject reason
 *
 * Go through each adapter and check if connection is in progress.
 * Output parameters @out_vdev_id and @out_reason will only be written
 * when a connection is in progress.
 *
 * Return: true if connection is in progress else false
 */
bool hdd_is_connection_in_progress(uint8_t *out_vdev_id,
				   enum scan_reject_states *out_reason);

void hdd_restart_sap(struct hdd_adapter *ap_adapter);
void hdd_check_and_restart_sap_with_non_dfs_acs(void);
bool hdd_set_connection_in_progress(bool value);

/**
 * wlan_hdd_init_chan_info() - initialize channel info variables
 * @hdd_ctx: hdd ctx
 *
 * This API initialize channel info variables
 *
 * Return: None
 */
void wlan_hdd_init_chan_info(struct hdd_context *hdd_ctx);

/**
 * wlan_hdd_deinit_chan_info() - deinitialize channel info variables
 * @hdd_ctx: hdd ctx
 *
 * This API deinitialize channel info variables
 *
 * Return: None
 */
void wlan_hdd_deinit_chan_info(struct hdd_context *hdd_ctx);
void wlan_hdd_start_sap(struct hdd_adapter *ap_adapter, bool reinit);

/**
 * hdd_is_any_interface_open() - Check for interface up
 * @hdd_ctx: HDD context
 *
 * Return: true if any interface is open
 */
bool hdd_is_any_interface_open(struct hdd_context *hdd_ctx);

#ifdef WIFI_POS_CONVERGED
/**
 * hdd_send_peer_status_ind_to_app() - wrapper to call legacy or new wifi_pos
 * function to send peer status to a registered application
 * @peer_mac: MAC address of peer
 * @peer_status: ePeerConnected or ePeerDisconnected
 * @peer_timing_meas_cap: 0: RTT/RTT2, 1: RTT3. Default is 0
 * @vdev_id: ID of the underlying vdev
 * @chan_info: operating channel information
 * @dev_mode: dev mode for which indication is sent
 *
 * Return: none
 */
static inline void hdd_send_peer_status_ind_to_app(
					struct qdf_mac_addr *peer_mac,
					uint8_t peer_status,
					uint8_t peer_timing_meas_cap,
					uint8_t vdev_id,
					struct oem_channel_info *chan_info,
					enum QDF_OPMODE dev_mode)
{
	struct wifi_pos_ch_info ch_info;

	if (!chan_info) {
		os_if_wifi_pos_send_peer_status(peer_mac, peer_status,
						peer_timing_meas_cap, vdev_id,
						NULL, dev_mode);
		return;
	}

	/* chan_id is obsoleted by mhz */
	ch_info.chan_id = 0;
	ch_info.mhz = chan_info->mhz;
	ch_info.band_center_freq1 = chan_info->band_center_freq1;
	ch_info.band_center_freq2 = chan_info->band_center_freq2;
	ch_info.info = chan_info->info;
	ch_info.reg_info_1 = chan_info->reg_info_1;
	ch_info.reg_info_2 = chan_info->reg_info_2;
	ch_info.nss = chan_info->nss;
	ch_info.rate_flags = chan_info->rate_flags;
	ch_info.sec_ch_offset = chan_info->sec_ch_offset;
	ch_info.ch_width = chan_info->ch_width;
	os_if_wifi_pos_send_peer_status(peer_mac, peer_status,
					peer_timing_meas_cap, vdev_id,
					&ch_info, dev_mode);
}
#else
static inline void hdd_send_peer_status_ind_to_app(
					struct qdf_mac_addr *peer_mac,
					uint8_t peer_status,
					uint8_t peer_timing_meas_cap,
					uint8_t vdev_id,
					struct oem_channel_info *chan_info,
					enum QDF_OPMODE dev_mode)
{
	hdd_send_peer_status_ind_to_oem_app(peer_mac, peer_status,
			peer_timing_meas_cap, vdev_id, chan_info, dev_mode);
}
#endif /* WIFI_POS_CONVERGENCE */

/**
 * wlan_hdd_send_p2p_quota()- Send P2P Quota value to FW
 * @adapter: Adapter data
 * @sval:    P2P quota value
 *
 * Send P2P quota value to FW
 *
 * Return: 0 success else failure
 */
int wlan_hdd_send_p2p_quota(struct hdd_adapter *adapter, int sval);

/**
 * wlan_hdd_send_p2p_quota()- Send MCC latency to FW
 * @adapter: Adapter data
 * @sval:    MCC latency value
 *
 * Send MCC latency value to FW
 *
 * Return: 0 success else failure
 */
int wlan_hdd_send_mcc_latency(struct hdd_adapter *adapter, int sval);

/**
 * wlan_hdd_get_adapter_from_vdev()- Get adapter from vdev id
 * and PSOC object data
 * @psoc: Psoc object data
 * @vdev_id: vdev id
 *
 * Get adapter from vdev id and PSOC object data
 *
 * Return: adapter pointer
 */
struct hdd_adapter *wlan_hdd_get_adapter_from_vdev(struct wlan_objmgr_psoc
					*psoc, uint8_t vdev_id);
/**
 * hdd_unregister_notifiers()- unregister kernel notifiers
 * @hdd_ctx: Hdd Context
 *
 * Unregister netdev notifiers like Netdevice,IPv4 and IPv6.
 *
 */
void hdd_unregister_notifiers(struct hdd_context *hdd_ctx);

/**
 * hdd_dbs_scan_selection_init() - initialization for DBS scan selection config
 * @hdd_ctx: HDD context
 *
 * This function sends the DBS scan selection config configuration to the
 * firmware via WMA
 *
 * Return: 0 - success, < 0 - failure
 */
int hdd_dbs_scan_selection_init(struct hdd_context *hdd_ctx);

/**
 * hdd_update_scan_config - API to update scan configuration parameters
 * @hdd_ctx: HDD context
 *
 * Return: 0 if success else err
 */
int hdd_update_scan_config(struct hdd_context *hdd_ctx);

/**
 * hdd_start_complete()- complete the start event
 * @ret: return value for complete event.
 *
 * complete the startup event and set the return in
 * global variable
 *
 * Return: void
 */

void hdd_start_complete(int ret);

/**
 * hdd_chip_pwr_save_fail_detected_cb() - chip power save failure detected
 * callback
 * @hdd_handle: HDD handle
 * @data: chip power save failure detected data
 *
 * This function reads the chip power save failure detected data and fill in
 * the skb with NL attributes and send up the NL event.
 * This callback execute in atomic context and must not invoke any
 * blocking calls.
 *
 * Return: none
 */

void hdd_chip_pwr_save_fail_detected_cb(hdd_handle_t hdd_handle,
				struct chip_pwr_save_fail_detected_params
				*data);

/**
 * hdd_update_ie_whitelist_attr() - Copy probe req ie whitelist attrs from cfg
 * @ie_whitelist: output parameter
 * @hdd_ctx: pointer to hdd context
 *
 * Return: None
 */
void hdd_update_ie_whitelist_attr(struct probe_req_whitelist_attr *ie_whitelist,
				  struct hdd_context *hdd_ctx);

/**
 * hdd_get_rssi_snr_by_bssid() - gets the rssi and snr by bssid from scan cache
 * @adapter: adapter handle
 * @bssid: bssid to look for in scan cache
 * @rssi: rssi value found
 * @snr: snr value found
 *
 * Return: QDF_STATUS
 */
int hdd_get_rssi_snr_by_bssid(struct hdd_adapter *adapter, const uint8_t *bssid,
			      int8_t *rssi, int8_t *snr);

/**
 * hdd_reset_limit_off_chan() - reset limit off-channel command parameters
 * @adapter - HDD adapter
 *
 * Return: 0 on success and non zero value on failure
 */
int hdd_reset_limit_off_chan(struct hdd_adapter *adapter);

#if defined(WLAN_FEATURE_FILS_SK) && \
	(defined(CFG80211_FILS_SK_OFFLOAD_SUPPORT) || \
		 (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 12, 0)))
/**
 * hdd_clear_fils_connection_info: API to clear fils info from roam profile and
 * free allocated memory
 * @adapter: pointer to hdd adapter
 *
 * Return: None
 */
void hdd_clear_fils_connection_info(struct hdd_adapter *adapter);

/**
 * hdd_update_hlp_info() - Update HLP packet received in FILS (re)assoc rsp
 * @dev: net device
 * @roam_fils_params: Fils join rsp params
 *
 * This API is used to send the received HLP packet in Assoc rsp(FILS AKM)
 * to the network layer.
 *
 * Return: None
 */
void hdd_update_hlp_info(struct net_device *dev,
			 struct csr_roam_info *roam_info);
#else
static inline void hdd_clear_fils_connection_info(struct hdd_adapter *adapter)
{ }
static inline void hdd_update_hlp_info(struct net_device *dev,
				       struct csr_roam_info *roam_info)
{}
#endif

#if LINUX_VERSION_CODE < KERNEL_VERSION(4, 12, 0)
static inline void hdd_dev_setup_destructor(struct net_device *dev)
{
	dev->destructor = free_netdev;
}
#else
static inline void hdd_dev_setup_destructor(struct net_device *dev)
{
	dev->needs_free_netdev = true;
}
#endif /* KERNEL_VERSION(4, 12, 0) */

/**
 * hdd_dp_trace_init() - initialize DP Trace by calling the QDF API
 * @config: hdd config
 *
 * Return: NONE
 */
#ifdef CONFIG_DP_TRACE
void hdd_dp_trace_init(struct hdd_config *config);
#else
static inline
void hdd_dp_trace_init(struct hdd_config *config) {}
#endif

/**
 * hdd_set_rx_mode_rps() - Enable/disable RPS in SAP mode
 * @enable: Set true to enable RPS in SAP mode
 *
 * Callback function registered with datapath
 *
 * Return: none
 */
void hdd_set_rx_mode_rps(bool enable);

/**
 * hdd_update_score_config - API to update candidate scoring related params
 * configuration parameters
 * @hdd_ctx: hdd context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_update_score_config(struct hdd_context *hdd_ctx);

/**
 * hdd_get_stainfo() - get stainfo for the specified peer
 * @astainfo: array of the station info in which the sta info
 * corresponding to mac_addr needs to be searched
 * @mac_addr: mac address of requested peer
 *
 * This function find the stainfo for the peer with mac_addr
 *
 * Return: stainfo if found, NULL if not found
 */
struct hdd_station_info *hdd_get_stainfo(struct hdd_station_info *astainfo,
					 struct qdf_mac_addr mac_addr);

/**
 * hdd_component_psoc_open() - Open the legacy components
 * @psoc: Pointer to psoc object
 *
 * This function opens the legacy components and initializes the
 * component's private objects.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_component_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * hdd_component_psoc_close() - Close the legacy components
 * @psoc: Pointer to psoc object
 *
 * This function closes the legacy components and resets the
 * component's private objects.
 *
 * Return: None
 */
void hdd_component_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * hdd_component_psoc_enable() - Trigger psoc enable for CLD Components
 *
 * Return: None
 */
void hdd_component_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * hdd_component_psoc_disable() - Trigger psoc disable for CLD Components
 *
 * Return: None
 */
void hdd_component_psoc_disable(struct wlan_objmgr_psoc *psoc);

/**
 * hdd_component_pdev_open() - Trigger pdev open for CLD Components
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_component_pdev_open(struct wlan_objmgr_pdev *pdev);

/**
 * hdd_component_pdev_close() - Trigger pdev close for CLD Components
 *
 * Return: None
 */
void hdd_component_pdev_close(struct wlan_objmgr_pdev *pdev);

#ifdef WLAN_FEATURE_MEMDUMP_ENABLE
int hdd_driver_memdump_init(void);
void hdd_driver_memdump_deinit(void);

/**
 * hdd_driver_mem_cleanup() - Frees memory allocated for
 * driver dump
 *
 * This function  frees driver dump memory.
 *
 * Return: None
 */
void hdd_driver_mem_cleanup(void);

#else /* WLAN_FEATURE_MEMDUMP_ENABLE */
static inline int hdd_driver_memdump_init(void)
{
	return 0;
}
static inline void hdd_driver_memdump_deinit(void)
{
}

static inline void hdd_driver_mem_cleanup(void)
{
}
#endif /* WLAN_FEATURE_MEMDUMP_ENABLE */
/**
 * hdd_set_disconnect_status() - set adapter disconnection status
 * @hdd_adapter: Pointer to hdd adapter
 * @disconnecting: Disconnect status to set
 *
 * Return: None
 */
void hdd_set_disconnect_status(struct hdd_adapter *adapter, bool disconnecting);

#ifdef FEATURE_MONITOR_MODE_SUPPORT
/**
 * wlan_hdd_set_mon_chan() - Set capture channel on the monitor mode interface.
 * @adapter: Handle to adapter
 * @freq: Monitor mode frequency (MHz)
 * @bandwidth: Capture channel bandwidth
 *
 * Return: 0 on success else error code.
 */
int wlan_hdd_set_mon_chan(struct hdd_adapter *adapter, qdf_freq_t freq,
			  uint32_t bandwidth);
#else
static inline
int wlan_hdd_set_mon_chan(struct hdd_adapter *adapter, qdf_freq_t freq,
			  uint32_t bandwidth)
{
	return 0;
}
#endif

/**
 * hdd_wlan_get_version() - Get version information
 * @hdd_ctx: Global HDD context
 * @version_len: length of the version buffer size
 * @version: the buffer to the version string
 *
 * This function is used to get Wlan Driver, Firmware, Hardware Version
 * & the Board related information.
 *
 * Return: the length of the version string
 */
uint32_t hdd_wlan_get_version(struct hdd_context *hdd_ctx,
			      const size_t version_len, uint8_t *version);
/**
 * hdd_assemble_rate_code() - assemble rate code to be sent to FW
 * @preamble: rate preamble
 * @nss: number of streams
 * @rate: rate index
 *
 * Rate code assembling is different for targets which are 11ax capable.
 * Check for the target support and assemble the rate code accordingly.
 *
 * Return: assembled rate code
 */
int hdd_assemble_rate_code(uint8_t preamble, uint8_t nss, uint8_t rate);

/**
 * hdd_set_11ax_rate() - set 11ax rate
 * @adapter: adapter being modified
 * @value: new 11ax rate code
 * @sap_config: pointer to SAP config to check HW mode
 *		this will be NULL for call from STA persona
 *
 * Return: 0 on success, negative errno on failure
 */
int hdd_set_11ax_rate(struct hdd_adapter *adapter, int value,
		      struct sap_config *sap_config);

/**
 * hdd_update_hw_sw_info() - API to update the HW/SW information
 * @hdd_ctx: Global HDD context
 *
 * API to update the HW and SW information in the driver
 *
 * Note:
 * All the version/revision information would only be retrieved after
 * firmware download
 *
 * Return: None
 */
void hdd_update_hw_sw_info(struct hdd_context *hdd_ctx);

/**
 * hdd_get_nud_stats_cb() - callback api to update the stats received from FW
 * @data: pointer to hdd context.
 * @rsp: pointer to data received from FW.
 * @context: callback context
 *
 * This is called when wlan driver received response event for
 * get arp stats to firmware.
 *
 * Return: None
 */
void hdd_get_nud_stats_cb(void *data, struct rsp_stats *rsp, void *context);

/**
 * hdd_context_get_mac_handle() - get mac handle from hdd context
 * @hdd_ctx: Global HDD context pointer
 *
 * Retrieves the global MAC handle from the HDD context
 *
 * Return: The global MAC handle (which may be NULL)
 */
static inline
mac_handle_t hdd_context_get_mac_handle(struct hdd_context *hdd_ctx)
{
	return hdd_ctx ? hdd_ctx->mac_handle : NULL;
}

/**
 * hdd_adapter_get_mac_handle() - get mac handle from hdd adapter
 * @adapter: HDD adapter pointer
 *
 * Retrieves the global MAC handle given an HDD adapter
 *
 * Return: The global MAC handle (which may be NULL)
 */
static inline
mac_handle_t hdd_adapter_get_mac_handle(struct hdd_adapter *adapter)
{
	return adapter ?
		hdd_context_get_mac_handle(adapter->hdd_ctx) : NULL;
}

/**
 * hdd_handle_to_context() - turn an HDD handle into an HDD context
 * @hdd_handle: HDD handle to be converted
 *
 * Return: HDD context referenced by @hdd_handle
 */
static inline
struct hdd_context *hdd_handle_to_context(hdd_handle_t hdd_handle)
{
	return (struct hdd_context *)hdd_handle;
}

/**
 * wlan_hdd_free_cache_channels() - Free the cache channels list
 * @hdd_ctx: Pointer to HDD context
 *
 * Return: None
 */
void wlan_hdd_free_cache_channels(struct hdd_context *hdd_ctx);

/**
 * hdd_update_dynamic_mac() - Updates the dynamic MAC list
 * @hdd_ctx: Pointer to HDD context
 * @curr_mac_addr: Current interface mac address
 * @new_mac_addr: New mac address which needs to be updated
 *
 * This function updates newly configured MAC address to the
 * dynamic MAC address list corresponding to the current
 * adapter MAC address
 *
 * Return: None
 */
void hdd_update_dynamic_mac(struct hdd_context *hdd_ctx,
			    struct qdf_mac_addr *curr_mac_addr,
			    struct qdf_mac_addr *new_mac_addr);

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
/**
 * wlan_hdd_send_tcp_param_update_event() - Send vendor event to update
 * TCP parameter through Wi-Fi HAL
 * @hdd_ctx: Pointer to HDD context
 * @data: Parameters to update
 * @dir: Direction(tx/rx) to update
 *
 * Return: None
 */
void wlan_hdd_send_tcp_param_update_event(struct hdd_context *hdd_ctx,
					  void *data,
					  uint8_t dir);

/**
 * wlan_hdd_update_tcp_rx_param() - update TCP param in RX dir
 * @hdd_ctx: Pointer to HDD context
 * @data: Parameters to update
 *
 * Return: None
 */
void wlan_hdd_update_tcp_rx_param(struct hdd_context *hdd_ctx, void *data);

/**
 * wlan_hdd_update_tcp_tx_param() - update TCP param in TX dir
 * @hdd_ctx: Pointer to HDD context
 * @data: Parameters to update
 *
 * Return: None
 */
void wlan_hdd_update_tcp_tx_param(struct hdd_context *hdd_ctx, void *data);
#else
static inline
void wlan_hdd_update_tcp_rx_param(struct hdd_context *hdd_ctx, void *data)
{
}

static inline
void wlan_hdd_update_tcp_tx_param(struct hdd_context *hdd_ctx, void *data)
{
}

static inline
void wlan_hdd_send_tcp_param_update_event(struct hdd_context *hdd_ctx,
					  void *data,
					  uint8_t dir)
{
}
#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/

#ifdef WLAN_FEATURE_MOTION_DETECTION
/**
 * hdd_md_host_evt_cb - Callback for Motion Detection Event
 * @ctx: HDD context
 * @sir_md_evt: motion detect event
 *
 * Callback for Motion Detection Event. Re-enables Motion
 * Detection again upon event
 *
 * Return: QDF_STATUS QDF_STATUS_SUCCESS on Success and
 * QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS hdd_md_host_evt_cb(void *ctx, struct sir_md_evt *event);

/**
 * hdd_md_bl_evt_cb - Callback for Motion Detection Baseline Event
 * @ctx: HDD context
 * @sir_md_bl_evt: motion detect baseline event
 *
 * Callback for Motion Detection Baseline Event
 *
 * Return: QDF_STATUS QDF_STATUS_SUCCESS on Success and
 * QDF_STATUS_E_FAILURE on failure
 */
QDF_STATUS hdd_md_bl_evt_cb(void *ctx, struct sir_md_bl_evt *event);
#endif /* WLAN_FEATURE_MOTION_DETECTION */

/**
 * hdd_hidden_ssid_enable_roaming() - enable roaming after hidden ssid rsp
 * @hdd_handle: Hdd handler
 * @vdev_id: Vdev Id
 *
 * This is a wrapper function to enable roaming after getting hidden
 * ssid rsp
 */
void hdd_hidden_ssid_enable_roaming(hdd_handle_t hdd_handle, uint8_t vdev_id);

/**
 * hdd_psoc_idle_shutdown - perform idle shutdown after interface inactivity
 *                          timeout
 * @device: pointer to struct device
 *
 * Return: 0 for success non-zero error code for failure
 */
int hdd_psoc_idle_shutdown(struct device *dev);

/**
 * hdd_psoc_idle_restart - perform idle restart after idle shutdown
 * @device: pointer to struct device
 *
 * Return: 0 for success non-zero error code for failure
 */
int hdd_psoc_idle_restart(struct device *dev);

/**
 * hdd_common_roam_callback() - common sme roam callback
 * @psoc: Object Manager Psoc
 * @session_id: session id for which callback is called
 * @roam_info: pointer to roam info
 * @roam_status: roam status
 * @roam_result: roam result
 *
 * Return: QDF_STATUS enumeration
 */
QDF_STATUS hdd_common_roam_callback(struct wlan_objmgr_psoc *psoc,
				    uint8_t session_id,
				    struct csr_roam_info *roam_info,
				    uint32_t roam_id,
				    eRoamCmdStatus roam_status,
				    eCsrRoamResult roam_result);

#ifdef WLAN_FEATURE_PKT_CAPTURE
/**
 * wlan_hdd_is_mon_concurrency() - check if MONITOR and STA concurrency
 * is UP when packet capture mode is enabled.
 *
 * Return: True - if STA and monitor concurrency is there, else False
 *
 */
bool wlan_hdd_is_mon_concurrency(void);

/**
 * wlan_hdd_del_monitor() - delete monitor interface
 * @hdd_ctx: pointer to hdd context
 * @adapter: adapter to be deleted
 * @rtnl_held: rtnl lock held
 *
 * This function is invoked to delete monitor interface.
 *
 * Return: None
 */
void wlan_hdd_del_monitor(struct hdd_context *hdd_ctx,
			  struct hdd_adapter *adapter, bool rtnl_held);

/**
 * wlan_hdd_del_p2p_interface() - delete p2p interface
 * @hdd_ctx: pointer to hdd context
 *
 * This function is invoked to delete p2p interface.
 *
 * Return: None
 */
void
wlan_hdd_del_p2p_interface(struct hdd_context *hdd_ctx);

/**
 * hdd_reset_monitor_interface() - reset monitor interface flags
 * @sta_adapter: station adapter
 *
 * Return: void
 */
void hdd_reset_monitor_interface(struct hdd_adapter *sta_adapter);

/**
 * hdd_is_pkt_capture_mon_enable() - Is packet capture monitor mode enable
 * @sta_adapter: station adapter
 *
 * Return: status of packet capture monitor adapter
 */
struct hdd_adapter *
hdd_is_pkt_capture_mon_enable(struct hdd_adapter *sta_adapter);
#else
static inline
void wlan_hdd_del_monitor(struct hdd_context *hdd_ctx,
			  struct hdd_adapter *adapter, bool rtnl_held)
{
}

static inline
bool wlan_hdd_is_mon_concurrency(void)
{
	return false;
}

static inline
void wlan_hdd_del_p2p_interface(struct hdd_context *hdd_ctx)
{
}

static inline void hdd_reset_monitor_interface(struct hdd_adapter *sta_adapter)
{
}

static inline int hdd_is_pkt_capture_mon_enable(struct hdd_adapter *adapter)
{
	return 0;
}
#endif /* WLAN_FEATURE_PKT_CAPTURE */
/**
 * wlan_hdd_is_session_type_monitor() - check if session type is MONITOR
 * @session_type: session type
 *
 * Return: True - if session type for adapter is monitor, else False
 *
 */
bool wlan_hdd_is_session_type_monitor(uint8_t session_type);

/**
 * wlan_hdd_add_monitor_check() - check for monitor intf and add if needed
 * @hdd_ctx: pointer to hdd context
 * @adapter: output pointer to hold created monitor adapter
 * @name: name of the interface
 * @rtnl_held: True if RTNL lock is held
 * @name_assign_type: the name of assign type of the netdev
 *
 * Return: 0 - on success
 *         err code - on failure
 */
int wlan_hdd_add_monitor_check(struct hdd_context *hdd_ctx,
			       struct hdd_adapter **adapter,
			       const char *name, bool rtnl_held,
			       unsigned char name_assign_type);

#ifdef CONFIG_WLAN_DEBUG_CRASH_INJECT
/**
 * hdd_crash_inject() - Inject a crash
 * @adapter: Adapter upon which the command was received
 * @v1: first value to inject
 * @v2: second value to inject
 *
 * This function is the handler for the crash inject debug feature.
 * This feature only exists for internal testing and must not be
 * enabled on a production device.
 *
 * Return: 0 on success and errno on failure
 */
int hdd_crash_inject(struct hdd_adapter *adapter, uint32_t v1, uint32_t v2);
#else
static inline
int hdd_crash_inject(struct hdd_adapter *adapter, uint32_t v1, uint32_t v2)
{
	return -ENOTSUPP;
}
#endif

#ifdef FEATURE_MONITOR_MODE_SUPPORT

void hdd_sme_monitor_mode_callback(uint8_t vdev_id);

QDF_STATUS hdd_monitor_mode_vdev_status(struct hdd_adapter *adapter);

QDF_STATUS hdd_monitor_mode_qdf_create_event(struct hdd_adapter *adapter,
					     uint8_t session_type);
#else
static inline void hdd_sme_monitor_mode_callback(uint8_t vdev_id) {}

static inline QDF_STATUS
hdd_monitor_mode_vdev_status(struct hdd_adapter *adapter)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
hdd_monitor_mode_qdf_create_event(struct hdd_adapter *adapter,
				  uint8_t session_type)
{
	return QDF_STATUS_SUCCESS;
}
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 19, 0)) && \
     defined(WLAN_FEATURE_11AX)
/**
 * hdd_cleanup_conn_info() - Cleanup connectin info
 * @adapter: Adapter upon which the command was received
 *
 * This function frees the memory allocated for the connection
 * info structure
 *
 * Return: none
 */
void hdd_cleanup_conn_info(struct hdd_adapter *adapter);
/**
 * hdd_sta_destroy_ctx_all() - cleanup all station contexts
 * @hdd_ctx: Global HDD context
 *
 * This function destroys all the station contexts
 *
 * Return: none
 */
void hdd_sta_destroy_ctx_all(struct hdd_context *hdd_ctx);
#else
static inline void hdd_cleanup_conn_info(struct hdd_adapter *adapter)
{
}
static inline void hdd_sta_destroy_ctx_all(struct hdd_context *hdd_ctx)
{
}
#endif

#ifdef FEATURE_WLAN_RESIDENT_DRIVER
extern char *country_code;
extern int con_mode;
extern const struct kernel_param_ops con_mode_ops;
extern int con_mode_ftm;
extern const struct kernel_param_ops con_mode_ftm_ops;
#endif

/**
 * hdd_driver_load() - Perform the driver-level load operation
 *
 * Note: this is used in both static and DLKM driver builds
 *
 * Return: Errno
 */
int hdd_driver_load(void);

/**
 * hdd_driver_unload() - Performs the driver-level unload operation
 *
 * Note: this is used in both static and DLKM driver builds
 *
 * Return: None
 */
void hdd_driver_unload(void);

/**
 * hdd_init_start_completion() - Init the completion variable to wait on ON/OFF
 *
 * Return: None
 */
void hdd_init_start_completion(void);

/**
 * hdd_max_sta_vdev_count_reached() - check sta vdev count
 * @hdd_ctx: global hdd context
 *
 * Return: true if vdev limit reached
 */
bool hdd_max_sta_vdev_count_reached(struct hdd_context *hdd_ctx);

#if defined(CLD_PM_QOS) && defined(WLAN_FEATURE_LL_MODE)
/**
 * hdd_beacon_latency_event_cb() - Callback function to get latency level
 * @latency_level: latency level received from firmware
 *
 * Return: None
 */
void hdd_beacon_latency_event_cb(uint32_t latency_level);
#else
static inline void hdd_beacon_latency_event_cb(uint32_t latency_level)
{
}
#endif

#if defined(CLD_PM_QOS) || defined(FEATURE_RUNTIME_PM)
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0))
/**
 * wlan_hdd_get_pm_qos_cpu_latency() - get PM QOS CPU latency
 *
 * Return: PM QOS CPU latency value
 */
static inline unsigned long wlan_hdd_get_pm_qos_cpu_latency(void)
{
	return PM_QOS_CPU_LATENCY_DEFAULT_VALUE;
}
#else
static inline unsigned long wlan_hdd_get_pm_qos_cpu_latency(void)
{
	return PM_QOS_CPU_DMA_LAT_DEFAULT_VALUE;
}
#endif /* (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 7, 0) */
#endif /* defined(CLD_PM_QOS) || defined(FEATURE_RUNTIME_PM) */

/**
 * hdd_is_runtime_pm_enabled - if runtime pm enabled
 * @hdd_ctx: hdd context
 *
 * Return: true if runtime pm enabled. false if disabled.
 */
bool hdd_is_runtime_pm_enabled(struct hdd_context *hdd_ctx);

/**
 * hdd_netdev_feature_update - Update the netdev features
 * @net_dev: Handle to net_device
 *
 * This func holds the rtnl_lock. Do not call with rtnl_lock held.
 *
 * Return: None
 */
void hdd_netdev_update_features(struct hdd_adapter *adapter);

/**
 * hdd_stop_no_trans() - HDD stop function
 * @dev:	Pointer to net_device structure
 *
 * This is called in response to ifconfig down. Vdev sync transaction
 * should be started before calling this API.
 *
 * Return: 0 for success; non-zero for failure
 */
int hdd_stop_no_trans(struct net_device *dev);

#if defined(CLD_PM_QOS)
/**
 * wlan_hdd_set_pm_qos_request() - Function to set pm_qos config in wlm mode
 * @hdd_ctx: HDD context
 * @pm_qos_request: pm_qos_request flag
 *
 * Return: None
 */
void wlan_hdd_set_pm_qos_request(struct hdd_context *hdd_ctx,
				 bool pm_qos_request);
#else
static inline
void wlan_hdd_set_pm_qos_request(struct hdd_context *hdd_ctx,
				 bool pm_qos_request)
{
}
#endif
#endif /* end #if !defined(WLAN_HDD_MAIN_H) */
