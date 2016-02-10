/*
 * Copyright (c) 2011-2014 The Linux Foundation. All rights reserved.
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

/*
 *
 * This file sirMacPropExts.h contains the MAC protocol
 * extensions to support ANI feature set.
 * Author:        Chandra Modumudi
 * Date:          11/27/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */
#ifndef __MAC_PROP_EXTS_H
#define __MAC_PROP_EXTS_H

#include "sirTypes.h"
#include "sirApi.h"
#include "aniSystemDefs.h"

/// EID (Element ID) definitions

#define PROP_CAPABILITY_GET(bitname, value) \
        (((value) >> SIR_MAC_PROP_CAPABILITY_ ## bitname) & 1)

#define IS_DOT11_MODE_HT(dot11Mode) \
        (((dot11Mode == WNI_CFG_DOT11_MODE_11N) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_11N_ONLY) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_11AC) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_11AC_ONLY) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_ALL)) ? TRUE: FALSE)

#ifdef WLAN_FEATURE_11AC
#define IS_DOT11_MODE_VHT(dot11Mode) \
        (((dot11Mode == WNI_CFG_DOT11_MODE_11AC) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_11AC_ONLY) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_ALL)) ? TRUE: FALSE)
#endif

#define IS_DOT11_MODE_11B(dot11Mode)  \
            ((dot11Mode == WNI_CFG_DOT11_MODE_11B) ? TRUE : FALSE)

#define IS_BSS_VHT_CAPABLE(vhtCaps) \
            ((vhtCaps).present && \
             ((vhtCaps).rxMCSMap != 0xFFFF) && \
             ((vhtCaps).txMCSMap != 0xFFFF))

#define WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ      0
#define WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ         1
#define WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ        2
#define WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ 3


/// Proprietary IE definition
typedef struct sSirMacPropIE
{
    tANI_U8    elementID;    // SIR_MAC_ANI_PROP_IE_EID
    tANI_U8    length;
    tANI_U8    oui[3];       // ANI_OUI for Airgo products
    tANI_U8    info[1];
} tSirMacPropIE, *tpSirMacPropIE;


typedef struct sSirMacPropRateSet
{
    tANI_U8  numPropRates;
    tANI_U8  propRate[8];
} tSirMacPropRateSet, *tpSirMacPropRateSet;


#define SIR_PROP_VERSION_STR_MAX 20
typedef struct sSirMacPropVersion
{
    tANI_U32  chip_rev;       // board, chipset info
    tANI_U8   card_type;      // Type of Card
    tANI_U8  build_version[SIR_PROP_VERSION_STR_MAX]; //build version string
} tSirMacPropVersion, *tpSirMacPropVersion;


/* Default value for gLimRestoreCBNumScanInterval */
#define LIM_RESTORE_CB_NUM_SCAN_INTERVAL_DEFAULT        2

// generic proprietary IE structure definition
typedef struct sSirPropIEStruct
{
    tANI_U8                    propRatesPresent:1;
    tANI_U8                    apNamePresent:1;
    tANI_U8                    loadBalanceInfoPresent:1;
    tANI_U8                    versionPresent:1;
    tANI_U8                    edcaParamPresent:1;
    tANI_U8                    capabilityPresent:1;
    tANI_U8                    propChannelSwitchPresent:1;
    tANI_U8                    triggerStaScanPresent:1;
    tANI_U8                    rsvd:8;


    tSirMacPropRateSet    propRates;
    tAniApName            apName;           // used in beacon/probe only
    tSirAlternateRadioInfo  alternateRadio; // used in assoc response only
    tANI_U16              capability;       // capability bit map
    tSirMacPropVersion    version;
    tSirMacEdcaParamSetIE edca;
    tANI_U8               triggerStaScanEnable;


} tSirPropIEStruct, *tpSirPropIEStruct;



#endif /* __MAC_PROP_EXTS_H */
