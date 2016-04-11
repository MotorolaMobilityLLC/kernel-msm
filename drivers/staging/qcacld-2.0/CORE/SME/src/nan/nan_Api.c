/*
 * Copyright (c) 2014, 2016 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
 */

#include "sme_Api.h"
#include "smsDebug.h"
#include "csrInsideApi.h"
#include "smeInside.h"
#include "nan_Api.h"
#include "cfgApi.h"
#include "wma.h"

/******************************************************************************
 * Function: sme_NanRegisterCallback
 *
 * Description:
 * This function gets called when HDD wants register nan rsp callback with
 * sme layer.
 *
 * Args:
 * hHal and callback which needs to be registered.
 *
 * Returns:
 * void
 *****************************************************************************/
void sme_NanRegisterCallback(tHalHandle hHal, NanCallback callback)
{
	 tpAniSirGlobal pMac = NULL;

	 if (NULL == hHal) {
		 VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
				FL("hHal is not valid"));
		 return;
	 }
	 pMac = PMAC_STRUCT(hHal);
	 pMac->sme.nanCallback = callback;
}

/******************************************************************************
 * Function: sme_NanRequest
 *
 * Description:
 * This function gets called when HDD receives NAN vendor command
 * from user space
 *
 * Args:
 * Nan Request structure ptr
 *
 * Returns:
 * VOS_STATUS
 *****************************************************************************/
VOS_STATUS sme_NanRequest(tpNanRequestReq input)
{
	 vos_msg_t msg;
	 tpNanRequest data;
	 size_t data_len;

	 data_len = sizeof(tNanRequest) + input->request_data_len;
	 data = vos_mem_malloc(data_len);

	 if (data == NULL) {
		 VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
				FL("Memory allocation failure"));
		 return VOS_STATUS_E_FAULT;
	 }

	 vos_mem_zero(data, data_len);
	 data->request_data_len = input->request_data_len;
	 if (input->request_data_len) {
		 vos_mem_copy(data->request_data,
				input->request_data, input->request_data_len);
	 }

	 msg.type = WDA_NAN_REQUEST;
	 msg.reserved = 0;
	 msg.bodyptr = data;

	 if (VOS_STATUS_SUCCESS != vos_mq_post_message(VOS_MODULE_ID_WDA,
					 &msg)) {
	 VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
		FL("Not able to post WDA_NAN_REQUEST message to WDA"));
		vos_mem_free(data);
	 return VOS_STATUS_SUCCESS;
	 }

	 return VOS_STATUS_SUCCESS;
}

/******************************************************************************
 * Function: sme_NanEvent
 *
 * Description:
 * This callback function will be called when SME received eWNI_SME_NAN_EVENT
 * event from WMA
 *
 * Args:
 * hHal - HAL handle for device
 * pMsg - Message body passed from WDA; includes NAN header
 *
 * Returns:
 * VOS_STATUS
******************************************************************************/
VOS_STATUS sme_NanEvent(tHalHandle hHal, void* pMsg)
{
	 tpAniSirGlobal pMac = PMAC_STRUCT(hHal);
	 VOS_STATUS status = VOS_STATUS_SUCCESS;

	 if (NULL == pMsg) {
		 VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_ERROR,
				FL("msg ptr is NULL"));
		 status = VOS_STATUS_E_FAILURE;
	 } else {
		 VOS_TRACE(VOS_MODULE_ID_SME, VOS_TRACE_LEVEL_INFO_MED,
				FL("SME: Received sme_NanEvent"));
		 if (pMac->sme.nanCallback) {
			 pMac->sme.nanCallback(pMac->hHdd,
                                              (tSirNanEvent *)pMsg);
		 }
	 }

	 return status;
}
