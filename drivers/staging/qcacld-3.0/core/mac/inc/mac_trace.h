/*
 * Copyright (c) 2013-2016, 2018-2020 The Linux Foundation. All rights reserved.
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

/**=========================================================================

 *  \file  mac_trace.h

 *  \brief definition for trace related APIs

   \author Sunit Bhatia

   ========================================================================*/

#ifndef __MAC_TRACE_H
#define __MAC_TRACE_H

#include "ani_global.h"
#include "qdf_trace.h"

#define MAC_TRACE_GET_MODULE_ID(data) ((data >> 8) & 0xff)
#define MAC_TRACE_GET_MSG_ID(data)       (data & 0xffff)

/**
 * mac_trace() - Main function used for MAC Trace
 * @mac_ctx:       Global MAC context
 * @code:          trace code
 * @session:       session id
 * @data:          data to be traced.
 *
 * Return: None
 */
static inline void mac_trace(struct mac_context *mac_ctx, uint16_t code,
			     uint16_t session, uint32_t data)
{
	qdf_trace(QDF_MODULE_ID_PE, code, session, data);
}

#ifdef TRACE_RECORD

#define eLOG_NODROP_MISSED_BEACON_SCENARIO 0
#define eLOG_PROC_DEAUTH_FRAME_SCENARIO 1

uint8_t *mac_trace_get_lim_msg_string(uint16_t limMsg);
uint8_t *mac_trace_get_sme_msg_string(uint16_t smeMsg);
uint8_t *mac_trace_get_info_log_string(uint16_t infoLog);

#endif
uint8_t *mac_trace_get_wma_msg_string(uint16_t wmaMsg);
uint8_t *mac_trace_get_neighbour_roam_state(uint16_t neighbourRoamState);
uint8_t *mac_trace_getcsr_roam_state(uint16_t csr_roamState);
uint8_t *mac_trace_getcsr_roam_sub_state(uint16_t csr_roamSubState);
uint8_t *mac_trace_get_lim_sme_state(uint16_t limState);
uint8_t *mac_trace_get_lim_mlm_state(uint16_t mlmState);

#endif
