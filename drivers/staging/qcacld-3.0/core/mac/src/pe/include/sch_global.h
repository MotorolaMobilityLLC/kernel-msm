/*
 * Copyright (c) 2013-2014, 2017-2019 The Linux Foundation. All rights reserved.
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
 *
 * Author:      Sandesh Goel
 * Date:        02/25/02
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __SCH_GLOBAL_H__
#define __SCH_GLOBAL_H__

#include "sir_mac_prop_exts.h"
#include "lim_global.h"

#include "parser_api.h"

#define TIM_IE_SIZE 0xB

/* ----------------------- Beacon processing ------------------------ */

/* / Beacon structure */
#define tSchBeaconStruct tSirProbeRespBeacon
#define tpSchBeaconStruct struct sSirProbeRespBeacon *

/**
 * struct sch_context - SCH global context
 * @beacon_interval: global beacon interval
 * @beacon_changed: flag to indicate that beacon template has been updated
 * @p2p_ie_offset: P2P IE offset
 * @csa_count_offset: CSA Switch Count Offset to be sent to FW
 * @ecsa_count_offset: ECSA Switch Count Offset to be sent to FW
 */
struct sch_context {
	uint16_t beacon_interval;
	uint8_t beacon_changed;
	uint16_t p2p_ie_offset;
	uint32_t csa_count_offset;
	uint32_t ecsa_count_offset;
};

#endif
