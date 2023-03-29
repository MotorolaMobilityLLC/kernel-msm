/*
 * Copyright (c) 2011, 2014-2019-2020 The Linux Foundation. All rights reserved.
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
 * @file htt.c
 * @brief Provide functions to create+init and destroy a HTT instance.
 * @details
 *  This file contains functions for creating a HTT instance; initializing
 *  the HTT instance, e.g. by allocating a pool of HTT tx descriptors and
 *  connecting the HTT service with HTC; and deleting a HTT instance.
 */

#include <qdf_mem.h>         /* qdf_mem_malloc */
#include <qdf_types.h>          /* qdf_device_t, qdf_print */

#include <htt.h>                /* htt_tx_msdu_desc_t */
#include <ol_cfg.h>
#include <ol_txrx_htt_api.h>    /* ol_tx_dowload_done_ll, etc. */
#include <ol_htt_api.h>

#include <htt_internal.h>
#include <ol_htt_tx_api.h>
#include <cds_api.h>
#include "hif.h"
#include <cdp_txrx_handle.h>
#include <ol_txrx_peer_find.h>

#define HTT_HTC_PKT_POOL_INIT_SIZE 100  /* enough for a large A-MPDU */

QDF_STATUS(*htt_h2t_rx_ring_cfg_msg)(struct htt_pdev_t *pdev);
QDF_STATUS(*htt_h2t_rx_ring_rfs_cfg_msg)(struct htt_pdev_t *pdev);

#ifdef IPA_OFFLOAD
static QDF_STATUS htt_ipa_config(htt_pdev_handle pdev, QDF_STATUS status)
{
	if ((QDF_STATUS_SUCCESS == status) &&
	    ol_cfg_ipa_uc_offload_enabled(pdev->ctrl_pdev))
		status = htt_h2t_ipa_uc_rsc_cfg_msg(pdev);
	return status;
}

#define HTT_IPA_CONFIG htt_ipa_config
#else
#define HTT_IPA_CONFIG(pdev, status) status     /* no-op */
#endif /* IPA_OFFLOAD */

struct htt_htc_pkt *htt_htc_pkt_alloc(struct htt_pdev_t *pdev)
{
	struct htt_htc_pkt_union *pkt = NULL;

	HTT_TX_MUTEX_ACQUIRE(&pdev->htt_tx_mutex);
	if (pdev->htt_htc_pkt_freelist) {
		pkt = pdev->htt_htc_pkt_freelist;
		pdev->htt_htc_pkt_freelist = pdev->htt_htc_pkt_freelist->u.next;
	}
	HTT_TX_MUTEX_RELEASE(&pdev->htt_tx_mutex);

	if (!pkt)
		pkt = qdf_mem_malloc(sizeof(*pkt));

	if (!pkt)
		return NULL;

	htc_packet_set_magic_cookie(&(pkt->u.pkt.htc_pkt), 0);
	return &pkt->u.pkt;     /* not actually a dereference */
}

void htt_htc_pkt_free(struct htt_pdev_t *pdev, struct htt_htc_pkt *pkt)
{
	struct htt_htc_pkt_union *u_pkt = (struct htt_htc_pkt_union *)pkt;

	if (!u_pkt) {
		qdf_print("HTC packet is NULL");
		return;
	}

	HTT_TX_MUTEX_ACQUIRE(&pdev->htt_tx_mutex);
	htc_packet_set_magic_cookie(&(u_pkt->u.pkt.htc_pkt), 0);
	u_pkt->u.next = pdev->htt_htc_pkt_freelist;
	pdev->htt_htc_pkt_freelist = u_pkt;
	HTT_TX_MUTEX_RELEASE(&pdev->htt_tx_mutex);
}

void htt_htc_pkt_pool_free(struct htt_pdev_t *pdev)
{
	struct htt_htc_pkt_union *pkt, *next;

	HTT_TX_MUTEX_ACQUIRE(&pdev->htt_tx_mutex);
	pkt = pdev->htt_htc_pkt_freelist;
	pdev->htt_htc_pkt_freelist = NULL;
	HTT_TX_MUTEX_RELEASE(&pdev->htt_tx_mutex);

	while (pkt) {
		next = pkt->u.next;
		qdf_mem_free(pkt);
		pkt = next;
	}
}

#ifdef ATH_11AC_TXCOMPACT

void
htt_htc_misc_pkt_list_trim(struct htt_pdev_t *pdev, int level)
{
	struct htt_htc_pkt_union *pkt, *next, *prev = NULL;
	int i = 0;
	qdf_nbuf_t netbuf;

	HTT_TX_MUTEX_ACQUIRE(&pdev->htt_tx_mutex);
	pkt = pdev->htt_htc_pkt_misclist;
	while (pkt) {
		next = pkt->u.next;
		/* trim the out grown list*/
		if (++i > level) {
			netbuf =
				(qdf_nbuf_t)(pkt->u.pkt.htc_pkt.pNetBufContext);
			qdf_nbuf_unmap(pdev->osdev, netbuf, QDF_DMA_TO_DEVICE);
			qdf_nbuf_free(netbuf);
			qdf_mem_free(pkt);
			pkt = NULL;
			if (prev)
				prev->u.next = NULL;
		}
		prev = pkt;
		pkt = next;
	}
	HTT_TX_MUTEX_RELEASE(&pdev->htt_tx_mutex);
}

void htt_htc_misc_pkt_list_add(struct htt_pdev_t *pdev, struct htt_htc_pkt *pkt)
{
	struct htt_htc_pkt_union *u_pkt = (struct htt_htc_pkt_union *)pkt;
	int misclist_trim_level = htc_get_tx_queue_depth(pdev->htc_pdev,
							pkt->htc_pkt.Endpoint)
				+ HTT_HTC_PKT_MISCLIST_SIZE;

	HTT_TX_MUTEX_ACQUIRE(&pdev->htt_tx_mutex);
	if (pdev->htt_htc_pkt_misclist) {
		u_pkt->u.next = pdev->htt_htc_pkt_misclist;
		pdev->htt_htc_pkt_misclist = u_pkt;
	} else {
		pdev->htt_htc_pkt_misclist = u_pkt;
	}
	HTT_TX_MUTEX_RELEASE(&pdev->htt_tx_mutex);

	/* only ce pipe size + tx_queue_depth could possibly be in use
	 * free older packets in the msiclist
	 */
	htt_htc_misc_pkt_list_trim(pdev, misclist_trim_level);
}

void htt_htc_misc_pkt_pool_free(struct htt_pdev_t *pdev)
{
	struct htt_htc_pkt_union *pkt, *next;
	qdf_nbuf_t netbuf;

	HTT_TX_MUTEX_ACQUIRE(&pdev->htt_tx_mutex);
	pkt = pdev->htt_htc_pkt_misclist;
	pdev->htt_htc_pkt_misclist = NULL;
	HTT_TX_MUTEX_RELEASE(&pdev->htt_tx_mutex);

	while (pkt) {
		next = pkt->u.next;
		if (htc_packet_get_magic_cookie(&(pkt->u.pkt.htc_pkt)) !=
				HTC_PACKET_MAGIC_COOKIE) {
			QDF_ASSERT(0);
			pkt = next;
			continue;
		}

		netbuf = (qdf_nbuf_t) (pkt->u.pkt.htc_pkt.pNetBufContext);
		qdf_nbuf_unmap(pdev->osdev, netbuf, QDF_DMA_TO_DEVICE);
		qdf_nbuf_free(netbuf);
		qdf_mem_free(pkt);
		pkt = next;
	}
}
#endif


/* AR6004 don't need HTT layer. */
#ifdef AR6004_HW
#define NO_HTT_NEEDED true
#else
#define NO_HTT_NEEDED false
#endif

#if defined(QCA_TX_HTT2_SUPPORT) && defined(CONFIG_HL_SUPPORT)

/**
 * htt_htc_tx_htt2_service_start() - Start TX HTT2 service
 *
 * @pdev: pointer to htt device.
 * @connect_req: pointer to service connection request information
 * @connect_resp: pointer to service connection response information
 *
 *
 * Return: None
 */
static void
htt_htc_tx_htt2_service_start(struct htt_pdev_t *pdev,
			      struct htc_service_connect_req *connect_req,
			      struct htc_service_connect_resp *connect_resp)
{
	QDF_STATUS status;

	qdf_mem_zero(connect_req, sizeof(struct htc_service_connect_req));
	qdf_mem_zero(connect_resp, sizeof(struct htc_service_connect_resp));

	/* The same as HTT service but no RX. */
	connect_req->EpCallbacks.pContext = pdev;
	connect_req->EpCallbacks.EpTxComplete = htt_h2t_send_complete;
	connect_req->EpCallbacks.EpSendFull = htt_h2t_full;
	connect_req->MaxSendQueueDepth = HTT_MAX_SEND_QUEUE_DEPTH;
	/* Should NOT support credit flow control. */
	connect_req->ConnectionFlags |=
				HTC_CONNECT_FLAGS_DISABLE_CREDIT_FLOW_CTRL;
	/* Enable HTC schedule mechanism for TX HTT2 service. */
	connect_req->ConnectionFlags |= HTC_CONNECT_FLAGS_ENABLE_HTC_SCHEDULE;

	connect_req->service_id = HTT_DATA2_MSG_SVC;

	status = htc_connect_service(pdev->htc_pdev, connect_req, connect_resp);

	if (status != QDF_STATUS_SUCCESS) {
		pdev->htc_tx_htt2_endpoint = ENDPOINT_UNUSED;
		pdev->htc_tx_htt2_max_size = 0;
	} else {
		pdev->htc_tx_htt2_endpoint = connect_resp->Endpoint;
		pdev->htc_tx_htt2_max_size = HTC_TX_HTT2_MAX_SIZE;
	}

	qdf_print("TX HTT %s, ep %d size %d\n",
		  (status == QDF_STATUS_SUCCESS ? "ON" : "OFF"),
		  pdev->htc_tx_htt2_endpoint,
		  pdev->htc_tx_htt2_max_size);
}
#else

static inline void
htt_htc_tx_htt2_service_start(struct htt_pdev_t *pdev,
			      struct htc_service_connect_req *connect_req,
			      struct htc_service_connect_resp *connect_resp)
{
}
#endif

/**
 * htt_htc_credit_flow_disable() - disable flow control for
 *				   HTT data message service
 *
 * @pdev: pointer to htt device.
 * @connect_req: pointer to service connection request information
 *
 * HTC Credit mechanism is disabled based on
 * default_tx_comp_req as throughput will be lower
 * if we disable htc credit mechanism with default_tx_comp_req
 * set since txrx download packet will be limited by ota
 * completion.
 *
 * Return: None
 */
static
void htt_htc_credit_flow_disable(struct htt_pdev_t *pdev,
				 struct htc_service_connect_req *connect_req)
{
	if (pdev->osdev->bus_type == QDF_BUS_TYPE_SDIO) {
		/*
		 * TODO:Conditional disabling will be removed once firmware
		 * with reduced tx completion is pushed into release builds.
		 */
		if (!pdev->cfg.default_tx_comp_req)
			connect_req->ConnectionFlags |=
			HTC_CONNECT_FLAGS_DISABLE_CREDIT_FLOW_CTRL;
	} else {
		connect_req->ConnectionFlags |=
			HTC_CONNECT_FLAGS_DISABLE_CREDIT_FLOW_CTRL;
	}
}

#if defined(DEBUG_HL_LOGGING) && defined(CONFIG_HL_SUPPORT)

/**
 * htt_dump_bundle_stats() - dump wlan stats
 * @pdev: handle to the HTT instance
 *
 * Return: None
 */
void htt_dump_bundle_stats(htt_pdev_handle pdev)
{
	htc_dump_bundle_stats(pdev->htc_pdev);
}

/**
 * htt_clear_bundle_stats() - clear wlan stats
 * @pdev: handle to the HTT instance
 *
 * Return: None
 */
void htt_clear_bundle_stats(htt_pdev_handle pdev)
{
	htc_clear_bundle_stats(pdev->htc_pdev);
}
#endif

#if defined(QCA_WIFI_3_0_ADRASTEA)
/**
 * htt_htc_attach_all() - Connect to HTC service for HTT
 * @pdev: pdev ptr
 *
 * Return: 0 for success or error code.
 */

#if defined(QCN7605_SUPPORT) && defined(IPA_OFFLOAD)

/* In case of QCN7605 with IPA offload only 2 CE
 * are used for RFS
 */
static int
htt_htc_attach_all(struct htt_pdev_t *pdev)
{
	if (htt_htc_attach(pdev, HTT_DATA_MSG_SVC))
		goto flush_endpoint;

	if (htt_htc_attach(pdev, HTT_DATA2_MSG_SVC))
		goto flush_endpoint;

	return 0;

flush_endpoint:
	htc_flush_endpoint(pdev->htc_pdev, ENDPOINT_0, HTC_TX_PACKET_TAG_ALL);

	return -EIO;
}

#else

static int
htt_htc_attach_all(struct htt_pdev_t *pdev)
{
	if (htt_htc_attach(pdev, HTT_DATA_MSG_SVC))
		goto flush_endpoint;

	if (htt_htc_attach(pdev, HTT_DATA2_MSG_SVC))
		goto flush_endpoint;

	if (htt_htc_attach(pdev, HTT_DATA3_MSG_SVC))
		goto flush_endpoint;

	return 0;

flush_endpoint:
	htc_flush_endpoint(pdev->htc_pdev, ENDPOINT_0, HTC_TX_PACKET_TAG_ALL);

	return -EIO;
}

#endif

#else
/**
 * htt_htc_attach_all() - Connect to HTC service for HTT
 * @pdev: pdev ptr
 *
 * Return: 0 for success or error code.
 */
static int
htt_htc_attach_all(struct htt_pdev_t *pdev)
{
	return htt_htc_attach(pdev, HTT_DATA_MSG_SVC);
}
#endif

/**
 * htt_pdev_alloc() - allocate HTT pdev
 * @txrx_pdev: txrx pdev
 * @ctrl_pdev: cfg pdev
 * @htc_pdev: HTC pdev
 * @osdev: os device
 *
 * Return: HTT pdev handle
 */
htt_pdev_handle
htt_pdev_alloc(ol_txrx_pdev_handle txrx_pdev,
	   struct cdp_cfg *ctrl_pdev,
	   HTC_HANDLE htc_pdev, qdf_device_t osdev)
{
	struct htt_pdev_t *pdev;
	struct hif_opaque_softc *osc =  cds_get_context(QDF_MODULE_ID_HIF);

	if (!osc)
		goto fail1;

	pdev = qdf_mem_malloc(sizeof(*pdev));
	if (!pdev)
		goto fail1;

	pdev->osdev = osdev;
	pdev->ctrl_pdev = ctrl_pdev;
	pdev->txrx_pdev = txrx_pdev;
	pdev->htc_pdev = htc_pdev;

	pdev->htt_htc_pkt_freelist = NULL;
#ifdef ATH_11AC_TXCOMPACT
	pdev->htt_htc_pkt_misclist = NULL;
#endif

	/* for efficiency, store a local copy of the is_high_latency flag */
	pdev->cfg.is_high_latency = ol_cfg_is_high_latency(pdev->ctrl_pdev);
	/*
	 * Credit reporting through HTT_T2H_MSG_TYPE_TX_CREDIT_UPDATE_IND
	 * enabled or not.
	 */
	pdev->cfg.credit_update_enabled =
		ol_cfg_is_credit_update_enabled(pdev->ctrl_pdev);

	pdev->cfg.request_tx_comp = cds_is_ptp_rx_opt_enabled() ||
		cds_is_packet_log_enabled();

	pdev->cfg.default_tx_comp_req =
			!ol_cfg_tx_free_at_download(pdev->ctrl_pdev);

	pdev->cfg.is_full_reorder_offload =
			ol_cfg_is_full_reorder_offload(pdev->ctrl_pdev);
	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO_LOW,
		  "full_reorder_offloaded %d",
		  (int)pdev->cfg.is_full_reorder_offload);

	pdev->cfg.ce_classify_enabled =
		ol_cfg_is_ce_classify_enabled(ctrl_pdev);
	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO_LOW,
		  "ce_classify %d",
		  pdev->cfg.ce_classify_enabled);

	if (pdev->cfg.is_high_latency) {
		qdf_atomic_init(&pdev->htt_tx_credit.target_delta);
		qdf_atomic_init(&pdev->htt_tx_credit.bus_delta);
		qdf_atomic_add(HTT_MAX_BUS_CREDIT,
			       &pdev->htt_tx_credit.bus_delta);
	}

	pdev->targetdef = htc_get_targetdef(htc_pdev);
#if defined(HELIUMPLUS)
	HTT_SET_WIFI_IP(pdev, 2, 0);
#endif /* defined(HELIUMPLUS) */

	if (NO_HTT_NEEDED)
		goto success;
	/*
	 * Connect to HTC service.
	 * This has to be done before calling htt_rx_attach,
	 * since htt_rx_attach involves sending a rx ring configure
	 * message to the target.
	 */
	HTT_TX_MUTEX_INIT(&pdev->htt_tx_mutex);
	HTT_TX_NBUF_QUEUE_MUTEX_INIT(pdev);
	HTT_TX_MUTEX_INIT(&pdev->credit_mutex);
	if (htt_htc_attach_all(pdev))
		goto htt_htc_attach_fail;
	if (hif_ce_fastpath_cb_register(osc, htt_t2h_msg_handler_fast, pdev))
		qdf_print("failed to register fastpath callback\n");

success:
	return pdev;

htt_htc_attach_fail:
	HTT_TX_MUTEX_DESTROY(&pdev->credit_mutex);
	HTT_TX_MUTEX_DESTROY(&pdev->htt_tx_mutex);
	HTT_TX_NBUF_QUEUE_MUTEX_DESTROY(pdev);
	qdf_mem_free(pdev);

fail1:
	return NULL;

}

/**
 * htt_attach() - Allocate and setup HTT TX/RX descriptors
 * @pdev: pdev ptr
 * @desc_pool_size: size of tx descriptors
 *
 * Return: 0 for success or error code.
 */
int
htt_attach(struct htt_pdev_t *pdev, int desc_pool_size)
{
	int i;
	int ret = 0;

	pdev->is_ipa_uc_enabled = false;
	if (ol_cfg_ipa_uc_offload_enabled(pdev->ctrl_pdev))
		pdev->is_ipa_uc_enabled = true;

	pdev->new_htt_format_enabled = false;
	if (ol_cfg_is_htt_new_format_enabled(pdev->ctrl_pdev))
		pdev->new_htt_format_enabled = true;

	htc_enable_hdr_length_check(pdev->htc_pdev,
				    pdev->new_htt_format_enabled);

	ret = htt_tx_attach(pdev, desc_pool_size);
	if (ret)
		goto fail1;

	ret = htt_rx_attach(pdev);
	if (ret)
		goto fail2;

	/* pre-allocate some HTC_PACKET objects */
	for (i = 0; i < HTT_HTC_PKT_POOL_INIT_SIZE; i++) {
		struct htt_htc_pkt_union *pkt;

		pkt = qdf_mem_malloc(sizeof(*pkt));
		if (!pkt)
			break;
		htt_htc_pkt_free(pdev, &pkt->u.pkt);
	}

	if (pdev->cfg.is_high_latency) {
		/*
		 * HL - download the whole frame.
		 * Specify a download length greater than the max MSDU size,
		 * so the downloads will be limited by the actual frame sizes.
		 */
		pdev->download_len = 5000;

		if (ol_cfg_tx_free_at_download(pdev->ctrl_pdev) &&
		    !pdev->cfg.request_tx_comp)
			pdev->tx_send_complete_part2 =
						ol_tx_download_done_hl_free;
		else
			pdev->tx_send_complete_part2 =
						ol_tx_download_done_hl_retain;

		/*
		 * CHECK THIS LATER: does the HL HTT version of
		 * htt_rx_mpdu_desc_list_next
		 * (which is not currently implemented) present the
		 * adf_nbuf_data(rx_ind_msg)
		 * as the abstract rx descriptor?
		 * If not, the rx_fw_desc_offset initialization
		 * here will have to be adjusted accordingly.
		 * NOTE: for HL, because fw rx desc is in ind msg,
		 * not in rx desc, so the
		 * offset should be negtive value
		 */
		pdev->rx_fw_desc_offset =
			HTT_ENDIAN_BYTE_IDX_SWAP(
					HTT_RX_IND_FW_RX_DESC_BYTE_OFFSET
					- HTT_RX_IND_HL_BYTES);

		htt_h2t_rx_ring_cfg_msg = htt_h2t_rx_ring_cfg_msg_hl;
		htt_h2t_rx_ring_rfs_cfg_msg = htt_h2t_rx_ring_rfs_cfg_msg_hl;

		/* initialize the txrx credit count */
		ol_tx_target_credit_update(
				pdev->txrx_pdev, ol_cfg_target_tx_credit(
					pdev->ctrl_pdev));
		DPTRACE(qdf_dp_trace_credit_record(QDF_HTT_ATTACH,
			QDF_CREDIT_INC,
			ol_cfg_target_tx_credit(pdev->ctrl_pdev),
			qdf_atomic_read(&pdev->txrx_pdev->target_tx_credit),
			qdf_atomic_read(&pdev->txrx_pdev->txq_grps[0].credit),
			qdf_atomic_read(&pdev->txrx_pdev->txq_grps[1].credit)));

	} else {
		enum wlan_frm_fmt frm_type;

		/*
		 * LL - download just the initial portion of the frame.
		 * Download enough to cover the encapsulation headers checked
		 * by the target's tx classification descriptor engine.
		 *
		 * For LL, the FW rx desc directly referenced at its location
		 * inside the rx indication message.
		 */

		/* account for the 802.3 or 802.11 header */
		frm_type = ol_cfg_frame_type(pdev->ctrl_pdev);

		if (frm_type == wlan_frm_fmt_native_wifi) {
			pdev->download_len = HTT_TX_HDR_SIZE_NATIVE_WIFI;
		} else if (frm_type == wlan_frm_fmt_802_3) {
			pdev->download_len = HTT_TX_HDR_SIZE_ETHERNET;
		} else {
			QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
				  "Unexpected frame type spec: %d", frm_type);
			HTT_ASSERT0(0);
		}

		/*
		 * Account for the optional L2 / ethernet header fields:
		 * 802.1Q, LLC/SNAP
		 */
		pdev->download_len +=
			HTT_TX_HDR_SIZE_802_1Q + HTT_TX_HDR_SIZE_LLC_SNAP;

		/*
		 * Account for the portion of the L3 (IP) payload that the
		 * target needs for its tx classification.
		 */
		pdev->download_len += ol_cfg_tx_download_size(pdev->ctrl_pdev);

		/*
		 * Account for the HTT tx descriptor, including the
		 * HTC header + alignment padding.
		 */
		pdev->download_len += sizeof(struct htt_host_tx_desc_t);

		/*
		 * The TXCOMPACT htt_tx_sched function uses pdev->download_len
		 * to apply for all requeued tx frames.  Thus,
		 * pdev->download_len has to be the largest download length of
		 * any tx frame that will be downloaded.
		 * This maximum download length is for management tx frames,
		 * which have an 802.11 header.
		 */
#ifdef ATH_11AC_TXCOMPACT
		pdev->download_len = sizeof(struct htt_host_tx_desc_t)
			+ HTT_TX_HDR_SIZE_OUTER_HDR_MAX /* worst case */
			+ HTT_TX_HDR_SIZE_802_1Q
			+ HTT_TX_HDR_SIZE_LLC_SNAP
			+ ol_cfg_tx_download_size(pdev->ctrl_pdev);
#endif
		pdev->tx_send_complete_part2 = ol_tx_download_done_ll;

		/*
		 * For LL, the FW rx desc is alongside the HW rx desc fields in
		 * the htt_host_rx_desc_base struct/.
		 */
		pdev->rx_fw_desc_offset = RX_STD_DESC_FW_MSDU_OFFSET;

		htt_h2t_rx_ring_cfg_msg = htt_h2t_rx_ring_cfg_msg_ll;
		htt_h2t_rx_ring_rfs_cfg_msg = htt_h2t_rx_ring_rfs_cfg_msg_ll;
	}

	return 0;

fail2:
	htt_tx_detach(pdev);

fail1:
	return ret;
}

QDF_STATUS htt_attach_target(htt_pdev_handle pdev)
{
	QDF_STATUS status;

	status = htt_h2t_ver_req_msg(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s:%d: could not send h2t_ver_req msg",
			  __func__, __LINE__);
		return status;
	}
#if defined(HELIUMPLUS)
	/*
	 * Send the frag_desc info to target.
	 */
	status = htt_h2t_frag_desc_bank_cfg_msg(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s:%d: could not send h2t_frag_desc_bank_cfg msg",
			  __func__, __LINE__);
		return status;
	}
#endif /* defined(HELIUMPLUS) */


	/*
	 * If applicable, send the rx ring config message to the target.
	 * The host could wait for the HTT version number confirmation message
	 * from the target before sending any further HTT messages, but it's
	 * reasonable to assume that the host and target HTT version numbers
	 * match, and proceed immediately with the remaining configuration
	 * handshaking.
	 */

	status = htt_h2t_rx_ring_rfs_cfg_msg(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s:%d: could not send h2t_rx_ring_rfs_cfg msg",
			  __func__, __LINE__);
		return status;
	}

	status = htt_h2t_rx_ring_cfg_msg(pdev);
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s:%d: could not send h2t_rx_ring_cfg msg",
			  __func__, __LINE__);
		return status;
	}

	status = HTT_IPA_CONFIG(pdev, status);
	if (status != QDF_STATUS_SUCCESS) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s:%d: could not send h2t_ipa_uc_rsc_cfg msg",
			  __func__, __LINE__);
		return status;
	}

	return status;
}

void htt_detach(htt_pdev_handle pdev)
{
	htt_rx_detach(pdev);
	htt_tx_detach(pdev);
	htt_htc_pkt_pool_free(pdev);
#ifdef ATH_11AC_TXCOMPACT
	htt_htc_misc_pkt_pool_free(pdev);
#endif
	HTT_TX_MUTEX_DESTROY(&pdev->credit_mutex);
	HTT_TX_MUTEX_DESTROY(&pdev->htt_tx_mutex);
	HTT_TX_NBUF_QUEUE_MUTEX_DESTROY(pdev);
}

/**
 * htt_pdev_free() - Free HTT pdev
 * @pdev: htt pdev
 *
 * Return: none
 */
void htt_pdev_free(htt_pdev_handle pdev)
{
	qdf_mem_free(pdev);
}

void htt_detach_target(htt_pdev_handle pdev)
{
}

static inline
int htt_update_endpoint(struct htt_pdev_t *pdev,
			uint16_t service_id, HTC_ENDPOINT_ID ep)
{
	struct hif_opaque_softc *hif_ctx;
	uint8_t ul = 0xff, dl = 0xff;
	int     ul_polled, dl_polled;
	int     tx_service = 0;
	int     rc = 0;

	hif_ctx = cds_get_context(QDF_MODULE_ID_HIF);
	if (qdf_unlikely(!hif_ctx)) {
		QDF_ASSERT(hif_ctx);
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s:%d: assuming non-tx service.",
			  __func__, __LINE__);
	} else {
		ul = dl = 0xff;
		if (QDF_STATUS_SUCCESS !=
		    hif_map_service_to_pipe(hif_ctx, service_id,
					    &ul, &dl,
					    &ul_polled, &dl_polled))
			QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO,
				  "%s:%d: assuming non-tx srv.",
				  __func__, __LINE__);
		else
			tx_service = (ul != 0xff);
	}
	if (tx_service) {
		/* currently we have only one OUT htt tx service */
		QDF_BUG(service_id == HTT_DATA_MSG_SVC);

		pdev->htc_tx_endpoint = ep;
		hif_save_htc_htt_config_endpoint(hif_ctx, ep);
		rc = 1;
	}
	return rc;
}

int htt_htc_attach(struct htt_pdev_t *pdev, uint16_t service_id)
{
	struct htc_service_connect_req connect;
	struct htc_service_connect_resp response;
	QDF_STATUS status;

	qdf_mem_zero(&connect, sizeof(connect));
	qdf_mem_zero(&response, sizeof(response));

	connect.pMetaData = NULL;
	connect.MetaDataLength = 0;
	connect.EpCallbacks.pContext = pdev;
	connect.EpCallbacks.EpTxComplete = htt_h2t_send_complete;
	connect.EpCallbacks.EpTxCompleteMultiple = NULL;
	connect.EpCallbacks.EpRecv = htt_t2h_msg_handler;
	connect.EpCallbacks.ep_resume_tx_queue = htt_tx_resume_handler;
	connect.EpCallbacks.ep_padding_credit_update =
					htt_tx_padding_credit_update_handler;

	/* rx buffers currently are provided by HIF, not by EpRecvRefill */
	connect.EpCallbacks.EpRecvRefill = NULL;
	connect.EpCallbacks.RecvRefillWaterMark = 1;
	/* N/A, fill is done by HIF */

	connect.EpCallbacks.EpSendFull = htt_h2t_full;
	/*
	 * Specify how deep to let a queue get before htc_send_pkt will
	 * call the EpSendFull function due to excessive send queue depth.
	 */
	connect.MaxSendQueueDepth = HTT_MAX_SEND_QUEUE_DEPTH;

	/* disable flow control for HTT data message service */
	htt_htc_credit_flow_disable(pdev, &connect);

	/* connect to control service */
	connect.service_id = service_id;

	status = htc_connect_service(pdev->htc_pdev, &connect, &response);

	if (status != QDF_STATUS_SUCCESS) {
		if (cds_is_fw_down())
			return -EIO;

		if (status == QDF_STATUS_E_NOMEM ||
		    cds_is_self_recovery_enabled())
			return qdf_status_to_os_return(status);

		QDF_BUG(0);
	}

	htt_update_endpoint(pdev, service_id, response.Endpoint);

	/* Start TX HTT2 service if the target support it. */
	htt_htc_tx_htt2_service_start(pdev, &connect, &response);

	return 0;               /* success */
}

void htt_log_rx_ring_info(htt_pdev_handle pdev)
{
	if (!pdev) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s: htt pdev is NULL", __func__);
		return;
	}
	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_DEBUG,
		  "%s: Data Stall Detected with reason 4 (=FW_RX_REFILL_FAILED)."
		  "src htt rx ring:  space for %d elements, filled with %d buffers, buffers in the ring %d, refill debt %d",
		  __func__, pdev->rx_ring.size, pdev->rx_ring.fill_level,
		  qdf_atomic_read(&pdev->rx_ring.fill_cnt),
		  qdf_atomic_read(&pdev->rx_ring.refill_debt));
}

void htt_rx_refill_failure(htt_pdev_handle pdev)
{
	QDF_BUG(qdf_atomic_read(&pdev->rx_ring.refill_debt));
}

#if HTT_DEBUG_LEVEL > 5
void htt_display(htt_pdev_handle pdev, int indent)
{
	qdf_print("%*s%s:\n", indent, " ", "HTT");
	qdf_print("%*stx desc pool: %d elems of %d bytes, %d allocated\n",
		  indent + 4, " ",
		  pdev->tx_descs.pool_elems,
		  pdev->tx_descs.size, pdev->tx_descs.alloc_cnt);
	qdf_print("%*srx ring: space for %d elems, filled with %d buffers\n",
		  indent + 4, " ",
		  pdev->rx_ring.size, pdev->rx_ring.fill_level);
	qdf_print("%*sat %pK (%llx paddr)\n", indent + 8, " ",
		  pdev->rx_ring.buf.paddrs_ring,
		  (unsigned long long)pdev->rx_ring.base_paddr);
	qdf_print("%*snetbuf ring @ %pK\n", indent + 8, " ",
		  pdev->rx_ring.buf.netbufs_ring);
	qdf_print("%*sFW_IDX shadow register: vaddr = %pK, paddr = %llx\n",
		  indent + 8, " ",
		  pdev->rx_ring.alloc_idx.vaddr,
		  (unsigned long long)pdev->rx_ring.alloc_idx.paddr);
	qdf_print("%*sSW enqueue idx= %d, SW dequeue idx: desc= %d, buf= %d\n",
		  indent + 8, " ", *pdev->rx_ring.alloc_idx.vaddr,
		  pdev->rx_ring.sw_rd_idx.msdu_desc,
		  pdev->rx_ring.sw_rd_idx.msdu_payld);
}
#endif

#ifdef IPA_OFFLOAD
/**
 * htt_ipa_uc_attach() - Allocate UC data path resources
 * @pdev: handle to the HTT instance
 *
 * Return: 0 success
 *         none 0 fail
 */
int htt_ipa_uc_attach(struct htt_pdev_t *pdev)
{
	int error;

	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_DEBUG, "%s: enter",
		  __func__);

	/* TX resource attach */
	error = htt_tx_ipa_uc_attach(
		pdev,
		ol_cfg_ipa_uc_tx_buf_size(pdev->ctrl_pdev),
		ol_cfg_ipa_uc_tx_max_buf_cnt(pdev->ctrl_pdev),
		ol_cfg_ipa_uc_tx_partition_base(pdev->ctrl_pdev));
	if (error) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "HTT IPA UC TX attach fail code %d", error);
		HTT_ASSERT0(0);
		return error;
	}

	/* RX resource attach */
	error = htt_rx_ipa_uc_attach(
		pdev, qdf_get_pwr2(pdev->rx_ring.fill_level));
	if (error) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "HTT IPA UC RX attach fail code %d", error);
		htt_tx_ipa_uc_detach(pdev);
		HTT_ASSERT0(0);
		return error;
	}

	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_DEBUG, "%s: exit",
		__func__);
	return 0;               /* success */
}

/**
 * htt_ipa_uc_attach() - Remove UC data path resources
 * @pdev: handle to the HTT instance
 *
 * Return: None
 */
void htt_ipa_uc_detach(struct htt_pdev_t *pdev)
{
	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_DEBUG, "%s: enter",
		__func__);

	/* TX IPA micro controller detach */
	htt_tx_ipa_uc_detach(pdev);

	/* RX IPA micro controller detach */
	htt_rx_ipa_uc_detach(pdev);

	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_DEBUG, "%s: exit",
		__func__);
}

int
htt_ipa_uc_get_resource(htt_pdev_handle pdev,
			qdf_shared_mem_t **ce_sr,
			qdf_shared_mem_t **tx_comp_ring,
			qdf_shared_mem_t **rx_rdy_ring,
			qdf_shared_mem_t **rx2_rdy_ring,
			qdf_shared_mem_t **rx_proc_done_idx,
			qdf_shared_mem_t **rx2_proc_done_idx,
			uint32_t *ce_sr_ring_size,
			qdf_dma_addr_t *ce_reg_paddr,
			uint32_t *tx_num_alloc_buffer)
{
	/* Release allocated resource to client */
	*tx_comp_ring = pdev->ipa_uc_tx_rsc.tx_comp_ring;
	*rx_rdy_ring = pdev->ipa_uc_rx_rsc.rx_ind_ring;
	*rx2_rdy_ring = pdev->ipa_uc_rx_rsc.rx2_ind_ring;
	*rx_proc_done_idx = pdev->ipa_uc_rx_rsc.rx_ipa_prc_done_idx;
	*rx2_proc_done_idx = pdev->ipa_uc_rx_rsc.rx2_ipa_prc_done_idx;
	*tx_num_alloc_buffer = (uint32_t)pdev->ipa_uc_tx_rsc.alloc_tx_buf_cnt;

	/* Get copy engine, bus resource */
	htc_ipa_get_ce_resource(pdev->htc_pdev, ce_sr,
				ce_sr_ring_size, ce_reg_paddr);

	return 0;
}

/**
 * htt_ipa_uc_set_doorbell_paddr() - Propagate IPA doorbell address
 * @pdev: handle to the HTT instance
 * @ipa_uc_tx_doorbell_paddr: TX doorbell base physical address
 * @ipa_uc_rx_doorbell_paddr: RX doorbell base physical address
 *
 * Return: 0 success
 */
int
htt_ipa_uc_set_doorbell_paddr(htt_pdev_handle pdev,
			      qdf_dma_addr_t ipa_uc_tx_doorbell_paddr,
			      qdf_dma_addr_t ipa_uc_rx_doorbell_paddr)
{
	pdev->ipa_uc_tx_rsc.tx_comp_idx_paddr = ipa_uc_tx_doorbell_paddr;
	pdev->ipa_uc_rx_rsc.rx_rdy_idx_paddr = ipa_uc_rx_doorbell_paddr;
	return 0;
}
#endif /* IPA_OFFLOAD */

/**
 * htt_mark_first_wakeup_packet() - set flag to indicate that
 *    fw is compatible for marking first packet after wow wakeup
 * @pdev: pointer to htt pdev
 * @value: 1 for enabled/ 0 for disabled
 *
 * Return: None
 */
void htt_mark_first_wakeup_packet(htt_pdev_handle pdev,
			uint8_t value)
{
	if (!pdev) {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%s: htt pdev is NULL", __func__);
		return;
	}

	pdev->cfg.is_first_wakeup_packet = value;
}

