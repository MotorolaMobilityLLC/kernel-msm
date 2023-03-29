/*
 * Copyright (c) 2012 - 2019 The Linux Foundation. All rights reserved.
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

#ifndef __CFG_FWOL_H
#define __CFG_FWOL_H

#include "cfg_define.h"
#include "cfg_converged.h"
#include "qdf_types.h"
#include "cfg_coex.h"
#include "cfg_thermal_temp.h"
#include "cfg_ie_whitelist.h"
#include "cfg_fwol_generic.h"
#include "cfg_neighbor_roam.h"
#include "cfg_adaptive_dwelltime.h"

#ifdef WLAN_FW_OFFLOAD
#define CFG_FWOL_ALL \
	CFG_ADAPTIVE_DWELLTIME_ALL \
	CFG_11K_ALL \
	CFG_COEX_ALL \
	CFG_FWOL_GENERIC_ALL \
	CFG_IE_WHITELIST \
	CFG_THERMAL_TEMP_ALL
#else
#define CFG_FWOL_ALL
#endif

#endif /* __CFG_FWOL_H */

