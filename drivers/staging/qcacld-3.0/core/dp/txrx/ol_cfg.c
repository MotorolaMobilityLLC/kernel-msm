/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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

#include <ol_cfg.h>
#include <ol_if_athvar.h>
#include <cdp_txrx_cfg.h>
#include <cdp_txrx_handle.h>

unsigned int vow_config;

#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
/**
 * ol_tx_set_flow_control_parameters() - set flow control parameters
 * @cfg_ctx: cfg context
 * @cfg_param: cfg parameters
 *
 * Return: none
 */
void ol_tx_set_flow_control_parameters(struct cdp_cfg *cfg_pdev,
	struct txrx_pdev_cfg_param_t *cfg_param)
{
	struct txrx_pdev_cfg_t *cfg_ctx = (struct txrx_pdev_cfg_t *)cfg_pdev;

	cfg_ctx->tx_flow_start_queue_offset =
					cfg_param->tx_flow_start_queue_offset;
	cfg_ctx->tx_flow_stop_queue_th =
					cfg_param->tx_flow_stop_queue_th;
}
#endif

#ifdef CONFIG_HL_SUPPORT

#ifdef CONFIG_CREDIT_REP_THROUGH_CREDIT_UPDATE
static inline
void ol_pdev_cfg_credit_update(struct txrx_pdev_cfg_t *cfg_ctx)
{
	cfg_ctx->tx_free_at_download = 1;
	cfg_ctx->credit_update_enabled = 1;
}
#else
static inline
void ol_pdev_cfg_credit_update(struct txrx_pdev_cfg_t *cfg_ctx)
{
	cfg_ctx->tx_free_at_download = 0;
	cfg_ctx->credit_update_enabled = 0;
}
#endif /* CONFIG_CREDIT_REP_THROUGH_CREDIT_UPDATE */

/**
 * ol_pdev_cfg_param_update() - assign download size of tx frame for txrx
 *				    pdev that will be used across datapath
 * @cfg_ctx: ptr to config parameter for txrx pdev
 *
 * Return: None
 */
static inline
void ol_pdev_cfg_param_update(struct txrx_pdev_cfg_t *cfg_ctx)
{
	cfg_ctx->is_high_latency = 1;
	/* 802.1Q and SNAP / LLC headers are accounted for elsewhere */
	cfg_ctx->tx_download_size = 1500;
	ol_pdev_cfg_credit_update(cfg_ctx);
}

#else /* CONFIG_HL_SUPPORT */
static inline
void ol_pdev_cfg_param_update(struct txrx_pdev_cfg_t *cfg_ctx)
{
	/*
	 * Need to change HTT_LL_TX_HDR_SIZE_IP accordingly.
	 * Include payload, up to the end of UDP header for IPv4 case
	 */
	cfg_ctx->tx_download_size = 16;
}
#endif

#ifdef CONFIG_RX_PN_CHECK_OFFLOAD
static inline
void ol_pdev_cfg_rx_pn_check(struct txrx_pdev_cfg_t *cfg_ctx)
{
	/* Do not do pn check on host */
	cfg_ctx->rx_pn_check = 0;
}
#else
static inline
void ol_pdev_cfg_rx_pn_check(struct txrx_pdev_cfg_t *cfg_ctx)
{
	/* Do pn check on host */
	cfg_ctx->rx_pn_check = 1;
}
#endif /* CONFIG_RX_PN_CHECK_OFFLOAD */

#if CFG_TGT_DEFAULT_RX_SKIP_DEFRAG_TIMEOUT_DUP_DETECTION_CHECK
static inline
uint8_t ol_defrag_timeout_check(void)
{
	return 1;
}
#else
static inline
uint8_t ol_defrag_timeout_check(void)
{
	return 0;
}
#endif

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK
/**
 * ol_cfg_update_del_ack_params() - update delayed ack params
 * @cfg_ctx: cfg context
 * @cfg_param: parameters
 *
 * Return: none
 */
void ol_cfg_update_del_ack_params(struct txrx_pdev_cfg_t *cfg_ctx,
				  struct txrx_pdev_cfg_param_t *cfg_param)
{
	cfg_ctx->del_ack_enable = cfg_param->del_ack_enable;
	cfg_ctx->del_ack_timer_value = cfg_param->del_ack_timer_value;
	cfg_ctx->del_ack_pkt_count = cfg_param->del_ack_pkt_count;
}
#endif

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
static inline
void ol_cfg_update_bundle_params(struct txrx_pdev_cfg_t *cfg_ctx,
				 struct txrx_pdev_cfg_param_t *cfg_param)
{
	cfg_ctx->bundle_timer_value = cfg_param->bundle_timer_value;
	cfg_ctx->bundle_size = cfg_param->bundle_size;
}
#else
static inline
void ol_cfg_update_bundle_params(struct txrx_pdev_cfg_t *cfg_ctx,
				 struct txrx_pdev_cfg_param_t *cfg_param)
{
}
#endif

/* FIX THIS -
 * For now, all these configuration parameters are hardcoded.
 * Many of these should actually be determined dynamically instead.
 */

struct cdp_cfg *ol_pdev_cfg_attach(qdf_device_t osdev, void *pcfg_param)
{
	struct txrx_pdev_cfg_param_t *cfg_param = pcfg_param;
	struct txrx_pdev_cfg_t *cfg_ctx;
	int i;

	cfg_ctx = qdf_mem_malloc(sizeof(*cfg_ctx));
	if (!cfg_ctx)
		return NULL;

	ol_pdev_cfg_param_update(cfg_ctx);
	ol_pdev_cfg_rx_pn_check(cfg_ctx);

	cfg_ctx->defrag_timeout_check = ol_defrag_timeout_check();
	cfg_ctx->max_peer_id = 511;
	cfg_ctx->max_vdev = CFG_TGT_NUM_VDEV;
	cfg_ctx->pn_rx_fwd_check = 1;
	cfg_ctx->frame_type = wlan_frm_fmt_802_3;
	cfg_ctx->max_thruput_mbps = MAX_THROUGHPUT;
	cfg_ctx->max_nbuf_frags = 1;
	cfg_ctx->vow_config = vow_config;
	cfg_ctx->target_tx_credit = CFG_TGT_NUM_MSDU_DESC;
	cfg_ctx->throttle_period_ms = 40;
	cfg_ctx->dutycycle_level[0] = THROTTLE_DUTY_CYCLE_LEVEL0;
	cfg_ctx->dutycycle_level[1] = THROTTLE_DUTY_CYCLE_LEVEL1;
	cfg_ctx->dutycycle_level[2] = THROTTLE_DUTY_CYCLE_LEVEL2;
	cfg_ctx->dutycycle_level[3] = THROTTLE_DUTY_CYCLE_LEVEL3;
	cfg_ctx->rx_fwd_disabled = 0;
	cfg_ctx->is_packet_log_enabled = 0;
	cfg_ctx->is_full_reorder_offload = cfg_param->is_full_reorder_offload;
#ifdef WLAN_FEATURE_TSF_PLUS
	cfg_ctx->is_ptp_rx_opt_enabled = 0;
#endif
	cfg_ctx->ipa_uc_rsc.uc_offload_enabled =
		cfg_param->is_uc_offload_enabled;
	cfg_ctx->ipa_uc_rsc.tx_max_buf_cnt = cfg_param->uc_tx_buffer_count;
	cfg_ctx->ipa_uc_rsc.tx_buf_size = cfg_param->uc_tx_buffer_size;
	cfg_ctx->ipa_uc_rsc.rx_ind_ring_size =
		cfg_param->uc_rx_indication_ring_count;
	cfg_ctx->ipa_uc_rsc.tx_partition_base = cfg_param->uc_tx_partition_base;
	cfg_ctx->enable_rxthread = cfg_param->enable_rxthread;
	cfg_ctx->ip_tcp_udp_checksum_offload =
		cfg_param->ip_tcp_udp_checksum_offload;
	cfg_ctx->p2p_ip_tcp_udp_checksum_offload =
		cfg_param->p2p_ip_tcp_udp_checksum_offload;
	cfg_ctx->nan_tcp_udp_checksumoffload =
		cfg_param->nan_ip_tcp_udp_checksum_offload;
	cfg_ctx->ce_classify_enabled = cfg_param->ce_classify_enabled;
	cfg_ctx->gro_enable = cfg_param->gro_enable;
	cfg_ctx->tso_enable = cfg_param->tso_enable;
	cfg_ctx->lro_enable = cfg_param->lro_enable;
	cfg_ctx->enable_data_stall_detection =
		cfg_param->enable_data_stall_detection;
	cfg_ctx->enable_flow_steering = cfg_param->enable_flow_steering;
	cfg_ctx->disable_intra_bss_fwd = cfg_param->disable_intra_bss_fwd;
	cfg_ctx->pktlog_buffer_size = cfg_param->pktlog_buffer_size;

	ol_cfg_update_del_ack_params(cfg_ctx, cfg_param);

	ol_cfg_update_bundle_params(cfg_ctx, cfg_param);

	ol_tx_set_flow_control_parameters((struct cdp_cfg *)cfg_ctx, cfg_param);

	for (i = 0; i < QCA_WLAN_AC_ALL; i++) {
		cfg_ctx->ac_specs[i].wrr_skip_weight =
			cfg_param->ac_specs[i].wrr_skip_weight;
		cfg_ctx->ac_specs[i].credit_threshold =
			cfg_param->ac_specs[i].credit_threshold;
		cfg_ctx->ac_specs[i].send_limit =
			cfg_param->ac_specs[i].send_limit;
		cfg_ctx->ac_specs[i].credit_reserve =
			cfg_param->ac_specs[i].credit_reserve;
		cfg_ctx->ac_specs[i].discard_weight =
			cfg_param->ac_specs[i].discard_weight;
	}

	return (struct cdp_cfg *)cfg_ctx;
}

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE

int ol_cfg_get_bundle_timer_value(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->bundle_timer_value;
}

int ol_cfg_get_bundle_size(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->bundle_size;
}
#endif

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK
/**
 * ol_cfg_get_del_ack_timer_value() - get delayed ack timer value
 * @cfg_pdev: pdev handle
 *
 * Return: timer value
 */
int ol_cfg_get_del_ack_timer_value(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->del_ack_timer_value;
}

/**
 * ol_cfg_get_del_ack_enable_value() - get delayed ack enable value
 * @cfg_pdev: pdev handle
 *
 * Return: enable/disable
 */
bool ol_cfg_get_del_ack_enable_value(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->del_ack_enable;
}

/**
 * ol_cfg_get_del_ack_count_value() - get delayed ack count value
 * @pdev: cfg_pdev handle
 *
 * Return: count value
 */
int ol_cfg_get_del_ack_count_value(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->del_ack_pkt_count;
}
#endif

int ol_cfg_is_high_latency(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->is_high_latency;
}

int ol_cfg_is_credit_update_enabled(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->credit_update_enabled;
}

int ol_cfg_max_peer_id(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;
	/*
	 * TBDXXX - this value must match the peer table
	 * size allocated in FW
	 */
	return cfg->max_peer_id;
}

int ol_cfg_max_vdevs(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->max_vdev;
}

int ol_cfg_rx_pn_check(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->rx_pn_check;
}

int ol_cfg_rx_fwd_check(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->pn_rx_fwd_check;
}

void ol_set_cfg_rx_fwd_disabled(struct cdp_cfg *cfg_pdev,
		uint8_t disable_rx_fwd)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	cfg->rx_fwd_disabled = disable_rx_fwd;
}

void ol_set_cfg_packet_log_enabled(struct cdp_cfg *cfg_pdev, uint8_t val)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	cfg->is_packet_log_enabled = val;
}

uint8_t ol_cfg_is_packet_log_enabled(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->is_packet_log_enabled;
}

int ol_cfg_rx_fwd_disabled(struct cdp_cfg *cfg_pdev)
{
#if defined(ATHR_WIN_NWF)
	/* for Windows, let the OS handle the forwarding */
	return 1;
#else
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->rx_fwd_disabled;
#endif
}

int ol_cfg_rx_fwd_inter_bss(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->rx_fwd_inter_bss;
}

enum wlan_frm_fmt ol_cfg_frame_type(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->frame_type;
}

int ol_cfg_max_thruput_mbps(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->max_thruput_mbps;
}

int ol_cfg_netbuf_frags_max(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->max_nbuf_frags;
}

int ol_cfg_tx_free_at_download(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->tx_free_at_download;
}

void ol_cfg_set_tx_free_at_download(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	cfg->tx_free_at_download = 1;
}


#ifdef CONFIG_HL_SUPPORT
uint16_t ol_cfg_target_tx_credit(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->target_tx_credit;
}
#else

uint16_t ol_cfg_target_tx_credit(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;
	uint16_t rc;
	uint16_t vow_max_sta = (cfg->vow_config & 0xffff0000) >> 16;
	uint16_t vow_max_desc_persta = cfg->vow_config & 0x0000ffff;

	rc =  (cfg->target_tx_credit + (vow_max_sta * vow_max_desc_persta));

	return rc;
}
#endif

int ol_cfg_tx_download_size(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->tx_download_size;
}

int ol_cfg_rx_host_defrag_timeout_duplicate_check(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->defrag_timeout_check;
}

int ol_cfg_throttle_period_ms(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->throttle_period_ms;
}

int ol_cfg_throttle_duty_cycle_level(struct cdp_cfg *cfg_pdev, int level)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->dutycycle_level[level];
}

#ifdef CONFIG_HL_SUPPORT
int ol_cfg_is_full_reorder_offload(struct cdp_cfg *cfg_pdev)
{
	return 0;
}
#else
int ol_cfg_is_full_reorder_offload(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->is_full_reorder_offload;
}
#endif

#ifdef WLAN_FEATURE_TSF_PLUS
void ol_set_cfg_ptp_rx_opt_enabled(struct cdp_cfg *cfg_pdev, u_int8_t val)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	cfg->is_ptp_rx_opt_enabled = val;
}

u_int8_t ol_cfg_is_ptp_rx_opt_enabled(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->is_ptp_rx_opt_enabled;
}
#endif

/**
 * ol_cfg_is_rx_thread_enabled() - return rx_thread is enable/disable
 * @pdev : handle to the physical device
 *
 * Return: 1 - enable, 0 - disable
 */
int ol_cfg_is_rx_thread_enabled(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->enable_rxthread;
}

#if defined(QCA_LL_TX_FLOW_CONTROL_V2) || defined(QCA_LL_PDEV_TX_FLOW_CONTROL)
/**
 * ol_cfg_get_tx_flow_stop_queue_th() - return stop queue threshold
 * @pdev : handle to the physical device
 *
 * Return: stop queue threshold
 */
int ol_cfg_get_tx_flow_stop_queue_th(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->tx_flow_stop_queue_th;
}

/**
 * ol_cfg_get_tx_flow_start_queue_offset() - return start queue offset
 * @pdev : handle to the physical device
 *
 * Return: start queue offset
 */
int ol_cfg_get_tx_flow_start_queue_offset(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->tx_flow_start_queue_offset;
}
#endif

#ifdef IPA_OFFLOAD
unsigned int ol_cfg_ipa_uc_offload_enabled(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return (unsigned int)cfg->ipa_uc_rsc.uc_offload_enabled;
}

unsigned int ol_cfg_ipa_uc_tx_buf_size(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->ipa_uc_rsc.tx_buf_size;
}

unsigned int ol_cfg_ipa_uc_tx_max_buf_cnt(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->ipa_uc_rsc.tx_max_buf_cnt;
}

unsigned int ol_cfg_ipa_uc_rx_ind_ring_size(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->ipa_uc_rsc.rx_ind_ring_size;
}

unsigned int ol_cfg_ipa_uc_tx_partition_base(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->ipa_uc_rsc.tx_partition_base;
}

void ol_cfg_set_ipa_uc_tx_partition_base(struct cdp_cfg *cfg_pdev, uint32_t val)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	cfg->ipa_uc_rsc.tx_partition_base = val;
}
#endif /* IPA_OFFLOAD */

/**
 * ol_cfg_is_ce_classify_enabled() - Return if CE classification is enabled
 *				     or disabled
 * @pdev : handle to the physical device
 *
 * Return: 1 - enabled, 0 - disabled
 */
bool ol_cfg_is_ce_classify_enabled(struct cdp_cfg *cfg_pdev)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)cfg_pdev;

	return cfg->ce_classify_enabled;
}

/**
 * ol_cfg_get_wrr_skip_weight() - brief Query for the param of wrr_skip_weight
 * @pdev: handle to the physical device.
 * @ac: access control, it will be BE, BK, VI, VO
 *
 * Return: wrr_skip_weight for specified ac.
 */
int ol_cfg_get_wrr_skip_weight(struct cdp_cfg *pdev, int ac)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)pdev;

	if (ac >= QCA_WLAN_AC_BE && ac <= QCA_WLAN_AC_VO)
		return cfg->ac_specs[ac].wrr_skip_weight;

	return 0;
}

/**
 * ol_cfg_get_credit_threshold() - Query for the param of credit_threshold
 * @pdev: handle to the physical device.
 * @ac: access control, it will be BE, BK, VI, VO
 *
 * Return: credit_threshold for specified ac.
 */
uint32_t ol_cfg_get_credit_threshold(struct cdp_cfg *pdev, int ac)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)pdev;

	if (ac >= QCA_WLAN_AC_BE && ac <= QCA_WLAN_AC_VO)
		return cfg->ac_specs[ac].credit_threshold;

	return 0;
}

/**
 * ol_cfg_get_send_limit() - Query for the param of send_limit
 * @pdev: handle to the physical device.
 * @ac: access control, it will be BE, BK, VI, VO
 *
 * Return: send_limit for specified ac.
 */
uint16_t ol_cfg_get_send_limit(struct cdp_cfg *pdev, int ac)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)pdev;

	if (ac >= QCA_WLAN_AC_BE && ac <= QCA_WLAN_AC_VO)
		return cfg->ac_specs[ac].send_limit;

	return 0;
}

/**
 * ol_cfg_get_credit_reserve() - Query for the param of credit_reserve
 * @pdev: handle to the physical device.
 * @ac: access control, it will be BE, BK, VI, VO
 *
 * Return: credit_reserve for specified ac.
 */
int ol_cfg_get_credit_reserve(struct cdp_cfg *pdev, int ac)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)pdev;

	if (ac >= QCA_WLAN_AC_BE && ac <= QCA_WLAN_AC_VO)
		return cfg->ac_specs[ac].credit_reserve;

	return 0;
}

/**
 * ol_cfg_get_discard_weight() - Query for the param of discard_weight
 * @pdev: handle to the physical device.
 * @ac: access control, it will be BE, BK, VI, VO
 *
 * Return: discard_weight for specified ac.
 */
int ol_cfg_get_discard_weight(struct cdp_cfg *pdev, int ac)
{
	struct txrx_pdev_cfg_t *cfg = (struct txrx_pdev_cfg_t *)pdev;

	if (ac >= QCA_WLAN_AC_BE && ac <= QCA_WLAN_AC_VO)
		return cfg->ac_specs[ac].discard_weight;

	return 0;
}
