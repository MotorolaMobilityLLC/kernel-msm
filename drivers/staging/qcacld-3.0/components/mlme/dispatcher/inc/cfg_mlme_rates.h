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

#ifndef __CFG_MLME_RATES_H
#define __CFG_MLME_RATES_H

#define CFG_SUPPORTED_RATES_11B_LEN    4
#define CFG_SUPPORTED_RATES_11A_LEN    8
#define CFG_OPERATIONAL_RATE_SET_LEN    12
#define CFG_EXTENDED_OPERATIONAL_RATE_SET_LEN    8
#define CFG_SUPPORTED_MCS_SET_LEN    16
#define CFG_BASIC_MCS_SET_LEN    16
#define CFG_CURRENT_MCS_SET_LEN    16
#define CFG_MLME_RATE_MASK_LEN    4

/*
 * <ini>
 * gMaxHTMCSForTxData - max HT mcs for TX
 * @Min: 0
 * @Max: 383
 * @Default: 0
 *
 * This ini is used to configure the max HT mcs
 * for tx data.
 *
 * Usage: External
 *
 * bits 0-15:  max HT mcs
 * bits 16-31: zero to disable, otherwise enable.
 *
 * </ini>
 */
#define CFG_MAX_HT_MCS_FOR_TX_DATA CFG_INI_UINT( \
		"gMaxHTMCSForTxData", \
		0, \
		0x17f, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"Max HT Mcs for Tx Data")

/*
 * <ini>
 * gDisableABGRateForTxData - disable abg rate for tx data
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to disable abg rate for tx data.
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_ABG_RATE_FOR_TX_DATA CFG_INI_BOOL( \
		"gDisableABGRateForTxData", \
		0, \
		"Disable ABG RATE for TX Data")

/*
 * <ini>
 * gSapMaxMCSForTxData - sap 11n max mcs
 * @Min: 0
 * @Max: 383
 * @Default: 0
 *
 * This ini configure SAP 11n max mcs
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAP_MAX_MCS_FOR_TX_DATA CFG_INI_UINT( \
		"gSapMaxMCSForTxData", \
		0, \
		383, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"SAP Max MCS for TX Data")

/*
 * <ini>
 * disable_high_ht_mcs_2x2 - disable high mcs index for 2nd stream in 2.4G
 * @Min: 0
 * @Max: 8
 * @Default: 0
 *
 * This ini is used to disable high HT MCS index for 2.4G STA connection.
 * It has been introduced to resolve IOT issue with one of the vendor.
 *
 * Note: This INI is not useful with 1x1 setting. If some platform supports
 * only 1x1 then this INI is not useful.
 *
 * 0 - It won't disable any HT MCS index (just like normal HT MCS)
 * 1 - It will disable 15th bit from HT RX MCS set (from 8-15 bits slot)
 * 2 - It will disable 14th & 15th bits from HT RX MCS set
 * 3 - It will disable 13th, 14th, & 15th bits from HT RX MCS set
 * and so on.
 *
 * Related: STA
 *
 * Supported Feature: 11n
 *
 * Usage: External
 */
#define CFG_DISABLE_HIGH_HT_RX_MCS_2x2 CFG_INI_UINT( \
		"disable_high_ht_mcs_2x2", \
		0, \
		8, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"Disable high MCS index for 2x2")

#define CFG_CFP_PERIOD CFG_UINT( \
		"cfpPeriod", \
		0, \
		255, \
		1, \
		CFG_VALUE_OR_DEFAULT, \
		"CFP Period")

#define CFG_CFP_MAX_DURATION CFG_UINT( \
		"cfpMaxDuration", \
		0, \
		65535, \
		30000, \
		CFG_VALUE_OR_DEFAULT, \
		"CFP Max Duration")
/*
 * <cfg>
 * supported_rates_11b - supported rates for 11b
 * @Min: 0 minimum length of supported rates
 * @Max: default data length of supported rates in string format
 * @Default: 2, 4, 11, 22
 */
#define CFG_SUPPORTED_RATES_11B_DATA "2, 4, 11, 22"
#define CFG_SUPPORTED_RATES_11B CFG_STRING( \
		"supported_rates_11b", \
		0, \
		sizeof(CFG_SUPPORTED_RATES_11B_DATA) - 1, \
		CFG_SUPPORTED_RATES_11B_DATA, \
		"Supported rates for 11B")

/*
 * <cfg>
 * supported_rates_11a - supported rates for 11a
 * @Min: 0 minimum length of supported rates
 * @Max: default data length of supported rates in string format
 * @Default: 12, 18, 24, 36, 48, 72, 96, 108
 */
#define CFG_SUPPORTED_RATES_11A_DATA "12, 18, 24, 36, 48, 72, 96, 108"
#define CFG_SUPPORTED_RATES_11A CFG_STRING( \
		"supported_rates_11a", \
		0, \
		sizeof(CFG_SUPPORTED_RATES_11A_DATA) - 1, \
		CFG_SUPPORTED_RATES_11A_DATA, \
		"Supported rates for 11A")

/*
 * <cfg>
 * supported_mcs_set - supported MCS set data
 * @Min: 0 minimum length of supported MCS set
 * @Max: default data length of supported mcs set in string format
 * @Default: 0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
 * 0x0, 0x0, 0x0
 */
#define CFG_SUPPORTED_MCS_SET_DATA "0xff, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0"
#define CFG_SUPPORTED_MCS_SET CFG_STRING( \
		"supported_mcs_set", \
		0, \
		sizeof(CFG_SUPPORTED_MCS_SET_DATA) - 1, \
		CFG_SUPPORTED_MCS_SET_DATA, \
		"supported MCS set")

/*
 * <cfg>
 * basic_mcs_set - basic MCS set data
 * @Min: 0 minimum length of basic MCS set
 * @Max: default data length of basic mcs set in string format
 * @Default: 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
 * 0x0, 0x0, 0x0
 */
#define CFG_BASIC_MCS_SET_DATA "0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0"
#define CFG_BASIC_MCS_SET CFG_STRING( \
		"basic_mcs_set", \
		0, \
		sizeof(CFG_BASIC_MCS_SET_DATA) - 1, \
		CFG_BASIC_MCS_SET_DATA, \
		"basic MCS set")

/*
 * <cfg>
 * current_mcs_set - current MCS set data
 * @Min: 0 minimum length of current MCS set
 * @Max: default data length of current mcs set in string format
 * @Default: 0x00, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0,
 * 0x0, 0x0, 0x0
 */
#define CFG_CURRENT_MCS_SET_DATA "0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0, 0x0"
#define CFG_CURRENT_MCS_SET CFG_STRING( \
		"current_mcs_set", \
		0, \
		sizeof(CFG_CURRENT_MCS_SET_DATA) - 1, \
		CFG_CURRENT_MCS_SET_DATA, \
		"current MCS set")

/*
 * <cfg>
 * ratemask_type - PHY type for the ratemask.
 * @Min: 0 No rate mask set defined - disabled the configuration
 * @Max: 4
 * @Default: 0
 *
 * This ini is used to set the PHY type for ratemask in rate selection.
 *
 * 0 = Disables the configuration
 * 1 = The rate mask specified is for CCK/OFDM configuration
 * 2 = The rate mask specified is for HT configuration
 * 3 = The rate mask specified is for VHT configuration
 * 4 = The rate mask specified is for HE/11ax configuration
 *
 * Related: CFG_RATEMASK_SET
 *
 * Usage: External
 */
#define CFG_RATEMASK_TYPE CFG_INI_UINT( \
		"ratemask_type", \
		0, \
		4, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"Ratemask type")

/*
 * <cfg>
 * ratemask_set - ratemasks for a PHY type used in rate selection
 * @Min: default data length of ratemask in string format
 * @Max: default data length of ratemask in string format
 * @Default: 0xFFFFFFFF,0xFFFFFFFF,0xFFFFFFFF, 0xFFFFFFFF
 *
 * This is used to set the rate mask value to be used in rate selection.
 * Each of the four words must be configured.
 * A bit value of 1 represents rate is enabled
 * A bit value of 0 represents rate is disabled
 *
 * [b31-b0],[b63-b32],[b95-b64],[b127-b96]
 * For HE targets, 12 bits correpond to one NSS setting. Ex:
 * b0-13  => NSS1, MCS 0-13
 * b14-27 => NSS2, MCS 0-13 and so on for other NSS.
 * Note that the bit representation is continuous.
 *
 * For VHT targets, 10 bits correspond to one NSS setting.
 * b0-9   => NSS1, MCS 0-9
 * b10-19 => NSS2, MCS 0-9 and so on for other NSS.
 *
 * For HT targets, 8 bits correspond to one NSS setting.
 * b0-7  => NSS1, MCS 0-7
 * b8-15 => NSS2, MCS 0-7 and so on for other NSS.
 *
 * For OFDM/CCK targets, 8 bits correspond to one NSS setting.
 * Bit position  |-b3-|-b2-|-b1-|-b0-|
 * Rates in Mbps |-1 -|-2 -|-5.5|-11-| CCK Rates
 *
 * Bit position  |-b11-|-b10-|-b09-|-b08-|-b07-|-b06-|-b05-|-b04-|
 * Rates in Mbps |- 9 -|- 18-|-36 -|-54 -|- 6 -|-12 -| -24-|-48- | OFDM Rates
 *
 * Related: CFG_RATEMASK_TYPE
 *
 * Usage: External
 */
#define CFG_RATEMASK_DATA "0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF, 0xFFFFFFFF"
#define CFG_RATEMASK_SET CFG_INI_STRING( \
		"ratemask_set", \
		0, \
		sizeof(CFG_RATEMASK_DATA) - 1, \
		CFG_RATEMASK_DATA, \
		"Ratemasks for rate selection")

#define CFG_RATES_ALL \
	CFG(CFG_MAX_HT_MCS_FOR_TX_DATA) \
	CFG(CFG_DISABLE_ABG_RATE_FOR_TX_DATA) \
	CFG(CFG_SAP_MAX_MCS_FOR_TX_DATA) \
	CFG(CFG_DISABLE_HIGH_HT_RX_MCS_2x2) \
	CFG(CFG_CFP_PERIOD) \
	CFG(CFG_CFP_MAX_DURATION) \
	CFG(CFG_SUPPORTED_RATES_11B) \
	CFG(CFG_SUPPORTED_RATES_11A) \
	CFG(CFG_SUPPORTED_MCS_SET) \
	CFG(CFG_BASIC_MCS_SET) \
	CFG(CFG_CURRENT_MCS_SET) \
	CFG(CFG_RATEMASK_TYPE) \
	CFG(CFG_RATEMASK_SET)

#endif /* __CFG_MLME_RATES_H */
