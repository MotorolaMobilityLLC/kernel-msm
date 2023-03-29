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

/* Include Files */
#include "wlan_ipa_main.h"
#include "wlan_ipa_core.h"
#include "cdp_txrx_ipa.h"
#include "qdf_platform.h"

/**
 * wlan_ipa_uc_rt_debug_host_fill - fill rt debug buffer
 * @ctext: pointer to ipa context.
 *
 * If rt debug enabled, periodically called, and fill debug buffer
 *
 * Return: none
 */
static void wlan_ipa_uc_rt_debug_host_fill(void *ctext)
{
	struct wlan_ipa_priv *ipa_ctx = ctext;
	struct uc_rt_debug_info *dump_info = NULL;

	if (!ipa_ctx)
		return;

	qdf_mutex_acquire(&ipa_ctx->rt_debug_lock);
	dump_info = &ipa_ctx->rt_bug_buffer[
		ipa_ctx->rt_buf_fill_index % WLAN_IPA_UC_RT_DEBUG_BUF_COUNT];

	dump_info->time = (uint64_t)qdf_mc_timer_get_system_time();
	dump_info->ipa_excep_count = ipa_ctx->stats.num_rx_excep;
	dump_info->rx_drop_count = ipa_ctx->ipa_rx_internal_drop_count;
	dump_info->net_sent_count = ipa_ctx->ipa_rx_net_send_count;
	dump_info->tx_fwd_count = ipa_ctx->ipa_tx_forward;
	dump_info->tx_fwd_ok_count = ipa_ctx->stats.num_tx_fwd_ok;
	dump_info->rx_discard_count = ipa_ctx->ipa_rx_discard;
	dump_info->rx_destructor_call = ipa_ctx->ipa_rx_destructor_count;
	ipa_ctx->rt_buf_fill_index++;
	qdf_mutex_release(&ipa_ctx->rt_debug_lock);

	qdf_mc_timer_start(&ipa_ctx->rt_debug_fill_timer,
		WLAN_IPA_UC_RT_DEBUG_FILL_INTERVAL);
}

void wlan_ipa_uc_rt_debug_host_dump(struct wlan_ipa_priv *ipa_ctx)
{
	unsigned int dump_count;
	unsigned int dump_index;
	struct uc_rt_debug_info *dump_info = NULL;

	if (!wlan_ipa_is_rt_debugging_enabled(ipa_ctx->config)) {
		ipa_debug("IPA RT debug is not enabled");
		return;
	}

	ipa_info("========= WLAN-IPA DEBUG BUF DUMP ==========\n");
	ipa_info("     TM     :   EXEP   :   DROP   :   NETS   :   FWOK"
		 ":   TXFD   :   DSTR   :   DSCD\n");

	qdf_mutex_acquire(&ipa_ctx->rt_debug_lock);
	for (dump_count = 0;
		dump_count < WLAN_IPA_UC_RT_DEBUG_BUF_COUNT;
		dump_count++) {
		dump_index = (ipa_ctx->rt_buf_fill_index + dump_count) %
			WLAN_IPA_UC_RT_DEBUG_BUF_COUNT;
		dump_info = &ipa_ctx->rt_bug_buffer[dump_index];
		ipa_info("%12llu:%10llu:%10llu:%10llu:%10llu:%10llu:%10llu:%10llu\n",
			dump_info->time, dump_info->ipa_excep_count,
			dump_info->rx_drop_count, dump_info->net_sent_count,
			dump_info->tx_fwd_ok_count, dump_info->tx_fwd_count,
			dump_info->rx_destructor_call,
			dump_info->rx_discard_count);
	}
	qdf_mutex_release(&ipa_ctx->rt_debug_lock);
}

/**
 * wlan_ipa_uc_rt_debug_handler - periodic memory health monitor handler
 * @ctext: pointer to ipa context.
 *
 * periodically called by timer expire
 * will try to alloc dummy memory and detect out of memory condition
 * if out of memory detected, dump wlan-ipa stats
 *
 * Return: none
 */
static void wlan_ipa_uc_rt_debug_handler(void *ctext)
{
	struct wlan_ipa_priv *ipa_ctx = ctext;
	void *dummy_ptr = NULL;

	if (!wlan_ipa_is_rt_debugging_enabled(ipa_ctx->config)) {
		ipa_debug("IPA RT debug is not enabled");
		return;
	}

	/* Allocate dummy buffer periodically and free immediately. this will
	 * proactively detect OOM and if allocation fails dump ipa stats
	 */
	dummy_ptr = qdf_mem_malloc(WLAN_IPA_UC_DEBUG_DUMMY_MEM_SIZE);
	if (!dummy_ptr) {
		wlan_ipa_uc_rt_debug_host_dump(ipa_ctx);
		wlan_ipa_uc_stat_request(ipa_ctx,
					 WLAN_IPA_UC_STAT_REASON_DEBUG);
	} else {
		qdf_mem_free(dummy_ptr);
	}

	qdf_mc_timer_start(&ipa_ctx->rt_debug_timer,
		WLAN_IPA_UC_RT_DEBUG_PERIOD);
}

void wlan_ipa_uc_rt_debug_destructor(qdf_nbuf_t nbuff)
{
	struct wlan_ipa_priv *ipa_ctx = wlan_ipa_get_obj_context();

	if (!ipa_ctx) {
		ipa_err("invalid ipa context");
		return;
	}

	ipa_ctx->ipa_rx_destructor_count++;
}

void wlan_ipa_uc_rt_debug_deinit(struct wlan_ipa_priv *ipa_ctx)
{
	qdf_mutex_destroy(&ipa_ctx->rt_debug_lock);

	if (!wlan_ipa_is_rt_debugging_enabled(ipa_ctx->config)) {
		ipa_debug("IPA RT debug is not enabled");
		return;
	}

	if (QDF_TIMER_STATE_STOPPED !=
		qdf_mc_timer_get_current_state(&ipa_ctx->rt_debug_fill_timer)) {
		qdf_mc_timer_stop(&ipa_ctx->rt_debug_fill_timer);
	}
	qdf_mc_timer_destroy(&ipa_ctx->rt_debug_fill_timer);

	if (QDF_TIMER_STATE_STOPPED !=
		qdf_mc_timer_get_current_state(&ipa_ctx->rt_debug_timer)) {
		qdf_mc_timer_stop(&ipa_ctx->rt_debug_timer);
	}
	qdf_mc_timer_destroy(&ipa_ctx->rt_debug_timer);
}

void wlan_ipa_uc_rt_debug_init(struct wlan_ipa_priv *ipa_ctx)
{
	qdf_mutex_create(&ipa_ctx->rt_debug_lock);
	ipa_ctx->rt_buf_fill_index = 0;
	qdf_mem_zero(ipa_ctx->rt_bug_buffer,
		sizeof(struct uc_rt_debug_info) *
		WLAN_IPA_UC_RT_DEBUG_BUF_COUNT);
	ipa_ctx->ipa_tx_forward = 0;
	ipa_ctx->ipa_rx_discard = 0;
	ipa_ctx->ipa_rx_net_send_count = 0;
	ipa_ctx->ipa_rx_internal_drop_count = 0;
	ipa_ctx->ipa_rx_destructor_count = 0;

	/* Reatime debug enable on feature enable */
	if (!wlan_ipa_is_rt_debugging_enabled(ipa_ctx->config)) {
		ipa_debug("IPA RT debug is not enabled");
		return;
	}

	qdf_mc_timer_init(&ipa_ctx->rt_debug_fill_timer, QDF_TIMER_TYPE_SW,
		wlan_ipa_uc_rt_debug_host_fill, (void *)ipa_ctx);
	qdf_mc_timer_start(&ipa_ctx->rt_debug_fill_timer,
		WLAN_IPA_UC_RT_DEBUG_FILL_INTERVAL);

	qdf_mc_timer_init(&ipa_ctx->rt_debug_timer, QDF_TIMER_TYPE_SW,
		wlan_ipa_uc_rt_debug_handler, (void *)ipa_ctx);
	qdf_mc_timer_start(&ipa_ctx->rt_debug_timer,
		WLAN_IPA_UC_RT_DEBUG_PERIOD);

}

/**
 * wlan_ipa_dump_ipa_ctx() - dump entries in IPA IPA struct
 * @ipa_ctx: IPA context
 *
 * Dump entries in struct ipa_ctx
 *
 * Return: none
 */
static void wlan_ipa_dump_ipa_ctx(struct wlan_ipa_priv *ipa_ctx)
{
	int i;

	/* IPA IPA */
	ipa_info("\n==== IPA IPA ====\n"
		"num_iface: %d\n"
		"rm_state: %d\n"
		"rm_lock: %pK\n"
		"uc_rm_work: %pK\n"
		"uc_op_work: %pK\n"
		"wake_lock: %pK\n"
		"wake_lock_work: %pK\n"
		"wake_lock_released: %d\n"
		"tx_ref_cnt: %d\n"
		"pm_queue_head----\n"
		"\thead: %pK\n"
		"\ttail: %pK\n"
		"\tqlen: %d\n"
		"pm_work: %pK\n"
		"pm_lock: %pK\n"
		"suspended: %d\n",
		ipa_ctx->num_iface,
		ipa_ctx->rm_state,
		&ipa_ctx->rm_lock,
		&ipa_ctx->uc_rm_work,
		&ipa_ctx->uc_op_work,
		&ipa_ctx->wake_lock,
		&ipa_ctx->wake_lock_work,
		ipa_ctx->wake_lock_released,
		ipa_ctx->tx_ref_cnt.counter,
		ipa_ctx->pm_queue_head.head,
		ipa_ctx->pm_queue_head.tail,
		ipa_ctx->pm_queue_head.qlen,
		&ipa_ctx->pm_work,
		&ipa_ctx->pm_lock,
		ipa_ctx->suspended);

	ipa_info("\nq_lock: %pK\n"
		"pend_desc_head----\n"
		"\tnext: %pK\n"
		"\tprev: %pK\n"
		"stats: %pK\n"
		"curr_prod_bw: %d\n"
		"curr_cons_bw: %d\n"
		"activated_fw_pipe: %d\n"
		"sap_num_connected_sta: %d\n"
		"sta_connected: %d\n",
		&ipa_ctx->q_lock,
		ipa_ctx->pend_desc_head.next,
		ipa_ctx->pend_desc_head.prev,
		&ipa_ctx->stats,
		ipa_ctx->curr_prod_bw,
		ipa_ctx->curr_cons_bw,
		ipa_ctx->activated_fw_pipe,
		ipa_ctx->sap_num_connected_sta,
		(unsigned int)ipa_ctx->sta_connected);

	ipa_info("\ntx_pipe_handle: 0x%x\n"
		"rx_pipe_handle: 0x%x\n"
		"resource_loading: %d\n"
		"resource_unloading: %d\n"
		"pending_cons_req: %d\n"
		"pending_event----\n"
		"\tanchor.next: %pK\n"
		"\tanchor.prev: %pK\n"
		"\tcount: %d\n"
		"\tmax_size: %d\n"
		"event_lock: %pK\n"
		"ipa_tx_packets_diff: %d\n"
		"ipa_rx_packets_diff: %d\n"
		"ipa_p_tx_packets: %d\n"
		"ipa_p_rx_packets: %d\n"
		"stat_req_reason: %d\n",
		ipa_ctx->tx_pipe_handle,
		ipa_ctx->rx_pipe_handle,
		ipa_ctx->resource_loading,
		ipa_ctx->resource_unloading,
		ipa_ctx->pending_cons_req,
		ipa_ctx->pending_event.anchor.next,
		ipa_ctx->pending_event.anchor.prev,
		ipa_ctx->pending_event.count,
		ipa_ctx->pending_event.max_size,
		&ipa_ctx->event_lock,
		ipa_ctx->ipa_tx_packets_diff,
		ipa_ctx->ipa_rx_packets_diff,
		ipa_ctx->ipa_p_tx_packets,
		ipa_ctx->ipa_p_rx_packets,
		ipa_ctx->stat_req_reason);

	ipa_info("\ncons_pipe_in----\n"
		"\tsys: %pK\n"
		"\tdl.comp_ring_base_pa: 0x%x\n"
		"\tdl.comp_ring_size: %d\n"
		"\tdl.ce_ring_base_pa: 0x%x\n"
		"\tdl.ce_door_bell_pa: 0x%x\n"
		"\tdl.ce_ring_size: %d\n"
		"\tdl.num_tx_buffers: %d\n"
		"prod_pipe_in----\n"
		"\tsys: %pK\n"
		"\tul.rdy_ring_base_pa: 0x%x\n"
		"\tul.rdy_ring_size: %d\n"
		"\tul.rdy_ring_rp_pa: 0x%x\n"
		"uc_loaded: %d\n"
		"wdi_enabled: %d\n"
		"rt_debug_fill_timer: %pK\n"
		"rt_debug_lock: %pK\n"
		"ipa_lock: %pK\n",
		&ipa_ctx->cons_pipe_in.sys,
		(unsigned int)ipa_ctx->cons_pipe_in.u.dl.comp_ring_base_pa,
		ipa_ctx->cons_pipe_in.u.dl.comp_ring_size,
		(unsigned int)ipa_ctx->cons_pipe_in.u.dl.ce_ring_base_pa,
		(unsigned int)ipa_ctx->cons_pipe_in.u.dl.ce_door_bell_pa,
		ipa_ctx->cons_pipe_in.u.dl.ce_ring_size,
		ipa_ctx->cons_pipe_in.u.dl.num_tx_buffers,
		&ipa_ctx->prod_pipe_in.sys,
		(unsigned int)ipa_ctx->prod_pipe_in.u.ul.rdy_ring_base_pa,
		ipa_ctx->prod_pipe_in.u.ul.rdy_ring_size,
		(unsigned int)ipa_ctx->prod_pipe_in.u.ul.rdy_ring_rp_pa,
		ipa_ctx->uc_loaded,
		ipa_ctx->wdi_enabled,
		&ipa_ctx->rt_debug_fill_timer,
		&ipa_ctx->rt_debug_lock,
		&ipa_ctx->ipa_lock);

	ipa_info("\nvdev_to_iface----");
	for (i = 0; i < WLAN_IPA_MAX_SESSION; i++)
		ipa_info("\n\t[%d]=%d", i, ipa_ctx->vdev_to_iface[i]);

	QDF_TRACE(QDF_MODULE_ID_IPA, QDF_TRACE_LEVEL_INFO,
		"\nvdev_offload_enabled----");
	for (i = 0; i < WLAN_IPA_MAX_SESSION; i++)
		ipa_info("\n\t[%d]=%d", i, ipa_ctx->vdev_offload_enabled[i]);

	QDF_TRACE(QDF_MODULE_ID_IPA, QDF_TRACE_LEVEL_INFO,
		"\nassoc_stas_map ----");
	for (i = 0; i < WLAN_IPA_MAX_STA_COUNT; i++) {
		ipa_info("\n\t[%d]: is_reserved=%d mac: " QDF_MAC_ADDR_FMT, i,
			 ipa_ctx->assoc_stas_map[i].is_reserved,
			 QDF_MAC_ADDR_REF(
				ipa_ctx->assoc_stas_map[i].mac_addr.bytes));
	}
}

/**
 * wlan_ipa_dump_sys_pipe() - dump IPA IPA SYS Pipe struct
 * @ipa_ctx: IPA IPA struct
 *
 * Dump entire struct wlan_ipa_sys_pipe
 *
 * Return: none
 */
static void wlan_ipa_dump_sys_pipe(struct wlan_ipa_priv *ipa_ctx)
{
	int i;

	/* IPA SYS Pipes */
	ipa_info("==== IPA SYS Pipes ====");

	for (i = 0; i < WLAN_IPA_MAX_SYSBAM_PIPE; i++) {
		struct wlan_ipa_sys_pipe *sys_pipe;
		qdf_ipa_sys_connect_params_t *ipa_sys_params;

		sys_pipe = &ipa_ctx->sys_pipe[i];
		ipa_sys_params = &sys_pipe->ipa_sys_params;

		ipa_info("\nsys_pipe[%d]----\n"
			"\tconn_hdl: 0x%x\n"
			"\tconn_hdl_valid: %d\n"
			"\tnat_en: %d\n"
			"\thdr_len %d\n"
			"\thdr_additional_const_len: %d\n"
			"\thdr_ofst_pkt_size_valid: %d\n"
			"\thdr_ofst_pkt_size: %d\n"
			"\thdr_little_endian: %d\n"
			"\tmode: %d\n"
			"\tclient: %d\n"
			"\tdesc_fifo_sz: %d\n"
			"\tpriv: %pK\n"
			"\tnotify: %pK\n"
			"\tskip_ep_cfg: %d\n"
			"\tkeep_ipa_awake: %d\n",
			i,
			sys_pipe->conn_hdl,
			sys_pipe->conn_hdl_valid,
			QDF_IPA_SYS_PARAMS_NAT_EN(ipa_sys_params),
			QDF_IPA_SYS_PARAMS_HDR_LEN(ipa_sys_params),
			QDF_IPA_SYS_PARAMS_HDR_ADDITIONAL_CONST_LEN(
				ipa_sys_params),
			QDF_IPA_SYS_PARAMS_HDR_OFST_PKT_SIZE_VALID(
				ipa_sys_params),
			QDF_IPA_SYS_PARAMS_HDR_OFST_PKT_SIZE(ipa_sys_params),
			QDF_IPA_SYS_PARAMS_HDR_LITTLE_ENDIAN(ipa_sys_params),
			QDF_IPA_SYS_PARAMS_MODE(ipa_sys_params),
			QDF_IPA_SYS_PARAMS_CLIENT(ipa_sys_params),
			QDF_IPA_SYS_PARAMS_DESC_FIFO_SZ(ipa_sys_params),
			QDF_IPA_SYS_PARAMS_PRIV(ipa_sys_params),
			QDF_IPA_SYS_PARAMS_NOTIFY(ipa_sys_params),
			QDF_IPA_SYS_PARAMS_SKIP_EP_CFG(ipa_sys_params),
			QDF_IPA_SYS_PARAMS_KEEP_IPA_AWAKE(ipa_sys_params));
	}
}

/**
 * wlan_ipa_dump_iface_context() - dump IPA IPA Interface Context struct
 * @ipa_ctx: IPA IPA struct
 *
 * Dump entire struct wlan_ipa_iface_context
 *
 * Return: none
 */
static void wlan_ipa_dump_iface_context(struct wlan_ipa_priv *ipa_ctx)
{
	int i;

	/* IPA Interface Contexts */
	ipa_info("\n==== IPA Interface Contexts ====\n");

	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		struct wlan_ipa_iface_context *iface_context;

		iface_context = &ipa_ctx->iface_context[i];

		ipa_info("\niface_context[%d]----\n"
			"\tipa_ctx: %pK\n"
			"\tsession_id: %d\n"
			"\tcons_client: %d\n"
			"\tprod_client: %d\n"
			"\tiface_id: %d\n"
			"\tinterface_lock: %pK\n"
			"\tifa_address: 0x%x\n",
			i,
			iface_context->ipa_ctx,
			iface_context->session_id,
			iface_context->cons_client,
			iface_context->prod_client,
			iface_context->iface_id,
			&iface_context->interface_lock,
			iface_context->ifa_address);
	}
}

void wlan_ipa_dump_info(struct wlan_ipa_priv *ipa_ctx)
{
	wlan_ipa_dump_ipa_ctx(ipa_ctx);
	wlan_ipa_dump_sys_pipe(ipa_ctx);
	wlan_ipa_dump_iface_context(ipa_ctx);
}

void wlan_ipa_uc_stat_query(struct wlan_ipa_priv *ipa_ctx,
			    uint32_t *ipa_tx_diff, uint32_t *ipa_rx_diff)
{
	*ipa_tx_diff = 0;
	*ipa_rx_diff = 0;

	qdf_mutex_acquire(&ipa_ctx->ipa_lock);
	if (wlan_ipa_is_fw_wdi_activated(ipa_ctx) &&
	    (false == ipa_ctx->resource_loading)) {
		*ipa_tx_diff = ipa_ctx->ipa_tx_packets_diff;
		*ipa_rx_diff = ipa_ctx->ipa_rx_packets_diff;
	}
	qdf_mutex_release(&ipa_ctx->ipa_lock);
}

void wlan_ipa_uc_stat_request(struct wlan_ipa_priv *ipa_ctx, uint8_t reason)
{
	qdf_mutex_acquire(&ipa_ctx->ipa_lock);
	if (wlan_ipa_is_fw_wdi_activated(ipa_ctx) &&
	    (false == ipa_ctx->resource_loading)) {
		ipa_ctx->stat_req_reason = reason;
		cdp_ipa_get_stat(ipa_ctx->dp_soc, ipa_ctx->dp_pdev_id);
		qdf_mutex_release(&ipa_ctx->ipa_lock);
	} else {
		qdf_mutex_release(&ipa_ctx->ipa_lock);
	}
}

/**
 * wlan_ipa_print_session_info - Print IPA session info
 * @ipa_ctx: IPA context
 *
 * Return: None
 */
static void wlan_ipa_print_session_info(struct wlan_ipa_priv *ipa_ctx)
{
	struct wlan_ipa_uc_pending_event *event = NULL, *next = NULL;
	struct wlan_ipa_iface_context *iface_context = NULL;
	int i;

	ipa_info("\n==== IPA SESSION INFO ====\n"
		"NUM IFACE: %d\n"
		"RM STATE: %d\n"
		"ACTIVATED FW PIPE: %d\n"
		"SAP NUM STAs: %d\n"
		"STA CONNECTED: %d\n"
		"CONCURRENT MODE: %s\n"
		"RSC LOADING: %d\n"
		"RSC UNLOADING: %d\n"
		"PENDING CONS REQ: %d\n"
		"IPA PIPES DOWN: %d\n"
		"IPA UC LOADED: %d\n"
		"IPA WDI ENABLED: %d\n"
		"NUM SEND MSG: %d\n"
		"NUM FREE MSG: %d\n",
		ipa_ctx->num_iface,
		ipa_ctx->rm_state,
		ipa_ctx->activated_fw_pipe,
		ipa_ctx->sap_num_connected_sta,
		ipa_ctx->sta_connected,
		(ipa_ctx->mcc_mode ? "MCC" : "SCC"),
		ipa_ctx->resource_loading,
		ipa_ctx->resource_unloading,
		ipa_ctx->pending_cons_req,
		ipa_ctx->ipa_pipes_down,
		ipa_ctx->uc_loaded,
		ipa_ctx->wdi_enabled,
		(unsigned int)ipa_ctx->stats.num_send_msg,
		(unsigned int)ipa_ctx->stats.num_free_msg);

	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {
		iface_context = &ipa_ctx->iface_context[i];

		if (iface_context->session_id == WLAN_IPA_MAX_SESSION)
			continue;

		ipa_info("\nIFACE[%d]: mode:%d, offload:%d",
			 i, iface_context->device_mode,
			 ipa_ctx->vdev_offload_enabled[iface_context->
						       session_id]);
	}

	for (i = 0; i < QDF_IPA_WLAN_EVENT_MAX; i++)
		ipa_info("\nEVENT[%d]=%d",
			 i, ipa_ctx->stats.event[i]);

	i = 0;
	qdf_list_peek_front(&ipa_ctx->pending_event,
			(qdf_list_node_t **)&event);
	while (event) {
		ipa_info("PENDING EVENT[%d]: EVT:%s, MAC:"QDF_MAC_ADDR_FMT,
			 i, wlan_ipa_wlan_event_to_str(event->type),
			 QDF_MAC_ADDR_REF(event->mac_addr));

		qdf_list_peek_next(&ipa_ctx->pending_event,
				   (qdf_list_node_t *)event,
				   (qdf_list_node_t **)&next);
		event = next;
		next = NULL;
		i++;
	}
}

/**
 * wlan_ipa_print_txrx_stats - Print IPA IPA TX/RX stats
 * @ipa_ctx: IPA context
 *
 * Return: None
 */
static void wlan_ipa_print_txrx_stats(struct wlan_ipa_priv *ipa_ctx)
{
	int i;
	struct wlan_ipa_iface_context *iface_context = NULL;

	ipa_info("\n==== IPA IPA TX/RX STATS ====\n"
		"NUM RM GRANT: %llu\n"
		"NUM RM RELEASE: %llu\n"
		"NUM RM GRANT IMM: %llu\n"
		"NUM CONS PERF REQ: %llu\n"
		"NUM PROD PERF REQ: %llu\n"
		"NUM RX DROP: %llu\n"
		"NUM EXCP PKT: %llu\n"
		"NUM TX FWD OK: %llu\n"
		"NUM TX FWD ERR: %llu\n"
		"NUM TX DESC Q CNT: %llu\n"
		"NUM TX DESC ERROR: %llu\n"
		"NUM TX COMP CNT: %llu\n"
		"NUM TX QUEUED: %llu\n"
		"NUM TX DEQUEUED: %llu\n"
		"NUM MAX PM QUEUE: %llu\n"
		"TX REF CNT: %d\n"
		"SUSPENDED: %d\n"
		"PEND DESC HEAD: %pK\n"
		"TX DESC LIST: %pK\n",
		ipa_ctx->stats.num_rm_grant,
		ipa_ctx->stats.num_rm_release,
		ipa_ctx->stats.num_rm_grant_imm,
		ipa_ctx->stats.num_cons_perf_req,
		ipa_ctx->stats.num_prod_perf_req,
		ipa_ctx->stats.num_rx_drop,
		ipa_ctx->stats.num_rx_excep,
		ipa_ctx->stats.num_tx_fwd_ok,
		ipa_ctx->stats.num_tx_fwd_err,
		ipa_ctx->stats.num_tx_desc_q_cnt,
		ipa_ctx->stats.num_tx_desc_error,
		ipa_ctx->stats.num_tx_comp_cnt,
		ipa_ctx->stats.num_tx_queued,
		ipa_ctx->stats.num_tx_dequeued,
		ipa_ctx->stats.num_max_pm_queue,
		ipa_ctx->tx_ref_cnt.counter,
		ipa_ctx->suspended,
		&ipa_ctx->pend_desc_head,
		&ipa_ctx->tx_desc_free_list);

	for (i = 0; i < WLAN_IPA_MAX_IFACE; i++) {

		iface_context = &ipa_ctx->iface_context[i];
		if (iface_context->session_id == WLAN_IPA_MAX_SESSION)
			continue;

		ipa_info("IFACE[%d]: TX:%llu, TX DROP:%llu, TX ERR:%llu,"
			 " TX CAC DROP:%llu, RX IPA EXCEP:%llu",
			 i, iface_context->stats.num_tx,
			 iface_context->stats.num_tx_drop,
			 iface_context->stats.num_tx_err,
			 iface_context->stats.num_tx_cac_drop,
			 iface_context->stats.num_rx_ipa_excep);
	}
}

void wlan_ipa_print_fw_wdi_stats(struct wlan_ipa_priv *ipa_ctx,
				 struct ipa_uc_fw_stats *uc_fw_stat)
{
	ipa_info("\n==== WLAN FW WDI TX STATS ====\n"
		"COMP RING SIZE: %d\n"
		"COMP RING DBELL IND VAL : %d\n"
		"COMP RING DBELL CACHED VAL : %d\n"
		"PKTS ENQ : %d\n"
		"PKTS COMP : %d\n"
		"IS SUSPEND : %d\n",
		uc_fw_stat->tx_comp_ring_size,
		uc_fw_stat->tx_comp_ring_dbell_ind_val,
		uc_fw_stat->tx_comp_ring_dbell_cached_val,
		uc_fw_stat->tx_pkts_enqueued,
		uc_fw_stat->tx_pkts_completed,
		uc_fw_stat->tx_is_suspend);

	ipa_info("\n==== WLAN FW WDI RX STATS ====\n"
		"IND RING SIZE: %d\n"
		"IND RING DBELL IND VAL : %d\n"
		"IND RING DBELL CACHED VAL : %d\n"
		"RDY IND CACHE VAL : %d\n"
		"RFIL IND : %d\n"
		"NUM PKT INDICAT : %d\n"
		"BUF REFIL : %d\n"
		"NUM DROP NO SPC : %d\n"
		"NUM DROP NO BUF : %d\n"
		"IS SUSPND : %d\n",
		uc_fw_stat->rx_ind_ring_size,
		uc_fw_stat->rx_ind_ring_dbell_ind_val,
		uc_fw_stat->rx_ind_ring_dbell_ind_cached_val,
		uc_fw_stat->rx_ind_ring_rd_idx_cached_val,
		uc_fw_stat->rx_refill_idx,
		uc_fw_stat->rx_num_pkts_indicated,
		uc_fw_stat->rx_buf_refilled,
		uc_fw_stat->rx_num_ind_drop_no_space,
		uc_fw_stat->rx_num_ind_drop_no_buf,
		uc_fw_stat->rx_is_suspend);
}

/**
 * wlan_ipa_print_ipa_wdi_stats - Print IPA WDI stats
 * @ipa_ctx: IPA context
 *
 * Return: None
 */
static void wlan_ipa_print_ipa_wdi_stats(struct wlan_ipa_priv *ipa_ctx)
{
	qdf_ipa_hw_stats_wdi_info_data_t ipa_stat;

	qdf_ipa_get_wdi_stats(&ipa_stat);

	ipa_info("\n==== IPA WDI TX STATS ====\n"
		"NUM PROCD : %d\n"
		"CE DBELL : 0x%x\n"
		"NUM DBELL FIRED : %d\n"
		"COMP RNG FULL : %d\n"
		"COMP RNG EMPT : %d\n"
		"COMP RNG USE HGH : %d\n"
		"COMP RNG USE LOW : %d\n"
		"BAM FIFO FULL : %d\n"
		"BAM FIFO EMPT : %d\n"
		"BAM FIFO USE HGH : %d\n"
		"BAM FIFO USE LOW : %d\n"
		"NUM DBELL : %d\n"
		"NUM UNEXP DBELL : %d\n"
		"NUM BAM INT HDL : 0x%x\n"
		"NUM QMB INT HDL : 0x%x\n",
		ipa_stat.tx_ch_stats.num_pkts_processed,
		ipa_stat.tx_ch_stats.copy_engine_doorbell_value,
		ipa_stat.tx_ch_stats.num_db_fired,
		ipa_stat.tx_ch_stats.tx_comp_ring_stats.ringFull,
		ipa_stat.tx_ch_stats.tx_comp_ring_stats.ringEmpty,
		ipa_stat.tx_ch_stats.tx_comp_ring_stats.ringUsageHigh,
		ipa_stat.tx_ch_stats.tx_comp_ring_stats.ringUsageLow,
		ipa_stat.tx_ch_stats.bam_stats.bamFifoFull,
		ipa_stat.tx_ch_stats.bam_stats.bamFifoEmpty,
		ipa_stat.tx_ch_stats.bam_stats.bamFifoUsageHigh,
		ipa_stat.tx_ch_stats.bam_stats.bamFifoUsageLow,
		ipa_stat.tx_ch_stats.num_db,
		ipa_stat.tx_ch_stats.num_unexpected_db,
		ipa_stat.tx_ch_stats.num_bam_int_handled,
		ipa_stat.tx_ch_stats.num_qmb_int_handled);

	ipa_info("\n==== IPA WDI RX STATS ====\n"
		"MAX OST PKT : %d\n"
		"NUM PKT PRCSD : %d\n"
		"RNG RP : 0x%x\n"
		"IND RNG FULL : %d\n"
		"IND RNG EMPT : %d\n"
		"IND RNG USE HGH : %d\n"
		"IND RNG USE LOW : %d\n"
		"BAM FIFO FULL : %d\n"
		"BAM FIFO EMPT : %d\n"
		"BAM FIFO USE HGH : %d\n"
		"BAM FIFO USE LOW : %d\n"
		"NUM DB : %d\n"
		"NUM UNEXP DB : %d\n"
		"NUM BAM INT HNDL : 0x%x\n",
		ipa_stat.rx_ch_stats.max_outstanding_pkts,
		ipa_stat.rx_ch_stats.num_pkts_processed,
		ipa_stat.rx_ch_stats.rx_ring_rp_value,
		ipa_stat.rx_ch_stats.rx_ind_ring_stats.ringFull,
		ipa_stat.rx_ch_stats.rx_ind_ring_stats.ringEmpty,
		ipa_stat.rx_ch_stats.rx_ind_ring_stats.ringUsageHigh,
		ipa_stat.rx_ch_stats.rx_ind_ring_stats.ringUsageLow,
		ipa_stat.rx_ch_stats.bam_stats.bamFifoFull,
		ipa_stat.rx_ch_stats.bam_stats.bamFifoEmpty,
		ipa_stat.rx_ch_stats.bam_stats.bamFifoUsageHigh,
		ipa_stat.rx_ch_stats.bam_stats.bamFifoUsageLow,
		ipa_stat.rx_ch_stats.num_db,
		ipa_stat.rx_ch_stats.num_unexpected_db,
		ipa_stat.rx_ch_stats.num_bam_int_handled);
}

void wlan_ipa_uc_info(struct wlan_ipa_priv *ipa_ctx)
{
	wlan_ipa_print_session_info(ipa_ctx);
}

void wlan_ipa_uc_stat(struct wlan_ipa_priv *ipa_ctx)
{
	/* IPA IPA TX/RX stats */
	wlan_ipa_print_txrx_stats(ipa_ctx);
	/* IPA WDI stats */
	wlan_ipa_print_ipa_wdi_stats(ipa_ctx);
	/* WLAN FW WDI stats */
	wlan_ipa_uc_stat_request(ipa_ctx, WLAN_IPA_UC_STAT_REASON_DEBUG);
}

#ifdef FEATURE_METERING

#ifdef WDI3_STATS_UPDATE
#ifdef WDI3_STATS_BW_MONITOR
/**
 * __wlan_ipa_wdi_meter_notifier_cb() - WLAN to IPA callback handler.
 * IPA calls to get WLAN stats or set quota limit.
 * @priv: pointer to private data registered with IPA (we register a
 *	  pointer to the IPA context)
 * @evt: the IPA event which triggered the callback
 * @data: data associated with the event
 *
 * Return: None
 */
static void __wlan_ipa_wdi_meter_notifier_cb(qdf_ipa_wdi_meter_evt_type_t evt,
					     void *data)
{
	struct wlan_ipa_priv *ipa_ctx = wlan_ipa_get_obj_context();
	struct qdf_ipa_inform_wlan_bw *bw_info;
	uint8_t bw_level_index;
	uint64_t throughput;

	if (evt != IPA_INFORM_WLAN_BW)
		return;

	bw_info = data;
	bw_level_index = QDF_IPA_INFORM_WLAN_BW_INDEX(bw_info);
	throughput = QDF_IPA_INFORM_WLAN_BW_THROUGHPUT(bw_info);
	ipa_debug("bw_info idx:%d tp:%llu", bw_level_index, throughput);

	if (bw_level_index == ipa_ctx->curr_bw_level)
		return;

	if (bw_level_index == WLAN_IPA_BW_LEVEL_LOW) {
		cdp_ipa_set_perf_level(ipa_ctx->dp_soc,
				       QDF_IPA_CLIENT_WLAN2_CONS,
				       ipa_ctx->config->ipa_bw_low);
		ipa_ctx->curr_bw_level = WLAN_IPA_BW_LEVEL_LOW;
	} else if (bw_level_index == WLAN_IPA_BW_LEVEL_MEDIUM) {
		cdp_ipa_set_perf_level(ipa_ctx->dp_soc,
				       QDF_IPA_CLIENT_WLAN2_CONS,
				       ipa_ctx->config->ipa_bw_medium);
		ipa_ctx->curr_bw_level = WLAN_IPA_BW_LEVEL_MEDIUM;
	} else if (bw_level_index == WLAN_IPA_BW_LEVEL_HIGH) {
		cdp_ipa_set_perf_level(ipa_ctx->dp_soc,
				       QDF_IPA_CLIENT_WLAN2_CONS,
				       ipa_ctx->config->ipa_bw_high);
		ipa_ctx->curr_bw_level = WLAN_IPA_BW_LEVEL_HIGH;
	}

	ipa_debug("Requested BW level: %d", ipa_ctx->curr_bw_level);
}

#else
static void __wlan_ipa_wdi_meter_notifier_cb(qdf_ipa_wdi_meter_evt_type_t evt,
					     void *data)
{
}
#endif

void wlan_ipa_update_tx_stats(struct wlan_ipa_priv *ipa_ctx, uint64_t sta_tx,
			      uint64_t ap_tx)
{
	qdf_ipa_wdi_tx_info_t tx_stats;

	if (!qdf_atomic_read(&ipa_ctx->stats_quota))
		return;

	QDF_IPA_WDI_TX_INFO_STA_TX_BYTES(&tx_stats) = sta_tx;
	QDF_IPA_WDI_TX_INFO_SAP_TX_BYTES(&tx_stats) = ap_tx;

	qdf_ipa_wdi_wlan_stats(&tx_stats);
}

#else

/**
 * wlan_ipa_uc_sharing_stats_request() - Get IPA stats from IPA.
 * @ipa_ctx: IPA context
 * @reset_stats: reset stat countis after response
 *
 * Return: None
 */
static void wlan_ipa_uc_sharing_stats_request(struct wlan_ipa_priv *ipa_ctx,
					      uint8_t reset_stats)
{
	qdf_mutex_acquire(&ipa_ctx->ipa_lock);
	if (false == ipa_ctx->resource_loading) {
		qdf_mutex_release(&ipa_ctx->ipa_lock);
		cdp_ipa_uc_get_share_stats(ipa_ctx->dp_soc, ipa_ctx->dp_pdev_id,
					   reset_stats);
	} else {
		qdf_mutex_release(&ipa_ctx->ipa_lock);
	}
}

/**
 * wlan_ipa_uc_set_quota() - Set quota limit bytes from IPA.
 * @ipa_ctx: IPA context
 * @set_quota: when 1, FW starts quota monitoring
 * @quota_bytes: quota limit in bytes
 *
 * Return: None
 */
static void wlan_ipa_uc_set_quota(struct wlan_ipa_priv *ipa_ctx,
				  uint8_t set_quota,
				  uint64_t quota_bytes)
{
	ipa_info("SET_QUOTA: set_quota=%d, quota_bytes=%llu",
		 set_quota, quota_bytes);

	qdf_mutex_acquire(&ipa_ctx->ipa_lock);
	if (false == ipa_ctx->resource_loading) {
		qdf_mutex_release(&ipa_ctx->ipa_lock);
		cdp_ipa_uc_set_quota(ipa_ctx->dp_soc, ipa_ctx->dp_pdev_id,
				     quota_bytes);
	} else {
		qdf_mutex_release(&ipa_ctx->ipa_lock);
	}
}

/**
 * __wlan_ipa_wdi_meter_notifier_cb() - WLAN to IPA callback handler.
 * IPA calls to get WLAN stats or set quota limit.
 * @priv: pointer to private data registered with IPA (we register a
 *	  pointer to the IPA context)
 * @evt: the IPA event which triggered the callback
 * @data: data associated with the event
 *
 * Return: None
 */
static void __wlan_ipa_wdi_meter_notifier_cb(qdf_ipa_wdi_meter_evt_type_t evt,
					     void *data)
{
	struct wlan_ipa_priv *ipa_ctx = wlan_ipa_get_obj_context();
	struct wlan_ipa_iface_context *iface_ctx;
	qdf_ipa_get_wdi_sap_stats_t *wdi_sap_stats;
	qdf_ipa_set_wifi_quota_t *ipa_set_quota;
	QDF_STATUS status;

	ipa_debug("event=%d", evt);

	iface_ctx = wlan_ipa_get_iface(ipa_ctx, QDF_STA_MODE);
	if (!iface_ctx) {
		ipa_err_rl("IPA uC share stats failed - no iface");
		return;
	}

	switch (evt) {
	case IPA_GET_WDI_SAP_STATS:
		/* fill-up ipa_get_wdi_sap_stats structure after getting
		 * ipa_uc_fw_stats from FW
		 */
		wdi_sap_stats = data;

		qdf_event_reset(&ipa_ctx->ipa_uc_sharing_stats_comp);
		wlan_ipa_uc_sharing_stats_request(ipa_ctx,
			QDF_IPA_GET_WDI_SAP_STATS_RESET_STATS(wdi_sap_stats));
		status = qdf_wait_for_event_completion(
			&ipa_ctx->ipa_uc_sharing_stats_comp,
			IPA_UC_SHARING_STATES_WAIT_TIME);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			ipa_err("IPA uC share stats request timed out");
			QDF_IPA_GET_WDI_SAP_STATS_STATS_VALID(wdi_sap_stats)
				= 0;
		} else {
			QDF_IPA_GET_WDI_SAP_STATS_STATS_VALID(wdi_sap_stats)
				= 1;

			QDF_IPA_GET_WDI_SAP_STATS_IPV4_RX_PACKETS(wdi_sap_stats)
				= ipa_ctx->ipa_sharing_stats.ipv4_rx_packets;
			QDF_IPA_GET_WDI_SAP_STATS_IPV4_RX_BYTES(wdi_sap_stats)
				= ipa_ctx->ipa_sharing_stats.ipv4_rx_bytes;
			QDF_IPA_GET_WDI_SAP_STATS_IPV6_RX_PACKETS(wdi_sap_stats)
				= ipa_ctx->ipa_sharing_stats.ipv6_rx_packets;
			QDF_IPA_GET_WDI_SAP_STATS_IPV6_RX_BYTES(wdi_sap_stats)
				= ipa_ctx->ipa_sharing_stats.ipv6_rx_bytes;
			QDF_IPA_GET_WDI_SAP_STATS_IPV4_TX_PACKETS(wdi_sap_stats)
				= ipa_ctx->ipa_sharing_stats.ipv4_tx_packets;
			QDF_IPA_GET_WDI_SAP_STATS_IPV4_TX_BYTES(wdi_sap_stats)
				= ipa_ctx->ipa_sharing_stats.ipv4_tx_bytes;
			QDF_IPA_GET_WDI_SAP_STATS_IPV6_TX_PACKETS(wdi_sap_stats)
				= ipa_ctx->ipa_sharing_stats.ipv6_tx_packets;
			QDF_IPA_GET_WDI_SAP_STATS_IPV6_TX_BYTES(wdi_sap_stats)
				= ipa_ctx->ipa_sharing_stats.ipv6_tx_bytes;
		}
		break;

	case IPA_SET_WIFI_QUOTA:
		/* get ipa_set_wifi_quota structure from IPA and pass to FW
		 * through quota_exceeded field in ipa_uc_fw_stats
		 */
		ipa_set_quota = data;

		qdf_event_reset(&ipa_ctx->ipa_uc_set_quota_comp);
		wlan_ipa_uc_set_quota(ipa_ctx, ipa_set_quota->set_quota,
				      ipa_set_quota->quota_bytes);

		status = qdf_wait_for_event_completion(
				&ipa_ctx->ipa_uc_set_quota_comp,
				IPA_UC_SET_QUOTA_WAIT_TIME);
		if (!QDF_IS_STATUS_SUCCESS(status)) {
			ipa_err("IPA uC set quota request timed out");
			QDF_IPA_SET_WIFI_QUOTA_SET_VALID(ipa_set_quota)	= 0;
		} else {
			QDF_IPA_SET_WIFI_QUOTA_BYTES(ipa_set_quota) =
				((uint64_t)(ipa_ctx->ipa_quota_rsp.quota_hi)
				  <<32)|ipa_ctx->ipa_quota_rsp.quota_lo;
			QDF_IPA_SET_WIFI_QUOTA_SET_VALID(ipa_set_quota)	=
				ipa_ctx->ipa_quota_rsp.success;
		}
		break;

	default:
		break;
	}
}

QDF_STATUS wlan_ipa_uc_op_metering(struct wlan_ipa_priv *ipa_ctx,
				   struct op_msg_type *op_msg)
{
	struct op_msg_type *msg = op_msg;
	struct ipa_uc_sharing_stats *uc_sharing_stats;
	struct ipa_uc_quota_rsp *uc_quota_rsp;
	struct ipa_uc_quota_ind *uc_quota_ind;
	struct wlan_ipa_iface_context *iface_ctx;
	uint64_t quota_bytes;

	if (msg->op_code == WLAN_IPA_UC_OPCODE_SHARING_STATS) {
		/* fill-up ipa_uc_sharing_stats structure from FW */
		uc_sharing_stats = (struct ipa_uc_sharing_stats *)
			     ((uint8_t *)op_msg + sizeof(struct op_msg_type));

		memcpy(&ipa_ctx->ipa_sharing_stats, uc_sharing_stats,
		       sizeof(struct ipa_uc_sharing_stats));

		qdf_event_set(&ipa_ctx->ipa_uc_sharing_stats_comp);
	} else if (msg->op_code == WLAN_IPA_UC_OPCODE_QUOTA_RSP) {
		/* received set quota response */
		uc_quota_rsp = (struct ipa_uc_quota_rsp *)
			     ((uint8_t *)op_msg + sizeof(struct op_msg_type));

		memcpy(&ipa_ctx->ipa_quota_rsp, uc_quota_rsp,
		       sizeof(struct ipa_uc_quota_rsp));

		qdf_event_set(&ipa_ctx->ipa_uc_set_quota_comp);
	} else if (msg->op_code == WLAN_IPA_UC_OPCODE_QUOTA_IND) {
		/* hit quota limit */
		uc_quota_ind = (struct ipa_uc_quota_ind *)
			     ((uint8_t *)op_msg + sizeof(struct op_msg_type));

		ipa_ctx->ipa_quota_ind.quota_bytes =
					uc_quota_ind->quota_bytes;

		/* send quota exceeded indication to IPA */
		iface_ctx = wlan_ipa_get_iface(ipa_ctx, QDF_STA_MODE);
		quota_bytes = uc_quota_ind->quota_bytes;
		if (iface_ctx)
			qdf_ipa_broadcast_wdi_quota_reach_ind(
							iface_ctx->dev->ifindex,
							quota_bytes);
		else
			ipa_err("Failed quota_reach_ind: NULL interface");
	} else {
		return QDF_STATUS_E_INVAL;
	}

	return QDF_STATUS_SUCCESS;
}
#endif /* WDI3_STATS_UPDATE */

/**
 * wlan_ipa_wdi_meter_notifier_cb() - SSR wrapper for
 * __wlan_ipa_wdi_meter_notifier_cb
 * @priv: pointer to private data registered with IPA (we register a
 *	  pointer to the IPA context)
 * @evt: the IPA event which triggered the callback
 * @data: data associated with the event
 *
 * Return: None
 */
void wlan_ipa_wdi_meter_notifier_cb(qdf_ipa_wdi_meter_evt_type_t evt,
				    void *data)
{
	struct qdf_op_sync *op_sync;

	if (qdf_op_protect(&op_sync))
		return;

	__wlan_ipa_wdi_meter_notifier_cb(evt, data);

	qdf_op_unprotect(op_sync);
}
#endif /* FEATURE_METERING */
