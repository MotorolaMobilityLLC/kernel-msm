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
 * DOC: declares driver functions interfacing with linux kernel
 */

#ifndef _WLAN_CFG80211_TDLS_H_
#define _WLAN_CFG80211_TDLS_H_

#include <linux/version.h>
#include <linux/netdevice.h>
#include <linux/completion.h>
#include <net/cfg80211.h>
#include <qca_vendor.h>
#include <wlan_tdls_public_structs.h>
#include <qdf_list.h>
#include <qdf_types.h>
#include <wlan_tdls_ucfg_api.h>

#ifdef FEATURE_WLAN_TDLS

#define TDLS_VDEV_MAGIC 0x54444c53       /* "TDLS" */

/**
 * struct osif_tdls_vdev - OS tdls vdev private structure
 * @tdls_add_peer_comp: Completion to add tdls peer
 * @tdls_del_peer_comp: Completion to delete tdls peer
 * @tdls_mgmt_comp: Completion to send tdls mgmt packets
 * @tdls_link_establish_req_comp: Completion to establish link, sync to
 * send establish params to firmware, not used today.
 * @tdls_teardown_comp: Completion to teardown tdls peer
 * @tdls_user_cmd_comp: tdls user command completion event
 * @tdls_antenna_switch_comp: Completion to switch antenna
 * @tdls_add_peer_status: Peer status after add peer
 * @mgmt_tx_completion_status: Tdls mgmt frames TX completion status code
 * @tdls_user_cmd_len: tdls user command written buffer length
 * @tdls_antenna_switch_status: return status after antenna switch
 */
struct osif_tdls_vdev {
	struct completion tdls_add_peer_comp;
	struct completion tdls_del_peer_comp;
	struct completion tdls_mgmt_comp;
	struct completion tdls_link_establish_req_comp;
	struct completion tdls_teardown_comp;
	struct completion tdls_user_cmd_comp;
	struct completion tdls_antenna_switch_comp;
	QDF_STATUS tdls_add_peer_status;
	uint32_t mgmt_tx_completion_status;
	uint32_t tdls_user_cmd_len;
	int tdls_antenna_switch_status;
};

/**
 * enum qca_wlan_vendor_tdls_trigger_mode_vdev_map: Maps the user space TDLS
 *	trigger mode in the host driver.
 * @WLAN_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT: TDLS Connection and
 *	disconnection handled by user space.
 * @WLAN_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT: TDLS connection and
 *	disconnection controlled by host driver based on data traffic.
 * @WLAN_VENDOR_TDLS_TRIGGER_MODE_EXTERNAL: TDLS connection and
 *	disconnection jointly controlled by user space and host driver.
 */
enum qca_wlan_vendor_tdls_trigger_mode_vdev_map {
	WLAN_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT =
		QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT,
	WLAN_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT =
		QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT,
	WLAN_VENDOR_TDLS_TRIGGER_MODE_EXTERNAL =
		((QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_EXPLICIT |
		  QCA_WLAN_VENDOR_TDLS_TRIGGER_MODE_IMPLICIT) << 1),
};

/**
 * wlan_cfg80211_tdls_osif_priv_init() - API to initialize tdls os private
 * @vdev: vdev object
 *
 * API to initialize tdls os private
 *
 * Return: QDF_STATUS
 */
QDF_STATUS wlan_cfg80211_tdls_osif_priv_init(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_cfg80211_tdls_osif_priv_deinit() - API to deinitialize tdls os private
 * @vdev: vdev object
 *
 * API to deinitialize tdls os private
 *
 * Return: None
 */
void wlan_cfg80211_tdls_osif_priv_deinit(struct wlan_objmgr_vdev *vdev);

/**
 * wlan_cfg80211_tdls_add_peer() - process cfg80211 add TDLS peer request
 * @vdev: vdev object
 * @mac: MAC address for TDLS peer
 *
 * Return: 0 for success; negative errno otherwise
 */
int wlan_cfg80211_tdls_add_peer(struct wlan_objmgr_vdev *vdev,
				const uint8_t *mac);

/**
 * wlan_cfg80211_tdls_update_peer() - process cfg80211 update TDLS peer request
 * @vdev: vdev object
 * @mac: MAC address for TDLS peer
 * @params: Pointer to station parameters
 *
 * Return: 0 for success; negative errno otherwise
 */
int wlan_cfg80211_tdls_update_peer(struct wlan_objmgr_vdev *vdev,
				   const uint8_t *mac,
				   struct station_parameters *params);

/**
 * wlan_cfg80211_tdls_configure_mode() - configure tdls mode
 * @vdev: vdev obj manager
 * @trigger_mode: tdls trgger mode
 *
 * Return: 0 for success; negative errno otherwise
 */
int wlan_cfg80211_tdls_configure_mode(struct wlan_objmgr_vdev *vdev,
						uint32_t trigger_mode);

/**
 * wlan_cfg80211_tdls_oper() - process cfg80211 operation on an TDLS peer
 * @vdev: vdev object
 * @peer: MAC address of the TDLS peer
 * @oper: cfg80211 TDLS operation
 *
 * Return: 0 on success; negative errno otherwise
 */
int wlan_cfg80211_tdls_oper(struct wlan_objmgr_vdev *vdev,
			    const uint8_t *peer,
			    enum nl80211_tdls_operation oper);

/**
 * wlan_cfg80211_tdls_get_all_peers() - get all the TDLS peers from the list
 * @vdev: vdev object
 * @buf: output buffer
 * @buflen: valid length of the output error
 *
 * Return: length of the output buffer
 */
int wlan_cfg80211_tdls_get_all_peers(struct wlan_objmgr_vdev *vdev,
				char *buf, int buflen);

/**
 * wlan_cfg80211_tdls_mgmt() - process tdls management frames from the supplicant
 * @vdev: vdev object
 * @peer: MAC address of the TDLS peer
 * @action_code: type of TDLS mgmt frame to be sent
 * @dialog_token: dialog token used in the frame
 * @status_code: status to be incuded in the frame
 * @peer_capability: peer capability information
 * @buf: additional IEs to be included
 * @len: length of additional Ies
 * @oper: cfg80211 TDLS operation
 *
 * Return: 0 on success; negative errno otherwise
 */
int wlan_cfg80211_tdls_mgmt(struct wlan_objmgr_vdev *vdev,
			    const uint8_t *peer,
			    uint8_t action_code, uint8_t dialog_token,
			    uint16_t status_code, uint32_t peer_capability,
			    const uint8_t *buf, size_t len);

/**
 * wlan_tdls_antenna_switch() - process tdls antenna switch
 * @vdev: vdev object
 * @mode: antenna mode
 *
 * Return: 0 on success; -EAGAIN to retry
 */
int wlan_tdls_antenna_switch(struct wlan_objmgr_vdev *vdev, uint32_t mode);

/**
 * wlan_cfg80211_tdls_event_callback() - callback for tdls module
 * @userdata: user data
 * @type: request callback type
 * @param: passed parameter
 *
 * This is used by TDLS to sync with os interface
 *
 * Return: None
 */
void wlan_cfg80211_tdls_event_callback(void *userdata,
				       enum tdls_event_type type,
				       struct tdls_osif_indication *param);

/**
 * wlan_cfg80211_tdls_rx_callback() - Callback for rx mgmt frame
 * @user_data: pointer to soc object
 * @rx_frame: RX mgmt frame information
 *
 * This callback will be used to rx frames in os interface.
 *
 * Return: None
 */
void wlan_cfg80211_tdls_rx_callback(void *user_data,
	struct tdls_rx_mgmt_frame *rx_frame);

/**
 * hdd_notify_tdls_reset_adapter() - notify reset adapter to TDLS
 * @vdev: vdev object manager
 *
 * Notify hdd reset adapter to TDLS component
 *
 * Return: None
 */
void hdd_notify_tdls_reset_adapter(struct wlan_objmgr_vdev *vdev);

/**
 * hdd_notify_sta_connect() - notify sta connect to TDLS
 * @session_id: pointer to soc object
 * @tdls_chan_swit_prohibited: indicates channel switch capability
 * @tdls_prohibited: indicates tdls allowed or not
 * @vdev: vdev object manager
 *
 * Notify sta connect event to TDLS component
 *
 * Return: None
 */
void
hdd_notify_sta_connect(uint8_t session_id,
		       bool tdls_chan_swit_prohibited,
		       bool tdls_prohibited,
		       struct wlan_objmgr_vdev *vdev);

/**
 * hdd_notify_sta_disconnect() - notify sta disconnect to TDLS
 * @session_id: pointer to soc object
 * @lfr_roam: indicate, whether disconnect due to lfr roam
 * @bool user_disconnect: disconnect from user space
 * @vdev: vdev object manager
 *
 * Notify sta disconnect event to TDLS component
 *
 * Return: None
 */
void hdd_notify_sta_disconnect(uint8_t session_id,
			       bool lfr_roam,
			       bool user_disconnect,
			       struct wlan_objmgr_vdev *vdev);

#else /* FEATURE_WLAN_TDLS */
static inline
QDF_STATUS wlan_cfg80211_tdls_osif_priv_init(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void wlan_cfg80211_tdls_osif_priv_deinit(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
hdd_notify_tdls_reset_adapter(struct wlan_objmgr_vdev *vdev)
{
}

static inline void
hdd_notify_sta_connect(uint8_t session_id,
		       bool tdls_chan_swit_prohibited,
		       bool tdls_prohibited,
		       struct wlan_objmgr_vdev *vdev)
{
}

static inline
void hdd_notify_sta_disconnect(uint8_t session_id,
			       bool lfr_roam,
			       bool user_disconnect,
			       struct wlan_objmgr_vdev *vdev)
{

}

static inline
int wlan_cfg80211_tdls_configure_mode(struct wlan_objmgr_vdev *vdev,
						uint32_t trigger_mode)
{
	return 0;
}

static inline
void hdd_notify_teardown_tdls_links(struct wlan_objmgr_psoc *psoc)
{

}
#endif /* FEATURE_WLAN_TDLS */
#endif /* _WLAN_CFG80211_TDLS_H_ */
