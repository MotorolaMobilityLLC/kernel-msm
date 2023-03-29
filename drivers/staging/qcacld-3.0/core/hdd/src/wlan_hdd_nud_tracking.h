/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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
 * DOC: contains nud event tracking function declarations
 */

#ifndef _WLAN_NUD_TRACKING_H_
#define _WLAN_NUD_TRACKING_H_

#ifdef WLAN_NUD_TRACKING

/**
 * struct hdd_nud_tx_rx_stats - Capture tx and rx count during NUD tracking
 * @pre_tx_packets: Number of tx packets at NUD_PROBE event
 * @pre_tx_acked: Number of tx acked at NUD_PROBE event
 * @pre_rx_packets: Number of rx packets at NUD_PROBE event
 * @post_tx_packets: Number of tx packets at NUD_FAILED event
 * @post_tx_acked: Number of tx acked at NUD_FAILED event
 * @post_rx_packets: Number of rx packets at NUD_FAILED event
 * @gw_rx_packets: Number of rx packets from the registered gateway
 *                 during the period from NUD_PROBE to NUD_FAILED
 */
struct hdd_nud_tx_rx_stats {
	uint32_t pre_tx_packets;
	uint32_t pre_tx_acked;
	uint32_t pre_rx_packets;
	uint32_t post_tx_packets;
	uint32_t post_tx_acked;
	uint32_t post_rx_packets;
	qdf_atomic_t gw_rx_packets;
};

 /**
  * struct hdd_nud_tracking_info - structure to keep track for NUD information
  * @curr_state: current state of NUD machine
  * @ignore_nud_tracking: true if nud tracking is not required else false
  * @tx_rx_stats: Number of packets during NUD tracking
  * @gw_mac_addr: gateway mac address for which NUD events are tracked
  * @nud_event_work: work to be scheduled during NUD_FAILED
  * @is_gw_rx_pkt_track_enabled: true if rx pkt capturing is enabled for GW,
  *                              else false
  * @is_gw_updated: true if GW is updated for NUD Tracking
  */
struct hdd_nud_tracking_info {
	uint8_t curr_state;
	bool ignore_nud_tracking;
	struct hdd_nud_tx_rx_stats tx_rx_stats;
	struct qdf_mac_addr gw_mac_addr;
	qdf_work_t nud_event_work;
	bool is_gw_rx_pkt_track_enabled;
	bool is_gw_updated;
};

/**
 * hdd_nud_set_gateway_addr() - set gateway mac address
 * @adapter: Pointer to adapter
 * @gw_mac_addr: mac address to be set
 *
 * Return: none
 */
void hdd_nud_set_gateway_addr(struct hdd_adapter *adapter,
			      struct qdf_mac_addr gw_mac_addr);

/**
 * hdd_nud_incr_gw_rx_pkt_cnt() - Increment rx count for gateway
 * @adapter: Pointer to adapter
 * @mac_addr: Gateway mac address
 *
 * Return: None
 */
void hdd_nud_incr_gw_rx_pkt_cnt(struct hdd_adapter *adapter,
				struct qdf_mac_addr *mac_addr);

/**
 * hdd_nud_init_tracking() - initialize NUD tracking
 * @hdd_adapter: Pointer to hdd adapter
 *
 * Return: None
 */
void hdd_nud_init_tracking(struct hdd_adapter *adapter);

/**
 * hdd_nud_reset_tracking() - reset NUD tracking
 * @hdd_adapter: Pointer to hdd adapter
 *
 * Return: None
 */
void hdd_nud_reset_tracking(struct hdd_adapter *adapter);

/**
 * hdd_nud_deinit_tracking() - deinitialize NUD tracking
 * @hdd_adapter: Pointer to hdd adapter
 *
 * Return: None
 */
void hdd_nud_deinit_tracking(struct hdd_adapter *adapter);

/**
 * hdd_nud_ignore_tracking() - set/reset nud trackig status
 * @data: Pointer to hdd_adapter
 * @ignoring: Ignore status to set
 *
 * Return: None
 */
void hdd_nud_ignore_tracking(struct hdd_adapter *adapter,
			     bool ignoring);

/**
 * hdd_nud_register_netevent_notifier - Register netevent notifiers.
 * @hdd_ctx: HDD context
 *
 * Register netevent notifiers.
 *
 * Return: 0 on success and errno on failure
 */
int hdd_nud_register_netevent_notifier(struct hdd_context *hdd_ctx);

/**
 * hdd_nud_unregister_netevent_notifier - Unregister netevent notifiers.
 * @hdd_ctx: HDD context
 *
 * Unregister netevent notifiers.
 *
 * Return: None
 */
void hdd_nud_unregister_netevent_notifier(struct hdd_context *hdd_ctx);

/**
 * hdd_nud_flush_work() - flush pending nud work
 * @adapter: Pointer to hdd adapter
 *
 * Return: None
 */
void hdd_nud_flush_work(struct hdd_adapter *adapter);

/**
 * hdd_nud_indicate_roam() - reset NUD when roaming happens
 * @adapter: Pointer to hdd adapter
 *
 * Return: None
 */
void hdd_nud_indicate_roam(struct hdd_adapter *adapter);

#else
static inline void hdd_nud_set_gateway_addr(struct hdd_adapter *adapter,
					    struct qdf_mac_addr gw_mac_addr)
{
}

static inline void hdd_nud_incr_gw_rx_pkt_cnt(struct hdd_adapter *adapter,
					      struct qdf_mac_addr *mac_addr)
{
}

static inline void hdd_nud_init_tracking(struct hdd_adapter *adapter)
{
}

static inline void hdd_nud_reset_tracking(struct hdd_adapter *adapter)
{
}

static inline void hdd_nud_deinit_tracking(struct hdd_adapter *adapter)
{
}

static inline void hdd_nud_ignore_tracking(struct hdd_adapter *adapter,
					   bool status)
{
}

static inline int
hdd_nud_register_netevent_notifier(struct hdd_context *hdd_ctx)
{
	return 0;
}

static inline void
hdd_nud_unregister_netevent_notifier(struct hdd_context *hdd_ctx)
{
}

static inline void
hdd_nud_flush_work(struct hdd_adapter *adapter)
{
}

static inline void
hdd_nud_indicate_roam(struct hdd_adapter *adapter)
{
}
#endif /* WLAN_NUD_TRACKING */
#endif /* end  of _WLAN_NUD_TRACKING_H_ */
