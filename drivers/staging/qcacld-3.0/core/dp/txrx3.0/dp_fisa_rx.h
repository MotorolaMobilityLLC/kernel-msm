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
#include <qdf_status.h>

//#define FISA_DEBUG_ENABLE

#ifdef FISA_DEBUG_ENABLE
#define dp_fisa_debug dp_info
#else
#define dp_fisa_debug(params...)
#endif

#if defined(WLAN_SUPPORT_RX_FISA)

#define FSE_CACHE_FLUSH_TIME_OUT	5 /* milliSeconds */
#define FISA_UDP_MAX_DATA_LEN		1470 /* udp max data length */
#define FISA_UDP_HDR_LEN		8 /* udp header length */
/* single packet max cumulative ip length */
#define FISA_MAX_SINGLE_CUMULATIVE_IP_LEN \
	(FISA_UDP_MAX_DATA_LEN + FISA_UDP_HDR_LEN)
/* max flow cumulative ip length */
#define FISA_FLOW_MAX_CUMULATIVE_IP_LEN \
	(FISA_MAX_SINGLE_CUMULATIVE_IP_LEN * FISA_FLOW_MAX_AGGR_COUNT)

#define IPSEC_PORT 500
#define IPSEC_NAT_PORT 4500

#define DP_FT_LOCK_MAX_RECORDS 32

struct dp_fisa_rx_fst_update_elem {
	/* Do not add new entries here */
	qdf_list_node_t node;
	struct cdp_rx_flow_tuple_info flow_tuple_info;
	struct dp_vdev *vdev;
	uint32_t flow_idx;
	uint32_t reo_dest_indication;
	bool is_tcp_flow;
	bool is_udp_flow;
	u8 reo_id;
};

enum dp_ft_lock_event_type {
	DP_FT_LOCK_EVENT,
	DP_FT_UNLOCK_EVENT,
};

struct dp_ft_lock_record {
	const char *func;
	int cpu_id;
	uint64_t timestamp;
	enum dp_ft_lock_event_type type;
};

struct dp_ft_lock_history {
	uint32_t record_idx;
	struct dp_ft_lock_record ft_lock_rec[DP_FT_LOCK_MAX_RECORDS];
};

/**
 * dp_rx_dump_fisa_stats() - Dump fisa stats
 * @soc: core txrx main context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_rx_dump_fisa_stats(struct dp_soc *soc);

/**
 * dp_fisa_rx() - FISA Rx packet delivery entry function
 * @soc: core txrx main context
 * @vdev: core txrx vdev
 * @nbuf_list: Delivery list of nbufs
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_fisa_rx(struct dp_soc *dp_fisa_rx_hdl, struct dp_vdev *vdev,
		      qdf_nbuf_t nbuf_list);

/**
 * dp_rx_fisa_flush_by_ctx_id() - FISA Rx flush function to flush
 *				  aggregation at end of NAPI
 * @soc: core txrx main context
 * @napi_id: Flows which are rxed on the NAPI ID to be flushed
 *
 * Return: QDF_STATUS
 */
QDF_STATUS dp_rx_fisa_flush_by_ctx_id(struct dp_soc *soc, int napi_id);

/**
 * dp_rx_fisa_flush_by_vdev_id() - Flush fisa aggregates per vdev id
 * @soc: core txrx main context
 * @vdev_id: vdev ID
 *
 * Return: Success on flushing the flows for the vdev
 */
QDF_STATUS dp_rx_fisa_flush_by_vdev_id(struct dp_soc *soc, uint8_t vdev_id);

/**
 * dp_rx_skip_fisa() - Set flags to skip fisa aggregation
 * @cdp_soc: core txrx main context
 * @value: allow or skip fisa
 *
 * Return: None
 */
static inline
void dp_rx_skip_fisa(struct cdp_soc_t *cdp_soc, uint32_t value)
{
	struct dp_soc *soc = (struct dp_soc *)cdp_soc;

	qdf_atomic_set(&soc->skip_fisa_param.skip_fisa, !value);
}

/**
 * dp_set_fisa_disallowed_for_vdev() - Set fisa disallowed flag for vdev
 * @cdp_soc: core txrx main context
 * @vdev_id: Vdev id
 * @rx_ctx_id: rx context id
 * @val: value to be set
 *
 * Return: None
 */
void dp_set_fisa_disallowed_for_vdev(struct cdp_soc_t *cdp_soc, uint8_t vdev_id,
				     uint8_t rx_ctx_id, uint8_t val);

/**
 * dp_fisa_rx_fst_update_work() - Work functions for FST updates
 * @arg: argument passed to the work function
 *
 * Return: None
 */
void dp_fisa_rx_fst_update_work(void *arg);

/**
 * dp_suspend_fse_cache_flush() - Suspend FSE cache flush
 * @soc: core txrx main context
 *
 * Return: None
 */
void dp_suspend_fse_cache_flush(struct dp_soc *soc);

/**
 * dp_resume_fse_cache_flush() - Resume FSE cache flush
 * @soc: core txrx main context
 *
 * Return: None
 */
void dp_resume_fse_cache_flush(struct dp_soc *soc);
#else
static QDF_STATUS dp_rx_dump_fisa_stats(struct dp_soc *soc)
{
	return QDF_STATUS_SUCCESS;
}

void dp_rx_dump_fisa_table(struct dp_soc *soc)
{
}
#endif
