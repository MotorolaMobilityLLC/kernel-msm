/*
 * Copyright (c) 2011-2019 The Linux Foundation. All rights reserved.
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

#ifndef __CFG_MLME_EDCA__PARAM_H
#define __CFG_MLME_EDCA__PARAM_H

#define STR_EDCA_ANI_ACBK_LOCAL "0x0, 0x7, 0x0, 0xf, 0x3, 0xff, 0x0, 0x0, 0x1f, 0x3, 0xff, 0x0, 0x0, 0xf, 0x3, 0xff, 0x0"
#define STR_EDCA_ANI_ACBK_LOCAL_LEN (sizeof(STR_EDCA_ANI_ACBK_LOCAL) - 1)

#define CFG_EDCA_ANI_ACBK_LOCAL CFG_INI_STRING( \
		"edca_ani_acbk_local", \
		0, \
		STR_EDCA_ANI_ACBK_LOCAL_LEN, \
		STR_EDCA_ANI_ACBK_LOCAL, \
		"EDCA ANI ACBK LOCAL")

#define STR_EDCA_ANI_ACBE_LOCAL "0x0, 0x2, 0x0, 0xf, 0x3, 0xff, 0x64, 0x0, 0x1f, 0x3, 0xff, 0x64, 0x0, 0xf, 0x3, 0xff, 0x64"
#define STR_EDCA_ANI_ACBE_LOCAL_LEN (sizeof(STR_EDCA_ANI_ACBE_LOCAL) - 1)

#define CFG_EDCA_ANI_ACBE_LOCAL CFG_INI_STRING( \
		"edca_ani_acbe_local", \
		0, \
		STR_EDCA_ANI_ACBE_LOCAL_LEN, \
		STR_EDCA_ANI_ACBE_LOCAL, \
		"EDCA ANI ACBE LOCAL")

#define STR_EDCA_ANI_ACVI_LOCAL "0x0, 0x2, 0x0, 0x7, 0x0, 0xf, 0xc8, 0x0, 0xf, 0x0, 0x1f, 0xbc, 0x0, 0x7, 0x0, 0xf, 0xc8"
#define STR_EDCA_ANI_ACVI_LOCAL_LEN (sizeof(STR_EDCA_ANI_ACVI_LOCAL) - 1)

#define CFG_EDCA_ANI_ACVI_LOCAL CFG_INI_STRING( \
		"edca_ani_acvi_local",\
		0, \
		STR_EDCA_ANI_ACVI_LOCAL_LEN, \
		STR_EDCA_ANI_ACVI_LOCAL, \
		"EDCA ANI ACVI LOCAL")

#define STR_EDCA_ANI_ACVO_LOCAL "0x0, 0x2, 0x0, 0x3, 0x0, 0x7, 0x64, 0x0, 0x7, 0x0, 0xf, 0x66, 0x0, 0x3, 0x0, 0x7, 0x64"
#define STR_EDCA_ANI_ACVO_LOCAL_LEN (sizeof(STR_EDCA_ANI_ACVO_LOCAL) - 1)

#define CFG_EDCA_ANI_ACVO_LOCAL CFG_INI_STRING( \
		"edca_ani_acvo_local", \
		0, \
		STR_EDCA_ANI_ACVO_LOCAL_LEN, \
		STR_EDCA_ANI_ACVO_LOCAL, \
		"EDCA ANI ACVO LOCAL")

#define STR_EDCA_ANI_ACBK "0x0, 0x7, 0x0, 0xf, 0x3, 0xff, 0x0, 0x0, 0x1f, 0x3, 0xff, 0x0, 0x0, 0xf, 0x3, 0xff, 0x0"
#define STR_EDCA_ANI_ACBK_LEN (sizeof(STR_EDCA_ANI_ACBK) - 1)

#define CFG_EDCA_ANI_ACBK CFG_INI_STRING( \
		"edca_ani_acbk", \
		0, \
		STR_EDCA_ANI_ACBK_LEN, \
		STR_EDCA_ANI_ACBK, \
		"EDCA ANI ACBK BROADCAST")

#define STR_EDCA_ANI_ACBE "0x0, 0x2, 0x0, 0xf, 0x3, 0xff, 0x64, 0x0, 0x1f, 0x3, 0xff, 0x64, 0x0, 0xf, 0x3, 0xff, 0x64"
#define STR_EDCA_ANI_ACBE_LEN (sizeof(STR_EDCA_ANI_ACBE) - 1)

#define CFG_EDCA_ANI_ACBE CFG_INI_STRING( \
		"edca_ani_acbe", \
		0, \
		STR_EDCA_ANI_ACBE_LEN, \
		STR_EDCA_ANI_ACBE, \
		"EDCA ANI ACBE BROADCAST")

#define STR_EDCA_ANI_ACVI "0x0, 0x2, 0x0, 0x7, 0x0, 0xf, 0xc8, 0x0, 0xf, 0x0, 0x1f, 0xbc, 0x0, 0x7, 0x0, 0xf, 0xc8"
#define STR_EDCA_ANI_ACVI_LEN (sizeof(STR_EDCA_ANI_ACVI) - 1)

#define CFG_EDCA_ANI_ACVI CFG_INI_STRING( \
		"edca_ani_acvi", \
		0, \
		STR_EDCA_ANI_ACVI_LEN, \
		STR_EDCA_ANI_ACVI, \
		"EDCA ANI ACVI BROADCAST")

#define STR_EDCA_ANI_ACVO "0x0, 0x2, 0x0, 0x3, 0x0, 0x7, 0x64, 0x0, 0x7, 0x0, 0xf, 0x66, 0x0, 0x3, 0x0, 0x7, 0x64"
#define STR_EDCA_ANI_ACVO_LEN (sizeof(STR_EDCA_ANI_ACVO) - 1)

#define CFG_EDCA_ANI_ACVO CFG_INI_STRING( \
		"edca_ani_acvo", \
		0, \
		STR_EDCA_ANI_ACVO_LEN, \
		STR_EDCA_ANI_ACVO, \
		"EDCA ANI ACVO BROADCAST")

#define STR_EDCA_WME_ACBK_LOCAL "0x0, 0x7, 0x0, 0xf, 0x3, 0xff, 0x0, 0x0, 0x1f, 0x3, 0xff, 0x0, 0x0, 0xf, 0x3, 0xff, 0x0"
#define STR_EDCA_WME_ACBK_LOCAL_LEN (sizeof(STR_EDCA_WME_ACBK_LOCAL) - 1)

/*
 * <ini>
 * edca_wme_acbk_local - Set EDCA parameters for WME local AC BK
 * @Default: 0x0,0x7,0x0,0xf,0x3,0xff,0x0,0x0,0x1f,0x3,0xff,0x0,0x0,0xf,0x3,
 *	     0xff,0x0
 *
 * This ini is used to set EDCA parameters for WME AC BK that are used locally
 * on AP. The ini is with 17 bytes and comma is used as a separator for each
 * byte. Index of each byte is defined in wlan_mlme_public_struct.h, such as
 * CFG_EDCA_PROFILE_ACM_IDX.
 *
 * For cwmin and cwmax, they each occupy two bytes with the index defined
 * above. The actual value are counted as number of bits with 1, e.g.
 * "0x0,0x3f" means a value of 6. And final cwmin and cwmax will be converted
 * to 2^value - 1.
 *
 * Related: None
 *
 * Supported Feature: AP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EDCA_WME_ACBK_LOCAL CFG_INI_STRING( \
		"edca_wme_acbk_local", \
		0, \
		STR_EDCA_WME_ACBK_LOCAL_LEN, \
		STR_EDCA_WME_ACBK_LOCAL, \
		"EDCA WME ACBK LOCAL")

#define STR_EDCA_WME_ACBE_LOCAL "0x0, 0x3, 0x0, 0xf, 0x0, 0x3f, 0x0, 0x0, 0x1f, 0x3, 0xff, 0x0, 0x0, 0xf, 0x0, 0x3f, 0x0"
#define STR_EDCA_WME_ACBE_LOCAL_LEN (sizeof(STR_EDCA_WME_ACBE_LOCAL) - 1)

/*
 * <ini>
 * edca_wme_acbe_local - Set EDCA parameters for WME local AC BE
 * @Default: 0x0,0x3,0x0,0xf,0x0,0x3f,0x0,0x0,0x1f,0x3,0xff,0x0,0x0,0xf,0x0,
 *	     0x3f,0x0
 *
 * This ini is used to set EDCA parameters for WME AC BE that are used locally
 * on AP. The ini is with 17 bytes and comma is used as a separator for each
 * byte. Index of each byte is defined in wlan_mlme_public_struct.h, such as
 * CFG_EDCA_PROFILE_ACM_IDX.
 *
 * For cwmin and cwmax, they each occupy two bytes with the index defined
 * above. The actual value are counted as number of bits with 1, e.g.
 * "0x0,0x3f" means a value of 6. And final cwmin and cwmax will be converted
 * to 2^value - 1.
 *
 * Related: None
 *
 * Supported Feature: AP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EDCA_WME_ACBE_LOCAL CFG_INI_STRING( \
		"edca_wme_acbe_local", \
		0, \
		STR_EDCA_WME_ACBE_LOCAL_LEN, \
		STR_EDCA_WME_ACBE_LOCAL, \
		"EDCA WME ACBE LOCAL")

#define STR_EDCA_WME_ACVI_LOCAL "0x0, 0x1, 0x0, 0x7, 0x0, 0xf, 0x5e, 0x0, 0x7, 0x0, 0xf, 0xbc, 0x0, 0x7, 0x0, 0xf, 0x5e"
#define STR_EDCA_WME_ACVI_LOCAL_LEN (sizeof(STR_EDCA_WME_ACVI_LOCAL) - 1)

/*
 * <ini>
 * edca_wme_acvi_local - Set EDCA parameters for WME AC VI
 * @Default: 0x0,0x1,0x0,0x7,0x0,0xf,0x5e,0x0,0x7,0x0,0xf,0xbc,0x0,0x7,0x0,0xf,
 *	     0x5e
 *
 * This ini is used to set EDCA parameters for WME AC VI that are used locally
 * on AP. The ini is with 17 bytes and comma is used as a separator for each
 * byte. Index of each byte is defined in wlan_mlme_public_struct.h, such as
 * CFG_EDCA_PROFILE_ACM_IDX.
 *
 * For cwmin and cwmax, they each occupy two bytes with the index defined
 * above. The actual value are counted as number of bits with 1, e.g.
 * "0x0,0x3f" means a value of 6. And final cwmin and cwmax will be converted
 * to 2^value - 1.
 *
 * Related: None
 *
 * Supported Feature: AP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EDCA_WME_ACVI_LOCAL CFG_INI_STRING( \
		"edca_wme_acvi_local", \
		0, \
		STR_EDCA_WME_ACVI_LOCAL_LEN, \
		STR_EDCA_WME_ACVI_LOCAL, \
		"EDCA WME ACVI LOCAL")

#define STR_EDCA_WME_ACVO_LOCAL "0x0, 0x1, 0x0, 0x3, 0x0, 0x7, 0x2f, 0x0, 0x3, 0x0, 0x7, 0x66, 0x0, 0x3, 0x0, 0x7, 0x2f"
#define STR_EDCA_WME_ACVO_LOCAL_LEN (sizeof(STR_EDCA_WME_ACVO_LOCAL) - 1)

/*
 * <ini>
 * edca_wme_acvo_local - Set EDCA parameters for WME AC VO
 * @Default: 0x0,0x1,0x0,0x3,0x0,0x7,0x2f,0x0,0x3,0x0,0x7,0x66,0x0,0x3,0x0,0x7,
 *	     0x2f
 *
 * This ini is used to set EDCA parameters for WME AC VO that are used locally
 * on AP. The ini is with 17 bytes and comma is used as a separator for each
 * byte. Index of each byte is defined in wlan_mlme_public_struct.h, such as
 * CFG_EDCA_PROFILE_ACM_IDX.
 *
 * For cwmin and cwmax, they each occupy two bytes with the index defined
 * above. The actual value are counted as number of bits with 1, e.g.
 * "0x0,0x3f" means a value of 6. And final cwmin and cwmax will be converted
 * to 2^value - 1.
 *
 * Related: None
 *
 * Supported Feature: AP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EDCA_WME_ACVO_LOCAL CFG_INI_STRING( \
		"edca_wme_acvo_local", \
		0, \
		STR_EDCA_WME_ACVO_LOCAL_LEN, \
		STR_EDCA_WME_ACVO_LOCAL, \
		"EDCA WME ACVO LOCAL")

#define STR_EDCA_WME_ACBK "0x0, 0x7, 0x0, 0xf, 0x3, 0xff, 0x0, 0x0, 0xf, 0x3, 0xff, 0x0, 0x0, 0xf, 0x3, 0xff, 0x0"
#define STR_EDCA_WME_ACBK_LEN (sizeof(STR_EDCA_WME_ACBK) - 1)

#define CFG_EDCA_WME_ACBK CFG_INI_STRING( \
		"edca_wme_acbk", \
		0, \
		STR_EDCA_WME_ACBK_LEN, \
		STR_EDCA_WME_ACBK, \
		"EDCA WME ACBK BROADCAST")

#define STR_EDCA_WME_ACBE "0x0, 0x3, 0x0, 0xf, 0x3, 0xff, 0x0, 0x0, 0xf, 0x3, 0xff, 0x0, 0x0, 0xf, 0x3, 0xff, 0x0"
#define STR_EDCA_WME_ACBE_LEN (sizeof(STR_EDCA_WME_ACBE) - 1)

#define CFG_EDCA_WME_ACBE CFG_INI_STRING( \
		"edca_wme_acbe", \
		0, \
		STR_EDCA_WME_ACBE_LEN, \
		STR_EDCA_WME_ACBE, \
		"EDCA WME ACBE BROADCAST")

#define STR_EDCA_WME_ACVI "0x0, 0x2, 0x0, 0x7, 0x0, 0xf, 0x5e, 0x0, 0x7, 0x0, 0xf, 0xbc, 0x0, 0x7, 0x0, 0xf, 0x5e"
#define STR_EDCA_WME_ACVI_LEN (sizeof(STR_EDCA_WME_ACVI) - 1)

#define CFG_EDCA_WME_ACVI CFG_INI_STRING( \
		"edca_wme_acvi", \
		0, \
		STR_EDCA_WME_ACVI_LEN, \
		STR_EDCA_WME_ACVI, \
		"EDCA WME ACVI BROADCAST")

#define STR_EDCA_WME_ACVO "0x0, 0x2, 0x0, 0x3, 0x0, 0x7, 0x2f, 0x0, 0x3, 0x0, 0x7, 0x66, 0x0, 0x3, 0x0, 0x7, 0x2f"
#define STR_EDCA_WME_ACVO_LEN (sizeof(STR_EDCA_WME_ACVO) - 1)

#define CFG_EDCA_WME_ACVO CFG_INI_STRING( \
		"edca_wme_acvo", \
		0, \
		STR_EDCA_WME_ACVO_LEN, \
		STR_EDCA_WME_ACVO, \
		"EDCA WME ACVO BROADCAST")

#define STR_EDCA_ETSI_ACBK_LOCAL "0x0, 0x7, 0x0, 0xf, 0x3, 0xff, 0xbb, 0x0, 0x1f, 0x3, 0xff, 0x0, 0x0, 0xf, 0x3, 0xff, 0x0"
#define STR_EDCA_ETSI_ACBK_LOCAL_LEN (sizeof(STR_EDCA_ETSI_ACBK_LOCAL) - 1)

/*
 * <ini>
 * edca_etsi_acbk_local - Set EDCA parameters for ETSI local AC BK
 * @Default: 0x0,0x7,0x0,0xf,0x3,0xff,0xbb,0x0,0x1f,0x3,0xff,0x0,0x0,0xf,0x3,
 *	     0xff,0x0
 *
 * This ini is used to set EDCA parameters for ETSI AC BK that are used locally
 * on AP. The ini is with 17 bytes and comma is used as a separator for each
 * byte. Index of each byte is defined in wlan_mlme_public_struct.h, such as
 * CFG_EDCA_PROFILE_ACM_IDX.
 *
 * For cwmin and cwmax, they each occupy two bytes with the index defined
 * above. The actual value are counted as number of bits with 1, e.g.
 * "0x0,0x3f" means a value of 6. And final cwmin and cwmax will be converted
 * to 2^value - 1.
 *
 * Related: None
 *
 * Supported Feature: AP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EDCA_ETSI_ACBK_LOCAL CFG_INI_STRING( \
		"edca_etsi_acbk_local", \
		0, \
		STR_EDCA_ETSI_ACBK_LOCAL_LEN, \
		STR_EDCA_ETSI_ACBK_LOCAL, \
		"EDCA ETSI ACBK LOCAL")

#define STR_EDCA_ETSI_ACBE_LOCAL "0x0, 0x3, 0x0, 0xf, 0x0, 0x3f, 0xbb, 0x0, 0x1f, 0x3, 0xff, 0x0, 0x0, 0xf, 0x0, 0x3f, 0x0"
#define STR_EDCA_ETSI_ACBE_LOCAL_LEN (sizeof(STR_EDCA_ETSI_ACBE_LOCAL) - 1)

/*
 * <ini>
 * edca_etsi_acbe_local - Set EDCA parameters for ETSI local AC BE
 * @Default: 0x0,0x3,0x0,0xf,0x0,0x3f,0xbb,0x0,0x1f,0x3,0xff,0x0,0x0,0xf,0x0,
 *	     0x3f,0x0
 *
 * This ini is used to set EDCA parameters for ETSI AC BE that are used locally
 * on AP. The ini is with 17 bytes and comma is used as a separator for each
 * byte. Index of each byte is defined in wlan_mlme_public_struct.h, such as
 * CFG_EDCA_PROFILE_ACM_IDX.
 *
 * For cwmin and cwmax, they each occupy two bytes with the index defined
 * above. The actual value are counted as number of bits with 1, e.g.
 * "0x0,0x3f" means a value of 6. And final cwmin and cwmax will be converted
 * to 2^value - 1.
 *
 * Related: None
 *
 * Supported Feature: AP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EDCA_ETSI_ACBE_LOCAL CFG_INI_STRING( \
		"edca_etsi_acbe_local", \
		0, \
		STR_EDCA_ETSI_ACBE_LOCAL_LEN, \
		STR_EDCA_ETSI_ACBE_LOCAL, \
		"EDCA ETSI ACBE LOCAL")

#define STR_EDCA_ETSI_ACVI_LOCAL "0x0, 0x1, 0x0, 0x7, 0x0, 0xf, 0x7d, 0x0, 0x7, 0x0, 0xf, 0xbc, 0x0, 0x7, 0x0, 0xf, 0x5e"
#define STR_EDCA_ETSI_ACVI_LOCAL_LEN (sizeof(STR_EDCA_ETSI_ACVI_LOCAL) - 1)

/*
 * <ini>
 * edca_etsi_acvi_local - Set EDCA parameters for ETSI local AC VI
 * @Default: 0x0,0x1,0x0,0x7,0x0,0xf,0x7d,0x0,0x7,0x0,0xf,0xbc,0x0,0x7,0x0,
 *	     0xf,0x5e
 *
 * This ini is used to set EDCA parameters for ETSI AC VI that are used locally
 * on AP. The ini is with 17 bytes and comma is used as a separator for each
 * byte. Index of each byte is defined in wlan_mlme_public_struct.h, such as
 * CFG_EDCA_PROFILE_ACM_IDX.
 *
 * For cwmin and cwmax, they each occupy two bytes with the index defined
 * above. The actual value are counted as number of bits with 1, e.g.
 * "0x0,0x3f" means a value of 6. And final cwmin and cwmax will be converted
 * to 2^value - 1.
 *
 * Related: None
 *
 * Supported Feature: AP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EDCA_ETSI_ACVI_LOCAL CFG_INI_STRING( \
		"edca_etsi_acvi_local", \
		0, \
		STR_EDCA_ETSI_ACVI_LOCAL_LEN, \
		STR_EDCA_ETSI_ACVI_LOCAL, \
		"EDCA ETSI ACVI LOCAL")

#define STR_EDCA_ETSI_ACVO_LOCAL "0x0, 0x1, 0x0, 0x3, 0x0, 0x7, 0x3e, 0x0, 0x3, 0x0, 0x7, 0x66, 0x0, 0x3, 0x0, 0x7, 0x2f"
#define STR_EDCA_ETSI_ACVO_LOCAL_LEN (sizeof(STR_EDCA_ETSI_ACVO_LOCAL) - 1)

/*
 * <ini>
 * edca_etsi_acvo_local - Set EDCA parameters for ETSI local AC VO
 * @Default: 0x0,0x1,0x0,0x3,0x0,0x7,0x3e,0x0,0x3,0x0,0x7,0x66,0x0,0x3,0x0,
 *	     0x7,0x2f
 *
 * This ini is used to set EDCA parameters for ETSI AC VO that are used locally
 * on AP. The ini is with 17 bytes and comma is used as a separator for each
 * byte. Index of each byte is defined in wlan_mlme_public_struct.h, such as
 * CFG_EDCA_PROFILE_ACM_IDX.
 *
 * For cwmin and cwmax, they each occupy two bytes with the index defined
 * above. The actual value are counted as number of bits with 1, e.g.
 * "0x0,0x3f" means a value of 6. And final cwmin and cwmax will be converted
 * to 2^value - 1.
 *
 * Related: None
 *
 * Supported Feature: AP
 *
 * Usage: External
 *
 * </ini>
 */
#define CFG_EDCA_ETSI_ACVO_LOCAL CFG_INI_STRING( \
		"edca_etsi_acvo_local", \
		0, \
		STR_EDCA_ETSI_ACVO_LOCAL_LEN, \
		STR_EDCA_ETSI_ACVO_LOCAL, \
		"EDCA ETSI ACVO LOCAL")

#define STR_EDCA_ETSI_ACBK "0x0, 0x7, 0x0, 0xf, 0x3, 0xff, 0xbb, 0x0, 0xf, 0x3, 0xff, 0x0, 0x0, 0xf, 0x3, 0xff, 0x0"
#define STR_EDCA_ETSI_ACBK_LEN (sizeof(STR_EDCA_ETSI_ACBK) - 1)

#define CFG_EDCA_ETSI_ACBK CFG_INI_STRING( \
		"edca_etsi_acbk", \
		0, \
		STR_EDCA_ETSI_ACBK_LEN, \
		STR_EDCA_ETSI_ACBK, \
		"EDCA ETSI ACBK BROADCAST")

#define STR_EDCA_ETSI_ACBE "0x0, 0x3, 0x0, 0xf, 0x3, 0xff, 0xbb, 0x0, 0xf, 0x3, 0xff, 0x0, 0x0, 0xf, 0x3, 0xff, 0x0"
#define STR_EDCA_ETSI_ACBE_LEN (sizeof(STR_EDCA_ETSI_ACBE) - 1)

#define CFG_EDCA_ETSI_ACBE CFG_INI_STRING( \
		"edca_etsi_acbe", \
		0, \
		STR_EDCA_ETSI_ACBE_LEN, \
		STR_EDCA_ETSI_ACBE, \
		"EDCA ETSI ACBE BROADCAST")

#define STR_EDCA_ETSI_ACVI "0x0, 0x2, 0x0, 0x7, 0x0, 0xf, 0x7d, 0x0, 0x7, 0x0, 0xf, 0xbc, 0x0, 0x7, 0x0, 0xf, 0x5e"
#define STR_EDCA_ETSI_ACVI_LEN (sizeof(STR_EDCA_ETSI_ACVI) - 1)

#define CFG_EDCA_ETSI_ACVI CFG_INI_STRING( \
		"edca_etsi_acvi", \
		0, \
		STR_EDCA_ETSI_ACVI_LEN, \
		STR_EDCA_ETSI_ACVI, \
		"EDCA ETSI ACVI BROADCAST")

#define STR_EDCA_ETSI_ACVO "0x0, 0x2, 0x0, 0x3, 0x0, 0x7, 0x3e, 0x0, 0x3, 0x0, 0x7, 0x66, 0x0, 0x3, 0x0, 0x7, 0x2f"
#define STR_EDCA_ETSI_ACVO_LEN (sizeof(STR_EDCA_ETSI_ACVO) - 1)

#define CFG_EDCA_ETSI_ACVO CFG_INI_STRING( \
		"edca_etsi_acvo", \
		0, \
		STR_EDCA_ETSI_ACVO_LEN, \
		STR_EDCA_ETSI_ACVO, \
		"EDCA ETSI ACVO BROADCAST")

/*
 * <ini>
 * gEnableEdcaParams - Enable edca parameter
 * @Min: 0
 * @Max: 1
 * @Default: 0
 *
 * This ini is used if gEnableEdcaParams is set to 1, params gEdcaVoCwmin,
 * gEdcaViCwmin, gEdcaBkCwmin, gEdcaBeCwmin, gEdcaVoCwmax,
 * gEdcaViCwmax, gEdcaBkCwmax, gEdcaBeCwmax, gEdcaVoAifs,
 * gEdcaViAifs, gEdcaBkAifs and gEdcaBeAifs values are used
 * to overwrite the values received from AP
 *
 * Related: None
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_ENABLE_PARAM CFG_INI_BOOL( \
		"gEnableEdcaParams", \
		0, \
		"Enable edca parameter")

/*
 * <ini>
 * gEdcaVoCwmin - Set Cwmin value for QCA_WLAN_AC_VO
 * @Min: 0
 * @Max: 0x15
 * @Default: 2
 *
 * This ini is used to set default Cwmin value for QCA_WLAN_AC_VO
 * Cwmin value for QCA_WLAN_AC_VO. CWVomin = 2^gEdcaVoCwmin -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin etc
 * are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_VO_CWMIN CFG_INI_UINT( \
		"gEdcaVoCwmin", \
		0x0, \
		15, \
		2, \
		CFG_VALUE_OR_DEFAULT, \
		"Cwmin value for QCA_WLAN_AC_VO")

/*
 * <ini>
 * gEdcaVoCwmax - Set Cwmax value for QCA_WLAN_AC_VO
 * @Min: 0
 * @Max: 15
 * @Default: 3
 *
 * This ini is used to set default Cwmax value for QCA_WLAN_AC_VO
 * Cwmax value for QCA_WLAN_AC_VO. CWVomax = 2^gEdcaVoCwmax -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_VO_CWMAX CFG_INI_UINT( \
		"gEdcaVoCwmax", \
		0x0, \
		15, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"Cwmax value for QCA_WLAN_AC_VO")

/*
 * <ini>
 * gEdcaVoAifs - Set Aifs value for QCA_WLAN_AC_VO
 * @Min: 0
 * @Max: 15
 * @Default: 2
 *
 * This ini is used to set default Aifs value for QCA_WLAN_AC_VO
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_VO_AIFS CFG_INI_UINT( \
		"gEdcaVoAifs", \
		0x0, \
		15, \
		2, \
		CFG_VALUE_OR_DEFAULT, \
		"default Aifs value for QCA_WLAN_AC_VO")

/*
 * <ini>
 * gEdcaViCwmin - Set Cwmin value for QCA_WLAN_AC_VI
 * @Min: 0x0
 * @Max: 15
 * @Default: 3
 *
 * This ini is used to set default value for QCA_WLAN_AC_VI
 * Cwmin value for QCA_WLAN_AC_VI. CWVimin = 2^gEdcaViCwmin -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_VI_CWMIN CFG_INI_UINT( \
		"gEdcaViCwmin", \
		0x0, \
		15, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"Cwmin value for QCA_WLAN_AC_VI")

/*
 * <ini>
 * gEdcaViCwmax - Set Cwmax value for QCA_WLAN_AC_VI
 * @Min: 0
 * @Max: 15
 * @Default: 4
 *
 * This ini is used to set default Cwmax value for QCA_WLAN_AC_VI
 * Cwmax value for QCA_WLAN_AC_VI. CWVimax = 2^gEdcaViCwmax -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_VI_CWMAX CFG_INI_UINT( \
		"gEdcaViCwmax", \
		0x0, \
		15, \
		4, \
		CFG_VALUE_OR_DEFAULT, \
		"cwmax value for QCA_WLAN_AC_VI")

/*
 * <ini>
 * gEdcaViAifs - Set Aifs value for QCA_WLAN_AC_VI
 * @Min: 0
 * @Max: 15
 * @Default: 2
 *
 * This ini is used to set default Aifs value for QCA_WLAN_AC_VI
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_VI_AIFS CFG_INI_UINT( \
		"gEdcaViAifs", \
		0x0, \
		15, \
		2, \
		CFG_VALUE_OR_DEFAULT, \
		"default Aifs value for QCA_WLAN_AC_VI")

/*
 * <ini>
 * gEdcaBkCwmin - Set Cwmin value for QCA_WLAN_AC_BK
 * @Min: 0x0
 * @Max: 15
 * @Default: 4
 *
 * This ini is used to set default Cwmin value for QCA_WLAN_AC_BK
 * Cwmin value for QCA_WLAN_AC_BK. CWBkmin = 2^gEdcaBkCwmin -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 * </ini>
 */
#define CFG_EDCA_BK_CWMIN CFG_INI_UINT( \
		"gEdcaBkCwmin", \
		0x0, \
		15, \
		4, \
		CFG_VALUE_OR_DEFAULT, \
		"Cwmin value for QCA_WLAN_AC_BK")

/*
 * <ini>
 * gEdcaBkCwmax - Set Cwmax value for QCA_WLAN_AC_BK
 * @Min: 0
 * @Max: 15
 * @Default: 10
 *
 * This ini is used to set default Cwmax value for QCA_WLAN_AC_BK
 * Cwmax value for QCA_WLAN_AC_BK. CWBkmax = 2^gEdcaBkCwmax -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_BK_CWMAX CFG_INI_UINT( \
		"gEdcaBkCwmax", \
		0, \
		15, \
		10, \
		CFG_VALUE_OR_DEFAULT, \
		"cwmax value for QCA_WLAN_AC_BK")

/*
 * <ini>
 * gEdcaBkAifs - Set Aifs value for QCA_WLAN_AC_BK
 * @Min: 0
 * @Max: 15
 * @Default: 7
 *
 * This ini is used to set default Aifs value for QCA_WLAN_AC_BK
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_BK_AIFS CFG_INI_UINT( \
		"gEdcaBkAifs", \
		0, \
		15, \
		7, \
		CFG_VALUE_OR_DEFAULT, \
		"default Aifs value for QCA_WLAN_AC_BK")

/*
 * <ini>
 * gEdcaBeCwmin - Set Cwmin value for QCA_WLAN_AC_BE
 * @Min: 0x0
 * @Max: 15
 * @Default: 4
 *
 * This ini is used to set default Cwmin value for QCA_WLAN_AC_BE
 * Cwmin value for QCA_WLAN_AC_BE. CWBemin = 2^gEdcaBeCwmin
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_BE_CWMIN CFG_INI_UINT( \
		"gEdcaBeCwmin", \
		0x0, \
		15, \
		4, \
		CFG_VALUE_OR_DEFAULT, \
		"Cwmin value for QCA_WLAN_AC_BE")

/*
 * <ini>
 * gEdcaBeCwmax - Set Cwmax value for QCA_WLAN_AC_BE
 * @Min: 0
 * @Max: 15
 * @Default: 10
 *
 * This ini is used to set default Cwmax value for QCA_WLAN_AC_BE
 * Cwmax value for QCA_WLAN_AC_BE. CWBemax = 2^gEdcaBeCwmax -1
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */

#define CFG_EDCA_BE_CWMAX CFG_INI_UINT( \
		"gEdcaBeCwmax", \
		0, \
		15, \
		10, \
		CFG_VALUE_OR_DEFAULT, \
		"cwmax value for QCA_WLAN_AC_BE")

/*
 * <ini>
 * gEdcaBeAifs - Set Aifs value for QCA_WLAN_AC_BE
 * @Min: 0
 * @Max: 15
 * @Default: 3
 *
 * This ini is used to set default Aifs value for QCA_WLAN_AC_BE
 *
 * Related: If gEnableEdcaParams is set to 1, params gEdcaVoCwmin
 * etc are aplicable
 *
 * Supported Feature: STA
 *
 * Usage: Internal/External
 *
 * </ini>
 */
#define CFG_EDCA_BE_AIFS CFG_INI_UINT( \
		"gEdcaBeAifs", \
		0, \
		15, \
		3, \
		CFG_VALUE_OR_DEFAULT, \
		"default Aifs value for QCA_WLAN_AC_BE")

#define CFG_EDCA_PARAMS_ALL \
	CFG(CFG_EDCA_ANI_ACBK_LOCAL) \
	CFG(CFG_EDCA_ANI_ACBE_LOCAL) \
	CFG(CFG_EDCA_ANI_ACVI_LOCAL) \
	CFG(CFG_EDCA_ANI_ACVO_LOCAL) \
	CFG(CFG_EDCA_ANI_ACBK) \
	CFG(CFG_EDCA_ANI_ACBE) \
	CFG(CFG_EDCA_ANI_ACVI) \
	CFG(CFG_EDCA_ANI_ACVO) \
	CFG(CFG_EDCA_WME_ACBK_LOCAL) \
	CFG(CFG_EDCA_WME_ACBE_LOCAL) \
	CFG(CFG_EDCA_WME_ACVI_LOCAL) \
	CFG(CFG_EDCA_WME_ACVO_LOCAL) \
	CFG(CFG_EDCA_WME_ACBK) \
	CFG(CFG_EDCA_WME_ACBE) \
	CFG(CFG_EDCA_WME_ACVI) \
	CFG(CFG_EDCA_WME_ACVO) \
	CFG(CFG_EDCA_ETSI_ACBK_LOCAL) \
	CFG(CFG_EDCA_ETSI_ACBE_LOCAL) \
	CFG(CFG_EDCA_ETSI_ACVI_LOCAL) \
	CFG(CFG_EDCA_ETSI_ACVO_LOCAL) \
	CFG(CFG_EDCA_ETSI_ACBK) \
	CFG(CFG_EDCA_ETSI_ACBE) \
	CFG(CFG_EDCA_ETSI_ACVI) \
	CFG(CFG_EDCA_ETSI_ACVO) \
	CFG(CFG_EDCA_ENABLE_PARAM) \
	CFG(CFG_EDCA_VO_CWMIN) \
	CFG(CFG_EDCA_VO_CWMAX) \
	CFG(CFG_EDCA_VO_AIFS) \
	CFG(CFG_EDCA_VI_CWMIN) \
	CFG(CFG_EDCA_VI_CWMAX) \
	CFG(CFG_EDCA_VI_AIFS) \
	CFG(CFG_EDCA_BK_CWMIN) \
	CFG(CFG_EDCA_BK_CWMAX) \
	CFG(CFG_EDCA_BK_AIFS) \
	CFG(CFG_EDCA_BE_CWMIN) \
	CFG(CFG_EDCA_BE_CWMAX) \
	CFG(CFG_EDCA_BE_AIFS)

#endif  /* __CFG_MLME_EDCA__PARAM_H */
