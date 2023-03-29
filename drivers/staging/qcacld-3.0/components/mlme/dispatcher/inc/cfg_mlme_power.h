/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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

#ifndef __CFG_MLME_POWER_H
#define __CFG_MLME_POWER_H

/*
 * <cfg>
 * max_tx_power_24 - max tx power allowed for 2.4 ghz
 * @Min: 0 minimum length of tx power
 * @Max: default data length of tx power in string format
 * @Default: 1, 14, 20
 *
 * This ini contains the string in the form of first_channel number,
 * number of channels and max tx power triplets
 */
#define CFG_MAX_TX_POWER_2_4_DATA "1, 14, 20"
#define CFG_MAX_TX_POWER_2_4 CFG_STRING( \
		"max_tx_power_24", \
		0, \
		sizeof(CFG_MAX_TX_POWER_2_4_DATA) - 1, \
		CFG_MAX_TX_POWER_2_4_DATA, \
		"max tx power 24")

/*
 * <cfg>
 * max_tx_power_5 - max tx power allowed for 5 ghz
 * @Min: 0 minimum length of tx power
 * @Max: default data length of tx power in string format
 * @Default: 36, 126, 20
 *
 * This ini contains the string in the form of first_channel number,
 * number of channels and max tx power triplets
 */
#define CFG_MAX_TX_POWER_5_DATA "36, 126, 20"
#define CFG_MAX_TX_POWER_5 CFG_STRING( \
		"max_tx_power_5", \
		0, \
		sizeof(CFG_MAX_TX_POWER_5_DATA) - 1, \
		CFG_MAX_TX_POWER_5_DATA, \
		"max tx power 5")

/*
 * <ini>
 * gPowerUsage - power usage name
 * @Min: "Min" - minimum power usage
 * @Max: "Max" - maximum power usage
 * @Default: "Mod"
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_POWER_USAGE CFG_INI_STRING( \
		"gPowerUsage", \
		0, \
		3, \
		"Mod", \
		"power usage")
/*
 * <ini>
 * TxPower2g - Limit power in case of 2.4ghz
 * @Min: 0
 * @Max: 30
 * @Default: 30
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_SET_TXPOWER_LIMIT2G CFG_INI_UINT( \
			"TxPower2g", \
			0, \
			30, \
			30, \
			CFG_VALUE_OR_DEFAULT, \
			"power limit 2g")
/*
 * <ini>
 * TxPower5g - Limit power in case of 5ghz
 * @Min: 0
 * @Max: 30
 * @Default: 30
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_SET_TXPOWER_LIMIT5G CFG_INI_UINT( \
			"TxPower5g", \
			0, \
			30, \
			30, \
			CFG_VALUE_OR_DEFAULT, \
			"power limit 5g")

/*
 * <cfg>
 * current_tx_power_level - current tx power level
 * @Min: 0
 * @Max: 128
 * @Default: 27
 */
#define CFG_CURRENT_TX_POWER_LEVEL CFG_UINT( \
		"current_tx_power_level", \
		0, \
		128, \
		27, \
		CFG_VALUE_OR_DEFAULT, \
		"current tx power level")

/*
 * <cfg>
 * local_power_constraint - local power constraint
 * @Min: 0
 * @Max: 255
 * @Default: 0
 */
#define CFG_LOCAL_POWER_CONSTRAINT CFG_UINT( \
		"local_power_constraint", \
		0, \
		255, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"local power constraint")

/*
 * <ini>
 * use_local_tpe - use local or regulatory TPE
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to set the preference of local or regulatory TPE. If the
 * preferred choice is not available, it will fall back on the other choice.
 *
 * Related: None
 *
 * Supported Feature: 6GHz channel transmit power
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_USE_LOCAL_TPE CFG_INI_BOOL("use_local_tpe", false, \
					"use local or regulatory TPE")

/*
 * <ini>
 * skip_tpe_consideration - Skip TPE IE value in tx power calculation for
 * 2G/5G bands
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is to determine if the TPE IE should be considered in the Tx power
 * calculation. If the ini is set, host will consider TPE IE in case of 6GHz
 * only (skip over in 2GHz or 5GHz case). If the ini is not set, honor the TPE
 * IE values in all bands.
 *
 * Related: None
 *
 * Supported Feature: Transmit power calculation (TPC)
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_SKIP_TPE_CONSIDERATION CFG_INI_BOOL("skip_tpe_consideration", \
						true, \
						"consider TPE IE in tx power")

#define CFG_MLME_POWER_ALL \
	CFG(CFG_MAX_TX_POWER_2_4) \
	CFG(CFG_MAX_TX_POWER_5) \
	CFG(CFG_POWER_USAGE) \
	CFG(CFG_SET_TXPOWER_LIMIT2G) \
	CFG(CFG_SET_TXPOWER_LIMIT5G) \
	CFG(CFG_CURRENT_TX_POWER_LEVEL) \
	CFG(CFG_LOCAL_POWER_CONSTRAINT) \
	CFG(CFG_USE_LOCAL_TPE) \
	CFG(CFG_SKIP_TPE_CONSIDERATION)

#endif /* __CFG_MLME_POWER_H */
