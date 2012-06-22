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

/*===========================================================================

                      s a p C h S e l e c t . C
                                               
  OVERVIEW:
  
  This software unit holds the implementation of the WLAN SAP modules
  functions for channel selection.  

  DEPENDENCIES: 

  Are listed for each API below. 
  
  
  Copyright (c) 2010 QUALCOMM Incorporated.
  All Rights Reserved.
  Qualcomm Confidential and Proprietary
===========================================================================*/

/*===========================================================================

                      EDIT HISTORY FOR FILE


  This section contains comments describing changes made to the module.
  Notice that changes are listed in reverse chronological order.



  when        who       what, where, why
----------    ---       --------------------------------------------------------
2010-03-15  SOFTAP      Created module

===========================================================================*/


/*--------------------------------------------------------------------------
  Include Files
------------------------------------------------------------------------*/
#include "vos_trace.h"
#include "csrApi.h"
#include "sme_Api.h"
#include "sapChSelect.h"
#include "sapInternal.h"

/*--------------------------------------------------------------------------
  Function definitions
--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
  Defines
--------------------------------------------------------------------------*/
#define SAP_DEBUG

/*==========================================================================
  FUNCTION    sapChanSelInit

  DESCRIPTION 
    Function sapChanSelInit allocates the memory, intializes the
         structures used by the channel selection algorithm

  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    *pSpectInfoParams  : Pointer to tSapChSelSpectInfo structure
   
  RETURN VALUE
    v_BOOL_t:  Success or FAIL
  
  SIDE EFFECTS 
============================================================================*/
v_BOOL_t sapChanSelInit(tHalHandle halHandle, tSapChSelSpectInfo *pSpectInfoParams)
{
    tSapSpectChInfo *pSpectCh = NULL;
    v_U8_t *pChans = NULL;
    v_U16_t channelnum = 0;
    tpAniSirGlobal pMac = PMAC_STRUCT(halHandle);

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s", __FUNCTION__);

    // Channels for that 2.4GHz band
    //Considered only for 2.4GHz need to change in future to support 5GHz support
    pSpectInfoParams->numSpectChans = pMac->scan.base20MHzChannels.numChannels;
       
    // Allocate memory for weight computation of 2.4GHz
    pSpectCh = (tSapSpectChInfo *)vos_mem_malloc((pSpectInfoParams->numSpectChans) * sizeof(*pSpectCh));

    if(pSpectCh == NULL) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "In %s, VOS_MALLOC_ERR", __FUNCTION__);
        return eSAP_FALSE;
    }

    vos_mem_zero(pSpectCh, (pSpectInfoParams->numSpectChans) * sizeof(*pSpectCh));

    // Initialize the pointers in the DfsParams to the allocated memory
    pSpectInfoParams->pSpectCh = pSpectCh;

    pChans = pMac->scan.base20MHzChannels.channelList;

    // Fill the channel number in the spectrum in the operating freq band
    for (channelnum = 0; channelnum < pSpectInfoParams->numSpectChans; channelnum++) {

        if(*pChans == 14 ) //OFDM rates are not supported on channel 14
            continue;
        pSpectCh->chNum = *pChans;
        pSpectCh->valid = eSAP_TRUE;
        pSpectCh->rssiAgr = SOFTAP_MIN_RSSI;// Initialise for all channels
        pSpectCh++;
        pChans++;
    }
    return eSAP_TRUE;
}

/*==========================================================================
  FUNCTION    sapweightRssiCount

  DESCRIPTION 
    Function weightRssiCount calculates the channel weight due to rssi
         and data count(here number of BSS observed)
         
  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    rssi        : Max signal strength receieved from a BSS for the channel
    count       : Number of BSS observed in the channel
   
  RETURN VALUE
    v_U32_t     : Calculated channel weight based on above two
  
  SIDE EFFECTS 
============================================================================*/
v_U32_t sapweightRssiCount(v_S7_t rssi, v_U16_t count)
{
    v_S31_t     rssiWeight=0;
    v_S31_t     countWeight=0;
    v_U32_t     rssicountWeight=0;
    
    // Weight from RSSI
    rssiWeight = SOFTAP_RSSI_WEIGHT * (rssi - SOFTAP_MIN_RSSI)
                 /(SOFTAP_MAX_RSSI - SOFTAP_MIN_RSSI);
                 
    if(rssiWeight > SOFTAP_RSSI_WEIGHT)
        rssiWeight = SOFTAP_RSSI_WEIGHT;
    else if (rssiWeight < 0)
        rssiWeight = 0;

    // Weight from data count
    countWeight = SOFTAP_COUNT_WEIGHT * (count - SOFTAP_MIN_COUNT)
                  /(SOFTAP_MAX_COUNT - SOFTAP_MIN_COUNT);
                      
    if(countWeight > SOFTAP_COUNT_WEIGHT)
        countWeight = SOFTAP_COUNT_WEIGHT;
    else if (countWeight < 0)
        countWeight = 0;
        
    rssicountWeight =  rssiWeight + countWeight;      

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, rssiWeight=%d, countWeight=%d, rssicountWeight=%d",
              __FUNCTION__, rssiWeight, countWeight, rssicountWeight);
    
    return(rssicountWeight);
}

/*==========================================================================
  FUNCTION    sapComputeSpectWeight

  DESCRIPTION 
    Main function for computing the weight of each channel in the
    spectrum based on the RSSI value of the BSSes on the channel
    and number of BSS
    
  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    pSpectInfoParams       : Pointer to the tSpectInfoParams structure
    halHandle              : Pointer to HAL handle
    pResult                : Pointer to tScanResultHandle
   
  RETURN VALUE
    void     : NULL
  
  SIDE EFFECTS 
============================================================================*/
void sapComputeSpectWeight( tSapChSelSpectInfo* pSpectInfoParams, 
                                 tHalHandle halHandle, tScanResultHandle pResult)
{
    v_S7_t rssi = 0;
    v_U8_t chn_num = 0;
    v_U8_t channel_id = 0;

    tCsrScanResultInfo *pScanResult;
    tSapSpectChInfo *pSpectCh   = pSpectInfoParams->pSpectCh;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Computing spectral weight", __FUNCTION__);

    /**
    * Soft AP specific channel weight calculation using DFS formula
    */

    pScanResult = sme_ScanResultGetFirst(halHandle, pResult);    

    while (pScanResult) {
        pSpectCh = pSpectInfoParams->pSpectCh;
        // Processing for each tCsrScanResultInfo in the tCsrScanResult DLink list
        for (chn_num = 0; chn_num < pSpectInfoParams->numSpectChans; chn_num++) {

            /*
             *  if the Beacon has channel ID, use it other wise we will 
             *  rely on the channelIdSelf
             */
            if(pScanResult->BssDescriptor.channelId == 0)
                channel_id = pScanResult->BssDescriptor.channelIdSelf;
            else
                channel_id = pScanResult->BssDescriptor.channelId;

            if (channel_id == pSpectCh->chNum) {        
                if (pSpectCh->rssiAgr < pScanResult->BssDescriptor.rssi)
                    pSpectCh->rssiAgr = pScanResult->BssDescriptor.rssi;

                ++pSpectCh->bssCount; // Increment the count of BSS

                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "In %s, bssdes.ch_self=%d, bssdes.ch_ID=%d, bssdes.rssi=%d, SpectCh.bssCount=%d, pScanResult=0x%x",
                  __FUNCTION__, pScanResult->BssDescriptor.channelIdSelf, pScanResult->BssDescriptor.channelId, 
                  pScanResult->BssDescriptor.rssi, pSpectCh->bssCount, pScanResult);
                         
                 pSpectCh++;
                 break;
           } else {
             pSpectCh++;
           }
        }

        pScanResult = sme_ScanResultGetNext(halHandle, pResult);
    }

    // Calculate the weights for all channels in the spectrum pSpectCh
    pSpectCh = pSpectInfoParams->pSpectCh;

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Spectrum Channels Weight", __FUNCTION__);

    for (chn_num = 0; chn_num < (pSpectInfoParams->numSpectChans); chn_num++) {
    
        /*
          rssi : Maximum received signal strength among all BSS on that channel
          bssCount : Number of BSS on that channel
        */

        rssi = (v_S7_t)pSpectCh->rssiAgr;

        pSpectCh->weight = SAPDFS_NORMALISE_1000 * sapweightRssiCount(rssi, pSpectCh->bssCount);

        //------ Debug Info ------ 
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Chan=%d Weight= %d rssiAgr=%d bssCount=%d", __FUNCTION__, pSpectCh->chNum,
            pSpectCh->weight, pSpectCh->rssiAgr, pSpectCh->bssCount);
        //------ Debug Info ------ 
        pSpectCh++;
    }
}

/*==========================================================================
  FUNCTION    sapChanSelExit

  DESCRIPTION 
    Exit function for free out the allocated memory, to be called 
    at the end of the dfsSelectChannel function
    
  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    pSpectInfoParams       : Pointer to the tSapChSelSpectInfo structure
   
  RETURN VALUE
    void     : NULL
  
  SIDE EFFECTS 
============================================================================*/
void sapChanSelExit( tSapChSelSpectInfo *pSpectInfoParams )
{
    // Free all the allocated memory
    vos_mem_free(pSpectInfoParams->pSpectCh);
}

/*==========================================================================
  FUNCTION    sapSortChlWeight

  DESCRIPTION 
    Funtion to sort the channels with the least weight first
    
  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    pSpectInfoParams       : Pointer to the tSapChSelSpectInfo structure
   
  RETURN VALUE
    void     : NULL
  
  SIDE EFFECTS 
============================================================================*/
void sapSortChlWeight(tSapChSelSpectInfo *pSpectInfoParams)
{
    tSapSpectChInfo temp;

    tSapSpectChInfo *pSpectCh = NULL;
    v_U32_t i = 0, j = 0, minWeightIndex = 0;

    pSpectCh = pSpectInfoParams->pSpectCh;
#ifdef SOFTAP_CHANNEL_RANGE
    // Sorting the channels as per weights
    for (i = 0; i < pSpectInfoParams->numSpectChans; i++) {
        minWeightIndex = i;
        for( j = i + 1; j < pSpectInfoParams->numSpectChans; j++) {
            if(pSpectCh[j].weight < pSpectCh[minWeightIndex].weight) {
                minWeightIndex = j; 
            }
        }
        if(minWeightIndex != i) {
            vos_mem_copy(&temp, &pSpectCh[minWeightIndex], sizeof(*pSpectCh));
            vos_mem_copy(&pSpectCh[minWeightIndex], &pSpectCh[i], sizeof(*pSpectCh));
            vos_mem_copy(&pSpectCh[i], &temp, sizeof(*pSpectCh));
        }
    }
#else
    // Sorting the channels as per weights
    for (i = 0; i < SPECT_24GHZ_CH_COUNT; i++) {
        minWeightIndex = i;
        for( j = i + 1; j < SPECT_24GHZ_CH_COUNT; j++) {
            if(pSpectCh[j].weight < pSpectCh[minWeightIndex].weight) {
                minWeightIndex = j; 
            }
        }
        if(minWeightIndex != i) {
            vos_mem_copy(&temp, &pSpectCh[minWeightIndex], sizeof(*pSpectCh));
            vos_mem_copy(&pSpectCh[minWeightIndex], &pSpectCh[i], sizeof(*pSpectCh));
            vos_mem_copy(&pSpectCh[i], &temp, sizeof(*pSpectCh));
        }
    }
#endif

    /* For testing */
    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Sorted Spectrum Channels Weight", __FUNCTION__);
    pSpectCh = pSpectInfoParams->pSpectCh;
    for (j = 0; j < (pSpectInfoParams->numSpectChans); j++) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Channel=%d Weight= %d rssi=%d bssCount=%d",
                    __FUNCTION__, pSpectCh->chNum, pSpectCh->weight, pSpectCh->rssiAgr, pSpectCh->bssCount);
        pSpectCh++;
    }

}

/*==========================================================================
  FUNCTION    sapSelectChannel

  DESCRIPTION 
    Runs a algorithm to select the best channel to operate in based on BSS 
    rssi and bss count on each channel
    
  DEPENDENCIES 
    NA. 

  PARAMETERS 

    IN
    halHandle       : Pointer to HAL handle
    pResult         : Pointer to tScanResultHandle
   
  RETURN VALUE
    v_U8_t          : Success - channel number, Fail - zero
  
  SIDE EFFECTS 
============================================================================*/
v_U8_t sapSelectChannel(tHalHandle halHandle, tScanResultHandle pScanResult)
{
    // DFS param object holding all the data req by the algo
    tSapChSelSpectInfo oSpectInfoParams = {NULL,0}; 
    tSapChSelSpectInfo *pSpectInfoParams = &oSpectInfoParams; // Memory? NB    

    v_U8_t bestChNum = 0;
#ifdef SOFTAP_CHANNEL_RANGE
    v_U32_t startChannelNum;
    v_U32_t endChannelNum;
    v_U32_t operatingBand;
    v_U8_t  count = 0;
#endif    
    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Running SAP Ch Select", __FUNCTION__);

    // Set to zero tSapChSelParams
    //vos_mem_zero(&sapChSelParams, sizeof(sapChSelParams));

    // Initialize the structure pointed by pSpectInfoParams
    if(sapChanSelInit( halHandle, pSpectInfoParams) != eSAP_TRUE ) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR, "In %s, Ch Select initialization failed", __FUNCTION__);
        return SAP_CHANNEL_NOT_SELECTED;
    }

    // Compute the weight of the entire spectrum in the operating band
    sapComputeSpectWeight( pSpectInfoParams, halHandle, pScanResult);

    // Sort the 20M channel list as per the computed weights, lesser weight first.
    sapSortChlWeight(pSpectInfoParams);

#ifdef SOFTAP_CHANNEL_RANGE
    ccmCfgGetInt( halHandle, WNI_CFG_SAP_CHANNEL_SELECT_START_CHANNEL, &startChannelNum);
    ccmCfgGetInt( halHandle, WNI_CFG_SAP_CHANNEL_SELECT_END_CHANNEL, &endChannelNum);
    ccmCfgGetInt( halHandle, WNI_CFG_SAP_CHANNEL_SELECT_OPERATING_BAND, &operatingBand);

    /*Loop till get the best channel in the given range */
    for(count=0; count < pSpectInfoParams->numSpectChans ; count++)
    {
        if((startChannelNum <= pSpectInfoParams->pSpectCh[count].chNum)&&
          ( endChannelNum >= pSpectInfoParams->pSpectCh[count].chNum))
        {
            bestChNum = (v_U8_t)pSpectInfoParams->pSpectCh[count].chNum;
            break;
        }
    }

#else
    // Get the first channel in sorted array as best 20M Channel
    bestChNum = (v_U8_t)pSpectInfoParams->pSpectCh[0].chNum;

#endif

    // Free all the allocated memory
    sapChanSelExit(pSpectInfoParams);

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Running SAP Ch select Completed, Ch=%d",
                __FUNCTION__, bestChNum);

    if (bestChNum > 0 && bestChNum <= 252)
        return bestChNum;
    else
        return SAP_CHANNEL_NOT_SELECTED;
}

