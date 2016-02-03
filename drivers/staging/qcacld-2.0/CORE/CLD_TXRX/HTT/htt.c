/*
 * Copyright (c) 2011, 2014-2015 The Linux Foundation. All rights reserved.
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
 * @file htt.c
 * @brief Provide functions to create+init and destroy a HTT instance.
 * @details
 *  This file contains functions for creating a HTT instance; initializing
 *  the HTT instance, e.g. by allocating a pool of HTT tx descriptors and
 *  connecting the HTT service with HTC; and deleting a HTT instance.
 */

#include <adf_os_mem.h>      /* adf_os_mem_alloc */
#include <adf_os_types.h>    /* adf_os_device_t, adf_os_print */

#include <htt.h>             /* htt_tx_msdu_desc_t */
#include <ol_cfg.h>
#include <ol_txrx_htt_api.h> /* ol_tx_dowload_done_ll, etc. */
#include <ol_htt_api.h>

#include <htt_internal.h>
#if defined(HIF_PCI)
#include "if_pci.h"
#endif

#define HTT_HTC_PKT_POOL_INIT_SIZE 100 /* enough for a large A-MPDU */

A_STATUS
htt_h2t_rx_ring_cfg_msg_ll(struct htt_pdev_t *pdev);

A_STATUS
htt_h2t_rx_ring_cfg_msg_hl(struct htt_pdev_t *pdev);

A_STATUS (*htt_h2t_rx_ring_cfg_msg)(
        struct htt_pdev_t *pdev);

#ifdef IPA_UC_OFFLOAD
A_STATUS
htt_ipa_config(htt_pdev_handle pdev, A_STATUS status)
{
    if ((A_OK == status) &&
        ol_cfg_ipa_uc_offload_enabled(pdev->ctrl_pdev)) {
        status = htt_h2t_ipa_uc_rsc_cfg_msg(pdev);
    }
    return status;
}

#define HTT_IPA_CONFIG htt_ipa_config
#else
#define HTT_IPA_CONFIG(pdev, status) status /* no-op */
#endif /* IPA_UC_OFFLOAD */


struct htt_htc_pkt *
htt_htc_pkt_alloc(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt_union *pkt = NULL;

    HTT_TX_MUTEX_ACQUIRE(&pdev->htt_tx_mutex);
    if (pdev->htt_htc_pkt_freelist) {
        pkt = pdev->htt_htc_pkt_freelist;
        pdev->htt_htc_pkt_freelist = pdev->htt_htc_pkt_freelist->u.next;
    }
    HTT_TX_MUTEX_RELEASE(&pdev->htt_tx_mutex);

    if (pkt == NULL) {
        pkt = adf_os_mem_alloc(pdev->osdev, sizeof(*pkt));
    }
    return &pkt->u.pkt; /* not actually a dereference */
}

void
htt_htc_pkt_free(struct htt_pdev_t *pdev, struct htt_htc_pkt *pkt)
{
    struct htt_htc_pkt_union *u_pkt = (struct htt_htc_pkt_union *) pkt;

    HTT_TX_MUTEX_ACQUIRE(&pdev->htt_tx_mutex);
    u_pkt->u.next = pdev->htt_htc_pkt_freelist;
    pdev->htt_htc_pkt_freelist = u_pkt;
    HTT_TX_MUTEX_RELEASE(&pdev->htt_tx_mutex);
}

void
htt_htc_pkt_pool_free(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt_union *pkt, *next;
    pkt = pdev->htt_htc_pkt_freelist;
    while (pkt) {
        next = pkt->u.next;
        adf_os_mem_free(pkt);
        pkt = next;
    }
    pdev->htt_htc_pkt_freelist = NULL;
}

#ifdef ATH_11AC_TXCOMPACT
void
htt_htc_misc_pkt_list_trim(struct htt_pdev_t *pdev, int level)
{
    struct htt_htc_pkt_union *pkt, *next, *prev = NULL;
    int i = 0;
    adf_nbuf_t netbuf;

    HTT_TX_MUTEX_ACQUIRE(&pdev->htt_tx_mutex);
    pkt = pdev->htt_htc_pkt_misclist;
    while (pkt) {
        next = pkt->u.next;
        /* trim the out grown list*/
        if (++i > level) {
            netbuf = (adf_nbuf_t)(pkt->u.pkt.htc_pkt.pNetBufContext);
            adf_nbuf_unmap(pdev->osdev, netbuf, ADF_OS_DMA_TO_DEVICE);
            adf_nbuf_free(netbuf);
            adf_os_mem_free(pkt);
            pkt = NULL;
            if (prev)
                prev->u.next = NULL;
        }
        prev = pkt;
        pkt = next;
    }
    HTT_TX_MUTEX_RELEASE(&pdev->htt_tx_mutex);
}

void
htt_htc_misc_pkt_list_add(struct htt_pdev_t *pdev, struct htt_htc_pkt *pkt)
{
    struct htt_htc_pkt_union *u_pkt = (struct htt_htc_pkt_union *) pkt;

    HTT_TX_MUTEX_ACQUIRE(&pdev->htt_tx_mutex);
    if (pdev->htt_htc_pkt_misclist) {
        u_pkt->u.next = pdev->htt_htc_pkt_misclist;
        pdev->htt_htc_pkt_misclist = u_pkt;
    } else {
        pdev->htt_htc_pkt_misclist = u_pkt;
    }
    HTT_TX_MUTEX_RELEASE(&pdev->htt_tx_mutex);
    htt_htc_misc_pkt_list_trim(pdev, HTT_HTC_PKT_MISCLIST_SIZE);
}

void
htt_htc_misc_pkt_pool_free(struct htt_pdev_t *pdev)
{
    struct htt_htc_pkt_union *pkt, *next;
    adf_nbuf_t netbuf;
    pkt = pdev->htt_htc_pkt_misclist;

    while (pkt) {
        next = pkt->u.next;
        netbuf = (adf_nbuf_t)(pkt->u.pkt.htc_pkt.pNetBufContext);
        adf_nbuf_unmap(pdev->osdev, netbuf, ADF_OS_DMA_TO_DEVICE);
        adf_nbuf_free(netbuf);
        adf_os_mem_free(pkt);
        pkt = next;
    }
    pdev->htt_htc_pkt_misclist = NULL;
}
#endif

/*---*/

htt_pdev_handle
htt_attach(
    ol_txrx_pdev_handle txrx_pdev,
    ol_pdev_handle ctrl_pdev,
    HTC_HANDLE htc_pdev,
    adf_os_device_t osdev,
    int desc_pool_size)
{
    struct htt_pdev_t *pdev;
    int i;

    pdev = adf_os_mem_alloc(osdev, sizeof(*pdev));

    if (!pdev) {
        goto fail1;
    }

    pdev->osdev = osdev;
    pdev->ctrl_pdev = ctrl_pdev;
    pdev->txrx_pdev = txrx_pdev;
    pdev->htc_pdev = htc_pdev;

    adf_os_mem_set(&pdev->stats, 0, sizeof(pdev->stats));
    pdev->htt_htc_pkt_freelist = NULL;
#ifdef ATH_11AC_TXCOMPACT
    pdev->htt_htc_pkt_misclist = NULL;
#endif

    /* for efficiency, store a local copy of the is_high_latency flag */
    pdev->cfg.is_high_latency = ol_cfg_is_high_latency(pdev->ctrl_pdev);
    pdev->cfg.default_tx_comp_req =
         !ol_cfg_tx_free_at_download(pdev->ctrl_pdev);

    pdev->cfg.is_full_reorder_offload =
         ol_cfg_is_full_reorder_offload(pdev->ctrl_pdev);
    adf_os_print("is_full_reorder_offloaded? %d\n",
                  (int)pdev->cfg.is_full_reorder_offload);
    pdev->targetdef = htc_get_targetdef(htc_pdev);
    /*
     * Connect to HTC service.
     * This has to be done before calling htt_rx_attach,
     * since htt_rx_attach involves sending a rx ring configure
     * message to the target.
     */
//AR6004 don't need HTT layer.
#ifndef AR6004_HW
    if (htt_htc_attach(pdev)) {
        goto fail2;
    }
#endif
    if (htt_tx_attach(pdev, desc_pool_size)) {
        goto fail2;
    }

    if (htt_rx_attach(pdev)) {
        goto fail3;
    }

    HTT_TX_MUTEX_INIT(&pdev->htt_tx_mutex);
    HTT_TX_NBUF_QUEUE_MUTEX_INIT(pdev);
    HTT_TX_MUTEX_INIT(&pdev->credit_mutex);

    /* pre-allocate some HTC_PACKET objects */
    for (i = 0; i < HTT_HTC_PKT_POOL_INIT_SIZE; i++) {
        struct htt_htc_pkt_union *pkt;
        pkt = adf_os_mem_alloc(pdev->osdev, sizeof(*pkt));
        if (! pkt) {
            break;
        }
        htt_htc_pkt_free(pdev, &pkt->u.pkt);
    }

    if (pdev->cfg.is_high_latency) {
        /*
         * HL - download the whole frame.
         * Specify a download length greater than the max MSDU size,
         * so the downloads will be limited by the actual frame sizes.
         */
        pdev->download_len = 5000;
        if (ol_cfg_tx_free_at_download(pdev->ctrl_pdev)) {
            pdev->tx_send_complete_part2 = ol_tx_download_done_hl_free;
        } else {
            pdev->tx_send_complete_part2 = ol_tx_download_done_hl_retain;
        }

        /*
         * For LL, the FW rx desc directly referenced at its location
         * inside the rx indication message.
         */
/*
 * CHECK THIS LATER: does the HL HTT version of htt_rx_mpdu_desc_list_next
 * (which is not currently implemented) present the adf_nbuf_data(rx_ind_msg)
 * as the abstract rx descriptor?
 * If not, the rx_fw_desc_offset initialization here will have to be
 * adjusted accordingly.
 * NOTE: for HL, because fw rx desc is in ind msg, not in rx desc, so the
 * offset should be negtive value
 */
        pdev->rx_fw_desc_offset =
            HTT_ENDIAN_BYTE_IDX_SWAP(
                    HTT_RX_IND_FW_RX_DESC_BYTE_OFFSET
                    - HTT_RX_IND_HL_BYTES);

        htt_h2t_rx_ring_cfg_msg = htt_h2t_rx_ring_cfg_msg_hl;

        /* initialize the txrx credit count */
        ol_tx_target_credit_update(
            pdev->txrx_pdev, ol_cfg_target_tx_credit(ctrl_pdev));
    } else {
        /*
         * LL - download just the initial portion of the frame.
         * Download enough to cover the encapsulation headers checked
         * by the target's tx classification descriptor engine.
         */
        enum wlan_frm_fmt frm_type;

        /* account for the 802.3 or 802.11 header */
        frm_type = ol_cfg_frame_type(pdev->ctrl_pdev);
        if (frm_type == wlan_frm_fmt_native_wifi) {
            pdev->download_len = HTT_TX_HDR_SIZE_NATIVE_WIFI;
        } else if (frm_type == wlan_frm_fmt_802_3) {
            pdev->download_len = HTT_TX_HDR_SIZE_ETHERNET;
        } else {
            adf_os_print("Unexpected frame type spec: %d\n", frm_type);
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
         * to apply for all requeued tx frames.  Thus, pdev->download_len
         * has to be the largest download length of any tx frame that will
         * be downloaded.
         * This maximum download length is for management tx frames,
         * which have an 802.11 header.
         */
        #ifdef ATH_11AC_TXCOMPACT
        pdev->download_len =
            sizeof(struct htt_host_tx_desc_t) +
            HTT_TX_HDR_SIZE_OUTER_HDR_MAX + /* worst case */
            HTT_TX_HDR_SIZE_802_1Q +
            HTT_TX_HDR_SIZE_LLC_SNAP +
            ol_cfg_tx_download_size(pdev->ctrl_pdev);
        #endif
        pdev->tx_send_complete_part2 = ol_tx_download_done_ll;

        /*
         * For LL, the FW rx desc is alongside the HW rx desc fields in
         * the htt_host_rx_desc_base struct/.
         */
        pdev->rx_fw_desc_offset = RX_STD_DESC_FW_MSDU_OFFSET;

        htt_h2t_rx_ring_cfg_msg = htt_h2t_rx_ring_cfg_msg_ll;
    }

    return pdev;

fail3:
    htt_tx_detach(pdev);

fail2:
    adf_os_mem_free(pdev);

fail1:
    return NULL;
}

A_STATUS
htt_attach_target(htt_pdev_handle pdev)
{
    A_STATUS status;
    status = htt_h2t_ver_req_msg(pdev);
    if (status != A_OK) {
        return status;
    }
    /*
     * If applicable, send the rx ring config message to the target.
     * The host could wait for the HTT version number confirmation message
     * from the target before sending any further HTT messages, but it's
     * reasonable to assume that the host and target HTT version numbers
     * match, and proceed immediately with the remaining configuration
     * handshaking.
     */

    status = htt_h2t_rx_ring_cfg_msg(pdev);
    status = HTT_IPA_CONFIG(pdev, status);

    return status;
}

void
htt_detach(htt_pdev_handle pdev)
{
    htt_rx_detach(pdev);
    htt_tx_detach(pdev);
    htt_htc_pkt_pool_free(pdev);
#ifdef ATH_11AC_TXCOMPACT
    htt_htc_misc_pkt_pool_free(pdev);
#endif
    HTT_TX_MUTEX_DESTROY(&pdev->htt_tx_mutex);
    HTT_TX_NBUF_QUEUE_MUTEX_DESTROY(pdev);
    HTT_TX_MUTEX_DESTROY(&pdev->credit_mutex);
#ifdef DEBUG_RX_RING_BUFFER
    if (pdev->rx_buff_list)
        adf_os_mem_free(pdev->rx_buff_list);
#endif
    adf_os_mem_free(pdev);
}

void
htt_detach_target(htt_pdev_handle pdev)
{
}

int
htt_htc_attach(struct htt_pdev_t *pdev)
{
    HTC_SERVICE_CONNECT_REQ connect;
    HTC_SERVICE_CONNECT_RESP response;
    A_STATUS status;

    adf_os_mem_set(&connect, 0, sizeof(connect));
    adf_os_mem_set(&response, 0, sizeof(response));

    connect.pMetaData = NULL;
    connect.MetaDataLength = 0;
    connect.EpCallbacks.pContext = pdev;
    connect.EpCallbacks.EpTxComplete = htt_h2t_send_complete;
    connect.EpCallbacks.EpTxCompleteMultiple = NULL;
    connect.EpCallbacks.EpRecv = htt_t2h_msg_handler;
    connect.EpCallbacks.EpResumeTxQueue = htt_tx_resume_handler;

    /* rx buffers currently are provided by HIF, not by EpRecvRefill */
    connect.EpCallbacks.EpRecvRefill = NULL;
    connect.EpCallbacks.RecvRefillWaterMark = 1; /* N/A, fill is done by HIF */

    connect.EpCallbacks.EpSendFull = htt_h2t_full;
    /*
     * Specify how deep to let a queue get before HTCSendPkt will
     * call the EpSendFull function due to excessive send queue depth.
     */
    connect.MaxSendQueueDepth = HTT_MAX_SEND_QUEUE_DEPTH;

    /* disable flow control for HTT data message service */
#ifdef HIF_SDIO
    /*
     * HTC Credit mechanism is disabled based on
     * default_tx_comp_req as throughput will be lower
     * if we disable htc credit mechanism with default_tx_comp_req
     * set since txrx download packet will be limited by ota
     * completion.
     * TODO:Conditional disabling will be removed once firmware
     * with reduced tx completion is pushed into release builds.
     */
    if (!pdev->cfg.default_tx_comp_req) {
       connect.ConnectionFlags |= HTC_CONNECT_FLAGS_DISABLE_CREDIT_FLOW_CTRL;
    }
#else
    connect.ConnectionFlags |= HTC_CONNECT_FLAGS_DISABLE_CREDIT_FLOW_CTRL;
#endif

    /* connect to control service */
    connect.ServiceID = HTT_DATA_MSG_SVC;

    status = HTCConnectService(pdev->htc_pdev, &connect, &response);

    if (status != A_OK) {
        return 1; /* failure */
    }
    pdev->htc_endpoint = response.Endpoint;
#if defined(HIF_PCI)
    hif_pci_save_htc_htt_config_endpoint(pdev->htc_endpoint);
#endif

#ifdef QCA_TX_HTT2_SUPPORT
    /* Start TX HTT2 service if the target support it. */
    if (pdev->cfg.is_high_latency) {
        adf_os_mem_set(&connect, 0, sizeof(connect));
        adf_os_mem_set(&response, 0, sizeof(response));

        /* The same as HTT service but no RX. */
        connect.EpCallbacks.pContext = pdev;
        connect.EpCallbacks.EpTxComplete = htt_h2t_send_complete;
        connect.EpCallbacks.EpSendFull = htt_h2t_full;
        connect.MaxSendQueueDepth = HTT_MAX_SEND_QUEUE_DEPTH;

        /* Should NOT support credit flow control. */
        connect.ConnectionFlags |= HTC_CONNECT_FLAGS_DISABLE_CREDIT_FLOW_CTRL;
        /* Enable HTC schedule mechanism for TX HTT2 service. */
        connect.ConnectionFlags |= HTC_CONNECT_FLAGS_ENABLE_HTC_SCHEDULE;

        connect.ServiceID = HTT_DATA2_MSG_SVC;

        status = HTCConnectService(pdev->htc_pdev, &connect, &response);
        if (status != A_OK) {
            pdev->htc_tx_htt2_endpoint = ENDPOINT_UNUSED;
            pdev->htc_tx_htt2_max_size = 0;
        } else {
            pdev->htc_tx_htt2_endpoint = response.Endpoint;
            pdev->htc_tx_htt2_max_size = HTC_TX_HTT2_MAX_SIZE;
        }

        adf_os_print("TX HTT %s, ep %d size %d\n",
                     (status == A_OK ? "ON" : "OFF"),
                     pdev->htc_tx_htt2_endpoint,
                     pdev->htc_tx_htt2_max_size);
    }
#endif /* QCA_TX_HTT2_SUPPORT */

    return 0; /* success */
}

#if HTT_DEBUG_LEVEL > 5
void
htt_display(htt_pdev_handle pdev, int indent)
{
    adf_os_print("%*s%s:\n", indent, " ", "HTT");
    adf_os_print(
        "%*stx desc pool: %d elems of %d bytes, "
        "%d currently allocated\n", indent+4, " ",
        pdev->tx_descs.pool_elems,
        pdev->tx_descs.size,
        pdev->tx_descs.alloc_cnt);
    adf_os_print(
        "%*srx ring: space for %d elems, filled with %d buffers\n",
        indent+4, " ",
        pdev->rx_ring.size,
        pdev->rx_ring.fill_level);
    adf_os_print("%*sat %p (%#x paddr)\n", indent+8, " ",
        pdev->rx_ring.buf.paddrs_ring,
        pdev->rx_ring.base_paddr);
    adf_os_print("%*snetbuf ring @ %p\n", indent+8, " ",
        pdev->rx_ring.buf.netbufs_ring);
    adf_os_print("%*sFW_IDX shadow register: vaddr = %p, paddr = %#x\n",
        indent+8, " ",
        pdev->rx_ring.alloc_idx.vaddr,
        pdev->rx_ring.alloc_idx.paddr);
    adf_os_print(
        "%*sSW enqueue index = %d, SW dequeue index: desc = %d, buf = %d\n",
        indent+8, " ",
        *pdev->rx_ring.alloc_idx.vaddr,
        pdev->rx_ring.sw_rd_idx.msdu_desc,
        pdev->rx_ring.sw_rd_idx.msdu_payld);
}
#endif

/* Disable ASPM : Disable PCIe low power */
void htt_htc_disable_aspm(void)
{
    htc_disable_aspm();
}

#ifdef IPA_UC_OFFLOAD
/*
 * Attach resource for micro controller data path
 */
int
htt_ipa_uc_attach(struct htt_pdev_t *pdev)
{
    int error;

    /* TX resource attach */
    error = htt_tx_ipa_uc_attach(pdev,
       ol_cfg_ipa_uc_tx_buf_size(pdev->ctrl_pdev),
       ol_cfg_ipa_uc_tx_max_buf_cnt(pdev->ctrl_pdev),
       ol_cfg_ipa_uc_tx_partition_base(pdev->ctrl_pdev));
    if (error) {
        adf_os_print("HTT IPA UC TX attach fail code %d\n", error);
        HTT_ASSERT0(0);
        return error;
    }

    /* RX resource attach */
    error = htt_rx_ipa_uc_attach(pdev,
       ol_cfg_ipa_uc_rx_ind_ring_size(pdev->ctrl_pdev));
    if (error) {
        adf_os_print("HTT IPA UC RX attach fail code %d\n", error);
        htt_tx_ipa_uc_detach(pdev);
        HTT_ASSERT0(0);
        return error;
    }

    return 0; /* success */
}

void
htt_ipa_uc_detach(struct htt_pdev_t *pdev)
{
    /* TX IPA micro controller detach */
    htt_tx_ipa_uc_detach(pdev);

    /* RX IPA micro controller detach */
    htt_rx_ipa_uc_detach(pdev);
}

/*
 * Distribute micro controller resource to control module
 */
int
htt_ipa_uc_get_resource(htt_pdev_handle pdev,
           u_int32_t *ce_sr_base_paddr,
           u_int32_t *ce_sr_ring_size,
           u_int32_t *ce_reg_paddr,
           u_int32_t *tx_comp_ring_base_paddr,
           u_int32_t *tx_comp_ring_size,
           u_int32_t *tx_num_alloc_buffer,
           u_int32_t *rx_rdy_ring_base_paddr,
           u_int32_t *rx_rdy_ring_size,
           u_int32_t *rx_proc_done_idx_paddr)
{
    /* Release allocated resource to client */
    *tx_comp_ring_base_paddr =
        (u_int32_t)pdev->ipa_uc_tx_rsc.tx_comp_base.paddr;
    *tx_comp_ring_size =
        (u_int32_t)ol_cfg_ipa_uc_tx_max_buf_cnt(pdev->ctrl_pdev);
    *tx_num_alloc_buffer =
        (u_int32_t)pdev->ipa_uc_tx_rsc.alloc_tx_buf_cnt;
    *rx_rdy_ring_base_paddr =
        (u_int32_t)pdev->ipa_uc_rx_rsc.rx_ind_ring_base.paddr;
    *rx_rdy_ring_size =
        (u_int32_t)pdev->ipa_uc_rx_rsc.rx_ind_ring_size;
    *rx_proc_done_idx_paddr =
        (u_int32_t)pdev->ipa_uc_rx_rsc.rx_ipa_prc_done_idx.paddr;

    /* Get copy engine, bus resource */
    HTCIpaGetCEResource(pdev->htc_pdev,
        ce_sr_base_paddr, ce_sr_ring_size, ce_reg_paddr);


    return 0;
}

/*
 * Distribute micro controller doorbell register to firmware
 */
int
htt_ipa_uc_set_doorbell_paddr(htt_pdev_handle pdev,
           u_int32_t ipa_uc_tx_doorbell_paddr,
           u_int32_t ipa_uc_rx_doorbell_paddr)
{
   pdev->ipa_uc_tx_rsc.tx_comp_idx_paddr = ipa_uc_tx_doorbell_paddr;
   pdev->ipa_uc_rx_rsc.rx_rdy_idx_paddr = ipa_uc_rx_doorbell_paddr;
   return 0;
}
#endif /* IPA_UC_OFFLOAD */

#if defined(DEBUG_HL_LOGGING) && defined(CONFIG_HL_SUPPORT)

void htt_dump_bundle_stats(htt_pdev_handle pdev)
{
    HTCDumpBundleStats(pdev->htc_pdev);
}

void htt_clear_bundle_stats(htt_pdev_handle pdev)
{
    HTCClearBundleStats(pdev->htc_pdev);
}
#endif

