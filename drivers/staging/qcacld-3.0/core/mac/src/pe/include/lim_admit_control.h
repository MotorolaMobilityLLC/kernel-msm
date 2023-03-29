/*
 * Copyright (c) 2011-2012, 2014-2019 The Linux Foundation. All rights reserved.
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
 * Author:      Dinesh Upadhyay
 * Date:        10/24/06
 * History:-
 * Date            Modified by    Modification Information
 * --------------------------------------------------------------------
 *
 */

#ifndef __LIM_ADMIT_CONTROL_H__
#define __LIM_ADMIT_CONTROL_H__

#include "sir_common.h"
#include "sir_mac_prot_def.h"

#include "ani_global.h"

QDF_STATUS
lim_tspec_find_by_assoc_id(struct mac_context *, uint16_t,
			   struct mac_tspec_ie *,
			   tpLimTspecInfo, tpLimTspecInfo *);

/* Add TSPEC in lim local table */
QDF_STATUS lim_tspec_add(struct mac_context *mac,
			    uint8_t *pAddr,
			    uint16_t assocId,
			    struct mac_tspec_ie *pTspec,
			    uint32_t interval, tpLimTspecInfo *ppInfo);

/* admit control interface */
QDF_STATUS lim_admit_control_add_ts(struct mac_context *mac,
				    uint8_t *pAddr, tSirAddtsReqInfo *addts,
				    tSirMacQosCapabilityStaIE *qos,
				    uint16_t assocId, uint8_t alloc,
				    tSirMacScheduleIE *pSch,
				    /* index to the lim tspec table. */
				    uint8_t *pTspecIdx,
				    struct pe_session *pe_session);

static inline QDF_STATUS
lim_admit_control_add_sta(struct mac_context *mac, uint8_t *staAddr, uint8_t alloc)
{
	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
lim_admit_control_delete_sta(struct mac_context *mac, uint16_t assocId);

QDF_STATUS
lim_admit_control_delete_ts(struct mac_context *mac,
			    uint16_t assocId,
			    struct mac_ts_info *tsinfo,
			    uint8_t *tsStatus, uint8_t *tspecIdx);

QDF_STATUS lim_admit_control_init(struct mac_context *mac);
#ifdef FEATURE_WLAN_ESE
QDF_STATUS lim_send_hal_msg_add_ts(struct mac_context *mac,
				      uint8_t tspecIdx,
				      struct mac_tspec_ie tspecIE,
				      uint8_t sessionId, uint16_t tsm_interval);
#else
QDF_STATUS lim_send_hal_msg_add_ts(struct mac_context *mac,
				      uint8_t tspecIdx,
				      struct mac_tspec_ie tspecIE,
				      uint8_t sessionId);
#endif

QDF_STATUS lim_send_hal_msg_del_ts(struct mac_context *mac,
				      uint8_t tspecIdx,
				      struct delts_req_info delts,
				      uint8_t sessionId, uint8_t *bssId);
void lim_process_hal_add_ts_rsp(struct mac_context *mac,
				struct scheduler_msg *limMsg);

#endif
