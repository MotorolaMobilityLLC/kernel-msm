/*
 * Copyright (c) 2011-2018 The Linux Foundation. All rights reserved.
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

#ifndef WLAN_MLME_PRODUCT_DETAILS_H__
#define WLAN_MLME_PRODUCT_DETAILS_H__

/*
 * manufacturer_name - Set manufacture Name
 * @Min_len: 0
 * @Max_len: 63
 * @Default: Qualcomm Atheros
 *
 * This internal CFG is used to set manufacture name
 *
 * Related: None
 *
 * Supported Feature: product details
 *
 * Usage: Internal
 *
 */
#define CFG_MFR_NAME CFG_STRING( \
	"manufacturer_name", \
	0, \
	WLAN_CFG_MFR_NAME_LEN, \
	"Qualcomm Atheros", \
	"Manufacture name")

/*
 * model_number - Set model number
 * @Min_len: 0
 * @Max_len: 31
 * @Default: MN1234
 *
 * This internal CFG is used to set model number
 *
 * Related: None
 *
 * Supported Feature: product details
 *
 * Usage: Internal
 *
 */
#define CFG_MODEL_NUMBER CFG_STRING( \
	"model_number", \
	0, \
	WLAN_CFG_MODEL_NUMBER_LEN, \
	"MN1234", \
	"model number")

/*
 * model_name - Set model name
 * @Min_len: 0
 * @Max_len: 31
 * @Default: WFR4031
 *
 * This internal CFG is used to set model name
 *
 * Related: None
 *
 * Supported Feature: product details
 *
 * Usage: Internal
 *
 */
#define CFG_MODEL_NAME CFG_STRING( \
	"model_name", \
	0, \
	WLAN_CFG_MODEL_NAME_LEN, \
	"WFR4031", \
	"model name")

/*
 * manufacture_product_name - Set manufacture product name
 * @Min_len: 0
 * @Max_len: 31
 * @Default: 11n-AP
 *
 * This internal CFG is used to set manufacture product name
 *
 * Related: None
 *
 * Supported Feature: product details
 *
 * Usage: Internal
 *
 */
#define CFG_MFR_PRODUCT_NAME CFG_STRING( \
	"manufacture_product_name", \
	0, \
	WLAN_CFG_MFR_PRODUCT_NAME_LEN, \
	"11n-AP", \
	"manufacture product name")

/*
 * model_number - Set manufacture product version
 * @Min_len: 0
 * @Max_len: 31
 * @Default: SN1234
 *
 * This internal CFG is used to set manufacture product version
 *
 * Related: None
 *
 * Supported Feature: product details
 *
 * Usage: Internal
 *
 */
#define CFG_MFR_PRODUCT_VERSION CFG_STRING( \
	"manufacture_product_version", \
	0, \
	WLAN_CFG_MFR_PRODUCT_VERSION_LEN, \
	"SN1234", \
	"manufacture product version")

#define CFG_MLME_PRODUCT_DETAILS_ALL \
		CFG(CFG_MFR_NAME) \
		CFG(CFG_MODEL_NUMBER) \
		CFG(CFG_MODEL_NAME) \
		CFG(CFG_MFR_PRODUCT_NAME) \
		CFG(CFG_MFR_PRODUCT_VERSION)

#endif

