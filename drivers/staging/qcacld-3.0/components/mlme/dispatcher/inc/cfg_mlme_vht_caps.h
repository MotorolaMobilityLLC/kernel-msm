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

#ifndef __CFG_MLME_VHT_CAPS_H
#define __CFG_MLME_VHT_CAPS_H

#define CFG_VHT_SUPP_CHAN_WIDTH CFG_UINT( \
		"supp_chan_width", \
		0, \
		2, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT SUPPORTED CHAN WIDTH SET")

/*
 * <ini>
 * gTxBFCsnValue - ini to set VHT/HE STS Caps field
 * @Min: 0
 * @Max: 7
 * @Default: 7
 *
 * This ini is used to configure the STS capability shown in AC/AX mode
 * MGMT frame IE, the final STS field shown in VHT/HE IE will be calculated
 * by MIN of (INI set, target report value). Only if gTxBFEnable is enabled
 * and SU/MU BEAMFORMEE Caps is shown, then STS Caps make sense.
 *
 * Related: gTxBFEnable.
 *
 * Supported Feature: STA/SAP
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_VHT_BEAMFORMEE_ANT_SUPP CFG_INI_UINT( \
		"txBFCsnValue", \
		0, \
		7, \
		7, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT BEAMFORMEE ANTENNA SUPPORTED CAP")

/*
 * <ini>
 * gEnableTxSUBeamformer - Enables TX Su beam former
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_TX_SU_BEAM_FORMER CFG_INI_BOOL( \
		"gEnableTxSUBeamformer", \
		0, \
		"vht tx su beam former")

#define CFG_VHT_NUM_SOUNDING_DIMENSIONS CFG_UINT( \
		"num_soundingdim", \
		0, \
		3, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT NUMBER OF SOUNDING DIMENSIONS")

#define CFG_VHT_HTC_VHTC CFG_BOOL( \
		"htc_vhtc", \
		0, \
		"VHT HTC VHTC")

#define CFG_VHT_LINK_ADAPTATION_CAP CFG_UINT( \
		"link_adap_cap", \
		0, \
		3, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT LINK ADAPTATION CAP")

#define CFG_VHT_RX_ANT_PATTERN CFG_BOOL( \
		"rx_antpattern", \
		1, \
		"VHT RX ANTENNA PATTERN CAP")

#define CFG_VHT_TX_ANT_PATTERN CFG_BOOL( \
		"tx_antpattern", \
		1, \
		"VHT TX ANTENNA PATTERN CAP")

#define CFG_VHT_RX_SUPP_DATA_RATE CFG_UINT( \
		"rx_supp_data_rate", \
		0, \
		780, \
		780, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT RX SUPP DATA RATE")

#define CFG_VHT_TX_SUPP_DATA_RATE CFG_UINT( \
		"tx_supp_data_rate", \
		0, \
		780, \
		780, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT TX SUPP DATA RATE")

#define CFG_TX_BF_CAP CFG_UINT( \
		"tx_bf_cap", \
		0, \
		4294967295, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"TX BF CAP")

#define CFG_AS_CAP CFG_UINT( \
		"as_cap", \
		0, \
		255, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"AS CAP")

/*
 * <ini>
 * gDisableLDPCWithTxbfAP - Disable LDPC with tx bf AP
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_LDPC_WITH_TXBF_AP CFG_INI_BOOL( \
		"gDisableLDPCWithTxbfAP", \
		0, \
		"Disable LDPC with tx bf AP")

/*
 * <ini>
 * gTxBFEnable - Enables SU beamformee caps
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_SU_BEAMFORMEE_CAP CFG_INI_BOOL( \
		"gTxBFEnable", \
		1, \
		"VHT SU BEAMFORMEE CAPABILITY")

/*
 * <ini>
 * gEnableTxBFin20MHz - Enables TXBF in 20mhz
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_TXBF_IN_20MHZ CFG_INI_BOOL( \
		"gEnableTxBFin20MHz", \
		0, \
		"VHT ENABLE TXBF 20MHZ")

#define CFG_VHT_MU_BEAMFORMER_CAP CFG_BOOL( \
		"mu_bformer", \
		0, \
		"VHT MU BEAMFORMER CAP")

#define CFG_VHT_TXOP_PS CFG_BOOL( \
		"txop_ps", \
		0, \
		"VHT TXOP PS")

/*
 * <ini>
 * gVhtChannelWidth - Channel width capability for 11ac
 * @Min: 0
 * @Max: 4
 * @Default: 2
 *
 * This ini is used to set channel width capability for 11AC.
 * eHT_CHANNEL_WIDTH_20MHZ = 0,
 * eHT_CHANNEL_WIDTH_40MHZ = 1,
 * eHT_CHANNEL_WIDTH_80MHZ = 2,
 * eHT_CHANNEL_WIDTH_160MHZ = 3,
 * eHT_CHANNEL_WIDTH_80P80MHZ = 4,
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_CHANNEL_WIDTH CFG_INI_UINT( \
		"gVhtChannelWidth", \
		0, \
		4, \
		2, \
		CFG_VALUE_OR_DEFAULT, \
		"Channel width capability for 11ac")

/*
 * <ini>
 * gVhtRxMCS - VHT Rx MCS capability for 1x1 mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is  used to set VHT Rx MCS capability for 1x1 mode.
 * 0, MCS0-7
 * 1, MCS0-8
 * 2, MCS0-9
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_RX_MCS_8_9 CFG_INI_UINT( \
		"gVhtRxMCS", \
		0, \
		2, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT Rx MCS")

/*
 * <ini>
 * gVhtTxMCS - VHT Tx MCS capability for 1x1 mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is  used to set VHT Tx MCS capability for 1x1 mode.
 * 0, MCS0-7
 * 1, MCS0-8
 * 2, MCS0-9
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_TX_MCS_8_9 CFG_INI_UINT( \
		"gVhtTxMCS", \
		0, \
		2, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT Tx MCS")

/*
 * <ini>
 * gVhtRxMCS2x2 - VHT Rx MCS capability for 2x2 mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is  used to set VHT Rx MCS capability for 2x2 mode.
 * 0, MCS0-7
 * 1, MCS0-8
 * 2, MCS0-9
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_RX_MCS2x2_8_9 CFG_INI_UINT( \
		"gVhtRxMCS2x2", \
		0, \
		2, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT Rx MCS 2x2")

/*
 * <ini>
 * gVhtTxMCS2x2 - VHT Tx MCS capability for 2x2 mode
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * This ini is  used to set VHT Tx MCS capability for 2x2 mode.
 * 0, MCS0-7
 * 1, MCS0-8
 * 2, MCS0-9
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_TX_MCS2x2_8_9 CFG_INI_UINT( \
		"gVhtTxMCS2x2", \
		0, \
		2, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT Tx MCS 2x2")

/*
 * <ini>
 * enable_vht20_mcs9 - Enables VHT MCS9 in 20M BW operation
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_VHT20_MCS9 CFG_INI_BOOL( \
		"enable_vht20_mcs9", \
		1, \
		"Enables VHT MCS9 in 20M BW")

/*
 * <ini>
 * gEnable2x2 - Enables/disables VHT Tx/Rx MCS values for 2x2
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini disables/enables 2x2 mode. If this is zero then DUT operates as 1x1
 *
 * 0, Disable
 * 1, Enable
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_2x2_CAP_FEATURE CFG_INI_BOOL( \
		"gEnable2x2", \
		0, \
		"VHT Enable 2x2")

/*
 * <ini>
 * gEnableMuBformee - Enables/disables multi-user (MU) beam formee capability
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini enables/disables multi-user (MU) beam formee
 * capability
 *
 * Change MU Bformee only when  gTxBFEnable is enabled.
 * When gTxBFEnable and gEnableMuBformee are set, MU beam formee capability is
 * enabled.
 * Related:  gTxBFEnable
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE CFG_INI_BOOL( \
		"gEnableMuBformee", \
		0, \
		"VHT Enable MU Beamformee")

/*
 * <ini>
 * gEnablePAID - VHT partial AID feature
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This option enables/disables VHT partial AID feature.
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_PAID_FEATURE CFG_INI_BOOL( \
		"gEnablePAID", \
		0, \
		"VHT Enable PAID")

/*
 * <ini>
 * gEnableGID - VHT Group ID feature
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This option enables/disables VHT Group ID feature.
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_GID_FEATURE CFG_INI_BOOL( \
		"gEnableGID", \
		0, \
		"VHT Enable GID")

/*
 * <ini>
 * gEnableVhtFor24GHzBand - Enable VHT for 2.4GHZ in SAP mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_VHT_FOR_24GHZ CFG_INI_BOOL( \
		"gEnableVhtFor24GHzBand", \
		0, \
		"VHT Enable for 24GHz")

/*
 * gEnableVendorVhtFor24GHzBand - Parameter to control VHT support
 * based on vendor ie in 2.4 GHz band
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This parameter will enable SAP to read VHT capability in vendor ie in Assoc
 * Req and send VHT caps in Resp to establish connection in VHT Mode.
 * Supported Feature: SAP
 *
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_VENDOR_VHT_FOR_24GHZ CFG_INI_BOOL( \
		"gEnableVendorVhtFor24GHzBand", \
		1, \
		"VHT Enable Vendor for 24GHz")

/*
 * <ini>
 * gVhtAmpduLenExponent - maximum receive AMPDU size configuration
 * @Min: 0
 * @Max: 7
 * @Default: 3
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_AMPDU_LEN_EXPONENT CFG_INI_UINT( \
		"gVhtAmpduLenExponent", \
		0, \
		7, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT AMPDU Len in Exponent")

/*
 * <ini>
 * gVhtMpduLen - VHT MPDU length
 * @Min: 0
 * @Max: 2
 * @Default: 0
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_MPDU_LEN CFG_INI_UINT( \
		"gVhtMpduLen", \
		0, \
		2, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"VHT MPDU Length")

/*
 * <ini>
 * gEnableTxBFeeSAP - Enable / Disable Tx beamformee in SAP mode
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * Related: NA
 *
 * Supported Feature: 11AC
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_VHT_ENABLE_TXBF_SAP_MODE CFG_INI_BOOL( \
			"gEnableTxBFeeSAP", \
			0, \
			"Enable tx bf sap mode")

/*
 * <ini>
 * enable_subfee_vendor_vhtie - ini to enable/disable SU Bformee in vendor VHTIE
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable SU Bformee in vendor vht ie if gTxBFEnable
 * is enabled. if gTxBFEnable is 0 this will not have any effect.
 *
 * Related: gTxBFEnable.
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_SUBFEE_IN_VENDOR_VHTIE CFG_INI_BOOL( \
			"enable_subfee_vendor_vhtie", \
			1, \
			"Enable subfee in vendor vht ie")

#define CFG_VHT_CAPS_ALL \
	CFG(CFG_VHT_SUPP_CHAN_WIDTH) \
	CFG(CFG_VHT_SU_BEAMFORMEE_CAP) \
	CFG(CFG_VHT_BEAMFORMEE_ANT_SUPP) \
	CFG(CFG_VHT_ENABLE_TX_SU_BEAM_FORMER) \
	CFG(CFG_VHT_NUM_SOUNDING_DIMENSIONS) \
	CFG(CFG_VHT_MU_BEAMFORMER_CAP) \
	CFG(CFG_VHT_TXOP_PS) \
	CFG(CFG_VHT_HTC_VHTC) \
	CFG(CFG_VHT_LINK_ADAPTATION_CAP) \
	CFG(CFG_VHT_RX_ANT_PATTERN) \
	CFG(CFG_VHT_TX_ANT_PATTERN) \
	CFG(CFG_VHT_RX_SUPP_DATA_RATE) \
	CFG(CFG_VHT_TX_SUPP_DATA_RATE) \
	CFG(CFG_VHT_ENABLE_TXBF_IN_20MHZ) \
	CFG(CFG_VHT_CHANNEL_WIDTH) \
	CFG(CFG_VHT_ENABLE_RX_MCS_8_9) \
	CFG(CFG_VHT_ENABLE_TX_MCS_8_9) \
	CFG(CFG_VHT_ENABLE_RX_MCS2x2_8_9) \
	CFG(CFG_VHT_ENABLE_TX_MCS2x2_8_9) \
	CFG(CFG_ENABLE_VHT20_MCS9) \
	CFG(CFG_VHT_ENABLE_2x2_CAP_FEATURE) \
	CFG(CFG_VHT_ENABLE_MU_BFORMEE_CAP_FEATURE) \
	CFG(CFG_VHT_ENABLE_PAID_FEATURE) \
	CFG(CFG_VHT_ENABLE_GID_FEATURE) \
	CFG(CFG_ENABLE_VHT_FOR_24GHZ) \
	CFG(CFG_ENABLE_VENDOR_VHT_FOR_24GHZ) \
	CFG(CFG_VHT_AMPDU_LEN_EXPONENT) \
	CFG(CFG_VHT_MPDU_LEN) \
	CFG(CFG_VHT_ENABLE_TXBF_SAP_MODE) \
	CFG(CFG_ENABLE_SUBFEE_IN_VENDOR_VHTIE) \
	CFG(CFG_TX_BF_CAP) \
	CFG(CFG_AS_CAP) \
	CFG(CFG_DISABLE_LDPC_WITH_TXBF_AP)

#endif /* __CFG_MLME_VHT_CAPS_H */
