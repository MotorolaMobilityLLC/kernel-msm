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

/**=========================================================================

  \file  macTrace.c

  \brief implementation for trace related APIs

  \author Sunit Bhatia

   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.

   Qualcomm Confidential and Proprietary.

  ========================================================================*/


/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/

#include "macTrace.h"


#ifdef TRACE_RECORD
static tTraceRecord gTraceTbl[MAX_TRACE_RECORDS];
static tTraceData gTraceData;
static tpTraceCb traceCBTable[VOS_MODULE_ID_MAX];





tANI_U8* macTraceGetSmeMsgString( tANI_U16 smeMsg )
{
    switch( smeMsg )
    {
        CASE_RETURN_STRING(eWNI_SME_START_REQ);
        CASE_RETURN_STRING(eWNI_SME_START_RSP);
        CASE_RETURN_STRING(eWNI_SME_SYS_READY_IND);
        CASE_RETURN_STRING(eWNI_SME_SCAN_REQ);
        CASE_RETURN_STRING(eWNI_SME_SCAN_ABORT_IND);
        CASE_RETURN_STRING(eWNI_SME_SCAN_RSP);
        CASE_RETURN_STRING(eWNI_SME_JOIN_REQ);
        CASE_RETURN_STRING(eWNI_SME_JOIN_RSP);
        CASE_RETURN_STRING(eWNI_SME_SETCONTEXT_REQ);
        CASE_RETURN_STRING(eWNI_SME_SETCONTEXT_RSP);
        CASE_RETURN_STRING(eWNI_SME_REASSOC_REQ);
        CASE_RETURN_STRING(eWNI_SME_REASSOC_RSP);
        CASE_RETURN_STRING(eWNI_SME_AUTH_REQ);
        CASE_RETURN_STRING(eWNI_SME_AUTH_RSP);
        CASE_RETURN_STRING(eWNI_SME_DISASSOC_REQ);
        CASE_RETURN_STRING(eWNI_SME_DISASSOC_RSP);
        CASE_RETURN_STRING(eWNI_SME_DISASSOC_IND);
        CASE_RETURN_STRING(eWNI_SME_DISASSOC_CNF);
        CASE_RETURN_STRING(eWNI_SME_DEAUTH_REQ);
        CASE_RETURN_STRING(eWNI_SME_DEAUTH_RSP);
        CASE_RETURN_STRING(eWNI_SME_DEAUTH_IND);
        CASE_RETURN_STRING(eWNI_SME_WM_STATUS_CHANGE_NTF);
        CASE_RETURN_STRING(eWNI_SME_IBSS_NEW_PEER_IND);
        CASE_RETURN_STRING(eWNI_SME_IBSS_PEER_DEPARTED_IND);
        CASE_RETURN_STRING(eWNI_SME_START_BSS_REQ);
        CASE_RETURN_STRING(eWNI_SME_START_BSS_RSP);
        CASE_RETURN_STRING(eWNI_SME_AUTH_IND);
        CASE_RETURN_STRING(eWNI_SME_ASSOC_IND);
        CASE_RETURN_STRING(eWNI_SME_ASSOC_CNF);
        CASE_RETURN_STRING(eWNI_SME_REASSOC_IND);
        CASE_RETURN_STRING(eWNI_SME_REASSOC_CNF);
        CASE_RETURN_STRING(eWNI_SME_SWITCH_CHL_REQ);
        CASE_RETURN_STRING(eWNI_SME_SWITCH_CHL_RSP);
        CASE_RETURN_STRING(eWNI_SME_STOP_BSS_REQ);
        CASE_RETURN_STRING(eWNI_SME_STOP_BSS_RSP);
        CASE_RETURN_STRING(eWNI_SME_DEL_BA_PEER_IND);
        CASE_RETURN_STRING(eWNI_SME_PROMISCUOUS_MODE_REQ);
        CASE_RETURN_STRING(eWNI_SME_PROMISCUOUS_MODE_RSP);
        CASE_RETURN_STRING(eWNI_SME_NEIGHBOR_BSS_IND);
        CASE_RETURN_STRING(eWNI_SME_MEASUREMENT_REQ);
        CASE_RETURN_STRING(eWNI_SME_MEASUREMENT_RSP);
        CASE_RETURN_STRING(eWNI_SME_MEASUREMENT_IND);
        CASE_RETURN_STRING(eWNI_SME_SET_WDS_INFO_REQ);
        CASE_RETURN_STRING(eWNI_SME_SET_WDS_INFO_RSP);
        CASE_RETURN_STRING(eWNI_SME_WDS_INFO_IND);
        CASE_RETURN_STRING(eWNI_SME_DEAUTH_CNF);
        CASE_RETURN_STRING(eWNI_SME_MIC_FAILURE_IND);
        CASE_RETURN_STRING(eWNI_SME_ADDTS_REQ);
        CASE_RETURN_STRING(eWNI_SME_ADDTS_RSP);
        CASE_RETURN_STRING(eWNI_SME_ADDTS_CNF);
        CASE_RETURN_STRING(eWNI_SME_ADDTS_IND);
        CASE_RETURN_STRING(eWNI_SME_DELTS_REQ);
        CASE_RETURN_STRING(eWNI_SME_DELTS_RSP);
        CASE_RETURN_STRING(eWNI_SME_DELTS_IND);
        CASE_RETURN_STRING(eWNI_SME_SET_BACKGROUND_SCAN_MODE_REQ);
        CASE_RETURN_STRING(eWNI_SME_SWITCH_CHL_CB_PRIMARY_REQ);
        CASE_RETURN_STRING(eWNI_SME_SWITCH_CHL_CB_PRIMARY_RSP);
        CASE_RETURN_STRING(eWNI_SME_SWITCH_CHL_CB_SECONDARY_REQ);
        CASE_RETURN_STRING(eWNI_SME_SWITCH_CHL_CB_SECONDARY_RSP);
        CASE_RETURN_STRING(eWNI_SME_PROBE_REQ);
        CASE_RETURN_STRING(eWNI_SME_STA_STAT_REQ);
        CASE_RETURN_STRING(eWNI_SME_STA_STAT_RSP);
        CASE_RETURN_STRING(eWNI_SME_AGGR_STAT_REQ);
        CASE_RETURN_STRING(eWNI_SME_AGGR_STAT_RSP);
        CASE_RETURN_STRING(eWNI_SME_GLOBAL_STAT_REQ);
        CASE_RETURN_STRING(eWNI_SME_GLOBAL_STAT_RSP);
        CASE_RETURN_STRING(eWNI_SME_STAT_SUMM_REQ);
        CASE_RETURN_STRING(eWNI_SME_STAT_SUMM_RSP);
        CASE_RETURN_STRING(eWNI_SME_REMOVEKEY_REQ);
        CASE_RETURN_STRING(eWNI_SME_REMOVEKEY_RSP);
        CASE_RETURN_STRING(eWNI_SME_GET_SCANNED_CHANNEL_REQ);
        CASE_RETURN_STRING(eWNI_SME_GET_SCANNED_CHANNEL_RSP);
        CASE_RETURN_STRING(eWNI_SME_SET_TX_POWER_REQ);
        CASE_RETURN_STRING(eWNI_SME_SET_TX_POWER_RSP);
        CASE_RETURN_STRING(eWNI_SME_GET_TX_POWER_REQ);
        CASE_RETURN_STRING(eWNI_SME_GET_TX_POWER_RSP);
        CASE_RETURN_STRING(eWNI_SME_GET_NOISE_REQ);
        CASE_RETURN_STRING(eWNI_SME_GET_NOISE_RSP);
        CASE_RETURN_STRING(eWNI_SME_LOW_RSSI_IND);
        CASE_RETURN_STRING(eWNI_SME_GET_STATISTICS_REQ);
        CASE_RETURN_STRING(eWNI_SME_GET_STATISTICS_RSP);        

        CASE_RETURN_STRING(eWNI_SME_MSG_TYPES_END);

        //General Power Save Messages
        CASE_RETURN_STRING(eWNI_PMC_PWR_SAVE_CFG);

        //BMPS Messages
        CASE_RETURN_STRING(eWNI_PMC_ENTER_BMPS_REQ);
        CASE_RETURN_STRING(eWNI_PMC_ENTER_BMPS_RSP);
        CASE_RETURN_STRING(eWNI_PMC_EXIT_BMPS_REQ);
        CASE_RETURN_STRING(eWNI_PMC_EXIT_BMPS_RSP);
        CASE_RETURN_STRING(eWNI_PMC_EXIT_BMPS_IND);

        //IMPS Messages.
        CASE_RETURN_STRING(eWNI_PMC_ENTER_IMPS_REQ);
        CASE_RETURN_STRING(eWNI_PMC_ENTER_IMPS_RSP);
        CASE_RETURN_STRING(eWNI_PMC_EXIT_IMPS_REQ);
        CASE_RETURN_STRING(eWNI_PMC_EXIT_IMPS_RSP);

        //UAPSD Messages
        CASE_RETURN_STRING(eWNI_PMC_ENTER_UAPSD_REQ);
        CASE_RETURN_STRING(eWNI_PMC_ENTER_UAPSD_RSP);
        CASE_RETURN_STRING(eWNI_PMC_EXIT_UAPSD_REQ);
        CASE_RETURN_STRING(eWNI_PMC_EXIT_UAPSD_RSP);

        CASE_RETURN_STRING(eWNI_PMC_SMPS_STATE_IND);

        default:
            return( (tANI_U8*)"UNKNOWN" );
            break;
    }
}


tANI_U8* macTraceGetHalMsgString( tANI_U16 halMsg )
{
    switch( halMsg )
    {

        CASE_RETURN_STRING(SIR_HAL_RADAR_DETECTED_IND);
        CASE_RETURN_STRING(SIR_HAL_WDT_KAM_RSP                );
        CASE_RETURN_STRING(SIR_HAL_TIMER_TEMP_MEAS_REQ        );
        CASE_RETURN_STRING(SIR_HAL_TIMER_PERIODIC_STATS_COLLECT_REQ   );
        CASE_RETURN_STRING(SIR_HAL_CAL_REQ_NTF                );
        CASE_RETURN_STRING(SIR_HAL_MNT_OPEN_TPC_TEMP_MEAS_REQ );
        CASE_RETURN_STRING(SIR_HAL_CCA_MONITOR_INTERVAL_TO    );
        CASE_RETURN_STRING(SIR_HAL_CCA_MONITOR_DURATION_TO    );
        CASE_RETURN_STRING(SIR_HAL_CCA_MONITOR_START          );
        CASE_RETURN_STRING(SIR_HAL_CCA_MONITOR_STOP           );
        CASE_RETURN_STRING(SIR_HAL_CCA_CHANGE_MODE            );

        CASE_RETURN_STRING(SIR_HAL_ADD_STA_REQ                );
        CASE_RETURN_STRING(SIR_HAL_ADD_STA_RSP                );
        CASE_RETURN_STRING(SIR_HAL_DELETE_STA_REQ             );
        CASE_RETURN_STRING(SIR_HAL_DELETE_STA_RSP             );
        CASE_RETURN_STRING(SIR_HAL_ADD_BSS_REQ                );
        CASE_RETURN_STRING(SIR_HAL_ADD_BSS_RSP                );
        CASE_RETURN_STRING(SIR_HAL_DELETE_BSS_REQ             );
        CASE_RETURN_STRING(SIR_HAL_DELETE_BSS_RSP             );
        CASE_RETURN_STRING(SIR_HAL_INIT_SCAN_REQ              );
        CASE_RETURN_STRING(SIR_HAL_INIT_SCAN_RSP              );
        CASE_RETURN_STRING(SIR_HAL_START_SCAN_REQ             );
        CASE_RETURN_STRING(SIR_HAL_START_SCAN_RSP             );
        CASE_RETURN_STRING(SIR_HAL_END_SCAN_REQ               );
        CASE_RETURN_STRING(SIR_HAL_END_SCAN_RSP               );
        CASE_RETURN_STRING(SIR_HAL_FINISH_SCAN_REQ            );
        CASE_RETURN_STRING(SIR_HAL_FINISH_SCAN_RSP            );
        CASE_RETURN_STRING(SIR_HAL_SEND_BEACON_REQ            );
        CASE_RETURN_STRING(SIR_HAL_SEND_BEACON_RSP            );

        CASE_RETURN_STRING(SIR_HAL_INIT_CFG_REQ               );
        CASE_RETURN_STRING(SIR_HAL_INIT_CFG_RSP               );

        CASE_RETURN_STRING(SIR_HAL_INIT_WM_CFG_REQ            );
        CASE_RETURN_STRING(SIR_HAL_INIT_WM_CFG_RSP            );

        CASE_RETURN_STRING(SIR_HAL_SET_BSSKEY_REQ             );
        CASE_RETURN_STRING(SIR_HAL_SET_BSSKEY_RSP             );
        CASE_RETURN_STRING(SIR_HAL_SET_STAKEY_REQ             );
        CASE_RETURN_STRING(SIR_HAL_SET_STAKEY_RSP             );
        CASE_RETURN_STRING(SIR_HAL_DPU_STATS_REQ              );
        CASE_RETURN_STRING(SIR_HAL_DPU_STATS_RSP              );
        CASE_RETURN_STRING(SIR_HAL_GET_DPUINFO_REQ            );
        CASE_RETURN_STRING(SIR_HAL_GET_DPUINFO_RSP            );

        CASE_RETURN_STRING(SIR_HAL_UPDATE_EDCA_PROFILE_IND    );

        CASE_RETURN_STRING(SIR_HAL_UPDATE_STARATEINFO_REQ     );
        CASE_RETURN_STRING(SIR_HAL_UPDATE_STARATEINFO_RSP     );

        CASE_RETURN_STRING(SIR_HAL_UPDATE_BEACON_IND          );
        CASE_RETURN_STRING(SIR_HAL_UPDATE_CF_IND              );
        CASE_RETURN_STRING(SIR_HAL_CHNL_SWITCH_REQ            );
        CASE_RETURN_STRING(SIR_HAL_SWITCH_CHANNEL_RSP         );
        CASE_RETURN_STRING(SIR_HAL_ADD_TS_REQ                 );
        CASE_RETURN_STRING(SIR_HAL_DEL_TS_REQ                 );
        CASE_RETURN_STRING(SIR_HAL_ADD_TS_RSP);
        CASE_RETURN_STRING(SIR_HAL_SOFTMAC_TXSTAT_REPORT      );
        CASE_RETURN_STRING(SIR_HAL_EXIT_BMPS_REQ              );
        CASE_RETURN_STRING(SIR_HAL_EXIT_BMPS_RSP              );
        CASE_RETURN_STRING(SIR_HAL_EXIT_BMPS_IND              );
        CASE_RETURN_STRING(SIR_HAL_ENTER_BMPS_REQ             );
        CASE_RETURN_STRING(SIR_HAL_ENTER_BMPS_RSP             );
        CASE_RETURN_STRING(SIR_HAL_BMPS_STATUS_IND            );
        CASE_RETURN_STRING(SIR_HAL_MISSED_BEACON_IND          );

        CASE_RETURN_STRING(SIR_HAL_PWR_SAVE_CFG               );

        CASE_RETURN_STRING(SIR_HAL_REGISTER_PE_CALLBACK       );
        CASE_RETURN_STRING(SIR_HAL_SOFTMAC_MEM_READREQUEST    );
        CASE_RETURN_STRING(SIR_HAL_SOFTMAC_MEM_WRITEREQUEST   );

        CASE_RETURN_STRING(SIR_HAL_SOFTMAC_MEM_READRESPONSE   );
        CASE_RETURN_STRING(SIR_HAL_SOFTMAC_BULKREGWRITE_CONFIRM      );
        CASE_RETURN_STRING(SIR_HAL_SOFTMAC_BULKREGREAD_RESPONSE      );
        CASE_RETURN_STRING(SIR_HAL_SOFTMAC_HOSTMESG_MSGPROCESSRESULT );

        CASE_RETURN_STRING(SIR_HAL_ADDBA_REQ                  );
        CASE_RETURN_STRING(SIR_HAL_ADDBA_RSP                  );
        CASE_RETURN_STRING(SIR_HAL_DELBA_IND                  );

        CASE_RETURN_STRING(SIR_HAL_DELBA_REQ                  );
        CASE_RETURN_STRING(SIR_HAL_IBSS_STA_ADD               );
        CASE_RETURN_STRING(SIR_HAL_TIMER_ADJUST_ADAPTIVE_THRESHOLD_IND   );
        CASE_RETURN_STRING(SIR_HAL_SET_LINK_STATE             );
        CASE_RETURN_STRING(SIR_HAL_ENTER_IMPS_REQ             );
        CASE_RETURN_STRING(SIR_HAL_ENTER_IMPS_RSP             );
        CASE_RETURN_STRING(SIR_HAL_EXIT_IMPS_RSP              );
        CASE_RETURN_STRING(SIR_HAL_EXIT_IMPS_REQ              );
        CASE_RETURN_STRING(SIR_HAL_SOFTMAC_HOSTMESG_PS_STATUS_IND  );
        CASE_RETURN_STRING(SIR_HAL_POSTPONE_ENTER_IMPS_RSP    );
        CASE_RETURN_STRING(SIR_HAL_STA_STAT_REQ               );
        CASE_RETURN_STRING(SIR_HAL_GLOBAL_STAT_REQ            );
        CASE_RETURN_STRING(SIR_HAL_AGGR_STAT_REQ              );
        CASE_RETURN_STRING(SIR_HAL_STA_STAT_RSP               );
        CASE_RETURN_STRING(SIR_HAL_GLOBAL_STAT_RSP            );
        CASE_RETURN_STRING(SIR_HAL_AGGR_STAT_RSP              );
        CASE_RETURN_STRING(SIR_HAL_STAT_SUMM_REQ              );
        CASE_RETURN_STRING(SIR_HAL_STAT_SUMM_RSP              );
        CASE_RETURN_STRING(SIR_HAL_REMOVE_BSSKEY_REQ          );
        CASE_RETURN_STRING(SIR_HAL_REMOVE_BSSKEY_RSP          );
        CASE_RETURN_STRING(SIR_HAL_REMOVE_STAKEY_REQ          );
        CASE_RETURN_STRING(SIR_HAL_REMOVE_STAKEY_RSP          );
        CASE_RETURN_STRING(SIR_HAL_SET_STA_BCASTKEY_REQ       );
        CASE_RETURN_STRING(SIR_HAL_SET_STA_BCASTKEY_RSP       );
        CASE_RETURN_STRING(SIR_HAL_REMOVE_STA_BCASTKEY_REQ    );
        CASE_RETURN_STRING(SIR_HAL_REMOVE_STA_BCASTKEY_RSP    );

        CASE_RETURN_STRING(SIR_HAL_DPU_MIC_ERROR              );

        CASE_RETURN_STRING(SIR_HAL_TIMER_BA_ACTIVITY_REQ      );
        CASE_RETURN_STRING(SIR_HAL_TIMER_CHIP_MONITOR_TIMEOUT );
        CASE_RETURN_STRING(SIR_HAL_TIMER_TRAFFIC_ACTIVITY_REQ );
        CASE_RETURN_STRING(SIR_HAL_SET_MIMOPS_REQ                      );
        CASE_RETURN_STRING(SIR_HAL_SET_MIMOPS_RSP                      );
        CASE_RETURN_STRING(SIR_HAL_SYS_READY_IND                       );
        CASE_RETURN_STRING(SIR_HAL_SET_TX_POWER_REQ                    );
        CASE_RETURN_STRING(SIR_HAL_SET_TX_POWER_RSP                    );
        CASE_RETURN_STRING(SIR_HAL_GET_TX_POWER_REQ                    );
        CASE_RETURN_STRING(SIR_HAL_GET_TX_POWER_RSP                    );
        CASE_RETURN_STRING(SIR_HAL_GET_NOISE_REQ                       );
        CASE_RETURN_STRING(SIR_HAL_GET_NOISE_RSP                       );

        CASE_RETURN_STRING(SIR_HAL_TRANSMISSION_CONTROL_IND            );
        CASE_RETURN_STRING(SIR_HAL_INIT_RADAR_IND                      );

        CASE_RETURN_STRING(SIR_HAL_BEACON_PRE_IND             );
        CASE_RETURN_STRING(SIR_HAL_ENTER_UAPSD_REQ            );
        CASE_RETURN_STRING(SIR_HAL_ENTER_UAPSD_RSP            );
        CASE_RETURN_STRING(SIR_HAL_EXIT_UAPSD_REQ             );
        CASE_RETURN_STRING(SIR_HAL_EXIT_UAPSD_RSP             );
        CASE_RETURN_STRING(SIR_HAL_LOW_RSSI_IND               );
        CASE_RETURN_STRING(SIR_HAL_GET_STATISTICS_RSP         );

#ifdef SUPPORT_BEACON_FILTER
        CASE_RETURN_STRING(SIR_HAL_BEACON_FILTER_IND   );
#endif //SUPPORT_BEACON_FILTER
        default:
            return((tANI_U8*) "UNKNOWN" );
            break;
    }
}



tANI_U8* macTraceGetLimMsgString( tANI_U16 limMsg )
{
    switch( limMsg )
    {
         CASE_RETURN_STRING(SIR_LIM_RESUME_ACTIVITY_NTF);
        CASE_RETURN_STRING(SIR_LIM_SUSPEND_ACTIVITY_REQ );
        CASE_RETURN_STRING(SIR_HAL_SUSPEND_ACTIVITY_RSP );
        CASE_RETURN_STRING(SIR_LIM_RETRY_INTERRUPT_MSG);
        CASE_RETURN_STRING(SIR_BB_XPORT_MGMT_MSG );
        CASE_RETURN_STRING(SIR_LIM_INV_KEY_INTERRUPT_MSG );
        CASE_RETURN_STRING(SIR_LIM_KEY_ID_INTERRUPT_MSG );
        CASE_RETURN_STRING(SIR_LIM_REPLAY_THRES_INTERRUPT_MSG );
        CASE_RETURN_STRING(SIR_LIM_TD_DUMMY_CALLBACK_MSG );
        CASE_RETURN_STRING(SIR_LIM_SCH_CLEAN_MSG  );
        CASE_RETURN_STRING(SIR_LIM_RADAR_DETECT_IND);
        CASE_RETURN_STRING(SIR_LIM_DEL_TS_IND);
        CASE_RETURN_STRING(SIR_LIM_ADD_BA_IND );
        CASE_RETURN_STRING(SIR_LIM_DEL_BA_ALL_IND);
        CASE_RETURN_STRING(SIR_LIM_DELETE_STA_CONTEXT_IND);
        CASE_RETURN_STRING(SIR_LIM_DEL_BA_IND );
        CASE_RETURN_STRING(SIR_LIM_MIN_CHANNEL_TIMEOUT);
        CASE_RETURN_STRING(SIR_LIM_MAX_CHANNEL_TIMEOUT);
        CASE_RETURN_STRING(SIR_LIM_JOIN_FAIL_TIMEOUT );
        CASE_RETURN_STRING(SIR_LIM_AUTH_FAIL_TIMEOUT );
        CASE_RETURN_STRING(SIR_LIM_AUTH_RSP_TIMEOUT);
        CASE_RETURN_STRING(SIR_LIM_ASSOC_FAIL_TIMEOUT);
        CASE_RETURN_STRING(SIR_LIM_REASSOC_FAIL_TIMEOUT);
        CASE_RETURN_STRING(SIR_LIM_HEART_BEAT_TIMEOUT);
#if (WNI_POLARIS_FW_PRODUCT == AP)
        CASE_RETURN_STRING(SIR_LIM_PREAUTH_CLNUP_TIMEOUT);
#endif
        CASE_RETURN_STRING(SIR_LIM_CHANNEL_SCAN_TIMEOUT );
        CASE_RETURN_STRING(SIR_LIM_PROBE_HB_FAILURE_TIMEOUT);
        CASE_RETURN_STRING(SIR_LIM_ADDTS_RSP_TIMEOUT );
#if (WNI_POLARIS_FW_PRODUCT == AP) && (WNI_POLARIS_FW_PACKAGE == ADVANCED)
        CASE_RETURN_STRING(SIR_LIM_MEASUREMENT_IND_TIMEOUT  );
        CASE_RETURN_STRING(SIR_LIM_LEARN_INTERVAL_TIMEOUT   );
        CASE_RETURN_STRING(SIR_LIM_LEARN_DURATION_TIMEOUT   );
#endif
        CASE_RETURN_STRING(SIR_LIM_LINK_TEST_DURATION_TIMEOUT );
        CASE_RETURN_STRING(SIR_LIM_HASH_MISS_THRES_TIMEOUT  );
        CASE_RETURN_STRING(SIR_LIM_CNF_WAIT_TIMEOUT         );
        CASE_RETURN_STRING(SIR_LIM_KEEPALIVE_TIMEOUT        );
        CASE_RETURN_STRING(SIR_LIM_UPDATE_OLBC_CACHEL_TIMEOUT );
        CASE_RETURN_STRING(SIR_LIM_CHANNEL_SWITCH_TIMEOUT   );
        CASE_RETURN_STRING(SIR_LIM_QUIET_TIMEOUT            );
        CASE_RETURN_STRING(SIR_LIM_QUIET_BSS_TIMEOUT      );
#ifdef WMM_APSD
        CASE_RETURN_STRING(SIR_LIM_WMM_APSD_SP_START_MSG_TYPE );
        CASE_RETURN_STRING(SIR_LIM_WMM_APSD_SP_END_MSG_TYPE );
#endif
        CASE_RETURN_STRING(SIR_LIM_BEACON_GEN_IND );
        CASE_RETURN_STRING(SIR_LIM_MSG_TYPES_END);

        default:
            return( (tANI_U8*)"UNKNOWN" );
            break;
    }
}




tANI_U8* macTraceGetCfgMsgString( tANI_U16 cfgMsg )
{
    switch( cfgMsg )
    {
        CASE_RETURN_STRING(WNI_CFG_PARAM_UPDATE_IND);
        CASE_RETURN_STRING(WNI_CFG_DNLD_REQ);
        CASE_RETURN_STRING(WNI_CFG_DNLD_CNF);
        CASE_RETURN_STRING(WNI_CFG_GET_RSP);
        CASE_RETURN_STRING(WNI_CFG_SET_CNF);
        CASE_RETURN_STRING(SIR_CFG_PARAM_UPDATE_IND);
        CASE_RETURN_STRING(SIR_CFG_DOWNLOAD_COMPLETE_IND);

        CASE_RETURN_STRING(WNI_CFG_SET_REQ_NO_RSP);
        default:
            return( (tANI_U8*)"UNKNOWN" );
            break;
    }
}


tANI_U8* macTraceGetModuleString( tANI_U8 moduleId  )
{
    return ((tANI_U8*)"PE");
    //return gVosTraceInfo[moduleId].moduleNameStr;
}










void macTraceInit(tpAniSirGlobal pMac)
{
    tANI_U8 i;
    gTraceData.head = INVALID_TRACE_ADDR;
    gTraceData.tail = INVALID_TRACE_ADDR;
    gTraceData.num = 0;
    gTraceData.enable = TRUE;
    gTraceData.dumpCount = DEFAULT_TRACE_DUMP_COUNT;
    gTraceData.numSinceLastDump = 0;

    for(i=0; i<VOS_MODULE_ID_MAX; i++)
        traceCBTable[i] = NULL;

}





void macTraceReset(tpAniSirGlobal pMac)
{
}


void macTrace(tpAniSirGlobal pMac,  tANI_U8 code, tANI_U8 session, tANI_U32 data)
{
    //Today macTrace is being invoked by PE only, need to remove this function once PE is migrated to using new trace API.
    macTraceNew(pMac, VOS_MODULE_ID_PE, code, session, data);

#if 0
    tpTraceRecord rec = NULL;

    //limLog(pMac, LOGE, "mac Trace code: %d, data: %x, head: %d, tail: %d\n",  code, data, gTraceData.head, gTraceData.tail);

    if(!gTraceData.enable)
        return;
    gTraceData.num++;

    if (gTraceData.head == INVALID_TRACE_ADDR)
    {
        /* first record */
        gTraceData.head = 0;
        gTraceData.tail = 0;
    }
    else
    {
        /* queue is not empty */
        tANI_U32 tail = gTraceData.tail + 1;

        if (tail == MAX_TRACE_RECORDS)
            tail = 0;

        if (gTraceData.head == tail)
        {
            /* full */
            if (++gTraceData.head == MAX_TRACE_RECORDS)
                gTraceData.head = 0;
        }

        gTraceData.tail = tail;
    }

    rec = &gTraceTbl[gTraceData.tail];
    rec->code = code;
    rec->session = session;
    rec->data = data;
    rec->time = vos_timer_get_system_time();
    rec->module =  VOS_MODULE_ID_PE;
    gTraceData.numSinceLastDump ++;

    if(gTraceData.numSinceLastDump == gTraceData.dumpCount)
        {
            limLog(pMac, LOGE, "Trace Dump last %d traces\n",  gTraceData.dumpCount);
            macTraceDumpAll(pMac, 0, 0, gTraceData.dumpCount);
            gTraceData.numSinceLastDump = 0;
        }
    #endif

}



void macTraceNew(tpAniSirGlobal pMac, tANI_U8 module, tANI_U8 code, tANI_U8 session, tANI_U32 data)
{
    tpTraceRecord rec = NULL;

    //limLog(pMac, LOGE, "mac Trace code: %d, data: %x, head: %d, tail: %d\n",  code, data, gTraceData.head, gTraceData.tail);

    if(!gTraceData.enable)
        return;
    //If module is not registered, don't record for that module.
    if(traceCBTable[module] == NULL)
        return;

    
    gTraceData.num++;

    if (gTraceData.head == INVALID_TRACE_ADDR)
    {
        /* first record */
        gTraceData.head = 0;
        gTraceData.tail = 0;
    }
    else
    {
        /* queue is not empty */
        tANI_U32 tail = gTraceData.tail + 1;

        if (tail == MAX_TRACE_RECORDS)
            tail = 0;

        if (gTraceData.head == tail)
        {
            /* full */
            if (++gTraceData.head == MAX_TRACE_RECORDS)
                gTraceData.head = 0;
        }

        gTraceData.tail = tail;
    }

    rec = &gTraceTbl[gTraceData.tail];
    rec->code = code;
    rec->session = session;
    rec->data = data;
    rec->time = (tANI_U16)vos_timer_get_system_time();
    rec->module =  module;
    gTraceData.numSinceLastDump ++;

    if(gTraceData.numSinceLastDump == gTraceData.dumpCount)
        {
             VOS_TRACE( VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR, "Trace Dump last %d traces\n",  gTraceData.dumpCount);
              macTraceDumpAll(pMac, 0, 0, gTraceData.dumpCount);
              gTraceData.numSinceLastDump = 0;
        }

}







tANI_U8* macTraceMsgString(tpAniSirGlobal pMac, tANI_U32 msgType)
{
    tANI_U16 msgId = (tANI_U16)MAC_TRACE_GET_MSG_ID(msgType);
    tANI_U8 moduleId = (tANI_U8)MAC_TRACE_GET_MODULE_ID(msgType);

    switch(moduleId)
    {
        case SIR_LIM_MODULE_ID:
            if(msgId >= SIR_LIM_ITC_MSG_TYPES_BEGIN)
                return macTraceGetLimMsgString((tANI_U16)msgType);
            else
                return macTraceGetSmeMsgString((tANI_U16)msgType);
            break;
        case SIR_HAL_MODULE_ID:
                return macTraceGetHalMsgString((tANI_U16)msgType);
        case SIR_CFG_MODULE_ID:
                return macTraceGetCfgMsgString((tANI_U16)msgType);
        default:
                return ((tANI_U8*)"Unknown MsgType");
    }
}






void macTraceDumpAll(tpAniSirGlobal pMac, tANI_U8 code, tANI_U8 session, tANI_U32 count)
{
    tpTraceRecord pRecord;
    tANI_S32 i, tail;


    if(!gTraceData.enable)
    {
        VOS_TRACE( VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR, "Tracing Disabled \n");
        return;
    }

    VOS_TRACE( VOS_MODULE_ID_SYS, VOS_TRACE_LEVEL_ERROR, 
                            "Total Records: %d, Head: %d, Tail: %d\n", gTraceData.num, gTraceData.head, gTraceData.tail);

    if (gTraceData.head != INVALID_TRACE_ADDR)
    {

        i = gTraceData.head;
        tail = gTraceData.tail;

        if (count)
        {
            if (count > gTraceData.num)
                count = gTraceData.num;
            if (count > MAX_TRACE_RECORDS)
                count = MAX_TRACE_RECORDS;
            if(tail >= (count + 1))
            {
                i = tail - count + 1;
            }
            else
            {
                i = MAX_TRACE_RECORDS - ((count + 1) - tail);
            }
        }

        pRecord = &gTraceTbl[i];

        for (;;)
        {
            if (   (code == 0 || (code == pRecord->code)) &&
                    (traceCBTable[pRecord->module] != NULL))
                traceCBTable[pRecord->module](pMac, pRecord, (tANI_U16)i);

            if (i == tail)
                break;
            i += 1;

            if (i == MAX_TRACE_RECORDS)
            {
                i = 0;
                pRecord = &gTraceTbl[0];
            }
            else
                pRecord += 1;
        }
        gTraceData.numSinceLastDump = 0;
 
    }

}


void macTraceCfg(tpAniSirGlobal pMac, tANI_U32 enable, tANI_U32 dumpCount, tANI_U32 code, tANI_U32 session)
{
    gTraceData.enable = (tANI_U8)enable;
    gTraceData.dumpCount= (tANI_U16)dumpCount;
    gTraceData.numSinceLastDump = 0;
}

void macTraceRegister( tpAniSirGlobal pMac, VOS_MODULE_ID moduleId,    tpTraceCb traceCb)
{
    traceCBTable[moduleId] = traceCb;
}


#endif
