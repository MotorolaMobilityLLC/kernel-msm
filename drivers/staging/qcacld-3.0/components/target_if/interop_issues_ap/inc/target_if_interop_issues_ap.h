/*
 * Copyright (c) 2019 The Linux Foundation. All rights reserved.
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
 * DOC: offload lmac interface APIs for interop issues ap
 */
#ifndef __TARGET_IF_INTEROP_ISSUES_AP_H__
#define __TARGET_IF_INTEROP_ISSUES_AP_H__

#ifdef WLAN_FEATURE_INTEROP_ISSUES_AP
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_pdev_obj.h>
#include <qdf_status.h>
#include <wlan_lmac_if_def.h>
#include <wlan_interop_issues_ap_public_structs.h>

/**
 * target_if_interop_issues_ap_register_event_handler() - register callback
 * @psoc: the pointer to psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_interop_issues_ap_register_event_handler(
						struct wlan_objmgr_psoc *psoc);

/**
 * target_if_interop_issues_ap_unregister_event_handler() - unregister callback
 * @psoc: the pointer to psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
target_if_interop_issues_ap_unregister_event_handler(
						struct wlan_objmgr_psoc *psoc);

/**
 * target_if_interop_issues_ap_register_tx_ops() - register tx ops funcs
 * @psoc: the pointer of psoc object
 * @tx_ops: pointer to rap_ps tx ops
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS
target_if_interop_issues_ap_register_tx_ops(struct wlan_objmgr_psoc *psoc,
				struct wlan_interop_issues_ap_tx_ops *tx_ops);
/**
 * target_if_interop_issues_ap_unregister_tx_ops() - unregister tx ops funcs
 * @psoc: the pointer of psoc object
 * @tx_ops: pointer to rap_ps tx ops
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS
target_if_interop_issues_ap_unregister_tx_ops(struct wlan_objmgr_psoc *psoc,
				 struct wlan_interop_issues_ap_tx_ops *tx_ops);
#endif
#endif /* __TARGET_IF_INTEROP_ISSUES_AP_H__ */
