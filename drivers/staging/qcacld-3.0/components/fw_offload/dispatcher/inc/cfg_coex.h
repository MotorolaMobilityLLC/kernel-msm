/*
 * Copyright (c) 2012 - 2019, 2021 The Linux Foundation. All rights reserved.
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

#ifndef __CFG_COEX_H
#define __CFG_COEX_H

/*
 * <ini>
 * gSetBTCMode - Config BTC mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * 0 - TDD
 * 1 - FDD
 * 2 - Hybrid
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BTC_MODE CFG_INI_UINT( \
			"gSetBTCMode", \
			0, \
			2, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"BTC mode")

/*
 * <ini>
 * gSetAntennaIsolation - Set Antenna Isolation
 * @Min: 0
 * @Max: 255
 * @Default: 25
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ANTENNA_ISOLATION CFG_INI_UINT( \
			"gSetAntennaIsolation", \
			0, \
			255, \
			25, \
			CFG_VALUE_OR_DEFAULT, \
			"Antenna Isolation")

/*
 * <ini>
 * gSetMaxTxPowerForBTC - Set Max WLAN Tx power in COEX scenario
 * @Min: 0
 * @Max: 100
 * @Default: 100
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MAX_TX_POWER_FOR_BTC CFG_INI_UINT( \
			"gSetMaxTxPowerForBTC", \
			0, \
			100, \
			100, \
			CFG_VALUE_OR_DEFAULT, \
			"Max Tx Power for BTC")

/*
 * <ini>
 * gSetWlanLowRssiThreshold - Set WLAN low RSSI threshold for BTC mode switching
 * @Min: -100
 * @Max: 0
 * @Default: -80
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_WLAN_LOW_RSSI_THRESHOLD CFG_INI_INT( \
			"gSetWlanLowRssiThreshold", \
			-100, \
			0, \
			-80, \
			CFG_VALUE_OR_DEFAULT, \
			"WLAN Low RSSI Threshold")

/*
 * <ini>
 * gSetBtLowRssiThreshold - Set BT low RSSI threshold for BTC mode switching
 * @Min: -100
 * @Max: 0
 * @Default: -80
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BT_LOW_RSSI_THRESHOLD CFG_INI_INT( \
			"gSetBtLowRssiThreshold", \
			-100, \
			0, \
			-80, \
			CFG_VALUE_OR_DEFAULT, \
			"BT Low RSSI Threshold")

/*
 * <ini>
 * gSetBtInterferenceLowLL - Set lower limit of low level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -25
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BT_INTERFERENCE_LOW_LL CFG_INI_INT( \
			"gSetBtInterferenceLowLL", \
			-100, \
			100, \
			-25, \
			CFG_VALUE_OR_DEFAULT, \
			"BT Interference Low LL")

/*
 * <ini>
 * gSetBtInterferenceLowUL - Set upper limit of low level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -21
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BT_INTERFERENCE_LOW_UL CFG_INI_INT( \
			"gSetBtInterferenceLowUL", \
			-100, \
			100, \
			-21, \
			CFG_VALUE_OR_DEFAULT, \
			"BT Interference Low UL")

/*
 * <ini>
 * gSetBtInterferenceMediumLL - Set lower limit of medium level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -20
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BT_INTERFERENCE_MEDIUM_LL CFG_INI_INT( \
			"gSetBtInterferenceMediumLL", \
			-100, \
			100, \
			-20, \
			CFG_VALUE_OR_DEFAULT, \
			"BT Interference Medium LL")

/*
 * <ini>
 * gSetBtInterferenceMediumUL - Set upper limit of medium level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -16
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BT_INTERFERENCE_MEDIUM_UL CFG_INI_INT( \
			"gSetBtInterferenceMediumUL", \
			-100, \
			100, \
			-16, \
			CFG_VALUE_OR_DEFAULT, \
			"BT Interference Medium UL")

/*
 * <ini>
 * gSetBtInterferenceHighLL - Set lower limit of high level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -15
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BT_INTERFERENCE_HIGH_LL CFG_INI_INT( \
			"gSetBtInterferenceHighLL", \
			-100, \
			100, \
			-15, \
			CFG_VALUE_OR_DEFAULT, \
			"BT Interference High LL")

/*
 * <ini>
 * gSetBtInterferenceHighUL - Set upper limit of high level BT interference
 * @Min: -100
 * @Max: 100
 * @Default: -11
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BT_INTERFERENCE_HIGH_UL CFG_INI_INT( \
			"gSetBtInterferenceHighUL", \
			-100, \
			100, \
			-11, \
			CFG_VALUE_OR_DEFAULT, \
			"BT Interference High UL")

#ifdef FEATURE_MPTA_HELPER
/*
 * <ini>
 * gMPTAHelperEnable - Enable MPTA Helper
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable or disable coex MPTA Helper.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_COEX_MPTA_HELPER CFG_INI_BOOL( \
		"gMPTAHelperEnable", \
		0, \
		"Enable/Disable MPTA Helper")

#define COEX_MPTA_HELPER_CFG CFG(CFG_COEX_MPTA_HELPER)
#else
#define COEX_MPTA_HELPER_CFG
#endif

/*
 * <ini>
 * gBtScoAllowWlan2GScan - Allow wlan 2g scan when BT SCO connection is on
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * 0 - Disable
 * 1 - Enable
 *
 * This ini is used to enable or disable wlan 2g scan
 * when BT SCO connection is on.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BT_SCO_ALLOW_WLAN_2G_SCAN CFG_INI_BOOL( \
		"gBtScoAllowWlan2GScan", \
		1, \
		"Bt Sco Allow Wlan 2G Scan")

/*
 * <ini>
 * ble_scan_coex_policy - Ini to configure coex policy
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * 0 - Better BLE Advertiser reception performance
 * 1 - Better WLAN performance
 *
 * This ini is used to control the performance of ble scan case,’0’ to place
 * more emphasis on BLE Scan results , ‘1’ to place more emphasis on WLAN
 * performance
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_BLE_SCAN_COEX_POLICY CFG_INI_BOOL( \
		"ble_scan_coex_policy", \
		0, \
		"BLE scan Coex policy")

#ifdef FEATURE_COEX_CONFIG
/*
 * <ini>
 * gThreeWayCoexConfigLegacyEnable - Enable coex config legacy feature
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable or disable three way coex config legacy feature.
 * This feature is designed only for non-mobile solution.
 * When the feature is disabled, Firmware use the default configuration to
 * set the coex priority of three antenna(WLAN, BT, ZIGBEE).
 * when enable this feature, customer can use the vendor command to set antenna
 * coex priority dynamically.
 *
 * Supported Feature: three way coex config
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_THREE_WAY_COEX_CONFIG_LEGACY CFG_INI_BOOL( \
		"gThreeWayCoexConfigLegacyEnable", \
		0, \
		"Enable/Disable COEX Config Legacy")

#define THREE_WAY_COEX_CONFIG_LEGACY_CFG CFG(CFG_THREE_WAY_COEX_CONFIG_LEGACY)
#else
#define THREE_WAY_COEX_CONFIG_LEGACY_CFG
#endif

#define CFG_COEX_ALL \
	CFG(CFG_BTC_MODE) \
	CFG(CFG_ANTENNA_ISOLATION) \
	CFG(CFG_MAX_TX_POWER_FOR_BTC) \
	CFG(CFG_WLAN_LOW_RSSI_THRESHOLD) \
	CFG(CFG_BT_LOW_RSSI_THRESHOLD) \
	CFG(CFG_BT_INTERFERENCE_LOW_LL) \
	CFG(CFG_BT_INTERFERENCE_LOW_UL) \
	CFG(CFG_BT_INTERFERENCE_MEDIUM_LL) \
	CFG(CFG_BT_INTERFERENCE_MEDIUM_UL) \
	CFG(CFG_BT_INTERFERENCE_HIGH_LL) \
	CFG(CFG_BT_INTERFERENCE_HIGH_UL) \
	COEX_MPTA_HELPER_CFG \
	CFG(CFG_BT_SCO_ALLOW_WLAN_2G_SCAN) \
	THREE_WAY_COEX_CONFIG_LEGACY_CFG \
	CFG(CFG_BLE_SCAN_COEX_POLICY)
#endif
