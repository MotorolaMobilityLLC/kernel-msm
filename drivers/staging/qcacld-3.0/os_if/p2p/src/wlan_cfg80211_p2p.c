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
 * DOC: defines driver functions interfacing with linux kernel
 */

#include <qdf_util.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>
#include <wlan_p2p_public_struct.h>
#include <wlan_p2p_ucfg_api.h>
#include <wlan_policy_mgr_api.h>
#include <wlan_utility.h>
#include <wlan_osif_priv.h>
#include "wlan_cfg80211.h"
#include "wlan_cfg80211_p2p.h"

#define MAX_NO_OF_2_4_CHANNELS 14
#define MAX_OFFCHAN_TIME_FOR_DNBS 150

/**
 * wlan_p2p_rx_callback() - Callback for rx mgmt frame
 * @user_data: pointer to soc object
 * @rx_frame: RX mgmt frame information
 *
 * This callback will be used to rx frames in os interface.
 *
 * Return: None
 */
static void wlan_p2p_rx_callback(void *user_data,
	struct p2p_rx_mgmt_frame *rx_frame)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_osif_priv *osif_priv;
	struct wireless_dev *wdev;

	psoc = user_data;
	if (!psoc) {
		osif_err("psoc is null");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
		rx_frame->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is null");
		goto fail;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wdev is null");
		goto fail;
	}

	osif_debug("Indicate frame over nl80211, idx:%d",
		   wdev->netdev->ifindex);

#if (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 18, 0))
	cfg80211_rx_mgmt(wdev, rx_frame->rx_freq, rx_frame->rx_rssi * 100,
			 rx_frame->buf, rx_frame->frame_len,
			 NL80211_RXMGMT_FLAG_ANSWERED);
#elif (LINUX_VERSION_CODE >= KERNEL_VERSION(3, 12, 0))
	cfg80211_rx_mgmt(wdev, rx_frame->rx_freq, rx_frame->rx_rssi * 100,
			 rx_frame->buf, rx_frame->frame_len,
			 NL80211_RXMGMT_FLAG_ANSWERED, GFP_ATOMIC);
#else
	cfg80211_rx_mgmt(wdev, rx_frame->rx_freq, rx_frame->rx_rssi * 100,
			 rx_frame->buf, rx_frame->frame_len, GFP_ATOMIC);
#endif /* LINUX_VERSION_CODE */
fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
}

/**
 * wlan_p2p_action_tx_cnf_callback() - Callback for tx confirmation
 * @user_data: pointer to soc object
 * @tx_cnf: tx confirmation information
 *
 * This callback will be used to give tx mgmt frame confirmation to
 * os interface.
 *
 * Return: None
 */
static void wlan_p2p_action_tx_cnf_callback(void *user_data,
	struct p2p_tx_cnf *tx_cnf)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_osif_priv *osif_priv;
	struct wireless_dev *wdev;
	bool is_success;

	psoc = user_data;
	if (!psoc) {
		osif_err("psoc is null");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
		tx_cnf->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is null");
		goto fail;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wireless dev is null");
		goto fail;
	}

	is_success = tx_cnf->status ? false : true;
	cfg80211_mgmt_tx_status(
		wdev,
		tx_cnf->action_cookie,
		tx_cnf->buf, tx_cnf->buf_len,
		is_success, GFP_KERNEL);
fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
}

#ifdef FEATURE_P2P_LISTEN_OFFLOAD
/**
 * wlan_p2p_lo_event_callback() - Callback for listen offload event
 * @user_data: pointer to soc object
 * @p2p_lo_event: listen offload event information
 *
 * This callback will be used to give listen offload event to os interface.
 *
 * Return: None
 */
static void wlan_p2p_lo_event_callback(void *user_data,
	struct p2p_lo_event *p2p_lo_event)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	struct vdev_osif_priv *osif_priv;
	struct wireless_dev *wdev;
	struct sk_buff *vendor_event;

	osif_debug("user data:%pK, vdev id:%d, reason code:%d",
		   user_data, p2p_lo_event->vdev_id,
		   p2p_lo_event->reason_code);

	psoc = user_data;
	if (!psoc) {
		osif_err("psoc is null");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
		p2p_lo_event->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is null");
		goto fail;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wireless dev is null");
		goto fail;
	}

	vendor_event = cfg80211_vendor_event_alloc(wdev->wiphy, NULL,
			sizeof(uint32_t) + NLMSG_HDRLEN,
			QCA_NL80211_VENDOR_SUBCMD_P2P_LO_EVENT_INDEX,
			GFP_KERNEL);
	if (!vendor_event) {
		osif_err("cfg80211_vendor_event_alloc failed");
		goto fail;
	}

	if (nla_put_u32(vendor_event,
		QCA_WLAN_VENDOR_ATTR_P2P_LISTEN_OFFLOAD_STOP_REASON,
		p2p_lo_event->reason_code)) {
		osif_err("nla put failed");
		kfree_skb(vendor_event);
		goto fail;
	}

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
}

static inline void wlan_p2p_init_lo_event(struct p2p_start_param *start_param,
					  struct wlan_objmgr_psoc *psoc)
{
	start_param->lo_event_cb = wlan_p2p_lo_event_callback;
	start_param->lo_event_cb_data = psoc;
}
#else
static inline void wlan_p2p_init_lo_event(struct p2p_start_param *start_param,
					  struct wlan_objmgr_psoc *psoc)
{
}
#endif /* FEATURE_P2P_LISTEN_OFFLOAD */
/**
 * wlan_p2p_event_callback() - Callback for P2P event
 * @user_data: pointer to soc object
 * @p2p_event: p2p event information
 *
 * This callback will be used to give p2p event to os interface.
 *
 * Return: None
 */
static void wlan_p2p_event_callback(void *user_data,
	struct p2p_event *p2p_event)
{
	struct wlan_objmgr_psoc *psoc;
	struct wlan_objmgr_vdev *vdev;
	struct ieee80211_channel *chan;
	struct vdev_osif_priv *osif_priv;
	struct wireless_dev *wdev;

	osif_debug("user data:%pK, vdev id:%d, event type:%d",
		   user_data, p2p_event->vdev_id, p2p_event->roc_event);

	psoc = user_data;
	if (!psoc) {
		osif_err("psoc is null");
		return;
	}

	vdev = wlan_objmgr_get_vdev_by_id_from_psoc(psoc,
		p2p_event->vdev_id, WLAN_P2P_ID);
	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		osif_err("osif_priv is null");
		goto fail;
	}

	wdev = osif_priv->wdev;
	if (!wdev) {
		osif_err("wireless dev is null");
		goto fail;
	}

	chan = ieee80211_get_channel(wdev->wiphy,
				     wlan_chan_to_freq(p2p_event->chan));
	if (!chan) {
		osif_err("channel conversion failed");
		goto fail;
	}

	if (p2p_event->roc_event == ROC_EVENT_READY_ON_CHAN) {
		cfg80211_ready_on_channel(wdev,
			p2p_event->cookie, chan,
			p2p_event->duration, GFP_KERNEL);
	} else if (p2p_event->roc_event == ROC_EVENT_COMPLETED) {
		cfg80211_remain_on_channel_expired(wdev,
			p2p_event->cookie, chan, GFP_KERNEL);
	} else {
		osif_err("Invalid p2p event");
	}

fail:
	wlan_objmgr_vdev_release_ref(vdev, WLAN_P2P_ID);
}

QDF_STATUS p2p_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	struct p2p_start_param start_param;

	if (!psoc) {
		osif_err("psoc null");
		return QDF_STATUS_E_INVAL;
	}

	start_param.rx_cb = wlan_p2p_rx_callback;
	start_param.rx_cb_data = psoc;
	start_param.event_cb = wlan_p2p_event_callback;
	start_param.event_cb_data = psoc;
	start_param.tx_cnf_cb = wlan_p2p_action_tx_cnf_callback;
	start_param.tx_cnf_cb_data = psoc;
	wlan_p2p_init_lo_event(&start_param, psoc);

	return ucfg_p2p_psoc_start(psoc, &start_param);
}

QDF_STATUS p2p_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	if (!psoc) {
		osif_err("psoc null");
		return QDF_STATUS_E_INVAL;
	}

	return ucfg_p2p_psoc_stop(psoc);
}

int wlan_cfg80211_roc(struct wlan_objmgr_vdev *vdev,
	struct ieee80211_channel *chan, uint32_t duration,
	uint64_t *cookie)
{
	struct p2p_roc_req roc_req = {0};
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;
	bool ok;
	int ret;

	if (!vdev) {
		osif_err("invalid vdev object");
		return -EINVAL;
	}

	if (!chan) {
		osif_err("invalid channel");
		return -EINVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	vdev_id = wlan_vdev_get_id(vdev);
	if (!psoc) {
		osif_err("psoc handle is NULL");
		return -EINVAL;
	}

	roc_req.chan = (uint32_t)wlan_freq_to_chan(chan->center_freq);
	roc_req.duration = duration;
	roc_req.vdev_id = (uint32_t)vdev_id;

	ret = policy_mgr_is_chan_ok_for_dnbs(psoc, chan->center_freq, &ok);
	if (QDF_IS_STATUS_ERROR(ret)) {
		osif_err("policy_mgr_is_chan_ok_for_dnbs():ret:%d",
			 ret);
		return -EINVAL;
	}

	if (!ok) {
		osif_err("channel%d not OK for DNBS", roc_req.chan);
		return -EINVAL;
	}

	return qdf_status_to_os_return(
		ucfg_p2p_roc_req(psoc, &roc_req, cookie));
}

int wlan_cfg80211_cancel_roc(struct wlan_objmgr_vdev *vdev,
		uint64_t cookie)
{
	struct wlan_objmgr_psoc *psoc;

	if (!vdev) {
		osif_err("invalid vdev object");
		return -EINVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("psoc handle is NULL");
		return -EINVAL;
	}

	return qdf_status_to_os_return(
		ucfg_p2p_roc_cancel_req(psoc, cookie));
}

int wlan_cfg80211_mgmt_tx(struct wlan_objmgr_vdev *vdev,
		struct ieee80211_channel *chan, bool offchan,
		unsigned int wait,
		const uint8_t *buf, uint32_t len, bool no_cck,
		bool dont_wait_for_ack, uint64_t *cookie)
{
	struct p2p_mgmt_tx mgmt_tx = {0};
	struct wlan_objmgr_psoc *psoc;
	uint8_t vdev_id;
	uint32_t channel = 0;

	if (!vdev) {
		osif_err("invalid vdev object");
		return -EINVAL;
	}

	if (chan)
		channel = (uint32_t)wlan_freq_to_chan(chan->center_freq);
	else
		osif_debug("NULL chan, set channel to 0");

	psoc = wlan_vdev_get_psoc(vdev);
	vdev_id = wlan_vdev_get_id(vdev);
	if (!psoc) {
		osif_err("psoc handle is NULL");
		return -EINVAL;
	}

	/**
	 * When offchannel time is more than MAX_OFFCHAN_TIME_FOR_DNBS,
	 * allow offchannel only if Do_Not_Switch_Channel is not set.
	 */
	if (wait > MAX_OFFCHAN_TIME_FOR_DNBS) {
		int ret;
		bool ok;

		ret = policy_mgr_is_chan_ok_for_dnbs(
				psoc, wlan_chan_to_freq(channel), &ok);
		if (QDF_IS_STATUS_ERROR(ret)) {
			osif_err("policy_mgr_is_chan_ok_for_dnbs():ret:%d",
				 ret);
			return -EINVAL;
		}
		if (!ok) {
			osif_err("Rejecting mgmt_tx for channel:%d as DNSC is set",
				 channel);
			return -EINVAL;
		}
	}

	mgmt_tx.vdev_id = (uint32_t)vdev_id;
	mgmt_tx.chan = channel;
	mgmt_tx.wait = wait;
	mgmt_tx.len = len;
	mgmt_tx.no_cck = (uint32_t)no_cck;
	mgmt_tx.dont_wait_for_ack = (uint32_t)dont_wait_for_ack;
	mgmt_tx.off_chan = (uint32_t)offchan;
	mgmt_tx.buf = buf;

	return qdf_status_to_os_return(
		ucfg_p2p_mgmt_tx(psoc, &mgmt_tx, cookie));
}

int wlan_cfg80211_mgmt_tx_cancel(struct wlan_objmgr_vdev *vdev,
	uint64_t cookie)
{
	struct wlan_objmgr_psoc *psoc;

	if (!vdev) {
		osif_err("invalid vdev object");
		return -EINVAL;
	}

	psoc = wlan_vdev_get_psoc(vdev);
	if (!psoc) {
		osif_err("psoc handle is NULL");
		return -EINVAL;
	}

	return qdf_status_to_os_return(
		ucfg_p2p_mgmt_tx_cancel(psoc, vdev, cookie));
}
