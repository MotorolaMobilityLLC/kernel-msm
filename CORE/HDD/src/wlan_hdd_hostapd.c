/*
 * Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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
 * Copyright (c) 2012, The Linux Foundation. All rights reserved.
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

/**========================================================================
  
  \file  wlan_hdd_hostapd.c
  \brief WLAN Host Device Driver implementation
               
   Copyright 2008 (c) Qualcomm, Incorporated.  All Rights Reserved.
   
   Qualcomm Confidential and Proprietary.
  
  ========================================================================*/
/**========================================================================= 
                       EDIT HISTORY FOR FILE 
   
   
  This section contains comments describing changes made to the module. 
  Notice that changes are listed in reverse chronological order. 
   
  $Header:$   $DateTime: $ $Author: $ 
   
   
  when        who    what, where, why 
  --------    ---    --------------------------------------------------------
  04/5/09     Shailender     Created module. 
  06/03/10    js - Added support to hostapd driven deauth/disassoc/mic failure
  ==========================================================================*/
/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
   
#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/wireless.h>
#include <linux/semaphore.h>
#include <vos_api.h>
#include <vos_sched.h>
#include <linux/etherdevice.h>
#include <wlan_hdd_includes.h>
#include <qc_sap_ioctl.h>
#include <wlan_hdd_hostapd.h>
#include <sapApi.h>
#include <sapInternal.h>
#include <wlan_qct_tl.h>
#include <wlan_hdd_softap_tx_rx.h>
#include <wlan_hdd_main.h>
#include <linux/netdevice.h>
#include <linux/mmc/sdio_func.h>
#include "wlan_nlink_common.h"
#include "wlan_btc_svc.h"
#include <bap_hdd_main.h>
#include "wlan_hdd_p2p.h"
#include "cfgApi.h"
#include "wniCfgAp.h"

#define    IS_UP(_dev) \
    (((_dev)->flags & (IFF_RUNNING|IFF_UP)) == (IFF_RUNNING|IFF_UP))
#define    IS_UP_AUTO(_ic) \
    (IS_UP((_ic)->ic_dev) && (_ic)->ic_roaming == IEEE80211_ROAMING_AUTO)
#define WE_WLAN_VERSION     1
#define STATS_CONTEXT_MAGIC 0x53544154
#define WE_GET_STA_INFO_SIZE 30
/* WEXT limition: MAX allowed buf len for any *
 * IW_PRIV_TYPE_CHAR is 2Kbytes *
 */
#define WE_SAP_MAX_STA_INFO 0x7FF

struct statsContext
{
   struct completion completion;
   hdd_adapter_t *pAdapter;
   unsigned int magic;
};
#define SAP_24GHZ_CH_COUNT (14) 

#define SAP_MAX_GET_ASSOC_STAS_TIMEOUT    500
/* Max possible supported rate count
 * Legacy 14 + 11N MCS 8 + 11AC MCS 10 */
#define SAP_MAX_SUPPORTED_RATE_COUNT      32
#define SAP_LEGACY_RATE_MASK              0x007F
#define SAP_GET_STAS_RATE_TIMEOUT         1000
#define SAP_AC_MCS_MAP_MASK               0x03
#define SAP_AC_MCS_MAP_OFFSET             7

#define SAP_LEGACY_RATE_COUNT             SIR_NUM_11B_RATES + SIR_NUM_11A_RATES
#define SAP_11N_RATE_COUNT                8

#define SAP_RATE_SUPPORT_MAP_LEGACY_MASK  0x0001
#define SAP_RATE_SUPPORT_MAP_N_MASK       0x001E
#define SAP_RATE_SUPPORT_MAP_AC_MASK      0x07E0

#define SAP_MAX_24_CHANNEL_NUMBER         14
#define SAP_GET_STAS_COOKIE               0xC000C1EE

/* Temp put here, will locate correct location
 * work on progress with UMAC */
/* Should syn with FW definition */
typedef enum
{
   WNI_CFG_FIXED_RATE_SAP_AUTO,
   WNI_CFG_FIXED_RATE_11B_LONG_1_MBPS,
   WNI_CFG_FIXED_RATE_11B_LONG_2_MBPS,
   WNI_CFG_FIXED_RATE_11B_LONG_5_5_MBPS,
   WNI_CFG_FIXED_RATE_11B_LONG_11_MBPS,
   WNI_CFG_FIXED_RATE_11A_6_MBPS,
   WNI_CFG_FIXED_RATE_11A_9_MBPS,
   WNI_CFG_FIXED_RATE_11A_12_MBPS,
   WNI_CFG_FIXED_RATE_11A_18_MBPS,
   WNI_CFG_FIXED_RATE_11A_24_MBPS,
   WNI_CFG_FIXED_RATE_11A_36_MBPS,
   WNI_CFG_FIXED_RATE_11A_48_MBPS,
   WNI_CFG_FIXED_RATE_11A_54_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_6_5_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_13_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_19_5_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_26_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_39_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_52_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_58_5_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_65_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_65_MBPS,
   WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_72_2_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_13_5_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_27_MBPS,                     /* 30 */
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_40_5_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_54_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_81_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_108_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_121_5_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_135_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_15_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_30_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_45_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_60_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_90_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_120_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_135_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_150_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_GF_13_5_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_GF_27_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_GF_40_5_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_GF_54_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_GF_81_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_GF_108_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_GF_121_5_MBPS,
   WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_GF_135_MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_NGI_6_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_NGI_13MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_NGI_19_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_NGI_26MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_NGI_39MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_NGI_52MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_NGI_58_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_NGI_65MBPS,                        /* 60 */
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_NGI_78MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_NGI_86_5_MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_SGI_21_667MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_SGI_28_889MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_SGI_43_333MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_SGI_57_778MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_SGI_65MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_SGI_72_222MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_SGI_86_667MBPS,
   WNI_CFG_FIXED_RATE_VHT_SIMO_CB_SGI_96_1_MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_13_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_27MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_40_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_54MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_81MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_108MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_121_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_135MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_RESERVED,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_162MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_180MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_15MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_30MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_45MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_60MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_90MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_120MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_135MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_150MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_RESERVED,                /* 90 */
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_180MBPS,
   WNI_CFG_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_200MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_29_25MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_58_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_87_75MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_117MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_175_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_234MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_263_25MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_292_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_RESERVED,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_351MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_390MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_32_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_65MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_97_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_130MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_195MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_260MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_292_5MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_325MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_RESERVED,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_390MBPS,
   WNI_CFG_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_433_33MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_6_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_13MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_19_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_26MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_39MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_52MBPS,                   /* 120 */
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_58_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_65MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_78MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_86_5_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_7_2222MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_14_444MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_21_667MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_28_889MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_43_333MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_57_778MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_65MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_72_222MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_86_667MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_96_1_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_13_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_27MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_40_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_54MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_81MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_108MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_121_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_135MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_RESERVED,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_162MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_180MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_15MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_30MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_45MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_60MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_90MBPS,             /* 150 */
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_120MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_135MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_150MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_RESERVED,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_180MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_200MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_29_25MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_58_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_87_75MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_117MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_175_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_234MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_263_25MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_292_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_RESERVED,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_351MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_390MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_32_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_65MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_97_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_130MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_195MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_260MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_292_5MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_325MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_RESERVED,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_390MBPS,
   WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_433_33MBPS,
   WNI_CFG_FIXED_RATE_11A_DUP_6_MBPS,
   WNI_CFG_FIXED_RATE_11A_DUP_9_MBPS,                                /* 180 */
   WNI_CFG_FIXED_RATE_11A_DUP_12_MBPS,
   WNI_CFG_FIXED_RATE_11A_DUP_18_MBPS,
   WNI_CFG_FIXED_RATE_11A_DUP_24_MBPS,
   WNI_CFG_FIXED_RATE_11A_DUP_36_MBPS,
   WNI_CFG_FIXED_RATE_11A_DUP_48_MBPS,
   WNI_CFG_FIXED_RATE_11A_DUP_54_MBPS,
   WNI_CFG_FIXED_RATE_11A_80MHZ_DUP_6_MBPS,
   WNI_CFG_FIXED_RATE_11A_80MHZDUP_9_MBPS,
   WNI_CFG_FIXED_RATE_11A_80MHZ_DUP_12_MBPS,
   WNI_CFG_FIXED_RATE_11A_80MHZ_DUP_18_MBPS,
   WNI_CFG_FIXED_RATE_11A_80MHZ_DUP_24_MBPS,
   WNI_CFG_FIXED_RATE_11A_80MHZ_DUP_36_MBPS,
   WNI_CFG_FIXED_RATE_11A_80MHZ_DUP_48_MBPS,
   WNI_CFG_FIXED_RATE_11A_80MHZ_DUP_54_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_6_5_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_13_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_19_5_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_26_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_39_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_52_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_58_5_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_65_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_SG_7_2_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_SG_14_4_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_SG_21_7_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_SG_28_9_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_SG_43_3_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_SG_57_8_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_SG_65_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_1NSS_MM_SG_72_2_MBPS,                 /* 210 */
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_13_5_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_27_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_40_5_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_54_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_81_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_108_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_121_5_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_135_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_15_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_30_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_45_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_60_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_90_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_120_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_135_MBPS,
   WNI_CFG_LDPC_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_150_MBPS
} eCfgFixedRateCfgType;

/* Legacy IDX based rate table */
typedef struct
{
   v_U16_t   legacy_rate_index;
   v_U32_t   legacy_rate;
} supported_legacy_rate_t;
static const supported_legacy_rate_t legacy_rate[] =
{
/* IDX   Rate, 100kbps */
   {2,   10},
   {4,   20},
   {11,  55},
   {12,  60},
   {18,  90},
   {24,  120},
   {36,  180},
   {48,  240},
   {66,  330},
   {72,  360},
   {96,  480},
   {108, 540}
};

/* 11N MCS based rate table */
typedef struct
{
   v_U8_t   mcs_index_11n;
   v_U32_t  rate_11n[4];
} supported_11n_rate_t;
static const supported_11n_rate_t mcs_rate_11n[] =
{
/* MCS  L20   L40   S20  S40 */
   {0,  {65,  135,  72,  150}},
   {1,  {130, 270,  144, 300}},
   {2,  {195, 405,  217, 450}},
   {3,  {260, 540,  289, 600}},
   {4,  {390, 810,  433, 900}},
   {5,  {520, 1080, 578, 1200}},
   {6,  {585, 1215, 650, 1350}},
   {7,  {650, 1350, 722, 1500}}
};

/* 11AC MCS based rate table */
typedef struct
{
   v_U8_t   mcs_index_11ac;
   v_U16_t  cb80_rate_11ac[2];
   v_U16_t  cb40_rate_11ac[2];
   v_U16_t  cb20_rate_11ac[2];
} supported_11ac_rate_t;
static const supported_11ac_rate_t mcs_rate_11ac[] =
{
/* MCS  L80    S80     L40   S40    L20   S40*/
   {0,  {293,  325},  {135,  150},  {65,   72}},
   {1,  {585,  650},  {270,  300},  {130,  144}},
   {2,  {878,  975},  {405,  450},  {195,  217}},
   {3,  {1170, 1300}, {540,  600},  {260,  289}},
   {4,  {1755, 1950}, {810,  900},  {390,  433}},
   {5,  {2340, 2600}, {1080, 1200}, {520,  578}},
   {6,  {2633, 2925}, {1215, 1350}, {585,  650}},
   {7,  {2925, 3250}, {1350, 1500}, {650,  722}},
   {8,  {3510, 3900}, {1620, 1800}, {780,  867}},
   {9,  {3900, 4333}, {1800, 2000}, {860,  867}}
};

typedef struct
{
   eCfgFixedRateCfgType  eRateCfg;
   v_U16_t               rate;
} rate_cfg_item_mapping_t;

static rate_cfg_item_mapping_t legacy_rate_mapping[] =
{
   {WNI_CFG_FIXED_RATE_11B_LONG_1_MBPS,                               10},
   {WNI_CFG_FIXED_RATE_11B_LONG_2_MBPS,                               20},
   {WNI_CFG_FIXED_RATE_11B_LONG_5_5_MBPS,                             55},
   {WNI_CFG_FIXED_RATE_11B_LONG_11_MBPS,                              110},
   {WNI_CFG_FIXED_RATE_11A_6_MBPS,                                    60},
   {WNI_CFG_FIXED_RATE_11A_9_MBPS,                                    90},
   {WNI_CFG_FIXED_RATE_11A_12_MBPS,                                   120},
   {WNI_CFG_FIXED_RATE_11A_18_MBPS,                                   180},
   {WNI_CFG_FIXED_RATE_11A_24_MBPS,                                   240},
   {WNI_CFG_FIXED_RATE_11A_36_MBPS,                                   360},
   {WNI_CFG_FIXED_RATE_11A_48_MBPS,                                   480},
   {WNI_CFG_FIXED_RATE_11A_54_MBPS,                                   540}
};
static rate_cfg_item_mapping_t n_l20_rate_mapping[] =
{
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_6_5_MBPS,                          65},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_13_MBPS,                           130},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_19_5_MBPS,                         195},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_26_MBPS,                           260},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_39_MBPS,                           390},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_52_MBPS,                           520},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_58_5_MBPS,                         585},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_65_MBPS,                           650}
};
static rate_cfg_item_mapping_t n_s20_rate_mapping[] =
{
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_7_2_MBPS,                       72},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_14_4_MBPS,                      144},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_21_7_MBPS,                      217},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_28_9_MBPS,                      289},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_43_3_MBPS,                      433},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_57_8_MBPS,                      578},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_65_MBPS,                        650},
   {WNI_CFG_FIXED_RATE_MCS_1NSS_MM_SG_72_2_MBPS,                      722}
};
static rate_cfg_item_mapping_t n_l40_rate_mapping[] =
{
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_13_5_MBPS,                   135},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_27_MBPS,                     270},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_40_5_MBPS,                   405},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_54_MBPS,                     540},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_81_MBPS,                     810},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_108_MBPS,                    1080},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_121_5_MBPS,                  1215},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_135_MBPS,                    1350}
};
static rate_cfg_item_mapping_t n_s40_rate_mapping[] =
{
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_15_MBPS,                  150},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_30_MBPS,                  300},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_45_MBPS,                  450},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_60_MBPS,                  600},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_90_MBPS,                  900},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_120_MBPS,                 1200},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_135_MBPS,                 1350},
   {WNI_CFG_FIXED_RATE_MCS_40MHZ_1NSS_MM_SG_150_MBPS,                 1500}
};

#ifdef WLAN_FEATURE_11AC
static rate_cfg_item_mapping_t ac_l20_rate_mapping[] =
{
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_6_5MBPS,                  65},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_13MBPS,                   130},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_19_5MBPS,                 195},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_26MBPS,                   260},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_39MBPS,                   390},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_52MBPS,                   520},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_58_5MBPS,                 585},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_65MBPS,                   650},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_78MBPS,                   780},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_NGI_86_5_MBPS,                865}
};
static rate_cfg_item_mapping_t ac_s20_rate_mapping[] =
{
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_7_2222MBPS,               72},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_14_444MBPS,               144},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_21_667MBPS,               217},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_28_889MBPS,               289},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_43_333MBPS,               433},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_57_778MBPS,               578},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_65MBPS,                   650},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_72_222MBPS,               722},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_86_667MBPS,               867},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_SIMO_CB_SGI_96_1_MBPS,                961}
};
static rate_cfg_item_mapping_t ac_l40_rate_mapping[] =
{
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_13_5MBPS,           135},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_27MBPS,             270},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_40_5MBPS,           405},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_54MBPS,             540},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_81MBPS,             810},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_108MBPS,            1080},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_121_5MBPS,          1215},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_135MBPS,            1350},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_162MBPS,            1620},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_NGI_180MBPS,            1800}
};
static rate_cfg_item_mapping_t ac_s40_rate_mapping[] =
{
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_15MBPS,             150},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_30MBPS,             300},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_45MBPS,             450},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_60MBPS,             600},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_90MBPS,             900},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_120MBPS,            1200},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_135MBPS,            1350},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_150MBPS,            1500},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_180MBPS,            1800},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_40MHZ_SIMO_CB_SGI_200MBPS,            2000}
};
static rate_cfg_item_mapping_t ac_l80_rate_mapping[] =
{
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_29_25MBPS,          293},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_58_5MBPS,           585},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_87_75MBPS,          878},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_117MBPS,            1170},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_175_5MBPS,          1755},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_234MBPS,            2340},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_263_25MBPS,         2633},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_292_5MBPS,          2925},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_351MBPS,            3510},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_NGI_390MBPS,            3900}
};
static rate_cfg_item_mapping_t ac_s80_rate_mapping[] =
{
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_32_5MBPS,           325},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_65MBPS,             650},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_97_5MBPS,           975},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_130MBPS,            1300},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_195MBPS,            1950},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_260MBPS,            2600},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_292_5MBPS,          2925},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_325MBPS,            3250},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_390MBPS,            3900},
   {WNI_CFG_LDPC_FIXED_RATE_VHT_80MHZ_SIMO_CB_SGI_433_33MBPS,         4333}
};
#endif /* WLAN_FEATURE_11AC */

typedef enum
{
   RATE_CFG_RATE_LEGACY,
   RATE_CFG_RATE_11N_MCS_LGI_20,
   RATE_CFG_RATE_11N_MCS_SGI_20,
   RATE_CFG_RATE_11N_MCS_LGI_40,
   RATE_CFG_RATE_11N_MCS_SGI_40,
   RATE_CFG_RATE_11AC_MCS_LGI_20,
   RATE_CFG_RATE_11AC_MCS_SGI_20,
   RATE_CFG_RATE_11AC_MCS_LGI_40,
   RATE_CFG_RATE_11AC_MCS_SGI_40,
   RATE_CFG_RATE_11AC_MCS_LGI_80,
   RATE_CFG_RATE_11AC_MCS_SGI_80
} rate_cfg_supported_rate_t;

typedef enum
{
   RATE_CFG_RATE_11AC_MAX_MCS_7,
   RATE_CFG_RATE_11AC_MAX_MCS_8,
   RATE_CFG_RATE_11AC_MAX_MCS_9
} rate_cfg_11ac_max_mcs_t;

typedef enum
{
   RATE_CFG_RATE_BW_20,
   RATE_CFG_RATE_BW_40,
   RATE_CFG_RATE_BW_80
} rate_cfg_supported_bw_t;

typedef enum
{
   RATE_CFG_RATE_GI_LONG,
   RATE_CFG_RATE_GI_SHORT
} rate_cfg_gi_t;

/*--------------------------------------------------------------------------- 
 *   Function definitions
 *-------------------------------------------------------------------------*/
/**---------------------------------------------------------------------------
  
  \brief hdd_hostapd_open() - HDD Open function for hostapd interface
  
  This is called in response to ifconfig up
  
  \param  - dev Pointer to net_device structure
  
  \return - 0 for success non-zero for failure
              
  --------------------------------------------------------------------------*/
int hdd_hostapd_open (struct net_device *dev)
{
   ENTER();

   //Turn ON carrier state
   netif_carrier_on(dev);
   //Enable all Tx queues  
   netif_tx_start_all_queues(dev);
   
   EXIT();
   return 0;
}
/**---------------------------------------------------------------------------
  
  \brief hdd_hostapd_stop() - HDD stop function for hostapd interface
  
  This is called in response to ifconfig down
  
  \param  - dev Pointer to net_device structure
  
  \return - 0 for success non-zero for failure
              
  --------------------------------------------------------------------------*/
int hdd_hostapd_stop (struct net_device *dev)
{
   ENTER();

   //Stop all tx queues
   netif_tx_disable(dev);
   
   //Turn OFF carrier state
   netif_carrier_off(dev);

   EXIT();
   return 0;
}
/**---------------------------------------------------------------------------

  \brief hdd_hostapd_uninit() - HDD uninit function

  This is called during the netdev unregister to uninitialize all data
associated with the device

  \param  - dev Pointer to net_device structure

  \return - void

  --------------------------------------------------------------------------*/
static void hdd_hostapd_uninit (struct net_device *dev)
{
   hdd_adapter_t *pHostapdAdapter = netdev_priv(dev);

   ENTER();

   if (pHostapdAdapter && pHostapdAdapter->pHddCtx)
   {
      hdd_deinit_adapter(pHostapdAdapter->pHddCtx, pHostapdAdapter);

      /* after uninit our adapter structure will no longer be valid */
      pHostapdAdapter->dev = NULL;
   }

   EXIT();
}


/**============================================================================
  @brief hdd_hostapd_hard_start_xmit() - Function registered with the Linux OS for 
  transmitting packets. There are 2 versions of this function. One that uses
  locked queue and other that uses lockless queues. Both have been retained to
  do some performance testing
  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param dev      : [in] pointer to Libra network device
  
  @return         : NET_XMIT_DROP if packets are dropped
                  : NET_XMIT_SUCCESS if packet is enqueued succesfully
  ===========================================================================*/
int hdd_hostapd_hard_start_xmit(struct sk_buff *skb, struct net_device *dev)
{
    return 0;    
}
int hdd_hostapd_change_mtu(struct net_device *dev, int new_mtu)
{
    return 0;
}

int hdd_hostapd_ioctl(struct net_device *dev, struct ifreq *ifr, int cmd)
{
    hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
    hdd_priv_data_t priv_data;
    tANI_U8 *command = NULL;
    int ret = 0;

    if (NULL == pAdapter)
    {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
          "%s: HDD adapter context is Null", __func__);
       ret = -ENODEV;
       goto exit;
    }

    if ((!ifr) || (!ifr->ifr_data))
    {
        ret = -EINVAL;
        goto exit;
    }

    if (copy_from_user(&priv_data, ifr->ifr_data, sizeof(hdd_priv_data_t)))
    {
        ret = -EFAULT;
        goto exit;
    }

    command = kmalloc(priv_data.total_len, GFP_KERNEL);
    if (!command)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
           "%s: failed to allocate memory\n", __func__);
        ret = -ENOMEM;
        goto exit;
    }

    if (copy_from_user(command, priv_data.buf, priv_data.total_len))
    {
        ret = -EFAULT;
        goto exit;
    }

    if ((SIOCDEVPRIVATE + 1) == cmd)
    {
        VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
           "***HOSTAPD*** : Received %s cmd from Wi-Fi GUI***", command);

        if(strncmp(command, "P2P_SET_NOA", 11) == 0 )   
        {
            hdd_setP2pNoa(dev, command);
        }
        else if( strncmp(command, "P2P_SET_PS", 10) == 0 )
        {
            hdd_setP2pOpps(dev, command);
        }

        /*
           command should be a string having format
           SET_SAP_CHANNEL_LIST <num of channels> <the channels seperated by spaces>
        */
        if(strncmp(command, "SET_SAP_CHANNEL_LIST", 20) == 0)
        {
            VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                       " Received Command to Set Preferred Channels for SAP in %s", __func__);

            ret = sapSetPreferredChannel(command);
        }
    }
exit:
   if (command)
   {
       kfree(command);
   }
   return ret;
}

/**---------------------------------------------------------------------------
  
  \brief hdd_hostapd_set_mac_address() - 
   This function sets the user specified mac address using 
   the command ifconfig wlanX hw ether <mac adress>.
   
  \param  - dev - Pointer to the net device.
              - addr - Pointer to the sockaddr.
  \return - 0 for success, non zero for failure
  
  --------------------------------------------------------------------------*/

static int hdd_hostapd_set_mac_address(struct net_device *dev, void *addr)
{
   struct sockaddr *psta_mac_addr = addr;
   ENTER();
   memcpy(dev->dev_addr, psta_mac_addr->sa_data, ETH_ALEN);
   EXIT();
   return 0;
}
void hdd_hostapd_inactivity_timer_cb(v_PVOID_t usrDataForCallback)
{
    struct net_device *dev = (struct net_device *)usrDataForCallback;
    v_BYTE_t we_custom_event[64];
    union iwreq_data wrqu;
#ifdef DISABLE_CONCURRENCY_AUTOSAVE    
    VOS_STATUS vos_status;
    hdd_adapter_t *pHostapdAdapter;
    hdd_ap_ctx_t *pHddApCtx;
#endif /*DISABLE_CONCURRENCY_AUTOSAVE */

    /* event_name space-delimiter driver_module_name */
    /* Format of the event is "AUTO-SHUT.indication" " " "module_name" */
    char * autoShutEvent = "AUTO-SHUT.indication" " "  KBUILD_MODNAME;
    int event_len = strlen(autoShutEvent) + 1; /* For the NULL at the end */

    ENTER();

#ifdef DISABLE_CONCURRENCY_AUTOSAVE    
    if (vos_concurrent_sessions_running())
    {  
       /*
              This timer routine is going to be called only when AP
              persona is up.
              If there are concurrent sessions running we do not want
              to shut down the Bss.Instead we run the timer again so
              that if Autosave is enabled next time and other session
              was down only then we bring down AP 
             */
        pHostapdAdapter = netdev_priv(dev);
        pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
        vos_status = vos_timer_start(
         &pHddApCtx->hdd_ap_inactivity_timer, 
         (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff
          * 1000);
        if (!VOS_IS_STATUS_SUCCESS(vos_status))
        {
            hddLog(LOGE, FL("Failed to init AP inactivity timer"));
        }
        EXIT();
        return;
    }
#endif /*DISABLE_CONCURRENCY_AUTOSAVE */
    memset(&we_custom_event, '\0', sizeof(we_custom_event));
    memcpy(&we_custom_event, autoShutEvent, event_len);

    memset(&wrqu, 0, sizeof(wrqu));
    wrqu.data.length = event_len;

    hddLog(LOG1, FL("Shutting down AP interface due to inactivity"));
    wireless_send_event(dev, IWEVCUSTOM, &wrqu, (char *)we_custom_event);    

    EXIT();
}

VOS_STATUS hdd_change_mcc_go_beacon_interval(hdd_adapter_t *pHostapdAdapter)
{
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
    ptSapContext  pSapCtx = NULL;
    eHalStatus halStatus = eHAL_STATUS_FAILURE;
    v_PVOID_t hHal = NULL;

    VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_INFO_HIGH,
               "%s: UPDATE Beacon Params", __func__);

    if(VOS_STA_SAP_MODE == vos_get_conparam ( )){
        pSapCtx = VOS_GET_SAP_CB(pVosContext);
        if ( NULL == pSapCtx )
        {
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid SAP pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }

        hHal = VOS_GET_HAL_CB(pSapCtx->pvosGCtx);
        if ( NULL == hHal ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Invalid HAL pointer from pvosGCtx", __func__);
            return VOS_STATUS_E_FAULT;
        }
        halStatus = sme_ChangeMCCBeaconInterval(hHal, pSapCtx->sessionId);
        if(halStatus == eHAL_STATUS_FAILURE ){
            VOS_TRACE( VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                       "%s: Failed to update Beacon Params", __func__);
            return VOS_STATUS_E_FAILURE;
        }
    }
    return VOS_STATUS_SUCCESS;
}

void hdd_clear_all_sta(hdd_adapter_t *pHostapdAdapter, v_PVOID_t usrDataForCallback)
{
    v_U8_t staId = 0;
    struct net_device *dev;
    dev = (struct net_device *)usrDataForCallback;

    hddLog(LOGE, FL("Clearing all the STA entry....\n"));
    for (staId = 0; staId < WLAN_MAX_STA_COUNT; staId++)
    {
        if ( pHostapdAdapter->aStaInfo[staId].isUsed && 
           ( staId != (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uBCStaId))
        {
            //Disconnect all the stations
            hdd_softap_sta_disassoc(pHostapdAdapter, &pHostapdAdapter->aStaInfo[staId].macAddrSTA.bytes[0]);
        }
    }
}

static int hdd_stop_p2p_link(hdd_adapter_t *pHostapdAdapter,v_PVOID_t usrDataForCallback)
{
    struct net_device *dev;
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    dev = (struct net_device *)usrDataForCallback;
    ENTER();
    if(test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags)) 
    {
        if ( VOS_STATUS_SUCCESS == (status = WLANSAP_StopBss((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext) ) )
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, FL("Deleting P2P link!!!!!!"));
        }
        clear_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
    }
    EXIT();
    return (status == VOS_STATUS_SUCCESS) ? 0 : -EBUSY;
}

VOS_STATUS hdd_hostapd_SAPEventCB( tpSap_Event pSapEvent, v_PVOID_t usrDataForCallback)
{
    hdd_adapter_t *pHostapdAdapter;
    hdd_ap_ctx_t *pHddApCtx;
    hdd_hostapd_state_t *pHostapdState;
    struct net_device *dev;
    eSapHddEvent sapEvent;
    union iwreq_data wrqu;
    v_BYTE_t *we_custom_event_generic = NULL;
    int we_event = 0;
    int i = 0;
    v_U8_t staId;
    VOS_STATUS vos_status; 
    v_BOOL_t bWPSState;
    v_BOOL_t bApActive = FALSE;
    v_BOOL_t bAuthRequired = TRUE;
    tpSap_AssocMacAddr pAssocStasArray = NULL;
    char unknownSTAEvent[IW_CUSTOM_MAX+1];
    char maxAssocExceededEvent[IW_CUSTOM_MAX+1];
    v_BYTE_t we_custom_start_event[64];
    char *startBssEvent; 
    hdd_context_t *pHddCtx;
    hdd_scaninfo_t *pScanInfo  = NULL;
    struct iw_michaelmicfailure msg;

    dev = (struct net_device *)usrDataForCallback;
    pHostapdAdapter = netdev_priv(dev);

    if ((NULL == pHostapdAdapter) ||
        (WLAN_HDD_ADAPTER_MAGIC != pHostapdAdapter->magic))
    {
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_FATAL,
                "invalid adapter or adapter has invalid magic");
        return eHAL_STATUS_FAILURE;
    }

    pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter); 
    pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
    sapEvent = pSapEvent->sapHddEventCode;
    memset(&wrqu, '\0', sizeof(wrqu));
    pHddCtx = (hdd_context_t*)(pHostapdAdapter->pHddCtx);

    switch(sapEvent)
    {
        case eSAP_START_BSS_EVENT :
            hddLog(LOG1, FL("BSS configured status = %s, channel = %lu, bc sta Id = %d\n"),
                            pSapEvent->sapevt.sapStartBssCompleteEvent.status ? "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS",
                            pSapEvent->sapevt.sapStartBssCompleteEvent.operatingChannel,
                              pSapEvent->sapevt.sapStartBssCompleteEvent.staId);

            pHostapdState->vosStatus = pSapEvent->sapevt.sapStartBssCompleteEvent.status;
            vos_status = vos_event_set(&pHostapdState->vosEvent);
   
            if (!VOS_IS_STATUS_SUCCESS(vos_status) || pHostapdState->vosStatus)
            {     
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: startbss event failed!!\n"));
                goto stopbss;
            }
            else
            {                
                pHddApCtx->uBCStaId = pSapEvent->sapevt.sapStartBssCompleteEvent.staId;
                //@@@ need wep logic here to set privacy bit
                hdd_softap_Register_BC_STA(pHostapdAdapter, pHddApCtx->uPrivacy);
            }
            
            if (0 != (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff)
            {
                // AP Inactivity timer init and start
                vos_status = vos_timer_init( &pHddApCtx->hdd_ap_inactivity_timer, VOS_TIMER_TYPE_SW, 
                                            hdd_hostapd_inactivity_timer_cb, (v_PVOID_t)dev );
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                   hddLog(LOGE, FL("Failed to init AP inactivity timer\n"));

                vos_status = vos_timer_start( &pHddApCtx->hdd_ap_inactivity_timer, (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff * 1000);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                   hddLog(LOGE, FL("Failed to init AP inactivity timer\n"));

            }
            pHddApCtx->operatingChannel = pSapEvent->sapevt.sapStartBssCompleteEvent.operatingChannel;
            pHostapdState->bssState = BSS_START;

            // Send current operating channel of SoftAP to BTC-ES
            send_btc_nlink_msg(WLAN_BTC_SOFTAP_BSS_START, 0);

            //Check if there is any group key pending to set.
            if( pHddApCtx->groupKey.keyLength )
            {
                 if( VOS_STATUS_SUCCESS !=  WLANSAP_SetKeySta( 
                               (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext,
                               &pHddApCtx->groupKey ) )
                 {
                      VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
                             "%s: WLANSAP_SetKeySta failed", __func__);
                 }
                 pHddApCtx->groupKey.keyLength = 0;
            }
            else if ( pHddApCtx->wepKey[0].keyLength )
            {
                int i=0;
                for ( i = 0; i < CSR_MAX_NUM_KEY; i++ ) 
                {
                    if( VOS_STATUS_SUCCESS !=  WLANSAP_SetKeySta(
                                (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext,
                                &pHddApCtx->wepKey[i] ) )
                    {   
                          VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                             "%s: WLANSAP_SetKeySta failed idx %d", __func__, i);
                    }
                    pHddApCtx->wepKey[i].keyLength = 0;
                }
           }
            //Fill the params for sending IWEVCUSTOM Event with SOFTAP.enabled
            startBssEvent = "SOFTAP.enabled";
            memset(&we_custom_start_event, '\0', sizeof(we_custom_start_event));
            memcpy(&we_custom_start_event, startBssEvent, strlen(startBssEvent));
            memset(&wrqu, 0, sizeof(wrqu));
            wrqu.data.length = strlen(startBssEvent);
            we_event = IWEVCUSTOM;
            we_custom_event_generic = we_custom_start_event;
            hdd_dump_concurrency_info(pHddCtx);
            break; //Event will be sent after Switch-Case stmt 

        case eSAP_STOP_BSS_EVENT:
            hddLog(LOG1, FL("BSS stop status = %s\n"),pSapEvent->sapevt.sapStopBssCompleteEvent.status ? 
                             "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

            //Free up Channel List incase if it is set
            sapCleanupChannelList();

            pHddApCtx->operatingChannel = 0; //Invalidate the channel info.
            goto stopbss;
        case eSAP_STA_SET_KEY_EVENT:
            //TODO: forward the message to hostapd once implementtation is done for now just print
            hddLog(LOG1, FL("SET Key: configured status = %s\n"),pSapEvent->sapevt.sapStationSetKeyCompleteEvent.status ? 
                            "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");
            return VOS_STATUS_SUCCESS;
        case eSAP_STA_DEL_KEY_EVENT:
           //TODO: forward the message to hostapd once implementtation is done for now just print
           hddLog(LOG1, FL("Event received %s\n"),"eSAP_STA_DEL_KEY_EVENT");
           return VOS_STATUS_SUCCESS;
        case eSAP_STA_MIC_FAILURE_EVENT:
        {
            memset(&msg, '\0', sizeof(msg));
            msg.src_addr.sa_family = ARPHRD_ETHER;
            memcpy(msg.src_addr.sa_data, &pSapEvent->sapevt.sapStationMICFailureEvent.staMac, sizeof(v_MACADDR_t));
            hddLog(LOG1, "MIC MAC "MAC_ADDRESS_STR"\n", MAC_ADDR_ARRAY(msg.src_addr.sa_data));
            if(pSapEvent->sapevt.sapStationMICFailureEvent.multicast == eSAP_TRUE)
             msg.flags = IW_MICFAILURE_GROUP;
            else 
             msg.flags = IW_MICFAILURE_PAIRWISE;
            memset(&wrqu, 0, sizeof(wrqu));
            wrqu.data.length = sizeof(msg);
            we_event = IWEVMICHAELMICFAILURE;
            we_custom_event_generic = (v_BYTE_t *)&msg;
        }
      /* inform mic failure to nl80211 */
        cfg80211_michael_mic_failure(dev, 
                                     pSapEvent->sapevt.
                                     sapStationMICFailureEvent.staMac.bytes,
                                     ((pSapEvent->sapevt.sapStationMICFailureEvent.multicast == eSAP_TRUE) ? 
                                      NL80211_KEYTYPE_GROUP :
                                      NL80211_KEYTYPE_PAIRWISE),
                                     pSapEvent->sapevt.sapStationMICFailureEvent.keyId, 
                                     pSapEvent->sapevt.sapStationMICFailureEvent.TSC, 
                                     GFP_KERNEL);
            break;
        
        case eSAP_STA_ASSOC_EVENT:
        case eSAP_STA_REASSOC_EVENT:
            wrqu.addr.sa_family = ARPHRD_ETHER;
            memcpy(wrqu.addr.sa_data, &pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staMac, 
                sizeof(v_MACADDR_t));
            hddLog(LOG1, " associated "MAC_ADDRESS_STR"\n", MAC_ADDR_ARRAY(wrqu.addr.sa_data));
            we_event = IWEVREGISTERED;
            
            WLANSAP_Get_WPS_State((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext, &bWPSState);
         
            if ( (eCSR_ENCRYPT_TYPE_NONE == pHddApCtx->ucEncryptType) ||
                 ( eCSR_ENCRYPT_TYPE_WEP40_STATICKEY == pHddApCtx->ucEncryptType ) || 
                 ( eCSR_ENCRYPT_TYPE_WEP104_STATICKEY == pHddApCtx->ucEncryptType ) )
            {
                bAuthRequired = FALSE;
            }

            if (bAuthRequired || bWPSState == eANI_BOOLEAN_TRUE )
            {
                hdd_softap_RegisterSTA( pHostapdAdapter,
                                       TRUE,
                                       pHddApCtx->uPrivacy,
                                       pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staId,
                                       0,
                                       0,
                                       (v_MACADDR_t *)wrqu.addr.sa_data,
                                       pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.wmmEnabled);
            }
            else
            {
                hdd_softap_RegisterSTA( pHostapdAdapter,
                                       FALSE,
                                       pHddApCtx->uPrivacy,
                                       pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staId,
                                       0,
                                       0,
                                       (v_MACADDR_t *)wrqu.addr.sa_data,
                                       pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.wmmEnabled);
            }

            // Stop AP inactivity timer
            if (pHddApCtx->hdd_ap_inactivity_timer.state == VOS_TIMER_STATE_RUNNING)
            {
                vos_status = vos_timer_stop(&pHddApCtx->hdd_ap_inactivity_timer);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                   hddLog(LOGE, FL("Failed to start AP inactivity timer\n"));
            }
#ifdef WLAN_OPEN_SOURCE
            if (wake_lock_active(&pHddCtx->sap_wake_lock))
            {
               wake_unlock(&pHddCtx->sap_wake_lock);
            }
            wake_lock_timeout(&pHddCtx->sap_wake_lock, msecs_to_jiffies(HDD_SAP_WAKE_LOCK_DURATION));
#endif
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
            {
                struct station_info staInfo;
                v_U16_t iesLen =  pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.iesLen;

                memset(&staInfo, 0, sizeof(staInfo));
                if (iesLen <= MAX_ASSOC_IND_IE_LEN )
                {
                    staInfo.assoc_req_ies =
                        (const u8 *)&pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.ies[0];
                    staInfo.assoc_req_ies_len = iesLen;
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3,0,31))
                    staInfo.filled |= STATION_INFO_ASSOC_REQ_IES;
#endif
                    cfg80211_new_sta(dev,
                                 (const u8 *)&pSapEvent->sapevt.sapStationAssocReassocCompleteEvent.staMac.bytes[0],
                                 &staInfo, GFP_KERNEL);
                }
                else
                {
                    hddLog(LOGE, FL(" Assoc Ie length is too long \n"));
                }
             }
#endif
            pScanInfo =  &pHddCtx->scan_info;
            // Lets do abort scan to ensure smooth authentication for client
            if ((pScanInfo != NULL) && pScanInfo->mScanPending)
            {
                hdd_abort_mac_scan(pHddCtx, pHostapdAdapter->sessionId);
            }

            break;
        case eSAP_STA_DISASSOC_EVENT:
            memcpy(wrqu.addr.sa_data, &pSapEvent->sapevt.sapStationDisassocCompleteEvent.staMac,
                   sizeof(v_MACADDR_t));
            hddLog(LOG1, " disassociated "MAC_ADDRESS_STR"\n", MAC_ADDR_ARRAY(wrqu.addr.sa_data));
            if (pSapEvent->sapevt.sapStationDisassocCompleteEvent.reason == eSAP_USR_INITATED_DISASSOC)
                hddLog(LOG1," User initiated disassociation");
            else
                hddLog(LOG1," MAC initiated disassociation");
            we_event = IWEVEXPIRED;
            vos_status = hdd_softap_GetStaId(pHostapdAdapter, &pSapEvent->sapevt.sapStationDisassocCompleteEvent.staMac, &staId);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
            {
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, FL("ERROR: HDD Failed to find sta id!!"));
                return VOS_STATUS_E_FAILURE;
            }
            hdd_softap_DeregisterSTA(pHostapdAdapter, staId);

            if (0 != (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff)
            {
                spin_lock_bh( &pHostapdAdapter->staInfo_lock );
                // Start AP inactivity timer if no stations associated with it
                for (i = 0; i < WLAN_MAX_STA_COUNT; i++)
                {
                    if (pHostapdAdapter->aStaInfo[i].isUsed && i != (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uBCStaId)
                    {
                        bApActive = TRUE;
                        break;
                    }
                }
                spin_unlock_bh( &pHostapdAdapter->staInfo_lock );

                if (bApActive == FALSE)
                {
                    if (pHddApCtx->hdd_ap_inactivity_timer.state == VOS_TIMER_STATE_STOPPED)
                    {
                        vos_status = vos_timer_start(&pHddApCtx->hdd_ap_inactivity_timer, (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff * 1000);
                        if (!VOS_IS_STATUS_SUCCESS(vos_status))
                            hddLog(LOGE, FL("Failed to init AP inactivity timer\n"));
                    }
                    else
                        VOS_ASSERT(vos_timer_getCurrentState(&pHddApCtx->hdd_ap_inactivity_timer) == VOS_TIMER_STATE_STOPPED);
                }
            }
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
            cfg80211_del_sta(dev,
                            (const u8 *)&pSapEvent->sapevt.sapStationDisassocCompleteEvent.staMac.bytes[0],
                            GFP_KERNEL);
#endif
            //Update the beacon Interval if it is P2P GO
            hdd_change_mcc_go_beacon_interval(pHostapdAdapter);
            break;
        case eSAP_WPS_PBC_PROBE_REQ_EVENT:
        {
                static const char * message ="MLMEWPSPBCPROBEREQ.indication";
                union iwreq_data wreq;
               
                down(&pHddApCtx->semWpsPBCOverlapInd);
                pHddApCtx->WPSPBCProbeReq.probeReqIELen = pSapEvent->sapevt.sapPBCProbeReqEvent.WPSPBCProbeReq.probeReqIELen;
                
                vos_mem_copy(pHddApCtx->WPSPBCProbeReq.probeReqIE, pSapEvent->sapevt.sapPBCProbeReqEvent.WPSPBCProbeReq.probeReqIE, 
                    pHddApCtx->WPSPBCProbeReq.probeReqIELen);
                     
                vos_mem_copy(pHddApCtx->WPSPBCProbeReq.peerMacAddr, pSapEvent->sapevt.sapPBCProbeReqEvent.WPSPBCProbeReq.peerMacAddr, sizeof(v_MACADDR_t));
                hddLog(LOG1, "WPS PBC probe req "MAC_ADDRESS_STR"\n", MAC_ADDR_ARRAY(pHddApCtx->WPSPBCProbeReq.peerMacAddr));
                memset(&wreq, 0, sizeof(wreq));
                wreq.data.length = strlen(message); // This is length of message
                wireless_send_event(dev, IWEVCUSTOM, &wreq, (char *)message); 
                
                return VOS_STATUS_SUCCESS;
        }
        case eSAP_ASSOC_STA_CALLBACK_EVENT:
            pAssocStasArray = pSapEvent->sapevt.sapAssocStaListEvent.pAssocStas;
            if (pSapEvent->sapevt.sapAssocStaListEvent.noOfAssocSta != 0)
            {   // List of associated stations
                for (i = 0; i < pSapEvent->sapevt.sapAssocStaListEvent.noOfAssocSta; i++)
                {
                    hddLog(LOG1,"Associated Sta Num %d:assocId=%d, staId=%d, staMac="MAC_ADDRESS_STR,
                        i+1,
                        pAssocStasArray->assocId,
                        pAssocStasArray->staId,
                                    MAC_ADDR_ARRAY(pAssocStasArray->staMac.bytes));
                        pAssocStasArray++;             
            }
            }
            vos_mem_free(pSapEvent->sapevt.sapAssocStaListEvent.pAssocStas);// Release caller allocated memory here
            return VOS_STATUS_SUCCESS;
        case eSAP_INDICATE_MGMT_FRAME:
           hdd_indicateMgmtFrame( pHostapdAdapter, 
                                 pSapEvent->sapevt.sapManagementFrameInfo.nFrameLength,
                                 pSapEvent->sapevt.sapManagementFrameInfo.pbFrames,
                                 pSapEvent->sapevt.sapManagementFrameInfo.frameType, 
                                 pSapEvent->sapevt.sapManagementFrameInfo.rxChan, 0);
           return VOS_STATUS_SUCCESS;
        case eSAP_REMAIN_CHAN_READY:
           hdd_remainChanReadyHandler( pHostapdAdapter );
           return VOS_STATUS_SUCCESS;
        case eSAP_SEND_ACTION_CNF:
           hdd_sendActionCnf( pHostapdAdapter, 
                              ( eSAP_STATUS_SUCCESS == 
                                pSapEvent->sapevt.sapActionCnf.actionSendSuccess ) ? 
                                TRUE : FALSE );
           return VOS_STATUS_SUCCESS;
        case eSAP_UNKNOWN_STA_JOIN:
            snprintf(unknownSTAEvent, IW_CUSTOM_MAX, "JOIN_UNKNOWN_STA-%02x:%02x:%02x:%02x:%02x:%02x",
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[0],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[1],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[2],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[3],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[4],
                pSapEvent->sapevt.sapUnknownSTAJoin.macaddr.bytes[5]);
            we_event = IWEVCUSTOM; /* Discovered a new node (AP mode). */
            wrqu.data.pointer = unknownSTAEvent;
            wrqu.data.length = strlen(unknownSTAEvent);
            we_custom_event_generic = (v_BYTE_t *)unknownSTAEvent;
            hddLog(LOG1,"%s\n", unknownSTAEvent);
            break;

        case eSAP_MAX_ASSOC_EXCEEDED:
            snprintf(maxAssocExceededEvent, IW_CUSTOM_MAX, "Peer %02x:%02x:%02x:%02x:%02x:%02x denied"
                    " assoc due to Maximum Mobile Hotspot connections reached. Please disconnect"
                    " one or more devices to enable the new device connection",
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[0],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[1],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[2],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[3],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[4],
                    pSapEvent->sapevt.sapMaxAssocExceeded.macaddr.bytes[5]);
            we_event = IWEVCUSTOM; /* Discovered a new node (AP mode). */
            wrqu.data.pointer = maxAssocExceededEvent;
            wrqu.data.length = strlen(maxAssocExceededEvent);
            we_custom_event_generic = (v_BYTE_t *)maxAssocExceededEvent;
            hddLog(LOG1,"%s\n", maxAssocExceededEvent);
            break;
        case eSAP_STA_ASSOC_IND:
            return VOS_STATUS_SUCCESS;

        case eSAP_DISCONNECT_ALL_P2P_CLIENT:
            hddLog(LOG1, FL(" Disconnecting all the P2P Clients....\n"));
            hdd_clear_all_sta(pHostapdAdapter, usrDataForCallback);
            return VOS_STATUS_SUCCESS;

        case eSAP_MAC_TRIG_STOP_BSS_EVENT :
            hdd_stop_p2p_link(pHostapdAdapter, usrDataForCallback);
            return VOS_STATUS_SUCCESS;

        default:
            hddLog(LOG1,"SAP message is not handled\n");
            goto stopbss;
            return VOS_STATUS_SUCCESS;
    }
    wireless_send_event(dev, we_event, &wrqu, (char *)we_custom_event_generic);
    return VOS_STATUS_SUCCESS;

stopbss :
    {
        v_BYTE_t we_custom_event[64];
        char *stopBssEvent = "STOP-BSS.response";//17
        int event_len = strlen(stopBssEvent);

        hddLog(LOG1, FL("BSS stop status = %s"),
               pSapEvent->sapevt.sapStopBssCompleteEvent.status ?
                            "eSAP_STATUS_FAILURE" : "eSAP_STATUS_SUCCESS");

        /* Change the BSS state now since, as we are shutting things down,
         * we don't want interfaces to become re-enabled */
        pHostapdState->bssState = BSS_STOP;

        if (0 != (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->nAPAutoShutOff)
        {
            if (VOS_TIMER_STATE_RUNNING == pHddApCtx->hdd_ap_inactivity_timer.state)
            {
                vos_status = vos_timer_stop(&pHddApCtx->hdd_ap_inactivity_timer);
                if (!VOS_IS_STATUS_SUCCESS(vos_status))
                    hddLog(LOGE, FL("Failed to stop AP inactivity timer"));
            }

            vos_status = vos_timer_destroy(&pHddApCtx->hdd_ap_inactivity_timer);
            if (!VOS_IS_STATUS_SUCCESS(vos_status))
                hddLog(LOGE, FL("Failed to Destroy AP inactivity timer"));
        }

        /* Stop the pkts from n/w stack as we are going to free all of
         * the TX WMM queues for all STAID's */
        hdd_hostapd_stop(dev);

        /* reclaim all resources allocated to the BSS */
        hdd_softap_stop_bss(pHostapdAdapter);

        /* once the event is set, structure dev/pHostapdAdapter should
         * not be touched since they are now subject to being deleted
         * by another thread */
        if (eSAP_STOP_BSS_EVENT == sapEvent)
            vos_event_set(&pHostapdState->vosEvent);

        /* notify userspace that the BSS has stopped */
        memset(&we_custom_event, '\0', sizeof(we_custom_event));
        memcpy(&we_custom_event, stopBssEvent, event_len);
        memset(&wrqu, 0, sizeof(wrqu));
        wrqu.data.length = event_len;
        we_event = IWEVCUSTOM;
        we_custom_event_generic = we_custom_event;
        wireless_send_event(dev, we_event, &wrqu, (char *)we_custom_event_generic);
        hdd_dump_concurrency_info(pHddCtx);
    }
    return VOS_STATUS_SUCCESS;
}
int hdd_softap_unpackIE( 
                tHalHandle halHandle,
                eCsrEncryptionType *pEncryptType, 
                eCsrEncryptionType *mcEncryptType, 
                eCsrAuthType *pAuthType, 
                u_int16_t gen_ie_len, 
                u_int8_t *gen_ie )
{
    tDot11fIERSN dot11RSNIE; 
    tDot11fIEWPA dot11WPAIE; 
 
    tANI_U8 *pRsnIe; 
    tANI_U16 RSNIeLen;
    
    if (NULL == halHandle)
    {
        hddLog(LOGE, FL("Error haHandle returned NULL\n"));
        return -EINVAL;
    }
    
    // Validity checks
    if ((gen_ie_len < VOS_MIN(DOT11F_IE_RSN_MIN_LEN, DOT11F_IE_WPA_MIN_LEN)) ||  
        (gen_ie_len > VOS_MAX(DOT11F_IE_RSN_MAX_LEN, DOT11F_IE_WPA_MAX_LEN)) ) 
        return -EINVAL;
    // Type check
    if ( gen_ie[0] ==  DOT11F_EID_RSN) 
    {         
        // Validity checks
        if ((gen_ie_len < DOT11F_IE_RSN_MIN_LEN ) ||  
            (gen_ie_len > DOT11F_IE_RSN_MAX_LEN) )
        {
            return VOS_STATUS_E_FAILURE;
        }
        // Skip past the EID byte and length byte  
        pRsnIe = gen_ie + 2; 
        RSNIeLen = gen_ie_len - 2; 
        // Unpack the RSN IE
        memset(&dot11RSNIE, 0, sizeof(tDot11fIERSN));
        dot11fUnpackIeRSN((tpAniSirGlobal) halHandle, 
                            pRsnIe, 
                            RSNIeLen, 
                            &dot11RSNIE);
        // Copy out the encryption and authentication types 
        hddLog(LOG1, FL("%s: pairwise cipher suite count: %d\n"), 
                __func__, dot11RSNIE.pwise_cipher_suite_count );
        hddLog(LOG1, FL("%s: authentication suite count: %d\n"), 
                __func__, dot11RSNIE.akm_suite_count);
        /*Here we have followed the apple base code, 
          but probably I suspect we can do something different*/
        //dot11RSNIE.akm_suite_count
        // Just translate the FIRST one 
        *pAuthType =  hdd_TranslateRSNToCsrAuthType(dot11RSNIE.akm_suites[0]); 
        //dot11RSNIE.pwise_cipher_suite_count 
        *pEncryptType = hdd_TranslateRSNToCsrEncryptionType(dot11RSNIE.pwise_cipher_suites[0]);                     
        //dot11RSNIE.gp_cipher_suite_count 
        *mcEncryptType = hdd_TranslateRSNToCsrEncryptionType(dot11RSNIE.gp_cipher_suite);                     
        // Set the PMKSA ID Cache for this interface
          
        // Calling csrRoamSetPMKIDCache to configure the PMKIDs into the cache
    } else 
    if (gen_ie[0] == DOT11F_EID_WPA) 
    {         
        // Validity checks
        if ((gen_ie_len < DOT11F_IE_WPA_MIN_LEN ) ||  
            (gen_ie_len > DOT11F_IE_WPA_MAX_LEN))
        {
            return VOS_STATUS_E_FAILURE;
        }
        // Skip past the EID byte and length byte - and four byte WiFi OUI  
        pRsnIe = gen_ie + 2 + 4; 
        RSNIeLen = gen_ie_len - (2 + 4); 
        // Unpack the WPA IE
        memset(&dot11WPAIE, 0, sizeof(tDot11fIEWPA));
        dot11fUnpackIeWPA((tpAniSirGlobal) halHandle, 
                            pRsnIe, 
                            RSNIeLen, 
                            &dot11WPAIE);
        // Copy out the encryption and authentication types 
        hddLog(LOG1, FL("%s: WPA unicast cipher suite count: %d\n"), 
                __func__, dot11WPAIE.unicast_cipher_count );
        hddLog(LOG1, FL("%s: WPA authentication suite count: %d\n"), 
                __func__, dot11WPAIE.auth_suite_count);
        //dot11WPAIE.auth_suite_count
        // Just translate the FIRST one 
        *pAuthType =  hdd_TranslateWPAToCsrAuthType(dot11WPAIE.auth_suites[0]); 
        //dot11WPAIE.unicast_cipher_count 
        *pEncryptType = hdd_TranslateWPAToCsrEncryptionType(dot11WPAIE.unicast_ciphers[0]);                       
        //dot11WPAIE.unicast_cipher_count 
        *mcEncryptType = hdd_TranslateWPAToCsrEncryptionType(dot11WPAIE.multicast_cipher);                       
    } 
    else 
    { 
        hddLog(LOGW, FL("%s: gen_ie[0]: %d\n"), __func__, gen_ie[0]);
        return VOS_STATUS_E_FAILURE; 
    }
    return VOS_STATUS_SUCCESS;
}

/**---------------------------------------------------------------------------

  \brief hdd_hostapd_set_mc_rate_cb() -

  This is called to notify associated stas information ready

  \param  - sapEvent Pointer to get associated stas event
  \param  - apDriver SoftAP context

  \return - none

  --------------------------------------------------------------------------*/
void hdd_hostapd_set_mc_rate_cb
(
   tSap_Event      *sapEvent,
   void            *apDriver
)
{
   hdd_ap_ctx_t    *apCtxt;

   if ((NULL == apDriver) || (NULL == sapEvent))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s : Invalid arguments", __func__);
      return;
   }

   apCtxt = (hdd_ap_ctx_t *)apDriver;

   /* there is a race condition that exists between this callback function
      and the caller since the caller could time out either before or
      while this code is executing.  we'll assume the timeout hasn't
      occurred, but we'll verify that right before complete our work */
   if (SAP_GET_STAS_COOKIE == apCtxt->getStasCookie)
   {
      vos_mem_copy((void *)&apCtxt->getStasEventBuffer,
                   (void *)sapEvent,
                   sizeof(tSap_Event));
      complete(&apCtxt->sap_get_associated_stas_complete);
   }
   else
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s : Invalid cookie", __func__);
   }
   return;
}

/**---------------------------------------------------------------------------

  \brief hdd_hostapd_set_mc_rate_update

  This is called to find rate and send cfg command to FW

  \param  - sapEvent Pointer to get associated stas event
  \param  - pHostapdAdapter SoftAP Adapter Context

  \return - int, 0 success
                 negative fail

  --------------------------------------------------------------------------*/
static int hdd_hostapd_set_mc_rate_update
(
   tSap_Event      *sapEvent,
   hdd_adapter_t   *pHostapdAdapter
)
{
   tHalHandle               hHal;
   hdd_ap_ctx_t            *apCtxt;
   tSap_AssocMacAddr       *assocSta;
   rate_cfg_11ac_max_mcs_t  supportedAcMaxMcs = RATE_CFG_RATE_11AC_MAX_MCS_7;
   rate_cfg_supported_bw_t  bandWidth;
   rate_cfg_gi_t            gi;
   rate_cfg_item_mapping_t *nMappingTable = NULL;
   rate_cfg_item_mapping_t *acMappingTable = NULL;
   v_U8_t                   stasLoop, ratesLoop;
   v_U8_t                   rateArrayOrder;
   v_U8_t                   mcsTable11n;
   v_U16_t                  targetCfgId = 0;
   v_U16_t                  targetCfgValue = 0;
   v_U16_t                  currentRate;
   v_U16_t                  combinedSupportMap = 0xFFFF;
   v_U16_t                  supportMap = 0x0000;
   v_U16_t                  supportedChannelCount = 0;
   v_U32_t                  legacyRates[SAP_LEGACY_RATE_COUNT];
   int                      rc = 0;
   tSirRetStatus            cfdStat;

   if ((NULL == pHostapdAdapter) || (NULL == sapEvent))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s : Invalid arguments", __func__);
      return -1;
   }

   apCtxt = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
   hHal   = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "setMcRateCB NUM SAT %d, targetMCRate %d, current channel %d",
             sapEvent->sapevt.sapAssocStaListEvent.noOfAssocSta,
             apCtxt->targetMCRate,
             apCtxt->operatingChannel);

   if (!sapEvent->sapevt.sapAssocStaListEvent.noOfAssocSta)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "Not connected any STA yet");
      return -1;
   }

   for (stasLoop = 0;
        stasLoop < sapEvent->sapevt.sapAssocStaListEvent.noOfAssocSta;
        stasLoop++)
   {
      vos_mem_zero((v_U8_t *)legacyRates, sizeof(legacyRates));
      rateArrayOrder = 0;
      mcsTable11n    = 0;
      supportedChannelCount = 0;
      assocSta = sapEvent->sapevt.sapAssocStaListEvent.pAssocStas++;
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "ASSOSID %d, OPM %d, nBM %d, SGI40 %d, SGI20 %d, S40 %d",
                assocSta->assocId,
                assocSta->supportedRates.opRateMode,
                assocSta->supportedRates.aniEnhancedRateBitmap,
                assocSta->ShortGI40Mhz,
                assocSta->ShortGI20Mhz,
                assocSta->Support40Mhz);

      /* Legacy Rate */
      for (ratesLoop = 0; ratesLoop < SIR_NUM_11B_RATES; ratesLoop++)
      {
         currentRate = assocSta->supportedRates.llbRates[ratesLoop] &
                       SAP_LEGACY_RATE_MASK;

         /* To fix KW error report */
         if (SAP_LEGACY_RATE_COUNT <= rateArrayOrder)
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s, Invalid array Size, break", __func__);
            break;
         }

         /* Make 100kbps order */
         legacyRates[rateArrayOrder] = (currentRate * 100) / 20;
         rateArrayOrder++;
         if (currentRate)
         {
            supportedChannelCount++;
         }
      }
      for (ratesLoop = 0; ratesLoop < SIR_NUM_11A_RATES; ratesLoop++)
      {
         currentRate = assocSta->supportedRates.llaRates[ratesLoop] &
                       SAP_LEGACY_RATE_MASK;
         /* To fix KW error report */
         if (SAP_LEGACY_RATE_COUNT <= rateArrayOrder)
         {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                      "%s, Invalid array Size, break", __func__);
            break;
         }

         /* Make 100kbps order */
         legacyRates[rateArrayOrder] = (currentRate * 100) / 20;
         rateArrayOrder++;
         if (currentRate)
         {
            supportedChannelCount++;
         }
      }
      if (supportedChannelCount)
      {
         for (ratesLoop = 0; ratesLoop < SAP_LEGACY_RATE_COUNT; ratesLoop++)
         {
            if (legacyRates[ratesLoop] == apCtxt->targetMCRate)
            {
               supportMap |= (1 << RATE_CFG_RATE_LEGACY);
               break;
            }
         }
      }

      /* 11N */
      if (eSTA_11n <= assocSta->supportedRates.opRateMode)
      {
         if (assocSta->Support40Mhz)
         {
            mcsTable11n |= 0x01;
            if (assocSta->ShortGI40Mhz)
            {
               mcsTable11n |= 0x02;
               supportMap |= (1 << RATE_CFG_RATE_11N_MCS_SGI_40);
               nMappingTable = n_s40_rate_mapping;
            }
            else
            {
               supportMap |= (1 << RATE_CFG_RATE_11N_MCS_LGI_40);
               nMappingTable = n_l40_rate_mapping;
            }
         }
         else
         {
            if (assocSta->ShortGI20Mhz)
            {
               mcsTable11n |= 0x02;
               supportMap |= (1 << RATE_CFG_RATE_11N_MCS_SGI_20);
               nMappingTable = n_s20_rate_mapping;
            }
            else
            {
               supportMap |= (1 << RATE_CFG_RATE_11N_MCS_LGI_20);
               nMappingTable = n_l20_rate_mapping;
            }
         }
      }

#ifdef WLAN_FEATURE_11AC
      /* 11AC */
      if (eSTA_11ac <= assocSta->supportedRates.opRateMode)
      {
         /* Find supported MAX MCS */
         supportedAcMaxMcs = assocSta->supportedRates.vhtRxMCSMap &
                             SAP_AC_MCS_MAP_MASK;
         supportedAcMaxMcs += SAP_AC_MCS_MAP_OFFSET;
         /* Find channel characteristics from MAX rate */
         if (mcs_rate_11ac[supportedAcMaxMcs].cb80_rate_11ac[0] ==
             assocSta->supportedRates.vhtRxHighestDataRate)
         {
            supportMap |= (1 << RATE_CFG_RATE_11AC_MCS_LGI_80);
            bandWidth = RATE_CFG_RATE_BW_80;
            gi = RATE_CFG_RATE_GI_LONG;
            acMappingTable = ac_l80_rate_mapping;
         }
         else if (mcs_rate_11ac[supportedAcMaxMcs].cb80_rate_11ac[1] ==
                  assocSta->supportedRates.vhtRxHighestDataRate)
         {
            supportMap |= (1 << RATE_CFG_RATE_11AC_MCS_SGI_80);
            bandWidth = RATE_CFG_RATE_BW_80;
            gi = RATE_CFG_RATE_GI_SHORT;
            acMappingTable = ac_s80_rate_mapping;
         }
         else if (mcs_rate_11ac[supportedAcMaxMcs].cb40_rate_11ac[0] ==
                  assocSta->supportedRates.vhtRxHighestDataRate)
         {
            supportMap |= (1 << RATE_CFG_RATE_11AC_MCS_LGI_40);
            bandWidth = RATE_CFG_RATE_BW_40;
            gi = RATE_CFG_RATE_GI_LONG;
            acMappingTable = ac_l40_rate_mapping;
         }
         else if (mcs_rate_11ac[supportedAcMaxMcs].cb40_rate_11ac[1] ==
                  assocSta->supportedRates.vhtRxHighestDataRate)
         {
            supportMap |= (1 << RATE_CFG_RATE_11AC_MCS_SGI_40);
            bandWidth = RATE_CFG_RATE_BW_40;
            gi = RATE_CFG_RATE_GI_SHORT;
            acMappingTable = ac_s40_rate_mapping;
         }
         else if (mcs_rate_11ac[supportedAcMaxMcs].cb20_rate_11ac[0] ==
                  assocSta->supportedRates.vhtRxHighestDataRate)
         {
            supportMap |= (1 << RATE_CFG_RATE_11AC_MCS_LGI_20);
            bandWidth = RATE_CFG_RATE_BW_20;
            gi = RATE_CFG_RATE_GI_LONG;
            acMappingTable = ac_l20_rate_mapping;
         }
         else if (mcs_rate_11ac[supportedAcMaxMcs].cb20_rate_11ac[1] ==
                  assocSta->supportedRates.vhtRxHighestDataRate)
         {
            supportMap |= (1 << RATE_CFG_RATE_11AC_MCS_SGI_20);
            bandWidth = RATE_CFG_RATE_BW_20;
            gi = RATE_CFG_RATE_GI_SHORT;
            acMappingTable = ac_s20_rate_mapping;
         }
      }
#endif /* WLAN_FEATURE_11AC */
      combinedSupportMap &= supportMap;
   }

   if ((!combinedSupportMap) &&
       (!sapEvent->sapevt.sapAssocStaListEvent.noOfAssocSta))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                "%s, No Common supported rate, discard", __func__);
      return -1;
   }

   /* Select target band */
   if (apCtxt->operatingChannel <=
       SAP_MAX_24_CHANNEL_NUMBER)
   {
      targetCfgId = WNI_CFG_FIXED_RATE_MULTICAST_24GHZ;
   }
   else
   {
      targetCfgId = WNI_CFG_FIXED_RATE_MULTICAST_5GHZ;
   }

   /* First find from legacy */
   if (combinedSupportMap & SAP_RATE_SUPPORT_MAP_LEGACY_MASK)
   {
      for (ratesLoop = 0; ratesLoop < SAP_LEGACY_RATE_COUNT; ratesLoop++)
      {
         if (apCtxt->targetMCRate ==
             legacy_rate_mapping[ratesLoop].rate)
         {
            targetCfgValue = legacy_rate_mapping[ratesLoop].eRateCfg;
            break;
         }
      }
   }

   /* If available same on 11N, update target rate */
   if ((combinedSupportMap & SAP_RATE_SUPPORT_MAP_N_MASK) &&
       (NULL != nMappingTable))
   {
      for (ratesLoop = 0; ratesLoop < SAP_11N_RATE_COUNT; ratesLoop++)
      {
         if (apCtxt->targetMCRate == nMappingTable[ratesLoop].rate)
         {
            targetCfgValue = nMappingTable[ratesLoop].eRateCfg;
            break;
         }
      }
   }

#ifdef WLAN_FEATURE_11AC
   /* If available same on 11AC, update target rate */
   if ((combinedSupportMap & SAP_RATE_SUPPORT_MAP_AC_MASK) &&
       (NULL != acMappingTable))
   {
      for (ratesLoop = 0; ratesLoop < supportedAcMaxMcs; ratesLoop++)
      {
         if (apCtxt->targetMCRate == acMappingTable[ratesLoop].rate)
         {
            targetCfgValue = acMappingTable[ratesLoop].eRateCfg;
            break;
         }
      }
   }
#endif /* WLAN_FEATURE_11AC */

   /* Finally send config to FW */
   if (targetCfgId && targetCfgValue)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s, Target Band %d, cfg value %d",
                __func__, targetCfgId, targetCfgValue);
      cfdStat = cfgSetInt((tpAniSirGlobal)hHal,
                          targetCfgId,
                          targetCfgValue);
      if (eSIR_SUCCESS != cfdStat)
      {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "%s, CFG Fail %d",
                   __func__, cfdStat);
         rc = -1;
      }
   }

   return rc;
};

/**---------------------------------------------------------------------------

  \brief hdd_hostapd_set_mc_rate() -

  This is called user application set forcefully MC rate

  \param  - pHostapdAdapter Pointer to adapter structure
  \param  - targetRateHkbps MC rate to set, hundreds kbps order

  \return - int, 0 success
                 negative fail

  --------------------------------------------------------------------------*/
int hdd_hostapd_set_mc_rate
(
   hdd_adapter_t *pHostapdAdapter,
   int            targetRateHkbps
)
{
   tHalHandle      hHal;
   hdd_ap_ctx_t   *apCtxt;
   eHalStatus      smeStatus;
   int             rc;

   if ((NULL == pHostapdAdapter) || (0 == targetRateHkbps))
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s : Invalid arguments", __func__);
      return -1;
   }

   apCtxt = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
   hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
   VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
             "hdd_hostapd_setMcRate %d", targetRateHkbps);

   init_completion(&apCtxt->sap_get_associated_stas_complete);

   apCtxt->getStasCookie = SAP_GET_STAS_COOKIE;
   apCtxt->targetMCRate = targetRateHkbps;
   apCtxt->getStasEventBuffer.sapevt.sapAssocStaListEvent.noOfAssocSta = 0;
   apCtxt->assocStasBuffer = (tSap_AssocMacAddr *)vos_mem_malloc(
                    sizeof(tSap_AssocMacAddr) * HAL_NUM_ASSOC_STA);
   if (NULL == apCtxt->assocStasBuffer)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s : Buffer Alloc fail", __func__);
      return -1;
   }
   smeStatus = sme_RoamGetAssociatedStas(hHal,
                             pHostapdAdapter->sessionId,
                             VOS_MODULE_ID_HDD,
                             (void *)apCtxt,
                             hdd_hostapd_set_mc_rate_cb,
                             (tANI_U8 *)apCtxt->assocStasBuffer);
   if (smeStatus)
   {
      apCtxt->getStasCookie = 0;
      vos_mem_free(apCtxt->assocStasBuffer);
      apCtxt->assocStasBuffer = NULL;
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s : SME Issue fail", __func__);
      return -1;
   }

   /* Wait for completion */
   rc = wait_for_completion_interruptible_timeout(
                       &apCtxt->sap_get_associated_stas_complete,
                       msecs_to_jiffies(SAP_MAX_GET_ASSOC_STAS_TIMEOUT));

   /* either we have a response or we timed out
      either way, first invalidate our cookie */
   apCtxt->getStasCookie = 0;
   if (0 >= rc)
   {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
             "%s : Wait timeout or interrupted", __func__);

      /* there is a race condition such that the callback
         function could be executing at the same time we are. of
         primary concern is if the callback function had already
         verified the "magic" but hasn't yet set the completion
         variable. Since the completion variable is on our
         stack, we'll delay just a bit to make sure the data is
         still valid if that is the case */
      vos_sleep(50);
      /* we'll now try to test memory */
   }

   rc = hdd_hostapd_set_mc_rate_update(
         &apCtxt->getStasEventBuffer,
         pHostapdAdapter);
   vos_mem_free(apCtxt->assocStasBuffer);
   apCtxt->assocStasBuffer = NULL;

   return rc;
}

int
static iw_softap_setparam(struct net_device *dev, 
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    int *value = (int *)extra;
    int sub_cmd = value[0];
    int set_value = value[1];
    eHalStatus status;
    int ret = 0; /* success */
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext; 

    switch(sub_cmd)
    {

        case QCSAP_PARAM_CLR_ACL:
            if ( VOS_STATUS_SUCCESS != WLANSAP_ClearACL( pVosContext ))
            {
               ret = -EIO;            
            }
            break;

        case QCSAP_PARAM_ACL_MODE:
            if ((eSAP_ALLOW_ALL < (eSapMacAddrACL)set_value) || 
                (eSAP_ACCEPT_UNLESS_DENIED > (eSapMacAddrACL)set_value))
            {
                hddLog(LOGE, FL("Invalid ACL Mode value %d"), set_value);
                ret = -EINVAL;
            }
            else
            {
                WLANSAP_SetMode(pVosContext, set_value);
            }
            break;
        case QCSAP_PARAM_MAX_ASSOC:
            if (WNI_CFG_ASSOC_STA_LIMIT_STAMIN > set_value)
            {
                hddLog(LOGE, FL("Invalid setMaxAssoc value %d"), set_value);
                ret = -EINVAL;
            }
            else
            {
                if (WNI_CFG_ASSOC_STA_LIMIT_STAMAX < set_value)
                {
                    hddLog(LOGW, FL("setMaxAssoc value %d higher than max allowed %d."
                                "Setting it to max allowed and continuing"),
                                set_value, WNI_CFG_ASSOC_STA_LIMIT_STAMAX);
                    set_value = WNI_CFG_ASSOC_STA_LIMIT_STAMAX;
                }
                status = ccmCfgSetInt(hHal, WNI_CFG_ASSOC_STA_LIMIT,
                                      set_value, NULL, eANI_BOOLEAN_FALSE);
                if ( status != eHAL_STATUS_SUCCESS ) 
                {
                    hddLog(LOGE, FL("setMaxAssoc failure, status %d"),
                            status);
                    ret = -EIO;
                }
            }
            break;

        case QCSAP_PARAM_HIDE_SSID:
            {
                eHalStatus status = eHAL_STATUS_SUCCESS;
                status = sme_HideSSID(hHal, pHostapdAdapter->sessionId, set_value);
                if(eHAL_STATUS_SUCCESS != status)
                {
                    hddLog(VOS_TRACE_LEVEL_ERROR,
                            "%s: QCSAP_PARAM_HIDE_SSID failed",
                            __func__);
                    return status;
                }
                break;
            }

        case QCSAP_PARAM_SET_MC_RATE:
            {
                if (hdd_hostapd_set_mc_rate(pHostapdAdapter, set_value))
                {
                   hddLog(VOS_TRACE_LEVEL_ERROR,
                          "%s: SET_MC_RATE failed", __func__);
                }
                break;
            }

        default:
            hddLog(LOGE, FL("Invalid setparam command %d value %d"),
                    sub_cmd, set_value);
            ret = -EINVAL;
            break;
    }

    return ret;
}


int
static iw_softap_getparam(struct net_device *dev, 
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    int *value = (int *)extra;
    int sub_cmd = value[0];
    eHalStatus status;
    int ret = 0; /* success */
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext; 

    switch (sub_cmd)
    {
    case QCSAP_PARAM_MAX_ASSOC:
        status = ccmCfgGetInt(hHal, WNI_CFG_ASSOC_STA_LIMIT, (tANI_U32 *)value);
        if (eHAL_STATUS_SUCCESS != status)
        {
            ret = -EIO;
        }
        break;
        
    case QCSAP_PARAM_CLR_ACL:
        if ( VOS_STATUS_SUCCESS != WLANSAP_ClearACL( pVosContext ))
        {
               ret = -EIO;            
        }               
        *value = 0;
        break;
        
    case QCSAP_PARAM_MODULE_DOWN_IND:
        {
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s: sending WLAN_MODULE_DOWN_IND", __func__);
            send_btc_nlink_msg(WLAN_MODULE_DOWN_IND, 0);
#ifdef WLAN_BTAMP_FEATURE 
            VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                "%s: Take down AMP PAL", __func__);
            BSL_Deinit(vos_get_global_context(VOS_MODULE_ID_HDD, NULL));
#endif            
            *value = 0;
            break;
        }

    case QCSAP_PARAM_GET_WLAN_DBG:
        {
            vos_trace_display();
            *value = 0;
            break;
        }

    case QCSAP_PARAM_AUTO_CHANNEL:
        {
            *value = (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apAutoChannelSelection;
             break;
        }

    default:
        hddLog(LOGE, FL("Invalid getparam command %d"), sub_cmd);
        ret = -EINVAL;
        break;

    }

    return ret;
}

/* Usage:
    BLACK_LIST  = 0
    WHITE_LIST  = 1 
    ADD MAC = 0
    REMOVE MAC  = 1

    mac addr will be accepted as a 6 octet mac address with each octet inputted in hex
    for e.g. 00:0a:f5:11:22:33 will be represented as 0x00 0x0a 0xf5 0x11 0x22 0x33
    while using this ioctl

    Syntax:
    iwpriv softap.0 modify_acl 
    <6 octet mac addr> <list type> <cmd type>

    Examples:
    eg 1. to add a mac addr 00:0a:f5:89:89:90 to the black list
    iwpriv softap.0 modify_acl 0x00 0x0a 0xf5 0x89 0x89 0x90 0 0
    eg 2. to delete a mac addr 00:0a:f5:89:89:90 from white list
    iwpriv softap.0 modify_acl 0x00 0x0a 0xf5 0x89 0x89 0x90 1 1
*/
int iw_softap_modify_acl(struct net_device *dev, struct iw_request_info *info,
        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext; 
    v_BYTE_t *value = (v_BYTE_t*)extra;
    v_U8_t pPeerStaMac[VOS_MAC_ADDR_SIZE];
    int listType, cmd, i;
    int ret = 0; /* success */

    ENTER();
    for (i=0; i<VOS_MAC_ADDR_SIZE; i++)
    {
        pPeerStaMac[i] = *(value+i);
    }
    listType = (int)(*(value+i));
    i++;
    cmd = (int)(*(value+i));

    hddLog(LOG1, "%s: SAP Modify ACL arg0 %02x:%02x:%02x:%02x:%02x:%02x arg1 %d arg2 %d\n",
            __func__, pPeerStaMac[0], pPeerStaMac[1], pPeerStaMac[2], 
            pPeerStaMac[3], pPeerStaMac[4], pPeerStaMac[5], listType, cmd);

    if (WLANSAP_ModifyACL(pVosContext, pPeerStaMac,(eSapACLType)listType,(eSapACLCmdType)cmd)
            != VOS_STATUS_SUCCESS)
    {
        hddLog(LOGE, FL("Modify ACL failed\n"));
        ret = -EIO;
    }
    EXIT();
    return ret;
}

int
static iw_softap_getchannel(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));

    int *value = (int *)extra;

    *value = (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->operatingChannel;
    return 0;
}

int
static iw_softap_set_max_tx_power(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    int *value = (int *)extra;
    int set_value;
    tSirMacAddr bssid = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    tSirMacAddr selfMac = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

    if (NULL == value)
        return -ENOMEM;

    /* Assign correct slef MAC address */
    vos_mem_copy(bssid, pHostapdAdapter->macAddressCurrent.bytes,
                 VOS_MAC_ADDR_SIZE);
    vos_mem_copy(selfMac, pHostapdAdapter->macAddressCurrent.bytes,
                 VOS_MAC_ADDR_SIZE);

    set_value = value[0];
    if (eHAL_STATUS_SUCCESS != sme_SetMaxTxPower(hHal, bssid, selfMac, set_value))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Setting maximum tx power failed",
                __func__);
        return -EIO;
    }

    return 0;
}

int
static iw_display_data_path_snapshot(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{

    /* Function intitiating dumping states of
     *  HDD(WMM Tx Queues)
     *  TL State (with Per Client infor)
     *  DXE Snapshot (Called at the end of TL Snapshot)
     */
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    hddLog(LOGE, "%s: called for SAP",__func__);
    hdd_wmm_tx_snapshot(pHostapdAdapter);
    WLANTL_TLDebugMessage(VOS_TRUE);
    return 0;
}

int
static iw_softap_set_tx_power(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    int *value = (int *)extra;
    int set_value;
    ptSapContext  pSapCtx = NULL;

    if (NULL == value)
        return -ENOMEM;

    pSapCtx = VOS_GET_SAP_CB(pVosContext);
    if (NULL == pSapCtx)
    {
        VOS_TRACE(VOS_MODULE_ID_SAP, VOS_TRACE_LEVEL_ERROR,
                   "%s: Invalid SAP pointer from pvosGCtx", __func__);
        return VOS_STATUS_E_FAULT;
    }

    set_value = value[0];
    if (eHAL_STATUS_SUCCESS != sme_SetTxPower(hHal, pSapCtx->sessionId, set_value))
    {
        hddLog(VOS_TRACE_LEVEL_ERROR, "%s: Setting tx power failed",
                __func__);
        return -EIO;
    }

    return 0;
}

#define IS_BROADCAST_MAC(x) (((x[0] & x[1] & x[2] & x[3] & x[4] & x[5]) == 0xff) ? 1 : 0)

int
static iw_softap_getassoc_stamacaddr(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    unsigned char *pmaclist;
    hdd_station_info_t *pStaInfo = pHostapdAdapter->aStaInfo;
    int cnt = 0, len;


    pmaclist = wrqu->data.pointer + sizeof(unsigned long int);
    len = wrqu->data.length;

    spin_lock_bh( &pHostapdAdapter->staInfo_lock );
    while((cnt < WLAN_MAX_STA_COUNT) && (len > (sizeof(v_MACADDR_t)+1))) {
        if (TRUE == pStaInfo[cnt].isUsed) {
            
            if(!IS_BROADCAST_MAC(pStaInfo[cnt].macAddrSTA.bytes)) {
                memcpy((void *)pmaclist, (void *)&(pStaInfo[cnt].macAddrSTA), sizeof(v_MACADDR_t));
                pmaclist += sizeof(v_MACADDR_t);
                len -= sizeof(v_MACADDR_t);
            }
        }
        cnt++;
    } 
    spin_unlock_bh( &pHostapdAdapter->staInfo_lock );

    *pmaclist = '\0';

    wrqu->data.length -= len;

    *(unsigned long int *)(wrqu->data.pointer) = wrqu->data.length;

    return 0;
}

/* Usage:
    mac addr will be accepted as a 6 octet mac address with each octet inputted in hex
    for e.g. 00:0a:f5:11:22:33 will be represented as 0x00 0x0a 0xf5 0x11 0x22 0x33
    while using this ioctl

    Syntax:
    iwpriv softap.0 disassoc_sta <6 octet mac address>

    e.g.
    disassociate sta with mac addr 00:0a:f5:11:22:33 from softap
    iwpriv softap.0 disassoc_sta 0x00 0x0a 0xf5 0x11 0x22 0x33
*/

int
static iw_softap_disassoc_sta(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_U8_t *peerMacAddr;    
    
    ENTER();
    /* iwpriv tool or framework calls this ioctl with
     * data passed in extra (less than 16 octets);
     */
    peerMacAddr = (v_U8_t *)(extra);

    hddLog(LOG1, "data %02x:%02x:%02x:%02x:%02x:%02x",
            peerMacAddr[0], peerMacAddr[1], peerMacAddr[2],
            peerMacAddr[3], peerMacAddr[4], peerMacAddr[5]);
    hdd_softap_sta_disassoc(pHostapdAdapter, peerMacAddr);
    EXIT();
    return 0;
}

int
static iw_softap_ap_stats(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    WLANTL_TRANSFER_STA_TYPE  statBuffer;
    char *pstatbuf;
    int len = wrqu->data.length;
    pstatbuf = wrqu->data.pointer;

    WLANSAP_GetStatistics((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext, &statBuffer, (v_BOOL_t)wrqu->data.flags);

    len = scnprintf(pstatbuf, len,
            "RUF=%d RMF=%d RBF=%d "
            "RUB=%d RMB=%d RBB=%d "
            "TUF=%d TMF=%d TBF=%d "
            "TUB=%d TMB=%d TBB=%d",
            (int)statBuffer.rxUCFcnt, (int)statBuffer.rxMCFcnt, (int)statBuffer.rxBCFcnt,
            (int)statBuffer.rxUCBcnt, (int)statBuffer.rxMCBcnt, (int)statBuffer.rxBCBcnt,
            (int)statBuffer.txUCFcnt, (int)statBuffer.txMCFcnt, (int)statBuffer.txBCFcnt,
            (int)statBuffer.txUCBcnt, (int)statBuffer.txMCBcnt, (int)statBuffer.txBCBcnt
            );

    wrqu->data.length -= len;
    return 0;
}

int
static iw_softap_commit(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    VOS_STATUS vos_status = VOS_STATUS_SUCCESS;
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    hdd_hostapd_state_t *pHostapdState;
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext; 
    tpWLAN_SAPEventCB pSapEventCallback;
    tsap_Config_t *pConfig;
    s_CommitConfig_t *pCommitConfig;
    struct qc_mac_acl_entry *acl_entry = NULL;
    v_SINT_t i = 0, num_mac = 0;
    v_U32_t status = 0;
    eCsrAuthType RSNAuthType;
    eCsrEncryptionType RSNEncryptType;
    eCsrEncryptionType mcRSNEncryptType;

    pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);
    pCommitConfig = (s_CommitConfig_t *)extra;
    
    pConfig = kmalloc(sizeof(tsap_Config_t), GFP_KERNEL);
    if(NULL == pConfig) {
        hddLog(LOG1, "VOS unable to allocate memory\n");
        return -ENOMEM;
    }
    pConfig->beacon_int =  pCommitConfig->beacon_int;
    pConfig->channel = pCommitConfig->channel;

    /*Protection parameter to enable or disable*/
    pConfig->protEnabled = (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apProtEnabled;
    pConfig->dtim_period = pCommitConfig->dtim_period;
    switch(pCommitConfig->hw_mode )
    {
        case eQC_DOT11_MODE_11A:
        pConfig->SapHw_mode = eSAP_DOT11_MODE_11a; 
            break;
        case eQC_DOT11_MODE_11B:
        pConfig->SapHw_mode = eSAP_DOT11_MODE_11b; 
            break;
        case eQC_DOT11_MODE_11G:
        pConfig->SapHw_mode = eSAP_DOT11_MODE_11g;
            break;
       
        case eQC_DOT11_MODE_11N:
        pConfig->SapHw_mode = eSAP_DOT11_MODE_11n;
            break;
        case eQC_DOT11_MODE_11G_ONLY:
            pConfig->SapHw_mode = eSAP_DOT11_MODE_11g_ONLY;
            break;
        case eQC_DOT11_MODE_11N_ONLY:
            pConfig->SapHw_mode = eSAP_DOT11_MODE_11n_ONLY;
            break;
        default:
        pConfig->SapHw_mode = eSAP_DOT11_MODE_11n;
            break;
            
    }
  
    pConfig->ieee80211d = pCommitConfig->qcsap80211d;
    vos_mem_copy(pConfig->countryCode, pCommitConfig->countryCode, 3);
    if(pCommitConfig->authType == eQC_AUTH_TYPE_SHARED_KEY)
        pConfig->authType = eSAP_SHARED_KEY;
    else if(pCommitConfig->authType == eQC_AUTH_TYPE_OPEN_SYSTEM) 
        pConfig->authType = eSAP_OPEN_SYSTEM;
    else
        pConfig->authType = eSAP_AUTO_SWITCH;
    
    pConfig->privacy = pCommitConfig->privacy;
    (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy = pCommitConfig->privacy;
    pConfig->wps_state = pCommitConfig->wps_state;
    pConfig->fwdWPSPBCProbeReq  = 1; // Forward WPS PBC probe request frame up 
    pConfig->RSNWPAReqIELength = pCommitConfig->RSNWPAReqIELength;
    if(pConfig->RSNWPAReqIELength){
        pConfig->pRSNWPAReqIE = &pCommitConfig->RSNWPAReqIE[0];
        if ((pConfig->pRSNWPAReqIE[0] == DOT11F_EID_RSN) || (pConfig->pRSNWPAReqIE[0] == DOT11F_EID_WPA)){
            // The actual processing may eventually be more extensive than this.
            // Right now, just consume any PMKIDs that are  sent in by the app.
            status = hdd_softap_unpackIE( 
                                  vos_get_context( VOS_MODULE_ID_PE, pVosContext),
                                  &RSNEncryptType,
                                  &mcRSNEncryptType,
                                  &RSNAuthType,
                                  pConfig->pRSNWPAReqIE[1]+2,
                                  pConfig->pRSNWPAReqIE );
             
            if( VOS_STATUS_SUCCESS == status )
            {
                 // Now copy over all the security attributes you have parsed out
                 //TODO: Need to handle mixed mode     
                 pConfig->RSNEncryptType = RSNEncryptType; // Use the cipher type in the RSN IE
                 pConfig->mcRSNEncryptType = mcRSNEncryptType;
                 hddLog( LOG1, FL("CSR AuthType = %d, EncryptionType = %d mcEncryptionType = %d\n"),
                                  RSNAuthType, RSNEncryptType, mcRSNEncryptType);
             } 
        }
    }
    else
    {
        /* If no RSNIE, set encrypt type to NONE*/
        pConfig->RSNEncryptType = eCSR_ENCRYPT_TYPE_NONE;
        pConfig->mcRSNEncryptType =  eCSR_ENCRYPT_TYPE_NONE;
        hddLog( LOG1, FL("EncryptionType = %d mcEncryptionType = %d\n"), 
                         pConfig->RSNEncryptType, pConfig->mcRSNEncryptType);
    }

    if (pConfig->RSNWPAReqIELength > QCSAP_MAX_OPT_IE) {
        hddLog(LOGE, FL("RSNWPAReqIELength: %d too large"), pConfig->RSNWPAReqIELength);
        kfree(pConfig);
        return -EIO;
    }

    pConfig->SSIDinfo.ssidHidden = pCommitConfig->SSIDinfo.ssidHidden; 
    pConfig->SSIDinfo.ssid.length = pCommitConfig->SSIDinfo.ssid.length;
    vos_mem_copy(pConfig->SSIDinfo.ssid.ssId, pCommitConfig->SSIDinfo.ssid.ssId, pConfig->SSIDinfo.ssid.length);
    vos_mem_copy(pConfig->self_macaddr.bytes, pHostapdAdapter->macAddressCurrent.bytes, sizeof(v_MACADDR_t));
    
    pConfig->SapMacaddr_acl = pCommitConfig->qc_macaddr_acl;

    // ht_capab is not what the name conveys,this is used for protection bitmap
    pConfig->ht_capab = (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apProtection;

    if (pCommitConfig->num_accept_mac > MAX_ACL_MAC_ADDRESS)
        num_mac = pConfig->num_accept_mac = MAX_ACL_MAC_ADDRESS;
    else
        num_mac = pConfig->num_accept_mac = pCommitConfig->num_accept_mac;
    acl_entry = pCommitConfig->accept_mac;
    for (i = 0; i < num_mac; i++)
    {
        vos_mem_copy(&pConfig->accept_mac[i], acl_entry->addr, sizeof(v_MACADDR_t));
        acl_entry++;
    }
    if (pCommitConfig->num_deny_mac > MAX_ACL_MAC_ADDRESS)
        num_mac = pConfig->num_deny_mac = MAX_ACL_MAC_ADDRESS;
    else
        num_mac = pConfig->num_deny_mac = pCommitConfig->num_deny_mac;
    acl_entry = pCommitConfig->deny_mac;
    for (i = 0; i < num_mac; i++)
    {
        vos_mem_copy(&pConfig->deny_mac[i], acl_entry->addr, sizeof(v_MACADDR_t));
        acl_entry++;
    }
    //Uapsd Enabled Bit
    pConfig->UapsdEnable =  (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apUapsdEnabled;
    //Enable OBSS protection
    pConfig->obssProtEnabled = (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apOBSSProtEnabled; 
    (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->apDisableIntraBssFwd = (WLAN_HDD_GET_CTX(pHostapdAdapter))->cfg_ini->apDisableIntraBssFwd;
    
    hddLog(LOGW, FL("SOftAP macaddress : "MAC_ADDRESS_STR"\n"), MAC_ADDR_ARRAY(pHostapdAdapter->macAddressCurrent.bytes));
    hddLog(LOGW,FL("ssid =%s\n"), pConfig->SSIDinfo.ssid.ssId);  
    hddLog(LOGW,FL("beaconint=%d, channel=%d\n"), (int)pConfig->beacon_int, (int)pConfig->channel);
    hddLog(LOGW,FL("hw_mode=%x\n"),  pConfig->SapHw_mode);
    hddLog(LOGW,FL("privacy=%d, authType=%d\n"), pConfig->privacy, pConfig->authType); 
    hddLog(LOGW,FL("RSN/WPALen=%d, \n"),(int)pConfig->RSNWPAReqIELength);
    hddLog(LOGW,FL("Uapsd = %d\n"),pConfig->UapsdEnable); 
    hddLog(LOGW,FL("ProtEnabled = %d, OBSSProtEnabled = %d\n"),pConfig->protEnabled, pConfig->obssProtEnabled); 
    hddLog(LOGW,FL("DisableIntraBssFwd = %d\n"),(WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->apDisableIntraBssFwd); 
            
    pSapEventCallback = hdd_hostapd_SAPEventCB;
    pConfig->persona = pHostapdAdapter->device_mode;
    if(WLANSAP_StartBss(pVosContext, pSapEventCallback, pConfig,(v_PVOID_t)dev) != VOS_STATUS_SUCCESS)
    {
           hddLog(LOGE,FL("SAP Start Bss fail\n"));
    }
    
    kfree(pConfig);
    
    hddLog(LOG1, FL("Waiting for Scan to complete(auto mode) and BSS to start"));
    vos_status = vos_wait_single_event(&pHostapdState->vosEvent, 10000);
   
    if (!VOS_IS_STATUS_SUCCESS(vos_status))
    {  
       VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: HDD vos wait for single_event failed!!\n"));
       VOS_ASSERT(0);
    }
 
    pHostapdState->bCommit = TRUE;
    if(pHostapdState->vosStatus)
    {
      return -EIO;
    }
    else
    {
        set_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
        WLANSAP_Update_WpsIe ( pVosContext );            
        return 0;
    }
}
static 
int iw_softap_setmlme(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    struct sQcSapreq_mlme *pmlme;
    hdd_adapter_t *pHostapdAdapter = (hdd_adapter_t*)(netdev_priv(dev));
    v_MACADDR_t destAddress;
    pmlme = (struct sQcSapreq_mlme *)(wrqu->name);
    /* NOTE: this address is not valid incase of TKIP failure, since not filled */
    vos_mem_copy(&destAddress.bytes, pmlme->im_macaddr, sizeof(v_MACADDR_t));
    switch(pmlme->im_op)
    {
        case QCSAP_MLME_AUTHORIZE:
                    hdd_softap_change_STA_state( pHostapdAdapter, &destAddress, WLANTL_STA_AUTHENTICATED);
        break;
        case QCSAP_MLME_ASSOC:
        //TODO:inform to TL after associating (not needed  as we do in sapCallback)
        break;
        case QCSAP_MLME_UNAUTHORIZE:
        //TODO: send the disassoc to station
        //hdd_softap_change_STA_state( pHostapdAdapter, pmlme->im_macaddr, WLANTL_STA_AUTHENTICATED);
        break;
        case QCSAP_MLME_DISASSOC:
            hdd_softap_sta_disassoc(pHostapdAdapter,pmlme->im_macaddr);
        break;
        case QCSAP_MLME_DEAUTH:
            hdd_softap_sta_deauth(pHostapdAdapter,pmlme->im_macaddr);
        break;
        case QCSAP_MLME_MICFAILURE:
            hdd_softap_tkip_mic_fail_counter_measure(pHostapdAdapter,pmlme->im_reason);
        break;
        default:
        break;
    }
    return 0;
}

static int iw_softap_set_channel_range(struct net_device *dev, 
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);

    int *value = (int *)extra;
    int startChannel = value[0];
    int endChannel = value[1];
    int band = value[2];
    VOS_STATUS status;
    int ret = 0; /* success */

    status = WLANSAP_SetChannelRange(hHal,startChannel,endChannel,band);
    if(status != VOS_STATUS_SUCCESS)
    {
      hddLog( LOGE, FL("iw_softap_set_channel_range:  startChannel = %d, endChannel = %d band = %d\n"), 
                                  startChannel,endChannel, band);
      ret = -EINVAL;
    }

    pHddCtx->is_dynamic_channel_range_set = 1;

    return ret;
}

int iw_softap_get_channel_list(struct net_device *dev, 
                          struct iw_request_info *info,
                          union iwreq_data *wrqu, char *extra)
{
    v_U32_t num_channels = 0;
    v_U8_t i = 0;
    v_U8_t bandStartChannel = RF_CHAN_1;
    v_U8_t bandEndChannel = RF_CHAN_165;
    v_U32_t temp_num_channels = 0;
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    tHalHandle hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);
    v_REGDOMAIN_t domainIdCurrentSoftap;
    tpChannelListInfo channel_list = (tpChannelListInfo) extra;
    eCsrBand curBand = eCSR_BAND_ALL;

    if (eHAL_STATUS_SUCCESS != sme_GetFreqBand(hHal, &curBand))
    {
        hddLog(LOGE,FL("not able get the current frequency band\n"));
        return -EIO;
    }
    wrqu->data.length = sizeof(tChannelListInfo);
    ENTER();

    if (eCSR_BAND_24 == curBand)
    {
        bandStartChannel = RF_CHAN_1;
        bandEndChannel = RF_CHAN_14;
    }
    else if (eCSR_BAND_5G == curBand)
    {
        bandStartChannel = RF_CHAN_36;
        bandEndChannel = RF_CHAN_165;
    }

    hddLog(LOG1, FL("\n curBand = %d, bandStartChannel = %hu, "
                "bandEndChannel = %hu "), curBand,
                bandStartChannel, bandEndChannel );

    for( i = bandStartChannel; i <= bandEndChannel; i++ )
    {
        if( NV_CHANNEL_ENABLE == regChannels[i].enabled )
        {
            channel_list->channels[num_channels] = rfChannels[i].channelNum; 
            num_channels++;
        }
    }

    /* remove indoor channels if the domain is FCC, channels 36 - 48 */

    temp_num_channels = num_channels;

    if(eHAL_STATUS_SUCCESS != sme_getSoftApDomain(hHal,(v_REGDOMAIN_t *) &domainIdCurrentSoftap))
    {
        hddLog(LOG1,FL("Failed to get Domain ID, %d \n"),domainIdCurrentSoftap);
        return -EIO;
    }

    if(REGDOMAIN_FCC == domainIdCurrentSoftap)
    {
        for(i = 0; i < temp_num_channels; i++)
        {
      
           if((channel_list->channels[i] > 35) && 
              (channel_list->channels[i] < 49))
           {
               vos_mem_move(&channel_list->channels[i], 
                            &channel_list->channels[i+1], 
                            temp_num_channels - (i-1));
               num_channels--;
               temp_num_channels--;
               i--;
           } 
        }
    }

    hddLog(LOG1,FL(" number of channels %d\n"), num_channels); 

    if (num_channels > IW_MAX_FREQUENCIES)
    {
        num_channels = IW_MAX_FREQUENCIES;
    }

    channel_list->num_channels = num_channels;
    EXIT();

    return 0;
}

static 
int iw_get_genie(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext; 
    eHalStatus status;
    v_U32_t length = DOT11F_IE_RSN_MAX_LEN;
    v_U8_t genIeBytes[DOT11F_IE_RSN_MAX_LEN];
    ENTER();
    hddLog(LOG1,FL("getGEN_IE ioctl\n"));
    // Actually retrieve the RSN IE from CSR.  (We previously sent it down in the CSR Roam Profile.)
    status = WLANSap_getstationIE_information(pVosContext, 
                                   &length,
                                   genIeBytes);
    wrqu->data.length = VOS_MIN((u_int16_t) length, DOT11F_IE_RSN_MAX_LEN);
    vos_mem_copy( wrqu->data.pointer, (v_VOID_t*)genIeBytes, wrqu->data.length);
    
    hddLog(LOG1,FL(" RSN IE of %d bytes returned\n"), wrqu->data.length ); 
    
   
    EXIT();
    return 0;
}
static 
int iw_get_WPSPBCProbeReqIEs(struct net_device *dev,
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));    
    sQcSapreq_WPSPBCProbeReqIES_t *pWPSPBCProbeReqIEs;
    hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
    ENTER();
        
    hddLog(LOG1,FL("get_WPSPBCProbeReqIEs ioctl\n"));
    
    pWPSPBCProbeReqIEs = (sQcSapreq_WPSPBCProbeReqIES_t *)(wrqu->data.pointer);
    pWPSPBCProbeReqIEs->probeReqIELen = pHddApCtx->WPSPBCProbeReq.probeReqIELen;
    vos_mem_copy(pWPSPBCProbeReqIEs->probeReqIE, pHddApCtx->WPSPBCProbeReq.probeReqIE, pWPSPBCProbeReqIEs->probeReqIELen);
    vos_mem_copy(pWPSPBCProbeReqIEs->macaddr, pHddApCtx->WPSPBCProbeReq.peerMacAddr, sizeof(v_MACADDR_t));
    wrqu->data.length = 12 + pWPSPBCProbeReqIEs->probeReqIELen;
    hddLog(LOG1, FL("Macaddress : "MAC_ADDRESS_STR"\n"),  MAC_ADDR_ARRAY(pWPSPBCProbeReqIEs->macaddr));
    up(&pHddApCtx->semWpsPBCOverlapInd);
    EXIT();
    return 0;
}

/**---------------------------------------------------------------------------
  
  \brief iw_set_auth_hostap() - 
   This function sets the auth type received from the wpa_supplicant.
   
  \param  - dev - Pointer to the net device.
              - info - Pointer to the iw_request_info.
              - wrqu - Pointer to the iwreq_data.
              - extra - Pointer to the data.        
  \return - 0 for success, non zero for failure
  
  --------------------------------------------------------------------------*/
int iw_set_auth_hostap(struct net_device *dev,struct iw_request_info *info,
                        union iwreq_data *wrqu,char *extra)
{
   hdd_adapter_t *pAdapter = WLAN_HDD_GET_PRIV_PTR(dev);
   hdd_wext_state_t *pWextState = WLAN_HDD_GET_WEXT_STATE_PTR(pAdapter); 

   ENTER();
   switch(wrqu->param.flags & IW_AUTH_INDEX)
   {
      case IW_AUTH_TKIP_COUNTERMEASURES:
      {
         if(wrqu->param.value) {
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                   "Counter Measure started %d", wrqu->param.value);
            pWextState->mTKIPCounterMeasures = TKIP_COUNTER_MEASURE_STARTED;
         }  
         else {   
            hddLog(VOS_TRACE_LEVEL_INFO_HIGH,
                   "Counter Measure stopped=%d", wrqu->param.value);
            pWextState->mTKIPCounterMeasures = TKIP_COUNTER_MEASURE_STOPED;
         }

         hdd_softap_tkip_mic_fail_counter_measure(pAdapter,
                                                  wrqu->param.value);
      }   
      break;
         
      default:
         
         hddLog(LOGW, "%s called with unsupported auth type %d", __func__, 
               wrqu->param.flags & IW_AUTH_INDEX);
      break;
   }
   
   EXIT();
   return 0;
}

static int iw_set_ap_encodeext(struct net_device *dev, 
                        struct iw_request_info *info,
                        union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;    
    hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);
    int retval = 0;
    VOS_STATUS vstatus;
    struct iw_encode_ext *ext = (struct iw_encode_ext*)extra;
    v_U8_t groupmacaddr[WNI_CFG_BSSID_LEN] = {0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};
    int key_index;
    struct iw_point *encoding = &wrqu->encoding;
    tCsrRoamSetKey  setKey;   
//    tCsrRoamRemoveKey RemoveKey;
    int i;

    ENTER();    
   
    key_index = encoding->flags & IW_ENCODE_INDEX;
   
    if(key_index > 0) {
      
         /*Convert from 1-based to 0-based keying*/
        key_index--;
    }
    if(!ext->key_len) {
#if 0     
      /*Set the encrytion type to NONE*/
#if 0
       pRoamProfile->EncryptionType.encryptionType[0] = eCSR_ENCRYPT_TYPE_NONE;
#endif
     
         RemoveKey.keyId = key_index;
         if(ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
              /*Key direction for group is RX only*/
             vos_mem_copy(RemoveKey.peerMac,groupmacaddr,WNI_CFG_BSSID_LEN);
         }
         else {
             vos_mem_copy(RemoveKey.peerMac,ext->addr.sa_data,WNI_CFG_BSSID_LEN);
         }
         switch(ext->alg)
         {
           case IW_ENCODE_ALG_NONE:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_NONE;
              break;
           case IW_ENCODE_ALG_WEP:
              RemoveKey.encType = (ext->key_len== 5) ? eCSR_ENCRYPT_TYPE_WEP40:eCSR_ENCRYPT_TYPE_WEP104;
              break;
           case IW_ENCODE_ALG_TKIP:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_TKIP;
              break;
           case IW_ENCODE_ALG_CCMP:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_AES;
              break;
          default:
              RemoveKey.encType = eCSR_ENCRYPT_TYPE_NONE;
              break;
         }
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: Remove key cipher_alg:%d key_len%d *pEncryptionType :%d \n",
                    __func__,(int)ext->alg,(int)ext->key_len,RemoveKey.encType);
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO, "%s: Peer Mac = "MAC_ADDRESS_STR"\n",
                    __func__, MAC_ADDR_ARRAY(RemoveKey.peerMac));
          );
         vstatus = WLANSAP_DelKeySta( pVosContext, &RemoveKey);
         if ( vstatus != VOS_STATUS_SUCCESS )
         {
             VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, "[%4d] WLANSAP_DeleteKeysSta returned ERROR status= %d",
                        __LINE__, vstatus );
             retval = -EINVAL;
         }
#endif
         return retval;

    }
    
    vos_mem_zero(&setKey,sizeof(tCsrRoamSetKey));
   
    setKey.keyId = key_index;
    setKey.keyLength = ext->key_len;
   
    if(ext->key_len <= CSR_MAX_KEY_LEN) {
       vos_mem_copy(&setKey.Key[0],ext->key,ext->key_len);
    }   
   
    if(ext->ext_flags & IW_ENCODE_EXT_GROUP_KEY) {
      /*Key direction for group is RX only*/
       setKey.keyDirection = eSIR_RX_ONLY;
       vos_mem_copy(setKey.peerMac,groupmacaddr,WNI_CFG_BSSID_LEN);
    }
    else {   
      
       setKey.keyDirection =  eSIR_TX_RX;
       vos_mem_copy(setKey.peerMac,ext->addr.sa_data,WNI_CFG_BSSID_LEN);
    }
    if(ext->ext_flags & IW_ENCODE_EXT_SET_TX_KEY)
    {
       setKey.keyDirection = eSIR_TX_DEFAULT;
       vos_mem_copy(setKey.peerMac,ext->addr.sa_data,WNI_CFG_BSSID_LEN);
    }
 
    /*For supplicant pae role is zero*/
    setKey.paeRole = 0;
      
    switch(ext->alg)
    {   
       case IW_ENCODE_ALG_NONE:   
         setKey.encType = eCSR_ENCRYPT_TYPE_NONE;
         break;
         
       case IW_ENCODE_ALG_WEP:
         setKey.encType = (ext->key_len== 5) ? eCSR_ENCRYPT_TYPE_WEP40:eCSR_ENCRYPT_TYPE_WEP104;
         pHddApCtx->uPrivacy = 1;
         hddLog(LOG1, "(%s) uPrivacy=%d", __func__, pHddApCtx->uPrivacy);
         break;
      
       case IW_ENCODE_ALG_TKIP:
       {
          v_U8_t *pKey = &setKey.Key[0];
  
          setKey.encType = eCSR_ENCRYPT_TYPE_TKIP;
  
          vos_mem_zero(pKey, CSR_MAX_KEY_LEN);
  
          /*Supplicant sends the 32bytes key in this order 
          
                |--------------|----------|----------|
                |   Tk1        |TX-MIC    |  RX Mic  | 
                |--------------|----------|----------|
                <---16bytes---><--8bytes--><--8bytes-->
                
                */
          /*Sme expects the 32 bytes key to be in the below order
  
                |--------------|----------|----------|
                |   Tk1        |RX-MIC    |  TX Mic  | 
                |--------------|----------|----------|
                <---16bytes---><--8bytes--><--8bytes-->
               */
          /* Copy the Temporal Key 1 (TK1) */
          vos_mem_copy(pKey,ext->key,16);
           
         /*Copy the rx mic first*/
          vos_mem_copy(&pKey[16],&ext->key[24],8); 
          
         /*Copy the tx mic */
          vos_mem_copy(&pKey[24],&ext->key[16],8); 
  
       }     
       break;
      
       case IW_ENCODE_ALG_CCMP:
          setKey.encType = eCSR_ENCRYPT_TYPE_AES;
          break;
          
       default:
          setKey.encType = eCSR_ENCRYPT_TYPE_NONE;
          break;
    }
         
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
          ("%s:EncryptionType:%d key_len:%d, KeyId:%d"), __func__, setKey.encType, setKey.keyLength,
            setKey.keyId);
    for(i=0; i< ext->key_len; i++)
        VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
          ("%02x"), setKey.Key[i]);    
    VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
          ("\n"));

    vstatus = WLANSAP_SetKeySta( pVosContext, &setKey);
    if ( vstatus != VOS_STATUS_SUCCESS )
    {
       VOS_TRACE( VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR,
                   "[%4d] WLANSAP_SetKeySta returned ERROR status= %d", __LINE__, vstatus );
       retval = -EINVAL;
    }
   
   return retval;
}


static int iw_set_ap_mlme(struct net_device *dev,
                       struct iw_request_info *info,
                       union iwreq_data *wrqu,
                       char *extra)
{
#if 0
    hdd_adapter_t *pAdapter = (netdev_priv(dev));
    struct iw_mlme *mlme = (struct iw_mlme *)extra;
 
    ENTER();    
   
    //reason_code is unused. By default it is set to eCSR_DISCONNECT_REASON_UNSPECIFIED
    switch (mlme->cmd) {
        case IW_MLME_DISASSOC:
        case IW_MLME_DEAUTH:
            hddLog(LOG1, "Station disassociate");    
            if( pAdapter->conn_info.connState == eConnectionState_Associated ) 
            {
                eCsrRoamDisconnectReason reason = eCSR_DISCONNECT_REASON_UNSPECIFIED;
                
                if( mlme->reason_code == HDD_REASON_MICHAEL_MIC_FAILURE )
                    reason = eCSR_DISCONNECT_REASON_MIC_ERROR;
                
                status = sme_RoamDisconnect( pAdapter->hHal,pAdapter->sessionId, reason);
                
                //clear all the reason codes
                if (status != 0)
                {
                    hddLog(LOGE,"%s %d Command Disassociate/Deauthenticate : csrRoamDisconnect failure returned %d \n", __func__, (int)mlme->cmd, (int)status );
                }
                
               netif_stop_queue(dev);
               netif_carrier_off(dev);
            }
            else
            {
                hddLog(LOGE,"%s %d Command Disassociate/Deauthenticate called but station is not in associated state \n", __func__, (int)mlme->cmd );
            }
        default:
            hddLog(LOGE,"%s %d Command should be Disassociate/Deauthenticate \n", __func__, (int)mlme->cmd );
            return -EINVAL;
    }//end of switch
    EXIT();
#endif    
    return 0;
//    return status;
}

static int iw_get_ap_rts_threshold(struct net_device *dev,
            struct iw_request_info *info,
            union iwreq_data *wrqu, char *extra)
{
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
   v_U32_t status = 0;

   status = hdd_wlan_get_rts_threshold(pHostapdAdapter, wrqu);

   return status;
}

static int iw_get_ap_frag_threshold(struct net_device *dev,
                                 struct iw_request_info *info,
                                 union iwreq_data *wrqu, char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_U32_t status = 0;

    status = hdd_wlan_get_frag_threshold(pHostapdAdapter, wrqu);

    return status;
}

static int iw_get_ap_freq(struct net_device *dev, struct iw_request_info *info,
             struct iw_freq *fwrq, char *extra)
{
   v_U32_t status = FALSE, channel = 0, freq = 0;
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
   tHalHandle hHal;
   hdd_hostapd_state_t *pHostapdState;
   hdd_ap_ctx_t *pHddApCtx = WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter);

   ENTER();

   if ((WLAN_HDD_GET_CTX(pHostapdAdapter))->isLogpInProgress) {
      VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_INFO,
                                  "%s:LOGP in Progress. Ignore!!!",__func__);
      return status;
   }

   pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);
   hHal = WLAN_HDD_GET_HAL_CTX(pHostapdAdapter);

   if(pHostapdState->bssState == BSS_STOP )
   {
       if (ccmCfgGetInt(hHal, WNI_CFG_CURRENT_CHANNEL, &channel)
                                                  != eHAL_STATUS_SUCCESS)
       {
           return -EIO;
       }
       else
       {
          status = hdd_wlan_get_freq(channel, &freq);
          if( TRUE == status)
          {
              /* Set Exponent parameter as 6 (MHZ) in struct iw_freq
               * iwlist & iwconfig command shows frequency into proper
               * format (2.412 GHz instead of 246.2 MHz)*/
              fwrq->m = freq;
              fwrq->e = MHZ;
          }
       }
    }
    else
    {
       channel = pHddApCtx->operatingChannel;
       status = hdd_wlan_get_freq(channel, &freq);
       if( TRUE == status)
       {
          /* Set Exponent parameter as 6 (MHZ) in struct iw_freq
           * iwlist & iwconfig command shows frequency into proper
           * format (2.412 GHz instead of 246.2 MHz)*/
           fwrq->m = freq;
           fwrq->e = MHZ;
       }
    }
   return 0;
}

static int iw_softap_setwpsie(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu, 
        char *extra)
{
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
   v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
   hdd_hostapd_state_t *pHostapdState;
   eHalStatus halStatus= eHAL_STATUS_SUCCESS;
   u_int8_t *wps_genie =  wrqu->data.pointer;
   u_int8_t *pos;
   tpSap_WPSIE pSap_WPSIe;
   u_int8_t WPSIeType;
   u_int16_t length;   
   ENTER();

   if(!wrqu->data.length)
      return 0;

   pSap_WPSIe = vos_mem_malloc(sizeof(tSap_WPSIE));
   if (NULL == pSap_WPSIe) 
   {
      hddLog(LOGE, "VOS unable to allocate memory\n");
      return -ENOMEM;
   }
   vos_mem_zero(pSap_WPSIe, sizeof(tSap_WPSIE));
 
   hddLog(LOG1,"%s WPS IE type[0x%X] IE[0x%X], LEN[%d]\n", __func__, wps_genie[0], wps_genie[1], wps_genie[2]);
   WPSIeType = wps_genie[0];
   if ( wps_genie[0] == eQC_WPS_BEACON_IE)
   {
      pSap_WPSIe->sapWPSIECode = eSAP_WPS_BEACON_IE; 
      wps_genie = wps_genie + 1;
      switch ( wps_genie[0] ) 
      {
         case DOT11F_EID_WPA: 
            if (wps_genie[1] < 2 + 4)
            {
               vos_mem_free(pSap_WPSIe); 
               return -EINVAL;
            }
            else if (memcmp(&wps_genie[2], "\x00\x50\xf2\x04", 4) == 0) 
            {
             hddLog (LOG1, "%s Set WPS BEACON IE(len %d)",__func__, wps_genie[1]+2);
             pos = &wps_genie[6];
             while (((size_t)pos - (size_t)&wps_genie[6])  < (wps_genie[1] - 4) )
             {
                switch((u_int16_t)(*pos<<8) | *(pos+1))
                {
                   case HDD_WPS_ELEM_VERSION:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.Version = *pos;   
                      hddLog(LOG1, "WPS version %d\n", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.Version);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_VER_PRESENT;   
                      pos += 1;
                      break;
                   
                   case HDD_WPS_ELEM_WPS_STATE:
                      pos +=4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.wpsState = *pos;
                      hddLog(LOG1, "WPS State %d\n", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.wpsState);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_STATE_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_APSETUPLOCK:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.APSetupLocked = *pos;
                      hddLog(LOG1, "AP setup lock %d\n", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.APSetupLocked);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_APSETUPLOCK_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_SELECTEDREGISTRA:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistra = *pos;
                      hddLog(LOG1, "Selected Registra %d\n", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistra);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_SELECTEDREGISTRA_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_DEVICE_PASSWORD_ID:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.DevicePasswordID = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Password ID: %x\n", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.DevicePasswordID);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_DEVICEPASSWORDID_PRESENT;
                      pos += 2; 
                      break;
                   case HDD_WPS_ELEM_REGISTRA_CONF_METHODS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistraCfgMethod = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Select Registra Config Methods: %x\n", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.SelectedRegistraCfgMethod);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_SELECTEDREGISTRACFGMETHOD_PRESENT;
                      pos += 2; 
                      break;
                
                   case HDD_WPS_ELEM_UUID_E:
                      pos += 2; 
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSBeaconIE.UUID_E, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_UUIDE_PRESENT; 
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_RF_BANDS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.RFBand = *pos;
                      hddLog(LOG1, "RF band: %d\n", pSap_WPSIe->sapwpsie.sapWPSBeaconIE.RFBand);
                      pSap_WPSIe->sapwpsie.sapWPSBeaconIE.FieldPresent |= WPS_BEACON_RF_BANDS_PRESENT;
                      pos += 1;
                      break;
                   
                   default:
                      hddLog (LOGW, "UNKNOWN TLV in WPS IE(%x)\n", (*pos<<8 | *(pos+1)));
                      vos_mem_free(pSap_WPSIe);
                      return -EINVAL; 
                }
              }  
            }
            else { 
                 hddLog (LOGE, "%s WPS IE Mismatch %X",
                         __func__, wps_genie[0]);
            }     
            break;
                 
         default:
            hddLog (LOGE, "%s Set UNKNOWN IE %X",__func__, wps_genie[0]);
            vos_mem_free(pSap_WPSIe);
            return 0;
      }
    } 
    else if( wps_genie[0] == eQC_WPS_PROBE_RSP_IE)
    {
      pSap_WPSIe->sapWPSIECode = eSAP_WPS_PROBE_RSP_IE; 
      wps_genie = wps_genie + 1;
      switch ( wps_genie[0] ) 
      {
         case DOT11F_EID_WPA: 
            if (wps_genie[1] < 2 + 4)
            {
               vos_mem_free(pSap_WPSIe); 
               return -EINVAL;
            }
            else if (memcmp(&wps_genie[2], "\x00\x50\xf2\x04", 4) == 0) 
            {
             hddLog (LOG1, "%s Set WPS PROBE RSP IE(len %d)",__func__, wps_genie[1]+2);
             pos = &wps_genie[6];
             while (((size_t)pos - (size_t)&wps_genie[6])  < (wps_genie[1] - 4) )
             {
              switch((u_int16_t)(*pos<<8) | *(pos+1))
              {
                   case HDD_WPS_ELEM_VERSION:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Version = *pos;   
                      hddLog(LOG1, "WPS version %d\n", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Version); 
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_VER_PRESENT;   
                      pos += 1;
                      break;
                   
                   case HDD_WPS_ELEM_WPS_STATE:
                      pos +=4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.wpsState = *pos;
                      hddLog(LOG1, "WPS State %d\n", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.wpsState);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_STATE_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_APSETUPLOCK:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.APSetupLocked = *pos;
                      hddLog(LOG1, "AP setup lock %d\n", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.APSetupLocked);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_APSETUPLOCK_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_SELECTEDREGISTRA:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistra = *pos;
                      hddLog(LOG1, "Selected Registra %d\n", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistra);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_SELECTEDREGISTRA_PRESENT;                      
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_DEVICE_PASSWORD_ID:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DevicePasswordID = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Password ID: %d\n", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DevicePasswordID);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_DEVICEPASSWORDID_PRESENT;
                      pos += 2; 
                      break;
                   case HDD_WPS_ELEM_REGISTRA_CONF_METHODS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistraCfgMethod = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Select Registra Config Methods: %x\n", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistraCfgMethod);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_SELECTEDREGISTRACFGMETHOD_PRESENT;
                      pos += 2; 
                      break;
                  case HDD_WPS_ELEM_RSP_TYPE:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ResponseType = *pos;
                      hddLog(LOG1, "Config Methods: %d\n", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ResponseType);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_RESPONSETYPE_PRESENT;
                      pos += 1;
                      break;
                   case HDD_WPS_ELEM_UUID_E:
                      pos += 2; 
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.UUID_E, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_UUIDE_PRESENT;
                      pos += length;
                      break;
                   
                   case HDD_WPS_ELEM_MANUFACTURER:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Manufacture.num_name = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.Manufacture.name, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_MANUFACTURE_PRESENT;
                      pos += length;
                      break;
 
                   case HDD_WPS_ELEM_MODEL_NAME:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelName.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelName.text, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_MODELNAME_PRESENT;
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_MODEL_NUM:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelNumber.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ModelNumber.text, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_MODELNUMBER_PRESENT;
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_SERIAL_NUM:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SerialNumber.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SerialNumber.text, pos, length);
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_SERIALNUMBER_PRESENT;
                      pos += length;
                      break;
                   case HDD_WPS_ELEM_PRIMARY_DEVICE_TYPE:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.PrimaryDeviceCategory = (*pos<<8 | *(pos+1));
                      hddLog(LOG1, "primary dev category: %d\n", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.PrimaryDeviceCategory);  
                      pos += 2;
                      
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.PrimaryDeviceOUI, pos, HDD_WPS_DEVICE_OUI_LEN);
                      hddLog(LOG1, "primary dev oui: %02x, %02x, %02x, %02x\n", pos[0], pos[1], pos[2], pos[3]);
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceSubCategory = (*pos<<8 | *(pos+1));
                      hddLog(LOG1, "primary dev sub category: %d\n", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceSubCategory);  
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_PRIMARYDEVICETYPE_PRESENT;                      
                      break;
                   case HDD_WPS_ELEM_DEVICE_NAME:
                      pos += 2;
                      length = *pos<<8 | *(pos+1);
                      pos += 2;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceName.num_text = length;
                      vos_mem_copy(pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.DeviceName.text, pos, length);
                      pos += length;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_DEVICENAME_PRESENT;
                      break;
                   case HDD_WPS_ELEM_CONFIG_METHODS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.ConfigMethod = (*pos<<8) | *(pos+1);
                      hddLog(LOG1, "Config Methods: %d\n", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.SelectedRegistraCfgMethod);
                      pos += 2; 
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_CONFIGMETHODS_PRESENT;
                      break;
 
                   case HDD_WPS_ELEM_RF_BANDS:
                      pos += 4;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.RFBand = *pos;
                      hddLog(LOG1, "RF band: %d\n", pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.RFBand);
                      pos += 1;
                      pSap_WPSIe->sapwpsie.sapWPSProbeRspIE.FieldPresent |= WPS_PROBRSP_RF_BANDS_PRESENT;
                      break;
              }  // switch
            }
         } 
         else
         {
            hddLog (LOGE, "%s WPS IE Mismatch %X",__func__, wps_genie[0]);
         }
         
      } // switch
    }
    halStatus = WLANSAP_Set_WpsIe(pVosContext, pSap_WPSIe);
    pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);
    if( pHostapdState->bCommit && WPSIeType == eQC_WPS_PROBE_RSP_IE)
    {
        //hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
        //v_CONTEXT_t pVosContext = pHostapdAdapter->pvosContext;
        WLANSAP_Update_WpsIe ( pVosContext );
    }
 
    vos_mem_free(pSap_WPSIe);   
    EXIT();
    return halStatus;
}

static int iw_softap_stopbss(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu, 
        char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    VOS_STATUS status = VOS_STATUS_SUCCESS;
    ENTER();
    if(test_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags)) 
    {
        if ( VOS_STATUS_SUCCESS == (status = WLANSAP_StopBss((WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext) ) )
        {
            hdd_hostapd_state_t *pHostapdState = WLAN_HDD_GET_HOSTAP_STATE_PTR(pHostapdAdapter);

            status = vos_wait_single_event(&pHostapdState->vosEvent, 10000);
   
            if (!VOS_IS_STATUS_SUCCESS(status))
            {  
                VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, 
                         ("ERROR: HDD vos wait for single_event failed!!\n"));
                VOS_ASSERT(0);
            }
        }
        clear_bit(SOFTAP_BSS_STARTED, &pHostapdAdapter->event_flags);
    }
    EXIT();
    return (status == VOS_STATUS_SUCCESS) ? 0 : -EBUSY;
}

static int iw_softap_version(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));

    ENTER();
    hdd_wlan_get_version(pHostapdAdapter, wrqu, extra);
    EXIT();
    return 0;
}

VOS_STATUS hdd_softap_get_sta_info(hdd_adapter_t *pAdapter, v_U8_t *pBuf, int buf_len)
{
    v_U8_t i;
    int len = 0;
    const char sta_info_header[] = "staId staAddress\n";

    len = scnprintf(pBuf, buf_len, sta_info_header);
    pBuf += len;
    buf_len -= len;

    for (i = 0; i < WLAN_MAX_STA_COUNT; i++)
    {
        if(pAdapter->aStaInfo[i].isUsed)
        {
            len = scnprintf(pBuf, buf_len, "%*d .%02x:%02x:%02x:%02x:%02x:%02x\n",
                                       strlen("staId"),
                                       pAdapter->aStaInfo[i].ucSTAId,
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[0],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[1],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[2],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[3],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[4],
                                       pAdapter->aStaInfo[i].macAddrSTA.bytes[5]);
            pBuf += len;
            buf_len -= len;
        }
        if(WE_GET_STA_INFO_SIZE > buf_len)
        {
            break;
        }
    }
    return VOS_STATUS_SUCCESS;
}

static int iw_softap_get_sta_info(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu, 
        char *extra)
{
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    VOS_STATUS status;
    ENTER();
    status = hdd_softap_get_sta_info(pHostapdAdapter, extra, WE_SAP_MAX_STA_INFO);
    if ( !VOS_IS_STATUS_SUCCESS( status ) ) {
       hddLog(VOS_TRACE_LEVEL_ERROR, "%s Failed!!!\n",__func__);
       return -EINVAL;
    }
    wrqu->data.length = strlen(extra);
    EXIT();
    return 0;
}

static int iw_set_ap_genie(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu, 
        char *extra)
{
 
    hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
    v_CONTEXT_t pVosContext = (WLAN_HDD_GET_CTX(pHostapdAdapter))->pvosContext;
    eHalStatus halStatus= eHAL_STATUS_SUCCESS;
    u_int8_t *genie = wrqu->data.pointer;
    
    ENTER();
    
    if(!wrqu->data.length)
    {
        EXIT();
        return 0;
    }
    
    switch (genie[0]) 
    {
        case DOT11F_EID_WPA: 
        case DOT11F_EID_RSN:
            if((WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy == 0)
            {
                hdd_softap_Deregister_BC_STA(pHostapdAdapter);
                hdd_softap_Register_BC_STA(pHostapdAdapter, 1);
            }   
            (WLAN_HDD_GET_AP_CTX_PTR(pHostapdAdapter))->uPrivacy = 1;
            halStatus = WLANSAP_Set_WPARSNIes(pVosContext, wrqu->data.pointer, wrqu->data.length);
            break;
            
        default:
            hddLog (LOGE, "%s Set UNKNOWN IE %X",__func__, genie[0]);
            halStatus = 0;
    }
    
    EXIT();
    return halStatus; 
}

static VOS_STATUS  wlan_hdd_get_classAstats_for_station(hdd_adapter_t *pAdapter, u8 staid)
{
   eHalStatus hstatus;
   long lrc;
   struct statsContext context;

   if (NULL == pAdapter)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,"%s: Padapter is NULL", __func__);
      return VOS_STATUS_E_FAULT;
   }

   init_completion(&context.completion);
   context.pAdapter = pAdapter;
   context.magic = STATS_CONTEXT_MAGIC;
   hstatus = sme_GetStatistics( WLAN_HDD_GET_HAL_CTX(pAdapter),
                                  eCSR_HDD,
                                  SME_GLOBAL_CLASSA_STATS,
                                  hdd_GetClassA_statisticsCB,
                                  0, // not periodic
                                  FALSE, //non-cached results
                                  staid,
                                  &context);
   if (eHAL_STATUS_SUCCESS != hstatus)
   {
      hddLog(VOS_TRACE_LEVEL_ERROR,
            "%s: Unable to retrieve statistics for link speed",
            __func__);
   }
   else
   {
      lrc = wait_for_completion_interruptible_timeout(&context.completion,
            msecs_to_jiffies(WLAN_WAIT_TIME_STATS));
      context.magic = 0;
      if (lrc <= 0)
      {
         hddLog(VOS_TRACE_LEVEL_ERROR,
               "%s: SME %s while retrieving link speed",
              __func__, (0 == lrc) ? "timeout" : "interrupt");
         msleep(50);
      }
   }
   return VOS_STATUS_SUCCESS;
}

int iw_get_softap_linkspeed(struct net_device *dev,
        struct iw_request_info *info,
        union iwreq_data *wrqu,
        char *extra)

{
   hdd_adapter_t *pHostapdAdapter = (netdev_priv(dev));
   hdd_context_t *pHddCtx;
   char *pLinkSpeed = (char*)extra;
   v_U32_t link_speed;
   unsigned short staId;
   int len = sizeof(v_U32_t)+1;
   v_BYTE_t macAddress[VOS_MAC_ADDR_SIZE];
   VOS_STATUS status;
   int rc, valid;

   pHddCtx = WLAN_HDD_GET_CTX(pHostapdAdapter);

   valid = wlan_hdd_validate_context(pHddCtx);

   if (0 != valid)
   {
       hddLog(VOS_TRACE_LEVEL_ERROR, FL("HDD context not valid"));
       return valid;
   }

   hddLog(VOS_TRACE_LEVEL_INFO, "%s wrqu->data.length= %d\n", __func__, wrqu->data.length);
   status = hdd_string_to_hex ((char *)wrqu->data.pointer, wrqu->data.length, macAddress );

   if (!VOS_IS_STATUS_SUCCESS(status ))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, FL("String to Hex conversion Failed"));
   }

   /* If no mac address is passed and/or its length is less than 18,
    * link speed for first connected client will be returned.
    */
   if (!VOS_IS_STATUS_SUCCESS(status ) || wrqu->data.length < 18)
   {
      status = hdd_softap_GetConnectedStaId(pHostapdAdapter, (void *)(&staId));
   }
   else
   {
      status = hdd_softap_GetStaId(pHostapdAdapter,
                               (v_MACADDR_t *)macAddress, (void *)(&staId));
   }

   if (!VOS_IS_STATUS_SUCCESS(status))
   {
      hddLog(VOS_TRACE_LEVEL_ERROR, FL("ERROR: HDD Failed to find sta id!!"));
      link_speed = 0;
   }
   else
   {
      status = wlan_hdd_get_classAstats_for_station(pHostapdAdapter , staId);

      if (!VOS_IS_STATUS_SUCCESS(status ))
      {
          hddLog(VOS_TRACE_LEVEL_ERROR, FL("Unable to retrieve SME statistics"));
          return -EINVAL;
      }

      WLANTL_GetSTALinkCapacity(pHddCtx->pvosContext,
                                staId, &link_speed);

      link_speed = link_speed / 10;

      if (0 == link_speed)
      {
          /* The linkspeed returned by HAL is in units of 500kbps.
           * converting it to mbps.
           * This is required to support legacy firmware which does
           * not return link capacity.
           */
          link_speed =(int)pHostapdAdapter->hdd_stats.ClassA_stat.tx_rate/2;
      }
   }

   wrqu->data.length = len;
   rc = snprintf(pLinkSpeed, len, "%lu", link_speed);

   if ((rc < 0) || (rc >= len))
   {
      // encoding or length error?
      hddLog(VOS_TRACE_LEVEL_ERROR,FL( "Unable to encode link speed"));
      return -EIO;
   }

   return 0;
}

static const iw_handler      hostapd_handler[] =
{
   (iw_handler) NULL,           /* SIOCSIWCOMMIT */
   (iw_handler) NULL,           /* SIOCGIWNAME */
   (iw_handler) NULL,           /* SIOCSIWNWID */
   (iw_handler) NULL,           /* SIOCGIWNWID */
   (iw_handler) NULL,           /* SIOCSIWFREQ */
   (iw_handler) iw_get_ap_freq,    /* SIOCGIWFREQ */
   (iw_handler) NULL,           /* SIOCSIWMODE */
   (iw_handler) NULL,           /* SIOCGIWMODE */
   (iw_handler) NULL,           /* SIOCSIWSENS */
   (iw_handler) NULL,           /* SIOCGIWSENS */
   (iw_handler) NULL,           /* SIOCSIWRANGE */
   (iw_handler) NULL,           /* SIOCGIWRANGE */
   (iw_handler) NULL,           /* SIOCSIWPRIV */
   (iw_handler) NULL,           /* SIOCGIWPRIV */
   (iw_handler) NULL,           /* SIOCSIWSTATS */
   (iw_handler) NULL,           /* SIOCGIWSTATS */
   (iw_handler) NULL,           /* SIOCSIWSPY */
   (iw_handler) NULL,           /* SIOCGIWSPY */
   (iw_handler) NULL,           /* SIOCSIWTHRSPY */
   (iw_handler) NULL,           /* SIOCGIWTHRSPY */
   (iw_handler) NULL,           /* SIOCSIWAP */
   (iw_handler) NULL,           /* SIOCGIWAP */
   (iw_handler) iw_set_ap_mlme,    /* SIOCSIWMLME */
   (iw_handler) NULL,           /* SIOCGIWAPLIST */
   (iw_handler) NULL,           /* SIOCSIWSCAN */
   (iw_handler) NULL,           /* SIOCGIWSCAN */
   (iw_handler) NULL,           /* SIOCSIWESSID */
   (iw_handler) NULL,           /* SIOCGIWESSID */
   (iw_handler) NULL,           /* SIOCSIWNICKN */
   (iw_handler) NULL,           /* SIOCGIWNICKN */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) NULL,           /* SIOCSIWRATE */
   (iw_handler) NULL,           /* SIOCGIWRATE */
   (iw_handler) NULL,           /* SIOCSIWRTS */
   (iw_handler) iw_get_ap_rts_threshold,     /* SIOCGIWRTS */
   (iw_handler) NULL,           /* SIOCSIWFRAG */
   (iw_handler) iw_get_ap_frag_threshold,    /* SIOCGIWFRAG */
   (iw_handler) NULL,           /* SIOCSIWTXPOW */
   (iw_handler) NULL,           /* SIOCGIWTXPOW */
   (iw_handler) NULL,           /* SIOCSIWRETRY */
   (iw_handler) NULL,           /* SIOCGIWRETRY */
   (iw_handler) NULL,           /* SIOCSIWENCODE */
   (iw_handler) NULL,           /* SIOCGIWENCODE */
   (iw_handler) NULL,           /* SIOCSIWPOWER */
   (iw_handler) NULL,           /* SIOCGIWPOWER */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) NULL,           /* -- hole -- */
   (iw_handler) iw_set_ap_genie,     /* SIOCSIWGENIE */
   (iw_handler) NULL,           /* SIOCGIWGENIE */
   (iw_handler) iw_set_auth_hostap,    /* SIOCSIWAUTH */
   (iw_handler) NULL,           /* SIOCGIWAUTH */
   (iw_handler) iw_set_ap_encodeext,     /* SIOCSIWENCODEEXT */
   (iw_handler) NULL,           /* SIOCGIWENCODEEXT */
   (iw_handler) NULL,           /* SIOCSIWPMKSA */
};

#define    IW_PRIV_TYPE_OPTIE    (IW_PRIV_TYPE_BYTE | QCSAP_MAX_OPT_IE)
#define    IW_PRIV_TYPE_MLME \
  (IW_PRIV_TYPE_BYTE | sizeof(struct ieee80211req_mlme))

static const struct iw_priv_args hostapd_private_args[] = {
  { QCSAP_IOCTL_SETPARAM,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 2, 0, "setparam" },
  { QCSAP_IOCTL_SETPARAM,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "" },
  { QCSAP_PARAM_MAX_ASSOC,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setMaxAssoc" },
   { QCSAP_PARAM_HIDE_SSID,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "hideSSID" },
   { QCSAP_PARAM_SET_MC_RATE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0,  "setMcRate" },
  { QCSAP_IOCTL_GETPARAM,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getparam" },
  { QCSAP_IOCTL_GETPARAM, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "" },
  { QCSAP_PARAM_MAX_ASSOC, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getMaxAssoc" },
  { QCSAP_PARAM_GET_WLAN_DBG, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getwlandbg" },
  { QCSAP_PARAM_AUTO_CHANNEL, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "getAutoChannel" },
  { QCSAP_PARAM_MODULE_DOWN_IND, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1,    "moduleDownInd" },
  { QCSAP_PARAM_CLR_ACL, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "setClearAcl" },
  { QCSAP_PARAM_ACL_MODE,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, 0, "setAclMode" },
  { QCSAP_IOCTL_COMMIT,
      IW_PRIV_TYPE_BYTE | sizeof(struct s_CommitConfig) | IW_PRIV_SIZE_FIXED, 0, "commit" },
  { QCSAP_IOCTL_SETMLME,
      IW_PRIV_TYPE_BYTE | sizeof(struct sQcSapreq_mlme)| IW_PRIV_SIZE_FIXED, 0, "setmlme" },
  { QCSAP_IOCTL_GET_STAWPAIE,
      IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 1, 0, "get_staWPAIE" },
  { QCSAP_IOCTL_SETWPAIE,
      IW_PRIV_TYPE_BYTE | QCSAP_MAX_WSC_IE | IW_PRIV_SIZE_FIXED, 0, "setwpaie" },
  { QCSAP_IOCTL_STOPBSS,
      IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED, 0, "stopbss" },
  { QCSAP_IOCTL_VERSION, 0,
      IW_PRIV_TYPE_CHAR | QCSAP_MAX_WSC_IE, "version" },
  { QCSAP_IOCTL_GET_STA_INFO, 0,
      IW_PRIV_TYPE_CHAR | WE_SAP_MAX_STA_INFO, "get_sta_info" },
  { QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES,
      IW_PRIV_TYPE_BYTE | sizeof(sQcSapreq_WPSPBCProbeReqIES_t) | IW_PRIV_SIZE_FIXED | 1, 0, "getProbeReqIEs" },
  { QCSAP_IOCTL_GET_CHANNEL, 0,
      IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 1, "getchannel" },
  { QCSAP_IOCTL_ASSOC_STA_MACADDR, 0,
      IW_PRIV_TYPE_BYTE | /*((WLAN_MAX_STA_COUNT*6)+100)*/1 , "get_assoc_stamac" },
    { QCSAP_IOCTL_DISASSOC_STA,
        IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 6 , 0, "disassoc_sta" },
  { QCSAP_IOCTL_AP_STATS,
        IW_PRIV_TYPE_BYTE | QCSAP_MAX_WSC_IE, 0, "ap_stats" },
  { QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED,
        IW_PRIV_TYPE_CHAR | 18,
        IW_PRIV_TYPE_CHAR | 5, "getLinkSpeed" },

  { QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE,
        IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3, 0, "" },
   /* handlers for sub-ioctl */
   {   WE_SET_WLAN_DBG,
       IW_PRIV_TYPE_INT | IW_PRIV_SIZE_FIXED | 3,
       0, 
       "setwlandbg" },

   /* handlers for main ioctl */
   {   QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0, 
       "" },

   /* handlers for sub-ioctl */
   {   WE_LOG_DUMP_CMD,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0, 
       "dump" },
   {   WE_P2P_NOA_CMD,
       IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
       0, 
       "SetP2pPs" },
     /* handlers for sub ioctl */
    {
        WE_MCC_CONFIG_CREDENTIAL,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "setMccCrdnl" },

     /* handlers for sub ioctl */
    {
        WE_MCC_CONFIG_PARAMS,
        IW_PRIV_TYPE_INT | MAX_VAR_ARGS,
        0,
        "setMccConfig" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_MODIFY_ACL,
        IW_PRIV_TYPE_BYTE | IW_PRIV_SIZE_FIXED | 8,
        0, 
        "modify_acl" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_GET_CHANNEL_LIST,
        0, 
        IW_PRIV_TYPE_BYTE | sizeof(tChannelListInfo),
        "getChannelList" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_SET_TX_POWER,
        IW_PRIV_TYPE_INT| IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTxPower" },

    /* handlers for main ioctl */
    {   QCSAP_IOCTL_SET_MAX_TX_POWER,
        IW_PRIV_TYPE_INT| IW_PRIV_SIZE_FIXED | 1,
        0,
        "setTxMaxPower" },
    { QCSAP_IOCTL_DATAPATH_SNAP_SHOT,
      IW_PRIV_TYPE_NONE | IW_PRIV_TYPE_NONE, 0, "dataSnapshot" },
};

static const iw_handler hostapd_private[] = {
   [QCSAP_IOCTL_SETPARAM - SIOCIWFIRSTPRIV] = iw_softap_setparam,  //set priv ioctl
   [QCSAP_IOCTL_GETPARAM - SIOCIWFIRSTPRIV] = iw_softap_getparam,  //get priv ioctl   
   [QCSAP_IOCTL_COMMIT   - SIOCIWFIRSTPRIV] = iw_softap_commit, //get priv ioctl   
   [QCSAP_IOCTL_SETMLME  - SIOCIWFIRSTPRIV] = iw_softap_setmlme,
   [QCSAP_IOCTL_GET_STAWPAIE - SIOCIWFIRSTPRIV] = iw_get_genie, //get station genIE
   [QCSAP_IOCTL_SETWPAIE - SIOCIWFIRSTPRIV] = iw_softap_setwpsie,
   [QCSAP_IOCTL_STOPBSS - SIOCIWFIRSTPRIV] = iw_softap_stopbss,       // stop bss
   [QCSAP_IOCTL_VERSION - SIOCIWFIRSTPRIV] = iw_softap_version,       // get driver version
   [QCSAP_IOCTL_GET_WPS_PBC_PROBE_REQ_IES - SIOCIWFIRSTPRIV] = iw_get_WPSPBCProbeReqIEs,
   [QCSAP_IOCTL_GET_CHANNEL - SIOCIWFIRSTPRIV] = iw_softap_getchannel,
   [QCSAP_IOCTL_ASSOC_STA_MACADDR - SIOCIWFIRSTPRIV] = iw_softap_getassoc_stamacaddr,
   [QCSAP_IOCTL_DISASSOC_STA - SIOCIWFIRSTPRIV] = iw_softap_disassoc_sta,
   [QCSAP_IOCTL_AP_STATS - SIOCIWFIRSTPRIV] = iw_softap_ap_stats,
   [QCSAP_IOCTL_PRIV_SET_THREE_INT_GET_NONE - SIOCIWFIRSTPRIV]  = iw_set_three_ints_getnone,   
   [QCSAP_IOCTL_PRIV_SET_VAR_INT_GET_NONE - SIOCIWFIRSTPRIV]     = iw_set_var_ints_getnone,
   [QCSAP_IOCTL_SET_CHANNEL_RANGE - SIOCIWFIRSTPRIV] = iw_softap_set_channel_range,
   [QCSAP_IOCTL_MODIFY_ACL - SIOCIWFIRSTPRIV]   = iw_softap_modify_acl,
   [QCSAP_IOCTL_GET_CHANNEL_LIST - SIOCIWFIRSTPRIV]   = iw_softap_get_channel_list,
   [QCSAP_IOCTL_GET_STA_INFO - SIOCIWFIRSTPRIV] = iw_softap_get_sta_info,
   [QCSAP_IOCTL_PRIV_GET_SOFTAP_LINK_SPEED - SIOCIWFIRSTPRIV]     = iw_get_softap_linkspeed,
   [QCSAP_IOCTL_SET_TX_POWER - SIOCIWFIRSTPRIV]   = iw_softap_set_tx_power,
   [QCSAP_IOCTL_SET_MAX_TX_POWER - SIOCIWFIRSTPRIV]   = iw_softap_set_max_tx_power,
   [QCSAP_IOCTL_DATAPATH_SNAP_SHOT - SIOCIWFIRSTPRIV]  =   iw_display_data_path_snapshot,
};
const struct iw_handler_def hostapd_handler_def = {
   .num_standard     = sizeof(hostapd_handler) / sizeof(hostapd_handler[0]),
   .num_private      = sizeof(hostapd_private) / sizeof(hostapd_private[0]),
   .num_private_args = sizeof(hostapd_private_args) / sizeof(hostapd_private_args[0]),
   .standard         = (iw_handler *)hostapd_handler,
   .private          = (iw_handler *)hostapd_private,
   .private_args     = hostapd_private_args,
   .get_wireless_stats = NULL,
};
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29)
struct net_device_ops net_ops_struct  = {
    .ndo_open = hdd_hostapd_open,
    .ndo_stop = hdd_hostapd_stop,
    .ndo_uninit = hdd_hostapd_uninit,
    .ndo_start_xmit = hdd_softap_hard_start_xmit,
    .ndo_tx_timeout = hdd_softap_tx_timeout,
    .ndo_get_stats = hdd_softap_stats,
    .ndo_set_mac_address = hdd_hostapd_set_mac_address,
    .ndo_do_ioctl = hdd_hostapd_ioctl,
    .ndo_change_mtu = hdd_hostapd_change_mtu,
    .ndo_select_queue = hdd_hostapd_select_queue,
 };
#endif

int hdd_set_hostapd(hdd_adapter_t *pAdapter)
{
    return VOS_STATUS_SUCCESS;
} 

void hdd_set_ap_ops( struct net_device *pWlanHostapdDev )
{
#if LINUX_VERSION_CODE > KERNEL_VERSION(2,6,29)
  pWlanHostapdDev->netdev_ops = &net_ops_struct;
#else
  pWlanHostapdDev->open = hdd_hostapd_open;
  pWlanHostapdDev->stop = hdd_hostapd_stop;
  pWlanHostapdDev->uninit = hdd_hostapd_uninit;
  pWlanHostapdDev->hard_start_xmit = hdd_softap_hard_start_xmit;
  pWlanHostapdDev->tx_timeout = hdd_softap_tx_timeout;
  pWlanHostapdDev->get_stats = hdd_softap_stats;
  pWlanHostapdDev->set_mac_address = hdd_hostapd_set_mac_address;
  pWlanHostapdDev->do_ioctl = hdd_hostapd_ioctl;
#endif
}

VOS_STATUS hdd_init_ap_mode( hdd_adapter_t *pAdapter )
{   
    hdd_hostapd_state_t * phostapdBuf;
    struct net_device *dev = pAdapter->dev;
    hdd_context_t *pHddCtx = WLAN_HDD_GET_CTX(pAdapter);
    VOS_STATUS status;
    ENTER();
       // Allocate the Wireless Extensions state structure   
    phostapdBuf = WLAN_HDD_GET_HOSTAP_STATE_PTR( pAdapter );
 
    sme_SetCurrDeviceMode(pHddCtx->hHal, pAdapter->device_mode);

    // Zero the memory.  This zeros the profile structure.
    memset(phostapdBuf, 0,sizeof(hdd_hostapd_state_t));
    
    // Set up the pointer to the Wireless Extensions state structure
    // NOP
    status = hdd_set_hostapd(pAdapter);
    if(!VOS_IS_STATUS_SUCCESS(status)) {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: hdd_set_hostapd failed!!\n"));
         return status;
    }
 
    status = vos_event_init(&phostapdBuf->vosEvent);
    if (!VOS_IS_STATUS_SUCCESS(status))
    {
         VOS_TRACE(VOS_MODULE_ID_HDD, VOS_TRACE_LEVEL_ERROR, ("ERROR: Hostapd HDD vos event init failed!!\n"));
         return status;
    }
    
    init_completion(&pAdapter->session_close_comp_var);
    init_completion(&pAdapter->session_open_comp_var);

    sema_init(&(WLAN_HDD_GET_AP_CTX_PTR(pAdapter))->semWpsPBCOverlapInd, 1);
 
     // Register as a wireless device
    dev->wireless_handlers = (struct iw_handler_def *)& hostapd_handler_def;

    //Initialize the data path module
    status = hdd_softap_init_tx_rx(pAdapter);
    if ( !VOS_IS_STATUS_SUCCESS( status ))
    {
       hddLog(VOS_TRACE_LEVEL_FATAL, "%s: hdd_softap_init_tx_rx failed", __func__);
    }

    status = hdd_wmm_adapter_init( pAdapter );
    if (!VOS_IS_STATUS_SUCCESS(status))
    {
       hddLog(VOS_TRACE_LEVEL_ERROR,
             "hdd_wmm_adapter_init() failed with status code %08d [x%08lx]",
                             status, status );
       goto error_wmm_init;
    }

    set_bit(WMM_INIT_DONE, &pAdapter->event_flags);

    wlan_hdd_set_monitor_tx_adapter( WLAN_HDD_GET_CTX(pAdapter), pAdapter );

    return status;

error_wmm_init:
    hdd_softap_deinit_tx_rx( pAdapter );
    EXIT();
    return status;
}

hdd_adapter_t* hdd_wlan_create_ap_dev( hdd_context_t *pHddCtx, tSirMacAddr macAddr, tANI_U8 *iface_name )
{
    struct net_device *pWlanHostapdDev = NULL;
    hdd_adapter_t *pHostapdAdapter = NULL;
    v_CONTEXT_t pVosContext= NULL;

   pWlanHostapdDev = alloc_netdev_mq(sizeof(hdd_adapter_t), iface_name, ether_setup, NUM_TX_QUEUES);

    if (pWlanHostapdDev != NULL)
    {
        pHostapdAdapter = netdev_priv(pWlanHostapdDev);

        //Init the net_device structure
        ether_setup(pWlanHostapdDev);

        //Initialize the adapter context to zeros.
        vos_mem_zero(pHostapdAdapter, sizeof( hdd_adapter_t ));
        pHostapdAdapter->dev = pWlanHostapdDev;
        pHostapdAdapter->pHddCtx = pHddCtx; 
        pHostapdAdapter->magic = WLAN_HDD_ADAPTER_MAGIC;

        //Get the Global VOSS context.
        pVosContext = vos_get_global_context(VOS_MODULE_ID_SYS, NULL);
        //Save the adapter context in global context for future.
        ((VosContextType*)(pVosContext))->pHDDSoftAPContext = (v_VOID_t*)pHostapdAdapter;

        //Init the net_device structure
        strlcpy(pWlanHostapdDev->name, (const char *)iface_name, IFNAMSIZ);

        hdd_set_ap_ops( pHostapdAdapter->dev );

        pWlanHostapdDev->tx_queue_len = NET_DEV_TX_QUEUE_LEN;
        pWlanHostapdDev->watchdog_timeo = HDD_TX_TIMEOUT;
        pWlanHostapdDev->mtu = HDD_DEFAULT_MTU;
    
        vos_mem_copy(pWlanHostapdDev->dev_addr, (void *)macAddr,sizeof(tSirMacAddr));
        vos_mem_copy(pHostapdAdapter->macAddressCurrent.bytes, (void *)macAddr, sizeof(tSirMacAddr));

        pWlanHostapdDev->destructor = free_netdev;
        pWlanHostapdDev->ieee80211_ptr = &pHostapdAdapter->wdev ;
        pHostapdAdapter->wdev.wiphy = pHddCtx->wiphy;  
        pHostapdAdapter->wdev.netdev =  pWlanHostapdDev;
        init_completion(&pHostapdAdapter->tx_action_cnf_event);
        init_completion(&pHostapdAdapter->cancel_rem_on_chan_var);
        init_completion(&pHostapdAdapter->rem_on_chan_ready_event);
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(2,6,38))
        init_completion(&pHostapdAdapter->offchannel_tx_event);
#endif

        SET_NETDEV_DEV(pWlanHostapdDev, pHddCtx->parent_dev);
    }
    return pHostapdAdapter;
}

VOS_STATUS hdd_register_hostapd( hdd_adapter_t *pAdapter, tANI_U8 rtnl_lock_held )
{
   struct net_device *dev = pAdapter->dev;
   VOS_STATUS status = VOS_STATUS_SUCCESS;

   ENTER();
   
   if( rtnl_lock_held )
   {
     if (strnchr(dev->name, strlen(dev->name), '%')) {
         if( dev_alloc_name(dev, dev->name) < 0 )
         {
            hddLog(VOS_TRACE_LEVEL_FATAL, "%s:Failed:dev_alloc_name", __func__);
            return VOS_STATUS_E_FAILURE;            
         }
      }
      if (register_netdevice(dev))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL,
                "%s:Failed:register_netdevice", __func__);
         return VOS_STATUS_E_FAILURE;         
      }
   }
   else
   {
      if (register_netdev(dev))
      {
         hddLog(VOS_TRACE_LEVEL_FATAL, "%s: Failed:register_netdev", __func__);
         return VOS_STATUS_E_FAILURE;
      }
   }
   set_bit(NET_DEVICE_REGISTERED, &pAdapter->event_flags);
     
   EXIT();
   return status;
}
    
VOS_STATUS hdd_unregister_hostapd(hdd_adapter_t *pAdapter)
{
   ENTER();
   
   hdd_softap_deinit_tx_rx(pAdapter);

   /* if we are being called during driver unload, then the dev has already
      been invalidated.  if we are being called at other times, then we can
      detatch the wireless device handlers */
   if (pAdapter->dev)
   {
      pAdapter->dev->wireless_handlers = NULL;
   }
   EXIT();
   return 0;
}
