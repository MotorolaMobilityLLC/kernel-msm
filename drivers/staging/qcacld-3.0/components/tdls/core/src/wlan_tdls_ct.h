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
 * DOC: wlan_tdls_ct.h
 *
 * TDLS connection tracker declarations
 */

#ifndef _WLAN_TDLS_CT_H_
#define _WLAN_TDLS_CT_H_

 /*
  * Before UpdateTimer expires, we want to timeout discovery response
  * should not be more than 2000.
  */
#define TDLS_DISCOVERY_TIMEOUT_ERE_UPDATE     1000

#define TDLS_PREFERRED_OFF_CHANNEL_NUM_MIN      1
#define TDLS_PREFERRED_OFF_CHANNEL_NUM_MAX      165
#define TDLS_PREFERRED_OFF_CHANNEL_NUM_DEFAULT  36

/**
 * tdls_implicit_enable() - enable implicit tdls triggering
 * @tdls_vdev: TDLS vdev
 *
 * Return: Void
 */
void tdls_implicit_enable(struct tdls_vdev_priv_obj *tdls_vdev);

/**
 * tdls_update_rx_pkt_cnt() - Update rx packet count
 * @vdev: vdev object manager
 * @mac_addr: mac address of the data
 * @dest_mac_addr: dest mac address of the data
 *
 * Increase the rx packet count, if the sender is not bssid and the packet is
 * not broadcast and multicast packet
 *
 * This sampling information will be used in TDLS connection tracker
 *
 * This function expected to be called in an atomic context so blocking APIs
 * not allowed
 *
 * Return: None
 */
void tdls_update_rx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				     struct qdf_mac_addr *mac_addr,
				     struct qdf_mac_addr *dest_mac_addr);

/**
 * tdls_update_tx_pkt_cnt() - update tx packet
 * @vdev: vdev object
 * @mac_addr: mac address of the data
 *
 * Increase the tx packet count, if the sender is not bssid and the packet is
 * not broadcast and multicast packet
 *
 * This sampling information will be used in TDLS connection tracker
 *
 * This function expected to be called in an atomic context so blocking APIs
 * not allowed
 *
 * Return: None
 */
void tdls_update_tx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				     struct qdf_mac_addr *mac_addr);

/**
 * wlan_hdd_tdls_implicit_send_discovery_request() - send discovery request
 * @tdls_vdev_obj: tdls vdev object
 *
 * Return: None
 */
void tdls_implicit_send_discovery_request(
				struct tdls_vdev_priv_obj *tdls_vdev_obj);

/**
 * tdls_recv_discovery_resp() - handling of tdls discovery response
 * @soc: object manager
 * @mac: mac address of peer from which the response was received
 *
 * Return: 0 for success or negative errno otherwise
 */
int tdls_recv_discovery_resp(struct tdls_vdev_priv_obj *tdls_vdev,
				   const uint8_t *mac);

/**
 * tdls_indicate_teardown() - indicate teardown to upper layer
 * @tdls_vdev: tdls vdev object
 * @curr_peer: teardown peer
 * @reason: teardown reason
 *
 * Return: Void
 */
void tdls_indicate_teardown(struct tdls_vdev_priv_obj *tdls_vdev,
				struct tdls_peer *curr_peer,
				uint16_t reason);

/**
 * tdls_ct_handler() - TDLS connection tracker handler
 * @user_data: user data from timer
 *
 * tdls connection tracker timer starts, when the STA connected to AP
 * and it's scan the traffic between two STA peers and make TDLS
 * connection and teardown, based on the traffic threshold
 *
 * Return: None
 */
void tdls_ct_handler(void *user_data);

/**
 * tdls_ct_idle_handler() - Check tdls idle traffic
 * @user_data: data from tdls idle timer
 *
 * Function to check the tdls idle traffic and make a decision about
 * tdls teardown
 *
 * Return: None
 */
void tdls_ct_idle_handler(void *user_data);

/**
 * tdls_discovery_timeout_peer_cb() - tdls discovery timeout callback
 * @userData: tdls vdev
 *
 * Return: None
 */
void tdls_discovery_timeout_peer_cb(void *user_data);

/**
 * tdls_implicit_disable() - disable implicit tdls triggering
 * @pHddTdlsCtx: TDLS context
 *
 * Return: Void
 */
void tdls_implicit_disable(struct tdls_vdev_priv_obj *tdls_vdev);

/**
 * tdls_is_vdev_authenticated() - check the vdev authentication state
 * @vdev: vdev oobject
 *
 * Return: true or false
 */
bool tdls_is_vdev_authenticated(struct wlan_objmgr_vdev *vdev);

/**
 * tdls_teardown_connections() - teardown and delete all the tdls peers
 * @tdls_teardown: tdls teardown struct
 *
 * Return: true or false
 */
void tdls_teardown_connections(struct tdls_link_teardown *tdls_teardown);

/**
 * tdls_disable_offchan_and_teardown_links - Disable offchannel
 * and teardown TDLS links
 * @tdls_soc : tdls soc object
 *
 * Return: None
 */
void tdls_disable_offchan_and_teardown_links(
				struct wlan_objmgr_vdev *vdev);

/**
 * tdls_delete_all_tdls_peers() - send request to delete tdls peers
 * @vdev: vdev object
 * @tdls_soc: tdls soc object
 *
 * This function sends request to lim to delete tdls peers
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_delete_all_tdls_peers(struct wlan_objmgr_vdev *vdev,
					  struct tdls_soc_priv_obj *tdls_soc);

/**
 * tdls_set_tdls_offchannel() - set tdls off-channel number
 * @tdls_soc: tdls soc object
 * @offchannel: tdls off-channel number
 *
 * This function sets tdls off-channel number
 *
 * Return: 0 on success; negative errno otherwise
 */
int tdls_set_tdls_offchannel(struct tdls_soc_priv_obj *tdls_soc,
			     int offchannel);

/**
 * tdls_set_tdls_offchannelmode() - set tdls off-channel mode
 * @adapter: Pointer to the HDD adapter
 * @offchanmode: tdls off-channel mode
 *
 * This function sets tdls off-channel mode
 *
 * Return: 0 on success; negative errno otherwise
 */

int tdls_set_tdls_offchannelmode(struct wlan_objmgr_vdev *vdev,
				 int offchanmode);

/**
 * tdls_set_tdls_secoffchanneloffset() - set secondary tdls off-channel offset
 * @tdls_soc: tdls soc object
 * @offchanoffset: tdls off-channel offset
 *
 * This function sets secondary tdls off-channel offset
 *
 * Return: 0 on success; negative errno otherwise
 */

int tdls_set_tdls_secoffchanneloffset(struct tdls_soc_priv_obj *tdls_soc,
				      int offchanoffset);

#endif
