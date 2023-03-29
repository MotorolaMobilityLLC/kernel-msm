/*
 * Copyright (c) 2012-2020 The Linux Foundation. All rights reserved.
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
 * DOC: wlan_hdd_active_tos.c
 *
 * WLAN active tos functions
 *
 */

#include "osif_sync.h"
#include <wlan_hdd_includes.h>
#include <linux/netdevice.h>
#include <linux/skbuff.h>
#include <linux/etherdevice.h>
#include <linux/if_ether.h>
#include <wlan_hdd_active_tos.h>
#include "wlan_policy_mgr_ucfg.h"
#include "wlan_scan_ucfg_api.h"

#define HDD_AC_BK_BIT                   1
#define HDD_AC_BE_BIT                   2
#define HDD_AC_VI_BIT                   4
#define HDD_AC_VO_BIT                   8

#define HDD_MAX_OFF_CHAN_TIME_FOR_VO    20
#define HDD_MAX_OFF_CHAN_TIME_FOR_VI    20
#define HDD_MAX_OFF_CHAN_TIME_FOR_BE    40
#define HDD_MAX_OFF_CHAN_TIME_FOR_BK    40

#define HDD_MAX_OFF_CHAN_ENTRIES        2

#define HDD_AC_BIT_INDX                 0
#define HDD_DWELL_TIME_INDX             1

static int limit_off_chan_tbl[QCA_WLAN_AC_ALL][HDD_MAX_OFF_CHAN_ENTRIES] = {
		{ HDD_AC_BK_BIT, HDD_MAX_OFF_CHAN_TIME_FOR_BK },
		{ HDD_AC_BE_BIT, HDD_MAX_OFF_CHAN_TIME_FOR_BE },
		{ HDD_AC_VI_BIT, HDD_MAX_OFF_CHAN_TIME_FOR_VI },
		{ HDD_AC_VO_BIT, HDD_MAX_OFF_CHAN_TIME_FOR_VO },
};

const struct nla_policy
wlan_hdd_set_limit_off_channel_param_policy
[QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS] = {.type = NLA_U8 },
	[QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS_START] = {.type = NLA_U8 },
};

/**
 * hdd_set_limit_off_chan_for_tos() - set limit off-channel command parameters
 * @adapter: HDD adapter
 * @tos: type of service
 * @is_tos_active: status of the traffic
 *
 * Return: 0 on success and non zero value on failure
 */

static int
hdd_set_limit_off_chan_for_tos(struct hdd_adapter *adapter,
			       enum qca_wlan_ac_type tos,
			       bool is_tos_active)
{
	int ac_bit;
	struct hdd_context *hdd_ctx;
	uint32_t max_off_chan_time = 0;
	QDF_STATUS status;
	int ret;
	uint8_t def_sys_pref = 0;
	uint32_t rest_conc_time;

	hdd_ctx = WLAN_HDD_GET_CTX(adapter);
	ret = wlan_hdd_validate_context(hdd_ctx);

	if (ret < 0)
		return ret;
	ucfg_policy_mgr_get_sys_pref(hdd_ctx->psoc,
				     &def_sys_pref);

	ac_bit = limit_off_chan_tbl[tos][HDD_AC_BIT_INDX];

	if (is_tos_active)
		adapter->active_ac |= ac_bit;
	else
		adapter->active_ac &= ~ac_bit;

	if (adapter->active_ac) {
		if (adapter->active_ac & HDD_AC_VO_BIT) {
			max_off_chan_time =
				limit_off_chan_tbl[QCA_WLAN_AC_VO][HDD_DWELL_TIME_INDX];
			policy_mgr_set_cur_conc_system_pref(hdd_ctx->psoc,
							    PM_LATENCY);
		} else if (adapter->active_ac & HDD_AC_VI_BIT) {
			max_off_chan_time =
				limit_off_chan_tbl[QCA_WLAN_AC_VI][HDD_DWELL_TIME_INDX];
			policy_mgr_set_cur_conc_system_pref(hdd_ctx->psoc,
							    PM_LATENCY);
		} else {
			/*ignore this command if only BE/BK is active */
			is_tos_active = false;
			policy_mgr_set_cur_conc_system_pref(hdd_ctx->psoc,
							    def_sys_pref);
		}
	} else {
		/* No active tos */
		policy_mgr_set_cur_conc_system_pref(hdd_ctx->psoc,
						    def_sys_pref);
	}

	ucfg_scan_cfg_get_conc_max_resttime(hdd_ctx->psoc, &rest_conc_time);
	status = sme_send_limit_off_channel_params(hdd_ctx->mac_handle,
					adapter->vdev_id,
					is_tos_active,
					max_off_chan_time,
					rest_conc_time,
					true);
	if (!QDF_IS_STATUS_SUCCESS(status)) {
		hdd_err("failed to set limit off chan params");
		ret = -EINVAL;
	}

	return ret;
}

/**
 * __wlan_hdd_cfg80211_set_limit_offchan_param() - set limit off-channel cmd
 * parameters
 * @wiphy: pointer to wireless wiphy structure.
 * @wdev: pointer to wireless_dev structure.
 * @data: pointer to limit off-channel command parameters.
 * @data_len: the length in byte of  limit off-channel command parameters.
 *
 * This is called when application wants to limit the off channel time due to
 * active voip traffic.
 *
 * Return: An error code or 0 on success.
 */
static int
__wlan_hdd_cfg80211_set_limit_offchan_param(struct wiphy *wiphy,
					    struct wireless_dev *wdev,
					    const void *data,
					    int data_len)
{
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS_MAX + 1];
	struct net_device   *dev = wdev->netdev;
	struct hdd_adapter *adapter = WLAN_HDD_GET_PRIV_PTR(dev);
	struct hdd_context *hdd_ctx = wiphy_priv(wiphy);
	int ret = 0;
	uint8_t tos;
	uint8_t tos_status;

	hdd_enter();

	ret = wlan_hdd_validate_context(hdd_ctx);
	if (ret < 0)
		return ret;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS_MAX,
				 data, data_len,
				 wlan_hdd_set_limit_off_channel_param_policy)) {
		hdd_err("Invalid ATTR");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS]) {
		hdd_err("attr tos failed");
		goto fail;
	}

	tos = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS]);
	if (tos >= QCA_WLAN_AC_ALL) {
		hdd_err("tos value %d exceeded Max value %d",
			tos, QCA_WLAN_AC_ALL);
		goto fail;
	}
	hdd_debug("tos %d", tos);

	if (!tb[QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS_START]) {
		hdd_err("attr tos active failed");
		goto fail;
	}
	tos_status = nla_get_u8(tb[QCA_WLAN_VENDOR_ATTR_ACTIVE_TOS_START]);

	hdd_debug("tos status %d", tos_status);
	ret = hdd_set_limit_off_chan_for_tos(adapter, tos, tos_status);

fail:
	return ret;
}

int wlan_hdd_cfg80211_set_limit_offchan_param(struct wiphy *wiphy,
					      struct wireless_dev *wdev,
					      const void *data, int data_len)

{
	struct osif_vdev_sync *vdev_sync;
	int errno;

	errno = osif_vdev_sync_op_start(wdev->netdev, &vdev_sync);
	if (errno)
		return errno;

	errno = __wlan_hdd_cfg80211_set_limit_offchan_param(wiphy, wdev, data,
							    data_len);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

