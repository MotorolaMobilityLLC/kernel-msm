/*
 * Copyright (c) 2017-2020 The Linux Foundation. All rights reserved.
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
 * DOC: Declare static configuration on vdev attach
 */

#ifndef _WLAN_PMO_STATIC_CONFIG_H_
#define _WLAN_PMO_STATIC_CONFIG_H_

#ifdef WLAN_POWER_MANAGEMENT_OFFLOAD

#include "wlan_pmo_common_public_struct.h"
#include "wlan_pmo_wow.h"

/**
 * pmo_register_wow_wakeup_events() - register vdev specific wake events with fw
 * @vdev: objmgr vdev
 *
 * WoW wake up event rule is following:
 * 1) STA mode and P2P CLI mode wake up events are same
 * 2) SAP mode and P2P GO mode wake up events are same
 * 3) IBSS mode wake events are same as STA mode plus WOW_BEACON_EVENT
 *
 * Return: none
 */
void pmo_register_wow_wakeup_events(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_register_wow_default_patterns() - register default wow patterns with fw
 * @vdev_id: vdev id
 *
 * WoW default wake up pattern rule is:
 *  - For STA & P2P CLI mode register for same STA specific wow patterns
 *  - For SAP/P2P GO & IBSS mode register for same SAP specific wow patterns
 *
 * Return: none
 */
void pmo_register_wow_default_patterns(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_register_action_frame_patterns() - register action frame map to fw
 * @vdev: objmgr vdev
 * @suspend_type: suspend mode runtime pm suspend or normal suspend.
 *
 * This is called to push action frames wow patterns from local
 * cache to firmware.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
pmo_register_action_frame_patterns(struct wlan_objmgr_vdev *vdev,
				   enum qdf_suspend_type suspend_type);

/**
 * pmo_clear_action_frame_patterns() - clear the action frame
 * pattern bitmap in firmware
 * @vdev: objmgr vdev
 *
 * Return: QDF_STATUS
 */
QDF_STATUS pmo_clear_action_frame_patterns(struct wlan_objmgr_vdev *vdev);

/**
 * pmo_set_wow_event_bitmap() - Assign bitmask with wow event
 * @event: wow event
 * @wow_bitmap_size: wow bitmask size
 * @bitmask: wow bitmask field
 *
 * Return: none
 */
void pmo_set_wow_event_bitmap(WOW_WAKE_EVENT_TYPE event,
			      uint32_t wow_bitmap_size,
			      uint32_t *bitmask);

/**
 * pmo_set_sta_wow_bitmask() - set predefined STA wow wakeup events
 * @bitmask: bitmask field
 * @wow_bitmask_size: bitmask field size
 *
 * Return: none
 */
void pmo_set_sta_wow_bitmask(uint32_t *bitmask, uint32_t wow_bitmask_size);

/**
 * pmo_set_sap_wow_bitmask() - set predefined SAP wow wakeup events
 * @bitmask: bitmask field
 * @wow_bitmask_size: bitmask field size
 *
 * Return: none
 */
void pmo_set_sap_wow_bitmask(uint32_t *bitmask, uint32_t wow_bitmask_size);

#ifdef WLAN_FEATURE_NAN
/**
 * pmo_set_ndp_wow_bitmask() - set predefined NDP wow wakeup events
 * @bitmask: bitmask field
 * @wow_bitmask_size: bitmask field size
 *
 * Return: none
 */
void pmo_set_ndp_wow_bitmask(uint32_t *bitmask, uint32_t wow_bitmask_size);
#else
static inline
void pmo_set_ndp_wow_bitmask(uint32_t *bitmask, uint32_t wow_bitmask_size)
{
}
#endif

#endif /* WLAN_POWER_MANAGEMENT_OFFLOAD */

#endif /* end  of _WLAN_PMO_STATIC_CONFIG_H_ */
