/*
 * Copyright (c) 2019-2020 The Linux Foundation. All rights reserved.
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
 * DOC:  wlan_hdd_bcn_recv.c
 * Feature for receiving beacons of connected AP and sending select
 * params to upper layer via vendor event
 */

#include <wlan_hdd_includes.h>
#include <net/cfg80211.h>
#include "wlan_osif_priv.h"
#include "qdf_trace.h"
#include "wlan_hdd_main.h"
#include "osif_sync.h"
#include "wlan_hdd_bcn_recv.h"
#include <linux/limits.h>
#include <wlan_hdd_object_manager.h>

#define SET_BIT(value, mask) ((value) |= (1 << (mask)))

#define BOOTTIME QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_BOOTTIME_WHEN_RECEIVED

#ifndef CHAR_BIT
#define CHAR_BIT 8	/* Normally in <limits.h> */
#endif

const struct nla_policy
	beacon_reporting_params_policy
	[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_ACTIVE_REPORTING] = {.type =
								     NLA_FLAG},
	[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_PERIOD] = {.type = NLA_U8},
	[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_DO_NOT_RESUME] = {.type =
								  NLA_FLAG},
};

/**
 * get_beacon_report_data_len() - Calculate length for beacon
 * report to allocate skb buffer
 * @report: beacon report structure
 *
 * Return: skb buffer length
 */
static
int get_beacon_report_data_len(struct wlan_beacon_report *report)
{
	uint32_t data_len = NLMSG_HDRLEN;

	/* QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE */
	data_len += nla_total_size(sizeof(u32));

	/* QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_SSID */
	data_len += nla_total_size(report->ssid.length);

	/* QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_BSSID */
	data_len += nla_total_size(ETH_ALEN);

	/* QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_FREQ */
	data_len += nla_total_size(sizeof(u32));

	/* QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_BI */
	data_len += nla_total_size(sizeof(u16));

	/* QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_TSF */
	data_len += nla_total_size(sizeof(uint64_t));

	/* QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_BOOTTIME_WHEN_RECEIVED */
	data_len += nla_total_size(sizeof(uint64_t));

	return data_len;
}

/**
 * get_pause_ind_data_len() - Calculate skb buffer length
 * @is_disconnected: Connection state
 *
 * Calculate length for pause indication to allocate skb buffer
 *
 * Return: skb buffer length
 */
static int get_pause_ind_data_len(bool is_disconnected)
{
	uint32_t data_len = NLMSG_HDRLEN;

	/* QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE */
	data_len += nla_total_size(sizeof(u32));

	/* QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_PAUSE_REASON */
	data_len += nla_total_size(sizeof(u32));

	/* QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_AUTO_RESUMES */
	if (!is_disconnected)
		data_len += nla_total_size(sizeof(u8));

	return data_len;
}

/**
 * hdd_send_bcn_recv_info() - Send beacon info to userspace for
 * connected AP
 * @hdd_handle: hdd_handle to get hdd_adapter
 * @beacon_report: Required beacon report
 *
 * Send beacon info to userspace for connected AP through a vendor event:
 * QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING.
 */
static QDF_STATUS hdd_send_bcn_recv_info(hdd_handle_t hdd_handle,
					 struct wlan_beacon_report
					 *beacon_report)
{
	struct sk_buff *vendor_event;
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	uint32_t data_len;
	int flags = cds_get_gfp_flags();
	struct hdd_adapter *adapter;

	if (wlan_hdd_validate_context(hdd_ctx))
		return QDF_STATUS_E_FAILURE;

	data_len = get_beacon_report_data_len(beacon_report);

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, beacon_report->vdev_id);
	if (hdd_validate_adapter(adapter))
		return QDF_STATUS_E_FAILURE;

	vendor_event =
		cfg80211_vendor_event_alloc(
			hdd_ctx->wiphy, &(adapter->wdev),
			data_len,
			QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING_INDEX,
			flags);
	if (!vendor_event) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return QDF_STATUS_E_FAILURE;
	}

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE,
			QCA_WLAN_VENDOR_BEACON_REPORTING_OP_BEACON_INFO) ||
	    nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_SSID,
		    beacon_report->ssid.length, beacon_report->ssid.ssid) ||
	    nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_BSSID,
		    ETH_ALEN, beacon_report->bssid.bytes) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_FREQ,
			beacon_report->frequency) ||
	    nla_put_u16(vendor_event,
			QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_BI,
			beacon_report->beacon_interval) ||
	    wlan_cfg80211_nla_put_u64(vendor_event,
				      QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_TSF,
				      beacon_report->time_stamp) ||
	    wlan_cfg80211_nla_put_u64(vendor_event, BOOTTIME,
				      beacon_report->boot_time)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		kfree_skb(vendor_event);
		return QDF_STATUS_E_FAILURE;
	}

	cfg80211_vendor_event(vendor_event, flags);
	return QDF_STATUS_SUCCESS;
}

/**
 * hdd_handle_beacon_reporting_start_op() - Process bcn recv start op
 * @hdd_ctx: Pointer to hdd context
 * @adapter: Pointer to network adapter
 * @active_report: Active reporting flag
 * @nth_value: Beacon report period
 * @do_not_resume: beacon reporting resume after a pause is completed
 *
 * This function process beacon reporting start operation.
 */
static int hdd_handle_beacon_reporting_start_op(struct hdd_context *hdd_ctx,
						struct hdd_adapter *adapter,
						bool active_report,
						uint32_t nth_value,
						bool do_not_resume)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	int errno;
	uint32_t mask = 0;

	if (active_report) {
		/* Register beacon report callback */
		qdf_status =
			sme_register_bcn_report_pe_cb(hdd_ctx->mac_handle,
						      hdd_send_bcn_recv_info);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			hdd_err("bcn recv info cb reg failed = %d", qdf_status);
			errno = qdf_status_to_os_return(qdf_status);
			return errno;
		}

		/* Register pause indication callback */
		qdf_status =
			sme_register_bcn_recv_pause_ind_cb(hdd_ctx->mac_handle,
					hdd_beacon_recv_pause_indication);
		if (QDF_IS_STATUS_ERROR(qdf_status)) {
			hdd_err("pause_ind_cb reg failed = %d", qdf_status);
			errno = qdf_status_to_os_return(qdf_status);
			return errno;
		}
		/* Update Beacon report period in case of active reporting */
		nth_value = 1;
		/*
		 * Set MSB which indicates fw to don't wakeup host in wow
		 * mode in case of active beacon report.
		 */
		mask = (sizeof(uint32_t) * CHAR_BIT) - 1;
		SET_BIT(nth_value, mask);
	}
	/* Handle beacon receive start indication */
	qdf_status = sme_handle_bcn_recv_start(hdd_ctx->mac_handle,
					       adapter->vdev_id, nth_value,
					       do_not_resume);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		hdd_err("bcn rcv start failed with status=%d", qdf_status);
		if (sme_register_bcn_report_pe_cb(hdd_ctx->mac_handle, NULL))
			hdd_err("bcn report cb deregistration failed");
		if (sme_register_bcn_recv_pause_ind_cb(hdd_ctx->mac_handle,
						       NULL))
			hdd_err("bcn pause ind cb deregistration failed");
		errno = qdf_status_to_os_return(qdf_status);
		return errno;
	}

	errno = qdf_status_to_os_return(qdf_status);

	return errno;
}

/**
 * hdd_handle_beacon_reporting_stop_op() - Process bcn recv stop op
 * @hdd_ctx: Pointer to hdd context
 * @adapter: Pointer to network adapter
 *
 * This function process beacon reporting stop operation.
 */
static int hdd_handle_beacon_reporting_stop_op(struct hdd_context *hdd_ctx,
					       struct hdd_adapter *adapter)
{
	QDF_STATUS qdf_status = QDF_STATUS_SUCCESS;
	int errno;

	/* Reset bcn recv start flag */
	sme_stop_beacon_report(hdd_ctx->mac_handle, adapter->vdev_id);

	/* Deregister beacon report callback */
	qdf_status = sme_register_bcn_report_pe_cb(hdd_ctx->mac_handle, NULL);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		hdd_err("Callback de-registration failed = %d", qdf_status);
		errno = qdf_status_to_os_return(qdf_status);
		return errno;
	}

	/* Deregister pause indication callback */
	qdf_status = sme_register_bcn_recv_pause_ind_cb(hdd_ctx->mac_handle,
							NULL);
	if (QDF_IS_STATUS_ERROR(qdf_status)) {
		hdd_err("scan even deregister failed = %d", qdf_status);
		errno = qdf_status_to_os_return(qdf_status);
		return errno;
	}

	if (hdd_adapter_is_connected_sta(adapter))
		/* Add beacon filter */
		if (hdd_add_beacon_filter(adapter)) {
			hdd_err("Beacon filter addition failed");
			return -EINVAL;
		}

	errno = qdf_status_to_os_return(qdf_status);

	return errno;
}

/**
 * __wlan_hdd_cfg80211_bcn_rcv_op() - enable/disable beacon reporting
 * indication
 * @wiphy: Pointer to wireless phy
 * @wdev: Pointer to wireless device
 * @data: Pointer to data
 * @data_len: Length of @data
 *
 * This function is used to enable/disable asynchronous beacon
 * reporting feature using vendor commands.
 *
 * Return: 0 on success, negative errno on failure
 */
static int __wlan_hdd_cfg80211_bcn_rcv_op(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data, int data_len)
{
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_MAX + 1];
	uint32_t bcn_report, nth_value = 1;
	int errno;
	bool active_report, do_not_resume;
	struct wlan_objmgr_vdev *vdev;
	enum scm_scan_status scan_req_status;

	hdd_enter_dev(dev);

	errno = hdd_validate_adapter(adapter);
	if (errno)
		return errno;

	if (adapter->device_mode != QDF_STA_MODE) {
		hdd_err("Command not allowed as device not in STA mode");
		return -EINVAL;
	}

	if (!hdd_conn_is_connected(WLAN_HDD_GET_STATION_CTX_PTR(adapter))) {
		hdd_err("STA not in connected state");
		return -EINVAL;
	}

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return -EINVAL;

	scan_req_status = ucfg_scan_get_pdev_status(wlan_vdev_get_pdev(vdev));
	wlan_objmgr_vdev_release_ref(vdev, WLAN_OSIF_ID);

	if (scan_req_status != SCAN_NOT_IN_PROGRESS) {
		hdd_debug("Scan in progress: %d, bcn rpt start OP not allowed",
			  scan_req_status);
		return -EBUSY;
	}

	errno =
	   wlan_cfg80211_nla_parse(tb,
				   QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_MAX,
				   data,
				   data_len, beacon_reporting_params_policy);
	if (errno) {
		hdd_err("Failed to parse the beacon reporting params %d",
			errno);
		return errno;
	}

	/* Parse and fetch OP Type */
	if (!tb[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE]) {
		hdd_err("attr beacon report OP type failed");
		return -EINVAL;
	}
	bcn_report =
		nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE]);
	hdd_debug("Bcn Report: OP type:%d", bcn_report);

	switch (bcn_report) {
	case QCA_WLAN_VENDOR_BEACON_REPORTING_OP_START:
		active_report =
			!!tb[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_ACTIVE_REPORTING];
		hdd_debug("attr active_report %d", active_report);

		do_not_resume =
			!!tb[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_DO_NOT_RESUME];
		hdd_debug("Attr beacon report do not resume %d", do_not_resume);

		if (tb[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_PERIOD])
			nth_value =
				nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_PERIOD]);
		hdd_debug("Beacon Report: Period: %d", nth_value);

		if (sme_is_beacon_report_started(hdd_ctx->mac_handle,
						 adapter->vdev_id)) {
			hdd_debug("Start cmd already in progress, issue the stop to FW, before new start");
			if (hdd_handle_beacon_reporting_stop_op(hdd_ctx,
								adapter)) {
				hdd_err("Failed to stop the beacon reporting before starting new start");
				return -EAGAIN;
			}
		}
		errno = hdd_handle_beacon_reporting_start_op(hdd_ctx,
							     adapter,
							     active_report,
							     nth_value,
							     do_not_resume);
		if (errno) {
			hdd_err("Failed to start beacon reporting %d,", errno);
			break;
		}
		break;
	case QCA_WLAN_VENDOR_BEACON_REPORTING_OP_STOP:
		if (sme_is_beacon_report_started(hdd_ctx->mac_handle,
						 adapter->vdev_id)) {
			errno = hdd_handle_beacon_reporting_stop_op(hdd_ctx,
								    adapter);
			if (errno) {
				hdd_err("Failed to stop the beacon report, %d",
					errno);
			}
		} else {
			hdd_err_rl("BCN_RCV_STOP rej as no START CMD active");
			errno = -EINVAL;
		}
		break;
	default:
		hdd_debug("Invalid bcn report type %d", bcn_report);
	}

	return errno;
}

void hdd_beacon_recv_pause_indication(hdd_handle_t hdd_handle,
				      uint8_t vdev_id,
				      enum scan_event_type type,
				      bool is_disconnected)
{
	struct hdd_context *hdd_ctx = hdd_handle_to_context(hdd_handle);
	struct hdd_adapter *adapter;
	struct sk_buff *vendor_event;
	uint32_t data_len;
	int flags;
	uint32_t abort_reason;
	bool do_not_resume;

	if (wlan_hdd_validate_context(hdd_ctx))
		return;

	adapter = hdd_get_adapter_by_vdev(hdd_ctx, vdev_id);
	if (hdd_validate_adapter(adapter))
		return;

	data_len = get_pause_ind_data_len(is_disconnected);
	flags = cds_get_gfp_flags();

	vendor_event =
		cfg80211_vendor_event_alloc(
			hdd_ctx->wiphy, &(adapter->wdev),
			data_len,
			QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING_INDEX,
			flags);
	if (!vendor_event) {
		hdd_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	do_not_resume =
		sme_is_beacon_reporting_do_not_resume(hdd_ctx->mac_handle,
						      adapter->vdev_id);

	if (is_disconnected) {
		abort_reason =
		     QCA_WLAN_VENDOR_BEACON_REPORTING_PAUSE_REASON_DISCONNECTED;
		/* Deregister callbacks and Reset bcn recv start flag */
		if (sme_is_beacon_report_started(hdd_ctx->mac_handle,
						 adapter->vdev_id))
			hdd_handle_beacon_reporting_stop_op(hdd_ctx,
							    adapter);
	} else {
		/*
		 * In case of scan, Check that auto resume of beacon reporting
		 * is allowed or not.
		 * If not allowed:
		 * Deregister callbacks and Reset bcn recv start flag in order
		 * to make sure host should not send beacon report to userspace
		 * further.
		 * If Auto resume allowed:
		 * Send pause indication to userspace and continue sending
		 * connected AP's beacon to userspace.
		 */
		if (do_not_resume)
			hdd_handle_beacon_reporting_stop_op(hdd_ctx,
							    adapter);

		switch (type) {
		case SCAN_EVENT_TYPE_STARTED:
			abort_reason =
		     QCA_WLAN_VENDOR_BEACON_REPORTING_PAUSE_REASON_SCAN_STARTED;
			break;
		default:
			abort_reason =
		      QCA_WLAN_VENDOR_BEACON_REPORTING_PAUSE_REASON_UNSPECIFIED;
		}
	}
	/* Send vendor event to user space to inform ABORT */
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_OP_TYPE,
			QCA_WLAN_VENDOR_BEACON_REPORTING_OP_PAUSE) ||
	    nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_PAUSE_REASON,
			abort_reason)) {
		hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
		kfree_skb(vendor_event);
		return;
	}

	/*
	 * Send auto resume flag to user space to specify the driver will
	 * automatically resume reporting beacon events only in case of
	 * pause indication due to scan started.
	 * If do_not_resume flag is set in the recent
	 * QCA_WLAN_VENDOR_BEACON_REPORTING_OP_START command, then in the
	 * subsequent QCA_WLAN_VENDOR_BEACON_REPORTING_OP_PAUSE event (if any)
	 * the QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_AUTO_RESUMES shall not be
	 * set by the driver.
	 */
	if (!is_disconnected && !do_not_resume)
		if (nla_put_flag(vendor_event,
			QCA_WLAN_VENDOR_ATTR_BEACON_REPORTING_AUTO_RESUMES)) {
			hdd_err("QCA_WLAN_VENDOR_ATTR put fail");
			kfree_skb(vendor_event);
			return;
		}

	cfg80211_vendor_event(vendor_event, flags);
}

int wlan_hdd_cfg80211_bcn_rcv_op(struct wiphy *wiphy,
				 struct wireless_dev *wdev,
				 const void *data, int data_len)
{
	int errno;
	struct osif_vdev_sync *vdev_sync;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_bcn_rcv_op(wiphy, wdev,
					       data, data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}
