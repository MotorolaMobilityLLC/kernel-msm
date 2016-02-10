/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
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

#include "vos_sched.h"
#include "wlan_qct_tl.h"
#include "wdi_in.h"
#include "ol_txrx_peer_find.h"
#include "tl_shim.h"
#include "wma.h"
#include "wmi_unified_api.h"
#include "vos_packet.h"
#include "vos_memory.h"
#include "adf_os_types.h"
#include "adf_nbuf.h"
#include "adf_os_mem.h"
#include "adf_os_lock.h"
#include "adf_nbuf.h"
#include "wma_api.h"
#include "vos_utils.h"
#include "wdi_out.h"

#define TLSHIM_PEER_AUTHORIZE_WAIT 50

#define ENTER() VOS_TRACE(VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO, "Enter:%s", __func__)

#define TLSHIM_LOGD(args...) \
	VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_INFO, ## args)
#define TLSHIM_LOGW(args...) \
	VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_WARN, ## args)
#define TLSHIM_LOGE(args...) \
	VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_ERROR, ## args)
#define TLSHIM_LOGP(args...) \
	VOS_TRACE( VOS_MODULE_ID_TL, VOS_TRACE_LEVEL_FATAL, ## args)

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)

/************************/
/*   Internal defines   */
/************************/
#define SIZEOF_80211_HDR (sizeof(struct ieee80211_frame))
#define LLC_SNAP_SIZE 8

/* Cisco Aironet SNAP hdr */
static u_int8_t AIRONET_SNAP_HEADER[] =  {0xAA, 0xAA, 0x03, 0x00, 0x40,
	0x96, 0x00, 0x00 };

/*
 * @brief:  Creates vos_pkt_t for IAPP packet and routes them to PE/LIM.
 * @detail: This function will be executed by new deferred task. It calls
 *          in the function to process and route IAPP frame. After IAPP
 *          has been processed, it will free the passed adb_nbuf_t pointer.
 *          This function will run in non interrupt context
 * @param:  ptr_work - pointer to work struct containing passed parameters
 *          from calling function.
 */
void
tlshim_mgmt_over_data_rx_handler(struct work_struct *ptr_work)
{
    struct deferred_iapp_work *ptr_my_work
        = container_of(ptr_work, struct deferred_iapp_work, deferred_work);
    pVosContextType pVosGCtx = ptr_my_work->pVosGCtx;
    u_int8_t *data = adf_nbuf_data(ptr_my_work->nbuf);
    u_int32_t data_len = adf_nbuf_len(ptr_my_work->nbuf);
    struct ol_txrx_vdev_t *vdev = ptr_my_work->vdev;

    /*
     * data : is a either data starting from snap hdr or 802.11 frame
     * data_len : length of above data
     */

    struct txrx_tl_shim_ctx *tl_shim = vos_get_context(VOS_MODULE_ID_TL,
        pVosGCtx);
    vos_pkt_t *rx_pkt;
    adf_nbuf_t wbuf;
    struct ieee80211_frame *wh;

    if (!tl_shim->mgmt_rx) {
        TLSHIM_LOGE("Not registered for Mgmt rx, dropping the frame");
        /* this buffer is used now, free it */
        adf_nbuf_free(ptr_my_work->nbuf);
        /* set inUse to false, so that next IAPP frame can be processed */
        ptr_my_work->inUse = false;
        return;
    }

    /*
     * allocate rx_packet: this will be used for encapsulating a
     * sk_buff which then is passed to peHandleMgmtFrame(ptr fn)
     * along with vos_ctx.
     */
    rx_pkt = vos_mem_malloc(sizeof(*rx_pkt));
    if (!rx_pkt) {
        TLSHIM_LOGE("Failed to allocate rx packet");
        /* this buffer is used now, free it */
        adf_nbuf_free(ptr_my_work->nbuf);
        /* set inUse to false, so that next IAPP frame can be processed */
        ptr_my_work->inUse = false;
        return;
    }

    vos_mem_zero(rx_pkt, sizeof(*rx_pkt));

    /*
     * TODO: also check if following is used for IAPP
     * if yes, find out how to populate this
     * rx_pkt->pkt_meta.channel = 0;
     */
    rx_pkt->pkt_meta.snr = rx_pkt->pkt_meta.rssi = 0;

    rx_pkt->pkt_meta.timestamp = (u_int32_t) jiffies;
    rx_pkt->pkt_meta.mpdu_hdr_len = SIZEOF_80211_HDR;

    /*
     * mpdu len and data len will be different for native and non native
     * format
     */
    if (vdev->pdev->frame_format == wlan_frm_fmt_native_wifi) {
        rx_pkt->pkt_meta.mpdu_len = data_len;
        rx_pkt->pkt_meta.mpdu_data_len = data_len -
            rx_pkt->pkt_meta.mpdu_hdr_len;
    }
    else {
        rx_pkt->pkt_meta.mpdu_len = data_len +
            rx_pkt->pkt_meta.mpdu_hdr_len - ETHERNET_HDR_LEN;
        rx_pkt->pkt_meta.mpdu_data_len = data_len - ETHERNET_HDR_LEN;
    }

    /* allocate a sk_buff with enough memory for 802.11 IAPP frame */
    wbuf = adf_nbuf_alloc(NULL, roundup(rx_pkt->pkt_meta.mpdu_len, 4),
        0, 4, FALSE);
    if (!wbuf) {
        TLSHIM_LOGE("Failed to allocate wbuf for mgmt rx");
        vos_mem_free(rx_pkt);
        /* this buffer is used now, free it */
        adf_nbuf_free(ptr_my_work->nbuf);
        /* set inUse to false, so that next IAPP frame can be processed */
        ptr_my_work->inUse = false;
        return;
    }

    adf_nbuf_put_tail(wbuf, data_len);
    adf_nbuf_set_protocol(wbuf, ETH_P_SNAP);

    /* wh will contain 802.11 frame, it will be encpsulated inside sk_buff */
    wh = (struct ieee80211_frame *) adf_nbuf_data(wbuf);

    /* set mpdu hdr pointre to data of sk_buff */
    rx_pkt->pkt_meta.mpdu_hdr_ptr = adf_nbuf_data(wbuf);
    /* set mpdu data pointer to appropriate offset from hdr */
    rx_pkt->pkt_meta.mpdu_data_ptr = rx_pkt->pkt_meta.mpdu_hdr_ptr +
    rx_pkt->pkt_meta.mpdu_hdr_len;
    /* encapsulate newly allocated sk_buff in rx_pkt */
    rx_pkt->pkt_buf = wbuf;

    if (vdev->pdev->frame_format == wlan_frm_fmt_native_wifi) {
        /* if native wifi: copy full frame */
        adf_os_mem_copy(wh, data, data_len);
    }
    else {
        /*
        * if not native wifi populate: copy just part after 802.11 hdr
        * i.e. part starting from snap header
        */
        tpEseIappHdr iapp_hdr_ptr = (tpEseIappHdr)&data[ETHERNET_HDR_LEN];
        u_int8_t *snap_hdr_ptr = &(((u_int8_t*)wh)[SIZEOF_80211_HDR]);
        tpSirMacFrameCtl ptr_80211_FC = (tpSirMacFrameCtl)&wh->i_fc;
        ptr_80211_FC->protVer = SIR_MAC_PROTOCOL_VERSION;
        ptr_80211_FC->type = SIR_MAC_DATA_FRAME;
        ptr_80211_FC->subType = SIR_MAC_DATA_QOS_DATA;
        ptr_80211_FC->toDS = 0;
        ptr_80211_FC->fromDS = 1;
        ptr_80211_FC->moreFrag = 0;
        ptr_80211_FC->retry = 0;
        ptr_80211_FC->powerMgmt = 0;
        ptr_80211_FC->moreData = 0;
        ptr_80211_FC->wep = 0;
        ptr_80211_FC->order = 0;

        wh->i_dur[0] = 0;
        wh->i_dur[1] = 0;

        adf_os_mem_copy(&wh->i_addr1, &iapp_hdr_ptr->DestMac[0],
            ETHERNET_ADDR_LEN);
        adf_os_mem_copy(&wh->i_addr2, &iapp_hdr_ptr->SrcMac[0],
            ETHERNET_ADDR_LEN);
        adf_os_mem_copy(&wh->i_addr3, &vdev->last_real_peer->mac_addr.raw[0],
            ETHERNET_ADDR_LEN);

        wh->i_seq[0] = 0;
        wh->i_seq[1] = 0;

        adf_os_mem_copy( snap_hdr_ptr, &data[ETHERNET_HDR_LEN],
        data_len - ETHERNET_HDR_LEN);
    }

    tl_shim->mgmt_rx(pVosGCtx, rx_pkt);
    /* this buffer is used now, free it */
    adf_nbuf_free(ptr_my_work->nbuf);
    /* set inUse to false, so that next IAPP frame can be processed */
    ptr_my_work->inUse = false;
}

/*
 * @brief: This function creates the deferred task and schedules it. this is
 *         still in interrrupt context. The deferred task is created to run
 *         in non interrut context as a memory allocation of vos_pkt_t is
 *         needed and memory allocation should not be done in interrupt
 *         context.
 * @param - pVosGCtx - vos context
 * @param - data - data containing ieee80211 IAPP frame
 * @param - data_len - data len containing ieee80211 IAPP frame
 * @param - vdev - virtual device
 */
void
tlshim_mgmt_over_data_rx_handler_non_interrupt_ctx(pVosContextType pVosGCtx,
	adf_nbuf_t nbuf, struct ol_txrx_vdev_t *vdev)
{
    struct txrx_tl_shim_ctx *tl_shim = vos_get_context(VOS_MODULE_ID_TL,
        pVosGCtx);

    /*
     * if there is already a deferred IAPP processing, do not start
     * another. Instead drop it as IAPP frames are not critical and
     * can be dropped without any disruptive effects.
     */
    if(tl_shim->iapp_work.inUse == false) {
        tl_shim->iapp_work.pVosGCtx = pVosGCtx;
        tl_shim->iapp_work.nbuf = nbuf;
        tl_shim->iapp_work.vdev = vdev;
        tl_shim->iapp_work.inUse = true;
        schedule_work(&(tl_shim->iapp_work.deferred_work));
        return;
    }

    /* Previous IAPP frame is not yet processed, drop this frame */
    TLSHIM_LOGE("Dropping IAPP frame because previous is yet unprocessed");
    /*
     * TODO: If needed this can changed to have queue rather
     * than drop frame
     */
    adf_nbuf_free(nbuf);
    return;
}

/*
 * @brief: This checks if frame is IAPP and if yes routes them to PE/LIM
 * @param - pVosGCtx - vos context
 * @param - msdu - frame
 * @param - sta_id - station ID
 */
bool
tlshim_check_n_process_iapp_frame (pVosContextType pVosGCtx,
	adf_nbuf_t *msdu, u_int16_t sta_id)
{
    u_int8_t *data;
    u_int8_t offset_snap_header;
    struct ol_txrx_pdev_t *pdev = pVosGCtx->pdev_txrx_ctx;
    struct ol_txrx_peer_t *peer =
    ol_txrx_peer_find_by_local_id(pVosGCtx->pdev_txrx_ctx, sta_id);
    struct ol_txrx_vdev_t *vdev = peer->vdev;
    adf_nbuf_t new_head = NULL, buf, new_list = NULL, next_buf;

    /* frame format is natve wifi */
    if(pdev->frame_format == wlan_frm_fmt_native_wifi)
        offset_snap_header = SIZEOF_80211_HDR;
    else
        offset_snap_header = ETHERNET_HDR_LEN;

    buf = *msdu;
    while (buf) {
	data = adf_nbuf_data(buf);
	next_buf = adf_nbuf_queue_next(buf);
	if (vos_mem_compare( &data[offset_snap_header],
           &AIRONET_SNAP_HEADER[0], LLC_SNAP_SIZE) == VOS_TRUE) {
		/* process IAPP frames */
		tlshim_mgmt_over_data_rx_handler_non_interrupt_ctx(pVosGCtx,
								   buf, vdev);
	} else { /* Add the packet onto a new list */
		if (new_list == NULL)
			new_head = buf;
		else
			adf_nbuf_set_next(new_list, buf);
		new_list = buf;
		adf_nbuf_set_next(buf, NULL);
	}
	buf = next_buf;
    }

    if (!new_list)
	return true;

    *msdu = new_head;
    /* if returned false the packet will be handled by the upper layer */
    return false;
}
#endif /* defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD) */


/*AR9888/AR6320  noise floor approx value*/
#define TLSHIM_TGT_NOISE_FLOOR_DBM (-96)

#ifdef WLAN_FEATURE_11W

/*
 * @brief - This routine will find the iface based on vdev id of provided bssid
 * @param - vos_ctx - vos context
 * @param - mac_addr - bss MAC address of received frame
 * @param - vdev_id - virtual device id
 */
static struct wma_txrx_node*
tlshim_mgmt_find_iface(void *vos_ctx, u_int8_t *mac_addr, u_int32_t *vdev_id)
{
	struct ol_txrx_vdev_t *vdev = NULL;
	struct wma_txrx_node *iface = NULL;
	tp_wma_handle wma = NULL;

	wma = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);
	if (wma) {
		/*
		 * Based on received frame's bssid, we will try to
		 * retrieve the vdev id. If we find the vdev then we will
		 * override with found vdev_id else we will use supplied
		 * vdev_id
		 */
		vdev = tl_shim_get_vdev_by_addr(vos_ctx, mac_addr);
		if (vdev) {
			*vdev_id = vdev->vdev_id;
		}
		iface = &wma->interfaces[*vdev_id];
	}
	return iface;
}

/*
 * @brief - This routine will extract 6 byte PN from the CCMP header
 * @param - ccmp_ptr - pointer to ccmp header
 */
static u_int64_t
tl_shim_extract_ccmp_pn(u_int8_t *ccmp_ptr)
{
	u_int8_t rsvd, key, pn[6];
	u_int64_t new_pn;

	/*
	 *   +-----+-----+------+----------+-----+-----+-----+-----+
	 *   | PN0 | PN1 | rsvd | rsvd/key | PN2 | PN3 | PN4 | PN5 |
	 *   +-----+-----+------+----------+-----+-----+-----+-----+
	 *                   CCMP Header Format
	 */

	/* Extract individual bytes */
	pn[0]  = (u_int8_t)*ccmp_ptr;
	pn[1]  = (u_int8_t)*(ccmp_ptr+1);
	rsvd   = (u_int8_t)*(ccmp_ptr+2);
	key    = (u_int8_t)*(ccmp_ptr+3);
	pn[2]  = (u_int8_t)*(ccmp_ptr+4);
	pn[3]  = (u_int8_t)*(ccmp_ptr+5);
	pn[4]  = (u_int8_t)*(ccmp_ptr+6);
	pn[5]  = (u_int8_t)*(ccmp_ptr+7);

	/* Form 6 byte PN with 6 individual bytes of PN */
	new_pn = ((uint64_t)pn[5] << 40) |
			((uint64_t)pn[4] << 32) |
			((uint64_t)pn[3] << 24) |
			((uint64_t)pn[2] << 16) |
			((uint64_t)pn[1] << 8)  |
			((uint64_t)pn[0] << 0);

	TLSHIM_LOGD("PN of received packet is %llu", new_pn);
	return new_pn;
}

/*
 * @brief - This routine is used to detect replay attacking using PN in CCMP
 * @param - vos_ctx - vos context
 * @param - wh - frame header
 * @param - ccmp_ptr - pointer to ccmp header
 */
static bool
is_ccmp_pn_replay_attack(void *vos_ctx, struct ieee80211_frame *wh,
		u_int8_t *ccmp_ptr)
{
	ol_txrx_pdev_handle pdev;
	ol_txrx_vdev_handle vdev;
	ol_txrx_peer_handle peer;
	u_int8_t peer_id;
	u_int8_t *last_pn_valid;
	u_int64_t *last_pn, new_pn;
	u_int32_t *rmf_pn_replays;

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, vos_ctx);
	if (!pdev) {
		TLSHIM_LOGE("%s: Failed to find pdev", __func__);
		TLSHIM_LOGE("%s: Not able to validate PN", __func__);
		return true;
	}

	vdev = tl_shim_get_vdev_by_addr(vos_ctx, wh->i_addr2);
	if (!vdev) {
		TLSHIM_LOGE("%s: Failed to find vdev", __func__);
		TLSHIM_LOGE("%s: Not able to validate PN", __func__);
		return true;
	}

	/* Retrieve the peer based on vdev and addr */
	peer = ol_txrx_find_peer_by_addr_and_vdev(pdev, vdev, wh->i_addr2,
							&peer_id);

	if (NULL == peer) {
		TLSHIM_LOGE(
		"%s: Failed to find peer, Not able to validate PN", __func__);
		return true;
	}

	new_pn = tl_shim_extract_ccmp_pn(ccmp_ptr);
	last_pn_valid = &peer->last_rmf_pn_valid;
	last_pn = &peer->last_rmf_pn;
	rmf_pn_replays = &peer->rmf_pn_replays;

	if (*last_pn_valid) {
		if (new_pn > *last_pn) {
			*last_pn = new_pn;
			TLSHIM_LOGD("%s: PN validation successful", __func__);
		} else {
			TLSHIM_LOGE("%s: PN Replay attack detected", __func__);
			/* per 11W amendment, keeping track of replay attacks */
			*rmf_pn_replays += 1;
			return true;
		}
	} else {
		*last_pn_valid = 1;
		*last_pn = new_pn;
	}

	return false;
}
#endif

static int tlshim_mgmt_rx_process(void *context, u_int8_t *data,
				       u_int32_t data_len, bool saved_beacon, u_int32_t vdev_id)
{
	void *vos_ctx = vos_get_global_context(VOS_MODULE_ID_TL, NULL);
	struct txrx_tl_shim_ctx *tl_shim = vos_get_context(VOS_MODULE_ID_TL,
							   vos_ctx);
#ifdef FEATURE_WLAN_D0WOW
	tp_wma_handle wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);
#endif
	WMI_MGMT_RX_EVENTID_param_tlvs *param_tlvs = NULL;
	wmi_mgmt_rx_hdr *hdr = NULL;
#ifdef WLAN_FEATURE_11W
	struct wma_txrx_node *iface = NULL;
	u_int8_t *efrm, *orig_hdr;
	u_int16_t key_id;
	u_int8_t *ccmp;
#endif /* WLAN_FEATURE_11W */

	vos_pkt_t *rx_pkt;
	adf_nbuf_t wbuf;
	struct ieee80211_frame *wh;
	u_int8_t mgt_type, mgt_subtype;

	if (!tl_shim) {
		TLSHIM_LOGE("%s: Failed to get TLSHIM context", __func__);
		return 0;
	}

#ifdef FEATURE_WLAN_D0WOW
	if (!wma_handle) {
		TLSHIM_LOGE("%s: Failed to get WMA context!", __func__);
		return 0;
	}
#endif

	adf_os_spin_lock_bh(&tl_shim->mgmt_lock);
	param_tlvs = (WMI_MGMT_RX_EVENTID_param_tlvs *) data;
	if (!param_tlvs) {
		adf_os_spin_unlock_bh(&tl_shim->mgmt_lock);
		TLSHIM_LOGE("Get NULL point message from FW");
		return 0;
	}

	hdr = param_tlvs->hdr;
	if (!hdr) {
		adf_os_spin_unlock_bh(&tl_shim->mgmt_lock);
		TLSHIM_LOGE("Rx event is NULL");
		return 0;
	}

	if (hdr->buf_len < sizeof(struct ieee80211_frame)) {
		adf_os_spin_unlock_bh(&tl_shim->mgmt_lock);
		TLSHIM_LOGE("Invalid rx mgmt packet");
		return 0;
	}

	rx_pkt = vos_mem_malloc(sizeof(*rx_pkt));
	if (!rx_pkt) {
		adf_os_spin_unlock_bh(&tl_shim->mgmt_lock);
		TLSHIM_LOGE("Failed to allocate rx packet");
		return 0;
	}

	vos_mem_zero(rx_pkt, sizeof(*rx_pkt));

	/*
	 * Fill in meta information needed by pe/lim
	 * TODO: Try to maintain rx metainfo as part of skb->data.
	 */
	rx_pkt->pkt_meta.channel = hdr->channel;
        rx_pkt->pkt_meta.scan_src = hdr->flags;

	/* Get the rssi value from the current snr value
	 * using standard noise floor of -96.
	 */
	rx_pkt->pkt_meta.rssi = hdr->snr + TLSHIM_TGT_NOISE_FLOOR_DBM;
	rx_pkt->pkt_meta.snr = hdr->snr;

	/* If absolute rssi is available from firmware, use it */
	if (hdr->rssi != 0)
		rx_pkt->pkt_meta.rssi_raw = hdr->rssi;
	else
		rx_pkt->pkt_meta.rssi_raw = rx_pkt->pkt_meta.rssi;

	/*
	 * FIXME: Assigning the local timestamp as hw timestamp is not
	 * available. Need to see if pe/lim really uses this data.
	 */
	rx_pkt->pkt_meta.timestamp = (u_int32_t) jiffies;
	rx_pkt->pkt_meta.mpdu_hdr_len = sizeof(struct ieee80211_frame);
	rx_pkt->pkt_meta.mpdu_len = hdr->buf_len;
	rx_pkt->pkt_meta.mpdu_data_len = hdr->buf_len -
					 rx_pkt->pkt_meta.mpdu_hdr_len;

    /*
     * saved_beacon means this beacon is a duplicate of one
     * sent earlier. roamCandidateInd flag is used to indicate to
     * PE that roam scan finished and a better candidate AP
     * was found.
     */
	rx_pkt->pkt_meta.roamCandidateInd = saved_beacon ? 1 : 0;
	rx_pkt->pkt_meta.sessionId = vdev_id;
	/* Why not just use rx_event->hdr.buf_len? */
	wbuf = adf_nbuf_alloc(NULL,
			      roundup(hdr->buf_len, 4),
			      0, 4, FALSE);
	if (!wbuf) {
		adf_os_spin_unlock_bh(&tl_shim->mgmt_lock);
		TLSHIM_LOGE("%s: Failed to allocate wbuf for mgmt rx len(%u)",
			__func__, hdr->buf_len);
		vos_mem_free(rx_pkt);
		return 0;
	}

	adf_nbuf_put_tail(wbuf, hdr->buf_len);
	adf_nbuf_set_protocol(wbuf, ETH_P_CONTROL);
	wh = (struct ieee80211_frame *) adf_nbuf_data(wbuf);

	rx_pkt->pkt_meta.mpdu_hdr_ptr = adf_nbuf_data(wbuf);
	rx_pkt->pkt_meta.mpdu_data_ptr = rx_pkt->pkt_meta.mpdu_hdr_ptr +
					  rx_pkt->pkt_meta.mpdu_hdr_len;
	rx_pkt->pkt_meta.tsf_delta = hdr->tsf_delta;
	rx_pkt->pkt_buf = wbuf;

#ifdef BIG_ENDIAN_HOST
	{
		/*
		 * for big endian host, copy engine byte_swap is enabled
		 * But the rx mgmt frame buffer content is in network byte order
		 * Need to byte swap the mgmt frame buffer content - so when
		 * copy engine does byte_swap - host gets buffer content in the
		 * correct byte order.
		 */
		int i;
		u_int32_t *destp, *srcp;
		destp = (u_int32_t *) wh;
		srcp =  (u_int32_t *) param_tlvs->bufp;
		for (i = 0;
		     i < (roundup(hdr->buf_len, sizeof(u_int32_t)) / 4);
		     i++) {
			*destp = cpu_to_le32(*srcp);
			destp++; srcp++;
		}
	}
#else
	adf_os_mem_copy(wh, param_tlvs->bufp, hdr->buf_len);
#endif

	TLSHIM_LOGD(
		FL("BSSID: "MAC_ADDRESS_STR" snr = %d, rssi = %d, rssi_raw = %d tsf_delta: %u"),
			MAC_ADDR_ARRAY(wh->i_addr3), hdr->snr,
			rx_pkt->pkt_meta.rssi,
			rx_pkt->pkt_meta.rssi_raw, hdr->tsf_delta);

	if (!tl_shim->mgmt_rx) {
		adf_os_spin_unlock_bh(&tl_shim->mgmt_lock);
		TLSHIM_LOGE("Not registered for Mgmt rx, dropping the frame");
		vos_pkt_return_packet(rx_pkt);
		return 0;
	}

	/* If it is a beacon/probe response, save it for future use */
	mgt_type    = (wh)->i_fc[0] & IEEE80211_FC0_TYPE_MASK;
	mgt_subtype = (wh)->i_fc[0] & IEEE80211_FC0_SUBTYPE_MASK;

#ifdef FEATURE_WLAN_D0WOW
	if (wma_read_d0wow_flag(wma_handle)) {
		TLSHIM_LOGE("%s: Frame subtype is 0x%x", __func__, mgt_subtype);
		wma_set_d0wow_flag(wma_handle, FALSE);
	}
#endif

	if (!saved_beacon && mgt_type == IEEE80211_FC0_TYPE_MGT &&
		(mgt_subtype == IEEE80211_FC0_SUBTYPE_BEACON || mgt_subtype == IEEE80211_FC0_SUBTYPE_PROBE_RESP))
	{
	    /* remember this beacon to be used later for better_ap event */
	    WMI_MGMT_RX_EVENTID_param_tlvs *last_tlvs =
		(WMI_MGMT_RX_EVENTID_param_tlvs *) tl_shim->last_beacon_data;
	    if (tl_shim->last_beacon_data) {
		/* Free the previously allocated buffers */
		if (last_tlvs->hdr)
			vos_mem_free(last_tlvs->hdr);
		if (last_tlvs->bufp)
			vos_mem_free(last_tlvs->bufp);
                vos_mem_free(tl_shim->last_beacon_data);
                tl_shim->last_beacon_data = NULL;
		tl_shim->last_beacon_len = 0;
	    }
	    if((tl_shim->last_beacon_data = vos_mem_malloc(sizeof(WMI_MGMT_RX_EVENTID_param_tlvs)))) {
		u_int32_t buf_len = roundup(hdr->buf_len, sizeof(u_int32_t));

		vos_mem_copy(tl_shim->last_beacon_data, data, sizeof(WMI_MGMT_RX_EVENTID_param_tlvs));
		tl_shim->last_beacon_len = sizeof(WMI_MGMT_RX_EVENTID_param_tlvs);
		last_tlvs = (WMI_MGMT_RX_EVENTID_param_tlvs *) tl_shim->last_beacon_data;
		if ((last_tlvs->hdr = vos_mem_malloc(sizeof(wmi_mgmt_rx_hdr)))) {
		    vos_mem_copy(last_tlvs->hdr, hdr, sizeof(wmi_mgmt_rx_hdr));
		    if ((last_tlvs->bufp = vos_mem_malloc(buf_len))) {
			vos_mem_copy(last_tlvs->bufp, param_tlvs->bufp, buf_len);
		    } else {
			vos_mem_free(last_tlvs->hdr);
			vos_mem_free(tl_shim->last_beacon_data);
			tl_shim->last_beacon_data = NULL;
			tl_shim->last_beacon_len = 0;
		    }
		} else {
		    vos_mem_free(tl_shim->last_beacon_data);
		    tl_shim->last_beacon_data = NULL;
		    tl_shim->last_beacon_len = 0;
		}
	    }
	}
	adf_os_spin_unlock_bh(&tl_shim->mgmt_lock);

#ifdef WLAN_FEATURE_11W
	if (mgt_type == IEEE80211_FC0_TYPE_MGT &&
		(mgt_subtype == IEEE80211_FC0_SUBTYPE_DISASSOC ||
		 mgt_subtype == IEEE80211_FC0_SUBTYPE_DEAUTH ||
		 mgt_subtype == IEEE80211_FC0_SUBTYPE_ACTION))
	{
		iface = tlshim_mgmt_find_iface(vos_ctx, wh->i_addr3, &vdev_id);
		if (iface && iface->rmfEnabled)
		{
			if ((wh)->i_fc[1] & IEEE80211_FC1_WEP)
			{
				if (IEEE80211_IS_BROADCAST(wh->i_addr1) ||
					 IEEE80211_IS_MULTICAST(wh->i_addr1)) {
					TLSHIM_LOGE("Encrypted BC/MC frame"
					" dropping the frame");
					vos_pkt_return_packet(rx_pkt);
					return 0;
				}

				orig_hdr = (u_int8_t*) adf_nbuf_data(wbuf);
				/* Pointer to head of CCMP header */
				ccmp = orig_hdr + sizeof(*wh);
				if (is_ccmp_pn_replay_attack(vos_ctx, wh,
									ccmp)) {
					TLSHIM_LOGE("Dropping the frame");
					vos_pkt_return_packet(rx_pkt);
					return 0;
				}

				/* Strip privacy headers (and trailer)
				   for a received frame */
				vos_mem_move(orig_hdr + IEEE80211_CCMP_HEADERLEN,
						wh, sizeof(*wh));
				adf_nbuf_pull_head(wbuf, IEEE80211_CCMP_HEADERLEN);
				adf_nbuf_trim_tail(wbuf, IEEE80211_CCMP_MICLEN);

				rx_pkt->pkt_meta.mpdu_hdr_ptr = adf_nbuf_data(wbuf);
				rx_pkt->pkt_meta.mpdu_len = adf_nbuf_len(wbuf);
				rx_pkt->pkt_meta.mpdu_data_len =
					rx_pkt->pkt_meta.mpdu_len -
					rx_pkt->pkt_meta.mpdu_hdr_len;
				rx_pkt->pkt_meta.mpdu_data_ptr =
					rx_pkt->pkt_meta.mpdu_hdr_ptr +
					rx_pkt->pkt_meta.mpdu_hdr_len;
				rx_pkt->pkt_buf = wbuf;
			}
			else
			{
				if (IEEE80211_IS_BROADCAST(wh->i_addr1) ||
					 IEEE80211_IS_MULTICAST(wh->i_addr1))
				{
					efrm = adf_nbuf_data(wbuf) + adf_nbuf_len(wbuf);

					key_id = (u_int16_t)*(efrm - vos_get_mmie_size() + 2);
					if (!((key_id == WMA_IGTK_KEY_INDEX_4) ||
						(key_id == WMA_IGTK_KEY_INDEX_5))) {
						TLSHIM_LOGE("Invalid KeyID(%d)"
						" dropping the frame", key_id);
						vos_pkt_return_packet(rx_pkt);
						return 0;
					}

					if (vos_is_mmie_valid(iface->key.key,
					    iface->key.key_id[key_id - WMA_IGTK_KEY_INDEX_4].ipn,
						 (u_int8_t *)wh, efrm))
					{
						TLSHIM_LOGD("Protected BC/MC frame MMIE"
							" validation successful");

						/* Remove MMIE */
						adf_nbuf_trim_tail(wbuf,
							vos_get_mmie_size());
					}
					else
					{
						TLSHIM_LOGE("BC/MC MIC error or MMIE"
						" not present, dropping the frame");
						vos_pkt_return_packet(rx_pkt);
						return 0;
					}
				}
				else
				{
					TLSHIM_LOGD("Rx unprotected unicast mgmt frame");
					rx_pkt->pkt_meta.dpuFeedback =
						 DPU_FEEDBACK_UNPROTECTED_ERROR;
				}

			}
		}
	}
#endif /* WLAN_FEATURE_11W */
	return tl_shim->mgmt_rx(vos_ctx, rx_pkt);
}

static int tlshim_mgmt_rx_wmi_handler(void *context, u_int8_t *data,
				       u_int32_t data_len)
{
	void *vos_ctx = vos_get_global_context(VOS_MODULE_ID_TL, NULL);
	struct txrx_tl_shim_ctx *tl_shim = vos_get_context(VOS_MODULE_ID_TL,
							   vos_ctx);
	VOS_STATUS ret = VOS_STATUS_SUCCESS;

	if (vos_is_logp_in_progress(VOS_MODULE_ID_TL, NULL)) {
			TLSHIM_LOGE("%s: LOGP in progress\n", __func__);
			return (-1);
	}

	if (vos_is_load_unload_in_progress(VOS_MODULE_ID_TL, NULL)) {
			TLSHIM_LOGE("%s: load/unload in progress\n", __func__);
			return (-1);
	}

	if (!tl_shim) {
		TLSHIM_LOGE("%s: tl shim ctx is NULL\n", __func__);
		return (-1);
	}

	ret = tlshim_mgmt_rx_process(context, data, data_len, FALSE, 0);

	return ret;
}
/*
 * tlshim_mgmt_roam_event_ind() is called from WMA layer when
 * BETTER_AP_FOUND event is received from roam engine.
 */
int tlshim_mgmt_roam_event_ind(void *context, u_int32_t vdev_id)
{
	void *vos_ctx = vos_get_global_context(VOS_MODULE_ID_TL, NULL);
	struct txrx_tl_shim_ctx *tl_shim = vos_get_context(VOS_MODULE_ID_TL,
							   vos_ctx);
	VOS_STATUS ret = VOS_STATUS_SUCCESS;

	if (!tl_shim) {
		TLSHIM_LOGE("%s: Failed to get TLSHIM context", __func__);
		return ret;
	}

	if (tl_shim->last_beacon_data && tl_shim->last_beacon_len) {
		ret = tlshim_mgmt_rx_process(context, tl_shim->last_beacon_data,
					tl_shim->last_beacon_len, TRUE, vdev_id);
	}
	return ret;
}

static void tl_shim_flush_rx_frames(void *vos_ctx,
				    struct txrx_tl_shim_ctx *tl_shim,
				    u_int8_t sta_id, bool drop)
{
	struct tlshim_sta_info *sta_info = &tl_shim->sta_info[sta_id];
	struct tlshim_buf *cache_buf, *tmp;
	VOS_STATUS ret;
	WLANTL_STARxCBType data_rx = NULL;

	if (test_and_set_bit(TLSHIM_FLUSH_CACHE_IN_PROGRESS, &sta_info->flags))
		return;

	adf_os_spin_lock_bh(&sta_info->stainfo_lock);
	if (sta_info->registered)
		data_rx = sta_info->data_rx;
	else
		drop = true;
	adf_os_spin_unlock_bh(&sta_info->stainfo_lock);

	adf_os_spin_lock_bh(&tl_shim->bufq_lock);
	list_for_each_entry_safe(cache_buf, tmp,
				 &sta_info->cached_bufq, list) {
		list_del(&cache_buf->list);
		adf_os_spin_unlock_bh(&tl_shim->bufq_lock);
		if (drop)
			adf_nbuf_free(cache_buf->buf);
		else {
			/* Flush the cached frames to HDD */
			ret = data_rx(vos_ctx, cache_buf->buf, sta_id);
			if (ret != VOS_STATUS_SUCCESS)
				adf_nbuf_free(cache_buf->buf);
		}
		adf_os_mem_free(cache_buf);
		adf_os_spin_lock_bh(&tl_shim->bufq_lock);
	}
	adf_os_spin_unlock_bh(&tl_shim->bufq_lock);
	clear_bit(TLSHIM_FLUSH_CACHE_IN_PROGRESS, &sta_info->flags);
}

static void tlshim_data_rx_cb(struct txrx_tl_shim_ctx *tl_shim,
			      adf_nbuf_t buf_list, u_int16_t staid)
{
	void *vos_ctx = vos_get_global_context(VOS_MODULE_ID_TL, tl_shim);
	struct tlshim_sta_info *sta_info;
	adf_nbuf_t buf, next_buf;
	VOS_STATUS ret;
	WLANTL_STARxCBType data_rx = NULL;

	if (unlikely(!vos_ctx))
		goto free_buf;

	sta_info = &tl_shim->sta_info[staid];
	adf_os_spin_lock_bh(&sta_info->stainfo_lock);
	if (unlikely(!sta_info->registered)) {
		adf_os_spin_unlock_bh(&sta_info->stainfo_lock);
		goto free_buf;
	}
	data_rx = sta_info->data_rx;
	adf_os_spin_unlock_bh(&sta_info->stainfo_lock);

	adf_os_spin_lock_bh(&tl_shim->bufq_lock);
	if (!list_empty(&sta_info->cached_bufq)) {
		sta_info->suspend_flush = 1;
		adf_os_spin_unlock_bh(&tl_shim->bufq_lock);
		/* Flush the cached frames to HDD before passing new rx frame */
		tl_shim_flush_rx_frames(vos_ctx, tl_shim, staid, 0);
	} else
		adf_os_spin_unlock_bh(&tl_shim->bufq_lock);

	ret = data_rx(vos_ctx, buf_list, staid);
	if (ret != VOS_STATUS_SUCCESS) {
		TLSHIM_LOGE("Frame Rx to HDD failed");
		goto free_buf;
	}
	return;

free_buf:
	TLSHIM_LOGW("%s:Dropping frames", __func__);
	buf = buf_list;
	while (buf) {
		next_buf = adf_nbuf_queue_next(buf);
		adf_nbuf_free(buf);
		buf = next_buf;
	}
}

/*
 * Rx callback from txrx module for data reception.
 */
static void tlshim_data_rx_handler(void *context, u_int16_t staid,
				   adf_nbuf_t rx_buf_list)
{
	struct txrx_tl_shim_ctx *tl_shim;
	/* Firmware data path active response will use shim RX thread
	 * T2H MSG running on SIRQ context,
	 * IPA kernel module API should not be called on SIRQ CTXT */
#if (defined(IPA_OFFLOAD) && !defined(IPA_UC_OFFLOAD))|| \
    (defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD))
	void *vos_ctx = vos_get_global_context(VOS_MODULE_ID_TL, context);
#endif
	struct tlshim_sta_info *sta_info;
	adf_nbuf_t buf, next_buf;
	WLANTL_STARxCBType data_rx = NULL;

	if (staid >= WLAN_MAX_STA_COUNT) {
		TLSHIM_LOGE("Invalid sta id :%d", staid);
		goto drop_rx_buf;
	}

	tl_shim = (struct txrx_tl_shim_ctx *) context;
	sta_info = &tl_shim->sta_info[staid];

	adf_os_spin_lock_bh(&sta_info->stainfo_lock);
	if (sta_info->registered)
		data_rx = sta_info->data_rx;
	adf_os_spin_unlock_bh(&sta_info->stainfo_lock);

	/*
	 * If there is a data frame from peer before the peer is
	 * registered for data service, enqueue them on to pending queue
	 * which will be flushed to HDD once that station is registered.
	 */
	if (!data_rx) {
		struct tlshim_buf *cache_buf;
		buf = rx_buf_list;
		while (buf) {
			next_buf = adf_nbuf_queue_next(buf);
			cache_buf = adf_os_mem_alloc(NULL, sizeof(*cache_buf));
			if (!cache_buf) {
				TLSHIM_LOGE("Failed to allocate buf to cache the rx frames");
				adf_nbuf_free(buf);
			} else {
				/* Add NULL terminator */
				adf_nbuf_set_next(buf, NULL);
				cache_buf->buf = buf;
				adf_os_spin_lock_bh(&tl_shim->bufq_lock);
				list_add_tail(&cache_buf->list,
					      &sta_info->cached_bufq);
				adf_os_spin_unlock_bh(&tl_shim->bufq_lock);
			}
			buf = next_buf;
		}
	} else { /* Send rx packet to HDD if there is no frame pending in cached_bufq */
		/* Suspend frames flush from timer */
		/*
		 * TODO: Need to see if acquiring/releasing lock even when
		 * there is no cached frames have any significant impact on
		 * performance.
		 */
#if defined (IPA_OFFLOAD) && !defined(IPA_UC_OFFLOAD)
		VOS_STATUS ret;
		adf_os_spin_lock_bh(&tl_shim->bufq_lock);
		sta_info->suspend_flush = 1;
		adf_os_spin_unlock_bh(&tl_shim->bufq_lock);

		/* Flush the cached frames to HDD before passing new rx frame */
		tl_shim_flush_rx_frames(vos_ctx, tl_shim, staid, 0);

		if (!adf_os_atomic_read(&tl_shim->vdev_active[sta_info->vdev_id])) {
			TLSHIM_LOGW("INACTIVE VDEV");
			goto drop_rx_buf;
		}
		ret = data_rx(vos_ctx, rx_buf_list, staid);
		if (ret == VOS_STATUS_E_INVAL) {
#endif

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
			/*
			 * in case following returns true, a defered task was created
			 * inside function, which does following:
			 * 1) create vos packet
			 * 2) send to PE/LIM
			 * 3) free the involved sk_buff
			 */
			if (tlshim_check_n_process_iapp_frame(vos_ctx,
							&rx_buf_list, staid))
				return;

			/*
			 * above returned false, the packet was not IAPP.
			 * process normally
			 */
#endif
#ifdef QCA_CONFIG_SMP
			/*
			 * If the kernel is SMP, schedule rx thread to
			 * better use multicores.
			 */
			if (!tl_shim->enable_rxthread) {
				tlshim_data_rx_cb(tl_shim, rx_buf_list, staid);
			} else {
				pVosSchedContext sched_ctx =
						get_vos_sched_ctxt();
				struct VosTlshimPkt *pkt;

				if (unlikely(!sched_ctx))
					goto drop_rx_buf;

				pkt = vos_alloc_tlshim_pkt(sched_ctx);
				if (!pkt) {
					TLSHIM_LOGW("No available Rx message buffer");
					goto drop_rx_buf;
				}
				pkt->callback = (vos_tlshim_cb)
						tlshim_data_rx_cb;
				pkt->context = (void *) tl_shim;
				pkt->Rxpkt = (void *) rx_buf_list;
				pkt->staId = staid;
				vos_indicate_rxpkt(sched_ctx, pkt);
			}
#else /* QCA_CONFIG_SMP */
			tlshim_data_rx_cb(tl_shim, rx_buf_list, staid);
#endif /* QCA_CONFIG_SMP */
#if defined(IPA_OFFLOAD) && !defined(IPA_UC_OFFLOAD)
	}
#endif
	}

	return;

drop_rx_buf:
	TLSHIM_LOGW("Dropping rx packets");
	buf = rx_buf_list;
	while (buf) {
		next_buf = adf_nbuf_queue_next(buf);
		adf_nbuf_free(buf);
		buf = next_buf;
	}
}

static void tl_shim_cache_flush_work(struct work_struct *work)
{
	struct txrx_tl_shim_ctx *tl_shim = container_of(work,
			struct txrx_tl_shim_ctx, cache_flush_work);
	void *vos_ctx = vos_get_global_context(VOS_MODULE_ID_TL, NULL);
	struct tlshim_sta_info *sta_info;
	u_int8_t i;

	for (i = 0; i < WLAN_MAX_STA_COUNT; i++) {
		sta_info = &tl_shim->sta_info[i];
		adf_os_spin_lock_bh(&sta_info->stainfo_lock);
		if (!sta_info->registered) {
			adf_os_spin_unlock_bh(&sta_info->stainfo_lock);
			continue;
        }

		adf_os_spin_lock_bh(&tl_shim->bufq_lock);
		if (sta_info->suspend_flush) {
			adf_os_spin_unlock_bh(&tl_shim->bufq_lock);
			adf_os_spin_unlock_bh(&sta_info->stainfo_lock);
			continue;
		}
		adf_os_spin_unlock_bh(&tl_shim->bufq_lock);
		adf_os_spin_unlock_bh(&sta_info->stainfo_lock);

		tl_shim_flush_rx_frames(vos_ctx, tl_shim, i, 0);
	}
}

/*************************/
/*	TL APIs		 */
/*************************/

/*
 * TL API called from WMA to register a vdev for data service with
 * txrx. This API is called once vdev create succeeds.
 */
void WLANTL_RegisterVdev(void *vos_ctx, void *vdev)
{
	struct txrx_tl_shim_ctx *tl_shim;
	struct ol_txrx_osif_ops txrx_ops;
	struct ol_txrx_vdev_t *vdev_handle = (struct ol_txrx_vdev_t  *) vdev;

	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_ctx);

	if (!tl_shim) {
		TLSHIM_LOGE("%s: Failed to get TLSHIM context", __func__);
		return;
	}

#ifdef QCA_LL_TX_FLOW_CT
	txrx_ops.tx.flow_control_cb = WLANTL_TXFlowControlCb;
	tl_shim->session_flow_control[vdev_handle->vdev_id].vdev = vdev;
#endif /* QCA_LL_TX_FLOW_CT */
	txrx_ops.rx.std = tlshim_data_rx_handler;
	wdi_in_osif_vdev_register(vdev_handle, tl_shim, &txrx_ops);
	/* TODO: Keep vdev specific tx callback, if needed */
	tl_shim->tx = txrx_ops.tx.std;
	adf_os_atomic_set(&tl_shim->vdev_active[vdev_handle->vdev_id], 1);
}

/*
 * TL API called from WMA to un-register a vdev for data service with
 * txrx. This API is called once vdev delete.
 */
void WLANTL_UnRegisterVdev(void *vos_ctx, u_int8_t vdev_id)
{
	struct txrx_tl_shim_ctx *tl_shim;

	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_ctx);
	if (!tl_shim) {
		TLSHIM_LOGE("%s: Failed to get TLSHIM context", __func__);
		return;
	}

	adf_os_atomic_set(&tl_shim->vdev_active[vdev_id], 0);
#ifdef QCA_LL_TX_FLOW_CT
	WLANTL_DeRegisterTXFlowControl(vos_ctx, vdev_id);
#endif /* QCA_LL_TX_FLOW_CT */
}

/**
 * tlshim_peer_validity() - determines whether peer is valid or not
 * @vos_ctx: vos context
 * @sta_id: station id
 *
 * Return: on success return vdev, NULL when peer is invalid/NULL
 */
void *tlshim_peer_validity(void *vos_ctx, uint8_t sta_id)
{
	struct txrx_tl_shim_ctx *tl_shim = vos_get_context(VOS_MODULE_ID_TL,
							vos_ctx);
	struct ol_txrx_peer_t *peer;

	if (!tl_shim) {
		TLSHIM_LOGE("tl_shim is NULL");
		return NULL;
	}

	if (sta_id >= WLAN_MAX_STA_COUNT) {
		TLSHIM_LOGE("Invalid sta id for data tx");
		return NULL;
	}

	if (!tl_shim->sta_info[sta_id].registered) {
		TLSHIM_LOGW("Staion is not yet registered for data service");
		return NULL;
	}

	peer = ol_txrx_peer_find_by_local_id(
			((pVosContextType) vos_ctx)->pdev_txrx_ctx,
			sta_id);
	if (!peer) {
		TLSHIM_LOGW("Invalid peer");
		return NULL;
	} else {
		return (void *)peer->vdev;
	}
}

/**
 * WLANTL_SendSTA_DataFrame() - transmit frame from upper layers
 * @vos_ctx: pointer to vos context
 * @vdev: vdev
 * @skb: pointer to OS packet list
 *
 * Return: return NULL in success, pointer to skb list in case of failure
 */
adf_nbuf_t WLANTL_SendSTA_DataFrame(void *vos_ctx, void *vdev,
				    adf_nbuf_t skb
#ifdef QCA_PKT_PROTO_TRACE
				  , v_U8_t proto_type
#endif /* QCA_PKT_PROTO_TRACE */
                   )
{
	struct txrx_tl_shim_ctx *tl_shim = vos_get_context(VOS_MODULE_ID_TL,
							   vos_ctx);
	void *adf_ctx = vos_get_context(VOS_MODULE_ID_ADF, vos_ctx);
	adf_nbuf_t ret, skb_list_head;

	if (!tl_shim) {
		TLSHIM_LOGE("tl_shim is NULL");
		return skb;
	}

	if (!adf_ctx) {
		TLSHIM_LOGE("adf_ct is NULL");
		return skb;
	}

	if (vos_is_load_unload_in_progress(VOS_MODULE_ID_TL, NULL)) {
		TLSHIM_LOGW("%s: Driver load/unload in progress", __func__);
		return skb;
	}

	skb_list_head = skb;
	while (skb) {
		/* Zero out skb's context buffer for the driver to use */
		adf_os_mem_set(skb->cb, 0, sizeof(skb->cb));
		adf_nbuf_map_single(adf_ctx, skb, ADF_OS_DMA_TO_DEVICE);

#ifdef QCA_PKT_PROTO_TRACE
		adf_nbuf_trace_set_proto_type(skb, proto_type);
#endif /* QCA_PKT_PROTO_TRACE */

		if ((tl_shim->ip_checksum_offload) &&
			(skb->protocol == htons(ETH_P_IP))
			 && (skb->ip_summed == CHECKSUM_PARTIAL))
			skb->ip_summed = CHECKSUM_COMPLETE;

		skb = skb->next;
	}

	ret = tl_shim->tx(vdev, skb_list_head);
	if (ret) {
		skb_list_head = ret;
		TLSHIM_LOGW("Failed to tx");
		while (ret) {
			adf_nbuf_unmap_single(adf_ctx, ret,
						ADF_OS_DMA_TO_DEVICE);
			ret = ret->next;
		}
		return skb_list_head;
	}

	return NULL;
}

#ifdef IPA_OFFLOAD
adf_nbuf_t WLANTL_SendIPA_DataFrame(void *vos_ctx, void *vdev,
                                    adf_nbuf_t skb,  v_U8_t interface_id)
{
    struct txrx_tl_shim_ctx *tl_shim = vos_get_context(VOS_MODULE_ID_TL,
                                                           vos_ctx);
	adf_nbuf_t ret;

	ENTER();
	if (NULL == tl_shim) {
		TLSHIM_LOGW("INVALID TL SHIM CONTEXT");
		return skb;
	}

	if (!adf_os_atomic_read(&tl_shim->vdev_active[interface_id])) {
		TLSHIM_LOGW("INACTIVE VDEV");
		return skb;
	}

	if ((tl_shim->ip_checksum_offload) && (skb->protocol == htons(ETH_P_IP))
		 && (skb->ip_summed == CHECKSUM_PARTIAL))
		skb->ip_summed = CHECKSUM_COMPLETE;

	/* Terminate the (single-element) list of tx frames */
	skb->next = NULL;
	ret = tl_shim->tx((struct ol_txrx_vdev_t *)vdev, skb);
	if (ret) {
		TLSHIM_LOGW("Failed to tx");
		return ret;
	}

	return NULL;
}
#endif

VOS_STATUS WLANTL_ResumeDataTx(void *vos_ctx, u_int8_t *sta_id)
{
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_SuspendDataTx(void *vos_ctx, u_int8_t *sta_id,
				WLANTL_SuspendCBType suspend_tx_cb)
{
	return VOS_STATUS_SUCCESS;
}

void WLANTL_AssocFailed(u_int8_t sta_id)
{
	/* Not needed */
}

VOS_STATUS WLANTL_Finish_ULA(void (*cb) (void *cb_ctx), void *cb_ctx)
{
	/* Not needed */
	return VOS_STATUS_SUCCESS;
}

void WLANTLPrintPktsRcvdPerRssi(void *vos_ctx, u_int8_t sta_id, bool flush)
{
	/* TBD */
}

void WLANTLPrintPktsRcvdPerRateIdx(void *vos_ctx, u_int8_t sta_id, bool flush)
{
	/* TBD */
}

VOS_STATUS WLANTL_TxProcessMsg(void *vos_ctx, vos_msg_t *msg)
{
	/* Not needed */
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_McProcessMsg(void *vos_ctx, vos_msg_t *message)
{
	/* Not needed */
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_McFreeMsg(void *vos_ctx, vos_msg_t *message)
{
	/* Not needed */
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_TxFreeMsg(void *vos_ctx, vos_msg_t *message)
{
	/* Not needed */
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_GetSoftAPStatistics(void *vos_ctx,
				      WLANTL_TRANSFER_STA_TYPE *stats_sum,
				      v_BOOL_t reset)
{
	/* TBD */
	return VOS_STATUS_SUCCESS;
}

/*
 * Return txrx stats for a given sta_id
 */
VOS_STATUS WLANTL_GetStatistics(void *vos_ctx,
				WLANTL_TRANSFER_STA_TYPE *stats_buf,
				u_int8_t sta_id)
{
	/*
	 * TODO: Txrx to be modified to maintain per peer stats which
	 * TL shim can return whenever requested.
	 */
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_DeregRSSIIndicationCB(void *adapter, v_S7_t rssi,
					u_int8_t trig_evt,
					WLANTL_RSSICrossThresholdCBType func,
					VOS_MODULE_ID mod_id)
{
	/* TBD */
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_RegRSSIIndicationCB(void *adapter, v_S7_t rssi,
				      u_int8_t trig_evt,
				      WLANTL_RSSICrossThresholdCBType func,
				      VOS_MODULE_ID mod_id, void *usr_ctx)
{
	/* TBD */
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_EnableUAPSDForAC(void *vos_ctx, u_int8_t sta_id,
				   WLANTL_ACEnumType ac, u_int8_t tid,
				   u_int8_t pri, v_U32_t srvc_int,
				   v_U32_t sus_int, WLANTL_TSDirType dir,
				   u_int8_t psb, v_U32_t sessionId)
{
	tp_wma_handle wma_handle;
	t_wma_trigger_uapsd_params uapsd_params;
	struct txrx_tl_shim_ctx *tl_shim;
	enum uapsd_ac access_category;

	ENTER();

	if (!psb) {
		TLSHIM_LOGD("No need to configure auto trigger:psb is 0");
		return VOS_STATUS_SUCCESS;
	}

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);
	if (!wma_handle) {
		TLSHIM_LOGE("wma_handle is NULL");
		return VOS_STATUS_E_FAILURE;
	}

	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_ctx);
	if (!tl_shim) {
		TLSHIM_LOGE("tl_shim is NULL");
		return VOS_STATUS_E_FAILURE;
	}

	switch (ac) {
		case WLANTL_AC_BK:
			access_category = UAPSD_BK;
			break;
		case WLANTL_AC_BE:
			access_category = UAPSD_BE;
			break;
		case WLANTL_AC_VI:
			access_category = UAPSD_VI;
			break;
		case WLANTL_AC_VO:
			access_category = UAPSD_VO;
			break;
		default:
			return VOS_STATUS_E_FAILURE;
	}

	uapsd_params.wmm_ac = access_category;
	uapsd_params.user_priority = pri;
	uapsd_params.service_interval = srvc_int;
	uapsd_params.delay_interval = tl_shim->delay_interval;
	uapsd_params.suspend_interval = sus_int;

	if(VOS_STATUS_SUCCESS !=
		wma_trigger_uapsd_params(wma_handle, sessionId, &uapsd_params))
	{
		TLSHIM_LOGE("Failed to Trigger Uapsd params for sessionId %d",
					sessionId);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_DisableUAPSDForAC(void *vos_ctx, u_int8_t sta_id,
				    WLANTL_ACEnumType ac, v_U32_t sessionId)
{
	tp_wma_handle wma_handle;
	enum uapsd_ac access_category;
	ENTER();

	switch (ac) {
		case WLANTL_AC_BK:
			access_category = UAPSD_BK;
			break;
		case WLANTL_AC_BE:
			access_category = UAPSD_BE;
			break;
		case WLANTL_AC_VI:
			access_category = UAPSD_VI;
			break;
		case WLANTL_AC_VO:
			access_category = UAPSD_VO;
			break;
		default:
			return VOS_STATUS_E_FAILURE;
	}

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);
	if (!wma_handle) {
		TLSHIM_LOGE("wma handle is NULL");
		return VOS_STATUS_E_FAILURE;
	}
	if (VOS_STATUS_SUCCESS !=
	wma_disable_uapsd_per_ac(wma_handle, sessionId, access_category)) {
		TLSHIM_LOGE("Failed to disable uapsd for ac %d for sessionId %d",
					ac, sessionId);
		return VOS_STATUS_E_FAILURE;
	}
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_DeRegisterMgmtFrmClient(void *vos_ctx)
{
	struct txrx_tl_shim_ctx *tl_shim;
	tp_wma_handle wma_handle;
	ENTER();

#ifdef QCA_WIFI_FTM
	if (vos_get_conparam() == VOS_FTM_MODE)
		return VOS_STATUS_SUCCESS;
#endif

	tl_shim = vos_get_context(VOS_MODULE_ID_TL,
				  vos_ctx);
	if (!tl_shim) {
		TLSHIM_LOGE("%s: Failed to get TLSHIM context", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);
	if (!wma_handle) {
		TLSHIM_LOGE("%s: Failed to get WMA context", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	if (wmi_unified_unregister_event_handler(wma_handle->wmi_handle,
						 WMI_MGMT_RX_EVENTID) != 0) {
		TLSHIM_LOGE("Failed to Unregister rx mgmt handler with wmi");
		return VOS_STATUS_E_FAILURE;
	}
	tl_shim->mgmt_rx = NULL;
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_RegisterMgmtFrmClient(void *vos_ctx,
					WLANTL_MgmtFrmRxCBType mgmt_frm_rx)
{
	struct txrx_tl_shim_ctx *tl_shim = vos_get_context(VOS_MODULE_ID_TL,
							   vos_ctx);

	tp_wma_handle wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);
	if (!tl_shim) {
		TLSHIM_LOGE("%s: Failed to get TLSHIM context", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	if (!wma_handle) {
		TLSHIM_LOGE("%s: Failed to get WMA context", __func__);
		return VOS_STATUS_E_FAILURE;
	}
	if (wmi_unified_register_event_handler(wma_handle->wmi_handle,
					       WMI_MGMT_RX_EVENTID,
					       tlshim_mgmt_rx_wmi_handler)
					       != 0) {
		TLSHIM_LOGE("Failed to register rx mgmt handler with wmi");
		return VOS_STATUS_E_FAILURE;
	}
	tl_shim->mgmt_rx = mgmt_frm_rx;

	return VOS_STATUS_SUCCESS;
}

/*
 * Return the data rssi for the given peer.
 */
VOS_STATUS WLANTL_GetRssi(void *vos_ctx, u_int8_t sta_id, v_S7_t *rssi, void *pGetRssiReq)
{
	tp_wma_handle wma_handle;
	struct txrx_tl_shim_ctx *tl_shim;
	struct tlshim_sta_info *sta_info;
	v_S7_t first_rssi;

	ENTER();

	wma_handle = vos_get_context(VOS_MODULE_ID_WDA, vos_ctx);
	if (!wma_handle) {
		TLSHIM_LOGE("wma_handle is NULL");
		return VOS_STATUS_E_FAILURE;
	}

	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_ctx);
	if (!tl_shim) {
		TLSHIM_LOGE("tl_shim is NULL");
		return VOS_STATUS_E_FAULT;
	}

	if (sta_id >= WLAN_MAX_STA_COUNT) {
		TLSHIM_LOGE("Invalid sta id :%d", sta_id);
		return VOS_STATUS_E_INVAL;
	}

	sta_info = &tl_shim->sta_info[sta_id];
	first_rssi = sta_info->first_rssi;

	if(VOS_STATUS_SUCCESS !=
		wma_send_snr_request(wma_handle, pGetRssiReq, first_rssi)) {
		TLSHIM_LOGE("Failed to Trigger wma stats request");
		return VOS_STATUS_E_FAILURE;
	}

	/* dont send success, otherwise call back
	 * will released with out values */
	return VOS_STATUS_E_BUSY;
}

/*
 * HDD will directly call tx function with the skb for transmission.
 * Txrx is reponsible to enqueue the packet and schedule it for Hight
 * Latency devices, so this API is not used for CLD.
 */
VOS_STATUS WLANTL_STAPktPending(void *vos_ctx, u_int8_t sta_id,
				WLANTL_ACEnumType ac)
{
	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_UpdateSTABssIdforIBSS(void *vos_ctx, u_int8_t sta_id,
					u_int8_t *bssid)
{
	/* TBD */
	return VOS_STATUS_SUCCESS;
}

/*
 * In CLD, sec_type along with the peer_state will be used to
 * make sure EAPOL frame after PTK is installed is getting encrypted.
 * So this API is no-op.
 */
VOS_STATUS WLANTL_STAPtkInstalled(void *vos_ctx, u_int8_t sta_id)
{
	return VOS_STATUS_SUCCESS;
}

/*
 * HDD calls this to notify the state change in client.
 * Txrx will do frame filtering.
 */
VOS_STATUS WLANTL_ChangeSTAState(void *vos_ctx, u_int8_t sta_id,
				 WLANTL_STAStateType sta_state,
				 v_BOOL_t roam_synch_in_progress)
{
	struct ol_txrx_peer_t *peer;
	enum ol_txrx_peer_state txrx_state = ol_txrx_peer_state_invalid;
	int err;
	struct txrx_tl_shim_ctx *tl_shim = vos_get_context(VOS_MODULE_ID_TL,
							   vos_ctx);

	ENTER();

	if (!tl_shim) {
		TLSHIM_LOGE("%s: Failed to get TLSHIM context", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	if (sta_id >= WLAN_MAX_STA_COUNT) {
		TLSHIM_LOGE("Invalid sta id :%d", sta_id);
		return VOS_STATUS_E_INVAL;
	}
	peer = ol_txrx_peer_find_by_local_id(
			((pVosContextType) vos_ctx)->pdev_txrx_ctx,
			sta_id);

	if ((peer == NULL) ||
                (adf_os_atomic_read(&peer->delete_in_progress) == 1))
		return VOS_STATUS_E_FAULT;

	if (sta_state == WLANTL_STA_CONNECTED)
		txrx_state = ol_txrx_peer_state_conn;
	else if (sta_state == WLANTL_STA_AUTHENTICATED)
		txrx_state = ol_txrx_peer_state_auth;

	ol_txrx_peer_state_update(peer->vdev->pdev,
				  (u_int8_t *) peer->mac_addr.raw,
				  txrx_state);
#ifdef WLAN_FEATURE_ROAM_OFFLOAD
	if (roam_synch_in_progress)
		return VOS_STATUS_SUCCESS;
#endif


	if (txrx_state == ol_txrx_peer_state_auth) {
#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
		/* make sure event is reset */
		vos_event_reset(&tl_shim->peer_authorized_events[peer->vdev->vdev_id]);
#endif
		err = wma_set_peer_param(
				((pVosContextType) vos_ctx)->pWDAContext,
				peer->mac_addr.raw, WMI_PEER_AUTHORIZE,
				1, peer->vdev->vdev_id);
		if (err) {
			TLSHIM_LOGE("Failed to set the peer state to authorized");
			return VOS_STATUS_E_FAULT;
		}

		if (peer->vdev->opmode == wlan_op_mode_sta) {
#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
			/*
			 * TODO: following code waits on event without
			 * checking if the event was already set. Currently
			 * there is no vos api to check if event was already
			 * set fix it cleanly later.
			 */
			/* wait for event from firmware to set the event */
			err = vos_wait_single_event(&tl_shim->peer_authorized_events[peer->vdev->vdev_id],
					      TLSHIM_PEER_AUTHORIZE_WAIT);
			if (err != VOS_STATUS_SUCCESS)
				TLSHIM_LOGE("%s:timeout for peer_authorized_event",
							__func__);
			wdi_in_vdev_unpause(peer->vdev,
				    OL_TXQ_PAUSE_REASON_PEER_UNAUTHORIZED);
#endif
		}
	}

	return VOS_STATUS_SUCCESS;
}

/*
 * Clear the station information.
 */
VOS_STATUS WLANTL_ClearSTAClient(void *vos_ctx, u_int8_t sta_id)
{
	struct txrx_tl_shim_ctx *tl_shim;

	if (sta_id >= WLAN_MAX_STA_COUNT) {
		TLSHIM_LOGE("Invalid sta id :%d", sta_id);
		return VOS_STATUS_E_INVAL;
	}

	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_ctx);
	if (!tl_shim) {
		TLSHIM_LOGE("%s: Failed to get TLSHIM context", __func__);
		return VOS_STATUS_E_FAILURE;
	}

#ifdef QCA_CONFIG_SMP
	{
		pVosSchedContext sched_ctx = get_vos_sched_ctxt();
		/* Drop pending Rx frames in VOSS */
		if (sched_ctx)
			vos_drop_rxpkt_by_staid(sched_ctx, sta_id);
	}
#endif

	/* Purge the cached rx frame queue */
	tl_shim_flush_rx_frames(vos_ctx, tl_shim, sta_id, 1);
	adf_os_spin_lock_bh(&tl_shim->bufq_lock);
	tl_shim->sta_info[sta_id].suspend_flush = 0;
	adf_os_spin_unlock_bh(&tl_shim->bufq_lock);

	adf_os_spin_lock_bh(&tl_shim->sta_info[sta_id].stainfo_lock);
	tl_shim->sta_info[sta_id].registered = 0;
	tl_shim->sta_info[sta_id].data_rx = NULL;
	tl_shim->sta_info[sta_id].first_rssi = 0;
	adf_os_spin_unlock_bh(&tl_shim->sta_info[sta_id].stainfo_lock);

	return VOS_STATUS_SUCCESS;
}

/*
 * Register a station for data service. This API gives flexibility
 * to register different callbacks for different client though it is
 * needed to register different callbacks for every vdev. Only rxcb
 * is used.
 */
VOS_STATUS WLANTL_RegisterSTAClient(void *vos_ctx,
				    WLANTL_STARxCBType rxcb,
				    WLAN_STADescType *sta_desc, v_S7_t rssi)
{
	struct txrx_tl_shim_ctx *tl_shim;
	struct ol_txrx_peer_t *peer;
	ol_txrx_peer_update_param_t param;
	struct tlshim_sta_info *sta_info;
	privacy_exemption privacy_filter;

	ENTER();
	if (sta_desc->ucSTAId >= WLAN_MAX_STA_COUNT) {
		TLSHIM_LOGE("Invalid sta id :%d", sta_desc->ucSTAId);
		return VOS_STATUS_E_INVAL;
	}
	peer = ol_txrx_peer_find_by_local_id(
			((pVosContextType) vos_ctx)->pdev_txrx_ctx,
			sta_desc->ucSTAId);
	if (!peer)
		return VOS_STATUS_E_FAULT;

	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_ctx);
	if (!tl_shim) {
		TLSHIM_LOGE("tl_shim is NULL");
		return VOS_STATUS_E_FAULT;
	}

	sta_info = &tl_shim->sta_info[sta_desc->ucSTAId];
	adf_os_spin_lock_bh(&sta_info->stainfo_lock);
	sta_info->data_rx = rxcb;
	sta_info->registered = true;
	sta_info->first_rssi = rssi;
	sta_info->vdev_id = peer->vdev->vdev_id;
	adf_os_spin_unlock_bh(&sta_info->stainfo_lock);

	param.qos_capable =  sta_desc->ucQosEnabled;
	wdi_in_peer_update(peer->vdev, peer->mac_addr.raw, &param,
			   ol_txrx_peer_update_qos_capable);
	if (sta_desc->ucIsWapiSta) {
		/*Privacy filter to accept unencrypted WAI frames*/
		privacy_filter.ether_type = ETHERTYPE_WAI;
		privacy_filter.filter_type = PRIVACY_FILTER_ALWAYS;
		privacy_filter.packet_type = PRIVACY_FILTER_PACKET_BOTH;
		ol_txrx_set_privacy_filters(peer->vdev, &privacy_filter, 1);
		/* param.sec_type = ol_sec_type_wapi; */
		/*
		 * TODO: Peer update also updates the other security types
		 * but HDD will not pass this information.

		wdi_in_peer_update(peer->vdev, peer->mac_addr.raw, &param,
				   ol_txrx_peer_update_peer_security);
		 */
	}

	/* Schedule a worker to flush cached rx frames */
	schedule_work(&tl_shim->cache_flush_work);

	return VOS_STATUS_SUCCESS;
}

VOS_STATUS WLANTL_Stop(void *vos_ctx)
{
	/* Nothing to do really */
	return VOS_STATUS_SUCCESS;
}

/*
 * Make txrx module ready
 */
VOS_STATUS WLANTL_Start(void *vos_ctx)
{
	ENTER();
	if (wdi_in_pdev_attach_target(((pVosContextType)
				      vos_ctx)->pdev_txrx_ctx))
		return VOS_STATUS_E_FAULT;
	return VOS_STATUS_SUCCESS;
}

/*
 * Deinit txrx module
 */
VOS_STATUS WLANTL_Close(void *vos_ctx)
{
	struct txrx_tl_shim_ctx *tl_shim;
#if defined(QCA_LL_TX_FLOW_CT) || defined(QCA_SUPPORT_TXRX_VDEV_PAUSE_LL)
	u_int8_t i;
#endif /* QCA_LL_TX_FLOW_CT */

	ENTER();
	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_ctx);
	if (!tl_shim) {
		TLSHIM_LOGE("tl_shim is NULL");
		return VOS_STATUS_E_FAILURE;
	}

#ifdef QCA_LL_TX_FLOW_CT
	for (i = 0;
		i < wdi_out_cfg_max_vdevs(((pVosContextType)vos_ctx)->cfg_ctx);
		i++) {
		adf_os_spinlock_destroy(&tl_shim->session_flow_control[i].fc_lock);
	}
	adf_os_mem_free(tl_shim->session_flow_control);
#endif /* QCA_LL_TX_FLOW_CT */

#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
	for (i = 0;
	     i < wdi_out_cfg_max_vdevs(((pVosContextType)vos_ctx)->cfg_ctx);
	     i++) {
		vos_event_destroy(&tl_shim->peer_authorized_events[i]);
	}
	adf_os_mem_free(tl_shim->peer_authorized_events);
#endif

	adf_os_mem_free(tl_shim->vdev_active);
#ifdef FEATURE_WLAN_ESE
	vos_flush_work(&tl_shim->iapp_work.deferred_work);
#endif
	vos_flush_work(&tl_shim->cache_flush_work);

	wdi_in_pdev_detach(((pVosContextType) vos_ctx)->pdev_txrx_ctx, 1);
	// Delete beacon buffer hanging off tl_shim
	if (tl_shim->last_beacon_data) {
		if (((WMI_MGMT_RX_EVENTID_param_tlvs *) tl_shim->last_beacon_data)->hdr)
			vos_mem_free(((WMI_MGMT_RX_EVENTID_param_tlvs *) tl_shim->last_beacon_data)->hdr);
		if (((WMI_MGMT_RX_EVENTID_param_tlvs *) tl_shim->last_beacon_data)->bufp)
			vos_mem_free(((WMI_MGMT_RX_EVENTID_param_tlvs *) tl_shim->last_beacon_data)->bufp);
		vos_mem_free(tl_shim->last_beacon_data);
	}
	vos_free_context(vos_ctx, VOS_MODULE_ID_TL, tl_shim);
	return VOS_STATUS_SUCCESS;
}

/*
 * Allocate and Initialize transport layer (txrx)
 */
VOS_STATUS WLANTL_Open(void *vos_ctx, WLANTL_ConfigInfoType *tl_cfg)
{
	struct txrx_tl_shim_ctx *tl_shim;
	VOS_STATUS status;
	u_int8_t i;
	int max_vdev;

	ENTER();
	status = vos_alloc_context(vos_ctx, VOS_MODULE_ID_TL,
				   (void *) &tl_shim, sizeof(*tl_shim));
	if (status != VOS_STATUS_SUCCESS)
		return status;

	((pVosContextType) vos_ctx)->pdev_txrx_ctx =
				wdi_in_pdev_attach(
					((pVosContextType) vos_ctx)->cfg_ctx,
					((pVosContextType) vos_ctx)->htc_ctx,
					((pVosContextType) vos_ctx)->adf_ctx);
	if (!((pVosContextType) vos_ctx)->pdev_txrx_ctx) {
		TLSHIM_LOGE("Failed to allocate memory for pdev txrx handle");
		vos_free_context(vos_ctx, VOS_MODULE_ID_TL, tl_shim);
		return VOS_STATUS_E_NOMEM;
	}

	adf_os_spinlock_init(&tl_shim->bufq_lock);
	adf_os_spinlock_init(&tl_shim->mgmt_lock);

	for (i = 0; i < WLAN_MAX_STA_COUNT; i++) {
		tl_shim->sta_info[i].suspend_flush = 0;
		adf_os_spinlock_init(&tl_shim->sta_info[i].stainfo_lock);
		tl_shim->sta_info[i].flags = 0;
		INIT_LIST_HEAD(&tl_shim->sta_info[i].cached_bufq);
	}

	vos_init_work(&tl_shim->cache_flush_work, tl_shim_cache_flush_work);

#if defined(FEATURE_WLAN_ESE) && !defined(FEATURE_WLAN_ESE_UPLOAD)
	vos_init_work(&(tl_shim->iapp_work.deferred_work),
		tlshim_mgmt_over_data_rx_handler);
#endif
	/*
	 * TODO: Allocate memory for tx callback for maximum supported
	 * vdevs to maintain tx callbacks per vdev.
	 */
	max_vdev = wdi_out_cfg_max_vdevs(((pVosContextType)vos_ctx)->cfg_ctx);
	tl_shim->vdev_active = adf_os_mem_alloc(NULL,
		max_vdev * sizeof(adf_os_atomic_t));
	for (i = 0; i < max_vdev; i++) {
		adf_os_atomic_init(&tl_shim->vdev_active[i]);
		adf_os_atomic_set(&tl_shim->vdev_active[i], 0);
	}

#ifdef QCA_LL_TX_FLOW_CT
	tl_shim->session_flow_control = adf_os_mem_alloc(NULL,
			max_vdev * sizeof(struct tlshim_session_flow_Control));
	if (!tl_shim->session_flow_control) {
		TLSHIM_LOGE("Failed to allocate memory for tx flow control");
		vos_free_context(vos_ctx, VOS_MODULE_ID_TL, tl_shim);
		return VOS_STATUS_E_NOMEM;
	}

	for (i = 0; i < max_vdev; i++) {
		tl_shim->session_flow_control[i].flowControl = NULL;
		tl_shim->session_flow_control[i].sessionId = 0xFF;
		tl_shim->session_flow_control[i].adpaterCtxt = NULL;
		tl_shim->session_flow_control[i].vdev = NULL;
		adf_os_spinlock_init(&tl_shim->session_flow_control[i].fc_lock);
	}
#endif /* QCA_LL_TX_FLOW_CT */

#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
	tl_shim->peer_authorized_events = adf_os_mem_alloc(NULL,
					max_vdev * sizeof(vos_event_t));
	if (!tl_shim->peer_authorized_events) {
		TLSHIM_LOGE("Failed to allocate memory for events");
#ifdef QCA_LL_TX_FLOW_CT
		adf_os_mem_free(tl_shim->session_flow_control);
#endif
		vos_free_context(vos_ctx, VOS_MODULE_ID_TL, tl_shim);
		return VOS_STATUS_E_NOMEM;
	}
	for (i = 0; i < max_vdev; i++) {
		status = vos_event_init(&tl_shim->peer_authorized_events[i]);
		if (!VOS_IS_STATUS_SUCCESS(status)) {
			TLSHIM_LOGE("%s: Failed to initialized a event.",
				    __func__);
			adf_os_mem_free(tl_shim->peer_authorized_events);
#ifdef QCA_LL_TX_FLOW_CT
			adf_os_mem_free(tl_shim->session_flow_control);
#endif
			vos_free_context(vos_ctx, VOS_MODULE_ID_TL, tl_shim);
			return status;
		}
	}
#endif

	tl_shim->ip_checksum_offload = tl_cfg->ip_checksum_offload;
	tl_shim->delay_interval = tl_cfg->uDelayedTriggerFrmInt;
	tl_shim->enable_rxthread = tl_cfg->enable_rxthread;
	if (tl_shim->enable_rxthread)
		TLSHIM_LOGD("TL Shim RX thread enabled");

	return status;
}

/*
 * Funtion to retrieve BSSID for peer sta.
 */
VOS_STATUS tl_shim_get_vdevid(struct ol_txrx_peer_t *peer, u_int8_t *vdev_id)
{
	if(!peer) {
		TLSHIM_LOGE("peer argument is null!!");
		return VOS_STATUS_E_FAILURE;
	}

	*vdev_id = peer->vdev->vdev_id;
	return VOS_STATUS_SUCCESS;
}

/*
 * Function to get vdev(tl_context) given the MAC address.
 */
void *tl_shim_get_vdev_by_addr(void *vos_context, uint8_t *mac_addr)
{
	struct ol_txrx_peer_t *peer = NULL;
	ol_txrx_pdev_handle pdev = NULL;
	uint8_t peer_id;

	if (vos_context == NULL || mac_addr == NULL) {
		TLSHIM_LOGE("Invalid argument %p, %p", vos_context, mac_addr);
		return NULL;
	}

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, vos_context);
	if (!pdev) {
		TLSHIM_LOGE("PDEV [%pM] not found", mac_addr);
		return NULL;
	}

	peer = ol_txrx_find_peer_by_addr(pdev, mac_addr, &peer_id);

	if (!peer) {
		TLSHIM_LOGW("PEER [%pM] not found", mac_addr);
		return NULL;
	}

	return peer->vdev;
}

/*
 * Function to get vdev(tl_context) given the TL station ID.
 */
void *tl_shim_get_vdev_by_sta_id(void *vos_context, uint8_t sta_id)
{
	struct ol_txrx_peer_t *peer = NULL;
	ol_txrx_pdev_handle pdev = NULL;

	if (sta_id >= WLAN_MAX_STA_COUNT) {
		TLSHIM_LOGE("Invalid sta id passed");
		return NULL;
	}

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, vos_context);
	if (!pdev) {
                TLSHIM_LOGE("PDEV not found for sta_id [%d]", sta_id);
		return NULL;
	}

	peer = ol_txrx_peer_find_by_local_id(pdev, sta_id);

	if (!peer) {
		TLSHIM_LOGE("PEER [%d] not found", sta_id);
		return NULL;
	}

	return peer->vdev;
}

void
WLANTL_PauseUnPauseQs(void *vos_context, v_BOOL_t flag)
{
	ol_txrx_pdev_handle pdev = vos_get_context(VOS_MODULE_ID_TXRX,
					vos_context);

	if (!pdev) {
		TLSHIM_LOGE("%s, PDEV NULL",__func__);
		return;
	}
	if (true == flag)
		wdi_in_pdev_pause(pdev,
				   OL_TXQ_PAUSE_REASON_VDEV_SUSPEND);
	else
		wdi_in_pdev_unpause(pdev,
				   OL_TXQ_PAUSE_REASON_VDEV_SUSPEND);
}

#ifdef QCA_LL_TX_FLOW_CT
/*
 * WLANTL_Get_llStats - get the stats for TXRX module
 * @sessionId: vdev sessionid.
 * @buffer:  buffer to update the stats
 * @length:  lenth of the buffer
 *
 * HDD will call this API to get the OL-TXRX module stats
 *
 */
void WLANTL_Get_llStats
(
	uint8_t sessionId,
	char *buffer,
	uint16_t length
)
{
	void *vos_context = vos_get_global_context(VOS_MODULE_ID_TL, NULL);
	struct txrx_tl_shim_ctx *tl_shim;
	struct ol_txrx_vdev_t *vdev;

	if (!vos_context) {
		return;
	}

	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_context);
	if (!tl_shim) {
		TLSHIM_LOGD("%s, tl_shim is NULL",
                    __func__);
		return;
	}

	vdev = tl_shim->session_flow_control[sessionId].vdev;
	if (!vdev) {
		TLSHIM_LOGE("%s, vdev is NULL", __func__);
		return;
	}
	ol_txrx_stats(vdev, buffer, (unsigned)length);
	return;

}

/*=============================================================================
  FUNCTION    WLANTL_GetTxResource

  DESCRIPTION
    This function will query WLAN kernel driver TX resource availability.
    Per STA/VDEV instance, if TX resource is not available, should back
    pressure to OS NET layer.

  DEPENDENCIES
    NONE

  PARAMETERS
   IN
   vos_context   : Pointer to VOS global context
   sessionId : VDEV instance to query TX resource
   low_watermark : Low threashold to block OS Q
   high_watermark_offset : Offset to high watermark from low watermark

  RETURN VALUE
    VOS_TRUE : Enough resource available, Not need to PAUSE TX OS Q
    VOS_FALSE : TX resource is not enough, stop OS TX Q

  SIDE EFFECTS

==============================================================================*/
v_BOOL_t WLANTL_GetTxResource
(
	void *vos_context,
	v_U8_t sessionId,
	unsigned int low_watermark,
	unsigned int high_watermark_offset
)
{
	struct txrx_tl_shim_ctx *tl_shim;
	v_BOOL_t enough_resource = VOS_TRUE;
	struct ol_txrx_vdev_t *vdev;

	/* If low watermark is zero, TX flow control is not enabled at all
	 * return TRUE by default */
	if ((!vos_context) || (!low_watermark)) {
		return VOS_TRUE;
	}

	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_context);
	if (!tl_shim) {
		TLSHIM_LOGD("%s, tl_shim is NULL",
                    __func__);
		/* Invalid instace */
		return VOS_TRUE;
	}

	adf_os_spin_lock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);
	if (!tl_shim->session_flow_control[sessionId].vdev) {
		TLSHIM_LOGD("%s, session id %d, VDEV NULL",
                    __func__, sessionId);
		adf_os_spin_unlock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);
		return VOS_TRUE;
	}
	vdev = (struct ol_txrx_vdev_t *)tl_shim->session_flow_control[sessionId].vdev;
	adf_os_spin_unlock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);

	enough_resource = (v_BOOL_t)wdi_in_get_tx_resource(vdev,
							low_watermark,
							high_watermark_offset);

	return enough_resource;
}

/*=============================================================================
  FUNCTION    WLANTL_TXFlowControlCb

  DESCRIPTION
    This function will be called bu TX resource management unit.
    If TC resource management unit reserved enough resource for TX session,
    Call this function to resume OS TX Q.

  PARAMETERS
   IN
   tlContext : Pointer to TL SHIM context
   sessionId : STA/VDEV instance to query TX resource
   resume_tx : Resume OS TX Q or not

  RETURN VALUE
    NONE

  SIDE EFFECTS

==============================================================================*/
void WLANTL_TXFlowControlCb
(
	void  *tlContext,
	v_U8_t sessionId,
	v_BOOL_t resume_tx
)
{
	struct txrx_tl_shim_ctx *tl_shim;
	WLANTL_TxFlowControlCBType flow_control_cb = NULL;
	void *adpter_ctxt = NULL;

	tl_shim = (struct txrx_tl_shim_ctx *)tlContext;
	if (!tl_shim) {
		TLSHIM_LOGE("%s, tl_shim is NULL", __func__);
		/* Invalid instace */
		return;
	}

	adf_os_spin_lock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);
	if ((tl_shim->session_flow_control[sessionId].sessionId == sessionId) &&
		(tl_shim->session_flow_control[sessionId].flowControl)) {
		flow_control_cb = tl_shim->session_flow_control[sessionId].flowControl;
		adpter_ctxt = tl_shim->session_flow_control[sessionId].adpaterCtxt;
	}

	if ((flow_control_cb) && (adpter_ctxt)) {
		flow_control_cb(adpter_ctxt, resume_tx);
	}
	adf_os_spin_unlock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);

	return;
}

/*=============================================================================
  FUNCTION    WLANTL_RegisterTXFlowControl

  DESCRIPTION
    This function will be called by TL client.
    Any device want to enable TX flow control, should register Cb function
    And needed information into TL SHIM

  PARAMETERS
   IN
   vos_ctx : Global OS context context
   sta_id  : STA/VDEV instance index
   flowControl : Flow control callback function pointer
   sessionId : VDEV ID
   adpaterCtxt : VDEV os interface adapter context

  RETURN VALUE
    NONE

  SIDE EFFECTS

==============================================================================*/
void WLANTL_RegisterTXFlowControl
(
	void *vos_ctx,
	WLANTL_TxFlowControlCBType flowControl,
	v_U8_t sessionId,
	void *adpaterCtxt
)
{
	struct txrx_tl_shim_ctx *tl_shim;

	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_ctx);
	if ((!tl_shim) || (!tl_shim->session_flow_control)) {
		TLSHIM_LOGE("%s : Invalid ARG", __func__);
		return;
	}

	if (sessionId >= wdi_out_cfg_max_vdevs(((pVosContextType)vos_ctx)->cfg_ctx)) {
		TLSHIM_LOGE("%s : Invalid session id", __func__);
		return;
	}

	adf_os_spin_lock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);
	tl_shim->session_flow_control[sessionId].flowControl = flowControl;
	tl_shim->session_flow_control[sessionId].sessionId = sessionId;
	tl_shim->session_flow_control[sessionId].adpaterCtxt = adpaterCtxt;
	adf_os_spin_unlock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);

	return;
}

/*=============================================================================
  FUNCTION    WLANTL_DeRegisterTXFlowControl

  DESCRIPTION
    This function will be called by TL client.
    Any device want to close TX flow control, should de-register Cb function
    And needed information into TL SHIM

  PARAMETERS
   IN
   vos_ctx : Global OS context context
   sessionId  : VDEV instance index

  RETURN VALUE
    NONE

  SIDE EFFECTS

==============================================================================*/
void WLANTL_DeRegisterTXFlowControl
(
	void *vos_ctx,
	v_U8_t sessionId
)
{
	struct txrx_tl_shim_ctx *tl_shim;

	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_ctx);
	if (!tl_shim) {
		TLSHIM_LOGE("%s : Invalid ARG", __func__);
		return;
	}

        if (sessionId >= wdi_out_cfg_max_vdevs(((pVosContextType)vos_ctx)->cfg_ctx)) {
                TLSHIM_LOGE("%s : Invalid session id", __func__);
                return;
        }

	adf_os_spin_lock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);
	tl_shim->session_flow_control[sessionId].flowControl = NULL;
	tl_shim->session_flow_control[sessionId].sessionId = 0xFF;
	tl_shim->session_flow_control[sessionId].adpaterCtxt = NULL;
        tl_shim->session_flow_control[sessionId].vdev = NULL;
	adf_os_spin_unlock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);

	return;
}

/*=============================================================================
  FUNCTION    WLANTL_SetAdapterMaxQDepth

  DESCRIPTION
    This function will be called by TL client.
    Based on the adapter TX available bandwidth, set different TX Pause Q size
    Low Bandwidth adapter will have less count of TX Pause Q size to prevent
    reserve all TX descriptors which shared with FW.
    High Bandwidth adapter will have more count of TX Pause Q size

  PARAMETERS
   IN
   vos_ctx : Global OS context context
   sessionId  : adapter instance index
   max_q_depth : Max pause Q depth for adapter

  RETURN VALUE
    NONE

  SIDE EFFECTS

==============================================================================*/
void WLANTL_SetAdapterMaxQDepth
(
	void *vos_ctx,
	v_U8_t sessionId,
	int max_q_depth
)
{
	struct txrx_tl_shim_ctx *tl_shim;

	tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_ctx);
	if ((!tl_shim) || (!tl_shim->session_flow_control)) {
		TLSHIM_LOGE("%s: TLSHIM NULL or FC main context NULL",
			__func__);
		return;
	}

	adf_os_spin_lock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);
	if (!tl_shim->session_flow_control[sessionId].vdev) {
		TLSHIM_LOGD("%s, session id %d, VDEV NULL",
                    __func__, sessionId);
		adf_os_spin_unlock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);
		return;
	}
	wdi_in_ll_set_tx_pause_q_depth(
		(struct ol_txrx_vdev_t *)tl_shim->session_flow_control[sessionId].vdev,
		max_q_depth);
	adf_os_spin_unlock_bh(&tl_shim->session_flow_control[sessionId].fc_lock);

	return;
}
#endif /* QCA_LL_TX_FLOW_CT */

#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
void tl_shim_set_peer_authorized_event(void *vos_ctx, v_U8_t session_id)
{
	struct txrx_tl_shim_ctx *tl_shim = vos_get_context(VOS_MODULE_ID_TL, vos_ctx);

	if (!tl_shim) {
		TLSHIM_LOGE("%s: Failed to get TLSHIM context", __func__);
		return;
	}

	vos_event_set(&tl_shim->peer_authorized_events[session_id]);
}
#endif

#ifdef IPA_UC_OFFLOAD
/*=============================================================================
  FUNCTION    WLANTL_GetIpaUcResource

  DESCRIPTION
    This function will be called by TL client.
    Data path resource will be used by FW should be allocated within lower layer.
    Shared resource information should be propagated to IPA.
    To propagate resource information, client will use this API

  PARAMETERS
    IN
    vos_ctx : Global OS context context
    ce_sr_base_paddr  : Copy Engine Source Ring base address
    ce_sr_ring_size : Copy Engine Source Ring size
    ce_reg_paddr : Copy engine register address
    tx_comp_ring_base_paddr : TX COMP ring base address
    tx_comp_ring_size : TX COMP ring size
    tx_num_alloc_buffer : Number of TX allocated buffer
    rx_rdy_ring_base_paddr : RX ready ring base address
    rx_rdy_ring_size : RX ready ring size
    rx_proc_done_idx_paddr : RX process done index physical address

  RETURN VALUE
    NONE

  SIDE EFFECTS

==============================================================================*/
void WLANTL_GetIpaUcResource(void *vos_ctx,
	v_U32_t *ce_sr_base_paddr,
	v_U32_t *ce_sr_ring_size,
	v_U32_t *ce_reg_paddr,
	v_U32_t *tx_comp_ring_base_paddr,
	v_U32_t *tx_comp_ring_size,
	v_U32_t *tx_num_alloc_buffer,
	v_U32_t *rx_rdy_ring_base_paddr,
	v_U32_t *rx_rdy_ring_size,
	v_U32_t *rx_proc_done_idx_paddr)
{
	if (!vos_ctx || !((pVosContextType)vos_ctx)->pdev_txrx_ctx) {
		TLSHIM_LOGE("%s: Invalid context", __func__);
		return;
	}

	wdi_in_ipa_uc_get_resource(((pVosContextType)vos_ctx)->pdev_txrx_ctx,
		ce_sr_base_paddr,
		ce_sr_ring_size,
		ce_reg_paddr,
		tx_comp_ring_base_paddr,
		tx_comp_ring_size,
		tx_num_alloc_buffer,
		rx_rdy_ring_base_paddr,
		rx_rdy_ring_size,
		rx_proc_done_idx_paddr);
}

/*=============================================================================
  FUNCTION    WLANTL_SetUcDoorbellPaddr

  DESCRIPTION
    This function will be called by TL client.
    UC controller should provide doorbell register address to firmware
    TL client will call this API to pass doorbell register address to firmware

  PARAMETERS
    IN
    vos_ctx : Global OS context context
    ipa_tx_uc_doorbell_paddr  : Micro Controller WLAN TX COMP doorbell regiser
    ipa_rx_uc_doorbell_paddr  : Micro Controller WLAN RX REDY doorbell regiser

  RETURN VALUE
    NONE

  SIDE EFFECTS

==============================================================================*/
void WLANTL_SetUcDoorbellPaddr(void *vos_ctx,
	v_U32_t ipa_tx_uc_doorbell_paddr,
	v_U32_t ipa_rx_uc_doorbell_paddr)
{
	if (!vos_ctx || !((pVosContextType)vos_ctx)->pdev_txrx_ctx) {
		TLSHIM_LOGE("%s: Invalid context", __func__);
		return;
	}

	wdi_in_ipa_uc_set_doorbell_paddr(((pVosContextType)vos_ctx)->pdev_txrx_ctx,
		ipa_tx_uc_doorbell_paddr,
		ipa_rx_uc_doorbell_paddr);
}

/*=============================================================================
  FUNCTION    WLANTL_SetUcActive

  DESCRIPTION
    This function will be called by TL client.
    Send Micro controller data path active or inactive notification to firmware

  PARAMETERS
    IN
    vos_ctx : Global OS context context
    uc_active  : Micro Controller data path is active or not
    is_tx  : Micro Controller WLAN TX data path is active or not
    is_rx  : Micro Controller WLAN RX data path is active or not

  RETURN VALUE
    NONE

  SIDE EFFECTS

==============================================================================*/
void WLANTL_SetUcActive(void *vos_ctx,
	v_BOOL_t uc_active,
	v_BOOL_t is_tx
)
{
	if (!vos_ctx || !((pVosContextType)vos_ctx)->pdev_txrx_ctx) {
		TLSHIM_LOGE("%s: Invalid context", __func__);
		return;
	}

	TLSHIM_LOGD("%s, ACTIVE %d, TX %d",
		__func__, uc_active, is_tx);
	wdi_in_ipa_uc_set_active(((pVosContextType)vos_ctx)->pdev_txrx_ctx,
			uc_active, is_tx);
}

/*=============================================================================
  FUNCTION    WLANTL_RegisterOPCbFnc

  DESCRIPTION
    This function will be called by TL client.

  PARAMETERS
    IN
    vos_ctx : Global OS context context
    func : callback function pointer

  RETURN VALUE
    NONE

  SIDE EFFECTS

==============================================================================*/
void WLANTL_RegisterOPCbFnc(void *vos_ctx,
	void (*func)(v_U8_t *op_msg, void *usr_ctxt), void *usr_ctxt)
{
	wdi_in_ipa_uc_register_op_cb(((pVosContextType)vos_ctx)->pdev_txrx_ctx,
		func, usr_ctxt);
}

/*=============================================================================
  FUNCTION    WLANTL_disable_intrabss_fwd

  DESCRIPTION
    Function to return if Intra-BSS FWD is disabled or not

  PARAMETERS
    IN
    vdev : vdev handle

  RETURN VALUE
    bool : TRUE if Intra-BSS FWD is disabled, FALSE if not

  SIDE EFFECTS

==============================================================================*/
bool WLANTL_disable_intrabss_fwd(void *vdev)
{
	return ((ol_txrx_vdev_handle)vdev)->disable_intrabss_fwd;
}
#endif /* IPA_UC_OFFLOAD */

/*=============================================================================
  FUNCTION    WLANTL_RegisterOCBPeer

  DESCRIPTION
    Function to register the OCB Self Peer

  PARAMETERS
    IN
    vos_ctx : Global OS context context
    mac_addr : MAC address of self peer

    OUT
    peer_id : Peer ID

  RETURN VALUE
    VOS_STATUS_SUCCESS on success
    VOS_STATUS_E_FAILURE on failure

  SIDE EFFECTS

==============================================================================*/
VOS_STATUS WLANTL_RegisterOCBPeer(void *vos_ctx, uint8_t *mac_addr,
    uint8_t *peer_id)
{
	ol_txrx_pdev_handle pdev;
	ol_txrx_peer_handle peer;

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, vos_ctx);
	if (!pdev) {
		TLSHIM_LOGE("%s: Unable to find pdev!", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	peer = ol_txrx_find_peer_by_addr(pdev, mac_addr, peer_id);
	if (!peer) {
		TLSHIM_LOGE("%s: Unable to find OCB peer!", __func__);
		return VOS_STATUS_E_FAILURE;
	}

	ol_txrx_set_ocb_peer(pdev, peer);

	/* Set peer state to connected */
	ol_txrx_peer_state_update(pdev, peer->mac_addr.raw,
						  ol_txrx_peer_state_auth);

	return VOS_STATUS_SUCCESS;
}

void WLANTL_display_datapath_stats(void *vos_ctx, uint16_t value)
{
	ol_txrx_pdev_handle pdev;

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, vos_ctx);
	if (!pdev) {
		TLSHIM_LOGE("%s: Unable to find pdev!", __func__);
		return;
	}

	wdi_in_display_stats(pdev, value);
	return;
}

void WLANTL_clear_datapath_stats(void *vos_ctx, uint16_t bitmap)
{
	ol_txrx_pdev_handle pdev;

	pdev = vos_get_context(VOS_MODULE_ID_TXRX, vos_ctx);
	if (!pdev) {
		TLSHIM_LOGE("%s: Unable to find pdev!", __func__);
		return;
	}

	wdi_in_clear_stats(pdev, bitmap);
	return;
}
