/*
 * Copyright (c) 2018 The Linux Foundation. All rights reserved.
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
 * DOC: Target interface file for disa component to
 * Implement api's which shall be used by ipa component
 * in target if internally.
 */

#include <target_if.h>
#include <qdf_status.h>
#include <wmi_unified_api.h>
#include <wmi_unified_priv.h>
#include <wmi_unified_param.h>
#include <target_if_ipa.h>
#include <wlan_objmgr_psoc_obj.h>

/**
 * target_if_ipa_uc_offload_control_req() - send IPA offload control to FW
 * @psoc: pointer to PSOC object
 * @req: IPA UC offload enable/disable control param
 *
 * Return: QDF_STATUS_SUCCESS on success
 */
static QDF_STATUS
target_if_ipa_uc_offload_control_req(struct wlan_objmgr_psoc *psoc,
			struct ipa_uc_offload_control_params *req)
{
	return wmi_unified_ipa_offload_control_cmd(
			get_wmi_unified_hdl_from_psoc(psoc), req);
}

void target_if_ipa_register_tx_ops(ipa_uc_offload_control_req *ipa_tx_op)
{
	*ipa_tx_op = target_if_ipa_uc_offload_control_req;
}
