/*
 * Copyright (c) 2012, Code Aurora Forum. All rights reserved.
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

#include "palTypes.h"
#include "wniApi.h"     /* WNI_CFG_SET_REQ */
#include "sirParams.h"  /* tSirMbMsg */
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halHddApis.h" /* palAllocateMemory */
#endif
#include "smsDebug.h"   /* smsLog */
#include "cfgApi.h"
#include "ccmApi.h"
#include "logDump.h"

//#define CCM_DEBUG
#undef CCM_DEBUG

#define CCM_DEBUG2
//#undef CCM_DEBUG2

#define CFGOBJ_ALIGNTO          4
#define CFGOBJ_ALIGN(len)       ( ((len)+CFGOBJ_ALIGNTO-1) & ~(CFGOBJ_ALIGNTO-1) )

#define CFGOBJ_ID_SIZE                  4       /* 4 bytes for cfgId */
#define CFGOBJ_LEN_SIZE                 4       /* 4 bytes for length */
#define CFGOBJ_INTEGER_VALUE_SIZE       4       /* 4 bytes for integer value */

#define CFG_UPDATE_MAGIC_DWORD     0xabab

#define halHandle2HddHandle(hHal)  ( (NULL == (hHal)) ? 0 : ((tpAniSirGlobal)(hHal))->hHdd )

static void ccmComplete(tHddHandle hHdd, void *done)
{
    if (done)
    {
        (void)palSemaphoreGive(hHdd, done);
    }
}

static void ccmWaitForCompletion(tHddHandle hHdd, void *done)
{
    if (done)
    {
        (void)palSemaphoreTake(hHdd, done);
    }
}

static tANI_U32 * encodeCfgReq(tHddHandle hHdd, tANI_U32 *pl, tANI_U32 cfgId, tANI_S32 length, void *pBuf, tANI_U32 value, tANI_U32 type)
{
    *pl++ = pal_cpu_to_be32(cfgId) ;
    *pl++ = pal_cpu_to_be32(length) ;
    if (type == CCM_INTEGER_TYPE)
    {
        *pl++ = pal_cpu_to_be32(value) ;
    }
    else
    {
        palCopyMemory(hHdd, (void *)pl, (void *)pBuf, length);
        pl += (CFGOBJ_ALIGN(length) / CFGOBJ_ALIGNTO);
    }
    return pl ;
}

/*
 * CCM_STRING_TYPE                       CCM_INTEGER_TYPE
 * |<--------  4   ----->|               |<--------  4   ----->|                          
 * +----------+            <-- msg  -->  +----------+                                 
 * |type      |                          |type      |           
 * +----------+                          +----------+           
 * |msgLen=24 |                          |msgLen=16 |           
 * +----------+----------+               +----------+----------+
 * | cfgId               |               | cfgId               |  
 * +---------------------+               +---------------------+
 * | length=11           |               | length=4            |  
 * +---------------------+               +---------------------+
 * |                     |               | value               |  
 * |                     |               +---------------------+  
 * |                     |
 * |                +----+
 * |                |////| <- padding to 4-byte boundary
 * +----------------+----+
 */
static eHalStatus sendCfg(tpAniSirGlobal pMac, tHddHandle hHdd, tCfgReq *req, tANI_BOOLEAN fRsp)
{
    tSirMbMsg *msg;
    eHalStatus status;
    tANI_S16 msgLen = (tANI_U16)(4 +    /* 4 bytes for msg header */
                                 CFGOBJ_ID_SIZE +
                                 CFGOBJ_LEN_SIZE +
                                 CFGOBJ_ALIGN(req->length)) ;

    status = palAllocateMemory(hHdd, (void **)&msg, msgLen);
    if (status == eHAL_STATUS_SUCCESS)
    {
        if( fRsp )
        {
            msg->type = pal_cpu_to_be16(WNI_CFG_SET_REQ);
        }
        else
        {
            msg->type = pal_cpu_to_be16(WNI_CFG_SET_REQ_NO_RSP);
        }
        msg->msgLen = pal_cpu_to_be16(msgLen);
        (void)encodeCfgReq(hHdd, msg->data, req->cfgId, req->length, req->ccmPtr, req->ccmValue, req->type) ;

        status = palSendMBMessage(hHdd, msg) ;
        if (status != eHAL_STATUS_SUCCESS)
        {
            smsLog( pMac, LOGW, FL("palSendMBMessage() failed\n"));
            //No need to free msg. palSendMBMessage frees it.
            status = eHAL_STATUS_FAILURE ;
        }
    }
    else
    {
        smsLog( pMac, LOGW, FL("palAllocateMemory(len=%d)\n"), msgLen );
    }

    return status ;
}

static tCfgReq * allocateCfgReq(tHddHandle hHdd, tANI_U32 type, tANI_S32 length)
{
    tCfgReq *req ;
    tANI_S32 alloc_len = sizeof(tCfgReq) ;

    if (type == CCM_STRING_TYPE)
    {
        alloc_len += length ;
    }

    if (palAllocateMemory(hHdd, (void **)&req, alloc_len) != eHAL_STATUS_SUCCESS)
    {
        return NULL ;
    }

    req->ccmPtr = (req+1);

    return req ;
}

static void freeCfgReq(tHddHandle hHdd, tCfgReq *req)
{
    palFreeMemory(hHdd, (void*)req) ;
}

static void add_req_tail(tCfgReq *req, struct ccmlink *q)
{
    if (q->tail)
    {
        q->tail->next = req;
        q->tail = req ;
    }
    else
    {
        q->head = q->tail = req ;
    }
}

static void del_req(tCfgReq *req, struct ccmlink *q)
{
    q->head = req->next ;
    req->next = NULL ;
    if (q->head == NULL)
    {
        q->tail = NULL ;
    }
}

static void purgeReqQ(tHalHandle hHal)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tCfgReq *req, *tmp ;

    for (req = pMac->ccm.reqQ.head; req; req = tmp)
    {
        /* loop thru reqQ and invoke callback to return failure */
        smsLog(pMac, LOGW, FL("deleting cfgReq, cfgid=%d\n"), (int)req->cfgId);

        tmp = req->next ;

        if (req->callback)
        {
            req->callback(hHal, eHAL_STATUS_FAILURE);
        }
        palSpinLockTake(hHdd, pMac->ccm.lock);
        del_req(req, &pMac->ccm.reqQ);
        palSpinLockGive(hHdd, pMac->ccm.lock);
        freeCfgReq(hHdd, req);
    }
    return ;
}

static void sendQueuedReqToMacSw(tpAniSirGlobal pMac, tHddHandle hHdd)
{
    tCfgReq *req ;

    /* Send the head req */
    req = pMac->ccm.reqQ.head ;
    if (req)
    {
        if (req->state == eCCM_REQ_QUEUED)
        {
            /* Send WNI_CFG_SET_REQ */
            req->state = eCCM_REQ_SENT;
            if (sendCfg(pMac, hHdd, req, eANI_BOOLEAN_TRUE) != eHAL_STATUS_SUCCESS)
            {
                smsLog( pMac, LOGW, FL("sendCfg() failed\n"));
                palSpinLockTake(hHdd, pMac->ccm.lock);
                del_req(req, &pMac->ccm.reqQ) ;
                palSpinLockGive(hHdd, pMac->ccm.lock);
                if (req->callback)
                {
                    req->callback((tHalHandle)pMac, WNI_CFG_OTHER_ERROR) ;
                }

#ifdef CCM_DEBUG
                smsLog(pMac, LOGW, FL("ccmComplete(%p)\n"), req->done);
#endif
                ccmComplete(hHdd, req->done);

                freeCfgReq(hHdd, req);
            }
        }
        else
        {
            smsLog( pMac, LOGW, FL("reqState is not eCCM_REQ_QUEUED, is %d\n"), req->state );
        }
    }

    return ;
}

static eHalStatus cfgSetSub(tpAniSirGlobal pMac, tHddHandle hHdd, tANI_U32 cfgId, tANI_U32 type, 
                            tANI_S32 length, void *ccmPtr, tANI_U32 ccmValue, 
                            tCcmCfgSetCallback callback, eAniBoolean toBeSaved, void *sem, tCfgReq **r)
{
    eHalStatus status = eHAL_STATUS_SUCCESS;
    tCfgReq *req ;

    do
    {
        *r = NULL ;

        if (pMac->ccm.state == eCCM_STOPPED)
        {
            status = eHAL_STATUS_FAILURE ;
            break ;
        }

        req = allocateCfgReq(hHdd, type, length);
        if (req == NULL)
        {
            status = eHAL_STATUS_FAILED_ALLOC ;
            break ;
        }

        req->next       = NULL ;
        req->cfgId      = (tANI_U16)cfgId ;
        req->type       = (tANI_U8)type ;
        req->state      = eCCM_REQ_QUEUED ;
        req->toBeSaved  = !!toBeSaved ;
        req->length     = length ;
        req->done       = sem ;
        req->callback   = callback ;
        if (type == CCM_INTEGER_TYPE)
        {
            req->ccmValue = ccmValue ;
        }
        else
        {
            palCopyMemory(hHdd, (void*)req->ccmPtr, (void*)ccmPtr, length); 
        }

        palSpinLockTake(hHdd, pMac->ccm.lock);

        add_req_tail(req, &pMac->ccm.reqQ);
        /* If this is the first req on the queue, send it to MAC SW */
        if ((pMac->ccm.replay.started == 0) && (pMac->ccm.reqQ.head == req))
        {
            /* Send WNI_CFG_SET_REQ */
            req->state = eCCM_REQ_SENT;
            palSpinLockGive(hHdd, pMac->ccm.lock);
            status = sendCfg(pMac, hHdd, req, eANI_BOOLEAN_TRUE) ;
            if (status != eHAL_STATUS_SUCCESS)
            {
                smsLog( pMac, LOGW, FL("sendCfg() failed\n"));
                palSpinLockTake(hHdd, pMac->ccm.lock);
                del_req(req, &pMac->ccm.reqQ);
                palSpinLockGive(hHdd, pMac->ccm.lock);
                freeCfgReq(hHdd, req);
                break ;
            }
            else
            {
                palSpinLockTake(hHdd, pMac->ccm.lock);
                if(req != pMac->ccm.reqQ.head)
                {
                    //We send the request and it must be done already
                    req = NULL;
                }
                palSpinLockGive(hHdd, pMac->ccm.lock);
            }
        }
        else
        {
            palSpinLockGive(hHdd, pMac->ccm.lock);
        }
        *r = req ;

    } while(0) ;

    return status;
}

static eHalStatus cfgSet(tHalHandle hHal, tANI_U32 cfgId, tANI_U32 type, tANI_S32 length, void * ccmPtr, tANI_U32 ccmValue, tCcmCfgSetCallback callback, eAniBoolean toBeSaved)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status;
    tCfgReq *req ;

    if (pal_in_interrupt())
    {
#ifdef CCM_DEBUG2
        smsLog(pMac, LOGE, FL("WNI_CFG_%s (%d 0x%x), in_interrupt()=TRUE\n"), gCfgParamName[cfgId], (int)cfgId, (int)cfgId);
#endif
        status = cfgSetSub(pMac, hHdd, cfgId, type, length, ccmPtr, ccmValue, callback, toBeSaved, NULL, &req);
    }
    else
    {
        void *sem ;

#ifdef CCM_DEBUG2
        smsLog(pMac, LOGE, FL("WNI_CFG_%s (%d 0x%x), in_interrupt()=FALSE\n"), gCfgParamName[cfgId], (int)cfgId, (int)cfgId);
#endif
        pal_local_bh_disable() ;

        status = palMutexAllocLocked( hHdd, &sem ) ;
        if (status != eHAL_STATUS_SUCCESS)
        {
            smsLog(pMac, LOGE, FL("mutex alloc failed\n"));
            sem = NULL;
        }
        else
        {
            status = cfgSetSub(pMac, hHdd, cfgId, type, length, ccmPtr, ccmValue, callback, toBeSaved, sem, &req);
            if ((status != eHAL_STATUS_SUCCESS) || (req == NULL))
            {
                //Either it fails to send or the req is finished already
                palSemaphoreFree( hHdd, sem );
                sem = NULL;
            }
        }

        pal_local_bh_enable() ;

        if ((status == eHAL_STATUS_SUCCESS) && (sem != NULL))
        {
#ifdef CCM_DEBUG
            smsLog(pMac, LOGW, FL("ccmWaitForCompletion(%p)\n"), req->done);
#endif
            ccmWaitForCompletion(hHdd, sem);

#ifdef CCM_DEBUG
            smsLog(pMac, LOGW, FL("free(%p)\n"), req->done);
#endif
            palSemaphoreFree( hHdd, sem ) ;
        }
    }

    return status ;
}

eHalStatus ccmOpen(tHalHandle hHal)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    (void)palZeroMemory(hHdd, &pMac->ccm, sizeof(tCcm)) ;
    return palSpinLockAlloc(hHdd, &pMac->ccm.lock);
}

eHalStatus ccmClose(tHalHandle hHal)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tANI_U32 i ; 
    tCfgReq *req ;

    ccmStop(hHal);

    /* Go thru comp[] to free all saved requests */
    for (i = 0 ; i < CFG_PARAM_MAX_NUM ; ++i)
    {
        if ((req = pMac->ccm.comp[i]) != NULL)
        {
            freeCfgReq(hHdd, req);
        }
    }

    return palSpinLockFree(hHdd, pMac->ccm.lock);
}

/* This function executes in (Linux) softirq context */
void ccmCfgCnfMsgHandler(tHalHandle hHal, void *m)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tSirMbMsg *msg = (tSirMbMsg *)m ;
    tANI_U32 result, cfgId ;
    tCfgReq *req, *old ;

#if 0
    if (pMac->ccm.state != eCCM_STARTED)
    {
        return ;
    }
#endif

    result  = pal_be32_to_cpu(msg->data[0]);
    cfgId   = pal_be32_to_cpu(msg->data[1]);

    if (pMac->ccm.replay.started && cfgId == CFG_UPDATE_MAGIC_DWORD)
    {
        pMac->ccm.replay.in_progress = 1 ;
        return ;
    }

    if (pMac->ccm.replay.in_progress)
    {
        /* save error code */
        if (!CCM_IS_RESULT_SUCCESS(result))
        {
            pMac->ccm.replay.result = result ;
        }

        if (--pMac->ccm.replay.nr_param == 0)
        {
            pMac->ccm.replay.in_progress = 0 ;

            if (pMac->ccm.replay.callback)
            {
                pMac->ccm.replay.callback(hHal, pMac->ccm.replay.result);
            }

            pMac->ccm.replay.started = 0 ;

            /* Wake up the sleeping process */
#ifdef CCM_DEBUG
            smsLog(pMac, LOGW, FL("ccmComplete(%p)\n"), pMac->ccm.replay.done);
#endif
            ccmComplete(hHdd, pMac->ccm.replay.done);
            //Let go with the rest of the set CFGs waiting.
            sendQueuedReqToMacSw(pMac, hHdd);
        }
    }
    else
    {
        /*
         * Try to match this response with the request.
         * What if i could not find the req entry ???
         */
        req = pMac->ccm.reqQ.head ;
        if (req)
        {

            if (req->cfgId == cfgId && req->state == eCCM_REQ_SENT)
            {
                palSpinLockTake(hHdd, pMac->ccm.lock);
                del_req(req, &pMac->ccm.reqQ);
                palSpinLockGive(hHdd, pMac->ccm.lock);
                req->state = eCCM_REQ_DONE ;

                if (result == WNI_CFG_NEED_RESTART ||
                    result == WNI_CFG_NEED_RELOAD)
                {
#ifdef CCM_DEBUG
                    smsLog(pMac, LOGW, FL("need restart/reload, cfgId=%d\n"), req->cfgId) ;
#endif
                    //purgeReqQ(hHal);
                }

                /* invoke callback */
                if (req->callback)
                {
#ifdef CCM_DEBUG
                    req->callback(hHal, cfgId) ;
#else
                    req->callback(hHal, result) ;
#endif
                }

                /* Wake up the sleeping process */
#ifdef CCM_DEBUG
                smsLog(pMac, LOGW, FL("cfgId=%ld, calling ccmComplete(%p)\n"), cfgId, req->done);
#endif
                ccmComplete(hHdd, req->done);

                /* move the completed req from reqQ to comp[] */
                if (req->toBeSaved && (CCM_IS_RESULT_SUCCESS(result)))
                {
                    if ((old = pMac->ccm.comp[cfgId]) != NULL)
                    {
                        freeCfgReq(hHdd, old) ;
                    }
                    pMac->ccm.comp[cfgId] = req ;
                }
                else
                {
                    freeCfgReq(hHdd, req) ;
                }
                sendQueuedReqToMacSw(pMac, hHdd);
            }
            else
            {
                smsLog( pMac, LOGW, FL("can not match RSP with REQ, rspcfgid=%d result=%d reqcfgid=%d reqstate=%d\n"),
                        (int)cfgId, (int)result, req->cfgId, req->state);

#ifdef CCM_DEBUG
                smsLog(pMac, LOGW, FL("ccmComplete(%p)\n"), req->done);
#endif
            }

        }
    }

    return ;
}

void ccmStart(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    pMac->ccm.state = eCCM_STARTED ;

#if defined(ANI_LOGDUMP)
    ccmDumpInit(hHal);
#endif //#if defined(ANI_LOGDUMP)

    return ;
}

void ccmStop(tHalHandle hHal)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

    pMac->ccm.state = eCCM_STOPPED ;

    pal_local_bh_disable() ;
    purgeReqQ(hHal);
    pal_local_bh_enable() ;

    return ;
}

eHalStatus ccmCfgSetInt(tHalHandle hHal, tANI_U32 cfgId, tANI_U32 ccmValue, tCcmCfgSetCallback callback, eAniBoolean toBeSaved)
{
    if( callback || toBeSaved)
    {
        //we need to sychronous this one
        return cfgSet(hHal, cfgId, CCM_INTEGER_TYPE, sizeof(tANI_U32), NULL, ccmValue, callback, toBeSaved);
    }
    else
    {
        //Simply push to CFG and not waiting for the response
        tCfgReq req;
        tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

        req.callback = NULL;
        req.next = NULL;
        req.cfgId = ( tANI_U16 )cfgId;
        req.length = sizeof( tANI_U32 );
        req.type = CCM_INTEGER_TYPE;
        req.ccmPtr = NULL;
        req.ccmValue = ccmValue;
        req.toBeSaved = toBeSaved;
        req.state = eCCM_REQ_SENT;

        return ( sendCfg( pMac, pMac->hHdd, &req, eANI_BOOLEAN_FALSE ) );
    }
}

eHalStatus ccmCfgSetStr(tHalHandle hHal, tANI_U32 cfgId, tANI_U8 *pStr, tANI_U32 length, tCcmCfgSetCallback callback, eAniBoolean toBeSaved)
{
    if( callback || toBeSaved )
    {
        //we need to sychronous this one
        return cfgSet(hHal, cfgId, CCM_STRING_TYPE, length, pStr, 0, callback, toBeSaved);
    }
    else
    {
        //Simply push to CFG and not waiting for the response
        tCfgReq req;
        tpAniSirGlobal pMac = PMAC_STRUCT( hHal );

        req.callback = NULL;
        req.next = NULL;
        req.cfgId = ( tANI_U16 )cfgId;
        req.length = length;
        req.type = CCM_STRING_TYPE;
        req.ccmPtr = pStr;
        req.ccmValue = 0;
        req.toBeSaved = toBeSaved;
        req.state = eCCM_REQ_SENT;

        return ( sendCfg( pMac, pMac->hHdd, &req, eANI_BOOLEAN_FALSE ) );
    }
}

eHalStatus ccmCfgGetInt(tHalHandle hHal, tANI_U32 cfgId, tANI_U32 *pValue)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status = eHAL_STATUS_SUCCESS ;
    tCfgReq *req = pMac->ccm.comp[cfgId] ;

    if (req && req->state == eCCM_REQ_DONE)
    {
        *pValue = req->ccmValue ; 
    }
    else
    {
        if (wlan_cfgGetInt(pMac, (tANI_U16)cfgId, pValue) != eSIR_SUCCESS)
            status = eHAL_STATUS_FAILURE;
    }

    return status ;
}

eHalStatus ccmCfgGetStr(tHalHandle hHal, tANI_U32 cfgId, tANI_U8 *pBuf, tANI_U32 *pLength)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    eHalStatus status = eHAL_STATUS_SUCCESS ;
    tCfgReq *req = pMac->ccm.comp[cfgId] ;

    if (req && req->state == eCCM_REQ_DONE && (tANI_U32)req->length <= *pLength)
    {
        *pLength = req->length ; 
        palCopyMemory(hHdd, (void*)pBuf, (void*)req->ccmPtr, req->length); 
    }
    else
    {
        if (wlan_cfgGetStr(pMac, (tANI_U16)cfgId, pBuf, pLength) != eSIR_SUCCESS)
            status = eHAL_STATUS_FAILURE;
    }

    return status ;
}

/*
 * Loop thru comp[] and form an ANI message which contains all completed cfgIds.
 * The message begins with an INTEGER parameter (cfgId=CFG_UPDATE_MAGIC_DWORD)     
 * to mark the start of the message.
 */ 
static eHalStatus cfgUpdate(tpAniSirGlobal pMac, tHddHandle hHdd, tCcmCfgSetCallback callback)
{
    tANI_U32 i, *pl ;
    tCfgReq *req ;
    tSirMbMsg *msg ;
    eHalStatus status ;
    tANI_S16 msgLen = 4 +       /* 4 bytes for msg header */ 
                                /* for CFG_UPDATE_MAGIC_DWORD */ 
        CFGOBJ_ID_SIZE +
        CFGOBJ_LEN_SIZE +
        CFGOBJ_INTEGER_VALUE_SIZE ;

    if (pMac->ccm.state == eCCM_STOPPED || pMac->ccm.replay.started)
    {
        status = eHAL_STATUS_FAILURE ;
        goto end ;
    }

    palSpinLockTake(hHdd, pMac->ccm.lock);

    pMac->ccm.replay.started    = 1 ;
    pMac->ccm.replay.nr_param   = 0 ;

    palSpinLockGive(hHdd, pMac->ccm.lock);

    /* Calculate message length */
    for (i = 0 ; i < CFG_PARAM_MAX_NUM ; ++i)
    {
        if ((req = pMac->ccm.comp[i]) != NULL)
        {
            msgLen += (tANI_S16)(CFGOBJ_ID_SIZE + CFGOBJ_LEN_SIZE + CFGOBJ_ALIGN(req->length)) ;
            pMac->ccm.replay.nr_param += 1 ;
#ifdef CCM_DEBUG
            smsLog(pMac, LOGW, FL("cfgId=%d\n"), req->cfgId);
#endif
        }
    }

    if (pMac->ccm.replay.nr_param == 0)
    {
        if (callback)
        {
            callback((tHalHandle)pMac, WNI_CFG_SUCCESS) ;
        }
        status = eHAL_STATUS_SUCCESS ;
        goto end ;
    }

    pMac->ccm.replay.in_progress = 0 ;
    pMac->ccm.replay.result      = WNI_CFG_SUCCESS ;
    pMac->ccm.replay.callback    = callback ;
    pMac->ccm.replay.done        = NULL ;

    status = palAllocateMemory(hHdd, (void **)&msg, msgLen) ;
    if (status != eHAL_STATUS_SUCCESS)
    {
        pMac->ccm.replay.started = 0 ;
        goto end ; 
    }

    msg->type   = pal_cpu_to_be16(WNI_CFG_SET_REQ);
    msg->msgLen = pal_cpu_to_be16(msgLen);

    /* Encode the starting cfgId */
    pl = encodeCfgReq(hHdd, msg->data, CFG_UPDATE_MAGIC_DWORD, 4, NULL, 0, CCM_INTEGER_TYPE) ;

    /* Encode the saved cfg requests */
    for (i = 0 ; i < CFG_PARAM_MAX_NUM ; ++i)
    {
        if ((req = pMac->ccm.comp[i]) != NULL)
        {
            pl = encodeCfgReq(hHdd, pl, req->cfgId, req->length, req->ccmPtr, req->ccmValue, req->type) ;
        }
    }

    status = palSendMBMessage(hHdd, msg) ;
    if (status != eHAL_STATUS_SUCCESS)
    {
        smsLog(pMac, LOGW, FL("palSendMBMessage() failed. status=%d\n"), status);
        pMac->ccm.replay.started = 0 ;
        //No need to free msg. palSendMBMessage frees it.
        goto end ;
    }

 end:
    return status ;
}

eHalStatus ccmCfgUpdate(tHalHandle hHal, tCcmCfgSetCallback callback)
{
    tHddHandle hHdd = halHandle2HddHandle(hHal);
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    eHalStatus status ;

    pal_local_bh_disable() ;

    status = cfgUpdate(pMac, hHdd, callback) ;
    if (status == eHAL_STATUS_SUCCESS)
    {
        if (pMac->ccm.replay.nr_param == 0)
        {
            /* there is nothing saved at comp[], so we are done! */
            pMac->ccm.replay.started = 0 ;
        }
        else
        {
            /* we have sent update message to MAC SW */
            void *sem ;

            status = palMutexAllocLocked( hHdd, &sem ) ;
            if (status != eHAL_STATUS_SUCCESS)
            {
                smsLog(pMac, LOGE, FL("mutex alloc failed\n"));
                pMac->ccm.replay.started = 0 ;
            }
            else
            {
                pMac->ccm.replay.done = sem ;
            }
        }
    }

    pal_local_bh_enable() ;

    /* Waiting here ... */
    if (status == eHAL_STATUS_SUCCESS && pMac->ccm.replay.done)
    {
#ifdef CCM_DEBUG
        smsLog(pMac, LOGW, FL("ccmWaitForCompletion(%p)\n"), pMac->ccm.replay.done);
#endif
        ccmWaitForCompletion(hHdd, pMac->ccm.replay.done);

#ifdef CCM_DEBUG
        smsLog(pMac, LOGW, FL("free(%p)\n"), pMac->ccm.replay.done);
#endif
        palSemaphoreFree( hHdd, pMac->ccm.replay.done) ;
    }

    return status ;
}

#if 0
///////////////////////////////////////////////////////////////////
#include <linux/netdevice.h>
#include <linux/delay.h>
extern struct net_device * hdd_dev[];

typedef struct pal_netdev_priv_s
{
    // pointer to the PCI device structure for this device
    struct pci_dev  *pci_dev;

    // TAURUS has three BAR registers

    // BAR0 is a fixed window for the PIF registers
    volatile void *win0_addr;
    tANI_U32 win0_size;

    // BAR1 is a movable window for all other registers
    volatile void *win1_addr;
    tANI_U32 win1_size;
    tANI_U32 win1_current;

    // BAR2 is a movable window for all other memory
    volatile void *win2_addr;
    tANI_U32 win2_size;
    tANI_U32 win2_current;


} pal_netdev_priv_t;

typedef struct hdd_stats_s {
    /* Stats on the MAC SW messages sent to applications */
    ulong stats_mac_rx_mbx_tot_cnt;
    ulong stats_mac_rx_mbx_success_cnt;
    ulong stats_mac_rx_mbx_drop_cnt;

    /* Stats on messages sent to the MAC SW messages from applications */
    ulong stats_mac_tx_mbx_tot_cnt;
    ulong stats_mac_tx_mbx_success_cnt;
    ulong stats_mac_tx_mbx_drop_cnt;
} hdd_stats_t;

/* HDD Driver Private Data structure */
typedef struct hdd_netdev_priv_s
{
    tANI_U32 magic_head;

    // chipset-specific private data
    pal_netdev_priv_t ANI_CHIPSET;

    tHalHandle halHandle;

#ifdef ANI_BUS_TYPE_PCI
    struct pci_dev  *pci_dev;
    struct pci_device_id * pPciDevId;
#endif // ANI_BUS_TYPE_PCI

    // Queued EAPOL frame destination.
    tANI_U32 eapol_pid;

    unsigned int num_xmit;

    /*
     * Various frequently used variables that pertain to this
     * instance of the driver
     */
    tANI_U32 rx_buf_sz;           /* Based on MTU+extra headroom needed.*/
    tANI_U32 td_enqueue_nested;

    /*
     * Flag set by MAC SW to indicate a TD ring is desired
     */
    int td_flush;

    int selectiveFlush;

    spinlock_t lock;

    /* Stats */
    struct net_device_stats stats;
    int curr_acc_cat;
#ifdef LX5280
    unsigned short rtl_pvid; //VLAN id this Interface belongs to
    int rtl_extPortNum; //ext port used in RTL865x driver
    int rtl_linkId[16];//link ID of each interface for RTL865x driver
    int rtl_wdsActive;
#endif
    tANI_U16 lport; /* switch logical port */

    /* management and control */
    tANI_U32 status;
    tANI_U32 msg_enable;
    tANI_U32 radio_id;                  /* Unit # of this device */

    int ap_flag;                        /* 
                                         * indicates if the Radio is in AP
                                         *  or BP mode.
                                         */

    /* NAPI Polling suport */
    struct timer_list oom_timer;    /* Out of memory timer.    */

    struct timer_list reset_req_timer;/* 
                                       * Timer started when a Reset
                                       * request is sent to WSM. Cleared
                                       * when a subsequent Radio Disable
                                       * Request is received.
                                       */

    struct tasklet_struct *rx_tasklet;
    struct tasklet_struct *tx_tasklet;
       
    tANI_U32 learn_mode_frame_cnt;      /*
                                         * Data Frames forwarded to MAC SW
                                         * when Polaris is in learn mode 
                                         */

    tANI_U32 mgmt_ctl_frames;           /* MGMT/CTL Frames forwarded to MAC SW */

    tANI_U32 nir;                       /* total number of times the ISR has
                                         * been invoked.*/
    tANI_U32 stats_dummy_pkt_requeue_cnt;
    tANI_U32 stats_rx_td_dummy_pkt_cnt;/* TD Dummy pkts that were after HIF loopback */
    tANI_U32 stats_rx_tm_dummy_pkt_cnt;/* TM Dummy pkts that were after HIF loopback */
    tANI_U32 stats_td_dummy_pkt_cnt;    /* Dummy pkts that were succesfully
                                         * put on the TD ring and that
                                         * were picked up by the HIF
                                         */

    tANI_U32 stats_mac_dummy_pkt_drop_cnt;/* Number of dummy pkts dropped by the HDD
                                           * due to any reason
                                           */
    tANI_U32 stats_wns_l2update_cnt; 
    tANI_U32 stats_app_hif_wr_pkt_cnt;
    hdd_stats_t hdd_stats;

    tANI_U32 stats_reset_req_timer_cnt; /*
                                         * Number of times the 
                                         * Reset Req Timer expired
                                         */

#ifdef TCP_PROFILE
    unsigned int pv_current_ip_proto;
    unsigned int pv_current_ip_byte;
    unsigned int pv_current_ack_seq;
    unsigned int pv_current_seq;
    unsigned int pv_rtt;
    unsigned int pv_sent_seq;
    unsigned int pv_p_ts;
    unsigned int pv_tfpl_ts;
#endif
    tANI_U32 stats_mac_reset_cnt;         /* MAC SW Reset Requests */
    tANI_U32 stats_mac_reset_eof_sof;
    tANI_U32 stats_mac_reset_bmu;
    tANI_U32 stats_mac_reset_pdu_low;
    tANI_U32 stats_mac_reset_user;
    tANI_U32 stats_mac_reset_wd_timeout;
    tANI_U32 stats_mac_reset_unspecified;

    tANI_U32 stats_wd_timeout_cnt;
    tANI_U32 stats_radio_enable_cnt;
    tANI_U32 stats_radio_disable_cnt;

#ifdef PLM_EXTRA_STATS
    tANI_U32 stats_tx_xmit_refilled;         /* Pkts xmit-filled */
    tANI_U32 stats_tx_queue_stop;
    tANI_U32 stats_tx_queue_start;

    tANI_U32 stats_alloc_fail;
    tANI_U32 stats_poll_starts;
    tANI_U32 stats_poll_pkts;
    tANI_U32 stats_poll_exit_done;
    tANI_U32 stats_poll_exit_not_done;
    tANI_U32 stats_poll_exit_oom;
    tANI_U32 stats_poll_exit_done_rx_pending;
    tANI_U32 stats_poll_zero_rx;

#ifdef CONFIG_PROC_FS
#ifdef ANI_USE_TASKLET
    struct proc_dir_entry *proc_driver_dir; // for /proc/net/drivers
#endif
    struct proc_dir_entry *proc_ent_dir;    // for the directory itself

    struct proc_dir_entry *proc_ent_stats;
    struct proc_dir_entry *proc_ent_np_dump;
    struct proc_dir_entry *proc_ent_ring;
    char proc_fname_stats[32];
    char proc_fname_np_dump[32];
    char proc_fname_ring[32];

    /* Setting Debug levels */
    struct proc_dir_entry * proc_ent_dbg;
    char proc_fname_dbg[32];

    /* For bypass flags */
    struct proc_dir_entry * proc_ent_bypass;
    char proc_fname_bypass[32];

    int sir_dump_cmd;   // Dump SIR command
    int sir_dump_arg1;  // Dump SIR argument 1
    int sir_dump_arg2;  // Dump SIR argument 2
    int sir_dump_arg3;  // Dump SIR argument 3
    int sir_dump_arg4;  // Dump SIR argument 4
    struct proc_dir_entry * proc_ent_sir_dump;
    char proc_fname_sir_dump[32];

    eHalStatus status ;
    struct proc_dir_entry * proc_ent_eeprom_info;
    char proc_fname_eeprom_info[32];

#endif  /* ifdef CONFIG_PROC_FS */

    tANI_U32 rx_small_skb_failure;

    unsigned long open_time; /* jiffies for last open */
#endif /* PLM_EXTRA_STATS */

    int mac_down;
    tANI_U32 rx_mac_msg_cnt;
    tANI_U32 tx_mac_msg_cnt;

    int mbx_sent;

    tANI_U32 bypass;                    // Used to various types of bypasses
                                        // in the driver

    /* 
     * this table is initialized once for all by poldrv and so is not in 
     * mac_param struct 
     */
    t_mac_block_table * mac_block_table;
    struct sk_buff_head mac_list;
#if  defined(ASICDXE_PROFILE) && defined(LX5280)
    tANI_U32 num_of_reg_switches;
#endif
    tANI_U32 magic_tail;
} hdd_netdev_priv_t;

static void ccm_callback(tHalHandle hHal, tANI_S32 cfgId)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    smsLog(pMac, LOGW, FL("cfgId = %d\n"), cfgId);
}

static void ccm_callback2(tHalHandle hHal, tANI_S32 result)
{
    tpAniSirGlobal pMac = PMAC_STRUCT( hHal );
    smsLog(pMac, LOGW, FL("result = %d\n"), result);
}

void ccm_test(void)
{
    tHalHandle hHal ;
    tpAniSirGlobal pMac;
    eHalStatus status ;
    struct net_device *dev;
    hdd_netdev_priv_t *np;
    char str[80] = {1} ;

    dev  = hdd_dev[0];
    np = (hdd_netdev_priv_t *)dev->priv;
    hHal = np->halHandle ;
    pMac = PMAC_STRUCT( hHal );

    smsLog(pMac, LOGW, "ccmStart()\n");
    ccmStart(hHal) ;

    status = ccmCfgUpdate(hHal, ccm_callback2) ;
    smsLog(pMac, LOGW, "ccmCfgUpdate(): status=%d\n", status);

    status = ccmCfgSetInt(hHal, WNI_CFG_FRAGMENTATION_THRESHOLD, 100, ccm_callback, 1) ;
    smsLog(pMac, LOGW, "ccmCfgSetInt(WNI_CFG_FRAGMENTATION_THRESHOLD = %d): status=%d\n",
           WNI_CFG_FRAGMENTATION_THRESHOLD, status);

    status = ccmCfgSetInt(hHal, WNI_CFG_RTS_THRESHOLD, 100, ccm_callback, 1) ;
    smsLog(pMac, LOGW, "ccmCfgSetInt(WNI_CFG_RTS_THRESHOLD = %d): status=%d\n",
           WNI_CFG_RTS_THRESHOLD, status);

    /* this cfgid causes reload */
    status = ccmCfgSetInt(hHal, WNI_CFG_MIMO_ENABLED, 1, ccm_callback, 1) ;
    smsLog(pMac, LOGW, "ccmCfgSetInt(WNI_CFG_MIMO_ENABLED = %d): status=%d\n",
           WNI_CFG_MIMO_ENABLED, status);

    status = ccmCfgSetInt(hHal, WNI_CFG_SHORT_RETRY_LIMIT, 100, ccm_callback, 1) ;
    smsLog(pMac, LOGW, "ccmCfgSetInt(WNI_CFG_SHORT_RETRY_LIMIT = %d): status=%d\n",
           WNI_CFG_SHORT_RETRY_LIMIT, status);

    status = ccmCfgSetInt(hHal, WNI_CFG_LONG_RETRY_LIMIT, 100, ccm_callback, 1) ;
    smsLog(pMac, LOGW, "ccmCfgSetInt(WNI_CFG_LONG_RETRY_LIMIT = %d): status=%d\n",
           WNI_CFG_LONG_RETRY_LIMIT, status);

    /* this cfgid causes restart */
    status = ccmCfgSetStr(hHal, WNI_CFG_EDCA_WME_ACVI, str, sizeof(str), ccm_callback, 1) ;
    smsLog(pMac, LOGW, "ccmCfgSetStr(WNI_CFG_EDCA_WME_ACVI = %d): status=%d\n",
           WNI_CFG_EDCA_WME_ACVI, status);

    mdelay(100);

    smsLog(pMac, LOGW, "ccmStop()\n");
    ccmStop(hHal);
    status = ccmCfgUpdate(hHal, ccm_callback2) ;
    smsLog(pMac, LOGW, "ccmCfgUpdate(): status=%d\n", status);

    smsLog(pMac, LOGW, "ccmStart()\n");
    ccmStart(hHal) ;
    status = ccmCfgUpdate(hHal, ccm_callback2) ;
    smsLog(pMac, LOGW, "ccmCfgUpdate(): status=%d\n", status);
}
#endif
