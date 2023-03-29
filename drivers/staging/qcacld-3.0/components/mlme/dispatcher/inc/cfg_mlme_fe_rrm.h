/*
 * Copyright (c) 2012-2018, 2020 The Linux Foundation. All rights reserved.
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

#ifndef __CFG_MLME_FE_RRM_H
#define __CFG_MLME_FE_RRM_H

/*
 * <ini>
 * gRrmEnable - Enable/Disable RRM on STA
 * @Min: 0
 * @Max: 1
 * @Default: 1
 *
 * This ini is used to controls the capabilities (11 k) included
 * in the capabilities field for STA.
 *
 * Related: None.
 *
 * Supported Feature: 11k
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_RRM_ENABLE CFG_INI_BOOL("gRrmEnable", \
				    1, \
				    "Enable/Disable RRM")

/*
 * <ini>
 * sap_rrm_enable - Enable/Disable RRM on SAP
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to control the capabilities (11 k) included
 * in the capabilities field for SAP.
 *
 * Related: None.
 *
 * Supported Feature: 11k
 *
 * Usage: Internal
 *
 * </ini>
 */
#define CFG_SAP_RRM_ENABLE CFG_INI_BOOL("sap_rrm_enable", \
					0, \
					"Enable/Disable RRM on SAP")

/*
 * <ini>
 * gRrmRandnIntvl - Randomization interval
 * @Min: 10
 * @Max: 100
 * @Default: 100
 *
 * This ini is used to set randomization interval which is used to start a timer
 * of a random value within randomization interval. Next RRM Scan request
 * will be issued after the expiry of this random interval.
 *
 * Related: None.
 *
 * Supported Feature: 11k
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_RRM_MEAS_RAND_INTVL CFG_INI_UINT("gRrmRandnIntvl", \
					     10, \
					     100, \
					     100, \
					     CFG_VALUE_OR_DEFAULT, \
					     "RRM Randomization interval")

/*
 * <ini>
 * rm_capability - Configure RM enabled capabilities IE
 * @Default: 0x73,0x10,0x91,0x00,0x04
 *
 * This ini is used to configure RM enabled capabilities IE.
 * Using this INI, we can set/unset any of the bits in 5 bytes
 * (last 4bytes are reserved). Bit details are updated as per
 * Draft version of 11mc spec. (Draft P802.11REVmc_D4.2)
 *
 * Bitwise details are defined as bit mask in rrm_global.h
 * Comma is used as a separator for each byte.
 *
 * Related: None.
 *
 * Supported Feature: 11k
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_RM_CAPABILITY CFG_INI_STRING("rm_capability", \
					 24, \
					 40, \
					 "0x73,0x10,0x91,0x00,0x04", \
					 "RM enabled capabilities IE")

#define CFG_FE_RRM_ALL \
	CFG(CFG_RRM_ENABLE) \
	CFG(CFG_SAP_RRM_ENABLE) \
	CFG(CFG_RRM_MEAS_RAND_INTVL) \
	CFG(CFG_RM_CAPABILITY)

#endif /* __CFG_MLME_FE_RRM_H */
