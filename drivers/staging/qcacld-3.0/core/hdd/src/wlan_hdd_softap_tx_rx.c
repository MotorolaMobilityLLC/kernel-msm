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

/* denote that this file does not allow legacy hddLog */
#define HDD_DISALLOW_LEGACY_HDDLOG 1

/* Include files */
#include <linux/semaphore.h>
#include "osif_sync.h"
#include <wlan_hdd_tx_rx.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <qdf_types.h>
#include <ani_global.h>
#include <qdf_types.h>
#include <net/ieee80211_radiotap.h>
#include <cds_sched.h>
#include <wlan_hdd_napi.h>
#include <cdp_txrx_cmn.h>
#include <cdp_txrx_peer_ops.h>
#include <cds_utils.h>
#include <cdp_txrx_flow_ctrl_v2.h>
#include <cdp_txrx_misc.h>
#include <wlan_hdd_object_manager.h>
#include "wlan_p2p_ucfg_api.h"
#include <wlan_hdd_regulatory.h>
#include "wlan_ipa_ucfg_api.h"
#include "wlan_policy_mgr_ucfg.h"
#include "wlan_mlme_twt_ucfg_api.h"
#include <wma_types.h>
#include "wlan_hdd_sta_info.h"
#include "ol_defines.h"
#include <wlan_hdd_sar_limits.h>

/* Preprocessor definitions and constants */
#undef QCA_HDD_SAP_DUMP_SK_BUFF

/* Type declarations */

/* Function definitions and documenation */
#ifdef QCA_HDD_SAP_DUMP_SK_BUFF
/**
 * hdd_softap_dump_sk_buff() - Dump an skb
 * @skb: skb to dump
 *
 * Return: None
 */
static void hdd_softap_dump_sk_buff(struct sk_buff *skb)
{
	QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
		  "%s: head = %pK ", __func__, skb->head);
	QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_INFO,
		  "%s: tail = %pK ", __func__, skb->tail);
	QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
		  "%s: end = %pK ", __func__, skb->end);
	QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
		  "%s: len = %d ", __func__, skb->len);
	QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
		  "%s: data_len = %d ", __func__, skb->data_len);
	QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
		  "%s: mac_len = %d", __func__, skb->mac_len);

	QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
		  "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x ", skb->data[0],
		  skb->data[1], skb->data[2], skb->data[3], skb->data[4],
		  skb->data[5], skb->data[6], skb->data[7]);
	QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
		  "0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x 0x%x", skb->data[8],
		  skb->data[9], skb->data[10], skb->data[11], skb->data[12],
		  skb->data[13], skb->data[14], skb->data[15]);
}
#else
static void hdd_softap_dump_sk_buff(struct sk_buff *skb)
{
}
#endif

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
void hdd_softap_tx_resume_timer_expired_handler(void *adapter_context)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) adapter_context;

	if (!adapter) {
		hdd_err("NULL adapter");
		return;
	}

	hdd_debug("Enabling queues");
	wlan_hdd_netif_queue_control(adapter, WLAN_WAKE_ALL_NETIF_QUEUE,
				     WLAN_CONTROL_PATH);
}

/**
 * hdd_softap_tx_resume_false() - Resume OS TX Q false leads to queue disabling
 * @adapter: pointer to hdd adapter
 * @tx_resume: TX Q resume trigger
 *
 *
 * Return: None
 */
static void
hdd_softap_tx_resume_false(struct hdd_adapter *adapter, bool tx_resume)
{
	if (true == tx_resume)
		return;

	hdd_debug("Disabling queues");
	wlan_hdd_netif_queue_control(adapter, WLAN_STOP_ALL_NETIF_QUEUE,
				     WLAN_DATA_FLOW_CONTROL);

	if (QDF_TIMER_STATE_STOPPED ==
			qdf_mc_timer_get_current_state(&adapter->
						       tx_flow_control_timer)) {
		QDF_STATUS status;

		status = qdf_mc_timer_start(&adapter->tx_flow_control_timer,
				WLAN_SAP_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);

		if (!QDF_IS_STATUS_SUCCESS(status))
			hdd_err("Failed to start tx_flow_control_timer");
		else
			adapter->hdd_stats.tx_rx_stats.txflow_timer_cnt++;
	}
}

void hdd_softap_tx_resume_cb(void *adapter_context, bool tx_resume)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) adapter_context;

	if (!adapter) {
		hdd_err("NULL adapter");
		return;
	}

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
	}
	hdd_softap_tx_resume_false(adapter, tx_resume);
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

#define IEEE8021X_AUTH_TYPE_EAP 0
#define EAP_CODE_OFFSET 18
#define EAP_CODE_FAILURE 4

/* Wait EAP Failure frame timeout in (MS) */
#define EAP_FRM_TIME_OUT 80

/**
 * hdd_softap_inspect_tx_eap_pkt() - Inspect eap pkt tx/tx-completion
 * @adapter: pointer to hdd adapter
 * @skb: sk_buff
 * @tx_comp: tx sending or tx completion
 *
 * Inspect the EAP-Failure pkt tx sending and tx completion.
 *
 * Return: void
 */
static void hdd_softap_inspect_tx_eap_pkt(struct hdd_adapter *adapter,
					  struct sk_buff *skb,
					  bool tx_comp)
{
	struct qdf_mac_addr *mac_addr;
	uint8_t *data;
	uint8_t auth_type, eap_code;
	struct hdd_station_info *sta_info;
	struct hdd_hostapd_state *hapd_state;

	if (qdf_likely(QDF_NBUF_CB_GET_PACKET_TYPE(skb) !=
	    QDF_NBUF_CB_PACKET_TYPE_EAPOL) || skb->len < (EAP_CODE_OFFSET + 1))
		return;

	if (cds_is_driver_recovering() || cds_is_driver_in_bad_state() ||
	    cds_is_load_or_unload_in_progress()) {
		hdd_debug("Recovery/(Un)load in Progress. Ignore!!!");
		return;
	}
	if (adapter->device_mode != QDF_P2P_GO_MODE)
		return;
	hapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);
	if (!hapd_state || hapd_state->bss_state != BSS_START) {
		hdd_debug("Hostapd State is not START");
		return;
	}
	data = skb->data;
	auth_type = *(uint8_t *)(data + EAPOL_PACKET_TYPE_OFFSET);
	if (auth_type != IEEE8021X_AUTH_TYPE_EAP)
		return;
	eap_code = *(uint8_t *)(data + EAP_CODE_OFFSET);
	if (eap_code != EAP_CODE_FAILURE)
		return;
	mac_addr = (struct qdf_mac_addr *)skb->data;
	sta_info = hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					   mac_addr->bytes,
					   STA_INFO_SOFTAP_INSPECT_TX_EAP_PKT);
	if (!sta_info)
		return;
	if (tx_comp) {
		hdd_debug("eap_failure frm tx done "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(mac_addr->bytes));
		qdf_atomic_clear_bit(PENDING_TYPE_EAP_FAILURE,
				     &sta_info->pending_eap_frm_type);
		qdf_event_set(&hapd_state->qdf_sta_eap_frm_done_event);
	} else {
		hdd_debug("eap_failure frm tx pending "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(mac_addr->bytes));
		qdf_event_reset(&hapd_state->qdf_sta_eap_frm_done_event);
		qdf_atomic_set_bit(PENDING_TYPE_EAP_FAILURE,
				   &sta_info->pending_eap_frm_type);
		QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(skb) = 1;
	}
	hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
			     STA_INFO_SOFTAP_INSPECT_TX_EAP_PKT);
}

void hdd_softap_check_wait_for_tx_eap_pkt(struct hdd_adapter *adapter,
					  struct qdf_mac_addr *mac_addr)
{
	struct hdd_station_info *sta_info;
	QDF_STATUS qdf_status;
	struct hdd_hostapd_state *hapd_state;

	if (adapter->device_mode != QDF_P2P_GO_MODE)
		return;
	hapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);
	if (!hapd_state || hapd_state->bss_state != BSS_START) {
		hdd_err("Hostapd State is not START");
		return;
	}
	sta_info = hdd_get_sta_info_by_mac(
				&adapter->sta_info_list,
				mac_addr->bytes,
				STA_INFO_SOFTAP_CHECK_WAIT_FOR_TX_EAP_PKT);
	if (!sta_info)
		return;
	if (qdf_atomic_test_bit(PENDING_TYPE_EAP_FAILURE,
				&sta_info->pending_eap_frm_type)) {
		hdd_debug("eap_failure frm pending "QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(mac_addr->bytes));
		qdf_status = qdf_wait_for_event_completion(
				&hapd_state->qdf_sta_eap_frm_done_event,
				EAP_FRM_TIME_OUT);
		if (!QDF_IS_STATUS_SUCCESS(qdf_status))
			hdd_debug("eap_failure tx timeout");
	}
	hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
			     STA_INFO_SOFTAP_CHECK_WAIT_FOR_TX_EAP_PKT);
}

#ifdef SAP_DHCP_FW_IND
/**
 * hdd_post_dhcp_ind() - Send DHCP START/STOP indication to FW
 * @adapter: pointer to hdd adapter
 * @sta_id: peer station ID
 * @type: WMA message type
 *
 * Return: error number
 */
int hdd_post_dhcp_ind(struct hdd_adapter *adapter, uint8_t *mac_addr,
		      uint16_t type)
{
	tAniDHCPInd pmsg;
	QDF_STATUS status = QDF_STATUS_SUCCESS;

	hdd_debug("Post DHCP indication,sta_mac=" QDF_MAC_ADDR_FMT
		  " ,  type=%d", QDF_MAC_ADDR_REF(mac_addr), type);

	if (!adapter) {
		hdd_err("NULL adapter");
		return -EINVAL;
	}

	pmsg.msgType = type;
	pmsg.msgLen = (uint16_t) sizeof(tAniDHCPInd);
	pmsg.device_mode = adapter->device_mode;
	qdf_mem_copy(pmsg.adapterMacAddr.bytes,
		     adapter->mac_addr.bytes,
		     QDF_MAC_ADDR_SIZE);
	qdf_mem_copy(pmsg.peerMacAddr.bytes,
		     mac_addr,
		     QDF_MAC_ADDR_SIZE);

	status = wma_process_dhcp_ind(cds_get_context(QDF_MODULE_ID_WMA),
				      &pmsg);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
			  "%s: Post DHCP Ind MSG fail", __func__);
		return -EFAULT;
	}

	return 0;
}

#define DHCP_CLIENT_MAC_ADDR_OFFSET 0x46

/**
 * hdd_softap_notify_dhcp_ind() - Notify SAP for DHCP indication for tx desc
 * @context: pointer to HDD context
 * @netbuf: pointer to OS packet (sk_buff)
 *
 * Return: None
 */
static void hdd_softap_notify_dhcp_ind(void *context, struct sk_buff *netbuf)
{
	struct hdd_ap_ctx *hdd_ap_ctx;
	uint8_t *dest_mac_addr;
	struct hdd_adapter *adapter = context;

	if (hdd_validate_adapter(adapter))
		return;

	hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
	if (!hdd_ap_ctx) {
		hdd_err("HDD sap context is NULL");
		return;
	}

	dest_mac_addr = netbuf->data + DHCP_CLIENT_MAC_ADDR_OFFSET;

	hdd_post_dhcp_ind(adapter, dest_mac_addr, WMA_DHCP_STOP_IND);
}

void hdd_ipa_update_rx_mcbc_stats(struct hdd_adapter *adapter,
				  struct sk_buff *skb)
{
	struct hdd_station_info *hdd_sta_info;
	struct qdf_mac_addr *src_mac;
	qdf_ether_header_t *eh;

	src_mac = (struct qdf_mac_addr *)(skb->data +
					  QDF_NBUF_SRC_MAC_OFFSET);

	hdd_sta_info = hdd_get_sta_info_by_mac(
				&adapter->sta_info_list,
				src_mac->bytes,
				STA_INFO_SOFTAP_IPA_RX_PKT_CALLBACK);
	if (!hdd_sta_info)
		return;

	if (qdf_nbuf_data_is_ipv4_mcast_pkt(skb->data))
		hdd_sta_info->rx_mc_bc_cnt++;

	eh = (qdf_ether_header_t *)qdf_nbuf_data(skb);
	if (QDF_IS_ADDR_BROADCAST(eh->ether_dhost))
		hdd_sta_info->rx_mc_bc_cnt++;

	hdd_put_sta_info_ref(&adapter->sta_info_list, &hdd_sta_info,
			     true, STA_INFO_SOFTAP_IPA_RX_PKT_CALLBACK);
}

int hdd_softap_inspect_dhcp_packet(struct hdd_adapter *adapter,
				   struct sk_buff *skb,
				   enum qdf_proto_dir dir)
{
	enum qdf_proto_subtype subtype = QDF_PROTO_INVALID;
	struct hdd_station_info *hdd_sta_info;
	int errno = 0;
	struct qdf_mac_addr *src_mac;

	if (((adapter->device_mode == QDF_SAP_MODE) ||
	     (adapter->device_mode == QDF_P2P_GO_MODE)) &&
	    ((dir == QDF_TX && QDF_NBUF_CB_PACKET_TYPE_DHCP ==
				QDF_NBUF_CB_GET_PACKET_TYPE(skb)) ||
	     (dir == QDF_RX && qdf_nbuf_is_ipv4_dhcp_pkt(skb) == true))) {

		src_mac = (struct qdf_mac_addr *)(skb->data +
						  DHCP_CLIENT_MAC_ADDR_OFFSET);

		subtype = qdf_nbuf_get_dhcp_subtype(skb);
		hdd_sta_info = hdd_get_sta_info_by_mac(
					&adapter->sta_info_list,
					src_mac->bytes,
					STA_INFO_SOFTAP_INSPECT_DHCP_PACKET);
		if (!hdd_sta_info) {
			hdd_debug("Station not found");
			return -EINVAL;
		}

		hdd_debug("ENTER: type=%d, phase=%d, nego_status=%d",
			  subtype,
			  hdd_sta_info->dhcp_phase,
			  hdd_sta_info->dhcp_nego_status);

		switch (subtype) {
		case QDF_PROTO_DHCP_DISCOVER:
			if (dir != QDF_RX)
				break;
			if (hdd_sta_info->dhcp_nego_status == DHCP_NEGO_STOP)
				errno =	hdd_post_dhcp_ind(
						adapter,
						hdd_sta_info->sta_mac.bytes,
						WMA_DHCP_START_IND);
			hdd_sta_info->dhcp_phase = DHCP_PHASE_DISCOVER;
			hdd_sta_info->dhcp_nego_status = DHCP_NEGO_IN_PROGRESS;
			break;
		case QDF_PROTO_DHCP_OFFER:
			hdd_sta_info->dhcp_phase = DHCP_PHASE_OFFER;
			break;
		case QDF_PROTO_DHCP_REQUEST:
			if (dir != QDF_RX)
				break;
			if (hdd_sta_info->dhcp_nego_status == DHCP_NEGO_STOP)
				errno = hdd_post_dhcp_ind(
						adapter,
						hdd_sta_info->sta_mac.bytes,
						WMA_DHCP_START_IND);
			hdd_sta_info->dhcp_nego_status = DHCP_NEGO_IN_PROGRESS;
			/* fallthrough */
		case QDF_PROTO_DHCP_DECLINE:
			if (dir == QDF_RX)
				hdd_sta_info->dhcp_phase = DHCP_PHASE_REQUEST;
			break;
		case QDF_PROTO_DHCP_ACK:
		case QDF_PROTO_DHCP_NACK:
			hdd_sta_info->dhcp_phase = DHCP_PHASE_ACK;
			if (hdd_sta_info->dhcp_nego_status ==
				DHCP_NEGO_IN_PROGRESS) {
				hdd_debug("Setting NOTIFY_COMP Flag");
				QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(skb)
									= 1;
			}
			hdd_sta_info->dhcp_nego_status = DHCP_NEGO_STOP;
			break;
		default:
			break;
		}

		hdd_debug("EXIT: phase=%d, nego_status=%d",
			  hdd_sta_info->dhcp_phase,
			  hdd_sta_info->dhcp_nego_status);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &hdd_sta_info,
				     true, STA_INFO_SOFTAP_INSPECT_DHCP_PACKET);
	}

	return errno;
}
#else
static void hdd_softap_notify_dhcp_ind(void *context, struct sk_buff *netbuf)
{
}
#endif

/**
 * __hdd_softap_hard_start_xmit() - Transmit a frame
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
static void __hdd_softap_hard_start_xmit(struct sk_buff *skb,
					 struct net_device *dev)
{
	sme_ac_enum_type ac = SME_AC_BE;
	struct hdd_adapter *adapter = (struct hdd_adapter *) netdev_priv(dev);
	struct hdd_ap_ctx *ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
	struct hdd_context *hdd_ctx = adapter->hdd_ctx;
	struct qdf_mac_addr *dest_mac_addr, *mac_addr;
	static struct qdf_mac_addr bcast_mac_addr = QDF_MAC_ADDR_BCAST_INIT;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	uint32_t num_seg;
	struct hdd_station_info *sta_info = NULL;

	++adapter->hdd_stats.tx_rx_stats.tx_called;
	adapter->hdd_stats.tx_rx_stats.cont_txtimeout_cnt = 0;

	/* Prevent this function from being called during SSR since TL
	 * context may not be reinitialized at this time which may
	 * lead to a crash.
	 */
	if (cds_is_driver_recovering() || cds_is_driver_in_bad_state() ||
	    cds_is_load_or_unload_in_progress()) {
		QDF_TRACE_DEBUG_RL(
			  QDF_MODULE_ID_HDD_SAP_DATA,
			  "%s: Recovery/(Un)load in Progress. Ignore!!!",
			  __func__);
		goto drop_pkt;
	}

	if (hdd_ctx->hdd_wlan_suspended) {
		hdd_err_rl("Device is system suspended, drop pkt");
		goto drop_pkt;
	}

	/*
	 * If the device is operating on a DFS Channel
	 * then check if SAP is in CAC WAIT state and
	 * drop the packets. In CAC WAIT state device
	 * is expected not to transmit any frames.
	 * SAP starts Tx only after the BSS START is
	 * done.
	 */
	if (ap_ctx->dfs_cac_block_tx)
		goto drop_pkt;

	if (ap_ctx->hostapd_state.bss_state != BSS_START) {
		QDF_TRACE_DEBUG_RL(
			QDF_MODULE_ID_HDD_SAP_DATA,
			"%s: SAP is not in START state (%d). Ignore!!!",
			__func__, ap_ctx->hostapd_state.bss_state);
		goto drop_pkt;
	}

	/*
	 * If a transmit function is not registered, drop packet
	 */
	if (!adapter->tx_fn) {
		QDF_TRACE_DEBUG_RL(
			 QDF_MODULE_ID_HDD_SAP_DATA,
			 "%s: TX function not registered by the data path",
			 __func__);
		goto drop_pkt;
	}

	wlan_hdd_classify_pkt(skb);

	dest_mac_addr = (struct qdf_mac_addr *)skb->data;

	/* In case of mcast, fetch the bcast sta_info. Else use the pkt addr */
	if (QDF_NBUF_CB_GET_IS_MCAST(skb))
		mac_addr = &bcast_mac_addr;
	else
		mac_addr = dest_mac_addr;

	sta_info = hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					   mac_addr->bytes,
					   STA_INFO_SOFTAP_HARD_START_XMIT);

	if (!QDF_NBUF_CB_GET_IS_BCAST(skb) && !QDF_NBUF_CB_GET_IS_MCAST(skb)) {
		if (!sta_info) {
			QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_HDD_SAP_DATA,
					   "%s: Failed to find right station",
					   __func__);
			goto drop_pkt;
		}

		if (sta_info->is_deauth_in_progress) {
			QDF_TRACE_DEBUG_RL(
				  QDF_MODULE_ID_HDD_SAP_DATA,
				  "%s: STA " QDF_MAC_ADDR_FMT
				  "deauth in progress", __func__,
				  QDF_MAC_ADDR_REF(sta_info->sta_mac.bytes));
			goto drop_pkt;
		}

		if (sta_info->peer_state != OL_TXRX_PEER_STATE_CONN &&
		    sta_info->peer_state != OL_TXRX_PEER_STATE_AUTH) {
			QDF_TRACE_DEBUG_RL(
				QDF_MODULE_ID_HDD_SAP_DATA,
				"%s: Station not connected yet", __func__);
			goto drop_pkt;
		}

		if (sta_info->peer_state == OL_TXRX_PEER_STATE_CONN) {
			if (ntohs(skb->protocol) != HDD_ETHERTYPE_802_1_X) {
				QDF_TRACE_DEBUG_RL(
					  QDF_MODULE_ID_HDD_SAP_DATA,
					  "%s: NON-EAPOL packet in non-Authenticated state",
					  __func__);
				goto drop_pkt;
			}
		}
	}

	if (QDF_NBUF_CB_GET_IS_BCAST(skb) || QDF_NBUF_CB_GET_IS_MCAST(skb))
		hdd_get_tx_resource(
				adapter, &adapter->mac_addr,
				WLAN_SAP_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);
	else
		hdd_get_tx_resource(
				adapter, dest_mac_addr,
				WLAN_SAP_HDD_TX_FLOW_CONTROL_OS_Q_BLOCK_TIME);

	/* Get TL AC corresponding to Qdisc queue index/AC. */
	ac = hdd_qdisc_ac_to_tl_ac[skb->queue_mapping];
	++adapter->hdd_stats.tx_rx_stats.tx_classified_ac[ac];

#if defined(IPA_OFFLOAD)
	if (!qdf_nbuf_ipa_owned_get(skb)) {
#endif

		skb = hdd_skb_orphan(adapter, skb);
		if (!skb)
			goto drop_pkt_accounting;

#if defined(IPA_OFFLOAD)
	} else {
		/*
		 * Clear the IPA ownership after check it to avoid ipa_free_skb
		 * is called when Tx completed for intra-BSS Tx packets
		 */
		qdf_nbuf_ipa_owned_clear(skb);
	}
#endif

	/*
	 * Add SKB to internal tracking table before further processing
	 * in WLAN driver.
	 */
	qdf_net_buf_debug_acquire_skb(skb, __FILE__, __LINE__);

	adapter->stats.tx_bytes += skb->len;

	if (sta_info) {
		sta_info->tx_bytes += skb->len;

		if (qdf_nbuf_is_tso(skb)) {
			num_seg = qdf_nbuf_get_tso_num_seg(skb);
			adapter->stats.tx_packets += num_seg;
			sta_info->tx_packets += num_seg;
		} else {
			++adapter->stats.tx_packets;
			sta_info->tx_packets++;
			hdd_ctx->no_tx_offload_pkt_cnt++;
		}
		sta_info->last_tx_rx_ts = qdf_system_ticks();
	}

	QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(skb) = 0;

	hdd_softap_inspect_dhcp_packet(adapter, skb, QDF_TX);
	hdd_softap_inspect_tx_eap_pkt(adapter, skb, false);

	hdd_event_eapol_log(skb, QDF_TX);
	QDF_NBUF_CB_TX_PACKET_TRACK(skb) = QDF_NBUF_TX_PKT_DATA_TRACK;
	QDF_NBUF_UPDATE_TX_PKT_COUNT(skb, QDF_NBUF_TX_PKT_HDD);
	qdf_dp_trace_set_track(skb, QDF_TX);
	DPTRACE(qdf_dp_trace(skb, QDF_DP_TRACE_HDD_TX_PACKET_PTR_RECORD,
			QDF_TRACE_DEFAULT_PDEV_ID, qdf_nbuf_data_addr(skb),
			sizeof(qdf_nbuf_data(skb)),
			QDF_TX));

	/* check whether need to linearize skb, like non-linear udp data */
	if (hdd_skb_nontso_linearize(skb) != QDF_STATUS_SUCCESS) {
		QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_HDD_DATA,
				   "%s: skb %pK linearize failed. drop the pkt",
				   __func__, skb);
		++adapter->hdd_stats.tx_rx_stats.tx_dropped_ac[ac];
		goto drop_pkt_and_release_skb;
	}

	if (adapter->tx_fn(soc, adapter->vdev_id, (qdf_nbuf_t)skb)) {
		QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_HDD_SAP_DATA,
				   "%s: Failed to send packet to txrx for sta: "
				   QDF_MAC_ADDR_FMT, __func__,
				   QDF_MAC_ADDR_REF(dest_mac_addr->bytes));
		++adapter->hdd_stats.tx_rx_stats.tx_dropped_ac[ac];
		goto drop_pkt_and_release_skb;
	}
	netif_trans_update(dev);

	wlan_hdd_sar_unsolicited_timer_start(hdd_ctx);
	hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
			     STA_INFO_SOFTAP_HARD_START_XMIT);

	return;

drop_pkt_and_release_skb:
	qdf_net_buf_debug_release_skb(skb);
drop_pkt:
	qdf_dp_trace_data_pkt(skb, QDF_TRACE_DEFAULT_PDEV_ID,
			      QDF_DP_TRACE_DROP_PACKET_RECORD, 0,
			      QDF_TX);
	kfree_skb(skb);

drop_pkt_accounting:
	if (sta_info)
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
				     STA_INFO_SOFTAP_HARD_START_XMIT);
	++adapter->stats.tx_dropped;
	++adapter->hdd_stats.tx_rx_stats.tx_dropped;
}

netdev_tx_t hdd_softap_hard_start_xmit(struct sk_buff *skb,
				       struct net_device *net_dev)
{
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(net_dev, &vdev_sync)) {
		hdd_debug_rl("Operation on net_dev is not permitted");
		kfree_skb(skb);
		return NETDEV_TX_OK;
	}

	__hdd_softap_hard_start_xmit(skb, net_dev);

	osif_vdev_sync_op_stop(vdev_sync);

	return NETDEV_TX_OK;
}

QDF_STATUS hdd_softap_ipa_start_xmit(qdf_nbuf_t nbuf, qdf_netdev_t dev)
{
	if (NETDEV_TX_OK == hdd_softap_hard_start_xmit(
					(struct sk_buff *)nbuf,
					(struct net_device *)dev))
		return QDF_STATUS_SUCCESS;
	else
		return QDF_STATUS_E_FAILURE;
}

static void __hdd_softap_tx_timeout(struct net_device *dev)
{
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx;
	struct netdev_queue *txq;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	int i;

	DPTRACE(qdf_dp_trace(NULL, QDF_DP_TRACE_HDD_SOFTAP_TX_TIMEOUT,
			QDF_TRACE_DEFAULT_PDEV_ID,
			NULL, 0, QDF_TX));
	/* Getting here implies we disabled the TX queues for too
	 * long. Queues are disabled either because of disassociation
	 * or low resource scenarios. In case of disassociation it is
	 * ok to ignore this. But if associated, we have do possible
	 * recovery here
	 */
	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	if (cds_is_driver_recovering() || cds_is_driver_in_bad_state()) {
		QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
			 "%s: Recovery in Progress. Ignore!!!", __func__);
		return;
	}

	if (hdd_ctx->hdd_wlan_suspended) {
		hdd_debug("wlan is suspended, ignore timeout");
		return;
	}

	TX_TIMEOUT_TRACE(dev, QDF_MODULE_ID_HDD_SAP_DATA);

	for (i = 0; i < NUM_TX_QUEUES; i++) {
		txq = netdev_get_tx_queue(dev, i);
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA,
			  QDF_TRACE_LEVEL_DEBUG,
			  "Queue: %d status: %d txq->trans_start: %lu",
			  i, netif_tx_queue_stopped(txq), txq->trans_start);
	}

	wlan_hdd_display_adapter_netif_queue_history(adapter);

	cdp_dump_flow_pool_info(cds_get_context(QDF_MODULE_ID_SOC));
	QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_DEBUG,
			"carrier state: %d", netif_carrier_ok(dev));

	++adapter->hdd_stats.tx_rx_stats.tx_timeout_cnt;
	++adapter->hdd_stats.tx_rx_stats.cont_txtimeout_cnt;

	if (adapter->hdd_stats.tx_rx_stats.cont_txtimeout_cnt >
	    HDD_TX_STALL_THRESHOLD) {
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_ERROR,
			  "Detected data stall due to continuous TX timeouts");
		adapter->hdd_stats.tx_rx_stats.cont_txtimeout_cnt = 0;

		if (cdp_cfg_get(soc, cfg_dp_enable_data_stall))
			cdp_post_data_stall_event(soc,
					  DATA_STALL_LOG_INDICATOR_HOST_DRIVER,
					  DATA_STALL_LOG_HOST_SOFTAP_TX_TIMEOUT,
					  OL_TXRX_PDEV_ID, 0xFF,
					  DATA_STALL_LOG_RECOVERY_TRIGGER_PDR);
	}
}

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
void hdd_softap_tx_timeout(struct net_device *net_dev, unsigned int txqueue)
#else
void hdd_softap_tx_timeout(struct net_device *net_dev)
#endif
{
	struct osif_vdev_sync *vdev_sync;

	if (osif_vdev_sync_op_start(net_dev, &vdev_sync))
		return;

	__hdd_softap_tx_timeout(net_dev);

	osif_vdev_sync_op_stop(vdev_sync);
}

void hdd_softap_init_tx_rx(struct hdd_adapter *adapter)
{
	qdf_mem_zero(&adapter->stats, sizeof(struct net_device_stats));
}

QDF_STATUS hdd_softap_deinit_tx_rx(struct hdd_adapter *adapter)
{
	QDF_BUG(adapter);
	if (!adapter)
		return QDF_STATUS_E_FAILURE;

	adapter->tx_fn = NULL;

	return QDF_STATUS_SUCCESS;
}

static void
hdd_reset_sta_info_during_reattach(struct hdd_station_info *sta_info)
{
	sta_info->in_use = 0;
	sta_info->sta_id = 0;
	sta_info->sta_type = 0;
	qdf_mem_zero(&sta_info->sta_mac, QDF_MAC_ADDR_SIZE);
	sta_info->peer_state = 0;
	sta_info->is_qos_enabled = 0;
	sta_info->is_deauth_in_progress = 0;
	sta_info->nss = 0;
	sta_info->rate_flags = 0;
	sta_info->ecsa_capable = 0;
	sta_info->max_phy_rate = 0;
	sta_info->tx_packets = 0;
	sta_info->tx_bytes = 0;
	sta_info->rx_packets = 0;
	sta_info->rx_bytes = 0;
	sta_info->last_tx_rx_ts = 0;
	sta_info->assoc_ts = 0;
	sta_info->disassoc_ts = 0;
	sta_info->tx_rate = 0;
	sta_info->rx_rate = 0;
	sta_info->ampdu = 0;
	sta_info->sgi_enable = 0;
	sta_info->tx_stbc = 0;
	sta_info->rx_stbc = 0;
	sta_info->ch_width = 0;
	sta_info->mode = 0;
	sta_info->max_supp_idx = 0;
	sta_info->max_ext_idx = 0;
	sta_info->max_mcs_idx = 0;
	sta_info->rx_mcs_map = 0;
	sta_info->tx_mcs_map = 0;
	sta_info->freq = 0;
	sta_info->dot11_mode = 0;
	sta_info->ht_present = 0;
	sta_info->vht_present = 0;
	qdf_mem_zero(&sta_info->ht_caps, sizeof(sta_info->ht_caps));
	qdf_mem_zero(&sta_info->vht_caps, sizeof(sta_info->vht_caps));
	sta_info->reason_code = 0;
	sta_info->rssi = 0;
	sta_info->dhcp_phase = 0;
	sta_info->dhcp_nego_status = 0;
	sta_info->capability = 0;
	sta_info->support_mode = 0;
	sta_info->rx_retry_cnt = 0;
	sta_info->rx_mc_bc_cnt = 0;

	if (sta_info->assoc_req_ies.len) {
		qdf_mem_free(sta_info->assoc_req_ies.data);
		sta_info->assoc_req_ies.data = NULL;
		sta_info->assoc_req_ies.len = 0;
	}

	sta_info->pending_eap_frm_type = 0;
}

/**
 * hdd_sta_info_re_attach() - Re-Attach the station info structure into the list
 * @sta_info_container: The station info container obj that stores and maintains
 *                      the sta_info obj.
 * @sta_info: The station info structure that is to be attached to the
 *            container object.
 *
 * This function re-attaches the station if it gets re-connect after
 * disconnecting and before its all references are released.
 *
 * Return: QDF STATUS SUCCESS on successful attach, error code otherwise
 */
static QDF_STATUS hdd_sta_info_re_attach(
				struct hdd_sta_info_obj *sta_info_container,
				struct hdd_station_info *sta_info,
				struct qdf_mac_addr *sta_mac)
{
	if (!sta_info_container || !sta_info) {
		hdd_err("Parameter(s) null");
		return QDF_STATUS_E_INVAL;
	}

	qdf_spin_lock_bh(&sta_info_container->sta_obj_lock);

	if (sta_info->is_attached) {
		qdf_spin_unlock_bh(&sta_info_container->sta_obj_lock);
		hdd_err("sta info is alredy attached");
		return QDF_STATUS_SUCCESS;
	}

	hdd_reset_sta_info_during_reattach(sta_info);
	/* Add one extra ref for reattach */
	hdd_take_sta_info_ref(sta_info_container, sta_info, false,
			      STA_INFO_ATTACH_DETACH);
	qdf_mem_copy(&sta_info->sta_mac, sta_mac, sizeof(struct qdf_mac_addr));
	sta_info->is_attached = true;
	qdf_spin_unlock_bh(&sta_info_container->sta_obj_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_softap_init_tx_rx_sta(struct hdd_adapter *adapter,
				     struct qdf_mac_addr *sta_mac)
{
	struct hdd_station_info *sta_info;
	QDF_STATUS status;

	sta_info = hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					   sta_mac->bytes,
					   STA_INFO_SOFTAP_INIT_TX_RX_STA);

	if (sta_info) {
		hdd_err("Reinit of in use station " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(sta_mac->bytes));
		status = hdd_sta_info_re_attach(&adapter->sta_info_list,
						sta_info, sta_mac);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
				     STA_INFO_SOFTAP_INIT_TX_RX_STA);
		return status;
	}

	sta_info = qdf_mem_malloc(sizeof(struct hdd_station_info));
	if (!sta_info)
		return QDF_STATUS_E_NOMEM;

	sta_info->is_deauth_in_progress = false;
	qdf_mem_copy(&sta_info->sta_mac, sta_mac, sizeof(struct qdf_mac_addr));

	status = hdd_sta_info_attach(&adapter->sta_info_list, sta_info);
	if (QDF_IS_STATUS_ERROR(status)) {
		hdd_err("Failed to attach station: " QDF_MAC_ADDR_FMT,
			QDF_MAC_ADDR_REF(sta_mac->bytes));
		qdf_mem_free(sta_info);
	}

	return status;
}

/**
 * hdd_softap_tsf_timestamp_rx() - time stamp Rx netbuf
 * @context: pointer to HDD context
 * @netbuf: pointer to a Rx netbuf
 *
 * Return: None
 */
#ifdef WLAN_FEATURE_TSF_PLUS
static inline void hdd_softap_tsf_timestamp_rx(struct hdd_context *hdd_ctx,
					       qdf_nbuf_t netbuf)
{
	uint64_t target_time;

	if (!hdd_tsf_is_rx_set(hdd_ctx))
		return;

	target_time = ktime_to_us(netbuf->tstamp);
	hdd_rx_timestamp(netbuf, target_time);
}
#else
static inline void hdd_softap_tsf_timestamp_rx(struct hdd_context *hdd_ctx,
					       qdf_nbuf_t netbuf)
{
}
#endif

/**
 * hdd_softap_notify_tx_compl_cbk() - callback to notify tx completion
 * @skb: pointer to skb data
 * @adapter: pointer to vdev apdapter
 * @flags: tx status flag
 *
 * Return: None
 */
static void hdd_softap_notify_tx_compl_cbk(struct sk_buff *skb,
					   void *context, uint16_t flag)
{
	int errno;
	struct hdd_adapter *adapter = context;

	errno = hdd_validate_adapter(adapter);
	if (errno)
		return;

	if (QDF_NBUF_CB_PACKET_TYPE_DHCP == QDF_NBUF_CB_GET_PACKET_TYPE(skb)) {
		hdd_debug("sending DHCP indication");
		hdd_softap_notify_dhcp_ind(context, skb);
	} else if (QDF_NBUF_CB_GET_PACKET_TYPE(skb) ==
						QDF_NBUF_CB_PACKET_TYPE_EAPOL) {
		hdd_softap_inspect_tx_eap_pkt(adapter, skb, true);
	}
}

QDF_STATUS hdd_softap_rx_packet_cbk(void *adapter_context, qdf_nbuf_t rx_buf)
{
	struct hdd_adapter *adapter = NULL;
	QDF_STATUS qdf_status;
	unsigned int cpu_index;
	struct sk_buff *skb = NULL;
	struct sk_buff *next = NULL;
	struct hdd_context *hdd_ctx = NULL;
	struct qdf_mac_addr *src_mac;
	struct hdd_station_info *sta_info;

	/* Sanity check on inputs */
	if (unlikely((!adapter_context) || (!rx_buf))) {
		QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
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
		QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA, QDF_TRACE_LEVEL_ERROR,
			  "%s: HDD context is Null", __func__);
		return QDF_STATUS_E_FAILURE;
	}

	/* walk the chain until all are processed */
	next = (struct sk_buff *)rx_buf;

	while (next) {
		skb = next;
		next = skb->next;
		skb->next = NULL;

/* Debug code, remove later */
#if defined(QCA_WIFI_QCA6290) || defined(QCA_WIFI_QCA6390) || \
    defined(QCA_WIFI_QCA6490) || defined(QCA_WIFI_QCA6750)
		QDF_TRACE(QDF_MODULE_ID_HDD_DATA, QDF_TRACE_LEVEL_DEBUG,
			 "%s: skb %pK skb->len %d\n", __func__, skb, skb->len);
#endif

		hdd_softap_dump_sk_buff(skb);

		skb->dev = adapter->dev;

		if (unlikely(!skb->dev)) {
			QDF_TRACE(QDF_MODULE_ID_HDD_SAP_DATA,
				  QDF_TRACE_LEVEL_ERROR,
				  "%s: ERROR!!Invalid netdevice", __func__);
			qdf_nbuf_free(skb);
			continue;
		}
		cpu_index = wlan_hdd_get_cpu();
		++adapter->hdd_stats.tx_rx_stats.rx_packets[cpu_index];
		++adapter->stats.rx_packets;
		/* count aggregated RX frame into stats */
		adapter->stats.rx_packets += qdf_nbuf_get_gso_segs(skb);
		adapter->stats.rx_bytes += skb->len;

		/* Send DHCP Indication to FW */
		src_mac = (struct qdf_mac_addr *)(skb->data +
						  QDF_NBUF_SRC_MAC_OFFSET);
		sta_info = hdd_get_sta_info_by_mac(
					&adapter->sta_info_list,
					(uint8_t *)src_mac,
					STA_INFO_SOFTAP_RX_PACKET_CBK);

		if (sta_info) {
			sta_info->rx_packets++;
			sta_info->rx_bytes += skb->len;
			sta_info->last_tx_rx_ts = qdf_system_ticks();
			hdd_softap_inspect_dhcp_packet(adapter, skb, QDF_RX);
			hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info,
					     true,
					     STA_INFO_SOFTAP_RX_PACKET_CBK);
		}

		if (qdf_unlikely(qdf_nbuf_is_ipv4_eapol_pkt(skb) &&
				 qdf_mem_cmp(qdf_nbuf_data(skb) +
					     QDF_NBUF_DEST_MAC_OFFSET,
					     adapter->mac_addr.bytes,
					     QDF_MAC_ADDR_SIZE))) {
			qdf_nbuf_free(skb);
			continue;
		}

		hdd_event_eapol_log(skb, QDF_RX);
		qdf_dp_trace_log_pkt(adapter->vdev_id,
				     skb, QDF_RX, QDF_TRACE_DEFAULT_PDEV_ID);
		DPTRACE(qdf_dp_trace(skb,
			QDF_DP_TRACE_RX_HDD_PACKET_PTR_RECORD,
			QDF_TRACE_DEFAULT_PDEV_ID,
			qdf_nbuf_data_addr(skb),
			sizeof(qdf_nbuf_data(skb)), QDF_RX));
		DPTRACE(qdf_dp_trace_data_pkt(skb, QDF_TRACE_DEFAULT_PDEV_ID,
				QDF_DP_TRACE_RX_PACKET_RECORD, 0, QDF_RX));

		skb->protocol = eth_type_trans(skb, skb->dev);

		/* hold configurable wakelock for unicast traffic */
		if (!hdd_is_current_high_throughput(hdd_ctx) &&
		    hdd_ctx->config->rx_wakelock_timeout &&
		    skb->pkt_type != PACKET_BROADCAST &&
		    skb->pkt_type != PACKET_MULTICAST) {
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

		hdd_softap_tsf_timestamp_rx(hdd_ctx, skb);

		qdf_status = hdd_rx_deliver_to_stack(adapter, skb);

		if (QDF_IS_STATUS_SUCCESS(qdf_status))
			++adapter->hdd_stats.tx_rx_stats.rx_delivered[cpu_index];
		else
			++adapter->hdd_stats.tx_rx_stats.rx_refused[cpu_index];
	}

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_softap_deregister_sta(struct hdd_adapter *adapter,
				     struct hdd_station_info **sta_info)
{
	struct hdd_context *hdd_ctx;
	struct qdf_mac_addr *mac_addr;
	struct hdd_station_info *sta = *sta_info;

	if (!adapter) {
		hdd_err("NULL adapter");
		return QDF_STATUS_E_INVAL;
	}

	if (WLAN_HDD_ADAPTER_MAGIC != adapter->magic) {
		hdd_err("Invalid adapter magic");
		return QDF_STATUS_E_INVAL;
	}

	if (!sta) {
		hdd_err("Invalid station");
		return QDF_STATUS_E_INVAL;
	}

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	if (!hdd_ctx) {
		hdd_err("HDD context is null");
		return QDF_STATUS_E_INVAL;
	}

	/*
	 * If the address is a broadcast address then the CDP layers expects
	 * the self mac address of the adapter.
	 */
	if (QDF_IS_ADDR_BROADCAST(sta->sta_mac.bytes))
		mac_addr = &adapter->mac_addr;
	else
		mac_addr = &sta->sta_mac;

	if (ucfg_ipa_is_enabled()) {
		if (ucfg_ipa_wlan_evt(hdd_ctx->pdev, adapter->dev,
				      adapter->device_mode,
				      adapter->vdev_id,
				      WLAN_IPA_CLIENT_DISCONNECT,
				      mac_addr->bytes) != QDF_STATUS_SUCCESS)
			hdd_debug("WLAN_CLIENT_DISCONNECT event failed");
	}

	hdd_del_latency_critical_client(adapter, sta->dot11_mode);
	hdd_sta_info_detach(&adapter->sta_info_list, &sta);

	ucfg_mlme_update_oce_flags(hdd_ctx->pdev);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS hdd_softap_register_sta(struct hdd_adapter *adapter,
				   bool auth_required,
				   bool privacy_required,
				   struct qdf_mac_addr *sta_mac,
				   tSap_StationAssocReassocCompleteEvent *event)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	struct ol_txrx_desc_type txrx_desc = {0};
	struct hdd_context *hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	struct ol_txrx_ops txrx_ops;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
	struct hdd_ap_ctx *ap_ctx;
	struct hdd_station_info *sta_info;
	bool wmm_enabled = false;
	enum qca_wlan_802_11_mode dot11mode = QCA_WLAN_802_11_MODE_INVALID;

	if (event) {
		wmm_enabled = event->wmmEnabled;
		dot11mode = hdd_convert_dot11mode_from_phymode(event->chan_info.info);
	}

	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);

	/*
	 * If the address is a broadcast address, then provide the self mac addr
	 * to the data path. Else provide the mac address of the connected peer.
	 */
	if (qdf_is_macaddr_broadcast(sta_mac) && ap_ctx)
		qdf_mem_copy(&txrx_desc.peer_addr, &adapter->mac_addr,
			     QDF_MAC_ADDR_SIZE);
	else
		qdf_mem_copy(&txrx_desc.peer_addr, sta_mac,
			     QDF_MAC_ADDR_SIZE);

	qdf_status = hdd_softap_init_tx_rx_sta(adapter, sta_mac);
	sta_info = hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					   sta_mac->bytes,
					   STA_INFO_SOFTAP_REGISTER_STA);

	if (!sta_info) {
		hdd_debug("STA not found");
		return QDF_STATUS_E_INVAL;
	}

	txrx_desc.is_qos_enabled = wmm_enabled;
	hdd_add_latency_critical_client(adapter, dot11mode);

	/* Register the vdev transmit and receive functions */
	qdf_mem_zero(&txrx_ops, sizeof(txrx_ops));

	txrx_ops.tx.tx_comp = hdd_softap_notify_tx_compl_cbk;

	if (adapter->hdd_ctx->enable_dp_rx_threads) {
		txrx_ops.rx.rx = hdd_rx_pkt_thread_enqueue_cbk;
		txrx_ops.rx.rx_stack = hdd_softap_rx_packet_cbk;
		txrx_ops.rx.rx_flush = hdd_rx_flush_packet_cbk;
		txrx_ops.rx.rx_gro_flush = hdd_rx_thread_gro_flush_ind_cbk;
		adapter->rx_stack = hdd_softap_rx_packet_cbk;
	} else {
		txrx_ops.rx.rx = hdd_softap_rx_packet_cbk;
		txrx_ops.rx.rx_stack = NULL;
		txrx_ops.rx.rx_flush = NULL;
	}

	cdp_vdev_register(soc,
			  adapter->vdev_id,
			  (ol_osif_vdev_handle)adapter,
			  &txrx_ops);
	adapter->tx_fn = txrx_ops.tx.tx;

	qdf_status = cdp_peer_register(soc, OL_TXRX_PDEV_ID, &txrx_desc);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		hdd_debug("cdp_peer_register() failed to register.  Status = %d [0x%08X]",
			  qdf_status, qdf_status);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
				     STA_INFO_SOFTAP_REGISTER_STA);
		return qdf_status;
	}

	/* if ( WPA ), tell TL to go to 'connected' and after keys come to the
	 * driver then go to 'authenticated'.  For all other authentication
	 * types (those that do not require upper layer authentication) we can
	 * put TL directly into 'authenticated' state
	 */

	sta_info->is_qos_enabled = wmm_enabled;

	if (!auth_required) {
		hdd_debug("open/shared auth STA MAC= " QDF_MAC_ADDR_FMT
			  ".  Changing TL state to AUTHENTICATED at Join time",
			 QDF_MAC_ADDR_REF(sta_info->sta_mac.bytes));

		/* Connections that do not need Upper layer auth,
		 * transition TL directly to 'Authenticated' state.
		 */
		qdf_status = hdd_change_peer_state(adapter,
						   txrx_desc.peer_addr.bytes,
						   OL_TXRX_PEER_STATE_AUTH);

		sta_info->peer_state = OL_TXRX_PEER_STATE_AUTH;
		if (!qdf_is_macaddr_broadcast(sta_mac))
			qdf_status = wlan_hdd_send_sta_authorized_event(
							adapter, hdd_ctx,
							sta_mac);
	} else {

		hdd_debug("ULA auth STA MAC = " QDF_MAC_ADDR_FMT
			  ".  Changing TL state to CONNECTED at Join time",
			 QDF_MAC_ADDR_REF(sta_info->sta_mac.bytes));

		qdf_status = hdd_change_peer_state(adapter,
						   txrx_desc.peer_addr.bytes,
						   OL_TXRX_PEER_STATE_CONN);

		sta_info->peer_state = OL_TXRX_PEER_STATE_CONN;
	}

	hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
			     STA_INFO_SOFTAP_REGISTER_STA);
	hdd_debug("Enabling queues");
	wlan_hdd_netif_queue_control(adapter,
				   WLAN_START_ALL_NETIF_QUEUE_N_CARRIER,
				   WLAN_CONTROL_PATH);
	ucfg_mlme_update_oce_flags(hdd_ctx->pdev);
	ucfg_mlme_init_twt_context(hdd_ctx->psoc, sta_mac,
				   WLAN_ALL_SESSIONS_DIALOG_ID);
	return qdf_status;
}

/**
 * hdd_softap_register_bc_sta() - Register the SoftAP broadcast STA
 * @adapter: pointer to adapter context
 * @privacy_required: should 802.11 privacy bit be set?
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS hdd_softap_register_bc_sta(struct hdd_adapter *adapter,
				      bool privacy_required)
{
	QDF_STATUS qdf_status = QDF_STATUS_E_FAILURE;
	struct qdf_mac_addr broadcast_macaddr = QDF_MAC_ADDR_BCAST_INIT;
	struct hdd_ap_ctx *ap_ctx;
	uint8_t sta_id;
	ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
	sta_id = ap_ctx->broadcast_sta_id;

	if (sta_id >= WLAN_MAX_STA_COUNT) {
		hdd_err("Error: Invalid sta_id: %u", sta_id);
		return qdf_status;
	}

	qdf_status = hdd_softap_register_sta(adapter, false,
					     privacy_required,
					     &broadcast_macaddr, NULL);

	return qdf_status;
}

QDF_STATUS hdd_softap_stop_bss(struct hdd_adapter *adapter)
{
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	uint8_t indoor_chnl_marking = 0;
	struct hdd_context *hdd_ctx;
	struct hdd_station_info *sta_info, *tmp = NULL;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);

	status = ucfg_policy_mgr_get_indoor_chnl_marking(hdd_ctx->psoc,
							 &indoor_chnl_marking);
	if (QDF_STATUS_SUCCESS != status)
		hdd_err("can't get indoor channel marking, using default");
	/* This is stop bss callback running in scheduler thread so do not
	 * driver unload in progress check otherwise it can lead to peer
	 * object leak
	 */

	hdd_for_each_sta_ref_safe(adapter->sta_info_list, sta_info, tmp,
				  STA_INFO_SOFTAP_STOP_BSS) {
		status = hdd_softap_deregister_sta(adapter, &sta_info);
		hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
				     STA_INFO_SOFTAP_STOP_BSS);
	}

	if (adapter->device_mode == QDF_SAP_MODE &&
	    !hdd_ctx->config->disable_channel)
		wlan_hdd_restore_channels(hdd_ctx);

	/*  Mark the indoor channel (passive) to enable  */
	if (indoor_chnl_marking && adapter->device_mode == QDF_SAP_MODE) {
		hdd_update_indoor_channel(hdd_ctx, false);
		sme_update_channel_list(hdd_ctx->mac_handle);
	}

	if (ucfg_ipa_is_enabled()) {
		if (ucfg_ipa_wlan_evt(hdd_ctx->pdev,
				      adapter->dev,
				      adapter->device_mode,
				      adapter->vdev_id,
				      WLAN_IPA_AP_DISCONNECT,
				      adapter->dev->dev_addr) !=
		    QDF_STATUS_SUCCESS)
			hdd_err("WLAN_AP_DISCONNECT event failed");
	}

	/* Setting the RTS profile to original value */
	if (sme_cli_set_command(adapter->vdev_id, WMI_VDEV_PARAM_ENABLE_RTSCTS,
				cfg_get(hdd_ctx->psoc,
					CFG_ENABLE_FW_RTS_PROFILE),
				VDEV_CMD))
		hdd_debug("Failed to set RTS_PROFILE");

	return status;
}

QDF_STATUS hdd_softap_change_sta_state(struct hdd_adapter *adapter,
				       struct qdf_mac_addr *sta_mac,
				       enum ol_txrx_peer_state state)
{
	QDF_STATUS qdf_status;
	struct hdd_station_info *sta_info;
	struct qdf_mac_addr mac_addr;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter_dev(adapter->dev);

	sta_info = hdd_get_sta_info_by_mac(&adapter->sta_info_list,
					   sta_mac->bytes,
					   STA_INFO_SOFTAP_CHANGE_STA_STATE);

	if (!sta_info) {
		hdd_debug("Failed to find right station MAC: " QDF_MAC_ADDR_FMT,
			  QDF_MAC_ADDR_REF(sta_mac->bytes));
		return QDF_STATUS_E_INVAL;
	}

	if (qdf_is_macaddr_broadcast(&sta_info->sta_mac))
		qdf_mem_copy(&mac_addr, &adapter->mac_addr, QDF_MAC_ADDR_SIZE);
	else
		qdf_mem_copy(&mac_addr, sta_mac, QDF_MAC_ADDR_SIZE);

	qdf_status =
		hdd_change_peer_state(adapter, mac_addr.bytes, state);
	hdd_debug("Station " QDF_MAC_ADDR_FMT " changed to state %d",
		  QDF_MAC_ADDR_REF(mac_addr.bytes), state);

	if (QDF_STATUS_SUCCESS == qdf_status) {
		sta_info->peer_state = OL_TXRX_PEER_STATE_AUTH;
		vdev = hdd_objmgr_get_vdev(adapter);
		if (vdev) {
			p2p_peer_authorized(vdev, sta_mac->bytes);
			hdd_objmgr_put_vdev(vdev);
		} else {
			hdd_err("vdev is NULL");
		}
	}

	hdd_put_sta_info_ref(&adapter->sta_info_list, &sta_info, true,
			     STA_INFO_SOFTAP_CHANGE_STA_STATE);
	hdd_exit();
	return qdf_status;
}

