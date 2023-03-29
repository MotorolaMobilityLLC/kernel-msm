/*
 * Copyright (c) 2013-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_ipa.c
 *
 * WLAN HDD and ipa interface implementation
 */

/* Include Files */
#include <wlan_hdd_includes.h>
#include <wlan_hdd_ipa.h>
#include "wlan_policy_mgr_api.h"
#include "wlan_ipa_ucfg_api.h"
#include <wlan_hdd_softap_tx_rx.h>
#include <linux/inetdevice.h>
#include <qdf_trace.h>

void hdd_ipa_set_tx_flow_info(void)
{
	struct hdd_adapter *adapter, *next_adapter = NULL;
	struct hdd_station_ctx *sta_ctx;
	struct hdd_ap_ctx *hdd_ap_ctx;
	struct hdd_hostapd_state *hostapd_state;
	struct qdf_mac_addr staBssid = QDF_MAC_ADDR_ZERO_INIT;
	struct qdf_mac_addr p2pBssid = QDF_MAC_ADDR_ZERO_INIT;
	struct qdf_mac_addr apBssid = QDF_MAC_ADDR_ZERO_INIT;
	uint8_t staChannel = 0, p2pChannel = 0, apChannel = 0;
	const char *p2pMode = "DEV";
	struct hdd_context *hdd_ctx;
	struct cds_context *cds_ctx;
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
	uint8_t targetChannel = 0;
	uint8_t preAdapterChannel = 0;
	uint8_t channel24;
	uint8_t channel5;
	struct hdd_adapter *preAdapterContext = NULL;
	struct hdd_adapter *adapter2_4 = NULL;
	struct hdd_adapter *adapter5 = NULL;
	void *soc = cds_get_context(QDF_MODULE_ID_SOC);
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
	struct wlan_objmgr_psoc *psoc;
	wlan_net_dev_ref_dbgid dbgid = NET_DEV_HOLD_IPA_SET_TX_FLOW_INFO;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return;
	}

	cds_ctx = cds_get_context(QDF_MODULE_ID_QDF);
	if (!cds_ctx) {
		hdd_err("Invalid CDS Context");
		return;
	}

	psoc = hdd_ctx->psoc;

	hdd_for_each_adapter_dev_held_safe(hdd_ctx, adapter, next_adapter,
					   dbgid) {
		switch (adapter->device_mode) {
		case QDF_STA_MODE:
			sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
			if (eConnectionState_Associated ==
			    sta_ctx->conn_info.conn_state) {
				staChannel = wlan_reg_freq_to_chan(
						hdd_ctx->pdev,
						sta_ctx->conn_info.chan_freq);
				qdf_copy_macaddr(&staBssid,
						 &sta_ctx->conn_info.bssid);
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
				targetChannel = staChannel;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
			}
			break;
		case QDF_P2P_CLIENT_MODE:
			sta_ctx = WLAN_HDD_GET_STATION_CTX_PTR(adapter);
			if (eConnectionState_Associated ==
			    sta_ctx->conn_info.conn_state) {
				p2pChannel = wlan_reg_freq_to_chan(
					hdd_ctx->pdev,
					sta_ctx->conn_info.chan_freq);
				qdf_copy_macaddr(&p2pBssid,
						&sta_ctx->conn_info.bssid);
				p2pMode = "CLI";
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
				targetChannel = p2pChannel;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
			}
			break;
		case QDF_P2P_GO_MODE:
			hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
			hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);
			if (hostapd_state->bss_state == BSS_START
			    && hostapd_state->qdf_status ==
			    QDF_STATUS_SUCCESS) {
				p2pChannel = wlan_reg_freq_to_chan(
					hdd_ctx->pdev,
					hdd_ap_ctx->operating_chan_freq);
				qdf_copy_macaddr(&p2pBssid,
						 &adapter->mac_addr);
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
				targetChannel = p2pChannel;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
			}
			p2pMode = "GO";
			break;
		case QDF_SAP_MODE:
			hdd_ap_ctx = WLAN_HDD_GET_AP_CTX_PTR(adapter);
			hostapd_state = WLAN_HDD_GET_HOSTAP_STATE_PTR(adapter);
			if (hostapd_state->bss_state == BSS_START
			    && hostapd_state->qdf_status ==
			    QDF_STATUS_SUCCESS) {
				apChannel = wlan_reg_freq_to_chan(
					hdd_ctx->pdev,
					hdd_ap_ctx->operating_chan_freq);
				qdf_copy_macaddr(&apBssid,
						&adapter->mac_addr);
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
				targetChannel = apChannel;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
			}
			break;
		default:
			break;
		}
#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
		if (targetChannel) {
			/*
			 * This is first adapter detected as active
			 * set as default for none concurrency case
			 */
			if (!preAdapterChannel) {
				/* If IPA UC data path is enabled,
				 * target should reserve extra tx descriptors
				 * for IPA data path.
				 * Then host data path should allow less TX
				 * packet pumping in case IPA
				 * data path enabled
				 */
				if (ucfg_ipa_uc_is_enabled() &&
				    (QDF_SAP_MODE == adapter->device_mode)) {
					adapter->tx_flow_low_watermark =
					hdd_ctx->config->tx_flow_low_watermark +
					WLAN_TFC_IPAUC_TX_DESC_RESERVE;
				} else {
					adapter->tx_flow_low_watermark =
						hdd_ctx->config->
							tx_flow_low_watermark;
				}
				adapter->tx_flow_hi_watermark_offset =
				   hdd_ctx->config->tx_flow_hi_watermark_offset;
				cdp_fc_ll_set_tx_pause_q_depth(soc,
						adapter->vdev_id,
						hdd_ctx->config->
						tx_flow_max_queue_depth);
				hdd_info("MODE %d,CH %d,LWM %d,HWM %d,TXQDEP %d",
				    adapter->device_mode,
				    targetChannel,
				    adapter->tx_flow_low_watermark,
				    adapter->tx_flow_low_watermark +
				    adapter->tx_flow_hi_watermark_offset,
				    hdd_ctx->config->tx_flow_max_queue_depth);
				preAdapterChannel = targetChannel;
				preAdapterContext = adapter;
			} else {
				/*
				 * SCC, disable TX flow control for both
				 * SCC each adapter cannot reserve dedicated
				 * channel resource, as a result, if any adapter
				 * blocked OS Q by flow control,
				 * blocked adapter will lost chance to recover
				 */
				if (preAdapterChannel == targetChannel) {
					/* Current adapter */
					adapter->tx_flow_low_watermark = 0;
					adapter->
					tx_flow_hi_watermark_offset = 0;
					cdp_fc_ll_set_tx_pause_q_depth(soc,
						adapter->vdev_id,
						hdd_ctx->config->
						tx_hbw_flow_max_queue_depth);
					hdd_info("SCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
					       qdf_opmode_str(
							adapter->device_mode),
					       adapter->device_mode,
					       targetChannel,
					       adapter->tx_flow_low_watermark,
					       adapter->tx_flow_low_watermark +
					       adapter->
					       tx_flow_hi_watermark_offset,
					       hdd_ctx->config->
					       tx_hbw_flow_max_queue_depth);

					if (!preAdapterContext) {
						hdd_err("SCC: Previous adapter context NULL");
						hdd_adapter_dev_put_debug(
								adapter, dbgid);
						continue;
					}

					/* Previous adapter */
					preAdapterContext->
					tx_flow_low_watermark = 0;
					preAdapterContext->
					tx_flow_hi_watermark_offset = 0;
					cdp_fc_ll_set_tx_pause_q_depth(soc,
						preAdapterContext->vdev_id,
						hdd_ctx->config->
						tx_hbw_flow_max_queue_depth);
					hdd_info("SCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
					       qdf_opmode_str(
						preAdapterContext->device_mode),
					       preAdapterContext->device_mode,
					       targetChannel,
					       preAdapterContext->
					       tx_flow_low_watermark,
					       preAdapterContext->
					       tx_flow_low_watermark +
					       preAdapterContext->
					       tx_flow_hi_watermark_offset,
					       hdd_ctx->config->
					       tx_hbw_flow_max_queue_depth);
				}
				/*
				 * MCC, each adapter will have dedicated
				 * resource
				 */
				else {
					/* current channel is 2.4 */
					if (targetChannel <=
				     WLAN_HDD_TX_FLOW_CONTROL_MAX_24BAND_CH) {
						channel24 = targetChannel;
						channel5 = preAdapterChannel;
						adapter2_4 = adapter;
						adapter5 = preAdapterContext;
					} else {
						/* Current channel is 5 */
						channel24 = preAdapterChannel;
						channel5 = targetChannel;
						adapter2_4 = preAdapterContext;
						adapter5 = adapter;
					}

					if (!adapter5) {
						hdd_err("MCC: 5GHz adapter context NULL");
						hdd_adapter_dev_put_debug(
								adapter, dbgid);
						continue;
					}
					adapter5->tx_flow_low_watermark =
						hdd_ctx->config->
						tx_hbw_flow_low_watermark;
					adapter5->
					tx_flow_hi_watermark_offset =
						hdd_ctx->config->
						tx_hbw_flow_hi_watermark_offset;
					cdp_fc_ll_set_tx_pause_q_depth(soc,
						adapter5->vdev_id,
						hdd_ctx->config->
						tx_hbw_flow_max_queue_depth);
					hdd_info("MCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
					    qdf_opmode_str(
						    adapter5->device_mode),
					    adapter5->device_mode,
					    channel5,
					    adapter5->tx_flow_low_watermark,
					    adapter5->
					    tx_flow_low_watermark +
					    adapter5->
					    tx_flow_hi_watermark_offset,
					    hdd_ctx->config->
					    tx_hbw_flow_max_queue_depth);

					if (!adapter2_4) {
						hdd_err("MCC: 2.4GHz adapter context NULL");
						hdd_adapter_dev_put_debug(
								adapter, dbgid);
						continue;
					}
					adapter2_4->tx_flow_low_watermark =
						hdd_ctx->config->
						tx_lbw_flow_low_watermark;
					adapter2_4->
					tx_flow_hi_watermark_offset =
						hdd_ctx->config->
						tx_lbw_flow_hi_watermark_offset;
					cdp_fc_ll_set_tx_pause_q_depth(soc,
						adapter2_4->vdev_id,
						hdd_ctx->config->
						tx_lbw_flow_max_queue_depth);
					hdd_info("MCC: MODE %s(%d), CH %d, LWM %d, HWM %d, TXQDEP %d",
						qdf_opmode_str(
						    adapter2_4->device_mode),
						adapter2_4->device_mode,
						channel24,
						adapter2_4->
						tx_flow_low_watermark,
						adapter2_4->
						tx_flow_low_watermark +
						adapter2_4->
						tx_flow_hi_watermark_offset,
						hdd_ctx->config->
						tx_lbw_flow_max_queue_depth);

				}
			}
		}
		targetChannel = 0;
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */
		hdd_adapter_dev_put_debug(adapter, dbgid);
	}
}

#if defined(QCA_CONFIG_SMP) && defined(PF_WAKE_UP_IDLE)
/**
 * hdd_ipa_get_wake_up_idle() - Get PF_WAKE_UP_IDLE flag in the task structure
 *
 * Get PF_WAKE_UP_IDLE flag in the task structure
 *
 * Return: 1 if PF_WAKE_UP_IDLE flag is set, 0 otherwise
 */
static uint32_t hdd_ipa_get_wake_up_idle(void)
{
	return sched_get_wake_up_idle(current);
}

/**
 * hdd_ipa_set_wake_up_idle() - Set PF_WAKE_UP_IDLE flag in the task structure
 *
 * Set PF_WAKE_UP_IDLE flag in the task structure
 * This task and any task woken by this will be waken to idle CPU
 *
 * Return: None
 */
static void hdd_ipa_set_wake_up_idle(bool wake_up_idle)
{
	sched_set_wake_up_idle(current, wake_up_idle);
}
#else
static uint32_t hdd_ipa_get_wake_up_idle(void)
{
	return 0;
}

static void hdd_ipa_set_wake_up_idle(bool wake_up_idle)
{
}
#endif

/**
 * hdd_ipa_send_to_nw_stack() - Check if IPA supports NAPI
 * polling during RX
 * @skb : data buffer sent to network stack
 *
 * If IPA LAN RX supports NAPI polling mechanism use
 * netif_receive_skb instead of netif_rx_ni to forward the skb
 * to network stack.
 *
 * Return: Return value from netif_rx_ni/netif_receive_skb
 */
static int hdd_ipa_send_to_nw_stack(qdf_nbuf_t skb)
{
	int result;

	if (qdf_ipa_get_lan_rx_napi())
		result = netif_receive_skb(skb);
	else
		result = netif_rx_ni(skb);
	return result;
}

#ifdef QCA_CONFIG_SMP

/**
 * hdd_ipa_aggregated_rx_ind() - Submit aggregated packets to the stack
 * @skb: skb to be submitted to the stack
 *
 * For CONFIG_SMP systems, simply call netif_rx_ni.
 * For non CONFIG_SMP systems call netif_rx till
 * IPA_WLAN_RX_SOFTIRQ_THRESH. When threshold is reached call netif_rx_ni.
 * In this manner, UDP/TCP packets are sent in an aggregated way to the stack.
 * For IP/ICMP packets, simply call netif_rx_ni.
 *
 * Check if IPA supports NAPI polling then use netif_receive_skb
 * instead of netif_rx_ni.
 *
 * Return: return value from the netif_rx_ni/netif_rx api.
 */
static int hdd_ipa_aggregated_rx_ind(qdf_nbuf_t skb)
{
	int ret;

	ret =  hdd_ipa_send_to_nw_stack(skb);
	return ret;
}
#else
static int hdd_ipa_aggregated_rx_ind(qdf_nbuf_t skb)
{
	struct iphdr *ip_h;
	static atomic_t softirq_mitigation_cntr =
		ATOMIC_INIT(IPA_WLAN_RX_SOFTIRQ_THRESH);
	int result;

	ip_h = (struct iphdr *)(skb->data);
	if ((skb->protocol == htons(ETH_P_IP)) &&
		(ip_h->protocol == IPPROTO_ICMP)) {
		result = hdd_ipa_send_to_nw_stack(skb);
	} else {
		/* Call netif_rx_ni for every IPA_WLAN_RX_SOFTIRQ_THRESH packets
		 * to avoid excessive softirq's.
		 */
		if (atomic_dec_and_test(&softirq_mitigation_cntr)) {
			result = hdd_ipa_send_to_nw_stack(skb);
			atomic_set(&softirq_mitigation_cntr,
					IPA_WLAN_RX_SOFTIRQ_THRESH);
		} else {
			result = netif_rx(skb);
		}
	}

	return result;
}
#endif

void hdd_ipa_send_nbuf_to_network(qdf_nbuf_t nbuf, qdf_netdev_t dev)
{
	struct hdd_adapter *adapter = (struct hdd_adapter *) netdev_priv(dev);
	int result;
	unsigned int cpu_index;
	uint32_t enabled;

	if (hdd_validate_adapter(adapter)) {
		kfree_skb(nbuf);
		return;
	}

	if (cds_is_driver_unloading()) {
		kfree_skb(nbuf);
		return;
	}

	hdd_ipa_update_rx_mcbc_stats(adapter, nbuf);

	if ((adapter->device_mode == QDF_SAP_MODE) &&
	    (qdf_nbuf_is_ipv4_dhcp_pkt(nbuf) == true)) {
		/* Send DHCP Indication to FW */
		hdd_softap_inspect_dhcp_packet(adapter, nbuf, QDF_RX);
	}

	qdf_dp_trace_set_track(nbuf, QDF_RX);

	hdd_event_eapol_log(nbuf, QDF_RX);
	qdf_dp_trace_log_pkt(adapter->vdev_id,
			     nbuf, QDF_RX, QDF_TRACE_DEFAULT_PDEV_ID);
	DPTRACE(qdf_dp_trace(nbuf,
			     QDF_DP_TRACE_RX_HDD_PACKET_PTR_RECORD,
			     QDF_TRACE_DEFAULT_PDEV_ID,
			     qdf_nbuf_data_addr(nbuf),
			     sizeof(qdf_nbuf_data(nbuf)), QDF_RX));
	DPTRACE(qdf_dp_trace_data_pkt(nbuf, QDF_TRACE_DEFAULT_PDEV_ID,
				      QDF_DP_TRACE_RX_PACKET_RECORD, 0,
				      QDF_RX));

	/*
	 * Set PF_WAKE_UP_IDLE flag in the task structure
	 * This task and any task woken by this will be waken to idle CPU
	 */
	enabled = hdd_ipa_get_wake_up_idle();
	if (!enabled)
		hdd_ipa_set_wake_up_idle(true);

	nbuf->dev = adapter->dev;
	nbuf->protocol = eth_type_trans(nbuf, nbuf->dev);
	nbuf->ip_summed = CHECKSUM_NONE;

	cpu_index = wlan_hdd_get_cpu();

	++adapter->hdd_stats.tx_rx_stats.rx_packets[cpu_index];

	/*
	 * Update STA RX exception packet stats.
	 * For SAP as part of IPA HW stats are updated.
	 */

	++adapter->stats.rx_packets;
	adapter->stats.rx_bytes += nbuf->len;

	result = hdd_ipa_aggregated_rx_ind(nbuf);
	if (result == NET_RX_SUCCESS)
		++adapter->hdd_stats.tx_rx_stats.rx_delivered[cpu_index];
	else
		++adapter->hdd_stats.tx_rx_stats.rx_refused[cpu_index];

	/*
	 * Restore PF_WAKE_UP_IDLE flag in the task structure
	 */
	if (!enabled)
		hdd_ipa_set_wake_up_idle(false);
}

void hdd_ipa_set_mcc_mode(bool mcc_mode)
{
	struct hdd_context *hdd_ctx;

	hdd_ctx = cds_get_context(QDF_MODULE_ID_HDD);
	if (!hdd_ctx) {
		hdd_err("HDD context is NULL");
		return;
	}

	ucfg_ipa_set_mcc_mode(hdd_ctx->pdev, mcc_mode);
}
