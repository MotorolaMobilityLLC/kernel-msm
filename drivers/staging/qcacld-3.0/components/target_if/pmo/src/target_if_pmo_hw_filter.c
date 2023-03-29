/*
 * Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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
 * DOC: target_if_pmo_non_arp_bcast_fltr.c
 *
 * Target interface file for pmo component to
 * send non arp hw bcast filtering related cmd and process event.
 */

#include "target_if.h"
#include "target_if_pmo.h"
#include "wmi_unified_api.h"
#include "wlan_pmo_hw_filter_public_struct.h"

QDF_STATUS target_if_pmo_conf_hw_filter(struct wlan_objmgr_psoc *psoc,
					struct pmo_hw_filter_params *req)
{
	QDF_STATUS status;
	wmi_unified_t wmi_handle;

	if (!psoc) {
		target_if_err("psoc is NULL");
		return QDF_STATUS_E_INVAL;
	}

	wmi_handle = get_wmi_unified_hdl_from_psoc(psoc);
	if (!wmi_handle) {
		target_if_err("Invalid wmi handle");
		return QDF_STATUS_E_INVAL;
	}

	status = wmi_unified_conf_hw_filter_cmd(wmi_handle, req);

	if (QDF_IS_STATUS_ERROR(status))
		target_if_err("Failed to configure HW Filter");

	return status;
}
