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
 * DOC: wlan_interop_issues_ap_tgt_api.h
 *
 * This header file provide with API declarations to interface with Southbound
 */
#ifndef __WLAN_INTEROP_ISSUES_AP_TGT_API_H__
#define __WLAN_INTEROP_ISSUES_AP_TGT_API_H__

#ifdef WLAN_FEATURE_INTEROP_ISSUES_AP
/**
 * tgt_interop_issues_ap_info_callback() - interop issues ap info callback
 * @psoc: the pointer to psoc object manager
 * @rap: the interop issues ap mac address
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
tgt_interop_issues_ap_info_callback(struct wlan_objmgr_psoc *psoc,
				    struct wlan_interop_issues_ap_event *rap);

/**
 * tgt_set_interop_issues_ap_req(): API to set interop issues ap to lmac
 * @rx_ops: rx ops struct
 * @rap: the pointer to interop issues ap info
 *
 * Return: status of operation
 */
QDF_STATUS
tgt_set_interop_issues_ap_req(struct wlan_objmgr_psoc *psoc,
			      struct wlan_interop_issues_ap_info *rap);
#endif
#endif /* __WLAN_INTEROP_ISSUES_AP_TGT_API_H__ */
