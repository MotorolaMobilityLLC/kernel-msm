/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_tdls_ct.c
 *
 * TDLS connection tracker function definitions
 */

#include "wlan_tdls_main.h"
#include "wlan_tdls_peer.h"
#include "wlan_tdls_ct.h"
#include "wlan_tdls_cmds_process.h"

bool tdls_is_vdev_authenticated(struct wlan_objmgr_vdev *vdev)
{
	struct wlan_objmgr_peer *peer;
	bool is_authenticated = false;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_TDLS_NB_ID);
	if (!peer) {
		tdls_err("peer is null");
		return false;
	}

	is_authenticated = wlan_peer_mlme_get_auth_state(peer);
	wlan_objmgr_peer_release_ref(peer, WLAN_TDLS_NB_ID);
	return is_authenticated;
}

/**
 * tdls_peer_reset_discovery_processed() - reset discovery status
 * @tdls_vdev: TDLS vdev object
 *
 * This function resets discovery processing bit for all TDLS peers
 *
 * Caller has to take the lock before calling this function
 *
 * Return: 0
 */
static int32_t tdls_peer_reset_discovery_processed(
					struct tdls_vdev_priv_obj *tdls_vdev)
{
	int i;
	qdf_list_t *head;
	qdf_list_node_t *p_node;
	struct tdls_peer *peer;
	QDF_STATUS status;

	tdls_vdev->discovery_peer_cnt = 0;

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		head = &tdls_vdev->peer_list[i];
		status = qdf_list_peek_front(head, &p_node);
		while (QDF_IS_STATUS_SUCCESS(status)) {
			peer = qdf_container_of(p_node, struct tdls_peer, node);
			peer->discovery_processed = 0;
			status = qdf_list_peek_next(head, p_node, &p_node);
		}
	}

	return 0;
}

void tdls_discovery_timeout_peer_cb(void *user_data)
{
	int i;
	qdf_list_t *head;
	qdf_list_node_t *p_node;
	struct tdls_peer *peer;
	QDF_STATUS status;
	struct tdls_vdev_priv_obj *tdls_vdev;
	struct wlan_objmgr_vdev *vdev;

	if (!user_data) {
		tdls_err("discovery time out data is null");
		return;
	}

	vdev = tdls_get_vdev(user_data, WLAN_TDLS_NB_ID);
	if (!vdev)
		return;
	tdls_vdev = wlan_vdev_get_tdls_vdev_obj(vdev);

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		head = &tdls_vdev->peer_list[i];
		status = qdf_list_peek_front(head, &p_node);
		while (QDF_IS_STATUS_SUCCESS(status)) {
			peer = qdf_container_of(p_node, struct tdls_peer,
						node);
			if (TDLS_LINK_DISCOVERING != peer->link_status) {
				status = qdf_list_peek_next(head, p_node,
							    &p_node);
				continue;
			}
			tdls_debug(QDF_MAC_ADDR_FMT " to idle state",
				   QDF_MAC_ADDR_REF(peer->peer_mac.bytes));
			tdls_set_peer_link_status(peer,
						  TDLS_LINK_IDLE,
						  TDLS_LINK_NOT_SUPPORTED);
		}
	}
	tdls_vdev->discovery_sent_cnt = 0;
	wlan_objmgr_vdev_release_ref(vdev, WLAN_TDLS_NB_ID);

	/* add tdls power save prohibited */

	return;
}

/**
 * tdls_reset_tx_rx() - reset tx/rx counters for all tdls peers
 * @tdls_vdev: TDLS vdev object
 *
 * Caller has to take the TDLS lock before calling this function
 *
 * Return: Void
 */
static void tdls_reset_tx_rx(struct tdls_vdev_priv_obj *tdls_vdev)
{
	int i;
	qdf_list_t *head;
	qdf_list_node_t *p_node;
	struct tdls_peer *peer;
	QDF_STATUS status;

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		head = &tdls_vdev->peer_list[i];
		status = qdf_list_peek_front(head, &p_node);
		while (QDF_IS_STATUS_SUCCESS(status)) {
			peer = qdf_container_of(p_node, struct tdls_peer, node);
			peer->tx_pkt = 0;
			peer->rx_pkt = 0;
			status = qdf_list_peek_next(head, p_node, &p_node);
		}
	}
	return;
}

void tdls_implicit_disable(struct tdls_vdev_priv_obj *tdls_vdev)
{
	tdls_debug("Disable Implicit TDLS");
	tdls_timers_stop(tdls_vdev);
}

/**
 * tdls_implicit_enable() - enable implicit tdls triggering
 * @tdls_vdev: TDLS vdev
 *
 * Return: Void
 */
void tdls_implicit_enable(struct tdls_vdev_priv_obj *tdls_vdev)
{
	tdls_debug("Enable Implicit TDLS");
	if (!tdls_vdev)
		return;

	tdls_peer_reset_discovery_processed(tdls_vdev);
	tdls_reset_tx_rx(tdls_vdev);
	/* TODO check whether tdls power save prohibited */

	/* Restart the connection tracker timer */
	tdls_timer_restart(tdls_vdev->vdev, &tdls_vdev->peer_update_timer,
			   tdls_vdev->threshold_config.tx_period_t);
}

/**
 * tdls_ct_sampling_tx_rx() - collect tx/rx traffic sample
 * @tdls_vdev_obj: tdls vdev object
 * @tdls_soc_obj: tdls soc object
 *
 * Function to update data traffic information in tdls connection
 * tracker data structure for connection tracker operation
 *
 * Return: None
 */
static void tdls_ct_sampling_tx_rx(struct tdls_vdev_priv_obj *tdls_vdev,
				   struct tdls_soc_priv_obj *tdls_soc)
{
	struct tdls_peer *curr_peer;
	uint8_t mac[QDF_MAC_ADDR_SIZE];
	uint8_t mac_cnt;
	uint8_t mac_entries;
	struct tdls_conn_tracker_mac_table mac_table[WLAN_TDLS_CT_TABLE_SIZE];

	qdf_spin_lock_bh(&tdls_soc->tdls_ct_spinlock);

	if (0 == tdls_vdev->valid_mac_entries) {
		qdf_spin_unlock_bh(&tdls_soc->tdls_ct_spinlock);
		return;
	}

	mac_entries = QDF_MIN(tdls_vdev->valid_mac_entries,
			      WLAN_TDLS_CT_TABLE_SIZE);

	qdf_mem_copy(mac_table, tdls_vdev->ct_peer_table,
	       (sizeof(struct tdls_conn_tracker_mac_table)) * mac_entries);

	qdf_mem_zero(tdls_vdev->ct_peer_table,
	       (sizeof(struct tdls_conn_tracker_mac_table)) * mac_entries);

	tdls_vdev->valid_mac_entries = 0;

	qdf_spin_unlock_bh(&tdls_soc->tdls_ct_spinlock);

	for (mac_cnt = 0; mac_cnt < mac_entries; mac_cnt++) {
		qdf_mem_copy(mac, mac_table[mac_cnt].mac_address.bytes,
		       QDF_MAC_ADDR_SIZE);
		curr_peer = tdls_get_peer(tdls_vdev, mac);
		if (curr_peer) {
			curr_peer->tx_pkt =
			mac_table[mac_cnt].tx_packet_cnt;
			curr_peer->rx_pkt =
			mac_table[mac_cnt].rx_packet_cnt;
		}
	}
}

void tdls_update_rx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *mac_addr,
				 struct qdf_mac_addr *dest_mac_addr)
{
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	uint8_t mac_cnt;
	uint8_t valid_mac_entries;
	struct tdls_conn_tracker_mac_table *mac_table;
	struct wlan_objmgr_peer *bss_peer;

	if (QDF_STATUS_SUCCESS != tdls_get_vdev_objects(vdev, &tdls_vdev_obj,
							&tdls_soc_obj))
		return;

	if (!tdls_soc_obj->enable_tdls_connection_tracker)
		return;

	/* Here we do without lock to ensure that in high throughput scenarios
	 * its fast and we quickly check the right mac entry and increment
	 * the pkt count. Here it may happen that
	 * "tdls_vdev_obj->valid_mac_entries", "tdls_vdev_obj->ct_peer_table"
	 * becomes zero in another thread but we are ok as this will not
	 * lead to any crash.
	 */
	valid_mac_entries = tdls_vdev_obj->valid_mac_entries;
	mac_table = tdls_vdev_obj->ct_peer_table;

	for (mac_cnt = 0; mac_cnt < valid_mac_entries; mac_cnt++) {
		if (qdf_mem_cmp(mac_table[mac_cnt].mac_address.bytes,
		    mac_addr, QDF_MAC_ADDR_SIZE) == 0) {
			mac_table[mac_cnt].rx_packet_cnt++;
			return;
		}
	}

	if (qdf_is_macaddr_group(mac_addr))
		return;

	if (qdf_is_macaddr_group(dest_mac_addr))
		return;

	if (!qdf_mem_cmp(vdev->vdev_mlme.macaddr, mac_addr,
			 QDF_MAC_ADDR_SIZE))
		return;

	bss_peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_TDLS_NB_ID);
	if (bss_peer) {
		if (!qdf_mem_cmp(bss_peer->macaddr, mac_addr,
				 QDF_MAC_ADDR_SIZE)) {
			wlan_objmgr_peer_release_ref(bss_peer, WLAN_TDLS_NB_ID);
			return;
		}
		wlan_objmgr_peer_release_ref(bss_peer, WLAN_TDLS_NB_ID);
	}
	qdf_spin_lock_bh(&tdls_soc_obj->tdls_ct_spinlock);

	/* when we take the lock we need to get the valid mac entries
	 * again as it may become zero in another thread and if is 0 then
	 * we need to reset "mac_cnt" to zero so that at zeroth index we
	 * add new entry
	 */
	valid_mac_entries = tdls_vdev_obj->valid_mac_entries;
	if (!valid_mac_entries)
		mac_cnt = 0;

	/* If we have more than 8 peers within 30 mins. we will
	 *  stop tracking till the old entries are removed
	 */
	if (mac_cnt < WLAN_TDLS_CT_TABLE_SIZE) {
		qdf_mem_copy(mac_table[mac_cnt].mac_address.bytes,
		       mac_addr, QDF_MAC_ADDR_SIZE);
		tdls_vdev_obj->valid_mac_entries = mac_cnt+1;
		mac_table[mac_cnt].rx_packet_cnt = 1;
	}

	qdf_spin_unlock_bh(&tdls_soc_obj->tdls_ct_spinlock);
	return;
}

void tdls_update_tx_pkt_cnt(struct wlan_objmgr_vdev *vdev,
			    struct qdf_mac_addr *mac_addr)
{
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	uint8_t mac_cnt;
	uint8_t valid_mac_entries;
	struct tdls_conn_tracker_mac_table *mac_table;
	struct wlan_objmgr_peer *bss_peer;

	if (QDF_STATUS_SUCCESS != tdls_get_vdev_objects(vdev, &tdls_vdev_obj,
							&tdls_soc_obj))
		return;

	if (!tdls_soc_obj->enable_tdls_connection_tracker)
		return;

	/* Here we do without lock to ensure that in high throughput scenarios
	 * its fast and we quickly check the right mac entry and increment
	 * the pkt count. Here it may happen that
	 * "tdls_vdev_obj->valid_mac_entries", "tdls_vdev_obj->ct_peer_table"
	 * becomes zero in another thread but we are ok as this will not
	 * lead to any crash.
	 */
	mac_table = tdls_vdev_obj->ct_peer_table;
	valid_mac_entries = tdls_vdev_obj->valid_mac_entries;

	for (mac_cnt = 0; mac_cnt < valid_mac_entries; mac_cnt++) {
		if (qdf_mem_cmp(mac_table[mac_cnt].mac_address.bytes,
		    mac_addr, QDF_MAC_ADDR_SIZE) == 0) {
			mac_table[mac_cnt].tx_packet_cnt++;
			return;
		}
	}

	if (qdf_is_macaddr_group(mac_addr))
		return;

	if (!qdf_mem_cmp(vdev->vdev_mlme.macaddr, mac_addr,
			 QDF_MAC_ADDR_SIZE))
		return;
	bss_peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_TDLS_NB_ID);
	if (bss_peer) {
		if (!qdf_mem_cmp(bss_peer->macaddr, mac_addr,
				 QDF_MAC_ADDR_SIZE)) {
			wlan_objmgr_peer_release_ref(bss_peer, WLAN_TDLS_NB_ID);
			return;
		}
		wlan_objmgr_peer_release_ref(bss_peer, WLAN_TDLS_NB_ID);
	}

	qdf_spin_lock_bh(&tdls_soc_obj->tdls_ct_spinlock);

	/* when we take the lock we need to get the valid mac entries
	 * again as it may become zero in another thread and if is 0 then
	 * we need to reset "mac_cnt" to zero so that at zeroth index we
	 * add new entry
	 */
	valid_mac_entries = tdls_vdev_obj->valid_mac_entries;
	if (!valid_mac_entries)
		mac_cnt = 0;

	/* If we have more than 8 peers within 30 mins. we will
	 *  stop tracking till the old entries are removed
	 */
	if (mac_cnt < WLAN_TDLS_CT_TABLE_SIZE) {
		qdf_mem_copy(mac_table[mac_cnt].mac_address.bytes,
			mac_addr, QDF_MAC_ADDR_SIZE);
		mac_table[mac_cnt].tx_packet_cnt = 1;
		tdls_vdev_obj->valid_mac_entries++;
	}

	qdf_spin_unlock_bh(&tdls_soc_obj->tdls_ct_spinlock);
	return;
}

void tdls_implicit_send_discovery_request(
				struct tdls_vdev_priv_obj *tdls_vdev_obj)
{
	struct tdls_peer *curr_peer;
	struct tdls_peer *temp_peer;
	struct tdls_soc_priv_obj *tdls_psoc;
	struct tdls_osif_indication tdls_ind;

	if (!tdls_vdev_obj) {
		tdls_notice("tdls_vdev_obj is NULL");
		return;
	}

	tdls_psoc = wlan_vdev_get_tdls_soc_obj(tdls_vdev_obj->vdev);

	if (!tdls_psoc) {
		tdls_notice("tdls_psoc_obj is NULL");
		return;
	}

	curr_peer = tdls_vdev_obj->curr_candidate;

	if (!curr_peer) {
		tdls_err("curr_peer is NULL");
		return;
	}

	/* This function is called in mutex_lock */
	temp_peer = tdls_is_progress(tdls_vdev_obj, NULL, 0);
	if (temp_peer) {
		tdls_notice(QDF_MAC_ADDR_FMT " ongoing. pre_setup ignored",
			    QDF_MAC_ADDR_REF(temp_peer->peer_mac.bytes));
		goto done;
	}

	if (TDLS_CAP_UNKNOWN != curr_peer->tdls_support)
		tdls_set_peer_link_status(curr_peer,
					  TDLS_LINK_DISCOVERING,
					  TDLS_LINK_SUCCESS);

	qdf_mem_copy(tdls_ind.peer_mac, curr_peer->peer_mac.bytes,
			QDF_MAC_ADDR_SIZE);

	tdls_ind.vdev = tdls_vdev_obj->vdev;

	tdls_debug("Implicit TDLS, Send Discovery request event");

	tdls_psoc->tdls_event_cb(tdls_psoc->tdls_evt_cb_data,
				 TDLS_EVENT_DISCOVERY_REQ, &tdls_ind);

	tdls_vdev_obj->discovery_sent_cnt++;

	tdls_timer_restart(tdls_vdev_obj->vdev,
				&tdls_vdev_obj->peer_discovery_timer,
				tdls_vdev_obj->threshold_config.tx_period_t -
				TDLS_DISCOVERY_TIMEOUT_ERE_UPDATE);

	tdls_debug("discovery count %u timeout %u msec",
		 tdls_vdev_obj->discovery_sent_cnt,
		 tdls_vdev_obj->threshold_config.tx_period_t -
		 TDLS_DISCOVERY_TIMEOUT_ERE_UPDATE);
done:
	tdls_vdev_obj->curr_candidate = NULL;
	tdls_vdev_obj->magic = 0;
	return;
}

int tdls_recv_discovery_resp(struct tdls_vdev_priv_obj *tdls_vdev,
				   const uint8_t *mac)
{
	struct tdls_peer *curr_peer;
	struct tdls_soc_priv_obj *tdls_soc;
	struct tdls_osif_indication indication;
	struct tdls_config_params *tdls_cfg;
	int status = 0;

	if (!tdls_vdev)
		return -EINVAL;

	tdls_soc = wlan_vdev_get_tdls_soc_obj(tdls_vdev->vdev);
	if (!tdls_soc) {
		tdls_err("tdls soc is NULL");
		return -EINVAL;
	}

	curr_peer = tdls_get_peer(tdls_vdev, mac);
	if (!curr_peer) {
		tdls_err("curr_peer is NULL");
		return -EINVAL;
	}

	if (tdls_vdev->discovery_sent_cnt)
		tdls_vdev->discovery_sent_cnt--;

	if (0 == tdls_vdev->discovery_sent_cnt)
		qdf_mc_timer_stop(&tdls_vdev->peer_discovery_timer);

	tdls_debug("Discovery(%u) Response from " QDF_MAC_ADDR_FMT
		   " link_status %d", tdls_vdev->discovery_sent_cnt,
		   QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes),
		   curr_peer->link_status);

	tdls_cfg = &tdls_vdev->threshold_config;
	if (TDLS_LINK_DISCOVERING == curr_peer->link_status) {
		/* Since we are here, it means Throughput threshold is
		 * already met. Make sure RSSI threshold is also met
		 * before setting up TDLS link.
		 */
		if ((int32_t) curr_peer->rssi >
		    (int32_t) tdls_cfg->rssi_trigger_threshold) {
			tdls_set_peer_link_status(curr_peer,
						TDLS_LINK_DISCOVERED,
						TDLS_LINK_SUCCESS);
			tdls_debug("Rssi Threshold met: " QDF_MAC_ADDR_FMT
				" rssi = %d threshold= %d",
				QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes),
				curr_peer->rssi,
				tdls_cfg->rssi_trigger_threshold);

			qdf_mem_copy(indication.peer_mac, mac,
					QDF_MAC_ADDR_SIZE);

			indication.vdev = tdls_vdev->vdev;

			tdls_soc->tdls_event_cb(tdls_soc->tdls_evt_cb_data,
						TDLS_EVENT_SETUP_REQ,
						&indication);
		} else {
			tdls_debug("Rssi Threshold not met: " QDF_MAC_ADDR_FMT
				" rssi = %d threshold = %d ",
				QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes),
				curr_peer->rssi,
				tdls_cfg->rssi_trigger_threshold);

			tdls_set_peer_link_status(curr_peer,
						TDLS_LINK_IDLE,
						TDLS_LINK_UNSPECIFIED);

			/* if RSSI threshold is not met then allow
			 * further discovery attempts by decrementing
			 * count for the last attempt
			 */
			if (curr_peer->discovery_attempt)
				curr_peer->discovery_attempt--;
		}
	}

	curr_peer->tdls_support = TDLS_CAP_SUPPORTED;

	return status;
}

void tdls_indicate_teardown(struct tdls_vdev_priv_obj *tdls_vdev,
			    struct tdls_peer *curr_peer,
			    uint16_t reason)
{
	struct tdls_soc_priv_obj *tdls_soc;
	struct tdls_osif_indication indication;

	if (!tdls_vdev || !curr_peer) {
		tdls_err("tdls_vdev: %pK, curr_peer: %pK",
			 tdls_vdev, curr_peer);
		return;
	}

	tdls_soc = wlan_vdev_get_tdls_soc_obj(tdls_vdev->vdev);
	if (!tdls_soc) {
		tdls_err("tdls_soc: %pK", tdls_soc);
		return;
	}

	if (curr_peer->link_status != TDLS_LINK_CONNECTED) {
		tdls_err("link state %d peer:" QDF_MAC_ADDR_FMT,
			 curr_peer->link_status,
			 QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes));
		return;
	}

	tdls_set_peer_link_status(curr_peer,
				  TDLS_LINK_TEARING,
				  TDLS_LINK_UNSPECIFIED);
	tdls_notice("Teardown reason %d", reason);

	if (tdls_soc->tdls_dp_vdev_update)
		tdls_soc->tdls_dp_vdev_update(
				&tdls_soc->soc,
				wlan_vdev_get_id(tdls_vdev->vdev),
				tdls_soc->tdls_update_dp_vdev_flags,
				false);

	indication.reason = reason;
	indication.vdev = tdls_vdev->vdev;
	qdf_mem_copy(indication.peer_mac, curr_peer->peer_mac.bytes,
			QDF_MAC_ADDR_SIZE);

	if (tdls_soc->tdls_event_cb)
		tdls_soc->tdls_event_cb(tdls_soc->tdls_evt_cb_data,
				     TDLS_EVENT_TEARDOWN_REQ, &indication);
}

/**
 * tdls_get_conn_info() - get the tdls connection information.
 * @tdls_soc: tdls soc object
 * @peer_mac: peer MAC address
 *
 * Function to check tdls sta index
 *
 * Return: tdls connection information
 */
static struct tdls_conn_info *
tdls_get_conn_info(struct tdls_soc_priv_obj *tdls_soc,
		   struct qdf_mac_addr *peer_mac)
{
	uint8_t sta_idx;
	/* check if there is available index for this new TDLS STA */
	for (sta_idx = 0; sta_idx < WLAN_TDLS_STA_MAX_NUM; sta_idx++) {
		if (!qdf_mem_cmp(
			    tdls_soc->tdls_conn_info[sta_idx].peer_mac.bytes,
			    peer_mac->bytes, QDF_MAC_ADDR_SIZE)) {
			tdls_debug("tdls peer exists idx %d " QDF_MAC_ADDR_FMT,
				   sta_idx,
				   QDF_MAC_ADDR_REF(peer_mac->bytes));
			tdls_soc->tdls_conn_info[sta_idx].index = sta_idx;
			return &tdls_soc->tdls_conn_info[sta_idx];
		}
	}

	tdls_err("tdls peer does not exists");
	return NULL;
}

static void
tdls_ct_process_idle_handler(struct wlan_objmgr_vdev *vdev,
			     struct tdls_conn_info *tdls_info)
{
	struct tdls_peer *curr_peer;
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct tdls_soc_priv_obj *tdls_soc_obj;

	if (QDF_STATUS_SUCCESS != tdls_get_vdev_objects(vdev, &tdls_vdev_obj,
						   &tdls_soc_obj))
		return;

	if (!tdls_info->valid_entry) {
		tdls_err("peer doesn't exists");
		return;
	}

	curr_peer = tdls_find_peer(tdls_vdev_obj,
		(u8 *) &tdls_info->peer_mac.bytes[0]);

	if (!curr_peer) {
		tdls_err("Invalid tdls idle timer expired");
		return;
	}

	tdls_debug(QDF_MAC_ADDR_FMT
		" tx_pkt: %d, rx_pkt: %d, idle_packet_n: %d",
		QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes),
		curr_peer->tx_pkt,
		curr_peer->rx_pkt,
		tdls_vdev_obj->threshold_config.idle_packet_n);

	/* Check tx/rx statistics on this tdls link for recent activities and
	 * then decide whether to tear down the link or keep it.
	 */
	if ((curr_peer->tx_pkt >=
	     tdls_vdev_obj->threshold_config.idle_packet_n) ||
	    (curr_peer->rx_pkt >=
	     tdls_vdev_obj->threshold_config.idle_packet_n)) {
		/* this tdls link got back to normal, so keep it */
		tdls_debug("tdls link to " QDF_MAC_ADDR_FMT
			 " back to normal, will stay",
			  QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes));
	} else {
		/* this tdls link needs to get torn down */
		tdls_notice("trigger tdls link to "QDF_MAC_ADDR_FMT" down",
			    QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes));
		tdls_indicate_teardown(tdls_vdev_obj,
					curr_peer,
					TDLS_TEARDOWN_PEER_UNSPEC_REASON);
	}

	return;
}

void tdls_ct_idle_handler(void *user_data)
{
	struct wlan_objmgr_vdev *vdev;
	struct tdls_conn_info *tdls_info;
	struct tdls_soc_priv_obj *tdls_soc_obj;
	uint32_t idx;

	tdls_info = (struct tdls_conn_info *)user_data;
	if (!tdls_info)
		return;

	idx = tdls_info->index;
	if (idx == INVALID_TDLS_PEER_INDEX || idx >= WLAN_TDLS_STA_MAX_NUM) {
		tdls_debug("invalid peer index %d" QDF_MAC_ADDR_FMT, idx,
			  QDF_MAC_ADDR_REF(tdls_info->peer_mac.bytes));
		return;
	}

	tdls_soc_obj = qdf_container_of(tdls_info, struct tdls_soc_priv_obj,
					tdls_conn_info[idx]);

	vdev = tdls_get_vdev(tdls_soc_obj->soc, WLAN_TDLS_NB_ID);
	if (!vdev) {
		tdls_err("Unable to fetch the vdev");
		return;
	}

	tdls_ct_process_idle_handler(vdev, tdls_info);
	wlan_objmgr_vdev_release_ref(vdev,
				     WLAN_TDLS_NB_ID);
}

/**
 * tdls_ct_process_idle_and_discovery() - process the traffic data
 * @curr_peer: tdls peer needs to be examined
 * @tdls_vdev_obj: tdls vdev object
 * @tdls_soc_obj: tdls soc object
 *
 * Function to check the peer traffic data in idle link and  tdls
 * discovering link
 *
 * Return: None
 */
static void
tdls_ct_process_idle_and_discovery(struct tdls_peer *curr_peer,
				struct tdls_vdev_priv_obj *tdls_vdev_obj,
				struct tdls_soc_priv_obj *tdls_soc_obj)
{
	uint16_t valid_peers;

	valid_peers = tdls_soc_obj->connected_peer_count;

	if ((curr_peer->tx_pkt + curr_peer->rx_pkt) >=
	     tdls_vdev_obj->threshold_config.tx_packet_n) {
		if (WLAN_TDLS_STA_MAX_NUM > valid_peers) {
			tdls_notice("Tput trigger TDLS pre-setup");
			tdls_vdev_obj->curr_candidate = curr_peer;
			tdls_implicit_send_discovery_request(tdls_vdev_obj);
		} else {
			tdls_notice("Maximum peers connected already! %d",
				 valid_peers);
		}
	}
}

/**
 * tdls_ct_process_connected_link() - process the traffic
 * @curr_peer: tdls peer needs to be examined
 * @tdls_vdev_obj: tdls vdev
 * @tdls_soc_obj: tdls soc context
 *
 * Function to check the peer traffic data in active STA
 * session
 *
 * Return: None
 */
static void tdls_ct_process_connected_link(
				struct tdls_peer *curr_peer,
				struct tdls_vdev_priv_obj *tdls_vdev,
				struct tdls_soc_priv_obj *tdls_soc)
{
	/* Don't trigger low rssi tear down here since FW will do it */
	/* Only teardown based on non zero idle packet threshold, to address
	 * a use case where this threshold does not get consider for TEAR DOWN
	 */
	if ((0 != tdls_vdev->threshold_config.idle_packet_n) &&
	    ((curr_peer->tx_pkt <
	      tdls_vdev->threshold_config.idle_packet_n) &&
	     (curr_peer->rx_pkt <
	      tdls_vdev->threshold_config.idle_packet_n))) {
		if (!curr_peer->is_peer_idle_timer_initialised) {
			struct tdls_conn_info *tdls_info;
			tdls_info = tdls_get_conn_info(tdls_soc,
						       &curr_peer->peer_mac);
			qdf_mc_timer_init(&curr_peer->peer_idle_timer,
					  QDF_TIMER_TYPE_SW,
					  tdls_ct_idle_handler,
					  (void *)tdls_info);
			curr_peer->is_peer_idle_timer_initialised = true;
		}
		if (QDF_TIMER_STATE_RUNNING !=
		    curr_peer->peer_idle_timer.state) {
			tdls_warn("Tx/Rx Idle timer start: "
				QDF_MAC_ADDR_FMT "!",
				QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes));
			tdls_timer_restart(tdls_vdev->vdev,
				&curr_peer->peer_idle_timer,
				tdls_vdev->threshold_config.idle_timeout_t);
		}
	} else if (QDF_TIMER_STATE_RUNNING ==
		   curr_peer->peer_idle_timer.state) {
		tdls_warn("Tx/Rx Idle timer stop: " QDF_MAC_ADDR_FMT "!",
			 QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes));
		qdf_mc_timer_stop(&curr_peer->peer_idle_timer);
	}
}

/**
 * tdls_ct_process_cap_supported() - process TDLS supported peer.
 * @curr_peer: tdls peer needs to be examined
 * @tdls_vdev_obj: tdls vdev context
 * @tdls_soc_obj: tdls soc context
 *
 * Function to check the peer traffic data  for tdls supported peer
 *
 * Return: None
 */
static void tdls_ct_process_cap_supported(struct tdls_peer *curr_peer,
					struct tdls_vdev_priv_obj *tdls_vdev,
					struct tdls_soc_priv_obj *tdls_soc_obj)
{
	if (curr_peer->rx_pkt || curr_peer->tx_pkt)
		tdls_debug(QDF_MAC_ADDR_FMT "link_status %d tdls_support %d tx %d rx %d rssi %d",
			   QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes),
			   curr_peer->link_status, curr_peer->tdls_support,
			   curr_peer->tx_pkt, curr_peer->rx_pkt,
			   curr_peer->rssi);

	switch (curr_peer->link_status) {
	case TDLS_LINK_IDLE:
	case TDLS_LINK_DISCOVERING:
		if (TDLS_IS_EXTERNAL_CONTROL_ENABLED(
			tdls_soc_obj->tdls_configs.tdls_feature_flags) &&
			(!curr_peer->is_forced_peer))
			break;
		tdls_ct_process_idle_and_discovery(curr_peer, tdls_vdev,
						   tdls_soc_obj);
		break;
	case TDLS_LINK_CONNECTED:
		tdls_ct_process_connected_link(curr_peer, tdls_vdev,
					       tdls_soc_obj);
		break;
	default:
		break;
	}
}

/**
 * tdls_ct_process_cap_unknown() - process unknown peer
 * @curr_peer: tdls peer needs to be examined
 * @tdls_vdev_obj: tdls vdev object
 * @tdls_soc_obj: tdls soc object
 *
 * Function check the peer traffic data , when tdls capability is unknown
 *
 * Return: None
 */
static void tdls_ct_process_cap_unknown(struct tdls_peer *curr_peer,
					struct tdls_vdev_priv_obj *tdls_vdev,
					struct tdls_soc_priv_obj *tdlsa_soc)
{
	if (TDLS_IS_EXTERNAL_CONTROL_ENABLED(
			tdlsa_soc->tdls_configs.tdls_feature_flags) &&
			(!curr_peer->is_forced_peer))
			return;

	if (curr_peer->rx_pkt || curr_peer->tx_pkt)
		tdls_debug(QDF_MAC_ADDR_FMT "link_status %d tdls_support %d tx %d rx %d",
			   QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes),
			   curr_peer->link_status, curr_peer->tdls_support,
			   curr_peer->tx_pkt, curr_peer->rx_pkt);

	if (!TDLS_IS_LINK_CONNECTED(curr_peer) &&
	    ((curr_peer->tx_pkt + curr_peer->rx_pkt) >=
	    tdls_vdev->threshold_config.tx_packet_n)) {
		/* Ignore discovery attempt if External Control is enabled, that
		 * is, peer is forced. In that case, continue discovery attempt
		 * regardless attempt count
		 */
		tdls_debug("TDLS UNKNOWN pre discover ");
		if (curr_peer->is_forced_peer ||
			curr_peer->discovery_attempt++ <
		    tdls_vdev->threshold_config.discovery_tries_n) {
			tdls_debug("TDLS UNKNOWN discover ");
			tdls_vdev->curr_candidate = curr_peer;
			tdls_implicit_send_discovery_request(tdls_vdev);
		} else {
			if (curr_peer->link_status != TDLS_LINK_CONNECTING) {
				curr_peer->tdls_support =
						TDLS_CAP_NOT_SUPPORTED;
				tdls_set_peer_link_status(
						curr_peer,
						TDLS_LINK_IDLE,
						TDLS_LINK_NOT_SUPPORTED);
			}
		}
	}
}

/**
 * tdls_ct_process_peers() - process the peer
 * @curr_peer: tdls peer needs to be examined
 * @tdls_vdev_obj: tdls vdev object
 * @tdls_soc_obj: tdls soc object
 *
 * This function check the peer capability and process the metadata from
 * the peer
 *
 * Return: None
 */
static void tdls_ct_process_peers(struct tdls_peer *curr_peer,
				  struct tdls_vdev_priv_obj *tdls_vdev_obj,
				  struct tdls_soc_priv_obj *tdls_soc_obj)
{
	switch (curr_peer->tdls_support) {
	case TDLS_CAP_SUPPORTED:
		tdls_ct_process_cap_supported(curr_peer, tdls_vdev_obj,
						       tdls_soc_obj);
		break;

	case TDLS_CAP_UNKNOWN:
		tdls_ct_process_cap_unknown(curr_peer, tdls_vdev_obj,
						     tdls_soc_obj);
		break;
	default:
		break;
	}

}

static void tdls_ct_process_handler(struct wlan_objmgr_vdev *vdev)
{
	int i;
	qdf_list_t *head;
	qdf_list_node_t *list_node;
	struct tdls_peer *curr_peer;
	QDF_STATUS status;
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct tdls_soc_priv_obj *tdls_soc_obj;

	if (QDF_STATUS_SUCCESS != tdls_get_vdev_objects(vdev, &tdls_vdev_obj,
						   &tdls_soc_obj))
		return;

	/* If any concurrency is detected */
	if (!tdls_soc_obj->enable_tdls_connection_tracker) {
		tdls_notice("Connection tracker is disabled");
		return;
	}

	/* Update tx rx traffic sample in tdls data structures */
	tdls_ct_sampling_tx_rx(tdls_vdev_obj, tdls_soc_obj);

	for (i = 0; i < WLAN_TDLS_PEER_LIST_SIZE; i++) {
		head = &tdls_vdev_obj->peer_list[i];
		status = qdf_list_peek_front(head, &list_node);
		while (QDF_IS_STATUS_SUCCESS(status)) {
			curr_peer = qdf_container_of(list_node,
						struct tdls_peer, node);
			tdls_ct_process_peers(curr_peer, tdls_vdev_obj,
					      tdls_soc_obj);
			curr_peer->tx_pkt = 0;
			curr_peer->rx_pkt = 0;
			status = qdf_list_peek_next(head,
						    list_node, &list_node);
		}
	}

	tdls_timer_restart(tdls_vdev_obj->vdev,
			   &tdls_vdev_obj->peer_update_timer,
			   tdls_vdev_obj->threshold_config.tx_period_t);

}

void tdls_ct_handler(void *user_data)
{
	struct wlan_objmgr_vdev *vdev;

	if (!user_data)
		return;

	vdev = tdls_get_vdev(user_data, WLAN_TDLS_NB_ID);
	if (!vdev)
		return;

	tdls_ct_process_handler(vdev);

	wlan_objmgr_vdev_release_ref(vdev,
				     WLAN_TDLS_NB_ID);
}

int tdls_set_tdls_offchannel(struct tdls_soc_priv_obj *tdls_soc,
			     int offchannel)
{
	uint32_t tdls_feature_flags;

	tdls_feature_flags = tdls_soc->tdls_configs.tdls_feature_flags;

	if (TDLS_IS_OFF_CHANNEL_ENABLED(tdls_feature_flags) &&
	   (TDLS_SUPPORT_EXP_TRIG_ONLY == tdls_soc->tdls_current_mode ||
	    TDLS_SUPPORT_IMP_MODE == tdls_soc->tdls_current_mode ||
	    TDLS_SUPPORT_EXT_CONTROL == tdls_soc->tdls_current_mode)) {
		if (offchannel < TDLS_PREFERRED_OFF_CHANNEL_NUM_MIN ||
			offchannel > TDLS_PREFERRED_OFF_CHANNEL_NUM_MAX) {
			tdls_err("Invalid tdls off channel %u", offchannel);
			return -EINVAL;
			}
	} else {
		tdls_err("Either TDLS or TDLS Off-channel is not enabled");
		return -ENOTSUPP;
	}
	tdls_notice("change tdls off channel from %d to %d",
		   tdls_soc->tdls_off_channel, offchannel);
	tdls_soc->tdls_off_channel = offchannel;
	return 0;
}

int tdls_set_tdls_secoffchanneloffset(struct tdls_soc_priv_obj *tdls_soc,
				int offchanoffset)
{
	uint32_t tdls_feature_flags;

	tdls_feature_flags = tdls_soc->tdls_configs.tdls_feature_flags;

	if (!TDLS_IS_OFF_CHANNEL_ENABLED(tdls_feature_flags) ||
	    TDLS_SUPPORT_SUSPENDED >= tdls_soc->tdls_current_mode) {
		tdls_err("Either TDLS or TDLS Off-channel is not enabled");
		return  -ENOTSUPP;
	}

	tdls_soc->tdls_channel_offset = BW_INVALID;

	switch (offchanoffset) {
	case TDLS_SEC_OFFCHAN_OFFSET_0:
		tdls_soc->tdls_channel_offset = BW20;
		break;
	case TDLS_SEC_OFFCHAN_OFFSET_40PLUS:
		tdls_soc->tdls_channel_offset = BW40_HIGH_PRIMARY;
		break;
	case TDLS_SEC_OFFCHAN_OFFSET_40MINUS:
		tdls_soc->tdls_channel_offset = BW40_LOW_PRIMARY;
		break;
	case TDLS_SEC_OFFCHAN_OFFSET_80:
		tdls_soc->tdls_channel_offset = BW80;
		break;
	case TDLS_SEC_OFFCHAN_OFFSET_160:
		tdls_soc->tdls_channel_offset = BWALL;
		break;
	default:
		tdls_err("Invalid tdls secondary off channel offset %d",
			offchanoffset);
		return -EINVAL;
	} /* end switch */

	tdls_notice("change tdls secondary off channel offset to 0x%x",
		    tdls_soc->tdls_channel_offset);
	return 0;
}

int tdls_set_tdls_offchannelmode(struct wlan_objmgr_vdev *vdev,
				 int offchanmode)
{
	struct tdls_peer *conn_peer = NULL;
	struct tdls_channel_switch_params chan_switch_params = {0};
	QDF_STATUS status = QDF_STATUS_E_FAILURE;
	int ret_value = 0;
	struct tdls_vdev_priv_obj *tdls_vdev;
	struct tdls_soc_priv_obj *tdls_soc;
	uint32_t tdls_feature_flags;


	status = tdls_get_vdev_objects(vdev, &tdls_vdev, &tdls_soc);

	if (status != QDF_STATUS_SUCCESS)
		return -EINVAL;


	if (offchanmode < ENABLE_CHANSWITCH ||
			offchanmode > DISABLE_CHANSWITCH) {
		tdls_err("Invalid tdls off channel mode %d", offchanmode);
		return -EINVAL;
	}

	if (wlan_vdev_is_up(vdev) != QDF_STATUS_SUCCESS) {
		tdls_err("tdls off channel req in not associated state %d",
			offchanmode);
		return -EPERM;
	}

	tdls_feature_flags = tdls_soc->tdls_configs.tdls_feature_flags;
	if (!TDLS_IS_OFF_CHANNEL_ENABLED(tdls_feature_flags) ||
	    TDLS_SUPPORT_SUSPENDED >= tdls_soc->tdls_current_mode) {
		tdls_err("Either TDLS or TDLS Off-channel is not enabled");
		return  -ENOTSUPP;
	}

	conn_peer = tdls_find_first_connected_peer(tdls_vdev);
	if (!conn_peer) {
		tdls_err("No TDLS Connected Peer");
		return -EPERM;
	}

	tdls_notice("TDLS Channel Switch in swmode=%d tdls_off_channel %d offchanoffset %d",
		   offchanmode, tdls_soc->tdls_off_channel,
		   tdls_soc->tdls_channel_offset);

	switch (offchanmode) {
	case ENABLE_CHANSWITCH:
		if (tdls_soc->tdls_off_channel &&
			tdls_soc->tdls_channel_offset != BW_INVALID) {
			chan_switch_params.tdls_off_ch =
				tdls_soc->tdls_off_channel;
			chan_switch_params.tdls_off_ch_bw_offset =
				tdls_soc->tdls_channel_offset;
			chan_switch_params.oper_class =
			   tdls_find_opclass(tdls_soc->soc,
				chan_switch_params.tdls_off_ch,
				chan_switch_params.tdls_off_ch_bw_offset);
			if (!chan_switch_params.oper_class) {
				if (chan_switch_params.tdls_off_ch_bw_offset ==
				    BW40_HIGH_PRIMARY)
					chan_switch_params.oper_class =
					tdls_find_opclass(tdls_soc->soc,
						chan_switch_params.tdls_off_ch,
						BW40_LOW_PRIMARY);
				else if (chan_switch_params.
					 tdls_off_ch_bw_offset ==
					 BW40_LOW_PRIMARY)
					chan_switch_params.oper_class =
					tdls_find_opclass(tdls_soc->soc,
						chan_switch_params.tdls_off_ch,
						BW40_HIGH_PRIMARY);
				tdls_debug("oper_class:%d",
					    chan_switch_params.oper_class);
			}
		} else if (conn_peer->off_channel_capable &&
			   conn_peer->pref_off_chan_num) {
			chan_switch_params.tdls_off_ch =
				conn_peer->pref_off_chan_num;
			chan_switch_params.oper_class =
				tdls_get_opclass_from_bandwidth(
				tdls_soc, conn_peer->pref_off_chan_num,
				tdls_soc->tdls_configs.tdls_pre_off_chan_bw,
				&chan_switch_params.tdls_off_ch_bw_offset);
		} else {
			tdls_err("TDLS off-channel parameters are not set yet!!!");
			return -EINVAL;

		}
		break;
	case DISABLE_CHANSWITCH:
		chan_switch_params.tdls_off_ch = 0;
		chan_switch_params.tdls_off_ch_bw_offset = 0;
		chan_switch_params.oper_class = 0;
		break;
	default:
		tdls_err("Incorrect Parameters mode: %d tdls_off_channel: %d offchanoffset: %d",
			offchanmode, tdls_soc->tdls_off_channel,
			tdls_soc->tdls_channel_offset);
		return -EINVAL;
	} /* end switch */

	chan_switch_params.vdev_id = tdls_vdev->session_id;
	chan_switch_params.tdls_sw_mode = offchanmode;
	chan_switch_params.is_responder =
		conn_peer->is_responder;
	qdf_mem_copy(&chan_switch_params.peer_mac_addr,
		     &conn_peer->peer_mac.bytes,
		     QDF_MAC_ADDR_SIZE);
	tdls_notice("Peer " QDF_MAC_ADDR_FMT " vdevId: %d, off channel: %d, offset: %d, mode: %d, is_responder: %d",
		 QDF_MAC_ADDR_REF(chan_switch_params.peer_mac_addr),
		 chan_switch_params.vdev_id,
		 chan_switch_params.tdls_off_ch,
		 chan_switch_params.tdls_off_ch_bw_offset,
		 chan_switch_params.tdls_sw_mode,
		 chan_switch_params.is_responder);

	status = tdls_set_offchan_mode(tdls_soc->soc,
				       &chan_switch_params);

	if (status != QDF_STATUS_SUCCESS) {
		tdls_err("Failed to send channel switch request to wmi");
		return -EINVAL;
	}

	tdls_soc->tdls_fw_off_chan_mode = offchanmode;

	if (ENABLE_CHANSWITCH == offchanmode) {
		conn_peer = tdls_find_first_connected_peer(tdls_vdev);
		if (!conn_peer) {
			tdls_err("No TDLS Connected Peer");
			return -EPERM;
		}
		conn_peer->pref_off_chan_num =
			chan_switch_params.tdls_off_ch;
		conn_peer->op_class_for_pref_off_chan =
			chan_switch_params.oper_class;
	}

	return ret_value;
}

static QDF_STATUS tdls_delete_all_tdls_peers_flush_cb(struct scheduler_msg *msg)
{
	if (msg && msg->bodyptr) {
		qdf_mem_free(msg->bodyptr);
		msg->bodyptr = NULL;
	}

	return QDF_STATUS_SUCCESS;
}
/**
 * tdls_delete_all_tdls_peers(): send request to delete tdls peers
 * @vdev: vdev object
 * @tdls_soc: tdls soc object
 *
 * This function sends request to lim to delete tdls peers
 *
 * Return: QDF_STATUS
 */
QDF_STATUS tdls_delete_all_tdls_peers(struct wlan_objmgr_vdev *vdev,
					  struct tdls_soc_priv_obj *tdls_soc)
{
	struct wlan_objmgr_peer *peer;
	struct tdls_del_all_tdls_peers *del_msg;
	struct scheduler_msg msg = {0};
	QDF_STATUS status;

	peer = wlan_objmgr_vdev_try_get_bsspeer(vdev, WLAN_TDLS_SB_ID);
	if (!peer) {
		tdls_err("bss peer is null");
		return QDF_STATUS_E_FAILURE;
	}

	del_msg = qdf_mem_malloc(sizeof(*del_msg));
	if (!del_msg) {
		wlan_objmgr_peer_release_ref(peer, WLAN_TDLS_SB_ID);
		return QDF_STATUS_E_FAILURE;
	}

	qdf_mem_copy(del_msg->bssid.bytes,
		     wlan_peer_get_macaddr(peer), QDF_MAC_ADDR_SIZE);

	wlan_objmgr_peer_release_ref(peer, WLAN_TDLS_SB_ID);

	del_msg->msg_type = tdls_soc->tdls_del_all_peers;
	del_msg->msg_len = (uint16_t) sizeof(*del_msg);

	/* Send the request to PE. */
	qdf_mem_zero(&msg, sizeof(msg));

	tdls_debug("sending delete all peers req to PE ");

	msg.type = del_msg->msg_type;
	msg.bodyptr = del_msg;
	msg.flush_callback = tdls_delete_all_tdls_peers_flush_cb;

	status = scheduler_post_message(QDF_MODULE_ID_TDLS,
					QDF_MODULE_ID_PE,
					QDF_MODULE_ID_PE, &msg);
	if (QDF_IS_STATUS_ERROR(status)) {
		tdls_err("post delete all peer req failed, status %d", status);
		qdf_mem_free(del_msg);
	}

	return status;
}

void tdls_disable_offchan_and_teardown_links(
				struct wlan_objmgr_vdev *vdev)
{
	uint16_t connected_tdls_peers = 0;
	uint8_t staidx;
	struct tdls_peer *curr_peer = NULL;
	struct tdls_vdev_priv_obj *tdls_vdev;
	struct tdls_soc_priv_obj *tdls_soc;
	QDF_STATUS status;
	uint8_t vdev_id;
	bool tdls_in_progress = false;

	status = tdls_get_vdev_objects(vdev, &tdls_vdev, &tdls_soc);
	if (QDF_STATUS_SUCCESS != status) {
		tdls_err("tdls objects are NULL ");
		return;
	}

	if (TDLS_SUPPORT_SUSPENDED >= tdls_soc->tdls_current_mode) {
		tdls_debug("TDLS mode %d is disabled OR not suspended now",
			   tdls_soc->tdls_current_mode);
		return;
	}

	connected_tdls_peers = tdls_soc->connected_peer_count;
	if (tdls_is_progress(tdls_vdev, NULL, 0))
		tdls_in_progress = true;

	if (!(connected_tdls_peers || tdls_in_progress)) {
		tdls_debug("No TDLS connected/progress peers to delete");
		vdev_id = vdev->vdev_objmgr.vdev_id;
		if (tdls_soc->set_state_info.set_state_cnt > 0) {
			tdls_debug("Disable the tdls in FW as second interface is coming up");
			tdls_send_update_to_fw(tdls_vdev, tdls_soc, true,
					       true, false, vdev_id);
		}
		return;
	}

	/* TDLS is not supported in case of concurrency.
	 * Disable TDLS Offchannel in FW to avoid more
	 * than two concurrent channels and generate TDLS
	 * teardown indication to supplicant.
	 * Below function Finds the first connected peer and
	 * disables TDLS offchannel for that peer.
	 * FW enables TDLS offchannel only when there is
	 * one TDLS peer. When there are more than one TDLS peer,
	 * there will not be TDLS offchannel in FW.
	 * So to avoid sending multiple request to FW, for now,
	 * just invoke offchannel mode functions only once
	 */
	tdls_set_tdls_offchannel(tdls_soc,
				tdls_soc->tdls_configs.tdls_pre_off_chan_num);
	tdls_set_tdls_secoffchanneloffset(tdls_soc,
			TDLS_SEC_OFFCHAN_OFFSET_40PLUS);
	tdls_set_tdls_offchannelmode(vdev, DISABLE_CHANSWITCH);

	/* Send Msg to PE for deleting all the TDLS peers */
	tdls_delete_all_tdls_peers(vdev, tdls_soc);

	for (staidx = 0; staidx < tdls_soc->max_num_tdls_sta;
							staidx++) {
		if (!tdls_soc->tdls_conn_info[staidx].valid_entry)
			continue;

		curr_peer = tdls_find_all_peer(tdls_soc,
			tdls_soc->tdls_conn_info[staidx].peer_mac.bytes);
		if (!curr_peer)
			continue;

		tdls_notice("indicate TDLS teardown "QDF_MAC_ADDR_FMT,
			    QDF_MAC_ADDR_REF(curr_peer->peer_mac.bytes));

		/* Indicate teardown to supplicant */
		tdls_indicate_teardown(tdls_vdev,
				       curr_peer,
				       TDLS_TEARDOWN_PEER_UNSPEC_REASON);

		/*
		 * Del Sta happened already as part of tdls_delete_all_tdls_peers
		 * Hence clear tdls vdev data structure.
		 */
		tdls_reset_peer(tdls_vdev, curr_peer->peer_mac.bytes);

		tdls_decrement_peer_count(tdls_soc);
		tdls_soc->tdls_conn_info[staidx].valid_entry = false;
		tdls_soc->tdls_conn_info[staidx].session_id = 255;
		tdls_soc->tdls_conn_info[staidx].index =
						INVALID_TDLS_PEER_INDEX;

		qdf_mem_zero(&tdls_soc->tdls_conn_info[staidx].peer_mac,
			     sizeof(struct qdf_mac_addr));
	}
}

void tdls_teardown_connections(struct tdls_link_teardown *tdls_teardown)
{
	struct tdls_vdev_priv_obj *tdls_vdev_obj;
	struct wlan_objmgr_vdev *tdls_vdev;

	/* Get the tdls specific vdev and clear the links */
	tdls_vdev = tdls_get_vdev(tdls_teardown->psoc, WLAN_TDLS_SB_ID);
	if (!tdls_vdev) {
		tdls_err("Unable get the vdev");
		goto fail_vdev;
	}

	tdls_vdev_obj = wlan_vdev_get_tdls_vdev_obj(tdls_vdev);
	if (!tdls_vdev_obj) {
		tdls_err("vdev priv is NULL");
		goto fail_tdls_vdev;
	}

	tdls_disable_offchan_and_teardown_links(tdls_vdev);
	qdf_event_set(&tdls_vdev_obj->tdls_teardown_comp);
fail_tdls_vdev:
	wlan_objmgr_vdev_release_ref(tdls_vdev, WLAN_TDLS_SB_ID);
fail_vdev:
	wlan_objmgr_psoc_release_ref(tdls_teardown->psoc, WLAN_TDLS_SB_ID);
	qdf_mem_free(tdls_teardown);
}
