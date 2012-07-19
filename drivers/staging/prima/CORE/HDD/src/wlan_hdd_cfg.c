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

                       EDIT HISTORY FOR FILE 
   
   
  This section contains comments describing changes made to the module. 
  Notice that changes are listed in reverse chronological order. 
   
   
  $Header:$   $DateTime: $ $Author: $ 
   
   
  when        who    what, where, why 
  --------    ---    --------------------------------------------------------
  07/27/09    kanand Created module. 

  ==========================================================================*/

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/


#include <linux/firmware.h>
#include <linux/string.h>
#include <wlan_hdd_includes.h>
#include <wlan_hdd_main.h>
#include <wlan_hdd_assoc.h>
#include <wlan_hdd_cfg.h>
#include <linux/string.h>
#include <vos_types.h>
#include <csrApi.h>
#include <pmcApi.h>
#include <wlan_hdd_misc.h>


REG_TABLE_ENTRY g_registry_table[] =
{
   REG_VARIABLE( CFG_RTS_THRESHOLD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, RTSThreshold, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_RTS_THRESHOLD_DEFAULT, 
                 CFG_RTS_THRESHOLD_MIN, 
                 CFG_RTS_THRESHOLD_MAX ), 

   REG_VARIABLE( CFG_FRAG_THRESHOLD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, FragmentationThreshold, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_FRAG_THRESHOLD_DEFAULT, 
                 CFG_FRAG_THRESHOLD_MIN, 
                 CFG_FRAG_THRESHOLD_MAX ),              

   REG_VARIABLE( CFG_CALIBRATION_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Calibration, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_CALIBRATION_DEFAULT, 
                 CFG_CALIBRATION_MIN, 
                 CFG_CALIBRATION_MAX ),
                
   REG_VARIABLE( CFG_CALIBRATION_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, CalibrationPeriod, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_CALIBRATION_PERIOD_DEFAULT, 
                 CFG_CALIBRATION_PERIOD_MIN, 
                 CFG_CALIBRATION_PERIOD_MAX ), 

   REG_VARIABLE( CFG_OPERATING_CHANNEL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, OperatingChannel,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_OPERATING_CHANNEL_DEFAULT, 
                 CFG_OPERATING_CHANNEL_MIN, 
                 CFG_OPERATING_CHANNEL_MAX ),

   REG_VARIABLE( CFG_SHORT_SLOT_TIME_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, ShortSlotTimeEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_SHORT_SLOT_TIME_ENABLED_DEFAULT, 
                 CFG_SHORT_SLOT_TIME_ENABLED_MIN, 
                 CFG_SHORT_SLOT_TIME_ENABLED_MAX ),

   REG_VARIABLE( CFG_11D_SUPPORT_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Is11dSupportEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_11D_SUPPORT_ENABLED_DEFAULT, 
                 CFG_11D_SUPPORT_ENABLED_MIN, 
                 CFG_11D_SUPPORT_ENABLED_MAX ),

   REG_VARIABLE( CFG_ENFORCE_11D_CHANNELS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fEnforce11dChannels, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_ENFORCE_11D_CHANNELS_DEFAULT, 
                 CFG_ENFORCE_11D_CHANNELS_MIN, 
                 CFG_ENFORCE_11D_CHANNELS_MAX ),

   REG_VARIABLE( CFG_COUNTRY_CODE_PRIORITY_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fSupplicantCountryCodeHasPriority, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_COUNTRY_CODE_PRIORITY_DEFAULT, 
                 CFG_COUNTRY_CODE_PRIORITY_MIN, 
                 CFG_COUNTRY_CODE_PRIORITY_MAX),

   REG_VARIABLE( CFG_ENFORCE_COUNTRY_CODE_MATCH_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fEnforceCountryCodeMatch, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_ENFORCE_COUNTRY_CODE_MATCH_DEFAULT, 
                 CFG_ENFORCE_COUNTRY_CODE_MATCH_MIN, 
                 CFG_ENFORCE_COUNTRY_CODE_MATCH_MAX ),

   REG_VARIABLE( CFG_ENFORCE_DEFAULT_DOMAIN_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fEnforceDefaultDomain, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_ENFORCE_DEFAULT_DOMAIN_DEFAULT, 
                 CFG_ENFORCE_DEFAULT_DOMAIN_MIN, 
                 CFG_ENFORCE_DEFAULT_DOMAIN_MAX ),
                
   REG_VARIABLE( CFG_GENERIC_ID1_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg1Id,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_GENERIC_ID1_DEFAULT, 
                 CFG_GENERIC_ID1_MIN, 
                 CFG_GENERIC_ID1_MAX ),
                
   REG_VARIABLE( CFG_GENERIC_ID2_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg2Id,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_GENERIC_ID2_DEFAULT, 
                 CFG_GENERIC_ID2_MIN, 
                 CFG_GENERIC_ID2_MAX ),
                
   REG_VARIABLE( CFG_GENERIC_ID3_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg3Id,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_GENERIC_ID3_DEFAULT, 
                 CFG_GENERIC_ID3_MIN, 
                 CFG_GENERIC_ID3_MAX ),
                
   REG_VARIABLE( CFG_GENERIC_ID4_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg4Id,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_GENERIC_ID4_DEFAULT, 
                 CFG_GENERIC_ID4_MIN, 
                 CFG_GENERIC_ID4_MAX ),
                
   REG_VARIABLE( CFG_GENERIC_ID5_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg5Id,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_GENERIC_ID5_DEFAULT, 
                 CFG_GENERIC_ID5_MIN, 
                 CFG_GENERIC_ID5_MAX ),

   REG_VARIABLE( CFG_GENERIC_VALUE1_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg1Value,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_GENERIC_VALUE1_DEFAULT, 
                 CFG_GENERIC_VALUE1_MIN, 
                 CFG_GENERIC_VALUE1_MAX ),
                
   REG_VARIABLE( CFG_GENERIC_VALUE2_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg2Value,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_GENERIC_VALUE2_DEFAULT, 
                 CFG_GENERIC_VALUE2_MIN, 
                 CFG_GENERIC_VALUE2_MAX ),
                
   REG_VARIABLE( CFG_GENERIC_VALUE3_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg3Value,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_GENERIC_VALUE3_DEFAULT, 
                 CFG_GENERIC_VALUE3_MIN, 
                 CFG_GENERIC_VALUE3_MAX ),
                
   REG_VARIABLE( CFG_GENERIC_VALUE4_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg4Value,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_GENERIC_VALUE4_DEFAULT, 
                 CFG_GENERIC_VALUE4_MIN, 
                 CFG_GENERIC_VALUE4_MAX ),
                
   REG_VARIABLE( CFG_GENERIC_VALUE5_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, Cfg5Value,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_GENERIC_VALUE5_DEFAULT, 
                 CFG_GENERIC_VALUE5_MIN, 
                 CFG_GENERIC_VALUE5_MAX ),

   REG_VARIABLE( CFG_HEARTBEAT_THRESH_24_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, HeartbeatThresh24, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_HEARTBEAT_THRESH_24_DEFAULT, 
                 CFG_HEARTBEAT_THRESH_24_MIN, 
                 CFG_HEARTBEAT_THRESH_24_MAX ),
                
   REG_VARIABLE_STRING( CFG_POWER_USAGE_NAME, WLAN_PARAM_String,
                        hdd_config_t, PowerUsageControl, 
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_POWER_USAGE_DEFAULT ),

   REG_VARIABLE( CFG_ENABLE_SUSPEND_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nEnableSuspend, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ENABLE_SUSPEND_DEFAULT, 
                 CFG_ENABLE_SUSPEND_MIN, 
                 CFG_ENABLE_SUSPEND_MAX ),

   REG_VARIABLE( CFG_ENABLE_ENABLE_DRIVER_STOP_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nEnableDriverStop, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ENABLE_ENABLE_DRIVER_STOP_DEFAULT, 
                 CFG_ENABLE_ENABLE_DRIVER_STOP_MIN, 
                 CFG_ENABLE_ENABLE_DRIVER_STOP_MAX ),

   REG_VARIABLE( CFG_ENABLE_IMPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsImpsEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ENABLE_IMPS_DEFAULT, 
                 CFG_ENABLE_IMPS_MIN, 
                 CFG_ENABLE_IMPS_MAX ),

   REG_VARIABLE( CFG_ENABLE_LOGP_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsLogpEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ENABLE_LOGP_DEFAULT, 
                 CFG_ENABLE_LOGP_MIN, 
                 CFG_ENABLE_LOGP_MAX ),

   REG_VARIABLE( CFG_IMPS_MINIMUM_SLEEP_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nImpsMinSleepTime, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_IMPS_MINIMUM_SLEEP_TIME_DEFAULT, 
                 CFG_IMPS_MINIMUM_SLEEP_TIME_MIN, 
                 CFG_IMPS_MINIMUM_SLEEP_TIME_MAX ),

   REG_VARIABLE( CFG_IMPS_MAXIMUM_SLEEP_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nImpsMaxSleepTime, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_IMPS_MAXIMUM_SLEEP_TIME_DEFAULT, 
                 CFG_IMPS_MAXIMUM_SLEEP_TIME_MIN, 
                 CFG_IMPS_MAXIMUM_SLEEP_TIME_MAX ),

   REG_VARIABLE( CFG_IMPS_MODERATE_SLEEP_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nImpsModSleepTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_IMPS_MODERATE_SLEEP_TIME_DEFAULT, 
                 CFG_IMPS_MODERATE_SLEEP_TIME_MIN, 
                 CFG_IMPS_MODERATE_SLEEP_TIME_MAX ),

   REG_VARIABLE( CFG_ENABLE_BMPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsBmpsEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ENABLE_BMPS_DEFAULT, 
                 CFG_ENABLE_BMPS_MIN, 
                 CFG_ENABLE_BMPS_MAX ),

   REG_VARIABLE( CFG_BMPS_MINIMUM_LI_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nBmpsMinListenInterval, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_BMPS_MINIMUM_LI_DEFAULT, 
                 CFG_BMPS_MINIMUM_LI_MIN, 
                 CFG_BMPS_MINIMUM_LI_MAX ),

   REG_VARIABLE( CFG_BMPS_MAXIMUM_LI_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nBmpsMaxListenInterval, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_BMPS_MAXIMUM_LI_DEFAULT, 
                 CFG_BMPS_MAXIMUM_LI_MIN, 
                 CFG_BMPS_MAXIMUM_LI_MAX ),

   REG_VARIABLE( CFG_BMPS_MODERATE_LI_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nBmpsModListenInterval,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_BMPS_MODERATE_LI_DEFAULT, 
                 CFG_BMPS_MODERATE_LI_MIN, 
                 CFG_BMPS_MODERATE_LI_MAX ),

   REG_VARIABLE( CFG_ENABLE_AUTO_BMPS_TIMER_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsAutoBmpsTimerEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ENABLE_AUTO_BMPS_TIMER_DEFAULT, 
                 CFG_ENABLE_AUTO_BMPS_TIMER_MIN, 
                 CFG_ENABLE_AUTO_BMPS_TIMER_MAX ),

   REG_VARIABLE( CFG_AUTO_BMPS_TIMER_VALUE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nAutoBmpsTimerValue, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_AUTO_BMPS_TIMER_VALUE_DEFAULT, 
                 CFG_AUTO_BMPS_TIMER_VALUE_MIN, 
                 CFG_AUTO_BMPS_TIMER_VALUE_MAX ),

   REG_VARIABLE( CFG_DOT11_MODE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, dot11Mode, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_DOT11_MODE_DEFAULT, 
                 CFG_DOT11_MODE_MIN, 
                 CFG_DOT11_MODE_MAX ),
                 
   REG_VARIABLE( CFG_CHANNEL_BONDING_MODE_24GHZ_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nChannelBondingMode24GHz, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_CHANNEL_BONDING_MODE_DEFAULT, 
                 CFG_CHANNEL_BONDING_MODE_MIN, 
                 CFG_CHANNEL_BONDING_MODE_MAX),
              
   REG_VARIABLE( CFG_CHANNEL_BONDING_MODE_5GHZ_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nChannelBondingMode5GHz, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_CHANNEL_BONDING_MODE_DEFAULT, 
                 CFG_CHANNEL_BONDING_MODE_MIN, 
                 CFG_CHANNEL_BONDING_MODE_MAX),
              
   REG_VARIABLE( CFG_MAX_RX_AMPDU_FACTOR_NAME, WLAN_PARAM_Integer,   
                 hdd_config_t, MaxRxAmpduFactor, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK , 
                 CFG_MAX_RX_AMPDU_FACTOR_DEFAULT, 
                 CFG_MAX_RX_AMPDU_FACTOR_MIN, 
                 CFG_MAX_RX_AMPDU_FACTOR_MAX),
                
   REG_VARIABLE( CFG_FIXED_RATE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, TxRate, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_FIXED_RATE_DEFAULT, 
                 CFG_FIXED_RATE_MIN, 
                 CFG_FIXED_RATE_MAX ),

   REG_VARIABLE( CFG_SHORT_GI_20MHZ_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, ShortGI20MhzEnable, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_SHORT_GI_20MHZ_DEFAULT, 
                 CFG_SHORT_GI_20MHZ_MIN, 
                 CFG_SHORT_GI_20MHZ_MAX ),

   REG_VARIABLE( CFG_BLOCK_ACK_AUTO_SETUP_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, BlockAckAutoSetup, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_BLOCK_ACK_AUTO_SETUP_DEFAULT, 
                 CFG_BLOCK_ACK_AUTO_SETUP_MIN, 
                 CFG_BLOCK_ACK_AUTO_SETUP_MAX ),
  
   REG_VARIABLE( CFG_SCAN_RESULT_AGE_COUNT_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, ScanResultAgeCount, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_SCAN_RESULT_AGE_COUNT_DEFAULT, 
                 CFG_SCAN_RESULT_AGE_COUNT_MIN, 
                 CFG_SCAN_RESULT_AGE_COUNT_MAX ),

   REG_VARIABLE( CFG_SCAN_RESULT_AGE_TIME_NCNPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nScanAgeTimeNCNPS, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_SCAN_RESULT_AGE_TIME_NCNPS_DEFAULT, 
                 CFG_SCAN_RESULT_AGE_TIME_NCNPS_MIN, 
                 CFG_SCAN_RESULT_AGE_TIME_NCNPS_MAX ),

   REG_VARIABLE( CFG_SCAN_RESULT_AGE_TIME_NCPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nScanAgeTimeNCPS, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_SCAN_RESULT_AGE_TIME_NCPS_DEFAULT, 
                 CFG_SCAN_RESULT_AGE_TIME_NCPS_MIN, 
                 CFG_SCAN_RESULT_AGE_TIME_NCPS_MAX ),

   REG_VARIABLE( CFG_SCAN_RESULT_AGE_TIME_CNPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nScanAgeTimeCNPS, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_SCAN_RESULT_AGE_TIME_CNPS_DEFAULT, 
                 CFG_SCAN_RESULT_AGE_TIME_CNPS_MIN, 
                 CFG_SCAN_RESULT_AGE_TIME_CNPS_MAX ),

   REG_VARIABLE( CFG_SCAN_RESULT_AGE_TIME_CPS_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nScanAgeTimeCPS, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_SCAN_RESULT_AGE_TIME_CPS_DEFAULT, 
                 CFG_SCAN_RESULT_AGE_TIME_CPS_MIN, 
                 CFG_SCAN_RESULT_AGE_TIME_CPS_MAX ),

   REG_VARIABLE( CFG_RSSI_CATEGORY_GAP_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nRssiCatGap, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_RSSI_CATEGORY_GAP_DEFAULT, 
                 CFG_RSSI_CATEGORY_GAP_MIN, 
                 CFG_RSSI_CATEGORY_GAP_MAX ),  

   REG_VARIABLE( CFG_STAT_TIMER_INTERVAL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nStatTimerInterval, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_STAT_TIMER_INTERVAL_DEFAULT, 
                 CFG_STAT_TIMER_INTERVAL_MIN, 
                 CFG_STAT_TIMER_INTERVAL_MAX ),

   REG_VARIABLE( CFG_SHORT_PREAMBLE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsShortPreamble, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_SHORT_PREAMBLE_DEFAULT, 
                 CFG_SHORT_PREAMBLE_MIN, 
                 CFG_SHORT_PREAMBLE_MAX ),

   REG_VARIABLE( CFG_IBSS_AUTO_BSSID_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsAutoIbssBssid, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_IBSS_AUTO_BSSID_DEFAULT, 
                 CFG_IBSS_AUTO_BSSID_MIN, 
                 CFG_IBSS_AUTO_BSSID_MAX ),

   REG_VARIABLE_STRING( CFG_IBSS_BSSID_NAME, WLAN_PARAM_MacAddr,
                        hdd_config_t, IbssBssid, 
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_IBSS_BSSID_DEFAULT ),

   REG_VARIABLE_STRING( CFG_INTF0_MAC_ADDR_NAME, WLAN_PARAM_MacAddr,
                        hdd_config_t, intfMacAddr[0], 
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_INTF0_MAC_ADDR_DEFAULT ),

   REG_VARIABLE_STRING( CFG_INTF1_MAC_ADDR_NAME, WLAN_PARAM_MacAddr,
                        hdd_config_t, intfMacAddr[1], 
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_INTF1_MAC_ADDR_DEFAULT ),

   REG_VARIABLE_STRING( CFG_INTF2_MAC_ADDR_NAME, WLAN_PARAM_MacAddr,
                        hdd_config_t, intfMacAddr[2], 
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_INTF2_MAC_ADDR_DEFAULT ),

   REG_VARIABLE_STRING( CFG_INTF3_MAC_ADDR_NAME, WLAN_PARAM_MacAddr,
                        hdd_config_t, intfMacAddr[3], 
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_INTF3_MAC_ADDR_DEFAULT ),

#ifdef WLAN_SOFTAP_FEATURE
   REG_VARIABLE( CFG_AP_QOS_UAPSD_MODE_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, apUapsdEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_AP_QOS_UAPSD_MODE_DEFAULT, 
                 CFG_AP_QOS_UAPSD_MODE_MIN, 
                 CFG_AP_QOS_UAPSD_MODE_MAX ),

   REG_VARIABLE_STRING( CFG_AP_COUNTRY_CODE, WLAN_PARAM_String,
                        hdd_config_t, apCntryCode, 
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_AP_COUNTRY_CODE_DEFAULT ),

   REG_VARIABLE( CFG_AP_ENABLE_PROTECTION_MODE_NAME, WLAN_PARAM_Integer,
                        hdd_config_t, apProtEnabled, 
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_AP_ENABLE_PROTECTION_MODE_DEFAULT,
                        CFG_AP_ENABLE_PROTECTION_MODE_MIN,
                        CFG_AP_ENABLE_PROTECTION_MODE_MAX ),

   REG_VARIABLE( CFG_AP_PROTECTION_MODE_NAME, WLAN_PARAM_HexInteger,
                        hdd_config_t, apProtection,
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_AP_PROTECTION_MODE_DEFAULT,
                        CFG_AP_PROTECTION_MODE_MIN,
                        CFG_AP_PROTECTION_MODE_MAX ),
                        
   REG_VARIABLE( CFG_AP_OBSS_PROTECTION_MODE_NAME, WLAN_PARAM_Integer,
                        hdd_config_t, apOBSSProtEnabled, 
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_AP_OBSS_PROTECTION_MODE_DEFAULT,
                        CFG_AP_OBSS_PROTECTION_MODE_MIN,
                        CFG_AP_OBSS_PROTECTION_MODE_MAX ),
                        
   REG_VARIABLE( CFG_AP_STA_SECURITY_SEPERATION_NAME, WLAN_PARAM_Integer,
                        hdd_config_t, apDisableIntraBssFwd, 
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_AP_STA_SECURITY_SEPERATION_DEFAULT,
                        CFG_AP_STA_SECURITY_SEPERATION_MIN,
                        CFG_AP_STA_SECURITY_SEPERATION_MAX ),

   REG_VARIABLE( CFG_FRAMES_PROCESSING_TH_MODE_NAME, WLAN_PARAM_Integer,
                        hdd_config_t, MinFramesProcThres, 
                        VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                        CFG_FRAMES_PROCESSING_TH_DEFAULT,
                        CFG_FRAMES_PROCESSING_TH_MIN,
                        CFG_FRAMES_PROCESSING_TH_MAX ),

   REG_VARIABLE(CFG_SAP_CHANNEL_SELECT_START_CHANNEL , WLAN_PARAM_Integer,
                 hdd_config_t, apStartChannelNum,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_START_CHANNEL_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_START_CHANNEL_MIN,
                 CFG_SAP_CHANNEL_SELECT_START_CHANNEL_MAX ),

   REG_VARIABLE(CFG_SAP_CHANNEL_SELECT_END_CHANNEL , WLAN_PARAM_Integer,
                 hdd_config_t, apEndChannelNum,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_END_CHANNEL_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_END_CHANNEL_MIN,
                 CFG_SAP_CHANNEL_SELECT_END_CHANNEL_MAX ),

   REG_VARIABLE(CFG_SAP_CHANNEL_SELECT_OPERATING_BAND , WLAN_PARAM_Integer,
                 hdd_config_t, apOperatingBand,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_OPERATING_BAND_DEFAULT,
                 CFG_SAP_CHANNEL_SELECT_OPERATING_BAND_MIN,
                 CFG_SAP_CHANNEL_SELECT_OPERATING_BAND_MAX ),

   REG_VARIABLE(CFG_SAP_AUTO_CHANNEL_SELECTION_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, apAutoChannelSelection,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_SAP_AUTO_CHANNEL_SELECTION_DEFAULT,
                 CFG_SAP_AUTO_CHANNEL_SELECTION_MIN,
                 CFG_SAP_AUTO_CHANNEL_SELECTION_MAX ),
   
   REG_VARIABLE(CFG_ENABLE_LTE_COEX , WLAN_PARAM_Integer,
                 hdd_config_t, enableLTECoex,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_LTE_COEX_DEFAULT,
                 CFG_ENABLE_LTE_COEX_MIN,
                 CFG_ENABLE_LTE_COEX_MAX ),

   REG_VARIABLE( CFG_AP_KEEP_ALIVE_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, apKeepAlivePeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_AP_KEEP_ALIVE_PERIOD_DEFAULT,
                 CFG_AP_KEEP_ALIVE_PERIOD_MIN,
                 CFG_AP_KEEP_ALIVE_PERIOD_MAX),

   REG_VARIABLE( CFG_GO_KEEP_ALIVE_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, goKeepAlivePeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_GO_KEEP_ALIVE_PERIOD_DEFAULT,
                 CFG_GO_KEEP_ALIVE_PERIOD_MIN,
                 CFG_GO_KEEP_ALIVE_PERIOD_MAX),

#endif
   REG_VARIABLE(CFG_DISABLE_PACKET_FILTER , WLAN_PARAM_Integer,
                 hdd_config_t, disablePacketFilter,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_DISABLE_PACKET_FILTER_DEFAULT,
                 CFG_DISABLE_PACKET_FILTER_MIN,
                 CFG_DISABLE_PACKET_FILTER_MAX ),

   REG_VARIABLE( CFG_BEACON_INTERVAL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nBeaconInterval, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK, 
                 CFG_BEACON_INTERVAL_DEFAULT, 
                 CFG_BEACON_INTERVAL_MIN, 
                 CFG_BEACON_INTERVAL_MAX ),

   REG_VARIABLE( CFG_ENABLE_HANDOFF_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsHandoffEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ENABLE_HANDOFF_DEFAULT, 
                 CFG_ENABLE_HANDOFF_MIN, 
                 CFG_ENABLE_HANDOFF_MAX ),


   REG_VARIABLE( CFG_ENABLE_IDLE_SCAN_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, nEnableIdleScan, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ENABLE_IDLE_SCAN_DEFAULT, 
                 CFG_ENABLE_IDLE_SCAN_MIN, 
                 CFG_ENABLE_IDLE_SCAN_MAX ),

   REG_VARIABLE( CFG_ROAMING_TIME_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, nRoamingTime, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ROAMING_TIME_DEFAULT, 
                 CFG_ROAMING_TIME_MIN, 
                 CFG_ROAMING_TIME_MAX ),

   REG_VARIABLE( CFG_VCC_RSSI_TRIGGER_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, nVccRssiTrigger, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_VCC_RSSI_TRIGGER_DEFAULT, 
                 CFG_VCC_RSSI_TRIGGER_MIN, 
                 CFG_VCC_RSSI_TRIGGER_MAX ),

   REG_VARIABLE( CFG_VCC_UL_MAC_LOSS_THRESH_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, nVccUlMacLossThreshold, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_VCC_UL_MAC_LOSS_THRESH_DEFAULT, 
                 CFG_VCC_UL_MAC_LOSS_THRESH_MIN, 
                 CFG_VCC_UL_MAC_LOSS_THRESH_MAX ),

   REG_VARIABLE( CFG_PASSIVE_MAX_CHANNEL_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nPassiveMaxChnTime, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_PASSIVE_MAX_CHANNEL_TIME_DEFAULT, 
                 CFG_PASSIVE_MAX_CHANNEL_TIME_MIN, 
                 CFG_PASSIVE_MAX_CHANNEL_TIME_MAX ),

   REG_VARIABLE( CFG_PASSIVE_MIN_CHANNEL_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nPassiveMinChnTime, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_PASSIVE_MIN_CHANNEL_TIME_DEFAULT, 
                 CFG_PASSIVE_MIN_CHANNEL_TIME_MIN, 
                 CFG_PASSIVE_MIN_CHANNEL_TIME_MAX ),

   REG_VARIABLE( CFG_ACTIVE_MAX_CHANNEL_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nActiveMaxChnTime, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ACTIVE_MAX_CHANNEL_TIME_DEFAULT, 
                 CFG_ACTIVE_MAX_CHANNEL_TIME_MIN, 
                 CFG_ACTIVE_MAX_CHANNEL_TIME_MAX ),

   REG_VARIABLE( CFG_ACTIVE_MIN_CHANNEL_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nActiveMinChnTime, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ACTIVE_MIN_CHANNEL_TIME_DEFAULT, 
                 CFG_ACTIVE_MIN_CHANNEL_TIME_MIN, 
                 CFG_ACTIVE_MIN_CHANNEL_TIME_MAX ),
   
   REG_VARIABLE( CFG_MAX_PS_POLL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nMaxPsPoll, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_MAX_PS_POLL_DEFAULT, 
                 CFG_MAX_PS_POLL_MIN, 
                 CFG_MAX_PS_POLL_MAX ),

    REG_VARIABLE( CFG_MAX_TX_POWER_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nTxPowerCap, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_MAX_TX_POWER_DEFAULT, 
                 CFG_MAX_TX_POWER_MIN, 
                 CFG_MAX_TX_POWER_MAX ),

   REG_VARIABLE( CFG_LOW_GAIN_OVERRIDE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIsLowGainOverride,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_LOW_GAIN_OVERRIDE_DEFAULT, 
                 CFG_LOW_GAIN_OVERRIDE_MIN, 
                 CFG_LOW_GAIN_OVERRIDE_MAX ),

   REG_VARIABLE( CFG_RSSI_FILTER_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nRssiFilterPeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_RSSI_FILTER_PERIOD_DEFAULT, 
                 CFG_RSSI_FILTER_PERIOD_MIN, 
                 CFG_RSSI_FILTER_PERIOD_MAX ),

   REG_VARIABLE( CFG_IGNORE_DTIM_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fIgnoreDtim,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_IGNORE_DTIM_DEFAULT, 
                 CFG_IGNORE_DTIM_MIN, 
                 CFG_IGNORE_DTIM_MAX ),
                 
   REG_VARIABLE( CFG_RX_ANT_CONFIGURATION_NAME, WLAN_PARAM_Integer,
                hdd_config_t, nRxAnt, 
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                CFG_RX_ANT_CONFIGURATION_NAME_DEFAULT, 
                CFG_RX_ANT_CONFIGURATION_NAME_MIN, 
                CFG_RX_ANT_CONFIGURATION_NAME_MAX ),

   REG_VARIABLE( CFG_FW_HEART_BEAT_MONITORING_NAME, WLAN_PARAM_Integer,
                hdd_config_t, fEnableFwHeartBeatMonitoring, 
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                CFG_FW_HEART_BEAT_MONITORING_DEFAULT, 
                CFG_FW_HEART_BEAT_MONITORING_MIN, 
                CFG_FW_HEART_BEAT_MONITORING_MAX ),

   REG_VARIABLE( CFG_FW_BEACON_FILTERING_NAME, WLAN_PARAM_Integer,
                hdd_config_t, fEnableFwBeaconFiltering, 
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                CFG_FW_BEACON_FILTERING_DEFAULT, 
                CFG_FW_BEACON_FILTERING_MIN, 
                CFG_FW_BEACON_FILTERING_MAX ),

   REG_VARIABLE( CFG_FW_RSSI_MONITORING_NAME, WLAN_PARAM_Integer,
                hdd_config_t, fEnableFwRssiMonitoring, 
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                CFG_FW_RSSI_MONITORING_DEFAULT, 
                CFG_FW_RSSI_MONITORING_MIN, 
                CFG_FW_RSSI_MONITORING_MAX ),

   REG_VARIABLE( CFG_DATA_INACTIVITY_TIMEOUT_NAME, WLAN_PARAM_Integer,
                hdd_config_t, nDataInactivityTimeout, 
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                CFG_DATA_INACTIVITY_TIMEOUT_DEFAULT, 
                CFG_DATA_INACTIVITY_TIMEOUT_MIN, 
                CFG_DATA_INACTIVITY_TIMEOUT_MAX ),
                
   REG_VARIABLE( CFG_NTH_BEACON_FILTER_NAME, WLAN_PARAM_Integer,
                hdd_config_t, nthBeaconFilter, 
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                CFG_NTH_BEACON_FILTER_DEFAULT, 
                CFG_NTH_BEACON_FILTER_MIN, 
                CFG_NTH_BEACON_FILTER_MAX ),              

   REG_VARIABLE( CFG_QOS_WMM_MODE_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, WmmMode, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_MODE_DEFAULT, 
                 CFG_QOS_WMM_MODE_MIN, 
                 CFG_QOS_WMM_MODE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_80211E_ENABLED_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, b80211eIsEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_80211E_ENABLED_DEFAULT, 
                 CFG_QOS_WMM_80211E_ENABLED_MIN, 
                 CFG_QOS_WMM_80211E_ENABLED_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_UAPSD_MASK_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, UapsdMask, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_UAPSD_MASK_DEFAULT, 
                 CFG_QOS_WMM_UAPSD_MASK_MIN, 
                 CFG_QOS_WMM_UAPSD_MASK_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_MAX_SP_LEN_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, MaxSpLength, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_MAX_SP_LEN_DEFAULT, 
                 CFG_QOS_WMM_MAX_SP_LEN_MIN, 
                 CFG_QOS_WMM_MAX_SP_LEN_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdVoSrvIntv, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_MIN, 
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SRV_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdVoSuspIntv, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_MIN, 
                 CFG_QOS_WMM_INFRA_UAPSD_VO_SUS_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdViSrvIntv, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_MIN, 
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SRV_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdViSuspIntv, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_MIN, 
                 CFG_QOS_WMM_INFRA_UAPSD_VI_SUS_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdBeSrvIntv, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_MIN, 
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SRV_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdBeSuspIntv, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_MIN, 
                 CFG_QOS_WMM_INFRA_UAPSD_BE_SUS_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdBkSrvIntv, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_MIN, 
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SRV_INTV_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraUapsdBkSuspIntv, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_DEFAULT, 
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_MIN, 
                 CFG_QOS_WMM_INFRA_UAPSD_BK_SUS_INTV_MAX ),

#ifdef FEATURE_WLAN_CCX
   REG_VARIABLE( CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, InfraInactivityInterval, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_DEFAULT, 
                 CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_MIN, 
                 CFG_QOS_WMM_INFRA_INACTIVITY_INTERVAL_MAX),
   REG_VARIABLE( CFG_CCX_FEATURE_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, isCcxIniFeatureEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_CCX_FEATURE_ENABLED_DEFAULT, 
                 CFG_CCX_FEATURE_ENABLED_MIN, 
                 CFG_CCX_FEATURE_ENABLED_MAX),
#endif // FEATURE_WLAN_CCX

#ifdef FEATURE_WLAN_LFR
   // flag to turn ON/OFF Legacy Fast Roaming
   REG_VARIABLE( CFG_LFR_FEATURE_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, isFastRoamIniFeatureEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_LFR_FEATURE_ENABLED_DEFAULT, 
                 CFG_LFR_FEATURE_ENABLED_MIN, 
                 CFG_LFR_FEATURE_ENABLED_MAX),
#endif // FEATURE_WLAN_LFR

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
   REG_VARIABLE( CFG_FT_RSSI_FILTER_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, FTRssiFilterPeriod,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_FT_RSSI_FILTER_PERIOD_DEFAULT, 
                 CFG_FT_RSSI_FILTER_PERIOD_MIN, 
                 CFG_FT_RSSI_FILTER_PERIOD_MAX ),


   // flag to turn ON/OFF 11r and CCX FastTransition
   REG_VARIABLE( CFG_FAST_TRANSITION_ENABLED_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, isFastTransitionEnabled,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_FAST_TRANSITION_ENABLED_NAME_DEFAULT, 
                 CFG_FAST_TRANSITION_ENABLED_NAME_MIN, 
                 CFG_FAST_TRANSITION_ENABLED_NAME_MAX),

   /* Variable to specify the delta/difference between the RSSI of current AP 
    * and roamable AP while roaming */
   REG_VARIABLE( CFG_ROAM_RSSI_DIFF_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, RoamRssiDiff, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ROAM_RSSI_DIFF_DEFAULT, 
                 CFG_ROAM_RSSI_DIFF_MIN, 
                 CFG_ROAM_RSSI_DIFF_MAX),
#endif

   REG_VARIABLE( CFG_QOS_WMM_PKT_CLASSIFY_BASIS_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, PktClassificationBasis, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_PKT_CLASSIFY_BASIS_DEFAULT, 
                 CFG_QOS_WMM_PKT_CLASSIFY_BASIS_MIN, 
                 CFG_QOS_WMM_PKT_CLASSIFY_BASIS_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_DIR_AC_VO_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraDirAcVo, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_DIR_AC_VO_DEFAULT, 
                 CFG_QOS_WMM_INFRA_DIR_AC_VO_MIN, 
                 CFG_QOS_WMM_INFRA_DIR_AC_VO_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraNomMsduSizeAcVo, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_DEFAULT, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_MIN, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VO_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMeanDataRateAcVo, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_MIN, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VO_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMinPhyRateAcVo, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_MIN, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VO_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_SBA_AC_VO_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraSbaAcVo, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_SBA_AC_VO_DEFAULT, 
                 CFG_QOS_WMM_INFRA_SBA_AC_VO_MIN, 
                 CFG_QOS_WMM_INFRA_SBA_AC_VO_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_DIR_AC_VI_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraDirAcVi, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_DIR_AC_VI_DEFAULT, 
                 CFG_QOS_WMM_INFRA_DIR_AC_VI_MIN, 
                 CFG_QOS_WMM_INFRA_DIR_AC_VI_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraNomMsduSizeAcVi, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_DEFAULT, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_MIN, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_VI_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMeanDataRateAcVi, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_MIN, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_VI_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMinPhyRateAcVi, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_MIN, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_VI_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_SBA_AC_VI_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraSbaAcVi, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_SBA_AC_VI_DEFAULT, 
                 CFG_QOS_WMM_INFRA_SBA_AC_VI_MIN, 
                 CFG_QOS_WMM_INFRA_SBA_AC_VI_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_DIR_AC_BE_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraDirAcBe, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_DIR_AC_BE_DEFAULT, 
                 CFG_QOS_WMM_INFRA_DIR_AC_BE_MIN, 
                 CFG_QOS_WMM_INFRA_DIR_AC_BE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraNomMsduSizeAcBe, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_DEFAULT, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_MIN, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMeanDataRateAcBe, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_MIN, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMinPhyRateAcBe, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_MIN, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_SBA_AC_BE_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraSbaAcBe, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_SBA_AC_BE_DEFAULT, 
                 CFG_QOS_WMM_INFRA_SBA_AC_BE_MIN, 
                 CFG_QOS_WMM_INFRA_SBA_AC_BE_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_DIR_AC_BK_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, InfraDirAcBk, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_DIR_AC_BK_DEFAULT, 
                 CFG_QOS_WMM_INFRA_DIR_AC_BK_MIN, 
                 CFG_QOS_WMM_INFRA_DIR_AC_BK_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraNomMsduSizeAcBk, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_DEFAULT, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_MIN, 
                 CFG_QOS_WMM_INFRA_NOM_MSDU_SIZE_AC_BK_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMeanDataRateAcBk, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_MIN, 
                 CFG_QOS_WMM_INFRA_MEAN_DATA_RATE_AC_BK_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraMinPhyRateAcBk, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_DEFAULT, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_MIN, 
                 CFG_QOS_WMM_INFRA_MIN_PHY_RATE_AC_BK_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_INFRA_SBA_AC_BK_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, InfraSbaAcBk, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_INFRA_SBA_AC_BK_DEFAULT, 
                 CFG_QOS_WMM_INFRA_SBA_AC_BK_MIN, 
                 CFG_QOS_WMM_INFRA_SBA_AC_BK_MAX ),

   REG_VARIABLE( CFG_TL_WFQ_BK_WEIGHT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, WfqBkWeight, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_TL_WFQ_BK_WEIGHT_DEFAULT, 
                 CFG_TL_WFQ_BK_WEIGHT_MIN, 
                 CFG_TL_WFQ_BK_WEIGHT_MAX ),

   REG_VARIABLE( CFG_TL_WFQ_BE_WEIGHT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, WfqBeWeight, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_TL_WFQ_BE_WEIGHT_DEFAULT, 
                 CFG_TL_WFQ_BE_WEIGHT_MIN, 
                 CFG_TL_WFQ_BE_WEIGHT_MAX ),

   REG_VARIABLE( CFG_TL_WFQ_VI_WEIGHT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, WfqViWeight, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_TL_WFQ_VI_WEIGHT_DEFAULT, 
                 CFG_TL_WFQ_VI_WEIGHT_MIN, 
                 CFG_TL_WFQ_VI_WEIGHT_MAX ),

   REG_VARIABLE( CFG_TL_WFQ_VO_WEIGHT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, WfqVoWeight, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_TL_WFQ_VO_WEIGHT_DEFAULT, 
                 CFG_TL_WFQ_VO_WEIGHT_MIN, 
                 CFG_TL_WFQ_VO_WEIGHT_MAX ),

   REG_VARIABLE( CFG_TL_DELAYED_TRGR_FRM_INT_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, DelayedTriggerFrmInt, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_TL_DELAYED_TRGR_FRM_INT_DEFAULT, 
                 CFG_TL_DELAYED_TRGR_FRM_INT_MIN, 
                 CFG_TL_DELAYED_TRGR_FRM_INT_MAX ),

   REG_VARIABLE_STRING( CFG_WOWL_PATTERN_NAME, WLAN_PARAM_String,
                        hdd_config_t, wowlPattern, 
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_WOWL_PATTERN_DEFAULT ),

   REG_VARIABLE( CFG_QOS_IMPLICIT_SETUP_ENABLED_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, bImplicitQosEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_IMPLICIT_SETUP_ENABLED_DEFAULT, 
                 CFG_QOS_IMPLICIT_SETUP_ENABLED_MIN, 
                 CFG_QOS_IMPLICIT_SETUP_ENABLED_MAX ),

   REG_VARIABLE( CFG_19P2_MHZ_PMIC_CLK_ENABLED_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, b19p2MhzPmicClkEnabled, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_19P2_MHZ_PMIC_CLK_ENABLED_DEFAULT, 
                 CFG_19P2_MHZ_PMIC_CLK_ENABLED_MIN, 
                 CFG_19P2_MHZ_PMIC_CLK_ENABLED_MAX ),

   REG_VARIABLE( CFG_BTC_EXECUTION_MODE_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcExecutionMode, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_BTC_EXECUTION_MODE_DEFAULT, 
                 CFG_BTC_EXECUTION_MODE_MIN, 
                 CFG_BTC_EXECUTION_MODE_MAX ),

   REG_VARIABLE( CFG_BTC_DHCP_PROTECTION_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcConsBtSlotsToBlockDuringDhcp,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_DHCP_PROTECTION_DEFAULT,
                 CFG_BTC_DHCP_PROTECTION_MIN,
                 CFG_BTC_DHCP_PROTECTION_MAX ),

   REG_VARIABLE( CFG_BTC_A2DP_DHCP_PROTECTION_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, btcA2DPBtSubIntervalsDuringDhcp,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BTC_A2DP_DHCP_PROTECTION_DEFAULT,
                 CFG_BTC_A2DP_DHCP_PROTECTION_MIN,
                 CFG_BTC_A2DP_DHCP_PROTECTION_MAX ),

#ifdef WLAN_SOFTAP_FEATURE
   REG_VARIABLE( CFG_AP_LISTEN_MODE_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, nEnableListenMode, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_AP_LISTEN_MODE_DEFAULT, 
                 CFG_AP_LISTEN_MODE_MIN, 
                 CFG_AP_LISTEN_MODE_MAX ),                     

   REG_VARIABLE( CFG_AP_AUTO_SHUT_OFF , WLAN_PARAM_Integer,
                 hdd_config_t, nAPAutoShutOff,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_AP_AUTO_SHUT_OFF_DEFAULT,
                 CFG_AP_AUTO_SHUT_OFF_MIN,
                 CFG_AP_AUTO_SHUT_OFF_MAX ),
#endif

#if defined WLAN_FEATURE_VOWIFI
   REG_VARIABLE( CFG_RRM_ENABLE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fRrmEnable, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_RRM_ENABLE_DEFAULT, 
                 CFG_RRM_ENABLE_MIN, 
                 CFG_RRM_ENABLE_MAX ),

   REG_VARIABLE( CFG_RRM_OPERATING_CHAN_MAX_DURATION_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nInChanMeasMaxDuration, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_RRM_OPERATING_CHAN_MAX_DURATION_DEFAULT, 
                 CFG_RRM_OPERATING_CHAN_MAX_DURATION_MIN, 
                 CFG_RRM_OPERATING_CHAN_MAX_DURATION_MAX ),

   REG_VARIABLE( CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nOutChanMeasMaxDuration, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_DEFAULT, 
                 CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_MIN, 
                 CFG_RRM_NON_OPERATING_CHAN_MAX_DURATION_MAX ),

   REG_VARIABLE( CFG_RRM_MEAS_RANDOMIZATION_INTVL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nRrmRandnIntvl, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_RRM_MEAS_RANDOMIZATION_INTVL_DEFAULT, 
                 CFG_RRM_MEAS_RANDOMIZATION_INTVL_MIN, 
                 CFG_RRM_MEAS_RANDOMIZATION_INTVL_MAX ),
#endif

#ifdef WLAN_FEATURE_VOWIFI_11R   
   REG_VARIABLE( CFG_FT_ENABLE_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fFTEnable, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_FT_ENABLE_DEFAULT, 
                 CFG_FT_ENABLE_MIN, 
                 CFG_FT_ENABLE_MAX ),

   REG_VARIABLE( CFG_FT_RESOURCE_REQ_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fFTResourceReqSupported, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_FT_RESOURCE_REQ_DEFAULT, 
                 CFG_FT_RESOURCE_REQ_MIN, 
                 CFG_FT_RESOURCE_REQ_MAX ),
#endif

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
   REG_VARIABLE( CFG_NEIGHBOR_SCAN_TIMER_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborScanPeriod, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_NEIGHBOR_SCAN_TIMER_PERIOD_DEFAULT, 
                 CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MIN, 
                 CFG_NEIGHBOR_SCAN_TIMER_PERIOD_MAX ),

   REG_VARIABLE( CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborReassocRssiThreshold, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_DEFAULT, 
                 CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_MIN, 
                 CFG_NEIGHBOR_REASSOC_RSSI_THRESHOLD_MAX ),

   REG_VARIABLE( CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborLookupRssiThreshold, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_DEFAULT, 
                 CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MIN, 
                 CFG_NEIGHBOR_LOOKUP_RSSI_THRESHOLD_MAX ),

   REG_VARIABLE_STRING( CFG_NEIGHBOR_SCAN_CHAN_LIST_NAME, WLAN_PARAM_String,
                        hdd_config_t, neighborScanChanList, 
                        VAR_FLAGS_OPTIONAL,
                        (void *)CFG_NEIGHBOR_SCAN_CHAN_LIST_DEFAULT ),

   REG_VARIABLE( CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborScanMinChanTime,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_DEFAULT,
                 CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MIN,
                 CFG_NEIGHBOR_SCAN_MIN_CHAN_TIME_MAX ),

   REG_VARIABLE( CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborScanMaxChanTime, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_DEFAULT, 
                 CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MIN, 
                 CFG_NEIGHBOR_SCAN_MAX_CHAN_TIME_MAX ),

   REG_VARIABLE( CFG_11R_NEIGHBOR_REQ_MAX_TRIES_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nMaxNeighborReqTries,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_11R_NEIGHBOR_REQ_MAX_TRIES_DEFAULT,
                 CFG_11R_NEIGHBOR_REQ_MAX_TRIES_MIN,
                 CFG_11R_NEIGHBOR_REQ_MAX_TRIES_MAX),

   REG_VARIABLE( CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nNeighborResultsRefreshPeriod, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_DEFAULT, 
                 CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MIN, 
                 CFG_NEIGHBOR_SCAN_RESULTS_REFRESH_PERIOD_MAX ),
#endif /* WLAN_FEATURE_NEIGHBOR_ROAMING */

   REG_VARIABLE( CFG_QOS_WMM_BURST_SIZE_DEFN_NAME , WLAN_PARAM_Integer,
                 hdd_config_t, burstSizeDefinition, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_BURST_SIZE_DEFN_DEFAULT, 
                 CFG_QOS_WMM_BURST_SIZE_DEFN_MIN, 
                 CFG_QOS_WMM_BURST_SIZE_DEFN_MAX ),

   REG_VARIABLE( CFG_MCAST_BCAST_FILTER_SETTING_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, mcastBcastFilterSetting,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_MCAST_BCAST_FILTER_SETTING_DEFAULT,
                 CFG_MCAST_BCAST_FILTER_SETTING_MIN,
                 CFG_MCAST_BCAST_FILTER_SETTING_MAX ),

   REG_VARIABLE( CFG_ENABLE_HOST_ARPOFFLOAD_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, fhostArpOffload,
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_ENABLE_HOST_ARPOFFLOAD_DEFAULT,
                 CFG_ENABLE_HOST_ARPOFFLOAD_MIN,
                 CFG_ENABLE_HOST_ARPOFFLOAD_MAX ),

   REG_VARIABLE( CFG_QOS_WMM_TS_INFO_ACK_POLICY_NAME , WLAN_PARAM_HexInteger,
                 hdd_config_t, tsInfoAckPolicy, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_QOS_WMM_TS_INFO_ACK_POLICY_DEFAULT, 
                 CFG_QOS_WMM_TS_INFO_ACK_POLICY_MIN, 
                 CFG_QOS_WMM_TS_INFO_ACK_POLICY_MAX ),

    REG_VARIABLE( CFG_SINGLE_TID_RC_NAME, WLAN_PARAM_Integer,
                  hdd_config_t, bSingleTidRc,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_SINGLE_TID_RC_DEFAULT,
                  CFG_SINGLE_TID_RC_MIN,
                  CFG_SINGLE_TID_RC_MAX),

    REG_VARIABLE( CFG_DYNAMIC_PSPOLL_VALUE_NAME, WLAN_PARAM_Integer,
                  hdd_config_t, dynamicPsPollValue,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_DYNAMIC_PSPOLL_VALUE_DEFAULT,
                  CFG_DYNAMIC_PSPOLL_VALUE_MIN,
                  CFG_DYNAMIC_PSPOLL_VALUE_MAX ),

   REG_VARIABLE( CFG_TELE_BCN_WAKEUP_EN_NAME, WLAN_PARAM_Integer,
                  hdd_config_t, teleBcnWakeupEn,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_TELE_BCN_WAKEUP_EN_DEFAULT,
                  CFG_TELE_BCN_WAKEUP_EN_MIN,
                  CFG_TELE_BCN_WAKEUP_EN_MAX ),

    REG_VARIABLE( CFG_INFRA_STA_KEEP_ALIVE_PERIOD_NAME, WLAN_PARAM_Integer,
                  hdd_config_t, infraStaKeepAlivePeriod,
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                  CFG_INFRA_STA_KEEP_ALIVE_PERIOD_DEFAULT,
                  CFG_INFRA_STA_KEEP_ALIVE_PERIOD_MIN,
                  CFG_INFRA_STA_KEEP_ALIVE_PERIOD_MAX),

    REG_VARIABLE( CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_NAME , WLAN_PARAM_Integer,
                  hdd_config_t, AddTSWhenACMIsOff, 
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                  CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_DEFAULT, 
                  CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_MIN, 
                  CFG_QOS_ADDTS_WHEN_ACM_IS_OFF_MAX ),


    REG_VARIABLE( CFG_VALIDATE_SCAN_LIST_NAME , WLAN_PARAM_Integer,
                  hdd_config_t, fValidateScanList, 
                  VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                  CFG_VALIDATE_SCAN_LIST_DEFAULT, 
                  CFG_VALIDATE_SCAN_LIST_MIN, 
                  CFG_VALIDATE_SCAN_LIST_MAX ),
   
    REG_VARIABLE( CFG_NULLDATA_AP_RESP_TIMEOUT_NAME, WLAN_PARAM_Integer,
                hdd_config_t, nNullDataApRespTimeout, 
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                CFG_NULLDATA_AP_RESP_TIMEOUT_DEFAULT, 
                CFG_NULLDATA_AP_RESP_TIMEOUT_MIN, 
                CFG_NULLDATA_AP_RESP_TIMEOUT_MAX ),

    REG_VARIABLE( CFG_AP_DATA_AVAIL_POLL_PERIOD_NAME, WLAN_PARAM_Integer,
                hdd_config_t, apDataAvailPollPeriodInMs, 
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                CFG_AP_DATA_AVAIL_POLL_PERIOD_DEFAULT, 
                CFG_AP_DATA_AVAIL_POLL_PERIOD_MIN, 
                CFG_AP_DATA_AVAIL_POLL_PERIOD_MAX ),
                                
   REG_VARIABLE( CFG_ENABLE_BTAMP_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, enableBtAmp, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_ENABLE_BTAMP_DEFAULT, 
                 CFG_ENABLE_BTAMP_MIN, 
                 CFG_ENABLE_BTAMP_MAX ),

#ifdef WLAN_BTAMP_FEATURE
   REG_VARIABLE( CFG_BT_AMP_PREFERRED_CHANNEL_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, preferredChannel, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                 CFG_BT_AMP_PREFERRED_CHANNEL_DEFAULT,
                 CFG_BT_AMP_PREFERRED_CHANNEL_MIN,
                 CFG_BT_AMP_PREFERRED_CHANNEL_MAX ),
#endif //WLAN_BTAMP_FEATURE
   REG_VARIABLE( CFG_BAND_CAPABILITY_NAME, WLAN_PARAM_Integer,
                 hdd_config_t, nBandCapability, 
                 VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                 CFG_BAND_CAPABILITY_DEFAULT, 
                 CFG_BAND_CAPABILITY_MIN, 
                 CFG_BAND_CAPABILITY_MAX ), 

   REG_VARIABLE( CFG_ENABLE_BEACON_EARLY_TERMINATION_NAME, WLAN_PARAM_Integer,
                hdd_config_t, fEnableBeaconEarlyTermination, 
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                CFG_ENABLE_BEACON_EARLY_TERMINATION_DEFAULT, 
                CFG_ENABLE_BEACON_EARLY_TERMINATION_MIN, 
                CFG_ENABLE_BEACON_EARLY_TERMINATION_MAX ), 

#ifdef FEATURE_WLAN_INTEGRATED_SOC
   /* note that since the default value is out of range we cannot
      enable range check, otherwise we get a system log message */
   REG_VARIABLE( CFG_WDI_TRACE_ENABLE_DAL_NAME, WLAN_PARAM_Integer,
                hdd_config_t, wdiTraceEnableDAL,
                VAR_FLAGS_OPTIONAL,
                CFG_WDI_TRACE_ENABLE_DEFAULT,
                CFG_WDI_TRACE_ENABLE_MIN,
                CFG_WDI_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_WDI_TRACE_ENABLE_CTL_NAME, WLAN_PARAM_Integer,
                hdd_config_t, wdiTraceEnableCTL,
                VAR_FLAGS_OPTIONAL,
                CFG_WDI_TRACE_ENABLE_DEFAULT,
                CFG_WDI_TRACE_ENABLE_MIN,
                CFG_WDI_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_WDI_TRACE_ENABLE_DAT_NAME, WLAN_PARAM_Integer,
                hdd_config_t, wdiTraceEnableDAT,
                VAR_FLAGS_OPTIONAL,
                CFG_WDI_TRACE_ENABLE_DEFAULT,
                CFG_WDI_TRACE_ENABLE_MIN,
                CFG_WDI_TRACE_ENABLE_MAX ),

   REG_VARIABLE( CFG_WDI_TRACE_ENABLE_PAL_NAME, WLAN_PARAM_Integer,
                hdd_config_t, wdiTraceEnablePAL,
                VAR_FLAGS_OPTIONAL,
                CFG_WDI_TRACE_ENABLE_DEFAULT,
                CFG_WDI_TRACE_ENABLE_MIN,
                CFG_WDI_TRACE_ENABLE_MAX ),
#endif /* FEATURE_WLAN_INTEGRATED_SOC */

  REG_VARIABLE( CFG_TELE_BCN_TRANS_LI_NAME, WLAN_PARAM_Integer,
               hdd_config_t, nTeleBcnTransListenInterval, 
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
               CFG_TELE_BCN_TRANS_LI_DEFAULT, 
               CFG_TELE_BCN_TRANS_LI_MIN, 
               CFG_TELE_BCN_TRANS_LI_MAX ), 

  REG_VARIABLE( CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_NAME, WLAN_PARAM_Integer,
               hdd_config_t, nTeleBcnTransLiNumIdleBeacons, 
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
               CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_DEFAULT, 
               CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_MIN, 
               CFG_TELE_BCN_TRANS_LI_NUM_IDLE_BCNS_MAX ), 

  REG_VARIABLE( CFG_TELE_BCN_MAX_LI_NAME, WLAN_PARAM_Integer,
               hdd_config_t, nTeleBcnMaxListenInterval, 
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
               CFG_TELE_BCN_MAX_LI_DEFAULT, 
               CFG_TELE_BCN_MAX_LI_MIN, 
               CFG_TELE_BCN_MAX_LI_MAX ), 

  REG_VARIABLE( CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_NAME, WLAN_PARAM_Integer,
               hdd_config_t, nTeleBcnMaxLiNumIdleBeacons, 
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
               CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_DEFAULT, 
               CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_MIN, 
               CFG_TELE_BCN_MAX_LI_NUM_IDLE_BCNS_MAX ), 

  REG_VARIABLE( CFG_BCN_EARLY_TERM_WAKE_NAME, WLAN_PARAM_Integer,
               hdd_config_t, bcnEarlyTermWakeInterval, 
               VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
               CFG_BCN_EARLY_TERM_WAKE_DEFAULT, 
               CFG_BCN_EARLY_TERM_WAKE_MIN, 
               CFG_BCN_EARLY_TERM_WAKE_MAX ), 

 REG_VARIABLE( CFG_AP_DATA_AVAIL_POLL_PERIOD_NAME, WLAN_PARAM_Integer,
              hdd_config_t, apDataAvailPollPeriodInMs, 
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
              CFG_AP_DATA_AVAIL_POLL_PERIOD_DEFAULT, 
              CFG_AP_DATA_AVAIL_POLL_PERIOD_MIN, 
              CFG_AP_DATA_AVAIL_POLL_PERIOD_MAX ),

   REG_VARIABLE( CFG_ENABLE_CLOSE_LOOP_NAME, WLAN_PARAM_Integer,
                hdd_config_t, enableCloseLoop, 
                VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                CFG_ENABLE_CLOSE_LOOP_DEFAULT, 
                CFG_ENABLE_CLOSE_LOOP_MIN, 
                CFG_ENABLE_CLOSE_LOOP_MAX ),

 REG_VARIABLE( CFG_ENABLE_BYPASS_11D_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableBypass11d, 
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
              CFG_ENABLE_BYPASS_11D_DEFAULT, 
              CFG_ENABLE_BYPASS_11D_MIN, 
              CFG_ENABLE_BYPASS_11D_MAX ),

 REG_VARIABLE( CFG_ENABLE_DFS_CHNL_SCAN_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableDFSChnlScan, 
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
              CFG_ENABLE_DFS_CHNL_SCAN_DEFAULT, 
              CFG_ENABLE_DFS_CHNL_SCAN_MIN, 
              CFG_ENABLE_DFS_CHNL_SCAN_MAX ),

 REG_VARIABLE( CFG_ENABLE_DYNAMIC_DTIM_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableDynamicDTIM, 
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
              CFG_ENABLE_DYNAMIC_DTIM_DEFAULT, 
              CFG_ENABLE_DYNAMIC_DTIM_MIN, 
              CFG_ENABLE_DYNAMIC_DTIM_MAX ),

 REG_VARIABLE( CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableAutomaticTxPowerControl, 
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
              CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_DEFAULT, 
              CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_MIN, 
              CFG_ENABLE_AUTOMATIC_TX_POWER_CONTROL_MAX ),

 REG_VARIABLE( CFG_SHORT_GI_40MHZ_NAME, WLAN_PARAM_Integer,
              hdd_config_t, ShortGI40MhzEnable, 
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
              CFG_SHORT_GI_40MHZ_DEFAULT, 
              CFG_SHORT_GI_40MHZ_MIN, 
              CFG_SHORT_GI_40MHZ_MAX ),

 REG_DYNAMIC_VARIABLE( CFG_REPORT_MAX_LINK_SPEED, WLAN_PARAM_Integer,
                       hdd_config_t, reportMaxLinkSpeed, 
                       VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
                       CFG_REPORT_MAX_LINK_SPEED_DEFAULT, 
                       CFG_REPORT_MAX_LINK_SPEED_MIN, 
                       CFG_REPORT_MAX_LINK_SPEED_MAX,
                       NULL, 0 ),

 REG_DYNAMIC_VARIABLE( CFG_LINK_SPEED_RSSI_HIGH, WLAN_PARAM_SignedInteger,
                       hdd_config_t, linkSpeedRssiHigh,
                       VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                       CFG_LINK_SPEED_RSSI_HIGH_DEFAULT,
                       CFG_LINK_SPEED_RSSI_HIGH_MIN,
                       CFG_LINK_SPEED_RSSI_HIGH_MAX,
                       NULL, 0 ),

 REG_DYNAMIC_VARIABLE( CFG_LINK_SPEED_RSSI_LOW, WLAN_PARAM_SignedInteger,
                       hdd_config_t, linkSpeedRssiLow,
                       VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
                       CFG_LINK_SPEED_RSSI_LOW_DEFAULT,
                       CFG_LINK_SPEED_RSSI_LOW_MIN,
                       CFG_LINK_SPEED_RSSI_LOW_MAX,
                       NULL, 0 ),

#ifdef WLAN_FEATURE_P2P
 REG_VARIABLE( CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_NAME, WLAN_PARAM_Integer,
              hdd_config_t, isP2pDeviceAddrAdministrated,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_DEFAULT,
              CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_MIN,
              CFG_P2P_DEVICE_ADDRESS_ADMINISTRATED_MAX ),
#endif

REG_VARIABLE( CFG_ENABLE_MCC_ENABLED_NAME, WLAN_PARAM_Integer,
             hdd_config_t, enableMCC, 
             VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
             CFG_ENABLE_MCC_ENABLED_DEFAULT, 
             CFG_ENABLE_MCC_ENABLED_MIN, 
             CFG_ENABLE_MCC_ENABLED_MAX ),

 REG_VARIABLE( CFG_THERMAL_MIGRATION_ENABLE_NAME, WLAN_PARAM_Integer,
              hdd_config_t, thermalMitigationEnable, 
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
              CFG_THERMAL_MIGRATION_ENABLE_DEFAULT, 
              CFG_THERMAL_MIGRATION_ENABLE_MIN, 
              CFG_THERMAL_MIGRATION_ENABLE_MAX ),
#ifdef WLAN_FEATURE_PACKET_FILTERING
 REG_VARIABLE( CFG_MC_ADDR_LIST_FILTER_NAME, WLAN_PARAM_Integer,
              hdd_config_t, isMcAddrListFilter,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_MC_ADDR_LIST_FILTER_DEFAULT,
              CFG_MC_ADDR_LIST_FILTER_MIN,
              CFG_MC_ADDR_LIST_FILTER_MAX ),
#endif
  
REG_VARIABLE( CFG_ENABLE_MODULATED_DTIM_NAME, WLAN_PARAM_Integer,
              hdd_config_t, enableModulatedDTIM, 
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT, 
              CFG_ENABLE_MODULATED_DTIM_DEFAULT, 
              CFG_ENABLE_MODULATED_DTIM_MIN, 
              CFG_ENABLE_MODULATED_DTIM_MAX ),

 REG_VARIABLE( CFG_MC_ADDR_LIST_ENABLE_NAME, WLAN_PARAM_Integer,
              hdd_config_t, fEnableMCAddrList,
              VAR_FLAGS_OPTIONAL | VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT,
              CFG_MC_ADDR_LIST_ENABLE_DEFAULT,
              CFG_MC_ADDR_LIST_ENABLE_MIN,
              CFG_MC_ADDR_LIST_ENABLE_MAX ),

};

/*
 * This function returns a pointer to the character after the occurence
 * of a new line character. It also modifies the original string by replacing
 * the '\n' character with the null character.
 * Function returns NULL if no new line character was found before end of
 * string was reached
 */
static char* get_next_line(char* str)
{
  char c;

  if( str == NULL || *str == '\0') {
    return NULL;
  }

  c = *str;
  while(c != '\n'  && c != '\0' && c != 0xd)  { 
    str = str + 1;
    c = *str;
  }

  if (c == '\0' ) {
    return NULL;
  }
  else
  {
    *str = '\0'; 
    return (str+1);
  }

  return NULL;
}

// look for space. Ascii values to look are -
// 0x09 == horizontal tab
// 0x0a == Newline ("\n")
// 0x0b == vertical tab
// 0x0c == Newpage or feed form.
// 0x0d == carriage return (CR or "\r")
// Null ('\0') should not considered as space.
#define i_isspace(ch)  (((ch) >= 0x09 && (ch) <= 0x0d) || (ch) == ' ')

/*
 * This function trims any leading and trailing white spaces
 */
static char *i_trim(char *str)

{
   char *ptr;

   if(*str == '\0') return str;

   /* Find the first non white-space*/
   for (ptr = str; i_isspace(*ptr); ptr++);
      if (*ptr == '\0')
         return str;

   /* This is the new start of the string*/
   str = ptr;

   /* Find the last non white-space */
   ptr += strlen(ptr) - 1;
   for (; ptr != str && i_isspace(*ptr); ptr--);
      /* Null terminate the following character */
      ptr[1] = '\0';
                                  
   return str;
}


//Structure to store each entry in qcom_cfg.ini file
typedef struct
{
   char *name;
   char *value;
}tCfgIniEntry;

static VOS_STATUS hdd_apply_cfg_ini( hdd_context_t * pHddCtx, 
    tCfgIniEntry* iniTable, unsigned long entries);

#ifdef WLAN_CFG_DEBUG
void dump_cfg_ini (tCfgIniEntry* iniTable, unsigned long entries) 
{
   unsigned long i;

   for (i = 0; i < entries; i++) {
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "%s entry Name=[%s] value=[%s]", 
           WLAN_INI_FILE, iniTable[i].name, iniTable[i].value);
     }
}
#endif 

/*
 * This function reads the qcom_cfg.ini file and
 * parses each 'Name=Value' pair in the ini file
 */
VOS_STATUS hdd_parse_config_ini(hdd_context_t* pHddCtx)
{
   int status, i=0;
   /** Pointer for firmware image data */
   const struct firmware *fw = NULL;
   char *buffer, *line,*pTemp;
   size_t size;
   char *name, *value;
   tCfgIniEntry cfgIniTable[MAX_CFG_INI_ITEMS];
   VOS_STATUS vos_status = VOS_STATUS_SUCCESS;

   memset(cfgIniTable, 0, sizeof(cfgIniTable));

   status = request_firmware(&fw, WLAN_INI_FILE, pHddCtx->parent_dev);
   
   if(status)
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: request_firmware failed %d\n",__FUNCTION__, status);
      return VOS_STATUS_E_FAILURE;   
   }
   if(!fw || !fw->data || !fw->size) 
   {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: %s download failed\n",
             __FUNCTION__, WLAN_INI_FILE);
      return VOS_STATUS_E_FAILURE;
   } 

   hddLog(LOG1, "%s: qcom_cfg.ini Size %d\n",__FUNCTION__, fw->size);

   buffer = (char*)vos_mem_malloc(fw->size);
   
   if(NULL == buffer) {
      hddLog(VOS_TRACE_LEVEL_FATAL, "%s: kmalloc failure",__FUNCTION__);
      release_firmware(fw);
      return VOS_STATUS_E_FAILURE;
   } 
   pTemp = buffer;

   vos_mem_copy((void*)buffer,(void *)fw->data, fw->size);
   size = fw->size;
  
   while (buffer != NULL)
   {
      line = get_next_line(buffer);
      buffer = i_trim(buffer);

      hddLog(LOG1, "%s: item", buffer);

      if(strlen((char*)buffer) == 0 || *buffer == '#')  {
         buffer = line;
         continue;
      }
      else if(strncmp(buffer, "END", 3) == 0 ) {
         break;
      }
      else
      {
         name = buffer;
         while(*buffer != '=' && *buffer != '\0') 
            buffer++;
         if(*buffer != '\0') {
            *buffer++ = '\0';
            i_trim(name);
            if(strlen (name) != 0) {
               buffer = i_trim(buffer);
               if(strlen(buffer)>0) {
                  value = buffer;
                  while(!i_isspace(*buffer) && *buffer != '\0') 
                     buffer++;
                  *buffer = '\0';
                  cfgIniTable[i].name= name;
                  cfgIniTable[i++].value= value;
                  if(i >= MAX_CFG_INI_ITEMS) {
                     hddLog(LOGE,"%s: Number of items in %s > %d \n",
                        __FUNCTION__, WLAN_INI_FILE, MAX_CFG_INI_ITEMS);
                     break;
                  }
               }
            }
         }
      }
      buffer = line;
   }

   //Loop through the registry table and apply all these configs
   vos_status = hdd_apply_cfg_ini(pHddCtx, cfgIniTable, i);

   release_firmware(fw);
   vos_mem_free(pTemp);
   return vos_status;
} 


static void print_hdd_cfg(hdd_context_t *pHddCtx)
{
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "*********Config values in HDD Adapter*******");
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [RTSThreshold] Value = %lu",pHddCtx->cfg_ini->RTSThreshold) ;
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [OperatingChannel] Value = [%u]",pHddCtx->cfg_ini->OperatingChannel);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [PowerUsageControl] Value = [%s]",pHddCtx->cfg_ini->PowerUsageControl);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [fIsImpsEnabled] Value = [%u]",pHddCtx->cfg_ini->fIsImpsEnabled);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [AutoBmpsTimerEnabled] Value = [%u]",pHddCtx->cfg_ini->fIsAutoBmpsTimerEnabled);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nAutoBmpsTimerValue] Value = [%lu]",pHddCtx->cfg_ini->nAutoBmpsTimerValue);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nVccRssiTrigger] Value = [%u]",pHddCtx->cfg_ini->nVccRssiTrigger);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gIbssBssid] Value =[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]",
      pHddCtx->cfg_ini->IbssBssid.bytes[0],pHddCtx->cfg_ini->IbssBssid.bytes[1],
      pHddCtx->cfg_ini->IbssBssid.bytes[2],pHddCtx->cfg_ini->IbssBssid.bytes[3],
      pHddCtx->cfg_ini->IbssBssid.bytes[4],pHddCtx->cfg_ini->IbssBssid.bytes[5]);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, 
          "Name = [Intf0MacAddress] Value =[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]",
                                  pHddCtx->cfg_ini->intfMacAddr[0].bytes[0],
                                  pHddCtx->cfg_ini->intfMacAddr[0].bytes[1],
                                  pHddCtx->cfg_ini->intfMacAddr[0].bytes[2],
                                  pHddCtx->cfg_ini->intfMacAddr[0].bytes[3],
                                  pHddCtx->cfg_ini->intfMacAddr[0].bytes[4],
                                  pHddCtx->cfg_ini->intfMacAddr[0].bytes[5]);


  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, 
          "Name = [Intf1MacAddress] Value =[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]",
                                  pHddCtx->cfg_ini->intfMacAddr[1].bytes[0],
                                  pHddCtx->cfg_ini->intfMacAddr[1].bytes[1],
                                  pHddCtx->cfg_ini->intfMacAddr[1].bytes[2],
                                  pHddCtx->cfg_ini->intfMacAddr[1].bytes[3],
                                  pHddCtx->cfg_ini->intfMacAddr[1].bytes[4],
                                  pHddCtx->cfg_ini->intfMacAddr[1].bytes[5]);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, 
          "Name = [Intf2MacAddress] Value =[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]",
                                  pHddCtx->cfg_ini->intfMacAddr[2].bytes[0],
                                  pHddCtx->cfg_ini->intfMacAddr[2].bytes[1],
                                  pHddCtx->cfg_ini->intfMacAddr[2].bytes[2],
                                  pHddCtx->cfg_ini->intfMacAddr[2].bytes[3],
                                  pHddCtx->cfg_ini->intfMacAddr[2].bytes[4],
                                  pHddCtx->cfg_ini->intfMacAddr[2].bytes[5]);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, 
          "Name = [Intf3MacAddress] Value =[0x%x 0x%x 0x%x 0x%x 0x%x 0x%x]",
                                  pHddCtx->cfg_ini->intfMacAddr[3].bytes[0],
                                  pHddCtx->cfg_ini->intfMacAddr[3].bytes[1],
                                  pHddCtx->cfg_ini->intfMacAddr[3].bytes[2],
                                  pHddCtx->cfg_ini->intfMacAddr[3].bytes[3],
                                  pHddCtx->cfg_ini->intfMacAddr[3].bytes[4],
                                  pHddCtx->cfg_ini->intfMacAddr[3].bytes[5]);

#ifdef WLAN_SOFTAP_FEATURE

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApEnableUapsd] value = [%u]\n",pHddCtx->cfg_ini->apUapsdEnabled);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAPCntryCode] Value =[%c%c%c]\n",
      pHddCtx->cfg_ini->apCntryCode[0],pHddCtx->cfg_ini->apCntryCode[1],
      pHddCtx->cfg_ini->apCntryCode[2]);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableApProt] value = [%u]", pHddCtx->cfg_ini->apProtEnabled);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAPAutoShutOff] Value = [%u]\n", pHddCtx->cfg_ini->nAPAutoShutOff);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableListenMode] Value = [%u]\n", pHddCtx->cfg_ini->nEnableListenMode);  
  VOS_TRACE (VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApProtection] value = [%u]\n",pHddCtx->cfg_ini->apProtection);
  VOS_TRACE (VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableApOBSSProt] value = [%u]\n",pHddCtx->cfg_ini->apOBSSProtEnabled);
  VOS_TRACE (VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApAutoChannelSelection] value = [%u]\n",pHddCtx->cfg_ini->apAutoChannelSelection);
#endif
  
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [ChannelBondingMode] Value = [%lu]",pHddCtx->cfg_ini->nChannelBondingMode24GHz);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [ChannelBondingMode] Value = [%lu]",pHddCtx->cfg_ini->nChannelBondingMode5GHz);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [dot11Mode] Value = [%lu]",pHddCtx->cfg_ini->dot11Mode);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [WmmMode] Value = [%u] ",pHddCtx->cfg_ini->WmmMode);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [UapsdMask] Value = [0x%x] ",pHddCtx->cfg_ini->UapsdMask);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [PktClassificationBasis] Value = [%u] ",pHddCtx->cfg_ini->PktClassificationBasis);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [ImplicitQosIsEnabled] Value = [%u]",(int)pHddCtx->cfg_ini->bImplicitQosEnabled);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdVoSrvIntv] Value = [%lu] ",pHddCtx->cfg_ini->InfraUapsdVoSrvIntv);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdVoSuspIntv] Value = [%lu] ",pHddCtx->cfg_ini->InfraUapsdVoSuspIntv);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdViSrvIntv] Value = [%lu] ",pHddCtx->cfg_ini->InfraUapsdViSrvIntv);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdViSuspIntv] Value = [%lu] ",pHddCtx->cfg_ini->InfraUapsdViSuspIntv);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdBeSrvIntv] Value = [%lu] ",pHddCtx->cfg_ini->InfraUapsdBeSrvIntv);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdBeSuspIntv] Value = [%lu] ",pHddCtx->cfg_ini->InfraUapsdBeSuspIntv);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdBkSrvIntv] Value = [%lu] ",pHddCtx->cfg_ini->InfraUapsdBkSrvIntv);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraUapsdBkSuspIntv] Value = [%lu] ",pHddCtx->cfg_ini->InfraUapsdBkSuspIntv);
#ifdef FEATURE_WLAN_CCX
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraInactivityInterval] Value = [%lu] ",pHddCtx->cfg_ini->InfraInactivityInterval);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [CcxEnabled] Value = [%lu] ",pHddCtx->cfg_ini->isCcxIniFeatureEnabled);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [FastTransitionEnabled] Value = [%lu] ",pHddCtx->cfg_ini->isFastTransitionEnabled);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gTxPowerCap] Value = [%lu] dBm ",pHddCtx->cfg_ini->nTxPowerCap);
#endif 
#ifdef FEATURE_WLAN_LFR
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [FastRoamEnabled] Value = [%lu] ",pHddCtx->cfg_ini->isFastRoamIniFeatureEnabled);
#endif 
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [RoamRssiDiff] Value = [%lu] ",pHddCtx->cfg_ini->RoamRssiDiff);
#endif
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraDirAcVo] Value = [%u] ",pHddCtx->cfg_ini->InfraDirAcVo);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraNomMsduSizeAcVo] Value = [0x%x] ",pHddCtx->cfg_ini->InfraNomMsduSizeAcVo);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMeanDataRateAcVo] Value = [0x%lx] ",pHddCtx->cfg_ini->InfraMeanDataRateAcVo);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMinPhyRateAcVo] Value = [0x%lx] ",pHddCtx->cfg_ini->InfraMinPhyRateAcVo);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraSbaAcVo] Value = [0x%x] ",pHddCtx->cfg_ini->InfraSbaAcVo);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraDirAcVi] Value = [%u] ",pHddCtx->cfg_ini->InfraDirAcVi);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraNomMsduSizeAcVi] Value = [0x%x] ",pHddCtx->cfg_ini->InfraNomMsduSizeAcVi);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMeanDataRateAcVi] Value = [0x%lx] ",pHddCtx->cfg_ini->InfraMeanDataRateAcVi);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMinPhyRateAcVi] Value = [0x%lx] ",pHddCtx->cfg_ini->InfraMinPhyRateAcVi);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraSbaAcVi] Value = [0x%x] ",pHddCtx->cfg_ini->InfraSbaAcVi);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraDirAcBe] Value = [%u] ",pHddCtx->cfg_ini->InfraDirAcBe);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraNomMsduSizeAcBe] Value = [0x%x] ",pHddCtx->cfg_ini->InfraNomMsduSizeAcBe);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMeanDataRateAcBe] Value = [0x%lx] ",pHddCtx->cfg_ini->InfraMeanDataRateAcBe);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMinPhyRateAcBe] Value = [0x%lx] ",pHddCtx->cfg_ini->InfraMinPhyRateAcBe);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraSbaAcBe] Value = [0x%x] ",pHddCtx->cfg_ini->InfraSbaAcBe);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraDirAcBk] Value = [%u] ",pHddCtx->cfg_ini->InfraDirAcBk);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraNomMsduSizeAcBk] Value = [0x%x] ",pHddCtx->cfg_ini->InfraNomMsduSizeAcBk);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMeanDataRateAcBk] Value = [0x%lx] ",pHddCtx->cfg_ini->InfraMeanDataRateAcBk);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraMinPhyRateAcBk] Value = [0x%lx] ",pHddCtx->cfg_ini->InfraMinPhyRateAcBk);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [InfraSbaAcBk] Value = [0x%x] ",pHddCtx->cfg_ini->InfraSbaAcBk);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [WfqBkWeight] Value = [%u] ",pHddCtx->cfg_ini->WfqBkWeight);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [WfqBeWeight] Value = [%u] ",pHddCtx->cfg_ini->WfqBeWeight);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [WfqViWeight] Value = [%u] ",pHddCtx->cfg_ini->WfqViWeight);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [WfqVoWeight] Value = [%u] ",pHddCtx->cfg_ini->WfqVoWeight);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [DelayedTriggerFrmInt] Value = [%lu] ",pHddCtx->cfg_ini->DelayedTriggerFrmInt);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [mcastBcastFilterSetting] Value = [%u] ",pHddCtx->cfg_ini->mcastBcastFilterSetting);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [fhostArpOffload] Value = [%u] ",pHddCtx->cfg_ini->fhostArpOffload);
#ifdef WLAN_FEATURE_VOWIFI_11R  
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [fFTEnable] Value = [%lu] ",pHddCtx->cfg_ini->fFTEnable);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [fFTResourceReqSupported] Value = [%lu] ",pHddCtx->cfg_ini->fFTResourceReqSupported);
#endif

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborReassocRssiThreshold] Value = [%lu] ",pHddCtx->cfg_ini->nNeighborReassocRssiThreshold);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborLookupRssiThreshold] Value = [%lu] ",pHddCtx->cfg_ini->nNeighborLookupRssiThreshold);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborScanMinChanTime] Value = [%lu] ",pHddCtx->cfg_ini->nNeighborScanMinChanTime);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborScanMaxChanTime] Value = [%lu] ",pHddCtx->cfg_ini->nNeighborScanMaxChanTime);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nMaxNeighborRetries] Value = [%lu] ",pHddCtx->cfg_ini->nMaxNeighborReqTries);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborScanPeriod] Value = [%lu] ",pHddCtx->cfg_ini->nNeighborScanPeriod);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [nNeighborScanResultsRefreshPeriod] Value = [%lu] ",pHddCtx->cfg_ini->nNeighborResultsRefreshPeriod);
#endif
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [burstSizeDefinition] Value = [0x%x] ",pHddCtx->cfg_ini->burstSizeDefinition);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [tsInfoAckPolicy] Value = [0x%x] ",pHddCtx->cfg_ini->tsInfoAckPolicy);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [rfSettlingTimeUs] Value = [%u] ",pHddCtx->cfg_ini->rfSettlingTimeUs);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [bSingleTidRc] Value = [%u] ",pHddCtx->cfg_ini->bSingleTidRc);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gDynamicPSPollvalue] Value = [%u] ",pHddCtx->cfg_ini->dynamicPsPollValue);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gAddTSWhenACMIsOff] Value = [%u] ",pHddCtx->cfg_ini->AddTSWhenACMIsOff);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gValidateScanList] Value = [%u] ",pHddCtx->cfg_ini->fValidateScanList);

  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gStaKeepAlivePeriod] Value = [%u] ",pHddCtx->cfg_ini->infraStaKeepAlivePeriod);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApDataAvailPollInterVal] Value = [%u] ",pHddCtx->cfg_ini->apDataAvailPollPeriodInMs);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableBtAmp] Value = [%u] ",pHddCtx->cfg_ini->enableBtAmp);
#ifdef WLAN_BTAMP_FEATURE
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [BtAmpPreferredChannel] Value = [%u] ",pHddCtx->cfg_ini->preferredChannel);
#endif //WLAN_BTAMP_FEATURE
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [BandCapability] Value = [%u] ",pHddCtx->cfg_ini->nBandCapability);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [fEnableBeaconEarlyTermination] Value = [%u] ",pHddCtx->cfg_ini->fEnableBeaconEarlyTermination);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [teleBcnWakeupEnable] Value = [%u] ",pHddCtx->cfg_ini->teleBcnWakeupEn);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [transListenInterval] Value = [%u] ",pHddCtx->cfg_ini->nTeleBcnTransListenInterval);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [transLiNumIdleBeacons] Value = [%u] ",pHddCtx->cfg_ini->nTeleBcnTransLiNumIdleBeacons);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [maxListenInterval] Value = [%u] ",pHddCtx->cfg_ini->nTeleBcnMaxListenInterval);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [maxLiNumIdleBeacons] Value = [%u] ",pHddCtx->cfg_ini->nTeleBcnMaxLiNumIdleBeacons);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [bcnEarlyTermWakeInterval] Value = [%u] ",pHddCtx->cfg_ini->bcnEarlyTermWakeInterval);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gApDataAvailPollInterVal] Value = [%u] ",pHddCtx->cfg_ini->apDataAvailPollPeriodInMs);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableBypass11d] Value = [%u] ",pHddCtx->cfg_ini->enableBypass11d);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gEnableDFSChnlScan] Value = [%u] ",pHddCtx->cfg_ini->enableDFSChnlScan);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [gReportMaxLinkSpeed] Value = [%u] ",pHddCtx->cfg_ini->reportMaxLinkSpeed);
  VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Name = [thermalMitigationEnable] Value = [%u] ",pHddCtx->cfg_ini->thermalMitigationEnable);
}


#define CFG_VALUE_MAX_LEN 256
#define CFG_ENTRY_MAX_LEN (32+CFG_VALUE_MAX_LEN)
VOS_STATUS hdd_cfg_get_config(hdd_context_t *pHddCtx, char *pBuf, int buflen)
{
   unsigned int idx;
   REG_TABLE_ENTRY *pRegEntry = g_registry_table;
   unsigned long cRegTableEntries  = sizeof(g_registry_table) / sizeof( g_registry_table[ 0 ]);
   v_U32_t value;
   char valueStr[CFG_VALUE_MAX_LEN];
   char configStr[CFG_ENTRY_MAX_LEN];
   char *fmt;
   void *pField;
   v_MACADDR_t *pMacAddr;
   char *pCur = pBuf;
   int curlen;

   // start with an empty string
   *pCur = '\0';

   for ( idx = 0; idx < cRegTableEntries; idx++, pRegEntry++ ) 
   {
      pField = ( (v_U8_t *)pHddCtx->cfg_ini) + pRegEntry->VarOffset;

      if ( ( WLAN_PARAM_Integer       == pRegEntry->RegType ) ||
           ( WLAN_PARAM_SignedInteger == pRegEntry->RegType ) ||
           ( WLAN_PARAM_HexInteger    == pRegEntry->RegType ) ) 
      {
         value = 0;
         memcpy( &value, pField, pRegEntry->VarSize );
         if ( WLAN_PARAM_HexInteger == pRegEntry->RegType )
         {
            fmt = "%x";
         }
         else if ( WLAN_PARAM_SignedInteger == pRegEntry->RegType )
         {
            fmt = "%d";
         }
         else
         {
            fmt = "%u";
         }
         snprintf(valueStr, CFG_VALUE_MAX_LEN, fmt, value);
      }
      else if ( WLAN_PARAM_String == pRegEntry->RegType )
      {
         snprintf(valueStr, CFG_VALUE_MAX_LEN, "%s", (char *)pField);
      }
      else if ( WLAN_PARAM_MacAddr == pRegEntry->RegType )
      {
         pMacAddr = (v_MACADDR_t *) pField;
         snprintf(valueStr, CFG_VALUE_MAX_LEN,
                  "%02x:%02x:%02x:%02x:%02x:%02x",
                  pMacAddr->bytes[0],
                  pMacAddr->bytes[1],
                  pMacAddr->bytes[2],
                  pMacAddr->bytes[3],
                  pMacAddr->bytes[4],
                  pMacAddr->bytes[5]);
      }
      else
      {
         snprintf(valueStr, CFG_VALUE_MAX_LEN, "(unhandled)");
      }
      curlen = snprintf(configStr, CFG_ENTRY_MAX_LEN,
                        "%s=[%s]%s\n",
                        pRegEntry->RegName,
                        valueStr,
                        test_bit(idx, (void *)&pHddCtx->cfg_ini->bExplicitCfg) ?
                        "*" : "");

      // ideally we want to return the config to the application
      // however the config is too big so we just printk() for now
#ifdef RETURN_IN_BUFFER
      if (curlen <= buflen)
      {
         // copy string + '\0'
         memcpy(pCur, configStr, curlen+1);

         // account for addition;
         pCur += curlen;
         buflen -= curlen;
      }
      else
      {
         // buffer space exhausted, return what we have
         return VOS_STATUS_E_RESOURCES;
      }
#else
      printk(KERN_CRIT "%s", configStr);
#endif // RETURN_IN_BUFFER

}

#ifndef RETURN_IN_BUFFER
   // notify application that output is in system log
   snprintf(pCur, buflen, "WLAN configuration written to system log");
#endif // RETURN_IN_BUFFER

   return VOS_STATUS_SUCCESS;
}

static VOS_STATUS find_cfg_item (tCfgIniEntry* iniTable, unsigned long entries,
    char *name, char** value) 
{
   VOS_STATUS status = VOS_STATUS_E_FAILURE;
   unsigned long i;

   for (i = 0; i < entries; i++) {
     if (strcmp(iniTable[i].name, name) == 0) {
       *value = iniTable[i].value;
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "Found %s entry for Name=[%s] Value=[%s] ",
           WLAN_INI_FILE, name, *value);
       return VOS_STATUS_SUCCESS;
     }
   }

   return status;
}

static int parseHexDigit(char c)
{
  if (c >= '0' && c <= '9')
    return c-'0';
  if (c >= 'a' && c <= 'f')
    return c-'a'+10;
  if (c >= 'A' && c <= 'F')
    return c-'A'+10;

  return 0;
}


static VOS_STATUS hdd_apply_cfg_ini( hdd_context_t *pHddCtx, tCfgIniEntry* iniTable, unsigned long entries)
{
   VOS_STATUS match_status = VOS_STATUS_E_FAILURE;
   VOS_STATUS ret_status = VOS_STATUS_SUCCESS;
   unsigned int idx;
   void *pField;
   char *value_str = NULL;
   unsigned long len_value_str;
   char *candidate;
   v_U32_t value;
   v_S31_t svalue;
   void *pStructBase = pHddCtx->cfg_ini;
   REG_TABLE_ENTRY *pRegEntry = g_registry_table;
   unsigned long cRegTableEntries  = sizeof(g_registry_table) / sizeof( g_registry_table[ 0 ]);
   v_U32_t cbOutString;
   int i;

   // sanity test
   if (MAX_CFG_INI_ITEMS < cRegTableEntries)
   {
      hddLog(LOGE, "%s: MAX_CFG_INI_ITEMS too small, must be at least %d",
             __FUNCTION__, cRegTableEntries);
   }

   for ( idx = 0; idx < cRegTableEntries; idx++, pRegEntry++ )
   {
      //Calculate the address of the destination field in the structure.
      pField = ( (v_U8_t *)pStructBase )+ pRegEntry->VarOffset;

      match_status = find_cfg_item(iniTable, entries, pRegEntry->RegName, &value_str);

      if( (match_status != VOS_STATUS_SUCCESS) && ( pRegEntry->Flags & VAR_FLAGS_REQUIRED ) )
      {
         // If we could not read the cfg item and it is required, this is an error.
         hddLog(LOGE, "%s: Failed to read required config parameter %s",
            __FUNCTION__, pRegEntry->RegName);
         ret_status = VOS_STATUS_E_FAILURE;
         break;
      }

      if ( ( WLAN_PARAM_Integer    == pRegEntry->RegType ) ||
           ( WLAN_PARAM_HexInteger == pRegEntry->RegType ) )
      {
         // If successfully read from the registry, use the value read.
         // If not, use the default value.
         if ( match_status == VOS_STATUS_SUCCESS && (WLAN_PARAM_Integer == pRegEntry->RegType)) {
            value = simple_strtoul(value_str, NULL, 10);
         }
         else if ( match_status == VOS_STATUS_SUCCESS && (WLAN_PARAM_HexInteger == pRegEntry->RegType)) {
            value = simple_strtoul(value_str, NULL, 16);
         }
         else {
            value = pRegEntry->VarDefault;
         }

         // If this parameter needs range checking, do it here.
         if ( pRegEntry->Flags & VAR_FLAGS_RANGE_CHECK )
         {
            if ( value > pRegEntry->VarMax )
            {
               hddLog(LOGE, "%s: Reg Parameter %s > allowed Maximum [%lu > %lu]. Enforcing Maximum",
                      __FUNCTION__, pRegEntry->RegName, value, pRegEntry->VarMax );
               value = pRegEntry->VarMax;
            }

            if ( value < pRegEntry->VarMin )
            {
               hddLog(LOGE, "%s: Reg Parameter %s < allowed Minimum [%lu < %lu]. Enforcing Minimum",
                      __FUNCTION__, pRegEntry->RegName, value, pRegEntry->VarMin);
               value = pRegEntry->VarMin;
            }
         }
         // If this parameter needs range checking, do it here.
         else if ( pRegEntry->Flags & VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT )
         {
            if ( value > pRegEntry->VarMax )
            {
               hddLog(LOGE, "%s: Reg Parameter %s > allowed Maximum [%lu > %lu]. Enforcing Default= %lu",
                  __FUNCTION__, pRegEntry->RegName, value, pRegEntry->VarMax, pRegEntry->VarDefault  );
               value = pRegEntry->VarDefault;
            }

            if ( value < pRegEntry->VarMin )
            {
               hddLog(LOGE, "%s: Reg Parameter %s < allowed Minimum [%lu < %lu]. Enforcing Default= %lu",
                  __FUNCTION__, pRegEntry->RegName, value, pRegEntry->VarMin, pRegEntry->VarDefault  );
               value = pRegEntry->VarDefault;
            }
         }

         // Move the variable into the output field.
         memcpy( pField, &value, pRegEntry->VarSize );
      }
      else if ( WLAN_PARAM_SignedInteger == pRegEntry->RegType )
      {
         // If successfully read from the registry, use the value read.
         // If not, use the default value.
         if (VOS_STATUS_SUCCESS == match_status)
         {
            svalue = simple_strtol(value_str, NULL, 10);
         }
         else
         {
            svalue = (v_S31_t)pRegEntry->VarDefault;
         }

         // If this parameter needs range checking, do it here.
         if ( pRegEntry->Flags & VAR_FLAGS_RANGE_CHECK )
         {
            if ( svalue > (v_S31_t)pRegEntry->VarMax )
            {
               hddLog(LOGE, "%s: Reg Parameter %s > allowed Maximum "
                      "[%ld > %ld]. Enforcing Maximum", __FUNCTION__,
                      pRegEntry->RegName, svalue, (int)pRegEntry->VarMax );
               svalue = (v_S31_t)pRegEntry->VarMax;
            }

            if ( svalue < (v_S31_t)pRegEntry->VarMin )
            {
               hddLog(LOGE, "%s: Reg Parameter %s < allowed Minimum "
                      "[%ld < %ld]. Enforcing Minimum",  __FUNCTION__,
                      pRegEntry->RegName, svalue, (int)pRegEntry->VarMin);
               svalue = (v_S31_t)pRegEntry->VarMin;
            }
         }
         // If this parameter needs range checking, do it here.
         else if ( pRegEntry->Flags & VAR_FLAGS_RANGE_CHECK_ASSUME_DEFAULT )
         {
            if ( svalue > (v_S31_t)pRegEntry->VarMax )
            {
               hddLog(LOGE, "%s: Reg Parameter %s > allowed Maximum "
                      "[%ld > %ld]. Enforcing Default= %ld",
                      __FUNCTION__, pRegEntry->RegName, svalue,
                      (int)pRegEntry->VarMax,
                      (int)pRegEntry->VarDefault  );
               svalue = (v_S31_t)pRegEntry->VarDefault;
            }

            if ( svalue < (v_S31_t)pRegEntry->VarMin )
            {
               hddLog(LOGE, "%s: Reg Parameter %s < allowed Minimum "
                      "[%ld < %ld]. Enforcing Default= %ld",
                      __FUNCTION__, pRegEntry->RegName, svalue,
                      (int)pRegEntry->VarMin,
                      (int)pRegEntry->VarDefault);
               svalue = pRegEntry->VarDefault;
            }
         }

         // Move the variable into the output field.
         memcpy( pField, &svalue, pRegEntry->VarSize );
      }
      // Handle string parameters
      else if ( WLAN_PARAM_String == pRegEntry->RegType )
      {
#ifdef WLAN_CFG_DEBUG
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, "RegName = %s, VarOffset %u VarSize %u VarDefault %s\n",
            pRegEntry->RegName, pRegEntry->VarOffset, pRegEntry->VarSize, (char*)pRegEntry->VarDefault); 
#endif

         if ( match_status == VOS_STATUS_SUCCESS) 
         {
            len_value_str = strlen(value_str);

            if(len_value_str > (pRegEntry->VarSize - 1)) {
               hddLog(LOGE, "%s: Invalid Value=[%s] specified for Name=[%s] in %s\n", 
                  __FUNCTION__, value_str, pRegEntry->RegName, WLAN_INI_FILE);
               cbOutString = utilMin( strlen( (char *)pRegEntry->VarDefault ), pRegEntry->VarSize - 1 );
               memcpy( pField, (void *)(pRegEntry->VarDefault), cbOutString );
               ( (v_U8_t *)pField )[ cbOutString ] = '\0';
            }
            else
            {
               memcpy( pField, (void *)(value_str), len_value_str);
               ( (v_U8_t *)pField )[ len_value_str ] = '\0';
            }
         }
         else 
         {
            // Failed to read the string parameter from the registry.  Use the default.
            cbOutString = utilMin( strlen( (char *)pRegEntry->VarDefault ), pRegEntry->VarSize - 1 );
            memcpy( pField, (void *)(pRegEntry->VarDefault), cbOutString );
            ( (v_U8_t *)pField )[ cbOutString ] = '\0';                 
         }
      }
      else if ( WLAN_PARAM_MacAddr == pRegEntry->RegType )
      {
         if(pRegEntry->VarSize != VOS_MAC_ADDR_SIZE) {
               hddLog(LOGE, "%s: Invalid VarSize %u for Name=[%s]\n", 
                   __FUNCTION__, pRegEntry->VarSize, pRegEntry->RegName); 
            continue;
         }
         candidate = (char*)pRegEntry->VarDefault;
         if ( match_status == VOS_STATUS_SUCCESS) {
            len_value_str = strlen(value_str);
            if(len_value_str != (VOS_MAC_ADDR_SIZE*2)) {
               hddLog(LOGE, "%s: Invalid MAC addr [%s] specified for Name=[%s] in %s\n", 
                  __FUNCTION__, value_str, pRegEntry->RegName, WLAN_INI_FILE);
            }
            else
               candidate = value_str;
         }
         //parse the string and store it in the byte array
         for(i=0; i<VOS_MAC_ADDR_SIZE; i++)
         {
            ((char*)pField)[i] = 
              (char)(parseHexDigit(candidate[i*2])*16 + parseHexDigit(candidate[i*2+1])); 
         }
      }
      else
      {
         hddLog(LOGE, "%s: Unknown param type for name[%s] in registry table\n", 
            __FUNCTION__, pRegEntry->RegName);
      }

      // did we successfully parse a cfg item for this parameter?
      if( (match_status == VOS_STATUS_SUCCESS) &&
          (idx < MAX_CFG_INI_ITEMS) )
      {
         set_bit(idx, (void *)&pHddCtx->cfg_ini->bExplicitCfg);
      }
   }

   print_hdd_cfg(pHddCtx);

  return( ret_status );
}

eCsrPhyMode hdd_cfg_xlate_to_csr_phy_mode( eHddDot11Mode dot11Mode )
{
   switch (dot11Mode) 
   {
      case (eHDD_DOT11_MODE_abg):
         return eCSR_DOT11_MODE_abg;
      case (eHDD_DOT11_MODE_11b):
         return eCSR_DOT11_MODE_11b;
      case (eHDD_DOT11_MODE_11g):
         return eCSR_DOT11_MODE_11g;
      default:
      case (eHDD_DOT11_MODE_11n):
         return eCSR_DOT11_MODE_11n;
      case (eHDD_DOT11_MODE_11g_ONLY):
         return eCSR_DOT11_MODE_11g_ONLY;
      case (eHDD_DOT11_MODE_11n_ONLY):
         return eCSR_DOT11_MODE_11n_ONLY;
      case (eHDD_DOT11_MODE_11b_ONLY):
         return eCSR_DOT11_MODE_11b_ONLY;
      case (eHDD_DOT11_MODE_AUTO):
         return eCSR_DOT11_MODE_AUTO;
   }

}

static void hdd_set_btc_config(hdd_context_t *pHddCtx) 
{
   hdd_config_t *pConfig = pHddCtx->cfg_ini;
   tSmeBtcConfig btcParams;
   
   sme_BtcGetConfig(pHddCtx->hHal, &btcParams);

   btcParams.btcExecutionMode = pConfig->btcExecutionMode;

   btcParams.btcConsBtSlotsToBlockDuringDhcp = pConfig->btcConsBtSlotsToBlockDuringDhcp;

   btcParams.btcA2DPBtSubIntervalsDuringDhcp = pConfig->btcA2DPBtSubIntervalsDuringDhcp;

   sme_BtcSetConfig(pHddCtx->hHal, &btcParams);
}

static void hdd_set_power_save_config(hdd_context_t *pHddCtx, tSmeConfigParams *smeConfig) 
{
   hdd_config_t *pConfig = pHddCtx->cfg_ini;

   tPmcBmpsConfigParams bmpsParams;
   
   sme_GetConfigPowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE, &bmpsParams);
   
   if (strcmp(pConfig->PowerUsageControl, "Min") == 0)
   {
      smeConfig->csrConfig.impsSleepTime   = pConfig->nImpsMinSleepTime;
      bmpsParams.bmpsPeriod                = pConfig->nBmpsMinListenInterval;
      bmpsParams.enableBeaconEarlyTermination = pConfig->fEnableBeaconEarlyTermination;
      bmpsParams.bcnEarlyTermWakeInterval  = pConfig->bcnEarlyTermWakeInterval;
   }
   if (strcmp(pConfig->PowerUsageControl, "Max") == 0)
   {
      smeConfig->csrConfig.impsSleepTime   = pConfig->nImpsMaxSleepTime;
      bmpsParams.bmpsPeriod                = pConfig->nBmpsMaxListenInterval;
      bmpsParams.enableBeaconEarlyTermination = pConfig->fEnableBeaconEarlyTermination;
      bmpsParams.bcnEarlyTermWakeInterval  = pConfig->bcnEarlyTermWakeInterval;
   }
   if (strcmp(pConfig->PowerUsageControl, "Mod") == 0)
   {
      smeConfig->csrConfig.impsSleepTime   = pConfig->nImpsModSleepTime;
      bmpsParams.bmpsPeriod                = pConfig->nBmpsModListenInterval;
      bmpsParams.enableBeaconEarlyTermination = pConfig->fEnableBeaconEarlyTermination;
      bmpsParams.bcnEarlyTermWakeInterval  = pConfig->bcnEarlyTermWakeInterval;
   }

   if (pConfig->fIsImpsEnabled)
   {
      sme_EnablePowerSave (pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   }
   else
   {
      sme_DisablePowerSave (pHddCtx->hHal, ePMC_IDLE_MODE_POWER_SAVE);
   }

   if (pConfig->fIsBmpsEnabled)
   {
      sme_EnablePowerSave (pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);
   }
   else
   {
      sme_DisablePowerSave (pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE);
   }

   bmpsParams.trafficMeasurePeriod = pConfig->nAutoBmpsTimerValue;

   if (sme_SetConfigPowerSave(pHddCtx->hHal, ePMC_BEACON_MODE_POWER_SAVE, &bmpsParams)== eHAL_STATUS_FAILURE)
   {
      hddLog(LOGE, "SetConfigPowerSave failed to set BMPS params\n");
   }
  
   if(pConfig->fIsAutoBmpsTimerEnabled)
   {
      sme_StartAutoBmpsTimer(pHddCtx->hHal);
   }

}

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
static VOS_STATUS hdd_string_to_u8_array( char *str, tANI_U8 *intArray, tANI_U8 *len, tANI_U8 intArrayMaxLen )
{
   char *s = str;

   if( str == NULL || intArray == NULL || len == NULL )
   {
      return VOS_STATUS_E_INVAL;
   }
   *len = 0;

#ifdef VERSION_USING_STRPBRK
   while ( (s != NULL) && (*len < intArrayMaxLen) )
   {
      int val;
      //Increment length only if sscanf succesfully extracted one element.
      //Any other return value means error. Ignore it.
      if( sscanf(s, "%d", &val ) == 1 )
      {
         intArray[*len] = (tANI_U8) val;
         *len += 1;
      }
      s = strpbrk( s, "," );
      if( s )
         s++;
   }
#else
   while( (*s != '\0')  && (*len < intArrayMaxLen) )
   {
      unsigned long val;

      val = simple_strtoul( s, &s, 10 );
      if( val )
      {
         intArray[*len] = (tANI_U8) val;
         *len += 1;
      }
      if( *s )
         s++;
   }
#endif

   return VOS_STATUS_SUCCESS;
   
}
#endif


v_BOOL_t hdd_update_config_dat( hdd_context_t *pHddCtx )
{
   v_BOOL_t  fStatus = TRUE;

   hdd_config_t *pConfig = pHddCtx->cfg_ini;

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_SHORT_GI_20MHZ, 
      pConfig->ShortGI20MhzEnable, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_SHORT_GI_20MHZ to CCM\n");
   }
       
   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_CAL_CONTROL, pConfig->Calibration, 
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_CAL_CONTROL to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_CAL_PERIOD, pConfig->CalibrationPeriod,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_CAL_PERIOD to CCM\n");
   }

   if ( 0 != pConfig->Cfg1Id )
   {
      if (ccmCfgSetInt(pHddCtx->hHal, pConfig->Cfg1Id, pConfig->Cfg1Value, NULL, 
         eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on Cfg1Id to CCM\n");
      }
          
   }

   if ( 0 != pConfig->Cfg2Id )
   {
      if (ccmCfgSetInt(pHddCtx->hHal, pConfig->Cfg2Id, pConfig->Cfg2Value, 
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on Cfg2Id to CCM\n");
      }
   }

   if ( 0 != pConfig->Cfg3Id )
   {
      if (ccmCfgSetInt(pHddCtx->hHal, pConfig->Cfg3Id, pConfig->Cfg3Value, 
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on Cfg3Id to CCM\n");
      }
   }

   if ( 0 != pConfig->Cfg4Id )
   {
      if (ccmCfgSetInt(pHddCtx->hHal, pConfig->Cfg4Id, pConfig->Cfg4Value, 
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on Cfg4Id to CCM\n");
      }
   }

   if ( 0 != pConfig->Cfg5Id )
   {
      if (ccmCfgSetInt(pHddCtx->hHal, pConfig->Cfg5Id, pConfig->Cfg5Value, 
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE, "Could not pass on Cfg5Id to CCM\n");
      }
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_BA_AUTO_SETUP, pConfig->BlockAckAutoSetup, 
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_BA_AUTO_SETUP to CCM\n");
   }
       
   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_FIXED_RATE, pConfig->TxRate, 
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_FIXED_RATE to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_RX_AMPDU_FACTOR, 
      pConfig->MaxRxAmpduFactor, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE,"Could not pass on WNI_CFG_HT_AMPDU_PARAMS_MAX_RX_AMPDU_FACTOR to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_SHORT_PREAMBLE, pConfig->fIsShortPreamble,
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE,"Could not pass on WNI_CFG_SHORT_PREAMBLE to CCM\n");
   }

   if (pConfig->fIsAutoIbssBssid) 
   {
      if (ccmCfgSetStr(pHddCtx->hHal, WNI_CFG_BSSID, (v_U8_t *)"000000000000", 
         sizeof(v_BYTE_t) * VOS_MAC_ADDR_SIZE, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
      {
         fStatus = FALSE;
         hddLog(LOGE,"Could not pass on WNI_CFG_BSSID to CCM\n");
      }
   }
   else
   { 
      if ( VOS_FALSE == vos_is_macaddr_group( &pConfig->IbssBssid ))
      {
         VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_MED,
                    "MAC Addr (IBSS BSSID) read from Registry is: %02x-%02x-%02x-%02x-%02x-%02x",
                    pConfig->IbssBssid.bytes[0], pConfig->IbssBssid.bytes[1], pConfig->IbssBssid.bytes[2], 
                    pConfig->IbssBssid.bytes[3], pConfig->IbssBssid.bytes[4], pConfig->IbssBssid.bytes[5]);
         if (ccmCfgSetStr(pHddCtx->hHal, WNI_CFG_BSSID, pConfig->IbssBssid.bytes, 
            sizeof(v_BYTE_t) * VOS_MAC_ADDR_SIZE, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
         {
            fStatus = FALSE;
            hddLog(LOGE,"Could not pass on WNI_CFG_BSSID to CCM\n");
         }
      }
      else
      {
         fStatus = FALSE;
         hddLog(LOGE,"Could not pass on WNI_CFG_BSSID to CCM\n");
      }
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_BEACON_INTERVAL, pConfig->nBeaconInterval, 
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_BEACON_INTERVAL to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MAX_PS_POLL, pConfig->nMaxPsPoll, 
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_MAX_PS_POLL to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_CURRENT_RX_ANTENNA, pConfig-> nRxAnt, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Failure: Could not pass on WNI_CFG_CURRENT_RX_ANTENNA configuration info to HAL\n"  );
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_LOW_GAIN_OVERRIDE, pConfig->fIsLowGainOverride, 
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_LOW_GAIN_OVERRIDE to HAL\n");
   }
 
   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RSSI_FILTER_PERIOD, pConfig->nRssiFilterPeriod,
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_RSSI_FILTER_PERIOD to CCM\n");
   }

#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_FT_RSSI_FILTER_PERIOD, pConfig->FTRssiFilterPeriod,
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_FT_RSSI_FILTER_PERIOD to CCM\n");
   }
#endif

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_IGNORE_DTIM, pConfig->fIgnoreDtim,
         NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_IGNORE_DTIM configuration to CCM\n"  );
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PS_ENABLE_HEART_BEAT, pConfig->fEnableFwHeartBeatMonitoring, 
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Failure: Could not pass on WNI_CFG_PS_HEART_BEAT configuration info to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PS_ENABLE_BCN_FILTER, pConfig->fEnableFwBeaconFiltering, 
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Failure: Could not pass on WNI_CFG_PS_BCN_FILTER configuration info to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PS_ENABLE_RSSI_MONITOR, pConfig->fEnableFwRssiMonitoring, 
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Failure: Could not pass on WNI_CFG_PS_RSSI_MONITOR configuration info to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PS_DATA_INACTIVITY_TIMEOUT, pConfig->nDataInactivityTimeout, 
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_PS_DATA_INACTIVITY_TIMEOUT configuration info to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_NTH_BEACON_FILTER, pConfig->nthBeaconFilter, 
                    NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_NTH_BEACON_FILTER configuration info to CCM\n");
   }

#ifdef WLAN_SOFTAP_FEATURE
     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_LTE_COEX, pConfig->enableLTECoex, 
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_LTE_COEX to CCM\n");
     }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_PHY_AGC_LISTEN_MODE, pConfig->nEnableListenMode, 
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_PHY_AGC_LISTEN_MODE to CCM\n");
     }

     WLANSAP_SetChannelRange(pHddCtx->hHal, pConfig->apStartChannelNum, pConfig->apEndChannelNum, pConfig->apOperatingBand);

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_AP_KEEP_ALIVE_TIMEOUT, pConfig->apKeepAlivePeriod, 
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_AP_KEEP_ALIVE_TIMEOUT to CCM\n");
     } 

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_GO_KEEP_ALIVE_TIMEOUT, pConfig->goKeepAlivePeriod, 
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_GO_KEEP_ALIVE_TIMEOUT to CCM\n");
     }
#endif
   
#if defined WLAN_FEATURE_VOWIFI
    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RRM_ENABLED, pConfig->fRrmEnable, 
                     NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
       fStatus = FALSE;
       hddLog(LOGE,"Failure: Could not pass on WNI_CFG_RRM_ENABLE configuration info to CCM\n");
    }

    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RRM_OPERATING_CHAN_MAX, pConfig->nInChanMeasMaxDuration, 
                     NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
       fStatus = FALSE;
       hddLog(LOGE,"Failure: Could not pass on WNI_CFG_RRM_OPERATING_CHAN_MAX configuration info to CCM\n");
    }

    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RRM_NON_OPERATING_CHAN_MAX, pConfig->nOutChanMeasMaxDuration, 
                     NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
       fStatus = FALSE;
       hddLog(LOGE,"Failure: Could not pass on WNI_CFG_RRM_OUT_CHAN_MAX configuration info to CCM\n");
    }

    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_MCAST_BCAST_FILTER_SETTING, pConfig->mcastBcastFilterSetting, 
                     NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
#endif

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_SINGLE_TID_RC, pConfig->bSingleTidRc, 
                      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_SINGLE_TID_RC configuration info to CCM\n");
     }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TELE_BCN_WAKEUP_EN, pConfig->teleBcnWakeupEn, 
                      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_TELE_BCN_WAKEUP_EN configuration info to CCM\n"  );
     }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TELE_BCN_TRANS_LI, pConfig->nTeleBcnTransListenInterval, 
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_TELE_BCN_TRANS_LI configuration info to CCM\n"  );
    }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TELE_BCN_MAX_LI, pConfig->nTeleBcnMaxListenInterval, 
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_TELE_BCN_MAX_LI configuration info to CCM\n"  );
    }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TELE_BCN_TRANS_LI_IDLE_BCNS, pConfig->nTeleBcnTransLiNumIdleBeacons, 
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_TELE_BCN_TRANS_LI_IDLE_BCNS configuration info to CCM\n"  );
    }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TELE_BCN_MAX_LI_IDLE_BCNS, pConfig->nTeleBcnMaxLiNumIdleBeacons, 
      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_TELE_BCN_MAX_LI_IDLE_BCNS configuration info to CCM\n"  );
    }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RF_SETTLING_TIME_CLK, pConfig->rfSettlingTimeUs,
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_RF_SETTLING_TIME_CLK configuration info to CCM\n"  );
    }

     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_INFRA_STA_KEEP_ALIVE_PERIOD, pConfig->infraStaKeepAlivePeriod, 
                      NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_INFRA_STA_KEEP_ALIVE_PERIOD configuration info to CCM\n"  );
     }
    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_DYNAMIC_PS_POLL_VALUE, pConfig->dynamicPsPollValue, 
                     NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
       fStatus = FALSE;
       hddLog(LOGE,"Failure: Could not pass on WNI_CFG_DYNAMIC_PS_POLL_VALUE configuration info to CCM\n"  );
    }

    if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_PS_NULLDATA_AP_RESP_TIMEOUT, pConfig->nNullDataApRespTimeout,
               NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
    {
       fStatus = FALSE;
       hddLog(LOGE,"Failure: Could not pass on WNI_CFG_PS_NULLDATA_DELAY_TIMEOUT configuration info to CCM\n"  );
    } 

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD, pConfig->apDataAvailPollPeriodInMs,
               NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
       fStatus = FALSE;
       hddLog(LOGE,"Failure: Could not pass on WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD configuration info to CCM\n"  );
   } 
#ifdef FEATURE_WLAN_INTEGRATED_SOC
    if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_FRAGMENTATION_THRESHOLD, pConfig->FragmentationThreshold, 
                   NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
    {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_FRAGMENTATION_THRESHOLD configuration info to CCM\n"  );
    }
    if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_RTS_THRESHOLD, pConfig->RTSThreshold, 
                        NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
    {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_RTS_THRESHOLD configuration info to CCM\n"  );
    }

    if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_11D_ENABLED, pConfig->Is11dSupportEnabled, 
                        NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
    {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_11D_ENABLED configuration info to CCM\n"  );
    }
    if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_HEART_BEAT_THRESHOLD, pConfig->HeartbeatThresh24, 
                        NULL, eANI_BOOLEAN_FALSE) == eHAL_STATUS_FAILURE)
    {
        fStatus = FALSE;
        hddLog(LOGE,"Failure: Could not pass on WNI_CFG_HEART_BEAT_THRESHOLD configuration info to CCM\n"  );
    }

#endif
   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD, pConfig->apDataAvailPollPeriodInMs,
               NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE,"Failure: Could not pass on WNI_CFG_AP_DATA_AVAIL_POLL_PERIOD configuration info to CCM\n"  );
   }

   if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_CLOSE_LOOP, 
                   pConfig->enableCloseLoop, NULL, eANI_BOOLEAN_FALSE)
       ==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_CLOSE_LOOP to CCM\n");
   }

   if(ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_TX_PWR_CTRL_ENABLE, 
                   pConfig->enableAutomaticTxPowerControl, NULL, eANI_BOOLEAN_FALSE)
                   ==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_TX_PWR_CTRL_ENABLE to CCM\n");
   }

   if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_SHORT_GI_40MHZ, 
      pConfig->ShortGI40MhzEnable, NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
   {
      fStatus = FALSE;
      hddLog(LOGE, "Could not pass on WNI_CFG_SHORT_GI_40MHZ to CCM\n");
   }


     if (ccmCfgSetInt(pHddCtx->hHal, WNI_CFG_ENABLE_MC_ADDR_LIST, pConfig->fEnableMCAddrList, 
        NULL, eANI_BOOLEAN_FALSE)==eHAL_STATUS_FAILURE)
     {
        fStatus = FALSE;
        hddLog(LOGE, "Could not pass on WNI_CFG_ENABLE_MC_ADDR_LIST to CCM\n");
     }

   return fStatus;
}


/**---------------------------------------------------------------------------

  \brief hdd_init_set_sme_config() - 

   This function initializes the sme configuration parameters
   
  \param  - pHddCtx - Pointer to the HDD Adapter.
              
  \return - 0 for success, non zero for failure
    
  --------------------------------------------------------------------------*/

VOS_STATUS hdd_set_sme_config( hdd_context_t *pHddCtx )
{
   VOS_STATUS status = VOS_STATUS_SUCCESS;
   eHalStatus halStatus;
   tSmeConfigParams smeConfig;

   hdd_config_t *pConfig = pHddCtx->cfg_ini;

   vos_mem_zero( &smeConfig, sizeof( smeConfig ) );

   VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO_HIGH, 
              "%s bWmmIsEnabled=%d 802_11e_enabled=%d dot11Mode=%d\n", __func__,
              pConfig->WmmMode, pConfig->b80211eIsEnabled, pConfig->dot11Mode);

   // Config params obtained from the registry
   smeConfig.csrConfig.RTSThreshold             = pConfig->RTSThreshold;
   smeConfig.csrConfig.FragmentationThreshold   = pConfig->FragmentationThreshold;
   smeConfig.csrConfig.shortSlotTime            = pConfig->ShortSlotTimeEnabled;
   smeConfig.csrConfig.Is11dSupportEnabled      = pConfig->Is11dSupportEnabled;
   smeConfig.csrConfig.HeartbeatThresh24        = pConfig->HeartbeatThresh24;

   smeConfig.csrConfig.phyMode                  = hdd_cfg_xlate_to_csr_phy_mode ( pConfig->dot11Mode );

   smeConfig.csrConfig.channelBondingMode24GHz  = pConfig->nChannelBondingMode24GHz;
   smeConfig.csrConfig.channelBondingMode5GHz   = pConfig->nChannelBondingMode5GHz;
   smeConfig.csrConfig.TxRate                   = pConfig->TxRate;
   smeConfig.csrConfig.nScanResultAgeCount      = pConfig->ScanResultAgeCount;
   smeConfig.csrConfig.scanAgeTimeNCNPS         = pConfig->nScanAgeTimeNCNPS;
   smeConfig.csrConfig.scanAgeTimeNCPS          = pConfig->nScanAgeTimeNCPS;
   smeConfig.csrConfig.scanAgeTimeCNPS          = pConfig->nScanAgeTimeCNPS;
   smeConfig.csrConfig.scanAgeTimeCPS           = pConfig->nScanAgeTimeCPS;
   smeConfig.csrConfig.AdHocChannel24           = pConfig->OperatingChannel;
   smeConfig.csrConfig.fEnforce11dChannels      = pConfig->fEnforce11dChannels;
   smeConfig.csrConfig.fSupplicantCountryCodeHasPriority     = pConfig->fSupplicantCountryCodeHasPriority;
   smeConfig.csrConfig.fEnforceCountryCodeMatch = pConfig->fEnforceCountryCodeMatch;
   smeConfig.csrConfig.fEnforceDefaultDomain    = pConfig->fEnforceDefaultDomain;
   smeConfig.csrConfig.bCatRssiOffset           = pConfig->nRssiCatGap;
   smeConfig.csrConfig.vccRssiThreshold         = pConfig->nVccRssiTrigger;
   smeConfig.csrConfig.vccUlMacLossThreshold    = pConfig->nVccUlMacLossThreshold;
   smeConfig.csrConfig.nRoamingTime             = pConfig->nRoamingTime;
   smeConfig.csrConfig.IsIdleScanEnabled        = pConfig->nEnableIdleScan; 
   smeConfig.csrConfig.nActiveMaxChnTime        = pConfig->nActiveMaxChnTime;
   smeConfig.csrConfig.nActiveMinChnTime        = pConfig->nActiveMinChnTime;
   smeConfig.csrConfig.nPassiveMaxChnTime       = pConfig->nPassiveMaxChnTime;
   smeConfig.csrConfig.nPassiveMinChnTime       = pConfig->nPassiveMinChnTime;
   smeConfig.csrConfig.Is11eSupportEnabled      = pConfig->b80211eIsEnabled;
   smeConfig.csrConfig.WMMSupportMode           = pConfig->WmmMode;

#if defined WLAN_FEATURE_VOWIFI
   smeConfig.rrmConfig.rrmEnabled = pConfig->fRrmEnable;
   smeConfig.rrmConfig.maxRandnInterval = pConfig->nRrmRandnIntvl;
#endif
   //Remaining config params not obtained from registry
   // On RF EVB beacon using channel 1.
   
   smeConfig.csrConfig.AdHocChannel5G            = 44; 
   smeConfig.csrConfig.ProprietaryRatesEnabled   = 0;  
   smeConfig.csrConfig.HeartbeatThresh50         = 40; 
   if( smeConfig.csrConfig.Is11dSupportEnabled )
   {
      smeConfig.csrConfig.Is11hSupportEnabled    = 1;
   }
   else
   {
      smeConfig.csrConfig.Is11hSupportEnabled    = 0;
   }
   smeConfig.csrConfig.bandCapability            = pConfig->nBandCapability; 
   smeConfig.csrConfig.cbChoice                  = 0;   
   smeConfig.csrConfig.bgScanInterval            = 0; 
   smeConfig.csrConfig.eBand                     = pConfig->nBandCapability; 
   smeConfig.csrConfig.nTxPowerCap = pConfig->nTxPowerCap;
   smeConfig.csrConfig.fEnableBypass11d          = pConfig->enableBypass11d;
   smeConfig.csrConfig.fEnableDFSChnlScan        = pConfig->enableDFSChnlScan;
   
   //FIXME 11d config is hardcoded
#ifdef WLAN_SOFTAP_FEATURE
   if ( VOS_STA_SAP_MODE != hdd_get_conparam()){
#endif
   smeConfig.csrConfig.Csr11dinfo.Channels.numChannels = 0;

   //if there is a requirement that HDD will control the default channel list & 
   //country code (say from .ini file) we need to add some logic here. Otherwise 
   //the default 11d info should come from NV as per our current implementation
   
#ifdef WLAN_SOFTAP_FEATURE
   }
   else{

    VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
               "AP country Code %s", pConfig->apCntryCode);

    if (memcmp(pConfig->apCntryCode, CFG_AP_COUNTRY_CODE_DEFAULT, 3) != 0)
       sme_setRegInfo(pHddCtx->hHal, pConfig->apCntryCode);
       sme_set11dinfo(pHddCtx->hHal, &smeConfig);
    }
#endif
   hdd_set_power_save_config(pHddCtx, &smeConfig);
   hdd_set_btc_config(pHddCtx);

#ifdef WLAN_FEATURE_VOWIFI_11R   
   smeConfig.csrConfig.csr11rConfig.IsFTResourceReqSupported = pConfig->fFTResourceReqSupported;
#endif
#ifdef FEATURE_WLAN_LFR
   smeConfig.csrConfig.isFastRoamIniFeatureEnabled = pConfig->isFastRoamIniFeatureEnabled;
#endif
#ifdef FEATURE_WLAN_CCX
   smeConfig.csrConfig.isCcxIniFeatureEnabled = pConfig->isCcxIniFeatureEnabled;
#endif
#if  defined (WLAN_FEATURE_VOWIFI_11R) || defined (FEATURE_WLAN_CCX) || defined(FEATURE_WLAN_LFR)
   smeConfig.csrConfig.isFastTransitionEnabled = pConfig->isFastTransitionEnabled;
   smeConfig.csrConfig.RoamRssiDiff = pConfig->RoamRssiDiff;
#endif

#ifdef WLAN_FEATURE_NEIGHBOR_ROAMING
   smeConfig.csrConfig.neighborRoamConfig.nNeighborReassocRssiThreshold = pConfig->nNeighborReassocRssiThreshold;
   smeConfig.csrConfig.neighborRoamConfig.nNeighborLookupRssiThreshold = pConfig->nNeighborLookupRssiThreshold;
   smeConfig.csrConfig.neighborRoamConfig.nNeighborScanMaxChanTime = pConfig->nNeighborScanMaxChanTime; 
   smeConfig.csrConfig.neighborRoamConfig.nNeighborScanMinChanTime = pConfig->nNeighborScanMinChanTime; 
   smeConfig.csrConfig.neighborRoamConfig.nNeighborScanTimerPeriod = pConfig->nNeighborScanPeriod;
   smeConfig.csrConfig.neighborRoamConfig.nMaxNeighborRetries = pConfig->nMaxNeighborReqTries; 
   smeConfig.csrConfig.neighborRoamConfig.nNeighborResultsRefreshPeriod = pConfig->nNeighborResultsRefreshPeriod;
   hdd_string_to_u8_array( pConfig->neighborScanChanList, 
                                        smeConfig.csrConfig.neighborRoamConfig.neighborScanChanList.channelList, 
                                        &smeConfig.csrConfig.neighborRoamConfig.neighborScanChanList.numChannels, 
                                        WNI_CFG_VALID_CHANNEL_LIST_LEN );
#endif

   smeConfig.csrConfig.addTSWhenACMIsOff = pConfig->AddTSWhenACMIsOff;
   smeConfig.csrConfig.fValidateList = pConfig->fValidateScanList;

   //Enable/Disable MCC 
   smeConfig.csrConfig.fEnableMCCMode = pConfig->enableMCC;

   halStatus = sme_UpdateConfig( pHddCtx->hHal, &smeConfig);    
   if ( !HAL_STATUS_SUCCESS( halStatus ) )
   {
      status = VOS_STATUS_E_FAILURE;
   }

   
   return status;   
}


/**---------------------------------------------------------------------------

  \brief hdd_execute_config_command() -

   This function executes an arbitrary configuration set command

  \param - pHddCtx - Pointer to the HDD Adapter.
  \parmm - command - a configuration command of the form:
                     <name>=<value>

  \return - 0 for success, non zero for failure

  --------------------------------------------------------------------------*/

VOS_STATUS hdd_execute_config_command(hdd_context_t *pHddCtx, char *command)
{
   size_t tableSize = sizeof(g_registry_table)/sizeof(g_registry_table[0]);
   REG_TABLE_ENTRY *pRegEntry;
   char *clone;
   char *pCmd;
   void *pField;
   char *name;
   char *value_str;
   v_U32_t value;
   v_S31_t svalue;
   size_t len_value_str;
   unsigned int idx;
   unsigned int i;
   VOS_STATUS vstatus;

   // assume failure until proven otherwise
   vstatus = VOS_STATUS_E_FAILURE;

   // clone the command so that we can manipulate it
   clone = kstrdup(command, GFP_ATOMIC);
   if (NULL == clone)
   {
      hddLog(LOGE, "%s: memory allocation failure, unable to process [%s]",
             __FUNCTION__, command);
      return vstatus;
   }

   // 'clone' will point to the beginning of the string so it can be freed
   // 'pCmd' will be used to walk/parse the command
   pCmd = clone;

   // get rid of leading/trailing whitespace
   pCmd = i_trim(pCmd);
   if ('\0' == *pCmd)
   {
      // only whitespace
      hddLog(LOGE, "%s: invalid command, only whitespace:[%s]",
             __FUNCTION__, command);
      goto done;
   }

   // parse the <name> = <value>
   name = pCmd;
   while (('=' != *pCmd) && ('\0' != *pCmd))
   {
      pCmd++;
   }
   if ('\0' == *pCmd)
   {
      // did not find '='
      hddLog(LOGE, "%s: invalid command, no '=':[%s]",
             __FUNCTION__, command);
      goto done;
   }

   // replace '=' with NUL to terminate the <name>
   *pCmd++ = '\0';
   name = i_trim(name);
   if ('\0' == *name)
   {
      // did not find a name
      hddLog(LOGE, "%s: invalid command, no <name>:[%s]",
             __FUNCTION__, command);
      goto done;
   }

   value_str = i_trim(pCmd);
   if ('\0' == *value_str)
   {
      // did not find a value
      hddLog(LOGE, "%s: invalid command, no <value>:[%s]",
             __FUNCTION__, command);
      goto done;
   }

   // lookup the configuration item
   for (idx = 0; idx < tableSize; idx++)
   {
      if (0 == strcmp(name, g_registry_table[idx].RegName))
      {
         // found a match
         break;
      }
   }
   if (tableSize == idx)
   {
      // did not match the name
      hddLog(LOGE, "%s: invalid command, unknown configuration item:[%s]",
             __FUNCTION__, command);
      goto done;
   }

   pRegEntry = &g_registry_table[idx];
   if (!(pRegEntry->Flags & VAR_FLAGS_DYNAMIC_CFG))
   {
      // does not support dynamic configuration
      hddLog(LOGE, "%s: invalid command, %s does not support "
             "dynamic configuration", __FUNCTION__, name);
      goto done;
   }

   pField = ((v_U8_t *)pHddCtx->cfg_ini) + pRegEntry->VarOffset;

   switch (pRegEntry->RegType)
   {
   case WLAN_PARAM_Integer:
      value = simple_strtoul(value_str, NULL, 10);
      if (value < pRegEntry->VarMin)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %u < min value %u",
                __FUNCTION__, value, pRegEntry->VarMin);
         goto done;
      }
      if (value > pRegEntry->VarMax)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %u > max value %u",
                __FUNCTION__, value, pRegEntry->VarMax);
         goto done;
      }
      memcpy(pField, &value, pRegEntry->VarSize);
      break;

   case WLAN_PARAM_HexInteger:
      value = simple_strtoul(value_str, NULL, 16);
      if (value < pRegEntry->VarMin)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %x < min value %x",
                __FUNCTION__, value, pRegEntry->VarMin);
         goto done;
      }
      if (value > pRegEntry->VarMax)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %x > max value %x",
                __FUNCTION__, value, pRegEntry->VarMax);
         goto done;
      }
      memcpy(pField, &value, pRegEntry->VarSize);
      break;

   case WLAN_PARAM_SignedInteger:
      svalue = simple_strtol(value_str, NULL, 10);
      if (svalue < (v_S31_t)pRegEntry->VarMin)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %d < min value %d",
                __FUNCTION__, svalue, (int)pRegEntry->VarMin);
         goto done;
      }
      if (svalue > (v_S31_t)pRegEntry->VarMax)
      {
         // out of range
         hddLog(LOGE, "%s: invalid command, value %d > max value %d",
                __FUNCTION__, svalue, (int)pRegEntry->VarMax);
         goto done;
      }
      memcpy(pField, &svalue, pRegEntry->VarSize);
      break;

   case WLAN_PARAM_String:
      len_value_str = strlen(value_str);
      if (len_value_str > (pRegEntry->VarSize - 1))
      {
         // too big
         hddLog(LOGE,
                "%s: invalid command, string [%s] length "
                "%u exceeds maximum length %u",
                __FUNCTION__, value_str,
                len_value_str, (pRegEntry->VarSize - 1));
         goto done;
      }
      // copy string plus NUL
      memcpy(pField, value_str, (len_value_str + 1));
      break;

   case WLAN_PARAM_MacAddr:
      len_value_str = strlen(value_str);
      if (len_value_str != (VOS_MAC_ADDR_SIZE * 2))
      {
         // out of range
         hddLog(LOGE,
                "%s: invalid command, MAC address [%s] length "
                "%u is not expected length %u",
                __FUNCTION__, value_str,
                len_value_str, (VOS_MAC_ADDR_SIZE * 2));
         goto done;
      }
      //parse the string and store it in the byte array
      for (i = 0; i < VOS_MAC_ADDR_SIZE; i++)
      {
         ((char*)pField)[i] = (char)
            ((parseHexDigit(value_str[(i * 2)]) * 16) +
             parseHexDigit(value_str[(i * 2) + 1]));
      }
      break;

   default:
      goto done;
   }

   // if we get here, we had a successful modification
   vstatus = VOS_STATUS_SUCCESS;

   // config table has been modified, is there a notifier?
   if (NULL != pRegEntry->pfnDynamicNotify)
   {
      (pRegEntry->pfnDynamicNotify)(pHddCtx, pRegEntry->NotifyId);
   }

   // note that this item was explicitly configured
   if (idx < MAX_CFG_INI_ITEMS)
   {
      set_bit(idx, (void *)&pHddCtx->cfg_ini->bExplicitCfg);
   }
 done:
   kfree(clone);
   return vstatus;
}
