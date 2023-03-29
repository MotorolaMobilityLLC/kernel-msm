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
 * DOC: wlan_hdd_spectral_scan.c
 *
 * WLAN Host Device Driver Spectral Scan Implementation
 */

#include <linux/version.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <net/cfg80211.h>
#include "osif_sync.h"
#include "wlan_hdd_includes.h"
#include "cds_api.h"
#include "ani_global.h"
#include "wlan_cfg80211_spectral.h"
#include "wlan_hdd_spectralscan.h"
#include <wlan_spectral_ucfg_api.h>
#include "wma.h"
#include "wlan_hdd_object_manager.h"
#ifdef CNSS_GENL
#include <net/cnss_nl.h>
#endif

/**
 * __wlan_hdd_cfg80211_spectral_scan_start() - start spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function starts spectral scan
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_spectral_scan_start(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	ret = hdd_validate_adapter(adapter);
	if (ret)
		return ret;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("can't get vdev");
		return -EINVAL;
	}
	wlan_spectral_update_rx_chainmask(adapter);
	ret = wlan_cfg80211_spectral_scan_config_and_start(
						wiphy, hdd_ctx->pdev,
						vdev, data, data_len);
	hdd_objmgr_put_vdev(vdev);
	hdd_exit();

	return ret;
}

/**
 * __wlan_hdd_cfg80211_spectral_scan_stop() - stop spectral scan
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function stops spectral scan
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_spectral_scan_stop(struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	ret = hdd_validate_adapter(adapter);
	if (ret)
		return ret;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("can't get vdev");
		return -EINVAL;
	}
	ret = wlan_cfg80211_spectral_scan_stop(wiphy, hdd_ctx->pdev,
					       vdev, data, data_len);
	hdd_objmgr_put_vdev(vdev);
	hdd_exit();

	return ret;
}

/**
 * __wlan_hdd_cfg80211_spectral_scan_get_config() - spectral scan get config
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function to get the spectral scan configuration
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_spectral_scan_get_config(
						struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	ret = hdd_validate_adapter(adapter);
	if (ret)
		return ret;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("can't get vdev");
		return -EINVAL;
	}
	ret = wlan_cfg80211_spectral_scan_get_config(wiphy, hdd_ctx->pdev,
						     vdev, data, data_len);
	hdd_objmgr_put_vdev(vdev);
	hdd_exit();

	return ret;
}

/**
 * __wlan_hdd_cfg80211_spectral_scan_get_diag_stats() - get diag stats
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function gets the spectral scan diag stats
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_spectral_scan_get_diag_stats(
						struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	ret = hdd_validate_adapter(adapter);
	if (ret)
		return ret;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("can't get vdev");
		return -EINVAL;
	}
	ret = wlan_cfg80211_spectral_scan_get_diag_stats(
						wiphy, hdd_ctx->pdev,
						vdev, data, data_len);
	hdd_objmgr_put_vdev(vdev);
	hdd_exit();

	return ret;
}

/**
 * __wlan_hdd_cfg80211_spectral_scan_get_cap_info() - get spectral caps
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function gets spectral scan configured capabilities
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_spectral_scan_get_cap_info(
						struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	ret = hdd_validate_adapter(adapter);
	if (ret)
		return ret;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("can't get vdev");
		return -EINVAL;
	}
	ret = wlan_cfg80211_spectral_scan_get_cap(wiphy, hdd_ctx->pdev,
						  vdev, data, data_len);
	hdd_objmgr_put_vdev(vdev);
	hdd_exit();

	return ret;
}

/*
 * __wlan_hdd_cfg80211_spectral_scan_get_status() - get spectral scan
 * status
 * @wiphy:    WIPHY structure pointer
 * @wdev:     Wireless device structure pointer
 * @data:     Pointer to the data received
 * @data_len: Length of the data received
 *
 * This function gets current status of spectral scan
 *
 * Return: 0 on success and errno on failure
 */
static int __wlan_hdd_cfg80211_spectral_scan_get_status(
						struct wiphy *wiphy,
						struct wireless_dev *wdev,
						const void *data,
						int data_len)
{
	int ret;
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	struct net_device *dev = wdev->netdev;
	struct hdd_adapter *adapter;
	struct wlan_objmgr_vdev *vdev;

	hdd_enter();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return ret;

	if (QDF_GLOBAL_FTM_MODE == hdd_get_conparam()) {
		hdd_err("Command not allowed in FTM mode");
		return -EPERM;
	}

	adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	ret = hdd_validate_adapter(adapter);
	if (ret)
		return ret;

	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev) {
		hdd_err("can't get vdev");
		return -EINVAL;
	}
	ret = wlan_cfg80211_spectral_scan_get_status(wiphy, hdd_ctx->pdev,
						     vdev, data, data_len);
	hdd_objmgr_put_vdev(vdev);
	hdd_exit();

	return ret;
}

int wlan_hdd_cfg80211_spectral_scan_start(struct wiphy *wiphy,
					  struct wireless_dev *wdev,
					  const void *data,
					  int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_spectral_scan_start(wiphy, wdev,
							data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

int wlan_hdd_cfg80211_spectral_scan_stop(struct wiphy *wiphy,
					 struct wireless_dev *wdev,
					 const void *data,
					 int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_spectral_scan_stop(wiphy, wdev,
						       data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

int wlan_hdd_cfg80211_spectral_scam_get_config(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_spectral_scan_get_config(wiphy, wdev,
							     data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

int wlan_hdd_cfg80211_spectral_scan_get_diag_stats(struct wiphy *wiphy,
						   struct wireless_dev *wdev,
						   const void *data,
						   int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_spectral_scan_get_diag_stats(wiphy, wdev,
								 data,
								 data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

int wlan_hdd_cfg80211_spectral_scan_get_cap_info(struct wiphy *wiphy,
						 struct wireless_dev *wdev,
						 const void *data,
						 int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_spectral_scan_get_cap_info(wiphy, wdev,
							       data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

int wlan_hdd_cfg80211_spectral_scan_get_status(struct wiphy *wiphy,
					       struct wireless_dev *wdev,
					       const void *data,
					       int data_len)
{
	struct osif_psoc_sync *psoc_sync;
	int errno;

	errno = osif_psoc_sync_op_start(wiphy_dev(wiphy), &psoc_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_spectral_scan_get_status(wiphy, wdev,
							     data, data_len);

	osif_psoc_sync_op_stop(psoc_sync);

	return errno;
}

void hdd_spectral_register_to_dbr(struct hdd_context *hdd_ctx)
{
	ucfg_spectral_register_to_dbr(hdd_ctx->pdev);
}

#if defined(CNSS_GENL) && defined(WLAN_CONV_SPECTRAL_ENABLE)
static void send_spectral_scan_reg_rsp_msg(struct hdd_context *hdd_ctx)
{
	struct sk_buff *skb;
	struct nlmsghdr *nlh;
	struct spectral_scan_msg_v *rsp_msg;
	int err;

	skb = alloc_skb(NLMSG_SPACE(sizeof(struct spectral_scan_msg_v)),
			GFP_KERNEL);
	if (!skb) {
		hdd_err("Skb allocation failed");
		return;
	}

	nlh = (struct nlmsghdr *)skb->data;
	nlh->nlmsg_pid = 0;
	nlh->nlmsg_flags = 0;
	nlh->nlmsg_seq = 0;
	nlh->nlmsg_type = WLAN_NL_MSG_SPECTRAL_SCAN;

	rsp_msg = NLMSG_DATA(nlh);
	rsp_msg->msg_type = SPECTRAL_SCAN_REGISTER_RSP;
	rsp_msg->pid = hdd_ctx->sscan_pid;
	ucfg_spectral_get_version(hdd_ctx->pdev, &rsp_msg->version,
				  &rsp_msg->sub_version);

	nlh->nlmsg_len = NLMSG_LENGTH(sizeof(struct spectral_scan_msg_v));
	skb_put(skb, NLMSG_SPACE(sizeof(struct spectral_scan_msg_v)));

	hdd_info("sending App Reg Response to process pid %d",
			hdd_ctx->sscan_pid);

	err = nl_srv_ucast(skb, hdd_ctx->sscan_pid, MSG_DONTWAIT,
			WLAN_NL_MSG_SPECTRAL_SCAN, CLD80211_MCGRP_OEM_MSGS);

	if (err < 0)
		hdd_err("SPECTRAL: failed to send to spectral scan reg"
			" response");
}

/**
 * __spectral_scan_msg_handler() - API to handle spectral scan command
 * @data: Data received
 * @data_len: length of the data received
 * @ctx: Pointer to stored context
 * @pid: Process ID
 *
 * API to handle spectral scan commands from user space
 *
 * Return: None
 */
static void __spectral_scan_msg_handler(const void *data, int data_len,
					void *ctx, int pid)
{
	struct spectral_scan_msg *ss_msg = NULL;
	struct nlattr *tb[CLD80211_ATTR_MAX + 1];
	struct hdd_context *hdd_ctx;
	int ret;

	hdd_ctx = (struct hdd_context *)cds_get_context(QDF_MODULE_ID_HDD);
	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret)
		return;

	/*
	 * audit note: it is ok to pass a NULL policy here since only
	 * one attribute is parsed and it is explicitly validated
	 */
	if (wlan_cfg80211_nla_parse(tb, CLD80211_ATTR_MAX, data,
				    data_len, NULL)) {
		hdd_err("nla parse fails");
		return;
	}

	if (!tb[CLD80211_ATTR_DATA]) {
		hdd_err("attr VENDOR_DATA fails");
		return;
	}

	if (nla_len(tb[CLD80211_ATTR_DATA]) < sizeof(*ss_msg)) {
		hdd_err_rl("Invalid length for ATTR_DATA");
		return;
	}

	ss_msg = (struct spectral_scan_msg *)nla_data(tb[CLD80211_ATTR_DATA]);

	if (!ss_msg) {
		hdd_err("data NULL");
		return;
	}

	switch (ss_msg->msg_type) {
	case SPECTRAL_SCAN_REGISTER_REQ:
		hdd_ctx->sscan_pid = ss_msg->pid;
		hdd_debug("spectral scan application registered, pid=%d",
				 hdd_ctx->sscan_pid);
		send_spectral_scan_reg_rsp_msg(hdd_ctx);
		ucfg_spectral_scan_set_ppid(hdd_ctx->pdev,
					    hdd_ctx->sscan_pid);
		break;
	default:
		hdd_warn("invalid message type %d", ss_msg->msg_type);
		break;
	}
}

static void spectral_scan_msg_handler(const void *data, int data_len,
				      void *ctx, int pid)
{
	struct device *dev = ctx;
	struct osif_psoc_sync *psoc_sync;

	if (osif_psoc_sync_op_start(dev, &psoc_sync))
		return;

	__spectral_scan_msg_handler(data, data_len, ctx, pid);

	osif_psoc_sync_op_stop(psoc_sync);
}

void spectral_scan_activate_service(struct hdd_context *hdd_ctx)
{
	register_cld_cmd_cb(WLAN_NL_MSG_SPECTRAL_SCAN,
			    spectral_scan_msg_handler, hdd_ctx->parent_dev);
}

void spectral_scan_deactivate_service(void)
{
	deregister_cld_cmd_cb(WLAN_NL_MSG_SPECTRAL_SCAN);
}

QDF_STATUS wlan_spectral_update_rx_chainmask(struct hdd_adapter *adapter)
{
	uint32_t chainmask_2g = 0;
	uint32_t chainmask_5g = 0;
	uint32_t chainmask;
	uint8_t home_chan;
	uint8_t pdev_id;
	struct wlan_objmgr_vdev *vdev;

	home_chan = hdd_get_adapter_home_channel(adapter);
	pdev_id = wlan_objmgr_pdev_get_pdev_id(adapter->hdd_ctx->pdev);
	wma_get_rx_chainmask(pdev_id, &chainmask_2g, &chainmask_5g);
	chainmask = home_chan > MAX_24GHZ_CHANNEL ? chainmask_5g : chainmask_2g;
	vdev = hdd_objmgr_get_vdev(adapter);
	if (!vdev)
		return QDF_STATUS_E_FAILURE;
	wlan_vdev_mlme_set_rxchainmask(vdev, chainmask);
	hdd_objmgr_put_vdev(vdev);

	return QDF_STATUS_SUCCESS;
}
#endif
