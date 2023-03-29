/*
 * Copyright (c) 2014-2019 The Linux Foundation. All rights reserved.
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

#if !defined(__SMERRMAPI_H)
#define __SMERRMAPI_H

/**
 * \file  sme_rrm_api.h
 *
 * \brief prototype for SME RRM APIs
 */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "qdf_lock.h"
#include "qdf_trace.h"
#include "qdf_mem.h"
#include "qdf_types.h"
#include "ani_global.h"
#include "sir_api.h"
#include "sme_internal.h"
#include "sme_rrm_internal.h"

QDF_STATUS sme_rrm_msg_processor(struct mac_context *mac, uint16_t msg_type,
		void *msg_buf);
QDF_STATUS rrm_close(struct mac_context *mac);
QDF_STATUS rrm_open(struct mac_context *mac);
QDF_STATUS sme_rrm_neighbor_report_request(struct mac_context *mac,
		uint8_t sessionId, tpRrmNeighborReq pNeighborReq,
		tpRrmNeighborRspCallbackInfo callbackInfo);
QDF_STATUS sme_rrm_process_beacon_report_req_ind(struct mac_context *mac,
		void *msg_buf);

/**
 * rrm_start() - start the RRM module
 * @mac_ctx: The handle returned by mac_open.
 *
 * Return: QDF_STATUS
 *           QDF_STATUS_SUCCESS  success
 */
QDF_STATUS rrm_start(struct mac_context *mac_ctx);

/**
 * rrm_stop() - stop the RRM module
 * @mac_ctx: The handle returned by mac_open.
 *
 * Return: QDF_STATUS
 *           QDF_STATUS_SUCCESS  success
 */
QDF_STATUS rrm_stop(struct mac_context *mac_ctx);

#endif
