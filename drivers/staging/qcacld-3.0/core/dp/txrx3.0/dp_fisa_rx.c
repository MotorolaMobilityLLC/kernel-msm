/*
 * Copyright (c) 2020-2021, The Linux Foundation. All rights reserved.
 *
 * Permission to use, copy, modify, and/or distribute this software for any
 * purpose with or without fee is hereby granted, provided that the above
 * copyright notice and this permission notice appear in all copies.
 *
 * THE SOFTWARE IS PROVIDED "AS IS" AND THE AUTHOR DISCLAIMS ALL WARRANTIES
 * WITH REGARD TO THIS SOFTWARE INCLUDING ALL IMPLIED WARRANTIES OF
 * MERCHANTABILITY AND FITNESS. IN NO EVENT SHALL THE AUTHOR BE LIABLE FOR
 * ANY SPECIAL, DIRECT, INDIRECT, OR CONSEQUENTIAL DAMAGES OR ANY DAMAGES
 * WHATSOEVER RESULTING FROM LOSS OF USE, DATA OR PROFITS, WHETHER IN AN
 * ACTION OF CONTRACT, NEGLIGENCE OR OTHER TORTIOUS ACTION, ARISING OUT OF
 * OR IN CONNECTION WITH THE USE OR PERFORMANCE OF THIS SOFTWARE.
 */

#include <dp_types.h>
#include <dp_fisa_rx.h>
#include "hal_rx_flow.h"
#include "dp_htt.h"
#include "dp_internal.h"
#include <enet.h>
#include <linux/skbuff.h>
#include "hif.h"

static void dp_rx_fisa_flush_flow_wrap(struct dp_fisa_rx_sw_ft *sw_ft);

/*
 * Used by FW to route RX packets to host REO2SW1 ring if IPA hit
 * RX back pressure.
 */
#define REO_DEST_IND_IPA_REROUTE 2

#if defined(FISA_DEBUG_ENABLE)
/**
 * hex_dump_skb_data() - Helper function to dump skb while debugging
 * @nbuf: Nbuf to be dumped
 * @dump: dump enable/disable dumping
 *
 * Return: NONE
 */
static void hex_dump_skb_data(qdf_nbuf_t nbuf, bool dump)
{
	qdf_nbuf_t next_nbuf;
	int i = 0;

	if (!dump)
		return;

	if (!nbuf)
		return;

	dp_fisa_debug("%ps: skb: %pK skb->next:%pK frag_list %pK skb->data:%pK len %d data_len %d",
		      (void *)_RET_IP_, nbuf, qdf_nbuf_next(nbuf),
		      skb_shinfo(nbuf)->frag_list, qdf_nbuf_data(nbuf),
		      nbuf->len, nbuf->data_len);
	QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO_HIGH,
			   nbuf->data, 64);

	next_nbuf = skb_shinfo(nbuf)->frag_list;
	while (next_nbuf) {
		dp_fisa_debug("%d nbuf:%pK nbuf->next:%pK nbuf->data:%pK len %d",
			      i, next_nbuf, qdf_nbuf_next(next_nbuf),
			      qdf_nbuf_data(next_nbuf),
			      qdf_nbuf_len(next_nbuf));
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_DP, QDF_TRACE_LEVEL_INFO_HIGH,
				   qdf_nbuf_data(next_nbuf), 64);
		next_nbuf = qdf_nbuf_next(next_nbuf);
		i++;
	}
}

/**
 * dump_tlvs() - Helper function to dump TLVs of msdu
 * @hal_soc_hdl: Handle to TLV functions
 * @buf: Pointer to TLV header
 * @dbg_level: level control output of TLV dump
 *
 * Return: NONE
 */
static void dump_tlvs(hal_soc_handle_t hal_soc_hdl, uint8_t *buf,
		      uint8_t dbg_level)
{
	uint32_t fisa_aggr_count, fisa_timeout, cumulat_l4_csum, cumulat_ip_len;
	int flow_aggr_cont;

	hal_rx_dump_pkt_tlvs(hal_soc_hdl, buf, dbg_level);

	flow_aggr_cont = hal_rx_get_fisa_flow_agg_continuation(hal_soc_hdl,
							       buf);
	fisa_aggr_count = hal_rx_get_fisa_flow_agg_count(hal_soc_hdl, buf);
	fisa_timeout = hal_rx_get_fisa_timeout(hal_soc_hdl, buf);
	cumulat_l4_csum = hal_rx_get_fisa_cumulative_l4_checksum(hal_soc_hdl,
								 buf);
	cumulat_ip_len = hal_rx_get_fisa_cumulative_ip_length(hal_soc_hdl, buf);

	dp_fisa_debug("flow_aggr_cont %d, fisa_timeout %d, fisa_aggr_count %d, cumulat_l4_csum %d, cumulat_ip_len %d",
	       flow_aggr_cont, fisa_timeout, fisa_aggr_count, cumulat_l4_csum,
	       cumulat_ip_len);
}
#else
static void hex_dump_skb_data(qdf_nbuf_t nbuf, bool dump)
{
}

static void dump_tlvs(hal_soc_handle_t hal_soc_hdl, uint8_t *buf,
		      uint8_t dbg_level)
{
}
#endif

#ifdef WLAN_SUPPORT_RX_FISA_HIST
static
void dp_fisa_record_pkt(struct dp_fisa_rx_sw_ft *fisa_flow, qdf_nbuf_t nbuf,
			uint8_t *rx_tlv_hdr)
{
	uint32_t index;
	struct fisa_pkt_hist_elem *hist_elem;

	if (!rx_tlv_hdr || !fisa_flow || !fisa_flow->pkt_hist)
		return;

	index = fisa_flow->pkt_hist->idx++ % FISA_FLOW_MAX_AGGR_COUNT;
	hist_elem = &fisa_flow->pkt_hist->hist_elem[index];

	hist_elem->ts = qdf_get_log_timestamp();
	qdf_mem_copy(&hist_elem->tlvs, rx_tlv_hdr, sizeof(hist_elem->tlvs));
}
#else
static
void dp_fisa_record_pkt(struct dp_fisa_rx_sw_ft *fisa_flow, qdf_nbuf_t nbuf,
			uint8_t *rx_tlv_hdr)
{
}

#endif

/**
 * nbuf_skip_rx_pkt_tlv() - Function to skip the TLVs and mac header from msdu
 * @hal_soc_hdl: Handle to hal_soc to get the TLV info
 * @nbuf: msdu for which TLVs has to be skipped
 *
 * Return: None
 */
static void nbuf_skip_rx_pkt_tlv(hal_soc_handle_t hal_soc_hdl, qdf_nbuf_t nbuf)
{
	uint8_t *rx_tlv_hdr;
	uint32_t l2_hdr_offset;

	rx_tlv_hdr = qdf_nbuf_data(nbuf);
	l2_hdr_offset = hal_rx_msdu_end_l3_hdr_padding_get(hal_soc_hdl,
							   rx_tlv_hdr);
	qdf_nbuf_pull_head(nbuf, RX_PKT_TLVS_LEN + l2_hdr_offset);
}

/**
 * print_flow_tuple() - Debug function to dump flow tuple
 * @flow_tuple: flow tuple containing tuple info
 *
 * Return: NONE
 */
static
void print_flow_tuple(struct cdp_rx_flow_tuple_info *flow_tuple, char *str,
		      uint32_t size)
{
	qdf_scnprintf(str, size,
		      "dest 0x%x%x%x%x(0x%x) src 0x%x%x%x%x(0x%x) proto 0x%x",
		      flow_tuple->dest_ip_127_96,
		      flow_tuple->dest_ip_95_64,
		      flow_tuple->dest_ip_63_32,
		      flow_tuple->dest_ip_31_0,
		      flow_tuple->dest_port,
		      flow_tuple->src_ip_127_96,
		      flow_tuple->src_ip_95_64,
		      flow_tuple->src_ip_63_32,
		      flow_tuple->src_ip_31_0,
		      flow_tuple->src_port,
		      flow_tuple->l4_protocol);

}

static bool
dp_fisa_is_ipsec_connection(struct cdp_rx_flow_tuple_info *flow_tuple_info)
{
	if (flow_tuple_info->dest_port == IPSEC_PORT ||
	    flow_tuple_info->dest_port == IPSEC_NAT_PORT ||
	    flow_tuple_info->src_port == IPSEC_PORT ||
	    flow_tuple_info->src_port == IPSEC_NAT_PORT)
		return true;

	return false;
}

/**
 * get_flow_tuple_from_nbuf() - Get the flow tuple from msdu
 * @hal_soc_hdl: Handle to hal soc
 * @flow_tuple_info: return argument where the flow is populated
 * @nbuf: msdu from which flow tuple is extracted.
 * @rx_tlv_hdr: Pointer to msdu TLVs
 *
 * Return: None
 */
static void
get_flow_tuple_from_nbuf(hal_soc_handle_t hal_soc_hdl,
			 struct cdp_rx_flow_tuple_info *flow_tuple_info,
			 qdf_nbuf_t nbuf, uint8_t *rx_tlv_hdr)
{
	struct iphdr *iph;
	struct tcphdr *tcph;
	uint32_t ip_hdr_offset = HAL_RX_TLV_GET_IP_OFFSET(rx_tlv_hdr);
	uint32_t tcp_hdr_offset = HAL_RX_TLV_GET_TCP_OFFSET(rx_tlv_hdr);
	uint32_t l2_hdr_offset = hal_rx_msdu_end_l3_hdr_padding_get(hal_soc_hdl,
								    rx_tlv_hdr);

	flow_tuple_info->tuple_populated = true;

	qdf_nbuf_pull_head(nbuf, RX_PKT_TLVS_LEN + l2_hdr_offset);

	iph = (struct iphdr *)(qdf_nbuf_data(nbuf) + ip_hdr_offset);
	tcph = (struct tcphdr *)(qdf_nbuf_data(nbuf) + ip_hdr_offset +
						tcp_hdr_offset);

	flow_tuple_info->dest_ip_31_0 = qdf_ntohl(iph->daddr);
	flow_tuple_info->dest_ip_63_32 = 0;
	flow_tuple_info->dest_ip_95_64 = 0;
	flow_tuple_info->dest_ip_127_96 =
		HAL_IP_DA_SA_PREFIX_IPV4_COMPATIBLE_IPV6;

	flow_tuple_info->src_ip_31_0 = qdf_ntohl(iph->saddr);
	flow_tuple_info->src_ip_63_32 = 0;
	flow_tuple_info->src_ip_95_64 = 0;
	flow_tuple_info->src_ip_127_96 =
		HAL_IP_DA_SA_PREFIX_IPV4_COMPATIBLE_IPV6;

	flow_tuple_info->dest_port = qdf_ntohs(tcph->dest);
	flow_tuple_info->src_port = qdf_ntohs(tcph->source);
	if (dp_fisa_is_ipsec_connection(flow_tuple_info))
		flow_tuple_info->is_exception = 1;
	else
		flow_tuple_info->is_exception = 0;

	flow_tuple_info->l4_protocol = iph->protocol;
	dp_fisa_debug("l4_protocol %d", flow_tuple_info->l4_protocol);

	qdf_nbuf_push_head(nbuf, RX_PKT_TLVS_LEN + l2_hdr_offset);

	dp_fisa_debug("head_skb: %pK head_skb->next:%pK head_skb->data:%pK len %d data_len %d",
		      nbuf, qdf_nbuf_next(nbuf), qdf_nbuf_data(nbuf), nbuf->len,
		      nbuf->data_len);
}

/**
 * dp_rx_fisa_setup_hw_fse() - Populate flow so as to update DDR flow table
 * @fisa_hdl: Handle fisa context
 * @hashed_flow_idx: Index to flow table
 * @rx_flow_info: tuple to be populated in flow table
 * @flow_steer_info: REO index to which flow to be steered
 *
 * Return: Pointer to DDR flow table entry
 */
static void *
dp_rx_fisa_setup_hw_fse(struct dp_rx_fst *fisa_hdl,
			uint32_t hashed_flow_idx,
			struct cdp_rx_flow_tuple_info *rx_flow_info,
			uint32_t flow_steer_info)
{
	struct hal_rx_flow flow;
	void *hw_fse;

	flow.reo_destination_indication = flow_steer_info;
	flow.fse_metadata = 0xDEADBEEF;
	flow.tuple_info.dest_ip_127_96 = rx_flow_info->dest_ip_127_96;
	flow.tuple_info.dest_ip_95_64 = rx_flow_info->dest_ip_95_64;
	flow.tuple_info.dest_ip_63_32 =	rx_flow_info->dest_ip_63_32;
	flow.tuple_info.dest_ip_31_0 = rx_flow_info->dest_ip_31_0;
	flow.tuple_info.src_ip_127_96 =	rx_flow_info->src_ip_127_96;
	flow.tuple_info.src_ip_95_64 = rx_flow_info->src_ip_95_64;
	flow.tuple_info.src_ip_63_32 = rx_flow_info->src_ip_63_32;
	flow.tuple_info.src_ip_31_0 = rx_flow_info->src_ip_31_0;
	flow.tuple_info.dest_port = rx_flow_info->dest_port;
	flow.tuple_info.src_port = rx_flow_info->src_port;
	flow.tuple_info.l4_protocol = rx_flow_info->l4_protocol;
	flow.reo_destination_handler = HAL_RX_FSE_REO_DEST_FT;
	hw_fse = hal_rx_flow_setup_fse(fisa_hdl->soc_hdl->hal_soc,
				       fisa_hdl->hal_rx_fst, hashed_flow_idx,
				       &flow);

	return hw_fse;
}

#ifdef DP_FT_LOCK_HISTORY
struct dp_ft_lock_history ft_lock_hist[MAX_REO_DEST_RINGS];

/**
 * dp_rx_fisa_record_ft_lock_event() - Record FT lock/unlock events
 * @reo_id: REO ID
 * @func: caller function
 * @type: lock/unlock event type
 *
 * Return: None
 */
static void dp_rx_fisa_record_ft_lock_event(uint8_t reo_id, const char *func,
					    enum dp_ft_lock_event_type type)
{
	struct dp_ft_lock_history *lock_hist;
	struct dp_ft_lock_record *record;
	uint32_t record_idx;

	if (reo_id >= MAX_REO_DEST_RINGS)
		return;

	lock_hist = &ft_lock_hist[reo_id];
	record_idx = lock_hist->record_idx % DP_FT_LOCK_MAX_RECORDS;
	ft_lock_hist->record_idx++;

	record = &lock_hist->ft_lock_rec[record_idx];

	record->func = func;
	record->cpu_id = qdf_get_cpu();
	record->timestamp = qdf_get_log_timestamp();
	record->type = type;
}

/**
 * __dp_rx_fisa_acquire_ft_lock() - Acquire lock which protects SW FT entries
 * @fisa_hdl: Handle to fisa context
 * @reo_id: REO ID
 *
 * Return: None
 */
static inline void
__dp_rx_fisa_acquire_ft_lock(struct dp_rx_fst *fisa_hdl,
			     uint8_t reo_id, const char *func)
{
	if (!fisa_hdl->flow_deletion_supported)
		return;

	qdf_spin_lock_bh(&fisa_hdl->dp_rx_sw_ft_lock[reo_id]);
	dp_rx_fisa_record_ft_lock_event(reo_id, func, DP_FT_LOCK_EVENT);
}

/**
 * __dp_rx_fisa_release_ft_lock() - Release lock which protects SW FT entries
 * @fisa_hdl: Handle to fisa context
 * @reo_id: REO ID
 *
 * Return: None
 */
static inline void
__dp_rx_fisa_release_ft_lock(struct dp_rx_fst *fisa_hdl,
			     uint8_t reo_id, const char *func)
{
	if (!fisa_hdl->flow_deletion_supported)
		return;

	qdf_spin_unlock_bh(&fisa_hdl->dp_rx_sw_ft_lock[reo_id]);
	dp_rx_fisa_record_ft_lock_event(reo_id, func, DP_FT_UNLOCK_EVENT);
}

#define dp_rx_fisa_acquire_ft_lock(fisa_hdl, reo_id) \
	__dp_rx_fisa_acquire_ft_lock(fisa_hdl, reo_id, __func__)

#define dp_rx_fisa_release_ft_lock(fisa_hdl, reo_id) \
	__dp_rx_fisa_release_ft_lock(fisa_hdl, reo_id, __func__)

#else
/**
 * dp_rx_fisa_acquire_ft_lock() - Acquire lock which protects SW FT entries
 * @fisa_hdl: Handle to fisa context
 * @reo_id: REO ID
 *
 * Return: None
 */
static inline void
dp_rx_fisa_acquire_ft_lock(struct dp_rx_fst *fisa_hdl, uint8_t reo_id)
{
	if (fisa_hdl->flow_deletion_supported)
		qdf_spin_lock_bh(&fisa_hdl->dp_rx_sw_ft_lock[reo_id]);
}

/**
 * dp_rx_fisa_release_ft_lock() - Release lock which protects SW FT entries
 * @fisa_hdl: Handle to fisa context
 * @reo_id: REO ID
 *
 * Return: None
 */
static inline void
dp_rx_fisa_release_ft_lock(struct dp_rx_fst *fisa_hdl, uint8_t reo_id)
{
	if (fisa_hdl->flow_deletion_supported)
		qdf_spin_unlock_bh(&fisa_hdl->dp_rx_sw_ft_lock[reo_id]);
}
#endif /* DP_FT_LOCK_HISTORY */

/**
 * dp_rx_fisa_setup_cmem_fse() - Setup the flow search entry in HW CMEM
 * @fisa_hdl: Handle to fisa context
 * @hashed_flow_idx: Index to flow table
 * @rx_flow_info: tuple to be populated in flow table
 * @flow_steer_info: REO index to which flow to be steered
 *
 * Return: Offset to the FSE entry in CMEM
 */
static uint32_t
dp_rx_fisa_setup_cmem_fse(struct dp_rx_fst *fisa_hdl, uint32_t hashed_flow_idx,
			  struct cdp_rx_flow_tuple_info *rx_flow_info,
			  uint32_t flow_steer_info)
{
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	struct hal_rx_flow flow;

	sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
				fisa_hdl->base)[hashed_flow_idx]);
	sw_ft_entry->metadata = ++fisa_hdl->meta_counter;

	flow.reo_destination_indication = flow_steer_info;
	flow.fse_metadata = sw_ft_entry->metadata;
	flow.tuple_info.dest_ip_127_96 = rx_flow_info->dest_ip_127_96;
	flow.tuple_info.dest_ip_95_64 = rx_flow_info->dest_ip_95_64;
	flow.tuple_info.dest_ip_63_32 =	rx_flow_info->dest_ip_63_32;
	flow.tuple_info.dest_ip_31_0 = rx_flow_info->dest_ip_31_0;
	flow.tuple_info.src_ip_127_96 =	rx_flow_info->src_ip_127_96;
	flow.tuple_info.src_ip_95_64 = rx_flow_info->src_ip_95_64;
	flow.tuple_info.src_ip_63_32 = rx_flow_info->src_ip_63_32;
	flow.tuple_info.src_ip_31_0 = rx_flow_info->src_ip_31_0;
	flow.tuple_info.dest_port = rx_flow_info->dest_port;
	flow.tuple_info.src_port = rx_flow_info->src_port;
	flow.tuple_info.l4_protocol = rx_flow_info->l4_protocol;
	flow.reo_destination_handler = HAL_RX_FSE_REO_DEST_FT;

	return hal_rx_flow_setup_cmem_fse(fisa_hdl->soc_hdl->hal_soc,
					  fisa_hdl->cmem_ba, hashed_flow_idx,
					  &flow);
}

/**
 * dp_rx_fisa_update_sw_ft_entry() - Helper function to update few SW FT entry
 * @sw_ft_entry: Pointer to softerware flow tabel entry
 * @flow_hash: flow_hash for the flow
 * @vdev: Saving dp_vdev in FT later used in the flushing the flow
 * @flow_id: Flow ID of the flow
 *
 * Return: NONE
 */
static void dp_rx_fisa_update_sw_ft_entry(struct dp_fisa_rx_sw_ft *sw_ft_entry,
					  uint32_t flow_hash,
					  struct dp_vdev *vdev,
					  struct dp_soc *soc_hdl,
					  uint32_t flow_id)
{
	sw_ft_entry->flow_hash = flow_hash;
	sw_ft_entry->flow_id = flow_id;
	sw_ft_entry->vdev = vdev;
	sw_ft_entry->soc_hdl = soc_hdl;
}

/**
 * is_same_flow() - Function to compare flow tuple to decide if they match
 * @tuple1: flow tuple 1
 * @tuple2: flow tuple 2
 *
 * Return: true if they match, false if they differ
 */
static bool is_same_flow(struct cdp_rx_flow_tuple_info *tuple1,
			 struct cdp_rx_flow_tuple_info *tuple2)
{
	if ((tuple1->src_port ^ tuple2->src_port) |
	    (tuple1->dest_port ^ tuple2->dest_port) |
	    (tuple1->src_ip_31_0 ^ tuple2->src_ip_31_0) |
	    (tuple1->src_ip_63_32 ^ tuple2->src_ip_63_32) |
	    (tuple1->src_ip_95_64 ^ tuple2->src_ip_95_64) |
	    (tuple1->src_ip_127_96 ^ tuple2->src_ip_127_96) |
	    (tuple1->dest_ip_31_0 ^ tuple2->dest_ip_31_0) |
	    /* DST IP check not required? */
	    (tuple1->dest_ip_63_32 ^ tuple2->dest_ip_63_32) |
	    (tuple1->dest_ip_95_64 ^ tuple2->dest_ip_95_64) |
	    (tuple1->dest_ip_127_96 ^ tuple2->dest_ip_127_96) |
	    (tuple1->l4_protocol ^ tuple2->l4_protocol))
		return false;
	else
		return true;
}

/**
 * dp_rx_fisa_add_ft_entry() - Add new flow to HW and SW FT if it is not added
 * @vdev: Handle DP vdev to save in SW flow table
 * @fisa_hdl: handle to FISA context
 * @nbuf: nbuf belonging to new flow
 * @rx_tlv_hdr: Pointer to TLV header
 * @flow_idx_hash: Hashed flow index
 * @reo_dest_indication: Reo destination indication for nbuf
 *
 * Return: pointer to sw FT entry on success, NULL otherwise
 */
static struct dp_fisa_rx_sw_ft *
dp_rx_fisa_add_ft_entry(struct dp_vdev *vdev,
			struct dp_rx_fst *fisa_hdl,
			qdf_nbuf_t nbuf,
			uint8_t *rx_tlv_hdr,
			uint32_t flow_idx_hash,
			uint32_t reo_dest_indication)
{
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	uint32_t flow_hash;
	uint32_t hashed_flow_idx;
	uint32_t skid_count = 0, max_skid_length;
	struct cdp_rx_flow_tuple_info rx_flow_tuple_info;
	bool is_fst_updated = false;
	bool is_flow_tcp, is_flow_udp, is_flow_ipv6;
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->soc_hdl->hal_soc;
	uint32_t reo_id = QDF_NBUF_CB_RX_CTX_ID(nbuf);

	is_flow_tcp = HAL_RX_TLV_GET_TCP_PROTO(rx_tlv_hdr);
	is_flow_udp = HAL_RX_TLV_GET_UDP_PROTO(rx_tlv_hdr);
	is_flow_ipv6 = HAL_RX_TLV_GET_IPV6(rx_tlv_hdr);

	if (is_flow_ipv6 || !(is_flow_tcp || is_flow_udp)) {
		dp_fisa_debug("Not UDP or TCP IPV4 flow");
		return NULL;
	}

	rx_flow_tuple_info.tuple_populated = false;
	flow_hash = flow_idx_hash;
	hashed_flow_idx = flow_hash & fisa_hdl->hash_mask;
	max_skid_length = fisa_hdl->max_skid_length;

	dp_fisa_debug("flow_hash 0x%x hashed_flow_idx 0x%x", flow_hash,
		      hashed_flow_idx);
	dp_fisa_debug("max_skid_length 0x%x", max_skid_length);
	qdf_spin_lock_bh(&fisa_hdl->dp_rx_fst_lock);
	do {
		sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
					fisa_hdl->base)[hashed_flow_idx]);
		if (!sw_ft_entry->is_populated) {
			/* Add SW FT entry */
			dp_rx_fisa_update_sw_ft_entry(sw_ft_entry,
						      flow_hash, vdev,
						      fisa_hdl->soc_hdl,
						      hashed_flow_idx);
			if (!rx_flow_tuple_info.tuple_populated)
				get_flow_tuple_from_nbuf(hal_soc_hdl,
							 &rx_flow_tuple_info,
							 nbuf, rx_tlv_hdr);

			/* Add HW FT entry */
			sw_ft_entry->hw_fse =
				dp_rx_fisa_setup_hw_fse(fisa_hdl,
							hashed_flow_idx,
							&rx_flow_tuple_info,
							reo_dest_indication);
			sw_ft_entry->is_populated = true;
			sw_ft_entry->napi_id = reo_id;
			sw_ft_entry->reo_dest_indication = reo_dest_indication;
			sw_ft_entry->flow_id_toeplitz =
				hal_rx_msdu_start_toeplitz_get(rx_tlv_hdr);
			sw_ft_entry->flow_init_ts = qdf_get_log_timestamp();

			qdf_mem_copy(&sw_ft_entry->rx_flow_tuple_info,
				     &rx_flow_tuple_info,
				     sizeof(struct cdp_rx_flow_tuple_info));

			sw_ft_entry->is_flow_tcp = is_flow_tcp;
			sw_ft_entry->is_flow_udp = is_flow_udp;

			is_fst_updated = true;
			fisa_hdl->add_flow_count++;
			break;
		}
		/* else */
		if (!rx_flow_tuple_info.tuple_populated)
			get_flow_tuple_from_nbuf(hal_soc_hdl,
						 &rx_flow_tuple_info,
						 nbuf, rx_tlv_hdr);

		if (is_same_flow(&sw_ft_entry->rx_flow_tuple_info,
				 &rx_flow_tuple_info)) {
			sw_ft_entry->vdev = vdev;
			dp_fisa_debug("It is same flow fse entry idx %d",
				      hashed_flow_idx);
			/* Incoming flow tuple matches with existing
			 * entry. This is subsequent skbs of the same
			 * flow. Earlier entry made is not reflected
			 * yet in FSE cache
			 */
			break;
		}
		/* else */
		/* hash collision move to the next FT entry */
		dp_fisa_debug("Hash collision %d", fisa_hdl->hash_collision_cnt);
		fisa_hdl->hash_collision_cnt++;
#ifdef NOT_YET /* assist Flow eviction algorithm */
	/* uint32_t lru_ft_entry_time = 0xffffffff, lru_ft_entry_idx = 0; */
		if (fisa_hdl->hw_ft_entry->timestamp < lru_ft_entry_time) {
			lru_ft_entry_time = fisa_hdl->hw_ft_entry->timestamp;
			lru_ft_entry_idx = hashed_flow_idx;
		}
#endif
		skid_count++;
		hashed_flow_idx++;
		hashed_flow_idx &= fisa_hdl->hash_mask;
	} while (skid_count <= max_skid_length);

	/*
	 * fisa_hdl->flow_eviction_cnt++;
	 * if (skid_count > max_skid_length)
	 * Remove LRU flow from HW FT
	 * Remove LRU flow from SW FT
	 */
	qdf_spin_unlock_bh(&fisa_hdl->dp_rx_fst_lock);

	if (skid_count > max_skid_length) {
		dp_fisa_debug("Max skid length reached flow cannot be added, evict exiting flow");
		return NULL;
	}

	/**
	 * Send HTT cache invalidation command to firmware to
	 * reflect the flow update
	 */
	if (is_fst_updated &&
	    fisa_hdl->fse_cache_flush_allow &&
	    (qdf_atomic_inc_return(&fisa_hdl->fse_cache_flush_posted) == 1)) {
		/* return 1 after increment implies FSE cache flush message
		 * already posted. so start restart the timer
		 */
		qdf_timer_start(&fisa_hdl->fse_cache_flush_timer,
				FSE_CACHE_FLUSH_TIME_OUT);

	}
	dp_fisa_debug("sw_ft_entry %pK", sw_ft_entry);
	return sw_ft_entry;
}

/**
 * is_flow_idx_valid() - Function to decide if flow_idx TLV is valid
 * @flow_invalid: flow invalid TLV value
 * @flow_timeout: flow timeout TLV value, set when FSE timedout flow search
 *
 * Return: True if flow_idx value is valid
 */
static bool is_flow_idx_valid(bool flow_invalid, bool flow_timeout)
{
	if (!flow_invalid && !flow_timeout)
		return true;
	else
		return false;
}

#ifdef WLAN_SUPPORT_RX_FISA_HIST
/**
 * dp_rx_fisa_get_pkt_hist() - Get ptr to pkt history from rx sw ft entry
 * @ft_entry: sw ft entry
 *
 * Return: ptr to pkt history
 */
static inline struct fisa_pkt_hist *
dp_rx_fisa_get_pkt_hist(struct dp_fisa_rx_sw_ft *ft_entry)
{
	return ft_entry->pkt_hist;
}

/**
 * dp_rx_fisa_set_pkt_hist() - Set rx sw ft entry pkt history
 * @ft_entry: sw ft entry
 * @pkt_hist: pkt history ptr
 *
 * Return: None
 */
static inline void
dp_rx_fisa_set_pkt_hist(struct dp_fisa_rx_sw_ft *ft_entry,
			struct fisa_pkt_hist *pkt_hist)
{
	ft_entry->pkt_hist = pkt_hist;
}
#else
static inline struct fisa_pkt_hist *
dp_rx_fisa_get_pkt_hist(struct dp_fisa_rx_sw_ft *ft_entry)
{
	return NULL;
}

static inline void
dp_rx_fisa_set_pkt_hist(struct dp_fisa_rx_sw_ft *ft_entry,
			struct fisa_pkt_hist *pkt_hist)
{
}
#endif

/**
 * dp_fisa_rx_delete_flow() - Delete a flow from SW and HW FST, currently
 * only applicable when FST is in CMEM
 * @fisa_hdl: handle to FISA context
 * @elem: details of the flow which is being added
 * @hashed_flow_idx: hashed flow idx of the deleting flow
 *
 * Return: None
 */
static void
dp_fisa_rx_delete_flow(struct dp_rx_fst *fisa_hdl,
		       struct dp_fisa_rx_fst_update_elem *elem,
		       uint32_t hashed_flow_idx)
{
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	u8 reo_id;
	struct fisa_pkt_hist *pkt_hist;

	sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
				fisa_hdl->base)[hashed_flow_idx]);
	reo_id = sw_ft_entry->napi_id;

	dp_rx_fisa_acquire_ft_lock(fisa_hdl, reo_id);

	/* Flush the flow before deletion */
	dp_rx_fisa_flush_flow_wrap(sw_ft_entry);

	pkt_hist = dp_rx_fisa_get_pkt_hist(sw_ft_entry);

	memset(sw_ft_entry, 0, sizeof(*sw_ft_entry));

	dp_rx_fisa_set_pkt_hist(sw_ft_entry, pkt_hist);
	dp_rx_fisa_update_sw_ft_entry(sw_ft_entry, elem->flow_idx, elem->vdev,
				      fisa_hdl->soc_hdl, hashed_flow_idx);

	/* Add HW FT entry */
	sw_ft_entry->cmem_offset = dp_rx_fisa_setup_cmem_fse(
					fisa_hdl, hashed_flow_idx,
					&elem->flow_tuple_info,
					elem->reo_dest_indication);

	sw_ft_entry->is_populated = true;
	sw_ft_entry->napi_id = elem->reo_id;
	sw_ft_entry->reo_dest_indication = elem->reo_dest_indication;
	qdf_mem_copy(&sw_ft_entry->rx_flow_tuple_info, &elem->flow_tuple_info,
		     sizeof(struct cdp_rx_flow_tuple_info));

	sw_ft_entry->is_flow_tcp = elem->is_tcp_flow;
	sw_ft_entry->is_flow_udp = elem->is_udp_flow;

	dp_rx_fisa_release_ft_lock(fisa_hdl, reo_id);

	fisa_hdl->add_flow_count++;
	fisa_hdl->del_flow_count++;
}

/**
 * dp_fisa_rx_get_hw_ft_timestamp() - Get timestamp maintained in the HW FSE
 * @fisa_hdl: handle to FISA context
 * @hashed_flow_idx: hashed idx of the flow
 *
 * Return: Timestamp
 */
static uint32_t
dp_fisa_rx_get_hw_ft_timestamp(struct dp_rx_fst *fisa_hdl,
			       uint32_t hashed_flow_idx)
{
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->soc_hdl->hal_soc;
	struct dp_fisa_rx_sw_ft *sw_ft_entry;

	sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
				fisa_hdl->base)[hashed_flow_idx]);

	if (fisa_hdl->fst_in_cmem)
		return hal_rx_flow_get_cmem_fse_timestamp(
				hal_soc_hdl, sw_ft_entry->cmem_offset);

	return ((struct rx_flow_search_entry *)sw_ft_entry->hw_fse)->timestamp;
}

/**
 * dp_fisa_rx_fst_update() - Core logic which helps in Addition/Deletion
 * of flows
 * into/from SW & HW FST
 * @fisa_hdl: handle to FISA context
 * @elem: details of the flow which is being added
 *
 * Return: None
 */
static void dp_fisa_rx_fst_update(struct dp_rx_fst *fisa_hdl,
				  struct dp_fisa_rx_fst_update_elem *elem)
{
	struct cdp_rx_flow_tuple_info *rx_flow_tuple_info;
	uint32_t skid_count = 0, max_skid_length;
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	bool is_fst_updated = false;
	uint32_t hashed_flow_idx;
	uint32_t flow_hash;
	uint32_t lru_ft_entry_time = 0xffffffff;
	uint32_t lru_ft_entry_idx = 0;
	uint32_t timestamp;
	uint32_t reo_dest_indication;

	/* Get the hash from TLV
	 * FSE FT Toeplitz hash is same Common parser hash available in TLV
	 * common parser toeplitz hash is same as FSE toeplitz hash as
	 * toeplitz key is same.
	 */
	flow_hash = elem->flow_idx;
	hashed_flow_idx = flow_hash & fisa_hdl->hash_mask;
	max_skid_length = fisa_hdl->max_skid_length;
	rx_flow_tuple_info = &elem->flow_tuple_info;
	reo_dest_indication = elem->reo_dest_indication;

	dp_fisa_debug("flow_hash 0x%x hashed_flow_idx 0x%x", flow_hash,
		      hashed_flow_idx);
	dp_fisa_debug("max_skid_length 0x%x", max_skid_length);

	do {
		sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
					fisa_hdl->base)[hashed_flow_idx]);
		if (!sw_ft_entry->is_populated) {
			/* Add SW FT entry */
			dp_rx_fisa_update_sw_ft_entry(sw_ft_entry,
						      flow_hash, elem->vdev,
						      fisa_hdl->soc_hdl,
						      hashed_flow_idx);

			/* Add HW FT entry */
			sw_ft_entry->cmem_offset =
				dp_rx_fisa_setup_cmem_fse(fisa_hdl,
							  hashed_flow_idx,
							  rx_flow_tuple_info,
							  reo_dest_indication);
			sw_ft_entry->is_populated = true;
			sw_ft_entry->napi_id = elem->reo_id;
			sw_ft_entry->reo_dest_indication = reo_dest_indication;
			qdf_mem_copy(&sw_ft_entry->rx_flow_tuple_info,
				     rx_flow_tuple_info,
				     sizeof(struct cdp_rx_flow_tuple_info));

			sw_ft_entry->flow_init_ts = qdf_get_log_timestamp();
			sw_ft_entry->is_flow_tcp = elem->is_tcp_flow;
			sw_ft_entry->is_flow_udp = elem->is_udp_flow;

			is_fst_updated = true;
			fisa_hdl->add_flow_count++;
			break;
		}
		/* else */
		/* hash collision move to the next FT entry */
		dp_fisa_debug("Hash collision %d",
			      fisa_hdl->hash_collision_cnt);
		fisa_hdl->hash_collision_cnt++;

		timestamp = dp_fisa_rx_get_hw_ft_timestamp(fisa_hdl,
							   hashed_flow_idx);
		if (timestamp < lru_ft_entry_time) {
			lru_ft_entry_time = timestamp;
			lru_ft_entry_idx = hashed_flow_idx;
		}
		skid_count++;
		hashed_flow_idx++;
		hashed_flow_idx &= fisa_hdl->hash_mask;
	} while (skid_count <= max_skid_length);

	/*
	 * if (skid_count > max_skid_length)
	 * Remove LRU flow from HW FT
	 * Remove LRU flow from SW FT
	 */
	if (skid_count > max_skid_length) {
		dp_fisa_debug("Max skid length reached flow cannot be added, evict exiting flow");
		dp_fisa_rx_delete_flow(fisa_hdl, elem, lru_ft_entry_idx);
		is_fst_updated = true;
	}

	/**
	 * Send HTT cache invalidation command to firmware to
	 * reflect the flow update
	 */
	if (is_fst_updated &&
	    fisa_hdl->fse_cache_flush_allow &&
	    (qdf_atomic_inc_return(&fisa_hdl->fse_cache_flush_posted) == 1)) {
		/* return 1 after increment implies FSE cache flush message
		 * already posted. so start restart the timer
		 */
		qdf_timer_start(&fisa_hdl->fse_cache_flush_timer,
				FSE_CACHE_FLUSH_TIME_OUT);
	}
}

/**
 * dp_fisa_rx_fst_update_work() - Work functions for FST updates
 * @arg: argument passed to the work function
 *
 * Return: None
 */
void dp_fisa_rx_fst_update_work(void *arg)
{
	struct dp_fisa_rx_fst_update_elem *elem;
	struct dp_rx_fst *fisa_hdl = arg;
	qdf_list_node_t *node;
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->soc_hdl->hal_soc;

	if (hif_force_wake_request(((struct hal_soc *)hal_soc_hdl)->hif_handle)) {
		dp_err("Wake up request failed");
		qdf_check_state_before_panic(__func__, __LINE__);
		return;
	}

	qdf_spin_lock_bh(&fisa_hdl->dp_rx_fst_lock);
	while (qdf_list_peek_front(&fisa_hdl->fst_update_list, &node) ==
	       QDF_STATUS_SUCCESS) {
		elem = (struct dp_fisa_rx_fst_update_elem *)node;
		dp_fisa_rx_fst_update(fisa_hdl, elem);
		qdf_list_remove_front(&fisa_hdl->fst_update_list, &node);
		qdf_mem_free(elem);
	}
	qdf_spin_unlock_bh(&fisa_hdl->dp_rx_fst_lock);

	if (hif_force_wake_release(((struct hal_soc *)hal_soc_hdl)->hif_handle)) {
		dp_err("Wake up release failed");
		qdf_check_state_before_panic(__func__, __LINE__);
		return;
	}
}

/**
 * dp_fisa_rx_is_fst_work_queued() - Check if work is already queued for
 * the flow
 * @fisa_hdl: handle to FISA context
 * @flow_idx: Flow index
 *
 * Return: True/False
 */
static inline bool
dp_fisa_rx_is_fst_work_queued(struct dp_rx_fst *fisa_hdl, uint32_t flow_idx)
{
	struct dp_fisa_rx_fst_update_elem *elem;
	qdf_list_node_t *cur_node, *next_node;
	QDF_STATUS status;

	status = qdf_list_peek_front(&fisa_hdl->fst_update_list, &cur_node);
	if (status == QDF_STATUS_E_EMPTY)
		return false;

	do {
		elem = (struct dp_fisa_rx_fst_update_elem *)cur_node;
		if (elem->flow_idx == flow_idx)
			return true;

		status = qdf_list_peek_next(&fisa_hdl->fst_update_list,
					    cur_node, &next_node);
		cur_node = next_node;
	} while (status == QDF_STATUS_SUCCESS);

	return false;
}

/**
 * dp_fisa_rx_queue_fst_update_work() - Queue FST update work
 * @fisa_hdl: Handle to FISA context
 * @flow_idx: Flow index
 * @nbuf: Received RX packet
 * @vdev: DP vdev handle
 *
 * Return: None
 */
static void *
dp_fisa_rx_queue_fst_update_work(struct dp_rx_fst *fisa_hdl, uint32_t flow_idx,
				 qdf_nbuf_t nbuf, struct dp_vdev *vdev)
{
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->soc_hdl->hal_soc;
	struct cdp_rx_flow_tuple_info flow_tuple_info;
	bool is_flow_tcp, is_flow_udp, is_flow_ipv6;
	uint8_t *rx_tlv_hdr = qdf_nbuf_data(nbuf);
	struct dp_fisa_rx_fst_update_elem *elem;
	struct dp_fisa_rx_sw_ft *sw_ft_entry;
	uint32_t hashed_flow_idx;
	uint32_t reo_dest_indication;
	bool found;

	is_flow_tcp = HAL_RX_TLV_GET_TCP_PROTO(rx_tlv_hdr);
	is_flow_udp = HAL_RX_TLV_GET_UDP_PROTO(rx_tlv_hdr);
	is_flow_ipv6 = HAL_RX_TLV_GET_IPV6(rx_tlv_hdr);

	if (is_flow_ipv6 || !(is_flow_tcp || is_flow_udp)) {
		dp_fisa_debug("Not UDP or TCP IPV4 flow");
		return NULL;
	}

	hal_rx_msdu_get_reo_destination_indication(hal_soc_hdl, rx_tlv_hdr,
						   &reo_dest_indication);
	qdf_spin_lock_bh(&fisa_hdl->dp_rx_fst_lock);
	found = dp_fisa_rx_is_fst_work_queued(fisa_hdl, flow_idx);
	qdf_spin_unlock_bh(&fisa_hdl->dp_rx_fst_lock);
	if (found)
		return NULL;

	hashed_flow_idx = flow_idx & fisa_hdl->hash_mask;
	sw_ft_entry = &(((struct dp_fisa_rx_sw_ft *)
				fisa_hdl->base)[hashed_flow_idx]);

	get_flow_tuple_from_nbuf(hal_soc_hdl, &flow_tuple_info,
				 nbuf, rx_tlv_hdr);

	if (sw_ft_entry->is_populated && is_same_flow(
			&sw_ft_entry->rx_flow_tuple_info, &flow_tuple_info))
		return sw_ft_entry;

	elem = qdf_mem_malloc(sizeof(*elem));
	if (!elem) {
		dp_fisa_debug("failed to allocate memory for FST update");
		return NULL;
	}

	qdf_mem_copy(&elem->flow_tuple_info, &flow_tuple_info,
		     sizeof(struct cdp_rx_flow_tuple_info));
	elem->flow_idx = flow_idx;
	elem->is_tcp_flow = is_flow_tcp;
	elem->is_udp_flow = is_flow_udp;
	elem->reo_id = QDF_NBUF_CB_RX_CTX_ID(nbuf);
	elem->reo_dest_indication = reo_dest_indication;
	elem->vdev = vdev;

	qdf_spin_lock_bh(&fisa_hdl->dp_rx_fst_lock);
	qdf_list_insert_back(&fisa_hdl->fst_update_list, &elem->node);
	qdf_spin_unlock_bh(&fisa_hdl->dp_rx_fst_lock);

	qdf_queue_work(fisa_hdl->soc_hdl->osdev, fisa_hdl->fst_update_wq,
		       &fisa_hdl->fst_update_work);

	return NULL;
}

/**
 * dp_fisa_rx_get_sw_ft_entry() - Get SW FT entry for the flow
 * @fisa_hdl: Handle to FISA context
 * @nbuf: Received RX packet
 * @flow_idx: Flow index
 * @vdev: handle to DP vdev
 *
 * Return: SW FT entry
 */
static inline struct dp_fisa_rx_sw_ft *
dp_fisa_rx_get_sw_ft_entry(struct dp_rx_fst *fisa_hdl, qdf_nbuf_t nbuf,
			   uint32_t flow_idx, struct dp_vdev *vdev)
{
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->soc_hdl->hal_soc;
	struct dp_fisa_rx_sw_ft *sw_ft_entry = NULL;
	struct dp_fisa_rx_sw_ft *sw_ft_base;
	uint32_t fse_metadata;
	uint8_t *rx_tlv_hdr;

	sw_ft_base = (struct dp_fisa_rx_sw_ft *)fisa_hdl->base;
	rx_tlv_hdr = qdf_nbuf_data(nbuf);

	if (qdf_unlikely(flow_idx >= fisa_hdl->max_entries)) {
		dp_info("flow_idx is invalid 0x%x", flow_idx);
		hal_rx_dump_pkt_tlvs(hal_soc_hdl, rx_tlv_hdr,
				     QDF_TRACE_LEVEL_INFO_HIGH);
		DP_STATS_INC(fisa_hdl, invalid_flow_index, 1);
		return NULL;
	}

	sw_ft_entry = &sw_ft_base[flow_idx];

	if (!fisa_hdl->flow_deletion_supported) {
		sw_ft_entry->vdev = vdev;
		return sw_ft_entry;
	}

	/* When a flow is deleted, there could be some packets of that flow
	 * with valid flow_idx in the REO queue and arrive at a later time,
	 * compare the metadata for such packets before returning the SW FT
	 * entry to avoid packets getting aggregated with the wrong flow.
	 */
	fse_metadata = hal_rx_msdu_fse_metadata_get(hal_soc_hdl, rx_tlv_hdr);
	if (fisa_hdl->del_flow_count && fse_metadata != sw_ft_entry->metadata)
		return NULL;

	sw_ft_entry->vdev = vdev;
	return sw_ft_entry;
}

/**
 * dp_rx_get_fisa_flow() - Get FT entry corresponding to incoming nbuf
 * @fisa_hdl: handle to FISA context
 * @vdev: handle to DP vdev
 * @nbuf: incoming msdu
 *
 * Return: handle SW FT entry for nbuf flow
 */
static struct dp_fisa_rx_sw_ft *
dp_rx_get_fisa_flow(struct dp_rx_fst *fisa_hdl, struct dp_vdev *vdev,
		    qdf_nbuf_t nbuf)
{
	uint8_t *rx_tlv_hdr;
	uint32_t flow_idx_hash;
	uint32_t tlv_reo_dest_ind;
	uint8_t  ring_reo_dest_ind;
	bool flow_invalid, flow_timeout, flow_idx_valid;
	struct dp_fisa_rx_sw_ft *sw_ft_entry = NULL;
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->soc_hdl->hal_soc;

	if (QDF_NBUF_CB_RX_TCP_PROTO(nbuf))
		return sw_ft_entry;

	rx_tlv_hdr = qdf_nbuf_data(nbuf);
	hal_rx_msdu_get_reo_destination_indication(hal_soc_hdl, rx_tlv_hdr,
						   &tlv_reo_dest_ind);
	ring_reo_dest_ind = qdf_nbuf_get_rx_reo_dest_ind(nbuf);
	/*
	 * Compare reo_destination_indication between reo ring descriptor
	 * and rx_pkt_tlvs, if they are different, then likely these kind
	 * of frames re-injected by FW or touched by other module already,
	 * skip FISA to avoid REO2SW ring mismatch issue for same flow.
	 */
	if (tlv_reo_dest_ind != ring_reo_dest_ind ||
	    REO_DEST_IND_IPA_REROUTE == ring_reo_dest_ind)
		return sw_ft_entry;

	hal_rx_msdu_get_flow_params(hal_soc_hdl, rx_tlv_hdr, &flow_invalid,
				    &flow_timeout, &flow_idx_hash);

	flow_idx_valid = is_flow_idx_valid(flow_invalid, flow_timeout);
	if (flow_idx_valid) {
		sw_ft_entry = dp_fisa_rx_get_sw_ft_entry(fisa_hdl, nbuf,
							 flow_idx_hash, vdev);
		goto print_and_return;
	}

	/* else new flow, add entry to FT */

	if (fisa_hdl->fst_in_cmem)
		return dp_fisa_rx_queue_fst_update_work(fisa_hdl, flow_idx_hash,
							nbuf, vdev);

	sw_ft_entry = dp_rx_fisa_add_ft_entry(vdev, fisa_hdl,
					      nbuf,
					      rx_tlv_hdr,
					      flow_idx_hash,
					      tlv_reo_dest_ind);

print_and_return:
	dp_fisa_debug("nbuf %pK fl_idx 0x%x fl_inv %d fl_timeout %d flow_id_toeplitz %x reo_dest_ind 0x%x",
		      nbuf, flow_idx_hash, flow_invalid, flow_timeout,
		      sw_ft_entry ? sw_ft_entry->flow_id_toeplitz : 0,
		      tlv_reo_dest_ind);

	return sw_ft_entry;
}

#ifdef NOT_YET
/**
 * dp_rx_fisa_aggr_tcp() - Aggregate incoming to TCP nbuf
 * @fisa_flow: Handle to SW flow entry, which holds the aggregated nbuf
 * @nbuf: Incoming nbuf
 *
 * Return: FISA_AGGR_DONE on successful aggregation
 */
static enum fisa_aggr_ret
dp_rx_fisa_aggr_tcp(struct dp_fisa_rx_sw_ft *fisa_flow,	qdf_nbuf_t nbuf)
{
	qdf_nbuf_t head_skb = fisa_flow->head_skb;
	struct iphdr *iph;
	uint32_t tcp_data_len;

	fisa_flow->bytes_aggregated += qdf_nbuf_len(nbuf);
	if (!head_skb) {
		/* First nbuf for the flow */
		dp_fisa_debug("first head skb");
		fisa_flow->head_skb = nbuf;
		return FISA_AGGR_DONE;
	}

	tcp_data_len = (ntohs(iph->tot_len) - sizeof(struct iphdr) -
			sizeof(struct tcphdr));
	qdf_nbuf_pull_head(nbuf, (nbuf->len - tcp_data_len));

	if (qdf_nbuf_get_ext_list(head_skb)) {
		/* this is 3rd skb after head skb, 2nd skb */
		fisa_flow->last_skb->next = nbuf;
	} else {
		/* 1st skb after head skb */
		qdf_nbuf_append_ext_list(head_skb, nbuf,
					 fisa_flow->cumulative_ip_length);
		qdf_nbuf_set_is_frag(head, 1);
	}

	fisa_flow->last_skb = nbuf;
	fisa_flow->aggr_count++;

	/* move it to while flushing the flow, that is update before flushing */
	return FISA_AGGR_DONE;
}
#else
static enum fisa_aggr_ret
dp_rx_fisa_aggr_tcp(struct dp_rx_fst *fisa_hdl,
		    struct dp_fisa_rx_sw_ft *fisa_flow,	qdf_nbuf_t nbuf)
{
	return FISA_AGGR_DONE;
}
#endif

/**
 * get_transport_payload_offset() - Get offset to payload
 * @fisa_hdl: Handle to FISA context
 * @rx_tlv_hdr: TLV hdr pointer
 *
 * Return: Offset value to transport payload
 */
static int get_transport_payload_offset(struct dp_rx_fst *fisa_hdl,
					uint8_t *rx_tlv_hdr)
{
	uint32_t eth_hdr_len = HAL_RX_TLV_GET_IP_OFFSET(rx_tlv_hdr);
	uint32_t ip_hdr_len = HAL_RX_TLV_GET_TCP_OFFSET(rx_tlv_hdr);

	/* ETHERNET_HDR_LEN + ip_hdr_len + UDP/TCP; */
	return (eth_hdr_len + ip_hdr_len + sizeof(struct udphdr));
}

/**
 * get_transport_header_offset() - Get transport header offset
 * @fisa_flow: Handle to FISA sw flow entry
 * @rx_tlv_hdr: TLV hdr pointer
 *
 * Return: Offset value to transport header
 */
static int get_transport_header_offset(struct dp_fisa_rx_sw_ft *fisa_flow,
				       uint8_t *rx_tlv_hdr)

{
	uint32_t eth_hdr_len = HAL_RX_TLV_GET_IP_OFFSET(rx_tlv_hdr);
	uint32_t ip_hdr_len = HAL_RX_TLV_GET_TCP_OFFSET(rx_tlv_hdr);

	/* ETHERNET_HDR_LEN + ip_hdr_len */
	return (eth_hdr_len + ip_hdr_len);
}

/**
 * dp_rx_fisa_aggr_udp() - Aggregate incoming to UDP nbuf
 * @fisa_flow: Handle to SW flow entry, which holds the aggregated nbuf
 * @nbuf: Incoming nbuf
 *
 * Return: FISA_AGGR_DONE on successful aggregation
 */
static enum fisa_aggr_ret
dp_rx_fisa_aggr_udp(struct dp_rx_fst *fisa_hdl,
		    struct dp_fisa_rx_sw_ft *fisa_flow,	qdf_nbuf_t nbuf)
{
	qdf_nbuf_t head_skb = fisa_flow->head_skb;
	uint8_t *rx_tlv_hdr = qdf_nbuf_data(nbuf);
	uint32_t l2_hdr_offset =
		hal_rx_msdu_end_l3_hdr_padding_get(fisa_hdl->soc_hdl->hal_soc,
						   rx_tlv_hdr);
	struct udphdr *udp_hdr;
	uint32_t udp_len;
	uint32_t transport_payload_offset;

	qdf_nbuf_pull_head(nbuf, RX_PKT_TLVS_LEN + l2_hdr_offset);

	udp_hdr = (struct udphdr *)(qdf_nbuf_data(nbuf) +
			get_transport_header_offset(fisa_flow, rx_tlv_hdr));

	udp_len = qdf_ntohs(udp_hdr->len);

	/**
	 * Incoming nbuf is of size greater than ongoing aggregation
	 * then flush the aggregate and start new aggregation for nbuf
	 */
	if (head_skb &&
	    (udp_len > qdf_ntohs(fisa_flow->head_skb_udp_hdr->len))) {
		/* current msdu should not take into account for flushing */
		fisa_flow->adjusted_cumulative_ip_length -=
			(udp_len - sizeof(struct udphdr));
		fisa_flow->cur_aggr--;
		dp_rx_fisa_flush_flow_wrap(fisa_flow);
		/* napi_flush_cumulative_ip_length  not include current msdu */
		fisa_flow->napi_flush_cumulative_ip_length -= udp_len;
		head_skb = NULL;
	}

	if (!head_skb) {
		dp_fisa_debug("first head skb nbuf %pK", nbuf);
		/* First nbuf for the flow */
		fisa_flow->head_skb = nbuf;
		fisa_flow->head_skb_udp_hdr = udp_hdr;
		fisa_flow->cur_aggr_gso_size = udp_len - sizeof(struct udphdr);
		fisa_flow->adjusted_cumulative_ip_length = udp_len;
		fisa_flow->head_skb_ip_hdr_offset =
					HAL_RX_TLV_GET_IP_OFFSET(rx_tlv_hdr);
		fisa_flow->head_skb_l4_hdr_offset =
					HAL_RX_TLV_GET_TCP_OFFSET(rx_tlv_hdr);

		fisa_flow->frags_cumulative_len = 0;

		return FISA_AGGR_DONE;
	}

	transport_payload_offset =
		get_transport_payload_offset(fisa_hdl, rx_tlv_hdr);

	hex_dump_skb_data(nbuf, false);
	qdf_nbuf_pull_head(nbuf, transport_payload_offset);
	hex_dump_skb_data(nbuf, false);

	fisa_flow->bytes_aggregated += qdf_nbuf_len(nbuf);

	fisa_flow->frags_cumulative_len += (udp_len - sizeof(struct udphdr));

	if (qdf_nbuf_get_ext_list(head_skb)) {
		/*
		 * This is 3rd skb for flow.
		 * After head skb, 2nd skb in fraglist
		 */
		if (qdf_likely(fisa_flow->last_skb)) {
			qdf_nbuf_set_next(fisa_flow->last_skb, nbuf);
		} else {
			qdf_nbuf_free(nbuf);
			return FISA_AGGR_DONE;
		}
	} else {
		/* 1st skb after head skb
		 * implement qdf wrapper set_ext_list
		 */
		skb_shinfo(head_skb)->frag_list = nbuf;
		qdf_nbuf_set_is_frag(nbuf, 1);
	}

	fisa_flow->last_skb = nbuf;
	fisa_flow->aggr_count++;

	dp_fisa_debug("Stiched head skb fisa_flow %pK", fisa_flow);
	hex_dump_skb_data(fisa_flow->head_skb, false);

	/**
	 * Incoming nbuf is of size less than ongoing aggregation
	 * then flush the aggregate
	 */
	if (udp_len < qdf_ntohs(fisa_flow->head_skb_udp_hdr->len))
		dp_rx_fisa_flush_flow_wrap(fisa_flow);

	return FISA_AGGR_DONE;
}
/**
 * dp_fisa_rx_linear_skb() - Linearize fraglist skb to linear skb
 * @vdev: handle to DP vdev
 * @head_skb: non linear skb
 * @size: Total length of non linear stiched skb
 *
 * Return: Linearized skb pointer
 */
static qdf_nbuf_t dp_fisa_rx_linear_skb(struct dp_vdev *vdev,
					qdf_nbuf_t head_skb, uint32_t size)
{
#if 0
	qdf_nbuf_t linear_skb, ext_skb, next_skb;

	linear_skb = qdf_nbuf_alloc(vdev->osdev, size, 0, 0, FALSE);
	if (!linear_skb)
		return NULL;

	dp_fisa_debug("head %pK data %pK tail %pK\n", head_skb->head,
		      head_skb->data, head_skb->tail);
	ext_skb = skb_shinfo(head_skb)->frag_list;
	if (ext_skb) {
		skb_put_data(linear_skb, head_skb->data, 1512);
		dp_fisa_debug("%d: len %d\n", __LINE__, ext_skb->len);
		skb_put_data(linear_skb, ext_skb->data, ext_skb->len);
	} else {
		dp_fisa_debug("%d: len %d\n", __LINE__, head_skb->len);
		skb_put_data(linear_skb, head_skb->data, head_skb->len);
		goto done;
	}

	next_skb = ext_skb->next;
	while (next_skb) {
		dp_fisa_debug("%d: len %d\n", __LINE__, next_skb->len);
		skb_put_data(linear_skb, next_skb->data, next_skb->len);
		next_skb = next_skb->next;
	}

done:
	skb_copy_header(linear_skb, head_skb);
	skb_reset_transport_header(linear_skb);
	skb_reset_network_header(linear_skb);
	linear_skb->pkt_type = PACKET_HOST;
	skb_set_mac_header(linear_skb, 0);
	linear_skb->ip_summed = CHECKSUM_PARTIAL;
	dp_fisa_debug("linear skb %pK len %d gso_size %d mac_len %d net_header %d mac_header %d",
		      linear_skb, linear_skb->len,
		      skb_shinfo(linear_skb)->gso_size,	linear_skb->mac_len,
		      linear_skb->network_header, linear_skb->mac_header);

	return linear_skb;
#endif
	return NULL;
}

/**
 * dp_rx_fisa_flush_udp_flow() - Flush all aggregated nbuf of the udp flow
 * @vdev: handle to dp_vdev
 * @fisa_flow: Flow for which aggregates to be flushed
 *
 * Return: None
 */
static void
dp_rx_fisa_flush_udp_flow(struct dp_vdev *vdev,
			  struct dp_fisa_rx_sw_ft *fisa_flow)
{
	qdf_nbuf_t head_skb = fisa_flow->head_skb;
	struct iphdr *head_skb_iph;
	struct udphdr *head_skb_udp_hdr;
	struct skb_shared_info *shinfo;
	qdf_nbuf_t linear_skb;
	struct dp_vdev *fisa_flow_vdev;

	dp_fisa_debug("head_skb %pK", head_skb);
	dp_fisa_debug("cumulative ip length %d",
		      fisa_flow->adjusted_cumulative_ip_length);
	if (!head_skb) {
		dp_fisa_debug("Already flushed");
		return;
	}

	head_skb->hash = QDF_NBUF_CB_RX_FLOW_ID(head_skb);
	head_skb->sw_hash = 1;
	if (qdf_nbuf_get_ext_list(head_skb)) {
		__sum16 pseudo;

		shinfo = skb_shinfo(head_skb);
		/* Update the head_skb before flush */
		dp_fisa_debug("cumu ip length host order 0x%x",
			      fisa_flow->adjusted_cumulative_ip_length);
		head_skb_iph = (struct iphdr *)(qdf_nbuf_data(head_skb) +
					fisa_flow->head_skb_ip_hdr_offset);
		dp_fisa_debug("iph ptr %pK", head_skb_iph);

		head_skb_udp_hdr = fisa_flow->head_skb_udp_hdr;

		dp_fisa_debug("udph ptr %pK", head_skb_udp_hdr);

		dp_fisa_debug("tot_len 0x%x", qdf_ntohs(head_skb_iph->tot_len));

		/* data_len is total length of non head_skb,
		 * cumulative ip length is including head_skb ip length also
		 */
		head_skb->data_len =
			(fisa_flow->adjusted_cumulative_ip_length) -
			qdf_ntohs(head_skb_udp_hdr->len);

		head_skb->len += head_skb->data_len;

		head_skb_iph->tot_len =
			qdf_htons((fisa_flow->adjusted_cumulative_ip_length)
			+ /* IP hdr len */
			fisa_flow->head_skb_l4_hdr_offset);
		pseudo = ~csum_tcpudp_magic(head_skb_iph->saddr,
				head_skb_iph->daddr,
				fisa_flow->adjusted_cumulative_ip_length,
				head_skb_iph->protocol, 0);

		head_skb_iph->check = 0;
		head_skb_iph->check = ip_fast_csum((u8 *)head_skb_iph,
						   head_skb_iph->ihl);

		head_skb_udp_hdr->len =
			qdf_htons(qdf_ntohs(head_skb_iph->tot_len) -
				  fisa_flow->head_skb_l4_hdr_offset);
		head_skb_udp_hdr->check = pseudo;
		head_skb->csum_start = (u8 *)head_skb_udp_hdr - head_skb->head;
		head_skb->csum_offset = offsetof(struct udphdr, check);

		shinfo->gso_size = fisa_flow->cur_aggr_gso_size;
		dp_fisa_debug("gso_size %d, udp_len %d\n", shinfo->gso_size,
			      qdf_ntohs(head_skb_udp_hdr->len));
		shinfo->gso_segs = fisa_flow->cur_aggr;
		shinfo->gso_type = SKB_GSO_UDP_L4;
		head_skb->ip_summed = CHECKSUM_PARTIAL;
	}

	qdf_nbuf_set_next(fisa_flow->head_skb, NULL);
	QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(fisa_flow->head_skb) = 1;
	if (fisa_flow->last_skb)
		qdf_nbuf_set_next(fisa_flow->last_skb, NULL);

	hex_dump_skb_data(fisa_flow->head_skb, false);

	fisa_flow_vdev = dp_vdev_get_ref_by_id(
				fisa_flow->soc_hdl,
				QDF_NBUF_CB_RX_VDEV_ID(fisa_flow->head_skb),
				DP_MOD_ID_RX);
	if (qdf_unlikely(!fisa_flow_vdev ||
					(fisa_flow_vdev != fisa_flow->vdev))) {
		qdf_nbuf_free(fisa_flow->head_skb);
		goto out;
	}
	dp_fisa_debug("fisa_flow->curr_aggr %d", fisa_flow->cur_aggr);
	linear_skb = dp_fisa_rx_linear_skb(vdev, fisa_flow->head_skb, 24000);
	if (linear_skb) {
		if (!vdev->osif_rx || QDF_STATUS_SUCCESS !=
		    vdev->osif_rx(vdev->osif_vdev, linear_skb))
			qdf_nbuf_free(linear_skb);
		/* Free non linear skb */
		qdf_nbuf_free(fisa_flow->head_skb);
	} else {
		/*
		 * Sanity check head data_len should be equal to sum of
		 * all fragments length
		 */
		if (qdf_unlikely(fisa_flow->frags_cumulative_len !=
				 fisa_flow->head_skb->data_len)) {
			qdf_assert(0);
			/* Drop the aggregate */
			qdf_nbuf_free(fisa_flow->head_skb);
			goto out;
		}

		if (!vdev->osif_rx || QDF_STATUS_SUCCESS !=
		    vdev->osif_rx(vdev->osif_vdev, fisa_flow->head_skb))
			qdf_nbuf_free(fisa_flow->head_skb);
	}

out:
	if (fisa_flow_vdev)
		dp_vdev_unref_delete(fisa_flow->soc_hdl,
				     fisa_flow_vdev,
				     DP_MOD_ID_RX);
	fisa_flow->head_skb = NULL;
	fisa_flow->last_skb = NULL;

	fisa_flow->flush_count++;
}

/**
 * dp_rx_fisa_flush_tcp_flow() - Flush all aggregated nbuf of the TCP flow
 * @vdev: handle to dp_vdev
 * @fisa_flow: Flow for which aggregates to be flushed
 *
 * Return: None
 */
static void
dp_rx_fisa_flush_tcp_flow(struct dp_vdev *vdev,
			  struct dp_fisa_rx_sw_ft *fisa_flow)
{
	qdf_nbuf_t head_skb = fisa_flow->head_skb;
	struct iphdr *head_skb_iph;
	struct skb_shared_info *shinfo;

	if (!head_skb) {
		dp_fisa_debug("Already flushed");
		return;
	}

	shinfo = skb_shinfo(head_skb);

	/* Update the head_skb before flush */
	head_skb->hash = fisa_flow->flow_hash;
	head_skb->sw_hash = 1;
	shinfo->gso_type = SKB_GSO_UDP_L4;

	head_skb_iph = (struct iphdr *)(qdf_nbuf_data(head_skb) +
					fisa_flow->head_skb_ip_hdr_offset);

	head_skb_iph->tot_len = fisa_flow->adjusted_cumulative_ip_length;
	head_skb_iph->check = ip_fast_csum((u8 *)head_skb_iph,
					   head_skb_iph->ihl);

	qdf_nbuf_set_next(fisa_flow->head_skb, NULL);
	if (fisa_flow->last_skb)
		qdf_nbuf_set_next(fisa_flow->last_skb, NULL);
	vdev->osif_rx(vdev->osif_vdev, fisa_flow->head_skb);

	fisa_flow->head_skb = NULL;

	fisa_flow->flush_count++;
}

/**
 * dp_rx_fisa_flush_flow() - Flush all aggregated nbuf of the flow
 * @vdev: handle to dp_vdev
 * @fisa_flow: Flow for which aggregates to be flushed
 *
 * Return: None
 */
static void dp_rx_fisa_flush_flow(struct dp_vdev *vdev,
				  struct dp_fisa_rx_sw_ft *flow)
{
	dp_fisa_debug("dp_rx_fisa_flush_flow");

	if (flow->is_flow_udp)
		dp_rx_fisa_flush_udp_flow(vdev, flow);
	else
		dp_rx_fisa_flush_tcp_flow(vdev, flow);
}

/**
 * dp_fisa_aggregation_should_stop - check if fisa aggregate should stop
 * @fisa_flow: Handle SW flow entry
 * @hal_aggr_count: current aggregate count from RX PKT TLV
 * @hal_cumulative_ip_len: current cumulative ip length from RX PKT TLV
 * @rx_tlv_hdr: current msdu RX PKT TLV
 *
 * Return: true - current flow aggregation should stop,
	   false - continue to aggregate.
 */
static bool dp_fisa_aggregation_should_stop(
				struct dp_fisa_rx_sw_ft *fisa_flow,
				uint32_t hal_aggr_count,
				uint16_t hal_cumulative_ip_len,
				uint8_t *rx_tlv_hdr)
{
	uint32_t msdu_len = hal_rx_msdu_start_msdu_len_get(rx_tlv_hdr);
	uint32_t l4_hdr_offset = HAL_RX_TLV_GET_IP_OFFSET(rx_tlv_hdr) +
				 HAL_RX_TLV_GET_TCP_OFFSET(rx_tlv_hdr);
	uint32_t cumulative_ip_len_delta = hal_cumulative_ip_len -
					   fisa_flow->hal_cumultive_ip_len;
	/**
	 * current cumulative ip length should > last cumulative_ip_len
	 * and <= last cumulative_ip_len + 1478, also current aggregate
	 * count should be equal to last aggregate count + 1,
	 * cumulative_ip_len delta should be equal to current msdu length
	 * - l4 header offset,
	 * otherwise, current fisa flow aggregation should be stopped.
	 */
	if (fisa_flow->do_not_aggregate ||
	    hal_cumulative_ip_len <= fisa_flow->hal_cumultive_ip_len ||
	    cumulative_ip_len_delta > FISA_MAX_SINGLE_CUMULATIVE_IP_LEN ||
	    (fisa_flow->last_hal_aggr_count + 1) != hal_aggr_count ||
	    cumulative_ip_len_delta != (msdu_len - l4_hdr_offset))
		return true;

	return false;
}

/**
 * dp_add_nbuf_to_fisa_flow() - Aggregate incoming nbuf
 * @fisa_hdl: handle to fisa context
 * @vdev: handle DP vdev
 * @nbuf: Incoming nbuf
 * @fisa_flow: Handle SW flow entry
 *
 * Return: Success on aggregation
 */
static int dp_add_nbuf_to_fisa_flow(struct dp_rx_fst *fisa_hdl,
				    struct dp_vdev *vdev, qdf_nbuf_t nbuf,
				    struct dp_fisa_rx_sw_ft *fisa_flow)
{
	bool flow_aggr_cont;
	uint8_t *rx_tlv_hdr = qdf_nbuf_data(nbuf);
	bool flow_invalid, flow_timeout;
	uint32_t flow_idx;
	uint16_t hal_cumulative_ip_len;
	hal_soc_handle_t hal_soc_hdl = fisa_hdl->soc_hdl->hal_soc;
	uint32_t hal_aggr_count;
	uint8_t napi_id = QDF_NBUF_CB_RX_CTX_ID(nbuf);
	uint8_t reo_id = fisa_flow->napi_id;
	uint32_t fse_metadata;

	dump_tlvs(hal_soc_hdl, rx_tlv_hdr, QDF_TRACE_LEVEL_INFO_HIGH);
	dp_fisa_debug("nbuf: %pK nbuf->next:%pK nbuf->data:%pK len %d data_len %d",
		      nbuf, qdf_nbuf_next(nbuf), qdf_nbuf_data(nbuf), nbuf->len,
		      nbuf->data_len);

	/* Packets of the flow are arriving on a different REO than
	 * the one configured.
	 */
	if (qdf_unlikely(fisa_flow->napi_id != napi_id)) {
		fse_metadata =
			hal_rx_msdu_fse_metadata_get(hal_soc_hdl, rx_tlv_hdr);
		if (fisa_hdl->del_flow_count &&
		    fse_metadata != fisa_flow->metadata)
			return FISA_AGGR_NOT_ELIGIBLE;

		dp_err("REO id mismatch flow: %pK napi_id: %u nbuf: %pK reo_id: %u",
		       fisa_flow, fisa_flow->napi_id, nbuf, napi_id);
		DP_STATS_INC(fisa_hdl, reo_mismatch, 1);
		QDF_BUG(0);
		return FISA_AGGR_NOT_ELIGIBLE;
	}

	hal_cumulative_ip_len = hal_rx_get_fisa_cumulative_ip_length(
								hal_soc_hdl,
								rx_tlv_hdr);
	flow_aggr_cont = hal_rx_get_fisa_flow_agg_continuation(hal_soc_hdl,
							       rx_tlv_hdr);
	hal_aggr_count = hal_rx_get_fisa_flow_agg_count(hal_soc_hdl,
							rx_tlv_hdr);

	dp_rx_fisa_acquire_ft_lock(fisa_hdl, reo_id);

	if (!flow_aggr_cont) {
		/* Start of new aggregation for the flow
		 * Flush previous aggregates for this flow
		 */
		dp_fisa_debug("no fgc nbuf %pK, flush %pK napi %d", nbuf,
			      fisa_flow, QDF_NBUF_CB_RX_CTX_ID(nbuf));
		dp_rx_fisa_flush_flow(vdev, fisa_flow);
		/* Clear of previoud context values */
		fisa_flow->napi_flush_cumulative_l4_checksum = 0;
		fisa_flow->napi_flush_cumulative_ip_length = 0;
		fisa_flow->cur_aggr = 0;
		fisa_flow->do_not_aggregate = false;
		fisa_flow->hal_cumultive_ip_len = 0;
		fisa_flow->last_hal_aggr_count = 0;
		/* Check fisa related HW TLV correct or not */
		if (qdf_unlikely(dp_fisa_aggregation_should_stop(
						fisa_flow,
						hal_aggr_count,
						hal_cumulative_ip_len,
						rx_tlv_hdr))) {
			qdf_assert(0);
			fisa_flow->do_not_aggregate = true;
			/*
			 * do not aggregate until next new aggregation
			 * start.
			 */
			goto invalid_fisa_assist;
		}
	} else if (qdf_unlikely(dp_fisa_aggregation_should_stop(
						fisa_flow,
						hal_aggr_count,
						hal_cumulative_ip_len,
						rx_tlv_hdr))) {
		qdf_assert(0);
		/* Either HW cumulative ip length is wrong, or packet is missed
		 * Flush the flow and do not aggregate until next start new
		 * aggreagtion
		 */
		dp_rx_fisa_flush_flow(vdev, fisa_flow);
		fisa_flow->do_not_aggregate = true;
		fisa_flow->cur_aggr = 0;
		fisa_flow->napi_flush_cumulative_ip_length = 0;
		goto invalid_fisa_assist;
	} else {
		/* takecare to skip the udp hdr len for sub sequent cumulative
		 * length
		 */
		fisa_flow->cur_aggr++;
	}
	hal_rx_msdu_get_flow_params(hal_soc_hdl, rx_tlv_hdr, &flow_invalid,
				    &flow_timeout, &flow_idx);
	dp_fisa_debug("nbuf %pK cumulat_ip_length %d flow %pK fl aggr cont %d",
		      nbuf, hal_cumulative_ip_len, fisa_flow, flow_aggr_cont);

	fisa_flow->aggr_count++;
	fisa_flow->last_hal_aggr_count = hal_aggr_count;
	fisa_flow->hal_cumultive_ip_len = hal_cumulative_ip_len;

	if (!fisa_flow->head_skb) {
		/* This is start of aggregation for the flow, save the offsets*/
		fisa_flow->napi_flush_cumulative_l4_checksum = 0;
		fisa_flow->cur_aggr = 0;
	}

	fisa_flow->adjusted_cumulative_ip_length =
		/* cumulative ip len has all the aggr msdu udp header len
		 * Aggr UDP msdu has one UDP header len
		 */
		(hal_cumulative_ip_len -
		(fisa_flow->cur_aggr * sizeof(struct udphdr))) -
		fisa_flow->napi_flush_cumulative_ip_length;

	/**
	 * cur_aggr does not include the head_skb, so compare with
	 * FISA_FLOW_MAX_AGGR_COUNT - 1.
	 */
	if (fisa_flow->cur_aggr > (FISA_FLOW_MAX_AGGR_COUNT - 1))
		dp_err("HAL cumulative_ip_length %d", hal_cumulative_ip_len);

	dp_fisa_debug("hal cum_len 0x%x - napI_cumu_len 0x%x = flow_cum_len 0x%x cur_aggr %d",
		      hal_cumulative_ip_len,
		      fisa_flow->napi_flush_cumulative_ip_length,
		      fisa_flow->adjusted_cumulative_ip_length,
		      fisa_flow->cur_aggr);

	if (fisa_flow->adjusted_cumulative_ip_length >
	    FISA_FLOW_MAX_CUMULATIVE_IP_LEN) {
		dp_err("fisa_flow %pK nbuf %pK", fisa_flow, nbuf);
		dp_err("fisa_flow->adjusted_cumulative_ip_length %d",
		       fisa_flow->adjusted_cumulative_ip_length);
		dp_err("HAL cumulative_ip_length %d", hal_cumulative_ip_len);
		dp_err("napi_flush_cumulative_ip_length %d",
		       fisa_flow->napi_flush_cumulative_ip_length);
		qdf_assert(0);
	}

	dp_fisa_record_pkt(fisa_flow, nbuf, rx_tlv_hdr);

	if (fisa_flow->is_flow_udp) {
		dp_rx_fisa_aggr_udp(fisa_hdl, fisa_flow, nbuf);
	} else if (fisa_flow->is_flow_tcp) {
		qdf_assert(0);
		dp_rx_fisa_aggr_tcp(fisa_hdl, fisa_flow, nbuf);
	}

	dp_rx_fisa_release_ft_lock(fisa_hdl, reo_id);
	fisa_flow->last_accessed_ts = qdf_get_log_timestamp();

	return FISA_AGGR_DONE;

invalid_fisa_assist:
	/* Not eligible aggregation deliver frame without FISA */
	dp_rx_fisa_release_ft_lock(fisa_hdl, reo_id);
	return FISA_AGGR_NOT_ELIGIBLE;
}

/**
 * dp_is_nbuf_bypass_fisa() - FISA bypass check for RX frame
 * @nbuf: RX nbuf pointer
 *
 * Return: true if FISA should be bypassed else false
 */
static bool dp_is_nbuf_bypass_fisa(qdf_nbuf_t nbuf)
{
	/* RX frame from non-regular path or DHCP packet */
	if (QDF_NBUF_CB_RX_TCP_PROTO(nbuf) ||
	    qdf_nbuf_is_exc_frame(nbuf) ||
	    qdf_nbuf_is_ipv4_dhcp_pkt(nbuf) ||
	    qdf_nbuf_is_da_mcbc(nbuf))
		return true;

	return false;
}

/**
 * dp_rx_fisa_flush_by_vdev_ctx_id() - Flush fisa aggregates per vdev and rx
 *  context id
 * @soc: core txrx main context
 * @vdev: Handle DP vdev
 * @rx_ctx_id: Rx context id
 *
 * Return: Success on flushing the flows for the vdev and rx ctx id
 */
static
QDF_STATUS dp_rx_fisa_flush_by_vdev_ctx_id(struct dp_soc *soc,
					   struct dp_vdev *vdev,
					   uint8_t rx_ctx_id)
{
	struct dp_rx_fst *fisa_hdl = soc->rx_fst;
	struct dp_fisa_rx_sw_ft *sw_ft_entry =
		(struct dp_fisa_rx_sw_ft *)fisa_hdl->base;
	int ft_size = fisa_hdl->max_entries;
	int i;

	dp_rx_fisa_acquire_ft_lock(fisa_hdl, rx_ctx_id);
	for (i = 0; i < ft_size; i++) {
		if (sw_ft_entry[i].is_populated &&
		    vdev == sw_ft_entry[i].vdev &&
		    sw_ft_entry[i].napi_id == rx_ctx_id) {
			dp_fisa_debug("flushing %d %pk vdev %pK napi id:%d", i,
				      &sw_ft_entry[i], vdev, rx_ctx_id);
			dp_rx_fisa_flush_flow_wrap(&sw_ft_entry[i]);
		}
	}
	dp_rx_fisa_release_ft_lock(fisa_hdl, rx_ctx_id);

	return QDF_STATUS_SUCCESS;
}

/**
 * dp_fisa_disallowed_for_vdev() - Check if fisa is allowed on vdev
 * @soc: core txrx main context
 * @vdev: Handle DP vdev
 * @rx_ctx_id: Rx context id
 *
 * Return: true if fisa is disallowed for vdev else false
 */
static bool dp_fisa_disallowed_for_vdev(struct dp_soc *soc,
					struct dp_vdev *vdev,
					uint8_t rx_ctx_id)
{
	if (!vdev->fisa_disallowed[rx_ctx_id]) {
		if (vdev->fisa_force_flushed[rx_ctx_id])
			vdev->fisa_force_flushed[rx_ctx_id] = 0;
		return false;
	}

	if (!vdev->fisa_force_flushed[rx_ctx_id]) {
		dp_rx_fisa_flush_by_vdev_ctx_id(soc, vdev, rx_ctx_id);
		vdev->fisa_force_flushed[rx_ctx_id] = 1;
	}

	return true;
}

/**
 * dp_fisa_rx() - Entry function to FISA to handle aggregation
 * @soc: core txrx main context
 * @vdev: Handle DP vdev
 * @nbuf_list: List nbufs to be aggregated
 *
 * Return: Success on aggregation
 */
QDF_STATUS dp_fisa_rx(struct dp_soc *soc, struct dp_vdev *vdev,
		      qdf_nbuf_t nbuf_list)
{
	struct dp_rx_fst *dp_fisa_rx_hdl = soc->rx_fst;
	qdf_nbuf_t head_nbuf;
	qdf_nbuf_t next_nbuf;
	struct dp_fisa_rx_sw_ft *fisa_flow;
	int fisa_ret;
	uint8_t rx_ctx_id = QDF_NBUF_CB_RX_CTX_ID(nbuf_list);

	head_nbuf = nbuf_list;

	while (head_nbuf) {
		next_nbuf = head_nbuf->next;
		qdf_nbuf_set_next(head_nbuf, NULL);

		/* bypass FISA check */
		if (dp_is_nbuf_bypass_fisa(head_nbuf))
			goto deliver_nbuf;

		if (dp_fisa_disallowed_for_vdev(soc, vdev, rx_ctx_id))
			goto deliver_nbuf;

		if (qdf_atomic_read(&soc->skip_fisa_param.skip_fisa)) {
			if (!soc->skip_fisa_param.fisa_force_flush[rx_ctx_id]) {
				dp_rx_fisa_flush_by_ctx_id(soc, rx_ctx_id);
				soc->skip_fisa_param.
						fisa_force_flush[rx_ctx_id] = 1;
			}
			goto deliver_nbuf;
		} else if (soc->skip_fisa_param.fisa_force_flush[rx_ctx_id]) {
			soc->skip_fisa_param.fisa_force_flush[rx_ctx_id] = 0;
		}

		qdf_nbuf_push_head(head_nbuf, RX_PKT_TLVS_LEN +
				   QDF_NBUF_CB_RX_PACKET_L3_HDR_PAD(head_nbuf));

		/* Add new flow if the there is no ongoing flow */
		fisa_flow = dp_rx_get_fisa_flow(dp_fisa_rx_hdl, vdev,
						head_nbuf);

		/* Do not FISA aggregate IPSec packets */
		if (fisa_flow &&
		    fisa_flow->rx_flow_tuple_info.is_exception)
			goto pull_nbuf;

		/* Fragmented skb do not handle via fisa
		 * get that flow and deliver that flow to rx_thread
		 */
		if (qdf_unlikely(qdf_nbuf_get_ext_list(head_nbuf))) {
			dp_fisa_debug("Fragmented skb, will not be FISAed");
			if (fisa_flow) {
				dp_rx_fisa_acquire_ft_lock(dp_fisa_rx_hdl,
							   fisa_flow->napi_id);
				dp_rx_fisa_flush_flow(vdev, fisa_flow);
				dp_rx_fisa_release_ft_lock(dp_fisa_rx_hdl,
							   fisa_flow->napi_id);
			}
			goto pull_nbuf;
		}

		if (!fisa_flow)
			goto pull_nbuf;

		fisa_ret = dp_add_nbuf_to_fisa_flow(dp_fisa_rx_hdl, vdev,
						    head_nbuf, fisa_flow);
		if (fisa_ret == FISA_AGGR_DONE)
			goto next_msdu;

pull_nbuf:
		nbuf_skip_rx_pkt_tlv(dp_fisa_rx_hdl->soc_hdl->hal_soc,
				     head_nbuf);

deliver_nbuf: /* Deliver without FISA */
		QDF_NBUF_CB_RX_NUM_ELEMENTS_IN_LIST(head_nbuf) = 1;
		qdf_nbuf_set_next(head_nbuf, NULL);
		hex_dump_skb_data(head_nbuf, false);
		if (!vdev->osif_rx || QDF_STATUS_SUCCESS !=
		    vdev->osif_rx(vdev->osif_vdev, head_nbuf))
			qdf_nbuf_free(head_nbuf);
next_msdu:
		head_nbuf = next_nbuf;
	}

	return QDF_STATUS_SUCCESS;
}

/* Length of string to store tuple information for printing */
#define DP_TUPLE_STR_LEN 512

QDF_STATUS dp_rx_dump_fisa_stats(struct dp_soc *soc)
{
	int i;
	char tuple_str[DP_TUPLE_STR_LEN] = {'\0'};
	struct dp_rx_fst *rx_fst = soc->rx_fst;
	struct dp_fisa_rx_sw_ft *sw_ft_entry =
		&((struct dp_fisa_rx_sw_ft *)rx_fst->base)[0];
	int ft_size = rx_fst->max_entries;

	dp_info("#flows added %d evicted %d hash collision %d",
		rx_fst->add_flow_count,
		rx_fst->del_flow_count,
		rx_fst->hash_collision_cnt);

	for (i = 0; i < ft_size; i++, sw_ft_entry++) {
		if (!sw_ft_entry->is_populated)
			continue;

		print_flow_tuple(&sw_ft_entry->rx_flow_tuple_info,
				 tuple_str,
				 sizeof(tuple_str));

		dp_info("Flow[%d][%s][%s] ring %d msdu-aggr %d flushes %d bytes-agg %llu avg-bytes-aggr %llu",
			sw_ft_entry->flow_id,
			sw_ft_entry->is_flow_udp ? "udp" : "tcp",
			tuple_str,
			sw_ft_entry->napi_id,
			sw_ft_entry->aggr_count,
			sw_ft_entry->flush_count,
			sw_ft_entry->bytes_aggregated,
			qdf_do_div(sw_ft_entry->bytes_aggregated,
				   sw_ft_entry->flush_count));
	}
	return QDF_STATUS_SUCCESS;
}

/**
 * dp_rx_fisa_flush_flow_wrap() - flush fisa flow by invoking
 *				  dp_rx_fisa_flush_flow()
 * @sw_ft: fisa flow for which aggregates to be flushed
 *
 * Return: None.
 */
static void dp_rx_fisa_flush_flow_wrap(struct dp_fisa_rx_sw_ft *sw_ft)
{
	/* Save the ip_len and checksum as hardware assist is
	 * always based on his start of aggregation
	 */
	sw_ft->napi_flush_cumulative_l4_checksum =
				sw_ft->cumulative_l4_checksum;
	sw_ft->napi_flush_cumulative_ip_length =
				sw_ft->hal_cumultive_ip_len;
	dp_fisa_debug("napi_flush_cumulative_ip_length 0x%x",
		      sw_ft->napi_flush_cumulative_ip_length);

	dp_rx_fisa_flush_flow(sw_ft->vdev,
			      sw_ft);
	sw_ft->cur_aggr = 0;
}

QDF_STATUS dp_rx_fisa_flush_by_ctx_id(struct dp_soc *soc, int napi_id)
{
	struct dp_rx_fst *fisa_hdl = soc->rx_fst;
	struct dp_fisa_rx_sw_ft *sw_ft_entry =
		(struct dp_fisa_rx_sw_ft *)fisa_hdl->base;
	int ft_size = fisa_hdl->max_entries;
	int i;

	dp_rx_fisa_acquire_ft_lock(fisa_hdl, napi_id);
	for (i = 0; i < ft_size; i++) {
		if (sw_ft_entry[i].napi_id == napi_id &&
		    sw_ft_entry[i].is_populated) {
			dp_fisa_debug("flushing %d %pK napi_id %d", i,
				      &sw_ft_entry[i], napi_id);
			dp_rx_fisa_flush_flow_wrap(&sw_ft_entry[i]);
		}
	}
	dp_rx_fisa_release_ft_lock(fisa_hdl, napi_id);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS dp_rx_fisa_flush_by_vdev_id(struct dp_soc *soc, uint8_t vdev_id)
{
	struct dp_rx_fst *fisa_hdl = soc->rx_fst;
	struct dp_fisa_rx_sw_ft *sw_ft_entry =
		(struct dp_fisa_rx_sw_ft *)fisa_hdl->base;
	int ft_size = fisa_hdl->max_entries;
	int i;
	struct dp_vdev *vdev;
	uint8_t reo_id;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_RX);
	if (qdf_unlikely(!vdev)) {
		dp_err("null vdev by vdev_id %d", vdev_id);
		return QDF_STATUS_E_FAILURE;
	}

	for (i = 0; i < ft_size; i++) {
		reo_id = sw_ft_entry[i].napi_id;
		if (reo_id >= MAX_REO_DEST_RINGS)
			continue;
		dp_rx_fisa_acquire_ft_lock(fisa_hdl, reo_id);
		if (vdev == sw_ft_entry[i].vdev) {
			dp_fisa_debug("flushing %d %pk vdev %pK", i,
				      &sw_ft_entry[i], vdev);

			dp_rx_fisa_flush_flow_wrap(&sw_ft_entry[i]);
		}
		dp_rx_fisa_release_ft_lock(fisa_hdl, reo_id);
	}
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_RX);

	return QDF_STATUS_SUCCESS;
}

void dp_set_fisa_disallowed_for_vdev(struct cdp_soc_t *cdp_soc, uint8_t vdev_id,
				     uint8_t rx_ctx_id, uint8_t val)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;
	struct dp_vdev *vdev;

	vdev = dp_vdev_get_ref_by_id(soc, vdev_id, DP_MOD_ID_RX);
	if (qdf_unlikely(!vdev))
		return;

	vdev->fisa_disallowed[rx_ctx_id] = val;
	dp_vdev_unref_delete(soc, vdev, DP_MOD_ID_RX);
}

void dp_suspend_fse_cache_flush(struct dp_soc *soc)
{
	struct dp_rx_fst *dp_fst;

	dp_fst = soc->rx_fst;
	if (dp_fst) {
		if (qdf_atomic_read(&dp_fst->fse_cache_flush_posted))
			qdf_timer_sync_cancel(&dp_fst->fse_cache_flush_timer);
		dp_fst->fse_cache_flush_allow = false;
	}

	dp_info("fse cache flush suspended");
}

void dp_resume_fse_cache_flush(struct dp_soc *soc)
{
	struct dp_rx_fst *dp_fst;

	dp_fst = soc->rx_fst;
	if (dp_fst) {
		qdf_atomic_set(&dp_fst->fse_cache_flush_posted, 0);
		dp_fst->fse_cache_flush_allow = true;
	}

	dp_info("fse cache flush resumed");
}
