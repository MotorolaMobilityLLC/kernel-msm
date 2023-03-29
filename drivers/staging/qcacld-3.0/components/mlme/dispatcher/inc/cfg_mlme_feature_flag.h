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

#ifndef __CFG_MLME_FEATURE_FLAG_H
#define __CFG_MLME_FEATURE_FLAG_H

#define CFG_ACCEPT_SHORT_SLOT_ASSOC_ONLY CFG_BOOL( \
		"accept_short_slot_assoc", \
		0, \
		"Accept short slot assoc only")

#define CFG_HCF_ENABLED CFG_BOOL( \
		"enable_hcf", \
		0, \
		"HCF enabled")

#define CFG_RSN_ENABLED CFG_BOOL( \
		"enable_rsn", \
		0, \
		"RSN enabled")

#define CFG_11G_SHORT_PREAMBLE_ENABLED CFG_BOOL( \
		"enable_short_preamble_11g", \
		0, \
		"Short Preamble Enable")

#define CFG_11G_SHORT_SLOT_TIME_ENABLED CFG_BOOL( \
		"enable_short_slot_time_11g", \
		1, \
		"Short Slot time enable")

#define CFG_CHANNEL_BONDING_MODE CFG_UINT( \
		"channel_bonding_mode", \
		0, \
		10, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"channel bonding mode")

#define CFG_BLOCK_ACK_ENABLED CFG_UINT( \
		"enable_block_ack", \
		0, \
		3, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"enable block Ack")
/*
 * <ini>
 * gEnableAMPDUPS - Enable the AMPDUPS
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default AMPDUPS
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_ENABLE_AMPDUPS CFG_INI_BOOL( \
				"gEnableAMPDUPS", \
				0, \
				"Enable AMPDU")

/*
 * <ini>
 * gFWMccRtsCtsProtection - RTS-CTS protection in MCC.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable use of long duration RTS-CTS protection
 * when SAP goes off channel in MCC mode.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_FW_MCC_RTS_CTS_PROT CFG_INI_UINT( \
		"gFWMccRtsCtsProtection", \
		0, 1, 0, \
		CFG_VALUE_OR_DEFAULT, \
		"RTS-CTS protection in MCC")

/*
 * <ini>
 * gFWMccBCastProbeResponse - Broadcast Probe Response in MCC.
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable use of broadcast probe response to
 * increase the detectability of SAP in MCC mode.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */

#define CFG_FW_MCC_BCAST_PROB_RESP CFG_INI_UINT( \
		"gFWMccBCastProbeResponse", \
		0, 1, 0, \
		CFG_VALUE_OR_DEFAULT, \
		"Broadcast Probe Response in MCC")

/*
 * <ini>
 * gEnableMCCMode - Enable/Disable MCC feature.
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable MCC feature.
 *
 * Related: None.
 *
 * Supported Feature: Concurrency
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MCC_FEATURE CFG_INI_UINT( \
		"gEnableMCCMode", \
		0, 1, 1, \
		CFG_VALUE_OR_DEFAULT, \
		"Enable/Disable MCC feature.")

/*
 * <ini>
 * gChannelBondingMode24GHz - Configures Channel Bonding in 24 GHz
 * @Min: 0
 * @Max: 10
 * @Default: 1
 *
 * This ini is used to set default channel bonding mode 24GHZ
 *
 * 0 - 20MHz IF bandwidth centered on IF carrier
 * 1 - 40MHz IF bandwidth with lower 20MHz supporting the primary channel
 * 2 - reserved
 * 3 - 40MHz IF bandwidth with higher 20MHz supporting the primary channel
 * 4 - 20/40MHZ offset LOW 40/80MHZ offset CENTERED
 * 5 - 20/40MHZ offset CENTERED 40/80MHZ offset CENTERED
 * 6 - 20/40MHZ offset HIGH 40/80MHZ offset CENTERED
 * 7 - 20/40MHZ offset LOW 40/80MHZ offset LOW
 * 8 - 20/40MHZ offset HIGH 40/80MHZ offset LOW
 * 9 - 20/40MHZ offset LOW 40/80MHZ offset HIGH
 * 10 - 20/40MHZ offset-HIGH 40/80MHZ offset HIGH
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CHANNEL_BONDING_MODE_24GHZ CFG_INI_UINT( \
	"gChannelBondingMode24GHz", \
	0, \
	10, \
	1, \
	CFG_VALUE_OR_DEFAULT, \
	"Configures Channel Bonding in 24 GHz")

/*
 * <ini>
 * gChannelBondingMode5GHz - Configures Channel Bonding in 5 GHz
 * @Min: 0
 * @Max: 10
 * @Default: 0
 *
 * This ini is used to set default channel bonding mode 5GHZ
 *
 * Values of 0 - 10 have the same meanings as for gChannelBondingMode24GHz.
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_CHANNEL_BONDING_MODE_5GHZ CFG_INI_UINT( \
	"gChannelBondingMode5GHz", \
	0, \
	10, \
	0, \
	CFG_VALUE_OR_DEFAULT, \
	"Configures Channel Bonding in 5 GHz")

#define CFG_FEATURE_FLAG_ALL \
	CFG(CFG_ACCEPT_SHORT_SLOT_ASSOC_ONLY) \
	CFG(CFG_HCF_ENABLED) \
	CFG(CFG_RSN_ENABLED) \
	CFG(CFG_FW_MCC_RTS_CTS_PROT) \
	CFG(CFG_FW_MCC_BCAST_PROB_RESP) \
	CFG(CFG_MCC_FEATURE) \
	CFG(CFG_11G_SHORT_PREAMBLE_ENABLED) \
	CFG(CFG_11G_SHORT_SLOT_TIME_ENABLED) \
	CFG(CFG_CHANNEL_BONDING_MODE) \
	CFG(CFG_BLOCK_ACK_ENABLED) \
	CFG(CFG_ENABLE_AMPDUPS) \
	CFG(CFG_CHANNEL_BONDING_MODE_24GHZ) \
	CFG(CFG_CHANNEL_BONDING_MODE_5GHZ)

#endif /* __CFG_MLME_FEATURE_FLAG_H */

