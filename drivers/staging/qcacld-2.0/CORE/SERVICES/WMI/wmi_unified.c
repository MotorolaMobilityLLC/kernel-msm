/*
 * Copyright (c) 2014-2015 The Linux Foundation. All rights reserved.
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


/*
 * Host WMI unified implementation
 */
#include "athdefs.h"
#include "osapi_linux.h"
#include "a_types.h"
#include "a_debug.h"
#include "ol_if_athvar.h"
#include "ol_defines.h"
#include "ol_fw.h"
#include "htc_api.h"
#include "htc_api.h"
#include "dbglog_host.h"
#include "wmi.h"
#include "wmi_unified_priv.h"
#include "wma_api.h"
#include "wma.h"
#include "macTrace.h"
#if defined(HIF_PCI)
#include "if_pci.h"
#elif defined(HIF_USB)
#include "if_usb.h"
#endif

#define WMI_MIN_HEAD_ROOM 64

#ifdef WMI_INTERFACE_EVENT_LOGGING
/* WMI commands */
u_int32_t g_wmi_command_buf_idx = 0;
struct wmi_command_debug wmi_command_log_buffer[WMI_EVENT_DEBUG_MAX_ENTRY];

/* WMI commands TX completed */
u_int32_t g_wmi_command_tx_cmp_buf_idx = 0;
struct wmi_command_debug wmi_command_tx_cmp_log_buffer[WMI_EVENT_DEBUG_MAX_ENTRY];

/* WMI events when processed */
u_int32_t g_wmi_event_buf_idx = 0;
struct wmi_event_debug wmi_event_log_buffer[WMI_EVENT_DEBUG_MAX_ENTRY];

/* WMI events when queued */
u_int32_t g_wmi_rx_event_buf_idx = 0;
struct wmi_event_debug wmi_rx_event_log_buffer[WMI_EVENT_DEBUG_MAX_ENTRY];

#define WMI_COMMAND_RECORD(a, b) {					\
	if (WMI_EVENT_DEBUG_MAX_ENTRY <= g_wmi_command_buf_idx)		\
		g_wmi_command_buf_idx = 0;				\
	wmi_command_log_buffer[g_wmi_command_buf_idx].command = a;	\
	adf_os_mem_copy(wmi_command_log_buffer[g_wmi_command_buf_idx].data, b, 16);\
	wmi_command_log_buffer[g_wmi_command_buf_idx].time =		\
		adf_get_boottime();					\
	g_wmi_command_buf_idx++;					\
}

#define WMI_COMMAND_TX_CMP_RECORD(a, b) {				\
	if (WMI_EVENT_DEBUG_MAX_ENTRY <= g_wmi_command_tx_cmp_buf_idx)	\
		g_wmi_command_tx_cmp_buf_idx = 0;			\
	wmi_command_tx_cmp_log_buffer[g_wmi_command_tx_cmp_buf_idx].command = a;\
	adf_os_mem_copy(wmi_command_tx_cmp_log_buffer			\
		[g_wmi_command_tx_cmp_buf_idx].data, b, 16);		\
	wmi_command_tx_cmp_log_buffer[g_wmi_command_tx_cmp_buf_idx].time =\
		adf_get_boottime();					\
	g_wmi_command_tx_cmp_buf_idx++;					\
}

#define WMI_EVENT_RECORD(a, b) {					\
	if (WMI_EVENT_DEBUG_MAX_ENTRY <= g_wmi_event_buf_idx)		\
		g_wmi_event_buf_idx = 0;				\
	wmi_event_log_buffer[g_wmi_event_buf_idx].event = a;		\
	adf_os_mem_copy(wmi_event_log_buffer[g_wmi_event_buf_idx].data, b, 16);\
	wmi_event_log_buffer[g_wmi_event_buf_idx].time =		\
		adf_get_boottime();					\
	g_wmi_event_buf_idx++;						\
}

#define WMI_RX_EVENT_RECORD(a,b) {					\
	if (WMI_EVENT_DEBUG_MAX_ENTRY <= g_wmi_rx_event_buf_idx)	\
		g_wmi_rx_event_buf_idx = 0;					\
	wmi_rx_event_log_buffer[g_wmi_rx_event_buf_idx].event = a;	\
	adf_os_mem_copy(wmi_rx_event_log_buffer[g_wmi_rx_event_buf_idx].data, b, 16);\
	wmi_rx_event_log_buffer[g_wmi_rx_event_buf_idx].time =		\
		adf_get_boottime();					\
	g_wmi_rx_event_buf_idx++;					\
}

#endif /*WMI_INTERFACE_EVENT_LOGGING*/


static void __wmi_control_rx(struct wmi_unified *wmi_handle, wmi_buf_t evt_buf);
int wmi_get_host_credits(wmi_unified_t wmi_handle);
/* WMI buffer APIs */

/**
 * wmi_get_max_msg_len() - get maximum WMI message length
 * @wmi_handle: WMI handle.
 *
 * This function returns the maximum WMI message length
 *
 * Return: maximum WMI message length
 */
uint16_t wmi_get_max_msg_len(wmi_unified_t wmi_handle)
{
	return wmi_handle->max_msg_len - WMI_MIN_HEAD_ROOM;
}

wmi_buf_t
wmi_buf_alloc(wmi_unified_t wmi_handle, u_int16_t len)
{
	wmi_buf_t wmi_buf;

	if (roundup(len + WMI_MIN_HEAD_ROOM, 4) >
				wmi_handle->max_msg_len) {
		VOS_ASSERT(0);
		return NULL;
	}
	wmi_buf = adf_nbuf_alloc(NULL, roundup(len + WMI_MIN_HEAD_ROOM, 4),
				 WMI_MIN_HEAD_ROOM, 4, FALSE);
	if (!wmi_buf)
		return NULL;

	/* Clear the wmi buffer */
	OS_MEMZERO(adf_nbuf_data(wmi_buf), len);

	/*
	 * Set the length of the buffer to match the allocation size.
	 */
	adf_nbuf_set_pktlen(wmi_buf, len);
	return wmi_buf;
}

static u_int8_t* get_wmi_cmd_string(WMI_CMD_ID wmi_command)
{
	switch(wmi_command)
	{
		/** initialize the wlan sub system */
		CASE_RETURN_STRING(WMI_INIT_CMDID);

		/* Scan specific commands */

		/** start scan request to FW  */
		CASE_RETURN_STRING(WMI_START_SCAN_CMDID);
		/** stop scan request to FW  */
		CASE_RETURN_STRING(WMI_STOP_SCAN_CMDID);
		/** full list of channels as defined by the regulatory that will be used by scanner   */
		CASE_RETURN_STRING(WMI_SCAN_CHAN_LIST_CMDID);
		/** overwrite default priority table in scan scheduler   */
		CASE_RETURN_STRING(WMI_SCAN_SCH_PRIO_TBL_CMDID);
		/** This command to adjust the priority and min.max_rest_time
		 * of an on ongoing scan request.
		 */
		CASE_RETURN_STRING(WMI_SCAN_UPDATE_REQUEST_CMDID);

		/* PDEV(physical device) specific commands */
		/** set regulatorty ctl id used by FW to determine the exact ctl power limits */
		CASE_RETURN_STRING(WMI_PDEV_SET_REGDOMAIN_CMDID);
		/** set channel. mainly used for supporting monitor mode */
		CASE_RETURN_STRING(WMI_PDEV_SET_CHANNEL_CMDID);
		/** set pdev specific parameters */
		CASE_RETURN_STRING(WMI_PDEV_SET_PARAM_CMDID);
		/** enable packet log */
		CASE_RETURN_STRING(WMI_PDEV_PKTLOG_ENABLE_CMDID);
		/** disable packet log*/
		CASE_RETURN_STRING(WMI_PDEV_PKTLOG_DISABLE_CMDID);
		/** set wmm parameters */
		CASE_RETURN_STRING(WMI_PDEV_SET_WMM_PARAMS_CMDID);
		/** set HT cap ie that needs to be carried probe requests HT/VHT channels */
		CASE_RETURN_STRING(WMI_PDEV_SET_HT_CAP_IE_CMDID);
		/** set VHT cap ie that needs to be carried on probe requests on VHT channels */
		CASE_RETURN_STRING(WMI_PDEV_SET_VHT_CAP_IE_CMDID);

		/** Command to send the DSCP-to-TID map to the target */
		CASE_RETURN_STRING(WMI_PDEV_SET_DSCP_TID_MAP_CMDID);
		/** set quiet ie parameters. primarily used in AP mode */
		CASE_RETURN_STRING(WMI_PDEV_SET_QUIET_MODE_CMDID);
		/** Enable/Disable Green AP Power Save  */
		CASE_RETURN_STRING(WMI_PDEV_GREEN_AP_PS_ENABLE_CMDID);
		/** get TPC config for the current operating channel */
		CASE_RETURN_STRING(WMI_PDEV_GET_TPC_CONFIG_CMDID);

		/** set the base MAC address for the physical device before a VDEV is created.
		 *  For firmware that doesn’t support this feature and this command, the pdev
		 *  MAC address will not be changed. */
		CASE_RETURN_STRING(WMI_PDEV_SET_BASE_MACADDR_CMDID);

		/* eeprom content dump , the same to bdboard data */
		CASE_RETURN_STRING(WMI_PDEV_DUMP_CMDID);

		/* VDEV(virtual device) specific commands */
		/** vdev create */
		CASE_RETURN_STRING(WMI_VDEV_CREATE_CMDID);
		/** vdev delete */
		CASE_RETURN_STRING(WMI_VDEV_DELETE_CMDID);
		/** vdev start request */
		CASE_RETURN_STRING(WMI_VDEV_START_REQUEST_CMDID);
		/** vdev restart request (RX only, NO TX, used for CAC period)*/
		CASE_RETURN_STRING(WMI_VDEV_RESTART_REQUEST_CMDID);
		/** vdev up request */
		CASE_RETURN_STRING(WMI_VDEV_UP_CMDID);
		/** vdev stop request */
		CASE_RETURN_STRING(WMI_VDEV_STOP_CMDID);
		/** vdev down request */
		CASE_RETURN_STRING(WMI_VDEV_DOWN_CMDID);
		/* set a vdev param */
		CASE_RETURN_STRING(WMI_VDEV_SET_PARAM_CMDID);
		/* set a key (used for setting per peer unicast and per vdev multicast) */
		CASE_RETURN_STRING(WMI_VDEV_INSTALL_KEY_CMDID);

		/* wnm sleep mode command */
		CASE_RETURN_STRING(WMI_VDEV_WNM_SLEEPMODE_CMDID);
		CASE_RETURN_STRING(WMI_VDEV_WMM_ADDTS_CMDID);
		CASE_RETURN_STRING(WMI_VDEV_WMM_DELTS_CMDID);
		CASE_RETURN_STRING(WMI_VDEV_SET_WMM_PARAMS_CMDID);
		CASE_RETURN_STRING(WMI_VDEV_SET_GTX_PARAMS_CMDID);
		CASE_RETURN_STRING(WMI_VDEV_IPSEC_NATKEEPALIVE_FILTER_CMDID);

		CASE_RETURN_STRING(WMI_VDEV_PLMREQ_START_CMDID);
		CASE_RETURN_STRING(WMI_VDEV_PLMREQ_STOP_CMDID);
		CASE_RETURN_STRING(WMI_VDEV_TSF_TSTAMP_ACTION_CMDID);
		CASE_RETURN_STRING(WMI_VDEV_SET_IE_CMDID);
		/* peer specific commands */

		/** create a peer */
		CASE_RETURN_STRING(WMI_PEER_CREATE_CMDID);
		/** delete a peer */
		CASE_RETURN_STRING(WMI_PEER_DELETE_CMDID);
		/** flush specific  tid queues of a peer */
		CASE_RETURN_STRING(WMI_PEER_FLUSH_TIDS_CMDID);
		/** set a parameter of a peer */
		CASE_RETURN_STRING(WMI_PEER_SET_PARAM_CMDID);
		/** set peer to associated state. will cary all parameters determined during assocication time */
		CASE_RETURN_STRING(WMI_PEER_ASSOC_CMDID);
		/**add a wds  (4 address ) entry. used only for testing WDS feature on AP products */
		CASE_RETURN_STRING(WMI_PEER_ADD_WDS_ENTRY_CMDID);
		/**remove wds  (4 address ) entry. used only for testing WDS feature on AP products */
		CASE_RETURN_STRING(WMI_PEER_REMOVE_WDS_ENTRY_CMDID);
		/** set up mcast group infor for multicast to unicast conversion */
		CASE_RETURN_STRING(WMI_PEER_MCAST_GROUP_CMDID);
		/** request peer info from FW. FW shall respond with PEER_INFO_EVENTID */
		CASE_RETURN_STRING(WMI_PEER_INFO_REQ_CMDID);

		/* beacon/management specific commands */

		/** transmit beacon by reference . used for transmitting beacon on low latency interface like pcie */
		CASE_RETURN_STRING(WMI_BCN_TX_CMDID);
		/** transmit beacon by value */
		CASE_RETURN_STRING(WMI_PDEV_SEND_BCN_CMDID);
		/** set the beacon template. used in beacon offload mode to setup the
		 *  the common beacon template with the FW to be used by FW to generate beacons */
		CASE_RETURN_STRING(WMI_BCN_TMPL_CMDID);
		/** set beacon filter with FW */
		CASE_RETURN_STRING(WMI_BCN_FILTER_RX_CMDID);
		/* enable/disable filtering of probe requests in the firmware */
		CASE_RETURN_STRING(WMI_PRB_REQ_FILTER_RX_CMDID);
		/** transmit management frame by value. will be deprecated */
		CASE_RETURN_STRING(WMI_MGMT_TX_CMDID);
		/** set the probe response template. used in beacon offload mode to setup the
		 *  the common probe response template with the FW to be used by FW to generate
		 *  probe responses */
		CASE_RETURN_STRING(WMI_PRB_TMPL_CMDID);

		/** commands to directly control ba negotiation directly from host. only used in test mode */

		/** turn off FW Auto addba mode and let host control addba */
		CASE_RETURN_STRING(WMI_ADDBA_CLEAR_RESP_CMDID);
		/** send add ba request */
		CASE_RETURN_STRING(WMI_ADDBA_SEND_CMDID);
		CASE_RETURN_STRING(WMI_ADDBA_STATUS_CMDID);
		/** send del ba */
		CASE_RETURN_STRING(WMI_DELBA_SEND_CMDID);
		/** set add ba response will be used by FW to generate addba response*/
		CASE_RETURN_STRING(WMI_ADDBA_SET_RESP_CMDID);
		/** send single VHT MPDU with AMSDU */
		CASE_RETURN_STRING(WMI_SEND_SINGLEAMSDU_CMDID);

		/** Station power save specific config */
		/** enable/disable station powersave */
		CASE_RETURN_STRING(WMI_STA_POWERSAVE_MODE_CMDID);
		/** set station power save specific parameter */
		CASE_RETURN_STRING(WMI_STA_POWERSAVE_PARAM_CMDID);
		/** set station mimo powersave mode */
		CASE_RETURN_STRING(WMI_STA_MIMO_PS_MODE_CMDID);


		/** DFS-specific commands */
		/** enable DFS (radar detection)*/
		CASE_RETURN_STRING(WMI_PDEV_DFS_ENABLE_CMDID);
		/** disable DFS (radar detection)*/
		CASE_RETURN_STRING(WMI_PDEV_DFS_DISABLE_CMDID);
		/** enable DFS phyerr/parse filter offload */
		CASE_RETURN_STRING(WMI_DFS_PHYERR_FILTER_ENA_CMDID);
		/** enable DFS phyerr/parse filter offload */
		CASE_RETURN_STRING(WMI_DFS_PHYERR_FILTER_DIS_CMDID);

		/* Roaming specific  commands */
		/** set roam scan mode */
		CASE_RETURN_STRING(WMI_ROAM_SCAN_MODE);
		/** set roam scan rssi threshold below which roam scan is enabled  */
		CASE_RETURN_STRING(WMI_ROAM_SCAN_RSSI_THRESHOLD);
		/** set roam scan period for periodic roam scan mode  */
		CASE_RETURN_STRING(WMI_ROAM_SCAN_PERIOD);
		/** set roam scan trigger rssi change threshold   */
		CASE_RETURN_STRING(WMI_ROAM_SCAN_RSSI_CHANGE_THRESHOLD);
		/** set roam AP profile   */
		CASE_RETURN_STRING(WMI_ROAM_AP_PROFILE);
		/** set channel list for roam scans */
		CASE_RETURN_STRING(WMI_ROAM_CHAN_LIST);
		/** offload scan specific commands */
		/** set offload scan AP profile   */
		CASE_RETURN_STRING(WMI_OFL_SCAN_ADD_AP_PROFILE);
		/** remove offload scan AP profile   */
		CASE_RETURN_STRING(WMI_OFL_SCAN_REMOVE_AP_PROFILE);
		/** set offload scan period   */
		CASE_RETURN_STRING(WMI_OFL_SCAN_PERIOD);

		/* P2P specific commands */
		/**set P2P device info. FW will used by FW to create P2P IE to be carried in probe response
		 * generated during p2p listen and for p2p discoverability  */
		CASE_RETURN_STRING(WMI_P2P_DEV_SET_DEVICE_INFO);
		/** enable/disable p2p discoverability on STA/AP VDEVs  */
		CASE_RETURN_STRING(WMI_P2P_DEV_SET_DISCOVERABILITY);
		/** set p2p ie to be carried in beacons generated by FW for GO  */
		CASE_RETURN_STRING(WMI_P2P_GO_SET_BEACON_IE);
		/** set p2p ie to be carried in probe response frames generated by FW for GO  */
		CASE_RETURN_STRING(WMI_P2P_GO_SET_PROBE_RESP_IE);
		/** set the vendor specific p2p ie data. FW will use this to parse the P2P NoA
		 *  attribute in the beacons/probe responses received.
		 */
		CASE_RETURN_STRING(WMI_P2P_SET_VENDOR_IE_DATA_CMDID);
		/** set the configure of p2p find offload */
		CASE_RETURN_STRING(WMI_P2P_DISC_OFFLOAD_CONFIG_CMDID);
		/** set the vendor specific p2p ie data for p2p find offload using */
		CASE_RETURN_STRING(WMI_P2P_DISC_OFFLOAD_APPIE_CMDID);
		/** set the BSSID/device name pattern of p2p find offload */
		CASE_RETURN_STRING(WMI_P2P_DISC_OFFLOAD_PATTERN_CMDID);
		/** set OppPS related parameters **/
		CASE_RETURN_STRING(WMI_P2P_SET_OPPPS_PARAM_CMDID);

		/** AP power save specific config */
		/** set AP power save specific param */
		CASE_RETURN_STRING(WMI_AP_PS_PEER_PARAM_CMDID);
		/** set AP UAPSD coex pecific param */
		CASE_RETURN_STRING(WMI_AP_PS_PEER_UAPSD_COEX_CMDID);


		/** Rate-control specific commands */
		CASE_RETURN_STRING(WMI_PEER_RATE_RETRY_SCHED_CMDID);

		/** WLAN Profiling commands. */
		CASE_RETURN_STRING(WMI_WLAN_PROFILE_TRIGGER_CMDID);
		CASE_RETURN_STRING(WMI_WLAN_PROFILE_SET_HIST_INTVL_CMDID);
		CASE_RETURN_STRING(WMI_WLAN_PROFILE_GET_PROFILE_DATA_CMDID);
		CASE_RETURN_STRING(WMI_WLAN_PROFILE_ENABLE_PROFILE_ID_CMDID);
		CASE_RETURN_STRING(WMI_WLAN_PROFILE_LIST_PROFILE_ID_CMDID);

		/** Suspend resume command Ids */
		CASE_RETURN_STRING(WMI_PDEV_SUSPEND_CMDID);
		CASE_RETURN_STRING(WMI_PDEV_RESUME_CMDID);

		/* Beacon filter commands */
		/** add a beacon filter */
		CASE_RETURN_STRING(WMI_ADD_BCN_FILTER_CMDID);
		/** remove a  beacon filter */
		CASE_RETURN_STRING(WMI_RMV_BCN_FILTER_CMDID);

		/* WOW Specific WMI commands*/
		/** add pattern for awake */
		CASE_RETURN_STRING(WMI_WOW_ADD_WAKE_PATTERN_CMDID);
		/** deleta a wake pattern */
		CASE_RETURN_STRING(WMI_WOW_DEL_WAKE_PATTERN_CMDID);
		/** enable/deisable wake event  */
		CASE_RETURN_STRING(WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID);
		/** enable WOW  */
		CASE_RETURN_STRING(WMI_WOW_ENABLE_CMDID);
		/** host woke up from sleep event to FW. Generated in response to WOW Hardware event */
		CASE_RETURN_STRING(WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID);

		/* RTT measurement related cmd */
		/** reques to make an RTT measurement */
		CASE_RETURN_STRING(WMI_RTT_MEASREQ_CMDID);
		/** reques to report a tsf measurement */
		CASE_RETURN_STRING(WMI_RTT_TSF_CMDID);

		/** spectral scan command */
		/** configure spectral scan */
		CASE_RETURN_STRING(WMI_VDEV_SPECTRAL_SCAN_CONFIGURE_CMDID);
		/** enable/disable spectral scan and trigger */
		CASE_RETURN_STRING(WMI_VDEV_SPECTRAL_SCAN_ENABLE_CMDID);

		/* F/W stats */
		/** one time request for stats */
		CASE_RETURN_STRING(WMI_REQUEST_STATS_CMDID);
		/** Push MCC Adaptive Scheduler Stats to Firmware */
		CASE_RETURN_STRING(WMI_MCC_SCHED_TRAFFIC_STATS_CMDID);

		/** ARP OFFLOAD REQUEST*/
		CASE_RETURN_STRING(WMI_SET_ARP_NS_OFFLOAD_CMDID);

		/** Proactive ARP Response Add Pattern Command*/
		CASE_RETURN_STRING(WMI_ADD_PROACTIVE_ARP_RSP_PATTERN_CMDID);

		/** Proactive ARP Response Del Pattern Command*/
		CASE_RETURN_STRING(WMI_DEL_PROACTIVE_ARP_RSP_PATTERN_CMDID);

		/** NS offload confid*/
		CASE_RETURN_STRING(WMI_NETWORK_LIST_OFFLOAD_CONFIG_CMDID);

		/* GTK offload Specific WMI commands*/
		CASE_RETURN_STRING(WMI_GTK_OFFLOAD_CMDID);

		/* CSA offload Specific WMI commands*/
		/** csa offload enable */
		CASE_RETURN_STRING(WMI_CSA_OFFLOAD_ENABLE_CMDID);
		/** chan switch command */
		CASE_RETURN_STRING(WMI_CSA_OFFLOAD_CHANSWITCH_CMDID);

		/* Chatter commands*/
		/* Change chatter mode of operation */
		CASE_RETURN_STRING(WMI_CHATTER_SET_MODE_CMDID);
		/** chatter add coalescing filter command */
		CASE_RETURN_STRING(WMI_CHATTER_ADD_COALESCING_FILTER_CMDID);
		/** chatter delete coalescing filter command */
		CASE_RETURN_STRING(WMI_CHATTER_DELETE_COALESCING_FILTER_CMDID);
		/** chatter coalecing query command */
		CASE_RETURN_STRING(WMI_CHATTER_COALESCING_QUERY_CMDID);

		/**addba specific commands */
		/** start the aggregation on this TID */
		CASE_RETURN_STRING(WMI_PEER_TID_ADDBA_CMDID);
		/** stop the aggregation on this TID */
		CASE_RETURN_STRING(WMI_PEER_TID_DELBA_CMDID);

		/** set station mimo powersave method */
		CASE_RETURN_STRING(WMI_STA_DTIM_PS_METHOD_CMDID);
		/** Configure the Station UAPSD AC Auto Trigger Parameters */
		CASE_RETURN_STRING(WMI_STA_UAPSD_AUTO_TRIG_CMDID);
		/** Configure the Keep Alive Parameters */
		CASE_RETURN_STRING(WMI_STA_KEEPALIVE_CMDID);

		/* Request ssn from target for a sta/tid pair */
		CASE_RETURN_STRING(WMI_BA_REQ_SSN_CMDID);
		/* misc command group */
		/** echo command mainly used for testing */
		CASE_RETURN_STRING(WMI_ECHO_CMDID);

		/* !!IMPORTANT!!
		 * If you need to add a new WMI command to the CASE_RETURN_STRING(WMI_GRP_MISC sub-group,
		 * please make sure you add it BEHIND CASE_RETURN_STRING(WMI_PDEV_UTF_CMDID);
		 * as we MUST have a fixed value here to maintain compatibility between
		 * UTF and the ART2 driver
		 */
		/** UTF WMI commands */
		CASE_RETURN_STRING(WMI_PDEV_UTF_CMDID);

		/** set debug log config */
		CASE_RETURN_STRING(WMI_DBGLOG_CFG_CMDID);
		/* QVIT specific command id */
		CASE_RETURN_STRING(WMI_PDEV_QVIT_CMDID);
		/* Factory Testing Mode request command
		 * used for integrated chipsets */
		CASE_RETURN_STRING(WMI_PDEV_FTM_INTG_CMDID);
		/* set and get keepalive parameters command */
		CASE_RETURN_STRING(WMI_VDEV_SET_KEEPALIVE_CMDID);
		CASE_RETURN_STRING(WMI_VDEV_GET_KEEPALIVE_CMDID);
		/* For fw recovery test command */
		CASE_RETURN_STRING(WMI_FORCE_FW_HANG_CMDID);
		/* Set Mcast/Bdcast filter */
		CASE_RETURN_STRING(WMI_SET_MCASTBCAST_FILTER_CMDID);
		/** set thermal management params **/
		CASE_RETURN_STRING(WMI_THERMAL_MGMT_CMDID);
		CASE_RETURN_STRING(WMI_RSSI_BREACH_MONITOR_CONFIG_CMDID);
                CASE_RETURN_STRING(WMI_LRO_CONFIG_CMDID);
                CASE_RETURN_STRING(WMI_TRANSFER_DATA_TO_FLASH_CMDID);
                CASE_RETURN_STRING(WMI_MAWC_SENSOR_REPORT_IND_CMDID);
                CASE_RETURN_STRING(WMI_ROAM_CONFIGURE_MAWC_CMDID);
                CASE_RETURN_STRING(WMI_NLO_CONFIGURE_MAWC_CMDID);
                CASE_RETURN_STRING(WMI_EXTSCAN_CONFIGURE_MAWC_CMDID);
		/* GPIO Configuration */
		CASE_RETURN_STRING(WMI_GPIO_CONFIG_CMDID);
		CASE_RETURN_STRING(WMI_GPIO_OUTPUT_CMDID);

		/* Txbf configuration command */
		CASE_RETURN_STRING(WMI_TXBF_CMDID);

		/* FWTEST Commands */
		CASE_RETURN_STRING(WMI_FWTEST_VDEV_MCC_SET_TBTT_MODE_CMDID);
		/** set NoA descs **/
		CASE_RETURN_STRING(WMI_FWTEST_P2P_SET_NOA_PARAM_CMDID);

		/** TDLS Configuration */
		/** enable/disable TDLS */
		CASE_RETURN_STRING(WMI_TDLS_SET_STATE_CMDID);
		/** set tdls peer state */
		CASE_RETURN_STRING(WMI_TDLS_PEER_UPDATE_CMDID);

		/** Resmgr Configuration */
		/** Adaptive OCS is enabled by default in the FW. This command is used to
		 * disable FW based adaptive OCS.
		 */
		CASE_RETURN_STRING(WMI_RESMGR_ADAPTIVE_OCS_ENABLE_DISABLE_CMDID);
		/** set the requested channel time quota for the home channels */
		CASE_RETURN_STRING(WMI_RESMGR_SET_CHAN_TIME_QUOTA_CMDID);
		/** set the requested latency for the home channels */
		CASE_RETURN_STRING(WMI_RESMGR_SET_CHAN_LATENCY_CMDID);

		/** STA SMPS Configuration */
		/** force SMPS mode */
		CASE_RETURN_STRING(WMI_STA_SMPS_FORCE_MODE_CMDID);
		/** set SMPS parameters */
		CASE_RETURN_STRING(WMI_STA_SMPS_PARAM_CMDID);

		/* Wlan HB commands*/
		/* enalbe/disable wlan HB */
		CASE_RETURN_STRING(WMI_HB_SET_ENABLE_CMDID);
		/* set tcp parameters for wlan HB */
		CASE_RETURN_STRING(WMI_HB_SET_TCP_PARAMS_CMDID);
		/* set tcp pkt filter for wlan HB */
		CASE_RETURN_STRING(WMI_HB_SET_TCP_PKT_FILTER_CMDID);
		/* set udp parameters for wlan HB */
		CASE_RETURN_STRING(WMI_HB_SET_UDP_PARAMS_CMDID);
		/* set udp pkt filter for wlan HB */
		CASE_RETURN_STRING(WMI_HB_SET_UDP_PKT_FILTER_CMDID);

		/** Wlan RMC commands*/
		/** enable/disable RMC */
		CASE_RETURN_STRING(WMI_RMC_SET_MODE_CMDID);
		/** configure action frame period */
		CASE_RETURN_STRING(WMI_RMC_SET_ACTION_PERIOD_CMDID);
		/** For debug/future enhancement purposes only,
		 *  configures/finetunes RMC algorithms */
		CASE_RETURN_STRING(WMI_RMC_CONFIG_CMDID);

		/** WLAN MHF offload commands */
		/** enable/disable MHF offload */
		CASE_RETURN_STRING(WMI_MHF_OFFLOAD_SET_MODE_CMDID);
		/** Plumb routing table for MHF offload */
		CASE_RETURN_STRING(WMI_MHF_OFFLOAD_PLUMB_ROUTING_TBL_CMDID);

		/*location scan commands*/
		/*start batch scan*/
		CASE_RETURN_STRING(WMI_BATCH_SCAN_ENABLE_CMDID);
		/*stop batch scan*/
		CASE_RETURN_STRING(WMI_BATCH_SCAN_DISABLE_CMDID);
		/*get batch scan result*/
		CASE_RETURN_STRING(WMI_BATCH_SCAN_TRIGGER_RESULT_CMDID);
		/* OEM related cmd */
		CASE_RETURN_STRING(WMI_OEM_REQ_CMDID);
		CASE_RETURN_STRING(WMI_OEM_REQUEST_CMDID);
		/* NAN request cmd */
		CASE_RETURN_STRING(WMI_NAN_CMDID);
		/* Modem power state cmd */
		CASE_RETURN_STRING(WMI_MODEM_POWER_STATE_CMDID);
		CASE_RETURN_STRING(WMI_REQUEST_STATS_EXT_CMDID);
		CASE_RETURN_STRING(WMI_OBSS_SCAN_ENABLE_CMDID);
		CASE_RETURN_STRING(WMI_OBSS_SCAN_DISABLE_CMDID);
		CASE_RETURN_STRING(WMI_PEER_GET_ESTIMATED_LINKSPEED_CMDID);
		CASE_RETURN_STRING(WMI_ROAM_SCAN_CMD);
		CASE_RETURN_STRING(WMI_PDEV_SET_LED_CONFIG_CMDID);
		CASE_RETURN_STRING(WMI_HOST_AUTO_SHUTDOWN_CFG_CMDID);
		CASE_RETURN_STRING(WMI_CHAN_AVOID_UPDATE_CMDID);
		CASE_RETURN_STRING(WMI_WOW_IOAC_ADD_KEEPALIVE_CMDID);
		CASE_RETURN_STRING(WMI_WOW_IOAC_DEL_KEEPALIVE_CMDID);
		CASE_RETURN_STRING(WMI_WOW_IOAC_ADD_WAKE_PATTERN_CMDID);
		CASE_RETURN_STRING(WMI_WOW_IOAC_DEL_WAKE_PATTERN_CMDID);
		CASE_RETURN_STRING(WMI_REQUEST_LINK_STATS_CMDID);
		CASE_RETURN_STRING(WMI_START_LINK_STATS_CMDID);
		CASE_RETURN_STRING(WMI_CLEAR_LINK_STATS_CMDID);
		CASE_RETURN_STRING(WMI_GET_FW_MEM_DUMP_CMDID);
		CASE_RETURN_STRING(WMI_LPI_MGMT_SNOOPING_CONFIG_CMDID);
		CASE_RETURN_STRING(WMI_LPI_START_SCAN_CMDID);
		CASE_RETURN_STRING(WMI_LPI_STOP_SCAN_CMDID);
		CASE_RETURN_STRING(WMI_EXTSCAN_START_CMDID);
		CASE_RETURN_STRING(WMI_EXTSCAN_STOP_CMDID);
		CASE_RETURN_STRING(WMI_EXTSCAN_CONFIGURE_WLAN_CHANGE_MONITOR_CMDID);
		CASE_RETURN_STRING(WMI_EXTSCAN_CONFIGURE_HOTLIST_MONITOR_CMDID);
		CASE_RETURN_STRING(WMI_EXTSCAN_GET_CACHED_RESULTS_CMDID);
		CASE_RETURN_STRING(WMI_EXTSCAN_GET_WLAN_CHANGE_RESULTS_CMDID);
		CASE_RETURN_STRING(WMI_EXTSCAN_SET_CAPABILITIES_CMDID);
		CASE_RETURN_STRING(WMI_EXTSCAN_GET_CAPABILITIES_CMDID);
		CASE_RETURN_STRING(WMI_EXTSCAN_CONFIGURE_HOTLIST_SSID_MONITOR_CMDID);
		CASE_RETURN_STRING(WMI_ROAM_SYNCH_COMPLETE);
		CASE_RETURN_STRING(WMI_D0_WOW_ENABLE_DISABLE_CMDID);
		CASE_RETURN_STRING(WMI_EXTWOW_ENABLE_CMDID);
		CASE_RETURN_STRING(WMI_EXTWOW_SET_APP_TYPE1_PARAMS_CMDID);
		CASE_RETURN_STRING(WMI_EXTWOW_SET_APP_TYPE2_PARAMS_CMDID);
		CASE_RETURN_STRING(WMI_UNIT_TEST_CMDID);
		CASE_RETURN_STRING(WMI_ROAM_SET_RIC_REQUEST_CMDID);
		CASE_RETURN_STRING(WMI_PDEV_GET_TEMPERATURE_CMDID);
		CASE_RETURN_STRING(WMI_SET_DHCP_SERVER_OFFLOAD_CMDID);
		CASE_RETURN_STRING(WMI_TPC_CHAINMASK_CONFIG_CMDID);
		CASE_RETURN_STRING(WMI_IPA_OFFLOAD_ENABLE_DISABLE_CMDID);
		CASE_RETURN_STRING(WMI_SCAN_PROB_REQ_OUI_CMDID);
		CASE_RETURN_STRING(WMI_TDLS_SET_OFFCHAN_MODE_CMDID);
		CASE_RETURN_STRING(WMI_PDEV_SET_LED_FLASHING_CMDID);
		CASE_RETURN_STRING(WMI_MDNS_OFFLOAD_ENABLE_CMDID);
		CASE_RETURN_STRING(WMI_MDNS_SET_FQDN_CMDID);
		CASE_RETURN_STRING(WMI_MDNS_SET_RESPONSE_CMDID);
		CASE_RETURN_STRING(WMI_MDNS_GET_STATS_CMDID);
		CASE_RETURN_STRING(WMI_ROAM_INVOKE_CMDID);
		CASE_RETURN_STRING(WMI_SET_ANTENNA_DIVERSITY_CMDID);
		CASE_RETURN_STRING(WMI_SAP_OFL_ENABLE_CMDID);
		CASE_RETURN_STRING(WMI_APFIND_CMDID);
		CASE_RETURN_STRING(WMI_PASSPOINT_LIST_CONFIG_CMDID);
		CASE_RETURN_STRING(WMI_OCB_SET_SCHED_CMDID);
		CASE_RETURN_STRING(WMI_OCB_SET_CONFIG_CMDID);
		CASE_RETURN_STRING(WMI_OCB_SET_UTC_TIME_CMDID);
		CASE_RETURN_STRING(WMI_OCB_START_TIMING_ADVERT_CMDID);
		CASE_RETURN_STRING(WMI_OCB_STOP_TIMING_ADVERT_CMDID);
		CASE_RETURN_STRING(WMI_OCB_GET_TSF_TIMER_CMDID);
		CASE_RETURN_STRING(WMI_DCC_GET_STATS_CMDID);
		CASE_RETURN_STRING(WMI_DCC_CLEAR_STATS_CMDID);
		CASE_RETURN_STRING(WMI_DCC_UPDATE_NDL_CMDID);
		CASE_RETURN_STRING(WMI_ROAM_FILTER_CMDID);
		CASE_RETURN_STRING(WMI_ROAM_SUBNET_CHANGE_CONFIG_CMDID);
		CASE_RETURN_STRING(WMI_DEBUG_MESG_FLUSH_CMDID);
		CASE_RETURN_STRING(WMI_PEER_SET_RATE_REPORT_CONDITION_CMDID);
		CASE_RETURN_STRING(WMI_SOC_SET_PCL_CMDID);
		CASE_RETURN_STRING(WMI_SOC_SET_HW_MODE_CMDID);
		CASE_RETURN_STRING(WMI_SOC_SET_DUAL_MAC_CONFIG_CMDID);
		CASE_RETURN_STRING(WMI_WOW_ENABLE_ICMPV6_NA_FLT_CMDID);
		CASE_RETURN_STRING(WMI_DIAG_EVENT_LOG_CONFIG_CMDID);
		CASE_RETURN_STRING(WMI_PACKET_FILTER_CONFIG_CMDID);
		CASE_RETURN_STRING(WMI_PACKET_FILTER_ENABLE_CMDID);
		CASE_RETURN_STRING(WMI_SAP_SET_BLACKLIST_PARAM_CMDID);
		CASE_RETURN_STRING(WMI_WOW_UDP_SVC_OFLD_CMDID);
		CASE_RETURN_STRING(WMI_MGMT_TX_SEND_CMDID);
		CASE_RETURN_STRING(WMI_SOC_SET_ANTENNA_MODE_CMDID);
		CASE_RETURN_STRING(WMI_WOW_HOSTWAKEUP_GPIO_PIN_PATTERN_CONFIG_CMDID);
		CASE_RETURN_STRING(WMI_AP_PS_EGAP_PARAM_CMDID);
		CASE_RETURN_STRING(WMI_PMF_OFFLOAD_SET_SA_QUERY_CMDID);
		CASE_RETURN_STRING(WMI_BPF_GET_CAPABILITY_CMDID);
		CASE_RETURN_STRING(WMI_BPF_GET_VDEV_STATS_CMDID);
		CASE_RETURN_STRING(WMI_BPF_SET_VDEV_INSTRUCTIONS_CMDID);
		CASE_RETURN_STRING(WMI_BPF_DEL_VDEV_INSTRUCTIONS_CMDID);
        }
	return "Invalid WMI cmd";
}

#ifdef FEATURE_RUNTIME_PM
inline bool wmi_get_runtime_pm_inprogress(wmi_unified_t wmi_handle)
{
	return adf_os_atomic_read(&wmi_handle->runtime_pm_inprogress);
}
#endif

/* WMI command API */
int wmi_unified_cmd_send(wmi_unified_t wmi_handle, wmi_buf_t buf, int len,
			 WMI_CMD_ID cmd_id)
{
	HTC_PACKET *pkt;
	A_STATUS status;
	void *vos_context;
	struct ol_softc *scn;
	A_UINT16 htc_tag = 0;

	if (wmi_get_runtime_pm_inprogress(wmi_handle))
		goto skip_suspend_check;

	if (adf_os_atomic_read(&wmi_handle->is_target_suspended) &&
			( (WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID != cmd_id) &&
			  (WMI_PDEV_RESUME_CMDID != cmd_id)) ) {
		pr_err("%s: Target is suspended  could not send WMI command: %d\n",
				__func__, cmd_id);
		VOS_ASSERT(0);
		return -EBUSY;
	} else
		goto dont_tag;

skip_suspend_check:
	switch(cmd_id) {
	case WMI_WOW_ENABLE_CMDID:
	case WMI_PDEV_SUSPEND_CMDID:
	case WMI_WOW_ENABLE_DISABLE_WAKE_EVENT_CMDID:
	case WMI_WOW_ADD_WAKE_PATTERN_CMDID:
	case WMI_WOW_HOSTWAKEUP_FROM_SLEEP_CMDID:
	case WMI_PDEV_RESUME_CMDID:
	case WMI_WOW_DEL_WAKE_PATTERN_CMDID:
#ifdef FEATURE_WLAN_D0WOW
	case WMI_D0_WOW_ENABLE_DISABLE_CMDID:
#endif
		htc_tag = HTC_TX_PACKET_TAG_AUTO_PM;
	case WMI_FORCE_FW_HANG_CMDID:
		if (wmi_handle->tag_crash_inject) {
			htc_tag = HTC_TX_PACKET_TAG_AUTO_PM;
			wmi_handle->tag_crash_inject = false;
		}
	default:
		break;
	}

dont_tag:
	/* Do sanity check on the TLV parameter structure */
	{
		void *buf_ptr = (void *) adf_nbuf_data(buf);

		if (wmitlv_check_command_tlv_params(NULL, buf_ptr, len, cmd_id) != 0)
		{
			adf_os_print("\nERROR: %s: Invalid WMI Parameter Buffer for Cmd:%d\n",
				     __func__, cmd_id);
			return -1;
		}
	}

	if (adf_nbuf_push_head(buf, sizeof(WMI_CMD_HDR)) == NULL) {
		pr_err("%s, Failed to send cmd %x, no memory\n",
		       __func__, cmd_id);
		return -ENOMEM;
	}

	WMI_SET_FIELD(adf_nbuf_data(buf), WMI_CMD_HDR, COMMANDID, cmd_id);

	adf_os_atomic_inc(&wmi_handle->pending_cmds);
	if (adf_os_atomic_read(&wmi_handle->pending_cmds) >= WMI_MAX_CMDS) {
		vos_context = vos_get_global_context(VOS_MODULE_ID_WDA, NULL);
		scn = vos_get_context(VOS_MODULE_ID_HIF, vos_context);
		pr_err("\n%s: hostcredits = %d\n", __func__,
		       wmi_get_host_credits(wmi_handle));
		HTC_dump_counter_info(wmi_handle->htc_handle);
		//dump_CE_register(scn);
		//dump_CE_debug_register(scn->hif_sc);
		adf_os_atomic_dec(&wmi_handle->pending_cmds);
		pr_err("%s: MAX %d WMI Pending cmds reached.\n",
			__func__, WMI_MAX_CMDS);
		VOS_BUG(0);
		return -EBUSY;
	}

	pkt = adf_os_mem_alloc(NULL, sizeof(*pkt));
	if (!pkt) {
		adf_os_atomic_dec(&wmi_handle->pending_cmds);
		pr_err("%s, Failed to alloc htc packet %x, no memory\n",
		       __func__, cmd_id);
		return -ENOMEM;
	}

	SET_HTC_PACKET_INFO_TX(pkt,
			NULL,
			adf_nbuf_data(buf),
			len + sizeof(WMI_CMD_HDR),
			/* htt_host_data_dl_len(buf)+20 */
			wmi_handle->wmi_endpoint_id,
			htc_tag);

	SET_HTC_PACKET_NET_BUF_CONTEXT(pkt, buf);

	WMA_LOGD("Send WMI command:%s command_id:%d",
			get_wmi_cmd_string(cmd_id), cmd_id);

#ifdef WMI_INTERFACE_EVENT_LOGGING
	adf_os_spin_lock_bh(&wmi_handle->wmi_record_lock);
        /*Record 16 bytes of WMI cmd data - exclude TLV and WMI headers*/
        WMI_COMMAND_RECORD(cmd_id ,((u_int32_t *)adf_nbuf_data(buf) + 2));
	adf_os_spin_unlock_bh(&wmi_handle->wmi_record_lock);
#endif

	status = HTCSendPkt(wmi_handle->htc_handle, pkt);

	if (A_OK != status) {
		adf_os_atomic_dec(&wmi_handle->pending_cmds);
		pr_err("%s %d, HTCSendPkt failed\n", __func__, __LINE__);
	}


	return ((status == A_OK) ? EOK : -1);
}


/* WMI Event handler register API */
int wmi_unified_get_event_handler_ix(wmi_unified_t wmi_handle,
					WMI_EVT_ID event_id)
{
	u_int32_t idx = 0;
	for (idx = 0; (idx < wmi_handle->max_event_idx &&
		idx < WMI_UNIFIED_MAX_EVENT); ++idx) {
		if (wmi_handle->event_id[idx] == event_id &&
			wmi_handle->event_handler[idx] != NULL ) {
			return idx;
		}
	}
	return  -1;
}

int wmi_unified_register_event_handler(wmi_unified_t wmi_handle,
                                       WMI_EVT_ID event_id,
				       wmi_unified_event_handler handler_func)
{
	u_int32_t idx=0;

    if ( wmi_unified_get_event_handler_ix( wmi_handle, event_id) != -1) {
	printk("%s : event handler already registered 0x%x \n",
		__func__, event_id);
        return -1;
    }
    if ( wmi_handle->max_event_idx == WMI_UNIFIED_MAX_EVENT ) {
	printk("%s : no more event handlers 0x%x \n",
                __func__, event_id);
        return -1;
    }
    idx=wmi_handle->max_event_idx;
    wmi_handle->event_handler[idx] = handler_func;
    wmi_handle->event_id[idx] = event_id;
    wmi_handle->max_event_idx++;

    return 0;
}

int wmi_unified_unregister_event_handler(wmi_unified_t wmi_handle,
                                       WMI_EVT_ID event_id)
{
    u_int32_t idx=0;
    if ( (idx = wmi_unified_get_event_handler_ix( wmi_handle, event_id)) == -1) {
        printk("%s : event handler is not registered: event id 0x%x \n",
                __func__, event_id);
        return -1;
    }
    wmi_handle->event_handler[idx] = NULL;
    wmi_handle->event_id[idx] = 0;
    --wmi_handle->max_event_idx;
    wmi_handle->event_handler[idx] = wmi_handle->event_handler[wmi_handle->max_event_idx];
    wmi_handle->event_id[idx]  = wmi_handle->event_id[wmi_handle->max_event_idx] ;
    return 0;
}

#if 0 /* currently not used */
static int wmi_unified_event_rx(struct wmi_unified *wmi_handle,
				wmi_buf_t evt_buf)
{
	u_int32_t id;
	u_int8_t *event;
	u_int16_t len;
	int status = -1;
	u_int32_t idx = 0;

	ASSERT(evt_buf != NULL);

	id = WMI_GET_FIELD(adf_nbuf_data(evt_buf), WMI_CMD_HDR, COMMANDID);

	if (adf_nbuf_pull_head(evt_buf, sizeof(WMI_CMD_HDR)) == NULL)
		goto end;

	idx = wmi_unified_get_event_handler_ix(wmi_handle, id);
	if (idx == -1) {
		pr_err("%s : event handler is not registered: event id: 0x%x\n",
		       __func__, id);
		goto end;
	}

	event = adf_nbuf_data(evt_buf);
	len = adf_nbuf_len(evt_buf);

	/* Call the WMI registered event handler */
	status = wmi_handle->event_handler[idx](wmi_handle->scn_handle,
						event, len);

end:
	adf_nbuf_free(evt_buf);
	return status;
}
#endif /* 0 */

/*
 * Temporarily added to support older WMI events. We should move all events to unified
 * when the target is ready to support it.
 */
void wmi_control_rx(void *ctx, HTC_PACKET *htc_packet)
{
	struct wmi_unified *wmi_handle = (struct wmi_unified *)ctx;
	wmi_buf_t evt_buf;
	u_int32_t len;
	void *wmi_cmd_struct_ptr = NULL;
	u_int32_t idx = 0;
	int tlv_ok_status = 0;

#if  defined(WMI_INTERFACE_EVENT_LOGGING) || !defined(QCA_CONFIG_SMP)
	u_int32_t id;
	u_int8_t *data;
#endif

	evt_buf = (wmi_buf_t) htc_packet->pPktContext;
	id = WMI_GET_FIELD(adf_nbuf_data(evt_buf), WMI_CMD_HDR, COMMANDID);
	/* TX_PAUSE EVENT should be handled with tasklet context */
	if ((WMI_TX_PAUSE_EVENTID == id) ||
		(WMI_WOW_WAKEUP_HOST_EVENTID == id) ||
		(WMI_D0_WOW_DISABLE_ACK_EVENTID == id)) {
		if (adf_nbuf_pull_head(evt_buf, sizeof(WMI_CMD_HDR)) == NULL)
			return;

		data = adf_nbuf_data(evt_buf);
		len = adf_nbuf_len(evt_buf);
		tlv_ok_status = wmitlv_check_and_pad_event_tlvs(
					wmi_handle->scn_handle,
					data, len, id,
					&wmi_cmd_struct_ptr);
		if (tlv_ok_status != 0) {
			WMA_LOGE("Error: id=0x%x, wmitlv_check_and_pad_tlvs ret=%d",
				id, tlv_ok_status);
			return;
		}

		idx = wmi_unified_get_event_handler_ix(wmi_handle, id);
		if (idx == -1) {
			wmitlv_free_allocated_event_tlvs(id,
				&wmi_cmd_struct_ptr);
			adf_nbuf_free(evt_buf);
			return;
		}
		wmi_handle->event_handler[idx](wmi_handle->scn_handle,
			       wmi_cmd_struct_ptr, len);
		wmitlv_free_allocated_event_tlvs(id, &wmi_cmd_struct_ptr);
		adf_nbuf_free(evt_buf);
		return;
	}

#ifdef WMI_INTERFACE_EVENT_LOGGING
	id = WMI_GET_FIELD(adf_nbuf_data(evt_buf), WMI_CMD_HDR, COMMANDID);
	data = adf_nbuf_data(evt_buf);

	adf_os_spin_lock_bh(&wmi_handle->wmi_record_lock);
	/* Exclude 4 bytes of TLV header */
	WMI_RX_EVENT_RECORD(id, ((u_int8_t *)data + 4));
	adf_os_spin_unlock_bh(&wmi_handle->wmi_record_lock);
#endif
	adf_os_spin_lock_bh(&wmi_handle->eventq_lock);
	adf_nbuf_queue_add(&wmi_handle->event_queue, evt_buf);
	adf_os_spin_unlock_bh(&wmi_handle->eventq_lock);
	schedule_work(&wmi_handle->rx_event_work);
}

void __wmi_control_rx(struct wmi_unified *wmi_handle, wmi_buf_t evt_buf)
{
	u_int32_t id;
	u_int8_t *data;
	u_int32_t len;
	void *wmi_cmd_struct_ptr = NULL;
	int tlv_ok_status = 0;

	id = WMI_GET_FIELD(adf_nbuf_data(evt_buf), WMI_CMD_HDR, COMMANDID);

	if (adf_nbuf_pull_head(evt_buf, sizeof(WMI_CMD_HDR)) == NULL)
		goto end;

	data = adf_nbuf_data(evt_buf);
	len = adf_nbuf_len(evt_buf);

	/* Validate and pad(if necessary) the TLVs */
	tlv_ok_status = wmitlv_check_and_pad_event_tlvs(wmi_handle->scn_handle,
							data, len, id,
							&wmi_cmd_struct_ptr);
	if (tlv_ok_status != 0) {
			pr_err("%s: Error: id=0x%d, wmitlv_check_and_pad_tlvs ret=%d\n",
				__func__, id, tlv_ok_status);
			goto end;
	}

#ifdef FEATURE_WLAN_D0WOW
	if (wmi_get_d0wow_flag(wmi_handle))
		pr_debug("%s: WMI event ID is 0x%x\n", __func__, id);
#endif

	if (id >= WMI_EVT_GRP_START_ID(WMI_GRP_START)) {
		u_int32_t idx = 0;

		idx = wmi_unified_get_event_handler_ix(wmi_handle, id) ;
		if (idx == -1) {
			pr_err("%s : event handler is not registered: event id 0x%x\n",
			       __func__, id);
			goto end;
		}

#ifdef WMI_INTERFACE_EVENT_LOGGING
		adf_os_spin_lock_bh(&wmi_handle->wmi_record_lock);
		/* Exclude 4 bytes of TLV header */
		WMI_EVENT_RECORD(id, ((u_int8_t *)data + 4));
		adf_os_spin_unlock_bh(&wmi_handle->wmi_record_lock);
#endif
		/* Call the WMI registered event handler */
		wmi_handle->event_handler[idx](wmi_handle->scn_handle,
					       wmi_cmd_struct_ptr, len);
		goto end;
	}

	switch (id) {
	default:
		pr_info("%s: Unhandled WMI event %d\n", __func__, id);
		break;
	case WMI_SERVICE_READY_EVENTID:
		pr_info("%s: WMI UNIFIED SERVICE READY event\n", __func__);
		wma_rx_service_ready_event(wmi_handle->scn_handle,
					   wmi_cmd_struct_ptr);
		break;
	case WMI_READY_EVENTID:
		pr_info("%s:  WMI UNIFIED READY event\n", __func__);
		wma_rx_ready_event(wmi_handle->scn_handle, wmi_cmd_struct_ptr);
		break;
	}
end:
	wmitlv_free_allocated_event_tlvs(id, &wmi_cmd_struct_ptr);
	adf_nbuf_free(evt_buf);
}

void __wmi_rx_event_work(struct work_struct *work)
{
	struct wmi_unified *wmi = container_of(work, struct wmi_unified,
					       rx_event_work);
	wmi_buf_t buf;

	adf_os_spin_lock_bh(&wmi->eventq_lock);
	buf = adf_nbuf_queue_remove(&wmi->event_queue);
	adf_os_spin_unlock_bh(&wmi->eventq_lock);
	while (buf) {
		__wmi_control_rx(wmi, buf);
		adf_os_spin_lock_bh(&wmi->eventq_lock);
		buf = adf_nbuf_queue_remove(&wmi->event_queue);
		adf_os_spin_unlock_bh(&wmi->eventq_lock);
	}
}

void wmi_rx_event_work(struct work_struct *work)
{
	vos_ssr_protect(__func__);
	__wmi_rx_event_work(work);
	vos_ssr_unprotect(__func__);
}

/* WMI Initialization functions */

void *
wmi_unified_attach(ol_scn_t scn_handle, wma_wow_tx_complete_cbk func)
{
    struct wmi_unified *wmi_handle;
    wmi_handle = (struct wmi_unified *)OS_MALLOC(NULL, sizeof(struct wmi_unified), GFP_ATOMIC);
    if (wmi_handle == NULL) {
        printk("allocation of wmi handle failed %zu \n", sizeof(struct wmi_unified));
        return NULL;
    }
    OS_MEMZERO(wmi_handle, sizeof(struct wmi_unified));
    wmi_handle->scn_handle = scn_handle;
    adf_os_atomic_init(&wmi_handle->pending_cmds);
    adf_os_atomic_init(&wmi_handle->is_target_suspended);
#ifdef FEATURE_RUNTIME_PM
    adf_os_atomic_init(&wmi_handle->runtime_pm_inprogress);
#endif
    adf_os_spinlock_init(&wmi_handle->eventq_lock);
    adf_nbuf_queue_init(&wmi_handle->event_queue);
    vos_init_work(&wmi_handle->rx_event_work, wmi_rx_event_work);
#ifdef WMI_INTERFACE_EVENT_LOGGING
    adf_os_spinlock_init(&wmi_handle->wmi_record_lock);
#endif
    wmi_handle->wma_wow_tx_complete_cbk = func;
    return wmi_handle;
}

void
wmi_unified_detach(struct wmi_unified* wmi_handle)
{
    wmi_buf_t buf;

    vos_flush_work(&wmi_handle->rx_event_work);
    adf_os_spin_lock_bh(&wmi_handle->eventq_lock);
    buf = adf_nbuf_queue_remove(&wmi_handle->event_queue);
    while (buf) {
	adf_nbuf_free(buf);
	buf = adf_nbuf_queue_remove(&wmi_handle->event_queue);
    }
    adf_os_spin_unlock_bh(&wmi_handle->eventq_lock);
    if (wmi_handle != NULL) {
        OS_FREE(wmi_handle);
        wmi_handle = NULL;
    }
}

/**
 * wmi_unified_remove_work() - detach for WMI work
 * @wmi_handle: handle to WMI
 *
 * A function that does not fully detach WMI, but just remove work
 * queue items associated with it. This is used to make sure that
 * before any other processing code that may destroy related contexts
 * (HTC, etc), work queue processing on WMI has already been stopped.
 *
 * Return: void.
 */
void
wmi_unified_remove_work(struct wmi_unified* wmi_handle)
{
	wmi_buf_t buf;

	VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_INFO,
		"Enter: %s", __func__);
	vos_flush_work(&wmi_handle->rx_event_work);
	adf_os_spin_lock_bh(&wmi_handle->eventq_lock);
	buf = adf_nbuf_queue_remove(&wmi_handle->event_queue);
	while (buf) {
		adf_nbuf_free(buf);
		buf = adf_nbuf_queue_remove(&wmi_handle->event_queue);
	}
	adf_os_spin_unlock_bh(&wmi_handle->eventq_lock);
	VOS_TRACE( VOS_MODULE_ID_WDA, VOS_TRACE_LEVEL_INFO,
		"Done: %s", __func__);
}

void wmi_htc_tx_complete(void *ctx, HTC_PACKET *htc_pkt)
{
	struct wmi_unified *wmi_handle = (struct wmi_unified *)ctx;
	wmi_buf_t wmi_cmd_buf = GET_HTC_PACKET_NET_BUF_CONTEXT(htc_pkt);
#ifdef WMI_INTERFACE_EVENT_LOGGING
	u_int32_t cmd_id;
#endif

	ASSERT(wmi_cmd_buf);
#ifdef WMI_INTERFACE_EVENT_LOGGING
	cmd_id = WMI_GET_FIELD(adf_nbuf_data(wmi_cmd_buf),
		WMI_CMD_HDR, COMMANDID);
	adf_os_spin_lock_bh(&wmi_handle->wmi_record_lock);
	/* Record 16 bytes of WMI cmd tx complete data
	   - exclude TLV and WMI headers */
	WMI_COMMAND_TX_CMP_RECORD(cmd_id,
		((u_int32_t *)adf_nbuf_data(wmi_cmd_buf) + 2));
	adf_os_spin_unlock_bh(&wmi_handle->wmi_record_lock);
#endif
	adf_nbuf_free(wmi_cmd_buf);
	adf_os_mem_free(htc_pkt);
	adf_os_atomic_dec(&wmi_handle->pending_cmds);
}

int
wmi_unified_connect_htc_service(struct wmi_unified * wmi_handle, void *htc_handle)
{

    int status;
    HTC_SERVICE_CONNECT_RESP response;
    HTC_SERVICE_CONNECT_REQ connect;

    OS_MEMZERO(&connect, sizeof(connect));
    OS_MEMZERO(&response, sizeof(response));

    /* meta data is unused for now */
    connect.pMetaData = NULL;
    connect.MetaDataLength = 0;
    /* these fields are the same for all service endpoints */
    connect.EpCallbacks.pContext = wmi_handle;
    connect.EpCallbacks.EpTxCompleteMultiple = NULL /* Control path completion ar6000_tx_complete */;
    connect.EpCallbacks.EpRecv = wmi_control_rx /* Control path rx */;
    connect.EpCallbacks.EpRecvRefill = NULL /* ar6000_rx_refill */;
    connect.EpCallbacks.EpSendFull = NULL /* ar6000_tx_queue_full */;
    connect.EpCallbacks.EpTxComplete = wmi_htc_tx_complete /* ar6000_tx_queue_full */;

    /* connect to control service */
    connect.ServiceID = WMI_CONTROL_SVC;

    if ((status = HTCConnectService(htc_handle, &connect, &response)) != EOK)
    {
        printk(" Failed to connect to WMI CONTROL  service status:%d \n",  status);
        return -1;;
    }
    wmi_handle->wmi_endpoint_id = response.Endpoint;
    wmi_handle->htc_handle = htc_handle;
    wmi_handle->max_msg_len = response.MaxMsgLength;

    return EOK;
}

int wmi_get_host_credits(wmi_unified_t wmi_handle)
{
	int host_credits;

	HTCGetControlEndpointTxHostCredits(wmi_handle->htc_handle,
					   &host_credits);
	return host_credits;
}

int wmi_get_pending_cmds(wmi_unified_t wmi_handle)
{
	return adf_os_atomic_read(&wmi_handle->pending_cmds);
}

void wmi_set_target_suspend(wmi_unified_t wmi_handle, A_BOOL val)
{
	adf_os_atomic_set(&wmi_handle->is_target_suspended, val);
}

/**
 * wmi_set_tgt_assert() - set target assert configuration
 * @wmi_handle: Pointer to WMI handle
 * @val: Target assert config value
 *
 * Return: none
 */
void wmi_set_tgt_assert(wmi_unified_t wmi_handle, bool val)
{
	wmi_handle->tgt_force_assert_enable = val;
}

#ifdef FEATURE_RUNTIME_PM
void wmi_set_runtime_pm_inprogress(wmi_unified_t wmi_handle, A_BOOL val)
{
	adf_os_atomic_set(&wmi_handle->runtime_pm_inprogress, val);
}
#endif

#ifdef FEATURE_WLAN_D0WOW
void wmi_set_d0wow_flag(wmi_unified_t wmi_handle, A_BOOL flag)
{
	tp_wma_handle wma = wmi_handle->scn_handle;
	struct ol_softc *scn =
		vos_get_context(VOS_MODULE_ID_HIF, wma->vos_context);

	if (NULL == scn) {
		WMA_LOGE("%s: Failed to get HIF context", __func__);
		return;
	}
	adf_os_atomic_set(&scn->hif_sc->in_d0wow, flag);
}

A_BOOL wmi_get_d0wow_flag(wmi_unified_t wmi_handle)
{
	tp_wma_handle wma = wmi_handle->scn_handle;
	struct ol_softc *scn =
		vos_get_context(VOS_MODULE_ID_HIF, wma->vos_context);

	if (NULL == scn) {
		WMA_LOGE("%s: Failed to get HIF context", __func__);
		return -EINVAL;
	}

	return adf_os_atomic_read(&scn->hif_sc->in_d0wow);
}
#endif

void wmi_tag_crash_inject(wmi_unified_t wmi_handle, A_BOOL flag)
{
	wmi_handle->tag_crash_inject = flag;
}
