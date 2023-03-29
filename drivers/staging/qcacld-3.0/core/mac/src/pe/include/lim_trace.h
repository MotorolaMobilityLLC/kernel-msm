/*
 * Copyright (c) 2013-2016, 2019-2020 The Linux Foundation. All rights reserved.
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

 *  \file  lim_trace.h

 *  \brief definition for trace related APIs

 *  \author Sunit Bhatia

   ========================================================================*/

#ifndef __LIM_TRACE_H
#define __LIM_TRACE_H

#include "lim_global.h"
#include "mac_trace.h"
#include "qdf_trace.h"

enum pecodetype {
	TRACE_CODE_MLM_STATE,
	TRACE_CODE_SME_STATE,
	TRACE_CODE_TX_MGMT,
	TRACE_CODE_RX_MGMT,
	TRACE_CODE_RX_MGMT_TSF,
	TRACE_CODE_TX_COMPLETE,
	TRACE_CODE_TX_SME_MSG,
	TRACE_CODE_RX_SME_MSG,
	TRACE_CODE_TX_WMA_MSG,
	TRACE_CODE_RX_WMA_MSG,
	TRACE_CODE_TX_LIM_MSG,
	TRACE_CODE_RX_LIM_MSG,
	TRACE_CODE_RX_MGMT_DROP,

	TRACE_CODE_TIMER_ACTIVATE,
	TRACE_CODE_TIMER_DEACTIVATE,
	TRACE_CODE_INFO_LOG
};

#ifdef LIM_TRACE_RECORD

#define LIM_TRACE_GET_SSN(data)    (((data) >> 16) & 0xff)
#define LIM_TRACE_GET_SUBTYPE(data)    ((data) & 0xff)
#define LIM_TRACE_GET_DEFRD_OR_DROPPED(data) ((data) & 0xc0000000)

#define LIM_MSG_PROCESSED 0
#define LIM_MSG_DEFERRED   1
#define LIM_MSG_DROPPED     2

#define LIM_TRACE_MAKE_RXMGMT(type, ssn) \
	(((ssn) << 16) | (type))
#define LIM_TRACE_MAKE_RXMSG(msg, action) \
	((msg) | ((action) << 30))

void lim_trace_init(struct mac_context *mac);
uint8_t *lim_trace_get_mlm_state_string(uint32_t mlmState);
uint8_t *lim_trace_get_sme_state_string(uint32_t smeState);
void lim_trace_dump(void *mac, tp_qdf_trace_record pRecord,
		    uint16_t recIndex);
void mac_trace_msg_tx(struct mac_context *mac, uint8_t session, uint32_t data);
void mac_trace_msg_rx(struct mac_context *mac, uint8_t session, uint32_t data);

#endif /* endof LIM_TRACE_RECORD MACRO */

#endif
