/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_sta_info.h
 *
 * Store and manage station info structure.
 *
 */
#if !defined(__WLAN_HDD_STA_INFO_H)
#define __WLAN_HDD_STA_INFO_H

#include <wlan_hdd_main.h>
#include "qdf_lock.h"
#include "qdf_types.h"
#include "qdf_list.h"
#include "sap_api.h"
#include "cdp_txrx_cmn_struct.h"
#include "sir_mac_prot_def.h"
#include <linux/ieee80211.h>
#include <wlan_mlme_public_struct.h>

/* Opaque handle for abstraction */
#define hdd_sta_info_entry qdf_list_node_t

/**
 * struct dhcp_phase - Per Peer DHCP Phases
 * @DHCP_PHASE_ACK: upon receiving DHCP_ACK/NAK message in REQUEST phase or
 *         DHCP_DELINE message in OFFER phase
 * @DHCP_PHASE_DISCOVER: upon receiving DHCP_DISCOVER message in ACK phase
 * @DHCP_PHASE_OFFER: upon receiving DHCP_OFFER message in DISCOVER phase
 * @DHCP_PHASE_REQUEST: upon receiving DHCP_REQUEST message in OFFER phase or
 *         ACK phase (Renewal process)
 */
enum dhcp_phase {
	DHCP_PHASE_ACK,
	DHCP_PHASE_DISCOVER,
	DHCP_PHASE_OFFER,
	DHCP_PHASE_REQUEST
};

/**
 * struct dhcp_nego_status - Per Peer DHCP Negotiation Status
 * @DHCP_NEGO_STOP: when the peer is in ACK phase or client disassociated
 * @DHCP_NEGO_IN_PROGRESS: when the peer is in DISCOVER or REQUEST
 *         (Renewal process) phase
 */
enum dhcp_nego_status {
	DHCP_NEGO_STOP,
	DHCP_NEGO_IN_PROGRESS
};

/**
 * Pending frame type of EAP_FAILURE, bit number used in "pending_eap_frm_type"
 * of sta_info.
 */
#define PENDING_TYPE_EAP_FAILURE  0

/**
 * enum wlan_sta_info_dbgid - sta info put/get debug id
 * @STA_INFO_ID_RESERVED:   Reserved
 * @STA_INFO_CFG80211_GET_LINK_PROPERTIES: Get link properties
 * @STA_INFO_SOFTAP_INSPECT_TX_EAP_PKT:    Inspect the EAP-Failure
 * @STA_INFO_SOFTAP_CHECK_WAIT_FOR_TX_EAP_PKT: Check and wait for eap failure
 *                                             pkt tx completion
 * @STA_INFO_SOFTAP_INSPECT_DHCP_PACKET:  Inspect DHCP packet
 * @STA_INFO_SOFTAP_HARD_START_XMIT:    Transmit a frame
 * @STA_INFO_SOFTAP_INIT_TX_RX_STA:     Initialize Tx/Rx for a softap station
 * @STA_INFO_SOFTAP_RX_PACKET_CBK:      Receive packet handler
 * @STA_INFO_SOFTAP_REGISTER_STA:       Register a SoftAP STA
 * @STA_INFO_GET_CACHED_STATION_REMOTE: Get cached peer's info
 * @STA_INFO_HDD_GET_STATION_REMOTE:    Get remote peer's info
 * @STA_INFO_WLAN_HDD_GET_STATION_REMOTE: NL80211_CMD_GET_STATION handler for
 *                                        SoftAP
 * @STA_INFO_SOFTAP_DEAUTH_CURRENT_STA: Deauth current sta
 * @STA_INFO_SOFTAP_DEAUTH_ALL_STA:     Deauth all sta in the sta list
 * @STA_INFO_CFG80211_DEL_STATION:      CFG80211 del station handler
 * @STA_INFO_HDD_CLEAR_ALL_STA:         Clear all stations
 * @STA_INFO_FILL_STATION_INFO:         Fill stainfo for connected station
 * @STA_INFO_HOSTAPD_SAP_EVENT_CB:      SAP event handler
 * @STA_INFO_SAP_INDICATE_DISCONNECT_FOR_STA: Indicating disconnect indication
 *                                            to the supplicant
 * @STA_INFO_IS_PEER_ASSOCIATED:       Is peer connected to softap
 * @STA_INFO_SAP_SET_TWO_INTS_GETNONE: Generic "set two integer" ioctl handler
 * @STA_INFO_SAP_GETASSOC_STAMACADDR:  Handler to get assoc station mac address
 * @STA_INFO_SOFTAP_GET_STA_INFO:      Get station info handler
 * @STA_INFO_GET_SOFTAP_LINKSPEED:     Get link speed handler
 * @STA_INFO_CONNECTION_IN_PROGRESS_ITERATOR: Check adapter connection based on
 *                                            device mode
 * @STA_INFO_SOFTAP_STOP_BSS:           Stop BSS
 * @STA_INFO_SOFTAP_CHANGE_STA_STATE:   Change the state of a SoftAP station
 * @STA_INFO_CLEAR_CACHED_STA_INFO:     Clear the cached sta info
 * @STA_INFO_ATTACH_DETACH:             Station info attach/detach
 * @STA_INFO_SHOW:     Station info show
 * @STA_INFO_SOFTAP_IPA_RX_PKT_CALLBACK: Update rx mcbc stats for IPA case
 *
 */
/*
 * New value added to the enum must also be reflected in function
 *  sta_info_string_from_dbgid()
 */
typedef enum {
	STA_INFO_ID_RESERVED   = 0,
	STA_INFO_CFG80211_GET_LINK_PROPERTIES = 1,
	STA_INFO_SOFTAP_INSPECT_TX_EAP_PKT = 2,
	STA_INFO_SOFTAP_CHECK_WAIT_FOR_TX_EAP_PKT = 3,
	STA_INFO_SOFTAP_INSPECT_DHCP_PACKET = 4,
	STA_INFO_SOFTAP_HARD_START_XMIT = 5,
	STA_INFO_SOFTAP_INIT_TX_RX_STA = 6,
	STA_INFO_SOFTAP_RX_PACKET_CBK = 7,
	STA_INFO_SOFTAP_REGISTER_STA = 8,
	STA_INFO_GET_CACHED_STATION_REMOTE = 9,
	STA_INFO_HDD_GET_STATION_REMOTE = 10,
	STA_INFO_WLAN_HDD_GET_STATION_REMOTE = 11,
	STA_INFO_SOFTAP_DEAUTH_CURRENT_STA = 12,
	STA_INFO_SOFTAP_DEAUTH_ALL_STA = 13,
	STA_INFO_CFG80211_DEL_STATION = 14,
	STA_INFO_HDD_CLEAR_ALL_STA = 15,
	STA_INFO_FILL_STATION_INFO = 16,
	STA_INFO_HOSTAPD_SAP_EVENT_CB = 17,
	STA_INFO_SAP_INDICATE_DISCONNECT_FOR_STA = 18,
	STA_INFO_IS_PEER_ASSOCIATED = 19,
	STA_INFO_SAP_SET_TWO_INTS_GETNONE = 20,
	STA_INFO_SAP_GETASSOC_STAMACADDR = 21,
	STA_INFO_SOFTAP_GET_STA_INFO = 22,
	STA_INFO_GET_SOFTAP_LINKSPEED = 23,
	STA_INFO_CONNECTION_IN_PROGRESS_ITERATOR = 24,
	STA_INFO_SOFTAP_STOP_BSS = 25,
	STA_INFO_SOFTAP_CHANGE_STA_STATE = 26,
	STA_INFO_CLEAR_CACHED_STA_INFO = 27,
	STA_INFO_ATTACH_DETACH = 28,
	STA_INFO_SHOW = 29,
	STA_INFO_SOFTAP_IPA_RX_PKT_CALLBACK = 30,

	STA_INFO_ID_MAX,
} wlan_sta_info_dbgid;

/**
 * sta_info_string_from_dbgid() - Convert dbgid to respective string
 * @id -  debug id
 *
 * Debug support function to convert  dbgid to string.
 * Please note to add new string in the array at index equal to
 * its enum value in wlan_sta_info_dbgid.
 */
char *sta_info_string_from_dbgid(wlan_sta_info_dbgid id);

/**
 * struct hdd_station_info - Per station structure kept in HDD for
 *                                     multiple station support for SoftAP
 * @sta_node: The sta_info node for the station info list maintained in adapter
 * @in_use: Is the station entry in use?
 * @sta_id: Station ID reported back from HAL (through SAP).
 *           Broadcast uses station ID zero by default.
 * @sta_type: Type of station i.e. p2p client or infrastructure station
 * @sta_mac: MAC address of the station
 * @peer_state: Current Station state so HDD knows how to deal with packet
 *              queue. Most recent states used to change TLSHIM STA state.
 * @is_qos_enabled: Track QoS status of station
 * @is_deauth_in_progress: The station entry for which Deauth is in progress
 * @nss: Number of spatial streams supported
 * @rate_flags: Rate Flags for this connection
 * @ecsa_capable: Extended CSA capabilities
 * @max_phy_rate: Calcuated maximum phy rate based on mode, nss, mcs etc.
 * @tx_packets: The number of frames from host to firmware
 * @tx_bytes: Bytes send to current station
 * @rx_packets: Packets received from current station
 * @rx_bytes: Bytes received from current station
 * @last_tx_rx_ts: Last tx/rx timestamp with current station
 * @assoc_ts: Current station association timestamp
 * @tx_rate: Tx rate with current station reported from F/W
 * @rx_rate: Rx rate with current station reported from F/W
 * @ampdu: Ampdu enable or not of the station
 * @sgi_enable: Short GI enable or not of the station
 * @guard_interval: Guard interval
 * @tx_stbc: Tx Space-time block coding enable/disable
 * @rx_stbc: Rx Space-time block coding enable/disable
 * @ch_width: Channel Width of the connection
 * @mode: Mode of the connection
 * @max_supp_idx: Max supported rate index of the station
 * @max_ext_idx: Max extended supported rate index of the station
 * @max_mcs_idx: Max supported mcs index of the station
 * @rx_mcs_map: VHT Rx mcs map
 * @tx_mcs_map: VHT Tx mcs map
 * @freq : Frequency of the current station
 * @dot11_mode: 802.11 Mode of the connection
 * @ht_present: HT caps present or not in the current station
 * @vht_present: VHT caps present or not in the current station
 * @ht_caps: HT capabilities of current station
 * @vht_caps: VHT capabilities of current station
 * @reason_code: Disconnection reason code for current station
 * @rssi: RSSI of the current station reported from F/W
 * @capability: Capability information of current station
 * @support_mode: Max supported mode of a station currently
 * connected to sap
 * @rx_retry_cnt: Number of rx retries received from current station
 *                Currently this feature is not supported from FW
 * @rx_mc_bc_cnt: Multicast broadcast packet count received from
 *                current station
 * MSB of rx_mc_bc_cnt indicates whether FW supports rx_mc_bc_cnt
 * feature or not, if first bit is 1 it indicates that FW supports this
 * feature, if it is 0 it indicates FW doesn't support this feature
 * @tx_retry_succeed: the number of frames retried but successfully transmit
 * @tx_retry_exhaust: the number of frames retried but finally failed
 *                    from host to firmware
 * @tx_total_fw: the number of all frames from firmware to remote station
 * @tx_retry_fw: the number of retried frames from firmware to remote station
 * @tx_retry_exhaust_fw: the number of frames retried but finally failed from
 *                    firmware to remote station
 * @rx_fcs_count: the number of frames received with fcs error
 * @assoc_req_ies: Assoc request IEs of the peer station
 * @ref_cnt: Reference count to synchronize sta_info access
 * @ref_cnt_dbgid: Reference count to debug sta_info synchronization issues
 * @pending_eap_frm_type: EAP frame type in tx queue without tx completion
 * @is_attached: Flag to check if the stainfo is attached/detached
 * @peer_rssi_per_chain: Average value of RSSI (dbm) per chain
 */
struct hdd_station_info {
	qdf_list_node_t sta_node;
	bool in_use;
	uint8_t sta_id;
	eStationType sta_type;
	struct qdf_mac_addr sta_mac;
	enum ol_txrx_peer_state peer_state;
	bool is_qos_enabled;
	bool is_deauth_in_progress;
	uint8_t   nss;
	uint32_t  rate_flags;
	uint8_t   ecsa_capable;
	uint32_t max_phy_rate;
	uint32_t tx_packets;
	uint64_t tx_bytes;
	uint32_t rx_packets;
	uint64_t rx_bytes;
	qdf_time_t last_tx_rx_ts;
	qdf_time_t assoc_ts;
	qdf_time_t disassoc_ts;
	uint32_t tx_rate;
	uint32_t rx_rate;
	bool ampdu;
	bool sgi_enable;
	enum txrate_gi guard_interval;
	bool tx_stbc;
	bool rx_stbc;
	tSirMacHTChannelWidth ch_width;
	uint8_t mode;
	uint8_t max_supp_idx;
	uint8_t max_ext_idx;
	uint8_t max_mcs_idx;
	uint8_t rx_mcs_map;
	uint8_t tx_mcs_map;
	uint32_t freq;
	uint8_t dot11_mode;
	bool ht_present;
	bool vht_present;
	struct ieee80211_ht_cap ht_caps;
	struct ieee80211_vht_cap vht_caps;
	uint32_t reason_code;
	int8_t rssi;
	enum dhcp_phase dhcp_phase;
	enum dhcp_nego_status dhcp_nego_status;
	uint16_t capability;
	uint8_t support_mode;
	uint32_t rx_retry_cnt;
	uint32_t rx_mc_bc_cnt;
	uint32_t tx_retry_succeed;
	uint32_t tx_retry_exhaust;
	uint32_t tx_total_fw;
	uint32_t tx_retry_fw;
	uint32_t tx_retry_exhaust_fw;
	uint32_t rx_fcs_count;
	struct wlan_ies assoc_req_ies;
	qdf_atomic_t ref_cnt;
	qdf_atomic_t ref_cnt_dbgid[STA_INFO_ID_MAX];
	unsigned long pending_eap_frm_type;
	bool is_attached;
	int32_t peer_rssi_per_chain[WMI_MAX_CHAINS];
};

/**
 * struct hdd_sta_info_obj - Station info container structure
 * @sta_obj: The sta info object that stores the sta_info
 * @sta_obj_lock: Lock to protect the sta_obj read/write access
 */
struct hdd_sta_info_obj {
	qdf_list_t sta_obj;
	qdf_spinlock_t sta_obj_lock;
};

/**
 * hdd_put_sta_info_ref() - Release sta_info ref for synchronization
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @sta_info: Station info structure to be released.
 * @lock_required: Flag to acquire lock or not
 * @sta_info_dbgid: Debug ID of the caller API
 *
 * Return: None
 */
void hdd_put_sta_info_ref(struct hdd_sta_info_obj *sta_info_container,
			  struct hdd_station_info **sta_info,
			  bool lock_required,
			  wlan_sta_info_dbgid sta_info_dbgid);

/**
 * hdd_take_sta_info_ref() - Increment sta info ref.
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @sta_info: Station info structure to be released.
 * @lock_required: Flag to acquire lock or not
 * @sta_info_dbgid: Debug ID of the caller API
 *
 * This function has to be accompanied by hdd_put_sta_info when the work with
 * the sta info is done. Failure to do so will result in a mem leak.
 *
 * Return: None
 */
void hdd_take_sta_info_ref(struct hdd_sta_info_obj *sta_info_container,
			   struct hdd_station_info *sta_info,
			   bool lock_required,
			   wlan_sta_info_dbgid sta_info_dbgid);

/**
 * hdd_get_front_sta_info_no_lock() - Get the first sta_info from the sta list
 * This API doesnot use any lock in it's implementation. It is the caller's
 * directive to ensure concurrency safety.
 *
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @out_sta_info: The station info structure that acts as the container object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
hdd_get_front_sta_info_no_lock(struct hdd_sta_info_obj *sta_info_container,
			       struct hdd_station_info **out_sta_info);

/**
 * hdd_get_next_sta_info_no_lock() - Get the next sta_info from the sta list
 * This API doesnot use any lock in it's implementation. It is the caller's
 * directive to ensure concurrency safety.
 *
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @out_sta_info: The station info structure that acts as the container object.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
hdd_get_next_sta_info_no_lock(struct hdd_sta_info_obj *sta_info_container,
			      struct hdd_station_info *current_sta_info,
			      struct hdd_station_info **out_sta_info);

/* Abstract wrapper to check sta_info validity */
#define __hdd_is_station_valid(sta_info) sta_info

/**
 * __hdd_take_ref_and_fetch_front_sta_info - Helper macro to lock, fetch front
 * sta_info, take ref and unlock.
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @sta_info: The station info structure that acts as the iterator object.
 */
#define __hdd_take_ref_and_fetch_front_sta_info(sta_info_container, sta_info, \
						sta_info_dbgid) \
	qdf_spin_lock_bh(&sta_info_container.sta_obj_lock), \
	hdd_get_front_sta_info_no_lock(&sta_info_container, &sta_info), \
	(sta_info) ? hdd_take_sta_info_ref(&sta_info_container, \
					   sta_info, false, sta_info_dbgid) : \
					(false), \
	qdf_spin_unlock_bh(&sta_info_container.sta_obj_lock)

/**
 * __hdd_take_ref_and_fetch_next_sta_info - Helper macro to lock, fetch next
 * sta_info, take ref and unlock.
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @sta_info: The station info structure that acts as the iterator object.
 */
#define __hdd_take_ref_and_fetch_next_sta_info(sta_info_container, sta_info, \
					       sta_info_dbgid) \
	qdf_spin_lock_bh(&sta_info_container.sta_obj_lock), \
	hdd_get_next_sta_info_no_lock(&sta_info_container, sta_info, \
				      &sta_info), \
	(sta_info) ? hdd_take_sta_info_ref(&sta_info_container, \
					   sta_info, false, sta_info_dbgid) : \
					(false), \
	qdf_spin_unlock_bh(&sta_info_container.sta_obj_lock)

/**
 * hdd_for_each_sta_ref - Iterate over each station stored in the sta info
 *                        container with ref taken
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @sta_info: The station info structure that acts as the iterator object.
 * @sta_info_dbgid: Debug ID of the caller API
 *
 * The sta_info will contain the structure that is fetched for that particular
 * iteration.
 *
 *			     ***** NOTE *****
 * Before the end of each iteration, dev_put(adapter->dev) must be
 * called. Not calling this will keep hold of a reference, thus preventing
 * unregister of the netdevice.
 *
 * Usage example:
 *	    hdd_for_each_sta_ref(sta_info_container, sta_info, sta_info_dbgid) {
 *		    <work involving station>
 *		    <some more work>
 *		    hdd_put_sta_info_ref(sta_info_container, sta_info, true,
 *					 sta_info_dbgid)
 *	    }
 */
#define hdd_for_each_sta_ref(sta_info_container, sta_info, sta_info_dbgid) \
	for (__hdd_take_ref_and_fetch_front_sta_info(sta_info_container, \
						     sta_info, sta_info_dbgid);\
	     __hdd_is_station_valid(sta_info); \
	     __hdd_take_ref_and_fetch_next_sta_info(sta_info_container, \
						    sta_info, sta_info_dbgid))

/**
 * __hdd_take_ref_and_fetch_front_sta_info_safe - Helper macro to lock, fetch
 * front sta_info, take ref and unlock in a delete safe manner.
 * @sta_info_container: The station info container obj that stores and maintains
 *			the sta_info obj.
 * @sta_info: The station info structure that acts as the iterator object.
 */
#define __hdd_take_ref_and_fetch_front_sta_info_safe(sta_info_container, \
						     sta_info, next_sta_info, \
						     sta_info_dbgid) \
	qdf_spin_lock_bh(&sta_info_container.sta_obj_lock), \
	hdd_get_front_sta_info_no_lock(&sta_info_container, &sta_info), \
	(sta_info) ? hdd_take_sta_info_ref(&sta_info_container, \
					   sta_info, false, sta_info_dbgid) : \
					(false), \
	hdd_get_next_sta_info_no_lock(&sta_info_container, sta_info, \
				      &next_sta_info), \
	(next_sta_info) ? hdd_take_sta_info_ref(&sta_info_container, \
						next_sta_info, false, \
						sta_info_dbgid) : \
						(false), \
	qdf_spin_unlock_bh(&sta_info_container.sta_obj_lock)

/**
 * __hdd_take_ref_and_fetch_next_sta_info_safe - Helper macro to lock, fetch
 * next sta_info, take ref and unlock.
 * @sta_info_container: The station info container obj that stores and maintains
 *			the sta_info obj.
 * @sta_info: The station info structure that acts as the iterator object.
 */
#define __hdd_take_ref_and_fetch_next_sta_info_safe(sta_info_container, \
						    sta_info, next_sta_info, \
						    sta_info_dbgid) \
	qdf_spin_lock_bh(&sta_info_container.sta_obj_lock), \
	sta_info = next_sta_info, \
	hdd_get_next_sta_info_no_lock(&sta_info_container, sta_info, \
				      &next_sta_info), \
	(next_sta_info) ? hdd_take_sta_info_ref(&sta_info_container, \
					     next_sta_info, false, \
					     sta_info_dbgid) : \
					(false), \
	qdf_spin_unlock_bh(&sta_info_container.sta_obj_lock)

/**
 * hdd_for_each_sta_ref_safe - Iterate over each station stored in the sta info
 *                             container in a delete safe manner
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @sta_info: The station info structure that acts as the iterator object.
 * @next_sta_info: A temporary node for maintaing del safe.
 * @sta_info_dbgid: Debug ID of the caller API
 *
 * The sta_info will contain the structure that is fetched for that particular
 * iteration. The next_sta_info is used to store the next station before the
 * current station is deleted so as to provide a safe way to iterate the list
 * while deletion is undergoing.
 *
 *			     ***** NOTE *****
 * Before the end of each iteration, hdd_put_sta_info_ref must be
 * called. Not calling this will keep hold of a reference, thus preventing
 * deletion of the station info
 *
 * Usage example:
 *	hdd_for_each_sta_ref_safe(sta_info_container, sta_info, next_sta_info,
 *				  sta_info_dbgid) {
 *		<work involving station>
 *		<some more work>
 *		hdd_put_sta_info_ref(sta_info_container, sta_info, true,
 *				     sta_info_dbgid)
 *	}
 */
#define hdd_for_each_sta_ref_safe(sta_info_container, sta_info, next_sta_info, \
				  sta_info_dbgid) \
	for (__hdd_take_ref_and_fetch_front_sta_info_safe(sta_info_container, \
							  sta_info, \
							  next_sta_info, \
							  sta_info_dbgid); \
	     __hdd_is_station_valid(sta_info); \
	     __hdd_take_ref_and_fetch_next_sta_info_safe(sta_info_container, \
							 sta_info, \
							 next_sta_info, \
							 sta_info_dbgid))

/**
 * wlan_sta_info_init() - Initialise the wlan hdd station info container obj
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 *
 * Return: QDF_STATUS_SUCCESS on success, failure code otherwise
 */
QDF_STATUS hdd_sta_info_init(struct hdd_sta_info_obj *sta_info_container);

/**
 * wlan_sta_info_deinit() - Deinit the wlan hdd station info container obj
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 *
 * Return: None
 */
void hdd_sta_info_deinit(struct hdd_sta_info_obj *sta_info_container);

/**
 * hdd_sta_info_detach() - Detach the station info structure from the list
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @sta_info: The station info structure that has to be detached from the
 *            container object.
 *
 * Return: None
 */
void hdd_sta_info_detach(struct hdd_sta_info_obj *sta_info_container,
			 struct hdd_station_info **sta_info);

/**
 * hdd_sta_info_attach() - Attach the station info structure into the list
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @sta_info: The station info structure that is to be attached to the
 *            container object.
 *
 * Return: QDF STATUS SUCCESS on successful attach, error code otherwise
 */
QDF_STATUS hdd_sta_info_attach(struct hdd_sta_info_obj *sta_info_container,
			       struct hdd_station_info *sta_info);

/**
 * hdd_get_sta_info_by_mac() - Find the sta_info structure by mac addr
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @mac_addr: The mac addr by which the sta_info has to be fetched.
 * @sta_info_dbgid: Debug ID of the caller API
 *
 * Return: Pointer to the hdd_station_info structure which contains the mac
 *         address passed
 */
struct hdd_station_info *hdd_get_sta_info_by_mac(
				struct hdd_sta_info_obj *sta_info_container,
				const uint8_t *mac_addr,
				wlan_sta_info_dbgid sta_info_dbgid);

/**
 * hdd_clear_cached_sta_info() - Clear the cached sta info from the container
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 *
 * Return: None
 */
void hdd_clear_cached_sta_info(struct hdd_adapter *hdd_adapter);

#endif /* __WLAN_HDD_STA_INFO_H */
