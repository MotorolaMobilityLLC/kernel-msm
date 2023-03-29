/*
 * Copyright (c) 2017, 2020 The Linux Foundation. All rights reserved.
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
 * DOC: wma_fips_api.c
 *
 * WLAN Host Device Driver FIPS Certification Feature
 */

#include "wma.h"
#include "wma_fips_api.h"
#include "wmi_unified_api.h"

static wma_fips_cb fips_callback;
static void *fips_context;

static int
wma_fips_event_handler(void *handle, uint8_t *event, uint32_t len)
{
	tp_wma_handle wma_handle;
	wmi_unified_t wmi_handle;
	struct wmi_host_fips_event_param param;
	wma_fips_cb callback;
	QDF_STATUS status;

	wma_handle = handle;
	if (!wma_handle) {
		wma_err("NULL wma_handle");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("NULL wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_extract_fips_event_data(wmi_handle, event, &param);

	wma_info("Received FIPS event, pdev:%u status:%u data_len:%u",
		 param.pdev_id, param.error_status, param.data_len);

	/* make sure extraction error is propagated to upper layers */
	if (QDF_IS_STATUS_ERROR(status))
		param.error_status = FIPS_ERROR_OPER_TIMEOUT;

	callback = fips_callback;
	fips_callback = NULL;
	if (callback)
		callback(fips_context, &param);

	return 0;
}

QDF_STATUS wma_fips_request(WMA_HANDLE handle,
			    struct fips_params *param,
			    wma_fips_cb callback,
			    void *context)
{
	tp_wma_handle wma_handle = handle;
	wmi_unified_t wmi_handle;
	QDF_STATUS status;

	if (!wma_handle) {
		wma_err("NULL wma_handle");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = wma_handle->wmi_handle;
	if (!wmi_handle) {
		wma_err("NULL wmi_handle");
		return QDF_STATUS_E_INVAL;
	}

	fips_callback = callback;
	fips_context = context;
	status = wmi_unified_pdev_fips_cmd_send(wmi_handle, param);
	if (QDF_IS_STATUS_ERROR(status)) {
		wma_err("wmi_unified_pdev_fips_cmd_send() error: %u",
			status);
		fips_callback = NULL;
	}

	return status;
}

QDF_STATUS wma_fips_register_event_handlers(WMA_HANDLE handle)
{
	tp_wma_handle wma_handle = handle;

	return wmi_unified_register_event_handler(wma_handle->wmi_handle,
						  wmi_pdev_fips_event_id,
						  wma_fips_event_handler,
						  WMA_RX_WORK_CTX);
}
