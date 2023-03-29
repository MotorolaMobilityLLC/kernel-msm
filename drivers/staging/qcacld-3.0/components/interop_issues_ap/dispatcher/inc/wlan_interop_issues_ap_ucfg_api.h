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
 * DOC: wlan_interop_issues_ap_ucfg_api.h
 *
 * This header file maintain API declaration required for northbound interaction
 */

#ifndef __WLAN_INTEROP_ISSUES_AP_UCFG_API_H__
#define __WLAN_INTEROP_ISSUES_AP_UCFG_API_H__

#ifdef WLAN_FEATURE_INTEROP_ISSUES_AP
#include <qdf_status.h>
#include <qdf_types.h>
#include <wlan_interop_issues_ap_public_structs.h>

/**
 * ucfg_interop_issues_ap_psoc_enable() - interop issues ap component enable
 * @psoc: the point to psoc object
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS ucfg_interop_issues_ap_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_interop_issues_ap_psoc_disable() - interop issues ap component disable
 * @psoc: the point to psoc object
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS ucfg_interop_issues_ap_psoc_disable(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_interop_issues_ap_init() - interop issues ap component initialization
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS ucfg_interop_issues_ap_init(void);

/**
 * ucfg_interop_issues_ap_deinit() - interop issues ap component de-init
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS ucfg_interop_issues_ap_deinit(void);

/**
 * ucfg_register_interop_issues_ap_callback() - API to register callback
 * @cbs: pointer to callback structure
 *
 * Return: none
 */
void ucfg_register_interop_issues_ap_callback(struct wlan_objmgr_pdev *pdev,
				struct wlan_interop_issues_ap_callbacks *cbs);

/**
 * ucfg_set_interop_issues_ap_config() - API to set interop issues ap
 * @psoc: the pointer of psoc object
 * @rap: the pointer of interop issues ap info
 *
 * Return: none
 */
QDF_STATUS ucfg_set_interop_issues_ap_config(struct wlan_objmgr_psoc *psoc,
				     struct wlan_interop_issues_ap_info *rap);
#else
static inline
QDF_STATUS ucfg_interop_issues_ap_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_interop_issues_ap_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_interop_issues_ap_init(void) { return QDF_STATUS_SUCCESS; }

static inline
QDF_STATUS ucfg_interop_issues_ap_deinit(void) { return QDF_STATUS_SUCCESS; }
#endif /* WLAN_FEATURE_INTEROP_ISSUES_AP */
#endif /* __WLAN_RAP_PS_UCFG_API_H__ */
