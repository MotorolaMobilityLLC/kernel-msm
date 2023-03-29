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

#ifndef __CFG_MLME_WIFI_POS_H
#define __CFG_MLME_WIFI_POS_H

/*
 * <ini>
 * gfine_time_meas_cap - fine timing measurement capability information
 * @Min: 0x0000
 * @Max: 0x00BD
 * @Default: 0x000D
 *
 * fine timing measurement capability information
 *
 * <----- fine_time_meas_cap (in bits) ----->
 * +---------+-----+-----+-----+-----+------+------+-------+-------+-----+-----+
 * |  10-31  |  9  |  8  |  7  |  6  |   5  |   4  |   3   |   2   |  1  |  0  |
 * +---------+-----+-----+-----+-----+------+------+-------+-------+-----+-----+
 * | reserved| NAN | NAN | SAP | SAP |P2P-GO|P2P-GO|P2P-CLI|P2P-CLI| STA | STA |
 * |         | resp|init |resp |init |resp  |init  |resp   |init   |resp |init |
 * +---------+-----+-----+-----+-----+------+------+-------+-------+-----+-----+
 *
 * resp - responder role; init- initiator role
 *
 * CFG_FINE_TIME_MEAS_CAPABILITY_MAX computed based on the table
 * +-----------------+-----------------+-----------+
 * |  Device Role    |   Initiator     | Responder |
 * +-----------------+-----------------+-----------+
 * |   Station       |       Y         |     N     |
 * |   P2P-CLI       |       Y         |     Y     |
 * |   P2P-GO        |       Y         |     Y     |
 * |   SAP           |       N         |     Y     |
 * +-----------------+-----------------+-----------+
 *
 * Related: None
 *
 * Supported Feature: WIFI POS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_FINE_TIME_MEAS_CAPABILITY CFG_INI_UINT( \
			"gfine_time_meas_cap", \
			0x0000, \
			0x003BD, \
			0x0030D, \
			CFG_VALUE_OR_DEFAULT, \
			"fine timing measurement capability")

/*
 * <ini>
 * oem_6g_support_disable - oem 6g support is disabled
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to show OEM is 6Ghz disabled. For legacy OEM apps
 * having no support for 6Ghz, the default value is 1 and thus driver will
 * not serve 6Ghz info to legacy oem application.
 * OEM apps supporting 6Ghz sets the ini value to 0 to get 6Ghz
 * information from driver.
 *
 * Related: None
 *
 * Supported Feature: WIFI POS
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_OEM_SIXG_SUPPORT_DISABLE CFG_INI_BOOL( \
		"oem_6g_support_disable", \
		1, \
		"oem 6Ghz support Enabled/disabled")

#define CFG_WIFI_POS_ALL \
	CFG(CFG_FINE_TIME_MEAS_CAPABILITY) \
	CFG(CFG_OEM_SIXG_SUPPORT_DISABLE)

#endif /* __CFG_MLME_WIFI_POS_H */
