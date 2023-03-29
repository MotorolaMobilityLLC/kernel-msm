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
 * DOC: wlan_interop_issues_ap_api.h
 *
 * This header file provide API declarations required for interop issues
 * ap global context specific to offload
 */

#ifndef __WLAN_INTEROP_ISSUES_AP_API_H__
#define __WLAN_INTEROP_ISSUES_AP_API_H__

#ifdef WLAN_FEATURE_INTEROP_ISSUES_AP
#include <qdf_types.h>
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_interop_issues_ap_public_structs.h>

#define interop_issues_ap_debug(args ...) \
	QDF_TRACE_DEBUG(QDF_MODULE_ID_INTEROP_ISSUES_AP, ## args)
#define interop_issues_ap_err(args ...) \
	QDF_TRACE_ERROR(QDF_MODULE_ID_INTEROP_ISSUES_AP, ## args)

/**
 * struct interop_issues_ap_psoc_priv_obj - psoc private object
 * @lock: qdf spin lock
 * @soc: pointer to psoc object
 * @cbs: interop issues ap ps event callbacks
 * @tx_ops: interop issues ap ps tx ops
 */
struct interop_issues_ap_psoc_priv_obj {
	qdf_spinlock_t lock;
	struct wlan_objmgr_psoc *soc;
	struct wlan_interop_issues_ap_callbacks cbs;
	struct wlan_interop_issues_ap_tx_ops tx_ops;
};

/**
 * wlan_interop_issues_ap_psoc_enable() - interop issues ap psoc enable
 * @psoc: the pointer to psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_interop_issues_ap_psoc_enable(struct wlan_objmgr_psoc *soc);

/**
 * wlan_interop_issues_ap_psoc_disable() - interop issues ap psoc disable
 * @psoc: the pointer to psoc object
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_interop_issues_ap_psoc_disable(struct wlan_objmgr_psoc *soc);

/**
 * wlan_interop_issues_ap_init() - API to init component
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_interop_issues_ap_init(void);

/**
 * wlan_interop_issues_ap_deinit() - API to deinit component
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS wlan_interop_issues_ap_deinit(void);

/**
 * interop_issues_ap_get_psoc_priv_obj() - get priv object from psoc object
 * @psoc: pointer to psoc object
 *
 * Return: pointer to interop issues ap psoc private object
 */
static inline
struct interop_issues_ap_psoc_priv_obj *interop_issues_ap_get_psoc_priv_obj(
						struct wlan_objmgr_psoc *psoc)
{
	struct interop_issues_ap_psoc_priv_obj *obj;

	if (!psoc)
		return NULL;

	obj = wlan_objmgr_psoc_get_comp_private_obj(psoc,
					WLAN_UMAC_COMP_INTEROP_ISSUES_AP);

	return obj;
}

/**
 * interop_issues_ap_psoc_get_tx_ops() - get TX ops from the private object
 * @psoc: pointer to psoc object
 *
 * Return: pointer to TX op callback
 */
static inline
struct wlan_interop_issues_ap_tx_ops *interop_issues_ap_psoc_get_tx_ops(
						struct wlan_objmgr_psoc *psoc)
{
	struct interop_issues_ap_psoc_priv_obj *interop_issues_ap_priv;

	if (!psoc)
		return NULL;

	interop_issues_ap_priv = interop_issues_ap_get_psoc_priv_obj(psoc);
	if (!interop_issues_ap_priv) {
		interop_issues_ap_err("psoc private object is null");
		return NULL;
	}

	return &interop_issues_ap_priv->tx_ops;
}

/**
 * interop_issues_ap_psoc_get_cbs() - get RX ops from private object
 * @psoc: pointer to psoc object
 *
 * Return: pointer to RX op callback
 */
static inline
struct wlan_interop_issues_ap_callbacks *interop_issues_ap_psoc_get_cbs(
						struct wlan_objmgr_psoc *psoc)
{
	struct interop_issues_ap_psoc_priv_obj *interop_issues_ap_priv;

	if (!psoc)
		return NULL;

	interop_issues_ap_priv = interop_issues_ap_get_psoc_priv_obj(psoc);
	if (!interop_issues_ap_priv) {
		interop_issues_ap_err("psoc private object is null");
		return NULL;
	}

	return &interop_issues_ap_priv->cbs;
}
#endif
#endif
