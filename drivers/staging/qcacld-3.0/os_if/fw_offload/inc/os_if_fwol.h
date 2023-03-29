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
 * DOC: os_if_fwol.h
 *
 * This Header file provide declaration for OS interface API
 */

#ifndef __OS_IF_FWOL_H__
#define __OS_IF_FWOL_H__

#include "wlan_objmgr_vdev_obj.h"
#include "wlan_fwol_public_structs.h"

#ifdef WLAN_FEATURE_ELNA
/**
 * os_if_fwol_set_elna_bypass() - Set eLNA bypass
 * @vdev: Pointer to vdev
 * @attr: Pointer to struct nlattr
 *
 * Return: 0 on success; error number otherwise
 */
int os_if_fwol_set_elna_bypass(struct wlan_objmgr_vdev *vdev,
			       const struct nlattr *attr);

/**
 * os_if_fwol_get_elna_bypass() - Get eLNA bypass
 * @vdev: Pointer to vdev
 * @skb: sk buffer to hold nl80211 attributes
 * @attr: Pointer to struct nlattr
 *
 * Return: 0 on success; error number otherwise
 */
int os_if_fwol_get_elna_bypass(struct wlan_objmgr_vdev *vdev,
			       struct sk_buff *skb,
			       const struct nlattr *attr);
#else
static inline int os_if_fwol_set_elna_bypass(struct wlan_objmgr_vdev *vdev,
					     const struct nlattr *attr)
{
	return 0;
}

static inline int os_if_fwol_get_elna_bypass(struct wlan_objmgr_vdev *vdev,
					     struct sk_buff *skb,
					     const struct nlattr *attr)
{
	return 0;
}
#endif /* WLAN_FEATURE_ELNA */

#ifdef WLAN_SEND_DSCP_UP_MAP_TO_FW
/**
 * os_if_fwol_send_dscp_up_map_to_fw() - Send DSCP to UP map to FW
 * @vdev: Pointer to vdev
 * @dscp_to_up_map: Array of DSCP to UP map values
 *
 * Return: 0 on success; error number otherwise
 */
int os_if_fwol_send_dscp_up_map_to_fw(struct wlan_objmgr_vdev *vdev,
				     uint32_t *dscp_to_up_map);
#else
static inline int os_if_fwol_send_dscp_up_map_to_fw(
		struct wlan_objmgr_vdev *vdev, uint32_t *dscp_to_up_map)
{
	return -EOPNOTSUPP;
}
#endif /* WLAN_SEND_DSCP_UP_MAP_TO_FW */

#ifdef THERMAL_STATS_SUPPORT
int os_if_fwol_get_thermal_stats_req(struct wlan_objmgr_psoc *psoc,
				     enum thermal_stats_request_type req,
				     void (*callback)(void *context,
				     struct thermal_throttle_info *response),
				     void *context);
#endif /* THERMAL_STATS_SUPPORT */

#endif /* __OS_IF_FWOL_H__ */
