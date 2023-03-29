/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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

/**
 * DOC: This file contains centralized definitions of converged configuration.
 */

#ifndef __CFG_MLME_SAP_H
#define __CFG_MLME_SAP_H

#define STR_SSID_DEFAULT "1234567890"
#define STR_SSID_DEFAULT_LEN sizeof(STR_SSID_DEFAULT)

#define CFG_SSID CFG_STRING( \
			"cfg_ssid", \
			0, \
			STR_SSID_DEFAULT_LEN, \
			STR_SSID_DEFAULT, \
			"CFG_SSID")

#define CFG_BEACON_INTERVAL CFG_INI_UINT( \
			"gBeaconInterval", \
			0, \
			65535, \
			100, \
			CFG_VALUE_OR_DEFAULT, \
			"CFG_BEACON_INTERVAL")

#define CFG_DTIM_PERIOD CFG_UINT( \
			"cfg_dtim_period", \
			0, \
			65535, \
			1, \
			CFG_VALUE_OR_DEFAULT, \
			"CFG_DTIM_PERIOD")

#define CFG_LISTEN_INTERVAL CFG_UINT( \
			"cfg_listen_interval", \
			0, \
			65535, \
			1, \
			CFG_VALUE_OR_DEFAULT, \
			"CFG_LISTEN_INTERVAL")

#define CFG_11G_ONLY_POLICY CFG_UINT( \
			"cfg_11g_only_policy", \
			0, \
			1, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"CFG_11G_ONLY_POLICY")

#define CFG_ASSOC_STA_LIMIT CFG_UINT( \
			"cfg_assoc_sta_limit", \
			1, \
			64, \
			10, \
			CFG_VALUE_OR_DEFAULT, \
			"CFG_ASSOC_STA_LIMIT")

/*
 * <ini>
 * cfg_enable_lte_coex - enable LTE COEX
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable LTE COEX
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_ENABLE_LTE_COEX CFG_INI_BOOL( \
			"gEnableLTECoex", \
			0, \
			"enabled lte coex")

/*
 * <ini>
 * cfg_rate_for_tx_mgmt - Set rate for tx mgmt
 * @Min: 0
 * @Max: 0xFF
 * @Default: 0xFF
 *
 * This ini is used to set rate for tx mgmt
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_RATE_FOR_TX_MGMT CFG_INI_UINT( \
			"gRateForTxMgmt", \
			0, \
			0xFF, \
			0xFF, \
			CFG_VALUE_OR_DEFAULT, \
			"set rate for mgmt tx")

/*
 * <ini>
 * cfg_rate_for_tx_mgmt_2g - Set rate for tx mgmt 2g
 * @Min: 0
 * @Max: 255
 * @Default: 255
 *
 * This ini is used to set rate for tx mgmt 2g
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_RATE_FOR_TX_MGMT_2G CFG_INI_UINT( \
			"gRateForTxMgmt2G", \
			0, \
			255, \
			255, \
			CFG_VALUE_OR_DEFAULT, \
			"set rate for mgmt tx 2g")

/*
 * <ini>
 * cfg_rate_for_tx_mgmt_5g - Set rate for tx mgmt 5g
 * @Min: 0
 * @Max: 255
 * @Default: 255
 *
 * This ini is used to set rate for tx mgmt 5g
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_RATE_FOR_TX_MGMT_5G CFG_INI_UINT( \
			"gRateForTxMgmt5G", \
			0, \
			255, \
			255, \
			CFG_VALUE_OR_DEFAULT, \
			"set rate for mgmt tx 5g")

/*
 * <ini>
 * gTelescopicBeaconWakeupEn - Set teles copic beacon wakeup
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default teles copic beacon wakeup
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_TELE_BCN_WAKEUP_EN CFG_INI_BOOL( \
			"gTelescopicBeaconWakeupEn", \
			0, \
			"set tescopic beacon wakeup")

/*
 * <ini>
 * telescopicBeaconMaxListenInterval - Set teles scopic beacon max listen value
 * @Min: 0
 * @Max: 7
 * @Default: 5
 *
 * This ini is used to set teles scopic beacon max listen interval value
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_TELE_BCN_MAX_LI CFG_INI_UINT( \
			"telescopicBeaconMaxListenInterval", \
			0, \
			7, \
			5, \
			CFG_VALUE_OR_DEFAULT, \
			"set telescopic beacon max listen")

/*
 * <ini>
 * gSapGetPeerInfo - Enable/Disable remote peer info query support
 * @Min: 0 - Disable remote peer info query support
 * @Max: 1 - Enable remote peer info query support
 * @Default: 1
 *
 * This ini is used to enable/disable remote peer info query support
 *
 * Usage: External
 *
 * </ini>
 */
 #define CFG_SAP_GET_PEER_INFO CFG_INI_BOOL( \
			"gSapGetPeerInfo", \
			1, \
			"sap get peer info")

/*
 * <ini>
 * gSapAllowAllChannel - Sap allow all channels
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to allow all channels for SAP
 *
 * Related: None
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_SAP_ALLOW_ALL_CHANNEL_PARAM CFG_INI_BOOL( \
			"gSapAllowAllChannel", \
			0, \
			"sap allow all channel params")

/*
 * <ini>
 * gSoftApMaxPeers - Set Max peers connected for SAP
 * @Min: 1
 * @Max: 64
 * @Default: 32
 *
 * This ini is used to set Max peers connected for SAP
 *
 * Related: None
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_SAP_MAX_NO_PEERS CFG_INI_UINT( \
			"gSoftApMaxPeers", \
			1, \
			64, \
			32, \
			CFG_VALUE_OR_DEFAULT, \
			"max no of peers")

/*
 * <ini>
 * gMaxOffloadPeers - Set max offload peers
 * @Min: 2
 * @Max: 5
 * @Default: 2
 *
 * This ini is used to set default teles copic beacon wakeup
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_SAP_MAX_OFFLOAD_PEERS CFG_INI_UINT( \
			"gMaxOffloadPeers", \
			2, \
			5, \
			2, \
			CFG_VALUE_OR_DEFAULT, \
			"max offload peers")

/*
 * <ini>
 * gMaxOffloadReorderBuffs - Set max offload reorder buffs
 * @Min: 0
 * @Max: 3
 * @Default: 2
 *
 * This ini is used to set max offload reorder buffs
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_SAP_MAX_OFFLOAD_REORDER_BUFFS CFG_INI_UINT( \
			"gMaxOffloadReorderBuffs", \
			0, \
			3, \
			2, \
			CFG_VALUE_OR_DEFAULT, \
			"sap max offload reorder buffs")

/*
 * <ini>
 * g_sap_chanswitch_beacon_cnt - Set channel switch beacon count
 * @Min: 1
 * @Max: 10
 * @Default: 10
 *
 * This ini is used to set channel switch beacon count
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_SAP_CH_SWITCH_BEACON_CNT CFG_INI_UINT( \
			"g_sap_chanswitch_beacon_cnt", \
			1, \
			10, \
			10, \
			CFG_VALUE_OR_DEFAULT, \
			"set channel switch beacon count")

/*
 * <ini>
 * g_sap_chanswitch_mode - channel switch mode
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to configure the value of channel switch mode, which is
 * contained in the Channel Switch Announcement(CSA) information element sent
 * by an SAP.
 *
 * 0 - CSA receiving STA doesn't need to do anything
 * 1 - CSA receiving STA shall not transmit any more frames on the channel
 *     until the scheduled channel switch occurs
 *
 * Related: none
 *
 * Supported Feature: SAP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAP_CH_SWITCH_MODE CFG_INI_BOOL( \
			"g_sap_chanswitch_mode", \
			1, \
			"sap channel switch mode")

/*
 * <ini>
 * gEnableSapInternalRestart - Sap internal restart name
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used for sap internal restart name
 *
 * Related: None
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_SAP_INTERNAL_RESTART CFG_INI_BOOL( \
			"gEnableSapInternalRestart", \
			1, \
			"sap internal restart")

/*
 * <ini>
 * gChanSwitchHostapdRateEnabled - Enable channale switch hostapd rate
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable channale switch hostapd rate
 *
 * Related: None
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
 #define CFG_CHAN_SWITCH_HOSTAPD_RATE_ENABLED_NAME CFG_INI_BOOL( \
			"gChanSwitchHostapdRateEnabled", \
			0, \
			"chan switch hostapd rate enabled")

/*
 * <ini>
 * gReducedBeaconInterval - beacon interval reduced
 * @Min: 0
 * @Max: 100
 * @Default: 0
 *
 * This ini is used to reduce beacon interval before channel
 * switch (when val great than 0, or the feature is disabled).
 * It would reduce the downtime on the STA side which is
 * waiting for beacons from the AP to resume back transmission.
 * Switch back the beacon_interval to its original value after
 * channel switch based on the timeout.
 *
 * Related: none
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_REDUCED_BEACON_INTERVAL CFG_INI_UINT( \
			"gReducedBeaconInterval", \
			0, \
			100, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"reduced beacon interval")

/*
 * <ini>
 * gCountryCodePriority - Priority to set country code
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default gCountryCodePriority
 *
 * Related: None
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_COUNTRY_CODE_PRIORITY CFG_INI_BOOL( \
			"gCountryCodePriority", \
			0, \
			"Country code priority")

/*
 * <ini>
 * gSapPreferredChanLocation - Restrict channel switches between ondoor and
 * outdoor.
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is used for restricting channel switches between Indoor and outdoor
 * channels after radar detection.
 * 0- No preferred channel location
 * 1- Use indoor channels only
 * 2- Use outdoor channels only
 * Related: NA.
 *
 * Supported Feature: DFS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SAP_PREF_CHANNEL_LOCATION CFG_INI_UINT( \
			"gSapPreferredChanLocation", \
			0, \
			2, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"Sap preferred channel location")
/*
 * <ini>
 * gSapForce11NFor11AC - Restrict SAP to 11n if set 1 even though
 * hostapd.conf request for 11ac.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Restrict SAP to 11n if set 1 even though hostapd.conf request for 11ac.
 *
 * 0- Do not force 11n for 11ac.
 * 1- Force 11n for 11ac.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SAP_FORCE_11N_FOR_11AC CFG_INI_BOOL( \
			"gSapForce11NFor11AC", \
			0, \
			"Sap force 11n for 11ac")

/*
 * <ini>
 * gGoForce11NFor11AC - Restrict GO to 11n if set 1 even though
 * hostapd.conf request for 11ac.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Restrict GO to 11n if set 1 even though hostapd.conf request for 11ac.
 *
 * 0- Do not force 11n for 11ac.
 * 1- Force 11n for 11ac.
 *
 * Supported Feature: GO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_GO_FORCE_11N_FOR_11AC CFG_INI_BOOL( \
			"gGoForce11NFor11AC", \
			0, \
			"GO force 11n for 11ac")


/*
 * <ini>
 * gEnableApRandomBssid - Create ramdom BSSID
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to create a random BSSID in SoftAP mode to meet
 * the Android requirement.
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_ENABLE_RANDOM_BSSID CFG_INI_BOOL( \
	"gEnableApRandomBssid", \
	0, \
	"Create ramdom BSSID")

/*
 * <ini>
 * gSapChannelAvoidance - SAP MCC channel avoidance.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to sets sap mcc channel avoidance.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SAP_MCC_CHANNEL_AVOIDANCE CFG_INI_UINT( \
			"gSapChannelAvoidance", \
			0, \
			1, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"SAP MCC channel avoidance")

/*
 * <ini>
 * gSAP11ACOverride - Override bw to 11ac for SAP in driver even if supplicant
 *                    or hostapd configures HT.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable 11AC override for SAP.
 * Android UI does not provide advanced configuration options
 * for SoftAP for Android O and below.
 * Default override disabled for android. Can be enabled from
 * ini for Android O and below.
 *
 *
 * Supported Feature: SAP
 *
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_SAP_11AC_OVERRIDE CFG_INI_BOOL( \
				"gSAP11ACOverride", \
				0, \
				"Override bw to 11ac for SAP")

/*
 * <ini>
 * gGO11ACOverride - Override bw to 11ac for P2P GO
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable 11AC override for GO.
 * P2P GO also follows start_bss and since P2P GO could not be
 * configured to setup VHT channel width in wpa_supplicant, driver
 * can override 11AC.
 *
 *
 * Supported Feature: P2P
 *
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_GO_11AC_OVERRIDE CFG_INI_BOOL( \
				"gGO11ACOverride", \
				1, \
				"Override bw to 11ac for P2P GO")

/*
 *
 * <ini>
 * enable_bcast_deauth_for_sap - Enable/Disable broadcast deauth support
 *                                                     in driver for SAP
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable broadcast deauth support in driver
 * for sap mode.
 *
 * Related: None
 *
 * Supported Feature: SAP
 * Usage: External
 *
 * </ini>
 */
#define CFG_IS_SAP_BCAST_DEAUTH_ENABLED CFG_INI_BOOL( \
				"enable_bcast_deauth_for_sap", \
				0, \
				"Enable/Disable bcast deauth for SAP")

#ifdef WLAN_FEATURE_SAE
/*
 *
 * <ini>
 * enable_sae_for_sap - Enable/Disable SAE support in driver for SAP
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable SAE support in driver for SAP mode
 * Driver will process/drop the SAE authentication frames based on this config.
 *
 * Related: None
 *
 * Supported Feature: SAE
 * Usage: External
 *
 * </ini>
 */
#define CFG_IS_SAP_SAE_ENABLED CFG_INI_BOOL( \
				"enable_sae_for_sap", \
				1, \
				"Enable/Disable SAE support for SAP")

#define CFG_SAP_SAE CFG(CFG_IS_SAP_SAE_ENABLED)

#else
#define CFG_SAP_SAE
#endif /* WLAN_FEATURE_SAE */

/*
 *
 * <ini>
 * enable_sap_fils_discovery - Enable/Disable fils discovery for 6Ghz SAP
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Enable: 6Ghz SAP transmits fils discovery frame at every 20ms
 * Disable: 6Ghz SAP transmits probe response frame at every 20ms
 *
 * Related: None
 *
 * Supported Feature: SAP
 * Usage: External
 *
 * </ini>
 */
#define CFG_6G_SAP_FILS_DISCOVERY_ENABLED CFG_INI_BOOL( \
					"enable_6g_sap_fils_discovery", \
					1, \
					"Enable/Disable fils discovery for SAP")

#define CFG_SAP_ALL \
	CFG_SAP_SAE \
	CFG(CFG_AP_ENABLE_RANDOM_BSSID) \
	CFG(CFG_SSID) \
	CFG(CFG_BEACON_INTERVAL) \
	CFG(CFG_DTIM_PERIOD) \
	CFG(CFG_LISTEN_INTERVAL) \
	CFG(CFG_11G_ONLY_POLICY) \
	CFG(CFG_ASSOC_STA_LIMIT) \
	CFG(CFG_ENABLE_LTE_COEX) \
	CFG(CFG_RATE_FOR_TX_MGMT) \
	CFG(CFG_RATE_FOR_TX_MGMT_2G) \
	CFG(CFG_RATE_FOR_TX_MGMT_5G) \
	CFG(CFG_TELE_BCN_WAKEUP_EN) \
	CFG(CFG_TELE_BCN_MAX_LI) \
	CFG(CFG_SAP_MCC_CHANNEL_AVOIDANCE) \
	CFG(CFG_SAP_GET_PEER_INFO) \
	CFG(CFG_SAP_ALLOW_ALL_CHANNEL_PARAM) \
	CFG(CFG_SAP_MAX_NO_PEERS) \
	CFG(CFG_SAP_MAX_OFFLOAD_PEERS) \
	CFG(CFG_SAP_MAX_OFFLOAD_REORDER_BUFFS) \
	CFG(CFG_SAP_CH_SWITCH_BEACON_CNT) \
	CFG(CFG_SAP_CH_SWITCH_MODE) \
	CFG(CFG_SAP_INTERNAL_RESTART) \
	CFG(CFG_CHAN_SWITCH_HOSTAPD_RATE_ENABLED_NAME) \
	CFG(CFG_REDUCED_BEACON_INTERVAL) \
	CFG(CFG_MAX_LI_MODULATED_DTIM) \
	CFG(CFG_COUNTRY_CODE_PRIORITY) \
	CFG(CFG_SAP_PREF_CHANNEL_LOCATION) \
	CFG(CFG_SAP_FORCE_11N_FOR_11AC) \
	CFG(CFG_SAP_11AC_OVERRIDE) \
	CFG(CFG_GO_FORCE_11N_FOR_11AC) \
	CFG(CFG_GO_11AC_OVERRIDE) \
	CFG(CFG_IS_SAP_BCAST_DEAUTH_ENABLED) \
	CFG(CFG_6G_SAP_FILS_DISCOVERY_ENABLED)

#endif /* __CFG_MLME_SAP_H */
