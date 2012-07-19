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

/*============================================================================
Copyright (c) 2007 QUALCOMM Incorporated.
All Rights Reserved.
Qualcomm Confidential and Proprietary

logDump.c
*/

/*
 * Woodside Networks, Inc proprietary. All rights reserved.
 * This file contains the utility functions to dump various
 * MAC states and to enable/disable certain features during
 * debugging.
 * Author:        Sandesh Goel
 * Date:          02/27/02
 * History:-
 * 02/11/02       Created.
 * --------------------------------------------------------------------
 *
 */

/* 
 * @note : Bytes is to print overflow message information.
 */

#include "palTypes.h"

#ifdef ANI_LOGDUMP

#define MAX_OVERFLOW_MSG    400
#if defined(ANI_OS_TYPE_WINDOWS)
#define MAX_LOGDUMP_SIZE    ((4*1024) - MAX_OVERFLOW_MSG)
#else
#define MAX_LOGDUMP_SIZE    ((4*1024) - MAX_OVERFLOW_MSG)
#endif

#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halDebug.h"
#include "halInterrupts.h"
#endif
#if defined (ANI_OS_TYPE_LINUX)

#include <sysDebug.h>

#elif defined(ANI_OS_TYPE_WINDOWS)

#include "stdarg.h"
#include "sirTypes.h"


#ifdef ANI_DVT_DEBUG
#include "dvtModuleApi.h"
#endif

#include "pmmApi.h"
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halInternal.h"
#endif
#include "csrApi.h"

#elif defined(ANI_OS_TYPE_OSX)

#include "stdarg.h"

#elif defined(ANI_OS_TYPE_AMSS)

#include "comdef.h"
#include "string.h"

#include "AEEstd.h"

#include "sirTypes.h"
#include "halInterrupts.h"

#include "pmmApi.h"
#include "halInternal.h"
#include "csrApi.h"

#elif defined(ANI_OS_TYPE_ANDROID)

#include <linux/kernel.h>

#endif


#include "palApi.h"
#include "aniGlobal.h"
#include "sirCommon.h"
#include <sirDebug.h>
#include <utilsApi.h>

#include <limApi.h>
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include <halCommonApi.h>
#endif
#include <cfgApi.h>
#include <utilsGlobal.h>
#include <dphGlobal.h>
#include <limGlobal.h>
#include "limUtils.h"
#include "schApi.h"

#include "pmmApi.h"
#include "limSerDesUtils.h"
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halLogDump.h"
//#include "testHalMsgApi.h"
#include "halMailbox.h"
#include "halRxp.h"
#include "halMTU.h"
#include "halPhyApi.h"
#endif
#include "limAssocUtils.h"
#include "limSendMessages.h"
#ifdef FEATURE_WLAN_NON_INTEGRATED_SOC
#include "halUtils.h"
#endif
#include "limSecurityUtils.h"
//#include "halRadar.h"
#include "logDump.h"
#include "sysDebug.h"
#include "wlan_qct_wda.h"

#define HAL_LOG_DUMP_CMD_START 0
#define HAL_LOG_DUMP_CMD_END 299

static int debug = 0;

    void
logPrintf(tpAniSirGlobal pMac, tANI_U32 cmd, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4)
{
    static tANI_U8 buf[MAX_LOGDUMP_SIZE + MAX_OVERFLOW_MSG];
    tANI_U16 bufLen;
    pMac->gCurrentLogSize = 0;

    bufLen = (tANI_U16)logRtaiDump(pMac, cmd, arg1, arg2, arg3, arg4, buf);
}

/**
  @brief: This function is used to Aggregate the formated buffer, this
  also check the overflow condition and adds the overflow message
  to the end of the log Dump buffer reserved of MAX_OVERFLOW_MSG size.
  @param: tpAniSirGlobal pMac
  @param: char *pBuf
  @param: variable arguments...
  @return: Returns the number of bytes added to the buffer.
  Returns 0 incase of overflow.

  @note: Currently in windows we do not print the Aggregated buffer as there
  is a limitation on the number of bytes that can be displayed by DbgPrint
  So we print the buffer immediately and we would also aggregate where
  the TestDbg might use this buffer to print out at the application level.
  */
int log_sprintf(tpAniSirGlobal pMac, char *pBuf, char *fmt, ...)
{
    tANI_S32 ret = 0;
#ifdef WLAN_DEBUG

#if defined(ANI_OS_TYPE_AMSS)
    AEEVaList args;
    AEEVA_START(args, fmt);
#else
    va_list args;
    va_start(args, fmt);
#endif

    if (pMac->gCurrentLogSize >= MAX_LOGDUMP_SIZE)
        return 0;

#if defined (ANI_OS_TYPE_WINDOWS)
    ret = _vsnprintf(pBuf, (MAX_LOGDUMP_SIZE - pMac->gCurrentLogSize), fmt, args);
#elif (defined (ANI_OS_TYPE_LINUX) || defined (ANI_OS_TYPE_ANDROID))
    ret = vsnprintf(pBuf, (MAX_LOGDUMP_SIZE - pMac->gCurrentLogSize), fmt, args);
#elif defined (ANI_OS_TYPE_OSX)
    ret = vsnprintf(pBuf, (MAX_LOGDUMP_SIZE - pMac->gCurrentLogSize), fmt, args);
    /* BSD kernel has a bug that vsnprintf() always return 0.
     * See bsd/kern/subr_prf.c 
     * Need to verify ...
     */
    if (ret >= 0)
        ret = strlen(pBuf);
#elif defined (ANI_OS_TYPE_AMSS)
    ret = std_vstrlprintf(pBuf, (MAX_LOGDUMP_SIZE - pMac->gCurrentLogSize), fmt, args);
#endif

#if defined(ANI_OS_TYPE_AMSS)
    AEEVA_END(args);
#else
    va_end(args);
#endif

    /* If an output error is encountered, a negative value is returned by vsnprintf */
    if (ret < 0)
        return 0;


    if ((tANI_U32) ret > (MAX_LOGDUMP_SIZE - pMac->gCurrentLogSize)) {
        pBuf += (MAX_LOGDUMP_SIZE - pMac->gCurrentLogSize);
        pMac->gCurrentLogSize = MAX_LOGDUMP_SIZE;

#if defined (ANI_OS_TYPE_WINDOWS)
        ret = _snprintf(pBuf, MAX_OVERFLOW_MSG, "\n-> ***********"
                "\nOutput Exceeded the Buffer Size, message truncated!!\n<- ***********\n");
#elif (defined (ANI_OS_TYPE_LINUX) || defined (ANI_OS_TYPE_ANDROID))
        ret = snprintf(pBuf, MAX_OVERFLOW_MSG, "\n-> ***********"
                "\nOutput Exceeded the Buffer Size, message truncated!!\n<- ***********\n");
#elif defined (ANI_OS_TYPE_OSX)
        ret = snprintf(pBuf, MAX_OVERFLOW_MSG, "\n-> ***********"
                "\nOutput Exceeded the Buffer Size, message truncated!!\n<- ***********\n");
        /* BSD kernel has a bug that snprintf() always return 0.
         * See bsd/kern/subr_prf.c 
         * but NEED TO VERIFY ...
         */
        if (ret >= 0)
            ret = strlen(pBuf);
#elif defined (ANI_OS_TYPE_AMSS)
        ret = snprintf(pBuf, MAX_OVERFLOW_MSG, "\n-> ***********"
                "\nOutput Exceeded the Buffer Size, message truncated!!\n<- ***********\n");
#endif
        /* If an output error is encountered, a negative value is returned by snprintf */
        if (ret < 0)
            return 0;

        if (ret > MAX_OVERFLOW_MSG)
            ret = MAX_OVERFLOW_MSG;
    }

    pMac->gCurrentLogSize += ret;


#if defined (ANI_OS_TYPE_WINDOWS)
    //DbgPrint("%s", pBuf);
    sysLog(pMac, LOGE, FL("%s"), pBuf);
#endif
#endif //for #ifdef WLAN_DEBUG
    return ret;
}


char* dumpLOG( tpAniSirGlobal pMac, char *p )
{
    tANI_U32 i;

    for( i = SIR_FIRST_MODULE_ID; i <= SIR_LAST_MODULE_ID; i++ ) {
        p += log_sprintf(pMac, p, "[0x%2x]", i);
        switch (i)
        {
            case SIR_HAL_MODULE_ID: p += log_sprintf( pMac, p, "HAL "); break;
            case SIR_CFG_MODULE_ID: p += log_sprintf( pMac, p, "CFG "); break;
            case SIR_LIM_MODULE_ID: p += log_sprintf( pMac, p, "LIM "); break;
            case SIR_ARQ_MODULE_ID: p += log_sprintf( pMac, p, "ARQ "); break;
            case SIR_SCH_MODULE_ID: p += log_sprintf( pMac, p, "SCH "); break;
            case SIR_PMM_MODULE_ID: p += log_sprintf( pMac, p, "PMM "); break;
            case SIR_MNT_MODULE_ID: p += log_sprintf( pMac, p, "MNT "); break;
            case SIR_DBG_MODULE_ID: p += log_sprintf( pMac, p, "DBG "); break;
            case SIR_DPH_MODULE_ID: p += log_sprintf( pMac, p, "DPH "); break;
            case SIR_SYS_MODULE_ID: p += log_sprintf( pMac, p, "SYS "); break;
            case SIR_PHY_MODULE_ID: p += log_sprintf( pMac, p, "PHY "); break;
            case SIR_DVT_MODULE_ID: p += log_sprintf( pMac, p, "DVT "); break;
            case SIR_SMS_MODULE_ID: p += log_sprintf( pMac, p, "SMS "); break;
            default: p += log_sprintf( pMac, p, "UNK ", i); break;
        }

        p += log_sprintf( pMac, p,
                ": debug level is [0x%x] ",
                pMac->utils.gLogDbgLevel[i - SIR_FIRST_MODULE_ID]);

        switch( pMac->utils.gLogDbgLevel[i - SIR_FIRST_MODULE_ID] )
        {
            case LOGOFF: p += log_sprintf( pMac, p, "LOG disabled\n"); break;
            case LOGP: p += log_sprintf( pMac, p, "LOGP(Panic only)\n"); break;
            case LOGE: p += log_sprintf( pMac, p, "LOGE(Errors only)\n"); break;
            case LOGW: p += log_sprintf( pMac, p, "LOGW(Warnings)\n"); break;
            case LOG1: p += log_sprintf( pMac, p, "LOG1(Minimal debug)\n"); break;
            case LOG2: p += log_sprintf( pMac, p, "LOG2(Verbose)\n"); break;
            case LOG3: p += log_sprintf( pMac, p, "LOG3(Very Verbose)\n"); break;
            case LOG4: p += log_sprintf( pMac, p, "LOG4(Very Very Verbose)\n"); break;
            default: p += log_sprintf( pMac, p, "Unknown\n"); break;
        }
    }

    return p;
}

char* setLOGLevel( tpAniSirGlobal pMac, char *p, tANI_U32 module, tANI_U32 level )
{
    tANI_U32 i;

    if((module > SIR_LAST_MODULE_ID || module < SIR_FIRST_MODULE_ID) && module != 0xff ) {
        p += log_sprintf( pMac, p, "Invalid module id 0x%x\n", module );
        return p;
    }

    if( 0xff == module ) {
        for( i = SIR_FIRST_MODULE_ID; i <= SIR_LAST_MODULE_ID; i++ )
            pMac->utils.gLogDbgLevel[i - SIR_FIRST_MODULE_ID] = level;
    } else {
        pMac->utils.gLogDbgLevel[module - SIR_FIRST_MODULE_ID] = level;
    }

#ifdef ANI_PHY_DEBUG
    if (module == 0xff || module == SIR_PHY_MODULE_ID) {
        pMac->hphy.phy.phyDebugLogLevel = level;
    }
#endif

    return dumpLOG( pMac, p );
}

static void Log_getCfg(tpAniSirGlobal pMac, tANI_U16 cfgId)
{
#define CFG_CTL_INT           0x00080000
    if ((pMac->cfg.gCfgEntry[cfgId].control & CFG_CTL_INT) != 0)
    {
        tANI_U32  val;

        // Get integer parameter
        if (wlan_cfgGetInt(pMac, (tANI_U16)cfgId, &val) != eSIR_SUCCESS)
        {
            sysLog(pMac, LOGE, FL("Get cfgId 0x%x failed\n"), cfgId);
        }
        else
        {
            sysLog( pMac, LOGE, FL("WNI_CFG_%s(%d  0x%x) = %ld\n"),  gCfgParamName[cfgId], cfgId, cfgId, val );
        }
    }
    else
    {
        tANI_U8 buf[CFG_MAX_STR_LEN] = {0} ;
        tANI_U32 valueLen ;

        // Get string parameter
        valueLen = CFG_MAX_STR_LEN ;
        if (wlan_cfgGetStr(pMac, cfgId, buf, &valueLen) != eSIR_SUCCESS)
        {
            sysLog(pMac, LOGE, FL("Get cfgId 0x%x failed\n"), cfgId);
        }
        else
        {
            sysLog( pMac, LOGE, FL("WNI_CFG_%s(%d  0x%x) len=%ld\n"),  gCfgParamName[cfgId], cfgId, cfgId, valueLen );
            sirDumpBuf(pMac, SIR_WDA_MODULE_ID, LOGW, buf, valueLen) ;
        }
    }

    return;
}

static void Log_setCfg(tpAniSirGlobal pMac, tANI_U16 cfgId, tANI_U32 val)
{
    sysLog(pMac, LOGE, FL("Set %s(0x%x) to value 0x%x\n"),
           gCfgParamName[cfgId], cfgId, val);

    if (cfgSetInt(pMac, (tANI_U16)cfgId, val) != eSIR_SUCCESS)
        sysLog(pMac, LOGE, FL("setting cfgId 0x%x to value 0x%x failed \n"),
               cfgId, val);
     return;
}


char * dump_cfg_get( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg2; (void) arg3; (void) arg4;
    Log_getCfg(pMac, (tANI_U16) arg1);
    return p;
}

char * dump_cfg_group_get( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tANI_U32 i, startId, endId;

    (void) arg3; (void) arg4;

    if (arg1 < CFG_PARAM_MAX_NUM) {
        startId = arg1;
    } else {
        p += log_sprintf( pMac, p, "Start CFGID must be less than %d\n", CFG_PARAM_MAX_NUM);
        return p;
    }

    if ((arg2 == 0) || (arg2 > CFG_PARAM_MAX_NUM))
        arg2 = 30;

    endId = ((startId + arg2) < CFG_PARAM_MAX_NUM) ? (startId + arg2) : CFG_PARAM_MAX_NUM;

    for (i=startId; i < endId; i++)
        Log_getCfg(pMac, (tANI_U16) i);

    return p;
}
char * dump_cfg_set( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg3; (void) arg4;
    Log_setCfg(pMac, (tANI_U16) arg1, arg2);
    return p;
}

char * dump_log_level_set( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    (void) arg1; (void) arg2; (void) arg3; (void) arg4;
    p = setLOGLevel( pMac, p, arg1, arg2 );
    return p;
}

#ifdef WLAN_DEBUG    
static tLogdRegList dataType[] =
{
    {SIR_MAC_DATA_DATA, "DATA "},
    {SIR_MAC_DATA_DATA_ACK, "D+A  "},
    {SIR_MAC_DATA_DATA_POLL, "D+P  "},
    {SIR_MAC_DATA_DATA_ACK_POLL, "D+A+P"},
    {SIR_MAC_DATA_NULL, "NULL "},
    {SIR_MAC_DATA_NULL_ACK, "ACK  "},
    {SIR_MAC_DATA_NULL_POLL, "POLL "},
    {SIR_MAC_DATA_NULL_ACK_POLL, "A+P  "},
    {SIR_MAC_DATA_QOS_DATA, "QDATA"},
    {SIR_MAC_DATA_QOS_DATA_ACK, "QD+A "},
    {SIR_MAC_DATA_QOS_DATA_POLL, "QD+P "},
    {SIR_MAC_DATA_QOS_DATA_ACK_POLL, "QD+AP"},
    {SIR_MAC_DATA_QOS_NULL, "QNULL"},
    {SIR_MAC_DATA_QOS_NULL_POLL, "QACK "},
    {SIR_MAC_DATA_QOS_NULL_ACK_POLL, "Q+A+P"}
};

static tLogdRegList mgmtType[] = {
    {SIR_MAC_MGMT_BEACON, "BEACON   "},
    {SIR_MAC_MGMT_ASSOC_REQ, "ASSOCREQ "},
    {SIR_MAC_MGMT_ASSOC_RSP, "ASSOCRSP "},
    {SIR_MAC_MGMT_REASSOC_RSP, "REASSRSP "},
    {SIR_MAC_MGMT_REASSOC_REQ, "REASSREQ "},
    {SIR_MAC_MGMT_PROBE_REQ, "PROBEREQ "},
    {SIR_MAC_MGMT_PROBE_RSP, "PROBERSP "},
    {SIR_MAC_MGMT_AUTH, "AUTH     "},
    {SIR_MAC_MGMT_DEAUTH, "DEAUTH   "},
    {SIR_MAC_MGMT_DISASSOC, "DISASSOC "},
    {SIR_MAC_MGMT_ACTION, "ACTION   "}
};

static tLogdRegList ctlType[] = {
    {SIR_MAC_CTRL_RTS, "RTS(CTL) "},
    {SIR_MAC_CTRL_CTS, "CTS(CTL) "},
    {SIR_MAC_CTRL_ACK, "ACK(CTL) "},
    {SIR_MAC_CTRL_PS_POLL, "PS-POLL  "},
    {SIR_MAC_CTRL_BAR, "BAR      "},
    {SIR_MAC_CTRL_BA,  "BA       "},
    {SIR_MAC_CTRL_CF_END,"CF-END   "},
    {SIR_MAC_CTRL_CF_END_ACK,"CFE+ACK  "}
};

static char * printMesgName(tpAniSirGlobal pMac, char *p, tANI_U32 type, tANI_U32 subType, tANI_U32 tcId)
{
    tLogdRegList *pEntry = NULL;
    tANI_U32 nEntries, i;

    switch (type)
    {
        case SIR_MAC_DATA_FRAME:
            pEntry   = &dataType[0];
            nEntries = sizeof(dataType)/sizeof(dataType[0]);
            break;
        case SIR_MAC_MGMT_FRAME:
            pEntry   = &mgmtType[0];
            nEntries = sizeof(mgmtType)/sizeof(mgmtType[0]);
            break;
        case SIR_MAC_CTRL_FRAME:
            pEntry   = &ctlType[0];
            nEntries = sizeof(ctlType)/sizeof(ctlType[0]);
            break;
        default:
            p += log_sprintf( pMac, p, "RESERVED ");
            return p;
    }

    for (i=0; i < nEntries; i++, pEntry++) {
        if (pEntry->addr == subType) {
            p += log_sprintf( pMac, p, "%s", pEntry->name);
            break;
        }
    }

    if (i >= nEntries)
        p += log_sprintf( pMac, p, "RESERVED ");

    if (type == SIR_MAC_DATA_FRAME)
        p += log_sprintf( pMac, p, "(%d) ", tcId);
    return p;
}

char * dump_thread_info( tpAniSirGlobal pMac, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, char *p)
{
    tANI_U32 i, j;

    p += log_sprintf( pMac, p, "\n**** BBT RECEIVE **** \n");
    p += log_sprintf( pMac, p, "\nBBT CRC errors received   : %d\n",
            pMac->sys.gSysBbtCrcFail);
    p += log_sprintf( pMac, p, "BBT duplicates received   : %d\n",
            pMac->sys.gSysBbtDuplicates);
    p += log_sprintf( pMac, p, "BBT total frames received : %d\n",
            pMac->sys.gSysBbtReceived);
    p += log_sprintf( pMac, p, "\tPosted to LIM : %d\n",
            pMac->sys.gSysBbtPostedToLim);
    p += log_sprintf( pMac, p, "\tPosted to SCH : %d\n",
            pMac->sys.gSysBbtPostedToSch);
    p += log_sprintf( pMac, p, "\tPosted to PMM : %d\n",
            pMac->sys.gSysBbtPostedToPmm);
    p += log_sprintf( pMac, p, "\tPosted to HAL : %d\n",
            pMac->sys.gSysBbtPostedToHal);
    p += log_sprintf( pMac, p, "\tDropped       : %d\n",
            pMac->sys.gSysBbtDropped);
    p += log_sprintf( pMac, p, "\t\tLearn mode frame out of learn mode : %d\n",
            pMac->sys.gSysBbtLearnFrameInv);
    p += log_sprintf( pMac, p, "\t\tNon-learn mode frame in learn mode : %d\n",
            pMac->sys.gSysBbtNonLearnFrameInv);

    p += log_sprintf( pMac, p, "\nMGMT RX frame counts\n");
    for (i = 0; i < 16; i++) {
        if (pMac->sys.gSysFrameCount[0][i]) {
            p += log_sprintf( pMac, p, "\t");
            p = printMesgName( pMac, p, 0, i, 0);
            p += log_sprintf( pMac, p, "[%d] %d\n", i, pMac->sys.gSysFrameCount[0][i]);
        }
    }

    p += log_sprintf( pMac, p, "CTRL RX frame counts\n");

    for (i = 0; i < 16; i++) {
        if (pMac->sys.gSysFrameCount[1][i]) {
            p += log_sprintf( pMac, p, "\t");
            p = printMesgName(pMac, p, 1, i, 0);
            p += log_sprintf( pMac, p, "[%d] %d\n", i, pMac->sys.gSysFrameCount[1][i]);
        }
    }

    p += log_sprintf( pMac, p, "DATA RX frame counts\n");
    for (i = 0; i < 16; i++) {
        if (pMac->sys.gSysFrameCount[2][i]) {
            p += log_sprintf( pMac, p, "\t");
            p = printMesgName(pMac, p, 2, i, 0);
            p += log_sprintf( pMac, p, "[%d] %d\n", i,
                    pMac->sys.gSysFrameCount[2][i]);
        }
    }

    if (pMac->sch.gSchBBXportRcvCnt) {
        p += log_sprintf( pMac, p, "\nSCH processed messages : %d\n",
                pMac->sch.gSchBBXportRcvCnt);
        p += log_sprintf( pMac, p, "\tBeacons  : %d\n", pMac->sch.gSchBcnRcvCnt);
        p += log_sprintf( pMac, p, "\t\tignored  : %d\n", pMac->sch.gSchBcnIgnored);
        p += log_sprintf( pMac, p, "\t\tin error : %d\n", pMac->sch.gSchBcnParseErrorCnt);
        p += log_sprintf( pMac, p, "\tQoS null : %d\n", pMac->sch.gSchRRRcvCnt);
        p += log_sprintf( pMac, p, "\tDropped  : %d\n", pMac->sch.gSchUnknownRcvCnt);
    }


    p += log_sprintf( pMac, p, "\nLIM processed messages : %d\n", pMac->lim.numTot);
    if (pMac->lim.numTot) {
        p += log_sprintf( pMac, p, "\tSME Messages (HDD) : %d\n", pMac->lim.numSme);
        p += log_sprintf( pMac, p, "\tMAC Messages (BBT) : %d\n", pMac->lim.numBbt);
        p += log_sprintf( pMac, p, "\t\tBad Protocol\t: %d\n", pMac->lim.numProtErr);
        p += log_sprintf( pMac, p, "\t\tIn Learn mode\t: %d (ignored %d)\n",
                pMac->lim.numLearn, pMac->lim.numLearnIgnore);

        for (i=0; i<4; i++) {
            for (j=0; j<16; j++) {
                if (pMac->lim.numMAC[i][j]) {
                    p += log_sprintf( pMac, p, "\t\t");
                    p = printMesgName(pMac, p, i, j, 0);
                    p += log_sprintf( pMac, p, "\t: %d\n", pMac->lim.numMAC[i][j]);
                }
            }
        }

        p += log_sprintf( pMac, p, "\tBeacons received : %d\n", pMac->lim.gLimNumBeaconsRcvd);
        p += log_sprintf( pMac, p, "\t\tIgnored : %d\n", pMac->lim.gLimNumBeaconsIgnored);
    }

    if (pMac->sys.gSysFrameCount[SIR_MAC_MGMT_FRAME][SIR_MAC_MGMT_PROBE_REQ]) {
        p += log_sprintf( pMac, p, "\nProbe Requests Received: %d\n",
                pMac->sys.gSysFrameCount[SIR_MAC_MGMT_FRAME][SIR_MAC_MGMT_PROBE_REQ]);
        p += log_sprintf( pMac, p, "\tIgnored  \t: %d\n", pMac->sys.probeIgnore);
        p += log_sprintf( pMac, p, "\tSSID miss\t: %d\n", pMac->sys.probeBadSsid);
        p += log_sprintf( pMac, p, "\tParse err\t: %d\n", pMac->sys.probeError);
        p += log_sprintf( pMac, p, "\tResponded\t: %d\n", pMac->sys.probeRespond);
    }

    return p;
}
#endif

/* Initialize the index */
void logDumpInit(tpAniSirGlobal pMac)
{
    pMac->dumpTablecurrentId = 0;

}

void logDumpRegisterTable( tpAniSirGlobal pMac, tDumpFuncEntry *pEntry, tANI_U32   nItems )
{

    pMac->dumpTableEntry[pMac->dumpTablecurrentId]->nItems = nItems;
    pMac->dumpTableEntry[pMac->dumpTablecurrentId]->mindumpid = pEntry->id;
    pMac->dumpTableEntry[pMac->dumpTablecurrentId]->maxdumpid = (pEntry + (nItems-1))->id;
    pMac->dumpTableEntry[pMac->dumpTablecurrentId]->dumpTable = pEntry;
    pMac->dumpTablecurrentId++;
}


/*
 * print nItems from the menu list ponted to by m
 */
static tANI_U32 print_menu(tpAniSirGlobal pMac, char  *p, tANI_U32 startId)
{
    tANI_U32 currentId = 0;
    tANI_U32 i, j;
    tANI_S32 ret = 0;
    tDumpFuncEntry *pEntry = NULL;
    tANI_U32 nItems = 0;

    for(i = 0; i < pMac->dumpTablecurrentId; i++) {
        pEntry = pMac->dumpTableEntry[i]->dumpTable;
        nItems = pMac->dumpTableEntry[i]->nItems;

        for (j = 0; j < nItems; j++, pEntry++) {
            if (pEntry->description == NULL) 
                continue;

            if (pEntry->id == 0) {
                ret = log_sprintf( pMac,p, "---- %s\n", pEntry->description); 

                if (ret <= 0)
                    break;

                p += ret;
                continue;
            }

            if (pEntry->id < startId)
                continue;

            ret = log_sprintf(pMac, p, "%4d\t%s\n", pEntry->id, pEntry->description);

            if (ret <= 0)
                break;

            currentId = pEntry->id;
            p += ret;
        }

        if (ret <= 0)
            break;
    }

    return currentId;
}

int logRtaiDump( tpAniSirGlobal pMac, tANI_U32 cmd, tANI_U32 arg1, tANI_U32 arg2, tANI_U32 arg3, tANI_U32 arg4, tANI_U8 *pBuf)
{
    char *p = (char *)pBuf;
    tANI_U32 i;
    tANI_U32 nItems = 0;
    tDumpFuncEntry *pEntry = NULL;

    pMac->gCurrentLogSize = 0;
    if (debug) {
        p += log_sprintf( pMac,p, "Cmd = %d Args (0x%x,0x%x,0x%x,0x%x)\n\n",
                cmd, arg1, arg2, arg3, arg4);
    }

    if( cmd == MAX_DUMP_CMD || cmd == 0 ) {
        pMac->menuCurrent = print_menu(pMac, p, pMac->menuCurrent);
        return pMac->gCurrentLogSize;
    }
    if(cmd <= HAL_LOG_DUMP_CMD_END)
    {
       WDA_HALDumpCmdReq(pMac, cmd, arg1, arg2, arg3, arg4, p);
    }
    else
    {
       for(i = 0; i < pMac->dumpTablecurrentId; i++) {
           if( (cmd > pMac->dumpTableEntry[i]->mindumpid) && (cmd <= pMac->dumpTableEntry[i]->maxdumpid)) {
               pEntry = pMac->dumpTableEntry[i]->dumpTable;
               nItems = pMac->dumpTableEntry[i]->nItems;
               break;
           } else {
               continue;
           }
       }
       
       if((nItems > 0) && (pEntry != NULL)) {
           for (i = 0; i < nItems; i++, pEntry++) {
               if( cmd == pEntry->id ) {
                   if ( pEntry->func != NULL ) {
                       pEntry->func(pMac, arg1, arg2, arg3, arg4, p);
                   } else {
                       p += log_sprintf( pMac,p, "Cmd not supported\n");
                   }
                   break;
               }
           }
       } else {
           p += log_sprintf( pMac,p, "Cmd not found \n");
       }
    }
    if (debug)
        p += log_sprintf( pMac,p, "Returned %d bytes\n", pMac->gCurrentLogSize);

    return pMac->gCurrentLogSize;

}

#endif
