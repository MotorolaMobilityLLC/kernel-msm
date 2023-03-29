/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_tdls_peer.h
 *
 * TDLS peer function declaration
 */

#if !defined(_WLAN_TDLS_PEER_H_)
#define _WLAN_TDLS_PEER_H_

/**
 * struct tdls_search_peer_param - used to search TDLS peer
 * @macaddr: MAC address of peer
 * @peer: pointer to the found peer
 */
struct tdls_search_peer_param {
	const uint8_t *macaddr;
	struct tdls_peer *peer;
};

/**
 * struct tdls_progress_param - used to search progress TDLS peer
 * @skip_self: skip self peer
 * @macaddr: MAC address of peer
 * @peer: pointer to the found peer
 */
struct tdls_search_progress_param {
	uint8_t skip_self;
	const uint8_t *macaddr;
	struct tdls_peer *peer;
};

/**
 * tdls_get_peer() -  find or add an TDLS peer in TDLS vdev object
 * @vdev_obj: TDLS vdev object
 * @macaddr: MAC address of peer
 *
 * Search the TDLS peer in the hash table and create a new one if not found.
 *
 * Return: Pointer to tdls_peer, NULL if failed.
 */
struct tdls_peer *tdls_get_peer(struct tdls_vdev_priv_obj *vdev_obj,
				const uint8_t *macaddr);

/**
 * tdls_find_peer() - find TDLS peer in TDLS vdev object
 * @vdev_obj: TDLS vdev object
 * @macaddr: MAC address of peer
 *
 * This is in scheduler thread context, no lock required.
 *
 * Return: If peer is found, then it returns pointer to tdls_peer;
 *         otherwise, it returns NULL.
 */
struct tdls_peer *tdls_find_peer(struct tdls_vdev_priv_obj *vdev_obj,
				 const uint8_t *macaddr);

/**
 * tdls_find_all_peer() - find peer matching the input MACaddr in soc range
 * @soc_obj: TDLS soc object
 * @macaddr: MAC address of TDLS peer
 *
 * This is in scheduler thread context, no lock required.
 *
 * Return: TDLS peer if a matching is detected; NULL otherwise
 */
struct tdls_peer *
tdls_find_all_peer(struct tdls_soc_priv_obj *soc_obj, const uint8_t *macaddr);

/**
 * tdls_find_all_peer() - find peer matching the input MACaddr in soc range
 * @soc_obj: TDLS soc object
 * @channel:channel number
 * @bw_offset: offset to bandwidth
 *
 * This is in scheduler thread context, no lock required.
 *
 * Return: Operating class
 */
uint8_t tdls_find_opclass(struct wlan_objmgr_psoc *psoc,
				 uint8_t channel,
				 uint8_t bw_offset);

/**
 * tdls_find_first_connected_peer() - find the 1st connected tdls peer from vdev
 * @vdev_obj: tdls vdev object
 *
 * This function searches for the 1st connected TDLS peer
 *
 * Return: The 1st connected TDLS peer if found; NULL otherwise
 */
struct tdls_peer *
tdls_find_first_connected_peer(struct tdls_vdev_priv_obj *vdev_obj);

/**
 * tdls_is_progress() - find the peer with ongoing TDLS progress on present psoc
 * @vdev_obj: TDLS vdev object
 * @macaddr: MAC address of the peer
 * @skip_self: if 1, skip checking self. If 0, search include self
 *
 * This is used in scheduler thread context, no lock required.
 *
 * Return: TDLS peer if found; NULL otherwise
 */
struct tdls_peer *tdls_is_progress(struct tdls_vdev_priv_obj *vdev_obj,
				   const uint8_t *macaddr, uint8_t skip_self);

/**
 * tdls_extract_peer_state_param() - extract peer update params from TDL peer
 * @peer_param: output peer update params
 * @peer: TDLS peer
 *
 * This is used when enable TDLS link
 *
 * Return: None.
 */
void tdls_extract_peer_state_param(struct tdls_peer_update_state *peer_param,
				   struct tdls_peer *peer);

/**
 * tdls_set_link_status() - set link statue for TDLS peer
 * @peer: TDLS peer
 * @link_state: link state
 * @link_reason: reason with link status
 *
 * This is in scheduler thread context, no lock required.
 *
 * Return: None.
 */
void tdls_set_peer_link_status(struct tdls_peer *peer,
			       enum tdls_link_state link_state,
			       enum tdls_link_state_reason link_reason);

/**
 * tdls_set_peer_caps() - set capability for TDLS peer
 * @vdev_obj: TDLS vdev object
 * @macaddr: MAC address for the TDLS peer
 * @req_info: parameters to update peer capability
 *
 * This is in scheduler thread context, no lock required.
 *
 * Return: None.
 */
void tdls_set_peer_caps(struct tdls_vdev_priv_obj *vdev_obj,
			const uint8_t *macaddr,
			struct tdls_update_peer_params  *req_info);

/**
 * tdls_set_valid() - set station ID on a TDLS peer
 * @vdev_obj: TDLS vdev object
 * @macaddr: MAC address of the TDLS peer
 *
 * Return: QDF_STATUS_SUCCESS if success; other values if failed
 */
QDF_STATUS tdls_set_valid(struct tdls_vdev_priv_obj *vdev_obj,
			   const uint8_t *macaddr);

/**
 * tdls_set_force_peer() - set/clear is_forced_peer flag on peer
 * @vdev_obj: TDLS vdev object
 * @macaddr: MAC address of TDLS peer
 * @forcepeer: value used to set is_forced_peer flag
 *
 * This is used in scheduler thread context, no lock required.
 *
 * Return: QDF_STATUS_SUCCESS if success; other values if failed
 */
QDF_STATUS tdls_set_force_peer(struct tdls_vdev_priv_obj *vdev_obj,
			       const uint8_t *macaddr, bool forcepeer);

/**
 * tdls_set_callback() - set state change callback on current TDLS peer
 * @peer: TDLS peer
 * @callback: state change callback
 *
 * This is used in scheduler thread context, no lock required.
 *
 * Return: QDF_STATUS_SUCCESS if success; other values if failed
 */
QDF_STATUS tdls_set_callback(struct tdls_peer *peer,
			     tdls_state_change_callback callback);

/**
 * tdls_set_extctrl_param() - set external control parameter on TDLS peer
 * @peer: TDLS peer
 * @chan: channel
 * @max_latency: maximum latency
 * @op_class: operation class
 * @min_bandwidth: minimal bandwidth
 *
 * This is used in scheduler thread context, no lock required.
 *
 * Return: QDF_STATUS_SUCCESS if success; other values if failed
 */
QDF_STATUS tdls_set_extctrl_param(struct tdls_peer *peer, uint32_t chan,
				  uint32_t max_latency, uint32_t op_class,
				  uint32_t min_bandwidth);

/**
 * tdls_reset_peer() - reset TDLS peer identified by MAC address
 * @vdev_obj: TDLS vdev object
 * @mac: MAC address of the peer
 *
 * Return: QDF_STATUS_SUCCESS if success; other values if failed
 */
QDF_STATUS tdls_reset_peer(struct tdls_vdev_priv_obj *vdev_obj,
			   const uint8_t *mac);

/**
 * tdls_peer_idle_timers_destroy() - destroy peer idle timers
 * @vdev_obj: TDLS vdev object
 *
 * Loop through the idle peer list and destroy their timers
 *
 * Return: None
 */
void tdls_peer_idle_timers_destroy(struct tdls_vdev_priv_obj *vdev_obj);

/**
 * tdls_free_peer_list() - free TDLS peer list
 * @vdev_obj: TDLS vdev object
 *
 * Free all the tdls peers
 *
 * Return: None
 */
void tdls_free_peer_list(struct tdls_vdev_priv_obj *vdev_obj);
#endif
