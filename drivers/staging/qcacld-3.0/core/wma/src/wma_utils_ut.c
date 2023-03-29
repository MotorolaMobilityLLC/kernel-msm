/*
 * Copyright (c) 2015-2016, 2018-2020 The Linux Foundation. All rights reserved.
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
 *  DOC:    wma_utils_ut.c
 *  This file contains utilities related to unit test framework
 */

/* Header files */

#include "wma.h"

/**
 * wma_set_dbs_capability_ut() - Set DBS capability for UT framework
 * @dbs: Value of DBS capability to be set
 *
 * Sets the DBS capability for unit test framework. If the HW mode is
 * already sent by the FW, only the DBS capability needs to be set. If the
 * FW did not send any HW mode list, a single entry is created and DBS mode
 * is set in it. The DBS capability is also set in the service bit map.
 *
 * Return: None
 */
void wma_set_dbs_capability_ut(uint32_t dbs)
{
	tp_wma_handle wma;
	uint32_t i;

	wma = cds_get_context(QDF_MODULE_ID_WMA);
	if (!wma) {
		wma_err("Invalid WMA handle");
		return;
	}

	/* DBS list was not configured by the FW, so
	 * for UT, we can configure a single entry
	 */
	if (!wma->hw_mode.hw_mode_list) {
		wma->num_dbs_hw_modes = 1;
		wma->hw_mode.hw_mode_list =
			qdf_mem_malloc(sizeof(*wma->hw_mode.hw_mode_list) *
					wma->num_dbs_hw_modes);
		if (!wma->hw_mode.hw_mode_list)
			return;

		wma->hw_mode.hw_mode_list[0] = 0x0000;
	}

	for (i = 0; i < wma->num_dbs_hw_modes; i++) {
		WMA_HW_MODE_DBS_MODE_SET(wma->hw_mode.hw_mode_list[i],
				dbs);
	}

	WMI_SERVICE_ENABLE(wma->wmi_service_bitmap,
			WMI_SERVICE_DUAL_BAND_SIMULTANEOUS_SUPPORT);
}
