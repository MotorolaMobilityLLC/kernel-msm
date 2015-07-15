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
#include <adf_nbuf.h>  /* adf_nbuf_t */
#if defined(HIF_PCI)
#include "if_pci.h"
#endif

extern unsigned int htc_credit_flow;

#ifndef DEBUG_CREDIT
#define DEBUG_CREDIT 0
#endif

A_STATUS HTCConnectService(HTC_HANDLE               HTCHandle,
                           HTC_SERVICE_CONNECT_REQ  *pConnectReq,
                           HTC_SERVICE_CONNECT_RESP *pConnectResp)
{
    HTC_TARGET *target = GET_HTC_TARGET_FROM_HANDLE(HTCHandle);
    A_STATUS                            status = A_OK;
    HTC_PACKET                          *pSendPacket = NULL;
    HTC_CONNECT_SERVICE_RESPONSE_MSG    *pResponseMsg;
    HTC_CONNECT_SERVICE_MSG             *pConnectMsg;
    HTC_ENDPOINT_ID                     assignedEndpoint = ENDPOINT_MAX;
    HTC_ENDPOINT                        *pEndpoint;
    unsigned int                        maxMsgSize = 0;
    adf_nbuf_t                          netbuf;
    A_UINT8                             txAlloc;
    int                                 length;
    A_BOOL                              disableCreditFlowCtrl = FALSE;
    A_UINT16                            conn_flags;
    A_UINT16                            rsp_msg_id, rsp_msg_serv_id, rsp_msg_max_msg_size;
    A_UINT8                             rsp_msg_status, rsp_msg_end_id, rsp_msg_serv_meta_len;

    AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("+HTCConnectService, target:%p SvcID:0x%X \n",
                                    target, pConnectReq->ServiceID));

    do {

        AR_DEBUG_ASSERT(pConnectReq->ServiceID != 0);

        if (HTC_CTRL_RSVD_SVC == pConnectReq->ServiceID) {
                /* special case for pseudo control service */
            assignedEndpoint = ENDPOINT_0;
            maxMsgSize = HTC_MAX_CONTROL_MESSAGE_LENGTH;
            txAlloc = 0;

        } else {

            txAlloc = HTCGetCreditAllocation(target,pConnectReq->ServiceID);
            if (!txAlloc) {
                AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("Service %d does not allocate target credits!\n",
                            pConnectReq->ServiceID));
            }

                /* allocate a packet to send to the target */
            pSendPacket = HTCAllocControlTxPacket(target);

            if (NULL == pSendPacket) {
                AR_DEBUG_ASSERT(FALSE);
                status = A_NO_MEMORY;
                break;
            }

            netbuf = (adf_nbuf_t)GET_HTC_PACKET_NET_BUF_CONTEXT(pSendPacket);
            length = sizeof(HTC_CONNECT_SERVICE_MSG) + pConnectReq->MetaDataLength;

                /* assemble connect service message */
            adf_nbuf_put_tail(netbuf, length);
            pConnectMsg = (HTC_CONNECT_SERVICE_MSG *)adf_nbuf_data(netbuf);

            if (NULL == pConnectMsg) {
                AR_DEBUG_ASSERT(0);
                status = A_EFAULT;
                break;
            }

            A_MEMZERO(pConnectMsg,sizeof(HTC_CONNECT_SERVICE_MSG));

            conn_flags = (pConnectReq->ConnectionFlags & ~HTC_SET_RECV_ALLOC_MASK) |
                        HTC_CONNECT_FLAGS_SET_RECV_ALLOCATION(txAlloc);
            HTC_SET_FIELD(pConnectMsg, HTC_CONNECT_SERVICE_MSG, MESSAGEID,
                            HTC_MSG_CONNECT_SERVICE_ID);
            HTC_SET_FIELD(pConnectMsg, HTC_CONNECT_SERVICE_MSG, SERVICE_ID,
                            pConnectReq->ServiceID);
            HTC_SET_FIELD(pConnectMsg, HTC_CONNECT_SERVICE_MSG,
                            CONNECTIONFLAGS, conn_flags);

            if (pConnectReq->ConnectionFlags & HTC_CONNECT_FLAGS_DISABLE_CREDIT_FLOW_CTRL) {
                disableCreditFlowCtrl = TRUE;
            }
#if defined(HIF_USB)
            if (!htc_credit_flow) {
                disableCreditFlowCtrl = TRUE;
            }
#else
            /* Only enable credit for WMI service */
            if (!htc_credit_flow && pConnectReq->ServiceID != WMI_CONTROL_SVC) {
                disableCreditFlowCtrl = TRUE;
            }
#endif
                /* check caller if it wants to transfer meta data */
            if ((pConnectReq->pMetaData != NULL) &&
                (pConnectReq->MetaDataLength <= HTC_SERVICE_META_DATA_MAX_LENGTH)) {
                    /* copy meta data into message buffer (after header ) */
                A_MEMCPY((A_UINT8 *)pConnectMsg + sizeof(HTC_CONNECT_SERVICE_MSG),
                         pConnectReq->pMetaData,
                         pConnectReq->MetaDataLength);

                HTC_SET_FIELD(pConnectMsg, HTC_CONNECT_SERVICE_MSG,
                            SERVICEMETALENGTH, pConnectReq->MetaDataLength);
            }

            SET_HTC_PACKET_INFO_TX(pSendPacket,
                                   NULL,
                                   (A_UINT8 *)pConnectMsg,
                                   length,
                                   ENDPOINT_0,
                                   HTC_SERVICE_TX_PACKET_TAG);


            status = HTCSendPkt((HTC_HANDLE)target,pSendPacket);
                /* we don't own it anymore */
            pSendPacket = NULL;
            if (A_FAILED(status)) {
                break;
            }

                /* wait for response */
            status = HTCWaitRecvCtrlMessage(target);
            if (A_FAILED(status)) {
                break;
            }
                /* we controlled the buffer creation so it has to be properly aligned */
            pResponseMsg = (HTC_CONNECT_SERVICE_RESPONSE_MSG *)target->CtrlResponseBuffer;

            rsp_msg_id = HTC_GET_FIELD(pResponseMsg,
                    HTC_CONNECT_SERVICE_RESPONSE_MSG, MESSAGEID);
            rsp_msg_serv_id = HTC_GET_FIELD(pResponseMsg,
                    HTC_CONNECT_SERVICE_RESPONSE_MSG, SERVICEID);
            rsp_msg_status = HTC_GET_FIELD(pResponseMsg,
                    HTC_CONNECT_SERVICE_RESPONSE_MSG, STATUS);
            rsp_msg_end_id = HTC_GET_FIELD(pResponseMsg,
                    HTC_CONNECT_SERVICE_RESPONSE_MSG, ENDPOINTID);
            rsp_msg_max_msg_size = HTC_GET_FIELD(pResponseMsg,
                    HTC_CONNECT_SERVICE_RESPONSE_MSG, MAXMSGSIZE);
            rsp_msg_serv_meta_len = HTC_GET_FIELD(pResponseMsg,
                    HTC_CONNECT_SERVICE_RESPONSE_MSG, SERVICEMETALENGTH);


            if ((rsp_msg_id != HTC_MSG_CONNECT_SERVICE_RESPONSE_ID) ||
                (target->CtrlResponseLength < sizeof(HTC_CONNECT_SERVICE_RESPONSE_MSG))) {
                    /* this message is not valid */
                AR_DEBUG_ASSERT(FALSE);
                status = A_EPROTO;
                break;
            }

            AR_DEBUG_PRINTF(ATH_DEBUG_TRC,
                    ("HTCConnectService, service 0x%X connect response from target status:%d, assigned ep: %d\n",
                                rsp_msg_serv_id, rsp_msg_status, rsp_msg_end_id));

            pConnectResp->ConnectRespCode = rsp_msg_status;

            /* check response status */
            if (rsp_msg_status != HTC_SERVICE_SUCCESS) {
                AR_DEBUG_PRINTF(ATH_DEBUG_ERR,
                    (" Target failed service 0x%X connect request (status:%d)\n",
                                rsp_msg_serv_id, rsp_msg_status));
                status = A_EPROTO;
#ifdef QCA_TX_HTT2_SUPPORT
                /* Keep work and not to block the control message. */
                target->CtrlResponseProcessing = FALSE;
#endif /* QCA_TX_HTT2_SUPPORT */
                break;
            }

            assignedEndpoint = (HTC_ENDPOINT_ID)rsp_msg_end_id;
            maxMsgSize = rsp_msg_max_msg_size;

            if ((pConnectResp->pMetaData != NULL) &&
                (rsp_msg_serv_meta_len > 0) &&
                (rsp_msg_serv_meta_len <= HTC_SERVICE_META_DATA_MAX_LENGTH)) {
                    /* caller supplied a buffer and the target responded with data */
                int copyLength = min((int)pConnectResp->BufferLength, (int)rsp_msg_serv_meta_len);
                    /* copy the meta data */
                A_MEMCPY(pConnectResp->pMetaData,
                         ((A_UINT8 *)pResponseMsg) + sizeof(HTC_CONNECT_SERVICE_RESPONSE_MSG),
                         copyLength);
                pConnectResp->ActualLength = copyLength;
            }
                /* done processing response buffer */
            target->CtrlResponseProcessing = FALSE;
        }

            /* the rest of these are parameter checks so set the error status */
        status = A_EPROTO;

        if (assignedEndpoint >= ENDPOINT_MAX) {
            AR_DEBUG_ASSERT(FALSE);
            break;
        }

        if (0 == maxMsgSize) {
            AR_DEBUG_ASSERT(FALSE);
            break;
        }

        pEndpoint = &target->EndPoint[assignedEndpoint];
        pEndpoint->Id = assignedEndpoint;
        if (pEndpoint->ServiceID != 0) {
            /* endpoint already in use! */
            AR_DEBUG_ASSERT(FALSE);
            break;
        }

            /* return assigned endpoint to caller */
        pConnectResp->Endpoint = assignedEndpoint;
        pConnectResp->MaxMsgLength = maxMsgSize;

            /* setup the endpoint */
        pEndpoint->ServiceID = pConnectReq->ServiceID; /* this marks the endpoint in use */
        pEndpoint->MaxTxQueueDepth = pConnectReq->MaxSendQueueDepth;
        pEndpoint->MaxMsgLength = maxMsgSize;
        pEndpoint->TxCredits = txAlloc;
        pEndpoint->TxCreditSize = target->TargetCreditSize;
        pEndpoint->TxCreditsPerMaxMsg = maxMsgSize / target->TargetCreditSize;
        if (maxMsgSize % target->TargetCreditSize) {
            pEndpoint->TxCreditsPerMaxMsg++;
        }
#if DEBUG_CREDIT
        adf_os_print(" Endpoint%d initial credit:%d, size:%d.\n",
                pEndpoint->Id, pEndpoint->TxCredits, pEndpoint->TxCreditSize);
#endif

            /* copy all the callbacks */
        pEndpoint->EpCallBacks = pConnectReq->EpCallbacks;

        status = HIFMapServiceToPipe(target->hif_dev,
                                     pEndpoint->ServiceID,
                                     &pEndpoint->UL_PipeID,
                                     &pEndpoint->DL_PipeID,
                                     &pEndpoint->ul_is_polled,
                                     &pEndpoint->dl_is_polled);
        if (A_FAILED(status)) {
            break;
        }

        adf_os_assert(!pEndpoint->dl_is_polled); /* not currently supported */

        if (pEndpoint->ul_is_polled) {
            adf_os_timer_init(
                target->osdev,
                &pEndpoint->ul_poll_timer,
                HTCSendCompleteCheckCleanup,
                pEndpoint, ADF_DEFERRABLE_TIMER);
        }

        AR_DEBUG_PRINTF(ATH_DEBUG_SETUP, ("HTC Service:0x%4.4X, ULpipe:%d DLpipe:%d id:%d Ready\n",
                       pEndpoint->ServiceID,pEndpoint->UL_PipeID,pEndpoint->DL_PipeID,pEndpoint->Id));

        if (disableCreditFlowCtrl && pEndpoint->TxCreditFlowEnabled) {
            pEndpoint->TxCreditFlowEnabled = FALSE;
            AR_DEBUG_PRINTF(ATH_DEBUG_WARN,("HTC Service:0x%4.4X ep:%d TX flow control disabled\n",
                                pEndpoint->ServiceID, assignedEndpoint));
        }

    } while (FALSE);

    AR_DEBUG_PRINTF(ATH_DEBUG_TRC, ("-HTCConnectService \n"));

    return status;
}


void HTCSetCreditDistribution(HTC_HANDLE               HTCHandle,
                              void                     *pCreditDistContext,
                              HTC_CREDIT_DIST_CALLBACK CreditDistFunc,
                              HTC_CREDIT_INIT_CALLBACK CreditInitFunc,
                              HTC_SERVICE_ID           ServicePriorityOrder[],
                              int                      ListLength)
{
   /* NOT Supported, this transport does not use a credit based flow control mechanism */

}


void HTCFwEventHandler(void *context, A_STATUS status)
{
    HTC_TARGET      *target = (HTC_TARGET *)context;
    HTC_INIT_INFO   *initInfo = &target->HTCInitInfo;

    /* check if target failure handler exists and pass error code to it. */
    if (target->HTCInitInfo.TargetFailure != NULL) {
        initInfo->TargetFailure(initInfo->pContext, status);
    }
}

/* Disable ASPM : disable PCIe low power */
void htc_disable_aspm(void)
{
#if defined(HIF_PCI)
   hif_disable_aspm();
#endif
}
