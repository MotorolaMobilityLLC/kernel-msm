/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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
#include "vos_api.h"
#include <adf_nbuf.h> /* adf_nbuf_t */
#include <vos_getBin.h>
#include "epping_main.h"

#ifdef DEBUG
void DebugDumpBytes(A_UCHAR *buffer, A_UINT16 length, char *pDescription)
{
    A_CHAR stream[60];
    A_CHAR byteOffsetStr[10];
    A_UINT32 i;
    A_UINT16 offset, count, byteOffset;

    A_PRINTF("<---------Dumping %d Bytes : %s ------>\n", length, pDescription);

    count = 0;
    offset = 0;
    byteOffset = 0;
    for(i = 0; i < length; i++) {
        A_SNPRINTF(stream + offset, (sizeof(stream) - offset),
                   "%02X ", buffer[i]);
        count ++;
        offset += 3;

        if(count == 16) {
            count = 0;
            offset = 0;
            A_SNPRINTF(byteOffsetStr, sizeof(byteOffset), "%4.4X",byteOffset);
            A_PRINTF("[%s]: %s\n", byteOffsetStr, stream);
            A_MEMZERO(stream, 60);
            byteOffset += 16;
        }
    }

    if(offset != 0) {
        A_SNPRINTF(byteOffsetStr, sizeof(byteOffset), "%4.4X",byteOffset);
        A_PRINTF("[%s]: %s\n", byteOffsetStr, stream);
    }

    A_PRINTF("<------------------------------------------------->\n");
}
#else
void DebugDumpBytes(A_UCHAR *buffer, A_UINT16 length, char *pDescription)
{}
#endif

static A_STATUS HTCProcessTrailer(HTC_TARGET     *target,
                                  A_UINT8        *pBuffer,
                                  int             Length,
                                  HTC_ENDPOINT_ID FromEndpoint);

static void DoRecvCompletion(HTC_ENDPOINT     *pEndpoint,
                             HTC_PACKET_QUEUE *pQueueToIndicate)
{

    do {

        if (HTC_QUEUE_EMPTY(pQueueToIndicate)) {
                /* nothing to indicate */
            break;
        }

        if (pEndpoint->EpCallBacks.EpRecvPktMultiple != NULL) {
            AR_DEBUG_PRINTF(ATH_DEBUG_RECV, (" HTC calling ep %d, recv multiple callback (%d pkts) \n",
                     pEndpoint->Id, HTC_PACKET_QUEUE_DEPTH(pQueueToIndicate)));
                /* a recv multiple handler is being used, pass the queue to the handler */
            pEndpoint->EpCallBacks.EpRecvPktMultiple(pEndpoint->EpCallBacks.pContext,
                                                     pQueueToIndicate);
            INIT_HTC_PACKET_QUEUE(pQueueToIndicate);
        } else {
            HTC_PACKET *pPacket;
            /* using legacy EpRecv */
            while (!HTC_QUEUE_EMPTY(pQueueToIndicate)) {
                pPacket = HTC_PACKET_DEQUEUE(pQueueToIndicate);
                if (pEndpoint->EpCallBacks.EpRecv == NULL) {
                    AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("HTC ep %d has NULL recv callback on packet %p\n",
                            pEndpoint->Id, pPacket));
                    continue;
                }
                AR_DEBUG_PRINTF(ATH_DEBUG_RECV, ("HTC calling ep %d recv callback on packet %p\n",
                        pEndpoint->Id, pPacket));
                pEndpoint->EpCallBacks.EpRecv(pEndpoint->EpCallBacks.pContext, pPacket);
            }
        }

    } while (FALSE);

}

static void RecvPacketCompletion(HTC_TARGET *target,HTC_ENDPOINT *pEndpoint, HTC_PACKET *pPacket)
{
    HTC_PACKET_QUEUE container;
    INIT_HTC_PACKET_QUEUE_AND_ADD(&container,pPacket);
        /* do completion */
    DoRecvCompletion(pEndpoint,&container);
}


void HTCControlRxComplete(void *Context, HTC_PACKET *pPacket)
{
    /* TODO, can't really receive HTC control messages yet.... */
    AR_DEBUG_PRINTF(ATH_DEBUG_ERR, ("Invalid call to  HTCControlRxComplete\n"));
}

void HTCUnblockRecv(HTC_HANDLE HTCHandle)
{
    /* TODO  find the Need in new model*/
}


void HTCEnableRecv(HTC_HANDLE HTCHandle)
{

    /* TODO  find the Need in new model*/
}

void HTCDisableRecv(HTC_HANDLE HTCHandle)
{

    /* TODO  find the Need in new model*/
}

int HTCGetNumRecvBuffers(HTC_HANDLE      HTCHandle,
                         HTC_ENDPOINT_ID Endpoint)
{
    HTC_TARGET      *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);
    HTC_ENDPOINT    *pEndpoint = &target->EndPoint[Endpoint];
    return HTC_PACKET_QUEUE_DEPTH(&pEndpoint->RxBufferHoldQueue);
}

HTC_PACKET *AllocateHTCPacketContainer(HTC_TARGET *target)
{
    HTC_PACKET *pPacket;

    LOCK_HTC_RX(target);

    if (NULL == target->pHTCPacketStructPool) {
        UNLOCK_HTC_RX(target);
        return NULL;
    }

    pPacket = target->pHTCPacketStructPool;
    target->pHTCPacketStructPool = (HTC_PACKET *)pPacket->ListLink.pNext;

    UNLOCK_HTC_RX(target);

    pPacket->ListLink.pNext = NULL;
    return pPacket;
}

void FreeHTCPacketContainer(HTC_TARGET *target, HTC_PACKET *pPacket)
{
    LOCK_HTC_RX(target);

    if (NULL == target->pHTCPacketStructPool) {
        target->pHTCPacketStructPool = pPacket;
        pPacket->ListLink.pNext = NULL;
    } else {
        pPacket->ListLink.pNext = (DL_LIST *)target->pHTCPacketStructPool;
        target->pHTCPacketStructPool = pPacket;
    }

    UNLOCK_HTC_RX(target);
}

#ifdef RX_SG_SUPPORT
adf_nbuf_t RxSgToSingleNetbuf(HTC_TARGET *target)
{
    adf_nbuf_t skb;
    a_uint8_t *anbdata;
    a_uint8_t *anbdata_new;
    a_uint32_t anblen;
    adf_nbuf_t new_skb = NULL;
    a_uint32_t sg_queue_len;
    adf_nbuf_queue_t *rx_sg_queue = &target->RxSgQueue;

    sg_queue_len = adf_nbuf_queue_len(rx_sg_queue);

    if (sg_queue_len <= 1) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
            ("RxSgToSingleNetbuf: invalid sg queue len %u\n"));
        goto _failed;
    }

    new_skb = adf_nbuf_alloc(target->ExpRxSgTotalLen, 0, 4, FALSE);
    if (new_skb == NULL) {
        AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
            ("RxSgToSingleNetbuf: can't allocate %u size netbuf\n",
                target->ExpRxSgTotalLen));
        goto _failed;
    }

    adf_nbuf_peek_header(new_skb, &anbdata_new, &anblen);

    skb = adf_nbuf_queue_remove(rx_sg_queue);
    do {
        adf_nbuf_peek_header(skb, &anbdata, &anblen);
        adf_os_mem_copy(anbdata_new, anbdata, adf_nbuf_len(skb));
        adf_nbuf_put_tail(new_skb, adf_nbuf_len(skb));
        anbdata_new += adf_nbuf_len(skb);
        adf_nbuf_free(skb);
        skb = adf_nbuf_queue_remove(rx_sg_queue);
    } while(skb != NULL);

    RESET_RX_SG_CONFIG(target);
    return new_skb;

_failed:

    while ((skb = adf_nbuf_queue_remove(rx_sg_queue)) != NULL) {
        adf_nbuf_free(skb);
    }

    RESET_RX_SG_CONFIG(target);
    return NULL;
}
#endif

static void HTCsuspendwow(HTC_TARGET *target)
{
    HIFsuspendwow(target->hif_dev);
    return;
}

A_STATUS HTCRxCompletionHandler(
    void *Context, adf_nbuf_t netbuf, a_uint8_t pipeID)
{
    A_STATUS        status = A_OK;
    HTC_FRAME_HDR   *HtcHdr;
    HTC_TARGET      *target = (HTC_TARGET *)Context;
    a_uint8_t       *netdata;
    a_uint32_t      netlen;
    HTC_ENDPOINT    *pEndpoint;
    HTC_PACKET      *pPacket;
    A_UINT16        payloadLen;
    a_uint32_t      trailerlen = 0;
    A_UINT8         htc_ep_id;

#ifdef RX_SG_SUPPORT
    LOCK_HTC_RX(target);
    if (target->IsRxSgInprogress) {
        target->CurRxSgTotalLen += adf_nbuf_len(netbuf);
        adf_nbuf_queue_add(&target->RxSgQueue, netbuf);
        if (target->CurRxSgTotalLen == target->ExpRxSgTotalLen) {
            netbuf = RxSgToSingleNetbuf(target);
            if (netbuf == NULL) {
                UNLOCK_HTC_RX(target);
                goto _out;
            }
        }
        else {
            netbuf = NULL;
            UNLOCK_HTC_RX(target);
            goto _out;
        }
    }
    UNLOCK_HTC_RX(target);
#endif

    netdata = adf_nbuf_data(netbuf);
    netlen = adf_nbuf_len(netbuf);

    HtcHdr = (HTC_FRAME_HDR *)netdata;

    do {

        htc_ep_id = HTC_GET_FIELD(HtcHdr, HTC_FRAME_HDR, ENDPOINTID);

        if (htc_ep_id >= ENDPOINT_MAX) {
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("HTC Rx: invalid EndpointID=%d\n",htc_ep_id));
            DebugDumpBytes((A_UINT8 *)HtcHdr,sizeof(HTC_FRAME_HDR),"BAD HTC Header");
            status = A_ERROR;
            VOS_BUG(0);
            break;
        }

        pEndpoint = &target->EndPoint[htc_ep_id];

        /*
         * If this endpoint that received a message from the target has
         * a to-target HIF pipe whose send completions are polled rather
         * than interrupt-driven, this is a good point to ask HIF to check
         * whether it has any completed sends to handle.
         */
        if (pEndpoint->ul_is_polled) {
            HTCSendCompleteCheck(pEndpoint, 1);
        }

        payloadLen = HTC_GET_FIELD(HtcHdr, HTC_FRAME_HDR, PAYLOADLEN);

        if (netlen < (payloadLen + HTC_HDR_LENGTH)) {
#ifdef RX_SG_SUPPORT
            LOCK_HTC_RX(target);
            target->IsRxSgInprogress = TRUE;
            adf_nbuf_queue_init(&target->RxSgQueue);
            adf_nbuf_queue_add(&target->RxSgQueue, netbuf);
            target->ExpRxSgTotalLen = (payloadLen + HTC_HDR_LENGTH);
            target->CurRxSgTotalLen += netlen;
            UNLOCK_HTC_RX(target);
            netbuf = NULL;
            break;
#else
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("HTC Rx: insufficient length, got:%d expected =%zu\n",
                netlen, payloadLen + HTC_HDR_LENGTH));
            DebugDumpBytes((A_UINT8 *)HtcHdr,sizeof(HTC_FRAME_HDR),"BAD RX packet length");
            status = A_ERROR;
            VOS_BUG(0);
            break;
#endif
        }

#ifdef HTC_EP_STAT_PROFILING
        LOCK_HTC_RX(target);
        INC_HTC_EP_STAT(pEndpoint,RxReceived,1);
        UNLOCK_HTC_RX(target);
#endif

        //if (IS_TX_CREDIT_FLOW_ENABLED(pEndpoint)) {
        {
            A_UINT8         temp;
                /* get flags to check for trailer */
            temp = HTC_GET_FIELD(HtcHdr, HTC_FRAME_HDR, FLAGS);
            if (temp & HTC_FLAGS_RECV_TRAILER) {
                    /* extract the trailer length */
                temp = HTC_GET_FIELD(HtcHdr, HTC_FRAME_HDR, CONTROLBYTES0);
                if ((temp < sizeof(HTC_RECORD_HDR)) || (temp > payloadLen)) {
                    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                        ("HTCRxCompletionHandler, invalid header (payloadlength should be :%d, CB[0] is:%d) \n",
                            payloadLen, temp));
                    status = A_EPROTO;
                    break;
                }

                trailerlen = temp;
                    /* process trailer data that follows HDR + application payload */
                status = HTCProcessTrailer(target,
                                           ((A_UINT8 *)HtcHdr + HTC_HDR_LENGTH + payloadLen - temp),
                                           temp, htc_ep_id);
                if (A_FAILED(status)) {
                    break;
                }

            }
        }

        if (((int)payloadLen - (int)trailerlen) <= 0) {
            /* zero length packet with trailer data, just drop these */
            break;
        }


        if (htc_ep_id == ENDPOINT_0) {
            A_UINT16 message_id;
            HTC_UNKNOWN_MSG *htc_msg;
            int wow_nack = 0;

                /* remove HTC header */
            adf_nbuf_pull_head(netbuf, HTC_HDR_LENGTH);
            netdata = adf_nbuf_data(netbuf);
            netlen = adf_nbuf_len(netbuf);

            htc_msg = (HTC_UNKNOWN_MSG*)netdata;
            message_id = HTC_GET_FIELD(htc_msg, HTC_UNKNOWN_MSG, MESSAGEID);

            switch (message_id) {
            default:
                /* handle HTC control message */
                if (target->CtrlResponseProcessing) {
                    /* this is a fatal error, target should not be sending unsolicited messages
                     * on the endpoint 0 */
                    AR_DEBUG_PRINTF(ATH_DEBUG_ERR,("HTC Rx Ctrl still processing\n"));
                    status = A_ERROR;
                    VOS_BUG(FALSE);
                    break;
                }

                LOCK_HTC_RX(target);
                target->CtrlResponseLength = min((int)netlen,HTC_MAX_CONTROL_MESSAGE_LENGTH);
                A_MEMCPY(target->CtrlResponseBuffer,netdata,target->CtrlResponseLength);

                /* Requester will clear this flag */
                target->CtrlResponseProcessing = TRUE;
                UNLOCK_HTC_RX(target);

                adf_os_mutex_release(target->osdev, &target->CtrlResponseValid);
                break;
            case HTC_MSG_SEND_SUSPEND_COMPLETE:
                wow_nack = 0;
                LOCK_HTC_CREDIT(target);
                htc_credit_record(HTC_SUSPEND_ACK, pEndpoint->TxCredits,
                                  HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue));
                UNLOCK_HTC_CREDIT(target);
                target->HTCInitInfo.TargetSendSuspendComplete((void *)&wow_nack);
                HTCsuspendwow(target);
                break;
            case HTC_MSG_NACK_SUSPEND:
                wow_nack = 1;
                LOCK_HTC_CREDIT(target);
                htc_credit_record(HTC_SUSPEND_NACK, pEndpoint->TxCredits,
                                  HTC_PACKET_QUEUE_DEPTH(&pEndpoint->TxQueue));
                UNLOCK_HTC_CREDIT(target);
                target->HTCInitInfo.TargetSendSuspendComplete((void *)&wow_nack);
                break;
            }

            adf_nbuf_free(netbuf);
            netbuf = NULL;
            break;
        }

            /* the current message based HIF architecture allocates net bufs for recv packets
             * since this layer bridges that HIF to upper layers , which expects HTC packets,
             * we form the packets here
             * TODO_FIXME */
        pPacket  = AllocateHTCPacketContainer(target);
        if (NULL == pPacket) {
            status = A_NO_RESOURCE;
            break;
        }
        pPacket->Status = A_OK;
        pPacket->Endpoint = htc_ep_id;
        pPacket->pPktContext = netbuf;
        pPacket->pBuffer = adf_nbuf_data(netbuf) + HTC_HDR_LENGTH;
        pPacket->ActualLength = netlen - HTC_HEADER_LEN - trailerlen;

        adf_nbuf_pull_head(netbuf, HTC_HEADER_LEN);
        adf_nbuf_set_pktlen(netbuf, pPacket->ActualLength);

        RecvPacketCompletion(target,pEndpoint,pPacket);
            /* recover the packet container */
        FreeHTCPacketContainer(target,pPacket);
        netbuf = NULL;

    } while(FALSE);

#ifdef RX_SG_SUPPORT
_out:
#endif

    if (netbuf != NULL) {
        adf_nbuf_free(netbuf);
    }

    return status;

}

A_STATUS HTCAddReceivePktMultiple(HTC_HANDLE HTCHandle, HTC_PACKET_QUEUE *pPktQueue)
{
    HTC_TARGET      *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);
    HTC_ENDPOINT    *pEndpoint;
    HTC_PACKET      *pFirstPacket;
    A_STATUS        status = A_OK;
    HTC_PACKET      *pPacket;

    pFirstPacket = HTC_GET_PKT_AT_HEAD(pPktQueue);

    if (NULL == pFirstPacket) {
        A_ASSERT(FALSE);
        return A_EINVAL;
    }

    AR_DEBUG_ASSERT(pFirstPacket->Endpoint < ENDPOINT_MAX);

    AR_DEBUG_PRINTF(ATH_DEBUG_RECV,
                    ("+- HTCAddReceivePktMultiple : endPointId: %d, cnt:%d, length: %d\n",
                    pFirstPacket->Endpoint,
                    HTC_PACKET_QUEUE_DEPTH(pPktQueue),
                    pFirstPacket->BufferLength));

    pEndpoint = &target->EndPoint[pFirstPacket->Endpoint];

    LOCK_HTC_RX(target);

    do {

        if (HTC_STOPPING(target)) {
            status = A_ERROR;
            break;
        }

            /* store receive packets */
        HTC_PACKET_QUEUE_TRANSFER_TO_TAIL(&pEndpoint->RxBufferHoldQueue,pPktQueue);

    } while (FALSE);

    UNLOCK_HTC_RX(target);

    if (A_FAILED(status)) {
            /* walk through queue and mark each one canceled */
        HTC_PACKET_QUEUE_ITERATE_ALLOW_REMOVE(pPktQueue,pPacket) {
            pPacket->Status = A_ECANCELED;
        } HTC_PACKET_QUEUE_ITERATE_END;

        DoRecvCompletion(pEndpoint,pPktQueue);
    }

    return status;
}


A_STATUS HTCAddReceivePkt(HTC_HANDLE HTCHandle, HTC_PACKET *pPacket)
{
    HTC_PACKET_QUEUE queue;
    INIT_HTC_PACKET_QUEUE_AND_ADD(&queue,pPacket);
    return HTCAddReceivePktMultiple(HTCHandle, &queue);
}

void HTCFlushRxHoldQueue(HTC_TARGET *target, HTC_ENDPOINT *pEndpoint)
{
    HTC_PACKET       *pPacket;
    HTC_PACKET_QUEUE container;

    LOCK_HTC_RX(target);

    while (1) {
        pPacket = HTC_PACKET_DEQUEUE(&pEndpoint->RxBufferHoldQueue);
        if (NULL == pPacket) {
            break;
        }
        UNLOCK_HTC_RX(target);
        pPacket->Status = A_ECANCELED;
        pPacket->ActualLength = 0;
        AR_DEBUG_PRINTF(ATH_DEBUG_RECV, ("  Flushing RX packet:%p, length:%d, ep:%d \n",
                pPacket, pPacket->BufferLength, pPacket->Endpoint));
        INIT_HTC_PACKET_QUEUE_AND_ADD(&container,pPacket);
            /* give the packet back */
        DoRecvCompletion(pEndpoint,&container);
        LOCK_HTC_RX(target);
    }

    UNLOCK_HTC_RX(target);
}

void HTCRecvInit(HTC_TARGET *target)
{
    /* Initialize CtrlResponseValid to block */
    adf_os_init_mutex(&target->CtrlResponseValid);
    adf_os_mutex_acquire(target->osdev, &target->CtrlResponseValid);
}


    /* polling routine to wait for a control packet to be received */
A_STATUS HTCWaitRecvCtrlMessage(HTC_TARGET *target)
{
//    int count = HTC_TARGET_MAX_RESPONSE_POLL;

    AR_DEBUG_PRINTF(ATH_DEBUG_TRC,("+HTCWaitCtrlMessageRecv\n"));

    /* Wait for BMI request/response transaction to complete */
    while (adf_os_mutex_acquire(target->osdev, &target->CtrlResponseValid)) {
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_TRC,("-HTCWaitCtrlMessageRecv success\n"));
    return A_OK;
}

static A_STATUS HTCProcessTrailer(HTC_TARGET     *target,
                                  A_UINT8        *pBuffer,
                                  int             Length,
                                  HTC_ENDPOINT_ID FromEndpoint)
{
    HTC_RECORD_HDR          *pRecord;
    A_UINT8                 htc_rec_id;
    A_UINT8                 htc_rec_len;
    A_UINT8                 *pRecordBuf;
    A_UINT8                 *pOrigBuffer;
    int                     origLength;
    A_STATUS                status;

    AR_DEBUG_PRINTF(ATH_DEBUG_RECV, ("+HTCProcessTrailer (length:%d) \n", Length));

    if (AR_DEBUG_LVL_CHECK(ATH_DEBUG_RECV)) {
        AR_DEBUG_PRINTBUF(pBuffer,Length,"Recv Trailer");
    }

    pOrigBuffer = pBuffer;
    origLength = Length;
    status = A_OK;

    while (Length > 0) {

        if (Length < sizeof(HTC_RECORD_HDR)) {
            status = A_EPROTO;
            break;
        }
            /* these are byte aligned structs */
        pRecord = (HTC_RECORD_HDR *)pBuffer;
        Length -= sizeof(HTC_RECORD_HDR);
        pBuffer += sizeof(HTC_RECORD_HDR);

        htc_rec_len = HTC_GET_FIELD(pRecord, HTC_RECORD_HDR, LENGTH);
        htc_rec_id = HTC_GET_FIELD(pRecord, HTC_RECORD_HDR, RECORDID);

        if (htc_rec_len > Length) {
                /* no room left in buffer for record */
            AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                (" invalid record length: %d (id:%d) buffer has: %d bytes left \n",
                        htc_rec_len, htc_rec_id, Length));
            status = A_EPROTO;
            break;
        }
            /* start of record follows the header */
        pRecordBuf = pBuffer;

        switch (htc_rec_id) {
            case HTC_RECORD_CREDITS:
                AR_DEBUG_ASSERT(htc_rec_len >= sizeof(HTC_CREDIT_REPORT));
                HTCProcessCreditRpt(target,
                                    (HTC_CREDIT_REPORT *)pRecordBuf,
                                    htc_rec_len / (sizeof(HTC_CREDIT_REPORT)),
                                    FromEndpoint);
                break;

#ifdef HIF_SDIO
            case HTC_RECORD_LOOKAHEAD:
                /* Process in HIF layer */
                break;

            case HTC_RECORD_LOOKAHEAD_BUNDLE:
                /* Process in HIF layer */
                break;
#endif /* HIF_SDIO */

            default:
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR, (" HTC unhandled record: id:%d length:%d \n",
                        htc_rec_id, htc_rec_len));
                break;
        }

        if (A_FAILED(status)) {
            break;
        }

            /* advance buffer past this record for next time around */
        pBuffer += htc_rec_len;
        Length -= htc_rec_len;
    }

    if (A_FAILED(status)) {
        DebugDumpBytes(pOrigBuffer,origLength,"BAD Recv Trailer");
    }

    AR_DEBUG_PRINTF(ATH_DEBUG_RECV, ("-HTCProcessTrailer \n"));
    return status;

}
