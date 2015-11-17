/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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

  \file     wma.c
  \brief    Implementation of WMA

  ========================================================================*/
/**=========================================================================
  EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.

  $Header:$   $DateTime: $ $Author: $


  when              who           what, where, why
  --------          ---           -----------------------------------------
  12/03/2013        Ganesh        Implementation of WMA APIs.
                    Kondabattini
  27/03/2013        Ganesh        Rx Management Support added
                    Babu
  ==========================================================================*/

/* ################ Header files ################ */
#include "wma.h"
#include "wma_api.h"
#include "vos_api.h"
#include "wmi_unified_api.h"
#include "wlan_qct_sys.h"
#include "wniApi.h"
#include "aniGlobal.h"
#include "wmi_unified.h"
#include "wniCfgAp.h"
#include "cfgApi.h"
#include "ol_txrx_ctrl_api.h"
#if defined(CONFIG_HL_SUPPORT)
#include "wlan_tgt_def_config_hl.h"
#else
#include "wlan_tgt_def_config.h"
#endif

#if defined(QCA_IBSS_SUPPORT)
#include "wlan_hdd_assoc.h"
#endif

#include "adf_nbuf.h"
#include "adf_os_types.h"
#include "ol_txrx_api.h"
#include "vos_memory.h"
#include "ol_txrx_types.h"
#include "ol_txrx_peer_find.h"

#include "wlan_qct_wda.h"
#include "wlan_qct_wda_msg.h"
#include "limApi.h"
#include "limSessionUtils.h"

#include "wdi_out.h"
#include "wdi_in.h"

#include "vos_utils.h"
#include "tl_shim.h"
#if defined(QCA_WIFI_FTM)
#include "testmode.h"
#endif

#if !defined(REMOVE_PKT_LOG)
#include "pktlog_ac.h"
#endif

#include "dbglog_host.h"
/* FIXME: Inclusion of .c looks odd but this is how it is in internal codebase */
#include "wmi_version_whitelist.c"
#include "csrApi.h"
#include "ol_fw.h"

#include "dfs.h"
#include "radar_filters.h"
#include "regdomain_common.h"
/* ################### defines ################### */
/*
 * TODO: Following constant should be shared by firwmare in
 * wmi_unified.h. This will be done once wmi_unified.h is updated.
 */
#define WMI_PEER_STATE_AUTHORIZED 0x2

#define WMA_2_4_GHZ_MAX_FREQ  3000
#define WOW_CSA_EVENT_OFFSET 12

#define WMA_DEFAULT_SCAN_REQUESTER_ID        1
#define WMI_SCAN_FINISH_EVENTS (WMI_SCAN_EVENT_START_FAILED |\
                                WMI_SCAN_EVENT_COMPLETED |\
                                WMI_SCAN_EVENT_DEQUEUED)
/* default value */
#define DEFAULT_INFRA_STA_KEEP_ALIVE_PERIOD  20
/* pdev vdev and peer stats*/
#define FW_PDEV_STATS_SET 0x1
#define FW_VDEV_STATS_SET 0x2
#define FW_PEER_STATS_SET 0x4
#define FW_STATS_SET 0x7
/*AR9888/AR6320  noise floor approx value
 * similar to the mentioned the TLSHIM
 */
#define WMA_TGT_NOISE_FLOOR_DBM (-96)

/*
 * Make sure that link monitor and keep alive
 * default values should be in sync with CFG.
 */
#define WMA_LINK_MONITOR_DEFAULT_TIME_SECS 10
#define WMA_KEEP_ALIVE_DEFAULT_TIME_SECS   5

#define AGC_DUMP  1
#define CHAN_DUMP 2
#define WD_DUMP   3
#ifdef CONFIG_ATH_PCIE_ACCESS_DEBUG
#define PCIE_DUMP 4
#endif

/* conformance test limits */
#define FCC       0x10
#define MKK       0x40
#define ETSI      0x30

/* Maximum Buffer length allowed for DFS phyerrors */
#define DFS_MAX_BUF_LENGHT 4096

#define WMI_DEFAULT_NOISE_FLOOR_DBM (-96)

#define WMI_MCC_MIN_CHANNEL_QUOTA             20
#define WMI_MCC_MAX_CHANNEL_QUOTA             80
#define WMI_MCC_MIN_NON_ZERO_CHANNEL_LATENCY  30

/* The maximum number of patterns that can be transmitted by the firmware
 *  and maximum patterns size.
 */
#define WMA_MAXNUM_PERIODIC_TX_PTRNS 6

#define WMI_MAX_HOST_CREDITS 2
#define WMI_WOW_REQUIRED_CREDITS 1

#ifdef FEATURE_WLAN_D0WOW
#define DISABLE_PCIE_POWER_COLLAPSE 1
#define ENABLE_PCIE_POWER_COLLAPSE  0
#endif

#define MAX_HT_MCS_IDX 8
#define MAX_VHT_MCS_IDX 10
#define INVALID_MCS_IDX 255

#define LINK_STATUS_LEGACY	0
#define LINK_STATUS_VHT		0x1
#define LINK_STATUS_MIMO	0x2
#define LINK_SUPPORT_VHT	0x4
#define LINK_SUPPORT_MIMO	0x8

#define LINK_RATE_VHT		0x3

#define WMA_LOG_COMPLETION_TIMER 10000 /* 10 seconds */

#ifdef FEATURE_WLAN_SCAN_PNO
static int wma_nlo_scan_cmp_evt_handler(void *handle, u_int8_t *event,
					u_int32_t len);
#endif

#ifdef FEATURE_WLAN_EXTSCAN
/**
 * enum extscan_report_events_type - extscan report events type
 * @EXTSCAN_REPORT_EVENTS_BUFFER_FULL: report only when scan history is % full
 * @EXTSCAN_REPORT_EVENTS_EACH_SCAN: report a scan completion event after scan
 * @EXTSCAN_REPORT_EVENTS_FULL_RESULTS: forward scan results
 *		(beacons/probe responses + IEs)
 *		in real time to HAL, in addition to completion events.
 *		Note: To keep backward compatibility,
 *		fire completion events regardless of REPORT_EVENTS_EACH_SCAN.
 * @EXTSCAN_REPORT_EVENTS_NO_BATCH: controls batching,
 *		0 => batching, 1 => no batching
 */
enum extscan_report_events_type {
	EXTSCAN_REPORT_EVENTS_BUFFER_FULL   = 0x00,
	EXTSCAN_REPORT_EVENTS_EACH_SCAN     = 0x01,
	EXTSCAN_REPORT_EVENTS_FULL_RESULTS  = 0x02,
	EXTSCAN_REPORT_EVENTS_NO_BATCH      = 0x04,
};

#define WMA_EXTSCAN_CYCLE_WAKE_LOCK_DURATION    (5 * 1000) /* in msec */

#endif

/* Data rate 100KBPS based on IE Index */
struct index_data_rate_type
{
	v_U8_t   beacon_rate_index;
	v_U16_t  supported_rate[4];
};

#ifdef WLAN_FEATURE_11AC
struct index_vht_data_rate_type
{
	v_U8_t   beacon_rate_index;
	v_U16_t  supported_VHT80_rate[2];
	v_U16_t  supported_VHT40_rate[2];
	v_U16_t  supported_VHT20_rate[2];
};
#endif

/* MCS Based rate table */
/* HT MCS parameters with Nss = 1 */
static struct index_data_rate_type supported_mcs_rate_nss1[] =
{
	/* MCS  L20   L40   S20  S40 */
	{0,  {65,  135,  72,  150}},
	{1,  {130, 270,  144, 300}},
	{2,  {195, 405,  217, 450}},
	{3,  {260, 540,  289, 600}},
	{4,  {390, 810,  433, 900}},
	{5,  {520, 1080, 578, 1200}},
	{6,  {585, 1215, 650, 1350}},
	{7,  {650, 1350, 722, 1500}}
};
/* HT MCS parameters with Nss = 2 */
static struct index_data_rate_type supported_mcs_rate_nss2[] =
{
	/* MCS  L20    L40   S20   S40 */
	{0,  {130,  270,  144,  300}},
	{1,  {260,  540,  289,  600}},
	{2,  {390,  810,  433,  900}},
	{3,  {520,  1080, 578,  1200}},
	{4,  {780,  1620, 867,  1800}},
	{5,  {1040, 2160, 1156, 2400}},
	{6,  {1170, 2430, 1300, 2700}},
	{7,  {1300, 2700, 1444, 3000}}
};

#ifdef WLAN_FEATURE_11AC
/* MCS Based VHT rate table */
/* MCS parameters with Nss = 1*/
static struct index_vht_data_rate_type supported_vht_mcs_rate_nss1[] =
{
	/* MCS  L80    S80     L40   S40    L20   S40*/
	{0,  {293,  325},  {135,  150},  {65,   72}},
	{1,  {585,  650},  {270,  300},  {130,  144}},
	{2,  {878,  975},  {405,  450},  {195,  217}},
	{3,  {1170, 1300}, {540,  600},  {260,  289}},
	{4,  {1755, 1950}, {810,  900},  {390,  433}},
	{5,  {2340, 2600}, {1080, 1200}, {520,  578}},
	{6,  {2633, 2925}, {1215, 1350}, {585,  650}},
	{7,  {2925, 3250}, {1350, 1500}, {650,  722}},
	{8,  {3510, 3900}, {1620, 1800}, {780,  867}},
	{9,  {3900, 4333}, {1800, 2000}, {780,  867}}
};

/*MCS parameters with Nss = 2*/
static struct index_vht_data_rate_type supported_vht_mcs_rate_nss2[] =
{
	/* MCS  L80    S80     L40   S40    L20   S40*/
	{0,  {585,  650},  {270,  300},  {130,  144}},
	{1,  {1170, 1300}, {540,  600},  {260,  289}},
	{2,  {1755, 1950}, {810,  900},  {390,  433}},
	{3,  {2340, 2600}, {1080, 1200}, {520,  578}},
	{4,  {3510, 3900}, {1620, 1800}, {780,  867}},
	{5,  {4680, 5200}, {2160, 2400}, {1040, 1156}},
	{6,  {5265, 5850}, {2430, 2700}, {1170, 1300}},
	{7,  {5850, 6500}, {2700, 3000}, {1300, 1444}},
	{8,  {7020, 7800}, {3240, 3600}, {1560, 1733}},
	{9,  {7800, 8667}, {3600, 4000}, {1560, 1733}}
};
#endif

static void wma_send_msg(tp_wma_handle wma_handle, u_int16_t msg_type,
			 void *body_ptr, u_int32_t body_val);

#ifdef QCA_IBSS_SUPPORT
static void wma_data_tx_ack_comp_hdlr(void *wma_context,
                                      adf_nbuf_t netbuf,
                                      int32_t status);
#endif
static VOS_STATUS wma_vdev_detach(tp_wma_handle wma_handle,
                                            tpDelStaSelfParams pdel_sta_self_req_param,
                                            u_int8_t generateRsp);
static struct wma_target_req *
wma_fill_vdev_req(tp_wma_handle wma, u_int8_t vdev_id,
		  u_int32_t msg_type, u_int8_t type, void *params,
		  u_int32_t timeout);
static int32_t wmi_unified_vdev_stop_send(wmi_unified_t wmi, u_int8_t vdev_id);
static void wma_remove_vdev_req(tp_wma_handle wma, u_int8_t vdev_id,
				u_int8_t type);

static tANI_U32 gFwWlanFeatCaps;

static eHalStatus wma_set_ppsconfig(tANI_U8 vdev_id, tANI_U16 pps_param,
                                    int value);
static eHalStatus wma_set_mimops(tp_wma_handle wma_handle,
                                 tANI_U8 vdev_id, int value);
#ifdef FEATURE_WLAN_TDLS
static int wma_update_fw_tdls_state(WMA_HANDLE handle, void *pwmaTdlsparams);
static int wma_update_tdls_peer_state(WMA_HANDLE handle,
               tTdlsPeerStateParams *peerStateParams);
static int wma_set_tdls_offchan_mode(WMA_HANDLE wma_handle,
                                     tTdlsChanSwitchParams *pChanSwitchParams);
#endif

static eHalStatus wma_set_smps_params(tp_wma_handle wma_handle,
                                 tANI_U8 vdev_id, int value);
#if defined(QCA_WIFI_FTM)
void wma_utf_attach(tp_wma_handle wma_handle);
void wma_utf_detach(tp_wma_handle wma_handle);
static VOS_STATUS
wma_process_ftm_command(tp_wma_handle wma_handle,
			struct ar6k_testmode_cmd_data *msg_buffer);
#endif

static VOS_STATUS wma_create_peer(tp_wma_handle wma, ol_txrx_pdev_handle pdev,
		ol_txrx_vdev_handle vdev, u8 peer_addr[6],
		u_int32_t peer_type, u_int8_t vdev_id,
		v_BOOL_t roam_synch_in_progress);
static ol_txrx_vdev_handle wma_vdev_attach(tp_wma_handle wma_handle,
		tpAddStaSelfParams self_sta_req,
		u_int8_t generateRsp);
static void wma_set_bsskey(tp_wma_handle wma_handle, tpSetBssKeyParams key_info);

/*DFS Attach*/
struct ieee80211com* wma_dfs_attach(struct ieee80211com *ic);
static void wma_dfs_detach(struct ieee80211com *ic);
static void wma_set_bss_rate_flags(struct wma_txrx_node *iface,
							tpAddBssParams add_bss);
/*Configure DFS with radar tables and regulatory domain*/
void wma_dfs_configure(struct ieee80211com *ic);

/*Configure the current channel with the DFS*/
struct ieee80211_channel *
wma_dfs_configure_channel(struct ieee80211com *dfs_ic,
                          wmi_channel *chan,
                          WLAN_PHY_MODE chanmode,
                          struct wma_vdev_start_req *req);

/* VDEV UP */
static int
wmi_unified_vdev_up_send(wmi_unified_t wmi,
                  u_int8_t vdev_id, u_int16_t aid,
                  u_int8_t bssid[IEEE80211_ADDR_LEN]);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
void wma_process_roam_synch_complete(WMA_HANDLE handle,
		tSirSmeRoamOffloadSynchCnf *synchcnf);
void wma_process_roam_synch_fail(WMA_HANDLE handle,
		tSirRoamOffloadSynchFail *synchfail);
#endif
/* Configure the regulatory domain for DFS radar filter initialization*/
void wma_set_dfs_regdomain(tp_wma_handle wma);

static VOS_STATUS wma_set_thermal_mgmt(tp_wma_handle wma_handle,
				t_thermal_cmd_params thermal_info);

#ifdef FEATURE_WLAN_CH_AVOID
VOS_STATUS wma_process_ch_avoid_update_req(tp_wma_handle wma_handle,
				tSirChAvoidUpdateReq *ch_avoid_update_req);
#endif /* FEATURE_WLAN_CH_AVOID */

static void wma_set_stakey(tp_wma_handle wma_handle, tpSetStaKeyParams key_info);

static void wma_beacon_miss_handler(tp_wma_handle wma, u_int32_t vdev_id);
static void wma_set_suspend_dtim(tp_wma_handle wma);
static void wma_set_resume_dtim(tp_wma_handle wma);
static int wma_roam_event_callback(WMA_HANDLE handle, u_int8_t *event_buf,
				u_int32_t len);
static VOS_STATUS wma_stop_scan(tp_wma_handle wma_handle,
		tAbortScanParams *abort_scan_req);

static void wma_set_sap_keepalive(tp_wma_handle wma, u_int8_t vdev_id);

static void *wma_find_vdev_by_addr(tp_wma_handle wma, u_int8_t *addr,
				   u_int8_t *vdev_id)
{
	u_int8_t i;

	for (i = 0; i < wma->max_bssid; i++) {
		if (vos_is_macaddr_equal(
			(v_MACADDR_t *) wma->interfaces[i].addr,
			(v_MACADDR_t *) addr) == VOS_TRUE) {
			*vdev_id = i;
			return wma->interfaces[i].handle;
		}
	}
	return NULL;
}

/*
 * 802.11n D2.0 defined values for "Minimum MPDU Start Spacing":
 *   0 for no restriction
 *   1 for 1/4 us - Our lower layer calculations limit our precision to 1 msec
 *   2 for 1/2 us - Our lower layer calculations limit our precision to 1 msec
 *   3 for 1 us
 *   4 for 2 us
 *   5 for 4 us
 *   6 for 8 us
 *   7 for 16 us
 */
static const u_int8_t wma_mpdu_spacing[] = {0, 1, 1, 1, 2, 4, 8, 16};

static inline uint8_t wma_parse_mpdudensity(u_int8_t mpdudensity)
{
	if (mpdudensity < sizeof(wma_mpdu_spacing))
		return wma_mpdu_spacing[mpdudensity];
	else
		return 0;
}

/* Function   : wma_find_vdev_by_id
 * Description : Returns vdev handle for given vdev id.
 * Args       : @wma - wma handle, @vdev_id - vdev ID
 * Returns    : Returns vdev handle if given vdev id is valid.
 *              Otherwise returns NULL.
 */
static inline void *wma_find_vdev_by_id(tp_wma_handle wma, u_int8_t vdev_id)
{
	if (vdev_id > wma->max_bssid)
		return NULL;

	return wma->interfaces[vdev_id].handle;
}

/* Function    : wma_get_vdev_count
 * Discription : Returns number of active vdev.
 * Args        : @wma - wma handle
 * Returns     : Returns valid vdev count.
 */
static inline u_int8_t wma_get_vdev_count(tp_wma_handle wma)
{
	u_int8_t vdev_count = 0, i;

	for (i = 0; i < wma->max_bssid; i++) {
		if (wma->interfaces[i].handle)
			vdev_count++;
	}
	return vdev_count;
}

/* Function   : wma_is_vdev_in_ap_mode
 * Description : Helper function to know whether given vdev id
 *              is in AP mode or not.
 * Args       : @wma - wma handle, @ vdev_id - vdev ID.
 * Returns    : True -  if given vdev id is in AP mode.
 *              False - if given vdev id is not in AP mode.
 */
static bool wma_is_vdev_in_ap_mode(tp_wma_handle wma, u_int8_t vdev_id)
{
	struct wma_txrx_node *intf = wma->interfaces;

	if (vdev_id > wma->max_bssid) {
		WMA_LOGP("%s: Invalid vdev_id %hu", __func__, vdev_id);
		VOS_ASSERT(0);
		return false;
	}

	if ((intf[vdev_id].type == WMI_VDEV_TYPE_AP) &&
		((intf[vdev_id].sub_type == WMI_UNIFIED_VDEV_SUBTYPE_P2P_GO) ||
		 (intf[vdev_id].sub_type == 0)))
		return true;

	return false;
}

#ifdef QCA_IBSS_SUPPORT
/* Function   : wma_is_vdev_in_ibss_mode
 s_vdev_in_ibss_mode* Description : Helper function to know whether given vdev id
 *              is in IBSS mode or not.
 * Args       : @wma - wma handle, @ vdev_id - vdev ID.
 * Retruns    : True -  if given vdev id is in IBSS mode.
 *              False - if given vdev id is not in IBSS mode.
 */
static bool wma_is_vdev_in_ibss_mode(tp_wma_handle wma, u_int8_t vdev_id)
{
	struct wma_txrx_node *intf = wma->interfaces;

	if (vdev_id > wma->max_bssid) {
		WMA_LOGP("%s: Invalid vdev_id %hu", __func__, vdev_id);
		VOS_ASSERT(0);
		return false;
	}

	if (intf[vdev_id].type == WMI_VDEV_TYPE_IBSS)
                return true;

        return false;
}
#endif

/*
 * Function     : wma_find_bssid_by_vdev_id
 * Description  : Get the BSS ID corresponding to the vdev ID
 * Args         : @wma - wma handle, @vdev_id - vdev ID
 * Returns      : Returns pointer to bssid on success,
 *                otherwise returns NULL.
 */
static inline u_int8_t *wma_find_bssid_by_vdev_id(tp_wma_handle wma,
						  u_int8_t vdev_id)
{
	if (vdev_id >= wma->max_bssid)
		return NULL;

	return wma->interfaces[vdev_id].bssid;
}

/*
 * Function	: wma_find_vdev_by_bssid
 * Description	: Get the VDEV ID corresponding from BSS ID
 * Args		: @wma - wma handle, @vdev_id - vdev ID
 * Returns	: Returns pointer to bssid on success,
 *                otherwise returns NULL.
 */
static void *wma_find_vdev_by_bssid(tp_wma_handle wma, u_int8_t *bssid,
				    u_int8_t *vdev_id)
{
	int i;

	for (i = 0; i < wma->max_bssid; i++) {
		if (vos_is_macaddr_equal(
			(v_MACADDR_t *)wma->interfaces[i].bssid,
			(v_MACADDR_t *)bssid) == VOS_TRUE) {
			*vdev_id = i;
			return wma->interfaces[i].handle;
		}
	}

	return NULL;
}

#ifdef BIG_ENDIAN_HOST

/* ############# function definitions ############ */

/* function   : wma_swap_bytes
 * Description :
 * Args       :
 * Retruns    :
 */
v_VOID_t wma_swap_bytes(v_VOID_t *pv, v_SIZE_t n)
{
	v_SINT_t no_words;
	v_SINT_t i;
	v_U32_t *word_ptr;

	no_words =   n/sizeof(v_U32_t);
	word_ptr = (v_U32_t *)pv;
	for (i=0; i<no_words; i++) {
		*(word_ptr + i) = __cpu_to_le32(*(word_ptr + i));
	}
}
#define SWAPME(x, len) wma_swap_bytes(&x, len);
#endif

static tANI_U8 wma_get_mcs_idx(tANI_U16 maxRate, tANI_U8 rate_flags,
		tANI_U8 nss,
		tANI_U8 *mcsRateFlag)
{
	tANI_U8  rateFlag = 0, curIdx = 0;
	tANI_U16 curRate;
	bool found = false;
#ifdef WLAN_FEATURE_11AC
	struct index_vht_data_rate_type *supported_vht_mcs_rate;
#endif
	struct index_data_rate_type *supported_mcs_rate;

	WMA_LOGD("%s rate:%d rate_flgs:%d", __func__, maxRate,
			rate_flags);
#ifdef WLAN_FEATURE_11AC
	supported_vht_mcs_rate = (struct index_vht_data_rate_type *)
		((nss == 1)? &supported_vht_mcs_rate_nss1 :
		 &supported_vht_mcs_rate_nss2);
#endif
	supported_mcs_rate = (struct index_data_rate_type *)
		((nss == 1)? &supported_mcs_rate_nss1 :
		 &supported_mcs_rate_nss2);

	*mcsRateFlag = rate_flags;
	*mcsRateFlag &= ~eHAL_TX_RATE_SGI;
#ifdef WLAN_FEATURE_11AC
	if (rate_flags &
			(eHAL_TX_RATE_VHT20 | eHAL_TX_RATE_VHT40 |
			 eHAL_TX_RATE_VHT80)) {

		if (rate_flags & eHAL_TX_RATE_VHT80) {
			for (curIdx = 0; curIdx < MAX_VHT_MCS_IDX; curIdx++) {
				rateFlag = 0;
				if (curIdx >= 7) {
					if (rate_flags & eHAL_TX_RATE_SGI)
						rateFlag |= 0x1;
				}

				curRate =
					supported_vht_mcs_rate[curIdx].supported_VHT80_rate[rateFlag];
				if (curRate == maxRate) {
					found = true;
					break;
				}
			}
		}

		if ((found == false) &&
				((rate_flags & eHAL_TX_RATE_VHT80) ||
				 (rate_flags & eHAL_TX_RATE_VHT40))) {
			for (curIdx = 0; curIdx < MAX_VHT_MCS_IDX; curIdx++) {
				rateFlag = 0;
				if (curIdx >= 7) {
					if (rate_flags & eHAL_TX_RATE_SGI)
						rateFlag |= 0x1;
				}

				curRate =
					supported_vht_mcs_rate[curIdx].supported_VHT40_rate[rateFlag];
				if (curRate == maxRate) {
					found = true;
					*mcsRateFlag &= ~eHAL_TX_RATE_VHT80;
					break;
				}
			}
		}

		if ((found == false) &&
				((rate_flags & eHAL_TX_RATE_VHT80) ||
				 (rate_flags & eHAL_TX_RATE_VHT40) ||
				 (rate_flags & eHAL_TX_RATE_VHT20))) {
			for (curIdx = 0; curIdx < MAX_VHT_MCS_IDX; curIdx++) {
				rateFlag = 0;
				if (curIdx >= 7) {
					if (rate_flags & eHAL_TX_RATE_SGI)
						rateFlag |= 0x1;
				}

				curRate =
					supported_vht_mcs_rate[curIdx].supported_VHT20_rate[rateFlag];
				if (curRate == maxRate) {
					found = true;
					*mcsRateFlag &= ~(eHAL_TX_RATE_VHT80|eHAL_TX_RATE_VHT40);
					break;
				}
			}
		}
	}
#endif
	if ((found == false) &&
			(rate_flags &
			 (eHAL_TX_RATE_HT40|eHAL_TX_RATE_HT20))) {
		if (rate_flags & eHAL_TX_RATE_HT40) {
			rateFlag = 0x1;

			for (curIdx = 0; curIdx < MAX_HT_MCS_IDX; curIdx++) {
				if (curIdx == 7) {
					if (rate_flags & eHAL_TX_RATE_SGI)
						rateFlag |= 0x2;
				}

				curRate = supported_mcs_rate[curIdx].supported_rate[rateFlag];
				if (curRate == maxRate) {
					found = true;
					*mcsRateFlag = eHAL_TX_RATE_HT40;
					break;
				}
			}
		}

		if (found == false) {
			rateFlag = 0;
			for (curIdx = 0; curIdx < MAX_HT_MCS_IDX; curIdx++) {
				if (curIdx == 7) {
					if (rate_flags & eHAL_TX_RATE_SGI)
						rateFlag |= 0x2;
				}

				curRate = supported_mcs_rate[curIdx].supported_rate[rateFlag];
				if (curRate == maxRate) {
					found = true;
					*mcsRateFlag = eHAL_TX_RATE_HT20;
					break;
				}
			}
		}
	}

	/*SGI rates are used by firmware only for MCS >= 7*/
	if (found && (curIdx >= 7))
		*mcsRateFlag |= eHAL_TX_RATE_SGI;

	return (found ? curIdx : INVALID_MCS_IDX);
}

static struct wma_target_req *wma_find_vdev_req(tp_wma_handle wma,
						u_int8_t vdev_id,
						u_int8_t type)
{
	struct wma_target_req *req_msg = NULL, *tmp;
	bool found = false;

	adf_os_spin_lock_bh(&wma->vdev_respq_lock);
	list_for_each_entry_safe(req_msg, tmp,
				 &wma->vdev_resp_queue, node) {
		if (req_msg->vdev_id != vdev_id)
			continue;
		if (req_msg->type != type)
			continue;

		found = true;
		list_del(&req_msg->node);
		break;
	}
	adf_os_spin_unlock_bh(&wma->vdev_respq_lock);
	if (!found) {
		WMA_LOGP("%s: target request not found for vdev_id %d type %d",
			 __func__, vdev_id, type);
		return NULL;
	}
	WMA_LOGD("%s: target request found for vdev id: %d type %d msg %d",
		 __func__, vdev_id, type, req_msg->msg_type);
	return req_msg;
}

tSmpsModeValue host_map_smps_mode (A_UINT32 fw_smps_mode)
{
	tSmpsModeValue smps_mode = SMPS_MODE_DISABLED;
	switch (fw_smps_mode) {
		case WMI_SMPS_FORCED_MODE_STATIC:
			smps_mode = STATIC_SMPS_MODE;
			break;
		case WMI_SMPS_FORCED_MODE_DYNAMIC:
			smps_mode = DYNAMIC_SMPS_MODE;
			break;
		default:
			smps_mode = SMPS_MODE_DISABLED;
	}

	return smps_mode;
}

#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
/* function   : wma_post_auto_shutdown_msg
 * Description : function to post auto shutdown event to sme
 */
static int wma_post_auto_shutdown_msg(void)
{
	tSirAutoShutdownEvtParams *auto_sh_evt;
	VOS_STATUS vos_status;
	vos_msg_t sme_msg = {0} ;

	auto_sh_evt = (tSirAutoShutdownEvtParams *)
			vos_mem_malloc(sizeof(tSirAutoShutdownEvtParams));
	if (!auto_sh_evt) {
		WMA_LOGE("%s: No Mem", __func__);
		return -ENOMEM;
	}

	auto_sh_evt->shutdown_reason =
				WMI_HOST_AUTO_SHUTDOWN_REASON_TIMER_EXPIRY;
	sme_msg.type = eWNI_SME_AUTO_SHUTDOWN_IND;
	sme_msg.bodyptr = auto_sh_evt;
	sme_msg.bodyval = 0;

	vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &sme_msg);
	if ( !VOS_IS_STATUS_SUCCESS(vos_status) ) {
		WMA_LOGE("Fail to post eWNI_SME_AUTO_SHUTDOWN_IND msg to SME");
		vos_mem_free(auto_sh_evt);
		return -EINVAL;
	}

	return 0;
}

/* function   : wma_auto_shutdown_event_handler
 * Description : function to process auto shutdown timer trigger
 */
static int wma_auto_shutdown_event_handler(void *handle, u_int8_t *event,
					u_int32_t len)
{
	wmi_host_auto_shutdown_event_fixed_param *wmi_auto_sh_evt;
        WMI_HOST_AUTO_SHUTDOWN_EVENTID_param_tlvs *param_buf =
		(WMI_HOST_AUTO_SHUTDOWN_EVENTID_param_tlvs *)
		event;

	if (!param_buf || !param_buf->fixed_param) {
		WMA_LOGE("%s:%d: Invalid Auto shutdown timer evt", __func__,
								__LINE__);
		return -EINVAL;
	}


	wmi_auto_sh_evt = param_buf->fixed_param;

	if (wmi_auto_sh_evt->shutdown_reason
			!= WMI_HOST_AUTO_SHUTDOWN_REASON_TIMER_EXPIRY) {
		WMA_LOGE("%s:%d: Invalid Auto shutdown timer evt", __func__,
								 __LINE__);
		return -EINVAL;
	}

	WMA_LOGD("%s:%d: Auto Shutdown Evt: %d", __func__, __LINE__,
					 wmi_auto_sh_evt->shutdown_reason);
	return(wma_post_auto_shutdown_msg());
}

/* function   : wma_set_auto_shutdown_timer_req
 * Description : function sets auto shutdown timer in firmware
 * Args       : wma handle, auto shutdown timer value
 * Returns    : status of wmi cmd
 */
static VOS_STATUS wma_set_auto_shutdown_timer_req(tp_wma_handle wma_handle,
			tSirAutoShutdownCmdParams *auto_sh_cmd)
{
	int status = 0;
	wmi_buf_t buf = NULL;
	u_int8_t *buf_ptr;
	wmi_host_auto_shutdown_cfg_cmd_fixed_param *wmi_auto_sh_cmd;
	int len = sizeof(wmi_host_auto_shutdown_cfg_cmd_fixed_param);

	if (auto_sh_cmd == NULL) {
		WMA_LOGE("%s : Invalid Autoshutdown cfg cmd", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("%s: Set WMI_HOST_AUTO_SHUTDOWN_CFG_CMDID:TIMER_VAL=%d",
					__func__, auto_sh_cmd->timer_val);

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	wmi_auto_sh_cmd = (wmi_host_auto_shutdown_cfg_cmd_fixed_param *)buf_ptr;
	wmi_auto_sh_cmd->timer_value = auto_sh_cmd->timer_val;

	WMITLV_SET_HDR(&wmi_auto_sh_cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_host_auto_shutdown_cfg_cmd_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN(wmi_host_auto_shutdown_cfg_cmd_fixed_param));

	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
		len, WMI_HOST_AUTO_SHUTDOWN_CFG_CMDID);
	if (status != EOK) {
		WMA_LOGE("%s: WMI_HOST_AUTO_SHUTDOWN_CFG_CMDID Err %d",
							__func__, status);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;
}
#endif

static void wma_vdev_start_rsp(tp_wma_handle wma,
			tpAddBssParams add_bss,
			wmi_vdev_start_response_event_fixed_param *resp_event)
{
	struct beacon_info *bcn;

#ifdef QCA_IBSS_SUPPORT
	WMA_LOGD("%s: vdev start response received for %s mode", __func__,
		add_bss->operMode == BSS_OPERATIONAL_MODE_IBSS ? "IBSS" : "non-IBSS");
#endif

	if (resp_event->status) {
		add_bss->status = VOS_STATUS_E_FAILURE;
		goto send_fail_resp;
	}

	if ((add_bss->operMode == BSS_OPERATIONAL_MODE_AP)
#ifdef QCA_IBSS_SUPPORT
                || (add_bss->operMode == BSS_OPERATIONAL_MODE_IBSS)
#endif
        ) {
	wma->interfaces[resp_event->vdev_id].beacon =
				vos_mem_malloc(sizeof(struct beacon_info));

	bcn = wma->interfaces[resp_event->vdev_id].beacon;
	if (!bcn) {
		WMA_LOGE("%s: Failed alloc memory for beacon struct",
			 __func__);
		add_bss->status = VOS_STATUS_E_FAILURE;
		goto send_fail_resp;
	}
	vos_mem_zero(bcn, sizeof(*bcn));
	bcn->buf = adf_nbuf_alloc(NULL, WMA_BCN_BUF_MAX_SIZE, 0,
				  sizeof(u_int32_t), 0);
	if (!bcn->buf) {
		WMA_LOGE("%s: No memory allocated for beacon buffer",
			  __func__);
		vos_mem_free(bcn);
		add_bss->status = VOS_STATUS_E_FAILURE;
		goto send_fail_resp;
	}
	bcn->seq_no = MIN_SW_SEQ;
	adf_os_spinlock_init(&bcn->lock);
	adf_os_atomic_set(&wma->interfaces[resp_event->vdev_id].bss_status,
			  WMA_BSS_STATUS_STARTED);
	WMA_LOGD("%s: AP mode (type %d subtype %d) BSS is started", __func__,
		 wma->interfaces[resp_event->vdev_id].type,
		 wma->interfaces[resp_event->vdev_id].sub_type);

	WMA_LOGD("%s: Allocated beacon struct %p, template memory %p",
		__func__, bcn, bcn->buf);
	}
	add_bss->status = VOS_STATUS_SUCCESS;
	add_bss->bssIdx = resp_event->vdev_id;
	add_bss->chainMask = resp_event->chain_mask;
	add_bss->smpsMode = host_map_smps_mode(resp_event->smps_mode);
send_fail_resp:
	WMA_LOGD("%s: Sending add bss rsp to umac(vdev %d status %d)",
		 __func__, resp_event->vdev_id, add_bss->status);
	wma_send_msg(wma, WDA_ADD_BSS_RSP, (void *)add_bss, 0);
}

static int wma_vdev_start_resp_handler(void *handle, u_int8_t *cmd_param_info,
				       u_int32_t len)
{
	WMI_VDEV_START_RESP_EVENTID_param_tlvs *param_buf;
	wmi_vdev_start_response_event_fixed_param *resp_event;
	u_int8_t *buf;
	vos_msg_t vos_msg = {0};

	WMA_LOGI("%s: Enter", __func__);
	param_buf = (WMI_VDEV_START_RESP_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("Invalid start response event buffer");
		return -EINVAL;
	}

	resp_event = param_buf->fixed_param;
	buf = vos_mem_malloc(sizeof(wmi_vdev_start_response_event_fixed_param));
	if (!buf) {
		WMA_LOGE("%s: Failed alloc memory for buf", __func__);
		return -EINVAL;
	}
	vos_mem_zero(buf, sizeof(wmi_vdev_start_response_event_fixed_param));
	vos_mem_copy(buf, (u_int8_t *)resp_event,
					sizeof(wmi_vdev_start_response_event_fixed_param));

	vos_msg.type = WDA_VDEV_START_RSP_IND;
	vos_msg.bodyptr = buf;
	vos_msg.bodyval = 0;

	if (VOS_STATUS_SUCCESS !=
	    vos_mq_post_message(VOS_MQ_ID_WDA, &vos_msg)) {
		WMA_LOGP("%s: Failed to post WDA_VDEV_START_RSP_IND msg", __func__);
		vos_mem_free(buf);
		return -1;
	}
	WMA_LOGD("WDA_VDEV_START_RSP_IND posted");
	return 0;
}

static int wma_vdev_start_rsp_ind(tp_wma_handle wma, u_int8_t *buf)
{
	struct wma_target_req *req_msg;
	struct wma_txrx_node *iface;
	wmi_vdev_start_response_event_fixed_param *resp_event;

	resp_event = (wmi_vdev_start_response_event_fixed_param *)buf;

	if (!resp_event) {
		WMA_LOGE("Invalid start response event buffer");
		return -EINVAL;
	}

	if ((resp_event->vdev_id <= wma->max_bssid) &&
		(adf_os_atomic_read(
		&wma->interfaces[resp_event->vdev_id].vdev_restart_params.hidden_ssid_restart_in_progress)) &&
		((wma->interfaces[resp_event->vdev_id].type == WMI_VDEV_TYPE_AP) &&
		(wma->interfaces[resp_event->vdev_id].sub_type == 0))) {
		WMA_LOGE(
			"%s: vdev restart event recevied for hidden ssid set using IOCTL",
			__func__);

		if (wmi_unified_vdev_up_send(wma->wmi_handle, resp_event->vdev_id, 0,
			wma->interfaces[resp_event->vdev_id].bssid) < 0) {
			WMA_LOGE("%s : failed to send vdev up", __func__);
			return -EEXIST;
		}
		adf_os_atomic_set(
		&wma->interfaces[resp_event->vdev_id].vdev_restart_params.hidden_ssid_restart_in_progress, 0);
		wma->interfaces[resp_event->vdev_id].vdev_up = TRUE;
	}

	req_msg = wma_find_vdev_req(wma, resp_event->vdev_id,
				    WMA_TARGET_REQ_TYPE_VDEV_START);

	if (!req_msg) {
		WMA_LOGE("%s: Failed to lookup request message for vdev %d",
			 __func__, resp_event->vdev_id);
		return -EINVAL;
	}

	vos_timer_stop(&req_msg->event_timeout);

	iface = &wma->interfaces[resp_event->vdev_id];
	if (req_msg->msg_type == WDA_CHNL_SWITCH_REQ) {
		tpSwitchChannelParams params =
			(tpSwitchChannelParams) req_msg->user_data;
		if(!params) {
			WMA_LOGE("%s: channel switch params is NULL for vdev %d",
				 __func__, resp_event->vdev_id);
			return -EINVAL;
		}

		WMA_LOGD("%s: Send channel switch resp vdev %d status %d",
			 __func__, resp_event->vdev_id, resp_event->status);
		params->chainMask = resp_event->chain_mask;
		params->smpsMode = host_map_smps_mode(resp_event->smps_mode);
		params->status = resp_event->status;
		if (resp_event->resp_type == WMI_VDEV_RESTART_RESP_EVENT &&
			(iface->type == WMI_VDEV_TYPE_STA)) {
			if (wmi_unified_vdev_up_send(wma->wmi_handle,
				resp_event->vdev_id, iface->aid,
				iface->bssid)) {
				WMA_LOGE("%s:vdev_up failed vdev_id %d",
				__func__, resp_event->vdev_id);
				wma->interfaces[resp_event->vdev_id].vdev_up =
									FALSE;
			} else {
				wma->interfaces[resp_event->vdev_id].vdev_up =
									TRUE;
			}
                }

		wma_send_msg(wma, WDA_SWITCH_CHANNEL_RSP, (void *)params, 0);
	} else if (req_msg->msg_type == WDA_ADD_BSS_REQ) {
		tpAddBssParams bssParams = (tpAddBssParams) req_msg->user_data;
                vos_mem_copy(iface->bssid, bssParams->bssId, ETH_ALEN);
		wma_vdev_start_rsp(wma, bssParams, resp_event);
	}


	if ((wma->interfaces[resp_event->vdev_id].type == WMI_VDEV_TYPE_AP) &&
		wma->interfaces[resp_event->vdev_id].vdev_up)
		wma_set_sap_keepalive(wma, resp_event->vdev_id);

	vos_timer_destroy(&req_msg->event_timeout);
	adf_os_mem_free(req_msg);

	return 0;
}

/* function   : wma_unified_debug_print_event_handler
 * Description :
 * Args       :
 * Returns    :
 */
static int wma_unified_debug_print_event_handler(void *handle, u_int8_t *datap,
						 u_int32_t len)
{
	WMI_DEBUG_PRINT_EVENTID_param_tlvs *param_buf;
	u_int8_t *data;
	u_int32_t datalen;

	param_buf = (WMI_DEBUG_PRINT_EVENTID_param_tlvs *)datap;
	if (!param_buf) {
		WMA_LOGE("Get NULL point message from FW");
		return -ENOMEM;
	}
	data = param_buf->data;
	datalen = param_buf->num_data;

#ifdef BIG_ENDIAN_HOST
    {
	    char dbgbuf[500] = {0};
	    memcpy(dbgbuf, data, datalen);
	    SWAPME(dbgbuf, datalen);
	    WMA_LOGD("FIRMWARE:%s", dbgbuf);
	    return 0;
    }
#else
	WMA_LOGD("FIRMWARE:%s", data);
    return 0;
#endif
}

static int
wmi_unified_vdev_set_param_send(wmi_unified_t wmi_handle, u_int32_t if_id,
				u_int32_t param_id, u_int32_t param_value)
{
	int ret;
	wmi_vdev_set_param_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	u_int16_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}
	cmd = (wmi_vdev_set_param_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_set_param_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_vdev_set_param_cmd_fixed_param));
	cmd->vdev_id = if_id;
	cmd->param_id = param_id;
	cmd->param_value = param_value;
	WMA_LOGD("Setting vdev %d param = %x, value = %u",
				if_id, param_id, param_value);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
					WMI_VDEV_SET_PARAM_CMDID);
	if (ret < 0) {
		WMA_LOGE("Failed to send set param command ret = %d", ret);
		wmi_buf_free(buf);
	}
	return ret;
}

VOS_STATUS wma_roam_scan_bmiss_cnt(tp_wma_handle wma_handle,
	A_INT32 first_bcnt,
	A_UINT32 final_bcnt,
	u_int32_t vdev_id)
{
	int status = 0;

	WMA_LOGI("%s: first_bcnt=%d, final_bcnt=%d", __func__,
		 first_bcnt, final_bcnt);

	status = wmi_unified_vdev_set_param_send(wma_handle->wmi_handle,
		vdev_id,
		WMI_VDEV_PARAM_BMISS_FIRST_BCNT,
		first_bcnt);
	if (status != EOK) {
		WMA_LOGE("wmi_unified_vdev_set_param_send"
		"WMI_VDEV_PARAM_BMISS_FIRST_BCNT returned Error %d",status);
		return VOS_STATUS_E_FAILURE;
	}

	status = wmi_unified_vdev_set_param_send(wma_handle->wmi_handle,
		vdev_id,
		WMI_VDEV_PARAM_BMISS_FINAL_BCNT,
		final_bcnt);
	if (status != EOK) {
		WMA_LOGE("wmi_unified_vdev_set_param_send"
		"WMI_VDEV_PARAM_BMISS_FINAL_BCNT returned Error %d",status);
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;
}

static v_VOID_t wma_set_default_tgt_config(tp_wma_handle wma_handle)
{
	struct ol_softc *scn;
	u_int8_t no_of_peers_supported;
	wmi_resource_config tgt_cfg = {
		0, /* Filling zero for TLV Tag and Length fields */
		CFG_TGT_NUM_VDEV,
		CFG_TGT_NUM_PEERS + CFG_TGT_NUM_VDEV + 2,
		CFG_TGT_NUM_OFFLOAD_PEERS,
		CFG_TGT_NUM_OFFLOAD_REORDER_BUFFS,
		CFG_TGT_NUM_PEER_KEYS,
		CFG_TGT_NUM_TIDS,
		CFG_TGT_AST_SKID_LIMIT,
		CFG_TGT_DEFAULT_TX_CHAIN_MASK,
		CFG_TGT_DEFAULT_RX_CHAIN_MASK,
		{ CFG_TGT_RX_TIMEOUT_LO_PRI, CFG_TGT_RX_TIMEOUT_LO_PRI, CFG_TGT_RX_TIMEOUT_LO_PRI, CFG_TGT_RX_TIMEOUT_HI_PRI },
		CFG_TGT_RX_DECAP_MODE,
		CFG_TGT_DEFAULT_SCAN_MAX_REQS,
		CFG_TGT_DEFAULT_BMISS_OFFLOAD_MAX_VDEV,
		CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_VDEV,
		CFG_TGT_DEFAULT_ROAM_OFFLOAD_MAX_PROFILES,
		CFG_TGT_DEFAULT_NUM_MCAST_GROUPS,
		CFG_TGT_DEFAULT_NUM_MCAST_TABLE_ELEMS,
		CFG_TGT_DEFAULT_MCAST2UCAST_MODE,
		CFG_TGT_DEFAULT_TX_DBG_LOG_SIZE,
		CFG_TGT_WDS_ENTRIES,
		CFG_TGT_DEFAULT_DMA_BURST_SIZE,
		CFG_TGT_DEFAULT_MAC_AGGR_DELIM,
		CFG_TGT_DEFAULT_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK,
		CFG_TGT_DEFAULT_VOW_CONFIG,
		CFG_TGT_DEFAULT_GTK_OFFLOAD_MAX_VDEV,
		CFG_TGT_NUM_MSDU_DESC,
		CFG_TGT_MAX_FRAG_TABLE_ENTRIES,
		CFG_TGT_NUM_TDLS_VDEVS,
		CFG_TGT_NUM_TDLS_CONN_TABLE_ENTRIES,
		CFG_TGT_DEFAULT_BEACON_TX_OFFLOAD_MAX_VDEV,
		CFG_TGT_MAX_MULTICAST_FILTER_ENTRIES,
		0,
		0,
		0,
		CFG_TGT_NUM_TDLS_CONC_SLEEP_STAS,
		CFG_TGT_NUM_TDLS_CONC_BUFFER_STAS,
	};

	/* Update the max number of peers */
	scn = vos_get_context(VOS_MODULE_ID_HIF, wma_handle->vos_context);
	if (!scn) {
		WMA_LOGE("%s: vos_context is NULL", __func__);
		return;
	}
	no_of_peers_supported = ol_get_number_of_peers_supported(scn);
	tgt_cfg.num_peers = no_of_peers_supported + CFG_TGT_NUM_VDEV + 2;
	tgt_cfg.num_tids = (2 * (no_of_peers_supported + CFG_TGT_NUM_VDEV + 2));

	WMITLV_SET_HDR(&tgt_cfg.tlv_header,WMITLV_TAG_STRUC_wmi_resource_config,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_resource_config));
	/* reduce the peer/vdev if CFG_TGT_NUM_MSDU_DESC exceeds 1000 */
#ifdef PERE_IP_HDR_ALIGNMENT_WAR
	if (scn->host_80211_enable) {
		/*
		 * To make the IP header begins at dword aligned address,
		 * we make the decapsulation mode as Native Wifi.
		 */
		tgt_cfg.rx_decap_mode = CFG_TGT_RX_DECAP_MODE_NWIFI;
	}
#endif
	wma_handle->wlan_resource_config = tgt_cfg;
}

static int32_t wmi_unified_peer_delete_send(wmi_unified_t wmi,
					u_int8_t peer_addr[IEEE80211_ADDR_LEN],
					u_int8_t vdev_id)
{
	wmi_peer_delete_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}
	cmd = (wmi_peer_delete_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_delete_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_peer_delete_cmd_fixed_param));
	WMI_CHAR_ARRAY_TO_MAC_ADDR(peer_addr, &cmd->peer_macaddr);
	cmd->vdev_id = vdev_id;

	if (wmi_unified_cmd_send(wmi, buf, len, WMI_PEER_DELETE_CMDID)) {
		WMA_LOGP("%s: Failed to send peer delete command", __func__);
		adf_nbuf_free(buf);
		return -EIO;
	}
	WMA_LOGD("%s: peer_addr %pM vdev_id %d", __func__, peer_addr, vdev_id);
	return 0;
}

static int32_t wmi_unified_peer_flush_tids_send(wmi_unified_t wmi,
					    u_int8_t peer_addr
							[IEEE80211_ADDR_LEN],
					    u_int32_t peer_tid_bitmap,
					    u_int8_t vdev_id)
{
	wmi_peer_flush_tids_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}
	cmd = (wmi_peer_flush_tids_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_flush_tids_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_peer_flush_tids_cmd_fixed_param));
	WMI_CHAR_ARRAY_TO_MAC_ADDR(peer_addr, &cmd->peer_macaddr);
	cmd->peer_tid_bitmap = peer_tid_bitmap;
	cmd->vdev_id = vdev_id;

	if (wmi_unified_cmd_send(wmi, buf, len, WMI_PEER_FLUSH_TIDS_CMDID)) {
		WMA_LOGP("%s: Failed to send flush tid command", __func__);
		adf_nbuf_free(buf);
		return -EIO;
	}
	WMA_LOGD("%s: peer_addr %pM vdev_id %d", __func__, peer_addr, vdev_id);
	return 0;
}

static void wma_remove_peer(tp_wma_handle wma, u_int8_t *bssid,
			    u_int8_t vdev_id, ol_txrx_peer_handle peer,
			    v_BOOL_t roam_synch_in_progress)
{
#define PEER_ALL_TID_BITMASK 0xffffffff
	u_int32_t peer_tid_bitmap = PEER_ALL_TID_BITMASK;
	u_int8_t *peer_addr = bssid;
        if (!wma->interfaces[vdev_id].peer_count)
        {
             WMA_LOGE("%s: Can't remove peer with peer_addr %pM vdevid %d peer_count %d",
                    __func__, bssid, vdev_id, wma->interfaces[vdev_id].peer_count);
             return;
        }
	if (peer)
		ol_txrx_peer_detach(peer);

	wma->interfaces[vdev_id].peer_count--;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (roam_synch_in_progress) {
	    WMA_LOGD("%s:LFR3:Removed peer with addr %pM vdevid %d peer_cnt %d",
		__func__, bssid, vdev_id, wma->interfaces[vdev_id].peer_count);
	    return;
	} else {
	    WMA_LOGE("%s: Removed peer with addr %pM vdevid %d peer_count %d",
		__func__, bssid, vdev_id, wma->interfaces[vdev_id].peer_count);
	}
#endif
	/* Flush all TIDs except MGMT TID for this peer in Target */
	peer_tid_bitmap &= ~(0x1 << WMI_MGMT_TID);
	wmi_unified_peer_flush_tids_send(wma->wmi_handle, bssid,
					 peer_tid_bitmap, vdev_id);

#if defined(QCA_IBSS_SUPPORT)
	if ((peer) && (wma_is_vdev_in_ibss_mode(wma, vdev_id))) {
		WMA_LOGD("%s: bssid %pM peer->mac_addr %pM", __func__,
			bssid, peer->mac_addr.raw);
		peer_addr = peer->mac_addr.raw;
	}
#endif

	wmi_unified_peer_delete_send(wma->wmi_handle, peer_addr, vdev_id);
#undef PEER_ALL_TID_BITMASK
}

static int wma_peer_sta_kickout_event_handler(void *handle, u8 *event, u32 len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	WMI_PEER_STA_KICKOUT_EVENTID_param_tlvs *param_buf = NULL;
	wmi_peer_sta_kickout_event_fixed_param *kickout_event = NULL;
	u_int8_t vdev_id, peer_id, macaddr[IEEE80211_ADDR_LEN];
	ol_txrx_peer_handle peer;
	ol_txrx_pdev_handle pdev;
	tpDeleteStaContext del_sta_ctx;
	tpSirIbssPeerInactivityInd p_inactivity;

	WMA_LOGD("%s: Enter", __func__);
	param_buf = (WMI_PEER_STA_KICKOUT_EVENTID_param_tlvs *) event;
	kickout_event = param_buf->fixed_param;
	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);
	if (!pdev) {
		WMA_LOGE("%s: pdev is NULL", __func__);
		return -EINVAL;
	}
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&kickout_event->peer_macaddr, macaddr);
	peer = ol_txrx_find_peer_by_addr(pdev, macaddr, &peer_id);
	if (!peer) {
		WMA_LOGE("PEER [%pM] not found", macaddr);
		return -EINVAL;
	}

	if (tl_shim_get_vdevid(peer, &vdev_id) != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Not able to find BSSID for peer [%pM]", macaddr);
		return -EINVAL;
	}

	WMA_LOGA("%s: PEER:[%pM], ADDR:[%pN], INTERFACE:%d, peer_id:%d, reason:%d",
	         __func__, macaddr,
	         wma->interfaces[vdev_id].addr, vdev_id,
	         peer_id, kickout_event->reason);

	switch (kickout_event->reason) {
	    case WMI_PEER_STA_KICKOUT_REASON_IBSS_DISCONNECT:
		p_inactivity = (tpSirIbssPeerInactivityInd)
			vos_mem_malloc(sizeof(tSirIbssPeerInactivityInd));
		if (!p_inactivity) {
			WMA_LOGE("VOS MEM Alloc Failed for tSirIbssPeerInactivity");
			return -EINVAL;
		}

		p_inactivity->staIdx = peer_id;
		vos_mem_copy(p_inactivity->peerAddr, macaddr, IEEE80211_ADDR_LEN);
		wma_send_msg(wma, WDA_IBSS_PEER_INACTIVITY_IND, (void *)p_inactivity, 0);
		goto exit_handler;
		break;

#ifdef FEATURE_WLAN_TDLS
	    case WMI_PEER_STA_KICKOUT_REASON_TDLS_DISCONNECT:
		del_sta_ctx =
			(tpDeleteStaContext)vos_mem_malloc(sizeof(tDeleteStaContext));
		if (!del_sta_ctx) {
			WMA_LOGE("%s: mem alloc failed for tDeleteStaContext for TDLS peer: %pM",
			         __func__, macaddr);
			return -EINVAL;
		}

		del_sta_ctx->is_tdls = true;
		del_sta_ctx->vdev_id = vdev_id;
		del_sta_ctx->staId = peer_id;
		vos_mem_copy(del_sta_ctx->addr2, macaddr, IEEE80211_ADDR_LEN);
		vos_mem_copy(del_sta_ctx->bssId, wma->interfaces[vdev_id].bssid,
				IEEE80211_ADDR_LEN);
		del_sta_ctx->reasonCode = HAL_DEL_STA_REASON_CODE_KEEP_ALIVE;
		wma_send_msg(wma, SIR_LIM_DELETE_STA_CONTEXT_IND, (void *)del_sta_ctx,
			0);
		goto exit_handler;
		break;
#endif /* FEATURE_WLAN_TDLS */

	    case WMI_PEER_STA_KICKOUT_REASON_XRETRY:
		if(wma->interfaces[vdev_id].type == WMI_VDEV_TYPE_STA &&
		   (wma->interfaces[vdev_id].sub_type == 0 ||
		    wma->interfaces[vdev_id].sub_type ==
				 WMI_UNIFIED_VDEV_SUBTYPE_P2P_CLIENT) &&
		   vos_mem_compare(wma->interfaces[vdev_id].bssid,
				   macaddr, ETH_ALEN)) {
		    /*
		     * KICKOUT event is for current station-AP connection.
		     * Treat it like final beacon miss. Station may not have
		     * missed beacons but not able to transmit frames to AP
		     * for a long time. Must disconnect to get out of
		     * this sticky situation.
		     * In future implementation, roaming module will also
		     * handle this event and perform a scan.
		     */
		    WMA_LOGW("%s: WMI_PEER_STA_KICKOUT_REASON_XRETRY event for STA",
				__func__);
		    wma_beacon_miss_handler(wma, vdev_id);
		    goto exit_handler;
		}
		break;

	    case WMI_PEER_STA_KICKOUT_REASON_UNSPECIFIED:
		/*
		 * Default legacy value used by original firmware implementation.
		 */
		if(wma->interfaces[vdev_id].type == WMI_VDEV_TYPE_STA &&
		   (wma->interfaces[vdev_id].sub_type == 0 ||
		    wma->interfaces[vdev_id].sub_type ==
				 WMI_UNIFIED_VDEV_SUBTYPE_P2P_CLIENT) &&
		   vos_mem_compare(wma->interfaces[vdev_id].bssid,
				   macaddr, ETH_ALEN)) {
		    /*
		     * KICKOUT event is for current station-AP connection.
		     * Treat it like final beacon miss. Station may not have
		     * missed beacons but not able to transmit frames to AP
		     * for a long time. Must disconnect to get out of
		     * this sticky situation.
		     * In future implementation, roaming module will also
		     * handle this event and perform a scan.
		     */
		    WMA_LOGW("%s: WMI_PEER_STA_KICKOUT_REASON_UNSPECIFIED event for STA",
				__func__);
		    wma_beacon_miss_handler(wma, vdev_id);
		    goto exit_handler;
		}
		break;

	    case WMI_PEER_STA_KICKOUT_REASON_INACTIVITY:
		/* This could be for STA or SAP role */
	    default:
		break;
	}

	/*
	 * default action is to send delete station context indication to LIM
	 */
	del_sta_ctx = (tpDeleteStaContext)vos_mem_malloc(sizeof(tDeleteStaContext));
	if (!del_sta_ctx) {
		WMA_LOGE("VOS MEM Alloc Failed for tDeleteStaContext");
		return -EINVAL;
	}

	del_sta_ctx->is_tdls = false;
	del_sta_ctx->vdev_id = vdev_id;
	del_sta_ctx->staId = peer_id;
	vos_mem_copy(del_sta_ctx->addr2, macaddr, IEEE80211_ADDR_LEN);
	vos_mem_copy(del_sta_ctx->bssId, wma->interfaces[vdev_id].addr,
		IEEE80211_ADDR_LEN);
	del_sta_ctx->reasonCode = HAL_DEL_STA_REASON_CODE_KEEP_ALIVE;
	del_sta_ctx->rssi = kickout_event->rssi + WMA_TGT_NOISE_FLOOR_DBM;
	wma_send_msg(wma, SIR_LIM_DELETE_STA_CONTEXT_IND, (void *)del_sta_ctx, 0);

exit_handler:
	WMA_LOGD("%s: Exit", __func__);
	return 0;
}

static int wmi_unified_vdev_down_send(wmi_unified_t wmi, u_int8_t vdev_id)
{
	wmi_vdev_down_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi, len);
	if (!buf) {
		WMA_LOGP("%s : wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}
	cmd = (wmi_vdev_down_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_down_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_down_cmd_fixed_param));
	cmd->vdev_id = vdev_id;
	if (wmi_unified_cmd_send(wmi, buf, len, WMI_VDEV_DOWN_CMDID)) {
		WMA_LOGP("%s: Failed to send vdev down", __func__);
		adf_nbuf_free(buf);
		return -EIO;
	}
	WMA_LOGE("%s: vdev_id %d", __func__, vdev_id);
	return 0;
}

#ifdef QCA_IBSS_SUPPORT
static void wma_delete_all_ibss_peers(tp_wma_handle wma, A_UINT32 vdev_id)
{
	ol_txrx_vdev_handle vdev;
	ol_txrx_peer_handle peer;

	if (!wma || vdev_id > wma->max_bssid)
		return;

	vdev = wma->interfaces[vdev_id].handle;
	if (!vdev)
		return;

	/* remove all IBSS remote peers first */
	adf_os_spin_lock_bh(&vdev->pdev->peer_ref_mutex);
	TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
		if (peer != TAILQ_FIRST(&vdev->peer_list)) {
			adf_os_spin_unlock_bh(&vdev->pdev->peer_ref_mutex);
			adf_os_atomic_init(&peer->ref_cnt);
			adf_os_atomic_inc(&peer->ref_cnt);
			wma_remove_peer(wma, wma->interfaces[vdev_id].bssid,
				vdev_id, peer, VOS_FALSE);
			adf_os_spin_lock_bh(&vdev->pdev->peer_ref_mutex);
		}
	}
	adf_os_spin_unlock_bh(&vdev->pdev->peer_ref_mutex);

	/* remove IBSS bss peer last */
	peer = TAILQ_FIRST(&vdev->peer_list);
	wma_remove_peer(wma, wma->interfaces[vdev_id].bssid, vdev_id, peer,
			VOS_FALSE);
}
#endif //#ifdef QCA_IBSS_SUPPORT

static void wma_delete_all_ap_remote_peers(tp_wma_handle wma, A_UINT32 vdev_id)
{
	ol_txrx_vdev_handle vdev;
	ol_txrx_peer_handle peer, temp;

	if (!wma || vdev_id > wma->max_bssid)
		return;

	vdev = wma->interfaces[vdev_id].handle;
	if (!vdev)
		return;

	WMA_LOGE("%s: vdev_id - %d", __func__, vdev_id);
	/* remove all remote peers of SAP */
	adf_os_spin_lock_bh(&vdev->pdev->peer_ref_mutex);

	temp = NULL;
	TAILQ_FOREACH_REVERSE(peer, &vdev->peer_list, peer_list_t, peer_list_elem) {
		if (temp) {
			adf_os_spin_unlock_bh(&vdev->pdev->peer_ref_mutex);
			if (adf_os_atomic_read(&temp->delete_in_progress) == 0){
				wma_remove_peer(wma, temp->mac_addr.raw,
					vdev_id, temp, VOS_FALSE);
			}
			adf_os_spin_lock_bh(&vdev->pdev->peer_ref_mutex);
		}
		/* self peer is deleted by caller */
		if (peer == TAILQ_FIRST(&vdev->peer_list)){
			WMA_LOGE("%s: self peer removed by caller ", __func__);
			break;
		} else
			temp = peer;
	}

	adf_os_spin_unlock_bh(&vdev->pdev->peer_ref_mutex);
}

#ifdef QCA_IBSS_SUPPORT
static void wma_recreate_ibss_vdev_and_bss_peer(tp_wma_handle wma, u_int8_t vdev_id)
{
	ol_txrx_vdev_handle vdev;
	tDelStaSelfParams del_sta_param;
	tAddStaSelfParams add_sta_self_param;
	VOS_STATUS status;

	if (!wma) {
		WMA_LOGE("%s: Null wma handle", __func__);
		return;
	}

	vdev = wma_find_vdev_by_id(wma, vdev_id);
	if (!vdev) {
		WMA_LOGE("%s: Can't find vdev with id %d", __func__, vdev_id);
		return;
	}

        vos_copy_macaddr((v_MACADDR_t *)&(add_sta_self_param.selfMacAddr),
			(v_MACADDR_t *)&(vdev->mac_addr));
	add_sta_self_param.sessionId = vdev_id;
	add_sta_self_param.type      = WMI_VDEV_TYPE_IBSS;
	add_sta_self_param.subType   = 0;
	add_sta_self_param.status    = 0;

	/* delete old ibss vdev */
	del_sta_param.sessionId   = vdev_id;
	vos_mem_copy((void *)del_sta_param.selfMacAddr,
		(void *)&(vdev->mac_addr),
	VOS_MAC_ADDR_SIZE);
	wma_vdev_detach(wma, &del_sta_param, 0);

	/* create new vdev for ibss */
	vdev = wma_vdev_attach(wma, &add_sta_self_param, 0);
	if (!vdev) {
		WMA_LOGE("%s: Failed to create vdev", __func__);
		return;
	}

	WLANTL_RegisterVdev(wma->vos_context, vdev);
	/* Register with TxRx Module for Data Ack Complete Cb */
	wdi_in_data_tx_cb_set(vdev, wma_data_tx_ack_comp_hdlr, wma);
	WMA_LOGA("new IBSS vdev created with mac %pM", add_sta_self_param.selfMacAddr);

	/* create ibss bss peer */
	status = wma_create_peer(wma, vdev->pdev, vdev, vdev->mac_addr.raw,
			WMI_PEER_TYPE_DEFAULT, vdev_id, VOS_FALSE);
	if (status != VOS_STATUS_SUCCESS)
		WMA_LOGE("%s: Failed to create IBSS bss peer", __func__);
	else
		WMA_LOGA("IBSS BSS peer created with mac %pM", vdev->mac_addr.raw);
}
#endif //#ifdef QCA_IBSS_SUPPORT

static int wma_vdev_stop_resp_handler(void *handle, u_int8_t *cmd_param_info,
				      u32 len)
{
	WMI_VDEV_STOPPED_EVENTID_param_tlvs *param_buf;
	wmi_vdev_stopped_event_fixed_param *event;
	u_int8_t *buf;
	vos_msg_t vos_msg = {0};

	WMA_LOGI("%s: Enter", __func__);
	param_buf = (WMI_VDEV_STOPPED_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("Invalid event buffer");
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	buf = vos_mem_malloc(sizeof(wmi_vdev_stopped_event_fixed_param));
	if (!buf) {
		WMA_LOGE("%s: Failed alloc memory for buf", __func__);
		return -EINVAL;
	}
	vos_mem_zero(buf, sizeof(wmi_vdev_stopped_event_fixed_param));
	vos_mem_copy(buf, (u_int8_t *)event,
					sizeof(wmi_vdev_stopped_event_fixed_param));

	vos_msg.type = WDA_VDEV_STOP_IND;
	vos_msg.bodyptr = buf;
	vos_msg.bodyval = 0;

	if (VOS_STATUS_SUCCESS !=
	    vos_mq_post_message(VOS_MQ_ID_WDA, &vos_msg)) {
		WMA_LOGP("%s: Failed to post WDA_VDEV_STOP_IND msg", __func__);
		vos_mem_free(buf);
		return -1;
	}
	WMA_LOGD("WDA_VDEV_STOP_IND posted");
	return 0;
}

void wma_hidden_ssid_vdev_restart_on_vdev_stop(tp_wma_handle wma_handle, u_int8_t sessionId)
{
	wmi_vdev_start_request_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	wmi_channel *chan;
	int32_t len;
	u_int8_t *buf_ptr;
	struct wma_txrx_node *intr = wma_handle->interfaces;
	int32_t ret=0;

	len = sizeof(*cmd) + sizeof(wmi_channel) +
			WMI_TLV_HDR_SIZE;
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		adf_os_atomic_set(&intr[sessionId].vdev_restart_params.hidden_ssid_restart_in_progress,0);
		return;
	}
	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_vdev_start_request_cmd_fixed_param *) buf_ptr;
	chan = (wmi_channel *) (buf_ptr + sizeof(*cmd));

	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_vdev_start_request_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
			wmi_vdev_start_request_cmd_fixed_param));

	WMITLV_SET_HDR(&chan->tlv_header,
			WMITLV_TAG_STRUC_wmi_channel,
			WMITLV_GET_STRUCT_TLVLEN(wmi_channel));

	cmd->vdev_id = sessionId;
	cmd->ssid.ssid_len = intr[sessionId].vdev_restart_params.ssid.ssid_len;
	vos_mem_copy(cmd->ssid.ssid,
		intr[sessionId].vdev_restart_params.ssid.ssid,
		cmd->ssid.ssid_len);
	cmd->flags = intr[sessionId].vdev_restart_params.flags;
	if (intr[sessionId].vdev_restart_params.ssidHidden)
		cmd->flags |= WMI_UNIFIED_VDEV_START_HIDDEN_SSID;
	else
		cmd->flags &= (0xFFFFFFFE);
	cmd->requestor_id = intr[sessionId].vdev_restart_params.requestor_id;
	cmd->disable_hw_ack = intr[sessionId].vdev_restart_params.disable_hw_ack;

	chan->mhz = intr[sessionId].vdev_restart_params.chan.mhz;
	chan->band_center_freq1 = intr[sessionId].vdev_restart_params.chan.band_center_freq1;
	chan->band_center_freq2 = intr[sessionId].vdev_restart_params.chan.band_center_freq2;
	chan->info = intr[sessionId].vdev_restart_params.chan.info;
	chan->reg_info_1 = intr[sessionId].vdev_restart_params.chan.reg_info_1;
	chan->reg_info_2 = intr[sessionId].vdev_restart_params.chan.reg_info_2;

	cmd->num_noa_descriptors = 0;
	buf_ptr = (u_int8_t *)(((u_int8_t *) cmd) + sizeof(*cmd) +
					sizeof(wmi_channel));
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
			cmd->num_noa_descriptors *
			sizeof(wmi_p2p_noa_descriptor));

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle,buf,len,
			WMI_VDEV_RESTART_REQUEST_CMDID);
	if (ret < 0) {
		WMA_LOGE("%s: Failed to send vdev restart command", __func__);
		adf_os_atomic_set(&intr[sessionId].vdev_restart_params.hidden_ssid_restart_in_progress,0);
		adf_nbuf_free(buf);
	}
}

static int wma_vdev_stop_ind(tp_wma_handle wma, u_int8_t *buf)
{
	wmi_vdev_stopped_event_fixed_param *resp_event;
	struct wma_target_req *req_msg;
	ol_txrx_peer_handle peer;
	ol_txrx_pdev_handle pdev;
	u_int8_t peer_id;
	struct wma_txrx_node *iface;
	int32_t status = 0;

	WMA_LOGI("%s: Enter", __func__);
	if (!buf) {
		WMA_LOGE("Invalid event buffer");
		return -EINVAL;
	}

	resp_event = (wmi_vdev_stopped_event_fixed_param *)buf;

	if ((resp_event->vdev_id <= wma->max_bssid) &&
	(adf_os_atomic_read(&wma->interfaces[resp_event->vdev_id].vdev_restart_params.hidden_ssid_restart_in_progress)) &&
	((wma->interfaces[resp_event->vdev_id].type == WMI_VDEV_TYPE_AP) &&
	(wma->interfaces[resp_event->vdev_id].sub_type == 0))) {
		WMA_LOGE("%s: vdev stop event recevied for hidden ssid set using IOCTL ", __func__);
		wma_hidden_ssid_vdev_restart_on_vdev_stop(wma, resp_event->vdev_id);
	}

	req_msg = wma_find_vdev_req(wma, resp_event->vdev_id,
				    WMA_TARGET_REQ_TYPE_VDEV_STOP);
	if (!req_msg) {
		WMA_LOGP("%s: Failed to lookup vdev request for vdev id %d",
			 __func__, resp_event->vdev_id);
		return -EINVAL;
	}
	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);
	if (!pdev) {
		WMA_LOGE("%s: pdev is NULL", __func__);
		status = -EINVAL;
		vos_timer_stop(&req_msg->event_timeout);
		goto free_req_msg;
	}

	vos_timer_stop(&req_msg->event_timeout);
	if (req_msg->msg_type == WDA_DELETE_BSS_REQ) {
		tpDeleteBssParams params =
			(tpDeleteBssParams)req_msg->user_data;
		struct beacon_info *bcn;
		if (resp_event->vdev_id > wma->max_bssid) {
			WMA_LOGE("%s: Invalid vdev_id %d", __func__,
				resp_event->vdev_id);
			status = -EINVAL;
			goto free_req_msg;
		}

		iface = &wma->interfaces[resp_event->vdev_id];
		if (iface->handle == NULL) {
			WMA_LOGE("%s vdev id %d is already deleted",
				__func__, resp_event->vdev_id);
			status = -EINVAL;
			goto free_req_msg;
		}

#ifdef QCA_IBSS_SUPPORT
		if ( wma_is_vdev_in_ibss_mode(wma, resp_event->vdev_id))
			wma_delete_all_ibss_peers(wma, resp_event->vdev_id);
		else
#endif
		{
			if (wma_is_vdev_in_ap_mode(wma, resp_event->vdev_id))
			{
				wma_delete_all_ap_remote_peers(wma, resp_event->vdev_id);
			}
			peer = ol_txrx_find_peer_by_addr(pdev, params->bssid,
					&peer_id);
			if (!peer)
				WMA_LOGD("%s Failed to find peer %pM",
					__func__, params->bssid);
			wma_remove_peer(wma, params->bssid, resp_event->vdev_id,
					peer, VOS_FALSE);
		}

		if (wmi_unified_vdev_down_send(wma->wmi_handle, resp_event->vdev_id) < 0) {
			WMA_LOGE("Failed to send vdev down cmd: vdev %d",
				resp_event->vdev_id);
		} else {
			wma->interfaces[resp_event->vdev_id].vdev_up = FALSE;
		}
		ol_txrx_vdev_flush(iface->handle);
                WMA_LOGD("%s, vdev_id: %d, un-pausing tx_ll_queue for VDEV_STOP rsp",
                         __func__, resp_event->vdev_id);
		wdi_in_vdev_unpause(iface->handle,
				    OL_TXQ_PAUSE_REASON_VDEV_STOP);
		iface->pause_bitmap &= ~(1 << PAUSE_TYPE_HOST);
		adf_os_atomic_set(&iface->bss_status, WMA_BSS_STATUS_STOPPED);
		WMA_LOGD("%s: (type %d subtype %d) BSS is stopped",
			 __func__, iface->type, iface->sub_type);
		bcn = wma->interfaces[resp_event->vdev_id].beacon;

		if (bcn) {
			WMA_LOGD("%s: Freeing beacon struct %p, "
				 "template memory %p", __func__,
				 bcn, bcn->buf);
			if (bcn->dma_mapped)
				adf_nbuf_unmap_single(pdev->osdev, bcn->buf,
						      ADF_OS_DMA_TO_DEVICE);
			adf_nbuf_free(bcn->buf);
			vos_mem_free(bcn);
			wma->interfaces[resp_event->vdev_id].beacon = NULL;
		}

#ifdef QCA_IBSS_SUPPORT
		/* recreate ibss vdev and bss peer for scan purpose */
		if (wma_is_vdev_in_ibss_mode(wma, resp_event->vdev_id))
			wma_recreate_ibss_vdev_and_bss_peer(wma, resp_event->vdev_id);
#endif
		/* Timeout status means its WMA generated DEL BSS REQ when ADD
		BSS REQ was timed out to stop the VDEV in this case no need to
		send response to UMAC */
		if (params->status == eHAL_STATUS_FW_MSG_TIMEDOUT){
			vos_mem_free(params);
			WMA_LOGE("%s: DEL BSS from ADD BSS timeout do not send "
					"resp to UMAC (vdev id %x)",
					__func__, resp_event->vdev_id);
		} else {
			params->status = VOS_STATUS_SUCCESS;
			wma_send_msg(wma, WDA_DELETE_BSS_RSP, (void *)params, 0);
		}

		if (iface->del_staself_req) {
			WMA_LOGA("scheduling defered deletion (vdev id %x)",
					resp_event->vdev_id);
			wma_vdev_detach(wma, iface->del_staself_req, 1);
		}
	}
free_req_msg:
	vos_timer_destroy(&req_msg->event_timeout);
	adf_os_mem_free(req_msg);
	return status;
}

#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
static void wma_send_status_of_ext_wow(tp_wma_handle wma, boolean status)
{
	tSirReadyToExtWoWInd *ready_to_extwow;
	VOS_STATUS vstatus;
	vos_msg_t vos_msg;
	u_int8_t len;

	WMA_LOGD("Posting ready to suspend indication to umac");

	len = sizeof(tSirReadyToExtWoWInd);
	ready_to_extwow = (tSirReadyToExtWoWInd *) vos_mem_malloc(len);

	if (NULL == ready_to_extwow) {
		WMA_LOGE("%s: Memory allocation failure", __func__);
		return;
	}

	ready_to_extwow->mesgType = eWNI_SME_READY_TO_EXTWOW_IND;
	ready_to_extwow->mesgLen = len;
	ready_to_extwow->status= status;

	vos_msg.type = eWNI_SME_READY_TO_EXTWOW_IND;
	vos_msg.bodyptr = (void *) ready_to_extwow;
	vos_msg.bodyval = 0;

	vstatus = vos_mq_post_message(VOS_MQ_ID_SME, &vos_msg);
	if (vstatus != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to post ready to suspend");
		vos_mem_free(ready_to_extwow);
	}
}

static int wma_enable_ext_wow(tp_wma_handle wma,
			 tpSirExtWoWParams params)
{
	wmi_extwow_enable_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len;
	int ret;

	len = sizeof(wmi_extwow_enable_cmd_fixed_param);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (wmi_extwow_enable_cmd_fixed_param *) wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_extwow_enable_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				wmi_extwow_enable_cmd_fixed_param));

	cmd->vdev_id = params->vdev_id;
	cmd->type = params->type;
	cmd->wakeup_pin_num = params->wakeup_pin_num;

	WMA_LOGD("%s: vdev_id %d type %d Wakeup_pin_num %x",
		 __func__, cmd->vdev_id,
		 cmd->type, cmd->wakeup_pin_num);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_EXTWOW_ENABLE_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to set EXTWOW Enable", __func__);
		wmi_buf_free(buf);
		wma_send_status_of_ext_wow(wma, FALSE);
		return VOS_STATUS_E_FAILURE;
	}

	wma_send_status_of_ext_wow(wma, TRUE);
	return VOS_STATUS_SUCCESS;

}

static int wma_set_app_type1_params_in_fw(tp_wma_handle wma,
			 tpSirAppType1Params appType1Params)
{
	wmi_extwow_set_app_type1_params_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len;
	int ret;

	len = sizeof(wmi_extwow_set_app_type1_params_cmd_fixed_param);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (wmi_extwow_set_app_type1_params_cmd_fixed_param *)
						    wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
	    WMITLV_TAG_STRUC_wmi_extwow_set_app_type1_params_cmd_fixed_param,
	    WMITLV_GET_STRUCT_TLVLEN(
			wmi_extwow_set_app_type1_params_cmd_fixed_param));

	cmd->vdev_id = appType1Params->vdev_id;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(appType1Params->wakee_mac_addr,
							 &cmd->wakee_mac);
	vos_mem_copy(cmd->ident, appType1Params->identification_id, 8);
	cmd->ident_len = appType1Params->id_length;
	vos_mem_copy(cmd->passwd, appType1Params->password, 16);
	cmd->passwd_len = appType1Params->pass_length;

	WMA_LOGD("%s: vdev_id %d wakee_mac_addr %pM "
			 "identification_id %.8s id_length %u "
			 "password %.16s pass_length %u",
		__func__, cmd->vdev_id, appType1Params->wakee_mac_addr,
		cmd->ident, cmd->ident_len,
		cmd->passwd, cmd->passwd_len);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_EXTWOW_SET_APP_TYPE1_PARAMS_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to set APP TYPE1 PARAMS", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;
}

static int wma_set_app_type2_params_in_fw(tp_wma_handle wma,
			 tpSirAppType2Params appType2Params)
{
	wmi_extwow_set_app_type2_params_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len;
	int ret;

	len = sizeof(wmi_extwow_set_app_type2_params_cmd_fixed_param);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (wmi_extwow_set_app_type2_params_cmd_fixed_param *)
						    wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
             WMITLV_TAG_STRUC_wmi_extwow_set_app_type2_params_cmd_fixed_param,
             WMITLV_GET_STRUCT_TLVLEN(
			wmi_extwow_set_app_type2_params_cmd_fixed_param));

	cmd->vdev_id = appType2Params->vdev_id;

	vos_mem_copy(cmd->rc4_key, appType2Params->rc4_key, 16);
	cmd->rc4_key_len = appType2Params->rc4_key_len;

	cmd->ip_id = appType2Params->ip_id;
	cmd->ip_device_ip = appType2Params->ip_device_ip;
	cmd->ip_server_ip = appType2Params->ip_server_ip;

	cmd->tcp_src_port = appType2Params->tcp_src_port;
	cmd->tcp_dst_port = appType2Params->tcp_dst_port;
	cmd->tcp_seq = appType2Params->tcp_seq;
	cmd->tcp_ack_seq = appType2Params->tcp_ack_seq;

	cmd->keepalive_init = appType2Params->keepalive_init;
	cmd->keepalive_min = appType2Params->keepalive_min;
	cmd->keepalive_max = appType2Params->keepalive_max;
	cmd->keepalive_inc = appType2Params->keepalive_inc;

	WMI_CHAR_ARRAY_TO_MAC_ADDR(appType2Params->gateway_mac,
						&cmd->gateway_mac);
	cmd->tcp_tx_timeout_val = appType2Params->tcp_tx_timeout_val;
	cmd->tcp_rx_timeout_val = appType2Params->tcp_rx_timeout_val;

	WMA_LOGD("%s: vdev_id %d gateway_mac %pM "
			 "rc4_key %.16s rc4_key_len %u "
			 "ip_id %x ip_device_ip %x ip_server_ip %x "
			 "tcp_src_port %u tcp_dst_port %u tcp_seq %u "
			 "tcp_ack_seq %u keepalive_init %u keepalive_min %u "
			 "keepalive_max %u keepalive_inc %u "
			 "tcp_tx_timeout_val %u tcp_rx_timeout_val %u",
		__func__, cmd->vdev_id, appType2Params->gateway_mac,
		cmd->rc4_key, cmd->rc4_key_len,
		cmd->ip_id, cmd->ip_device_ip, cmd->ip_server_ip,
		cmd->tcp_src_port, cmd->tcp_dst_port, cmd->tcp_seq,
		cmd->tcp_ack_seq, cmd->keepalive_init, cmd->keepalive_min,
		cmd->keepalive_max, cmd->keepalive_inc,
		cmd->tcp_tx_timeout_val, cmd->tcp_rx_timeout_val);


	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_EXTWOW_SET_APP_TYPE2_PARAMS_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to set APP TYPE2 PARAMS", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;

}
#endif
static void wma_update_pdev_stats(tp_wma_handle wma,
					wmi_pdev_stats *pdev_stats)
{
	tAniGetPEStatsRsp *stats_rsp_params;
	tANI_U32 temp_mask;
	tANI_U8 *stats_buf;
	tCsrGlobalClassAStatsInfo *classa_stats = NULL;
	struct wma_txrx_node *node;
	u_int8_t i;

	for (i = 0; i < wma->max_bssid; i++) {
		node = &wma->interfaces[i];
		stats_rsp_params = node->stats_rsp;
		if (stats_rsp_params) {
			node->fw_stats_set |= FW_PDEV_STATS_SET;
			WMA_LOGD("<---FW PDEV STATS received for vdevId:%d",
				i);
			stats_buf = (tANI_U8 *) (stats_rsp_params + 1);
			temp_mask = stats_rsp_params->statsMask;
			if (temp_mask & (1 << eCsrSummaryStats))
				stats_buf += sizeof(tCsrSummaryStatsInfo);

			if (temp_mask & (1 << eCsrGlobalClassAStats)) {
				classa_stats =
				       (tCsrGlobalClassAStatsInfo *) stats_buf;
				classa_stats->max_pwr = pdev_stats->chan_tx_pwr;
			}
		}
	}
}

static void wma_update_vdev_stats(tp_wma_handle wma,
					wmi_vdev_stats *vdev_stats)
{
	tAniGetPEStatsRsp *stats_rsp_params;
	tCsrSummaryStatsInfo *summary_stats = NULL;
	tANI_U8 *stats_buf;
	struct wma_txrx_node *node;
	tANI_U8 i;
	v_S7_t rssi = 0;
	tAniGetRssiReq *pGetRssiReq = (tAniGetRssiReq*)wma->pGetRssiReq;

	node = &wma->interfaces[vdev_stats->vdev_id];
	stats_rsp_params = node->stats_rsp;
	if (stats_rsp_params) {
		stats_buf = (tANI_U8 *) (stats_rsp_params + 1);
		node->fw_stats_set |=  FW_VDEV_STATS_SET;
		WMA_LOGD("<---FW VDEV STATS received for vdevId:%d",
			vdev_stats->vdev_id);
		if (stats_rsp_params->statsMask &
			(1 << eCsrSummaryStats)) {
			summary_stats = (tCsrSummaryStatsInfo *) stats_buf;
			for (i=0 ; i < 4 ; i++) {
				summary_stats->tx_frm_cnt[i] =
					vdev_stats->tx_frm_cnt[i];
				summary_stats->fail_cnt[i] =
					vdev_stats->fail_cnt[i];
				summary_stats->multiple_retry_cnt[i] =
					vdev_stats->multiple_retry_cnt[i];
			}

			summary_stats->rx_frm_cnt = vdev_stats->rx_frm_cnt;
			summary_stats->rx_error_cnt = vdev_stats->rx_err_cnt;
			summary_stats->rx_discard_cnt =
						vdev_stats->rx_discard_cnt;
			summary_stats->ack_fail_cnt = vdev_stats->ack_fail_cnt;
			summary_stats->rts_succ_cnt = vdev_stats->rts_succ_cnt;
			summary_stats->rts_fail_cnt = vdev_stats->rts_fail_cnt;
		}
	}

	if (pGetRssiReq &&
		pGetRssiReq->sessionId == vdev_stats->vdev_id) {
		if ((vdev_stats->vdev_snr.bcn_snr == WMA_TGT_INVALID_SNR) &&
			(vdev_stats->vdev_snr.dat_snr == WMA_TGT_INVALID_SNR)) {
			/*
			 * Firmware sends invalid snr till it sees
			 * Beacon/Data after connection since after
			 * vdev up fw resets the snr to invalid.
			 * In this duartion Host will return the last know
			 * rssi during connection.
			 */
			rssi = wma->first_rssi;
		} else {
			if (vdev_stats->vdev_snr.bcn_snr != WMA_TGT_INVALID_SNR) {
				rssi = vdev_stats->vdev_snr.bcn_snr;
			} else if (vdev_stats->vdev_snr.dat_snr != WMA_TGT_INVALID_SNR) {
				rssi = vdev_stats->vdev_snr.dat_snr;
			}

			/*
			 * Get the absolute rssi value from the current rssi value
			 * the sinr value is hardcoded into 0 in the core stack
			 */
			rssi = rssi + WMA_TGT_NOISE_FLOOR_DBM;
		}

		WMA_LOGD("vdev id %d beancon snr %d data snr %d",
				vdev_stats->vdev_id,
				vdev_stats->vdev_snr.bcn_snr,
				vdev_stats->vdev_snr.dat_snr);
		WMA_LOGD("Average Rssi = %d, vdev id= %d", rssi,
				pGetRssiReq->sessionId);

		/* update the average rssi value to UMAC layer */
		if (NULL != pGetRssiReq->rssiCallback) {
			((tCsrRssiCallback)(pGetRssiReq->rssiCallback))(rssi,pGetRssiReq->staId,
					 pGetRssiReq->pDevContext);
		}

		adf_os_mem_free(pGetRssiReq);
		wma->pGetRssiReq = NULL;
	}
}

static void wma_post_stats(tp_wma_handle wma, struct wma_txrx_node *node)
{
	tAniGetPEStatsRsp *stats_rsp_params;

	stats_rsp_params = node->stats_rsp;
	/* send response to UMAC*/
	wma_send_msg(wma, WDA_GET_STATISTICS_RSP, (void *)stats_rsp_params, 0) ;
	node->stats_rsp = NULL;
	node->fw_stats_set = 0;
}

static void wma_update_peer_stats(tp_wma_handle wma, wmi_peer_stats *peer_stats)
{
	tAniGetPEStatsRsp *stats_rsp_params;
	tCsrGlobalClassAStatsInfo *classa_stats = NULL;
	struct wma_txrx_node *node;
	tANI_U8 *stats_buf, vdev_id, macaddr[IEEE80211_ADDR_LEN], mcsRateFlags;
	tANI_U32 temp_mask;

	WMI_MAC_ADDR_TO_CHAR_ARRAY(&peer_stats->peer_macaddr, &macaddr[0]);
	if (!wma_find_vdev_by_bssid(wma, macaddr, &vdev_id))
		return;

	node = &wma->interfaces[vdev_id];
	if (node->stats_rsp) {
		node->fw_stats_set |=  FW_PEER_STATS_SET;
		WMA_LOGD("<-- FW PEER STATS received for vdevId:%d", vdev_id);
		stats_rsp_params = (tAniGetPEStatsRsp *) node->stats_rsp;
		stats_buf = (tANI_U8 *) (stats_rsp_params + 1);
		temp_mask = stats_rsp_params->statsMask;
		if (temp_mask & (1 << eCsrSummaryStats))
			stats_buf += sizeof(tCsrSummaryStatsInfo);

		if (temp_mask & (1 << eCsrGlobalClassAStats)) {
			classa_stats = (tCsrGlobalClassAStatsInfo *) stats_buf;
			WMA_LOGD("peer tx rate:%d", peer_stats->peer_tx_rate);
			/*The linkspeed returned by fw is in kbps so convert
			 *it in to units of 500kbps which is expected by UMAC*/
			if (peer_stats->peer_tx_rate) {
				classa_stats->tx_rate =
					peer_stats->peer_tx_rate/500;
			}

			classa_stats->tx_rate_flags = node->rate_flags;
                        if (!(node->rate_flags & eHAL_TX_RATE_LEGACY)) {
				classa_stats->mcs_index =
					wma_get_mcs_idx((peer_stats->peer_tx_rate/100),
							node->rate_flags,
							node->nss,
							&mcsRateFlags);
				/* rx_frag_cnt and promiscuous_rx_frag_cnt
				 * parameter is currently not used. lets use the
				 * same parameter to hold the nss value and mcs
				 * rate flags */
				classa_stats->rx_frag_cnt = node->nss;
				classa_stats->promiscuous_rx_frag_cnt = mcsRateFlags;
				WMA_LOGD("Computed mcs_idx:%d mcs_rate_flags:%d",
						classa_stats->mcs_index,
						mcsRateFlags);
			}
			/* FW returns tx power in intervals of 0.5 dBm
			   Convert it back to intervals of 1 dBm */
			classa_stats->max_pwr =
				 roundup(classa_stats->max_pwr, 2) >> 1;
			WMA_LOGD("peer tx rate flags:%d nss:%d max_txpwr:%d",
					node->rate_flags, node->nss,
					classa_stats->max_pwr);
		}

		if (node->fw_stats_set & FW_STATS_SET) {
			WMA_LOGD("<--STATS RSP VDEV_ID:%d", vdev_id);
			wma_post_stats(wma, node);
		}
	}
}

static void wma_post_link_status(tAniGetLinkStatus *pGetLinkStatus,
					u_int8_t link_status)
{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	vos_msg_t sme_msg = {0} ;

	pGetLinkStatus->linkStatus = link_status;
	sme_msg.type = eWNI_SME_LINK_STATUS_IND;
	sme_msg.bodyptr = pGetLinkStatus;
	sme_msg.bodyval = 0;

	vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &sme_msg);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
	    WMA_LOGE("%s: Fail to post link status ind msg", __func__);
	    vos_mem_free(pGetLinkStatus);
	}
}

#ifdef WLAN_FEATURE_MEMDUMP
/**
 * wma_fw_mem_dump_rsp() - send fw mem dump response to SME
 *
 * @req_id - request id.
 * @status - copy status from the firmware.
 *
 * This function is called by the memory dump response handler to
 * indicate SME that firmware dump copy is complete
 */
static VOS_STATUS wma_fw_mem_dump_rsp(uint32_t req_id, uint32_t status)
{
	struct fw_dump_rsp *dump_rsp;
	vos_msg_t sme_msg = {0} ;
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

	dump_rsp = vos_mem_malloc(sizeof(*dump_rsp));

	if (!dump_rsp) {
		WMA_LOGE(FL("Memory allocation failed."));
		vos_status = VOS_STATUS_E_NOMEM;
		return vos_status;
	}

	WMA_LOGI(FL("FW memory dump copy complete status: %d for request: %d"),
		 status, req_id);

	dump_rsp->request_id = req_id;
	dump_rsp->dump_complete = status;

	sme_msg.type = eWNI_SME_FW_DUMP_IND;
	sme_msg.bodyptr = dump_rsp;
	sme_msg.bodyval = 0;

	vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &sme_msg);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		WMA_LOGE(FL("Fail to post fw mem dump ind msg"));
		vos_mem_free(dump_rsp);
	}

	return vos_status;
}
#endif /* WLAN_FEATURE_MEMDUMP */

static int wma_link_status_rsp(tp_wma_handle wma, u_int8_t *buf)
{
	wmi_vdev_rate_stats_event_fixed_param *event;
	wmi_vdev_rate_ht_info *ht_info;
	struct wma_txrx_node *intr = wma->interfaces;
	u_int8_t link_status = LINK_STATUS_LEGACY;
	int i;

	event = (wmi_vdev_rate_stats_event_fixed_param *)buf;
	ht_info = (wmi_vdev_rate_ht_info *)(buf + sizeof(*event));

	WMA_LOGD("num_vdev_stats: %d", event->num_vdev_stats);
	for (i = 0; (i < event->num_vdev_stats) && ht_info; i++) {
		WMA_LOGD(
			"%s vdevId:%d  tx_nss:%d rx_nss:%d tx_preamble:%d rx_preamble:%d",
				__func__,
				ht_info->vdevid,
				ht_info->tx_nss,
				ht_info->rx_nss,
				ht_info->tx_preamble,
				ht_info->rx_preamble);
		if (ht_info->vdevid < wma->max_bssid &&
			intr[ht_info->vdevid].plink_status_req) {
			if (ht_info->tx_nss || ht_info->rx_nss)
				link_status = LINK_STATUS_MIMO;

			if ((ht_info->tx_preamble == LINK_RATE_VHT) ||
				(ht_info->rx_preamble == LINK_RATE_VHT))
				link_status |= LINK_STATUS_VHT;

			if (intr[ht_info->vdevid].nss == 2)
				link_status |= LINK_SUPPORT_MIMO;

			if (intr[ht_info->vdevid].rate_flags &
				(eHAL_TX_RATE_VHT20 | eHAL_TX_RATE_VHT40 |
				eHAL_TX_RATE_VHT80))
				link_status |= LINK_SUPPORT_VHT;

			wma_post_link_status(intr[ht_info->vdevid].plink_status_req,
						link_status);
			intr[ht_info->vdevid].plink_status_req = NULL;
			link_status = LINK_STATUS_LEGACY;
		}

		ht_info++;
	}

	return 0;
}

static int wma_link_status_event_handler(void *handle, u_int8_t *cmd_param_info,
				   u_int32_t len)
{
	WMI_UPDATE_VDEV_RATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_vdev_rate_stats_event_fixed_param *event;
	vos_msg_t vos_msg = {0};
	u_int32_t buf_size;
	u_int8_t *buf;

	param_buf =
		(WMI_UPDATE_VDEV_RATE_STATS_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_buf) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}

	event = param_buf->fixed_param;
	buf_size = sizeof(wmi_vdev_rate_stats_event_fixed_param) +
			sizeof(wmi_vdev_rate_ht_info) * event->num_vdev_stats;
	buf = vos_mem_malloc(buf_size);
	if (!buf) {
		WMA_LOGE("%s: Failed alloc memory for buf", __func__);
		return -ENOMEM;
	}

	vos_mem_zero(buf, buf_size);
	vos_mem_copy(buf, param_buf->fixed_param,
			sizeof(wmi_vdev_rate_stats_event_fixed_param));
	vos_mem_copy((buf + sizeof(wmi_vdev_rate_stats_event_fixed_param)),
			param_buf->ht_info,
			sizeof(wmi_vdev_rate_ht_info) * event->num_vdev_stats);

	vos_msg.type = WDA_GET_LINK_STATUS_RSP_IND;
	vos_msg.bodyptr = buf;
	vos_msg.bodyval = 0;

	if (VOS_STATUS_SUCCESS !=
	    vos_mq_post_message(VOS_MQ_ID_WDA, &vos_msg)) {
		WMA_LOGP("%s: Failed to post WDA_GET_LINK_STATUS_RSP_IND msg",
				__func__);
		vos_mem_free(buf);
		return -1;
	}
	WMA_LOGD("posted WDA_GET_LINK_STATUS_RSP_IND");

	return 0;
}

static int wma_stats_event_handler(void *handle, u_int8_t *cmd_param_info,
				   u_int32_t len)
{
	WMI_UPDATE_STATS_EVENTID_param_tlvs *param_buf;
	wmi_stats_event_fixed_param *event;
	vos_msg_t vos_msg = {0};
	u_int32_t buf_size;
	u_int8_t *buf;

	param_buf = (WMI_UPDATE_STATS_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_buf) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	buf_size = sizeof(*event) +
		   (event->num_pdev_stats * sizeof(wmi_pdev_stats)) +
		   (event->num_vdev_stats * sizeof(wmi_vdev_stats)) +
		   (event->num_peer_stats * sizeof(wmi_peer_stats));
	buf = vos_mem_malloc(buf_size);
	if (!buf) {
		WMA_LOGE("%s: Failed alloc memory for buf", __func__);
		return -ENOMEM;
	}
	vos_mem_zero(buf, buf_size);
	vos_mem_copy(buf, event, sizeof(*event));
	vos_mem_copy(buf + sizeof(*event), (u_int8_t *)param_buf->data,
		     (buf_size - sizeof(*event)));
	vos_msg.type = WDA_FW_STATS_IND;
	vos_msg.bodyptr = buf;
	vos_msg.bodyval = 0;

	if (VOS_STATUS_SUCCESS !=
	    vos_mq_post_message(VOS_MQ_ID_WDA, &vos_msg)) {
		WMA_LOGP("%s: Failed to post WDA_FW_STATS_IND msg", __func__);
		vos_mem_free(buf);
		return -1;
	}
	WMA_LOGD("WDA_FW_STATS_IND posted");
	return 0;
}

static VOS_STATUS wma_send_link_speed(u_int32_t link_speed)
{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	vos_msg_t sme_msg = {0} ;
	tSirLinkSpeedInfo *ls_ind =
		(tSirLinkSpeedInfo *) vos_mem_malloc(sizeof(tSirLinkSpeedInfo));
	if (!ls_ind) {
		WMA_LOGE("%s: Memory allocation failed.", __func__);
		vos_status = VOS_STATUS_E_NOMEM;
	}
	else
	{
		ls_ind->estLinkSpeed = link_speed;
		sme_msg.type = eWNI_SME_LINK_SPEED_IND;
		sme_msg.bodyptr = ls_ind;
		sme_msg.bodyval = 0;

		vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &sme_msg);
		if (!VOS_IS_STATUS_SUCCESS(vos_status) ) {
		    WMA_LOGE("%s: Fail to post linkspeed ind  msg", __func__);
		    vos_mem_free(ls_ind);
		}
	}
	return vos_status;
}

static int wma_link_speed_event_handler(void *handle, u_int8_t *cmd_param_info,
					u_int32_t len)
{
	WMI_PEER_ESTIMATED_LINKSPEED_EVENTID_param_tlvs *param_buf;
	wmi_peer_estimated_linkspeed_event_fixed_param *event;
	VOS_STATUS vos_status;

	param_buf = (WMI_PEER_ESTIMATED_LINKSPEED_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid linkspeed event", __func__);
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	vos_status = wma_send_link_speed(event->est_linkspeed_kbps);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		return -EINVAL;
	}
	return 0;
}

static void wma_fw_stats_ind(tp_wma_handle wma, u_int8_t *buf)
{
	wmi_stats_event_fixed_param *event = (wmi_stats_event_fixed_param *)buf;
	wmi_pdev_stats *pdev_stats;
	wmi_vdev_stats *vdev_stats;
	wmi_peer_stats *peer_stats;
	u_int8_t i, *temp;

	WMA_LOGI("%s: Enter", __func__);

	temp = buf + sizeof(*event);
	WMA_LOGD("%s: num_stats: pdev: %u vdev: %u peer %u",
		 __func__, event->num_pdev_stats, event->num_vdev_stats,
		 event->num_peer_stats);
	if (event->num_pdev_stats > 0) {
		for (i = 0; i < event->num_pdev_stats; i++) {
			pdev_stats = (wmi_pdev_stats*)temp;
			wma_update_pdev_stats(wma, pdev_stats);
			temp += sizeof(wmi_pdev_stats);
		}
	}

	if (event->num_vdev_stats > 0) {
		for (i = 0; i < event->num_vdev_stats; i++) {
			vdev_stats = (wmi_vdev_stats *)temp;
			wma_update_vdev_stats(wma, vdev_stats);
			temp += sizeof(wmi_vdev_stats);
		}
	}

	if (event->num_peer_stats > 0) {
		for (i = 0; i < event->num_peer_stats; i++) {
			peer_stats = (wmi_peer_stats *)temp;
			wma_update_peer_stats(wma, peer_stats);
			temp += sizeof(wmi_peer_stats);
		}
	}

	WMA_LOGI("%s: Exit", __func__);
}

#ifdef FEATURE_WLAN_EXTSCAN
/**
 * wma_extscan_rsp_handler() - extscan rsp handler
 * @wma: WMA global handle
 * @buf: event fixed param buffer
 *
 * Return: 0 on success, error number otherwise
 */
static int wma_extscan_rsp_handler(tp_wma_handle wma, uint8_t *buf)
{
	wmi_extscan_start_stop_event_fixed_param *event;
	struct sir_extscan_generic_response   *extscan_ind;
	uint16_t event_type;
	uint8_t vdev_id;
	tpAniSirGlobal mac_ctx = (tpAniSirGlobal)vos_get_context(
					VOS_MODULE_ID_PE, wma->vos_context);
	WMA_LOGI("%s: Enter", __func__);
	if (!mac_ctx) {
		WMA_LOGE("%s: Invalid mac_ctx", __func__);
		return -EINVAL;
	}
	if (!mac_ctx->sme.pExtScanIndCb) {
		WMA_LOGE("%s: Callback not registered", __func__);
		return -EINVAL;
        }
	if (!buf) {
		WMA_LOGE("Invalid event buffer");
		return -EINVAL;
	}

	event = (wmi_extscan_start_stop_event_fixed_param *)buf;
	vdev_id = event->vdev_id;
	extscan_ind = vos_mem_malloc(sizeof(*extscan_ind));
	if (!extscan_ind) {
		WMA_LOGE("%s: extscan memory allocation failed", __func__);
		return -ENOMEM;
	}

	switch (event->command) {
	case WMI_EXTSCAN_START_CMDID:
		event_type = eSIR_EXTSCAN_START_RSP;
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;

		if (!extscan_ind->status)
		        wma->interfaces[vdev_id].extscan_in_progress = true;

		break;
	case WMI_EXTSCAN_STOP_CMDID:
		event_type = eSIR_EXTSCAN_STOP_RSP;
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;

		if (!extscan_ind->status)
		        wma->interfaces[vdev_id].extscan_in_progress = false;

		break;
	case WMI_EXTSCAN_CONFIGURE_WLAN_CHANGE_MONITOR_CMDID:
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;
		if (event->mode == WMI_EXTSCAN_MODE_STOP) {
			event_type =
				eSIR_EXTSCAN_RESET_SIGNIFICANT_WIFI_CHANGE_RSP;
		} else {
			event_type =
				eSIR_EXTSCAN_SET_SIGNIFICANT_WIFI_CHANGE_RSP;
		}
		break;
	case WMI_EXTSCAN_CONFIGURE_HOTLIST_MONITOR_CMDID:
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;
		if (event->mode == WMI_EXTSCAN_MODE_STOP) {
			event_type =
				eSIR_EXTSCAN_RESET_BSSID_HOTLIST_RSP;
		} else {
			event_type =
				eSIR_EXTSCAN_SET_BSSID_HOTLIST_RSP;
		}
		break;
	case WMI_EXTSCAN_GET_CACHED_RESULTS_CMDID:
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;
		event_type = eSIR_EXTSCAN_CACHED_RESULTS_RSP;
		break;
	case WMI_EXTSCAN_CONFIGURE_HOTLIST_SSID_MONITOR_CMDID:
		extscan_ind->status = event->status;
		extscan_ind->request_id = event->request_id;
		if (event->mode == WMI_EXTSCAN_MODE_STOP) {
			event_type =
				eSIR_EXTSCAN_RESET_SSID_HOTLIST_RSP;
		} else {
			event_type =
				eSIR_EXTSCAN_SET_SSID_HOTLIST_RSP;
		}
		break;
	default:
		WMA_LOGE("%s: Unknown event %d from target vdev_id %u",
			__func__, event->status, vdev_id);
		vos_mem_free(extscan_ind);
		return -EINVAL;
	}
	mac_ctx->sme.pExtScanIndCb(mac_ctx->hHdd,
				event_type, extscan_ind);
	WMA_LOGD("%s: sending event to umac for requestid %u with status %d",
		__func__,
		extscan_ind->request_id, extscan_ind->status);
	vos_mem_free(extscan_ind);
	return 0;
}

/**
 * wma_extscan_start_stop_event_handler() - extscan event handler
 * @handle: WMA global handle
 * @cmd_param_info: command event data
 * @len: Length of @cmd_param_info
 *
 * Return: 0 on success, error number otherwise
 */
static int wma_extscan_start_stop_event_handler(void *handle,
					uint8_t *cmd_param_info, uint32_t len)
{
	WMI_EXTSCAN_START_STOP_EVENTID_param_tlvs *param_buf;
	wmi_extscan_start_stop_event_fixed_param *event;
	u_int8_t *buf;
	int buf_len = sizeof(*event);
	vos_msg_t vos_msg = {0};

	WMA_LOGI("%s: Enter", __func__);
	param_buf = (WMI_EXTSCAN_START_STOP_EVENTID_param_tlvs *)
					cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid extscan event", __func__);
		return -EINVAL;
	}

	event = param_buf->fixed_param;
	buf = vos_mem_malloc(buf_len);
	if (!buf) {
		WMA_LOGE("%s: extscan memory allocation failed", __func__);
		return -ENOMEM;
	}

	vos_mem_zero(buf, buf_len);
	vos_mem_copy(buf, (u_int8_t *)event, buf_len);

	vos_msg.type = WDA_EXTSCAN_STATUS_IND;
	vos_msg.bodyptr = buf;
	vos_msg.bodyval = 0;

	if (VOS_STATUS_SUCCESS !=
	    vos_mq_post_message(VOS_MQ_ID_WDA, &vos_msg)) {
		WMA_LOGP("%s: Failed to post WDA_EXTSCAN_STATUS_IND msg",
			__func__);
		vos_mem_free(buf);
		return -EINVAL;
	}
	WMA_LOGI("WDA_EXTSCAN_STATUS_IND posted");

	return 0;
}

/**
 * wma_extscan_operations_ind_handler() - extscan operations handler
 * @wma: WMA global handle
 * @buf: command event data
 *
 * This function extracts the event data, fill in appropriate structures
 * and invoke upper layer callback.
 *
 * Return: 0 on success, error number otherwise
 */
static int wma_extscan_operations_ind_handler(tp_wma_handle wma, uint8_t *buf)
{
	wmi_extscan_operation_event_fixed_param *event;
	tSirExtScanOnScanEventIndParams   *oprn_ind;
	tpAniSirGlobal mac_ctx = (tpAniSirGlobal)vos_get_context(
					VOS_MODULE_ID_PE, wma->vos_context);
	if (!mac_ctx) {
		WMA_LOGE("%s: Invalid mac_ctx", __func__);
		return -EINVAL;
	}
	if (!mac_ctx->sme.pExtScanIndCb) {
		WMA_LOGE("%s: Callback not registered", __func__);
		return -EINVAL;
        }
	if (!buf) {
		WMA_LOGE("%s: Invalid event buffer", __func__);
		return -EINVAL;
	}
	event = (wmi_extscan_operation_event_fixed_param *)buf;
	oprn_ind = vos_mem_malloc(sizeof(*oprn_ind));
	if (!oprn_ind) {
		WMA_LOGE("%s: extscan memory allocation failed", __func__);
		vos_mem_free(oprn_ind);
		return -EINVAL;
	}

	oprn_ind->requestId = event->request_id;

	switch (event->event) {
	case WMI_EXTSCAN_BUCKET_COMPLETED_EVENT:
		oprn_ind->scanEventType =  WIFI_SCAN_COMPLETE;
		oprn_ind->status = 0;
		break;
	case WMI_EXTSCAN_CYCLE_STARTED_EVENT:
		WMA_LOGD("%s: received WMI_EXTSCAN_CYCLE_STARTED_EVENT",
			 __func__);
		vos_wake_lock_timeout_acquire(&wma->extscan_wake_lock,
				      WMA_EXTSCAN_CYCLE_WAKE_LOCK_DURATION,
				      WIFI_POWER_EVENT_WAKELOCK_EXT_SCAN);
		goto exit_handler;
	case WMI_EXTSCAN_CYCLE_COMPLETED_EVENT:
		WMA_LOGD("%s: received WMI_EXTSCAN_CYCLE_COMPLETED_EVENT",
			__func__);
		vos_wake_lock_release(&wma->extscan_wake_lock,
					WIFI_POWER_EVENT_WAKELOCK_EXT_SCAN);
		goto exit_handler;
	default:
		WMA_LOGE("%s: Unknown event %d from target",
			__func__, event->event);
		vos_mem_free(oprn_ind);
		return -EINVAL;
	}
	mac_ctx->sme.pExtScanIndCb(mac_ctx->hHdd,
				eSIR_EXTSCAN_SCAN_PROGRESS_EVENT_IND,
				oprn_ind);
	WMA_LOGD("%s: sending scan progress event to hdd", __func__);

exit_handler:
	vos_mem_free(oprn_ind);
	return 0;
}

/**
 * wma_extscan_operations_event_handler() - extscan operations event handler
 * @handle: WMA global handle
 * @cmd_param_info: command event data
 * @len: Length of @cmd_param_info
 *
 * Return: 0 on success, error number otherwise
 */
static int wma_extscan_operations_event_handler(void *handle,
					uint8_t *cmd_param_info, uint32_t len)
{
	WMI_EXTSCAN_OPERATION_EVENTID_param_tlvs *param_buf;
	wmi_extscan_operation_event_fixed_param *event, *buf;
	int buf_len = sizeof(*event);
	vos_msg_t vos_msg = {0};

	WMA_LOGI("%s: Enter", __func__);
	param_buf = (WMI_EXTSCAN_OPERATION_EVENTID_param_tlvs *)
					cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid extscan event", __func__);
		return -EINVAL;
	}

	event = param_buf->fixed_param;
	buf = vos_mem_malloc(buf_len);
	if (!buf) {
		WMA_LOGE("%s: extscan memory allocation failed", __func__);
		return -ENOMEM;
	}

	*buf = *event;

	vos_msg.type = WDA_EXTSCAN_OPERATION_IND;
	vos_msg.bodyptr = buf;
	vos_msg.bodyval = 0;

	if (VOS_STATUS_SUCCESS !=
	    vos_mq_post_message(VOS_MQ_ID_WDA, &vos_msg)) {
		WMA_LOGP("%s: Failed to post WDA_EXTSCAN_OPERATION_IND msg",
			__func__);
		vos_mem_free(buf);
		return -EINVAL;
	}
	WMA_LOGI("WDA_EXTSCAN_OPERATION_IND posted");

	return 0;
}


static int wma_extscan_table_usage_event_handler (void *handle,
		u_int8_t  *cmd_param_info, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	WMI_EXTSCAN_TABLE_USAGE_EVENTID_param_tlvs *param_buf;
	wmi_extscan_table_usage_event_fixed_param  *event;
	tSirExtScanResultsAvailableIndParams  *tbl_usg_ind;
	tpAniSirGlobal pMac = (tpAniSirGlobal)vos_get_context(
					VOS_MODULE_ID_PE, wma->vos_context);
	if (!pMac) {
		WMA_LOGE("%s: Invalid pMac", __func__);
		return -EINVAL;
	}
	if (!pMac->sme.pExtScanIndCb) {
		WMA_LOGE("%s: Callback not registered", __func__);
		return -EINVAL;
        }
	param_buf = (WMI_EXTSCAN_TABLE_USAGE_EVENTID_param_tlvs *)
					cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid table usage event", __func__);
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	tbl_usg_ind =  vos_mem_malloc(sizeof(*tbl_usg_ind));
	if (!tbl_usg_ind) {
		WMA_LOGE("%s: table usage allocation failed", __func__);
		return -EINVAL;
	}
	tbl_usg_ind->requestId = event->request_id;
	tbl_usg_ind->numResultsAvailable = event->entries_in_use;

	pMac->sme.pExtScanIndCb(pMac->hHdd,
				eSIR_EXTSCAN_SCAN_RES_AVAILABLE_IND,
				tbl_usg_ind);
	WMA_LOGD("%s: sending scan_res available event to hdd", __func__);
	vos_mem_free(tbl_usg_ind);
	return 0;
}

static int wma_extscan_capabilities_event_handler (void *handle,
			u_int8_t *cmd_param_info, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	WMI_EXTSCAN_CAPABILITIES_EVENTID_param_tlvs *param_buf;
	wmi_extscan_capabilities_event_fixed_param  *event;
	wmi_extscan_cache_capabilities   *src_cache;
	wmi_extscan_hotlist_monitor_capabilities  *src_hotlist;
	wmi_extscan_wlan_change_monitor_capabilities *src_change;

	struct ext_scan_capabilities_response  *dest_capab;
	tpAniSirGlobal pMac = (tpAniSirGlobal )vos_get_context(
					VOS_MODULE_ID_PE, wma->vos_context);
	if (!pMac) {
		WMA_LOGE("%s: Invalid pMac", __func__);
		return -EINVAL;
	}
	if (!pMac->sme.pExtScanIndCb) {
		WMA_LOGE("%s: Callback not registered", __func__);
		return -EINVAL;
        }
	param_buf = (WMI_EXTSCAN_CAPABILITIES_EVENTID_param_tlvs *)
					cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid capabilities event", __func__);
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	src_cache = param_buf->extscan_cache_capabilities;
	src_hotlist = param_buf->hotlist_capabilities;
	src_change = param_buf->wlan_change_capabilities;

	if (!src_cache || !src_hotlist || !src_change) {
		WMA_LOGE("%s: Invalid capabilities list", __func__);
		return -EINVAL;
	}
	dest_capab = vos_mem_malloc(sizeof(*dest_capab));
	if (!dest_capab) {
		WMA_LOGE("%s: Allocation failed for capabilities buffer",
			__func__);
		return -EINVAL;
	}
	dest_capab->requestId = event->request_id;
	dest_capab->max_scan_buckets = src_cache->max_buckets;
	dest_capab->max_scan_cache_size = src_cache->scan_cache_entry_size;
	dest_capab->max_ap_cache_per_scan = src_cache->max_bssid_per_scan;
	dest_capab->max_scan_reporting_threshold =
				src_cache->max_table_usage_threshold;

	dest_capab->max_hotlist_bssids = src_hotlist->max_hotlist_entries;
	dest_capab->max_rssi_sample_size =
				src_change->max_rssi_averaging_samples;
	dest_capab->max_bssid_history_entries =
				src_change->max_rssi_history_entries;
	dest_capab->max_significant_wifi_change_aps =
				src_change->max_wlan_change_entries;
	dest_capab->max_hotlist_ssids =
				event->num_extscan_hotlist_ssid;
	dest_capab->max_number_epno_networks =
				event->num_epno_networks;
	dest_capab->max_number_epno_networks_by_ssid =
				event->num_epno_networks;
	dest_capab->max_number_of_white_listed_ssid =
				event->num_roam_ssid_whitelist;
	dest_capab->status = 0;

	WMA_LOGD("%s: request_id: %u status: %d",
		__func__, dest_capab->requestId, dest_capab->status);

	WMA_LOGD("%s: Capabilities: max_scan_buckets: %d,"
		"max_hotlist_bssids: %d, max_scan_cache_size: %d,"
		"max_ap_cache_per_scan: %d, max_scan_reporting_threshold: %d,"
		"max_rssi_sample_size: %d, max_bssid_history_entries: %d,"
		"max_significant_wifi_change_aps: %d",
		 __func__, dest_capab->max_scan_buckets,
		dest_capab->max_hotlist_bssids,
		dest_capab->max_scan_cache_size,
		dest_capab->max_ap_cache_per_scan,
		dest_capab->max_scan_reporting_threshold,
		dest_capab->max_rssi_sample_size,
		dest_capab->max_bssid_history_entries,
		dest_capab->max_significant_wifi_change_aps);
	WMA_LOGD("%s: Capabilities: max_hotlist_ssids: %d,"
		 "max_number_epno_networks: %d, max_number_epno_networks_by_ssid: %d,"
		 "max_number_of_white_listed_ssid: %d",
		 __func__, dest_capab->max_hotlist_ssids,
		dest_capab->max_number_epno_networks,
		dest_capab->max_number_epno_networks_by_ssid,
		dest_capab->max_number_of_white_listed_ssid);

	pMac->sme.pExtScanIndCb(pMac->hHdd,
				eSIR_EXTSCAN_GET_CAPABILITIES_IND,
				dest_capab);
	WMA_LOGD("%s: sending capabilities event to hdd", __func__);
	vos_mem_free(dest_capab);
	return 0;
}

static int wma_extscan_hotlist_match_event_handler(void *handle,
		      u_int8_t  *cmd_param_info, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_EXTSCAN_HOTLIST_MATCH_EVENTID_param_tlvs *param_buf;
	wmi_extscan_hotlist_match_event_fixed_param  *event;
	struct extscan_hotlist_match  *dest_hotlist;
	tSirWifiScanResult      *dest_ap;
	wmi_extscan_wlan_descriptor    *src_hotlist;
	int numap, j, ap_found = 0;

	tpAniSirGlobal pMac = (tpAniSirGlobal )vos_get_context(
					VOS_MODULE_ID_PE, wma->vos_context);
	if (!pMac) {
		WMA_LOGE("%s: Invalid pMac", __func__);
		return -EINVAL;
	}
	if (!pMac->sme.pExtScanIndCb) {
		WMA_LOGE("%s: Callback not registered", __func__);
		return -EINVAL;
        }
	param_buf = (WMI_EXTSCAN_HOTLIST_MATCH_EVENTID_param_tlvs *)
					cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid hotlist match event", __func__);
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	src_hotlist = param_buf->hotlist_match;
	numap = event->total_entries;

	if (!src_hotlist || !numap) {
		WMA_LOGE("%s: Hotlist AP's list invalid", __func__);
		return -EINVAL;
	}
	dest_hotlist = vos_mem_malloc(sizeof(*dest_hotlist) +
					sizeof(*dest_ap) * numap);
	if (!dest_hotlist) {
		WMA_LOGE("%s: Allocation failed for hotlist buffer", __func__);
		return -EINVAL;
	}
	dest_ap = &dest_hotlist->ap[0];
	dest_hotlist->numOfAps = event->total_entries;
	dest_hotlist->requestId = event->config_request_id;
	if (event->first_entry_index +
		event->num_entries_in_page < event->total_entries)
		dest_hotlist->moreData = 1;
	else
		dest_hotlist->moreData = 0;

	WMA_LOGD("%s: Hotlist match: requestId: %u,"
		"numOfAps: %d", __func__,
		 dest_hotlist->requestId, dest_hotlist->numOfAps);

	/*
	 * Currently firmware sends only one bss information in-case
	 * of both hotlist ap found and lost.
	 */
	for (j = 0; j < numap; j++) {
		dest_ap->rssi = 0;
		dest_ap->channel = src_hotlist->channel;
		dest_ap->ts = src_hotlist->tstamp;
		ap_found = src_hotlist->flags & WMI_HOTLIST_FLAG_PRESENCE;
		dest_ap->rtt = src_hotlist->rtt;
		dest_ap->rtt_sd = src_hotlist->rtt_sd;
		dest_ap->beaconPeriod = src_hotlist->beacon_interval;
		dest_ap->capability = src_hotlist->capabilities;
		dest_ap->ieLength = src_hotlist-> ie_length;
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&src_hotlist->bssid,
						dest_ap->bssid);
		vos_mem_copy(dest_ap->ssid, src_hotlist->ssid.ssid,
					src_hotlist->ssid.ssid_len);
		dest_ap->ssid[src_hotlist->ssid.ssid_len] = '\0';
		dest_ap++;
		src_hotlist++;
	}
	dest_hotlist->ap_found = ap_found;
	pMac->sme.pExtScanIndCb(pMac->hHdd,
				eSIR_EXTSCAN_HOTLIST_MATCH_IND,
				dest_hotlist);
	WMA_LOGD("%s: sending hotlist match event to hdd", __func__);
	vos_mem_free(dest_hotlist);
	return 0;
}

/** wma_extscan_find_unique_scan_ids() - find unique scan ids
 * @cmd_param_info: event data.
 *
 * This utility function parses the input bss table of information
 * and find the unique number of scan ids
 *
 * Return: 0 on success; error number otherwise
 */
static int wma_extscan_find_unique_scan_ids(const u_int8_t *cmd_param_info)
{
	WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *param_buf;
	wmi_extscan_cached_results_event_fixed_param  *event;
	wmi_extscan_wlan_descriptor  *src_hotlist;
	wmi_extscan_rssi_info  *src_rssi;
	int prev_scan_id, scan_ids_cnt, i;

	param_buf = (WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *)
						cmd_param_info;
	event = param_buf->fixed_param;
	src_hotlist = param_buf->bssid_list;
	src_rssi = param_buf->rssi_list;

	/* Find the unique number of scan_id's for grouping */
	prev_scan_id = src_rssi->scan_cycle_id;
	scan_ids_cnt = 1;
	for (i = 1; i < event->num_entries_in_page; i++) {
		src_rssi++;

		if (prev_scan_id != src_rssi->scan_cycle_id) {
			scan_ids_cnt++;
			prev_scan_id = src_rssi->scan_cycle_id;
		}
	}

	return scan_ids_cnt;
}

/** wma_fill_num_results_per_scan_id() - fill number of bss per scan id
 * @cmd_param_info: event data.
 * @scan_id_group: pointer to scan id group.
 *
 * This utility function parses the input bss table of information
 * and finds how many bss are there per unique scan id.
 *
 * Return: 0 on success; error number otherwise
 */
static int wma_fill_num_results_per_scan_id(const u_int8_t *cmd_param_info,
			struct extscan_cached_scan_result *scan_id_group)
{
	WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *param_buf;
	wmi_extscan_cached_results_event_fixed_param  *event;
	wmi_extscan_wlan_descriptor  *src_hotlist;
	wmi_extscan_rssi_info  *src_rssi;
	struct extscan_cached_scan_result *t_scan_id_grp;
	int i, prev_scan_id;

	param_buf = (WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *)
						cmd_param_info;
	event = param_buf->fixed_param;
	src_hotlist = param_buf->bssid_list;
	src_rssi = param_buf->rssi_list;
	t_scan_id_grp = scan_id_group;

	prev_scan_id = src_rssi->scan_cycle_id;

	t_scan_id_grp->scan_id = src_rssi->scan_cycle_id;
	t_scan_id_grp->flags = src_rssi->flags;
	t_scan_id_grp->num_results = 1;
	for (i = 1; i < event->num_entries_in_page; i++) {
		src_rssi++;
		if (prev_scan_id == src_rssi->scan_cycle_id) {
			t_scan_id_grp->num_results++;
		} else {
			t_scan_id_grp++;
			prev_scan_id = t_scan_id_grp->scan_id =
				src_rssi->scan_cycle_id;
			t_scan_id_grp->flags = src_rssi->flags;
			t_scan_id_grp->num_results = 1;
		}
	}
	return 0;
}

/** wma_group_num_bss_to_scan_id() - group bss to scan id table
 * @cmd_param_info: event data.
 * @cached_result: pointer to cached table.
 *
 * This function reads the bss information from the format
 * ------------------------------------------------------------------------
 * | bss info {rssi, channel, ssid, bssid, timestamp} | scan id_1 | flags |
 * | bss info {rssi, channel, ssid, bssid, timestamp} | scan id_2 | flags |
 * ........................................................................
 * | bss info {rssi, channel, ssid, bssid, timestamp} | scan id_N | flags |
 * ------------------------------------------------------------------------
 *
 * and converts it into the below format and store it
 *
 * ------------------------------------------------------------------------
 * | scan id_1 | -> bss info_1 -> bss info_2 -> .... bss info_M1
 * | scan id_2 | -> bss info_1 -> bss info_2 -> .... bss info_M2
 * ......................
 * | scan id_N | -> bss info_1 -> bss info_2 -> .... bss info_Mn
 * ------------------------------------------------------------------------
 *
 * Return: 0 on success; error number otherwise
 */
static int wma_group_num_bss_to_scan_id(const u_int8_t *cmd_param_info,
			struct extscan_cached_scan_results *cached_result)
{
	WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *param_buf;
	wmi_extscan_cached_results_event_fixed_param  *event;
	wmi_extscan_wlan_descriptor  *src_hotlist;
	wmi_extscan_rssi_info  *src_rssi;
	struct extscan_cached_scan_results *t_cached_result;
	struct extscan_cached_scan_result *t_scan_id_grp;
	int i, j;
	tSirWifiScanResult *ap;

	param_buf = (WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *)
						cmd_param_info;
	event = param_buf->fixed_param;
	src_hotlist = param_buf->bssid_list;
	src_rssi = param_buf->rssi_list;
	t_cached_result = cached_result;
	t_scan_id_grp = &t_cached_result->result[0];

	WMA_LOGD("%s: num_scan_ids:%d", __func__,
			t_cached_result->num_scan_ids);
	for (i = 0; i < t_cached_result->num_scan_ids; i++) {
		WMA_LOGD("%s: num_results:%d", __func__,
			t_scan_id_grp->num_results);
		t_scan_id_grp->ap = vos_mem_malloc(t_scan_id_grp->num_results *
						sizeof(*ap));
		if (!t_scan_id_grp->ap) {
			WMA_LOGD("%s: vos_mem_malloc failed", __func__);
			return -ENOMEM;
		}

		ap = &t_scan_id_grp->ap[0];
		for (j = 0; j < t_scan_id_grp->num_results; j++) {
			ap->channel = src_hotlist->channel;
			ap->ts = WMA_MSEC_TO_USEC(src_rssi->tstamp);
			ap->rtt = src_hotlist->rtt;
			ap->rtt_sd = src_hotlist->rtt_sd;
			ap->beaconPeriod = src_hotlist->beacon_interval;
			ap->capability = src_hotlist->capabilities;
			ap->ieLength = src_hotlist->ie_length;

			/* Firmware already applied noise floor adjustment and
			 * due to WMI interface "UINT32 rssi", host driver
			 * receives a positive value, hence convert to
			 * signed char to get the absolute rssi.
			 */
			ap->rssi = (signed char) src_rssi->rssi;
			WMI_MAC_ADDR_TO_CHAR_ARRAY(&src_hotlist->bssid,
						ap->bssid);

			vos_mem_copy(ap->ssid, src_hotlist->ssid.ssid,
					src_hotlist->ssid.ssid_len);
			ap->ssid[src_hotlist->ssid.ssid_len] = '\0';
			ap++;
			src_rssi++;
			src_hotlist++;
		}
		t_scan_id_grp++;
	}
	return 0;
}

static int wma_extscan_cached_results_event_handler(void *handle,
		     u_int8_t  *cmd_param_info, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *param_buf;
	wmi_extscan_cached_results_event_fixed_param  *event;
	struct extscan_cached_scan_results *dest_cachelist;
	struct extscan_cached_scan_result *dest_result;
	struct extscan_cached_scan_results empty_cachelist;
	wmi_extscan_wlan_descriptor  *src_hotlist;
	wmi_extscan_rssi_info  *src_rssi;
	int numap, i, moredata, scan_ids_cnt;
	int buf_len;

	tpAniSirGlobal pMac = (tpAniSirGlobal )vos_get_context(
					VOS_MODULE_ID_PE, wma->vos_context);
	if (!pMac) {
		WMA_LOGE("%s: Invalid pMac", __func__);
		return -EINVAL;
	}
	if (!pMac->sme.pExtScanIndCb) {
		WMA_LOGE("%s: Callback not registered", __func__);
		return -EINVAL;
        }
	param_buf = (WMI_EXTSCAN_CACHED_RESULTS_EVENTID_param_tlvs *)
						cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid cached results event", __func__);
		return -EINVAL;
	}

	event = param_buf->fixed_param;
	src_hotlist = param_buf->bssid_list;
	src_rssi = param_buf->rssi_list;
	numap = event->num_entries_in_page;
	WMA_LOGI("Total_entries %u first_entry_index %u", event->total_entries,
			event->first_entry_index);
	WMA_LOGI("num_entries_in_page %d", numap);
	if (!src_hotlist || !src_rssi || !numap) {
		WMA_LOGE("%s: Cached results empty, send 0 results", __func__);
		goto noresults;
        }

	if (event->first_entry_index +
		event->num_entries_in_page < event->total_entries)
		moredata = 1;
	else
		moredata = 0;

	dest_cachelist = vos_mem_malloc(sizeof(*dest_cachelist));
	if (!dest_cachelist) {
		WMA_LOGE("%s: vos_mem_malloc failed", __func__);
		return -ENOMEM;
	}
	vos_mem_zero(dest_cachelist, sizeof(*dest_cachelist));
	dest_cachelist->request_id = event->request_id;
	dest_cachelist->more_data = moredata;

	scan_ids_cnt = wma_extscan_find_unique_scan_ids(cmd_param_info);
	WMA_LOGI("scan_ids_cnt %d", scan_ids_cnt);
	dest_cachelist->num_scan_ids = scan_ids_cnt;

	buf_len = sizeof(*dest_result) * scan_ids_cnt;
	dest_cachelist->result = vos_mem_malloc(buf_len);
	if (!dest_cachelist->result) {
		WMA_LOGE("%s: vos_mem_malloc failed", __func__);
		vos_mem_free(dest_cachelist);
		return -ENOMEM;
	}

	dest_result = dest_cachelist->result;
	wma_fill_num_results_per_scan_id(cmd_param_info, dest_result);
	wma_group_num_bss_to_scan_id(cmd_param_info, dest_cachelist);

	pMac->sme.pExtScanIndCb(pMac->hHdd,
				eSIR_EXTSCAN_CACHED_RESULTS_IND,
				dest_cachelist);
	WMA_LOGI("%s: sending cached results event", __func__);

	dest_result = dest_cachelist->result;
	for (i = 0; i < dest_cachelist->num_scan_ids; i++) {
		vos_mem_free(dest_result->ap);
		dest_result++;
	}
	vos_mem_free(dest_cachelist->result);
	vos_mem_free(dest_cachelist);
	return 0;

noresults:
        empty_cachelist.request_id = event->request_id;
        empty_cachelist.more_data = 0;
        empty_cachelist.num_scan_ids = 0;

	pMac->sme.pExtScanIndCb(pMac->hHdd,
				eSIR_EXTSCAN_CACHED_RESULTS_IND,
				&empty_cachelist);
	WMA_LOGI("%s: sending cached results event", __func__);
	return 0;
}

static int wma_extscan_change_results_event_handler(void *handle,
				u_int8_t  *cmd_param_info, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID_param_tlvs *param_buf;
	wmi_extscan_wlan_change_results_event_fixed_param  *event;
	tSirWifiSignificantChangeEvent  *dest_chglist;
	tSirWifiSignificantChange  *dest_ap;
	wmi_extscan_wlan_change_result_bssid    *src_chglist;

	int numap;
	int i, k;
	u_int8_t *src_rssi;
	int count = 0;
	int moredata;
	int rssi_num = 0;
	tpAniSirGlobal pMac = (tpAniSirGlobal )vos_get_context(
					VOS_MODULE_ID_PE, wma->vos_context);
	if (!pMac) {
		WMA_LOGE("%s: Invalid pMac", __func__);
		return -EINVAL;
	}
	if (!pMac->sme.pExtScanIndCb) {
		WMA_LOGE("%s: Callback not registered", __func__);
		return -EINVAL;
        }
	param_buf = (WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID_param_tlvs *)
						cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid change monitor event", __func__);
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	src_chglist = param_buf->bssid_signal_descriptor_list;
	src_rssi = param_buf->rssi_list;
	numap = event->num_entries_in_page;

	if (!src_chglist || !numap) {
		WMA_LOGE("%s: Results invalid", __func__);
		return -EINVAL;
	}
	for (i = 0; i < numap; i++) {
		rssi_num += src_chglist->num_rssi_samples;
	}
	if (event->first_entry_index +
		event->num_entries_in_page < event->total_entries)
		moredata = 1;
	else
		moredata = 0;

	dest_chglist = vos_mem_malloc(sizeof(*dest_chglist) +
			sizeof(*dest_ap) * numap +
			sizeof(tANI_S32) * rssi_num);
	if (!dest_chglist) {
		WMA_LOGE("%s: vos_mem_malloc failed", __func__);
		return -EINVAL;
	}
	dest_ap = &dest_chglist->ap[0];
	for (i = 0; i < numap; i++) {
		dest_ap->channel = src_chglist->channel;
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&src_chglist->bssid,
						dest_ap->bssid);
		dest_ap->numOfRssi =
				src_chglist->num_rssi_samples;
		if (dest_ap->numOfRssi) {
			for (k = 0; k < dest_ap->numOfRssi; k++) {
				dest_ap->rssi[k] = WMA_TGT_NOISE_FLOOR_DBM +
							src_rssi[count++];
			}
		}
		dest_ap += dest_ap->numOfRssi * sizeof(tANI_S32);
		src_chglist++;
	}
	dest_chglist->requestId = event->request_id;
	dest_chglist->moreData = moredata;
	dest_chglist->numResults = event->total_entries;

	pMac->sme.pExtScanIndCb(pMac->hHdd,
				eSIR_EXTSCAN_SIGNIFICANT_WIFI_CHANGE_RESULTS_IND,
				dest_chglist);
	WMA_LOGD("%s: sending change monitor results", __func__);
	vos_mem_free(dest_chglist);
	return 0;
}

/**
 * wma_passpoint_match_event_handler() - passpoint match found event handler
 * @handle: WMA handle
 * @cmd_param_info: event data
 * @len: event data length
 *
 * This is the passpoint match found event handler; it reads event data from
 * @cmd_param_info and fill in the destination buffer and sends indication
 * up layer.
 *
 * Return: 0 on success; error number otherwise
 */
static int wma_passpoint_match_event_handler(void *handle,
					     uint8_t  *cmd_param_info,
					     uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_PASSPOINT_MATCH_EVENTID_param_tlvs *param_buf;
	wmi_passpoint_event_hdr  *event;
	struct wifi_passpoint_match  *dest_match;
	tSirWifiScanResult      *dest_ap;
	uint8_t *buf_ptr;

	tpAniSirGlobal pMac = (tpAniSirGlobal )vos_get_context(
					VOS_MODULE_ID_PE, wma->vos_context);
	if (!pMac) {
		WMA_LOGE("%s: Invalid pMac", __func__);
		return -EINVAL;
	}
	if (!pMac->sme.pExtScanIndCb) {
		WMA_LOGE("%s: Callback not registered", __func__);
		return -EINVAL;
        }
	param_buf = (WMI_PASSPOINT_MATCH_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid passpoint match event", __func__);
		return -EINVAL;
	}
	event = param_buf->fixed_param;
	buf_ptr = (uint8_t *)param_buf->fixed_param;

	dest_match = vos_mem_malloc(sizeof(*dest_match) +
				event->ie_length + event->anqp_length);
	if (!dest_match) {
		WMA_LOGE("%s: vos_mem_malloc failed", __func__);
		return -EINVAL;
	}
	dest_ap = &dest_match->ap;
	dest_match->request_id = 0;
	dest_match->id = event->id;
	dest_match->anqp_len = event->anqp_length;
	WMA_LOGI("%s: passpoint match: id: %u anqp length %u", __func__,
		 dest_match->id, dest_match->anqp_len);

	dest_ap->channel = event->channel_mhz;
	dest_ap->ts = event->timestamp;
	dest_ap->rtt = event->rtt;
	dest_ap->rssi = event->rssi;
	dest_ap->rtt_sd = event->rtt_sd;
	dest_ap->beaconPeriod = event->beacon_period;
	dest_ap->capability = event->capability;
	dest_ap->ieLength = event->ie_length;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&event->bssid, dest_ap->bssid);
	vos_mem_copy(dest_ap->ssid, event->ssid.ssid,
				event->ssid.ssid_len);
	dest_ap->ssid[event->ssid.ssid_len] = '\0';
	vos_mem_copy(dest_ap->ieData, buf_ptr + sizeof(*event) +
			WMI_TLV_HDR_SIZE, dest_ap->ieLength);
	vos_mem_copy(dest_match->anqp, buf_ptr + sizeof(*event) +
			WMI_TLV_HDR_SIZE + dest_ap->ieLength,
			dest_match->anqp_len);

	pMac->sme.pExtScanIndCb(pMac->hHdd,
				eSIR_PASSPOINT_NETWORK_FOUND_IND,
				dest_match);
	WMA_LOGD("%s: sending passpoint match event to hdd", __func__);
	vos_mem_free(dest_match);
	return 0;
}

/**
 * wma_extscan_hotlist_ssid_match_event_handler() -
 *	Handler for SSID hotlist match event from firmware
 * @handle: WMA handle
 * @cmd_param_info: WMI command buffer
 * @len: length of @cmd_param_info
 *
 * Return: 0 on success, non-zero on failure
 */
static int
wma_extscan_hotlist_ssid_match_event_handler(void *handle,
					     uint8_t *cmd_param_info,
					     uint32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_EXTSCAN_HOTLIST_SSID_MATCH_EVENTID_param_tlvs *param_buf;
	wmi_extscan_hotlist_ssid_match_event_fixed_param  *event;
	tSirWifiScanResultEvent  *dest_hotlist;
	tSirWifiScanResult      *dest_ap;
	wmi_extscan_wlan_descriptor    *src_hotlist;
	int numap, j;
	bool ssid_found = false;
	tpAniSirGlobal mac =
		vos_get_context(VOS_MODULE_ID_PE, wma->vos_context);

	if (!mac) {
		WMA_LOGE("%s: Invalid mac", __func__);
		return -EINVAL;
	}

	if (!mac->sme.pExtScanIndCb) {
		WMA_LOGE("%s: Callback not registered", __func__);
		return -EINVAL;
	}

	param_buf = (WMI_EXTSCAN_HOTLIST_SSID_MATCH_EVENTID_param_tlvs *)
					cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid hotlist match event", __func__);
		return -EINVAL;
	}

	event = param_buf->fixed_param;
	src_hotlist = param_buf->hotlist_ssid_match;
	numap = event->total_entries;
	if (!src_hotlist || !numap) {
		WMA_LOGE("%s: Hotlist AP's list invalid", __func__);
		return -EINVAL;
	}

	dest_hotlist = vos_mem_malloc(sizeof(*dest_hotlist) +
					sizeof(*dest_ap) * numap);
	if (!dest_hotlist) {
		WMA_LOGE("%s: Allocation failed for hotlist buffer", __func__);
		return -EINVAL;
	}

	dest_ap = &dest_hotlist->ap[0];
	dest_hotlist->numOfAps = event->total_entries;
	dest_hotlist->requestId = event->config_request_id;

	if (event->first_entry_index +
		event->num_entries_in_page < event->total_entries)
		dest_hotlist->moreData = 1;
	else
		dest_hotlist->moreData = 0;

	WMA_LOGD("%s: Hotlist match: requestId: %u, numOfAps: %d", __func__,
		 dest_hotlist->requestId, dest_hotlist->numOfAps);

	for (j = 0; j < numap; j++) {
		dest_ap->channel = src_hotlist->channel;
		dest_ap->ts = src_hotlist->tstamp;
		ssid_found = src_hotlist->flags & WMI_HOTLIST_FLAG_PRESENCE;
		dest_ap->rtt = src_hotlist->rtt;
		dest_ap->rtt_sd = src_hotlist->rtt_sd;
		dest_ap->beaconPeriod = src_hotlist->beacon_interval;
		dest_ap->capability = src_hotlist->capabilities;
		dest_ap->ieLength = src_hotlist-> ie_length;
		WMI_MAC_ADDR_TO_CHAR_ARRAY(&src_hotlist->bssid,
						dest_ap->bssid);
		vos_mem_copy(dest_ap->ssid, src_hotlist->ssid.ssid,
					src_hotlist->ssid.ssid_len);
		dest_ap->ssid[src_hotlist->ssid.ssid_len] = '\0';
		dest_ap++;
		src_hotlist++;
	}

	dest_hotlist->ap_found = ssid_found;
	mac->sme.pExtScanIndCb(mac->hHdd,
			       eSIR_EXTSCAN_HOTLIST_SSID_MATCH_IND,
			       dest_hotlist);
	WMA_LOGD("%s: sending hotlist ssid match event", __func__);
	vos_mem_free(dest_hotlist);
	return 0;
}
#endif

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
static int wma_unified_link_iface_stats_event_handler(void *handle,
			u_int8_t *cmd_param_info, u_int32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_IFACE_LINK_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_iface_link_stats_event_fixed_param *fixed_param;
	wmi_iface_link_stats *link_stats;
	wmi_wmm_ac_stats *ac_stats;
	tSirLLStatsResults *link_stats_results;
	u_int8_t *results, *t_link_stats, *t_ac_stats;
	u_int32_t next_res_offset, next_ac_offset, count;
	u_int32_t roaming_offset , roaming_size;
	size_t link_stats_size, ac_stats_size, iface_info_size;
	size_t link_stats_results_size;

	tpAniSirGlobal pMac = (tpAniSirGlobal )vos_get_context(VOS_MODULE_ID_PE,
				wma_handle->vos_context);

	if (!pMac) {
		WMA_LOGD("%s: NULL pMac ptr. Exiting", __func__);
		return -EINVAL;
	}

	if (!pMac->sme.pLinkLayerStatsIndCallback) {
		WMA_LOGD("%s: HDD callback is null", __func__);
		return -EINVAL;
	}

	WMA_LOGD("%s: Posting Iface Stats event to HDD", __func__);
	param_tlvs = (WMI_IFACE_LINK_STATS_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_tlvs) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}

	/*
	 * cmd_param_info contains
	 * wmi_iface_link_stats_event_fixed_param fixed_param;
	 * wmi_iface_link_stats iface_link_stats;
	 * iface_link_stats->num_ac * size of(struct wmi_wmm_ac_stats)
	 */
	fixed_param = param_tlvs->fixed_param;
	link_stats  = param_tlvs->iface_link_stats;
	ac_stats    = param_tlvs->ac;

	if (!fixed_param || !link_stats || (link_stats->num_ac && !ac_stats)) {
		WMA_LOGA("%s: Invalid param_tlvs for Iface Stats", __func__);
		return -EINVAL;
	}

	link_stats_size = sizeof(tSirWifiIfaceStat);
	iface_info_size = sizeof(tSirWifiInterfaceInfo);
	ac_stats_size   = sizeof(tSirWifiWmmAcStat);
	link_stats_results_size = sizeof(*link_stats_results) +
					link_stats_size;

	link_stats_results = vos_mem_malloc(link_stats_results_size);
	if (!link_stats_results) {
		WMA_LOGD("%s: could not allocate mem for stats results-len %zu",
		__func__, link_stats_results_size);
		return -ENOMEM;
	}

	WMA_LOGD("Interface stats from FW event buf");
	WMA_LOGD("Fixed Param:");
	WMA_LOGD("request_id %u vdev_id %u",
		fixed_param->request_id,fixed_param->vdev_id);

	WMA_LOGD("Iface Stats:");
	WMA_LOGD("beacon_rx %u mgmt_rx %u mgmt_action_rx %u mgmt_action_tx %u "
		 "rssi_mgmt %u rssi_data %u rssi_ack %u num_peers %u "
		 "num_peer_events %u num_ac %u roam_state %u"
		 " avg_bcn_spread_offset_high %u"
		 " avg_bcn_spread_offset_low %u"
		 " is leaky_ap %u"
		 " avg_rx_frames_leaked %u"
		 " rx_leak_window %u",
		 link_stats->beacon_rx, link_stats->mgmt_rx,
		 link_stats->mgmt_action_rx, link_stats->mgmt_action_tx,
		 link_stats->rssi_mgmt, link_stats->rssi_data,
		 link_stats->rssi_ack, link_stats->num_peers,
		 link_stats->num_peer_events, link_stats->num_ac,
		 link_stats->roam_state,
		 link_stats->avg_bcn_spread_offset_high,
		 link_stats->avg_bcn_spread_offset_low,
		 link_stats->is_leaky_ap,
		 link_stats->avg_rx_frms_leaked,
		 link_stats->rx_leak_window);

	vos_mem_zero(link_stats_results, link_stats_results_size);

	link_stats_results->paramId            = WMI_LINK_STATS_IFACE;
	link_stats_results->rspId              = fixed_param->request_id;
	link_stats_results->ifaceId            = fixed_param->vdev_id;
	link_stats_results->num_peers          = link_stats->num_peers;
	link_stats_results->peer_event_number  = 0;
	link_stats_results->moreResultToFollow = 0;

	results      = (u_int8_t *)link_stats_results->results;
	t_link_stats = (u_int8_t *)link_stats;
	t_ac_stats   = (u_int8_t *)ac_stats;

	/* Copy roaming state */
	roaming_offset = offsetof(tSirWifiInterfaceInfo, roaming);
	roaming_size   = member_size(tSirWifiInterfaceInfo, roaming);

	vos_mem_copy(results + roaming_offset, &link_stats->roam_state,
				roaming_size);

	vos_mem_copy(results + iface_info_size,
		t_link_stats + WMI_TLV_HDR_SIZE,
		link_stats_size - iface_info_size - WIFI_AC_MAX * ac_stats_size);

	next_res_offset = link_stats_size - WIFI_AC_MAX * ac_stats_size;
	next_ac_offset = WMI_TLV_HDR_SIZE;

        WMA_LOGD("AC Stats:");
	for (count = 0; count < link_stats->num_ac; count++) {
		WMA_LOGD("ac_type %u tx_mpdu %u rx_mpdu %u tx_mcast %u "
			"rx_mcast %u rx_ampdu %u tx_ampdu %u mpdu_lost %u "
			"retries %u retries_short %u retries_long %u "
			"contention_time_min %u contention_time_max %u "
			"contention_time_avg %u contention_num_samples %u",
			ac_stats->ac_type, ac_stats->tx_mpdu, ac_stats->rx_mpdu,
			ac_stats->tx_mcast, ac_stats->rx_mcast,
			ac_stats->rx_ampdu,ac_stats->tx_ampdu,
			ac_stats->mpdu_lost, ac_stats->retries,
			ac_stats->retries_short, ac_stats->retries_long,
			ac_stats->contention_time_min,
			ac_stats->contention_time_max,
			ac_stats->contention_time_avg,
			ac_stats->contention_num_samples);
		ac_stats++;

		vos_mem_copy(results + next_res_offset,
				t_ac_stats + next_ac_offset,
				ac_stats_size);
		next_res_offset += ac_stats_size;
		next_ac_offset  += sizeof(*ac_stats);
	}

	/* call hdd callback with Link Layer Statistics
	 * vdev_id/ifacId in link_stats_results will be
	 * used to retrieve the correct HDD context
	 */
	pMac->sme.pLinkLayerStatsIndCallback(pMac->hHdd,
		WDA_LINK_LAYER_STATS_RESULTS_RSP,
		link_stats_results);
	WMA_LOGD("%s: Iface Stats event posted to HDD", __func__);
	vos_mem_free(link_stats_results);

	return 0;
}

static int wma_unified_link_peer_stats_event_handler(void *handle,
			u_int8_t *cmd_param_info, u_int32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_PEER_LINK_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_peer_stats_event_fixed_param *fixed_param;
	wmi_peer_link_stats *peer_stats, *temp_peer_stats;
	wmi_rate_stats *rate_stats;
	tSirLLStatsResults *link_stats_results;
	u_int8_t *results, *t_peer_stats, *t_rate_stats;
	u_int32_t count, num_rates=0;
	u_int32_t next_res_offset, next_peer_offset, next_rate_offset;
	size_t peer_info_size, peer_stats_size, rate_stats_size;
	size_t link_stats_results_size;

	tpAniSirGlobal pMac = (tpAniSirGlobal )vos_get_context(VOS_MODULE_ID_PE,
                                wma_handle->vos_context);

	if (!pMac) {
		WMA_LOGD("%s: NULL pMac ptr. Exiting", __func__);
		return -EINVAL;
	}

	if (!pMac->sme.pLinkLayerStatsIndCallback) {
		WMA_LOGD("%s: HDD callback is null", __func__);
		return -EINVAL;
	}

	WMA_LOGD("%s: Posting Peer Stats event to HDD", __func__);
	param_tlvs = (WMI_PEER_LINK_STATS_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_tlvs) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}
	/*
	 * cmd_param_info contains
	 * wmi_peer_stats_event_fixed_param fixed_param;
	 * num_peers * size of(struct wmi_peer_link_stats)
	 * num_rates * size of(struct wmi_rate_stats)
	 * num_rates is the sum of the rates of all the peers.
	 */
	fixed_param = param_tlvs->fixed_param;
	peer_stats  = param_tlvs->peer_stats;
	rate_stats  = param_tlvs->peer_rate_stats;

	if (!fixed_param || !peer_stats ||
	    (peer_stats->num_rates && !rate_stats)) {
		WMA_LOGA("%s: Invalid param_tlvs for Peer Stats", __func__);
		return -EINVAL;
	}

	/*
	 * num_rates - sum of the rates of all the peers
	 */
	temp_peer_stats = (wmi_peer_link_stats*)peer_stats;
	for (count = 0; count < fixed_param->num_peers; count++) {
		num_rates += temp_peer_stats->num_rates;
		temp_peer_stats++;
	}

	peer_stats_size = sizeof(tSirWifiPeerStat);
	peer_info_size  = sizeof(tSirWifiPeerInfo);
	rate_stats_size = sizeof(tSirWifiRateStat);
	link_stats_results_size = sizeof(*link_stats_results) + peer_stats_size +
			(fixed_param->num_peers * peer_info_size) +
			(num_rates * rate_stats_size);

	link_stats_results = vos_mem_malloc(link_stats_results_size);
	if (NULL == link_stats_results ) {
		WMA_LOGD("%s: could not allocate mem for stats results-len %zu",
		__func__, link_stats_results_size);
		return -ENOMEM;
	}

	WMA_LOGD("Peer stats from FW event buf");
	WMA_LOGD("Fixed Param:");
	WMA_LOGD("request_id %u num_peers %u peer_event_number %u more_data %u",
			fixed_param->request_id, fixed_param->num_peers,
			fixed_param->peer_event_number, fixed_param->more_data);

	vos_mem_zero(link_stats_results, link_stats_results_size);

	link_stats_results->paramId            = WMI_LINK_STATS_ALL_PEER;
	link_stats_results->rspId              = fixed_param->request_id;
	link_stats_results->ifaceId            = 0;
	link_stats_results->num_peers          = fixed_param->num_peers;
	link_stats_results->peer_event_number  = fixed_param->peer_event_number;
	link_stats_results->moreResultToFollow = fixed_param->more_data;

	vos_mem_copy(link_stats_results->results,
		&fixed_param->num_peers, peer_stats_size);

	results      = (u_int8_t *)link_stats_results->results;
	t_peer_stats = (u_int8_t *)peer_stats;
	t_rate_stats = (u_int8_t *)rate_stats;
	next_res_offset  = peer_stats_size;
	next_peer_offset = WMI_TLV_HDR_SIZE;
	next_rate_offset = WMI_TLV_HDR_SIZE;
	for (count = 0; count < fixed_param->num_peers; count++) {
		WMA_LOGD("Peer Info:");
		WMA_LOGD("peer_type %u capabilities %u num_rates %u",
				peer_stats->peer_type, peer_stats->capabilities,
				peer_stats->num_rates);

		vos_mem_copy(results + next_res_offset,
				t_peer_stats + next_peer_offset,
				peer_info_size);
		next_res_offset += peer_info_size;

		/* Copy rate stats associated with this peer */
		for (count = 0; count < peer_stats->num_rates; count++) {
			WMA_LOGD("Rate Stats Info:");
			WMA_LOGD("rate %u bitrate %u tx_mpdu %u rx_mpdu %u "
				"mpdu_lost %u retries %u retries_short %u "
				"retries_long %u", rate_stats->rate,
				rate_stats->bitrate, rate_stats->tx_mpdu,
				rate_stats->rx_mpdu, rate_stats->mpdu_lost,
				rate_stats->retries, rate_stats->retries_short,
				rate_stats->retries_long);
			rate_stats++;

			vos_mem_copy(results + next_res_offset,
				t_rate_stats + next_rate_offset,
				rate_stats_size);
			next_res_offset  += rate_stats_size;
			next_rate_offset += sizeof(*rate_stats);
		}
		next_peer_offset += sizeof(*peer_stats);
		peer_stats++;
	}

	/* call hdd callback with Link Layer Statistics
	 * vdev_id/ifacId in link_stats_results will be
	 * used to retrieve the correct HDD context
	 */
	pMac->sme.pLinkLayerStatsIndCallback(pMac->hHdd,
	        WDA_LINK_LAYER_STATS_RESULTS_RSP,
	        link_stats_results);
	WMA_LOGD("%s: Peer Stats event posted to HDD", __func__);
	vos_mem_free(link_stats_results);

	return 0;
}

static int wma_unified_link_radio_stats_event_handler(void *handle,
			u_int8_t *cmd_param_info, u_int32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_RADIO_LINK_STATS_EVENTID_param_tlvs *param_tlvs;
	wmi_radio_link_stats_event_fixed_param *fixed_param;
	wmi_radio_link_stats *radio_stats;
	wmi_channel_stats *channel_stats;
	tSirLLStatsResults *link_stats_results;
	u_int8_t *results, *t_radio_stats, *t_channel_stats;
	u_int32_t next_res_offset, next_chan_offset, count;
	size_t radio_stats_size, chan_stats_size;
	size_t link_stats_results_size;

	tpAniSirGlobal pMac = (tpAniSirGlobal )vos_get_context(VOS_MODULE_ID_PE,
				wma_handle->vos_context);

	if (!pMac) {
		WMA_LOGD("%s: NULL pMac ptr. Exiting", __func__);
		return -EINVAL;
	}

	if (!pMac->sme.pLinkLayerStatsIndCallback) {
		WMA_LOGD("%s: HDD callback is null", __func__);
		return -EINVAL;
	}

	WMA_LOGD("%s: Posting Radio Stats event to HDD", __func__);
	param_tlvs = (WMI_RADIO_LINK_STATS_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_tlvs) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}

	/*
	 * cmd_param_info contains
	 * wmi_radio_link_stats_event_fixed_param fixed_param;
	 * size of(struct wmi_radio_link_stats);
	 * num_channels * size of(struct wmi_channel_stats)
	 */
	fixed_param    = param_tlvs->fixed_param;
	radio_stats    = param_tlvs->radio_stats;
	channel_stats  = param_tlvs->channel_stats;

	if (!fixed_param || !radio_stats ||
	    (radio_stats->num_channels && !channel_stats)) {
		WMA_LOGA("%s: Invalid param_tlvs for Radio Stats", __func__);
		return -EINVAL;
	}

	radio_stats_size = sizeof(tSirWifiRadioStat);
	chan_stats_size  = sizeof(tSirWifiChannelStats);
	link_stats_results_size = sizeof(*link_stats_results) +
			radio_stats_size +
			(radio_stats->num_channels * chan_stats_size);

	link_stats_results = vos_mem_malloc(link_stats_results_size);
	if (NULL == link_stats_results ) {
		WMA_LOGD("%s: could not allocate mem for stats results-len %zu",
		__func__, link_stats_results_size);
		return -ENOMEM;
	}

	WMA_LOGD("Radio stats from FW event buf");
	WMA_LOGD("Fixed Param:");
	WMA_LOGD("request_id %u num_radio %u more_radio_events %u",
			fixed_param->request_id, fixed_param->num_radio,
			fixed_param->more_radio_events);

	WMA_LOGD("Radio Info");
	WMA_LOGD("radio_id %u on_time %u tx_time %u rx_time %u on_time_scan %u "
			"on_time_nbd %u on_time_gscan %u on_time_roam_scan %u "
			"on_time_pno_scan %u on_time_hs20 %u num_channels %u",
			radio_stats->radio_id, radio_stats->on_time,
			radio_stats->tx_time, radio_stats->rx_time,
			radio_stats->on_time_scan, radio_stats->on_time_nbd,
			radio_stats->on_time_gscan,
			radio_stats->on_time_roam_scan,
			radio_stats->on_time_pno_scan,
			radio_stats->on_time_hs20,
			radio_stats->num_channels);

	vos_mem_zero(link_stats_results, link_stats_results_size);

	link_stats_results->paramId            = WMI_LINK_STATS_RADIO;
	link_stats_results->rspId              = fixed_param->request_id;
	link_stats_results->ifaceId            = 0;
	link_stats_results->num_radio          = fixed_param->num_radio;
	link_stats_results->peer_event_number  = 0;
	link_stats_results->moreResultToFollow = fixed_param->more_radio_events;

	results          = (u_int8_t *)link_stats_results->results;
	t_radio_stats    = (u_int8_t *)radio_stats;
	t_channel_stats  = (u_int8_t *)channel_stats;

	vos_mem_copy(results, t_radio_stats + WMI_TLV_HDR_SIZE,
			radio_stats_size);

	next_res_offset  = radio_stats_size;
	next_chan_offset = WMI_TLV_HDR_SIZE;
	WMA_LOGD("Channel Stats Info");
	for (count = 0; count < radio_stats->num_channels; count++) {
		WMA_LOGD("channel_width %u center_freq %u center_freq0 %u "
			"center_freq1 %u radio_awake_time %u cca_busy_time %u",
			channel_stats->channel_width, channel_stats->center_freq,
			channel_stats->center_freq0, channel_stats->center_freq1,
			channel_stats->radio_awake_time,
			channel_stats->cca_busy_time);
		channel_stats++;

		vos_mem_copy(results + next_res_offset,
				t_channel_stats + next_chan_offset,
				chan_stats_size);
		next_res_offset  += chan_stats_size;
		next_chan_offset += sizeof(*channel_stats);
	}

	/* call hdd callback with Link Layer Statistics
	 * vdev_id/ifacId in link_stats_results will be
	 * used to retrieve the correct HDD context
	 */
	pMac->sme.pLinkLayerStatsIndCallback(pMac->hHdd,
	        WDA_LINK_LAYER_STATS_RESULTS_RSP,
	        link_stats_results);
	WMA_LOGD("%s: Radio Stats event posted to HDD", __func__);
	vos_mem_free(link_stats_results);

	return 0;
}

#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

/**
 * wma_fw_mem_dump_event_handler() - handles fw memory dump event
 *
 * handle - pointer to wma handle.
 * cmd_param_info - pointer to TLV info received in the event.
 * len - length of data in @cmd_param_info
 *
 * This function is a handler for firmware memory dump event.
 */
#ifdef WLAN_FEATURE_MEMDUMP
static int wma_fw_mem_dump_event_handler(void *handle, u_int8_t *cmd_param_info,
					 u_int32_t len)
{
	WMI_UPDATE_FW_MEM_DUMP_EVENTID_param_tlvs *param_buf;
	wmi_update_fw_mem_dump_fixed_param *event;
	VOS_STATUS status;

	param_buf =
	    (WMI_UPDATE_FW_MEM_DUMP_EVENTID_param_tlvs *) cmd_param_info;
	if (!param_buf) {
		WMA_LOGA("%s: Invalid stats event", __func__);
		return -EINVAL;
	}

	event = param_buf->fixed_param;

	status = wma_fw_mem_dump_rsp(event->request_id,
					 event->fw_mem_dump_complete);
	if (VOS_STATUS_SUCCESS != status) {
		return -EINVAL;
	}

	WMA_LOGI("FW MEM DUMP RSP posted successfully");
	return 0;
}
#else
static int wma_fw_mem_dump_event_handler(void *handle, u_int8_t *cmd_param_info,
					 u_int32_t len)
{
	return 0;
}
#endif /* WLAN_FEATURE_MEMDUMP */

u_int8_t *wma_add_p2p_ie(u_int8_t *frm)
{
	u_int8_t wfa_oui[3] = WMA_P2P_WFA_OUI;
	struct p2p_ie *p2p_ie=(struct p2p_ie *) frm;

	p2p_ie->p2p_id = WMA_P2P_IE_ID;
	p2p_ie->p2p_oui[0] = wfa_oui[0];
	p2p_ie->p2p_oui[1] = wfa_oui[1];
	p2p_ie->p2p_oui[2] = wfa_oui[2];
	p2p_ie->p2p_oui_type = WMA_P2P_WFA_VER;
	p2p_ie->p2p_len = 4;
	return (frm + sizeof(struct p2p_ie));
}

static void wma_update_beacon_noa_ie(
		struct beacon_info *bcn,
		u_int16_t new_noa_sub_ie_len)
{
	struct p2p_ie *p2p_ie;
	u_int8_t *buf;

	/* if there is nothing to add, just return */
	if (new_noa_sub_ie_len == 0) {
		if (bcn->noa_sub_ie_len && bcn->noa_ie) {
			WMA_LOGD("%s: NoA is present in previous beacon, "
				"but not present in swba event, "
				"So Reset the NoA",
				__func__);
			/* TODO: Assuming p2p noa ie is last ie in the beacon */
			vos_mem_zero(bcn->noa_ie, (bcn->noa_sub_ie_len +
						sizeof(struct p2p_ie)) );
			bcn->len -= (bcn->noa_sub_ie_len +
					sizeof(struct p2p_ie));
			bcn->noa_ie = NULL;
			bcn->noa_sub_ie_len = 0;
		}
		WMA_LOGD("%s: No need to update NoA", __func__);
		return;
	}

	if (bcn->noa_sub_ie_len && bcn->noa_ie) {
		/* NoA present in previous beacon, update it */
		WMA_LOGD("%s: NoA present in previous beacon, "
			"update the NoA IE, bcn->len %u"
			"bcn->noa_sub_ie_len %u",
			__func__, bcn->len, bcn->noa_sub_ie_len);
		bcn->len -= (bcn->noa_sub_ie_len + sizeof(struct p2p_ie)) ;
		vos_mem_zero(bcn->noa_ie,
				(bcn->noa_sub_ie_len + sizeof(struct p2p_ie)));
	} else { /* NoA is not present in previous beacon */
		WMA_LOGD("%s: NoA not present in previous beacon, add it"
			"bcn->len %u", __func__, bcn->len);
		buf = adf_nbuf_data(bcn->buf);
		bcn->noa_ie = buf + bcn->len;
	}

	bcn->noa_sub_ie_len = new_noa_sub_ie_len;
	wma_add_p2p_ie(bcn->noa_ie);
	p2p_ie = (struct p2p_ie *) bcn->noa_ie;
	p2p_ie->p2p_len += new_noa_sub_ie_len;
	vos_mem_copy((bcn->noa_ie + sizeof(struct p2p_ie)), bcn->noa_sub_ie,
			new_noa_sub_ie_len);

	bcn->len += (new_noa_sub_ie_len + sizeof(struct p2p_ie));
	WMA_LOGI("%s: Updated beacon length with NoA Ie is %u",
		__func__, bcn->len);
}

static void wma_p2p_create_sub_ie_noa(
		u_int8_t *buf,
		struct p2p_sub_element_noa *noa,
		u_int16_t *new_noa_sub_ie_len)
{
	u_int8_t tmp_octet = 0;
	int i;
	u_int8_t *buf_start = buf;

	*buf++ = WMA_P2P_SUB_ELEMENT_NOA;     /* sub-element id */
	ASSERT(noa->num_descriptors <= WMA_MAX_NOA_DESCRIPTORS);

	/*
	 * Length = (2 octets for Index and CTWin/Opp PS) and
	 * (13 octets for each NOA Descriptors)
	 */
	P2PIE_PUT_LE16(buf, WMA_NOA_IE_SIZE(noa->num_descriptors));
	buf += 2;

	*buf++ = noa->index;        /* Instance Index */

	tmp_octet = noa->ctwindow & WMA_P2P_NOA_IE_CTWIN_MASK;
	if (noa->oppPS) {
		tmp_octet |= WMA_P2P_NOA_IE_OPP_PS_SET;
	}
	*buf++ = tmp_octet;         /* Opp Ps and CTWin capabilities */

	for (i = 0; i < noa->num_descriptors; i++) {
		ASSERT(noa->noa_descriptors[i].type_count != 0);

		*buf++ = noa->noa_descriptors[i].type_count;

		P2PIE_PUT_LE32(buf, noa->noa_descriptors[i].duration);
		buf += 4;
		P2PIE_PUT_LE32(buf, noa->noa_descriptors[i].interval);
		buf += 4;
		P2PIE_PUT_LE32(buf, noa->noa_descriptors[i].start_time);
		buf += 4;
	}
	*new_noa_sub_ie_len = (buf - buf_start);
}

static void wma_update_noa(struct beacon_info *beacon,
		struct p2p_sub_element_noa *noa_ie)
{
	u_int16_t new_noa_sub_ie_len;

	/* Call this function by holding the spinlock on beacon->lock */

	if (noa_ie) {
		if ((noa_ie->ctwindow == 0) && (noa_ie->oppPS == 0) &&
				(noa_ie->num_descriptors == 0)) {
			/* NoA is not present */
			WMA_LOGD("%s: NoA is not present", __func__);
			new_noa_sub_ie_len = 0;
		}
		else {
			/* Create the binary blob containing NOA sub-IE */
			WMA_LOGD("%s: Create NOA sub ie", __func__);
			wma_p2p_create_sub_ie_noa(&beacon->noa_sub_ie[0],
					noa_ie, &new_noa_sub_ie_len);
		}
	}
	else {
		WMA_LOGD("%s: No need to add NOA", __func__);
		new_noa_sub_ie_len = 0;  /* no NOA IE sub-attributes */
	}

	wma_update_beacon_noa_ie(beacon, new_noa_sub_ie_len);
}

static void wma_update_probe_resp_noa(tp_wma_handle wma_handle,
					struct p2p_sub_element_noa *noa_ie)
{
	tSirP2PNoaAttr *noa_attr = (tSirP2PNoaAttr *) vos_mem_malloc(sizeof(tSirP2PNoaAttr));
	WMA_LOGD("Received update NoA event");
	if (!noa_attr) {
		WMA_LOGE("Failed to allocate memory for tSirP2PNoaAttr");
		return;
	}

	vos_mem_zero(noa_attr, sizeof(tSirP2PNoaAttr));

	noa_attr->index = noa_ie->index;
	noa_attr->oppPsFlag = noa_ie->oppPS;
	noa_attr->ctWin = noa_ie->ctwindow;
	if (!noa_ie->num_descriptors) {
		WMA_LOGD("Zero NoA descriptors");
	}
	else {
		WMA_LOGD("%d NoA descriptors", noa_ie->num_descriptors);
		noa_attr->uNoa1IntervalCnt =
			noa_ie->noa_descriptors[0].type_count;
		noa_attr->uNoa1Duration =
			noa_ie->noa_descriptors[0].duration;
		noa_attr->uNoa1Interval =
			noa_ie->noa_descriptors[0].interval;
		noa_attr->uNoa1StartTime =
			noa_ie->noa_descriptors[0].start_time;
		if (noa_ie->num_descriptors > 1) {
			noa_attr->uNoa2IntervalCnt =
				noa_ie->noa_descriptors[1].type_count;
			noa_attr->uNoa2Duration =
				noa_ie->noa_descriptors[1].duration;
			noa_attr->uNoa2Interval =
				noa_ie->noa_descriptors[1].interval;
			noa_attr->uNoa2StartTime =
				noa_ie->noa_descriptors[1].start_time;
		}
	}
	WMA_LOGI("Sending SIR_HAL_P2P_NOA_ATTR_IND to LIM");
	wma_send_msg(wma_handle, SIR_HAL_P2P_NOA_ATTR_IND, (void *)noa_attr ,
			0);
}

static void wma_send_bcn_buf_ll(tp_wma_handle wma,
				ol_txrx_pdev_handle pdev,
				u_int8_t vdev_id,
				WMI_HOST_SWBA_EVENTID_param_tlvs *param_buf)
{
	wmi_bcn_send_from_host_cmd_fixed_param *cmd;
	struct ieee80211_frame *wh;
	struct beacon_info *bcn;
	wmi_tim_info *tim_info = param_buf->tim_info;
	u_int8_t *bcn_payload;
	wmi_buf_t wmi_buf;
	a_status_t ret;
	struct beacon_tim_ie *tim_ie;
	wmi_p2p_noa_info *p2p_noa_info = param_buf->p2p_noa_info;
	struct p2p_sub_element_noa noa_ie;
	u_int8_t i;
	int status;

	bcn = wma->interfaces[vdev_id].beacon;
	if (!bcn->buf) {
		WMA_LOGE("%s: Invalid beacon buffer", __func__);
		return;
	}

	wmi_buf = wmi_buf_alloc(wma->wmi_handle, sizeof(*cmd));
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return;
	}

	adf_os_spin_lock_bh(&bcn->lock);

	bcn_payload = adf_nbuf_data(bcn->buf);

	tim_ie = (struct beacon_tim_ie *)(&bcn_payload[bcn->tim_ie_offset]);

	if(tim_info->tim_changed) {
		if(tim_info->tim_num_ps_pending)
			vos_mem_copy(&tim_ie->tim_bitmap, tim_info->tim_bitmap,
				WMA_TIM_SUPPORTED_PVB_LENGTH);
		else
			vos_mem_zero(&tim_ie->tim_bitmap,
				WMA_TIM_SUPPORTED_PVB_LENGTH);
		/*
		 * Currently we support fixed number of
		 * peers as limited by HAL_NUM_STA.
		 * tim offset is always 0
		 */
		tim_ie->tim_bitctl = 0;
	}

	/* Update DTIM Count */
	if (tim_ie->dtim_count == 0)
		tim_ie->dtim_count = tim_ie->dtim_period - 1;
	else
		tim_ie->dtim_count--;

	/*
	 * DTIM count needs to be backedup so that
	 * when umac updates the beacon template
	 * current dtim count can be updated properly
	 */
	bcn->dtim_count = tim_ie->dtim_count;

	/* update state for buffered multicast frames on DTIM */
	if (tim_info->tim_mcast && (tim_ie->dtim_count == 0 ||
		tim_ie->dtim_period == 1))
		tim_ie->tim_bitctl |= 1;
	else
		tim_ie->tim_bitctl &= ~1;

	/* To avoid sw generated frame sequence the same as H/W generated frame,
	 * the value lower than min_sw_seq is reserved for HW generated frame */
	if ((bcn->seq_no & IEEE80211_SEQ_MASK) < MIN_SW_SEQ)
		bcn->seq_no = MIN_SW_SEQ;

	wh = (struct ieee80211_frame *) bcn_payload;
	*(u_int16_t *)&wh->i_seq[0] = htole16(bcn->seq_no
					      << IEEE80211_SEQ_SEQ_SHIFT);
	bcn->seq_no++;

	if (WMI_UNIFIED_NOA_ATTR_IS_MODIFIED(p2p_noa_info)) {
		vos_mem_zero(&noa_ie, sizeof(noa_ie));

		noa_ie.index = (u_int8_t)WMI_UNIFIED_NOA_ATTR_INDEX_GET(p2p_noa_info);
		noa_ie.oppPS = (u_int8_t)WMI_UNIFIED_NOA_ATTR_OPP_PS_GET(p2p_noa_info);
		noa_ie.ctwindow = (u_int8_t)WMI_UNIFIED_NOA_ATTR_CTWIN_GET(p2p_noa_info);
		noa_ie.num_descriptors = (u_int8_t)WMI_UNIFIED_NOA_ATTR_NUM_DESC_GET(
				p2p_noa_info);
		WMA_LOGI("%s: index %u, oppPs %u, ctwindow %u, "
			"num_descriptors = %u", __func__, noa_ie.index,
			noa_ie.oppPS, noa_ie.ctwindow, noa_ie.num_descriptors);
		for(i = 0; i < noa_ie.num_descriptors; i++) {
			noa_ie.noa_descriptors[i].type_count =
				 (u_int8_t)p2p_noa_info->noa_descriptors[i].type_count;
			noa_ie.noa_descriptors[i].duration =
				p2p_noa_info->noa_descriptors[i].duration;
			noa_ie.noa_descriptors[i].interval =
				p2p_noa_info->noa_descriptors[i].interval;
			noa_ie.noa_descriptors[i].start_time =
				p2p_noa_info->noa_descriptors[i].start_time;
			WMA_LOGI("%s: NoA descriptor[%d] type_count %u, "
				"duration %u, interval %u, start_time = %u",
				__func__, i,
				noa_ie.noa_descriptors[i].type_count,
				noa_ie.noa_descriptors[i].duration,
				noa_ie.noa_descriptors[i].interval,
				noa_ie.noa_descriptors[i].start_time);
		}
		wma_update_noa(bcn, &noa_ie);

		/* Send a msg to LIM to update the NoA IE in probe response
		 * frames transmitted by the host */
		wma_update_probe_resp_noa(wma, &noa_ie);
	}

	if (bcn->dma_mapped) {
		adf_nbuf_unmap_single(pdev->osdev, bcn->buf,
				      ADF_OS_DMA_TO_DEVICE);
		bcn->dma_mapped = 0;
	}
	ret = adf_nbuf_map_single(pdev->osdev, bcn->buf,
				  ADF_OS_DMA_TO_DEVICE);
	if (ret != A_STATUS_OK) {
		adf_nbuf_free(wmi_buf);
		WMA_LOGE("%s: failed map beacon buf to DMA region",
				__func__);
		adf_os_spin_unlock_bh(&bcn->lock);
		return;
	}

	bcn->dma_mapped = 1;
	cmd = (wmi_bcn_send_from_host_cmd_fixed_param *) wmi_buf_data(wmi_buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_bcn_send_from_host_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_bcn_send_from_host_cmd_fixed_param));
	cmd->vdev_id = vdev_id;
	cmd->data_len = bcn->len;
	cmd->frame_ctrl = *((A_UINT16 *)wh->i_fc);
	cmd->frag_ptr = adf_nbuf_get_frag_paddr_lo(bcn->buf, 0);

	/* Notify Firmware of DTM and mcast/bcast traffic */
	if (tim_ie->dtim_count == 0) {
		cmd->dtim_flag |= WMI_BCN_SEND_DTIM_ZERO;
		 /* deliver mcast/bcast traffic in next DTIM beacon */
		if (tim_ie->tim_bitctl & 0x01)
			cmd->dtim_flag |= WMI_BCN_SEND_DTIM_BITCTL_SET;
	}

	status = wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, sizeof(*cmd),
			     WMI_PDEV_SEND_BCN_CMDID);

	if (status != EOK) {
		WMA_LOGE("Failed to send WMI_PDEV_SEND_BCN_CMDID command");
		wmi_buf_free(wmi_buf);
	}
	adf_os_spin_unlock_bh(&bcn->lock);
}

static int wma_beacon_swba_handler(void *handle, u_int8_t *event, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_HOST_SWBA_EVENTID_param_tlvs *param_buf;
	wmi_host_swba_event_fixed_param *swba_event;
	u_int32_t vdev_map;
	ol_txrx_pdev_handle pdev;
	u_int8_t vdev_id = 0;

	param_buf = (WMI_HOST_SWBA_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		WMA_LOGE("Invalid swba event buffer");
		return -EINVAL;
	}
	swba_event = param_buf->fixed_param;
	vdev_map = swba_event->vdev_map;

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);
	if (!pdev) {
		WMA_LOGE("%s: pdev is NULL", __func__);
		return -EINVAL;
	}

	for ( ; vdev_map; vdev_id++, vdev_map >>= 1) {
		if (!(vdev_map & 0x1))
			continue;
		if (!wdi_out_cfg_is_high_latency(pdev->ctrl_pdev))
			wma_send_bcn_buf_ll(wma, pdev, vdev_id, param_buf);
		break;
	}
	return 0;
}

static int wma_csa_offload_handler(void *handle, u_int8_t *event, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	WMI_CSA_HANDLING_EVENTID_param_tlvs *param_buf;
	wmi_csa_event_fixed_param *csa_event;
	u_int8_t bssid[IEEE80211_ADDR_LEN];
	u_int8_t vdev_id = 0;
	u_int8_t cur_chan = 0;
	struct ieee80211_channelswitch_ie *csa_ie;
	tpCSAOffloadParams csa_offload_event;
	struct ieee80211_extendedchannelswitch_ie *xcsa_ie;
	struct ieee80211_ie_wide_bw_switch *wb_ie;
	struct wma_txrx_node *intr = wma->interfaces;

	param_buf = (WMI_CSA_HANDLING_EVENTID_param_tlvs *) event;

	WMA_LOGD("%s: Enter", __func__);
	if (!param_buf) {
		WMA_LOGE("Invalid csa event buffer");
		return -EINVAL;
	}
	csa_event = param_buf->fixed_param;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&csa_event->i_addr2, &bssid[0]);

	if (wma_find_vdev_by_bssid(wma, bssid, &vdev_id) == NULL) {
		WMA_LOGE("Invalid bssid received %s:%d", __func__, __LINE__);
		return -EINVAL;
	}

	csa_offload_event = vos_mem_malloc(sizeof(*csa_offload_event));
	if (!csa_offload_event) {
		WMA_LOGE("VOS MEM Alloc Failed for csa_offload_event");
		return -EINVAL;
	}

	vos_mem_zero(csa_offload_event, sizeof(*csa_offload_event));
	vos_mem_copy(csa_offload_event->bssId, &bssid, ETH_ALEN);

	if (csa_event->ies_present_flag & WMI_CSA_IE_PRESENT) {
		csa_ie = (struct ieee80211_channelswitch_ie *)(&csa_event->csa_ie[0]);
		csa_offload_event->channel = csa_ie->newchannel;
		csa_offload_event->switchmode = csa_ie->switchmode;
	} else if (csa_event->ies_present_flag & WMI_XCSA_IE_PRESENT) {
		xcsa_ie = (struct ieee80211_extendedchannelswitch_ie*)(&csa_event->xcsa_ie[0]);
		csa_offload_event->channel = xcsa_ie->newchannel;
		csa_offload_event->switchmode = xcsa_ie->switchmode;
	} else {
		WMA_LOGE("CSA Event error: No CSA IE present");
		vos_mem_free(csa_offload_event);
		return -EINVAL;
	}

	if (csa_event->ies_present_flag & WMI_WBW_IE_PRESENT) {
		wb_ie = (struct ieee80211_ie_wide_bw_switch*)(&csa_event->wb_ie[0]);
		csa_offload_event->new_ch_width = wb_ie->new_ch_width;
		csa_offload_event->new_ch_freq_seg1 = wb_ie->new_ch_freq_seg1;
		csa_offload_event->new_ch_freq_seg2 = wb_ie->new_ch_freq_seg2;
	}

	csa_offload_event->ies_present_flag = csa_event->ies_present_flag;

	WMA_LOGD("CSA: New Channel = %d BSSID:%pM",
			csa_offload_event->channel,
			csa_offload_event->bssId);

	cur_chan = vos_freq_to_chan(intr[vdev_id].mhz);
	/*
	 * basic sanity check: requested channel should not be 0
	 * and equal to home channel
	 */
	if( (0 == csa_offload_event->channel) ||
	    (cur_chan == csa_offload_event->channel) ) {
		WMA_LOGE("CSA Event with channel %d. Ignore !!",
		csa_offload_event->channel);
		vos_mem_free(csa_offload_event);
		return -EINVAL;
	}
	wma->interfaces[vdev_id].is_channel_switch = VOS_TRUE;
	wma_send_msg(wma, WDA_CSA_OFFLOAD_EVENT, (void *)csa_offload_event, 0);
	return 0;
}

#ifdef WLAN_FEATURE_GTK_OFFLOAD
static int wma_gtk_offload_status_event(void *handle, u_int8_t *event,
					u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	WMI_GTK_OFFLOAD_STATUS_EVENT_fixed_param *status;
	WMI_GTK_OFFLOAD_STATUS_EVENTID_param_tlvs *param_buf;
	tpSirGtkOffloadGetInfoRspParams resp;
	vos_msg_t vos_msg;
	u_int8_t *bssid;

	WMA_LOGD("%s Enter", __func__);

	param_buf = (WMI_GTK_OFFLOAD_STATUS_EVENTID_param_tlvs *)event;
	if (!param_buf) {
		WMA_LOGE("param_buf is NULL");
		return -EINVAL;
	}

	status = (WMI_GTK_OFFLOAD_STATUS_EVENT_fixed_param *)param_buf->fixed_param;

	if (len < sizeof(WMI_GTK_OFFLOAD_STATUS_EVENT_fixed_param)) {
		WMA_LOGE("Invalid length for GTK status");
		return -EINVAL;
	}
	bssid = wma_find_bssid_by_vdev_id(wma, status->vdev_id);
	if (!bssid) {
		WMA_LOGE("invalid bssid for vdev id %d", status->vdev_id);
		return -ENOENT;
	}

	resp = vos_mem_malloc(sizeof(*resp));
	if (!resp) {
		WMA_LOGE("%s: Failed to alloc response", __func__);
		return -ENOMEM;
	}
	vos_mem_zero(resp, sizeof(*resp));
	resp->mesgType = eWNI_PMC_GTK_OFFLOAD_GETINFO_RSP;
	resp->mesgLen = sizeof(*resp);
	resp->ulStatus = VOS_STATUS_SUCCESS;
	resp->ulTotalRekeyCount = status->refresh_cnt;
	/* TODO: Is the total rekey count and GTK rekey count same? */
	resp->ulGTKRekeyCount = status->refresh_cnt;

	vos_mem_copy(&resp->ullKeyReplayCounter,  &status->replay_counter,
		     GTK_REPLAY_COUNTER_BYTES);

	vos_mem_copy(resp->bssId, bssid, ETH_ALEN);

#ifdef IGTK_OFFLOAD
	/* TODO: Is the refresh count same for GTK and IGTK? */
	resp->ulIGTKRekeyCount = status->refresh_cnt;
#endif

	vos_msg.type = eWNI_PMC_GTK_OFFLOAD_GETINFO_RSP;
	vos_msg.bodyptr = (void *)resp;
	vos_msg.bodyval = 0;

	if (vos_mq_post_message(VOS_MQ_ID_SME, (vos_msg_t*)&vos_msg)
			!= VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to post GTK response to SME");
		vos_mem_free(resp);
		return -EINVAL;
	}

	WMA_LOGD("GTK: got target status with replay counter "
		 "%02x%02x%02x%02x%02x%02x%02x%02x. vdev %d "
		 "Refresh GTK %d times exchanges since last set operation",
		 status->replay_counter[0],
		 status->replay_counter[1],
		 status->replay_counter[2],
		 status->replay_counter[3],
		 status->replay_counter[4],
		 status->replay_counter[5],
		 status->replay_counter[6],
		 status->replay_counter[7],
		 status->vdev_id, status->refresh_cnt);

	WMA_LOGD("%s Exit", __func__);

	return 0;
}
#endif

#ifdef FEATURE_OEM_DATA_SUPPORT
static int wma_oem_capability_event_callback(void *handle,
	u_int8_t *datap, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_OEM_CAPABILITY_EVENTID_param_tlvs *param_buf;
	u_int8_t *data;
	u_int32_t datalen;
	u_int32_t *msg_subtype;
	tStartOemDataRsp *pStartOemDataRsp;

	param_buf = (WMI_OEM_CAPABILITY_EVENTID_param_tlvs *)datap;
	if (!param_buf) {
		WMA_LOGE("%s: Received NULL buf ptr from FW", __func__);
		return -ENOMEM;
	}

	data = param_buf->data;
	datalen = param_buf->num_data;

	if (!data) {
		WMA_LOGE("%s: Received NULL data from FW", __func__);
		return -EINVAL;
	}

	/* wma puts 4 bytes prefix for msg subtype, so length
	 * of data received from target should be 4 bytes less
	 * then max allowed
	 */
	if (datalen > (OEM_DATA_RSP_SIZE - 4)) {
		WMA_LOGE("%s: Received data len (%d) exceeds max value (%d)",
		         __func__, datalen, (OEM_DATA_RSP_SIZE - 4));
		return -EINVAL;
	}

	pStartOemDataRsp = vos_mem_malloc(sizeof(*pStartOemDataRsp));
	if (!pStartOemDataRsp) {
		WMA_LOGE("%s: Failed to alloc pStartOemDataRsp", __func__);
		return -ENOMEM;
	}

	vos_mem_zero(pStartOemDataRsp, sizeof(tStartOemDataRsp));
	msg_subtype = (u_int32_t *)(&pStartOemDataRsp->oemDataRsp[0]);
	*msg_subtype = WMI_OEM_CAPABILITY_RSP;
	vos_mem_copy(&pStartOemDataRsp->oemDataRsp[4], data, datalen);

	WMA_LOGI("%s: Sending WDA_START_OEM_DATA_RSP, data len (%d)",
	         __func__, datalen);

	wma_send_msg(wma, WDA_START_OEM_DATA_RSP, (void *)pStartOemDataRsp, 0);
	return 0;
}

static int wma_oem_measurement_report_event_callback(void *handle,
	u_int8_t *datap, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_OEM_MEASUREMENT_REPORT_EVENTID_param_tlvs *param_buf;
	u_int8_t *data;
	u_int32_t datalen;
	u_int32_t *msg_subtype;
	tStartOemDataRsp *pStartOemDataRsp;

	param_buf = (WMI_OEM_MEASUREMENT_REPORT_EVENTID_param_tlvs *)datap;
	if (!param_buf) {
		WMA_LOGE("%s: Received NULL buf ptr from FW", __func__);
		return -ENOMEM;
	}

	data = param_buf->data;
	datalen = param_buf->num_data;

	if (!data) {
		WMA_LOGE("%s: Received NULL data from FW", __func__);
		return -EINVAL;
	}

	/* wma puts 4 bytes prefix for msg subtype, so length
	 * of data received from target should be 4 bytes less
	 * then max allowed
	 */
	if (datalen > (OEM_DATA_RSP_SIZE - 4)) {
		WMA_LOGE("%s: Received data len (%d) exceeds max value (%d)",
		         __func__, datalen, (OEM_DATA_RSP_SIZE - 4));
		return -EINVAL;
	}

	pStartOemDataRsp = vos_mem_malloc(sizeof(*pStartOemDataRsp));
	if (!pStartOemDataRsp) {
		WMA_LOGE("%s: Failed to alloc pStartOemDataRsp", __func__);
		return -ENOMEM;
	}

	vos_mem_zero(pStartOemDataRsp, sizeof(tStartOemDataRsp));
	msg_subtype = (u_int32_t *)(&pStartOemDataRsp->oemDataRsp[0]);
	*msg_subtype = WMI_OEM_MEASUREMENT_RSP;
	vos_mem_copy(&pStartOemDataRsp->oemDataRsp[4], data, datalen);

	WMA_LOGI("%s: Sending WDA_START_OEM_DATA_RSP, data len (%d)",
	         __func__, datalen);

	wma_send_msg(wma, WDA_START_OEM_DATA_RSP, (void *)pStartOemDataRsp, 0);
	return 0;
}

static int wma_oem_error_report_event_callback(void *handle,
	u_int8_t *datap, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_OEM_ERROR_REPORT_EVENTID_param_tlvs *param_buf;
	u_int8_t *data;
	u_int32_t datalen;
	u_int32_t *msg_subtype;
	tStartOemDataRsp *pStartOemDataRsp;

	param_buf = (WMI_OEM_ERROR_REPORT_EVENTID_param_tlvs *)datap;
	if (!param_buf) {
		WMA_LOGE("%s: Received NULL buf ptr from FW", __func__);
		return -ENOMEM;
	}

	data = param_buf->data;
	datalen = param_buf->num_data;

	if (!data) {
		WMA_LOGE("%s: Received NULL data from FW", __func__);
		return -EINVAL;
	}

	/* wma puts 4 bytes prefix for msg subtype, so length
	 * of data received from target should be 4 bytes less
	 * then max allowed
	 */
	if (datalen > (OEM_DATA_RSP_SIZE - 4)) {
		WMA_LOGE("%s: Received data len (%d) exceeds max value (%d)",
		         __func__, datalen, (OEM_DATA_RSP_SIZE - 4));
		return -EINVAL;
	}

	pStartOemDataRsp = vos_mem_malloc(sizeof(*pStartOemDataRsp));
	if (!pStartOemDataRsp) {
		WMA_LOGE("%s: Failed to alloc pStartOemDataRsp", __func__);
		return -ENOMEM;
	}

	vos_mem_zero(pStartOemDataRsp, sizeof(tStartOemDataRsp));
	msg_subtype = (u_int32_t *)(&pStartOemDataRsp->oemDataRsp[0]);
	*msg_subtype = WMI_OEM_ERROR_REPORT_RSP;
	vos_mem_copy(&pStartOemDataRsp->oemDataRsp[4], data, datalen);

	WMA_LOGI("%s: Sending WDA_START_OEM_DATA_RSP, data len (%d)",
	         __func__, datalen);

	wma_send_msg(wma, WDA_START_OEM_DATA_RSP, (void *)pStartOemDataRsp, 0);
	return 0;
}
#endif /* FEATURE_OEM_DATA_SUPPORT */

static int wma_p2p_noa_event_handler(void *handle, u_int8_t *event, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_P2P_NOA_EVENTID_param_tlvs *param_buf;
	wmi_p2p_noa_event_fixed_param *p2p_noa_event;
	u_int8_t vdev_id, i;
	wmi_p2p_noa_info *p2p_noa_info;
	struct p2p_sub_element_noa noa_ie;
	u_int8_t *buf_ptr;
	u_int32_t descriptors;

	param_buf = (WMI_P2P_NOA_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		WMA_LOGE("Invalid P2P NoA event buffer");
		return -EINVAL;
	}

	p2p_noa_event = param_buf->fixed_param;
	buf_ptr = (u_int8_t *) p2p_noa_event;
	buf_ptr += sizeof(wmi_p2p_noa_event_fixed_param);
	p2p_noa_info = (wmi_p2p_noa_info *) (buf_ptr);
	vdev_id = p2p_noa_event->vdev_id;

	if (WMI_UNIFIED_NOA_ATTR_IS_MODIFIED(p2p_noa_info)) {

		vos_mem_zero(&noa_ie, sizeof(noa_ie));
		noa_ie.index = (u_int8_t)WMI_UNIFIED_NOA_ATTR_INDEX_GET(p2p_noa_info);
		noa_ie.oppPS = (u_int8_t)WMI_UNIFIED_NOA_ATTR_OPP_PS_GET(p2p_noa_info);
		noa_ie.ctwindow = (u_int8_t)WMI_UNIFIED_NOA_ATTR_CTWIN_GET(p2p_noa_info);
		descriptors = WMI_UNIFIED_NOA_ATTR_NUM_DESC_GET(p2p_noa_info);
		noa_ie.num_descriptors = (u_int8_t)descriptors;

		WMA_LOGI("%s: index %u, oppPs %u, ctwindow %u, "
				"num_descriptors = %u", __func__, noa_ie.index,
				noa_ie.oppPS, noa_ie.ctwindow, noa_ie.num_descriptors);
		for(i = 0; i < noa_ie.num_descriptors; i++) {
			noa_ie.noa_descriptors[i].type_count =
				(u_int8_t)p2p_noa_info->noa_descriptors[i].type_count;
			noa_ie.noa_descriptors[i].duration =
				p2p_noa_info->noa_descriptors[i].duration;
			noa_ie.noa_descriptors[i].interval =
				p2p_noa_info->noa_descriptors[i].interval;
			noa_ie.noa_descriptors[i].start_time =
				p2p_noa_info->noa_descriptors[i].start_time;
			WMA_LOGI("%s: NoA descriptor[%d] type_count %u, "
					"duration %u, interval %u, start_time = %u",
					__func__, i,
					noa_ie.noa_descriptors[i].type_count,
					noa_ie.noa_descriptors[i].duration,
					noa_ie.noa_descriptors[i].interval,
					noa_ie.noa_descriptors[i].start_time);
		}

		/* Send a msg to LIM to update the NoA IE in probe response
		 * frames transmitted by the host */
		wma_update_probe_resp_noa(wma, &noa_ie);
	}

	return 0;
}

#ifdef FEATURE_WLAN_TDLS
static int wma_tdls_event_handler(void *handle, u_int8_t *event, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_TDLS_PEER_EVENTID_param_tlvs *param_buf = NULL;
	wmi_tdls_peer_event_fixed_param *peer_event = NULL;
	tSirTdlsEventNotify *tdls_event;

	if (!event) {
	 WMA_LOGE("%s: event param null", __func__);
	 return -1;
	}

	param_buf = (WMI_TDLS_PEER_EVENTID_param_tlvs *) event;
	if (!param_buf) {
	 WMA_LOGE("%s: received null buf from target", __func__);
	 return -1;
	}

	peer_event = param_buf->fixed_param;
	if (!peer_event) {
	 WMA_LOGE("%s: received null event data from target", __func__);
	 return -1;
	}

	tdls_event = (tSirTdlsEventNotify *)
	              vos_mem_malloc(sizeof(*tdls_event));
	if (!tdls_event) {
	 WMA_LOGE("%s: failed to allocate memory for tdls_event", __func__);
	 return -1;
	}

	tdls_event->sessionId = peer_event->vdev_id;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&peer_event->peer_macaddr, tdls_event->peerMac);

	switch(peer_event->peer_status) {
	case WMI_TDLS_SHOULD_DISCOVER:
	 tdls_event->messageType = WDA_TDLS_SHOULD_DISCOVER;
	 break;
	case WMI_TDLS_SHOULD_TEARDOWN:
	 tdls_event->messageType = WDA_TDLS_SHOULD_TEARDOWN;
	 break;
	case WMI_TDLS_PEER_DISCONNECTED:
	 tdls_event->messageType = WDA_TDLS_PEER_DISCONNECTED;
	 break;
	default:
	 WMA_LOGE("%s: Discarding unknown tdls event(%d) from target",
	          __func__, peer_event->peer_status);
	 return -1;
	}

	switch (peer_event->peer_reason) {
	case WMI_TDLS_TEARDOWN_REASON_TX:
		tdls_event->peer_reason = eWNI_TDLS_TEARDOWN_REASON_TX;
		break;
	case WMI_TDLS_TEARDOWN_REASON_RSSI:
		tdls_event->peer_reason = eWNI_TDLS_TEARDOWN_REASON_RSSI;
		break;
	case WMI_TDLS_TEARDOWN_REASON_SCAN:
		tdls_event->peer_reason = eWNI_TDLS_TEARDOWN_REASON_SCAN;
		break;
	case WMI_TDLS_DISCONNECTED_REASON_PEER_DELETE:
		tdls_event->peer_reason = eWNI_TDLS_DISCONNECTED_REASON_PEER_DELETE;
		break;
	case WMI_TDLS_TEARDOWN_REASON_PTR_TIMEOUT:
		tdls_event->peer_reason = eWNI_TDLS_TEARDOWN_REASON_PTR_TIMEOUT;
		break;
	case WMI_TDLS_TEARDOWN_REASON_BAD_PTR:
		tdls_event->peer_reason = eWNI_TDLS_TEARDOWN_REASON_BAD_PTR;
		break;
	case WMI_TDLS_TEARDOWN_REASON_NO_RESPONSE:
		tdls_event->peer_reason = eWNI_TDLS_TEARDOWN_REASON_NO_RESPONSE;
		break;
	default:
		WMA_LOGE("%s: unknown reason(%d) in tdls event(%d) from target",
		         __func__, peer_event->peer_reason, peer_event->peer_status);
		return -1;
	}

	WMA_LOGD("%s: sending msg to umac, messageType: 0x%x, "
	         "for peer: %pM, reason: %d, smesessionId: %d",
	         __func__, tdls_event->messageType, tdls_event->peerMac,
	         tdls_event->peer_reason, tdls_event->sessionId);

	wma_send_msg(wma, tdls_event->messageType, (void *)tdls_event, 0);
	return 0;
}
#endif /* FEATURE_WLAN_TDLS */

/*
 * WMI Handler for WMI_PHYERR_EVENTID event from firmware.
 * This handler is currently handling only DFS phy errors.
 * This handler will be invoked only when the DFS phyerror
 * filtering offload is disabled.
 * Return- 1:Success, 0:Failure
 */
static int wma_unified_phyerr_rx_event_handler(void * handle,
                                       u_int8_t *data, u_int32_t datalen)
{
    tp_wma_handle wma = (tp_wma_handle) handle;
    WMI_PHYERR_EVENTID_param_tlvs *param_tlvs;
    wmi_comb_phyerr_rx_hdr *pe_hdr;
    u_int8_t *bufp;
    wmi_single_phyerr_rx_event *ev;
    struct ieee80211com *ic = wma->dfs_ic;
    adf_os_size_t n;
    A_UINT64 tsf64 = 0;
    int phy_err_code = 0;
    int error = 0;
    param_tlvs = (WMI_PHYERR_EVENTID_param_tlvs *)data;

    if (!param_tlvs)
    {
        WMA_LOGE("%s: Received NULL data from FW", __func__);
        return 0;
    }

    pe_hdr = param_tlvs->hdr;
    if (pe_hdr == NULL)
    {
        WMA_LOGE("%s: Received Data PE Header is NULL", __func__);
        return 0;
    }

    /* Ensure it's at least the size of the header */
    if (datalen < sizeof(*pe_hdr))
    {
        WMA_LOGE("%s:  Expected minimum size %zu, received %d",
                  __func__, sizeof(*pe_hdr), datalen);
        return 0;
    }
    if (pe_hdr->buf_len > DFS_MAX_BUF_LENGHT)
    {
        WMA_LOGE("%s: Received Invalid Phyerror event buffer length = %d"
                 "Maximum allowed buf length = %d",
                  __func__, pe_hdr->buf_len, DFS_MAX_BUF_LENGHT);

        return 0;
    }

	 /*
     * Reconstruct the 64 bit event TSF. This isn't from the MAC, it's
     * at the time the event was sent to us, the TSF value will be
     * in the future.
     */
    tsf64 = pe_hdr->tsf_l32;
    tsf64 |= (((uint64_t) pe_hdr->tsf_u32) << 32);

    /*
     * Loop over the bufp, extracting out phyerrors
     * wmi_unified_comb_phyerr_rx_event.bufp is a char pointer,
     * which isn't correct here - what we have received here
     * is an array of TLV-style PHY errors.
     */
    n = 0;/* Start just after the header */
    bufp = param_tlvs->bufp;
    while (n < pe_hdr->buf_len)
    {
        /* ensure there's at least space for the header */
        if ((pe_hdr->buf_len - n) < sizeof(ev->hdr))
        {
            WMA_LOGE("%s: Not enough space.(datalen=%d, n=%zu, hdr=%zu bytes",
                      __func__,pe_hdr->buf_len,n,sizeof(ev->hdr));
            error = 1;
            break;
        }
        /*
         * Obtain a pointer to the beginning of the current event.
         * data[0] is the beginning of the WMI payload.
         */
        ev = (wmi_single_phyerr_rx_event *) &bufp[n];

        /*
         * Sanity check the buffer length of the event against
         * what we currently have.
         * Since buf_len is 32 bits, we check if it overflows
         * a large 32 bit value. It's not 0x7fffffff because
         * we increase n by (buf_len + sizeof(hdr)), which would
         * in itself cause n to overflow.
         * If "int" is 64 bits then this becomes a moot point.
         */
        if (ev->hdr.buf_len > 0x7f000000)
        {
            WMA_LOGE("%s:buf_len is garbage (0x%x)",__func__,
                                              ev->hdr.buf_len);
            error = 1;
            break;
        }
        if (n + ev->hdr.buf_len > pe_hdr->buf_len)
        {
            WMA_LOGE("%s: buf_len exceeds available space n=%zu,"
                          "buf_len=%d, datalen=%d",
                          __func__,n,ev->hdr.buf_len,pe_hdr->buf_len);
            error = 1;
            break;
        }
        phy_err_code = WMI_UNIFIED_PHYERRCODE_GET(&ev->hdr);

        /*
         * If the phyerror category matches,
         * pass radar events to the dfs pattern matching code.
         * Don't pass radar events with no buffer payload.
         */
        if (phy_err_code == 0x5 || phy_err_code == 0x24)
        {
            if (ev->hdr.buf_len > 0)
            {
                /* Calling in to the DFS module to process the phyerr */
                dfs_process_phyerr(ic, &ev->bufp[0], ev->hdr.buf_len,
                            WMI_UNIFIED_RSSI_COMB_GET(&ev->hdr) & 0xff,
                            /* Extension RSSI */
                            WMI_UNIFIED_RSSI_COMB_GET(&ev->hdr) & 0xff,
                            ev->hdr.tsf_timestamp,
                            tsf64);
            }
        }

        /*
         * Advance the buffer pointer to the next PHY error.
         * buflen is the length of this payload, so we need to
         * advance past the current header _AND_ the payload.
         */
        n += sizeof(*ev) + ev->hdr.buf_len;

    }/*end while()*/
    if (error)
    {
        return (0);
    }
    else
    {
        return (1);
    }
}

#ifdef WLAN_FEATURE_NAN
/* function   : wma_nan_rsp_event_handler
 * Descriptin : Function is used to handle nan response
 * Args       : wma_handle, event buffer and its length
 * Returns    : SUCCESS or FAILURE
 */
static int wma_nan_rsp_event_handler(void *handle, u_int8_t *event_buf,
                                     u_int32_t len)
{
	WMI_NAN_EVENTID_param_tlvs *param_buf;
	tSirNanEvent *nan_rsp_event;
	wmi_nan_event_hdr *nan_rsp_event_hdr;
	VOS_STATUS status;
	vos_msg_t vos_msg;
	u_int8_t *buf_ptr;
	u_int32_t alloc_len;

	/*
	 * This is how received event_buf looks like
	 *
	 * <-------------------- event_buf ----------------------------------->
	 *
	 * <--wmi_nan_event_hdr--><---WMI_TLV_HDR_SIZE---><----- data -------->
	 *
	 * +-----------+---------+-----------------------+--------------------+
	 * | tlv_header| data_len| WMITLV_TAG_ARRAY_BYTE | nan_rsp_event_data |
	 * +-----------+---------+-----------------------+--------------------+
	 */

	WMA_LOGD("%s: Posting NaN response event to SME", __func__);
	param_buf = (WMI_NAN_EVENTID_param_tlvs *)event_buf;
	if (!param_buf) {
	    WMA_LOGE("%s: Invalid nan response event buf", __func__);
	    return -EINVAL;
	}
	nan_rsp_event_hdr = param_buf->fixed_param;
	buf_ptr = (u_int8_t *)nan_rsp_event_hdr;
	alloc_len = sizeof(tSirNanEvent);
	alloc_len += nan_rsp_event_hdr->data_len;
	nan_rsp_event = (tSirNanEvent *) vos_mem_malloc(alloc_len);
	if (NULL == nan_rsp_event) {
	    WMA_LOGE("%s: Memory allocation failure", __func__);
	    return -ENOMEM;
	}

	nan_rsp_event->event_data_len = nan_rsp_event_hdr->data_len;
	vos_mem_copy(nan_rsp_event->event_data, buf_ptr +
	        sizeof(wmi_nan_event_hdr) + WMI_TLV_HDR_SIZE,
	        nan_rsp_event->event_data_len);
	vos_msg.type = eWNI_SME_NAN_EVENT;
	vos_msg.bodyptr = (void *)nan_rsp_event;
	vos_msg.bodyval = 0;

	status = vos_mq_post_message(VOS_MQ_ID_SME, &vos_msg);
	if (status != VOS_STATUS_SUCCESS) {
	    WMA_LOGE("%s: Failed to post NaN response event to SME", __func__);
	    vos_mem_free(nan_rsp_event);
	    return -1;
	}
	WMA_LOGD("%s: NaN response event Posted to SME", __func__);
	return 0;
}
#endif

/*
 * WMI handler for WMI_DFS_RADAR_EVENTID
 * This handler is registered for handling
 * filtered DFS Phyerror. This handler is
 * will be invoked only when DFS Phyerr
 * filtering offload is enabled.
 * Return- 1:Success, 0:Failure
 */
static int wma_unified_dfs_radar_rx_event_handler(void *handle,
	u_int8_t *data, u_int32_t datalen)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	struct ieee80211com *ic;
	struct ath_dfs *dfs;
	struct dfs_event *event;
	struct ieee80211_channel *chan;
	int empty;
	int do_check_chirp = 0;
	int is_hw_chirp = 0;
	int is_sw_chirp = 0;
	int is_pri = 0;

	WMI_DFS_RADAR_EVENTID_param_tlvs *param_tlvs;
	wmi_dfs_radar_event_fixed_param *radar_event;

	ic = wma->dfs_ic;
	if (NULL == ic) {
		WMA_LOGE("%s: dfs_ic is  NULL ", __func__);
		return 0;
	}

	dfs = (struct ath_dfs *)ic->ic_dfs;
	param_tlvs = (WMI_DFS_RADAR_EVENTID_param_tlvs *) data;

	if (NULL == dfs) {
		WMA_LOGE("%s: dfs is  NULL ", __func__);
		return 0;
	}
	/*
	 * This parameter holds the number
	 * of phyerror interrupts to the host
	 * after the phyerrors have passed through
	 * false detect filters in the firmware.
	 */
	dfs->dfs_phyerr_count++;

	if (!param_tlvs) {
		WMA_LOGE("%s: Received NULL data from FW", __func__);
		return 0;
	}

	radar_event = param_tlvs->fixed_param;

	vos_lock_acquire(&ic->chan_lock);
	chan = ic->ic_curchan;
	if (NV_CHANNEL_DFS != vos_nv_getChannelEnabledState(chan->ic_ieee)) {
		WMA_LOGE("%s: Invalid DFS Phyerror event. Channel=%d is Non-DFS",
			 __func__, chan->ic_ieee);
		vos_lock_release(&ic->chan_lock);
		return 0;
	}

	vos_lock_release(&ic->chan_lock);
	dfs->ath_dfs_stats.total_phy_errors++;

	if (dfs->dfs_caps.ath_chip_is_bb_tlv) {
		do_check_chirp = 1;
		is_pri = 1;
		is_hw_chirp = radar_event->pulse_is_chirp;

		if ((u_int32_t)dfs->dfs_phyerr_freq_min >
			radar_event->pulse_center_freq) {
			dfs->dfs_phyerr_freq_min =
					(int)radar_event->pulse_center_freq;
		}

		if (dfs->dfs_phyerr_freq_max <
			(int)radar_event->pulse_center_freq) {
			dfs->dfs_phyerr_freq_max =
					(int)radar_event->pulse_center_freq;
		}
	}

	/*
	 * Now, add the parsed, checked and filtered
	 * radar phyerror event radar pulse event list.
	 * This event will then be processed by
	 * dfs_radar_processevent() to see if the pattern
	 * of pulses in radar pulse list match any radar
	 * singnature in the current regulatory domain.
	 */

	ATH_DFSEVENTQ_LOCK(dfs);
	empty = STAILQ_EMPTY(&(dfs->dfs_eventq));
	ATH_DFSEVENTQ_UNLOCK(dfs);
	if (empty) {
		return 0;
	}
	/*
	 * Add the event to the list, if there's space.
	 */
	ATH_DFSEVENTQ_LOCK(dfs);
	event = STAILQ_FIRST(&(dfs->dfs_eventq));
	if (event == NULL) {
		ATH_DFSEVENTQ_UNLOCK(dfs);
		WMA_LOGE("%s: No more space left for queuing DFS Phyerror events",
					__func__);
		return 0;
	}
	STAILQ_REMOVE_HEAD(&(dfs->dfs_eventq), re_list);
	ATH_DFSEVENTQ_UNLOCK(dfs);
	dfs->dfs_phyerr_queued_count++;
	dfs->dfs_phyerr_w53_counter++;
	event->re_dur = (u_int8_t)radar_event->pulse_duration;
	event->re_rssi = radar_event->rssi;
	event->re_ts = radar_event->pulse_detect_ts & DFS_TSMASK;
	event->re_full_ts = (((uint64_t)radar_event->upload_fullts_high) << 32)
			| radar_event->upload_fullts_low;

	/*
	 * Handle chirp flags.
	 */
	if (do_check_chirp) {
		event->re_flags |= DFS_EVENT_CHECKCHIRP;
		if (is_hw_chirp) {
			event->re_flags |= DFS_EVENT_HW_CHIRP;
		}
		if (is_sw_chirp) {
			event->re_flags |= DFS_EVENT_SW_CHIRP;
		}
	}
	/*
	 * Correctly set which channel is being reported on
	 */
	if (is_pri) {
		event->re_chanindex = (u_int8_t)dfs->dfs_curchan_radindex;
	} else {
		if (dfs->dfs_extchan_radindex == -1) {
			WMA_LOGI("%s phyerr on ext channel", __func__);
		}
		event->re_chanindex = (u_int8_t)dfs->dfs_extchan_radindex;
		WMA_LOGI("%s:New extension channel event is added to queue",
					__func__);
	}

	ATH_DFSQ_LOCK(dfs);

	STAILQ_INSERT_TAIL(&(dfs->dfs_radarq), event, re_list);

	empty = STAILQ_EMPTY(&dfs->dfs_radarq);

	ATH_DFSQ_UNLOCK(dfs);

	if (!empty && !dfs->ath_radar_tasksched) {
		dfs->ath_radar_tasksched = 1;
		OS_SET_TIMER(&dfs->ath_dfs_task_timer, 0);
	}

	return 1;

}

/*
 * Register appropriate dfs phyerror event handler
 * based on phyerror filtering offload is enabled
 * or disabled.
 */
static void
wma_register_dfs_event_handler(tp_wma_handle wma_handle)
{
	if (NULL == wma_handle) {
		WMA_LOGE("%s:wma_handle is NULL", __func__);
		return;
	}

	if (VOS_FALSE == wma_handle->dfs_phyerr_filter_offload) {
		/*
		 * Register the wma_unified_phyerr_rx_event_handler
		 * for filtering offload disabled case to handle
		 * the DFS phyerrors.
		 */
		WMA_LOGD("%s:Phyerror Filtering offload is Disabled in ini",
					__func__);
		wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_PHYERR_EVENTID, wma_unified_phyerr_rx_event_handler);
		WMA_LOGD("%s: WMI_PHYERR_EVENTID event handler registered",
					__func__);
	} else {
		WMA_LOGD("%s:Phyerror Filtering offload is Enabled in ini",
					__func__);
		wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_DFS_RADAR_EVENTID,
				wma_unified_dfs_radar_rx_event_handler);
		WMA_LOGD("%s:WMI_DFS_RADAR_EVENTID event handler registered",
					__func__);
	}

	return;
}

static int wma_peer_state_change_event_handler(void *handle,
					       u_int8_t *event_buff,
					       u_int32_t len)
{
	WMI_PEER_STATE_EVENTID_param_tlvs *param_buf;
	wmi_peer_state_event_fixed_param *event;
	ol_txrx_vdev_handle vdev;
	tp_wma_handle wma_handle = (tp_wma_handle)handle;

	param_buf = (WMI_PEER_STATE_EVENTID_param_tlvs *)event_buff;
	if (!param_buf) {
		WMA_LOGE("%s: Received NULL buf ptr from FW", __func__);
		return -ENOMEM;
	}

	event = param_buf->fixed_param;
	vdev = wma_find_vdev_by_id( wma_handle, event->vdev_id);
	if (NULL == vdev) {
		WMA_LOGP("%s: Couldn't find vdev for vdev_id: %d",
		__func__, event->vdev_id);
		return -EINVAL;
	}

	if (vdev->opmode == wlan_op_mode_sta
		&& event->state == WMI_PEER_STATE_AUTHORIZED) {
		/*
		 * set event so that WLANTL_ChangeSTAState
		 * can procced and unpause tx queue
		 */
		tl_shim_set_peer_authorized_event(wma_handle->vos_context,
						  event->vdev_id);
	}

	return 0;
}

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
/*
 * Register all the Link Layer Stats related event
 * handler
 */
static void
wma_register_ll_stats_event_handler(tp_wma_handle wma_handle)
{
	if (NULL == wma_handle) {
		WMA_LOGE("%s: wma_handle is NULL", __func__);
		return;
	}

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_IFACE_LINK_STATS_EVENTID,
			wma_unified_link_iface_stats_event_handler);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_PEER_LINK_STATS_EVENTID,
			wma_unified_link_peer_stats_event_handler);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_RADIO_LINK_STATS_EVENTID,
			wma_unified_link_radio_stats_event_handler);

	return;
}
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static int wma_roam_synch_event_handler(void *handle, u_int8_t *event, u_int32_t len)
{
	WMI_ROAM_SYNCH_EVENTID_param_tlvs *param_buf = NULL;
	wmi_roam_synch_event_fixed_param *synch_event = NULL;
	u_int8_t *bcn_probersp_ptr = NULL;
	u_int8_t *reassoc_rsp_ptr = NULL;
        tp_wma_handle wma = (tp_wma_handle)handle;
	wmi_channel *chan = NULL;
	wmi_key_material *key = NULL;
	int size=0;
	tSirRoamOffloadSynchInd *pRoamOffloadSynchInd;

	WMA_LOGD("LFR3:%s", __func__);
	if (!event) {
	 WMA_LOGE("%s: event param null", __func__);
	 return -EINVAL;
	}

	param_buf = (WMI_ROAM_SYNCH_EVENTID_param_tlvs *) event;
	if (!param_buf) {
	 WMA_LOGE("%s: received null buf from target", __func__);
	 return -EINVAL;
	}

	synch_event = param_buf->fixed_param;
	if (!synch_event) {
	 WMA_LOGE("%s: received null event data from target", __func__);
	 return -EINVAL;
	}

	if(wma->interfaces[synch_event->vdev_id].roam_synch_in_progress ==
		VOS_TRUE) {
	  WMA_LOGE("%s: Ignoring RSI since one is already in progress",
	  __func__);
	  return -EINVAL;
	}
	adf_os_spin_lock_bh(&wma->roam_synch_lock);
	wma->interfaces[synch_event->vdev_id].roam_synch_in_progress = VOS_TRUE;
	adf_os_spin_unlock_bh(&wma->roam_synch_lock);
	len = sizeof(tSirRoamOffloadSynchInd) +
		synch_event->bcn_probe_rsp_len +
		synch_event->reassoc_rsp_len;
	pRoamOffloadSynchInd = (tSirRoamOffloadSynchInd *)vos_mem_malloc(len);
	if (!pRoamOffloadSynchInd) {
	 WMA_LOGE("%s: failed to allocate memory for roam_synch_event", __func__);
	 return -ENOMEM;
	}
	/* abort existing scan if any */
	if (wma->interfaces[synch_event->vdev_id].scan_info.scan_id != 0) {
		tAbortScanParams abortScan;
		WMA_LOGD("LFR3: Aborting Scan with scan_id=%d\n",
		wma->interfaces[synch_event->vdev_id].scan_info.scan_id);
		abortScan.SessionId = synch_event->vdev_id;
		wma_stop_scan(wma, &abortScan);
	}
	pRoamOffloadSynchInd->messageType = eWNI_SME_ROAM_OFFLOAD_SYNCH_IND;
	pRoamOffloadSynchInd->length = size;
	pRoamOffloadSynchInd->roamedVdevId = synch_event->vdev_id;
	pRoamOffloadSynchInd->authStatus = synch_event->auth_status;
	pRoamOffloadSynchInd->roamReason = synch_event->roam_reason;
	pRoamOffloadSynchInd->rssi = synch_event->rssi;
	pRoamOffloadSynchInd->isBeacon = synch_event->is_beacon;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&synch_event->bssid, pRoamOffloadSynchInd->bssId);
	pRoamOffloadSynchInd->beaconProbeRespOffset = sizeof(tSirRoamOffloadSynchInd);
	bcn_probersp_ptr = (tANI_U8 *)pRoamOffloadSynchInd +
		pRoamOffloadSynchInd->beaconProbeRespOffset;
	pRoamOffloadSynchInd->beaconProbeRespLength = synch_event->bcn_probe_rsp_len;
	vos_mem_copy(bcn_probersp_ptr, param_buf->bcn_probe_rsp_frame,
			pRoamOffloadSynchInd->beaconProbeRespLength);
	pRoamOffloadSynchInd->reassocRespOffset = sizeof(tSirRoamOffloadSynchInd) +
		pRoamOffloadSynchInd->beaconProbeRespLength;
	pRoamOffloadSynchInd->reassocRespLength = synch_event->reassoc_rsp_len;
	reassoc_rsp_ptr = (tANI_U8 *)pRoamOffloadSynchInd +
		pRoamOffloadSynchInd->reassocRespOffset;
	vos_mem_copy(reassoc_rsp_ptr,
			param_buf->reassoc_rsp_frame,
			pRoamOffloadSynchInd->reassocRespLength);
	chan = (wmi_channel *) param_buf->chan;
	pRoamOffloadSynchInd->chan_freq = chan->mhz;
	key = (wmi_key_material *) param_buf->key;
	if (key != NULL)
	{
		VOS_TRACE_HEX_DUMP(VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_DEBUG,
				key->replay_counter,
				SIR_REPLAY_CTR_LEN);
		vos_mem_copy(pRoamOffloadSynchInd->kck, key->kck,
				SIR_KCK_KEY_LEN);
		vos_mem_copy(pRoamOffloadSynchInd->kek, key->kek,
				SIR_KEK_KEY_LEN);
		vos_mem_copy(pRoamOffloadSynchInd->replay_ctr, key->replay_counter,
				SIR_REPLAY_CTR_LEN);
	}
	wma_send_msg(wma, WDA_ROAM_OFFLOAD_SYNCH_IND,
			(void *) pRoamOffloadSynchInd, 0);
	return 0;
}
#endif

/**
 * wma_rssi_breached_event_handler() - rssi breached event handler
 * @handle: wma handle
 * @cmd_param_info: event handler data
 * @len: length of @cmd_param_info
 *
 * Return: 0 on success; error number otherwise
 */
static int wma_rssi_breached_event_handler(void *handle,
				u_int8_t  *cmd_param_info, u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	WMI_RSSI_BREACH_EVENTID_param_tlvs *param_buf;
	wmi_rssi_breach_event_fixed_param  *event;
	struct rssi_breach_event  rssi;
	tpAniSirGlobal mac = (tpAniSirGlobal)vos_get_context(
					VOS_MODULE_ID_PE, wma->vos_context);
	if (!mac) {
		WMA_LOGE("%s: Invalid mac context", __func__);
		return -EINVAL;
	}
	if (!mac->sme.rssi_threshold_breached_cb) {
		WMA_LOGE("%s: Callback not registered", __func__);
		return -EINVAL;
	}
	param_buf = (WMI_RSSI_BREACH_EVENTID_param_tlvs *)cmd_param_info;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid rssi breached event", __func__);
		return -EINVAL;
	}
	event = param_buf->fixed_param;

	rssi.request_id = event->request_id;
	rssi.session_id = event->vdev_id;
	rssi.curr_rssi = event->rssi + WMA_TGT_NOISE_FLOOR_DBM;
	WMI_MAC_ADDR_TO_CHAR_ARRAY(&event->bssid, rssi.curr_bssid.bytes);

	WMA_LOGD("%s: req_id: %u vdev_id: %d curr_rssi: %d", __func__,
		rssi.request_id, rssi.session_id, rssi.curr_rssi);
	WMA_LOGI("%s: curr_bssid: %pM", __func__, rssi.curr_bssid.bytes);

	mac->sme.rssi_threshold_breached_cb(mac->hHdd, &rssi);
	WMA_LOGD("%s: Invoke HDD rssi breached callback", __func__);
	return 0;
}

/*
 * Send WMI_DFS_PHYERR_FILTER_ENA_CMDID or
 * WMI_DFS_PHYERR_FILTER_DIS_CMDID command
 * to firmware based on phyerr filtering
 * offload status.
 */
static int
wma_unified_dfs_phyerr_filter_offload_enable(tp_wma_handle wma_handle)
{
	wmi_dfs_phyerr_filter_ena_cmd_fixed_param* enable_phyerr_offload_cmd;
	wmi_dfs_phyerr_filter_dis_cmd_fixed_param* disable_phyerr_offload_cmd;
	wmi_buf_t buf;
	u_int16_t len;
	int ret;

	if (NULL == wma_handle) {
		WMA_LOGE("%s:wma_handle is NULL", __func__);
		return 0;
	}

	if (VOS_FALSE == wma_handle->dfs_phyerr_filter_offload) {
		WMA_LOGD("%s:Phyerror Filtering offload is Disabled in ini",
					__func__);
		len = sizeof(*disable_phyerr_offload_cmd);
		buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
		if (!buf) {
			WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
			return 0;
		}
		disable_phyerr_offload_cmd =
			(wmi_dfs_phyerr_filter_dis_cmd_fixed_param *)
			wmi_buf_data(buf);

		WMITLV_SET_HDR(&disable_phyerr_offload_cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_dfs_phyerr_filter_dis_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_dfs_phyerr_filter_dis_cmd_fixed_param));

		/*
		 * Send WMI_DFS_PHYERR_FILTER_DIS_CMDID
		 * to the firmware to disable the phyerror
		 * filtering offload.
		 */
		ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
			WMI_DFS_PHYERR_FILTER_DIS_CMDID);
		if (ret < 0) {
			WMA_LOGE("%s: Failed to send WMI_DFS_PHYERR_FILTER_DIS_CMDID ret=%d",
						__func__, ret);
			wmi_buf_free(buf);
			return 0;
		}
		WMA_LOGD("%s: WMI_DFS_PHYERR_FILTER_DIS_CMDID Send Success",
					__func__);
	} else {
		WMA_LOGD("%s:Phyerror Filtering offload is Enabled in ini",
					__func__);

		len = sizeof(*enable_phyerr_offload_cmd);
		buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
		if (!buf) {
			WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
			return 0;
		}

		enable_phyerr_offload_cmd =
			(wmi_dfs_phyerr_filter_ena_cmd_fixed_param *)
			wmi_buf_data(buf);

		WMITLV_SET_HDR(&enable_phyerr_offload_cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_dfs_phyerr_filter_ena_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_dfs_phyerr_filter_ena_cmd_fixed_param));

		/*
		 * Send a WMI_DFS_PHYERR_FILTER_ENA_CMDID
		 * to the firmware to enable the phyerror
		 * filtering offload.
		 */
		ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
					WMI_DFS_PHYERR_FILTER_ENA_CMDID);

		if (ret < 0) {
			WMA_LOGE("%s: Failed to send WMI_DFS_PHYERR_FILTER_ENA_CMDID ret=%d",
						__func__, ret);
			wmi_buf_free(buf);
			return 0;
		}
		WMA_LOGD("%s: WMI_DFS_PHYERR_FILTER_ENA_CMDID Send Success",
					__func__);
	}

	return 1;
}

/*
 * WMI Handler for WMI_OFFLOAD_BCN_TX_STATUS_EVENTID event from firmware.
 * This event is generated by FW when the beacon transmission is offloaded
 * and the host performs beacon template modification using WMI_BCN_TMPL_CMDID
 * The FW generates this event when the first successful beacon transmission
 * after template update
 * Return- 1:Success, 0:Failure
 */
static int wma_unified_bcntx_status_event_handler(void *handle, u_int8_t *cmd_param_info,
                                                              u_int32_t len)
{
   tp_wma_handle wma = (tp_wma_handle) handle;
   WMI_OFFLOAD_BCN_TX_STATUS_EVENTID_param_tlvs *param_buf;
   wmi_offload_bcn_tx_status_event_fixed_param *resp_event;
   tSirFirstBeaconTxCompleteInd *beacon_tx_complete_ind;

   param_buf = (WMI_OFFLOAD_BCN_TX_STATUS_EVENTID_param_tlvs *) cmd_param_info;
   if (!param_buf) {
      WMA_LOGE("Invalid bcn tx response event buffer");
      return -EINVAL;
   }

   resp_event = param_buf->fixed_param;

   /* Check for valid handle to ensure session is not deleted in any race */
   if (!wma->interfaces[resp_event->vdev_id].handle) {
      WMA_LOGE("%s: The session does not exist", __func__);
      return -EINVAL;
   }

   /* Beacon Tx Indication supports only AP mode. Ignore in other modes */
   if ((wma->interfaces[resp_event->vdev_id].type != WMI_VDEV_TYPE_AP) ||
       (wma->interfaces[resp_event->vdev_id].sub_type != 0)) {
      WMA_LOGI("%s: Beacon Tx Indication does not support type %d and sub_type %d",
                    __func__, wma->interfaces[resp_event->vdev_id].type,
                     wma->interfaces[resp_event->vdev_id].sub_type);
      return 0;
   }

   beacon_tx_complete_ind = (tSirFirstBeaconTxCompleteInd *)
               vos_mem_malloc(sizeof(tSirFirstBeaconTxCompleteInd));
   if (!beacon_tx_complete_ind) {
	   WMA_LOGE("%s: Failed to alloc beacon_tx_complete_ind", __func__);
	   return -ENOMEM;
   }

   beacon_tx_complete_ind->messageType = WDA_DFS_BEACON_TX_SUCCESS_IND;
   beacon_tx_complete_ind->length = sizeof(tSirFirstBeaconTxCompleteInd);
   beacon_tx_complete_ind->bssIdx = resp_event->vdev_id;

   wma_send_msg(wma, WDA_DFS_BEACON_TX_SUCCESS_IND, (void *)beacon_tx_complete_ind, 0);
   return 0;
}

static int wma_vdev_install_key_complete_event_handler(void *handle, u_int8_t *event, u_int32_t len)
{
    WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID_param_tlvs *param_buf = NULL;
    wmi_vdev_install_key_complete_event_fixed_param *key_fp = NULL;

    if (!event) {
        WMA_LOGE("%s: event param null", __func__);
        return -1;
    }

    param_buf = (WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID_param_tlvs *) event;
    if (!param_buf) {
        WMA_LOGE("%s: received null buf from target", __func__);
        return -1;
    }

    key_fp = param_buf->fixed_param;
    if (!key_fp) {
        WMA_LOGE("%s: received null event data from target", __func__);
        return -1;
    }
    /*
     * Do nothing for now. Completion of set key is already indicated to lim
     */
    WMA_LOGI("%s: WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID", __func__);
    return 0;
}

#ifdef WLAN_FEATURE_STATS_EXT
static int wma_stats_ext_event_handler(void *handle, u_int8_t *event_buf,
				       u_int32_t len)
{
	WMI_STATS_EXT_EVENTID_param_tlvs *param_buf;
	tSirStatsExtEvent *stats_ext_event;
	wmi_stats_ext_event_fixed_param *stats_ext_info;
	VOS_STATUS status;
	vos_msg_t vos_msg;
	u_int8_t *buf_ptr;
	u_int32_t alloc_len;

	WMA_LOGD("%s: Posting stats ext event to SME", __func__);

	param_buf = (WMI_STATS_EXT_EVENTID_param_tlvs *)event_buf;
	if (!param_buf) {
		WMA_LOGE("%s: Invalid stats ext event buf", __func__);
		return -EINVAL;
	}

	stats_ext_info =  param_buf->fixed_param;
	buf_ptr = (u_int8_t *)stats_ext_info;

	alloc_len = sizeof(tSirStatsExtEvent);
	alloc_len += stats_ext_info->data_len;

	stats_ext_event = (tSirStatsExtEvent *) vos_mem_malloc(alloc_len);
	if (NULL == stats_ext_event) {
		WMA_LOGE("%s: Memory allocation failure", __func__);
		return -ENOMEM;
	}

	buf_ptr +=  sizeof(wmi_stats_ext_event_fixed_param) + WMI_TLV_HDR_SIZE ;

	stats_ext_event->vdev_id = stats_ext_info->vdev_id;
	stats_ext_event->event_data_len = stats_ext_info->data_len;
	vos_mem_copy(stats_ext_event->event_data,
		     buf_ptr,
		     stats_ext_event->event_data_len);

	vos_msg.type = eWNI_SME_STATS_EXT_EVENT;
	vos_msg.bodyptr = (void *)stats_ext_event;
	vos_msg.bodyval = 0;

	status = vos_mq_post_message(VOS_MQ_ID_SME, &vos_msg);
	if (status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("%s: Failed to post stats ext event to SME", __func__);
		vos_mem_free(stats_ext_event);
		return -1;
	}

	WMA_LOGD("%s: stats ext event Posted to SME", __func__);
	return 0;
}
#endif

static void
wma_register_extscan_event_handler(tp_wma_handle wma_handle)
{
	if (!wma_handle) {
		WMA_LOGE("%s: extscan wma_handle is NULL", __func__);
		return;
	}
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_EXTSCAN_START_STOP_EVENTID,
				wma_extscan_start_stop_event_handler);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_EXTSCAN_CAPABILITIES_EVENTID,
				wma_extscan_capabilities_event_handler);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_EXTSCAN_HOTLIST_MATCH_EVENTID,
				wma_extscan_hotlist_match_event_handler);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID,
				wma_extscan_change_results_event_handler);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_EXTSCAN_OPERATION_EVENTID,
				wma_extscan_operations_event_handler);
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_EXTSCAN_TABLE_USAGE_EVENTID,
				wma_extscan_table_usage_event_handler);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_EXTSCAN_CACHED_RESULTS_EVENTID,
			wma_extscan_cached_results_event_handler);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_PASSPOINT_MATCH_EVENTID,
			wma_passpoint_match_event_handler);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_EXTSCAN_HOTLIST_SSID_MATCH_EVENTID,
			wma_extscan_hotlist_ssid_match_event_handler);

	return;

}

void wma_wow_tx_complete(void *wma)
{
	tp_wma_handle wma_handle = (tp_wma_handle)wma;
	WMA_LOGD("WOW_TX_COMPLETE DONE");
	vos_event_set(&wma_handle->wow_tx_complete);
}

/*
 * Allocate and init wmi adaptation layer.
 */
VOS_STATUS WDA_open(v_VOID_t *vos_context, v_VOID_t *os_ctx,
		    wda_tgt_cfg_cb tgt_cfg_cb,
		    wda_dfs_radar_indication_cb radar_ind_cb,
		    tMacOpenParameters *mac_params)
{
	tp_wma_handle wma_handle;
	HTC_HANDLE htc_handle;
	adf_os_device_t adf_dev;
	v_VOID_t *wmi_handle;
	VOS_STATUS vos_status;
	struct ol_softc *scn;
	struct txrx_pdev_cfg_param_t olCfg = {0};

	WMA_LOGD("%s: Enter", __func__);

	adf_dev = vos_get_context(VOS_MODULE_ID_ADF, vos_context);
	htc_handle = vos_get_context(VOS_MODULE_ID_HTC, vos_context);

	if (!htc_handle) {
		WMA_LOGP("%s: Invalid HTC handle", __func__);
		return VOS_STATUS_E_INVAL;
	}

	/* Alloc memory for WMA Context */
	vos_status = vos_alloc_context(vos_context, VOS_MODULE_ID_WDA,
				       (v_VOID_t **) &wma_handle,
				       sizeof (t_wma_handle));

	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGP("%s: Memory allocation failed for wma_handle",
			__func__);
		return vos_status;
	}

	vos_mem_zero(wma_handle, sizeof (t_wma_handle));

	if (vos_get_conparam() != VOS_FTM_MODE) {
#ifdef FEATURE_WLAN_SCAN_PNO
		vos_wake_lock_init(&wma_handle->pno_wake_lock, "wlan_pno_wl");
#endif
#ifdef FEATURE_WLAN_EXTSCAN
		vos_wake_lock_init(&wma_handle->extscan_wake_lock,
					"wlan_extscan_wl");
#endif
		vos_wake_lock_init(&wma_handle->wow_wake_lock, "wlan_wow_wl");
	}

	/* attach the wmi */
	wmi_handle = wmi_unified_attach(wma_handle, wma_wow_tx_complete);
	if (!wmi_handle) {
		WMA_LOGP("%s: failed to attach WMI", __func__);
		vos_status = VOS_STATUS_E_NOMEM;
		goto err_wma_handle;
	}

	WMA_LOGA("WMA --> wmi_unified_attach - success");

	/* Save the WMI & HTC handle */
	wma_handle->wmi_handle = wmi_handle;
	wma_handle->htc_handle = htc_handle;
	wma_handle->vos_context = vos_context;
	wma_handle->adf_dev = adf_dev;

	/* initialize default target config */
	wma_set_default_tgt_config(wma_handle);

#ifdef IPA_UC_OFFLOAD
	olCfg.is_uc_offload_enabled = mac_params->ucOffloadEnabled;
	olCfg.uc_tx_buffer_count = mac_params->ucTxBufCount;
	olCfg.uc_tx_buffer_size = mac_params->ucTxBufSize;
	olCfg.uc_rx_indication_ring_count = mac_params->ucRxIndRingCount;
	olCfg.uc_tx_partition_base = mac_params->ucTxPartitionBase;
#endif /* IPA_UC_OFFLOAD*/

	wma_handle->tx_chain_mask_cck = mac_params->tx_chain_mask_cck;
	wma_handle->self_gen_frm_pwr = mac_params->self_gen_frm_pwr;

	/* Allocate cfg handle */

	/* RX Full reorder should enable for PCIe, ROME3.X project only now
	 * MDM should enable later, schedule TBD
	 * HL also sdould be enabled, schedule TBD */
#ifdef WLAN_FEATURE_RX_FULL_REORDER_OL
	olCfg.is_full_reorder_offload = mac_params->reorderOffload;
#else
	olCfg.is_full_reorder_offload = 0;
#endif
	((pVosContextType) vos_context)->cfg_ctx =
		ol_pdev_cfg_attach(((pVosContextType) vos_context)->adf_ctx, olCfg);
	if (!(((pVosContextType) vos_context)->cfg_ctx)) {
		WMA_LOGP("%s: failed to init cfg handle", __func__);
		vos_status = VOS_STATUS_E_NOMEM;
		goto err_wmi_handle;
	}

	/* adjust the cfg_ctx default value based on setting */
	wdi_in_set_cfg_rx_fwd_disabled((ol_pdev_handle)((pVosContextType)vos_context)->cfg_ctx,
		(u_int8_t)mac_params->apDisableIntraBssFwd);

	/* adjust the packet log enable default value based on CFG INI setting */
	wdi_in_set_cfg_pakcet_log_enabled((ol_pdev_handle)
		((pVosContextType)vos_context)->cfg_ctx, (u_int8_t)vos_is_packet_log_enabled());


	/* Allocate dfs_ic and initialize DFS */
	wma_handle->dfs_ic = wma_dfs_attach(wma_handle->dfs_ic);
	if(wma_handle->dfs_ic == NULL) {
		WMA_LOGE("%s: Memory allocation failed for dfs_ic", __func__);
		goto err_wmi_handle;
	}

#if defined(QCA_WIFI_FTM)
	if (vos_get_conparam() == VOS_FTM_MODE)
		wma_utf_attach(wma_handle);
#endif

        /*TODO: Recheck below parameters */
	scn = vos_get_context(VOS_MODULE_ID_HIF, vos_context);

	if (NULL == scn) {
		WMA_LOGE("%s: Failed to get scn",__func__);
		vos_status = VOS_STATUS_E_NOMEM;
		goto err_scn_context;
	}

	mac_params->maxStation = ol_get_number_of_peers_supported(scn);

	mac_params->maxBssId = WMA_MAX_SUPPORTED_BSS;
	mac_params->frameTransRequired = 0;

	wma_handle->wlan_resource_config.num_wow_filters = mac_params->maxWoWFilters;
	wma_handle->wlan_resource_config.num_keep_alive_pattern = WMA_MAXNUM_PERIODIC_TX_PTRNS;

	/* The current firmware implementation requires the number of offload peers
	* should be (number of vdevs + 1).
	*/
	wma_handle->wlan_resource_config.num_offload_peers =
		mac_params->apMaxOffloadPeers + 1;

	wma_handle->wlan_resource_config.num_offload_reorder_buffs =
		mac_params->apMaxOffloadReorderBuffs + 1;

	wma_handle->ol_ini_info = mac_params->olIniInfo;
	wma_handle->max_station = mac_params->maxStation;
	wma_handle->max_bssid = mac_params->maxBssId;
	wma_handle->frame_xln_reqd = mac_params->frameTransRequired;
	wma_handle->driver_type = mac_params->driverType;
	wma_handle->ssdp = mac_params->ssdp;
#ifdef FEATURE_WLAN_RA_FILTERING
	wma_handle->IsRArateLimitEnabled = mac_params->IsRArateLimitEnabled;
	wma_handle->RArateLimitInterval = mac_params->RArateLimitInterval;
#endif
	/*
	 * Indicates if DFS Phyerr filtering offload
	 * is Enabled/Disabed from ini
	 */
	wma_handle->dfs_phyerr_filter_offload =
						mac_params->dfsPhyerrFilterOffload;
	wma_handle->dfs_pri_multiplier =
					mac_params->dfsRadarPriMultiplier;
	wma_handle->interfaces = vos_mem_malloc(sizeof(struct wma_txrx_node) *
						wma_handle->max_bssid);
	if (!wma_handle->interfaces) {
		WMA_LOGP("%s: failed to allocate interface table", __func__);
		vos_status = VOS_STATUS_E_NOMEM;
		goto err_scn_context;
	}
	vos_mem_zero(wma_handle->interfaces, sizeof(struct wma_txrx_node) *
					wma_handle->max_bssid);
	/* Register the debug print event handler */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   WMI_DEBUG_PRINT_EVENTID,
					   wma_unified_debug_print_event_handler);

	wma_handle->tgt_cfg_update_cb = tgt_cfg_cb;
	wma_handle->dfs_radar_indication_cb = radar_ind_cb;

        vos_status = vos_event_init(&wma_handle->wma_ready_event);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGP("%s: wma_ready_event initialization failed", __func__);
		goto err_event_init;
	}
        vos_status = vos_event_init(&wma_handle->target_suspend);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGP("%s: target suspend event initialization failed",
				__func__);
		goto err_event_init;
	}

	vos_status = vos_event_init(&wma_handle->wow_tx_complete);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGP("%s: wow_tx_complete event initialization failed",
				__func__);
		goto err_event_init;
	}

	/* Init Tx Frame Complete event */
	vos_status = vos_event_init(&wma_handle->tx_frm_download_comp_event);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		WMA_LOGP("%s: failed to init tx_frm_download_comp_event",
				__func__);
		goto err_event_init;
	}

	/* Init tx queue empty check event */
	vos_status = vos_event_init(&wma_handle->tx_queue_empty_event);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		WMA_LOGP("%s: failed to init tx_queue_empty_event",
			 __func__);
		goto err_event_init;
	}

	vos_status = vos_event_init(&wma_handle->wma_resume_event);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGP("%s: wma_resume_event initialization failed", __func__);
		goto err_event_init;
	}

	vos_status = vos_event_init(&wma_handle->recovery_event);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGP("%s: recovery event initialization failed", __func__);
		goto err_event_init;
	}

	vos_status = vos_event_init(&wma_handle->runtime_suspend);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGP("%s: runtime suspend event initialization failed",
				__func__);
		goto err_event_init;
	}

	INIT_LIST_HEAD(&wma_handle->vdev_resp_queue);
	adf_os_spinlock_init(&wma_handle->vdev_respq_lock);
	adf_os_spinlock_init(&wma_handle->vdev_detach_lock);
	adf_os_spinlock_init(&wma_handle->roam_preauth_lock);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	adf_os_spinlock_init(&wma_handle->roam_synch_lock);
#endif
	adf_os_atomic_init(&wma_handle->is_wow_bus_suspended);

	/* Register vdev start response event handler */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   WMI_VDEV_START_RESP_EVENTID,
					   wma_vdev_start_resp_handler);

	/* Register vdev stop response event handler */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   WMI_VDEV_STOPPED_EVENTID,
					   wma_vdev_stop_resp_handler);

	/* register for STA kickout function */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   WMI_PEER_STA_KICKOUT_EVENTID,
					   wma_peer_sta_kickout_event_handler);

	/* register for stats response event */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   WMI_UPDATE_STATS_EVENTID,
					   wma_stats_event_handler);
	/* register for linkspeed response event */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
						WMI_PEER_ESTIMATED_LINKSPEED_EVENTID,
						wma_link_speed_event_handler);

#ifdef FEATURE_OEM_DATA_SUPPORT
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				    WMI_OEM_CAPABILITY_EVENTID,
				    wma_oem_capability_event_callback);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				    WMI_OEM_MEASUREMENT_REPORT_EVENTID,
				    wma_oem_measurement_report_event_callback);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				    WMI_OEM_ERROR_REPORT_EVENTID,
				    wma_oem_error_report_event_callback);
#endif
	/*
	 * Register appropriate DFS phyerr event handler for
	 * Phyerror events. Handlers differ for phyerr filtering
	 * offload enable and disable cases.
	 */
	wma_register_dfs_event_handler(wma_handle);

	/* Register peer change event handler */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   WMI_PEER_STATE_EVENTID,
					   wma_peer_state_change_event_handler);


   /* Register beacon tx complete event id. The event is required
    * for sending channel switch announcement frames
    */
   wmi_unified_register_event_handler(wma_handle->wmi_handle,
                  WMI_OFFLOAD_BCN_TX_STATUS_EVENTID,
                  wma_unified_bcntx_status_event_handler);

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   WMI_UPDATE_VDEV_RATE_STATS_EVENTID,
					   wma_link_status_event_handler);
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
	/* Register event handler for processing Link Layer Stats
	 * response from the FW
	 */
	wma_register_ll_stats_event_handler(wma_handle);

#endif /* WLAN_FEATURE_LINK_LAYER_STATS */
	/* Register event handler to receive firmware mem dump
	 * copy complete indication
	 */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
		WMI_UPDATE_FW_MEM_DUMP_EVENTID,
		wma_fw_mem_dump_event_handler);

	/* Firmware debug log */
	vos_status = dbglog_init(wma_handle->wmi_handle);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGP("%s: Firmware Dbglog initialization failed", __func__);
		goto err_dbglog_init;
	}

	/*
	 * Update Powersave mode
	 * 1 - Legacy Powersave + Deepsleep Disabled
	 * 2 - QPower + Deepsleep Disabled
	 * 3 - Legacy Powersave + Deepsleep Enabled
	 * 4 - QPower + Deepsleep Enabled
	 */
	wma_handle->powersave_mode = mac_params->powersaveOffloadEnabled;
	wma_handle->staMaxLIModDtim = mac_params->staMaxLIModDtim;
	wma_handle->staModDtim = mac_params->staModDtim;
	wma_handle->staDynamicDtim = mac_params->staDynamicDtim;

	/*
	 * Value of mac_params->wowEnable can be,
	 * 0 - Disable both magic pattern match and pattern byte match.
	 * 1 - Enable magic pattern match on all interfaces.
	 * 2 - Enable pattern byte match on all interfaces.
	 * 3 - Enable both magic patter and pattern byte match on
	 *     all interfaces.
	 */
	wma_handle->wow.magic_ptrn_enable =
			(mac_params->wowEnable & 0x01) ? TRUE : FALSE;
	wma_handle->ptrn_match_enable_all_vdev =
			(mac_params->wowEnable & 0x02) ? TRUE : FALSE;

#ifdef FEATURE_WLAN_TDLS
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
	        WMI_TDLS_PEER_EVENTID,
	        wma_tdls_event_handler);
#endif /* FEATURE_WLAN_TDLS */

    /* register for install key completion event */
    wmi_unified_register_event_handler(wma_handle->wmi_handle,
                                      WMI_VDEV_INSTALL_KEY_COMPLETE_EVENTID,
                                      wma_vdev_install_key_complete_event_handler);
#ifdef WLAN_FEATURE_NAN
	/* register for nan response event */
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_NAN_EVENTID,
			wma_nan_rsp_event_handler);
#endif

#ifdef WLAN_FEATURE_STATS_EXT
        /* register for extended stats event */
        wmi_unified_register_event_handler(wma_handle->wmi_handle,
					   WMI_STATS_EXT_EVENTID,
					   wma_stats_ext_event_handler);
#endif
#ifdef FEATURE_WLAN_EXTSCAN
	wma_register_extscan_event_handler(wma_handle);
#endif

	WMA_LOGD("%s: Exit", __func__);

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	wmi_unified_register_event_handler(wma_handle->wmi_handle,
	         WMI_ROAM_SYNCH_EVENTID,
	        wma_roam_synch_event_handler);
#endif /* WLAN_FEATURE_ROAM_OFFLOAD */

	wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_RSSI_BREACH_EVENTID,
				wma_rssi_breached_event_handler);
	return VOS_STATUS_SUCCESS;

err_dbglog_init:
	adf_os_spinlock_destroy(&wma_handle->vdev_respq_lock);
	adf_os_spinlock_destroy(&wma_handle->vdev_detach_lock);
	adf_os_spinlock_destroy(&wma_handle->roam_preauth_lock);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	adf_os_spinlock_destroy(&wma_handle->roam_synch_lock);
#endif
err_event_init:
	wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
					     WMI_DEBUG_PRINT_EVENTID);
	vos_mem_free(wma_handle->interfaces);
err_scn_context:
	wma_dfs_detach(wma_handle->dfs_ic);
#if defined(QCA_WIFI_FTM)
	wma_utf_detach(wma_handle);
#endif
err_wmi_handle:
	adf_os_mem_free(((pVosContextType) vos_context)->cfg_ctx);
	OS_FREE(wmi_handle);

err_wma_handle:

	if (vos_get_conparam() != VOS_FTM_MODE) {
#ifdef FEATURE_WLAN_SCAN_PNO
		vos_wake_lock_destroy(&wma_handle->pno_wake_lock);
#endif
#ifdef FEATURE_WLAN_EXTSCAN
		vos_wake_lock_destroy(&wma_handle->extscan_wake_lock);
#endif
		vos_wake_lock_destroy(&wma_handle->wow_wake_lock);
	}
	vos_free_context(vos_context, VOS_MODULE_ID_WDA, wma_handle);

	WMA_LOGD("%s: Exit", __func__);

	return vos_status;
}

/* function   : wma_pre_start
 * Description :
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_pre_start(v_VOID_t *vos_ctx)
{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	A_STATUS status = A_OK;
	tp_wma_handle wma_handle;
	vos_msg_t wma_msg = {0} ;

	WMA_LOGD("%s: Enter", __func__);

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);

	/* Validate the wma_handle */
	if (NULL == wma_handle) {
		WMA_LOGP("%s: invalid argument", __func__);
		vos_status = VOS_STATUS_E_INVAL;
		goto end;
	}
	/* Open endpoint for ctrl path - WMI <--> HTC */
	status = wmi_unified_connect_htc_service(
			wma_handle->wmi_handle,
			wma_handle->htc_handle);
	if (A_OK != status) {
		WMA_LOGP("%s: wmi_unified_connect_htc_service", __func__);
		vos_status = VOS_STATUS_E_FAULT;
		goto end;
	}

	WMA_LOGA("WMA --> wmi_unified_connect_htc_service - success");

	/* Trigger the CFG DOWNLOAD */
	wma_msg.type = WNI_CFG_DNLD_REQ ;
	wma_msg.bodyptr = NULL;
	wma_msg.bodyval = 0;

	vos_status = vos_mq_post_message( VOS_MQ_ID_WDA, &wma_msg );
	if (VOS_STATUS_SUCCESS !=vos_status) {
		WMA_LOGP("%s: Failed to post WNI_CFG_DNLD_REQ msg", __func__);
		VOS_ASSERT(0);
		vos_status = VOS_STATUS_E_FAILURE;
	}
end:
	WMA_LOGD("%s: Exit", __func__);
	return vos_status;
}

/* function   : wma_send_msg
 * Description :
 * Args       :
 * Returns    :
 */
static void wma_send_msg(tp_wma_handle wma_handle, u_int16_t msg_type,
		void *body_ptr, u_int32_t body_val)
{
	tSirMsgQ msg = {0} ;
	tANI_U32 status = VOS_STATUS_SUCCESS ;
	tpAniSirGlobal pMac = (tpAniSirGlobal )vos_get_context(VOS_MODULE_ID_PE,
			wma_handle->vos_context);
	msg.type        = msg_type;
	msg.bodyval     = body_val;
	msg.bodyptr     = body_ptr;
	status = limPostMsgApi(pMac, &msg);
	if (VOS_STATUS_SUCCESS != status) {
		if(NULL != body_ptr)
			vos_mem_free(body_ptr);
		VOS_ASSERT(0) ;
	}
	return ;
}

/* function   : wma_get_txrx_vdev_type
 * Description :
 * Args       :
 * Returns    :
 */
enum wlan_op_mode wma_get_txrx_vdev_type(u_int32_t type)
{
	enum wlan_op_mode vdev_type = wlan_op_mode_unknown;
	switch (type) {
		case WMI_VDEV_TYPE_AP:
			vdev_type = wlan_op_mode_ap;
			break;
		case WMI_VDEV_TYPE_STA:
			vdev_type = wlan_op_mode_sta;
			break;
#ifdef QCA_IBSS_SUPPORT
		case WMI_VDEV_TYPE_IBSS:
                        vdev_type = wlan_op_mode_ibss;
                        break;
#endif
		case WMI_VDEV_TYPE_MONITOR:
		default:
			WMA_LOGE("Invalid vdev type %u", type);
			vdev_type = wlan_op_mode_unknown;
	}

	return vdev_type;
}

/* function   : wma_unified_vdev_create_send
 * Description :
 * Args       :
 * Returns    :
 */
int wma_unified_vdev_create_send(wmi_unified_t wmi_handle, u_int8_t if_id,
				 u_int32_t type, u_int32_t subtype,
				 u_int8_t macaddr[IEEE80211_ADDR_LEN])
{
	wmi_vdev_create_cmd_fixed_param* cmd;
	wmi_buf_t buf;
	int len = sizeof(*cmd);
	int ret;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s:wmi_buf_alloc failed", __FUNCTION__);
		return ENOMEM;
	}
	cmd = (wmi_vdev_create_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_create_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_vdev_create_cmd_fixed_param));
	cmd->vdev_id = if_id;
	cmd->vdev_type = type;
	cmd->vdev_subtype = subtype;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(macaddr, &cmd->vdev_macaddr);
	WMA_LOGE("%s: ID = %d VAP Addr = %02x:%02x:%02x:%02x:%02x:%02x",
		 __func__, if_id,
		 macaddr[0], macaddr[1], macaddr[2],
		 macaddr[3], macaddr[4], macaddr[5]);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len, WMI_VDEV_CREATE_CMDID);
	if (ret != EOK) {
		WMA_LOGE("Failed to send WMI_VDEV_CREATE_CMDID");
		wmi_buf_free(buf);
	}
	return ret;
}

/* function   : wma_unified_vdev_delete_send
 * Description :
 * Args       :
 * Returns    :
 */
static int wma_unified_vdev_delete_send(wmi_unified_t wmi_handle, u_int8_t if_id)
{
	wmi_vdev_delete_cmd_fixed_param* cmd;
	wmi_buf_t buf;
	int ret;

	buf = wmi_buf_alloc(wmi_handle, sizeof(*cmd));
	if (!buf) {
		WMA_LOGP("%s:wmi_buf_alloc failed", __FUNCTION__);
		return ENOMEM;
	}

	cmd = (wmi_vdev_delete_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_delete_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_vdev_delete_cmd_fixed_param));
	cmd->vdev_id = if_id;
	ret = wmi_unified_cmd_send(wmi_handle, buf, sizeof(wmi_vdev_delete_cmd_fixed_param),
			WMI_VDEV_DELETE_CMDID);
	if (ret != EOK) {
		WMA_LOGE("Failed to send WMI_VDEV_DELETE_CMDID");
		wmi_buf_free(buf);
	}
	return ret;
}

void wma_vdev_detach_callback(void *ctx)
{
	tp_wma_handle wma;
	struct wma_txrx_node *iface = (struct wma_txrx_node *)ctx;
	tpDelStaSelfParams param;
	struct wma_target_req *req_msg;

	wma = vos_get_context(VOS_MODULE_ID_WDA,
			      vos_get_global_context(VOS_MODULE_ID_WDA, NULL));

	if (!wma || !iface->del_staself_req) {
		WMA_LOGP("%s: wma %p iface %p", __func__, wma,
			 iface->del_staself_req);
		return;
	}
	param = (tpDelStaSelfParams) iface->del_staself_req;
	WMA_LOGD("%s: sending WDA_DEL_STA_SELF_RSP for vdev %d",
		 __func__, param->sessionId);

	req_msg = wma_find_vdev_req(wma, param->sessionId,
				    WMA_TARGET_REQ_TYPE_VDEV_DEL);
	if (req_msg) {
		WMA_LOGD("%s: Found vdev request for vdev id %d",
			 __func__, param->sessionId);
		vos_timer_stop(&req_msg->event_timeout);
		vos_timer_destroy(&req_msg->event_timeout);
		adf_os_mem_free(req_msg);
	}
	if(iface->addBssStaContext)
                adf_os_mem_free(iface->addBssStaContext);

#if defined WLAN_FEATURE_VOWIFI_11R
        if (iface->staKeyParams)
                adf_os_mem_free(iface->staKeyParams);
#endif
	vos_mem_zero(iface, sizeof(*iface));
	param->status = VOS_STATUS_SUCCESS;

	wma_send_msg(wma, WDA_DEL_STA_SELF_RSP, (void *)param, 0);
}

/* function   : wma_vdev_detach
 * Description :
 * Args       :
 * Returns    :
 */
static VOS_STATUS wma_vdev_detach(tp_wma_handle wma_handle,
				tpDelStaSelfParams pdel_sta_self_req_param,
                                u_int8_t generateRsp)
{
	VOS_STATUS status = VOS_STATUS_SUCCESS;
	ol_txrx_peer_handle peer;
	ol_txrx_pdev_handle pdev;
	u_int8_t peer_id;
	u_int8_t vdev_id = pdel_sta_self_req_param->sessionId;
	struct wma_txrx_node *iface = &wma_handle->interfaces[vdev_id];
	struct wma_target_req *msg;

	if ((iface->type == WMI_VDEV_TYPE_AP) &&
	    (iface->sub_type == WMI_UNIFIED_VDEV_SUBTYPE_P2P_DEVICE)) {

		WMA_LOGA("P2P Device: removing self peer %pM",
				pdel_sta_self_req_param->selfMacAddr);

		pdev = vos_get_context(VOS_MODULE_ID_TXRX,
				wma_handle->vos_context);

		if (NULL == pdev) {
			WMA_LOGE("%s: Failed to get pdev",__func__);
			return VOS_STATUS_E_FAULT;
		}

		peer = ol_txrx_find_peer_by_addr(pdev,
				pdel_sta_self_req_param->selfMacAddr,
				&peer_id);
		if (!peer) {
			WMA_LOGE("%s Failed to find peer %pM", __func__,
					pdel_sta_self_req_param->selfMacAddr);
		}
		wma_remove_peer(wma_handle,
				pdel_sta_self_req_param->selfMacAddr,
				vdev_id, peer, VOS_FALSE);
	}
	if (adf_os_atomic_read(&iface->bss_status) == WMA_BSS_STATUS_STARTED) {
		WMA_LOGA("BSS is not yet stopped. Defering vdev(vdev id %x) deletion",
				vdev_id);
		iface->del_staself_req = pdel_sta_self_req_param;
		return status;
	}

        adf_os_spin_lock_bh(&wma_handle->vdev_detach_lock);
        if(!iface->handle) {
                WMA_LOGE("handle of vdev_id %d is NULL vdev is already freed",
                    vdev_id);
                adf_os_spin_unlock_bh(&wma_handle->vdev_detach_lock);
		vos_mem_free(pdel_sta_self_req_param);
		pdel_sta_self_req_param = NULL;
		return status;
        }

        /* Unregister vdev from TL shim before vdev delete
         * Will protect from invalid vdev access */
        WLANTL_UnRegisterVdev(wma_handle->vos_context, vdev_id);

        /* remove the interface from ath_dev */
        if (wma_unified_vdev_delete_send(wma_handle->wmi_handle, vdev_id)) {
                WMA_LOGE("Unable to remove an interface for ath_dev.");
                status = VOS_STATUS_E_FAILURE;
                adf_os_spin_unlock_bh(&wma_handle->vdev_detach_lock);
                goto out;
        }


        WMA_LOGA("vdev_id:%hu vdev_hdl:%p", vdev_id, iface->handle);
        if (!generateRsp) {
                WMA_LOGE("Call txrx detach w/o callback for vdev %d", vdev_id);
                ol_txrx_vdev_detach(iface->handle, NULL, NULL);
                adf_os_spin_unlock_bh(&wma_handle->vdev_detach_lock);
                goto out;
        }

        iface->del_staself_req = pdel_sta_self_req_param;
        msg = wma_fill_vdev_req(wma_handle, vdev_id, WDA_DEL_STA_SELF_REQ,
                                WMA_TARGET_REQ_TYPE_VDEV_DEL, iface, 2000);
        if (!msg) {
                WMA_LOGE("%s: Failed to fill vdev request for vdev_id %d",
                         __func__, vdev_id);
                status = VOS_STATUS_E_NOMEM;
                adf_os_spin_unlock_bh(&wma_handle->vdev_detach_lock);
                goto out;
        }
        WMA_LOGE("Call txrx detach with callback for vdev %d", vdev_id);
        ol_txrx_vdev_detach(iface->handle, NULL, NULL);
        wma_vdev_detach_callback(iface);
        adf_os_spin_unlock_bh(&wma_handle->vdev_detach_lock);
        return status;
out:
        if(iface->addBssStaContext)
                adf_os_mem_free(iface->addBssStaContext);
#if defined WLAN_FEATURE_VOWIFI_11R
        if (iface->staKeyParams)
                adf_os_mem_free(iface->staKeyParams);
#endif
	vos_mem_zero(iface, sizeof(*iface));
	pdel_sta_self_req_param->status = status;
	if (generateRsp)
		wma_send_msg(wma_handle, WDA_DEL_STA_SELF_RSP, (void *)pdel_sta_self_req_param, 0);

	return status;
}

static int wmi_unified_peer_create_send(wmi_unified_t wmi,
		const u_int8_t *peer_addr, u_int32_t peer_type,
		u_int32_t vdev_id)
{
	wmi_peer_create_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}
	cmd = (wmi_peer_create_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_create_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_peer_create_cmd_fixed_param));
	WMI_CHAR_ARRAY_TO_MAC_ADDR(peer_addr, &cmd->peer_macaddr);
	cmd->peer_type = peer_type;
	cmd->vdev_id = vdev_id;

	if (wmi_unified_cmd_send(wmi, buf, len, WMI_PEER_CREATE_CMDID)) {
		WMA_LOGP("%s: failed to send WMI_PEER_CREATE_CMDID", __func__);
		adf_nbuf_free(buf);
		return -EIO;
	}
	WMA_LOGD("%s: peer_addr %pM vdev_id %d", __func__, peer_addr, vdev_id);
	return 0;
}

static VOS_STATUS wma_create_peer(tp_wma_handle wma, ol_txrx_pdev_handle pdev,
		ol_txrx_vdev_handle vdev, u8 peer_addr[6],
		u_int32_t peer_type, u_int8_t vdev_id,
		v_BOOL_t roam_synch_in_progress)
{
	ol_txrx_peer_handle peer;

	if (++wma->interfaces[vdev_id].peer_count > wma->wlan_resource_config.num_peers) {
		WMA_LOGP("%s, the peer count exceeds the limit %d",
			 __func__, wma->interfaces[vdev_id].peer_count - 1);
		goto err;
	}
	peer = ol_txrx_peer_attach(pdev, vdev, peer_addr);
	if (!peer) {
		WMA_LOGE("%s : Unable to attach peer %pM", __func__, peer_addr);
		goto err;
	}
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (roam_synch_in_progress) {

		WMA_LOGD("%s: Created peer with peer_addr %pM vdev_id %d,"
				"peer_count - %d",__func__, peer_addr, vdev_id,
				wma->interfaces[vdev_id].peer_count);
		return VOS_STATUS_SUCCESS;
	}
#endif
	if (wmi_unified_peer_create_send(wma->wmi_handle, peer_addr,
					 peer_type, vdev_id) < 0) {
		WMA_LOGP("%s : Unable to create peer in Target", __func__);
		ol_txrx_peer_detach(peer);
		goto err;
	}
	WMA_LOGE("%s: Created peer with peer_addr %pM vdev_id %d, peer_count - %d",
                    __func__, peer_addr, vdev_id, wma->interfaces[vdev_id].peer_count);

#ifdef QCA_IBSS_SUPPORT
	/* for each remote ibss peer, clear its keys */
	if (wma_is_vdev_in_ibss_mode(wma, vdev_id)
		&& !vos_mem_compare(peer_addr, vdev->mac_addr.raw, ETH_ALEN)) {

		tSetStaKeyParams key_info;
		WMA_LOGD("%s: remote ibss peer %pM key clearing\n", __func__,
			peer_addr);
		vos_mem_set(&key_info, sizeof(key_info), 0);
		key_info.smesessionId= vdev_id;
		vos_mem_copy(key_info.peerMacAddr, peer_addr, ETH_ALEN);
        key_info.sendRsp = FALSE;

		wma_set_stakey(wma, &key_info);
	}
#endif

	return VOS_STATUS_SUCCESS;
err:
	wma->interfaces[vdev_id].peer_count--;
	return VOS_STATUS_E_FAILURE;
}

static void wma_set_sta_keep_alive(tp_wma_handle wma, u_int8_t vdev_id,
				   v_U32_t method, v_U32_t timeperiod,
				   u_int8_t *hostv4addr, u_int8_t *destv4addr,
				   u_int8_t *destmac)
{
	wmi_buf_t buf;
	WMI_STA_KEEPALIVE_CMD_fixed_param *cmd;
	WMI_STA_KEEPALVE_ARP_RESPONSE *arp_rsp;
	u_int8_t *buf_ptr;
	int len;

	WMA_LOGD("%s: Enter", __func__);
	len = sizeof(*cmd) + sizeof(*arp_rsp);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		 WMA_LOGE("wmi_buf_alloc failed");
		 return;
	}

	cmd = (WMI_STA_KEEPALIVE_CMD_fixed_param *) wmi_buf_data(buf);
	buf_ptr = (u_int8_t *)cmd;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_WMI_STA_KEEPALIVE_CMD_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       WMI_STA_KEEPALIVE_CMD_fixed_param));
	cmd->interval = timeperiod;
	cmd->enable = (timeperiod)? 1:0;
	cmd->vdev_id = vdev_id;
	WMA_LOGD("Keep Alive: vdev_id:%d interval:%u method:%d", vdev_id,
		 timeperiod, method);
	arp_rsp = (WMI_STA_KEEPALVE_ARP_RESPONSE *)(buf_ptr + sizeof(*cmd));
	WMITLV_SET_HDR(&arp_rsp->tlv_header,
		       WMITLV_TAG_STRUC_WMI_STA_KEEPALVE_ARP_RESPONSE,
		       WMITLV_GET_STRUCT_TLVLEN(WMI_STA_KEEPALVE_ARP_RESPONSE));

	if (method == SIR_KEEP_ALIVE_UNSOLICIT_ARP_RSP) {
		cmd->method = WMI_STA_KEEPALIVE_METHOD_UNSOLICITED_ARP_RESPONSE;
		vos_mem_copy(&arp_rsp->sender_prot_addr, hostv4addr,
				SIR_IPV4_ADDR_LEN);
		vos_mem_copy(&arp_rsp->target_prot_addr, destv4addr,
				SIR_IPV4_ADDR_LEN);
		WMI_CHAR_ARRAY_TO_MAC_ADDR(destmac,&arp_rsp->dest_mac_addr);
	} else {
		cmd->method = WMI_STA_KEEPALIVE_METHOD_NULL_FRAME;
	}

	if (wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				 WMI_STA_KEEPALIVE_CMDID)) {
		WMA_LOGE("Failed to set KeepAlive");
		adf_nbuf_free(buf);
	}

	WMA_LOGD("%s: Exit", __func__);
	return;
}

static inline void wma_get_link_probe_timeout(struct sAniSirGlobal *mac,
					      tANI_U32 sub_type,
					      tANI_U32 *max_inactive_time,
					      tANI_U32 *max_unresponsive_time)
{
	tANI_U32 keep_alive;
	tANI_U16 lm_id, ka_id;

	switch (sub_type) {
	case WMI_UNIFIED_VDEV_SUBTYPE_P2P_GO:
		lm_id = WNI_CFG_GO_LINK_MONITOR_TIMEOUT;
		ka_id = WNI_CFG_GO_KEEP_ALIVE_TIMEOUT;
		break;
	default:
		/*For softAp the subtype value will be zero*/
		lm_id = WNI_CFG_AP_LINK_MONITOR_TIMEOUT;
		ka_id = WNI_CFG_AP_KEEP_ALIVE_TIMEOUT;
	}

	if(wlan_cfgGetInt(mac, lm_id, max_inactive_time) != eSIR_SUCCESS) {
		WMA_LOGE("Failed to read link monitor for subtype %d", sub_type);
		*max_inactive_time = WMA_LINK_MONITOR_DEFAULT_TIME_SECS;
	}

	if(wlan_cfgGetInt(mac, ka_id, &keep_alive) != eSIR_SUCCESS) {
		WMA_LOGE("Failed to read keep alive for subtype %d", sub_type);
		keep_alive = WMA_KEEP_ALIVE_DEFAULT_TIME_SECS;
	}
	*max_unresponsive_time = *max_inactive_time + keep_alive;
}

static void wma_set_sap_keepalive(tp_wma_handle wma, u_int8_t vdev_id)
{
	tANI_U32 min_inactive_time, max_inactive_time, max_unresponsive_time;
	struct sAniSirGlobal *mac =
		(struct sAniSirGlobal*)vos_get_context(VOS_MODULE_ID_PE,
						      wma->vos_context);

	if (NULL == mac) {
		WMA_LOGE("%s: Failed to get mac", __func__);
		return;
	}

	wma_get_link_probe_timeout(mac, wma->interfaces[vdev_id].sub_type,
				   &max_inactive_time, &max_unresponsive_time);

	min_inactive_time = max_inactive_time / 2;

	if (wmi_unified_vdev_set_param_send(wma->wmi_handle,
					    vdev_id,
	       WMI_VDEV_PARAM_AP_KEEPALIVE_MIN_IDLE_INACTIVE_TIME_SECS,
					    min_inactive_time))
		WMA_LOGE("Failed to Set AP MIN IDLE INACTIVE TIME");

	if (wmi_unified_vdev_set_param_send(wma->wmi_handle,
					    vdev_id,
	       WMI_VDEV_PARAM_AP_KEEPALIVE_MAX_IDLE_INACTIVE_TIME_SECS,
					    max_inactive_time))
		WMA_LOGE("Failed to Set AP MAX IDLE INACTIVE TIME");

	if (wmi_unified_vdev_set_param_send(wma->wmi_handle,
					    vdev_id,
		WMI_VDEV_PARAM_AP_KEEPALIVE_MAX_UNRESPONSIVE_TIME_SECS,
					    max_unresponsive_time))
		WMA_LOGE("Failed to Set MAX UNRESPONSIVE TIME");

	WMA_LOGD("%s:vdev_id:%d min_inactive_time: %u max_inactive_time: %u"
		 " max_unresponsive_time: %u", __func__, vdev_id,
		 min_inactive_time, max_inactive_time, max_unresponsive_time);
}

static VOS_STATUS wma_set_enable_disable_mcc_adaptive_scheduler(tANI_U32 mcc_adaptive_scheduler)
{
	int ret = -1;
	wmi_buf_t buf = 0;
	wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param *cmd = NULL;
	tp_wma_handle wma = NULL;
	void *vos_context = NULL;
	u_int16_t len = sizeof(wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param);

	vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
	wma = vos_get_context(VOS_MODULE_ID_WDA, vos_context);

	if (NULL == wma) {
		WMA_LOGE("%s : Failed to get wma", __func__);
		return VOS_STATUS_E_FAULT;
	}

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s : wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	cmd = (wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param *) wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_resmgr_adaptive_ocs_enable_disable_cmd_fixed_param));
	cmd->enable = mcc_adaptive_scheduler;

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_RESMGR_ADAPTIVE_OCS_ENABLE_DISABLE_CMDID);
	if (ret) {
		WMA_LOGP("%s: Failed to send enable/disable MCC"
			" adaptive scheduler command", __func__);
		adf_nbuf_free(buf);
	}
	return VOS_STATUS_SUCCESS;
}

/**
  * Currently used to set time latency for an MCC vdev/adapter using operating
  * channel of it and channel number. The info is provided run time using
  * iwpriv command: iwpriv <wlan0 | p2p0> setMccLatency <latency in ms>.
  */
static VOS_STATUS wma_set_mcc_channel_time_latency
					(
					tp_wma_handle wma,
					tANI_U32 mcc_channel,
					tANI_U32 mcc_channel_time_latency
					)
{
	int ret = -1;
	wmi_buf_t buf = 0;
	wmi_resmgr_set_chan_latency_cmd_fixed_param *cmdTL = NULL;
	u_int16_t len = 0;
	u_int8_t *buf_ptr = NULL;
	tANI_U32 cfg_val = 0;
	wmi_resmgr_chan_latency chan_latency;
	struct sAniSirGlobal *pMac = NULL;
	/* Note: we only support MCC time latency for a single channel */
	u_int32_t num_channels = 1;
	u_int32_t channel1 = mcc_channel;
	u_int32_t chan1_freq = vos_chan_to_freq( channel1 );
	u_int32_t latency_chan1 = mcc_channel_time_latency;

	if (!wma) {
		WMA_LOGE("%s:NULL wma ptr. Exiting", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}
	pMac =
		(struct sAniSirGlobal*)vos_get_context(VOS_MODULE_ID_PE,
						      wma->vos_context);
	if (!pMac) {
		WMA_LOGE("%s:NULL pMac ptr. Exiting", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}

	/* First step is to confirm if MCC is active */
	if (!limIsInMCC(pMac)) {
		WMA_LOGE("%s: MCC is not active. Exiting", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}
	/* Confirm MCC adaptive scheduler feature is disabled */
	if (wlan_cfgGetInt(pMac, WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED,
	        &cfg_val) == eSIR_SUCCESS) {
		if (cfg_val == WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED_STAMAX) {
			WMA_LOGD("%s: Can't set channel latency while MCC "
				"ADAPTIVE SCHED is enabled. Exit", __func__);
			return VOS_STATUS_SUCCESS;
		}
	} else {
		WMA_LOGE("%s: Failed to get value for MCC_ADAPTIVE_SCHED, "
				"Exit w/o setting latency", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}
	/* If 0ms latency is provided, then FW will set to a default.
	 * Otherwise, latency must be at least 30ms.
	 */
	if ((latency_chan1 > 0) &&
		(latency_chan1 < WMI_MCC_MIN_NON_ZERO_CHANNEL_LATENCY)) {
		WMA_LOGE("%s: Invalid time latency for Channel #1 = %dms "
			"Minimum is 30ms (or 0 to use default value by "
			"firmware)", __func__, latency_chan1);
		return VOS_STATUS_E_INVAL;
	}

	/*   Set WMI CMD for channel time latency here */
	len = sizeof(wmi_resmgr_set_chan_latency_cmd_fixed_param) +
		WMI_TLV_HDR_SIZE + /*Place holder for chan_time_latency array*/
		num_channels * sizeof(wmi_resmgr_chan_latency);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmdTL = (wmi_resmgr_set_chan_latency_cmd_fixed_param *)
						wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmdTL->tlv_header,
		WMITLV_TAG_STRUC_wmi_resmgr_set_chan_latency_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_resmgr_set_chan_latency_cmd_fixed_param));
	cmdTL->num_chans = num_channels;
	/* Update channel time latency information for home channel(s) */
	buf_ptr += sizeof(*cmdTL);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		       num_channels * sizeof(wmi_resmgr_chan_latency));
	buf_ptr += WMI_TLV_HDR_SIZE;
	chan_latency.chan_mhz = chan1_freq;
	chan_latency.latency = latency_chan1;
	vos_mem_copy(buf_ptr, &chan_latency, sizeof(chan_latency));
	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_RESMGR_SET_CHAN_LATENCY_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send MCC Channel Time Latency command",
			__func__);
		adf_nbuf_free(buf);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

/**
  * Currently used to set time quota for 2 MCC vdevs/adapters using (operating
  * channel, quota) for each mode . The info is provided run time using
  * iwpriv command: iwpriv <wlan0 | p2p0> setMccQuota <quota in ms>.
  * Note: the quota provided in command is for the same mode in cmd. HDD
  * checks if MCC mode is active, gets the second mode and its operating chan.
  * Quota for the 2nd role is calculated as 100 - quota of first mode.
  */
static VOS_STATUS wma_set_mcc_channel_time_quota
					(
					tp_wma_handle wma,
					tANI_U32 adapter_1_chan_number,
					tANI_U32 adapter_1_quota,
					tANI_U32 adapter_2_chan_number
					)
{
	int ret = -1;
	wmi_buf_t buf = 0;
	u_int16_t len = 0;
	u_int8_t *buf_ptr = NULL;
	tANI_U32 cfg_val = 0;
	struct sAniSirGlobal *pMac = NULL;
	wmi_resmgr_set_chan_time_quota_cmd_fixed_param *cmdTQ = NULL;
	wmi_resmgr_chan_time_quota chan_quota;
	u_int32_t channel1 = adapter_1_chan_number;
	u_int32_t channel2 = adapter_2_chan_number;
	u_int32_t quota_chan1 = adapter_1_quota;
	/* Knowing quota of 1st chan., derive quota for 2nd chan. */
	u_int32_t quota_chan2 = 100 - quota_chan1;
	/* Note: setting time quota for MCC requires info for 2 channels */
	u_int32_t num_channels = 2;
	u_int32_t chan1_freq = vos_chan_to_freq(adapter_1_chan_number);
	u_int32_t chan2_freq = vos_chan_to_freq(adapter_2_chan_number);

	WMA_LOGD("%s: Channel1:%d, freq1:%dMHz, Quota1:%dms, "
		"Channel2:%d, freq2:%dMHz, Quota2:%dms", __func__,
		channel1, chan1_freq, quota_chan1, channel2, chan2_freq,
		quota_chan2);

	if (!wma) {
		WMA_LOGE("%s:NULL wma ptr. Exiting", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}
	pMac =
		(struct sAniSirGlobal*)vos_get_context(VOS_MODULE_ID_PE,
							  wma->vos_context);
	if (!pMac) {
		WMA_LOGE("%s:NULL pMac ptr. Exiting", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}

	/* First step is to confirm if MCC is active */
	if (!limIsInMCC(pMac)) {
		WMA_LOGD("%s: MCC is not active. Exiting", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}

	/* Confirm MCC adaptive scheduler feature is disabled */
	if (wlan_cfgGetInt(pMac, WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED,
			&cfg_val) == eSIR_SUCCESS) {
		if (cfg_val == WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED_STAMAX) {
			WMA_LOGD("%s: Can't set channel quota while "
					"MCC_ADAPTIVE_SCHED is enabled. Exit",
					__func__);
			return VOS_STATUS_SUCCESS;
		}
	} else {
		WMA_LOGE("%s: Failed to retrieve "
			"WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED. Exit",
			__func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}

	/**
	 * Perform sanity check on time quota values provided.
	 */
	if (quota_chan1 < WMI_MCC_MIN_CHANNEL_QUOTA ||
		quota_chan1 > WMI_MCC_MAX_CHANNEL_QUOTA) {
		WMA_LOGE("%s: Invalid time quota for Channel #1=%dms. Minimum "
			"is 20ms & maximum is 80ms", __func__, quota_chan1);
		return VOS_STATUS_E_INVAL;
	}
	/* Set WMI CMD for channel time quota here */
	len = sizeof(wmi_resmgr_set_chan_time_quota_cmd_fixed_param) +
	WMI_TLV_HDR_SIZE + /* Place holder for chan_time_quota array */
		num_channels * sizeof(wmi_resmgr_chan_time_quota);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_NOMEM;
	}
	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmdTQ = (wmi_resmgr_set_chan_time_quota_cmd_fixed_param *)
							wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmdTQ->tlv_header,
		WMITLV_TAG_STRUC_wmi_resmgr_set_chan_time_quota_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_resmgr_set_chan_time_quota_cmd_fixed_param));
	cmdTQ->num_chans = num_channels;

	/* Update channel time quota information for home channel(s) */
	buf_ptr += sizeof(*cmdTQ);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		   num_channels * sizeof(wmi_resmgr_chan_time_quota));
	buf_ptr += WMI_TLV_HDR_SIZE;
	chan_quota.chan_mhz = chan1_freq;
	chan_quota.channel_time_quota = quota_chan1;
	vos_mem_copy(buf_ptr, &chan_quota, sizeof(chan_quota));
	/* Construct channel and quota record for the 2nd MCC mode. */
	buf_ptr += sizeof(chan_quota);
	chan_quota.chan_mhz = chan2_freq;
	chan_quota.channel_time_quota = quota_chan2;
	vos_mem_copy(buf_ptr, &chan_quota, sizeof(chan_quota));

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_RESMGR_SET_CHAN_TIME_QUOTA_CMDID);
	if (ret) {
		WMA_LOGE("Failed to send MCC Channel Time Quota command");
		adf_nbuf_free(buf);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

/* function   : wma_vdev_attach
 * Description :
 * Args       :
 * Returns    :
 */
static ol_txrx_vdev_handle wma_vdev_attach(tp_wma_handle wma_handle,
					   tpAddStaSelfParams self_sta_req,
                                           u_int8_t generateRsp)
{
	ol_txrx_vdev_handle txrx_vdev_handle = NULL;
	ol_txrx_pdev_handle txrx_pdev = vos_get_context(VOS_MODULE_ID_TXRX,
			wma_handle->vos_context);
	enum wlan_op_mode txrx_vdev_type;
	VOS_STATUS status = VOS_STATUS_SUCCESS;
	struct sAniSirGlobal *mac =
		(struct sAniSirGlobal*)vos_get_context(VOS_MODULE_ID_PE,
						      wma_handle->vos_context);
	tANI_U32 cfg_val;
        tANI_U16 val16;
	int ret;
	tSirMacHTCapabilityInfo *phtCapInfo;

	if (NULL == mac) {
		WMA_LOGE("%s: Failed to get mac",__func__);
		goto end;
	}

	/* Create a vdev in target */
	if (wma_unified_vdev_create_send(wma_handle->wmi_handle,
						self_sta_req->sessionId,
						self_sta_req->type,
						self_sta_req->subType,
						self_sta_req->selfMacAddr))
	{
		WMA_LOGP("%s: Unable to add an interface for ath_dev", __func__);
		status = VOS_STATUS_E_RESOURCES;
		goto end;
	}

	txrx_vdev_type = wma_get_txrx_vdev_type(self_sta_req->type);

	if (wlan_op_mode_unknown == txrx_vdev_type) {
		WMA_LOGE("Failed to get txrx vdev type");
		wma_unified_vdev_delete_send(wma_handle->wmi_handle,
						self_sta_req->sessionId);
		goto end;
	}

	txrx_vdev_handle = ol_txrx_vdev_attach(txrx_pdev,
						self_sta_req->selfMacAddr,
						self_sta_req->sessionId,
						txrx_vdev_type);
	wma_handle->interfaces[self_sta_req->sessionId].pause_bitmap = 0;

	WMA_LOGA("vdev_id %hu, txrx_vdev_handle = %p", self_sta_req->sessionId,
			txrx_vdev_handle);

	if (NULL == txrx_vdev_handle) {
		WMA_LOGP("%s: ol_txrx_vdev_attach failed", __func__);
		status = VOS_STATUS_E_FAILURE;
		wma_unified_vdev_delete_send(wma_handle->wmi_handle,
						self_sta_req->sessionId);
		goto end;
	}
	wma_handle->interfaces[self_sta_req->sessionId].handle = txrx_vdev_handle;

	wma_handle->interfaces[self_sta_req->sessionId].ptrn_match_enable =
		wma_handle->ptrn_match_enable_all_vdev ? TRUE : FALSE;

	if (wlan_cfgGetInt(mac, WNI_CFG_WOWLAN_DEAUTH_ENABLE, &cfg_val)
			!= eSIR_SUCCESS)
		wma_handle->wow.deauth_enable = TRUE;
	else
		wma_handle->wow.deauth_enable = cfg_val ? TRUE : FALSE;

	if (wlan_cfgGetInt(mac, WNI_CFG_WOWLAN_DISASSOC_ENABLE, &cfg_val)
			!= eSIR_SUCCESS)
		wma_handle->wow.disassoc_enable = TRUE;
	else
		wma_handle->wow.disassoc_enable = cfg_val ? TRUE : FALSE;

	if (wlan_cfgGetInt(mac, WNI_CFG_WOWLAN_MAX_MISSED_BEACON, &cfg_val)
			!= eSIR_SUCCESS)
		wma_handle->wow.bmiss_enable = TRUE;
	else
		wma_handle->wow.bmiss_enable = cfg_val ? TRUE : FALSE;

	vos_mem_copy(wma_handle->interfaces[self_sta_req->sessionId].addr,
		     self_sta_req->selfMacAddr,
		     sizeof(wma_handle->interfaces[self_sta_req->sessionId].addr));
	switch (self_sta_req->type) {
	case WMI_VDEV_TYPE_STA:
		if(wlan_cfgGetInt(mac, WNI_CFG_INFRA_STA_KEEP_ALIVE_PERIOD,
				  &cfg_val ) != eSIR_SUCCESS) {
			WMA_LOGE("Failed to get value for "
				 "WNI_CFG_INFRA_STA_KEEP_ALIVE_PERIOD");
			cfg_val = DEFAULT_INFRA_STA_KEEP_ALIVE_PERIOD;
		}

		wma_set_sta_keep_alive(wma_handle,
				       self_sta_req->sessionId,
				       SIR_KEEP_ALIVE_NULL_PKT,
				       cfg_val,
				       NULL,
				       NULL,
				       NULL);
		break;
	}

	wma_handle->interfaces[self_sta_req->sessionId].type =
		self_sta_req->type;
	wma_handle->interfaces[self_sta_req->sessionId].sub_type =
		self_sta_req->subType;
	adf_os_atomic_init(&wma_handle->interfaces
			   [self_sta_req->sessionId].bss_status);

	if ((self_sta_req->type == WMI_VDEV_TYPE_AP) &&
			(self_sta_req->subType ==
			 WMI_UNIFIED_VDEV_SUBTYPE_P2P_DEVICE)) {
		WMA_LOGA("P2P Device: creating self peer %pM, vdev_id %hu",
				self_sta_req->selfMacAddr,
				self_sta_req->sessionId);
		status = wma_create_peer(wma_handle, txrx_pdev,
		  txrx_vdev_handle, self_sta_req->selfMacAddr,
		  WMI_PEER_TYPE_DEFAULT,self_sta_req->sessionId,
		  VOS_FALSE);
		if (status != VOS_STATUS_SUCCESS) {
			WMA_LOGE("%s: Failed to create peer", __func__);
			status = VOS_STATUS_E_FAILURE;
			wma_unified_vdev_delete_send(wma_handle->wmi_handle,
					self_sta_req->sessionId);
		}
	}
	ret = wmi_unified_vdev_set_param_send(wma_handle->wmi_handle,
					      self_sta_req->sessionId,
					      WMI_VDEV_PARAM_DISCONNECT_TH,
					      self_sta_req->pkt_err_disconn_th);
	if (ret)
		WMA_LOGE("Failed to set WMI_VDEV_PARAM_DISCONNECT_TH");

	if (wlan_cfgGetInt(mac, WNI_CFG_RTS_THRESHOLD,
			&cfg_val) == eSIR_SUCCESS) {
		ret = wmi_unified_vdev_set_param_send(wma_handle->wmi_handle,
						      self_sta_req->sessionId,
						      WMI_VDEV_PARAM_RTS_THRESHOLD,
						      cfg_val);
		if (ret)
			WMA_LOGE("Failed to set WMI_VDEV_PARAM_RTS_THRESHOLD");
	} else {
		WMA_LOGE("Failed to get value for WNI_CFG_RTS_THRESHOLD, leaving unchanged");
	}

	if (wlan_cfgGetInt(mac, WNI_CFG_FRAGMENTATION_THRESHOLD,
                        &cfg_val) == eSIR_SUCCESS) {
		ret = wmi_unified_vdev_set_param_send(wma_handle->wmi_handle,
						      self_sta_req->sessionId,
						      WMI_VDEV_PARAM_FRAGMENTATION_THRESHOLD,
						      cfg_val);
		if (ret)
			WMA_LOGE("Failed to set WMI_VDEV_PARAM_FRAGMENTATION_THRESHOLD");
	} else {
		WMA_LOGE("Failed to get value for WNI_CFG_FRAGMENTATION_THRESHOLD, leaving unchanged");
	}

	if (wlan_cfgGetInt(mac, WNI_CFG_HT_CAP_INFO,
							&cfg_val) == eSIR_SUCCESS) {
		val16 = (tANI_U16)cfg_val;
		phtCapInfo = (tSirMacHTCapabilityInfo *)&cfg_val;
		ret = wmi_unified_vdev_set_param_send(wma_handle->wmi_handle,
								self_sta_req->sessionId,
								WMI_VDEV_PARAM_TX_STBC,
								phtCapInfo->txSTBC);
		if (ret)
			WMA_LOGE("Failed to set WMI_VDEV_PARAM_TX_STBC");
	} else {
		WMA_LOGE("Failed to get value of HT_CAP, TX STBC unchanged");
	}
	/* Initialize roaming offload state */
	if ((self_sta_req->type == WMI_VDEV_TYPE_STA) &&
			(self_sta_req->subType == 0)) {
		wma_handle->roam_offload_enabled = TRUE;
		wmi_unified_vdev_set_param_send(wma_handle->wmi_handle,
				self_sta_req->sessionId,
				WMI_VDEV_PARAM_ROAM_FW_OFFLOAD,
				(WMI_ROAM_FW_OFFLOAD_ENABLE_FLAG |
				WMI_ROAM_BMISS_FINAL_SCAN_ENABLE_FLAG));
	}

	/* Initialize BMISS parameters */
	if ((self_sta_req->type == WMI_VDEV_TYPE_STA) &&
		(self_sta_req->subType == 0)) {
		wma_roam_scan_bmiss_cnt(wma_handle,
		mac->roam.configParam.neighborRoamConfig.nRoamBmissFirstBcnt,
		mac->roam.configParam.neighborRoamConfig.nRoamBmissFinalBcnt,
		self_sta_req->sessionId);
	}

	if (wlan_cfgGetInt(mac, WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED,
	        &cfg_val) == eSIR_SUCCESS) {
	        WMA_LOGD("%s: setting ini value for WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED: %d",
			 __func__, cfg_val);
		ret = wma_set_enable_disable_mcc_adaptive_scheduler(cfg_val);
		if (ret != VOS_STATUS_SUCCESS) {
			WMA_LOGE("Failed to set WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED");
		}
	} else {
		WMA_LOGE("Failed to get value for WNI_CFG_ENABLE_MCC_ADAPTIVE_SCHED, leaving unchanged");
	}

end:
	self_sta_req->status = status;

#ifdef QCA_IBSS_SUPPORT
	if (generateRsp)
#endif
		wma_send_msg(wma_handle, WDA_ADD_STA_SELF_RSP, (void *)self_sta_req, 0);

	return txrx_vdev_handle;
}

static VOS_STATUS wma_wni_cfg_dnld(tp_wma_handle wma_handle)
{
	VOS_STATUS vos_status = VOS_STATUS_E_FAILURE;
	v_VOID_t *file_img = NULL;
	v_SIZE_t file_img_sz = 0;
	v_VOID_t *cfg_bin = NULL;
	v_SIZE_t cfg_bin_sz = 0;
	v_BOOL_t status = VOS_FALSE;
	v_VOID_t *mac = vos_get_context(VOS_MODULE_ID_PE,
			wma_handle->vos_context);

	WMA_LOGD("%s: Enter", __func__);

	if (NULL == mac) {
		WMA_LOGP("%s: Invalid context", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}

	/* get the number of bytes in the CFG Binary... */
	vos_status = vos_get_binary_blob(VOS_BINARY_ID_CONFIG, NULL,
			&file_img_sz);
	if (VOS_STATUS_E_NOMEM != vos_status) {
		WMA_LOGP("%s: Error in obtaining the binary size", __func__);
		goto fail;
	}

	/* malloc a buffer to read in the Configuration binary file. */
	file_img = vos_mem_malloc(file_img_sz);
	if (NULL == file_img) {
		WMA_LOGP("%s: Unable to allocate memory for the CFG binary "
				"[size= %d bytes]", __func__, file_img_sz);
		vos_status = VOS_STATUS_E_NOMEM;
		goto fail;
	}

	/* Get the entire CFG file image. */
	vos_status = vos_get_binary_blob(VOS_BINARY_ID_CONFIG, file_img,
			&file_img_sz);
	if (VOS_STATUS_SUCCESS != vos_status) {
		WMA_LOGP("%s: Cannot retrieve CFG file image from vOSS "
				"[size= %d bytes]", __func__, file_img_sz);
		goto fail;
	}

	/*
	 * Validate the binary image.  This function will return a pointer
	 * and length where the CFG binary is located within the binary image file.
	 */
	status = sys_validateStaConfig( file_img, file_img_sz,
			&cfg_bin, &cfg_bin_sz );
	if ( VOS_FALSE == status )
	{
		WMA_LOGP("%s: Cannot find STA CFG in binary image file",
				__func__);
		vos_status = VOS_STATUS_E_FAILURE;
		goto fail;
	}
	/*
	 * TODO: call the config download function
	 * for now calling the existing cfg download API
	 */
	processCfgDownloadReq(mac, cfg_bin_sz, cfg_bin);
	if (file_img != NULL) {
		vos_mem_free(file_img);
	}

	WMA_LOGD("%s: Exit", __func__);
	return vos_status;

fail:
	if(cfg_bin != NULL)
		vos_mem_free( file_img );

	WMA_LOGD("%s: Exit", __func__);
	return vos_status;
}

/* function   : wma_set_scan_info
 * Description : function to save current ongoing scan info
 * Args       : wma handle, scan id, scan requestor id, vdev id
 * Returns    : None
 */
static inline void wma_set_scan_info(tp_wma_handle wma_handle,
					u_int32_t scan_id,
					u_int32_t requestor,
					u_int32_t vdev_id,
					tSirP2pScanType p2p_scan_type)
{
	wma_handle->interfaces[vdev_id].scan_info.scan_id = scan_id;
	wma_handle->interfaces[vdev_id].scan_info.scan_requestor_id =
								requestor;
	wma_handle->interfaces[vdev_id].scan_info.p2p_scan_type = p2p_scan_type;
}

/* function   : wma_reset_scan_info
 * Description : function to reset the current ongoing scan info
 * Args       : wma handle, vdev_id
 * Returns    : None
 */
static inline void wma_reset_scan_info(tp_wma_handle wma_handle,
				       u_int8_t vdev_id)
{
	vos_mem_zero((void *) &(wma_handle->interfaces[vdev_id].scan_info),
			sizeof(struct scan_param));
}

bool wma_check_scan_in_progress(WMA_HANDLE handle)
{
	tp_wma_handle wma_handle = handle;
	int i;

	for (i = 0; i < wma_handle->max_bssid; i++){
		if (wma_handle->interfaces[i].scan_info.scan_id){

			WMA_LOGE("%s: scan in progress on interface[%d],scanid = %d",
			__func__, i, wma_handle->interfaces[i].scan_info.scan_id );
			return true;
		}
	}
	return false;
}

v_BOOL_t wma_is_SAP_active(tp_wma_handle wma_handle)
{
	int i;

	for (i = 0; i < wma_handle->max_bssid; i++) {
		if (!wma_handle->interfaces[i].vdev_up)
			continue;
		if (wma_handle->interfaces[i].type == WMI_VDEV_TYPE_AP &&
		    wma_handle->interfaces[i].sub_type == 0)
			return TRUE;
	}
	return FALSE;
}

v_BOOL_t wma_is_P2P_GO_active(tp_wma_handle wma_handle)
{
	int i;

	for (i = 0; i < wma_handle->max_bssid; i++) {
		if (!wma_handle->interfaces[i].vdev_up)
			continue;
		if (wma_handle->interfaces[i].type == WMI_VDEV_TYPE_AP &&
		    wma_handle->interfaces[i].sub_type == WMI_UNIFIED_VDEV_SUBTYPE_P2P_GO)
			return TRUE;
	}
	return FALSE;
}

v_BOOL_t wma_is_P2P_CLI_active(tp_wma_handle wma_handle)
{
	int i;

	for (i = 0; i < wma_handle->max_bssid; i++) {
		if (!wma_handle->interfaces[i].vdev_up)
			continue;
		if (wma_handle->interfaces[i].type == WMI_VDEV_TYPE_STA &&
		    wma_handle->interfaces[i].sub_type == WMI_UNIFIED_VDEV_SUBTYPE_P2P_CLIENT)
			return TRUE;
	}
	return FALSE;
}

v_BOOL_t wma_is_STA_active(tp_wma_handle wma_handle)
{
	int i;

	for (i = 0; i < wma_handle->max_bssid; i++) {
		if (!wma_handle->interfaces[i].vdev_up)
			continue;
		if (wma_handle->interfaces[i].type == WMI_VDEV_TYPE_STA &&
		    wma_handle->interfaces[i].sub_type == 0)
			return TRUE;
		if (wma_handle->interfaces[i].type == WMI_VDEV_TYPE_IBSS)
			return TRUE;
	}
	return FALSE;
}


/* function   : wma_get_buf_start_scan_cmd
 * Description :
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_get_buf_start_scan_cmd(tp_wma_handle wma_handle,
					tSirScanOffloadReq *scan_req,
					wmi_buf_t *buf,
					int *buf_len)
{
	wmi_start_scan_cmd_fixed_param *cmd;
	wmi_chan_list *chan_list = NULL;
	wmi_mac_addr *bssid;
	wmi_ssid *ssid = NULL;
	u_int32_t *tmp_ptr, ie_len_with_pad;
	VOS_STATUS vos_status = VOS_STATUS_E_FAILURE;
	u_int8_t *buf_ptr;
	u_int32_t dwell_time;
	u_int8_t SSID_num;
	int i;
	int len = sizeof(*cmd);
	tpAniSirGlobal pMac = (tpAniSirGlobal )vos_get_context(VOS_MODULE_ID_PE,
				wma_handle->vos_context);

	if (!pMac) {
		WMA_LOGP("%s: pMac is NULL!", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	len += WMI_TLV_HDR_SIZE; /* Length TLV placeholder for array of uint32 */
	/* calculate the length of buffer required */
	if (scan_req->channelList.numChannels)
		len += scan_req->channelList.numChannels * sizeof(u_int32_t);

	len += WMI_TLV_HDR_SIZE; /* Length TLV placeholder for array of wmi_ssid structures */
	if (scan_req->numSsid)
		len += scan_req->numSsid * sizeof(wmi_ssid);

	len += WMI_TLV_HDR_SIZE; /* Length TLV placeholder for array of wmi_mac_addr structures */
	len += sizeof(wmi_mac_addr);

	len += WMI_TLV_HDR_SIZE; /* Length TLV placeholder for array of bytes */
	if (scan_req->uIEFieldLen)
		len += roundup(scan_req->uIEFieldLen, sizeof(u_int32_t));

	/* Allocate the memory */
	*buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
        if (!*buf) {
                WMA_LOGP("%s: failed to allocate memory for start scan cmd",
				__func__);
                return VOS_STATUS_E_FAILURE;
        }

	buf_ptr = (u_int8_t *) wmi_buf_data(*buf);
	cmd = (wmi_start_scan_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_start_scan_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_start_scan_cmd_fixed_param));

	if (wma_handle->scan_id >= WMA_MAX_SCAN_ID)
		wma_handle->scan_id = 0;

	cmd->vdev_id = scan_req->sessionId;
	/* host cycles through the lower 12 bits of
	   wma_handle->scan_id to generate ids */
	cmd->scan_id = WMA_HOST_SCAN_REQID_PREFIX | ++wma_handle->scan_id;
	cmd->scan_priority = WMI_SCAN_PRIORITY_LOW;
	cmd->scan_req_id = WMA_HOST_SCAN_REQUESTOR_ID_PREFIX |
			   WMA_DEFAULT_SCAN_REQUESTER_ID;

	/* Set the scan events which the driver is intereseted to receive */
	/* TODO: handle all the other flags also */
	cmd->notify_scan_events = WMI_SCAN_EVENT_STARTED |
				WMI_SCAN_EVENT_START_FAILED |
				WMI_SCAN_EVENT_FOREIGN_CHANNEL |
				WMI_SCAN_EVENT_COMPLETED |
				WMI_SCAN_EVENT_DEQUEUED |
				WMI_SCAN_EVENT_PREEMPTED |
				WMI_SCAN_EVENT_RESTARTED;

	cmd->dwell_time_active = scan_req->maxChannelTime;

	if (scan_req->scanType == eSIR_ACTIVE_SCAN) {
		/* In Active scan case, the firmware has to do passive scan on DFS channels
		 * So the passive scan duration should be updated properly so that the duration
		 * will be sufficient enough to receive the beacon from AP */

		if (wlan_cfgGetInt(pMac, WNI_CFG_PASSIVE_MAXIMUM_CHANNEL_TIME,
					&dwell_time) != eSIR_SUCCESS) {
			WMA_LOGE("Failed to get passive max channel value"
					"using default value");
			dwell_time = WMA_DWELL_TIME_PASSIVE_DEFAULT;
		}
		cmd->dwell_time_passive = dwell_time;
	}
	else
		cmd->dwell_time_passive = scan_req->maxChannelTime;

	WMA_LOGI("Scan Type %x, Active dwell time %u, Passive dwell time %u",
			scan_req->scanType, cmd->dwell_time_active,
			cmd->dwell_time_passive);

	/* Ensure correct number of probes are sent on active channel */
	cmd->repeat_probe_time = cmd->dwell_time_active / WMA_SCAN_NPROBES_DEFAULT;

	/* CSR sends only one value restTime for staying on home channel
	 * to continue data traffic. Rome fw has facility to monitor the traffic
	 * and move to next channel. Stay on the channel for at least half
	 * of the requested time and then leave if there is no traffic.
	 */
	cmd->min_rest_time = scan_req->restTime / 2;
	cmd->max_rest_time = scan_req->restTime;

	/* Check for traffic at idle_time interval after min_rest_time.
	 * Default value is 25 ms to allow full use of max_rest_time
	 * when voice packets are running at 20 ms interval.
	 */
	cmd->idle_time = WMA_SCAN_IDLE_TIME_DEFAULT;

	/* Large timeout value for full scan cycle, 30 seconds */
	cmd->max_scan_time = WMA_HW_DEF_SCAN_MAX_DURATION;

	/* do not add OFDM rates in 11B mode */
	if (scan_req->dot11mode != WNI_CFG_DOT11_MODE_11B)
		cmd->scan_ctrl_flags |= WMI_SCAN_ADD_OFDM_RATES;
	else
		WMA_LOGD("OFDM_RATES not included in 11B mode");

	/* Do not combine multiple channels in a single burst. Come back
	 * to home channel for data traffic after every foreign channel.
	 * By default, prefer throughput performance over scan cycle time.
	 */
	cmd->burst_duration = 0;

	if (!scan_req->p2pScanType) {
		WMA_LOGD("Normal Scan request");
		cmd->scan_ctrl_flags |= WMI_SCAN_ADD_CCK_RATES;
                if (!scan_req->numSsid)
                        cmd->scan_ctrl_flags |= WMI_SCAN_ADD_BCAST_PROBE_REQ;
		if (scan_req->scanType == eSIR_PASSIVE_SCAN)
			cmd->scan_ctrl_flags |= WMI_SCAN_FLAG_PASSIVE;
		cmd->scan_ctrl_flags |= WMI_SCAN_FILTER_PROBE_REQ;
		/*
		 * Decide burst_duration and dwell_time_active based on
		 * what type of devices are active.
		 */
		do {
		    if (wma_is_SAP_active(wma_handle)) {
			/* Background scan while SoftAP is sending beacons.
			 * Max duration of CTS2self is 32 ms, which limits
			 * the dwell time.
			 */
		        cmd->dwell_time_active = MIN(scan_req->maxChannelTime,
				(WMA_CTS_DURATION_MS_MAX - WMA_ROAM_SCAN_CHANNEL_SWITCH_TIME));
			cmd->dwell_time_passive = cmd->dwell_time_active;
			cmd->burst_duration = 0;
			break;
		    }
		    if (wma_is_P2P_GO_active(wma_handle)) {
			/* Background scan while GO is sending beacons.
			 * Every off-channel transition has overhead of 2 beacon
			 * intervals for NOA. Maximize number of channels in
			 * every transition by using burst scan.
			 */
			if (IS_MIRACAST_SESSION_PRESENT(pMac)) {
				/* When miracast is running, burst duration
				 * needs to be minimum to avoid any stutter
				 * or glitch in miracast during station scan
				 */
				if (scan_req->maxChannelTime <=
					WMA_GO_MIN_ACTIVE_SCAN_BURST_DURATION)
					cmd->burst_duration =
						scan_req->maxChannelTime;
				else
					cmd->burst_duration =
					WMA_GO_MIN_ACTIVE_SCAN_BURST_DURATION;
			}
			else {
				/* If miracast is not running, accomodate max
				 * stations to make the scans faster
				 */
				cmd->burst_duration =
					WMA_BURST_SCAN_MAX_NUM_OFFCHANNELS *
					scan_req->maxChannelTime;
				if (cmd->burst_duration >
				WMA_GO_MAX_ACTIVE_SCAN_BURST_DURATION) {
					u_int8_t channels =
						WMA_P2P_SCAN_MAX_BURST_DURATION
						/ scan_req->maxChannelTime;
					if (channels)
						cmd->burst_duration = channels *
						scan_req->maxChannelTime;
					else
						cmd->burst_duration =
					WMA_GO_MAX_ACTIVE_SCAN_BURST_DURATION;
					}
			}
			break;
		    }
		    if (wma_is_STA_active(wma_handle) ||
			wma_is_P2P_CLI_active(wma_handle)) {
			/* Typical background scan. Disable burst scan for now. */
			cmd->burst_duration = 0;
			break;
		    }
		} while (0);

	}
	else {
		WMA_LOGD("P2P Scan");
		switch (scan_req->p2pScanType) {
		case P2P_SCAN_TYPE_LISTEN:
			WMA_LOGD("P2P_SCAN_TYPE_LISTEN");
			cmd->scan_ctrl_flags |= WMI_SCAN_FLAG_PASSIVE;
			cmd->notify_scan_events |=
				WMI_SCAN_EVENT_FOREIGN_CHANNEL;
			cmd->repeat_probe_time = 0;
			break;
		case P2P_SCAN_TYPE_SEARCH:
			WMA_LOGD("P2P_SCAN_TYPE_SEARCH");
			cmd->scan_ctrl_flags |= WMI_SCAN_FILTER_PROBE_REQ;
			/* Default P2P burst duration of 120 ms will cover
			 * 3 channels with default max dwell time 40 ms.
			 * Cap limit will be set by
			 * WMA_P2P_SCAN_MAX_BURST_DURATION. Burst duration
			 * should be such that no channel is scanned less
			 * than the dwell time in normal scenarios.
			 */
			if (scan_req->channelList.numChannels == P2P_SOCIAL_CHANNELS
			 && (!IS_MIRACAST_SESSION_PRESENT(pMac)))
				cmd->repeat_probe_time = scan_req->maxChannelTime/5;
			else
				cmd->repeat_probe_time = scan_req->maxChannelTime/3;

			cmd->burst_duration = WMA_BURST_SCAN_MAX_NUM_OFFCHANNELS * scan_req->maxChannelTime;
			if (cmd->burst_duration > WMA_P2P_SCAN_MAX_BURST_DURATION) {
				u_int8_t channels = WMA_P2P_SCAN_MAX_BURST_DURATION / scan_req->maxChannelTime;
				if (channels)
					cmd->burst_duration = channels * scan_req->maxChannelTime;
				else
					cmd->burst_duration = WMA_P2P_SCAN_MAX_BURST_DURATION;
			}
			break;
		default:
			WMA_LOGE("Invalid scan type");
			goto error;
		}
	}

	cmd->n_probes = (cmd->repeat_probe_time > 0) ?
			    cmd->dwell_time_active/cmd->repeat_probe_time : 0;

	buf_ptr += sizeof(*cmd);
	tmp_ptr = (u_int32_t *) (buf_ptr + WMI_TLV_HDR_SIZE);

	if (scan_req->channelList.numChannels) {
		chan_list = (wmi_chan_list *) tmp_ptr;
		cmd->num_chan = scan_req->channelList.numChannels;
		for (i = 0; i < scan_req->channelList.numChannels; ++i) {
			tmp_ptr[i] = vos_chan_to_freq(
					scan_req->channelList.channelNumber[i]);
		}
	}
	WMITLV_SET_HDR(buf_ptr,
		       WMITLV_TAG_ARRAY_UINT32,
		       (cmd->num_chan * sizeof(u_int32_t)));
	buf_ptr += WMI_TLV_HDR_SIZE + (cmd->num_chan * sizeof(u_int32_t));
	if (scan_req->numSsid > SIR_SCAN_MAX_NUM_SSID) {
		WMA_LOGE("Invalid value for numSsid");
		goto error;
	}
	cmd->num_ssids = scan_req->numSsid;
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_FIXED_STRUC,
		       (cmd->num_ssids * sizeof(wmi_ssid)));
	if (scan_req->numSsid) {
		ssid = (wmi_ssid *) (buf_ptr + WMI_TLV_HDR_SIZE);
		for (i = 0; i < scan_req->numSsid; ++i) {
			ssid->ssid_len = scan_req->ssId[i].length;
			vos_mem_copy(ssid->ssid, scan_req->ssId[i].ssId,
					scan_req->ssId[i].length);
			ssid++;
		}
	}
	buf_ptr +=  WMI_TLV_HDR_SIZE + (cmd->num_ssids * sizeof(wmi_ssid));

	cmd->num_bssid = 1;

	if (!scan_req->p2pScanType) {
		if (wma_is_SAP_active(wma_handle)) {
			SSID_num = cmd->num_ssids * cmd->num_bssid;
			cmd->repeat_probe_time =
				probeTime_dwellTime_map[MIN(SSID_num,
				WMA_DWELL_TIME_PROBE_TIME_MAP_SIZE - 1)].
				probe_time;
		}
	}

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_FIXED_STRUC,
		       (cmd->num_bssid * sizeof(wmi_mac_addr)));
	bssid = (wmi_mac_addr *) (buf_ptr + WMI_TLV_HDR_SIZE);
	WMI_CHAR_ARRAY_TO_MAC_ADDR(scan_req->bssId, bssid);
	buf_ptr += WMI_TLV_HDR_SIZE + (cmd->num_bssid * sizeof(wmi_mac_addr));

	cmd->ie_len = scan_req->uIEFieldLen;
	ie_len_with_pad = roundup(scan_req->uIEFieldLen, sizeof(u_int32_t));
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, ie_len_with_pad);
	if (scan_req->uIEFieldLen) {
		vos_mem_copy(buf_ptr + WMI_TLV_HDR_SIZE,
			     (u_int8_t *)scan_req +
			     (scan_req->uIEFieldOffset),
			     scan_req->uIEFieldLen);
	}
	buf_ptr += WMI_TLV_HDR_SIZE + ie_len_with_pad;

	*buf_len = len;
	return VOS_STATUS_SUCCESS;
error:
        vos_mem_free(*buf);
        *buf = NULL;
        return vos_status;
}

/* function   : wma_get_buf_stop_scan_cmd
 * Description : function to fill the args for wmi_stop_scan_cmd
 * Args       : wma handle, wmi command buffer, buffer length, vdev_id
 * Returns    : failure or success
 */
VOS_STATUS wma_get_buf_stop_scan_cmd(tp_wma_handle wma_handle,
					wmi_buf_t *buf,
					int *buf_len,
					tAbortScanParams *abort_scan_req)
{
	wmi_stop_scan_cmd_fixed_param *cmd;
	VOS_STATUS vos_status;
	int len = sizeof(*cmd);

	/* Allocate the memory */
	*buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!*buf) {
		WMA_LOGP("%s: failed to allocate memory for stop scan cmd",
				__func__);
		vos_status = VOS_STATUS_E_FAILURE;
		goto error;
	}

	cmd = (wmi_stop_scan_cmd_fixed_param *) wmi_buf_data(*buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_stop_scan_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_stop_scan_cmd_fixed_param));
	cmd->vdev_id = abort_scan_req->SessionId;
	cmd->requestor =
	      wma_handle->interfaces[cmd->vdev_id].scan_info.scan_requestor_id;
	cmd->scan_id = wma_handle->interfaces[cmd->vdev_id].scan_info.scan_id;
	/* stop the scan with the corresponding scan_id */
	cmd->req_type = WMI_SCAN_STOP_ONE;

	*buf_len = len;
	vos_status = VOS_STATUS_SUCCESS;
error:
	return vos_status;

}

void wma_process_link_status_req(tp_wma_handle wma,
					tAniGetLinkStatus *pGetLinkStatus)
{
	wmi_buf_t buf;
	wmi_request_stats_cmd_fixed_param *cmd;
	u_int8_t len = sizeof(wmi_request_stats_cmd_fixed_param);
	struct wma_txrx_node *iface = &wma->interfaces[pGetLinkStatus->sessionId];

	if (iface->plink_status_req) {
		WMA_LOGE(
		"%s:previous link status request is pending,deleting the new request",
				__func__);
		vos_mem_free(pGetLinkStatus);
		return;
	}

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		goto end;
	}

	iface->plink_status_req = pGetLinkStatus;
	cmd = (wmi_request_stats_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_request_stats_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_request_stats_cmd_fixed_param));
	cmd->stats_id = WMI_REQUEST_VDEV_RATE_STAT;
	cmd->vdev_id = pGetLinkStatus->sessionId;
	if (wmi_unified_cmd_send(wma->wmi_handle, buf, len,
						WMI_REQUEST_STATS_CMDID)) {
		WMA_LOGE("Failed to send WMI link  status request to fw");
		wmi_buf_free(buf);
		iface->plink_status_req = NULL;
		goto end;
	}

	return;

end:
	wma_post_link_status(pGetLinkStatus, LINK_STATUS_LEGACY);
}

VOS_STATUS wma_send_snr_request(tp_wma_handle wma_handle, void *pGetRssiReq,
				v_S7_t first_rssi)
{
	wmi_buf_t buf;
	wmi_request_stats_cmd_fixed_param *cmd;
	u_int8_t len = sizeof(wmi_request_stats_cmd_fixed_param);
	tAniGetRssiReq *pRssiBkUp = NULL;

	/* command is in progess */
	if(NULL != wma_handle->pGetRssiReq)
		return VOS_STATUS_SUCCESS;

	wma_handle->first_rssi = first_rssi;

	/* create a copy of csrRssiCallback to send rssi value
	 * after wmi event
	 */
	if(pGetRssiReq) {
		pRssiBkUp = adf_os_mem_alloc(NULL, sizeof(tAniGetRssiReq));
		if(!pRssiBkUp) {
			WMA_LOGE("Failed to allocate memory for tAniGetRssiReq");
			wma_handle->pGetRssiReq = NULL;
			return VOS_STATUS_E_FAILURE;
		}
		adf_os_mem_set(pRssiBkUp, 0, sizeof(tAniGetRssiReq));
		pRssiBkUp->sessionId = ((tAniGetRssiReq*)pGetRssiReq)->sessionId;
		pRssiBkUp->rssiCallback = ((tAniGetRssiReq*)pGetRssiReq)->rssiCallback;
		pRssiBkUp->pDevContext = ((tAniGetRssiReq*)pGetRssiReq)->pDevContext;
		wma_handle->pGetRssiReq = (void*)pRssiBkUp;
	}

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		adf_os_mem_free(pRssiBkUp);
		wma_handle->pGetRssiReq = NULL;
		return VOS_STATUS_E_FAILURE;
	}

	cmd = (wmi_request_stats_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header, WMITLV_TAG_STRUC_wmi_request_stats_cmd_fixed_param,
				WMITLV_GET_STRUCT_TLVLEN(wmi_request_stats_cmd_fixed_param));
	cmd->stats_id = WMI_REQUEST_VDEV_STAT;
	if (wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,WMI_REQUEST_STATS_CMDID)) {
		WMA_LOGE("Failed to send host stats request to fw");
		wmi_buf_free(buf);
		adf_os_mem_free(pRssiBkUp);
		wma_handle->pGetRssiReq = NULL;
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

/* function   : wma_start_scan
 * Description :
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_start_scan(tp_wma_handle wma_handle,
                        tSirScanOffloadReq *scan_req, v_U16_t msg_type)
{
	uint32_t vdev_id, scan_id;
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	wmi_buf_t buf = NULL;
	wmi_start_scan_cmd_fixed_param *cmd;
	int status = 0;
	int len;
	tSirScanOffloadEvent *scan_event;

	if (scan_req->sessionId > wma_handle->max_bssid) {
		WMA_LOGE("%s: Invalid vdev_id %d, msg_type : 0x%x", __func__,
			scan_req->sessionId, msg_type);
		goto error1;
	}

	/* Sanity check to find whether vdev id active or not */
	if (!wma_handle->interfaces[scan_req->sessionId].handle) {
		WMA_LOGA("vdev id [%d] is not active", scan_req->sessionId);
		goto error1;
	}
        if (msg_type == WDA_START_SCAN_OFFLOAD_REQ) {
            /* Start the timer for scan completion */
            vos_status = vos_timer_start(&wma_handle->wma_scan_comp_timer,
                                            WMA_HW_DEF_SCAN_MAX_DURATION);
            if (vos_status != VOS_STATUS_SUCCESS ) {
                WMA_LOGE("Failed to start the scan completion timer");
                vos_status = VOS_STATUS_E_FAILURE;
                goto error1;
            }
        }
	/* Fill individual elements of wmi_start_scan_req and
	 * TLV for channel list, bssid, ssid etc ... */
	vos_status = wma_get_buf_start_scan_cmd(wma_handle, scan_req,
			&buf, &len);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to get buffer for start scan cmd");
		goto error0;
	}

	if (NULL == buf) {
		WMA_LOGE("Failed to get buffer for saving current scan info");
		goto error0;
	}

	/* Save current scan info */
	cmd = (wmi_start_scan_cmd_fixed_param *) wmi_buf_data(buf);
	if (msg_type == WDA_CHNL_SWITCH_REQ) {
	    /* Adjust parameters for channel switch scan */
	    cmd->min_rest_time = WMA_ROAM_PREAUTH_REST_TIME;
	    cmd->max_rest_time = WMA_ROAM_PREAUTH_REST_TIME;
	    cmd->max_scan_time = WMA_ROAM_PREAUTH_MAX_SCAN_TIME;
	    cmd->scan_priority = WMI_SCAN_PRIORITY_HIGH;
	    adf_os_spin_lock_bh(&wma_handle->roam_preauth_lock);
	    cmd->scan_id =  ( (cmd->scan_id & WMA_MAX_SCAN_ID) |
				WMA_HOST_ROAM_SCAN_REQID_PREFIX);
	    wma_handle->roam_preauth_scan_id = cmd->scan_id;
	    adf_os_spin_unlock_bh(&wma_handle->roam_preauth_lock);
	}

	wma_set_scan_info(wma_handle, cmd->scan_id,
			cmd->scan_req_id, cmd->vdev_id,
			scan_req->p2pScanType);

	WMA_LOGE("scan_id %x, vdev_id %x, scan type %x, msg_type %x",
			cmd->scan_id, cmd->vdev_id, scan_req->p2pScanType,
			msg_type);

	/*
	 * Cache vdev_id and scan_id because cmd is freed after calling
	 * wmi_unified_cmd_send cmd. WMI internally frees cmd buffer after
	 * getting TX complete from CE
	 */
	vdev_id = cmd->vdev_id;
	scan_id = cmd->scan_id;

	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
			len, WMI_START_SCAN_CMDID);
	/* Call the wmi api to request the scan */
	if (status != EOK) {
		WMA_LOGE("wmi_unified_cmd_send returned Error %d",
			status);
		vos_status = VOS_STATUS_E_FAILURE;
		goto error;
	}

	WMA_LOGI("WMA --> WMI_START_SCAN_CMDID");

	/* Update the scan parameters for handler */
	wma_handle->wma_scan_timer_info.vdev_id = vdev_id;
	wma_handle->wma_scan_timer_info.scan_id = scan_id;

	return VOS_STATUS_SUCCESS;
error:
	wma_reset_scan_info(wma_handle, cmd->vdev_id);
	if (buf)
		adf_nbuf_free(buf);
error0:
	/* Stop the timer for scan completion */
	if (vos_timer_stop(&wma_handle->wma_scan_comp_timer)
		!= VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to stop the scan completion timer");
	}
error1:
        /* Send completion event for only for start scan request */
        if (msg_type == WDA_START_SCAN_OFFLOAD_REQ) {
                scan_event =
                    (tSirScanOffloadEvent *) vos_mem_malloc(sizeof(tSirScanOffloadEvent));
                if (!scan_event) {
                        WMA_LOGP("%s: Failed to allocate memory for scan rsp",
					__func__);
                        return VOS_STATUS_E_NOMEM;
                }
                memset(scan_event, 0x00, sizeof(*scan_event));
                scan_event->event = WMI_SCAN_EVENT_COMPLETED;
                scan_event->reasonCode = eSIR_SME_SCAN_FAILED;
                scan_event->p2pScanType = scan_req->p2pScanType;
                scan_event->sessionId = scan_req->sessionId;
                wma_send_msg(wma_handle, WDA_RX_SCAN_EVENT, (void *) scan_event, 0) ;
        }
	return vos_status;
}

/* function   : wma_stop_scan
 * Description : function to send the stop scan command
 * Args       : wma_handle
 * Returns    : failure or success
 */
static VOS_STATUS wma_stop_scan(tp_wma_handle wma_handle,
			 tAbortScanParams *abort_scan_req)
{
	VOS_STATUS vos_status;
	wmi_buf_t buf;
	int status = 0;
	int len;

	vos_status = wma_get_buf_stop_scan_cmd(wma_handle, &buf, &len,
					       abort_scan_req);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to get buffer for stop scan cmd");
		goto error1;
	}

	if (NULL == buf) {
		WMA_LOGE("Failed to get buffer for stop scan cmd");
		vos_status = VOS_STATUS_E_FAULT;
		goto error1;
	}

	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
			len, WMI_STOP_SCAN_CMDID);
	/* Call the wmi api to request the scan */
	if (status != EOK) {
		WMA_LOGE("wmi_unified_cmd_send WMI_STOP_SCAN_CMDID returned Error %d",
			status);
		vos_status = VOS_STATUS_E_FAILURE;
		goto error;
	}

	WMA_LOGI("WMA --> WMI_STOP_SCAN_CMDID");

	return VOS_STATUS_SUCCESS;
error:
	if (buf)
		adf_nbuf_free(buf);
error1:
	return vos_status;
}

/* function   : wma_update_channel_list
 * Description : Function is used to update the support channel list
 * Args       : wma_handle, list of supported channels and power
 * Returns    : SUCCESS or FAILURE
 */
VOS_STATUS wma_update_channel_list(WMA_HANDLE handle,
				tSirUpdateChanList *chan_list)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	wmi_buf_t buf;
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	wmi_scan_chan_list_cmd_fixed_param *cmd;
	int status, i;
	u_int8_t *buf_ptr;
	wmi_channel *chan_info;
	u_int16_t len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;

	len += sizeof(wmi_channel) * chan_list->numChan;
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("Failed to allocate memory");
		vos_status = VOS_STATUS_E_NOMEM;
		goto end;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_scan_chan_list_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_scan_chan_list_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_scan_chan_list_cmd_fixed_param));

	WMA_LOGD("no of channels = %d, len = %d", chan_list->numChan, len);

	cmd->num_scan_chans = chan_list->numChan;
	WMITLV_SET_HDR((buf_ptr + sizeof(wmi_scan_chan_list_cmd_fixed_param)),
		       WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_channel) * chan_list->numChan);
	chan_info = (wmi_channel *) (buf_ptr + sizeof(*cmd) + WMI_TLV_HDR_SIZE);

	for (i = 0; i < chan_list->numChan; ++i) {
		WMITLV_SET_HDR(&chan_info->tlv_header,
			       WMITLV_TAG_STRUC_wmi_channel,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_channel));
		chan_info->mhz =
			vos_chan_to_freq(chan_list->chanParam[i].chanId);
		chan_info->band_center_freq1 = chan_info->mhz;
		chan_info->band_center_freq2 = 0;

		WMA_LOGD("chan[%d] = %u", i, chan_info->mhz);
		if (chan_list->chanParam[i].dfsSet) {
			WMI_SET_CHANNEL_FLAG(chan_info, WMI_CHAN_FLAG_PASSIVE);
			WMA_LOGI("chan[%d] DFS[%d]\n",
					chan_list->chanParam[i].chanId,
					chan_list->chanParam[i].dfsSet);
		}

		if (chan_info->mhz < WMA_2_4_GHZ_MAX_FREQ) {
			WMI_SET_CHANNEL_MODE(chan_info, MODE_11G);
		} else {
			WMI_SET_CHANNEL_MODE(chan_info, MODE_11A);
		}


		WMI_SET_CHANNEL_MAX_TX_POWER(chan_info,
					  chan_list->chanParam[i].pwr);

		WMI_SET_CHANNEL_REG_POWER(chan_info,
					  chan_list->chanParam[i].pwr);
		WMA_LOGD("Channel TX power[%d] = %u: %d", i, chan_info->mhz,
				chan_list->chanParam[i].pwr);
		/*TODO: Set WMI_SET_CHANNEL_MIN_POWER */
		/*TODO: Set WMI_SET_CHANNEL_ANTENNA_MAX */
		/*TODO: WMI_SET_CHANNEL_REG_CLASSID*/
		chan_info++;
	}

	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
			WMI_SCAN_CHAN_LIST_CMDID);

	if (status != EOK) {
		vos_status = VOS_STATUS_E_FAILURE;
		WMA_LOGE("Failed to send WMI_SCAN_CHAN_LIST_CMDID");
		wmi_buf_free(buf);
	}
end:
	return vos_status;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
VOS_STATUS wma_roam_scan_fill_self_caps(tp_wma_handle wma_handle,
		wmi_roam_offload_tlv_param *roam_offload_params,
		tSirRoamOffloadScanReq *roam_req)
{
	struct sAniSirGlobal *pMac = NULL;
	tSirMacCapabilityInfo selfCaps;
	tANI_U32 val = 0;
	tANI_U32 nCfgValue;
	tANI_U16 *pCfgValue16;
	tANI_U8  nCfgValue8, *pCfgValue8;
	tSirMacQosInfoStation macQosInfoSta;
	union {
		tANI_U16			nCfgValue16;
		tSirMacHTCapabilityInfo	 htCapInfo;
		tSirMacExtendedHTCapabilityInfo extHtCapInfo;
	} uHTCapabilityInfo;

	vos_mem_set(&macQosInfoSta, 0, sizeof(tSirMacQosInfoStation));
	/* Roaming is done only for INFRA STA type.
	 * So, ess will be one and ibss will be Zero */
	pMac = (struct sAniSirGlobal*)vos_get_context(VOS_MODULE_ID_PE,
				wma_handle->vos_context);
	if (!pMac) {
		WMA_LOGE("%s:NULL pMac ptr. Exiting", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAILURE;
	}

	if (wlan_cfgGetInt(pMac, WNI_CFG_PRIVACY_ENABLED, &val) !=
							eSIR_SUCCESS){
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_PRIVACY_ENABLED");
		return VOS_STATUS_E_FAILURE;
	}
	selfCaps.ess = 1;
	selfCaps.ibss = 0;
	if (val)
		selfCaps.privacy = 1;
	if (wlan_cfgGetInt(pMac, WNI_CFG_SHORT_PREAMBLE, &val) !=
						       eSIR_SUCCESS){
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_SHORT_PREAMBLE");
		return VOS_STATUS_E_FAILURE;
	}
	if (val)
		selfCaps.shortPreamble = 1;

	selfCaps.pbcc = 0;
	selfCaps.channelAgility = 0;
	if (wlan_cfgGetInt(pMac, WNI_CFG_11G_SHORT_SLOT_TIME_ENABLED,
					  &val) != eSIR_SUCCESS){
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_11G_SHORT_SLOT_TIME_ENABLED");
		return VOS_STATUS_E_FAILURE;
	}
	if (val)
		selfCaps.shortSlotTime = 1;
	if (wlan_cfgGetInt(pMac, WNI_CFG_11H_ENABLED, &val) != eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_11H_ENABLED");
		return VOS_STATUS_E_FAILURE;
	}
	if (val)
		selfCaps.spectrumMgt = 1;
	if (wlan_cfgGetInt(pMac, WNI_CFG_QOS_ENABLED, &val) != eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_QOS_ENABLED");
		return VOS_STATUS_E_FAILURE;
	}
	if (val)
		selfCaps.qos = 1;
	if (wlan_cfgGetInt(pMac, WNI_CFG_APSD_ENABLED, &val) != eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_APSD_ENABLED");
		return VOS_STATUS_E_FAILURE;
	}
	if (val)
		selfCaps.apsd = 1;
	if (wlan_cfgGetInt(pMac, WNI_CFG_RRM_ENABLED, &val) != eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_RRM_ENABLED");
		return VOS_STATUS_E_FAILURE;
	}
	if (val)
		selfCaps.rrm = 1;
	if (wlan_cfgGetInt(pMac, WNI_CFG_BLOCK_ACK_ENABLED, &val) !=
							       eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_BLOCK_ACK_ENABLED");
		return VOS_STATUS_E_FAILURE;
	}
	selfCaps.delayedBA =
		(tANI_U16)((val >> WNI_CFG_BLOCK_ACK_ENABLED_DELAYED) & 1);
	selfCaps.immediateBA =
		(tANI_U16)((val >> WNI_CFG_BLOCK_ACK_ENABLED_IMMEDIATE) & 1);
	pCfgValue16 = (tANI_U16 *)&selfCaps;
	roam_offload_params->capability = (*pCfgValue16) & 0xFFFF;

	if (wlan_cfgGetInt(pMac, WNI_CFG_HT_CAP_INFO, &nCfgValue) !=
							    eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_HT_CAP_INFO");
		return VOS_STATUS_E_FAILURE;
	}
	uHTCapabilityInfo.nCfgValue16 = nCfgValue & 0xFFFF;
	roam_offload_params->ht_caps_info =
				   uHTCapabilityInfo.nCfgValue16 & 0xFFFF;
	if (wlan_cfgGetInt(pMac, WNI_CFG_HT_AMPDU_PARAMS, &nCfgValue) !=
							    eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_HT_AMPDU_PARAMS");
		return VOS_STATUS_E_FAILURE;
	}
	/* tSirMacHTParametersInfo */
	nCfgValue8 = ( tANI_U8 ) nCfgValue;
	roam_offload_params->ampdu_param = (nCfgValue8) & 0xFF;

	val = ROAM_OFFLOAD_NUM_MCS_SET;
	if (wlan_cfgGetStr(pMac, WNI_CFG_SUPPORTED_MCS_SET,
			(tANI_U8*)roam_offload_params->mcsset,
				&val) != eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_SUPPORTED_MCS_SET");
		return VOS_STATUS_E_FAILURE;
	}
	if (wlan_cfgGetInt(pMac, WNI_CFG_EXT_HT_CAP_INFO, &nCfgValue) !=
							       eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_EXT_HT_CAP_INFO");
		return VOS_STATUS_E_FAILURE;
	}
	/* uHTCapabilityInfo.extHtCapInfo */
	uHTCapabilityInfo.nCfgValue16 = nCfgValue & 0xFFFF;
	roam_offload_params->ht_ext_cap =
				   uHTCapabilityInfo.nCfgValue16 & 0xFFFF;

	if (wlan_cfgGetInt(pMac, WNI_CFG_TX_BF_CAP, &nCfgValue) !=
							  eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_TX_BF_CAP");
		return VOS_STATUS_E_FAILURE;
	}
	/* tSirMacTxBFCapabilityInfo */
	nCfgValue8 = ( tANI_U8 ) nCfgValue;
	roam_offload_params->ht_txbf = nCfgValue8 & 0xFF;
	if (wlan_cfgGetInt(pMac, WNI_CFG_AS_CAP, &nCfgValue) !=
                                             eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_AS_CAP");
		return VOS_STATUS_E_FAILURE;
	}
	/* tSirMacASCapabilityInfo */
	nCfgValue8 = ( tANI_U8 ) nCfgValue;
	roam_offload_params->asel_cap = nCfgValue8 & 0xFF;

	/* QOS Info */
	if (wlan_cfgGetInt(pMac, WNI_CFG_MAX_SP_LENGTH, &nCfgValue) !=
							   eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get WNI_CFG_MAX_SP_LENGTH");
		return VOS_STATUS_E_FAILURE;
	}
	nCfgValue8 = ( tANI_U8 ) nCfgValue;
	macQosInfoSta.maxSpLen = nCfgValue8;
	macQosInfoSta.moreDataAck = 0;
	macQosInfoSta.qack = 0;
	macQosInfoSta.acbe_uapsd = roam_req->AcUapsd.acbe_uapsd;
	macQosInfoSta.acbk_uapsd = roam_req->AcUapsd.acbk_uapsd;
	macQosInfoSta.acvi_uapsd = roam_req->AcUapsd.acvi_uapsd;
	macQosInfoSta.acvo_uapsd = roam_req->AcUapsd.acvo_uapsd;
	pCfgValue8 = (tANI_U8 *)&macQosInfoSta;
	/* macQosInfoSta Only queue_request is set.Refer to
	 * PopulateDot11fWMMCaps for more details
	 */
	roam_offload_params->qos_caps = (*pCfgValue8) & 0xFF;
	roam_offload_params->wmm_caps = 0x4 & 0xFF;
	return VOS_STATUS_SUCCESS;
}

#endif
/* function   : wma_roam_scan_offload_mode
 * Description : send WMI_ROAM_SCAN_MODE TLV to firmware. It has a piggyback
 *	    : of WMI_ROAM_SCAN_MODE.
 * Args       : scan_cmd_fp contains the scan parameters.
 *	    : mode controls rssi based and periodic scans by roam engine.
 * Returns    :
 */
VOS_STATUS wma_roam_scan_offload_mode(tp_wma_handle wma_handle,
		wmi_start_scan_cmd_fixed_param *scan_cmd_fp,
		tSirRoamOffloadScanReq *roam_req,
		u_int32_t mode,
		u_int32_t vdev_id)
{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	wmi_buf_t buf = NULL;
	int status = 0;
	int len;
	u_int8_t *buf_ptr;
	wmi_roam_scan_mode_fixed_param *roam_scan_mode_fp;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	int auth_mode = WMI_AUTH_NONE;
	wmi_roam_offload_tlv_param *roam_offload_params;
	wmi_roam_11i_offload_tlv_param *roam_offload_11i;
	wmi_roam_11r_offload_tlv_param *roam_offload_11r;
	wmi_roam_ese_offload_tlv_param *roam_offload_ese;
	if (roam_req)
	    auth_mode = eCsrAuthType_to_rsn_authmode
	    (roam_req->ConnectedNetwork.authentication,
		 roam_req->ConnectedNetwork.encryption);
	WMA_LOGD("%s : auth mode = %d",__func__, auth_mode);
#endif
	/* Need to create a buf with roam_scan command at
	 * front and piggyback with scan command */
	len = sizeof(wmi_roam_scan_mode_fixed_param) +
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
		(2 * WMI_TLV_HDR_SIZE) +
#endif
		sizeof(wmi_start_scan_cmd_fixed_param);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (roam_req && roam_req->RoamOffloadEnabled) {
		len += sizeof(wmi_roam_offload_tlv_param);
		len += WMI_TLV_HDR_SIZE;
		if((auth_mode != WMI_AUTH_NONE) &&
			((auth_mode != WMI_AUTH_OPEN) ||
			(auth_mode == WMI_AUTH_OPEN &&
			roam_req->MDID.mdiePresent) || roam_req->IsESEAssoc)){
			len += WMI_TLV_HDR_SIZE;
			if(roam_req->IsESEAssoc)
			len += sizeof(wmi_roam_ese_offload_tlv_param);
			else if (auth_mode == WMI_AUTH_FT_RSNA ||
			auth_mode == WMI_AUTH_FT_RSNA_PSK ||
			(auth_mode == WMI_AUTH_OPEN &&
			roam_req->MDID.mdiePresent))
			len += sizeof(wmi_roam_11r_offload_tlv_param);
			else
			len += sizeof(wmi_roam_11i_offload_tlv_param);
		} else {
			len += WMI_TLV_HDR_SIZE;
		}
	} else {
		if (roam_req)
		   WMA_LOGD("%s : roam offload = %d",
			       __func__, roam_req->RoamOffloadEnabled);
		else
			WMA_LOGD("%s : roam_req is NULL",__func__);
		len += (2 * WMI_TLV_HDR_SIZE);
	}
	if (roam_req && roam_req->RoamOffloadEnabled) {
		mode = mode | WMI_ROAM_SCAN_MODE_ROAMOFFLOAD;
	}
#endif
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	roam_scan_mode_fp = (wmi_roam_scan_mode_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&roam_scan_mode_fp->tlv_header,
			WMITLV_TAG_STRUC_wmi_roam_scan_mode_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_roam_scan_mode_fixed_param));

	roam_scan_mode_fp->roam_scan_mode = mode;
	roam_scan_mode_fp->vdev_id = vdev_id;
	/* Fill in scan parameters suitable for roaming scan */
	buf_ptr += sizeof(wmi_roam_scan_mode_fixed_param);
	vos_mem_copy(buf_ptr, scan_cmd_fp, sizeof(wmi_start_scan_cmd_fixed_param));
	/* Ensure there is no additional IEs */
	scan_cmd_fp->ie_len = 0;
	WMITLV_SET_HDR(buf_ptr,
			WMITLV_TAG_STRUC_wmi_start_scan_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_start_scan_cmd_fixed_param));
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	buf_ptr += sizeof(wmi_start_scan_cmd_fixed_param);
	if (roam_req && roam_req->RoamOffloadEnabled) {
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		sizeof(wmi_roam_offload_tlv_param));
		buf_ptr += WMI_TLV_HDR_SIZE;
	    roam_offload_params = (wmi_roam_offload_tlv_param *) buf_ptr;
		 WMITLV_SET_HDR(buf_ptr,
			WMITLV_TAG_STRUC_wmi_roam_offload_tlv_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_roam_offload_tlv_param));
	    roam_offload_params->prefer_5g = roam_req->Prefer5GHz;
	    roam_offload_params->rssi_cat_gap = roam_req->RoamRssiCatGap;
	    roam_offload_params->select_5g_margin = roam_req->Select5GHzMargin;
	    roam_offload_params->reassoc_failure_timeout =
		roam_req->ReassocFailureTimeout;
	    /* Fill the capabilities */
	    wma_roam_scan_fill_self_caps(wma_handle, roam_offload_params, roam_req);
	    buf_ptr += sizeof(wmi_roam_offload_tlv_param);
	    /* The TLV's are in the order of 11i, 11R, ESE. Hence,
	     * they are filled in the same order.Depending on the
	     * authentication type, the other mode TLV's are nullified
	     * and only headers are filled.*/
	    if ((auth_mode != WMI_AUTH_NONE) &&
		((auth_mode != WMI_AUTH_OPEN) ||
		 (auth_mode == WMI_AUTH_OPEN && roam_req->MDID.mdiePresent) ||
		 (roam_req->IsESEAssoc))) {
		    if (roam_req->IsESEAssoc){
				WMITLV_SET_HDR(buf_ptr,WMITLV_TAG_ARRAY_STRUC,
				WMITLV_GET_STRUCT_TLVLEN(0));
				buf_ptr += WMI_TLV_HDR_SIZE;
				WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
				WMITLV_GET_STRUCT_TLVLEN(0));
				buf_ptr += WMI_TLV_HDR_SIZE;
				WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
				sizeof(wmi_roam_ese_offload_tlv_param));
				buf_ptr += WMI_TLV_HDR_SIZE;
				roam_offload_ese =
				  (wmi_roam_ese_offload_tlv_param *) buf_ptr;
				vos_mem_copy (roam_offload_ese->krk, roam_req->KRK,
							 sizeof(roam_req->KRK));
				vos_mem_copy (roam_offload_ese->btk, roam_req->BTK,
							 sizeof(roam_req->BTK));
				WMITLV_SET_HDR(&roam_offload_ese->tlv_header,
				WMITLV_TAG_STRUC_wmi_roam_ese_offload_tlv_param,
				WMITLV_GET_STRUCT_TLVLEN
				  (wmi_roam_ese_offload_tlv_param));
				buf_ptr += sizeof(wmi_roam_ese_offload_tlv_param);
			} else if (auth_mode == WMI_AUTH_FT_RSNA ||
				auth_mode == WMI_AUTH_FT_RSNA_PSK ||
				(auth_mode == WMI_AUTH_OPEN &&
				roam_req->MDID.mdiePresent)){
				WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
				buf_ptr += WMI_TLV_HDR_SIZE;
				WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
				sizeof(wmi_roam_11r_offload_tlv_param));
				buf_ptr += WMI_TLV_HDR_SIZE;
				roam_offload_11r =
				(wmi_roam_11r_offload_tlv_param *) buf_ptr;
				roam_offload_11r->r0kh_id_len = roam_req->R0KH_ID_Length;
				vos_mem_copy (roam_offload_11r->r0kh_id, roam_req->R0KH_ID,
						roam_offload_11r->r0kh_id_len);
				vos_mem_copy (roam_offload_11r->psk_msk, roam_req->PSK_PMK,
						       sizeof(roam_req->PSK_PMK));
				roam_offload_11r->psk_msk_len = roam_req->pmk_len;
				roam_offload_11r->mdie_present = roam_req->MDID.mdiePresent;
				roam_offload_11r->mdid = roam_req->MDID.mobilityDomain;
				if(auth_mode == WMI_AUTH_OPEN) {
					/* If FT-Open ensure pmk length
					 and r0khid len are zero */
					roam_offload_11r->r0kh_id_len = 0;
					roam_offload_11r->psk_msk_len = 0;
				}
				WMITLV_SET_HDR(&roam_offload_11r->tlv_header,
				WMITLV_TAG_STRUC_wmi_roam_11r_offload_tlv_param,
				WMITLV_GET_STRUCT_TLVLEN
				    (wmi_roam_11r_offload_tlv_param));
				buf_ptr += sizeof(wmi_roam_11r_offload_tlv_param);
				WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
				WMITLV_GET_STRUCT_TLVLEN(0));
				buf_ptr += WMI_TLV_HDR_SIZE;
			} else {
				WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
				sizeof(wmi_roam_11i_offload_tlv_param));
				buf_ptr += WMI_TLV_HDR_SIZE;
				roam_offload_11i =
				(wmi_roam_11i_offload_tlv_param *) buf_ptr;
				if (roam_req->RoamKeyMgmtOffloadEnabled) {
					WMI_SET_ROAM_OFFLOAD_OKC_ENABLED(
						roam_offload_11i->flags);
				        WMA_LOGE("LFR3:OKC Enabled");
				} else {
					WMI_SET_ROAM_OFFLOAD_OKC_DISABLED(
						roam_offload_11i->flags);
				        WMA_LOGE("LFR3:OKC Disabled");
				}


				vos_mem_copy (roam_offload_11i->pmk, roam_req->PSK_PMK,
                                             sizeof(roam_req->PSK_PMK));
				roam_offload_11i->pmk_len = roam_req->pmk_len;
				WMITLV_SET_HDR(&roam_offload_11i->tlv_header,
				WMITLV_TAG_STRUC_wmi_roam_11i_offload_tlv_param,
				WMITLV_GET_STRUCT_TLVLEN
				(wmi_roam_11i_offload_tlv_param) );
				buf_ptr += sizeof(wmi_roam_11i_offload_tlv_param);
				WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,0);
				buf_ptr += WMI_TLV_HDR_SIZE;
				WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,0);
				buf_ptr += WMI_TLV_HDR_SIZE;
			}
		} else {
			WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
			WMITLV_GET_STRUCT_TLVLEN(0));
			buf_ptr += WMI_TLV_HDR_SIZE;
			WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
			WMITLV_GET_STRUCT_TLVLEN(0));
			buf_ptr += WMI_TLV_HDR_SIZE;
			WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
			WMITLV_GET_STRUCT_TLVLEN(0));
	    }
	} else {
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		    WMITLV_GET_STRUCT_TLVLEN(0));
	    buf_ptr += WMI_TLV_HDR_SIZE;
		WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		    WMITLV_GET_STRUCT_TLVLEN(0));
	    buf_ptr += WMI_TLV_HDR_SIZE;
	    WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		    WMITLV_GET_STRUCT_TLVLEN(0));
	    buf_ptr += WMI_TLV_HDR_SIZE;
	    WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		    WMITLV_GET_STRUCT_TLVLEN(0));
	}
#endif
	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
			len, WMI_ROAM_SCAN_MODE);
	if (status != EOK) {
		WMA_LOGE
		("wmi_unified_cmd_send WMI_ROAM_SCAN_MODE returned Error %d",
							     status);
		vos_status = VOS_STATUS_E_FAILURE;
		goto error;
	}

	WMA_LOGI("%s: WMA --> WMI_ROAM_SCAN_MODE", __func__);
	return VOS_STATUS_SUCCESS;
error:
	wmi_buf_free(buf);

	return vos_status;
}

/* function   : wma_roam_scan_offload_rssi_threshold
 * Description : Send WMI_ROAM_SCAN_RSSI_THRESHOLD TLV to firmware
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_roam_scan_offload_rssi_thresh(tp_wma_handle wma_handle,
         tSirRoamOffloadScanReq *roam_req)
{
    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
    wmi_buf_t buf = NULL;
    int status = 0;
    int len, rssi_thresh, rssi_thresh_diff;
    u_int8_t *buf_ptr;
    wmi_roam_scan_rssi_threshold_fixed_param *rssi_threshold_fp;
    wmi_roam_scan_extended_threshold_param *ext_thresholds = NULL;
    struct roam_ext_params *roam_params;
    uint32_t hirssi_scan_max_count;
    uint32_t hirssi_scan_delta;
    int32_t hirssi_upper_bound;
    int32_t good_rssi_threshold;

    /* Send rssi threshold */
    roam_params = &roam_req->roam_params;
    rssi_thresh = roam_req->LookupThreshold - WMA_NOISE_FLOOR_DBM_DEFAULT;
    rssi_thresh_diff = roam_req->OpportunisticScanThresholdDiff;
    hirssi_scan_max_count = roam_req->hi_rssi_scan_max_count;
    hirssi_scan_delta = roam_req->hi_rssi_scan_rssi_delta;
    hirssi_upper_bound = roam_req->hi_rssi_scan_rssi_ub -
                                 WMA_NOISE_FLOOR_DBM_DEFAULT;
    len = sizeof(wmi_roam_scan_rssi_threshold_fixed_param);
    len += WMI_TLV_HDR_SIZE; /* TLV for ext_thresholds*/
    len += sizeof(wmi_roam_scan_extended_threshold_param);
    buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
    if (!buf) {
        WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    buf_ptr = (u_int8_t *) wmi_buf_data(buf);
    rssi_threshold_fp = (wmi_roam_scan_rssi_threshold_fixed_param *) buf_ptr;
    WMITLV_SET_HDR(&rssi_threshold_fp->tlv_header,
               WMITLV_TAG_STRUC_wmi_roam_scan_rssi_threshold_fixed_param,
               WMITLV_GET_STRUCT_TLVLEN(
                   wmi_roam_scan_rssi_threshold_fixed_param));
    /* fill in threshold values */
    rssi_threshold_fp->vdev_id = roam_req->sessionId;
    rssi_threshold_fp->roam_scan_rssi_thresh = rssi_thresh & 0x000000ff;
    rssi_threshold_fp->roam_rssi_thresh_diff = rssi_thresh_diff & 0x000000ff;
    rssi_threshold_fp->hirssi_scan_max_count = hirssi_scan_max_count;
    rssi_threshold_fp->hirssi_scan_delta = hirssi_scan_delta;
    rssi_threshold_fp->hirssi_upper_bound = hirssi_upper_bound & 0x00000ff;

    buf_ptr += sizeof(wmi_roam_scan_rssi_threshold_fixed_param);
    WMITLV_SET_HDR(buf_ptr,
               WMITLV_TAG_ARRAY_STRUC,
               sizeof(wmi_roam_scan_extended_threshold_param));
    buf_ptr += WMI_TLV_HDR_SIZE;
    ext_thresholds = (wmi_roam_scan_extended_threshold_param *) buf_ptr;
    /* The current Noise floor in firmware is -96dBm. Penalty/Boost threshold is
     * applied on a weaker signal to make it even more weaker. So, there
     * is a chance that the user may configure a very low Penalty/Boost
     * threshold beyond the noise floor. If that is the case, then suppress
     * the penalty/boost threshold to the noise floor.
     */
    if (roam_params->raise_rssi_thresh_5g < WMA_NOISE_FLOOR_DBM_DEFAULT)
         ext_thresholds->penalty_threshold_5g = 0;
    else
         ext_thresholds->boost_threshold_5g =
          (roam_params->raise_rssi_thresh_5g - WMA_NOISE_FLOOR_DBM_DEFAULT) &
          0x000000ff;
    if (roam_params->drop_rssi_thresh_5g < WMA_NOISE_FLOOR_DBM_DEFAULT)
         ext_thresholds->penalty_threshold_5g = 0;
    else
         ext_thresholds->penalty_threshold_5g =
           (roam_params->drop_rssi_thresh_5g - WMA_NOISE_FLOOR_DBM_DEFAULT) &
            0x000000ff;
    ext_thresholds->boost_algorithm_5g = WMI_ROAM_5G_BOOST_PENALIZE_ALGO_LINEAR;
    ext_thresholds->boost_factor_5g = roam_params->raise_factor_5g;
    ext_thresholds->penalty_algorithm_5g =
      WMI_ROAM_5G_BOOST_PENALIZE_ALGO_LINEAR;
    ext_thresholds->penalty_factor_5g = roam_params->drop_factor_5g;
    ext_thresholds->max_boost_5g = roam_params->max_raise_rssi_5g;
    ext_thresholds->max_penalty_5g = roam_params->max_drop_rssi_5g;
    if (roam_params->good_rssi_roam)
       good_rssi_threshold = WMA_NOISE_FLOOR_DBM_DEFAULT;
    else
       good_rssi_threshold = 0;
    ext_thresholds->good_rssi_threshold =
      (good_rssi_threshold - WMA_NOISE_FLOOR_DBM_DEFAULT) & 0x000000ff;
    WMA_LOGD("WMA --> good_rssi_threshold=%d",
              ext_thresholds->good_rssi_threshold);
    WMITLV_SET_HDR(&ext_thresholds->tlv_header,
      WMITLV_TAG_STRUC_wmi_roam_scan_extended_threshold_param,
      WMITLV_GET_STRUCT_TLVLEN
      (wmi_roam_scan_extended_threshold_param));

    status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
            len, WMI_ROAM_SCAN_RSSI_THRESHOLD);
    if (status != EOK) {
        WMA_LOGE("wmi_unified_cmd_send WMI_ROAM_SCAN_RSSI_THRESHOLD returned Error %d",
            status);
        vos_status = VOS_STATUS_E_FAILURE;
        goto error;
    }

    WMA_LOGI(
            "%s: WMA --> WMI_ROAM_SCAN_RSSI_THRESHOLD roam_scan_rssi_thresh=%d, roam_rssi_thresh_diff=%d",
            __func__, rssi_thresh, rssi_thresh_diff);
    WMA_LOGI(
            "%s: WMA --> WMI_ROAM_SCAN_RSSI_THRESHOLD hirssi_scan max_count=%d, delta=%d",
            __func__, hirssi_scan_max_count, hirssi_scan_delta);
    WMA_LOGI(
            "%s: WMA --> WMI_ROAM_SCAN_RSSI_THRESHOLD hirssi_upper_bound=%d",
            __func__, hirssi_upper_bound);
    return VOS_STATUS_SUCCESS;
error:
    wmi_buf_free(buf);

    return vos_status;
}

/* function   : wma_roam_scan_offload_scan_period
 * Description : Send WMI_ROAM_SCAN_PERIOD TLV to firmware
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_roam_scan_offload_scan_period(tp_wma_handle wma_handle,
        A_UINT32 scan_period,
        A_UINT32 scan_age,
        u_int32_t vdev_id)
{
    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
    wmi_buf_t buf = NULL;
    int status = 0;
    int len;
    u_int8_t *buf_ptr;
    wmi_roam_scan_period_fixed_param *scan_period_fp;

    /* Send scan period values */
    len = sizeof(wmi_roam_scan_period_fixed_param);
    buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
    if (!buf) {
        WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    buf_ptr = (u_int8_t *) wmi_buf_data(buf);
    scan_period_fp = (wmi_roam_scan_period_fixed_param *) buf_ptr;
    WMITLV_SET_HDR(&scan_period_fp->tlv_header,
               WMITLV_TAG_STRUC_wmi_roam_scan_period_fixed_param,
               WMITLV_GET_STRUCT_TLVLEN(
                   wmi_roam_scan_period_fixed_param));
    /* fill in scan period values */
    scan_period_fp->vdev_id = vdev_id;
    scan_period_fp->roam_scan_period = scan_period; /* 20 seconds */
    scan_period_fp->roam_scan_age = scan_age;

    status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
            len, WMI_ROAM_SCAN_PERIOD);
    if (status != EOK) {
        WMA_LOGE("wmi_unified_cmd_send WMI_ROAM_SCAN_PERIOD returned Error %d",
            status);
        vos_status = VOS_STATUS_E_FAILURE;
        goto error;
    }

    WMA_LOGI("%s: WMA --> WMI_ROAM_SCAN_PERIOD roam_scan_period=%d, roam_scan_age=%d",
                    __func__, scan_period, scan_age);
    return VOS_STATUS_SUCCESS;
error:
    wmi_buf_free(buf);

    return vos_status;
}
/* function   : wma_roam_scan_offload_rssi_change
 * Description : Send WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD TLV to firmware
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_roam_scan_offload_rssi_change(tp_wma_handle wma_handle,
        u_int32_t vdev_id,
        A_INT32 rssi_change_thresh,
        A_UINT32 bcn_rssi_weight,
        A_UINT32 hirssi_delay_btw_scans)
{
    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
    wmi_buf_t buf = NULL;
    int status = 0;
    int len;
    u_int8_t *buf_ptr;
    wmi_roam_scan_rssi_change_threshold_fixed_param *rssi_change_fp;

    /* Send rssi change parameters */
    len = sizeof(wmi_roam_scan_rssi_change_threshold_fixed_param);
    buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
    if (!buf) {
        WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    buf_ptr = (u_int8_t *) wmi_buf_data(buf);
    rssi_change_fp = (wmi_roam_scan_rssi_change_threshold_fixed_param *) buf_ptr;
    WMITLV_SET_HDR(&rssi_change_fp->tlv_header,
               WMITLV_TAG_STRUC_wmi_roam_scan_rssi_change_threshold_fixed_param,
               WMITLV_GET_STRUCT_TLVLEN(
                   wmi_roam_scan_rssi_change_threshold_fixed_param));
    /* fill in rssi change threshold (hysteresis) values */
    rssi_change_fp->vdev_id = vdev_id;
    rssi_change_fp->roam_scan_rssi_change_thresh = rssi_change_thresh;
    rssi_change_fp->bcn_rssi_weight = bcn_rssi_weight;
    rssi_change_fp->hirssi_delay_btw_scans = hirssi_delay_btw_scans;

    status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
            len, WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD);
    if (status != EOK) {
        WMA_LOGE("wmi_unified_cmd_send WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD returned Error %d",
            status);
        vos_status = VOS_STATUS_E_FAILURE;
        goto error;
    }

    WMA_LOGI("%s: WMA --> WMI_ROAM_SCAN_RSSI_CHANGE_THERSHOLD roam_scan_rssi_change_thresh=%d, bcn_rssi_weight=%d",
                    __func__, rssi_change_thresh, bcn_rssi_weight);
    WMA_LOGI("%s: WMA --> WMI_ROAM_SCAN_RSSI_CHANGE_THERSHOLD hirssi_delay_btw_scans=%d",
                    __func__, hirssi_delay_btw_scans);
    return VOS_STATUS_SUCCESS;
error:
    wmi_buf_free(buf);

    return vos_status;
}

/* function   : wma_roam_scan_offload_chan_list
 * Description : Send WMI_ROAM_CHAN_LIST TLV to firmware
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_roam_scan_offload_chan_list(tp_wma_handle wma_handle,
        u_int8_t chan_count,
        u_int8_t *chan_list,
        u_int8_t list_type,
        u_int32_t vdev_id)
{
    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
    wmi_buf_t buf = NULL;
    int status = 0;
    int len, list_tlv_len;
    int i;
    u_int8_t *buf_ptr;
    wmi_roam_chan_list_fixed_param *chan_list_fp;
    A_UINT32    *roam_chan_list_array;

    if (chan_count == 0)
    {
        WMA_LOGD("%s : invalid number of channels %d", __func__, chan_count);
        return VOS_STATUS_E_EMPTY;
    }
    /* Channel list is a table of 2 TLV's */
    list_tlv_len = WMI_TLV_HDR_SIZE + chan_count * sizeof(A_UINT32);
    len = sizeof(wmi_roam_chan_list_fixed_param) + list_tlv_len;
    buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
    if (!buf) {
        WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    buf_ptr = (u_int8_t *) wmi_buf_data(buf);
    chan_list_fp = (wmi_roam_chan_list_fixed_param *) buf_ptr;
    WMITLV_SET_HDR(&chan_list_fp->tlv_header,
               WMITLV_TAG_STRUC_wmi_roam_chan_list_fixed_param,
               WMITLV_GET_STRUCT_TLVLEN(wmi_roam_chan_list_fixed_param));
    chan_list_fp->vdev_id = vdev_id;
    chan_list_fp->num_chan = chan_count;
    if (chan_count > 0 && list_type == CHANNEL_LIST_STATIC) {
        /* external app is controlling channel list */
        chan_list_fp->chan_list_type = WMI_ROAM_SCAN_CHAN_LIST_TYPE_STATIC;
    } else {
        /* umac supplied occupied channel list in LFR */
        chan_list_fp->chan_list_type = WMI_ROAM_SCAN_CHAN_LIST_TYPE_DYNAMIC;
    }

    buf_ptr += sizeof(wmi_roam_chan_list_fixed_param);
    WMITLV_SET_HDR(buf_ptr,    WMITLV_TAG_ARRAY_UINT32,
               (chan_list_fp->num_chan * sizeof(u_int32_t)));
    roam_chan_list_array = (A_UINT32 *)(buf_ptr + WMI_TLV_HDR_SIZE);
    WMA_LOGI("%s: %d channels = ", __func__, chan_list_fp->num_chan);
    for (i = 0; ((i < chan_list_fp->num_chan) &&
                 (i < SIR_ROAM_MAX_CHANNELS)); i++) {
        roam_chan_list_array[i] = vos_chan_to_freq(chan_list[i]);
        WMA_LOGI("%d,",roam_chan_list_array[i]);
    }

    status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
            len, WMI_ROAM_CHAN_LIST);
    if (status != EOK) {
        WMA_LOGE("wmi_unified_cmd_send WMI_ROAM_CHAN_LIST returned Error %d",
            status);
        vos_status = VOS_STATUS_E_FAILURE;
        goto error;
    }

    WMA_LOGI("%s: WMA --> WMI_ROAM_SCAN_CHAN_LIST", __func__);
    return VOS_STATUS_SUCCESS;
error:
    wmi_buf_free(buf);

    return vos_status;
}

/* function   : eCsrAuthType_to_rsn_authmode
 * Description : Map CSR's authentication type into RSN auth mode used by firmware
 * Args       :
 * Returns    :
 */


A_UINT32 eCsrAuthType_to_rsn_authmode (eCsrAuthType authtype, eCsrEncryptionType encr) {
    switch(authtype) {
        case    eCSR_AUTH_TYPE_OPEN_SYSTEM:
            return (WMI_AUTH_OPEN);
        case    eCSR_AUTH_TYPE_WPA:
            return (WMI_AUTH_WPA);
        case    eCSR_AUTH_TYPE_WPA_PSK:
            return (WMI_AUTH_WPA_PSK);
        case    eCSR_AUTH_TYPE_RSN:
            return (WMI_AUTH_RSNA);
        case    eCSR_AUTH_TYPE_RSN_PSK:
            return (WMI_AUTH_RSNA_PSK);
#if defined WLAN_FEATURE_VOWIFI_11R
        case    eCSR_AUTH_TYPE_FT_RSN:
            return (WMI_AUTH_FT_RSNA);
        case    eCSR_AUTH_TYPE_FT_RSN_PSK:
            return (WMI_AUTH_FT_RSNA_PSK);
#endif
#ifdef FEATURE_WLAN_WAPI
        case    eCSR_AUTH_TYPE_WAPI_WAI_CERTIFICATE:
            return (WMI_AUTH_WAPI);
        case    eCSR_AUTH_TYPE_WAPI_WAI_PSK:
            return(WMI_AUTH_WAPI_PSK);
#endif
#ifdef FEATURE_WLAN_ESE
        case    eCSR_AUTH_TYPE_CCKM_WPA:
        case    eCSR_AUTH_TYPE_CCKM_RSN:
            return (WMI_AUTH_CCKM);
#endif
#ifdef WLAN_FEATURE_11W
        case    eCSR_AUTH_TYPE_RSN_PSK_SHA256:
            return (WMI_AUTH_RSNA_PSK_SHA256);
        case    eCSR_AUTH_TYPE_RSN_8021X_SHA256:
            return (WMI_AUTH_RSNA_8021X_SHA256);
#endif
        case    eCSR_AUTH_TYPE_NONE:
        case    eCSR_AUTH_TYPE_AUTOSWITCH:
            /* In case of WEP and other keys, NONE means OPEN auth */
            if (encr == eCSR_ENCRYPT_TYPE_WEP40_STATICKEY ||
                encr == eCSR_ENCRYPT_TYPE_WEP104_STATICKEY ||
                encr == eCSR_ENCRYPT_TYPE_WEP40 ||
                encr == eCSR_ENCRYPT_TYPE_WEP104 ||
                encr == eCSR_ENCRYPT_TYPE_TKIP ||
                encr == eCSR_ENCRYPT_TYPE_AES) {
                return (WMI_AUTH_OPEN);
            }
            return(WMI_AUTH_NONE);
        default:
            return(WMI_AUTH_NONE);
    }
}

/* function   : eCsrEncryptionType_to_rsn_cipherset
 * Description : Map CSR's encryption type into RSN cipher types used by firmware
 * Args       :
 * Returns    :
 */

A_UINT32 eCsrEncryptionType_to_rsn_cipherset (eCsrEncryptionType encr) {

    switch (encr) {
        case    eCSR_ENCRYPT_TYPE_WEP40_STATICKEY:
        case    eCSR_ENCRYPT_TYPE_WEP104_STATICKEY:
        case    eCSR_ENCRYPT_TYPE_WEP40:
        case    eCSR_ENCRYPT_TYPE_WEP104:
            return (WMI_CIPHER_WEP);
        case    eCSR_ENCRYPT_TYPE_TKIP:
            return (WMI_CIPHER_TKIP);
        case    eCSR_ENCRYPT_TYPE_AES:
            return (WMI_CIPHER_AES_CCM);
#ifdef FEATURE_WLAN_WAPI
        case    eCSR_ENCRYPT_TYPE_WPI:
            return (WMI_CIPHER_WAPI);
#endif /* FEATURE_WLAN_WAPI */
        case    eCSR_ENCRYPT_TYPE_ANY:
            return (WMI_CIPHER_ANY);
        case    eCSR_ENCRYPT_TYPE_NONE:
        default:
            return (WMI_CIPHER_NONE);
    }
}

/* function   : wma_roam_scan_fill_ap_profile
 * Description : Fill ap_profile structure from configured parameters
 * Args       :
 * Returns    :
 */
v_VOID_t wma_roam_scan_fill_ap_profile(tp_wma_handle wma_handle, tpAniSirGlobal pMac,
                       tSirRoamOffloadScanReq *roam_req, wmi_ap_profile *ap_profile_p)
{
    vos_mem_zero(ap_profile_p, sizeof(wmi_ap_profile));
    if (roam_req == NULL) {
        ap_profile_p->ssid.ssid_len = 0;
        ap_profile_p->ssid.ssid[0] = 0;
        ap_profile_p->rsn_authmode = WMI_AUTH_NONE;
        ap_profile_p->rsn_ucastcipherset = WMI_CIPHER_NONE;
        ap_profile_p->rsn_mcastcipherset = WMI_CIPHER_NONE;
        ap_profile_p->rsn_mcastmgmtcipherset = WMI_CIPHER_NONE;
        ap_profile_p->rssi_threshold = WMA_ROAM_RSSI_DIFF_DEFAULT;
    } else {
        ap_profile_p->ssid.ssid_len = roam_req->ConnectedNetwork.ssId.length;
        vos_mem_copy(ap_profile_p->ssid.ssid, roam_req->ConnectedNetwork.ssId.ssId,
                 ap_profile_p->ssid.ssid_len);
        ap_profile_p->rsn_authmode =
                eCsrAuthType_to_rsn_authmode(roam_req->ConnectedNetwork.authentication,
                                             roam_req->ConnectedNetwork.encryption);
        ap_profile_p->rsn_ucastcipherset =
                eCsrEncryptionType_to_rsn_cipherset(roam_req->ConnectedNetwork.encryption);
        ap_profile_p->rsn_mcastcipherset =
                eCsrEncryptionType_to_rsn_cipherset(roam_req->ConnectedNetwork.mcencryption);
        ap_profile_p->rsn_mcastmgmtcipherset = ap_profile_p->rsn_mcastcipherset;
        ap_profile_p->rssi_threshold = roam_req->RoamRssiDiff;
    }
}

/* function   : wma_roam_scan_scan_params
 * Description : Fill scan_params structure from configured parameters
 * Args       : roam_req pointer = NULL if this routine is called before connect
 *            : It will be non-NULL if called after assoc.
 * Returns    :
 */
v_VOID_t wma_roam_scan_fill_scan_params(tp_wma_handle wma_handle,
                                   tpAniSirGlobal pMac,
                                   tSirRoamOffloadScanReq *roam_req,
                                   wmi_start_scan_cmd_fixed_param *scan_params)
{
    tANI_U8 channels_per_burst = 0;
    tANI_U32 val = 0;

    if (NULL == pMac) {
        WMA_LOGE("%s: pMac is NULL", __func__);
        return;
    }

    vos_mem_zero(scan_params, sizeof(wmi_start_scan_cmd_fixed_param));
    scan_params->scan_ctrl_flags = WMI_SCAN_ADD_CCK_RATES |
                WMI_SCAN_ADD_OFDM_RATES;
    if (roam_req != NULL) {
        /* Parameters updated after association is complete */
        WMA_LOGI("%s: Input parameters: NeighborScanChannelMinTime"
                 " = %d, NeighborScanChannelMaxTime = %d",
                 __func__,
                 roam_req->NeighborScanChannelMinTime,
                 roam_req->NeighborScanChannelMaxTime);
        WMA_LOGI("%s: Input parameters: NeighborScanTimerPeriod ="
                 " %d, HomeAwayTime = %d, nProbes = %d",
                 __func__,
                 roam_req->NeighborScanTimerPeriod,
                 roam_req->HomeAwayTime,
                 roam_req->nProbes);

        /*
         * roam_req->NeighborScanChannelMaxTime = SCAN_CHANNEL_TIME
         * roam_req->HomeAwayTime               = SCAN_HOME_AWAY_TIME
         * roam_req->NeighborScanTimerPeriod    = SCAN_HOME_TIME
         *
         * scan_params->dwell_time_active  = time station stays on channel
         *                                   and sends probes;
         * scan_params->dwell_time_passive = time station stays on channel
         *                                   and listens probes;
         * scan_params->burst_duration     = time station goes off channel
         *                                   to scan;
         */

        if (wlan_cfgGetInt(pMac, WNI_CFG_PASSIVE_MAXIMUM_CHANNEL_TIME, &val) != eSIR_SUCCESS)
        {
            /*
             * Could not get max channel value from CFG. Log error.
             */
            WMA_LOGE("could not retrieve passive max channel value");

            /* use a default value of 110ms */
            val = WMA_ROAM_DWELL_TIME_PASSIVE_DEFAULT;
        }

        scan_params->dwell_time_passive = val;
        /*
         * Here is the formula,
         * T(HomeAway) = N * T(dwell) + (N+1) * T(cs)
         * where N is number of channels scanned in single burst
         */
        scan_params->dwell_time_active  = roam_req->NeighborScanChannelMaxTime;
        if (roam_req->HomeAwayTime < 2*WMA_ROAM_SCAN_CHANNEL_SWITCH_TIME) {
            /* clearly we can't follow home away time.
             * Make it a split scan.
             */
            scan_params->burst_duration     = 0;
        } else {
            channels_per_burst =
              (roam_req->HomeAwayTime - WMA_ROAM_SCAN_CHANNEL_SWITCH_TIME)
              / ( scan_params->dwell_time_active + WMA_ROAM_SCAN_CHANNEL_SWITCH_TIME);

            if (channels_per_burst < 1) {
                // dwell time and home away time conflicts
                // we will override dwell time
                scan_params->dwell_time_active =
                  roam_req->HomeAwayTime - 2*WMA_ROAM_SCAN_CHANNEL_SWITCH_TIME;
                scan_params->burst_duration = scan_params->dwell_time_active;
            } else {
                scan_params->burst_duration =
                  channels_per_burst * scan_params->dwell_time_active;
            }
        }
        if (roam_req->allowDFSChannelRoam == SIR_ROAMING_DFS_CHANNEL_ENABLED_NORMAL &&
            roam_req->HomeAwayTime > 0 &&
            roam_req->ChannelCacheType != CHANNEL_LIST_STATIC) {
            /* Roaming on DFS channels is supported and it is not app channel list.
             * It is ok to override homeAwayTime to accomodate DFS dwell time in burst
             * duration.
             */
            scan_params->burst_duration = MAX(scan_params->burst_duration,
                                                scan_params->dwell_time_passive);
        }
        scan_params->min_rest_time = roam_req->NeighborScanTimerPeriod;
        scan_params->max_rest_time = roam_req->NeighborScanTimerPeriod;
        scan_params->repeat_probe_time = (roam_req->nProbes > 0) ?
              VOS_MAX(scan_params->dwell_time_active / roam_req->nProbes, 1) : 0;
        scan_params->probe_spacing_time = 0;
        scan_params->probe_delay = 0;
        scan_params->max_scan_time = WMA_HW_DEF_SCAN_MAX_DURATION; /* 30 seconds for full scan cycle */
        scan_params->idle_time = scan_params->min_rest_time;
        scan_params->n_probes = roam_req->nProbes;
        if (roam_req->allowDFSChannelRoam == SIR_ROAMING_DFS_CHANNEL_DISABLED) {
            scan_params->scan_ctrl_flags |= WMI_SCAN_BYPASS_DFS_CHN;
        } else {
            /* Roaming scan on DFS channel is allowed.
             * No need to change any flags for default allowDFSChannelRoam = 1.
             * Special case where static channel list is given by application
             * that contains DFS channels. Assume that the application
             * has knowledge of matching APs being active and that
             * probe request transmission is permitted on those channel.
             * Force active scans on those channels.
             */

            if (roam_req->allowDFSChannelRoam ==
                SIR_ROAMING_DFS_CHANNEL_ENABLED_ACTIVE &&
                roam_req->ChannelCacheType == CHANNEL_LIST_STATIC &&
                roam_req->ConnectedNetwork.ChannelCount > 0) {
                scan_params->scan_ctrl_flags |=
                        WMI_SCAN_FLAG_FORCE_ACTIVE_ON_DFS;
            }
        }
    } else {
        /* roam_req = NULL during initial or pre-assoc invocation */
        scan_params->dwell_time_active = WMA_ROAM_DWELL_TIME_ACTIVE_DEFAULT;
        scan_params->dwell_time_passive = WMA_ROAM_DWELL_TIME_PASSIVE_DEFAULT;
        scan_params->min_rest_time = WMA_ROAM_MIN_REST_TIME_DEFAULT;
        scan_params->max_rest_time = WMA_ROAM_MAX_REST_TIME_DEFAULT;
        scan_params->repeat_probe_time = 0;
        scan_params->probe_spacing_time = 0;
        scan_params->probe_delay = 0;
        scan_params->max_scan_time = WMA_HW_DEF_SCAN_MAX_DURATION;
        scan_params->idle_time = scan_params->min_rest_time;
        scan_params->burst_duration = 0;
        scan_params->n_probes = 0;
    }

    WMA_LOGI("%s: Rome roam scan parameters:"
             " dwell_time_active = %d, dwell_time_passive = %d",
             __func__,
             scan_params->dwell_time_active,
             scan_params->dwell_time_passive);
    WMA_LOGI("%s: min_rest_time = %d, max_rest_time = %d,"
             " repeat_probe_time = %d n_probes = %d",
             __func__,
             scan_params->min_rest_time,
             scan_params->max_rest_time,
             scan_params->repeat_probe_time,
             scan_params->n_probes);
    WMA_LOGI("%s: max_scan_time = %d, idle_time = %d,"
             " burst_duration = %d, scan_ctrl_flags = 0x%x",
             __func__,
             scan_params->max_scan_time,
             scan_params->idle_time,
             scan_params->burst_duration,
             scan_params->scan_ctrl_flags);
}

/* function   : wma_roam_scan_offload_ap_profile
 * Description : Send WMI_ROAM_AP_PROFILE TLV to firmware
 * Args       : AP profile parameters are passed in as the structure used in TLV
 * Returns    :
 */
VOS_STATUS wma_roam_scan_offload_ap_profile(tp_wma_handle wma_handle,
        wmi_ap_profile *ap_profile_p,
        u_int32_t vdev_id)
{
    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
    wmi_buf_t buf = NULL;
    int status = 0;
    int len;
    u_int8_t *buf_ptr;
    wmi_roam_ap_profile_fixed_param *roam_ap_profile_fp;

    len = sizeof(wmi_roam_ap_profile_fixed_param) +
          sizeof(wmi_ap_profile);

    buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
    if (!buf) {
        WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    buf_ptr = (u_int8_t *) wmi_buf_data(buf);
    roam_ap_profile_fp = (wmi_roam_ap_profile_fixed_param *) buf_ptr;
    WMITLV_SET_HDR(&roam_ap_profile_fp->tlv_header,
               WMITLV_TAG_STRUC_wmi_roam_ap_profile_fixed_param,
               WMITLV_GET_STRUCT_TLVLEN(
                   wmi_roam_ap_profile_fixed_param));
    /* fill in threshold values */
    roam_ap_profile_fp->vdev_id = vdev_id;
    roam_ap_profile_fp->id = 0;
    buf_ptr += sizeof(wmi_roam_ap_profile_fixed_param);

    vos_mem_copy(buf_ptr, ap_profile_p, sizeof(wmi_ap_profile));
    WMITLV_SET_HDR(buf_ptr,
               WMITLV_TAG_STRUC_wmi_ap_profile,
               WMITLV_GET_STRUCT_TLVLEN(
                   wmi_ap_profile));
    status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
            len, WMI_ROAM_AP_PROFILE);
    if (status != EOK) {
        WMA_LOGE("wmi_unified_cmd_send WMI_ROAM_AP_PROFILE returned Error %d",
            status);
        vos_status = VOS_STATUS_E_FAILURE;
        goto error;
    }

    WMA_LOGI("WMA --> WMI_ROAM_AP_PROFILE and other parameters");
    return VOS_STATUS_SUCCESS;
error:
    wmi_buf_free(buf);

    return vos_status;
}

VOS_STATUS wma_roam_scan_filter(tp_wma_handle wma_handle,
                tSirRoamOffloadScanReq *roam_req)
{
	wmi_buf_t buf = NULL;
	int status = 0, i;
	uint32_t len, num_bssid_black_list = 0, num_ssid_white_list = 0,
	   num_bssid_preferred_list = 0;
	uint32_t op_bitmap = 0;
	uint8_t *buf_ptr;
	wmi_roam_filter_fixed_param *roam_filter;
	uint8_t *bssid_src_ptr = NULL;
	wmi_mac_addr *bssid_dst_ptr = NULL;
	wmi_ssid *ssid_ptr = NULL;
	uint32_t *bssid_preferred_factor_ptr = NULL;
	struct roam_ext_params *roam_params;

	roam_params = &roam_req->roam_params;
	len = sizeof(wmi_roam_filter_fixed_param);
	len += WMI_TLV_HDR_SIZE;
    if (roam_req->Command != ROAM_SCAN_OFFLOAD_STOP) {
	switch (roam_req->reason) {
	case REASON_ROAM_SET_BLACKLIST_BSSID:
		op_bitmap |= 0x1;
		num_bssid_black_list = roam_params->num_bssid_avoid_list;
		len += num_bssid_black_list * sizeof(wmi_mac_addr);
		len += WMI_TLV_HDR_SIZE;
		break;
	case REASON_ROAM_SET_SSID_ALLOWED:
		op_bitmap |= 0x2;
		num_ssid_white_list = roam_params->num_ssid_allowed_list;
		len += num_ssid_white_list * sizeof(wmi_ssid);
		len += WMI_TLV_HDR_SIZE;
		break;
	case REASON_ROAM_SET_FAVORED_BSSID:
		op_bitmap |= 0x4;
		num_bssid_preferred_list = roam_params->num_bssid_favored;
		len += num_bssid_preferred_list * sizeof(wmi_mac_addr);
		len += WMI_TLV_HDR_SIZE;
		len += num_bssid_preferred_list * sizeof(A_UINT32);
		break;
	default:
		WMA_LOGD("%s : Roam Filter need not be sent", __func__);
		return VOS_STATUS_SUCCESS;
		break;
	}
    } else {
	    /* In case of STOP command, reset all the variables
	     * except for blacklist BSSID which should be retained
	     * across connections.*/
	    op_bitmap = 0x2 | 0x4;
	    num_ssid_white_list = roam_params->num_ssid_allowed_list;
	    len += num_ssid_white_list * sizeof(wmi_ssid);
	    num_bssid_preferred_list = roam_params->num_bssid_favored;
	    len += num_bssid_preferred_list * sizeof(wmi_mac_addr);
	    len += num_bssid_preferred_list * sizeof(A_UINT32);
	    len += (2 * WMI_TLV_HDR_SIZE);
    }

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	roam_filter = (wmi_roam_filter_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&roam_filter->tlv_header,
		WMITLV_TAG_STRUC_wmi_roam_filter_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_roam_filter_fixed_param));
	/* fill in fixed values */
	roam_filter->vdev_id = roam_req->sessionId;
	roam_filter->flags = 0;
	roam_filter->op_bitmap = op_bitmap;
	roam_filter->num_bssid_black_list = num_bssid_black_list;
	roam_filter->num_ssid_white_list = num_ssid_white_list;
	roam_filter->num_bssid_preferred_list = num_bssid_preferred_list;
	buf_ptr += sizeof(wmi_roam_filter_fixed_param);

	WMITLV_SET_HDR((buf_ptr),
		WMITLV_TAG_ARRAY_FIXED_STRUC,
		(num_bssid_black_list * sizeof(wmi_mac_addr)));
	bssid_src_ptr = (uint8_t *)&roam_params->bssid_avoid_list;
	bssid_dst_ptr = (wmi_mac_addr *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for(i=0; i<num_bssid_black_list; i++) {
		WMI_CHAR_ARRAY_TO_MAC_ADDR(bssid_src_ptr, bssid_dst_ptr);
		bssid_src_ptr += ATH_MAC_LEN;
		bssid_dst_ptr++;
	}
	buf_ptr += WMI_TLV_HDR_SIZE + (num_bssid_black_list * sizeof(wmi_mac_addr));
	WMITLV_SET_HDR((buf_ptr),
		WMITLV_TAG_ARRAY_FIXED_STRUC,
		(num_ssid_white_list * sizeof(wmi_ssid)));
	ssid_ptr = (wmi_ssid *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for(i=0; i<num_ssid_white_list; i++) {
		memcpy(&ssid_ptr->ssid, &roam_params->ssid_allowed_list[i].ssId,
			roam_params->ssid_allowed_list[i].length);
		ssid_ptr->ssid_len = roam_params->ssid_allowed_list[i].length;
		WMA_LOGD("%s: Passing SSID of length=%d", __func__,ssid_ptr->ssid_len);
		VOS_TRACE_HEX_DUMP(VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_DEBUG,
				(uint8_t *)ssid_ptr->ssid,
				ssid_ptr->ssid_len);
		ssid_ptr++;
	}
	buf_ptr += WMI_TLV_HDR_SIZE + (num_ssid_white_list * sizeof(wmi_ssid));
	WMITLV_SET_HDR((buf_ptr),
		WMITLV_TAG_ARRAY_FIXED_STRUC,
		(num_bssid_preferred_list * sizeof(wmi_mac_addr)));
	bssid_src_ptr = (uint8_t *)&roam_params->bssid_favored;
	bssid_dst_ptr = (wmi_mac_addr *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for(i=0; i<num_bssid_preferred_list; i++) {
		WMI_CHAR_ARRAY_TO_MAC_ADDR(bssid_src_ptr,
				(wmi_mac_addr *)bssid_dst_ptr);
		bssid_src_ptr += ATH_MAC_LEN;
		bssid_dst_ptr++;
	}
	buf_ptr += WMI_TLV_HDR_SIZE +
		(num_bssid_preferred_list * sizeof(wmi_mac_addr));
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
			(num_bssid_preferred_list * sizeof(uint32_t)));
	bssid_preferred_factor_ptr = (uint32_t *)(buf_ptr + WMI_TLV_HDR_SIZE);
	for (i=0; i<num_bssid_preferred_list; i++) {
		*bssid_preferred_factor_ptr = roam_params->bssid_favored_factor[i];
		bssid_preferred_factor_ptr++;
	}
	buf_ptr += WMI_TLV_HDR_SIZE + (num_bssid_preferred_list * sizeof(uint32_t));
	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
		len, WMI_ROAM_FILTER_CMDID);
	if (status != EOK) {
		WMA_LOGE("wmi_unified_cmd_send WMI_ROAM_FILTER_CMDID returned Error %d",
				status);
		goto error;
	}
	return VOS_STATUS_SUCCESS;
error:
	wmi_buf_free(buf);
	return VOS_STATUS_E_FAILURE;
}

VOS_STATUS wma_roam_scan_offload_command(tp_wma_handle wma_handle,
        u_int32_t command,
        u_int32_t vdev_id)
{
    VOS_STATUS vos_status;
    wmi_roam_scan_cmd_fixed_param *cmd_fp;
    wmi_buf_t buf = NULL;
    int status = 0;
    int len;
    u_int8_t *buf_ptr;

    len = sizeof(wmi_roam_scan_cmd_fixed_param);
    buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
    if (!buf) {
        WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
        return VOS_STATUS_E_NOMEM;
    }

    buf_ptr = (u_int8_t *) wmi_buf_data(buf);

    cmd_fp = (wmi_roam_scan_cmd_fixed_param *) buf_ptr;
    WMITLV_SET_HDR(&cmd_fp->tlv_header,
               WMITLV_TAG_STRUC_wmi_roam_scan_cmd_fixed_param,
               WMITLV_GET_STRUCT_TLVLEN(wmi_roam_scan_cmd_fixed_param));
    cmd_fp->vdev_id = vdev_id;
    cmd_fp->command_arg = command;

    status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
               len, WMI_ROAM_SCAN_CMD);
    if (status != EOK) {
        WMA_LOGE("wmi_unified_cmd_send WMI_ROAM_SCAN_CMD returned Error %d",
            status);
        vos_status = VOS_STATUS_E_FAILURE;
        goto error;
    }

    WMA_LOGI("%s: WMA --> WMI_ROAM_SCAN_CMD", __func__);
    return VOS_STATUS_SUCCESS;

error:
    wmi_buf_free(buf);

    return vos_status;
}

/* function   : wma_process_roam_scan_req
 * Description : Main routine to handle ROAM commands coming from CSR module.
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_process_roam_scan_req(tp_wma_handle wma_handle,
            tSirRoamOffloadScanReq *roam_req)
{
    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
    wmi_start_scan_cmd_fixed_param scan_params;
    wmi_ap_profile ap_profile;
    tpAniSirGlobal pMac = (tpAniSirGlobal)vos_get_context(VOS_MODULE_ID_PE,
                wma_handle->vos_context);
    u_int32_t mode = 0;
    struct wma_txrx_node *intr = NULL;

    WMA_LOGI("%s: command 0x%x, reason %d", __func__, roam_req->Command,
                                            roam_req->reason);

    if (NULL == pMac)
    {
        WMA_LOGE("%s: pMac is NULL", __func__);
        return VOS_STATUS_E_FAILURE;
    }

    if (!wma_handle->roam_offload_enabled) {
	/* roam scan offload is not enabled in firmware.
	 * Cannot initialize it in the middle of connection.
	*/
	vos_mem_free(roam_req);
	return VOS_STATUS_E_PERM;
    }
    switch (roam_req->Command) {
        case ROAM_SCAN_OFFLOAD_START:
            intr = &wma_handle->interfaces[roam_req->sessionId];
            intr->delay_before_vdev_stop = roam_req->delay_before_vdev_stop;
            /*
             * Scan/Roam threshold parameters are translated from fields of tSirRoamOffloadScanReq
             * to WMITLV values sent to Rome firmware.
             * some of these parameters are configurable in qcom_cfg.ini file.
             */

            /* First parameter is positive rssi value to trigger rssi based scan.
             * Opportunistic scan is started at 30 dB higher that trigger rssi.
             */
            wma_handle->suitable_ap_hb_failure = FALSE;

            vos_status = wma_roam_scan_offload_rssi_thresh(wma_handle,
                                                           roam_req);
            if (vos_status != VOS_STATUS_SUCCESS) {
                break;
            }
            vos_status = wma_roam_scan_bmiss_cnt(wma_handle,
                    roam_req->RoamBmissFirstBcnt, roam_req->RoamBmissFinalBcnt,
                    roam_req->sessionId);
            if (vos_status != VOS_STATUS_SUCCESS) {
                break;
            }

            /* Opportunistic scan runs on a timer, value set by EmptyRefreshScanPeriod.
             * Age out the entries after 3 such cycles.
             */
            if (roam_req->EmptyRefreshScanPeriod > 0) {
                vos_status = wma_roam_scan_offload_scan_period(wma_handle,
                        roam_req->EmptyRefreshScanPeriod,
                        roam_req->EmptyRefreshScanPeriod * 3,
                        roam_req->sessionId);
                if (vos_status != VOS_STATUS_SUCCESS) {
                    break;
                }
                mode = WMI_ROAM_SCAN_MODE_PERIODIC;
                /* Don't use rssi triggered roam scans if external app
                 * is in control of channel list.
                 */
                if (roam_req->ChannelCacheType != CHANNEL_LIST_STATIC) {
                    mode |= WMI_ROAM_SCAN_MODE_RSSI_CHANGE;
                }
            } else {
                mode = WMI_ROAM_SCAN_MODE_RSSI_CHANGE;
            }

            /* Start new rssi triggered scan only if it changes by RoamRssiDiff value.
             * Beacon weight of 14 means average rssi is taken over 14 previous samples +
             * 2 times the current beacon's rssi.
             */
            vos_status = wma_roam_scan_offload_rssi_change(wma_handle,
                    roam_req->sessionId,
                    roam_req->RoamRescanRssiDiff,
                    roam_req->RoamBeaconRssiWeight,
                    roam_req->hi_rssi_scan_delay);
            if (vos_status != VOS_STATUS_SUCCESS) {
                break;
            }
            wma_roam_scan_fill_ap_profile(wma_handle, pMac, roam_req, &ap_profile);

            vos_status = wma_roam_scan_offload_ap_profile(wma_handle,
                  &ap_profile,
                  roam_req->sessionId);
            if (vos_status != VOS_STATUS_SUCCESS) {
                break;
            }
            vos_status = wma_roam_scan_offload_chan_list(wma_handle,
                    roam_req->ConnectedNetwork.ChannelCount,
                    &roam_req->ConnectedNetwork.ChannelCache[0],
                    roam_req->ChannelCacheType,
                    roam_req->sessionId);
            if ((vos_status != VOS_STATUS_SUCCESS) &&
                (vos_status != VOS_STATUS_E_EMPTY)) {
                break;
            }


            wma_roam_scan_fill_scan_params(wma_handle, pMac, roam_req, &scan_params);
            vos_status = wma_roam_scan_offload_mode(wma_handle,
                    &scan_params,
                    roam_req,
                    mode,
                    roam_req->sessionId);
            if (vos_status != VOS_STATUS_SUCCESS) {
                break;
            }
            vos_status = wma_roam_scan_filter(wma_handle, roam_req);
            if (vos_status != VOS_STATUS_SUCCESS) {
                WMA_LOGE("Sending start for roam scan filter failed");
                break;
            }
            break;

        case ROAM_SCAN_OFFLOAD_STOP:
            wma_handle->suitable_ap_hb_failure = FALSE;
            if (wma_handle->roam_offload_enabled) {

                wma_roam_scan_fill_scan_params(wma_handle, pMac,
                                               NULL, &scan_params);
                vos_status = wma_roam_scan_offload_mode(wma_handle,
                                                       &scan_params,
                                                       NULL,
                                                       WMI_ROAM_SCAN_MODE_NONE,
                                                       roam_req->sessionId);
            }
            /*
             * If the STOP command is due to a disconnect, then
             * send the filter command to clear all the filter
             * entries. If it is roaming scenario, then do not
             * send the cleared entries.
             */
             if (!roam_req->middle_of_roaming) {
               vos_status = wma_roam_scan_filter(wma_handle, roam_req);
               if (vos_status != VOS_STATUS_SUCCESS) {
                WMA_LOGE("Sending clear for roam scan filter failed");
                 break;
               }
             }

            if (roam_req->reason == REASON_OS_REQUESTED_ROAMING_NOW) {
                vos_msg_t vosMsg;
                tSirRoamOffloadScanRsp *scan_offload_rsp;
                scan_offload_rsp = vos_mem_malloc(sizeof(*scan_offload_rsp));
                if (!scan_offload_rsp) {
                    WMA_LOGE("%s: Alloc failed for scan_offload_rsp", __func__);
                    vos_mem_free(roam_req);
                    return VOS_STATUS_E_NOMEM;
                }
                vosMsg.type = eWNI_SME_ROAM_SCAN_OFFLOAD_RSP;
                scan_offload_rsp->sessionId = roam_req->sessionId;
                scan_offload_rsp->reason = roam_req->reason;
                vosMsg.bodyptr = scan_offload_rsp;
                /*
                 * Since REASSOC request is processed in Roam_Scan_Offload_Rsp
                 * post a dummy rsp msg back to SME with proper reason code.
                 */
                if (VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MQ_ID_SME,
                    (vos_msg_t*)&vosMsg))
                {
                    vos_mem_free(scan_offload_rsp);
                    VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_INFO,
                               "%s: Failed to post Scan Offload Rsp to UMAC",
                               __func__);
                }
            }
            break;

        case ROAM_SCAN_OFFLOAD_ABORT_SCAN:
            /* If roam scan is running, stop that cycle.
             * It will continue automatically on next trigger.
             */
            vos_status = wma_roam_scan_offload_command(wma_handle,
                    WMI_ROAM_SCAN_STOP_CMD,
                    roam_req->sessionId);
            break;

        case ROAM_SCAN_OFFLOAD_RESTART:
            /* Rome offload engine does not stop after any scan.
             * If this command is sent because all preauth attempts failed
             * and WMI_ROAM_REASON_SUITABLE_AP event was received earlier,
             * now it is time to call it heartbeat failure.
             */
            if ((roam_req->reason == REASON_PREAUTH_FAILED_FOR_ALL)
                  && wma_handle->suitable_ap_hb_failure) {
                WMA_LOGE("%s: Sending heartbeat failure after preauth failures",
                           __func__);
                wma_beacon_miss_handler(wma_handle, roam_req->sessionId);
                wma_handle->suitable_ap_hb_failure = FALSE;
            }
            break;

        case ROAM_SCAN_OFFLOAD_UPDATE_CFG:
            wma_handle->suitable_ap_hb_failure = FALSE;
            wma_roam_scan_fill_scan_params(wma_handle, pMac, roam_req, &scan_params);
            vos_status = wma_roam_scan_offload_mode(wma_handle,
                    &scan_params,
                    roam_req,
                    WMI_ROAM_SCAN_MODE_NONE,
                    roam_req->sessionId);
            if (vos_status != VOS_STATUS_SUCCESS) {
                break;
            }

            if (roam_req->RoamScanOffloadEnabled == FALSE) {
                break;
            }

            vos_status = wma_roam_scan_bmiss_cnt(wma_handle,
                    roam_req->RoamBmissFirstBcnt, roam_req->RoamBmissFinalBcnt,
                    roam_req->sessionId);
            if (vos_status != VOS_STATUS_SUCCESS) {
                break;
            }
            vos_status = wma_roam_scan_filter(wma_handle, roam_req);
            if (vos_status != VOS_STATUS_SUCCESS) {
                WMA_LOGE("Sending update for roam scan filter failed");
                break;
            }

            /*
             * Runtime (after association) changes to rssi thresholds and other parameters.
             */
            vos_status = wma_roam_scan_offload_chan_list(wma_handle,
                    roam_req->ConnectedNetwork.ChannelCount,
                    &roam_req->ConnectedNetwork.ChannelCache[0],
                    roam_req->ChannelCacheType,
                    roam_req->sessionId);
            /*
             * Even though the channel list is empty, we can
             * still go ahead and start Roaming.
             */
            if ((vos_status != VOS_STATUS_SUCCESS) &&
                (vos_status != VOS_STATUS_E_EMPTY)) {
                break;
            }

            vos_status = wma_roam_scan_offload_rssi_thresh(wma_handle,
                                                           roam_req);
            if (vos_status != VOS_STATUS_SUCCESS) {
                break;
            }

            if (roam_req->EmptyRefreshScanPeriod > 0) {
                vos_status = wma_roam_scan_offload_scan_period(wma_handle,
                        roam_req->EmptyRefreshScanPeriod,
                        roam_req->EmptyRefreshScanPeriod * 3,
                        roam_req->sessionId);
                if (vos_status != VOS_STATUS_SUCCESS) {
                    break;
                }
                mode = WMI_ROAM_SCAN_MODE_PERIODIC;
                /* Don't use rssi triggered roam scans if external app
                 * is in control of channel list.
                 */
                if (roam_req->ChannelCacheType != CHANNEL_LIST_STATIC) {
                    mode |= WMI_ROAM_SCAN_MODE_RSSI_CHANGE;
                }
            } else {
                mode = WMI_ROAM_SCAN_MODE_RSSI_CHANGE;
            }

            vos_status = wma_roam_scan_offload_rssi_change(wma_handle,
                    roam_req->sessionId,
                    roam_req->RoamRescanRssiDiff,
                    roam_req->RoamBeaconRssiWeight,
                    roam_req->hi_rssi_scan_delay);
            if (vos_status != VOS_STATUS_SUCCESS) {
                break;
            }

            wma_roam_scan_fill_ap_profile(wma_handle, pMac, roam_req, &ap_profile);
            vos_status = wma_roam_scan_offload_ap_profile(wma_handle,
                    &ap_profile,
                    roam_req->sessionId);
            if (vos_status != VOS_STATUS_SUCCESS) {
                break;
            }

            wma_roam_scan_fill_scan_params(wma_handle, pMac, roam_req, &scan_params);
            vos_status = wma_roam_scan_offload_mode(
                    wma_handle,
                    &scan_params,
                    roam_req,
                    mode,
                    roam_req->sessionId);

            break;

        default:
            break;
    }
    vos_mem_free(roam_req);
    return vos_status;
}

#ifdef FEATURE_WLAN_LPHB
/* function   : wma_lphb_conf_hbenable
 * Description : handles the enable command of LPHB configuration requests
 * Args       : wma_handle - WMA handle
 *              lphb_conf_req - configuration info
 *              by_user - whether this call is from user or cached resent
 * Returns    :
 */
VOS_STATUS wma_lphb_conf_hbenable(tp_wma_handle wma_handle,
				tSirLPHBReq *lphb_conf_req,
				v_BOOL_t by_user)

{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	int status = 0;
	tSirLPHBEnableStruct *ts_lphb_enable;
	wmi_buf_t buf = NULL;
	u_int8_t *buf_ptr;
	wmi_hb_set_enable_cmd_fixed_param *hb_enable_fp;
	int len = sizeof(wmi_hb_set_enable_cmd_fixed_param);
	int i;

	if (lphb_conf_req == NULL)
	{
		WMA_LOGE("%s : LPHB configuration is NULL", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	ts_lphb_enable = &(lphb_conf_req->params.lphbEnableReq);
	WMA_LOGI("%s: WMA --> WMI_HB_SET_ENABLE enable=%d, item=%d, session=%d",
		__func__,
		ts_lphb_enable->enable,
		ts_lphb_enable->item,
		ts_lphb_enable->session);

	if ((ts_lphb_enable->item != 1) && (ts_lphb_enable->item != 2)) {
		WMA_LOGE("%s : LPHB configuration wrong item %d",
		__func__,
		ts_lphb_enable->item);
		return VOS_STATUS_E_FAILURE;
        }

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	hb_enable_fp = (wmi_hb_set_enable_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&hb_enable_fp->tlv_header,
               WMITLV_TAG_STRUC_wmi_hb_set_enable_cmd_fixed_param,
               WMITLV_GET_STRUCT_TLVLEN(
			wmi_hb_set_enable_cmd_fixed_param));

	/* fill in values */
	hb_enable_fp->vdev_id = ts_lphb_enable->session;
	hb_enable_fp->enable= ts_lphb_enable->enable;
	hb_enable_fp->item = ts_lphb_enable->item;
	hb_enable_fp->session = ts_lphb_enable->session;

	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
            len, WMI_HB_SET_ENABLE_CMDID);
	if (status != EOK) {
		WMA_LOGE("wmi_unified_cmd_send WMI_HB_SET_ENABLE returned Error %d",
			status);
		vos_status = VOS_STATUS_E_FAILURE;
		goto error;
	}

	if (by_user) {
		/* target already configured, now cache command status */
		if (ts_lphb_enable->enable) {
			i = ts_lphb_enable->item-1;
			wma_handle->wow.lphb_cache[i].cmd
				= LPHB_SET_EN_PARAMS_INDID;
			wma_handle->wow.lphb_cache[i].params.lphbEnableReq.enable
				= ts_lphb_enable->enable;
			wma_handle->wow.lphb_cache[i].params.lphbEnableReq.item
				= ts_lphb_enable->item;
			wma_handle->wow.lphb_cache[i].params.lphbEnableReq.session
				= ts_lphb_enable->session;

			WMA_LOGI("%s: cached LPHB status in WMA context for item %d",
				__func__, i);
		} else {
			vos_mem_zero((void *)&wma_handle->wow.lphb_cache,
				sizeof(wma_handle->wow.lphb_cache));
			WMA_LOGI("%s: cleared all cached LPHB status in WMA context",
				__func__);
		}
	}

	return VOS_STATUS_SUCCESS;
error:
	return vos_status;
}

/* function   : wma_lphb_conf_tcp_params
 * Description : handles the tcp params command of LPHB configuration requests
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_lphb_conf_tcp_params(tp_wma_handle wma_handle,
				tSirLPHBReq *lphb_conf_req)
{
        VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
        int status = 0;
        tSirLPHBTcpParamStruct *ts_lphb_tcp_param;
        wmi_buf_t buf = NULL;
        u_int8_t *buf_ptr;
        wmi_hb_set_tcp_params_cmd_fixed_param *hb_tcp_params_fp;
        int len = sizeof(wmi_hb_set_tcp_params_cmd_fixed_param);

        if (lphb_conf_req == NULL)
        {
                WMA_LOGE("%s : LPHB configuration is NULL", __func__);
                return VOS_STATUS_E_FAILURE;
        }

        ts_lphb_tcp_param = &(lphb_conf_req->params.lphbTcpParamReq);
        WMA_LOGI("%s: WMA --> WMI_HB_SET_TCP_PARAMS srv_ip=%08x, dev_ip=%08x, src_port=%d, "
		"dst_port=%d, timeout=%d, session=%d, gateway_mac=%02x:%02x:%02x:%02x:%02x:%02x, "
		"timePeriodSec=%d, tcpSn=%d",
                __func__,
                ts_lphb_tcp_param->srv_ip, ts_lphb_tcp_param->dev_ip,
                ts_lphb_tcp_param->src_port, ts_lphb_tcp_param->dst_port,
                ts_lphb_tcp_param->timeout, ts_lphb_tcp_param->session,
                ts_lphb_tcp_param->gateway_mac[0],
                ts_lphb_tcp_param->gateway_mac[1],
                ts_lphb_tcp_param->gateway_mac[2],
                ts_lphb_tcp_param->gateway_mac[3],
                ts_lphb_tcp_param->gateway_mac[4],
                ts_lphb_tcp_param->gateway_mac[5],
                ts_lphb_tcp_param->timePeriodSec, ts_lphb_tcp_param->tcpSn);

        buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
        if (!buf) {
                WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
                return VOS_STATUS_E_NOMEM;
        }

        buf_ptr = (u_int8_t *) wmi_buf_data(buf);
        hb_tcp_params_fp = (wmi_hb_set_tcp_params_cmd_fixed_param *) buf_ptr;
        WMITLV_SET_HDR(&hb_tcp_params_fp->tlv_header,
               WMITLV_TAG_STRUC_wmi_hb_set_tcp_params_cmd_fixed_param,
               WMITLV_GET_STRUCT_TLVLEN(
			wmi_hb_set_tcp_params_cmd_fixed_param));

        /* fill in values */
        hb_tcp_params_fp->vdev_id = ts_lphb_tcp_param->session;
        hb_tcp_params_fp->srv_ip = ts_lphb_tcp_param->srv_ip;
        hb_tcp_params_fp->dev_ip = ts_lphb_tcp_param->dev_ip;
        hb_tcp_params_fp->seq = ts_lphb_tcp_param->tcpSn;
        hb_tcp_params_fp->src_port = ts_lphb_tcp_param->src_port;
        hb_tcp_params_fp->dst_port = ts_lphb_tcp_param->dst_port;
        hb_tcp_params_fp->interval = ts_lphb_tcp_param->timePeriodSec;
        hb_tcp_params_fp->timeout = ts_lphb_tcp_param->timeout;
        hb_tcp_params_fp->session = ts_lphb_tcp_param->session;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(ts_lphb_tcp_param->gateway_mac, &hb_tcp_params_fp->gateway_mac);

        status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
            len, WMI_HB_SET_TCP_PARAMS_CMDID);
        if (status != EOK) {
                WMA_LOGE("wmi_unified_cmd_send WMI_HB_SET_TCP_PARAMS returned Error %d",
                        status);
                vos_status = VOS_STATUS_E_FAILURE;
                goto error;
        }

	return VOS_STATUS_SUCCESS;
error:
	return vos_status;
}

/* function   : wma_lphb_conf_tcp_pkt_filter
 * Description : handles the tcp packet filter command of LPHB configuration requests
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_lphb_conf_tcp_pkt_filter(tp_wma_handle wma_handle,
				tSirLPHBReq *lphb_conf_req)
{
        VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
        int status = 0;
        tSirLPHBTcpFilterStruct *ts_lphb_tcp_filter;
        wmi_buf_t buf = NULL;
        u_int8_t *buf_ptr;
        wmi_hb_set_tcp_pkt_filter_cmd_fixed_param *hb_tcp_filter_fp;
        int len = sizeof(wmi_hb_set_tcp_pkt_filter_cmd_fixed_param);

        if (lphb_conf_req == NULL)
        {
                WMA_LOGE("%s : LPHB configuration is NULL", __func__);
                return VOS_STATUS_E_FAILURE;
        }

        ts_lphb_tcp_filter = &(lphb_conf_req->params.lphbTcpFilterReq);
        WMA_LOGI("%s: WMA --> WMI_HB_SET_TCP_PKT_FILTER length=%d, offset=%d, session=%d, "
		"filter=%2x:%2x:%2x:%2x:%2x:%2x ...",
                __func__,
                ts_lphb_tcp_filter->length,
                ts_lphb_tcp_filter->offset,
                ts_lphb_tcp_filter->session,
                ts_lphb_tcp_filter->filter[0],
                ts_lphb_tcp_filter->filter[1],
                ts_lphb_tcp_filter->filter[2],
                ts_lphb_tcp_filter->filter[3],
                ts_lphb_tcp_filter->filter[4],
                ts_lphb_tcp_filter->filter[5]);

        buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
        if (!buf) {
                WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
                return VOS_STATUS_E_NOMEM;
        }

        buf_ptr = (u_int8_t *) wmi_buf_data(buf);
        hb_tcp_filter_fp = (wmi_hb_set_tcp_pkt_filter_cmd_fixed_param *) buf_ptr;
        WMITLV_SET_HDR(&hb_tcp_filter_fp->tlv_header,
               WMITLV_TAG_STRUC_wmi_hb_set_tcp_pkt_filter_cmd_fixed_param,
               WMITLV_GET_STRUCT_TLVLEN(
			wmi_hb_set_tcp_pkt_filter_cmd_fixed_param));

        /* fill in values */
        hb_tcp_filter_fp->vdev_id = ts_lphb_tcp_filter->session;
        hb_tcp_filter_fp->length = ts_lphb_tcp_filter->length;
        hb_tcp_filter_fp->offset = ts_lphb_tcp_filter->offset;
        hb_tcp_filter_fp->session = ts_lphb_tcp_filter->session;
        memcpy((void *) &hb_tcp_filter_fp->filter, (void *) &ts_lphb_tcp_filter->filter,
		WMI_WLAN_HB_MAX_FILTER_SIZE);

        status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
            len, WMI_HB_SET_TCP_PKT_FILTER_CMDID);
        if (status != EOK) {
                WMA_LOGE("wmi_unified_cmd_send WMI_HB_SET_TCP_PKT_FILTER returned Error %d",
                        status);
                vos_status = VOS_STATUS_E_FAILURE;
                goto error;
        }

	return VOS_STATUS_SUCCESS;
error:
	return vos_status;
}

/* function   : wma_lphb_conf_udp_params
 * Description : handles the udp params command of LPHB configuration requests
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_lphb_conf_udp_params(tp_wma_handle wma_handle,
				tSirLPHBReq *lphb_conf_req)
{
        VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
        int status = 0;
        tSirLPHBUdpParamStruct *ts_lphb_udp_param;
        wmi_buf_t buf = NULL;
        u_int8_t *buf_ptr;
        wmi_hb_set_udp_params_cmd_fixed_param *hb_udp_params_fp;
        int len = sizeof(wmi_hb_set_udp_params_cmd_fixed_param);

        if (lphb_conf_req == NULL)
        {
                WMA_LOGE("%s : LPHB configuration is NULL", __func__);
                return VOS_STATUS_E_FAILURE;
        }

        ts_lphb_udp_param = &(lphb_conf_req->params.lphbUdpParamReq);
        WMA_LOGI("%s: WMA --> WMI_HB_SET_UDP_PARAMS srv_ip=%d, dev_ip=%d, src_port=%d, "
		"dst_port=%d, interval=%d, timeout=%d, session=%d, "
		"gateway_mac=%2x:%2x:%2x:%2x:%2x:%2x",
                __func__,
                ts_lphb_udp_param->srv_ip, ts_lphb_udp_param->dev_ip,
                ts_lphb_udp_param->src_port, ts_lphb_udp_param->dst_port,
                ts_lphb_udp_param->interval, ts_lphb_udp_param->timeout, ts_lphb_udp_param->session,
                ts_lphb_udp_param->gateway_mac[0],
                ts_lphb_udp_param->gateway_mac[1],
                ts_lphb_udp_param->gateway_mac[2],
                ts_lphb_udp_param->gateway_mac[3],
                ts_lphb_udp_param->gateway_mac[4],
                ts_lphb_udp_param->gateway_mac[5]);

        buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
        if (!buf) {
                WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
                return VOS_STATUS_E_NOMEM;
        }

        buf_ptr = (u_int8_t *) wmi_buf_data(buf);
        hb_udp_params_fp = (wmi_hb_set_udp_params_cmd_fixed_param *) buf_ptr;
        WMITLV_SET_HDR(&hb_udp_params_fp->tlv_header,
               WMITLV_TAG_STRUC_wmi_hb_set_udp_params_cmd_fixed_param,
               WMITLV_GET_STRUCT_TLVLEN(
			wmi_hb_set_udp_params_cmd_fixed_param));

        /* fill in values */
        hb_udp_params_fp->vdev_id = ts_lphb_udp_param->session;
        hb_udp_params_fp->srv_ip = ts_lphb_udp_param->srv_ip;
        hb_udp_params_fp->dev_ip = ts_lphb_udp_param->dev_ip;
        hb_udp_params_fp->src_port = ts_lphb_udp_param->src_port;
        hb_udp_params_fp->dst_port = ts_lphb_udp_param->dst_port;
        hb_udp_params_fp->interval = ts_lphb_udp_param->interval;
        hb_udp_params_fp->timeout = ts_lphb_udp_param->timeout;
        hb_udp_params_fp->session = ts_lphb_udp_param->session;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(ts_lphb_udp_param->gateway_mac, &hb_udp_params_fp->gateway_mac);

        status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
            len, WMI_HB_SET_UDP_PARAMS_CMDID);
        if (status != EOK) {
                WMA_LOGE("wmi_unified_cmd_send WMI_HB_SET_UDP_PARAMS returned Error %d",
                        status);
                vos_status = VOS_STATUS_E_FAILURE;
                goto error;
        }

	return VOS_STATUS_SUCCESS;
error:
	return vos_status;
}

/* function   : wma_lphb_conf_udp_pkt_filter
 * Description : handles the udp packet filter command of LPHB configuration requests
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_lphb_conf_udp_pkt_filter(tp_wma_handle wma_handle,
				tSirLPHBReq *lphb_conf_req)
{
        VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
        int status = 0;
        tSirLPHBUdpFilterStruct *ts_lphb_udp_filter;
        wmi_buf_t buf = NULL;
        u_int8_t *buf_ptr;
        wmi_hb_set_udp_pkt_filter_cmd_fixed_param *hb_udp_filter_fp;
        int len = sizeof(wmi_hb_set_udp_pkt_filter_cmd_fixed_param);

        if (lphb_conf_req == NULL)
        {
                WMA_LOGE("%s : LPHB configuration is NULL", __func__);
                return VOS_STATUS_E_FAILURE;
        }

        ts_lphb_udp_filter = &(lphb_conf_req->params.lphbUdpFilterReq);
        WMA_LOGI("%s: WMA --> WMI_HB_SET_UDP_PKT_FILTER length=%d, offset=%d, session=%d, "
		"filter=%2x:%2x:%2x:%2x:%2x:%2x ...",
                __func__,
                ts_lphb_udp_filter->length,
                ts_lphb_udp_filter->offset,
                ts_lphb_udp_filter->session,
                ts_lphb_udp_filter->filter[0],
                ts_lphb_udp_filter->filter[1],
                ts_lphb_udp_filter->filter[2],
                ts_lphb_udp_filter->filter[3],
                ts_lphb_udp_filter->filter[4],
                ts_lphb_udp_filter->filter[5]);

        buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
        if (!buf) {
                WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
                return VOS_STATUS_E_NOMEM;
        }

        buf_ptr = (u_int8_t *) wmi_buf_data(buf);
        hb_udp_filter_fp = (wmi_hb_set_udp_pkt_filter_cmd_fixed_param *) buf_ptr;
        WMITLV_SET_HDR(&hb_udp_filter_fp->tlv_header,
               WMITLV_TAG_STRUC_wmi_hb_set_udp_pkt_filter_cmd_fixed_param,
               WMITLV_GET_STRUCT_TLVLEN(
			wmi_hb_set_udp_pkt_filter_cmd_fixed_param));

        /* fill in values */
        hb_udp_filter_fp->vdev_id = ts_lphb_udp_filter->session;
        hb_udp_filter_fp->length = ts_lphb_udp_filter->length;
        hb_udp_filter_fp->offset = ts_lphb_udp_filter->offset;
        hb_udp_filter_fp->session = ts_lphb_udp_filter->session;
        memcpy((void *) &hb_udp_filter_fp->filter, (void *) &ts_lphb_udp_filter->filter,
		WMI_WLAN_HB_MAX_FILTER_SIZE);

        status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
            len, WMI_HB_SET_UDP_PKT_FILTER_CMDID);
        if (status != EOK) {
                WMA_LOGE("wmi_unified_cmd_send WMI_HB_SET_UDP_PKT_FILTER returned Error %d",
                        status);
                vos_status = VOS_STATUS_E_FAILURE;
                goto error;
        }

	return VOS_STATUS_SUCCESS;
error:
	return vos_status;
}

/* function   : wma_process_lphb_conf_req
 * Description : handles LPHB configuration requests
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_process_lphb_conf_req(tp_wma_handle wma_handle,
				tSirLPHBReq *lphb_conf_req)
{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

	if (lphb_conf_req == NULL)
	{
		WMA_LOGE("%s : LPHB configuration is NULL", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGI("%s : LPHB configuration cmd id is %d", __func__,
							lphb_conf_req->cmd);
	switch (lphb_conf_req->cmd) {
	case LPHB_SET_EN_PARAMS_INDID:
		vos_status = wma_lphb_conf_hbenable(wma_handle,
				lphb_conf_req, TRUE);
		break;

	case LPHB_SET_TCP_PARAMS_INDID:
		vos_status = wma_lphb_conf_tcp_params(wma_handle,
				lphb_conf_req);
		break;

	case LPHB_SET_TCP_PKT_FILTER_INDID:
		vos_status = wma_lphb_conf_tcp_pkt_filter(wma_handle,
				lphb_conf_req);
		break;

	case LPHB_SET_UDP_PARAMS_INDID:
		vos_status = wma_lphb_conf_udp_params(wma_handle,
				lphb_conf_req);
		break;

	case LPHB_SET_UDP_PKT_FILTER_INDID:
		vos_status = wma_lphb_conf_udp_pkt_filter(wma_handle,
				lphb_conf_req);
		break;

	case LPHB_SET_NETWORK_INFO_INDID:
	default:
		break;
	}

	vos_mem_free(lphb_conf_req);
	return vos_status;
}
#endif

VOS_STATUS wma_process_dhcp_ind(tp_wma_handle wma_handle,
				tAniDHCPInd *ta_dhcp_ind)
{
	uint8_t vdev_id;
	int status = 0;
	wmi_buf_t buf = NULL;
	u_int8_t *buf_ptr;
	wmi_peer_set_param_cmd_fixed_param *peer_set_param_fp;
	int len = sizeof(wmi_peer_set_param_cmd_fixed_param);

	if (!ta_dhcp_ind)
	{
		WMA_LOGE("%s : DHCP indication is NULL", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	if (!wma_find_vdev_by_addr(wma_handle, ta_dhcp_ind->adapterMacAddr,
		&vdev_id))
	{
		WMA_LOGE("%s: Failed to find vdev id for DHCP indication",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGI("%s: WMA --> WMI_PEER_SET_PARAM triggered by DHCP, "
		"msgType=%s,"
		"device_mode=%d, macAddr=" MAC_ADDRESS_STR,
		__func__,
		ta_dhcp_ind->msgType==WDA_DHCP_START_IND?
			"WDA_DHCP_START_IND":"WDA_DHCP_STOP_IND",
		ta_dhcp_ind->device_mode,
		MAC_ADDR_ARRAY(ta_dhcp_ind->peerMacAddr));

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	peer_set_param_fp = (wmi_peer_set_param_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&peer_set_param_fp->tlv_header,
			WMITLV_TAG_STRUC_wmi_peer_set_param_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
			wmi_peer_set_param_cmd_fixed_param));

	/* fill in values */
	peer_set_param_fp->vdev_id = vdev_id;
	peer_set_param_fp->param_id = WMI_PEER_CRIT_PROTO_HINT_ENABLED;
	if (WDA_DHCP_START_IND == ta_dhcp_ind->msgType)
		peer_set_param_fp->param_value = 1;
	else
		peer_set_param_fp->param_value = 0;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(ta_dhcp_ind->peerMacAddr,
		&peer_set_param_fp->peer_macaddr);

	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
		len, WMI_PEER_SET_PARAM_CMDID);
	if (status != EOK) {
		WMA_LOGE("%s: wmi_unified_cmd_send WMI_PEER_SET_PARAM_CMD"
			" returned Error %d",
			__func__, status);
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;
}

static WLAN_PHY_MODE wma_chan_to_mode(u8 chan, ePhyChanBondState chan_offset,
                                      u8 vht_capable, u8 dot11_mode)
{
	WLAN_PHY_MODE phymode = MODE_UNKNOWN;

	/* 2.4 GHz band */
	if ((chan >= WMA_11G_CHANNEL_BEGIN) && (chan <= WMA_11G_CHANNEL_END)) {
		switch (chan_offset) {
		case PHY_SINGLE_CHANNEL_CENTERED:
                        /* In case of no channel bonding, use dot11_mode
			 * to set phy mode
			 */
			switch (dot11_mode) {
			case WNI_CFG_DOT11_MODE_11A:
				phymode = MODE_11A;
				break;
			case WNI_CFG_DOT11_MODE_11B:
				phymode = MODE_11B;
				break;
			case WNI_CFG_DOT11_MODE_11G:
				phymode = MODE_11G;
				break;
			case WNI_CFG_DOT11_MODE_11G_ONLY:
				phymode = MODE_11GONLY;
				break;
			default:
			/* Configure MODE_11NG_HT20 for
			 * self vdev(for vht too)
			 */
				phymode = MODE_11NG_HT20;
				break;
                        }
			break;
		case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
			phymode = vht_capable ? MODE_11AC_VHT40_2G :MODE_11NG_HT40;
			break;
                case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
                        phymode = MODE_11AC_VHT80_2G;
                        break;

		default:
			break;
		}
	}

	/* 5 GHz band */
	if ((chan >= WMA_11A_CHANNEL_BEGIN) && (chan <= WMA_11A_CHANNEL_END)) {
		switch (chan_offset) {
		case PHY_SINGLE_CHANNEL_CENTERED:
			phymode = vht_capable ? MODE_11AC_VHT20 :MODE_11NA_HT20;
			break;
		case PHY_DOUBLE_CHANNEL_LOW_PRIMARY:
		case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY:
			phymode = vht_capable ? MODE_11AC_VHT40 :MODE_11NA_HT40;
			break;
                case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_CENTERED_40MHZ_CENTERED:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH:
                case PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH:
                        phymode = MODE_11AC_VHT80;
                        break;

		default:
			break;
		}
	}
	WMA_LOGD("%s: phymode %d channel %d offset %d vht_capable %d "
			"dot11_mode %d", __func__, phymode, chan,
			chan_offset, vht_capable, dot11_mode);

	return phymode;
}

tANI_U8 wma_getCenterChannel(tANI_U8 chan, tANI_U8 chan_offset)
{
        tANI_U8 band_center_chan = 0;

        if ((chan_offset == PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_CENTERED) ||
            (chan_offset == PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_LOW))
               band_center_chan = chan + 2;
        else if (chan_offset == PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_LOW)
               band_center_chan = chan + 6;
        else if ((chan_offset == PHY_QUADRUPLE_CHANNEL_20MHZ_LOW_40MHZ_HIGH) ||
              (chan_offset == PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_CENTERED))
               band_center_chan = chan - 2;
        else if (chan_offset == PHY_QUADRUPLE_CHANNEL_20MHZ_HIGH_40MHZ_HIGH)
               band_center_chan = chan - 6;

        return band_center_chan;
}

static VOS_STATUS wma_vdev_start(tp_wma_handle wma,
				 struct wma_vdev_start_req *req, v_BOOL_t isRestart)
{
	wmi_vdev_start_request_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	wmi_channel *chan;
	int32_t len, ret;
	WLAN_PHY_MODE chanmode;
	u_int8_t *buf_ptr;
	struct wma_txrx_node *intr = wma->interfaces;

	WMA_LOGD("%s: Enter isRestart=%d vdev=%d", __func__, isRestart,req->vdev_id);
	len = sizeof(*cmd) + sizeof(wmi_channel) +
	       WMI_TLV_HDR_SIZE;
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_vdev_start_request_cmd_fixed_param *) buf_ptr;
	chan = (wmi_channel *) (buf_ptr + sizeof(*cmd));
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_start_request_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_vdev_start_request_cmd_fixed_param));
	WMITLV_SET_HDR(&chan->tlv_header,
		       WMITLV_TAG_STRUC_wmi_channel,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_channel));
	cmd->vdev_id = req->vdev_id;

	/* Fill channel info */
	chan->mhz = vos_chan_to_freq(req->chan);
	chanmode = wma_chan_to_mode(req->chan, req->chan_offset,
				req->vht_capable, req->dot11_mode);

	intr[cmd->vdev_id].chanmode = chanmode; /* save channel mode */
	intr[cmd->vdev_id].ht_capable = req->ht_capable;
	intr[cmd->vdev_id].vht_capable = req->vht_capable;
	intr[cmd->vdev_id].config.gtx_info.gtxRTMask[0] = CFG_TGT_DEFAULT_GTX_HT_MASK;
	intr[cmd->vdev_id].config.gtx_info.gtxRTMask[1] = CFG_TGT_DEFAULT_GTX_VHT_MASK;
	intr[cmd->vdev_id].config.gtx_info.gtxUsrcfg = CFG_TGT_DEFAULT_GTX_USR_CFG;
	intr[cmd->vdev_id].config.gtx_info.gtxPERThreshold = CFG_TGT_DEFAULT_GTX_PER_THRESHOLD;
	intr[cmd->vdev_id].config.gtx_info.gtxPERMargin = CFG_TGT_DEFAULT_GTX_PER_MARGIN;
	intr[cmd->vdev_id].config.gtx_info.gtxTPCstep = CFG_TGT_DEFAULT_GTX_TPC_STEP;
	intr[cmd->vdev_id].config.gtx_info.gtxTPCMin = CFG_TGT_DEFAULT_GTX_TPC_MIN;
	intr[cmd->vdev_id].config.gtx_info.gtxBWMask = CFG_TGT_DEFAULT_GTX_BW_MASK;
	intr[cmd->vdev_id].mhz = chan->mhz;

	WMI_SET_CHANNEL_MODE(chan, chanmode);
	chan->band_center_freq1 = chan->mhz;

	if (chanmode == MODE_11AC_VHT80)
		chan->band_center_freq1 = vos_chan_to_freq(wma_getCenterChannel
			(req->chan, req->chan_offset));

	if ((chanmode == MODE_11NA_HT40) || (chanmode == MODE_11NG_HT40) ||
			(chanmode == MODE_11AC_VHT40)) {
		if (req->chan_offset == PHY_DOUBLE_CHANNEL_LOW_PRIMARY)
			chan->band_center_freq1 += 10;
		else
			chan->band_center_freq1 -= 10;
	}
	chan->band_center_freq2 = 0;
	/*
	 * If the channel has DFS set, flip on radar reporting.
	 *
	 * It may be that this should only be done for IBSS/hostap operation
	 * as this flag may be interpreted (at some point in the future)
	 * by the firmware as "oh, and please do radar DETECTION."
	 *
	 * If that is ever the case we would insert the decision whether to
	 * enable the firmware flag here.
	 */

   /*
    * If the Channel is DFS,
    * set the WMI_CHAN_FLAG_DFS flag
    */
   if (req->is_dfs) {
		WMI_SET_CHANNEL_FLAG(chan, WMI_CHAN_FLAG_DFS);
		cmd->disable_hw_ack = VOS_TRUE;

		req->dfs_pri_multiplier = wma->dfs_pri_multiplier;

		/*
		 * Configure the current operating channel
		 * to DFS module only if the device operating
		 * mode is AP.
		 * Enable/Disable Phyerr filtering offload
		 * depending on dfs_phyerr_filter_offload
		 * flag status as set in ini for SAP mode.
		 * Currently, only AP supports DFS master
		 * mode operation on DFS channels, P2P-GO
		 * does not support operation on DFS Channels.
		 */
		if (intr[cmd->vdev_id].type == WMI_VDEV_TYPE_AP &&
			 intr[cmd->vdev_id].sub_type == 0) {
			/*
			 * If DFS regulatory domain is invalid,
			 * then, DFS radar filters intialization
			 * will fail. So, do not configure the
			 * channel in to DFS modlue, do not
			 * indicate if phyerror filtering offload
			 * is enabled or not to the firmware, simply
			 * fail the VDEV start on the DFS channel
			 * early on, to protect the DFS module from
			 * processing phyerrors without being intialized.
			 */
			if (DFS_UNINIT_DOMAIN == wma->dfs_ic->current_dfs_regdomain) {
				WMA_LOGE("%s[%d]:DFS Configured with Invalid regdomain"
							" Failed to send VDEV START command",
							__func__, __LINE__);

				adf_nbuf_free(buf);
				return VOS_STATUS_E_FAILURE;
			}

			vos_lock_acquire(&wma->dfs_ic->chan_lock);
			if (wma->dfs_ic->ic_curchan)
			{
				OS_FREE(wma->dfs_ic->ic_curchan);
				wma->dfs_ic->ic_curchan = NULL;
			}

			/* provide the current channel to DFS */
			wma->dfs_ic->ic_curchan =
				wma_dfs_configure_channel(wma->dfs_ic,chan,chanmode,req);
			vos_lock_release(&wma->dfs_ic->chan_lock);

			wma_unified_dfs_phyerr_filter_offload_enable(wma);
		}
	}

   cmd->beacon_interval = req->beacon_intval;
	cmd->dtim_period = req->dtim_period;
	/* FIXME: Find out min, max and regulatory power levels */
	WMI_SET_CHANNEL_REG_POWER(chan, req->max_txpow);
	WMI_SET_CHANNEL_MAX_TX_POWER(chan, req->max_txpow);

	/* TODO: Handle regulatory class, max antenna */
   if (!isRestart) {
	   cmd->beacon_interval = req->beacon_intval;
		cmd->dtim_period = req->dtim_period;

      /* Copy the SSID */
      if (req->ssid.length) {
         if (req->ssid.length < sizeof(cmd->ssid.ssid))
            cmd->ssid.ssid_len = req->ssid.length;
         else
            cmd->ssid.ssid_len = sizeof(cmd->ssid.ssid);
         vos_mem_copy(cmd->ssid.ssid, req->ssid.ssId,
                      cmd->ssid.ssid_len);
      }

      if (req->hidden_ssid)
         cmd->flags |= WMI_UNIFIED_VDEV_START_HIDDEN_SSID;

      if (req->pmf_enabled)
         cmd->flags |= WMI_UNIFIED_VDEV_START_PMF_ENABLED;
   }

	cmd->num_noa_descriptors = 0;
	buf_ptr = (u_int8_t *)(((uintptr_t) cmd) + sizeof(*cmd) +
				sizeof(wmi_channel));
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       cmd->num_noa_descriptors *
		       sizeof(wmi_p2p_noa_descriptor));
	WMA_LOGD("%s: vdev_id %d freq %d channel %d chanmode %d is_dfs %d "
		 "beacon interval %d dtim %d center_chan %d center_freq2 %d "
		 "reg_info_1: 0x%x reg_info_2: 0x%x, req->max_txpow: 0x%x",
		 __func__, req->vdev_id, chan->mhz, req->chan, chanmode, req->is_dfs,
		 req->beacon_intval, cmd->dtim_period, chan->band_center_freq1,
		 chan->band_center_freq2, chan->reg_info_1, chan->reg_info_2,
		 req->max_txpow);

	/* Store vdev params in SAP mode which can be used in vdev restart */
	if (intr[req->vdev_id].type == WMI_VDEV_TYPE_AP &&
		intr[req->vdev_id].sub_type == 0) {
		intr[req->vdev_id].vdev_restart_params.vdev_id = req->vdev_id;
		intr[req->vdev_id].vdev_restart_params.ssid.ssid_len = cmd->ssid.ssid_len;
		vos_mem_copy(intr[req->vdev_id].vdev_restart_params.ssid.ssid, cmd->ssid.ssid,
				cmd->ssid.ssid_len);
		intr[req->vdev_id].vdev_restart_params.flags = cmd->flags;
		intr[req->vdev_id].vdev_restart_params.requestor_id = cmd->requestor_id;
		intr[req->vdev_id].vdev_restart_params.disable_hw_ack = cmd->disable_hw_ack;
		intr[req->vdev_id].vdev_restart_params.chan.mhz = chan->mhz;
		intr[req->vdev_id].vdev_restart_params.chan.band_center_freq1 = chan->band_center_freq1;
		intr[req->vdev_id].vdev_restart_params.chan.band_center_freq2 = chan->band_center_freq1;
		intr[req->vdev_id].vdev_restart_params.chan.info = chan->info;
		intr[req->vdev_id].vdev_restart_params.chan.reg_info_1 = chan->reg_info_1;
		intr[req->vdev_id].vdev_restart_params.chan.reg_info_2 = chan->reg_info_2;
	}

	if (isRestart) {
		/*
		* Marking the VDEV UP STATUS to FALSE
		* since, VDEV RESTART will do a VDEV DOWN
		* in the firmware.
		*/
		intr[cmd->vdev_id].vdev_up = FALSE;

		ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
					WMI_VDEV_RESTART_REQUEST_CMDID);

	} else {
		WMA_LOGD("%s, vdev_id: %d, unpausing tx_ll_queue at VDEV_START",
			 __func__, cmd->vdev_id);
		wdi_in_vdev_unpause(wma->interfaces[cmd->vdev_id].handle,
				    0xffffffff);
		wma->interfaces[cmd->vdev_id].pause_bitmap = 0;
		ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_VDEV_START_REQUEST_CMDID);
	}

	if (ret < 0) {
		WMA_LOGP("%s: Failed to send vdev start command", __func__);
		adf_nbuf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;
}

void wma_vdev_resp_timer(void *data)
{
	tp_wma_handle wma;
	struct wma_target_req *tgt_req = (struct wma_target_req *)data;
	void *vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
	ol_txrx_peer_handle peer;
	ol_txrx_pdev_handle pdev;
	u_int8_t peer_id;
	struct wma_target_req *msg;

	wma = (tp_wma_handle) vos_get_context(VOS_MODULE_ID_WDA, vos_context);

	if (NULL == wma) {
		WMA_LOGE("%s: Failed to get wma", __func__);
		return;
	}

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		vos_timer_stop(&tgt_req->event_timeout);
		goto free_tgt_req;
	}

	WMA_LOGA("%s: request %d is timed out for vdev_id - %d", __func__,
						tgt_req->msg_type, tgt_req->vdev_id);
	msg = wma_find_vdev_req(wma, tgt_req->vdev_id, tgt_req->type);

	if (!msg) {
		WMA_LOGE("%s: Failed to lookup request message - %d",
			 __func__, tgt_req->msg_type);
		return;
	}

	if (tgt_req->msg_type == WDA_CHNL_SWITCH_REQ) {
		tpSwitchChannelParams params =
			(tpSwitchChannelParams)tgt_req->user_data;
		params->status = VOS_STATUS_E_TIMEOUT;
		WMA_LOGA("%s: WDA_SWITCH_CHANNEL_REQ timedout", __func__);
		wma_send_msg(wma, WDA_SWITCH_CHANNEL_RSP, (void *)params, 0);
		wma->roam_preauth_chan_context = NULL;
		adf_os_spin_lock_bh(&wma->roam_preauth_lock);
		wma->roam_preauth_scan_id = -1;
		adf_os_spin_unlock_bh(&wma->roam_preauth_lock);
	} else if (tgt_req->msg_type == WDA_DELETE_BSS_REQ) {
		tpDeleteBssParams params =
			(tpDeleteBssParams)tgt_req->user_data;
		struct beacon_info *bcn;
		struct wma_txrx_node *iface;

		if (tgt_req->vdev_id > wma->max_bssid) {
			WMA_LOGE("%s: Invalid vdev_id %d", __func__,
				tgt_req->vdev_id);
			vos_timer_stop(&tgt_req->event_timeout);
			goto free_tgt_req;
		}

		iface = &wma->interfaces[tgt_req->vdev_id];
		if (iface->handle == NULL) {
			WMA_LOGE("%s vdev id %d is already deleted",
				__func__, tgt_req->vdev_id);
			vos_timer_stop(&tgt_req->event_timeout);
			goto free_tgt_req;
		}

#ifdef QCA_IBSS_SUPPORT
		if (wma_is_vdev_in_ibss_mode(wma, tgt_req->vdev_id))
			wma_delete_all_ibss_peers(wma, tgt_req->vdev_id);
		else
#endif
		{
			if (wma_is_vdev_in_ap_mode(wma, tgt_req->vdev_id))
			{
				wma_delete_all_ap_remote_peers(wma, tgt_req->vdev_id);
			}
			peer = ol_txrx_find_peer_by_addr(pdev, params->bssid,
				&peer_id);
			wma_remove_peer(wma, params->bssid, tgt_req->vdev_id,
				peer, VOS_FALSE);
		}

		if (wmi_unified_vdev_down_send(wma->wmi_handle, tgt_req->vdev_id) < 0) {
			WMA_LOGE("Failed to send vdev down cmd: vdev %d",
				tgt_req->vdev_id);
		} else {
			wma->interfaces[tgt_req->vdev_id].vdev_up = FALSE;
		}
		ol_txrx_vdev_flush(iface->handle);
                WMA_LOGD("%s, vdev_id: %d, un-pausing tx_ll_queue for WDA_DELETE_BSS_REQ timeout",
                         __func__, tgt_req->vdev_id);
		wdi_in_vdev_unpause(iface->handle,
				    OL_TXQ_PAUSE_REASON_VDEV_STOP);
		iface->pause_bitmap &= ~(1 << PAUSE_TYPE_HOST);
		adf_os_atomic_set(&iface->bss_status, WMA_BSS_STATUS_STOPPED);
		WMA_LOGD("%s: (type %d subtype %d) BSS is stopped",
			 __func__, iface->type, iface->sub_type);

		bcn = wma->interfaces[tgt_req->vdev_id].beacon;

		if (bcn) {
			WMA_LOGD("%s: Freeing beacon struct %p, "
				 "template memory %p", __func__,
				 bcn, bcn->buf);
			if (bcn->dma_mapped)
				adf_nbuf_unmap_single(pdev->osdev, bcn->buf,
						      ADF_OS_DMA_TO_DEVICE);
			adf_nbuf_free(bcn->buf);
			vos_mem_free(bcn);
			wma->interfaces[tgt_req->vdev_id].beacon = NULL;
		}

#ifdef QCA_IBSS_SUPPORT
		/* recreate ibss vdev and bss peer for scan purpose */
		if (wma_is_vdev_in_ibss_mode(wma, tgt_req->vdev_id))
			wma_recreate_ibss_vdev_and_bss_peer(wma, tgt_req->vdev_id);
#endif
		params->status = VOS_STATUS_E_TIMEOUT;
		WMA_LOGA("%s: WDA_DELETE_BSS_REQ timedout", __func__);
		wma_send_msg(wma, WDA_DELETE_BSS_RSP, (void *)params, 0);
		if (iface->del_staself_req) {
			WMA_LOGA("scheduling defered deletion(vdev id %x)",
					tgt_req->vdev_id);
			wma_vdev_detach(wma, iface->del_staself_req, 1);
		}
	} else if (tgt_req->msg_type == WDA_DEL_STA_SELF_REQ) {
		struct wma_txrx_node *iface =
			(struct wma_txrx_node *)tgt_req->user_data;
		tpDelStaSelfParams params =
			(tpDelStaSelfParams)iface->del_staself_req;

		params->status = VOS_STATUS_E_TIMEOUT;
		WMA_LOGA("%s: WDA_DEL_STA_SELF_REQ timedout", __func__);
		wma_send_msg(wma, WDA_DEL_STA_SELF_RSP,
			     (void *)iface->del_staself_req, 0);
		if(iface->addBssStaContext)
			adf_os_mem_free(iface->addBssStaContext);
#if defined WLAN_FEATURE_VOWIFI_11R
                if (iface->staKeyParams)
                    adf_os_mem_free(iface->staKeyParams);
#endif
		vos_mem_zero(iface, sizeof(*iface));
	} else if (tgt_req->msg_type == WDA_ADD_BSS_REQ) {
		tpAddBssParams params = (tpAddBssParams)tgt_req->user_data;
		tDeleteBssParams *del_bss_params =
			vos_mem_malloc(sizeof(tDeleteBssParams));
		if (NULL == del_bss_params) {
			WMA_LOGE("Failed to allocate memory for del_bss_params");
			peer = ol_txrx_find_peer_by_addr(pdev, params->bssId,
					&peer_id);
			goto error0;
		}

		del_bss_params->status = params->status =
			eHAL_STATUS_FW_MSG_TIMEDOUT;
		del_bss_params->sessionId = params->sessionId;
		del_bss_params->bssIdx = params->bssIdx;
		vos_mem_copy(del_bss_params->bssid, params->bssId,
			sizeof(tSirMacAddr));

		WMA_LOGA("%s: WDA_ADD_BSS_REQ timedout", __func__);
		peer = ol_txrx_find_peer_by_addr(pdev, params->bssId, &peer_id);
		if (!peer) {
			WMA_LOGP("%s: Failed to find peer %pM", __func__, params->bssId);
		}
		msg = wma_fill_vdev_req(wma, tgt_req->vdev_id, WDA_DELETE_BSS_REQ,
						WMA_TARGET_REQ_TYPE_VDEV_STOP,
						del_bss_params,
						WMA_VDEV_STOP_REQUEST_TIMEOUT);
		if (!msg) {
			WMA_LOGP("%s: Failed to fill vdev request for vdev_id %d",
							__func__, tgt_req->vdev_id);
			goto error0;
		}

		WMA_LOGD("%s, vdev_id: %d, pausing tx_ll_queue for VDEV_STOP (WDA_ADD_BSS_REQ timedout)",
			 __func__, tgt_req->vdev_id);
		wdi_in_vdev_pause(wma->interfaces[tgt_req->vdev_id].handle,
				  OL_TXQ_PAUSE_REASON_VDEV_STOP);
		wma->interfaces[tgt_req->vdev_id].pause_bitmap |=
						(1 << PAUSE_TYPE_HOST);
		if (wmi_unified_vdev_stop_send(wma->wmi_handle, tgt_req->vdev_id)) {
				WMA_LOGP("%s: %d Failed to send vdev stop",	__func__, __LINE__);
				wma_remove_vdev_req(wma, tgt_req->vdev_id,
									WMA_TARGET_REQ_TYPE_VDEV_STOP);
				goto error0;
		}
		WMA_LOGI("%s: bssid %pM vdev_id %d", __func__, params->bssId,
						tgt_req->vdev_id);
		wma_send_msg(wma, WDA_ADD_BSS_RSP, (void *)params, 0);
		goto free_tgt_req;
error0:
		if (peer)
			wma_remove_peer(wma, params->bssId,
					tgt_req->vdev_id, peer,
					VOS_FALSE);
		wma_send_msg(wma, WDA_ADD_BSS_RSP, (void *)params, 0);
	}
free_tgt_req:
	vos_timer_destroy(&tgt_req->event_timeout);
	adf_os_mem_free(tgt_req);
}

static struct wma_target_req *wma_fill_vdev_req(tp_wma_handle wma, u_int8_t vdev_id,
						u_int32_t msg_type, u_int8_t type,
						void *params, u_int32_t timeout)
{
	struct wma_target_req *req;

	req = adf_os_mem_alloc(NULL, sizeof(*req));
	if (!req) {
		WMA_LOGP("%s: Failed to allocate memory for msg %d vdev %d",
			 __func__, msg_type, vdev_id);
		return NULL;
	}

	WMA_LOGD("%s: vdev_id %d msg %d", __func__, vdev_id, msg_type);
	req->vdev_id = vdev_id;
	req->msg_type = msg_type;
	req->type = type;
	req->user_data = params;
	vos_timer_init(&req->event_timeout, VOS_TIMER_TYPE_SW,
		       wma_vdev_resp_timer, req);
	vos_timer_start(&req->event_timeout, timeout);
	adf_os_spin_lock_bh(&wma->vdev_respq_lock);
	list_add_tail(&req->node, &wma->vdev_resp_queue);
	adf_os_spin_unlock_bh(&wma->vdev_respq_lock);
	return req;
}

static void wma_remove_vdev_req(tp_wma_handle wma, u_int8_t vdev_id,
				u_int8_t type)
{
	struct wma_target_req *req_msg;

	req_msg = wma_find_vdev_req(wma, vdev_id, type);
	if (!req_msg)
		return;

	vos_timer_stop(&req_msg->event_timeout);
	vos_timer_destroy(&req_msg->event_timeout);
	adf_os_mem_free(req_msg);
}

/* function   : wma_roam_preauth_chan_set
 * Description: Send a single channel passive scan request
 *              to handle set_channel operation for preauth
 * Args:
 * Returns    :
 */
VOS_STATUS wma_roam_preauth_chan_set(tp_wma_handle wma_handle,
                          tpSwitchChannelParams params, u_int8_t vdev_id)
{
        VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
        tSirScanOffloadReq scan_req;
        u_int8_t bssid[ETH_ALEN] = { 0xff, 0xff, 0xff, 0xff, 0xff, 0xff};

        WMA_LOGI("%s: channel %d", __func__, params->channelNumber);

	/* Check for prior operation in progress */
	if (wma_handle->roam_preauth_chan_context != NULL) {
		vos_status = VOS_STATUS_E_FAILURE;
		WMA_LOGE("%s: Rejected request. Previous operation in progress", __func__);
		goto send_resp;
	}
	wma_handle->roam_preauth_chan_context = params;

        /* Prepare a dummy scan request and get the
         * wmi_start_scan_cmd_fixed_param structure filled properly
         */
        vos_mem_zero(&scan_req, sizeof(scan_req));
        vos_copy_macaddr((v_MACADDR_t *) &scan_req.bssId, (v_MACADDR_t *)bssid);
        vos_copy_macaddr((v_MACADDR_t *)&scan_req.selfMacAddr, (v_MACADDR_t *)&params->selfStaMacAddr);
        scan_req.channelList.numChannels = 1;
        scan_req.channelList.channelNumber[0] = params->channelNumber;
        scan_req.numSsid = 0;
        scan_req.minChannelTime = WMA_ROAM_PREAUTH_SCAN_TIME;
        scan_req.maxChannelTime = WMA_ROAM_PREAUTH_SCAN_TIME;
        scan_req.scanType = eSIR_PASSIVE_SCAN;
        scan_req.p2pScanType = P2P_SCAN_TYPE_LISTEN;
        scan_req.sessionId = vdev_id;
        wma_handle->roam_preauth_chanfreq = vos_chan_to_freq(params->channelNumber);

        /* set the state in advance before calling wma_start_scan and be ready
         * to handle scan events from firmware. Otherwise print statments
         * in wma_start_can create a race condition.
         */
        wma_handle->roam_preauth_scan_state = WMA_ROAM_PREAUTH_CHAN_REQUESTED;
        vos_status = wma_start_scan(wma_handle, &scan_req, WDA_CHNL_SWITCH_REQ);

        if (vos_status == VOS_STATUS_SUCCESS)
		return vos_status;
	wma_handle->roam_preauth_scan_state = WMA_ROAM_PREAUTH_CHAN_NONE;
	/* Failed operation. Safely clear context */
        wma_handle->roam_preauth_chan_context = NULL;

send_resp:
	WMA_LOGI("%s: sending WDA_SWITCH_CHANNEL_RSP, status = 0x%x",
			__func__, vos_status);
	params->chainMask = wma_handle->pdevconfig.txchainmask;
	params->smpsMode = SMPS_MODE_DISABLED;
	params->status = vos_status;
	wma_send_msg(wma_handle, WDA_SWITCH_CHANNEL_RSP, (void *)params, 0);
	return vos_status;
}

VOS_STATUS wma_roam_preauth_chan_cancel(tp_wma_handle wma_handle,
                tpSwitchChannelParams params, u_int8_t vdev_id)
{
        tAbortScanParams abort_scan_req;
        VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

        WMA_LOGI("%s: channel %d", __func__, params->channelNumber);
	/* Check for prior operation in progress */
	if (wma_handle->roam_preauth_chan_context != NULL) {
		vos_status = VOS_STATUS_E_FAILURE;
		WMA_LOGE("%s: Rejected request. Previous operation in progress", __func__);
		goto send_resp;
	}
	wma_handle->roam_preauth_chan_context = params;

        abort_scan_req.SessionId = vdev_id;
        wma_handle->roam_preauth_scan_state = WMA_ROAM_PREAUTH_CHAN_CANCEL_REQUESTED;
        vos_status = wma_stop_scan(wma_handle, &abort_scan_req);
        if (vos_status == VOS_STATUS_SUCCESS)
		return vos_status;
	/* Failed operation. Safely clear context */
        wma_handle->roam_preauth_chan_context = NULL;

send_resp:
	WMA_LOGI("%s: sending WDA_SWITCH_CHANNEL_RSP, status = 0x%x",
			__func__, vos_status);
	params->chainMask = wma_handle->pdevconfig.txchainmask;
	params->smpsMode = SMPS_MODE_DISABLED;
	params->status = vos_status;
	wma_send_msg(wma_handle, WDA_SWITCH_CHANNEL_RSP, (void *)params, 0);
        return vos_status;
}

static void wma_roam_preauth_scan_event_handler(tp_wma_handle wma_handle,
                u_int8_t vdev_id, wmi_scan_event_fixed_param *wmi_event)
{
        VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
        tSwitchChannelParams *params;

        WMA_LOGI("%s: preauth_scan_state %d, event 0x%x, reason 0x%x",
                    __func__, wma_handle->roam_preauth_scan_state,
                    wmi_event->event, wmi_event->reason);
        switch(wma_handle->roam_preauth_scan_state) {
        case WMA_ROAM_PREAUTH_CHAN_REQUESTED:
                if (wmi_event->event & WMI_SCAN_EVENT_FOREIGN_CHANNEL) {
                        /* complete set_chan request */
                        wma_handle->roam_preauth_scan_state = WMA_ROAM_PREAUTH_ON_CHAN;
                        vos_status = VOS_STATUS_SUCCESS;
                } else if (wmi_event->event & WMI_SCAN_FINISH_EVENTS){
                        /* Failed to get preauth channel or finished (unlikely) */
                        wma_handle->roam_preauth_scan_state = WMA_ROAM_PREAUTH_CHAN_NONE;
                        vos_status = VOS_STATUS_E_FAILURE;
                } else
                        return;
                break;
        case WMA_ROAM_PREAUTH_CHAN_CANCEL_REQUESTED:
                /* Completed or cancelled, complete set_chan cancel request */
                wma_handle->roam_preauth_scan_state = WMA_ROAM_PREAUTH_CHAN_NONE;
                break;

        case WMA_ROAM_PREAUTH_ON_CHAN:
                if ((wmi_event->event & WMI_SCAN_EVENT_BSS_CHANNEL) ||
                    (wmi_event->event & WMI_SCAN_FINISH_EVENTS))
                        wma_handle->roam_preauth_scan_state = WMA_ROAM_PREAUTH_CHAN_COMPLETED;

                        /* There is no WDA request to complete. Next set channel request will
                         * look at this state and complete it.
                         */
                break;
        default:
                WMA_LOGE("%s: unhandled event 0x%x, reason 0x%x",
                           __func__, wmi_event->event, wmi_event->reason);
                return;
        }

        if((params = (tpSwitchChannelParams) wma_handle->roam_preauth_chan_context)) {
                WMA_LOGI("%s: sending WDA_SWITCH_CHANNEL_RSP, status = 0x%x",
                          __func__, vos_status);
                params->chainMask = wma_handle->pdevconfig.txchainmask;
                params->smpsMode = SMPS_MODE_DISABLED;
                params->status = vos_status;
                wma_send_msg(wma_handle, WDA_SWITCH_CHANNEL_RSP, (void *)params, 0);
                wma_handle->roam_preauth_chan_context = NULL;
        }

}

void wma_roam_preauth_ind(tp_wma_handle wma_handle, u_int8_t *buf) {
	wmi_scan_event_fixed_param *wmi_event = NULL;
	u_int8_t vdev_id;

	wmi_event = (wmi_scan_event_fixed_param *)buf;
	if (wmi_event == NULL) {
		WMA_LOGE("%s: Invalid param wmi_event is null", __func__);
		return;
	}

	vdev_id = wmi_event->vdev_id;
	if (vdev_id >= wma_handle->max_bssid) {
		WMA_LOGE("%s: Invalid vdev_id %d wmi_event %p", __func__,
			vdev_id, wmi_event);
		return;
	}

	wma_roam_preauth_scan_event_handler(wma_handle, vdev_id, wmi_event);
	return;
}

/*
 * wma_set_channel
 * If this request is called when station is connected, it should use
 */
static void wma_set_channel(tp_wma_handle wma, tpSwitchChannelParams params)
{
	struct wma_vdev_start_req req;
	struct wma_target_req *msg;
        VOS_STATUS status = VOS_STATUS_SUCCESS;
        u_int8_t vdev_id, peer_id;
        ol_txrx_peer_handle peer;
        ol_txrx_pdev_handle pdev;
        struct wma_txrx_node *intr = wma->interfaces;

	WMA_LOGD("%s: Enter", __func__);
        if (!wma_find_vdev_by_addr(wma, params->selfStaMacAddr, &vdev_id)) {
                WMA_LOGP("%s: Failed to find vdev id for %pM",
                         __func__, params->selfStaMacAddr);
                status = VOS_STATUS_E_FAILURE;
                goto send_resp;
        }
        pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);
	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		status = VOS_STATUS_E_FAILURE;
		goto send_resp;
	}

	peer = ol_txrx_find_peer_by_addr(pdev, intr[vdev_id].bssid, &peer_id);

	/*
	 * Roam offload feature is currently supported
	 * only in STA mode. Other modes still require
	 * to issue a Vdev Start/Vdev Restart for
	 * channel change.
	 */
	if (((wma->interfaces[vdev_id].type == WMI_VDEV_TYPE_STA) &&
		(wma->interfaces[vdev_id].sub_type == 0)) &&
			!wma->interfaces[vdev_id].is_channel_switch) {

		if (peer && (peer->state == ol_txrx_peer_state_conn ||
			peer->state == ol_txrx_peer_state_auth)) {
			/* Trying to change channel while connected
			 * should not invoke VDEV_START.
			 * Instead, use start scan command in passive
			 * mode to park station on that channel
			 */
			WMA_LOGI("%s: calling set_scan, state 0x%x",
						__func__, wma->roam_preauth_scan_state);
			if (wma->roam_preauth_scan_state ==
				WMA_ROAM_PREAUTH_CHAN_NONE) {
				/* Is channel change required?
				 */
				if(vos_chan_to_freq(params->channelNumber) !=
					wma->interfaces[vdev_id].mhz)
				{
				    status = wma_roam_preauth_chan_set(wma,
							params, vdev_id);
				/* response will be asynchronous */
				return;
				}
			} else if (wma->roam_preauth_scan_state ==
				WMA_ROAM_PREAUTH_CHAN_REQUESTED ||
				wma->roam_preauth_scan_state == WMA_ROAM_PREAUTH_ON_CHAN) {
				status = wma_roam_preauth_chan_cancel(wma, params, vdev_id);
				/* response will be asynchronous */
				return;
			} else if (wma->roam_preauth_scan_state ==
				WMA_ROAM_PREAUTH_CHAN_COMPLETED) {
				/* Already back on home channel. Complete the request */
				wma->roam_preauth_scan_state = WMA_ROAM_PREAUTH_CHAN_NONE;
				status = VOS_STATUS_SUCCESS;
			}
			goto send_resp;
		}
	}
        vos_mem_zero(&req, sizeof(req));
        req.vdev_id = vdev_id;
	msg = wma_fill_vdev_req(wma, req.vdev_id, WDA_CHNL_SWITCH_REQ,
				WMA_TARGET_REQ_TYPE_VDEV_START, params,
				WMA_VDEV_START_REQUEST_TIMEOUT);
	if (!msg) {
		WMA_LOGP("%s: Failed to fill channel switch request for vdev %d",
			 __func__, req.vdev_id);
		status = VOS_STATUS_E_NOMEM;
		goto send_resp;
	}
	req.chan = params->channelNumber;
	req.chan_offset = params->secondaryChannelOffset;
	req.vht_capable = params->vhtCapable;
	req.dot11_mode = params->dot11_mode;
#ifdef WLAN_FEATURE_VOWIFI
	req.max_txpow = params->maxTxPower;
#else
	req.max_txpow = params->localPowerConstraint;
#endif
	req.beacon_intval = 100;
	req.dtim_period = 1;
   req.is_dfs = params->isDfsChannel;

	/* In case of AP mode, once radar is detected, we need to
	 * issuse VDEV RESTART, so we making is_channel_switch as
	 * TRUE
	 */
	if((wma->interfaces[req.vdev_id].type == WMI_VDEV_TYPE_AP ) &&
			(wma->interfaces[req.vdev_id].sub_type == 0))
		wma->interfaces[req.vdev_id].is_channel_switch = VOS_TRUE;

	status = wma_vdev_start(wma, &req,
			wma->interfaces[req.vdev_id].is_channel_switch);
   if (status != VOS_STATUS_SUCCESS) {
		wma_remove_vdev_req(wma, req.vdev_id, WMA_TARGET_REQ_TYPE_VDEV_START);
		WMA_LOGP("%s: vdev start failed status = %d", __func__, status);
		goto send_resp;
	}

	if (wma->interfaces[req.vdev_id].is_channel_switch)
		wma->interfaces[req.vdev_id].is_channel_switch = VOS_FALSE;
	return;
send_resp:
	WMA_LOGD("%s: channel %d offset %d txpower %d status %d", __func__,
		 params->channelNumber, params->secondaryChannelOffset,
#ifdef WLAN_FEATURE_VOWIFI
		 params->maxTxPower,
#else
		 params->localPowerConstraint,
#endif
		 status);
	params->status = status;
        WMA_LOGI("%s: sending WDA_SWITCH_CHANNEL_RSP, status = 0x%x",
                        __func__, status);
	wma_send_msg(wma, WDA_SWITCH_CHANNEL_RSP, (void *)params, 0);
}

static WLAN_PHY_MODE wma_peer_phymode(tSirNwType nw_type, u_int8_t sta_type,
	u_int8_t is_ht, u_int8_t is_cw40, u_int8_t is_vht, u_int8_t is_cw_vht)
{
	WLAN_PHY_MODE phymode = MODE_UNKNOWN;

	switch (nw_type) {
		case eSIR_11B_NW_TYPE:
			if (is_vht) {
			    if (is_cw_vht)
					phymode = MODE_11AC_VHT80;
			    else
					phymode = (is_cw40) ?
					          MODE_11AC_VHT40 :
					          MODE_11AC_VHT20;
			}
			else if (is_ht) {
				phymode = (is_cw40) ?
				          MODE_11NG_HT40 : MODE_11NG_HT20;
			} else
				phymode = MODE_11B;
			break;
		case eSIR_11G_NW_TYPE:
			if (is_vht) {
				if (is_cw_vht)
					phymode = MODE_11AC_VHT80;
				else
					phymode = (is_cw40) ?
					          MODE_11AC_VHT40 :
					          MODE_11AC_VHT20;
			}
			else if (is_ht) {
				phymode = (is_cw40) ?
				          MODE_11NG_HT40 :
				          MODE_11NG_HT20;
			} else
				phymode = MODE_11G;
			break;
		case eSIR_11A_NW_TYPE:
			if (is_vht) {
				if (is_cw_vht)
					phymode = MODE_11AC_VHT80;
				else
					phymode = (is_cw40) ?
					          MODE_11AC_VHT40 :
					          MODE_11AC_VHT20;
			}
			else if (is_ht) {
				phymode = (is_cw40) ?
				          MODE_11NA_HT40 :
				          MODE_11NA_HT20;
			} else
				phymode = MODE_11A;
			break;
		default:
			WMA_LOGP("%s: Invalid nw type %d", __func__, nw_type);
			break;
	}
	WMA_LOGD("%s: nw_type %d is_ht %d is_cw40 %d is_vht %d is_cw_vht %d\
	         phymode %d", __func__, nw_type, is_ht, is_cw40,
	         is_vht, is_cw_vht, phymode);

	return phymode;
}

static int32_t wmi_unified_send_txbf(tp_wma_handle wma,
					   tpAddStaParams params)
{
    wmi_vdev_txbf_en txbf_en;

    /* This is set when Other partner is Bformer
	and we are capable bformee(enabled both in ini and fw) */
	txbf_en.sutxbfee = params->vhtTxBFCapable;
	txbf_en.mutxbfee = params->vhtTxMUBformeeCapable;
	txbf_en.sutxbfer = 0;
	txbf_en.mutxbfer = 0;

	/* When MU TxBfee is set, SU TxBfee must be set by default */
	if (txbf_en.mutxbfee)
			txbf_en.sutxbfee = txbf_en.mutxbfee;

	WMA_LOGD("txbf_en.sutxbfee %d txbf_en.mutxbfee %d",
			txbf_en.sutxbfee, txbf_en.mutxbfee);

	return(wmi_unified_vdev_set_param_send(wma->wmi_handle,
			params->smesessionId, WMI_VDEV_PARAM_TXBF,
			*((A_UINT8 *)&txbf_en)));
}

static void wma_update_txrx_chainmask(int num_rf_chains, int *cmd_value)
{
	if (*cmd_value > WMA_MAX_RF_CHAINS(num_rf_chains)) {
		WMA_LOGE("%s: Chainmask value exceeds the maximum"
				" supported range setting it to"
				" maximum value. Requested value %d"
				" Updated value %d", __func__, *cmd_value,
				WMA_MAX_RF_CHAINS(num_rf_chains));
		*cmd_value = WMA_MAX_RF_CHAINS(num_rf_chains);
	} else if (*cmd_value < WMA_MIN_RF_CHAINS) {
		WMA_LOGE("%s: Chainmask value is less than the minimum"
				" supported range setting it to"
				" minimum value. Requested value %d"
				" Updated value %d", __func__, *cmd_value,
				WMA_MIN_RF_CHAINS);
		*cmd_value = WMA_MIN_RF_CHAINS;
	}
}

static int32_t wmi_unified_send_peer_assoc(tp_wma_handle wma,
					   tSirNwType nw_type,
					   tpAddStaParams params)
{
	ol_txrx_pdev_handle pdev;
	wmi_peer_assoc_complete_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len;
	int32_t ret, max_rates, i;
	u_int8_t rx_stbc, tx_stbc;
	u_int8_t *rate_pos, *buf_ptr;
	wmi_rate_set peer_legacy_rates, peer_ht_rates;
        wmi_vht_rate_set *mcs;
	u_int32_t num_peer_legacy_rates;
	u_int32_t num_peer_ht_rates;
	u_int32_t num_peer_11b_rates=0;
	u_int32_t num_peer_11a_rates=0;
	u_int32_t phymode;
	u_int32_t peer_nss=1;
	struct wma_txrx_node *intr = NULL;

	if (NULL == params) {
		WMA_LOGE("%s: params is NULL", __func__);
		return -EINVAL;
	}
	intr = &wma->interfaces[params->smesessionId];

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		return -EINVAL;
	}

	vos_mem_zero(&peer_legacy_rates, sizeof(wmi_rate_set));
	vos_mem_zero(&peer_ht_rates, sizeof(wmi_rate_set));

	phymode = wma_peer_phymode(nw_type, params->staType,
	                           params->htCapable,
	                           params->txChannelWidthSet,
	                           params->vhtCapable,
	                           params->vhtTxChannelWidthSet);

	/* Legacy Rateset */
	rate_pos = (u_int8_t *) peer_legacy_rates.rates;
	for (i = 0; i < SIR_NUM_11B_RATES; i++) {
		if (!params->supportedRates.llbRates[i])
			continue;
		rate_pos[peer_legacy_rates.num_rates++] =
			params->supportedRates.llbRates[i];
                num_peer_11b_rates++;
	}
	for (i = 0; i < SIR_NUM_11A_RATES; i++) {
		if (!params->supportedRates.llaRates[i])
			continue;
		rate_pos[peer_legacy_rates.num_rates++] =
			params->supportedRates.llaRates[i];
                num_peer_11a_rates++;
	}

    if ((phymode == MODE_11A && num_peer_11a_rates == 0) ||
        (phymode == MODE_11B && num_peer_11b_rates == 0)) {
	WMA_LOGW("%s: Invalid phy rates. phymode 0x%x, 11b_rates %d, 11a_rates %d",
			__func__, phymode, num_peer_11b_rates, num_peer_11a_rates);
		return -EINVAL;
    }
	/* Set the Legacy Rates to Word Aligned */
	num_peer_legacy_rates = roundup(peer_legacy_rates.num_rates,
					sizeof(u_int32_t));

	/* HT Rateset */
	max_rates = sizeof(peer_ht_rates.rates) /
		    sizeof(peer_ht_rates.rates[0]);
	rate_pos = (u_int8_t *) peer_ht_rates.rates;
	for (i = 0; i < MAX_SUPPORTED_RATES; i++) {
		if (params->supportedRates.supportedMCSSet[i / 8] &
					(1 << (i % 8))) {
			rate_pos[peer_ht_rates.num_rates++] = i;
			if (i >= 8) {
				/* MCS8 or higher rate is present, must be 2x2 */
				peer_nss = 2;
			}
		}
		if (peer_ht_rates.num_rates == max_rates)
		       break;
	}

	if (params->htCapable && !peer_ht_rates.num_rates) {
		u_int8_t temp_ni_rates[8] = {0x0, 0x1, 0x2, 0x3,
					     0x4, 0x5, 0x6, 0x7};
		/*
		 * Workaround for EV 116382: The peer is marked HT but with
		 * supported rx mcs set is set to 0. 11n spec mandates MCS0-7
		 * for a HT STA. So forcing the supported rx mcs rate to
		 * MCS 0-7. This workaround will be removed once we get
		 * clarification from WFA regarding this STA behavior.
		 */

		/* TODO: Do we really need this? */
		WMA_LOGW("Peer is marked as HT capable but supported mcs rate is 0");
		peer_ht_rates.num_rates = sizeof(temp_ni_rates);
		vos_mem_copy((u_int8_t *) peer_ht_rates.rates, temp_ni_rates,
			     peer_ht_rates.num_rates);
	}

	/* Set the Peer HT Rates to Word Aligned */
	num_peer_ht_rates = roundup(peer_ht_rates.num_rates,
					sizeof(u_int32_t));

	len = sizeof(*cmd) +
		WMI_TLV_HDR_SIZE + /* Place holder for peer legacy rate array */
		(num_peer_legacy_rates * sizeof(u_int8_t)) + /* peer legacy rate array size */
		WMI_TLV_HDR_SIZE + /* Place holder for peer Ht rate array */
		(num_peer_ht_rates * sizeof(u_int8_t)) + /* peer HT rate array size */
		sizeof(wmi_vht_rate_set);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_peer_assoc_complete_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_peer_assoc_complete_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_peer_assoc_complete_cmd_fixed_param));

	/* in ap/ibss mode and for tdls peer, use mac address of the peer in
	 * the other end as the new peer address; in sta mode, use bss id to
	 * be the new peer address
	 */
	if ((wma_is_vdev_in_ap_mode(wma, params->smesessionId))
#ifdef QCA_IBSS_SUPPORT
		|| (wma_is_vdev_in_ibss_mode(wma, params->smesessionId))
#endif
#ifdef FEATURE_WLAN_TDLS
		|| (STA_ENTRY_TDLS_PEER == params->staType)
#endif
	)
		WMI_CHAR_ARRAY_TO_MAC_ADDR(params->staMac, &cmd->peer_macaddr);
	else
		WMI_CHAR_ARRAY_TO_MAC_ADDR(params->bssId, &cmd->peer_macaddr);
	cmd->vdev_id = params->smesessionId;
	cmd->peer_new_assoc = 1;
	cmd->peer_associd = params->assocId;

	/*
	 * The target only needs a subset of the flags maintained in the host.
	 * Just populate those flags and send it down
	 */
	cmd->peer_flags = 0;

	if (params->wmmEnabled)
		cmd->peer_flags |= WMI_PEER_QOS;

	if (params->uAPSD) {
		cmd->peer_flags |= WMI_PEER_APSD;
		WMA_LOGD("Set WMI_PEER_APSD: uapsd Mask %d", params->uAPSD);
	}

	if (params->htCapable) {
		cmd->peer_flags |= (WMI_PEER_HT | WMI_PEER_QOS);
		cmd->peer_rate_caps |= WMI_RC_HT_FLAG;
	}

	if (params->txChannelWidthSet) {
		cmd->peer_flags |= WMI_PEER_40MHZ;
		cmd->peer_rate_caps |= WMI_RC_CW40_FLAG;
		if (params->fShortGI40Mhz)
			cmd->peer_rate_caps |= WMI_RC_SGI_FLAG;
	} else if (params->fShortGI20Mhz)
		cmd->peer_rate_caps |= WMI_RC_SGI_FLAG;

#ifdef WLAN_FEATURE_11AC
	if (params->vhtCapable) {
		cmd->peer_flags |= (WMI_PEER_HT | WMI_PEER_VHT | WMI_PEER_QOS);
		cmd->peer_rate_caps |= WMI_RC_HT_FLAG;
	}

	if (params->vhtTxChannelWidthSet)
		cmd->peer_flags |= WMI_PEER_80MHZ;

	cmd->peer_vht_caps = params->vht_caps;
#endif

	if (params->rmfEnabled)
		cmd->peer_flags |= WMI_PEER_PMF;

	rx_stbc = (params->ht_caps & IEEE80211_HTCAP_C_RXSTBC) >>
			IEEE80211_HTCAP_C_RXSTBC_S;
	if (rx_stbc) {
		cmd->peer_flags |= WMI_PEER_STBC;
		cmd->peer_rate_caps |= (rx_stbc << WMI_RC_RX_STBC_FLAG_S);
	}

        tx_stbc = (params->ht_caps & IEEE80211_HTCAP_C_TXSTBC) >>
                        IEEE80211_HTCAP_C_TXSTBC_S;
        if (tx_stbc) {
                cmd->peer_flags |= WMI_PEER_STBC;
                cmd->peer_rate_caps |= (tx_stbc << WMI_RC_TX_STBC_FLAG_S);
        }

	if (params->htLdpcCapable || params->vhtLdpcCapable)
		cmd->peer_flags |= WMI_PEER_LDPC;

	switch (params->mimoPS) {
		case eSIR_HT_MIMO_PS_STATIC:
			cmd->peer_flags |= WMI_PEER_STATIC_MIMOPS;
			break;
		case eSIR_HT_MIMO_PS_DYNAMIC:
			cmd->peer_flags |= WMI_PEER_DYN_MIMOPS;
			break;
		case eSIR_HT_MIMO_PS_NO_LIMIT:
			cmd->peer_flags |= WMI_PEER_SPATIAL_MUX;
			break;
		default:
			break;
	}

#ifdef FEATURE_WLAN_TDLS
	if (STA_ENTRY_TDLS_PEER == params->staType)
		cmd->peer_flags |= WMI_PEER_AUTH;
#endif

	if (params->wpa_rsn
#ifdef FEATURE_WLAN_WAPI
	    || params->encryptType == eSIR_ED_WPI
#endif
	   )
		cmd->peer_flags |= WMI_PEER_NEED_PTK_4_WAY;
	if (params->wpa_rsn >> 1)
		cmd->peer_flags |= WMI_PEER_NEED_GTK_2_WAY;

        #if defined(CONFIG_HL_SUPPORT) && defined(FEATURE_WLAN_TDLS)
        if (STA_ENTRY_TDLS_PEER == params->staType)
            ol_txrx_peer_state_update(pdev, params->staMac, ol_txrx_peer_state_auth);
        else
	    ol_txrx_peer_state_update(pdev, params->bssId, ol_txrx_peer_state_auth);
        #else
	ol_txrx_peer_state_update(pdev, params->bssId, ol_txrx_peer_state_auth);
        #endif

#ifdef FEATURE_WLAN_WAPI
	if (params->encryptType == eSIR_ED_WPI) {
		ret = wmi_unified_vdev_set_param_send(wma->wmi_handle,
						params->smesessionId,
						WMI_VDEV_PARAM_DROP_UNENCRY,
						FALSE);
		if (ret) {
			WMA_LOGE("Set WMI_VDEV_PARAM_DROP_UNENCRY Param status:%d\n", ret);
			adf_nbuf_free(buf);
			return ret;
		}
	}
#endif

	cmd->peer_caps = params->capab_info;
	cmd->peer_listen_intval = params->listenInterval;
	cmd->peer_ht_caps = params->ht_caps;
	cmd->peer_max_mpdu = (1 << (IEEE80211_HTCAP_MAXRXAMPDU_FACTOR +
				    params->maxAmpduSize)) - 1;
	cmd->peer_mpdu_density = wma_parse_mpdudensity(params->maxAmpduDensity);

	if (params->supportedRates.supportedMCSSet[1] &&
	    params->supportedRates.supportedMCSSet[2])
		cmd->peer_rate_caps |= WMI_RC_TS_FLAG;
	else if (params->supportedRates.supportedMCSSet[1])
		cmd->peer_rate_caps |= WMI_RC_DS_FLAG;

	/* Update peer legacy rate information */
	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		       num_peer_legacy_rates);
	buf_ptr += WMI_TLV_HDR_SIZE;
	cmd->num_peer_legacy_rates = peer_legacy_rates.num_rates;
	vos_mem_copy(buf_ptr, peer_legacy_rates.rates,
		     peer_legacy_rates.num_rates);

	/* Update peer HT rate information */
	buf_ptr += num_peer_legacy_rates;
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		       num_peer_ht_rates);
	buf_ptr += WMI_TLV_HDR_SIZE;
	cmd->num_peer_ht_rates = peer_ht_rates.num_rates;
	vos_mem_copy(buf_ptr, peer_ht_rates.rates,
		     peer_ht_rates.num_rates);

	/* VHT Rates */
	buf_ptr += num_peer_ht_rates;
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_STRUC_wmi_vht_rate_set,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_vht_rate_set));

	cmd->peer_nss = peer_nss;

	WMA_LOGD("peer_nss %d peer_ht_rates.num_rates %d ", cmd->peer_nss,
                  peer_ht_rates.num_rates);

        mcs = (wmi_vht_rate_set *)buf_ptr;
        if ( params->vhtCapable) {
#define VHT2x2MCSMASK 0xc
                mcs->rx_max_rate = params->supportedRates.vhtRxHighestDataRate;
                mcs->rx_mcs_set  = params->supportedRates.vhtRxMCSMap;
                mcs->tx_max_rate = params->supportedRates.vhtTxHighestDataRate;
                mcs->tx_mcs_set  = params->supportedRates.vhtTxMCSMap;

                if(params->vhtSupportedRxNss) {
                    cmd->peer_nss = params->vhtSupportedRxNss;
                } else {
                    cmd->peer_nss = ((mcs->rx_mcs_set & VHT2x2MCSMASK)
                                       == VHT2x2MCSMASK) ? 1 : 2;
                }
	}

	/*
	 * Limit nss to max number of rf chain supported by target
	 * Otherwise Fw will crash
	 */
	wma_update_txrx_chainmask(wma->num_rf_chains, &cmd->peer_nss);

	intr->nss = cmd->peer_nss;
        cmd->peer_phymode = phymode;

        WMA_LOGD("%s: vdev_id %d associd %d peer_flags %x rate_caps %x "
                 "peer_caps %x listen_intval %d ht_caps %x max_mpdu %d "
                 "nss %d phymode %d peer_mpdu_density %d"
                 "cmd->peer_vht_caps %x", __func__,
                 cmd->vdev_id, cmd->peer_associd, cmd->peer_flags,
                 cmd->peer_rate_caps, cmd->peer_caps,
                 cmd->peer_listen_intval, cmd->peer_ht_caps,
                 cmd->peer_max_mpdu, cmd->peer_nss, cmd->peer_phymode,
                 cmd->peer_mpdu_density, cmd->peer_vht_caps);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_PEER_ASSOC_CMDID);
	if (ret != EOK) {
		WMA_LOGP("%s: Failed to send peer assoc command ret = %d",
				__func__, ret);
		adf_nbuf_free(buf);
	}
	return ret;
}

static int
wmi_unified_modem_power_state(wmi_unified_t wmi_handle, u_int32_t param_value)
{
	int ret;
	wmi_modem_power_state_cmd_param *cmd;
	wmi_buf_t buf;
	u_int16_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}
	cmd = (wmi_modem_power_state_cmd_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_modem_power_state_cmd_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_modem_power_state_cmd_param));
	cmd->modem_power_state = param_value;
	WMA_LOGD("%s: Setting cmd->modem_power_state = %u", __func__, param_value);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
				   WMI_MODEM_POWER_STATE_CMDID);
	if (ret != EOK) {
		WMA_LOGE("Failed to send notify cmd ret = %d", ret);
		wmi_buf_free(buf);
	}
	return ret;
}

VOS_STATUS wma_get_link_speed(WMA_HANDLE handle,
				tSirLinkSpeedInfo *pLinkSpeed)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	wmi_peer_get_estimated_linkspeed_cmd_fixed_param* cmd;
	wmi_buf_t wmi_buf;
	uint32_t   len;
	u_int8_t *buf_ptr;

	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue get link speed cmd",
                        __func__);
		return VOS_STATUS_E_INVAL;
	}
	if (!WMI_SERVICE_IS_ENABLED(wma_handle->wmi_service_bitmap,
				WMI_SERVICE_ESTIMATE_LINKSPEED)) {
		WMA_LOGE("%s: Linkspeed feature bit not enabled"
			 " Sending value 0 as link speed.",
			__func__);
		wma_send_link_speed(0);
		return VOS_STATUS_E_FAILURE;
	}
	len  = sizeof(wmi_peer_get_estimated_linkspeed_cmd_fixed_param);
	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_buf);

	cmd = (wmi_peer_get_estimated_linkspeed_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_peer_get_estimated_linkspeed_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_peer_get_estimated_linkspeed_cmd_fixed_param));

	/* Copy the peer macaddress to the wma buffer */
	WMI_CHAR_ARRAY_TO_MAC_ADDR(pLinkSpeed->peer_macaddr, &cmd->peer_macaddr);

	WMA_LOGD("%s: pLinkSpeed->peerMacAddr: %pM, "
			"peer_macaddr.mac_addr31to0: 0x%x, peer_macaddr.mac_addr47to32: 0x%x",
			__func__, pLinkSpeed->peer_macaddr,
			cmd->peer_macaddr.mac_addr31to0,
			cmd->peer_macaddr.mac_addr47to32);

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, len,
		WMI_PEER_GET_ESTIMATED_LINKSPEED_CMDID)) {
		WMA_LOGE("%s: failed to send link speed command", __func__);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}


static int
wmi_unified_pdev_set_param(wmi_unified_t wmi_handle, WMI_PDEV_PARAM param_id,
				u_int32_t param_value)
{
	int ret;
	wmi_pdev_set_param_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	u_int16_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}
	cmd = (wmi_pdev_set_param_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_set_param_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_pdev_set_param_cmd_fixed_param));
	cmd->reserved0 = 0;
	cmd->param_id = param_id;
	cmd->param_value = param_value;
	WMA_LOGD("Setting pdev param = %x, value = %u",
			param_id, param_value);
	ret = wmi_unified_cmd_send(wmi_handle, buf, len,
					WMI_PDEV_SET_PARAM_CMDID);
	if (ret != EOK) {
		WMA_LOGE("Failed to send set param command ret = %d", ret);
		wmi_buf_free(buf);
	}
	return ret;
}

static int32_t wma_txrx_fw_stats_reset(tp_wma_handle wma_handle,
					uint8_t vdev_id, u_int32_t value)
{
	struct ol_txrx_stats_req req;
	ol_txrx_vdev_handle vdev;

	vdev = wma_find_vdev_by_id(wma_handle, vdev_id);
	if (!vdev) {
		WMA_LOGE("%s:Invalid vdev handle", __func__);
		return -EINVAL;
	}
	vos_mem_zero(&req, sizeof(req));
	req.stats_type_reset_mask = value;
	ol_txrx_fw_stats_get(vdev, &req);

	return 0;
}

static int32_t wma_set_txrx_fw_stats_level(tp_wma_handle wma_handle,
					   uint8_t vdev_id, u_int32_t value)
{
	struct ol_txrx_stats_req req;
	ol_txrx_vdev_handle vdev;

	vdev = wma_find_vdev_by_id(wma_handle, vdev_id);
	if (!vdev) {
		WMA_LOGE("%s:Invalid vdev handle", __func__);
		return -EINVAL;
	}
	vos_mem_zero(&req, sizeof(req));
	req.print.verbose = 1;
	if (value <= WMA_FW_TX_PPDU_STATS)
		req.stats_type_upload_mask = 1 << (value - 1);
	else if (value == WMA_FW_TX_CONCISE_STATS) {
		/*
		 * Stats request 5 is the same as stats request 4,
		 * but with only a concise printout.
		 */
		req.print.concise = 1;
		req.stats_type_upload_mask = 1 << (WMA_FW_TX_PPDU_STATS - 1);
	} else if (value == WMA_FW_TX_RC_STATS) {
		req.stats_type_upload_mask = 1 << (WMA_FW_TX_CONCISE_STATS - 1);
    /*
     * This part of the code is a bit confusing.
     * For the statistics command iwpriv wlan0 txrx_fw_stats <n>,
     * for all n <= 4, there is 1:1 mapping of WMA defined value (n)
     * with f/w required stats_type_upload_mask.
     * For n <= 4, stats_type_upload_mask = 1 << (n - 1)
     * With the introduction of WMA_FW_TX_CONCISE_STATS, this changed
     * & the code has a special case handling, where for n = 5,
     * stats_type_upload_mask = 1 << (n - 2).
     * However per current code, there is no way to set the value of
     * stats_type_upload_mask for n > 5.
     * In the mean-time for dumping Remote Ring Buffer information,
     * f/w expects stats_type_upload_mask = 1 << 12.
     * However going by CLI command arguments, n should be 7.
     * There seems to be no apparent correlation between 7 & 12.
     * To fix this properly, one needs to fix the WMA defines appropriately
     * and always let n have a 1:1 correspondence with the bitmask expected by f/w.
     * Do not want to disturb the existing code now, but extending this code,
     * so that CLI argument "n" has 1:1 correpondence with f/w bitmask.
     * With this approach, for remote ring information, the statistics
     * command should be:
     * iwpriv wlan0 txrx_fw_stats 12
     */
    /* FIXME : Fix all the values in the appropriate way. */
    } else if (value == WMA_FW_RX_REM_RING_BUF) {
		req.stats_type_upload_mask = 1 << WMA_FW_RX_REM_RING_BUF;
    }

	ol_txrx_fw_stats_get(vdev, &req);

	return 0;
}

static int32_t wma_set_priv_cfg(tp_wma_handle wma_handle,
				wda_cli_set_cmd_t *privcmd)
{
	int32_t ret = 0;

	switch (privcmd->param_id) {
	case WMA_VDEV_TXRX_FWSTATS_ENABLE_CMDID:
		ret = wma_set_txrx_fw_stats_level(wma_handle,
						  privcmd->param_vdev_id,
						  privcmd->param_value);
		break;
	case WMA_VDEV_TXRX_FWSTATS_RESET_CMDID:
		ret = wma_txrx_fw_stats_reset(wma_handle,
						privcmd->param_vdev_id,
						privcmd->param_value);
		break;
        case WMI_STA_SMPS_FORCE_MODE_CMDID:
                  wma_set_mimops(wma_handle, privcmd->param_vdev_id,
                                 privcmd->param_value);
                break;
        case WMI_STA_SMPS_PARAM_CMDID:
                  wma_set_smps_params(wma_handle, privcmd->param_vdev_id,
                                 privcmd->param_value);
                break;
	case WMA_VDEV_MCC_SET_TIME_LATENCY:
	{
		/* Extract first MCC adapter/vdev channel number and latency */
		tANI_U8 mcc_channel         = privcmd->param_value & 0x000000FF;
		tANI_U8 mcc_channel_latency =
				(privcmd->param_value & 0x0000FF00) >> 8;
		int ret = -1;
		WMA_LOGD("%s: Parsed input: Channel #1:%d, latency:%dms",
			__func__, mcc_channel, mcc_channel_latency);
		ret = wma_set_mcc_channel_time_latency
						(
						wma_handle,
						mcc_channel,
						mcc_channel_latency
						);
	}
		break;
	case WMA_VDEV_MCC_SET_TIME_QUOTA:
	{
		/** Extract the MCC 2 adapters/vdevs channel numbers and time
		  *  quota value for the first adapter only (which is specified
		  *  in iwpriv command.
		  */
		tANI_U8 adapter_2_chan_number =
					privcmd->param_value & 0x000000FF;
		tANI_U8 adapter_1_chan_number =
				(privcmd->param_value & 0x0000FF00) >> 8;
		tANI_U8 adapter_1_quota =
				(privcmd->param_value & 0x00FF0000) >> 16;
		int ret = -1;

		WMA_LOGD("%s: Parsed input: Channel #1:%d, Channel #2:%d,"
			"quota 1:%dms", __func__, adapter_1_chan_number,
			adapter_2_chan_number, adapter_1_quota);
		ret = wma_set_mcc_channel_time_quota
						(
						wma_handle,
						adapter_1_chan_number,
						adapter_1_quota,
						adapter_2_chan_number
						);
	}
		break;
	case WMA_VDEV_IBSS_SET_ATIM_WINDOW_SIZE:
	{
		wma_handle->wma_ibss_power_save_params.atimWindowLength =
					privcmd->param_value;
		WMA_LOGD("%s: IBSS power save ATIM Window = %d", __func__,
			wma_handle->wma_ibss_power_save_params.atimWindowLength);
	}
		break;
	case WMA_VDEV_IBSS_SET_POWER_SAVE_ALLOWED:
	{
		wma_handle->wma_ibss_power_save_params.isPowerSaveAllowed =
					privcmd->param_value;
		WMA_LOGD("%s: IBSS is Power Save Allowed = %d", __func__,
			wma_handle->wma_ibss_power_save_params.isPowerSaveAllowed);
	}
		break;
	case WMA_VDEV_IBSS_SET_POWER_COLLAPSE_ALLOWED:
	{
		wma_handle->wma_ibss_power_save_params.isPowerCollapseAllowed =
					privcmd->param_value;
		WMA_LOGD("%s: IBSS is Power Collapse Allowed = %d", __func__,
			wma_handle->wma_ibss_power_save_params.isPowerCollapseAllowed);
	}
		break;
	case WMA_VDEV_IBSS_SET_AWAKE_ON_TX_RX:
	{
		wma_handle->wma_ibss_power_save_params.isAwakeonTxRxEnabled =
					privcmd->param_value;
		WMA_LOGD("%s: IBSS Power Save Awake on Tx/Rx Enabled = %d", __func__,
			wma_handle->wma_ibss_power_save_params.isAwakeonTxRxEnabled);
	}
		break;
	case WMA_VDEV_IBSS_SET_INACTIVITY_TIME:
	{
		wma_handle->wma_ibss_power_save_params.inactivityCount =
					privcmd->param_value;
		WMA_LOGD("%s: IBSS Power Save Data Inactivity Count = %d", __func__,
			wma_handle->wma_ibss_power_save_params.inactivityCount);
	}
		break;
	case WMA_VDEV_IBSS_SET_TXSP_END_INACTIVITY_TIME:
	{
		wma_handle->wma_ibss_power_save_params.txSPEndInactivityTime =
					privcmd->param_value;
		WMA_LOGD("%s: IBSS Power Save Transmit EOSP inactivity time out = %d",
			__func__,
			wma_handle->wma_ibss_power_save_params.txSPEndInactivityTime);
	}
		break;
	case WMA_VDEV_DFS_CONTROL_CMDID:
	{
		struct ieee80211com *dfs_ic = wma_handle->dfs_ic;
		struct ath_dfs *dfs;

		if (!dfs_ic) {
			ret = -ENOENT;
		} else {
			if (dfs_ic->ic_curchan) {
				WMA_LOGD("%s: Debug cmd: %s received on ch: %d",
						__func__,
						"WMA_VDEV_DFS_CONTROL_CMDID",
						dfs_ic->ic_curchan->ic_ieee);

				if (dfs_ic->ic_curchan->ic_flagext &
						IEEE80211_CHAN_DFS) {
					dfs = (struct ath_dfs *)dfs_ic->ic_dfs;
					dfs->dfs_bangradar = 1;
					dfs->ath_radar_tasksched = 1;
					OS_SET_TIMER(&dfs->ath_dfs_task_timer,
						0);
				} else {
					ret = -ENOENT;
				}
			} else {
				ret = -ENOENT;
			}
		}

		if ( ret == -ENOENT) {
			WMA_LOGE("%s: Operating channel is not DFS capable, "
					"ignoring %s",
					__func__,
					"WMA_VDEV_DFS_CONTROL_CMDID");
		} else if (ret) {
			WMA_LOGE("%s: Sending command %s failed with %d\n",
					__func__,
					"WMA_VDEV_DFS_CONTROL_CMDID",
					ret);
		}
	}
		break;
	case WMA_VDEV_IBSS_PS_SET_WARMUP_TIME_SECS:
	{
		wma_handle->wma_ibss_power_save_params.ibssPsWarmupTime =
					privcmd->param_value;
		WMA_LOGD("%s: IBSS Power Save Warm Up Time in Seconds = %d",
			__func__,
			wma_handle->wma_ibss_power_save_params.ibssPsWarmupTime);
	}
		break;
	case WMA_VDEV_IBSS_PS_SET_1RX_CHAIN_IN_ATIM_WINDOW:
	{
		wma_handle->wma_ibss_power_save_params.ibssPs1RxChainInAtimEnable =
					privcmd->param_value;
		WMA_LOGD("%s: IBSS Power Save single RX Chain Enable In ATIM  = %d",
			__func__,
			wma_handle->wma_ibss_power_save_params.ibssPs1RxChainInAtimEnable);
	}
		break;

#ifdef IPA_UC_OFFLOAD
	case WMA_VDEV_TXRX_GET_IPA_UC_FW_STATS_CMDID:
	{
		ol_txrx_pdev_handle pdev;

		pdev = vos_get_context(VOS_MODULE_ID_TXRX,
			wma_handle->vos_context);
		if (!pdev) {
			WMA_LOGE("pdev NULL for uc stat");
			return -EINVAL;
		}
		ol_txrx_ipa_uc_get_stat(pdev);
	}
		break;
#endif /* IPA_UC_OFFLOAD */

	default:
		WMA_LOGE("Invalid wma config command id:%d",
			privcmd->param_id);
		ret = -EINVAL;
	}
	return ret;
}

static int wmi_crash_inject(wmi_unified_t wmi_handle, u_int32_t type,
				u_int32_t delay_time_ms)
{
	int ret = 0;
	WMI_FORCE_FW_HANG_CMD_fixed_param *cmd;
	u_int16_t len = sizeof(*cmd);
	wmi_buf_t buf;

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed!", __func__);
		return -ENOMEM;
	}

	cmd = (WMI_FORCE_FW_HANG_CMD_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_WMI_FORCE_FW_HANG_CMD_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(WMI_FORCE_FW_HANG_CMD_fixed_param));
	cmd->type = type;
	cmd->delay_time_ms = delay_time_ms;

	ret = wmi_unified_cmd_send(wmi_handle, buf, len, WMI_FORCE_FW_HANG_CMDID);
	if (ret < 0) {
		WMA_LOGE("%s: Failed to send set param command, ret = %d",
			__func__, ret);
		wmi_buf_free(buf);
	}

	return ret;
}

/**
 * wma_crash_inject() - sends command to FW to simulate crash
 * @wma_handle:         pointer of WMA context
 * @type:               subtype of the command
 * @delay_time_ms:      time in milliseconds for FW to delay the crash
 *
 * This function will send a command to FW in order to simulate different
 * kinds of FW crashes.
 *
 * Return: 0 for success or reasons for failure
 */

int wma_crash_inject(tp_wma_handle wma_handle, uint32_t type,
			uint32_t delay_time_ms)
{
	return wmi_crash_inject(wma_handle->wmi_handle, type, delay_time_ms);
}

static int32_t wmi_unified_set_sta_ps_param(wmi_unified_t wmi_handle,
		u_int32_t vdev_id, u_int32_t param, u_int32_t value)
{
	wmi_sta_powersave_param_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);
	tp_wma_handle wma;
	struct wma_txrx_node *iface;
	wma = vos_get_context(VOS_MODULE_ID_WDA,
			vos_get_global_context(VOS_MODULE_ID_WDA, NULL));
	if (NULL == wma) {
		WMA_LOGE("%s: wma is NULL", __func__);
		return -EIO;
	}
	iface = &wma->interfaces[vdev_id];

	WMA_LOGD("Set Sta Ps param vdevId %d Param %d val %d",
		      vdev_id, param, value);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s: Set Sta Ps param Mem Alloc Failed", __func__);
		return -ENOMEM;
	}

	cmd = (wmi_sta_powersave_param_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_sta_powersave_param_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_sta_powersave_param_cmd_fixed_param));
	cmd->vdev_id = vdev_id;
	cmd->param = param;
	cmd->value = value;

	if (wmi_unified_cmd_send(wmi_handle, buf, len,
			WMI_STA_POWERSAVE_PARAM_CMDID)) {
		WMA_LOGE("Set Sta Ps param Failed vdevId %d Param %d val %d",
			vdev_id, param, value);
		adf_nbuf_free(buf);
		return -EIO;
	}
	/* Store the PS Status */
	iface->ps_enabled = value ? TRUE : FALSE;
	return 0;
}

#ifdef FEATURE_GREEN_AP
static int32_t wmi_unified_pdev_green_ap_ps_enable_cmd(wmi_unified_t wmi_handle,
		u_int32_t value)
{
	wmi_pdev_green_ap_ps_enable_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	WMA_LOGD("Set Green AP PS val %d", value);

	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s: Green AP PS Mem Alloc Failed", __func__);
		return -ENOMEM;
	}

	cmd = (wmi_pdev_green_ap_ps_enable_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_pdev_green_ap_ps_enable_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_pdev_green_ap_ps_enable_cmd_fixed_param));
	cmd->reserved0 = 0;
	cmd->enable = value;

	if (wmi_unified_cmd_send(wmi_handle, buf, len,
				WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID)) {
		WMA_LOGE("Set Green AP PS param Failed val %d", value);

		adf_nbuf_free(buf);
		return -EIO;
	}
	return 0;
}
#endif /* FEATURE_GREEN_AP */

static int
wmi_unified_vdev_set_gtx_cfg_send(wmi_unified_t wmi_handle, u_int32_t if_id,
				gtx_config_t *gtx_info)
{
	wmi_vdev_set_gtx_params_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int len = sizeof(wmi_vdev_set_gtx_params_cmd_fixed_param);
	buf = wmi_buf_alloc(wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s:wmi_buf_alloc failed", __FUNCTION__);
		return -1;
	}
	cmd = (wmi_vdev_set_gtx_params_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_vdev_set_gtx_params_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_set_gtx_params_cmd_fixed_param));
	cmd->vdev_id = if_id;

	cmd->gtxRTMask[0] = gtx_info->gtxRTMask[0];
	cmd->gtxRTMask[1] = gtx_info->gtxRTMask[1];
	cmd->userGtxMask = gtx_info->gtxUsrcfg;
	cmd->gtxPERThreshold = gtx_info->gtxPERThreshold;
	cmd->gtxPERMargin = gtx_info->gtxPERMargin;
	cmd->gtxTPCstep = gtx_info->gtxTPCstep;
	cmd->gtxTPCMin = gtx_info->gtxTPCMin;
	cmd->gtxBWMask = gtx_info->gtxBWMask;

	WMA_LOGD("Setting vdev%d GTX values:htmcs 0x%x, vhtmcs 0x%x, usermask 0x%x, \
		gtxPERThreshold %d, gtxPERMargin %d, gtxTPCstep %d, gtxTPCMin %d, \
                gtxBWMask 0x%x.", if_id, cmd->gtxRTMask[0], cmd->gtxRTMask[1],
		cmd->userGtxMask, cmd->gtxPERThreshold, cmd->gtxPERMargin,
		cmd->gtxTPCstep, cmd->gtxTPCMin, cmd->gtxBWMask);
	return wmi_unified_cmd_send(wmi_handle, buf, len, WMI_VDEV_SET_GTX_PARAMS_CMDID);
}

/**
 * wma_set_modulated_dtim() - function to configure modulated dtim
 * @wma: wma handle
 * @privcmd: structure containing parameters
 *
 * This function configures the modulated dtim in firmware
 *
 * Return: none
 */
static void wma_set_modulated_dtim(tp_wma_handle wma,
				   wda_cli_set_cmd_t *privcmd)
{
	uint8_t vdev_id = privcmd->param_vdev_id;
	struct wma_txrx_node *iface =
		&wma->interfaces[vdev_id];
	bool prev_dtim_enabled;
	uint32_t listen_interval;
	int ret;

	iface->alt_modulated_dtim = privcmd->param_value;

	prev_dtim_enabled = iface->alt_modulated_dtim_enabled;

	if (1 != privcmd->param_value)
		iface->alt_modulated_dtim_enabled = true;
	else
		iface->alt_modulated_dtim_enabled = false;

	if ((true == iface->alt_modulated_dtim_enabled) ||
	    (true == prev_dtim_enabled)) {

		listen_interval = iface->alt_modulated_dtim
			* iface->dtimPeriod;

		ret = wmi_unified_vdev_set_param_send(wma->wmi_handle,
						privcmd->param_vdev_id,
						WMI_VDEV_PARAM_LISTEN_INTERVAL,
						listen_interval);
		if (ret)
			/* Even if it fails, continue */
			WMA_LOGW("Failed to set listen interval %d",
				 listen_interval);

		ret = wmi_unified_vdev_set_param_send(wma->wmi_handle,
						privcmd->param_vdev_id,
						WMI_VDEV_PARAM_DTIM_POLICY ,
						NORMAL_DTIM);
		if (ret)
			WMA_LOGE("Failed to Set to Normal DTIM policy");
	}
}

static void wma_process_cli_set_cmd(tp_wma_handle wma,
				    wda_cli_set_cmd_t *privcmd)
{
	int ret = 0, vid = privcmd->param_vdev_id, pps_val = 0;
	struct wma_txrx_node *intr = wma->interfaces;
	tpAniSirGlobal pMac = (tpAniSirGlobal )vos_get_context(VOS_MODULE_ID_PE,
				wma->vos_context);
	struct qpower_params *qparams = &intr[vid].config.qpower_params;

	WMA_LOGD("wmihandle %p", wma->wmi_handle);

	if (NULL == pMac) {
		WMA_LOGE("%s: Failed to get pMac", __func__);
		return;
	}

	if (privcmd->param_id >= WMI_CMDID_MAX) {
		/*
		 * This configuration setting is not done using any wmi
		 * command, call appropriate handler.
		 */
		if (wma_set_priv_cfg(wma, privcmd))
			WMA_LOGE("Failed to set wma priv congiuration");
		return;
	}

	switch (privcmd->param_vp_dev) {
	case VDEV_CMD:
		WMA_LOGD("vdev id %d pid %d pval %d", privcmd->param_vdev_id,
				privcmd->param_id, privcmd->param_value);
		ret = wmi_unified_vdev_set_param_send(wma->wmi_handle,
						privcmd->param_vdev_id,
						privcmd->param_id,
						privcmd->param_value);
		if (ret) {
			WMA_LOGE("wmi_unified_vdev_set_param_send"
					" failed ret %d", ret);
			return;
		}
		break;
	case PDEV_CMD:
		WMA_LOGD("pdev pid %d pval %d", privcmd->param_id,
				privcmd->param_value);
		if ((privcmd->param_id == WMI_PDEV_PARAM_RX_CHAIN_MASK) ||
			(privcmd->param_id == WMI_PDEV_PARAM_TX_CHAIN_MASK)) {
			wma_update_txrx_chainmask(wma->num_rf_chains,
						&privcmd->param_value);
		}

		ret = wmi_unified_pdev_set_param(wma->wmi_handle,
						privcmd->param_id,
						privcmd->param_value);
		if (ret) {
			WMA_LOGE("wmi_unified_vdev_set_param_send"
					" failed ret %d", ret);
			return;
		}
		break;
	case GEN_CMD:
	{
		ol_txrx_vdev_handle vdev = NULL;
		struct wma_txrx_node *intr = wma->interfaces;

		vdev = wma_find_vdev_by_id(wma, privcmd->param_vdev_id);
		if (!vdev) {
			WMA_LOGE("%s:Invalid vdev handle", __func__);
			return;
                }
		WMA_LOGD("gen pid %d pval %d", privcmd->param_id,
				privcmd->param_value);

		switch (privcmd->param_id) {
		case GEN_VDEV_PARAM_AMPDU:
			ret = ol_txrx_aggr_cfg(vdev, privcmd->param_value, 0);
			if (ret)
				WMA_LOGE("ol_txrx_aggr_cfg set ampdu"
					" failed ret %d", ret);
			else
				intr[privcmd->param_vdev_id].config.ampdu = privcmd->param_value;
			break;
		case GEN_VDEV_PARAM_AMSDU:
			ret = ol_txrx_aggr_cfg(vdev, 0, privcmd->param_value);
			if (ret)
				WMA_LOGE("ol_txrx_aggr_cfg set amsdu"
					" failed ret %d", ret);
			else
				intr[privcmd->param_vdev_id].config.amsdu = privcmd->param_value;
			break;
		case GEN_PARAM_DUMP_AGC_START:
			HTCDump(wma->htc_handle, AGC_DUMP, true);
			break;
		case GEN_PARAM_DUMP_AGC:
			HTCDump(wma->htc_handle, AGC_DUMP, false);
			break;
		case GEN_PARAM_DUMP_CHANINFO_START:
			HTCDump(wma->htc_handle, CHAN_DUMP, true);
			break;
		case GEN_PARAM_DUMP_CHANINFO:
			HTCDump(wma->htc_handle, CHAN_DUMP, false);
			break;
		case GEN_PARAM_DUMP_WATCHDOG:
			HTCDump(wma->htc_handle, WD_DUMP, false);
			break;
		case GEN_PARAM_CRASH_INJECT:
			ret = wmi_crash_inject(wma->wmi_handle,
				privcmd->param_value, privcmd->param_sec_value);
			break;
#ifdef CONFIG_ATH_PCIE_ACCESS_DEBUG
		case GEN_PARAM_DUMP_PCIE_ACCESS_LOG:
			HTCDump(wma->htc_handle, PCIE_DUMP, false);
			break;
#endif
		case GEN_PARAM_MODULATED_DTIM:
			wma_set_modulated_dtim(wma, privcmd);
			break;

		default:
			WMA_LOGE("Invalid param id 0x%x", privcmd->param_id);
			break;
		}
		break;
        }
	case DBG_CMD:
		WMA_LOGD("dbg pid %d pval %d", privcmd->param_id,
				privcmd->param_value);
		switch (privcmd->param_id) {
		case WMI_DBGLOG_LOG_LEVEL:
                        ret = dbglog_set_log_lvl(wma->wmi_handle, privcmd->param_value);
			if (ret)
				WMA_LOGE("dbglog_set_log_lvl"
						" failed ret %d", ret);
			break;
		case WMI_DBGLOG_VAP_ENABLE:
                        ret = dbglog_vap_log_enable(wma->wmi_handle, privcmd->param_value, TRUE);
			if (ret)
				WMA_LOGE("dbglog_vap_log_enable"
						" failed ret %d", ret);
			break;
		case WMI_DBGLOG_VAP_DISABLE:
                        ret = dbglog_vap_log_enable(wma->wmi_handle, privcmd->param_value, FALSE);
			if (ret)
				WMA_LOGE("dbglog_vap_log_enable"
						" failed ret %d", ret);
			break;
		case WMI_DBGLOG_MODULE_ENABLE:
                        ret = dbglog_module_log_enable(wma->wmi_handle, privcmd->param_value, TRUE);
			if (ret)
				WMA_LOGE("dbglog_module_log_enable"
						" failed ret %d", ret);
			break;
		case WMI_DBGLOG_MODULE_DISABLE:
                        ret = dbglog_module_log_enable(wma->wmi_handle, privcmd->param_value, FALSE);
			if (ret)
				WMA_LOGE("dbglog_module_log_enable"
						" failed ret %d", ret);
			break;
	        case WMI_DBGLOG_MOD_LOG_LEVEL:
                        ret = dbglog_set_mod_log_lvl(wma->wmi_handle, privcmd->param_value);
			if (ret)
				WMA_LOGE("dbglog_module_log_enable"
						" failed ret %d", ret);
			break;
		case WMI_DBGLOG_TYPE:
                        ret = dbglog_parser_type_init(wma->wmi_handle, privcmd->param_value);
			if (ret)
				WMA_LOGE("dbglog_parser_type_init"
						" failed ret %d", ret);
			break;
		case WMI_DBGLOG_REPORT_ENABLE:
                        ret = dbglog_report_enable(wma->wmi_handle, privcmd->param_value);
			if (ret)
				WMA_LOGE("dbglog_report_enable"
						" failed ret %d", ret);
			break;
#ifdef FEATURE_GREEN_AP
		case WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID:
			/* Set the Green AP */
			ret = wmi_unified_pdev_green_ap_ps_enable_cmd(wma->wmi_handle,
					privcmd->param_value);
			if (ret) {
				WMA_LOGE("Set GreenAP Failed val %d", privcmd->param_value);
			}
			break;
#endif /* FEATURE_GREEN_AP */

		default:
			WMA_LOGE("Invalid param id 0x%x", privcmd->param_id);
			break;
		}
		break;
	case PPS_CMD:
		WMA_LOGD("dbg pid %d pval %d", privcmd->param_id,
				privcmd->param_value);
		switch (privcmd->param_id) {

		case WMI_VDEV_PPS_PAID_MATCH:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				   (PKT_PWR_SAVE_PAID_MATCH & 0xffff);
			intr[vid].config.pps_params.paid_match_enable = privcmd->param_value;
			break;
		case WMI_VDEV_PPS_GID_MATCH:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				   (PKT_PWR_SAVE_GID_MATCH & 0xffff);
			intr[vid].config.pps_params.gid_match_enable = privcmd->param_value;
			break;
		case WMI_VDEV_PPS_EARLY_TIM_CLEAR:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				   (PKT_PWR_SAVE_EARLY_TIM_CLEAR & 0xffff);
			intr[vid].config.pps_params.tim_clear = privcmd->param_value;
			break;
		case WMI_VDEV_PPS_EARLY_DTIM_CLEAR:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				   (PKT_PWR_SAVE_EARLY_DTIM_CLEAR & 0xffff);
			intr[vid].config.pps_params.dtim_clear = privcmd->param_value;
			break;
		case WMI_VDEV_PPS_EOF_PAD_DELIM:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				   (PKT_PWR_SAVE_EOF_PAD_DELIM & 0xffff);
			intr[vid].config.pps_params.eof_delim = privcmd->param_value;
			break;
		case WMI_VDEV_PPS_MACADDR_MISMATCH:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				   (PKT_PWR_SAVE_MACADDR_MISMATCH & 0xffff);
			intr[vid].config.pps_params.mac_match = privcmd->param_value;
			break;
		case WMI_VDEV_PPS_DELIM_CRC_FAIL:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				   (PKT_PWR_SAVE_DELIM_CRC_FAIL & 0xffff);
			intr[vid].config.pps_params.delim_fail = privcmd->param_value;
			break;
		case WMI_VDEV_PPS_GID_NSTS_ZERO:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				   (PKT_PWR_SAVE_GID_NSTS_ZERO & 0xffff);
			intr[vid].config.pps_params.nsts_zero = privcmd->param_value;
			break;
		case WMI_VDEV_PPS_RSSI_CHECK:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				   (PKT_PWR_SAVE_RSSI_CHECK & 0xffff);
			intr[vid].config.pps_params.rssi_chk = privcmd->param_value;
			break;
		case WMI_VDEV_PPS_5G_EBT:
			pps_val = ((privcmd->param_value << 31) & 0xffff0000) |
				   (PKT_PWR_SAVE_5G_EBT & 0xffff);
			intr[vid].config.pps_params.ebt_5g = privcmd->param_value;
			break;
		default:
			WMA_LOGE("Invalid param id 0x%x", privcmd->param_id);
			break;
		}
		break;

	case QPOWER_CMD:
		WMA_LOGD("QPOWER CLI CMD pid %d pval %d", privcmd->param_id,
			privcmd->param_value);
		switch (privcmd->param_id) {
		case WMI_STA_PS_PARAM_QPOWER_PSPOLL_COUNT:
			WMA_LOGD("QPOWER CLI CMD:Ps Poll Cnt val %d",
				privcmd->param_value);
			/* Set the QPower Ps Poll Count */
			ret = wmi_unified_set_sta_ps_param(wma->wmi_handle,
					vid,
					WMI_STA_PS_PARAM_QPOWER_PSPOLL_COUNT,
					privcmd->param_value);
			if (ret) {
			WMA_LOGE("Set Q-PsPollCnt Failed vdevId %d val %d",
				vid, privcmd->param_value);
			} else {
				qparams->max_ps_poll_cnt =
					privcmd->param_value;
			}
			break;
		case WMI_STA_PS_PARAM_QPOWER_MAX_TX_BEFORE_WAKE:
			WMA_LOGD("QPOWER CLI CMD:Max Tx Before wake val %d",
					privcmd->param_value);
			/* Set the QPower Max Tx Before Wake */
			ret = wmi_unified_set_sta_ps_param(wma->wmi_handle,
				vid,
				WMI_STA_PS_PARAM_QPOWER_MAX_TX_BEFORE_WAKE,
				privcmd->param_value);
			if (ret) {
			WMA_LOGE("Set Q-MaxTxBefWake Failed vId %d val %d",
				vid, privcmd->param_value);
			} else {
				qparams->max_tx_before_wake =
						privcmd->param_value;
			}
			break;
		case WMI_STA_PS_PARAM_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL:
			WMA_LOGD("QPOWER CLI CMD:Ps Poll Wake Inv val %d",
					privcmd->param_value);
			/* Set the QPower Spec Ps Poll Wake Inv */
			ret = wmi_unified_set_sta_ps_param(wma->wmi_handle,
								vid,
			WMI_STA_PS_PARAM_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL,
			privcmd->param_value);
			if (ret) {
			WMA_LOGE("Set Q-PsPoll WakeIntv Failed vId %d val %d",
				vid, privcmd->param_value);
			} else {
				qparams->spec_ps_poll_wake_interval =
							privcmd->param_value;
			}
			break;
		case WMI_STA_PS_PARAM_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL:
			WMA_LOGD("QPOWER CLI CMD:Spec NoData Ps Poll val %d",
					privcmd->param_value);
			/* Set the QPower Spec NoData PsPoll */
			ret = wmi_unified_set_sta_ps_param(wma->wmi_handle,
								vid,
			WMI_STA_PS_PARAM_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL,
						privcmd->param_value);
			if (ret) {
			WMA_LOGE("Set Q-SpecNoDataPsPoll Failed vId %d val %d",
				vid, privcmd->param_value);
			} else {
				qparams->max_spec_nodata_ps_poll =
						privcmd->param_value;
			}
			break;

		default:
			WMA_LOGE("Invalid param id 0x%x", privcmd->param_id);
			break;
		}
		break;
	case GTX_CMD:
		WMA_LOGD("vdev id %d pid %d pval %d", privcmd->param_vdev_id,
				privcmd->param_id, privcmd->param_value);
		switch (privcmd->param_id) {
		case WMI_VDEV_PARAM_GTX_HT_MCS:
			intr[vid].config.gtx_info.gtxRTMask[0] = privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
							privcmd->param_vdev_id,
							&intr[vid].config.gtx_info);
			break;
		case WMI_VDEV_PARAM_GTX_VHT_MCS:
			intr[vid].config.gtx_info.gtxRTMask[1] = privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
							privcmd->param_vdev_id,
							&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_USR_CFG:
			intr[vid].config.gtx_info.gtxUsrcfg = privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
							privcmd->param_vdev_id,
							&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_THRE:
			intr[vid].config.gtx_info.gtxPERThreshold = privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
							privcmd->param_vdev_id,
							&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_MARGIN:
			intr[vid].config.gtx_info.gtxPERMargin = privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
							privcmd->param_vdev_id,
							&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_STEP:
			intr[vid].config.gtx_info.gtxTPCstep = privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
							privcmd->param_vdev_id,
							&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_MINTPC:
			intr[vid].config.gtx_info.gtxTPCMin = privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
							privcmd->param_vdev_id,
							&intr[vid].config.gtx_info);
			break;

		case WMI_VDEV_PARAM_GTX_BW_MASK:
			intr[vid].config.gtx_info.gtxBWMask = privcmd->param_value;
			ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle,
							privcmd->param_vdev_id,
							&intr[vid].config.gtx_info);
			if (ret) {
				WMA_LOGE("wmi_unified_vdev_set_param_send"
						" failed ret %d", ret);
				return;
			}
			break;
		default:
			break;
		}
		break;

	default:
		WMA_LOGE("Invalid vpdev command id");
	}
	if (1 == privcmd->param_vp_dev) {
		switch (privcmd->param_id) {
		case WMI_VDEV_PARAM_NSS:
			intr[vid].config.nss = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_LDPC:
			intr[vid].config.ldpc = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_TX_STBC:
			intr[vid].config.tx_stbc = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_RX_STBC:
			intr[vid].config.rx_stbc = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_SGI:
			intr[vid].config.shortgi = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_ENABLE_RTSCTS:
			intr[vid].config.rtscts_en = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_CHWIDTH:
			intr[vid].config.chwidth = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_FIXED_RATE:
			intr[vid].config.tx_rate = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_ADJUST_ENABLE:
			intr[vid].config.erx_adjust = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_TGT_BMISS_NUM:
			intr[vid].config.erx_bmiss_num = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_BMISS_SAMPLE_CYCLE:
			intr[vid].config.erx_bmiss_cycle = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_SLOP_STEP:
			intr[vid].config.erx_slop_step = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_INIT_SLOP:
			intr[vid].config.erx_init_slop = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_ADJUST_PAUSE:
			intr[vid].config.erx_adj_pause = privcmd->param_value;
			break;
		case WMI_VDEV_PARAM_EARLY_RX_DRIFT_SAMPLE:
			intr[vid].config.erx_dri_sample = privcmd->param_value;
			break;
		default:
			WMA_LOGE("Invalid wda_cli_set vdev command/Not"
				" yet implemented 0x%x", privcmd->param_id);
		     break;
		}
	} else if (2 == privcmd->param_vp_dev) {
		switch (privcmd->param_id) {
		case WMI_PDEV_PARAM_ANI_ENABLE:
			wma->pdevconfig.ani_enable = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_ANI_POLL_PERIOD:
			wma->pdevconfig.ani_poll_len = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_ANI_LISTEN_PERIOD:
			wma->pdevconfig.ani_listen_len = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_ANI_OFDM_LEVEL:
			wma->pdevconfig.ani_ofdm_level = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_ANI_CCK_LEVEL:
			wma->pdevconfig.ani_cck_level = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_DYNAMIC_BW:
			wma->pdevconfig.cwmenable = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_TX_CHAIN_MASK:
			wma->pdevconfig.txchainmask = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_RX_CHAIN_MASK:
			wma->pdevconfig.rxchainmask = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_BURST_ENABLE:
			wma->pdevconfig.burst_enable = privcmd->param_value;
			if ((wma->pdevconfig.burst_enable == 1) &&
				(wma->pdevconfig.burst_dur == 0))
				wma->pdevconfig.burst_dur = WMA_DEFAULT_SIFS_BURST_DURATION;
			else if (wma->pdevconfig.burst_enable == 0)
				wma->pdevconfig.burst_dur = 0;
			break;
		case WMI_PDEV_PARAM_BURST_DUR:
			wma->pdevconfig.burst_dur = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_POWER_GATING_SLEEP:
			wma->pdevconfig.pwrgating = privcmd->param_value;
			break;
		case WMI_PDEV_PARAM_TXPOWER_LIMIT2G:
			wma->pdevconfig.txpow2g = privcmd->param_value;
			if ((pMac->roam.configParam.bandCapability ==
				eCSR_BAND_ALL) ||
				(pMac->roam.configParam.bandCapability ==
				eCSR_BAND_24)) {
				if (cfgSetInt(pMac,
					WNI_CFG_CURRENT_TX_POWER_LEVEL,
					privcmd->param_value) != eSIR_SUCCESS) {
					WMA_LOGE("could not set"
					" WNI_CFG_CURRENT_TX_POWER_LEVEL");
				}
			}
			else
				WMA_LOGE("Current band is not 2G");
			break;
		case WMI_PDEV_PARAM_TXPOWER_LIMIT5G:
			wma->pdevconfig.txpow5g = privcmd->param_value;
			if ((pMac->roam.configParam.bandCapability ==
				eCSR_BAND_ALL) ||
				(pMac->roam.configParam.bandCapability ==
				eCSR_BAND_5G)) {
				if (cfgSetInt(pMac,
					WNI_CFG_CURRENT_TX_POWER_LEVEL,
					privcmd->param_value) != eSIR_SUCCESS) {
					WMA_LOGE("could not set"
					" WNI_CFG_CURRENT_TX_POWER_LEVEL");
				}
			}
			else
				WMA_LOGE("Current band is not 5G");
			break;
		default:
			WMA_LOGE("Invalid wda_cli_set pdev command/Not"
				" yet implemented 0x%x", privcmd->param_id);
			break;
		}
	} else if (5 == privcmd->param_vp_dev) {
		ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, privcmd->param_vdev_id,
					      WMI_VDEV_PARAM_PACKET_POWERSAVE,
					      pps_val);
		if (ret)
			WMA_LOGE("Failed to send wmi packet power save cmd");
		else
			WMA_LOGD("Sent packet power save cmd %d value %x to target",
				 privcmd->param_id, pps_val);
	}
}

int wma_cli_get_command(void *wmapvosContext, int vdev_id,
			int param_id, int vpdev)
{
	int ret = 0;
	tp_wma_handle wma;
	struct wma_txrx_node *intr = NULL;

	wma = (tp_wma_handle) vos_get_context(VOS_MODULE_ID_WDA,
						wmapvosContext);

	if (NULL == wma)
	{
		WMA_LOGE("%s: Invalid wma handle", __func__);
		return -EINVAL;
	}

	intr = wma->interfaces;

	if (VDEV_CMD == vpdev) {
		switch (param_id) {
		case WMI_VDEV_PARAM_NSS:
			ret = intr[vdev_id].config.nss;
			break;
#ifdef QCA_SUPPORT_GTX
		case WMI_VDEV_PARAM_GTX_HT_MCS:
			ret = intr[vdev_id].config.gtx_info.gtxRTMask[0];
			break;
		case WMI_VDEV_PARAM_GTX_VHT_MCS:
			ret = intr[vdev_id].config.gtx_info.gtxRTMask[1];
			break;
		case WMI_VDEV_PARAM_GTX_USR_CFG:
			ret = intr[vdev_id].config.gtx_info.gtxUsrcfg;
			break;
		case WMI_VDEV_PARAM_GTX_THRE:
			ret = intr[vdev_id].config.gtx_info.gtxPERThreshold;
			break;
		case WMI_VDEV_PARAM_GTX_MARGIN:
			ret = intr[vdev_id].config.gtx_info.gtxPERMargin;
			break;
		case WMI_VDEV_PARAM_GTX_STEP:
			ret = intr[vdev_id].config.gtx_info.gtxTPCstep;
			break;
		case WMI_VDEV_PARAM_GTX_MINTPC:
			ret = intr[vdev_id].config.gtx_info.gtxTPCMin;
			break;
		case WMI_VDEV_PARAM_GTX_BW_MASK:
			ret = intr[vdev_id].config.gtx_info.gtxBWMask;
			break;
#endif
		case WMI_VDEV_PARAM_LDPC:
			ret = intr[vdev_id].config.ldpc;
			break;
		case WMI_VDEV_PARAM_TX_STBC:
			ret = intr[vdev_id].config.tx_stbc;
			break;
		case WMI_VDEV_PARAM_RX_STBC:
			ret = intr[vdev_id].config.rx_stbc;
			break;
		case WMI_VDEV_PARAM_SGI:
			ret = intr[vdev_id].config.shortgi;
			break;
		case WMI_VDEV_PARAM_ENABLE_RTSCTS:
			ret = intr[vdev_id].config.rtscts_en;
			break;
		case WMI_VDEV_PARAM_CHWIDTH:
			ret = intr[vdev_id].config.chwidth;
			break;
		case WMI_VDEV_PARAM_FIXED_RATE:
			ret = intr[vdev_id].config.tx_rate;
			break;

		default:
			WMA_LOGE("Invalid cli_get vdev command/Not"
					" yet implemented 0x%x", param_id);
			return -EINVAL;
		}
	} else if (PDEV_CMD == vpdev) {
		switch (param_id) {
		case WMI_PDEV_PARAM_ANI_ENABLE:
			ret = wma->pdevconfig.ani_enable;
			break;
		case WMI_PDEV_PARAM_ANI_POLL_PERIOD:
			ret = wma->pdevconfig.ani_poll_len;
			break;
		case WMI_PDEV_PARAM_ANI_LISTEN_PERIOD:
			ret = wma->pdevconfig.ani_listen_len;
			break;
		case WMI_PDEV_PARAM_ANI_OFDM_LEVEL:
			ret = wma->pdevconfig.ani_ofdm_level;
			break;
		case WMI_PDEV_PARAM_ANI_CCK_LEVEL:
			ret = wma->pdevconfig.ani_cck_level;
			break;
		case WMI_PDEV_PARAM_DYNAMIC_BW:
			ret = wma->pdevconfig.cwmenable;
			break;
		case WMI_PDEV_PARAM_TX_CHAIN_MASK:
			ret = wma->pdevconfig.txchainmask;
			break;
		case WMI_PDEV_PARAM_RX_CHAIN_MASK:
			ret = wma->pdevconfig.rxchainmask;
			break;
		case WMI_PDEV_PARAM_TXPOWER_LIMIT2G:
			ret = wma->pdevconfig.txpow2g;
			break;
		case WMI_PDEV_PARAM_TXPOWER_LIMIT5G:
			ret = wma->pdevconfig.txpow5g;
			break;
                case WMI_PDEV_PARAM_POWER_GATING_SLEEP:
			ret = wma->pdevconfig.pwrgating;
			break;
                case WMI_PDEV_PARAM_BURST_ENABLE:
			ret = wma->pdevconfig.burst_enable;
			break;
                case WMI_PDEV_PARAM_BURST_DUR:
			ret = wma->pdevconfig.burst_dur;
			break;
		default:
			WMA_LOGE("Invalid cli_get pdev command/Not"
					" yet implemented 0x%x", param_id);
			return -EINVAL;
		}
	} else if (GEN_CMD == vpdev) {
		switch (param_id) {
		case GEN_VDEV_PARAM_AMPDU:
			ret = intr[vdev_id].config.ampdu;
			break;
		case GEN_VDEV_PARAM_AMSDU:
			ret = intr[vdev_id].config.amsdu;
			break;
		default:
			WMA_LOGE("Invalid generic vdev command/Not"
					" yet implemented 0x%x", param_id);
			return -EINVAL;
		}
	} else if (PPS_CMD == vpdev) {
		switch (param_id) {
		case WMI_VDEV_PPS_PAID_MATCH:
			ret = intr[vdev_id].config.pps_params.paid_match_enable;
			break;
		case WMI_VDEV_PPS_GID_MATCH:
			ret = intr[vdev_id].config.pps_params.gid_match_enable;
			break;
		case WMI_VDEV_PPS_EARLY_TIM_CLEAR:
			ret = intr[vdev_id].config.pps_params.tim_clear;
			break;
		case WMI_VDEV_PPS_EARLY_DTIM_CLEAR:
			ret = intr[vdev_id].config.pps_params.dtim_clear;
			break;
		case WMI_VDEV_PPS_EOF_PAD_DELIM:
			ret = intr[vdev_id].config.pps_params.eof_delim;
			break;
		case WMI_VDEV_PPS_MACADDR_MISMATCH:
			ret = intr[vdev_id].config.pps_params.mac_match;
			break;
		case WMI_VDEV_PPS_DELIM_CRC_FAIL:
			ret = intr[vdev_id].config.pps_params.delim_fail;
			break;
		case WMI_VDEV_PPS_GID_NSTS_ZERO:
			ret = intr[vdev_id].config.pps_params.nsts_zero;
			break;
		case WMI_VDEV_PPS_RSSI_CHECK:
			ret = intr[vdev_id].config.pps_params.rssi_chk;
			break;
		default:
			WMA_LOGE("Invalid pps vdev command/Not"
					" yet implemented 0x%x", param_id);
			return -EINVAL;
		}
	} else if (QPOWER_CMD == vpdev) {
		switch (param_id) {
		case WMI_STA_PS_PARAM_QPOWER_PSPOLL_COUNT:
		ret = intr[vdev_id].config.qpower_params.max_ps_poll_cnt;
		break;

		case WMI_STA_PS_PARAM_QPOWER_MAX_TX_BEFORE_WAKE:
		ret = intr[vdev_id].config.qpower_params.max_tx_before_wake;
		break;

		case WMI_STA_PS_PARAM_QPOWER_SPEC_PSPOLL_WAKE_INTERVAL:
		ret =
		intr[vdev_id].config.qpower_params.spec_ps_poll_wake_interval;
		break;

		case WMI_STA_PS_PARAM_QPOWER_SPEC_MAX_SPEC_NODATA_PSPOLL:
		ret = intr[vdev_id].config.qpower_params.max_spec_nodata_ps_poll;
		break;

		default:
		WMA_LOGE("Invalid generic vdev command/Not"
			" yet implemented 0x%x", param_id);
		return -EINVAL;
		}
	} else if (GTX_CMD == vpdev) {
		switch (param_id) {
		case WMI_VDEV_PARAM_GTX_HT_MCS:
			ret = intr[vdev_id].config.gtx_info.gtxRTMask[0];
			break;
		case WMI_VDEV_PARAM_GTX_VHT_MCS:
			ret = intr[vdev_id].config.gtx_info.gtxRTMask[1];
			break;
		case WMI_VDEV_PARAM_GTX_USR_CFG:
			ret = intr[vdev_id].config.gtx_info.gtxUsrcfg;
			break;
		case WMI_VDEV_PARAM_GTX_THRE:
			ret = intr[vdev_id].config.gtx_info.gtxPERThreshold;
			break;
		case WMI_VDEV_PARAM_GTX_MARGIN:
			ret = intr[vdev_id].config.gtx_info.gtxPERMargin;
			break;
		case WMI_VDEV_PARAM_GTX_STEP:
			ret = intr[vdev_id].config.gtx_info.gtxTPCstep;
			break;
		case WMI_VDEV_PARAM_GTX_MINTPC:
			ret = intr[vdev_id].config.gtx_info.gtxTPCMin;
			break;
		case WMI_VDEV_PARAM_GTX_BW_MASK:
			ret = intr[vdev_id].config.gtx_info.gtxBWMask;
			break;
		default:
			WMA_LOGE("Invalid generic vdev command/Not"
					" yet implemented 0x%x", param_id);
			return -EINVAL;
		}
	}
	return ret;
}

static void
wma_update_protection_mode(tp_wma_handle wma, u_int8_t vdev_id,
			   u_int8_t llbcoexist)
{
	int ret;
	enum ieee80211_protmode prot_mode;

	prot_mode = llbcoexist ? IEEE80211_PROT_CTSONLY : IEEE80211_PROT_NONE;

	if (!wma->interfaces[vdev_id].vdev_up) {
		WMA_LOGE("%s:vdev %d is not up. Skipping to send PROTECTION_MODE"
				, __func__, vdev_id);
		return;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_PROTECTION_MODE,
					      prot_mode);

	if (ret)
		WMA_LOGE("Failed to send wmi protection mode cmd");
	else
		WMA_LOGD("Updated protection mode %d to target", prot_mode);
}

static void
wma_update_beacon_interval(tp_wma_handle wma, u_int8_t vdev_id,
                           u_int16_t beaconInterval)
{
        int ret;

        ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
                                              WMI_VDEV_PARAM_BEACON_INTERVAL,
                                              beaconInterval);

        if (ret)
                WMA_LOGE("Failed to update beacon interval");
        else
                WMA_LOGI("Updated beacon interval %d for vdev %d", beaconInterval, vdev_id);
}


/*
 * Function	: wma_process_update_beacon_params
 * Description	: update the beacon parameters to target
 * Args		: wma handle, beacon parameters
 * Returns	: None
 */
static void
wma_process_update_beacon_params(tp_wma_handle wma,
				 tUpdateBeaconParams *bcn_params)
{
	if (!bcn_params) {
		WMA_LOGE("bcn_params NULL");
		return;
	}

	if (bcn_params->smeSessionId >= wma->max_bssid) {
		WMA_LOGE("Invalid vdev id %d", bcn_params->smeSessionId);
		return;
	}

	if (bcn_params->paramChangeBitmap & PARAM_BCN_INTERVAL_CHANGED) {
		wma_update_beacon_interval(wma, bcn_params->smeSessionId,
						bcn_params->beaconInterval);
	}

	if (bcn_params->paramChangeBitmap & PARAM_llBCOEXIST_CHANGED)
		wma_update_protection_mode(wma, bcn_params->smeSessionId,
					   bcn_params->llbCoexist);
}

/*
 * Function    : wma_update_cfg_params
 * Description : update the cfg parameters to target
 * Args        : wma handle, cfg parameter
 * Returns     : None
 */
static void
wma_update_cfg_params(tp_wma_handle wma, tSirMsgQ *cfgParam)
{
	u_int8_t vdev_id;
	u_int32_t param_id;
	tANI_U32 cfg_val;
	int ret;
	/* get mac to acess CFG data base */
	struct sAniSirGlobal *pmac;

	switch(cfgParam->bodyval) {
	case WNI_CFG_RTS_THRESHOLD:
		param_id = WMI_VDEV_PARAM_RTS_THRESHOLD;
		break;
	case WNI_CFG_FRAGMENTATION_THRESHOLD:
		param_id = WMI_VDEV_PARAM_FRAGMENTATION_THRESHOLD;
		break;
	default:
		WMA_LOGD("Unhandled cfg parameter %d", cfgParam->bodyval);
		return;
	}

	pmac = (struct sAniSirGlobal*)vos_get_context(VOS_MODULE_ID_PE,
					wma->vos_context);

	if (NULL == pmac) {
		WMA_LOGE("%s: Failed to get pmac", __func__);
		return;
	}

	if (wlan_cfgGetInt(pmac, (tANI_U16) cfgParam->bodyval,
			   &cfg_val) != eSIR_SUCCESS)
	{
		WMA_LOGE("Failed to get value for CFG PARAMS %d. returning without updating",
			 cfgParam->bodyval);
		return;
	}

	for (vdev_id = 0; vdev_id < wma->max_bssid; vdev_id++) {
		if (wma->interfaces[vdev_id].handle != 0) {
			ret = wmi_unified_vdev_set_param_send(wma->wmi_handle,
				vdev_id, param_id, cfg_val);
			if (ret)
				WMA_LOGE("Update cfg params failed for vdevId %d", vdev_id);
		}
	}
}

/* BSS set params functions */
static void
wma_vdev_set_bss_params(tp_wma_handle wma, int vdev_id,
		tSirMacBeaconInterval beaconInterval, tANI_U8 dtimPeriod,
		tANI_U8 shortSlotTimeSupported, tANI_U8 llbCoexist,
		tPowerdBm maxTxPower)
{
	int ret;
	uint32_t slot_time;
	struct wma_txrx_node *intr = wma->interfaces;

	/* Beacon Interval setting */
	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_BEACON_INTERVAL,
					      beaconInterval);

	if (ret)
		WMA_LOGE("failed to set WMI_VDEV_PARAM_BEACON_INTERVAL");

	ret = wmi_unified_vdev_set_gtx_cfg_send(wma->wmi_handle, vdev_id,
						&intr[vdev_id].config.gtx_info);
	if (ret)
		WMA_LOGE("failed to set WMI_VDEV_PARAM_DTIM_PERIOD");

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_DTIM_PERIOD,
					      dtimPeriod);
	if (ret)
		WMA_LOGE("failed to set WMI_VDEV_PARAM_DTIM_PERIOD");

	if (!maxTxPower)
	{
		WMA_LOGW("Setting Tx power limit to 0");
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
					WMI_VDEV_PARAM_TX_PWRLIMIT,
					maxTxPower);
	if (ret)
		WMA_LOGE("failed to set WMI_VDEV_PARAM_TX_PWRLIMIT");
	else
		intr[vdev_id].max_tx_power = maxTxPower;

	/* Slot time */
	if (shortSlotTimeSupported)
		slot_time = WMI_VDEV_SLOT_TIME_SHORT;
	else
		slot_time = WMI_VDEV_SLOT_TIME_LONG;

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
					      WMI_VDEV_PARAM_SLOT_TIME,
					      slot_time);
	if (ret)
		WMA_LOGE("failed to set WMI_VDEV_PARAM_SLOT_TIME");

	/* Initialize protection mode in case of coexistence */
	wma_update_protection_mode(wma, vdev_id, llbCoexist);
}

static void wma_add_bss_ap_mode(tp_wma_handle wma, tpAddBssParams add_bss)
{
	ol_txrx_pdev_handle pdev;
	ol_txrx_vdev_handle vdev;
	struct wma_vdev_start_req req;
	ol_txrx_peer_handle peer;
	struct wma_target_req *msg;
	u_int8_t vdev_id, peer_id;
	VOS_STATUS status;
	tPowerdBm maxTxPower;
#ifdef WLAN_FEATURE_11W
	int ret = 0;
#endif /* WLAN_FEATURE_11W */

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		goto send_fail_resp;
	}

	vdev = wma_find_vdev_by_addr(wma, add_bss->bssId, &vdev_id);
	if (!vdev) {
		WMA_LOGE("%s: Failed to get vdev handle", __func__);
		goto send_fail_resp;
	}
	wma_set_bss_rate_flags(&wma->interfaces[vdev_id], add_bss);
	status = wma_create_peer(wma, pdev, vdev, add_bss->bssId,
	                         WMI_PEER_TYPE_DEFAULT, vdev_id,
				 VOS_FALSE);
	if (status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("%s: Failed to create peer", __func__);
		goto send_fail_resp;
	}

	peer = ol_txrx_find_peer_by_addr(pdev, add_bss->bssId, &peer_id);
	if (!peer) {
		WMA_LOGE("%s Failed to find peer %pM", __func__,
			 add_bss->bssId);
		goto send_fail_resp;
	}
	msg = wma_fill_vdev_req(wma, vdev_id, WDA_ADD_BSS_REQ,
				WMA_TARGET_REQ_TYPE_VDEV_START, add_bss,
				WMA_VDEV_START_REQUEST_TIMEOUT);
	if (!msg) {
		WMA_LOGP("%s Failed to allocate vdev request vdev_id %d",
			 __func__, vdev_id);
		goto peer_cleanup;
	}

	add_bss->staContext.staIdx = ol_txrx_local_peer_id(peer);

	vos_mem_zero(&req, sizeof(req));
	req.vdev_id = vdev_id;
	req.chan = add_bss->currentOperChannel;
	req.chan_offset = add_bss->currentExtChannel;
        req.vht_capable = add_bss->vhtCapable;
#if defined WLAN_FEATURE_VOWIFI
	req.max_txpow = add_bss->maxTxPower;
	maxTxPower = add_bss->maxTxPower;
#else
	req.max_txpow = 0;
	maxTxPower = 0;
#endif
#ifdef WLAN_FEATURE_11W
	if (add_bss->rmfEnabled) {
		/*
		 * when 802.11w PMF is enabled for hw encr/decr
		 * use hw MFP Qos bits 0x10
		 */
		ret = wmi_unified_pdev_set_param(wma->wmi_handle,
				WMI_PDEV_PARAM_PMF_QOS, TRUE);
		if(ret) {
			WMA_LOGE("%s: Failed to set QOS MFP/PMF (%d)",
				__func__, ret);
		} else {
			WMA_LOGI("%s: QOS MFP/PMF set to %d",
				__func__, TRUE);
		}
	}
#endif /* WLAN_FEATURE_11W */

	req.beacon_intval = add_bss->beaconInterval;
	req.dtim_period = add_bss->dtimPeriod;
	req.hidden_ssid = add_bss->bHiddenSSIDEn;
	req.is_dfs = add_bss->bSpectrumMgtEnabled;
	req.oper_mode = BSS_OPERATIONAL_MODE_AP;
	req.ssid.length = add_bss->ssId.length;
	if (req.ssid.length > 0)
		vos_mem_copy(req.ssid.ssId, add_bss->ssId.ssId,
			     add_bss->ssId.length);

	status = wma_vdev_start(wma, &req, VOS_FALSE);
	if (status != VOS_STATUS_SUCCESS) {
		wma_remove_vdev_req(wma, vdev_id,
				    WMA_TARGET_REQ_TYPE_VDEV_START);
		goto peer_cleanup;
	}

	wma_vdev_set_bss_params(wma, vdev_id,
		add_bss->beaconInterval, add_bss->dtimPeriod,
		add_bss->shortSlotTimeSupported, add_bss->llbCoexist,
		maxTxPower);

	return;

peer_cleanup:
	wma_remove_peer(wma, add_bss->bssId, vdev_id, peer,
			VOS_FALSE);
send_fail_resp:
	add_bss->status = VOS_STATUS_E_FAILURE;
	wma_send_msg(wma, WDA_ADD_BSS_RSP, (void *)add_bss, 0);
}

#ifdef QCA_IBSS_SUPPORT
static VOS_STATUS
wma_set_ibss_pwrsave_params(tp_wma_handle wma, u_int8_t vdev_id)
{
	int ret;

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_ATIM_WINDOW_LENGTH,
				wma->wma_ibss_power_save_params.atimWindowLength);
	if (ret < 0) {
		WMA_LOGE("Failed to set WMI_VDEV_PARAM_ATIM_WINDOW_LENGTH ret = %d",
				ret);
		return VOS_STATUS_E_FAILURE;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_IS_IBSS_POWER_SAVE_ALLOWED,
				wma->wma_ibss_power_save_params.isPowerSaveAllowed);
	if (ret < 0) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_IS_IBSS_POWER_SAVE_ALLOWED ret=%d",
				ret);
		return VOS_STATUS_E_FAILURE;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_IS_POWER_COLLAPSE_ALLOWED,
				wma->wma_ibss_power_save_params.isPowerCollapseAllowed);
	if (ret < 0) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_IS_POWER_COLLAPSE_ALLOWED ret=%d",
				ret);
		return VOS_STATUS_E_FAILURE;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_IS_AWAKE_ON_TXRX_ENABLED,
				wma->wma_ibss_power_save_params.isAwakeonTxRxEnabled);
	if (ret < 0) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_IS_AWAKE_ON_TXRX_ENABLED ret=%d",
				ret);
		return VOS_STATUS_E_FAILURE;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_INACTIVITY_CNT,
				wma->wma_ibss_power_save_params.inactivityCount);
	if (ret < 0) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_INACTIVITY_CNT ret=%d",
				ret);
		return VOS_STATUS_E_FAILURE;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_TXSP_END_INACTIVITY_TIME_MS,
				wma->wma_ibss_power_save_params.txSPEndInactivityTime);
	if (ret < 0) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_TXSP_END_INACTIVITY_TIME_MS ret=%d",
				ret);
		return VOS_STATUS_E_FAILURE;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
            WMI_VDEV_PARAM_IBSS_PS_WARMUP_TIME_SECS,
            wma->wma_ibss_power_save_params.ibssPsWarmupTime);
	if (ret < 0) {
		WMA_LOGE("Failed, set WMI_VDEV_PARAM_IBSS_PS_WARMUP_TIME_SECS ret=%d",
				ret);
		return VOS_STATUS_E_FAILURE;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_IBSS_PS_1RX_CHAIN_IN_ATIM_WINDOW_ENABLE,
				wma->wma_ibss_power_save_params.ibssPs1RxChainInAtimEnable);
	if (ret < 0) {
		WMA_LOGE("Failed to set IBSS_PS_1RX_CHAIN_IN_ATIM_WINDOW_ENABLE ret=%d",
				ret);
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;
}

static void wma_add_bss_ibss_mode(tp_wma_handle wma, tpAddBssParams add_bss)
{
	ol_txrx_pdev_handle pdev;
	ol_txrx_vdev_handle vdev;
	struct wma_vdev_start_req req;
	ol_txrx_peer_handle peer = NULL;
	struct wma_target_req *msg;
	u_int8_t vdev_id, peer_id;
	VOS_STATUS status;
        tDelStaSelfParams del_sta_param;
        tAddStaSelfParams add_sta_self_param;
	tSetBssKeyParams key_info;

        WMA_LOGD("%s: add_bss->sessionId = %d", __func__, add_bss->sessionId);
        vdev_id = add_bss->sessionId;
	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		goto send_fail_resp;
	}
	wma_set_bss_rate_flags(&wma->interfaces[vdev_id], add_bss);

	vdev = wma_find_vdev_by_id(wma, vdev_id);
	if (!vdev) {
	        WMA_LOGE("%s: vdev not found for vdev id %d.",
			__func__, vdev_id);
		goto send_fail_resp;
	}

	/* only change vdev type to ibss during 1st time join_ibss handling */

        if (FALSE == wma_is_vdev_in_ibss_mode(wma, vdev_id)) {

	        WMA_LOGD("%s: vdev found for vdev id %d. deleting the vdev",
			__func__, vdev_id);

		/* remove peers on the existing non-ibss vdev */
                TAILQ_FOREACH(peer, &vdev->peer_list, peer_list_elem) {
	                WMA_LOGE("%s: peer found for vdev id %d. deleting the peer",
                                __func__, vdev_id);
                        wma_remove_peer(wma, (u_int8_t *)&vdev->mac_addr,
		                vdev_id, peer, VOS_FALSE);
                }

		/* remove the non-ibss vdev */
                vos_copy_macaddr((v_MACADDR_t *)&(del_sta_param.selfMacAddr),
                                 (v_MACADDR_t *)&(vdev->mac_addr));
                del_sta_param.sessionId   = vdev_id;
                del_sta_param.status      = 0;

		wma_vdev_detach(wma, &del_sta_param, 0);

		/* create new vdev for ibss */
		vos_copy_macaddr((v_MACADDR_t *)&(add_sta_self_param.selfMacAddr),
			(v_MACADDR_t *)&(add_bss->selfMacAddr));
		add_sta_self_param.sessionId = vdev_id;
		add_sta_self_param.type      = WMI_VDEV_TYPE_IBSS;
		add_sta_self_param.subType   = 0;
		add_sta_self_param.status    = 0;

		vdev = wma_vdev_attach(wma, &add_sta_self_param, 0);
		if (!vdev) {
			WMA_LOGE("%s: Failed to create vdev", __func__);
			goto send_fail_resp;
		}

		WLANTL_RegisterVdev(wma->vos_context, vdev);
		/* Register with TxRx Module for Data Ack Complete Cb */
		wdi_in_data_tx_cb_set(vdev, wma_data_tx_ack_comp_hdlr, wma);
		WMA_LOGA("new IBSS vdev created with mac %pM", add_bss->selfMacAddr);

		/* create ibss bss peer */
		status = wma_create_peer(wma, pdev, vdev, add_bss->selfMacAddr,
	                         WMI_PEER_TYPE_DEFAULT, vdev_id,
				 VOS_FALSE);
		if (status != VOS_STATUS_SUCCESS) {
			WMA_LOGE("%s: Failed to create peer", __func__);
			goto send_fail_resp;
		}
		WMA_LOGA("IBSS BSS peer created with mac %pM", add_bss->selfMacAddr);
        }

	peer = ol_txrx_find_peer_by_addr(pdev, add_bss->selfMacAddr, &peer_id);
	if (!peer) {
		WMA_LOGE("%s Failed to find peer %pM", __func__,
			add_bss->selfMacAddr);
		goto send_fail_resp;
	}

	/* clear leftover ibss keys on bss peer */

	WMA_LOGD("%s: ibss bss key clearing", __func__);
	vos_mem_set(&key_info, sizeof(key_info), 0);
	key_info.smesessionId = vdev_id;
	key_info.numKeys = SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS;
        vos_mem_copy(&wma->ibsskey_info, &key_info, sizeof(tSetBssKeyParams));

	/* start ibss vdev */

        add_bss->operMode = BSS_OPERATIONAL_MODE_IBSS;

	msg = wma_fill_vdev_req(wma, vdev_id, WDA_ADD_BSS_REQ,
				WMA_TARGET_REQ_TYPE_VDEV_START, add_bss,
				WMA_VDEV_START_REQUEST_TIMEOUT);
	if (!msg) {
		WMA_LOGP("%s Failed to allocate vdev request vdev_id %d",
			 __func__, vdev_id);
		goto peer_cleanup;
	}
	WMA_LOGD("%s: vdev start request for IBSS enqueued", __func__);

	add_bss->staContext.staIdx = ol_txrx_local_peer_id(peer);

	/*
	 * If IBSS Power Save is supported by firmware
	 * set the IBSS power save params to firmware.
	 */
	if (WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
            WMI_SERVICE_IBSS_PWRSAVE)) {
		status = wma_set_ibss_pwrsave_params(wma, vdev_id);
		if (status != VOS_STATUS_SUCCESS) {
			WMA_LOGE("%s: Failed to Set IBSS Power Save Params to firmware",
				__func__);
			goto peer_cleanup;
		}
	}


	vos_mem_zero(&req, sizeof(req));
	req.vdev_id = vdev_id;
	req.chan = add_bss->currentOperChannel;
	req.chan_offset = add_bss->currentExtChannel;
        req.vht_capable = add_bss->vhtCapable;
#if defined WLAN_FEATURE_VOWIF
	req.max_txpow = add_bss->maxTxPower;
#else
	req.max_txpow = 0;
#endif
	req.beacon_intval = add_bss->beaconInterval;
	req.dtim_period = add_bss->dtimPeriod;
	req.hidden_ssid = add_bss->bHiddenSSIDEn;
	req.is_dfs = add_bss->bSpectrumMgtEnabled;
	req.oper_mode = BSS_OPERATIONAL_MODE_IBSS;
	req.ssid.length = add_bss->ssId.length;
	if (req.ssid.length > 0)
		vos_mem_copy(req.ssid.ssId, add_bss->ssId.ssId,
			     add_bss->ssId.length);

        WMA_LOGD("%s: chan %d chan_offset %d", __func__, req.chan, req.chan_offset);
        WMA_LOGD("%s: ssid = %s", __func__, req.ssid.ssId);

	status = wma_vdev_start(wma, &req, VOS_FALSE);
	if (status != VOS_STATUS_SUCCESS) {
		wma_remove_vdev_req(wma, vdev_id,
				    WMA_TARGET_REQ_TYPE_VDEV_START);
		goto peer_cleanup;
	}
	WMA_LOGD("%s: vdev start request for IBSS sent to target", __func__);

	/* Initialize protection mode to no protection */
	if (wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
					    WMI_VDEV_PARAM_PROTECTION_MODE,
					    IEEE80211_PROT_NONE)) {
		WMA_LOGE("Failed to initialize protection mode");
	}

	return;

peer_cleanup:
	if (peer) {
		wma_remove_peer(wma, add_bss->bssId, vdev_id, peer,
				VOS_FALSE);
	}
send_fail_resp:
	add_bss->status = VOS_STATUS_E_FAILURE;
	wma_send_msg(wma, WDA_ADD_BSS_RSP, (void *)add_bss, 0);
}
#endif

static void wma_set_bss_rate_flags(struct wma_txrx_node *iface,
							tpAddBssParams add_bss)
{
	iface->rate_flags = 0;

#ifdef WLAN_FEATURE_11AC
	if (add_bss->vhtCapable) {
		if (add_bss->vhtTxChannelWidthSet)
			iface->rate_flags |= eHAL_TX_RATE_VHT80;
		else if (add_bss->txChannelWidthSet)
			iface->rate_flags |= eHAL_TX_RATE_VHT40;
		else
			iface->rate_flags |= eHAL_TX_RATE_VHT20;
	}
	/* avoid to conflict with htCapable flag */
	else
#endif
	if (add_bss->htCapable) {
		if (add_bss->txChannelWidthSet)
			iface->rate_flags |= eHAL_TX_RATE_HT40;
		else
			iface->rate_flags |= eHAL_TX_RATE_HT20;
	}

	if (add_bss->staContext.fShortGI20Mhz ||
		add_bss->staContext.fShortGI40Mhz)
		iface->rate_flags |= eHAL_TX_RATE_SGI;

	if (!add_bss->htCapable && !add_bss->vhtCapable)
		iface->rate_flags = eHAL_TX_RATE_LEGACY;
}

static void wma_add_bss_sta_mode(tp_wma_handle wma, tpAddBssParams add_bss)
{
	ol_txrx_pdev_handle pdev;
	struct wma_vdev_start_req req;
	struct wma_target_req *msg;
	u_int8_t vdev_id, peer_id;
	ol_txrx_peer_handle peer;
	VOS_STATUS status;
	struct wma_txrx_node *iface;
	int ret = 0;
	int pps_val = 0;
	v_BOOL_t roam_synch_in_progress = VOS_FALSE;
	tpAniSirGlobal pMac = (tpAniSirGlobal)vos_get_context(VOS_MODULE_ID_PE,
				wma->vos_context);

	if (NULL == pMac) {
		WMA_LOGE("%s: Unable to get PE context", __func__);
		goto send_fail_resp;
	}

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s Failed to get pdev", __func__);
		goto send_fail_resp;
	}

	vdev_id = add_bss->staContext.smesessionId;
	iface = &wma->interfaces[vdev_id];

	wma_set_bss_rate_flags(iface, add_bss);
	if (add_bss->operMode) {
                // Save parameters later needed by WDA_ADD_STA_REQ
                if (iface->addBssStaContext) {
                        adf_os_mem_free(iface->addBssStaContext);
                }
                iface->addBssStaContext = adf_os_mem_alloc(NULL, sizeof(tAddStaParams));
                if (!iface->addBssStaContext) {
                        WMA_LOGE("%s Failed to allocat memory", __func__);
                        goto send_fail_resp;
                }
                adf_os_mem_copy(iface->addBssStaContext, &add_bss->staContext,
                                                sizeof(tAddStaParams));

#if defined WLAN_FEATURE_VOWIFI_11R
                if (iface->staKeyParams) {
                        adf_os_mem_free(iface->staKeyParams);
                        iface->staKeyParams = NULL;
                }
                if (add_bss->extSetStaKeyParamValid) {
                    iface->staKeyParams = adf_os_mem_alloc(NULL, sizeof(tSetStaKeyParams));
                    if (!iface->staKeyParams) {
                            WMA_LOGE("%s Failed to allocat memory", __func__);
                            goto send_fail_resp;
                    }
                    adf_os_mem_copy(iface->staKeyParams, &add_bss->extSetStaKeyParam,
                                                sizeof(tSetStaKeyParams));
                }
#endif
		// Save parameters later needed by WDA_ADD_STA_REQ
		iface->rmfEnabled = add_bss->rmfEnabled;
		iface->beaconInterval = add_bss->beaconInterval;
		iface->dtimPeriod = add_bss->dtimPeriod;
		iface->llbCoexist = add_bss->llbCoexist;
		iface->shortSlotTimeSupported = add_bss->shortSlotTimeSupported;
                iface->nwType = add_bss->nwType;
		if(add_bss->nonRoamReassoc) {
			peer = ol_txrx_find_peer_by_addr(pdev, add_bss->bssId, &peer_id);
			if(peer) {
				add_bss->staContext.staIdx = ol_txrx_local_peer_id(peer);
				goto send_bss_resp;
			}
		}
		if (add_bss->reassocReq) {
#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
			ol_txrx_vdev_handle vdev;
#endif
			// Called in preassoc state. BSSID peer is already added by set_linkstate
			peer = ol_txrx_find_peer_by_addr(pdev, add_bss->bssId, &peer_id);
			if (!peer) {
				WMA_LOGE("%s Failed to find peer %pM", __func__,
					 add_bss->bssId);
				goto send_fail_resp;
			}
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
            if(iface->roam_synch_in_progress) {
				add_bss->staContext.staIdx = ol_txrx_local_peer_id(peer);
				goto send_bss_resp;
			}
#endif
			msg = wma_fill_vdev_req(wma, vdev_id, WDA_ADD_BSS_REQ,
						WMA_TARGET_REQ_TYPE_VDEV_START,
						add_bss,
						WMA_VDEV_START_REQUEST_TIMEOUT);
			if (!msg) {
				WMA_LOGP("%s Failed to allocate vdev request vdev_id %d",
					 __func__, vdev_id);
				goto peer_cleanup;
			}

			add_bss->staContext.staIdx = ol_txrx_local_peer_id(peer);

			vos_mem_zero(&req, sizeof(req));
			req.vdev_id = vdev_id;
			req.chan = add_bss->currentOperChannel;
			req.chan_offset = add_bss->currentExtChannel;
#if defined WLAN_FEATURE_VOWIFI
			req.max_txpow = add_bss->maxTxPower;
#else
			req.max_txpow = 0;
#endif
			req.beacon_intval = add_bss->beaconInterval;
			req.dtim_period = add_bss->dtimPeriod;
			req.hidden_ssid = add_bss->bHiddenSSIDEn;
			req.is_dfs = add_bss->bSpectrumMgtEnabled;
			req.ssid.length = add_bss->ssId.length;
			req.oper_mode = BSS_OPERATIONAL_MODE_STA;
			if (req.ssid.length > 0)
				vos_mem_copy(req.ssid.ssId, add_bss->ssId.ssId,
						 add_bss->ssId.length);

			status = wma_vdev_start(wma, &req, VOS_FALSE);
			if (status != VOS_STATUS_SUCCESS) {
				wma_remove_vdev_req(wma, vdev_id,
							WMA_TARGET_REQ_TYPE_VDEV_START);
				goto peer_cleanup;
			}
#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
			vdev = wma_find_vdev_by_id(wma, vdev_id);
			if (!vdev) {
				WMA_LOGE("%s Invalid txrx vdev", __func__);
				goto peer_cleanup;
			}
			wdi_in_vdev_pause(vdev,
					OL_TXQ_PAUSE_REASON_PEER_UNAUTHORIZED);
#endif
			// ADD_BSS_RESP will be deferred to completion of VDEV_START

		    return;
		}
		if (!add_bss->updateBss) {
			goto send_bss_resp;

		}
		/* Update peer state */
		if (add_bss->staContext.encryptType == eSIR_ED_NONE) {
			WMA_LOGD("%s: Update peer(%pM) state into auth",
				 __func__, add_bss->bssId);
			ol_txrx_peer_state_update(pdev, add_bss->bssId,
						  ol_txrx_peer_state_auth);
		} else {
#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
			ol_txrx_vdev_handle vdev;
#endif
			WMA_LOGD("%s: Update peer(%pM) state into conn",
				 __func__, add_bss->bssId);
			ol_txrx_peer_state_update(pdev, add_bss->bssId,
						  ol_txrx_peer_state_conn);
#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
			peer = ol_txrx_find_peer_by_addr(pdev, add_bss->bssId, &peer_id);
			if (!peer) {
				WMA_LOGE("%s:%d Failed to find peer %pM", __func__,
					__LINE__, add_bss->bssId);
				goto send_fail_resp;
			}

			vdev = wma_find_vdev_by_id(wma, vdev_id);
			if (!vdev) {
				WMA_LOGE("%s Invalid txrx vdev", __func__);
				goto peer_cleanup;
			}
			wdi_in_vdev_pause(vdev,
					OL_TXQ_PAUSE_REASON_PEER_UNAUTHORIZED);
#endif
		}

		wmi_unified_send_txbf(wma, &add_bss->staContext);

		pps_val = ((pMac->enable5gEBT << 31) & 0xffff0000) | (PKT_PWR_SAVE_5G_EBT & 0xffff);
		ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
							WMI_VDEV_PARAM_PACKET_POWERSAVE,
							pps_val);
		if (ret)
			WMA_LOGE("Failed to send wmi packet power save cmd");
		else
			WMA_LOGD("Sent PKT_PWR_SAVE_5G_EBT cmd to target, val = %x, ret = %d",
				 pps_val, ret);

		wmi_unified_send_peer_assoc(wma, add_bss->nwType,
					    &add_bss->staContext);
#ifdef WLAN_FEATURE_11W
		if (add_bss->rmfEnabled) {
			/* when 802.11w PMF is enabled for hw encr/decr
			   use hw MFP Qos bits 0x10 */
			ret = wmi_unified_pdev_set_param(wma->wmi_handle,
					WMI_PDEV_PARAM_PMF_QOS, TRUE);
			if(ret) {
				WMA_LOGE("%s: Failed to set QOS MFP/PMF (%d)",
				__func__, ret);
			} else {
				WMA_LOGI("%s: QOS MFP/PMF set to %d",
				 __func__, TRUE);
			}
		}
#endif /* WLAN_FEATURE_11W */

		wma_vdev_set_bss_params(wma, add_bss->staContext.smesessionId,
				add_bss->beaconInterval, add_bss->dtimPeriod,
				add_bss->shortSlotTimeSupported, add_bss->llbCoexist,
				add_bss->maxTxPower);

		/*
		 * Store the bssid in interface table, bssid will
		 * be used during group key setting sta mode.
		 */
		vos_mem_copy(iface->bssid, add_bss->bssId, ETH_ALEN);

	}
send_bss_resp:
		ol_txrx_find_peer_by_addr(pdev, add_bss->bssId,
					  &add_bss->staContext.staIdx);
		add_bss->status = (add_bss->staContext.staIdx < 0) ?
				VOS_STATUS_E_FAILURE : VOS_STATUS_SUCCESS;
		add_bss->bssIdx = add_bss->staContext.smesessionId;
		vos_mem_copy(add_bss->staContext.staMac, add_bss->bssId,
				 sizeof(add_bss->staContext.staMac));
	WMA_LOGD("%s: opermode %d update_bss %d nw_type %d bssid %pM"
			 " staIdx %d status %d", __func__, add_bss->operMode,
			 add_bss->updateBss, add_bss->nwType, add_bss->bssId,
			 add_bss->staContext.staIdx, add_bss->status);
		wma_send_msg(wma, WDA_ADD_BSS_RSP, (void *)add_bss, 0);
		return;

peer_cleanup:
		wma_remove_peer(wma, add_bss->bssId, vdev_id, peer,
				roam_synch_in_progress);
send_fail_resp:
		add_bss->status = VOS_STATUS_E_FAILURE;
		wma_send_msg(wma, WDA_ADD_BSS_RSP, (void *)add_bss, 0);
}

static void wma_add_bss(tp_wma_handle wma, tpAddBssParams params)
{
        WMA_LOGD("%s: add_bss_param.halPersona = %d",
	         __func__, params->halPersona);

	switch(params->halPersona) {

        case VOS_STA_SAP_MODE:
        case VOS_P2P_GO_MODE:
		wma_add_bss_ap_mode(wma, params);
                break;

#ifdef QCA_IBSS_SUPPORT
        case VOS_IBSS_MODE:
		wma_add_bss_ibss_mode(wma, params);
                break;
#endif

        default:
		wma_add_bss_sta_mode(wma, params);
                break;
        }
}

static int wmi_unified_vdev_up_send(wmi_unified_t wmi,
				    u_int8_t vdev_id, u_int16_t aid,
				    u_int8_t bssid[IEEE80211_ADDR_LEN])
{
	wmi_vdev_up_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	WMA_LOGD("%s: VDEV_UP", __func__);
	WMA_LOGD("%s: vdev_id %d aid %d bssid %pM", __func__,
		 vdev_id, aid, bssid);
	buf = wmi_buf_alloc(wmi, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}
	cmd = (wmi_vdev_up_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_up_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_up_cmd_fixed_param));
	cmd->vdev_id = vdev_id;
	cmd->vdev_assoc_id = aid;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(bssid, &cmd->vdev_bssid);
	if (wmi_unified_cmd_send(wmi, buf, len, WMI_VDEV_UP_CMDID)) {
		WMA_LOGP("%s: Failed to send vdev up command", __func__);
		adf_nbuf_free(buf);
		return -EIO;
	}
	return 0;
}

static int32_t wmi_unified_set_ap_ps_param(void *wma_ctx, u_int32_t vdev_id,
			u_int8_t *peer_addr, u_int32_t param, u_int32_t value)
{
	tp_wma_handle wma_handle = (tp_wma_handle) wma_ctx;
	wmi_ap_ps_peer_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t err;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, sizeof(*cmd));
	if (!buf) {
		WMA_LOGE("Failed to allocate buffer to send set_ap_ps_param cmd");
		return -ENOMEM;
	}
	cmd = (wmi_ap_ps_peer_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_ap_ps_peer_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_ap_ps_peer_cmd_fixed_param));
	cmd->vdev_id = vdev_id;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(peer_addr, &cmd->peer_macaddr);
	cmd->param = param;
	cmd->value = value;
	err = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
				   sizeof(*cmd), WMI_AP_PS_PEER_PARAM_CMDID);
	if (err) {
		WMA_LOGE("Failed to send set_ap_ps_param cmd");
		adf_os_mem_free(buf);
		return -EIO;
	}
	return 0;
}

static int32_t wma_set_ap_peer_uapsd(tp_wma_handle wma, u_int32_t vdev_id,
		u_int8_t *peer_addr, u_int8_t uapsd_value, u_int8_t max_sp)
{
	u_int32_t uapsd = 0;
	u_int32_t max_sp_len = 0;
	int32_t ret = 0;

	if (uapsd_value & UAPSD_VO_ENABLED) {
		uapsd |= WMI_AP_PS_UAPSD_AC3_DELIVERY_EN |
			WMI_AP_PS_UAPSD_AC3_TRIGGER_EN;
	}

	if (uapsd_value & UAPSD_VI_ENABLED) {
		uapsd |= WMI_AP_PS_UAPSD_AC2_DELIVERY_EN |
			WMI_AP_PS_UAPSD_AC2_TRIGGER_EN;
	}

	if (uapsd_value & UAPSD_BK_ENABLED) {
		uapsd |= WMI_AP_PS_UAPSD_AC1_DELIVERY_EN |
			WMI_AP_PS_UAPSD_AC1_TRIGGER_EN;
	}

	if (uapsd_value & UAPSD_BE_ENABLED) {
		uapsd |= WMI_AP_PS_UAPSD_AC0_DELIVERY_EN |
			WMI_AP_PS_UAPSD_AC0_TRIGGER_EN;
	}

	switch (max_sp) {
	case UAPSD_MAX_SP_LEN_2:
		max_sp_len = WMI_AP_PS_PEER_PARAM_MAX_SP_2;
		break;
	case UAPSD_MAX_SP_LEN_4:
		max_sp_len = WMI_AP_PS_PEER_PARAM_MAX_SP_4;
		break;
	case UAPSD_MAX_SP_LEN_6:
		max_sp_len = WMI_AP_PS_PEER_PARAM_MAX_SP_6;
		break;
	default:
		max_sp_len = WMI_AP_PS_PEER_PARAM_MAX_SP_UNLIMITED;
		break;
	}

	WMA_LOGD("Set WMI_AP_PS_PEER_PARAM_UAPSD 0x%x for %pM",
		uapsd, peer_addr);

	ret = wmi_unified_set_ap_ps_param(wma, vdev_id,
					peer_addr,
					WMI_AP_PS_PEER_PARAM_UAPSD,
					uapsd);
	if (ret) {
		WMA_LOGE("Failed to set WMI_AP_PS_PEER_PARAM_UAPSD for %pM",
			peer_addr);
		return ret;
	}

	WMA_LOGD("Set WMI_AP_PS_PEER_PARAM_MAX_SP 0x%x for %pM",
		max_sp_len, peer_addr);

	ret = wmi_unified_set_ap_ps_param(wma, vdev_id,
					peer_addr,
					WMI_AP_PS_PEER_PARAM_MAX_SP,
					max_sp_len);
	if (ret) {
		WMA_LOGE("Failed to set WMI_AP_PS_PEER_PARAM_MAX_SP for %pM",
			 peer_addr);
		return ret;
	}
	return 0;
}

static void wma_add_sta_req_ap_mode(tp_wma_handle wma, tpAddStaParams add_sta)
{
	enum ol_txrx_peer_state state = ol_txrx_peer_state_conn;
	ol_txrx_pdev_handle pdev;
	ol_txrx_vdev_handle vdev;
	ol_txrx_peer_handle peer;
	u_int8_t peer_id;
	VOS_STATUS status;
	int32_t ret;
#ifdef WLAN_FEATURE_11W
	struct wma_txrx_node *iface = NULL;
#endif /* WLAN_FEATURE_11W */

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to find pdev", __func__);
		add_sta->status = VOS_STATUS_E_FAILURE;
		goto send_rsp;
	}
	/* UMAC sends WDA_ADD_STA_REQ msg twice to WMA when the station
	 * associates. First WDA_ADD_STA_REQ will have staType as
	 * STA_ENTRY_PEER and second posting will have STA_ENTRY_SELF.
	 * Peer creation is done in first WDA_ADD_STA_REQ and second
	 * WDA_ADD_STA_REQ which has STA_ENTRY_SELF is ignored and
	 * send fake response with success to UMAC. Otherwise UMAC
	 * will get blocked.
	 */
	if (add_sta->staType != STA_ENTRY_PEER) {
		add_sta->status = VOS_STATUS_SUCCESS;
		goto send_rsp;
	}

	vdev = wma_find_vdev_by_id(wma, add_sta->smesessionId);
	if (!vdev) {
		WMA_LOGE("%s: Failed to find vdev", __func__);
		add_sta->status = VOS_STATUS_E_FAILURE;
		goto send_rsp;
	}

	peer = ol_txrx_find_peer_by_addr_and_vdev(pdev,
                                                  vdev,
                                                  add_sta->staMac,
				                  &peer_id);
	if (peer) {
		wma_remove_peer(wma, add_sta->staMac, add_sta->smesessionId,
				peer, VOS_FALSE);
		WMA_LOGE("%s: Peer already exists, Deleted peer with peer_addr %pM",
			 __func__, add_sta->staMac);
	}

	status = wma_create_peer(wma, pdev, vdev, add_sta->staMac,
		         WMI_PEER_TYPE_DEFAULT, add_sta->smesessionId,
			 VOS_FALSE);
	if (status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("%s: Failed to create peer for %pM",
			 __func__, add_sta->staMac);
		add_sta->status = status;
		goto send_rsp;
	}

	peer = ol_txrx_find_peer_by_addr_and_vdev(pdev,
                                                  vdev,
                                                  add_sta->staMac,
                                                  &peer_id);
	if (!peer) {
		WMA_LOGE("%s: Failed to find peer handle using peer mac %pM",
			 __func__, add_sta->staMac);
		add_sta->status = VOS_STATUS_E_FAILURE;
		wma_remove_peer(wma, add_sta->staMac, add_sta->smesessionId,
				peer, VOS_FALSE);
		goto send_rsp;
	}

	wmi_unified_send_txbf(wma, add_sta);

	ret = wmi_unified_send_peer_assoc(wma, add_sta->nwType, add_sta);
	if (ret) {
		add_sta->status = VOS_STATUS_E_FAILURE;
		wma_remove_peer(wma, add_sta->staMac, add_sta->smesessionId,
				peer, VOS_FALSE);
		goto send_rsp;
	}

#ifdef QCA_IBSS_SUPPORT
	/*
	 * In IBSS mode send the peer
	 * Atim Window length if IBSS
	 * power save is enabled by the
	 * firmware.
	 */
	if ( wma_is_vdev_in_ibss_mode(wma, add_sta->smesessionId) &&
		  WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
					WMI_SERVICE_IBSS_PWRSAVE) ) {
		/*
		 * If ATIM Window is present in the peer
		 * beacon then send it to firmware else
		 * configure Zero ATIM Window length to
		 * firmware.
		 */
		if(add_sta->atimIePresent) {
			wma_set_peer_param(wma, add_sta->staMac,
				WMI_PEER_IBSS_ATIM_WINDOW_LENGTH,
				add_sta->peerAtimWindowLength,
				add_sta->smesessionId);
		}
		else {
			wma_set_peer_param(wma, add_sta->staMac,
				WMI_PEER_IBSS_ATIM_WINDOW_LENGTH,
				0,
				add_sta->smesessionId);
		}
	}
#endif

#ifdef WLAN_FEATURE_11W
	if (add_sta->rmfEnabled) {
		/*
		 * We have to store the state of PMF connection
		 * per STA for SAP case
		 * We will isolate the ifaces based on vdevid
		 */
		iface = &wma->interfaces[vdev->vdev_id];
		iface->rmfEnabled = add_sta->rmfEnabled;
		/*
		 * when 802.11w PMF is enabled for hw encr/decr
		 * use hw MFP Qos bits 0x10
		 */
		ret = wmi_unified_pdev_set_param(wma->wmi_handle,
				WMI_PDEV_PARAM_PMF_QOS, TRUE);
		if(ret) {
			WMA_LOGE("%s: Failed to set QOS MFP/PMF (%d)",
				__func__, ret);
		}
		else {
			WMA_LOGI("%s: QOS MFP/PMF set to %d",
				__func__, TRUE);
		}
	}
#endif /* WLAN_FEATURE_11W */

	if (add_sta->uAPSD) {
		ret = wma_set_ap_peer_uapsd(wma, add_sta->smesessionId,
					add_sta->staMac,
					add_sta->uAPSD,
					add_sta->maxSPLen);
		if (ret) {
			WMA_LOGE("Failed to set peer uapsd param for %pM",
				 add_sta->staMac);
			add_sta->status = VOS_STATUS_E_FAILURE;
			wma_remove_peer(wma, add_sta->staMac,
					add_sta->smesessionId, peer,
					VOS_FALSE);
			goto send_rsp;
		}
	}

	WMA_LOGD("%s: Moving peer %pM to state %d",
		 __func__, add_sta->staMac, state);
	ol_txrx_peer_state_update(pdev, add_sta->staMac, state);

	add_sta->staIdx = ol_txrx_local_peer_id(peer);
	add_sta->status = VOS_STATUS_SUCCESS;
send_rsp:
	WMA_LOGD("%s: Sending add sta rsp to umac (mac:%pM, status:%d)",
		__func__, add_sta->staMac, add_sta->status);
	wma_send_msg(wma, WDA_ADD_STA_RSP, (void *)add_sta, 0);
}

static int wmi_unified_nat_keepalive_enable(tp_wma_handle wma,
		u_int8_t vdev_id)
{
	WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMD_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	WMA_LOGD("%s: vdev_id %d", __func__, vdev_id);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}
	cmd = (WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMD_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMD_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMD_fixed_param));
	cmd->vdev_id = vdev_id;
	cmd->action = IPSEC_NATKEEPALIVE_FILTER_ENABLE;
	if (wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMDID)) {
		WMA_LOGP("%s: Failed to send NAT keepalive enable command",
				__func__);
		wmi_buf_free(buf);
		return -EIO;
	}
	return 0;
}

static int wmi_unified_csa_offload_enable(tp_wma_handle wma,
		u_int8_t vdev_id)
{
	wmi_csa_offload_enable_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	WMA_LOGD("%s: vdev_id %d", __func__, vdev_id);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}
	cmd = (wmi_csa_offload_enable_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_csa_offload_enable_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_csa_offload_enable_cmd_fixed_param));
	cmd->vdev_id = vdev_id;
	cmd->csa_offload_enable = WMI_CSA_OFFLOAD_ENABLE;
	if (wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_CSA_OFFLOAD_ENABLE_CMDID)) {
		WMA_LOGP("%s: Failed to send CSA offload enable command",
				__func__);
		wmi_buf_free(buf);
		return -EIO;
	}
	return 0;
}

#ifdef  QCA_IBSS_SUPPORT

static u_int16_t wma_calc_ibss_heart_beat_timer(int16_t peer_num)
{
        /* heart beat timer value look-up table */
        /* entry index : (the number of currently connected peers) - 1
           entry value : the heart time threshold value in seconds for
                         detecting ibss peer departure */
        static const u_int16_t heart_beat_timer[HDD_MAX_NUM_IBSS_STA] = {
                 4,  4,  4,  4,  4,  4,  4,  4,
                 8,  8,  8,  8,  8,  8,  8,  8,
                12, 12, 12, 12, 12, 12, 12, 12,
                16, 16, 16, 16, 16, 16, 16, 16};

        if (peer_num < 1 || peer_num > HDD_MAX_NUM_IBSS_STA)
                return 0;

        return heart_beat_timer[peer_num - 1];

}

static void wma_adjust_ibss_heart_beat_timer(tp_wma_handle wma,
                                             u_int8_t      vdev_id,
                                             int8_t        peer_num_delta)
{
        ol_txrx_vdev_handle vdev;
        int16_t             new_peer_num;
        u_int16_t           new_timer_value_sec;
        u_int32_t           new_timer_value_ms;

        if (peer_num_delta != 1 && peer_num_delta != -1) {
                WMA_LOGE("Invalid peer_num_delta value %d", peer_num_delta);
                return;
        }

        vdev = wma_find_vdev_by_id(wma, vdev_id);
        if (!vdev) {
                WMA_LOGE("vdev not found : vdev_id %d", vdev_id);
                return;
        }

        new_peer_num = vdev->ibss_peer_num + peer_num_delta;
        if (new_peer_num > HDD_MAX_NUM_IBSS_STA || new_peer_num < 0) {
                WMA_LOGE("new peer num %d out of valid boundary", new_peer_num);
                return;
        }

        /* adjust peer numbers */
        vdev->ibss_peer_num = new_peer_num;

        /* reset timer value if all peers departed */
        if (new_peer_num == 0) {
                vdev->ibss_peer_heart_beat_timer = 0;
                return;
        }

        /* calculate new timer value */
        new_timer_value_sec = wma_calc_ibss_heart_beat_timer(new_peer_num);
        if (new_timer_value_sec == 0) {
                WMA_LOGE("timer value %d is invalid for peer number %d",
                        new_timer_value_sec, new_peer_num);
                return;
        }
        if (new_timer_value_sec == vdev->ibss_peer_heart_beat_timer) {
                WMA_LOGD("timer value %d stays same, no need to notify target",
                                new_timer_value_sec);
                return;
        }

        /* send new timer value to target */
        vdev->ibss_peer_heart_beat_timer = new_timer_value_sec;

        new_timer_value_ms = ((u_int32_t)new_timer_value_sec) * 1000;

        if (wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
                                            WMI_VDEV_PARAM_IBSS_MAX_BCN_LOST_MS,
                                            new_timer_value_ms)) {
                WMA_LOGE("Failed to set IBSS link monitoring timer value");
                return;
        }

        WMA_LOGD("Set IBSS link monitor timer: peer_num = %d timer_value = %d",
                new_peer_num, new_timer_value_ms);
}

#endif /* QCA_IBSS_SUPPORT */

#ifdef FEATURE_WLAN_TDLS
static void wma_add_tdls_sta(tp_wma_handle wma, tpAddStaParams add_sta)
{
	ol_txrx_pdev_handle pdev;
	ol_txrx_vdev_handle vdev;
	ol_txrx_peer_handle peer;
	u_int8_t peer_id;
	VOS_STATUS status;
	int32_t ret;
	tTdlsPeerStateParams *peerStateParams;

	WMA_LOGD("%s: staType: %d, staIdx: %d, updateSta: %d, "
	         "bssId: %pM, staMac: %pM",
	         __func__, add_sta->staType, add_sta->staIdx,
	         add_sta->updateSta, add_sta->bssId, add_sta->staMac);

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to find pdev", __func__);
		add_sta->status = VOS_STATUS_E_FAILURE;
		goto send_rsp;
	}

	vdev = wma_find_vdev_by_id(wma, add_sta->smesessionId);
	if (!vdev) {
	 WMA_LOGE("%s: Failed to find vdev", __func__);
	 add_sta->status = VOS_STATUS_E_FAILURE;
	 goto send_rsp;
	 }

	if (0 == add_sta->updateSta) {
	 /* its a add sta request **/
#if defined(CONFIG_HL_SUPPORT)
         if (add_sta->bssId && vdev->last_real_peer &&
            (adf_os_mem_cmp((u8 *)add_sta->bssId,
                             vdev->last_real_peer->mac_addr.raw,
                             IEEE80211_ADDR_LEN) == 0)) {
            adf_os_mem_copy(vdev->hl_tdls_ap_mac_addr.raw,
                            vdev->last_real_peer->mac_addr.raw,
                            OL_TXRX_MAC_ADDR_LEN);
         }
#endif
         WMA_LOGD("%s: addSta, calling wma_create_peer for %pM, vdev_id %hu",
	          __func__, add_sta->staMac, add_sta->smesessionId);

	 status = wma_create_peer(wma, pdev, vdev, add_sta->staMac,
		               WMI_PEER_TYPE_TDLS, add_sta->smesessionId,
			       VOS_FALSE);
	 if (status != VOS_STATUS_SUCCESS) {
	  WMA_LOGE("%s: Failed to create peer for %pM",
	           __func__, add_sta->staMac);
	  add_sta->status = status;
	  goto send_rsp;
	 }

	 peer = ol_txrx_find_peer_by_addr(pdev, add_sta->staMac, &peer_id);
	 if (!peer) {
	  WMA_LOGE("%s: addSta, failed to find peer handle for mac %pM",
	           __func__, add_sta->staMac);
	  add_sta->status = VOS_STATUS_E_FAILURE;
	  wma_remove_peer(wma, add_sta->staMac, add_sta->smesessionId, peer,
			  VOS_FALSE);
#if defined(CONFIG_HL_SUPPORT)
          if (vdev->last_real_peer == NULL) {
              peer = NULL;
              peer = ol_txrx_find_peer_by_addr(pdev, vdev->hl_tdls_ap_mac_addr.raw, &peer_id);
              if (peer && (peer->peer_ids[0] != HTT_INVALID_PEER_ID))
                  vdev->last_real_peer = peer;
          }
#endif
          goto send_rsp;
	 }

	 add_sta->staIdx = ol_txrx_local_peer_id(peer);
	 WMA_LOGD("%s: addSta, after calling ol_txrx_local_peer_id, "
	          "staIdx: %d, staMac: %pM",
	          __func__, add_sta->staIdx, add_sta->staMac);

	 peerStateParams = vos_mem_malloc(sizeof(tTdlsPeerStateParams));
	 if (!peerStateParams) {
	  WMA_LOGE("%s: Failed to allocate memory for peerStateParams for %pM",
	           __func__, add_sta->staMac);
	  add_sta->status = VOS_STATUS_E_FAILURE;
	  goto send_rsp;
	 }

	 vos_mem_zero(peerStateParams, sizeof(*peerStateParams));
	 peerStateParams->peerState = WMI_TDLS_PEER_STATE_PEERING;
	 peerStateParams->vdevId = vdev->vdev_id;
	 vos_mem_copy(&peerStateParams->peerMacAddr,
	              &add_sta->staMac,
	              sizeof(tSirMacAddr));
	 wma_update_tdls_peer_state(wma, peerStateParams);
	} else {
	 /* its a change sta request **/
	 peer = ol_txrx_find_peer_by_addr(pdev, add_sta->staMac, &peer_id);
	 if (!peer) {
	  WMA_LOGE("%s: changeSta,failed to find peer handle for mac %pM",
	           __func__, add_sta->staMac);
	  add_sta->status = VOS_STATUS_E_FAILURE;
	  wma_remove_peer(wma, add_sta->staMac, add_sta->smesessionId, peer,
			  VOS_FALSE);
#if defined(CONFIG_HL_SUPPORT)
          if (vdev->last_real_peer == NULL) {
              peer = NULL;
              peer = ol_txrx_find_peer_by_addr(pdev, vdev->hl_tdls_ap_mac_addr.raw, &peer_id);
              if (peer && (peer->peer_ids[0] != HTT_INVALID_PEER_ID))
                  vdev->last_real_peer = peer;
          }
#endif
          goto send_rsp;
	 }

	 WMA_LOGD("%s: changeSta, calling wmi_unified_send_peer_assoc",
	          __func__);

	 ret = wmi_unified_send_peer_assoc(wma, add_sta->nwType, add_sta);
	 if (ret) {
	  add_sta->status = VOS_STATUS_E_FAILURE;
	  wma_remove_peer(wma, add_sta->staMac, add_sta->smesessionId, peer,
			  VOS_FALSE);
#if defined(CONFIG_HL_SUPPORT)
          if (vdev->last_real_peer == NULL) {
              peer = NULL;
              peer = ol_txrx_find_peer_by_addr(pdev, vdev->hl_tdls_ap_mac_addr.raw, &peer_id);
              if (peer && (peer->peer_ids[0] != HTT_INVALID_PEER_ID))
                  vdev->last_real_peer = peer;
          }
#endif
          goto send_rsp;
	 }
	}

send_rsp:
	WMA_LOGD("%s: Sending add sta rsp to umac (mac:%pM, status:%d), "
	         "staType: %d, staIdx: %d, updateSta: %d",
	         __func__, add_sta->staMac, add_sta->status,
	         add_sta->staType, add_sta->staIdx, add_sta->updateSta);
	wma_send_msg(wma, WDA_ADD_STA_RSP, (void *)add_sta, 0);
}
#endif

static void wma_add_sta_req_sta_mode(tp_wma_handle wma, tpAddStaParams params)
{
	ol_txrx_pdev_handle pdev;
	VOS_STATUS status = VOS_STATUS_SUCCESS;
	ol_txrx_peer_handle peer;
	struct wma_txrx_node *iface = NULL;
	tPowerdBm maxTxPower;
        int ret = 0;

#ifdef FEATURE_WLAN_TDLS
	if (STA_ENTRY_TDLS_PEER == params->staType)
	{
	 wma_add_tdls_sta(wma, params);
	 return;
	}
#endif

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Unable to get pdev", __func__);
		goto out;
	}

	iface = &wma->interfaces[params->smesessionId];
	if (params->staType != STA_ENTRY_SELF) {
		WMA_LOGP("%s: unsupported station type %d",
			 __func__, params->staType);
		goto out;
	}
	peer = ol_txrx_find_peer_by_addr(pdev, params->bssId, &params->staIdx);
	if(params->nonRoamReassoc) {
		ol_txrx_peer_state_update(pdev, params->bssId,
			ol_txrx_peer_state_auth);
		adf_os_atomic_set(&iface->bss_status, WMA_BSS_STATUS_STARTED);
		iface->aid = params->assocId;
		goto out;
	}
	if (wma->interfaces[params->smesessionId].vdev_up == true) {
		WMA_LOGE("%s: vdev id %d is already UP for %pM", __func__,
			params->smesessionId, params->bssId);
		status = VOS_STATUS_E_FAILURE;
		goto out;
	}
	if (peer != NULL && peer->state == ol_txrx_peer_state_disc) {
		/*
		 * This is the case for reassociation.
		 * peer state update and peer_assoc is required since it
		 * was not done by WDA_ADD_BSS_REQ.
		 */

		/* Update peer state */
		if (params->encryptType == eSIR_ED_NONE) {
			WMA_LOGD("%s: Update peer(%pM) state into auth",
				 __func__, params->bssId);
			ol_txrx_peer_state_update(pdev, params->bssId,
						  ol_txrx_peer_state_auth);
		} else {
			WMA_LOGD("%s: Update peer(%pM) state into conn",
				 __func__, params->bssId);
			ol_txrx_peer_state_update(pdev, params->bssId,
						  ol_txrx_peer_state_conn);
		}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
		if(iface->roam_synch_in_progress)
		{
			// iface->nss = params->nss;
			/*In LFR2.0, the following operations are performed as
			 * part of wmi_unified_send_peer_assoc. As we are
			 * skipping this operation, we are just executing the
			 * following which are useful for LFR3.0.*/
			ol_txrx_peer_state_update(pdev, params->bssId,
					ol_txrx_peer_state_auth);
			adf_os_atomic_set(&iface->bss_status, WMA_BSS_STATUS_STARTED);
			iface->aid = params->assocId;
			goto out;
		}
#endif
		wmi_unified_send_txbf(wma, params);

		ret = wmi_unified_send_peer_assoc(wma,
                        iface->nwType,
                        (tAddStaParams *)iface->addBssStaContext);
		if (ret) {
			status = VOS_STATUS_E_FAILURE;
			wma_remove_peer(wma, params->bssId,
					params->smesessionId, peer, VOS_FALSE);
			goto out;
		}

#ifdef WLAN_FEATURE_11W
		if (params->rmfEnabled) {
			/* when 802.11w PMF is enabled for hw encr/decr
			   use hw MFP Qos bits 0x10 */
			ret = wmi_unified_pdev_set_param(wma->wmi_handle,
					WMI_PDEV_PARAM_PMF_QOS, TRUE);
			if(ret) {
				WMA_LOGE("%s: Failed to set QOS MFP/PMF (%d)",
				__func__, ret);
			} else {
				WMA_LOGI("%s: QOS MFP/PMF set to %d",
				 __func__, TRUE);
			}
		}
#endif /* WLAN_FEATURE_11W */
#if defined WLAN_FEATURE_VOWIFI_11R
             /*
              * Set the PTK in 11r mode because we already have it.
              */
              if (iface->staKeyParams) {
                  wma_set_stakey(wma, (tpSetStaKeyParams) iface->staKeyParams);
             }
#endif
	}
#if defined WLAN_FEATURE_VOWIFI
	maxTxPower = params->maxTxPower;
#else
	maxTxPower = 0;
#endif
	wma_vdev_set_bss_params(wma, params->smesessionId, iface->beaconInterval,
				iface->dtimPeriod, iface->shortSlotTimeSupported,
				iface->llbCoexist, maxTxPower);

	params->csaOffloadEnable = 0;
	if (WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
				WMI_SERVICE_CSA_OFFLOAD)) {
		params->csaOffloadEnable = 1;
		if (wmi_unified_csa_offload_enable(wma, params->smesessionId) < 0) {
			WMA_LOGE("Unable to enable CSA offload for vdev_id:%d",
					params->smesessionId);
		}
	}

	if (WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
				WMI_SERVICE_FILTER_IPSEC_NATKEEPALIVE)) {
		if (wmi_unified_nat_keepalive_enable(wma, params->smesessionId) < 0) {
			WMA_LOGE("Unable to enable NAT keepalive for vdev_id:%d",
					params->smesessionId);
		}
	}

	if (wmi_unified_vdev_up_send(wma->wmi_handle, params->smesessionId,
				     params->assocId, params->bssId) < 0) {
		WMA_LOGP("%s: Failed to send vdev up cmd: vdev %d bssid %pM",
			 __func__, params->smesessionId, params->bssId);
		status = VOS_STATUS_E_FAILURE;
	}
	else {
		wma->interfaces[params->smesessionId].vdev_up = TRUE;
	}

	adf_os_atomic_set(&iface->bss_status, WMA_BSS_STATUS_STARTED);
	WMA_LOGD("%s: STA mode (type %d subtype %d) BSS is started",
		 __func__, iface->type, iface->sub_type);
        /* Sta is now associated, configure various params */

        /* SM power save, configure the h/w as configured
         * in the ini file. SMPS is not published in assoc
         * request. Once configured, fw sends the required
         * action frame to AP.
         */
        if (params->enableHtSmps)
            wma_set_mimops(wma, params->smesessionId,
                           params->htSmpsconfig);

#ifdef WLAN_FEATURE_11AC
        /* Partial AID match power save, enable when SU bformee*/
        if (params->enableVhtpAid && params->vhtTxBFCapable)
            wma_set_ppsconfig(params->smesessionId,
                              WMA_VHT_PPS_PAID_MATCH, 1);
#endif

        /* Enable AMPDU power save, if htCapable/vhtCapable */
        if (params->enableAmpduPs &&
                   (params->htCapable || params->vhtCapable))
               wma_set_ppsconfig(params->smesessionId,
                                 WMA_VHT_PPS_DELIM_CRC_FAIL, 1);
   iface->aid = params->assocId;
out:
	params->status = status;
/* change logging before release */
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
        if(iface && iface->roam_synch_in_progress)
	WMA_LOGD("%s:statype %d vdevid %d aid %d bssid %pM staIdx %d status %d",
		 __func__, params->staType, params->smesessionId,
		 params->assocId, params->bssId, params->staIdx, status);
	else
	WMA_LOGE("%s:statype %d vdevid %d aid %d bssid %pM staIdx %d status %d",
		 __func__, params->staType, params->smesessionId,
		 params->assocId, params->bssId, params->staIdx, status);
#endif
	wma_send_msg(wma, WDA_ADD_STA_RSP, (void *)params, 0);
}

#ifdef FEATURE_WLAN_D0WOW
static void wma_add_pm_vote(tp_wma_handle wma)
{
	if (++wma->ap_client_cnt == 1) {
		htc_pm_runtime_get(wma->htc_handle);
		vos_pm_control(DISABLE_PCIE_POWER_COLLAPSE);
	}
}

static void wma_del_pm_vote(tp_wma_handle wma)
{
	if (--wma->ap_client_cnt == 0) {
		htc_pm_runtime_put(wma->htc_handle);
		vos_pm_control(ENABLE_PCIE_POWER_COLLAPSE);
	}
}

int wma_get_client_count(WMA_HANDLE handle)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	return wma->ap_client_cnt;
}
#else
static void wma_prevent_suspend_check(tp_wma_handle wma)
{
	wma->ap_client_cnt++;
	if (wma->ap_client_cnt ==
	    wma->wlan_resource_config.num_offload_peers) {
		vos_wake_lock_acquire(&wma->wow_wake_lock,
				WIFI_POWER_EVENT_WAKELOCK_ADD_STA);
		WMA_LOGW("%s: %d clients connected, prevent suspend",
			 __func__, wma->ap_client_cnt);
	}
}

static void wma_allow_suspend_check(tp_wma_handle wma)
{
	wma->ap_client_cnt--;
	if (wma->ap_client_cnt ==
	    wma->wlan_resource_config.num_offload_peers - 1) {
		vos_wake_lock_release(&wma->wow_wake_lock,
                                      WIFI_POWER_EVENT_WAKELOCK_DEL_STA);
		WMA_LOGW("%s: %d clients connected, allow suspend",
			 __func__, wma->ap_client_cnt);
	}
}
#endif /* FEATURE_WLAN_D0WOW */

static void wma_add_sta(tp_wma_handle wma, tpAddStaParams add_sta)
{
	tANI_U8 oper_mode = BSS_OPERATIONAL_MODE_STA;

	WMA_LOGD("%s: add_sta->sessionId = %d.", __func__, add_sta->smesessionId);
	WMA_LOGD("%s: add_sta->bssId = %x:%x:%x:%x:%x:%x", __func__,
                 add_sta->bssId[0], add_sta->bssId[1], add_sta->bssId[2],
                 add_sta->bssId[3], add_sta->bssId[4], add_sta->bssId[5]);

	if (wma_is_vdev_in_ap_mode(wma, add_sta->smesessionId)) {
#ifdef FEATURE_WLAN_D0WOW
		wma_add_pm_vote(wma);
#else
		wma_prevent_suspend_check(wma);
#endif
		oper_mode = BSS_OPERATIONAL_MODE_AP;
	}
#ifdef QCA_IBSS_SUPPORT
	else if (wma_is_vdev_in_ibss_mode(wma, add_sta->smesessionId)) {
		oper_mode = BSS_OPERATIONAL_MODE_IBSS;
#ifdef FEATURE_WLAN_D0WOW
		wma_add_pm_vote(wma);
#endif
	}
#endif

	switch (oper_mode) {
	case BSS_OPERATIONAL_MODE_STA:
		wma_add_sta_req_sta_mode(wma, add_sta);
		break;

#ifdef QCA_IBSS_SUPPORT
	case BSS_OPERATIONAL_MODE_IBSS: /* IBSS should share the same code as AP mode */
#endif
	case BSS_OPERATIONAL_MODE_AP:
		wma_add_sta_req_ap_mode(wma, add_sta);
		break;
	}

#ifdef QCA_IBSS_SUPPORT
        /* adjust heart beat thresold timer value for detecting ibss peer departure */
        if (oper_mode == BSS_OPERATIONAL_MODE_IBSS)
                wma_adjust_ibss_heart_beat_timer(wma, add_sta->smesessionId, 1);
#endif

}

/*
 * This function reads WEP keys from cfg and fills
 * up key_info.
 */
static void wma_read_cfg_wepkey(tp_wma_handle wma_handle,
				tSirKeys *key_info, v_U32_t *def_key_idx,
				u_int8_t *num_keys)
{
	tSirRetStatus status;
	v_U32_t val = SIR_MAC_KEY_LENGTH;
	u_int8_t i, j;

	WMA_LOGD("Reading WEP keys from cfg");
	/* NOTE:def_key_idx is initialized to 0 by the caller */
	status = wlan_cfgGetInt(wma_handle->mac_context,
				WNI_CFG_WEP_DEFAULT_KEYID, def_key_idx);
	if (status != eSIR_SUCCESS)
		WMA_LOGE("Unable to read default id, defaulting to 0");

	for (i = 0, j = 0; i < SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS; i++) {
		status = wlan_cfgGetStr(wma_handle->mac_context,
				(u_int16_t) WNI_CFG_WEP_DEFAULT_KEY_1 + i,
				key_info[j].key, &val);
		if (status != eSIR_SUCCESS) {
			WMA_LOGE("WEP key is not configured at :%d", i);
		} else {
			key_info[j].keyId = i;
			key_info[j].keyLength = (u_int16_t) val;
			j++;
		}
	}
	*num_keys = j;
}

/*
 * This function setsup wmi buffer from information
 * passed in key_params.
 */
static wmi_buf_t wma_setup_install_key_cmd(tp_wma_handle wma_handle,
				struct wma_set_key_params *key_params,
				u_int32_t *len)
{
	wmi_vdev_install_key_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	u_int8_t *key_data;
#ifdef WLAN_FEATURE_11W
	struct wma_txrx_node *iface = NULL;
#endif /* WLAN_FEATURE_11W */
	if ((key_params->key_type == eSIR_ED_NONE &&
	    key_params->key_len) || (key_params->key_type != eSIR_ED_NONE &&
	    !key_params->key_len)) {
		WMA_LOGE("%s:Invalid set key request", __func__);
		return NULL;
	}

	*len = sizeof(*cmd) + roundup(key_params->key_len, sizeof(u_int32_t)) +
		WMI_TLV_HDR_SIZE;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, *len);
	if (!buf) {
		WMA_LOGE("Failed to allocate buffer to send set key cmd");
		return NULL;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_vdev_install_key_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_install_key_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_vdev_install_key_cmd_fixed_param));
	cmd->vdev_id = key_params->vdev_id;
	cmd->key_ix = key_params->key_idx;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(key_params->peer_mac,
				   &cmd->peer_macaddr);
	if (key_params->unicast)
		cmd->key_flags |= PAIRWISE_USAGE;
	else
		cmd->key_flags |= GROUP_USAGE;

	switch (key_params->key_type) {
	case eSIR_ED_NONE:
		cmd->key_cipher = WMI_CIPHER_NONE;
		break;
	case eSIR_ED_WEP40:
	case eSIR_ED_WEP104:
		cmd->key_cipher = WMI_CIPHER_WEP;
		if (key_params->unicast &&
		    cmd->key_ix == key_params->def_key_idx)
			cmd->key_flags |= TX_USAGE;
		break;
	case eSIR_ED_TKIP:
		cmd->key_txmic_len = WMA_TXMIC_LEN;
		cmd->key_rxmic_len = WMA_RXMIC_LEN;
		cmd->key_cipher = WMI_CIPHER_TKIP;
		break;
#ifdef FEATURE_WLAN_WAPI
#define WPI_IV_LEN 16
	case eSIR_ED_WPI:
	{
		/*initialize receive and transmit IV with default values*/
		unsigned char tx_iv[16] = {0x36,0x5c,0x36,0x5c,0x36,0x5c,0x36,
					   0x5c,0x36,0x5c,0x36,0x5c,0x36,0x5c,
					   0x36,0x5c};
		unsigned char rx_iv[16] = {0x5c,0x36,0x5c,0x36,0x5c,0x36,0x5c,
					   0x36,0x5c,0x36,0x5c,0x36,0x5c,0x36,
					   0x5c,0x37};
		cmd->key_txmic_len = WMA_TXMIC_LEN;
		cmd->key_rxmic_len = WMA_RXMIC_LEN;
		/*Authenticator initializes the value of PN as
		 *0x5C365C365C365C365C365C365C365C36 for multicast key update.
		 */
		if (!key_params->unicast)
			rx_iv[WPI_IV_LEN - 1] = 0x36;

		vos_mem_copy(&cmd->wpi_key_rsc_counter, &rx_iv, WPI_IV_LEN);
		vos_mem_copy(&cmd->wpi_key_tsc_counter, &tx_iv, WPI_IV_LEN);
		cmd->key_cipher = WMI_CIPHER_WAPI;
		break;
	}
#endif
	case eSIR_ED_CCMP:
		cmd->key_cipher = WMI_CIPHER_AES_CCM;
		break;
#ifdef WLAN_FEATURE_11W
	case eSIR_ED_AES_128_CMAC:
		cmd->key_cipher = WMI_CIPHER_AES_CMAC;
		break;
#endif /* WLAN_FEATURE_11W */
	default:
		/* TODO: MFP ? */
		WMA_LOGE("%s:Invalid encryption type:%d", __func__, key_params->key_type);
		adf_nbuf_free(buf);
		return NULL;
	}

	buf_ptr += sizeof(wmi_vdev_install_key_cmd_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE,
		       roundup(key_params->key_len, sizeof(u_int32_t)));
	key_data = (A_UINT8*)(buf_ptr + WMI_TLV_HDR_SIZE);
#ifdef BIG_ENDIAN_HOST
	{
		/* for big endian host, copy engine byte_swap is enabled
		 * But the key data content is in network byte order
		 * Need to byte swap the key data content - so when copy engine
		 * does byte_swap - target gets key_data content in the correct
		 * order.
		 */
		int8_t i;
		u_int32_t *destp, *srcp;

		destp = (u_int32_t *) key_data;
		srcp =  (u_int32_t *) key_params->key_data;
		for(i = 0;
		    i < roundup(key_params->key_len, sizeof(u_int32_t)) / 4;
		    i++) {
			*destp = le32_to_cpu(*srcp);
			destp++;
			srcp++;
		}
	}
#else
	vos_mem_copy((void *) key_data,
		     (const void *) key_params->key_data,
		     key_params->key_len);
#endif
	cmd->key_len = key_params->key_len;

#ifdef WLAN_FEATURE_11W
	if (key_params->key_type == eSIR_ED_AES_128_CMAC)
	{
		iface = &wma_handle->interfaces[key_params->vdev_id];
		if (iface) {
			iface->key.key_length = key_params->key_len;
			vos_mem_copy (iface->key.key,
					(const void *) key_params->key_data,
					iface->key.key_length);
			if ((cmd->key_ix == WMA_IGTK_KEY_INDEX_4) ||
				(cmd->key_ix == WMA_IGTK_KEY_INDEX_5))
				vos_mem_zero (iface->key.key_id[cmd->key_ix - WMA_IGTK_KEY_INDEX_4].ipn,
						 CMAC_IPN_LEN);
		}
	}
#endif /* WLAN_FEATURE_11W */

	WMA_LOGD("Key setup : vdev_id %d key_idx %d key_type %d key_len %d"
		 " unicast %d peer_mac %pM def_key_idx %d", key_params->vdev_id,
		 key_params->key_idx, key_params->key_type, key_params->key_len,
		 key_params->unicast, key_params->peer_mac,
		 key_params->def_key_idx);

	return buf;
}

static void wma_set_bsskey(tp_wma_handle wma_handle, tpSetBssKeyParams key_info)
{
	struct wma_set_key_params key_params;
	wmi_buf_t buf;
	int32_t status;
	u_int32_t len = 0, i;
	v_U32_t def_key_idx = 0;
	ol_txrx_vdev_handle txrx_vdev;

	WMA_LOGD("BSS key setup");
	txrx_vdev = wma_find_vdev_by_id(wma_handle, key_info->smesessionId);
	if (!txrx_vdev) {
		WMA_LOGE("%s:Invalid vdev handle", __func__);
		key_info->status = eHAL_STATUS_FAILURE;
		goto out;
	}

        /*
        ** For IBSS, WMI expects the BSS key to be set per peer key
        ** So cache the BSS key in the wma_handle and re-use it when the STA key is been setup for a peer
        */
        if (wlan_op_mode_ibss == txrx_vdev->opmode) {
          key_info->status = eHAL_STATUS_SUCCESS;
          if (wma_handle->ibss_started > 0)
             goto out;
          WMA_LOGD("Caching IBSS Key");
          vos_mem_copy(&wma_handle->ibsskey_info, key_info, sizeof(tSetBssKeyParams));
        }

	adf_os_mem_set(&key_params, 0, sizeof(key_params));
	key_params.vdev_id = key_info->smesessionId;
	key_params.key_type = key_info->encType;
	key_params.singl_tid_rc = key_info->singleTidRc;
	key_params.unicast = FALSE;
	if (txrx_vdev->opmode == wlan_op_mode_sta) {
		vos_mem_copy(key_params.peer_mac,
			wma_handle->interfaces[key_info->smesessionId].bssid,
			ETH_ALEN);
	} else {
               /* vdev mac address will be passed for all other modes */
		vos_mem_copy(key_params.peer_mac, txrx_vdev->mac_addr.raw,
			     ETH_ALEN);
		WMA_LOGA("BSS Key setup with vdev_mac %pM\n",
			 txrx_vdev->mac_addr.raw);
        }

	if (key_info->numKeys == 0 &&
	    (key_info->encType == eSIR_ED_WEP40 ||
	     key_info->encType == eSIR_ED_WEP104)) {
		wma_read_cfg_wepkey(wma_handle, key_info->key,
				    &def_key_idx, &key_info->numKeys);
	}

	for (i = 0; i < key_info->numKeys; i++) {
		if (key_params.key_type != eSIR_ED_NONE &&
		    !key_info->key[i].keyLength)
			continue;
		if (key_info->encType == eSIR_ED_WPI) {
			key_params.key_idx = key_info->key[i].keyId;
			key_params.def_key_idx = key_info->key[i].keyId;
		} else
			key_params.key_idx = key_info->key[i].keyId;

		key_params.key_len = key_info->key[i].keyLength;
		if (key_info->encType == eSIR_ED_TKIP) {
			vos_mem_copy(key_params.key_data,
				     key_info->key[i].key, 16);
			vos_mem_copy(&key_params.key_data[16],
				     &key_info->key[i].key[24], 8);
			vos_mem_copy(&key_params.key_data[24],
				     &key_info->key[i].key[16], 8);
		} else
			vos_mem_copy((v_VOID_t *) key_params.key_data,
				     (const v_VOID_t *) key_info->key[i].key,
				     key_info->key[i].keyLength);

	        WMA_LOGD("%s: bss key[%d] length %d", __func__, i,
			key_info->key[i].keyLength);

		buf = wma_setup_install_key_cmd(wma_handle, &key_params, &len);
		if (!buf) {
			WMA_LOGE("%s:Failed to setup install key buf", __func__);
			key_info->status = eHAL_STATUS_FAILED_ALLOC;
			goto out;
		}

		status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
					      WMI_VDEV_INSTALL_KEY_CMDID);
		if (status) {
			adf_nbuf_free(buf);
			WMA_LOGE("%s:Failed to send install key command", __func__);
			key_info->status = eHAL_STATUS_FAILURE;
			goto out;
		}
	}

        wma_handle->ibss_started++;
	/* TODO: Should we wait till we get HTT_T2H_MSG_TYPE_SEC_IND? */
	key_info->status = eHAL_STATUS_SUCCESS;

out:
	wma_send_msg(wma_handle, WDA_SET_BSSKEY_RSP, (void *)key_info, 0);
}

static void wma_set_ibsskey_helper(tp_wma_handle wma_handle, tpSetBssKeyParams key_info, u_int8_t* peerMacAddr)
{
        struct wma_set_key_params key_params;
        wmi_buf_t buf;
        int32_t status;
        u_int32_t len = 0, i;
        v_U32_t def_key_idx = 0;
        ol_txrx_vdev_handle txrx_vdev;

        WMA_LOGD("BSS key setup for peer");
        ASSERT( NULL != peerMacAddr);
        txrx_vdev = wma_find_vdev_by_id(wma_handle, key_info->smesessionId);
        if (!txrx_vdev) {
                WMA_LOGE("%s:Invalid vdev handle", __func__);
                key_info->status = eHAL_STATUS_FAILURE;
               return;
        }

        adf_os_mem_set(&key_params, 0, sizeof(key_params));
        key_params.vdev_id = key_info->smesessionId;
        key_params.key_type = key_info->encType;
        key_params.singl_tid_rc = key_info->singleTidRc;
        key_params.unicast = FALSE;
        ASSERT(wlan_op_mode_ibss == txrx_vdev->opmode);

        vos_mem_copy(key_params.peer_mac, peerMacAddr, ETH_ALEN);

        if (key_info->numKeys == 0 &&
            (key_info->encType == eSIR_ED_WEP40 ||
             key_info->encType == eSIR_ED_WEP104)) {
                wma_read_cfg_wepkey(wma_handle, key_info->key,
                                    &def_key_idx, &key_info->numKeys);
        }

        for (i = 0; i < key_info->numKeys; i++) {
                if (key_params.key_type != eSIR_ED_NONE &&
                    !key_info->key[i].keyLength)
                        continue;
                key_params.key_idx = key_info->key[i].keyId;
                key_params.key_len = key_info->key[i].keyLength;
                if (key_info->encType == eSIR_ED_TKIP) {
                        vos_mem_copy(key_params.key_data,
                                     key_info->key[i].key, 16);
                        vos_mem_copy(&key_params.key_data[16],
                                     &key_info->key[i].key[24], 8);
                        vos_mem_copy(&key_params.key_data[24],
                                     &key_info->key[i].key[16], 8);
                } else
                        vos_mem_copy((v_VOID_t *) key_params.key_data,
                                     (const v_VOID_t *) key_info->key[i].key,
                                     key_info->key[i].keyLength);

                WMA_LOGD("%s: peer bcast key[%d] length %d", __func__, i,
			key_info->key[i].keyLength);

                buf = wma_setup_install_key_cmd(wma_handle, &key_params, &len);
                if (!buf) {
                        WMA_LOGE("%s:Failed to setup install key buf", __func__);
                        return;
                }

                status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
                                              WMI_VDEV_INSTALL_KEY_CMDID);
                if (status) {
                        adf_nbuf_free(buf);
                        WMA_LOGE("%s:Failed to send install key command", __func__);
                }
        }
}

static void wma_set_stakey(tp_wma_handle wma_handle, tpSetStaKeyParams key_info)
{
	wmi_buf_t buf;
	int32_t status, i;
	u_int32_t len = 0;
	ol_txrx_pdev_handle txrx_pdev;
	ol_txrx_vdev_handle txrx_vdev;
	struct ol_txrx_peer_t *peer;
	u_int8_t num_keys = 0, peer_id;
	struct wma_set_key_params key_params;
	v_U32_t def_key_idx = 0;

	WMA_LOGD("STA key setup");

	/* Get the txRx Pdev handle */
	txrx_pdev = vos_get_context(VOS_MODULE_ID_TXRX,
				    wma_handle->vos_context);
	if (!txrx_pdev) {
		WMA_LOGE("%s:Invalid txrx pdev handle", __func__);
		key_info->status = eHAL_STATUS_FAILURE;
		goto out;
	}

	peer = ol_txrx_find_peer_by_addr(txrx_pdev, key_info->peerMacAddr,
					 &peer_id);
	if (!peer) {
		WMA_LOGE("%s:Invalid peer for key setting", __func__);
		key_info->status = eHAL_STATUS_FAILURE;
		goto out;
	}

	txrx_vdev = wma_find_vdev_by_id(wma_handle, key_info->smesessionId);
	if(!txrx_vdev) {
		WMA_LOGE("%s:TxRx Vdev Handle is NULL", __func__);
		key_info->status = eHAL_STATUS_FAILURE;
		goto out;
	}

	if (key_info->defWEPIdx == WMA_INVALID_KEY_IDX &&
	    (key_info->encType == eSIR_ED_WEP40 ||
	     key_info->encType == eSIR_ED_WEP104) &&
	     txrx_vdev->opmode != wlan_op_mode_ap) {
		wma_read_cfg_wepkey(wma_handle, key_info->key,
				    &def_key_idx, &num_keys);
		key_info->defWEPIdx = def_key_idx;
	} else {
		num_keys = SIR_MAC_MAX_NUM_OF_DEFAULT_KEYS;
		if (key_info->encType != eSIR_ED_NONE) {
			for (i = 0; i < num_keys; i++) {
				if (key_info->key[i].keyDirection ==
							eSIR_TX_DEFAULT) {
					key_info->defWEPIdx = i;
					break;
				}
			}
		}
	}
	adf_os_mem_set(&key_params, 0, sizeof(key_params));
	key_params.vdev_id = key_info->smesessionId;
	key_params.key_type = key_info->encType;
	key_params.singl_tid_rc = key_info->singleTidRc;
	key_params.unicast = TRUE;
	key_params.def_key_idx = key_info->defWEPIdx;
	vos_mem_copy((v_VOID_t *) key_params.peer_mac,
		     (const v_VOID_t *) key_info->peerMacAddr, ETH_ALEN);
	for (i = 0; i < num_keys; i++) {
		if (key_params.key_type != eSIR_ED_NONE &&
		    !key_info->key[i].keyLength)
			continue;
		if (key_info->encType == eSIR_ED_TKIP) {
			vos_mem_copy(key_params.key_data,
				     key_info->key[i].key, 16);
			vos_mem_copy(&key_params.key_data[16],
				     &key_info->key[i].key[24], 8);
			vos_mem_copy(&key_params.key_data[24],
				     &key_info->key[i].key[16], 8);
		} else
			vos_mem_copy(key_params.key_data, key_info->key[i].key,
				     key_info->key[i].keyLength);
		if (key_info->encType == eSIR_ED_WPI) {
			key_params.key_idx = key_info->key[i].keyId;
			key_params.def_key_idx = key_info->key[i].keyId;
		} else
			key_params.key_idx = i;

		key_params.key_len = key_info->key[i].keyLength;
		buf = wma_setup_install_key_cmd(wma_handle, &key_params, &len);
		if (!buf) {
			WMA_LOGE("%s:Failed to setup install key buf", __func__);
			key_info->status = eHAL_STATUS_FAILED_ALLOC;
			goto out;
		}

	        WMA_LOGD("%s: peer unicast key[%d] %d ", __func__, i,
			key_info->key[i].keyLength);

		status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
					      WMI_VDEV_INSTALL_KEY_CMDID);
		if (status) {
			adf_nbuf_free(buf);
			WMA_LOGE("%s:Failed to send install key command", __func__);
			key_info->status = eHAL_STATUS_FAILURE;
			goto out;
		}
	}

        /* In IBSS mode, set the BSS KEY for this peer
         ** BSS key is supposed to be cache into wma_handle
        */
        if (wlan_op_mode_ibss == txrx_vdev->opmode){
           wma_set_ibsskey_helper(wma_handle, &wma_handle->ibsskey_info, key_info->peerMacAddr);
        }

	/* TODO: Should we wait till we get HTT_T2H_MSG_TYPE_SEC_IND? */
	key_info->status = eHAL_STATUS_SUCCESS;
out:
        if (key_info->sendRsp)
            wma_send_msg(wma_handle, WDA_SET_STAKEY_RSP, (void *) key_info, 0);
}

static void wma_delete_sta_req_ap_mode(tp_wma_handle wma,
					tpDeleteStaParams del_sta)
{
	ol_txrx_pdev_handle pdev;
	struct ol_txrx_peer_t *peer;

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		del_sta->status = VOS_STATUS_E_FAILURE;
		goto send_del_rsp;
	}

	peer = ol_txrx_peer_find_by_local_id(pdev, del_sta->staIdx);
	if (!peer) {
		WMA_LOGE("%s: Failed to get peer handle using peer id %d",
			 __func__, del_sta->staIdx);
		del_sta->status = VOS_STATUS_E_FAILURE;
		goto send_del_rsp;
	}

	wma_remove_peer(wma, peer->mac_addr.raw, del_sta->smesessionId, peer,
			VOS_FALSE);
	del_sta->status = VOS_STATUS_SUCCESS;

send_del_rsp:
	if (del_sta->respReqd) {
		WMA_LOGD("%s: Sending del rsp to umac (status: %d)",
					__func__, del_sta->status);
		wma_send_msg(wma, WDA_DELETE_STA_RSP, (void *)del_sta, 0);
	}
}

#ifdef FEATURE_WLAN_TDLS
static void wma_del_tdls_sta(tp_wma_handle wma,
	                            tpDeleteStaParams del_sta)
{
	ol_txrx_pdev_handle pdev;
	ol_txrx_vdev_handle vdev;
	struct ol_txrx_peer_t *peer;
	tTdlsPeerStateParams *peerStateParams;

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to find pdev", __func__);
		del_sta->status = VOS_STATUS_E_FAILURE;
		goto send_del_rsp;
	}

	vdev = wma_find_vdev_by_id(wma, del_sta->smesessionId);
	if (!vdev) {
		WMA_LOGE("%s: Failed to find vdev", __func__);
		del_sta->status = VOS_STATUS_E_FAILURE;
		goto send_del_rsp;
	}

	peer = ol_txrx_peer_find_by_local_id(pdev, del_sta->staIdx);
	if (!peer) {
		WMA_LOGE("%s: Failed to get peer handle using peer id %d",
		         __func__, del_sta->staIdx);
		del_sta->status = VOS_STATUS_E_FAILURE;
		goto send_del_rsp;
	}

	peerStateParams = vos_mem_malloc(sizeof(tTdlsPeerStateParams));
	if (!peerStateParams) {
		WMA_LOGE("%s: Failed to allocate memory for peerStateParams for: %pM",
		         __func__, del_sta->staMac);
		del_sta->status = VOS_STATUS_E_FAILURE;
		goto send_del_rsp;
	}

	vos_mem_zero(peerStateParams, sizeof(*peerStateParams));
	peerStateParams->peerState = WDA_TDLS_PEER_STATE_TEARDOWN;
	peerStateParams->vdevId = vdev->vdev_id;
	vos_mem_copy(&peerStateParams->peerMacAddr,
	             &del_sta->staMac,
	             sizeof(tSirMacAddr));

	WMA_LOGD("%s: sending tdls_peer_state for peer mac: %pM, "
	         " peerState: %d",
	         __func__, peerStateParams->peerMacAddr,
	         peerStateParams->peerState);

	wma_update_tdls_peer_state(wma, peerStateParams);

	del_sta->status = VOS_STATUS_SUCCESS;

send_del_rsp:
	if (del_sta->respReqd) {
		WMA_LOGD("%s: Sending del rsp to umac (status: %d)",
					__func__, del_sta->status);
		wma_send_msg(wma, WDA_DELETE_STA_RSP, (void *)del_sta, 0);
	}
}
#endif

static void wma_delete_sta_req_sta_mode(tp_wma_handle wma,
					tpDeleteStaParams params)
{
	VOS_STATUS status = VOS_STATUS_SUCCESS;
	struct wma_txrx_node *iface;
	iface = &wma->interfaces[params->smesessionId];
	iface->uapsd_cached_val = 0;

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	/* In case of LFR3.0 we need not send any
	 * WMI commands to FW before SYNCH_CONFIRM */
if(iface->roam_synch_in_progress)
		goto send_del_sta_rsp;
#endif
#ifdef FEATURE_WLAN_TDLS
	if (STA_ENTRY_TDLS_PEER == params->staType)
	{
	 wma_del_tdls_sta(wma, params);
	 return;
	}
#endif
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
send_del_sta_rsp:
#endif
	params->status = status;
	if (params->respReqd) {
		WMA_LOGD("%s: vdev_id %d status %d", __func__,
				params->smesessionId, status);
		wma_send_msg(wma, WDA_DELETE_STA_RSP, (void *)params, 0);
	}
}

static void wma_delete_sta(tp_wma_handle wma, tpDeleteStaParams del_sta)
{
	tANI_U8 oper_mode = BSS_OPERATIONAL_MODE_STA;
	u_int8_t smesession_id = del_sta->smesessionId;
	bool rsp_requested = del_sta->respReqd;

	if (wma_is_vdev_in_ap_mode(wma, smesession_id)) {
#ifdef FEATURE_WLAN_D0WOW
		wma_del_pm_vote(wma);
#else
		wma_allow_suspend_check(wma);
#endif
		oper_mode = BSS_OPERATIONAL_MODE_AP;
	}
#ifdef QCA_IBSS_SUPPORT
	if (wma_is_vdev_in_ibss_mode(wma, smesession_id)) {
		oper_mode = BSS_OPERATIONAL_MODE_IBSS;
#ifdef FEATURE_WLAN_D0WOW
		wma_del_pm_vote(wma);
#endif
		WMA_LOGD("%s: to delete sta for IBSS mode", __func__);
	}
#endif

	switch (oper_mode) {
	case BSS_OPERATIONAL_MODE_STA:
		wma_delete_sta_req_sta_mode(wma, del_sta);
		break;

#ifdef QCA_IBSS_SUPPORT
	case BSS_OPERATIONAL_MODE_IBSS:  /* IBSS shares AP code */
#endif
	case BSS_OPERATIONAL_MODE_AP:
		wma_delete_sta_req_ap_mode(wma, del_sta);
		break;
	}

#ifdef QCA_IBSS_SUPPORT
	/* adjust heart beat thresold timer value for
	 * detecting ibss peer departure
	 */
        if (oper_mode == BSS_OPERATIONAL_MODE_IBSS)
		wma_adjust_ibss_heart_beat_timer(wma, smesession_id, -1);
#endif
	if (!rsp_requested) {
		WMA_LOGD("%s: vdev_id %d status %d", __func__,
			del_sta->smesessionId, del_sta->status);
		vos_mem_free(del_sta);
	}
}

static int32_t wmi_unified_vdev_stop_send(wmi_unified_t wmi, u_int8_t vdev_id)
{
	wmi_vdev_stop_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wmi, len);
	if (!buf) {
		WMA_LOGP("%s : wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}
	cmd = (wmi_vdev_stop_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_stop_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_stop_cmd_fixed_param));
	cmd->vdev_id = vdev_id;
	if (wmi_unified_cmd_send(wmi, buf, len, WMI_VDEV_STOP_CMDID)) {
		WMA_LOGP("%s: Failed to send vdev stop command", __func__);
		adf_nbuf_free(buf);
		return -EIO;
	}
	return 0;
}

static void wma_delete_bss(tp_wma_handle wma, tpDeleteBssParams params)
{
	ol_txrx_pdev_handle pdev;
	ol_txrx_peer_handle peer = NULL;
	struct wma_target_req *msg;
	VOS_STATUS status = VOS_STATUS_SUCCESS;
	u_int8_t peer_id;
	u_int8_t max_wait_iterations = 0;
	ol_txrx_vdev_handle txrx_vdev = NULL;
	v_BOOL_t roam_synch_in_progress = VOS_FALSE;

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s:Unable to get TXRX context", __func__);
		goto out;
	}

#ifdef QCA_IBSS_SUPPORT
	if (wma_is_vdev_in_ibss_mode(wma, params->smesessionId))
		/* in rome ibss case, self mac is used to create the bss peer */
		peer = ol_txrx_find_peer_by_addr(pdev,
				wma->interfaces[params->smesessionId].addr,
				&peer_id);
	else
#endif
		peer = ol_txrx_find_peer_by_addr(pdev, params->bssid,
					 &peer_id);

	if (!peer) {
		WMA_LOGP("%s: Failed to find peer %pM", __func__,
			 params->bssid);
		status = VOS_STATUS_E_FAILURE;
		goto out;
	}

	vos_mem_zero(wma->interfaces[params->smesessionId].bssid, ETH_ALEN);

        txrx_vdev = wma_find_vdev_by_id(wma, params->smesessionId);
        if (!txrx_vdev) {
                WMA_LOGE("%s:Invalid vdev handle", __func__);
                status = VOS_STATUS_E_FAILURE;
                goto out;
        }

	/*Free the allocated stats response buffer for the the session*/
	if (wma->interfaces[params->smesessionId].stats_rsp) {
		vos_mem_free(wma->interfaces[params->smesessionId].stats_rsp);
		wma->interfaces[params->smesessionId].stats_rsp = NULL;
	}

        if (wlan_op_mode_ibss == txrx_vdev->opmode) {
                wma->ibss_started = 0;
        }
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
    if(wma->interfaces[params->smesessionId].roam_synch_in_progress)
	{
	  roam_synch_in_progress = VOS_TRUE;
	  goto detach_peer;
	}
#endif
	msg = wma_fill_vdev_req(wma, params->smesessionId, WDA_DELETE_BSS_REQ,
				WMA_TARGET_REQ_TYPE_VDEV_STOP, params,
				WMA_VDEV_STOP_REQUEST_TIMEOUT);
	if (!msg) {
		WMA_LOGP("%s: Failed to fill vdev request for vdev_id %d",
			 __func__, params->smesessionId);
		status = VOS_STATUS_E_NOMEM;
		goto detach_peer;
	}

	WMA_LOGW(FL("Outstanding msdu packets: %d"),
		 ol_txrx_get_tx_pending(pdev));

	max_wait_iterations =
		wma->interfaces[params->smesessionId].delay_before_vdev_stop /
						WMA_TX_Q_RECHECK_TIMER_WAIT;

	while ( ol_txrx_get_tx_pending(pdev) && max_wait_iterations )
	{
                WMA_LOGW(FL("Waiting for outstanding packet to drain."));
		vos_wait_single_event(&wma->tx_queue_empty_event,
				      WMA_TX_Q_RECHECK_TIMER_WAIT);
		max_wait_iterations--;
	}

	if (ol_txrx_get_tx_pending(pdev))
	{
		WMA_LOGW(FL("Outstanding msdu packets before VDEV_STOP : %d"),
			 ol_txrx_get_tx_pending(pdev));
	}

	WMA_LOGD("%s, vdev_id: %d, pausing tx_ll_queue for VDEV_STOP (del_bss)",
		 __func__, params->smesessionId);
	wdi_in_vdev_pause(wma->interfaces[params->smesessionId].handle,
			  OL_TXQ_PAUSE_REASON_VDEV_STOP);
	wma->interfaces[params->smesessionId].pause_bitmap |=
						(1 << PAUSE_TYPE_HOST);

	if (wmi_unified_vdev_stop_send(wma->wmi_handle, params->smesessionId)) {
		WMA_LOGP("%s: %d Failed to send vdev stop",
			 __func__, __LINE__);
		wma_remove_vdev_req(wma, params->smesessionId,
				    WMA_TARGET_REQ_TYPE_VDEV_STOP);
		status = VOS_STATUS_E_FAILURE;
		goto detach_peer;
	}
	WMA_LOGD("%s: bssid %pM vdev_id %d",
		__func__, params->bssid, params->smesessionId);
	return;
detach_peer:
	wma_remove_peer(wma, params->bssid, params->smesessionId, peer,
			roam_synch_in_progress);
out:
	params->status = status;
	wma_send_msg(wma, WDA_DELETE_BSS_RSP, (void *)params, 0);
}

static void wma_set_linkstate(tp_wma_handle wma, tpLinkStateParams params)
{
	ol_txrx_pdev_handle pdev;
	ol_txrx_vdev_handle vdev;
	ol_txrx_peer_handle peer;
	u_int8_t vdev_id, peer_id;
	v_BOOL_t roam_synch_in_progress = VOS_FALSE;
	VOS_STATUS status;

	params->status = VOS_TRUE;
	WMA_LOGD("%s: state %d selfmac %pM", __func__,
		 params->state, params->selfMacAddr);
	if ((params->state != eSIR_LINK_PREASSOC_STATE) &&
	    (params->state != eSIR_LINK_DOWN_STATE)) {
		WMA_LOGD("%s: unsupported link state %d",
			 __func__, params->state);
		goto out;
	}

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Unable to get TXRX context", __func__);
		goto out;
	}

	vdev = wma_find_vdev_by_addr(wma, params->selfMacAddr, &vdev_id);
	if (!vdev) {
		WMA_LOGP("%s: vdev not found for addr: %pM",
			 __func__, params->selfMacAddr);
		goto out;
	}

	if (wma_is_vdev_in_ap_mode(wma, vdev_id)) {
		WMA_LOGD("%s: Ignoring set link req in ap mode", __func__);
		goto out;
	}

	if (params->state == eSIR_LINK_PREASSOC_STATE) {
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
      if(wma->interfaces[vdev_id].roam_synch_in_progress){
			roam_synch_in_progress = VOS_TRUE;
		}
#endif
		status = wma_create_peer(wma, pdev, vdev, params->bssid,
		                WMI_PEER_TYPE_DEFAULT, vdev_id,
				roam_synch_in_progress);
		if (status != VOS_STATUS_SUCCESS)
			params->status = VOS_FALSE;
	}
	else {
		WMA_LOGD("%s, vdev_id: %d, pausing tx_ll_queue for VDEV_STOP",
			 __func__, vdev_id);
		wdi_in_vdev_pause(wma->interfaces[vdev_id].handle,
				  OL_TXQ_PAUSE_REASON_VDEV_STOP);
		wma->interfaces[vdev_id].pause_bitmap |= (1 << PAUSE_TYPE_HOST);
		if (wmi_unified_vdev_stop_send(wma->wmi_handle, vdev_id)) {
			WMA_LOGP("%s: %d Failed to send vdev stop",
				 __func__, __LINE__);
		}
		peer = ol_txrx_find_peer_by_addr(pdev, params->bssid, &peer_id);
		if (peer) {
			WMA_LOGP("%s: Deleting peer %pM vdev id %d",
				 __func__, params->bssid, vdev_id);
			wma_remove_peer(wma, params->bssid, vdev_id, peer,
					roam_synch_in_progress);
		}
	}
out:
	wma_send_msg(wma, WDA_SET_LINK_STATE_RSP, (void *)params, 0);
}

/*
 * Function to update per ac EDCA parameters
 */
static void wma_update_edca_params_for_ac(tSirMacEdcaParamRecord *edca_param,
					  wmi_wmm_vparams *wmm_param,
					  int ac)
{
#define WMA_WMM_EXPO_TO_VAL(val)	((1 << (val)) - 1)
	wmm_param->cwmin = WMA_WMM_EXPO_TO_VAL(edca_param->cw.min);
	wmm_param->cwmax = WMA_WMM_EXPO_TO_VAL(edca_param->cw.max);
	wmm_param->aifs = edca_param->aci.aifsn;
	wmm_param->txoplimit = edca_param->txoplimit;
	wmm_param->acm = edca_param->aci.acm;

	/* TODO: No ack is not present in EdcaParamRecord */
	wmm_param->no_ack = 0;

	WMA_LOGI("WMM PARAMS AC[%d]: AIFS %d Min %d Max %d TXOP %d ACM %d NOACK %d",
		 ac,
		 wmm_param->aifs,
		 wmm_param->cwmin,
		 wmm_param->cwmax,
		 wmm_param->txoplimit,
		 wmm_param->acm,
		 wmm_param->no_ack);
}

/*
 * Set TX power limit through vdev param
 */
static void wma_set_tx_power(WMA_HANDLE handle,
	tMaxTxPowerParams *tx_pwr_params)
{
	tp_wma_handle wma_handle = (tp_wma_handle)handle;
	u_int8_t vdev_id;
	int ret = -1;
	void *pdev;

	if (tx_pwr_params->dev_mode == VOS_STA_SAP_MODE ||
		tx_pwr_params->dev_mode == VOS_P2P_GO_MODE) {
		pdev = wma_find_vdev_by_addr(wma_handle,
					tx_pwr_params->bssId, &vdev_id);
	} else {
		pdev = wma_find_vdev_by_bssid(wma_handle,
					tx_pwr_params->bssId, &vdev_id);
	}
	if (!pdev) {
		WMA_LOGE("vdev handle is invalid for %pM", tx_pwr_params->bssId);
		vos_mem_free(tx_pwr_params);
		return;
	}

	if (!(wma_handle->interfaces[vdev_id].vdev_up)) {
		WMA_LOGE("%s: vdev id %d is not up for %pM", __func__, vdev_id,
				tx_pwr_params->bssId);
		vos_mem_free(tx_pwr_params);
		return;
	}

	if (tx_pwr_params->power == 0) {
		/* set to default. Since the app does not care the tx power
		 * we keep the previous setting */
		wma_handle->interfaces[vdev_id].tx_power = 0;
		ret = 0;
		goto end;
	}
	if (wma_handle->interfaces[vdev_id].max_tx_power != 0) {
		/* make sure tx_power less than max_tx_power */
		if (tx_pwr_params->power >
				wma_handle->interfaces[vdev_id].max_tx_power) {
			tx_pwr_params->power =
				wma_handle->interfaces[vdev_id].max_tx_power;
		}
	}
	if (wma_handle->interfaces[vdev_id].tx_power != tx_pwr_params->power) {

		/* tx_power changed, Push the tx_power to FW */
		WMA_LOGW("%s: Set TX power limit [WMI_VDEV_PARAM_TX_PWRLIMIT] to %d",
			__func__, tx_pwr_params->power);
		ret = wmi_unified_vdev_set_param_send(wma_handle->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_TX_PWRLIMIT, tx_pwr_params->power);
		if (ret == 0)
			wma_handle->interfaces[vdev_id].tx_power = tx_pwr_params->power;
	} else {
		/* no tx_power change */
		ret = 0;
	}
end:
	vos_mem_free(tx_pwr_params);
	if (ret)
		WMA_LOGE("Failed to set vdev param WMI_VDEV_PARAM_TX_PWRLIMIT");
}

/*
 * Set TX power limit through vdev param
 */
static void wma_set_max_tx_power(WMA_HANDLE handle,
	tMaxTxPowerParams *tx_pwr_params)
{
	tp_wma_handle wma_handle = (tp_wma_handle)handle;
	u_int8_t vdev_id;
	int ret = -1;
	void *pdev;
	tPowerdBm  prev_max_power;

	pdev = wma_find_vdev_by_addr(wma_handle, tx_pwr_params->bssId, &vdev_id);
	if (pdev == NULL) {
		/* not in SAP array. Try the station/p2p array */
		pdev = wma_find_vdev_by_bssid(wma_handle,
					tx_pwr_params->bssId, &vdev_id);
	}
	if (!pdev) {
		WMA_LOGE("vdev handle is invalid for %pM", tx_pwr_params->bssId);
		vos_mem_free(tx_pwr_params);
		return;
	}

	if (! (wma_handle->interfaces[vdev_id].vdev_up)) {
		WMA_LOGE("%s: vdev id %d is not up",__func__, vdev_id);
		vos_mem_free(tx_pwr_params);
		return;
	}

	if (wma_handle->interfaces[vdev_id].max_tx_power == tx_pwr_params->power) {
		ret = 0;
		goto end;
	}
	prev_max_power = wma_handle->interfaces[vdev_id].max_tx_power;
	wma_handle->interfaces[vdev_id].max_tx_power = tx_pwr_params->power;
	if (wma_handle->interfaces[vdev_id].max_tx_power == 0) {
		ret = 0;
		goto end;
	}
	WMA_LOGW("Set MAX TX power limit [WMI_VDEV_PARAM_TX_PWRLIMIT] to %d",
		wma_handle->interfaces[vdev_id].max_tx_power);
	ret = wmi_unified_vdev_set_param_send(wma_handle->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_TX_PWRLIMIT,
			wma_handle->interfaces[vdev_id].max_tx_power);
	if (ret == 0)
		wma_handle->interfaces[vdev_id].tx_power =
			wma_handle->interfaces[vdev_id].max_tx_power;
	else
		wma_handle->interfaces[vdev_id].max_tx_power = prev_max_power;
end:
	vos_mem_free(tx_pwr_params);
	if (ret)
		WMA_LOGE("%s: Failed to set vdev param WMI_VDEV_PARAM_TX_PWRLIMIT", __func__);
}

/*
 * Function to update the EDCA parameters to the target
 */
static VOS_STATUS wma_process_update_edca_param_req(WMA_HANDLE handle,
						    tEdcaParams *edca_params)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	u_int8_t *buf_ptr;
	wmi_buf_t buf;
	wmi_vdev_set_wmm_params_cmd_fixed_param *cmd;
	wmi_wmm_vparams *wmm_param;
	tSirMacEdcaParamRecord *edca_record;
	int ac;
	int len = sizeof(*cmd);
	ol_txrx_pdev_handle pdev;
	struct ol_tx_wmm_param_t ol_tx_wmm_param;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);

	if (!buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_vdev_set_wmm_params_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_set_wmm_params_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_vdev_set_wmm_params_cmd_fixed_param));
	cmd->vdev_id = edca_params->bssIdx;

	for (ac = 0; ac < WME_NUM_AC; ac++) {
		wmm_param = (wmi_wmm_vparams *)(&cmd->wmm_params[ac]);
		WMITLV_SET_HDR(&wmm_param->tlv_header,
			       WMITLV_TAG_STRUC_wmi_vdev_set_wmm_params_cmd_fixed_param,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_wmm_vparams));
		switch (ac) {
		case WME_AC_BE:
			edca_record = &edca_params->acbe;
			break;
		case WME_AC_BK:
			edca_record = &edca_params->acbk;
			break;
		case WME_AC_VI:
			edca_record = &edca_params->acvi;
			break;
		case WME_AC_VO:
			edca_record = &edca_params->acvo;
			break;
		default:
			goto fail;
		}

		wma_update_edca_params_for_ac(edca_record, wmm_param, ac);

		ol_tx_wmm_param.ac[ac].aifs = wmm_param->aifs;
		ol_tx_wmm_param.ac[ac].cwmin = wmm_param->cwmin;
		ol_tx_wmm_param.ac[ac].cwmax = wmm_param->cwmax;
	}

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				  WMI_VDEV_SET_WMM_PARAMS_CMDID))
		goto fail;

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma_handle->vos_context);
	wdi_in_set_wmm_param(pdev, ol_tx_wmm_param);

	return VOS_STATUS_SUCCESS;

fail:
	wmi_buf_free(buf);
	WMA_LOGE("%s: Failed to set WMM Paremeters", __func__);
	return VOS_STATUS_E_FAILURE;
}

static int wmi_unified_probe_rsp_tmpl_send(tp_wma_handle wma,
					u_int8_t vdev_id,
					tpSendProbeRespParams probe_rsp_info)
{
	wmi_prb_tmpl_cmd_fixed_param  *cmd;
	wmi_bcn_prb_info *bcn_prb_info;
	wmi_buf_t wmi_buf;
	u_int32_t tmpl_len, tmpl_len_aligned, wmi_buf_len;
	u_int8_t *frm, *buf_ptr;
	int ret;
	u_int64_t adjusted_tsf_le;
	struct ieee80211_frame *wh;

	WMA_LOGD(FL("Send probe response template for vdev %d"), vdev_id);

	frm = probe_rsp_info->pProbeRespTemplate;
	tmpl_len = probe_rsp_info->probeRespTemplateLen;
	tmpl_len_aligned = roundup(tmpl_len, sizeof(A_UINT32));
	/*
	 * Make the TSF offset negative so probe response in the same
	 * staggered batch have the same TSF.
	 */
	adjusted_tsf_le = cpu_to_le64(0ULL -
				      wma->interfaces[vdev_id].tsfadjust);
	/* Update the timstamp in the probe response buffer with adjusted TSF */
	wh = (struct ieee80211_frame *)frm;
	A_MEMCPY(&wh[1], &adjusted_tsf_le, sizeof(adjusted_tsf_le));

	wmi_buf_len = sizeof(wmi_prb_tmpl_cmd_fixed_param) +
	          sizeof(wmi_bcn_prb_info) + WMI_TLV_HDR_SIZE +
		  tmpl_len_aligned;

	if (wmi_buf_len > BEACON_TX_BUFFER_SIZE) {
		WMA_LOGE(FL("wmi_buf_len: %d > %d. Can't send wmi cmd"),
			 wmi_buf_len, BEACON_TX_BUFFER_SIZE);
		return -EINVAL;
	}

	wmi_buf = wmi_buf_alloc(wma->wmi_handle, wmi_buf_len);
	if (!wmi_buf) {
		WMA_LOGE(FL("wmi_buf_alloc failed"));
		return -ENOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_buf);

	cmd = (wmi_prb_tmpl_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_prb_tmpl_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_prb_tmpl_cmd_fixed_param));
	cmd->vdev_id = vdev_id;
	cmd->buf_len = tmpl_len;
	buf_ptr += sizeof(wmi_prb_tmpl_cmd_fixed_param);

	bcn_prb_info = (wmi_bcn_prb_info *)buf_ptr;
        WMITLV_SET_HDR(&bcn_prb_info->tlv_header,
			WMITLV_TAG_STRUC_wmi_bcn_prb_info,
			WMITLV_GET_STRUCT_TLVLEN(wmi_bcn_prb_info));
	bcn_prb_info->caps = 0;
	bcn_prb_info->erp = 0;
	buf_ptr += sizeof(wmi_bcn_prb_info);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, tmpl_len_aligned);
	buf_ptr += WMI_TLV_HDR_SIZE;
	vos_mem_copy(buf_ptr, frm, tmpl_len);

	ret = wmi_unified_cmd_send(wma->wmi_handle,
			wmi_buf, wmi_buf_len,
			WMI_PRB_TMPL_CMDID);
	if (ret) {
		WMA_LOGE(FL("Failed to send PRB RSP tmpl: %d"), ret);
		wmi_buf_free(wmi_buf);
	}

	return ret;
}

static int wmi_unified_bcn_tmpl_send(tp_wma_handle wma,
				     u_int8_t vdev_id,
				     tpSendbeaconParams bcn_info,
				     u_int8_t bytes_to_strip)
{
	wmi_bcn_tmpl_cmd_fixed_param  *cmd;
	wmi_bcn_prb_info *bcn_prb_info;
	wmi_buf_t wmi_buf;
	u_int32_t tmpl_len, tmpl_len_aligned, wmi_buf_len;
	u_int8_t *frm, *buf_ptr;
	int ret;
	u_int8_t *p2p_ie;
	u_int16_t p2p_ie_len = 0;
	u_int64_t adjusted_tsf_le;
	struct ieee80211_frame *wh;


	WMA_LOGD("Send beacon template for vdev %d", vdev_id);

	if (bcn_info->p2pIeOffset) {
		p2p_ie = bcn_info->beacon + bcn_info->p2pIeOffset;
		p2p_ie_len = (u_int16_t) p2p_ie[1] + 2;
	}

	/*
	 * XXX: The first byte of beacon buffer contains beacon length
	 * only when UMAC in sending the beacon template. In othercases
	 * (ex: from tbtt update) beacon length is read from beacon
	 * information.
	 */
	if (bytes_to_strip)
		tmpl_len = *(u_int32_t *)&bcn_info->beacon[0];
	else
		tmpl_len = bcn_info->beaconLength;
	if (p2p_ie_len) {
		tmpl_len -= (u_int32_t) p2p_ie_len;
	}

	frm = bcn_info->beacon + bytes_to_strip;
	tmpl_len_aligned = roundup(tmpl_len, sizeof(A_UINT32));
	/*
	 * Make the TSF offset negative so beacons in the same
	 * staggered batch have the same TSF.
	 */
	adjusted_tsf_le = cpu_to_le64(0ULL -
				      wma->interfaces[vdev_id].tsfadjust);
	/* Update the timstamp in the beacon buffer with adjusted TSF */
	wh = (struct ieee80211_frame *)frm;
	A_MEMCPY(&wh[1], &adjusted_tsf_le, sizeof(adjusted_tsf_le));

	wmi_buf_len = sizeof(wmi_bcn_tmpl_cmd_fixed_param) +
	          sizeof(wmi_bcn_prb_info) + WMI_TLV_HDR_SIZE +
		  tmpl_len_aligned;

	wmi_buf = wmi_buf_alloc(wma->wmi_handle, wmi_buf_len);
	if (!wmi_buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_buf);

	cmd = (wmi_bcn_tmpl_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_bcn_tmpl_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_bcn_tmpl_cmd_fixed_param));
	cmd->vdev_id = vdev_id;
	cmd->tim_ie_offset = bcn_info->timIeOffset - bytes_to_strip;
	cmd->buf_len = tmpl_len;
	buf_ptr += sizeof(wmi_bcn_tmpl_cmd_fixed_param);

	bcn_prb_info = (wmi_bcn_prb_info *)buf_ptr;
        WMITLV_SET_HDR(&bcn_prb_info->tlv_header,
			WMITLV_TAG_STRUC_wmi_bcn_prb_info,
			WMITLV_GET_STRUCT_TLVLEN(wmi_bcn_prb_info));
	bcn_prb_info->caps = 0;
	bcn_prb_info->erp = 0;
	buf_ptr += sizeof(wmi_bcn_prb_info);

	WMITLV_SET_HDR(buf_ptr,	WMITLV_TAG_ARRAY_BYTE, tmpl_len_aligned);
	buf_ptr += WMI_TLV_HDR_SIZE;
	vos_mem_copy(buf_ptr, frm, tmpl_len);

	ret = wmi_unified_cmd_send(wma->wmi_handle,
			wmi_buf, wmi_buf_len,
			WMI_BCN_TMPL_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send bcn tmpl: %d", __func__, ret);
		wmi_buf_free(wmi_buf);
	}

	return ret;
}

VOS_STATUS wma_store_bcn_tmpl(tp_wma_handle wma, u_int8_t vdev_id,
			      tpSendbeaconParams bcn_info)
{
	struct beacon_info *bcn;
	u_int32_t len;
	u_int8_t *bcn_payload;
	struct beacon_tim_ie *tim_ie;

	bcn = wma->interfaces[vdev_id].beacon;
	if (!bcn || !bcn->buf) {
		WMA_LOGE("%s: Memory is not allocated to hold bcn template",
			 __func__);
		return VOS_STATUS_E_INVAL;
	}

	len = *(u32 *)&bcn_info->beacon[0];
	if (len > WMA_BCN_BUF_MAX_SIZE) {
		WMA_LOGE("%s: Received beacon len %d exceeding max limit %d",
			 __func__, len, WMA_BCN_BUF_MAX_SIZE);
		return VOS_STATUS_E_INVAL;
	}
	WMA_LOGD("%s: Storing received beacon template buf to local buffer",
		 __func__);
	adf_os_spin_lock_bh(&bcn->lock);

	/*
	 * Copy received beacon template content in local buffer.
	 * this will be send to target on the reception of SWBA
	 * event from target.
	 */
	adf_nbuf_trim_tail(bcn->buf, adf_nbuf_len(bcn->buf));
	memcpy(adf_nbuf_data(bcn->buf),
			bcn_info->beacon + 4 /* Exclude beacon length field */,
			len);
	if (bcn_info->timIeOffset > 3)
	{
		bcn->tim_ie_offset = bcn_info->timIeOffset - 4;
	}
	else
	{
		bcn->tim_ie_offset = bcn_info->timIeOffset;
	}

	if (bcn_info->p2pIeOffset > 3)
	{
		bcn->p2p_ie_offset = bcn_info->p2pIeOffset - 4;
	}
	else
	{
		bcn->p2p_ie_offset = bcn_info->p2pIeOffset;
	}
	bcn_payload = adf_nbuf_data(bcn->buf);
	if (bcn->tim_ie_offset)
	{
		tim_ie = (struct beacon_tim_ie *)(&bcn_payload[bcn->tim_ie_offset]);
		/*
		* Intial Value of bcn->dtim_count will be 0.
		* But if the beacon gets updated then current dtim
		* count will be restored
		*/
		tim_ie->dtim_count = bcn->dtim_count;
		tim_ie->tim_bitctl = 0;
	}

	adf_nbuf_put_tail(bcn->buf, len);
	bcn->len = len;

	adf_os_spin_unlock_bh(&bcn->lock);

	return VOS_STATUS_SUCCESS;
}



static int wma_tbtt_update_ind(tp_wma_handle wma, u_int8_t *buf)
{
	struct wma_txrx_node *intf;
	struct beacon_info *bcn;
	tSendbeaconParams bcn_info;
	u_int32_t *adjusted_tsf = NULL;
	u_int32_t if_id = 0, vdev_map;
	u_int32_t num_tbttoffset_list;
	wmi_tbtt_offset_event_fixed_param *tbtt_offset_event;
	WMA_LOGI("%s: Enter", __func__);
	if (!buf) {
		WMA_LOGE("Invalid event buffer");
		return -EINVAL;
	}
	if (!wma) {
		WMA_LOGE("Invalid wma handle");
		return -EINVAL;
	}
	intf = wma->interfaces;
	tbtt_offset_event = (wmi_tbtt_offset_event_fixed_param *)buf;
	vdev_map = tbtt_offset_event->vdev_map;
	num_tbttoffset_list = *(u_int32_t *)(buf + sizeof(wmi_tbtt_offset_event_fixed_param));
	adjusted_tsf = (u_int32_t *) ((u_int8_t *)buf +
			sizeof(wmi_tbtt_offset_event_fixed_param) +
			sizeof (u_int32_t));
	if (!adjusted_tsf) {
		WMA_LOGE("%s: Invalid adjusted_tsf", __func__);
		return -EINVAL;
	}

	for ( ;(vdev_map); vdev_map >>= 1, if_id++) {
		if (!(vdev_map & 0x1) || (!(intf[if_id].handle)))
			continue;

		bcn = intf[if_id].beacon;
		if (!bcn) {
			WMA_LOGE("%s: Invalid beacon", __func__);
			return -EINVAL;
		}
		if (!bcn->buf) {
			WMA_LOGE("%s: Invalid beacon buffer", __func__);
			return -EINVAL;
		}
		/* Save the adjusted TSF */
		intf[if_id].tsfadjust = adjusted_tsf[if_id];

		adf_os_spin_lock_bh(&bcn->lock);
		vos_mem_zero(&bcn_info, sizeof(bcn_info));
		bcn_info.beacon = adf_nbuf_data(bcn->buf);
		bcn_info.p2pIeOffset = bcn->p2p_ie_offset;
		bcn_info.beaconLength = bcn->len;
		bcn_info.timIeOffset = bcn->tim_ie_offset;
		adf_os_spin_unlock_bh(&bcn->lock);

		/* Update beacon template in firmware */
		wmi_unified_bcn_tmpl_send(wma, if_id, &bcn_info, 0);
	}
	return 0;
}

static int wma_tbttoffset_update_event_handler(void *handle, u_int8_t *event,
					       u_int32_t len)
{
	WMI_TBTTOFFSET_UPDATE_EVENTID_param_tlvs *param_buf;
	wmi_tbtt_offset_event_fixed_param *tbtt_offset_event;
	u_int8_t *buf, *tempBuf;
	vos_msg_t vos_msg = {0};

	param_buf = (WMI_TBTTOFFSET_UPDATE_EVENTID_param_tlvs *)event;
	if(!param_buf) {
		WMA_LOGE("Invalid tbtt update event buffer");
		return -EINVAL;
	}

	tbtt_offset_event = param_buf->fixed_param;
	buf = vos_mem_malloc(sizeof(wmi_tbtt_offset_event_fixed_param) +
			sizeof (u_int32_t) +
			(param_buf->num_tbttoffset_list * sizeof (u_int32_t)));
	if (!buf) {
		WMA_LOGE("%s: Failed alloc memory for buf", __func__);
		return -EINVAL;
	}

	tempBuf = buf;
	vos_mem_zero(buf, (sizeof(wmi_tbtt_offset_event_fixed_param) +
			sizeof (u_int32_t) +
			(param_buf->num_tbttoffset_list * sizeof (u_int32_t))));
	vos_mem_copy(buf, (u_int8_t *)tbtt_offset_event, sizeof (wmi_tbtt_offset_event_fixed_param));
	buf += sizeof (wmi_tbtt_offset_event_fixed_param);

	vos_mem_copy(buf, (u_int8_t *) &param_buf->num_tbttoffset_list, sizeof (u_int32_t));
	buf += sizeof(u_int32_t);

	vos_mem_copy(buf, (u_int8_t *)param_buf->tbttoffset_list, (param_buf->num_tbttoffset_list * sizeof(u_int32_t)));

	vos_msg.type = WDA_TBTT_UPDATE_IND;
	vos_msg.bodyptr = tempBuf;
	vos_msg.bodyval = 0;

	if (VOS_STATUS_SUCCESS !=
	vos_mq_post_message(VOS_MQ_ID_WDA, &vos_msg)) {
		WMA_LOGP("%s: Failed to post WDA_TBTT_UPDATE_IND msg", __func__);
		vos_mem_free(tempBuf);
		return -1;
	}
	WMA_LOGD("WDA_TBTT_UPDATE_IND posted");
	return 0;
}


static int wma_p2p_go_set_beacon_ie(t_wma_handle *wma_handle,
					A_UINT32 vdev_id, u_int8_t *p2pIe)
{
	int ret;
	wmi_p2p_go_set_beacon_ie_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	u_int32_t ie_len, ie_len_aligned, wmi_buf_len;
	u_int8_t *buf_ptr;

	ie_len = (u_int32_t) (p2pIe[1] + 2);

	/* More than one P2P IE may be included in a single frame.
	   If multiple P2P IEs are present, the complete P2P attribute
	   data consists of the concatenation of the P2P Attribute
	   fields of the P2P IEs. The P2P Attributes field of each
	   P2P IE may be any length up to the maximum (251 octets).
	   In this case host sends one P2P IE to firmware so the length
	   should not exceed more than 251 bytes
	 */
	if (ie_len > 251) {
		WMA_LOGE("%s : invalid p2p ie length %u", __func__, ie_len);
		return -EINVAL;
	}

	ie_len_aligned = roundup(ie_len, sizeof(A_UINT32));

	wmi_buf_len = sizeof(wmi_p2p_go_set_beacon_ie_fixed_param) + ie_len_aligned + WMI_TLV_HDR_SIZE;

	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, wmi_buf_len);
	if (!wmi_buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_buf);

	cmd = (wmi_p2p_go_set_beacon_ie_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_p2p_go_set_beacon_ie_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_p2p_go_set_beacon_ie_fixed_param));
	cmd->vdev_id = vdev_id;
	cmd->ie_buf_len = ie_len;

	buf_ptr += sizeof(wmi_p2p_go_set_beacon_ie_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, ie_len_aligned);
	buf_ptr += WMI_TLV_HDR_SIZE;
	vos_mem_copy(buf_ptr, p2pIe, ie_len);

	WMA_LOGI("%s: Sending WMI_P2P_GO_SET_BEACON_IE", __func__);

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle,
			wmi_buf, wmi_buf_len,
			WMI_P2P_GO_SET_BEACON_IE
			);
	if (ret) {
		WMA_LOGE("Failed to send bcn tmpl: %d", ret);
		wmi_buf_free(wmi_buf);
	}

	WMA_LOGI("%s: Successfully sent WMI_P2P_GO_SET_BEACON_IE", __func__);
	return ret;
}

static void wma_send_probe_rsp_tmpl(tp_wma_handle wma,
				tpSendProbeRespParams probe_rsp_info)
{
        ol_txrx_vdev_handle vdev;
        u_int8_t vdev_id;
	tpAniProbeRspStruct probe_rsp;

	if(!probe_rsp_info) {
		WMA_LOGE(FL("probe_rsp_info is NULL"));
		return;
	}

	probe_rsp = (tpAniProbeRspStruct)(probe_rsp_info->pProbeRespTemplate);
	if(!probe_rsp) {
		WMA_LOGE(FL("probe_rsp is NULL"));
		return;
	}

	vdev = wma_find_vdev_by_addr(wma, probe_rsp->macHdr.sa, &vdev_id);
	if (!vdev) {
		WMA_LOGE(FL("failed to get vdev handle"));
		return;
	}

	if (WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
				WMI_SERVICE_BEACON_OFFLOAD)) {
		WMA_LOGA("Beacon Offload Enabled Sending Unified command");
		if (wmi_unified_probe_rsp_tmpl_send(wma, vdev_id,
					probe_rsp_info) < 0){
		WMA_LOGE(FL("wmi_unified_probe_rsp_tmpl_send Failed "));
		return;
		}
	}
}

static void wma_send_beacon(tp_wma_handle wma, tpSendbeaconParams bcn_info)
{
	ol_txrx_vdev_handle vdev;
	u_int8_t vdev_id;
	VOS_STATUS status;
        u_int8_t *p2p_ie;
        tpAniBeaconStruct beacon;

        beacon = (tpAniBeaconStruct)(bcn_info->beacon);
        vdev = wma_find_vdev_by_addr(wma, beacon->macHdr.sa, &vdev_id);
	if (!vdev) {
		WMA_LOGE("%s : failed to get vdev handle", __func__);
		return;
	}

	if (WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
				   WMI_SERVICE_BEACON_OFFLOAD)) {
	    WMA_LOGA("Beacon Offload Enabled Sending Unified command");
	    if (wmi_unified_bcn_tmpl_send(wma, vdev_id, bcn_info, 4) < 0){
                WMA_LOGE("%s : wmi_unified_bcn_tmpl_send Failed ", __func__);
		return;
	    }

	    if (bcn_info->p2pIeOffset) {
		    p2p_ie = bcn_info->beacon + bcn_info->p2pIeOffset;
		    WMA_LOGI(" %s: p2pIe is present - vdev_id %hu, p2p_ie = %p, p2p ie len = %hu",
				    __func__, vdev_id, p2p_ie, p2p_ie[1]);
		    if (wma_p2p_go_set_beacon_ie(wma, vdev_id, p2p_ie) < 0) {
			    WMA_LOGE("%s : wmi_unified_bcn_tmpl_send Failed ", __func__);
			    return;
		    }
	    }
	}
	status = wma_store_bcn_tmpl(wma, vdev_id, bcn_info);
	if (status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("%s : wma_store_bcn_tmpl Failed", __func__);
		return;
	}
	if (!wma->interfaces[vdev_id].vdev_up) {
	      if (wmi_unified_vdev_up_send(wma->wmi_handle, vdev_id, 0,
				      bcn_info->bssId) < 0) {
		WMA_LOGE("%s : failed to send vdev up", __func__);
		return;
	     }
	     wma->interfaces[vdev_id].vdev_up = TRUE;
		wma_set_sap_keepalive(wma, vdev_id);
	}
}

#if !defined(REMOVE_PKT_LOG)
static VOS_STATUS wma_pktlog_wmi_send_cmd(WMA_HANDLE handle,
					  struct ath_pktlog_wmi_params *params)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_PKTLOG_EVENT PKTLOG_EVENT;
	WMI_CMD_ID CMD_ID;
	wmi_pdev_pktlog_enable_cmd_fixed_param *cmd;
	wmi_pdev_pktlog_disable_cmd_fixed_param *disable_cmd;
	int len = 0;
	wmi_buf_t buf;

	/*Check if packet log is enabled in cfg.ini*/
	if (! vos_is_packet_log_enabled())
	{
		WMA_LOGE("%s:pkt log is not enabled in cfg.ini", __func__);
		return VOS_STATUS_E_FAILURE;
	}


	PKTLOG_EVENT = params->pktlog_event;
	CMD_ID = params->cmd_id;

	switch (CMD_ID) {
	case WMI_PDEV_PKTLOG_ENABLE_CMDID:
		len = sizeof(*cmd);
		buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
		if (!buf) {
			WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
			return VOS_STATUS_E_NOMEM;
		}
		cmd =
		    (wmi_pdev_pktlog_enable_cmd_fixed_param *)wmi_buf_data(buf);
		WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_pdev_pktlog_enable_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_pdev_pktlog_enable_cmd_fixed_param));
		cmd->evlist = PKTLOG_EVENT;
		if (wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
					 WMI_PDEV_PKTLOG_ENABLE_CMDID)) {
			WMA_LOGE("failed to send pktlog enable cmdid");
			goto wmi_send_failed;
		}
		break;
	case WMI_PDEV_PKTLOG_DISABLE_CMDID:
		len = sizeof(*disable_cmd);
		buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
		if (!buf) {
			WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
			return VOS_STATUS_E_NOMEM;
		}
		disable_cmd = (wmi_pdev_pktlog_disable_cmd_fixed_param *)
			      wmi_buf_data(buf);
		WMITLV_SET_HDR(&disable_cmd->tlv_header,
		      WMITLV_TAG_STRUC_wmi_pdev_pktlog_disable_cmd_fixed_param,
		      WMITLV_GET_STRUCT_TLVLEN(
			      wmi_pdev_pktlog_disable_cmd_fixed_param));
		disable_cmd->reserved0 = 0;
		if (wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
					 WMI_PDEV_PKTLOG_DISABLE_CMDID)) {
			WMA_LOGE("failed to send pktlog disable cmdid");
			goto wmi_send_failed;
		}
		break;
	default:
		WMA_LOGD("%s: invalid PKTLOG command", __func__);
		break;
	}

	return VOS_STATUS_SUCCESS;

wmi_send_failed:
	wmi_buf_free(buf);
	return VOS_STATUS_E_FAILURE;
}
#endif

static int32_t wmi_unified_set_sta_ps(wmi_unified_t wmi_handle,
                               u_int32_t vdev_id, u_int8_t val)
{
        wmi_sta_powersave_mode_cmd_fixed_param *cmd;
        wmi_buf_t buf;
        int32_t len = sizeof(*cmd);

        WMA_LOGD("Set Sta Mode Ps vdevId %d val %d", vdev_id, val);

        buf = wmi_buf_alloc(wmi_handle, len);
        if (!buf) {
                WMA_LOGP("%s: Set Sta Mode Ps Mem Alloc Failed", __func__);
                return -ENOMEM;
        }
        cmd = (wmi_sta_powersave_mode_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_sta_powersave_mode_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_sta_powersave_mode_cmd_fixed_param));
        cmd->vdev_id = vdev_id;
        if(val)
                cmd->sta_ps_mode = WMI_STA_PS_MODE_ENABLED;
        else
                cmd->sta_ps_mode = WMI_STA_PS_MODE_DISABLED;

        if(wmi_unified_cmd_send(wmi_handle, buf, len,
                       WMI_STA_POWERSAVE_MODE_CMDID))
        {
                WMA_LOGE("Set Sta Mode Ps Failed vdevId %d val %d",
                         vdev_id, val);
                adf_nbuf_free(buf);
                return -EIO;
        }
        return 0;
}

static inline u_int32_t wma_get_uapsd_mask(tpUapsd_Params uapsd_params)
{
	u_int32_t uapsd_val = 0;

	if(uapsd_params->beDeliveryEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC0_DELIVERY_EN;

	if(uapsd_params->beTriggerEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC0_TRIGGER_EN;

	if(uapsd_params->bkDeliveryEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC1_DELIVERY_EN;

	if(uapsd_params->bkTriggerEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC1_TRIGGER_EN;

	if(uapsd_params->viDeliveryEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC2_DELIVERY_EN;

	if(uapsd_params->viTriggerEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC2_TRIGGER_EN;

	if(uapsd_params->voDeliveryEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC3_DELIVERY_EN;

	if(uapsd_params->voTriggerEnabled)
		uapsd_val |= WMI_STA_PS_UAPSD_AC3_TRIGGER_EN;

	return uapsd_val;
}

static int32_t wma_set_force_sleep(tp_wma_handle wma, u_int32_t vdev_id,
			u_int8_t enable, u_int8_t is_qpower_enabled)
{
	int32_t ret;
	tANI_U32 cfg_data_val = 0;
	/* get mac to acess CFG data base */
	struct sAniSirGlobal *mac =
		(struct sAniSirGlobal*)vos_get_context(VOS_MODULE_ID_PE,
		wma->vos_context);
	u_int32_t rx_wake_policy;
	u_int32_t tx_wake_threshold;
	u_int32_t pspoll_count;
	u_int32_t inactivity_time;
	u_int32_t psmode;

	WMA_LOGD("Set Force Sleep vdevId %d val %d", vdev_id, enable);

	if (NULL == mac) {
		WMA_LOGE("%s: Unable to get PE context", __func__);
		return -ENOMEM;
	}

	/* Set Tx/Rx Data InActivity Timeout   */
	if (wlan_cfgGetInt(mac, WNI_CFG_PS_DATA_INACTIVITY_TIMEOUT,
		&cfg_data_val ) != eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
			"Failed to get WNI_CFG_PS_DATA_INACTIVITY_TIMEOUT");
			cfg_data_val = POWERSAVE_DEFAULT_INACTIVITY_TIME;
	}
	inactivity_time = (u_int32_t)cfg_data_val;

	if (enable) {
		/* override normal configuration and force station asleep */
		rx_wake_policy = WMI_STA_PS_RX_WAKE_POLICY_POLL_UAPSD;
		tx_wake_threshold = WMI_STA_PS_TX_WAKE_THRESHOLD_NEVER;

		if (wlan_cfgGetInt(mac, WNI_CFG_MAX_PS_POLL,
			&cfg_data_val ) != eSIR_SUCCESS) {
			VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get value for WNI_CFG_MAX_PS_POLL");
		}
		if (cfg_data_val)
			pspoll_count = (u_int32_t)cfg_data_val;
		else
			pspoll_count = WMA_DEFAULT_MAX_PSPOLL_BEFORE_WAKE;

		psmode = WMI_STA_PS_MODE_ENABLED;
	} else {
		/* Ps Poll Wake Policy */
		if (wlan_cfgGetInt(mac, WNI_CFG_MAX_PS_POLL,
				&cfg_data_val ) != eSIR_SUCCESS) {
			VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get value for WNI_CFG_MAX_PS_POLL");
		}
		if (cfg_data_val) {
			/* Ps Poll is enabled */
			rx_wake_policy = WMI_STA_PS_RX_WAKE_POLICY_POLL_UAPSD;
			pspoll_count = (u_int32_t)cfg_data_val;
			tx_wake_threshold = WMI_STA_PS_TX_WAKE_THRESHOLD_NEVER;
		} else {
			rx_wake_policy = WMI_STA_PS_RX_WAKE_POLICY_WAKE;
			pspoll_count = WMI_STA_PS_PSPOLL_COUNT_NO_MAX;
			tx_wake_threshold = WMI_STA_PS_TX_WAKE_THRESHOLD_ALWAYS;
		}
		psmode = WMI_STA_PS_MODE_ENABLED;
	}

	/*
	 * QPower is enabled by default in Firmware
	 * So Disable QPower explicitly
	 */
	ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
				WMI_STA_PS_ENABLE_QPOWER, is_qpower_enabled);
	if (ret) {
		WMA_LOGE("Disable QPower Failed vdevId %d", vdev_id);
		return ret;
	}
	WMA_LOGD("QPower Disabled vdevId %d", vdev_id);

	/* Set the Wake Policy to WMI_STA_PS_RX_WAKE_POLICY_POLL_UAPSD*/
	ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					WMI_STA_PS_PARAM_RX_WAKE_POLICY,
					rx_wake_policy);

	if (ret) {
		WMA_LOGE("Setting wake policy Failed vdevId %d", vdev_id);
		return ret;
	}
	WMA_LOGD("Setting wake policy to %d vdevId %d",
		rx_wake_policy, vdev_id);

	/* Set the Tx Wake Threshold */
	ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					WMI_STA_PS_PARAM_TX_WAKE_THRESHOLD,
					tx_wake_threshold);

	if (ret) {
		WMA_LOGE("Setting TxWake Threshold vdevId %d", vdev_id);
		return ret;
	}
	WMA_LOGD("Setting TxWake Threshold to %d vdevId %d",
		tx_wake_threshold, vdev_id);

	/* Set the Ps Poll Count */
	ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					WMI_STA_PS_PARAM_PSPOLL_COUNT,
					pspoll_count);

	if (ret) {
		WMA_LOGE("Set Ps Poll Count Failed vdevId %d ps poll cnt %d",
			vdev_id, pspoll_count);
		return ret;
	}
	WMA_LOGD("Set Ps Poll Count vdevId %d ps poll cnt %d",
		vdev_id, pspoll_count);

	/* Set the Tx/Rx InActivity */
	ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					WMI_STA_PS_PARAM_INACTIVITY_TIME,
					inactivity_time);

	if (ret) {
		WMA_LOGE("Setting Tx/Rx InActivity Failed vdevId %d InAct %d",
			vdev_id, inactivity_time);
		return ret;
	}
	WMA_LOGD("Set Tx/Rx InActivity vdevId %d InAct %d",
		vdev_id, inactivity_time);

	/* Enable Sta Mode Power save */
	ret = wmi_unified_set_sta_ps(wma->wmi_handle, vdev_id, true);

	if (ret) {
		WMA_LOGE("Enable Sta Mode Ps Failed vdevId %d", vdev_id);
		return ret;
	}

	/* Set Listen Interval */
	if (wlan_cfgGetInt(mac, WNI_CFG_LISTEN_INTERVAL,
			&cfg_data_val ) != eSIR_SUCCESS)	{
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
			"Failed to get value for WNI_CFG_LISTEN_INTERVAL");
		cfg_data_val = POWERSAVE_DEFAULT_LISTEN_INTERVAL;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
					WMI_VDEV_PARAM_LISTEN_INTERVAL,
					cfg_data_val);
	if (ret) {
		/* Even it fails continue Fw will take default LI */
		WMA_LOGE("Failed to Set Listen Interval vdevId %d",
			vdev_id);
	}
	WMA_LOGD("Set Listen Interval vdevId %d Listen Intv %d",
		vdev_id, cfg_data_val);
	return 0;
}

int32_t wma_set_qpower_force_sleep(tp_wma_handle wma, u_int32_t vdev_id,
							u_int8_t enable)
{
	int32_t ret;
	tANI_U32 cfg_data_val = 0;
	/* get mac to acess CFG data base */
	struct sAniSirGlobal *mac =
		(struct sAniSirGlobal*)vos_get_context(VOS_MODULE_ID_PE,
		wma->vos_context);
	u_int32_t pspoll_count = WMA_DEFAULT_MAX_PSPOLL_BEFORE_WAKE;

	WMA_LOGE("Set QPower Force(1)/Normal(0) Sleep vdevId %d val %d",
		vdev_id, enable);

	if (NULL == mac) {
		WMA_LOGE("%s: Unable to get PE context", __func__);
		return -ENOMEM;
	}

	/* Get Configured Ps Poll Count */
	if (wlan_cfgGetInt(mac, WNI_CFG_MAX_PS_POLL,
			&cfg_data_val ) != eSIR_SUCCESS) {
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
			"Failed to get value for WNI_CFG_MAX_PS_POLL");
	}
	if (cfg_data_val) {
		pspoll_count = (u_int32_t)cfg_data_val;
	}

	/* Enable QPower */
	ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					WMI_STA_PS_ENABLE_QPOWER, 1);

	if (ret) {
		WMA_LOGE("Enable QPower Failed vdevId %d", vdev_id);
		return ret;
	}
	WMA_LOGD("QPower Enabled vdevId %d", vdev_id);

	/* Set the Wake Policy to WMI_STA_PS_RX_WAKE_POLICY_POLL_UAPSD*/
	ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					WMI_STA_PS_PARAM_RX_WAKE_POLICY,
					WMI_STA_PS_RX_WAKE_POLICY_POLL_UAPSD);

	if (ret) {
		WMA_LOGE("Setting wake policy to pspoll/uapsd Failed vdevId %d", vdev_id);
		return ret;
	}
	WMA_LOGD("Wake policy set to to pspoll/uapsd vdevId %d",
		vdev_id);

	if (enable) {
		/* Set the Tx Wake Threshold */
		ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
						WMI_STA_PS_PARAM_TX_WAKE_THRESHOLD,
						WMI_STA_PS_TX_WAKE_THRESHOLD_NEVER);

		if (ret) {
			WMA_LOGE("Setting TxWake Threshold vdevId %d", vdev_id);
			return ret;
		}
		WMA_LOGD("TxWake Threshold set to TX_WAKE_THRESHOLD_NEVER %d", vdev_id);
	}

	/* Set the QPower Ps Poll Count */
	ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					WMI_STA_PS_PARAM_QPOWER_PSPOLL_COUNT,
					pspoll_count);

	if (ret) {
		WMA_LOGE("Set QPower Ps Poll Count Failed vdevId %d ps poll cnt %d",
			vdev_id, pspoll_count);
		return ret;
	}
	WMA_LOGD("Set QPower Ps Poll Count vdevId %d ps poll cnt %d",
		vdev_id, pspoll_count);

	/* Enable Sta Mode Power save */
	ret = wmi_unified_set_sta_ps(wma->wmi_handle, vdev_id, true);

	if (ret) {
		WMA_LOGE("Enable Sta Mode Ps Failed vdevId %d", vdev_id);
		return ret;
	}

	/* Set Listen Interval */
	if (wlan_cfgGetInt(mac, WNI_CFG_LISTEN_INTERVAL,
			&cfg_data_val ) != eSIR_SUCCESS)	{
		VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
			"Failed to get value for WNI_CFG_LISTEN_INTERVAL");
		cfg_data_val = POWERSAVE_DEFAULT_LISTEN_INTERVAL;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
					WMI_VDEV_PARAM_LISTEN_INTERVAL,
					cfg_data_val);
	if (ret) {
		/* Even it fails continue Fw will take default LI */
		WMA_LOGE("Failed to Set Listen Interval vdevId %d",
			vdev_id);
	}
	WMA_LOGD("Set Listen Interval vdevId %d Listen Intv %d",
		vdev_id, cfg_data_val);
	return 0;
}

static u_int8_t wma_is_qpower_enabled(tp_wma_handle wma)
{
	if((wma->powersave_mode == PS_QPOWER_NODEEPSLEEP) ||
		(wma->powersave_mode == PS_QPOWER_DEEPSLEEP)) {
		return true;
	}
	return false;
}

static void wma_enable_sta_ps_mode(tp_wma_handle wma, tpEnablePsParams ps_req)
{
	uint32_t vdev_id = ps_req->sessionid;
	int32_t ret;
	u_int8_t is_qpower_enabled = wma_is_qpower_enabled(wma);
	struct wma_txrx_node *iface = &wma->interfaces[vdev_id];

	if (eSIR_ADDON_NOTHING == ps_req->psSetting) {
		WMA_LOGD("Enable Sta Mode Ps vdevId %d", vdev_id);
		ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
						WMI_STA_PS_PARAM_UAPSD, 0);
		if (ret) {
			WMA_LOGE("Set Uapsd param 0 Failed vdevId %d", vdev_id);
			ps_req->status = VOS_STATUS_E_FAILURE;
			goto resp;
		}

		ret = wma_set_force_sleep(wma, vdev_id, false,
						is_qpower_enabled);
		if (ret) {
			WMA_LOGE("Enable Sta Ps Failed vdevId %d", vdev_id);
			ps_req->status = VOS_STATUS_E_FAILURE;
			goto resp;
		}
	} else if (eSIR_ADDON_ENABLE_UAPSD == ps_req->psSetting) {
		u_int32_t uapsd_val = 0;
		uapsd_val = wma_get_uapsd_mask(&ps_req->uapsdParams);

		if(uapsd_val != iface->uapsd_cached_val) {
			WMA_LOGD("Enable Uapsd vdevId %d Mask %d",
				vdev_id, uapsd_val);
			ret = wmi_unified_set_sta_ps_param(wma->wmi_handle,
						vdev_id, WMI_STA_PS_PARAM_UAPSD,
						uapsd_val);
			if (ret) {
				WMA_LOGE("Enable Uapsd Failed vdevId %d",
					vdev_id);
				ps_req->status = VOS_STATUS_E_FAILURE;
				goto resp;
			}
			/* Cache the Uapsd Mask */
			iface->uapsd_cached_val = uapsd_val;
		} else {
			WMA_LOGD("Already Uapsd Enabled vdevId %d Mask %d",
				vdev_id, uapsd_val);
		}

		WMA_LOGD("Enable Forced Sleep vdevId %d", vdev_id);

		ret = wma_set_force_sleep(wma, vdev_id, true,
						is_qpower_enabled);

		if (ret) {
			WMA_LOGE("Enable Forced Sleep Failed vdevId %d",
				vdev_id);
			ps_req->status = VOS_STATUS_E_FAILURE;
			goto resp;
		}
	}
	ps_req->status = VOS_STATUS_SUCCESS;
	iface->dtimPeriod = ps_req->bcnDtimPeriod;
resp:
	wma_send_msg(wma, WDA_ENTER_BMPS_RSP, ps_req, 0);
}

static void wma_disable_sta_ps_mode(tp_wma_handle wma, tpDisablePsParams ps_req)
{
        int32_t ret;
        uint32_t vdev_id = ps_req->sessionid;

        WMA_LOGD("Disable Sta Mode Ps vdevId %d", vdev_id);

        /* Disable Sta Mode Power save */
        ret = wmi_unified_set_sta_ps(wma->wmi_handle, vdev_id, false);
        if(ret) {
                WMA_LOGE("Disable Sta Mode Ps Failed vdevId %d", vdev_id);
                ps_req->status = VOS_STATUS_E_FAILURE;
                goto resp;
        }

	/* Disable UAPSD incase if additional Req came */
	if (eSIR_ADDON_DISABLE_UAPSD == ps_req->psSetting) {
		WMA_LOGD("Disable Uapsd vdevId %d", vdev_id);
		ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
						WMI_STA_PS_PARAM_UAPSD, 0);
		if (ret) {
			WMA_LOGE("Disable Uapsd Failed vdevId %d", vdev_id);
			/*
			 * Even this fails we can proceed as success
			 * since we disabled powersave
			 */
		}
	}

        ps_req->status = VOS_STATUS_SUCCESS;
resp:
        wma_send_msg(wma, WDA_EXIT_BMPS_RSP, ps_req, 0);
}

static void wma_enable_uapsd_mode(tp_wma_handle wma,
				tpEnableUapsdParams ps_req)
{
	int32_t ret;
	u_int32_t vdev_id = ps_req->sessionid;
	u_int32_t uapsd_val = 0;
	u_int8_t is_qpower_enabled = wma_is_qpower_enabled(wma);

	/* Disable Sta Mode Power save */
	ret = wmi_unified_set_sta_ps(wma->wmi_handle, vdev_id, false);
	if (ret) {
		WMA_LOGE("Disable Sta Mode Ps Failed vdevId %d", vdev_id);
		ps_req->status = VOS_STATUS_E_FAILURE;
		goto resp;
	}

	uapsd_val = wma_get_uapsd_mask(&ps_req->uapsdParams);

	WMA_LOGD("Enable Uapsd vdevId %d Mask %d", vdev_id, uapsd_val);
	ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
				WMI_STA_PS_PARAM_UAPSD, uapsd_val);
	if (ret) {
		WMA_LOGE("Enable Uapsd Failed vdevId %d", vdev_id);
		ps_req->status = VOS_STATUS_E_FAILURE;
		goto resp;
	}

	WMA_LOGD("Enable Forced Sleep vdevId %d", vdev_id);

	ret = wma_set_force_sleep(wma, vdev_id, true,
						is_qpower_enabled);
	if (ret) {
		WMA_LOGE("Enable Forced Sleep Failed vdevId %d", vdev_id);
		ps_req->status = VOS_STATUS_E_FAILURE;
		goto resp;
	}

	ps_req->status = VOS_STATUS_SUCCESS;
resp:
	wma_send_msg(wma, WDA_ENTER_UAPSD_RSP, ps_req, 0);
}

static void wma_disable_uapsd_mode(tp_wma_handle wma,
			tpDisableUapsdParams ps_req)
{
	int32_t ret;
	u_int32_t vdev_id = ps_req->sessionid;
	u_int8_t is_qpower_enabled = wma_is_qpower_enabled(wma);

	WMA_LOGD("Disable Uapsd vdevId %d", vdev_id);

	/* Disable Sta Mode Power save */
	ret = wmi_unified_set_sta_ps(wma->wmi_handle, vdev_id, false);
	if (ret) {
		WMA_LOGE("Disable Sta Mode Ps Failed vdevId %d", vdev_id);
		ps_req->status = VOS_STATUS_E_FAILURE;
		goto resp;
	}

	ret = wmi_unified_set_sta_ps_param(wma->wmi_handle, vdev_id,
					WMI_STA_PS_PARAM_UAPSD, 0);
	if (ret) {
		WMA_LOGE("Disable Uapsd Failed vdevId %d", vdev_id);
		ps_req->status = VOS_STATUS_E_FAILURE;
		goto resp;
	}

	/* Re enable Sta Mode Powersave with proper configuration */
	ret = wma_set_force_sleep(wma, vdev_id, false,
						is_qpower_enabled);
	if (ret) {
		WMA_LOGE("Disable Forced Sleep Failed vdevId %d", vdev_id);
		ps_req->status = VOS_STATUS_E_FAILURE;
		goto resp;
	}

	ps_req->status = VOS_STATUS_SUCCESS;
resp:
	wma_send_msg(wma, WDA_EXIT_UAPSD_RSP, ps_req, 0);
}

static void wma_set_keepalive_req(tp_wma_handle wma,
				  tSirKeepAliveReq *keepalive)
{
	WMA_LOGD("KEEPALIVE:PacketType:%d", keepalive->packetType);
	wma_set_sta_keep_alive(wma, keepalive->sessionId,
				    keepalive->packetType,
				    keepalive->timePeriod,
				    keepalive->hostIpv4Addr,
				    keepalive->destIpv4Addr,
				    keepalive->destMacAddr);

	vos_mem_free(keepalive);
}
/*
 * This function sets the trigger uapsd
 * params such as service interval, delay
 * interval and suspend interval which
 * will be used by the firmware to send
 * trigger frames periodically when there
 * is no traffic on the transmit side.
 */
int32_t
wmi_unified_set_sta_uapsd_auto_trig_cmd(
        wmi_unified_t wmi_handle,
        u_int32_t vdevid,
        u_int8_t peer_addr[IEEE80211_ADDR_LEN],
        u_int8_t *autoTriggerparam,
        u_int32_t num_ac)
{
	wmi_sta_uapsd_auto_trig_cmd_fixed_param *cmd;
	int32_t ret;
	u_int32_t param_len = num_ac *
				sizeof(wmi_sta_uapsd_auto_trig_param);
	u_int32_t cmd_len = sizeof(*cmd) + param_len + WMI_TLV_HDR_SIZE;
	u_int32_t i;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;

	buf = wmi_buf_alloc(wmi_handle, cmd_len);
	if (!buf) {
		WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_sta_uapsd_auto_trig_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_sta_uapsd_auto_trig_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				wmi_sta_uapsd_auto_trig_cmd_fixed_param));
	cmd->vdev_id = vdevid;
	cmd->num_ac = num_ac;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(peer_addr, &cmd->peer_macaddr);

	/* TLV indicating array of structures to follow */
	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, param_len);

	buf_ptr += WMI_TLV_HDR_SIZE;
	vos_mem_copy(buf_ptr, autoTriggerparam, param_len);

	/*
	 * Update tag and length for uapsd auto trigger params (this will take
	 * care of updating tag and length if it is not pre-filled by caller).
	 */
	for (i = 0; i < num_ac; i++) {
		WMITLV_SET_HDR((buf_ptr +
			       (i * sizeof(wmi_sta_uapsd_auto_trig_param))),
				WMITLV_TAG_STRUC_wmi_sta_uapsd_auto_trig_param,
				WMITLV_GET_STRUCT_TLVLEN(
					wmi_sta_uapsd_auto_trig_param));
	}

	ret = wmi_unified_cmd_send(wmi_handle, buf, cmd_len,
				WMI_STA_UAPSD_AUTO_TRIG_CMDID);
	if (ret != EOK) {
		WMA_LOGE("Failed to send set uapsd param ret = %d", ret);
		wmi_buf_free(buf);
	}
	return ret;
}

/*
 * This function sets the trigger uapsd
 * params such as service interval, delay
 * interval and suspend interval which
 * will be used by the firmware to send
 * trigger frames periodically when there
 * is no traffic on the transmit side.
 */
VOS_STATUS wma_trigger_uapsd_params(tp_wma_handle wma_handle, u_int32_t vdev_id,
			tp_wma_trigger_uapsd_params trigger_uapsd_params)
{
	int32_t ret;
	wmi_sta_uapsd_auto_trig_param uapsd_trigger_param;

	WMA_LOGD("Trigger uapsd params vdev id %d", vdev_id);

	WMA_LOGD("WMM AC %d User Priority %d SvcIntv %d DelIntv %d SusIntv %d",
		trigger_uapsd_params->wmm_ac,
		trigger_uapsd_params->user_priority,
		trigger_uapsd_params->service_interval,
		trigger_uapsd_params->delay_interval,
		trigger_uapsd_params->suspend_interval);

	if (!WMI_SERVICE_IS_ENABLED(wma_handle->wmi_service_bitmap,
				 WMI_STA_UAPSD_BASIC_AUTO_TRIG) ||
		!WMI_SERVICE_IS_ENABLED(wma_handle->wmi_service_bitmap,
				 WMI_STA_UAPSD_VAR_AUTO_TRIG)) {
		WMA_LOGD("Trigger uapsd is not supported vdev id %d", vdev_id);
		return VOS_STATUS_SUCCESS;
	}

	uapsd_trigger_param.wmm_ac =
				trigger_uapsd_params->wmm_ac;
	uapsd_trigger_param.user_priority =
				trigger_uapsd_params->user_priority;
	uapsd_trigger_param.service_interval =
				trigger_uapsd_params->service_interval;
	uapsd_trigger_param.suspend_interval =
				trigger_uapsd_params->suspend_interval;
	uapsd_trigger_param.delay_interval =
				trigger_uapsd_params->delay_interval;

	ret = wmi_unified_set_sta_uapsd_auto_trig_cmd(wma_handle->wmi_handle, vdev_id,
					wma_handle->interfaces[vdev_id].bssid,
					(u_int8_t*)(&uapsd_trigger_param),
					1);
	if (ret) {
		WMA_LOGE("Fail to send uapsd param cmd for vdevid %d ret = %d",
			ret, vdev_id);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS wma_disable_uapsd_per_ac(tp_wma_handle wma_handle,
					u_int32_t vdev_id,
					enum uapsd_ac ac)
{
	int32_t ret;
	struct wma_txrx_node *iface = &wma_handle->interfaces[vdev_id];
	wmi_sta_uapsd_auto_trig_param uapsd_trigger_param;
	enum uapsd_up user_priority;

	WMA_LOGD("Disable Uapsd per ac vdevId %d ac %d", vdev_id, ac);

	switch (ac) {
		case UAPSD_VO:
			iface->uapsd_cached_val &=
					~(WMI_STA_PS_UAPSD_AC3_DELIVERY_EN |
					WMI_STA_PS_UAPSD_AC3_TRIGGER_EN);
			user_priority = UAPSD_UP_VO;
			break;
		case UAPSD_VI:
			iface->uapsd_cached_val &=
					~(WMI_STA_PS_UAPSD_AC2_DELIVERY_EN |
					WMI_STA_PS_UAPSD_AC2_TRIGGER_EN);
			user_priority = UAPSD_UP_VI;
			break;
		case UAPSD_BK:
			iface->uapsd_cached_val &=
					~(WMI_STA_PS_UAPSD_AC1_DELIVERY_EN |
					WMI_STA_PS_UAPSD_AC1_TRIGGER_EN);
			user_priority = UAPSD_UP_BK;
			break;
		case UAPSD_BE:
			iface->uapsd_cached_val &=
					~(WMI_STA_PS_UAPSD_AC0_DELIVERY_EN |
					WMI_STA_PS_UAPSD_AC0_TRIGGER_EN);
			user_priority = UAPSD_UP_BE;
			break;
		default:
			WMA_LOGE("Invalid AC vdevId %d ac %d", vdev_id, ac);
			return VOS_STATUS_E_FAILURE;
	}

	/*
	 * Disable Auto Trigger Functionality before
	 * disabling uapsd for a particular AC
	 */
	uapsd_trigger_param.wmm_ac = ac;
	uapsd_trigger_param.user_priority = user_priority;
	uapsd_trigger_param.service_interval = 0;
	uapsd_trigger_param.suspend_interval = 0;
	uapsd_trigger_param.delay_interval = 0;

	ret = wmi_unified_set_sta_uapsd_auto_trig_cmd(wma_handle->wmi_handle,
					vdev_id,
					wma_handle->interfaces[vdev_id].bssid,
					(u_int8_t*)(&uapsd_trigger_param),
					1);
	if (ret) {
		WMA_LOGE("Fail to send auto trig cmd for vdevid %d ret = %d",
			ret, vdev_id);
		return VOS_STATUS_E_FAILURE;
	}

	ret = wmi_unified_set_sta_ps_param(wma_handle->wmi_handle, vdev_id,
			WMI_STA_PS_PARAM_UAPSD, iface->uapsd_cached_val);
	if (ret) {
		WMA_LOGE("Disable Uapsd per ac Failed vdevId %d ac %d", vdev_id, ac);
		return VOS_STATUS_E_FAILURE;
	}
	WMA_LOGD("Disable Uapsd per ac vdevId %d val %d", vdev_id,
		iface->uapsd_cached_val);
	return VOS_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_SCAN_PNO

/* Request FW to start PNO operation */
static VOS_STATUS wma_pno_start(tp_wma_handle wma, tpSirPNOScanReq pno)
{
	wmi_nlo_config_cmd_fixed_param *cmd;
	nlo_configured_parameters *nlo_list;
	u_int32_t *channel_list;
	int32_t len;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	u_int8_t i;
	int ret;

	WMA_LOGD("PNO Start");

	len = sizeof(*cmd) +
	      WMI_TLV_HDR_SIZE + /* TLV place holder for array of structures nlo_configured_parameters(nlo_list) */
	      WMI_TLV_HDR_SIZE; /* TLV place holder for array of uint32 channel_list */

	len += sizeof(u_int32_t) * MIN(pno->aNetworks[0].ucChannelCount,
				   WMI_NLO_MAX_CHAN);
	len += sizeof(nlo_configured_parameters) *
				MIN(pno->ucNetworksCount, WMI_NLO_MAX_SSIDS);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (wmi_nlo_config_cmd_fixed_param *) wmi_buf_data(buf);

	buf_ptr = (u_int8_t *) cmd;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_nlo_config_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_nlo_config_cmd_fixed_param));
	cmd->vdev_id = pno->sessionId;
	cmd->flags = WMI_NLO_CONFIG_START | WMI_NLO_CONFIG_SSID_HIDE_EN;

	/* Copy scan interval */
	cmd->fast_scan_period = pno->fast_scan_period;
	cmd->slow_scan_period = pno->slow_scan_period;
	cmd->fast_scan_max_cycles = pno->fast_scan_max_cycles;
	WMA_LOGD("fast_scan_period: %d msec slow_scan_period: %d msec",
			cmd->fast_scan_period, cmd->slow_scan_period);
	WMA_LOGD("fast_scan_max_cycles: %d", cmd->fast_scan_max_cycles);

	buf_ptr += sizeof(wmi_nlo_config_cmd_fixed_param);

	cmd->no_of_ssids = MIN(pno->ucNetworksCount, WMI_NLO_MAX_SSIDS);
	WMA_LOGD("SSID count : %d", cmd->no_of_ssids);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       cmd->no_of_ssids * sizeof(nlo_configured_parameters));
	buf_ptr += WMI_TLV_HDR_SIZE;

	nlo_list = (nlo_configured_parameters *) buf_ptr;
	for (i = 0; i < cmd->no_of_ssids; i++) {
		WMITLV_SET_HDR(&nlo_list[i].tlv_header,
			WMITLV_TAG_ARRAY_BYTE,
			WMITLV_GET_STRUCT_TLVLEN(nlo_configured_parameters));
		/* Copy ssid and it's length */
		nlo_list[i].ssid.valid = TRUE;
		nlo_list[i].ssid.ssid.ssid_len = pno->aNetworks[i].ssId.length;
		vos_mem_copy(nlo_list[i].ssid.ssid.ssid,
			     pno->aNetworks[i].ssId.ssId,
			     nlo_list[i].ssid.ssid.ssid_len);
		WMA_LOGD("index: %d ssid: %.*s len: %d", i,
			 nlo_list[i].ssid.ssid.ssid_len,
			 (char *) nlo_list[i].ssid.ssid.ssid,
			 nlo_list[i].ssid.ssid.ssid_len);

		/* Copy rssi threshold */
		if (pno->aNetworks[i].rssiThreshold &&
		    pno->aNetworks[i].rssiThreshold > WMA_RSSI_THOLD_DEFAULT) {
			nlo_list[i].rssi_cond.valid = TRUE;
			nlo_list[i].rssi_cond.rssi =
				pno->aNetworks[i].rssiThreshold;
			WMA_LOGD("RSSI threshold : %d dBm",
				nlo_list[i].rssi_cond.rssi);
		}
		nlo_list[i].bcast_nw_type.valid = TRUE;
		nlo_list[i].bcast_nw_type.bcast_nw_type =
					 pno->aNetworks[i].bcastNetwType;
		WMA_LOGI("Broadcast NW type (%u)",
				nlo_list[i].bcast_nw_type.bcast_nw_type);
	}
	buf_ptr += cmd->no_of_ssids * sizeof(nlo_configured_parameters);

	/* Copy channel info */
	cmd->num_of_channels = MIN(pno->aNetworks[0].ucChannelCount,
				   WMI_NLO_MAX_CHAN);
	WMA_LOGD("Channel count: %d", cmd->num_of_channels);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		       (cmd->num_of_channels * sizeof(u_int32_t)));
	buf_ptr += WMI_TLV_HDR_SIZE;

	channel_list = (u_int32_t *) buf_ptr;
	for (i = 0; i < cmd->num_of_channels; i++) {
		channel_list[i] = pno->aNetworks[0].aChannels[i];

		if (channel_list[i] < WMA_NLO_FREQ_THRESH)
			channel_list[i] = vos_chan_to_freq(channel_list[i]);

		WMA_LOGD("Ch[%d]: %d MHz", i, channel_list[i]);
	}
	buf_ptr += cmd->num_of_channels * sizeof(u_int32_t);

	/* TODO: Discrete firmware doesn't have command/option to configure
	 * App IE which comes from wpa_supplicant as of part PNO start request.
	 */
	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send nlo wmi cmd", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	wma->interfaces[pno->sessionId].pno_in_progress = TRUE;

	WMA_LOGD("PNO start request sent successfully for vdev %d",
		 pno->sessionId);

	return VOS_STATUS_SUCCESS;
}

/* Request FW to stop ongoing PNO operation */
static VOS_STATUS wma_pno_stop(tp_wma_handle wma, u_int8_t vdev_id)
{
	wmi_nlo_config_cmd_fixed_param *cmd;
	int32_t len = sizeof(*cmd);
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	int ret;

	if (!wma->interfaces[vdev_id].pno_in_progress) {
		WMA_LOGD("No active pno session found for vdev %d, skip pno stop request",
			 vdev_id);
		return VOS_STATUS_SUCCESS;
	}

	WMA_LOGD("PNO Stop");

	len += WMI_TLV_HDR_SIZE + /* TLV place holder for array of structures nlo_configured_parameters(nlo_list) */
	       WMI_TLV_HDR_SIZE; /* TLV place holder for array of uint32 channel_list */
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (wmi_nlo_config_cmd_fixed_param *) wmi_buf_data(buf);
	buf_ptr = (u_int8_t *) cmd;

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_nlo_config_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_nlo_config_cmd_fixed_param));

	cmd->vdev_id = vdev_id;
	cmd->flags = WMI_NLO_CONFIG_STOP;
	buf_ptr += sizeof(*cmd);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	buf_ptr += WMI_TLV_HDR_SIZE;

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32, 0);
	buf_ptr += WMI_TLV_HDR_SIZE;

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send nlo wmi cmd", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	wma->interfaces[vdev_id].pno_in_progress = FALSE;

	WMA_LOGD("PNO stop request sent successfully for vdev %d",
		 vdev_id);

	return VOS_STATUS_SUCCESS;
}

static void wma_config_pno(tp_wma_handle wma, tpSirPNOScanReq pno)
{
	VOS_STATUS ret;

	if (pno->enable)
		ret = wma_pno_start(wma, pno);
	else
		ret = wma_pno_stop(wma, pno->sessionId);

	if (ret)
		WMA_LOGE("%s: PNO %s failed %d", __func__,
			 pno->enable ? "start" : "stop", ret);

	/* SME expects WMA to free tpSirPNOScanReq memory after
	 * processing PNO request. */
	vos_mem_free(pno);
}

#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
static VOS_STATUS wma_plm_start(tp_wma_handle wma, const tpSirPlmReq plm)
{
	wmi_vdev_plmreq_start_cmd_fixed_param *cmd;
	u_int32_t *channel_list;
	int32_t len;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	u_int8_t count;
	int ret;

        if (NULL == plm || NULL == wma) {
		WMA_LOGE("%s: input pointer is NULL ", __func__);
		return VOS_STATUS_E_FAILURE;
	}
	WMA_LOGD("PLM Start");

	len = sizeof(*cmd) +
		WMI_TLV_HDR_SIZE; /* TLV place holder for channel_list */
	len += sizeof(u_int32_t) * plm->plmNumCh;

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	cmd = (wmi_vdev_plmreq_start_cmd_fixed_param *) wmi_buf_data(buf);

	buf_ptr = (u_int8_t *) cmd;

        WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_plmreq_start_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
                       wmi_vdev_plmreq_start_cmd_fixed_param));

	cmd->vdev_id = plm->sessionId;

	cmd->meas_token = plm->meas_token;
	cmd->dialog_token = plm->diag_token;
	cmd->number_bursts = plm->numBursts;
        cmd->burst_interval = WMA_SEC_TO_MSEC(plm->burstInt);
	cmd->off_duration = plm->measDuration;
	cmd->burst_cycle = plm->burstLen;
	cmd->tx_power = plm->desiredTxPwr;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(plm->macAddr, &cmd->dest_mac);
	cmd->num_chans = plm->plmNumCh;

	buf_ptr += sizeof(wmi_vdev_plmreq_start_cmd_fixed_param);

	WMA_LOGD("vdev : %d measu token : %d", cmd->vdev_id, cmd->meas_token);
	WMA_LOGD("dialog_token: %d", cmd->dialog_token);
	WMA_LOGD("number_bursts: %d", cmd->number_bursts);
	WMA_LOGD("burst_interval: %d", cmd->burst_interval);
	WMA_LOGD("off_duration: %d", cmd->off_duration);
	WMA_LOGD("burst_cycle: %d", cmd->burst_cycle);
	WMA_LOGD("tx_power: %d", cmd->tx_power);
	WMA_LOGD("Number of channels : %d", cmd->num_chans);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
			(cmd->num_chans * sizeof(u_int32_t)));

	buf_ptr += WMI_TLV_HDR_SIZE;
	if (cmd->num_chans)
        {
		channel_list = (u_int32_t *) buf_ptr;
		for (count = 0; count < cmd->num_chans; count++) {
			channel_list[count] = plm->plmChList[count];
			if (channel_list[count] < WMA_NLO_FREQ_THRESH)
				channel_list[count] =
					vos_chan_to_freq(channel_list[count]);
			WMA_LOGD("Ch[%d]: %d MHz", count, channel_list[count]);
		}
		buf_ptr += cmd->num_chans * sizeof(u_int32_t);
	}

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
					WMI_VDEV_PLMREQ_START_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send plm start wmi cmd", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}
	wma->interfaces[plm->sessionId].plm_in_progress = TRUE;

	WMA_LOGD("Plm start request sent successfully for vdev %d",
		plm->sessionId);

	return VOS_STATUS_SUCCESS;
}

static VOS_STATUS wma_plm_stop(tp_wma_handle wma, const tpSirPlmReq plm)
{
	wmi_vdev_plmreq_stop_cmd_fixed_param *cmd;
	int32_t len;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	int ret;

	if (NULL == plm || NULL == wma) {
		WMA_LOGE("%s: input pointer is NULL ", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	if (FALSE == wma->interfaces[plm->sessionId].plm_in_progress) {
		WMA_LOGE("No active plm req found, skip plm stop req" );
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("PLM Stop");

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (wmi_vdev_plmreq_stop_cmd_fixed_param *) wmi_buf_data(buf);

	buf_ptr = (u_int8_t *) cmd;

	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_vdev_plmreq_stop_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
			wmi_vdev_plmreq_stop_cmd_fixed_param));

	cmd->vdev_id = plm->sessionId;

	cmd->meas_token = plm->meas_token;
	WMA_LOGD("vdev %d meas token %d", cmd->vdev_id, cmd->meas_token);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_VDEV_PLMREQ_STOP_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send plm stop wmi cmd", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}
	wma->interfaces[plm->sessionId].plm_in_progress = FALSE;

	WMA_LOGD("Plm stop request sent successfully for vdev %d",
		plm->sessionId);

	return VOS_STATUS_SUCCESS;
}
static void wma_config_plm(tp_wma_handle wma, tpSirPlmReq plm)
{
	VOS_STATUS ret = 0;

	if (NULL == plm || NULL == wma)
		return;

	if (plm->enable)
		ret = wma_plm_start(wma, plm);
	else
		ret = wma_plm_stop(wma, plm);

	if (ret)
		WMA_LOGE("%s: PLM %s failed %d", __func__,
			 plm->enable ? "start" : "stop", ret);

	/* SME expects WMA to free tpSirPlmReq memory after
	 * processing PLM request. */
	vos_mem_free(plm);
	plm = NULL;
}
#endif

/*
 * After pushing cached scan results (that are stored in LIM) to SME,
 * PE will post WDA_SME_SCAN_CACHE_UPDATED message indication to
 * wma and intern this function handles that message. This function will
 * check for PNO completion (by checking NLO match event) and post PNO
 * completion back to SME if PNO operation is completed successfully.
 */
void wma_scan_cache_updated_ind(tp_wma_handle wma, u_int8_t sessionId)
{
	tSirPrefNetworkFoundInd *nw_found_ind;
	VOS_STATUS status;
	vos_msg_t vos_msg;
	u_int8_t len, i;

	for (i = 0; i < wma->max_bssid; i++) {
		if (wma->interfaces[i].nlo_match_evt_received)
			break;
	}

	if (i == wma->max_bssid) {
		WMA_LOGD("PNO match event is not received in any vdev, skip scan cache update indication");
		return;
	}
	wma->interfaces[i].nlo_match_evt_received = FALSE;

	WMA_LOGD("Posting PNO completion to umac");

	len = sizeof(tSirPrefNetworkFoundInd);
	nw_found_ind = (tSirPrefNetworkFoundInd *) vos_mem_malloc(len);

	if (NULL == nw_found_ind) {
		WMA_LOGE("%s: Memory allocation failure", __func__);
		return;
	}

	nw_found_ind->mesgType = eWNI_SME_PREF_NETWORK_FOUND_IND;
	nw_found_ind->mesgLen = len;
	nw_found_ind->sessionId = sessionId;

	vos_msg.type = eWNI_SME_PREF_NETWORK_FOUND_IND;
	vos_msg.bodyptr = (void *) nw_found_ind;
	vos_msg.bodyval = 0;

	status = vos_mq_post_message(VOS_MQ_ID_SME, &vos_msg);
	if (status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("%s: Failed to post PNO completion match event to SME",
			 __func__);
		vos_mem_free(nw_found_ind);
	}
}

#endif

static void wma_send_status_to_suspend_ind(tp_wma_handle wma, boolean suspended,
		bool runtime_pm)
{
	tSirReadyToSuspendInd *ready_to_suspend;
	VOS_STATUS status;
	vos_msg_t vos_msg;
	u_int8_t len;

	if (runtime_pm) {
		WMA_LOGD("Runtime PM: Posting ready to suspend indication");
		vos_event_set(&wma->runtime_suspend);
		return;
	}

	WMA_LOGD("Posting ready to suspend indication to umac");

	len = sizeof(tSirReadyToSuspendInd);
	ready_to_suspend = (tSirReadyToSuspendInd *) vos_mem_malloc(len);

	if (NULL == ready_to_suspend) {
		WMA_LOGE("%s: Memory allocation failure", __func__);
		return;
	}

	ready_to_suspend->mesgType = eWNI_SME_READY_TO_SUSPEND_IND;
	ready_to_suspend->mesgLen = len;
	ready_to_suspend->suspended = suspended;

	vos_msg.type = eWNI_SME_READY_TO_SUSPEND_IND;
	vos_msg.bodyptr = (void *) ready_to_suspend;
	vos_msg.bodyval = 0;

	status = vos_mq_post_message(VOS_MQ_ID_SME, &vos_msg);
	if (status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to post ready to suspend");
		vos_mem_free(ready_to_suspend);
	}
}

/* Frees memory associated to given pattern ID in wow pattern cache. */
static inline void wma_free_wow_ptrn(tp_wma_handle wma, u_int8_t ptrn_id)
{
	if (wma->wow.no_of_ptrn_cached <= 0 ||
	    !wma->wow.cache[ptrn_id])
		return;

	WMA_LOGD("Deleting wow pattern %d from cache which belongs to vdev id %d",
		 ptrn_id, wma->wow.cache[ptrn_id]->vdev_id);

	vos_mem_free(wma->wow.cache[ptrn_id]->ptrn);
	vos_mem_free(wma->wow.cache[ptrn_id]->mask);
	vos_mem_free(wma->wow.cache[ptrn_id]);
	wma->wow.cache[ptrn_id] = NULL;

	wma->wow.no_of_ptrn_cached--;
}

/* Converts wow wakeup reason code to text format */
static const u8 *wma_wow_wake_reason_str(A_INT32 wake_reason, tp_wma_handle wma)
{
	switch (wake_reason) {
	case WOW_REASON_UNSPECIFIED:
		if (!wmi_get_runtime_pm_inprogress(wma->wmi_handle))
			return "UNSPECIFIED";
		else
			return "RUNTIME PM resume";
	case WOW_REASON_NLOD:
		return "NLOD";
	case WOW_REASON_AP_ASSOC_LOST:
		return "AP_ASSOC_LOST";
	case WOW_REASON_LOW_RSSI:
		return "LOW_RSSI";
	case WOW_REASON_DEAUTH_RECVD:
		return "DEAUTH_RECVD";
	case WOW_REASON_DISASSOC_RECVD:
		return "DISASSOC_RECVD";
	case WOW_REASON_GTK_HS_ERR:
		return "GTK_HS_ERR";
	case WOW_REASON_EAP_REQ:
		return "EAP_REQ";
	case WOW_REASON_FOURWAY_HS_RECV:
		return "FOURWAY_HS_RECV";
	case WOW_REASON_TIMER_INTR_RECV:
		return "TIMER_INTR_RECV";
	case WOW_REASON_PATTERN_MATCH_FOUND:
		return "PATTERN_MATCH_FOUND";
	case WOW_REASON_RECV_MAGIC_PATTERN:
		return "RECV_MAGIC_PATTERN";
	case WOW_REASON_P2P_DISC:
		return "P2P_DISC";
#ifdef FEATURE_WLAN_LPHB
	case WOW_REASON_WLAN_HB:
		return "WLAN_HB";
#endif /* FEATURE_WLAN_LPHB */

	case WOW_REASON_CSA_EVENT:
		return "CSA_EVENT";
	case WOW_REASON_PROBE_REQ_WPS_IE_RECV:
		return "PROBE_REQ_RECV";
	case WOW_REASON_AUTH_REQ_RECV:
		return "AUTH_REQ_RECV";
	case WOW_REASON_ASSOC_REQ_RECV:
		return "ASSOC_REQ_RECV";
	case WOW_REASON_HTT_EVENT:
		return "WOW_REASON_HTT_EVENT";
#ifdef FEATURE_WLAN_RA_FILTERING
	case  WOW_REASON_RA_MATCH:
		return "WOW_REASON_RA_MATCH";
#endif
	case WOW_REASON_BEACON_RECV:
		return "WOW_REASON_IBSS_BEACON_RECV";
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	case  WOW_REASON_HOST_AUTO_SHUTDOWN:
		return "WOW_REASON_HOST_AUTO_SHUTDOWN";
#endif
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	case  WOW_REASON_ROAM_HO:
		return "WOW_REASON_ROAM_HO";
#endif
#ifdef FEATURE_WLAN_EXTSCAN
	case WOW_REASON_EXTSCAN:
		return "WOW_REASON_EXTSCAN";
#endif
	case WOW_REASON_RSSI_BREACH_EVENT:
		return "WOW_REASON_RSSI_BREACH_EVENT";

	case WOW_REASON_NLO_SCAN_COMPLETE:
		return "WOW_REASON_NLO_SCAN_COMPLETE";
	}
	return "unknown";
}

static void wma_beacon_miss_handler(tp_wma_handle wma, u_int32_t vdev_id)
{
	tSirSmeMissedBeaconInd *beacon_miss_ind;

	beacon_miss_ind = (tSirSmeMissedBeaconInd *) vos_mem_malloc
		                             (sizeof(tSirSmeMissedBeaconInd));

	if (NULL == beacon_miss_ind) {
		WMA_LOGE("%s: Memory allocation failure", __func__);
		return;
	}
	beacon_miss_ind->messageType = WDA_MISSED_BEACON_IND;
	beacon_miss_ind->length = sizeof(tSirSmeMissedBeaconInd);
	beacon_miss_ind->bssIdx = vdev_id;

	wma_send_msg(wma, WDA_MISSED_BEACON_IND,
		         (void *)beacon_miss_ind, 0);
}

#ifdef FEATURE_WLAN_LPHB
static int wma_lphb_handler(tp_wma_handle wma, u_int8_t *event)
{
	wmi_hb_ind_event_fixed_param *hb_fp;
	tSirLPHBInd *slphb_indication;
	VOS_STATUS vos_status;
	vos_msg_t sme_msg = {0} ;

	hb_fp = (wmi_hb_ind_event_fixed_param *)event;
	if (!hb_fp) {
		WMA_LOGE("Invalid wmi_hb_ind_event_fixed_param buffer");
		return -EINVAL;
	}

	WMA_LOGD("lphb indication received with vdev_id=%d, session=%d, reason=%d",
		hb_fp->vdev_id, hb_fp->session, hb_fp->reason);

        slphb_indication = (tSirLPHBInd *) vos_mem_malloc(sizeof(tSirLPHBInd));

	if (!slphb_indication) {
		WMA_LOGE("Invalid LPHB indication buffer");
		return -EINVAL;
	}

	slphb_indication->sessionIdx = hb_fp->session;
	slphb_indication->protocolType = hb_fp->reason;
	slphb_indication->eventReason= hb_fp->reason;

	sme_msg.type = eWNI_SME_LPHB_IND;
	sme_msg.bodyptr = slphb_indication;
	sme_msg.bodyval = 0;

	vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &sme_msg);
	if ( !VOS_IS_STATUS_SUCCESS(vos_status) )
	{
		WMA_LOGE("Fail to post eWNI_SME_LPHB_IND msg to SME");
		vos_mem_free(slphb_indication);
		return -EINVAL;
	}

	return 0;
}
#endif /* FEATURE_WLAN_LPHB */

#ifdef FEATURE_WLAN_D0WOW
/*
 * Handler to catch D0-WOW disable ACK event.
 */
static int wma_d0_wow_disable_ack_event(void *handle, u_int8_t *event,
					u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_D0_WOW_DISABLE_ACK_EVENTID_param_tlvs *param_buf;
	wmi_d0_wow_disable_ack_event_fixed_param *resp_data;

	param_buf = (WMI_D0_WOW_DISABLE_ACK_EVENTID_param_tlvs *)event;
	if (!param_buf) {
		WMA_LOGE("Invalid D0-WOW disable ACK event buffer!");
		return -EINVAL;
	}

	resp_data = param_buf->fixed_param;
	vos_event_set(&wma->wma_resume_event);
	WMA_LOGD("Received D0-WOW disable ACK");
	return 0;
}
#endif

/**
 * wma_log_supported_evt_handler() - Enable/Disable FW diag/log events
 * @handle: WMA handle
 * @event:  Event received from FW
 * @len:    Length of the event
 *
 * Enables the low frequency events and disables the high frequency
 * events. Bit 17 indicates if the event if low/high frequency.
 * 1 - high frequency, 0 - low frequency
 *
 * Return: 0 on successfully enabling/disabling the events
 */
static int wma_log_supported_evt_handler(void *handle,
			u_int8_t *event,
			u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	uint32_t num_of_diag_events_logs;
	wmi_diag_event_log_config_fixed_param *cmd;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	uint32_t *cmd_args, *evt_args;
	uint32_t buf_len, i;

	WMI_DIAG_EVENT_LOG_SUPPORTED_EVENTID_param_tlvs *param_buf;
	wmi_diag_event_log_supported_event_fixed_params *wmi_event;

	WMA_LOGI("Received WMI_DIAG_EVENT_LOG_SUPPORTED_EVENTID");

	param_buf = (WMI_DIAG_EVENT_LOG_SUPPORTED_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		WMA_LOGE("Invalid log supported event buffer");
		return -EINVAL;
	}
	wmi_event = param_buf->fixed_param;
	num_of_diag_events_logs = wmi_event->num_of_diag_events_logs;
	evt_args = param_buf->diag_events_logs_list;
	if (!evt_args) {
		WMA_LOGE("%s: Event list is empty, num_of_diag_events_logs=%d",
			__func__, num_of_diag_events_logs);
		return -EINVAL;
	}

	WMA_LOGD("%s: num_of_diag_events_logs=%d",
			__func__, num_of_diag_events_logs);

	/* Free any previous allocation */
	if (wma->events_logs_list)
		vos_mem_free(wma->events_logs_list);

	/* Store the event list for run time enable/disable */
	wma->events_logs_list = vos_mem_malloc(num_of_diag_events_logs *
					sizeof(uint32_t));
	if (!wma->events_logs_list) {
		WMA_LOGE("%s: event log list memory allocation failed",
			__func__);
		return -ENOMEM;
	}
	wma->num_of_diag_events_logs = num_of_diag_events_logs;

	/* Prepare the send buffer */
	buf_len = sizeof(*cmd) + WMI_TLV_HDR_SIZE +
		(num_of_diag_events_logs * sizeof(uint32_t));

	buf = wmi_buf_alloc(wma->wmi_handle, buf_len);
	if (!buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		vos_mem_free(wma->events_logs_list);
		wma->events_logs_list = NULL;
		return -ENOMEM;
	}

	cmd = (wmi_diag_event_log_config_fixed_param *) wmi_buf_data(buf);
	buf_ptr = (uint8_t *) cmd;

	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_diag_event_log_config_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_diag_event_log_config_fixed_param));

	cmd->num_of_diag_events_logs = num_of_diag_events_logs;

	buf_ptr += sizeof(wmi_diag_event_log_config_fixed_param);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
		(num_of_diag_events_logs * sizeof(uint32_t)));

	cmd_args = (uint32_t *) (buf_ptr + WMI_TLV_HDR_SIZE);

	/* Populate the events */
	for (i = 0; i < num_of_diag_events_logs; i++) {
		/* Low freq (0) - Enable (1) the event
		 * High freq (1) - Disable (0) the event
		 */
		WMI_DIAG_ID_ENABLED_DISABLED_SET(cmd_args[i],
				!(WMI_DIAG_FREQUENCY_GET(evt_args[i])));
		/* Set the event ID */
		WMI_DIAG_ID_SET(cmd_args[i],
				WMI_DIAG_ID_GET(evt_args[i]));
		/* Set the type */
		WMI_DIAG_TYPE_SET(cmd_args[i],
				WMI_DIAG_TYPE_GET(evt_args[i]));
		/* Storing the event/log list in WMA */
		wma->events_logs_list[i] = evt_args[i];
	}

	if (wmi_unified_cmd_send(wma->wmi_handle, buf, buf_len,
				WMI_DIAG_EVENT_LOG_CONFIG_CMDID)) {
		WMA_LOGE("%s: WMI_DIAG_EVENT_LOG_CONFIG_CMDID failed",
				__func__);
		wmi_buf_free(buf);
		/* Not clearing events_logs_list, though wmi cmd failed.
		 * Host can still have this list
		 */
		return -EINVAL;
	}

	return 0;
}

/**
 * wma_flush_complete_evt_handler() - FW log flush complete event handler
 * @handle: WMI handle
 * @event:  Event recevied from FW
 * @len:    Length of the event
 *
 */
static int wma_flush_complete_evt_handler(void *handle,
		u_int8_t *event,
		u_int32_t len)
{
	VOS_STATUS status;
	tp_wma_handle wma = (tp_wma_handle) handle;

	WMI_DEBUG_MESG_FLUSH_COMPLETE_EVENTID_param_tlvs *param_buf;
	wmi_debug_mesg_flush_complete_fixed_param *wmi_event;
	uint32_t reason_code;

	param_buf = (WMI_DEBUG_MESG_FLUSH_COMPLETE_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		WMA_LOGE("Invalid log flush complete event buffer");
		return VOS_STATUS_E_FAILURE;
	}

	wmi_event = param_buf->fixed_param;
	reason_code = wmi_event->reserved0;

	/*
	 * reason_code = 0; Flush event in response to flush command
	 * reason_code = other value; Asynchronous flush event for fatal events
	 */
	if (!reason_code && (vos_is_log_report_in_progress() == false)) {
		WMA_LOGE("Received WMI flush event without sending CMD");
		return -EINVAL;
	} else if (!reason_code && vos_is_log_report_in_progress() == true) {
		/* Flush event in response to flush command */
		WMA_LOGI("Received WMI flush event in response to flush CMD");
		status = vos_timer_stop(&wma->log_completion_timer);
		if (status != VOS_STATUS_SUCCESS)
			WMA_LOGE("Failed to stop the log completion timeout");
		vos_logging_set_fw_flush_complete();
	} else if (reason_code && vos_is_log_report_in_progress() == false) {
		/* Asynchronous flush event for fatal events */
		WMA_LOGE("Received asynchronous WMI flush event: reason=%d",
			reason_code);
		status = vos_set_log_completion(WLAN_LOG_TYPE_FATAL,
				WLAN_LOG_INDICATOR_FIRMWARE,
				reason_code);
		if (VOS_STATUS_SUCCESS != status) {
			WMA_LOGE("%s: Failed to set log trigger params",
					__func__);
			return VOS_STATUS_E_FAILURE;
		}
		vos_logging_set_fw_flush_complete();
		return status;
	} else {
		/* Asynchronous flush event for fatal event,
		 * but, report in progress already
		 */
		WMA_LOGI("%s: Bug report already in progress - dropping! type:%d, indicator=%d reason_code=%d",
				__func__, WLAN_LOG_TYPE_FATAL,
				WLAN_LOG_INDICATOR_FIRMWARE, reason_code);
		return VOS_STATUS_E_FAILURE;
	}
	return 0;
}

#ifdef FEATURE_WLAN_EXTSCAN
/**
 * wma_extscan_get_eventid_from_tlvtag() - map tlv tag to corresponding event id
 * @tag: WMI TLV tag
 *
 * Return:
 *	0 if TLV tag is invalid
 *	else return corresponding WMI event id
 */
static int wma_extscan_get_eventid_from_tlvtag(uint32_t tag)
{
	uint32_t event_id;

	switch (tag) {
	case WMITLV_TAG_STRUC_wmi_extscan_start_stop_event_fixed_param:
		event_id = WMI_EXTSCAN_START_STOP_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_operation_event_fixed_param:
		event_id = WMI_EXTSCAN_OPERATION_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_table_usage_event_fixed_param:
		event_id = WMI_EXTSCAN_TABLE_USAGE_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_cached_results_event_fixed_param:
		event_id = WMI_EXTSCAN_CACHED_RESULTS_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_wlan_change_results_event_fixed_param:
		event_id = WMI_EXTSCAN_WLAN_CHANGE_RESULTS_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_hotlist_match_event_fixed_param:
		event_id = WMI_EXTSCAN_HOTLIST_MATCH_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_capabilities_event_fixed_param:
		event_id = WMI_EXTSCAN_CAPABILITIES_EVENTID;
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_hotlist_ssid_match_event_fixed_param:
		event_id = WMI_EXTSCAN_HOTLIST_SSID_MATCH_EVENTID;
		break;

	default:
		event_id = 0;
		WMA_LOGE("%s: Unknown tag: %d", __func__, tag);
		break;
	}

	WMA_LOGI("%s: For tag %d WMI event 0x%x", __func__, tag, event_id);
	return event_id;
}

/**
 * wma_extscan_wow_event_callback() - extscan wow event callback
 * @handle: WMA handle
 * @event: event buffer
 * @len: length of @event buffer
 *
 * In wow case, the wow event is followed by the payload of the event
 * which generated the wow event.
 * payload is 4 bytes of length followed by event buffer. the first 4 bytes
 * of event buffer is common tlv header, which is a combination
 * of tag (higher 2 bytes) and length (lower 2 bytes). The tag is used to
 * identify the event which triggered wow event.
 *
 * @Return: none
 */
static void wma_extscan_wow_event_callback(void *handle, void *event,
					  uint32_t len)
{
	uint32_t id;
	int tlv_ok_status = 0;
	void *wmi_cmd_struct_ptr = NULL;
	uint32_t tag = WMITLV_GET_TLVTAG(WMITLV_GET_HDR(event));

	id = wma_extscan_get_eventid_from_tlvtag(tag);
	if (!id) {
		WMA_LOGE("%s: Invalid  Tag: %d", __func__, tag);
		return;
	}

	tlv_ok_status = wmitlv_check_and_pad_event_tlvs(
				handle, event, len, id,
				&wmi_cmd_struct_ptr);
	if (tlv_ok_status != 0) {
		WMA_LOGE("%s: Invalid Tag: %d could not check and pad tlvs",
			 __func__, tag);
		return;
	}

	switch (tag) {
	case WMITLV_TAG_STRUC_wmi_extscan_start_stop_event_fixed_param:
		wma_extscan_start_stop_event_handler(handle,
				wmi_cmd_struct_ptr, len);
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_operation_event_fixed_param:
		wma_extscan_operations_event_handler(handle,
				wmi_cmd_struct_ptr, len);
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_table_usage_event_fixed_param:
		wma_extscan_table_usage_event_handler(handle,
				wmi_cmd_struct_ptr, len);
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_cached_results_event_fixed_param:
		wma_extscan_cached_results_event_handler(handle,
				wmi_cmd_struct_ptr, len);
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_wlan_change_results_event_fixed_param:
		wma_extscan_change_results_event_handler(handle,
				wmi_cmd_struct_ptr, len);
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_hotlist_match_event_fixed_param:
		wma_extscan_hotlist_match_event_handler(handle,
				wmi_cmd_struct_ptr, len);
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_capabilities_event_fixed_param:
		wma_extscan_capabilities_event_handler(handle,
				wmi_cmd_struct_ptr, len);
		break;

	case WMITLV_TAG_STRUC_wmi_extscan_hotlist_ssid_match_event_fixed_param:
		wma_extscan_hotlist_ssid_match_event_handler(handle,
				wmi_cmd_struct_ptr, len);
		break;

	default:
		WMA_LOGE("%s: Unknown tag: %d", __func__, tag);
		break;
	}
	wmitlv_free_allocated_event_tlvs(id, &wmi_cmd_struct_ptr);

	return;
}
#endif

/**
 * wma_wow_wake_up_stats_display() - display wow wake up stats
 * @wma: Pointer to wma handle
 *
 * Return: none
 */
static void wma_wow_wake_up_stats_display(tp_wma_handle wma)
{
	WMA_LOGA("uc %d bc %d v4_mc %d v6_mc %d ra %d ns %d na %d pno_match %d pno_complete %d gscan %d low_rssi %d rssi_breach %d",
		wma->wow_ucast_wake_up_count,
		wma->wow_bcast_wake_up_count,
		wma->wow_ipv4_mcast_wake_up_count,
		wma->wow_ipv6_mcast_wake_up_count,
		wma->wow_ipv6_mcast_ra_stats,
		wma->wow_ipv6_mcast_ns_stats,
		wma->wow_ipv6_mcast_na_stats,
		wma->wow_pno_match_wake_up_count,
		wma->wow_pno_complete_wake_up_count,
		wma->wow_gscan_wake_up_count,
		wma->wow_low_rssi_wake_up_count,
		wma->wow_rssi_breach_wake_up_count);

	return;
}

/**
 * wma_wow_ipv6_mcast_stats() - ipv6 mcast wake up stats
 * @wma: Pointer to wma handle
 * @data: Pointer to pattern match data
 *
 * Return: none
 */
static void wma_wow_ipv6_mcast_stats(tp_wma_handle wma, uint8_t *data)
{
	static const uint8_t ipv6_mcast[] = {0x86, 0xDD};

	if (!memcmp(ipv6_mcast, (data + WMA_ETHER_TYPE_OFFSET),
						sizeof(ipv6_mcast))) {
		if (WMA_ICMP_V6_HEADER_TYPE ==
			*(data + WMA_ICMP_V6_HEADER_OFFSET)) {
			if (WMA_ICMP_V6_RA_TYPE ==
				*(data + WMA_ICMP_V6_TYPE_OFFSET))
				wma->wow_ipv6_mcast_ra_stats++;
			else if (WMA_ICMP_V6_NS_TYPE ==
				*(data + WMA_ICMP_V6_TYPE_OFFSET))
				wma->wow_ipv6_mcast_ns_stats++;
			else if (WMA_ICMP_V6_NA_TYPE ==
				*(data + WMA_ICMP_V6_TYPE_OFFSET))
				wma->wow_ipv6_mcast_na_stats++;
			else
				WMA_LOGA("ICMP V6 type : 0x%x",
					*(data + WMA_ICMP_V6_TYPE_OFFSET));
		} else {
			WMA_LOGA("ICMP_V6 header 0x%x",
				*(data + WMA_ICMP_V6_HEADER_OFFSET));
		}
	} else {
		WMA_LOGA("Ethertype x%x:0x%x",
			*(data + WMA_ETHER_TYPE_OFFSET),
			*(data + WMA_ETHER_TYPE_OFFSET + 1));
	}

	return;
}

/**
 * wma_wow_wake_up_stats() - maintain wow pattern match wake up stats
 * @wma: Pointer to wma handle
 * @data: Pointer to pattern match data
 * @len: Pattern match data length
 * @event: Wake up event
 *
 * Return: none
 */
static void wma_wow_wake_up_stats(tp_wma_handle wma, uint8_t *data,
	int32_t len, WOW_WAKE_REASON_TYPE event)
{
	/* Do not increment debug stats if resume is for runtime pm */
	if (wmi_get_runtime_pm_inprogress(wma->wmi_handle))
		return;

	switch(event) {

	case WOW_REASON_PATTERN_MATCH_FOUND:
		if (WMA_BCAST_MAC_ADDR == *data) {
			wma->wow_bcast_wake_up_count++;
		} else if (WMA_MCAST_IPV4_MAC_ADDR == *data) {
			wma->wow_ipv4_mcast_wake_up_count++;
		} else if (WMA_MCAST_IPV6_MAC_ADDR == *data) {
			wma->wow_ipv6_mcast_wake_up_count++;
			if (len > WMA_ICMP_V6_TYPE_OFFSET)
				wma_wow_ipv6_mcast_stats(wma, data);
			else
				WMA_LOGA("ICMP_V6 data len %d", len);
		} else {
			wma->wow_ucast_wake_up_count++;
		}
		break;

	case WOW_REASON_RA_MATCH:
		wma->wow_ipv6_mcast_ra_stats++;
		break;

	case WOW_REASON_NLOD:
		wma->wow_pno_match_wake_up_count++;
		break;

	case WOW_REASON_NLO_SCAN_COMPLETE:
		wma->wow_pno_complete_wake_up_count++;
		break;

	case WOW_REASON_LOW_RSSI:
		wma->wow_low_rssi_wake_up_count++;
		break;

	case WOW_REASON_EXTSCAN:
		wma->wow_gscan_wake_up_count++;
		break;

	case WOW_REASON_RSSI_BREACH_EVENT:
		wma->wow_rssi_breach_wake_up_count++;
		break;

	default:
		WMA_LOGE("Unknown wake up reason");
		break;
	}

	wma_wow_wake_up_stats_display(wma);
	return;
}

/*
 * Handler to catch wow wakeup host event. This event will have
 * reason why the firmware has woken the host.
 */
static int wma_wow_wakeup_host_event(void *handle, u_int8_t *event,
				     u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_WOW_WAKEUP_HOST_EVENTID_param_tlvs *param_buf;
	WOW_EVENT_INFO_fixed_param *wake_info;
#ifdef FEATURE_WLAN_SCAN_PNO
	struct wma_txrx_node *node;
#endif
	u_int32_t wake_lock_duration = 0;
	u_int32_t wow_buf_pkt_len = 0;

	param_buf = (WMI_WOW_WAKEUP_HOST_EVENTID_param_tlvs *) event;
	if (!param_buf) {
		WMA_LOGE("Invalid wow wakeup host event buf");
		return -EINVAL;
	}

	wake_info = param_buf->fixed_param;

	WMA_LOGA("WOW wakeup host event received (reason: %s(%d)) for vdev %d",
		 wma_wow_wake_reason_str(wake_info->wake_reason, wma),
		 wake_info->wake_reason,
		 wake_info->vdev_id);

	vos_event_set(&wma->wma_resume_event);

	switch (wake_info->wake_reason) {
	case WOW_REASON_AUTH_REQ_RECV:
		wake_lock_duration = WMA_AUTH_REQ_RECV_WAKE_LOCK_TIMEOUT;
		break;

	case WOW_REASON_ASSOC_REQ_RECV:
		wake_lock_duration = WMA_ASSOC_REQ_RECV_WAKE_LOCK_DURATION;
		break;

	case WOW_REASON_DEAUTH_RECVD:
		wake_lock_duration = WMA_DEAUTH_RECV_WAKE_LOCK_DURATION;
		break;

	case WOW_REASON_DISASSOC_RECVD:
		wake_lock_duration = WMA_DISASSOC_RECV_WAKE_LOCK_DURATION;
		break;

	case WOW_REASON_AP_ASSOC_LOST:
		wake_lock_duration = WMA_BMISS_EVENT_WAKE_LOCK_DURATION;
		WMA_LOGA("Beacon miss indication on vdev %x",
			 wake_info->vdev_id);
		wma_beacon_miss_handler(wma, wake_info->vdev_id);
		break;
#ifdef FEATURE_WLAN_RA_FILTERING
	case WOW_REASON_RA_MATCH:
		wma_wow_wake_up_stats(wma, NULL, 0, WOW_REASON_RA_MATCH);
		break;
#endif
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	case WOW_REASON_HOST_AUTO_SHUTDOWN:
		wake_lock_duration = WMA_AUTO_SHUTDOWN_WAKE_LOCK_DURATION;
		WMA_LOGA("Received WOW Auto Shutdown trigger in suspend");
		if (wma_post_auto_shutdown_msg())
			return -EINVAL;
		break;
#endif
#ifdef FEATURE_WLAN_SCAN_PNO
	case WOW_REASON_NLOD:
		wma_wow_wake_up_stats(wma, NULL, 0, WOW_REASON_NLOD);
		node = &wma->interfaces[wake_info->vdev_id];
		if (node) {
			WMA_LOGD("NLO match happened");
			node->nlo_match_evt_received = TRUE;

			vos_wake_lock_timeout_acquire(&wma->pno_wake_lock,
					      WMA_PNO_MATCH_WAKE_LOCK_TIMEOUT,
					      WIFI_POWER_EVENT_WAKELOCK_PNO);
		}
		break;

	case WOW_REASON_NLO_SCAN_COMPLETE:
		{
			WMI_NLO_SCAN_COMPLETE_EVENTID_param_tlvs param;

			wma_wow_wake_up_stats(wma, NULL, 0,
					WOW_REASON_NLO_SCAN_COMPLETE);
			WMA_LOGD("Host woken up due to pno scan complete reason");
			/* First 4-bytes of wow_packet_buffer is the length */
			if (param_buf->wow_packet_buffer) {
				param.fixed_param = (wmi_nlo_event *)
					(param_buf->wow_packet_buffer + 4);
				wma_nlo_scan_cmp_evt_handler(handle,
						(u_int8_t *)&param,
						sizeof(param));
			} else
				WMA_LOGD("No wow_packet_buffer present");
		}
		break;
#endif

	case WOW_REASON_CSA_EVENT:
		{
			WMI_CSA_HANDLING_EVENTID_param_tlvs param;
			WMA_LOGD("Host woken up because of CSA IE");
			param.fixed_param = (wmi_csa_event_fixed_param *)
					    (((u_int8_t *) wake_info)
					    + sizeof(WOW_EVENT_INFO_fixed_param)
					    + WOW_CSA_EVENT_OFFSET);
			wma_csa_offload_handler(handle, (u_int8_t *)&param,
						sizeof(param));
		}
		break;

#ifdef FEATURE_WLAN_LPHB
	case WOW_REASON_WLAN_HB:
		wma_lphb_handler(wma, (u_int8_t *)param_buf->hb_indevt);
		break;
#endif

	case WOW_REASON_HTT_EVENT:
		break;
	case WOW_REASON_PATTERN_MATCH_FOUND:
		WMA_LOGD("Wake up for Rx packet, dump starting from ethernet hdr");
		if (param_buf->wow_packet_buffer) {
		    /* First 4-bytes of wow_packet_buffer is the length */
		    vos_mem_copy((u_int8_t *) &wow_buf_pkt_len,
			param_buf->wow_packet_buffer, 4);
		    wma_wow_wake_up_stats(wma,
			param_buf->wow_packet_buffer + 4, wow_buf_pkt_len,
			WOW_REASON_PATTERN_MATCH_FOUND);
		    vos_trace_hex_dump(VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_DEBUG,
			param_buf->wow_packet_buffer + 4,
			wow_buf_pkt_len);
		} else {
			WMA_LOGE("No wow packet buffer present");
		}
		break;

	case WOW_REASON_LOW_RSSI:
	    {
		/* WOW_REASON_LOW_RSSI is used for all roaming events.
		 * WMI_ROAM_REASON_BETTER_AP, WMI_ROAM_REASON_BMISS,
		 * WMI_ROAM_REASON_SUITABLE_AP will be handled by
		 * wma_roam_event_callback().
		 */
		WMI_ROAM_EVENTID_param_tlvs param;
		wma_wow_wake_up_stats(wma, NULL, 0, WOW_REASON_LOW_RSSI);
		if (param_buf->wow_packet_buffer) {
		    /* Roam event is embedded in wow_packet_buffer */
		    WMA_LOGD("Host woken up because of roam event");
		    vos_mem_copy((u_int8_t *) &wow_buf_pkt_len,
				param_buf->wow_packet_buffer, 4);
		    WMA_LOGD("wow_packet_buffer dump");
				vos_trace_hex_dump(VOS_MODULE_ID_WDA,
				VOS_TRACE_LEVEL_DEBUG,
				param_buf->wow_packet_buffer, wow_buf_pkt_len);
		    if (wow_buf_pkt_len >= sizeof(param)) {
			param.fixed_param = (wmi_roam_event_fixed_param *)
					(param_buf->wow_packet_buffer +4);
			wma_roam_event_callback(handle, (u_int8_t *)&param,
							sizeof(param));
		    } else {
			WMA_LOGE("Wrong length for roam event = %d bytes",
					wow_buf_pkt_len);
		    }
		} else {
		    /* No wow_packet_buffer means a better AP beacon
		     * will follow in a later event.
		     */
		    WMA_LOGD("Host woken up because of better AP beacon");
		}
		break;
	    }
#ifdef FEATURE_WLAN_EXTSCAN
	case WOW_REASON_EXTSCAN:
		WMA_LOGD("Host woken up because of extscan reason");
		wma_wow_wake_up_stats(wma, NULL, 0, WOW_REASON_EXTSCAN);
		if (param_buf->wow_packet_buffer) {
			wow_buf_pkt_len =
				*(uint32_t *)param_buf->wow_packet_buffer;
			WMA_LOGD("wow_packet_buffer dump");
			vos_trace_hex_dump(VOS_MODULE_ID_WDA,
				VOS_TRACE_LEVEL_DEBUG,
				param_buf->wow_packet_buffer,
				wow_buf_pkt_len);
			wma_extscan_wow_event_callback(handle,
				(u_int8_t *)(param_buf->wow_packet_buffer + 4),
				wow_buf_pkt_len);
		} else
			WMA_LOGE("wow_packet_buffer is empty");
		break;
#endif
	case WOW_REASON_RSSI_BREACH_EVENT:
		{
			WMI_RSSI_BREACH_EVENTID_param_tlvs param;

			wma_wow_wake_up_stats(wma, NULL, 0,
				WOW_REASON_RSSI_BREACH_EVENT);
			WMA_LOGD("Host woken up because of rssi breach reason");
			/* rssi breach event is embedded in wow_packet_buffer */
			if (param_buf->wow_packet_buffer) {
				vos_mem_copy((u_int8_t *) &wow_buf_pkt_len,
					param_buf->wow_packet_buffer, 4);
				if (wow_buf_pkt_len >= sizeof(param)) {
					param.fixed_param =
					(wmi_rssi_breach_event_fixed_param *)
					(param_buf->wow_packet_buffer + 4);
					wma_rssi_breached_event_handler(handle,
							(u_int8_t *)&param,
							sizeof(param));
				} else {
					WMA_LOGE("%s: Wrong length: %d bytes",
						__func__, wow_buf_pkt_len);
				}
			} else
			    WMA_LOGD("No wow_packet_buffer present");
		}
		break;

	default:
		break;
	}

	if (wake_lock_duration) {
		vos_wake_lock_timeout_acquire(&wma->wow_wake_lock,
					      wake_lock_duration,
					      WIFI_POWER_EVENT_WAKELOCK_WOW);
		WMA_LOGA("Holding %d msec wake_lock", wake_lock_duration);
	}

	return 0;
}

static inline void wma_set_wow_bus_suspend(tp_wma_handle wma, int val) {

	adf_os_atomic_set(&wma->is_wow_bus_suspended, val);
}

static inline int wma_get_wow_bus_suspend(tp_wma_handle wma) {

	return adf_os_atomic_read(&wma->is_wow_bus_suspended);
}

/* Configures wow wakeup events. */
static VOS_STATUS wma_add_wow_wakeup_event(tp_wma_handle wma,
					   WOW_WAKE_EVENT_TYPE event,
					   v_BOOL_t enable)
{
	WMI_WOW_ADD_DEL_EVT_CMD_fixed_param *cmd;
	u_int16_t len;
	wmi_buf_t buf;
	int ret;

	len = sizeof(WMI_WOW_ADD_DEL_EVT_CMD_fixed_param);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	cmd = (WMI_WOW_ADD_DEL_EVT_CMD_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_WMI_WOW_ADD_DEL_EVT_CMD_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				WMI_WOW_ADD_DEL_EVT_CMD_fixed_param));
	cmd->vdev_id = 0;
	cmd->is_add = enable;
	cmd->event_bitmap = (1 << event);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID);
	if (ret) {
		WMA_LOGE("Failed to config wow wakeup event");
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("Wakeup pattern 0x%x %s in fw", event,
		 enable ? "enabled":"disabled");

	return VOS_STATUS_SUCCESS;
}

/* Sends WOW patterns to FW. */
static VOS_STATUS wma_send_wow_patterns_to_fw(tp_wma_handle wma,
			u_int8_t vdev_id, u_int8_t ptrn_id,
			u_int8_t *ptrn, u_int8_t ptrn_len,
			u_int8_t ptrn_offset, u_int8_t *mask,
			u_int8_t mask_len)

{
	WMI_WOW_ADD_PATTERN_CMD_fixed_param *cmd;
	WOW_BITMAP_PATTERN_T *bitmap_pattern;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
#ifdef WMA_DUMP_WOW_PTRN
	u_int8_t pos;
	u_int8_t *tmp;
#endif
	int32_t len;
	int ret;

	len = sizeof(WMI_WOW_ADD_PATTERN_CMD_fixed_param) +
		     WMI_TLV_HDR_SIZE +
		     1 * sizeof(WOW_BITMAP_PATTERN_T) +
		     WMI_TLV_HDR_SIZE +
		     0 * sizeof(WOW_IPV4_SYNC_PATTERN_T) +
		     WMI_TLV_HDR_SIZE +
		     0 * sizeof(WOW_IPV6_SYNC_PATTERN_T) +
		     WMI_TLV_HDR_SIZE +
		     0 * sizeof(WOW_MAGIC_PATTERN_CMD) +
		     WMI_TLV_HDR_SIZE +
		     0 * sizeof(A_UINT32) +
		     WMI_TLV_HDR_SIZE +
		     1 * sizeof(A_UINT32);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (WMI_WOW_ADD_PATTERN_CMD_fixed_param *)wmi_buf_data(buf);
	buf_ptr = (u_int8_t *)cmd;

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_WMI_WOW_ADD_PATTERN_CMD_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				WMI_WOW_ADD_PATTERN_CMD_fixed_param));
	cmd->vdev_id = vdev_id;
	cmd->pattern_id = ptrn_id;
	cmd->pattern_type = WOW_BITMAP_PATTERN;
	buf_ptr += sizeof(WMI_WOW_ADD_PATTERN_CMD_fixed_param);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(WOW_BITMAP_PATTERN_T));
	buf_ptr += WMI_TLV_HDR_SIZE;
	bitmap_pattern = (WOW_BITMAP_PATTERN_T *)buf_ptr;

	WMITLV_SET_HDR(&bitmap_pattern->tlv_header,
		       WMITLV_TAG_STRUC_WOW_BITMAP_PATTERN_T,
		       WMITLV_GET_STRUCT_TLVLEN(WOW_BITMAP_PATTERN_T));

	vos_mem_copy(&bitmap_pattern->patternbuf[0], ptrn, ptrn_len);
	vos_mem_copy(&bitmap_pattern->bitmaskbuf[0], mask, mask_len);

	bitmap_pattern->pattern_offset = ptrn_offset;
	bitmap_pattern->pattern_len = ptrn_len;

	if(bitmap_pattern->pattern_len > WOW_DEFAULT_BITMAP_PATTERN_SIZE)
		bitmap_pattern->pattern_len = WOW_DEFAULT_BITMAP_PATTERN_SIZE;

	if(bitmap_pattern->pattern_len > WOW_DEFAULT_BITMASK_SIZE)
		bitmap_pattern->pattern_len = WOW_DEFAULT_BITMASK_SIZE;

	bitmap_pattern->bitmask_len = bitmap_pattern->pattern_len;
	bitmap_pattern->pattern_id = ptrn_id;

	WMA_LOGD("vdev id : %d, ptrn id: %d, ptrn len: %d, ptrn offset: %d",
		 cmd->vdev_id, cmd->pattern_id, bitmap_pattern->pattern_len,
		 bitmap_pattern->pattern_offset);

#ifdef WMA_DUMP_WOW_PTRN
	printk("Pattern : ");
	tmp = (u_int8_t *) &bitmap_pattern->patternbuf[0];
	for (pos = 0; pos < bitmap_pattern->pattern_len; pos++)
		printk("%02X ", tmp[pos]);

	printk("\nMask    : ");
	tmp = (u_int8_t *) &bitmap_pattern->bitmaskbuf[0];
	for (pos = 0; pos < bitmap_pattern->pattern_len; pos++)
		printk("%02X ", tmp[pos]);
#endif

	buf_ptr += sizeof(WOW_BITMAP_PATTERN_T);

	/* Fill TLV for WMITLV_TAG_STRUC_WOW_IPV4_SYNC_PATTERN_T but no data. */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	buf_ptr += WMI_TLV_HDR_SIZE;

	/* Fill TLV for WMITLV_TAG_STRUC_WOW_IPV6_SYNC_PATTERN_T but no data. */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	buf_ptr += WMI_TLV_HDR_SIZE;

	/* Fill TLV for WMITLV_TAG_STRUC_WOW_MAGIC_PATTERN_CMD but no data. */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	buf_ptr += WMI_TLV_HDR_SIZE;

	/* Fill TLV for pattern_info_timeout but no data. */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32, 0);
	buf_ptr += WMI_TLV_HDR_SIZE;

	/* Fill TLV for ra_ratelimit_interval with dummy data as this fix elem*/
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32, 1 * sizeof(A_UINT32));
	buf_ptr += WMI_TLV_HDR_SIZE;
	*(A_UINT32 *)buf_ptr = 0;

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_WOW_ADD_WAKE_PATTERN_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send wow ptrn to fw", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;
}

/* Sends delete pattern request to FW for given pattern ID on particular vdev */
static VOS_STATUS wma_del_wow_pattern_in_fw(tp_wma_handle wma,
					    u_int8_t ptrn_id)
{
	WMI_WOW_DEL_PATTERN_CMD_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len;
	int ret;

	len = sizeof(WMI_WOW_DEL_PATTERN_CMD_fixed_param);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (WMI_WOW_DEL_PATTERN_CMD_fixed_param *) wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_WMI_WOW_DEL_PATTERN_CMD_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				WMI_WOW_DEL_PATTERN_CMD_fixed_param));
	cmd->vdev_id = 0;
	cmd->pattern_id = ptrn_id;
	cmd->pattern_type = WOW_BITMAP_PATTERN;

	WMA_LOGD("Deleting pattern id: %d in fw", cmd->pattern_id);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_WOW_DEL_WAKE_PATTERN_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to delete wow ptrn from fw", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_D0WOW
void wma_set_d0wow_flag(tp_wma_handle wma_handle, A_BOOL flag)
{
	atomic_set(&wma_handle->in_d0wow, flag);
}

A_BOOL wma_read_d0wow_flag(tp_wma_handle wma_handle)
{
	return atomic_read(&wma_handle->in_d0wow);
}

/* Enable D0-WOW in firmware. */
VOS_STATUS wma_enable_d0wow_in_fw(tp_wma_handle wma)
{
	wmi_d0_wow_enable_disable_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len;
	int host_credits;
	int wmi_pending_cmds;
	int ret = 0;
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

	len = sizeof(wmi_d0_wow_enable_disable_cmd_fixed_param);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed to allocate WMI buffer!", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (wmi_d0_wow_enable_disable_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_d0_wow_enable_disable_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_d0_wow_enable_disable_cmd_fixed_param));
	cmd->enable = 1;

	vos_event_reset(&wma->target_suspend);
	wma->wow_nack = 0;

	host_credits = wmi_get_host_credits(wma->wmi_handle);
	wmi_pending_cmds = wmi_get_pending_cmds(wma->wmi_handle);
	if (host_credits < WMI_WOW_REQUIRED_CREDITS) {
		WMA_LOGE("%s: Not enough credits to post "
				"WMI_D0_WOW_ENABLE_DISABLE_CMDID, try anyway! "
				"Credits: %d, pending_cmds: %d", __func__,
				host_credits, wmi_pending_cmds);
	}

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
		WMI_D0_WOW_ENABLE_DISABLE_CMDID);
	if (ret) {
		WMA_LOGE("Failed to enable D0-WOW in FW!");
		goto error;
	}

	vos_status = vos_wait_single_event(&wma->target_suspend,
		WMA_TGT_SUSPEND_COMPLETE_TIMEOUT);
	if (VOS_STATUS_SUCCESS != vos_status) {
		WMA_LOGE("Failed to receive D0-WoW enable HTC ACK from FW! "
			"Credits: %d, pending_cmds: %d",
			wmi_get_host_credits(wma->wmi_handle),
			wmi_get_pending_cmds(wma->wmi_handle));

		if (vos_is_logp_in_progress(VOS_MODULE_ID_WDA, NULL)) {
			VOS_ASSERT(0);
			return VOS_STATUS_E_FAILURE;
		} else {
			VOS_BUG(0);
			return VOS_STATUS_E_FAILURE;
		}
	}

	if (wma->wow_nack) {
		WMA_LOGE("FW not ready for D0WOW.");
		return VOS_STATUS_E_AGAIN;
	}

	host_credits = wmi_get_host_credits(wma->wmi_handle);
	wmi_pending_cmds = wmi_get_pending_cmds(wma->wmi_handle);
	if (host_credits < WMI_WOW_REQUIRED_CREDITS) {
		WMA_LOGE("%s: No credits after HTC ACK: %d, pending_cmds: %d, "
			"cannot resume back!", __func__, host_credits,
			wmi_pending_cmds);
		HTC_dump_counter_info(wma->htc_handle);
	}

	wma->wow.wow_enable_cmd_sent = TRUE;
	wmi_set_d0wow_flag(wma->wmi_handle, TRUE);
	wma_set_d0wow_flag(wma, TRUE);
	WMA_LOGD("D0-WOW is enabled successfully in FW.");
	return vos_status;

error:
	wmi_buf_free(buf);
	return VOS_STATUS_E_FAILURE;
}
#endif /* FEATURE_WLAN_D0WOW */

/* Enables WOW in firmware. */
int wma_enable_wow_in_fw(WMA_HANDLE handle, int runtime_pm)
{
	tp_wma_handle wma = handle;
	wmi_wow_enable_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len;
	int ret;
	struct ol_softc *scn;
	int host_credits;
	int wmi_pending_cmds;
#ifdef CONFIG_CNSS
	tpAniSirGlobal pMac = (tpAniSirGlobal)vos_get_context(VOS_MODULE_ID_PE,
				wma->vos_context);

	if (NULL == pMac) {
		WMA_LOGE("%s: Unable to get PE context", __func__);
		return VOS_STATUS_E_FAILURE;
	}
#endif

#ifdef FEATURE_WLAN_D0WOW
	if (wma->ap_client_cnt > 0) {
		WMA_LOGD("Entering D0-WOW since client count is %d.",
			wma->ap_client_cnt);
		return wma_enable_d0wow_in_fw(wma);
}
#endif

	len = sizeof(wmi_wow_enable_cmd_fixed_param);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (wmi_wow_enable_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_wow_enable_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
					wmi_wow_enable_cmd_fixed_param));
	cmd->enable = TRUE;

	vos_event_reset(&wma->target_suspend);
	vos_event_reset(&wma->wow_tx_complete);
	wma->wow_nack = 0;

	host_credits = wmi_get_host_credits(wma->wmi_handle);
	wmi_pending_cmds = wmi_get_pending_cmds(wma->wmi_handle);

	WMA_LOGD("Credits:%d; Pending_Cmds: %d",
		host_credits, wmi_pending_cmds);

	if (!runtime_pm && host_credits < WMI_WOW_REQUIRED_CREDITS) {
		WMA_LOGE("%s: Not enough credits but trying to send WoW enable"
			" anyway! Credits:%d, pending_cmds:%d\n", __func__,
				host_credits, wmi_pending_cmds);
	}

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_WOW_ENABLE_CMDID);
	if (ret) {
		WMA_LOGE("Failed to enable wow in fw");
		goto error;
	}

	wmi_set_target_suspend(wma->wmi_handle, TRUE);

	if (vos_wait_single_event(&wma->target_suspend,
				  WMA_TGT_SUSPEND_COMPLETE_TIMEOUT)
				  != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to receive WoW Enable Ack from FW");
		WMA_LOGE("Credits:%d; Pending_Cmds: %d",
			wmi_get_host_credits(wma->wmi_handle),
			wmi_get_pending_cmds(wma->wmi_handle));
		if (!vos_is_logp_in_progress(VOS_MODULE_ID_VOSS, NULL)) {
#ifdef CONFIG_CNSS
			if (pMac->sme.enableSelfRecovery) {
				vos_trigger_recovery();
			} else {
				VOS_BUG(0);
			}
#else
			VOS_BUG(0);
#endif
		} else {
			WMA_LOGE("%s: LOGP is in progress, ignore!", __func__);
		}
		wmi_set_target_suspend(wma->wmi_handle, FALSE);
		return VOS_STATUS_E_FAILURE;
	}

	if (wma->wow_nack) {
		WMA_LOGE("FW not ready to WOW");
		wmi_set_target_suspend(wma->wmi_handle, FALSE);
		return VOS_STATUS_E_AGAIN;
	}

	host_credits = wmi_get_host_credits(wma->wmi_handle);
	wmi_pending_cmds = wmi_get_pending_cmds(wma->wmi_handle);

	if (host_credits < WMI_WOW_REQUIRED_CREDITS) {
		WMA_LOGE("%s: No Credits after HTC ACK:%d, pending_cmds:%d, "
			"cannot resume back", __func__, host_credits, wmi_pending_cmds);
		HTC_dump_counter_info(wma->htc_handle);
		if (!vos_is_logp_in_progress(VOS_MODULE_ID_HIF, NULL))
			VOS_BUG(0);
		else
			WMA_LOGE("%s: SSR in progress, ignore no credit issue", __func__);
	}


	WMA_LOGD("WOW enabled successfully in fw: credits:%d"
		"pending_cmds: %d", host_credits, wmi_pending_cmds);

	scn = vos_get_context(VOS_MODULE_ID_HIF, wma->vos_context);

	if (scn == NULL) {
		WMA_LOGE("%s: Failed to get HIF context", __func__);
		VOS_ASSERT(0);
		return VOS_STATUS_E_FAULT;
	}

	HTCCancelDeferredTargetSleep(scn);

	wma->wow.wow_enable_cmd_sent = TRUE;

	return VOS_STATUS_SUCCESS;

error:
	wmi_buf_free(buf);
	return VOS_STATUS_E_FAILURE;
}

/* Sends user configured WOW patterns to the firmware. */
static VOS_STATUS wma_wow_usr(tp_wma_handle wma, u_int8_t vdev_id,
			      u_int8_t *enable_ptrn_match)
{
	struct wma_wow_ptrn_cache *cache;
	VOS_STATUS ret = VOS_STATUS_SUCCESS;
	u_int8_t new_mask[SIR_WOWL_BCAST_PATTERN_MAX_SIZE];
	u_int8_t bit_to_check, ptrn_id, pos;

	WMA_LOGD("Configuring user wow patterns for vdev %d", vdev_id);

	for (ptrn_id = 0; ptrn_id < WOW_MAX_BITMAP_FILTERS; ptrn_id++) {
		cache = wma->wow.cache[ptrn_id];
		if (!cache)
			continue;

		if (cache->vdev_id != vdev_id)
			continue;
		/*
		 * Convert received pattern mask value from bit representaion
		 * to byte representation.
		 *
		 * For example, received value from umac,
		 *
		 *      Mask value    : A1 (equivalent binary is "1010 0001")
		 *      Pattern value : 12:00:13:00:00:00:00:44
		 *
		 * The value which goes to FW after the conversion from this
		 * function (1 in mask value will become FF and 0 will
		 * become 00),
		 *
		 *      Mask value    : FF:00:FF:00:0:00:00:FF
		 *      Pattern value : 12:00:13:00:00:00:00:44
		 */
		vos_mem_zero(new_mask, sizeof(new_mask));
		for (pos = 0; pos < cache->ptrn_len; pos++) {
		     bit_to_check = (WMA_NUM_BITS_IN_BYTE - 1) -
					(pos % WMA_NUM_BITS_IN_BYTE);
		     bit_to_check = 0x1 << bit_to_check;
		     if (cache->mask[pos / WMA_NUM_BITS_IN_BYTE] & bit_to_check)
				new_mask[pos] = WMA_WOW_PTRN_MASK_VALID;
		}

		ret = wma_send_wow_patterns_to_fw(wma, vdev_id, ptrn_id,
				cache->ptrn, cache->ptrn_len,
				cache->ptrn_offset, new_mask,
				cache->ptrn_len);
		if (ret != VOS_STATUS_SUCCESS) {
			WMA_LOGE("Failed to submit wow pattern to fw (ptrn_id %d)",
				 ptrn_id);
			break;
		}
	}

	*enable_ptrn_match = 1 << vdev_id;
	return ret;
}

/* Configures default WOW pattern for the given vdev_id which is in AP mode. */
static VOS_STATUS wma_wow_ap(tp_wma_handle wma, u_int8_t vdev_id,
			     u_int8_t *enable_ptrn_match)
{
	u_int8_t arp_ptrn[] = { 0x08, 0x06 };
	u_int8_t arp_mask[] = { 0xff, 0xff };
	u_int8_t arp_offset = 20;
	VOS_STATUS ret;

	/* Setup all ARP pkt pattern. This is dummy pattern hence the lenght
	is zero */
	ret = wma_send_wow_patterns_to_fw(wma, vdev_id,
			wma->wow.free_ptrn_id[wma->wow.used_free_ptrn_id++],
			arp_ptrn, 0, arp_offset,
			arp_mask, 0);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to add WOW ARP pattern");
		return ret;
	}

	*enable_ptrn_match = 1 << vdev_id;
	return ret;
}

#ifdef FEATURE_WLAN_RA_FILTERING
static VOS_STATUS wma_wow_sta_ra_filter(tp_wma_handle wma, u_int8_t vdev_id)
{

	WMI_WOW_ADD_PATTERN_CMD_fixed_param *cmd;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	int32_t len;
	int ret;

	len = sizeof(WMI_WOW_ADD_PATTERN_CMD_fixed_param) +
		     WMI_TLV_HDR_SIZE +
		     0 * sizeof(WOW_BITMAP_PATTERN_T) +
		     WMI_TLV_HDR_SIZE +
		     0 * sizeof(WOW_IPV4_SYNC_PATTERN_T) +
		     WMI_TLV_HDR_SIZE +
		     0 * sizeof(WOW_IPV6_SYNC_PATTERN_T) +
		     WMI_TLV_HDR_SIZE +
		     0 * sizeof(WOW_MAGIC_PATTERN_CMD) +
		     WMI_TLV_HDR_SIZE +
		     0 * sizeof(A_UINT32) +
		     WMI_TLV_HDR_SIZE +
		     1 * sizeof(A_UINT32);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (WMI_WOW_ADD_PATTERN_CMD_fixed_param *)wmi_buf_data(buf);
	buf_ptr = (u_int8_t *)cmd;

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_WMI_WOW_ADD_PATTERN_CMD_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				WMI_WOW_ADD_PATTERN_CMD_fixed_param));
	cmd->vdev_id = vdev_id;
	cmd->pattern_id = 0;
	cmd->pattern_type = WOW_IPV6_RA_PATTERN;
	buf_ptr += sizeof(WMI_WOW_ADD_PATTERN_CMD_fixed_param);

	/* Fill TLV for WMITLV_TAG_STRUC_WOW_BITMAP_PATTERN_T but no data. */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	buf_ptr += WMI_TLV_HDR_SIZE;

	/* Fill TLV for WMITLV_TAG_STRUC_WOW_IPV4_SYNC_PATTERN_T but no data. */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	buf_ptr += WMI_TLV_HDR_SIZE;

	/* Fill TLV for WMITLV_TAG_STRUC_WOW_IPV6_SYNC_PATTERN_T but no data. */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	buf_ptr += WMI_TLV_HDR_SIZE;

	/* Fill TLV for WMITLV_TAG_STRUC_WOW_MAGIC_PATTERN_CMD but no data. */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, 0);
	buf_ptr += WMI_TLV_HDR_SIZE;

	/* Fill TLV for pattern_info_timeout but no data. */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32, 0);
	buf_ptr += WMI_TLV_HDR_SIZE;

	/* Fill TLV for ra_ratelimit_interval. */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32, sizeof(A_UINT32));
	buf_ptr += WMI_TLV_HDR_SIZE;


	*((A_UINT32 *)buf_ptr) = wma->RArateLimitInterval;

	WMA_LOGD("%s: send RA rate limit [%d] to fw vdev = %d", __func__,
			                wma->RArateLimitInterval, vdev_id);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_WOW_ADD_WAKE_PATTERN_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send RA rate limit to fw", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;

}
#endif /* FEATURE_WLAN_RA_FILTERING */


/* Configures default WOW pattern for the given vdev_id which is in STA mode. */
static VOS_STATUS wma_wow_sta(tp_wma_handle wma, u_int8_t vdev_id,
			      u_int8_t *enable_ptrn_match, bool runtime_pm)
{
	u_int8_t discvr_ptrn[] = { 0xe0, 0x00, 0x00, 0xf8 };
	u_int8_t discvr_mask[] = { 0xf0, 0x00, 0x00, 0xf8 };
	u_int8_t discvr_offset = 30;
	u_int8_t mac_mask[ETH_ALEN], free_slot;
	VOS_STATUS ret = VOS_STATUS_SUCCESS;
	u_int8_t arp_ptrn[] = { 0x08, 0x06 };
	u_int8_t arp_mask[] = { 0xff, 0xff };
	u_int8_t arp_offset = 12;
	u_int8_t ns_ptrn[] = {0x86, 0xDD};

	free_slot = wma->wow.total_free_ptrn_id - wma->wow.used_free_ptrn_id ;

	if (free_slot < WMA_STA_WOW_DEFAULT_PTRN_MAX) {
		WMA_LOGD("Free slots are not enough, avail:%d, need: %d",
			 free_slot, WMA_STA_WOW_DEFAULT_PTRN_MAX);
		WMA_LOGD("Ignoring default STA mode wow pattern for vdev : %d",
			 vdev_id);
		return ret;
	}

	WMA_LOGD("Configuring default STA mode wow pattern for vdev %d",
		  vdev_id);

	/* Setup unicast pkt pattern */
	vos_mem_set(&mac_mask, ETH_ALEN, 0xFF);
	ret = wma_send_wow_patterns_to_fw(wma, vdev_id,
			wma->wow.free_ptrn_id[wma->wow.used_free_ptrn_id++],
			wma->interfaces[vdev_id].addr, ETH_ALEN, 0,
			mac_mask, ETH_ALEN);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to add WOW unicast pattern");
		return ret;
	}

	/*
	 * Setup multicast pattern for mDNS 224.0.0.251,
	 * SSDP 239.255.255.250 and LLMNR 224.0.0.252
	 */
	if (wma->ssdp) {
		ret = wma_send_wow_patterns_to_fw(wma, vdev_id,
				wma->wow.free_ptrn_id[wma->wow.used_free_ptrn_id++],
				discvr_ptrn, sizeof(discvr_ptrn), discvr_offset,
				discvr_mask, sizeof(discvr_ptrn));
		if (ret != VOS_STATUS_SUCCESS) {
			WMA_LOGE("Failed to add WOW mDNS/SSDP/LLMNR pattern");
			return ret;
		}
	}
	else
		WMA_LOGD("mDNS, SSDP, LLMNR patterns are disabled from ini");

	/* when arp offload or ns offloaded is disabled
	 * from ini file, configure broad cast arp pattern
	 * to fw, so that host can wake up
	 */
	if (runtime_pm || !(wma->ol_ini_info & 0x1)) {
		/* Setup all ARP pkt pattern */
		ret = wma_send_wow_patterns_to_fw(wma, vdev_id,
				wma->wow.free_ptrn_id[wma->wow.used_free_ptrn_id++],
				arp_ptrn, sizeof(arp_ptrn), arp_offset,
				arp_mask, sizeof(arp_mask));
		if (ret != VOS_STATUS_SUCCESS) {
			WMA_LOGE("Failed to add WOW ARP pattern");
			return ret;
		}
	}

	/* for NS or NDP offload packets */
	if (runtime_pm || !(wma->ol_ini_info & 0x2)) {
		/* Setup all NS pkt pattern */
		ret = wma_send_wow_patterns_to_fw(wma, vdev_id,
				wma->wow.free_ptrn_id[wma->wow.used_free_ptrn_id++],
				ns_ptrn, sizeof(arp_ptrn), arp_offset,
				arp_mask, sizeof(arp_mask));
		if (ret != VOS_STATUS_SUCCESS) {
			WMA_LOGE("Failed to add WOW NS pattern");
			return ret;
		}

	}

	*enable_ptrn_match = 1 << vdev_id;
	return ret;
}

/* Finds out list of unused slots in wow pattern cache. Those free slots number
 * can be used as pattern ID while configuring default wow pattern. */
static void wma_update_free_wow_ptrn_id(tp_wma_handle wma)
{
	struct wma_wow_ptrn_cache *cache;
	u_int8_t ptrn_id;

	vos_mem_zero(wma->wow.free_ptrn_id, sizeof(wma->wow.free_ptrn_id));
	wma->wow.total_free_ptrn_id = 0;
	wma->wow.used_free_ptrn_id = 0;

	for (ptrn_id = 0; ptrn_id < wma->wlan_resource_config.num_wow_filters;
								  ptrn_id++) {
		cache = wma->wow.cache[ptrn_id];
		if (!cache) {
			wma->wow.free_ptrn_id[wma->wow.total_free_ptrn_id] =
						ptrn_id;
			wma->wow.total_free_ptrn_id += 1;

		}
	}

	WMA_LOGD("Total free wow pattern id for default patterns: %d",
		 wma->wow.total_free_ptrn_id );
}

/* Returns true if the user configured any wow pattern for given vdev id */
static bool wma_is_wow_prtn_cached(tp_wma_handle wma, u_int8_t vdev_id)
{
	struct wma_wow_ptrn_cache *cache;
	u_int8_t ptrn_id;

	for (ptrn_id = 0; ptrn_id < WOW_MAX_BITMAP_FILTERS; ptrn_id++) {
		cache = wma->wow.cache[ptrn_id];
		if (!cache)
			continue;

		if (cache->vdev_id == vdev_id)
			return true;
	}

	return false;
}

/* Unpause all the vdev after resume */
static void wma_unpause_vdev(tp_wma_handle wma) {
	int8_t vdev_id;
	struct wma_txrx_node *iface;

	for (vdev_id = 0; vdev_id < wma->max_bssid; vdev_id++) {
		if (!wma->interfaces[vdev_id].handle)
			continue;

	#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
	/* When host resume, by default, unpause all active vdev */
		if (wma->interfaces[vdev_id].pause_bitmap) {
			wdi_in_vdev_unpause(wma->interfaces[vdev_id].handle,
					    0xffffffff);
			wma->interfaces[vdev_id].pause_bitmap = 0;
		}
	#endif /* QCA_SUPPORT_TXRX_VDEV_PAUSE_LL */

		iface = &wma->interfaces[vdev_id];
		iface->conn_state = FALSE;
	}
}

static VOS_STATUS wma_resume_req(tp_wma_handle wma, bool runtime_pm)
{
	VOS_STATUS ret = VOS_STATUS_E_AGAIN;
	u_int8_t ptrn_id;

	if (runtime_pm)
		goto skip_vdev_suspend;

	wma->no_of_resume_ind++;

	if (wma->no_of_resume_ind < wma_get_vdev_count(wma))
		return VOS_STATUS_SUCCESS;

	wma->no_of_resume_ind = 0;

skip_vdev_suspend:
	WMA_LOGD("Clearing already configured wow patterns in fw%s,"
			" wow_enable: %d, wow_enable_cmd_sent: %d",
			runtime_pm ? " for runtime PM" : "",
			wma->wow.wow_enable, wma->wow.wow_enable_cmd_sent);

	if (wma->wow.wow_enable_cmd_sent) {
		WMA_LOGD("Firmware is still in WoW mode, don't delete WoW"
				" patterns");
		VOS_ASSERT(0);
		return ret;
	}

	if (!wma->wow.wow_enable) {
		WMA_LOGD("WoW pattern not configured in FW during suspend,"
				" skip delete!");
		ret = VOS_STATUS_SUCCESS;
		goto pdev_resume;
	}

	/* Clear existing wow patterns in FW. */
	for (ptrn_id = 0; ptrn_id < wma->wlan_resource_config.num_wow_filters;
		ptrn_id++) {
		ret = wma_del_wow_pattern_in_fw(wma, ptrn_id);
		if (ret != VOS_STATUS_SUCCESS)
			goto end;
	}

end:
	wma->wow.wow_enable = FALSE;
	/* Reset the DTIM Parameters */
	wma_set_resume_dtim(wma);
pdev_resume:
	wmi_set_runtime_pm_inprogress(wma->wmi_handle, FALSE);
	/* need to reset if hif_pci_suspend_fails */
	wma_set_wow_bus_suspend(wma, 0);
	/* unpause the vdev if left paused and hif_pci_suspend fails */
	wma_unpause_vdev(wma);

	if (runtime_pm)
		vos_runtime_pm_allow_suspend();

	return ret;
}

/*
 * Pushes wow patterns from local cache to FW and configures
 * wakeup trigger events.
 */
static VOS_STATUS wma_feed_wow_config_to_fw(tp_wma_handle wma,
				    v_BOOL_t pno_in_progress,
				bool extscan_in_progress, bool pno_matched,
				bool runtime_pm)
{
	struct wma_txrx_node *iface;
	VOS_STATUS ret = VOS_STATUS_SUCCESS;
	u_int8_t vdev_id;
	u_int8_t enable_ptrn_match = 0;
	v_BOOL_t ap_vdev_available = FALSE;
#ifdef QCA_IBSS_SUPPORT
	v_BOOL_t ibss_vdev_available = FALSE;
#endif

	if (wma->wow.wow_enable) {
		WMA_LOGD("Already%s Fatal Error!",
			runtime_pm ? " runtime suspended" : " cfg suspended");
		VOS_BUG(0);
		return VOS_STATUS_E_AGAIN;
	}

	/* Gather list of free ptrn id. This is needed while configuring
	* default wow patterns.
	*/
	wma_update_free_wow_ptrn_id(wma);

	for (vdev_id = 0; vdev_id < wma->max_bssid; vdev_id++) {
		iface = &wma->interfaces[vdev_id];

		if (!iface->handle ||
		    !iface->ptrn_match_enable ||
		    (!(wma_is_vdev_in_ap_mode(wma, vdev_id)
#ifdef QCA_IBSS_SUPPORT
		    || wma_is_vdev_in_ibss_mode(wma, vdev_id)
#endif
		    ) && !iface->conn_state))
			continue;

		if (wma_is_vdev_in_ap_mode(wma, vdev_id)
#ifdef QCA_IBSS_SUPPORT
		|| wma_is_vdev_in_ibss_mode(wma, vdev_id)
#endif
		)
			ap_vdev_available = TRUE;

#ifdef QCA_IBSS_SUPPORT
		if (wma_is_vdev_in_ibss_mode(wma, vdev_id))
			ibss_vdev_available = TRUE;
#endif

		if (wma_is_wow_prtn_cached(wma, vdev_id)) {
			/* Configure wow patterns provided by the user */
			ret = wma_wow_usr(wma, vdev_id, &enable_ptrn_match);
		} else if (wma_is_vdev_in_ap_mode(wma, vdev_id)
#ifdef QCA_IBSS_SUPPORT
		||wma_is_vdev_in_ibss_mode(wma, vdev_id)
#endif
		)
		{
			/* Configure AP mode default wow patterns */
			ret = wma_wow_ap(wma, vdev_id, &enable_ptrn_match);
		}
		else
		{
			/* Configure STA mode default wow patterns */
			ret = wma_wow_sta(wma, vdev_id, &enable_ptrn_match,
					runtime_pm);
		}

#ifdef FEATURE_WLAN_RA_FILTERING
		if ((ap_vdev_available == FALSE) && (wma->IsRArateLimitEnabled))
		{
			ret = wma_wow_sta_ra_filter(wma, vdev_id);

		}

#endif
		if (ret != VOS_STATUS_SUCCESS)
			goto end;
	}

	/*
	* Configure csa ie wakeup event.
	*/
	ret = wma_add_wow_wakeup_event(wma, WOW_CSA_IE_EVENT, TRUE);

	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure WOW_CSA_IE_EVENT");
		goto end;
	}
	else
		WMA_LOGD("CSA IE match is enabled in fw");

	/*
	 * Configure pattern match wakeup event. FW does pattern match
	 * only if pattern match event is enabled.
	 */
	ret = wma_add_wow_wakeup_event(wma, WOW_PATTERN_MATCH_EVENT,
				       enable_ptrn_match ? TRUE : FALSE);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to Configure WOW_PATTERN_MATCH_EVENT");
		goto end;
	} else
		WMA_LOGD("WOW_PATTERN_MATCH_EVENT enabled in fw");

	WMA_LOGD("Pattern byte match is %s in fw",
		 enable_ptrn_match ? "enabled" : "disabled");

	/* Configure magic pattern wakeup event */
	ret = wma_add_wow_wakeup_event(wma, WOW_MAGIC_PKT_RECVD_EVENT,
				       wma->wow.magic_ptrn_enable);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure magic pattern matching");
		goto end;
	} else
		WMA_LOGD("Magic pattern is %s in fw",
			wma->wow.magic_ptrn_enable ? "enabled" : "disabled");

	/* Configure deauth based wakeup */
	ret = wma_add_wow_wakeup_event(wma, WOW_DEAUTH_RECVD_EVENT,
				       wma->wow.deauth_enable);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure deauth based wakeup");
		goto end;
	} else
		WMA_LOGD("Deauth based wakeup is %s in fw",
			 wma->wow.deauth_enable ? "enabled" : "disabled");

	/* Configure disassoc based wakeup */
	ret = wma_add_wow_wakeup_event(wma, WOW_DISASSOC_RECVD_EVENT,
				       wma->wow.disassoc_enable);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure disassoc based wakeup");
		goto end;
	} else
		WMA_LOGD("Disassoc based wakeup is %s in fw",
			 wma->wow.disassoc_enable ? "enabled" : "disabled");

	/* Configure beacon miss based wakeup */
	ret = wma_add_wow_wakeup_event(wma, WOW_BMISS_EVENT,
				       wma->wow.bmiss_enable);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure beacon miss based wakeup");
		goto end;
	} else
		WMA_LOGD("Beacon miss based wakeup is %s in fw",
			 wma->wow.bmiss_enable ? "enabled" : "disabled");

#ifdef WLAN_FEATURE_GTK_OFFLOAD
	/* Configure GTK based wakeup. Passing vdev_id 0 because
	  wma_add_wow_wakeup_event always uses vdev 0 for wow wake event id*/
	ret = wma_add_wow_wakeup_event(wma, WOW_GTK_ERR_EVENT,
				       wma->wow.gtk_pdev_enable);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure GTK based wakeup");
		goto end;
	} else
		WMA_LOGD("GTK based wakeup is %s in fw",
			 wma->wow.gtk_pdev_enable ? "enabled" : "disabled");
#endif
	/* Configure probe req based wakeup */
	ret = wma_add_wow_wakeup_event(wma, WOW_PROBE_REQ_WPS_IE_EVENT,
					ap_vdev_available);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure probe req based wakeup");
		goto end;
	 } else
		WMA_LOGD("Probe req based wakeup is %s in fw",
			ap_vdev_available ? "enabled" : "disabled");

	/* Configure auth req based wakeup */
	ret = wma_add_wow_wakeup_event(wma, WOW_AUTH_REQ_EVENT,
					ap_vdev_available);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure auth req based wakeup");
		goto end;
	} else
		WMA_LOGD("Auth req based wakeup is %s in fw",
			ap_vdev_available ? "enabled" : "disabled");

	/* Configure assoc req based wakeup */
	ret = wma_add_wow_wakeup_event(wma, WOW_ASSOC_REQ_EVENT,
					ap_vdev_available);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure assoc req based wakeup");
		goto end;
	} else
		WMA_LOGD("Assoc req based wakeup is %s in fw",
			ap_vdev_available ? "enabled" : "disabled");

#ifdef FEATURE_WLAN_SCAN_PNO
	/* Configure pno based wakeup */
	ret = wma_add_wow_wakeup_event(wma, WOW_NLO_DETECTED_EVENT,
					pno_in_progress);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure pno based wakeup");
		goto end;
	} else
		WMA_LOGD("PNO based wakeup is %s in fw",
			pno_in_progress ? "enabled" : "disabled");

	/* Configure pno scan complete wakeup */
	ret = wma_add_wow_wakeup_event(wma,
				WOW_NLO_SCAN_COMPLETE_EVENT, pno_matched);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure pno scan complete wakeup");
		goto end;
	} else
		WMA_LOGD("PNO scan complete wakeup is %s in fw",
			pno_matched ? "enabled" : "disabled");
#endif

	/* Configure roaming scan better AP based wakeup */
	ret = wma_add_wow_wakeup_event(wma, WOW_BETTER_AP_EVENT,
				       TRUE);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure roaming scan better AP based wakeup");
		goto end;
	} else
		WMA_LOGD("Roaming scan better AP based wakeup is enabled in fw");

	/* Configure ADDBA/DELBA wakeup */
	ret = wma_add_wow_wakeup_event(wma, WOW_HTT_EVENT, TRUE);

	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to Configure WOW_HTT_EVENT to FW");
		goto end;
	} else
		WMA_LOGD("Successfully Configured WOW_HTT_EVENT to FW");

#ifdef FEATURE_WLAN_RA_FILTERING
	/* Configure RA filter wakeup */
	if (wma->IsRArateLimitEnabled) {
		ret = wma_add_wow_wakeup_event(wma, WOW_RA_MATCH_EVENT, TRUE);

		if (ret != VOS_STATUS_SUCCESS) {
			WMA_LOGE("Failed to Configure WOW_RA_MATCH_EVENT to FW");
			goto end;
		} else
			WMA_LOGD("Successfully Configured WOW_RA_MATCH_EVENT to FW");
	} else
		WMA_LOGD("gRAFilterEnable is not set, RA filterning is disabled");
#endif
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	ret = wma_add_wow_wakeup_event(wma, WOW_HOST_AUTO_SHUTDOWN_EVENT, TRUE);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to Configure auto shutdown WOW event to FW");
		goto end;
	} else
		WMA_LOGE("Configure auto shutdown WOW event to FW: success");
#endif

#ifdef QCA_IBSS_SUPPORT
	/* Configure beacon based wakeup */
	ret = wma_add_wow_wakeup_event(wma,
				WOW_BEACON_EVENT,ibss_vdev_available);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to configure IBSS Beacon based wakeup");
		goto end;
	} else {
		WMA_LOGD("IBSS Beacon based wakeup is %s in fw",
			ibss_vdev_available ? "enabled" : "disabled");
	}
#endif

#ifdef FEATURE_WLAN_EXTSCAN
	ret = wma_add_wow_wakeup_event(wma, WOW_EXTSCAN_EVENT,
					extscan_in_progress);

	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to Configure WOW_EXTSCAN_EVENT to FW");
		goto end;
	} else
		WMA_LOGD("Successfully Configured WOW_EXTSCAN_EVENT to FW");
#endif

	/* WOW is enabled in pcie suspend callback */
	wma->wow.wow_enable = TRUE;
	wma->wow.wow_enable_cmd_sent = FALSE;

end:
	return ret;
}

/* Adds received wow patterns in local wow pattern cache. */
static VOS_STATUS wma_wow_add_pattern(tp_wma_handle wma,
				      tpSirWowlAddBcastPtrn ptrn)
{
	struct wma_wow_ptrn_cache *cache;

	WMA_LOGD("wow add pattern");

	/* Free if there are any pattern cached already in the same slot. */
	if (wma->wow.cache[ptrn->ucPatternId])
		wma_free_wow_ptrn(wma, ptrn->ucPatternId);

	wma->wow.cache[ptrn->ucPatternId] = (struct wma_wow_ptrn_cache *)
					     vos_mem_malloc(sizeof(*cache));

	cache = wma->wow.cache[ptrn->ucPatternId];
	if (!cache) {
		WMA_LOGE("Unable to alloc memory for wow");
		return VOS_STATUS_E_NOMEM;
	}

	cache->ptrn = (u_int8_t *) vos_mem_malloc(ptrn->ucPatternSize);
	if (!cache->ptrn) {
		WMA_LOGE("Unable to alloce memory to cache wow pattern");
		vos_mem_free(cache);
		wma->wow.cache[ptrn->ucPatternId] = NULL;
		return VOS_STATUS_E_NOMEM;
	}

	cache->mask = (u_int8_t *) vos_mem_malloc(ptrn->ucPatternMaskSize);
	if (!cache->mask) {
		WMA_LOGE("Unable to alloc memory to cache wow ptrn mask");
		vos_mem_free(cache->ptrn);
		vos_mem_free(cache);
		wma->wow.cache[ptrn->ucPatternId] = NULL;
		return VOS_STATUS_E_NOMEM;
	}

	/* Cache wow pattern info until platform goes to suspend. */

	cache->vdev_id = ptrn->sessionId;
	cache->ptrn_len = ptrn->ucPatternSize;
	cache->ptrn_offset = ptrn->ucPatternByteOffset;
	cache->mask_len = ptrn->ucPatternMaskSize;

	vos_mem_copy(cache->ptrn, ptrn->ucPattern, cache->ptrn_len);
	vos_mem_copy(cache->mask, ptrn->ucPatternMask, cache->mask_len);
	wma->wow.no_of_ptrn_cached++;

	WMA_LOGD("wow pattern stored in cache (slot_id: %d, vdev id: %d)",
		 ptrn->ucPatternId, cache->vdev_id);
	return VOS_STATUS_SUCCESS;
}

/* Deletes given pattern from local wow pattern cache. */
static VOS_STATUS wma_wow_del_pattern(tp_wma_handle wma,
				      tpSirWowlDelBcastPtrn ptrn)
{
	WMA_LOGD("wow delete pattern");

	if (!wma->wow.cache[ptrn->ucPatternId]) {
		WMA_LOGE("wow pattern not found (pattern id: %d) in cache",
			 ptrn->ucPatternId);
		return VOS_STATUS_E_INVAL;
	}

	wma_free_wow_ptrn(wma, ptrn->ucPatternId);

	return VOS_STATUS_SUCCESS;
}

/*
 * Records pattern enable/disable status locally. This choice will
 * take effect when the driver enter into suspend state.
 */
static VOS_STATUS wma_wow_enter(tp_wma_handle wma,
				tpSirHalWowlEnterParams info)
{
	struct wma_txrx_node *iface;

	WMA_LOGD("wow enable req received for vdev id: %d", info->sessionId);

	if (info->sessionId > wma->max_bssid) {
		WMA_LOGE("Invalid vdev id (%d)", info->sessionId);
		vos_mem_free(info);
		return VOS_STATUS_E_INVAL;
	}

	iface = &wma->interfaces[info->sessionId];
	iface->ptrn_match_enable = info->ucPatternFilteringEnable ?
							    TRUE : FALSE;
	wma->wow.magic_ptrn_enable = info->ucMagicPktEnable ? TRUE : FALSE;
	wma->wow.deauth_enable = info->ucWowDeauthRcv ? TRUE : FALSE;
	wma->wow.disassoc_enable = info->ucWowDeauthRcv ? TRUE : FALSE;
	wma->wow.bmiss_enable = info->ucWowMaxMissedBeacons ? TRUE : FALSE;

	vos_mem_free(info);

	return VOS_STATUS_SUCCESS;
}

/* Clears all wow states */
static VOS_STATUS wma_wow_exit(tp_wma_handle wma,
			       tpSirHalWowlExitParams info)
{
	struct wma_txrx_node *iface;

	WMA_LOGD("wow disable req received for vdev id: %d", info->sessionId);

	if (info->sessionId > wma->max_bssid) {
		WMA_LOGE("Invalid vdev id (%d)", info->sessionId);
		vos_mem_free(info);
		return VOS_STATUS_E_INVAL;
	}

	iface = &wma->interfaces[info->sessionId];
	iface->ptrn_match_enable = FALSE;
	wma->wow.magic_ptrn_enable = FALSE;
	vos_mem_free(info);

	return VOS_STATUS_SUCCESS;
}

/* Handles suspend indication request received from umac. */
static VOS_STATUS wma_suspend_req(tp_wma_handle wma, tpSirWlanSuspendParam info)
{
	v_BOOL_t connected = FALSE, pno_in_progress = FALSE;
	VOS_STATUS ret;
	u_int8_t i;
	bool extscan_in_progress = false;
	bool pno_matched = false;
	struct wma_txrx_node *iface;

	if (info == NULL) {
		WMA_LOGD("runtime PM: Request to suspend all interfaces");
		wmi_set_runtime_pm_inprogress(wma->wmi_handle, TRUE);
		goto suspend_all_iface;
	}

	wma->no_of_suspend_ind++;

	if (info->sessionId > wma->max_bssid) {
		WMA_LOGE("Invalid vdev id (%d)", info->sessionId);
		vos_mem_free(info);
		return VOS_STATUS_E_INVAL;
	}

	iface = &wma->interfaces[info->sessionId];
	if (!iface) {
		WMA_LOGD("vdev %d node is not found", info->sessionId);
		vos_mem_free(info);
		return VOS_STATUS_SUCCESS;
	}

	if (!wma->wow.magic_ptrn_enable && !iface->ptrn_match_enable) {
		vos_mem_free(info);

		if (wma->no_of_suspend_ind == wma_get_vdev_count(wma)) {
			WMA_LOGD("Both magic and pattern byte match are disabled");
			wma->no_of_suspend_ind = 0;
			goto send_ready_to_suspend;
		}

		return VOS_STATUS_SUCCESS;
	}

	iface->conn_state = (info->connectedState) ? TRUE : FALSE;

	/*
	 * Once WOW is enabled in FW, host can't send anymore
	 * data to fw. umac sends suspend indication on each
	 * vdev during platform suspend. WMA has to wait until
	 * suspend indication received on last vdev before
	 * enabling wow in fw.
	 */
	if (wma->no_of_suspend_ind < wma_get_vdev_count(wma)) {
		vos_mem_free(info);
		return VOS_STATUS_SUCCESS;
	}

	wma->no_of_suspend_ind = 0;
	wma->wow.gtk_pdev_enable = 0;

suspend_all_iface:
	for (i = 0; i < wma->max_bssid && info == NULL; i++) {
		wma->interfaces[i].conn_state =
			!!(wma->interfaces[i].vdev_up &&
				!wma_is_vdev_in_ap_mode(wma, i));
	}

	/*
	 * Enable WOW if any one of the condition meets,
	 *  1) Is any one of vdev in beaconning mode (in AP mode) ?
	 *  2) Is any one of vdev in connected state (in STA mode) ?
	 *  3) Is PNO in progress in any one of vdev ?
	 *  4) Is Extscan in progress in any one of vdev ?
	 */
	for (i = 0; i < wma->max_bssid; i++) {
		if ( (wma_is_vdev_in_ap_mode(wma, i)
#ifdef QCA_IBSS_SUPPORT
		|| wma_is_vdev_in_ibss_mode(wma, i)
#endif
		    ) &&  wma->interfaces[i].vdev_up &&
		    WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
                                   WMI_SERVICE_BEACON_OFFLOAD)) {
			WMA_LOGD("vdev %d is in beaconning mode, enabling wow",
				 i);
			goto enable_wow;
		}
	}
	for (i = 0; i < wma->max_bssid; i++) {
		if (wma->interfaces[i].conn_state) {
			connected = TRUE;
			break;
		}
#ifdef FEATURE_WLAN_SCAN_PNO
		if (wma->interfaces[i].pno_in_progress) {
			WMA_LOGD("PNO is in progress, enabling wow");
			pno_in_progress = TRUE;
			if (wma->interfaces[i].nlo_match_evt_received)
				pno_matched = true;
			break;
		}
#endif
	}

#ifdef FEATURE_WLAN_EXTSCAN
	for (i = 0; i < wma->max_bssid; i++) {
		if (wma->interfaces[i].extscan_in_progress) {
			WMA_LOGD("Extscan is in progress, enabling wow");
			extscan_in_progress = true;
			break;
		}
	}
#endif

	for (i = 0; i < wma->max_bssid; i++) {
		wma->wow.gtk_pdev_enable |= wma->wow.gtk_err_enable[i];
		WMA_LOGD("VDEV_ID:%d, gtk_err_enable[%d]:%d, gtk_pdev_enable:%d",
						i, i, wma->wow.gtk_err_enable[i],
						wma->wow.gtk_pdev_enable);
	}

	if (!connected && !pno_in_progress && !extscan_in_progress) {
		WMA_LOGD("All vdev are in disconnected state and pno/extscan is not in progress, skipping wow");
		vos_mem_free(info);
		goto send_ready_to_suspend;
	}

enable_wow:
	WMA_LOGD("%sWOW Suspend", info ? "" : "Runtime PM ");

	/*
	 * At this point, suspend indication is received on
	 * last vdev. It's the time to enable wow in fw.
	 */
#ifdef FEATURE_WLAN_LPHB
	/* LPHB cache, if any item was enabled, should be
	 * applied.
	 */
	WMA_LOGD("%s: checking LPHB cache", __func__);
	for (i = 0; i < 2; i++) {
		if (wma->wow.lphb_cache[i].params.lphbEnableReq.enable) {
			WMA_LOGD("%s: LPHB cache for item %d is marked as enable",
				__func__, i + 1);
			wma_lphb_conf_hbenable(
				wma,
				&(wma->wow.lphb_cache[i]),
				FALSE);
		}
	}
#endif

	ret = wma_feed_wow_config_to_fw(wma, pno_in_progress,
				extscan_in_progress, pno_matched,
				info == NULL);
	if (ret != VOS_STATUS_SUCCESS) {
		wma_send_status_to_suspend_ind(wma, FALSE, info == NULL);
		vos_mem_free(info);
		return ret;
	}
	vos_mem_free(info);

send_ready_to_suspend:
	/* Set the Suspend DTIM Parameters */
	wma_set_suspend_dtim(wma);

	/* to handle race between hif_pci_suspend and
	* unpause/pause tx handler
	*/
	wma_set_wow_bus_suspend(wma, 1);

	wma_send_status_to_suspend_ind(wma, TRUE, info == NULL);

	return VOS_STATUS_SUCCESS;
}

/*
 * Sends host wakeup indication to FW. On receiving this indication,
 * FW will come out of WOW.
 */
static VOS_STATUS wma_send_host_wakeup_ind_to_fw(tp_wma_handle wma)
{
	wmi_wow_hostwakeup_from_sleep_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	int32_t len;
	int ret;
#ifdef CONFIG_CNSS
	tpAniSirGlobal pMac = (tpAniSirGlobal)vos_get_context(VOS_MODULE_ID_PE,
				wma->vos_context);
	if (NULL == pMac) {
		WMA_LOGE("%s: Unable to get PE context", __func__);
		return VOS_STATUS_E_FAILURE;
	}
#endif

	len = sizeof(wmi_wow_hostwakeup_from_sleep_cmd_fixed_param);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (wmi_wow_hostwakeup_from_sleep_cmd_fixed_param *)
				wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_wow_hostwakeup_from_sleep_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
				wmi_wow_hostwakeup_from_sleep_cmd_fixed_param));

	vos_event_reset(&wma->wma_resume_event);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID);
	if (ret) {
		WMA_LOGE("Failed to send host wakeup indication to fw");
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("Host wakeup indication sent to fw");

	vos_status = vos_wait_single_event(&(wma->wma_resume_event),
			WMA_RESUME_TIMEOUT);
	if (VOS_STATUS_SUCCESS != vos_status) {
		WMA_LOGP("%s: Timeout waiting for resume event from FW", __func__);
		WMA_LOGP("%s: Pending commands %d credits %d", __func__,
				wmi_get_pending_cmds(wma->wmi_handle),
				wmi_get_host_credits(wma->wmi_handle));
		if (!vos_is_logp_in_progress(VOS_MODULE_ID_WDA, NULL)) {
#ifdef CONFIG_CNSS
			if (pMac->sme.enableSelfRecovery) {
				vos_trigger_recovery();
			} else {
				VOS_BUG(0);
			}
#else
			VOS_BUG(0);
#endif
		} else {
			WMA_LOGE("%s: SSR in progress, ignore resume timeout", __func__);
		}
	} else {
		WMA_LOGD("Host wakeup received");
	}

	if (VOS_STATUS_SUCCESS == vos_status)
		wmi_set_target_suspend(wma->wmi_handle, FALSE);

	return vos_status;
}

#ifdef FEATURE_WLAN_D0WOW
/* Disable D0-WOW in firmware. */
VOS_STATUS wma_disable_d0wow_in_fw(tp_wma_handle wma)
{
	wmi_d0_wow_enable_disable_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len;
	int host_credits;
	int wmi_pending_cmds;
	int ret = 0;
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
#ifdef CONFIG_CNSS
	tpAniSirGlobal pmac;

	pmac = vos_get_context(VOS_MODULE_ID_PE, wma->vos_context);
	if (!pmac) {
		WMA_LOGE("%s: Unable to get PE context!", __func__);
		return VOS_STATUS_E_FAILURE;
	}
#endif

	len = sizeof(wmi_d0_wow_enable_disable_cmd_fixed_param);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed to allocate WMI buffer!", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (wmi_d0_wow_enable_disable_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_d0_wow_enable_disable_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_d0_wow_enable_disable_cmd_fixed_param));
	cmd->enable = 0;

	host_credits = wmi_get_host_credits(wma->wmi_handle);
	wmi_pending_cmds = wmi_get_pending_cmds(wma->wmi_handle);
	if (host_credits < WMI_WOW_REQUIRED_CREDITS) {
		WMA_LOGE("%s: No Credits when resume: %d, pending_cmds: %d, "
			"cannot resume back", __func__, host_credits,
			wmi_pending_cmds);
		HTC_dump_counter_info(wma->htc_handle);
		if (vos_is_logp_in_progress(VOS_MODULE_ID_WDA, NULL)) {
			VOS_ASSERT(0);
		} else {
#ifdef CONFIG_CNSS
			if (pmac->sme.enableSelfRecovery) {
				vos_trigger_recovery();
			} else {
				VOS_BUG(0);
			}
#endif
		}

		return VOS_STATUS_E_FAILURE;
	}

	vos_event_reset(&wma->wma_resume_event);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
		WMI_D0_WOW_ENABLE_DISABLE_CMDID);
	if (ret) {
		WMA_LOGE("Failed to disable D0-WOW in FW!");
		goto error;
	}

	vos_status = vos_wait_single_event(&(wma->wma_resume_event),
		WMA_RESUME_TIMEOUT);
	if (VOS_STATUS_SUCCESS != vos_status) {
		WMA_LOGP("%s: Timeout waiting for resume event from FW!",
			__func__);
		WMA_LOGP("%s: Pending commands: %d credits: %d", __func__,
			wmi_get_pending_cmds(wma->wmi_handle),
			wmi_get_host_credits(wma->wmi_handle));
		if (vos_is_logp_in_progress(VOS_MODULE_ID_WDA, NULL)) {
			VOS_ASSERT(0);
		} else {
#ifdef CONFIG_CNSS
			if (pmac->sme.enableSelfRecovery) {
				vos_trigger_recovery();
			} else {
				VOS_BUG(0);
			}
#endif
		}

		return VOS_STATUS_E_FAILURE;
	}

	wma->wow.wow_enable_cmd_sent = FALSE;
	wmi_set_d0wow_flag(wma->wmi_handle, FALSE);
	WMA_LOGD("D0-WOW is disabled successfully in FW.");
	return vos_status;

error:
	wmi_buf_free(buf);
	return VOS_STATUS_E_FAILURE;
}
#endif /* FEATURE_WLAN_D0WOW */

/* Disable wow in PCIe resume context. */
int wma_disable_wow_in_fw(WMA_HANDLE handle, int runtime_pm)
{
	tp_wma_handle wma = handle;
	VOS_STATUS ret;

	if (!wma->wow.wow_enable_cmd_sent) {
		return VOS_STATUS_SUCCESS;
	}

	WMA_LOGD("WoW Resume in PCIe Context\n");

#ifdef FEATURE_WLAN_D0WOW
	if (wma->ap_client_cnt > 0) {
		WMA_LOGD("Exiting D0-WOW since client count is %d.",
			wma->ap_client_cnt);
		return wma_disable_d0wow_in_fw(wma);
	}
#endif

	ret = wma_send_host_wakeup_ind_to_fw(wma);

	if (ret != VOS_STATUS_SUCCESS)
		return ret;

	wma->wow.wow_enable_cmd_sent = FALSE;

	/* To allow the tx pause/unpause events */
	wma_set_wow_bus_suspend(wma, 0);
	/* Unpause the vdev as we are resuming */
	wma_unpause_vdev(wma);

	return ret;
}

/*
 * Returns true if wow parameters (patterns, wakeup events, etc)
 * are configured in fw and waiting for wow to be enabled in fw.
 * Other cases, returns false.
 */
int wma_is_wow_mode_selected(WMA_HANDLE handle)
{
	tp_wma_handle wma = (tp_wma_handle) handle;

	return wma->wow.wow_enable;
}

tAniGetPEStatsRsp * wma_get_stats_rsp_buf(tAniGetPEStatsReq *get_stats_param)
{
	tAniGetPEStatsRsp *stats_rsp_params;
	tANI_U32 len, temp_mask, counter = 0;

	len= sizeof(tAniGetPEStatsRsp);
	temp_mask = get_stats_param->statsMask;

	while (temp_mask) {
		if (temp_mask & 1) {
			switch (counter) {
			case eCsrSummaryStats:
				len += sizeof(tCsrSummaryStatsInfo);
				break;
			case eCsrGlobalClassAStats:
				len += sizeof(tCsrGlobalClassAStatsInfo);
				break;
			case eCsrGlobalClassBStats:
				len += sizeof(tCsrGlobalClassBStatsInfo);
				break;
			case eCsrGlobalClassCStats:
				len += sizeof(tCsrGlobalClassCStatsInfo);
				break;
			case eCsrGlobalClassDStats:
				len += sizeof(tCsrGlobalClassDStatsInfo);
				break;
			case eCsrPerStaStats:
				len += sizeof(tCsrPerStaStatsInfo);
				break;
			}
		}

		counter++;
		temp_mask >>= 1;
	}

	stats_rsp_params = (tAniGetPEStatsRsp *)vos_mem_malloc(len);
	if (!stats_rsp_params) {
		WMA_LOGE("memory allocation failed for tAniGetPEStatsRsp");
		VOS_ASSERT(0);
		return NULL;
	}

	vos_mem_zero(stats_rsp_params, len);
	stats_rsp_params->staId = get_stats_param->staId;
	stats_rsp_params->statsMask = get_stats_param->statsMask;
	stats_rsp_params->msgType = WDA_GET_STATISTICS_RSP;
	stats_rsp_params->msgLen = len - sizeof(tAniGetPEStatsRsp);
	stats_rsp_params->rc = eHAL_STATUS_SUCCESS;
	return stats_rsp_params;
}

/* function    : wma_get_stats_req
 * Description : return the statistics
 * Args        : wma handle, pointer to tAniGetPEStatsReq
 * Returns     : nothing
 */
static void wma_get_stats_req(WMA_HANDLE handle,
		tAniGetPEStatsReq *get_stats_param)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	struct wma_txrx_node *node;
	wmi_buf_t buf;
	wmi_request_stats_cmd_fixed_param *cmd;
	tAniGetPEStatsRsp *pGetPEStatsRspParams;
	u_int8_t len = sizeof(wmi_request_stats_cmd_fixed_param);

	WMA_LOGD("%s: Enter", __func__);
	node = &wma_handle->interfaces[get_stats_param->sessionId];
	if (node->stats_rsp) {
		pGetPEStatsRspParams = node->stats_rsp;
		if (pGetPEStatsRspParams->staId == get_stats_param->staId &&
			pGetPEStatsRspParams->statsMask ==
				get_stats_param->statsMask) {
			WMA_LOGI("Stats for staId %d with stats mask %d "
				 "is pending.... ignore new request",
				 get_stats_param->staId,
				 get_stats_param->statsMask);
			goto end;
		} else {
			vos_mem_free(node->stats_rsp);
			node->stats_rsp = NULL;
			node->fw_stats_set = 0;
		}
	}

	pGetPEStatsRspParams = wma_get_stats_rsp_buf(get_stats_param);
	if (!pGetPEStatsRspParams)
		goto end;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed to allocate wmi buffer", __func__);
		goto failed;
	}

	node->fw_stats_set = 0;
	node->stats_rsp = pGetPEStatsRspParams;
	cmd = (wmi_request_stats_cmd_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_request_stats_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_request_stats_cmd_fixed_param));
	cmd->stats_id = WMI_REQUEST_PEER_STAT|WMI_REQUEST_PDEV_STAT|
			WMI_REQUEST_VDEV_STAT;
	cmd->vdev_id = get_stats_param->sessionId;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(node->bssid, &cmd->peer_macaddr);
	WMA_LOGD("STATS REQ VDEV_ID:%d-->", cmd->vdev_id);
	if (wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				WMI_REQUEST_STATS_CMDID)) {

		WMA_LOGE("%s: Failed to send WMI_REQUEST_STATS_CMDID",
			__func__);
		wmi_buf_free(buf);
		goto failed;
	}

	goto end;
failed:

	pGetPEStatsRspParams->rc = eHAL_STATUS_FAILURE;
	node->stats_rsp = NULL;
	/* send response to UMAC*/
	wma_send_msg(wma_handle, WDA_GET_STATISTICS_RSP, pGetPEStatsRspParams,
			0) ;
end:
	vos_mem_free(get_stats_param);
	WMA_LOGD("%s: Exit", __func__);
	return;
}

static void wma_init_scan_req(tp_wma_handle wma_handle,
				tInitScanParams *init_scan_param)
{
	WMA_LOGD("%s: Send dummy init scan response for legacy scan request",
			__func__);
	init_scan_param->status = eHAL_STATUS_SUCCESS;
	/* send ini scan response message back to PE */
	wma_send_msg(wma_handle, WDA_INIT_SCAN_RSP, (void *)init_scan_param,
			0);
}

static void wma_finish_scan_req(tp_wma_handle wma_handle,
				tFinishScanParams *finish_scan_param)
{
	WMA_LOGD("%s: Send dummy finish scan response for legacy scan request",
			__func__);
	finish_scan_param->status = eHAL_STATUS_SUCCESS;
	/* send finish scan response message back to PE */
	wma_send_msg(wma_handle, WDA_FINISH_SCAN_RSP, (void *)finish_scan_param,
			0);
}

static void wma_process_update_opmode(tp_wma_handle wma_handle,
                                tUpdateVHTOpMode *update_vht_opmode)
{
        WMA_LOGD("%s: opMode = %d", __func__, update_vht_opmode->opMode);

        wma_set_peer_param(wma_handle, update_vht_opmode->peer_mac,
                           WMI_PEER_CHWIDTH, update_vht_opmode->opMode,
                           update_vht_opmode->smesessionId);
}

static void wma_process_update_rx_nss(tp_wma_handle wma_handle,
                                tUpdateRxNss *update_rx_nss)
{
	struct wma_txrx_node *intr =
		&wma_handle->interfaces[update_rx_nss->smesessionId];
	int rxNss = update_rx_nss->rxNss;

	wma_update_txrx_chainmask(wma_handle->num_rf_chains, &rxNss);
	intr->nss = (tANI_U8) rxNss;
	update_rx_nss->rxNss = (tANI_U32) rxNss;

	WMA_LOGD("%s: Rx Nss = %d", __func__, update_rx_nss->rxNss);

	wma_set_peer_param(wma_handle, update_rx_nss->peer_mac,
			WMI_PEER_NSS, update_rx_nss->rxNss,
			update_rx_nss->smesessionId);
}

#ifdef FEATURE_OEM_DATA_SUPPORT
static void wma_start_oem_data_req(tp_wma_handle wma_handle,
				tStartOemDataReq *startOemDataReq)
{
	wmi_buf_t buf;
	u_int8_t *cmd;
	int ret = 0;
	u_int32_t *msg_subtype;
	tStartOemDataRsp *pStartOemDataRsp;

	WMA_LOGD("%s: Send OEM Data Request to target", __func__);

	if (!startOemDataReq) {
		WMA_LOGE("%s: startOemDataReq is null", __func__);
		goto out;
	}

	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not send Oem data request cmd", __func__);
		return;
	}

	buf = wmi_buf_alloc(wma_handle->wmi_handle,
		                   (OEM_DATA_REQ_SIZE + WMI_TLV_HDR_SIZE));
	if (!buf) {
		WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
		goto out;
	}

	cmd = (u_int8_t *)wmi_buf_data(buf);

	WMITLV_SET_HDR(cmd, WMITLV_TAG_ARRAY_BYTE,
			       OEM_DATA_REQ_SIZE);
	cmd += WMI_TLV_HDR_SIZE;
	vos_mem_copy(cmd, &startOemDataReq->oemDataReq[0], OEM_DATA_REQ_SIZE);

	WMA_LOGI("%s: Sending OEM Data Request to target, data len (%d)",
	         __func__, OEM_DATA_REQ_SIZE);

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
			(OEM_DATA_REQ_SIZE +
			 WMI_TLV_HDR_SIZE),
			 WMI_OEM_REQ_CMDID);

	if (ret != EOK) {
		WMA_LOGE("%s:wmi cmd send failed", __func__);
		adf_nbuf_free(buf);
	}

out:
	/* free oem data req buffer received from UMAC */
	if (startOemDataReq)
		vos_mem_free(startOemDataReq);

	/* Now send data resp back to PE/SME with message sub-type of
	 * WMI_OEM_INTERNAL_RSP. This is required so that PE/SME clears
	 * up pending active command. Later when desired oem response(s)
	 * comes as wmi event from target then those shall be passed
	 * to oem application
	 */
	pStartOemDataRsp = vos_mem_malloc(sizeof(*pStartOemDataRsp));
	if (!pStartOemDataRsp)
	{
	 WMA_LOGE("%s:failed to allocate memory for OEM Data Resp to PE",
	          __func__);
	 return;
	}
	vos_mem_zero(pStartOemDataRsp, sizeof(tStartOemDataRsp));
	msg_subtype = (u_int32_t *)(&pStartOemDataRsp->oemDataRsp[0]);
	*msg_subtype = WMI_OEM_INTERNAL_RSP;

	WMA_LOGI("%s: Sending WDA_START_OEM_DATA_RSP to clear up PE/SME pending cmd",
	         __func__);

	wma_send_msg(wma_handle, WDA_START_OEM_DATA_RSP, (void *)pStartOemDataRsp, 0);

	return;
}
#endif /* FEATURE_OEM_DATA_SUPPORT */

#ifdef FEATURE_WLAN_ESE

#define TSM_DELAY_HISTROGRAM_BINS 4
/*
 * @brief: A parallel function to WDA_ProcessTsmStatsReq for pronto. This
 *         function fetches stats from data path APIs and post
 *         WDA_TSM_STATS_RSP msg back to LIM.
 * @param: wma_handler - handle to wma
 * @param: pTsmStatsMsg - TSM stats struct that needs to be populated and
 *         passed in message.
 */
VOS_STATUS wma_process_tsm_stats_req(tp_wma_handle wma_handler,
	void *pTsmStatsMsg)
{
    u_int8_t counter;
    u_int32_t queue_delay_microsec = 0;
    u_int32_t tx_delay_microsec = 0;
    u_int16_t packet_count = 0;
    u_int16_t packet_loss_count = 0;
    tpAniTrafStrmMetrics pTsmMetric = NULL;
#ifdef FEATURE_WLAN_ESE_UPLOAD
    tpAniGetTsmStatsReq pStats = (tpAniGetTsmStatsReq)pTsmStatsMsg;
    tpAniGetTsmStatsRsp pTsmRspParams = NULL;
#else
    tpTSMStats pStats = (tpTSMStats)pTsmStatsMsg;
#endif
    int tid = pStats->tid;
    /*
     * The number of histrogram bin report by data path api are different
     * than required by TSM, hence different (6) size array used
     */
    u_int16_t bin_values[QCA_TX_DELAY_HIST_REPORT_BINS] = {0,};

    ol_txrx_pdev_handle pdev = vos_get_context(VOS_MODULE_ID_TXRX,
    wma_handler->vos_context);

    if (NULL == pdev) {
        WMA_LOGE("%s: Failed to get pdev", __func__);
        vos_mem_free(pTsmStatsMsg);
        return VOS_STATUS_E_INVAL;
    }

    /* get required values from data path APIs */
    ol_tx_delay(pdev, &queue_delay_microsec, &tx_delay_microsec, tid);
    ol_tx_delay_hist(pdev, bin_values, tid);
    ol_tx_packet_count(pdev, &packet_count, &packet_loss_count, tid );

#ifdef FEATURE_WLAN_ESE_UPLOAD
    pTsmRspParams =
    (tpAniGetTsmStatsRsp)vos_mem_malloc(sizeof(tAniGetTsmStatsRsp));
    if(NULL == pTsmRspParams)
    {
      VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
        "%s: VOS MEM Alloc Failure", __func__);
        VOS_ASSERT(0);
        vos_mem_free(pTsmStatsMsg);
      return VOS_STATUS_E_NOMEM;
    }
    pTsmRspParams->staId = pStats->staId;
    pTsmRspParams->rc    = eSIR_FAILURE;
    pTsmRspParams->tsmStatsReq = pStats;
    pTsmMetric = &pTsmRspParams->tsmMetrics;
#else
    pTsmMetric = (tpAniTrafStrmMetrics)&pStats->tsmMetrics;
#endif
    /* populate pTsmMetric */
    pTsmMetric->UplinkPktQueueDly = queue_delay_microsec;
    /* store only required number of bin values */
    for ( counter = 0; counter < TSM_DELAY_HISTROGRAM_BINS; counter++)
    {
      pTsmMetric->UplinkPktQueueDlyHist[counter] = bin_values[counter];
    }
    pTsmMetric->UplinkPktTxDly = tx_delay_microsec;
    pTsmMetric->UplinkPktLoss = packet_loss_count;
    pTsmMetric->UplinkPktCount = packet_count;

    /*
     * No need to populate roaming delay and roaming count as they are
     * being populated just before sending IAPP frame out
     */
    /* post this message to LIM/PE */
#ifdef FEATURE_WLAN_ESE_UPLOAD
    wma_send_msg(wma_handler, WDA_TSM_STATS_RSP, (void *)pTsmRspParams , 0) ;
#else
    wma_send_msg(wma_handler, WDA_TSM_STATS_RSP, (void *)pTsmStatsMsg , 0) ;
#endif
    return VOS_STATUS_SUCCESS;
}

#endif /* FEATURE_WLAN_ESE */

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void wma_set_ric_req(tp_wma_handle wma, void *msg, tANI_U8 is_add_ts)
{
	wmi_ric_request_fixed_param *cmd;
	wmi_ric_tspec *tspec_param;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	tSirMacTspecIE *ptspecIE;
	int32_t len = sizeof(wmi_ric_request_fixed_param)+
					WMI_TLV_HDR_SIZE +
					sizeof(wmi_ric_tspec);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		return;
	}

	buf_ptr = (u_int8_t *)wmi_buf_data(buf);

	cmd = (wmi_ric_request_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_ric_request_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_ric_request_fixed_param));
	if (is_add_ts)
		cmd->vdev_id = ((tAddTsParams *)msg)->sessionId;
	else
		cmd->vdev_id = ((tDelTsParams *)msg)->sessionId;
	cmd->num_ric_request = 1; /* Today we are sending only 1 ric at once */
	cmd->is_add_ric = is_add_ts;

	buf_ptr += sizeof(wmi_ric_request_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		sizeof(wmi_ric_tspec));

	buf_ptr += WMI_TLV_HDR_SIZE;
	tspec_param = (wmi_ric_tspec *)buf_ptr;
	WMITLV_SET_HDR(&tspec_param->tlv_header,
		WMITLV_TAG_STRUC_wmi_ric_tspec,
		WMITLV_GET_STRUCT_TLVLEN(wmi_ric_tspec));

	if(is_add_ts)
		ptspecIE = &(((tAddTsParams *)msg)->tspec);
	else
		ptspecIE = &(((tDelTsParams *)msg)->delTsInfo.tspec);

	/* Fill the tsinfo in the format expected by firmware */
#ifndef ANI_LITTLE_BIT_ENDIAN
	vos_mem_copy(((tANI_U8 *)&tspec_param->ts_info)+1,
			((tANI_U8 *)&ptspecIE->tsinfo)+1, 2);
#else
	vos_mem_copy(((tANI_U8 *)&tspec_param->ts_info),
			((tANI_U8 *)&ptspecIE->tsinfo)+1, 2);
#endif

	tspec_param->nominal_msdu_size = ptspecIE->nomMsduSz;
	tspec_param->maximum_msdu_size = ptspecIE->maxMsduSz;
	tspec_param->min_service_interval = ptspecIE->minSvcInterval;
	tspec_param->max_service_interval = ptspecIE->maxSvcInterval;
	tspec_param->inactivity_interval = ptspecIE->inactInterval;
	tspec_param->suspension_interval = ptspecIE->suspendInterval;
	tspec_param->svc_start_time = ptspecIE->svcStartTime;
	tspec_param->min_data_rate = ptspecIE->minDataRate;
	tspec_param->mean_data_rate = ptspecIE->meanDataRate;
	tspec_param->peak_data_rate = ptspecIE->peakDataRate;
	tspec_param->max_burst_size = ptspecIE->maxBurstSz;
	tspec_param->delay_bound = ptspecIE->delayBound;
	tspec_param->min_phy_rate = ptspecIE->minPhyRate;
	tspec_param->surplus_bw_allowance = ptspecIE->surplusBw;
	tspec_param->medium_time = 0;

	WMA_LOGI("%s: Set RIC Req is_add_ts:%d", __func__, is_add_ts);

	if (wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_ROAM_SET_RIC_REQUEST_CMDID)) {
		WMA_LOGP("%s: Failed to send vdev Set RIC Req command", __func__);
		if(is_add_ts)
			((tAddTsParams *)msg)->status = eHAL_STATUS_FAILURE;
		adf_nbuf_free(buf);
	}
}
#endif

static void wma_del_ts_req(tp_wma_handle wma, tDelTsParams *msg)
{
	wmi_vdev_wmm_delts_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		goto err;
	}
	cmd = (wmi_vdev_wmm_delts_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_vdev_wmm_delts_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_wmm_delts_cmd_fixed_param));
	cmd->vdev_id = msg->sessionId;
	cmd->ac = TID_TO_WME_AC(msg->userPrio);

	WMA_LOGD("Delts vdev:%d, ac:%d, %s:%d",
			cmd->vdev_id, cmd->ac, __FUNCTION__, __LINE__);
	if (wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_VDEV_WMM_DELTS_CMDID)) {
		WMA_LOGP("%s: Failed to send vdev DELTS command", __func__);
		adf_nbuf_free(buf);
	}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if(msg->setRICparams == true)
		wma_set_ric_req(wma, msg, false);
#endif

err:
	vos_mem_free(msg);
}

/*
 * @brief: A function to handle WDA_AGGR_QOS_REQ. This will send out
 *         ADD_TS requestes to firmware in loop for all the ACs with
 *         active flow.
 * @param: wma_handler - handle to wma
 * @param: pAggrQosRspMsg - combined struct for all ADD_TS requests.
 */
static void wma_aggr_qos_req(tp_wma_handle wma, tAggrAddTsParams *pAggrQosRspMsg)
{
    int i = 0;
    wmi_vdev_wmm_addts_cmd_fixed_param *cmd;
    wmi_buf_t buf;
    int32_t len = sizeof(*cmd);

    for( i = 0; i < HAL_QOS_NUM_AC_MAX; i++ )
    {
      // if flow in this AC is active
      if ( ((1 << i) & pAggrQosRspMsg->tspecIdx) )
      {
        /*
         * as per implementation of wma_add_ts_req() we
         * are not waiting any response from firmware so
         * apart from sending ADDTS to firmware just send
         * success to upper layers
         */
        pAggrQosRspMsg->status[i] = eHAL_STATUS_SUCCESS;

        buf = wmi_buf_alloc(wma->wmi_handle, len);
        if (!buf) {
                WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
                goto aggr_qos_exit;
        }
        cmd = (wmi_vdev_wmm_addts_cmd_fixed_param *) wmi_buf_data(buf);
        WMITLV_SET_HDR(&cmd->tlv_header,
                        WMITLV_TAG_STRUC_wmi_vdev_wmm_addts_cmd_fixed_param,
                        WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_wmm_addts_cmd_fixed_param));
        cmd->vdev_id = pAggrQosRspMsg->sessionId;
        cmd->ac = TID_TO_WME_AC(pAggrQosRspMsg->tspec[i].tsinfo.traffic.userPrio);
        cmd->medium_time_us = pAggrQosRspMsg->tspec[i].mediumTime * 32;
        cmd->downgrade_type = WMM_AC_DOWNGRADE_DEPRIO;
        WMA_LOGD("%s:%d: Addts vdev:%d, ac:%d, mediumTime:%d downgrade_type:%d",
              __func__, __LINE__, cmd->vdev_id, cmd->ac,
              cmd->medium_time_us, cmd->downgrade_type);
        if (wmi_unified_cmd_send(wma->wmi_handle, buf, len,
                                WMI_VDEV_WMM_ADDTS_CMDID)) {
                WMA_LOGP("%s: Failed to send vdev ADDTS command", __func__);
                pAggrQosRspMsg->status[i] = eHAL_STATUS_FAILURE;
                adf_nbuf_free(buf);
        }
      }
    }

aggr_qos_exit:
    // send reponse to upper layers from here only.
    wma_send_msg(wma, WDA_AGGR_QOS_RSP, pAggrQosRspMsg, 0);
}

static void wma_add_ts_req(tp_wma_handle wma, tAddTsParams *msg)
{
	wmi_vdev_wmm_addts_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t len = sizeof(*cmd);

#ifdef FEATURE_WLAN_ESE
	/*
	 * msmt_interval is in unit called TU (1 TU = 1024 us)
	 * max value of msmt_interval cannot make resulting
	 * interval_miliseconds overflow 32 bit
	 */
	tANI_U32 intervalMiliseconds;
	ol_txrx_pdev_handle pdev =
		vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);
	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		goto err;
	}

	intervalMiliseconds = (msg->tsm_interval*1024)/1000;

	ol_tx_set_compute_interval(pdev, intervalMiliseconds);
#endif
	msg->status = eHAL_STATUS_SUCCESS;

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		goto err;
	}
	cmd = (wmi_vdev_wmm_addts_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_vdev_wmm_addts_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_vdev_wmm_addts_cmd_fixed_param));
	cmd->vdev_id = msg->sme_session_id;
	cmd->ac = TID_TO_WME_AC(msg->tspec.tsinfo.traffic.userPrio);
	cmd->medium_time_us = msg->tspec.mediumTime * 32;
	cmd->downgrade_type = WMM_AC_DOWNGRADE_DROP;
	WMA_LOGD("Addts vdev:%d, ac:%d, mediumTime:%d, downgrade_type:%d %s:%d",
			cmd->vdev_id, cmd->ac, cmd->medium_time_us,
			cmd->downgrade_type, __func__, __LINE__);
	if (wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_VDEV_WMM_ADDTS_CMDID)) {
		WMA_LOGP("%s: Failed to send vdev ADDTS command", __func__);
		msg->status = eHAL_STATUS_FAILURE;
		adf_nbuf_free(buf);
	}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if(msg->setRICparams == true)
		wma_set_ric_req(wma, msg, true);
#endif

err:
	wma_send_msg(wma, WDA_ADD_TS_RSP, msg, 0);
}

static int wma_process_receive_filter_set_filter_req(tp_wma_handle wma_handle,
						tSirRcvPktFilterCfgType *rcv_filter_param)
{
	wmi_chatter_coalescing_add_filter_cmd_fixed_param *cmd;
	chatter_pkt_coalescing_filter *cmd_filter;
	u_int8_t *buf_ptr;
	wmi_buf_t buf;
	int num_rules = 1; /* Only one rule at a time */
	int len;
	int err;
	int i;

	/* allocate the memory */
	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE + sizeof(*cmd_filter) * num_rules;
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("Failed to allocate buffer to send set_param cmd");
		vos_mem_free(rcv_filter_param);
		return -ENOMEM;
	}
	buf_ptr = (u_int8_t *) wmi_buf_data(buf);

	/* fill the fixed part */
	cmd = (wmi_chatter_coalescing_add_filter_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_chatter_coalescing_add_filter_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
					wmi_chatter_coalescing_add_filter_cmd_fixed_param));
	cmd->num_of_filters = num_rules;

	/* specify the type of data in the subsequent buffer */
	buf_ptr += sizeof(*cmd);
	cmd_filter = (chatter_pkt_coalescing_filter *) buf_ptr;
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
			num_rules * sizeof(chatter_pkt_coalescing_filter));

	/* fill the actual filter data */
	buf_ptr += WMI_TLV_HDR_SIZE;
	cmd_filter = (chatter_pkt_coalescing_filter *) buf_ptr;

	WMITLV_SET_HDR(&cmd_filter->tlv_header,
			WMITLV_TAG_STRUC_wmi_chatter_pkt_coalescing_filter,
			WMITLV_GET_STRUCT_TLVLEN(chatter_pkt_coalescing_filter));

	cmd_filter->filter_id = rcv_filter_param->filterId;
	cmd_filter->max_coalescing_delay = rcv_filter_param->coalesceTime;
	cmd_filter->pkt_type = CHATTER_COALESCING_PKT_TYPE_UNICAST |
				CHATTER_COALESCING_PKT_TYPE_MULTICAST |
				CHATTER_COALESCING_PKT_TYPE_BROADCAST;
	cmd_filter->num_of_test_field = MIN(rcv_filter_param->numFieldParams,
						CHATTER_MAX_FIELD_TEST);

	for (i = 0; i < cmd_filter->num_of_test_field; i++) {
		cmd_filter->test_fields[i].offset = rcv_filter_param->paramsData[i].dataOffset;
		cmd_filter->test_fields[i].length = MIN(rcv_filter_param->paramsData[i].dataLength,
							CHATTER_MAX_TEST_FIELD_LEN32);
		cmd_filter->test_fields[i].test = rcv_filter_param->paramsData[i].cmpFlag;
		memcpy(&cmd_filter->test_fields[i].value, rcv_filter_param->paramsData[i].compareData,
			cmd_filter->test_fields[i].length);
		memcpy(&cmd_filter->test_fields[i].mask, rcv_filter_param->paramsData[i].dataMask,
			cmd_filter->test_fields[i].length);
	}
	WMA_LOGD("Chatter packets, adding filter with id: %d, num_test_fields=%d",cmd_filter->filter_id,
		cmd_filter->num_of_test_field);
	/* send the command along with data */
	err = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
					WMI_CHATTER_ADD_COALESCING_FILTER_CMDID);
	if (err) {
		WMA_LOGE("Failed to send set_param cmd");
		wmi_buf_free(buf);
		vos_mem_free(rcv_filter_param);
		return -EIO;
	}
	vos_mem_free(rcv_filter_param);
	return 0; /* SUCCESS */
}

static int wma_process_receive_filter_clear_filter_req(tp_wma_handle wma_handle,
						tSirRcvFltPktClearParam *rcv_clear_param)
{
	wmi_chatter_coalescing_delete_filter_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int err;

	/* allocate the memory */
	buf = wmi_buf_alloc(wma_handle->wmi_handle, sizeof(*cmd));
	if (!buf) {
		WMA_LOGE("Failed to allocate buffer to send set_param cmd");
		vos_mem_free(rcv_clear_param);
		return -ENOMEM;
	}

	/* fill the fixed part */
	cmd = (wmi_chatter_coalescing_delete_filter_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_chatter_coalescing_delete_filter_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
					wmi_chatter_coalescing_delete_filter_cmd_fixed_param));
	cmd->filter_id = rcv_clear_param->filterId;
	WMA_LOGD("Chatter packets, clearing filter with id: %d",cmd->filter_id);

	/* send the command along with data */
	err = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
				sizeof(*cmd), WMI_CHATTER_DELETE_COALESCING_FILTER_CMDID);
	if (err) {
		WMA_LOGE("Failed to send set_param cmd");
		wmi_buf_free(buf);
		vos_mem_free(rcv_clear_param);
		return -EIO;
	}
	vos_mem_free(rcv_clear_param);
	return 0; /* SUCCESS */
}

static int wma_set_base_macaddr_indicate(tp_wma_handle wma_handle,
					tSirMacAddr *customAddr)
{
	wmi_pdev_set_base_macaddr_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int err;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, sizeof(*cmd));
	if (!buf) {
		WMA_LOGE("Failed to allocate buffer to send set_base_macaddr cmd");
		return -ENOMEM;
	}

	cmd = (wmi_pdev_set_base_macaddr_cmd_fixed_param *) wmi_buf_data(buf);
	vos_mem_zero(cmd, sizeof(*cmd));

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_pdev_set_base_macaddr_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_pdev_set_base_macaddr_cmd_fixed_param));
	WMI_CHAR_ARRAY_TO_MAC_ADDR(*customAddr, &cmd->base_macaddr);
	err = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
			sizeof(*cmd), WMI_PDEV_SET_BASE_MACADDR_CMDID);
	if (err) {
		WMA_LOGE("Failed to send set_base_macaddr cmd");
		adf_os_mem_free(buf);
		return -EIO;
	}
	WMA_LOGD("Base MAC Addr: "MAC_ADDRESS_STR,
		MAC_ADDR_ARRAY((*customAddr)));

	return 0;
}

static void wma_data_tx_ack_work_handler(struct work_struct *ack_work)
{
	struct wma_tx_ack_work_ctx *work;
	tp_wma_handle wma_handle;
	pWDAAckFnTxComp ack_cb;

	if (vos_is_load_unload_in_progress(VOS_MODULE_ID_WDA, NULL)) {
		WMA_LOGE("%s: Driver load/unload in progress", __func__);
		return;
	}

	work = container_of(ack_work, struct wma_tx_ack_work_ctx, ack_cmp_work);
	wma_handle = work->wma_handle;
	ack_cb = wma_handle->umac_data_ota_ack_cb;

	if (work->status)
		WMA_LOGE("Data Tx Ack Cb Status %d", work->status);
	else
		WMA_LOGD("Data Tx Ack Cb Status %d", work->status);

	/* Call the Ack Cb registered by UMAC */
	if (ack_cb)
		ack_cb((tpAniSirGlobal)(wma_handle->mac_context),
				work->status ? 0 : 1);
	else
		WMA_LOGE("Data Tx Ack Cb is NULL");

	wma_handle->umac_data_ota_ack_cb = NULL;
	wma_handle->last_umac_data_nbuf = NULL;
	adf_os_mem_free(work);
	wma_handle->ack_work_ctx = NULL;
}

/**
  * wma_data_tx_ack_comp_hdlr - handles tx data ack completion
  * @context: context with which the handler is registered
  * @netbuf: tx data nbuf
  * @err: status of tx completion
  *
  * This is the cb registered with TxRx for
  * Ack Complete
  */
static void
wma_data_tx_ack_comp_hdlr(void *wma_context,
		adf_nbuf_t netbuf, int32_t status)
{
	ol_txrx_pdev_handle pdev;
	tp_wma_handle wma_handle = (tp_wma_handle)wma_context;

	if (NULL == wma_handle) {
		WMA_LOGE("%s: Invalid WMA Handle", __func__);
		return;
	}

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma_handle->vos_context);

	if (NULL == pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		return;
	}

	/*
	 * if netBuf does not match with pending nbuf then just free the
	 * netbuf and do not call ack cb
	 */
	if (wma_handle->last_umac_data_nbuf != netbuf) {
		if (wma_handle->umac_data_ota_ack_cb) {
			WMA_LOGE("%s: nbuf does not match but umac_data_ota_ack_cb is not null",
				        __func__);
		} else {
			WMA_LOGE("%s: nbuf does not match and umac_data_ota_ack_cb is also null",
				        __func__);
		}
		goto free_nbuf;
	}

	if(wma_handle && wma_handle->umac_data_ota_ack_cb) {
		struct wma_tx_ack_work_ctx *ack_work;

		ack_work =
		adf_os_mem_alloc(NULL, sizeof(struct wma_tx_ack_work_ctx));
		wma_handle->ack_work_ctx = ack_work;
		if(ack_work) {
#ifdef CONFIG_CNSS
			cnss_init_work(&ack_work->ack_cmp_work,
					wma_data_tx_ack_work_handler);
#else
			INIT_WORK(&ack_work->ack_cmp_work,
					wma_data_tx_ack_work_handler);
#endif
			ack_work->wma_handle = wma_handle;
			ack_work->sub_type = 0;
			ack_work->status = status;

			/* Schedue the Work */
			schedule_work(&ack_work->ack_cmp_work);
		}
	}

free_nbuf:
	/* unmap and freeing the tx buf as txrx is not taking care */
	adf_nbuf_unmap_single(pdev->osdev, netbuf, ADF_OS_DMA_TO_DEVICE);
	adf_nbuf_free(netbuf);
}

static int wma_add_clear_mcbc_filter(tp_wma_handle wma_handle, uint8_t vdev_id,
					tSirMacAddr multicastAddr, bool clearList)
{
	WMI_SET_MCASTBCAST_FILTER_CMD_fixed_param *cmd;
	wmi_buf_t buf;
	int err;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, sizeof(*cmd));
	if (!buf) {
		WMA_LOGE("Failed to allocate buffer to send set_param cmd");
		return -ENOMEM;
	}

	cmd = (WMI_SET_MCASTBCAST_FILTER_CMD_fixed_param *) wmi_buf_data(buf);
	vos_mem_zero(cmd, sizeof(*cmd));

	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_WMI_SET_MCASTBCAST_FILTER_CMD_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
					WMI_SET_MCASTBCAST_FILTER_CMD_fixed_param));
	cmd->action = (clearList? WMI_MCAST_FILTER_DELETE : WMI_MCAST_FILTER_SET);
	cmd->vdev_id = vdev_id;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(multicastAddr, &cmd->mcastbdcastaddr);
	err = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
					sizeof(*cmd), WMI_SET_MCASTBCAST_FILTER_CMDID);
	if (err) {
		WMA_LOGE("Failed to send set_param cmd");
		adf_os_mem_free(buf);
		return -EIO;
	}
	WMA_LOGD("Action:%d; vdev_id:%d; clearList:%d\n",
			cmd->action, vdev_id, clearList);
	WMA_LOGD("MCBC MAC Addr: %0x:%0x:%0x:%0x:%0x:%0x\n",
		multicastAddr[0], multicastAddr[1], multicastAddr[2],
		multicastAddr[3], multicastAddr[4], multicastAddr[5]);
	return 0;
}

static VOS_STATUS wma_process_mcbc_set_filter_req(tp_wma_handle wma_handle,
                                               tSirRcvFltMcAddrList *mcbc_param)
{
	uint8_t vdev_id = 0;
	int i;

	if(mcbc_param->ulMulticastAddrCnt <= 0) {
		WMA_LOGE("Number of multicast addresses is 0");
		return VOS_STATUS_E_FAILURE;
	}

	if (!wma_find_vdev_by_addr(wma_handle, mcbc_param->selfMacAddr, &vdev_id)) {
		WMA_LOGE("%s: Failed to find vdev id for %pM",
						__func__, mcbc_param->bssId);
		return VOS_STATUS_E_FAILURE;
	}
	/* set mcbc_param->action to clear MCList and reset
	 * to configure the MCList in FW
	*/

	for (i = 0; i < mcbc_param->ulMulticastAddrCnt; i++) {
		wma_add_clear_mcbc_filter(wma_handle, vdev_id,
					mcbc_param->multicastAddr[i],
					(mcbc_param->action == 0));
	}
	return VOS_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_GTK_OFFLOAD
#define GTK_OFFLOAD_ENABLE	0
#define GTK_OFFLOAD_DISABLE	1

static VOS_STATUS wma_send_gtk_offload_req(tp_wma_handle wma, u_int8_t vdev_id,
					   tpSirGtkOffloadParams params)
{
	int len;
	wmi_buf_t buf;
	WMI_GTK_OFFLOAD_CMD_fixed_param *cmd;
	VOS_STATUS status = VOS_STATUS_SUCCESS;

	WMA_LOGD("%s Enter", __func__);

	len = sizeof(*cmd);

	/* alloc wmi buffer */
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("wmi_buf_alloc failed for WMI_GTK_OFFLOAD_CMD");
		status = VOS_STATUS_E_NOMEM;
		goto out;
	}

	cmd = (WMI_GTK_OFFLOAD_CMD_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_WMI_GTK_OFFLOAD_CMD_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				WMI_GTK_OFFLOAD_CMD_fixed_param));

	cmd->vdev_id = vdev_id;

	/* Request target to enable GTK offload */
	if (params->ulFlags == GTK_OFFLOAD_ENABLE) {
		cmd->flags = GTK_OFFLOAD_ENABLE_OPCODE;
		wma->wow.gtk_err_enable[vdev_id] = TRUE;

		/* Copy the keys and replay counter */
		vos_mem_copy(cmd->KCK, params->aKCK, GTK_OFFLOAD_KCK_BYTES);
		vos_mem_copy(cmd->KEK, params->aKEK, GTK_OFFLOAD_KEK_BYTES);
		vos_mem_copy(cmd->replay_counter, &params->ullKeyReplayCounter,
			     GTK_REPLAY_COUNTER_BYTES);
	} else {
		wma->wow.gtk_err_enable[vdev_id] = FALSE;
		cmd->flags = GTK_OFFLOAD_DISABLE_OPCODE;
	}

	/* send the wmi command */
	if (wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				 WMI_GTK_OFFLOAD_CMDID)) {
		WMA_LOGE("Failed to send WMI_GTK_OFFLOAD_CMDID");
		wmi_buf_free(buf);
		status = VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("VDEVID: %d, GTK_FLAGS: x%x", vdev_id, cmd->flags);
out:
	WMA_LOGD("%s Exit", __func__);
	return status;
}

static VOS_STATUS wma_process_gtk_offload_req(tp_wma_handle wma,
					      tpSirGtkOffloadParams params)
{
	u_int8_t vdev_id;
	VOS_STATUS status = VOS_STATUS_SUCCESS;

	WMA_LOGD("%s Enter", __func__);

	/* Get the vdev id */
	if (!wma_find_vdev_by_bssid(wma, params->bssId, &vdev_id)) {
		WMA_LOGE("vdev handle is invalid for %pM", params->bssId);
		status = VOS_STATUS_E_INVAL;
		goto out;
	}

	/* Validate vdev id */
	if (vdev_id >= wma->max_bssid){
		WMA_LOGE("invalid vdev_id %d for %pM", vdev_id, params->bssId);
		status = VOS_STATUS_E_INVAL;
		goto out;
	}

	if ((params->ulFlags == GTK_OFFLOAD_ENABLE) &&
	    (wma->wow.gtk_err_enable[vdev_id] == TRUE)) {
		WMA_LOGE("%s GTK Offload already enabled. Disable it first "
			 "vdev_id %d", __func__, vdev_id);
		params->ulFlags = GTK_OFFLOAD_DISABLE;
		status = wma_send_gtk_offload_req(wma, vdev_id, params);
		if (status != VOS_STATUS_SUCCESS) {
			WMA_LOGE("%s Failed to disable GTK Offload", __func__);
			goto out;
		}
		WMA_LOGD("%s Enable GTK Offload again with updated inputs",
			 __func__);
		params->ulFlags = GTK_OFFLOAD_ENABLE;
	}
	status = wma_send_gtk_offload_req(wma, vdev_id, params);
out:
	vos_mem_free(params);
	WMA_LOGD("%s Exit", __func__);
	return status;
}

static VOS_STATUS wma_process_gtk_offload_getinfo_req(tp_wma_handle wma,
					tpSirGtkOffloadGetInfoRspParams params)
{
	u_int8_t vdev_id;
	int len;
	wmi_buf_t buf;
	WMI_GTK_OFFLOAD_CMD_fixed_param *cmd;
	VOS_STATUS status = VOS_STATUS_SUCCESS;

	WMA_LOGD("%s Enter", __func__);

	/* Get the vdev id */
	if (!wma_find_vdev_by_bssid(wma, params->bssId, &vdev_id)) {
		WMA_LOGE("vdev handle is invalid for %pM", params->bssId);
		status = VOS_STATUS_E_INVAL;
		goto out;
	}

	len = sizeof(*cmd);

	/* alloc wmi buffer */
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("wmi_buf_alloc failed for WMI_GTK_OFFLOAD_CMD");
		status = VOS_STATUS_E_NOMEM;
		goto out;
	}

	cmd = (WMI_GTK_OFFLOAD_CMD_fixed_param *)wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_WMI_GTK_OFFLOAD_CMD_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
				WMI_GTK_OFFLOAD_CMD_fixed_param));

	/* Request for GTK offload status */
	cmd->flags = GTK_OFFLOAD_REQUEST_STATUS_OPCODE;
	cmd->vdev_id = vdev_id;

	/* send the wmi command */
	if (wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				 WMI_GTK_OFFLOAD_CMDID)) {
		WMA_LOGE("Failed to send WMI_GTK_OFFLOAD_CMDID for req info");
		wmi_buf_free(buf);
		status = VOS_STATUS_E_FAILURE;
	}
out:
	vos_mem_free(params);
	WMA_LOGD("%s Exit", __func__);
	return status;
}
#endif

/*
 * Function	:	wma_enable_arp_ns_offload
 * Description	:	To configure ARP NS off load data to firmware
 *			when target goes to wow mode.
 * Args		:	@wma - wma handle, @tpSirHostOffloadReq -
 *			pHostOffloadParams,@bool bArpOnly
 * Returns	:	Returns Failure or Success based on WMI cmd.
 * Comments	:	Since firware expects ARP and NS to be configured
 *			at a time, Arp info is cached in wma and send along
 *			with NS info to make both work.
 */
static VOS_STATUS wma_enable_arp_ns_offload(tp_wma_handle wma, tpSirHostOffloadReq pHostOffloadParams, bool bArpOnly)
{
	int32_t i;
	int32_t res;
	WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param *cmd;
	WMI_NS_OFFLOAD_TUPLE *ns_tuple;
	WMI_ARP_OFFLOAD_TUPLE *arp_tuple;
	A_UINT8* buf_ptr;
	wmi_buf_t buf;
	int32_t len;
	u_int8_t vdev_id;
	uint32_t count = 0, num_ns_ext_tuples = 0;

	/* Get the vdev id */
	if (!wma_find_vdev_by_bssid(wma, pHostOffloadParams->bssId, &vdev_id)) {
		WMA_LOGE("vdev handle is invalid for %pM", pHostOffloadParams->bssId);
		vos_mem_free(pHostOffloadParams);
		return VOS_STATUS_E_INVAL;
	}

	if (!wma->interfaces[vdev_id].vdev_up) {

		WMA_LOGE("vdev %d is not up skipping arp/ns offload", vdev_id);
		vos_mem_free(pHostOffloadParams);
		return VOS_STATUS_E_FAILURE;
	}

	if (!bArpOnly)
		count = pHostOffloadParams->num_ns_offload_count;

	len = sizeof(WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param) +
		WMI_TLV_HDR_SIZE + // TLV place holder size for array of NS tuples
		WMI_MAX_NS_OFFLOADS*sizeof(WMI_NS_OFFLOAD_TUPLE) +
		WMI_TLV_HDR_SIZE + // TLV place holder size for array of ARP tuples
		WMI_MAX_ARP_OFFLOADS*sizeof(WMI_ARP_OFFLOAD_TUPLE);

	/*
	 * If there are more than WMI_MAX_NS_OFFLOADS addresses then allocate
	 * extra length for extended NS offload tuples which follows ARP offload
	 * tuples. Host needs to fill this structure in following format:
	 * 2 NS ofload tuples
	 * 2 ARP offload tuples
	 * N numbers of extended NS offload tuples if HDD has given more than
	 * 2 NS offload addresses
	 */
	if (!bArpOnly && count > WMI_MAX_NS_OFFLOADS) {
		num_ns_ext_tuples = count - WMI_MAX_NS_OFFLOADS;
		len += WMI_TLV_HDR_SIZE + num_ns_ext_tuples *
					sizeof(WMI_NS_OFFLOAD_TUPLE);
	}

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		vos_mem_free(pHostOffloadParams);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (A_UINT8*)wmi_buf_data(buf);
	cmd = (WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param));
	cmd->flags = 0;
	cmd->vdev_id = vdev_id;
	if (!bArpOnly)
		cmd->num_ns_ext_tuples = num_ns_ext_tuples;

	WMA_LOGD("ARP NS Offload vdev_id: %d",cmd->vdev_id);

	/* Have copy of arp info to send along with NS, Since FW expects
	 * both ARP and NS info in single cmd */
	if(bArpOnly)
		vos_mem_copy(&wma->mArpInfo, pHostOffloadParams, sizeof(tSirHostOffloadReq));

	buf_ptr += sizeof(WMI_SET_ARP_NS_OFFLOAD_CMD_fixed_param);
	WMITLV_SET_HDR(buf_ptr,WMITLV_TAG_ARRAY_STRUC,(WMI_MAX_NS_OFFLOADS*sizeof(WMI_NS_OFFLOAD_TUPLE)));
	buf_ptr += WMI_TLV_HDR_SIZE;
	for(i = 0; i < WMI_MAX_NS_OFFLOADS; i++ ){
		ns_tuple = (WMI_NS_OFFLOAD_TUPLE *)buf_ptr;
		WMITLV_SET_HDR(&ns_tuple->tlv_header,
				WMITLV_TAG_STRUC_WMI_NS_OFFLOAD_TUPLE,
				(sizeof(WMI_NS_OFFLOAD_TUPLE)-WMI_TLV_HDR_SIZE));

		/* Fill data only for NS offload in the first ARP tuple for LA */
		if (!bArpOnly  &&
		   ((pHostOffloadParams->enableOrDisable & SIR_OFFLOAD_ENABLE))) {
			ns_tuple->flags |= WMI_NSOFF_FLAGS_VALID;

#ifdef WLAN_NS_OFFLOAD
			/*Copy the target/solicitation/remote ip addr */
			if(pHostOffloadParams->nsOffloadInfo.targetIPv6AddrValid[i])
				A_MEMCPY(&ns_tuple->target_ipaddr[0],
				&pHostOffloadParams->nsOffloadInfo.targetIPv6Addr[i], sizeof(WMI_IPV6_ADDR));
			A_MEMCPY(&ns_tuple->solicitation_ipaddr,
				&pHostOffloadParams->nsOffloadInfo.selfIPv6Addr[i], sizeof(WMI_IPV6_ADDR));
			WMA_LOGD("Index %d NS solicitedIp: %pI6, targetIp: %pI6", i,
				&pHostOffloadParams->nsOffloadInfo.selfIPv6Addr[i],
				&pHostOffloadParams->nsOffloadInfo.targetIPv6Addr[i]);

			/* target MAC is optional, check if it is valid, if this is not valid,
			* the target will use the known local MAC address rather than the tuple */
			WMI_CHAR_ARRAY_TO_MAC_ADDR(pHostOffloadParams->nsOffloadInfo.selfMacAddr,
					&ns_tuple->target_mac);
#endif
			if ((ns_tuple->target_mac.mac_addr31to0 != 0) ||
				(ns_tuple->target_mac.mac_addr47to32 != 0))
			{
				ns_tuple->flags |= WMI_NSOFF_FLAGS_MAC_VALID;
			}
		}
	        buf_ptr += sizeof(WMI_NS_OFFLOAD_TUPLE);
	}

	WMITLV_SET_HDR(buf_ptr,WMITLV_TAG_ARRAY_STRUC,(WMI_MAX_ARP_OFFLOADS*sizeof(WMI_ARP_OFFLOAD_TUPLE)));
	buf_ptr += WMI_TLV_HDR_SIZE;
	for(i = 0; i < WMI_MAX_ARP_OFFLOADS; i++){
		arp_tuple = (WMI_ARP_OFFLOAD_TUPLE *)buf_ptr;
		WMITLV_SET_HDR(&arp_tuple->tlv_header,
				WMITLV_TAG_STRUC_WMI_ARP_OFFLOAD_TUPLE,
				WMITLV_GET_STRUCT_TLVLEN(WMI_ARP_OFFLOAD_TUPLE));

		/* Fill data for ARP and NS in the first tupple for LA */
		if ((wma->mArpInfo.enableOrDisable & SIR_OFFLOAD_ENABLE) && (i==0)) {
			/*Copy the target ip addr and flags*/
			arp_tuple->flags = WMI_ARPOFF_FLAGS_VALID;
			A_MEMCPY(&arp_tuple->target_ipaddr,wma->mArpInfo.params.hostIpv4Addr,
						SIR_IPV4_ADDR_LEN);
			WMA_LOGD("ARPOffload IP4 address: %pI4",
					wma->mArpInfo.params.hostIpv4Addr);
		}
		buf_ptr += sizeof(WMI_ARP_OFFLOAD_TUPLE);
	}

	/* Populate extended NS offload tuples */
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
					(num_ns_ext_tuples*sizeof(WMI_NS_OFFLOAD_TUPLE)));
	buf_ptr += WMI_TLV_HDR_SIZE;

	if (num_ns_ext_tuples) {
		for(i = WMI_MAX_NS_OFFLOADS; i < count; i++ ){
			ns_tuple = (WMI_NS_OFFLOAD_TUPLE *)buf_ptr;
			WMITLV_SET_HDR(&ns_tuple->tlv_header,
				WMITLV_TAG_STRUC_WMI_NS_OFFLOAD_TUPLE,
				(sizeof(WMI_NS_OFFLOAD_TUPLE)-WMI_TLV_HDR_SIZE));

			/* Fill data only for NS offload in the first ARP tuple for LA */
			if (!bArpOnly  &&
				((pHostOffloadParams->enableOrDisable & SIR_OFFLOAD_ENABLE))) {
				ns_tuple->flags |= WMI_NSOFF_FLAGS_VALID;
#ifdef WLAN_NS_OFFLOAD
				/*Copy the target/solicitation/remote ip addr */
				if(pHostOffloadParams->nsOffloadInfo.targetIPv6AddrValid[i])
					A_MEMCPY(&ns_tuple->target_ipaddr[0],
						&pHostOffloadParams->nsOffloadInfo.targetIPv6Addr[i],
						sizeof(WMI_IPV6_ADDR));
				A_MEMCPY(&ns_tuple->solicitation_ipaddr,
					&pHostOffloadParams->nsOffloadInfo.selfIPv6Addr[i],
					sizeof(WMI_IPV6_ADDR));
				WMA_LOGD("Index %d NS solicitedIp: %pI6, targetIp: %pI6", i,
					&pHostOffloadParams->nsOffloadInfo.selfIPv6Addr[i],
					&pHostOffloadParams->nsOffloadInfo.targetIPv6Addr[i]);

				/* target MAC is optional, check if it is valid, if this is not valid,
				 * the target will use the known local MAC address rather than the tuple */
				 WMI_CHAR_ARRAY_TO_MAC_ADDR(pHostOffloadParams->nsOffloadInfo.selfMacAddr,
					&ns_tuple->target_mac);
#endif
				if ((ns_tuple->target_mac.mac_addr31to0 != 0) ||
					(ns_tuple->target_mac.mac_addr47to32 != 0)) {
					ns_tuple->flags |= WMI_NSOFF_FLAGS_MAC_VALID;
				}
			}
			buf_ptr += sizeof(WMI_NS_OFFLOAD_TUPLE);
		}
	}

	res = wmi_unified_cmd_send(wma->wmi_handle, buf, len, WMI_SET_ARP_NS_OFFLOAD_CMDID);
	if(res) {
		WMA_LOGE("Failed to enable ARP NDP/NSffload");
		wmi_buf_free(buf);
		vos_mem_free(pHostOffloadParams);
		return VOS_STATUS_E_FAILURE;
	}

	vos_mem_free(pHostOffloadParams);
	return VOS_STATUS_SUCCESS;
}

typedef struct {
	int32_t rate;
	tANI_U8 flag;
} wma_search_rate_t;

#define WMA_MAX_OFDM_CCK_RATE_TBL_SIZE 12
/* In ofdm_cck_rate_tbl->flag, if bit 7 is 1 it's CCK, otherwise it ofdm.
 * Lower bit carries the ofdm/cck index for encoding the rate */
static wma_search_rate_t ofdm_cck_rate_tbl[WMA_MAX_OFDM_CCK_RATE_TBL_SIZE] = {
	{540, 4},            /* 4: OFDM 54 Mbps */
	{480, 0},            /* 0: OFDM 48 Mbps */
	{360, 5},            /* 5: OFDM 36 Mbps */
	{240, 1},            /* 1: OFDM 24 Mbps */
	{180, 6},            /* 6: OFDM 18 Mbps */
	{120, 2},            /* 2: OFDM 12 Mbps */
	{110, (1 << 7)},     /* 0: CCK 11 Mbps Long */
	{90,  7},            /* 7: OFDM 9 Mbps  */
	{60,  3},            /* 3: OFDM 6 Mbps  */
	{55,  ((1 << 7)|1)}, /* 1: CCK 5.5 Mbps Long */
	{20,  ((1 << 7)|2)}, /* 2: CCK 2 Mbps Long   */
	{10,  ((1 << 7)|3)}  /* 3: CCK 1 Mbps Long   */
};

#define WMA_MAX_VHT20_RATE_TBL_SIZE 9
/* In vht20_400ns_rate_tbl flag carries the mcs index for encoding the rate */
static wma_search_rate_t vht20_400ns_rate_tbl[WMA_MAX_VHT20_RATE_TBL_SIZE] = {
	{867, 8}, /* MCS8 1SS short GI */
	{722, 7}, /* MCS7 1SS short GI */
	{650, 6}, /* MCS6 1SS short GI */
	{578, 5}, /* MCS5 1SS short GI */
	{433, 4}, /* MCS4 1SS short GI */
	{289, 3}, /* MCS3 1SS short GI */
	{217, 2}, /* MCS2 1SS short GI */
	{144, 1}, /* MCS1 1SS short GI */
	{72,  0}  /* MCS0 1SS short GI */
};
/* In vht20_800ns_rate_tbl flag carries the mcs index for encoding the rate */
static wma_search_rate_t vht20_800ns_rate_tbl[WMA_MAX_VHT20_RATE_TBL_SIZE] = {
	{780, 8}, /* MCS8 1SS long GI */
	{650, 7}, /* MCS7 1SS long GI */
	{585, 6}, /* MCS6 1SS long GI */
	{520, 5}, /* MCS5 1SS long GI */
	{390, 4}, /* MCS4 1SS long GI */
	{260, 3}, /* MCS3 1SS long GI */
	{195, 2}, /* MCS2 1SS long GI */
	{130, 1}, /* MCS1 1SS long GI */
	{65,  0}  /* MCS0 1SS long GI */
};

#define WMA_MAX_VHT40_RATE_TBL_SIZE 10
/* In vht40_400ns_rate_tbl flag carries the mcs index for encoding the rate */
static wma_search_rate_t vht40_400ns_rate_tbl[WMA_MAX_VHT40_RATE_TBL_SIZE] = {
	{2000, 9}, /* MCS9 1SS short GI */
	{1800, 8}, /* MCS8 1SS short GI */
	{1500, 7}, /* MCS7 1SS short GI */
	{1350, 6}, /* MCS6 1SS short GI */
	{1200, 5}, /* MCS5 1SS short GI */
	{900,  4}, /* MCS4 1SS short GI */
	{600,  3}, /* MCS3 1SS short GI */
	{450,  2}, /* MCS2 1SS short GI */
	{300,  1}, /* MCS1 1SS short GI */
	{150,  0}, /* MCS0 1SS short GI */
};
static wma_search_rate_t vht40_800ns_rate_tbl[WMA_MAX_VHT40_RATE_TBL_SIZE] = {
	{1800, 9}, /* MCS9 1SS long GI */
	{1620, 8}, /* MCS8 1SS long GI */
	{1350, 7}, /* MCS7 1SS long GI */
	{1215, 6}, /* MCS6 1SS long GI */
	{1080, 5}, /* MCS5 1SS long GI */
	{810,  4}, /* MCS4 1SS long GI */
	{540,  3}, /* MCS3 1SS long GI */
	{405,  2}, /* MCS2 1SS long GI */
	{270,  1}, /* MCS1 1SS long GI */
	{135,  0}  /* MCS0 1SS long GI */
};

#define WMA_MAX_VHT80_RATE_TBL_SIZE 10
static wma_search_rate_t vht80_400ns_rate_tbl[WMA_MAX_VHT80_RATE_TBL_SIZE] = {
	{4333, 9}, /* MCS9 1SS short GI */
	{3900, 8}, /* MCS8 1SS short GI */
	{3250, 7}, /* MCS7 1SS short GI */
	{2925, 6}, /* MCS6 1SS short GI */
	{2600, 5}, /* MCS5 1SS short GI */
	{1950, 4}, /* MCS4 1SS short GI */
	{1300, 3}, /* MCS3 1SS short GI */
	{975,  2}, /* MCS2 1SS short GI */
	{650,  1}, /* MCS1 1SS short GI */
	{325,  0}  /* MCS0 1SS short GI */
};
static wma_search_rate_t vht80_800ns_rate_tbl[WMA_MAX_VHT80_RATE_TBL_SIZE] = {
	{3900, 9}, /* MCS9 1SS long GI */
	{3510, 8}, /* MCS8 1SS long GI */
	{2925, 7}, /* MCS7 1SS long GI */
	{2633, 6}, /* MCS6 1SS long GI */
	{2340, 5}, /* MCS5 1SS long GI */
	{1755, 4}, /* MCS4 1SS long GI */
	{1170, 3}, /* MCS3 1SS long GI */
	{878,  2}, /* MCS2 1SS long GI */
	{585,  1}, /* MCS1 1SS long GI */
	{293,  0}  /* MCS0 1SS long GI */
};

#define WMA_MAX_HT20_RATE_TBL_SIZE 8
static wma_search_rate_t ht20_400ns_rate_tbl[WMA_MAX_HT20_RATE_TBL_SIZE] = {
	{722, 7}, /* MCS7 1SS short GI */
	{650, 6}, /* MCS6 1SS short GI */
	{578, 5}, /* MCS5 1SS short GI */
	{433, 4}, /* MCS4 1SS short GI */
	{289, 3}, /* MCS3 1SS short GI */
	{217, 2}, /* MCS2 1SS short GI */
	{144, 1}, /* MCS1 1SS short GI */
	{72,  0}  /* MCS0 1SS short GI */
};
static wma_search_rate_t ht20_800ns_rate_tbl[WMA_MAX_HT20_RATE_TBL_SIZE] = {
	{650, 7}, /* MCS7 1SS long GI */
	{585, 6}, /* MCS6 1SS long GI */
	{520, 5}, /* MCS5 1SS long GI */
	{390, 4}, /* MCS4 1SS long GI */
	{260, 3}, /* MCS3 1SS long GI */
	{195, 2}, /* MCS2 1SS long GI */
	{130, 1}, /* MCS1 1SS long GI */
	{65,  0}  /* MCS0 1SS long GI */
};

#define WMA_MAX_HT40_RATE_TBL_SIZE 8
static wma_search_rate_t ht40_400ns_rate_tbl[WMA_MAX_HT40_RATE_TBL_SIZE] = {
	{1500, 7}, /* MCS7 1SS short GI */
	{1350, 6}, /* MCS6 1SS short GI */
	{1200, 5}, /* MCS5 1SS short GI */
	{900,  4}, /* MCS4 1SS short GI */
	{600,  3}, /* MCS3 1SS short GI */
	{450,  2}, /* MCS2 1SS short GI */
	{300,  1}, /* MCS1 1SS short GI */
	{150,  0}  /* MCS0 1SS short GI */
};
static wma_search_rate_t ht40_800ns_rate_tbl[WMA_MAX_HT40_RATE_TBL_SIZE] = {
	{1350, 7}, /* MCS7 1SS long GI */
	{1215, 6}, /* MCS6 1SS long GI */
	{1080, 5}, /* MCS5 1SS long GI */
	{810,  4}, /* MCS4 1SS long GI */
	{540,  3}, /* MCS3 1SS long GI */
	{405,  2}, /* MCS2 1SS long GI */
	{270,  1}, /* MCS1 1SS long GI */
	{135,  0}  /* MCS0 1SS long GI */
};

static void wma_bin_search_rate(wma_search_rate_t *tbl, int32_t tbl_size,
	tANI_S32 *mbpsx10_rate, tANI_U8 *ret_flag)
{
	int32_t upper, lower, mid;

	/* the table is descenting. index holds the largest value and the
	 * bottom index holds the smallest value */

	upper = 0; /* index 0 */
	lower = tbl_size -1; /* last index */

	if (*mbpsx10_rate >= tbl[upper].rate) {
		/* use the largest rate */
		*mbpsx10_rate = tbl[upper].rate;
		*ret_flag = tbl[upper].flag;
		return;
	} else if (*mbpsx10_rate <= tbl[lower].rate) {
		/* use the smallest rate */
		*mbpsx10_rate = tbl[lower].rate;
		*ret_flag = tbl[lower].flag;
		return;
	}
	/* now we do binery search to get the floor value */
	while (lower - upper > 1) {
		mid = (upper + lower) >> 1;
		if (*mbpsx10_rate == tbl[mid].rate) {
		/* found the exact match */
			*mbpsx10_rate = tbl[mid].rate;
			*ret_flag = tbl[mid].flag;
			return;
		} else {
		/* not found. if mid's rate is larger than input move
		 * upper to mid. If mid's rate is larger than input
		 * move lower to mid. */
			if (*mbpsx10_rate > tbl[mid].rate)
				lower = mid;
			else
				upper = mid;
		}
	}
	/* after the bin search the index is the ceiling of rate */
	*mbpsx10_rate = tbl[upper].rate;
	*ret_flag = tbl[upper].flag;
	return;
}

static VOS_STATUS wma_fill_ofdm_cck_mcast_rate(tANI_S32 mbpsx10_rate,
	tANI_U8 nss, tANI_U8 *rate)
{
	tANI_U8 idx = 0;
	wma_bin_search_rate(ofdm_cck_rate_tbl, WMA_MAX_OFDM_CCK_RATE_TBL_SIZE,
		&mbpsx10_rate, &idx);

	/* if bit 7 is set it uses CCK */
	if (idx & 0x80)
		*rate |= (1 << 6) | (idx & 0xF); /* set bit 6 to 1 for CCK */
	else
		*rate |= (idx & 0xF);
	return VOS_STATUS_SUCCESS;
}

static void wma_set_ht_vht_mcast_rate(u_int32_t shortgi, tANI_S32 mbpsx10_rate,
	tANI_U8 sgi_idx, tANI_S32 sgi_rate, tANI_U8 lgi_idx, tANI_S32 lgi_rate,
	tANI_U8 premable, tANI_U8 *rate, tANI_S32 *streaming_rate)
{
	if (shortgi == 0) {
		*rate |= (premable << 6) | (lgi_idx & 0xF);
		*streaming_rate = lgi_rate;
	} else {
		*rate |= (premable << 6) | (sgi_idx & 0xF);
		*streaming_rate = sgi_rate;
	}
}

static VOS_STATUS wma_fill_ht20_mcast_rate(u_int32_t shortgi,
	tANI_S32 mbpsx10_rate, tANI_U8 nss, tANI_U8 *rate,
	tANI_S32 *streaming_rate)
{
	tANI_U8 sgi_idx = 0, lgi_idx = 0;
	tANI_S32 sgi_rate, lgi_rate;
	if (nss == 1)
		mbpsx10_rate= mbpsx10_rate >> 1;

	sgi_rate = mbpsx10_rate;
	lgi_rate = mbpsx10_rate;
	if (shortgi)
		wma_bin_search_rate(ht20_400ns_rate_tbl,
			WMA_MAX_HT20_RATE_TBL_SIZE, &sgi_rate, &sgi_idx);
	else
		wma_bin_search_rate(ht20_800ns_rate_tbl,
			WMA_MAX_HT20_RATE_TBL_SIZE, &lgi_rate, &lgi_idx);

	wma_set_ht_vht_mcast_rate(shortgi, mbpsx10_rate, sgi_idx, sgi_rate,
		lgi_idx, lgi_rate, 2, rate, streaming_rate);
	if (nss == 1)
		*streaming_rate = *streaming_rate << 1;
	return VOS_STATUS_SUCCESS;
}

static VOS_STATUS wma_fill_ht40_mcast_rate(u_int32_t shortgi,
	tANI_S32 mbpsx10_rate, tANI_U8 nss, tANI_U8 *rate,
	tANI_S32 *streaming_rate)
{
	tANI_U8 sgi_idx = 0, lgi_idx = 0;
	tANI_S32 sgi_rate, lgi_rate;

	/* for 2x2 divide the rate by 2 */
	if (nss == 1)
		mbpsx10_rate= mbpsx10_rate >> 1;

	sgi_rate = mbpsx10_rate;
	lgi_rate = mbpsx10_rate;
	if (shortgi)
		wma_bin_search_rate(ht40_400ns_rate_tbl,
			WMA_MAX_HT40_RATE_TBL_SIZE, &sgi_rate, &sgi_idx);
	else
		wma_bin_search_rate(ht40_800ns_rate_tbl,
			WMA_MAX_HT40_RATE_TBL_SIZE, &lgi_rate, &lgi_idx);

	wma_set_ht_vht_mcast_rate(shortgi, mbpsx10_rate, sgi_idx, sgi_rate,
			lgi_idx, lgi_rate, 2, rate, streaming_rate);

	return VOS_STATUS_SUCCESS;
}

static VOS_STATUS wma_fill_vht20_mcast_rate(u_int32_t shortgi,
	tANI_S32 mbpsx10_rate, tANI_U8 nss, tANI_U8 *rate,
	tANI_S32 *streaming_rate)
{
	tANI_U8 sgi_idx = 0, lgi_idx = 0;
	tANI_S32 sgi_rate, lgi_rate;

	/* for 2x2 divide the rate by 2 */
	if (nss == 1)
		mbpsx10_rate= mbpsx10_rate >> 1;

	sgi_rate = mbpsx10_rate;
	lgi_rate = mbpsx10_rate;
	if (shortgi)
		wma_bin_search_rate(vht20_400ns_rate_tbl,
			WMA_MAX_VHT20_RATE_TBL_SIZE, &sgi_rate, &sgi_idx);
	else
		wma_bin_search_rate(vht20_800ns_rate_tbl,
			WMA_MAX_VHT20_RATE_TBL_SIZE, &lgi_rate, &lgi_idx);

	wma_set_ht_vht_mcast_rate(shortgi, mbpsx10_rate, sgi_idx, sgi_rate,
			lgi_idx, lgi_rate, 3, rate, streaming_rate);
	if (nss == 1)
		*streaming_rate = *streaming_rate << 1;
	return VOS_STATUS_SUCCESS;
}

static VOS_STATUS wma_fill_vht40_mcast_rate(u_int32_t shortgi,
	tANI_S32 mbpsx10_rate, tANI_U8 nss, tANI_U8 *rate,
	tANI_S32 *streaming_rate)
{
	tANI_U8 sgi_idx = 0, lgi_idx = 0;
	tANI_S32 sgi_rate, lgi_rate;

	/* for 2x2 divide the rate by 2 */
	if (nss == 1)
		mbpsx10_rate= mbpsx10_rate >> 1;

	sgi_rate = mbpsx10_rate;
	lgi_rate = mbpsx10_rate;
	if (shortgi)
		wma_bin_search_rate(vht40_400ns_rate_tbl,
			WMA_MAX_VHT40_RATE_TBL_SIZE, &sgi_rate, &sgi_idx);
	else
		wma_bin_search_rate(vht40_800ns_rate_tbl,
			WMA_MAX_VHT40_RATE_TBL_SIZE, &lgi_rate, &lgi_idx);

	wma_set_ht_vht_mcast_rate(shortgi, mbpsx10_rate,
					sgi_idx, sgi_rate, lgi_idx, lgi_rate,
					3, rate, streaming_rate);
	if (nss == 1)
		*streaming_rate = *streaming_rate << 1;
	return VOS_STATUS_SUCCESS;
}

static VOS_STATUS wma_fill_vht80_mcast_rate(u_int32_t shortgi,
	tANI_S32 mbpsx10_rate, tANI_U8 nss, tANI_U8 *rate,
	tANI_S32 *streaming_rate)
{
	tANI_U8 sgi_idx = 0, lgi_idx = 0;
	tANI_S32 sgi_rate, lgi_rate;

	/* for 2x2 divide the rate by 2 */
	if (nss == 1)
		mbpsx10_rate= mbpsx10_rate >> 1;

	sgi_rate = mbpsx10_rate;
	lgi_rate = mbpsx10_rate;
	if (shortgi)
		wma_bin_search_rate(vht80_400ns_rate_tbl,
			WMA_MAX_VHT80_RATE_TBL_SIZE, &sgi_rate, &sgi_idx);
	else
		wma_bin_search_rate(vht80_800ns_rate_tbl,
			WMA_MAX_VHT80_RATE_TBL_SIZE, &lgi_rate, &lgi_idx);

	wma_set_ht_vht_mcast_rate(shortgi, mbpsx10_rate, sgi_idx, sgi_rate,
			lgi_idx, lgi_rate, 3, rate, streaming_rate);
	if (nss == 1)
		*streaming_rate = *streaming_rate << 1;
	return VOS_STATUS_SUCCESS;
}

static VOS_STATUS wma_fill_ht_mcast_rate(u_int32_t shortgi,
	u_int32_t chwidth, tANI_S32 mbpsx10_rate, tANI_U8 nss,
	WLAN_PHY_MODE chanmode, tANI_U8 *rate, tANI_S32 *streaming_rate)
{
	int32_t ret = 0;

	*streaming_rate = 0;
	if (chwidth== 0)
		ret = wma_fill_ht20_mcast_rate(shortgi, mbpsx10_rate,
						nss, rate, streaming_rate);
	else if (chwidth == 1)
		ret = wma_fill_ht40_mcast_rate(shortgi, mbpsx10_rate,
						nss, rate, streaming_rate);
	else
		WMA_LOGE("%s: Error, Invalid chwidth enum %d", __func__, chwidth);
	return (*streaming_rate != 0) ? VOS_STATUS_SUCCESS : VOS_STATUS_E_INVAL;
}

static VOS_STATUS wma_fill_vht_mcast_rate(u_int32_t shortgi,
	u_int32_t chwidth, tANI_S32 mbpsx10_rate, tANI_U8 nss,
	WLAN_PHY_MODE chanmode, tANI_U8 *rate, tANI_S32 *streaming_rate)
{
	int32_t ret = 0;

	*streaming_rate = 0;
	if (chwidth== 0)
		ret = wma_fill_vht20_mcast_rate(shortgi, mbpsx10_rate, nss,
						rate, streaming_rate);
	else if (chwidth == 1)
		ret = wma_fill_vht40_mcast_rate(shortgi, mbpsx10_rate, nss,
						rate, streaming_rate);
	else if (chwidth == 2)
		ret = wma_fill_vht80_mcast_rate(shortgi, mbpsx10_rate, nss,
						rate, streaming_rate);
	else
		WMA_LOGE("%s: chwidth enum %d not supported",
			__func__, chwidth);
	return (*streaming_rate != 0) ? VOS_STATUS_SUCCESS : VOS_STATUS_E_INVAL;
}

#define WMA_MCAST_1X1_CUT_OFF_RATE 2000
/*
 * FUNCTION: wma_encode_mc_rate
 *
 */
static VOS_STATUS wma_encode_mc_rate(u_int32_t shortgi, u_int32_t chwidth,
	WLAN_PHY_MODE chanmode, A_UINT32 mhz, tANI_S32 mbpsx10_rate,
	tANI_U8 nss, tANI_U8 *rate)
{
	int32_t ret = 0;

	/* nss input value: 0 - 1x1; 1 - 2x2; 2 - 3x3
	 * the phymode selection is based on following assumption:
	 * (1) if the app specifically requested 1x1 or 2x2 we hornor it
	 * (2) if mbpsx10_rate <= 540: always use BG
	 * (3) 540 < mbpsx10_rate <= 2000: use 1x1 HT/VHT
	 * (4) 2000 < mbpsx10_rate: use 2x2 HT/VHT
	 */
	WMA_LOGE("%s: Input: nss = %d, chanmode = %d, "
		"mbpsx10 = 0x%x, chwidth = %d, shortgi = %d",
		__func__, nss, chanmode, mbpsx10_rate, chwidth, shortgi);
	if ((mbpsx10_rate & 0x40000000) && nss > 0) {
		/* bit 30 indicates user inputed nss,
		 * bit 28 and 29 used to encode nss */
		tANI_U8 user_nss = (mbpsx10_rate & 0x30000000) >> 28;

		nss = (user_nss < nss) ? user_nss : nss;
		/* zero out bits 19 - 21 to recover the actual rate */
		mbpsx10_rate &= ~0x70000000;
	} else if (mbpsx10_rate <= WMA_MCAST_1X1_CUT_OFF_RATE) {
		/* if the input rate is less or equal to the
		 * 1x1 cutoff rate we use 1x1 only */
		nss = 0;
	}
	/* encode NSS bits (bit 4, bit 5) */
	*rate = (nss & 0x3) << 4;
	/* if mcast input rate exceeds the ofdm/cck max rate 54mpbs
	 * we try to choose best ht/vht mcs rate */
	if (540 < mbpsx10_rate) {
		/* cannot use ofdm/cck, choose closest ht/vht mcs rate */
		tANI_U8 rate_ht = *rate;
		tANI_U8 rate_vht = *rate;
		tANI_S32 stream_rate_ht = 0;
		tANI_S32 stream_rate_vht = 0;
		tANI_S32 stream_rate = 0;

		ret = wma_fill_ht_mcast_rate(shortgi, chwidth, mbpsx10_rate,
				  nss, chanmode, &rate_ht, &stream_rate_ht);
		if (ret != VOS_STATUS_SUCCESS) {
			stream_rate_ht = 0;
		}
		if (mhz < WMA_2_4_GHZ_MAX_FREQ) {
			/* not in 5 GHZ frequency */
			*rate = rate_ht;
			stream_rate = stream_rate_ht;
			goto ht_vht_done;
		}
		/* capable doing 11AC mcast so that search vht tables */
		ret = wma_fill_vht_mcast_rate(shortgi, chwidth, mbpsx10_rate,
				nss, chanmode, &rate_vht, &stream_rate_vht);
		if (ret != VOS_STATUS_SUCCESS) {
			if (stream_rate_ht != 0)
				ret = VOS_STATUS_SUCCESS;
			*rate = rate_ht;
			stream_rate = stream_rate_ht;
			goto ht_vht_done;
		}
		if (stream_rate_ht == 0) {
			/* only vht rate available */
			*rate = rate_vht;
			stream_rate = stream_rate_vht;
		} else {
			/* set ht as default first */
			*rate = rate_ht;
			stream_rate = stream_rate_ht;
			if (stream_rate < mbpsx10_rate) {
				if (mbpsx10_rate <= stream_rate_vht ||
					stream_rate < stream_rate_vht) {
					*rate = rate_vht;
					stream_rate = stream_rate_vht;
				}
			} else {
				if (stream_rate_vht >= mbpsx10_rate &&
					stream_rate_vht < stream_rate) {
					*rate = rate_vht;
					stream_rate = stream_rate_vht;
				}
			}
		}
ht_vht_done:
		WMA_LOGE("%s: NSS = %d, ucast_chanmode = %d, "
				"freq = %d, input_rate = %d, chwidth = %d "
				"rate = 0x%x, streaming_rate = %d",
				__func__, nss, chanmode, mhz,
				mbpsx10_rate, chwidth, *rate, stream_rate);
	} else {
		if (mbpsx10_rate > 0)
			ret = wma_fill_ofdm_cck_mcast_rate(mbpsx10_rate,
					nss, rate);
		else
			*rate = 0xFF;
		WMA_LOGE("%s: NSS = %d, ucast_chanmode = %d, "
				"input_rate = %d, rate = 0x%x",
				__func__, nss, chanmode, mbpsx10_rate, *rate);
	}
	return ret;
}
/*
 * FUNCTION: wma_process_rate_update_indate
 *
 */
VOS_STATUS wma_process_rate_update_indicate(tp_wma_handle wma,
	tSirRateUpdateInd *pRateUpdateParams)
{
	int32_t ret = 0;
	u_int8_t vdev_id = 0;
	void *pdev;
	tANI_S32 mbpsx10_rate = -1;
	tANI_U32 paramId;
	tANI_U8 rate = 0;
	u_int32_t short_gi;
	struct wma_txrx_node *intr = wma->interfaces;

	/* Get the vdev id */
	pdev = wma_find_vdev_by_addr(wma, pRateUpdateParams->bssid, &vdev_id);
	if (!pdev) {
		WMA_LOGE("vdev handle is invalid for %pM", pRateUpdateParams->bssid);
		vos_mem_free(pRateUpdateParams);
		return VOS_STATUS_E_INVAL;
	}
	short_gi = intr[vdev_id].config.shortgi;
	if (short_gi == 0)
		short_gi = (intr[vdev_id].rate_flags & eHAL_TX_RATE_SGI) ? TRUE : FALSE;
	/* first check if reliable TX mcast rate is used. If not check the bcast.
	 * Then is mcast. Mcast rate is saved in mcastDataRate24GHz */
	if (pRateUpdateParams->reliableMcastDataRateTxFlag > 0) {
		mbpsx10_rate = pRateUpdateParams->reliableMcastDataRate;
		paramId = WMI_VDEV_PARAM_MCAST_DATA_RATE;
		if (pRateUpdateParams->reliableMcastDataRateTxFlag & eHAL_TX_RATE_SGI)
			short_gi = 1; /* upper layer specified short GI */
	} else if (pRateUpdateParams->bcastDataRate > -1) {
		mbpsx10_rate = pRateUpdateParams->bcastDataRate;
		paramId = WMI_VDEV_PARAM_BCAST_DATA_RATE;
	} else {
		mbpsx10_rate = pRateUpdateParams->mcastDataRate24GHz;
		paramId = WMI_VDEV_PARAM_MCAST_DATA_RATE;
		if (pRateUpdateParams->mcastDataRate24GHzTxFlag & eHAL_TX_RATE_SGI)
			short_gi = 1; /* upper layer specified short GI */
	}
	WMA_LOGE("%s: dev_id = %d, dev_type = %d, dev_mode = %d, "
		"mac = %pM, config.shortgi = %d, rate_flags = 0x%x",
		__func__, vdev_id, intr[vdev_id].type,
		pRateUpdateParams->dev_mode, pRateUpdateParams->bssid,
		intr[vdev_id].config.shortgi, intr[vdev_id].rate_flags);
	ret = wma_encode_mc_rate(short_gi, intr[vdev_id].config.chwidth,
			intr[vdev_id].chanmode, intr[vdev_id].mhz,
			mbpsx10_rate, pRateUpdateParams->nss, &rate);
	if (ret != VOS_STATUS_SUCCESS) {
		WMA_LOGE("%s: Error, Invalid input rate value", __func__);
		vos_mem_free(pRateUpdateParams);
		return ret;
	}
	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
			WMI_VDEV_PARAM_SGI, short_gi);
	if (ret) {
		WMA_LOGE("%s: Failed to Set WMI_VDEV_PARAM_SGI (%d), ret = %d",
			__func__, short_gi, ret);
	        vos_mem_free(pRateUpdateParams);
		return VOS_STATUS_E_FAILURE;
	}
	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle,
			vdev_id, paramId, rate);
	vos_mem_free(pRateUpdateParams);
	if (ret) {
		WMA_LOGE("%s: Failed to Set rate, ret = %d", __func__, ret);
		return VOS_STATUS_E_FAILURE;
	}

	return VOS_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_11AC
static void wma_process_update_membership(tp_wma_handle wma_handle,
                                tUpdateMembership *membership)
{
        WMA_LOGD("%s: membership = %x ", __func__,
                  membership->membership);

        wma_set_peer_param(wma_handle, membership->peer_mac,
                           WMI_PEER_MEMBERSHIP, membership->membership,
                           membership->smesessionId);
}

static void wma_process_update_userpos(tp_wma_handle wma_handle,
                                tUpdateUserPos *userpos)
{
        WMA_LOGD("%s: userPos = %x ", __func__, userpos->userPos);

        wma_set_peer_param(wma_handle, userpos->peer_mac,
                           WMI_PEER_USERPOS, userpos->userPos,
                           userpos->smesessionId);

       /* Now that membership/userpos is updated in fw,
        * enable GID PPS.
        */
        wma_set_ppsconfig(userpos->smesessionId,
                          WMA_VHT_PPS_GID_MATCH, 1);

}
#endif

/* function   : wma_process_init_thermal_info
 * Description : This function initializes the thermal management table in WMA,
                sends down the initial temperature thresholds to the firmware and
                configures the throttle period in the tx rx module
 * Args       :
                wma            : Pointer to WMA handle
 *              pThermalParams : Pointer to thermal mitigation parameters
 * Returns    :
 *              VOS_STATUS_SUCCESS for success otherwise failure
 */
VOS_STATUS wma_process_init_thermal_info(tp_wma_handle wma,
					t_thermal_mgmt *pThermalParams)
{
	t_thermal_cmd_params thermal_params;
	ol_txrx_pdev_handle curr_pdev;

	if (NULL == wma || NULL == pThermalParams) {
		WMA_LOGE("TM Invalid input");
		return VOS_STATUS_E_FAILURE;
	}

	curr_pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);
	if (NULL == curr_pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("TM enable %d period %d", pThermalParams->thermalMgmtEnabled,
			 pThermalParams->throttlePeriod);

	wma->thermal_mgmt_info.thermalMgmtEnabled =
		pThermalParams->thermalMgmtEnabled;
	wma->thermal_mgmt_info.thermalLevels[0].minTempThreshold =
		pThermalParams->thermalLevels[0].minTempThreshold;
	wma->thermal_mgmt_info.thermalLevels[0].maxTempThreshold =
		pThermalParams->thermalLevels[0].maxTempThreshold;
	wma->thermal_mgmt_info.thermalLevels[1].minTempThreshold =
		pThermalParams->thermalLevels[1].minTempThreshold;
	wma->thermal_mgmt_info.thermalLevels[1].maxTempThreshold =
		pThermalParams->thermalLevels[1].maxTempThreshold;
	wma->thermal_mgmt_info.thermalLevels[2].minTempThreshold =
		pThermalParams->thermalLevels[2].minTempThreshold;
	wma->thermal_mgmt_info.thermalLevels[2].maxTempThreshold =
		pThermalParams->thermalLevels[2].maxTempThreshold;
	wma->thermal_mgmt_info.thermalLevels[3].minTempThreshold =
		pThermalParams->thermalLevels[3].minTempThreshold;
	wma->thermal_mgmt_info.thermalLevels[3].maxTempThreshold =
		pThermalParams->thermalLevels[3].maxTempThreshold;
	wma->thermal_mgmt_info.thermalCurrLevel = WLAN_WMA_THERMAL_LEVEL_0;

	WMA_LOGD("TM level min max:\n"
			 "0 %d   %d\n"
			 "1 %d   %d\n"
			 "2 %d   %d\n"
			 "3 %d   %d",
			 wma->thermal_mgmt_info.thermalLevels[0].minTempThreshold,
			 wma->thermal_mgmt_info.thermalLevels[0].maxTempThreshold,
			 wma->thermal_mgmt_info.thermalLevels[1].minTempThreshold,
			 wma->thermal_mgmt_info.thermalLevels[1].maxTempThreshold,
			 wma->thermal_mgmt_info.thermalLevels[2].minTempThreshold,
			 wma->thermal_mgmt_info.thermalLevels[2].maxTempThreshold,
			 wma->thermal_mgmt_info.thermalLevels[3].minTempThreshold,
			 wma->thermal_mgmt_info.thermalLevels[3].maxTempThreshold);

	if (wma->thermal_mgmt_info.thermalMgmtEnabled)
	{
		ol_tx_throttle_init_period(curr_pdev, pThermalParams->throttlePeriod);

		/* Get the temperature thresholds to set in firmware */
		thermal_params.minTemp = wma->thermal_mgmt_info.
			thermalLevels[WLAN_WMA_THERMAL_LEVEL_0].minTempThreshold;
		thermal_params.maxTemp = wma->thermal_mgmt_info.
			thermalLevels[WLAN_WMA_THERMAL_LEVEL_0].maxTempThreshold;
		thermal_params.thermalEnable =
			wma->thermal_mgmt_info.thermalMgmtEnabled;

		WMA_LOGE("TM sending the following to firmware: min %d max %d enable %d",
			thermal_params.minTemp, thermal_params.maxTemp,
				 thermal_params.thermalEnable);

		if(VOS_STATUS_SUCCESS != wma_set_thermal_mgmt(wma, thermal_params))
		{
			WMA_LOGE("Could not send thermal mgmt command to the firmware!");
		}
	}
        return VOS_STATUS_SUCCESS;
}


/**
 * wma_process_set_thermal_level() - Sets new thermal throttle level
 * wma:			Pointer to wma handle
 * thermal_level:	Thermal level to set
 *
 * This function sets new thermal throttle level in the txrx module and sends
 * down the corresponding temperature thresholds to the firmware.
 *
 * Return: VOS_STATUS_SUCCESS for success otherwise failure.
 *
 */
VOS_STATUS wma_process_set_thermal_level(tp_wma_handle wma,
					 uint8_t thermal_level)
{
	ol_txrx_pdev_handle curr_pdev;


	if (NULL == wma) {
		WMA_LOGE("TM Invalid input");
		return VOS_STATUS_E_FAILURE;
	}


	curr_pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);
	if (NULL == curr_pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGE("TM set level %d", thermal_level);

	/* Check if thermal mitigation is enabled */
	if (!wma->thermal_mgmt_info.thermalMgmtEnabled) {
		WMA_LOGE("Thermal mgmt is not enabled, ignoring set level cmd");
		return VOS_STATUS_E_FAILURE;
	}

	if (thermal_level >= WLAN_WMA_MAX_THERMAL_LEVELS) {
		WMA_LOGE("Invalid thermal level set %d", thermal_level);
		return VOS_STATUS_E_FAILURE;
	}

	if (thermal_level == wma->thermal_mgmt_info.thermalCurrLevel) {
		WMA_LOGD("Current level %d is same as the set level, ignoring",
			 wma->thermal_mgmt_info.thermalCurrLevel);
		return VOS_STATUS_SUCCESS;
	}

	wma->thermal_mgmt_info.thermalCurrLevel = thermal_level;

	ol_tx_throttle_set_level(curr_pdev, thermal_level);

	return VOS_STATUS_SUCCESS;
}

/* function     :   wma_ProcessTxPowerLimits
 * Description  :   This function sends the power limits for 2g/5g to firmware
 * Args         :
		    handle     : Pointer to WMA handle
 *                  ptxlim     : Pointer to  power limit values
 * Returns      :   VOS_STATUS based on values sent to firmware
 *
 */
VOS_STATUS wma_ProcessTxPowerLimits(WMA_HANDLE handle,
			tSirTxPowerLimit *ptxlim)
{
	tp_wma_handle wma = (tp_wma_handle)handle;
	int32_t ret = 0;
	u_int32_t txpower_params2g = 0;
	u_int32_t txpower_params5g = 0;

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue tx power limit",
			 __func__);
		return VOS_STATUS_E_INVAL;
        }
	/* Set value and reason code for 2g and 5g power limit */

	SET_PDEV_PARAM_TXPOWER_REASON(txpower_params2g,
			WMI_PDEV_PARAM_TXPOWER_REASON_SAR);
	SET_PDEV_PARAM_TXPOWER_VALUE(txpower_params2g,
			ptxlim->txPower2g);

	SET_PDEV_PARAM_TXPOWER_REASON(txpower_params5g,
			WMI_PDEV_PARAM_TXPOWER_REASON_SAR);
	SET_PDEV_PARAM_TXPOWER_VALUE(txpower_params5g,
			ptxlim->txPower5g);

	WMA_LOGD("%s: txpower2g: %x txpower5g: %x",
			__func__, txpower_params2g, txpower_params5g);

	ret = wmi_unified_pdev_set_param(wma->wmi_handle,
			WMI_PDEV_PARAM_TXPOWER_LIMIT2G, txpower_params2g);
	if (ret) {
		WMA_LOGE("%s: Failed to set txpower 2g (%d)",
			__func__, ret);
		return VOS_STATUS_E_FAILURE;
	}
	ret = wmi_unified_pdev_set_param(wma->wmi_handle,
			WMI_PDEV_PARAM_TXPOWER_LIMIT5G, txpower_params5g);
	if (ret) {
		WMA_LOGE("%s: Failed to set txpower 5g (%d)",
				 __func__, ret);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

/*
 * FUNCTION: wma_ProcessAddPeriodicTxPtrnInd
 * WMI command sent to firmware to add patterns
 * for the corresponding vdev id
 */
VOS_STATUS wma_ProcessAddPeriodicTxPtrnInd(WMA_HANDLE handle,
			tSirAddPeriodicTxPtrn *pAddPeriodicTxPtrnParams)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param* cmd;
	wmi_buf_t wmi_buf;
	uint32_t   len;
	uint8_t vdev_id;
	u_int8_t *buf_ptr;
	u_int32_t ptrn_len, ptrn_len_aligned;
	int j;

	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue fw add pattern cmd",
			__func__);
		return VOS_STATUS_E_INVAL;
	}
	ptrn_len = pAddPeriodicTxPtrnParams->ucPtrnSize;
	ptrn_len_aligned = roundup(ptrn_len, sizeof(uint32_t));
	len  = sizeof(WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param) +
			WMI_TLV_HDR_SIZE + ptrn_len_aligned;

	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	if (!wma_find_vdev_by_addr(wma_handle,
		pAddPeriodicTxPtrnParams->macAddress, &vdev_id)) {
		WMA_LOGE("%s: Failed to find vdev id for %pM",__func__,
		pAddPeriodicTxPtrnParams->macAddress);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_INVAL;
	}
	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_buf);

	cmd = (WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param *)buf_ptr;
		WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param));

	/* Pass the pattern id to delete for the corresponding vdev id */
	cmd->vdev_id = vdev_id;
	cmd->pattern_id = pAddPeriodicTxPtrnParams->ucPtrnId;
	cmd->timeout = pAddPeriodicTxPtrnParams->usPtrnIntervalMs;
	cmd->length = pAddPeriodicTxPtrnParams->ucPtrnSize;

	/* Pattern info */
	buf_ptr += sizeof(WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, ptrn_len_aligned);
	buf_ptr += WMI_TLV_HDR_SIZE;
	vos_mem_copy(buf_ptr, pAddPeriodicTxPtrnParams->ucPattern,
			ptrn_len);
	for (j = 0; j < pAddPeriodicTxPtrnParams->ucPtrnSize; j++) {
		WMA_LOGD("%s: Add Ptrn: %02x", __func__, buf_ptr[j] & 0xff);
	}
	WMA_LOGD("%s: Add ptrn id: %d vdev_id: %d",
			__func__, cmd->pattern_id, cmd->vdev_id);

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, len,
		WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMDID)) {
		WMA_LOGE("%s: failed to add pattern set state command", __func__);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

/*
 * FUNCTION: wma_ProcessDelPeriodicTxPtrnInd
 * WMI command sent to firmware to del patterns
 * for the corresponding vdev id
 */
VOS_STATUS wma_ProcessDelPeriodicTxPtrnInd(WMA_HANDLE handle,
			tSirDelPeriodicTxPtrn *pDelPeriodicTxPtrnParams)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param* cmd;
	wmi_buf_t wmi_buf;
	uint8_t vdev_id;
	u_int32_t len = sizeof(WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param);

	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue Del Pattern cmd",
			 __func__);
	return VOS_STATUS_E_INVAL;
	}
	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	if (!wma_find_vdev_by_addr(wma_handle,
		pDelPeriodicTxPtrnParams->macAddress, &vdev_id)) {
		WMA_LOGE("%s: Failed to find vdev id for %pM",__func__,
		pDelPeriodicTxPtrnParams->macAddress);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_INVAL;
	}
	cmd = (WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param *)wmi_buf_data(wmi_buf);
		WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMD_fixed_param));

	/* Pass the pattern id to delete for the corresponding vdev id */
	cmd->vdev_id = vdev_id;
	cmd->pattern_id = pDelPeriodicTxPtrnParams->ucPtrnId;
	WMA_LOGD("%s: Del ptrn id: %d vdev_id: %d",
			__func__, cmd->pattern_id, cmd->vdev_id);

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, len,
		WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMDID)) {
		WMA_LOGE("%s: failed to send del pattern command", __func__);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

static void wma_set_p2pgo_noa_Req(tp_wma_handle wma,
						tP2pPsParams *noa)
{
	wmi_p2p_set_noa_cmd_fixed_param *cmd;
	wmi_p2p_noa_descriptor *noa_discriptor;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	u_int16_t len;
	int32_t status;
	u_int32_t duration;

	WMA_LOGD("%s: Enter", __func__);
	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE + sizeof(*noa_discriptor);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("Failed to allocate memory");
		goto end;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_p2p_set_noa_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_p2p_set_noa_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_p2p_set_noa_cmd_fixed_param));
	duration = (noa->count == 1)? noa->single_noa_duration : noa->duration;
	cmd->vdev_id = noa->sessionId;
	cmd->enable = (duration)? true : false;
	cmd->num_noa = 1;

	WMITLV_SET_HDR((buf_ptr + sizeof(wmi_p2p_set_noa_cmd_fixed_param)),
				WMITLV_TAG_ARRAY_STRUC,
				sizeof(wmi_p2p_noa_descriptor));
	noa_discriptor = (wmi_p2p_noa_descriptor *) (buf_ptr +
				sizeof(wmi_p2p_set_noa_cmd_fixed_param) +
				WMI_TLV_HDR_SIZE);
	WMITLV_SET_HDR(&noa_discriptor->tlv_header,
		       WMITLV_TAG_STRUC_wmi_p2p_noa_descriptor,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_p2p_noa_descriptor));
	noa_discriptor->type_count = noa->count;
	noa_discriptor->duration = duration;
	noa_discriptor->interval = noa->interval;
	noa_discriptor->start_time = 0;

	WMA_LOGI("SET P2P GO NOA:vdev_id:%d count:%d duration:%d interval:%d",
			cmd->vdev_id, noa->count, noa_discriptor->duration,
			noa->interval);
	status = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
			WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID);
	if (status != EOK) {
		WMA_LOGE("Failed to send WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID");
		wmi_buf_free(buf);
	}

end:
	WMA_LOGD("%s: Exit", __func__);
}

static void wma_set_p2pgo_oppps_req(tp_wma_handle wma,
						tP2pPsParams *oppps)
{
	wmi_p2p_set_oppps_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int32_t status;

	WMA_LOGD("%s: Enter", __func__);
	buf = wmi_buf_alloc(wma->wmi_handle, sizeof(*cmd));
	if (!buf) {
		WMA_LOGE("Failed to allocate memory");
		goto end;
	}

	cmd = (wmi_p2p_set_oppps_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_p2p_set_oppps_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_p2p_set_oppps_cmd_fixed_param));
	cmd->vdev_id = oppps->sessionId;
	if (oppps->ctWindow)
		WMI_UNIFIED_OPPPS_ATTR_ENABLED_SET(cmd);

	WMI_UNIFIED_OPPPS_ATTR_CTWIN_SET(cmd, oppps->ctWindow);
	WMA_LOGI("SET P2P GO OPPPS:vdev_id:%d ctwindow:%d",
			cmd->vdev_id, oppps->ctWindow);
	status = wmi_unified_cmd_send(wma->wmi_handle, buf, sizeof(*cmd),
			WMI_P2P_SET_OPPPS_PARAM_CMDID);
	if (status != EOK) {
		WMA_LOGE("Failed to send WMI_P2P_SET_OPPPS_PARAM_CMDID");
		wmi_buf_free(buf);
	}

end:
	WMA_LOGD("%s: Exit", __func__);
}

static void wma_process_set_p2pgo_noa_Req(tp_wma_handle wma,
						tP2pPsParams *ps_params)
{
	WMA_LOGD("%s: Enter", __func__);
	if (ps_params->opp_ps) {
		wma_set_p2pgo_oppps_req(wma, ps_params);
	} else {
		wma_set_p2pgo_noa_Req(wma, ps_params);
	}

	WMA_LOGD("%s: Exit", __func__);
}

/* function   : wma_process_set_mimops_req
 * Description : Set the received MiMo PS state to firmware.
 * Args       :
                wma_handle  : Pointer to WMA handle
 *              tSetMIMOPS  : Pointer to MiMo PS struct
 * Returns    :
 */
static void wma_process_set_mimops_req(tp_wma_handle wma_handle,
					tSetMIMOPS *mimops)
{
	/* Translate to what firmware understands */
	if ( mimops->htMIMOPSState == eSIR_HT_MIMO_PS_DYNAMIC)
		mimops->htMIMOPSState = WMI_PEER_MIMO_PS_DYNAMIC;
	else if ( mimops->htMIMOPSState == eSIR_HT_MIMO_PS_STATIC)
		mimops->htMIMOPSState = WMI_PEER_MIMO_PS_STATIC;
	else if ( mimops->htMIMOPSState == eSIR_HT_MIMO_PS_NO_LIMIT)
		mimops->htMIMOPSState = WMI_PEER_MIMO_PS_NONE;

	WMA_LOGD("%s: htMIMOPSState = %d, sessionId = %d \
		peerMac <%02x:%02x:%02x:%02x:%02x:%02x>", __func__,
		mimops->htMIMOPSState, mimops->sessionId, mimops->peerMac[0],
		mimops->peerMac[1], mimops->peerMac[2], mimops->peerMac[3],
		mimops->peerMac[4], mimops->peerMac[5]);

	wma_set_peer_param(wma_handle, mimops->peerMac,
			WMI_PEER_MIMO_PS_STATE, mimops->htMIMOPSState,
			mimops->sessionId);
}

/* function   : wma_set_vdev_intrabss_fwd
 * Description : Set intra_fwd value to wni_in.
 * Args       :
 *             wma_handle  : Pointer to WMA handle
 *             pdis_intra_fwd  : Pointer to DisableIntraBssFwd struct
 * Returns    :
 */
static void wma_set_vdev_intrabss_fwd(tp_wma_handle wma_handle,
		tpDisableIntraBssFwd pdis_intra_fwd)
{
	ol_txrx_vdev_handle txrx_vdev;
	WMA_LOGD("%s:intra_fwd:vdev(%d) intrabss_dis=%s",
	__func__, pdis_intra_fwd->sessionId,
	(pdis_intra_fwd->disableintrabssfwd ? "true" : "false"));

	txrx_vdev = wma_handle->interfaces[pdis_intra_fwd->sessionId].handle;
	wdi_in_vdev_rx_fwd_disabled(txrx_vdev, pdis_intra_fwd->disableintrabssfwd);
}

VOS_STATUS wma_notify_modem_power_state(void *wda_handle,
			tSirModemPowerStateInd *pReq)
{
	int32_t ret;
	tp_wma_handle wma = (tp_wma_handle)wda_handle;

	WMA_LOGD("%s: WMA Notify Modem Power State %d", __func__, pReq->param);

	ret = wmi_unified_modem_power_state(wma->wmi_handle, pReq->param);
	if (ret) {
		WMA_LOGE("%s: Fail to notify Modem Power State %d",
		 __func__, pReq->param);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("Successfully notify Modem Power State %d", pReq->param);
	return VOS_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_STATS_EXT
VOS_STATUS wma_stats_ext_req(void *wda_handle,
			     tpStatsExtRequest preq)
{
	int32_t ret;
	tp_wma_handle wma = (tp_wma_handle)wda_handle;
	wmi_req_stats_ext_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	u_int16_t len;
	u_int8_t *buf_ptr;

	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE +
		preq->request_data_len;

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
		return -ENOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_req_stats_ext_cmd_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_req_stats_ext_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_req_stats_ext_cmd_fixed_param));
	cmd->vdev_id = preq->vdev_id;
	cmd->data_len = preq->request_data_len;

	WMA_LOGD("%s: The data len value is %u and vdev id set is %u ",
		 __func__, preq->request_data_len, preq->vdev_id);

	buf_ptr += sizeof(wmi_req_stats_ext_cmd_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, cmd->data_len);

	buf_ptr += WMI_TLV_HDR_SIZE;
	vos_mem_copy(buf_ptr, preq->request_data,
		     cmd->data_len);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_REQUEST_STATS_EXT_CMDID);
	if (ret != EOK) {
		WMA_LOGE("%s: Failed to send notify cmd ret = %d", __func__, ret);
		wmi_buf_free(buf);
	}

	return ret;
}

#endif

void wma_hidden_ssid_vdev_restart(tp_wma_handle wma_handle,
                        tHalHiddenSsidVdevRestart *pReq)
{
        struct wma_txrx_node *intr = wma_handle->interfaces;

        if ((pReq->sessionId  != intr[pReq->sessionId].vdev_restart_params.vdev_id) ||
		!((intr[pReq->sessionId].type == WMI_VDEV_TYPE_AP) &&
		(intr[pReq->sessionId].sub_type == 0)))
        {
                WMA_LOGE("%s : invalid session id", __func__);
                return;
        }

        intr[pReq->sessionId].vdev_restart_params.ssidHidden = pReq->ssidHidden;
        adf_os_atomic_set(&intr[pReq->sessionId].vdev_restart_params.hidden_ssid_restart_in_progress,1);

        /* vdev stop -> vdev restart -> vdev up */
	WMA_LOGD("%s, vdev_id: %d, pausing tx_ll_queue for VDEV_STOP",
		 __func__, pReq->sessionId);
	wdi_in_vdev_pause(wma_handle->interfaces[pReq->sessionId].handle,
			  OL_TXQ_PAUSE_REASON_VDEV_STOP);
	wma_handle->interfaces[pReq->sessionId].pause_bitmap |=
						(1 << PAUSE_TYPE_HOST);
        if (wmi_unified_vdev_stop_send(wma_handle->wmi_handle, pReq->sessionId)) {
                WMA_LOGE("%s: %d Failed to send vdev stop",
                         __func__, __LINE__);
                adf_os_atomic_set(&intr[pReq->sessionId].vdev_restart_params.hidden_ssid_restart_in_progress,0);
                return;
        }
}

#ifdef WLAN_FEATURE_LINK_LAYER_STATS
static VOS_STATUS wma_process_ll_stats_clearReq
(
	tp_wma_handle wma,
	const tpSirLLStatsClearReq clearReq
)
{
	wmi_clear_link_stats_cmd_fixed_param *cmd;
	int32_t len;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	int ret;

	if (!clearReq || !wma) {
		WMA_LOGE("%s: input pointer is NULL", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma->wmi_handle, len);

	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	vos_mem_zero(buf_ptr, len);
	cmd = (wmi_clear_link_stats_cmd_fixed_param *) buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_clear_link_stats_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_clear_link_stats_cmd_fixed_param));

	cmd->stop_stats_collection_req = clearReq->stopReq;
	cmd->vdev_id                    = clearReq->staId;
	cmd->stats_clear_req_mask       = clearReq->statsClearReqMask;

	WMI_CHAR_ARRAY_TO_MAC_ADDR(wma->interfaces[clearReq->staId].addr,
						&cmd->peer_macaddr);

	WMA_LOGD("LINK_LAYER_STATS - Clear Request Params");
	WMA_LOGD("StopReq         : %d", cmd->stop_stats_collection_req);
	WMA_LOGD("Vdev Id         : %d", cmd->vdev_id);
	WMA_LOGD("Clear Stat Mask : %d", cmd->stats_clear_req_mask);
	WMA_LOGD("Peer MAC Addr   : %pM", wma->interfaces[clearReq->staId].addr);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_CLEAR_LINK_STATS_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send clear link stats req", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("Clear Link Layer Stats request sent successfully");
	return VOS_STATUS_SUCCESS;
}

static VOS_STATUS wma_process_ll_stats_setReq
(
	tp_wma_handle wma,
	const tpSirLLStatsSetReq setReq
)
{
	wmi_start_link_stats_cmd_fixed_param *cmd;
	int32_t len;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	int ret;

	if (!setReq || !wma) {
		WMA_LOGE("%s: input pointer is NULL", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma->wmi_handle, len);

	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	vos_mem_zero(buf_ptr, len);
	cmd = (wmi_start_link_stats_cmd_fixed_param *) buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_start_link_stats_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_start_link_stats_cmd_fixed_param));

	cmd->mpdu_size_threshold             = setReq->mpduSizeThreshold;
	cmd->aggressive_statistics_gathering = setReq->aggressiveStatisticsGathering;

	WMA_LOGD("LINK_LAYER_STATS - Start/Set Request Params");
	WMA_LOGD("MPDU Size Thresh : %d", cmd->mpdu_size_threshold);
	WMA_LOGD("Aggressive Gather: %d", cmd->aggressive_statistics_gathering);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_START_LINK_STATS_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send set link stats request", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("Set Link Layer Stats request sent successfully");
	return VOS_STATUS_SUCCESS;
}

static VOS_STATUS wma_process_ll_stats_getReq
(
	tp_wma_handle wma,
	const tpSirLLStatsGetReq getReq
)
{
	wmi_request_link_stats_cmd_fixed_param *cmd;
	int32_t len;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	int ret;

	if (!getReq || !wma) {
		WMA_LOGE("%s: input pointer is NULL", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma->wmi_handle, len);

	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	vos_mem_zero(buf_ptr, len);
	cmd = (wmi_request_link_stats_cmd_fixed_param *) buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_request_link_stats_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_request_link_stats_cmd_fixed_param));

	cmd->request_id = getReq->reqId;
	cmd->stats_type = getReq->paramIdMask;
	cmd->vdev_id    = getReq->staId;

	WMI_CHAR_ARRAY_TO_MAC_ADDR(wma->interfaces[getReq->staId].addr,
						&cmd->peer_macaddr);

	WMA_LOGD("LINK_LAYER_STATS - Get Request Params");
	WMA_LOGD("Request ID      : %d", cmd->request_id);
	WMA_LOGD("Stats Type      : %d", cmd->stats_type);
	WMA_LOGD("Vdev ID         : %d", cmd->vdev_id);
	WMA_LOGD("Peer MAC Addr   : %pM", wma->interfaces[getReq->staId].addr);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_REQUEST_LINK_STATS_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send get link stats request", __func__);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("Get Link Layer Stats request sent successfully");
	return VOS_STATUS_SUCCESS;
}

#endif /* WLAN_FEATURE_LINK_LAYER_STATS */

#ifdef FEATURE_WLAN_EXTSCAN
 VOS_STATUS wma_get_buf_extscan_start_cmd(tp_wma_handle wma_handle,
					tSirWifiScanCmdReqParams *pstart,
					wmi_buf_t *buf,
					int *buf_len)
{
	wmi_extscan_start_cmd_fixed_param *cmd;
	wmi_extscan_bucket *dest_blist;
	wmi_extscan_bucket_channel *dest_clist;
	tSirWifiScanBucketSpec *src_bucket = pstart->buckets;
	tSirWifiScanChannelSpec *src_channel = src_bucket->channels;
	tSirWifiScanChannelSpec save_channel[WLAN_EXTSCAN_MAX_CHANNELS];

	u_int8_t *buf_ptr;
	int i, k, count = 0;
	int len = sizeof(*cmd);
	int nbuckets = pstart->numBuckets;
	int nchannels = 0;

	/* These TLV's are are NULL by default */
	u_int32_t ie_len_with_pad = 0;
	int num_ssid = 0;
	int num_bssid = 0;
	int ie_len = 0;

	uint32_t base_period = pstart->basePeriod;

	/* TLV placeholder for ssid_list (NULL) */
	len += WMI_TLV_HDR_SIZE;
	len += num_ssid * sizeof(wmi_ssid);

	/* TLV placeholder for bssid_list (NULL) */
	len += WMI_TLV_HDR_SIZE;
	len += num_bssid * sizeof(wmi_mac_addr);

	/* TLV placeholder for ie_data (NULL) */
	len += WMI_TLV_HDR_SIZE;
	len += ie_len * sizeof(u_int32_t);

	/* TLV placeholder for bucket */
	len += WMI_TLV_HDR_SIZE;
	len += nbuckets * sizeof(wmi_extscan_bucket);

	/* TLV channel placeholder */
	len += WMI_TLV_HDR_SIZE;
	for (i = 0; i < nbuckets; i++) {
		nchannels +=  src_bucket->numChannels;
		src_bucket++;
	}
	WMA_LOGD("%s: Total buckets: %d total #of channels is %d",
		__func__, nbuckets, nchannels);
	len += nchannels * sizeof(wmi_extscan_bucket_channel);
	/* Allocate the memory */
	*buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!*buf) {
		WMA_LOGP("%s: failed to allocate memory"
			" for start extscan cmd",
			__func__);
		return VOS_STATUS_E_NOMEM;
	}
	buf_ptr = (u_int8_t *)wmi_buf_data(*buf);
	cmd = (wmi_extscan_start_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_extscan_start_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_extscan_start_cmd_fixed_param));

	cmd->request_id = pstart->requestId;
	cmd->vdev_id = pstart->sessionId;
	cmd->base_period = pstart->basePeriod;
	cmd->num_buckets = nbuckets;
	cmd->configuration_flags = 0;

	if (pstart->configuration_flags & EXTSCAN_LP_EXTENDED_BATCHING)
		cmd->configuration_flags |= WMI_EXTSCAN_EXTENDED_BATCHING_EN;
	WMA_LOGI("%s: configuration_flags: 0x%x", __func__,
			cmd->configuration_flags);

	cmd->min_rest_time = WMA_EXTSCAN_REST_TIME;
	cmd->max_rest_time = WMA_EXTSCAN_REST_TIME;
	cmd->max_bssids_per_scan_cycle = pstart->maxAPperScan;

	/* The max dwell time is retrieved from the first channel
	 * of the first bucket and kept common for all channels.
	 */
	cmd->min_dwell_time_active = pstart->min_dwell_time_active;
	cmd->max_dwell_time_active = pstart->max_dwell_time_active;
	cmd->min_dwell_time_passive = pstart->min_dwell_time_passive;
	cmd->max_dwell_time_passive = pstart->max_dwell_time_passive;
	cmd->max_bssids_per_scan_cycle = pstart->maxAPperScan;
	cmd->max_table_usage = pstart->report_threshold_percent;
	cmd->report_threshold_num_scans = pstart->report_threshold_num_scans;

	cmd->repeat_probe_time = cmd->max_dwell_time_active /
					WMA_SCAN_NPROBES_DEFAULT;
	cmd->max_scan_time = WMA_EXTSCAN_MAX_SCAN_TIME;
	cmd->probe_delay = 0;
	cmd->probe_spacing_time = 0;
	cmd->idle_time = 0;
	cmd->burst_duration = WMA_EXTSCAN_BURST_DURATION;
	cmd->scan_ctrl_flags = WMI_SCAN_ADD_BCAST_PROBE_REQ |
				WMI_SCAN_ADD_CCK_RATES |
				WMI_SCAN_ADD_OFDM_RATES |
				WMI_SCAN_ADD_SPOOFED_MAC_IN_PROBE_REQ;
	cmd->scan_priority = WMI_SCAN_PRIORITY_HIGH;
	cmd->num_ssids = 0;
	cmd->num_bssid = 0;
	cmd->ie_len = 0;
	cmd->n_probes = (cmd->repeat_probe_time > 0) ?
			cmd->max_dwell_time_active / cmd->repeat_probe_time : 0;

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr,
	       WMITLV_TAG_ARRAY_FIXED_STRUC,
		num_ssid * sizeof(wmi_ssid));
	buf_ptr += WMI_TLV_HDR_SIZE + (num_ssid * sizeof(wmi_ssid));

	WMITLV_SET_HDR(buf_ptr,
	       WMITLV_TAG_ARRAY_FIXED_STRUC,
		num_bssid * sizeof(wmi_mac_addr));
	buf_ptr += WMI_TLV_HDR_SIZE + (num_bssid * sizeof(wmi_mac_addr));

	ie_len_with_pad = 0;
	WMITLV_SET_HDR(buf_ptr,
	       WMITLV_TAG_ARRAY_BYTE,
		ie_len_with_pad);
	buf_ptr += WMI_TLV_HDR_SIZE + ie_len_with_pad;

	WMITLV_SET_HDR(buf_ptr,
			WMITLV_TAG_ARRAY_STRUC,
			nbuckets * sizeof(wmi_extscan_bucket));
	dest_blist = (wmi_extscan_bucket *)
				(buf_ptr + WMI_TLV_HDR_SIZE);
	src_bucket = pstart->buckets;

	/* Retrieve scanning information from each bucket and
	 * channels and send it to the target
	 */
	for (i = 0; i < nbuckets; i++) {
		WMITLV_SET_HDR(dest_blist,
			WMITLV_TAG_STRUC_wmi_extscan_bucket_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_extscan_bucket));

		dest_blist->bucket_id = src_bucket->bucket;
		dest_blist->base_period_multiplier =
				src_bucket->period / base_period;
		dest_blist->min_period = src_bucket->period;
		dest_blist->max_period = src_bucket->max_period;
		dest_blist->exp_backoff = src_bucket->exponent;
		dest_blist->exp_max_step_count = src_bucket->step_count;
		dest_blist->channel_band = src_bucket->band;
		dest_blist->num_channels = src_bucket->numChannels;
		dest_blist->notify_extscan_events = 0;

		if (src_bucket->reportEvents & EXTSCAN_REPORT_EVENTS_EACH_SCAN)
			dest_blist->notify_extscan_events =
					WMI_EXTSCAN_BUCKET_COMPLETED_EVENT;

		if (src_bucket->reportEvents &
				EXTSCAN_REPORT_EVENTS_FULL_RESULTS) {
			dest_blist->forwarding_flags =
				WMI_EXTSCAN_FORWARD_FRAME_TO_HOST;
			dest_blist->notify_extscan_events |=
				WMI_EXTSCAN_BUCKET_COMPLETED_EVENT |
				WMI_EXTSCAN_CYCLE_STARTED_EVENT |
				WMI_EXTSCAN_CYCLE_COMPLETED_EVENT;
		} else {
			dest_blist->forwarding_flags =
				WMI_EXTSCAN_NO_FORWARDING;
		}

		if (src_bucket->reportEvents & EXTSCAN_REPORT_EVENTS_NO_BATCH)
			dest_blist->configuration_flags = 0;
		else
			dest_blist->configuration_flags =
				WMI_EXTSCAN_BUCKET_CACHE_RESULTS;

		WMA_LOGI("%s: ntfy_extscan_events:%u cfg_flags:%u fwd_flags:%u",
			__func__, dest_blist->notify_extscan_events,
			dest_blist->configuration_flags,
			dest_blist->forwarding_flags);

		dest_blist->min_dwell_time_active = src_bucket->min_dwell_time_active;
		dest_blist->max_dwell_time_active = src_bucket->max_dwell_time_active;
		dest_blist->min_dwell_time_passive = src_bucket->min_dwell_time_passive;
		dest_blist->max_dwell_time_passive = src_bucket->max_dwell_time_passive;
		src_channel = src_bucket->channels;

		/* save the channel info to later populate
		 * the channel TLV
		 */
		for (k = 0; k < src_bucket->numChannels; k++) {
			save_channel[count++].channel =
					src_channel->channel;
			src_channel++;
		}
		dest_blist++;
		src_bucket++;
	}
	buf_ptr +=  WMI_TLV_HDR_SIZE +
				(nbuckets * sizeof(wmi_extscan_bucket));
	WMITLV_SET_HDR(buf_ptr,
			WMITLV_TAG_ARRAY_STRUC,
			nchannels * sizeof(wmi_extscan_bucket_channel));
	dest_clist = (wmi_extscan_bucket_channel *)
				(buf_ptr + WMI_TLV_HDR_SIZE);

	/* Active or passive scan is based on the bucket dwell time
	 * and channel specific active,passive scans are not
	 * supported yet
	 */
	for (i = 0; i < nchannels; i++) {
		WMITLV_SET_HDR(dest_clist,
			WMITLV_TAG_STRUC_wmi_extscan_bucket_channel_event_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_extscan_bucket_channel));
		dest_clist->channel = save_channel[i].channel;
		dest_clist++;
	}
	buf_ptr +=  WMI_TLV_HDR_SIZE +
			(nchannels * sizeof(wmi_extscan_bucket_channel));
	*buf_len = len;
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS wma_start_extscan(tp_wma_handle wma,
				tSirWifiScanCmdReqParams *pstart)
{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	wmi_buf_t buf;
	int len;
	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed,can not issue extscan cmd",
		 __func__);
		return VOS_STATUS_E_INVAL;
	}
	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
					WMI_SERVICE_EXTSCAN)) {
		WMA_LOGE("%s: extscan feature bit not enabled",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}
	/* Fill individual elements of extscan request and
	 * TLV for buckets, channel list.
	 */
	vos_status = wma_get_buf_extscan_start_cmd(wma, pstart,
				&buf, &len);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("%s: Failed to get buffer for ext scan cmd",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}
	if (!buf) {
		WMA_LOGE("%s:Failed to get buffer"
			"for current extscan info", __func__);
		return VOS_STATUS_E_FAILURE;
	}
	if (wmi_unified_cmd_send(wma->wmi_handle, buf,
				len, WMI_EXTSCAN_START_CMDID)) {
		WMA_LOGE("%s: failed to send command", __func__);
		adf_nbuf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("Extscan start request sent successfully for vdev %d",
		 pstart->sessionId);

	return VOS_STATUS_SUCCESS;
}

VOS_STATUS wma_stop_extscan(tp_wma_handle wma,
				tSirExtScanStopReqParams *pstopcmd)
{
	wmi_extscan_stop_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint32_t   len;
	u_int8_t *buf_ptr;

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, cannot issue cmd",
			__func__);
		return VOS_STATUS_E_INVAL;
	}
	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
					WMI_SERVICE_EXTSCAN)) {
		WMA_LOGE("%s: extscan not enabled",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}
	len  = sizeof(*cmd);
	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_buf);
	cmd = (wmi_extscan_stop_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_extscan_stop_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_extscan_stop_cmd_fixed_param));

	cmd->request_id = pstopcmd->requestId;
	cmd->vdev_id = pstopcmd->sessionId;

	if (wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, len,
		WMI_EXTSCAN_STOP_CMDID)) {
		WMA_LOGE("%s: failed to  command", __func__);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("Extscan stop request sent successfully for vdev %d",
		 pstopcmd->sessionId);

	return VOS_STATUS_SUCCESS;
}

/** wma_get_hotlist_entries_per_page() - hotlist entries per page
 * @cmd: size of command structure.
 * @per_entry_size: per entry size.
 *
 * This utility function calculates how many hotlist entries can
 * fit in one page.
 *
 * Return: number of entries
 */
static inline int wma_get_hotlist_entries_per_page(wmi_unified_t wmi_handle,
						   size_t cmd_size,
						   size_t per_entry_size)
{
	uint32_t avail_space = 0;
	int num_entries = 0;
	uint16_t max_msg_len = wmi_get_max_msg_len(wmi_handle);

	/* Calculate number of hotlist entries that can
	 * be passed in wma message request.
	 */
	avail_space = max_msg_len - cmd_size;
	num_entries = avail_space / per_entry_size;
	return num_entries;
}

/** wma_get_buf_extscan_hotlist_cmd() - extscan hotlist command
 * @wma_handle: pointer to WMA handle
 * @photlist: pointer to input hotlist request message
 * @buf_len: buffer len
 *
 * This function constructs the WMA set bssid hotlist command message
 * and based on the maximum length of the WMA command buf, it issues
 * multiple request.
 *
 * Return: VOS_STATUS enumeration.
 */
VOS_STATUS wma_get_buf_extscan_hotlist_cmd(tp_wma_handle wma_handle,
			tSirExtScanSetBssidHotListReqParams *photlist,
			int *buf_len)
{
	wmi_extscan_configure_hotlist_monitor_cmd_fixed_param *cmd;
	wmi_extscan_hotlist_entry  *dest_hotlist;
	tSirAPThresholdParam  *src_ap = photlist->ap;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;

	int j, index = 0;
	int cmd_len = 0;
	int num_entries = 0;
	int min_entries = 0;
	int numap = photlist->numAp;
	int len = sizeof(*cmd);

	len += WMI_TLV_HDR_SIZE;
	cmd_len = len;

	/* setbssid hotlist expects the bssid list
	 * to be non zero value
	 */
	if (!numap) {
		WMA_LOGE("%s: Invalid number of bssid's", __func__);
		return VOS_STATUS_E_INVAL;
	}
	num_entries = wma_get_hotlist_entries_per_page(wma_handle->wmi_handle,
							cmd_len,
							sizeof(*dest_hotlist));

	/* Split the hot list entry pages and send multiple command
	 * requests if the buffer reaches the maximum request size
	 */
	while (index < numap) {
		min_entries = VOS_MIN(num_entries, numap);
		len += min_entries * sizeof(wmi_extscan_hotlist_entry);
		buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
		if (!buf) {
			WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
			return VOS_STATUS_E_NOMEM;
		}
		buf_ptr = (u_int8_t *)wmi_buf_data(buf);
		cmd = (wmi_extscan_configure_hotlist_monitor_cmd_fixed_param *)
								buf_ptr;
		WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_extscan_configure_hotlist_monitor_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_extscan_configure_hotlist_monitor_cmd_fixed_param));

		/* Multiple requests are sent until the num_entries_in_page
		 * matches the total_entries
		 */
		cmd->request_id = photlist->requestId;
		cmd->vdev_id = photlist->sessionId;
		cmd->total_entries = numap;
		cmd->mode = 1;
		cmd->num_entries_in_page = min_entries;
		cmd->lost_ap_scan_count = photlist->lost_ap_sample_size;
		cmd->first_entry_index = index;

		WMA_LOGD("%s: vdev id:%d total_entries: %d num_entries: %d lost_ap_sample_size: %d",
			__func__, cmd->vdev_id, cmd->total_entries,
			cmd->num_entries_in_page,
			cmd->lost_ap_scan_count);

		buf_ptr += sizeof(*cmd);
		WMITLV_SET_HDR(buf_ptr,
		WMITLV_TAG_ARRAY_STRUC,
			min_entries * sizeof(wmi_extscan_hotlist_entry));
		dest_hotlist = (wmi_extscan_hotlist_entry *)
					(buf_ptr + WMI_TLV_HDR_SIZE);

		/* Populate bssid, channel info and rssi
		 * for the bssid's that are sent as hotlists.
		 */
		for (j = 0; j < min_entries; j++) {
			WMITLV_SET_HDR(dest_hotlist,
				WMITLV_TAG_STRUC_wmi_extscan_bucket_cmd_fixed_param,
				WMITLV_GET_STRUCT_TLVLEN(
					wmi_extscan_hotlist_entry));

			dest_hotlist->min_rssi = src_ap->low;
			WMI_CHAR_ARRAY_TO_MAC_ADDR(src_ap->bssid,
						&dest_hotlist->bssid);

			WMA_LOGD("%s: min_rssi %d", __func__,
				dest_hotlist->min_rssi);
			WMA_LOGD("%s: bssid mac_addr31to0: 0x%x, mac_addr47to32: 0x%x",
				__func__, dest_hotlist->bssid.mac_addr31to0,
				dest_hotlist->bssid.mac_addr47to32);
			dest_hotlist++;
			src_ap++;
		}
		buf_ptr +=  WMI_TLV_HDR_SIZE +
				(min_entries * sizeof(wmi_extscan_hotlist_entry));

		if (wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
					len, WMI_EXTSCAN_CONFIGURE_HOTLIST_MONITOR_CMDID)) {
			WMA_LOGE("%s: failed to send command", __func__);
			adf_nbuf_free(buf);
			return VOS_STATUS_E_FAILURE;
		}
		index = index + min_entries;
		num_entries = numap - min_entries;
		len = cmd_len;
	}
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS wma_extscan_start_hotlist_monitor(tp_wma_handle wma,
			tSirExtScanSetBssidHotListReqParams *photlist)
{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	int len;

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue hotlist cmd",
			__func__);
		return VOS_STATUS_E_INVAL;
	}
	/* Fill individual elements for  hotlist request and
	 * TLV for bssid entries
	 */
	vos_status = wma_get_buf_extscan_hotlist_cmd(wma, photlist,
						&len);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("%s: Failed to get buffer for hotlist scan cmd",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS wma_extscan_stop_hotlist_monitor(tp_wma_handle wma,
			tSirExtScanResetBssidHotlistReqParams *photlist_reset)
{
	wmi_extscan_configure_hotlist_monitor_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint32_t   len;
	u_int8_t *buf_ptr;
	int hotlist_entries = 0;

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue  cmd",
		__func__);
		return VOS_STATUS_E_INVAL;
	}
	if (!photlist_reset) {
		WMA_LOGE("%s: Invalid reset hotlist buffer",
			__func__);
		return VOS_STATUS_E_INVAL;
	}
	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
					WMI_SERVICE_EXTSCAN)) {
		WMA_LOGE("%s: extscan not enabled",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}
	len  = sizeof(*cmd);

	/* reset bssid hotlist with tlv set to 0 */
	len += WMI_TLV_HDR_SIZE;
	len += hotlist_entries * sizeof(wmi_extscan_hotlist_entry);

	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_buf);
	cmd = (wmi_extscan_configure_hotlist_monitor_cmd_fixed_param *)
						buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_extscan_configure_hotlist_monitor_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_extscan_configure_hotlist_monitor_cmd_fixed_param));

	cmd->request_id = photlist_reset->requestId;
	cmd->vdev_id = photlist_reset->sessionId;
	cmd->mode = 0;

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr,
		WMITLV_TAG_ARRAY_STRUC,
		hotlist_entries * sizeof(wmi_extscan_hotlist_entry));
	buf_ptr += WMI_TLV_HDR_SIZE +
			(hotlist_entries * sizeof(wmi_extscan_hotlist_entry));

	if (wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, len,
			WMI_EXTSCAN_CONFIGURE_HOTLIST_MONITOR_CMDID)) {
		WMA_LOGE("%s: failed to  command", __func__);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

/**
 * wma_set_ssid_hotlist() - Handle an SSID hotlist set request
 * @wma: WMA handle
 * @request: SSID hotlist set request from SME
 *
 * Return: VOS_STATUS
 */
static VOS_STATUS
wma_set_ssid_hotlist(tp_wma_handle wma,
		     struct sir_set_ssid_hotlist_request *request)
{
	wmi_extscan_configure_hotlist_ssid_monitor_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint32_t len;
	uint32_t array_size;
	uint8_t *buf_ptr;

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue hotlist cmd",
			 __func__);
		return VOS_STATUS_E_INVAL;
	}
	if (!request) {
		WMA_LOGE("%s: Invalid request buffer",
			__func__);
		return VOS_STATUS_E_INVAL;
	}
	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
				    WMI_SERVICE_EXTSCAN)) {
		WMA_LOGE("%s: extscan not enabled",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}

	/* length of fixed portion */
	len = sizeof(*cmd);

	/* length of variable portion */
	array_size =
		request->ssid_count * sizeof(wmi_extscan_hotlist_ssid_entry);
	len += WMI_TLV_HDR_SIZE + array_size;

	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (uint8_t *) wmi_buf_data(wmi_buf);
	cmd = (wmi_extscan_configure_hotlist_ssid_monitor_cmd_fixed_param *)
						buf_ptr;
	WMITLV_SET_HDR
		(&cmd->tlv_header,
		 WMITLV_TAG_STRUC_wmi_extscan_configure_hotlist_ssid_monitor_cmd_fixed_param,
		 WMITLV_GET_STRUCT_TLVLEN
			(wmi_extscan_configure_hotlist_ssid_monitor_cmd_fixed_param));

	cmd->request_id = request->request_id;
	cmd->requestor_id = 0;
	cmd->vdev_id = request->session_id;
	cmd->table_id = 0;
	cmd->lost_ap_scan_count = request->lost_ssid_sample_size;
	cmd->total_entries = request->ssid_count;
	cmd->num_entries_in_page = request->ssid_count;
	cmd->first_entry_index = 0;

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC, array_size);

	if (request->ssid_count) {
		wmi_extscan_hotlist_ssid_entry *entry;
		int i;

		buf_ptr += WMI_TLV_HDR_SIZE;
		entry = (wmi_extscan_hotlist_ssid_entry *)buf_ptr;
		for (i = 0; i < request->ssid_count; i++) {
			WMITLV_SET_HDR
				(entry,
				 WMITLV_TAG_ARRAY_STRUC,
				 WMITLV_GET_STRUCT_TLVLEN
					(wmi_extscan_hotlist_ssid_entry));
			entry->ssid.ssid_len = request->ssids[i].ssid.length;
			vos_mem_copy(entry->ssid.ssid,
				     request->ssids[i].ssid.ssId,
				     request->ssids[i].ssid.length);
			entry->band = request->ssids[i].band;
			entry->min_rssi = request->ssids[i].rssi_low;
			entry->max_rssi = request->ssids[i].rssi_high;
			entry++;
		}
		cmd->mode = WMI_EXTSCAN_MODE_START;
	} else {
		cmd->mode = WMI_EXTSCAN_MODE_STOP;
	}

	if (wmi_unified_cmd_send
		(wma->wmi_handle, wmi_buf, len,
		 WMI_EXTSCAN_CONFIGURE_HOTLIST_SSID_MONITOR_CMDID)) {
		WMA_LOGE("%s: failed to send command", __func__);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS wma_get_buf_extscan_change_monitor_cmd(tp_wma_handle wma_handle,
			tSirExtScanSetSigChangeReqParams *psigchange,
			wmi_buf_t *buf, int *buf_len)
{
	wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param *cmd;
	wmi_extscan_wlan_change_bssid_param  *dest_chglist;
	u_int8_t *buf_ptr;
	int j;
	int len = sizeof(*cmd);
	int numap = psigchange->numAp;
	tSirAPThresholdParam  *src_ap = psigchange->ap;

	if (!numap) {
		WMA_LOGE("%s: Invalid number of bssid's",
			__func__);
		return VOS_STATUS_E_INVAL;
	}
	len += WMI_TLV_HDR_SIZE;
	len += numap * sizeof(wmi_extscan_wlan_change_bssid_param);

	*buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!*buf) {
		WMA_LOGP("%s: failed to allocate memory for change monitor cmd",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}
	buf_ptr = (u_int8_t *) wmi_buf_data(*buf);
	cmd = (wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param *)buf_ptr;
		WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param));

	cmd->request_id = psigchange->requestId;
	cmd->vdev_id = psigchange->sessionId;
	cmd->total_entries = numap;
	cmd->mode = 1;
	cmd->num_entries_in_page = numap;
	cmd->lost_ap_scan_count = psigchange->lostApSampleSize;
	cmd->max_rssi_samples = psigchange->rssiSampleSize;
	cmd->rssi_averaging_samples = psigchange->rssiSampleSize;
	cmd->max_out_of_range_count = psigchange->minBreaching;

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr,
	       WMITLV_TAG_ARRAY_STRUC,
	       numap * sizeof(wmi_extscan_wlan_change_bssid_param));
	dest_chglist = (wmi_extscan_wlan_change_bssid_param *)
			 (buf_ptr + WMI_TLV_HDR_SIZE);

	for (j = 0; j < numap; j++) {
		WMITLV_SET_HDR(dest_chglist,
			WMITLV_TAG_STRUC_wmi_extscan_bucket_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_extscan_wlan_change_bssid_param));

		dest_chglist->lower_rssi_limit = src_ap->low;
		dest_chglist->upper_rssi_limit = src_ap->high;
		WMI_CHAR_ARRAY_TO_MAC_ADDR(src_ap->bssid,
					&dest_chglist->bssid);

		WMA_LOGD("%s:min_rssi %d", __func__,
			dest_chglist->lower_rssi_limit);
		dest_chglist++;
		src_ap++;
	}
	buf_ptr +=  WMI_TLV_HDR_SIZE +
			(numap * sizeof(wmi_extscan_wlan_change_bssid_param));
	*buf_len = len;
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS wma_extscan_start_change_monitor(tp_wma_handle wma,
			tSirExtScanSetSigChangeReqParams *psigchange)
{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	wmi_buf_t buf;
	int len;

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed,can not issue extscan cmd",
			 __func__);
		return VOS_STATUS_E_INVAL;
	}
	/* Fill individual elements of change monitor and
	* TLV info ... */

	vos_status = wma_get_buf_extscan_change_monitor_cmd(wma,
					psigchange,  &buf, &len);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("%s: Failed to get buffer for change monitor cmd",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}
	if (!buf) {
		WMA_LOGE("%s: Failed to get buffer", __func__);
		return VOS_STATUS_E_FAILURE;
	}
	if (wmi_unified_cmd_send(wma->wmi_handle, buf,
		len, WMI_EXTSCAN_CONFIGURE_WLAN_CHANGE_MONITOR_CMDID)) {
		WMA_LOGE("%s: failed to send command", __func__);
		adf_nbuf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS wma_extscan_stop_change_monitor(tp_wma_handle wma,
			tSirExtScanResetSignificantChangeReqParams *pResetReq)
{
	wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint32_t   len;
	u_int8_t *buf_ptr;
	int change_list = 0;

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue  cmd",
			__func__);
		return VOS_STATUS_E_INVAL;
	}
	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
					WMI_SERVICE_EXTSCAN)) {
		WMA_LOGE("%s: ext scan not enabled",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}
	len  = sizeof(*cmd);

	/* reset significant change tlv is set to 0 */
	len += WMI_TLV_HDR_SIZE;
	len += change_list *
			sizeof(wmi_extscan_wlan_change_bssid_param);
	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_buf);

	cmd = (wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_extscan_configure_wlan_change_monitor_cmd_fixed_param));

	cmd->request_id = pResetReq->requestId;
	cmd->vdev_id = pResetReq->sessionId;
	cmd->mode = 0;

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr,
		 WMITLV_TAG_ARRAY_STRUC,
			change_list *
				sizeof(wmi_extscan_wlan_change_bssid_param));
	buf_ptr += WMI_TLV_HDR_SIZE + (change_list *
					sizeof(wmi_extscan_wlan_change_bssid_param));

	if (wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, len,
			WMI_EXTSCAN_CONFIGURE_WLAN_CHANGE_MONITOR_CMDID)) {
		WMA_LOGE("%s: failed to  command", __func__);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS  wma_extscan_get_cached_results(tp_wma_handle wma,
			tSirExtScanGetCachedResultsReqParams *pcached_results)
{
	wmi_extscan_get_cached_results_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint32_t   len;
	u_int8_t *buf_ptr;

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, cannot issue cmd",
			__func__);
		return VOS_STATUS_E_INVAL;
	}
	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
			WMI_SERVICE_EXTSCAN)) {
		WMA_LOGE("%s: extscan not enabled",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}
	len  = sizeof(*cmd);
	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	buf_ptr = (u_int8_t *)wmi_buf_data(wmi_buf);

	cmd = (wmi_extscan_get_cached_results_cmd_fixed_param *)buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_extscan_get_cached_results_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_extscan_get_cached_results_cmd_fixed_param));

	cmd->request_id = pcached_results->requestId;
	cmd->vdev_id = pcached_results->sessionId;
	cmd->control_flags = pcached_results->flush;

	if (wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, len,
			WMI_EXTSCAN_GET_CACHED_RESULTS_CMDID)) {
		WMA_LOGE("%s: failed to  command", __func__);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS  wma_extscan_get_capabilities(tp_wma_handle wma,
		tSirGetExtScanCapabilitiesReqParams  *pgetcapab)
{
	wmi_extscan_get_capabilities_cmd_fixed_param  *cmd;
	wmi_buf_t wmi_buf;
	uint32_t   len;
	u_int8_t *buf_ptr;

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue  cmd",
			__func__);
		return VOS_STATUS_E_INVAL;
	}
	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
			WMI_SERVICE_EXTSCAN)) {
		WMA_LOGE("%s: extscan not enabled",
			__func__);
		return VOS_STATUS_E_FAILURE;
	}
	len  = sizeof(*cmd);
	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	buf_ptr = (u_int8_t *)wmi_buf_data(wmi_buf);

	cmd = (wmi_extscan_get_capabilities_cmd_fixed_param *)buf_ptr;
		WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_extscan_get_capabilities_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_extscan_get_capabilities_cmd_fixed_param));

	cmd->request_id = pgetcapab->requestId;

	if (wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, len,
		WMI_EXTSCAN_GET_CAPABILITIES_CMDID)) {
		WMA_LOGE("%s: failed to  command", __func__);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

/** wma_set_epno_network_list() - set epno network list
 * @wma: WMA handle
 * @req: epno config params request structure
 *
 * This function reads the incoming epno config request structure
 * and constructs the WMI message to the firmware.
 *
 * Returns: 0 on success, error number otherwise
 */
static int wma_set_epno_network_list(tp_wma_handle wma,
					struct wifi_epno_params *req)
{
	wmi_nlo_config_cmd_fixed_param *cmd;
	nlo_configured_parameters *nlo_list;
	u_int8_t i, *buf_ptr;
	wmi_buf_t buf;
	uint32_t len;
	int ret;

	WMA_LOGD("wma_set_epno_network_list");

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue cmd", __func__);
		return -EINVAL;
	}
	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
			WMI_SERVICE_EXTSCAN)) {
		WMA_LOGE("%s: extscan not enabled", __func__);
		return -EINVAL;
	}

	/* TLV place holder for array of structures
	 * nlo_configured_parameters(nlo_list) */
	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;
	len += sizeof(nlo_configured_parameters) *
				MIN(req->num_networks, WMI_NLO_MAX_SSIDS);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return -ENOMEM;
	}

	cmd = (wmi_nlo_config_cmd_fixed_param *) wmi_buf_data(buf);

	buf_ptr = (u_int8_t *) cmd;
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_nlo_config_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_nlo_config_cmd_fixed_param));
	cmd->vdev_id = req->session_id;
	cmd->flags = WMI_NLO_CONFIG_ENLO;

	buf_ptr += sizeof(wmi_nlo_config_cmd_fixed_param);

	cmd->no_of_ssids = MIN(req->num_networks, WMI_NLO_MAX_SSIDS);
	WMA_LOGD("SSID count: %d", cmd->no_of_ssids);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       cmd->no_of_ssids * sizeof(nlo_configured_parameters));
	buf_ptr += WMI_TLV_HDR_SIZE;

	nlo_list = (nlo_configured_parameters *) buf_ptr;
	for (i = 0; i < cmd->no_of_ssids; i++) {
		WMITLV_SET_HDR(&nlo_list[i].tlv_header,
			WMITLV_TAG_ARRAY_BYTE,
			WMITLV_GET_STRUCT_TLVLEN(nlo_configured_parameters));
		/* Copy ssid and it's length */
		nlo_list[i].ssid.valid = TRUE;
		nlo_list[i].ssid.ssid.ssid_len = req->networks[i].ssid.length;
		vos_mem_copy(nlo_list[i].ssid.ssid.ssid,
			     req->networks[i].ssid.ssId,
			     nlo_list[i].ssid.ssid.ssid_len);
		WMA_LOGD("index: %d ssid: %.*s len: %d", i,
			 nlo_list[i].ssid.ssid.ssid_len,
			 (char *) nlo_list[i].ssid.ssid.ssid,
			 nlo_list[i].ssid.ssid.ssid_len);

		/* Copy rssi threshold */
		nlo_list[i].rssi_cond.valid = TRUE;
		nlo_list[i].rssi_cond.rssi =
			req->networks[i].rssi_threshold;
		WMA_LOGD("RSSI threshold : %d dBm",
			nlo_list[i].rssi_cond.rssi);

		/* Copy pno flags */
		nlo_list[i].bcast_nw_type.valid = TRUE;
		nlo_list[i].bcast_nw_type.bcast_nw_type =
				req->networks[i].flags;
		WMA_LOGD("PNO flags (%u)",
				nlo_list[i].bcast_nw_type.bcast_nw_type);

		/* Copy auth bit field */
		nlo_list[i].auth_type.valid = TRUE;
		nlo_list[i].auth_type.auth_type =
				req->networks[i].auth_bit_field;
		WMA_LOGD("Auth bit field (%u)",
				nlo_list[i].auth_type.auth_type);
	}
	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send nlo wmi cmd", __func__);
		wmi_buf_free(buf);
		return -EINVAL;
	}

	WMA_LOGD("set ePNO list request sent successfully for vdev %d",
		 req->session_id);

	return 0;
}

/**
 * wma_set_passpoint_network_list() - set passpoint network list
 * @handle: WMA handle
 * @req: passpoint network request structure
 *
 * This function reads the incoming @req and fill in the destination
 * WMI structure and send down the passpoint configs down to the firmware
 *
 * Return: 0 on success; error number otherwise
 */
static int wma_set_passpoint_network_list(tp_wma_handle wma,
					struct wifi_passpoint_req *req)
{
	wmi_passpoint_config_cmd_fixed_param *cmd;
	u_int8_t i, j, *bytes;
	wmi_buf_t buf;
	uint32_t len;
	int ret;

	WMA_LOGD("wma_set_passpoint_network_list");

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue cmd", __func__);
		return -EINVAL;
	}
	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
			WMI_SERVICE_EXTSCAN)) {
		WMA_LOGE("%s: extscan not enabled", __func__);
		return -EINVAL;
	}

	len = sizeof(*cmd);
	for (i = 0; i < req->num_networks; i++) {
		buf = wmi_buf_alloc(wma->wmi_handle, len);
		if (!buf) {
			WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
			return -ENOMEM;
		}

		cmd = (wmi_passpoint_config_cmd_fixed_param *)
				wmi_buf_data(buf);

		WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_passpoint_config_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
			wmi_passpoint_config_cmd_fixed_param));
		cmd->id = req->networks[i].id;
		WMA_LOGD("%s: network id: %u", __func__, cmd->id);
		vos_mem_copy(cmd->realm, req->networks[i].realm,
			strlen(req->networks[i].realm) + 1);
		WMA_LOGD("%s: realm: %s", __func__, cmd->realm);
		for (j = 0; j < PASSPOINT_ROAMING_CONSORTIUM_ID_NUM; j++) {
			bytes = (uint8_t *) &req->networks[i].roaming_consortium_ids[j];
			WMA_LOGD("index: %d rcids: %02x %02x %02x %02x %02x %02x %02x %02x",
				j, bytes[0], bytes[1], bytes[2], bytes[3],
				bytes[4], bytes[5], bytes[6], bytes[7]);

			vos_mem_copy(&cmd->roaming_consortium_ids[j],
				&req->networks[i].roaming_consortium_ids[j],
				PASSPOINT_ROAMING_CONSORTIUM_ID_LEN);
		}
		vos_mem_copy(cmd->plmn, req->networks[i].plmn,
				PASSPOINT_PLMN_ID_LEN);
		WMA_LOGD("%s: plmn: [%02x %02x %02x]", __func__,
			cmd->plmn[0], cmd->plmn[1], cmd->plmn[2]);

		ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
					   WMI_PASSPOINT_LIST_CONFIG_CMDID);
		if (ret) {
			WMA_LOGE("%s: Failed to send set passpoint network list wmi cmd",
				 __func__);
			wmi_buf_free(buf);
			return -EINVAL;
		}
	}

	WMA_LOGD("Set passpoint network list request is sent successfully for vdev %d",
		 req->session_id);

	return 0;
}

/**
 * wma_reset_passpoint_network_list() - reset passpoint network list
 * @handle: WMA handle
 * @req: passpoint network request structure
 *
 * This function sends down WMI command with network id set to wildcard id.
 * firmware shall clear all the config entries
 *
 * Return: 0 on success; error number otherwise
 */
static int wma_reset_passpoint_network_list(tp_wma_handle wma,
					struct wifi_passpoint_req *req)
{
	wmi_passpoint_config_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint32_t len;
	int ret;

	WMA_LOGD("wma_reset_passpoint_network_list");

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue cmd", __func__);
		return -EINVAL;
	}
	if (!WMI_SERVICE_IS_ENABLED(wma->wmi_service_bitmap,
			WMI_SERVICE_EXTSCAN)) {
		WMA_LOGE("%s: extscan not enabled", __func__);
		return -EINVAL;
	}

	len = sizeof(*cmd);
	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: Failed allocate wmi buffer", __func__);
		return -ENOMEM;
	}

	cmd = (wmi_passpoint_config_cmd_fixed_param *) wmi_buf_data(buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_passpoint_config_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
			wmi_passpoint_config_cmd_fixed_param));
	cmd->id = WMI_PASSPOINT_NETWORK_ID_WILDCARD;

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_PASSPOINT_LIST_CONFIG_CMDID);
	if (ret) {
		WMA_LOGE("%s: Failed to send reset passpoint network list wmi cmd",
			 __func__);
		wmi_buf_free(buf);
		return -EINVAL;
	}

	WMA_LOGD("Reset passpoint network list request is sent successfully for vdev %d",
		 req->session_id);

	return 0;
}

#endif

#ifdef WLAN_FEATURE_NAN
/* function   : wma_nan_req
 * Descriptin : Function is used to send nan request down
 * Args       : wma_handle, request data which will be non-null
 * Returns    : SUCCESS or FAILURE
 */
static VOS_STATUS wma_nan_req(void *wda_handle, tpNanRequest nan_req)
{
	int ret;
	tp_wma_handle wma_handle = (tp_wma_handle)wda_handle;
	wmi_nan_cmd_param *cmd;
	wmi_buf_t buf;
	u_int16_t len = sizeof(*cmd);
        u_int16_t nan_data_len, nan_data_len_aligned;
	u_int8_t *buf_ptr;

	/*
	 *    <----- cmd ------------><-- WMI_TLV_HDR_SIZE --><--- data ---->
	 *    +------------+----------+-----------------------+--------------+
	 *    | tlv_header | data_len | WMITLV_TAG_ARRAY_BYTE | nan_req_data |
	 *    +------------+----------+-----------------------+--------------+
	 */
	if (!nan_req) {
		WMA_LOGE("%s:nan req is not valid", __func__);
		return VOS_STATUS_E_FAILURE;
	}
	nan_data_len = nan_req->request_data_len;
	nan_data_len_aligned = roundup(nan_req->request_data_len,
				sizeof(u_int32_t));
	len += WMI_TLV_HDR_SIZE + nan_data_len_aligned;
	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_nan_cmd_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_nan_cmd_param,
		WMITLV_GET_STRUCT_TLVLEN(
		wmi_nan_cmd_param));
	cmd->data_len = nan_req->request_data_len;
	WMA_LOGD("%s: The data len value is %u",
		__func__, nan_req->request_data_len);
	buf_ptr += sizeof(wmi_nan_cmd_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, nan_data_len_aligned);
	buf_ptr += WMI_TLV_HDR_SIZE;
	vos_mem_copy(buf_ptr, nan_req->request_data,
		cmd->data_len);

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
			WMI_NAN_CMDID);
	if (ret != EOK) {
	WMA_LOGE("%s Failed to send set param command ret = %d", __func__, ret);
	wmi_buf_free(buf);
	}
	return ret;
}
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void wma_process_unit_test_cmd(WMA_HANDLE handle,
                                      t_wma_unit_test_cmd  *wma_utest)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	wmi_unit_test_cmd_fixed_param* cmd;
	wmi_buf_t wmi_buf;
	u_int8_t *buf_ptr;
	int i;
	u_int16_t len, args_tlv_len;
	A_UINT32 *unit_test_cmd_args;

	args_tlv_len = WMI_TLV_HDR_SIZE + wma_utest->num_args * sizeof(A_UINT32);
	len = sizeof(wmi_unit_test_cmd_fixed_param) + args_tlv_len;
	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue fw unit test cmd",
				__func__);
		return;
	}
	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmai_buf_alloc failed", __func__);
		return;
	}

	cmd = (wmi_unit_test_cmd_fixed_param *)wmi_buf_data(wmi_buf);
	buf_ptr = (u_int8_t *) cmd;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_unit_test_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_unit_test_cmd_fixed_param));
	cmd->vdev_id = wma_utest->vdev_id;
	cmd->module_id = wma_utest->module_id;
	cmd->num_args = wma_utest->num_args;
	buf_ptr += sizeof(wmi_unit_test_cmd_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
			(wma_utest->num_args * sizeof(u_int32_t)));
	unit_test_cmd_args = (A_UINT32 *)(buf_ptr + WMI_TLV_HDR_SIZE);
	WMA_LOGI("%s: %d num of args = ", __func__, wma_utest->num_args);
	for (i = 0; (i < wma_utest->num_args && i < WMA_MAX_NUM_ARGS); i++) {
		unit_test_cmd_args[i] = wma_utest->args[i];
		WMA_LOGI("%d,", wma_utest->args[i]);
	}
	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, len,
				WMI_UNIT_TEST_CMDID)) {
		WMA_LOGP("%s: failed to send unit test command", __func__);
		adf_nbuf_free(wmi_buf);
		return;
	}
	return;
}
#endif

VOS_STATUS  wma_scan_probe_setoui(tp_wma_handle wma,
		tSirScanMacOui *psetoui)
{
	wmi_scan_prob_req_oui_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint32_t   len;
	u_int8_t *buf_ptr;
	u_int32_t *oui_buf;

	if (!wma || !wma->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue  cmd",
			__func__);
		return VOS_STATUS_E_INVAL;
	}
	len  = sizeof(*cmd);
	wmi_buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}
	buf_ptr = (u_int8_t *)wmi_buf_data(wmi_buf);
	cmd = (wmi_scan_prob_req_oui_cmd_fixed_param *)buf_ptr;
		WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_scan_prob_req_oui_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_scan_prob_req_oui_cmd_fixed_param));

	oui_buf = &cmd->prob_req_oui;
	vos_mem_zero(oui_buf, sizeof(cmd->prob_req_oui));
	*oui_buf = psetoui->oui[0] << 16 | psetoui->oui[1] << 8
					| psetoui->oui[2];
	WMA_LOGD("%s: wma:oui received from hdd %08x", __func__,
			cmd->prob_req_oui);

	if (wmi_unified_cmd_send(wma->wmi_handle, wmi_buf, len,
		WMI_SCAN_PROB_REQ_OUI_CMDID)) {
		WMA_LOGE("%s: failed to send command", __func__);
		adf_nbuf_free(wmi_buf);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void wma_process_roam_invoke(WMA_HANDLE handle,
		t_wma_roam_invoke_cmd *roaminvoke)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	wmi_roam_invoke_cmd_fixed_param* cmd;
	wmi_buf_t wmi_buf;
	u_int8_t *buf_ptr;
	u_int16_t len, args_tlv_len;
	A_UINT32 *channel_list;
	wmi_mac_addr *bssid_list;

	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not send roam invoke",
				__func__);
		return;
	}
	/* Host sends only one channel and one bssid */
	args_tlv_len = 2 * WMI_TLV_HDR_SIZE + sizeof(A_UINT32) +
					sizeof(wmi_mac_addr);
	len = sizeof(wmi_roam_invoke_cmd_fixed_param) + args_tlv_len;
	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmai_buf_alloc failed", __func__);
		return;
	}

	cmd = (wmi_roam_invoke_cmd_fixed_param *)wmi_buf_data(wmi_buf);
	buf_ptr = (u_int8_t *) cmd;
	WMITLV_SET_HDR(&cmd->tlv_header,
	WMITLV_TAG_STRUC_wmi_roam_invoke_cmd_fixed_param,
	WMITLV_GET_STRUCT_TLVLEN(wmi_roam_invoke_cmd_fixed_param));
	cmd->vdev_id = roaminvoke->vdev_id;
	cmd->flags = 0;
	cmd->roam_scan_mode = 0;
	cmd->roam_ap_sel_mode = 0;
	cmd->roam_delay = 0;
	cmd->num_chan = 1;
	cmd->num_bssid = 1;
	buf_ptr += sizeof(wmi_roam_invoke_cmd_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
			(sizeof(u_int32_t)));
	channel_list = (A_UINT32 *)(buf_ptr + WMI_TLV_HDR_SIZE);
	*channel_list = (A_UINT32)vos_chan_to_freq(roaminvoke->channel);
	buf_ptr += sizeof(A_UINT32) + WMI_TLV_HDR_SIZE;
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_FIXED_STRUC,
			(sizeof(wmi_mac_addr)));
	bssid_list = (wmi_mac_addr *)(buf_ptr + WMI_TLV_HDR_SIZE);
	WMI_CHAR_ARRAY_TO_MAC_ADDR(roaminvoke->bssid, bssid_list);
	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, len,
				WMI_ROAM_INVOKE_CMDID)) {
		WMA_LOGP("%s: failed to send roam invoke command", __func__);
		adf_nbuf_free(wmi_buf);
		return;
	}
	return;
}
#endif

/**
 * wma_config_stats_factor() - Function to configure stats avg. factor
 * @wma:  pointer to WMA handle
 * @avg_factor:	stats. avg. factor passed down by userspace
 *
 * This function configures the avg. stats value in firmware
 *
 * Return: VOS_STATUS_SUCCESS for success otherwise failure
 *
 */
VOS_STATUS wma_config_stats_factor(tp_wma_handle wma,
				   struct sir_stats_avg_factor *avg_factor)
{
	int ret;

	if (NULL == wma || NULL == avg_factor) {
		WMA_LOGE("%s: Invalid input of stats avg factor", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle,
					    avg_factor->vdev_id,
					    WMI_VDEV_PARAM_STATS_AVG_FACTOR,
					    avg_factor->stats_avg_factor);
	if (ret) {
		WMA_LOGE(" failed to set avg_factor for vdev_id %d",
			 avg_factor->vdev_id);
	}

	WMA_LOGD("%s: Set stats_avg_factor %d for vdev_id %d", __func__,
		 avg_factor->stats_avg_factor, avg_factor->vdev_id);

	return ret;
}

/**
 * wma_config_guard_time() - Function to set guard time in firmware
 * @wma:  pointer to WMA handle
 * @guard_time:  guard time passed down by userspace
 *
 * This function configures the guard time in firmware
 *
 * Return: VOS_STATUS_SUCCESS for success otherwise failure
 *
 */
VOS_STATUS wma_config_guard_time(tp_wma_handle wma,
				 struct sir_guard_time_request *guard_time)
{
	int ret;

	if (NULL == wma || NULL == guard_time) {
		WMA_LOGE("%s: Invalid input of guard time", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle,
					      guard_time->vdev_id,
					      WMI_VDEV_PARAM_RX_LEAK_WINDOW,
					      guard_time->guard_time);
	if (ret) {
		WMA_LOGE(" failed to set guard time for vdev_id %d",
			 guard_time->vdev_id);
	}

	WMA_LOGD("Set guard time %d for vdev_id %d",
		 guard_time->guard_time, guard_time->vdev_id);

	return ret;
}

#ifdef WLAN_FEATURE_MEMDUMP
/*
 * wma_process_fw_mem_dump_req() - Function to request fw memory dump from
 *				   firmware
 * @wma:                Pointer to WMA handle
 * @mem_dump_req:       Pointer for mem_dump_req
 *
 * This function sends memory dump request to firmware
 *
 * Return: VOS_STATUS_SUCCESS for success otherwise failure
 *
 */
static VOS_STATUS wma_process_fw_mem_dump_req(tp_wma_handle wma,
					struct fw_dump_req* mem_dump_req)
{
	wmi_get_fw_mem_dump_fixed_param *cmd;
	wmi_fw_mem_dump *dump_params;
	struct fw_dump_seg_req *seg_req;
	int32_t len;
	wmi_buf_t buf;
	u_int8_t *buf_ptr;
	int ret, loop;

	if (!mem_dump_req || !wma) {
		WMA_LOGE(FL("input pointer is NULL"));
		return VOS_STATUS_E_FAILURE;
	}

	/*
	 * len = sizeof(fixed param) that includes tlv header +
	 *       tlv header for array of struc +
	 *       sizeof (each struct)
	 */
	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE;
	len += mem_dump_req->num_seg * sizeof(wmi_fw_mem_dump);
	buf = wmi_buf_alloc(wma->wmi_handle, len);

	if (!buf) {
		WMA_LOGE(FL("Failed allocate wmi buffer"));
		return VOS_STATUS_E_NOMEM;
        }

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	vos_mem_zero(buf_ptr, len);
	cmd = (wmi_get_fw_mem_dump_fixed_param *) buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_get_fw_mem_dump_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_get_fw_mem_dump_fixed_param));

	cmd->request_id = mem_dump_req->request_id;
	cmd->num_fw_mem_dump_segs = mem_dump_req->num_seg;

	/* TLV indicating array of structures to follow */
	buf_ptr += sizeof(wmi_get_fw_mem_dump_fixed_param);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_STRUC,
		       sizeof(wmi_fw_mem_dump) *
		       cmd->num_fw_mem_dump_segs);

	buf_ptr += WMI_TLV_HDR_SIZE;
	dump_params = (wmi_fw_mem_dump *) buf_ptr;

	WMA_LOGI(FL("request_id:%d num_seg:%d"),
		    mem_dump_req->request_id, mem_dump_req->num_seg);
	for (loop = 0; loop < cmd->num_fw_mem_dump_segs; loop++) {
		seg_req = (struct fw_dump_seg_req *)
			  ((uint8_t *)(mem_dump_req->segment) +
			    loop * sizeof(*seg_req));
		WMITLV_SET_HDR(&dump_params->tlv_header,
			    WMITLV_TAG_STRUC_wmi_fw_mem_dump_params,
			    WMITLV_GET_STRUCT_TLVLEN(wmi_fw_mem_dump));
		dump_params->seg_id = seg_req->seg_id;
		dump_params->seg_start_addr_lo = seg_req-> seg_start_addr_lo;
		dump_params->seg_start_addr_hi = seg_req->seg_start_addr_hi;
		dump_params->seg_length = seg_req->seg_length;
		dump_params->dest_addr_lo = seg_req->dst_addr_lo;
		dump_params->dest_addr_hi = seg_req->dst_addr_hi;
		WMA_LOGI(FL("seg_number:%d"), loop);
		WMA_LOGI(FL("seg_id:%d start_addr_lo:0x%x start_addr_hi:0x%x"),
			 dump_params->seg_id, dump_params->seg_start_addr_lo,
			 dump_params->seg_start_addr_hi);
		WMA_LOGI(FL("seg_length:%d dst_addr_lo:0x%x dst_addr_hi:0x%x"),
			 dump_params->seg_length, dump_params->dest_addr_lo,
			 dump_params->dest_addr_hi);
		dump_params++;
	}

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_GET_FW_MEM_DUMP_CMDID);
        if (ret) {
		WMA_LOGE(FL("Failed to send get firmware mem dump request"));
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
        }

	WMA_LOGI(FL("Get firmware mem dump request sent successfully"));
	return VOS_STATUS_SUCCESS;
}
#else
static VOS_STATUS wma_process_fw_mem_dump_req(tp_wma_handle wma,
                                        void *mem_dump_req)
{
	return VOS_STATUS_SUCCESS;
}
#endif /* WLAN_FEATURE_MEMDUMP */

/*
 * wma_process_set_ie_info() - Function to send IE info to firmware
 * @wma:                Pointer to WMA handle
 * @ie_data:       Pointer for ie data
 *
 * This function sends IE information to firmware
 *
 * Return: VOS_STATUS_SUCCESS for success otherwise failure
 *
 */
static VOS_STATUS wma_process_set_ie_info(tp_wma_handle wma,
					  struct vdev_ie_info* ie_info)
{
	wmi_vdev_set_ie_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	uint32_t len, ie_len_aligned;
	int ret;

	if (!ie_info || !wma) {
		WMA_LOGE(FL("input pointer is NULL"));
		return VOS_STATUS_E_FAILURE;
	}

	/* Validate the input */
	if (ie_info->length  <= 0) {
		WMA_LOGE(FL("Invalid IE length"));
		return -EINVAL;
	}

	ie_len_aligned = roundup(ie_info->length, sizeof(uint32_t));
	/* Allocate memory for the WMI command */
	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE + ie_len_aligned;

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGE(FL("wmi_buf_alloc failed"));
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = wmi_buf_data(buf);
	vos_mem_zero(buf_ptr, len);

	/* Populate the WMI command */
	cmd = (wmi_vdev_set_ie_cmd_fixed_param *)buf_ptr;

	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_vdev_set_ie_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			wmi_vdev_set_ie_cmd_fixed_param));
	cmd->vdev_id = ie_info->vdev_id;
	cmd->ie_id = ie_info->ie_id;
	cmd->ie_len = ie_info->length;

	WMA_LOGE(FL("IE:%d of size:%d sent for vdev:%d"), ie_info->ie_id,
		 ie_info->length, ie_info->vdev_id);

	buf_ptr += sizeof(*cmd);
	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_BYTE, ie_len_aligned);
	buf_ptr += WMI_TLV_HDR_SIZE;

	vos_mem_copy(buf_ptr, ie_info->data, cmd->ie_len);

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_VDEV_SET_IE_CMDID);
	if (ret != EOK) {
		WMA_LOGE(FL("Failed to send set IE command ret = %d"), ret);
		wmi_buf_free(buf);
	}

	return ret;
}

/**
 * wma_enable_specific_fw_logs() - Start/Stop logging of diag event/log id
 * @wma_handle: WMA handle
 * @start_log: Start logging related parameters
 *
 * Send the command to the FW based on which specific logging of diag
 * event/log id can be started/stopped
 *
 * Return: None
 */
void wma_enable_specific_fw_logs(tp_wma_handle wma_handle,
		struct sir_wifi_start_log *start_log)
{
	wmi_diag_event_log_config_fixed_param *cmd;
	wmi_buf_t buf;
	uint8_t *buf_ptr;
	uint32_t len, count, log_level, i;
	uint32_t *cmd_args;
	uint32_t total_len;
	count = 0;

	if (!start_log) {
		WMA_LOGE("%s: start_log pointer is NULL", __func__);
		return;
	}
	if (!wma_handle) {
		WMA_LOGE("%s: Invalid wma handle", __func__);
		return;
	}

	if (!((start_log->ring_id == RING_ID_CONNECTIVITY) ||
			(start_log->ring_id == RING_ID_FIRMWARE_DEBUG))) {
		WMA_LOGD("%s: Not connectivity or fw debug ring: %d",
			__func__, start_log->ring_id);
		return;
	}

	if (!wma_handle->events_logs_list) {
		WMA_LOGE("%s: Not received event/log list from FW, yet",
			__func__);
		return;
	}

	/* total_len stores the number of events where BITS 17 and 18 are set.
	 * i.e., events of high frequency (17) and for extended debugging (18)
	 */
	total_len = 0;
	for (i = 0; i < wma_handle->num_of_diag_events_logs; i++) {
		if ((WMI_DIAG_FREQUENCY_GET(wma_handle->events_logs_list[i])) &&
		    (WMI_DIAG_EXT_FEATURE_GET(wma_handle->events_logs_list[i])))
			total_len++;
	}

	len = sizeof(*cmd) + WMI_TLV_HDR_SIZE +
		(total_len * sizeof(uint32_t));

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return;
	}
	cmd = (wmi_diag_event_log_config_fixed_param *) wmi_buf_data(buf);
	buf_ptr = (uint8_t *) cmd;

	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_diag_event_log_config_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_diag_event_log_config_fixed_param));

	cmd->num_of_diag_events_logs = total_len;

	buf_ptr += sizeof(wmi_diag_event_log_config_fixed_param);

	WMITLV_SET_HDR(buf_ptr, WMITLV_TAG_ARRAY_UINT32,
			(total_len * sizeof(uint32_t)));

	cmd_args = (uint32_t *) (buf_ptr + WMI_TLV_HDR_SIZE);

	if (start_log->verbose_level >= LOG_LEVEL_ACTIVE)
		log_level = 1;
	else
		log_level = 0;

	WMA_LOGD("%s: Length:%d, Log_level:%d", __func__, total_len, log_level);
	for (i = 0; i < wma_handle->num_of_diag_events_logs; i++) {
		uint32_t val = wma_handle->events_logs_list[i];
		if ((WMI_DIAG_FREQUENCY_GET(val)) &&
				(WMI_DIAG_EXT_FEATURE_GET(val))) {

			WMI_DIAG_ID_SET(cmd_args[count],
				WMI_DIAG_ID_GET(val));
			WMI_DIAG_TYPE_SET(cmd_args[count],
				WMI_DIAG_TYPE_GET(val));
			WMI_DIAG_ID_ENABLED_DISABLED_SET(cmd_args[count],
				log_level);
			WMA_LOGD("%s: Idx:%d, val:%x", __func__, i, val);
			count++;
		}
	}

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				WMI_DIAG_EVENT_LOG_CONFIG_CMDID)) {
		WMA_LOGE("%s: WMI_DIAG_EVENT_LOG_CONFIG_CMDID failed",
				__func__);
		wmi_buf_free(buf);
	}
	return;
}

#if !defined(REMOVE_PKT_LOG)
/**
 * wma_set_wifi_start_packet_stats() - Start/stop packet stats
 * @wma_handle: WMA handle
 * @start_log: Struture containing the start wifi logger params
 *
 * This function is used to send the WMA commands to start/stop logging
 * of per packet statistics
 *
 * Return: None
 *
 */
void wma_set_wifi_start_packet_stats(void *wma_handle,
		struct sir_wifi_start_log *start_log)
{
	void *vos_context;
	struct ol_softc *scn;
	uint32_t log_state;

	if (!start_log) {
		WMA_LOGE("%s: start_log pointer is NULL", __func__);
		return;
	}
	if (!wma_handle) {
		WMA_LOGE("%s: Invalid wma handle", __func__);
		return;
	}

	/* No need to register for ring IDs other than packet stats */
	if (start_log->ring_id != RING_ID_PER_PACKET_STATS) {
		WMA_LOGI("%s: Ring id is not for per packet stats: %d",
			__func__, start_log->ring_id);
		return;
	}

	vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
	scn = vos_get_context(VOS_MODULE_ID_HIF, vos_context);

	if (!scn) {
		WMA_LOGE("%s: ol_softc context is NULL\n", __func__);
		return;
	}

	log_state = ATH_PKTLOG_ANI | ATH_PKTLOG_RCUPDATE | ATH_PKTLOG_RCFIND |
		ATH_PKTLOG_RX | ATH_PKTLOG_TX | ATH_PKTLOG_TEXT;

	if (start_log->verbose_level == WLAN_LOG_LEVEL_ACTIVE) {
		pktlog_enable(scn, log_state);
		WMA_LOGI("%s: Enabling per packet stats", __func__);
	} else {
		pktlog_enable(scn, 0);
		WMA_LOGI("%s: Disabling per packet stats", __func__);
	}
}
#endif

/**
 * wma_send_flush_logs_to_fw() - Send log flush command to FW
 * @wma_handle: WMI handle
 *
 * This function is used to send the flush command to the FW,
 * that will flush the fw logs that are residue in the FW
 *
 * Return: None
 */
void wma_send_flush_logs_to_fw(tp_wma_handle wma_handle)
{
	VOS_STATUS status;
	wmi_debug_mesg_flush_fixed_param *cmd;
	wmi_buf_t buf;
	int len = sizeof(*cmd);
	int ret;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		return;
	}

	cmd = (wmi_debug_mesg_flush_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_debug_mesg_flush_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_debug_mesg_flush_fixed_param));
	cmd->reserved0 = 0;

	ret = wmi_unified_cmd_send(wma_handle->wmi_handle,
				   buf,
				   len,
				   WMI_DEBUG_MESG_FLUSH_CMDID);
	if (ret != EOK) {
		WMA_LOGE("Failed to send WMI_DEBUG_MESG_FLUSH_CMDID");
		wmi_buf_free(buf);
		return;
	}
	WMA_LOGI("Sent WMI_DEBUG_MESG_FLUSH_CMDID to FW");

	status = vos_timer_start(&wma_handle->log_completion_timer,
				 WMA_LOG_COMPLETION_TIMER);
	if (status != VOS_STATUS_SUCCESS)
		WMA_LOGE("Failed to start the log completion timer");
}

/**
 * wma_set_rssi_monitoring() - set rssi monitoring
 * @handle: WMA handle
 * @req: rssi monitoring request structure
 *
 * This function reads the incoming @req and fill in the destination
 * WMI structure and send down the rssi monitoring configs down to the firmware
 *
 * Return: 0 on success; error number otherwise
 */
static VOS_STATUS wma_set_rssi_monitoring(tp_wma_handle wma,
					struct rssi_monitor_req *req)
{
	wmi_rssi_breach_monitor_config_fixed_param *cmd;
	wmi_buf_t buf;
	int ret, len = sizeof(*cmd);

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	cmd = (wmi_rssi_breach_monitor_config_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_rssi_breach_monitor_config_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_rssi_breach_monitor_config_fixed_param));

	cmd->vdev_id = req->session_id;
	cmd->request_id = req->request_id;
	cmd->lo_rssi_reenable_hysteresis = 0;
	cmd->hi_rssi_reenable_histeresis = 0;
	cmd->min_report_interval = 0;
	cmd->max_num_report = 1;
	if (req->control) {
		/* enable one threshold for each min/max */
		cmd->enabled_bitmap = 0x09;
		cmd->low_rssi_breach_threshold[0] = req->min_rssi;
		cmd->hi_rssi_breach_threshold[0] = req->max_rssi;
	} else {
		cmd->enabled_bitmap = 0;
		cmd->low_rssi_breach_threshold[0] = 0;
		cmd->hi_rssi_breach_threshold[0] = 0;
	}

	ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				   WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID);
	if (ret != EOK) {
		WMA_LOGE("Failed to send WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID");
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGI("Sent WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID to FW");
	return VOS_STATUS_SUCCESS;
}

/*
 * function   : wma_mc_process_msg
 * Description :
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_mc_process_msg(v_VOID_t *vos_context, vos_msg_t *msg)
{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	tp_wma_handle wma_handle;
	ol_txrx_vdev_handle txrx_vdev_handle = NULL;
	extern tANI_U8* macTraceGetWdaMsgString( tANI_U16 wdaMsg );

	WMA_LOGI("%s: Enter", __func__);
	if(NULL == msg)	{
		WMA_LOGE("msg is NULL");
		VOS_ASSERT(0);
		vos_status = VOS_STATUS_E_INVAL;
		goto end;
	}

	WMA_LOGD("msg->type = %x %s", msg->type, macTraceGetWdaMsgString(msg->type));

	wma_handle = (tp_wma_handle) vos_get_context(VOS_MODULE_ID_WDA,
			vos_context);

	if (NULL == wma_handle) {
		WMA_LOGP("%s: wma_handle is NULL", __func__);
		VOS_ASSERT(0);
		vos_mem_free(msg->bodyptr);
		vos_status = VOS_STATUS_E_INVAL;
		goto end;
	}

	switch (msg->type) {
#ifdef FEATURE_WLAN_ESE
        case WDA_TSM_STATS_REQ:
            WMA_LOGA("McThread: WDA_TSM_STATS_REQ");
            wma_process_tsm_stats_req(wma_handle, (void*)msg->bodyptr);
        break;
#endif
		case WNI_CFG_DNLD_REQ:
			WMA_LOGA("McThread: WNI_CFG_DNLD_REQ");
			vos_status = wma_wni_cfg_dnld(wma_handle);
			if (VOS_IS_STATUS_SUCCESS(vos_status)) {
				vos_WDAComplete_cback(vos_context);
			}
			else {
				WMA_LOGD("config download failure");
			}
			break ;
		case WDA_ADD_STA_SELF_REQ:
			txrx_vdev_handle = wma_vdev_attach(wma_handle,
					(tAddStaSelfParams *)msg->bodyptr, 1);
			if (!txrx_vdev_handle) {
				WMA_LOGE("Failed to attach vdev");
			} else {
				WLANTL_RegisterVdev(vos_context,
						    txrx_vdev_handle);
				/* Register with TxRx Module for Data Ack Complete Cb */
				wdi_in_data_tx_cb_set(txrx_vdev_handle,
					wma_data_tx_ack_comp_hdlr, wma_handle);
			}
			break;
		case WDA_DEL_STA_SELF_REQ:
			wma_vdev_detach(wma_handle,
                            (tDelStaSelfParams *)msg->bodyptr, 1);
			break;
		case WDA_START_SCAN_OFFLOAD_REQ:
			wma_start_scan(wma_handle, msg->bodyptr, msg->type);
                        vos_mem_free(msg->bodyptr);
			break;
		case WDA_STOP_SCAN_OFFLOAD_REQ:
			wma_stop_scan(wma_handle, msg->bodyptr);
                        vos_mem_free(msg->bodyptr);
			break;
		case WDA_UPDATE_CHAN_LIST_REQ:
			wma_update_channel_list(wma_handle,
					(tSirUpdateChanList *)msg->bodyptr);
                        vos_mem_free(msg->bodyptr);
			break;
		case WDA_SET_LINK_STATE:
			wma_set_linkstate(wma_handle,
					  (tpLinkStateParams)msg->bodyptr);
			break;
		case WDA_CHNL_SWITCH_REQ:
			wma_set_channel(wma_handle,
					(tpSwitchChannelParams)msg->bodyptr);
			break;
		case WDA_ADD_BSS_REQ:
			wma_add_bss(wma_handle, (tpAddBssParams)msg->bodyptr);
			break;
		case WDA_ADD_STA_REQ:
			wma_add_sta(wma_handle, (tpAddStaParams)msg->bodyptr);
			break;
		case WDA_SET_BSSKEY_REQ:
			wma_set_bsskey(wma_handle,
					(tpSetBssKeyParams)msg->bodyptr);
			break;
		case WDA_SET_STAKEY_REQ:
			wma_set_stakey(wma_handle,
					(tpSetStaKeyParams)msg->bodyptr);
			break;
		case WDA_DELETE_STA_REQ:
			wma_delete_sta(wma_handle,
					(tpDeleteStaParams)msg->bodyptr);
			break;
		case WDA_DELETE_BSS_REQ:
			wma_delete_bss(wma_handle,
					(tpDeleteBssParams)msg->bodyptr);
			break;
		case WDA_UPDATE_EDCA_PROFILE_IND:
			wma_process_update_edca_param_req(
						wma_handle,
						(tEdcaParams *)msg->bodyptr);
                        vos_mem_free(msg->bodyptr);
			break;
		case WDA_SEND_BEACON_REQ:
			wma_send_beacon(wma_handle,
					(tpSendbeaconParams)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_SEND_PROBE_RSP_TMPL:
			wma_send_probe_rsp_tmpl(wma_handle,
					(tpSendProbeRespParams)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_CLI_SET_CMD:
			wma_process_cli_set_cmd(wma_handle,
					(wda_cli_set_cmd_t *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#if !defined(REMOVE_PKT_LOG)
		case WDA_PKTLOG_ENABLE_REQ:
			wma_pktlog_wmi_send_cmd(wma_handle,
						(struct ath_pktlog_wmi_params *)
						msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#endif
#if defined(QCA_WIFI_FTM)
		case WDA_FTM_CMD_REQ:
			wma_process_ftm_command(wma_handle,
				(struct ar6k_testmode_cmd_data *)msg->bodyptr);
			break;
#endif
		case WDA_ENTER_BMPS_REQ:
			wma_enable_sta_ps_mode(wma_handle,
                                        (tpEnablePsParams)msg->bodyptr);
			break;
		case WDA_EXIT_BMPS_REQ:
			wma_disable_sta_ps_mode(wma_handle,
                                        (tpDisablePsParams)msg->bodyptr);
			break;
		case WDA_ENTER_UAPSD_REQ:
			wma_enable_uapsd_mode(wma_handle,
					(tpEnableUapsdParams)msg->bodyptr);
			break;
		case WDA_EXIT_UAPSD_REQ:
			wma_disable_uapsd_mode(wma_handle,
					(tpDisableUapsdParams)msg->bodyptr);
			break;
		case WDA_SET_TX_POWER_REQ:
			wma_set_tx_power(wma_handle,
					(tpMaxTxPowerParams)msg->bodyptr);
			break;
		case WDA_SET_MAX_TX_POWER_REQ:
			wma_set_max_tx_power(wma_handle,
					(tpMaxTxPowerParams)msg->bodyptr);
			break;
		case WDA_SET_KEEP_ALIVE:
			wma_set_keepalive_req(wma_handle,
					(tSirKeepAliveReq *)msg->bodyptr);
			break;
#ifdef FEATURE_WLAN_SCAN_PNO
		case WDA_SET_PNO_REQ:
			wma_config_pno(wma_handle,
				       (tpSirPNOScanReq)msg->bodyptr);
			break;

		case WDA_SME_SCAN_CACHE_UPDATED:
			wma_scan_cache_updated_ind(wma_handle, msg->bodyval);
			break;
#endif
#if defined(FEATURE_WLAN_ESE) && defined(FEATURE_WLAN_ESE_UPLOAD)
		case WDA_SET_PLM_REQ:
			wma_config_plm(wma_handle,
					(tpSirPlmReq)msg->bodyptr);
                        break;
#endif
		case WDA_GET_STATISTICS_REQ:
			wma_get_stats_req(wma_handle,
					(tAniGetPEStatsReq *) msg->bodyptr);
			break;

		case WDA_CONFIG_PARAM_UPDATE_REQ:
			wma_update_cfg_params(wma_handle,
					(tSirMsgQ *)msg);
			break;

		case WDA_INIT_SCAN_REQ:
			wma_init_scan_req(wma_handle,
					(tInitScanParams *)msg->bodyptr);
			break;

		case WDA_FINISH_SCAN_REQ:
			wma_finish_scan_req(wma_handle,
					(tFinishScanParams *)msg->bodyptr);
			break;
                case WDA_UPDATE_OP_MODE:
                        wma_process_update_opmode(wma_handle,
                                       (tUpdateVHTOpMode *)msg->bodyptr);
                        vos_mem_free(msg->bodyptr);
                        break;
                case WDA_UPDATE_RX_NSS:
                        wma_process_update_rx_nss(wma_handle,
                                       (tUpdateRxNss *)msg->bodyptr);
                        vos_mem_free(msg->bodyptr);
                        break;
#ifdef WLAN_FEATURE_11AC
                case WDA_UPDATE_MEMBERSHIP:
                        wma_process_update_membership(wma_handle,
                                       (tUpdateMembership *)msg->bodyptr);
                        break;
                case WDA_UPDATE_USERPOS:
                        wma_process_update_userpos(wma_handle,
                                       (tUpdateUserPos *)msg->bodyptr);
                        break;
#endif
		case WDA_UPDATE_BEACON_IND:
			wma_process_update_beacon_params(wma_handle,
					(tUpdateBeaconParams *)msg->bodyptr);
                        vos_mem_free(msg->bodyptr);
			break;

		case WDA_ADD_TS_REQ:
			wma_add_ts_req(wma_handle, (tAddTsParams *)msg->bodyptr);
			break;

		case WDA_DEL_TS_REQ:
			wma_del_ts_req(wma_handle, (tDelTsParams *)msg->bodyptr);
			break;

                case WDA_AGGR_QOS_REQ:
                        wma_aggr_qos_req(wma_handle, (tAggrAddTsParams *)msg->bodyptr);
                        break;

		case WDA_RECEIVE_FILTER_SET_FILTER_REQ:
			wma_process_receive_filter_set_filter_req(wma_handle,
						(tSirRcvPktFilterCfgType *)msg->bodyptr);
			break;

		case WDA_RECEIVE_FILTER_CLEAR_FILTER_REQ:
			wma_process_receive_filter_clear_filter_req(wma_handle,
						(tSirRcvFltPktClearParam *)msg->bodyptr);
			break;

		case WDA_WOWL_ADD_BCAST_PTRN:
			wma_wow_add_pattern(wma_handle,
					   (tpSirWowlAddBcastPtrn)msg->bodyptr);
			break;
		case WDA_WOWL_DEL_BCAST_PTRN:
			wma_wow_del_pattern(wma_handle,
					   (tpSirWowlDelBcastPtrn)msg->bodyptr);
			break;
		case WDA_WOWL_ENTER_REQ:
			wma_wow_enter(wma_handle,
				      (tpSirHalWowlEnterParams)msg->bodyptr);
			break;
		case WDA_WOWL_EXIT_REQ:
			wma_wow_exit(wma_handle,
				    (tpSirHalWowlExitParams)msg->bodyptr);
			break;
		case WDA_WLAN_SUSPEND_IND:
			wma_suspend_req(wma_handle,
					(tpSirWlanSuspendParam)msg->bodyptr);
			break;
		case WDA_8023_MULTICAST_LIST_REQ:
			wma_process_mcbc_set_filter_req(wma_handle,
				       (tpSirRcvFltMcAddrList)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#ifdef WLAN_FEATURE_GTK_OFFLOAD
		case WDA_GTK_OFFLOAD_REQ:
			wma_process_gtk_offload_req(
					wma_handle,
					(tpSirGtkOffloadParams)msg->bodyptr);
			break;

		case WDA_GTK_OFFLOAD_GETINFO_REQ:
			wma_process_gtk_offload_getinfo_req(
				wma_handle,
				(tpSirGtkOffloadGetInfoRspParams)msg->bodyptr);
			break;
#endif /* WLAN_FEATURE_GTK_OFFLOAD */
#ifdef FEATURE_OEM_DATA_SUPPORT
		case WDA_START_OEM_DATA_REQ:
			wma_start_oem_data_req(wma_handle,
					(tStartOemDataReq *)msg->bodyptr);
			break;
#endif /* FEATURE_OEM_DATA_SUPPORT */
		case WDA_SET_HOST_OFFLOAD:
			wma_enable_arp_ns_offload(wma_handle, (tpSirHostOffloadReq)msg->bodyptr, true);
			break;
#ifdef WLAN_NS_OFFLOAD
		case WDA_SET_NS_OFFLOAD:
			wma_enable_arp_ns_offload(wma_handle, (tpSirHostOffloadReq)msg->bodyptr, false);
			break;
#endif /*WLAN_NS_OFFLOAD */
                case WDA_ROAM_SCAN_OFFLOAD_REQ:
			/*
			 * Main entry point or roaming directives from CSR.
			 */
		    wma_process_roam_scan_req(wma_handle,
				(tSirRoamOffloadScanReq *)msg->bodyptr);
		    break;

		case WDA_RATE_UPDATE_IND:
			wma_process_rate_update_indicate(wma_handle, (tSirRateUpdateInd *)msg->bodyptr);
			break;

#ifdef FEATURE_WLAN_TDLS
		case WDA_UPDATE_FW_TDLS_STATE:
			wma_update_fw_tdls_state(wma_handle,
			  (t_wma_tdls_params *)msg->bodyptr);
			break;
		case WDA_UPDATE_TDLS_PEER_STATE:
			wma_update_tdls_peer_state(wma_handle,
			  (tTdlsPeerStateParams *)msg->bodyptr);
			break;
		case WDA_TDLS_SET_OFFCHAN_MODE:
			wma_set_tdls_offchan_mode(wma_handle,
			  (tTdlsChanSwitchParams*)msg->bodyptr);
			break;
#endif /* FEATURE_WLAN_TDLS */
		case WDA_ADD_PERIODIC_TX_PTRN_IND:
			wma_ProcessAddPeriodicTxPtrnInd(wma_handle,
				(tSirAddPeriodicTxPtrn *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_DEL_PERIODIC_TX_PTRN_IND:
			wma_ProcessDelPeriodicTxPtrnInd(wma_handle,
				 (tSirDelPeriodicTxPtrn *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_TX_POWER_LIMIT:
			wma_ProcessTxPowerLimits(wma_handle,
				(tSirTxPowerLimit *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#ifdef FEATURE_WLAN_LPHB
		case WDA_LPHB_CONF_REQ:
			wma_process_lphb_conf_req(wma_handle, (tSirLPHBReq *)msg->bodyptr);
			break;
#endif

#ifdef FEATURE_WLAN_CH_AVOID
		case WDA_CH_AVOID_UPDATE_REQ:
			wma_process_ch_avoid_update_req(wma_handle,
				(tSirChAvoidUpdateReq *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#endif
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
		case WDA_SET_AUTO_SHUTDOWN_TIMER_REQ:
			wma_set_auto_shutdown_timer_req(wma_handle,
							msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#endif
		case WDA_DHCP_START_IND:
		case WDA_DHCP_STOP_IND:
			wma_process_dhcp_ind(wma_handle,
				(tAniDHCPInd *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;


		case WDA_INIT_THERMAL_INFO_CMD:
			wma_process_init_thermal_info(wma_handle, (t_thermal_mgmt *)msg->bodyptr);
                        vos_mem_free(msg->bodyptr);
			break;

		case WDA_SET_THERMAL_LEVEL:
			wma_process_set_thermal_level(wma_handle, msg->bodyval);
			break;

		case WDA_SET_P2P_GO_NOA_REQ:
			wma_process_set_p2pgo_noa_Req(wma_handle,
						(tP2pPsParams *)msg->bodyptr);
                        vos_mem_free(msg->bodyptr);
			break;
		case WDA_SET_MIMOPS_REQ:
			wma_process_set_mimops_req(wma_handle, (tSetMIMOPS *) msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_SET_SAP_INTRABSS_DIS:
			wma_set_vdev_intrabss_fwd(wma_handle, (tDisableIntraBssFwd *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_FW_STATS_IND:
			wma_fw_stats_ind(wma_handle, msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_GET_LINK_SPEED:
			wma_get_link_speed(wma_handle, msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_MODEM_POWER_STATE_IND:
			wma_notify_modem_power_state(wma_handle,
					(tSirModemPowerStateInd *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_VDEV_STOP_IND:
			wma_vdev_stop_ind(wma_handle, msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_WLAN_RESUME_REQ:
			wma_resume_req(wma_handle, false);
			break;

#ifdef WLAN_FEATURE_STATS_EXT
		case WDA_STATS_EXT_REQUEST:
			wma_stats_ext_req(wma_handle,
					  (tpStatsExtRequest)(msg->bodyptr));
			vos_mem_free(msg->bodyptr);
			break;
#endif
		case WDA_HIDDEN_SSID_VDEV_RESTART:
			wma_hidden_ssid_vdev_restart(wma_handle,
					(tHalHiddenSsidVdevRestart *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#ifdef WLAN_FEATURE_EXTWOW_SUPPORT
		case WDA_WLAN_EXT_WOW:
			wma_enable_ext_wow(wma_handle,
				(tSirExtWoWParams *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_WLAN_SET_APP_TYPE1_PARAMS:
			wma_set_app_type1_params_in_fw(wma_handle,
				(tSirAppType1Params *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_WLAN_SET_APP_TYPE2_PARAMS:
			wma_set_app_type2_params_in_fw(wma_handle,
				(tSirAppType2Params *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#endif
		case WDA_VDEV_START_RSP_IND:
			wma_vdev_start_rsp_ind(wma_handle, msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_ROAM_PREAUTH_IND:
			wma_roam_preauth_ind(wma_handle,  msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_TBTT_UPDATE_IND:
			wma_tbtt_update_ind(wma_handle, msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#ifdef FEATURE_WLAN_EXTSCAN
		case WDA_EXTSCAN_START_REQ:
			wma_start_extscan(wma_handle,
				(tSirWifiScanCmdReqParams *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_EXTSCAN_STOP_REQ:
			wma_stop_extscan(wma_handle,
				(tSirExtScanStopReqParams *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_EXTSCAN_SET_BSSID_HOTLIST_REQ:
			wma_extscan_start_hotlist_monitor(wma_handle,
				(tSirExtScanSetBssidHotListReqParams *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_EXTSCAN_RESET_BSSID_HOTLIST_REQ:
			wma_extscan_stop_hotlist_monitor(wma_handle,
			(tSirExtScanResetBssidHotlistReqParams *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_EXTSCAN_SET_SSID_HOTLIST_REQ:
			wma_set_ssid_hotlist(wma_handle,
				(struct sir_set_ssid_hotlist_request *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_EXTSCAN_SET_SIGNF_CHANGE_REQ:
			wma_extscan_start_change_monitor(wma_handle,
				(tSirExtScanSetSigChangeReqParams *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_EXTSCAN_RESET_SIGNF_CHANGE_REQ:
			wma_extscan_stop_change_monitor(wma_handle,
				(tSirExtScanResetSignificantChangeReqParams *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_EXTSCAN_GET_CACHED_RESULTS_REQ:
			wma_extscan_get_cached_results(wma_handle,
				(tSirExtScanGetCachedResultsReqParams *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_EXTSCAN_GET_CAPABILITIES_REQ:
			wma_extscan_get_capabilities(wma_handle,
				(tSirGetExtScanCapabilitiesReqParams *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
		break;
		case WDA_SET_EPNO_LIST_REQ:
			wma_set_epno_network_list(wma_handle,
				(struct wifi_epno_params *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
		break;
		case WDA_SET_PASSPOINT_LIST_REQ:
			/* Issue reset passpoint network list first and clear
			 * the entries */
			wma_reset_passpoint_network_list(wma_handle,
				(struct wifi_passpoint_req *)msg->bodyptr);

			wma_set_passpoint_network_list(wma_handle,
				(struct wifi_passpoint_req *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
		break;
		case WDA_RESET_PASSPOINT_LIST_REQ:
			wma_reset_passpoint_network_list(wma_handle,
				(struct wifi_passpoint_req *)msg->bodyptr);
		break;
		case WDA_EXTSCAN_STATUS_IND:
			wma_extscan_rsp_handler(wma_handle, msg->bodyptr);
			vos_mem_free(msg->bodyptr);
		break;
		case WDA_EXTSCAN_OPERATION_IND:
			wma_extscan_operations_ind_handler(wma_handle,
							   msg->bodyptr);
			vos_mem_free(msg->bodyptr);
		break;
#endif
		case WDA_SET_SCAN_MAC_OUI_REQ:
			wma_scan_probe_setoui(wma_handle, msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#ifdef WLAN_FEATURE_LINK_LAYER_STATS
		case WDA_LINK_LAYER_STATS_CLEAR_REQ:
			wma_process_ll_stats_clearReq(wma_handle,
				(tpSirLLStatsClearReq)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_LINK_LAYER_STATS_SET_REQ:
			wma_process_ll_stats_setReq(wma_handle,
				(tpSirLLStatsSetReq)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_LINK_LAYER_STATS_GET_REQ:
			wma_process_ll_stats_getReq(wma_handle,
				(tpSirLLStatsGetReq)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#endif /* WLAN_FEATURE_LINK_LAYER_STATS */
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
		case WDA_ROAM_OFFLOAD_SYNCH_CNF:
			wma_process_roam_synch_complete(wma_handle,
					(tSirSmeRoamOffloadSynchCnf *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case SIR_HAL_UNIT_TEST_CMD:
			wma_process_unit_test_cmd(wma_handle,
					(t_wma_unit_test_cmd *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_ROAM_OFFLOAD_SYNCH_FAIL:
			wma_process_roam_synch_fail(wma_handle,
				(tSirRoamOffloadSynchFail *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case SIR_HAL_ROAM_INVOKE:
			wma_process_roam_invoke(wma_handle,
				(t_wma_roam_invoke_cmd *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#endif
#ifdef WLAN_FEATURE_NAN
		case WDA_NAN_REQUEST:
			wma_nan_req(wma_handle,
				(tNanRequest *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
#endif
		case SIR_HAL_SET_BASE_MACADDR_IND:
			wma_set_base_macaddr_indicate(wma_handle,
				       (tSirMacAddr *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_LINK_STATUS_GET_REQ:
			wma_process_link_status_req(wma_handle,
				(tAniGetLinkStatus *)msg->bodyptr);
#ifdef FEATURE_RUNTIME_PM
		case WDA_RUNTIME_PM_SUSPEND_IND:
			wma_suspend_req(wma_handle, NULL);
			break;
		case WDA_RUNTIME_PM_RESUME_IND:
			wma_resume_req(wma_handle, true);
			break;
#endif
			break;
		case WDA_GET_LINK_STATUS_RSP_IND:
			wma_link_status_rsp(wma_handle, msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case SIR_HAL_CONFIG_STATS_FACTOR:
			wma_config_stats_factor(wma_handle,
				(struct sir_stats_avg_factor *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case SIR_HAL_CONFIG_GUARD_TIME:
			wma_config_guard_time(wma_handle,
				(struct sir_guard_time_request *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_FW_MEM_DUMP_REQ:
			wma_process_fw_mem_dump_req(wma_handle,
				(struct fw_dump_req*)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
                        break;
		case SIR_HAL_START_STOP_LOGGING:
			wma_set_wifi_start_packet_stats(wma_handle,
				(struct sir_wifi_start_log *)msg->bodyptr);
			wma_enable_specific_fw_logs(wma_handle,
				(struct sir_wifi_start_log *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case SIR_HAL_FLUSH_LOG_TO_FW:
			wma_send_flush_logs_to_fw(wma_handle);
			/* Body ptr is NULL here */
			break;
		case WDA_SET_IE_INFO:
			wma_process_set_ie_info(wma_handle,
					(struct vdev_ie_info *) msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		case WDA_SET_RSSI_MONITOR_REQ:
			wma_set_rssi_monitoring(wma_handle,
				(struct rssi_monitor_req *)msg->bodyptr);
			vos_mem_free(msg->bodyptr);
			break;
		default:
			WMA_LOGD("unknow msg type %x", msg->type);
			/* Do Nothing? MSG Body should be freed at here */
			if(NULL != msg->bodyptr) {
				vos_mem_free(msg->bodyptr);
			}
	}
end:
	WMA_LOGI("%s: Exit", __func__);
	return vos_status ;
}

static int wma_scan_event_callback(WMA_HANDLE handle, u_int8_t *data,
                                    u_int32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_SCAN_EVENTID_param_tlvs *param_buf = NULL;
	wmi_scan_event_fixed_param *wmi_event = NULL;
	tSirScanOffloadEvent *scan_event;
	u_int8_t vdev_id;
	v_U32_t scan_id;
	u_int8_t *buf;
	vos_msg_t vos_msg = {0};
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

        param_buf = (WMI_SCAN_EVENTID_param_tlvs *) data;
        wmi_event = param_buf->fixed_param;
        vdev_id = wmi_event->vdev_id;
        scan_id = wma_handle->interfaces[vdev_id].scan_info.scan_id;

	adf_os_spin_lock_bh(&wma_handle->roam_preauth_lock);
	if (wma_handle->roam_preauth_scan_id == wmi_event->scan_id) {
		/* This is the scan requested by roam preauth set_channel operation */
		adf_os_spin_unlock_bh(&wma_handle->roam_preauth_lock);

		if (wmi_event->event & WMI_SCAN_FINISH_EVENTS) {
			WMA_LOGE(" roam scan complete - scan_id %x, vdev_id %x",
					wmi_event->scan_id, vdev_id);
			wma_reset_scan_info(wma_handle, vdev_id);
		}

		buf = vos_mem_malloc(sizeof(wmi_scan_event_fixed_param));
		if (!buf) {
			WMA_LOGE("%s: Memory alloc failed for roam preauth ind",
				__func__);
			return -ENOMEM;
		}
		vos_mem_zero(buf, sizeof(wmi_scan_event_fixed_param));
		vos_mem_copy(buf, (u_int8_t *)wmi_event,
				sizeof(wmi_scan_event_fixed_param));

		vos_msg.type = WDA_ROAM_PREAUTH_IND;
		vos_msg.bodyptr = buf;
		vos_msg.bodyval = 0;

		if (VOS_STATUS_SUCCESS !=
			vos_mq_post_message(VOS_MQ_ID_WDA, &vos_msg)) {
			WMA_LOGE("%s: Failed to post WDA_ROAM_PREAUTH_IND msg",
				 __func__);
			vos_mem_free(buf);
			return -1;
		}
		WMA_LOGD("%s: WDA_ROAM_PREAUTH_IND posted", __func__);
		return 0;
	}
	adf_os_spin_unlock_bh(&wma_handle->roam_preauth_lock);

	scan_event = (tSirScanOffloadEvent *) vos_mem_malloc
                                (sizeof(tSirScanOffloadEvent));
	if (!scan_event) {
		WMA_LOGE("Memory allocation failed for tSirScanOffloadEvent");
		return -ENOMEM;
	}

	scan_event->event = wmi_event->event;

	WMA_LOGI("WMA <-- wmi_scan_event : event %u, scan_id %u, "
			"freq %u, reason %u",
			wmi_event->event, wmi_event->scan_id,
			wmi_event->channel_freq, wmi_event->reason);

	scan_event->scanId = wmi_event->scan_id;
	scan_event->chanFreq = wmi_event->channel_freq;
	scan_event->p2pScanType =
		wma_handle->interfaces[vdev_id].scan_info.p2p_scan_type;
	scan_event->sessionId = vdev_id;

	if (wmi_event->reason == WMI_SCAN_REASON_COMPLETED ||
	    wmi_event->reason == WMI_SCAN_REASON_TIMEDOUT)
		scan_event->reasonCode = eSIR_SME_SUCCESS;
	else
		scan_event->reasonCode = eSIR_SME_SCAN_FAILED;

	switch (wmi_event->event) {
	case WMI_SCAN_EVENT_COMPLETED:
	case WMI_SCAN_EVENT_DEQUEUED:
		/*
		 * return success always so that SME can pick whatever scan
		 * results is available in scan cache(due to partial or
		 * aborted scan)
		 */
		scan_event->event = WMI_SCAN_EVENT_COMPLETED;
		scan_event->reasonCode = eSIR_SME_SUCCESS;
		break;
	case WMI_SCAN_EVENT_START_FAILED:
		scan_event->event = WMI_SCAN_EVENT_COMPLETED;
		scan_event->reasonCode = eSIR_SME_SCAN_FAILED;
		break;
	case WMI_SCAN_EVENT_PREEMPTED:
		WMA_LOGW("%s: Unhandled Scan Event WMI_SCAN_EVENT_PREEMPTED", __func__);
		break;
	case WMI_SCAN_EVENT_RESTARTED:
		WMA_LOGW("%s: Unhandled Scan Event WMI_SCAN_EVENT_RESTARTED", __func__);
		break;
	}

        /* Stop the scan completion timeout if the event is WMI_SCAN_EVENT_COMPLETED */
        if (scan_event->event == (tSirScanEventType)WMI_SCAN_EVENT_COMPLETED) {
                WMA_LOGE(" scan complete - scan_id %x, vdev_id %x",
		wmi_event->scan_id, vdev_id);
		/*
		 * first stop the timer then reset scan info, else there is a
		 * race condition between, timeout handler in host and reset
		 * operation here. because of that, sometime timeout handler
		 * triggers and scan ID mismatch messages is printed.
		 */
		vos_status = vos_timer_stop(&wma_handle->wma_scan_comp_timer);
                if (vos_status != VOS_STATUS_SUCCESS) {
			WMA_LOGE("Failed to stop the scan completion timeout");
			vos_mem_free(scan_event);
			return -EPERM;
                }
		if (wmi_event->scan_id == scan_id)
			wma_reset_scan_info(wma_handle, vdev_id);
		else
			WMA_LOGE("Scan id not matched for SCAN COMPLETE event");
        }

	wma_send_msg(wma_handle, WDA_RX_SCAN_EVENT, (void *) scan_event, 0) ;
	return 0;
}

static void wma_mgmt_tx_ack_work_handler(struct work_struct *ack_work)
{
	struct wma_tx_ack_work_ctx *work;
	tp_wma_handle wma_handle;
	pWDAAckFnTxComp ack_cb;

	if (vos_is_load_unload_in_progress(VOS_MODULE_ID_WDA, NULL)) {
		WMA_LOGE("%s: Driver load/unload in progress", __func__);
		return;
	}

	work = container_of(ack_work, struct wma_tx_ack_work_ctx, ack_cmp_work);
        wma_handle = work->wma_handle;
	ack_cb = wma_handle->umac_ota_ack_cb[work->sub_type];

	WMA_LOGD("Tx Ack Cb SubType %d Status %d",
			work->sub_type, work->status);

	/* Call the Ack Cb registered by UMAC */
	ack_cb((tpAniSirGlobal)(wma_handle->mac_context),
                                work->status ? 0 : 1);

	adf_os_mem_free(work);
	wma_handle->ack_work_ctx = NULL;
}

/* function   : wma_mgmt_tx_comp_conf_ind
 * Description : Post mgmt tx complete indication to PE.
 * Args       :
                wma_handle  : Pointer to WMA handle
 *              sub_type    : Tx mgmt frame sub type
 *              status      : Mgmt frame tx status
 * Returns    :
 */
static void
wma_mgmt_tx_comp_conf_ind(tp_wma_handle wma_handle, u_int8_t sub_type,
							int32_t status)
{
	int32_t tx_comp_status;

	tx_comp_status = status ? 0 : 1;
	if(sub_type == SIR_MAC_MGMT_DISASSOC) {
		wma_send_msg(wma_handle, WDA_DISASSOC_TX_COMP, NULL, tx_comp_status);
	}
	else if(sub_type == SIR_MAC_MGMT_DEAUTH) {
		wma_send_msg(wma_handle, WDA_DEAUTH_TX_COMP, NULL, tx_comp_status);
	}
}

/**
  * wma_mgmt_tx_ack_comp_hdlr - handles tx ack mgmt completion
  * @context: context with which the handler is registered
  * @netbuf: tx mgmt nbuf
  * @err: status of tx completion
  *
  * This is the cb registered with TxRx for
  * Ack Complete
  */
static void
wma_mgmt_tx_ack_comp_hdlr(void *wma_context,
		adf_nbuf_t netbuf, int32_t status)
{
	tpSirMacFrameCtl pFc =
		(tpSirMacFrameCtl)(adf_nbuf_data(netbuf));
	tp_wma_handle wma_handle = (tp_wma_handle)wma_context;

	if(wma_handle && wma_handle->umac_ota_ack_cb[pFc->subType]) {
		if((pFc->subType == SIR_MAC_MGMT_DISASSOC) ||
			(pFc->subType == SIR_MAC_MGMT_DEAUTH)) {
			wma_mgmt_tx_comp_conf_ind(wma_handle, (u_int8_t)pFc->subType,
										status);
		}
		else {
			struct wma_tx_ack_work_ctx *ack_work;

			ack_work =
			adf_os_mem_alloc(NULL, sizeof(struct wma_tx_ack_work_ctx));

			if(ack_work) {
#ifdef CONFIG_CNSS
				cnss_init_work(&ack_work->ack_cmp_work,
						wma_mgmt_tx_ack_work_handler);
#else
				INIT_WORK(&ack_work->ack_cmp_work,
						wma_mgmt_tx_ack_work_handler);
#endif
				ack_work->wma_handle = wma_handle;
				ack_work->sub_type = pFc->subType;
				ack_work->status = status;

				/* Schedue the Work */
				schedule_work(&ack_work->ack_cmp_work);
			}
		}
	}
}

/**
  * wma_mgmt_tx_dload_comp_hldr - handles tx mgmt completion
  * @context: context with which the handler is registered
  * @netbuf: tx mgmt nbuf
  * @err: status of tx completion
  */
static void
wma_mgmt_tx_dload_comp_hldr(void *wma_context, adf_nbuf_t netbuf,
					int32_t status)
{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

	tp_wma_handle wma_handle = (tp_wma_handle)wma_context;
	void *mac_context = wma_handle->mac_context;

	WMA_LOGD("Tx Complete Status %d", status);

	if (!wma_handle->tx_frm_download_comp_cb) {
		WMA_LOGE("Tx Complete Cb not registered by umac");
		return;
	}

	/* Call Tx Mgmt Complete Callback registered by umac */
	wma_handle->tx_frm_download_comp_cb(mac_context,
					netbuf, 0);

	/* Reset Callback */
	wma_handle->tx_frm_download_comp_cb = NULL;

	/* Set the Tx Mgmt Complete Event */
	vos_status  = vos_event_set(
			&wma_handle->tx_frm_download_comp_event);
	if (!VOS_IS_STATUS_SUCCESS(vos_status))
		WMA_LOGP("%s: Event Set failed - tx_frm_comp_event", __func__);
}

/**
  * wma_tx_attach - attaches tx fn with underlying layer
  * @pwmaCtx: wma context
  */
VOS_STATUS wma_tx_attach(tp_wma_handle wma_handle)
{
	/* Get the Vos Context */
	pVosContextType vos_handle =
		(pVosContextType)(wma_handle->vos_context);

	/* Get the txRx Pdev handle */
	ol_txrx_pdev_handle txrx_pdev =
		(ol_txrx_pdev_handle)(vos_handle->pdev_txrx_ctx);

	/* Register for Tx Management Frames */
	wdi_in_mgmt_tx_cb_set(txrx_pdev, GENERIC_NODOWLOAD_ACK_COMP_INDEX,
				NULL, wma_mgmt_tx_ack_comp_hdlr,wma_handle);

	wdi_in_mgmt_tx_cb_set(txrx_pdev, GENERIC_DOWNLD_COMP_NOACK_COMP_INDEX,
				wma_mgmt_tx_dload_comp_hldr, NULL, wma_handle);

	wdi_in_mgmt_tx_cb_set(txrx_pdev, GENERIC_DOWNLD_COMP_ACK_COMP_INDEX,
				wma_mgmt_tx_dload_comp_hldr,
				wma_mgmt_tx_ack_comp_hdlr,wma_handle);

	/* Store the Mac Context */
	wma_handle->mac_context = vos_handle->pMACContext;

	return VOS_STATUS_SUCCESS;
}

/**
 * wma_tx_detach - detaches mgmt fn with underlying layer
 * Deregister with TxRx for Tx Mgmt Download and Ack completion.
 * @tp_wma_handle: wma context
 */
static VOS_STATUS wma_tx_detach(tp_wma_handle wma_handle)
{
	u_int32_t frame_index = 0;

	/* Get the Vos Context */
	pVosContextType vos_handle =
		(pVosContextType)(wma_handle->vos_context);

	/* Get the txRx Pdev handle */
	ol_txrx_pdev_handle txrx_pdev =
		(ol_txrx_pdev_handle)(vos_handle->pdev_txrx_ctx);

	if (txrx_pdev) {
		/* Deregister with TxRx for Tx Mgmt completion call back */
		for (frame_index = 0; frame_index < FRAME_INDEX_MAX;
			frame_index++) {
			wdi_in_mgmt_tx_cb_set(txrx_pdev, frame_index, NULL, NULL,
			txrx_pdev);
		}
	}
	/* Destroy Tx Frame Complete event */
	vos_event_destroy(&wma_handle->tx_frm_download_comp_event);

	/* Tx queue empty check event (dummy event) */
	vos_event_destroy(&wma_handle->tx_queue_empty_event);

	/* Reset Tx Frm Callbacks */
	wma_handle->tx_frm_download_comp_cb = NULL;

	/* Reset Tx Data Frame Ack Cb */
	wma_handle->umac_data_ota_ack_cb = NULL;

	/* Reset last Tx Data Frame nbuf ptr */
	wma_handle->last_umac_data_nbuf = NULL;

	return VOS_STATUS_SUCCESS;
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
static void wma_roam_ho_fail_handler(tp_wma_handle wma, u_int32_t vdev_id)
{
	tSirSmeHOFailureInd *ho_failure_ind;
	vos_msg_t sme_msg = {0};
        VOS_STATUS vos_status;

	ho_failure_ind = vos_mem_malloc(sizeof(tSirSmeHOFailureInd));

	if (NULL == ho_failure_ind) {
		WMA_LOGE("%s: Memory allocation failure", __func__);
		return;
	}
	ho_failure_ind->sessionId = vdev_id;
	sme_msg.type = eWNI_SME_HO_FAIL_IND;
	sme_msg.bodyptr = ho_failure_ind;
	sme_msg.bodyval = 0;

	vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &sme_msg);
	if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
		WMA_LOGE("Fail to post eWNI_SME_HO_FAIL_IND msg to SME");
		vos_mem_free(ho_failure_ind);
		return;
	}
	return;
}
#endif

/* function   : wma_roam_better_ap_handler
 * Description : Handler for WMI_ROAM_REASON_BETTER_AP event from roam firmware in Rome.
 *            : This event means roam algorithm in Rome has found a better matching
 *            : candidate AP. The indication is sent through tl_shim as by repeating
 *            : the last beacon. Hence this routine calls a tlshim routine.
 * Args       :
 * Returns    :
 */
static void wma_roam_better_ap_handler(tp_wma_handle wma, u_int32_t vdev_id)
{
	VOS_STATUS ret;
	/* abort existing scans from GUI, but not roaming preauth scan */
	if (wma->interfaces[vdev_id].scan_info.scan_id != 0 &&
	    (wma->interfaces[vdev_id].scan_info.scan_id &
	     WMA_HOST_ROAM_SCAN_REQID_PREFIX) !=
		WMA_HOST_ROAM_SCAN_REQID_PREFIX) {
		tAbortScanParams abortScan;
		abortScan.SessionId = vdev_id;
		wma_stop_scan(wma, &abortScan);
	}
	ret = tlshim_mgmt_roam_event_ind(wma->vos_context, vdev_id);
}

/* function   : wma_roam_event_callback
 * Description : Handler for all events from roam engine in firmware
 * Args       :
 * Returns    :
 */

static int wma_roam_event_callback(WMA_HANDLE handle, u_int8_t *event_buf,
				u_int32_t len)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_ROAM_EVENTID_param_tlvs *param_buf;
	wmi_roam_event_fixed_param *wmi_event;

	param_buf = (WMI_ROAM_EVENTID_param_tlvs *) event_buf;
	if (!param_buf) {
		WMA_LOGE("Invalid roam event buffer");
		return -EINVAL;
	}

	wmi_event = param_buf->fixed_param;
	WMA_LOGD("%s: Reason %x for vdevid %x, rssi %d",
		__func__, wmi_event->reason, wmi_event->vdev_id, wmi_event->rssi);

	switch(wmi_event->reason) {
	case WMI_ROAM_REASON_BMISS:
		WMA_LOGD("Beacon Miss for vdevid %x",
			wmi_event->vdev_id);
		wma_beacon_miss_handler(wma_handle, wmi_event->vdev_id);
		break;
	case WMI_ROAM_REASON_BETTER_AP:
		WMA_LOGD("%s:Better AP found for vdevid %x, rssi %d", __func__,
			wmi_event->vdev_id, wmi_event->rssi);
		wma_handle->suitable_ap_hb_failure = FALSE;
		wma_roam_better_ap_handler(wma_handle, wmi_event->vdev_id);
		break;
	case WMI_ROAM_REASON_SUITABLE_AP:
		wma_handle->suitable_ap_hb_failure = TRUE;
		WMA_LOGD("%s:Bmiss scan AP found for vdevid %x, rssi %d", __func__,
			wmi_event->vdev_id, wmi_event->rssi);
		wma_roam_better_ap_handler(wma_handle, wmi_event->vdev_id);
		break;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	case WMI_ROAM_REASON_HO_FAILED:
		WMA_LOGE("LFR3:Hand-Off Failed for vdevid %x",
			wmi_event->vdev_id);
		wma_roam_ho_fail_handler(wma_handle, wmi_event->vdev_id);
		break;
#endif
	default:
		WMA_LOGD("%s:Unhandled Roam Event %x for vdevid %x", __func__,
		wmi_event->reason, wmi_event->vdev_id);
		break;
	}
	return 0;
}

#ifdef FEATURE_WLAN_SCAN_PNO

/* Record NLO match event comes from FW. It's a indication that
 * one of the profile is matched.
 */
static int wma_nlo_match_evt_handler(void *handle, u_int8_t *event,
				     u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	wmi_nlo_event *nlo_event;
	WMI_NLO_MATCH_EVENTID_param_tlvs *param_buf =
				(WMI_NLO_MATCH_EVENTID_param_tlvs *) event;
	struct wma_txrx_node *node;

	if (!param_buf) {
		WMA_LOGE("Invalid NLO match event buffer");
		return -EINVAL;
	}

	nlo_event = param_buf->fixed_param;
	WMA_LOGD("PNO match event received for vdev %d",
		 nlo_event->vdev_id);

	node = &wma->interfaces[nlo_event->vdev_id];
	if (node)
		node->nlo_match_evt_received = TRUE;

	vos_wake_lock_timeout_acquire(&wma->pno_wake_lock,
				      WMA_PNO_MATCH_WAKE_LOCK_TIMEOUT,
				      WIFI_POWER_EVENT_WAKELOCK_PNO);

	return 0;
}

/* Handles NLO scan completion event. */
static int wma_nlo_scan_cmp_evt_handler(void *handle, u_int8_t *event,
					u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	wmi_nlo_event *nlo_event;
	WMI_NLO_SCAN_COMPLETE_EVENTID_param_tlvs *param_buf =
			(WMI_NLO_SCAN_COMPLETE_EVENTID_param_tlvs *) event;
	tSirScanOffloadEvent *scan_event;
	struct wma_txrx_node *node;

	if (!param_buf) {
		WMA_LOGE("Invalid NLO scan comp event buffer");
		return -EINVAL;
	}

	nlo_event = param_buf->fixed_param;
	WMA_LOGD("PNO scan completion event received for vdev %d",
		 nlo_event->vdev_id);

	node = &wma->interfaces[nlo_event->vdev_id];

	/* Handle scan completion event only after NLO match event. */
	if (!node || !node->nlo_match_evt_received) {

		WMA_LOGD("NLO match not received skipping PNO complete ind for vdev %d",
		nlo_event->vdev_id);
		goto skip_pno_cmp_ind;
	}
	vos_wake_lock_release(&wma->pno_wake_lock,
			      WIFI_POWER_EVENT_WAKELOCK_PNO);

	scan_event = (tSirScanOffloadEvent *) vos_mem_malloc(
					      sizeof(tSirScanOffloadEvent));
	if (scan_event) {
		/* Posting scan completion msg would take scan cache result
		 * from LIM module and update in scan cache maintained in SME.*/
		WMA_LOGD("Posting Scan completion to umac");
		vos_wake_lock_timeout_acquire(&wma->pno_wake_lock,
				      WMA_PNO_SCAN_COMPLETE_WAKE_LOCK_TIMEOUT,
				      WIFI_POWER_EVENT_WAKELOCK_PNO);
		vos_mem_zero(scan_event, sizeof(tSirScanOffloadEvent));
		scan_event->reasonCode = eSIR_SME_SUCCESS;
		scan_event->event = SCAN_EVENT_COMPLETED;
		scan_event->sessionId = nlo_event->vdev_id;
		wma_send_msg(wma, WDA_RX_SCAN_EVENT,
			     (void *) scan_event, 0);
	} else {
		WMA_LOGE("Memory allocation failed for tSirScanOffloadEvent");
	}

skip_pno_cmp_ind:
	return 0;
}

#endif

#if defined(CONFIG_HL_SUPPORT) || defined(QCA_SUPPORT_TXRX_VDEV_PAUSE_LL)
/* Handle TX pause event from FW */
static int wma_mcc_vdev_tx_pause_evt_handler(void *handle, u_int8_t *event,
					u_int32_t len)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	WMI_TX_PAUSE_EVENTID_param_tlvs *param_buf;
	wmi_tx_pause_event_fixed_param  *wmi_event;
	u_int8_t vdev_id;
	A_UINT32 vdev_map;

	param_buf = (WMI_TX_PAUSE_EVENTID_param_tlvs *) event;
	if (!param_buf)
	{
		WMA_LOGE("Invalid roam event buffer");
		return -EINVAL;
	}

	if (wma_get_wow_bus_suspend(wma)) {
		WMA_LOGD(" Suspend is in progress: Pause/Unpause Tx is NoOp");
		return 0;
	}

	wmi_event = param_buf->fixed_param;
	vdev_map  = wmi_event->vdev_map;
	/* FW mapped vdev from ID
	 * vdev_map = (1 << vdev_id)
	 * So, host should unmap to ID */
	for (vdev_id = 0; vdev_map != 0; vdev_id++)
	{
		if (!(vdev_map & 0x1))
		{
			/* No Vdev */
		}
		else
		{
			if (!wma->interfaces[vdev_id].handle)
			{
				WMA_LOGE("%s: invalid vdev ID %d", __func__, vdev_id);
				/* Test Next VDEV */
				vdev_map >>= 1;
				continue;
			}

			/* PAUSE action, add bitmap */
			if (ACTION_PAUSE == wmi_event->action)
			{
				/*
				 * Now only support per-dev pause so it is not necessary
				 * to pause a paused queue again.
				 */
				if (!wma->interfaces[vdev_id].pause_bitmap)
					wdi_in_vdev_pause(wma->interfaces[vdev_id].handle,
							OL_TXQ_PAUSE_REASON_FW);
				wma->interfaces[vdev_id].pause_bitmap |= (1 << wmi_event->pause_type);
			}
			/* UNPAUSE action, clean bitmap */
			else if (ACTION_UNPAUSE == wmi_event->action)
			{
				/* Handle unpause only if already paused*/
				if(wma->interfaces[vdev_id].pause_bitmap)
				{
					wma->interfaces[vdev_id].pause_bitmap &= ~(1 << wmi_event->pause_type);

					if (!wma->interfaces[vdev_id].pause_bitmap)
					{
						/* PAUSE BIT MAP is cleared
						 * UNPAUSE VDEV */
						wdi_in_vdev_unpause(wma->interfaces[vdev_id].handle,
							OL_TXQ_PAUSE_REASON_FW);
					}
				}
			}
			else
			{
				WMA_LOGE("Not Valid Action Type %d", wmi_event->action);
			}

			WMA_LOGD("vdev_id %d, pause_map 0x%x, pause type %d, action %d",
				vdev_id, wma->interfaces[vdev_id].pause_bitmap,
				wmi_event->pause_type, wmi_event->action);
		}
		/* Test Next VDEV */
		vdev_map >>= 1;
	}

	return 0;
}
#endif /* QCA_SUPPORT_TXRX_VDEV_PAUSE_LL */

/* function   : wma_set_thermal_mgmt
 * Description : This function sends the thermal management command to the firmware
 * Args       :
                wma_handle     : Pointer to WMA handle
 *              thermal_info   : Thermal command information
 * Returns    :
 *              VOS_STATUS_SUCCESS for success otherwise failure
 */
static VOS_STATUS wma_set_thermal_mgmt(tp_wma_handle wma_handle,
					t_thermal_cmd_params thermal_info)
{
	wmi_thermal_mgmt_cmd_fixed_param *cmd = NULL;
	wmi_buf_t buf = NULL;
	int status = 0;
	u_int32_t len = 0;

	len = sizeof(*cmd);

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("Failed to allocate buffer to send set key cmd");
		return eHAL_STATUS_FAILURE;
	}

	cmd = (wmi_thermal_mgmt_cmd_fixed_param *) wmi_buf_data (buf);

	WMITLV_SET_HDR(&cmd->tlv_header,
				   WMITLV_TAG_STRUC_wmi_thermal_mgmt_cmd_fixed_param,
				   WMITLV_GET_STRUCT_TLVLEN(wmi_thermal_mgmt_cmd_fixed_param));

	cmd->lower_thresh_degreeC = thermal_info.minTemp;
	cmd->upper_thresh_degreeC = thermal_info.maxTemp;
	cmd->enable = thermal_info.thermalEnable;

	WMA_LOGE("TM Sending thermal mgmt cmd: low temp %d, upper temp %d, enabled %d",
			 cmd->lower_thresh_degreeC, cmd->upper_thresh_degreeC, cmd->enable);

	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len,
				  WMI_THERMAL_MGMT_CMDID);
	if (status) {
		adf_nbuf_free(buf);
		WMA_LOGE("%s:Failed to send thermal mgmt command", __func__);
		return eHAL_STATUS_FAILURE;
	}

	return eHAL_STATUS_SUCCESS;
}

/* function   : wma_thermal_mgmt_get_level
 * Description : This function returns the thermal(throttle) level given the temperature
 * Args       :
                handle     : Pointer to WMA handle
 *              temp       : temperature
 * Returns    :
 *              thermal (throttle) level
 */
u_int8_t wma_thermal_mgmt_get_level(void *handle, u_int32_t temp)
{
	tp_wma_handle wma = (tp_wma_handle) handle;
	int i;
	u_int8_t level;

	level = i = wma->thermal_mgmt_info.thermalCurrLevel;
	while (temp < wma->thermal_mgmt_info.thermalLevels[i].minTempThreshold &&
		   i > 0) {
		i--;
		level = i;
	}

	i = wma->thermal_mgmt_info.thermalCurrLevel;
	while (temp > wma->thermal_mgmt_info.thermalLevels[i].maxTempThreshold &&
		   i < (WLAN_WMA_MAX_THERMAL_LEVELS - 1)) {
		i++;
		level = i;
	}

	WMA_LOGW("Change thermal level from %d -> %d\n",
			  wma->thermal_mgmt_info.thermalCurrLevel, level);

	return level;
}

/* function   : wma_thermal_mgmt_evt_handler
 * Description : This function handles the thermal mgmt event from the firmware
 * Args       :
                wma_handle     : Pointer to WMA handle
 *              event          : Thermal event information
 *              len            :
 * Returns    :
 *              0 for success otherwise failure
 */
static int wma_thermal_mgmt_evt_handler(void *handle, u_int8_t *event,
					u_int32_t len)
{
	tp_wma_handle wma;
	wmi_thermal_mgmt_event_fixed_param *tm_event;
	u_int8_t thermal_level;
	t_thermal_cmd_params thermal_params;
	WMI_THERMAL_MGMT_EVENTID_param_tlvs *param_buf;
	ol_txrx_pdev_handle curr_pdev;

	if (NULL == event || NULL == handle) {
                WMA_LOGE("Invalid thermal mitigation event buffer");
                return -EINVAL;
        }

	wma = (tp_wma_handle) handle;

	if (NULL == wma) {
		WMA_LOGE("%s: Failed to get wma handle", __func__);
		return -EINVAL;
	}

	param_buf = (WMI_THERMAL_MGMT_EVENTID_param_tlvs *) event;

	curr_pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma->vos_context);
	if (NULL == curr_pdev) {
		WMA_LOGE("%s: Failed to get pdev", __func__);
		return -EINVAL;
	}

	/* Check if thermal mitigation is enabled */
	if (!wma->thermal_mgmt_info.thermalMgmtEnabled){
		WMA_LOGE("Thermal mgmt is not enabled, ignoring event");
		return -EINVAL;
	}

	tm_event = param_buf->fixed_param;
	WMA_LOGD("Thermal mgmt event received with temperature %d",
		 tm_event->temperature_degreeC);

	/* Get the thermal mitigation level for the reported temperature*/
	thermal_level = wma_thermal_mgmt_get_level(handle, tm_event->temperature_degreeC);
	WMA_LOGD("Thermal mgmt level  %d", thermal_level);

	if (thermal_level == wma->thermal_mgmt_info.thermalCurrLevel) {
		WMA_LOGD("Current level %d is same as the set level, ignoring",
				  wma->thermal_mgmt_info.thermalCurrLevel);
		return 0;
	}

	wma->thermal_mgmt_info.thermalCurrLevel = thermal_level;

	/* Inform txrx */
	ol_tx_throttle_set_level(curr_pdev, thermal_level);

	/* Get the temperature thresholds to set in firmware */
	thermal_params.minTemp =
		 wma->thermal_mgmt_info.thermalLevels[thermal_level].minTempThreshold;
	thermal_params.maxTemp =
		 wma->thermal_mgmt_info.thermalLevels[thermal_level].maxTempThreshold;
	thermal_params.thermalEnable =
		 wma->thermal_mgmt_info.thermalMgmtEnabled;

	if (VOS_STATUS_SUCCESS != wma_set_thermal_mgmt(wma, thermal_params)) {
		WMA_LOGE("Could not send thermal mgmt command to the firmware!");
		return -EINVAL;
	}

	return 0;
}

#ifdef FEATURE_WLAN_CH_AVOID
/* Process channel to avoid event comes from FW.
 */
static int wma_channel_avoid_evt_handler(void *handle, u_int8_t *event,
					u_int32_t len)
{
	wmi_avoid_freq_ranges_event_fixed_param *afr_fixed_param;
	wmi_avoid_freq_range_desc *afr_desc;
	u_int32_t num_freq_ranges, freq_range_idx;
	tSirChAvoidIndType *sca_indication;
	VOS_STATUS vos_status;
	vos_msg_t sme_msg = {0} ;
	WMI_WLAN_FREQ_AVOID_EVENTID_param_tlvs *param_buf =
			(WMI_WLAN_FREQ_AVOID_EVENTID_param_tlvs *) event;

	if (!param_buf) {
		WMA_LOGE("Invalid channel avoid event buffer");
		return -EINVAL;
	}

	afr_fixed_param = param_buf->fixed_param;
	if (!afr_fixed_param) {
		WMA_LOGE("Invalid channel avoid event fixed param buffer");
		return -EINVAL;
	}

	num_freq_ranges = (afr_fixed_param->num_freq_ranges > SIR_CH_AVOID_MAX_RANGE)?
				SIR_CH_AVOID_MAX_RANGE:afr_fixed_param->num_freq_ranges;

	WMA_LOGD("Channel avoid event received with %d ranges", num_freq_ranges);
	for (freq_range_idx = 0; freq_range_idx < num_freq_ranges; freq_range_idx++) {
			afr_desc = (wmi_avoid_freq_range_desc *) ((void *)param_buf->avd_freq_range
				+ freq_range_idx * sizeof(wmi_avoid_freq_range_desc));
			WMA_LOGD("range %d: tlv id = %u, start freq = %u,  end freq = %u",
					freq_range_idx,
					afr_desc->tlv_header,
					afr_desc->start_freq,
					afr_desc->end_freq);
	}

	sca_indication = (tSirChAvoidIndType *)
				vos_mem_malloc(sizeof(tSirChAvoidIndType));
	if (!sca_indication) {
		WMA_LOGE("Invalid channel avoid indication buffer");
		return -EINVAL;
	}

	sca_indication->avoid_range_count = num_freq_ranges;
	for (freq_range_idx = 0; freq_range_idx < num_freq_ranges; freq_range_idx++) {
		afr_desc = (wmi_avoid_freq_range_desc *) ((void *)param_buf->avd_freq_range
			+ freq_range_idx * sizeof(wmi_avoid_freq_range_desc));
		sca_indication->avoid_freq_range[freq_range_idx].start_freq =
			afr_desc->start_freq;
		sca_indication->avoid_freq_range[freq_range_idx].end_freq =
			afr_desc->end_freq;
	}

	sme_msg.type = eWNI_SME_CH_AVOID_IND;
	sme_msg.bodyptr = sca_indication;
	sme_msg.bodyval = 0;

	vos_status = vos_mq_post_message(VOS_MODULE_ID_SME, &sme_msg);
	if ( !VOS_IS_STATUS_SUCCESS(vos_status) )
	{
		WMA_LOGE("Fail to post eWNI_SME_CH_AVOID_IND msg to SME");
		vos_mem_free(sca_indication);
		return -EINVAL;
	}

	return 0;
}

/* function   : wma_process_ch_avoid_update_req
 * Description : handles channel avoid update request
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_process_ch_avoid_update_req(tp_wma_handle wma_handle,
			tSirChAvoidUpdateReq *ch_avoid_update_req)
{
	int status = 0;
	wmi_buf_t buf = NULL;
	u_int8_t *buf_ptr;
	wmi_chan_avoid_update_cmd_param *ch_avoid_update_fp;
	int len = sizeof(wmi_chan_avoid_update_cmd_param);

	if (ch_avoid_update_req == NULL)
	{
		WMA_LOGE("%s : ch_avoid_update_req is NULL", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGI("%s: WMA --> WMI_CHAN_AVOID_UPDATE",
		__func__);

	buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!buf) {
		WMA_LOGE("%s : wmi_buf_alloc failed", __func__);
		return VOS_STATUS_E_NOMEM;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	ch_avoid_update_fp = (wmi_chan_avoid_update_cmd_param *) buf_ptr;
	WMITLV_SET_HDR(&ch_avoid_update_fp->tlv_header,
	WMITLV_TAG_STRUC_wmi_chan_avoid_update_cmd_param,
	WMITLV_GET_STRUCT_TLVLEN(
		wmi_chan_avoid_update_cmd_param));

	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
		len, WMI_CHAN_AVOID_UPDATE_CMDID);
	if (status != EOK) {
		WMA_LOGE("wmi_unified_cmd_send"
			" WMITLV_TABLE_WMI_CHAN_AVOID_UPDATE"
			" returned Error %d",
			status);
		wmi_buf_free(buf);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGI("%s: WMA --> WMI_CHAN_AVOID_UPDATE sent through WMI",
		__func__);
	return VOS_STATUS_SUCCESS;
}
#endif /* FEATURE_WLAN_CH_AVOID */

/* function   :  wma_scan_completion_timeout
 * Description :
 * Args       :
 * Returns    :
 */
void wma_scan_completion_timeout(void *data)
{
        tp_wma_handle wma_handle;
        tSirScanOffloadEvent *scan_event;
        u_int8_t vdev_id;

        WMA_LOGE("%s: Timeout occured for scan command", __func__);

        wma_handle = (tp_wma_handle) data;

        scan_event = (tSirScanOffloadEvent *) vos_mem_malloc
                                (sizeof(tSirScanOffloadEvent));
        if (!scan_event) {
                WMA_LOGE("%s: Memory allocation failed for tSirScanOffloadEvent", __func__);
                return;
        }

        vdev_id = wma_handle->wma_scan_timer_info.vdev_id;

        if (wma_handle->wma_scan_timer_info.scan_id !=
                wma_handle->interfaces[vdev_id].scan_info.scan_id) {
                vos_mem_free(scan_event);
                WMA_LOGE("%s: Scan ID mismatch", __func__);
                return;
        }

	/*
	 * To avoid race condition between scan timeout in host and in firmware
	 * here we should just send abort scan to firmware and do cleanup after
	 * receiving event from firmware. Since at this moment there will be no
	 * outstanding scans, aborting should not cause any problem in firmware.
	 */
	if (wma_handle->interfaces[vdev_id].scan_info.scan_id != 0) {
		tAbortScanParams abortScan;
		abortScan.SessionId = vdev_id;
		WMA_LOGW("%s: Sending abort for timed out scan", __func__);
		wma_stop_scan(wma_handle, &abortScan);
	}

        return;
}

/**
 * wma_log_completion_timeout() - Log completion timeout
 * @data: Timeout handler data
 *
 * This function is called when log completion timer expires
 *
 * Return: None
 */
void wma_log_completion_timeout(void *data)
{
	tp_wma_handle wma_handle;

	WMA_LOGE("%s: Timeout occured for log completion command", __func__);

	wma_handle = (tp_wma_handle) data;
	if (!wma_handle)
		WMA_LOGE("%s: Invalid WMA handle", __func__);

	/* Though we did not receive any event from FW,
	 * we can flush whatever logs we have with us */
	vos_logging_set_fw_flush_complete();

	return;
}

/* function   : wma_start
 * Description :
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_start(v_VOID_t *vos_ctx)
{
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	tp_wma_handle wma_handle;
	int status;
	WMA_LOGD("%s: Enter", __func__);

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);

	/* validate the wma_handle */
	if (NULL == wma_handle) {
		WMA_LOGP("%s: Invalid handle", __func__);
		vos_status = VOS_STATUS_E_INVAL;
		goto end;
	}


	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
						WMI_SCAN_EVENTID,
						wma_scan_event_callback);
	if (0 != status) {
		WMA_LOGP("%s: Failed to register scan callback", __func__);
		vos_status = VOS_STATUS_E_FAILURE;
		goto end;
	}

	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
						WMI_ROAM_EVENTID,
						wma_roam_event_callback);
	if (0 != status) {
		WMA_LOGP("%s: Failed to register Roam callback", __func__);
		vos_status = VOS_STATUS_E_FAILURE;
		goto end;
	}

	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
						WMI_WOW_WAKEUP_HOST_EVENTID,
						wma_wow_wakeup_host_event);
	if (status) {
		WMA_LOGP("%s: Failed to register wow wakeup host event handler",
				__func__);
		vos_status = VOS_STATUS_E_FAILURE;
		goto end;
	}

#ifdef FEATURE_WLAN_D0WOW
	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
					WMI_D0_WOW_DISABLE_ACK_EVENTID,
					wma_d0_wow_disable_ack_event);
	if (status) {
		WMA_LOGE("%s: Failed to register D0-WOW disable event handler!",
			__func__);
		vos_status = VOS_STATUS_E_FAILURE;
		goto end;
	}
#endif

#ifdef FEATURE_WLAN_SCAN_PNO
	if (WMI_SERVICE_IS_ENABLED(wma_handle->wmi_service_bitmap,
				   WMI_SERVICE_NLO)) {

		WMA_LOGD("FW supports pno offload, registering nlo match handler");

		status = wmi_unified_register_event_handler(
				wma_handle->wmi_handle,
				WMI_NLO_MATCH_EVENTID,
				wma_nlo_match_evt_handler);
		if (status) {
			WMA_LOGE("Failed to register nlo match event cb");
			vos_status = VOS_STATUS_E_FAILURE;
			goto end;
		}

		status = wmi_unified_register_event_handler(
				wma_handle->wmi_handle,
				WMI_NLO_SCAN_COMPLETE_EVENTID,
				wma_nlo_scan_cmp_evt_handler);
		if (status) {
			WMA_LOGE("Failed to register nlo scan comp event cb");
			vos_status = VOS_STATUS_E_FAILURE;
			goto end;
		}
	}
#endif

#if defined(CONFIG_HL_SUPPORT) || defined(QCA_SUPPORT_TXRX_VDEV_PAUSE_LL)
	WMA_LOGE("MCC TX Pause Event Handler register");
	status = wmi_unified_register_event_handler(
			wma_handle->wmi_handle,
			WMI_TX_PAUSE_EVENTID,
			wma_mcc_vdev_tx_pause_evt_handler);
#endif /* QCA_SUPPORT_TXRX_VDEV_PAUSE_LL */

#ifdef FEATURE_WLAN_CH_AVOID
	WMA_LOGD("Registering channel to avoid handler");

	status = wmi_unified_register_event_handler(
			wma_handle->wmi_handle,
			WMI_WLAN_FREQ_AVOID_EVENTID,
			wma_channel_avoid_evt_handler);
	if (status) {
		WMA_LOGE("Failed to register channel to avoid event cb");
		vos_status = VOS_STATUS_E_FAILURE;
		goto end;
	}
#endif /* FEATURE_WLAN_CH_AVOID */
#ifdef FEATURE_WLAN_AUTO_SHUTDOWN
	WMA_LOGD("Registering auto shutdown handler");
	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
	   WMI_HOST_AUTO_SHUTDOWN_EVENTID, wma_auto_shutdown_event_handler);
	if (status) {
		WMA_LOGE("Failed to register WMI Auto shutdown event handler");
		vos_status = VOS_STATUS_E_FAILURE;
		goto end;
	}
#endif
	status = wmi_unified_register_event_handler(
	   wma_handle->wmi_handle,
	   WMI_THERMAL_MGMT_EVENTID,
	   wma_thermal_mgmt_evt_handler);
	if (status) {
		WMA_LOGE("Failed to register thermal mitigation event cb");
		vos_status = VOS_STATUS_E_FAILURE;
		goto end;
	}

	vos_status = VOS_STATUS_SUCCESS;

#ifdef QCA_WIFI_FTM
	/*
	 * Tx mgmt attach requires TXRX context which is not created
	 * in FTM mode as WLANTL_Open will not be called in this mode.
	 * So skip the TX mgmt attach.
	 */
	if (vos_get_conparam() == VOS_FTM_MODE)
		goto end;
#endif

	vos_status = wma_tx_attach(wma_handle);
	if(vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGP("%s: Failed to register tx management", __func__);
		goto end;
	}

	/* Initialize scan completion timeout */
	vos_status = vos_timer_init(&wma_handle->wma_scan_comp_timer,
					VOS_TIMER_TYPE_SW,
					wma_scan_completion_timeout,
					wma_handle);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to initialize scan completion timeout");
		goto end;
	}

	/* Initialize log completion timeout */
	vos_status = vos_timer_init(&wma_handle->log_completion_timer,
			VOS_TIMER_TYPE_SW,
			wma_log_completion_timeout,
			wma_handle);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to initialize log completion timeout");
		goto end;
	}

	/* Initialize the log flush complete event handler */
	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_DEBUG_MESG_FLUSH_COMPLETE_EVENTID,
			wma_flush_complete_evt_handler);
	if (status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to register log flush complete event cb");
		vos_status = VOS_STATUS_E_FAILURE;
		goto end;
	}

end:
	WMA_LOGD("%s: Exit", __func__);
	return vos_status;
}

/* function   : wma_stop
 * Description :
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_stop(v_VOID_t *vos_ctx, tANI_U8 reason)
{
	tp_wma_handle wma_handle;
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	int i;

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);

	WMA_LOGD("%s: Enter", __func__);

	/* validate the wma_handle */
	if (NULL == wma_handle) {
		WMA_LOGP("%s: Invalid handle", __func__);
		vos_status = VOS_STATUS_E_INVAL;
		goto end;
	}

#ifdef QCA_WIFI_FTM
	/*
	 * Tx mgmt detach requires TXRX context which is not created
	 * in FTM mode as WLANTL_Open will not be called in this mode.
	 * So skip the TX mgmt detach.
	 */
	if (vos_get_conparam() == VOS_FTM_MODE) {
		vos_status = VOS_STATUS_SUCCESS;
		goto end;
	}
#endif

	if (wma_handle->ack_work_ctx) {
		vos_flush_work(&wma_handle->ack_work_ctx->ack_cmp_work);
		adf_os_mem_free(wma_handle->ack_work_ctx);
		wma_handle->ack_work_ctx = NULL;
	}

        /* Destroy the timer for scan completion */
        vos_status = vos_timer_destroy(&wma_handle->wma_scan_comp_timer);
        if (vos_status != VOS_STATUS_SUCCESS) {
                WMA_LOGE("Failed to destroy the scan completion timer");
        }

	/* Destroy the timer for log completion */
	vos_status = vos_timer_destroy(&wma_handle->log_completion_timer);
	if (vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to destroy the log completion timer");
	}

	/* There's no need suspend target which is already down during SSR. */
	if (!vos_is_logp_in_progress(VOS_MODULE_ID_HIF, NULL)) {
#ifdef HIF_USB
		/* Suspend the target and enable interrupt */
		if (wma_suspend_target(wma_handle, 0))
			WMA_LOGE("Failed to suspend target");
#else
		/* Suspend the target and disable interrupt */
		if (wma_suspend_target(wma_handle, 1))
			WMA_LOGE("Failed to suspend target");
#endif
	}

	/* clean up ll-queue for all vdev */
	for (i = 0; i < wma_handle->max_bssid; i++) {
		if (wma_handle->interfaces[i].handle &&
				wma_handle->interfaces[i].vdev_up) {
			ol_txrx_vdev_flush(wma_handle->interfaces[i].handle);
		}
	}

	vos_status = wma_tx_detach(wma_handle);
	if(vos_status != VOS_STATUS_SUCCESS) {
		WMA_LOGP("%s: Failed to deregister tx management", __func__);
		goto end;
	}

end:
	WMA_LOGD("%s: Exit", __func__);
	return vos_status;
}

static void wma_cleanup_vdev_resp(tp_wma_handle wma)
{
	struct wma_target_req *msg, *tmp;

	adf_os_spin_lock_bh(&wma->vdev_respq_lock);
	list_for_each_entry_safe(msg, tmp,
				 &wma->vdev_resp_queue, node) {
		list_del(&msg->node);
		vos_timer_destroy(&msg->event_timeout);
		adf_os_mem_free(msg);
	}
	adf_os_spin_unlock_bh(&wma->vdev_respq_lock);
}

VOS_STATUS wma_wmi_service_close(v_VOID_t *vos_ctx)
{
	tp_wma_handle wma_handle;
	struct beacon_info *bcn;
	int i;

	WMA_LOGD("%s: Enter", __func__);

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);

	/* validate the wma_handle */
	if (NULL == wma_handle) {
		WMA_LOGE("%s: Invalid wma handle", __func__);
		return VOS_STATUS_E_INVAL;
	}

	/* validate the wmi handle */
	if (NULL == wma_handle->wmi_handle) {
		WMA_LOGE("%s: Invalid wmi handle", __func__);
		return VOS_STATUS_E_INVAL;
	}

	/* dettach the wmi serice */
	WMA_LOGD("calling wmi_unified_detach");
	wmi_unified_detach(wma_handle->wmi_handle);
	wma_handle->wmi_handle = NULL;

	for (i = 0; i < wma_handle->max_bssid; i++) {
		bcn = wma_handle->interfaces[i].beacon;

		if (bcn) {
			if (bcn->dma_mapped)
				adf_nbuf_unmap_single(wma_handle->adf_dev,
						bcn->buf, ADF_OS_DMA_TO_DEVICE);
			adf_nbuf_free(bcn->buf);
			vos_mem_free(bcn);
			wma_handle->interfaces[i].beacon = NULL;
		}

		if (wma_handle->interfaces[i].handle) {
			adf_os_mem_free(wma_handle->interfaces[i].handle);
			wma_handle->interfaces[i].handle = NULL;
		}
	}

	vos_mem_free(wma_handle->interfaces);
	/* free the wma_handle */
	vos_free_context(wma_handle->vos_context, VOS_MODULE_ID_WDA, wma_handle);

	adf_os_mem_free(((pVosContextType) vos_ctx)->cfg_ctx);
	WMA_LOGD("%s: Exit", __func__);
	return VOS_STATUS_SUCCESS;
}

/**
 * wma_wmi_work_close() - close the work queue items associated with WMI
 * @vos_ctx:	Pointer to vos context
 *
 * This function closes work queue items associated with WMI, but not fully
 * closes WMI service.
 *
 * Return: VOS_STATUS_SUCCESS if work close is successful. Otherwise
 *         proper error codes.
 */
VOS_STATUS wma_wmi_work_close(v_VOID_t *vos_ctx)
{
	tp_wma_handle wma_handle;

	WMA_LOGD("%s: Enter", __func__);

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);

	/* validate the wma_handle */
	if (NULL == wma_handle) {
		WMA_LOGE("%s: Invalid wma handle", __func__);
		return VOS_STATUS_E_INVAL;
	}

	/* validate the wmi handle */
	if (NULL == wma_handle->wmi_handle) {
		WMA_LOGE("%s: Invalid wmi handle", __func__);
		return VOS_STATUS_E_INVAL;
	}

	/* remove the wmi work */
	WMA_LOGD("calling wmi_unified_remove_work");
	wmi_unified_remove_work(wma_handle->wmi_handle);

	return VOS_STATUS_SUCCESS;
}

/*
 * Detach DFS methods
 */
static void wma_dfs_detach(struct ieee80211com *dfs_ic)
{
	dfs_detach(dfs_ic);

        vos_lock_destroy(&dfs_ic->chan_lock);
	if (NULL != dfs_ic->ic_curchan) {
		OS_FREE(dfs_ic->ic_curchan);
		dfs_ic->ic_curchan = NULL;
	}

	OS_FREE(dfs_ic);
}

/* function   : wma_close
 * Description :
 * Args       :
 * Returns    :
 */
VOS_STATUS wma_close(v_VOID_t *vos_ctx)
{
	tp_wma_handle wma_handle;
	u_int32_t idx;
	u_int8_t ptrn_id;
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

	WMA_LOGD("%s: Enter", __func__);

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);

	/* validate the wma_handle */
	if (NULL == wma_handle) {
		WMA_LOGE("%s: Invalid wma handle", __func__);
		return VOS_STATUS_E_INVAL;
	}

	/* validate the wmi handle */
	if (NULL == wma_handle->wmi_handle) {
		WMA_LOGP("%s: Invalid wmi handle", __func__);
		return VOS_STATUS_E_INVAL;
	}

	if (wma_handle->events_logs_list) {
		vos_mem_free(wma_handle->events_logs_list);
		wma_handle->events_logs_list = NULL;
		WMA_LOGD("%s: Event log list freed", __func__);
	}

	/* Free wow pattern cache */
	for (ptrn_id = 0; ptrn_id < wma_handle->wlan_resource_config.num_wow_filters;
		ptrn_id++)
		wma_free_wow_ptrn(wma_handle, ptrn_id);

	if (vos_get_conparam() != VOS_FTM_MODE) {
#ifdef FEATURE_WLAN_SCAN_PNO
		vos_wake_lock_destroy(&wma_handle->pno_wake_lock);
#endif
#ifdef FEATURE_WLAN_EXTSCAN
		vos_wake_lock_destroy(&wma_handle->extscan_wake_lock);
#endif
		vos_wake_lock_destroy(&wma_handle->wow_wake_lock);
	}

	/* unregister Firmware debug log */
	vos_status = dbglog_deinit(wma_handle->wmi_handle);
	if(vos_status != VOS_STATUS_SUCCESS)
		WMA_LOGP("%s: dbglog_deinit failed", __func__);

	/* close the vos events */
	vos_event_destroy(&wma_handle->wma_ready_event);
	vos_event_destroy(&wma_handle->target_suspend);
	vos_event_destroy(&wma_handle->wma_resume_event);
	vos_event_destroy(&wma_handle->wow_tx_complete);
	vos_event_destroy(&wma_handle->recovery_event);
	vos_event_destroy(&wma_handle->runtime_suspend);
	wma_cleanup_vdev_resp(wma_handle);
	for(idx = 0; idx < wma_handle->num_mem_chunks; ++idx) {
		adf_os_mem_free_consistent(
				wma_handle->adf_dev,
				wma_handle->mem_chunks[idx].len,
				wma_handle->mem_chunks[idx].vaddr,
				wma_handle->mem_chunks[idx].paddr,
				adf_os_get_dma_mem_context(
					(&(wma_handle->mem_chunks[idx])),
					memctx));
	}

#if defined(QCA_WIFI_FTM)
	/* Detach UTF and unregister the handler */
	if (vos_get_conparam() == VOS_FTM_MODE)
		wma_utf_detach(wma_handle);
#endif

	if (NULL != wma_handle->dfs_ic){
		wma_dfs_detach(wma_handle->dfs_ic);
		wma_handle->dfs_ic = NULL;
	}

	if (NULL != wma_handle->pGetRssiReq) {
		adf_os_mem_free(wma_handle->pGetRssiReq);
		wma_handle->pGetRssiReq = NULL;
	}

	WMA_LOGD("%s: Exit", __func__);
	return VOS_STATUS_SUCCESS;
}

static v_VOID_t wma_update_fw_config(tp_wma_handle wma_handle,
				     struct wma_target_cap *tgt_cap)
{
	/*
	 * tgt_cap contains default target resource configuration
	 * which can be modified here, if required
	 */
	/* Override the no. of max fragments as per platform configuration */
	tgt_cap->wlan_resource_config.max_frag_entries =
		MIN(QCA_OL_11AC_TX_MAX_FRAGS, wma_handle->max_frag_entry);
	wma_handle->max_frag_entry = tgt_cap->wlan_resource_config.max_frag_entries;
}

/**
 * allocate a chunk of memory at the index indicated and
 * if allocation fail allocate smallest size possiblr and
 * return number of units allocated.
 */
static u_int32_t wma_alloc_host_mem_chunk(tp_wma_handle wma_handle,
					  u_int32_t req_id, u_int32_t idx,
					  u_int32_t num_units,
					  u_int32_t unit_len)
{
	adf_os_dma_addr_t paddr;
	if (!num_units  || !unit_len)  {
		return 0;
	}
	wma_handle->mem_chunks[idx].vaddr = NULL ;
	/** reduce the requested allocation by half until allocation succeeds */
	while(wma_handle->mem_chunks[idx].vaddr == NULL && num_units ) {
		wma_handle->mem_chunks[idx].vaddr = adf_os_mem_alloc_consistent(
				wma_handle->adf_dev, num_units*unit_len, &paddr,
				adf_os_get_dma_mem_context(
					(&(wma_handle->mem_chunks[idx])),
					memctx));
		if(wma_handle->mem_chunks[idx].vaddr == NULL) {
			num_units = (num_units >> 1) ; /* reduce length by half */
		} else {
			wma_handle->mem_chunks[idx].paddr = paddr;
			wma_handle->mem_chunks[idx].len = num_units*unit_len;
			wma_handle->mem_chunks[idx].req_id =  req_id;
		}
	}
	return num_units;
}

#define HOST_MEM_SIZE_UNIT 4
/*
 * allocate amount of memory requested by FW.
 */
static void wma_alloc_host_mem(tp_wma_handle wma_handle, u_int32_t req_id,
				u_int32_t num_units, u_int32_t unit_len)
{
	u_int32_t remaining_units,allocated_units, idx;

	/* adjust the length to nearest multiple of unit size */
	unit_len = (unit_len + (HOST_MEM_SIZE_UNIT - 1)) &
			(~(HOST_MEM_SIZE_UNIT - 1));
	idx = wma_handle->num_mem_chunks ;
	remaining_units = num_units;
	while(remaining_units) {
		allocated_units = wma_alloc_host_mem_chunk(wma_handle, req_id,
							   idx, remaining_units,
							   unit_len);
		if (allocated_units == 0) {
			WMA_LOGE("FAILED TO ALLOCATED memory unit len %d"
				" units requested %d units allocated %d ",
				unit_len, num_units,
				(num_units - remaining_units));
			wma_handle->num_mem_chunks = idx;
			break;
		}
		remaining_units -= allocated_units;
		++idx;
		if (idx == MAX_MEM_CHUNKS ) {
			WMA_LOGE("RWACHED MAX CHUNK LIMIT for memory units %d"
				" unit len %d requested by FW,"
				" only allocated %d ",
				num_units,unit_len,
				(num_units - remaining_units));
			wma_handle->num_mem_chunks = idx;
			break;
		}
	}
	wma_handle->num_mem_chunks = idx;
}

static inline void wma_update_target_services(tp_wma_handle wh,
					      struct hdd_tgt_services *cfg)
{
	/* STA power save */
	cfg->sta_power_save = WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
						     WMI_SERVICE_STA_PWRSAVE);

	/* Enable UAPSD */
	cfg->uapsd = WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
					    WMI_SERVICE_AP_UAPSD);

	/* Update AP DFS service */
	cfg->ap_dfs = WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
						    WMI_SERVICE_AP_DFS);

	/* Enable 11AC */
	cfg->en_11ac = WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
					      WMI_SERVICE_11AC);
        if (cfg->en_11ac)
		gFwWlanFeatCaps |= (1 << DOT11AC);

	/* Proactive ARP response */
	gFwWlanFeatCaps |= (1 << WLAN_PERIODIC_TX_PTRN);

	/* Enable WOW */
	gFwWlanFeatCaps |= (1 << WOW);

	/* ARP offload */
	cfg->arp_offload = WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
						  WMI_SERVICE_ARPNS_OFFLOAD);

	/* Adaptive early-rx */
	cfg->early_rx = WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
						WMI_SERVICE_EARLY_RX);
#ifdef FEATURE_WLAN_SCAN_PNO
	/* PNO offload */
	if (WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap, WMI_SERVICE_NLO))
		cfg->pno_offload = TRUE;
#endif

#ifdef FEATURE_WLAN_EXTSCAN
	if (WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
		WMI_SERVICE_EXTSCAN)) {
		gFwWlanFeatCaps |= (1 << EXTENDED_SCAN);
	}
#endif
	cfg->lte_coex_ant_share = WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
					WMI_SERVICE_LTE_ANT_SHARE_SUPPORT);
#ifdef FEATURE_WLAN_TDLS
	/* Enable TDLS */
	if (WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap, WMI_SERVICE_TDLS)) {
		cfg->en_tdls = 1;
		gFwWlanFeatCaps |= (1 << TDLS);
	}
	/* Enable advanced TDLS features */
	if (WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
                                   WMI_SERVICE_TDLS_OFFCHAN)) {
		cfg->en_tdls_offchan = 1;
		gFwWlanFeatCaps |= (1 << TDLS_OFF_CHANNEL);
	}

	cfg->en_tdls_uapsd_buf_sta = WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
	                                      WMI_SERVICE_TDLS_UAPSD_BUFFER_STA);
	cfg->en_tdls_uapsd_sleep_sta = WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
	                                      WMI_SERVICE_TDLS_UAPSD_SLEEP_STA);
#endif
	if (WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap, WMI_SERVICE_BEACON_OFFLOAD))
		cfg->beacon_offload = TRUE;
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	/* Enable Roam Offload */
	cfg->en_roam_offload = WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap,
	                                      WMI_SERVICE_ROAM_HO_OFFLOAD);
#endif
	if (WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap, WMI_SERVICE_NAN))
		gFwWlanFeatCaps |= (1 << NAN);

	if (WMI_SERVICE_IS_ENABLED(wh->wmi_service_bitmap, WMI_SERVICE_RTT))
		gFwWlanFeatCaps |= (1 << RTT);
}

static inline void wma_update_target_ht_cap(tp_wma_handle wh,
					    struct hdd_tgt_ht_cap *cfg)
{
	/* RX STBC */
	cfg->ht_rx_stbc = !!(wh->ht_cap_info & WMI_HT_CAP_RX_STBC);

	/* TX STBC */
	cfg->ht_tx_stbc = !!(wh->ht_cap_info & WMI_HT_CAP_TX_STBC);

	/* MPDU density */
	cfg->mpdu_density = wh->ht_cap_info & WMI_HT_CAP_MPDU_DENSITY;

	/* HT RX LDPC */
	cfg->ht_rx_ldpc = !!(wh->ht_cap_info & WMI_HT_CAP_LDPC);

	/* HT SGI */
	cfg->ht_sgi_20 = !!(wh->ht_cap_info & WMI_HT_CAP_HT20_SGI);

	cfg->ht_sgi_40 = !!(wh->ht_cap_info & WMI_HT_CAP_HT40_SGI);

	/* RF chains */
	cfg->num_rf_chains = wh->num_rf_chains;

        WMA_LOGD("%s: ht_cap_info - %x ht_rx_stbc - %d, ht_tx_stbc - %d\n\
                mpdu_density - %d ht_rx_ldpc - %d ht_sgi_20 - %d\n\
                ht_sgi_40 - %d num_rf_chains - %d ", __func__,
                wh->ht_cap_info, cfg->ht_rx_stbc, cfg->ht_tx_stbc,
                cfg->mpdu_density, cfg->ht_rx_ldpc, cfg->ht_sgi_20,
                cfg->ht_sgi_40, cfg->num_rf_chains);

}

#ifdef WLAN_FEATURE_11AC
static inline void wma_update_target_vht_cap(tp_wma_handle wh,
					     struct hdd_tgt_vht_cap *cfg)
{
	/* Max MPDU length */
	if (wh->vht_cap_info & IEEE80211_VHTCAP_MAX_MPDU_LEN_3839)
		cfg->vht_max_mpdu = 0;
	else if (wh->vht_cap_info & IEEE80211_VHTCAP_MAX_MPDU_LEN_7935)
		cfg->vht_max_mpdu = 1;
	else if (wh->vht_cap_info & IEEE80211_VHTCAP_MAX_MPDU_LEN_11454)
		cfg->vht_max_mpdu = 2;
	else
		cfg->vht_max_mpdu = 0;

	/* supported channel width */
	if (wh->vht_cap_info & IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80)
		cfg->supp_chan_width = 1 << eHT_CHANNEL_WIDTH_80MHZ;

	else if (wh->vht_cap_info & IEEE80211_VHTCAP_SUP_CHAN_WIDTH_160)
		cfg->supp_chan_width = 1 << eHT_CHANNEL_WIDTH_160MHZ;

	else if (wh->vht_cap_info & IEEE80211_VHTCAP_SUP_CHAN_WIDTH_80_160) {
		cfg->supp_chan_width = 1 << eHT_CHANNEL_WIDTH_80MHZ;
		cfg->supp_chan_width |= 1 << eHT_CHANNEL_WIDTH_160MHZ;
	}

	else
		cfg->supp_chan_width = 0;

	/* LDPC capability */
	cfg->vht_rx_ldpc = wh->vht_cap_info & IEEE80211_VHTCAP_RX_LDPC;

	/* Guard interval */
	cfg->vht_short_gi_80 = wh->vht_cap_info & IEEE80211_VHTCAP_SHORTGI_80;
	cfg->vht_short_gi_160 = wh->vht_cap_info & IEEE80211_VHTCAP_SHORTGI_160;

	/* TX STBC capability */
	cfg->vht_tx_stbc = wh->vht_cap_info & IEEE80211_VHTCAP_TX_STBC;

	/* RX STBC capability */
        cfg->vht_rx_stbc = wh->vht_cap_info & IEEE80211_VHTCAP_RX_STBC;

        cfg->vht_max_ampdu_len_exp = (wh->vht_cap_info &
                                     IEEE80211_VHTCAP_MAX_AMPDU_LEN_EXP)
                                      >> IEEE80211_VHTCAP_MAX_AMPDU_LEN_EXP_S;

	/* SU beamformer cap */
	cfg->vht_su_bformer = wh->vht_cap_info & IEEE80211_VHTCAP_SU_BFORMER;

	/* SU beamformee cap */
	cfg->vht_su_bformee = wh->vht_cap_info & IEEE80211_VHTCAP_SU_BFORMEE;

	/* MU beamformer cap */
	cfg->vht_mu_bformer = wh->vht_cap_info & IEEE80211_VHTCAP_MU_BFORMER;

	/* MU beamformee cap */
	cfg->vht_mu_bformee = wh->vht_cap_info & IEEE80211_VHTCAP_MU_BFORMEE;

	/* VHT Max AMPDU Len exp */
	cfg->vht_max_ampdu_len_exp = wh->vht_cap_info &
					IEEE80211_VHTCAP_MAX_AMPDU_LEN_EXP;

	/* VHT TXOP PS cap */
	cfg->vht_txop_ps = wh->vht_cap_info & IEEE80211_VHTCAP_TXOP_PS;

        WMA_LOGD(" %s: max_mpdu %d supp_chan_width %x rx_ldpc %x\n \
                short_gi_80 %x tx_stbc %x rx_stbc %x txop_ps %x\n \
                su_bformee %x mu_bformee %x max_ampdu_len_exp %d",
                __func__, cfg->vht_max_mpdu, cfg->supp_chan_width,
                cfg->vht_rx_ldpc, cfg->vht_short_gi_80, cfg->vht_tx_stbc,
                cfg->vht_rx_stbc, cfg->vht_txop_ps, cfg->vht_su_bformee,
                cfg->vht_mu_bformee, cfg->vht_max_ampdu_len_exp);
}
#endif	/* #ifdef WLAN_FEATURE_11AC */

static void wma_update_hdd_cfg(tp_wma_handle wma_handle)
{
	struct hdd_tgt_cfg hdd_tgt_cfg;
	void *hdd_ctx = vos_get_context(VOS_MODULE_ID_HDD,
					wma_handle->vos_context);

	hdd_tgt_cfg.reg_domain = wma_handle->reg_cap.eeprom_rd;
	hdd_tgt_cfg.eeprom_rd_ext = wma_handle->reg_cap.eeprom_rd_ext;

	switch (wma_handle->phy_capability) {
	case WMI_11G_CAPABILITY:
	case WMI_11NG_CAPABILITY:
		hdd_tgt_cfg.band_cap = eCSR_BAND_24;
		break;
	case WMI_11A_CAPABILITY:
	case WMI_11NA_CAPABILITY:
	case WMI_11AC_CAPABILITY:
		hdd_tgt_cfg.band_cap = eCSR_BAND_5G;
		break;
	case WMI_11AG_CAPABILITY:
	case WMI_11NAG_CAPABILITY:
	default:
		hdd_tgt_cfg.band_cap = eCSR_BAND_ALL;
	}

        hdd_tgt_cfg.max_intf_count = wma_handle->wlan_resource_config.num_vdevs;

	adf_os_mem_copy(hdd_tgt_cfg.hw_macaddr.bytes, wma_handle->hwaddr,
			ATH_MAC_LEN);

	wma_update_target_services(wma_handle, &hdd_tgt_cfg.services);
	wma_update_target_ht_cap(wma_handle, &hdd_tgt_cfg.ht_cap);
#ifdef WLAN_FEATURE_11AC
	wma_update_target_vht_cap(wma_handle, &hdd_tgt_cfg.vht_cap);
#endif	/* #ifdef WLAN_FEATURE_11AC */

	hdd_tgt_cfg.target_fw_version = wma_handle->target_fw_version;
#ifdef WLAN_FEATURE_LPSS
	hdd_tgt_cfg.lpss_support = wma_handle->lpss_support;
#endif
	wma_handle->tgt_cfg_update_cb(hdd_ctx, &hdd_tgt_cfg);
}
static wmi_buf_t wma_setup_wmi_init_msg(tp_wma_handle wma_handle,
				wmi_service_ready_event_fixed_param *ev,
				WMI_SERVICE_READY_EVENTID_param_tlvs *param_buf,
				v_SIZE_t *len)
{
	wmi_buf_t buf;
	wmi_init_cmd_fixed_param *cmd;
	wlan_host_mem_req *ev_mem_reqs;
	wmi_abi_version my_vers;
	int num_whitelist;
	u_int8_t *buf_ptr;
	wmi_resource_config *resource_cfg;
	wlan_host_memory_chunk *host_mem_chunks;
	u_int32_t mem_chunk_len = 0;
	u_int16_t idx;
	u_int32_t num_units;

	*len = sizeof(*cmd) + sizeof(wmi_resource_config) + WMI_TLV_HDR_SIZE;
	mem_chunk_len = (sizeof(wlan_host_memory_chunk) * MAX_MEM_CHUNKS);
	buf = wmi_buf_alloc(wma_handle->wmi_handle, *len + mem_chunk_len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		return NULL;
	}

	ev_mem_reqs = param_buf->mem_reqs;
	buf_ptr = (u_int8_t *) wmi_buf_data(buf);
	cmd = (wmi_init_cmd_fixed_param *) buf_ptr;
	resource_cfg = (wmi_resource_config *) (buf_ptr + sizeof(*cmd));
	host_mem_chunks = (wlan_host_memory_chunk*)
			  (buf_ptr + sizeof(*cmd) + sizeof(wmi_resource_config)
			  + WMI_TLV_HDR_SIZE);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_init_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_init_cmd_fixed_param));

	*resource_cfg = wma_handle->wlan_resource_config;
	WMITLV_SET_HDR(&resource_cfg->tlv_header,
		       WMITLV_TAG_STRUC_wmi_resource_config,
		       WMITLV_GET_STRUCT_TLVLEN(wmi_resource_config));

	/* allocate memory requested by FW */
	if (ev->num_mem_reqs > WMI_MAX_MEM_REQS) {
		VOS_ASSERT(0);
		adf_nbuf_free(buf);
		return NULL;
	}

	cmd->num_host_mem_chunks = 0;
	for(idx = 0; idx < ev->num_mem_reqs; ++idx) {
		num_units = ev_mem_reqs[idx].num_units;
		if  (ev_mem_reqs[idx].num_unit_info & NUM_UNITS_IS_NUM_PEERS) {
			/*
			 * number of units to allocate is number
			 * of peers, 1 extra for self peer on
			 * target. this needs to be fied, host
			 * and target can get out of sync
			 */
			num_units = resource_cfg->num_peers + 1;
		}
		WMA_LOGD("idx %d req %d  num_units %d num_unit_info %d unit size %d actual units %d ",
			 idx, ev_mem_reqs[idx].req_id,
			 ev_mem_reqs[idx].num_units,
			 ev_mem_reqs[idx].num_unit_info,
			 ev_mem_reqs[idx].unit_size,
			 num_units);
		wma_alloc_host_mem(wma_handle, ev_mem_reqs[idx].req_id,
				   num_units, ev_mem_reqs[idx].unit_size);
	}
	for(idx = 0; idx < wma_handle->num_mem_chunks; ++idx) {
		WMITLV_SET_HDR(&(host_mem_chunks[idx].tlv_header),
			WMITLV_TAG_STRUC_wlan_host_memory_chunk,
			WMITLV_GET_STRUCT_TLVLEN(wlan_host_memory_chunk));
		host_mem_chunks[idx].ptr = wma_handle->mem_chunks[idx].paddr;
		host_mem_chunks[idx].size = wma_handle->mem_chunks[idx].len;
		host_mem_chunks[idx].req_id =
				wma_handle->mem_chunks[idx].req_id;
		WMA_LOGD("chunk %d len %d requested ,ptr  0x%x ",
			 idx, host_mem_chunks[idx].size,
			 host_mem_chunks[idx].ptr) ;
	}
	cmd->num_host_mem_chunks = wma_handle->num_mem_chunks;
	len += (wma_handle->num_mem_chunks * sizeof(wlan_host_memory_chunk));
	WMITLV_SET_HDR((buf_ptr + sizeof(*cmd) + sizeof(wmi_resource_config)),
			WMITLV_TAG_ARRAY_STRUC,
			(sizeof(wlan_host_memory_chunk) *
			 wma_handle->num_mem_chunks));
	vos_mem_copy(&wma_handle->target_abi_vers,
		     &param_buf->fixed_param->fw_abi_vers,
		     sizeof(wmi_abi_version));
	num_whitelist = sizeof(version_whitelist) /
			sizeof(wmi_whitelist_version_info);
	my_vers.abi_version_0 = WMI_ABI_VERSION_0;
	my_vers.abi_version_1 = WMI_ABI_VERSION_1;
	my_vers.abi_version_ns_0 = WMI_ABI_VERSION_NS_0;
	my_vers.abi_version_ns_1 = WMI_ABI_VERSION_NS_1;
	my_vers.abi_version_ns_2 = WMI_ABI_VERSION_NS_2;
	my_vers.abi_version_ns_3 = WMI_ABI_VERSION_NS_3;

	wmi_cmp_and_set_abi_version(num_whitelist, version_whitelist,
			&my_vers, &param_buf->fixed_param->fw_abi_vers,
			&cmd->host_abi_vers);

	WMA_LOGD("%s: INIT_CMD version: %d, %d, 0x%x, 0x%x, 0x%x, 0x%x",
		 __func__, WMI_VER_GET_MAJOR(cmd->host_abi_vers.abi_version_0),
		 WMI_VER_GET_MINOR(cmd->host_abi_vers.abi_version_0),
		 cmd->host_abi_vers.abi_version_ns_0,
		 cmd->host_abi_vers.abi_version_ns_1,
		 cmd->host_abi_vers.abi_version_ns_2,
		 cmd->host_abi_vers.abi_version_ns_3);

	vos_mem_copy(&wma_handle->final_abi_vers, &cmd->host_abi_vers,
		     sizeof(wmi_abi_version));
	return buf;
}

/* Process service ready event and send wmi_init command */
v_VOID_t wma_rx_service_ready_event(WMA_HANDLE handle, void *cmd_param_info)
{
	wmi_buf_t buf;
	v_SIZE_t len;
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	struct wma_target_cap target_cap;
	WMI_SERVICE_READY_EVENTID_param_tlvs *param_buf;
	wmi_service_ready_event_fixed_param *ev;
	int status;

	WMA_LOGD("%s: Enter", __func__);

	param_buf = (WMI_SERVICE_READY_EVENTID_param_tlvs *) cmd_param_info;
	if (!(handle && param_buf)) {
		WMA_LOGP("%s: Invalid arguments", __func__);
		return;
	}

	ev = param_buf->fixed_param;
	if (!ev) {
		WMA_LOGP("%s: Invalid buffer", __func__);
		return;
	}

	WMA_LOGA("WMA <-- WMI_SERVICE_READY_EVENTID");

	wma_handle->phy_capability = ev->phy_capability;
	wma_handle->max_frag_entry = ev->max_frag_entry;
	wma_handle->num_rf_chains = ev->num_rf_chains;
	vos_mem_copy(&wma_handle->reg_cap, param_buf->hal_reg_capabilities,
		     sizeof(HAL_REG_CAPABILITIES));
	wma_handle->ht_cap_info = ev->ht_cap_info;
#ifdef WLAN_FEATURE_11AC
	wma_handle->vht_cap_info = ev->vht_cap_info;
        wma_handle->vht_supp_mcs = ev->vht_supp_mcs;
#endif
	wma_handle->num_rf_chains = ev->num_rf_chains;

	wma_handle->target_fw_version = ev->fw_build_vers;

	WMA_LOGE("%s: Firmware build version : %08x",
			__func__, ev->fw_build_vers);

	if (ev->hw_bd_id) {
		wma_handle->hw_bd_id = ev->hw_bd_id;
		vos_mem_copy(wma_handle->hw_bd_info,
				ev->hw_bd_info, sizeof(ev->hw_bd_info));

		WMA_LOGE("%s: Board version: %x.%x",
			__func__,
			wma_handle->hw_bd_info[0],
			wma_handle->hw_bd_info[1]);
	} else {
		wma_handle->hw_bd_id = 0;
		vos_mem_zero(wma_handle->hw_bd_info,
				sizeof(wma_handle->hw_bd_info));
		WMA_LOGE("%s: Board version is unknown!", __func__);
	}

	 /* TODO: Recheck below line to dump service ready event */
	 /* dbg_print_wmi_service_11ac(ev); */

	/* wmi service is ready */
	vos_mem_copy(wma_handle->wmi_service_bitmap,
		     param_buf->wmi_service_bitmap,
		     sizeof(wma_handle->wmi_service_bitmap));
	/* SWBA event handler for beacon transmission */
	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
						    WMI_HOST_SWBA_EVENTID,
						    wma_beacon_swba_handler);
	if (status) {
		WMA_LOGE("Failed to register swba beacon event cb");
		return;
	}

#ifdef WLAN_FEATURE_LPSS
	wma_handle->lpss_support =
		WMI_SERVICE_IS_ENABLED(wma_handle->wmi_service_bitmap,
				       WMI_SERVICE_LPASS);
#endif

	if (WMI_SERVICE_IS_ENABLED(wma_handle->wmi_service_bitmap,
				WMI_SERVICE_CSA_OFFLOAD)) {
		WMA_LOGD("%s: FW support CSA offload capability", __func__);
		status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
				WMI_CSA_HANDLING_EVENTID,
				wma_csa_offload_handler);
		if (status) {
			WMA_LOGE("Failed to register CSA offload event cb");
			return;
		}
	}

#ifdef WLAN_FEATURE_GTK_OFFLOAD
	if (WMI_SERVICE_IS_ENABLED(wma_handle->wmi_service_bitmap,
				   WMI_SERVICE_GTK_OFFLOAD)) {
		status = wmi_unified_register_event_handler(
						   wma_handle->wmi_handle,
						   WMI_GTK_OFFLOAD_STATUS_EVENTID,
						   wma_gtk_offload_status_event);
		if (status) {
			WMA_LOGE("Failed to register GTK offload event cb");
			return;
		}
	}
#endif

	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
							WMI_P2P_NOA_EVENTID,
							wma_p2p_noa_event_handler);
	if (status) {
		WMA_LOGE("Failed to register WMI_P2P_NOA_EVENTID callback");
		return;
	}
	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
						    WMI_TBTTOFFSET_UPDATE_EVENTID,
						    wma_tbttoffset_update_event_handler);
	if (status) {
		WMA_LOGE("Failed to register WMI_TBTTOFFSET_UPDATE_EVENTID callback");
		return;
	}

	/* Initialize the log supported event handler */
	status = wmi_unified_register_event_handler(wma_handle->wmi_handle,
			WMI_DIAG_EVENT_LOG_SUPPORTED_EVENTID,
			wma_log_supported_evt_handler);
	if (status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to register log supported event cb");
		return;
	}

	vos_mem_copy(target_cap.wmi_service_bitmap,
		     param_buf->wmi_service_bitmap,
		     sizeof(wma_handle->wmi_service_bitmap));
	target_cap.wlan_resource_config = wma_handle->wlan_resource_config;
	wma_update_fw_config(wma_handle, &target_cap);
	vos_mem_copy(wma_handle->wmi_service_bitmap, target_cap.wmi_service_bitmap,
		     sizeof(wma_handle->wmi_service_bitmap));
	wma_handle->wlan_resource_config = target_cap.wlan_resource_config;

	buf = wma_setup_wmi_init_msg(wma_handle, ev, param_buf, &len);
	if (!buf) {
		WMA_LOGE("Failed to setup buffer for wma init command");
		return;
	}

	WMA_LOGA("WMA --> WMI_INIT_CMDID");
	status = wmi_unified_cmd_send(wma_handle->wmi_handle, buf, len, WMI_INIT_CMDID);
	if (status != EOK) {
		WMA_LOGE("Failed to send WMI_INIT_CMDID command");
		wmi_buf_free(buf);
		return;
	}
}

/* function   : wma_rx_ready_event
 * Description :
 * Args       :
 * Retruns    :
 */
v_VOID_t wma_rx_ready_event(WMA_HANDLE handle, void *cmd_param_info)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	WMI_READY_EVENTID_param_tlvs *param_buf = NULL;
	wmi_ready_event_fixed_param  *ev = NULL;

	WMA_LOGD("%s: Enter", __func__);

	param_buf = (WMI_READY_EVENTID_param_tlvs *) cmd_param_info;
	if (!(wma_handle && param_buf)) {
		WMA_LOGP("%s: Invalid arguments", __func__);
		VOS_ASSERT(0);
		return;
	}

	WMA_LOGA("WMA <-- WMI_READY_EVENTID");

	ev = param_buf->fixed_param;
	/* Indicate to the waiting thread that the ready
	 * event was received */
	wma_handle->wmi_ready = TRUE;
	wma_handle->wlan_init_status = ev->status;

	/*
	 * We need to check the WMI versions and make sure both
	 * host and fw are compatible.
	 */
	if (!wmi_versions_are_compatible(&wma_handle->final_abi_vers,
					 &ev->fw_abi_vers)) {
		/*
		 * Error: Our host version and the given firmware version
		 * are incompatible.
		 */
		WMA_LOGE("%s: Error: Incompatible WMI version."
			"Host: %d,%d,0x%x 0x%x 0x%x 0x%x, FW: %d,%d,0x%x 0x%x 0x%x 0x%x",
			__func__,
			 WMI_VER_GET_MAJOR(
			 wma_handle->final_abi_vers.abi_version_0),
			 WMI_VER_GET_MINOR(
				 wma_handle->final_abi_vers.abi_version_0),
			 wma_handle->final_abi_vers.abi_version_ns_0,
			 wma_handle->final_abi_vers.abi_version_ns_1,
			 wma_handle->final_abi_vers.abi_version_ns_2,
			 wma_handle->final_abi_vers.abi_version_ns_3,
			 WMI_VER_GET_MAJOR(ev->fw_abi_vers.abi_version_0),
			 WMI_VER_GET_MINOR(ev->fw_abi_vers.abi_version_0),
			 ev->fw_abi_vers.abi_version_ns_0,
			 ev->fw_abi_vers.abi_version_ns_1,
			 ev->fw_abi_vers.abi_version_ns_2,
			 ev->fw_abi_vers.abi_version_ns_3);
		if (wma_handle->wlan_init_status == WLAN_INIT_STATUS_SUCCESS) {
			/* Failed this connection to FW */
			wma_handle->wlan_init_status =
						WLAN_INIT_STATUS_GEN_FAILED;
		}
	}
	vos_mem_copy(&wma_handle->final_abi_vers, &ev->fw_abi_vers,
		     sizeof(wmi_abi_version));
	vos_mem_copy(&wma_handle->target_abi_vers, &ev->fw_abi_vers,
		     sizeof(wmi_abi_version));

	/* copy the mac addr */
	WMI_MAC_ADDR_TO_CHAR_ARRAY (&ev->mac_addr, wma_handle->myaddr);
	WMI_MAC_ADDR_TO_CHAR_ARRAY (&ev->mac_addr, wma_handle->hwaddr);

	wma_update_hdd_cfg(wma_handle);

	vos_event_set(&wma_handle->wma_ready_event);

	wma_set_dfs_regdomain(wma_handle);

	WMA_LOGD("Exit");
}

int wma_set_peer_param(void *wma_ctx, u_int8_t *peer_addr, u_int32_t param_id,
		       u_int32_t param_value, u_int32_t vdev_id)
{
	tp_wma_handle wma_handle = (tp_wma_handle) wma_ctx;
	wmi_peer_set_param_cmd_fixed_param *cmd;
	wmi_buf_t buf;
	int err;

	buf = wmi_buf_alloc(wma_handle->wmi_handle, sizeof(*cmd));
	if (!buf) {
		WMA_LOGE("Failed to allocate buffer to send set_param cmd");
		return -ENOMEM;
	}
	cmd = (wmi_peer_set_param_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_peer_set_param_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(
				wmi_peer_set_param_cmd_fixed_param));
	cmd->vdev_id = vdev_id;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(peer_addr, &cmd->peer_macaddr);
	cmd->param_id = param_id;
	cmd->param_value = param_value;
	err = wmi_unified_cmd_send(wma_handle->wmi_handle, buf,
				   sizeof(wmi_peer_set_param_cmd_fixed_param),
				   WMI_PEER_SET_PARAM_CMDID);
	if (err) {
		WMA_LOGE("Failed to send set_param cmd");
		adf_os_mem_free(buf);
		return -EIO;
	}

	return 0;
}

static void
wma_decap_to_8023 (adf_nbuf_t msdu, struct wma_decap_info_t *info)
{
	struct llc_snap_hdr_t *llc_hdr;
	u_int16_t ether_type;
	u_int16_t l2_hdr_space;
	struct ieee80211_qosframe_addr4 *wh;
	u_int8_t local_buf[ETHERNET_HDR_LEN];
	u_int8_t *buf;
	struct ethernet_hdr_t *ethr_hdr;

	buf = (u_int8_t *)adf_nbuf_data(msdu);
	llc_hdr = (struct llc_snap_hdr_t *)buf;
	ether_type = (llc_hdr->ethertype[0] << 8)|llc_hdr->ethertype[1];
	/* do llc remove if needed */
	l2_hdr_space = 0;
	if (IS_SNAP(llc_hdr)) {
		if (IS_BTEP(llc_hdr)) {
			/* remove llc*/
			l2_hdr_space += sizeof(struct llc_snap_hdr_t);
			llc_hdr = NULL;
		} else if (IS_RFC1042(llc_hdr)) {
			if (!(ether_type == ETHERTYPE_AARP ||
				ether_type == ETHERTYPE_IPX)) {
				/* remove llc*/
				l2_hdr_space += sizeof(struct llc_snap_hdr_t);
				llc_hdr = NULL;
			}
		}
	}
	if (l2_hdr_space > ETHERNET_HDR_LEN) {
		buf = adf_nbuf_pull_head(msdu, l2_hdr_space - ETHERNET_HDR_LEN);
	} else if (l2_hdr_space <  ETHERNET_HDR_LEN) {
		buf = adf_nbuf_push_head(msdu, ETHERNET_HDR_LEN - l2_hdr_space);
	}

	/* mpdu hdr should be present in info,re-create ethr_hdr based on mpdu hdr*/
	wh = (struct ieee80211_qosframe_addr4 *)info->hdr;
	ethr_hdr = (struct ethernet_hdr_t *)local_buf;
	switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
		case IEEE80211_FC1_DIR_NODS:
			adf_os_mem_copy(ethr_hdr->dest_addr, wh->i_addr1,
							ETHERNET_ADDR_LEN);
			adf_os_mem_copy(ethr_hdr->src_addr, wh->i_addr2,
							ETHERNET_ADDR_LEN);
			break;
		case IEEE80211_FC1_DIR_TODS:
			adf_os_mem_copy(ethr_hdr->dest_addr, wh->i_addr3,
							ETHERNET_ADDR_LEN);
			adf_os_mem_copy(ethr_hdr->src_addr, wh->i_addr2,
							ETHERNET_ADDR_LEN);
			break;
		case IEEE80211_FC1_DIR_FROMDS:
			adf_os_mem_copy(ethr_hdr->dest_addr, wh->i_addr1,
							ETHERNET_ADDR_LEN);
			adf_os_mem_copy(ethr_hdr->src_addr, wh->i_addr3,
							ETHERNET_ADDR_LEN);
			break;
		case IEEE80211_FC1_DIR_DSTODS:
			adf_os_mem_copy(ethr_hdr->dest_addr, wh->i_addr3,
							ETHERNET_ADDR_LEN);
			adf_os_mem_copy(ethr_hdr->src_addr, wh->i_addr4,
							ETHERNET_ADDR_LEN);
			break;
	}

	if (llc_hdr == NULL) {
		ethr_hdr->ethertype[0] = (ether_type >> 8) & 0xff;
		ethr_hdr->ethertype[1] = (ether_type) & 0xff;
	} else {
		u_int32_t pktlen = adf_nbuf_len(msdu) - sizeof(ethr_hdr->ethertype);
		ether_type = (u_int16_t)pktlen;
		ether_type = adf_nbuf_len(msdu) - sizeof(struct ethernet_hdr_t);
		ethr_hdr->ethertype[0] = (ether_type >> 8) & 0xff;
		ethr_hdr->ethertype[1] = (ether_type) & 0xff;
	}
	adf_os_mem_copy(buf, ethr_hdr, ETHERNET_HDR_LEN);
}

static int32_t
wma_ieee80211_hdrsize(const void *data)
{
	const struct ieee80211_frame *wh = (const struct ieee80211_frame *)data;
	int32_t size = sizeof(struct ieee80211_frame);

	if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS)
		size += IEEE80211_ADDR_LEN;
	if (IEEE80211_QOS_HAS_SEQ(wh))
		size += sizeof(u_int16_t);
	return size;
}

/**
  * WDA_TxPacket - Sends Tx Frame to TxRx
  * This function sends the frame corresponding to the
  * given vdev id.
  * This is blocking call till the downloading of frame is complete.
  */
VOS_STATUS WDA_TxPacket(void *wma_context, void *tx_frame, u_int16_t frmLen,
			eFrameType frmType, eFrameTxDir txDir, u_int8_t tid,
			pWDATxRxCompFunc tx_frm_download_comp_cb, void *pData,
			pWDAAckFnTxComp tx_frm_ota_comp_cb, u_int8_t tx_flag,
			u_int8_t vdev_id, bool tdlsFlag)
{
	tp_wma_handle wma_handle = (tp_wma_handle)(wma_context);
	int32_t status;
	VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
	int32_t is_high_latency;
	ol_txrx_vdev_handle txrx_vdev;
	enum frame_index tx_frm_index =
		GENERIC_NODOWNLD_NOACK_COMP_INDEX;
	tpSirMacFrameCtl pFc = (tpSirMacFrameCtl)(adf_nbuf_data(tx_frame));
	u_int8_t use_6mbps = 0;
	u_int8_t downld_comp_required = 0;
	u_int16_t chanfreq;
#ifdef WLAN_FEATURE_11W
	tANI_U8         *pFrame = NULL;
	void            *pPacket = NULL;
	u_int16_t	newFrmLen = 0;
#endif /* WLAN_FEATURE_11W */
	struct wma_txrx_node *iface;
	tpAniSirGlobal pMac;
#ifdef QCA_PKT_PROTO_TRACE
	v_U8_t proto_type = 0;
#endif

        if (NULL == wma_handle)
        {
            WMA_LOGE("wma_handle is NULL");
            return VOS_STATUS_E_FAILURE;
        }
        iface = &wma_handle->interfaces[vdev_id];
        pMac = (tpAniSirGlobal)vos_get_context(VOS_MODULE_ID_PE,
                wma_handle->vos_context);
	/* Get the vdev handle from vdev id */
	txrx_vdev = wma_handle->interfaces[vdev_id].handle;

	if(!txrx_vdev) {
		WMA_LOGE("TxRx Vdev Handle is NULL");
		return VOS_STATUS_E_FAILURE;
	}

#if defined(CONFIG_HL_SUPPORT) && defined(FEATURE_WLAN_TDLS)
        txrx_vdev->hlTdlsFlag = false;
#endif

        if (frmType >= HAL_TXRX_FRM_MAX) {
		WMA_LOGE("Invalid Frame Type Fail to send Frame");
		return VOS_STATUS_E_FAILURE;
	}

	if(!pMac) {
		WMA_LOGE("pMac Handle is NULL");
		return VOS_STATUS_E_FAILURE;
	}
	/*
	 * Currently only support to
	 * send 80211 Mgmt and 80211 Data are added.
	 */
	if (!((frmType == HAL_TXRX_FRM_802_11_MGMT) ||
		 (frmType == HAL_TXRX_FRM_802_11_DATA))) {
		WMA_LOGE("No Support to send other frames except 802.11 Mgmt/Data");
		return VOS_STATUS_E_FAILURE;
	}
#ifdef WLAN_FEATURE_11W
	if ((iface && iface->rmfEnabled) &&
		(frmType == HAL_TXRX_FRM_802_11_MGMT) &&
		(pFc->subType == SIR_MAC_MGMT_DISASSOC ||
			pFc->subType == SIR_MAC_MGMT_DEAUTH ||
			pFc->subType == SIR_MAC_MGMT_ACTION)) {
		struct ieee80211_frame *wh =
				(struct ieee80211_frame *)adf_nbuf_data(tx_frame);
		if(!IEEE80211_IS_BROADCAST(wh->i_addr1) &&
		   !IEEE80211_IS_MULTICAST(wh->i_addr1)) {
			if (pFc->wep) {
				/* Allocate extra bytes for privacy header and trailer */
				newFrmLen = frmLen + IEEE80211_CCMP_HEADERLEN +
							IEEE80211_CCMP_MICLEN;
				vos_status = palPktAlloc( pMac->hHdd,
							  HAL_TXRX_FRM_802_11_MGMT,
							  ( tANI_U16 )newFrmLen,
							  ( void** ) &pFrame,
							  ( void** ) &pPacket );

				if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
					WMA_LOGP("%s: Failed to allocate %d bytes for RMF status "
							 "code (%x)", __func__, newFrmLen, vos_status);
					/* Free the original packet memory */
					palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
								( void* ) pData, ( void* ) tx_frame );
					goto error;
				}

				/*
				 * Initialize the frame with 0's and only fill
				 * MAC header and data, Keep the CCMP header and
				 * trailer as 0's, firmware shall fill this
				 */
				vos_mem_set(  pFrame, newFrmLen , 0 );
				vos_mem_copy( pFrame, wh, sizeof(*wh));
				vos_mem_copy( pFrame + sizeof(*wh) + IEEE80211_CCMP_HEADERLEN,
							  pData + sizeof(*wh), frmLen - sizeof(*wh));

				palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
							( void* ) pData, ( void* ) tx_frame );
				tx_frame = pPacket;
				frmLen = newFrmLen;
			}
		} else {
			/* Allocate extra bytes for MMIE */
			newFrmLen = frmLen + IEEE80211_MMIE_LEN;
			vos_status = palPktAlloc( pMac->hHdd,
						  HAL_TXRX_FRM_802_11_MGMT,
						  ( tANI_U16 )newFrmLen,
						  ( void** ) &pFrame,
						  ( void** ) &pPacket );

			if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
				WMA_LOGP("%s: Failed to allocate %d bytes for RMF status "
						 "code (%x)", __func__, newFrmLen, vos_status);
				/* Free the original packet memory */
				palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
							( void* ) pData, ( void* ) tx_frame );
				goto error;
			}
			/*
			 * Initialize the frame with 0's and only fill
			 * MAC header and data. MMIE field will be
			 * filled by vos_attach_mmie API
			 */
			vos_mem_set(  pFrame, newFrmLen , 0 );
			vos_mem_copy( pFrame, wh, sizeof(*wh));
			vos_mem_copy( pFrame + sizeof(*wh),
						  pData + sizeof(*wh), frmLen - sizeof(*wh));
			if (!vos_attach_mmie(iface->key.key,
					iface->key.key_id[0].ipn,
					WMA_IGTK_KEY_INDEX_4,
					pFrame,
					pFrame+newFrmLen, newFrmLen)) {
				WMA_LOGP("%s: Failed to attach MMIE at the end of "
						 "frame", __func__);
				/* Free the original packet memory */
				palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
					( void* ) pData, ( void* ) tx_frame );
					goto error;
			}
			palPktFree( pMac->hHdd, HAL_TXRX_FRM_802_11_MGMT,
						( void* ) pData, ( void* ) tx_frame );
			tx_frame = pPacket;
			frmLen = newFrmLen;
		}
	}
#endif /* WLAN_FEATURE_11W */

	if ((frmType == HAL_TXRX_FRM_802_11_MGMT) &&
	    (pFc->subType == SIR_MAC_MGMT_PROBE_RSP)) {
		u_int64_t adjusted_tsf_le;
		struct ieee80211_frame *wh =
			(struct ieee80211_frame *)adf_nbuf_data(tx_frame);

		/* Make the TSF offset negative to match TSF in beacons */
		adjusted_tsf_le = cpu_to_le64(0ULL -
				wma_handle->interfaces[vdev_id].tsfadjust);
		A_MEMCPY(&wh[1], &adjusted_tsf_le, sizeof(adjusted_tsf_le));
	}
	if (frmType == HAL_TXRX_FRM_802_11_DATA) {
		adf_nbuf_t ret;
		adf_nbuf_t skb = (adf_nbuf_t)tx_frame;
		ol_txrx_pdev_handle pdev =
		vos_get_context(VOS_MODULE_ID_TXRX, wma_handle->vos_context);

		struct wma_decap_info_t decap_info;
		struct ieee80211_frame *wh =
			(struct ieee80211_frame *)adf_nbuf_data(skb);
		v_TIME_t curr_timestamp = vos_timer_get_system_ticks();

		if (pdev == NULL) {
			WMA_LOGE("%s: pdev pointer is not available", __func__);
			return VOS_STATUS_E_FAULT;
		}

		/*
		 * 1) TxRx Module expects data input to be 802.3 format
		 * So Decapsulation has to be done.
		 * 2) Only one Outstanding Data pending for Ack is allowed
		 */
		if (tx_frm_ota_comp_cb) {
			if (wma_handle->umac_data_ota_ack_cb) {
				/*
				 * If last data frame was sent more than 5 seconds
				 * ago and still we did not receive ack/nack from
				 * fw then allow Tx of this data frame
				 */
				if (curr_timestamp >=
					 wma_handle->last_umac_data_ota_timestamp + 500) {
					WMA_LOGE("%s: No Tx Ack for last data frame for more than 5 secs, allow Tx of current data frame",
					 __func__);
			 } else {
					WMA_LOGE("%s: Already one Data pending for Ack, reject Tx of data frame",
					 __func__);
					return VOS_STATUS_E_FAILURE;
				}
			}
		} else {
			/*
			 * Data Frames are sent through TxRx Non Standard Data Path
			 * so Ack Complete Cb is must
			 */
			WMA_LOGE("No Ack Complete Cb. Don't Allow");
			return VOS_STATUS_E_FAILURE;
		}

		/* Take out 802.11 header from skb */
		decap_info.hdr_len = wma_ieee80211_hdrsize(wh);
		adf_os_mem_copy(decap_info.hdr, wh, decap_info.hdr_len);
		adf_nbuf_pull_head(skb, decap_info.hdr_len);

		/*  Decapsulate to 802.3 format */
		wma_decap_to_8023(skb, &decap_info);

		/* Zero out skb's context buffer for the driver to use */
		adf_os_mem_set(skb->cb, 0, sizeof(skb->cb));

		/* Do the DMA Mapping */
		adf_nbuf_map_single(pdev->osdev, skb, ADF_OS_DMA_TO_DEVICE);

		/* Terminate the (single-element) list of tx frames */
		skb->next = NULL;

		/* Store the Ack Complete Cb */
		wma_handle->umac_data_ota_ack_cb = tx_frm_ota_comp_cb;

		/* Store the timestamp and nbuf for this data Tx */
		wma_handle->last_umac_data_ota_timestamp = curr_timestamp;
		wma_handle->last_umac_data_nbuf = skb;

		/* Send the Data frame to TxRx in Non Standard Path */
#if defined(CONFIG_HL_SUPPORT) && defined(FEATURE_WLAN_TDLS)
                txrx_vdev->hlTdlsFlag = tdlsFlag;
#endif
                ret = ol_tx_non_std(txrx_vdev, ol_tx_spec_no_free, skb);
#if defined(CONFIG_HL_SUPPORT) && defined(FEATURE_WLAN_TDLS)
                txrx_vdev->hlTdlsFlag = false;
#endif
                if (ret) {
			WMA_LOGE("TxRx Rejected. Fail to do Tx");
			adf_nbuf_unmap_single(pdev->osdev, skb, ADF_OS_DMA_TO_DEVICE);
			/* Call Download Cb so that umac can free the buffer */
			if (tx_frm_download_comp_cb)
				tx_frm_download_comp_cb(wma_handle->mac_context,
						tx_frame,
						WMA_TX_FRAME_BUFFER_FREE);
			wma_handle->umac_data_ota_ack_cb = NULL;
			wma_handle->last_umac_data_nbuf = NULL;
			return VOS_STATUS_E_FAILURE;
		}

		/* Call Download Callback if passed */
		if (tx_frm_download_comp_cb)
			tx_frm_download_comp_cb(wma_handle->mac_context,
						tx_frame,
						WMA_TX_FRAME_BUFFER_NO_FREE);

		return VOS_STATUS_SUCCESS;
	}

	is_high_latency = wdi_out_cfg_is_high_latency(
				txrx_vdev->pdev->ctrl_pdev);

	downld_comp_required = tx_frm_download_comp_cb && is_high_latency;

	/* Fill the frame index to send */
	if(pFc->type == SIR_MAC_MGMT_FRAME) {
		if(tx_frm_ota_comp_cb) {
			if(downld_comp_required)
				tx_frm_index =
					GENERIC_DOWNLD_COMP_ACK_COMP_INDEX;
			else
				tx_frm_index =
					GENERIC_NODOWLOAD_ACK_COMP_INDEX;

			/* Store the Ack Cb sent by UMAC */
			if(pFc->subType < SIR_MAC_MGMT_RESERVED15) {
				wma_handle->umac_ota_ack_cb[pFc->subType] =
							tx_frm_ota_comp_cb;
			}
#ifdef QCA_PKT_PROTO_TRACE
			if (pFc->subType == SIR_MAC_MGMT_ACTION)
				proto_type = vos_pkt_get_proto_type(tx_frame,
						pMac->fEnableDebugLog,
						NBUF_PKT_TRAC_TYPE_MGMT_ACTION);
			if (proto_type & NBUF_PKT_TRAC_TYPE_MGMT_ACTION)
				vos_pkt_trace_buf_update("WM:T:MACT");
			adf_nbuf_trace_set_proto_type(tx_frame, proto_type);
#endif /* QCA_PKT_PROTO_TRACE */
		} else {
			if(downld_comp_required)
				tx_frm_index =
					GENERIC_DOWNLD_COMP_NOACK_COMP_INDEX;
			else
				tx_frm_index =
					GENERIC_NODOWNLD_NOACK_COMP_INDEX;
		}
	}

	/*
	 * If Dowload Complete is required
	 * Wait for download complete
	 */
	if(downld_comp_required) {
		/* Store Tx Comp Cb */
		wma_handle->tx_frm_download_comp_cb = tx_frm_download_comp_cb;

		/* Reset the Tx Frame Complete Event */
		vos_status  = vos_event_reset(
				&wma_handle->tx_frm_download_comp_event);

		if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
			WMA_LOGP("%s: Event Reset failed tx comp event %x",
					__func__, vos_status);
			goto error;
		}
	}

	/* If the frame has to be sent at BD Rate2 inform TxRx */
	if(tx_flag & HAL_USE_BD_RATE2_FOR_MANAGEMENT_FRAME)
		use_6mbps = 1;

        if (wma_handle->roam_preauth_scan_state == WMA_ROAM_PREAUTH_ON_CHAN) {
                chanfreq = wma_handle->roam_preauth_chanfreq;
                WMA_LOGI("%s: Preauth frame on channel %d", __func__, chanfreq);
        } else if(pFc->subType == SIR_MAC_MGMT_PROBE_RSP){
		chanfreq = wma_handle->interfaces[vdev_id].mhz;
                WMA_LOGI("%s: Probe response frame on channel %d", __func__, chanfreq);
                WMA_LOGI("%s: Probe response frame on vdev id %d", __func__, vdev_id);
	} else {
                chanfreq = 0;
        }
        if (pMac->fEnableDebugLog & 0x1) {
          if ((pFc->type == SIR_MAC_MGMT_FRAME) &&
              (pFc->subType != SIR_MAC_MGMT_PROBE_REQ) &&
              (pFc->subType != SIR_MAC_MGMT_PROBE_RSP)) {
              WMA_LOGE("TX MGMT - Type %hu, SubType %hu",
              pFc->type, pFc->subType);
          }
        }
	/* Hand over the Tx Mgmt frame to TxRx */
	status = wdi_in_mgmt_send(txrx_vdev, tx_frame, tx_frm_index, use_6mbps, chanfreq);

	/*
	 * Failed to send Tx Mgmt Frame
	 */
	if (status) {
	/* Call Download Cb so that umac can free the buffer */
		if (tx_frm_download_comp_cb)
			tx_frm_download_comp_cb(wma_handle->mac_context,
						tx_frame,
						WMA_TX_FRAME_BUFFER_FREE);
		WMA_LOGP("%s: Failed to send Mgmt Frame", __func__);
		goto error;
	}

	if (!tx_frm_download_comp_cb)
		return VOS_STATUS_SUCCESS;

	/*
	 * Wait for Download Complete
	 * if required
	 */
	if (downld_comp_required) {
		/*
		 * Wait for Download Complete
		 * @ Integrated : Dxe Complete
		 * @ Discrete : Target Download Complete
		 */
		vos_status = vos_wait_single_event(
				&wma_handle->tx_frm_download_comp_event,
				WMA_TX_FRAME_COMPLETE_TIMEOUT);

		if (!VOS_IS_STATUS_SUCCESS(vos_status)) {
			WMA_LOGP("Wait Event failed txfrm_comp_event");
			/*
			 * @Integrated: Something Wrong with Dxe
			 *   TODO: Some Debug Code
			 * Here We need to trigger SSR since
			 * since system went into a bad state where
			 * we didn't get Download Complete for almost
			 * WMA_TX_FRAME_COMPLETE_TIMEOUT (1 sec)
			 */
		}
	} else {
		/*
		 * For Low Latency Devices
		 * Call the download complete
		 * callback once the frame is successfully
		 * given to txrx module
		 */
		tx_frm_download_comp_cb(wma_handle->mac_context, tx_frame,
					WMA_TX_FRAME_BUFFER_NO_FREE);
	}

	return VOS_STATUS_SUCCESS;

error:
	wma_handle->tx_frm_download_comp_cb = NULL;
	return VOS_STATUS_E_FAILURE;
}

/* function   :wma_setneedshutdown
 * Description :
 * Args       :
 * Returns    :
 */
v_VOID_t wma_setneedshutdown(v_VOID_t *vos_ctx)
{
	tp_wma_handle wma_handle;

	WMA_LOGD("%s: Enter", __func__);

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);

	if (NULL == wma_handle) {
		WMA_LOGP("%s: Invalid arguments", __func__);
		VOS_ASSERT(0);
		return;
        }

	wma_handle->needShutdown  = TRUE;
	WMA_LOGD("%s: Exit", __func__);
}

/* function   : wma_rx_ready_event
 * Description :
 * Args       :
 * Returns    :
 */
 v_BOOL_t wma_needshutdown(v_VOID_t *vos_ctx)
 {
	tp_wma_handle wma_handle;

	WMA_LOGD("%s: Enter", __func__);

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);

	if (NULL == wma_handle) {
		WMA_LOGP("%s: Invalid arguments", __func__);
		VOS_ASSERT(0);
		return 0;
        }

	WMA_LOGD("%s: Exit", __func__);
	return wma_handle->needShutdown;
}

VOS_STATUS wma_wait_for_ready_event(WMA_HANDLE handle)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	VOS_STATUS vos_status;

	/* wait until WMI_READY_EVENTID received from FW */
	vos_status = vos_wait_single_event( &(wma_handle->wma_ready_event),
			WMA_READY_EVENTID_TIMEOUT );

	if (VOS_STATUS_SUCCESS != vos_status) {
		WMA_LOGP("%s: Timeout waiting for ready event from FW", __func__);
		vos_status = VOS_STATUS_E_FAILURE;
	}
	return vos_status;
}

int wma_suspend_target(WMA_HANDLE handle, int disable_target_intr)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	wmi_pdev_suspend_cmd_fixed_param* cmd;
	wmi_buf_t wmibuf;
	u_int32_t len = sizeof(*cmd);
	struct ol_softc *scn;
#ifdef CONFIG_CNSS
	tpAniSirGlobal pmac;
#endif

	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("WMA is closed. can not issue suspend cmd");
		return -EINVAL;
	}

#ifdef CONFIG_CNSS
	pmac = vos_get_context(VOS_MODULE_ID_PE, wma_handle->vos_context);
	if (!pmac) {
		WMA_LOGE("%s: Unable to get PE context!", __func__);
		return -EINVAL;
	}
#endif

	/*
	 * send the comand to Target to ignore the
	 * PCIE reset so as to ensure that Host and target
	 * states are in sync
	 */
	wmibuf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (wmibuf == NULL) {
		return -1;
	}

	cmd = (wmi_pdev_suspend_cmd_fixed_param *) wmi_buf_data(wmibuf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_pdev_suspend_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
			       wmi_pdev_suspend_cmd_fixed_param));
	if (disable_target_intr) {
		cmd->suspend_opt = WMI_PDEV_SUSPEND_AND_DISABLE_INTR;
	}
	else {
		cmd->suspend_opt = WMI_PDEV_SUSPEND;
	}
	vos_event_reset(&wma_handle->target_suspend);
	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmibuf, len,
				    WMI_PDEV_SUSPEND_CMDID)) {
		adf_nbuf_free(wmibuf);
		return -1;
	}


	wmi_set_target_suspend(wma_handle->wmi_handle, TRUE);

	if (vos_wait_single_event(&wma_handle->target_suspend,
				  WMA_TGT_SUSPEND_COMPLETE_TIMEOUT)
				  != VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to get ACK from firmware for pdev suspend");
		wmi_set_target_suspend(wma_handle->wmi_handle, FALSE);
#ifdef CONFIG_CNSS
		if (!vos_is_logp_in_progress(VOS_MODULE_ID_VOSS, NULL)) {
			if (pmac->sme.enableSelfRecovery) {
				vos_trigger_recovery();
			} else {
				VOS_BUG(0);
			}
		} else {
			WMA_LOGE("%s: LOGP is in progress, ignore!", __func__);
		}
#endif
		return -1;
	}

	scn = vos_get_context(VOS_MODULE_ID_HIF,wma_handle->vos_context);

	if (scn == NULL) {
		WMA_LOGE("%s: Failed to get HIF context", __func__);
		VOS_ASSERT(0);
		return -1;
	}

	HTCCancelDeferredTargetSleep(scn);

	return 0;
}

void wma_target_suspend_acknowledge(void *context)
{
	void *vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
	tp_wma_handle wma = vos_get_context(VOS_MODULE_ID_WDA, vos_context);
	int wow_nack = *((int *)context);

	if (NULL == wma) {
		WMA_LOGE("%s: wma is NULL", __func__);
		return;
	}

	wma->wow_nack = wow_nack;
	vos_event_set(&wma->target_suspend);
	if (wow_nack)
		vos_wake_lock_timeout_acquire(&wma->wow_wake_lock, WMA_WAKE_LOCK_TIMEOUT,
					      WIFI_POWER_EVENT_WAKELOCK_WOW);
}

int wma_resume_target(WMA_HANDLE handle, int runtime_pm)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	wmi_buf_t wmibuf;
	wmi_pdev_resume_cmd_fixed_param *cmd;
	int ret;
	int timeout = 0;
	int wmi_pending_cmds;

	wmibuf = wmi_buf_alloc(wma_handle->wmi_handle, sizeof(*cmd));
	if (wmibuf == NULL) {
		return  -1;
	}
	cmd = (wmi_pdev_resume_cmd_fixed_param *) wmi_buf_data(wmibuf);
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_pdev_resume_cmd_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_pdev_resume_cmd_fixed_param));
	cmd->reserved0 = 0;
	ret = wmi_unified_cmd_send(wma_handle->wmi_handle, wmibuf, sizeof(*cmd),
				WMI_PDEV_RESUME_CMDID);
	if(ret != EOK) {
		WMA_LOGE("Failed to send WMI_PDEV_RESUME_CMDID command");
		wmi_buf_free(wmibuf);
	}

	if (runtime_pm)
		goto end;

	wmi_pending_cmds = wmi_get_pending_cmds(wma_handle->wmi_handle);
	while (wmi_pending_cmds && timeout++ < WMA_MAX_RESUME_RETRY) {
		msleep(10);
		wmi_pending_cmds = wmi_get_pending_cmds(wma_handle->wmi_handle);
	}

	if (wmi_pending_cmds) {
		WMA_LOGE("Failed to deliver WMI_PDEV_RESUME_CMDID command %d\n", timeout);
		ret = -1;
	}

end:
	if (EOK == ret)
		wmi_set_target_suspend(wma_handle->wmi_handle, FALSE);

	return ret;
}

void WDA_TimerTrafficStatsInd(tWDA_CbContext *pWDA)
{
}
/* TODO: Below is stub should be removed later */
void WDI_DS_ActivateTrafficStats(void)
{
}
/*
 * Function fills the rx packet meta info from the the vos packet
 */
VOS_STATUS WDA_DS_PeekRxPacketInfo(vos_pkt_t *pkt, v_PVOID_t *pkt_meta,
					v_BOOL_t  bSwap)
{
	/* Sanity Check */
	if(pkt == NULL) {
		WMA_LOGE("wma:Invalid parameter sent on wma_peek_rx_pkt_info");
		return VOS_STATUS_E_FAULT;
	}

	*pkt_meta = &(pkt->pkt_meta);

	return VOS_STATUS_SUCCESS;
}

/*
 * Function to lookup MAC address from vdev ID
 */
u_int8_t *wma_get_vdev_address_by_vdev_id(u_int8_t vdev_id)
{
	tp_wma_handle wma;

	wma = vos_get_context(VOS_MODULE_ID_WDA,
			      vos_get_global_context(VOS_MODULE_ID_WDA, NULL));
	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return NULL;
	}

	if (vdev_id >= wma->max_bssid) {
		WMA_LOGE("%s: Invalid vdev_id %u", __func__, vdev_id);
		return NULL;
	}

	return wma->interfaces[vdev_id].addr;
}

/*
 * Function to get the beacon buffer from vdev ID
 * Note: The buffer returned must be freed explicitly by caller
 */
void *wma_get_beacon_buffer_by_vdev_id(u_int8_t vdev_id, u_int32_t *buffer_size)
{
	tp_wma_handle wma;
	struct beacon_info *beacon;
	u_int8_t *buf;
	u_int32_t buf_size;

	wma = vos_get_context(VOS_MODULE_ID_WDA,
			      vos_get_global_context(VOS_MODULE_ID_WDA, NULL));

	if (!wma) {
		WMA_LOGE("%s: Invalid WMA handle", __func__);
		return NULL;
	}

	if (vdev_id >= wma->max_bssid) {
		WMA_LOGE("%s: Invalid vdev_id %u", __func__, vdev_id);
		return NULL;
	}

	if (!wma_is_vdev_in_ap_mode(wma, vdev_id)) {
		WMA_LOGE("%s: vdevid %d is not in AP mode",
			 __func__, vdev_id);
		return NULL;
	}

	beacon = wma->interfaces[vdev_id].beacon;

	if (!beacon) {
		WMA_LOGE("%s: beacon invalid", __func__);
		return NULL;
	}

	adf_os_spin_lock_bh(&beacon->lock);

	buf_size = adf_nbuf_len(beacon->buf);
	buf = adf_os_mem_alloc(NULL, buf_size);

	if (!buf) {
		adf_os_spin_unlock_bh(&beacon->lock);
		WMA_LOGE("%s: alloc failed for beacon buf", __func__);
		return NULL;
	}

	adf_os_mem_copy(buf, adf_nbuf_data(beacon->buf), buf_size);

	adf_os_spin_unlock_bh(&beacon->lock);

	if (buffer_size)
		*buffer_size = buf_size;

	return buf;
}

#if defined(QCA_WIFI_FTM)
int wma_utf_rsp(tp_wma_handle wma_handle, u_int8_t **payload, u_int32_t *len)
{
	int ret = -1;
	u_int32_t payload_len;

	payload_len = wma_handle->utf_event_info.length;
	if (payload_len) {
		ret = 0;

		/*
		 * The first 4 bytes holds the payload size
		 * and the actual payload sits next to it
		 */
		*payload = (u_int8_t *)vos_mem_malloc((v_SIZE_t)payload_len
						      + sizeof(A_UINT32));
		*(A_UINT32*)&(*payload[0]) = wma_handle->utf_event_info.length;
		memcpy(*payload + sizeof(A_UINT32),
		       wma_handle->utf_event_info.data,
		       payload_len);
		wma_handle->utf_event_info.length = 0;
		*len = payload_len;
	}

	return ret;
}

static void wma_post_ftm_response(tp_wma_handle wma_handle)
{
	int ret;
	u_int8_t *payload;
	u_int32_t data_len;
	vos_msg_t msg = {0};
	VOS_STATUS status;

	ret = wma_utf_rsp(wma_handle, &payload, &data_len);

	if (ret) {
		return;
	}

	msg.type = SYS_MSG_ID_FTM_RSP;
	msg.bodyptr = payload;
	msg.bodyval = 0;
	msg.reserved = SYS_MSG_COOKIE;

	status = vos_mq_post_message(VOS_MQ_ID_SYS, &msg);

	if (status != VOS_STATUS_SUCCESS) {
		WMA_LOGE("failed to post ftm response to SYS");
		vos_mem_free(payload);
	}
}

static int
wma_process_utf_event(WMA_HANDLE handle,
		      u_int8_t *datap, u_int32_t dataplen)
{
	tp_wma_handle wma_handle = (tp_wma_handle)handle;
	SEG_HDR_INFO_STRUCT segHdrInfo;
	u_int8_t totalNumOfSegments,currentSeq;
	WMI_PDEV_UTF_EVENTID_param_tlvs *param_buf;
	u_int8_t *data;
	u_int32_t datalen;

	param_buf = (WMI_PDEV_UTF_EVENTID_param_tlvs *) datap;
	if (!param_buf) {
		WMA_LOGE("Get NULL point message from FW");
		return -EINVAL;
	}
	data = param_buf->data;
	datalen = param_buf->num_data;


	segHdrInfo = *(SEG_HDR_INFO_STRUCT *)&(data[0]);

	wma_handle->utf_event_info.currentSeq = (segHdrInfo.segmentInfo & 0xF);

	currentSeq = (segHdrInfo.segmentInfo & 0xF);
	totalNumOfSegments = (segHdrInfo.segmentInfo >> 4) & 0xF;

	datalen = datalen - sizeof(segHdrInfo);

	if (currentSeq == 0) {
		wma_handle->utf_event_info.expectedSeq = 0;
		wma_handle->utf_event_info.offset = 0;
	} else {
		if (wma_handle->utf_event_info.expectedSeq != currentSeq)
			WMA_LOGE("Mismatch in expecting seq expected"
				 " Seq %d got seq %d",
				 wma_handle->utf_event_info.expectedSeq,
				 currentSeq);
	}

	memcpy(&wma_handle->utf_event_info.data[wma_handle->utf_event_info.offset],
	       &data[sizeof(segHdrInfo)],
               datalen);
	wma_handle->utf_event_info.offset = wma_handle->utf_event_info.offset + datalen;
	wma_handle->utf_event_info.expectedSeq++;

	if (wma_handle->utf_event_info.expectedSeq == totalNumOfSegments) {
		if (wma_handle->utf_event_info.offset != segHdrInfo.len)
			WMA_LOGE("All segs received total len mismatch.."
				 " len %zu total len %d",
				 wma_handle->utf_event_info.offset,
				 segHdrInfo.len);

		wma_handle->utf_event_info.length = wma_handle->utf_event_info.offset;
	}

	wma_post_ftm_response(wma_handle);

	return 0;
}

void wma_utf_detach(tp_wma_handle wma_handle)
{
    if (wma_handle->utf_event_info.data) {
        vos_mem_free(wma_handle->utf_event_info.data);
        wma_handle->utf_event_info.data = NULL;
        wma_handle->utf_event_info.length = 0;
        wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
					     WMI_PDEV_UTF_EVENTID);
    }
}

void wma_utf_attach(tp_wma_handle wma_handle)
{
	int ret;

	wma_handle->utf_event_info.data = (unsigned char *)
					  vos_mem_malloc(MAX_UTF_EVENT_LENGTH);
	wma_handle->utf_event_info.length = 0;

	ret = wmi_unified_register_event_handler(wma_handle->wmi_handle,
						 WMI_PDEV_UTF_EVENTID,
						 wma_process_utf_event);

	if (ret)
		WMA_LOGP("%s: Failed to register UTF event callback", __func__);
}

static int
wmi_unified_pdev_utf_cmd(wmi_unified_t wmi_handle, u_int8_t *utf_payload,
                         u_int16_t len)
{
	wmi_buf_t buf;
	u_int8_t *cmd;
	int ret = 0;
	static u_int8_t msgref = 1;
	u_int8_t segNumber = 0, segInfo, numSegments;
	u_int16_t chunk_len, total_bytes;
	u_int8_t *bufpos;
	SEG_HDR_INFO_STRUCT segHdrInfo;

	bufpos = utf_payload;
	total_bytes = len;
	ASSERT(total_bytes / MAX_WMI_UTF_LEN ==
	       (u_int8_t)(total_bytes / MAX_WMI_UTF_LEN));
	numSegments = (u_int8_t)(total_bytes / MAX_WMI_UTF_LEN);

	if (len - (numSegments * MAX_WMI_UTF_LEN))
		numSegments++;

	while (len) {
		if (len > MAX_WMI_UTF_LEN)
			chunk_len = MAX_WMI_UTF_LEN; /* MAX messsage */
		else
			chunk_len = len;

		buf = wmi_buf_alloc(wmi_handle,
				 (chunk_len + sizeof(segHdrInfo) +
				  WMI_TLV_HDR_SIZE));
		if (!buf) {
			WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
			return -1;
		}

		cmd = (u_int8_t *)wmi_buf_data(buf);

		segHdrInfo.len = total_bytes;
		segHdrInfo.msgref =  msgref;
		segInfo = ((numSegments << 4 ) & 0xF0) | (segNumber & 0xF);
		segHdrInfo.segmentInfo = segInfo;
		segHdrInfo.pad = 0;

		WMA_LOGD("%s:segHdrInfo.len = %d, segHdrInfo.msgref = %d,"
			 " segHdrInfo.segmentInfo = %d",
			 __func__, segHdrInfo.len, segHdrInfo.msgref,
			 segHdrInfo.segmentInfo);

		WMA_LOGD("%s:total_bytes %d segNumber %d totalSegments %d"
			 "chunk len %d", __func__, total_bytes, segNumber,
			 numSegments, chunk_len);

		segNumber++;

		WMITLV_SET_HDR(cmd, WMITLV_TAG_ARRAY_BYTE,
			       (chunk_len + sizeof(segHdrInfo)));
		cmd += WMI_TLV_HDR_SIZE;
		memcpy(cmd, &segHdrInfo, sizeof(segHdrInfo)); /* 4 bytes */
		memcpy(&cmd[sizeof(segHdrInfo)], bufpos, chunk_len);

		ret = wmi_unified_cmd_send(wmi_handle, buf,
				(chunk_len + sizeof(segHdrInfo) +
				 WMI_TLV_HDR_SIZE),
				WMI_PDEV_UTF_CMDID);

		if (ret != EOK) {
			WMA_LOGE("Failed to send WMI_PDEV_UTF_CMDID command");
			wmi_buf_free(buf);
			break;
		}

		len -= chunk_len;
		bufpos += chunk_len;
	}

	msgref++;

	return ret;
}

int wma_utf_cmd(tp_wma_handle wma_handle, u_int8_t *data, u_int16_t len)
{
	wma_handle->utf_event_info.length = 0;
	return wmi_unified_pdev_utf_cmd(wma_handle->wmi_handle, data, len);
}

static VOS_STATUS
wma_process_ftm_command(tp_wma_handle wma_handle,
			struct ar6k_testmode_cmd_data *msg_buffer)
{
	u_int8_t *data = NULL;
	u_int16_t len = 0;
	int ret;

	if (!msg_buffer)
		return VOS_STATUS_E_INVAL;

	if (vos_get_conparam() != VOS_FTM_MODE) {
		WMA_LOGE("FTM command issued in non-FTM mode");
		vos_mem_free(msg_buffer->data);
		vos_mem_free(msg_buffer);
		return VOS_STATUS_E_NOSUPPORT;
	}

	data = msg_buffer->data;
	len = msg_buffer->len;

	ret = wma_utf_cmd(wma_handle, data, len);

	vos_mem_free(msg_buffer->data);
	vos_mem_free(msg_buffer);

	if (ret)
		return VOS_STATUS_E_FAILURE;

	return VOS_STATUS_SUCCESS;
}
#endif

/* Function to enable/disble Low Power Support(Pdev Specific) */
VOS_STATUS WDA_SetIdlePsConfig(void *wda_handle, tANI_U32 idle_ps)
{
	int32_t ret;
	tp_wma_handle wma = (tp_wma_handle)wda_handle;

	WMA_LOGD("WMA Set Idle Ps Config [1:set 0:clear] val %d", idle_ps);

	/* Set Idle Mode Power Save Config */
	ret = wmi_unified_pdev_set_param(wma->wmi_handle,
			WMI_PDEV_PARAM_IDLE_PS_CONFIG, idle_ps);
	if(ret) {
		WMA_LOGE("Fail to Set Idle Ps Config %d", idle_ps);
		return VOS_STATUS_E_FAILURE;
	}

	WMA_LOGD("Successfully Set Idle Ps Config %d", idle_ps);
	return VOS_STATUS_SUCCESS;
}

eHalStatus wma_set_htconfig(tANI_U8 vdev_id, tANI_U16 ht_capab, int value)
{
	void *vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
	tp_wma_handle wma = vos_get_context(VOS_MODULE_ID_WDA, vos_context);
	int ret = -EIO;

	if (NULL == wma) {
		WMA_LOGE("%s: Failed to get wma", __func__);
		return eHAL_STATUS_INVALID_PARAMETER;
	}

	switch (ht_capab) {
	case WNI_CFG_HT_CAP_INFO_ADVANCE_CODING:
	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
						WMI_VDEV_PARAM_LDPC, value);
	break;
	case WNI_CFG_HT_CAP_INFO_TX_STBC:
	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
						WMI_VDEV_PARAM_TX_STBC, value);
	break;
	case WNI_CFG_HT_CAP_INFO_RX_STBC:
	ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
						WMI_VDEV_PARAM_RX_STBC, value);
	break;
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_20MHZ:
	case WNI_CFG_HT_CAP_INFO_SHORT_GI_40MHZ:
		WMA_LOGE("%s: ht_capab = %d, value = %d", __func__, ht_capab, value);
		ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
				WMI_VDEV_PARAM_SGI, value);
		if (ret == 0)
			wma->interfaces[vdev_id].config.shortgi = value;
		break;
	default:
	WMA_LOGE("%s:INVALID HT CONFIG", __func__);
	}

	return (ret)? eHAL_STATUS_FAILURE : eHAL_STATUS_SUCCESS;
}

eHalStatus WMA_SetRegDomain(void * clientCtxt, v_REGDOMAIN_t regId,
		tAniBool sendRegHint)
{
	if(VOS_STATUS_SUCCESS != vos_nv_setRegDomain(clientCtxt, regId, sendRegHint))
		return eHAL_STATUS_INVALID_PARAMETER;

	return eHAL_STATUS_SUCCESS;
}

tANI_U8 wma_getFwWlanFeatCaps(tANI_U8 featEnumValue)
{
	return ((gFwWlanFeatCaps & (1 << featEnumValue)) ? TRUE : FALSE);
}

void wma_send_regdomain_info(u_int32_t reg_dmn, u_int16_t regdmn2G,
			     u_int16_t regdmn5G, int8_t ctl2G, int8_t ctl5G)
{
	wmi_buf_t buf;
	wmi_pdev_set_regdomain_cmd_fixed_param *cmd;
	int32_t len = sizeof(*cmd);
	void *vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
	tp_wma_handle wma = vos_get_context(VOS_MODULE_ID_WDA, vos_context);
	int32_t cck_mask_val = 0;
	int ret = 0;

	if (NULL == wma) {
		WMA_LOGE("%s: wma context is NULL", __func__);
		return;
	}

	buf = wmi_buf_alloc(wma->wmi_handle, len);
	if (!buf) {
		WMA_LOGP("%s: wmi_buf_alloc failed", __func__);
		return;
	}

	cmd = (wmi_pdev_set_regdomain_cmd_fixed_param *) wmi_buf_data(buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_pdev_set_regdomain_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(
			wmi_pdev_set_regdomain_cmd_fixed_param));
	cmd->reg_domain = reg_dmn;
	cmd->reg_domain_2G = regdmn2G;
	cmd->reg_domain_5G = regdmn5G;
	cmd->conformance_test_limit_2G = ctl2G;
	cmd->conformance_test_limit_5G = ctl5G;

	if (wmi_unified_cmd_send(wma->wmi_handle, buf, len,
				WMI_PDEV_SET_REGDOMAIN_CMDID)) {
		WMA_LOGP("%s: Failed to send pdev set regdomain command",
				__func__);
		adf_nbuf_free(buf);
	}

	if ((((reg_dmn & ~COUNTRY_ERD_FLAG) == CTRY_JAPAN) ||
	     ((reg_dmn & ~COUNTRY_ERD_FLAG) == CTRY_KOREA_ROC)) &&
	    (true == wma->tx_chain_mask_cck))
		cck_mask_val = 1;

	cck_mask_val |= (wma->self_gen_frm_pwr << 16);
	ret = wmi_unified_pdev_set_param(wma->wmi_handle,
					 WMI_PDEV_PARAM_TX_CHAIN_MASK_CCK,
					 cck_mask_val);
	if (ret) {
		WMA_LOGE("failed to set PDEV tx_chain_mask_cck %d",
			 ret);
	}

	return;
}

void wma_get_modeselect(tp_wma_handle wma, u_int32_t *modeSelect)
{

	switch (wma->phy_capability) {
	case WMI_11G_CAPABILITY:
	case WMI_11NG_CAPABILITY:
		*modeSelect &= ~(REGDMN_MODE_11A | REGDMN_MODE_TURBO |
			REGDMN_MODE_108A | REGDMN_MODE_11A_HALF_RATE |
			REGDMN_MODE_11A_QUARTER_RATE | REGDMN_MODE_11NA_HT20 |
			REGDMN_MODE_11NA_HT40PLUS | REGDMN_MODE_11NA_HT40MINUS |
			REGDMN_MODE_11AC_VHT20 | REGDMN_MODE_11AC_VHT40PLUS |
			REGDMN_MODE_11AC_VHT40MINUS | REGDMN_MODE_11AC_VHT80);
		break;
	case WMI_11A_CAPABILITY:
	case WMI_11NA_CAPABILITY:
	case WMI_11AC_CAPABILITY:
		*modeSelect &= ~(REGDMN_MODE_11B | REGDMN_MODE_11G |
			REGDMN_MODE_108G | REGDMN_MODE_11NG_HT20 |
			REGDMN_MODE_11NG_HT40PLUS | REGDMN_MODE_11NG_HT40MINUS |
			REGDMN_MODE_11AC_VHT20_2G | REGDMN_MODE_11AC_VHT40_2G |
			REGDMN_MODE_11AC_VHT80_2G);
		break;
	}
}

tANI_U8 wma_map_channel(tANI_U8 mapChannel)
{
	return mapChannel;
}

static eHalStatus wma_set_mimops(tp_wma_handle wma, tANI_U8 vdev_id, int value)
{
        int ret = eHAL_STATUS_SUCCESS;
        wmi_sta_smps_force_mode_cmd_fixed_param *cmd;
        wmi_buf_t buf;
        u_int16_t len = sizeof(*cmd);

        buf = wmi_buf_alloc(wma->wmi_handle, len);
        if (!buf) {
                WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
                return -ENOMEM;
        }
        cmd = (wmi_sta_smps_force_mode_cmd_fixed_param *) wmi_buf_data(buf);
        WMITLV_SET_HDR(&cmd->tlv_header,
                WMITLV_TAG_STRUC_wmi_sta_smps_force_mode_cmd_fixed_param,
                WMITLV_GET_STRUCT_TLVLEN(
                wmi_sta_smps_force_mode_cmd_fixed_param));

        cmd->vdev_id = vdev_id;

        switch(value)
        {
            case 0:
                   cmd->forced_mode = WMI_SMPS_FORCED_MODE_NONE;
                   break;
            case 1:
                   cmd->forced_mode = WMI_SMPS_FORCED_MODE_DISABLED;
                   break;
            case 2:
                   cmd->forced_mode = WMI_SMPS_FORCED_MODE_STATIC;
                   break;
            case 3:
                   cmd->forced_mode = WMI_SMPS_FORCED_MODE_DYNAMIC;
                   break;
            default:
                   WMA_LOGE("%s:INVALID Mimo PS CONFIG", __func__);
                   return eHAL_STATUS_FAILURE;
        }

        WMA_LOGD("Setting vdev %d value = %u", vdev_id, value);

        ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
                                WMI_STA_SMPS_FORCE_MODE_CMDID);
        if (ret < 0) {
                WMA_LOGE("Failed to send set Mimo PS ret = %d", ret);
                wmi_buf_free(buf);
        }

        return ret;
}

static eHalStatus wma_set_ppsconfig(tANI_U8 vdev_id, tANI_U16 pps_param, int val)
{
        void *vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
        tp_wma_handle wma = vos_get_context(VOS_MODULE_ID_WDA, vos_context);
        int ret = -EIO;
        u_int32_t pps_val;

	if (NULL == wma) {
		WMA_LOGE("%s: Failed to get wma", __func__);
		return eHAL_STATUS_INVALID_PARAMETER;
	}

        switch (pps_param) {
        case WMA_VHT_PPS_PAID_MATCH:
            pps_val = ((val << 31) & 0xffff0000) |
                (PKT_PWR_SAVE_PAID_MATCH & 0xffff);
            goto pkt_pwr_save_config;
        case WMA_VHT_PPS_GID_MATCH:
            pps_val = ((val << 31) & 0xffff0000) |
                (PKT_PWR_SAVE_GID_MATCH & 0xffff);
            goto pkt_pwr_save_config;
        case WMA_VHT_PPS_DELIM_CRC_FAIL:
            pps_val = ((val << 31) & 0xffff0000) |
                (PKT_PWR_SAVE_DELIM_CRC_FAIL & 0xffff);
            goto pkt_pwr_save_config;

        /* Enable the code below as and when the functionality
         * is supported/added in host.
         */
#ifdef NOT_YET
        case WMA_VHT_PPS_EARLY_TIM_CLEAR:
            pps_val = ((val << 31) & 0xffff0000) |
                (PKT_PWR_SAVE_EARLY_TIM_CLEAR & 0xffff);
            goto pkt_pwr_save_config;
        case WMA_VHT_PPS_EARLY_DTIM_CLEAR:
            pps_val = ((val << 31) & 0xffff0000) |
                (PKT_PWR_SAVE_EARLY_DTIM_CLEAR & 0xffff);
            goto pkt_pwr_save_config;
        case WMA_VHT_PPS_EOF_PAD_DELIM:
            pps_val = ((val << 31) & 0xffff0000) |
                (PKT_PWR_SAVE_EOF_PAD_DELIM & 0xffff);
            goto pkt_pwr_save_config;
        case WMA_VHT_PPS_MACADDR_MISMATCH:
            pps_val = ((val << 31) & 0xffff0000) |
                (PKT_PWR_SAVE_MACADDR_MISMATCH & 0xffff);
            goto pkt_pwr_save_config;
        case WMA_VHT_PPS_GID_NSTS_ZERO:
            pps_val = ((val << 31) & 0xffff0000) |
                (PKT_PWR_SAVE_GID_NSTS_ZERO & 0xffff);
            goto pkt_pwr_save_config;
        case WMA_VHT_PPS_RSSI_CHECK:
            pps_val = ((val << 31) & 0xffff0000) |
                (PKT_PWR_SAVE_RSSI_CHECK & 0xffff);
            goto pkt_pwr_save_config;
#endif
pkt_pwr_save_config:
            WMA_LOGD("vdev_id:%d val:0x%x pps_val:0x%x", vdev_id,
                     val, pps_val);
            ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
                    WMI_VDEV_PARAM_PACKET_POWERSAVE, pps_val);
            break;
        default:
            WMA_LOGE("%s:INVALID PPS CONFIG", __func__);
        }

        return (ret)? eHAL_STATUS_FAILURE : eHAL_STATUS_SUCCESS;
}

#ifdef FEATURE_WLAN_TDLS
static int wma_set_tdls_offchan_mode(WMA_HANDLE handle,
		                     tTdlsChanSwitchParams* pChanSwitchParams)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	wmi_tdls_set_offchan_mode_cmd_fixed_param* cmd;
	wmi_buf_t wmi_buf;
	u_int16_t len = sizeof(wmi_tdls_set_offchan_mode_cmd_fixed_param);
	int ret = 0;

	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue tdls off channel cmd",
		         __func__);
		ret = -EINVAL;
		goto end;
	}
	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmai_buf_alloc failed", __func__);
		ret = -ENOMEM;
		goto end;
	}
	cmd = (wmi_tdls_set_offchan_mode_cmd_fixed_param*)wmi_buf_data(wmi_buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		WMITLV_TAG_STRUC_wmi_tdls_set_offchan_mode_cmd_fixed_param,
		WMITLV_GET_STRUCT_TLVLEN(wmi_tdls_set_offchan_mode_cmd_fixed_param));

	WMI_CHAR_ARRAY_TO_MAC_ADDR(pChanSwitchParams->peerMacAddr,
		                   &cmd->peer_macaddr);
	cmd->vdev_id = pChanSwitchParams->vdevId;
	cmd->offchan_mode = pChanSwitchParams->tdlsSwMode;
	cmd->is_peer_responder = pChanSwitchParams->is_responder;
	cmd->offchan_num = pChanSwitchParams->tdlsOffCh;
	cmd->offchan_bw_bitmap = pChanSwitchParams->tdlsOffChBwOffset;
	cmd->offchan_oper_class = pChanSwitchParams->operClass;

	WMA_LOGD("%s: Peer MAC Addr mac_addr31to0: 0x%x, "
		 "mac_addr47to32: 0x%x",
		__func__, cmd->peer_macaddr.mac_addr31to0,
		cmd->peer_macaddr.mac_addr47to32);

	WMA_LOGD("%s: vdev_id: %d, "
		 "off channel mode: %d, "
		 "off channel Num: %d, "
		 "off channel offset: 0x%x, "
		 "is_peer_responder: %d, "
		 "operating class: %d, ",
		 __func__, cmd->vdev_id,
		 cmd->offchan_mode,
		 cmd->offchan_num,
		 cmd->offchan_bw_bitmap,
		 cmd->is_peer_responder,
		 cmd->offchan_oper_class);

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, len,
		WMI_TDLS_SET_OFFCHAN_MODE_CMDID)) {
		WMA_LOGP("%s: failed to send tdls off chan command", __func__);
		adf_nbuf_free(wmi_buf);
		ret = -EIO;
	}

end:
	if (pChanSwitchParams)
		vos_mem_free(pChanSwitchParams);
	return ret;
}

/* wmi tdls command sent to firmware to enable/disable tdls for a vdev */
static int wma_update_fw_tdls_state(WMA_HANDLE handle, void *pwmaTdlsparams)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	wmi_tdls_set_state_cmd_fixed_param* cmd;
	wmi_buf_t wmi_buf;
	t_wma_tdls_mode  tdls_mode;
	t_wma_tdls_params  *wma_tdls = (t_wma_tdls_params *)pwmaTdlsparams;
	u_int16_t len = sizeof(wmi_tdls_set_state_cmd_fixed_param);
	int ret = 0;

	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue fw tdls state cmd",
		         __func__);
		ret = -EINVAL;
		goto end_fw_tdls_state;
	}

	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmai_buf_alloc failed", __func__);
		ret = ENOMEM;
		goto end_fw_tdls_state;
	}
	tdls_mode = wma_tdls->tdls_state;
	cmd = (wmi_tdls_set_state_cmd_fixed_param *)wmi_buf_data(wmi_buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
	      WMITLV_TAG_STRUC_wmi_tdls_set_state_cmd_fixed_param,
	      WMITLV_GET_STRUCT_TLVLEN(wmi_tdls_set_state_cmd_fixed_param));
	cmd->vdev_id = wma_tdls->vdev_id;

	if (WMA_TDLS_SUPPORT_EXPLICIT_TRIGGER_ONLY == tdls_mode) {
		cmd->state = WMI_TDLS_ENABLE_PASSIVE;
	} else if (WMA_TDLS_SUPPORT_ENABLED == tdls_mode) {
		cmd->state = WMI_TDLS_ENABLE_ACTIVE;
	} else {
		cmd->state = WMI_TDLS_DISABLE;
	}

	cmd->notification_interval_ms = wma_tdls->notification_interval_ms;
	cmd->tx_discovery_threshold = wma_tdls->tx_discovery_threshold;
	cmd->tx_teardown_threshold = wma_tdls->tx_teardown_threshold;
	cmd->rssi_teardown_threshold = wma_tdls->rssi_teardown_threshold;
	cmd->rssi_delta = wma_tdls->rssi_delta;
	cmd->tdls_options = wma_tdls->tdls_options;
	cmd->tdls_peer_traffic_ind_window =
	    wma_tdls->peer_traffic_ind_window;
	cmd->tdls_peer_traffic_response_timeout_ms =
	    wma_tdls->peer_traffic_response_timeout;
	cmd->tdls_puapsd_mask =
	    wma_tdls->puapsd_mask;
	cmd->tdls_puapsd_inactivity_time_ms =
	    wma_tdls->puapsd_inactivity_time;
	cmd->tdls_puapsd_rx_frame_threshold =
	    wma_tdls->puapsd_rx_frame_threshold;

	WMA_LOGD("%s: tdls_mode: %d, state: %d, "
	         "notification_interval_ms: %d, "
	         "tx_discovery_threshold: %d, "
	         "tx_teardown_threshold: %d, "
	         "rssi_teardown_threshold: %d, "
	         "rssi_delta: %d, "
	         "tdls_options: 0x%x, "
	         "tdls_peer_traffic_ind_window: %d, "
	         "tdls_peer_traffic_response_timeout: %d, "
	         "tdls_puapsd_mask: 0x%x, "
	         "tdls_puapsd_inactivity_time: %d, "
	         "tdls_puapsd_rx_frame_threshold: %d ",
	         __func__, tdls_mode, cmd->state,
	         cmd->notification_interval_ms,
	         cmd->tx_discovery_threshold,
	         cmd->tx_teardown_threshold,
	         cmd->rssi_teardown_threshold,
	         cmd->rssi_delta,
	         cmd->tdls_options,
	         cmd->tdls_peer_traffic_ind_window,
	         cmd->tdls_peer_traffic_response_timeout_ms,
	         cmd->tdls_puapsd_mask,
	         cmd->tdls_puapsd_inactivity_time_ms,
	         cmd->tdls_puapsd_rx_frame_threshold);

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, len,
		   WMI_TDLS_SET_STATE_CMDID)) {
		WMA_LOGP("%s: failed to send tdls set state command", __func__);
		adf_nbuf_free(wmi_buf);
		ret = -EIO;
		goto end_fw_tdls_state;
	}
	WMA_LOGD("%s: vdev_id %d", __func__, wma_tdls->vdev_id);

end_fw_tdls_state:
	if (pwmaTdlsparams)
		vos_mem_free(pwmaTdlsparams);
	return ret;
}

static int wma_update_tdls_peer_state(WMA_HANDLE handle,
	                 tTdlsPeerStateParams *peerStateParams)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	wmi_tdls_peer_update_cmd_fixed_param* cmd;
	wmi_tdls_peer_capabilities *peer_cap;
	wmi_channel *chan_info;
	wmi_buf_t wmi_buf;
	u_int8_t *buf_ptr;
	u_int32_t i;
	ol_txrx_pdev_handle pdev;
	u_int8_t peer_id;
	struct ol_txrx_peer_t *peer;
	int32_t len = sizeof(wmi_tdls_peer_update_cmd_fixed_param) +
	              sizeof(wmi_tdls_peer_capabilities);
	int ret = 0;
#if defined(CONFIG_HL_SUPPORT)
        struct ol_txrx_vdev_t *vdev;
        bool restore_last_peer = false;
#endif

	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue cmd", __func__);
		ret = -EINVAL;
		goto end_tdls_peer_state;
	}

	/* peer capability info is valid only when peer state is connected */
	if (WDA_TDLS_PEER_STATE_CONNECTED != peerStateParams->peerState) {
		vos_mem_zero(&peerStateParams->peerCap, sizeof(tTdlsPeerCapParams));
	}

	len += WMI_TLV_HDR_SIZE +
	       sizeof(wmi_channel) * peerStateParams->peerCap.peerChanLen;

	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		ret = ENOMEM;
		goto end_tdls_peer_state;
	}

	buf_ptr = (u_int8_t *) wmi_buf_data(wmi_buf);
	cmd = (wmi_tdls_peer_update_cmd_fixed_param *) buf_ptr;
	WMITLV_SET_HDR(&cmd->tlv_header,
	       WMITLV_TAG_STRUC_wmi_tdls_peer_update_cmd_fixed_param,
	       WMITLV_GET_STRUCT_TLVLEN(wmi_tdls_peer_update_cmd_fixed_param));

	cmd->vdev_id = peerStateParams->vdevId;
	WMI_CHAR_ARRAY_TO_MAC_ADDR(peerStateParams->peerMacAddr, &cmd->peer_macaddr);

	switch (peerStateParams->peerState) {
		case  WDA_TDLS_PEER_STATE_PEERING:
			cmd->peer_state = WMI_TDLS_PEER_STATE_PEERING;
			break;
		case  WDA_TDLS_PEER_STATE_CONNECTED:
			cmd->peer_state = WMI_TDLS_PEER_STATE_CONNECTED;
			break;
		case  WDA_TDLS_PEER_STATE_TEARDOWN:
			cmd->peer_state = WMI_TDLS_PEER_STATE_TEARDOWN;
			break;
	}

	WMA_LOGD("%s: vdev_id: %d, peerStateParams->peerMacAddr: %pM, "
	         "peer_macaddr.mac_addr31to0: 0x%x, "
	         "peer_macaddr.mac_addr47to32: 0x%x, peer_state: %d",
	         __func__, cmd->vdev_id, peerStateParams->peerMacAddr,
	         cmd->peer_macaddr.mac_addr31to0,
	         cmd->peer_macaddr.mac_addr47to32, cmd->peer_state);

	buf_ptr += sizeof(wmi_tdls_peer_update_cmd_fixed_param);
	peer_cap = (wmi_tdls_peer_capabilities *) buf_ptr;
	WMITLV_SET_HDR(&peer_cap->tlv_header,
	       WMITLV_TAG_STRUC_wmi_tdls_peer_capabilities,
	       WMITLV_GET_STRUCT_TLVLEN(wmi_tdls_peer_capabilities));

	if ((peerStateParams->peerCap.peerUapsdQueue & 0x08) >> 3)
		WMI_SET_TDLS_PEER_VO_UAPSD(peer_cap);
	if ((peerStateParams->peerCap.peerUapsdQueue & 0x04) >> 2)
		WMI_SET_TDLS_PEER_VI_UAPSD(peer_cap);
	if ((peerStateParams->peerCap.peerUapsdQueue & 0x02) >> 1)
		WMI_SET_TDLS_PEER_BK_UAPSD(peer_cap);
	if (peerStateParams->peerCap.peerUapsdQueue & 0x01)
		WMI_SET_TDLS_PEER_BE_UAPSD(peer_cap);

	/* Ack and More Data Ack are sent as 0, so no need to set
	 * but fill SP
	 */
	WMI_SET_TDLS_PEER_SP_UAPSD(peer_cap, peerStateParams->peerCap.peerMaxSp);

	peer_cap->buff_sta_support =
		peerStateParams->peerCap.peerBuffStaSupport;
	peer_cap->off_chan_support =
		peerStateParams->peerCap.peerOffChanSupport;
	peer_cap->peer_curr_operclass =
		peerStateParams->peerCap.peerCurrOperClass;
	/* self curr operclass is not being used and so pass op class for
	 * preferred off chan in it.
	 */
	peer_cap->self_curr_operclass =
		peerStateParams->peerCap.opClassForPrefOffChan;
	peer_cap->peer_chan_len =
		peerStateParams->peerCap.peerChanLen;
	peer_cap->peer_operclass_len =
		peerStateParams->peerCap.peerOperClassLen;

	WMA_LOGD("%s: peer_operclass_len: %d",
	         __func__, peer_cap->peer_operclass_len);
	for (i = 0; i < WMI_TDLS_MAX_SUPP_OPER_CLASSES; i++) {
		peer_cap->peer_operclass[i] =
			peerStateParams->peerCap.peerOperClass[i];
		WMA_LOGD("%s: peer_operclass[%d]: %d",
	          __func__, i, peer_cap->peer_operclass[i]);
	}

	peer_cap->is_peer_responder =
		peerStateParams->peerCap.isPeerResponder;
	peer_cap->pref_offchan_num =
		peerStateParams->peerCap.prefOffChanNum;
	peer_cap->pref_offchan_bw =
		peerStateParams->peerCap.prefOffChanBandwidth;

	WMA_LOGD("%s: peer_qos: 0x%x, buff_sta_support: %d, off_chan_support: %d, peer_curr_operclass: %d, self_curr_operclass: %d, peer_chan_len: %d, peer_operclass_len: %d, is_peer_responder: %d, pref_offchan_num: %d, pref_offchan_bw: %d",
		        __func__, peer_cap->peer_qos, peer_cap->buff_sta_support,
		        peer_cap->off_chan_support, peer_cap->peer_curr_operclass,
		        peer_cap->self_curr_operclass, peer_cap->peer_chan_len,
		        peer_cap->peer_operclass_len, peer_cap->is_peer_responder,
		        peer_cap->pref_offchan_num, peer_cap->pref_offchan_bw);

	/* next fill variable size array of peer chan info */
	buf_ptr += sizeof(wmi_tdls_peer_capabilities);
	WMITLV_SET_HDR(buf_ptr,
	       WMITLV_TAG_ARRAY_STRUC,
	       sizeof(wmi_channel) * peerStateParams->peerCap.peerChanLen);
	chan_info = (wmi_channel *) (buf_ptr + WMI_TLV_HDR_SIZE);

	for (i = 0; i < peerStateParams->peerCap.peerChanLen; ++i) {
		WMITLV_SET_HDR(&chan_info->tlv_header,
			       WMITLV_TAG_STRUC_wmi_channel,
			       WMITLV_GET_STRUCT_TLVLEN(wmi_channel));
		chan_info->mhz =
			vos_chan_to_freq(peerStateParams->peerCap.peerChan[i].chanId);
		chan_info->band_center_freq1 = chan_info->mhz;
		chan_info->band_center_freq2 = 0;

		WMA_LOGD("%s: chan[%d] = %u", __func__, i, chan_info->mhz);

		if (peerStateParams->peerCap.peerChan[i].dfsSet) {
			WMI_SET_CHANNEL_FLAG(chan_info, WMI_CHAN_FLAG_PASSIVE);
			WMA_LOGI("chan[%d] DFS[%d]\n",
					peerStateParams->peerCap.peerChan[i].chanId,
					peerStateParams->peerCap.peerChan[i].dfsSet);
		}

		if (chan_info->mhz < WMA_2_4_GHZ_MAX_FREQ) {
			WMI_SET_CHANNEL_MODE(chan_info, MODE_11G);
		} else {
			WMI_SET_CHANNEL_MODE(chan_info, MODE_11A);
		}

		WMI_SET_CHANNEL_MAX_TX_POWER(chan_info,
			peerStateParams->peerCap.peerChan[i].pwr);

		WMI_SET_CHANNEL_REG_POWER(chan_info,
			peerStateParams->peerCap.peerChan[i].pwr);
		WMA_LOGD("Channel TX power[%d] = %u: %d", i, chan_info->mhz,
			peerStateParams->peerCap.peerChan[i].pwr);

		chan_info++;
	}

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, len,
	    WMI_TDLS_PEER_UPDATE_CMDID)) {
		WMA_LOGE("%s: failed to send tdls peer update state command",
		         __func__);
		adf_nbuf_free(wmi_buf);
		ret = -EIO;
		goto end_tdls_peer_state;
	}

	/* in case of teardown, remove peer from fw */
	if (WDA_TDLS_PEER_STATE_TEARDOWN == peerStateParams->peerState) {
		pdev = vos_get_context(VOS_MODULE_ID_TXRX, wma_handle->vos_context);
		if (!pdev) {
			WMA_LOGE("%s: Failed to find pdev", __func__);
			ret = -EIO;
			goto end_tdls_peer_state;
		}

		peer = ol_txrx_find_peer_by_addr(pdev, peerStateParams->peerMacAddr,
		                                 &peer_id);
		if (!peer) {
			WMA_LOGE("%s: Failed to get peer handle using peer mac %pM",
			         __func__, peerStateParams->peerMacAddr);
			ret = -EIO;
			goto end_tdls_peer_state;
		}

#if defined(CONFIG_HL_SUPPORT)
                vdev = peer->vdev;
                if (vdev->last_real_peer && (vdev->last_real_peer == peer))
                        restore_last_peer = true;
#endif

		WMA_LOGD("%s: calling wma_remove_peer for peer " MAC_ADDRESS_STR
		         " vdevId: %d", __func__,
		         MAC_ADDR_ARRAY(peer->mac_addr.raw), peerStateParams->vdevId);
		wma_remove_peer(wma_handle, peer->mac_addr.raw,
		                peerStateParams->vdevId, peer,
				VOS_FALSE);
#if defined(CONFIG_HL_SUPPORT)
                if (restore_last_peer && (vdev->last_real_peer == NULL)) {
                       peer = NULL;
                       peer = ol_txrx_find_peer_by_addr(pdev, vdev->hl_tdls_ap_mac_addr.raw, &peer_id);
                       if (peer && (peer->peer_ids[0] != HTT_INVALID_PEER_ID))
                            vdev->last_real_peer = peer;
                }
#endif
         }

end_tdls_peer_state:
	if (peerStateParams)
		vos_mem_free(peerStateParams);
	return ret;
}
#endif /* FEATURE_WLAN_TDLS */

/*
 * Attach DFS methods to the umac state.
 */
struct ieee80211com* wma_dfs_attach(struct ieee80211com *dfs_ic)
{
    /*Allocate memory for dfs_ic before passing it up to dfs_attach()*/
    dfs_ic = (struct ieee80211com *)
              OS_MALLOC(NULL, sizeof(struct ieee80211com), GFP_ATOMIC);
    if (dfs_ic == NULL)
    {
        WMA_LOGE("%s:Allocation of dfs_ic failed %zu",
                              __func__, sizeof(struct ieee80211com));
        return NULL;
    }
    OS_MEMZERO(dfs_ic, sizeof (struct ieee80211com));
    /* DFS pattern matching hooks */
    dfs_ic->ic_dfs_attach = ol_if_dfs_attach;
    dfs_ic->ic_dfs_disable = ol_if_dfs_disable;
    dfs_ic->ic_find_channel = ieee80211_find_channel;
    dfs_ic->ic_dfs_enable = ol_if_dfs_enable;
    dfs_ic->ic_ieee2mhz = ieee80211_ieee2mhz;

    /* Hardware facing hooks */
    dfs_ic->ic_get_ext_busy = ol_if_dfs_get_ext_busy;
    dfs_ic->ic_get_mib_cycle_counts_pct =
                             ol_if_dfs_get_mib_cycle_counts_pct;
    dfs_ic->ic_get_TSF64 = ol_if_get_tsf64;

    /* NOL related hooks */
    dfs_ic->ic_dfs_usenol = ol_if_dfs_usenol;
    /*
     * Hooks from wma/dfs/ back
     * into the PE/SME
     * and shared DFS code
     */
    dfs_ic->ic_dfs_notify_radar = ieee80211_mark_dfs;
    vos_lock_init(&dfs_ic->chan_lock);
    /* Initializes DFS Data Structures and queues*/
    dfs_attach(dfs_ic);

    return dfs_ic;
}

/*
 * Configures Radar Filters during
 * vdev start/channel change/regulatory domain
 * change.This Configuration enables to program
 * the DFS pattern matching module.
 */
void wma_dfs_configure(struct ieee80211com *ic)
{
	struct ath_dfs_radar_tab_info rinfo;
	int dfsdomain;
	int radar_enabled_status = 0;
	if(ic == NULL) {
		WMA_LOGE("%s: DFS ic is Invalid",__func__);
		return;
	}

	dfsdomain = ic->current_dfs_regdomain;

	/* Fetch current radar patterns from the lmac */
	OS_MEMZERO(&rinfo, sizeof(rinfo));

	/*
	 * Look up the current DFS
	 * regulatory domain and decide
	 * which radar pulses to use.
	 */
	switch (dfsdomain)
	{
	case DFS_FCC_DOMAIN:
		WMA_LOGI("%s: DFS-FCC domain",__func__);
		rinfo.dfsdomain = DFS_FCC_DOMAIN;
		rinfo.dfs_radars = dfs_fcc_radars;
		rinfo.numradars = ARRAY_LENGTH(dfs_fcc_radars);
		rinfo.b5pulses = dfs_fcc_bin5pulses;
		rinfo.numb5radars = ARRAY_LENGTH(dfs_fcc_bin5pulses);
		break;
	case DFS_ETSI_DOMAIN:
		WMA_LOGI("%s: DFS-ETSI domain",__func__);
		rinfo.dfsdomain = DFS_ETSI_DOMAIN;
		rinfo.dfs_radars = dfs_etsi_radars;
		rinfo.numradars = ARRAY_LENGTH(dfs_etsi_radars);
		rinfo.b5pulses = NULL;
		rinfo.numb5radars = 0;
		break;
	case DFS_MKK4_DOMAIN:
		WMA_LOGI("%s: DFS-MKK4 domain",__func__);
		rinfo.dfsdomain = DFS_MKK4_DOMAIN;
		rinfo.dfs_radars = dfs_mkk4_radars;
		rinfo.numradars = ARRAY_LENGTH(dfs_mkk4_radars);
		rinfo.b5pulses = dfs_jpn_bin5pulses;
		rinfo.numb5radars = ARRAY_LENGTH(dfs_jpn_bin5pulses);
		break;
	default:
		WMA_LOGI("%s: DFS-UNINT domain",__func__);
		rinfo.dfsdomain = DFS_UNINIT_DOMAIN;
		rinfo.dfs_radars = NULL;
		rinfo.numradars = 0;
		rinfo.b5pulses = NULL;
		rinfo.numb5radars = 0;
		break;
	}

	rinfo.dfs_pri_multiplier = ic->dfs_pri_multiplier;


	/*
	 * Set the regulatory domain,
	 * radar pulse table and enable
	 * radar events if required.
	 * dfs_radar_enable() returns
	 * 0 on success and non-zero
	 * failure.
	 */
	radar_enabled_status = dfs_radar_enable(ic, &rinfo);
	if (radar_enabled_status != DFS_STATUS_SUCCESS) {
		WMA_LOGE("%s[%d]: DFS- Radar Detection Enabling Failed",
			__func__, __LINE__);
	}
}

/*
 * Set the Channel parameters in to DFS module
 * Also,configure the DFS radar filters for
 * matching the DFS phyerrors.
 */
struct ieee80211_channel *
wma_dfs_configure_channel(struct ieee80211com *dfs_ic,
                          wmi_channel *chan,
                          WLAN_PHY_MODE chanmode,
                          struct wma_vdev_start_req *req)
{
    if(dfs_ic == NULL)
    {
        WMA_LOGE("%s: DFS ic is Invalid",__func__);
        return NULL;
    }
    dfs_ic->ic_curchan = (struct ieee80211_channel *) OS_MALLOC(NULL,
                                        sizeof(struct ieee80211_channel),
                                        GFP_ATOMIC);
    if (dfs_ic->ic_curchan == NULL)
    {
        WMA_LOGE("%s: allocation of dfs_ic->ic_curchan failed %zu",
                                     __func__,
                                     sizeof(struct ieee80211_channel));
        return NULL;
    }
    OS_MEMZERO(dfs_ic->ic_curchan, sizeof (struct ieee80211_channel));

    dfs_ic->ic_curchan->ic_ieee = req->chan;
    dfs_ic->ic_curchan->ic_freq = chan->mhz;
    dfs_ic->ic_curchan->ic_vhtop_ch_freq_seg1 = chan->band_center_freq1;
    dfs_ic->ic_curchan->ic_vhtop_ch_freq_seg2 = chan->band_center_freq2;

    if ( (dfs_ic->ic_curchan->ic_ieee >= WMA_11A_CHANNEL_BEGIN) &&
         (dfs_ic->ic_curchan->ic_ieee <= WMA_11A_CHANNEL_END) )
    {
        dfs_ic->ic_curchan->ic_flags |= IEEE80211_CHAN_5GHZ;
    }
    if(chanmode == MODE_11AC_VHT80)
    {
        dfs_ic->ic_curchan->ic_flags |= IEEE80211_CHAN_VHT80;
    }
    if (req->chan_offset == PHY_DOUBLE_CHANNEL_LOW_PRIMARY)
    {
        dfs_ic->ic_curchan->ic_flags |=
                (req->vht_capable ?
                 IEEE80211_CHAN_VHT40PLUS : IEEE80211_CHAN_HT40PLUS);
    }
    else if (req->chan_offset == PHY_DOUBLE_CHANNEL_HIGH_PRIMARY)
    {
        dfs_ic->ic_curchan->ic_flags |=
                (req->vht_capable ?
                 IEEE80211_CHAN_VHT40MINUS : IEEE80211_CHAN_HT40MINUS);
    }
    else if (req->chan_offset == PHY_SINGLE_CHANNEL_CENTERED)
    {
        dfs_ic->ic_curchan->ic_flags |=
           (req->vht_capable ? IEEE80211_CHAN_VHT20 : IEEE80211_CHAN_HT20);
    }
        dfs_ic->ic_curchan->ic_flagext |= IEEE80211_CHAN_DFS;

    if (req->oper_mode == BSS_OPERATIONAL_MODE_AP)
    {
        dfs_ic->ic_opmode = IEEE80211_M_HOSTAP;
        dfs_ic->vdev_id   = req->vdev_id;
    }

    dfs_ic->dfs_pri_multiplier = req->dfs_pri_multiplier;

    /*
     * Configuring the DFS with current channel and the radar filters
     */
    wma_dfs_configure(dfs_ic);
    WMA_LOGI("%s: DFS- CHANNEL CONFIGURED",__func__);
    return dfs_ic->ic_curchan;
}
/*
 * Configure the regulatory domain for DFS radar filter initialization
 */
void wma_set_dfs_regdomain(tp_wma_handle wma)
{
	u_int8_t ctl;
	u_int32_t regdmn = wma->reg_cap.eeprom_rd;
	u_int32_t regdmn5G;

	if (regdmn < 0)
	{
		WMA_LOGE("%s:DFS-Invalid regdomain",__func__);
		/*
		 * Set the DFS reg domain to unintlialized domain
		 * to indicate dfs regdomain configuration failure
		 */
		wma->dfs_ic->current_dfs_regdomain = DFS_UNINIT_DOMAIN;
		return;
	}

	regdmn5G = get_regdmn_5g(regdmn);
	ctl = regdmn_get_ctl_for_regdmn(regdmn5G);

	if (!ctl)
	{
		WMA_LOGI("%s:DFS-Invalid CTL",__func__);
		/*
		 * Set the DFS reg domain to unintlialized domain
		 * to indicate dfs regdomain configuration failure
		 */
		wma->dfs_ic->current_dfs_regdomain = DFS_UNINIT_DOMAIN;
		return;
	}
	if (ctl == FCC)
	{
		WMA_LOGI("%s:DFS- CTL = FCC",__func__);
		wma->dfs_ic->current_dfs_regdomain = DFS_FCC_DOMAIN;
	}
	else if (ctl == ETSI)
	{
		WMA_LOGI("%s:DFS- CTL = ETSI",__func__);
		wma->dfs_ic->current_dfs_regdomain = DFS_ETSI_DOMAIN;
	}
	else if (ctl == MKK)
	{
		WMA_LOGI("%s:DFS- CTL = MKK",__func__);
		wma->dfs_ic->current_dfs_regdomain = DFS_MKK4_DOMAIN;
	}
	WMA_LOGI("%s: ****** Current Reg Domain: %d *******", __func__,
			wma->dfs_ic->current_dfs_regdomain);
}

int wma_get_channels(struct ieee80211_channel *ichan,
		struct wma_dfs_radar_channel_list *chan_list)
{
	uint8_t center_chan = vos_freq_to_chan(ichan->ic_vhtop_ch_freq_seg1);

	chan_list->nchannels = 0;

	if (IEEE80211_IS_CHAN_11AC_VHT80(ichan))
	{
		chan_list->nchannels= 4;
		chan_list->channels[0] = center_chan - 6;
		chan_list->channels[1] = center_chan - 2;
		chan_list->channels[2] = center_chan + 2;
		chan_list->channels[3] = center_chan + 6;
	}
	else if(IEEE80211_IS_CHAN_11N_HT40(ichan) ||
			IEEE80211_IS_CHAN_11AC_VHT40(ichan))
	{
		chan_list->nchannels = 2;
		chan_list->channels[0] = center_chan - 2;
		chan_list->channels[1] = center_chan + 2;
	}
	else
	{
		chan_list->nchannels = 1;
		chan_list->channels[0] = center_chan;
	}

	return chan_list->nchannels;
}


/*
 * Indicate Radar to SAP/HDD
 */
int wma_dfs_indicate_radar(struct ieee80211com *ic,
		struct ieee80211_channel *ichan)
{
	tp_wma_handle wma;
	void *hdd_ctx;
	struct wma_dfs_radar_indication *radar_event;
	struct hdd_dfs_radar_ind hdd_radar_event;
	void *vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
	tpAniSirGlobal pmac = NULL;

	wma = (tp_wma_handle) vos_get_context(VOS_MODULE_ID_WDA, vos_context);

	if (wma == NULL)
	{
		WMA_LOGE("%s: DFS- Invalid wma", __func__);
		return -ENOENT;
	}

	hdd_ctx = vos_get_context(VOS_MODULE_ID_HDD,wma->vos_context);
	pmac = (tpAniSirGlobal)
		vos_get_context(VOS_MODULE_ID_PE, wma->vos_context);

	if (wma->dfs_ic != ic)
	{
		WMA_LOGE("%s:DFS- Invalid WMA handle",__func__);
		return -ENOENT;
	}
	radar_event = (struct wma_dfs_radar_indication *)
		vos_mem_malloc(sizeof(struct wma_dfs_radar_indication));
	if (radar_event == NULL)
	{
		WMA_LOGE("%s:DFS- Invalid radar_event",__func__);
		return -ENOENT;
	}

	/*
	 * Do not post multiple Radar events on the same channel.
	 * But, when DFS test mode is enabled, allow multiple dfs
	 * radar events to be posted on the same channel.
	 */
	if ((ichan->ic_ieee  != (wma->dfs_ic->last_radar_found_chan)) ||
	    ( pmac->sap.SapDfsInfo.disable_dfs_ch_switch == VOS_TRUE) )
	{
		wma->dfs_ic->last_radar_found_chan = ichan->ic_ieee;
		/* Indicate the radar event to HDD to stop the netif Tx queues*/
		hdd_radar_event.ieee_chan_number = ichan->ic_ieee;
		hdd_radar_event.chan_freq = ichan->ic_freq;
		hdd_radar_event.dfs_radar_status = WMA_DFS_RADAR_FOUND;
		wma->dfs_radar_indication_cb(hdd_ctx,&hdd_radar_event);
		WMA_LOGE("%s:DFS- RADAR INDICATED TO HDD",__func__);

		/*
		 * Indicate to the radar event to SAP to
		 * select a new channel and set CSA IE
		 */
		radar_event->vdev_id = ic->vdev_id;
		wma_get_channels(ichan, &radar_event->chan_list);
		radar_event->dfs_radar_status = WMA_DFS_RADAR_FOUND;
		radar_event->use_nol = ic->ic_dfs_usenol(ic);
		wma_send_msg(wma, WDA_DFS_RADAR_IND, (void *)radar_event, 0);
		WMA_LOGE("%s:DFS- WDA_DFS_RADAR_IND Message Posted",__func__);
	}

	return 0;
}

static eHalStatus wma_set_smps_params(tp_wma_handle wma, tANI_U8 vdev_id, int value)
{
        int ret = eHAL_STATUS_SUCCESS;
        wmi_sta_smps_param_cmd_fixed_param *cmd;
        wmi_buf_t buf;
        u_int16_t len = sizeof(*cmd);

        buf = wmi_buf_alloc(wma->wmi_handle, len);
        if (!buf) {
                WMA_LOGE("%s:wmi_buf_alloc failed", __func__);
                return -ENOMEM;
        }
        cmd = (wmi_sta_smps_param_cmd_fixed_param *) wmi_buf_data(buf);
        WMITLV_SET_HDR(&cmd->tlv_header,
                WMITLV_TAG_STRUC_wmi_sta_smps_param_cmd_fixed_param,
                WMITLV_GET_STRUCT_TLVLEN(
                wmi_sta_smps_param_cmd_fixed_param));

        cmd->vdev_id = vdev_id;
        cmd->value = value & WMA_SMPS_MASK_LOWER_16BITS;
        cmd->param = (value >> WMA_SMPS_PARAM_VALUE_S) & WMA_SMPS_MASK_UPPER_3BITS;

        WMA_LOGD("Setting vdev %d value = %x param %x", vdev_id, cmd->value, cmd->param);

        ret = wmi_unified_cmd_send(wma->wmi_handle, buf, len,
                                    WMI_STA_SMPS_PARAM_CMDID);
        if (ret < 0) {
                WMA_LOGE("Failed to send set Mimo PS ret = %d", ret);
                wmi_buf_free(buf);
        }

        return ret;
}

VOS_STATUS WMA_GetWcnssSoftwareVersion(v_PVOID_t pvosGCtx,
                tANI_U8 *pVersion,
                tANI_U32 versionBufferSize)
{

        tp_wma_handle wma_handle;
        wma_handle = vos_get_context(VOS_MODULE_ID_WDA, pvosGCtx);

	if (NULL == wma_handle) {
		WMA_LOGE("%s: Failed to get wma", __func__);
		return VOS_STATUS_E_FAULT;
	}

        snprintf(pVersion, versionBufferSize, "%x", (unsigned int)wma_handle->target_fw_version);
        return VOS_STATUS_SUCCESS;
}

void ol_rx_err(ol_pdev_handle pdev, u_int8_t vdev_id,
	       u_int8_t *peer_mac_addr, int tid, u_int32_t tsf32,
	       enum ol_rx_err_type err_type, adf_nbuf_t rx_frame,
	       u_int64_t *pn, u_int8_t key_id)
{
	void *g_vos_ctx = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
	tp_wma_handle wma = vos_get_context(VOS_MODULE_ID_WDA, g_vos_ctx);
	tpSirSmeMicFailureInd mic_err_ind;
	struct ether_header *eth_hdr;

	if (NULL == wma) {
		WMA_LOGE("%s: Failed to get wma", __func__);
		return;
	}

	if (err_type != OL_RX_ERR_TKIP_MIC)
		return;

	if (adf_nbuf_len(rx_frame) < sizeof(*eth_hdr))
		return;
	eth_hdr = (struct ether_header *) adf_nbuf_data(rx_frame);
	mic_err_ind = vos_mem_malloc(sizeof(*mic_err_ind));
	if (!mic_err_ind) {
		WMA_LOGE("%s: Failed to allocate memory for MIC indication message", __func__);
		return;
	}
	adf_os_mem_set((void *) mic_err_ind, 0, sizeof(*mic_err_ind));

	mic_err_ind->messageType = eWNI_SME_MIC_FAILURE_IND;
	mic_err_ind->length = sizeof(*mic_err_ind);
	adf_os_mem_copy(mic_err_ind->bssId,
		     (v_MACADDR_t *) wma->interfaces[vdev_id].bssid,
		     sizeof(tSirMacAddr));
	adf_os_mem_copy(mic_err_ind->info.taMacAddr,
		     (v_MACADDR_t *) peer_mac_addr, sizeof(tSirMacAddr));
	adf_os_mem_copy(mic_err_ind->info.srcMacAddr,
		     (v_MACADDR_t *) eth_hdr->ether_shost, sizeof(tSirMacAddr));
	adf_os_mem_copy(mic_err_ind->info.dstMacAddr,
		     (v_MACADDR_t *) eth_hdr->ether_dhost, sizeof(tSirMacAddr));
        mic_err_ind->info.keyId = key_id;
        mic_err_ind->info.multicast = IEEE80211_IS_MULTICAST(eth_hdr->ether_dhost);
	adf_os_mem_copy(mic_err_ind->info.TSC, pn, SIR_CIPHER_SEQ_CTR_SIZE);
	wma_send_msg(wma, SIR_HAL_MIC_FAILURE_IND, (void *) mic_err_ind, 0);
}

void WDA_TxAbort(v_U8_t vdev_id)
{
#define PEER_ALL_TID_BITMASK 0xffffffff
	tp_wma_handle wma;
	u_int32_t peer_tid_bitmap = PEER_ALL_TID_BITMASK;
	struct wma_txrx_node *iface;

	wma = vos_get_context(VOS_MODULE_ID_WDA,
			      vos_get_global_context(VOS_MODULE_ID_WDA, NULL));
	if (NULL == wma) {
		WMA_LOGE("%s: wma is NULL", __func__);
		return;
	}

	iface = &wma->interfaces[vdev_id];
	if (!iface->handle) {
		WMA_LOGE("%s: Failed to get iface handle: %p",
			 __func__, iface->handle);
		return;
	}
	WMA_LOGA("%s: vdevid %d bssid %pM", __func__, vdev_id, iface->bssid);
	iface->pause_bitmap |= (1 << PAUSE_TYPE_HOST);
	wdi_in_vdev_pause(iface->handle, OL_TXQ_PAUSE_REASON_TX_ABORT);

	/* Flush all TIDs except MGMT TID for this peer in Target */
	peer_tid_bitmap &= ~(0x1 << WMI_MGMT_TID);
	wmi_unified_peer_flush_tids_send(wma->wmi_handle, iface->bssid,
					 peer_tid_bitmap, vdev_id);
}

static void wma_set_vdev_suspend_dtim(tp_wma_handle wma, v_U8_t vdev_id)
{
	struct wma_txrx_node *iface = &wma->interfaces[vdev_id];
	u_int8_t is_qpower_enabled = wma_is_qpower_enabled(wma);

	if ((iface->type == WMI_VDEV_TYPE_STA) &&
		(iface->ps_enabled == TRUE) &&
		(iface->dtimPeriod != 0)) {
		int32_t ret;
		u_int32_t listen_interval;
		u_int32_t max_mod_dtim;

		if (wma->staDynamicDtim) {
			listen_interval = wma->staDynamicDtim;
		} else if ((wma->staModDtim) &&
			   (wma->staMaxLIModDtim)) {
			/*
			  * When the system is in suspend
			  * (maximum beacon will be at 1s == 10)
			  * If maxModulatedDTIM ((MAX_LI_VAL = 10) / AP_DTIM)
			  * equal or larger than MDTIM (configured in WCNSS_qcom_cfg.ini)
			  * Set LI to MDTIM * AP_DTIM
			  * If Dtim = 2 and Mdtim = 2 then LI is 4
			  * Else
			  * Set LI to maxModulatedDTIM * AP_DTIM
			  */
			max_mod_dtim = wma->staMaxLIModDtim/iface->dtimPeriod;
			if (max_mod_dtim >= wma->staModDtim) {
				listen_interval =
					(wma->staModDtim * iface->dtimPeriod);
			} else {
				listen_interval =
					(max_mod_dtim * iface->dtimPeriod);
			}
		} else {
			return;
		}

		WMA_LOGD("Set Listen Interval vdevId %d Listen Intv %d",
			vdev_id, listen_interval);

		ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
							WMI_VDEV_PARAM_LISTEN_INTERVAL,
							listen_interval);
		if (ret) {
			/* Even it fails continue Fw will take default LI */
			WMA_LOGE("Failed to Set Listen Interval vdevId %d",
				vdev_id);
		}

		if (is_qpower_enabled) {
			WMA_LOGD("disable Qpower in suspend mode!");
			ret = wmi_unified_set_sta_ps_param(wma->wmi_handle,
						vdev_id,
						WMI_STA_PS_ENABLE_QPOWER,
						0);
			if (ret)
				WMA_LOGE("Failed to disable Qpower in suspend mode!");

			iface->ps_enabled = TRUE;
		}

		ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
							WMI_VDEV_PARAM_DTIM_POLICY ,
							NORMAL_DTIM);
		if (ret) {
			/* Set it to Normal DTIM */
			WMA_LOGE("Failed to Set to Normal DTIM vdevId %d",
					vdev_id);
		}
		iface->dtim_policy = NORMAL_DTIM;
		WMA_LOGD("Set DTIM Policy to Normal Dtim vdevId %d", vdev_id);
	}
}

static void wma_set_suspend_dtim(tp_wma_handle wma)
{
	u_int8_t i;

	if (NULL == wma) {
		WMA_LOGE("%s: wma is NULL", __func__);
		return;
	}

	for (i = 0; i < wma->max_bssid; i++) {
		if ((wma->interfaces[i].handle) &&
		    (false == wma->interfaces[i].alt_modulated_dtim_enabled)) {
			wma_set_vdev_suspend_dtim(wma, i);
		}
	}
}

static void wma_set_vdev_resume_dtim(tp_wma_handle wma, v_U8_t vdev_id)
{
	struct wma_txrx_node *iface = &wma->interfaces[vdev_id];
	u_int8_t is_qpower_enabled = wma_is_qpower_enabled(wma);

	if ((iface->type == WMI_VDEV_TYPE_STA) &&
		(iface->ps_enabled == TRUE) &&
		(iface->dtim_policy == NORMAL_DTIM)) {
		int32_t ret;
		tANI_U32 cfg_data_val = 0;
		/* get mac to acess CFG data base */
		struct sAniSirGlobal *mac =
		(struct sAniSirGlobal*)vos_get_context(VOS_MODULE_ID_PE,
							wma->vos_context);
		/* Set Listen Interval */
		if ((NULL == mac) || (wlan_cfgGetInt(mac,
				WNI_CFG_LISTEN_INTERVAL,
				&cfg_data_val ) != eSIR_SUCCESS)) {
			VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_ERROR,
				"Failed to get value for listen interval");
			cfg_data_val = POWERSAVE_DEFAULT_LISTEN_INTERVAL;
		}

		ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
							WMI_VDEV_PARAM_LISTEN_INTERVAL,
							cfg_data_val);
		if (ret) {
			/* Even it fails continue Fw will take default LI */
			WMA_LOGE("Failed to Set Listen Interval vdevId %d",
				vdev_id);
		}

		WMA_LOGD("Set Listen Interval vdevId %d Listen Intv %d",
			vdev_id, cfg_data_val);

		ret = wmi_unified_vdev_set_param_send(wma->wmi_handle, vdev_id,
							WMI_VDEV_PARAM_DTIM_POLICY ,
							STICK_DTIM);
		if (ret) {
			/* Set it back to Stick DTIM */
			WMA_LOGE("Failed to Set to Stick DTIM vdevId %d",
				vdev_id);
		}
		iface->dtim_policy = STICK_DTIM;
		WMA_LOGD("Set DTIM Policy to Stick Dtim vdevId %d", vdev_id);

		if (is_qpower_enabled) {
			WMA_LOGD("enable Qpower in resume mode!");
			ret = wmi_unified_set_sta_ps_param(wma->wmi_handle,
						vdev_id,
						WMI_STA_PS_ENABLE_QPOWER,
						1);
			if (ret)
				WMA_LOGE("Failed to enable Qpower in resume mode!");
		}
	}
}

static void wma_set_resume_dtim(tp_wma_handle wma)
{
	u_int8_t i;

	if (NULL == wma) {
		WMA_LOGE("%s: wma is NULL", __func__);
		return;
	}

	for (i = 0; i < wma->max_bssid; i++) {
		if ((wma->interfaces[i].handle) &&
		    (false == wma->interfaces[i].alt_modulated_dtim_enabled)) {
			wma_set_vdev_resume_dtim(wma, i);
		}
	}
}

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
void wma_process_roam_synch_complete(WMA_HANDLE handle,
		tSirSmeRoamOffloadSynchCnf *synchcnf)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	wmi_roam_synch_complete_fixed_param* cmd;
	wmi_buf_t wmi_buf;
	u_int8_t *buf_ptr;
	u_int16_t len;
	v_BOOL_t roam_synch_in_progress;
	len = sizeof(wmi_roam_synch_complete_fixed_param);

	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not issue roam synch cnf",
				__func__);
		return;
	}
	roam_synch_in_progress =
	wma_handle->interfaces[synchcnf->sessionId].roam_synch_in_progress;
	if (roam_synch_in_progress == VOS_FALSE) {
	  WMA_LOGE("%s: Dont send the roam synch complete since Roam Synch"
	  "Propagation is not in Progress", __func__);
	  return;
	} else {
	  adf_os_spin_lock_bh(&wma_handle->roam_synch_lock);
	  wma_handle->interfaces[synchcnf->sessionId].roam_synch_in_progress =
	  VOS_FALSE;
	  adf_os_spin_unlock_bh(&wma_handle->roam_synch_lock);
	}
	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!wmi_buf) {
		WMA_LOGE("%s: wmi_buf_alloc failed", __func__);
		return;
	}
	cmd = (wmi_roam_synch_complete_fixed_param *)wmi_buf_data(wmi_buf);
	buf_ptr = (u_int8_t *) cmd;
	WMITLV_SET_HDR(&cmd->tlv_header,
			WMITLV_TAG_STRUC_wmi_roam_synch_complete_fixed_param,
			WMITLV_GET_STRUCT_TLVLEN(wmi_roam_synch_complete_fixed_param));
	cmd->vdev_id = synchcnf->sessionId;
	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, len,
				WMI_ROAM_SYNCH_COMPLETE)) {
		WMA_LOGP("%s: failed to send roam synch confirmation", __func__);
		adf_nbuf_free(wmi_buf);
		return;
	}
	return;
}
void wma_process_roam_synch_fail(WMA_HANDLE handle,
		tSirRoamOffloadSynchFail *synchfail)
{
	tp_wma_handle wma_handle = (tp_wma_handle) handle;
	if (!wma_handle || !wma_handle->wmi_handle) {
		WMA_LOGE("%s: WMA is closed, can not clean-up roam synch",
				__func__);
		return;
	}
	/* Hand Off Failure could happen as an exception, when a roam synch
	 * indication is posted to Host, but a roam synch complete is not
	 * posted to the firmware.So, clear the roam synch in progress
	 * flag before disconnecting the session through this event.*/
	adf_os_spin_lock_bh(&wma_handle->roam_synch_lock);
	wma_handle->interfaces[synchfail->sessionId].roam_synch_in_progress =
		VOS_FALSE;
	adf_os_spin_unlock_bh(&wma_handle->roam_synch_lock);
}
#endif

#ifdef FEATURE_RUNTIME_PM
int wma_runtime_suspend_req(WMA_HANDLE handle)
{
	vos_msg_t       vosMessage;
	VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
	int ret = 0;
	tp_wma_handle wma = (tp_wma_handle) handle;

	vos_event_reset(&wma->runtime_suspend);

	vosMessage.bodyptr = NULL;
	vosMessage.type    = WDA_RUNTIME_PM_SUSPEND_IND;
	vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage );

	if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {
		ret = -EAGAIN;
		return ret;
	}

	if (vos_wait_single_event(&wma->runtime_suspend,
				WMA_TGT_SUSPEND_COMPLETE_TIMEOUT) !=
			VOS_STATUS_SUCCESS) {
		WMA_LOGE("Failed to get runtime suspend event");
		ret = -EAGAIN;
		goto out;
	}

	/* Check if suspend completed, if not return failure */
	if (!wma_get_wow_bus_suspend(wma)) {
		WMA_LOGE("Runtime Suspend Failed: %d",
				wma_get_wow_bus_suspend(wma));
		ret = -EAGAIN;
	}
out:
	if (ret)
		wma_runtime_resume_req(wma);

	return ret;
}

int wma_runtime_resume_req(WMA_HANDLE handle)
{
	vos_msg_t       vosMessage;
	VOS_STATUS vosStatus = VOS_STATUS_SUCCESS;
	int ret = 0;

	vos_runtime_pm_prevent_suspend();

	vosMessage.bodyptr = NULL;
	vosMessage.type    = WDA_RUNTIME_PM_RESUME_IND;
	vosStatus = vos_mq_post_message(VOS_MQ_ID_WDA, &vosMessage );

	if (!VOS_IS_STATUS_SUCCESS(vosStatus)) {
		WMA_LOGE("Failed to post Runtime PM Resume IND to VOS");
		ret = -EAGAIN;
		vos_runtime_pm_allow_suspend();
	}

	return ret;
}
#endif
