/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

#if !defined( __SAP_CH_SELECT_H )
#define __SAP_CH_SELECT_H

/*===========================================================================

                      s a p C h S e l e c t . h

  OVERVIEW:

  This software unit holds the implementation of the WLAN SAP modules
  functions for channel selection.

  DEPENDENCIES:


  Are listed for each API below.
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.



  when             who       what, where, why
----------       -----    --------------------------------------------------------
2010-03-15        SoftAP    Created module

===========================================================================*/


/*--------------------------------------------------------------------------
  Include Files
------------------------------------------------------------------------*/
#include "aniGlobal.h"
/*--------------------------------------------------------------------------
  defines and enum
--------------------------------------------------------------------------*/

#define SPECT_24GHZ_CH_COUNT    (11) // USA regulatory domain
#define SAPDFS_NORMALISE_1000      (1000/9) // Case of spec20 with channel diff = 0
/* Gen 5 values
#define SOFTAP_MIN_RSSI         (-85)
#define SOFTAP_MAX_RSSI         (-45)
*/
#define SOFTAP_MIN_RSSI         (-100)
#define SOFTAP_MAX_RSSI         (0)
#define SOFTAP_MIN_COUNT        (0)
#define SOFTAP_MAX_COUNT        (60)
#define SOFTAP_RSSI_WEIGHT      (20)
#define SOFTAP_COUNT_WEIGHT     (20)

#define SAP_DEFAULT_24GHZ_CHANNEL     (6)
#define SAP_DEFAULT_5GHZ_CHANNEL      (40)
#define SAP_CHANNEL_NOT_SELECTED (0)

#define SOFTAP_HT20_CHANNELWIDTH 0
#define SAP_SUBBAND1_RSSI_EFFECT_PRIMARY  (-20) // In HT40/VHT80, Effect of primary Channel RSSi on Subband1
#define SAP_SUBBAND2_RSSI_EFFECT_PRIMARY  (-30) // In VHT80, Effect of primary Channel RSSI on Subband2
#define SAP_SUBBAND3_RSSI_EFFECT_PRIMARY  (-40) // In VHT80, Effect of Primary Channel RSSI on Subband3

#define SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY      (-10) // In 2.4GHZ, Effect of Primary  Channel RSSI on First Overlapping Channel
#define SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY        (-20) // In 2.4GHZ, Effect of Primary  Channel RSSI on Second Overlapping Channel
#define SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY      (-30) // In 2.4GHZ, Effect of Primary  Channel RSSI on Third Overlapping Channel
#define SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY     (-40) // In 2.4GHZ, Effect of Primary  Channel RSSI on Fourth Overlapping Channel

typedef enum
{
    CHANNEL_1 = 1,
    CHANNEL_2,
    CHANNEL_3,
    CHANNEL_4,
    CHANNEL_5,
    CHANNEL_6,
    CHANNEL_7,
    CHANNEL_8,
    CHANNEL_9,
    CHANNEL_10,
    CHANNEL_11,
    CHANNEL_12,
    CHANNEL_13,
    CHANNEL_14
} tSapChannel;

#define MAX_80MHZ_BANDS 6
#define SAP_80MHZ_MASK     0x0F
#define SAP_40MHZ_MASK_L   0x03
#define SAP_40MHZ_MASK_H   0x0C

/*
 * structs for holding channel bonding bitmap
 * used for finding new channel when SAP is on
 * DFS channel and radar is detected.
 */
typedef struct sChannelBondingInfo {
  v_U8_t channelMap:4;
  v_U8_t rsvd:4;
  v_U8_t startChannel;
} tChannelBondingInfo;

typedef struct __chan_bonding_bitmap {
  tChannelBondingInfo chanBondingSet[MAX_80MHZ_BANDS];
} chan_bonding_bitmap;

/**
* Structure holding information of each channel in the spectrum,
* it contains the channel number, the computed weight
*/
typedef struct sChannelInfo {
    v_U8_t   channel;
    v_BOOL_t valid; // if the channel is valid to be picked as new channel
} tChannelInfo;

typedef struct sAll5GChannelList{
    v_U8_t       numChannel;
    tChannelInfo *channelList;
} tAll5GChannelList;

typedef struct sSapChannelListInfo{
    v_U8_t numChannel;
    v_U8_t *channelList;
} tSapChannelListInfo;

typedef struct {
    v_U16_t chNum;      // Channel Number
    v_U16_t channelWidth;      // Channel Width
    v_U16_t bssCount;   // bss found in scanresult for this channel
    v_S31_t rssiAgr;    // Max value of rssi among all BSS(es) from scanresult for this channel
    v_U32_t weight;     // Weightage of this channel
    v_U32_t weight_copy; //copy of the orignal weight
    v_BOOL_t valid;     // Is this a valid center frequency for regulatory domain
} tSapSpectChInfo;//tDfsSpectChInfo;

/**
* Structure holding all the information required to make a
* decision for the best operating channel based on dfs formula
*/

typedef struct {
    tSapSpectChInfo *pSpectCh;//tDfsSpectChInfo *pSpectCh;  // Ptr to the channels in the entire spectrum band
    v_U8_t numSpectChans;      // Total num of channels in the spectrum
} tSapChSelSpectInfo;//tDfsChSelParams;

/**
 * Structure for channel weight calculation parameters
 */
typedef struct sSapChSelParams {
    void *pSpectInfoParams;//*pDfsParams;   // Filled with tSapChSelSpectInfo
    v_U16_t numChannels;
} tSapChSelParams;

#define SAP_TX_LEAKAGE_THRES 310
#define SAP_TX_LEAKAGE_MAX  1000
#define SAP_TX_LEAKAGE_MIN  200
/*
 * This define is used to block additional channels
 * based on the new data gathered on auto platforms
 * and to differentiate the leakage data among different
 * platforms.
 */
#define SAP_TX_LEAKAGE_AUTO_MIN  210

typedef struct sSapTxLeakInfo {
    v_U8_t  leak_chan;      /* leak channel */
    v_U32_t leak_lvl;       /* tx leakage lvl */
} tSapTxLeakInfo;

typedef struct sSapChanMatrixInfo {
    v_U8_t channel;         /* channel to switch from */
#ifdef FEATURE_WLAN_CH144
    tSapTxLeakInfo chan_matrix[RF_CHAN_144 - RF_CHAN_36 + 1];
#else
    tSapTxLeakInfo chan_matrix[RF_CHAN_140 - RF_CHAN_36 + 1];
#endif
} tSapChanMatrixInfo;

#endif // if !defined __SAP_CH_SELECT_H
