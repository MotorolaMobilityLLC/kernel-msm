/*
 * Copyright (c) 2011-2015, 2017-2019 The Linux Foundation. All rights reserved.
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

/*
 *
 * This file sir_mac_prop_exts.h contains the MAC protocol
 * extensions to support ANI feature set.
 * Author:        Chandra Modumudi
 * Date:          11/27/02
 */
#ifndef __MAC_PROP_EXTS_H
#define __MAC_PROP_EXTS_H

#include "sir_types.h"
#include "sir_api.h"
#include "ani_system_defs.h"

/* / EID (Element ID) definitions */

#define PROP_CAPABILITY_GET(bitname, value) \
	(((value) >> SIR_MAC_PROP_CAPABILITY_ ## bitname) & 1)

#define IS_DOT11_MODE_HT(dot11Mode) \
	(((dot11Mode == MLME_DOT11_MODE_11N) || \
	  (dot11Mode ==  MLME_DOT11_MODE_11N_ONLY) || \
	  (dot11Mode ==  MLME_DOT11_MODE_11AC) || \
	  (dot11Mode ==  MLME_DOT11_MODE_11AC_ONLY) || \
	  (dot11Mode ==  MLME_DOT11_MODE_11AX) || \
	  (dot11Mode ==  MLME_DOT11_MODE_11AX_ONLY) || \
	  (dot11Mode ==  MLME_DOT11_MODE_ALL)) ? true:false)

#define IS_DOT11_MODE_VHT(dot11Mode) \
	(((dot11Mode == MLME_DOT11_MODE_11AC) || \
	  (dot11Mode ==  MLME_DOT11_MODE_11AC_ONLY) || \
	  (dot11Mode ==  MLME_DOT11_MODE_11AX) || \
	  (dot11Mode ==  MLME_DOT11_MODE_11AX_ONLY) || \
	  (dot11Mode ==  MLME_DOT11_MODE_ALL)) ? true:false)

#define IS_DOT11_MODE_HE(dot11Mode) \
	(((dot11Mode == MLME_DOT11_MODE_11AX) || \
	  (dot11Mode ==  MLME_DOT11_MODE_11AX_ONLY) || \
	  (dot11Mode ==  MLME_DOT11_MODE_ALL)) ? true:false)

#define IS_DOT11_MODE_11B(dot11Mode)  \
	((dot11Mode == MLME_DOT11_MODE_11B) ? true:false)

#define IS_BSS_VHT_CAPABLE(vhtCaps) \
	((vhtCaps).present && \
	 ((vhtCaps).rxMCSMap != 0xFFFF) && \
	 ((vhtCaps).txMCSMap != 0xFFFF))

#define WNI_CFG_VHT_CHANNEL_WIDTH_20_40MHZ		0
#define WNI_CFG_VHT_CHANNEL_WIDTH_80MHZ		1
#define WNI_CFG_VHT_CHANNEL_WIDTH_160MHZ		2
#define WNI_CFG_VHT_CHANNEL_WIDTH_80_PLUS_80MHZ	3

#endif /* __MAC_PROP_EXTS_H */
