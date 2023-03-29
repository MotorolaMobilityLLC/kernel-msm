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

/**
 * DOC: This file contains centralized definitions of WEP parameters related
 * converged configuration.
 */

#ifndef __CFG_MLME_WEP_PARAMS_H
#define __CFG_MLME_WEP_PARAMS_H

#define CFG_WEP_DEFAULT_KEYID CFG_UINT( \
			"wep_default_key_id", \
			0, \
			3, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"wep default key id")

#define CFG_SHARED_KEY_AUTH_ENABLE CFG_BOOL( \
			"shared_key_auth", \
			1, \
			"shared key authentication")

#define CFG_OPEN_SYSTEM_AUTH_ENABLE CFG_BOOL( \
			"open_system_auth", \
			1, \
			"Open system authentication")

#define CFG_AUTHENTICATION_TYPE CFG_UINT( \
			"auth_type", \
			0, \
			3, \
			0, \
			CFG_VALUE_OR_DEFAULT, \
			"authentication type")

#define CFG_PRIVACY_ENABLED CFG_BOOL( \
			"privacy_enabled", \
			0, \
			"wep privacy")

#define CFG_WEP_PARAMS_ALL \
	CFG(CFG_WEP_DEFAULT_KEYID) \
	CFG(CFG_SHARED_KEY_AUTH_ENABLE) \
	CFG(CFG_OPEN_SYSTEM_AUTH_ENABLE) \
	CFG(CFG_AUTHENTICATION_TYPE) \
	CFG(CFG_PRIVACY_ENABLED)

#endif /* __CFG_MLME_WEP_PARAMS_H */
