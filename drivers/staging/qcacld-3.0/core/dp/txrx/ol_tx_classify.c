/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

#include <qdf_nbuf.h>         /* qdf_nbuf_t, etc. */
#include <htt.h>              /* HTT_TX_EXT_TID_MGMT */
#include <ol_htt_tx_api.h>    /* htt_tx_desc_tid */
#include <ol_txrx_api.h>      /* ol_txrx_vdev_handle */
#include <ol_txrx_ctrl_api.h> /* ol_txrx_sync */
#include <ol_txrx.h>
#include <ol_txrx_internal.h> /* TXRX_ASSERT1 */
#include <ol_txrx_types.h>    /* pdev stats */
#include <ol_tx_desc.h>       /* ol_tx_desc */
#include <ol_tx_send.h>       /* ol_tx_send */
#include <ol_txrx_peer_find.h>
#include <ol_tx_classify.h>
#include <ol_tx_queue.h>
#include <ipv4.h>
#include <ipv6_defs.h>
#include <ip_prot.h>
#include <enet.h>             /* ETHERTYPE_VLAN, etc. */
#include <cds_ieee80211_common.h>        /* ieee80211_frame */
#include <cdp_txrx_handle.h>
/*
 * In theory, this tx classify code could be used on the host or in the target.
 * Thus, this code uses generic OS primitives, that can be aliased to either
 * the host's OS primitives or the target's OS primitives.
 * For now, the following #defines set up these host-specific or
 * target-specific aliases.
 */

#define OL_TX_CLASSIFY_EXTENSION(vdev, tx_desc, netbuf, msdu_info, txq)
#define OL_TX_CLASSIFY_MGMT_EXTENSION(vdev, tx_desc, netbuf, msdu_info, txq)

#ifdef QCA_TX_HTT2_SUPPORT
static void
ol_tx_classify_htt2_frm(
	struct ol_txrx_vdev_t *vdev,
	qdf_nbuf_t tx_nbuf,
	struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	struct htt_msdu_info_t *htt = &tx_msdu_info->htt;
	A_UINT8 candi_frm = 0;

	/*
	 * Offload the frame re-order to L3 protocol and ONLY support
	 * TCP protocol now.
	 */
	if ((htt->info.l2_hdr_type == htt_pkt_type_ethernet) &&
	    (htt->info.frame_type == htt_frm_type_data) &&
	    htt->info.is_unicast &&
	    (htt->info.ethertype == ETHERTYPE_IPV4)) {
		struct ipv4_hdr_t *ipHdr;

		ipHdr = (struct ipv4_hdr_t *)(qdf_nbuf_data(tx_nbuf) +
			htt->info.l3_hdr_offset);
		if (ipHdr->protocol == IP_PROTOCOL_TCP)
			candi_frm = 1;
	}

	qdf_nbuf_set_tx_parallel_dnload_frm(tx_nbuf, candi_frm);
}

#define OL_TX_CLASSIFY_HTT2_EXTENSION(vdev, netbuf, msdu_info)      \
	ol_tx_classify_htt2_frm(vdev, netbuf, msdu_info)
#else
#define OL_TX_CLASSIFY_HTT2_EXTENSION(vdev, netbuf, msdu_info)      /* no-op */
#endif /* QCA_TX_HTT2_SUPPORT */
/* DHCP go with voice priority; WMM_AC_VO_TID1();*/
#define TX_DHCP_TID  6

#if defined(QCA_BAD_PEER_TX_FLOW_CL)
static inline A_BOOL
ol_if_tx_bad_peer_txq_overflow(
	struct ol_txrx_pdev_t *pdev,
	struct ol_txrx_peer_t *peer,
	struct ol_tx_frms_queue_t *txq)
{
	if (peer && pdev && txq && (peer->tx_limit_flag) &&
	    (txq->frms >= pdev->tx_peer_bal.peer_bal_txq_limit))
		return true;
	else
		return false;
}
#else
static inline A_BOOL ol_if_tx_bad_peer_txq_overflow(
	struct ol_txrx_pdev_t *pdev,
	struct ol_txrx_peer_t *peer,
	struct ol_tx_frms_queue_t *txq)
{
	return false;
}
#endif

/* EAPOL go with voice priority: WMM_AC_TO_TID1(WMM_AC_VO);*/
#define TX_EAPOL_TID  6

/* ARP go with voice priority: WMM_AC_TO_TID1(pdev->arp_ac_override)*/
#define TX_ARP_TID  6

/* For non-IP case, use default TID */
#define TX_DEFAULT_TID  0

/*
 * Determine IP TOS priority
 * IP Tos format :
 *        (Refer Pg 57 WMM-test-plan-v1.2)
 * IP-TOS - 8bits
 *            : DSCP(6-bits) ECN(2-bits)
 *            : DSCP - P2 P1 P0 X X X
 *                where (P2 P1 P0) form 802.1D
 */
static inline A_UINT8
ol_tx_tid_by_ipv4(A_UINT8 *pkt)
{
	A_UINT8 ipPri, tid;
	struct ipv4_hdr_t *ipHdr = (struct ipv4_hdr_t *)pkt;

	ipPri = ipHdr->tos >> 5;
	tid = ipPri & 0x7;

	return tid;
}

static inline A_UINT8
ol_tx_tid_by_ipv6(A_UINT8 *pkt)
{
	return (ipv6_traffic_class((struct ipv6_hdr_t *)pkt) >> 5) & 0x7;
}

static inline void
ol_tx_set_ether_type(
	A_UINT8 *datap,
	struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	A_UINT16 typeorlength;
	A_UINT8 *ptr;
	A_UINT8 *l3_data_ptr;

	if (tx_msdu_info->htt.info.l2_hdr_type == htt_pkt_type_raw) {
		/* adjust hdr_ptr to RA */
		struct ieee80211_frame *wh = (struct ieee80211_frame *)datap;

		if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
					IEEE80211_FC0_TYPE_DATA) {
			struct llc_snap_hdr_t *llc;
			/* dot11 encapsulated frame */
			struct ieee80211_qosframe *whqos =
					(struct ieee80211_qosframe *)datap;
			if (whqos->i_fc[0] & QDF_IEEE80211_FC0_SUBTYPE_QOS) {
				tx_msdu_info->htt.info.l3_hdr_offset =
					sizeof(struct ieee80211_qosframe);
			} else {
				tx_msdu_info->htt.info.l3_hdr_offset =
					sizeof(struct ieee80211_frame);
			}
			llc = (struct llc_snap_hdr_t *)
				(datap + tx_msdu_info->htt.info.l3_hdr_offset);
			tx_msdu_info->htt.info.ethertype =
				(llc->ethertype[0] << 8) | llc->ethertype[1];
			/*
			 * l3_hdr_offset refers to the end of the 802.3 or
			 * 802.11 header, which may be a LLC/SNAP header rather
			 * than the IP header.
			 * Thus, don't increment l3_hdr_offset += sizeof(*llc);
			 * rather,leave it as is.
			 */
		} else {
			/*
			 * This function should only be applied to data frames.
			 * For management frames, we already know to use
			 * HTT_TX_EXT_TID_MGMT.
			 */
			TXRX_ASSERT2(0);
		}
	} else if (tx_msdu_info->htt.info.l2_hdr_type ==
					htt_pkt_type_ethernet) {
		ptr = (datap + QDF_MAC_ADDR_SIZE * 2);
		typeorlength = (ptr[0] << 8) | ptr[1];
		/*ETHERNET_HDR_LEN;*/
		l3_data_ptr = datap + sizeof(struct ethernet_hdr_t);

		if (typeorlength == ETHERTYPE_VLAN) {
			ptr = (datap + QDF_MAC_ADDR_SIZE * 2
					+ ETHERTYPE_VLAN_LEN);
			typeorlength = (ptr[0] << 8) | ptr[1];
			l3_data_ptr += ETHERTYPE_VLAN_LEN;
		}

		if (!IS_ETHERTYPE(typeorlength)) {
			/* 802.3 header*/
			struct llc_snap_hdr_t *llc_hdr =
				(struct llc_snap_hdr_t *)l3_data_ptr;
			typeorlength = (llc_hdr->ethertype[0] << 8) |
							llc_hdr->ethertype[1];
			l3_data_ptr += sizeof(struct llc_snap_hdr_t);
		}

		tx_msdu_info->htt.info.l3_hdr_offset = (A_UINT8)(l3_data_ptr -
									datap);
		tx_msdu_info->htt.info.ethertype = typeorlength;
	}
}

static inline A_UINT8
ol_tx_tid_by_ether_type(
	A_UINT8 *datap,
	struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	A_UINT8 tid;
	A_UINT8 *l3_data_ptr;
	A_UINT16 typeorlength;

	l3_data_ptr = datap + tx_msdu_info->htt.info.l3_hdr_offset;
	typeorlength = tx_msdu_info->htt.info.ethertype;

	/* IP packet, do packet inspection for TID */
	if (typeorlength == ETHERTYPE_IPV4) {
		tid = ol_tx_tid_by_ipv4(l3_data_ptr);
	} else if (typeorlength == ETHERTYPE_IPV6) {
		tid = ol_tx_tid_by_ipv6(l3_data_ptr);
	} else if (ETHERTYPE_IS_EAPOL_WAPI(typeorlength)) {
		/* EAPOL go with voice priority*/
		tid = TX_EAPOL_TID;
	} else if (typeorlength == ETHERTYPE_ARP) {
		tid = TX_ARP_TID;
	} else {
		/* For non-IP case, use default TID */
		tid = TX_DEFAULT_TID;
	}
	return tid;
}

static inline A_UINT8
ol_tx_tid_by_raw_type(
	A_UINT8 *datap,
	struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	A_UINT8 tid = HTT_TX_EXT_TID_NON_QOS_MCAST_BCAST;

	/* adjust hdr_ptr to RA */
	struct ieee80211_frame *wh = (struct ieee80211_frame *)datap;

	/* FIXME: This code does not handle 4 address formats. The QOS field
	 * is not at usual location.
	 */
	if ((wh->i_fc[0] & IEEE80211_FC0_TYPE_MASK) ==
					IEEE80211_FC0_TYPE_DATA) {
		/* dot11 encapsulated frame */
		struct ieee80211_qosframe *whqos =
					(struct ieee80211_qosframe *)datap;
		if (whqos->i_fc[0] & QDF_IEEE80211_FC0_SUBTYPE_QOS)
			tid = whqos->i_qos[0] & IEEE80211_QOS_TID;
		else
			tid = HTT_NON_QOS_TID;
	} else {
		/*
		 * This function should only be applied to data frames.
		 * For management frames, we already know to use
		 * HTT_TX_EXT_TID_MGMT.
		 */
		qdf_assert(0);
	}
	return tid;
}

static A_UINT8
ol_tx_tid(
	struct ol_txrx_pdev_t *pdev,
	qdf_nbuf_t tx_nbuf,
	struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	A_UINT8 *datap = qdf_nbuf_data(tx_nbuf);
	A_UINT8 tid;

	if (pdev->frame_format == wlan_frm_fmt_raw) {
		tx_msdu_info->htt.info.l2_hdr_type = htt_pkt_type_raw;

		ol_tx_set_ether_type(datap, tx_msdu_info);
		tid = tx_msdu_info->htt.info.ext_tid ==
					QDF_NBUF_TX_EXT_TID_INVALID ?
			ol_tx_tid_by_raw_type(datap, tx_msdu_info) :
			tx_msdu_info->htt.info.ext_tid;
	} else if (pdev->frame_format == wlan_frm_fmt_802_3) {
		tx_msdu_info->htt.info.l2_hdr_type = htt_pkt_type_ethernet;

		ol_tx_set_ether_type(datap, tx_msdu_info);
		tid =
			tx_msdu_info->htt.info.ext_tid ==
					QDF_NBUF_TX_EXT_TID_INVALID ?
				ol_tx_tid_by_ether_type(datap, tx_msdu_info) :
				tx_msdu_info->htt.info.ext_tid;
	} else if (pdev->frame_format == wlan_frm_fmt_native_wifi) {
		struct llc_snap_hdr_t *llc;

		tx_msdu_info->htt.info.l2_hdr_type = htt_pkt_type_native_wifi;
		tx_msdu_info->htt.info.l3_hdr_offset =
						sizeof(struct ieee80211_frame);
		llc = (struct llc_snap_hdr_t *)
			(datap + tx_msdu_info->htt.info.l3_hdr_offset);
		tx_msdu_info->htt.info.ethertype =
			(llc->ethertype[0] << 8) | llc->ethertype[1];
		/*
		 * Native WiFi is a special case of "raw" 802.11 header format.
		 * However, we expect that for all cases that use native WiFi,
		 * the TID will be directly specified out of band.
		 */
		tid = tx_msdu_info->htt.info.ext_tid;
	} else {
		QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_FATAL,
			  "Invalid standard frame type: %d\n",
			  pdev->frame_format);
		qdf_assert(0);
		tid = HTT_TX_EXT_TID_INVALID;
	}
	return tid;
}

#if defined(FEATURE_WLAN_TDLS)
static inline
struct ol_txrx_peer_t *ol_tx_tdls_peer_find(struct ol_txrx_pdev_t *pdev,
						struct ol_txrx_vdev_t *vdev,
						uint8_t *dest_addr,
						uint8_t *peer_id)
{
	struct ol_txrx_peer_t *peer = NULL;
	uint8_t zero_mac_addr[QDF_MAC_ADDR_SIZE] = { 0, 0, 0, 0, 0, 0 };
	enum peer_debug_id_type id_type = PEER_DEBUG_ID_OL_INTERNAL;

	struct ol_txrx_peer_t *(*find_peer)(struct ol_txrx_pdev_t *pdev,
					    uint8_t *peer_mac_addr,
					    int mac_addr_is_aligned,
					    u8 check_valid,
					    enum peer_debug_id_type dbg_id)
		= ol_txrx_peer_find_hash_find_get_ref;

	if (vdev->hlTdlsFlag) {
		peer = find_peer(pdev, vdev->hl_tdls_ap_mac_addr.raw,
				 0, 1, id_type);

		if (peer && (peer->peer_ids[0] == HTT_INVALID_PEER_ID)) {
			ol_txrx_peer_release_ref(peer, id_type);
			peer = NULL;
		} else {
			if (peer) {
				*peer_id = peer->local_id;
				return peer;
			}
		}
	}

	/* Packets destined to TDLS Peer or AP with 'No TDLS Link'.
	 * Optimized to directly get the peer based on 'dest_addr'
	 */
	if (vdev->last_real_peer &&
	    !qdf_mem_cmp(vdev->last_real_peer->mac_addr.raw,
			 dest_addr, QDF_MAC_ADDR_SIZE)) {
		ol_txrx_peer_get_ref(vdev->last_real_peer, id_type);
		*peer_id = vdev->last_real_peer->local_id;
		peer = vdev->last_real_peer;
	} else {
		/* packets destined for other peers or AP with TDLS Link */
		if (vdev->last_real_peer &&
		    !qdf_mem_cmp(vdev->hl_tdls_ap_mac_addr.raw,
				 zero_mac_addr,
				 QDF_MAC_ADDR_SIZE)) {
		/* With No TDLS Link return last_real_peer for both AP
		 * and other bss peer
		 */
			ol_txrx_peer_get_ref(vdev->last_real_peer, id_type);
			*peer_id = vdev->last_real_peer->local_id;
			peer = vdev->last_real_peer;
		} else { /* packet destined for other peers and AP when
			  * STA has TDLS link
			  */
			peer = find_peer(pdev, vdev->hl_tdls_ap_mac_addr.raw,
					 0, 1, id_type);

			if (peer &&
			    (peer->peer_ids[0] == HTT_INVALID_PEER_ID)) {
				ol_txrx_peer_release_ref(peer, id_type);
				peer = NULL;
			} else {
				if (peer)
					*peer_id = peer->local_id;
			}
		}
	}
	return peer;
}

#else
static struct ol_txrx_peer_t *ol_tx_tdls_peer_find(struct ol_txrx_pdev_t *pdev,
						struct ol_txrx_vdev_t *vdev,
						uint8_t *dest_addr,
						uint8_t *peer_id)
{
	struct ol_txrx_peer_t *peer = NULL;

	peer = ol_txrx_assoc_peer_find(vdev);

	return peer;
}
#endif

struct ol_tx_frms_queue_t *
ol_tx_classify(
	struct ol_txrx_vdev_t *vdev,
	struct ol_tx_desc_t *tx_desc,
	qdf_nbuf_t tx_nbuf,
	struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	struct ol_txrx_peer_t *peer = NULL;
	struct ol_tx_frms_queue_t *txq = NULL;
	A_UINT8 *dest_addr;
	A_UINT8 tid;
	u_int8_t peer_id;

	TX_SCHED_DEBUG_PRINT("Enter");
	dest_addr = ol_tx_dest_addr_find(pdev, tx_nbuf);
	if (unlikely(!dest_addr)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX,
				QDF_TRACE_LEVEL_ERROR,
				"Error: dest_addr is NULL.\n");
		return NULL; /*error*/
	}
	if ((IEEE80211_IS_MULTICAST(dest_addr)) ||
	    (vdev->opmode == wlan_op_mode_ocb)) {
		txq = &vdev->txqs[OL_TX_VDEV_MCAST_BCAST];
		tx_msdu_info->htt.info.ext_tid =
					HTT_TX_EXT_TID_NON_QOS_MCAST_BCAST;
		if (vdev->opmode == wlan_op_mode_sta) {
			/*
			 * The STA sends a frame with a broadcast
			 * dest addr (DA) as a
			 * unicast frame to the AP's receive addr (RA).
			 * Find the peer object that represents the AP
			 * that the STA is associated with.
			 */
			peer = ol_txrx_assoc_peer_find(vdev);
			if (!peer) {
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_ERROR,
					  "Error: STA %pK ("QDF_MAC_ADDR_FMT") trying to send bcast DA tx data frame w/o association\n",
					  vdev,
					  QDF_MAC_ADDR_REF(vdev->mac_addr.raw));
				return NULL; /* error */
			} else if ((peer->security[
				OL_TXRX_PEER_SECURITY_MULTICAST].sec_type
						!= htt_sec_type_wapi) &&
				   (qdf_nbuf_is_ipv4_pkt(tx_nbuf) == true)) {
				if (QDF_NBUF_CB_PACKET_TYPE_DHCP ==
						QDF_NBUF_CB_GET_PACKET_TYPE(
								tx_nbuf)) {
					/* DHCP frame to go with
					 * voice priority
					 */
					txq = &peer->txqs[TX_DHCP_TID];
					tx_msdu_info->htt.info.ext_tid =
								TX_DHCP_TID;
				}
			}
			/*
			 * The following line assumes each peer object has a
			 * single ID. This is currently true, and is expected
			 * to remain true.
			 */
			tx_msdu_info->htt.info.peer_id = peer->peer_ids[0];
		} else if (vdev->opmode == wlan_op_mode_ocb) {
			tx_msdu_info->htt.info.peer_id = HTT_INVALID_PEER_ID;
			/*
			 * In OCB mode, don't worry about the peer.
			 * We don't need it.
			 */
			peer = NULL;
		} else {
			tx_msdu_info->htt.info.peer_id = HTT_INVALID_PEER_ID;
			/*
			 * Look up the vdev's BSS peer, so that the
			 * classify_extension function can check whether to
			 * encrypt multicast / broadcast frames.
			 */
			peer = ol_txrx_peer_find_hash_find_get_ref
						(pdev,
						 vdev->mac_addr.raw,
						 0, 1,
						 PEER_DEBUG_ID_OL_INTERNAL);
			if (!peer) {
				QDF_TRACE(QDF_MODULE_ID_TXRX,
					  QDF_TRACE_LEVEL_ERROR,
					  "Error: vdev %pK ("QDF_MAC_ADDR_FMT") trying to send bcast/mcast, but no self-peer found\n",
					  vdev,
					  QDF_MAC_ADDR_REF(vdev->mac_addr.raw));
				return NULL; /* error */
			}
		}
		tx_msdu_info->htt.info.is_unicast = false;
	} else {
		/* tid would be overwritten for non QoS case*/
		tid = ol_tx_tid(pdev, tx_nbuf, tx_msdu_info);
		if ((HTT_TX_EXT_TID_INVALID == tid) ||
		    (tid >= OL_TX_NUM_TIDS)) {
			QDF_TRACE(QDF_MODULE_ID_TXRX,
				  QDF_TRACE_LEVEL_ERROR,
				  "%s Error: could not classify packet into valid TID(%d).\n",
				  __func__, tid);
			return NULL;
		}
#ifdef ATH_SUPPORT_WAPI
		/* Check to see if a frame is a WAI frame */
		if (tx_msdu_info->htt.info.ethertype == ETHERTYPE_WAI) {
			/* WAI frames should not be encrypted */
			tx_msdu_info->htt.action.do_encrypt = 0;
			QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO,
				  "Tx Frame is a WAI frame\n");
		}
#endif /* ATH_SUPPORT_WAPI */

		/*
		 * Find the peer and increment its reference count.
		 * If this vdev is an AP, use the dest addr (DA) to determine
		 * which peer STA this unicast data frame is for.
		 * If this vdev is a STA, the unicast data frame is for the
		 * AP the STA is associated with.
		 */
		if (vdev->opmode == wlan_op_mode_sta) {
			/*
			 * TO DO:
			 * To support TDLS, first check if there is a TDLS
			 * peer STA,
			 * and if so, check if the DA matches the TDLS peer
			 * STA's MAC address. If there is no peer TDLS STA,
			 * or if the DA is not the TDLS STA's address,
			 * then the frame is either for the AP itself, or is
			 * supposed to be sent to the AP for forwarding.
			 */
			peer = ol_tx_tdls_peer_find(pdev, vdev,
						    dest_addr,
						    &peer_id);
		} else {
			peer = ol_txrx_peer_find_hash_find_get_ref(pdev,
								   dest_addr,
								   0, 1,
						PEER_DEBUG_ID_OL_INTERNAL);
		}
		tx_msdu_info->htt.info.is_unicast = true;
		if (!peer) {
			/*
			 * Unicast data xfer can only happen to an
			 * associated peer. It is illegitimate to send unicast
			 * data if there is no peer to send it to.
			 */
			ol_txrx_err_rl("Error: vdev %pK (" QDF_MAC_ADDR_FMT ") trying to send unicast tx data frame to an unknown peer",
				       vdev,
				       QDF_MAC_ADDR_REF(vdev->mac_addr.raw));
			return NULL; /* error */
		}
		TX_SCHED_DEBUG_PRINT("Peer found");
		if (!peer->qos_capable) {
			tid = OL_TX_NON_QOS_TID;
		} else if ((peer->security[
				OL_TXRX_PEER_SECURITY_UNICAST].sec_type
					!= htt_sec_type_wapi) &&
			   (qdf_nbuf_is_ipv4_pkt(tx_nbuf) == true)) {
			if (QDF_NBUF_CB_PACKET_TYPE_DHCP ==
					QDF_NBUF_CB_GET_PACKET_TYPE(tx_nbuf))
				/* DHCP frame to go with voice priority */
				tid = TX_DHCP_TID;
		}

		/* Only allow encryption when in authenticated state */
		if (OL_TXRX_PEER_STATE_AUTH != peer->state)
			tx_msdu_info->htt.action.do_encrypt = 0;

		txq = &peer->txqs[tid];
		tx_msdu_info->htt.info.ext_tid = tid;
		/*
		 * The following line assumes each peer object has a single ID.
		 * This is currently true, and is expected to remain true.
		 */
		tx_msdu_info->htt.info.peer_id = peer->peer_ids[0];
		/*
		 * WORKAROUND - check that the peer ID is valid.
		 * If tx data is provided before ol_rx_peer_map_handler is
		 * called to record the peer ID specified by the target,
		 * then we could end up here with an invalid peer ID.
		 * TO DO: rather than dropping the tx frame, pause the txq it
		 * goes into, then fill in the peer ID for the entries in the
		 * txq when the peer_map event provides the peer ID, and then
		 * unpause the txq.
		 */
		if (tx_msdu_info->htt.info.peer_id == HTT_INVALID_PEER_ID) {
			if (peer) {
				ol_txrx_info("remove the peer for invalid peer_id %pK",
					     peer);
				/* remove the peer reference added above */
				ol_txrx_peer_release_ref
						(peer,
						 PEER_DEBUG_ID_OL_INTERNAL);
				tx_msdu_info->peer = NULL;
			}
			return NULL;
		}
	}
	tx_msdu_info->peer = peer;
	if (ol_if_tx_bad_peer_txq_overflow(pdev, peer, txq))
		return NULL;
	/*
	 * If relevant, do a deeper inspection to determine additional
	 * characteristics of the tx frame.
	 * If the frame is invalid, then the txq will be set to NULL to
	 * indicate an error.
	 */
	OL_TX_CLASSIFY_EXTENSION(vdev, tx_desc, tx_nbuf, tx_msdu_info, txq);
	if (IEEE80211_IS_MULTICAST(dest_addr) && vdev->opmode !=
				wlan_op_mode_sta && tx_msdu_info->peer !=
								NULL) {
		ol_txrx_dbg("remove the peer reference %pK", peer);
		/* remove the peer reference added above */
		ol_txrx_peer_release_ref(tx_msdu_info->peer,
					 PEER_DEBUG_ID_OL_INTERNAL);
		/* Making peer NULL in case if multicast non STA mode */
		tx_msdu_info->peer = NULL;
	}

	/* Whether this frame can download though HTT2 data pipe or not. */
	OL_TX_CLASSIFY_HTT2_EXTENSION(vdev, tx_nbuf, tx_msdu_info);

	/* Update Tx Queue info */
	tx_desc->txq = txq;

	TX_SCHED_DEBUG_PRINT("Leave");
	return txq;
}

struct ol_tx_frms_queue_t *
ol_tx_classify_mgmt(
	struct ol_txrx_vdev_t *vdev,
	struct ol_tx_desc_t *tx_desc,
	qdf_nbuf_t tx_nbuf,
	struct ol_txrx_msdu_info_t *tx_msdu_info)
{
	struct ol_txrx_pdev_t *pdev = vdev->pdev;
	struct ol_txrx_peer_t *peer = NULL;
	struct ol_tx_frms_queue_t *txq = NULL;
	A_UINT8 *dest_addr;
	union ol_txrx_align_mac_addr_t local_mac_addr_aligned, *mac_addr;

	TX_SCHED_DEBUG_PRINT("Enter");
	dest_addr = ol_tx_dest_addr_find(pdev, tx_nbuf);
	if (unlikely(!dest_addr)) {
		QDF_TRACE(QDF_MODULE_ID_TXRX,
				QDF_TRACE_LEVEL_ERROR,
				"Error: dest_addr is NULL.\n");
		return NULL; /*error*/
	}
	if (IEEE80211_IS_MULTICAST(dest_addr)) {
		/*
		 * AP:  beacons are broadcast,
		 *      public action frames (e.g. extended channel
		 *      switch announce) may be broadcast
		 * STA: probe requests can be either broadcast or unicast
		 */
		txq = &vdev->txqs[OL_TX_VDEV_DEFAULT_MGMT];
		tx_msdu_info->htt.info.peer_id = HTT_INVALID_PEER_ID;
		tx_msdu_info->peer = NULL;
		tx_msdu_info->htt.info.is_unicast = 0;
	} else {
		/*
		 * Find the peer and increment its reference count.
		 * If this vdev is an AP, use the receiver addr (RA) to
		 * determine which peer STA this unicast mgmt frame is for.
		 * If this vdev is a STA, the unicast mgmt frame is for the
		 * AP the STA is associated with.
		 * Probe request / response and Assoc request / response are
		 * sent before the peer exists - in this case, use the
		 * vdev's default tx queue.
		 */
		if (vdev->opmode == wlan_op_mode_sta) {
			/*
			 * TO DO:
			 * To support TDLS, first check if there is a TDLS
			 * peer STA, and if so, check if the DA matches
			 * the TDLS peer STA's MAC address.
			 */
			peer = ol_txrx_assoc_peer_find(vdev);
			/*
			 * Some special case(preauth for example) needs to send
			 * unicast mgmt frame to unassociated AP. In such case,
			 * we need to check if dest addr match the associated
			 * peer addr. If not, we set peer as NULL to queue this
			 * frame to vdev queue.
			 */
			if (peer) {

				qdf_mem_copy(
					&local_mac_addr_aligned.raw[0],
					dest_addr, QDF_MAC_ADDR_SIZE);
				mac_addr = &local_mac_addr_aligned;
				if (ol_txrx_peer_find_mac_addr_cmp
						(mac_addr,
						 &peer->mac_addr) != 0) {
					ol_txrx_peer_release_ref
						(peer,
						 PEER_DEBUG_ID_OL_INTERNAL);
					peer = NULL;
				}
			}
		} else {
			/* find the peer and increment its reference count */
			peer = ol_txrx_peer_find_hash_find_get_ref(pdev,
								   dest_addr,
								   0, 1,
						PEER_DEBUG_ID_OL_INTERNAL);
		}
		tx_msdu_info->peer = peer;
		if (!peer) {
			txq = &vdev->txqs[OL_TX_VDEV_DEFAULT_MGMT];
			tx_msdu_info->htt.info.peer_id = HTT_INVALID_PEER_ID;
		} else {
			txq = &peer->txqs[HTT_TX_EXT_TID_MGMT];
			tx_msdu_info->htt.info.ext_tid = HTT_TX_EXT_TID_MGMT;
			/*
			 * The following line assumes each peer object has a
			 * single ID. This is currently true, and is expected
			 * to remain true.
			 */
			tx_msdu_info->htt.info.peer_id = peer->peer_ids[0];
		}
		tx_msdu_info->htt.info.is_unicast = 1;
	}
	/*
	 * If relevant, do a deeper inspection to determine additional
	 * characteristics of the tx frame.
	 * If the frame is invalid, then the txq will be set to NULL to
	 * indicate an error.
	 */
	OL_TX_CLASSIFY_MGMT_EXTENSION(vdev, tx_desc, tx_nbuf,
				      tx_msdu_info, txq);

	/* Whether this frame can download though HTT2 data pipe or not. */
	OL_TX_CLASSIFY_HTT2_EXTENSION(vdev, tx_nbuf, tx_msdu_info);

	/* Update Tx Queue info */
	tx_desc->txq = txq;

	TX_SCHED_DEBUG_PRINT("Leave");
	return txq;
}

#ifdef currently_unused
QDF_STATUS
ol_tx_classify_extension(
	struct ol_txrx_vdev_t *vdev,
	struct ol_tx_desc_t *tx_desc,
	qdf_nbuf_t tx_msdu,
	struct ol_txrx_msdu_info_t *msdu_info)
{
	u8 *datap = qdf_nbuf_data(tx_msdu);
	struct ol_txrx_peer_t *peer;
	int which_key;

	/*
	 * The following msdu_info fields were already filled in by the
	 * ol_tx entry function or the regular ol_tx_classify function:
	 *     htt.info.vdev_id            (ol_tx_hl or ol_tx_non_std_hl)
	 *     htt.info.ext_tid            (ol_tx_non_std_hl or ol_tx_classify)
	 *     htt.info.frame_type         (ol_tx_hl or ol_tx_non_std_hl)
	 *     htt.info.l2_hdr_type        (ol_tx_hl or ol_tx_non_std_hl)
	 *     htt.info.is_unicast         (ol_tx_classify)
	 *     htt.info.peer_id            (ol_tx_classify)
	 *     peer                        (ol_tx_classify)
	 *     if (is_unicast) {
	 *         htt.info.ethertype      (ol_tx_classify)
	 *         htt.info.l3_hdr_offset  (ol_tx_classify)
	 *     }
	 * The following fields need to be filled in by this function:
	 *     if (!is_unicast) {
	 *         htt.info.ethertype
	 *         htt.info.l3_hdr_offset
	 *     }
	 *     htt.action.band (NOT CURRENTLY USED)
	 *     htt.action.do_encrypt
	 *     htt.action.do_tx_complete
	 * The following fields are not needed for data frames, and can
	 * be left uninitialized:
	 *     htt.info.frame_subtype
	 */

	if (!msdu_info->htt.info.is_unicast) {
		int l2_hdr_size;
		u16 ethertype;

		if (msdu_info->htt.info.l2_hdr_type == htt_pkt_type_ethernet) {
			struct ethernet_hdr_t *eh;

			eh = (struct ethernet_hdr_t *)datap;
			l2_hdr_size = sizeof(*eh);
			ethertype = (eh->ethertype[0] << 8) | eh->ethertype[1];

			if (ethertype == ETHERTYPE_VLAN) {
				struct ethernet_vlan_hdr_t *evh;

				evh = (struct ethernet_vlan_hdr_t *)datap;
				l2_hdr_size = sizeof(*evh);
				ethertype = (evh->ethertype[0] << 8) |
							evh->ethertype[1];
			}

			if (!IS_ETHERTYPE(ethertype)) {
				/* 802.3 header*/
				struct llc_snap_hdr_t *llc =
					(struct llc_snap_hdr_t *)(datap +
							l2_hdr_size);
				ethertype = (llc->ethertype[0] << 8) |
							llc->ethertype[1];
				l2_hdr_size += sizeof(*llc);
			}
			msdu_info->htt.info.l3_hdr_offset = l2_hdr_size;
			msdu_info->htt.info.ethertype = ethertype;
		} else { /* 802.11 */
			struct llc_snap_hdr_t *llc;

			l2_hdr_size = ol_txrx_ieee80211_hdrsize(datap);
			llc = (struct llc_snap_hdr_t *)(datap + l2_hdr_size);
			ethertype = (llc->ethertype[0] << 8) |
							llc->ethertype[1];
			/*
			 * Don't include the LLC/SNAP header in l2_hdr_size,
			 * because l3_hdr_offset is actually supposed to refer
			 * to the header after the 802.3 or 802.11 header,
			 * which could be a LLC/SNAP header rather
			 * than the L3 header.
			 */
		}
		msdu_info->htt.info.l3_hdr_offset = l2_hdr_size;
		msdu_info->htt.info.ethertype = ethertype;
		which_key = txrx_sec_mcast;
	} else {
		which_key = txrx_sec_ucast;
	}
	peer = msdu_info->peer;
	/*
	 * msdu_info->htt.action.do_encrypt is initially set in ol_tx_desc_hl.
	 * Add more check here.
	 */
	msdu_info->htt.action.do_encrypt = (!peer) ? 0 :
		(peer->security[which_key].sec_type == htt_sec_type_none) ? 0 :
		msdu_info->htt.action.do_encrypt;
	/*
	 * For systems that have a frame by frame spec for whether to receive
	 * a tx completion notification, use the tx completion notification
	 * only  for certain management frames, not for data frames.
	 * (In the future, this may be changed slightly, e.g. to request a
	 * tx completion notification for the final EAPOL message sent by a
	 * STA during the key delivery handshake.)
	 */
	msdu_info->htt.action.do_tx_complete = 0;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
ol_tx_classify_mgmt_extension(
		struct ol_txrx_vdev_t *vdev,
		struct ol_tx_desc_t *tx_desc,
		qdf_nbuf_t tx_msdu,
		struct ol_txrx_msdu_info_t *msdu_info)
{
	struct ieee80211_frame *wh;

	/*
	 * The following msdu_info fields were already filled in by the
	 * ol_tx entry function or the regular ol_tx_classify_mgmt function:
	 *     htt.info.vdev_id          (ol_txrx_mgmt_send)
	 *     htt.info.frame_type       (ol_txrx_mgmt_send)
	 *     htt.info.l2_hdr_type      (ol_txrx_mgmt_send)
	 *     htt.action.do_tx_complete (ol_txrx_mgmt_send)
	 *     htt.info.peer_id          (ol_tx_classify_mgmt)
	 *     htt.info.ext_tid          (ol_tx_classify_mgmt)
	 *     htt.info.is_unicast       (ol_tx_classify_mgmt)
	 *     peer                      (ol_tx_classify_mgmt)
	 * The following fields need to be filled in by this function:
	 *     htt.info.frame_subtype
	 *     htt.info.l3_hdr_offset
	 *     htt.action.band (NOT CURRENTLY USED)
	 * The following fields are not needed for mgmt frames, and can
	 * be left uninitialized:
	 *     htt.info.ethertype
	 *     htt.action.do_encrypt
	 *         (This will be filled in by other SW, which knows whether
	 *         the peer has robust-management-frames enabled.)
	 */
	wh = (struct ieee80211_frame *)qdf_nbuf_data(tx_msdu);
	msdu_info->htt.info.frame_subtype =
		(wh->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK) >>
		IEEE80211_FC0_SUBTYPE_SHIFT;
	msdu_info->htt.info.l3_hdr_offset = sizeof(struct ieee80211_frame);

	return QDF_STATUS_SUCCESS;
}
#endif
