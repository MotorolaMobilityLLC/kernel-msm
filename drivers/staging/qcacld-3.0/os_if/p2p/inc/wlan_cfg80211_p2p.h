/*
 * Copyright (c) 2017-2018 The Linux Foundation. All rights reserved.
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
 * DOC: declares P2P functions interfacing with linux kernel
 */

#ifndef _WLAN_CFG80211_P2P_H_
#define _WLAN_CFG80211_P2P_H_

#include <qdf_types.h>

struct wlan_objmgr_psoc;
struct wlan_objmgr_vdev;
struct ieee80211_channel;

/**
 * p2p_psoc_enable() - psoc API to enable p2p component
 * @psoc: soc object
 *
 * This function used to enable P2P component and register events.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * p2p_psoc_disable() - psoc API to disable p2p component
 * @psoc: soc object
 *
 * This function used to disable P2P component and unregister events.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_psoc_disable(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_cfg80211_roc() - API to process cfg80211 roc request
 * @vdev: Pointer to vdev object
 * @chan: Pointer to channel
 * @duration: Duration for this roc request
 * @cookie: Pointer to return cookie to up layer
 *
 * API to trigger remain on channel request. It returns cookie
 * as the identifier of roc.
 *
 * Return: 0 for success, non zero for failure
 */
int wlan_cfg80211_roc(struct wlan_objmgr_vdev *vdev,
	struct ieee80211_channel *chan, uint32_t duration,
	uint64_t *cookie);

/**
 * wlan_cfg80211_cancel_roc() - API to process cfg80211 cancel remain
 * on channel request
 * @vdev: Pointer to vdev object
 * @cookie: Find out the roc request by cookie
 *
 * API to trigger cancel remain on channel request.
 *
 * Return: 0 for success, non zero for failure
 */
int wlan_cfg80211_cancel_roc(struct wlan_objmgr_vdev *vdev,
	uint64_t cookie);

/**
 * wlan_cfg80211_mgmt_tx() - API to process cfg80211 mgmt tx request
 * @vdev: Pointer to vdev object
 * @chan: Pointer to channel
 * @wait: wait time for this mgmt tx request
 * @buf: TX buffer
 * @len: Length of tx buffer
 * @no_cck: Required cck or not
 * @dont_wait_for_ack: Wait for ack or not
 * @cookie: Return the cookie to caller
 *
 * API to trigger mgmt frame tx request. It returns cookie as the
 * identifier of this tx.
 *
 * Return: 0 for success, non zero for failure
 */
int wlan_cfg80211_mgmt_tx(struct wlan_objmgr_vdev *vdev,
	struct ieee80211_channel *chan, bool offchan, uint32_t wait,
	const uint8_t *buf, uint32_t len, bool no_cck,
	bool dont_wait_for_ack, uint64_t *cookie);

/**
 * wlan_cfg80211_mgmt_tx_cancel() - API to process cfg80211 cancel to
 * wait mgmt tx
 * @vdev: Pointer to vdev object
 * @cookie: Find out the mgmt tx request by cookie
 *
 * API to trigger cancel mgmt frame tx request.
 *
 * Return: 0 for success, non zero for failure
 */
int wlan_cfg80211_mgmt_tx_cancel(struct wlan_objmgr_vdev *vdev,
		uint64_t cookie);

#endif /* _WLAN_CFG80211_P2P_H_ */
