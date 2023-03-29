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

#if !defined(WLAN_HDD_TX_RX_H)
#define WLAN_HDD_TX_RX_H

/**
 *
 * DOC: wlan_hdd_tx_rx.h
 *
 * Linux HDD Tx/RX APIs
 */

#include <wlan_hdd_includes.h>
#include <cds_api.h>
#include <linux/skbuff.h>
#include "cdp_txrx_flow_ctrl_legacy.h"

struct hdd_netif_queue_history;
struct hdd_context;

#define hdd_dp_alert(params...) QDF_TRACE_FATAL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_err(params...) QDF_TRACE_ERROR(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_warn(params...) QDF_TRACE_WARN(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_info(params...) QDF_TRACE_INFO(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_debug(params...) QDF_TRACE_DEBUG(QDF_MODULE_ID_HDD_DATA, params)

#define hdd_dp_nofl_alert(params...) \
	QDF_TRACE_FATAL_NO_FL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_nofl_err(params...) \
	QDF_TRACE_ERROR_NO_FL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_nofl_warn(params...) \
	QDF_TRACE_WARN_NO_FL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_nofl_info(params...) \
	QDF_TRACE_INFO_NO_FL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_nofl_debug(params...) \
	QDF_TRACE_DEBUG_NO_FL(QDF_MODULE_ID_HDD_DATA, params)

#define hdd_dp_alert_rl(params...) \
			QDF_TRACE_FATAL_RL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_err_rl(params...) \
			QDF_TRACE_ERROR_RL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_warn_rl(params...) \
			QDF_TRACE_WARN_RL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_info_rl(params...) \
			QDF_TRACE_INFO_RL(QDF_MODULE_ID_HDD_DATA, params)
#define hdd_dp_debug_rl(params...) \
			QDF_TRACE_DEBUG_RL(QDF_MODULE_ID_HDD_DATA, params)

#define hdd_dp_enter() hdd_dp_debug("enter")
#define hdd_dp_enter_dev(dev) hdd_dp_debug("enter(%s)", (dev)->name)
#define hdd_dp_exit() hdd_dp_debug("exit")

#define HDD_ETHERTYPE_802_1_X              0x888E
#ifdef FEATURE_WLAN_WAPI
#define HDD_ETHERTYPE_WAI                  0x88b4
#define IS_HDD_ETHERTYPE_WAI(_skb) (ntohs(_skb->protocol) == \
					HDD_ETHERTYPE_WAI)
#else
#define IS_HDD_ETHERTYPE_WAI(_skb) (false)
#endif

#define HDD_PSB_CFG_INVALID                   0xFF
#define HDD_PSB_CHANGED                       0xFF
#define SME_QOS_UAPSD_CFG_BK_CHANGED_MASK     0xF1
#define SME_QOS_UAPSD_CFG_BE_CHANGED_MASK     0xF2
#define SME_QOS_UAPSD_CFG_VI_CHANGED_MASK     0xF4
#define SME_QOS_UAPSD_CFG_VO_CHANGED_MASK     0xF8

netdev_tx_t hdd_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);

/**
 * hdd_tx_timeout() - Wrapper function to protect __hdd_tx_timeout from SSR
 * @net_dev: pointer to net_device structure
 * @txqueue: tx queue
 *
 * Function called by OS if there is any timeout during transmission.
 * Since HDD simply enqueues packet and returns control to OS right away,
 * this would never be invoked
 *
 * Return: none
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
void hdd_tx_timeout(struct net_device *dev, unsigned int txqueue);
#else
void hdd_tx_timeout(struct net_device *dev);
#endif

QDF_STATUS hdd_init_tx_rx(struct hdd_adapter *adapter);
QDF_STATUS hdd_deinit_tx_rx(struct hdd_adapter *adapter);

/**
 * hdd_rx_flush_packet_cbk() - flush rx packet handler
 * @adapter_context: pointer to HDD adapter context
 * @vdev_id: vdev_id of the packets to be flushed
 *
 * Flush rx packet callback registered with data path. DP will call this to
 * notify HDD when packets for a particular vdev is to be flushed out.
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS hdd_rx_flush_packet_cbk(void *adapter_context, uint8_t vdev_id);

/**
 * hdd_rx_packet_cbk() - Receive packet handler
 * @adapter_context: pointer to HDD adapter context
 * @rxBuf: pointer to rx qdf_nbuf
 *
 * Receive callback registered with data path.  DP will call this to notify
 * the HDD when one or more packets were received for a registered
 * STA.
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS hdd_rx_packet_cbk(void *adapter_context, qdf_nbuf_t rxBuf);

#if defined(WLAN_SUPPORT_RX_FISA)
/**
 * hdd_rx_fisa_cbk() - Entry function to FISA to handle aggregation
 * @soc: core txrx main context
 * @vdev: Handle DP vdev
 * @nbuf_list: List nbufs to be aggregated
 *
 * Return: Success on aggregation
 */
QDF_STATUS hdd_rx_fisa_cbk(void *dp_soc, void *dp_vdev, qdf_nbuf_t rxbuf_list);

/**
 * hdd_rx_fisa_flush_by_ctx_id() - Flush function to end of context
 *				   flushing of aggregates
 * @soc: core txrx main context
 * @ring_num: REO number to flush the flow Rxed on the REO
 *
 * Return: Success on flushing the flows for the REO
 */
QDF_STATUS hdd_rx_fisa_flush_by_ctx_id(void *dp_soc, int ring_num);

/**
 * hdd_rx_fisa_flush_by_vdev_id() - Flush fisa aggregates per vdev id
 * @soc: core txrx main context
 * @vdev_id: vdev ID
 *
 * Return: Success on flushing the flows for the vdev
 */
QDF_STATUS hdd_rx_fisa_flush_by_vdev_id(void *dp_soc, uint8_t vdev_id);
#else
static inline QDF_STATUS hdd_rx_fisa_flush_by_vdev_id(void *dp_soc,
						      uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * hdd_rx_deliver_to_stack() - HDD helper function to deliver RX pkts to stack
 * @adapter: pointer to HDD adapter context
 * @skb: pointer to skb
 *
 * The function calls the appropriate stack function depending upon the packet
 * type and whether GRO/LRO is enabled.
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS hdd_rx_deliver_to_stack(struct hdd_adapter *adapter,
				   struct sk_buff *skb);

/**
 * hdd_rx_thread_gro_flush_ind_cbk() - receive handler to flush GRO packets
 * @adapter: pointer to HDD adapter
 * @rx_ctx_id: RX CTX Id for which flush should happen
 *
 * Receive callback registered with DP layer which flushes GRO packets
 * for a given RX CTX ID (RX Thread)
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS hdd_rx_thread_gro_flush_ind_cbk(void *adapter, int rx_ctx_id);

/**
 * hdd_rx_pkt_thread_enqueue_cbk() - receive pkt handler to enqueue into thread
 * @adapter: pointer to HDD adapter
 * @nbuf_list: pointer to qdf_nbuf list
 *
 * Receive callback registered with DP layer which enqueues packets into dp rx
 * thread
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS hdd_rx_pkt_thread_enqueue_cbk(void *adapter_context,
					 qdf_nbuf_t nbuf_list);

/**
 * hdd_rx_ol_init() - Initialize Rx offload mode (LRO or GRO)
 * @hdd_ctx: pointer to HDD Station Context
 *
 * Return: 0 on success and non zero on failure.
 */
int hdd_rx_ol_init(struct hdd_context *hdd_ctx);

/**
 * hdd_disable_rx_ol_in_concurrency() - Disable Rx offload due to concurrency
 * @disable: true/false to disable/enable the Rx offload
 *
 * Return: none
 */
void hdd_disable_rx_ol_in_concurrency(bool disable);

/**
 * hdd_disable_rx_ol_for_low_tput() - Disable Rx offload in low TPUT scenario
 * @hdd_ctx: hdd context
 * @disable: true/false to disable/enable the Rx offload
 *
 * Return: none
 */
void hdd_disable_rx_ol_for_low_tput(struct hdd_context *hdd_ctx, bool disable);

/**
 * hdd_reset_all_adapters_connectivity_stats() - reset connectivity stats
 * @hdd_ctx: pointer to HDD Station Context
 *
 * Return: None
 */
void hdd_reset_all_adapters_connectivity_stats(struct hdd_context *hdd_ctx);

/**
 * hdd_tx_rx_collect_connectivity_stats_info() - collect connectivity stats
 * @skb: pointer to skb data
 * @adapter: pointer to vdev apdapter
 * @action: action done on pkt.
 * @pkt_type: data pkt type
 *
 * Return: None
 */
void hdd_tx_rx_collect_connectivity_stats_info(struct sk_buff *skb,
		void *adapter, enum connectivity_stats_pkt_status action,
		uint8_t *pkt_type);

/**
 * hdd_tx_queue_cb() - Disable/Enable the Transmit Queues
 * @hdd_handle: HDD handle
 * @vdev_id: vdev id
 * @action: Action to be taken on the Tx Queues
 * @reason: Reason for the netif action
 *
 * Return: None
 */
void hdd_tx_queue_cb(hdd_handle_t hdd_handle, uint32_t vdev_id,
		     enum netif_action_type action,
		     enum netif_reason_type reason);

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
void hdd_tx_resume_cb(void *adapter_context, bool tx_resume);

/**
 * hdd_tx_flow_control_is_pause() - Is TX Q paused by flow control
 * @adapter_context: pointer to vdev apdapter
 *
 * Return: true if TX Q is paused by flow control
 */
bool hdd_tx_flow_control_is_pause(void *adapter_context);

/**
 * hdd_register_tx_flow_control() - Register TX Flow control
 * @adapter: adapter handle
 * @timer_callback: timer callback
 * @flow_control_fp: txrx flow control
 * @flow_control_is_pause_fp: is txrx paused by flow control
 *
 * Return: none
 */
void hdd_register_tx_flow_control(struct hdd_adapter *adapter,
		qdf_mc_timer_callback_t timer_callback,
		ol_txrx_tx_flow_control_fp flow_control_fp,
		ol_txrx_tx_flow_control_is_pause_fp flow_control_is_pause);
void hdd_deregister_tx_flow_control(struct hdd_adapter *adapter);

/**
 * hdd_get_tx_resource() - check tx resources and take action
 * @adapter: adapter handle
 * @mac_addr: mac address
 * @timer_value: timer value
 *
 * Return: none
 */
void hdd_get_tx_resource(struct hdd_adapter *adapter,
			 struct qdf_mac_addr *mac_addr, uint16_t timer_value);

#else
static inline void hdd_tx_resume_cb(void *adapter_context, bool tx_resume)
{
}
static inline bool hdd_tx_flow_control_is_pause(void *adapter_context)
{
	return false;
}
static inline void hdd_register_tx_flow_control(struct hdd_adapter *adapter,
		qdf_mc_timer_callback_t timer_callback,
		ol_txrx_tx_flow_control_fp flow_control_fp,
		ol_txrx_tx_flow_control_is_pause_fp flow_control_is_pause)
{
}
static inline void hdd_deregister_tx_flow_control(struct hdd_adapter *adapter)
{
}


/**
 * hdd_get_tx_resource() - check tx resources and take action
 * @adapter: adapter handle
 * @mac_addr: mac address
 * @timer_value: timer value
 *
 * Return: none
 */
static inline
void hdd_get_tx_resource(struct hdd_adapter *adapter,
			 struct qdf_mac_addr *mac_addr, uint16_t timer_value)
{
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

#if defined(QCA_LL_LEGACY_TX_FLOW_CONTROL) || \
		defined(QCA_HL_NETDEV_FLOW_CONTROL)
void hdd_tx_resume_timer_expired_handler(void *adapter_context);
#else
static inline void hdd_tx_resume_timer_expired_handler(void *adapter_context)
{
}
#endif

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
void hdd_register_hl_netdev_fc_timer(struct hdd_adapter *adapter,
				     qdf_mc_timer_callback_t timer_callback);
void hdd_deregister_hl_netdev_fc_timer(struct hdd_adapter *adapter);
#else
static inline void hdd_register_hl_netdev_fc_timer(struct hdd_adapter *adapter,
						   qdf_mc_timer_callback_t
						   timer_callback)
{}

static inline void
	hdd_deregister_hl_netdev_fc_timer(struct hdd_adapter *adapter)
{}
#endif /* QCA_HL_NETDEV_FLOW_CONTROL */

const char *hdd_reason_type_to_string(enum netif_reason_type reason);
const char *hdd_action_type_to_string(enum netif_action_type action);

void wlan_hdd_netif_queue_control(struct hdd_adapter *adapter,
		enum netif_action_type action, enum netif_reason_type reason);

#ifdef FEATURE_MONITOR_MODE_SUPPORT
int hdd_set_mon_rx_cb(struct net_device *dev);
/**
 * hdd_mon_rx_packet_cbk() - Receive callback registered with OL layer.
 * @context: pointer to qdf context
 * @rxBuf: pointer to rx qdf_nbuf
 *
 * TL will call this to notify the HDD when one or more packets were
 * received for a registered STA.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS hdd_mon_rx_packet_cbk(void *context, qdf_nbuf_t rxbuf);
#else
static inline
int hdd_set_mon_rx_cb(struct net_device *dev)
{
	return 0;
}
static inline
QDF_STATUS hdd_mon_rx_packet_cbk(void *context, qdf_nbuf_t rxbuf)
{
	return QDF_STATUS_SUCCESS;
}
#endif

void hdd_send_rps_ind(struct hdd_adapter *adapter);
void hdd_send_rps_disable_ind(struct hdd_adapter *adapter);

/**
 * hdd_adapter_set_rps() - Enable/disable RPS for mode specified
 * @vdev_id: vdev id of adapter for which RPS needs to be enabled
 * @enable: Set true to enable RPS in SAP mode
 *
 * Callback function registered with ipa
 *
 * Return: none
 */
#ifdef IPA_LAN_RX_NAPI_SUPPORT
void hdd_adapter_set_rps(uint8_t vdev_id, bool enable);
#else
static inline
void hdd_adapter_set_rps(uint8_t vdev_id, bool enable)
{
}
#endif

void wlan_hdd_classify_pkt(struct sk_buff *skb);

#ifdef WLAN_FEATURE_DP_BUS_BANDWIDTH
void hdd_reset_tcp_delack(struct hdd_context *hdd_ctx);

/**
 * hdd_reset_tcp_adv_win_scale() - Reset tcp adv window scale value to default
 * @hdd_ctx: Handle to hdd context
 *
 * Function used to reset TCP advance window scale value to its default value
 *
 * Return: None
 */
void hdd_reset_tcp_adv_win_scale(struct hdd_context *hdd_ctx);
#ifdef RX_PERFORMANCE
bool hdd_is_current_high_throughput(struct hdd_context *hdd_ctx);
#else
static inline bool hdd_is_current_high_throughput(struct hdd_context *hdd_ctx)
{
	return false;
}
#endif
#define HDD_MSM_CFG(msm_cfg)	msm_cfg
#else
static inline void hdd_reset_tcp_delack(struct hdd_context *hdd_ctx) {}
static inline void hdd_reset_tcp_adv_win_scale(struct hdd_context *hdd_ctx) {}
static inline bool hdd_is_current_high_throughput(struct hdd_context *hdd_ctx)
{
	return false;
}
#define HDD_MSM_CFG(msm_cfg)	0
#endif

#ifdef FEATURE_WLAN_DIAG_SUPPORT
void hdd_event_eapol_log(struct sk_buff *skb, enum qdf_proto_dir dir);
#else
static inline
void hdd_event_eapol_log(struct sk_buff *skb, enum qdf_proto_dir dir)
{}
#endif

/**
 * hdd_set_udp_qos_upgrade_config() - Set the threshold for UDP packet
 *				      QoS upgrade.
 * @adapter: adapter for which this configuration is to be applied
 * @priority: the threshold priority
 *
 * Returns: 0 on success, -EINVAL on failure
 */
int hdd_set_udp_qos_upgrade_config(struct hdd_adapter *adapter,
				   uint8_t priority);

/*
 * As of the 4.7 kernel, net_device->trans_start is removed. Create shims to
 * support compiling against older versions of the kernel.
 */
#if (LINUX_VERSION_CODE < KERNEL_VERSION(4, 7, 0))
static inline void netif_trans_update(struct net_device *dev)
{
	dev->trans_start = jiffies;
}

#define TX_TIMEOUT_TRACE(dev, module_id) QDF_TRACE( \
	module_id, QDF_TRACE_LEVEL_ERROR, \
	"%s: Transmission timeout occurred jiffies %lu trans_start %lu", \
	__func__, jiffies, dev->trans_start)
#else
#define TX_TIMEOUT_TRACE(dev, module_id) QDF_TRACE( \
	module_id, QDF_TRACE_LEVEL_ERROR, \
	"%s: Transmission timeout occurred jiffies %lu", \
	__func__, jiffies)
#endif

static inline void
hdd_skb_fill_gso_size(struct net_device *dev, struct sk_buff *skb)
{
	if (skb_cloned(skb) && skb_is_nonlinear(skb) &&
	    skb_shinfo(skb)->gso_size == 0 &&
	    ip_hdr(skb)->protocol == IPPROTO_TCP) {
		skb_shinfo(skb)->gso_size = dev->mtu -
			((skb_transport_header(skb) - skb_network_header(skb))
				+ tcp_hdrlen(skb));
	}
}

/**
 * hdd_txrx_get_tx_ack_count() - get tx acked count
 * @adapter: Pointer to adapter
 *
 * Return: tx acked count
 */
uint32_t hdd_txrx_get_tx_ack_count(struct hdd_adapter *adapter);

#ifdef CONFIG_HL_SUPPORT
static inline QDF_STATUS
hdd_skb_nontso_linearize(struct sk_buff *skb)
{
	return QDF_STATUS_SUCCESS;
}
#else
static inline QDF_STATUS
hdd_skb_nontso_linearize(struct sk_buff *skb)
{
	if (qdf_nbuf_is_nonlinear(skb) && qdf_nbuf_is_tso(skb) == false) {
		if (qdf_unlikely(skb_linearize(skb)))
			return QDF_STATUS_E_NOMEM;
	}
	return QDF_STATUS_SUCCESS;
}
#endif

/**
 * hdd_dp_cfg_update() - update hdd config for HDD DP INIs
 * @psoc: Pointer to psoc obj
 * @hdd_ctx: Pointer to hdd context
 *
 * Return: None
 */
void hdd_dp_cfg_update(struct wlan_objmgr_psoc *psoc,
		       struct hdd_context *hdd_ctx);

/**
 * hdd_print_netdev_txq_status() - print netdev tx queue status
 * @dev: Pointer to network device
 *
 * This function is used to print netdev tx queue status
 *
 * Return: None
 */
void hdd_print_netdev_txq_status(struct net_device *dev);

/**
 * wlan_hdd_dump_queue_history_state() - Dump hdd queue history states
 * @q_hist: pointer to hdd queue history structure
 * @buf: buffer where the queue history string is dumped
 * @size: size of the buffer
 *
 * Dump hdd queue history states into a buffer
 *
 * Return: number of bytes written to the buffer
 */
uint32_t
wlan_hdd_dump_queue_history_state(struct hdd_netif_queue_history *q_hist,
				  char *buf, uint32_t size);

/**
 * wlan_hdd_rx_rpm_mark_last_busy() - Check if dp rx marked last busy
 * @hdd_ctx: Pointer to hdd context
 * @hif_ctx: Pointer to hif context
 *
 * Return: dp mark last busy less than runtime delay value
 */
bool wlan_hdd_rx_rpm_mark_last_busy(struct hdd_context *hdd_ctx,
				    void *hif_ctx);

/**
 * hdd_sta_notify_tx_comp_cb() - notify tx comp callback registered with dp
 * @skb: pointer to skb
 * @ctx: osif context
 * @flag: tx status flag
 *
 * Return: None
 */
void hdd_sta_notify_tx_comp_cb(qdf_nbuf_t skb, void *ctx, uint16_t flag);
#endif /* end #if !defined(WLAN_HDD_TX_RX_H) */
