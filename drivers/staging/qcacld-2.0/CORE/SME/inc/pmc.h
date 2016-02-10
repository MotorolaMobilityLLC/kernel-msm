/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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

/******************************************************************************
*
* Name:  pmc.h
*
* Description: Power Management Control (PMC) internal definitions.
*

*
******************************************************************************/

#ifndef __PMC_H__
#define __PMC_H__


#include "csrLinkList.h"
#include "pmcApi.h"
#include "smeInternal.h"


//Change PMC_ABORT to no-op for now. We need to define it as VOS_ASSERT(0) once we
//cleanup the usage.
#define PMC_ABORT

#define PMC_SESSION_MAX 5

/* Auto Ps Entry Timer Default value - 1000 ms */
#define AUTO_PS_ENTRY_TIMER_DEFAULT_VALUE 1000

/* Auto Deferred Ps Entry Timer value - 5000 ms */
#define AUTO_DEFERRED_PS_ENTRY_TIMER_DEFAULT_VALUE 5000


/* Host power sources. */
typedef enum ePowerSource
{
    AC_POWER,  /* host is operating from AC power */
    BATTERY_POWER  /* host is operating from battery power */
} tPowerSource;


/* Power save check routine list entry. */
typedef struct sPowerSaveCheckEntry
{
    tListElem link;  /* list links */
    tANI_BOOLEAN (*checkRoutine) (void *checkContext);  /* power save check routine */
    void *checkContext;  /* value to be passed as parameter to routine specified above */
} tPowerSaveCheckEntry, *tpPowerSaveCheckEntry;


/* Device Power State update indication list entry. */
typedef struct sDeviceStateUpdateIndEntry
{
    tListElem link;  /* list links */
    void (*callbackRoutine) (void *callbackContext, tPmcState pmcState); /* Callback routine to be invoked when pmc changes device state */
    void *callbackContext;  /* value to be passed as parameter to routine specified above */
} tDeviceStateUpdateIndEntry, *tpDeviceStateUpdateIndEntry;

/* Request full power callback routine list entry. */
typedef struct sRequestFullPowerEntry
{
    tListElem link;  /* list links */
    void (*callbackRoutine) (void *callbackContext, eHalStatus status);  /* routine to call when full power is restored */
    void *callbackContext;  /* value to be passed as parameter to routine specified above */
} tRequestFullPowerEntry, *tpRequestFullPowerEntry;


/* Request BMPS callback routine list entry. */
typedef struct sRequestBmpsEntry
{
   tListElem link;  /* list links */

   /* routine to call when BMPS request succeeded/failed */
   void (*callbackRoutine) (void *callbackContext, eHalStatus status);

   /* value to be passed as parameter to routine specified above */
   void *callbackContext;

} tRequestBmpsEntry, *tpRequestBmpsEntry;


/* Start U-APSD callback routine list entry. */
typedef struct sStartUapsdEntry
{
   tListElem link;  /* list links */

   /* routine to call when Uapsd Start succeeded/failed*/
   void (*callbackRoutine) (void *callbackContext, eHalStatus status);

   /* value to be passed as parameter to routine specified above */
   void *callbackContext;

} tStartUapsdEntry, *tpStartUapsdEntry;

typedef struct sPmcDeferredMsg
{
    tListElem link;
    tpAniSirGlobal pMac;
    tANI_U16 messageType;
    tANI_U16 size;  //number of bytes in u.data
    union
    {
        tSirPowerSaveCfg powerSaveConfig;
        tSirWowlAddBcastPtrn wowlAddPattern;
        tSirWowlDelBcastPtrn wowlDelPattern;
        tANI_U8 data[1];    //a place holder
    }u;
} tPmcDeferredMsg;



/* Current PMC information for a particular device. */
typedef struct sPmcInfo
{
    tPowerSource powerSource;  /* host power source */
    tPmcSwitchState hwWlanSwitchState;  /* Hardware WLAN Switch state */
    tPmcSwitchState swWlanSwitchState;  /* Software WLAN Switch state */
    tPmcState pmcState;  /* PMC state */
    tANI_BOOLEAN requestFullPowerPending;  /* TRUE if a request for full power is pending */
    tRequestFullPowerReason requestFullPowerReason; /* reason for requesting full power */
    tPmcImpsConfigParams impsConfig;  /* IMPS configuration */
    tPmcBmpsConfigParams bmpsConfig;  /* BMPS configuration */
    tPmcSmpsConfigParams smpsConfig;  /* SMPS configuration */
    tANI_BOOLEAN impsEnabled;  /* TRUE if IMPS is enabled */
    tANI_BOOLEAN bmpsEnabled;  /* TRUE if BMPS is enabled */
    tANI_BOOLEAN autoBmpsEntryEnabled;  /* TRUE if auto BMPS entry is enabled. If set to TRUE, PMC will
                                           attempt to put the device into BMPS on entry into full Power */
    tANI_BOOLEAN bmpsRequestedByHdd; /*TRUE if BMPS mode has been requested by HDD */
    tANI_BOOLEAN bmpsRequestQueued; /*If a enter BMPS request is queued*/
    tANI_BOOLEAN smpsEnabled;  /* TRUE if SMPS is enabled */
    tANI_BOOLEAN remainInPowerActiveTillDHCP;  /* Remain in Power active till DHCP completes */
    tANI_U32 remainInPowerActiveThreshold;  /*Remain in Power active till DHCP threshold*/
    tANI_U32 impsPeriod;  /* amount of time to remain in IMPS */
    void (*impsCallbackRoutine) (void *callbackContext, eHalStatus status);  /* routine to call when IMPS period
                                                                                has finished */
    void *impsCallbackContext;  /* value to be passed as parameter to routine specified above */
    vos_timer_t hImpsTimer;  /* timer to use with IMPS */
    vos_timer_t hTrafficTimer;  /* timer to measure traffic for BMPS */
    vos_timer_t hExitPowerSaveTimer;  /* timer for deferred exiting of power save mode */
    tDblLinkList powerSaveCheckList; /* power save check routine list */
    tDblLinkList requestFullPowerList; /* request full power callback routine list */
    tANI_U32 cLastTxUnicastFrames;  /* transmit unicast frame count at last BMPS traffic timer expiration */
    tANI_U32 cLastRxUnicastFrames;  /* receive unicast frame count at last BMPS traffic timer expiration */


    tANI_BOOLEAN uapsdEnabled;  /* TRUE if UAPSD is enabled */
    tANI_BOOLEAN uapsdSessionRequired; /* TRUE if device should go to UAPSD on entering BMPS*/
    tDblLinkList requestBmpsList; /* request Bmps callback routine list */
    tDblLinkList requestStartUapsdList; /* request start Uapsd callback routine list */
    tANI_BOOLEAN standbyEnabled;  /* TRUE if Standby is enabled */
    void (*standbyCallbackRoutine) (void *callbackContext, eHalStatus status); /* routine to call for standby request */
    void *standbyCallbackContext;/* value to be passed as parameter to routine specified above */
    tDblLinkList deviceStateUpdateIndList; /*update device state indication list */
    tANI_BOOLEAN pmcReady; /*whether eWNI_SME_SYS_READY_IND has been sent to PE or not */
    tANI_BOOLEAN wowlEnabled;  /* TRUE if WoWL is enabled */
    tANI_BOOLEAN wowlModeRequired; /* TRUE if device should go to WOWL on entering BMPS */
    void (*enterWowlCallbackRoutine) (void *callbackContext, eHalStatus status); /* routine to call for wowl request */
    void *enterWowlCallbackContext;/* value to be passed as parameter to routine specified above */
    tSirSmeWowlEnterParams wowlEnterParams; /* WOWL mode configuration */
    tDblLinkList deferredMsgList;   //The message in here are deferred and DONOT expect response from PE
#ifdef FEATURE_WLAN_SCAN_PNO
    preferredNetworkFoundIndCallback  prefNetwFoundCB; /* routine to call for Preferred Network Found Indication */
    void *preferredNetworkFoundIndCallbackContext;/* value to be passed as parameter to routine specified above */
#endif // FEATURE_WLAN_SCAN_PNO
#ifdef WLAN_FEATURE_PACKET_FILTERING
    FilterMatchCountCallback  FilterMatchCountCB; /* routine to call for Packet Coalescing Filter Match Count */
    void *FilterMatchCountCBContext;/* value to be passed as parameter to routine specified above */
#endif // WLAN_FEATURE_PACKET_FILTERING
#ifdef WLAN_FEATURE_GTK_OFFLOAD
    GTKOffloadGetInfoCallback  GtkOffloadGetInfoCB; /* routine to call for GTK Offload Information */
    void *GtkOffloadGetInfoCBContext;        /* value to be passed as parameter to routine specified above */
#endif // WLAN_FEATURE_GTK_OFFLOAD

#ifdef WLAN_WAKEUP_EVENTS
    void (*wakeReasonIndCB) (void *callbackContext, tpSirWakeReasonInd pWakeReasonInd);  /* routine to call for Wake Reason Indication */
    void *wakeReasonIndCBContext;  /* value to be passed as parameter to routine specified above */
#endif // WLAN_WAKEUP_EVENTS

/*
 * If TRUE driver will go to BMPS only if host operating system
 * asks to enter BMPS. For android wlan_hdd_cfg80211_set_power_mgmt API will
 * be used to set host power save
 */
    v_BOOL_t    isHostPsEn;
    v_BOOL_t    ImpsReqFailed;
    v_BOOL_t    ImpsReqTimerFailed;
    tANI_U8     ImpsReqFailCnt;
    tANI_U8     ImpsReqTimerfailCnt;

} tPmcInfo, *tpPmcInfo;


//MACRO
#define PMC_IS_READY(pMac)  ( ((pMac)->pmc.pmcReady) && (STOPPED != (pMac)->pmc.pmcState) )


/* Routine definitions. */
extern eHalStatus pmcEnterLowPowerState (tHalHandle hHal);
extern eHalStatus pmcExitLowPowerState (tHalHandle hHal);
extern eHalStatus pmcEnterFullPowerState (tHalHandle hHal);
extern eHalStatus pmcEnterRequestFullPowerState (tHalHandle hHal, tRequestFullPowerReason fullPowerReason);
extern eHalStatus pmcEnterRequestImpsState (tHalHandle hHal);
extern eHalStatus pmcEnterImpsState (tHalHandle hHal);
extern eHalStatus pmcEnterRequestBmpsState (tHalHandle hHal);
extern eHalStatus pmcEnterBmpsState (tHalHandle hHal);
extern eHalStatus pmcEnterRequestStartUapsdState (tHalHandle hHal);
extern eHalStatus pmcEnterUapsdState (tHalHandle hHal);
extern eHalStatus pmcEnterRequestStopUapsdState (tHalHandle hHal);
extern eHalStatus pmcEnterRequestStandbyState (tHalHandle hHal);
extern eHalStatus pmcEnterStandbyState (tHalHandle hHal);
extern tANI_BOOLEAN pmcPowerSaveCheck (tHalHandle hHal);
extern eHalStatus pmcSendPowerSaveConfigMessage (tHalHandle hHal);
extern eHalStatus pmcSendMessage (tpAniSirGlobal pMac, tANI_U16 messageType, void *pMessageData, tANI_U32 messageSize);
extern void pmcDoCallbacks (tHalHandle hHal, eHalStatus callbackStatus);
extern void pmcDoBmpsCallbacks (tHalHandle hHal, eHalStatus callbackStatus);
extern void pmcDoStartUapsdCallbacks (tHalHandle hHal, eHalStatus callbackStatus);
extern void pmcDoStandbyCallbacks (tHalHandle hHal, eHalStatus callbackStatus);
extern eHalStatus pmcStartTrafficTimer (tHalHandle hHal, tANI_U32 expirationTime);
extern void pmcStopTrafficTimer (tHalHandle hHal);
extern void pmcImpsTimerExpired (tHalHandle hHal);
extern void pmcTrafficTimerExpired (tHalHandle hHal);

extern void pmcExitPowerSaveTimerExpired (tHalHandle hHal);
extern tPmcState pmcGetPmcState (tHalHandle hHal);
extern const char* pmcGetPmcStateStr(tPmcState state);
extern void pmcDoDeviceStateUpdateCallbacks (tHalHandle hHal, tPmcState state);
extern eHalStatus pmcRequestEnterWowlState(tHalHandle hHal, tpSirSmeWowlEnterParams wowlEnterParams);
extern eHalStatus pmcEnterWowlState (tHalHandle hHal);
extern eHalStatus pmcRequestExitWowlState(tHalHandle hHal,
                                          tpSirSmeWowlExitParams wowlExitParams);
extern void pmcDoEnterWowlCallbacks (tHalHandle hHal, eHalStatus callbackStatus);
//The function will request for full power as well in addition to defer the message
extern eHalStatus pmcDeferMsg( tpAniSirGlobal pMac, tANI_U16 messageType,
                                               void *pData, tANI_U32 size);
extern eHalStatus pmcIssueCommand(tpAniSirGlobal pMac, tANI_U32 sessionId,
                                 eSmeCommandType cmdType, void *pvParam,
                                 tANI_U32 size, tANI_BOOLEAN fPutToListHead);
extern eHalStatus pmcEnterImpsCheck( tpAniSirGlobal pMac );
extern eHalStatus pmcEnterBmpsCheck( tpAniSirGlobal pMac );
extern tANI_BOOLEAN pmcShouldBmpsTimerRun( tpAniSirGlobal pMac );

/* Power Save Offload Changes */
/* Per SME Session PMC Offload Structure */
typedef struct sPsOffloadPerSessionInfo
{
    tpAniSirGlobal pMac;

    tANI_U32 sessionId;

    /* TRUE if Sta Mode Ps is Enabled */
    tANI_BOOLEAN configStaPsEnabled;

    /* TRUE if deferred Sta Mode Ps is Enabled */
    tANI_BOOLEAN configDefStaPsEnabled;

    /*
     * Indicates current uapsd status
     * Enabled/Disabled/Required
     */
    tUapsdStatus uapsdStatus;

    tANI_BOOLEAN uapsdSessionRequired;

    /* Current Power Save State */
    tPmcState pmcState;

    /*
     * Auto Sta Ps Enable Timer
     * Upon expiration of this timer
     * Power Save Offload module will
     * try to enable sta mode ps
     */
     vos_timer_t autoPsEnableTimer;

    /*  Auto Sta Ps Entry Timer Period */
    tANI_U32 autoPsEntryTimerPeriod;

    /* Full Power Request Pending */
    tANI_BOOLEAN fullPowerReqPend;

    /*
     * List contains functions registered by different modules
     * PsOffload Module will call this to check whether
     * the particular module is ok to enable station mode power save
     */
    tDblLinkList pwrsaveCheckList;

    /*
     * List contains cbs passed by different modules
     * to indicate power state change
     */
    tDblLinkList deviceStateUpdateIndList;

    /*
     * List contains cbs passed by different modules
     * upon requesting full power
     */
    tDblLinkList fullPowerCbList;

    /*
     * List contains cbs passed by different modules
     * upon requesting uapsd
     */
    tDblLinkList uapsdCbList;

    /*
     * Whether TDLS session allows power save or not
     */
#ifdef FEATURE_WLAN_TDLS
    v_BOOL_t isTdlsPowerSaveProhibited;
#endif
    tANI_BOOLEAN UapsdEnabled;
}tPsOffloadPerSessionInfo,*tpPsOffloadPerSessionInfo;

typedef struct sPmcOffloadInfo
{
    /* Based on Whether BMPS is enabled or not in ini */
    tANI_BOOLEAN staPsEnabled;

    tPsOffloadPerSessionInfo pmc[PMC_SESSION_MAX];

}tPmcOffloadInfo,*tpPmcOffloadInfo;

/* Power save check routine list entry. */
typedef struct sPmcOffloadPsCheckEntry
{
    /* list links */
    tListElem link;

    /* power save check routine */
    PwrSaveCheckRoutine pwrsaveCheckCb;

    /* value to be passed as parameter to routine specified above */
    void *checkContext;

    /* Session Id */
    tANI_U32 sessionId;
} tPmcOffloadPsCheckEntry,*tpPmcOffloadPsCheckEntry;


/* Device Power State update indication list entry. */
typedef struct sPmcOffloadDevStateUpdIndEntry
{
    /* list links */
    tListElem link;

    /* Callback routine to be invoked when pmc changes device state */
    PwrSaveStateChangeIndCb stateChangeCb;

    /* value to be passed as parameter to routine specified above */
    void *callbackContext;

    /* Session Id */
    tANI_U32 sessionId;
} tPmcOffloadDevStateUpdIndEntry,*tpPmcOffloadDevStateUpdIndEntry;

/* Request full power callback routine list entry. */
typedef struct sPmcOffloadReqFullPowerEntry
{
    /* list links */
    tListElem link;

    /* routine to call when full power is restored */
    FullPowerReqCb fullPwrCb;

    /* value to be passed as parameter to routine specified above */
    void *callbackContext;

    /* SessionId */
    tANI_U32 sessionId;
}tPmcOffloadReqFullPowerEntry,*tpPmcOffloadReqFullPowerEntry;

/* Start U-APSD callback routine list entry. */
typedef struct sPmcOffloadStartUapsdEntry
{
   tListElem link;  /* list links */

   /* routine to call when Uapsd Start succeeded/failed*/
   UapsdStartIndCb uapsdStartInd;

   /* value to be passed as parameter to routine specified above */
   void *callbackContext;

   /* SessionId */
   tANI_U32 sessionId;
} tPmcOffloadStartUapsdEntry,*tpPmcOffloadStartUapsdEntry;

eHalStatus pmcOffloadOpenPerSession(tHalHandle hHal, tANI_U32 sessionId);
eHalStatus pmcOffloadClosePerSession(tHalHandle hHal, tANI_U32 sessionId);
eHalStatus pmcOffloadStartPerSession(tHalHandle hHal, tANI_U32 sessionId);
eHalStatus pmcOffloadStopPerSession(tHalHandle hHal, tANI_U32 sessionId);

eHalStatus pmcOffloadStartAutoStaPsTimer (tpAniSirGlobal pMac,
                                          tANI_U32 sessionId,
                                          tANI_U32 timerValue);

void pmcOffloadStopAutoStaPsTimer(tpAniSirGlobal pMac, tANI_U32 sessionId);

eHalStatus pmcOffloadQueueRequestFullPower (tpAniSirGlobal pMac,
             tANI_U32 sessionId, tRequestFullPowerReason fullPowerReason);

eHalStatus pmcOffloadEnableStaPsHandler(tpAniSirGlobal pMac,
                                        tANI_U32 sessionId);

void pmcOffloadProcessResponse(tpAniSirGlobal pMac, tSirSmeRsp *pMsg);
void pmcOffloadAutoPsEntryTimerExpired(void *pmcInfo);
void pmcOffloadDoFullPowerCallbacks (tpAniSirGlobal pMac, tANI_U32 sessionId,
                                     eHalStatus status);
void pmcOffloadDoDeviceStateUpdateCallbacks (tpAniSirGlobal pMac,
                            tANI_U32 sessionId, tPmcState state);
void pmcOffloadDoStartUapsdCallbacks (tpAniSirGlobal pMac, tANI_U32 sessionId,
                                      eHalStatus status);
eHalStatus pmcOffloadDisableStaPsHandler(tpAniSirGlobal pMac,
                                         tANI_U8 sessionId);
eHalStatus pmcOffloadEnableStaPsCheck(tpAniSirGlobal pMac,
                                      tANI_U32 sessionId);
eHalStatus pmcOffloadExitPowersaveState(tpAniSirGlobal pMac, tANI_U32 sessionId);

eHalStatus pmcOffloadEnterPowersaveState(tpAniSirGlobal pMac, tANI_U32 sessionId);
void pmcOffloadExitBmpsIndHandler(tpAniSirGlobal pMac, tSirSmeRsp *pMsg);

eHalStatus pmcOffloadQueueStartUapsdRequest(tpAniSirGlobal pMac, tANI_U32 sessionId);
eHalStatus pmcOffloadQueueStopUapsdRequest(tpAniSirGlobal pMac, tANI_U32 sessionId);

#endif
