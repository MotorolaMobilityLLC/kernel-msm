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
 * DOC: contains interop issues ap structure definations
 */

#ifndef _WLAN_INTEROP_ISSUES_AP_STRUCTS_H_
#define _WLAN_INTEROP_ISSUES_AP_STRUCTS_H_

#ifdef WLAN_FEATURE_INTEROP_ISSUES_AP
#include <qdf_types.h>
#include <wlan_objmgr_psoc_obj.h>

#define MAX_INTEROP_ISSUES_AP_NUM 20

/**
 * struct wlan_interop_issues_ap_info - interop issues ap info
 * @detect_enable: the flag to enable detect issue ap
 * @count: the number of interop issues ap
 * @rap_items: interop issues ap items
 */
struct wlan_interop_issues_ap_info {
	bool detect_enable;
	uint32_t count;
	struct qdf_mac_addr rap_items[MAX_INTEROP_ISSUES_AP_NUM];
};

/**
 * struct wlan_interop_issues_ap_event - interop issues ap event
 * @pdev: pdev object
 * @psoc: psoc object
 * @pdev_id: pdev id number
 * @rap_addr: interop issues ap mac address
 */
struct wlan_interop_issues_ap_event {
	struct wlan_objmgr_pdev *pdev;
	struct wlan_objmgr_psoc *psoc;
	uint32_t pdev_id;
	struct qdf_mac_addr rap_addr;
};

/**
 * struct wlan_interop_issues_ap_callbacks - interop issues ap callbacks
 * @os_if_interop_issues_ap_event_handler: OS IF callback for handling events
 */
struct wlan_interop_issues_ap_callbacks {
	void (*os_if_interop_issues_ap_event_handler)
				(struct wlan_interop_issues_ap_event *event);
};

/**
 * struct wlan_interop_issues_ap_tx_ops - structure of tx func pointers
 * @set_rap_ps: handler for TX operations for the interop issues ap ps config
 */
struct wlan_interop_issues_ap_tx_ops {
	QDF_STATUS (*set_rap_ps)(struct wlan_objmgr_psoc *psoc,
				 struct wlan_interop_issues_ap_info *rap);
};
#endif
#endif /* _WLAN_INTEROP_ISSUES_AP_STRUCTS_H_ */
