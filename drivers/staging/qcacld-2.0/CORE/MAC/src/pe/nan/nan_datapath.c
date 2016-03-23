/*
 * Copyright (c) 2016 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
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
 * DOC: nan_datapath.c
 *
 * MAC NAN Data path API implementation
 */

#include "limUtils.h"
#include "limApi.h"
#include "nan_datapath.h"
#include "limTypes.h"
#include "limSendMessages.h"

/**
 * handle_ndp_request_message() - Function to handle NDP requests from SME
 * @mac_ctx: handle to mac structure
 * @msg: pointer to message
 *
 * Return: VOS_STATUS_SUCCESS on success; error number otherwise
 */
VOS_STATUS handle_ndp_request_message(tpAniSirGlobal mac_ctx, tpSirMsgQ msg)
{
	return VOS_STATUS_SUCCESS;
}

/**
 * handle_ndp_event_message() - Handler for NDP events from WMA
 * @mac_ctx: handle to mac structure
 * @msg: pointer to message
 *
 * Return: VOS_STATUS_SUCCESS on success; error number otherwise
 */
VOS_STATUS handle_ndp_event_message(tpAniSirGlobal mac_ctx, tpSirMsgQ msg)
{
	return VOS_STATUS_SUCCESS;
}

/**
 * lim_process_ndi_mlm_add_bss_rsp() - Process ADD_BSS response for NDI
 * @mac_ctx: Pointer to Global MAC structure
 * @lim_msgq: The MsgQ header, which contains the response buffer
 * @session_entry: PE session
 *
 * Return: None
 */
void lim_process_ndi_mlm_add_bss_rsp(tpAniSirGlobal mac_ctx, tpSirMsgQ lim_msgq,
					tpPESession session_entry)
{
	tLimMlmStartCnf mlm_start_cnf;
	tpAddBssParams add_bss_params = (tpAddBssParams) lim_msgq->bodyptr;

	limLog(mac_ctx, LOG1, FL("Status %d"), add_bss_params->status);
	if (NULL == add_bss_params) {
		limLog(mac_ctx, LOGE, FL("Invalid body pointer in message"));
		goto end;
	}
	if (eHAL_STATUS_SUCCESS == add_bss_params->status) {
		limLog(mac_ctx, LOG1,
		       FL("WDA_ADD_BSS_RSP returned eHAL_STATUS_SUCCESS"));
		session_entry->limMlmState = eLIM_MLM_BSS_STARTED_STATE;
		MTRACE(macTrace(mac_ctx, TRACE_CODE_MLM_STATE,
			session_entry->peSessionId,
			session_entry->limMlmState));
		session_entry->bssIdx = (uint8_t) add_bss_params->bssIdx;
		session_entry->limSystemRole = eLIM_NDI_ROLE;
		session_entry->statypeForBss = STA_ENTRY_SELF;
		/* Apply previously set configuration at HW */
		limApplyConfiguration(mac_ctx, session_entry);
		mlm_start_cnf.resultCode = eSIR_SME_SUCCESS;
	} else {
		limLog(mac_ctx, LOGE,
			FL("WDA_ADD_BSS_REQ failed with status %d"),
			add_bss_params->status);
		mlm_start_cnf.resultCode = eSIR_SME_HAL_SEND_MESSAGE_FAIL;
	}
	mlm_start_cnf.sessionId = session_entry->peSessionId;
	limPostSmeMessage(mac_ctx, LIM_MLM_START_CNF,
				(uint32_t *) &mlm_start_cnf);
end:
	vos_mem_free(lim_msgq->bodyptr);
	lim_msgq->bodyptr = NULL;
}

/**
 * lim_ndi_del_bss_rsp() - Handler DEL BSS resp for NDI interface
 * @mac_ctx: handle to mac structure
 * @msg: pointer to message
 * @session_entry: session entry
 *
 * Return: void
 */
void lim_ndi_del_bss_rsp(tpAniSirGlobal  mac_ctx,
			void *msg, tpPESession session_entry)
{
	tSirResultCodes rc = eSIR_SME_SUCCESS;
	tpDeleteBssParams del_bss = (tpDeleteBssParams) msg;

	SET_LIM_PROCESS_DEFD_MESGS(mac_ctx, true);
	if (del_bss == NULL) {
		limLog(mac_ctx, LOGE,
			FL("NDI: DEL_BSS_RSP with no body!"));
		rc = eSIR_SME_STOP_BSS_FAILURE;
		goto end;
	}
	session_entry =
		peFindSessionBySessionId(mac_ctx, del_bss->sessionId);
	if (!session_entry) {
		limLog(mac_ctx, LOGE,
			FL("Session Does not exist for given sessionID"));
		goto end;
	}

	if (del_bss->status != eHAL_STATUS_SUCCESS) {
		limLog(mac_ctx, LOGE, FL("NDI: DEL_BSS_RSP error (%x) Bss %d "),
			del_bss->status, del_bss->bssIdx);
		rc = eSIR_SME_STOP_BSS_FAILURE;
		goto end;
	}

	if (limSetLinkState(mac_ctx, eSIR_LINK_IDLE_STATE,
			session_entry->selfMacAddr,
			session_entry->selfMacAddr, NULL, NULL)
			!= eSIR_SUCCESS) {
		limLog(mac_ctx, LOGE,
			FL("NDI: DEL_BSS_RSP setLinkState failed"));
		goto end;
	}

	session_entry->limMlmState = eLIM_MLM_IDLE_STATE;

end:
	if (del_bss)
		vos_mem_free(del_bss);
	/* Delete PE session once BSS is deleted */
	if (NULL != session_entry) {
		limSendSmeRsp(mac_ctx, eWNI_SME_STOP_BSS_RSP,
			rc, session_entry->smeSessionId,
			session_entry->transactionId);
		peDeleteSession(mac_ctx, session_entry);
		session_entry = NULL;
	}
}
