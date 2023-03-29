/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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
 * DOC: Contains p2p configures interface definitions
 */

#ifndef _WLAN_P2P_CFG_API_H_
#define _WLAN_P2P_CFG_API_H_

#include <qdf_types.h>

struct wlan_objmgr_psoc;

/**
 * cfg_p2p_get_go_keepalive_period() - get go keepalive period
 * @psoc:        pointer to psoc object
 * @period:      go keepalive period
 *
 * This function gets go keepalive period to p2p component
 */
QDF_STATUS
cfg_p2p_get_go_keepalive_period(struct wlan_objmgr_psoc *psoc,
				uint32_t *period);

/**
 * cfg_p2p_get_go_link_monitor_period() - get go link monitor period
 * @psoc:        pointer to psoc object
 * @period:      go link monitor period
 *
 * This function gets go link monitor period to p2p component
 */
QDF_STATUS
cfg_p2p_get_go_link_monitor_period(struct wlan_objmgr_psoc *psoc,
				   uint32_t *period);

/**
 * cfg_p2p_get_device_addr_admin() - get enable/disable p2p device
 * addr administrated
 * @psoc:        pointer to psoc object
 * @enable:      enable/disable p2p device addr administrated
 *
 * This function gets enable/disable p2p device addr administrated
 */
QDF_STATUS
cfg_p2p_get_device_addr_admin(struct wlan_objmgr_psoc *psoc,
			      bool *enable);

/**
 * cfg_p2p_is_roam_config_disabled() - get disable roam config on sta interface
 * during p2p connection
 * @psoc:        pointer to psoc object
 *
 * Get disable roam configuration during p2p connection
 */
bool cfg_p2p_is_roam_config_disabled(struct wlan_objmgr_psoc *psoc);

#endif /* _WLAN_P2P_CFG_API_H_ */
