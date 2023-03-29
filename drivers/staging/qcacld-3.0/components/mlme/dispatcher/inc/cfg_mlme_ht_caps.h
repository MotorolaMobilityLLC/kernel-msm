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

#ifndef __CFG_MLME_HT_CAPS_H
#define __CFG_MLME_HT_CAPS_H

/*
 * <ini>
 * gTxLdpcEnable - Config Param to enable Tx LDPC capability
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to enable/disable Tx LDPC capability
 * 0 - disable
 * 1 - HT LDPC enable
 * 2 - VHT LDPC enable
 * 3 - HT & VHT LDPC enable
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Concurrency/Standalone
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_LDPC_ENABLE CFG_INI_UINT( \
		"gTxLdpcEnable", \
		0, \
		3, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"Tx LDPC capability")

/*
 * <ini>
 * gEnableRXLDPC - Config Param to enable Rx LDPC capability
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable Rx LDPC capability
 * 0 - disable Rx LDPC
 * 1 - enable Rx LDPC
 *
 * Related: STA/SAP/P2P/IBSS/NAN.
 *
 * Supported Feature: Concurrency/Standalone
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_LDPC_ENABLE CFG_INI_BOOL( \
		"gEnableRXLDPC", \
		1, \
		"Rx LDPC capability")

/*
 * <ini>
 * gEnableTXSTBC - Enables/disables Tx STBC capability in STA mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default Tx STBC capability
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_TX_STBC_ENABLE CFG_INI_BOOL( \
		"gEnableTXSTBC", \
		0, \
		"Tx STBC capability")

/*
 * <ini>
 * gEnableRXSTBC - Enables/disables Rx STBC capability in STA mode
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default Rx STBC capability
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_RX_STBC_ENABLE CFG_INI_BOOL( \
		"gEnableRXSTBC", \
		1, \
		"Rx STBC capability")

/*
 * <ini>
 * gShortGI20Mhz - Short Guard Interval for HT20
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default short interval for HT20
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SHORT_GI_20MHZ CFG_INI_BOOL( \
		"gShortGI20Mhz", \
		1, \
		"Short Guard Interval for HT20")

/*
 * <ini>
 * gShortGI40Mhz - It will check gShortGI20Mhz and
 * gShortGI40Mhz from session entry
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default gShortGI40Mhz
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SHORT_GI_40MHZ CFG_INI_BOOL( \
		"gShortGI40Mhz", \
		1, \
		"Short Guard Interval for HT40")

#define CFG_HT_CAP_INFO CFG_UINT( \
		"ht_cap_info", \
		0, \
		65535, \
		364, \
		CFG_VALUE_OR_DEFAULT, \
		"HT cap info")

/*
 * <ini>
 * gShortPreamble - Set Short Preamble
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default short Preamble
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SHORT_PREAMBLE CFG_INI_BOOL( \
		"gShortPreamble", \
		1, \
		"Short Preamble")

#define CFG_HT_AMPDU_PARAMS CFG_UINT( \
		"ht_ampdu_params", \
		0, \
		255, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"HT AMPDU Params")

#define CFG_EXT_HT_CAP_INFO CFG_UINT( \
		"ext_ht_cap_info", \
		0, \
		65535, \
		1024, \
		CFG_VALUE_OR_DEFAULT, \
		"HT Ext Cap Info")

#define CFG_HT_INFO_FIELD_1 CFG_UINT( \
		"ht_info_field_1", \
		0, \
		255, \
		15, \
		CFG_VALUE_OR_DEFAULT, \
		"HT Info Field 1")

#define CFG_HT_INFO_FIELD_2 CFG_UINT( \
		"ht_info_field_2", \
		0, \
		65535, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"HT Info Field 2")

#define CFG_HT_INFO_FIELD_3 CFG_UINT( \
		"ht_info_field_3", \
		0, \
		65535, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"HT Info Field 3")

/*
 * <ini>
 * gEnableHtSMPS - Enable the SM Power Save
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable SM Power Save
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_HT_SMPS CFG_INI_BOOL( \
	"gEnableHtSMPS", \
	0, \
	"Enable HT SM PowerSave")

/*
 * <ini>
 * gHtSMPS - SMPS Mode
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to set default SM Power Save Antenna mode
 * 0 - Static
 * 1 - Dynamic
 * 2 - Reserved/Invalid
 * 3 - Disabled
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_HT_SMPS_MODE CFG_INI_UINT( \
	"gHtSMPS", \
	0, \
	3, \
	3, \
	CFG_VALUE_OR_DEFAULT, \
	"HT SM Power Save Config")

/*
 * <ini>
 * gMaxAmsduNum - Max number of MSDU's in aggregate
 * @Min: 0
 * @Max: 15
 * @Default: 0
 *
 * gMaxAmsduNum is the number of MSDU's transmitted in the aggregated
 * frame. Setting it to a value larger than 1 enables transmit aggregation.
 * Set the value to 0 to enable FW automode selection where it decides
 * the maximum number of MSDUs in AMSDU based on connection mode.
 *
 * It is a PHY parameter that applies to all vdev's in firmware.
 *
 * Supported Feature: 11n aggregation
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_MAX_AMSDU_NUM CFG_INI_UINT( \
	"gMaxAmsduNum", \
	0, \
	15, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Max AMSDU Number")

/*
 * <ini>
 * gMaxRxAmpduFactor - Provide the maximum ampdu factor.
 * @Min: 0
 * @Max: 3
 * @Default: 3
 *
 * This ini is used to set default maxampdu factor
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MAX_RX_AMPDU_FACTOR CFG_INI_UINT( \
	"gMaxRxAmpduFactor", \
	0, \
	3, \
	3, \
	CFG_VALUE_OR_DEFAULT, \
	"Max Rx AMPDU Factor")

/*
 * <ini>
 * ght_mpdu_density - Configuration option for HT MPDU density
 * @Min: 0
 * @Max: 7
 * @Default: 7
 *
 * This ini is used to set default MPDU Density
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * As per (Table 8-125 802.11-2012)
 * 0 for no restriction
 * 1 for 1/4 micro sec
 * 2 for 1/2 micro sec
 * 3 for 1 micro sec
 * 4 for 2 micro sec
 * 5 for 4 micro sec
 * 6 for 8 micro sec
 * 7 for 16 micro sec
 *
 * </ini>
 */
#define CFG_MPDU_DENSITY CFG_INI_UINT( \
	"ght_mpdu_density", \
	0, \
	7, \
	7, \
	CFG_VALUE_OR_DEFAULT, \
	"MPDU Density")

/*
 * <ini>
 * gShortSlotTimeEnabled - It will set slot timing slot.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to set default timing slot.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SHORT_SLOT_TIME_ENABLED CFG_INI_BOOL( \
	"gShortSlotTimeEnabled", \
	1, \
	"Short Slot Time Enabled")

#define CFG_HT_CAPS_ALL \
	CFG(CFG_HT_CAP_INFO) \
	CFG(CFG_TX_LDPC_ENABLE) \
	CFG(CFG_RX_LDPC_ENABLE) \
	CFG(CFG_TX_STBC_ENABLE) \
	CFG(CFG_RX_STBC_ENABLE) \
	CFG(CFG_SHORT_GI_20MHZ) \
	CFG(CFG_SHORT_GI_40MHZ) \
	CFG(CFG_SHORT_PREAMBLE) \
	CFG(CFG_HT_AMPDU_PARAMS) \
	CFG(CFG_EXT_HT_CAP_INFO) \
	CFG(CFG_HT_INFO_FIELD_1) \
	CFG(CFG_HT_INFO_FIELD_2) \
	CFG(CFG_HT_INFO_FIELD_3) \
	CFG(CFG_ENABLE_HT_SMPS) \
	CFG(CFG_HT_SMPS_MODE) \
	CFG(CFG_MAX_AMSDU_NUM) \
	CFG(CFG_MAX_RX_AMPDU_FACTOR) \
	CFG(CFG_MPDU_DENSITY) \
	CFG(CFG_SHORT_SLOT_TIME_ENABLED)

#endif /* __CFG_MLME_HT_CAPS_H */
