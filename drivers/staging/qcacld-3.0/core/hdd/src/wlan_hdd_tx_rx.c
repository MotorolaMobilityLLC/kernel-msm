/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_tx_rx.c
 *
 * Linux HDD Tx/RX APIs
 */

/* denote that this file does not allow legacy hddLog */
#define HDD_DISALLOW_LEGACY_HDDLOG 1
#include "osif_sync.h"
#include <wlan_hdd_tx_rx.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_napi.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <linux/inetdevice.h>
#include <cds_sched.h>
#include <cds_utils.h>

#include <wlan_hdd_p2p.h>
#include <linux/wireless.h>
#include <net/cfg80211.h>
#include "sap_api.h"
#include "wlan_hdd_wmm.h"
#include "wlan_hdd_tdls.h"
#include "wlan_hdd_ocb.h"
#include "wlan_hdd_lro.h"
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_peer_ops.h>
#include <cdp_txrx_flow_ctrl_v2.h>
#include "wlan_hdd_nan_datapath.h"
#include "pld_common.h"
#include <cdp_txrx_misc.h>
#include "wlan_hdd_rx_monitor.h"
#include "wlan_hdd_power.h"
#include "wlan_hdd_cfg80211.h"
#include <wlan_hdd_tsf.h>
#include <net/tcp.h>
#include "wma_api.h"

#include "wlan_hdd_nud_tracking.h"
#include "dp_txrx.h"
#if defined(WLAN_SUPPORT_RX_FISA)
#include "dp_fisa_rx.h"
#else
#include <net/ieee80211_radiotap.h>
#endif
#include <ol_defines.h>
#include "cfg_ucfg_api.h"
#include "target_type.h"
#include "wlan_hdd_object_manager.h"
#include "nan_public_structs.h"
#include "nan_ucfg_api.h"
#include <wlan_hdd_sar_limits.h>
#include "wlan_hdd_object_manager.h"

#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
/*
 * Mapping Linux AC interpretation to SME AC.
 * Host has 5 tx queues, 4 flow-controlled queues for regular traffic and
 * one non-flow-controlled queue for high priority control traffic(EOPOL, DHCP).
 * The fifth queue is mapped to AC_VO to allow for proper prioritization.
 */
const uint8_t hdd_qdisc_ac_to_tl_ac[] = {
	SME_AC_VO,
	SME_AC_VI,
	SME_AC_BE,
	SME_AC_BK,
	SME_AC_VO,
};

#else
const uint8_t hdd_qdisc_ac_to_tl_ac[] = {
	SME_AC_VO,
	SME_AC_VI,
	SME_AC_BE,
	SME_AC_BK,
};

#endif

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
void hdd_register_hl_netdev_fc_timer(struct hdd_adapter *adapter,
				     qdf_mc_timer_callback_t timer_callback)
{
	if (!adapter->tx_flow_timer_initialized) {
		qdf_mc_timer_init(&adapter->tx_flow_control_timer,
				  QDF_TIMER_TYPE_SW, timer_callback, adapter);
		adapter->tx_flow_timer_initialized = true;
	}
}

/**
 * hdd_deregister_hl_netdev_fc_timer() - Deregister HL Flow Control Timer
 * @adapter: adapter handle
 *
 * Return: none
 */
void hdd_deregister_hl_netdev_fc_timer(struct hdd_adapter *adapter)
{
	if (adapter->tx_flow_timer_initialized) {
		qdf_mc_timer_stop(&adapter->tx_flow_control_timer);
		qdf_mc_timer_destroy(&adapter->tx_flow_control_timer);
		adapter->tx_flow_timer_initialized = false;
	}
}

/**
 * hdd_tx_resume_timer_expired_handler() - TX Q resume timer handler
 * @adapter_context: pointer to vdev adapter
 *
 * Return: None
 */
void hdd_tx_resume_timer_expired_handler(void *adapter_context)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *)adapter_context;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	u32 p_qpaused;
	u32 np_qpaused;

	if (!adapter) {
		hdd_err("invalid adapter context");
		return;
	}

	cdp_display_stats(soc, CDP_DUMP_TX_FLOW_POOL_INFO,
			  QDF_STATS_VERBOSITY_LEVEL_LOW);
	wlan_hdd_display_adapter_netif_queue_history(adapter);
	hdd_debug("Enabling queues");
	spin_lock_bh(&adapter->pause_map_lock);
	p_qpaused = adapter->pause_map & BIT(WLAN_DATA_FLOW_CONTROL_PRIORITY);
	np_qpaused = adapter->pause_map & BIT(WLAN_DATA_FLOW_CONTROL);
	spin_unlock_bh(&adapter->pause_map_lock);

	if (p_qpaused) {
		wlan_hdd_netif_queue_control(adapter,
					     WLAN_NETIF_PRIORITY_QUEUE_ON,
					     WLAN_DATA_FLOW_CONTROL_PRIORITY);
		cdp_hl_fc_set_os_queue_status(soc,
					      adapter->vdev_id,
					      WLAN_NETIF_PRIORITY_QUEUE_ON);
	}
	if (np_qpaused) {
		wlan_hdd_netif_queue_control(adapter,
					     WLAN_WAKE_NON_PRIORITY_QUEUE,
					     WLAN_DATA_FLOW_CONTROL);
		cdp_hl_fc_set_os_queue_status(soc,
					      adapter->vdev_id,
					      WLAN_WAKE_NON_PRIORITY_QUEUE);
	}
}

#endif /* QCA_HL_NETDEV_FLOW_CONTROL */

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
/**
 * hdd_tx_resume_timer_expired_handler() - TX Q resume timer handler
 * @adapter_context: pointer to vdev adapter
 *
 * If Blocked OS Q is not resumed during timeout period, to prevent
 * permanent stall, resume OS Q forcefully.
 *
 * Return: None
 */
void hdd_tx_resume_timer_expired_handler(void *adapter_context)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) adapter_context;

	if (!adapter) {
		/* INVALID ARG */
		return;
	}

	hdd_debug("Enabling queues");
	wlan_hdd_netif_queue_control(adapter, WLAN_WAKE_ALL_NETIF_QUEUE,
				     WLAN_CONTROL_PATH);
}

/**
 * hdd_tx_resume_false() - Resume OS TX Q false leads to queue disabling
 * @adapter: pointer to hdd adapter
 * @tx_resume: TX Q resume trigger
 *
 *
 * Return: None
 */
static void
hdd_tx_resume_false(struct hdd_adapter *adapter, bool tx_resume)
{
	if (true == tx_resume)
		return;

	/* Pause TX  */
	hdd_debug("Disabling queues");
	wlan_hdd_netif_queue_control(adapter, WLAN_STOP_ALL_NETIF_QUEUE,
				     WLAN_DATA_FLOW_CONTROL);

	if (QDF_TIMER_STATE_STOPPED ==
			qdf_mc_timer_get_current_state(&adapter->
						       tx_flow_control_timer)) {
		QDF_STATUS status;

		status = qdf_mc_timer_start(&adapter->tx_flow_control_timer,
				WLAN_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);

		if (!QDF_IS_STATUS_SUCCESS(status))
			hdd_err("Failed to start tx_flow_control_timer");
		else
			adapter->hdd_stats.tx_rx_stats.txflow_timer_cnt++;
	}

	adapter->hdd_stats.tx_rx_stats.txflow_pause_cnt++;
	adapter->hdd_stats.tx_rx_stats.is_txflow_paused = true;
}

static inline struct sk_buff *hdd_skb_orphan(struct hdd_adapter *adapter,
		struct sk_buff *skb)
{
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	int need_orphan = 0;

	if (adapter->tx_flow_low_watermark > 0) {
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 19, 0))
		/*
		 * The TCP TX throttling logic is changed a little after
		 * 3.19-rc1 kernel, the TCP sending limit will be smaller,
		 * which will throttle the TCP packets to the host driver.
		 * The TCP UP LINK throughput will drop heavily. In order to
		 * fix this issue, need to orphan the socket buffer asap, which
		 * will call skb's destructor to notify the TCP stack that the
		 * SKB buffer is unowned. And then the TCP stack will pump more
		 * packets to host driver.
		 *
		 * The TX packets might be dropped for UDP case in the iperf
		 * testing. So need to be protected by follow control.
		 */
		need_orphan = 1;
#else
		if (hdd_ctx->config->tx_orphan_enable)
			need_orphan = 1;
#endif
	} else if (hdd_ctx->config->tx_orphan_enable) {
		if (qdf_nbuf_is_ipv4_tcp_pkt(skb) ||
		    qdf_nbuf_is_ipv6_tcp_pkt(skb))
			need_orphan = 1;
	}

	if (need_orphan) {
		skb_orphan(skb);
		++adapter->hdd_stats.tx_rx_stats.tx_orphaned;
	} else
		skb = skb_unshare(skb, GFP_ATOMIC);

	return skb;
}

/**
 * hdd_tx_resume_cb() - Resume OS TX Q.
 * @adapter_context: pointer to vdev apdapter
 * @tx_resume: TX Q resume trigger
 *
 * Q was stopped due to WLAN TX path low resource condition
 *
 * Return: None
 */
void hdd_tx_resume_cb(void *adapter_context, bool tx_resume)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) adapter_context;
	struct hdd_station_ctx *hdd_sta_ctx = NULL;

	if (!adapter) {
		/* INVALID ARG */
		return;
	}

	hdd_sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);

	/* Resume TX  */
	if (true == tx_resume) {
		if (QDF_TIMER_STATE_STOPPED !=
		    qdf_mc_timer_get_current_state(&adapter->
						   tx_flow_control_timer)) {
			qdf_mc_timer_stop(&adapter->tx_flow_control_timer);
		}
		hdd_debug("Enabling queues");
		wlan_hdd_netif_queue_control(adapter,
					     WLAN_WAKE_ALL_NETIF_QUEUE,
					     WLAN_DATA_FLOW_CONTROL);
		adapter->hdd_stats.tx_rx_stats.is_txflow_paused = false;
		adapter->hdd_stats.tx_rx_stats.txflow_unpause_cnt++;
	}
	hdd_tx_resume_false(adapter, tx_resume);
}

bool hdd_tx_flow_control_is_pause(void *adapter_context)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) adapter_context;

	if ((!adapter) || (WLAN_HDD_ADAPTER_MAGIC != adapter->magic)) {
		/* INVALID ARG */
		hdd_err("invalid adapter %pK", adapter);
		return false;
	}

	return adapter->pause_map & (1 << WLAN_DATA_FLOW_CONTROL);
}

void hdd_register_tx_flow_control(struct hdd_adapter *adapter,
		qdf_mc_timer_callback_t timer_callback,
		ol_txrx_tx_flow_control_fp flow_control_fp,
		ol_txrx_tx_flow_control_is_pause_fp flow_control_is_pause_fp)
{
	if (adapter->tx_flow_timer_initialized == false) {
		qdf_mc_timer_init(&adapter->tx_flow_control_timer,
			  QDF_TIMER_TYPE_SW,
			  timer_callback,
			  adapter);
		adapter->tx_flow_timer_initialized = true;
	}
	cdp_fc_register(cds_get_context(QDF_MODULE_ID_SOC),
		adapter->vdev_id, flow_control_fp, adapter,
		flow_control_is_pause_fp);
}

/**
 * hdd_deregister_tx_flow_control() - Deregister TX Flow control
 * @adapter: adapter handle
 *
 * Return: none
 */
void hdd_deregister_tx_flow_control(struct hdd_adapter *adapter)
{
	cdp_fc_deregister(cds_get_context(QDF_MODULE_ID_SOC),
			adapter->vdev_id);
	if (adapter->tx_flow_timer_initialized == true) {
		qdf_mc_timer_stop(&adapter->tx_flow_control_timer);
		qdf_mc_timer_destroy(&adapter->tx_flow_control_timer);
		adapter->tx_flow_timer_initialized = false;
	}
}

void hdd_get_tx_resource(struct hdd_adapter *adapter,
			 struct qdf_mac_addr *mac_addr, uint16_t timer_value)
{
	if (false ==
	    cdp_fc_get_tx_resource(cds_get_context(QDF_MODULE_ID_SOC),
				   OL_TXRX_PDEV_ID,
				   *mac_addr,
				   adapter->tx_flow_low_watermark,
				   adapter->tx_flow_hi_watermark_offset)) {
		hdd_debug("Disabling queues lwm %d hwm offset %d",
			 adapter->tx_flow_low_watermark,
			 adapter->tx_flow_hi_watermark_offset);
		wlan_hdd_netif_queue_control(adapter, WLAN_STOP_ALL_NETIF_QUEUE,
					     WLAN_DATA_FLOW_CONTROL);
		if ((adapter->tx_flow_timer_initialized == true) &&
		    (QDF_TIMER_STATE_STOPPED ==
		    qdf_mc_timer_get_current_state(&adapter->
						    tx_flow_control_timer))) {
			qdf_mc_timer_start(&adapter->tx_flow_control_timer,
					   timer_value);
			adapter->hdd_stats.tx_rx_stats.txflow_timer_cnt++;
			adapter->hdd_stats.tx_rx_stats.txflow_pause_cnt++;
			adapter->hdd_stats.tx_rx_stats.is_txflow_paused = true;
		}
	}
}

#else
/**
 * hdd_skb_orphan() - skb_unshare a cloned packed else skb_orphan
 * @adapter: pointer to HDD adapter
 * @skb: pointer to skb data packet
 *
 * Return: pointer to skb structure
 */
static inline struct sk_buff *hdd_skb_orphan(struct hdd_adapter *adapter,
		struct sk_buff *skb) {

	struct sk_buff *nskb;
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 19, 0))
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
#endif

	hdd_skb_fill_gso_size(adapter->dev, skb);

	nskb = skb_unshare(skb, GFP_ATOMIC);
#if (LINUX_VERSION_CODE > KERNEL_VERSION(3, 19, 0))
	if (unlikely(hdd_ctx->config->tx_orphan_enable) && (nskb == skb)) {
		/*
		 * For UDP packets we want to orphan the packet to allow the app
		 * to send more packets. The flow would ultimately be controlled
		 * by the limited number of tx descriptors for the vdev.
		 */
		++adapter->hdd_stats.tx_rx_stats.tx_orphaned;
		skb_orphan(skb);
	}
#endif
	return nskb;
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

uint32_t hdd_txrx_get_tx_ack_count(struct hdd_adapter *adapter)
{
	return cdp_get_tx_ack_stats(cds_get_context(QDF_MODULE_ID_SOC),
				    adapter->vdev_id);
}

#ifdef FEATURE_WLAN_DIAG_SUPPORT
/**
 * qdf_event_eapol_log() - send event to wlan diag
 * @skb: skb ptr
 * @dir: direction
 * @eapol_key_info: eapol key info
 *
 * Return: None
 */
void hdd_event_eapol_log(struct sk_buff *skb, enum qdf_proto_dir dir)
{
	int16_t eapol_key_info;

	WLAN_HOST_DIAG_EVENT_DEF(wlan_diag_event, struct host_event_wlan_eapol);

	if ((dir == QDF_TX &&
		(QDF_NBUF_CB_PACKET_TYPE_EAPOL !=
		 QDF_NBUF_CB_GET_PACKET_TYPE(skb))))
		return;
	else if (!qdf_nbuf_is_ipv4_eapol_pkt(skb))
		return;

	eapol_key_info = (uint16_t)(*(uint16_t *)
				(skb->data + EAPOL_KEY_INFO_OFFSET));

	wlan_diag_event.event_sub_type =
		(dir == QDF_TX ?
		 WIFI_EVENT_DRIVER_EAPOL_FRAME_TRANSMIT_REQUESTED :
		 WIFI_EVENT_DRIVER_EAPOL_FRAME_RECEIVED);
	wlan_diag_event.eapol_packet_type = (uint8_t)(*(uint8_t *)
				(skb->data + EAPOL_PACKET_TYPE_OFFSET));
	wlan_diag_event.eapol_key_info = eapol_key_info;
	wlan_diag_event.eapol_rate = 0;
	qdf_mem_copy(wlan_diag_event.dest_addr,
			(skb->data + QDF_NBUF_DEST_MAC_OFFSET),
			sizeof(wlan_diag_event.dest_addr));
	qdf_mem_copy(wlan_diag_event.src_addr,
			(skb->data + QDF_NBUF_SRC_MAC_OFFSET),
			sizeof(wlan_diag_event.src_addr));

	WLAN_HOST_DIAG_EVENT_REPORT(&wlan_diag_event, EVENT_WLAN_EAPOL);
}
#endif /* FEATURE_WLAN_DIAG_SUPPORT */

int hdd_set_udp_qos_upgrade_config(struct hdd_adapter *adapter,
				   uint8_t priority)
{
	if (adapter->device_mode != QDF_STA_MODE) {
		hdd_info_rl("Data priority upgrade only allowed in STA mode:%d",
			    adapter->device_mode);
		return -EINVAL;
	}

	if (priority >= QCA_WLAN_AC_ALL) {
		hdd_err_rl("Invlid data priority: %d", priority);
		return -EINVAL;
	}

	adapter->upgrade_udp_qos_threshold = priority;

	hdd_debug("UDP packets qos upgrade to: %d", priority);

	return 0;
}

/**
 * wlan_hdd_classify_pkt() - classify packet
 * @skb - sk buff
 *
 * Return: none
 */
void wlan_hdd_classify_pkt(struct sk_buff *skb)
{
	struct ethhdr *eh = (struct ethhdr *)skb->data;

	qdf_mem_zero(skb->cb, sizeof(skb->cb));

	/* check destination mac address is broadcast/multicast */
	if (is_broadcast_ether_addr((uint8_t *)eh))
		QDF_NBUF_CB_GET_IS_BCAST(skb) = true;
	else if (is_multicast_ether_addr((uint8_t *)eh))
		QDF_NBUF_CB_GET_IS_MCAST(skb) = true;

	if (qdf_nbuf_is_ipv4_arp_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_ARP;
	else if (qdf_nbuf_is_ipv4_dhcp_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_DHCP;
	else if (qdf_nbuf_is_ipv4_eapol_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_EAPOL;
	else if (qdf_nbuf_is_ipv4_wapi_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_WAPI;
	else if (qdf_nbuf_is_icmp_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_ICMP;
	else if (qdf_nbuf_is_icmpv6_pkt(skb))
		QDF_NBUF_CB_GET_PACKET_TYPE(skb) =
			QDF_NBUF_CB_PACKET_TYPE_ICMPv6;
}

/**
 * hdd_clear_tx_rx_connectivity_stats() - clear connectivity stats
 * @hdd_ctx: pointer to HDD Station Context
 *
 * Return: None
 */
static void hdd_clear_tx_rx_connectivity_stats(struct hdd_adapter *adapter)
{
	hdd_debug("Clear txrx connectivity stats");
	qdf_mem_zero(&adapter->hdd_stats.hdd_arp_stats,
		     sizeof(adapter->hdd_stats.hdd_arp_stats));
	qdf_mem_zero(&adapter->hdd_stats.hdd_dns_stats,
		     sizeof(adapter->hdd_stats.hdd_dns_stats));
	qdf_mem_zero(&adapter->hdd_stats.hdd_tcp_stats,
		     sizeof(adapter->hdd_stats.hdd_tcp_stats));
	qdf_mem_zero(&adapter->hdd_stats.hdd_icmpv4_stats,
		     sizeof(adapter->hdd_stats.hdd_icmpv4_stats));
	adapter->pkt_type_bitmap = 0;
	adapter->track_arp_ip = 0;
	qdf_mem_zero(adapter->dns_payload, adapter->track_dns_domain_len);
	adapter->track_dns_domain_len = 0;
	adapter->track_src_port = 0;
	adapter->track_dest_port = 0;
	adapter->track_dest_ipv4 = 0;
}

void hdd_reset_all_adapters_connectivity_stats(struct hdd_context *hdd_ctx)
{
	struct hdd_adapter *adapter = NULL, *next = NULL;
	QDF_STATUS status;

	hdd_enter();

	status = hdd_get_front_adapter(hdd_ctx, &adapter);

	while (adapter && QDF_STATUS_SUCCESS == status) {
		hdd_clear_tx_rx_connectivity_stats(adapter);
		status = hdd_get_next_adapter(hdd_ctx, adapter, &next);
		adapter = next;
	}

	hdd_exit();
}

/**
 * hdd_is_tx_allowed() - check if Tx is allowed based on current peer state
 * @skb: pointer to OS packet (sk_buff)
 * @vdev_id: virtual interface id
 * @peer_mac: Peer mac address
 *
 * This function gets the peer state from DP and check if it is either
 * in OL_TXRX_PEER_STATE_CONN or OL_TXRX_PEER_STATE_AUTH. Only EAP packets
 * are allowed when peer_state is OL_TXRX_PEER_STATE_CONN. All packets
 * allowed when peer_state is OL_TXRX_PEER_STATE_AUTH.
 *
 * Return: true if Tx is allowed and false otherwise.
 */
static inline bool hdd_is_tx_allowed(struct sk_buff *skb, uint8_t vdev_id,
				     uint8_t *peer_mac)
{
	enum ol_txrx_peer_state peer_state;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	QDF_BUG(soc);

	peer_state = cdp_peer_state_get(soc, vdev_id, peer_mac);
	if (likely(OL_TXRX_PEER_STATE_AUTH == peer_state))
		return true;
	if (OL_TXRX_PEER_STATE_CONN == peer_state &&
		(ntohs(skb->protocol) == HDD_ETHERTYPE_802_1_X
		|| IS_HDD_ETHERTYPE_WAI(skb)))
		return true;
	QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_INFO_HIGH,
		  FL("Invalid peer state for Tx: %d"), peer_state);
	return false;
}

/**
 * hdd_tx_rx_is_dns_domain_name_match() - function to check whether dns
 * domain name in the received skb matches with the tracking dns domain
 * name or not
 *
 * @skb: pointer to skb
 * @adapter: pointer to adapter
 *
 * Returns: true if matches else false
 */
static bool hdd_tx_rx_is_dns_domain_name_match(struct sk_buff *skb,
					       struct hdd_adapter *adapter)
{
	uint8_t *domain_name;

	if (adapter->track_dns_domain_len == 0)
		return false;

	/* check OOB , is strncmp accessing data more than skb->len */
	if ((adapter->track_dns_domain_len +
	    QDF_NBUF_PKT_DNS_NAME_OVER_UDP_OFFSET) > qdf_nbuf_len(skb))
		return false;

	domain_name = qdf_nbuf_get_dns_domain_name(skb,
						adapter->track_dns_domain_len);
	if (strncmp(domain_name, adapter->dns_payload,
		    adapter->track_dns_domain_len) == 0)
		return true;
	else
		return false;
}

void hdd_tx_rx_collect_connectivity_stats_info(struct sk_buff *skb,
			void *context,
			enum connectivity_stats_pkt_status action,
			uint8_t *pkt_type)
{
	uint32_t pkt_type_bitmap;
	struct hdd_adapter *adapter = NULL;

	adapter = (struct hdd_adapter *)context;
	if (unlikely(adapter->magic != WLAN_HDD_ADAPTER_MAGIC)) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			  "Magic cookie(%x) for adapter sanity verification is invalid",
			  adapter->magic);
		return;
	}

	/* ARP tracking is done already. */
	pkt_type_bitmap = adapter->pkt_type_bitmap;
	pkt_type_bitmap &= ~CONNECTIVITY_CHECK_SET_ARP;

	if (!pkt_type_bitmap)
		return;

	switch (action) {
	case PKT_TYPE_REQ:
	case PKT_TYPE_TX_HOST_FW_SENT:
		if (qdf_nbuf_is_icmp_pkt(skb)) {
			if (qdf_nbuf_data_is_icmpv4_req(skb) &&
			    (adapter->track_dest_ipv4 ==
					qdf_nbuf_get_icmpv4_tgt_ip(skb))) {
				*pkt_type = CONNECTIVITY_CHECK_SET_ICMPV4;
				if (action == PKT_TYPE_REQ) {
					++adapter->hdd_stats.hdd_icmpv4_stats.
							tx_icmpv4_req_count;
					QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
						  QDF_TRACE_LEVEL_INFO_HIGH,
						  "%s : ICMPv4 Req packet",
						  __func__);
				} else
					/* host receives tx completion */
					++adapter->hdd_stats.hdd_icmpv4_stats.
								tx_host_fw_sent;
			}
		} else if (qdf_nbuf_is_ipv4_tcp_pkt(skb)) {
			if (qdf_nbuf_data_is_tcp_syn(skb) &&
			    (adapter->track_dest_port ==
					qdf_nbuf_data_get_tcp_dst_port(skb))) {
				*pkt_type = CONNECTIVITY_CHECK_SET_TCP_SYN;
				if (action == PKT_TYPE_REQ) {
					++adapter->hdd_stats.hdd_tcp_stats.
							tx_tcp_syn_count;
					QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
						  QDF_TRACE_LEVEL_INFO_HIGH,
						  "%s : TCP Syn packet",
						  __func__);
				} else
					/* host receives tx completion */
					++adapter->hdd_stats.hdd_tcp_stats.
							tx_tcp_syn_host_fw_sent;
			} else if ((adapter->hdd_stats.hdd_tcp_stats.
				    is_tcp_syn_ack_rcv || adapter->hdd_stats.
					hdd_tcp_stats.is_tcp_ack_sent) &&
				   qdf_nbuf_data_is_tcp_ack(skb) &&
				   (adapter->track_dest_port ==
				    qdf_nbuf_data_get_tcp_dst_port(skb))) {
				*pkt_type = CONNECTIVITY_CHECK_SET_TCP_ACK;
				if (action == PKT_TYPE_REQ &&
					adapter->hdd_stats.hdd_tcp_stats.
							is_tcp_syn_ack_rcv) {
					++adapter->hdd_stats.hdd_tcp_stats.
							tx_tcp_ack_count;
					adapter->hdd_stats.hdd_tcp_stats.
						is_tcp_syn_ack_rcv = false;
					adapter->hdd_stats.hdd_tcp_stats.
						is_tcp_ack_sent = true;
					QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
						  QDF_TRACE_LEVEL_INFO_HIGH,
						  "%s : TCP Ack packet",
						  __func__);
				} else if (action == PKT_TYPE_TX_HOST_FW_SENT &&
					adapter->hdd_stats.hdd_tcp_stats.
							is_tcp_ack_sent) {
					/* host receives tx completion */
					++adapter->hdd_stats.hdd_tcp_stats.
							tx_tcp_ack_host_fw_sent;
					adapter->hdd_stats.hdd_tcp_stats.
							is_tcp_ack_sent = false;
				}
			}
		} else if (qdf_nbuf_is_ipv4_udp_pkt(skb)) {
			if (qdf_nbuf_data_is_dns_query(skb) &&
			    hdd_tx_rx_is_dns_domain_name_match(skb, adapter)) {
				*pkt_type = CONNECTIVITY_CHECK_SET_DNS;
				if (action == PKT_TYPE_REQ) {
					++adapter->hdd_stats.hdd_dns_stats.
							tx_dns_req_count;
					QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
						  QDF_TRACE_LEVEL_INFO_HIGH,
						  "%s : DNS query packet",
						  __func__);
				} else
					/* host receives tx completion */
					++adapter->hdd_stats.hdd_dns_stats.
								tx_host_fw_sent;
			}
		}
		break;

	case PKT_TYPE_RSP:
		if (qdf_nbuf_is_icmp_pkt(skb)) {
			if (qdf_nbuf_data_is_icmpv4_rsp(skb) &&
			    (adapter->track_dest_ipv4 ==
					qdf_nbuf_get_icmpv4_src_ip(skb))) {
				++adapter->hdd_stats.hdd_icmpv4_stats.
							rx_icmpv4_rsp_count;
				*pkt_type =
				CONNECTIVITY_CHECK_SET_ICMPV4;
				QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
					  QDF_TRACE_LEVEL_INFO_HIGH,
					  "%s : ICMPv4 Res packet", __func__);
			}
		} else if (qdf_nbuf_is_ipv4_tcp_pkt(skb)) {
			if (qdf_nbuf_data_is_tcp_syn_ack(skb) &&
			    (adapter->track_dest_port ==
					qdf_nbuf_data_get_tcp_src_port(skb))) {
				++adapter->hdd_stats.hdd_tcp_stats.
							rx_tcp_syn_ack_count;
				adapter->hdd_stats.hdd_tcp_stats.
					is_tcp_syn_ack_rcv = true;
				*pkt_type =
				CONNECTIVITY_CHECK_SET_TCP_SYN_ACK;
				QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
					  QDF_TRACE_LEVEL_INFO_HIGH,
					  "%s : TCP Syn ack packet", __func__);
			}
		} else if (qdf_nbuf_is_ipv4_udp_pkt(skb)) {
			if (qdf_nbuf_data_is_dns_response(skb) &&
			    hdd_tx_rx_is_dns_domain_name_match(skb, adapter)) {
				++adapter->hdd_stats.hdd_dns_stats.
							rx_dns_rsp_count;
				*pkt_type = CONNECTIVITY_CHECK_SET_DNS;
				QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
					  QDF_TRACE_LEVEL_INFO_HIGH,
					  "%s : DNS response packet", __func__);
			}
		}
		break;

	case PKT_TYPE_TX_DROPPED:
		switch (*pkt_type) {
		case CONNECTIVITY_CHECK_SET_ICMPV4:
			++adapter->hdd_stats.hdd_icmpv4_stats.tx_dropped;
			QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
				  QDF_TRACE_LEVEL_INFO_HIGH,
				  "%s : ICMPv4 Req packet dropped", __func__);
			break;
		case CONNECTIVITY_CHECK_SET_TCP_SYN:
			++adapter->hdd_stats.hdd_tcp_stats.tx_tcp_syn_dropped;
			QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
				  QDF_TRACE_LEVEL_INFO_HIGH,
				  "%s : TCP syn packet dropped", __func__);
			break;
		case CONNECTIVITY_CHECK_SET_TCP_ACK:
			++adapter->hdd_stats.hdd_tcp_stats.tx_tcp_ack_dropped;
			QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
				  QDF_TRACE_LEVEL_INFO_HIGH,
				  "%s : TCP ack packet dropped", __func__);
			break;
		case CONNECTIVITY_CHECK_SET_DNS:
			++adapter->hdd_stats.hdd_dns_stats.tx_dropped;
			QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
				  QDF_TRACE_LEVEL_INFO_HIGH,
				  "%s : DNS query packet dropped", __func__);
			break;
		default:
			break;
		}
		break;
	case PKT_TYPE_RX_DELIVERED:
		switch (*pkt_type) {
		case CONNECTIVITY_CHECK_SET_ICMPV4:
			++adapter->hdd_stats.hdd_icmpv4_stats.rx_delivered;
			break;
		case CONNECTIVITY_CHECK_SET_TCP_SYN_ACK:
			++adapter->hdd_stats.hdd_tcp_stats.rx_delivered;
			break;
		case CONNECTIVITY_CHECK_SET_DNS:
			++adapter->hdd_stats.hdd_dns_stats.rx_delivered;
			break;
		default:
			break;
		}
		break;
	case PKT_TYPE_RX_REFUSED:
		switch (*pkt_type) {
		case CONNECTIVITY_CHECK_SET_ICMPV4:
			++adapter->hdd_stats.hdd_icmpv4_stats.rx_refused;
			break;
		case CONNECTIVITY_CHECK_SET_TCP_SYN_ACK:
			++adapter->hdd_stats.hdd_tcp_stats.rx_refused;
			break;
		case CONNECTIVITY_CHECK_SET_DNS:
			++adapter->hdd_stats.hdd_dns_stats.rx_refused;
			break;
		default:
			break;
		}
		break;
	case PKT_TYPE_TX_ACK_CNT:
		switch (*pkt_type) {
		case CONNECTIVITY_CHECK_SET_ICMPV4:
			++adapter->hdd_stats.hdd_icmpv4_stats.tx_ack_cnt;
			break;
		case CONNECTIVITY_CHECK_SET_TCP_SYN:
			++adapter->hdd_stats.hdd_tcp_stats.tx_tcp_syn_ack_cnt;
			break;
		case CONNECTIVITY_CHECK_SET_TCP_ACK:
			++adapter->hdd_stats.hdd_tcp_stats.tx_tcp_ack_ack_cnt;
			break;
		case CONNECTIVITY_CHECK_SET_DNS:
			++adapter->hdd_stats.hdd_dns_stats.tx_ack_cnt;
			break;
		default:
			break;
		}
		break;
	default:
		break;
	}
}

/**
 * hdd_is_xmit_allowed_on_ndi() - Verify if xmit is allowed on NDI
 * @adapter: The adapter structure
 *
 * Return: True if xmit is allowed on NDI and false otherwise
 */
static bool hdd_is_xmit_allowed_on_ndi(struct hdd_adapter *adapter)
{
	enum nan_datapath_state state;
	struct wlan_objmgr_vdev *vdev;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return false;

	state = ucfg_nan_get_ndi_state(vdev);
	hdd_objmgr_put_vdev(vdev);
	return (state == NAN_DATA_NDI_CREATED_STATE ||
		state == NAN_DATA_CONNECTED_STATE ||
		state == NAN_DATA_CONNECTING_STATE ||
		state == NAN_DATA_PEER_CREATE_STATE);
}

/**
 * hdd_get_transmit_mac_addr() - Get the mac address to validate the xmit
 * @adapter: The adapter structure
 * @skb: The network buffer
 * @mac_addr_tx_allowed: The mac address to be filled
 *
 * Return: None
 */
static
void hdd_get_transmit_mac_addr(struct hdd_adapter *adapter, struct sk_buff *skb,
			       struct qdf_mac_addr *mac_addr_tx_allowed)
{
	struct hdd_station_ctx *sta_ctx = &adapter->session.station;
	bool is_mc_bc_addr = false;

	if (QDF_NBUF_CB_GET_IS_BCAST(skb) || QDF_NBUF_CB_GET_IS_MCAST(skb))
		is_mc_bc_addr = true;

	if (adapter->device_mode == QDF_NDI_MODE &&
		   hdd_is_xmit_allowed_on_ndi(adapter)) {
		if (is_mc_bc_addr)
			qdf_copy_macaddr(mac_addr_tx_allowed,
					 &adapter->mac_addr);
		else
			qdf_copy_macaddr(mac_addr_tx_allowed,
					 (struct qdf_mac_addr *)skb->data);
	} else {
		if (sta_ctx->conn_info.conn_state ==
		    eConnectionState_Associated)
			qdf_copy_macaddr(mac_addr_tx_allowed,
					 &sta_ctx->conn_info.bssid);
	}
}

#ifdef HANDLE_BROADCAST_EAPOL_TX_FRAME
/**
 * wlan_hdd_fix_broadcast_eapol() - Fix broadcast eapol
 * @adapter: pointer to adapter
 * @skb: pointer to OS packet (sk_buff)
 *
 * Override DA of broadcast eapol with bssid addr.
 *
 * Return: None
 */
static void wlan_hdd_fix_broadcast_eapol(struct hdd_adapter *adapter,
					 struct sk_buff *skb)
{
	struct ethhdr *eh = (struct ethhdr *)skb->data;
	unsigned char *ap_mac_addr =
		&adapter->session.station.conn_info.bssid.bytes[0];

	if (qdf_unlikely((QDF_NBUF_CB_GET_PACKET_TYPE(skb) ==
			  QDF_NBUF_CB_PACKET_TYPE_EAPOL) &&
			 QDF_NBUF_CB_GET_IS_BCAST(skb))) {
		hdd_debug("SA: "QDF_MAC_ADDR_FMT " override DA: "QDF_MAC_ADDR_FMT " with AP mac address "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(&eh->h_source[0]),
			  QDF_MAC_ADDR_REF(&eh->h_dest[0]),
			  QDF_MAC_ADDR_REF(ap_mac_addr));

		qdf_mem_copy(&eh->h_dest, ap_mac_addr, QDF_MAC_ADDR_SIZE);
	}
}
#else
static void wlan_hdd_fix_broadcast_eapol(struct hdd_adapter *adapter,
					 struct sk_buff *skb)
{
}
#endif /* HANDLE_BROADCAST_EAPOL_TX_FRAME */

#ifdef WLAN_DP_FEATURE_MARK_ICMP_REQ_TO_FW
/**
 * hdd_mark_icmp_req_to_fw() - Mark the ICMP request at a certain time interval
 *			       to be sent to the FW.
 * @hdd_ctx: Global hdd context (Caller's responsibility to validate)
 * @skb: packet to be transmitted
 *
 * This func sets the "to_fw" flag in the packet context block, if the
 * current packet is an ICMP request packet. This marking is done at a
 * specific time interval, unless the INI value indicates to disable/enable
 * this for all frames.
 *
 * Return: none
 */
static void hdd_mark_icmp_req_to_fw(struct hdd_context *hdd_ctx,
				    struct sk_buff *skb)
{
	uint64_t curr_time, time_delta;
	int time_interval_ms = hdd_ctx->config->icmp_req_to_fw_mark_interval;
	static uint64_t prev_marked_icmp_time;

	if (!hdd_ctx->config->icmp_req_to_fw_mark_interval)
		return;

	if (qdf_nbuf_get_icmp_subtype(skb) != QDF_PROTO_ICMP_REQ)
		return;

	/* Mark all ICMP request to be sent to FW */
	if (time_interval_ms == WLAN_CFG_ICMP_REQ_TO_FW_MARK_ALL)
		QDF_NBUF_CB_TX_PACKET_TO_FW(skb) = 1;

	curr_time = qdf_get_log_timestamp();
	time_delta = curr_time - prev_marked_icmp_time;
	if (time_delta >= (time_interval_ms *
			   QDF_LOG_TIMESTAMP_CYCLES_PER_10_US * 100)) {
		QDF_NBUF_CB_TX_PACKET_TO_FW(skb) = 1;
		prev_marked_icmp_time = curr_time;
	}
}
#else
static void hdd_mark_icmp_req_to_fw(struct hdd_context *hdd_ctx,
				    struct sk_buff *skb)
{
}
#endif

/**
 * __hdd_hard_start_xmit() - Transmit a frame
 * @skb: pointer to OS packet (sk_buff)
 * @dev: pointer to network device
 *
 * Function registered with the Linux OS for transmitting
 * packets. This version of the function directly passes
 * the packet to Transport Layer.
 * In case of any packet drop or error, log the error with
 * INFO HIGH/LOW/MEDIUM to avoid excessive logging in kmsg.
 *
 * Return: None
 */
static void __hdd_hard_start_xmit(struct sk_buff *skb,
				  struct net_device *dev)
{
	QDF_STATUS status;
	sme_ac_enum_type ac;
	enum sme_qos_wmmuptype up;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	bool granted;
	struct hdd_station_ctx *sta_ctx = &adapter->session.station;
	struct qdf_mac_addr mac_addr;
	struct qdf_mac_addr mac_addr_tx_allowed = QDF_MAC_ADDR_ZERO_INIT;
	uint8_t pkt_type = 0;
	bool is_arp = false;
	struct wlan_objmgr_vdev *vdev;
	struct hdd_context *hdd_ctx;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	enum qdf_proto_subtype subtype = QDF_PROTO_INVALID;
	bool is_eapol = false;
	bool is_dhcp = false;

#ifdef QCA_WIFI_FTM
	if (hdd_get_conparam() == QDF_GLOBAL_FTM_MODE) {
		kfree_skb(skb);
		return;
	}
#endif

	++adapter->hdd_stats.tx_rx_stats.tx_called;
	adapter->hdd_stats.tx_rx_stats.cont_txtimeout_cnt = 0;
	qdf_mem_copy(mac_addr.bytes, skb->data, sizeof(mac_addr.bytes));

	if (cds_is_driver_recovering() || cds_is_driver_in_bad_state() ||
	    cds_is_load_or_unload_in_progress()) {
		QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_HDD_DATA,
				   "Recovery/(Un)load in progress, dropping the packet");
		goto drop_pkt;
	}

	hdd_ctx = adapter->hdd_ctx;
	if (wlan_hdd_validate_context(hdd_ctx))
		goto drop_pkt;

	if (hdd_ctx->hdd_wlan_suspended) {
		hdd_err_rl("Device is system suspended, drop pkt");
		goto drop_pkt;
	}

	wlan_hdd_classify_pkt(skb);
	if (QDF_NBUF_CB_GET_PACKET_TYPE(skb) == QDF_NBUF_CB_PACKET_TYPE_ARP) {
		if (qdf_nbuf_data_is_arp_req(skb) &&
		    (adapter->track_arp_ip == qdf_nbuf_get_arp_tgt_ip(skb))) {
			is_arp = true;
			QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(skb) = 1;
			++adapter->hdd_stats.hdd_arp_stats.tx_arp_req_count;
			QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
				  QDF_TRACE_LEVEL_INFO_HIGH,
					"%s : ARP packet", __func__);
		}
	} else if (QDF_NBUF_CB_GET_PACKET_TYPE(skb) ==
		   QDF_NBUF_CB_PACKET_TYPE_EAPOL) {
		subtype = qdf_nbuf_get_eapol_subtype(skb);
		if (subtype == QDF_PROTO_EAPOL_M2) {
			++adapter->hdd_stats.hdd_eapol_stats.eapol_m2_count;
			QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(skb) = 1;
			is_eapol = true;
		} else if (subtype == QDF_PROTO_EAPOL_M4) {
			++adapter->hdd_stats.hdd_eapol_stats.eapol_m4_count;
			QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(skb) = 1;
			is_eapol = true;
		}
	} else if (QDF_NBUF_CB_GET_PACKET_TYPE(skb) ==
		   QDF_NBUF_CB_PACKET_TYPE_DHCP) {
		subtype = qdf_nbuf_get_dhcp_subtype(skb);
		if (subtype == QDF_PROTO_DHCP_DISCOVER) {
			++adapter->hdd_stats.hdd_dhcp_stats.dhcp_dis_count;
			QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(skb) = 1;
			is_dhcp = true;
		} else if (subtype == QDF_PROTO_DHCP_REQUEST) {
			++adapter->hdd_stats.hdd_dhcp_stats.dhcp_req_count;
			QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(skb) = 1;
			is_dhcp = true;
		}
	} else if (QDF_NBUF_CB_GET_PACKET_TYPE(skb) ==
		   QDF_NBUF_CB_PACKET_TYPE_ICMP) {
		hdd_mark_icmp_req_to_fw(hdd_ctx, skb);
	}

	/* track connectivity stats */
	if (adapter->pkt_type_bitmap)
		hdd_tx_rx_collect_connectivity_stats_info(skb, adapter,
						PKT_TYPE_REQ, &pkt_type);

	hdd_get_transmit_mac_addr(adapter, skb, &mac_addr_tx_allowed);
	if (qdf_is_macaddr_zero(&mac_addr_tx_allowed)) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_INFO_HIGH,
			  "tx not allowed, transmit operation suspended");
		goto drop_pkt;
	}

	hdd_get_tx_resource(adapter, &mac_addr_tx_allowed,
			    WLAN_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);

	/* Get TL AC corresponding to Qdisc queue index/AC. */
	ac = hdd_qdisc_ac_to_tl_ac[skb->queue_mapping];

	if (!qdf_nbuf_ipa_owned_get(skb)) {
		skb = hdd_skb_orphan(adapter, skb);
		if (!skb)
			goto drop_pkt_accounting;
	}

	/*
	 * Add SKB to internal tracking table before further processing
	 * in WLAN driver.
	 */
	qdf_net_buf_debug_acquire_skb(skb, __FILE__, __LINE__);

	/*
	 * user priority from IP header, which is already extracted and set from
	 * select_queue call back function
	 */
	up = skb->priority;

	++adapter->hdd_stats.tx_rx_stats.tx_classified_ac[ac];
#ifdef HDD_WMM_DEBUG
	QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_DEBUG,
		  "%s: Classified as ac %d up %d", __func__, ac, up);
#endif /* HDD_WMM_DEBUG */

	if (HDD_PSB_CHANGED == adapter->psb_changed) {
		/*
		 * Function which will determine acquire admittance for a
		 * WMM AC is required or not based on psb configuration done
		 * in the framework
		 */
		hdd_wmm_acquire_access_required(adapter, ac);
	}
	/*
	 * Make sure we already have access to this access category
	 * or it is EAPOL or WAPI frame during initial authentication which
	 * can have artifically boosted higher qos priority.
	 */

	if (((adapter->psb_changed & (1 << ac)) &&
		likely(adapter->hdd_wmm_status.ac_status[ac].
			is_access_allowed)) ||
		((sta_ctx->conn_info.is_authenticated == false) &&
		 (QDF_NBUF_CB_PACKET_TYPE_EAPOL ==
			QDF_NBUF_CB_GET_PACKET_TYPE(skb) ||
		  QDF_NBUF_CB_PACKET_TYPE_WAPI ==
			QDF_NBUF_CB_GET_PACKET_TYPE(skb)))) {
		granted = true;
	} else {
		status = hdd_wmm_acquire_access(adapter, ac, &granted);
		adapter->psb_changed |= (1 << ac);
	}

	if (!granted) {
		bool isDefaultAc = false;
		/*
		 * ADDTS request for this AC is sent, for now
		 * send this packet through next available lower
		 * Access category until ADDTS negotiation completes.
		 */
		while (!likely
			       (adapter->hdd_wmm_status.ac_status[ac].
			       is_access_allowed)) {
			switch (ac) {
			case SME_AC_VO:
				ac = SME_AC_VI;
				up = SME_QOS_WMM_UP_VI;
				break;
			case SME_AC_VI:
				ac = SME_AC_BE;
				up = SME_QOS_WMM_UP_BE;
				break;
			case SME_AC_BE:
				ac = SME_AC_BK;
				up = SME_QOS_WMM_UP_BK;
				break;
			default:
				ac = SME_AC_BK;
				up = SME_QOS_WMM_UP_BK;
				isDefaultAc = true;
				break;
			}
			if (isDefaultAc)
				break;
		}
		skb->priority = up;
		skb->queue_mapping = hdd_linux_up_to_ac_map[up];
	}

	adapter->stats.tx_bytes += skb->len;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (vdev) {
		ucfg_tdls_update_tx_pkt_cnt(vdev, &mac_addr);
		hdd_objmgr_put_vdev(vdev);
	}

	if (qdf_nbuf_is_tso(skb)) {
		adapter->stats.tx_packets += qdf_nbuf_get_tso_num_seg(skb);
	} else {
		++adapter->stats.tx_packets;
		hdd_ctx->no_tx_offload_pkt_cnt++;
	}

	hdd_event_eapol_log(skb, QDF_TX);
	QDF_NBUF_CB_TX_PACKET_TRACK(skb) = QDF_NBUF_TX_PKT_DATA_TRACK;
	QDF_NBUF_UPDATE_TX_PKT_COUNT(skb, QDF_NBUF_TX_PKT_HDD);

	qdf_dp_trace_set_track(skb, QDF_TX);

	DPTRACE(qdf_dp_trace(skb, QDF_DP_TRACE_HDD_TX_PACKET_PTR_RECORD,
			QDF_TRACE_DEFAULT_PDEV_ID, qdf_nbuf_data_addr(skb),
			sizeof(qdf_nbuf_data(skb)),
			QDF_TX));

	if (!hdd_is_tx_allowed(skb, adapter->vdev_id,
			       mac_addr_tx_allowed.bytes)) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
			  QDF_TRACE_LEVEL_INFO_HIGH,
			  FL("Tx not allowed for sta: "
			  QDF_MAC_ADDR_FMT), QDF_MAC_ADDR_REF(
			  mac_addr_tx_allowed.bytes));
		++adapter->hdd_stats.tx_rx_stats.tx_dropped_ac[ac];
		goto drop_pkt_and_release_skb;
	}

	/* check whether need to linearize skb, like non-linear udp data */
	if (hdd_skb_nontso_linearize(skb) != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
			  QDF_TRACE_LEVEL_INFO_HIGH,
			  "%s: skb %pK linearize failed. drop the pkt",
			  __func__, skb);
		++adapter->hdd_stats.tx_rx_stats.tx_dropped_ac[ac];
		goto drop_pkt_and_release_skb;
	}

	/*
	 * If a transmit function is not registered, drop packet
	 */
	if (!adapter->tx_fn) {
		QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_INFO_HIGH,
			 "%s: TX function not registered by the data path",
			 __func__);
		++adapter->hdd_stats.tx_rx_stats.tx_dropped_ac[ac];
		goto drop_pkt_and_release_skb;
	}

	wlan_hdd_fix_broadcast_eapol(adapter, skb);

	if (adapter->tx_fn(soc, adapter->vdev_id, (qdf_nbuf_t)skb)) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_INFO_HIGH,
			  "%s: Failed to send packet to txrx for sta_id: "
			  QDF_MAC_ADDR_FMT,
			  __func__, QDF_MAC_ADDR_REF(mac_addr.bytes));
		++adapter->hdd_stats.tx_rx_stats.tx_dropped_ac[ac];
		goto drop_pkt_and_release_skb;
	}

	netif_trans_update(dev);

	wlan_hdd_sar_unsolicited_timer_start(hdd_ctx);

	return;

drop_pkt_and_release_skb:
	qdf_net_buf_debug_release_skb(skb);
drop_pkt:

	/* track connectivity stats */
	if (adapter->pkt_type_bitmap)
		hdd_tx_rx_collect_connectivity_stats_info(skb, adapter,
							  PKT_TYPE_TX_DROPPED,
							  &pkt_type);
	qdf_dp_trace_data_pkt(skb, QDF_TRACE_DEFAULT_PDEV_ID,
			      QDF_DP_TRACE_DROP_PACKET_RECORD, 0,
			      QDF_TX);
	kfree_skb(skb);

drop_pkt_accounting:

	++adapter->stats.tx_dropped;
	++adapter->hdd_stats.tx_rx_stats.tx_dropped;
	if (is_arp) {
		++adapter->hdd_stats.hdd_arp_stats.tx_dropped;
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_INFO_HIGH,
			"%s : ARP packet dropped", __func__);
	} else if (is_eapol) {
		++adapter->hdd_stats.hdd_eapol_stats.
				tx_dropped[subtype - QDF_PROTO_EAPOL_M1];
	} else if (is_dhcp) {
		++adapter->hdd_stats.hdd_dhcp_stats.
				tx_dropped[subtype - QDF_PROTO_DHCP_DISCOVER];
	}
}

/**
 * hdd_hard_start_xmit() - Wrapper function to protect
 * __hdd_hard_start_xmit from SSR
 * @skb: pointer to OS packet
 * @net_dev: pointer to net_device structure
 *
 * Function called by OS if any packet needs to transmit.
 *
 * Return: Always returns NETDEV_TX_OK
 */
netdev_tx_t hdd_hard_start_xmit(struct sk_buff *skb, struct net_device *net_dev)
{
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(net_dev, &vdev_sync)) {
		hdd_debug_rl("Operation on net_dev is not permitted");
		kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	__hdd_hard_start_xmit(skb, net_dev);

	osif_vdev_sync_op_stop(vdev_sync);

	return NETDEV_TX_OK;
}

/**
 * __hdd_tx_timeout() - TX timeout handler
 * @dev: pointer to network device
 *
 * This function is registered as a netdev ndo_tx_timeout method, and
 * is invoked by the kernel if the driver takes too long to transmit a
 * frame.
 *
 * Return: None
 */
static void __hdd_tx_timeout(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;
	struct netdev_queue *txq;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	u64 diff_jiffies;
	int i = 0;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (hdd_ctx->hdd_wlan_suspended) {
		hdd_debug("Device is suspended, ignore WD timeout");
		return;
	}

	TX_TIMEOUT_TRACE(dev, QDF_MODULE_ID_HDD_DATA);
	DPTRACE(qdf_dp_trace(NULL, QDF_DP_TRACE_HDD_TX_TIMEOUT,
				QDF_TRACE_DEFAULT_PDEV_ID,
				NULL, 0, QDF_TX));

	/* Getting here implies we disabled the TX queues for too
	 * long. Queues are disabled either because of disassociation
	 * or low resource scenarios. In case of disassociation it is
	 * ok to ignore this. But if associated, we have do possible
	 * recovery here
	 */

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		txq = netdev_get_tx_queue(dev, i);
		hdd_debug("Queue: %d status: %d txq->trans_start: %lu",
			  i, netif_tx_queue_stopped(txq), txq->trans_start);
	}

	hdd_debug("carrier state: %d", netif_carrier_ok(dev));

	wlan_hdd_display_adapter_netif_queue_history(adapter);

	cdp_dump_flow_pool_info(cds_get_context(QDF_MODULE_ID_SOC));

	++adapter->hdd_stats.tx_rx_stats.tx_timeout_cnt;
	++adapter->hdd_stats.tx_rx_stats.cont_txtimeout_cnt;

	diff_jiffies = jiffies -
		       adapter->hdd_stats.tx_rx_stats.jiffies_last_txtimeout;

	if ((adapter->hdd_stats.tx_rx_stats.cont_txtimeout_cnt > 1) &&
	    (diff_jiffies > (HDD_TX_TIMEOUT * 2))) {
		/*
		 * In case when there is no traffic is running, it may
		 * possible tx time-out may once happen and later system
		 * recovered then continuous tx timeout count has to be
		 * reset as it is gets modified only when traffic is running.
		 * If over a period of time if this count reaches to threshold
		 * then host triggers a false subsystem restart. In genuine
		 * time out case kernel will call the tx time-out back to back
		 * at interval of HDD_TX_TIMEOUT. Here now check if previous
		 * TX TIME out has occurred more than twice of HDD_TX_TIMEOUT
		 * back then host may recovered here from data stall.
		 */
		adapter->hdd_stats.tx_rx_stats.cont_txtimeout_cnt = 0;
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_DEBUG,
			  "Reset continuous tx timeout stat");
	}

	adapter->hdd_stats.tx_rx_stats.jiffies_last_txtimeout = jiffies;

	if (adapter->hdd_stats.tx_rx_stats.cont_txtimeout_cnt >
	    HDD_TX_STALL_THRESHOLD) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			  "Data stall due to continuous TX timeouts");
		adapter->hdd_stats.tx_rx_stats.cont_txtimeout_cnt = 0;

		if (cdp_cfg_get(soc, cfg_dp_enable_data_stall))
			cdp_post_data_stall_event(soc,
					  DATA_STALL_LOG_INDICATOR_HOST_DRIVER,
					  DATA_STALL_LOG_HOST_STA_TX_TIMEOUT,
					  OL_TXRX_PDEV_ID, 0xFF,
					  DATA_STALL_LOG_RECOVERY_TRIGGER_PDR);
	}
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
void hdd_tx_timeout(struct net_device *net_dev, unsigned int txqueue)
#else
void hdd_tx_timeout(struct net_device *net_dev)
#endif
{
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(net_dev, &vdev_sync))
		return;

	__hdd_tx_timeout(net_dev);

	osif_vdev_sync_op_stop(vdev_sync);
}

/**
 * @hdd_init_tx_rx() - Initialize Tx/RX module
 * @adapter: pointer to adapter context
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS hdd_init_tx_rx(struct hdd_adapter *adapter)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	if (!adapter) {
		hdd_err("adapter is NULL");
		QDF_ASSERT(0);
		return QDF_STATUS_E_FAILURE;
	}

	return status;
}

/**
 * @hdd_deinit_tx_rx() - Deinitialize Tx/RX module
 * @adapter: pointer to adapter context
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS hdd_deinit_tx_rx(struct hdd_adapter *adapter)
{
	QDF_BUG(adapter);
	if (!adapter)
		return QDF_STATUS_E_FAILURE;

	adapter->tx_fn = NULL;

	return QDF_STATUS_SUCCESS;
}

#ifdef FEATURE_MONITOR_MODE_SUPPORT
QDF_STATUS hdd_mon_rx_packet_cbk(void *context, qdf_nbuf_t rxbuf)
{
	struct hdd_adapter *adapter;
	int rxstat;
	struct sk_buff *skb;
	struct sk_buff *skb_next;
	unsigned int cpu_index;

	/* Sanity check on inputs */
	if ((!context) || (!rxbuf)) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			  "%s: Null params being passed", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	adapter = (struct hdd_adapter *)context;
	if ((!adapter) || (WLAN_HDD_ADAPTER_MAGIC != adapter->magic)) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			  "invalid adapter %pK", adapter);
		return QDF_STATUS_E_FAILURE;
	}

	cpu_index = wlan_hdd_get_cpu();

	/* walk the chain until all are processed */
	skb = (struct sk_buff *) rxbuf;
	while (skb) {
		skb_next = skb->next;
		skb->dev = adapter->dev;

		++adapter->hdd_stats.tx_rx_stats.rx_packets[cpu_index];
		++adapter->stats.rx_packets;
		adapter->stats.rx_bytes += skb->len;

		/* Remove SKB from internal tracking table before submitting
		 * it to stack
		 */
		qdf_net_buf_debug_release_skb(skb);

		/*
		 * If this is not a last packet on the chain
		 * Just put packet into backlog queue, not scheduling RX sirq
		 */
		if (skb->next) {
			rxstat = netif_rx(skb);
		} else {
			/*
			 * This is the last packet on the chain
			 * Scheduling rx sirq
			 */
			rxstat = netif_rx_ni(skb);
		}

		if (NET_RX_SUCCESS == rxstat)
			++adapter->
				hdd_stats.tx_rx_stats.rx_delivered[cpu_index];
		else
			++adapter->hdd_stats.tx_rx_stats.rx_refused[cpu_index];

		skb = skb_next;
	}

	return QDF_STATUS_SUCCESS;
}
#endif

/*
 * hdd_is_mcast_replay() - checks if pkt is multicast replay
 * @skb: packet skb
 *
 * Return: true if replayed multicast pkt, false otherwise
 */
static bool hdd_is_mcast_replay(struct sk_buff *skb)
{
	struct ethhdr *eth;

	eth = eth_hdr(skb);
	if (unlikely(skb->pkt_type == PACKET_MULTICAST)) {
		if (unlikely(ether_addr_equal(eth->h_source,
				skb->dev->dev_addr)))
			return true;
	}
	return false;
}

/**
 * hdd_is_arp_local() - check if local or non local arp
 * @skb: pointer to sk_buff
 *
 * Return: true if local arp or false otherwise.
 */
static bool hdd_is_arp_local(struct sk_buff *skb)
{
	struct arphdr *arp;
	struct in_ifaddr **ifap = NULL;
	struct in_ifaddr *ifa = NULL;
	struct in_device *in_dev;
	unsigned char *arp_ptr;
	__be32 tip;

	arp = (struct arphdr *)skb->data;
	if (arp->ar_op == htons(ARPOP_REQUEST)) {
		/* if fail to acquire rtnl lock, assume it's local arp */
		if (!rtnl_trylock())
			return true;

		in_dev = __in_dev_get_rtnl(skb->dev);
		if (in_dev) {
			for (ifap = &in_dev->ifa_list; (ifa = *ifap) != NULL;
				ifap = &ifa->ifa_next) {
				if (!strcmp(skb->dev->name, ifa->ifa_label))
					break;
			}
		}

		if (ifa && ifa->ifa_local) {
			arp_ptr = (unsigned char *)(arp + 1);
			arp_ptr += (skb->dev->addr_len + 4 +
					skb->dev->addr_len);
			memcpy(&tip, arp_ptr, 4);
			hdd_debug("ARP packet: local IP: %x dest IP: %x",
				ifa->ifa_local, tip);
			if (ifa->ifa_local == tip) {
				rtnl_unlock();
				return true;
			}
		}
		rtnl_unlock();
	}

	return false;
}

/**
 * hdd_is_rx_wake_lock_needed() - check if wake lock is needed
 * @skb: pointer to sk_buff
 *
 * RX wake lock is needed for:
 * 1) Unicast data packet OR
 * 2) Local ARP data packet
 *
 * Return: true if wake lock is needed or false otherwise.
 */
static bool hdd_is_rx_wake_lock_needed(struct sk_buff *skb)
{
	if ((skb->pkt_type != PACKET_BROADCAST &&
	     skb->pkt_type != PACKET_MULTICAST) || hdd_is_arp_local(skb))
		return true;

	return false;
}

#ifdef RECEIVE_OFFLOAD
/**
 * hdd_resolve_rx_ol_mode() - Resolve Rx offload method, LRO or GRO
 * @hdd_ctx: pointer to HDD Station Context
 *
 * Return: None
 */
static void hdd_resolve_rx_ol_mode(struct hdd_context *hdd_ctx)
{
	void *soc;

	soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (!(cdp_cfg_get(soc, cfg_dp_lro_enable) ^
	    cdp_cfg_get(soc, cfg_dp_gro_enable))) {
		cdp_cfg_get(soc, cfg_dp_lro_enable) &&
			cdp_cfg_get(soc, cfg_dp_gro_enable) ?
		hdd_debug("Can't enable both LRO and GRO, disabling Rx offload") :
		hdd_debug("LRO and GRO both are disabled");
		hdd_ctx->ol_enable = 0;
	} else if (cdp_cfg_get(soc, cfg_dp_lro_enable)) {
		hdd_debug("Rx offload LRO is enabled");
		hdd_ctx->ol_enable = CFG_LRO_ENABLED;
	} else {
		hdd_debug("Rx offload: GRO is enabled");
		hdd_ctx->ol_enable = CFG_GRO_ENABLED;
	}
}

/**
 * When bus bandwidth is idle, if RX data is delivered with
 * napi_gro_receive, to reduce RX delay related with GRO,
 * check gro_result returned from napi_gro_receive to determine
 * is extra GRO flush still necessary.
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 4, 0))
#define HDD_IS_EXTRA_GRO_FLUSH_NECESSARY(_gro_ret) \
	((_gro_ret) != GRO_DROP)
#else
#define HDD_IS_EXTRA_GRO_FLUSH_NECESSARY(_gro_ret) \
	((_gro_ret) != GRO_DROP && (_gro_ret) != GRO_NORMAL)
#endif

#ifdef WLAN_FEATURE_DYNAMIC_RX_AGGREGATION
/**
 * hdd_gro_rx_bh_disable() - GRO RX/flush function.
 * @napi_to_use: napi to be used to give packets to the stack, gro flush
 * @skb: pointer to sk_buff
 *
 * Function calls napi_gro_receive for the skb. If the skb indicates that a
 * flush needs to be done (set by the lower DP layer), the function also calls
 * napi_gro_flush. Local softirqs are disabled (and later enabled) while making
 * napi_gro__ calls.
 *
 * Return: QDF_STATUS_SUCCESS if not dropped by napi_gro_receive or
 *	   QDF error code.
 */
static QDF_STATUS hdd_gro_rx_bh_disable(struct hdd_adapter *adapter,
					struct napi_struct *napi_to_use,
					struct sk_buff *skb)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;
	gro_result_t gro_ret;
	uint32_t rx_aggregation;
	uint8_t rx_ctx_id = QDF_NBUF_CB_RX_CTX_ID(skb);

	rx_aggregation = qdf_atomic_read(&hdd_ctx->dp_agg_param.rx_aggregation);

	skb_set_hash(skb, QDF_NBUF_CB_RX_FLOW_ID(skb), PKT_HASH_TYPE_L4);

	local_bh_disable();
	gro_ret = napi_gro_receive(napi_to_use, skb);

	if (hdd_get_current_throughput_level(hdd_ctx) == PLD_BUS_WIDTH_IDLE ||
	    !rx_aggregation || adapter->gro_disallowed[rx_ctx_id]) {
		if (HDD_IS_EXTRA_GRO_FLUSH_NECESSARY(gro_ret)) {
			adapter->hdd_stats.tx_rx_stats.
					rx_gro_low_tput_flush++;
			dp_rx_napi_gro_flush(napi_to_use,
					     DP_RX_GRO_NORMAL_FLUSH);
		}
		if (!rx_aggregation)
			hdd_ctx->dp_agg_param.gro_force_flush[rx_ctx_id] = 1;
		if (adapter->gro_disallowed[rx_ctx_id])
			adapter->gro_flushed[rx_ctx_id] = 1;
	}
	local_bh_enable();

	if (gro_ret == GRO_DROP)
		status = QDF_STATUS_E_GRO_DROP;

	return status;
}

#else /* WLAN_FEATURE_DYNAMIC_RX_AGGREGATION */

/**
 * hdd_gro_rx_bh_disable() - GRO RX/flush function.
 * @napi_to_use: napi to be used to give packets to the stack, gro flush
 * @skb: pointer to sk_buff
 *
 * Function calls napi_gro_receive for the skb. If the skb indicates that a
 * flush needs to be done (set by the lower DP layer), the function also calls
 * napi_gro_flush. Local softirqs are disabled (and later enabled) while making
 * napi_gro__ calls.
 *
 * Return: QDF_STATUS_SUCCESS if not dropped by napi_gro_receive or
 *	   QDF error code.
 */
static QDF_STATUS hdd_gro_rx_bh_disable(struct hdd_adapter *adapter,
					struct napi_struct *napi_to_use,
					struct sk_buff *skb)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;
	gro_result_t gro_ret;

	skb_set_hash(skb, QDF_NBUF_CB_RX_FLOW_ID(skb), PKT_HASH_TYPE_L4);

	local_bh_disable();
	gro_ret = napi_gro_receive(napi_to_use, skb);

	if (hdd_get_current_throughput_level(hdd_ctx) == PLD_BUS_WIDTH_IDLE) {
		if (HDD_IS_EXTRA_GRO_FLUSH_NECESSARY(gro_ret)) {
			adapter->hdd_stats.tx_rx_stats.rx_gro_low_tput_flush++;
			dp_rx_napi_gro_flush(napi_to_use,
					     DP_RX_GRO_NORMAL_FLUSH);
		}
	}
	local_bh_enable();

	if (gro_ret == GRO_DROP)
		status = QDF_STATUS_E_GRO_DROP;

	return status;
}
#endif /* WLAN_FEATURE_DYNAMIC_RX_AGGREGATION */

/**
 * hdd_gro_rx_dp_thread() - Handle Rx procesing via GRO for DP thread
 * @adapter: pointer to adapter context
 * @skb: pointer to sk_buff
 *
 * Return: QDF_STATUS_SUCCESS if processed via GRO or non zero return code
 */
static
QDF_STATUS hdd_gro_rx_dp_thread(struct hdd_adapter *adapter,
				struct sk_buff *skb)
{
	struct napi_struct *napi_to_use = NULL;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;

	if (!adapter->hdd_ctx->enable_dp_rx_threads) {
		hdd_dp_err_rl("gro not supported without DP RX thread!");
		return status;
	}

	napi_to_use =
		dp_rx_get_napi_context(cds_get_context(QDF_MODULE_ID_SOC),
				       QDF_NBUF_CB_RX_CTX_ID(skb));

	if (!napi_to_use) {
		hdd_dp_err_rl("no napi to use for GRO!");
		return status;
	}

	status = hdd_gro_rx_bh_disable(adapter, napi_to_use, skb);

	return status;
}

/**
 * hdd_gro_rx_legacy() - Handle Rx processing via GRO for ihelium based targets
 * @adapter: pointer to adapter context
 * @skb: pointer to sk_buff
 *
 * Supports GRO for only station mode
 *
 * Return: QDF_STATUS_SUCCESS if processed via GRO or non zero return code
 */
static
QDF_STATUS hdd_gro_rx_legacy(struct hdd_adapter *adapter, struct sk_buff *skb)
{
	struct qca_napi_info *qca_napii;
	struct qca_napi_data *napid;
	struct napi_struct *napi_to_use;
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;

	/* Only enabling it for STA mode like LRO today */
	if (QDF_STA_MODE != adapter->device_mode)
		return QDF_STATUS_E_NOSUPPORT;

	if (qdf_atomic_read(&hdd_ctx->disable_rx_ol_in_low_tput) ||
	    qdf_atomic_read(&hdd_ctx->disable_rx_ol_in_concurrency))
		return QDF_STATUS_E_NOSUPPORT;

	napid = hdd_napi_get_all();
	if (unlikely(!napid))
		goto out;

	qca_napii = hif_get_napi(QDF_NBUF_CB_RX_CTX_ID(skb), napid);
	if (unlikely(!qca_napii))
		goto out;

	/*
	 * As we are breaking context in Rxthread mode, there is rx_thread NAPI
	 * corresponds each hif_napi.
	 */
	if (adapter->hdd_ctx->enable_rxthread)
		napi_to_use =  &qca_napii->rx_thread_napi;
	else
		napi_to_use = &qca_napii->napi;

	status = hdd_gro_rx_bh_disable(adapter, napi_to_use, skb);
out:

	return status;
}

/**
 * hdd_rxthread_napi_gro_flush() - GRO flush callback for NAPI+Rx_Thread Rx mode
 * @data: hif NAPI context
 *
 * Return: none
 */
static void hdd_rxthread_napi_gro_flush(void *data)
{
	struct qca_napi_info *qca_napii = (struct qca_napi_info *)data;

	local_bh_disable();
	/*
	 * As we are breaking context in Rxthread mode, there is rx_thread NAPI
	 * corresponds each hif_napi.
	 */
	dp_rx_napi_gro_flush(&qca_napii->rx_thread_napi,
			     DP_RX_GRO_NORMAL_FLUSH);
	local_bh_enable();
}

/**
 * hdd_hif_napi_gro_flush() - GRO flush callback for NAPI Rx mode
 * @data: hif NAPI context
 *
 * Return: none
 */
static void hdd_hif_napi_gro_flush(void *data)
{
	struct qca_napi_info *qca_napii = (struct qca_napi_info *)data;

	local_bh_disable();
	napi_gro_flush(&qca_napii->napi, false);
	local_bh_enable();
}

#ifdef FEATURE_LRO
/**
 * hdd_qdf_lro_flush() - LRO flush wrapper
 * @data: hif NAPI context
 *
 * Return: none
 */
static void hdd_qdf_lro_flush(void *data)
{
	struct qca_napi_info *qca_napii = (struct qca_napi_info *)data;
	qdf_lro_ctx_t qdf_lro_ctx = qca_napii->lro_ctx;

	qdf_lro_flush(qdf_lro_ctx);
}
#else
static void hdd_qdf_lro_flush(void *data)
{
}
#endif

/**
 * hdd_register_rx_ol() - Register LRO/GRO rx processing callbacks
 * @hdd_ctx: pointer to hdd_ctx
 * @lithium_based_target: whether its a lithium arch based target or not
 *
 * Return: none
 */
static void hdd_register_rx_ol_cb(struct hdd_context *hdd_ctx,
				  bool lithium_based_target)
{
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	if  (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return;
	}

	hdd_ctx->en_tcp_delack_no_lro = 0;

	if (!hdd_is_lro_enabled(hdd_ctx)) {
		cdp_register_rx_offld_flush_cb(soc, hdd_qdf_lro_flush);
		hdd_ctx->receive_offload_cb = hdd_lro_rx;
		hdd_debug("LRO is enabled");
	} else if (hdd_ctx->ol_enable == CFG_GRO_ENABLED) {
		qdf_atomic_set(&hdd_ctx->dp_agg_param.rx_aggregation, 1);
		if (lithium_based_target) {
		/* no flush registration needed, it happens in DP thread */
			hdd_ctx->receive_offload_cb = hdd_gro_rx_dp_thread;
		} else {
			/*ihelium based targets */
			if (hdd_ctx->enable_rxthread)
				cdp_register_rx_offld_flush_cb(soc,
							       hdd_rxthread_napi_gro_flush);
			else
				cdp_register_rx_offld_flush_cb(soc,
							       hdd_hif_napi_gro_flush);
			hdd_ctx->receive_offload_cb = hdd_gro_rx_legacy;
		}
		hdd_debug("GRO is enabled");
	} else if (HDD_MSM_CFG(hdd_ctx->config->enable_tcp_delack)) {
		hdd_ctx->en_tcp_delack_no_lro = 1;
		hdd_debug("TCP Del ACK is enabled");
	}
}

/**
 * hdd_rx_ol_send_config() - Send RX offload configuration to FW
 * @hdd_ctx: pointer to hdd_ctx
 *
 * This function is only used for non lithium targets. Lithium based targets are
 * sending LRO config to FW in vdev attach implemented in cmn DP layer.
 *
 * Return: 0 on success, non zero on failure
 */
static int hdd_rx_ol_send_config(struct hdd_context *hdd_ctx)
{
	struct cdp_lro_hash_config lro_config = {0};
	/*
	 * This will enable flow steering and Toeplitz hash
	 * So enable it for LRO or GRO processing.
	 */
	if (cfg_get(hdd_ctx->psoc, CFG_DP_GRO) ||
	    cfg_get(hdd_ctx->psoc, CFG_DP_LRO)) {
		lro_config.lro_enable = 1;
		lro_config.tcp_flag = TCPHDR_ACK;
		lro_config.tcp_flag_mask = TCPHDR_FIN | TCPHDR_SYN |
					   TCPHDR_RST | TCPHDR_ACK |
					   TCPHDR_URG | TCPHDR_ECE |
					   TCPHDR_CWR;
	}

	get_random_bytes(lro_config.toeplitz_hash_ipv4,
			 (sizeof(lro_config.toeplitz_hash_ipv4[0]) *
			  LRO_IPV4_SEED_ARR_SZ));

	get_random_bytes(lro_config.toeplitz_hash_ipv6,
			 (sizeof(lro_config.toeplitz_hash_ipv6[0]) *
			  LRO_IPV6_SEED_ARR_SZ));

	if (wma_lro_init(&lro_config))
		return -EAGAIN;
	else
		hdd_debug("LRO Config: lro_enable: 0x%x tcp_flag 0x%x tcp_flag_mask 0x%x",
			  lro_config.lro_enable, lro_config.tcp_flag,
			  lro_config.tcp_flag_mask);

	return 0;
}

int hdd_rx_ol_init(struct hdd_context *hdd_ctx)
{
	int ret = 0;
	bool lithium_based_target = false;

	if (hdd_ctx->target_type == TARGET_TYPE_QCA6290 ||
	    hdd_ctx->target_type == TARGET_TYPE_QCA6390 ||
	    hdd_ctx->target_type == TARGET_TYPE_QCA6490 ||
	    hdd_ctx->target_type == TARGET_TYPE_QCA6750)
		lithium_based_target = true;

	hdd_resolve_rx_ol_mode(hdd_ctx);
	hdd_register_rx_ol_cb(hdd_ctx, lithium_based_target);

	if (!lithium_based_target) {
		ret = hdd_rx_ol_send_config(hdd_ctx);
		if (ret) {
			hdd_ctx->ol_enable = 0;
			hdd_err("Failed to send LRO/GRO configuration! %u", ret);
			return ret;
		}
	}

	return 0;
}

void hdd_disable_rx_ol_in_concurrency(bool disable)
{
	struct hdd_context *hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);

	if (!hdd_ctx) {
		hdd_err("hdd_ctx is NULL");
		return;
	}

	if (disable) {
		if (HDD_MSM_CFG(hdd_ctx->config->enable_tcp_delack)) {
			struct wlan_rx_tp_data rx_tp_data;

			hdd_info("Enable TCP delack as LRO disabled in concurrency");
			rx_tp_data.rx_tp_flags = TCP_DEL_ACK_IND;
			rx_tp_data.level = GET_CUR_RX_LVL(hdd_ctx);
			wlan_hdd_update_tcp_rx_param(hdd_ctx, &rx_tp_data);
			hdd_ctx->en_tcp_delack_no_lro = 1;
		}
		qdf_atomic_set(&hdd_ctx->disable_rx_ol_in_concurrency, 1);
	} else {
		if (HDD_MSM_CFG(hdd_ctx->config->enable_tcp_delack)) {
			hdd_info("Disable TCP delack as LRO is enabled");
			hdd_ctx->en_tcp_delack_no_lro = 0;
			hdd_reset_tcp_delack(hdd_ctx);
		}
		qdf_atomic_set(&hdd_ctx->disable_rx_ol_in_concurrency, 0);
	}
}

void hdd_disable_rx_ol_for_low_tput(struct hdd_context *hdd_ctx, bool disable)
{
	if (disable)
		qdf_atomic_set(&hdd_ctx->disable_rx_ol_in_low_tput, 1);
	else
		qdf_atomic_set(&hdd_ctx->disable_rx_ol_in_low_tput, 0);
}

#else /* RECEIVE_OFFLOAD */
int hdd_rx_ol_init(struct hdd_context *hdd_ctx)
{
	hdd_err("Rx_OL, LRO/GRO not supported");
	return -EPERM;
}

void hdd_disable_rx_ol_in_concurrency(bool disable)
{
}

void hdd_disable_rx_ol_for_low_tput(struct hdd_context *hdd_ctx, bool disable)
{
}
#endif /* RECEIVE_OFFLOAD */

#ifdef WLAN_FEATURE_TSF_PLUS
static inline void hdd_tsf_timestamp_rx(struct hdd_context *hdd_ctx,
					qdf_nbuf_t netbuf,
					uint64_t target_time)
{
	if (!hdd_tsf_is_rx_set(hdd_ctx))
		return;

	hdd_rx_timestamp(netbuf, target_time);
}
#else
static inline void hdd_tsf_timestamp_rx(struct hdd_context *hdd_ctx,
					qdf_nbuf_t netbuf,
					uint64_t target_time)
{
}
#endif

QDF_STATUS hdd_rx_thread_gro_flush_ind_cbk(void *adapter, int rx_ctx_id)
{
	struct hdd_adapter *hdd_adapter = adapter;
	enum dp_rx_gro_flush_code gro_flush_code = DP_RX_GRO_NORMAL_FLUSH;

	if (qdf_unlikely((!hdd_adapter) || (!hdd_adapter->hdd_ctx))) {
		hdd_err("Null params being passed");
		return QDF_STATUS_E_FAILURE;
	}

	if (hdd_adapter->runtime_disable_rx_thread)
		return QDF_STATUS_SUCCESS;

	if (hdd_is_low_tput_gro_enable(hdd_adapter->hdd_ctx)) {
		hdd_adapter->hdd_stats.tx_rx_stats.rx_gro_flush_skip++;
		gro_flush_code = DP_RX_GRO_LOW_TPUT_FLUSH;
	}

	return dp_rx_gro_flush_ind(cds_get_context(QDF_MODULE_ID_SOC),
				   rx_ctx_id, gro_flush_code);
}

QDF_STATUS hdd_rx_pkt_thread_enqueue_cbk(void *adapter,
					 qdf_nbuf_t nbuf_list)
{
	struct hdd_adapter *hdd_adapter;
	uint8_t vdev_id;
	qdf_nbuf_t head_ptr;

	if (qdf_unlikely(!adapter || !nbuf_list)) {
		hdd_err("Null params being passed");
		return QDF_STATUS_E_FAILURE;
	}

	hdd_adapter = (struct hdd_adapter *)adapter;
	if (hdd_validate_adapter(hdd_adapter))
		return QDF_STATUS_E_FAILURE;

	if (hdd_adapter->runtime_disable_rx_thread &&
	    hdd_adapter->rx_stack)
		return hdd_adapter->rx_stack(adapter, nbuf_list);

	vdev_id = hdd_adapter->vdev_id;

	if (vdev_id >= WLAN_UMAC_VDEV_ID_MAX) {
		hdd_info_rl("Vdev invalid. Dropping packets");
		qdf_nbuf_list_free(nbuf_list);
		return QDF_STATUS_E_NETDOWN;
	}

	head_ptr = nbuf_list;
	while (head_ptr) {
		qdf_nbuf_cb_update_vdev_id(head_ptr, vdev_id);
		head_ptr = qdf_nbuf_next(head_ptr);
	}

	return dp_rx_enqueue_pkt(cds_get_context(QDF_MODULE_ID_SOC), nbuf_list);
}

#ifdef CONFIG_HL_SUPPORT
QDF_STATUS hdd_rx_deliver_to_stack(struct hdd_adapter *adapter,
				   struct sk_buff *skb)
{
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;
	int status = QDF_STATUS_E_FAILURE;
	int netif_status;

	adapter->hdd_stats.tx_rx_stats.rx_non_aggregated++;
	hdd_ctx->no_rx_offload_pkt_cnt++;
	netif_status = netif_rx_ni(skb);

	if (netif_status == NET_RX_SUCCESS)
		status = QDF_STATUS_SUCCESS;

	return status;
}
#else

#if defined(WLAN_SUPPORT_RX_FISA)
/**
 * hdd_set_fisa_disallowed_for_vdev() - Set fisa disallowed bit for a vdev
 * @soc: DP soc handle
 * @vdev_id: Vdev id
 * @rx_ctx_id: rx context id
 * @val: Enable or disable
 *
 * The function sets the fisa disallowed flag for a given vdev
 *
 * Return: None
 */
static inline
void hdd_set_fisa_disallowed_for_vdev(ol_txrx_soc_handle soc, uint8_t vdev_id,
				      uint8_t rx_ctx_id, uint8_t val)
{
	dp_set_fisa_disallowed_for_vdev(soc, vdev_id, rx_ctx_id, val);
}
#else
static inline
void hdd_set_fisa_disallowed_for_vdev(ol_txrx_soc_handle soc, uint8_t vdev_id,
				      uint8_t rx_ctx_id, uint8_t val)
{
}
#endif

#ifdef WLAN_FEATURE_DYNAMIC_RX_AGGREGATION
/**
 * hdd_is_chain_list_non_empty_for_clsact_qdisc() - Check if chain_list in
 *  ingress block is non-empty for a clsact qdisc.
 * @qdisc: pointer to clsact qdisc
 *
 * Return: true if chain_list is not empty else false
 */
static bool
hdd_is_chain_list_non_empty_for_clsact_qdisc(struct Qdisc *qdisc)
{
	const struct Qdisc_class_ops *cops;
	struct tcf_block *ingress_block;

	cops = qdisc->ops->cl_ops;
	if (qdf_unlikely(!cops || !cops->tcf_block))
		return false;

	ingress_block = cops->tcf_block(qdisc, TC_H_MIN_INGRESS, NULL);
	if (qdf_unlikely(!ingress_block))
		return false;

	if (list_empty(&ingress_block->chain_list))
		return false;
	else
		return true;
}

/**
 * hdd_rx_check_qdisc_for_adapter() - Check if any ingress qdisc is configured
 *  for given adapter
 * @adapter: pointer to HDD adapter context
 * @rx_ctx_id: Rx context id
 *
 * The function checks if ingress qdisc is registered for a given
 * net device.
 *
 * Return: None
 */
static void
hdd_rx_check_qdisc_for_adapter(struct hdd_adapter *adapter, uint8_t rx_ctx_id)
{
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct netdev_queue *ingress_q;
	struct Qdisc *ingress_qdisc;

	if (qdf_unlikely(!soc))
		return;

	if (!adapter->dev->ingress_queue)
		goto reset_wl;

	rcu_read_lock();

	ingress_q = rcu_dereference(adapter->dev->ingress_queue);
	if (qdf_unlikely(!ingress_q))
		goto reset;

	ingress_qdisc = rcu_dereference(ingress_q->qdisc);
	if (qdf_unlikely(!ingress_qdisc))
		goto reset;

	if (!(qdf_str_eq(ingress_qdisc->ops->id, "ingress") ||
	      (qdf_str_eq(ingress_qdisc->ops->id, "clsact") &&
	       hdd_is_chain_list_non_empty_for_clsact_qdisc(ingress_qdisc))))
		goto reset;

	rcu_read_unlock();

	if (qdf_likely(adapter->gro_disallowed[rx_ctx_id]))
		return;

	hdd_debug("ingress qdisc/filter configured disable GRO");
	adapter->gro_disallowed[rx_ctx_id] = 1;
	hdd_set_fisa_disallowed_for_vdev(soc, adapter->vdev_id, rx_ctx_id, 1);

	return;

reset:
	rcu_read_unlock();

reset_wl:
	if (qdf_unlikely(adapter->gro_disallowed[rx_ctx_id])) {
		hdd_debug("ingress qdisc/filter removed enable GRO");
		hdd_set_fisa_disallowed_for_vdev(soc, adapter->vdev_id,
						 rx_ctx_id, 0);
		adapter->gro_disallowed[rx_ctx_id] = 0;
		adapter->gro_flushed[rx_ctx_id] = 0;
	}
}

QDF_STATUS hdd_rx_deliver_to_stack(struct hdd_adapter *adapter,
				   struct sk_buff *skb)
{
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;
	int status = QDF_STATUS_E_FAILURE;
	int netif_status;
	bool skb_receive_offload_ok = false;
	uint8_t rx_ctx_id = QDF_NBUF_CB_RX_CTX_ID(skb);

	/* rx_ctx_id is already verified for out-of-range */
	hdd_rx_check_qdisc_for_adapter(adapter, rx_ctx_id);

	if (QDF_NBUF_CB_RX_TCP_PROTO(skb) &&
	    !QDF_NBUF_CB_RX_PEER_CACHED_FRM(skb))
		skb_receive_offload_ok = true;

	if (skb_receive_offload_ok && hdd_ctx->receive_offload_cb &&
	    !hdd_ctx->dp_agg_param.gro_force_flush[rx_ctx_id] &&
	    !adapter->gro_flushed[rx_ctx_id] &&
	    !adapter->runtime_disable_rx_thread) {
		status = hdd_ctx->receive_offload_cb(adapter, skb);

		if (QDF_IS_STATUS_SUCCESS(status)) {
			adapter->hdd_stats.tx_rx_stats.rx_aggregated++;
			return status;
		}

		if (status == QDF_STATUS_E_GRO_DROP) {
			adapter->hdd_stats.tx_rx_stats.rx_gro_dropped++;
			return status;
		}
	}

	/*
	 * The below case handles the scenario when rx_aggregation is
	 * re-enabled dynamically, in which case gro_force_flush needs
	 * to be reset to 0 to allow GRO.
	 */
	if (qdf_atomic_read(&hdd_ctx->dp_agg_param.rx_aggregation) &&
	    hdd_ctx->dp_agg_param.gro_force_flush[rx_ctx_id])
		hdd_ctx->dp_agg_param.gro_force_flush[rx_ctx_id] = 0;

	adapter->hdd_stats.tx_rx_stats.rx_non_aggregated++;

	/* Account for GRO/LRO ineligible packets, mostly UDP */
	if (qdf_nbuf_get_gso_segs(skb) == 0)
		hdd_ctx->no_rx_offload_pkt_cnt++;

	if (qdf_likely((hdd_ctx->enable_dp_rx_threads ||
		        hdd_ctx->enable_rxthread) &&
		        !adapter->runtime_disable_rx_thread)) {
		local_bh_disable();
		netif_status = netif_receive_skb(skb);
		local_bh_enable();
	} else if (qdf_unlikely(QDF_NBUF_CB_RX_PEER_CACHED_FRM(skb))) {
		/*
		 * Frames before peer is registered to avoid contention with
		 * NAPI softirq.
		 * Refer fix:
		 * qcacld-3.0: Do netif_rx_ni() for frames received before
		 * peer assoc
		 */
		netif_status = netif_rx_ni(skb);
	} else { /* NAPI Context */
		netif_status = netif_receive_skb(skb);
	}

	if (netif_status == NET_RX_SUCCESS)
		status = QDF_STATUS_SUCCESS;

	return status;
}

#else /* WLAN_FEATURE_DYNAMIC_RX_AGGREGATION */

QDF_STATUS hdd_rx_deliver_to_stack(struct hdd_adapter *adapter,
				   struct sk_buff *skb)
{
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;
	int status = QDF_STATUS_E_FAILURE;
	int netif_status;
	bool skb_receive_offload_ok = false;

	if (QDF_NBUF_CB_RX_TCP_PROTO(skb) &&
	    !QDF_NBUF_CB_RX_PEER_CACHED_FRM(skb))
		skb_receive_offload_ok = true;

	if (skb_receive_offload_ok && hdd_ctx->receive_offload_cb) {
		status = hdd_ctx->receive_offload_cb(adapter, skb);

		if (QDF_IS_STATUS_SUCCESS(status)) {
			adapter->hdd_stats.tx_rx_stats.rx_aggregated++;
			return status;
		}

		if (status == QDF_STATUS_E_GRO_DROP) {
			adapter->hdd_stats.tx_rx_stats.rx_gro_dropped++;
			return status;
		}
	}

	adapter->hdd_stats.tx_rx_stats.rx_non_aggregated++;

	/* Account for GRO/LRO ineligible packets, mostly UDP */
	if (qdf_nbuf_get_gso_segs(skb) == 0)
		hdd_ctx->no_rx_offload_pkt_cnt++;

	if (qdf_likely((hdd_ctx->enable_dp_rx_threads ||
		        hdd_ctx->enable_rxthread) &&
		        !adapter->runtime_disable_rx_thread)) {
		local_bh_disable();
		netif_status = netif_receive_skb(skb);
		local_bh_enable();
	} else if (qdf_unlikely(QDF_NBUF_CB_RX_PEER_CACHED_FRM(skb))) {
		/*
		 * Frames before peer is registered to avoid contention with
		 * NAPI softirq.
		 * Refer fix:
		 * qcacld-3.0: Do netif_rx_ni() for frames received before
		 * peer assoc
		 */
		netif_status = netif_rx_ni(skb);
	} else { /* NAPI Context */
		netif_status = netif_receive_skb(skb);
	}

	if (netif_status == NET_RX_SUCCESS)
		status = QDF_STATUS_SUCCESS;

	return status;
}
#endif /* WLAN_FEATURE_DYNAMIC_RX_AGGREGATION */
#endif

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(4, 6, 0))
static bool hdd_is_gratuitous_arp_unsolicited_na(struct sk_buff *skb)
{
	return false;
}
#else
static bool hdd_is_gratuitous_arp_unsolicited_na(struct sk_buff *skb)
{
	return cfg80211_is_gratuitous_arp_unsolicited_na(skb);
}
#endif

QDF_STATUS hdd_rx_flush_packet_cbk(void *adapter_context, uint8_t vdev_id)
{
	struct hdd_adapter *adapter;
	struct hdd_context *hdd_ctx;
	ol_txrx_soc_handle soc = cds_get_context(QDF_MODULE_ID_SOC);

	if (qdf_unlikely(!soc))
		return QDF_STATUS_E_FAILURE;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (unlikely(!hdd_ctx)) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			  "%s: HDD context is Null", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	adapter = hdd_adapter_get_by_reference(hdd_ctx, adapter_context);
	if (!adapter) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			  "%s: Adapter reference is Null", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	/* do fisa flush for this vdev */
	if (hdd_ctx->config->fisa_enable)
		hdd_rx_fisa_flush_by_vdev_id(soc, vdev_id);

	if (hdd_ctx->enable_dp_rx_threads)
		dp_txrx_flush_pkts_by_vdev_id(soc, vdev_id);

	hdd_adapter_put(adapter);

	return QDF_STATUS_SUCCESS;
}

#if defined(WLAN_SUPPORT_RX_FISA)
QDF_STATUS hdd_rx_fisa_cbk(void *dp_soc, void *dp_vdev, qdf_nbuf_t nbuf_list)
{
	return dp_fisa_rx((struct dp_soc *)dp_soc, (struct dp_vdev *)dp_vdev,
			  nbuf_list);
}

QDF_STATUS hdd_rx_fisa_flush_by_ctx_id(void *dp_soc, int ring_num)
{
	return dp_rx_fisa_flush_by_ctx_id((struct dp_soc *)dp_soc, ring_num);
}

QDF_STATUS hdd_rx_fisa_flush_by_vdev_id(void *dp_soc, uint8_t vdev_id)
{
	return dp_rx_fisa_flush_by_vdev_id((struct dp_soc *)dp_soc, vdev_id);
}
#endif

QDF_STATUS hdd_rx_packet_cbk(void *adapter_context,
			     qdf_nbuf_t rxBuf)
{
	struct hdd_adapter *adapter = NULL;
	struct hdd_context *hdd_ctx = NULL;
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	struct sk_buff *skb = NULL;
	struct sk_buff *next = NULL;
	struct hdd_station_ctx *sta_ctx = NULL;
	unsigned int cpu_index;
	struct qdf_mac_addr *mac_addr, *dest_mac_addr;
	bool wake_lock = false;
	uint8_t pkt_type = 0;
	bool track_arp = false;
	struct wlan_objmgr_vdev *vdev;
	enum qdf_proto_subtype subtype = QDF_PROTO_INVALID;
	bool is_eapol;
	bool is_dhcp;

	/* Sanity check on inputs */
	if (unlikely((!adapter_context) || (!rxBuf))) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			  "%s: Null params being passed", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	adapter = (struct hdd_adapter *)adapter_context;
	if (unlikely(WLAN_HDD_ADAPTER_MAGIC != adapter->magic)) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			  "Magic cookie(%x) for adapter sanity verification is invalid",
			  adapter->magic);
		return QDF_STATUS_E_FAILURE;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (unlikely(!hdd_ctx)) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			  "%s: HDD context is Null", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	cpu_index = wlan_hdd_get_cpu();

	next = (struct sk_buff *)rxBuf;

	while (next) {
		skb = next;
		next = skb->next;
		skb->next = NULL;
		is_eapol = false;
		is_dhcp = false;

		if (qdf_nbuf_is_ipv4_arp_pkt(skb)) {
			if (qdf_nbuf_data_is_arp_rsp(skb) &&
				(adapter->track_arp_ip ==
			     qdf_nbuf_get_arp_src_ip(skb))) {
				++adapter->hdd_stats.hdd_arp_stats.
							rx_arp_rsp_count;
				QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
						QDF_TRACE_LEVEL_DEBUG,
						"%s: ARP packet received",
						__func__);
				track_arp = true;
			}
		} else if (qdf_nbuf_is_ipv4_eapol_pkt(skb)) {
			subtype = qdf_nbuf_get_eapol_subtype(skb);
			if (subtype == QDF_PROTO_EAPOL_M1) {
				++adapter->hdd_stats.hdd_eapol_stats.
						eapol_m1_count;
				is_eapol = true;
			} else if (subtype == QDF_PROTO_EAPOL_M3) {
				++adapter->hdd_stats.hdd_eapol_stats.
						eapol_m3_count;
				is_eapol = true;
			}
		} else if (qdf_nbuf_is_ipv4_dhcp_pkt(skb)) {
			subtype = qdf_nbuf_get_dhcp_subtype(skb);
			if (subtype == QDF_PROTO_DHCP_OFFER) {
				++adapter->hdd_stats.hdd_dhcp_stats.
						dhcp_off_count;
				is_dhcp = true;
			} else if (subtype == QDF_PROTO_DHCP_ACK) {
				++adapter->hdd_stats.hdd_dhcp_stats.
						dhcp_ack_count;
				is_dhcp = true;
			}
		}
		/* track connectivity stats */
		if (adapter->pkt_type_bitmap)
			hdd_tx_rx_collect_connectivity_stats_info(skb, adapter,
						PKT_TYPE_RSP, &pkt_type);

		sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
		if ((sta_ctx->conn_info.proxy_arp_service) &&
		    hdd_is_gratuitous_arp_unsolicited_na(skb)) {
			qdf_atomic_inc(&adapter->hdd_stats.tx_rx_stats.
						rx_usolict_arp_n_mcast_drp);
			/* Remove SKB from internal tracking table before
			 * submitting it to stack.
			 */
			qdf_nbuf_free(skb);
			continue;
		}

		hdd_event_eapol_log(skb, QDF_RX);
		qdf_dp_trace_log_pkt(adapter->vdev_id, skb, QDF_RX,
				     QDF_TRACE_DEFAULT_PDEV_ID);

		DPTRACE(qdf_dp_trace(skb,
			QDF_DP_TRACE_RX_HDD_PACKET_PTR_RECORD,
			QDF_TRACE_DEFAULT_PDEV_ID,
			qdf_nbuf_data_addr(skb),
			sizeof(qdf_nbuf_data(skb)), QDF_RX));

		DPTRACE(qdf_dp_trace_data_pkt(skb, QDF_TRACE_DEFAULT_PDEV_ID,
			QDF_DP_TRACE_RX_PACKET_RECORD,
			0, QDF_RX));

		dest_mac_addr = (struct qdf_mac_addr *)(skb->data);
		mac_addr = (struct qdf_mac_addr *)(skb->data+QDF_MAC_ADDR_SIZE);

		vdev = hdd_objmgr_get_vdev(adapter);
		if (vdev) {
			ucfg_tdls_update_rx_pkt_cnt(vdev, mac_addr,
						    dest_mac_addr);
			hdd_objmgr_put_vdev(vdev);
		}

		skb->dev = adapter->dev;
		skb->protocol = eth_type_trans(skb, skb->dev);
		++adapter->hdd_stats.tx_rx_stats.rx_packets[cpu_index];
		++adapter->stats.rx_packets;
		/* count aggregated RX frame into stats */
		adapter->stats.rx_packets += qdf_nbuf_get_gso_segs(skb);
		adapter->stats.rx_bytes += skb->len;

		/* Incr GW Rx count for NUD tracking based on GW mac addr */
		hdd_nud_incr_gw_rx_pkt_cnt(adapter, mac_addr);

		/* Check & drop replayed mcast packets (for IPV6) */
		if (hdd_ctx->config->multicast_replay_filter &&
				hdd_is_mcast_replay(skb)) {
			qdf_atomic_inc(&adapter->hdd_stats.tx_rx_stats.
						rx_usolict_arp_n_mcast_drp);
			qdf_nbuf_free(skb);
			continue;
		}

		/* hold configurable wakelock for unicast traffic */
		if (!hdd_is_current_high_throughput(hdd_ctx) &&
		    hdd_ctx->config->rx_wakelock_timeout &&
		    sta_ctx->conn_info.is_authenticated)
			wake_lock = hdd_is_rx_wake_lock_needed(skb);

		if (wake_lock) {
			cds_host_diag_log_work(&hdd_ctx->rx_wake_lock,
						   hdd_ctx->config->rx_wakelock_timeout,
						   WIFI_POWER_EVENT_WAKELOCK_HOLD_RX);
			qdf_wake_lock_timeout_acquire(&hdd_ctx->rx_wake_lock,
							  hdd_ctx->config->
								  rx_wakelock_timeout);
		}

		/* Remove SKB from internal tracking table before submitting
		 * it to stack
		 */
		qdf_net_buf_debug_release_skb(skb);

		hdd_tsf_timestamp_rx(hdd_ctx, skb, ktime_to_us(skb->tstamp));

		qdf_status = hdd_rx_deliver_to_stack(adapter, skb);

		if (QDF_IS_STATUS_SUCCESS(qdf_status)) {
			++adapter->hdd_stats.tx_rx_stats.
						rx_delivered[cpu_index];
			if (track_arp)
				++adapter->hdd_stats.hdd_arp_stats.
							rx_delivered;
			if (is_eapol)
				++adapter->hdd_stats.hdd_eapol_stats.
				     rx_delivered[subtype - QDF_PROTO_EAPOL_M1];
			else if (is_dhcp)
				++adapter->hdd_stats.hdd_dhcp_stats.
				rx_delivered[subtype - QDF_PROTO_DHCP_DISCOVER];

			/* track connectivity stats */
			if (adapter->pkt_type_bitmap)
				hdd_tx_rx_collect_connectivity_stats_info(
					skb, adapter,
					PKT_TYPE_RX_DELIVERED, &pkt_type);
		} else {
			++adapter->hdd_stats.tx_rx_stats.rx_refused[cpu_index];
			if (track_arp)
				++adapter->hdd_stats.hdd_arp_stats.rx_refused;

			if (is_eapol)
				++adapter->hdd_stats.hdd_eapol_stats.
				       rx_refused[subtype - QDF_PROTO_EAPOL_M1];
			else if (is_dhcp)
				++adapter->hdd_stats.hdd_dhcp_stats.
				  rx_refused[subtype - QDF_PROTO_DHCP_DISCOVER];

			/* track connectivity stats */
			if (adapter->pkt_type_bitmap)
				hdd_tx_rx_collect_connectivity_stats_info(
					skb, adapter,
					PKT_TYPE_RX_REFUSED, &pkt_type);
		}
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_reason_type_to_string() - return string conversion of reason type
 * @reason: reason type
 *
 * This utility function helps log string conversion of reason type.
 *
 * Return: string conversion of device mode, if match found;
 *        "Unknown" otherwise.
 */
const char *hdd_reason_type_to_string(enum netif_reason_type reason)
{
	switch (reason) {
	CASE_RETURN_STRING(WLAN_CONTROL_PATH);
	CASE_RETURN_STRING(WLAN_DATA_FLOW_CONTROL);
	CASE_RETURN_STRING(WLAN_FW_PAUSE);
	CASE_RETURN_STRING(WLAN_TX_ABORT);
	CASE_RETURN_STRING(WLAN_VDEV_STOP);
	CASE_RETURN_STRING(WLAN_PEER_UNAUTHORISED);
	CASE_RETURN_STRING(WLAN_THERMAL_MITIGATION);
	CASE_RETURN_STRING(WLAN_DATA_FLOW_CONTROL_PRIORITY);
	default:
		return "Invalid";
	}
}

/**
 * hdd_action_type_to_string() - return string conversion of action type
 * @action: action type
 *
 * This utility function helps log string conversion of action_type.
 *
 * Return: string conversion of device mode, if match found;
 *        "Unknown" otherwise.
 */
const char *hdd_action_type_to_string(enum netif_action_type action)
{

	switch (action) {
	CASE_RETURN_STRING(WLAN_STOP_ALL_NETIF_QUEUE);
	CASE_RETURN_STRING(WLAN_START_ALL_NETIF_QUEUE);
	CASE_RETURN_STRING(WLAN_WAKE_ALL_NETIF_QUEUE);
	CASE_RETURN_STRING(WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER);
	CASE_RETURN_STRING(WLAN_START_ALL_NETIF_QUEUE_N_CARRIER);
	CASE_RETURN_STRING(WLAN_NETIF_TX_DISABLE);
	CASE_RETURN_STRING(WLAN_NETIF_TX_DISABLE_N_CARRIER);
	CASE_RETURN_STRING(WLAN_NETIF_CARRIER_ON);
	CASE_RETURN_STRING(WLAN_NETIF_CARRIER_OFF);
	CASE_RETURN_STRING(WLAN_NETIF_PRIORITY_QUEUE_ON);
	CASE_RETURN_STRING(WLAN_NETIF_PRIORITY_QUEUE_OFF);
	CASE_RETURN_STRING(WLAN_NETIF_VO_QUEUE_ON);
	CASE_RETURN_STRING(WLAN_NETIF_VO_QUEUE_OFF);
	CASE_RETURN_STRING(WLAN_NETIF_VI_QUEUE_ON);
	CASE_RETURN_STRING(WLAN_NETIF_VI_QUEUE_OFF);
	CASE_RETURN_STRING(WLAN_NETIF_BE_BK_QUEUE_ON);
	CASE_RETURN_STRING(WLAN_NETIF_BE_BK_QUEUE_OFF);
	CASE_RETURN_STRING(WLAN_WAKE_NON_PRIORITY_QUEUE);
	CASE_RETURN_STRING(WLAN_STOP_NON_PRIORITY_QUEUE);
	default:
		return "Invalid";
	}
}

/**
 * wlan_hdd_update_queue_oper_stats - update queue operation statistics
 * @adapter: adapter handle
 * @action: action type
 * @reason: reason type
 */
static void wlan_hdd_update_queue_oper_stats(struct hdd_adapter *adapter,
	enum netif_action_type action, enum netif_reason_type reason)
{
	switch (action) {
	case WLAN_STOP_ALL_NETIF_QUEUE:
	case WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER:
	case WLAN_NETIF_BE_BK_QUEUE_OFF:
	case WLAN_NETIF_VI_QUEUE_OFF:
	case WLAN_NETIF_VO_QUEUE_OFF:
	case WLAN_NETIF_PRIORITY_QUEUE_OFF:
	case WLAN_STOP_NON_PRIORITY_QUEUE:
		adapter->queue_oper_stats[reason].pause_count++;
		break;
	case WLAN_START_ALL_NETIF_QUEUE:
	case WLAN_WAKE_ALL_NETIF_QUEUE:
	case WLAN_START_ALL_NETIF_QUEUE_N_CARRIER:
	case WLAN_NETIF_BE_BK_QUEUE_ON:
	case WLAN_NETIF_VI_QUEUE_ON:
	case WLAN_NETIF_VO_QUEUE_ON:
	case WLAN_NETIF_PRIORITY_QUEUE_ON:
	case WLAN_WAKE_NON_PRIORITY_QUEUE:
		adapter->queue_oper_stats[reason].unpause_count++;
		break;
	default:
		break;
	}
}

/**
 * hdd_netdev_queue_is_locked()
 * @txq: net device tx queue
 *
 * For SMP system, always return false and we could safely rely on
 * __netif_tx_trylock().
 *
 * Return: true locked; false not locked
 */
#ifdef QCA_CONFIG_SMP
static inline bool hdd_netdev_queue_is_locked(struct netdev_queue *txq)
{
	return false;
}
#else
static inline bool hdd_netdev_queue_is_locked(struct netdev_queue *txq)
{
	return txq->xmit_lock_owner != -1;
}
#endif

/**
 * wlan_hdd_update_txq_timestamp() - update txq timestamp
 * @dev: net device
 *
 * Return: none
 */
static void wlan_hdd_update_txq_timestamp(struct net_device *dev)
{
	struct netdev_queue *txq;
	int i;

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		txq = netdev_get_tx_queue(dev, i);

		/*
		 * On UP system, kernel will trigger watchdog bite if spinlock
		 * recursion is detected. Unfortunately recursion is possible
		 * when it is called in dev_queue_xmit() context, where stack
		 * grabs the lock before calling driver's ndo_start_xmit
		 * callback.
		 */
		if (!hdd_netdev_queue_is_locked(txq)) {
			if (__netif_tx_trylock(txq)) {
				txq_trans_update(txq);
				__netif_tx_unlock(txq);
			}
		}
	}
}

/**
 * wlan_hdd_update_unpause_time() - update unpause time
 * @adapter: adapter handle
 *
 * Return: none
 */
static void wlan_hdd_update_unpause_time(struct hdd_adapter *adapter)
{
	qdf_time_t curr_time = qdf_system_ticks();

	adapter->total_unpause_time += curr_time - adapter->last_time;
	adapter->last_time = curr_time;
}

/**
 * wlan_hdd_update_pause_time() - update pause time
 * @adapter: adapter handle
 *
 * Return: none
 */
static void wlan_hdd_update_pause_time(struct hdd_adapter *adapter,
	 uint32_t temp_map)
{
	qdf_time_t curr_time = qdf_system_ticks();
	uint8_t i;
	qdf_time_t pause_time;

	pause_time = curr_time - adapter->last_time;
	adapter->total_pause_time += pause_time;
	adapter->last_time = curr_time;

	for (i = 0; i < WLAN_REASON_TYPE_MAX; i++) {
		if (temp_map & (1 << i)) {
			adapter->queue_oper_stats[i].total_pause_time +=
								 pause_time;
			break;
		}
	}

}

uint32_t
wlan_hdd_dump_queue_history_state(struct hdd_netif_queue_history *queue_history,
				  char *buf, uint32_t size)
{
	unsigned int i;
	unsigned int index = 0;

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		index += qdf_scnprintf(buf + index,
				       size - index,
				       "%u:0x%lx ",
				       i, queue_history->tx_q_state[i]);
	}

	return index;
}

/**
 * wlan_hdd_update_queue_history_state() - Save a copy of dev TX queues state
 * @adapter: adapter handle
 *
 * Save netdev TX queues state into adapter queue history.
 *
 * Return: None
 */
static void
wlan_hdd_update_queue_history_state(struct net_device *dev,
				    struct hdd_netif_queue_history *q_hist)
{
	unsigned int i = 0;
	uint32_t num_tx_queues = 0;
	struct netdev_queue *txq = NULL;

	num_tx_queues = qdf_min(dev->num_tx_queues, (uint32_t)NUM_TX_QUEUES);

	for (i = 0; i < num_tx_queues; i++) {
		txq = netdev_get_tx_queue(dev, i);
		q_hist->tx_q_state[i] = txq->state;
	}
}

/**
 * wlan_hdd_stop_non_priority_queue() - stop non prority queues
 * @adapter: adapter handle
 *
 * Return: None
 */
static inline void wlan_hdd_stop_non_priority_queue(struct hdd_adapter *adapter)
{
	netif_stop_subqueue(adapter->dev, HDD_LINUX_AC_VO);
	netif_stop_subqueue(adapter->dev, HDD_LINUX_AC_VI);
	netif_stop_subqueue(adapter->dev, HDD_LINUX_AC_BE);
	netif_stop_subqueue(adapter->dev, HDD_LINUX_AC_BK);
}

/**
 * wlan_hdd_wake_non_priority_queue() - wake non prority queues
 * @adapter: adapter handle
 *
 * Return: None
 */
static inline void wlan_hdd_wake_non_priority_queue(struct hdd_adapter *adapter)
{
	netif_wake_subqueue(adapter->dev, HDD_LINUX_AC_VO);
	netif_wake_subqueue(adapter->dev, HDD_LINUX_AC_VI);
	netif_wake_subqueue(adapter->dev, HDD_LINUX_AC_BE);
	netif_wake_subqueue(adapter->dev, HDD_LINUX_AC_BK);
}

/**
 * wlan_hdd_netif_queue_control() - Use for netif_queue related actions
 * @adapter: adapter handle
 * @action: action type
 * @reason: reason type
 *
 * This is single function which is used for netif_queue related
 * actions like start/stop of network queues and on/off carrier
 * option.
 *
 * Return: None
 */
void wlan_hdd_netif_queue_control(struct hdd_adapter *adapter,
	enum netif_action_type action, enum netif_reason_type reason)
{
	uint32_t temp_map;
	uint8_t index;
	struct hdd_netif_queue_history *txq_hist_ptr;

	if ((!adapter) || (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) ||
		 (!adapter->dev)) {
		hdd_err("adapter is invalid");
		return;
	}

	switch (action) {

	case WLAN_NETIF_CARRIER_ON:
		netif_carrier_on(adapter->dev);
		break;

	case WLAN_NETIF_CARRIER_OFF:
		netif_carrier_off(adapter->dev);
		break;

	case WLAN_STOP_ALL_NETIF_QUEUE:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			netif_tx_stop_all_queues(adapter->dev);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_STOP_NON_PRIORITY_QUEUE:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			wlan_hdd_stop_non_priority_queue(adapter);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_PRIORITY_QUEUE_ON:
		spin_lock_bh(&adapter->pause_map_lock);
		if (reason == WLAN_DATA_FLOW_CTRL_PRI) {
			temp_map = adapter->subqueue_pause_map;
			adapter->subqueue_pause_map &= ~(1 << reason);
		} else {
			temp_map = adapter->pause_map;
			adapter->pause_map &= ~(1 << reason);
		}
		if (!adapter->pause_map) {
			netif_wake_subqueue(adapter->dev, HDD_LINUX_AC_HI_PRIO);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_PRIORITY_QUEUE_OFF:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			netif_stop_subqueue(adapter->dev, HDD_LINUX_AC_HI_PRIO);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		if (reason == WLAN_DATA_FLOW_CTRL_PRI)
			adapter->subqueue_pause_map |= (1 << reason);
		else
			adapter->pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_BE_BK_QUEUE_OFF:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			netif_stop_subqueue(adapter->dev, HDD_LINUX_AC_BK);
			netif_stop_subqueue(adapter->dev, HDD_LINUX_AC_BE);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->subqueue_pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_BE_BK_QUEUE_ON:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->subqueue_pause_map;
		adapter->subqueue_pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			netif_wake_subqueue(adapter->dev, HDD_LINUX_AC_BK);
			netif_wake_subqueue(adapter->dev, HDD_LINUX_AC_BE);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_VI_QUEUE_OFF:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			netif_stop_subqueue(adapter->dev, HDD_LINUX_AC_VI);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->subqueue_pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_VI_QUEUE_ON:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->subqueue_pause_map;
		adapter->subqueue_pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			netif_wake_subqueue(adapter->dev, HDD_LINUX_AC_VI);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_VO_QUEUE_OFF:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			netif_stop_subqueue(adapter->dev, HDD_LINUX_AC_VO);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->subqueue_pause_map |= (1 << reason);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_VO_QUEUE_ON:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->subqueue_pause_map;
		adapter->subqueue_pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			netif_wake_subqueue(adapter->dev, HDD_LINUX_AC_VO);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_START_ALL_NETIF_QUEUE:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->pause_map;
		adapter->pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			netif_tx_start_all_queues(adapter->dev);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_WAKE_ALL_NETIF_QUEUE:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->pause_map;
		adapter->pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			netif_tx_wake_all_queues(adapter->dev);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_WAKE_NON_PRIORITY_QUEUE:
		spin_lock_bh(&adapter->pause_map_lock);
		temp_map = adapter->pause_map;
		adapter->pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			wlan_hdd_wake_non_priority_queue(adapter);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_STOP_ALL_NETIF_QUEUE_N_CARRIER:
		spin_lock_bh(&adapter->pause_map_lock);
		if (!adapter->pause_map) {
			netif_tx_stop_all_queues(adapter->dev);
			wlan_hdd_update_txq_timestamp(adapter->dev);
			wlan_hdd_update_unpause_time(adapter);
		}
		adapter->pause_map |= (1 << reason);
		netif_carrier_off(adapter->dev);
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_START_ALL_NETIF_QUEUE_N_CARRIER:
		spin_lock_bh(&adapter->pause_map_lock);
		netif_carrier_on(adapter->dev);
		temp_map = adapter->pause_map;
		adapter->pause_map &= ~(1 << reason);
		if (!adapter->pause_map) {
			netif_tx_start_all_queues(adapter->dev);
			wlan_hdd_update_pause_time(adapter, temp_map);
		}
		spin_unlock_bh(&adapter->pause_map_lock);
		break;

	case WLAN_NETIF_ACTION_TYPE_NONE:
		break;

	default:
		hdd_err("unsupported action %d", action);
	}

	spin_lock_bh(&adapter->pause_map_lock);
	if (adapter->pause_map & (1 << WLAN_PEER_UNAUTHORISED))
		wlan_hdd_process_peer_unauthorised_pause(adapter);

	index = adapter->history_index++;
	if (adapter->history_index == WLAN_HDD_MAX_HISTORY_ENTRY)
		adapter->history_index = 0;
	spin_unlock_bh(&adapter->pause_map_lock);

	wlan_hdd_update_queue_oper_stats(adapter, action, reason);

	adapter->queue_oper_history[index].time = qdf_system_ticks();
	adapter->queue_oper_history[index].netif_action = action;
	adapter->queue_oper_history[index].netif_reason = reason;
	if (reason >= WLAN_DATA_FLOW_CTRL_BE_BK)
		adapter->queue_oper_history[index].pause_map =
			adapter->subqueue_pause_map;
	else
		adapter->queue_oper_history[index].pause_map =
			adapter->pause_map;

	txq_hist_ptr = &adapter->queue_oper_history[index];

	wlan_hdd_update_queue_history_state(adapter->dev, txq_hist_ptr);
}

void hdd_print_netdev_txq_status(struct net_device *dev)
{
	unsigned int i;

	if (!dev)
		return;

	for (i = 0; i < dev->num_tx_queues; i++) {
		struct netdev_queue *txq = netdev_get_tx_queue(dev, i);

			hdd_debug("netdev tx queue[%u] state:0x%lx",
				  i, txq->state);
	}
}

#ifdef FEATURE_MONITOR_MODE_SUPPORT
/**
 * hdd_set_mon_rx_cb() - Set Monitor mode Rx callback
 * @dev:        Pointer to net_device structure
 *
 * Return: 0 for success; non-zero for failure
 */
int hdd_set_mon_rx_cb(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx =  WLAN_HDD_GET_CTX(adapter);
	int ret;
	QDF_STATUS qdf_status;
	struct ol_txrx_desc_type sta_desc = {0};
	struct ol_txrx_ops txrx_ops;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);

	WLAN_ADDR_COPY(sta_desc.peer_addr.bytes, adapter->mac_addr.bytes);
	qdf_mem_zero(&txrx_ops, sizeof(txrx_ops));
	txrx_ops.rx.rx = hdd_mon_rx_packet_cbk;
	hdd_monitor_set_rx_monitor_cb(&txrx_ops, hdd_rx_monitor_callback);
	cdp_vdev_register(soc, adapter->vdev_id,
			  (ol_osif_vdev_handle)adapter,
			  &txrx_ops);
	/* peer is created wma_vdev_attach->wma_create_peer */
	qdf_status = cdp_peer_register(soc, OL_TXRX_PDEV_ID, &sta_desc);
	if (QDF_STATUS_SUCCESS != qdf_status) {
		hdd_err("cdp_peer_register() failed to register. Status= %d [0x%08X]",
			qdf_status, qdf_status);
		goto exit;
	}

	qdf_status = sme_create_mon_session(hdd_ctx->mac_handle,
					    adapter->mac_addr.bytes,
					    adapter->vdev_id);
	if (QDF_STATUS_SUCCESS != qdf_status) {
		hdd_err("sme_create_mon_session() failed to register. Status= %d [0x%08X]",
			qdf_status, qdf_status);
	}

exit:
	ret = qdf_status_to_os_return(qdf_status);
	return ret;
}
#endif

/**
 * hdd_send_rps_ind() - send rps indication to daemon
 * @adapter: adapter context
 *
 * If RPS feature enabled by INI, send RPS enable indication to daemon
 * Indication contents is the name of interface to find correct sysfs node
 * Should send all available interfaces
 *
 * Return: none
 */
void hdd_send_rps_ind(struct hdd_adapter *adapter)
{
	int i;
	uint8_t cpu_map_list_len = 0;
	struct hdd_context *hdd_ctxt = NULL;
	struct wlan_rps_data rps_data;
	struct cds_config_info *cds_cfg;

	cds_cfg = cds_get_ini_config();

	if (!adapter) {
		hdd_err("adapter is NULL");
		return;
	}

	if (!cds_cfg) {
		hdd_err("cds_cfg is NULL");
		return;
	}

	hdd_ctxt = WLAN_HDD_GET_CTX(adapter);
	rps_data.num_queues = NUM_TX_QUEUES;

	hdd_debug("cpu_map_list '%s'", hdd_ctxt->config->cpu_map_list);

	/* in case no cpu map list is provided, simply return */
	if (!strlen(hdd_ctxt->config->cpu_map_list)) {
		hdd_debug("no cpu map list found");
		goto err;
	}

	if (QDF_STATUS_SUCCESS !=
		hdd_hex_string_to_u16_array(hdd_ctxt->config->cpu_map_list,
				rps_data.cpu_map_list,
				&cpu_map_list_len,
				WLAN_SVC_IFACE_NUM_QUEUES)) {
		hdd_err("invalid cpu map list");
		goto err;
	}

	rps_data.num_queues =
		(cpu_map_list_len < rps_data.num_queues) ?
				cpu_map_list_len : rps_data.num_queues;

	for (i = 0; i < rps_data.num_queues; i++) {
		hdd_debug("cpu_map_list[%d] = 0x%x",
			  i, rps_data.cpu_map_list[i]);
	}

	strlcpy(rps_data.ifname, adapter->dev->name,
			sizeof(rps_data.ifname));
	wlan_hdd_send_svc_nlink_msg(hdd_ctxt->radio_index,
				WLAN_SVC_RPS_ENABLE_IND,
				&rps_data, sizeof(rps_data));

	cds_cfg->rps_enabled = true;

	return;

err:
	hdd_debug("Wrong RPS configuration. enabling rx_thread");
	cds_cfg->rps_enabled = false;
}

/**
 * hdd_send_rps_disable_ind() - send rps disable indication to daemon
 * @adapter: adapter context
 *
 * Return: none
 */
void hdd_send_rps_disable_ind(struct hdd_adapter *adapter)
{
	struct hdd_context *hdd_ctxt = NULL;
	struct wlan_rps_data rps_data;
	struct cds_config_info *cds_cfg;

	cds_cfg = cds_get_ini_config();

	if (!adapter) {
		hdd_err("adapter is NULL");
		return;
	}

	if (!cds_cfg) {
		hdd_err("cds_cfg is NULL");
		return;
	}

	hdd_ctxt = WLAN_HDD_GET_CTX(adapter);
	rps_data.num_queues = NUM_TX_QUEUES;

	hdd_info("Set cpu_map_list 0");

	qdf_mem_zero(&rps_data.cpu_map_list, sizeof(rps_data.cpu_map_list));

	strlcpy(rps_data.ifname, adapter->dev->name, sizeof(rps_data.ifname));
	wlan_hdd_send_svc_nlink_msg(hdd_ctxt->radio_index,
				    WLAN_SVC_RPS_ENABLE_IND,
				    &rps_data, sizeof(rps_data));

	cds_cfg->rps_enabled = false;
}

#ifdef IPA_LAN_RX_NAPI_SUPPORT
void hdd_adapter_set_rps(uint8_t vdev_id, bool enable)
{
	struct hdd_context *hdd_ctx;
	struct hdd_adapter *adapter;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx)
		return;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err_rl("Adapter not found for vdev_id: %d", vdev_id);
		return;
	}

	hdd_debug("Set RPS to %d for vdev_id %d", enable, vdev_id);
	if (!hdd_ctx->rps) {
		if (enable)
			hdd_send_rps_ind(adapter);
		else
			hdd_send_rps_disable_ind(adapter);
	}
}
#endif

void hdd_tx_queue_cb(hdd_handle_t hdd_handle, uint32_t vdev_id,
		     enum netif_action_type action,
		     enum netif_reason_type reason)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	struct hdd_adapter *adapter;

	/*
	 * Validating the context is not required here.
	 * if there is a driver unload/SSR in progress happening in a
	 * different context and it has been scheduled to run and
	 * driver got a firmware event of sta kick out, then it is
	 * good to disable the Tx Queue to stop the influx of traffic.
	 */
	if (!hdd_ctx) {
		hdd_err("Invalid context passed");
		return;
	}

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (!adapter) {
		hdd_err("vdev_id %d does not exist with host", vdev_id);
		return;
	}
	hdd_debug("Tx Queue action %d on vdev %d", action, vdev_id);

	wlan_hdd_netif_queue_control(adapter, action, reason);
}

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
/**
 * hdd_reset_tcp_delack() - Reset tcp delack value to default
 * @hdd_ctx: Handle to hdd context
 *
 * Function used to reset TCP delack value to its default value
 *
 * Return: None
 */
void hdd_reset_tcp_delack(struct hdd_context *hdd_ctx)
{
	enum wlan_tp_level next_level = WLAN_SVC_TP_LOW;
	struct wlan_rx_tp_data rx_tp_data = {0};

	if (!hdd_ctx->en_tcp_delack_no_lro)
		return;

	rx_tp_data.rx_tp_flags |= TCP_DEL_ACK_IND;
	rx_tp_data.level = next_level;
	hdd_ctx->rx_high_ind_cnt = 0;
	wlan_hdd_update_tcp_rx_param(hdd_ctx, &rx_tp_data);
}

void hdd_reset_tcp_adv_win_scale(struct hdd_context *hdd_ctx)
{
	enum wlan_tp_level next_level = WLAN_SVC_TP_NONE;
	struct wlan_rx_tp_data rx_tp_data = {0};

	rx_tp_data.rx_tp_flags |= TCP_ADV_WIN_SCL;
	rx_tp_data.level = next_level;
	hdd_ctx->cur_rx_level = WLAN_SVC_TP_NONE;
	wlan_hdd_update_tcp_rx_param(hdd_ctx, &rx_tp_data);
}

/**
 * hdd_is_current_high_throughput() - Check if vote level is high
 * @hdd_ctx: Handle to hdd context
 *
 * Function used to check if vote level is high
 *
 * Return: True if vote level is high
 */
#ifdef RX_PERFORMANCE
bool hdd_is_current_high_throughput(struct hdd_context *hdd_ctx)
{
	if (hdd_ctx->cur_vote_level < PLD_BUS_WIDTH_MEDIUM)
		return false;
	else
		return true;
}
#endif
#endif

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
/**
 * hdd_ini_tx_flow_control() - Initialize INIs concerned about tx flow control
 * @config: pointer to hdd config
 * @psoc: pointer to psoc obj
 *
 * Return: none
 */
static void hdd_ini_tx_flow_control(struct hdd_config *config,
				    struct wlan_objmgr_psoc *psoc)
{
	config->tx_flow_low_watermark =
		cfg_get(psoc, CFG_DP_LL_TX_FLOW_LWM);
	config->tx_flow_hi_watermark_offset =
		cfg_get(psoc, CFG_DP_LL_TX_FLOW_HWM_OFFSET);
	config->tx_flow_max_queue_depth =
		cfg_get(psoc, CFG_DP_LL_TX_FLOW_MAX_Q_DEPTH);
	config->tx_lbw_flow_low_watermark =
		cfg_get(psoc, CFG_DP_LL_TX_LBW_FLOW_LWM);
	config->tx_lbw_flow_hi_watermark_offset =
		cfg_get(psoc, CFG_DP_LL_TX_LBW_FLOW_HWM_OFFSET);
	config->tx_lbw_flow_max_queue_depth =
		cfg_get(psoc, CFG_DP_LL_TX_LBW_FLOW_MAX_Q_DEPTH);
	config->tx_hbw_flow_low_watermark =
		cfg_get(psoc, CFG_DP_LL_TX_HBW_FLOW_LWM);
	config->tx_hbw_flow_hi_watermark_offset =
		cfg_get(psoc, CFG_DP_LL_TX_HBW_FLOW_HWM_OFFSET);
	config->tx_hbw_flow_max_queue_depth =
		cfg_get(psoc, CFG_DP_LL_TX_HBW_FLOW_MAX_Q_DEPTH);
}
#else
static void hdd_ini_tx_flow_control(struct hdd_config *config,
				    struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef WLAN_FEATURE_MSCS
/**
 * hdd_ini_mscs_params() - Initialize INIs related to MSCS feature
 * @config: pointer to hdd config
 * @psoc: pointer to psoc obj
 *
 * Return: none
 */
static void hdd_ini_mscs_params(struct hdd_config *config,
				struct wlan_objmgr_psoc *psoc)
{
	config->mscs_pkt_threshold =
		cfg_get(psoc, CFG_VO_PKT_COUNT_THRESHOLD);
	config->mscs_voice_interval =
		cfg_get(psoc, CFG_MSCS_VOICE_INTERVAL);
}

#else
static inline void hdd_ini_mscs_params(struct hdd_config *config,
				       struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
/**
 * hdd_ini_tx_flow_control() - Initialize INIs concerned about bus bandwidth
 * @config: pointer to hdd config
 * @psoc: pointer to psoc obj
 *
 * Return: none
 */
static void hdd_ini_bus_bandwidth(struct hdd_config *config,
				  struct wlan_objmgr_psoc *psoc)
{
	config->bus_bw_very_high_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_VERY_HIGH_THRESHOLD);
	config->bus_bw_high_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_HIGH_THRESHOLD);
	config->bus_bw_medium_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_MEDIUM_THRESHOLD);
	config->bus_bw_low_threshold =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_LOW_THRESHOLD);
	config->bus_bw_compute_interval =
		cfg_get(psoc, CFG_DP_BUS_BANDWIDTH_COMPUTE_INTERVAL);
	config->bus_low_cnt_threshold =
		cfg_get(psoc, CFG_DP_BUS_LOW_BW_CNT_THRESHOLD);
	config->enable_latency_crit_clients =
		cfg_get(psoc, CFG_DP_BUS_HANDLE_LATENCY_CRITICAL_CLIENTS);
}

/**
 * hdd_ini_tx_flow_control() - Initialize INIs concerned about tcp settings
 * @config: pointer to hdd config
 * @psoc: pointer to psoc obj
 *
 * Return: none
 */
static void hdd_ini_tcp_settings(struct hdd_config *config,
				 struct wlan_objmgr_psoc *psoc)
{
	config->enable_tcp_limit_output =
		cfg_get(psoc, CFG_DP_ENABLE_TCP_LIMIT_OUTPUT);
	config->enable_tcp_adv_win_scale =
		cfg_get(psoc, CFG_DP_ENABLE_TCP_ADV_WIN_SCALE);
	config->enable_tcp_delack =
		cfg_get(psoc, CFG_DP_ENABLE_TCP_DELACK);
	config->tcp_delack_thres_high =
		cfg_get(psoc, CFG_DP_TCP_DELACK_THRESHOLD_HIGH);
	config->tcp_delack_thres_low =
		cfg_get(psoc, CFG_DP_TCP_DELACK_THRESHOLD_LOW);
	config->tcp_delack_timer_count =
		cfg_get(psoc, CFG_DP_TCP_DELACK_TIMER_COUNT);
	config->tcp_tx_high_tput_thres =
		cfg_get(psoc, CFG_DP_TCP_TX_HIGH_TPUT_THRESHOLD);
	config->enable_tcp_param_update =
		cfg_get(psoc, CFG_DP_ENABLE_TCP_PARAM_UPDATE);
}
#else
static void hdd_ini_bus_bandwidth(struct hdd_config *config,
				 struct wlan_objmgr_psoc *psoc)
{
}

static void hdd_ini_tcp_settings(struct hdd_config *config,
				 struct wlan_objmgr_psoc *psoc)
{
}
#endif /*WLAN_FEATURE_DP_BUS_BANDWIDTH*/

/**
 * hdd_set_rx_mode_value() - set rx_mode values
 * @hdd_ctx: hdd context
 *
 * Return: none
 */
static void hdd_set_rx_mode_value(struct hdd_context *hdd_ctx)
{
	uint32_t rx_mode = hdd_ctx->config->rx_mode;
	enum QDF_GLOBAL_MODE con_mode = 0;

	con_mode = hdd_get_conparam();

	/* RPS has higher priority than dynamic RPS when both bits are set */
	if (rx_mode & CFG_ENABLE_RPS && rx_mode & CFG_ENABLE_DYNAMIC_RPS)
		rx_mode &= ~CFG_ENABLE_DYNAMIC_RPS;

	if (rx_mode & CFG_ENABLE_RX_THREAD && rx_mode & CFG_ENABLE_RPS) {
		hdd_warn("rx_mode wrong configuration. Make it default");
		rx_mode = CFG_RX_MODE_DEFAULT;
	}

	if (rx_mode & CFG_ENABLE_RX_THREAD)
		hdd_ctx->enable_rxthread = true;
	else if (rx_mode & CFG_ENABLE_DP_RX_THREADS) {
		if (con_mode == QDF_GLOBAL_MONITOR_MODE)
			hdd_ctx->enable_dp_rx_threads = false;
		else
			hdd_ctx->enable_dp_rx_threads = true;
	}

	if (rx_mode & CFG_ENABLE_RPS)
		hdd_ctx->rps = true;

	if (rx_mode & CFG_ENABLE_NAPI)
		hdd_ctx->napi_enable = true;

	if (rx_mode & CFG_ENABLE_DYNAMIC_RPS)
		hdd_ctx->dynamic_rps = true;

	hdd_debug("rx_mode:%u dp_rx_threads:%u rx_thread:%u napi:%u rps:%u dynamic rps %u",
		  rx_mode, hdd_ctx->enable_dp_rx_threads,
		  hdd_ctx->enable_rxthread, hdd_ctx->napi_enable,
		  hdd_ctx->rps, hdd_ctx->dynamic_rps);
}

#ifdef CONFIG_DP_TRACE
static void
hdd_dp_dp_trace_cfg_update(struct hdd_config *config,
			   struct wlan_objmgr_psoc *psoc)
{
	qdf_size_t array_out_size;

	config->enable_dp_trace = cfg_get(psoc, CFG_DP_ENABLE_DP_TRACE);
	qdf_uint8_array_parse(cfg_get(psoc, CFG_DP_DP_TRACE_CONFIG),
			      config->dp_trace_config,
			      sizeof(config->dp_trace_config), &array_out_size);
	config->dp_proto_event_bitmap = cfg_get(psoc,
						CFG_DP_PROTO_EVENT_BITMAP);
}
#else
static void
hdd_dp_dp_trace_cfg_update(struct hdd_config *config,
			   struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef WLAN_NUD_TRACKING
static void
hdd_dp_nud_tracking_cfg_update(struct hdd_config *config,
			       struct wlan_objmgr_psoc *psoc)
{
	config->enable_nud_tracking = cfg_get(psoc, CFG_DP_ENABLE_NUD_TRACKING);
}
#else
static void
hdd_dp_nud_tracking_cfg_update(struct hdd_config *config,
			       struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK
static void hdd_ini_tcp_del_ack_settings(struct hdd_config *config,
					 struct wlan_objmgr_psoc *psoc)
{
	config->del_ack_threshold_high =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_HIGH_THRESHOLD);
	config->del_ack_threshold_low =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_LOW_THRESHOLD);
	config->del_ack_enable =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_ENABLE);
	config->del_ack_pkt_count =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_PKT_CNT);
	config->del_ack_timer_value =
		cfg_get(psoc, CFG_DP_DRIVER_TCP_DELACK_TIMER_VALUE);
}
#else
static void hdd_ini_tcp_del_ack_settings(struct hdd_config *config,
					 struct wlan_objmgr_psoc *psoc)
{
}
#endif

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
static void hdd_dp_hl_bundle_cfg_update(struct hdd_config *config,
					struct wlan_objmgr_psoc *psoc)
{
	config->pkt_bundle_threshold_high =
		cfg_get(psoc, CFG_DP_HL_BUNDLE_HIGH_TH);
	config->pkt_bundle_threshold_low =
		cfg_get(psoc, CFG_DP_HL_BUNDLE_LOW_TH);
	config->pkt_bundle_timer_value =
		cfg_get(psoc, CFG_DP_HL_BUNDLE_TIMER_VALUE);
	config->pkt_bundle_size =
		cfg_get(psoc, CFG_DP_HL_BUNDLE_SIZE);
}
#else
static void hdd_dp_hl_bundle_cfg_update(struct hdd_config *config,
					struct wlan_objmgr_psoc *psoc)
{
}
#endif

void hdd_dp_cfg_update(struct wlan_objmgr_psoc *psoc,
		       struct hdd_context *hdd_ctx)
{
	struct hdd_config *config;
	uint16_t cfg_len;

	config = hdd_ctx->config;
	cfg_len = qdf_str_len(cfg_get(psoc, CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST))
		  + 1;
	hdd_ini_tx_flow_control(config, psoc);
	hdd_ini_bus_bandwidth(config, psoc);
	hdd_ini_tcp_settings(config, psoc);
	hdd_ini_mscs_params(config, psoc);

	hdd_ini_tcp_del_ack_settings(config, psoc);

	hdd_dp_hl_bundle_cfg_update(config, psoc);

	config->napi_cpu_affinity_mask =
		cfg_get(psoc, CFG_DP_NAPI_CE_CPU_MASK);
	config->rx_thread_ul_affinity_mask =
		cfg_get(psoc, CFG_DP_RX_THREAD_UL_CPU_MASK);
	config->rx_thread_affinity_mask =
		cfg_get(psoc, CFG_DP_RX_THREAD_CPU_MASK);
	config->fisa_enable = cfg_get(psoc, CFG_DP_RX_FISA_ENABLE);
	if (cfg_len < CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST_LEN) {
		qdf_str_lcopy(config->cpu_map_list,
			      cfg_get(psoc, CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST),
			      cfg_len);
	} else {
		hdd_err("ini string length greater than max size %d",
			CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST_LEN);
		cfg_len = qdf_str_len(cfg_default(CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST));
		qdf_str_lcopy(config->cpu_map_list,
			      cfg_default(CFG_DP_RPS_RX_QUEUE_CPU_MAP_LIST),
			      cfg_len);
	}
	config->tx_orphan_enable = cfg_get(psoc, CFG_DP_TX_ORPHAN_ENABLE);
	config->rx_mode = cfg_get(psoc, CFG_DP_RX_MODE);
	hdd_set_rx_mode_value(hdd_ctx);
	config->multicast_replay_filter =
		cfg_get(psoc, CFG_DP_FILTER_MULTICAST_REPLAY);
	config->rx_wakelock_timeout =
		cfg_get(psoc, CFG_DP_RX_WAKELOCK_TIMEOUT);
	config->num_dp_rx_threads = cfg_get(psoc, CFG_DP_NUM_DP_RX_THREADS);
	config->cfg_wmi_credit_cnt = cfg_get(psoc, CFG_DP_HTC_WMI_CREDIT_CNT);
	config->icmp_req_to_fw_mark_interval =
		cfg_get(psoc, CFG_DP_ICMP_REQ_TO_FW_MARK_INTERVAL);
	hdd_dp_dp_trace_cfg_update(config, psoc);
	hdd_dp_nud_tracking_cfg_update(config, psoc);
}

bool wlan_hdd_rx_rpm_mark_last_busy(struct hdd_context *hdd_ctx,
				    void *hif_ctx)
{
	uint64_t duration_us, dp_rx_busy_us, current_us;
	uint32_t rpm_delay_ms;

	if (!hif_pm_runtime_is_dp_rx_busy(hif_ctx))
		return false;

	dp_rx_busy_us = hif_pm_runtime_get_dp_rx_busy_mark(hif_ctx);
	current_us = qdf_get_log_timestamp_usecs();
	duration_us = (unsigned long)((ULONG_MAX - dp_rx_busy_us) +
				      current_us + 1);
	rpm_delay_ms = ucfg_pmo_get_runtime_pm_delay(hdd_ctx->psoc);

	if (duration_us < (rpm_delay_ms * 1000))
		return true;
	else
		return false;
}

void hdd_sta_notify_tx_comp_cb(qdf_nbuf_t skb, void *ctx, uint16_t flag)
{
	struct hdd_adapter *adapter = ctx;
	enum qdf_proto_subtype subtype;

	if (hdd_validate_adapter(adapter))
		return;

	switch (QDF_NBUF_CB_GET_PACKET_TYPE(skb)) {
	case QDF_NBUF_CB_PACKET_TYPE_ARP:
		if (flag & BIT(QDF_TX_RX_STATUS_DOWNLOAD_SUCC))
			++adapter->hdd_stats.hdd_arp_stats.
				tx_host_fw_sent;
		if (flag & BIT(QDF_TX_RX_STATUS_OK))
			++adapter->hdd_stats.hdd_arp_stats.tx_ack_cnt;
		break;
	case QDF_NBUF_CB_PACKET_TYPE_EAPOL:
		subtype = qdf_nbuf_get_eapol_subtype(skb);
		if (!(flag & BIT(QDF_TX_RX_STATUS_OK)) &&
		    subtype != QDF_PROTO_INVALID)
			++adapter->hdd_stats.hdd_eapol_stats.
				tx_noack_cnt[subtype - QDF_PROTO_EAPOL_M1];
		break;
	case QDF_NBUF_CB_PACKET_TYPE_DHCP:
		subtype = qdf_nbuf_get_dhcp_subtype(skb);
		if (!(flag & BIT(QDF_TX_RX_STATUS_OK)) &&
		    subtype != QDF_PROTO_INVALID &&
		    subtype <= QDF_PROTO_DHCP_ACK)
			++adapter->hdd_stats.hdd_dhcp_stats.
				tx_noack_cnt[subtype - QDF_PROTO_DHCP_DISCOVER];
		break;
	default:
		break;
	}
}
