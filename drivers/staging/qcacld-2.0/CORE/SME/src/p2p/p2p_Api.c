/*
 * Copyright (c) 2012-2014,2016 The Linux Foundation. All rights reserved.
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




#include "sme_Api.h"
#include "smsDebug.h"
#include "csrInsideApi.h"
#include "smeInside.h"
#include "p2p_Api.h"
#include "limApi.h"
#include "cfgApi.h"


eHalStatus p2pProcessNoAReq(tpAniSirGlobal pMac, tSmeCmd *pNoACmd);

/**
 * csr_release_roc_req_cmd() - Release the command
 * @mac_ctx: Global MAC Context
 *
 * Release the remain on channel request command from the queue
 *
 * Return: None
 */
void csr_release_roc_req_cmd(tpAniSirGlobal mac_ctx)
{
	tListElem *entry = NULL;
	tSmeCmd *cmd = NULL;

	entry = csrLLPeekHead(&mac_ctx->sme.smeCmdActiveList, LL_ACCESS_LOCK);
	if (entry) {
		cmd = GET_BASE_ADDR(entry, tSmeCmd, Link);
		if (eSmeCommandRemainOnChannel == cmd->command) {
			remainOnChanCallback callback =
				cmd->u.remainChlCmd.callback;
			/* process the msg */
			if (callback)
				callback(mac_ctx,
					cmd->u.remainChlCmd.callbackCtx, 0);
			smsLog(mac_ctx, LOGE,
				FL("Remove RoC Request from Active Cmd List"));
			/* Now put this cmd back on the avilable command list */
			if (csrLLRemoveEntry(&mac_ctx->sme.smeCmdActiveList,
					entry, LL_ACCESS_LOCK))
				smeReleaseCommand(mac_ctx, cmd);
		}
	}
}

/*------------------------------------------------------------------
 *
 * handle SME remain on channel request.
 *
 *------------------------------------------------------------------*/

eHalStatus p2pProcessRemainOnChannelCmd(tpAniSirGlobal pMac, tSmeCmd *p2pRemainonChn)
{
    eHalStatus status = eHAL_STATUS_FAILURE;
    tSirRemainOnChnReq* pMsg;
    tANI_U32 len;
    tCsrRoamSession *pSession = CSR_GET_SESSION( pMac, p2pRemainonChn->sessionId );

    if(!pSession)
    {
       smsLog(pMac, LOGE, FL("  session %d not found "), p2pRemainonChn->sessionId);
       goto error;
    }


    if(!pSession->sessionActive)
    {
       smsLog(pMac, LOGE, FL("  session %d is invalid or listen is disabled "),
            p2pRemainonChn->sessionId);
       goto error;
    }
    len = sizeof(tSirRemainOnChnReq) + pMac->p2pContext.probeRspIeLength;

    if( len > 0xFFFF )
    {
       /*In coming len for Msg is more then 16bit value*/
       smsLog(pMac, LOGE, FL("  Message length is very large, %d"),
            len);
       goto error;
    }

    pMsg = vos_mem_malloc(len);
    if ( NULL == pMsg )
        goto error;
    else
    {
        VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO, "%s call", __func__);
        vos_mem_set(pMsg, sizeof(tSirRemainOnChnReq), 0);
        pMsg->messageType = eWNI_SME_REMAIN_ON_CHANNEL_REQ;
        pMsg->length = (tANI_U16)len;
        vos_mem_copy(pMsg->selfMacAddr, pSession->selfMacAddr, sizeof(tSirMacAddr));
        pMsg->chnNum = p2pRemainonChn->u.remainChlCmd.chn;
        pMsg->phyMode = p2pRemainonChn->u.remainChlCmd.phyMode;
        pMsg->duration = p2pRemainonChn->u.remainChlCmd.duration;
        pMsg->sessionId = p2pRemainonChn->sessionId;
        pMsg->isProbeRequestAllowed = p2pRemainonChn->u.remainChlCmd.isP2PProbeReqAllowed;
        if( pMac->p2pContext.probeRspIeLength )
           vos_mem_copy((void *)pMsg->probeRspIe, (void *)pMac->p2pContext.probeRspIe,
                        pMac->p2pContext.probeRspIeLength);
        status = palSendMBMessage(pMac->hHdd, pMsg);
    }
error:
    if (eHAL_STATUS_FAILURE == status)
       csr_release_roc_req_cmd(pMac);
    return status;
}


/*------------------------------------------------------------------
 *
 * handle LIM remain on channel rsp: Success/failure.
 *
 *------------------------------------------------------------------*/

eHalStatus sme_remainOnChnRsp( tpAniSirGlobal pMac, tANI_U8 *pMsg)
{
    eHalStatus                         status = eHAL_STATUS_SUCCESS;
    tListElem                          *pEntry = NULL;
    tSmeCmd                            *pCommand = NULL;
    tANI_BOOLEAN fFound;
    tSirSmeRsp *pRsp = (tSirSmeRsp *)pMsg;

    if (pMac->fP2pListenOffload)
        pEntry = csrLLPeekHead(&pMac->sme.smeScanCmdActiveList, LL_ACCESS_LOCK);
    else
        pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);
    if( pEntry )
    {
        pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
        if( eSmeCommandRemainOnChannel == pCommand->command )
        {
            remainOnChanCallback callback = pCommand->u.remainChlCmd.callback;
            /* process the msg */
            if( callback )
                callback(pMac, pCommand->u.remainChlCmd.callbackCtx,
                         pRsp->statusCode);

            if (pMac->fP2pListenOffload)
            {
                fFound = csrLLRemoveEntry( &pMac->sme.smeScanCmdActiveList,
                        pEntry, LL_ACCESS_LOCK);
            }
            else
            {
                fFound = csrLLRemoveEntry( &pMac->sme.smeCmdActiveList, pEntry,
                        LL_ACCESS_LOCK);
            }

            if (fFound)
            {
                /* Now put this command back on the available command list */
                smeReleaseCommand(pMac, pCommand);
            }
            smeProcessPendingQueue( pMac );
        }
    }
    return status;
}

/*------------------------------------------------------------------
 *
 * Handle the remain on channel ready indication from PE
 *
 *------------------------------------------------------------------*/

eHalStatus sme_remainOnChnReady( tHalHandle hHal, tANI_U8* pMsg)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus  status = eHAL_STATUS_SUCCESS;
    tListElem *pEntry = NULL;
    tSmeCmd *pCommand = NULL;
    tCsrRoamInfo RoamInfo;

    if (pMac->fP2pListenOffload)
        pEntry = csrLLPeekHead(&pMac->sme.smeScanCmdActiveList, LL_ACCESS_LOCK);
    else
        pEntry = csrLLPeekHead(&pMac->sme.smeCmdActiveList, LL_ACCESS_LOCK);

    if( pEntry )
    {
        pCommand = GET_BASE_ADDR(pEntry, tSmeCmd, Link);
        if( eSmeCommandRemainOnChannel == pCommand->command )
        {

            /* forward the indication to HDD */
            RoamInfo.pRemainCtx = pCommand->u.remainChlCmd.callbackCtx;
            csrRoamCallCallback(pMac, ((tSirSmeRsp*)pMsg)->sessionId, &RoamInfo,
                                0, eCSR_ROAM_REMAIN_CHAN_READY, 0);
        }
    }

    return status;
}


eHalStatus sme_sendActionCnf( tHalHandle hHal, tANI_U8* pMsg)
{
   tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
   eHalStatus  status = eHAL_STATUS_SUCCESS;
   tCsrRoamInfo RoamInfo;
   tSirSmeRsp* pSmeRsp = (tSirSmeRsp*)pMsg;

    /* forward the indication to HDD */
    //RoamInfo can be passed as NULL....todo
    csrRoamCallCallback(pMac, pSmeRsp->sessionId, &RoamInfo, 0,
                        eCSR_ROAM_SEND_ACTION_CNF,
                       (pSmeRsp->statusCode == eSIR_SME_SUCCESS) ? 0:
                        eCSR_ROAM_RESULT_SEND_ACTION_FAIL);
    return status;
}




eHalStatus sme_p2pOpen( tHalHandle hHal )
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
   eHalStatus status = eHAL_STATUS_SUCCESS;

   //If static structure is too big, Need to change this function to allocate memory dynamically
   vos_mem_zero(&pMac->p2pContext, sizeof( tp2pContext ));

   if(!HAL_STATUS_SUCCESS(status))
   {
      sme_p2pClose(hHal);
    }

   return status;
}


eHalStatus p2pStop( tHalHandle hHal )
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

   if( pMac->p2pContext.probeRspIe )
   {
      vos_mem_free(pMac->p2pContext.probeRspIe);
      pMac->p2pContext.probeRspIe = NULL;
   }

   pMac->p2pContext.probeRspIeLength = 0;

   return eHAL_STATUS_SUCCESS;
}


eHalStatus sme_p2pClose( tHalHandle hHal )
{
   tpAniSirGlobal pMac = PMAC_STRUCT(hHal);

    if( pMac->p2pContext.probeRspIe )
    {
        vos_mem_free(pMac->p2pContext.probeRspIe);
        pMac->p2pContext.probeRspIe = NULL;
    }

    pMac->p2pContext.probeRspIeLength = 0;

   return eHAL_STATUS_SUCCESS;
}


tSirRFBand GetRFBand(tANI_U8 channel)
{
    if ((channel >= SIR_11A_CHANNEL_BEGIN) &&
        (channel <= SIR_11A_CHANNEL_END))
        return SIR_BAND_5_GHZ;

    if ((channel >= SIR_11B_CHANNEL_BEGIN) &&
        (channel <= SIR_11B_CHANNEL_END))
        return SIR_BAND_2_4_GHZ;

    return SIR_BAND_UNKNOWN;
}

/* ---------------------------------------------------------------------------

    \fn p2pRemainOnChannel
    \brief  API to post the remain on channel command.
    \param  hHal - The handle returned by macOpen.
    \param  sessinId - HDD session ID.
    \param  channel - Channel to remain on channel.
    \param  duration - Duration for which we should remain on channel
    \param  callback - callback function.
    \param  pContext - argument to the callback function
    \return eHalStatus

  -------------------------------------------------------------------------------*/
eHalStatus p2pRemainOnChannel(tHalHandle hHal, tANI_U8 sessionId,
         tANI_U8 channel, tANI_U32 duration,
        remainOnChanCallback callback,
        void *pContext, tANI_U8 isP2PProbeReqAllowed
        )
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tSmeCmd *pRemainChlCmd = NULL;
    tANI_U32 phyMode;

    pRemainChlCmd = smeGetCommandBuffer(pMac);
    if(pRemainChlCmd == NULL)
        return eHAL_STATUS_FAILURE;

    if (SIR_BAND_5_GHZ == GetRFBand(channel))
    {
       phyMode = WNI_CFG_PHY_MODE_11A;
    }
    else
    {
       phyMode = WNI_CFG_PHY_MODE_11G;
    }

    cfgSetInt(pMac, WNI_CFG_PHY_MODE, phyMode);

    do
    {
        /* call set in context */
        pRemainChlCmd->command = eSmeCommandRemainOnChannel;
        pRemainChlCmd->sessionId = sessionId;
        pRemainChlCmd->u.remainChlCmd.chn = channel;
        pRemainChlCmd->u.remainChlCmd.duration = duration;
        pRemainChlCmd->u.remainChlCmd.isP2PProbeReqAllowed = isP2PProbeReqAllowed;
        pRemainChlCmd->u.remainChlCmd.callback = callback;
        pRemainChlCmd->u.remainChlCmd.callbackCtx = pContext;

        //Put it at the head of the Q if we just finish finding the peer and ready to send a frame
        status = csrQueueSmeCommand(pMac, pRemainChlCmd, eANI_BOOLEAN_FALSE);
    } while(0);

    smsLog(pMac, LOGW, "exiting function %s", __func__);

    return(status);
}

eHalStatus p2pSendAction(tHalHandle hHal, tANI_U8 sessionId,
         const tANI_U8 *pBuf, tANI_U32 len, tANI_U16 wait, tANI_BOOLEAN noack)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tSirMbMsgP2p *pMsg;
    tANI_U16 msgLen;

    VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED,
       " %s sends action frame", __func__);
    msgLen = (tANI_U16)((sizeof( tSirMbMsg )) + len);
    pMsg = vos_mem_malloc(msgLen);
    if ( NULL == pMsg )
        status = eHAL_STATUS_FAILURE;
    else
    {
        vos_mem_set((void *)pMsg, msgLen, 0);
        pMsg->type = pal_cpu_to_be16((tANI_U16)eWNI_SME_SEND_ACTION_FRAME_IND);
        pMsg->msgLen = pal_cpu_to_be16(msgLen);
        pMsg->sessionId = sessionId;
        pMsg->noack = noack;
        pMsg->wait = (tANI_U16)wait;
        vos_mem_copy(pMsg->data, pBuf, len);
        status = palSendMBMessage(pMac->hHdd, pMsg);
    }

    return( status );
}

eHalStatus p2pCancelRemainOnChannel(tHalHandle hHal, tANI_U8 sessionId)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
    tSirMbMsgP2p *pMsg;
    tANI_U16 msgLen;

    //Need to check session ID to support concurrency

    msgLen = (tANI_U16)(sizeof( tSirMbMsg ));
    pMsg = vos_mem_malloc(msgLen);
    if ( NULL == pMsg )
       status = eHAL_STATUS_FAILURE;
    else
    {
        vos_mem_set((void *)pMsg, msgLen, 0);
        pMsg->type = pal_cpu_to_be16((tANI_U16)eWNI_SME_ABORT_REMAIN_ON_CHAN_IND);
        pMsg->msgLen = pal_cpu_to_be16(msgLen);
        pMsg->sessionId = sessionId;
        status = palSendMBMessage(pMac->hHdd, pMsg);
    }

    return( status );
}

eHalStatus p2pSetPs(tHalHandle hHal, tP2pPsConfig *pNoA)
{
    tpP2pPsConfig pNoAParam;
    tSirMsgQ msg;
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    pNoAParam = vos_mem_malloc(sizeof(tP2pPsConfig));
    if ( NULL == pNoAParam )
       status = eHAL_STATUS_FAILURE;
    else
    {
        vos_mem_set(pNoAParam, sizeof(tP2pPsConfig), 0);
        vos_mem_copy(pNoAParam, pNoA, sizeof(tP2pPsConfig));
        msg.type = eWNI_SME_UPDATE_NOA;
        msg.bodyval = 0;
        msg.bodyptr = pNoAParam;
        limPostMsgApi(pMac, &msg);
    }
    return status;
}


eHalStatus p2pProcessNoAReq(tpAniSirGlobal pMac, tSmeCmd *pNoACmd)
{
    tpP2pPsConfig pNoA;
    tSirMsgQ msg;
    eHalStatus status = eHAL_STATUS_SUCCESS;

    pNoA = vos_mem_malloc(sizeof(tP2pPsConfig));
    if ( NULL == pNoA )
        status = eHAL_STATUS_FAILURE;
    else
    {
        vos_mem_set(pNoA, sizeof(tP2pPsConfig), 0);
        pNoA->opp_ps = pNoACmd->u.NoACmd.NoA.opp_ps;
        pNoA->ctWindow = pNoACmd->u.NoACmd.NoA.ctWindow;
        pNoA->duration = pNoACmd->u.NoACmd.NoA.duration;
        pNoA->interval = pNoACmd->u.NoACmd.NoA.interval;
        pNoA->count = pNoACmd->u.NoACmd.NoA.count;
        pNoA->single_noa_duration = pNoACmd->u.NoACmd.NoA.single_noa_duration;
        pNoA->psSelection = pNoACmd->u.NoACmd.NoA.psSelection;
        pNoA->sessionid = pNoACmd->u.NoACmd.NoA.sessionid;
        msg.type = eWNI_SME_UPDATE_NOA;
        msg.bodyval = 0;
        msg.bodyptr = pNoA;
        limPostMsgApi(pMac, &msg);
    }
    return status;
}
