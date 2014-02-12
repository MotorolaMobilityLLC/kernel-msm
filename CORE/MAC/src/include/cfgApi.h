/*
 * Copyright (c) 2012-2013 Qualcomm Atheros, Inc.
 * All Rights Reserved.
 * Qualcomm Atheros Confidential and Proprietary.
 */
/*
 *  
 * Airgo Networks, Inc proprietary. All rights reserved.
 * Author:      Kevin Nguyen    
 * Date:        04/09/02
 * History:-
 * 04/09/02        Created.
 * --------------------------------------------------------------------
 * 
 */

#ifndef __CFGAPI_H
#define __CFGAPI_H

#include <sirCommon.h>
#include <sirParams.h>
#include <sirMacProtDef.h>
#include <wniApi.h>
#include <aniGlobal.h>


/*---------------------------------------------------------------------*/
/* CFG definitions                                                     */
/*---------------------------------------------------------------------*/
#define CFG_TYPE_STR                0x0000000
#define CFG_TYPE_INT                0x0000001
#define CFG_HOST_RE                 0x0000002
#define CFG_HOST_WE                 0x0000004

// CFG status
typedef enum eCfgStatusTypes {
    CFG_INCOMPLETE,
    CFG_SUCCESS,
    CFG_FAILURE
} tCfgStatusTypes;

// WEP key mapping table row structure
typedef struct
{
    tANI_U8    keyMappingAddr[SIR_MAC_ADDR_LENGTH];
    tANI_U32   wepOn;
    tANI_U8    key[SIR_MAC_KEY_LENGTH]; 
    tANI_U32   status;
} tCfgWepKeyEntry;


/*---------------------------------------------------------------------*/
/* CFG function prototypes                                             */
/*---------------------------------------------------------------------*/

tANI_U32 cfgNeedRestart(tpAniSirGlobal pMac, tANI_U16 cfgId) ;
tANI_U32 cfgNeedReload(tpAniSirGlobal pMac, tANI_U16 cfgId) ;

/// CFG initialization function
void wlan_cfgInit(tpAniSirGlobal);

/// Process host message
void cfgProcessMbMsg(tpAniSirGlobal, tSirMbMsg*);

/// Set integer parameter value
tSirRetStatus cfgSetInt(tpAniSirGlobal, tANI_U16, tANI_U32);

/// Check if the parameter is valid
tSirRetStatus cfgCheckValid(tpAniSirGlobal, tANI_U16);

/// Get integer parameter value
tSirRetStatus wlan_cfgGetInt(tpAniSirGlobal, tANI_U16, tANI_U32*);

/// Increment integer parameter
tSirRetStatus cfgIncrementInt(tpAniSirGlobal, tANI_U16, tANI_U32);

/// Set string parameter value
tSirRetStatus cfgSetStr(tpAniSirGlobal, tANI_U16, tANI_U8*, tANI_U32);

tSirRetStatus cfgSetStrNotify(tpAniSirGlobal, tANI_U16, tANI_U8*, tANI_U32, int);

//Cfg Download function for Prima or Integrated solutions.
void processCfgDownloadReq(tpAniSirGlobal, tANI_U16, tANI_U32*);

/// Get string parameter value
tSirRetStatus wlan_cfgGetStr(tpAniSirGlobal, tANI_U16, tANI_U8*, tANI_U32*);

/// Get string parameter maximum length
tSirRetStatus wlan_cfgGetStrMaxLen(tpAniSirGlobal, tANI_U16, tANI_U32*);

/// Get string parameter maximum length
tSirRetStatus wlan_cfgGetStrLen(tpAniSirGlobal, tANI_U16, tANI_U32*);

/// Get the regulatory tx power on given channel
tPowerdBm cfgGetRegulatoryMaxTransmitPower(tpAniSirGlobal pMac, tANI_U8 channel);

/// Dump CFG data to memory
void cfgDump(tANI_U32*);

/// Save parameters with P flag set
void cfgSave(void);

/// Get capability info
extern tSirRetStatus cfgGetCapabilityInfo(tpAniSirGlobal pMac, tANI_U16 *pCap,tpPESession psessionEntry);

/// Set capability info
extern void cfgSetCapabilityInfo(tpAniSirGlobal, tANI_U16);

/// Cleanup CFG module
void cfgCleanup(tpAniSirGlobal pMac);

extern tANI_U8 *gCfgParamName[];

#endif /* __CFGAPI_H */




