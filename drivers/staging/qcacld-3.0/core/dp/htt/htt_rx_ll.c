/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
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

#include <qdf_mem.h>         /* qdf_mem_malloc,free, etc. */
#include <qdf_types.h>          /* qdf_print, bool */
#include <qdf_nbuf.h>           /* qdf_nbuf_t, etc. */
#include <qdf_timer.h>		/* qdf_timer_free */

#include <htt.h>                /* HTT_HL_RX_DESC_SIZE */
#include <ol_cfg.h>
#include <ol_rx.h>
#include <ol_htt_rx_api.h>
#include <htt_internal.h>       /* HTT_ASSERT, htt_pdev_t, HTT_RX_BUF_SIZE */
#include "regtable.h"

#include <cds_ieee80211_common.h>   /* ieee80211_frame, ieee80211_qoscntl */
#include <cds_utils.h>
#include <wlan_policy_mgr_api.h>
#include "ol_txrx_types.h"
#ifdef DEBUG_DMA_DONE
#include <asm/barrier.h>
#include <wma_api.h>
#endif
#include <pktlog_ac_fmt.h>

#ifdef DEBUG_DMA_DONE
#define MAX_DONE_BIT_CHECK_ITER 5
#endif

#ifdef HTT_DEBUG_DATA
#define HTT_PKT_DUMP(x) x
#else
#define HTT_PKT_DUMP(x) /* no-op */
#endif

/*--- setup / tear-down functions -------------------------------------------*/

#ifndef HTT_RX_HOST_LATENCY_MAX_MS
#define HTT_RX_HOST_LATENCY_MAX_MS 20 /* ms */	/* very conservative */
#endif

 /* very conservative to ensure enough buffers are allocated */
#ifndef HTT_RX_HOST_LATENCY_WORST_LIKELY_MS
#ifdef QCA_WIFI_3_0
#define HTT_RX_HOST_LATENCY_WORST_LIKELY_MS 20
#else
#define HTT_RX_HOST_LATENCY_WORST_LIKELY_MS 10
#endif
#endif

#ifndef HTT_RX_RING_REFILL_RETRY_TIME_MS
#define HTT_RX_RING_REFILL_RETRY_TIME_MS    50
#endif

#define RX_PADDR_MAGIC_PATTERN 0xDEAD0000

#ifdef ENABLE_DEBUG_ADDRESS_MARKING
static qdf_dma_addr_t
htt_rx_paddr_mark_high_bits(qdf_dma_addr_t paddr)
{
	if (sizeof(qdf_dma_addr_t) > 4) {
		/* clear high bits, leave lower 37 bits (paddr) */
		paddr &= 0x01FFFFFFFFF;
		/* mark upper 16 bits of paddr */
		paddr |= (((uint64_t)RX_PADDR_MAGIC_PATTERN) << 32);
	}
	return paddr;
}
#else
static qdf_dma_addr_t
htt_rx_paddr_mark_high_bits(qdf_dma_addr_t paddr)
{
	return paddr;
}
#endif

/**
 * htt_get_first_packet_after_wow_wakeup() - get first packet after wow wakeup
 * @msg_word: pointer to rx indication message word
 * @buf: pointer to buffer
 *
 * Return: None
 */
static void
htt_get_first_packet_after_wow_wakeup(uint32_t *msg_word, qdf_nbuf_t buf)
{
	if (HTT_RX_IN_ORD_PADDR_IND_MSDU_INFO_GET(*msg_word) &
			FW_MSDU_INFO_FIRST_WAKEUP_M) {
		qdf_nbuf_mark_wakeup_frame(buf);
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO,
			  "%s: First packet after WOW Wakeup rcvd", __func__);
	}
}

/**
 * htt_rx_ring_smmu_mapped() - check if rx ring is smmu mapped or not
 * @pdev: HTT pdev handle
 *
 * Return: true or false.
 */
static inline bool htt_rx_ring_smmu_mapped(htt_pdev_handle pdev)
{
	if (qdf_mem_smmu_s1_enabled(pdev->osdev) &&
	    pdev->is_ipa_uc_enabled &&
	    pdev->rx_ring.smmu_map)
		return true;
	else
		return false;
}

static inline qdf_nbuf_t htt_rx_netbuf_pop(htt_pdev_handle pdev)
{
	int idx;
	qdf_nbuf_t msdu;

	HTT_ASSERT1(htt_rx_ring_elems(pdev) != 0);

#ifdef DEBUG_DMA_DONE
	pdev->rx_ring.dbg_ring_idx++;
	pdev->rx_ring.dbg_ring_idx &= pdev->rx_ring.size_mask;
#endif

	idx = pdev->rx_ring.sw_rd_idx.msdu_payld;
	msdu = pdev->rx_ring.buf.netbufs_ring[idx];
	idx++;
	idx &= pdev->rx_ring.size_mask;
	pdev->rx_ring.sw_rd_idx.msdu_payld = idx;
	qdf_atomic_dec(&pdev->rx_ring.fill_cnt);
	return msdu;
}

static inline unsigned int htt_rx_ring_elems(struct htt_pdev_t *pdev)
{
	return
		(*pdev->rx_ring.alloc_idx.vaddr -
		 pdev->rx_ring.sw_rd_idx.msdu_payld) & pdev->rx_ring.size_mask;
}

/**
 * htt_rx_buff_pool_init() - initialize the pool of buffers
 * @pdev: pointer to device
 *
 * Return: 0 - success, 1 - failure
 */
static int htt_rx_buff_pool_init(struct htt_pdev_t *pdev)
{
	qdf_nbuf_t net_buf;
	int i;

	pdev->rx_buff_pool.netbufs_ring =
		qdf_mem_malloc(HTT_RX_PRE_ALLOC_POOL_SIZE * sizeof(qdf_nbuf_t));

	if (!pdev->rx_buff_pool.netbufs_ring)
		return 1; /* failure */

	qdf_atomic_init(&pdev->rx_buff_pool.fill_cnt);
	qdf_atomic_init(&pdev->rx_buff_pool.refill_low_mem);

	for (i = 0; i < HTT_RX_PRE_ALLOC_POOL_SIZE; i++) {
		net_buf = qdf_nbuf_alloc(pdev->osdev,
					 HTT_RX_BUF_SIZE,
					 0, 4, false);
		if (net_buf) {
			qdf_atomic_inc(&pdev->rx_buff_pool.fill_cnt);
			/*
			 * Mark this netbuf to differentiate it
			 * from other buf. If set 1, this buf
			 * is from pre allocated pool.
			 */
			QDF_NBUF_CB_RX_PACKET_BUFF_POOL(net_buf) = 1;
		}
		/* Allow NULL to be inserted.
		 * Taken care during alloc from this pool.
		 */
		pdev->rx_buff_pool.netbufs_ring[i] = net_buf;
	}
	QDF_TRACE(QDF_MODULE_ID_HTT,
		  QDF_TRACE_LEVEL_INFO,
		  "max pool size %d pool filled %d",
		  HTT_RX_PRE_ALLOC_POOL_SIZE,
		  qdf_atomic_read(&pdev->rx_buff_pool.fill_cnt));

	qdf_spinlock_create(&pdev->rx_buff_pool.rx_buff_pool_lock);
	return 0;
}

/**
 * htt_rx_buff_pool_deinit() - deinitialize the pool of buffers
 * @pdev: pointer to device
 *
 * Return: none
 */
static void htt_rx_buff_pool_deinit(struct htt_pdev_t *pdev)
{
	qdf_nbuf_t net_buf;
	int i;

	if (!pdev->rx_buff_pool.netbufs_ring)
		return;

	qdf_spin_lock_bh(&pdev->rx_buff_pool.rx_buff_pool_lock);
	for (i = 0; i < HTT_RX_PRE_ALLOC_POOL_SIZE; i++) {
		net_buf = pdev->rx_buff_pool.netbufs_ring[i];
		if (!net_buf)
			continue;
		qdf_nbuf_free(net_buf);
		qdf_atomic_dec(&pdev->rx_buff_pool.fill_cnt);
	}
	qdf_spin_unlock_bh(&pdev->rx_buff_pool.rx_buff_pool_lock);
	QDF_TRACE(QDF_MODULE_ID_HTT,
		  QDF_TRACE_LEVEL_INFO,
		  "max pool size %d pool filled %d",
		  HTT_RX_PRE_ALLOC_POOL_SIZE,
		  qdf_atomic_read(&pdev->rx_buff_pool.fill_cnt));

	qdf_mem_free(pdev->rx_buff_pool.netbufs_ring);
	qdf_spinlock_destroy(&pdev->rx_buff_pool.rx_buff_pool_lock);
}

/**
 * htt_rx_buff_pool_refill() - refill the pool with new buf or reuse same buf
 * @pdev: pointer to device
 * @netbuf: netbuf to reuse
 *
 * Return: true - if able to alloc new buf and insert into pool,
 * false - if need to reuse the netbuf or not able to insert into pool
 */
static bool htt_rx_buff_pool_refill(struct htt_pdev_t *pdev, qdf_nbuf_t netbuf)
{
	bool ret = false;
	qdf_nbuf_t net_buf;
	int i;

	net_buf = qdf_nbuf_alloc(pdev->osdev,
				 HTT_RX_BUF_SIZE,
				 0, 4, false);
	if (net_buf) {
		/* able to alloc new net_buf.
		 * mark this netbuf as pool buf.
		 */
		QDF_NBUF_CB_RX_PACKET_BUFF_POOL(net_buf) = 1;
		ret = true;
	} else {
		/* reuse the netbuf and
		 * reset all fields of this netbuf.
		 */
		net_buf = netbuf;
		qdf_nbuf_reset(net_buf, 0, 4);

		/* mark this netbuf as pool buf */
		QDF_NBUF_CB_RX_PACKET_BUFF_POOL(net_buf) = 1;
	}

	qdf_spin_lock_bh(&pdev->rx_buff_pool.rx_buff_pool_lock);
	for (i = 0; i < HTT_RX_PRE_ALLOC_POOL_SIZE; i++) {
		/* insert the netbuf in empty slot of pool */
		if (pdev->rx_buff_pool.netbufs_ring[i])
			continue;

		pdev->rx_buff_pool.netbufs_ring[i] = net_buf;
		qdf_atomic_inc(&pdev->rx_buff_pool.fill_cnt);
		break;
	}
	qdf_spin_unlock_bh(&pdev->rx_buff_pool.rx_buff_pool_lock);

	if (i == HTT_RX_PRE_ALLOC_POOL_SIZE) {
		/* fail to insert into pool, free net_buf */
		qdf_nbuf_free(net_buf);
		ret = false;
	}

	return ret;
}

/**
 * htt_rx_buff_alloc() - alloc the net buf from the pool
 * @pdev: pointer to device
 *
 * Return: nbuf or NULL
 */
static qdf_nbuf_t htt_rx_buff_alloc(struct htt_pdev_t *pdev)
{
	qdf_nbuf_t net_buf = NULL;
	int i;

	if (!pdev->rx_buff_pool.netbufs_ring)
		return net_buf;

	qdf_spin_lock_bh(&pdev->rx_buff_pool.rx_buff_pool_lock);
	for (i = 0; i < HTT_RX_PRE_ALLOC_POOL_SIZE; i++) {
		/* allocate the valid netbuf */
		if (!pdev->rx_buff_pool.netbufs_ring[i])
			continue;

		net_buf = pdev->rx_buff_pool.netbufs_ring[i];
		qdf_atomic_dec(&pdev->rx_buff_pool.fill_cnt);
		pdev->rx_buff_pool.netbufs_ring[i] = NULL;
		break;
	}
	qdf_spin_unlock_bh(&pdev->rx_buff_pool.rx_buff_pool_lock);
	return net_buf;
}

/**
 * htt_rx_ring_buf_attach() - retrun net buf to attach in ring
 * @pdev: pointer to device
 *
 * Return: nbuf or NULL
 */
static qdf_nbuf_t htt_rx_ring_buf_attach(struct htt_pdev_t *pdev)
{
	qdf_nbuf_t net_buf = NULL;
	bool allocated = true;

	net_buf =
		qdf_nbuf_alloc(pdev->osdev, HTT_RX_BUF_SIZE,
			       0, 4, false);
	if (!net_buf) {
		if (pdev->rx_buff_pool.netbufs_ring &&
		    qdf_atomic_read(&pdev->rx_buff_pool.refill_low_mem) &&
		    qdf_atomic_read(&pdev->rx_buff_pool.fill_cnt))
			net_buf = htt_rx_buff_alloc(pdev);

		allocated = false; /* allocated from pool */
	}

	if (allocated || !qdf_atomic_read(&pdev->rx_buff_pool.fill_cnt))
		qdf_atomic_set(&pdev->rx_buff_pool.refill_low_mem, 0);

	return net_buf;
}

/**
 * htt_rx_ring_buff_free() - free the net buff or reuse it
 * @pdev: pointer to device
 * @netbuf: netbuf
 *
 * Return: none
 */
static void htt_rx_ring_buff_free(struct htt_pdev_t *pdev, qdf_nbuf_t netbuf)
{
	bool status = false;

	if (pdev->rx_buff_pool.netbufs_ring &&
	    QDF_NBUF_CB_RX_PACKET_BUFF_POOL(netbuf)) {
		int i;

		/* rest this netbuf before putting back into pool */
		qdf_nbuf_reset(netbuf, 0, 4);

		/* mark this netbuf as pool buf */
		QDF_NBUF_CB_RX_PACKET_BUFF_POOL(netbuf) = 1;

		qdf_spin_lock_bh(&pdev->rx_buff_pool.rx_buff_pool_lock);
		for (i = 0; i < HTT_RX_PRE_ALLOC_POOL_SIZE; i++) {
			/* insert the netbuf in empty slot of pool */
			if (!pdev->rx_buff_pool.netbufs_ring[i]) {
				pdev->rx_buff_pool.netbufs_ring[i] = netbuf;
				qdf_atomic_inc(&pdev->rx_buff_pool.fill_cnt);
				status = true;    /* valid insertion */
				break;
			}
		}
		qdf_spin_unlock_bh(&pdev->rx_buff_pool.rx_buff_pool_lock);
	}
	if (!status)
		qdf_nbuf_free(netbuf);
}

/* full_reorder_offload case: this function is called with lock held */
static int htt_rx_ring_fill_n(struct htt_pdev_t *pdev, int num)
{
	int idx;
	QDF_STATUS status;
	struct htt_host_rx_desc_base *rx_desc;
	int filled = 0;
	int debt_served = 0;
	qdf_mem_info_t mem_map_table = {0};

	idx = *pdev->rx_ring.alloc_idx.vaddr;

	if ((idx < 0) || (idx > pdev->rx_ring.size_mask) ||
	    (num > pdev->rx_ring.size))  {
		QDF_TRACE(QDF_MODULE_ID_HTT,
			  QDF_TRACE_LEVEL_ERROR,
			  "%s:rx refill failed!", __func__);
		return filled;
	}

moretofill:
	while (num > 0) {
		qdf_dma_addr_t paddr, paddr_marked;
		qdf_nbuf_t rx_netbuf;
		int headroom;

		rx_netbuf = htt_rx_ring_buf_attach(pdev);
		if (!rx_netbuf) {
			qdf_timer_stop(&pdev->rx_ring.
						 refill_retry_timer);
			/*
			 * Failed to fill it to the desired level -
			 * we'll start a timer and try again next time.
			 * As long as enough buffers are left in the ring for
			 * another A-MPDU rx, no special recovery is needed.
			 */
#ifdef DEBUG_DMA_DONE
			pdev->rx_ring.dbg_refill_cnt++;
#endif
			pdev->refill_retry_timer_starts++;
			qdf_timer_start(
				&pdev->rx_ring.refill_retry_timer,
				HTT_RX_RING_REFILL_RETRY_TIME_MS);
			goto update_alloc_idx;
		}

		/* Clear rx_desc attention word before posting to Rx ring */
		rx_desc = htt_rx_desc(rx_netbuf);
		*(uint32_t *)&rx_desc->attention = 0;

#ifdef DEBUG_DMA_DONE
		*(uint32_t *)&rx_desc->msdu_end = 1;

#define MAGIC_PATTERN 0xDEADBEEF
		*(uint32_t *)&rx_desc->msdu_start = MAGIC_PATTERN;

		/*
		 * To ensure that attention bit is reset and msdu_end is set
		 * before calling dma_map
		 */
		smp_mb();
#endif
		/*
		 * Adjust qdf_nbuf_data to point to the location in the buffer
		 * where the rx descriptor will be filled in.
		 */
		headroom = qdf_nbuf_data(rx_netbuf) - (uint8_t *)rx_desc;
		qdf_nbuf_push_head(rx_netbuf, headroom);

#ifdef DEBUG_DMA_DONE
		status = qdf_nbuf_map(pdev->osdev, rx_netbuf,
				      QDF_DMA_BIDIRECTIONAL);
#else
		status = qdf_nbuf_map(pdev->osdev, rx_netbuf,
				      QDF_DMA_FROM_DEVICE);
#endif
		if (status != QDF_STATUS_SUCCESS) {
			htt_rx_ring_buff_free(pdev, rx_netbuf);
			goto update_alloc_idx;
		}

		paddr = qdf_nbuf_get_frag_paddr(rx_netbuf, 0);
		paddr_marked = htt_rx_paddr_mark_high_bits(paddr);
		if (pdev->cfg.is_full_reorder_offload) {
			if (qdf_unlikely(htt_rx_hash_list_insert(
					pdev, paddr_marked, rx_netbuf))) {
				QDF_TRACE(QDF_MODULE_ID_HTT,
					  QDF_TRACE_LEVEL_ERROR,
					  "%s: hash insert failed!", __func__);
#ifdef DEBUG_DMA_DONE
				qdf_nbuf_unmap(pdev->osdev, rx_netbuf,
					       QDF_DMA_BIDIRECTIONAL);
#else
				qdf_nbuf_unmap(pdev->osdev, rx_netbuf,
					       QDF_DMA_FROM_DEVICE);
#endif
				htt_rx_ring_buff_free(pdev, rx_netbuf);

				goto update_alloc_idx;
			}
			htt_rx_dbg_rxbuf_set(pdev, paddr_marked, rx_netbuf);
		} else {
			pdev->rx_ring.buf.netbufs_ring[idx] = rx_netbuf;
		}

		/* Caller already protected this function with refill_lock */
		if (qdf_nbuf_is_rx_ipa_smmu_map(rx_netbuf)) {
			qdf_update_mem_map_table(pdev->osdev, &mem_map_table,
						 paddr, HTT_RX_BUF_SIZE);
			qdf_assert_always(
				!cds_smmu_map_unmap(true, 1, &mem_map_table));
		}

		pdev->rx_ring.buf.paddrs_ring[idx] = paddr_marked;
		qdf_atomic_inc(&pdev->rx_ring.fill_cnt);

		num--;
		idx++;
		filled++;
		idx &= pdev->rx_ring.size_mask;
	}

	if (debt_served <  qdf_atomic_read(&pdev->rx_ring.refill_debt)) {
		num = qdf_atomic_read(&pdev->rx_ring.refill_debt) - debt_served;
		debt_served += num;
		goto moretofill;
	}

update_alloc_idx:
	/*
	 * Make sure alloc index write is reflected correctly before FW polls
	 * remote ring write index as compiler can reorder the instructions
	 * based on optimizations.
	 */
	qdf_mb();
	*pdev->rx_ring.alloc_idx.vaddr = idx;
	htt_rx_dbg_rxbuf_indupd(pdev, idx);

	return filled;
}

static int htt_rx_ring_size(struct htt_pdev_t *pdev)
{
	int size;

	/*
	 * It is expected that the host CPU will typically be able to service
	 * the rx indication from one A-MPDU before the rx indication from
	 * the subsequent A-MPDU happens, roughly 1-2 ms later.
	 * However, the rx ring should be sized very conservatively, to
	 * accommodate the worst reasonable delay before the host CPU services
	 * a rx indication interrupt.
	 * The rx ring need not be kept full of empty buffers.  In theory,
	 * the htt host SW can dynamically track the low-water mark in the
	 * rx ring, and dynamically adjust the level to which the rx ring
	 * is filled with empty buffers, to dynamically meet the desired
	 * low-water mark.
	 * In contrast, it's difficult to resize the rx ring itself, once
	 * it's in use.
	 * Thus, the ring itself should be sized very conservatively, while
	 * the degree to which the ring is filled with empty buffers should
	 * be sized moderately conservatively.
	 */
	size =
		ol_cfg_max_thruput_mbps(pdev->ctrl_pdev) *
		1000 /* 1e6 bps/mbps / 1e3 ms per sec = 1000 */  /
		(8 * HTT_RX_AVG_FRM_BYTES) * HTT_RX_HOST_LATENCY_MAX_MS;

	if (size < HTT_RX_RING_SIZE_MIN)
		size = HTT_RX_RING_SIZE_MIN;
	else if (size > HTT_RX_RING_SIZE_MAX)
		size = HTT_RX_RING_SIZE_MAX;

	size = qdf_get_pwr2(size);
	return size;
}

static int htt_rx_ring_fill_level(struct htt_pdev_t *pdev)
{
	int size;

	size = ol_cfg_max_thruput_mbps(pdev->ctrl_pdev) *
		1000 /* 1e6 bps/mbps / 1e3 ms per sec = 1000 */  /
		(8 * HTT_RX_AVG_FRM_BYTES) *
		HTT_RX_HOST_LATENCY_WORST_LIKELY_MS;

	size = qdf_get_pwr2(size);
	/*
	 * Make sure the fill level is at least 1 less than the ring size.
	 * Leaving 1 element empty allows the SW to easily distinguish
	 * between a full ring vs. an empty ring.
	 */
	if (size >= pdev->rx_ring.size)
		size = pdev->rx_ring.size - 1;

	return size;
}

static void htt_rx_ring_refill_retry(void *arg)
{
	htt_pdev_handle pdev = (htt_pdev_handle)arg;
	int filled = 0;
	int num;

	pdev->refill_retry_timer_calls++;
	qdf_spin_lock_bh(&pdev->rx_ring.refill_lock);

	num = qdf_atomic_read(&pdev->rx_ring.refill_debt);
	qdf_atomic_sub(num, &pdev->rx_ring.refill_debt);

	qdf_atomic_set(&pdev->rx_buff_pool.refill_low_mem, 1);

	filled = htt_rx_ring_fill_n(pdev, num);

	if (filled > num) {
		/* we served ourselves and some other debt */
		/* sub is safer than  = 0 */
		qdf_atomic_sub(filled - num, &pdev->rx_ring.refill_debt);
	} else if (num == filled) { /* nothing to be done */
	} else {
		qdf_atomic_add(num - filled, &pdev->rx_ring.refill_debt);
		/* we could not fill all, timer must have been started */
		pdev->refill_retry_timer_doubles++;
	}
	qdf_spin_unlock_bh(&pdev->rx_ring.refill_lock);
}

/*--- rx descriptor field access functions ----------------------------------*/
/*
 * These functions need to use bit masks and shifts to extract fields
 * from the rx descriptors, rather than directly using the bitfields.
 * For example, use
 *     (desc & FIELD_MASK) >> FIELD_LSB
 * rather than
 *     desc.field
 * This allows the functions to work correctly on either little-endian
 * machines (no endianness conversion needed) or big-endian machines
 * (endianness conversion provided automatically by the HW DMA's
 * byte-swizzling).
 */

#ifdef CHECKSUM_OFFLOAD
static inline void
htt_set_checksum_result_ll(htt_pdev_handle pdev, qdf_nbuf_t msdu,
			   struct htt_host_rx_desc_base *rx_desc)
{
#define MAX_IP_VER          2
#define MAX_PROTO_VAL       4
	struct rx_msdu_start *rx_msdu = &rx_desc->msdu_start;
	unsigned int proto = (rx_msdu->tcp_proto) | (rx_msdu->udp_proto << 1);

	/*
	 * HW supports TCP & UDP checksum offload for ipv4 and ipv6
	 */
	static const qdf_nbuf_l4_rx_cksum_type_t
		cksum_table[][MAX_PROTO_VAL][MAX_IP_VER] = {
		{
			/* non-fragmented IP packet */
			/* non TCP/UDP packet */
			{QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO},
			/* TCP packet */
			{QDF_NBUF_RX_CKSUM_TCP, QDF_NBUF_RX_CKSUM_TCPIPV6},
			/* UDP packet */
			{QDF_NBUF_RX_CKSUM_UDP, QDF_NBUF_RX_CKSUM_UDPIPV6},
			/* invalid packet type */
			{QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO},
		},
		{
			/* fragmented IP packet */
			{QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO},
			{QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO},
			{QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO},
			{QDF_NBUF_RX_CKSUM_ZERO, QDF_NBUF_RX_CKSUM_ZERO},
		}
	};

	qdf_nbuf_rx_cksum_t cksum = {
		cksum_table[rx_msdu->ip_frag][proto][rx_msdu->ipv6_proto],
		QDF_NBUF_RX_CKSUM_NONE,
		0
	};

	if (cksum.l4_type !=
	    (qdf_nbuf_l4_rx_cksum_type_t)QDF_NBUF_RX_CKSUM_NONE) {
		cksum.l4_result =
			((*(uint32_t *)&rx_desc->attention) &
			 RX_ATTENTION_0_TCP_UDP_CHKSUM_FAIL_MASK) ?
			QDF_NBUF_RX_CKSUM_NONE :
			QDF_NBUF_RX_CKSUM_TCP_UDP_UNNECESSARY;
	}
	qdf_nbuf_set_rx_cksum(msdu, &cksum);
#undef MAX_IP_VER
#undef MAX_PROTO_VAL
}

#else

static inline
void htt_set_checksum_result_ll(htt_pdev_handle pdev, qdf_nbuf_t msdu,
				struct htt_host_rx_desc_base *rx_desc)
{
}

#endif

static void *htt_rx_msdu_desc_retrieve_ll(htt_pdev_handle pdev, qdf_nbuf_t msdu)
{
	return htt_rx_desc(msdu);
}

static bool htt_rx_mpdu_is_encrypted_ll(htt_pdev_handle pdev, void *mpdu_desc)
{
	struct htt_host_rx_desc_base *rx_desc =
		(struct htt_host_rx_desc_base *)mpdu_desc;

	return (((*((uint32_t *)&rx_desc->mpdu_start)) &
		 RX_MPDU_START_0_ENCRYPTED_MASK) >>
		RX_MPDU_START_0_ENCRYPTED_LSB) ? true : false;
}

static
bool htt_rx_msdu_chan_info_present_ll(htt_pdev_handle pdev, void *mpdu_desc)
{
	return false;
}

static bool htt_rx_msdu_center_freq_ll(htt_pdev_handle pdev,
				       struct ol_txrx_peer_t *peer,
				       void *mpdu_desc,
				       uint16_t *primary_chan_center_freq_mhz,
				       uint16_t *contig_chan1_center_freq_mhz,
				       uint16_t *contig_chan2_center_freq_mhz,
				       uint8_t *phy_mode)
{
	if (primary_chan_center_freq_mhz)
		*primary_chan_center_freq_mhz = 0;
	if (contig_chan1_center_freq_mhz)
		*contig_chan1_center_freq_mhz = 0;
	if (contig_chan2_center_freq_mhz)
		*contig_chan2_center_freq_mhz = 0;
	if (phy_mode)
		*phy_mode = 0;
	return false;
}

static bool
htt_rx_msdu_first_msdu_flag_ll(htt_pdev_handle pdev, void *msdu_desc)
{
	struct htt_host_rx_desc_base *rx_desc =
		(struct htt_host_rx_desc_base *)msdu_desc;
	return (bool)
		(((*(((uint32_t *)&rx_desc->msdu_end) + 4)) &
		  RX_MSDU_END_4_FIRST_MSDU_MASK) >>
		 RX_MSDU_END_4_FIRST_MSDU_LSB);
}

static bool
htt_rx_msdu_desc_key_id_ll(htt_pdev_handle pdev, void *mpdu_desc,
			   uint8_t *key_id)
{
	struct htt_host_rx_desc_base *rx_desc = (struct htt_host_rx_desc_base *)
						mpdu_desc;

	if (!htt_rx_msdu_first_msdu_flag_ll(pdev, mpdu_desc))
		return false;

	*key_id = ((*(((uint32_t *)&rx_desc->msdu_end) + 1)) &
		   (RX_MSDU_END_1_KEY_ID_OCT_MASK >>
		    RX_MSDU_END_1_KEY_ID_OCT_LSB));

	return true;
}

/**
 * htt_rx_mpdu_desc_retry_ll() - Returns the retry bit from the Rx descriptor
 *                               for the Low Latency driver
 * @pdev:                          Handle (pointer) to HTT pdev.
 * @mpdu_desc:                     Void pointer to the Rx descriptor for MPDU
 *                                 before the beginning of the payload.
 *
 *  This function returns the retry bit of the 802.11 header for the
 *  provided rx MPDU descriptor.
 *
 * Return:        boolean -- true if retry is set, false otherwise
 */
static bool
htt_rx_mpdu_desc_retry_ll(htt_pdev_handle pdev, void *mpdu_desc)
{
	struct htt_host_rx_desc_base *rx_desc =
		(struct htt_host_rx_desc_base *)mpdu_desc;

	return
		(bool)(((*((uint32_t *)&rx_desc->mpdu_start)) &
		RX_MPDU_START_0_RETRY_MASK) >>
		RX_MPDU_START_0_RETRY_LSB);
}

static uint16_t htt_rx_mpdu_desc_seq_num_ll(htt_pdev_handle pdev,
					    void *mpdu_desc,
					    bool update_seq_num)
{
	struct htt_host_rx_desc_base *rx_desc =
		(struct htt_host_rx_desc_base *)mpdu_desc;

	return
		(uint16_t)(((*((uint32_t *)&rx_desc->mpdu_start)) &
			     RX_MPDU_START_0_SEQ_NUM_MASK) >>
			    RX_MPDU_START_0_SEQ_NUM_LSB);
}

static void
htt_rx_mpdu_desc_pn_ll(htt_pdev_handle pdev,
		       void *mpdu_desc, union htt_rx_pn_t *pn, int pn_len_bits)
{
	struct htt_host_rx_desc_base *rx_desc =
		(struct htt_host_rx_desc_base *)mpdu_desc;

	switch (pn_len_bits) {
	case 24:
		/* bits 23:0 */
		pn->pn24 = rx_desc->mpdu_start.pn_31_0 & 0xffffff;
		break;
	case 48:
		/* bits 31:0 */
		pn->pn48 = rx_desc->mpdu_start.pn_31_0;
		/* bits 47:32 */
		pn->pn48 |= ((uint64_t)
			     ((*(((uint32_t *)&rx_desc->mpdu_start) + 2))
			      & RX_MPDU_START_2_PN_47_32_MASK))
			<< (32 - RX_MPDU_START_2_PN_47_32_LSB);
		break;
	case 128:
		/* bits 31:0 */
		pn->pn128[0] = rx_desc->mpdu_start.pn_31_0;
		/* bits 47:32 */
		pn->pn128[0] |=
			((uint64_t)((*(((uint32_t *)&rx_desc->mpdu_start) + 2))
				     & RX_MPDU_START_2_PN_47_32_MASK))
			<< (32 - RX_MPDU_START_2_PN_47_32_LSB);
		/* bits 63:48 */
		pn->pn128[0] |=
			((uint64_t)((*(((uint32_t *)&rx_desc->msdu_end) + 2))
				     & RX_MSDU_END_1_EXT_WAPI_PN_63_48_MASK))
			<< (48 - RX_MSDU_END_1_EXT_WAPI_PN_63_48_LSB);
		/* bits 95:64 */
		pn->pn128[1] = rx_desc->msdu_end.ext_wapi_pn_95_64;
		/* bits 127:96 */
		pn->pn128[1] |=
			((uint64_t)rx_desc->msdu_end.ext_wapi_pn_127_96) << 32;
		break;
	default:
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "Error: invalid length spec (%d bits) for PN",
			  pn_len_bits);
	};
}

/**
 * htt_rx_mpdu_desc_tid_ll() - Returns the TID value from the Rx descriptor
 *                             for Low Latency driver
 * @pdev:                        Handle (pointer) to HTT pdev.
 * @mpdu_desc:                   Void pointer to the Rx descriptor for the MPDU
 *                               before the beginning of the payload.
 *
 * This function returns the TID set in the 802.11 QoS Control for the MPDU
 * in the packet header, by looking at the mpdu_start of the Rx descriptor.
 * Rx descriptor gets a copy of the TID from the MAC.
 *
 * Return:        Actual TID set in the packet header.
 */
static uint8_t
htt_rx_mpdu_desc_tid_ll(htt_pdev_handle pdev, void *mpdu_desc)
{
	struct htt_host_rx_desc_base *rx_desc =
		(struct htt_host_rx_desc_base *)mpdu_desc;

	return
		(uint8_t)(((*(((uint32_t *)&rx_desc->mpdu_start) + 2)) &
		RX_MPDU_START_2_TID_MASK) >>
		RX_MPDU_START_2_TID_LSB);
}

static bool htt_rx_msdu_desc_completes_mpdu_ll(htt_pdev_handle pdev,
					       void *msdu_desc)
{
	struct htt_host_rx_desc_base *rx_desc =
		(struct htt_host_rx_desc_base *)msdu_desc;
	return (bool)
		(((*(((uint32_t *)&rx_desc->msdu_end) + 4)) &
		  RX_MSDU_END_4_LAST_MSDU_MASK) >> RX_MSDU_END_4_LAST_MSDU_LSB);
}

static int htt_rx_msdu_has_wlan_mcast_flag_ll(htt_pdev_handle pdev,
					      void *msdu_desc)
{
	struct htt_host_rx_desc_base *rx_desc =
		(struct htt_host_rx_desc_base *)msdu_desc;
	/*
	 * HW rx desc: the mcast_bcast flag is only valid
	 * if first_msdu is set
	 */
	return ((*(((uint32_t *)&rx_desc->msdu_end) + 4)) &
		RX_MSDU_END_4_FIRST_MSDU_MASK) >> RX_MSDU_END_4_FIRST_MSDU_LSB;
}

static bool htt_rx_msdu_is_wlan_mcast_ll(htt_pdev_handle pdev, void *msdu_desc)
{
	struct htt_host_rx_desc_base *rx_desc =
		(struct htt_host_rx_desc_base *)msdu_desc;
	return ((*((uint32_t *)&rx_desc->attention)) &
		RX_ATTENTION_0_MCAST_BCAST_MASK)
		>> RX_ATTENTION_0_MCAST_BCAST_LSB;
}

static int htt_rx_msdu_is_frag_ll(htt_pdev_handle pdev, void *msdu_desc)
{
	struct htt_host_rx_desc_base *rx_desc =
		(struct htt_host_rx_desc_base *)msdu_desc;
	return ((*((uint32_t *)&rx_desc->attention)) &
		 RX_ATTENTION_0_FRAGMENT_MASK) >> RX_ATTENTION_0_FRAGMENT_LSB;
}

static inline int
htt_rx_offload_msdu_cnt_ll(htt_pdev_handle pdev)
{
	return htt_rx_ring_elems(pdev);
}

static int
htt_rx_offload_msdu_pop_ll(htt_pdev_handle pdev,
			   qdf_nbuf_t offload_deliver_msg,
			   int *vdev_id,
			   int *peer_id,
			   int *tid,
			   uint8_t *fw_desc,
			   qdf_nbuf_t *head_buf, qdf_nbuf_t *tail_buf)
{
	qdf_nbuf_t buf;
	uint32_t *msdu_hdr, msdu_len;

	*head_buf = *tail_buf = buf = htt_rx_netbuf_pop(pdev);

	if (qdf_unlikely(!buf)) {
		qdf_print("netbuf pop failed!");
		return 1;
	}

	/* Fake read mpdu_desc to keep desc ptr in sync */
	htt_rx_mpdu_desc_list_next(pdev, NULL);
	qdf_nbuf_set_pktlen(buf, HTT_RX_BUF_SIZE);
#ifdef DEBUG_DMA_DONE
	qdf_nbuf_unmap(pdev->osdev, buf, QDF_DMA_BIDIRECTIONAL);
#else
	qdf_nbuf_unmap(pdev->osdev, buf, QDF_DMA_FROM_DEVICE);
#endif
	msdu_hdr = (uint32_t *)qdf_nbuf_data(buf);

	/* First dword */
	msdu_len = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_LEN_GET(*msdu_hdr);
	*peer_id = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_PEER_ID_GET(*msdu_hdr);

	/* Second dword */
	msdu_hdr++;
	*vdev_id = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_VDEV_ID_GET(*msdu_hdr);
	*tid = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_TID_GET(*msdu_hdr);
	*fw_desc = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_DESC_GET(*msdu_hdr);

	qdf_nbuf_pull_head(buf, HTT_RX_OFFLOAD_DELIVER_IND_MSDU_HDR_BYTES);
	qdf_nbuf_set_pktlen(buf, msdu_len);
	return 0;
}

int
htt_rx_offload_paddr_msdu_pop_ll(htt_pdev_handle pdev,
				 uint32_t *msg_word,
				 int msdu_iter,
				 int *vdev_id,
				 int *peer_id,
				 int *tid,
				 uint8_t *fw_desc,
				 qdf_nbuf_t *head_buf, qdf_nbuf_t *tail_buf)
{
	qdf_nbuf_t buf;
	uint32_t *msdu_hdr, msdu_len;
	uint32_t *curr_msdu;
	qdf_dma_addr_t paddr;

	curr_msdu =
		msg_word + (msdu_iter * HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS);
	paddr = htt_rx_in_ord_paddr_get(curr_msdu);
	*head_buf = *tail_buf = buf = htt_rx_in_order_netbuf_pop(pdev, paddr);

	if (qdf_unlikely(!buf)) {
		qdf_print("netbuf pop failed!");
		return 1;
	}
	qdf_nbuf_set_pktlen(buf, HTT_RX_BUF_SIZE);
#ifdef DEBUG_DMA_DONE
	qdf_nbuf_unmap(pdev->osdev, buf, QDF_DMA_BIDIRECTIONAL);
#else
	qdf_nbuf_unmap(pdev->osdev, buf, QDF_DMA_FROM_DEVICE);
#endif

	if (pdev->cfg.is_first_wakeup_packet)
		htt_get_first_packet_after_wow_wakeup(
			msg_word + NEXT_FIELD_OFFSET_IN32, buf);

	msdu_hdr = (uint32_t *)qdf_nbuf_data(buf);

	/* First dword */
	msdu_len = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_LEN_GET(*msdu_hdr);
	*peer_id = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_PEER_ID_GET(*msdu_hdr);

	/* Second dword */
	msdu_hdr++;
	*vdev_id = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_VDEV_ID_GET(*msdu_hdr);
	*tid = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_TID_GET(*msdu_hdr);
	*fw_desc = HTT_RX_OFFLOAD_DELIVER_IND_MSDU_DESC_GET(*msdu_hdr);

	qdf_nbuf_pull_head(buf, HTT_RX_OFFLOAD_DELIVER_IND_MSDU_HDR_BYTES);
	qdf_nbuf_set_pktlen(buf, msdu_len);
	return 0;
}

#ifdef WLAN_FULL_REORDER_OFFLOAD

/* Number of buckets in the hash table */
#define RX_NUM_HASH_BUCKETS 1024        /* This should always be a power of 2 */
#define RX_NUM_HASH_BUCKETS_MASK (RX_NUM_HASH_BUCKETS - 1)

/* Number of hash entries allocated per bucket */
#define RX_ENTRIES_SIZE 10

#define RX_HASH_FUNCTION(a) \
	((((a) >> 14) ^ ((a) >> 4)) & RX_NUM_HASH_BUCKETS_MASK)

#ifdef RX_HASH_DEBUG_LOG
#define RX_HASH_LOG(x) x
#else
#define RX_HASH_LOG(x)          /* no-op */
#endif

/* Return values: 1 - success, 0 - failure */
#define RX_DESC_DISCARD_IS_SET ((*((u_int8_t *)&rx_desc->fw_desc.u.val)) & \
							FW_RX_DESC_DISCARD_M)
#define RX_DESC_MIC_ERR_IS_SET ((*((u_int8_t *)&rx_desc->fw_desc.u.val)) & \
							FW_RX_DESC_ANY_ERR_M)

#define RX_RING_REFILL_DEBT_MAX 128

/* Initializes the circular linked list */
static inline void htt_list_init(struct htt_list_node *head)
{
	head->prev = head;
	head->next = head;
}

/* Adds entry to the end of the linked list */
static inline void htt_list_add_tail(struct htt_list_node *head,
				     struct htt_list_node *node)
{
	head->prev->next = node;
	node->prev = head->prev;
	node->next = head;
	head->prev = node;
}

/* Removes the entry corresponding to the input node from the linked list */
static inline void htt_list_remove(struct htt_list_node *node)
{
	node->prev->next = node->next;
	node->next->prev = node->prev;
}

/* Helper macro to iterate through the linked list */
#define HTT_LIST_ITER_FWD(iter, head) for (iter = (head)->next;		\
					   (iter) != (head);		\
					   (iter) = (iter)->next)	\

#ifdef RX_HASH_DEBUG
/* Hash cookie related macros */
#define HTT_RX_HASH_COOKIE 0xDEED

#define HTT_RX_HASH_COOKIE_SET(hash_element) \
	((hash_element)->cookie = HTT_RX_HASH_COOKIE)

#define HTT_RX_HASH_COOKIE_CHECK(hash_element) \
	HTT_ASSERT_ALWAYS((hash_element)->cookie == HTT_RX_HASH_COOKIE)

/* Hash count related macros */
#define HTT_RX_HASH_COUNT_INCR(hash_bucket) \
	((hash_bucket)->count++)

#define HTT_RX_HASH_COUNT_DECR(hash_bucket) \
	((hash_bucket)->count--)

#define HTT_RX_HASH_COUNT_RESET(hash_bucket) ((hash_bucket)->count = 0)

#define HTT_RX_HASH_COUNT_PRINT(hash_bucket) \
	RX_HASH_LOG(qdf_print(" count %d\n", (hash_bucket)->count))
#else                           /* RX_HASH_DEBUG */
/* Hash cookie related macros */
#define HTT_RX_HASH_COOKIE_SET(hash_element)    /* no-op */
#define HTT_RX_HASH_COOKIE_CHECK(hash_element)  /* no-op */
/* Hash count related macros */
#define HTT_RX_HASH_COUNT_INCR(hash_bucket)     /* no-op */
#define HTT_RX_HASH_COUNT_DECR(hash_bucket)     /* no-op */
#define HTT_RX_HASH_COUNT_PRINT(hash_bucket)    /* no-op */
#define HTT_RX_HASH_COUNT_RESET(hash_bucket)    /* no-op */
#endif /* RX_HASH_DEBUG */

/*
 * Inserts the given "physical address - network buffer" pair into the
 * hash table for the given pdev. This function will do the following:
 * 1. Determine which bucket to insert the pair into
 * 2. First try to allocate the hash entry for this pair from the pre-allocated
 *    entries list
 * 3. If there are no more entries in the pre-allocated entries list, allocate
 *    the hash entry from the hash memory pool
 * Note: this function is not thread-safe
 * Returns 0 - success, 1 - failure
 */
int
htt_rx_hash_list_insert(struct htt_pdev_t *pdev,
			qdf_dma_addr_t paddr,
			qdf_nbuf_t netbuf)
{
	int i;
	int rc = 0;
	struct htt_rx_hash_entry *hash_element = NULL;

	qdf_spin_lock_bh(&pdev->rx_ring.rx_hash_lock);

	/* get rid of the marking bits if they are available */
	paddr = htt_paddr_trim_to_37(paddr);

	i = RX_HASH_FUNCTION(paddr);

	/* Check if there are any entries in the pre-allocated free list */
	if (pdev->rx_ring.hash_table[i]->freepool.next !=
	    &pdev->rx_ring.hash_table[i]->freepool) {
		hash_element =
			(struct htt_rx_hash_entry *)(
				(char *)
				pdev->rx_ring.hash_table[i]->freepool.next -
				pdev->rx_ring.listnode_offset);
		if (qdf_unlikely(!hash_element)) {
			HTT_ASSERT_ALWAYS(0);
			rc = 1;
			goto hli_end;
		}

		htt_list_remove(pdev->rx_ring.hash_table[i]->freepool.next);
	} else {
		hash_element = qdf_mem_malloc(sizeof(struct htt_rx_hash_entry));
		if (qdf_unlikely(!hash_element)) {
			HTT_ASSERT_ALWAYS(0);
			rc = 1;
			goto hli_end;
		}
		hash_element->fromlist = 0;
	}

	hash_element->netbuf = netbuf;
	hash_element->paddr = paddr;
	HTT_RX_HASH_COOKIE_SET(hash_element);

	htt_list_add_tail(&pdev->rx_ring.hash_table[i]->listhead,
			  &hash_element->listnode);

	RX_HASH_LOG(qdf_print("rx hash: paddr 0x%x netbuf %pK bucket %d\n",
			      paddr, netbuf, (int)i));

	if (htt_rx_ring_smmu_mapped(pdev)) {
		if (qdf_unlikely(qdf_nbuf_is_rx_ipa_smmu_map(netbuf))) {
			qdf_err("Already smmu mapped, nbuf: %pK",
				netbuf);
			qdf_assert_always(0);
		}
		qdf_nbuf_set_rx_ipa_smmu_map(netbuf, true);
	}

	HTT_RX_HASH_COUNT_INCR(pdev->rx_ring.hash_table[i]);
	HTT_RX_HASH_COUNT_PRINT(pdev->rx_ring.hash_table[i]);

hli_end:
	qdf_spin_unlock_bh(&pdev->rx_ring.rx_hash_lock);
	return rc;
}

/*
 * Given a physical address this function will find the corresponding network
 *  buffer from the hash table.
 *  paddr is already stripped off of higher marking bits.
 */
qdf_nbuf_t htt_rx_hash_list_lookup(struct htt_pdev_t *pdev,
				   qdf_dma_addr_t     paddr)
{
	uint32_t i;
	struct htt_list_node *list_iter = NULL;
	qdf_nbuf_t netbuf = NULL;
	struct htt_rx_hash_entry *hash_entry;

	qdf_spin_lock_bh(&pdev->rx_ring.rx_hash_lock);

	if (!pdev->rx_ring.hash_table) {
		qdf_spin_unlock_bh(&pdev->rx_ring.rx_hash_lock);
		return NULL;
	}

	i = RX_HASH_FUNCTION(paddr);

	HTT_LIST_ITER_FWD(list_iter, &pdev->rx_ring.hash_table[i]->listhead) {
		hash_entry = (struct htt_rx_hash_entry *)
			     ((char *)list_iter -
			      pdev->rx_ring.listnode_offset);

		HTT_RX_HASH_COOKIE_CHECK(hash_entry);

		if (hash_entry->paddr == paddr) {
			/* Found the entry corresponding to paddr */
			netbuf = hash_entry->netbuf;
			/* set netbuf to NULL to trace if freed entry
			 * is getting unmapped in hash deinit.
			 */
			hash_entry->netbuf = NULL;
			htt_list_remove(&hash_entry->listnode);
			HTT_RX_HASH_COUNT_DECR(pdev->rx_ring.hash_table[i]);
			/*
			 * if the rx entry is from the pre-allocated list,
			 * return it
			 */
			if (hash_entry->fromlist)
				htt_list_add_tail(
					&pdev->rx_ring.hash_table[i]->freepool,
					&hash_entry->listnode);
			else
				qdf_mem_free(hash_entry);

			htt_rx_dbg_rxbuf_reset(pdev, netbuf);
			break;
		}
	}

	if (netbuf && htt_rx_ring_smmu_mapped(pdev)) {
		if (qdf_unlikely(!qdf_nbuf_is_rx_ipa_smmu_map(netbuf))) {
			qdf_err("smmu not mapped nbuf: %pK", netbuf);
			qdf_assert_always(0);
		}
	}

	RX_HASH_LOG(qdf_print("rx hash: paddr 0x%llx, netbuf %pK, bucket %d\n",
			      (unsigned long long)paddr, netbuf, (int)i));
	HTT_RX_HASH_COUNT_PRINT(pdev->rx_ring.hash_table[i]);

	qdf_spin_unlock_bh(&pdev->rx_ring.rx_hash_lock);

	if (!netbuf) {
		qdf_print("rx hash: no entry found for %llx!\n",
			  (unsigned long long)paddr);
		cds_trigger_recovery(QDF_RX_HASH_NO_ENTRY_FOUND);
	}

	return netbuf;
}

/*
 * Initialization function of the rx buffer hash table. This function will
 * allocate a hash table of a certain pre-determined size and initialize all
 * the elements
 */
static int htt_rx_hash_init(struct htt_pdev_t *pdev)
{
	int i, j;
	int rc = 0;
	void *allocation;

	HTT_ASSERT2(QDF_IS_PWR2(RX_NUM_HASH_BUCKETS));

	/* hash table is array of bucket pointers */
	pdev->rx_ring.hash_table =
		qdf_mem_malloc(RX_NUM_HASH_BUCKETS *
			       sizeof(struct htt_rx_hash_bucket *));

	if (!pdev->rx_ring.hash_table)
		return 1;

	qdf_spinlock_create(&pdev->rx_ring.rx_hash_lock);
	qdf_spin_lock_bh(&pdev->rx_ring.rx_hash_lock);

	for (i = 0; i < RX_NUM_HASH_BUCKETS; i++) {
		qdf_spin_unlock_bh(&pdev->rx_ring.rx_hash_lock);
		/* pre-allocate bucket and pool of entries for this bucket */
		allocation = qdf_mem_malloc((sizeof(struct htt_rx_hash_bucket) +
			(RX_ENTRIES_SIZE * sizeof(struct htt_rx_hash_entry))));
		qdf_spin_lock_bh(&pdev->rx_ring.rx_hash_lock);
		pdev->rx_ring.hash_table[i] = allocation;

		HTT_RX_HASH_COUNT_RESET(pdev->rx_ring.hash_table[i]);

		/* initialize the hash table buckets */
		htt_list_init(&pdev->rx_ring.hash_table[i]->listhead);

		/* initialize the hash table free pool per bucket */
		htt_list_init(&pdev->rx_ring.hash_table[i]->freepool);

		/* pre-allocate a pool of entries for this bucket */
		pdev->rx_ring.hash_table[i]->entries =
			(struct htt_rx_hash_entry *)
			((uint8_t *)pdev->rx_ring.hash_table[i] +
			sizeof(struct htt_rx_hash_bucket));

		if (!pdev->rx_ring.hash_table[i]->entries) {
			qdf_print("rx hash bucket %d entries alloc failed\n",
				  (int)i);
			while (i) {
				i--;
				qdf_mem_free(pdev->rx_ring.hash_table[i]);
			}
			qdf_mem_free(pdev->rx_ring.hash_table);
			pdev->rx_ring.hash_table = NULL;
			rc = 1;
			goto hi_end;
		}

		/* initialize the free list with pre-allocated entries */
		for (j = 0; j < RX_ENTRIES_SIZE; j++) {
			pdev->rx_ring.hash_table[i]->entries[j].fromlist = 1;
			htt_list_add_tail(
				&pdev->rx_ring.hash_table[i]->freepool,
				&pdev->rx_ring.hash_table[i]->entries[j].
				listnode);
		}
	}

	pdev->rx_ring.listnode_offset =
		qdf_offsetof(struct htt_rx_hash_entry, listnode);
hi_end:
	qdf_spin_unlock_bh(&pdev->rx_ring.rx_hash_lock);

	return rc;
}

/* De -initialization function of the rx buffer hash table. This function will
 *   free up the hash table which includes freeing all the pending rx buffers
 */
static void htt_rx_hash_deinit(struct htt_pdev_t *pdev)
{
	uint32_t i;
	struct htt_rx_hash_entry *hash_entry;
	struct htt_rx_hash_bucket **hash_table;
	struct htt_list_node *list_iter = NULL;
	qdf_mem_info_t mem_map_table = {0};
	bool ipa_smmu = false;

	if (!pdev->rx_ring.hash_table)
		return;

	qdf_spin_lock_bh(&pdev->rx_ring.rx_hash_lock);
	ipa_smmu = htt_rx_ring_smmu_mapped(pdev);
	hash_table = pdev->rx_ring.hash_table;
	pdev->rx_ring.hash_table = NULL;
	qdf_spin_unlock_bh(&pdev->rx_ring.rx_hash_lock);

	for (i = 0; i < RX_NUM_HASH_BUCKETS; i++) {
		/* Free the hash entries in hash bucket i */
		list_iter = hash_table[i]->listhead.next;
		while (list_iter != &hash_table[i]->listhead) {
			hash_entry =
				(struct htt_rx_hash_entry *)((char *)list_iter -
							     pdev->rx_ring.
							     listnode_offset);
			if (hash_entry->netbuf) {
				if (ipa_smmu) {
					if (qdf_unlikely(
						!qdf_nbuf_is_rx_ipa_smmu_map(
							hash_entry->netbuf))) {
						qdf_err("nbuf: %pK NOT mapped",
							hash_entry->netbuf);
						qdf_assert_always(0);
					}
					qdf_nbuf_set_rx_ipa_smmu_map(
							hash_entry->netbuf,
							false);
					qdf_update_mem_map_table(pdev->osdev,
						&mem_map_table,
						QDF_NBUF_CB_PADDR(
							hash_entry->netbuf),
						HTT_RX_BUF_SIZE);

					qdf_assert_always(
						!cds_smmu_map_unmap(
							false, 1,
							&mem_map_table));
				}
#ifdef DEBUG_DMA_DONE
				qdf_nbuf_unmap(pdev->osdev, hash_entry->netbuf,
					       QDF_DMA_BIDIRECTIONAL);
#else
				qdf_nbuf_unmap(pdev->osdev, hash_entry->netbuf,
					       QDF_DMA_FROM_DEVICE);
#endif
				qdf_nbuf_free(hash_entry->netbuf);
				hash_entry->paddr = 0;
			}
			list_iter = list_iter->next;

			if (!hash_entry->fromlist)
				qdf_mem_free(hash_entry);
		}

		qdf_mem_free(hash_table[i]);
	}
	qdf_mem_free(hash_table);

	qdf_spinlock_destroy(&pdev->rx_ring.rx_hash_lock);
}

int htt_rx_msdu_buff_in_order_replenish(htt_pdev_handle pdev, uint32_t num)
{
	int filled = 0;

	if (!qdf_spin_trylock_bh(&pdev->rx_ring.refill_lock)) {
		if (qdf_atomic_read(&pdev->rx_ring.refill_debt)
			 < RX_RING_REFILL_DEBT_MAX) {
			qdf_atomic_add(num, &pdev->rx_ring.refill_debt);
			pdev->rx_buff_debt_invoked++;
			return filled; /* 0 */
		}
		/*
		 * else:
		 * If we have quite a debt, then it is better for the lock
		 * holder to finish its work and then acquire the lock and
		 * fill our own part.
		 */
		qdf_spin_lock_bh(&pdev->rx_ring.refill_lock);
	}
	pdev->rx_buff_fill_n_invoked++;

	filled = htt_rx_ring_fill_n(pdev, num);

	if (filled > num) {
		/* we served ourselves and some other debt */
		/* sub is safer than  = 0 */
		qdf_atomic_sub(filled - num, &pdev->rx_ring.refill_debt);
	} else {
		qdf_atomic_add(num - filled, &pdev->rx_ring.refill_debt);
	}
	qdf_spin_unlock_bh(&pdev->rx_ring.refill_lock);

	return filled;
}

#if defined(WLAN_FEATURE_TSF_PLUS) && !defined(CONFIG_HL_SUPPORT)
/**
 * htt_rx_tail_msdu_timestamp() - update tail msdu tsf64 timestamp
 * @tail_rx_desc: pointer to tail msdu descriptor
 * @timestamp_rx_desc: pointer to timestamp msdu descriptor
 *
 * Return: none
 */
static inline void htt_rx_tail_msdu_timestamp(
			struct htt_host_rx_desc_base *tail_rx_desc,
			struct htt_host_rx_desc_base *timestamp_rx_desc)
{
	if (tail_rx_desc) {
		if (!timestamp_rx_desc) {
			tail_rx_desc->ppdu_end.wb_timestamp_lower_32 = 0;
			tail_rx_desc->ppdu_end.wb_timestamp_upper_32 = 0;
		} else {
			if (timestamp_rx_desc != tail_rx_desc) {
				tail_rx_desc->ppdu_end.wb_timestamp_lower_32 =
			timestamp_rx_desc->ppdu_end.wb_timestamp_lower_32;
				tail_rx_desc->ppdu_end.wb_timestamp_upper_32 =
			timestamp_rx_desc->ppdu_end.wb_timestamp_upper_32;
			}
		}
	}
}
#else
static inline void htt_rx_tail_msdu_timestamp(
			struct htt_host_rx_desc_base *tail_rx_desc,
			struct htt_host_rx_desc_base *timestamp_rx_desc)
{
}
#endif

static int
htt_rx_amsdu_rx_in_order_pop_ll(htt_pdev_handle pdev,
				qdf_nbuf_t rx_ind_msg,
				qdf_nbuf_t *head_msdu, qdf_nbuf_t *tail_msdu,
				uint32_t *replenish_cnt)
{
	qdf_nbuf_t msdu, next, prev = NULL;
	uint8_t *rx_ind_data;
	uint32_t *msg_word;
	uint32_t rx_ctx_id;
	unsigned int msdu_count = 0;
	uint8_t offload_ind, frag_ind;
	uint8_t peer_id;
	struct htt_host_rx_desc_base *rx_desc = NULL;
	enum rx_pkt_fate status = RX_PKT_FATE_SUCCESS;
	qdf_dma_addr_t paddr;
	qdf_mem_info_t mem_map_table = {0};
	int ret = 1;
	struct htt_host_rx_desc_base *timestamp_rx_desc = NULL;

	HTT_ASSERT1(htt_rx_in_order_ring_elems(pdev) != 0);

	rx_ind_data = qdf_nbuf_data(rx_ind_msg);
	rx_ctx_id = QDF_NBUF_CB_RX_CTX_ID(rx_ind_msg);
	msg_word = (uint32_t *)rx_ind_data;
	peer_id = HTT_RX_IN_ORD_PADDR_IND_PEER_ID_GET(
					*(u_int32_t *)rx_ind_data);

	offload_ind = HTT_RX_IN_ORD_PADDR_IND_OFFLOAD_GET(*msg_word);
	frag_ind = HTT_RX_IN_ORD_PADDR_IND_FRAG_GET(*msg_word);

	/* Get the total number of MSDUs */
	msdu_count = HTT_RX_IN_ORD_PADDR_IND_MSDU_CNT_GET(*(msg_word + 1));
	HTT_RX_CHECK_MSDU_COUNT(msdu_count);

	ol_rx_update_histogram_stats(msdu_count, frag_ind, offload_ind);
	htt_rx_dbg_rxbuf_httrxind(pdev, msdu_count);

	msg_word =
		(uint32_t *)(rx_ind_data + HTT_RX_IN_ORD_PADDR_IND_HDR_BYTES);
	if (offload_ind) {
		ol_rx_offload_paddr_deliver_ind_handler(pdev, msdu_count,
							msg_word);
		*head_msdu = *tail_msdu = NULL;
		ret = 0;
		goto end;
	}

	paddr = htt_rx_in_ord_paddr_get(msg_word);
	(*head_msdu) = msdu = htt_rx_in_order_netbuf_pop(pdev, paddr);

	if (qdf_unlikely(!msdu)) {
		qdf_print("netbuf pop failed!");
		*tail_msdu = NULL;
		pdev->rx_ring.pop_fail_cnt++;
		ret = 0;
		goto end;
	}

	while (msdu_count > 0) {
		if (qdf_nbuf_is_rx_ipa_smmu_map(msdu)) {
			/*
			 * nbuf was already detached from hash_entry,
			 * there is no parallel IPA context to access
			 * this nbuf for smmu map/unmap, so updating
			 * this flag here without lock.
			 *
			 * This flag was not updated in netbuf_pop context
			 * htt_rx_hash_list_lookup (where lock held), to
			 * differentiate whether this nbuf to be
			 * smmu unmapped or it was never mapped so far.
			 */
			qdf_nbuf_set_rx_ipa_smmu_map(msdu, false);
			qdf_update_mem_map_table(pdev->osdev, &mem_map_table,
						 QDF_NBUF_CB_PADDR(msdu),
						 HTT_RX_BUF_SIZE);
			qdf_assert_always(
				!cds_smmu_map_unmap(false, 1, &mem_map_table));
		}

		/*
		 * Set the netbuf length to be the entire buffer length
		 * initially, so the unmap will unmap the entire buffer.
		 */
		qdf_nbuf_set_pktlen(msdu, HTT_RX_BUF_SIZE);
#ifdef DEBUG_DMA_DONE
		qdf_nbuf_unmap(pdev->osdev, msdu, QDF_DMA_BIDIRECTIONAL);
#else
		qdf_nbuf_unmap(pdev->osdev, msdu, QDF_DMA_FROM_DEVICE);
#endif
		msdu_count--;

		if (pdev->rx_buff_pool.netbufs_ring &&
		    QDF_NBUF_CB_RX_PACKET_BUFF_POOL(msdu) &&
		    !htt_rx_buff_pool_refill(pdev, msdu)) {
			if (!msdu_count) {
				if (!prev) {
					*head_msdu = *tail_msdu = NULL;
					ret = 1;
					goto end;
				}
				*tail_msdu = prev;
				qdf_nbuf_set_next(prev, NULL);
				goto end;
			} else {
				/* get the next msdu */
				msg_word += HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS;
				paddr = htt_rx_in_ord_paddr_get(msg_word);
				next = htt_rx_in_order_netbuf_pop(pdev, paddr);
				if (qdf_unlikely(!next)) {
					qdf_print("netbuf pop failed!");
					*tail_msdu = NULL;
					pdev->rx_ring.pop_fail_cnt++;
					ret = 0;
					goto end;
				}
				/* if this is not the first msdu, update the
				 * next pointer of the preceding msdu
				 */
				if (prev) {
					qdf_nbuf_set_next(prev, next);
				} else {
					/* if this is the first msdu, update
					 * head pointer
					 */
					*head_msdu = next;
				}
				msdu = next;
				continue;
			}
		}

		/* cache consistency has been taken care of by qdf_nbuf_unmap */
		rx_desc = htt_rx_desc(msdu);
		htt_rx_extract_lro_info(msdu, rx_desc);

		/* check if the msdu is last mpdu */
		if (rx_desc->attention.last_mpdu)
			timestamp_rx_desc = rx_desc;

		/*
		 * Make the netbuf's data pointer point to the payload rather
		 * than the descriptor.
		 */
		qdf_nbuf_pull_head(msdu, HTT_RX_STD_DESC_RESERVATION);

		QDF_NBUF_CB_DP_TRACE_PRINT(msdu) = false;
		qdf_dp_trace_set_track(msdu, QDF_RX);
		QDF_NBUF_CB_TX_PACKET_TRACK(msdu) = QDF_NBUF_TX_PKT_DATA_TRACK;
		QDF_NBUF_CB_RX_CTX_ID(msdu) = rx_ctx_id;

		if (qdf_nbuf_is_ipv4_arp_pkt(msdu))
			QDF_NBUF_CB_GET_PACKET_TYPE(msdu) =
				QDF_NBUF_CB_PACKET_TYPE_ARP;

		DPTRACE(qdf_dp_trace(msdu,
				     QDF_DP_TRACE_RX_HTT_PACKET_PTR_RECORD,
				     QDF_TRACE_DEFAULT_PDEV_ID,
				     qdf_nbuf_data_addr(msdu),
				     sizeof(qdf_nbuf_data(msdu)), QDF_RX));

		qdf_nbuf_trim_tail(msdu,
				   HTT_RX_BUF_SIZE -
				   (RX_STD_DESC_SIZE +
				    HTT_RX_IN_ORD_PADDR_IND_MSDU_LEN_GET(
				    *(msg_word + NEXT_FIELD_OFFSET_IN32))));
#if defined(HELIUMPLUS_DEBUG)
		ol_txrx_dump_pkt(msdu, 0, 64);
#endif
		*((uint8_t *)&rx_desc->fw_desc.u.val) =
			HTT_RX_IN_ORD_PADDR_IND_FW_DESC_GET(*(msg_word +
						NEXT_FIELD_OFFSET_IN32));

		/* calling callback function for packet logging */
		if (pdev->rx_pkt_dump_cb) {
			if (qdf_unlikely(RX_DESC_MIC_ERR_IS_SET &&
					 !RX_DESC_DISCARD_IS_SET))
				status = RX_PKT_FATE_FW_DROP_INVALID;
			pdev->rx_pkt_dump_cb(msdu, peer_id, status);
		}

		if (pdev->cfg.is_first_wakeup_packet)
			htt_get_first_packet_after_wow_wakeup(
				msg_word + NEXT_FIELD_OFFSET_IN32, msdu);

		/* if discard flag is set (SA is self MAC), then
		 * don't check mic failure.
		 */
		if (qdf_unlikely(RX_DESC_MIC_ERR_IS_SET &&
				 !RX_DESC_DISCARD_IS_SET)) {
			uint8_t tid =
				HTT_RX_IN_ORD_PADDR_IND_EXT_TID_GET(
					*(u_int32_t *)rx_ind_data);
			ol_rx_mic_error_handler(pdev->txrx_pdev, tid, peer_id,
						rx_desc, msdu);

			htt_rx_desc_frame_free(pdev, msdu);
			/* if this is the last msdu */
			if (!msdu_count) {
				/* if this is the only msdu */
				if (!prev) {
					*head_msdu = *tail_msdu = NULL;
					ret = 0;
					goto end;
				}
				*tail_msdu = prev;
				qdf_nbuf_set_next(prev, NULL);
				goto end;
			} else { /* if this is not the last msdu */
				/* get the next msdu */
				msg_word += HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS;
				paddr = htt_rx_in_ord_paddr_get(msg_word);
				next = htt_rx_in_order_netbuf_pop(pdev, paddr);
				if (qdf_unlikely(!next)) {
					qdf_print("netbuf pop failed!");
					*tail_msdu = NULL;
					pdev->rx_ring.pop_fail_cnt++;
					ret = 0;
					goto end;
				}

				/* if this is not the first msdu, update the
				 * next pointer of the preceding msdu
				 */
				if (prev) {
					qdf_nbuf_set_next(prev, next);
				} else {
					/* if this is the first msdu, update the
					 * head pointer
					 */
					*head_msdu = next;
				}
				msdu = next;
				continue;
			}
		}
		/* check if this is the last msdu */
		if (msdu_count) {
			msg_word += HTT_RX_IN_ORD_PADDR_IND_MSDU_DWORDS;
			paddr = htt_rx_in_ord_paddr_get(msg_word);
			next = htt_rx_in_order_netbuf_pop(pdev, paddr);
			if (qdf_unlikely(!next)) {
				qdf_print("netbuf pop failed!");
				*tail_msdu = NULL;
				pdev->rx_ring.pop_fail_cnt++;
				ret = 0;
				goto end;
			}
			qdf_nbuf_set_next(msdu, next);
			prev = msdu;
			msdu = next;
		} else {
			*tail_msdu = msdu;
			qdf_nbuf_set_next(msdu, NULL);
		}
	}

	htt_rx_tail_msdu_timestamp(rx_desc, timestamp_rx_desc);

end:
	return ret;
}

static void *htt_rx_in_ord_mpdu_desc_list_next_ll(htt_pdev_handle pdev,
						  qdf_nbuf_t netbuf)
{
	return (void *)htt_rx_desc(netbuf);
}
#else

static inline
int htt_rx_hash_init(struct htt_pdev_t *pdev)
{
	return 0;
}

static inline
void htt_rx_hash_deinit(struct htt_pdev_t *pdev)
{
}

static inline int
htt_rx_amsdu_rx_in_order_pop_ll(htt_pdev_handle pdev,
				qdf_nbuf_t rx_ind_msg,
				qdf_nbuf_t *head_msdu, qdf_nbuf_t *tail_msdu,
				uint32_t *replenish_cnt)
{
	return 0;
}

static inline
void *htt_rx_in_ord_mpdu_desc_list_next_ll(htt_pdev_handle pdev,
					   qdf_nbuf_t netbuf)
{
	return NULL;
}
#endif

#ifdef WLAN_PARTIAL_REORDER_OFFLOAD

/* AR9888v1 WORKAROUND for EV#112367 */
/* FIX THIS - remove this WAR when the bug is fixed */
#define PEREGRINE_1_0_ZERO_LEN_PHY_ERR_WAR

static int
htt_rx_amsdu_pop_ll(htt_pdev_handle pdev,
		    qdf_nbuf_t rx_ind_msg,
		    qdf_nbuf_t *head_msdu, qdf_nbuf_t *tail_msdu,
		    uint32_t *msdu_count)
{
	int msdu_len, msdu_chaining = 0;
	qdf_nbuf_t msdu;
	struct htt_host_rx_desc_base *rx_desc;
	uint8_t *rx_ind_data;
	uint32_t *msg_word, num_msdu_bytes;
	qdf_dma_addr_t rx_desc_paddr;
	enum htt_t2h_msg_type msg_type;
	uint8_t pad_bytes = 0;

	HTT_ASSERT1(htt_rx_ring_elems(pdev) != 0);
	rx_ind_data = qdf_nbuf_data(rx_ind_msg);
	msg_word = (uint32_t *)rx_ind_data;

	msg_type = HTT_T2H_MSG_TYPE_GET(*msg_word);

	if (qdf_unlikely(msg_type == HTT_T2H_MSG_TYPE_RX_FRAG_IND)) {
		num_msdu_bytes = HTT_RX_FRAG_IND_FW_RX_DESC_BYTES_GET(
			*(msg_word + HTT_RX_FRAG_IND_HDR_PREFIX_SIZE32));
	} else {
		num_msdu_bytes = HTT_RX_IND_FW_RX_DESC_BYTES_GET(
			*(msg_word
			  + HTT_RX_IND_HDR_PREFIX_SIZE32
			  + HTT_RX_PPDU_DESC_SIZE32));
	}
	msdu = *head_msdu = htt_rx_netbuf_pop(pdev);
	while (1) {
		int last_msdu, msdu_len_invalid, msdu_chained;
		int byte_offset;
		qdf_nbuf_t next;

		/*
		 * Set the netbuf length to be the entire buffer length
		 * initially, so the unmap will unmap the entire buffer.
		 */
		qdf_nbuf_set_pktlen(msdu, HTT_RX_BUF_SIZE);
#ifdef DEBUG_DMA_DONE
		qdf_nbuf_unmap(pdev->osdev, msdu, QDF_DMA_BIDIRECTIONAL);
#else
		qdf_nbuf_unmap(pdev->osdev, msdu, QDF_DMA_FROM_DEVICE);
#endif

		/* cache consistency has been taken care of by qdf_nbuf_unmap */

		/*
		 * Now read the rx descriptor.
		 * Set the length to the appropriate value.
		 * Check if this MSDU completes a MPDU.
		 */
		rx_desc = htt_rx_desc(msdu);
#if defined(HELIUMPLUS)
		if (HTT_WIFI_IP(pdev, 2, 0))
			pad_bytes = rx_desc->msdu_end.l3_header_padding;
#endif /* defined(HELIUMPLUS) */

		/*
		 * Save PADDR of descriptor and make the netbuf's data pointer
		 * point to the payload rather than the descriptor.
		 */
		rx_desc_paddr = QDF_NBUF_CB_PADDR(msdu);
		qdf_nbuf_pull_head(msdu, HTT_RX_STD_DESC_RESERVATION +
					 pad_bytes);

		/*
		 * Sanity check - confirm the HW is finished filling in
		 * the rx data.
		 * If the HW and SW are working correctly, then it's guaranteed
		 * that the HW's MAC DMA is done before this point in the SW.
		 * To prevent the case that we handle a stale Rx descriptor,
		 * just assert for now until we have a way to recover.
		 */

#ifdef DEBUG_DMA_DONE
		if (qdf_unlikely(!((*(uint32_t *)&rx_desc->attention)
				   & RX_ATTENTION_0_MSDU_DONE_MASK))) {
			int dbg_iter = MAX_DONE_BIT_CHECK_ITER;

			QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
				  "malformed frame");

			while (dbg_iter &&
			       (!((*(uint32_t *)&rx_desc->attention) &
				  RX_ATTENTION_0_MSDU_DONE_MASK))) {
				qdf_mdelay(1);
				qdf_mem_dma_sync_single_for_cpu(
					pdev->osdev,
					rx_desc_paddr,
					HTT_RX_STD_DESC_RESERVATION,
					DMA_FROM_DEVICE);

				QDF_TRACE(QDF_MODULE_ID_HTT,
					  QDF_TRACE_LEVEL_INFO,
					  "debug iter %d success %d", dbg_iter,
					  pdev->rx_ring.dbg_sync_success);

				dbg_iter--;
			}

			if (qdf_unlikely(!((*(uint32_t *)&rx_desc->attention)
					   & RX_ATTENTION_0_MSDU_DONE_MASK))) {
#ifdef HTT_RX_RESTORE
				QDF_TRACE(QDF_MODULE_ID_HTT,
					  QDF_TRACE_LEVEL_ERROR,
					  "RX done bit error detected!");

				qdf_nbuf_set_next(msdu, NULL);
				*tail_msdu = msdu;
				pdev->rx_ring.rx_reset = 1;
				return msdu_chaining;
#else
				wma_cli_set_command(0, GEN_PARAM_CRASH_INJECT,
						    0, GEN_CMD);
				HTT_ASSERT_ALWAYS(0);
#endif
			}
			pdev->rx_ring.dbg_sync_success++;
			QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO,
				  "debug iter %d success %d", dbg_iter,
				  pdev->rx_ring.dbg_sync_success);
		}
#else
		HTT_ASSERT_ALWAYS((*(uint32_t *)&rx_desc->attention) &
				  RX_ATTENTION_0_MSDU_DONE_MASK);
#endif
		/*
		 * Copy the FW rx descriptor for this MSDU from the rx
		 * indication message into the MSDU's netbuf.
		 * HL uses the same rx indication message definition as LL, and
		 * simply appends new info (fields from the HW rx desc, and the
		 * MSDU payload itself).
		 * So, the offset into the rx indication message only has to
		 * account for the standard offset of the per-MSDU FW rx
		 * desc info within the message, and how many bytes of the
		 * per-MSDU FW rx desc info have already been consumed.
		 * (And the endianness of the host,
		 * since for a big-endian host, the rx ind message contents,
		 * including the per-MSDU rx desc bytes, were byteswapped during
		 * upload.)
		 */
		if (pdev->rx_ind_msdu_byte_idx < num_msdu_bytes) {
			if (qdf_unlikely
				    (msg_type == HTT_T2H_MSG_TYPE_RX_FRAG_IND))
				byte_offset =
					HTT_ENDIAN_BYTE_IDX_SWAP
					(HTT_RX_FRAG_IND_FW_DESC_BYTE_OFFSET);
			else
				byte_offset =
					HTT_ENDIAN_BYTE_IDX_SWAP
					(HTT_RX_IND_FW_RX_DESC_BYTE_OFFSET +
						pdev->rx_ind_msdu_byte_idx);

			*((uint8_t *)&rx_desc->fw_desc.u.val) =
				rx_ind_data[byte_offset];
			/*
			 * The target is expected to only provide the basic
			 * per-MSDU rx descriptors.  Just to be sure,
			 * verify that the target has not attached
			 * extension data (e.g. LRO flow ID).
			 */
			/*
			 * The assertion below currently doesn't work for
			 * RX_FRAG_IND messages, since their format differs
			 * from the RX_IND format (no FW rx PPDU desc in
			 * the current RX_FRAG_IND message).
			 * If the RX_FRAG_IND message format is updated to match
			 * the RX_IND message format, then the following
			 * assertion can be restored.
			 */
			/*
			 * qdf_assert((rx_ind_data[byte_offset] &
			 * FW_RX_DESC_EXT_M) == 0);
			 */
			pdev->rx_ind_msdu_byte_idx += 1;
			/* or more, if there's ext data */
		} else {
			/*
			 * When an oversized AMSDU happened, FW will lost some
			 * of MSDU status - in this case, the FW descriptors
			 * provided will be less than the actual MSDUs
			 * inside this MPDU.
			 * Mark the FW descriptors so that it will still
			 * deliver to upper stack, if no CRC error for the MPDU.
			 *
			 * FIX THIS - the FW descriptors are actually for MSDUs
			 * in the end of this A-MSDU instead of the beginning.
			 */
			*((uint8_t *)&rx_desc->fw_desc.u.val) = 0;
		}

		/*
		 *  TCP/UDP checksum offload support
		 */
		htt_set_checksum_result_ll(pdev, msdu, rx_desc);

		msdu_len_invalid = (*(uint32_t *)&rx_desc->attention) &
				   RX_ATTENTION_0_MPDU_LENGTH_ERR_MASK;
		msdu_chained = (((*(uint32_t *)&rx_desc->frag_info) &
				 RX_FRAG_INFO_0_RING2_MORE_COUNT_MASK) >>
				RX_FRAG_INFO_0_RING2_MORE_COUNT_LSB);
		msdu_len =
			((*((uint32_t *)&rx_desc->msdu_start)) &
			 RX_MSDU_START_0_MSDU_LENGTH_MASK) >>
			RX_MSDU_START_0_MSDU_LENGTH_LSB;

		do {
			if (!msdu_len_invalid && !msdu_chained) {
#if defined(PEREGRINE_1_0_ZERO_LEN_PHY_ERR_WAR)
				if (msdu_len > 0x3000)
					break;
#endif
				qdf_nbuf_trim_tail(msdu,
						   HTT_RX_BUF_SIZE -
						   (RX_STD_DESC_SIZE +
						    msdu_len));
			}
		} while (0);

		while (msdu_chained--) {
			next = htt_rx_netbuf_pop(pdev);
			qdf_nbuf_set_pktlen(next, HTT_RX_BUF_SIZE);
			msdu_len -= HTT_RX_BUF_SIZE;
			qdf_nbuf_set_next(msdu, next);
			msdu = next;
			msdu_chaining = 1;

			if (msdu_chained == 0) {
				/* Trim the last one to the correct size -
				 * accounting for inconsistent HW lengths
				 * causing length overflows and underflows
				 */
				if (((unsigned int)msdu_len) >
				    ((unsigned int)
				     (HTT_RX_BUF_SIZE - RX_STD_DESC_SIZE))) {
					msdu_len =
						(HTT_RX_BUF_SIZE -
						 RX_STD_DESC_SIZE);
				}

				qdf_nbuf_trim_tail(next,
						   HTT_RX_BUF_SIZE -
						   (RX_STD_DESC_SIZE +
						    msdu_len));
			}
		}

		last_msdu =
			((*(((uint32_t *)&rx_desc->msdu_end) + 4)) &
			 RX_MSDU_END_4_LAST_MSDU_MASK) >>
			RX_MSDU_END_4_LAST_MSDU_LSB;

		if (last_msdu) {
			qdf_nbuf_set_next(msdu, NULL);
			break;
		}

		next = htt_rx_netbuf_pop(pdev);
		qdf_nbuf_set_next(msdu, next);
		msdu = next;
	}
	*tail_msdu = msdu;

	/*
	 * Don't refill the ring yet.
	 * First, the elements popped here are still in use - it is
	 * not safe to overwrite them until the matching call to
	 * mpdu_desc_list_next.
	 * Second, for efficiency it is preferable to refill the rx ring
	 * with 1 PPDU's worth of rx buffers (something like 32 x 3 buffers),
	 * rather than one MPDU's worth of rx buffers (sth like 3 buffers).
	 * Consequently, we'll rely on the txrx SW to tell us when it is done
	 * pulling all the PPDU's rx buffers out of the rx ring, and then
	 * refill it just once.
	 */
	return msdu_chaining;
}

static
void *htt_rx_mpdu_desc_list_next_ll(htt_pdev_handle pdev, qdf_nbuf_t rx_ind_msg)
{
	int idx = pdev->rx_ring.sw_rd_idx.msdu_desc;
	qdf_nbuf_t netbuf = pdev->rx_ring.buf.netbufs_ring[idx];

	pdev->rx_ring.sw_rd_idx.msdu_desc = pdev->rx_ring.sw_rd_idx.msdu_payld;
	return (void *)htt_rx_desc(netbuf);
}

#else

static inline int
htt_rx_amsdu_pop_ll(htt_pdev_handle pdev,
		    qdf_nbuf_t rx_ind_msg,
		    qdf_nbuf_t *head_msdu, qdf_nbuf_t *tail_msdu,
		    uint32_t *msdu_count)
{
	return 0;
}

static inline
void *htt_rx_mpdu_desc_list_next_ll(htt_pdev_handle pdev, qdf_nbuf_t rx_ind_msg)
{
	return NULL;
}
#endif

/**
 * htt_rx_fill_ring_count() - replenish rx msdu buffer
 * @pdev: Handle (pointer) to HTT pdev.
 *
 * This funciton will replenish the rx buffer to the max number
 * that can be kept in the ring
 *
 * Return: None
 */
void htt_rx_fill_ring_count(htt_pdev_handle pdev)
{
	int num_to_fill;

	num_to_fill = pdev->rx_ring.fill_level -
		qdf_atomic_read(&pdev->rx_ring.fill_cnt);
	htt_rx_ring_fill_n(pdev, num_to_fill /* okay if <= 0 */);
}

int htt_rx_attach(struct htt_pdev_t *pdev)
{
	qdf_dma_addr_t paddr;
	uint32_t ring_elem_size = sizeof(target_paddr_t);

	pdev->rx_ring.size = htt_rx_ring_size(pdev);
	HTT_ASSERT2(QDF_IS_PWR2(pdev->rx_ring.size));
	pdev->rx_ring.size_mask = pdev->rx_ring.size - 1;

	/*
	 * Set the initial value for the level to which the rx ring
	 * should be filled, based on the max throughput and the worst
	 * likely latency for the host to fill the rx ring.
	 * In theory, this fill level can be dynamically adjusted from
	 * the initial value set here to reflect the actual host latency
	 * rather than a conservative assumption.
	 */
	pdev->rx_ring.fill_level = htt_rx_ring_fill_level(pdev);

	if (pdev->cfg.is_full_reorder_offload) {
		if (htt_rx_hash_init(pdev))
			goto fail1;

		/* allocate the target index */
		pdev->rx_ring.target_idx.vaddr =
			 qdf_mem_alloc_consistent(pdev->osdev, pdev->osdev->dev,
						  sizeof(uint32_t), &paddr);

		if (!pdev->rx_ring.target_idx.vaddr)
			goto fail2;

		pdev->rx_ring.target_idx.paddr = paddr;
		*pdev->rx_ring.target_idx.vaddr = 0;
	} else {
		pdev->rx_ring.buf.netbufs_ring =
			qdf_mem_malloc(pdev->rx_ring.size * sizeof(qdf_nbuf_t));
		if (!pdev->rx_ring.buf.netbufs_ring)
			goto fail1;

		pdev->rx_ring.sw_rd_idx.msdu_payld = 0;
		pdev->rx_ring.sw_rd_idx.msdu_desc = 0;
	}

	pdev->rx_ring.buf.paddrs_ring =
		qdf_mem_alloc_consistent(
			pdev->osdev, pdev->osdev->dev,
			 pdev->rx_ring.size * ring_elem_size,
			 &paddr);
	if (!pdev->rx_ring.buf.paddrs_ring)
		goto fail3;

	pdev->rx_ring.base_paddr = paddr;
	pdev->rx_ring.alloc_idx.vaddr =
		 qdf_mem_alloc_consistent(
			pdev->osdev, pdev->osdev->dev,
			 sizeof(uint32_t), &paddr);

	if (!pdev->rx_ring.alloc_idx.vaddr)
		goto fail4;

	pdev->rx_ring.alloc_idx.paddr = paddr;
	*pdev->rx_ring.alloc_idx.vaddr = 0;

	if (htt_rx_buff_pool_init(pdev))
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO_LOW,
			  "HTT: pre allocated packet pool alloc failed");

	/*
	 * Initialize the Rx refill reference counter to be one so that
	 * only one thread is allowed to refill the Rx ring.
	 */
	qdf_atomic_init(&pdev->rx_ring.refill_ref_cnt);
	qdf_atomic_inc(&pdev->rx_ring.refill_ref_cnt);

	/* Initialize the refill_lock and debt (for rx-parallelization) */
	qdf_spinlock_create(&pdev->rx_ring.refill_lock);
	qdf_atomic_init(&pdev->rx_ring.refill_debt);

	/* Initialize the Rx refill retry timer */
	qdf_timer_init(pdev->osdev,
		       &pdev->rx_ring.refill_retry_timer,
		       htt_rx_ring_refill_retry, (void *)pdev,
		       QDF_TIMER_TYPE_SW);

	qdf_atomic_init(&pdev->rx_ring.fill_cnt);
	pdev->rx_ring.pop_fail_cnt = 0;
#ifdef DEBUG_DMA_DONE
	pdev->rx_ring.dbg_ring_idx = 0;
	pdev->rx_ring.dbg_refill_cnt = 0;
	pdev->rx_ring.dbg_sync_success = 0;
#endif
#ifdef HTT_RX_RESTORE
	pdev->rx_ring.rx_reset = 0;
	pdev->rx_ring.htt_rx_restore = 0;
#endif
	htt_rx_dbg_rxbuf_init(pdev);
	htt_rx_ring_fill_n(pdev, pdev->rx_ring.fill_level);

	if (pdev->cfg.is_full_reorder_offload) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO_LOW,
			  "HTT: full reorder offload enabled");
		htt_rx_amsdu_pop = htt_rx_amsdu_rx_in_order_pop_ll;
		htt_rx_frag_pop = htt_rx_amsdu_rx_in_order_pop_ll;
		htt_rx_mpdu_desc_list_next =
			 htt_rx_in_ord_mpdu_desc_list_next_ll;
	} else {
		htt_rx_amsdu_pop = htt_rx_amsdu_pop_ll;
		htt_rx_frag_pop = htt_rx_amsdu_pop_ll;
		htt_rx_mpdu_desc_list_next = htt_rx_mpdu_desc_list_next_ll;
	}

	if (cds_get_conparam() == QDF_GLOBAL_MONITOR_MODE)
		htt_rx_amsdu_pop = htt_rx_mon_amsdu_rx_in_order_pop_ll;

	htt_rx_offload_msdu_cnt = htt_rx_offload_msdu_cnt_ll;
	htt_rx_offload_msdu_pop = htt_rx_offload_msdu_pop_ll;
	htt_rx_mpdu_desc_retry = htt_rx_mpdu_desc_retry_ll;
	htt_rx_mpdu_desc_seq_num = htt_rx_mpdu_desc_seq_num_ll;
	htt_rx_mpdu_desc_pn = htt_rx_mpdu_desc_pn_ll;
	htt_rx_mpdu_desc_tid = htt_rx_mpdu_desc_tid_ll;
	htt_rx_msdu_desc_completes_mpdu = htt_rx_msdu_desc_completes_mpdu_ll;
	htt_rx_msdu_first_msdu_flag = htt_rx_msdu_first_msdu_flag_ll;
	htt_rx_msdu_has_wlan_mcast_flag = htt_rx_msdu_has_wlan_mcast_flag_ll;
	htt_rx_msdu_is_wlan_mcast = htt_rx_msdu_is_wlan_mcast_ll;
	htt_rx_msdu_is_frag = htt_rx_msdu_is_frag_ll;
	htt_rx_msdu_desc_retrieve = htt_rx_msdu_desc_retrieve_ll;
	htt_rx_mpdu_is_encrypted = htt_rx_mpdu_is_encrypted_ll;
	htt_rx_msdu_desc_key_id = htt_rx_msdu_desc_key_id_ll;
	htt_rx_msdu_chan_info_present = htt_rx_msdu_chan_info_present_ll;
	htt_rx_msdu_center_freq = htt_rx_msdu_center_freq_ll;

	return 0;               /* success */

fail4:
	qdf_mem_free_consistent(pdev->osdev, pdev->osdev->dev,
				pdev->rx_ring.size * sizeof(target_paddr_t),
				pdev->rx_ring.buf.paddrs_ring,
				pdev->rx_ring.base_paddr,
				qdf_get_dma_mem_context((&pdev->rx_ring.buf),
							memctx));

fail3:
	if (pdev->cfg.is_full_reorder_offload)
		qdf_mem_free_consistent(pdev->osdev, pdev->osdev->dev,
					sizeof(uint32_t),
					pdev->rx_ring.target_idx.vaddr,
					pdev->rx_ring.target_idx.paddr,
					qdf_get_dma_mem_context((&pdev->
								 rx_ring.
								 target_idx),
								 memctx));
	else
		qdf_mem_free(pdev->rx_ring.buf.netbufs_ring);

fail2:
	if (pdev->cfg.is_full_reorder_offload)
		htt_rx_hash_deinit(pdev);

fail1:
	return 1;               /* failure */
}

void htt_rx_detach(struct htt_pdev_t *pdev)
{
	bool ipa_smmu = false;
	qdf_nbuf_t nbuf;

	qdf_timer_stop(&pdev->rx_ring.refill_retry_timer);
	qdf_timer_free(&pdev->rx_ring.refill_retry_timer);
	htt_rx_dbg_rxbuf_deinit(pdev);

	ipa_smmu = htt_rx_ring_smmu_mapped(pdev);

	if (pdev->cfg.is_full_reorder_offload) {
		qdf_mem_free_consistent(pdev->osdev, pdev->osdev->dev,
					sizeof(uint32_t),
					pdev->rx_ring.target_idx.vaddr,
					pdev->rx_ring.target_idx.paddr,
					qdf_get_dma_mem_context((&pdev->
								 rx_ring.
								 target_idx),
								 memctx));
		htt_rx_hash_deinit(pdev);
	} else {
		int sw_rd_idx = pdev->rx_ring.sw_rd_idx.msdu_payld;
		qdf_mem_info_t mem_map_table = {0};

		while (sw_rd_idx != *pdev->rx_ring.alloc_idx.vaddr) {
			nbuf = pdev->rx_ring.buf.netbufs_ring[sw_rd_idx];
			if (ipa_smmu) {
				if (qdf_unlikely(
					!qdf_nbuf_is_rx_ipa_smmu_map(nbuf))) {
					qdf_err("smmu not mapped, nbuf: %pK",
						nbuf);
					qdf_assert_always(0);
				}
				qdf_nbuf_set_rx_ipa_smmu_map(nbuf, false);
				qdf_update_mem_map_table(pdev->osdev,
					&mem_map_table,
					QDF_NBUF_CB_PADDR(nbuf),
					HTT_RX_BUF_SIZE);
				qdf_assert_always(
					!cds_smmu_map_unmap(false, 1,
							    &mem_map_table));
			}
#ifdef DEBUG_DMA_DONE
			qdf_nbuf_unmap(pdev->osdev, nbuf,
				       QDF_DMA_BIDIRECTIONAL);
#else
			qdf_nbuf_unmap(pdev->osdev, nbuf,
				       QDF_DMA_FROM_DEVICE);
#endif
			qdf_nbuf_free(nbuf);
			sw_rd_idx++;
			sw_rd_idx &= pdev->rx_ring.size_mask;
		}
		qdf_mem_free(pdev->rx_ring.buf.netbufs_ring);
	}

	htt_rx_buff_pool_deinit(pdev);

	qdf_mem_free_consistent(pdev->osdev, pdev->osdev->dev,
				sizeof(uint32_t),
				pdev->rx_ring.alloc_idx.vaddr,
				pdev->rx_ring.alloc_idx.paddr,
				qdf_get_dma_mem_context((&pdev->rx_ring.
							 alloc_idx),
							 memctx));

	qdf_mem_free_consistent(pdev->osdev, pdev->osdev->dev,
				pdev->rx_ring.size * sizeof(target_paddr_t),
				pdev->rx_ring.buf.paddrs_ring,
				pdev->rx_ring.base_paddr,
				qdf_get_dma_mem_context((&pdev->rx_ring.buf),
							memctx));

	/* destroy the rx-parallelization refill spinlock */
	qdf_spinlock_destroy(&pdev->rx_ring.refill_lock);
}

static QDF_STATUS htt_rx_hash_smmu_map(bool map, struct htt_pdev_t *pdev)
{
	uint32_t i;
	struct htt_rx_hash_entry *hash_entry;
	struct htt_rx_hash_bucket **hash_table;
	struct htt_list_node *list_iter = NULL;
	qdf_mem_info_t mem_map_table = {0};
	qdf_nbuf_t nbuf;
	int ret;

	qdf_spin_lock_bh(&pdev->rx_ring.rx_hash_lock);
	hash_table = pdev->rx_ring.hash_table;

	for (i = 0; i < RX_NUM_HASH_BUCKETS; i++) {
		/* Free the hash entries in hash bucket i */
		list_iter = hash_table[i]->listhead.next;
		while (list_iter != &hash_table[i]->listhead) {
			hash_entry =
				(struct htt_rx_hash_entry *)((char *)list_iter -
							     pdev->rx_ring.
							     listnode_offset);
			nbuf = hash_entry->netbuf;
			if (nbuf) {
				if (qdf_unlikely(map ==
					qdf_nbuf_is_rx_ipa_smmu_map(nbuf))) {
					qdf_err("map/unmap err:%d, nbuf:%pK",
						map, nbuf);
					list_iter = list_iter->next;
					continue;
				}
				qdf_nbuf_set_rx_ipa_smmu_map(nbuf, map);
				qdf_update_mem_map_table(pdev->osdev,
						&mem_map_table,
						QDF_NBUF_CB_PADDR(nbuf),
						HTT_RX_BUF_SIZE);
				ret = cds_smmu_map_unmap(map, 1,
							 &mem_map_table);
				if (ret) {
					qdf_nbuf_set_rx_ipa_smmu_map(nbuf,
								     !map);
					qdf_err("map: %d failure, nbuf: %pK",
						map, nbuf);
					qdf_spin_unlock_bh(
						&pdev->rx_ring.rx_hash_lock);
					return QDF_STATUS_E_FAILURE;
				}
			}
			list_iter = list_iter->next;
		}
	}

	pdev->rx_ring.smmu_map = map;
	qdf_spin_unlock_bh(&pdev->rx_ring.rx_hash_lock);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS htt_rx_update_smmu_map(struct htt_pdev_t *pdev, bool map)
{
	QDF_STATUS status;

	if (!pdev->rx_ring.hash_table)
		return QDF_STATUS_SUCCESS;

	if (!qdf_mem_smmu_s1_enabled(pdev->osdev) || !pdev->is_ipa_uc_enabled)
		return QDF_STATUS_SUCCESS;

	qdf_spin_lock_bh(&pdev->rx_ring.refill_lock);
	status = htt_rx_hash_smmu_map(map, pdev);
	qdf_spin_unlock_bh(&pdev->rx_ring.refill_lock);

	return status;
}
