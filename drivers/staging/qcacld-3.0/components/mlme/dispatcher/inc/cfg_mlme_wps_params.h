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

#ifndef __CFG_MLME_WPS_PARAMS_H
#define __CFG_MLME_WPS_PARAMS_H

#define CFG_WPS_ENABLE CFG_UINT( \
		"enable_wps", \
		0, \
		255, \
		0, \
		CFG_VALUE_OR_DEFAULT, \
		"enable wps")

#define CFG_WPS_STATE CFG_UINT( \
		"wps_state", \
		0, \
		255, \
		1, \
		CFG_VALUE_OR_DEFAULT, \
		"wps_state")

#define CFG_WPS_VERSION CFG_UINT( \
		"wps_version", \
		0, \
		255, \
		16, \
		CFG_VALUE_OR_DEFAULT, \
		"wps version")

#define CFG_WPS_CFG_METHOD CFG_UINT( \
			"wps_cfg_method", \
			0, \
			4294967295, \
			8, \
			CFG_VALUE_OR_DEFAULT, \
			"wps cfg method")

#define CFG_WPS_PRIMARY_DEVICE_CATEGORY CFG_UINT( \
			"wps_primary_device_category", \
			0, \
			65535, \
			1, \
			CFG_VALUE_OR_DEFAULT, \
			"wps primary device category")

#define CFG_WPS_PIMARY_DEVICE_OUI CFG_UINT( \
			"wps_primary_device_oui", \
			0, \
			4294967295, \
			5304836, \
			CFG_VALUE_OR_DEFAULT, \
			"wps primary device oui")

#define CFG_WPS_DEVICE_SUB_CATEGORY CFG_UINT( \
				"wps_device_sub_category", \
				0, \
				65535, \
				1, \
				CFG_VALUE_OR_DEFAULT, \
				"wps device sub category")

#define CFG_WPS_DEVICE_PASSWORD_ID CFG_UINT( \
				"wps_device_password_id", \
				0, \
				4294967295, \
				0, \
				CFG_VALUE_OR_DEFAULT, \
				"wps device password id")

#define WPS_UUID_DEF_STR "0xa, 0xb, 0xc, 0xd, 0xe, 0xf"
#define WPS_UUID_DEF_LEN (sizeof(WPS_UUID_DEF_STR) - 1)

#define CFG_WPS_UUID CFG_STRING( \
		"wps_uuid", \
		0, \
		WPS_UUID_DEF_LEN, \
		WPS_UUID_DEF_STR, \
		"wps uuid")

#define CFG_WPS_ALL \
	CFG(CFG_WPS_ENABLE) \
	CFG(CFG_WPS_STATE) \
	CFG(CFG_WPS_VERSION) \
	CFG(CFG_WPS_CFG_METHOD) \
	CFG(CFG_WPS_PRIMARY_DEVICE_CATEGORY) \
	CFG(CFG_WPS_PIMARY_DEVICE_OUI) \
	CFG(CFG_WPS_DEVICE_SUB_CATEGORY) \
	CFG(CFG_WPS_DEVICE_PASSWORD_ID) \
	CFG(CFG_WPS_UUID)

#endif /* __CFG_MLME_WPS_PARAMS_H */

