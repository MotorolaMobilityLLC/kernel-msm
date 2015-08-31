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



#include "htc_debug.h"
#include "htc_internal.h"
#include <adf_nbuf.h> /* adf_nbuf_t */
#include <adf_os_mem.h> /* adf_os_mem_alloc */
#include <vos_getBin.h>
#include "epping_main.h"

//#define USB_HIF_SINGLE_PIPE_DATA_SCHED
//#ifdef USB_HIF_SINGLE_PIPE_DATA_SCHED
#define DATA_EP_SIZE 4
//#endif
#define HTC_DATA_RESOURCE_THRS 256
#define HTC_DATA_MINDESC_PERPACKET 2

typedef enum _HTC_SEND_QUEUE_RESULT {
    HTC_SEND_QUEUE_OK = 0,    /* packet was queued */
    HTC_SEND_QUEUE_DROP = 1,  /* this packet should be dropped */
} HTC_SEND_QUEUE_RESULT;

#ifndef DEBUG_CREDIT
#define DEBUG_CREDIT 0
#endif

#if DEBUG_CREDIT
/* bit mask to enable debug certain endpoint */
static unsigned ep_debug_mask = (1 << ENDPOINT_0) | (1 << ENDPOINT_1) | (1 << ENDPOINT_2);
#endif

/* HTC Control Path Credit History */
A_UINT32 g_htc_credit_history_idx = 0;
HTC_CREDIT_HISTORY htc_credit_history_buffer[HTC_CREDIT_HISTORY_MAX];

void htc_credit_record(htc_credit_exchange_type type, A_UINT32 tx_credit,
                       A_UINT32 htc_tx_queue_depth)
{
    if (HTC_CREDIT_HISTORY_MAX <= g_htc_credit_history_idx)
        g_htc_credit_history_idx = 0;

    htc_credit_history_buffer[g_htc_credit_history_idx].type = type;
    htc_credit_history_buffer[g_htc_credit_history_idx].time =
        adf_get_boottime();
    htc_credit_history_buffer[g_htc_credit_history_idx].tx_credit = tx_credit;
    htc_credit_history_buffer[g_htc_credit_history_idx].htc_tx_queue_depth =
        htc_tx_queue_depth;
    g_htc_credit_history_idx++;
}

void HTC_dump_counter_info(HTC_HANDLE HTCHandle)
{
    HTC_TARGET *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);

    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                    ("\n%s: CE_send_cnt = %d, TX_comp_cnt = %d\n",
                     __func__, target->CE_send_cnt, target->TX_comp_cnt));
}

void HTCGetControlEndpointTxHostCredits(HTC_HANDLE HTCHandle, int *credits)
{
    HTC_TARGET *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);
    HTC_ENDPOINT *pEndpoint;
    int i;

    if (!credits || !target) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("%s: invalid args", __func__));
        return;
    }

    *credits = 0;
    LOCK_HTC_TX(target);
    for (i = 0; i < ENDPOINT_MAX; i++) {
        pEndpoint = &target->EndPoint[i];
        if (pEndpoint->ServiceID == WMI_CONTROL_SVC) {
            *credits = pEndpoint->TxCredits;
            break;
        }
    }
    UNLOCK_HTC_TX(target);
}

static INLINE void RestoreTxPacket(HTC_TARGET *target, HTC_PACKET *pPacket)
{
    if (pPacket->PktInfo.AsTx.Flags & HTC_TX_PACKET_FLAG_FIXUP_NETBUF) {
        adf_nbuf_t netbuf = GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket);
        adf_nbuf_unmap(target->osdev, netbuf, ADF_OS_DMA_TO_DEVICE);
        adf_nbuf_pull_head(netbuf, sizeof(HTC_FRAME_HDR));
        pPacket->PktInfo.AsTx.Flags &= ~HTC_TX_PACKET_FLAG_FIXUP_NETBUF;
    }

}

static void DoSendCompletion(HTC_ENDPOINT       *pEndpoint,
                             HTC_PACKET_QUEUE   *pQueueToIndicate)
{
    do {

        if (HTC_QUEUE_EMPTY(pQueueToIndicate)) {
                /* nothing to indicate */
            break;
        }

        if (pEndpoint->EpCallBacks.EpTxCompleteMultiple != NULL) {
            AR_DEBUG_PRINTF(ATH_DEBUG_SEND, (" HTC calling ep %d, send complete multiple callback (%d pkts) \n",
                     pEndpoint->Id, HTC_PACKET_QUEUE_DEPTH(pQueueToIndicate)));
                /* a multiple send complete handler is being used, pass the queue to the handler */
            pEndpoint->EpCallBacks.EpTxCompleteMultiple(pEndpoint->EpCallBacks.pContext,
                                                        pQueueToIndicate);
                /* all packets are now owned by the callback, reset queue to be safe */
            INIT_HTC_PACKET_QUEUE(pQueueToIndicate);
        } else {
            HTC_PACKET *pPacket;
            /* using legacy EpTxComplete */
            do {
                pPacket = HTC_PACKET_DEQUEUE(pQueueToIndicate);
                AR_DEBUG_PRINTF(ATH_DEBUG_SEND, (" HTC calling ep %d send complete callback on packet %p \n", \
                        pEndpoint->Id, pPacket));
                pEndpoint->EpCallBacks.EpTxComplete(pEndpoint->EpCallBacks.pContext, pPacket);
            } while (!HTC_QUEUE_EMPTY(pQueueToIndicate));
        }

    } while (FALSE);

}

static void SendPacketCompletion(HTC_TARGET *target, HTC_PACKET *pPacket)
{
    HTC_ENDPOINT    *pEndpoint = &target->EndPoint[pPacket->Endpoint];
    HTC_PACKET_QUEUE container;

    RestoreTxPacket(target, pPacket);
    INIT_HTC_PACKET_QUEUE_AND_ADD(&container,pPacket);

    /* do completion */
    DoSendCompletion(pEndpoint,&container);
}

void
HTCSendCompleteCheckCleanup(void *context)
{
    HTC_ENDPOINT *pEndpoint = (HTC_ENDPOINT *) context;
    HTCSendCompleteCheck(pEndpoint, 1);
}


HTC_PACKET *AllocateHTCBundlePacket(HTC_TARGET *target)
{
    HTC_PACKET *pPacket;
    HTC_PACKET_QUEUE *pQueueSave;
    adf_nbuf_t netbuf;
    LOCK_HTC_TX(target);
    if (NULL == target->pBundleFreeList) {
        UNLOCK_HTC_TX(target);
        netbuf = adf_nbuf_alloc(NULL,
                target->MaxMsgsPerHTCBundle * target->TargetCreditSize,
                0,
                4,
                FALSE);
        AR_DEBUG_ASSERT(netbuf);
        if (!netbuf)
        {
            return NULL;
        }
        pPacket = adf_os_mem_alloc(NULL, sizeof(HTC_PACKET));
        AR_DEBUG_ASSERT(pPacket);
        if (!pPacket)
        {
            adf_nbuf_free(netbuf);
            return NULL;
        }
        pQueueSave = adf_os_mem_alloc(NULL, sizeof(HTC_PACKET_QUEUE));
        AR_DEBUG_ASSERT(pQueueSave);
        if (!pQueueSave)
        {
            adf_nbuf_free(netbuf);
            adf_os_mem_free(pPacket);
            return NULL;
        }
        INIT_HTC_PACKET_QUEUE(pQueueSave);
        pPacket->pContext = pQueueSave;
        SET_HTC_PACKET_NET_BUF_CONTEXT(pPacket, netbuf);
        pPacket->pBuffer = adf_nbuf_data(netbuf);
        pPacket->BufferLength = adf_nbuf_len(netbuf);

        //store the original head room so that we can restore this when we "free" the packet
        //free packet puts the packet back on the free list
        pPacket->netbufOrigHeadRoom = adf_nbuf_headroom(netbuf);
        return pPacket;
    }

    //already done malloc - restore from free list
    pPacket = target->pBundleFreeList;
    AR_DEBUG_ASSERT(pPacket);
    if (!pPacket)
    {
        UNLOCK_HTC_TX(target);
        return NULL;
    }
    target->pBundleFreeList = (HTC_PACKET *)pPacket->ListLink.pNext;
    UNLOCK_HTC_TX(target);
    pPacket->ListLink.pNext = NULL;

    return pPacket;
}

void FreeHTCBundlePacket(HTC_TARGET *target, HTC_PACKET *pPacket)
{
    A_UINT32 curentHeadRoom;
    adf_nbuf_t netbuf;
    HTC_PACKET_QUEUE *pQueueSave;

    netbuf = GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket);
    AR_DEBUG_ASSERT(netbuf);
    if (!netbuf)
    {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("\n%s: Invalid netbuf in HTC "
                                        "Packet\n", __func__));
        return;
    }

    // HIF adds data to the headroom section of the nbuf, restore the original
    // size. If this is not done, headroom keeps shrinking with every HIF send
    // and eventually HIF ends up doing another malloc big enough to store the
    // data + its header

    curentHeadRoom = adf_nbuf_headroom(netbuf);
    adf_nbuf_pull_head(netbuf, pPacket->netbufOrigHeadRoom - curentHeadRoom);
    adf_nbuf_trim_tail(netbuf, adf_nbuf_len(netbuf));

    //restore the pBuffer pointer. HIF changes this
    pPacket->pBuffer = adf_nbuf_data(netbuf);
    pPacket->BufferLength = adf_nbuf_len(netbuf);

    //restore queue
    pQueueSave = (HTC_PACKET_QUEUE*)pPacket->pContext;
    AR_DEBUG_ASSERT(pQueueSave);

    INIT_HTC_PACKET_QUEUE(pQueueSave);

    LOCK_HTC_TX(target);
    if (target->pBundleFreeList == NULL) {
        target->pBundleFreeList = pPacket;
        pPacket->ListLink.pNext = NULL;
    } else {
        pPacket->ListLink.pNext = (DL_LIST *)target->pBundleFreeList;
        target->pBundleFreeList = pPacket;
    }
    UNLOCK_HTC_TX(target);
}

#if defined(HIF_USB) || defined(HIF_SDIO)
#ifdef ENABLE_BUNDLE_TX
static A_STATUS HTCSendBundledNetbuf(HTC_TARGET *target,
        HTC_ENDPOINT *pEndpoint,
        unsigned char *pBundleBuffer,
        HTC_PACKET *pPacketTx)
{
    adf_os_size_t data_len;
    A_STATUS status;
    adf_nbuf_t bundleBuf;
    bundleBuf = GET_HTC_PACKET_NET_BUF_CONTEXT(pPacketTx);
    data_len = pBundleBuffer - adf_nbuf_data(bundleBuf);
    adf_nbuf_put_tail(bundleBuf, data_len);
    SET_HTC_PACKET_INFO_TX(pPacketTx,
            target,
            pBundleBuffer,
            data_len,
            pEndpoint->Id,
            HTC_TX_PACKET_TAG_BUNDLED);
    LOCK_HTC_TX(target);
    HTC_PACKET_ENQUEUE(&pEndpoint->TxLookupQueue, pPacketTx);
    UNLOCK_HTC_TX(target);
#if DEBUG_BUNDLE
    adf_os_print(" Send bundle EP%d buffer size:0x%x, total:0x%x, count:%d.\n",
            pEndpoint->Id,
            pEndpoint->TxCreditSize,
            data_len,
            data_len / pEndpoint->TxCreditSize);
#endif
    status = HIFSend_head(target->hif_dev,
               pEndpoint->UL_PipeID,
               pEndpoint->Id,
               data_len,
               bundleBuf);
    if (status != A_OK){
        adf_os_print("%s:HIFSend_head failed(len=%zu).\n", __FUNCTION__,
                data_len);
    }
    return status;
}

static void HTCIssuePacketsBundle(HTC_TARGET *target,
                                  HTC_ENDPOINT *pEndpoint,
                                  HTC_PACKET_QUEUE *pPktQueue)
{
   int              i, frag_count, nbytes;
   adf_nbuf_t       netbuf, bundleBuf;
   unsigned char    *pBundleBuffer = NULL;
   HTC_PACKET       *pPacket = NULL, *pPacketTx = NULL;
   HTC_FRAME_HDR    *pHtcHdr;
   int              creditPad, creditRemainder,transferLength, bundlesSpaceRemaining = 0;
   HTC_PACKET_QUEUE *pQueueSave = NULL;

   bundlesSpaceRemaining = target->MaxMsgsPerHTCBundle * pEndpoint->TxCreditSize;
   pPacketTx = AllocateHTCBundlePacket(target);
   if (!pPacketTx)
   {
       //good time to panic
       AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("AllocateHTCBundlePacket failed \n"));
       AR_DEBUG_ASSERT(FALSE);
       return;
   }
   bundleBuf = GET_HTC_PACKET_NET_BUF_CONTEXT(pPacketTx);
   pBundleBuffer = adf_nbuf_data(bundleBuf);
   pQueueSave = (HTC_PACKET_QUEUE*)pPacketTx->pContext;
   while (1) {
       pPacket = HTC_PACKET_DEQUEUE(pPktQueue);
       if (pPacket == NULL){
           break;
       }
       creditPad = 0;
       transferLength = pPacket->ActualLength + HTC_HDR_LENGTH;
       creditRemainder = transferLength % pEndpoint->TxCreditSize;
       if(creditRemainder != 0){
            if(transferLength < pEndpoint->TxCreditSize){
                creditPad = pEndpoint->TxCreditSize - transferLength;
            } else {
                creditPad = creditRemainder;
            }
            transferLength += creditPad;
       }

       if (bundlesSpaceRemaining < transferLength){
           /* send out previous buffer */
           HTCSendBundledNetbuf(target, pEndpoint, pBundleBuffer, pPacketTx);
           if (HTC_PACKET_QUEUE_DEPTH(pPktQueue) < HTC_MIN_MSG_PER_BUNDLE){
               return;
           }
           bundlesSpaceRemaining = target->MaxMsgsPerHTCBundle * pEndpoint->TxCreditSize;
           pPacketTx = AllocateHTCBundlePacket(target);
           if (!pPacketTx)
           {
               //good time to panic
               AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("AllocateHTCBundlePacket failed \n"));
               AR_DEBUG_ASSERT(FALSE);
               return;
           }
           bundleBuf = GET_HTC_PACKET_NET_BUF_CONTEXT(pPacketTx);
           pBundleBuffer = adf_nbuf_data(bundleBuf);
           pQueueSave = (HTC_PACKET_QUEUE*)pPacketTx->pContext;
       }

       bundlesSpaceRemaining -= transferLength;
       netbuf = GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket);
       pHtcHdr = (HTC_FRAME_HDR *) adf_nbuf_get_frag_vaddr(netbuf, 0);
       HTC_WRITE32(pHtcHdr, SM(pPacket->ActualLength, HTC_FRAME_HDR_PAYLOADLEN) |
               SM(pPacket->PktInfo.AsTx.SendFlags | HTC_FLAGS_SEND_BUNDLE, HTC_FRAME_HDR_FLAGS) |
               SM(pPacket->Endpoint, HTC_FRAME_HDR_ENDPOINTID));
       HTC_WRITE32((A_UINT32 *)pHtcHdr + 1,
                 SM(pPacket->PktInfo.AsTx.SeqNo, HTC_FRAME_HDR_CONTROLBYTES1) |
                 SM(creditPad, HTC_FRAME_HDR_RESERVED));
       pHtcHdr->reserved = creditPad;
       frag_count = adf_nbuf_get_num_frags(netbuf);
       nbytes = pPacket->ActualLength + HTC_HDR_LENGTH;
       for (i = 0; i < frag_count && nbytes > 0; i ++){
           int frag_len = adf_nbuf_get_frag_len(netbuf, i);
           unsigned char *frag_addr = adf_nbuf_get_frag_vaddr(netbuf, i);
           if (frag_len > nbytes){
               frag_len = nbytes;
           }
           A_MEMCPY(pBundleBuffer, frag_addr, frag_len);
           nbytes -= frag_len;
           pBundleBuffer += frag_len;
       }
       HTC_PACKET_ENQUEUE(pQueueSave, pPacket);
       pBundleBuffer += creditPad;
   }
   if (pBundleBuffer != adf_nbuf_data(bundleBuf)){
       /* send out remaining buffer */
       HTCSendBundledNetbuf(target, pEndpoint, pBundleBuffer, pPacketTx);
   } else {
       FreeHTCBundlePacket(target, pPacketTx);
   }
}
#endif /* ENABLE_BUNDLE_TX */
#endif

static A_STATUS HTCIssuePackets(HTC_TARGET       *target,
                                HTC_ENDPOINT     *pEndpoint,
                                HTC_PACKET_QUEUE *pPktQueue)
{
    A_STATUS            status = A_OK;
    adf_nbuf_t          netbuf;
    HTC_PACKET          *pPacket = NULL;
    u_int16_t           payloadLen;
    HTC_FRAME_HDR       *pHtcHdr;

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND, ("+HTCIssuePackets: Queue: %p, Pkts %d \n",
                    pPktQueue, HTC_PACKET_QUEUE_DEPTH(pPktQueue)));
    while (TRUE) {
#if defined(HIF_USB) || defined(HIF_SDIO)
#ifdef ENABLE_BUNDLE_TX
        if(IS_TX_CREDIT_FLOW_ENABLED(pEndpoint) &&
                HTC_ENABLE_BUNDLE(target) &&
                HTC_PACKET_QUEUE_DEPTH(pPktQueue) >= HTC_MIN_MSG_PER_BUNDLE){
            HTCIssuePacketsBundle(target, pEndpoint, pPktQueue);
        }
#endif
#endif
        /* if not bundling or there was a packet that could not be placed in a bundle,
         * and send it by normal way
         */
        pPacket = HTC_PACKET_DEQUEUE(pPktQueue);
        if(NULL == pPacket){
            /* local queue is fully drained */
            break;
        }

        netbuf = GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket);
        AR_DEBUG_ASSERT(netbuf);
        /* Non-credit enabled endpoints have been mapped and setup by now,
         * so no need to revisit the HTC headers
         */
        if (IS_TX_CREDIT_FLOW_ENABLED(pEndpoint)) {

            payloadLen = pPacket->ActualLength;
            /* setup HTC frame header */

            pHtcHdr = (HTC_FRAME_HDR *) adf_nbuf_get_frag_vaddr(netbuf, 0);
            AR_DEBUG_ASSERT(pHtcHdr);


            HTC_WRITE32(pHtcHdr, SM(payloadLen, HTC_FRAME_HDR_PAYLOADLEN) |
                    SM(pPacket->PktInfo.AsTx.SendFlags, HTC_FRAME_HDR_FLAGS) |
                    SM(pPacket->Endpoint, HTC_FRAME_HDR_ENDPOINTID));
            HTC_WRITE32(((A_UINT32 *)pHtcHdr) + 1,
                    SM(pPacket->PktInfo.AsTx.SeqNo, HTC_FRAME_HDR_CONTROLBYTES1));

            /*
             * Now that the HTC frame header has been added, the netbuf can be
             * mapped.  This only applies to non-data frames, since data frames
             * were already mapped as they entered into the driver.
             * Check the "FIXUP_NETBUF" flag to see whether this is a data netbuf
             * that is already mapped, or a non-data netbuf that needs to be
             * mapped.
             */
            if (pPacket->PktInfo.AsTx.Flags & HTC_TX_PACKET_FLAG_FIXUP_NETBUF) {
                adf_nbuf_map(
                        target->osdev,
                        GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket),
                        ADF_OS_DMA_TO_DEVICE);
            }
        }
        LOCK_HTC_TX(target);
            /* store in look up queue to match completions */
#ifdef ATH_11AC_TXCOMPACT
        if (HTT_DATA_MSG_SVC != pEndpoint->ServiceID)
#endif /* ATH_11AC_TXCOMPACT */
        {
            HTC_PACKET_ENQUEUE(&pEndpoint->TxLookupQueue,pPacket);
        }
        INC_HTC_EP_STAT(pEndpoint,TxIssued,1);
        pEndpoint->ul_outstanding_cnt++;
        UNLOCK_HTC_TX(target);

        status = HIFSend_head(target->hif_dev,
                              pEndpoint->UL_PipeID, pEndpoint->Id,
                              HTC_HDR_LENGTH + pPacket->ActualLength,
                              netbuf);
#if DEBUG_BUNDLE
        adf_os_print(" Send single EP%d buffer size:0x%x, total:0x%x.\n",
            pEndpoint->Id,
            pEndpoint->TxCreditSize,
            HTC_HDR_LENGTH + pPacket->ActualLength);
#endif

	target->CE_send_cnt++;

        if (adf_os_unlikely(A_FAILED(status))) {
            if (status != A_NO_RESOURCE) {
                /* TODO : if more than 1 endpoint maps to the same PipeID it is possible
                 * to run out of resources in the HIF layer. Don't emit the error */
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("HIFSend Failed status:%d \n",status));
            }
            LOCK_HTC_TX(target);
	    target->CE_send_cnt--;
            pEndpoint->ul_outstanding_cnt--;
            HTC_PACKET_REMOVE(&pEndpoint->TxLookupQueue,pPacket);
                /* reclaim credits */
#if defined(HIF_USB)
            if (pEndpoint->Id >= ENDPOINT_2 && pEndpoint->Id <= ENDPOINT_5)
                target->avail_tx_credits += pPacket->PktInfo.AsTx.CreditsUsed;
            else
                pEndpoint->TxCredits += pPacket->PktInfo.AsTx.CreditsUsed;
#else
            pEndpoint->TxCredits += pPacket->PktInfo.AsTx.CreditsUsed;
#endif
                /* put it back into the callers queue */
            HTC_PACKET_ENQUEUE_TO_HEAD(pPktQueue,pPacket);
            UNLOCK_HTC_TX(target);
            break;
        }

    }
    if (adf_os_unlikely(A_FAILED(status))) {
#if defined(HIF_USB)
        if (pEndpoint->Id >= ENDPOINT_2 && pEndpoint->Id <= ENDPOINT_5)
            target->avail_tx_credits += pPacket->PktInfo.AsTx.CreditsUsed;
        else
            pEndpoint->TxCredits += pPacket->PktInfo.AsTx.CreditsUsed;
#endif
        while (!HTC_QUEUE_EMPTY(pPktQueue)) {
            if (status != A_NO_RESOURCE) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("HTCIssuePackets, failed pkt:0x%p status:%d \n",pPacket,status));
            }
            pPacket = HTC_PACKET_DEQUEUE(pPktQueue);
	    if (pPacket) {
		    pPacket->Status = status;
		    SendPacketCompletion(target,pPacket);
	    }
        }
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND, ("-HTCIssuePackets \n"));

    return status;
}

#ifdef FEATURE_RUNTIME_PM
static void GetHTCAutoPmSendPackets(HTC_TARGET        *target,
                                    HTC_ENDPOINT      *pEndpoint,
                                    HTC_PACKET_QUEUE  *pQueue)
{
	HTC_PACKET   *pPacket;
	/*
	 * If the EP is not WMI EP then it is not required to go through
	 * the queue to find the tagged packkets.
	 */
	if (pEndpoint->ServiceID != WMI_CONTROL_SVC)
		return;

	ITERATE_OVER_LIST_ALLOW_REMOVE(&pEndpoint->TxQueue.QueueHead, pPacket,
			HTC_PACKET, ListLink) {

		if (pPacket->PktInfo.AsTx.Tag != HTC_TX_PACKET_TAG_AUTO_PM)
			continue;

		HTC_PACKET_REMOVE(&pEndpoint->TxQueue, pPacket);
		HTC_PACKET_ENQUEUE(pQueue, pPacket);
	} ITERATE_END
}

static void RequeueHTCAutoPmSendPackets(HTC_TARGET        *target,
                                        HTC_ENDPOINT      *pEndpoint,
                                        HTC_PACKET_QUEUE  *pQueue)
{
	if (pEndpoint->ServiceID != WMI_CONTROL_SVC)
		return;

	/*
	 * Go through remaining packets in the temporary queue and add them to
	 * head of the queue so that those would be sent at priority than other
	 * packets
	 */
	HTC_PACKET_QUEUE_TRANSFER_TO_HEAD(&pEndpoint->TxQueue, pQueue);
}
#else
static inline void GetHTCAutoPmSendPackets(HTC_TARGET        *target,
                                           HTC_ENDPOINT      *pEndpoint,
                                           HTC_PACKET_QUEUE  *pQueue)
{
	return;
}
static inline void RequeueHTCAutoPmSendPackets(HTC_TARGET        *target,
                                               HTC_ENDPOINT      *pEndpoint,
                                               HTC_PACKET_QUEUE  *pQueue)
{
	return;
}
#endif

    /* get HTC send packets from the TX queue on an endpoint, based on available credits */
void GetHTCSendPacketsCreditBased(HTC_TARGET        *target,
                                  HTC_ENDPOINT      *pEndpoint,
                                  HTC_PACKET_QUEUE  *pQueue)
{
    int          creditsRequired;
    int          remainder;
    A_UINT8      sendFlags;
    HTC_PACKET   *pPacket;
    unsigned int transferLength;
    HTC_PACKET_QUEUE *pTxQueue;
    HTC_PACKET_QUEUE tempQueue;
    A_BOOL AutoPMQueue = FALSE;

    /****** NOTE : the TX lock is held when this function is called *****************/
    AR_DEBUG_PRINTF(ATH_DEBUG_SEND,("+GetHTCSendPacketsCreditBased \n"));

    INIT_HTC_PACKET_QUEUE(&tempQueue);

    GetHTCAutoPmSendPackets(target, pEndpoint, &tempQueue);

    if (!HTC_QUEUE_EMPTY(&tempQueue)) {
        pTxQueue = &tempQueue;
        AutoPMQueue = TRUE;
    } else
        pTxQueue = &pEndpoint->TxQueue;

        /* loop until we can grab as many packets out of the queue as we can */
    while (TRUE) {

        sendFlags = 0;

        if (!AutoPMQueue && hif_pm_runtime_get(target->hif_dev)) {
           A_ASSERT(HTC_PACKET_QUEUE_DEPTH(pQueue) == 0);
           break;
        }

        /* get packet at head, but don't remove it */
        pPacket = HTC_GET_PKT_AT_HEAD(pTxQueue);
        if (pPacket == NULL) {
            if (!AutoPMQueue)
                hif_pm_runtime_put(target->hif_dev);
            break;
        }

        AR_DEBUG_PRINTF(ATH_DEBUG_SEND,(" Got head packet:%p , Queue Depth: %d\n",
                pPacket, HTC_PACKET_QUEUE_DEPTH(pTxQueue)));

        transferLength = pPacket->ActualLength + HTC_HDR_LENGTH;

        if (transferLength <= pEndpoint->TxCreditSize) {
            creditsRequired = 1;
        } else {
                /* figure out how many credits this message requires */
            creditsRequired = transferLength / pEndpoint->TxCreditSize;
            remainder = transferLength % pEndpoint->TxCreditSize;

            if (remainder) {
                creditsRequired++;
            }
        }

        AR_DEBUG_PRINTF(ATH_DEBUG_SEND,(" Credits Required:%d   Got:%d\n",
                            creditsRequired, pEndpoint->TxCredits));

        if (pEndpoint->Id == ENDPOINT_0) {
            /* endpoint 0 is special, it always has a credit and does not require credit based
             * flow control */
            creditsRequired = 0;
#if defined(HIF_USB)
        } else if (pEndpoint->Id >= ENDPOINT_2 && pEndpoint->Id <= ENDPOINT_5) {
            if (target->avail_tx_credits < creditsRequired)
                break;

            target->avail_tx_credits -= creditsRequired;

            if (target->avail_tx_credits < 9) {
                /* tell the target we need credits ASAP! */
                sendFlags |= HTC_FLAGS_NEED_CREDIT_UPDATE;
                INC_HTC_EP_STAT(pEndpoint, TxCreditLowIndications, 1);
                AR_DEBUG_PRINTF(ATH_DEBUG_SEND,(" Host Needs Credits  \n"));
            }
#endif
        } else {

            if (pEndpoint->TxCredits < creditsRequired) {
#if DEBUG_CREDIT
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR,(" EP%d, No Credit now. %d < %d\n",
                        pEndpoint->Id, pEndpoint->TxCredits, creditsRequired));
#endif
                if (!AutoPMQueue)
                   hif_pm_runtime_put(target->hif_dev);
                break;
            }

            pEndpoint->TxCredits -= creditsRequired;
            INC_HTC_EP_STAT(pEndpoint, TxCreditsConsummed, creditsRequired);

                /* check if we need credits back from the target */
            if (pEndpoint->TxCredits <= pEndpoint->TxCreditsPerMaxMsg) {
                /* tell the target we need credits ASAP! */
                sendFlags |= HTC_FLAGS_NEED_CREDIT_UPDATE;

                if (pEndpoint->ServiceID == WMI_CONTROL_SVC) {
                    LOCK_HTC_CREDIT(target);
                    htc_credit_record(HTC_REQUEST_CREDIT, pEndpoint->TxCredits,
                        HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue));
                    UNLOCK_HTC_CREDIT(target);
                }

                INC_HTC_EP_STAT(pEndpoint, TxCreditLowIndications, 1);
#if DEBUG_CREDIT
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR,(" EP%d Needs Credits\n", pEndpoint->Id));
#endif
            }
        }

            /* now we can fully dequeue */
        pPacket = HTC_PACKET_DEQUEUE(pTxQueue);
	if (pPacket) {
		/* save the number of credits this packet consumed */
		pPacket->PktInfo.AsTx.CreditsUsed = creditsRequired;
		/* save send flags */
		pPacket->PktInfo.AsTx.SendFlags = sendFlags;

		/* queue this packet into the caller's queue */
		HTC_PACKET_ENQUEUE(pQueue,pPacket);
	}
    }

    if (AutoPMQueue)
        RequeueHTCAutoPmSendPackets(target, pEndpoint, &tempQueue);

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND,("-GetHTCSendPacketsCreditBased \n"));

}

void GetHTCSendPackets(HTC_TARGET        *target,
                       HTC_ENDPOINT      *pEndpoint,
                       HTC_PACKET_QUEUE  *pQueue,
                       int                Resources)
{

    HTC_PACKET   *pPacket;
    HTC_PACKET_QUEUE *pTxQueue;
    HTC_PACKET_QUEUE tempQueue;
    A_BOOL AutoPMQueue = FALSE;

    /****** NOTE : the TX lock is held when this function is called *****************/
    AR_DEBUG_PRINTF(ATH_DEBUG_SEND,("+GetHTCSendPackets %d resources\n",Resources));

    INIT_HTC_PACKET_QUEUE(&tempQueue);

    GetHTCAutoPmSendPackets(target, pEndpoint, &tempQueue);

    if (!HTC_QUEUE_EMPTY(&tempQueue)) {
        pTxQueue = &tempQueue;
        AutoPMQueue = TRUE;
    } else
        pTxQueue = &pEndpoint->TxQueue;
        /* loop until we can grab as many packets out of the queue as we can */
    while (Resources > 0) {
        int num_frags;

        if (!AutoPMQueue && hif_pm_runtime_get(target->hif_dev)) {
           A_ASSERT(HTC_PACKET_QUEUE_DEPTH(pQueue) == 0);
           break;
        }

        pPacket = HTC_PACKET_DEQUEUE(pTxQueue);
        if (pPacket == NULL) {
            if (!AutoPMQueue)
               hif_pm_runtime_put(target->hif_dev);
            break;
        }
        AR_DEBUG_PRINTF(ATH_DEBUG_SEND,(" Got packet:%p , New Queue Depth: %d\n",
                pPacket, HTC_PACKET_QUEUE_DEPTH(pTxQueue)));
        /* For non-credit path the sequence number is already embedded
         * in the constructed HTC header
         */
#if 0
        pPacket->PktInfo.AsTx.SeqNo = pEndpoint->SeqNo;
        pEndpoint->SeqNo++;
#endif
        pPacket->PktInfo.AsTx.SendFlags = 0;
        pPacket->PktInfo.AsTx.CreditsUsed = 0;
        /* queue this packet into the caller's queue */
        HTC_PACKET_ENQUEUE(pQueue,pPacket);

        /*
         * FIX THIS:
         * For now, avoid calling adf_nbuf_get_num_frags before calling
         * adf_nbuf_map, because the MacOS version of adf_nbuf_t doesn't
         * support adf_nbuf_get_num_frags until after adf_nbuf_map has
         * been done.
         * Assume that the non-data netbufs, i.e. the WMI message netbufs,
         * consist of a single fragment.
         */
        num_frags =
            (pPacket->PktInfo.AsTx.Flags & HTC_TX_PACKET_FLAG_FIXUP_NETBUF) ?
            1 /* WMI messages are in a single-fragment network buffer */ :
            adf_nbuf_get_num_frags(GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket));
        Resources -= num_frags;
    }

    if (AutoPMQueue)
        RequeueHTCAutoPmSendPackets(target, pEndpoint, &tempQueue);

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND,("-GetHTCSendPackets \n"));

}

static HTC_SEND_QUEUE_RESULT HTCTrySend(HTC_TARGET       *target,
                                        HTC_ENDPOINT     *pEndpoint,
                                        HTC_PACKET_QUEUE *pCallersSendQueue)
{
    HTC_PACKET_QUEUE      sendQueue; /* temp queue to hold packets at various stages */
    HTC_PACKET            *pPacket;
    int                   tx_resources;
    int                   overflow;
    HTC_SEND_QUEUE_RESULT result = HTC_SEND_QUEUE_OK;

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND,("+HTCTrySend (Queue:%p Depth:%d)\n",
            pCallersSendQueue,
            (pCallersSendQueue == NULL) ? 0 : HTC_PACKET_QUEUE_DEPTH(pCallersSendQueue)));

        /* init the local send queue */
    INIT_HTC_PACKET_QUEUE(&sendQueue);

    do {

        if (NULL == pCallersSendQueue) {
                /* caller didn't provide a queue, just wants us to check queues and send */
            break;
        }

        if (HTC_QUEUE_EMPTY(pCallersSendQueue)) {
                /* empty queue */
            OL_ATH_HTC_PKT_ERROR_COUNT_INCR(target,HTC_PKT_Q_EMPTY);
            result = HTC_SEND_QUEUE_DROP;
            break;
        }

        if (HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue) >= pEndpoint->MaxTxQueueDepth) {
                    /* we've already overflowed */
            overflow = HTC_PACKET_QUEUE_DEPTH(pCallersSendQueue);
        } else {
                /* figure out how much we will overflow by */
            overflow = HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue);
            overflow += HTC_PACKET_QUEUE_DEPTH(pCallersSendQueue);
                /* figure out how much we will overflow the TX queue by */
            overflow -= pEndpoint->MaxTxQueueDepth;
        }

            /* if overflow is negative or zero, we are okay */
        if (overflow > 0) {
            AR_DEBUG_PRINTF(ATH_DEBUG_SEND,
                (" Endpoint %d, TX queue will overflow :%d , Tx Depth:%d, Max:%d \n",
                pEndpoint->Id, overflow, HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue), pEndpoint->MaxTxQueueDepth));
        }
        if ((overflow <= 0) || (pEndpoint->EpCallBacks.EpSendFull == NULL)) {
                /* all packets will fit or caller did not provide send full indication handler
                 * --  just move all of them to the local sendQueue object */
		HTC_PACKET_QUEUE_TRANSFER_TO_TAIL(&sendQueue, pCallersSendQueue);
        } else {
            int               i;
            int               goodPkts = HTC_PACKET_QUEUE_DEPTH(pCallersSendQueue) - overflow;

            A_ASSERT(goodPkts >= 0);
                /* we have overflowed, and a callback is provided */
                /* dequeue all non-overflow packets into the sendqueue */
            for (i = 0; i < goodPkts; i++) {
                    /* pop off caller's queue*/
                pPacket = HTC_PACKET_DEQUEUE(pCallersSendQueue);
                A_ASSERT(pPacket != NULL);
                    /* insert into local queue */
                HTC_PACKET_ENQUEUE(&sendQueue,pPacket);
            }

                /* the caller's queue has all the packets that won't fit*/
                /* walk through the caller's queue and indicate each one to the send full handler */
            ITERATE_OVER_LIST_ALLOW_REMOVE(&pCallersSendQueue->QueueHead, pPacket, HTC_PACKET, ListLink) {

                AR_DEBUG_PRINTF(ATH_DEBUG_SEND, (" Indicating overflowed TX packet: %p \n",
                                pPacket));
                /*
                 * Remove headroom reserved for HTC_FRAME_HDR before giving
                 * the packet back to the user via the EpSendFull callback.
                 */
                RestoreTxPacket(target, pPacket);

                if (pEndpoint->EpCallBacks.EpSendFull(pEndpoint->EpCallBacks.pContext,
                                                      pPacket) == HTC_SEND_FULL_DROP) {
                        /* callback wants the packet dropped */
                    INC_HTC_EP_STAT(pEndpoint, TxDropped, 1);
                        /* leave this one in the caller's queue for cleanup */
                } else {
                        /* callback wants to keep this packet, remove from caller's queue */
                    HTC_PACKET_REMOVE(pCallersSendQueue, pPacket);
                        /* put it in the send queue */
                    /* add HTC_FRAME_HDR space reservation again */
                    adf_nbuf_push_head(
                        GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket),
                        sizeof(HTC_FRAME_HDR));

                    HTC_PACKET_ENQUEUE(&sendQueue,pPacket);
                }

            } ITERATE_END;

            if (HTC_QUEUE_EMPTY(&sendQueue)) {
                    /* no packets made it in, caller will cleanup */
                OL_ATH_HTC_PKT_ERROR_COUNT_INCR(target,HTC_SEND_Q_EMPTY);
                result = HTC_SEND_QUEUE_DROP;
                break;
            }
        }

    } while (FALSE);

    if (result != HTC_SEND_QUEUE_OK) {
        AR_DEBUG_PRINTF(ATH_DEBUG_SEND,("-HTCTrySend:  \n"));
        return result;
    }

    if (!IS_TX_CREDIT_FLOW_ENABLED(pEndpoint)) {
        tx_resources = HIFGetFreeQueueNumber(target->hif_dev,pEndpoint->UL_PipeID);
    } else {
        tx_resources = 0;
    }

    LOCK_HTC_TX(target);

    if (!HTC_QUEUE_EMPTY(&sendQueue)) {
            /* transfer packets to tail */
        HTC_PACKET_QUEUE_TRANSFER_TO_TAIL(&pEndpoint->TxQueue,&sendQueue);
        A_ASSERT(HTC_QUEUE_EMPTY(&sendQueue));
        INIT_HTC_PACKET_QUEUE(&sendQueue);
    }

        /* increment tx processing count on entry */
    adf_os_atomic_inc(&pEndpoint->TxProcessCount);
    if (adf_os_atomic_read(&pEndpoint->TxProcessCount) > 1) {
            /* another thread or task is draining the TX queues on this endpoint
             * that thread will reset the tx processing count when the queue is drained */
        adf_os_atomic_dec(&pEndpoint->TxProcessCount);
        UNLOCK_HTC_TX(target);
        AR_DEBUG_PRINTF(ATH_DEBUG_SEND,("-HTCTrySend (busy) \n"));
        return HTC_SEND_QUEUE_OK;
    }

    /***** beyond this point only 1 thread may enter ******/

        /* now drain the endpoint TX queue for transmission as long as we have enough
         * transmit resources */
    while (TRUE) {

        if (HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue) == 0) {
            break;
        }

        if (IS_TX_CREDIT_FLOW_ENABLED(pEndpoint)) {
#if DEBUG_CREDIT
            int cred = pEndpoint->TxCredits;
#endif
            /* credit based mechanism provides flow control based on target transmit resource availability, we
             * assume that the HIF layer will always have bus resources greater than target transmit resources */
            GetHTCSendPacketsCreditBased(target,pEndpoint,&sendQueue);
#if DEBUG_CREDIT
        if (ep_debug_mask & (1 << pEndpoint->Id)){
            if (cred - pEndpoint->TxCredits > 0){
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR,(" <HTC> Decrease EP%d %d - %d = %d credits.\n",
                        pEndpoint->Id, cred, cred - pEndpoint->TxCredits, pEndpoint->TxCredits));
            }
        }
#endif
        } else {
                /* get all the packets for this endpoint that we can for this pass */
            GetHTCSendPackets(target,pEndpoint,&sendQueue,tx_resources);
        }

        if (HTC_PACKET_QUEUE_DEPTH(&sendQueue) == 0) {
                /* didn't get any packets due to a lack of resources or TX queue was drained */
            break;
        }

        UNLOCK_HTC_TX(target);

            /* send what we can */
        HTCIssuePackets(target,pEndpoint,&sendQueue);

        if (!IS_TX_CREDIT_FLOW_ENABLED(pEndpoint)) {
            tx_resources = HIFGetFreeQueueNumber(target->hif_dev,pEndpoint->UL_PipeID);
        }

        LOCK_HTC_TX(target);

    }

        /* done with this endpoint, we can clear the count */
    adf_os_atomic_init(&pEndpoint->TxProcessCount);
    UNLOCK_HTC_TX(target);

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND,("-HTCTrySend:  \n"));

    return HTC_SEND_QUEUE_OK;
}

#ifdef USB_HIF_SINGLE_PIPE_DATA_SCHED
static A_UINT16 HTCSendPktsSchedCheck(HTC_HANDLE HTCHandle, HTC_ENDPOINT_ID id)
{
    HTC_TARGET        *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);
    HTC_ENDPOINT      *pEndpoint;
    HTC_ENDPOINT_ID   eid;
    HTC_PACKET_QUEUE  *pTxQueue;
    A_UINT16          resources;
    A_UINT16          acQueueStatus[DATA_EP_SIZE] = {0, 0, 0, 0};

    if (id < ENDPOINT_2 || id > ENDPOINT_5) {
        return 1;
    }

    for (eid = ENDPOINT_2; eid <= ENDPOINT_5; eid++) {
        pEndpoint = &target->EndPoint[eid];
        pTxQueue = &pEndpoint->TxQueue;

        if (HTC_QUEUE_EMPTY(pTxQueue)) {
            acQueueStatus[eid - 2] = 1;
        }
    }

    switch (id)
    {
        case ENDPOINT_2: //BE
            return (acQueueStatus[0] && acQueueStatus[2] && acQueueStatus[3]);
        case ENDPOINT_3: //BK
            return (acQueueStatus[0] && acQueueStatus[1] && acQueueStatus[2] && acQueueStatus[3]);
        case ENDPOINT_4: //VI
            return (acQueueStatus[2] && acQueueStatus[3]);
        case ENDPOINT_5: //VO
            return (acQueueStatus[3]);
        default:
            return 0;
    }

}

static A_STATUS HTCSendPktsSchedQueue(HTC_TARGET *target, HTC_PACKET_QUEUE *pPktQueue, HTC_ENDPOINT_ID eid)
{
    HTC_ENDPOINT      *pEndpoint;
    HTC_PACKET_QUEUE  *pTxQueue;
    HTC_PACKET        *pPacket;
    int               goodPkts;

    pEndpoint = &target->EndPoint[eid];
    pTxQueue = &pEndpoint->TxQueue;

    LOCK_HTC_TX(target);

    goodPkts = pEndpoint->MaxTxQueueDepth - HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue);

    if (goodPkts > 0){
        while (!HTC_QUEUE_EMPTY(pPktQueue)) {
            pPacket = HTC_PACKET_DEQUEUE(pPktQueue);
            HTC_PACKET_ENQUEUE(pTxQueue, pPacket);
            goodPkts--;

            if (goodPkts <= 0) {
                break;
            }
        }
    }

    if (HTC_PACKET_QUEUE_DEPTH(pPktQueue)) {
        ITERATE_OVER_LIST_ALLOW_REMOVE(&pPktQueue->QueueHead, pPacket, HTC_PACKET, ListLink) {

            if (pEndpoint->EpCallBacks.EpSendFull(pEndpoint->EpCallBacks.pContext,
                                                    pPacket) == HTC_SEND_FULL_DROP) {
                INC_HTC_EP_STAT(pEndpoint, TxDropped, 1);
            } else {
                HTC_PACKET_REMOVE(pPktQueue, pPacket);
                HTC_PACKET_ENQUEUE(pTxQueue,pPacket);
            }
        } ITERATE_END;
    }

    UNLOCK_HTC_TX(target);

    return A_OK;
}

#endif

A_STATUS HTCSendPktsMultiple(HTC_HANDLE HTCHandle, HTC_PACKET_QUEUE *pPktQueue)
{
    HTC_TARGET      *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);
    HTC_ENDPOINT    *pEndpoint;
    HTC_PACKET      *pPacket;
    adf_nbuf_t      netbuf;
    HTC_FRAME_HDR       *pHtcHdr;

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND, ("+HTCSendPktsMultiple: Queue: %p, Pkts %d \n",
                    pPktQueue, HTC_PACKET_QUEUE_DEPTH(pPktQueue)));

        /* get packet at head to figure out which endpoint these packets will go into */
    pPacket = HTC_GET_PKT_AT_HEAD(pPktQueue);
    if (NULL == pPacket) {
        OL_ATH_HTC_PKT_ERROR_COUNT_INCR(target,GET_HTC_PKT_Q_FAIL);
        AR_DEBUG_PRINTF(ATH_DEBUG_SEND, ("-HTCSendPktsMultiple \n"));
        return A_EINVAL;
    }

    AR_DEBUG_ASSERT(pPacket->Endpoint < ENDPOINT_MAX);
    pEndpoint = &target->EndPoint[pPacket->Endpoint];

    if (!pEndpoint->ServiceID) {
       AR_DEBUG_PRINTF(ATH_DEBUG_SEND, ("%s: ServiceID is invalid\n",
                                                 __func__));
       return A_EINVAL;
    }

#ifdef HTC_EP_STAT_PROFILING
    LOCK_HTC_TX(target);
    INC_HTC_EP_STAT(pEndpoint,TxPosted,HTC_PACKET_QUEUE_DEPTH(pPktQueue));
    UNLOCK_HTC_TX(target);
#endif

    /* provide room in each packet's netbuf for the HTC frame header */
    HTC_PACKET_QUEUE_ITERATE_ALLOW_REMOVE(pPktQueue,pPacket) {
        netbuf = GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket);
        AR_DEBUG_ASSERT(netbuf);

        adf_nbuf_push_head(netbuf, sizeof(HTC_FRAME_HDR));
        /* setup HTC frame header */
        pHtcHdr = (HTC_FRAME_HDR *) adf_nbuf_get_frag_vaddr(netbuf, 0);
        AR_DEBUG_ASSERT(pHtcHdr);
        HTC_WRITE32(pHtcHdr, SM(pPacket->ActualLength, HTC_FRAME_HDR_PAYLOADLEN) |
                SM(pPacket->Endpoint, HTC_FRAME_HDR_ENDPOINTID));

            LOCK_HTC_TX(target);

            pPacket->PktInfo.AsTx.SeqNo = pEndpoint->SeqNo;
            pEndpoint->SeqNo++;

            HTC_WRITE32(((A_UINT32 *)pHtcHdr) + 1,
                    SM(pPacket->PktInfo.AsTx.SeqNo, HTC_FRAME_HDR_CONTROLBYTES1));

            UNLOCK_HTC_TX(target);
            /*
             * Now that the HTC frame header has been added, the netbuf can be
             * mapped.  This only applies to non-data frames, since data frames
             * were already mapped as they entered into the driver.
             */
            adf_nbuf_map(
                    target->osdev,
                    GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket),
                    ADF_OS_DMA_TO_DEVICE);

	pPacket->PktInfo.AsTx.Flags |= HTC_TX_PACKET_FLAG_FIXUP_NETBUF;
    } HTC_PACKET_QUEUE_ITERATE_END;

#ifdef USB_HIF_SINGLE_PIPE_DATA_SCHED
    if (!HTCSendPktsSchedCheck(HTCHandle, pEndpoint->Id)) {
        HTCSendPktsSchedQueue(HTCHandle, pPktQueue, pEndpoint->Id);
    } else {
        HTCTrySend(target,pEndpoint,pPktQueue);
    }
#else
    HTCTrySend(target,pEndpoint,pPktQueue);
#endif

        /* do completion on any packets that couldn't get in */
    if (!HTC_QUEUE_EMPTY(pPktQueue)) {

        HTC_PACKET_QUEUE_ITERATE_ALLOW_REMOVE(pPktQueue,pPacket) {
            /* remove the headroom reserved for HTC_FRAME_HDR */
            RestoreTxPacket(target, pPacket);

            if (HTC_STOPPING(target)) {
                pPacket->Status = A_ECANCELED;
            } else {
                pPacket->Status = A_NO_RESOURCE;
            }
        } HTC_PACKET_QUEUE_ITERATE_END;

        DoSendCompletion(pEndpoint,pPktQueue);
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND, ("-HTCSendPktsMultiple \n"));

    return A_OK;
}

/* HTC API - HTCSendPkt */
A_STATUS    HTCSendPkt(HTC_HANDLE HTCHandle, HTC_PACKET *pPacket)
{
    HTC_PACKET_QUEUE queue;

    if (HTCHandle == NULL || pPacket == NULL) {
        return A_ERROR;
    }
    a_mem_trace(GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket));
    AR_DEBUG_PRINTF(ATH_DEBUG_SEND,
                    ("+-HTCSendPkt: Enter endPointId: %d, buffer: %p, length: %d \n",
                    pPacket->Endpoint, pPacket->pBuffer, pPacket->ActualLength));
    INIT_HTC_PACKET_QUEUE_AND_ADD(&queue,pPacket);
    return HTCSendPktsMultiple(HTCHandle, &queue);
}

#ifdef ATH_11AC_TXCOMPACT

A_STATUS HTCSendDataPkt(HTC_HANDLE HTCHandle, adf_nbuf_t       netbuf, int Epid, int ActualLength)
{
    HTC_TARGET       *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);
    HTC_ENDPOINT     *pEndpoint;
    HTC_FRAME_HDR    *pHtcHdr;
    A_STATUS         status = A_OK;
    int              tx_resources;

    pEndpoint = &target->EndPoint[Epid];

    tx_resources = HIFGetFreeQueueNumber(target->hif_dev, pEndpoint->UL_PipeID);

    if(tx_resources <  HTC_DATA_RESOURCE_THRS){
        if (pEndpoint->ul_is_polled){
            HIFSendCompleteCheck(
                    pEndpoint->target->hif_dev, pEndpoint->UL_PipeID, 1);
            tx_resources = HIFGetFreeQueueNumber(target->hif_dev, pEndpoint->UL_PipeID);
        }
        if(tx_resources < HTC_DATA_MINDESC_PERPACKET){
            return A_ERROR;
        }
    }

    if (hif_pm_runtime_get(target->hif_dev))
        return A_ERROR;

    pHtcHdr = (HTC_FRAME_HDR *) adf_nbuf_get_frag_vaddr(netbuf, 0);
    AR_DEBUG_ASSERT(pHtcHdr);

    HTC_WRITE32(pHtcHdr, SM(ActualLength, HTC_FRAME_HDR_PAYLOADLEN) |
			 SM(Epid, HTC_FRAME_HDR_ENDPOINTID));
    /*
     * If the HIF pipe for the data endpoint is polled rather than
     * interrupt-driven, this is a good point to check whether any
     * data previously sent through the HIF pipe have finished being
     * sent.
     * Since this may result in callbacks to HTCTxCompletionHandler,
     * which can take the HTC tx lock, make the HIFSendCompleteCheck
     * call before acquiring the HTC tx lock.
     * Call HIFSendCompleteCheck directly, rather than calling
     * HTCSendCompleteCheck, and call the PollTimerStart separately
     * after calling HIFSend_head, so the timer will be started to
     * check for completion of the new outstanding download (in the
     * unexpected event that other polling calls don't catch it).
     */

    LOCK_HTC_TX(target);

    HTC_WRITE32(((A_UINT32 *)pHtcHdr) + 1, SM(pEndpoint->SeqNo, HTC_FRAME_HDR_CONTROLBYTES1));

    pEndpoint->SeqNo++;

    status = HIFSend_head(target->hif_dev,
            pEndpoint->UL_PipeID,
            pEndpoint->Id,
            ActualLength,
            netbuf);


    UNLOCK_HTC_TX(target);
    return status ;
}
#else /*ATH_11AC_TXCOMPACT*/

A_STATUS HTCSendDataPkt(HTC_HANDLE HTCHandle, HTC_PACKET *pPacket,
                        A_UINT8 more_data)
{
    HTC_TARGET       *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);
    HTC_ENDPOINT     *pEndpoint;
    HTC_FRAME_HDR    *pHtcHdr;
    HTC_PACKET_QUEUE sendQueue;
    adf_nbuf_t       netbuf;
    int              tx_resources;
    A_STATUS         status = A_OK;
    if (pPacket){
        AR_DEBUG_ASSERT(pPacket->Endpoint < ENDPOINT_MAX);
        pEndpoint = &target->EndPoint[pPacket->Endpoint];

        /* add HTC_FRAME_HDR in the initial fragment */
        netbuf = GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket);
        pHtcHdr = (HTC_FRAME_HDR *) adf_nbuf_get_frag_vaddr(netbuf, 0);
        AR_DEBUG_ASSERT(pHtcHdr);

        HTC_WRITE32(pHtcHdr, SM(pPacket->ActualLength, HTC_FRAME_HDR_PAYLOADLEN) |
                 SM(pPacket->PktInfo.AsTx.SendFlags, HTC_FRAME_HDR_FLAGS) |
                 SM(pPacket->Endpoint, HTC_FRAME_HDR_ENDPOINTID));
        /*
         * If the HIF pipe for the data endpoint is polled rather than
         * interrupt-driven, this is a good point to check whether any
         * data previously sent through the HIF pipe have finished being
         * sent.
         * Since this may result in callbacks to HTCTxCompletionHandler,
         * which can take the HTC tx lock, make the HIFSendCompleteCheck
         * call before acquiring the HTC tx lock.
         * Call HIFSendCompleteCheck directly, rather than calling
         * HTCSendCompleteCheck, and call the PollTimerStart separately
         * after calling HIFSend_head, so the timer will be started to
         * check for completion of the new outstanding download (in the
         * unexpected event that other polling calls don't catch it).
         */
        if (pEndpoint->ul_is_polled) {
            HTCSendCompletePollTimerStop(pEndpoint);
            HIFSendCompleteCheck(
                pEndpoint->target->hif_dev, pEndpoint->UL_PipeID, 0);
        }

        LOCK_HTC_TX(target);

        pPacket->PktInfo.AsTx.SeqNo = pEndpoint->SeqNo;
        pEndpoint->SeqNo++;


        HTC_WRITE32(((A_UINT32 *)pHtcHdr) + 1, SM(pPacket->PktInfo.AsTx.SeqNo, HTC_FRAME_HDR_CONTROLBYTES1));

        /* append new packet to pEndpoint->TxQueue */
        HTC_PACKET_ENQUEUE(&pEndpoint->TxQueue, pPacket);
#ifdef ENABLE_BUNDLE_TX
        if (HTC_ENABLE_BUNDLE(target) && (more_data)) {
            UNLOCK_HTC_TX(target);
            return A_OK;
        }
#endif
    } else {
        LOCK_HTC_TX(target);
        pEndpoint = &target->EndPoint[1];
    }

    /* increment tx processing count on entry */
    adf_os_atomic_inc(&pEndpoint->TxProcessCount);
    if (adf_os_atomic_read(&pEndpoint->TxProcessCount) > 1) {
        /*
         * Another thread or task is draining the TX queues on this endpoint.
         * That thread will reset the tx processing count when the queue is
         * drained.
         */
        adf_os_atomic_dec(&pEndpoint->TxProcessCount);
        UNLOCK_HTC_TX(target);
        return A_OK;
    }

    /***** beyond this point only 1 thread may enter ******/

    INIT_HTC_PACKET_QUEUE(&sendQueue);
    if (IS_TX_CREDIT_FLOW_ENABLED(pEndpoint)) {
#if DEBUG_CREDIT
        int cred = pEndpoint->TxCredits;
#endif
        GetHTCSendPacketsCreditBased(target, pEndpoint, &sendQueue);
#if DEBUG_CREDIT
        if (ep_debug_mask & (1 << pEndpoint->Id)){
            if (cred - pEndpoint->TxCredits > 0){
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR,(" <HTC> Decrease EP%d %d - %d = %d credits.\n",
                        pEndpoint->Id, cred, cred - pEndpoint->TxCredits, pEndpoint->TxCredits));
            }
        }
#endif
        UNLOCK_HTC_TX(target);
    }
#ifdef ENABLE_BUNDLE_TX
    else if (HTC_ENABLE_BUNDLE(target)) {
        /* Dequeue max packets from endpoint tx queue */
        GetHTCSendPackets(target, pEndpoint, &sendQueue,
                          HTC_MAX_TX_BUNDLE_SEND_LIMIT);
        UNLOCK_HTC_TX(target);
    }
#endif
    else {
        /*
         * Now drain the endpoint TX queue for transmission as long as we have
         * enough transmit resources
         */
        tx_resources = HIFGetFreeQueueNumber(target->hif_dev,pEndpoint->UL_PipeID);
        GetHTCSendPackets(target, pEndpoint, &sendQueue, tx_resources);
        UNLOCK_HTC_TX(target);
    }
    /* send what we can */
    while (TRUE) {
#if defined(HIF_USB) || defined(HIF_SDIO)
#ifdef ENABLE_BUNDLE_TX
        if (HTC_ENABLE_BUNDLE(target) &&
           HTC_PACKET_QUEUE_DEPTH(&sendQueue) >= HTC_MIN_MSG_PER_BUNDLE) {
            HTCIssuePacketsBundle(target, pEndpoint, &sendQueue);
        }
#endif
#endif
        pPacket = HTC_PACKET_DEQUEUE(&sendQueue);
        if (pPacket == NULL){
            break;
        }
        netbuf = GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket);

        LOCK_HTC_TX(target);
        /* store in look up queue to match completions */
        HTC_PACKET_ENQUEUE(&pEndpoint->TxLookupQueue,pPacket);
        INC_HTC_EP_STAT(pEndpoint,TxIssued,1);
        pEndpoint->ul_outstanding_cnt++;
        UNLOCK_HTC_TX(target);

        status = HIFSend_head(target->hif_dev,
                              pEndpoint->UL_PipeID,
                              pEndpoint->Id,
                              HTC_HDR_LENGTH + pPacket->ActualLength,
                              netbuf);
#if DEBUG_BUNDLE
        adf_os_print(" Send single EP%d buffer size:0x%x, total:0x%x.\n",
            pEndpoint->Id,
            pEndpoint->TxCreditSize,
            HTC_HDR_LENGTH + pPacket->ActualLength);
#endif

        if (adf_os_unlikely(A_FAILED(status))) {
            LOCK_HTC_TX(target);
            pEndpoint->ul_outstanding_cnt--;
            /* remove this packet from the tx completion queue */
            HTC_PACKET_REMOVE(&pEndpoint->TxLookupQueue,pPacket);

            /*
             * Don't bother reclaiming credits - HTC flow control
             * is not applicable to tx data.
             * In LL systems, there is no download flow control,
             * since there's virtually no download delay.
             * In HL systems, the txrx SW explicitly performs the
             * tx flow control.
             */
            //pEndpoint->TxCredits += pPacket->PktInfo.AsTx.CreditsUsed;

            /* put this frame back at the front of the sendQueue */
            HTC_PACKET_ENQUEUE_TO_HEAD(&sendQueue, pPacket);

            /* put the sendQueue back at the front of pEndpoint->TxQueue */
            HTC_PACKET_QUEUE_TRANSFER_TO_HEAD(&pEndpoint->TxQueue, &sendQueue);
            UNLOCK_HTC_TX(target);
            break; /* still need to reset TxProcessCount */
        }
    }
    /* done with this endpoint, we can clear the count */
    adf_os_atomic_init(&pEndpoint->TxProcessCount);

    if (pEndpoint->ul_is_polled) {
        /*
         * Start a cleanup timer to poll for download completion.
         * The download completion should be noticed promptly from
         * other polling calls, but the timer provides a safety net
         * in case other polling calls don't occur as expected.
         */
        HTCSendCompletePollTimerStart(pEndpoint);
    }

    return status;
}
#endif /*ATH_11AC_TXCOMPACT*/

/*
 * In the adapted HIF layer, adf_nbuf_t are passed between HIF and HTC, since upper layers expects
 * HTC_PACKET containers we use the completed netbuf and lookup its corresponding HTC packet buffer
 * from a lookup list.
 * This is extra overhead that can be fixed by re-aligning HIF interfaces with HTC.
 *
 */
static HTC_PACKET *HTCLookupTxPacket(HTC_TARGET *target, HTC_ENDPOINT *pEndpoint, adf_nbuf_t netbuf)
{
    HTC_PACKET *pPacket = NULL;
    HTC_PACKET *pFoundPacket = NULL;
    HTC_PACKET_QUEUE lookupQueue;

    INIT_HTC_PACKET_QUEUE(&lookupQueue);
    LOCK_HTC_TX(target);

    /* mark that HIF has indicated the send complete for another packet */
    pEndpoint->ul_outstanding_cnt--;

    /* Dequeue first packet directly because of in-order completion */
    pPacket = HTC_PACKET_DEQUEUE(&pEndpoint->TxLookupQueue);
    if (adf_os_unlikely(!pPacket)) {
        UNLOCK_HTC_TX(target);
        return NULL;
    }
    if (netbuf == (adf_nbuf_t)GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket)) {
        UNLOCK_HTC_TX(target);
        return pPacket;
    } else {
        HTC_PACKET_ENQUEUE(&lookupQueue, pPacket);
    }

    /*
     * Move TX lookup queue to temp queue because most of packets that are not index 0
     * are not top 10 packets.
     */
    HTC_PACKET_QUEUE_TRANSFER_TO_TAIL(&lookupQueue, &pEndpoint->TxLookupQueue);
    UNLOCK_HTC_TX(target);

    ITERATE_OVER_LIST_ALLOW_REMOVE(&lookupQueue.QueueHead,pPacket,HTC_PACKET,ListLink) {

        if (NULL == pPacket){
            pFoundPacket = pPacket;
            break;
        }
            /* check for removal */
        if (netbuf == (adf_nbuf_t)GET_HTC_PACKET_NET_BUF_CONTEXT(pPacket)) {
                /* found it */
            HTC_PACKET_REMOVE(&lookupQueue, pPacket);
            pFoundPacket = pPacket;
            break;
        }

    } ITERATE_END;

    LOCK_HTC_TX(target);
    HTC_PACKET_QUEUE_TRANSFER_TO_HEAD(&pEndpoint->TxLookupQueue, &lookupQueue);
    UNLOCK_HTC_TX(target);

    return pFoundPacket;
}


A_STATUS HTCTxCompletionHandler(void *Context,
                                adf_nbuf_t netbuf,
                                unsigned int EpID)
{
    HTC_TARGET      *target = (HTC_TARGET *)Context;
    HTC_ENDPOINT    *pEndpoint;
    HTC_PACKET      *pPacket;
#ifdef USB_HIF_SINGLE_PIPE_DATA_SCHED
    HTC_ENDPOINT_ID eid[DATA_EP_SIZE] = {ENDPOINT_5, ENDPOINT_4, ENDPOINT_2, ENDPOINT_3};
    int epidIdx;
    A_UINT16 resourcesThresh[DATA_EP_SIZE]; //urb resources
    A_UINT16 resources;
    A_UINT16 resourcesMax;
#endif

    pEndpoint = &target->EndPoint[EpID];
    target->TX_comp_cnt++;

    do {
        pPacket = HTCLookupTxPacket(target, pEndpoint, netbuf);
        if (NULL == pPacket) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("HTC TX lookup failed!\n"));
                /* may have already been flushed and freed */
            netbuf = NULL;
            break;
        }
        a_mem_trace(netbuf);

        if (pPacket->PktInfo.AsTx.Tag != HTC_TX_PACKET_TAG_AUTO_PM)
           hif_pm_runtime_put(target->hif_dev);

        if (pPacket->PktInfo.AsTx.Tag == HTC_TX_PACKET_TAG_BUNDLED){
            HTC_PACKET      *pPacketTemp;
            HTC_PACKET_QUEUE *pQueueSave = (HTC_PACKET_QUEUE *)pPacket->pContext;
            HTC_PACKET_QUEUE_ITERATE_ALLOW_REMOVE(pQueueSave, pPacketTemp){
                pPacket->Status = A_OK;
                SendPacketCompletion(target,pPacketTemp);
            }HTC_PACKET_QUEUE_ITERATE_END;
            FreeHTCBundlePacket(target, pPacket);
            return A_OK;
        }
            /* will be giving this buffer back to upper layers */
        netbuf = NULL;
        pPacket->Status = A_OK;
        SendPacketCompletion(target,pPacket);

    } while (FALSE);

    if (!IS_TX_CREDIT_FLOW_ENABLED(pEndpoint)) {
#ifdef USB_HIF_SINGLE_PIPE_DATA_SCHED
        if ((HTC_ENDPOINT_ID)EpID >= ENDPOINT_2 &&
            (HTC_ENDPOINT_ID)EpID <= ENDPOINT_5) {

            resourcesMax = HIFGetMaxQueueNumber(target->hif_dev, pEndpoint->UL_PipeID);

            resourcesThresh[0] = 0;                           //VO
            resourcesThresh[1] = (resourcesMax *  2) / 32;    //VI
            resourcesThresh[2] = (resourcesMax *  3) / 32;    //BE
            resourcesThresh[3] = (resourcesMax *  4) / 32;    //BK

            resources = HIFGetFreeQueueNumber(target->hif_dev, pEndpoint->UL_PipeID);

            for (epidIdx = 0; epidIdx < DATA_EP_SIZE; epidIdx++) {

                pEndpoint = &target->EndPoint[eid[epidIdx]];

                if (!HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue)) {
                    continue;
                }

                if (resources >= resourcesThresh[epidIdx]) {
                    break;
                }
            }

            if (epidIdx == DATA_EP_SIZE) {
                #if 0
                if (resources) {
                    for (epidIdx = 0; epidIdx < DATA_EP_SIZE; epidIdx++) {

                        pEndpoint = &target->EndPoint[eid[epidIdx]];

                        if (HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue)) {
                            break;
                        }
                    }
                } else
                #endif
                return A_OK;
            }
        }
#endif

        /* note: when using TX credit flow, the re-checking of queues happens
         * when credits flow back from the target.
         * in the non-TX credit case, we recheck after the packet completes */
        HTCTrySend(target,pEndpoint,NULL);
    }

    return A_OK;
}

    /* callback when TX resources become available */
void  HTCTxResourceAvailHandler(void *context, A_UINT8 pipeID)
{
    int             i;
    HTC_TARGET      *target = (HTC_TARGET *)context;
    HTC_ENDPOINT    *pEndpoint = NULL;

    for (i = 0; i < ENDPOINT_MAX; i++) {
        pEndpoint = &target->EndPoint[i];
        if (pEndpoint->ServiceID != 0) {
            if (pEndpoint->UL_PipeID == pipeID) {
                break;
            }
        }
    }

    if (i >= ENDPOINT_MAX) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("Invalid pipe indicated for TX resource avail : %d!\n",pipeID));
        return;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND,("HIF indicated more resources for pipe:%d \n",pipeID));

    HTCTrySend(target,pEndpoint,NULL);
}

void HTCTxResumeAllHandler(void *context)
{
	int             i;
	HTC_TARGET      *target = (HTC_TARGET *)context;
	HTC_ENDPOINT    *pEndpoint = NULL;

	for (i = 0; i < ENDPOINT_MAX; i++) {
		pEndpoint = &target->EndPoint[i];

		if (pEndpoint->ServiceID == 0)
			continue;

		if (pEndpoint->EpCallBacks.EpResumeTxQueue)
			pEndpoint->EpCallBacks.EpResumeTxQueue
					(pEndpoint->EpCallBacks.pContext);

		HTCTrySend(target, pEndpoint, NULL);
	}
}

/* flush endpoint TX queue */
void HTCFlushEndpointTX(HTC_TARGET *target, HTC_ENDPOINT *pEndpoint, HTC_TX_TAG Tag)
{
    HTC_PACKET *pPacket;

    LOCK_HTC_TX(target);
    while(HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue)){
        pPacket = HTC_PACKET_DEQUEUE(&pEndpoint->TxQueue);

	if (pPacket) {
		/* let the sender know the packet was not delivered */
		pPacket->Status = A_ECANCELED;
		SendPacketCompletion(target, pPacket);
	}
    }
    UNLOCK_HTC_TX(target);
}

/* HTC API to flush an endpoint's TX queue*/
void HTCFlushEndpoint(HTC_HANDLE HTCHandle, HTC_ENDPOINT_ID Endpoint, HTC_TX_TAG Tag)
{
    HTC_TARGET      *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);
    HTC_ENDPOINT    *pEndpoint = &target->EndPoint[Endpoint];

    if (pEndpoint->ServiceID == 0) {
        AR_DEBUG_ASSERT(FALSE);
        /* not in use.. */
        return;
    }

    HTCFlushEndpointTX(target,pEndpoint,Tag);
}

/* HTC API to indicate activity to the credit distribution function */
void HTCIndicateActivityChange(HTC_HANDLE      HTCHandle,
                               HTC_ENDPOINT_ID Endpoint,
                               A_BOOL          Active)
{
   /*  TODO */
}

A_BOOL HTCIsEndpointActive(HTC_HANDLE      HTCHandle,
                           HTC_ENDPOINT_ID Endpoint)
{
    return TRUE;
}

/* process credit reports and call distribution function */
void HTCProcessCreditRpt(HTC_TARGET *target, HTC_CREDIT_REPORT *pRpt, int NumEntries, HTC_ENDPOINT_ID FromEndpoint)
{
    int             i;
    HTC_ENDPOINT    *pEndpoint;
    int             totalCredits = 0;
    A_UINT8         rpt_credits, rpt_ep_id;

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND, ("+HTCProcessCreditRpt, Credit Report Entries:%d \n", NumEntries));

        /* lock out TX while we update credits */
    LOCK_HTC_TX(target);


    for (i = 0; i < NumEntries; i++, pRpt++) {

        rpt_ep_id = HTC_GET_FIELD(pRpt, HTC_CREDIT_REPORT, ENDPOINTID);

        if (rpt_ep_id >= ENDPOINT_MAX) {
            AR_DEBUG_ASSERT(FALSE);
            break;
        }

        rpt_credits = HTC_GET_FIELD(pRpt, HTC_CREDIT_REPORT, CREDITS);

        pEndpoint = &target->EndPoint[rpt_ep_id];
#if DEBUG_CREDIT
        if (ep_debug_mask & (1 << pEndpoint->Id)){
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (" <HTC> Increase EP%d %d + %d = %d credits\n",
                    rpt_ep_id, pEndpoint->TxCredits, rpt_credits, pEndpoint->TxCredits + rpt_credits));
        }
#endif

#ifdef HTC_EP_STAT_PROFILING

        INC_HTC_EP_STAT(pEndpoint, TxCreditRpts, 1);
        INC_HTC_EP_STAT(pEndpoint, TxCreditsReturned, rpt_credits);

        if (FromEndpoint == rpt_ep_id) {
                /* this credit report arrived on the same endpoint indicating it arrived in an RX
                 * packet */
            INC_HTC_EP_STAT(pEndpoint, TxCreditsFromRx, rpt_credits);
            INC_HTC_EP_STAT(pEndpoint, TxCreditRptsFromRx, 1);
        } else if (FromEndpoint == ENDPOINT_0) {
                /* this credit arrived on endpoint 0 as a NULL message */
            INC_HTC_EP_STAT(pEndpoint, TxCreditsFromEp0, rpt_credits);
            INC_HTC_EP_STAT(pEndpoint, TxCreditRptsFromEp0, 1);
        } else {
                /* arrived on another endpoint */
            INC_HTC_EP_STAT(pEndpoint, TxCreditsFromOther, rpt_credits);
            INC_HTC_EP_STAT(pEndpoint, TxCreditRptsFromOther, 1);
        }

#endif
#if defined(HIF_USB)
        if (pEndpoint->Id >= ENDPOINT_2 && pEndpoint->Id <= ENDPOINT_5) {
            HTC_ENDPOINT_ID eid[DATA_EP_SIZE] = {ENDPOINT_5, ENDPOINT_4, ENDPOINT_2, ENDPOINT_3};
            int epid_idx;

            target->avail_tx_credits += rpt_credits;

            for (epid_idx = 0; epid_idx < DATA_EP_SIZE; epid_idx++) {
                pEndpoint = &target->EndPoint[eid[epid_idx]];
                if (HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue)) {
                    break;
                }

            }
            UNLOCK_HTC_TX(target);
            HTCTrySend(target,pEndpoint,NULL);
            LOCK_HTC_TX(target);
        } else {
            pEndpoint->TxCredits += rpt_credits;

            if (pEndpoint->TxCredits && HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue)) {
                UNLOCK_HTC_TX(target);
                HTCTrySend(target,pEndpoint,NULL);
                LOCK_HTC_TX(target);
            }
        }
#else
        pEndpoint->TxCredits += rpt_credits;

        if (pEndpoint->ServiceID == WMI_CONTROL_SVC) {
            LOCK_HTC_CREDIT(target);
            htc_credit_record(HTC_PROCESS_CREDIT_REPORT, pEndpoint->TxCredits,
                              HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue));
            UNLOCK_HTC_CREDIT(target);
        }

        if (pEndpoint->TxCredits && HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue)) {
            UNLOCK_HTC_TX(target);
#ifdef ATH_11AC_TXCOMPACT
            HTCTrySend(target,pEndpoint,NULL);
#else
            if (pEndpoint->ServiceID == HTT_DATA_MSG_SVC){
                HTCSendDataPkt(target, NULL, 0);
            } else {
                HTCTrySend(target,pEndpoint,NULL);
            }
#endif
            LOCK_HTC_TX(target);
        }
#endif
        totalCredits += rpt_credits;
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND, ("  Report indicated %d credits to distribute \n", totalCredits));

    UNLOCK_HTC_TX(target);

    AR_DEBUG_PRINTF(ATH_DEBUG_SEND, ("-HTCProcessCreditRpt \n"));
}

/* function to fetch stats from htc layer*/
struct ol_ath_htc_stats *ieee80211_ioctl_get_htc_stats(HTC_HANDLE HTCHandle)
{
    HTC_TARGET *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);

    return(&(target->htc_pkt_stats));
}
