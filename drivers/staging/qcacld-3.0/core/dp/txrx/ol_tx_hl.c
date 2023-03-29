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

#include <qdf_atomic.h>         /* qdf_atomic_inc, etc. */
#include <qdf_lock.h>           /* qdf_os_spinlock */
#include <qdf_time.h>           /* qdf_system_ticks, etc. */
#include <qdf_nbuf.h>           /* qdf_nbuf_t */
#include <qdf_net_types.h>      /* QDF_NBUF_TX_EXT_TID_INVALID */

#include "queue.h"          /* TAILQ */
#ifdef QCA_COMPUTE_TX_DELAY
#include <enet.h>               /* ethernet_hdr_t, etc. */
#include <ipv6_defs.h>          /* ipv6_traffic_class */
#endif

#include <ol_txrx_api.h>        /* ol_txrx_vdev_handle, etc. */
#include <ol_htt_tx_api.h>      /* htt_tx_compl_desc_id */
#include <ol_txrx_htt_api.h>    /* htt_tx_status */

#include <ol_ctrl_txrx_api.h>
#include <cdp_txrx_tx_delay.h>
#include <ol_txrx_types.h>      /* ol_txrx_vdev_t, etc */
#include <ol_tx_desc.h>         /* ol_tx_desc_find, ol_tx_desc_frame_free */
#ifdef QCA_COMPUTE_TX_DELAY
#include <ol_tx_classify.h>     /* ol_tx_dest_addr_find */
#endif
#include <ol_txrx_internal.h>   /* OL_TX_DESC_NO_REFS, etc. */
#include <ol_osif_txrx_api.h>
#include <ol_tx.h>              /* ol_tx_reinject */
#include <ol_tx_send.h>

#include <ol_cfg.h>             /* ol_cfg_is_high_latency */
#include <ol_tx_sched.h>
#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
#include <ol_txrx_encap.h>      /* OL_TX_RESTORE_HDR, etc */
#endif
#include <ol_tx_queue.h>
#include <ol_txrx.h>
#include <pktlog_ac_fmt.h>
#include <cdp_txrx_handle.h>
#include <wlan_reg_services_api.h>
#include "qdf_hrtimer.h"

/* High/Low tx resource count in percentage */
/* Set default high threashold to 15% */
#ifndef TX_RESOURCE_HIGH_TH_IN_PER
#define TX_RESOURCE_HIGH_TH_IN_PER 15
#endif

/* Set default low threshold to 5% */
#ifndef TX_RESOURCE_LOW_TH_IN_PER
#define TX_RESOURCE_LOW_TH_IN_PER 5
#endif

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
static u16 ol_txrx_tx_desc_alloc_table[TXRX_FC_MAX] = {
	[TXRX_FC_5GH_80M_2x2] = 2000,
	[TXRX_FC_2GH_40M_2x2] = 800,
};
#endif /* QCA_HL_NETDEV_FLOW_CONTROL */

/* tx filtering is handled within the target FW */
#define TX_FILTER_CHECK(tx_msdu_info) 0 /* don't filter */

u_int16_t
ol_tx_desc_pool_size_hl(struct cdp_cfg *ctrl_pdev)
{
	uint16_t desc_pool_size;
	uint16_t steady_state_tx_lifetime_ms;
	uint16_t safety_factor;

	/*
	 * Steady-state tx latency:
	 *     roughly 1-2 ms flight time
	 *   + roughly 1-2 ms prep time,
	 *   + roughly 1-2 ms target->host notification time.
	 * = roughly 6 ms total
	 * Thus, steady state number of frames =
	 * steady state max throughput / frame size * tx latency, e.g.
	 * 1 Gbps / 1500 bytes * 6 ms = 500
	 *
	 */
	steady_state_tx_lifetime_ms = 6;

	safety_factor = 8;

	desc_pool_size =
		ol_cfg_max_thruput_mbps(ctrl_pdev) *
		1000 /* 1e6 bps/mbps / 1e3 ms per sec = 1000 */ /
		(8 * OL_TX_AVG_FRM_BYTES) *
		steady_state_tx_lifetime_ms *
		safety_factor;

	/* minimum */
	if (desc_pool_size < OL_TX_DESC_POOL_SIZE_MIN_HL)
		desc_pool_size = OL_TX_DESC_POOL_SIZE_MIN_HL;

	/* maximum */
	if (desc_pool_size > OL_TX_DESC_POOL_SIZE_MAX_HL)
		desc_pool_size = OL_TX_DESC_POOL_SIZE_MAX_HL;

	return desc_pool_size;
}

#ifdef CONFIG_TX_DESC_HI_PRIO_RESERVE

/**
 * ol_tx_hl_desc_alloc() - Allocate and initialize a tx descriptor
 *                        for a HL system.
 * @pdev: the data physical device sending the data
 * @vdev: the virtual device sending the data
 * @msdu: the tx frame
 * @msdu_info: the tx meta data
 *
 * Return: the tx decriptor
 */
static inline
struct ol_tx_desc_t *ol_tx_hl_desc_alloc(struct ol_txrx_pdev_t *pdev,
					 struct ol_txrx_vdev_t *vdev,
					 qdf_nbuf_t msdu,
					 struct ol_txrx_msdu_info_t *msdu_info)
{
	struct ol_tx_desc_t *tx_desc = NULL;

	if (qdf_atomic_read(&pdev->tx_queue.rsrc_cnt) >
	    TXRX_HL_TX_DESC_HI_PRIO_RESERVED) {
		tx_desc = ol_tx_desc_hl(pdev, vdev, msdu, msdu_info);
	} else if (qdf_nbuf_is_ipv4_pkt(msdu) == true) {
		if ((QDF_NBUF_CB_GET_PACKET_TYPE(msdu) ==
		    QDF_NBUF_CB_PACKET_TYPE_DHCP) ||
		    (QDF_NBUF_CB_GET_PACKET_TYPE(msdu) ==
		    QDF_NBUF_CB_PACKET_TYPE_EAPOL)) {
			tx_desc = ol_tx_desc_hl(pdev, vdev, msdu, msdu_info);
			ol_txrx_info("Got tx desc from resv pool\n");
		}
	}
	return tx_desc;
}

#elif defined(QCA_HL_NETDEV_FLOW_CONTROL)
bool ol_tx_desc_is_high_prio(qdf_nbuf_t msdu)
{
	enum qdf_proto_subtype proto_subtype;
	bool high_prio = false;

	if (qdf_nbuf_is_ipv4_pkt(msdu) == true) {
		if ((QDF_NBUF_CB_GET_PACKET_TYPE(msdu) ==
		    QDF_NBUF_CB_PACKET_TYPE_DHCP) ||
		    (QDF_NBUF_CB_GET_PACKET_TYPE(msdu) ==
		    QDF_NBUF_CB_PACKET_TYPE_EAPOL))
			high_prio = true;
	} else if (QDF_NBUF_CB_GET_PACKET_TYPE(msdu) ==
		   QDF_NBUF_CB_PACKET_TYPE_ARP) {
		high_prio = true;
	} else if ((QDF_NBUF_CB_GET_PACKET_TYPE(msdu) ==
		   QDF_NBUF_CB_PACKET_TYPE_ICMPv6)) {
		proto_subtype = qdf_nbuf_get_icmpv6_subtype(msdu);
		switch (proto_subtype) {
		case QDF_PROTO_ICMPV6_NA:
		case QDF_PROTO_ICMPV6_NS:
			high_prio = true;
		default:
			high_prio = false;
		}
	}
	return high_prio;
}

static inline
struct ol_tx_desc_t *ol_tx_hl_desc_alloc(struct ol_txrx_pdev_t *pdev,
					 struct ol_txrx_vdev_t *vdev,
					 qdf_nbuf_t msdu,
					 struct ol_txrx_msdu_info_t *msdu_info)
{
	struct ol_tx_desc_t *tx_desc =
			ol_tx_desc_hl(pdev, vdev, msdu, msdu_info);

	if (!tx_desc)
		return NULL;

	qdf_spin_lock_bh(&pdev->tx_mutex);
	/* return if TX flow control disabled */
	if (vdev->tx_desc_limit == 0) {
		qdf_spin_unlock_bh(&pdev->tx_mutex);
		return tx_desc;
	}

	if (!qdf_atomic_read(&vdev->os_q_paused) &&
	    (qdf_atomic_read(&vdev->tx_desc_count) >= vdev->queue_stop_th)) {
		/*
		 * Pause normal priority
		 * netdev queues if tx desc limit crosses
		 */
		pdev->pause_cb(vdev->vdev_id,
			       WLAN_STOP_NON_PRIORITY_QUEUE,
			       WLAN_DATA_FLOW_CONTROL);
		qdf_atomic_set(&vdev->os_q_paused, 1);
	} else if (ol_tx_desc_is_high_prio(msdu) && !vdev->prio_q_paused &&
		   (qdf_atomic_read(&vdev->tx_desc_count)
		   == vdev->tx_desc_limit)) {
		/* Pause high priority queue */
		pdev->pause_cb(vdev->vdev_id,
			       WLAN_NETIF_PRIORITY_QUEUE_OFF,
			       WLAN_DATA_FLOW_CONTROL_PRIORITY);
		vdev->prio_q_paused = 1;
	}
	qdf_spin_unlock_bh(&pdev->tx_mutex);

	return tx_desc;
}

#else

static inline
struct ol_tx_desc_t *ol_tx_hl_desc_alloc(struct ol_txrx_pdev_t *pdev,
					 struct ol_txrx_vdev_t *vdev,
					 qdf_nbuf_t msdu,
					 struct ol_txrx_msdu_info_t *msdu_info)
{
	struct ol_tx_desc_t *tx_desc = NULL;

	tx_desc = ol_tx_desc_hl(pdev, vdev, msdu, msdu_info);
	return tx_desc;
}
#endif

static inline uint16_t
ol_txrx_rsrc_threshold_lo(int desc_pool_size)
{
	int threshold_low;

	/* always maintain a 5% margin of unallocated descriptors */
	threshold_low = ((TX_RESOURCE_LOW_TH_IN_PER) *
			 desc_pool_size) / 100;

	return threshold_low;
}

static inline uint16_t
ol_txrx_rsrc_threshold_hi(int desc_pool_size)
{
	int threshold_high;
	/* when freeing up descriptors, keep going until
	 * there's a 15% margin
	 */
	threshold_high = ((TX_RESOURCE_HIGH_TH_IN_PER) *
			  desc_pool_size) / 100;

	return threshold_high;
}

void ol_tx_init_pdev(ol_txrx_pdev_handle pdev)
{
	uint16_t desc_pool_size, i;

	desc_pool_size = ol_tx_desc_pool_size_hl(pdev->ctrl_pdev);

	qdf_atomic_init(&pdev->tx_queue.rsrc_cnt);
	qdf_atomic_add(desc_pool_size, &pdev->tx_queue.rsrc_cnt);

	pdev->tx_queue.rsrc_threshold_lo =
		ol_txrx_rsrc_threshold_lo(desc_pool_size);
	pdev->tx_queue.rsrc_threshold_hi =
		ol_txrx_rsrc_threshold_hi(desc_pool_size);

	for (i = 0 ; i < OL_TX_MAX_TXQ_GROUPS; i++)
		qdf_atomic_init(&pdev->txq_grps[i].credit);

	ol_tx_target_credit_init(pdev, desc_pool_size);
}

#ifdef QCA_SUPPORT_SW_TXRX_ENCAP
static inline int ol_tx_encap_wrapper(struct ol_txrx_pdev_t *pdev,
				      ol_txrx_vdev_handle vdev,
				      struct ol_tx_desc_t *tx_desc,
				      qdf_nbuf_t msdu,
				      struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	if (OL_TX_ENCAP(vdev, tx_desc, msdu, tx_msdu_info) != A_OK) {
		qdf_atomic_inc(&pdev->tx_queue.rsrc_cnt);
		ol_tx_desc_frame_free_nonstd(pdev, tx_desc, 1);
		if (tx_msdu_info->peer) {
			/* remove the peer reference added above */
			ol_txrx_peer_release_ref(tx_msdu_info->peer,
						 PEER_DEBUG_ID_OL_INTERNAL);
		}
		return -EINVAL;
	}

	return 0;
}
#else
static inline int ol_tx_encap_wrapper(struct ol_txrx_pdev_t *pdev,
				      ol_txrx_vdev_handle vdev,
				      struct ol_tx_desc_t *tx_desc,
				      qdf_nbuf_t msdu,
				      struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	/* no-op */
	return 0;
}
#endif

/**
 * parse_ocb_tx_header() - Function to check for OCB
 * @msdu:   Pointer to OS packet (qdf_nbuf_t)
 * @tx_ctrl: TX control header on a packet and extract it if present
 *
 * Return: true if ocb parsing is successful
 */
#ifdef WLAN_FEATURE_DSRC
#define OCB_HEADER_VERSION     1
static bool parse_ocb_tx_header(qdf_nbuf_t msdu,
				struct ocb_tx_ctrl_hdr_t *tx_ctrl)
{
	qdf_ether_header_t *eth_hdr_p;
	struct ocb_tx_ctrl_hdr_t *tx_ctrl_hdr;

	/* Check if TX control header is present */
	eth_hdr_p = (qdf_ether_header_t *)qdf_nbuf_data(msdu);
	if (eth_hdr_p->ether_type != QDF_SWAP_U16(ETHERTYPE_OCB_TX))
		/* TX control header is not present. Nothing to do.. */
		return true;

	/* Remove the ethernet header */
	qdf_nbuf_pull_head(msdu, sizeof(qdf_ether_header_t));

	/* Parse the TX control header */
	tx_ctrl_hdr = (struct ocb_tx_ctrl_hdr_t *)qdf_nbuf_data(msdu);

	if (tx_ctrl_hdr->version == OCB_HEADER_VERSION) {
		if (tx_ctrl)
			qdf_mem_copy(tx_ctrl, tx_ctrl_hdr,
				     sizeof(*tx_ctrl_hdr));
	} else {
		/* The TX control header is invalid. */
		return false;
	}

	/* Remove the TX control header */
	qdf_nbuf_pull_head(msdu, tx_ctrl_hdr->length);
	return true;
}
#else
static bool parse_ocb_tx_header(qdf_nbuf_t msdu,
				struct ocb_tx_ctrl_hdr_t *tx_ctrl)
{
	return true;
}
#endif

/**
 * ol_txrx_mgmt_tx_desc_alloc() - Allocate and initialize a tx descriptor
 *				 for management frame
 * @pdev: the data physical device sending the data
 * @vdev: the virtual device sending the data
 * @tx_mgmt_frm: the tx management frame
 * @tx_msdu_info: the tx meta data
 *
 * Return: the tx decriptor
 */
struct ol_tx_desc_t *
ol_txrx_mgmt_tx_desc_alloc(
	struct ol_txrx_pdev_t *pdev,
	struct ol_txrx_vdev_t *vdev,
	qdf_nbuf_t tx_mgmt_frm,
	struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	struct ol_tx_desc_t *tx_desc;

	tx_msdu_info->htt.action.tx_comp_req = 1;
	tx_desc = ol_tx_desc_hl(pdev, vdev, tx_mgmt_frm, tx_msdu_info);
	return tx_desc;
}

/**
 * ol_txrx_mgmt_send_frame() - send a management frame
 * @vdev: virtual device sending the frame
 * @tx_desc: tx desc
 * @tx_mgmt_frm: management frame to send
 * @tx_msdu_info: the tx meta data
 * @chanfreq: download change frequency
 *
 * Return:
 *      0 -> the frame is accepted for transmission, -OR-
 *      1 -> the frame was not accepted
 */
int ol_txrx_mgmt_send_frame(
	struct ol_txrx_vdev_t *vdev,
	struct ol_tx_desc_t *tx_desc,
	qdf_nbuf_t tx_mgmt_frm,
	struct ol_txrx_msdu_info_t *tx_msdu_info,
	uint16_t chanfreq)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	struct ol_tx_frms_queue_t *txq;
	int status = 1;

	/*
	 * 1.  Look up the peer and queue the frame in the peer's mgmt queue.
	 * 2.  Invoke the download scheduler.
	 */
	txq = ol_tx_classify_mgmt(vdev, tx_desc, tx_mgmt_frm, tx_msdu_info);
	if (!txq) {
		/* TXRX_STATS_MSDU_LIST_INCR(vdev->pdev, tx.dropped.no_txq,
		 *			     msdu);
		 */
		qdf_atomic_inc(&pdev->tx_queue.rsrc_cnt);
		ol_tx_desc_frame_free_nonstd(vdev->pdev, tx_desc,
					     1 /* error */);
		goto out; /* can't accept the tx mgmt frame */
	}
	/* Initialize the HTT tx desc l2 header offset field.
	 * Even though tx encap does not apply to mgmt frames,
	 * htt_tx_desc_mpdu_header still needs to be called,
	 * to specifiy that there was no L2 header added by tx encap,
	 * so the frame's length does not need to be adjusted to account for
	 * an added L2 header.
	 */
	htt_tx_desc_mpdu_header(tx_desc->htt_tx_desc, 0);
	if (qdf_unlikely(htt_tx_desc_init(
			pdev->htt_pdev, tx_desc->htt_tx_desc,
			tx_desc->htt_tx_desc_paddr,
			ol_tx_desc_id(pdev, tx_desc),
			tx_mgmt_frm,
			&tx_msdu_info->htt, &tx_msdu_info->tso_info, NULL, 0)))
		goto out;
	htt_tx_desc_display(tx_desc->htt_tx_desc);
	htt_tx_desc_set_chanfreq(tx_desc->htt_tx_desc, chanfreq);

	ol_tx_enqueue(vdev->pdev, txq, tx_desc, tx_msdu_info);
	ol_tx_sched(vdev->pdev);
	status = 0;
out:
	if (tx_msdu_info->peer) {
		/* remove the peer reference added above */
		ol_txrx_peer_release_ref(tx_msdu_info->peer,
					 PEER_DEBUG_ID_OL_INTERNAL);
	}

	return status;
}

/**
 * ol_tx_hl_base() - send tx frames for a HL system.
 * @vdev: the virtual device sending the data
 * @tx_spec: indicate what non-standard transmission actions to apply
 * @msdu_list: the tx frames to send
 * @tx_comp_req: tx completion req
 * @call_sched: will schedule the tx if true
 *
 * Return: NULL if all MSDUs are accepted
 */
static inline qdf_nbuf_t
ol_tx_hl_base(
	ol_txrx_vdev_handle vdev,
	enum ol_tx_spec tx_spec,
	qdf_nbuf_t msdu_list,
	int tx_comp_req,
	bool call_sched)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	qdf_nbuf_t msdu = msdu_list;
	struct ol_txrx_msdu_info_t tx_msdu_info;
	struct ocb_tx_ctrl_hdr_t tx_ctrl;
	htt_pdev_handle htt_pdev = pdev->htt_pdev;

	tx_msdu_info.tso_info.is_tso = 0;

	/*
	 * The msdu_list variable could be used instead of the msdu var,
	 * but just to clarify which operations are done on a single MSDU
	 * vs. a list of MSDUs, use a distinct variable for single MSDUs
	 * within the list.
	 */
	while (msdu) {
		qdf_nbuf_t next;
		struct ol_tx_frms_queue_t *txq;
		struct ol_tx_desc_t *tx_desc = NULL;

		qdf_mem_zero(&tx_ctrl, sizeof(tx_ctrl));
		tx_msdu_info.peer = NULL;
		/*
		 * The netbuf will get stored into a (peer-TID) tx queue list
		 * inside the ol_tx_classify_store function or else dropped,
		 * so store the next pointer immediately.
		 */
		next = qdf_nbuf_next(msdu);

		tx_desc = ol_tx_hl_desc_alloc(pdev, vdev, msdu, &tx_msdu_info);

		if (!tx_desc) {
			/*
			 * If we're out of tx descs, there's no need to try
			 * to allocate tx descs for the remaining MSDUs.
			 */
			TXRX_STATS_MSDU_LIST_INCR(pdev, tx.dropped.host_reject,
						  msdu);
			return msdu; /* the list of unaccepted MSDUs */
		}

		/* OL_TXRX_PROT_AN_LOG(pdev->prot_an_tx_sent, msdu);*/

		qdf_dp_trace_log_pkt(vdev->vdev_id, msdu, QDF_TX,
				     QDF_TRACE_DEFAULT_PDEV_ID);
		DPTRACE(qdf_dp_trace_data_pkt(msdu, QDF_TRACE_DEFAULT_PDEV_ID,
					      QDF_DP_TRACE_TX_PACKET_RECORD,
					      tx_desc->id, QDF_TX));

		if (tx_spec != OL_TX_SPEC_STD) {
#if defined(FEATURE_WLAN_TDLS)
			if (tx_spec & OL_TX_SPEC_NO_FREE) {
				tx_desc->pkt_type = OL_TX_FRM_NO_FREE;
			} else if (tx_spec & OL_TX_SPEC_TSO) {
#else
				if (tx_spec & OL_TX_SPEC_TSO) {
#endif
					tx_desc->pkt_type = OL_TX_FRM_TSO;
				}
				if (ol_txrx_tx_is_raw(tx_spec)) {
					/* CHECK THIS: does this need
					 * to happen after htt_tx_desc_init?
					 */
					/* different types of raw frames */
					u_int8_t sub_type =
						ol_txrx_tx_raw_subtype(
								tx_spec);
					htt_tx_desc_type(htt_pdev,
							 tx_desc->htt_tx_desc,
							 htt_pkt_type_raw,
							 sub_type);
				}
			}

			tx_msdu_info.htt.info.ext_tid = qdf_nbuf_get_tid(msdu);
			tx_msdu_info.htt.info.vdev_id = vdev->vdev_id;
			tx_msdu_info.htt.info.frame_type = htt_frm_type_data;
			tx_msdu_info.htt.info.l2_hdr_type = pdev->htt_pkt_type;

			if (QDF_NBUF_CB_TX_EXTRA_FRAG_FLAGS_NOTIFY_COMP(msdu)
									== 1) {
				tx_msdu_info.htt.action.tx_comp_req = 1;
				tx_desc->pkt_type = OL_TX_FRM_NO_FREE;
			} else {
				tx_msdu_info.htt.action.tx_comp_req =
								tx_comp_req;
			}

			/* If the vdev is in OCB mode,
			 * parse the tx control header.
			 */
			if (vdev->opmode == wlan_op_mode_ocb) {
				if (!parse_ocb_tx_header(msdu, &tx_ctrl)) {
					/* There was an error parsing
					 * the header.Skip this packet.
					 */
					goto MSDU_LOOP_BOTTOM;
				}
			}

			txq = ol_tx_classify(vdev, tx_desc, msdu,
					     &tx_msdu_info);

			/* initialize the HW tx descriptor */
			htt_tx_desc_init(
					pdev->htt_pdev, tx_desc->htt_tx_desc,
					tx_desc->htt_tx_desc_paddr,
					ol_tx_desc_id(pdev, tx_desc),
					msdu,
					&tx_msdu_info.htt,
					&tx_msdu_info.tso_info,
					&tx_ctrl,
					vdev->opmode == wlan_op_mode_ocb);

			if ((!txq) || TX_FILTER_CHECK(&tx_msdu_info)) {
				/* drop this frame,
				 * but try sending subsequent frames
				 */
				/* TXRX_STATS_MSDU_LIST_INCR(pdev,
				 * tx.dropped.no_txq, msdu);
				 */
				qdf_atomic_inc(&pdev->tx_queue.rsrc_cnt);
				ol_tx_desc_frame_free_nonstd(pdev, tx_desc, 1);
				if (tx_msdu_info.peer) {
					/* remove the peer reference
					 * added above
					 */
					ol_txrx_peer_release_ref(
						tx_msdu_info.peer,
						PEER_DEBUG_ID_OL_INTERNAL);
				}
				goto MSDU_LOOP_BOTTOM;
			}

			if (tx_msdu_info.peer) {
				/*
				 * If the state is not associated then drop all
				 * the data packets received for that peer
				 */
				if (tx_msdu_info.peer->state ==
						OL_TXRX_PEER_STATE_DISC) {
					qdf_atomic_inc(
						&pdev->tx_queue.rsrc_cnt);
					ol_tx_desc_frame_free_nonstd(pdev,
								     tx_desc,
								     1);
					ol_txrx_peer_release_ref(
						tx_msdu_info.peer,
						PEER_DEBUG_ID_OL_INTERNAL);
					msdu = next;
					continue;
				} else if (tx_msdu_info.peer->state !=
						OL_TXRX_PEER_STATE_AUTH) {
					if (tx_msdu_info.htt.info.ethertype !=
						ETHERTYPE_PAE &&
						tx_msdu_info.htt.info.ethertype
							!= ETHERTYPE_WAI) {
						qdf_atomic_inc(
							&pdev->tx_queue.
								rsrc_cnt);
						ol_tx_desc_frame_free_nonstd(
								pdev,
								tx_desc, 1);
						ol_txrx_peer_release_ref(
						 tx_msdu_info.peer,
						 PEER_DEBUG_ID_OL_INTERNAL);
						msdu = next;
						continue;
					}
				}
			}
			/*
			 * Initialize the HTT tx desc l2 header offset field.
			 * htt_tx_desc_mpdu_header  needs to be called to
			 * make sure, the l2 header size is initialized
			 * correctly to handle cases where TX ENCAP is disabled
			 * or Tx Encap fails to perform Encap
			 */
			htt_tx_desc_mpdu_header(tx_desc->htt_tx_desc, 0);

			/*
			 * Note: when the driver is built without support for
			 * SW tx encap,the following macro is a no-op.
			 * When the driver is built with support for SW tx
			 * encap, it performs encap, and if an error is
			 * encountered, jumps to the MSDU_LOOP_BOTTOM label.
			 */
			if (ol_tx_encap_wrapper(pdev, vdev, tx_desc, msdu,
						&tx_msdu_info))
				goto MSDU_LOOP_BOTTOM;

			/*
			 * If debug display is enabled, show the meta-data
			 * being downloaded to the target via the
			 * HTT tx descriptor.
			 */
			htt_tx_desc_display(tx_desc->htt_tx_desc);

			ol_tx_enqueue(pdev, txq, tx_desc, &tx_msdu_info);
			if (tx_msdu_info.peer) {
				OL_TX_PEER_STATS_UPDATE(tx_msdu_info.peer,
							msdu);
				/* remove the peer reference added above */
				ol_txrx_peer_release_ref
						(tx_msdu_info.peer,
						 PEER_DEBUG_ID_OL_INTERNAL);
			}
MSDU_LOOP_BOTTOM:
			msdu = next;
		}

		if (call_sched)
			ol_tx_sched(pdev);
		return NULL; /* all MSDUs were accepted */
}

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK

/**
 * ol_tx_pdev_reset_driver_del_ack() - reset driver delayed ack enabled flag
 * @soc_hdl: soc handle
 * @pdev_id: datapath pdev identifier
 *
 * Return: none
 */
void
ol_tx_pdev_reset_driver_del_ack(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);
	struct ol_txrx_vdev_t *vdev;

	if (!pdev)
		return;

	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		vdev->driver_del_ack_enabled = false;

		dp_debug("vdev_id %d driver_del_ack_enabled %d",
			 vdev->vdev_id, vdev->driver_del_ack_enabled);
	}
}

/**
 * ol_tx_vdev_set_driver_del_ack_enable() - set driver delayed ack enabled flag
 * @soc_hdl: datapath soc handle
 * @vdev_id: vdev id
 * @rx_packets: number of rx packets
 * @time_in_ms: time in ms
 * @high_th: high threshold
 * @low_th: low threshold
 *
 * Return: none
 */
void
ol_tx_vdev_set_driver_del_ack_enable(struct cdp_soc_t *soc_hdl,
				     uint8_t vdev_id,
				     unsigned long rx_packets,
				     uint32_t time_in_ms,
				     uint32_t high_th,
				     uint32_t low_th)
{
	struct ol_txrx_vdev_t *vdev =
			(struct ol_txrx_vdev_t *)
			ol_txrx_get_vdev_from_vdev_id(vdev_id);
	bool old_driver_del_ack_enabled;

	if ((!vdev) || (low_th > high_th))
		return;

	old_driver_del_ack_enabled = vdev->driver_del_ack_enabled;
	if (rx_packets > high_th)
		vdev->driver_del_ack_enabled = true;
	else if (rx_packets < low_th)
		vdev->driver_del_ack_enabled = false;

	if (old_driver_del_ack_enabled != vdev->driver_del_ack_enabled) {
		dp_debug("vdev_id %d driver_del_ack_enabled %d rx_packets %ld time_in_ms %d high_th %d low_th %d",
			 vdev->vdev_id, vdev->driver_del_ack_enabled,
			 rx_packets, time_in_ms, high_th, low_th);
	}
}

/**
 * ol_tx_hl_send_all_tcp_ack() - send all queued tcp ack packets
 * @vdev: vdev handle
 *
 * Return: none
 */
void ol_tx_hl_send_all_tcp_ack(struct ol_txrx_vdev_t *vdev)
{
	int i;
	struct tcp_stream_node *tcp_node_list;
	struct tcp_stream_node *temp;
	struct ol_txrx_pdev_t *pdev = vdev->pdev;

	for (i = 0; i < OL_TX_HL_DEL_ACK_HASH_SIZE; i++) {
		tcp_node_list = NULL;
		qdf_spin_lock_bh(&vdev->tcp_ack_hash.node[i].hash_node_lock);
		if (vdev->tcp_ack_hash.node[i].no_of_entries)
			tcp_node_list = vdev->tcp_ack_hash.node[i].head;

		vdev->tcp_ack_hash.node[i].no_of_entries = 0;
		vdev->tcp_ack_hash.node[i].head = NULL;
		qdf_spin_unlock_bh(&vdev->tcp_ack_hash.node[i].hash_node_lock);

		/* Send all packets */
		while (tcp_node_list) {
			int tx_comp_req = pdev->cfg.default_tx_comp_req ||
						pdev->cfg.request_tx_comp;
			qdf_nbuf_t msdu_list;

			temp = tcp_node_list;
			tcp_node_list = temp->next;

			msdu_list = ol_tx_hl_base(vdev, OL_TX_SPEC_STD,
						  temp->head,
						  tx_comp_req, false);
			if (msdu_list)
				qdf_nbuf_tx_free(msdu_list, 1/*error*/);
			ol_txrx_vdev_free_tcp_node(vdev, temp);
		}
	}
	ol_tx_sched(vdev->pdev);
}

/**
 * tcp_del_ack_tasklet() - tasklet function to send ack packets
 * @data: vdev handle
 *
 * Return: none
 */
void tcp_del_ack_tasklet(void *data)
{
	struct ol_txrx_vdev_t *vdev = data;

	ol_tx_hl_send_all_tcp_ack(vdev);
}

/**
 * ol_tx_get_stream_id() - get stream_id from packet info
 * @info: packet info
 *
 * Return: stream_id
 */
uint16_t ol_tx_get_stream_id(struct packet_info *info)
{
	return ((info->dst_port + info->dst_ip + info->src_port + info->src_ip)
					 & (OL_TX_HL_DEL_ACK_HASH_SIZE - 1));
}

/**
 * ol_tx_is_tcp_ack() - check whether the packet is tcp ack frame
 * @msdu: packet
 *
 * Return: true if the packet is tcp ack frame
 */
static bool
ol_tx_is_tcp_ack(qdf_nbuf_t msdu)
{
	uint16_t ether_type;
	uint8_t  protocol;
	uint8_t  flag, ip_header_len, tcp_header_len;
	uint32_t seg_len;
	uint8_t  *skb_data;
	uint32_t skb_len;
	bool tcp_acked = false;
	uint32_t tcp_header_off;

	qdf_nbuf_peek_header(msdu, &skb_data, &skb_len);
	if (skb_len < (QDF_NBUF_TRAC_IPV4_OFFSET +
	    QDF_NBUF_TRAC_IPV4_HEADER_SIZE +
	    QDF_NBUF_TRAC_TCP_FLAGS_OFFSET))
		goto exit;

	ether_type = (uint16_t)(*(uint16_t *)
			(skb_data + QDF_NBUF_TRAC_ETH_TYPE_OFFSET));
	protocol = (uint16_t)(*(uint16_t *)
			(skb_data + QDF_NBUF_TRAC_IPV4_PROTO_TYPE_OFFSET));

	if ((QDF_SWAP_U16(QDF_NBUF_TRAC_IPV4_ETH_TYPE) == ether_type) &&
	    (protocol == QDF_NBUF_TRAC_TCP_TYPE)) {
		ip_header_len = ((uint8_t)(*(uint8_t *)
				(skb_data + QDF_NBUF_TRAC_IPV4_OFFSET)) &
				QDF_NBUF_TRAC_IPV4_HEADER_MASK) << 2;
		tcp_header_off = QDF_NBUF_TRAC_IPV4_OFFSET + ip_header_len;

		tcp_header_len = ((uint8_t)(*(uint8_t *)
			(skb_data + tcp_header_off +
			QDF_NBUF_TRAC_TCP_HEADER_LEN_OFFSET))) >> 2;
		seg_len = skb_len - tcp_header_len - tcp_header_off;
		flag = (uint8_t)(*(uint8_t *)
			(skb_data + tcp_header_off +
			QDF_NBUF_TRAC_TCP_FLAGS_OFFSET));

		if ((flag == QDF_NBUF_TRAC_TCP_ACK_MASK) && (seg_len == 0))
			tcp_acked = true;
	}

exit:

	return tcp_acked;
}

/**
 * ol_tx_get_packet_info() - update packet info for passed msdu
 * @msdu: packet
 * @info: packet info
 *
 * Return: none
 */
void ol_tx_get_packet_info(qdf_nbuf_t msdu, struct packet_info *info)
{
	uint16_t ether_type;
	uint8_t  protocol;
	uint8_t  flag, ip_header_len, tcp_header_len;
	uint32_t seg_len;
	uint8_t  *skb_data;
	uint32_t skb_len;
	uint32_t tcp_header_off;

	info->type = NO_TCP_PKT;

	qdf_nbuf_peek_header(msdu, &skb_data, &skb_len);
	if (skb_len < (QDF_NBUF_TRAC_IPV4_OFFSET +
	    QDF_NBUF_TRAC_IPV4_HEADER_SIZE +
	    QDF_NBUF_TRAC_TCP_FLAGS_OFFSET))
		return;

	ether_type = (uint16_t)(*(uint16_t *)
			(skb_data + QDF_NBUF_TRAC_ETH_TYPE_OFFSET));
	protocol = (uint16_t)(*(uint16_t *)
			(skb_data + QDF_NBUF_TRAC_IPV4_PROTO_TYPE_OFFSET));

	if ((QDF_SWAP_U16(QDF_NBUF_TRAC_IPV4_ETH_TYPE) == ether_type) &&
	    (protocol == QDF_NBUF_TRAC_TCP_TYPE)) {
		ip_header_len = ((uint8_t)(*(uint8_t *)
				(skb_data + QDF_NBUF_TRAC_IPV4_OFFSET)) &
				QDF_NBUF_TRAC_IPV4_HEADER_MASK) << 2;
		tcp_header_off = QDF_NBUF_TRAC_IPV4_OFFSET + ip_header_len;

		tcp_header_len = ((uint8_t)(*(uint8_t *)
			(skb_data + tcp_header_off +
			QDF_NBUF_TRAC_TCP_HEADER_LEN_OFFSET))) >> 2;
		seg_len = skb_len - tcp_header_len - tcp_header_off;
		flag = (uint8_t)(*(uint8_t *)
			(skb_data + tcp_header_off +
			QDF_NBUF_TRAC_TCP_FLAGS_OFFSET));

		info->src_ip = QDF_SWAP_U32((uint32_t)(*(uint32_t *)
			(skb_data + QDF_NBUF_TRAC_IPV4_SRC_ADDR_OFFSET)));
		info->dst_ip = QDF_SWAP_U32((uint32_t)(*(uint32_t *)
			(skb_data + QDF_NBUF_TRAC_IPV4_DEST_ADDR_OFFSET)));
		info->src_port = QDF_SWAP_U16((uint16_t)(*(uint16_t *)
				(skb_data + tcp_header_off +
				QDF_NBUF_TRAC_TCP_SPORT_OFFSET)));
		info->dst_port = QDF_SWAP_U16((uint16_t)(*(uint16_t *)
				(skb_data + tcp_header_off +
				QDF_NBUF_TRAC_TCP_DPORT_OFFSET)));
		info->stream_id = ol_tx_get_stream_id(info);

		if ((flag == QDF_NBUF_TRAC_TCP_ACK_MASK) && (seg_len == 0)) {
			info->type = TCP_PKT_ACK;
			info->ack_number = (uint32_t)(*(uint32_t *)
				(skb_data + tcp_header_off +
				QDF_NBUF_TRAC_TCP_ACK_OFFSET));
			info->ack_number = QDF_SWAP_U32(info->ack_number);
		} else {
			info->type = TCP_PKT_NO_ACK;
		}
	}
}

/**
 * ol_tx_hl_find_and_send_tcp_stream() - find and send tcp stream for passed
 *                                       stream info
 * @vdev: vdev handle
 * @info: packet info
 *
 * Return: none
 */
void ol_tx_hl_find_and_send_tcp_stream(struct ol_txrx_vdev_t *vdev,
				       struct packet_info *info)
{
	uint8_t no_of_entries;
	struct tcp_stream_node *node_to_be_remove = NULL;
	struct ol_txrx_pdev_t *pdev = vdev->pdev;

	/* remove tcp node from hash */
	qdf_spin_lock_bh(&vdev->tcp_ack_hash.node[info->stream_id].
			hash_node_lock);

	no_of_entries = vdev->tcp_ack_hash.node[info->stream_id].
			no_of_entries;
	if (no_of_entries > 1) {
		/* collision case */
		struct tcp_stream_node *head =
			vdev->tcp_ack_hash.node[info->stream_id].head;
		struct tcp_stream_node *temp;

		if ((head->dst_ip == info->dst_ip) &&
		    (head->src_ip == info->src_ip) &&
		    (head->src_port == info->src_port) &&
		    (head->dst_port == info->dst_port)) {
			node_to_be_remove = head;
			vdev->tcp_ack_hash.node[info->stream_id].head =
				head->next;
			vdev->tcp_ack_hash.node[info->stream_id].
				no_of_entries--;
		} else {
			temp = head;
			while (temp->next) {
				if ((temp->next->dst_ip == info->dst_ip) &&
				    (temp->next->src_ip == info->src_ip) &&
				    (temp->next->src_port == info->src_port) &&
				    (temp->next->dst_port == info->dst_port)) {
					node_to_be_remove = temp->next;
					temp->next = temp->next->next;
					vdev->tcp_ack_hash.
						node[info->stream_id].
						no_of_entries--;
					break;
				}
				temp = temp->next;
			}
		}
	} else if (no_of_entries == 1) {
		/* Only one tcp_node */
		node_to_be_remove =
			 vdev->tcp_ack_hash.node[info->stream_id].head;
		vdev->tcp_ack_hash.node[info->stream_id].head = NULL;
		vdev->tcp_ack_hash.node[info->stream_id].no_of_entries = 0;
	}
	qdf_spin_unlock_bh(&vdev->tcp_ack_hash.
			  node[info->stream_id].hash_node_lock);

	/* send packets */
	if (node_to_be_remove) {
		int tx_comp_req = pdev->cfg.default_tx_comp_req ||
					pdev->cfg.request_tx_comp;
		qdf_nbuf_t msdu_list;

		msdu_list = ol_tx_hl_base(vdev, OL_TX_SPEC_STD,
					  node_to_be_remove->head,
					  tx_comp_req, true);
		if (msdu_list)
			qdf_nbuf_tx_free(msdu_list, 1/*error*/);
		ol_txrx_vdev_free_tcp_node(vdev, node_to_be_remove);
	}
}

static struct tcp_stream_node *
ol_tx_hl_rep_tcp_ack(struct ol_txrx_vdev_t *vdev, qdf_nbuf_t msdu,
		     struct packet_info *info, bool *is_found,
		     bool *start_timer)
{
	struct tcp_stream_node *node_to_be_remove = NULL;
	struct tcp_stream_node *head =
		 vdev->tcp_ack_hash.node[info->stream_id].head;
	struct tcp_stream_node *temp;

	if ((head->dst_ip == info->dst_ip) &&
	    (head->src_ip == info->src_ip) &&
	    (head->src_port == info->src_port) &&
	    (head->dst_port == info->dst_port)) {
		*is_found = true;
		if ((head->ack_number < info->ack_number) &&
		    (head->no_of_ack_replaced <
		    ol_cfg_get_del_ack_count_value(vdev->pdev->ctrl_pdev))) {
			/* replace ack packet */
			qdf_nbuf_tx_free(head->head, 1);
			head->head = msdu;
			head->ack_number = info->ack_number;
			head->no_of_ack_replaced++;
			*start_timer = true;

			vdev->no_of_tcpack_replaced++;

			if (head->no_of_ack_replaced ==
			    ol_cfg_get_del_ack_count_value(
			    vdev->pdev->ctrl_pdev)) {
				node_to_be_remove = head;
				vdev->tcp_ack_hash.node[info->stream_id].head =
					head->next;
				vdev->tcp_ack_hash.node[info->stream_id].
					no_of_entries--;
			}
		} else {
			/* append and send packets */
			head->head->next = msdu;
			node_to_be_remove = head;
			vdev->tcp_ack_hash.node[info->stream_id].head =
				head->next;
			vdev->tcp_ack_hash.node[info->stream_id].
				no_of_entries--;
		}
	} else {
		temp = head;
		while (temp->next) {
			if ((temp->next->dst_ip == info->dst_ip) &&
			    (temp->next->src_ip == info->src_ip) &&
			    (temp->next->src_port == info->src_port) &&
			    (temp->next->dst_port == info->dst_port)) {
				*is_found = true;
				if ((temp->next->ack_number <
					info->ack_number) &&
				    (temp->next->no_of_ack_replaced <
					 ol_cfg_get_del_ack_count_value(
					 vdev->pdev->ctrl_pdev))) {
					/* replace ack packet */
					qdf_nbuf_tx_free(temp->next->head, 1);
					temp->next->head  = msdu;
					temp->next->ack_number =
						info->ack_number;
					temp->next->no_of_ack_replaced++;
					*start_timer = true;

					vdev->no_of_tcpack_replaced++;

					if (temp->next->no_of_ack_replaced ==
					   ol_cfg_get_del_ack_count_value(
					   vdev->pdev->ctrl_pdev)) {
						node_to_be_remove = temp->next;
						temp->next = temp->next->next;
						vdev->tcp_ack_hash.
							node[info->stream_id].
							no_of_entries--;
					}
				} else {
					/* append and send packets */
					temp->next->head->next = msdu;
					node_to_be_remove = temp->next;
					temp->next = temp->next->next;
					vdev->tcp_ack_hash.
						node[info->stream_id].
						no_of_entries--;
				}
				break;
			}
			temp = temp->next;
		}
	}
	return node_to_be_remove;
}

/**
 * ol_tx_hl_find_and_replace_tcp_ack() - find and replace tcp ack packet for
 *                                       passed packet info
 * @vdev: vdev handle
 * @msdu: packet
 * @info: packet info
 *
 * Return: none
 */
void ol_tx_hl_find_and_replace_tcp_ack(struct ol_txrx_vdev_t *vdev,
				       qdf_nbuf_t msdu,
				       struct packet_info *info)
{
	uint8_t no_of_entries;
	struct tcp_stream_node *node_to_be_remove = NULL;
	bool is_found = false, start_timer = false;
	struct ol_txrx_pdev_t *pdev = vdev->pdev;

	/* replace ack if required or send packets */
	qdf_spin_lock_bh(&vdev->tcp_ack_hash.node[info->stream_id].
			hash_node_lock);

	no_of_entries = vdev->tcp_ack_hash.node[info->stream_id].no_of_entries;
	if (no_of_entries > 0) {
		node_to_be_remove = ol_tx_hl_rep_tcp_ack(vdev, msdu, info,
							 &is_found,
							 &start_timer);
	}

	if (no_of_entries == 0 || !is_found) {
		/* Alloc new tcp node */
		struct tcp_stream_node *new_node;

		new_node = ol_txrx_vdev_alloc_tcp_node(vdev);
		if (!new_node) {
			qdf_spin_unlock_bh(&vdev->tcp_ack_hash.
					  node[info->stream_id].hash_node_lock);
			dp_alert("Malloc failed");
			return;
		}
		new_node->stream_id = info->stream_id;
		new_node->dst_ip = info->dst_ip;
		new_node->src_ip = info->src_ip;
		new_node->dst_port = info->dst_port;
		new_node->src_port = info->src_port;
		new_node->ack_number = info->ack_number;
		new_node->head = msdu;
		new_node->next = NULL;
		new_node->no_of_ack_replaced = 0;

		start_timer = true;
		/* insert new_node */
		if (!vdev->tcp_ack_hash.node[info->stream_id].head) {
			vdev->tcp_ack_hash.node[info->stream_id].head =
				new_node;
			vdev->tcp_ack_hash.node[info->stream_id].
				no_of_entries = 1;
		} else {
			struct tcp_stream_node *temp =
				 vdev->tcp_ack_hash.node[info->stream_id].head;
			while (temp->next)
				temp = temp->next;

			temp->next = new_node;
			vdev->tcp_ack_hash.node[info->stream_id].
				no_of_entries++;
		}
	}
	qdf_spin_unlock_bh(&vdev->tcp_ack_hash.node[info->stream_id].
			  hash_node_lock);

	/* start timer */
	if (start_timer &&
	    (!qdf_atomic_read(&vdev->tcp_ack_hash.is_timer_running))) {
		qdf_hrtimer_start(&vdev->tcp_ack_hash.timer,
				  qdf_ns_to_ktime((
						ol_cfg_get_del_ack_timer_value(
						vdev->pdev->ctrl_pdev) *
						1000000)),
			__QDF_HRTIMER_MODE_REL);
		qdf_atomic_set(&vdev->tcp_ack_hash.is_timer_running, 1);
	}

	/* send packets */
	if (node_to_be_remove) {
		int tx_comp_req = pdev->cfg.default_tx_comp_req ||
					pdev->cfg.request_tx_comp;
		qdf_nbuf_t msdu_list = NULL;

		msdu_list = ol_tx_hl_base(vdev, OL_TX_SPEC_STD,
					  node_to_be_remove->head,
					  tx_comp_req, true);
		if (msdu_list)
			qdf_nbuf_tx_free(msdu_list, 1/*error*/);
		ol_txrx_vdev_free_tcp_node(vdev, node_to_be_remove);
	}
}

/**
 * ol_tx_hl_vdev_tcp_del_ack_timer() - delayed ack timer function
 * @timer: timer handle
 *
 * Return: enum
 */
enum qdf_hrtimer_restart_status
ol_tx_hl_vdev_tcp_del_ack_timer(qdf_hrtimer_data_t *timer)
{
	struct ol_txrx_vdev_t *vdev = qdf_container_of(timer,
						       struct ol_txrx_vdev_t,
						       tcp_ack_hash.timer);
	enum qdf_hrtimer_restart_status ret = __QDF_HRTIMER_NORESTART;

	qdf_sched_bh(&vdev->tcp_ack_hash.tcp_del_ack_tq);
	qdf_atomic_set(&vdev->tcp_ack_hash.is_timer_running, 0);
	return ret;
}

/**
 * ol_tx_hl_del_ack_queue_flush_all() - drop all queued packets
 * @vdev: vdev handle
 *
 * Return: none
 */
void ol_tx_hl_del_ack_queue_flush_all(struct ol_txrx_vdev_t *vdev)
{
	int i;
	struct tcp_stream_node *tcp_node_list;
	struct tcp_stream_node *temp;

	qdf_hrtimer_cancel(&vdev->tcp_ack_hash.timer);
	for (i = 0; i < OL_TX_HL_DEL_ACK_HASH_SIZE; i++) {
		tcp_node_list = NULL;
		qdf_spin_lock_bh(&vdev->tcp_ack_hash.node[i].hash_node_lock);

		if (vdev->tcp_ack_hash.node[i].no_of_entries)
			tcp_node_list = vdev->tcp_ack_hash.node[i].head;

		vdev->tcp_ack_hash.node[i].no_of_entries = 0;
		vdev->tcp_ack_hash.node[i].head = NULL;
		qdf_spin_unlock_bh(&vdev->tcp_ack_hash.node[i].hash_node_lock);

		/* free all packets */
		while (tcp_node_list) {
			temp = tcp_node_list;
			tcp_node_list = temp->next;

			qdf_nbuf_tx_free(temp->head, 1/*error*/);
			ol_txrx_vdev_free_tcp_node(vdev, temp);
		}
	}
	ol_txrx_vdev_deinit_tcp_del_ack(vdev);
}

/**
 * ol_txrx_vdev_init_tcp_del_ack() - initialize tcp delayed ack structure
 * @vdev: vdev handle
 *
 * Return: none
 */
void ol_txrx_vdev_init_tcp_del_ack(struct ol_txrx_vdev_t *vdev)
{
	int i;

	vdev->driver_del_ack_enabled = false;

	dp_debug("vdev-id=%u, driver_del_ack_enabled=%d",
		 vdev->vdev_id,
		 vdev->driver_del_ack_enabled);

	vdev->no_of_tcpack = 0;
	vdev->no_of_tcpack_replaced = 0;

	qdf_hrtimer_init(&vdev->tcp_ack_hash.timer,
			 ol_tx_hl_vdev_tcp_del_ack_timer,
			 __QDF_CLOCK_MONOTONIC,
			 __QDF_HRTIMER_MODE_REL,
			 QDF_CONTEXT_HARDWARE
			 );
	qdf_create_bh(&vdev->tcp_ack_hash.tcp_del_ack_tq,
		      tcp_del_ack_tasklet,
		      vdev);
	qdf_atomic_init(&vdev->tcp_ack_hash.is_timer_running);
	qdf_atomic_init(&vdev->tcp_ack_hash.tcp_node_in_use_count);
	qdf_spinlock_create(&vdev->tcp_ack_hash.tcp_free_list_lock);
	vdev->tcp_ack_hash.tcp_free_list = NULL;
	for (i = 0; i < OL_TX_HL_DEL_ACK_HASH_SIZE; i++) {
		qdf_spinlock_create(&vdev->tcp_ack_hash.node[i].hash_node_lock);
		vdev->tcp_ack_hash.node[i].no_of_entries = 0;
		vdev->tcp_ack_hash.node[i].head = NULL;
	}
}

/**
 * ol_txrx_vdev_deinit_tcp_del_ack() - deinitialize tcp delayed ack structure
 * @vdev: vdev handle
 *
 * Return: none
 */
void ol_txrx_vdev_deinit_tcp_del_ack(struct ol_txrx_vdev_t *vdev)
{
	struct tcp_stream_node *temp;

	qdf_destroy_bh(&vdev->tcp_ack_hash.tcp_del_ack_tq);

	qdf_spin_lock_bh(&vdev->tcp_ack_hash.tcp_free_list_lock);
	while (vdev->tcp_ack_hash.tcp_free_list) {
		temp = vdev->tcp_ack_hash.tcp_free_list;
		vdev->tcp_ack_hash.tcp_free_list = temp->next;
		qdf_mem_free(temp);
	}
	qdf_spin_unlock_bh(&vdev->tcp_ack_hash.tcp_free_list_lock);
}

/**
 * ol_txrx_vdev_free_tcp_node() - add tcp node in free list
 * @vdev: vdev handle
 * @node: tcp stream node
 *
 * Return: none
 */
void ol_txrx_vdev_free_tcp_node(struct ol_txrx_vdev_t *vdev,
				struct tcp_stream_node *node)
{
	qdf_atomic_dec(&vdev->tcp_ack_hash.tcp_node_in_use_count);

	qdf_spin_lock_bh(&vdev->tcp_ack_hash.tcp_free_list_lock);
	if (vdev->tcp_ack_hash.tcp_free_list) {
		node->next = vdev->tcp_ack_hash.tcp_free_list;
		vdev->tcp_ack_hash.tcp_free_list = node;
	} else {
		vdev->tcp_ack_hash.tcp_free_list = node;
		node->next = NULL;
	}
	qdf_spin_unlock_bh(&vdev->tcp_ack_hash.tcp_free_list_lock);
}

/**
 * ol_txrx_vdev_alloc_tcp_node() - allocate tcp node
 * @vdev: vdev handle
 *
 * Return: tcp stream node
 */
struct tcp_stream_node *ol_txrx_vdev_alloc_tcp_node(struct ol_txrx_vdev_t *vdev)
{
	struct tcp_stream_node *node = NULL;

	qdf_spin_lock_bh(&vdev->tcp_ack_hash.tcp_free_list_lock);
	if (vdev->tcp_ack_hash.tcp_free_list) {
		node = vdev->tcp_ack_hash.tcp_free_list;
		vdev->tcp_ack_hash.tcp_free_list = node->next;
	}
	qdf_spin_unlock_bh(&vdev->tcp_ack_hash.tcp_free_list_lock);

	if (!node) {
		node = qdf_mem_malloc(sizeof(struct ol_txrx_vdev_t));
		if (!node)
			return NULL;
	}
	qdf_atomic_inc(&vdev->tcp_ack_hash.tcp_node_in_use_count);
	return node;
}

qdf_nbuf_t
ol_tx_hl(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	int tx_comp_req = pdev->cfg.default_tx_comp_req ||
				pdev->cfg.request_tx_comp;
	struct packet_info pkt_info;
	qdf_nbuf_t temp;

	if (ol_tx_is_tcp_ack(msdu_list))
		vdev->no_of_tcpack++;

	/* check Enable through ini */
	if (!ol_cfg_get_del_ack_enable_value(vdev->pdev->ctrl_pdev) ||
	    (!vdev->driver_del_ack_enabled)) {
		if (qdf_atomic_read(&vdev->tcp_ack_hash.tcp_node_in_use_count))
			ol_tx_hl_send_all_tcp_ack(vdev);

		return ol_tx_hl_base(vdev, OL_TX_SPEC_STD, msdu_list,
				    tx_comp_req, true);
	}

	ol_tx_get_packet_info(msdu_list, &pkt_info);

	if (pkt_info.type == TCP_PKT_NO_ACK) {
		ol_tx_hl_find_and_send_tcp_stream(vdev, &pkt_info);
		temp = ol_tx_hl_base(vdev, OL_TX_SPEC_STD, msdu_list,
				     tx_comp_req, true);
		return temp;
	}

	if (pkt_info.type == TCP_PKT_ACK) {
		ol_tx_hl_find_and_replace_tcp_ack(vdev, msdu_list, &pkt_info);
		return NULL;
	}

	temp = ol_tx_hl_base(vdev, OL_TX_SPEC_STD, msdu_list,
			     tx_comp_req, true);
	return temp;
}
#else

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
void
ol_tx_pdev_reset_bundle_require(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	struct ol_txrx_pdev_t *pdev = ol_txrx_get_pdev_from_pdev_id(soc,
								    pdev_id);
	struct ol_txrx_vdev_t *vdev;

	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		vdev->bundling_required = false;
		ol_txrx_info("vdev_id %d bundle_require %d\n",
			     vdev->vdev_id, vdev->bundling_required);
	}
}

void
ol_tx_vdev_set_bundle_require(uint8_t vdev_id, unsigned long tx_bytes,
			      uint32_t time_in_ms, uint32_t high_th,
			      uint32_t low_th)
{
	struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)
				ol_txrx_get_vdev_from_vdev_id(vdev_id);
	bool old_bundle_required;

	if ((!vdev) || (low_th > high_th))
		return;

	old_bundle_required = vdev->bundling_required;
	if (tx_bytes > ((high_th * time_in_ms * 1500) / 1000))
		vdev->bundling_required = true;
	else if (tx_bytes < ((low_th * time_in_ms * 1500) / 1000))
		vdev->bundling_required = false;

	if (old_bundle_required != vdev->bundling_required)
		ol_txrx_info("vdev_id %d bundle_require %d tx_bytes %ld time_in_ms %d high_th %d low_th %d\n",
			     vdev->vdev_id, vdev->bundling_required, tx_bytes,
			     time_in_ms, high_th, low_th);
}

/**
 * ol_tx_hl_queue_flush_all() - drop all packets in vdev bundle queue
 * @vdev: vdev handle
 *
 * Return: none
 */
void
ol_tx_hl_queue_flush_all(struct ol_txrx_vdev_t *vdev)
{
	qdf_spin_lock_bh(&vdev->bundle_queue.mutex);
	if (vdev->bundle_queue.txq.depth != 0) {
		qdf_timer_stop(&vdev->bundle_queue.timer);
		vdev->pdev->total_bundle_queue_length -=
				vdev->bundle_queue.txq.depth;
		qdf_nbuf_tx_free(vdev->bundle_queue.txq.head, 1/*error*/);
		vdev->bundle_queue.txq.depth = 0;
		vdev->bundle_queue.txq.head = NULL;
		vdev->bundle_queue.txq.tail = NULL;
	}
	qdf_spin_unlock_bh(&vdev->bundle_queue.mutex);
}

/**
 * ol_tx_hl_vdev_queue_append() - append pkt in tx queue
 * @vdev: vdev handle
 * @msdu_list: msdu list
 *
 * Return: none
 */
static void
ol_tx_hl_vdev_queue_append(struct ol_txrx_vdev_t *vdev, qdf_nbuf_t msdu_list)
{
	qdf_spin_lock_bh(&vdev->bundle_queue.mutex);

	if (!vdev->bundle_queue.txq.head) {
		qdf_timer_start(
			&vdev->bundle_queue.timer,
			ol_cfg_get_bundle_timer_value(vdev->pdev->ctrl_pdev));
		vdev->bundle_queue.txq.head = msdu_list;
		vdev->bundle_queue.txq.tail = msdu_list;
	} else {
		qdf_nbuf_set_next(vdev->bundle_queue.txq.tail, msdu_list);
	}

	while (qdf_nbuf_next(msdu_list)) {
		vdev->bundle_queue.txq.depth++;
		vdev->pdev->total_bundle_queue_length++;
		msdu_list = qdf_nbuf_next(msdu_list);
	}

	vdev->bundle_queue.txq.depth++;
	vdev->pdev->total_bundle_queue_length++;
	vdev->bundle_queue.txq.tail = msdu_list;
	qdf_spin_unlock_bh(&vdev->bundle_queue.mutex);
}

/**
 * ol_tx_hl_vdev_queue_send_all() - send all packets in vdev bundle queue
 * @vdev: vdev handle
 * @call_sched: invoke scheduler
 *
 * Return: NULL for success
 */
static qdf_nbuf_t
ol_tx_hl_vdev_queue_send_all(struct ol_txrx_vdev_t *vdev, bool call_sched,
			     bool in_timer_context)
{
	qdf_nbuf_t msdu_list = NULL;
	qdf_nbuf_t skb_list_head, skb_list_tail;
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	int tx_comp_req = pdev->cfg.default_tx_comp_req ||
				pdev->cfg.request_tx_comp;
	int pkt_to_sent;

	qdf_spin_lock_bh(&vdev->bundle_queue.mutex);

	if (!vdev->bundle_queue.txq.depth) {
		qdf_spin_unlock_bh(&vdev->bundle_queue.mutex);
		return msdu_list;
	}

	if (likely((qdf_atomic_read(&vdev->tx_desc_count) +
		    vdev->bundle_queue.txq.depth) <
		    vdev->queue_stop_th)) {
		qdf_timer_stop(&vdev->bundle_queue.timer);
		vdev->pdev->total_bundle_queue_length -=
				vdev->bundle_queue.txq.depth;
		msdu_list = ol_tx_hl_base(vdev, OL_TX_SPEC_STD,
					  vdev->bundle_queue.txq.head,
					  tx_comp_req, call_sched);
		vdev->bundle_queue.txq.depth = 0;
		vdev->bundle_queue.txq.head = NULL;
		vdev->bundle_queue.txq.tail = NULL;
	} else {
		pkt_to_sent = vdev->queue_stop_th -
			qdf_atomic_read(&vdev->tx_desc_count);

		if (pkt_to_sent) {
			skb_list_head = vdev->bundle_queue.txq.head;

			while (pkt_to_sent) {
				skb_list_tail =
					vdev->bundle_queue.txq.head;
				vdev->bundle_queue.txq.head =
				    qdf_nbuf_next(vdev->bundle_queue.txq.head);
				vdev->pdev->total_bundle_queue_length--;
				vdev->bundle_queue.txq.depth--;
				pkt_to_sent--;
				if (!vdev->bundle_queue.txq.head) {
					qdf_timer_stop(
						&vdev->bundle_queue.timer);
					break;
				}
			}

			qdf_nbuf_set_next(skb_list_tail, NULL);
			msdu_list = ol_tx_hl_base(vdev, OL_TX_SPEC_STD,
						  skb_list_head, tx_comp_req,
						  call_sched);
		}

		if (in_timer_context &&	vdev->bundle_queue.txq.head) {
			qdf_timer_start(
				&vdev->bundle_queue.timer,
				ol_cfg_get_bundle_timer_value(
					vdev->pdev->ctrl_pdev));
		}
	}
	qdf_spin_unlock_bh(&vdev->bundle_queue.mutex);

	return msdu_list;
}

/**
 * ol_tx_hl_pdev_queue_send_all() - send all packets from all vdev bundle queue
 * @pdev: pdev handle
 *
 * Return: NULL for success
 */
qdf_nbuf_t
ol_tx_hl_pdev_queue_send_all(struct ol_txrx_pdev_t *pdev)
{
	struct ol_txrx_vdev_t *vdev;
	qdf_nbuf_t msdu_list;

	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		msdu_list = ol_tx_hl_vdev_queue_send_all(vdev, false, false);
		if (msdu_list)
			qdf_nbuf_tx_free(msdu_list, 1/*error*/);
	}
	ol_tx_sched(pdev);
	return NULL; /* all msdus were accepted */
}

/**
 * ol_tx_hl_vdev_bundle_timer() - bundle timer function
 * @vdev: vdev handle
 *
 * Return: none
 */
void
ol_tx_hl_vdev_bundle_timer(void *ctx)
{
	qdf_nbuf_t msdu_list;
	struct ol_txrx_vdev_t *vdev = (struct ol_txrx_vdev_t *)ctx;

	vdev->no_of_bundle_sent_in_timer++;
	msdu_list = ol_tx_hl_vdev_queue_send_all(vdev, true, true);
	if (msdu_list)
		qdf_nbuf_tx_free(msdu_list, 1/*error*/);
}

qdf_nbuf_t
ol_tx_hl(struct ol_txrx_vdev_t *vdev, qdf_nbuf_t msdu_list)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	int tx_comp_req = pdev->cfg.default_tx_comp_req ||
				pdev->cfg.request_tx_comp;

	/* No queuing for high priority packets */
	if (ol_tx_desc_is_high_prio(msdu_list)) {
		vdev->no_of_pkt_not_added_in_queue++;
		return ol_tx_hl_base(vdev, OL_TX_SPEC_STD, msdu_list,
					     tx_comp_req, true);
	} else if (vdev->bundling_required &&
	    (ol_cfg_get_bundle_size(vdev->pdev->ctrl_pdev) > 1)) {
		ol_tx_hl_vdev_queue_append(vdev, msdu_list);

		if (pdev->total_bundle_queue_length >=
		    ol_cfg_get_bundle_size(vdev->pdev->ctrl_pdev)) {
			vdev->no_of_bundle_sent_after_threshold++;
			return ol_tx_hl_pdev_queue_send_all(pdev);
		}
	} else {
		if (vdev->bundle_queue.txq.depth != 0) {
			ol_tx_hl_vdev_queue_append(vdev, msdu_list);
			return ol_tx_hl_vdev_queue_send_all(vdev, true, false);
		} else {
			vdev->no_of_pkt_not_added_in_queue++;
			return ol_tx_hl_base(vdev, OL_TX_SPEC_STD, msdu_list,
					     tx_comp_req, true);
		}
	}

	return NULL; /* all msdus were accepted */
}

#else

qdf_nbuf_t
ol_tx_hl(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	int tx_comp_req = pdev->cfg.default_tx_comp_req ||
				pdev->cfg.request_tx_comp;

	return ol_tx_hl_base(vdev, OL_TX_SPEC_STD,
			     msdu_list, tx_comp_req, true);
}
#endif
#endif

qdf_nbuf_t ol_tx_non_std_hl(struct ol_txrx_vdev_t *vdev,
			    enum ol_tx_spec tx_spec,
			    qdf_nbuf_t msdu_list)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	int tx_comp_req = pdev->cfg.default_tx_comp_req ||
				pdev->cfg.request_tx_comp;

	if (!tx_comp_req) {
		if ((tx_spec == OL_TX_SPEC_NO_FREE) &&
		    (pdev->tx_data_callback.func))
			tx_comp_req = 1;
	}
	return ol_tx_hl_base(vdev, tx_spec, msdu_list, tx_comp_req, true);
}

#ifdef FEATURE_WLAN_TDLS
void ol_txrx_copy_mac_addr_raw(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			       uint8_t *bss_addr)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_vdev_handle vdev = ol_txrx_get_vdev_from_soc_vdev_id(soc,
								     vdev_id);

	if (!vdev)
		return;

	qdf_spin_lock_bh(&vdev->pdev->last_real_peer_mutex);
	if (bss_addr && vdev->last_real_peer &&
	    !qdf_mem_cmp((u8 *)bss_addr,
			     vdev->last_real_peer->mac_addr.raw,
			     QDF_MAC_ADDR_SIZE))
		qdf_mem_copy(vdev->hl_tdls_ap_mac_addr.raw,
			     vdev->last_real_peer->mac_addr.raw,
			     QDF_MAC_ADDR_SIZE);
	qdf_spin_unlock_bh(&vdev->pdev->last_real_peer_mutex);
}

void
ol_txrx_add_last_real_peer(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			   uint8_t vdev_id)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);
	ol_txrx_vdev_handle vdev = ol_txrx_get_vdev_from_soc_vdev_id(soc,
								     vdev_id);
	ol_txrx_peer_handle peer;

	if (!pdev || !vdev)
		return;

	peer = ol_txrx_find_peer_by_addr(
		(struct cdp_pdev *)pdev,
		vdev->hl_tdls_ap_mac_addr.raw);

	qdf_spin_lock_bh(&pdev->last_real_peer_mutex);
	if (!vdev->last_real_peer && peer &&
	    (peer->peer_ids[0] != HTT_INVALID_PEER_ID)) {
		vdev->last_real_peer = peer;
		qdf_mem_zero(vdev->hl_tdls_ap_mac_addr.raw,
			     QDF_MAC_ADDR_SIZE);
	}
	qdf_spin_unlock_bh(&pdev->last_real_peer_mutex);
}

bool is_vdev_restore_last_peer(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			       uint8_t *peer_mac)
{
	struct ol_txrx_peer_t *peer;
	struct ol_txrx_pdev_t *pdev;
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_vdev_handle vdev = ol_txrx_get_vdev_from_soc_vdev_id(soc,
								     vdev_id);

	if (!vdev)
		return false;

	pdev = vdev->pdev;
	peer = ol_txrx_find_peer_by_addr((struct cdp_pdev *)pdev, peer_mac);

	return vdev->last_real_peer && (vdev->last_real_peer == peer);
}

void ol_txrx_update_last_real_peer(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
				   uint8_t vdev_id, bool restore_last_peer)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);
	ol_txrx_vdev_handle vdev = ol_txrx_get_vdev_from_soc_vdev_id(soc,
								     vdev_id);
	struct ol_txrx_peer_t *peer;

	if (!restore_last_peer || !pdev || !vdev)
		return;

	peer = ol_txrx_find_peer_by_addr((struct cdp_pdev *)pdev,
					 vdev->hl_tdls_ap_mac_addr.raw);

	qdf_spin_lock_bh(&pdev->last_real_peer_mutex);
	if (!vdev->last_real_peer && peer &&
	    (peer->peer_ids[0] != HTT_INVALID_PEER_ID)) {
		vdev->last_real_peer = peer;
		qdf_mem_zero(vdev->hl_tdls_ap_mac_addr.raw,
			     QDF_MAC_ADDR_SIZE);
	}
	qdf_spin_unlock_bh(&pdev->last_real_peer_mutex);
}

void ol_txrx_set_peer_as_tdls_peer(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				   uint8_t *peer_mac, bool val)
{
	ol_txrx_peer_handle peer;
	struct ol_txrx_pdev_t *pdev;
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_vdev_handle vdev = ol_txrx_get_vdev_from_soc_vdev_id(soc,
								     vdev_id);

	if (!vdev)
		return;

	pdev = vdev->pdev;
	peer = ol_txrx_find_peer_by_addr((struct cdp_pdev *)pdev, peer_mac);

	ol_txrx_info_high("peer %pK, peer->ref_cnt %d",
			  peer, qdf_atomic_read(&peer->ref_cnt));

	/* Mark peer as tdls */
	if (peer)
		peer->is_tdls_peer = val;
}

void ol_txrx_set_tdls_offchan_enabled(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				      uint8_t *peer_mac, bool val)
{
	ol_txrx_peer_handle peer;
	struct ol_txrx_pdev_t *pdev;
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_vdev_handle vdev = ol_txrx_get_vdev_from_soc_vdev_id(soc,
								     vdev_id);

	if (!vdev)
		return;

	pdev = vdev->pdev;
	peer = ol_txrx_find_peer_by_addr((struct cdp_pdev *)pdev, peer_mac);

	ol_txrx_info_high("peer %pK, peer->ref_cnt %d",
			  peer, qdf_atomic_read(&peer->ref_cnt));

	/* Set TDLS Offchan operation enable/disable */
	if (peer && peer->is_tdls_peer)
		peer->tdls_offchan_enabled = val;
}
#endif

#if defined(CONFIG_HL_SUPPORT) && defined(DEBUG_HL_LOGGING)
/**
 * ol_txrx_pdev_txq_log_init() - initialise pdev txq logs
 * @pdev: the physical device object
 *
 * Return: None
 */
void ol_txrx_pdev_txq_log_init(struct ol_txrx_pdev_t *pdev)
{
	qdf_spinlock_create(&pdev->txq_log_spinlock);
	pdev->txq_log.size = OL_TXQ_LOG_SIZE;
	pdev->txq_log.oldest_record_offset = 0;
	pdev->txq_log.offset = 0;
	pdev->txq_log.allow_wrap = 1;
	pdev->txq_log.wrapped = 0;
}

/**
 * ol_txrx_pdev_txq_log_destroy() - remove txq log spinlock for pdev
 * @pdev: the physical device object
 *
 * Return: None
 */
void ol_txrx_pdev_txq_log_destroy(struct ol_txrx_pdev_t *pdev)
{
	qdf_spinlock_destroy(&pdev->txq_log_spinlock);
}
#endif

#if defined(DEBUG_HL_LOGGING)

/**
 * ol_txrx_pdev_grp_stats_init() - initialise group stat spinlock for pdev
 * @pdev: the physical device object
 *
 * Return: None
 */
void ol_txrx_pdev_grp_stats_init(struct ol_txrx_pdev_t *pdev)
{
	qdf_spinlock_create(&pdev->grp_stat_spinlock);
	pdev->grp_stats.last_valid_index = -1;
	pdev->grp_stats.wrap_around = 0;
}

/**
 * ol_txrx_pdev_grp_stat_destroy() - destroy group stat spinlock for pdev
 * @pdev: the physical device object
 *
 * Return: None
 */
void ol_txrx_pdev_grp_stat_destroy(struct ol_txrx_pdev_t *pdev)
{
	qdf_spinlock_destroy(&pdev->grp_stat_spinlock);
}
#endif

#if defined(CONFIG_HL_SUPPORT) && defined(FEATURE_WLAN_TDLS)

/**
 * ol_txrx_hl_tdls_flag_reset() - reset tdls flag for vdev
 * @soc_hdl: Datapath soc handle
 * @vdev_id: id of vdev
 * @flag: flag
 *
 * Return: None
 */
void
ol_txrx_hl_tdls_flag_reset(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
			   bool flag)
{
	struct ol_txrx_vdev_t *vdev =
		(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);
	if (!vdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid vdev_id %d", __func__, vdev_id);
		return;
	}

	vdev->hlTdlsFlag = flag;
}
#endif

/**
 * ol_txrx_vdev_txqs_init() - initialise vdev tx queues
 * @vdev: the virtual device object
 *
 * Return: None
 */
void ol_txrx_vdev_txqs_init(struct ol_txrx_vdev_t *vdev)
{
	uint8_t i;

	for (i = 0; i < OL_TX_VDEV_NUM_QUEUES; i++) {
		TAILQ_INIT(&vdev->txqs[i].head);
		vdev->txqs[i].paused_count.total = 0;
		vdev->txqs[i].frms = 0;
		vdev->txqs[i].bytes = 0;
		vdev->txqs[i].ext_tid = OL_TX_NUM_TIDS + i;
		vdev->txqs[i].flag = ol_tx_queue_empty;
		/* aggregation is not applicable for vdev tx queues */
		vdev->txqs[i].aggr_state = ol_tx_aggr_disabled;
		ol_tx_txq_set_group_ptr(&vdev->txqs[i], NULL);
		ol_txrx_set_txq_peer(&vdev->txqs[i], NULL);
	}
}

/**
 * ol_txrx_vdev_tx_queue_free() - free vdev tx queues
 * @vdev: the virtual device object
 *
 * Return: None
 */
void ol_txrx_vdev_tx_queue_free(struct ol_txrx_vdev_t *vdev)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	struct ol_tx_frms_queue_t *txq;
	int i;

	for (i = 0; i < OL_TX_VDEV_NUM_QUEUES; i++) {
		txq = &vdev->txqs[i];
		ol_tx_queue_free(pdev, txq, (i + OL_TX_NUM_TIDS), false);
	}
}

/**
 * ol_txrx_peer_txqs_init() - initialise peer tx queues
 * @pdev: the physical device object
 * @peer: peer object
 *
 * Return: None
 */
void ol_txrx_peer_txqs_init(struct ol_txrx_pdev_t *pdev,
			    struct ol_txrx_peer_t *peer)
{
	uint8_t i;
	struct ol_txrx_vdev_t *vdev = peer->vdev;

	qdf_spin_lock_bh(&pdev->tx_queue_spinlock);
	for (i = 0; i < OL_TX_NUM_TIDS; i++) {
		TAILQ_INIT(&peer->txqs[i].head);
		peer->txqs[i].paused_count.total = 0;
		peer->txqs[i].frms = 0;
		peer->txqs[i].bytes = 0;
		peer->txqs[i].ext_tid = i;
		peer->txqs[i].flag = ol_tx_queue_empty;
		peer->txqs[i].aggr_state = ol_tx_aggr_untried;
		ol_tx_set_peer_group_ptr(pdev, peer, vdev->vdev_id, i);
		ol_txrx_set_txq_peer(&peer->txqs[i], peer);
	}
	qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);

	/* aggregation is not applicable for mgmt and non-QoS tx queues */
	for (i = OL_TX_NUM_QOS_TIDS; i < OL_TX_NUM_TIDS; i++)
		peer->txqs[i].aggr_state = ol_tx_aggr_disabled;

	ol_txrx_peer_pause(peer);
}

/**
 * ol_txrx_peer_tx_queue_free() - free peer tx queues
 * @pdev: the physical device object
 * @peer: peer object
 *
 * Return: None
 */
void ol_txrx_peer_tx_queue_free(struct ol_txrx_pdev_t *pdev,
				struct ol_txrx_peer_t *peer)
{
	struct ol_tx_frms_queue_t *txq;
	uint8_t i;

	for (i = 0; i < OL_TX_NUM_TIDS; i++) {
		txq = &peer->txqs[i];
		ol_tx_queue_free(pdev, txq, i, true);
	}
}

#ifdef FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL

/**
 * ol_txrx_update_group_credit() - update group credit for tx queue
 * @group: for which credit needs to be updated
 * @credit: credits
 * @absolute: TXQ group absolute
 *
 * Return: allocated pool size
 */
void ol_txrx_update_group_credit(
		struct ol_tx_queue_group_t *group,
		int32_t credit,
		u_int8_t absolute)
{
	if (absolute)
		qdf_atomic_set(&group->credit, credit);
	else
		qdf_atomic_add(credit, &group->credit);
}

/**
 * ol_txrx_update_tx_queue_groups() - update vdev tx queue group if
 *				      vdev id mask and ac mask is not matching
 * @pdev: the data physical device
 * @group_id: TXQ group id
 * @credit: TXQ group credit count
 * @absolute: TXQ group absolute
 * @vdev_id_mask: TXQ vdev group id mask
 * @ac_mask: TQX access category mask
 *
 * Return: None
 */
void ol_txrx_update_tx_queue_groups(
		ol_txrx_pdev_handle pdev,
		u_int8_t group_id,
		int32_t credit,
		u_int8_t absolute,
		u_int32_t vdev_id_mask,
		u_int32_t ac_mask
		)
{
	struct ol_tx_queue_group_t *group;
	u_int32_t group_vdev_bit_mask, vdev_bit_mask, group_vdev_id_mask;
	u_int32_t membership;
	struct ol_txrx_vdev_t *vdev;

	if (group_id >= OL_TX_MAX_TXQ_GROUPS) {
		ol_txrx_warn("invalid group_id=%u, ignore update", group_id);
		return;
	}

	group = &pdev->txq_grps[group_id];

	membership = OL_TXQ_GROUP_MEMBERSHIP_GET(vdev_id_mask, ac_mask);

	qdf_spin_lock_bh(&pdev->tx_queue_spinlock);
	/*
	 * if the membership (vdev id mask and ac mask)
	 * matches then no need to update tx qeue groups.
	 */
	if (group->membership == membership)
		/* Update Credit Only */
		goto credit_update;

	credit += ol_txrx_distribute_group_credits(pdev, group_id,
						   vdev_id_mask);
	/*
	 * membership (vdev id mask and ac mask) is not matching
	 * TODO: ignoring ac mask for now
	 */
	qdf_assert(ac_mask == 0xffff);
	group_vdev_id_mask =
		OL_TXQ_GROUP_VDEV_ID_MASK_GET(group->membership);

	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		group_vdev_bit_mask =
			OL_TXQ_GROUP_VDEV_ID_BIT_MASK_GET(
					group_vdev_id_mask, vdev->vdev_id);
		vdev_bit_mask =
			OL_TXQ_GROUP_VDEV_ID_BIT_MASK_GET(
					vdev_id_mask, vdev->vdev_id);

		if (group_vdev_bit_mask != vdev_bit_mask) {
			/*
			 * Change in vdev tx queue group
			 */
			if (!vdev_bit_mask) {
				/* Set Group Pointer (vdev and peer) to NULL */
				ol_txrx_info("Group membership removed for vdev_id %d from group_id %d",
					     vdev->vdev_id, group_id);
				ol_tx_set_vdev_group_ptr(
						pdev, vdev->vdev_id, NULL);
			} else {
				/* Set Group Pointer (vdev and peer) */
				ol_txrx_info("Group membership updated for vdev_id %d to group_id %d",
					     vdev->vdev_id, group_id);
				ol_tx_set_vdev_group_ptr(
						pdev, vdev->vdev_id, group);
			}
		}
	}
	/* Update membership */
	group->membership = membership;
	ol_txrx_info("Group membership updated for group_id %d membership 0x%x",
		     group_id, group->membership);
credit_update:
	/* Update Credit */
	ol_txrx_update_group_credit(group, credit, absolute);
	qdf_spin_unlock_bh(&pdev->tx_queue_spinlock);
}
#endif

#if defined(FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL) && \
	defined(FEATURE_HL_DBS_GROUP_CREDIT_SHARING)
#define MIN_INIT_GROUP_CREDITS	10
int ol_txrx_distribute_group_credits(struct ol_txrx_pdev_t *pdev,
				     u8 group_id,
				     u32 vdevid_mask_new)
{
	struct ol_tx_queue_group_t *grp = &pdev->txq_grps[group_id];
	struct ol_tx_queue_group_t *grp_nxt = &pdev->txq_grps[!group_id];
	int creds_nxt = qdf_atomic_read(&grp_nxt->credit);
	int vdevid_mask = OL_TXQ_GROUP_VDEV_ID_MASK_GET(grp->membership);
	int vdevid_mask_othgrp =
		OL_TXQ_GROUP_VDEV_ID_MASK_GET(grp_nxt->membership);
	int creds_distribute = 0;

	/* if vdev added to the group is the first vdev */
	if ((vdevid_mask == 0) && (vdevid_mask_new != 0)) {
		/* if other group has members */
		if (vdevid_mask_othgrp) {
			if (creds_nxt < MIN_INIT_GROUP_CREDITS)
				creds_distribute = creds_nxt / 2;
			else
				creds_distribute = MIN_INIT_GROUP_CREDITS;

			ol_txrx_update_group_credit(grp_nxt, -creds_distribute,
						    0);
		} else {
			/*
			 * Other grp has no members, give all credits to this
			 * grp.
			 */
			creds_distribute =
				qdf_atomic_read(&pdev->target_tx_credit);
		}
	/* if all vdevs are removed from this grp */
	} else if ((vdevid_mask != 0) && (vdevid_mask_new == 0)) {
		if (vdevid_mask_othgrp)
			/* Transfer credits to other grp */
			ol_txrx_update_group_credit(grp_nxt,
						    qdf_atomic_read(&grp->
						    credit),
						    0);
		/* Set current grp credits to zero */
		ol_txrx_update_group_credit(grp, 0, 1);
	}

	return creds_distribute;
}
#endif /*
	* FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL &&
	* FEATURE_HL_DBS_GROUP_CREDIT_SHARING
	*/

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
int ol_txrx_register_hl_flow_control(struct cdp_soc_t *soc_hdl,
				     uint8_t pdev_id,
				     tx_pause_callback flowcontrol)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev = ol_txrx_get_pdev_from_pdev_id(soc, pdev_id);
	u32 desc_pool_size;

	if (!pdev || !flowcontrol) {
		ol_txrx_err("pdev or pause_cb is NULL");
		return QDF_STATUS_E_INVAL;
	}

	desc_pool_size = ol_tx_desc_pool_size_hl(pdev->ctrl_pdev);
	/*
	 * Assert if the tx descriptor pool size meets the requirements
	 * Maximum 2 sessions are allowed on a band.
	 */
	QDF_ASSERT((2 * ol_txrx_tx_desc_alloc_table[TXRX_FC_5GH_80M_2x2] +
		    ol_txrx_tx_desc_alloc_table[TXRX_FC_2GH_40M_2x2])
		    <= desc_pool_size);

	pdev->pause_cb = flowcontrol;
	return 0;
}

int ol_txrx_set_vdev_os_queue_status(struct cdp_soc_t *soc_hdl, u8 vdev_id,
				     enum netif_action_type action)
{
	struct ol_txrx_vdev_t *vdev =
	(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);

	if (!vdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid vdev_id %d", __func__, vdev_id);
		return -EINVAL;
	}

	switch (action) {
	case WLAN_NETIF_PRIORITY_QUEUE_ON:
		qdf_spin_lock_bh(&vdev->pdev->tx_mutex);
		vdev->prio_q_paused = 0;
		qdf_spin_unlock_bh(&vdev->pdev->tx_mutex);
		break;
	case WLAN_WAKE_NON_PRIORITY_QUEUE:
		qdf_atomic_set(&vdev->os_q_paused, 0);
		break;
	default:
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid action %d", __func__, action);
		return -EINVAL;
	}
	return 0;
}

int ol_txrx_set_vdev_tx_desc_limit(struct cdp_soc_t *soc_hdl, u8 vdev_id,
				   u32 chan_freq)
{
	struct ol_txrx_vdev_t *vdev =
	(struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);
	enum ol_txrx_fc_limit_id fc_limit_id;
	u32 td_limit;

	if (!vdev) {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_ERROR,
			  "%s: Invalid vdev_id %d", __func__, vdev_id);
		return -EINVAL;
	}

	/* TODO: Handle no of spatial streams and channel BW */
	if (WLAN_REG_IS_5GHZ_CH_FREQ(chan_freq))
		fc_limit_id = TXRX_FC_5GH_80M_2x2;
	else
		fc_limit_id = TXRX_FC_2GH_40M_2x2;

	qdf_spin_lock_bh(&vdev->pdev->tx_mutex);
	td_limit = ol_txrx_tx_desc_alloc_table[fc_limit_id];
	vdev->tx_desc_limit = td_limit;
	vdev->queue_stop_th = td_limit - TXRX_HL_TX_DESC_HI_PRIO_RESERVED;
	vdev->queue_restart_th = td_limit - TXRX_HL_TX_DESC_QUEUE_RESTART_TH;
	qdf_spin_unlock_bh(&vdev->pdev->tx_mutex);

	return 0;
}

void ol_tx_dump_flow_pool_info_compact(struct ol_txrx_pdev_t *pdev)
{
	char *comb_log_str;
	int bytes_written = 0;
	uint32_t free_size;
	struct ol_txrx_vdev_t *vdev;
	int i = 0;

	free_size = WLAN_MAX_VDEVS * 100;
	comb_log_str = qdf_mem_malloc(free_size);
	if (!comb_log_str)
		return;

	qdf_spin_lock_bh(&pdev->tx_mutex);
	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		bytes_written += snprintf(&comb_log_str[bytes_written],
				free_size, "%d (%d,%d)(%d,%d)(%d,%d) |",
				vdev->vdev_id, vdev->tx_desc_limit,
				qdf_atomic_read(&vdev->tx_desc_count),
				qdf_atomic_read(&vdev->os_q_paused),
				vdev->prio_q_paused, vdev->queue_stop_th,
				vdev->queue_restart_th);
		free_size -= bytes_written;
	}
	qdf_spin_unlock_bh(&pdev->tx_mutex);
	qdf_nofl_debug("STATS | FC: %s", comb_log_str);

	free_size = WLAN_MAX_VDEVS * 100;
	bytes_written = 0;
	qdf_mem_zero(comb_log_str, free_size);

	bytes_written = snprintf(&comb_log_str[bytes_written], free_size,
				 "%d ",
				 qdf_atomic_read(&pdev->target_tx_credit));
	for (i = 0; i < OL_TX_MAX_TXQ_GROUPS; i++) {
		bytes_written += snprintf(&comb_log_str[bytes_written],
					  free_size, "|%d, (0x%x, %d)", i,
					  OL_TXQ_GROUP_VDEV_ID_MASK_GET(
					  pdev->txq_grps[i].membership),
					  qdf_atomic_read(
					  &pdev->txq_grps[i].credit));
	       free_size -= bytes_written;
	}
	qdf_nofl_debug("STATS | CREDIT: %s", comb_log_str);
	qdf_mem_free(comb_log_str);
}

void ol_tx_dump_flow_pool_info(struct cdp_soc_t *soc_hdl)
{
	struct ol_txrx_soc_t *soc = cdp_soc_t_to_ol_txrx_soc_t(soc_hdl);
	ol_txrx_pdev_handle pdev;
	struct ol_txrx_vdev_t *vdev;

	pdev = ol_txrx_get_pdev_from_pdev_id(soc, OL_TXRX_PDEV_ID);
	if (!pdev) {
		ol_txrx_err("pdev is NULL");
		return;
	}

	qdf_spin_lock_bh(&pdev->tx_mutex);
	TAILQ_FOREACH(vdev, &pdev->vdev_list, vdev_list_elem) {
		txrx_nofl_info("vdev_id %d", vdev->vdev_id);
		txrx_nofl_info("limit %d available %d stop_threshold %d restart_threshold %d",
			       vdev->tx_desc_limit,
			       qdf_atomic_read(&vdev->tx_desc_count),
			       vdev->queue_stop_th, vdev->queue_restart_th);
		txrx_nofl_info("q_paused %d prio_q_paused %d",
			       qdf_atomic_read(&vdev->os_q_paused),
			       vdev->prio_q_paused);
		txrx_nofl_info("no_of_bundle_sent_after_threshold %lld",
			       vdev->no_of_bundle_sent_after_threshold);
		txrx_nofl_info("no_of_bundle_sent_in_timer %lld",
			       vdev->no_of_bundle_sent_in_timer);
		txrx_nofl_info("no_of_pkt_not_added_in_queue %lld",
			       vdev->no_of_pkt_not_added_in_queue);
	}
	qdf_spin_unlock_bh(&pdev->tx_mutex);
}
#endif /* QCA_HL_NETDEV_FLOW_CONTROL */
