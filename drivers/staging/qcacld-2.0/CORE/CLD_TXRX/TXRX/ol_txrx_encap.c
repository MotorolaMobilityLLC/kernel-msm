/*
 * Copyright (c) 2012-2013 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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

/*
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

/**
 * @file ol_txrx_encap.c
 * @brief Provide functions to encap/decap on txrx frames.
 * @details
 *  This file contains functions for data frame encap/decap:
 *  ol_tx_encap: encap outgoing data frames.
 *  ol_rx_decap: decap incoming data frames.
 */
#ifdef QCA_SUPPORT_SW_TXRX_ENCAP

#include <adf_nbuf.h>         /* adf_nbuf_t, etc. */
#include <ieee80211_common.h>         /* ieee80211_frame */
#include <net.h>               /* struct llc,struct struct ether_header, etc. */
#include <ol_txrx_internal.h> /* TXRX_ASSERT1 */
#include <ol_txrx_types.h>    /* struct ol_txrx_vdev_t,struct ol_txrx_pdev_t,etc. */
#include <ol_txrx_encap.h>    /* struct ol_rx_decap_info_t*/

#define OL_TX_COPY_NATIVE_WIFI_HEADER(wh, msdu, hdsize, localbuf) \
    do { \
        wh = (struct ieee80211_frame*)adf_nbuf_data(msdu); \
        if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) { \
            hdsize = sizeof(struct ieee80211_frame_addr4); \
        } else { \
            hdsize = sizeof(struct ieee80211_frame); \
        } \
        if ( adf_nbuf_len(msdu) < hdsize) { \
            return A_ERROR; \
        } \
        adf_os_mem_copy(localbuf, wh, hdsize); \
        wh = (struct ieee80211_frame*)localbuf; \
    } while (0);

static inline A_STATUS
ol_tx_copy_native_wifi_header(
    adf_nbuf_t msdu,
    u_int8_t *hdsize,
    u_int8_t *localbuf)
{
    struct ieee80211_frame *wh = (struct ieee80211_frame*)adf_nbuf_data(msdu);
    if ((wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) {
        *hdsize = sizeof(struct ieee80211_frame_addr4);
    } else {
        *hdsize = sizeof(struct ieee80211_frame);
    }
    if (adf_nbuf_len(msdu) < *hdsize) {
        return A_ERROR;
    }
    adf_os_mem_copy(localbuf, wh, *hdsize);
    return A_OK;
}

static inline A_STATUS
ol_tx_encap_from_native_wifi (
    struct ol_txrx_vdev_t *vdev,
    struct ol_tx_desc_t *tx_desc,
    adf_nbuf_t msdu,
    struct ol_txrx_msdu_info_t *tx_msdu_info
)
{
    u_int8_t localbuf[sizeof(struct ieee80211_qosframe_htc_addr4)];
    struct ieee80211_frame *wh;
    u_int8_t hdsize, new_hdsize;
    struct ieee80211_qoscntl *qos_cntl;
    struct ol_txrx_peer_t *peer;

    if (tx_msdu_info->htt.info.frame_type != htt_frm_type_data) {
        return A_OK;
    }
    peer = tx_msdu_info->peer;
    /*
     * for unicast,the peer should not be NULL.
     * for multicast, the peer is AP.
     */
    if (tx_msdu_info->htt.info.is_unicast
        && peer->qos_capable)
    {
        if (A_OK != ol_tx_copy_native_wifi_header(msdu, &hdsize, localbuf))
            return A_ERROR;
        wh = (struct ieee80211_frame*)localbuf;

        /*add qos cntl*/
        qos_cntl = (struct ieee80211_qoscntl*)(localbuf + hdsize);
        qos_cntl->i_qos[0] = tx_msdu_info->htt.info.ext_tid & IEEE80211_QOS_TID;

#if 0
        if ( wmmParam[ac].wmep_noackPolicy ) {
            qos_cntl->i_qos[0]|= 1 << IEEE80211_QOS_ACKPOLICY_S;
        }
#endif

        qos_cntl->i_qos[1] = 0;
        wh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_QOS;
        /* count for qos field */
        new_hdsize = hdsize + sizeof(struct ieee80211_qosframe) - sizeof(struct ieee80211_frame);

        /*add ht control field if needed */

        /* copy new hd to bd */
        adf_os_mem_copy(
                       (void*)htt_tx_desc_mpdu_header(tx_desc->htt_tx_desc, new_hdsize),
                       localbuf,
                       new_hdsize);
        adf_nbuf_pull_head(msdu, hdsize);
        tx_msdu_info->htt.info.l3_hdr_offset = new_hdsize;
        tx_desc->orig_l2_hdr_bytes = hdsize;
    }
    /* Set Protected Frame bit in MAC header */
    if (vdev->pdev->sw_pf_proc_enable && tx_msdu_info->htt.action.do_encrypt) {
        if (tx_desc->orig_l2_hdr_bytes) {
            wh = (struct ieee80211_frame*)htt_tx_desc_mpdu_header(tx_desc->htt_tx_desc,
                tx_msdu_info->htt.info.l3_hdr_offset);
        } else {
            if (A_OK != ol_tx_copy_native_wifi_header(msdu, &hdsize, localbuf))
                return A_ERROR;
            wh = (struct ieee80211_frame*)htt_tx_desc_mpdu_header(tx_desc->htt_tx_desc, hdsize);
            adf_os_mem_copy((void*)wh, localbuf, hdsize);
            adf_nbuf_pull_head(msdu, hdsize);
            tx_msdu_info->htt.info.l3_hdr_offset = hdsize;
            tx_desc->orig_l2_hdr_bytes = hdsize;
        }
        wh->i_fc[1] |= IEEE80211_FC1_WEP;
    }
    return A_OK;
}

static inline A_STATUS
ol_tx_encap_from_8023 (
    struct ol_txrx_vdev_t *vdev,
    struct ol_tx_desc_t *tx_desc,
    adf_nbuf_t msdu,
    struct ol_txrx_msdu_info_t *tx_msdu_info
)
{
    u_int8_t localbuf[ sizeof(struct ieee80211_qosframe_htc_addr4) \
                       + sizeof(struct llc_snap_hdr_t)];
    struct llc_snap_hdr_t *llc_hdr;
    struct ethernet_hdr_t *eth_hdr;
    struct ieee80211_frame *wh;
    u_int8_t hdsize, new_l2_hdsize, new_hdsize;
    struct ieee80211_qoscntl *qos_cntl;
    const u_int8_t ethernet_II_llc_snap_header_prefix[] = \
                                { 0xaa, 0xaa, 0x03, 0x00, 0x00, 0x00 };
    struct ol_txrx_peer_t *peer;
    u_int16_t ether_type;

    if (tx_msdu_info->htt.info.frame_type != htt_frm_type_data)
        return A_OK;

    /*
     * for unicast,the peer should not be NULL.
     * for multicast, the peer is AP.
     */
    peer = tx_msdu_info->peer;

    eth_hdr = (struct ethernet_hdr_t *)adf_nbuf_data(msdu);
    hdsize = sizeof(struct ethernet_hdr_t);
    wh = (struct ieee80211_frame *)localbuf;
    wh->i_fc[0] = IEEE80211_FC0_VERSION_0 | IEEE80211_FC0_TYPE_DATA;
    *(u_int16_t *)wh->i_dur = 0;
    new_hdsize = 0;

    switch (vdev->opmode) {
    case wlan_op_mode_ap:
        /* DA , BSSID , SA*/
        adf_os_mem_copy(wh->i_addr1, eth_hdr->dest_addr,
                        IEEE80211_ADDR_LEN);
        adf_os_mem_copy(wh->i_addr2, &vdev->mac_addr.raw,
                        IEEE80211_ADDR_LEN);
        adf_os_mem_copy(wh->i_addr3, eth_hdr->src_addr,
                        IEEE80211_ADDR_LEN);
        wh->i_fc[1] = IEEE80211_FC1_DIR_FROMDS;
        new_hdsize = sizeof(struct ieee80211_frame);
        break;
    case wlan_op_mode_ibss:
        /* DA, SA, BSSID */
        adf_os_mem_copy(wh->i_addr1, eth_hdr->dest_addr,
                        IEEE80211_ADDR_LEN);
        adf_os_mem_copy(wh->i_addr2, eth_hdr->src_addr,
                        IEEE80211_ADDR_LEN);
         /* need to check the bssid behaviour for IBSS vdev */
        adf_os_mem_copy(wh->i_addr3, &vdev->mac_addr.raw,
                        IEEE80211_ADDR_LEN);
        wh->i_fc[1] = IEEE80211_FC1_DIR_NODS;
        new_hdsize = sizeof(struct ieee80211_frame);
        break;
    case wlan_op_mode_sta:
        /* BSSID, SA , DA*/
        adf_os_mem_copy(wh->i_addr1, &peer->mac_addr.raw,
                        IEEE80211_ADDR_LEN);
        adf_os_mem_copy(wh->i_addr2, eth_hdr->src_addr,
                        IEEE80211_ADDR_LEN);
        adf_os_mem_copy(wh->i_addr3, eth_hdr->dest_addr,
                        IEEE80211_ADDR_LEN);
        wh->i_fc[1] = IEEE80211_FC1_DIR_TODS;
        new_hdsize = sizeof(struct ieee80211_frame);
        break;
    case wlan_op_mode_monitor:
    default:
        return A_ERROR;
    }
    /*add qos cntl*/
    if (tx_msdu_info->htt.info.is_unicast && peer->qos_capable ) {
        qos_cntl = (struct ieee80211_qoscntl*)(localbuf + new_hdsize);
        qos_cntl->i_qos[0] = tx_msdu_info->htt.info.ext_tid & IEEE80211_QOS_TID;
        wh->i_fc[0] |= IEEE80211_FC0_SUBTYPE_QOS;
#if 0
        if ( wmmParam[ac].wmep_noackPolicy ) {
            qos_cntl->i_qos[0]|= 1 << IEEE80211_QOS_ACKPOLICY_S;
        }
#endif
        qos_cntl->i_qos[1] = 0;
        new_hdsize += sizeof(struct ieee80211_qoscntl);

        /*add ht control field if needed */
    }
    /* Set Protected Frame bit in MAC header */
    if (vdev->pdev->sw_pf_proc_enable && tx_msdu_info->htt.action.do_encrypt) {
        wh->i_fc[1] |= IEEE80211_FC1_WEP;
    }
    new_l2_hdsize = new_hdsize;
    /* add llc snap if needed */
    if (vdev->pdev->sw_tx_llc_proc_enable) {
        llc_hdr = (struct llc_snap_hdr_t *) (localbuf + new_hdsize);
        ether_type = (eth_hdr->ethertype[0]<<8) |(eth_hdr->ethertype[1]);
        if ( ether_type >= IEEE8023_MAX_LEN ) {
            adf_os_mem_copy(llc_hdr,
                            ethernet_II_llc_snap_header_prefix,
                            sizeof(ethernet_II_llc_snap_header_prefix));
            if ( ether_type == ETHERTYPE_AARP || ether_type == ETHERTYPE_IPX) {
                llc_hdr->org_code[2] = BTEP_SNAP_ORGCODE_2;// 0xf8; bridge tunnel header
            }
            llc_hdr->ethertype[0] = eth_hdr->ethertype[0];
            llc_hdr->ethertype[1] = eth_hdr->ethertype[1];
            new_hdsize += sizeof(struct llc_snap_hdr_t);
        }
        else {
            /*llc ready, and it's in payload pdu, do we need to move to BD pdu?*/
        }
    }
    adf_os_mem_copy(
                    (void*)htt_tx_desc_mpdu_header(tx_desc->htt_tx_desc,new_l2_hdsize),
                    localbuf,
                    new_hdsize);
    adf_nbuf_pull_head(msdu,hdsize);
    tx_msdu_info->htt.info.l3_hdr_offset = new_l2_hdsize;
    tx_desc->orig_l2_hdr_bytes = hdsize;
    return A_OK;
}

A_STATUS
ol_tx_encap(
    struct ol_txrx_vdev_t *vdev,
    struct ol_tx_desc_t *tx_desc,
    adf_nbuf_t msdu,
    struct ol_txrx_msdu_info_t *msdu_info)
{
    struct ol_txrx_pdev_t *pdev = vdev->pdev;

    if (pdev->frame_format == wlan_frm_fmt_native_wifi) {
        return ol_tx_encap_from_native_wifi(vdev, tx_desc, msdu,msdu_info);
    } else if (pdev->frame_format == wlan_frm_fmt_802_3) {
        return ol_tx_encap_from_8023(vdev, tx_desc, msdu, msdu_info);
    } else {
        /* todo for other types */
        return A_ERROR;
    }
}

static inline void
ol_rx_decap_to_native_wifi(
    struct ol_txrx_vdev_t *vdev,
    adf_nbuf_t msdu,
    struct ol_rx_decap_info_t *info,
    struct ethernet_hdr_t *ethr_hdr)
{
    struct ieee80211_frame_addr4 *wh;
    u_int16_t hdsize;

    /*
     * we need to remove Qos control field and HT control.
     * MSFT: http://msdn.microsoft.com/en-us/library/windows/hardware/ff552608(v=vs.85).aspx
     */
    wh = (struct ieee80211_frame_addr4 *)info->hdr;
    if ( (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) == IEEE80211_FC1_DIR_DSTODS) {
        hdsize = sizeof(struct ieee80211_frame_addr4);
    }
    else {
        hdsize = sizeof(struct ieee80211_frame);
    }
    wh = (struct ieee80211_frame_addr4 *) adf_nbuf_push_head(msdu, hdsize);
    TXRX_ASSERT2(wh != NULL);
    TXRX_ASSERT2(hdsize <= info->hdr_len);
    adf_os_mem_copy ((u_int8_t *)wh, info->hdr, hdsize);

    /* amsdu subfrm handling if ethr_hdr is not NULL  */
    if (ethr_hdr != NULL) {
        switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
        case IEEE80211_FC1_DIR_NODS:
            adf_os_mem_copy(wh->i_addr1, ethr_hdr->dest_addr, ETHERNET_ADDR_LEN);
            adf_os_mem_copy(wh->i_addr2, ethr_hdr->src_addr, ETHERNET_ADDR_LEN);
            break;
        case IEEE80211_FC1_DIR_TODS:
            adf_os_mem_copy(wh->i_addr2, ethr_hdr->src_addr, ETHERNET_ADDR_LEN);
            adf_os_mem_copy(wh->i_addr3, ethr_hdr->dest_addr, ETHERNET_ADDR_LEN);
            break;
        case IEEE80211_FC1_DIR_FROMDS:
            adf_os_mem_copy(wh->i_addr1, ethr_hdr->dest_addr, ETHERNET_ADDR_LEN);
            adf_os_mem_copy(wh->i_addr3, ethr_hdr->src_addr, ETHERNET_ADDR_LEN);
            break;
        case IEEE80211_FC1_DIR_DSTODS:
            adf_os_mem_copy(wh->i_addr3, ethr_hdr->dest_addr, ETHERNET_ADDR_LEN);
            adf_os_mem_copy(wh->i_addr4, ethr_hdr->src_addr, ETHERNET_ADDR_LEN);
            break;
        }
    }
    if (IEEE80211_QOS_HAS_SEQ(wh) ) {
        if (wh->i_fc[1] & IEEE80211_FC1_ORDER) {
            wh->i_fc[1] &= ~IEEE80211_FC1_ORDER;
        }
        wh->i_fc[0] &= ~IEEE80211_FC0_SUBTYPE_QOS;
    }
}

static inline void
ol_rx_decap_to_8023 (
    struct ol_txrx_vdev_t *vdev,
    adf_nbuf_t msdu,
    struct ol_rx_decap_info_t *info,
    struct ethernet_hdr_t *ethr_hdr)
{
    struct llc_snap_hdr_t *llc_hdr;
    u_int16_t ether_type;
    u_int16_t l2_hdr_space;
    struct ieee80211_frame_addr4 *wh;
    u_int8_t local_buf[ETHERNET_HDR_LEN];
    u_int8_t *buf;

    /*
    * populate Ethernet header,
    * if ethr_hdr is null, rx frame is 802.11 format(HW ft disabled)
    * if ethr_hdr is not null, rx frame is "subfrm of amsdu".
    */
    buf = (u_int8_t *)adf_nbuf_data(msdu);
    llc_hdr = (struct llc_snap_hdr_t *)buf;
    ether_type = (llc_hdr->ethertype[0] << 8)|llc_hdr->ethertype[1];
    /* do llc remove if needed */
    l2_hdr_space = 0;
    if (IS_SNAP(llc_hdr)) {
        if (IS_BTEP(llc_hdr)) {
            /* remove llc*/
            l2_hdr_space += sizeof(struct llc_snap_hdr_t);
            llc_hdr = NULL;
        }
        else if (IS_RFC1042(llc_hdr)) {
            if ( !(ether_type == ETHERTYPE_AARP ||
                  ether_type == ETHERTYPE_IPX) ) {
                /* remove llc*/
                l2_hdr_space += sizeof(struct llc_snap_hdr_t);
                llc_hdr = NULL;
            }
        }
    }
    if (l2_hdr_space > ETHERNET_HDR_LEN) {
        buf = adf_nbuf_pull_head(msdu, l2_hdr_space - ETHERNET_HDR_LEN);
    }
    else if (l2_hdr_space <  ETHERNET_HDR_LEN) {
        buf = adf_nbuf_push_head(msdu, ETHERNET_HDR_LEN - l2_hdr_space);
    }

    /* normal msdu(non-subfrm of A-MSDU) if ethr_hdr is null */
    if (ethr_hdr == NULL) {
        /* mpdu hdr should be present in info,re-create ethr_hdr based on mpdu hdr*/
        TXRX_ASSERT2(info->hdr_len != 0);
        wh = (struct ieee80211_frame_addr4 *)info->hdr;
        ethr_hdr = (struct ethernet_hdr_t *)local_buf;
        switch (wh->i_fc[1] & IEEE80211_FC1_DIR_MASK) {
        case IEEE80211_FC1_DIR_NODS:
            adf_os_mem_copy(ethr_hdr->dest_addr, wh->i_addr1, ETHERNET_ADDR_LEN);
            adf_os_mem_copy(ethr_hdr->src_addr, wh->i_addr2, ETHERNET_ADDR_LEN);
            break;
        case IEEE80211_FC1_DIR_TODS:
            adf_os_mem_copy(ethr_hdr->dest_addr, wh->i_addr3, ETHERNET_ADDR_LEN);
            adf_os_mem_copy(ethr_hdr->src_addr, wh->i_addr2, ETHERNET_ADDR_LEN);
            break;
        case IEEE80211_FC1_DIR_FROMDS:
            adf_os_mem_copy(ethr_hdr->dest_addr, wh->i_addr1, ETHERNET_ADDR_LEN);
            adf_os_mem_copy(ethr_hdr->src_addr, wh->i_addr3, ETHERNET_ADDR_LEN);
            break;
        case IEEE80211_FC1_DIR_DSTODS:
            adf_os_mem_copy(ethr_hdr->dest_addr, wh->i_addr3, ETHERNET_ADDR_LEN);
            adf_os_mem_copy(ethr_hdr->src_addr, wh->i_addr4, ETHERNET_ADDR_LEN);
            break;
        }
    }
    if (llc_hdr == NULL) {
        ethr_hdr->ethertype[0] = (ether_type >> 8) & 0xff;
        ethr_hdr->ethertype[1] = (ether_type) & 0xff;
    }
    else {
        u_int32_t pktlen = adf_nbuf_len(msdu) - sizeof(ethr_hdr->ethertype);
        TXRX_ASSERT2(pktlen <= ETHERNET_MTU);
        ether_type = (u_int16_t)pktlen;
        ether_type = adf_nbuf_len(msdu) - sizeof(struct ethernet_hdr_t);
        ethr_hdr->ethertype[0] = (ether_type >> 8) & 0xff;
        ethr_hdr->ethertype[1] = (ether_type) & 0xff;
    }
    adf_os_mem_copy(buf, ethr_hdr, ETHERNET_HDR_LEN);
}

static inline A_STATUS
ol_rx_decap_subfrm_amsdu(
    struct ol_txrx_vdev_t *vdev,
    adf_nbuf_t msdu,
    struct ol_rx_decap_info_t *info)
{
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    u_int8_t * subfrm_hdr;
    u_int8_t localbuf[ETHERNET_HDR_LEN];
    struct ethernet_hdr_t *ether_hdr = (struct ethernet_hdr_t*)localbuf;

    subfrm_hdr = (u_int8_t *)adf_nbuf_data(msdu);
    if (pdev->frame_format == wlan_frm_fmt_native_wifi) {
        /* decap to native wifi */
        adf_os_mem_copy(ether_hdr,
                        subfrm_hdr,
                        ETHERNET_HDR_LEN);
        adf_nbuf_pull_head(msdu, ETHERNET_HDR_LEN);
        ol_rx_decap_to_native_wifi(vdev,
                                msdu,
                                info,
                                ether_hdr);
    }
    else if (pdev->frame_format == wlan_frm_fmt_802_3) {
        if (pdev->sw_rx_llc_proc_enable)  {
             /* remove llc snap hdr if it's necessary according to
             * 802.11 table P-3
             */
            adf_os_mem_copy(ether_hdr,
                            subfrm_hdr,
                            ETHERNET_HDR_LEN);
            adf_nbuf_pull_head(msdu, ETHERNET_HDR_LEN);
            ol_rx_decap_to_8023(vdev,
                                msdu,
                                info,
                                ether_hdr);
        }
        else {
            /* subfrm of A-MSDU is already in 802.3 format.
             * if target HW or FW has done LLC rmv process,
             * we do nothing here.
             */
        }
    }
    else {
        /* todo for othertype*/
    }
    return A_OK;

}

static inline A_STATUS
ol_rx_decap_msdu(
    struct ol_txrx_vdev_t *vdev,
    adf_nbuf_t msdu,
    struct ol_rx_decap_info_t *info)
{
    struct ol_txrx_pdev_t *pdev = vdev->pdev;
    struct ieee80211_frame *wh;
    wh = (struct ieee80211_frame *)adf_nbuf_data(msdu);

    if (pdev->frame_format == wlan_frm_fmt_native_wifi) {
        /* Decap to native wifi because according to MSFT(
         * MSFT: http://msdn.microsoft.com/en-us/library/windows/hardware/ff552608(v=vs.85).aspx),
         * we need to remove Qos and HTC field before indicate to OS.
         */
        if (IEEE80211_QOS_HAS_SEQ(wh) ) {
            info->hdr_len = ol_txrx_ieee80211_hdrsize(wh);
            TXRX_ASSERT2(info->hdr_len <= sizeof(info->hdr));
            adf_os_mem_copy(info->hdr, /* use info->hdr as temp buf.*/
                            wh,
                            info->hdr_len);
            adf_nbuf_pull_head(msdu, info->hdr_len);
            ol_rx_decap_to_native_wifi(vdev,
                                       msdu,
                                       info, /* 802.11 hdr*/
                                       NULL); /* ethernet hdr*/
        }
    }
    else if (pdev->frame_format == wlan_frm_fmt_802_3) {
        if (pdev->sw_rx_llc_proc_enable)  {
            info->hdr_len = ol_txrx_ieee80211_hdrsize(wh);
            TXRX_ASSERT2(info->hdr_len <= sizeof(info->hdr));
            adf_os_mem_copy(info->hdr, /* use info->hdr as temp buf.*/
                            wh,
                            info->hdr_len);
            adf_nbuf_pull_head(msdu, info->hdr_len);
            /* remove llc snap hdr if it's necessary according to
             * 802.11 table P-3
             */
            ol_rx_decap_to_8023(vdev,
                                msdu,
                                info, /* 802.11 hdr*/
                                NULL); /* ethernet hdr*/
        }
        else {
            /* Subfrm of A-MSDU is already in 802.3 format.
             * And if target HW or FW has done LLC rmv process (
             * sw_rx_lc_proc_enable == 0), we do nothing here.
             */
        }
    }
    else {
         /* todo for othertype*/
    }
    return A_OK;

}

A_STATUS
ol_rx_decap(
    struct ol_txrx_vdev_t *vdev,
    struct ol_txrx_peer_t *peer,
    adf_nbuf_t msdu,
    struct ol_rx_decap_info_t *info)
{
    A_STATUS status;
    u_int8_t *mpdu_hdr;

    if (!info->is_subfrm) {
        if (info->is_msdu_cmpl_mpdu && !info->is_first_subfrm) {
            /* It's normal MSDU. */
        } else {
            /* It's a first subfrm of A-MSDU and may also be the last subfrm of A-MSDU */
            info->is_subfrm = 1;
            info->hdr_len = 0;
            if (vdev->pdev->sw_subfrm_hdr_recovery_enable) {
                /* we save the first subfrm mpdu hdr for subsequent
                 * subfrm 802.11 header recovery in certain chip(such as Riva).
                 */
                mpdu_hdr = adf_nbuf_data(msdu);
                info->hdr_len = ol_txrx_ieee80211_hdrsize(mpdu_hdr);
                TXRX_ASSERT2(info->hdr_len <= sizeof(info->hdr));
                adf_os_mem_copy(info->hdr, mpdu_hdr, info->hdr_len);
                adf_nbuf_pull_head(msdu, info->hdr_len);
            }
        }
    }

    if (info->is_subfrm && vdev->pdev->sw_subfrm_hdr_recovery_enable) {
       /*
        * This case is enabled for some HWs(such as Riva).The HW de-aggregate
        * doesn't have capability to generate 802.11 header for non-first subframe
        * of A-MSDU.That means sw needs to cache the first subfrm mpdu header
        * to generate the subsequent subfrm's 802.11 header.
        */
        TXRX_ASSERT2(info->hdr_len != 0);
        status = ol_rx_decap_subfrm_amsdu(vdev, msdu, info);
    } else {
        status = ol_rx_decap_msdu(vdev, msdu, info);
    }

    if (info->is_msdu_cmpl_mpdu) {
        info->is_subfrm =
        info->is_first_subfrm =
        info->hdr_len = 0;
    }
    return status;
}
#endif
