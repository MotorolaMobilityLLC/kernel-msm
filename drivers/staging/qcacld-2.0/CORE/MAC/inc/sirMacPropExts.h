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

// Proprietary IEs

// the bit map is also used as a config enable, setting a bit in the
// propIE config variable, enables the corresponding capability in the propIE
// the enables simply result in including the corresponding element in the
// propIE
// Ex: setting the capability bit HCF would result in using the capability bit map for
// hcf instead of including the full HCF element in the IE
// capabilities bit map - bit offsets
// setting 11eQos has effect only if QoS is also enabled. then it overrides
// 11e support and implements it silently (as part of the prop ie)
#define SIR_MAC_PROP_CAPABILITY_HCF           WNI_CFG_PROP_CAPABILITY_HCF
#define SIR_MAC_PROP_CAPABILITY_11EQOS        WNI_CFG_PROP_CAPABILITY_11EQOS
#define SIR_MAC_PROP_CAPABILITY_WME           WNI_CFG_PROP_CAPABILITY_WME
#define SIR_MAC_PROP_CAPABILITY_WSM           WNI_CFG_PROP_CAPABILITY_WSM
#define SIR_MAC_PROP_CAPABILITY_EXTRATES      WNI_CFG_PROP_CAPABILITY_EXTRATES
// ap->sta only, request STA to stop using prop rates for some time
#define SIR_MAC_PROP_CAPABILITY_EXTRATE_STOP  WNI_CFG_PROP_CAPABILITY_EXTRATE_STOP
#define SIR_MAC_PROP_CAPABILITY_TITAN         WNI_CFG_PROP_CAPABILITY_TITAN
#define SIR_MAC_PROP_CAPABILITY_TAURUS         WNI_CFG_PROP_CAPABILITY_TAURUS
#define SIR_MAC_PROP_CAPABILITY_ESCORT_PKT    WNI_CFG_PROP_CAPABILITY_ESCORT_PKT
// unused                                     9-12
#define SIR_MAC_PROP_CAPABILITY_EDCAPARAMS    WNI_CFG_PROP_CAPABILITY_EDCAPARAMS
#define SIR_MAC_PROP_CAPABILITY_LOADINFO      WNI_CFG_PROP_CAPABILITY_LOADINFO
#define SIR_MAC_PROP_CAPABILITY_VERSION       WNI_CFG_PROP_CAPABILITY_VERSION
#define SIR_MAC_PROP_CAPABILITY_MAXBITOFFSET  WNI_CFG_PROP_CAPABILITY_MAXBITOFFSET

#define PROP_CAPABILITY_GET(bitname, value) \
        (((value) >> SIR_MAC_PROP_CAPABILITY_ ## bitname) & 1)


#define IS_DOT11_MODE_PROPRIETARY(dot11Mode) \
        (((dot11Mode == WNI_CFG_DOT11_MODE_POLARIS) || \
          (dot11Mode == WNI_CFG_DOT11_MODE_TITAN) || \
          (dot11Mode == WNI_CFG_DOT11_MODE_TAURUS) || \
          (dot11Mode == WNI_CFG_DOT11_MODE_ALL)) ? TRUE: FALSE)

#define IS_DOT11_MODE_HT(dot11Mode) \
        (((dot11Mode == WNI_CFG_DOT11_MODE_11N) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_11N_ONLY) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_11AC) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_11AC_ONLY) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_TAURUS) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_ALL)) ? TRUE: FALSE)

#ifdef WLAN_FEATURE_11AC
#define IS_DOT11_MODE_VHT(dot11Mode) \
        (((dot11Mode == WNI_CFG_DOT11_MODE_11AC) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_11AC_ONLY) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_TAURUS) || \
          (dot11Mode ==  WNI_CFG_DOT11_MODE_ALL)) ? TRUE: FALSE)
#endif

#define IS_DOT11_MODE_11B(dot11Mode)  \
            ((dot11Mode == WNI_CFG_DOT11_MODE_11B) ? TRUE : FALSE)

#define IS_BSS_VHT_CAPABLE(vhtCaps) \
		((vhtCaps).present && \
		((vhtCaps).rxMCSMap != 0xFFFF) && \
		((vhtCaps).txMCSMap != 0xFFFF))

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

//
// Proprietary Quite BSS IE structure
//
// Based on the setting of the "Titan" proprietary bit
// in the tSirPropIEStruct.capability field (bit #6),
// this IE will be sent appropriately to all the ANI
// peers in the following management frames -
// 1) Beacons
// 2) Probe Rsp
//
typedef struct sQuietBssIEStruct
{

  // Indicates the number of TBTT's until the next beacon
  // interval during which the next quiet interval will
  // start
  // 1 - Quiet Interval will start during the beacon
  // interval starting at the next TBTT
  // 0 - Reserved
  tANI_U8 quietCount;

  // Shall be set to the number of beacon intervals between
  // the start of regularly scheduled quiet intervals
  // defined by this Quiet Element
  // 0 - No periodic quiet interval is defined
  tANI_U8 quietPeriod;

  // Duration of the quiet interval, expressed in TUs
  // 1 TU = 1024 microseconds??
  tANI_U16 quietDuration;

  // Set to the offset of the start of the quiet interval
  // from the TBTT specified by the quietCount field,
  // expressed in TUs. The value of this offset field will
  // be less than one beacon interval
  // 1 TU = 1024 microseconds??
  tANI_U16 quietOffset;

} tQuietBssIEStruct, *tpQuietBssIEStruct;

typedef struct sChannelSwitchPropIEStruct
{
    tANI_U8                  mode;
    tANI_U8                  primaryChannel;
    tANI_U8                  subBand;
    tANI_U8                  channelSwitchCount;

} tChannelSwitchPropIEStruct, *tpChannelSwitchPropIEStruct;

// generic proprietary IE structure definition
typedef struct sSirPropIEStruct
{
    tANI_U8                    aniIndicator;

    tANI_U8                    propRatesPresent:1;
    tANI_U8                    apNamePresent:1;
    tANI_U8                    loadBalanceInfoPresent:1;
    tANI_U8                    versionPresent:1;
    tANI_U8                    edcaParamPresent:1;
    tANI_U8                    capabilityPresent:1;
    tANI_U8                    titanPresent:1;
    tANI_U8                    taurusPresent:1;
    tANI_U8                    propChannelSwitchPresent:1;
    tANI_U8                    quietBssPresent:1;
    tANI_U8                    triggerStaScanPresent:1;
    tANI_U8                    rsvd:5;


    tSirMacPropRateSet    propRates;
    tAniApName            apName;           // used in beacon/probe only
    tSirAlternateRadioInfo  alternateRadio; // used in assoc response only
    tANI_U16              capability;       // capability bit map
    tSirMacPropVersion    version;
    tSirMacEdcaParamSetIE edca;
    tChannelSwitchPropIEStruct  channelSwitch;
    tQuietBssIEStruct     quietBss;
    tANI_U8               triggerStaScanEnable;


} tSirPropIEStruct, *tpSirPropIEStruct;



#endif /* __MAC_PROP_EXTS_H */
