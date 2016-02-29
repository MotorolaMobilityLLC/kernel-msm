/*
 * Linux Debugability support code
 *
 * Copyright (C) 1999-2014, Broadcom Corporation
 *
 *      Unless you and Broadcom execute a separate written software license
 * agreement governing use of this software, this software is licensed to you
 * under the terms of the GNU General Public License version 2 (the "GPL"),
 * available at http://www.broadcom.com/licenses/GPLv2.php, with the
 * following added to such license:
 *
 *      As a special exception, the copyright holders of this software give you
 * permission to link this software with independent modules, and to copy and
 * distribute the resulting executable under terms of your choice, provided that
 * you also meet, for each linked independent module, the terms and conditions of
 * the license of that module.  An independent module is a module which is not
 * derived from this software.  The special exception does not apply to any
 * modifications of the software.
 *
 *      Notwithstanding the above, under no circumstances may you combine this
 * software in any way with any other Broadcom software provided under a license
 * other than the GPL, without Broadcom's express prior written consent.
 *
 * $Id: dhd_debug.h 545157 2015-03-30 23:47:38Z $
 */


#ifndef _dhd_debug_h_
#define _dhd_debug_h_
enum {
	DEBUG_RING_ID_INVALID	= 0,
	FW_VERBOSE_RING_ID,
	FW_EVENT_RING_ID,
	DHD_EVENT_RING_ID,
	/* add new id here */
	DEBUG_RING_ID_MAX
};

enum {
	/* Feature set */
	DBG_MEMORY_DUMP_SUPPORTED = (1 << (0)), /* Memory dump of FW */
	DBG_PER_PACKET_TX_RX_STATUS_SUPPORTED = (1 << (1)), /* PKT Status */
	DBG_CONNECT_EVENT_SUPPORTED = (1 << (2)), /* Connectivity Event */
	DBG_POWER_EVENT_SUPOORTED = (1 << (3)), /* POWER of Driver */
	DBG_WAKE_LOCK_SUPPORTED = (1 << (4)), /* WAKE LOCK of Driver */
	DBG_VERBOSE_LOG_SUPPORTED = (1 << (5)), /* verbose log of FW */
	DBG_HEALTH_CHECK_SUPPORTED = (1 << (6)), /* monitor the health of FW */
	DBG_DRIVER_DUMP_SUPPORTED = (1 << (7)), /* dumps driver state */
	DBG_PACKET_FATE_SUPPORTED = (1 << (8)), /* tracks connection packets' fate */
};

enum {
	/* set for binary entries */
	DBG_RING_ENTRY_FLAGS_HAS_BINARY = (1 << (0)),
	/* set if 64 bits timestamp is present */
	DBG_RING_ENTRY_FLAGS_HAS_TIMESTAMP = (1 << (1))
};

#define DBGRING_NAME_MAX		32
/* firmware verbose ring, ring id 1 */
#define FW_VERBOSE_RING_NAME		"fw_verbose"
#define FW_VERBOSE_RING_SIZE		(64 * 1024)
/* firmware event ring, ring id 2 */
#define FW_EVENT_RING_NAME		"fw_event"
#define FW_EVENT_RING_SIZE		(64 * 1024)
/* DHD connection event ring, ring id 3 */
#define DHD_EVENT_RING_NAME		"dhd_event"
#define DHD_EVENT_RING_SIZE		(64 * 1024)
/* Default interval for all rings */
#define DEFAULT_INTERVAL 5
/* driver receive association command from kernel */
#define WIFI_EVENT_ASSOCIATION_REQUESTED 0
#define WIFI_EVENT_AUTH_COMPLETE 1
#define WIFI_EVENT_ASSOC_COMPLETE 2
/* received firmware event indicating auth frames are sent */
#define WIFI_EVENT_FW_AUTH_STARTED 3
/* received firmware event indicating assoc frames are sent */
#define WIFI_EVENT_FW_ASSOC_STARTED 4
/* received firmware event indicating reassoc frames are sent */
#define WIFI_EVENT_FW_RE_ASSOC_STARTED 5
#define WIFI_EVENT_DRIVER_SCAN_REQUESTED 6
#define WIFI_EVENT_DRIVER_SCAN_RESULT_FOUND 7
#define WIFI_EVENT_DRIVER_SCAN_COMPLETE 8
#define WIFI_EVENT_G_SCAN_STARTED 9
#define WIFI_EVENT_G_SCAN_COMPLETE 10
#define WIFI_EVENT_DISASSOCIATION_REQUESTED 11
#define WIFI_EVENT_RE_ASSOCIATION_REQUESTED 12
#define WIFI_EVENT_ROAM_REQUESTED 13
/* received beacon from AP (event enabled only in verbose mode) */
#define WIFI_EVENT_BEACON_RECEIVED 14
/* firmware has triggered a roam scan (not g-scan) */
#define WIFI_EVENT_ROAM_SCAN_STARTED 15
/* firmware has completed a roam scan (not g-scan) */
#define WIFI_EVENT_ROAM_SCAN_COMPLETE 16
/* firmware has started searching for roam candidates (with reason =xx) */
#define WIFI_EVENT_ROAM_SEARCH_STARTED 17
/* firmware has stopped searching for roam candidates (with reason =xx) */
#define WIFI_EVENT_ROAM_SEARCH_STOPPED 18
/* received channel switch anouncement from AP */
#define WIFI_EVENT_CHANNEL_SWITCH_ANOUNCEMENT 20
/* fw start transmit eapol frame, with EAPOL index 1-4 */
#define WIFI_EVENT_FW_EAPOL_FRAME_TRANSMIT_START 21
/*	fw gives up eapol frame, with rate, success/failure and number retries	*/
#define WIFI_EVENT_FW_EAPOL_FRAME_TRANSMIT_STOP 22
/* kernel queue EAPOL for transmission in driver with EAPOL index 1-4 */
#define WIFI_EVENT_DRIVER_EAPOL_FRAME_TRANSMIT_REQUESTED 23
/* with rate, regardless of the fact that EAPOL frame  is accepted or rejected by firmware */
#define WIFI_EVENT_FW_EAPOL_FRAME_RECEIVED 24
/* with rate, and eapol index, driver has received */
/* EAPOL frame and will queue it up to wpa_supplicant */
#define WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED 26
/* with success/failure, parameters */
#define WIFI_EVENT_BLOCK_ACK_NEGOTIATION_COMPLETE 27
#define WIFI_EVENT_BT_COEX_BT_SCO_START 28
#define WIFI_EVENT_BT_COEX_BT_SCO_STOP 29
/* for paging/scan etc..., when BT starts transmiting twice per BT slot */
#define WIFI_EVENT_BT_COEX_BT_SCAN_START 30
#define WIFI_EVENT_BT_COEX_BT_SCAN_STOP 31
#define WIFI_EVENT_BT_COEX_BT_HID_START 32
#define WIFI_EVENT_BT_COEX_BT_HID_STOP 33
/* firmware sends auth frame in roaming to next candidate */
#define WIFI_EVENT_ROAM_AUTH_STARTED 34
/* firmware receive auth confirm from ap */
#define WIFI_EVENT_ROAM_AUTH_COMPLETE 35
/* firmware sends assoc/reassoc frame in */
#define WIFI_EVENT_ROAM_ASSOC_STARTED 36
/* firmware receive assoc/reassoc confirm from ap */
#define WIFI_EVENT_ROAM_ASSOC_COMPLETE 37
/* firmware sends stop G_SCAN */
#define WIFI_EVENT_G_SCAN_STOP 38
/* firmware indicates G_SCAN scan cycle started */
#define WIFI_EVENT_G_SCAN_CYCLE_STARTED 39
/* firmware indicates G_SCAN scan cycle completed */
#define WIFI_EVENT_G_SCAN_CYCLE_COMPLETED 40
/* firmware indicates G_SCAN scan start for a particular bucket */
#define WIFI_EVENT_G_SCAN_BUCKET_STARTED 41
/* firmware indicates G_SCAN scan completed for particular bucket */
#define WIFI_EVENT_G_SCAN_BUCKET_COMPLETED  42
/* Event received from firmware about G_SCAN scan results being available */
#define WIFI_EVENT_G_SCAN_RESULTS_AVAILABLE 43
/* Event received from firmware with G_SCAN capabilities */
#define WIFI_EVENT_G_SCAN_CAPABILITIES 44
/* Event received from firmware when eligible candidate is found */
#define WIFI_EVENT_ROAM_CANDIDATE_FOUND 45
/* Event received from firmware when roam scan configuration gets enabled or disabled */
#define WIFI_EVENT_ROAM_SCAN_CONFIG 46
/* firmware/driver timed out authentication */
#define WIFI_EVENT_AUTH_TIMEOUT 47
/* firmware/driver timed out association */
#define WIFI_EVENT_ASSOC_TIMEOUT 48
/* firmware/driver encountered allocation failure */
#define WIFI_EVENT_MEM_ALLOC_FAILURE 49
/* driver added a PNO network in firmware */
#define WIFI_EVENT_DRIVER_PNO_ADD 50
/* driver removed a PNO network in firmware */
#define WIFI_EVENT_DRIVER_PNO_REMOVE 51
/* driver received PNO networks found indication from firmware */
#define WIFI_EVENT_DRIVER_PNO_NETWORK_FOUND 52
/* driver triggered a scan for PNO networks */
#define WIFI_EVENT_DRIVER_PNO_SCAN_REQUESTED 53
/* driver received scan results of PNO networks */
#define WIFI_EVENT_DRIVER_PNO_SCAN_RESULT_FOUND 54
/* driver updated scan results from PNO candidates to cfg80211 */
#define WIFI_EVENT_DRIVER_PNO_SCAN_COMPLETE 55

#define WIFI_TAG_VENDOR_SPECIFIC 0 /* take a byte stream as parameter */
#define WIFI_TAG_BSSID 1 /* takes a 6 bytes MAC address as parameter */
#define WIFI_TAG_ADDR 2 /* takes a 6 bytes MAC address as parameter */
#define WIFI_TAG_SSID 3 /* takes a 32 bytes SSID address as parameter */
#define WIFI_TAG_STATUS 4 /* takes an integer as parameter */
#define WIFI_TAG_CHANNEL_SPEC 5 /* takes one or more wifi_channel_spec as parameter */
#define WIFI_TAG_WAKE_LOCK_EVENT 6 /* takes a wake_lock_event struct as parameter */
#define WIFI_TAG_ADDR1 7 /* takes a 6 bytes MAC address as parameter */
#define WIFI_TAG_ADDR2 8 /* takes a 6 bytes MAC address as parameter */
#define WIFI_TAG_ADDR3 9 /* takes a 6 bytes MAC address as parameter */
#define WIFI_TAG_ADDR4 10 /* takes a 6 bytes MAC address as parameter */
#define WIFI_TAG_TSF 11 /* take a 64 bits TSF value as parameter */
#define WIFI_TAG_IE 12  /* take one or more specific 802.11 IEs parameter, */
						/* IEs are in turn indicated */
						/* in TLV format as per 802.11 spec */
#define WIFI_TAG_INTERFACE 13 /* take interface name as parameter */
#define WIFI_TAG_REASON_CODE 14 /* take a reason code as per 802.11 as parameter */
#define WIFI_TAG_RATE_MBPS 15 /* take a wifi rate in 0.5 mbps */
#define WIFI_TAG_REQUEST_ID 16 /* take an integer as parameter */
#define WIFI_TAG_BUCKET_ID 17 /* take an integer as parameter */
#define WIFI_TAG_GSCAN_PARAMS 18 /* takes a wifi_scan_cmd_params struct as parameter */
#define WIFI_TAG_GSCAN_CAPABILITIES 19 /* takes a wifi_gscan_capabilities struct as parameter */
#define WIFI_TAG_SCAN_ID 20 /* take an integer as parameter */
#define WIFI_TAG_RSSI 21 /* takes s16 as parameter */
#define WIFI_TAG_CHANNEL 22 /* takes u16 as parameter */
#define WIFI_TAG_LINK_ID 23 /* take an integer as parameter */
#define WIFI_TAG_LINK_ROLE 24 /* take an integer as parameter */
#define WIFI_TAG_LINK_STATE 25 /* take an integer as parameter */
#define WIFI_TAG_LINK_TYPE 26 /* take an integer as parameter */
#define WIFI_TAG_TSCO 27 /* take an integer as parameter */
#define WIFI_TAG_RSCO 28 /* take an integer as parameter */
#define WIFI_TAG_EAPOL_MESSAGE_TYPE 29 /* take an integer as parameter */
									   /* M1-1, M2-2, M3-3, M4-4 */

#define TLV_LOG_SIZE(tlv) ((tlv) ? (sizeof(tlv_log) + (tlv)->len) : 0)

#define TLV_LOG_NEXT(tlv) \
	((tlv) ? ((tlv_log *)((uint8 *)tlv + TLV_LOG_SIZE(tlv))) : 0)

#define DBG_RING_STATUS_SIZE (sizeof(dhd_dbg_ring_status_t))

#define VALID_RING(id) \
	((id) > DEBUG_RING_ID_INVALID && id < DEBUG_RING_ID_MAX)

#define DBG_RING_ENTRY_SIZE (sizeof(dhd_dbg_ring_entry_t))

#define ENTRY_LENGTH(hdr) ((hdr)->len + DBG_RING_ENTRY_SIZE)

#define DBG_RING_ACTIVE(dhdp, ring_id) \
	((dhdp)->dbg->dbg_rings[(ring_id)].state == RING_ACTIVE)

#define DBG_EVENT_LOG(dhdp, connect_state) \
{ \
	do { \
		uint16 state = (connect_state); \
		if (DBG_RING_ACTIVE((dhdp), DHD_EVENT_RING_ID)) \
			dhd_os_push_push_ring_data((dhdp), DHD_EVENT_RING_ID, \
				&state, sizeof(state)); \
	} while (0); \
}

#define PKT_MON_ATTACHED(state) \
	(((state) != PKT_MON_INVALID) && ((state) != PKT_MON_DETACHED))
#define PKT_MON_DETACHED(state) \
	(((state) == PKT_MON_INVALID) || ((state) == PKT_MON_DETACHED))
#define PKT_MON_STARTED(state) ((state) == PKT_MON_STARTED)
#define PKT_MON_STOPPED(state) ((state) == PKT_MON_STOPPED)
#define PKT_MON_PKT_FULL(pkt_count) ((pkt_count) >= MAX_FATE_LOG_LEN)
#define PKT_MON_STATUS_FULL(pkt_count, status_count) \
	(((status_count) >= (pkt_count)) || ((status_count) >= MAX_FATE_LOG_LEN))

#ifdef D11_STATUS
#define DHD_DBG_PKT_MON_TX(dhdp, pkt, pktid) \
	do { \
		if ((dhdp) && (dhdp)->dbg && (dhdp)->dbg->pkt_mon.tx_pkt_mon && (pkt)) { \
			(dhdp)->dbg->pkt_mon.tx_pkt_mon((dhdp), (pkt), (pktid)); \
		} \
	} while (0);
#define DHD_DBG_PKT_MON_TX_STATUS(dhdp, pkt, pktid, status) \
	do { \
		if ((dhdp) && (dhdp)->dbg && (dhdp)->dbg->pkt_mon.tx_status_mon && (pkt)) { \
			(dhdp)->dbg->pkt_mon.tx_status_mon((dhdp), (pkt), (pktid), (status)); \
		} \
	} while (0);
#define DHD_DBG_PKT_MON_RX(dhdp, pkt) \
	do { \
		if ((dhdp) && (dhdp)->dbg && (dhdp)->dbg->pkt_mon.rx_pkt_mon && (pkt)) { \
			if (ntoh16((pkt)->protocol) != ETHER_TYPE_BRCM) { \
				(dhdp)->dbg->pkt_mon.rx_pkt_mon((dhdp), (pkt)); \
			} \
		} \
	} while (0);
#define DHD_DBG_PKT_MON_START(dhdp) \
	dhd_os_dbg_start_pkt_monitor((dhdp));
#define DHD_DBG_PKT_MON_STOP(dhdp) \
	dhd_os_dbg_stop_pkt_monitor((dhdp));
#else
#define DHD_DBG_PKT_MON_TX(dhdp, pkt, pktid)
#define DHD_DBG_PKT_MON_TX_STATUS(dhdp, pkt, pktid, status)
#define DHD_DBG_PKT_MON_RX(dhdp, pkt)
#define DHD_DBG_PKT_MON_START(dhdp)
#define DHD_DBG_PKT_MON_STOP(dhdp)
#endif /* D11_STATUS */

typedef struct {
    uint16 tag;
    uint16 len; /* length of value */
    uint8 value[0];
} __attribute__ ((packed)) tlv_log;

typedef struct per_packet_status_entry {
    uint8 flags;
    uint8 tid; /* transmit or received tid */
    uint16 MCS; /* modulation and bandwidth */
	/*
	* TX: RSSI of ACK for that packet
	* RX: RSSI of packet
	*/
    uint8 rssi;
    uint8 num_retries; /* number of attempted retries */
    uint16 last_transmit_rate; /* last transmit rate in .5 mbps */
	 /* transmit/reeive sequence for that MPDU packet */
    uint16 link_layer_transmit_sequence;
	/*
	* TX: firmware timestamp (us) when packet is queued within firmware buffer
	* for SDIO/HSIC or into PCIe buffer
	* RX : firmware receive timestamp
	*/
    uint64 firmware_entry_timestamp;
	/*
	* firmware timestamp (us) when packet start contending for the
	* medium for the first time, at head of its AC queue,
	* or as part of an MPDU or A-MPDU. This timestamp is not updated
	* for each retry, only the first transmit attempt.
	*/
    uint64 start_contention_timestamp;
	/*
	* fimrware timestamp (us) when packet is successfully transmitted
	* or aborted because it has exhausted its maximum number of retries
	*/
	uint64 transmit_success_timestamp;
	/*
	* packet data. The length of packet data is determined by the entry_size field of
	* the wifi_ring_buffer_entry structure. It is expected that first bytes of the
	* packet, or packet headers only (up to TCP or RTP/UDP headers) will be copied into the ring
	*/
    uint8 data[0];
} __attribute__ ((packed)) per_packet_status_entry_t;

typedef struct log_conn_event {
    uint16 event;
    tlv_log tlvs[0];
	/*
	* separate parameter structure per event to be provided and optional data
	* the event_data is expected to include an official android part, with some
	* parameter as transmit rate, num retries, num scan result found etc...
	* as well, event_data can include a vendor proprietary part which is
	* understood by the developer only.
	*/
} __attribute__ ((packed)) log_conn_event_t;

/*
 * Ring buffer name for power events ring. note that power event are extremely frequents
 * and thus should be stored in their own ring/file so as not to clobber connectivity events
 */

typedef struct wake_lock_event {
    uint32 status; /* 0 taken, 1 released */
    uint32 reason; /* reason why this wake lock is taken */
    char name[0]; /* null terminated */
} __attribute__ ((packed)) wake_lock_event_t;

typedef struct wifi_power_event {
    uint16 event;
    tlv_log tlvs[0];
}  __attribute__ ((packed)) wifi_power_event_t;

/* entry type */
enum {
	DBG_RING_ENTRY_EVENT_TYPE = 1,
	DBG_RING_ENTRY_PKT_TYPE,
	DBG_RING_ENTRY_WAKE_LOCK_EVENT_TYPE,
	DBG_RING_ENTRY_POWER_EVENT_TYPE,
	DBG_RING_ENTRY_DATA_TYPE
};

/*
 * Packet logging - HAL specific data
 * XXX: These should be moved to wl_cfgvendor.h
 */

#define MD5_PREFIX_LEN				4
#define MAX_FATE_LOG_LEN			32
#define MAX_FRAME_LEN_ETHERNET		1518
#define MAX_FRAME_LEN_80211_MGMT	2352 /* 802.11-2012 Fig. 8-34 */

typedef enum {
	/* Sent over air and ACKed. */
	TX_PKT_FATE_ACKED,

	/* Sent over air but not ACKed. (Normal for broadcast/multicast.) */
	TX_PKT_FATE_SENT,

	/* Queued within firmware, but not yet sent over air. */
	TX_PKT_FATE_FW_QUEUED,

	/*
	 * Dropped by firmware as invalid. E.g. bad source address,
	 * bad checksum, or invalid for current state.
	 */
	TX_PKT_FATE_FW_DROP_INVALID,

	/* Dropped by firmware due to lack of buffer space. */
	TX_PKT_FATE_FW_DROP_NOBUFS,

	/*
	 * Dropped by firmware for any other reason. Includes
	 * frames that were sent by driver to firmware, but
	 * unaccounted for by firmware.
	 */
	TX_PKT_FATE_FW_DROP_OTHER,

	/* Queued within driver, not yet sent to firmware. */
	TX_PKT_FATE_DRV_QUEUED,

	/*
	 * Dropped by driver as invalid. E.g. bad source address,
	 * or invalid for current state.
	 */
	TX_PKT_FATE_DRV_DROP_INVALID,

	/* Dropped by driver due to lack of buffer space. */
	TX_PKT_FATE_DRV_DROP_NOBUFS,

	/*  Dropped by driver for any other reason. */
	TX_PKT_FATE_DRV_DROP_OTHER,
} wifi_tx_packet_fate;

typedef enum {
	/* Valid and delivered to network stack (e.g., netif_rx()). */
	RX_PKT_FATE_SUCCESS,

	/* Queued within firmware, but not yet sent to driver. */
	RX_PKT_FATE_FW_QUEUED,

	/* Dropped by firmware due to host-programmable filters. */
	RX_PKT_FATE_FW_DROP_FILTER,

	/*
	 * Dropped by firmware as invalid. E.g. bad checksum,
	 * decrypt failed, or invalid for current state.
	 */
	RX_PKT_FATE_FW_DROP_INVALID,

	/* Dropped by firmware due to lack of buffer space. */
	RX_PKT_FATE_FW_DROP_NOBUFS,

	/* Dropped by firmware for any other reason. */
	RX_PKT_FATE_FW_DROP_OTHER,

	/* Queued within driver, not yet delivered to network stack. */
	RX_PKT_FATE_DRV_QUEUED,

	/* Dropped by driver due to filter rules. */
	RX_PKT_FATE_DRV_DROP_FILTER,

	/* Dropped by driver as invalid. E.g. not permitted in current state. */
	RX_PKT_FATE_DRV_DROP_INVALID,

	/* Dropped by driver due to lack of buffer space. */
	RX_PKT_FATE_DRV_DROP_NOBUFS,

	/* Dropped by driver for any other reason. */
	RX_PKT_FATE_DRV_DROP_OTHER,
} wifi_rx_packet_fate;

typedef enum {
	FRAME_TYPE_UNKNOWN,
	FRAME_TYPE_ETHERNET_II,
	FRAME_TYPE_80211_MGMT,
} frame_type;

typedef struct wifi_frame_info {
	/*
	 * The type of MAC-layer frame that this frame_info holds.
	 * - For data frames, use FRAME_TYPE_ETHERNET_II.
	 * - For management frames, use FRAME_TYPE_80211_MGMT.
	 * - If the type of the frame is unknown, use FRAME_TYPE_UNKNOWN.
	 */
	frame_type payload_type;

	/*
	 * The number of bytes included in |frame_content|. If the frame
	 * contents are missing (e.g. RX frame dropped in firmware),
	 * |frame_len| should be set to 0.
	 */
	size_t frame_len;

	/*
	 * Host clock when this frame was received by the driver (either
	 *	outbound from the host network stack, or inbound from the
	 *	firmware).
	 *	- The timestamp should be taken from a clock which includes time
	 *	  the host spent suspended (e.g. ktime_get_boottime()).
	 *	- If no host timestamp is available (e.g. RX frame was dropped in
	 *	  firmware), this field should be set to 0.
	 */
	uint32 driver_timestamp_usec;

	/*
	 * Firmware clock when this frame was received by the firmware
	 *	(either outbound from the host, or inbound from a remote
	 *	station).
	 *	- The timestamp should be taken from a clock which includes time
	 *	  firmware spent suspended (if applicable).
	 *	- If no firmware timestamp is available (e.g. TX frame was
	 *	  dropped by driver), this field should be set to 0.
	 *	- Consumers of |frame_info| should _not_ assume any
	 *	  synchronization between driver and firmware clocks.
	 */
	uint32 firmware_timestamp_usec;

	/*
	 * Actual frame content.
	 * - Should be provided for TX frames originated by the host.
	 * - Should be provided for RX frames received by the driver.
	 * - Optionally provided for TX frames originated by firmware. (At
	 *   discretion of HAL implementation.)
	 * - Optionally provided for RX frames dropped in firmware. (At
	 *   discretion of HAL implementation.)
	 * - If frame content is not provided, |frame_len| should be set
	 *   to 0.
	 */
	union {
		char ethernet_ii[MAX_FRAME_LEN_ETHERNET];
		char ieee_80211_mgmt[MAX_FRAME_LEN_80211_MGMT];
	} frame_content;
} wifi_frame_info_t;

typedef struct wifi_tx_report {
	/*
	 * Prefix of MD5 hash of |frame_inf.frame_content|. If frame
	 * content is not provided, prefix of MD5 hash over the same data
	 * that would be in frame_content, if frame content were provided.
	 */
	char md5_prefix[MD5_PREFIX_LEN];
	wifi_tx_packet_fate fate;
	wifi_frame_info_t frame_inf;
} wifi_tx_report_t;

typedef struct wifi_rx_report {
	/*
	 * Prefix of MD5 hash of |frame_inf.frame_content|. If frame
	 * content is not provided, prefix of MD5 hash over the same data
	 * that would be in frame_content, if frame content were provided.
	 */
	char md5_prefix[MD5_PREFIX_LEN];
	wifi_rx_packet_fate fate;
	wifi_frame_info_t frame_inf;
} wifi_rx_report_t;

/*
 * Packet logging - internal data
 */

typedef enum dhd_dbg_pkt_mon_state {
	PKT_MON_INVALID = 0,
	PKT_MON_ATTACHED,
	PKT_MON_STARTED,
	PKT_MON_STOPPED,
	PKT_MON_DETACHED,
} dhd_dbg_pkt_mon_state_t;

typedef struct dhd_dbg_pkt_info
{
	frame_type payload_type;
	size_t pkt_len;
	uint32 driver_ts;
	uint32 firmware_ts;
	uint32 pkt_hash;
	void *pkt;
} dhd_dbg_pkt_info_t;

typedef struct dhd_dbg_tx_info
{
	wifi_tx_packet_fate fate;
	dhd_dbg_pkt_info_t info;
} dhd_dbg_tx_info_t;

typedef struct dhd_dbg_rx_info
{
	wifi_rx_packet_fate fate;
	dhd_dbg_pkt_info_t info;
} dhd_dbg_rx_info_t;

typedef struct dhd_dbg_tx_report
{
	dhd_dbg_tx_info_t *tx_pkts;
	uint16 pkt_pos;
	uint16 status_pos;
} dhd_dbg_tx_report_t;

typedef struct dhd_dbg_rx_report
{
	dhd_dbg_rx_info_t *rx_pkts;
	uint16 pkt_pos;
} dhd_dbg_rx_report_t;

typedef void (*dbg_pullreq_t)(void *os_priv, const int ring_id);
typedef void (*dbg_urgent_noti_t) (dhd_pub_t *dhdp, const void *data, const uint32 len);
typedef int (*dbg_mon_tx_pkts_t) (dhd_pub_t *dhdp, void *pkt, uint32 pktid);
typedef int (*dbg_mon_tx_status_t) (dhd_pub_t *dhdp, void *pkt,
	uint32 pktid, uint16 status);
typedef int (*dbg_mon_rx_pkts_t) (dhd_pub_t *dhdp, void *pkt);

typedef struct dhd_dbg_pkt_mon
{
	dhd_dbg_tx_report_t *tx_report;
	dhd_dbg_rx_report_t *rx_report;
	dhd_dbg_pkt_mon_state_t tx_pkt_state;
	dhd_dbg_pkt_mon_state_t tx_status_state;
	dhd_dbg_pkt_mon_state_t rx_pkt_state;

	/* call backs */
	dbg_mon_tx_pkts_t tx_pkt_mon;
	dbg_mon_tx_status_t tx_status_mon;
	dbg_mon_rx_pkts_t rx_pkt_mon;
} dhd_dbg_pkt_mon_t;

typedef struct dhd_dbg_ring_entry {
	uint16 len; /* payload length excluding the header */
	uint8 flags;
	uint8 type; /* Per ring specific */
	uint64 timestamp; /* present if has_timestamp bit is set. */
}  __attribute__ ((packed)) dhd_dbg_ring_entry_t;

typedef struct dhd_dbg_ring_status {
	uint8 name[DBGRING_NAME_MAX];
	uint32 flags;
	int ring_id; /* unique integer representing the ring */
	/* total memory size allocated for the buffer */
	uint32 ring_buffer_byte_size;
	uint32 verbose_level;
	/* number of bytes that was written to the buffer by driver */
	uint32 written_bytes;
	/* number of bytes that was read from the buffer by user land */
	uint32 read_bytes;
	/* number of records that was read from the buffer by user land */
	uint32 written_records;
} dhd_dbg_ring_status_t;

struct ring_statistics {
	/* number of bytes that was written to the buffer by driver */
	uint32 written_bytes;
	/* number of bytes that was read from the buffer by user land */
	uint32 read_bytes;
	/* number of records that was written to the buffer by driver */
	uint32 written_records;
};

enum dbg_ring_state {
	RING_STOP	= 0,	/* ring is not initialized */
	RING_ACTIVE,	/* ring is live and logging */
	RING_SUSPEND	/* ring is initialized but not logging */
};

typedef struct dhd_dbg_ring {
	int	id;		/* ring id */
	uint8	name[DBGRING_NAME_MAX];	/* name string */
	uint32	ring_size;	/* numbers of item in ring */
	uint32	wp;		/* write pointer */
	uint32	rp;		/* read pointer */
	uint32  log_level; /* log_level */
	uint32	threshold; /* threshold bytes */
	void *	ring_buf;	/* pointer of actually ring buffer */
	void *	lock;		/* spin lock for ring access */
	struct ring_statistics stat; /* statistics */
	enum dbg_ring_state state;	/* ring state enum */
	uint32  wp_pad; /* padding start position */
	bool no_space; /* writer does not have enough space */
	uint32 rem_len; /* number of bytes from wp_pad to end */
	bool sched_pull; /* schedule reader immediately */

} dhd_dbg_ring_t;

typedef struct dhd_dbg {
	dhd_dbg_ring_t dbg_rings[DEBUG_RING_ID_MAX];
	void *private;		/* os private_data */
	dhd_dbg_pkt_mon_t pkt_mon;
	dbg_pullreq_t pullreq;
	dbg_urgent_noti_t urgent_notifier;
} dhd_dbg_t;

struct log_level_table {
	int log_level;
	uint16 tag;
	char *desc;
};


/* dhd_dbg functions */
extern int dhd_dbg_attach(dhd_pub_t *dhdp, dbg_pullreq_t os_pullreq,
	dbg_urgent_noti_t os_urgent_notifier, void *os_priv);
extern void dhd_dbg_detach(dhd_pub_t *dhdp);
extern int dhd_dbg_start(dhd_pub_t *dhdp, bool start);
extern void dhd_dbg_trace_evnt_handler(dhd_pub_t *dhdp, void *event_data,
		void *raw_event_ptr, uint datalen);
extern int dhd_dbg_set_configuration(dhd_pub_t *dhdp, int ring_id,
		int log_level, int flags, int threshold);
extern int dhd_dbg_get_ring_status(dhd_pub_t *dhdp, int ring_id,
		dhd_dbg_ring_status_t *dbg_ring_status);

extern int dhd_dbg_ring_push(dhd_pub_t *dhdp, int ring_id, dhd_dbg_ring_entry_t *hdr, void *data);

extern int dhd_dbg_ring_pull(dhd_pub_t *dhdp, int ring_id, void *data, uint32 buf_len);
extern int dhd_dbg_find_ring_id(dhd_pub_t *dhdp, char *ring_name);
extern void *dhd_dbg_get_priv(dhd_pub_t *dhdp);
extern int dhd_dbg_send_urgent_evt(dhd_pub_t *dhdp, const void *data, const uint32 len);

#ifdef D11_STATUS
extern int dhd_dbg_attach_pkt_monitor(dhd_pub_t *dhdp,
	dbg_mon_tx_pkts_t tx_pkt_mon,
	dbg_mon_tx_status_t tx_status_mon,
	dbg_mon_rx_pkts_t rx_pkt_mon);
extern int dhd_dbg_start_pkt_monitor(dhd_pub_t *dhdp);
extern int dhd_dbg_monitor_tx_pkts(dhd_pub_t *dhdp, void *pkt, uint32 pktid);
extern int dhd_dbg_monitor_tx_status(dhd_pub_t *dhdp, void *pkt,
	uint32 pktid, uint16 status);
extern int dhd_dbg_monitor_rx_pkts(dhd_pub_t *dhdp, void *pkt);
extern int dhd_dbg_stop_pkt_monitor(dhd_pub_t *dhdp);
extern int dhd_dbg_monitor_get_tx_pkts(dhd_pub_t *dhdp, void __user *user_buf,
	uint16 req_count, uint16 *resp_count);
extern int dhd_dbg_monitor_get_rx_pkts(dhd_pub_t *dhdp, void __user *user_buf,
	uint16 req_count, uint16 *resp_count);
extern int dhd_dbg_detach_pkt_monitor(dhd_pub_t *dhdp);
#endif /* D11_STATUS */

/* wrapper function */
extern int dhd_os_dbg_attach(dhd_pub_t *dhdp);
extern void dhd_os_dbg_detach(dhd_pub_t *dhdp);
extern int dhd_os_dbg_register_callback(int ring_id,
	void (*dbg_ring_sub_cb)(void *ctx, const int ring_id, const void *data,
		const uint32 len, const dhd_dbg_ring_status_t dbg_ring_status));
extern int dhd_os_dbg_register_urgent_notifier(dhd_pub_t *dhdp,
	void (*urgent_noti)(void *ctx, const void *data, const uint32 len, const uint32 fw_len));

extern int dhd_os_start_logging(dhd_pub_t *dhdp, char *ring_name, int log_level,
		int flags, int time_intval, int threshold);
extern int dhd_os_reset_logging(dhd_pub_t *dhdp);
extern int dhd_os_suppress_logging(dhd_pub_t *dhdp, bool suppress);

extern int dhd_os_get_ring_status(dhd_pub_t *dhdp, int ring_id,
		dhd_dbg_ring_status_t *dbg_ring_status);
extern int dhd_os_trigger_get_ring_data(dhd_pub_t *dhdp, char *ring_name);
extern int dhd_os_push_push_ring_data(dhd_pub_t *dhdp, int ring_id, void *data, int32 data_len);

extern int dhd_os_dbg_get_feature(dhd_pub_t *dhdp, int32 *features);

#ifdef D11_STATUS
extern int dhd_os_dbg_attach_pkt_monitor(dhd_pub_t *dhdp);
extern int dhd_os_dbg_start_pkt_monitor(dhd_pub_t *dhdp);
extern int dhd_os_dbg_monitor_tx_pkts(dhd_pub_t *dhdp, void *pkt,
	uint32 pktid);
extern int dhd_os_dbg_monitor_tx_status(dhd_pub_t *dhdp, void *pkt,
	uint32 pktid, uint16 status);
extern int dhd_os_dbg_monitor_rx_pkts(dhd_pub_t *dhdp, void *pkt);
extern int dhd_os_dbg_stop_pkt_monitor(dhd_pub_t *dhdp);
extern int dhd_os_dbg_monitor_get_tx_pkts(dhd_pub_t *dhdp,
	void __user *user_buf, uint16 req_count, uint16 *resp_count);
extern int dhd_os_dbg_monitor_get_rx_pkts(dhd_pub_t *dhdp,
	void __user *user_buf, uint16 req_count, uint16 *resp_count);
extern int dhd_os_dbg_detach_pkt_monitor(dhd_pub_t *dhdp);
#endif /* D11_STATUS */
#endif /* _dhd_debug_h_ */
