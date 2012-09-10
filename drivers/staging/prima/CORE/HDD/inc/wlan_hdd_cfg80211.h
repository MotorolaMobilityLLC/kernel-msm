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

#if !defined( HDD_CFG80211_H__ )
#define HDD_CFG80211_H__


/**===========================================================================
  
  \file  wlan_hdd_cfg80211.h
  
  \brief cfg80211 functions declarations
    
               Copyright 2008 (c) Qualcomm, Incorporated.
               All Rights Reserved.
               Qualcomm Confidential and Proprietary.
  
  ==========================================================================*/
  
/* $HEADER$ */

#ifdef CONFIG_CFG80211

//value for initial part of frames and number of bytes to be compared
#define GAS_INITIAL_REQ "\x04\x0a"  
#define GAS_INITIAL_REQ_SIZE 2

#define GAS_INITIAL_RSP "\x04\x0b"
#define GAS_INITIAL_RSP_SIZE 2

#define GAS_COMEBACK_REQ "\x04\x0c"
#define GAS_COMEBACK_REQ_SIZE 2

#define GAS_COMEBACK_RSP "\x04\x0d"
#define GAS_COMEBACK_RSP_SIZE 2

#define P2P_PUBLIC_ACTION_FRAME "\x04\x09\x50\x6f\x9a\x09" 
#define P2P_PUBLIC_ACTION_FRAME_SIZE 6

#define P2P_ACTION_FRAME "\x7f\x50\x6f\x9a\x09"
#define P2P_ACTION_FRAME_SIZE 5

#define HDD_P2P_WILDCARD_SSID "DIRECT-" //TODO Put it in proper place;
#define HDD_P2P_WILDCARD_SSID_LEN 7

#define WPA_OUI_TYPE   "\x00\x50\xf2\x01"
#define BLACKLIST_OUI_TYPE   "\x00\x50\x00\x00"
#define WHITELIST_OUI_TYPE   "\x00\x50\x00\x01"
#define WPA_OUI_TYPE_SIZE  4

#define WLAN_BSS_MEMBERSHIP_SELECTOR_HT_PHY 127
#define BASIC_RATE_MASK   0x80
#define RATE_MASK         0x7f

struct cfg80211_bss* wlan_hdd_cfg80211_update_bss_db( hdd_adapter_t *pAdapter,
                                      tCsrRoamInfo *pRoamInfo
                                      );

#ifdef FEATURE_WLAN_LFR
int wlan_hdd_cfg80211_pmksa_candidate_notify(
                    hdd_adapter_t *pAdapter, tCsrRoamInfo *pRoamInfo,
                    int index, bool preauth );
#endif

#ifdef FEATURE_WLAN_WAPI
void wlan_hdd_cfg80211_set_key_wapi(hdd_adapter_t* pAdapter,
              u8 key_index, const u8 *mac_addr, u8 *key , int key_Len);
#endif
struct wiphy *wlan_hdd_cfg80211_init(int priv_size);

int wlan_hdd_cfg80211_register(struct device *dev,
                               struct wiphy *wiphy,
                               hdd_config_t *pCfg
                                         );

void wlan_hdd_cfg80211_post_voss_start(hdd_adapter_t* pAdapter);

void wlan_hdd_cfg80211_pre_voss_stop(hdd_adapter_t* pAdapter);


#endif // CONFIG_CFG80211

#endif
