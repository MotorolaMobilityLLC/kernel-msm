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

#ifndef __CFG_MLME_DOT11MODE_H
#define __CFG_MLME_DOT11MODE_H

#define CFG_DOT11_MODE CFG_UINT( \
		"dot11_mode", \
		0, \
		10, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"dot 11 mode")

/*
 * <ini>
 * vdev_dot11_mode- Bit mask to set the dot11 mode for different vdev types
 * @Min: 0x0
 * @Max: 0x333333
 * @Default: 0
 *
 * This ini is used to set the dot11mode different vdev types.
 * dot11_mode ini value (CFG_DOT11_MODE) is the master configuration
 * Min configuration of INI dot11_mode and vdev_dot11_mode is used for that
 * vdev type.
 * dot11_mode vdev_dot11_mode dot11_mode_used
 * 11AX        11AC              11AC
 * 11AC        11AX              11AC
 *
 * Dot11 mode value is 4 bit length for each vdev. Below is the bit definition
 * for different vdev types dot11 mode value bit index.
 *
 * Bits used for dot11mode        Vdev Type
 * BIT[3:0]                       STA mode
 * BIT[7:4]                       P2P_CLI/P2P_DEVICE mode
 * BIT[11:8]                      NAN DISCOVERY
 * BIT[15:12]                     OCB
 * BIT[19:16]                     TDLS
 * BIT[23:20]                     NDI mode
 *
 * Dot11 mode value to be set in the above bit definition:
 * 0 - Auto, Uses CFG_DOT11_MODE setting
 * 1 - HT mode(11N)
 * 2 - VHT mode(11AC)
 * 3 - HE mode(11AX)
 *
 * E.g: vdev_dot11_mode=0x013220
 *
 * 0          1        3       2             2        0
 * NDI(auto)  TDLS HT  OCB_HE  VHT NAN_DISC  VHT P2P  STA_AUTO
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_VDEV_DOT11_MODE CFG_INI_UINT( \
		"vdev_dot11_mode", \
		0, \
		0x333333, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"vdev dot 11 mode")

#define CFG_DOT11_MODE_ALL \
	CFG(CFG_DOT11_MODE) \
	CFG(CFG_VDEV_DOT11_MODE) \

#endif /* __CFG_MLME_DOT11MODE_H */

