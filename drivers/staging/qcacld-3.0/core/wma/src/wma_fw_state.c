/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wma_fw_state.c
 *
 * The implementation for getting firmware state
 */

#include "wma_fw_state.h"
#include "wmi_unified_api.h"

QDF_STATUS wma_get_fw_state(tp_wma_handle wma_handle)
{
	wmi_echo_cmd_fixed_param *cmd;
	wmi_buf_t wmi_buf;
	uint32_t len = sizeof(*cmd);

	if (!wma_handle) {
		wma_err("WMA is closed, can not issue cmd");
		return QDF_STATUS_E_INVAL;
	}

	wmi_buf = wmi_buf_alloc(wma_handle->wmi_handle, len);
	if (!wmi_buf)
		return QDF_STATUS_E_NOMEM;

	cmd = (wmi_echo_cmd_fixed_param *)wmi_buf_data(wmi_buf);
	WMITLV_SET_HDR(&cmd->tlv_header,
		       WMITLV_TAG_STRUC_wmi_echo_cmd_fixed_param,
		       WMITLV_GET_STRUCT_TLVLEN(
		       wmi_echo_cmd_fixed_param));
	cmd->value = true;

	if (wmi_unified_cmd_send(wma_handle->wmi_handle, wmi_buf, len,
				 WMI_ECHO_CMDID)) {
		wmi_buf_free(wmi_buf);
		return QDF_STATUS_E_FAILURE;
	}

	return QDF_STATUS_SUCCESS;
}

/**
 * wma_echo_event_handler() - process fw state rsp
 * @handle: wma interface
 * @buf: wmi event buf pointer
 * @len: length of event buffer
 *
 * This function will send eWNI_SME_FW_STATUS_IND to SME
 *
 * Return: 0 for success or error code
 */
static int wma_echo_event_handler(void *handle, uint8_t *buf, uint32_t len)
{
	struct scheduler_msg sme_msg = {
		.type = eWNI_SME_FW_STATUS_IND,
	};
	QDF_STATUS qdf_status;

	wma_debug("Received Echo reply from firmware!");

	qdf_status = scheduler_post_message(QDF_MODULE_ID_WMA,
					    QDF_MODULE_ID_SME,
					    QDF_MODULE_ID_SME, &sme_msg);
	if (!QDF_IS_STATUS_SUCCESS(qdf_status)) {
		wma_err("Fail to post fw state reply msg");
		return -EINVAL;
	}

	return 0;
}

void wma_register_fw_state_events(wmi_unified_t wmi_handle)
{
	wmi_unified_register_event_handler(wmi_handle,
					   wmi_echo_event_id,
					   wma_echo_event_handler,
					   WMA_RX_SERIALIZER_CTX);
}
