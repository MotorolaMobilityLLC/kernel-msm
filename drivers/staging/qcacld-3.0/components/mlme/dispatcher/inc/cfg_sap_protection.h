/*
 * Copyright (c) 2012-2019 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains centralized definitions of sap erp protection related
 * converged configurations.
 */

#ifndef __CFG_MLME_SAP_PROTECTION_H
#define __CFG_MLME_SAP_PROTECTION_H

#define CFG_PROTECTION_ENABLED CFG_UINT( \
		"protection_enabled", \
		0, \
		65535, \
		65535, \
		CFG_VALUE_OR_DEFAULT, \
		"sap protection enabled")

#define CFG_FORCE_POLICY_PROTECTION CFG_UINT( \
		"protection_force_policy", \
		0, \
		5, \
		5, \
		CFG_VALUE_OR_DEFAULT, \
		"force policy protection")

/*
 * <ini>
 * gignore_peer_ht_opmode
 *
 * @min 0
 * @max 1
 * @default 1
 *
 * Enabling gignore_peer_ht_opmode will enable 11g
 * protection only when there is a 11g AP in vicinity.
 *
 * Related: None
 *
 * Supported Feature: SAP Protection
 * </ini>
 */
#define CFG_IGNORE_PEER_HT_MODE CFG_INI_BOOL( \
		"gignore_peer_ht_opmode", \
		false, \
		"ignore the peer ht mode")

/*
 * <ini>
 * gEnableApProt - Enable/Disable AP protection
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to enable/disable AP protection
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_ENABLE_PROTECTION_MODE CFG_INI_BOOL( \
		"gEnableApProt", \
		true, \
		"enable protection on sap")

/*
 * <ini>
 * gApProtection - Set AP protection parameter
 * @Min: 0x0
 * @Max: 0xFFFF
 * @Default: 0xBFFF
 *
 * This ini is used to set AP protection parameter
 * Bit map for CFG_AP_PROTECTION_MODE_DEFAULT
 * LOWER byte for associated stations
 * UPPER byte for overlapping stations
 * each byte will have the following info
 * bit15 bit14 bit13     bit12  bit11 bit10    bit9     bit8
 * OBSS  RIFS  LSIG_TXOP NON_GF HT20  FROM_11G FROM_11B FROM_11A
 * bit7  bit6  bit5      bit4   bit3  bit2     bit1     bit0
 * OBSS  RIFS  LSIG_TXOP NON_GF HT_20 FROM_11G FROM_11B FROM_11A
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_PROTECTION_MODE CFG_INI_UINT( \
				"gApProtection", \
				0x0, \
				0xFFFF, \
				0xBFFF, \
				CFG_VALUE_OR_DEFAULT, \
				"AP protection mode bitmap")

/*
 * <ini>
 * gEnableApOBSSProt - Enable/Disable AP OBSS protection
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable AP OBSS protection
 *
 * Related: None.
 *
 * Supported Feature: SAP
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_AP_OBSS_PROTECTION_ENABLE CFG_INI_BOOL( \
		"gEnableApOBSSProt", \
		false, \
		"Enable/Disable AP OBSS protection")

#define CFG_SAP_PROTECTION_ALL \
	CFG(CFG_PROTECTION_ENABLED) \
	CFG(CFG_FORCE_POLICY_PROTECTION) \
	CFG(CFG_IGNORE_PEER_HT_MODE) \
	CFG(CFG_AP_ENABLE_PROTECTION_MODE) \
	CFG(CFG_AP_PROTECTION_MODE) \
	CFG(CFG_AP_OBSS_PROTECTION_ENABLE)

#endif /* __CFG_MLME_SAP_PROTECTION_H */
