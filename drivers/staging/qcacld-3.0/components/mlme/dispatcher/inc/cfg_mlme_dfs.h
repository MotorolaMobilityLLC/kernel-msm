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

#ifndef __CFG_MLME_DFS_H
#define __CFG_MLME_DFS_H

/*
 * <ini>
 * gsap_tx_leakage_threshold - sap tx leakage threshold
 * @Min: 100
 * @Max: 1000
 * @Default: 310
 *
 * customer can set this value from 100 to 1000 which means
 * sap tx leakage threshold is -10db to -100db
 *
 * Related: none
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SAP_TX_LEAKAGE_THRESHOLD CFG_INI_UINT( \
			"gsap_tx_leakage_threshold", \
			100, \
			1000, \
			310, \
			CFG_VALUE_OR_DEFAULT, \
			"sap tx leakage threshold")

/*
 * <ini>
 * gDFSradarMappingPriMultiplier - dfs pri multiplier
 * @Min: 1
 * @Max: 10
 * @Default: 2
 *
 * customer can set this value from 1 to 10 which means
 * host could handle missing pulses while there is high
 * channel loading, for example: 30% ETSI and 50% Japan W53
 *
 * Related: none
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DFS_RADAR_PRI_MULTIPLIER CFG_INI_UINT( \
			"gDFSradarMappingPriMultiplier", \
			1, \
			10, \
			2, \
			CFG_VALUE_OR_DEFAULT, \
			"dfs pri multiplier")

/*
 * <ini>
 * gDfsBeaconTxEnhanced - beacon tx enhanced
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enhance dfs beacon tx
 *
 * Related: none
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DFS_BEACON_TX_ENHANCED CFG_INI_BOOL( \
			"gDfsBeaconTxEnhanced", \
			0, \
			"beacon tx enhanced")

/*
 * <ini>
 * gPreferNonDfsChanOnRadar - During random channel selection prefer non dfs
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * During random channel selection prefer non dfs.
 *
 * Related: none
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_NON_DFS_CHAN_ON_RADAR CFG_INI_BOOL( \
			"gPreferNonDfsChanOnRadar", \
			0, \
			"channel selection prefer non dfs")

/*
 * <ini>
 * dfsPhyerrFilterOffload - Enable dfs phyerror filtering offload in FW
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to to enable dfs phyerror filtering offload to firmware
 * Enabling it will cause basic phy error to be discarding in firmware.
 * Related: NA.
 *
 * Supported Feature: DFS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_DFS_PHYERR_FILTEROFFLOAD CFG_INI_BOOL( \
			"dfsPhyerrFilterOffload", \
			0, \
			"dfs phyerror filtering offload")

/*
 * <ini>
 * gIgnoreCAC - Used to ignore CAC
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set default CAC
 *
 * Related: None
 *
 * Supported Feature: DFS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_IGNORE_CAC CFG_INI_BOOL( \
			"gIgnoreCAC", \
			0, \
			"ignore CAC on DFS channel")

/*
 * <ini>
 * gDisableDFSChSwitch - Disable channel switch if radar is found
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to disable channel switch if radar is found
 * on that channel.
 * Related: NA.
 *
 * Supported Feature: DFS
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_DISABLE_DFS_CH_SWITCH CFG_INI_BOOL( \
			"gDisableDFSChSwitch", \
			0, \
			"Disable channel switch on radar")

/*
 * <ini>
 * gEnableDFSMasterCap - Enable DFS master capability
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable the DFS master capability.
 * Disabling it will cause driver to not advertise the spectrum
 * management capability
 * Related: NA.
 *
 * upported Feature: DFS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_ENABLE_DFS_MASTER_CAPABILITY CFG_INI_BOOL( \
			"gEnableDFSMasterCap", \
			0, \
			"DFS master mode capability")

/*
 * <ini>
 * gDisableDfsJapanW53 - Block W53 channels in random channel selection
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to block W53 Japan channel in random channel selection
 *
 * Related: none
 *
 * Supported Feature: DFS
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_DISABLE_DFS_JAPAN_W53 CFG_INI_BOOL( \
			"gDisableDfsJapanW53", \
			0, \
			"Block W53 channels in random selection")

#define CFG_DFS_ALL \
	CFG(CFG_IGNORE_CAC) \
	CFG(CFG_DISABLE_DFS_CH_SWITCH) \
	CFG(CFG_DFS_BEACON_TX_ENHANCED) \
	CFG(CFG_SAP_TX_LEAKAGE_THRESHOLD) \
	CFG(CFG_DFS_RADAR_PRI_MULTIPLIER) \
	CFG(CFG_ENABLE_NON_DFS_CHAN_ON_RADAR) \
	CFG(CFG_ENABLE_DFS_MASTER_CAPABILITY) \
	CFG(CFG_DISABLE_DFS_JAPAN_W53) \
	CFG(CFG_ENABLE_DFS_PHYERR_FILTEROFFLOAD)

#endif /* __CFG_MLME_DFS_H */
