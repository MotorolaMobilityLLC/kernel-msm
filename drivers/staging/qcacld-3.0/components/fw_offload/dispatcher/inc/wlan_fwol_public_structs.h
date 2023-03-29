/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
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
 * DOC: contains fw offload structure definations
 */

#ifndef _WLAN_FWOL_PUBLIC_STRUCTS_H_
#define _WLAN_FWOL_PUBLIC_STRUCTS_H_

#include "wlan_objmgr_psoc_obj.h"
#include "wlan_thermal_public_struct.h"
#include "wmi_unified.h"

#ifdef WLAN_FEATURE_ELNA
/**
 * struct set_elna_bypass_request - set eLNA bypass request
 * @vdev_id: vdev id
 * @en_dis: 0 - disable eLNA bypass
 *          1 - enable eLNA bypass
 */
struct set_elna_bypass_request {
	uint8_t vdev_id;
	uint8_t en_dis;
};

/**
 * struct get_elna_bypass_request - get eLNA bypass request
 * @vdev_id: vdev id
 */
struct get_elna_bypass_request {
	uint8_t vdev_id;
};

/**
 * struct get_elna_bypass_response - get eLNA bypass response
 * @vdev_id: vdev id
 * @en_dis: 0 - disable eLNA bypass
 *          1 - enable eLNA bypass
 */
struct get_elna_bypass_response {
	uint8_t vdev_id;
	uint8_t en_dis;
};
#endif

/**
 * struct thermal_throttle_info - thermal throttle info from Target
 * @temperature: current temperature in c Degree
 * @level: target thermal level info
 * @pdev_id: pdev id
 * @therm_throt_levels: Number of thermal throttle levels
 * @level_info: Thermal Stats for each level
 */
struct thermal_throttle_info {
	uint32_t temperature;
	enum thermal_throttle_level level;
	uint32_t pdev_id;
	uint32_t therm_throt_levels;
	struct thermal_throt_level_stats level_info[WMI_THERMAL_STATS_TEMP_THRESH_LEVEL_MAX];
};

/**
 * struct wlan_fwol_callbacks - fw offload callbacks
 * @get_elna_bypass_callback: callback for get eLNA bypass
 * @get_elna_bypass_context: context for get eLNA bypass
 * @get_thermal_stats_callback: callback for get thermal stats
 * @get_thermal_stats_context: context for get thermal stats
 */
struct wlan_fwol_callbacks {
#ifdef WLAN_FEATURE_ELNA
	void (*get_elna_bypass_callback)(void *context,
				     struct get_elna_bypass_response *response);
	void *get_elna_bypass_context;
#endif
#ifdef THERMAL_STATS_SUPPORT
	void (*get_thermal_stats_callback)(void *context,
				     struct thermal_throttle_info *response);
	void *get_thermal_stats_context;
#endif
};

/**
 * struct wlan_fwol_tx_ops - structure of tx func pointers
 * @set_elna_bypass: set eLNA bypass
 * @get_elna_bypass: get eLNA bypass
 * @reg_evt_handler: register event handler
 * @unreg_evt_handler: unregister event handler
 * @send_dscp_up_map_to_fw: send dscp-to-up map values to FW
 * @get_thermal_stats: send get_thermal_stats cmd to FW
 */
struct wlan_fwol_tx_ops {
#ifdef WLAN_FEATURE_ELNA
	QDF_STATUS (*set_elna_bypass)(struct wlan_objmgr_psoc *psoc,
				      struct set_elna_bypass_request *req);
	QDF_STATUS (*get_elna_bypass)(struct wlan_objmgr_psoc *psoc,
				      struct get_elna_bypass_request *req);
#endif
	QDF_STATUS (*reg_evt_handler)(struct wlan_objmgr_psoc *psoc,
				      void *arg);
	QDF_STATUS (*unreg_evt_handler)(struct wlan_objmgr_psoc *psoc,
					void *arg);
#ifdef WLAN_SEND_DSCP_UP_MAP_TO_FW
	QDF_STATUS (*send_dscp_up_map_to_fw)(
			struct wlan_objmgr_psoc *psoc,
			uint32_t *dscp_to_up_map);
#endif
#ifdef THERMAL_STATS_SUPPORT
	QDF_STATUS (*get_thermal_stats)(struct wlan_objmgr_psoc *psoc,
				        enum thermal_stats_request_type req_type,
				        uint8_t therm_stats_offset);
#endif
};

/**
 * struct wlan_fwol_rx_ops - structure of rx func pointers
 * @get_elna_bypass_resp: get eLNA bypass response
 * @get_thermal_stats_resp: thermal stats cmd response callback to fwol
 */
struct wlan_fwol_rx_ops {
#ifdef WLAN_FEATURE_ELNA
	QDF_STATUS (*get_elna_bypass_resp)(struct wlan_objmgr_psoc *psoc,
					 struct get_elna_bypass_response *resp);
#endif
#ifdef THERMAL_STATS_SUPPORT
	QDF_STATUS (*get_thermal_stats_resp)(struct wlan_objmgr_psoc *psoc,
					    struct thermal_throttle_info *resp);
#endif
};

#endif /* _WLAN_FWOL_PUBLIC_STRUCTS_H_ */

