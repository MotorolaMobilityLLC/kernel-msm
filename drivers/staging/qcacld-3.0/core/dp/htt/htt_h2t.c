/*
 * Copyright (c) 2011-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * @file htt_h2t.c
 * @brief Provide functions to send host->target HTT messages.
 * @details
 *  This file contains functions related to host->target HTT messages.
 *  There are a couple aspects of this host->target messaging:
 *  1.  This file contains the function that is called by HTC when
 *      a host->target send completes.
 *      This send-completion callback is primarily relevant to HL,
 *      to invoke the download scheduler to set up a new download,
 *      and optionally free the tx frame whose download is completed.
 *      For both HL and LL, this completion callback frees up the
 *      HTC_PACKET object used to specify the download.
 *  2.  This file contains functions for creating messages to send
 *      from the host to the target.
 */

#include <qdf_mem.h>         /* qdf_mem_copy */
#include <qdf_nbuf.h>           /* qdf_nbuf_map_single */
#include <htc_api.h>            /* HTC_PACKET */
#include <htc.h>                /* HTC_HDR_ALIGNMENT_PADDING */
#include <htt.h>                /* HTT host->target msg defs */
#include <wdi_ipa.h>            /* HTT host->target WDI IPA msg defs */
#include <ol_txrx_htt_api.h>    /* ol_tx_completion_handler, htt_tx_status */
#include <ol_htt_tx_api.h>
#include <ol_txrx_types.h>
#include <ol_tx_send.h>
#include <ol_htt_rx_api.h>

#include <htt_internal.h>
#include <wlan_policy_mgr_api.h>

#define HTT_MSG_BUF_SIZE(msg_bytes) \
	((msg_bytes) + HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING)

#ifndef container_of
#define container_of(ptr, type, member) \
	((type *)((char *)(ptr) - (char *)(&((type *)0)->member)))
#endif

#ifdef ATH_11AC_TXCOMPACT
#define HTT_SEND_HTC_PKT(pdev, pkt)                              \
do {                                                             \
	if (htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt) ==       \
	    QDF_STATUS_SUCCESS) {                                \
		htt_htc_misc_pkt_list_add(pdev, pkt);            \
	} else {                                                 \
		qdf_nbuf_free((qdf_nbuf_t)(pkt->htc_pkt.pNetBufContext));   \
		htt_htc_pkt_free(pdev, pkt);                     \
	}                                                        \
} while (0)
#else
#define HTT_SEND_HTC_PKT(pdev, ppkt) \
	htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif


static void
htt_h2t_send_complete_free_netbuf(void *pdev, QDF_STATUS status,
				  qdf_nbuf_t netbuf, uint16_t msdu_id)
{
	qdf_nbuf_free(netbuf);
}

#ifndef QCN7605_SUPPORT
static void htt_t2h_adjust_bus_target_delta(struct htt_pdev_t *pdev)
{
	int32_t credit_delta;

	if (pdev->cfg.is_high_latency && !pdev->cfg.default_tx_comp_req) {
		HTT_TX_MUTEX_ACQUIRE(&pdev->credit_mutex);
		qdf_atomic_add(1, &pdev->htt_tx_credit.bus_delta);
		credit_delta = htt_tx_credit_update(pdev);
		HTT_TX_MUTEX_RELEASE(&pdev->credit_mutex);

		if (credit_delta)
			ol_tx_credit_completion_handler(pdev->txrx_pdev,
							credit_delta);
	}
}
#else
static void htt_t2h_adjust_bus_target_delta(struct htt_pdev_t *pdev)
{
	/* UNPAUSE OS Q */
	ol_tx_flow_ct_unpause_os_q(pdev->txrx_pdev);
}
#endif

void htt_h2t_send_complete(void *context, HTC_PACKET *htc_pkt)
{
	void (*send_complete_part2)(void *pdev, QDF_STATUS status,
				    qdf_nbuf_t msdu, uint16_t msdu_id);
	struct htt_pdev_t *pdev = (struct htt_pdev_t *)context;
	struct htt_htc_pkt *htt_pkt;
	qdf_nbuf_t netbuf;

	send_complete_part2 = htc_pkt->pPktContext;

	htt_pkt = container_of(htc_pkt, struct htt_htc_pkt, htc_pkt);

	/* process (free or keep) the netbuf that held the message */
	netbuf = (qdf_nbuf_t) htc_pkt->pNetBufContext;
	if (send_complete_part2) {
		send_complete_part2(htt_pkt->pdev_ctxt, htc_pkt->Status, netbuf,
				    htt_pkt->msdu_id);
	}

	htt_t2h_adjust_bus_target_delta(pdev);
	/* free the htt_htc_pkt / HTC_PACKET object */
	htt_htc_pkt_free(pdev, htt_pkt);
}

enum htc_send_full_action htt_h2t_full(void *context, HTC_PACKET *pkt)
{
/* FIX THIS */
	return HTC_SEND_FULL_KEEP;
}

#if defined(HELIUMPLUS)
QDF_STATUS htt_h2t_frag_desc_bank_cfg_msg(struct htt_pdev_t *pdev)
{
	QDF_STATUS rc = QDF_STATUS_SUCCESS;

	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	u_int32_t *msg_word;
	struct htt_tx_frag_desc_bank_cfg_t *bank_cfg;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return QDF_STATUS_E_FAILURE; /* failure */

	/* show that this is not a tx frame download
	 * (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL; /* not used during send-done callback */

	msg = qdf_nbuf_alloc(
		pdev->osdev,
		HTT_MSG_BUF_SIZE(sizeof(struct htt_tx_frag_desc_bank_cfg_t)),
		/* reserve room for the HTC header */
		HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, true);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return QDF_STATUS_E_FAILURE; /* failure */
	}

	/*
	 * Set the length of the message.
	 * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
	 * separately during the below call to adf_nbuf_push_head.
	 * The contribution from the HTC header is added separately inside HTC.
	 */
	qdf_nbuf_put_tail(msg, sizeof(struct htt_tx_frag_desc_bank_cfg_t));

	/* fill in the message contents */
	msg_word = (u_int32_t *) qdf_nbuf_data(msg);

	memset(msg_word, 0, sizeof(struct htt_tx_frag_desc_bank_cfg_t));
	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_FRAG_DESC_BANK_CFG);

	bank_cfg = (struct htt_tx_frag_desc_bank_cfg_t *)msg_word;

	/** @note @todo Hard coded to 0 Assuming just one pdev for now.*/
	HTT_H2T_FRAG_DESC_BANK_PDEVID_SET(*msg_word, 0);
	/** @note Hard coded to 1.*/
	HTT_H2T_FRAG_DESC_BANK_NUM_BANKS_SET(*msg_word, 1);
	HTT_H2T_FRAG_DESC_BANK_DESC_SIZE_SET(*msg_word, pdev->frag_descs.size);
	HTT_H2T_FRAG_DESC_BANK_SWAP_SET(*msg_word, 0);

	/** Bank specific data structure.*/
#if HTT_PADDR64
	bank_cfg->bank_base_address[0].lo = qdf_get_lower_32_bits(
			pdev->frag_descs.desc_pages.dma_pages->page_p_addr);
	bank_cfg->bank_base_address[0].hi = qdf_get_upper_32_bits(
			pdev->frag_descs.desc_pages.dma_pages->page_p_addr);
#else /* ! HTT_PADDR64 */
	bank_cfg->bank_base_address[0] =
		pdev->frag_descs.desc_pages.dma_pages->page_p_addr;
#endif /* HTT_PADDR64 */
	/* Logical Min index */
	HTT_H2T_FRAG_DESC_BANK_MIN_IDX_SET(bank_cfg->bank_info[0], 0);
	/* Logical Max index */
	HTT_H2T_FRAG_DESC_BANK_MAX_IDX_SET(bank_cfg->bank_info[0],
					   pdev->frag_descs.pool_elems-1);

	SET_HTC_PACKET_INFO_TX(
		&pkt->htc_pkt,
		htt_h2t_send_complete_free_netbuf,
		qdf_nbuf_data(msg),
		qdf_nbuf_len(msg),
		pdev->htc_tx_endpoint,
		1); /* tag - not relevant here */

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

	rc = htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt);
#ifdef ATH_11AC_TXCOMPACT
	if (rc == QDF_STATUS_SUCCESS) {
		htt_htc_misc_pkt_list_add(pdev, pkt);
	} else {
		qdf_nbuf_free(msg);
		htt_htc_pkt_free(pdev, pkt);
	}
#endif

	return rc;
}

#endif /* defined(HELIUMPLUS) */

QDF_STATUS htt_h2t_ver_req_msg(struct htt_pdev_t *pdev)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;
	uint32_t msg_size;
	uint32_t max_tx_group;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return QDF_STATUS_E_FAILURE; /* failure */

	max_tx_group = ol_tx_get_max_tx_groups_supported(pdev->txrx_pdev);

	if (max_tx_group)
		msg_size = HTT_VER_REQ_BYTES +
			sizeof(struct htt_option_tlv_mac_tx_queue_groups_t);
	else
		msg_size = HTT_VER_REQ_BYTES;

	/* show that this is not a tx frame download
	 * (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */

	/* reserve room for the HTC header */
	msg = qdf_nbuf_alloc(pdev->osdev, HTT_MSG_BUF_SIZE(msg_size),
			     HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4,
			     true);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return QDF_STATUS_E_FAILURE; /* failure */
	}

	/*
	 * Set the length of the message.
	 * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
	 * separately during the below call to qdf_nbuf_push_head.
	 * The contribution from the HTC header is added separately inside HTC.
	 */
	qdf_nbuf_put_tail(msg, msg_size);

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_VERSION_REQ);

	if (max_tx_group) {
		*(msg_word + 1) = 0;

		/* Fill Group Info */
		HTT_OPTION_TLV_TAG_SET(*(msg_word+1),
				       HTT_OPTION_TLV_TAG_MAX_TX_QUEUE_GROUPS);
		HTT_OPTION_TLV_LENGTH_SET(*(msg_word+1),
			(sizeof(struct htt_option_tlv_mac_tx_queue_groups_t)/
			 sizeof(uint32_t)));
		HTT_OPTION_TLV_VALUE0_SET(*(msg_word+1), max_tx_group);
	}

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg), qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       1); /* tag - not relevant here */

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
	HTT_SEND_HTC_PKT(pdev, pkt);

	ol_tx_deduct_one_credit(pdev->txrx_pdev);

	return QDF_STATUS_SUCCESS;
}

#if defined(HELIUMPLUS)
/**
 * htt_h2t_rx_ring_rfs_cfg_msg_ll() - Configure receive flow steering
 * @pdev: handle to the HTT instance
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         A_NO_MEMORY No memory fail
 */
QDF_STATUS htt_h2t_rx_ring_rfs_cfg_msg_ll(struct htt_pdev_t *pdev)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;
	uint32_t  msg_local;
	struct cds_config_info *cds_cfg;

	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO_LOW,
		  "Receive flow steering configuration, disable gEnableFlowSteering(=0) in ini if FW doesnot support it\n");
	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return QDF_STATUS_E_NOMEM; /* failure */

	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */

	/* reserve room for the HTC header */
	msg = qdf_nbuf_alloc(pdev->osdev,
			     HTT_MSG_BUF_SIZE(HTT_RFS_CFG_REQ_BYTES),
			     HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4,
			     true);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return QDF_STATUS_E_NOMEM; /* failure */
	}
	/*
	 * Set the length of the message.
	 * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
	 * separately during the below call to qdf_nbuf_push_head.
	 * The contribution from the HTC header is added separately inside HTC.
	 */
	qdf_nbuf_put_tail(msg, HTT_RFS_CFG_REQ_BYTES);

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	msg_local = 0;
	HTT_H2T_MSG_TYPE_SET(msg_local, HTT_H2T_MSG_TYPE_RFS_CONFIG);
	if (ol_cfg_is_flow_steering_enabled(pdev->ctrl_pdev)) {
		HTT_RX_RFS_CONFIG_SET(msg_local, 1);
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO_LOW,
			  "Enable Rx flow steering");
	} else {
	    QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO_LOW,
		      "Disable Rx flow steering");
	}
	cds_cfg = cds_get_ini_config();
	if (cds_cfg) {
		msg_local |= ((cds_cfg->max_msdus_per_rxinorderind & 0xff)
			      << 16);
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO_LOW,
			  "Updated maxMSDUsPerRxInd");
	}

	*msg_word = msg_local;
	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO_LOW,
		  "%s: Sending msg_word: 0x%08x",
		  __func__, *msg_word);

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg), qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       1); /* tag - not relevant here */

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
	HTT_SEND_HTC_PKT(pdev, pkt);
	return QDF_STATUS_SUCCESS;
}
#else
/**
 * htt_h2t_rx_ring_rfs_cfg_msg_ll() - Configure receive flow steering
 * @pdev: handle to the HTT instance
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         A_NO_MEMORY No memory fail
 */
QDF_STATUS htt_h2t_rx_ring_rfs_cfg_msg_ll(struct htt_pdev_t *pdev)
{
	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO,
		  "Doesnot support receive flow steering configuration\n");
	return QDF_STATUS_SUCCESS;
}
#endif /* HELIUMPLUS */

QDF_STATUS htt_h2t_rx_ring_cfg_msg_ll(struct htt_pdev_t *pdev)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;
	int enable_ctrl_data, enable_mgmt_data,
	    enable_null_data, enable_phy_data, enable_hdr,
	    enable_ppdu_start, enable_ppdu_end;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return QDF_STATUS_E_FAILURE; /* failure */

	/*
	 * show that this is not a tx frame download
	 *  (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */

	/* reserve room for the HTC header */
	msg = qdf_nbuf_alloc(pdev->osdev,
			     HTT_MSG_BUF_SIZE(HTT_RX_RING_CFG_BYTES(1)),
			     HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4,
			     true);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return QDF_STATUS_E_FAILURE; /* failure */
	}
	/*
	 * Set the length of the message.
	 * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
	 * separately during the below call to qdf_nbuf_push_head.
	 * The contribution from the HTC header is added separately inside HTC.
	 */
	qdf_nbuf_put_tail(msg, HTT_RX_RING_CFG_BYTES(1));

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_RX_RING_CFG);
	HTT_RX_RING_CFG_NUM_RINGS_SET(*msg_word, 1);

	msg_word++;
	*msg_word = 0;
#if HTT_PADDR64
	HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_LO_SET(
			*msg_word,
			qdf_get_lower_32_bits(pdev->rx_ring.alloc_idx.paddr));
	msg_word++;
	HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_HI_SET(
			*msg_word,
			qdf_get_upper_32_bits(pdev->rx_ring.alloc_idx.paddr));
#else /* ! HTT_PADDR64 */
	HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_SET(*msg_word,
						 pdev->rx_ring.alloc_idx.paddr);
#endif /* HTT_PADDR64 */

	msg_word++;
	*msg_word = 0;
#if HTT_PADDR64
	HTT_RX_RING_CFG_BASE_PADDR_LO_SET(*msg_word,
					  pdev->rx_ring.base_paddr);
	{
		uint32_t tmp;

		tmp = qdf_get_upper_32_bits(pdev->rx_ring.base_paddr);
		if (tmp & 0xfffffe0) {
			QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO,
				  "%s:%d paddr > 37 bits!. Trimmed.",
				  __func__, __LINE__);
			tmp &= 0x01f;
		}


		msg_word++;
		HTT_RX_RING_CFG_BASE_PADDR_HI_SET(*msg_word, tmp);
	}
#else /* ! HTT_PADDR64 */
	HTT_RX_RING_CFG_BASE_PADDR_SET(*msg_word, pdev->rx_ring.base_paddr);
#endif /* HTT_PADDR64 */

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_LEN_SET(*msg_word, pdev->rx_ring.size);
	HTT_RX_RING_CFG_BUF_SZ_SET(*msg_word, HTT_RX_BUF_SIZE);

/* FIX THIS: if the FW creates a complete translated rx descriptor,
 * then the MAC DMA of the HW rx descriptor should be disabled.
 */
	msg_word++;
	*msg_word = 0;
#ifndef REMOVE_PKT_LOG
	if (ol_cfg_is_packet_log_enabled(pdev->ctrl_pdev)) {
		enable_ctrl_data = 1;
		enable_mgmt_data = 1;
		enable_null_data = 1;
		enable_phy_data = 1;
		enable_hdr = 1;
		enable_ppdu_start = 1;
		enable_ppdu_end = 1;
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO_LOW,
			  "%s : %d Pkt log is enabled\n",  __func__, __LINE__);
	} else {
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO,
			  "%s : %d Pkt log is disabled\n",  __func__, __LINE__);
		enable_ctrl_data = 0;
		enable_mgmt_data = 0;
		enable_null_data = 0;
		enable_phy_data = 0;
		enable_hdr = 0;
		enable_ppdu_start = 0;
		enable_ppdu_end = 0;
	}
#else
	enable_ctrl_data = 0;
	enable_mgmt_data = 0;
	enable_null_data = 0;
	enable_phy_data = 0;
	enable_hdr = 0;
	enable_ppdu_start = 0;
	enable_ppdu_end = 0;
#endif
	if (QDF_GLOBAL_MONITOR_MODE == cds_get_conparam()) {
		enable_ctrl_data  = 1;
		enable_mgmt_data  = 1;
		enable_null_data  = 1;
		enable_phy_data   = 1;
		enable_hdr        = 1;
		enable_ppdu_start = 1;
		enable_ppdu_end   = 1;
		/* Disable ASPM for monitor mode */
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO,
			  "%s : %d Monitor mode is enabled\n",
			  __func__, __LINE__);
	}

	htt_rx_enable_ppdu_end(&enable_ppdu_end);
	HTT_RX_RING_CFG_ENABLED_802_11_HDR_SET(*msg_word, enable_hdr);
	HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_SET(*msg_word, 1);
	HTT_RX_RING_CFG_ENABLED_PPDU_START_SET(*msg_word, enable_ppdu_start);
	HTT_RX_RING_CFG_ENABLED_PPDU_END_SET(*msg_word, enable_ppdu_end);
	HTT_RX_RING_CFG_ENABLED_MPDU_START_SET(*msg_word, 1);
	HTT_RX_RING_CFG_ENABLED_MPDU_END_SET(*msg_word, 1);
	HTT_RX_RING_CFG_ENABLED_MSDU_START_SET(*msg_word, 1);
	HTT_RX_RING_CFG_ENABLED_MSDU_END_SET(*msg_word, 1);
	HTT_RX_RING_CFG_ENABLED_RX_ATTN_SET(*msg_word, 1);
	/* always present? */
	HTT_RX_RING_CFG_ENABLED_FRAG_INFO_SET(*msg_word, 1);
	HTT_RX_RING_CFG_ENABLED_UCAST_SET(*msg_word, 1);
	HTT_RX_RING_CFG_ENABLED_MCAST_SET(*msg_word, 1);
	/* Must change to dynamic enable at run time
	 * rather than at compile time
	 */
	HTT_RX_RING_CFG_ENABLED_CTRL_SET(*msg_word, enable_ctrl_data);
	HTT_RX_RING_CFG_ENABLED_MGMT_SET(*msg_word, enable_mgmt_data);
	HTT_RX_RING_CFG_ENABLED_NULL_SET(*msg_word, enable_null_data);
	HTT_RX_RING_CFG_ENABLED_PHY_SET(*msg_word, enable_phy_data);
	HTT_RX_RING_CFG_IDX_INIT_VAL_SET(*msg_word,
					 *pdev->rx_ring.alloc_idx.vaddr);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_OFFSET_802_11_HDR_SET(*msg_word,
					      RX_DESC_HDR_STATUS_OFFSET32);
	HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_SET(*msg_word,
					      HTT_RX_DESC_RESERVATION32);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_OFFSET_PPDU_START_SET(*msg_word,
					      RX_DESC_PPDU_START_OFFSET32);
	HTT_RX_RING_CFG_OFFSET_PPDU_END_SET(*msg_word,
					    RX_DESC_PPDU_END_OFFSET32);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_OFFSET_MPDU_START_SET(*msg_word,
					      RX_DESC_MPDU_START_OFFSET32);
	HTT_RX_RING_CFG_OFFSET_MPDU_END_SET(*msg_word,
					    RX_DESC_MPDU_END_OFFSET32);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_OFFSET_MSDU_START_SET(*msg_word,
					      RX_DESC_MSDU_START_OFFSET32);
	HTT_RX_RING_CFG_OFFSET_MSDU_END_SET(*msg_word,
					    RX_DESC_MSDU_END_OFFSET32);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_OFFSET_RX_ATTN_SET(*msg_word,
					   RX_DESC_ATTN_OFFSET32);
	HTT_RX_RING_CFG_OFFSET_FRAG_INFO_SET(*msg_word,
					     RX_DESC_FRAG_INFO_OFFSET32);

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg),
			       qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       HTC_TX_PACKET_TAG_RUNTIME_PUT);

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
	HTT_SEND_HTC_PKT(pdev, pkt);
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
htt_h2t_rx_ring_cfg_msg_hl(struct htt_pdev_t *pdev)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	u_int32_t *msg_word;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return A_ERROR; /* failure */

	/*
	 * show that this is not a tx frame download
	 * (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL; /* not used during send-done callback */

	msg = qdf_nbuf_alloc(
		pdev->osdev,
		HTT_MSG_BUF_SIZE(HTT_RX_RING_CFG_BYTES(1)),
		/* reserve room for the HTC header */
		HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, true);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return A_ERROR; /* failure */
	}
	/*
	 * Set the length of the message.
	 * The contribution from the HTC_HDR_ALIGNMENT_PADDING is added
	 * separately during the below call to adf_nbuf_push_head.
	 * The contribution from the HTC header is added separately inside HTC.
	 */
	qdf_nbuf_put_tail(msg, HTT_RX_RING_CFG_BYTES(1));

	/* fill in the message contents */
	msg_word = (u_int32_t *)qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_RX_RING_CFG);
	HTT_RX_RING_CFG_NUM_RINGS_SET(*msg_word, 1);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_IDX_SHADOW_REG_PADDR_SET(
			*msg_word, pdev->rx_ring.alloc_idx.paddr);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_BASE_PADDR_SET(*msg_word, pdev->rx_ring.base_paddr);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_LEN_SET(*msg_word, pdev->rx_ring.size);
	HTT_RX_RING_CFG_BUF_SZ_SET(*msg_word, HTT_RX_BUF_SIZE);

	/* FIX THIS: if the FW creates a complete translated rx descriptor,
	 * then the MAC DMA of the HW rx descriptor should be disabled. */
	msg_word++;
	*msg_word = 0;

	HTT_RX_RING_CFG_ENABLED_802_11_HDR_SET(*msg_word, 0);
	HTT_RX_RING_CFG_ENABLED_MSDU_PAYLD_SET(*msg_word, 1);
	HTT_RX_RING_CFG_ENABLED_PPDU_START_SET(*msg_word, 0);
	HTT_RX_RING_CFG_ENABLED_PPDU_END_SET(*msg_word, 0);
	HTT_RX_RING_CFG_ENABLED_MPDU_START_SET(*msg_word, 0);
	HTT_RX_RING_CFG_ENABLED_MPDU_END_SET(*msg_word,   0);
	HTT_RX_RING_CFG_ENABLED_MSDU_START_SET(*msg_word, 0);
	HTT_RX_RING_CFG_ENABLED_MSDU_END_SET(*msg_word,   0);
	HTT_RX_RING_CFG_ENABLED_RX_ATTN_SET(*msg_word,    0);
	/* always present? */
	HTT_RX_RING_CFG_ENABLED_FRAG_INFO_SET(*msg_word,  0);
	HTT_RX_RING_CFG_ENABLED_UCAST_SET(*msg_word, 1);
	HTT_RX_RING_CFG_ENABLED_MCAST_SET(*msg_word, 1);
	/* Must change to dynamic enable at run time
	 * rather than at compile time
	 */
	HTT_RX_RING_CFG_ENABLED_CTRL_SET(*msg_word, 0);
	HTT_RX_RING_CFG_ENABLED_MGMT_SET(*msg_word, 0);
	HTT_RX_RING_CFG_ENABLED_NULL_SET(*msg_word, 0);
	HTT_RX_RING_CFG_ENABLED_PHY_SET(*msg_word, 0);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_OFFSET_802_11_HDR_SET(*msg_word,
					      0);
	HTT_RX_RING_CFG_OFFSET_MSDU_PAYLD_SET(*msg_word,
					      0);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_OFFSET_PPDU_START_SET(*msg_word,
					      0);
	HTT_RX_RING_CFG_OFFSET_PPDU_END_SET(*msg_word,
					    0);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_OFFSET_MPDU_START_SET(*msg_word,
					      0);
	HTT_RX_RING_CFG_OFFSET_MPDU_END_SET(*msg_word,
					    0);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_OFFSET_MSDU_START_SET(*msg_word,
					      0);
	HTT_RX_RING_CFG_OFFSET_MSDU_END_SET(*msg_word,
					    0);

	msg_word++;
	*msg_word = 0;
	HTT_RX_RING_CFG_OFFSET_RX_ATTN_SET(*msg_word,
					   0);
	HTT_RX_RING_CFG_OFFSET_FRAG_INFO_SET(*msg_word,
					     0);

	SET_HTC_PACKET_INFO_TX(
		&pkt->htc_pkt,
		htt_h2t_send_complete_free_netbuf,
		qdf_nbuf_data(msg),
		qdf_nbuf_len(msg),
		pdev->htc_tx_endpoint,
		1); /* tag - not relevant here */

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

#ifdef ATH_11AC_TXCOMPACT
	if (htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt) == QDF_STATUS_SUCCESS) {
		htt_htc_misc_pkt_list_add(pdev, pkt);
	} else {
		qdf_nbuf_free(msg);
		htt_htc_pkt_free(pdev, pkt);
	}
#else
	htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif

	ol_tx_deduct_one_credit(pdev->txrx_pdev);

	return QDF_STATUS_SUCCESS;
}

/**
 * htt_h2t_rx_ring_rfs_cfg_msg_hl() - Configure receive flow steering
 * @pdev: handle to the HTT instance
 *
 * Return: QDF_STATUS_SUCCESS on success
 *         A_NO_MEMORY No memory fail
 */
QDF_STATUS htt_h2t_rx_ring_rfs_cfg_msg_hl(struct htt_pdev_t *pdev)
{
	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO,
		  "Doesnot support Receive flow steering configuration\n");
	return QDF_STATUS_SUCCESS;
}

int
htt_h2t_dbg_stats_get(struct htt_pdev_t *pdev,
		      uint32_t stats_type_upload_mask,
		      uint32_t stats_type_reset_mask,
		      uint8_t cfg_stat_type, uint32_t cfg_val, uint8_t cookie)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;
	uint16_t htc_tag = 1;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return -EINVAL;      /* failure */

	if (stats_type_upload_mask >= 1 << HTT_DBG_NUM_STATS ||
	    stats_type_reset_mask >= 1 << HTT_DBG_NUM_STATS) {
		/* FIX THIS - add more details? */
		QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_ERROR,
			  "%#x %#x stats not supported\n",
			  stats_type_upload_mask, stats_type_reset_mask);
		htt_htc_pkt_free(pdev, pkt);
		return -EINVAL;      /* failure */
	}

	if (stats_type_reset_mask)
		htc_tag = HTC_TX_PACKET_TAG_RUNTIME_PUT;

	/* show that this is not a tx frame download
	 * (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */


	msg = qdf_nbuf_alloc(pdev->osdev,
			     HTT_MSG_BUF_SIZE(HTT_H2T_STATS_REQ_MSG_SZ),
			     /* reserve room for HTC header */
			     HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4,
			     false);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return -EINVAL;      /* failure */
	}
	/* set the length of the message */
	qdf_nbuf_put_tail(msg, HTT_H2T_STATS_REQ_MSG_SZ);

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_STATS_REQ);
	HTT_H2T_STATS_REQ_UPLOAD_TYPES_SET(*msg_word, stats_type_upload_mask);

	msg_word++;
	*msg_word = 0;
	HTT_H2T_STATS_REQ_RESET_TYPES_SET(*msg_word, stats_type_reset_mask);

	msg_word++;
	*msg_word = 0;
	HTT_H2T_STATS_REQ_CFG_VAL_SET(*msg_word, cfg_val);
	HTT_H2T_STATS_REQ_CFG_STAT_TYPE_SET(*msg_word, cfg_stat_type);

	/* cookie LSBs */
	msg_word++;
	*msg_word = cookie;

	/* cookie MSBs */
	msg_word++;
	*msg_word = 0;

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg),
			       qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       htc_tag); /* tag - not relevant here */

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

#ifdef ATH_11AC_TXCOMPACT
	if (htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt) == QDF_STATUS_SUCCESS) {
		htt_htc_misc_pkt_list_add(pdev, pkt);
	} else {
		qdf_nbuf_free(msg);
		htt_htc_pkt_free(pdev, pkt);
	}
#else
	htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif

	ol_tx_deduct_one_credit(pdev->txrx_pdev);

	return 0;
}

A_STATUS htt_h2t_sync_msg(struct htt_pdev_t *pdev, uint8_t sync_cnt)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return A_NO_MEMORY;

	/* show that this is not a tx frame download
	   (not required, but helpful)
	*/
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */

	/* reserve room for HTC header */
	msg = qdf_nbuf_alloc(pdev->osdev, HTT_MSG_BUF_SIZE(HTT_H2T_SYNC_MSG_SZ),
			     HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4,
			     false);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return A_NO_MEMORY;
	}
	/* set the length of the message */
	qdf_nbuf_put_tail(msg, HTT_H2T_SYNC_MSG_SZ);

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_SYNC);
	HTT_H2T_SYNC_COUNT_SET(*msg_word, sync_cnt);

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg),
			       qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       HTC_TX_PACKET_TAG_RUNTIME_PUT);

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
	HTT_SEND_HTC_PKT(pdev, pkt);

	ol_tx_deduct_one_credit(pdev->txrx_pdev);

	return A_OK;
}

int
htt_h2t_aggr_cfg_msg(struct htt_pdev_t *pdev,
		     int max_subfrms_ampdu, int max_subfrms_amsdu)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return -EINVAL;      /* failure */

	/* show that this is not a tx frame download
	 * (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */

	/* reserve room for HTC header */
	msg = qdf_nbuf_alloc(pdev->osdev, HTT_MSG_BUF_SIZE(HTT_AGGR_CFG_MSG_SZ),
			     HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4,
			     false);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return -EINVAL;      /* failure */
	}
	/* set the length of the message */
	qdf_nbuf_put_tail(msg, HTT_AGGR_CFG_MSG_SZ);

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_AGGR_CFG);

	if (max_subfrms_ampdu && (max_subfrms_ampdu <= 64)) {
		HTT_AGGR_CFG_MAX_NUM_AMPDU_SUBFRM_SET(*msg_word,
						      max_subfrms_ampdu);
	}

	if (max_subfrms_amsdu && (max_subfrms_amsdu < 32)) {
		HTT_AGGR_CFG_MAX_NUM_AMSDU_SUBFRM_SET(*msg_word,
						      max_subfrms_amsdu);
	}

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg),
			       qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       HTC_TX_PACKET_TAG_RUNTIME_PUT);

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);

#ifdef ATH_11AC_TXCOMPACT
	if (htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt) == QDF_STATUS_SUCCESS) {
		htt_htc_misc_pkt_list_add(pdev, pkt);
	} else {
		qdf_nbuf_free(msg);
		htt_htc_pkt_free(pdev, pkt);
	}
#else
	htc_send_pkt(pdev->htc_pdev, &pkt->htc_pkt);
#endif

	ol_tx_deduct_one_credit(pdev->txrx_pdev);

	return 0;
}

#ifdef IPA_OFFLOAD
/**
 * htt_h2t_ipa_uc_rsc_cfg_msg() - Send WDI IPA config message to firmware
 * @pdev: handle to the HTT instance
 *
 * Return: 0 success
 *         A_NO_MEMORY No memory fail
 */
#ifdef QCA_WIFI_3_0
int htt_h2t_ipa_uc_rsc_cfg_msg(struct htt_pdev_t *pdev)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;
	uint32_t addr;
	qdf_mem_info_t *mem_info_t;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return -A_NO_MEMORY;

	/* show that this is not a tx frame download
	 * (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */

	/* reserve room for HTC header */
	msg = qdf_nbuf_alloc(pdev->osdev, HTT_MSG_BUF_SIZE(HTT_WDI_IPA_CFG_SZ),
			     HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4,
			     false);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return -A_NO_MEMORY;
	}
	/* set the length of the message */
	qdf_nbuf_put_tail(msg, HTT_WDI_IPA_CFG_SZ);

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_SET(*msg_word,
		pdev->ipa_uc_tx_rsc.alloc_tx_buf_cnt);
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_WDI_IPA_CFG);

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_LO_SET(*msg_word,
		(unsigned int)qdf_mem_get_dma_addr(pdev->osdev,
			&pdev->ipa_uc_tx_rsc.tx_comp_ring->mem_info));
	msg_word++;
	*msg_word = 0;
	mem_info_t = &pdev->ipa_uc_tx_rsc.tx_comp_ring->mem_info;
	addr = (uint64_t)qdf_mem_get_dma_addr(pdev->osdev, mem_info_t) >> 32;
	HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_HI_SET(*msg_word, addr);

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_SET(*msg_word,
		(unsigned int)ol_cfg_ipa_uc_tx_max_buf_cnt(pdev->ctrl_pdev));

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_LO_SET(*msg_word,
		(unsigned int)pdev->ipa_uc_tx_rsc.tx_comp_idx_paddr);
	msg_word++;
	*msg_word = 0;
	addr = (uint64_t)pdev->ipa_uc_tx_rsc.tx_comp_idx_paddr >> 32;
	HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_HI_SET(*msg_word, addr);

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_LO_SET(*msg_word,
		(unsigned int)qdf_mem_get_dma_addr(pdev->osdev,
			&pdev->ipa_uc_tx_rsc.tx_ce_idx->mem_info));
	msg_word++;
	*msg_word = 0;
	mem_info_t = &pdev->ipa_uc_tx_rsc.tx_ce_idx->mem_info;
	addr = (uint64_t)qdf_mem_get_dma_addr(pdev->osdev, mem_info_t) >> 32;
	HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_HI_SET(*msg_word, addr);

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_LO_SET(*msg_word,
		(unsigned int)qdf_mem_get_dma_addr(pdev->osdev,
			&pdev->ipa_uc_rx_rsc.rx_ind_ring->mem_info));
	msg_word++;
	*msg_word = 0;
	mem_info_t = &pdev->ipa_uc_rx_rsc.rx_ind_ring->mem_info;
	addr = (uint64_t)qdf_mem_get_dma_addr(pdev->osdev, mem_info_t) >> 32;
	HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_HI_SET(*msg_word, addr);

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_SET(*msg_word,
		(unsigned int)qdf_get_pwr2(pdev->rx_ring.fill_level));

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_LO_SET(*msg_word,
		(unsigned int)qdf_mem_get_dma_addr(pdev->osdev,
			&pdev->ipa_uc_rx_rsc.rx_ipa_prc_done_idx->mem_info));
	msg_word++;
	*msg_word = 0;
	mem_info_t = &pdev->ipa_uc_rx_rsc.rx_ipa_prc_done_idx->mem_info;
	addr = (uint64_t)qdf_mem_get_dma_addr(pdev->osdev, mem_info_t) >> 32;
	HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_HI_SET(*msg_word, addr);

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_LO_SET(*msg_word,
		(unsigned int)pdev->ipa_uc_rx_rsc.rx_rdy_idx_paddr);
	msg_word++;
	*msg_word = 0;
	addr = (uint64_t)pdev->ipa_uc_rx_rsc.rx_rdy_idx_paddr >> 32;
	HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_HI_SET(*msg_word, addr);

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_LO_SET(*msg_word,
		(unsigned int)qdf_mem_get_dma_addr(pdev->osdev,
			&pdev->ipa_uc_rx_rsc.rx2_ind_ring->mem_info));
	msg_word++;
	*msg_word = 0;
	mem_info_t = &pdev->ipa_uc_rx_rsc.rx2_ind_ring->mem_info;
	addr = (uint64_t)qdf_mem_get_dma_addr(pdev->osdev, mem_info_t) >> 32;
	HTT_WDI_IPA_CFG_RX_RING2_BASE_ADDR_HI_SET(*msg_word, addr);

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_RING2_SIZE_SET(*msg_word,
		(unsigned int)qdf_get_pwr2(pdev->rx_ring.fill_level));

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_LO_SET(*msg_word,
		(unsigned int)qdf_mem_get_dma_addr(pdev->osdev,
			&pdev->ipa_uc_rx_rsc.rx2_ipa_prc_done_idx->mem_info));
	msg_word++;
	*msg_word = 0;
	mem_info_t = &pdev->ipa_uc_rx_rsc.rx2_ipa_prc_done_idx->mem_info;
	addr = (uint64_t)qdf_mem_get_dma_addr(pdev->osdev, mem_info_t) >> 32;
	HTT_WDI_IPA_CFG_RX_RING2_RD_IDX_ADDR_HI_SET(*msg_word, addr);

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_LO_SET(*msg_word,
		(unsigned int)qdf_mem_get_dma_addr(pdev->osdev,
			&pdev->ipa_uc_rx_rsc.rx2_ipa_prc_done_idx->mem_info));
	msg_word++;
	*msg_word = 0;
	mem_info_t = &pdev->ipa_uc_rx_rsc.rx2_ipa_prc_done_idx->mem_info;
	addr = (uint64_t)qdf_mem_get_dma_addr(pdev->osdev, mem_info_t) >> 32;
	HTT_WDI_IPA_CFG_RX_RING2_WR_IDX_ADDR_HI_SET(*msg_word, addr);

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg),
			       qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       HTC_TX_PACKET_TAG_RUNTIME_PUT);

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
	HTT_SEND_HTC_PKT(pdev, pkt);
	return A_OK;
}
#else
/* Rome Support only WDI 1.0 */
int htt_h2t_ipa_uc_rsc_cfg_msg(struct htt_pdev_t *pdev)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return A_NO_MEMORY;

	/* show that this is not a tx frame download
	 * (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */

	/* reserve room for HTC header */
	msg = qdf_nbuf_alloc(pdev->osdev, HTT_MSG_BUF_SIZE(HTT_WDI_IPA_CFG_SZ),
			     HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4,
			     false);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return A_NO_MEMORY;
	}
	/* set the length of the message */
	qdf_nbuf_put_tail(msg, HTT_WDI_IPA_CFG_SZ);

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_WDI_IPA_CFG_TX_PKT_POOL_SIZE_SET(*msg_word,
				pdev->ipa_uc_tx_rsc.alloc_tx_buf_cnt);
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_WDI_IPA_CFG);

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_TX_COMP_RING_BASE_ADDR_SET(*msg_word,
		(unsigned int)qdf_mem_get_dma_addr(pdev->osdev,
			&pdev->ipa_uc_tx_rsc.tx_comp_ring->mem_info));

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_TX_COMP_RING_SIZE_SET(
		*msg_word,
		(unsigned int)ol_cfg_ipa_uc_tx_max_buf_cnt(pdev->ctrl_pdev));

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_TX_COMP_WR_IDX_ADDR_SET(*msg_word,
		(unsigned int)pdev->ipa_uc_tx_rsc.tx_comp_idx_paddr);

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_TX_CE_WR_IDX_ADDR_SET(*msg_word,
		(unsigned int)qdf_mem_get_dma_addr(pdev->osdev,
			&pdev->ipa_uc_tx_rsc.tx_ce_idx->mem_info));

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_IND_RING_BASE_ADDR_SET(*msg_word,
		(unsigned int)qdf_mem_get_dma_addr(pdev->osdev,
			&pdev->ipa_uc_rx_rsc.rx_ind_ring->mem_info));

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_IND_RING_SIZE_SET(*msg_word,
		(unsigned int)qdf_get_pwr2(pdev->rx_ring.fill_level));

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_IND_RD_IDX_ADDR_SET(*msg_word,
		(unsigned int)qdf_mem_get_dma_addr(pdev->osdev,
			&pdev->ipa_uc_rx_rsc.rx_ipa_prc_done_idx->mem_info));

	msg_word++;
	*msg_word = 0;
	HTT_WDI_IPA_CFG_RX_IND_WR_IDX_ADDR_SET(*msg_word,
	       (unsigned int)pdev->ipa_uc_rx_rsc.rx_rdy_idx_paddr);

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg),
			       qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       HTC_TX_PACKET_TAG_RUNTIME_PUT);

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
	HTT_SEND_HTC_PKT(pdev, pkt);
	return A_OK;
}
#endif

/**
 * htt_h2t_ipa_uc_set_active() - Propagate WDI path enable/disable to firmware
 * @pdev: handle to the HTT instance
 * @uc_active: WDI UC path enable or not
 * @is_tx: TX path or RX path
 *
 * Return: 0 success
 *         A_NO_MEMORY No memory fail
 */
int htt_h2t_ipa_uc_set_active(struct htt_pdev_t *pdev,
			      bool uc_active, bool is_tx)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;
	uint8_t active_target = 0;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return -A_NO_MEMORY;

	/* show that this is not a tx frame download
	 * (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */

	/* reserve room for HTC header */
	msg = qdf_nbuf_alloc(pdev->osdev,
			     HTT_MSG_BUF_SIZE(HTT_WDI_IPA_OP_REQUEST_SZ),
			     HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4,
			     false);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return -A_NO_MEMORY;
	}
	/* set the length of the message */
	qdf_nbuf_put_tail(msg, HTT_WDI_IPA_OP_REQUEST_SZ);

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	if (uc_active && is_tx)
		active_target = HTT_WDI_IPA_OPCODE_TX_RESUME;
	else if (!uc_active && is_tx)
		active_target = HTT_WDI_IPA_OPCODE_TX_SUSPEND;
	else if (uc_active && !is_tx)
		active_target = HTT_WDI_IPA_OPCODE_RX_RESUME;
	else if (!uc_active && !is_tx)
		active_target = HTT_WDI_IPA_OPCODE_RX_SUSPEND;

	QDF_TRACE(QDF_MODULE_ID_HTT, QDF_TRACE_LEVEL_INFO,
			"%s: HTT_H2T_MSG_TYPE_WDI_IPA_OP_REQ (%d)\n",
			__func__, active_target);

	HTT_WDI_IPA_OP_REQUEST_OP_CODE_SET(*msg_word, active_target);
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_WDI_IPA_OP_REQ);

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg),
			       qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       1); /* tag - not relevant here */

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
	HTT_SEND_HTC_PKT(pdev, pkt);
	return A_OK;
}

/**
 * htt_h2t_ipa_uc_get_stats() - WDI UC state query request to firmware
 * @pdev: handle to the HTT instance
 *
 * Return: 0 success
 *         A_NO_MEMORY No memory fail
 */
int htt_h2t_ipa_uc_get_stats(struct htt_pdev_t *pdev)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return -A_NO_MEMORY;

	/* show that this is not a tx frame download
	 * (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */

	/* reserve room for HTC header */
	msg = qdf_nbuf_alloc(pdev->osdev,
			     HTT_MSG_BUF_SIZE(HTT_WDI_IPA_OP_REQUEST_SZ),
			     HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4,
			     false);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return -A_NO_MEMORY;
	}
	/* set the length of the message */
	qdf_nbuf_put_tail(msg, HTT_WDI_IPA_OP_REQUEST_SZ);

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_WDI_IPA_OP_REQUEST_OP_CODE_SET(*msg_word,
					   HTT_WDI_IPA_OPCODE_DBG_STATS);
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_WDI_IPA_OP_REQ);

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg),
			       qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       1); /* tag - not relevant here */

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
	HTT_SEND_HTC_PKT(pdev, pkt);
	return A_OK;
}

/**
 * htt_h2t_ipa_uc_get_share_stats() - WDI UC wifi sharing state request to FW
 * @pdev: handle to the HTT instance
 *
 * Return: A_OK success
 *         A_NO_MEMORY No memory fail
 */
int htt_h2t_ipa_uc_get_share_stats(struct htt_pdev_t *pdev, uint8_t reset_stats)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return -A_NO_MEMORY;

	/* show that this is not a tx frame download
	 * (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */

	/* reserve room for HTC header */
	msg = qdf_nbuf_alloc(pdev->osdev,
		HTT_MSG_BUF_SIZE(HTT_WDI_IPA_OP_REQUEST_SZ)+
		HTT_MSG_BUF_SIZE(WLAN_WDI_IPA_GET_SHARING_STATS_REQ_SZ),
		HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, false);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return -A_NO_MEMORY;
	}
	/* set the length of the message */
	qdf_nbuf_put_tail(msg, HTT_WDI_IPA_OP_REQUEST_SZ+
			  WLAN_WDI_IPA_GET_SHARING_STATS_REQ_SZ);

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_WDI_IPA_OP_REQUEST_OP_CODE_SET(*msg_word,
				   HTT_WDI_IPA_OPCODE_GET_SHARING_STATS);
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_WDI_IPA_OP_REQ);

	msg_word++;
	*msg_word = 0;
	WLAN_WDI_IPA_GET_SHARING_STATS_REQ_RESET_STATS_SET(*msg_word,
							     reset_stats);

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg),
			       qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       1); /* tag - not relevant here */

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
	HTT_SEND_HTC_PKT(pdev, pkt);
	return A_OK;
}

/**
 * htt_h2t_ipa_uc_set_quota() - WDI UC state query request to firmware
 * @pdev: handle to the HTT instance
 *
 * Return: A_OK success
 *         A_NO_MEMORY No memory fail
 */
int htt_h2t_ipa_uc_set_quota(struct htt_pdev_t *pdev, uint64_t quota_bytes)
{
	struct htt_htc_pkt *pkt;
	qdf_nbuf_t msg;
	uint32_t *msg_word;

	pkt = htt_htc_pkt_alloc(pdev);
	if (!pkt)
		return -A_NO_MEMORY;

	/* show that this is not a tx frame download
	 * (not required, but helpful)
	 */
	pkt->msdu_id = HTT_TX_COMPL_INV_MSDU_ID;
	pkt->pdev_ctxt = NULL;  /* not used during send-done callback */

	/* reserve room for HTC header */
	msg = qdf_nbuf_alloc(pdev->osdev,
		HTT_MSG_BUF_SIZE(HTT_WDI_IPA_OP_REQUEST_SZ)+
		HTT_MSG_BUF_SIZE(WLAN_WDI_IPA_SET_QUOTA_REQ_SZ),
		HTC_HEADER_LEN + HTC_HDR_ALIGNMENT_PADDING, 4, false);
	if (!msg) {
		htt_htc_pkt_free(pdev, pkt);
		return -A_NO_MEMORY;
	}
	/* set the length of the message */
	qdf_nbuf_put_tail(msg, HTT_WDI_IPA_OP_REQUEST_SZ+
			  WLAN_WDI_IPA_SET_QUOTA_REQ_SZ);

	/* fill in the message contents */
	msg_word = (uint32_t *) qdf_nbuf_data(msg);

	/* rewind beyond alignment pad to get to the HTC header reserved area */
	qdf_nbuf_push_head(msg, HTC_HDR_ALIGNMENT_PADDING);

	*msg_word = 0;
	HTT_WDI_IPA_OP_REQUEST_OP_CODE_SET(*msg_word,
					   HTT_WDI_IPA_OPCODE_SET_QUOTA);
	HTT_H2T_MSG_TYPE_SET(*msg_word, HTT_H2T_MSG_TYPE_WDI_IPA_OP_REQ);

	msg_word++;
	*msg_word = 0;
	WLAN_WDI_IPA_SET_QUOTA_REQ_SET_QUOTA_SET(*msg_word, quota_bytes > 0);

	msg_word++;
	*msg_word = 0;
	WLAN_WDI_IPA_SET_QUOTA_REQ_QUOTA_LO_SET(*msg_word,
			(uint32_t)(quota_bytes &
				   WLAN_WDI_IPA_SET_QUOTA_REQ_QUOTA_LO_M));

	msg_word++;
	*msg_word = 0;
	WLAN_WDI_IPA_SET_QUOTA_REQ_QUOTA_HI_SET(*msg_word,
			(uint32_t)(quota_bytes>>32 &
				   WLAN_WDI_IPA_SET_QUOTA_REQ_QUOTA_HI_M));

	SET_HTC_PACKET_INFO_TX(&pkt->htc_pkt,
			       htt_h2t_send_complete_free_netbuf,
			       qdf_nbuf_data(msg),
			       qdf_nbuf_len(msg),
			       pdev->htc_tx_endpoint,
			       1); /* tag - not relevant here */

	SET_HTC_PACKET_NET_BUF_CONTEXT(&pkt->htc_pkt, msg);
	HTT_SEND_HTC_PKT(pdev, pkt);
	return A_OK;
}
#endif /* IPA_OFFLOAD */
