/*
 * Copyright (c) 2012-2018,2020 The Linux Foundation. All rights reserved.
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
 * DOC: This file contains configuration definitions for MLME WMC.
 */
#ifndef CFG_MLME_MWC_H_
#define CFG_MLME_MWC_H_

#ifdef MWS_COEX
/*
 * <ini>
 * gMwsCoex4gQuickTdm - Bitmap to control MWS-COEX 4G quick FTDM policy
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * It is a 32 bit value such that the various bits represent as below:
 * Bit-0 : 0 - Don't allow quick FTDM policy (Default)
 *        1 - Allow quick FTDM policy
 * Bit 1-31 : reserved for future use
 *
 * It is used to enable or disable MWS-COEX 4G (LTE) Quick FTDM
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_MWS_COEX_4G_QUICK_FTDM CFG_INI_UINT( \
	"gMwsCoex4gQuickTdm", \
	0x00000000, \
	0xFFFFFFFF, \
	0x00000000, \
	CFG_VALUE_OR_DEFAULT, \
	"set mws-coex 4g quick ftdm policy")

/*
 * <ini>
 * gMwsCoex5gnrPwrLimit - Bitmap to set MWS-COEX 5G-NR power limit
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * It is a 32 bit value such that the various bits represent as below:
 * Bit-0 : Don't apply user specific power limit,
 *        use internal power limit (Default)
 * Bit 1-2 : Invalid value (Ignored)
 * Bit 3-21 : Apply the specified value as the external power limit, in dBm
 * Bit 22-31 : Invalid value (Ignored)
 *
 * It is used to set MWS-COEX 5G-NR power limit
 *
 * Usage: Internal
 *
 * </ini>
 */

#define CFG_MWS_COEX_5G_NR_PWR_LIMIT CFG_INI_UINT( \
	"gMwsCoex5gnrPwrLimit", \
	0x00000000, \
	0xFFFFFFFF, \
	0x00000000, \
	CFG_VALUE_OR_DEFAULT, \
	"set mws-coex 5g-nr power limit")

/*
 * <ini>
 * mws_coex_pcc_channel_avoid_delay - configures the duration, when WWAN PCC
 * (Primary Component Carrier) conflicts with WLAN channel.
 * @Min: 0x00
 * @Max: 0xFF
 * @Default: 0x3C
 *
 * It is used to set MWS-COEX WWAN PCC channel avoidance delay
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MWS_COEX_PCC_CHANNEL_AVOID_DELAY CFG_INI_UINT(\
		"mws_coex_pcc_channel_avoid_delay", \
		0x00000000, \
		0xFFFFFFFF, \
		0x3C, \
		CFG_VALUE_OR_DEFAULT, \
		"set mws-coex PCC channel avoidance delay")

/*
 * <ini>
 * mws_coex_scc_channel_avoid_delay - configures the duration, when WWAN SCC
 * (Secondary Component Carrier) conflicts with WLAN channel.
 * @Min: 0x00
 * @Max: 0xFF
 * @Default: 0x78
 *
 * It is used to set MWS-COEX WWAN SCC channel avoidance delay
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_MWS_COEX_SCC_CHANNEL_AVOID_DELAY CFG_INI_UINT(\
		"mws_coex_scc_channel_avoid_delay", \
		0x00000000, \
		0xFFFFFFFF, \
		0x78, \
		CFG_VALUE_OR_DEFAULT, \
		"set mws-coex SCC channel avoidance delay")

#define CFG_MWC_ALL \
	CFG(CFG_MWS_COEX_4G_QUICK_FTDM) \
	CFG(CFG_MWS_COEX_5G_NR_PWR_LIMIT) \
	CFG(CFG_MWS_COEX_PCC_CHANNEL_AVOID_DELAY) \
	CFG(CFG_MWS_COEX_SCC_CHANNEL_AVOID_DELAY)

#else
#define CFG_MWC_ALL
#endif /* MWS_COEX */

#endif /* CFG_MLME_MWC_H_ */
