/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * sys_entry_func.cc - This file has all the system level entry functions
 *                   for all the defined threads at system level.
 * Author:    V. K. Kandarpa
 * Date:      01/16/2002
 * History:-
 * Date       Modified by            Modification Information
 * --------------------------------------------------------------------------
 *
 */
/* Standard include files */

/* Application Specific include files */
#include "sir_common.h"
#include "ani_global.h"

#include "lim_api.h"
#include "sch_api.h"
#include "utils_api.h"

#include "sys_def.h"
#include "sys_entry_func.h"
#include "sys_startup.h"
#include "lim_trace.h"
#include "wma_types.h"
#include "qdf_types.h"
#include "cds_packet.h"

/**
 * sys_init_globals
 *
 * FUNCTION:
 *    Initializes system level global parameters
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 *
 * NOTE:
 *
 * @param struct mac_context *Sirius software parameter struct pointer
 * @return None
 */

QDF_STATUS sys_init_globals(struct mac_context *mac)
{

	qdf_mem_zero((uint8_t *) &mac->sys, sizeof(mac->sys));

	mac->sys.gSysEnableLinkMonitorMode = 0;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS sys_bbt_process_message_core(struct mac_context *mac_ctx,
					struct scheduler_msg *msg,
					uint32_t type, uint32_t subtype)
{
	uint32_t framecount;
	QDF_STATUS ret;
	void *bd_ptr;
	tMgmtFrmDropReason dropreason;
	cds_pkt_t *vos_pkt = (cds_pkt_t *) msg->bodyptr;
	QDF_STATUS qdf_status =
		wma_ds_peek_rx_packet_info(vos_pkt, &bd_ptr, false);

	mac_ctx->sys.gSysBbtReceived++;

	if (!QDF_IS_STATUS_SUCCESS(qdf_status))
		goto fail;

	mac_ctx->sys.gSysFrameCount[type][subtype]++;
	framecount = mac_ctx->sys.gSysFrameCount[type][subtype];

	if (type == SIR_MAC_MGMT_FRAME) {
		tpSirMacMgmtHdr mac_hdr;

		/*
		 * Drop beacon frames in deferred state to avoid VOSS run out of
		 * message wrappers.
		 */
		if ((subtype == SIR_MAC_MGMT_BEACON) &&
		     !GET_LIM_PROCESS_DEFD_MESGS(mac_ctx)) {
			pe_debug("dropping received beacon in deffered state");
			goto fail;
		}

		dropreason = lim_is_pkt_candidate_for_drop(mac_ctx, bd_ptr,
				subtype);
		if (eMGMT_DROP_NO_DROP != dropreason) {
			pe_debug("Mgmt Frame %d being dropped, reason: %d\n",
				subtype, dropreason);
				MTRACE(mac_trace(mac_ctx,
					TRACE_CODE_RX_MGMT_DROP, NO_SESSION,
					dropreason));
			goto fail;
		}

		mac_hdr = WMA_GET_RX_MAC_HEADER(bd_ptr);
		if (subtype == SIR_MAC_MGMT_ASSOC_REQ) {
			pe_debug("ASSOC REQ frame allowed: da: " QDF_MAC_ADDR_FMT ", sa: " QDF_MAC_ADDR_FMT ", bssid: " QDF_MAC_ADDR_FMT ", Assoc Req count so far: %d",
				 QDF_MAC_ADDR_REF(mac_hdr->da),
				 QDF_MAC_ADDR_REF(mac_hdr->sa),
				 QDF_MAC_ADDR_REF(mac_hdr->bssId),
				 mac_ctx->sys.gSysFrameCount[type][subtype]);
		}
		if (subtype == SIR_MAC_MGMT_DEAUTH) {
			pe_debug("DEAUTH frame allowed: da: " QDF_MAC_ADDR_FMT ", sa: " QDF_MAC_ADDR_FMT ", bssid: " QDF_MAC_ADDR_FMT ", DEAUTH count so far: %d",
				 QDF_MAC_ADDR_REF(mac_hdr->da),
				 QDF_MAC_ADDR_REF(mac_hdr->sa),
				 QDF_MAC_ADDR_REF(mac_hdr->bssId),
				 mac_ctx->sys.gSysFrameCount[type][subtype]);
		}
		if (subtype == SIR_MAC_MGMT_DISASSOC) {
			pe_debug("DISASSOC frame allowed: da: " QDF_MAC_ADDR_FMT ", sa: " QDF_MAC_ADDR_FMT ", bssid: " QDF_MAC_ADDR_FMT ", DISASSOC count so far: %d",
				 QDF_MAC_ADDR_REF(mac_hdr->da),
				 QDF_MAC_ADDR_REF(mac_hdr->sa),
				 QDF_MAC_ADDR_REF(mac_hdr->bssId),
				 mac_ctx->sys.gSysFrameCount[type][subtype]);
		}

		/* Post the message to PE Queue */
		ret = lim_post_msg_api(mac_ctx, msg);
		if (ret != QDF_STATUS_SUCCESS) {
			pe_err("posting to LIM2 failed, ret %d\n", ret);
			goto fail;
		}
		mac_ctx->sys.gSysBbtPostedToLim++;
#ifdef FEATURE_WLAN_ESE
	} else if (type == SIR_MAC_DATA_FRAME) {
		pe_debug("IAPP Frame...");
		/* Post the message to PE Queue */
		ret = lim_post_msg_api(mac_ctx, msg);
		if (ret != QDF_STATUS_SUCCESS) {
			pe_err("posting to LIM2 failed, ret: %d", ret);
			goto fail;
		}
		mac_ctx->sys.gSysBbtPostedToLim++;
#endif
	} else {
		pe_debug("BBT received Invalid type: %d subtype: %d "
			"LIM state %X", type, subtype,
			lim_get_sme_state(mac_ctx));
		goto fail;
	}
	return QDF_STATUS_SUCCESS;
fail:
	mac_ctx->sys.gSysBbtDropped++;
	return QDF_STATUS_E_FAILURE;
}

