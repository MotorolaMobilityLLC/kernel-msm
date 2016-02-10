/*
 * Copyright (c) 2012-2015 The Linux Foundation. All rights reserved.
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

/*===========================================================================

                      s a p C h S e l e c t . C
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
#ifdef ANI_OS_TYPE_QNX
#include "stdio.h"
#endif
#include "wlan_hdd_main.h"

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
#include "limUtils.h"
#include "parserApi.h"
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

/*--------------------------------------------------------------------------
  Function definitions
--------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
  Defines
--------------------------------------------------------------------------*/
#define SAP_DEBUG

#define IS_RSSI_VALID( extRssi, rssi ) \
( \
   ((extRssi < rssi)?eANI_BOOLEAN_TRUE:eANI_BOOLEAN_FALSE) \
)

#define SET_ACS_BAND(acs_band, pSapCtx) \
{ \
    if (pSapCtx->acs_cfg->start_ch <= 14 && \
            pSapCtx->acs_cfg->end_ch <= 14) \
        acs_band = eCSR_DOT11_MODE_11g; \
    else \
        acs_band = eCSR_DOT11_MODE_11a;\
}

#ifdef FEATURE_WLAN_CH_AVOID
sapSafeChannelType safeChannels[NUM_20MHZ_RF_CHANNELS] =
{
  /*CH  , SAFE, default safe */
    {1  , VOS_TRUE},      //RF_CHAN_1,
    {2  , VOS_TRUE},      //RF_CHAN_2,
    {3  , VOS_TRUE},      //RF_CHAN_3,
    {4  , VOS_TRUE},      //RF_CHAN_4,
    {5  , VOS_TRUE},      //RF_CHAN_5,
    {6  , VOS_TRUE},      //RF_CHAN_6,
    {7  , VOS_TRUE},      //RF_CHAN_7,
    {8  , VOS_TRUE},      //RF_CHAN_8,
    {9  , VOS_TRUE},      //RF_CHAN_9,
    {10 , VOS_TRUE},      //RF_CHAN_10,
    {11 , VOS_TRUE},      //RF_CHAN_11,
    {12 , VOS_TRUE},      //RF_CHAN_12,
    {13 , VOS_TRUE},      //RF_CHAN_13,
    {14 , VOS_TRUE},      //RF_CHAN_14,
    {240, VOS_TRUE},      //RF_CHAN_240,
    {244, VOS_TRUE},      //RF_CHAN_244,
    {248, VOS_TRUE},      //RF_CHAN_248,
    {252, VOS_TRUE},      //RF_CHAN_252,
    {208, VOS_TRUE},      //RF_CHAN_208,
    {212, VOS_TRUE},      //RF_CHAN_212,
    {216, VOS_TRUE},      //RF_CHAN_216,
    {36 , VOS_TRUE},      //RF_CHAN_36,
    {40 , VOS_TRUE},      //RF_CHAN_40,
    {44 , VOS_TRUE},      //RF_CHAN_44,
    {48 , VOS_TRUE},      //RF_CHAN_48,
    {52 , VOS_TRUE},      //RF_CHAN_52,
    {56 , VOS_TRUE},      //RF_CHAN_56,
    {60 , VOS_TRUE},      //RF_CHAN_60,
    {64 , VOS_TRUE},      //RF_CHAN_64,
    {100, VOS_TRUE},      //RF_CHAN_100,
    {104, VOS_TRUE},      //RF_CHAN_104,
    {108, VOS_TRUE},      //RF_CHAN_108,
    {112, VOS_TRUE},      //RF_CHAN_112,
    {116, VOS_TRUE},      //RF_CHAN_116,
    {120, VOS_TRUE},      //RF_CHAN_120,
    {124, VOS_TRUE},      //RF_CHAN_124,
    {128, VOS_TRUE},      //RF_CHAN_128,
    {132, VOS_TRUE},      //RF_CHAN_132,
    {136, VOS_TRUE},      //RF_CHAN_136,
    {140, VOS_TRUE},      //RF_CHAN_140,
#ifdef FEATURE_WLAN_CH144
    {144, VOS_TRUE},      //RF_CHAN_144,
#endif
    {149, VOS_TRUE},      //RF_CHAN_149,
    {153, VOS_TRUE},      //RF_CHAN_153,
    {157, VOS_TRUE},      //RF_CHAN_157,
    {161, VOS_TRUE},      //RF_CHAN_161,
    {165, VOS_TRUE},      //RF_CHAN_165,
};
#endif

typedef struct
{
    v_U16_t chStartNum;
    v_U32_t weight;
} sapAcsChannelInfo;

#define ACS_WEIGHT_MAX     4444

sapAcsChannelInfo acsHT40Channels5G[ ] = {
    {36,   ACS_WEIGHT_MAX},
    {44,   ACS_WEIGHT_MAX},
    {52,   ACS_WEIGHT_MAX},
    {60,   ACS_WEIGHT_MAX},
    {100,  ACS_WEIGHT_MAX},
    {108,  ACS_WEIGHT_MAX},
    {116,  ACS_WEIGHT_MAX},
    {124,  ACS_WEIGHT_MAX},
    {132,  ACS_WEIGHT_MAX},
    {140,  ACS_WEIGHT_MAX},
    {149,  ACS_WEIGHT_MAX},
    {157,  ACS_WEIGHT_MAX},
};

sapAcsChannelInfo acsHT80Channels[ ] = {
    {36,   ACS_WEIGHT_MAX},
    {52,   ACS_WEIGHT_MAX},
    {100,  ACS_WEIGHT_MAX},
    {116,  ACS_WEIGHT_MAX},
    {132,  ACS_WEIGHT_MAX},
    {149,  ACS_WEIGHT_MAX},
};

sapAcsChannelInfo acsHT40Channels24G[ ] = {
    {1,    ACS_WEIGHT_MAX},
    {2,    ACS_WEIGHT_MAX},
    {3,    ACS_WEIGHT_MAX},
    {4,    ACS_WEIGHT_MAX},
    {9,    ACS_WEIGHT_MAX},
};

#define CHANNEL_165  165

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
/**
 * sap_check_n_add_channel() - checks and add given channel in sap context's
 * avoid_channels_info struct
 * @sap_ctx:           sap context.
 * @new_channel:       channel to be added to sap_ctx's avoid ch info
 *
 * sap_ctx contains sap_avoid_ch_info strcut containing the list of channels on
 * which MDM device's AP with MCC was detected. This function will add channels
 * to that list after checking for duplicates.
 *
 * Return: true: if channel was added or already present
 *   else false: if channel list was already full.
 */
bool
sap_check_n_add_channel(ptSapContext sap_ctx,
			uint8_t new_channel)
{
	uint8_t i = 0;
	struct sap_avoid_channels_info *ie_info =
		&sap_ctx->sap_detected_avoid_ch_ie;

	for (i = 0; i < sizeof(ie_info->channels); i++) {
		if (ie_info->channels[i] == new_channel)
			break;

		if (ie_info->channels[i] == 0) {
			ie_info->channels[i] = new_channel;
			break;
		}
	}
	if(i == sizeof(ie_info->channels))
		return false;
	else
		return true;
}

/**
 * sap_check_n_add_overlapped_chnls() - checks & add overlapped channels
 *                                      to primary channel in 2.4Ghz band.
 * @sap_ctx:           sap context.
 * @primary_chnl:      primary channel to be avoided.
 *
 * sap_ctx contains sap_avoid_ch_info struct containing the list of channels on
 * which MDM device's AP with MCC was detected. This function will add channels
 * to that list after checking for duplicates.
 *
 * Return: true: if channel was added or already present
 *   else false: if channel list was already full.
 */
static bool
sap_check_n_add_overlapped_chnls(ptSapContext sap_ctx,
                                 uint8_t primary_channel)
{
	uint8_t i = 0, j = 0, upper_chnl = 0, lower_chnl = 0;
	struct sap_avoid_channels_info *ie_info =
		&sap_ctx->sap_detected_avoid_ch_ie;
	/*
	 * if primary channel less than channel 1 or out of 2g band then
	 * no further process is required. return true in this case.
	 */
	if (primary_channel < CHANNEL_1 || primary_channel > CHANNEL_14)
		return true;

	/* lower channel is one channel right before primary channel */
	lower_chnl = primary_channel - 1;
	/* upper channel is one channel right after primary channel */
	upper_chnl = primary_channel + 1;

	/* lower channel needs to be non-zero, zero is not valid channel */
	if (lower_chnl > (CHANNEL_1 - 1)) {
		for (i = 0; i < sizeof(ie_info->channels); i++) {
			if (ie_info->channels[i] == lower_chnl)
				break;
			if (ie_info->channels[i] == 0) {
				ie_info->channels[i] = lower_chnl;
				break;
			}
		}
	}
	/* upper channel needs to be atleast last channel in 2.4Ghz band */
	if (upper_chnl < (CHANNEL_14 + 1)) {
		for (j = 0; j < sizeof(ie_info->channels); j++) {
			if (ie_info->channels[j] == upper_chnl)
				break;
			if (ie_info->channels[j] == 0) {
				ie_info->channels[j] = upper_chnl;
				break;
			}
		}
	}
	if (i == sizeof(ie_info->channels) || j == sizeof(ie_info->channels))
		return false;
	else
		return true;
}

/**
 * sap_process_avoid_ie() - processes the detected Q2Q IE
 * context's avoid_channels_info struct
 * @hal:                hal handle
 * @sap_ctx:            sap context.
 * @scan_result:        scan results for ACS scan.
 * @spect_info:         spectrum weights array to update
 *
 * Detection of Q2Q IE indicates presence of another MDM device with its AP
 * operating in MCC mode. This function parses the scan results and processes
 * the Q2Q IE if found. It then extracts the channels and populates them in
 * sap_ctx struct. It also increases the weights of those channels so that
 * ACS logic will avoid those channels in its selection algorithm.
 *
 * Return: void
 */

void
sap_process_avoid_ie(tHalHandle hal,
		     ptSapContext sap_ctx,
		     tScanResultHandle scan_result,
		     tSapChSelSpectInfo *spect_info)
{
	uint32_t total_ie_len = 0;
	uint8_t *temp_ptr = NULL;
	uint8_t i = 0;
	struct sAvoidChannelIE *avoid_ch_ie;
	tCsrScanResultInfo *node = NULL;
	tpAniSirGlobal mac_ctx = NULL;
	tSapSpectChInfo *spect_ch = NULL;

	mac_ctx = PMAC_STRUCT(hal);
	spect_ch = spect_info->pSpectCh;
	node = sme_ScanResultGetFirst(hal, scan_result);

	while (node) {
		total_ie_len = (node->BssDescriptor.length +
				sizeof(tANI_U16) + sizeof(tANI_U32) -
				sizeof(tSirBssDescription));
		temp_ptr = cfg_get_vendor_ie_ptr_from_oui(mac_ctx,
				SIR_MAC_QCOM_VENDOR_OUI,
				SIR_MAC_QCOM_VENDOR_SIZE,
				((tANI_U8 *)&node->BssDescriptor.ieFields),
				total_ie_len);

		if (temp_ptr) {
			avoid_ch_ie = (struct sAvoidChannelIE*)temp_ptr;
			if (avoid_ch_ie->type != QCOM_VENDOR_IE_MCC_AVOID_CH) {
				continue;
			}
			sap_ctx->sap_detected_avoid_ch_ie.present = 1;
			VOS_TRACE(VOS_MODULE_ID_SAP,
				  VOS_TRACE_LEVEL_DEBUG,
				  "Q2Q IE - avoid ch %d",
				  avoid_ch_ie->channel);
			/* add this channel to to_avoid channel list */
			sap_check_n_add_channel(sap_ctx,
						avoid_ch_ie->channel);
			sap_check_n_add_overlapped_chnls(sap_ctx,
						avoid_ch_ie->channel);
			/*
			 * Mark weight of these channel present in IE to MAX
			 * so that ACS logic will to avoid thse channels
			 */
			for (i = 0; i < spect_info->numSpectChans; i++) {
				if (spect_ch[i].chNum == avoid_ch_ie->channel) {
					/*
					 * weight is set more than max so that,
					 * in the case of other channels being
					 * assigned max weight due to noise,
					 * they may be preferred over channels
					 * with Q2Q IE.
					 */
					spect_ch[i].weight = ACS_WEIGHT_MAX + 1;
					spect_ch[i].weight_copy =
							ACS_WEIGHT_MAX + 1;
					break;
				}
			}
		} /* if (temp_ptr) */
		node = sme_ScanResultGetNext(hal, scan_result);
	}
}
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

#ifdef FEATURE_WLAN_CH_AVOID
/*==========================================================================
  FUNCTION    sapUpdateUnsafeChannelList

  DESCRIPTION
    Function  Undate unsafe channel list table

  DEPENDENCIES
    NA.

  IN
    SapContext pointer

  RETURN VALUE
    NULL
============================================================================*/
void sapUpdateUnsafeChannelList(ptSapContext pSapCtx)
{
   v_U16_t   i, j;

   v_PVOID_t pvosGCtx = vos_get_global_context(VOS_MODULE_ID_SAP, NULL);
   struct hdd_context_s *hdd_ctxt;

   if (NULL == pvosGCtx)
   {
       VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                  "VOSS Global Context is NULL");
       return ;
   }

   hdd_ctxt = (struct hdd_context_s *) vos_get_context(VOS_MODULE_ID_HDD, pvosGCtx);

   if (NULL == hdd_ctxt)
   {
       VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                  "HDD Context is NULL");
       return ;
   }

   /* Flush, default set all channel safe */
   for (i = 0; i < NUM_20MHZ_RF_CHANNELS; i++)
   {
      safeChannels[i].isSafe = VOS_TRUE;
   }

   /* Try to find unsafe channel */
#if defined(FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE) || \
    defined(WLAN_FEATURE_MBSSID)
   for (i = 0; i < NUM_20MHZ_RF_CHANNELS; i++) {
        if (pSapCtx->dfs_ch_disable == VOS_TRUE) {
            if (VOS_IS_DFS_CH(safeChannels[i].channelNumber)) {
                safeChannels[i].isSafe = VOS_FALSE;
                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                    "%s: DFS Ch %d is not safe in Concurrent mode", __func__,
                    safeChannels[i].channelNumber);
           }
        }
    }
#endif

   for (i = 0; i < hdd_ctxt->unsafe_channel_count; i++)
   {
      for (j = 0; j < NUM_20MHZ_RF_CHANNELS; j++)
      {
         if(safeChannels[j].channelNumber == hdd_ctxt->unsafe_channel_list[i])
         {
            /* Found unsafe channel, update it */
            safeChannels[j].isSafe = VOS_FALSE;
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                      "%s : CH %d is not safe",
                      __func__, hdd_ctxt->unsafe_channel_list[i]);
            break;
         }
      }
   }

   return;
}

#endif /* FEATURE_WLAN_CH_AVOID */

/*==========================================================================
  FUNCTION    sapCleanupChannelList

  DESCRIPTION
    Function sapCleanupChannelList frees up the memory allocated to the channel list.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    NULL

  RETURN VALUE
    NULL
============================================================================*/

void sapCleanupChannelList
(
#ifdef WLAN_FEATURE_MBSSID
    v_PVOID_t pvosGCtx
#else
    void
#endif
)
{
#ifndef WLAN_FEATURE_MBSSID
    v_PVOID_t pvosGCtx = vos_get_global_context(VOS_MODULE_ID_SAP, NULL);
#endif
    ptSapContext pSapCtx;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO,
               "Cleaning up the channel list structure");

    if (NULL == pvosGCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                   "SAP Global Context is NULL");
        return ;
    }

	pSapCtx = VOS_GET_SAP_CB(pvosGCtx);
    if (NULL == pSapCtx)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_FATAL,
                   "SAP Context is NULL");
        return ;
    }

    pSapCtx->SapChnlList.numChannel = 0;
    if (pSapCtx->SapChnlList.channelList) {
        vos_mem_free(pSapCtx->SapChnlList.channelList);
        pSapCtx->SapChnlList.channelList = NULL;
    }

    pSapCtx->SapAllChnlList.numChannel = 0;
    if (pSapCtx->SapAllChnlList.channelList) {
        vos_mem_free(pSapCtx->SapAllChnlList.channelList);
        pSapCtx->SapAllChnlList.channelList = NULL;
    }
}

/*==========================================================================
  FUNCTION    sapSelectPreferredChannelFromChannelList

  DESCRIPTION
    Function sapSelectPreferredChannelFromChannelList calculates the best channel
    among the configured channel list. If channel list not configured then returns
    the best channel calculated among all the channel list.

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    *pSpectInfoParams  : Pointer to tSapChSelSpectInfo structure
    bestChNum: best channel already calculated among all the chanels
    pSapCtx: having info of channel list from which best channel is selected

  RETURN VALUE
    v_U8_t:  best channel
============================================================================*/
v_U8_t sapSelectPreferredChannelFromChannelList(v_U8_t bestChNum,
                                                ptSapContext pSapCtx,
                                                tSapChSelSpectInfo *pSpectInfoParams)
{
    v_U8_t j = 0;
    v_U8_t count = 0;

    //If Channel List is not Configured don't do anything
    //Else return the Best Channel from the Channel List
    if((NULL == pSapCtx->acs_cfg->ch_list) ||
       (NULL == pSpectInfoParams))
    {
        return bestChNum;
    }

    if (bestChNum > 0 && bestChNum <= 252)
    {
        for(count=0; count < pSpectInfoParams->numSpectChans ; count++)
        {
            bestChNum = (v_U8_t)pSpectInfoParams->pSpectCh[count].chNum;
            // Select the best channel from allowed list
            for(j=0;j < pSapCtx->acs_cfg->ch_list_count;j++)
            {
                if( (pSapCtx->acs_cfg->ch_list[j]) == bestChNum)
                {
                    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                               "Best channel computed from Channel List is: %d",
                               bestChNum);
                    return bestChNum;
                }
            }
        }

        return SAP_CHANNEL_NOT_SELECTED;
    }
    else
        return SAP_CHANNEL_NOT_SELECTED;
}


/*==========================================================================
  FUNCTION    sapChanSelInit

  DESCRIPTION
    Function sapChanSelInit allocates the memory, intializes the
         structures used by the channel selection algorithm

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    halHandle          : Pointer to tHalHandle
    *pSpectInfoParams  : Pointer to tSapChSelSpectInfo structure
     pSapCtx           : Pointer to SAP Context

  RETURN VALUE
    v_BOOL_t:  Success or FAIL

  SIDE EFFECTS
============================================================================*/
v_BOOL_t sapChanSelInit(tHalHandle halHandle,
                        tSapChSelSpectInfo *pSpectInfoParams,
                        ptSapContext pSapCtx)
{
    tSapSpectChInfo *pSpectCh = NULL;
    v_U8_t *pChans = NULL;
    v_U16_t channelnum = 0;
    tpAniSirGlobal pMac = PMAC_STRUCT(halHandle);
    v_BOOL_t chSafe = VOS_TRUE;
#ifdef FEATURE_WLAN_CH_AVOID
    v_U16_t i;
#endif
    v_U32_t dfs_master_cap_enabled;
    v_BOOL_t include_dfs_ch = VOS_TRUE;

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s", __func__);

    pSpectInfoParams->numSpectChans = pMac->scan.base20MHzChannels.numChannels;

    // Allocate memory for weight computation of 2.4GHz
    pSpectCh = (tSapSpectChInfo *)vos_mem_malloc((pSpectInfoParams->numSpectChans) * sizeof(*pSpectCh));

    if(pSpectCh == NULL) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                "In %s, VOS_MALLOC_ERR", __func__);
        return eSAP_FALSE;
    }

    vos_mem_zero(pSpectCh, (pSpectInfoParams->numSpectChans) * sizeof(*pSpectCh));

    // Initialize the pointers in the DfsParams to the allocated memory
    pSpectInfoParams->pSpectCh = pSpectCh;

    pChans = pMac->scan.base20MHzChannels.channelList;
#if defined(FEATURE_WLAN_STA_AP_MODE_DFS_DISABLE) || defined(WLAN_FEATURE_MBSSID)
        if (pSapCtx->dfs_ch_disable == VOS_TRUE)
            include_dfs_ch = VOS_FALSE;
#endif
        ccmCfgGetInt(halHandle, WNI_CFG_DFS_MASTER_ENABLED,
                                                     &dfs_master_cap_enabled);
        if (dfs_master_cap_enabled == 0)
            include_dfs_ch = VOS_FALSE;

    // Fill the channel number in the spectrum in the operating freq band
    for (channelnum = 0;
            channelnum < pSpectInfoParams->numSpectChans;
                channelnum++, pChans++, pSpectCh++) {
        chSafe = VOS_TRUE;

        /* check if the channel is in NOL blacklist */
        if(sapDfsIsChannelInNolList(pSapCtx, *pChans,
                        PHY_SINGLE_CHANNEL_CENTERED))
        {
            VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                "In %s, Ch %d is in NOL list", __func__, *pChans);
            chSafe = VOS_FALSE;
            continue;
        }

        if (include_dfs_ch == VOS_FALSE) {
            if (VOS_IS_DFS_CH(*pChans)) {
                chSafe = VOS_FALSE;
                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                          "In %s, DFS Ch %d not considered for ACS", __func__,
                          *pChans);
                continue;
            }
        }

#ifdef FEATURE_WLAN_CH_AVOID
        for(i = 0; i < NUM_20MHZ_RF_CHANNELS; i++) {
            if((safeChannels[i].channelNumber == *pChans) &&
                (VOS_FALSE == safeChannels[i].isSafe))
            {
                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        "In %s, Ch %d is not safe",
                          __func__, *pChans);
                chSafe = VOS_FALSE;
                break;
            }
        }
#endif /* FEATURE_WLAN_CH_AVOID */

        /* OFDM rates are not supported on channel 14 */
        if(*pChans == 14 &&
               eCSR_DOT11_MODE_11b != pSapCtx->csrRoamProfile.phyMode)
        {
            continue;
        }

        if (VOS_TRUE == chSafe)
        {
            pSpectCh->chNum = *pChans;
            pSpectCh->valid = eSAP_TRUE;
            pSpectCh->rssiAgr = SOFTAP_MIN_RSSI;// Initialise for all channels
            pSpectCh->channelWidth = SOFTAP_HT20_CHANNELWIDTH; // Initialise 20MHz for all the Channels
        }
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

    rssicountWeight =  rssiWeight + countWeight;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, rssiWeight=%d, countWeight=%d, rssicountWeight=%d",
              __func__, rssiWeight, countWeight, rssicountWeight);

    return(rssicountWeight);
}


/*==========================================================================
  FUNCTION    sapInterferenceRssiCount

  DESCRIPTION
    Function sapInterferenceRssiCount Considers the Adjacent channel rssi
    and data count(here number of BSS observed)

  DEPENDENCIES
    NA.

  PARAMETERS

    pSpectCh    : Channel Information

  RETURN VALUE
    NA.

  SIDE EFFECTS
============================================================================*/
void sapInterferenceRssiCount(tSapSpectChInfo *pSpectCh)
{
    tSapSpectChInfo *pExtSpectCh = NULL;
    v_S31_t rssi;

    if (NULL == pSpectCh)
    {
        VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: pSpectCh is NULL", __func__);
        return;
    }

    switch(pSpectCh->chNum)
    {
        case CHANNEL_1:
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
        break;

        case CHANNEL_2:
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;
        case CHANNEL_3:
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;
        case CHANNEL_4:
            pExtSpectCh = (pSpectCh - 3);
            if(pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        case CHANNEL_5:
        case CHANNEL_6:
        case CHANNEL_7:
        case CHANNEL_8:
        case CHANNEL_9:
        case CHANNEL_10:
            pExtSpectCh = (pSpectCh - 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if ((pExtSpectCh != NULL) && (pExtSpectCh->chNum <= CHANNEL_14))
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if ((pExtSpectCh != NULL) && (pExtSpectCh->chNum <= CHANNEL_14))
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if ((pExtSpectCh != NULL) && (pExtSpectCh->chNum <= CHANNEL_14))
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 4);
            if ((pExtSpectCh != NULL) && (pExtSpectCh->chNum <= CHANNEL_14))
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        case CHANNEL_11:
            pExtSpectCh = (pSpectCh - 4);
            if(pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }

            pExtSpectCh = (pSpectCh - 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if ((pExtSpectCh != NULL) && (pExtSpectCh->chNum <= CHANNEL_14))
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if ((pExtSpectCh != NULL) && (pExtSpectCh->chNum <= CHANNEL_14))
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 3);
            if ((pExtSpectCh != NULL) && (pExtSpectCh->chNum <= CHANNEL_14))
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        case CHANNEL_12:
            pExtSpectCh = (pSpectCh - 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }

            pExtSpectCh = (pSpectCh - 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if ((pExtSpectCh != NULL) && (pExtSpectCh->chNum <= CHANNEL_14))
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 2);
            if ((pExtSpectCh != NULL) && (pExtSpectCh->chNum <= CHANNEL_14))
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        case CHANNEL_13:
            pExtSpectCh = (pSpectCh - 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }

            pExtSpectCh = (pSpectCh - 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if(pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh + 1);
            if ((pExtSpectCh != NULL) && (pExtSpectCh->chNum <= CHANNEL_14))
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        case CHANNEL_14:
            pExtSpectCh = (pSpectCh - 1);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FIRST_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 2);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_SEC_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 3);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_THIRD_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            pExtSpectCh = (pSpectCh - 4);
            if (pExtSpectCh != NULL)
            {
                ++pExtSpectCh->bssCount;
                rssi = pSpectCh->rssiAgr +
                       SAP_24GHZ_FOURTH_OVERLAP_CHAN_RSSI_EFFECT_PRIMARY;
                if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                {
                    pExtSpectCh->rssiAgr = rssi;
                }
                if (pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                    pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
            }
            break;

        default:
            break;
    }
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
    sap_ctx                : Pointer to Sap context
  RETURN VALUE
    void     : NULL

  SIDE EFFECTS
============================================================================*/
void sapComputeSpectWeight( tSapChSelSpectInfo* pSpectInfoParams,
                            tHalHandle halHandle, tScanResultHandle pResult,
                            ptSapContext sap_ctx)
{
    v_S7_t rssi = 0;
    v_U8_t chn_num = 0;
    v_U8_t channel_id = 0;

    tCsrScanResultInfo *pScanResult;
    tSapSpectChInfo *pSpectCh   = pSpectInfoParams->pSpectCh;
    v_U32_t operatingBand = eCSR_DOT11_MODE_11g;
    v_U16_t channelWidth;
    v_U16_t secondaryChannelOffset;
    v_U16_t centerFreq;
    v_U16_t vhtSupport;
    v_U32_t ieLen = 0;
    tSirProbeRespBeacon *pBeaconStruct;
    tpAniSirGlobal  pMac = (tpAniSirGlobal) halHandle;

    pBeaconStruct = vos_mem_malloc(sizeof(tSirProbeRespBeacon));
    if ( NULL == pBeaconStruct )
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "Unable to allocate memory in sapComputeSpectWeight\n");
        return;
    }
    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Computing spectral weight", __func__);

    /**
    * Soft AP specific channel weight calculation using DFS formula
    */
    SET_ACS_BAND(operatingBand, sap_ctx);

    pScanResult = sme_ScanResultGetFirst(halHandle, pResult);

    while (pScanResult) {
        pSpectCh = pSpectInfoParams->pSpectCh;
        // Defining the default values, so that any value will hold the default values
        channelWidth = eHT_CHANNEL_WIDTH_20MHZ;
        secondaryChannelOffset = PHY_SINGLE_CHANNEL_CENTERED;
        vhtSupport = 0;
        centerFreq = 0;

        if (pScanResult->BssDescriptor.ieFields != NULL)
        {
            ieLen = (pScanResult->BssDescriptor.length + sizeof(tANI_U16) + sizeof(tANI_U32) - sizeof(tSirBssDescription));
            vos_mem_set((tANI_U8 *) pBeaconStruct, sizeof(tSirProbeRespBeacon), 0);

            if ((sirParseBeaconIE(pMac, pBeaconStruct,(tANI_U8 *)( pScanResult->BssDescriptor.ieFields), ieLen)) == eSIR_SUCCESS)
            {
                if (pBeaconStruct->HTCaps.present && pBeaconStruct->HTInfo.present)
                {
                    channelWidth = pBeaconStruct->HTCaps.supportedChannelWidthSet;
                    secondaryChannelOffset = pBeaconStruct->HTInfo.secondaryChannelOffset;
                    if(pBeaconStruct->VHTOperation.present)
                    {
                        vhtSupport = pBeaconStruct->VHTOperation.present;
                        if(pBeaconStruct->VHTOperation.chanWidth > WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ)
                        {
                            channelWidth = eHT_CHANNEL_WIDTH_80MHZ;
                            centerFreq = pBeaconStruct->VHTOperation.chanCenterFreqSeg1;
                        }
                    }
                }
            }
        }
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

            if (pSpectCh && (channel_id == pSpectCh->chNum)) {
                if (pSpectCh->rssiAgr < pScanResult->BssDescriptor.rssi)
                    pSpectCh->rssiAgr = pScanResult->BssDescriptor.rssi;

                ++pSpectCh->bssCount; // Increment the count of BSS

                if(operatingBand) // Connsidering the Extension Channel only in a channels
                {
                    /* Updating the received ChannelWidth */
                    if (pSpectCh->channelWidth != channelWidth)
                        pSpectCh->channelWidth = channelWidth;
                    /* If received ChannelWidth is other than HT20, we need to update the extension channel Params as well */
                    /* channelWidth == 0, HT20 */
                    /* channelWidth == 1, HT40 */
                    /* channelWidth == 2, VHT80*/
                    switch(pSpectCh->channelWidth)
                    {
                        case eHT_CHANNEL_WIDTH_40MHZ: //HT40
                            switch( secondaryChannelOffset)
                            {
                                tSapSpectChInfo *pExtSpectCh = NULL;
                                case PHY_DOUBLE_CHANNEL_LOW_PRIMARY: // Above the Primary Channel
                                    pExtSpectCh = (pSpectCh + 1);
                                    if(pExtSpectCh != NULL)
                                    {
                                        ++pExtSpectCh->bssCount;
                                        rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                        // REducing the rssi by -20 and assigning it to Extension channel
                                        if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                        {
                                            pExtSpectCh->rssiAgr = rssi;
                                        }
                                        if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                            pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                    }
                                break;

                                case PHY_DOUBLE_CHANNEL_HIGH_PRIMARY: // Below the Primary channel
                                    pExtSpectCh = (pSpectCh - 1);
                                    if(pExtSpectCh != NULL)
                                    {
                                        rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                        if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                        {
                                            pExtSpectCh->rssiAgr = rssi;
                                        }
                                        if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                            pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                        ++pExtSpectCh->bssCount;
                                    }
                                break;
                            }
                        break;
                        case eHT_CHANNEL_WIDTH_80MHZ: // VHT80
                            if((centerFreq - channel_id) == 6)
                            {
                                tSapSpectChInfo *pExtSpectCh = NULL;
                                pExtSpectCh = (pSpectCh + 1);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi; // Reducing the rssi by -20 and assigning it to Subband 1
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh + 2);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND2_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi; // Reducing the rssi by -30 and assigning it to Subband 2
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh + 3);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND3_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi; // Reducing the rssi by -40 and assigning it to Subband 3
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                            }
                            else if((centerFreq - channel_id) == 2)
                            {
                                tSapSpectChInfo *pExtSpectCh = NULL;
                                pExtSpectCh = (pSpectCh - 1 );
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh + 1);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh + 2);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND2_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                            }
                            else if((centerFreq - channel_id) == -2)
                            {
                                tSapSpectChInfo *pExtSpectCh = NULL;
                                pExtSpectCh = (pSpectCh - 1 );
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh - 2);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND2_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh + 1);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                            }
                            else if((centerFreq - channel_id) == -6)
                            {
                                tSapSpectChInfo *pExtSpectCh = NULL;
                                pExtSpectCh = (pSpectCh - 1 );
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND1_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh - 2);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND2_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                                pExtSpectCh = (pSpectCh - 3);
                                if(pExtSpectCh != NULL)
                                {
                                    ++pExtSpectCh->bssCount;
                                    rssi = pSpectCh->rssiAgr + SAP_SUBBAND3_RSSI_EFFECT_PRIMARY;
                                    if (IS_RSSI_VALID(pExtSpectCh->rssiAgr, rssi))
                                    {
                                        pExtSpectCh->rssiAgr = rssi;
                                    }
                                    if(pExtSpectCh->rssiAgr < SOFTAP_MIN_RSSI)
                                        pExtSpectCh->rssiAgr = SOFTAP_MIN_RSSI;
                                }
                            }
                        break;
                    }
                }

                if(operatingBand == eCSR_DOT11_MODE_11g)
                {
                     sapInterferenceRssiCount(pSpectCh);
                }

                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                   "In %s, bssdes.ch_self=%d, bssdes.ch_ID=%d, bssdes.rssi=%d, SpectCh.bssCount=%d, pScanResult=%p, ChannelWidth %d, secondaryChanOffset %d, center frequency %d \n",
                  __func__, pScanResult->BssDescriptor.channelIdSelf, pScanResult->BssDescriptor.channelId, pScanResult->BssDescriptor.rssi, pSpectCh->bssCount, pScanResult,pSpectCh->channelWidth,secondaryChannelOffset,centerFreq);
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

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Spectrum Channels Weight", __func__);

    for (chn_num = 0; chn_num < (pSpectInfoParams->numSpectChans); chn_num++) {

        /*
          rssi : Maximum received signal strength among all BSS on that channel
          bssCount : Number of BSS on that channel
        */

        rssi = (v_S7_t)pSpectCh->rssiAgr;

        pSpectCh->weight = SAPDFS_NORMALISE_1000 * sapweightRssiCount(rssi, pSpectCh->bssCount);
        pSpectCh->weight_copy = pSpectCh->weight;

        //------ Debug Info ------
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
             "In %s, Chan=%d Weight= %d rssiAgr=%d bssCount=%d", __func__,
             pSpectCh->chNum, pSpectCh->weight,
             pSpectCh->rssiAgr, pSpectCh->bssCount);
        //------ Debug Info ------
        pSpectCh++;
    }
    vos_mem_free(pBeaconStruct);
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
    Funtion to sort the channels with the least weight first for 20MHz channels

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
}

/*==========================================================================
  FUNCTION    sapSortChlWeightHT80

  DESCRIPTION
    Funtion to sort the channels with the least weight first for HT80 channels

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pSpectInfoParams       : Pointer to the tSapChSelSpectInfo structure

  RETURN VALUE
    void     : NULL

  SIDE EFFECTS
============================================================================*/
void sapSortChlWeightHT80(tSapChSelSpectInfo *pSpectInfoParams)
{
    v_U8_t i, j, n;
    tSapSpectChInfo *pSpectInfo;
    v_U8_t minIdx;

    pSpectInfo = pSpectInfoParams->pSpectCh;
    /* for each HT80 channel, calculate the combined weight of the
       four 20MHz weight */
    for (i = 0; i < ARRAY_SIZE(acsHT80Channels); i++)
    {
        for (j = 0; j < pSpectInfoParams->numSpectChans; j++)
        {
            if ( pSpectInfo[j].chNum == acsHT80Channels[i].chStartNum )
                break;
        }
        if (j == pSpectInfoParams->numSpectChans)
            continue;

        /*found the channel, add the 4 adjacent channels' weight*/
        if (((pSpectInfo[j].chNum +4) == pSpectInfo[j+1].chNum) &&
             ((pSpectInfo[j].chNum +8) == pSpectInfo[j+2].chNum) &&
             ((pSpectInfo[j].chNum +12) == pSpectInfo[j+3].chNum))
        {
            acsHT80Channels[i].weight = pSpectInfo[j].weight +
                                           pSpectInfo[j+1].weight +
                                           pSpectInfo[j+2].weight +
                                           pSpectInfo[j+3].weight;
            /* find best channel among 4 channels as the primary channel */
            if ((pSpectInfo[j].weight + pSpectInfo[j+1].weight) <
                    (pSpectInfo[j+2].weight + pSpectInfo[j+3].weight))
            {
                /* lower 2 channels are better choice */
                if (pSpectInfo[j].weight < pSpectInfo[j+1].weight)
                    minIdx = 0;
                else
                    minIdx = 1;
            }
            else
            {
                /* upper 2 channels are better choice */
                if (pSpectInfo[j+2].weight <= pSpectInfo[j+3].weight)
                    minIdx = 2;
                else
                    minIdx = 3;
            }

            /* set all 4 channels to max value first, then reset the
               best channel as the selected primary channel, update its
               weightage with the combined weight value */
            for (n=0; n<4; n++)
                pSpectInfo[j+n].weight = ACS_WEIGHT_MAX * 4;

            pSpectInfo[j+minIdx].weight = acsHT80Channels[i].weight;
        }
        else
        {
            /* some channels does not exist in pSectInfo array,
               skip this channel and those in the same HT80 width*/
            pSpectInfo[j].weight = ACS_WEIGHT_MAX * 4;
            if ((pSpectInfo[j].chNum +4) == pSpectInfo[j+1].chNum)
                pSpectInfo[j+1].weight = ACS_WEIGHT_MAX * 4;
            if ((pSpectInfo[j].chNum +8) == pSpectInfo[j+2].chNum)
                pSpectInfo[j+2].weight = ACS_WEIGHT_MAX * 4;
            if ((pSpectInfo[j].chNum +12) == pSpectInfo[j+3].chNum)
                pSpectInfo[j+3].weight = ACS_WEIGHT_MAX * 4;
        }
    }

    pSpectInfo = pSpectInfoParams->pSpectCh;
    for (j = 0; j < pSpectInfoParams->numSpectChans; j++)
    {
        if ( CHANNEL_165 == pSpectInfo[j].chNum )
        {
            pSpectInfo[j].weight = ACS_WEIGHT_MAX * 4;
            break;
        }
    }

    pSpectInfo = pSpectInfoParams->pSpectCh;
    for (j = 0; j < (pSpectInfoParams->numSpectChans); j++) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                    "In %s, Channel=%d Weight= %d rssi=%d bssCount=%d",
                    __func__, pSpectInfo->chNum, pSpectInfo->weight,
                    pSpectInfo->rssiAgr, pSpectInfo->bssCount);
        pSpectInfo++;
    }

    sapSortChlWeight(pSpectInfoParams);
}

/*==========================================================================
  FUNCTION    sapSortChlWeightHT40_24G

  DESCRIPTION
    Funtion to sort the channels with the least weight first for HT40 channels

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pSpectInfoParams       : Pointer to the tSapChSelSpectInfo structure

  RETURN VALUE
    void     : NULL

  SIDE EFFECTS
============================================================================*/
void sapSortChlWeightHT40_24G(tSapChSelSpectInfo *pSpectInfoParams)
{
    v_U8_t i, j;
    tSapSpectChInfo *pSpectInfo;
    v_U32_t tmpWeight1, tmpWeight2;

    pSpectInfo = pSpectInfoParams->pSpectCh;
    /*for each HT40 channel, calculate the combined weight of the
      two 20MHz weight */
    for (i = 0; i < ARRAY_SIZE(acsHT40Channels24G); i++)
    {
        for (j = 0; j < pSpectInfoParams->numSpectChans; j++)
        {
            if (pSpectInfo[j].chNum == acsHT40Channels24G[i].chStartNum)
                break;
        }
        if (j == pSpectInfoParams->numSpectChans)
            continue;

        if ((pSpectInfo[j].chNum +4) == pSpectInfo[j+4].chNum)
        {
            /* check if there is another channel combination possiblity
               e.g., {1, 5} & {5, 9} */
            if ((pSpectInfo[j+4].chNum + 4)== pSpectInfo[j+8].chNum)
            {
                /* need to compare two channel pairs */
                tmpWeight1 = pSpectInfo[j].weight + pSpectInfo[j+4].weight;
                tmpWeight2 = pSpectInfo[j+4].weight + pSpectInfo[j+8].weight;
                if (tmpWeight1 <= tmpWeight2)
                {
                    if (pSpectInfo[j].weight <= pSpectInfo[j+4].weight)
                    {
                        pSpectInfo[j].weight = tmpWeight1;
                        pSpectInfo[j+4].weight = ACS_WEIGHT_MAX * 2;
                        pSpectInfo[j+8].weight = ACS_WEIGHT_MAX * 2;
                    }
                    else
                    {
                        pSpectInfo[j+4].weight = tmpWeight1;
                        /* for secondary channel selection */
                        pSpectInfo[j].weight = ACS_WEIGHT_MAX * 2 - 1;
                        pSpectInfo[j+8].weight = ACS_WEIGHT_MAX * 2;
                    }
                }
                else
                {
                    if (pSpectInfo[j+4].weight <= pSpectInfo[j+8].weight)
                    {
                        pSpectInfo[j+4].weight = tmpWeight2;
                        pSpectInfo[j].weight = ACS_WEIGHT_MAX * 2;
                        /* for secondary channel selection */
                        pSpectInfo[j+8].weight = ACS_WEIGHT_MAX * 2 - 1;
                    }
                    else
                    {
                        pSpectInfo[j+8].weight = tmpWeight2;
                        pSpectInfo[j].weight = ACS_WEIGHT_MAX * 2;
                        pSpectInfo[j+4].weight = ACS_WEIGHT_MAX * 2;
                    }
                }
            }
            else
            {
                tmpWeight1 = pSpectInfo[j].weight + pSpectInfo[j+4].weight;
                if (pSpectInfo[j].weight <= pSpectInfo[j+4].weight)
                {
                    pSpectInfo[j].weight = tmpWeight1;
                    pSpectInfo[j+4].weight = ACS_WEIGHT_MAX * 2;
                }
                else
                {
                    pSpectInfo[j+4].weight = tmpWeight1;
                    pSpectInfo[j].weight = ACS_WEIGHT_MAX * 2;
                }
            }
        }
        else
            pSpectInfo[j].weight = ACS_WEIGHT_MAX * 2;
    }

    sapSortChlWeight(pSpectInfoParams);
}


/*==========================================================================
  FUNCTION    sapSortChlWeightHT40_5G

  DESCRIPTION
    Funtion to sort the channels with the least weight first for HT40 channels

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    pSpectInfoParams       : Pointer to the tSapChSelSpectInfo structure

  RETURN VALUE
    void     : NULL

  SIDE EFFECTS
============================================================================*/
void sapSortChlWeightHT40_5G(tSapChSelSpectInfo *pSpectInfoParams)
{
    v_U8_t i, j;
    tSapSpectChInfo *pSpectInfo;

    pSpectInfo = pSpectInfoParams->pSpectCh;
    /*for each HT40 channel, calculate the combined weight of the
      two 20MHz weight */
    for (i = 0; i < ARRAY_SIZE(acsHT40Channels5G); i++)
    {
        for (j = 0; j < pSpectInfoParams->numSpectChans; j++)
        {
            if (pSpectInfo[j].chNum == acsHT40Channels5G[i].chStartNum)
                break;
        }
        if (j == pSpectInfoParams->numSpectChans)
            continue;

        /* found the channel, add the two adjacent channels' weight */
        if ( (pSpectInfo[j].chNum +4) == pSpectInfo[j+1].chNum)
        {
            acsHT40Channels5G[i].weight = pSpectInfo[j].weight +
                                           pSpectInfo[j+1].weight;
            /* select better of the adjact channel as the primary channel */
            if (pSpectInfo[j].weight <= pSpectInfo[j+1].weight)
            {
                pSpectInfo[j].weight = acsHT40Channels5G[i].weight;
                /* mark the adjacent channel's weight as max value so
                   that it will be sorted to the bottom */
                pSpectInfo[j+1].weight = ACS_WEIGHT_MAX * 2;
            }
            else
            {
                pSpectInfo[j+1].weight = acsHT40Channels5G[i].weight;
                /* mark the adjacent channel's weight as max value so
                   that it will be sorted to the bottom */
                pSpectInfo[j].weight = ACS_WEIGHT_MAX * 2;
            }

        }
        else
           pSpectInfo[j].weight = ACS_WEIGHT_MAX * 2;
    }

    /* avoid channel 165 by setting its weight to max */
    pSpectInfo = pSpectInfoParams->pSpectCh;
    for (j = 0; j < pSpectInfoParams->numSpectChans; j++)
    {
        if ( CHANNEL_165  == pSpectInfo[j].chNum )
        {
            pSpectInfo[j].weight = ACS_WEIGHT_MAX * 2;
            break;
        }
    }

    pSpectInfo = pSpectInfoParams->pSpectCh;
    for (j = 0; j < (pSpectInfoParams->numSpectChans); j++) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                    "In %s, Channel=%d Weight= %d rssi=%d bssCount=%d",
                    __func__, pSpectInfo->chNum, pSpectInfo->weight,
                    pSpectInfo->rssiAgr, pSpectInfo->bssCount);
        pSpectInfo++;
    }

    sapSortChlWeight(pSpectInfoParams);
}

/*==========================================================================
  FUNCTION    sapSortChlWeightAll

  DESCRIPTION
    Funtion to sort the channels with the least weight first

  DEPENDENCIES
    NA.

  PARAMETERS

    IN
    ptSapContext           : Pointer to the ptSapContext structure
    pSpectInfoParams       : Pointer to the tSapChSelSpectInfo structure

  RETURN VALUE
    void     : NULL

  SIDE EFFECTS
============================================================================*/
void sapSortChlWeightAll(ptSapContext pSapCtx,
                           tSapChSelSpectInfo *pSpectInfoParams,
                           v_U32_t operatingBand)
{
    tSapSpectChInfo *pSpectCh = NULL;
    v_U32_t j = 0;
#ifndef  SOFTAP_CHANNEL_RANGE
    v_U32_t i = 0;
#endif

    pSpectCh = pSpectInfoParams->pSpectCh;
#ifdef SOFTAP_CHANNEL_RANGE

    switch (pSapCtx->acs_cfg->ch_width)
    {
    case eHT_CHANNEL_WIDTH_40MHZ:
        if (eCSR_DOT11_MODE_11a == operatingBand)
            sapSortChlWeightHT40_5G(pSpectInfoParams);
        else
            sapSortChlWeightHT40_24G(pSpectInfoParams);
        break;

    case eHT_CHANNEL_WIDTH_80MHZ:
        sapSortChlWeightHT80(pSpectInfoParams);
        break;

    case eHT_CHANNEL_WIDTH_20MHZ:
    default:
        /* Sorting the channels as per weights as 20MHz channels */
        sapSortChlWeight(pSpectInfoParams);
    }

#else
    /* Sorting the channels as per weights */
    for (i = 0; i < SPECT_24GHZ_CH_COUNT; i++) {
        minWeightIndex = i;
        for( j = i + 1; j < SPECT_24GHZ_CH_COUNT; j++) {
            if(pSpectCh[j].weight < pSpectCh[minWeightIndex].weight) {
                minWeightIndex = j;
            }
        }
        if(minWeightIndex != i) {
            vos_mem_copy(&temp, &pSpectCh[minWeightIndex], sizeof(*pSpectCh));
            vos_mem_copy(&pSpectCh[minWeightIndex], &pSpectCh[i],
                                                    sizeof(*pSpectCh));
            vos_mem_copy(&pSpectCh[i], &temp, sizeof(*pSpectCh));
        }
    }
#endif

    /* For testing */
    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                    "In %s, Sorted Spectrum Channels Weight", __func__);
    pSpectCh = pSpectInfoParams->pSpectCh;
    for (j = 0; j < (pSpectInfoParams->numSpectChans); j++) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                    "In %s, Channel=%d Weight= %d rssi=%d bssCount=%d",
                    __func__, pSpectCh->chNum, pSpectCh->weight,
                    pSpectCh->rssiAgr, pSpectCh->bssCount);
        pSpectCh++;
    }

}

/*==========================================================================
  FUNCTION    sapFilterOverLapCh

  DESCRIPTION
    return true if ch is acceptable.
    This function will decide if we will filter over lap channel or not.

  DEPENDENCIES
    shall called after ap start.

  PARAMETERS

  IN
    pSapCtx          : Pointer to ptSapContext.
    chNum             : Filter channel number.

  RETURN VALUE
    v_BOOL_t          : true if channel is accepted.

  SIDE EFFECTS
============================================================================*/
v_BOOL_t sapFilterOverLapCh(ptSapContext pSapCtx, v_U16_t chNum)
{
    if (pSapCtx->enableOverLapCh)
        return eSAP_TRUE;
    else if((chNum == CHANNEL_1) ||
            (chNum == CHANNEL_6) ||
            (chNum == CHANNEL_11))
        return eSAP_TRUE;

    return eSAP_FALSE;
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
v_U8_t sapSelectChannel(tHalHandle halHandle, ptSapContext pSapCtx,  tScanResultHandle pScanResult)
{
    // DFS param object holding all the data req by the algo
    tSapChSelSpectInfo oSpectInfoParams = {NULL,0};
    tSapChSelSpectInfo *pSpectInfoParams = &oSpectInfoParams; // Memory? NB
    v_U8_t bestChNum = SAP_CHANNEL_NOT_SELECTED;
#ifdef FEATURE_WLAN_CH_AVOID
    v_U8_t i;
    v_U8_t firstSafeChannelInRange = SAP_CHANNEL_NOT_SELECTED;
    v_U32_t dfs_master_cap_enabled;
#endif
#ifdef SOFTAP_CHANNEL_RANGE
    v_U32_t startChannelNum;
    v_U32_t endChannelNum;
    v_U32_t operatingBand = 0;
    v_U32_t tmpChNum;
    v_U8_t  count;
#endif

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Running SAP Ch Select", __func__);

#ifdef FEATURE_WLAN_CH_AVOID
    sapUpdateUnsafeChannelList(pSapCtx);
#endif

    if (NULL == pScanResult)
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                  "%s: No external AP present\n", __func__);

#ifndef SOFTAP_CHANNEL_RANGE
        return bestChNum;
#else
        //scan is successfull, but no AP is present
        startChannelNum = pSapCtx->acs_cfg->start_ch;
        endChannelNum = pSapCtx->acs_cfg->end_ch;

        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                  "%s: start - end: %d - %d\n", __func__,
                  startChannelNum, endChannelNum);

#ifndef FEATURE_WLAN_CH_AVOID /* FEATURE_WLAN_CH_AVOID NOT defined case*/
        // pick the first channel in configured range
	pSapCtx->acs_cfg->pri_ch = startChannelNum;
	pSapCtx->acs_cfg->ht_sec_ch = 0;
        return startChannelNum;
#else /* FEATURE_WLAN_CH_AVOID defined */

        ccmCfgGetInt(halHandle, WNI_CFG_DFS_MASTER_ENABLED,
                &dfs_master_cap_enabled);

        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                  "%s: dfs_master %x", __func__, dfs_master_cap_enabled);
        // any safe channels in the configured range?
        for (i = 0; i < NUM_20MHZ_RF_CHANNELS; i++)
        {
            if((safeChannels[i].channelNumber >= startChannelNum) &&
               (safeChannels[i].channelNumber <= endChannelNum))
            {
                eNVChannelEnabledType enable_type =
                   vos_nv_getChannelEnabledState(safeChannels[i].channelNumber);
                if ((NV_CHANNEL_DISABLE == enable_type) ||
                    (NV_CHANNEL_INVALID == enable_type))
                    continue;

                if ((!dfs_master_cap_enabled) &&
                        (NV_CHANNEL_DFS == enable_type))
                {
                    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                            "%s: DFS master mode disabled. Skip DFS channel %d",
                            __func__, safeChannels[i].channelNumber);
                    continue;
                }

                if (safeChannels[i].isSafe == VOS_TRUE)
                {
                    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                        "%s: channel %d in the configuration is safe\n",
                        __func__, safeChannels[i].channelNumber);
                    firstSafeChannelInRange = safeChannels[i].channelNumber;
                    break;
                }

                VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
                    "%s: channel %d in the configuration is unsafe\n", __func__,
                    safeChannels[i].channelNumber);
            }
        }

        /* if there is no channel selected return SAP_CHANNEL_NOT_SELECTED */
        return firstSafeChannelInRange;
#endif /* !FEATURE_WLAN_CH_AVOID */
#endif /* SOFTAP_CHANNEL_RANGE */
    }

    // Initialize the structure pointed by pSpectInfoParams
    if (sapChanSelInit( halHandle, pSpectInfoParams, pSapCtx ) != eSAP_TRUE ) {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                  "In %s, Ch Select initialization failed", __func__);
        return SAP_CHANNEL_NOT_SELECTED;
    }

    // Compute the weight of the entire spectrum in the operating band
    sapComputeSpectWeight( pSpectInfoParams, halHandle, pScanResult, pSapCtx);

#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
    /* process avoid channel IE to collect all channels to avoid */
    sap_process_avoid_ie(halHandle, pSapCtx, pScanResult, pSpectInfoParams);
#endif /* FEATURE_AP_MCC_CH_AVOIDANCE */

#ifdef SOFTAP_CHANNEL_RANGE
    if (eCSR_BAND_ALL == pSapCtx->scanBandPreference)
    {
        startChannelNum = pSapCtx->acs_cfg->start_ch;
        endChannelNum = pSapCtx->acs_cfg->end_ch;
        SET_ACS_BAND(operatingBand, pSapCtx);
    }
    else
    {
        if (eCSR_BAND_24 == pSapCtx->currentPreferredBand)
        {
            startChannelNum = rfChannels[RF_CHAN_1].channelNum;
            endChannelNum = rfChannels[RF_CHAN_14].channelNum;
            operatingBand = eCSR_DOT11_MODE_11g;
        }
        else
        {
            startChannelNum = rfChannels[RF_CHAN_36].channelNum;
            endChannelNum = rfChannels[RF_CHAN_165].channelNum;
            operatingBand = eCSR_DOT11_MODE_11a;
        }
    }

    pSapCtx->acsBestChannelInfo.channelNum = 0;
    pSapCtx->acsBestChannelInfo.weight = CFG_ACS_BAND_SWITCH_THRESHOLD_MAX;

    /* Sort the channel list as per the computed weights, lesser weight first.*/
    sapSortChlWeightAll(pSapCtx, pSpectInfoParams, operatingBand);

    /*Loop till get the best channel in the given range */
    for (count=0; count < pSpectInfoParams->numSpectChans ; count++)
    {
        if ((startChannelNum <= pSpectInfoParams->pSpectCh[count].chNum)&&
            (endChannelNum >= pSpectInfoParams->pSpectCh[count].chNum))
        {
            if (bestChNum == SAP_CHANNEL_NOT_SELECTED)
            {
                bestChNum = pSpectInfoParams->pSpectCh[count].chNum;
                /* check if bestChNum is in preferred channel list */
                bestChNum = sapSelectPreferredChannelFromChannelList(
                                       bestChNum, pSapCtx, pSpectInfoParams);
                if (bestChNum == SAP_CHANNEL_NOT_SELECTED)
                {
                    /* not in preferred channel list, go to next best channel*/
                    continue;
                }

                if (pSpectInfoParams->pSpectCh[count].weight_copy >
                                  pSapCtx->acsBandSwitchThreshold)
                {
                    /* the best channel exceeds the threshold
                       check if need to scan next band */
                    if ((eCSR_BAND_ALL !=  pSapCtx->scanBandPreference) &&
                                   !pSapCtx->allBandScanned)
                    {
                        /* store best channel for later comparison */
                        pSapCtx->acsBestChannelInfo.channelNum = bestChNum;
                        pSapCtx->acsBestChannelInfo.weight =
                                   pSpectInfoParams->pSpectCh[count].weight;
                        bestChNum = SAP_CHANNEL_NOT_SELECTED;
                        break;
                    }
                    else
                    {
                        /* all bands are scanned, compare current best channel
                           with channel scanned previously */
                        if (( pSpectInfoParams->pSpectCh[count].weight_copy >
                              pSapCtx->acsBestChannelInfo.weight)
#ifdef FEATURE_AP_MCC_CH_AVOIDANCE
                             /* Weight of the channels(MDM device's AP is
                              operating) increased to MAX+1 so that they will
                              be choosen only when there is no other best channel
                              to choose*/
                              && !sap_check_in_avoid_ch_list(pSapCtx, bestChNum)
#endif
                        )
                        {
                            /* previous stored channel is better */
                            bestChNum = pSapCtx->acsBestChannelInfo.channelNum;
                        }
                        else
                        {
                            pSapCtx->acsBestChannelInfo.channelNum = bestChNum;
                            pSapCtx->acsBestChannelInfo.weight =
                                  pSpectInfoParams->pSpectCh[count].weight_copy;
                        }
                    }
                }
                else
                {
                    pSapCtx->acsBestChannelInfo.channelNum = bestChNum;
                    pSapCtx->acsBestChannelInfo.weight =
                                  pSpectInfoParams->pSpectCh[count].weight_copy;
                }
            }

            if (bestChNum != SAP_CHANNEL_NOT_SELECTED)
            {
                if (operatingBand == eCSR_DOT11_MODE_11g)
                {
                    /* Give preference to Non-overlap channels */
                    if (sapFilterOverLapCh(pSapCtx,
                            pSpectInfoParams->pSpectCh[count].chNum) &&
                        (pSpectInfoParams->pSpectCh[count].weight_copy <=
                                 pSapCtx->acsBestChannelInfo.weight))
                    {
                        tmpChNum = pSpectInfoParams->pSpectCh[count].chNum;
                        tmpChNum = sapSelectPreferredChannelFromChannelList(
                                       tmpChNum, pSapCtx, pSpectInfoParams);
                        if ( tmpChNum != SAP_CHANNEL_NOT_SELECTED)
                        {
                            bestChNum = tmpChNum;
                            break;
                        }
                    }
                }
            }
        }
    }
#else
    // Sort the channel list as per the computed weights, lesser weight first.
    sapSortChlWeightAll(pSapCtx, halHandle, pSpectInfoParams);
    // Get the first channel in sorted array as best 20M Channel
    bestChNum = (v_U8_t)pSpectInfoParams->pSpectCh[0].chNum;
    //Select Best Channel from Channel List if Configured
    bestChNum = sapSelectPreferredChannelFromChannelList(bestChNum,
                                                  pSapCtx, pSpectInfoParams);
#endif

    pSapCtx->acs_cfg->pri_ch = bestChNum;
    /* determine secondary channel for 2.4G channel 5, 6, 7 in HT40 */
    if ((operatingBand == eCSR_DOT11_MODE_11g) && (pSapCtx->acs_cfg->ch_width ==
                                             eHT_CHANNEL_WIDTH_40MHZ)) {

            if ((bestChNum >= 5) && (bestChNum <= 7)) {
                int weight_below, weight_above, i;
                tSapSpectChInfo *pSpectInfo;

               weight_below = weight_above  = ACS_WEIGHT_MAX;
                pSpectInfo = pSpectInfoParams->pSpectCh;

                for (i = 0; i < pSpectInfoParams->numSpectChans ; i++) {
                   if (pSpectInfo[i].chNum == (bestChNum - 4))
                        weight_below = pSpectInfo[i].weight;

               if (pSpectInfo[i].chNum == (bestChNum + 4))
                    weight_above = pSpectInfo[i].weight;
            }

            if (weight_below < weight_above)
               pSapCtx->acs_cfg->ht_sec_ch = pSapCtx->acs_cfg->pri_ch - 4;
            else
                pSapCtx->acs_cfg->ht_sec_ch = pSapCtx->acs_cfg->pri_ch + 4;
        } else {
            if (bestChNum >= 1 && bestChNum <= 4)
                pSapCtx->acs_cfg->ht_sec_ch = pSapCtx->acs_cfg->pri_ch + 4;
           else if (bestChNum >= 8 && bestChNum <= 13)
                pSapCtx->acs_cfg->ht_sec_ch = pSapCtx->acs_cfg->pri_ch - 4;
            else if (bestChNum ==14)
                pSapCtx->acs_cfg->ht_sec_ch = 0;
        }
        pSapCtx->secondary_ch = pSapCtx->acs_cfg->ht_sec_ch;
    }

    // Free all the allocated memory
    sapChanSelExit(pSpectInfoParams);

    VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH, "In %s, Running SAP Ch select Completed, Ch=%d",
                        __func__, bestChNum);
    if (bestChNum > 0 && bestChNum <= 252)
        return bestChNum;
    else
        return SAP_CHANNEL_NOT_SELECTED;
}
