/*
 * Copyright (c) 2012-2018 The Linux Foundation. All rights reserved.
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

#ifndef __CFG_IE_WHITELIST_H
#define __CFG_IE_WHITELIST_H

/*
 * <ini>
 * g_enable_probereq_whitelist_ies - Enable IE white listing
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used to enable/disable probe request IE white listing feature.
 * Values 0 and 1 are used to disable and enable respectively, by default this
 * feature is disabled.
 *
 * Related: None
 *
 * Supported Feature: Probe request IE whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PROBE_REQ_IE_WHITELIST CFG_INI_BOOL( \
			"g_enable_probereq_whitelist_ies", \
			0, \
			"Enable IE whitelisting")

/*
 * For IE white listing in Probe Req, following ini parameters from
 * g_probe_req_ie_bitmap_0 to g_probe_req_ie_bitmap_7 are used. User needs to
 * input this values in hexa decimal format, when bit is set in bitmap,
 * corresponding IE needs to be included in probe request.
 *
 * Example:
 * ========
 * If IE 221 needs to be in the probe request, set the corresponding bit
 * as follows:
 * a= IE/32 = 221/32 = 6 = g_probe_req_ie_bitmap_6
 * b = IE modulo 32 = 29,
 * means set the bth bit in g_probe_req_ie_bitmap_a,
 * therefore set 29th bit in g_probe_req_ie_bitmap_6,
 * as a result, g_probe_req_ie_bitmap_6=20000000
 *
 * Note: For IE 221, its mandatory to set the gProbeReqOUIs.
 */

/*
 * <ini>
 * g_probe_req_ie_bitmap_0 - Used to set the bitmap of IEs from 0 to 31
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 0 to 31 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PROBE_REQ_IE_BIT_MAP0 CFG_INI_UINT( \
			"g_probe_req_ie_bitmap_0", \
			0x00000000, \
			0xFFFFFFFF, \
			0x00000000, \
			CFG_VALUE_OR_DEFAULT, \
			"IE Bitmap 0")

/*
 * <ini>
 * g_probe_req_ie_bitmap_1 - Used to set the bitmap of IEs from 32 to 63
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 32 to 63 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PROBE_REQ_IE_BIT_MAP1 CFG_INI_UINT( \
			"g_probe_req_ie_bitmap_1", \
			0x00000000, \
			0xFFFFFFFF, \
			0x00000000, \
			CFG_VALUE_OR_DEFAULT, \
			"IE Bitmap 1")

/*
 * <ini>
 * g_probe_req_ie_bitmap_2 - Used to set the bitmap of IEs from 64 to 95
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 64 to 95 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PROBE_REQ_IE_BIT_MAP2 CFG_INI_UINT( \
			"g_probe_req_ie_bitmap_2", \
			0x00000000, \
			0xFFFFFFFF, \
			0x00000000, \
			CFG_VALUE_OR_DEFAULT, \
			"IE Bitmap 2")

/*
 * <ini>
 * g_probe_req_ie_bitmap_3 - Used to set the bitmap of IEs from 96 to 127
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 96 to 127 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PROBE_REQ_IE_BIT_MAP3 CFG_INI_UINT( \
			"g_probe_req_ie_bitmap_3", \
			0x00000000, \
			0xFFFFFFFF, \
			0x00000000, \
			CFG_VALUE_OR_DEFAULT, \
			"IE Bitmap 3")

/*
 * <ini>
 * g_probe_req_ie_bitmap_4 - Used to set the bitmap of IEs from 128 to 159
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 128 to 159 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PROBE_REQ_IE_BIT_MAP4 CFG_INI_UINT( \
			"g_probe_req_ie_bitmap_4", \
			0x00000000, \
			0xFFFFFFFF, \
			0x00000000, \
			CFG_VALUE_OR_DEFAULT, \
			"IE Bitmap 4")

/*
 * <ini>
 * g_probe_req_ie_bitmap_5 - Used to set the bitmap of IEs from 160 to 191
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 160 to 191 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PROBE_REQ_IE_BIT_MAP5 CFG_INI_UINT( \
			"g_probe_req_ie_bitmap_5", \
			0x00000000, \
			0xFFFFFFFF, \
			0x00000000, \
			CFG_VALUE_OR_DEFAULT, \
			"IE Bitmap 5")

/*
 * <ini>
 * g_probe_req_ie_bitmap_6 - Used to set the bitmap of IEs from 192 to 223
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 192 to 223 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PROBE_REQ_IE_BIT_MAP6 CFG_INI_UINT( \
			"g_probe_req_ie_bitmap_6", \
			0x00000000, \
			0xFFFFFFFF, \
			0x00000000, \
			CFG_VALUE_OR_DEFAULT, \
			"IE Bitmap 6")

/*
 * <ini>
 * g_probe_req_ie_bitmap_7 - Used to set the bitmap of IEs from 224 to 255
 * @Min: 0x00000000
 * @Max: 0xFFFFFFFF
 * @Default: 0x00000000
 *
 * This ini is used to include the IEs from 224 to 255 in probe request,
 * when corresponding bit is set.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PROBE_REQ_IE_BIT_MAP7 CFG_INI_UINT( \
			"g_probe_req_ie_bitmap_7", \
			0x00000000, \
			0xFFFFFFFF, \
			0x00000000, \
			CFG_VALUE_OR_DEFAULT, \
			"IE Bitmap 7")

#define MAX_PRB_REQ_VENDOR_OUI_INI_LEN 160
#define VENDOR_SPECIFIC_IE_BITMAP 0x20000000
/*
 * For vendor specific IE, Probe Req OUI types and sub types which are
 * to be white listed are specified in gProbeReqOUIs in the following
 * example format - gProbeReqOUIs=AABBCCDD EEFF1122
 */

/*
 * <ini>
 * gProbeReqOUIs - Used to specify vendor specific OUIs
 * @Default: Empty string
 *
 * This ini is used to include the specified OUIs in vendor specific IE
 * of probe request.
 *
 * Related: Need to enable g_enable_probereq_whitelist_ies and
 * vendor specific IE should be set in g_probe_req_ie_bitmap_6.
 *
 * Supported Feature: Probe request ie whitelisting
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_PROBE_REQ_OUI CFG_INI_STRING( \
			"gProbeReqOUIs", \
			0, \
			MAX_PRB_REQ_VENDOR_OUI_INI_LEN, \
			"", \
			"Probe Req OUIs")

#define CFG_IE_WHITELIST \
	CFG(CFG_PROBE_REQ_IE_WHITELIST) \
	CFG(CFG_PROBE_REQ_IE_BIT_MAP0) \
	CFG(CFG_PROBE_REQ_IE_BIT_MAP1) \
	CFG(CFG_PROBE_REQ_IE_BIT_MAP2) \
	CFG(CFG_PROBE_REQ_IE_BIT_MAP3) \
	CFG(CFG_PROBE_REQ_IE_BIT_MAP4) \
	CFG(CFG_PROBE_REQ_IE_BIT_MAP5) \
	CFG(CFG_PROBE_REQ_IE_BIT_MAP6) \
	CFG(CFG_PROBE_REQ_IE_BIT_MAP7) \
	CFG(CFG_PROBE_REQ_OUI)

#endif
