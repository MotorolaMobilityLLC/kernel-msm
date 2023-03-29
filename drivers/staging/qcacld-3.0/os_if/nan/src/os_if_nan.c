/*
 * Copyright (c) 2016-2020 The Linux Foundation. All rights reserved.
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
 * DOC: defines nan component os interface APIs
 */

#include "osif_sync.h"
#include "qdf_str.h"
#include "qdf_trace.h"
#include "qdf_types.h"
#include "os_if_nan.h"
#include "wlan_nan_api.h"
#include "nan_ucfg_api.h"
#include "wlan_osif_priv.h"
#include <net/cfg80211.h>
#include "wlan_cfg80211.h"
#include "wlan_objmgr_psoc_obj.h"
#include "wlan_objmgr_pdev_obj.h"
#include "wlan_objmgr_vdev_obj.h"
#include "wlan_utility.h"
#include "wlan_osif_request_manager.h"
#include "wlan_mlme_ucfg_api.h"

#define NAN_CMD_MAX_SIZE 2048

/* NLA policy */
const struct nla_policy nan_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NAN_PARAMS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_NAN_CMD_DATA] = {
						.type = NLA_BINARY,
						.len = NAN_CMD_MAX_SIZE
	},
	[QCA_WLAN_VENDOR_ATTR_NAN_SUBCMD_TYPE] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NAN_DISC_24GHZ_BAND_FREQ] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NAN_DISC_5GHZ_BAND_FREQ] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
};

/* NLA policy */
const struct nla_policy vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX + 1] = {
	[QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID] = {
						.type = NLA_U16,
						.len = sizeof(uint16_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR] = {
						.type = NLA_NUL_STRING,
						.len = IFNAMSIZ - 1
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR] =
						VENDOR_NLA_POLICY_MAC_ADDR,
	[QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_SECURITY] = {
						.type = NLA_U16,
						.len = sizeof(uint16_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO] = {
						.type = NLA_BINARY,
						.len = NDP_APP_INFO_LEN
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR] = {
						.type = NLA_BINARY,
						.len = QDF_MAC_ADDR_SIZE
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY] = {
						.type = NLA_BINARY,
						.len = NDP_NUM_INSTANCE_ID
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_CONFIG] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_CSID] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_PMK] = {
						.type = NLA_BINARY,
						.len = NDP_PMK_LEN
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_SCID] = {
						.type = NLA_BINARY,
						.len = NDP_SCID_BUF_LEN
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_PASSPHRASE] = {
						.type = NLA_BINARY,
						.len = NAN_PASSPHRASE_MAX_LEN
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_NAME] = {
						.type = NLA_BINARY,
						.len = NAN_MAX_SERVICE_NAME_LEN
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO] = {
						.type = NLA_BINARY,
						.len = NAN_CH_INFO_MAX_LEN
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_NSS] = {
						.type = NLA_U32,
						.len = sizeof(uint32_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR] = {
						.type = NLA_EXACT_LEN,
						.len = QDF_IPV6_ADDR_SIZE
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PORT] = {
						.type = NLA_U16,
						.len = sizeof(uint16_t)
	},
	[QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PROTOCOL] = {
						.type = NLA_U8,
						.len = sizeof(uint8_t)
	},
};

/**
 * os_if_get_ndi_vdev_by_ifname_cb() - callback function to return vdev object
 * from psoc matching given interface name
 * @psoc: psoc object
 * @obj: object used to iterate the callback function
 * @arg: return argument which will be filled by the function
 *
 * Return : NULL
 */
static void os_if_get_ndi_vdev_by_ifname_cb(struct wlan_objmgr_psoc *psoc,
					    void *obj, void *arg)
{
	struct wlan_objmgr_vdev *vdev = obj;
	struct ndi_find_vdev_filter *filter = arg;
	struct vdev_osif_priv *osif_priv;

	if (filter->found_vdev)
		return;

	wlan_vdev_obj_lock(vdev);

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		wlan_vdev_obj_unlock(vdev);
		return;
	}

	if (!osif_priv->wdev) {
		wlan_vdev_obj_unlock(vdev);
		return;
	}

	if (!qdf_str_cmp(osif_priv->wdev->netdev->name, filter->ifname))
		filter->found_vdev = vdev;

	wlan_vdev_obj_unlock(vdev);
}

/**
 * os_if_get_ndi_vdev_by_ifname() - function to return vdev object from psoc
 * matching given interface name
 * @psoc: psoc object
 * @ifname: interface name
 *
 * This function returns vdev object from psoc by interface name. If found this
 * will also take reference with given ref_id
 *
 * Return : vdev object if found, NULL otherwise
 */
static struct wlan_objmgr_vdev *
os_if_get_ndi_vdev_by_ifname(struct wlan_objmgr_psoc *psoc, char *ifname)
{
	QDF_STATUS status;
	struct ndi_find_vdev_filter filter = {0};

	filter.ifname = ifname;
	wlan_objmgr_iterate_obj_list(psoc, WLAN_VDEV_OP,
				     os_if_get_ndi_vdev_by_ifname_cb,
				     &filter, 0, WLAN_NAN_ID);

	if (!filter.found_vdev)
		return NULL;

	status = wlan_objmgr_vdev_try_get_ref(filter.found_vdev, WLAN_NAN_ID);
	if (QDF_IS_STATUS_ERROR(status))
		return NULL;

	return filter.found_vdev;
}

/**
 * os_if_ndi_get_if_name() - get vdev's interface name
 * @vdev: VDEV object
 *
 * API to get vdev's interface name
 *
 * Return: vdev's interface name
 */
static const uint8_t *os_if_ndi_get_if_name(struct wlan_objmgr_vdev *vdev)
{
	struct vdev_osif_priv *osif_priv;

	wlan_vdev_obj_lock(vdev);

	osif_priv = wlan_vdev_get_ospriv(vdev);
	if (!osif_priv) {
		wlan_vdev_obj_unlock(vdev);
		return NULL;
	}

	if (!osif_priv->wdev) {
		wlan_vdev_obj_unlock(vdev);
		return NULL;
	}

	wlan_vdev_obj_unlock(vdev);

	return osif_priv->wdev->netdev->name;
}

static int __os_if_nan_process_ndi_create(struct wlan_objmgr_psoc *psoc,
					  char *iface_name,
					  struct nlattr **tb)
{
	int ret;
	QDF_STATUS status;
	uint16_t transaction_id;
	struct wlan_objmgr_vdev *nan_vdev;
	struct nan_callbacks cb_obj;

	osif_debug("enter");

	nan_vdev = os_if_get_ndi_vdev_by_ifname(psoc, iface_name);
	if (nan_vdev) {
		osif_err("NAN data interface %s is already present",
			 iface_name);
		wlan_objmgr_vdev_release_ref(nan_vdev, WLAN_NAN_ID);
		return -EEXIST;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		osif_err("transaction id is unavailable");
		return -EINVAL;
	}
	transaction_id =
		nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);

	status = ucfg_nan_get_callbacks(psoc, &cb_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Couldn't get ballback object");
		return -EINVAL;
	}

	ret = cb_obj.ndi_open(iface_name);
	if (ret) {
		osif_err("ndi_open failed");
		return ret;
	}

	return cb_obj.ndi_start(iface_name, transaction_id);
}

static int
osif_nla_str(struct nlattr **tb, size_t attr_id, char **out_str)
{
	if (!tb || !tb[attr_id])
		return -EINVAL;

	*out_str = nla_data(tb[attr_id]);

	return 0;
}

static int
osif_device_from_psoc(struct wlan_objmgr_psoc *psoc, struct device **out_dev)
{
	qdf_device_t qdf_dev;

	if (!psoc)
		return -EINVAL;

	qdf_dev = wlan_psoc_get_qdf_dev(psoc);
	if (!qdf_dev || !qdf_dev->dev)
		return -EINVAL;

	*out_dev = qdf_dev->dev;

	return 0;
}

static int osif_net_dev_from_vdev(struct wlan_objmgr_vdev *vdev,
				  struct net_device **out_net_dev)
{
	struct vdev_osif_priv *priv;

	if (!vdev)
		return -EINVAL;

	priv = wlan_vdev_get_ospriv(vdev);
	if (!priv || !priv->wdev || !priv->wdev->netdev)
		return -EINVAL;

	*out_net_dev = priv->wdev->netdev;

	return 0;
}

static int osif_net_dev_from_ifname(struct wlan_objmgr_psoc *psoc,
				    char *iface_name,
				    struct net_device **out_net_dev)
{
	struct wlan_objmgr_vdev *vdev;
	struct net_device *net_dev;
	int errno;

	vdev = os_if_get_ndi_vdev_by_ifname(psoc, iface_name);
	if (!vdev)
		return -EINVAL;

	errno = osif_net_dev_from_vdev(vdev, &net_dev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_NAN_ID);

	if (errno)
		return errno;

	*out_net_dev = net_dev;

	return 0;
}

static int os_if_nan_process_ndi_create(struct wlan_objmgr_psoc *psoc,
					struct nlattr **tb)
{
	struct device *dev;
	struct net_device *net_dev;
	struct osif_vdev_sync *vdev_sync;
	char *ifname;
	int errno;

	errno = osif_nla_str(tb, QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR, &ifname);
	if (errno)
		return errno;

	errno = osif_device_from_psoc(psoc, &dev);
	if (errno)
		return errno;

	errno = osif_vdev_sync_create_and_trans(dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __os_if_nan_process_ndi_create(psoc, ifname, tb);
	if (errno)
		goto destroy_sync;

	errno = osif_net_dev_from_ifname(psoc, ifname, &net_dev);
	if (errno)
		goto destroy_sync;

	osif_vdev_sync_register(net_dev, vdev_sync);
	osif_vdev_sync_trans_stop(vdev_sync);

	return 0;

destroy_sync:
	osif_vdev_sync_trans_stop(vdev_sync);
	osif_vdev_sync_destroy(vdev_sync);

	return errno;
}

static int __os_if_nan_process_ndi_delete(struct wlan_objmgr_psoc *psoc,
					  char *iface_name,
					  struct nlattr **tb)
{
	uint8_t vdev_id;
	QDF_STATUS status;
	uint32_t num_peers;
	uint16_t transaction_id;
	struct nan_callbacks cb_obj;
	struct wlan_objmgr_vdev *nan_vdev = NULL;

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		osif_err("Transaction id is unavailable");
		return -EINVAL;
	}

	nan_vdev = os_if_get_ndi_vdev_by_ifname(psoc, iface_name);
	if (!nan_vdev) {
		osif_debug("Nan datapath interface is not present");
		return -EINVAL;
	}

	transaction_id =
		nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);
	vdev_id = wlan_vdev_get_id(nan_vdev);
	num_peers = ucfg_nan_get_active_peers(nan_vdev);
	/*
	 * os_if_get_ndi_vdev_by_ifname increments ref count
	 * decrement here since vdev returned by that api is not used any more
	 */
	wlan_objmgr_vdev_release_ref(nan_vdev, WLAN_NAN_ID);

	/* check if there are active peers on the adapter */
	if (num_peers)
		osif_err("NDP peers active: %d, active NDPs may not be terminated",
			 num_peers);

	status = ucfg_nan_get_callbacks(psoc, &cb_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Couldn't get ballback object");
		return -EINVAL;
	}

	return cb_obj.ndi_delete(vdev_id, iface_name, transaction_id);
}

static int os_if_nan_process_ndi_delete(struct wlan_objmgr_psoc *psoc,
					struct nlattr **tb)
{
	struct net_device *net_dev;
	struct osif_vdev_sync *vdev_sync;
	char *ifname;
	int errno;

	errno = osif_nla_str(tb, QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR, &ifname);
	if (errno)
		return errno;

	errno = osif_net_dev_from_ifname(psoc, ifname, &net_dev);
	if (errno)
		return errno;

	errno = osif_vdev_sync_trans_start_wait(net_dev, &vdev_sync);
	if (errno)
		return errno;

	osif_vdev_sync_unregister(net_dev);
	osif_vdev_sync_wait_for_ops(vdev_sync);

	errno = __os_if_nan_process_ndi_delete(psoc, ifname, tb);
	if (errno)
		goto reregister;

	osif_vdev_sync_trans_stop(vdev_sync);
	osif_vdev_sync_destroy(vdev_sync);

	return 0;

reregister:
	osif_vdev_sync_register(net_dev, vdev_sync);
	osif_vdev_sync_trans_stop(vdev_sync);

	return errno;
}

/**
 * os_if_nan_parse_security_params() - parse vendor attributes for security
 * params.
 * @tb: parsed NL attribute list
 * @ncs_sk_type: out parameter to populate ncs_sk_type
 * @pmk: out parameter to populate pmk
 * @passphrase: out parameter to populate passphrase
 * @service_name: out parameter to populate service_name
 *
 * Return:  0 on success or error code on failure
 */
static int os_if_nan_parse_security_params(struct nlattr **tb,
			uint32_t *ncs_sk_type, struct nan_datapath_pmk *pmk,
			struct ndp_passphrase *passphrase,
			struct ndp_service_name *service_name)
{
	if (!ncs_sk_type || !pmk || !passphrase || !service_name) {
		osif_err("out buffers for one ore more parameters is null");
		return -EINVAL;
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_CSID]) {
		*ncs_sk_type =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_NDP_CSID]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_PMK]) {
		pmk->pmk_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_NDP_PMK]);
		qdf_mem_copy(pmk->pmk,
			     nla_data(tb[QCA_WLAN_VENDOR_ATTR_NDP_PMK]),
			     pmk->pmk_len);
		osif_err("pmk len: %d", pmk->pmk_len);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_ERROR,
				   pmk->pmk, pmk->pmk_len);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_PASSPHRASE]) {
		passphrase->passphrase_len =
			nla_len(tb[QCA_WLAN_VENDOR_ATTR_NDP_PASSPHRASE]);
		qdf_mem_copy(passphrase->passphrase,
			     nla_data(tb[QCA_WLAN_VENDOR_ATTR_NDP_PASSPHRASE]),
			     passphrase->passphrase_len);
		osif_err("passphrase len: %d", passphrase->passphrase_len);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_ERROR,
				   passphrase->passphrase,
				   passphrase->passphrase_len);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_NAME]) {
		service_name->service_name_len =
			nla_len(tb[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_NAME]);
		qdf_mem_copy(service_name->service_name,
			     nla_data(
				     tb[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_NAME]),
			     service_name->service_name_len);
		osif_err("service_name len: %d",
			 service_name->service_name_len);
		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_ERROR,
				   service_name->service_name,
				   service_name->service_name_len);
	}

	return 0;
}

/**
 * __os_if_nan_process_ndp_initiator_req() - NDP initiator request handler
 * @ctx: hdd context
 * @tb: parsed NL attribute list
 *
 * tb will contain following vendor attributes:
 * QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR
 * QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID
 * QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL - optional
 * QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_CONFIG
 * QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID
 * QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR
 * QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO - optional
 * QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS - optional
 * QCA_WLAN_VENDOR_ATTR_NDP_PMK - optional
 * QCA_WLAN_VENDOR_ATTR_NDP_CSID - optional
 * QCA_WLAN_VENDOR_ATTR_NDP_PASSPHRASE - optional
 * QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_NAME - optional
 *
 * Return:  0 on success or error code on failure
 */
static int __os_if_nan_process_ndp_initiator_req(struct wlan_objmgr_psoc *psoc,
						 char *iface_name,
						 struct nlattr **tb)
{
	int ret = 0;
	QDF_STATUS status;
	enum nan_datapath_state state;
	struct wlan_objmgr_vdev *nan_vdev;
	struct nan_datapath_initiator_req req = {0};

	nan_vdev = os_if_get_ndi_vdev_by_ifname(psoc, iface_name);
	if (!nan_vdev) {
		osif_err("NAN data interface %s not available", iface_name);
		return -EINVAL;
	}

	if (nan_vdev->vdev_mlme.vdev_opmode != QDF_NDI_MODE) {
		osif_err("Interface found is not NDI");
		ret = -EINVAL;
		goto initiator_req_failed;
	}

	state = ucfg_nan_get_ndi_state(nan_vdev);
	if (state == NAN_DATA_NDI_DELETED_STATE ||
	    state == NAN_DATA_NDI_DELETING_STATE ||
	    state == NAN_DATA_NDI_CREATING_STATE) {
		osif_err("Data request not allowed in NDI current state: %d",
			 state);
		ret = -EINVAL;
		goto initiator_req_failed;
	}

	if (!ucfg_nan_is_sta_ndp_concurrency_allowed(psoc, nan_vdev)) {
		osif_err("NDP creation not allowed");
		ret = -EOPNOTSUPP;
		goto initiator_req_failed;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		osif_err("Transaction ID is unavailable");
		ret = -EINVAL;
		goto initiator_req_failed;
	}
	req.transaction_id =
		nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL]) {
		req.channel = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL]);

		if (tb[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_CONFIG]) {
			req.channel_cfg = nla_get_u32(
				tb[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_CONFIG]);
		} else {
			osif_err("Channel config is unavailable");
			ret = -EINVAL;
			goto initiator_req_failed;
		}
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID]) {
		osif_err("NDP service instance ID is unavailable");
		ret = -EINVAL;
		goto initiator_req_failed;
	}
	req.service_instance_id =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID]);

	qdf_mem_copy(req.self_ndi_mac_addr.bytes,
		     wlan_vdev_mlme_get_macaddr(nan_vdev), QDF_MAC_ADDR_SIZE);

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR]) {
		osif_err("NDI peer discovery mac addr is unavailable");
		ret = -EINVAL;
		goto initiator_req_failed;
	}
	qdf_mem_copy(req.peer_discovery_mac_addr.bytes,
		     nla_data(
			  tb[QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR]),
		     QDF_MAC_ADDR_SIZE);

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]) {
		req.ndp_info.ndp_app_info_len =
			nla_len(tb[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]);
		qdf_mem_copy(req.ndp_info.ndp_app_info,
			     nla_data(tb[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]),
			     req.ndp_info.ndp_app_info_len);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS]) {
		/* at present ndp config stores 4 bytes QOS info only */
		req.ndp_config.ndp_cfg_len = 4;
		*((uint32_t *)req.ndp_config.ndp_cfg) =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS]);
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR]) {
		req.is_ipv6_addr_present = true;
		qdf_mem_copy(req.ipv6_addr,
			     nla_data(tb[QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR]),
			     QDF_IPV6_ADDR_SIZE);
	}

	if (os_if_nan_parse_security_params(tb, &req.ncs_sk_type, &req.pmk,
					    &req.passphrase,
					    &req.service_name)) {
		osif_err("inconsistent security params in request.");
		ret = -EINVAL;
		goto initiator_req_failed;
	}

	req.vdev = nan_vdev;
	status = ucfg_nan_req_processor(nan_vdev, &req, NDP_INITIATOR_REQ);
	ret = qdf_status_to_os_return(status);
initiator_req_failed:
	if (ret)
		wlan_objmgr_vdev_release_ref(nan_vdev, WLAN_NAN_ID);

	return ret;
}

static int os_if_nan_process_ndp_initiator_req(struct wlan_objmgr_psoc *psoc,
					       struct nlattr **tb)
{
	struct net_device *net_dev;
	struct osif_vdev_sync *vdev_sync;
	char *ifname;
	int errno;

	errno = osif_nla_str(tb, QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR, &ifname);
	if (errno)
		return errno;

	errno = osif_net_dev_from_ifname(psoc, ifname, &net_dev);
	if (errno)
		return errno;

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __os_if_nan_process_ndp_initiator_req(psoc, ifname, tb);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __os_if_nan_process_ndp_responder_req() - NDP responder request handler
 * @nan_ctx: hdd context
 * @tb: parsed NL attribute list
 *
 * tb includes following vendor attributes:
 * QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR
 * QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID
 * QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID
 * QCA_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE
 * QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO - optional
 * QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS - optional
 * QCA_WLAN_VENDOR_ATTR_NDP_PMK - optional
 * QCA_WLAN_VENDOR_ATTR_NDP_CSID - optional
 * QCA_WLAN_VENDOR_ATTR_NDP_PASSPHRASE - optional
 * QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_NAME - optional
 *
 * Return: 0 on success or error code on failure
 */
static int __os_if_nan_process_ndp_responder_req(struct wlan_objmgr_psoc *psoc,
						 char *iface_name,
						 struct nlattr **tb)
{
	int ret = 0;
	QDF_STATUS status;
	enum nan_datapath_state state;
	struct wlan_objmgr_vdev *nan_vdev = NULL;
	struct nan_datapath_responder_req req = {0};

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE]) {
		osif_err("ndp_rsp is unavailable");
		return -EINVAL;
	}
	req.ndp_rsp = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE]);

	if (req.ndp_rsp == NAN_DATAPATH_RESPONSE_ACCEPT) {
		/* Check for an existing NAN interface */
		nan_vdev = os_if_get_ndi_vdev_by_ifname(psoc, iface_name);
		if (!nan_vdev) {
			osif_err("NAN data iface %s not available",
				 iface_name);
			return -ENODEV;
		}

		if (nan_vdev->vdev_mlme.vdev_opmode != QDF_NDI_MODE) {
			osif_err("Interface found is not NDI");
			ret = -ENODEV;
			goto responder_req_failed;
		}

		if (!ucfg_nan_is_sta_ndp_concurrency_allowed(psoc, nan_vdev)) {
			osif_err("NDP creation not allowed");
			ret = -EOPNOTSUPP;
			goto responder_req_failed;
		}
	} else {
		/*
		 * If the data indication is rejected, the userspace
		 * may not send the iface name. Use the first NDI
		 * in that case
		 */
		osif_debug("ndp rsp rejected, using first NDI");

		nan_vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(
				psoc, QDF_NDI_MODE, WLAN_NAN_ID);
		if (!nan_vdev) {
			osif_err("NAN data iface is not available");
			return -ENODEV;
		}
	}

	state = ucfg_nan_get_ndi_state(nan_vdev);
	if (state == NAN_DATA_NDI_DELETED_STATE ||
	    state == NAN_DATA_NDI_DELETING_STATE ||
	    state == NAN_DATA_NDI_CREATING_STATE) {
		osif_err("Data request not allowed in current NDI state:%d",
			 state);
		ret = -EAGAIN;
		goto responder_req_failed;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		osif_err("Transaction ID is unavailable");
		ret = -EINVAL;
		goto responder_req_failed;
	}
	req.transaction_id =
		nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID]) {
		osif_err("Instance ID is unavailable");
		ret = -EINVAL;
		goto responder_req_failed;
	}
	req.ndp_instance_id =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID]);

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]) {
		req.ndp_info.ndp_app_info_len =
			nla_len(tb[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]);
		qdf_mem_copy(req.ndp_info.ndp_app_info,
			     nla_data(tb[QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO]),
			     req.ndp_info.ndp_app_info_len);
	} else {
		osif_debug("NDP app info is unavailable");
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS]) {
		/* at present ndp config stores 4 bytes QOS info only */
		req.ndp_config.ndp_cfg_len = 4;
		*((uint32_t *)req.ndp_config.ndp_cfg) =
			nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS]);
	} else {
		osif_debug("NDP config data is unavailable");
	}

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR]) {
		req.is_ipv6_addr_present = true;
		qdf_mem_copy(req.ipv6_addr,
			     nla_data(tb[QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR]),
			     QDF_IPV6_ADDR_SIZE);
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PORT]) {
		req.is_port_present = true;
		req.port = nla_get_u16(
			tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PORT]);
	}
	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PROTOCOL]) {
		req.is_protocol_present = true;
		req.protocol = nla_get_u8(
			tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PROTOCOL]);
	}
	osif_debug("ipv6 addr present: %d, addr: %pI6",
		   req.is_ipv6_addr_present, req.ipv6_addr);
	osif_debug("port %d,  present: %d protocol %d,  present: %d",
		   req.port, req.is_port_present, req.protocol,
		   req.is_protocol_present);

	if (os_if_nan_parse_security_params(tb, &req.ncs_sk_type, &req.pmk,
			&req.passphrase, &req.service_name)) {
		osif_err("inconsistent security params in request.");
		ret = -EINVAL;
		goto responder_req_failed;
	}

	osif_debug("vdev_id: %d, transaction_id: %d, ndp_rsp %d, ndp_instance_id: %d, ndp_app_info_len: %d, csid: %d",
		   wlan_vdev_get_id(nan_vdev), req.transaction_id, req.ndp_rsp,
		   req.ndp_instance_id, req.ndp_info.ndp_app_info_len,
		   req.ncs_sk_type);

	req.vdev = nan_vdev;
	status = ucfg_nan_req_processor(nan_vdev, &req, NDP_RESPONDER_REQ);
	ret = qdf_status_to_os_return(status);

responder_req_failed:
	if (ret)
		wlan_objmgr_vdev_release_ref(nan_vdev, WLAN_NAN_ID);

	return ret;

}

static int os_if_nan_process_ndp_responder_req(struct wlan_objmgr_psoc *psoc,
					       struct nlattr **tb)
{
	struct net_device *net_dev;
	struct osif_vdev_sync *vdev_sync;
	char *ifname;
	int errno;

	errno = osif_nla_str(tb, QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR, &ifname);
	if (errno)
		return errno;

	errno = osif_net_dev_from_ifname(psoc, ifname, &net_dev);
	if (errno)
		return errno;

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __os_if_nan_process_ndp_responder_req(psoc, ifname, tb);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

/**
 * __os_if_nan_process_ndp_end_req() - NDP end request handler
 * @psoc: pointer to psoc object
 *
 * @tb: parsed NL attribute list
 * tb includes following vendor attributes:
 * QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID
 *
 * Return: 0 on success or error code on failure
 */
static int __os_if_nan_process_ndp_end_req(struct wlan_objmgr_psoc *psoc,
					   struct nlattr **tb)
{
	int ret = 0;
	QDF_STATUS status;
	struct wlan_objmgr_vdev *nan_vdev;
	struct nan_datapath_end_req req = {0};

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		osif_err("Transaction ID is unavailable");
		return -EINVAL;
	}
	req.transaction_id =
		nla_get_u16(tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY]) {
		osif_err("NDP instance ID array is unavailable");
		return -EINVAL;
	}

	req.num_ndp_instances =
		nla_len(tb[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY]) /
			sizeof(uint32_t);
	if (0 >= req.num_ndp_instances) {
		osif_err("Num NDP instances is 0");
		return -EINVAL;
	}
	qdf_mem_copy(req.ndp_ids,
		     nla_data(tb[QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY]),
		     req.num_ndp_instances * sizeof(uint32_t));

	osif_debug("sending ndp_end_req to SME, transaction_id: %d",
		   req.transaction_id);

	nan_vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(psoc, QDF_NDI_MODE,
							    WLAN_NAN_ID);
	if (!nan_vdev) {
		osif_err("NAN data interface is not available");
		return -EINVAL;
	}

	req.vdev = nan_vdev;
	status = ucfg_nan_req_processor(nan_vdev, &req, NDP_END_REQ);
	ret = qdf_status_to_os_return(status);
	if (ret)
		wlan_objmgr_vdev_release_ref(nan_vdev, WLAN_NAN_ID);

	return ret;
}

static int os_if_nan_process_ndp_end_req(struct wlan_objmgr_psoc *psoc,
					 struct nlattr **tb)
{
	struct wlan_objmgr_vdev *vdev;
	struct net_device *net_dev;
	struct osif_vdev_sync *vdev_sync;
	int errno;

	vdev = wlan_objmgr_get_vdev_by_opmode_from_psoc(psoc, QDF_NDI_MODE,
							WLAN_NAN_ID);
	if (!vdev)
		return -EINVAL;

	errno = osif_net_dev_from_vdev(vdev, &net_dev);

	wlan_objmgr_vdev_release_ref(vdev, WLAN_NAN_ID);

	if (errno)
		return errno;

	errno = osif_vdev_sync_op_start(net_dev, &vdev_sync);
	if (errno)
		return errno;

	errno = __os_if_nan_process_ndp_end_req(psoc, tb);

	osif_vdev_sync_op_stop(vdev_sync);

	return errno;
}

int os_if_nan_process_ndp_cmd(struct wlan_objmgr_psoc *psoc,
			      const void *data, int data_len,
			      bool is_ndp_allowed)
{
	uint32_t ndp_cmd_type;
	uint16_t transaction_id;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX + 1];
	char *iface_name;

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX,
				    data, data_len, vendor_attr_policy)) {
		osif_err("Invalid NDP vendor command attributes");
		return -EINVAL;
	}

	/* Parse and fetch NDP Command Type*/
	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD]) {
		osif_err("NAN datapath cmd type failed");
		return -EINVAL;
	}
	ndp_cmd_type = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD]);

	if (!tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]) {
		osif_err("attr transaction id failed");
		return -EINVAL;
	}
	transaction_id = nla_get_u16(
			tb[QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID]);

	if (tb[QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR]) {
		iface_name = nla_data(tb[QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR]);
		osif_debug("Transaction Id: %u NDPCmd: %u iface_name: %s",
			   transaction_id, ndp_cmd_type, iface_name);
	} else {
		osif_debug("Transaction Id: %u NDPCmd: %u iface_name: unspecified",
			   transaction_id, ndp_cmd_type);
	}

	switch (ndp_cmd_type) {
	case QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE:
		return os_if_nan_process_ndi_create(psoc, tb);
	case QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_DELETE:
		return os_if_nan_process_ndi_delete(psoc, tb);
	case QCA_WLAN_VENDOR_ATTR_NDP_INITIATOR_REQUEST:
		if (!is_ndp_allowed) {
			osif_err("Unsupported concurrency for NAN datapath");
			return -EOPNOTSUPP;
		}
		return os_if_nan_process_ndp_initiator_req(psoc, tb);
	case QCA_WLAN_VENDOR_ATTR_NDP_RESPONDER_REQUEST:
		if (!is_ndp_allowed) {
			osif_err("Unsupported concurrency for NAN datapath");
			return -EOPNOTSUPP;
		}
		return os_if_nan_process_ndp_responder_req(psoc, tb);
	case QCA_WLAN_VENDOR_ATTR_NDP_END_REQUEST:
		if (!is_ndp_allowed) {
			osif_err("Unsupported concurrency for NAN datapath");
			return -EOPNOTSUPP;
		}
		return os_if_nan_process_ndp_end_req(psoc, tb);
	default:
		osif_err("Unrecognized NDP vendor cmd %d", ndp_cmd_type);
		return -EINVAL;
	}

	return -EINVAL;
}

static inline uint32_t osif_ndp_get_ndp_initiator_rsp_len(void)
{
	uint32_t data_len = NLMSG_HDRLEN;

	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE].len);

	return data_len;
}

/**
 * os_if_ndp_initiator_rsp_handler() - NDP initiator response handler
 * @vdev: pointer to vdev object
 * @rsp_params: response parameters
 *
 * Following vendor event is sent to cfg80211:
 * QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD =
 *         QCA_WLAN_VENDOR_ATTR_NDP_INITIATOR_RESPONSE (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID (2 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE (4 bytes)
 *
 * Return: none
 */
static void os_if_ndp_initiator_rsp_handler(struct wlan_objmgr_vdev *vdev,
					struct nan_datapath_initiator_rsp *rsp)
{
	uint32_t data_len;
	struct sk_buff *vendor_event;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct pdev_osif_priv *os_priv = wlan_pdev_get_ospriv(pdev);

	if (!rsp) {
		osif_err("Invalid NDP Initator response");
		return;
	}

	data_len = osif_ndp_get_ndp_initiator_rsp_len();
	vendor_event = cfg80211_vendor_event_alloc(os_priv->wiphy, NULL,
				data_len, QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
				GFP_ATOMIC);
	if (!vendor_event) {
		osif_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
			QCA_WLAN_VENDOR_ATTR_NDP_INITIATOR_RESPONSE))
		goto ndp_initiator_rsp_nla_failed;

	if (nla_put_u16(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
			rsp->transaction_id))
		goto ndp_initiator_rsp_nla_failed;

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID,
			rsp->ndp_instance_id))
		goto ndp_initiator_rsp_nla_failed;

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
			rsp->status))
		goto ndp_initiator_rsp_nla_failed;

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
			rsp->reason))
		goto ndp_initiator_rsp_nla_failed;

	osif_debug("NDP Initiator rsp sent, tid:%d, instance id:%d, status:%d, reason: %d",
		   rsp->transaction_id, rsp->ndp_instance_id, rsp->status,
		   rsp->reason);
	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	return;
ndp_initiator_rsp_nla_failed:
	osif_err("nla_put api failed");
	kfree_skb(vendor_event);
}

static inline uint32_t osif_ndp_get_ndp_responder_rsp_len(void)
{
	uint32_t data_len = NLMSG_HDRLEN;

	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE].len);

	return data_len;
}

/*
 * os_if_ndp_responder_rsp_handler() - NDP responder response handler
 * @vdev: pointer to vdev object
 * @rsp: response parameters
 *
 * Following vendor event is sent to cfg80211:
 * QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD =
 *         QCA_WLAN_VENDOR_ATTR_NDP_RESPONDER_RESPONSE (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID (2 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE (4 bytes)
 *
 * Return: none
 */
static void os_if_ndp_responder_rsp_handler(struct wlan_objmgr_vdev *vdev,
				      struct nan_datapath_responder_rsp *rsp)
{
	uint16_t data_len;
	struct sk_buff *vendor_event;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct pdev_osif_priv *os_priv = wlan_pdev_get_ospriv(pdev);

	if (!rsp) {
		osif_err("Invalid NDP Responder response");
		return;
	}

	data_len = osif_ndp_get_ndp_responder_rsp_len();
	vendor_event = cfg80211_vendor_event_alloc(os_priv->wiphy, NULL,
				data_len, QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
				GFP_ATOMIC);
	if (!vendor_event) {
		osif_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
	   QCA_WLAN_VENDOR_ATTR_NDP_RESPONDER_RESPONSE))
		goto ndp_responder_rsp_nla_failed;

	if (nla_put_u16(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
	   rsp->transaction_id))
		goto ndp_responder_rsp_nla_failed;

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
	   rsp->status))
		goto ndp_responder_rsp_nla_failed;

	if (nla_put_u32(vendor_event,
	   QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
	   rsp->reason))
		goto ndp_responder_rsp_nla_failed;

	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	return;
ndp_responder_rsp_nla_failed:
	osif_err("nla_put api failed");
	kfree_skb(vendor_event);
}

static inline uint32_t osif_ndp_get_ndp_req_ind_len(
				struct nan_datapath_indication_event *event)
{
	uint32_t data_len = NLMSG_HDRLEN;

	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_CSID].len);
	/* allocate space including NULL terminator */
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR].len + 1);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR].len);
	if (event->is_ipv6_addr_present)
		data_len += nla_total_size(vendor_attr_policy[
				QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR].len);
	if (event->scid.scid_len)
		data_len += nla_total_size(event->scid.scid_len);
	if (event->ndp_info.ndp_app_info_len)
		data_len += nla_total_size(event->ndp_info.ndp_app_info_len);

	return data_len;
}

/**
 * os_if_ndp_indication_handler() - NDP indication handler
 * @vdev: pointer to vdev object
 * @ind_params: indication parameters
 *
 * Following vendor event is sent to cfg80211:
 * QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD =
 *         QCA_WLAN_VENDOR_ATTR_NDP_REQUEST_IND (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR (IFNAMSIZ)
 * QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR (6 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR (6 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO (ndp_app_info_len size)
 * QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_CSID(4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_SCID(scid_len in size)
 * QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR (16 bytes)
 *
 * Return: none
 */
static void os_if_ndp_indication_handler(struct wlan_objmgr_vdev *vdev,
				struct nan_datapath_indication_event *event)
{
	const uint8_t *ifname;
	uint16_t data_len;
	qdf_size_t ifname_len;
	uint32_t ndp_qos_config;
	struct sk_buff *vendor_event;
	enum nan_datapath_state state;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct pdev_osif_priv *os_priv = wlan_pdev_get_ospriv(pdev);

	if (!event) {
		osif_err("Invalid NDP Indication");
		return;
	}

	osif_debug("NDP Indication, policy: %d", event->policy);
	state = ucfg_nan_get_ndi_state(vdev);
	/* check if we are in middle of deleting/creating the interface */

	if (state == NAN_DATA_NDI_DELETED_STATE ||
	    state == NAN_DATA_NDI_DELETING_STATE ||
	    state == NAN_DATA_NDI_CREATING_STATE) {
		osif_err("Data request not allowed in current NDI state: %d",
			 state);
		return;
	}

	ifname = os_if_ndi_get_if_name(vdev);
	if (!ifname) {
		osif_err("ifname is null");
		return;
	}
	ifname_len = qdf_str_len(ifname);
	if (ifname_len > IFNAMSIZ) {
		osif_err("ifname(%zu) too long", ifname_len);
		return;
	}

	data_len = osif_ndp_get_ndp_req_ind_len(event);
	/* notify response to the upper layer */
	vendor_event = cfg80211_vendor_event_alloc(os_priv->wiphy,
					NULL, data_len,
					QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
					GFP_ATOMIC);
	if (!vendor_event) {
		osif_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
			QCA_WLAN_VENDOR_ATTR_NDP_REQUEST_IND))
		goto ndp_indication_nla_failed;

	if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR,
		    ifname_len, ifname))
		goto ndp_indication_nla_failed;

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_SERVICE_INSTANCE_ID,
			event->service_instance_id))
		goto ndp_indication_nla_failed;

	if (nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR,
		    QDF_MAC_ADDR_SIZE, event->peer_mac_addr.bytes))
		goto ndp_indication_nla_failed;

	if (nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR,
		    QDF_MAC_ADDR_SIZE, event->peer_discovery_mac_addr.bytes))
		goto ndp_indication_nla_failed;

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID,
			event->ndp_instance_id))
		goto ndp_indication_nla_failed;

	if (event->ndp_info.ndp_app_info_len)
		if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO,
			    event->ndp_info.ndp_app_info_len,
			    event->ndp_info.ndp_app_info))
			goto ndp_indication_nla_failed;

	if (event->ndp_config.ndp_cfg_len) {
		ndp_qos_config = *((uint32_t *)event->ndp_config.ndp_cfg);
		/* at present ndp config stores 4 bytes QOS info only */
		if (nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_NDP_CONFIG_QOS,
				ndp_qos_config))
			goto ndp_indication_nla_failed;
	}

	if (event->scid.scid_len) {
		if (nla_put_u32(vendor_event,
				QCA_WLAN_VENDOR_ATTR_NDP_CSID,
				event->ncs_sk_type))
			goto ndp_indication_nla_failed;

		if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SCID,
			    event->scid.scid_len,
			    event->scid.scid))
			goto ndp_indication_nla_failed;

		osif_debug("csid: %d, scid_len: %d",
			   event->ncs_sk_type, event->scid.scid_len);

		QDF_TRACE_HEX_DUMP(QDF_MODULE_ID_HDD, QDF_TRACE_LEVEL_DEBUG,
				   event->scid.scid, event->scid.scid_len);
	}

	if (event->is_ipv6_addr_present) {
		if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR,
			    QDF_IPV6_ADDR_SIZE, event->ipv6_addr))
			goto ndp_indication_nla_failed;
	}

	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	return;
ndp_indication_nla_failed:
	osif_err("nla_put api failed");
	kfree_skb(vendor_event);
}

static inline uint32_t osif_ndp_get_ndp_confirm_ind_len(
				struct nan_datapath_confirm_event *ndp_confirm)
{
	uint32_t ch_info_len = 0;
	uint32_t data_len = NLMSG_HDRLEN;

	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR].len);
	/* allocate space including NULL terminator */
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR].len + 1);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE].len);
	if (ndp_confirm->ndp_info.ndp_app_info_len)
		data_len +=
			nla_total_size(ndp_confirm->ndp_info.ndp_app_info_len);

	if (ndp_confirm->is_ipv6_addr_present)
		data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR].len);
	if (ndp_confirm->is_port_present)
		data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PORT].len);
	if (ndp_confirm->is_protocol_present)
		data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PROTOCOL].len);

	/* ch_info is a nested array of following attributes */
	ch_info_len += nla_total_size(
		vendor_attr_policy[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL].len);
	ch_info_len += nla_total_size(
		vendor_attr_policy[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH].len);
	ch_info_len += nla_total_size(
		vendor_attr_policy[QCA_WLAN_VENDOR_ATTR_NDP_NSS].len);

	if (ndp_confirm->num_channels)
		data_len += ndp_confirm->num_channels *
				nla_total_size(ch_info_len);

	return data_len;
}

static QDF_STATUS os_if_ndp_confirm_pack_ch_info(struct sk_buff *event,
				struct nan_datapath_confirm_event *ndp_confirm)
{
	int idx = 0;
	struct nlattr *ch_array, *ch_element;

	if (!ndp_confirm->num_channels)
		return QDF_STATUS_SUCCESS;

	ch_array = nla_nest_start(event, QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO);
	if (!ch_array)
		return QDF_STATUS_E_FAULT;

	for (idx = 0; idx < ndp_confirm->num_channels; idx++) {
		ch_element = nla_nest_start(event, idx);
		if (!ch_element)
			return QDF_STATUS_E_FAULT;

		if (nla_put_u32(event, QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL,
				ndp_confirm->ch[idx].freq))
			return QDF_STATUS_E_FAULT;

		if (nla_put_u32(event, QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH,
				ndp_confirm->ch[idx].ch_width))
			return QDF_STATUS_E_FAULT;

		if (nla_put_u32(event, QCA_WLAN_VENDOR_ATTR_NDP_NSS,
				ndp_confirm->ch[idx].nss))
			return QDF_STATUS_E_FAULT;
		nla_nest_end(event, ch_element);
	}
	nla_nest_end(event, ch_array);

	return QDF_STATUS_SUCCESS;
}

/**
 * os_if_ndp_confirm_ind_handler() - NDP confirm indication handler
 * @vdev: pointer to vdev object
 * @ind_params: indication parameters
 *
 * Following vendor event is sent to cfg80211:
 * QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD =
 *         QCA_WLAN_VENDOR_ATTR_NDP_CONFIRM_IND (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR (6 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR (IFNAMSIZ)
 * QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO (ndp_app_info_len size)
 * QCA_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR (16 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PORT (2 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PROTOCOL (1 byte)
 *
 * Return: none
 */
static void
os_if_ndp_confirm_ind_handler(struct wlan_objmgr_vdev *vdev,
			      struct nan_datapath_confirm_event *ndp_confirm)
{
	const uint8_t *ifname;
	uint32_t data_len;
	QDF_STATUS status;
	qdf_size_t ifname_len;
	struct sk_buff *vendor_event;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct pdev_osif_priv *os_priv = wlan_pdev_get_ospriv(pdev);

	if (!ndp_confirm) {
		osif_err("Invalid NDP Initator response");
		return;
	}

	ifname = os_if_ndi_get_if_name(vdev);
	if (!ifname) {
		osif_err("ifname is null");
		return;
	}
	ifname_len = qdf_str_len(ifname);
	if (ifname_len > IFNAMSIZ) {
		osif_err("ifname(%zu) too long", ifname_len);
		return;
	}

	data_len = osif_ndp_get_ndp_confirm_ind_len(ndp_confirm);
	vendor_event = cfg80211_vendor_event_alloc(os_priv->wiphy, NULL,
				data_len, QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
				GFP_ATOMIC);
	if (!vendor_event) {
		osif_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
			QCA_WLAN_VENDOR_ATTR_NDP_CONFIRM_IND))
		goto ndp_confirm_nla_failed;

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID,
			ndp_confirm->ndp_instance_id))
		goto ndp_confirm_nla_failed;

	if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_NDI_MAC_ADDR,
		    QDF_MAC_ADDR_SIZE, ndp_confirm->peer_ndi_mac_addr.bytes))
		goto ndp_confirm_nla_failed;

	if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_IFACE_STR,
		    ifname_len, ifname))
		goto ndp_confirm_nla_failed;

	if (ndp_confirm->ndp_info.ndp_app_info_len &&
		nla_put(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_APP_INFO,
			ndp_confirm->ndp_info.ndp_app_info_len,
			ndp_confirm->ndp_info.ndp_app_info))
		goto ndp_confirm_nla_failed;

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_RESPONSE_CODE,
			ndp_confirm->rsp_code))
		goto ndp_confirm_nla_failed;

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
			ndp_confirm->reason_code))
		goto ndp_confirm_nla_failed;

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_NUM_CHANNELS,
			ndp_confirm->num_channels))
		goto ndp_confirm_nla_failed;

	status = os_if_ndp_confirm_pack_ch_info(vendor_event, ndp_confirm);
	if (QDF_IS_STATUS_ERROR(status))
		goto ndp_confirm_nla_failed;

	if (ndp_confirm->is_ipv6_addr_present) {
		if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_IPV6_ADDR,
			    QDF_IPV6_ADDR_SIZE, ndp_confirm->ipv6_addr))
			goto ndp_confirm_nla_failed;
	}
	if (ndp_confirm->is_port_present)
		if (nla_put_u16(vendor_event,
				QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PORT,
				ndp_confirm->port))
			goto ndp_confirm_nla_failed;
	if (ndp_confirm->is_protocol_present)
		if (nla_put_u8(vendor_event,
			       QCA_WLAN_VENDOR_ATTR_NDP_TRANSPORT_PROTOCOL,
			       ndp_confirm->protocol))
			goto ndp_confirm_nla_failed;

	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	osif_debug("NDP confim sent, ndp instance id: %d, peer addr: "QDF_MAC_ADDR_FMT" rsp_code: %d, reason_code: %d",
		   ndp_confirm->ndp_instance_id,
		   QDF_MAC_ADDR_REF(ndp_confirm->peer_ndi_mac_addr.bytes),
		   ndp_confirm->rsp_code, ndp_confirm->reason_code);

	return;
ndp_confirm_nla_failed:
	osif_err("nla_put api failed");
	kfree_skb(vendor_event);
}

static inline uint32_t osif_ndp_get_ndp_end_rsp_len(void)
{
	uint32_t data_len = NLMSG_HDRLEN;

	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID].len);

	return data_len;
}

/**
 * os_if_ndp_end_rsp_handler() - NDP end response handler
 * @vdev: pointer to vdev object
 * @rsp_params: response parameters
 *
 * Following vendor event is sent to cfg80211:
 * QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD =
 *         QCA_WLAN_VENDOR_ATTR_NDP_END_RESPONSE(4 bytest)
 * QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID (2 bytes)
 *
 * Return: none
 */
static void os_if_ndp_end_rsp_handler(struct wlan_objmgr_vdev *vdev,
				struct nan_datapath_end_rsp_event *rsp)
{
	uint32_t data_len;
	struct sk_buff *vendor_event;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct pdev_osif_priv *os_priv = wlan_pdev_get_ospriv(pdev);

	if (!rsp) {
		osif_err("Invalid ndp end response");
		return;
	}

	data_len = osif_ndp_get_ndp_end_rsp_len();
	vendor_event = cfg80211_vendor_event_alloc(os_priv->wiphy, NULL,
				data_len, QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
				GFP_ATOMIC);
	if (!vendor_event) {
		osif_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
			QCA_WLAN_VENDOR_ATTR_NDP_END_RESPONSE))
		goto ndp_end_rsp_nla_failed;

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
			rsp->status))
		goto ndp_end_rsp_nla_failed;

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
			rsp->reason))
		goto ndp_end_rsp_nla_failed;

	if (nla_put_u16(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
			rsp->transaction_id))
		goto ndp_end_rsp_nla_failed;

	osif_debug("NDP End rsp sent, transaction id: %u, status: %u, reason: %u",
		   rsp->transaction_id, rsp->status, rsp->reason);
	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	return;

ndp_end_rsp_nla_failed:
	osif_err("nla_put api failed");
	kfree_skb(vendor_event);
}

static inline uint32_t osif_ndp_get_ndp_end_ind_len(
			struct nan_datapath_end_indication_event *end_ind)
{
	uint32_t data_len = NLMSG_HDRLEN;

	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD].len);
	if (end_ind->num_ndp_ids)
		data_len += nla_total_size(end_ind->num_ndp_ids *
							sizeof(uint32_t));

	return data_len;
}

/**
 * os_if_ndp_end_ind_handler() - NDP end indication handler
 * @vdev: pointer to vdev object
 * @ind_params: indication parameters
 *
 * Following vendor event is sent to cfg80211:
 * QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD =
 *         QCA_WLAN_VENDOR_ATTR_NDP_END_IND (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY (4 * num of NDP Instances)
 *
 * Return: none
 */
static void os_if_ndp_end_ind_handler(struct wlan_objmgr_vdev *vdev,
			struct nan_datapath_end_indication_event *end_ind)
{
	uint32_t data_len, i;
	uint32_t *ndp_instance_array;
	struct sk_buff *vendor_event;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct pdev_osif_priv *os_priv = wlan_pdev_get_ospriv(pdev);

	if (!end_ind) {
		osif_err("Invalid ndp end indication");
		return;
	}

	ndp_instance_array = qdf_mem_malloc(end_ind->num_ndp_ids *
		sizeof(*ndp_instance_array));
	if (!ndp_instance_array)
		return;

	for (i = 0; i < end_ind->num_ndp_ids; i++)
		ndp_instance_array[i] = end_ind->ndp_map[i].ndp_instance_id;

	data_len = osif_ndp_get_ndp_end_ind_len(end_ind);
	vendor_event = cfg80211_vendor_event_alloc(os_priv->wiphy, NULL,
				data_len, QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
				GFP_ATOMIC);
	if (!vendor_event) {
		qdf_mem_free(ndp_instance_array);
		osif_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
			QCA_WLAN_VENDOR_ATTR_NDP_END_IND))
		goto ndp_end_ind_nla_failed;

	if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY,
			end_ind->num_ndp_ids * sizeof(*ndp_instance_array),
			ndp_instance_array))
		goto ndp_end_ind_nla_failed;

	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	qdf_mem_free(ndp_instance_array);
	return;

ndp_end_ind_nla_failed:
	osif_err("nla_put api failed");
	kfree_skb(vendor_event);
	qdf_mem_free(ndp_instance_array);
}

/**
 * os_if_new_peer_ind_handler() - NDP new peer indication handler
 * @adapter: pointer to adapter context
 * @ind_params: indication parameters
 *
 * Return: none
 */
static void os_if_new_peer_ind_handler(struct wlan_objmgr_vdev *vdev,
			struct nan_datapath_peer_ind *peer_ind)
{
	int ret;
	QDF_STATUS status;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	uint32_t active_peers = ucfg_nan_get_active_peers(vdev);
	struct nan_callbacks cb_obj;

	if (!peer_ind) {
		osif_err("Invalid new NDP peer params");
		return;
	}

	status = ucfg_nan_get_callbacks(psoc, &cb_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("failed to get callbacks");
		return;
	}

	osif_debug("vdev_id: %d, peer_mac: "QDF_MAC_ADDR_FMT,
		   vdev_id, QDF_MAC_ADDR_REF(peer_ind->peer_mac_addr.bytes));
	ret = cb_obj.new_peer_ind(vdev_id, peer_ind->sta_id,
				&peer_ind->peer_mac_addr,
				(active_peers == 0 ? true : false));
	if (ret) {
		osif_err("new peer handling at HDD failed %d", ret);
		return;
	}

	active_peers++;
	ucfg_nan_set_active_peers(vdev, active_peers);
	osif_debug("num_peers: %d", active_peers);
}

/**
 * os_if_peer_departed_ind_handler() - Handle NDP peer departed indication
 * @adapter: pointer to adapter context
 * @ind_params: indication parameters
 *
 * Return: none
 */
static void os_if_peer_departed_ind_handler(struct wlan_objmgr_vdev *vdev,
			struct nan_datapath_peer_ind *peer_ind)
{
	QDF_STATUS status;
	struct nan_callbacks cb_obj;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct wlan_objmgr_psoc *psoc = wlan_vdev_get_psoc(vdev);
	uint32_t active_peers = ucfg_nan_get_active_peers(vdev);

	status = ucfg_nan_get_callbacks(psoc, &cb_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("failed to get callbacks");
		return;
	}

	if (!peer_ind) {
		osif_err("Invalid new NDP peer params");
		return;
	}
	osif_debug("vdev_id: %d, peer_mac: "QDF_MAC_ADDR_FMT,
		   vdev_id, QDF_MAC_ADDR_REF(peer_ind->peer_mac_addr.bytes));
	active_peers--;
	ucfg_nan_set_active_peers(vdev, active_peers);
	cb_obj.peer_departed_ind(vdev_id, peer_ind->sta_id,
				&peer_ind->peer_mac_addr,
				(active_peers == 0 ? true : false));
}

static inline uint32_t osif_ndp_get_ndi_create_rsp_len(void)
{
	uint32_t data_len = NLMSG_HDRLEN;

	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE].len);

	return data_len;
}

/**
 * os_if_ndp_iface_create_rsp_handler() - NDP iface create response handler
 * @adapter: pointer to adapter context
 * @rsp_params: response parameters
 *
 * The function is expected to send a response back to the user space
 * even if the creation of BSS has failed
 *
 * Following vendor event is sent to cfg80211:
 * QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD =
 * QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID (2 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE (4 bytes)
 * QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE
 *
 * Return: none
 */
static void os_if_ndp_iface_create_rsp_handler(struct wlan_objmgr_psoc *psoc,
					       struct wlan_objmgr_vdev *vdev,
					       void *rsp_params)
{
	uint32_t data_len;
	QDF_STATUS status;
	bool create_fail = false;
	struct nan_callbacks cb_obj;
	struct sk_buff *vendor_event;
	uint16_t create_transaction_id;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct pdev_osif_priv *os_priv = wlan_pdev_get_ospriv(pdev);
	uint32_t create_status = NAN_DATAPATH_RSP_STATUS_ERROR;
	uint32_t create_reason = NAN_DATAPATH_NAN_DATA_IFACE_CREATE_FAILED;
	struct nan_datapath_inf_create_rsp *ndi_rsp =
			(struct nan_datapath_inf_create_rsp *)rsp_params;

	status = ucfg_nan_get_callbacks(psoc, &cb_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Couldn't get ballback object");
		return;
	}

	if (ndi_rsp) {
		create_status = ndi_rsp->status;
		create_reason = ndi_rsp->reason;
	} else {
		osif_debug("Invalid ndi create response");
		create_fail = true;
	}

	create_transaction_id = ucfg_nan_get_ndp_create_transaction_id(vdev);
	data_len = osif_ndp_get_ndi_create_rsp_len();
	/* notify response to the upper layer */
	vendor_event = cfg80211_vendor_event_alloc(os_priv->wiphy,
				NULL,
				data_len,
				QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
				GFP_KERNEL);
	if (!vendor_event) {
		osif_err("cfg80211_vendor_event_alloc failed");
		create_fail = true;
		goto close_ndi;
	}

	/* Sub vendor command */
	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
		QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_CREATE)) {
		osif_err("QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD put fail");
		goto nla_put_failure;
	}

	/* Transaction id */
	if (nla_put_u16(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
		create_transaction_id)) {
		osif_err("VENDOR_ATTR_NDP_TRANSACTION_ID put fail");
		goto nla_put_failure;
	}

	/* Status code */
	if (nla_put_u32(vendor_event,
		QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
		create_status)) {
		osif_err("VENDOR_ATTR_NDP_DRV_RETURN_TYPE put fail");
		goto nla_put_failure;
	}

	/* Status return value */
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
			create_reason)) {
		osif_err("VENDOR_ATTR_NDP_DRV_RETURN_VALUE put fail");
		goto nla_put_failure;
	}

	osif_debug("transaction id: %u status code: %u Reason: %u",
		   create_transaction_id, create_status, create_reason);

	if (!create_fail) {
		/* update txrx queues and register self sta */
		cb_obj.drv_ndi_create_rsp_handler(wlan_vdev_get_id(vdev),
						  ndi_rsp);
		cfg80211_vendor_event(vendor_event, GFP_KERNEL);
	} else {
		osif_err("NDI interface creation failed with reason %d",
			 create_reason);
		cfg80211_vendor_event(vendor_event, GFP_KERNEL);
		goto close_ndi;
	}

	return;

nla_put_failure:
	kfree_skb(vendor_event);
close_ndi:
	cb_obj.ndi_close(wlan_vdev_get_id(vdev));
	return;
}

/**
 * os_if_ndp_iface_delete_rsp_handler() - NDP iface delete response handler
 * @adapter: pointer to adapter context
 * @rsp_params: response parameters
 *
 * Return: none
 */
static void os_if_ndp_iface_delete_rsp_handler(struct wlan_objmgr_psoc *psoc,
					      struct wlan_objmgr_vdev *vdev,
					      void *rsp_params)
{
	QDF_STATUS status;
	uint8_t vdev_id = wlan_vdev_get_id(vdev);
	struct nan_datapath_inf_delete_rsp *ndi_rsp = rsp_params;
	struct nan_callbacks cb_obj;

	if (!ndi_rsp) {
		osif_err("Invalid ndi delete response");
		return;
	}

	status = ucfg_nan_get_callbacks(psoc, &cb_obj);
	if (QDF_IS_STATUS_ERROR(status)) {
		osif_err("Couldn't get ballback object");
		return;
	}

	if (ndi_rsp->status == NAN_DATAPATH_RSP_STATUS_SUCCESS)
		osif_debug("NDI BSS successfully stopped");
	else
		osif_debug("NDI BSS stop failed with reason %d",
			   ndi_rsp->reason);

	ucfg_nan_set_ndi_delete_rsp_reason(vdev, ndi_rsp->reason);
	ucfg_nan_set_ndi_delete_rsp_status(vdev, ndi_rsp->status);
	cb_obj.drv_ndi_delete_rsp_handler(vdev_id);
}

static inline uint32_t osif_ndp_get_ndp_sch_update_ind_len(
			struct nan_datapath_sch_update_event *sch_update)
{
	uint32_t ch_info_len = 0;
	uint32_t data_len = NLMSG_HDRLEN;

	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR].len);
	if (sch_update->num_ndp_instances)
		data_len += nla_total_size(sch_update->num_ndp_instances *
					   sizeof(uint32_t));
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_REASON].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_NUM_CHANNELS].len);
	/* ch_info is a nested array of following attributes */
	ch_info_len += nla_total_size(
		vendor_attr_policy[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL].len);
	ch_info_len += nla_total_size(
		vendor_attr_policy[QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH].len);
	ch_info_len += nla_total_size(
		vendor_attr_policy[QCA_WLAN_VENDOR_ATTR_NDP_NSS].len);

	if (sch_update->num_ndp_instances)
		data_len += sch_update->num_ndp_instances *
						nla_total_size(ch_info_len);

	return data_len;
}

static QDF_STATUS os_if_ndp_sch_update_pack_ch_info(struct sk_buff *event,
			struct nan_datapath_sch_update_event *sch_update)
{
	int idx = 0;
	struct nlattr *ch_array, *ch_element;

	osif_debug("num_ch: %d", sch_update->num_channels);
	if (!sch_update->num_channels)
		return QDF_STATUS_SUCCESS;

	ch_array = nla_nest_start(event, QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_INFO);
	if (!ch_array)
		return QDF_STATUS_E_FAULT;

	for (idx = 0; idx < sch_update->num_channels; idx++) {
		osif_debug("ch[%d]: freq: %d, width: %d, nss: %d",
			   idx, sch_update->ch[idx].freq,
			   sch_update->ch[idx].ch_width,
			   sch_update->ch[idx].nss);
		ch_element = nla_nest_start(event, idx);
		if (!ch_element)
			return QDF_STATUS_E_FAULT;

		if (nla_put_u32(event, QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL,
				sch_update->ch[idx].freq))
			return QDF_STATUS_E_FAULT;

		if (nla_put_u32(event, QCA_WLAN_VENDOR_ATTR_NDP_CHANNEL_WIDTH,
				sch_update->ch[idx].ch_width))
			return QDF_STATUS_E_FAULT;

		if (nla_put_u32(event, QCA_WLAN_VENDOR_ATTR_NDP_NSS,
				sch_update->ch[idx].nss))
			return QDF_STATUS_E_FAULT;
		nla_nest_end(event, ch_element);
	}
	nla_nest_end(event, ch_array);

	return QDF_STATUS_SUCCESS;
}

/**
 * os_if_ndp_sch_update_ind_handler() - NDP schedule update handler
 * @vdev: vdev object pointer
 * @ind: sch update pointer
 *
 * Following vendor event is sent to cfg80211:
 *
 * Return: none
 */
static void os_if_ndp_sch_update_ind_handler(struct wlan_objmgr_vdev *vdev,
					     void *ind)
{
	int idx = 0;
	const uint8_t *ifname;
	QDF_STATUS status;
	uint32_t data_len;
	uint8_t ifname_len;
	struct sk_buff *vendor_event;
	struct nan_datapath_sch_update_event *sch_update = ind;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct pdev_osif_priv *os_priv = wlan_pdev_get_ospriv(pdev);

	if (!sch_update) {
		osif_err("Invalid sch update params");
		return;
	}

	ifname = os_if_ndi_get_if_name(vdev);
	if (!ifname) {
		osif_err("ifname is null");
		return;
	}
	ifname_len = qdf_str_len(ifname);
	if (ifname_len > IFNAMSIZ) {
		osif_err("ifname(%d) too long", ifname_len);
		return;
	}

	data_len = osif_ndp_get_ndp_sch_update_ind_len(sch_update);
	vendor_event = cfg80211_vendor_event_alloc(os_priv->wiphy, NULL,
				data_len, QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
				GFP_ATOMIC);
	if (!vendor_event) {
		osif_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
			QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_IND))
		goto ndp_sch_ind_nla_failed;

	if (nla_put(vendor_event,
		    QCA_WLAN_VENDOR_ATTR_NDP_PEER_DISCOVERY_MAC_ADDR,
		    QDF_MAC_ADDR_SIZE, sch_update->peer_addr.bytes))
		goto ndp_sch_ind_nla_failed;

	if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_INSTANCE_ID_ARRAY,
		    sch_update->num_ndp_instances * sizeof(uint32_t),
		    sch_update->ndp_instances))
		goto ndp_sch_ind_nla_failed;

	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_SCHEDULE_UPDATE_REASON,
			sch_update->flags))
		goto ndp_sch_ind_nla_failed;

	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_NUM_CHANNELS,
			sch_update->num_channels))
		goto ndp_sch_ind_nla_failed;

	status = os_if_ndp_sch_update_pack_ch_info(vendor_event, sch_update);
	if (QDF_IS_STATUS_ERROR(status))
		goto ndp_sch_ind_nla_failed;

	osif_debug("Flags: %d, num_instance_id: %d", sch_update->flags,
		   sch_update->num_ndp_instances);

	for (idx = 0; idx < sch_update->num_ndp_instances; idx++)
		osif_debug("ndp_instance[%d]: %d", idx,
			   sch_update->ndp_instances[idx]);

	cfg80211_vendor_event(vendor_event, GFP_ATOMIC);
	return;

ndp_sch_ind_nla_failed:
	osif_err("nla_put api failed");
	kfree_skb(vendor_event);
}

/**
 * os_if_ndp_host_update_handler() - NDP Host update handler
 * @vdev: vdev object pointer
 * @evt: pointer to host update event
 *
 * Return: none
 */
static void os_if_ndp_host_update_handler(struct wlan_objmgr_vdev *vdev,
					  void *evt)
{
	struct nan_vdev_priv_obj *vdev_nan_obj;
	struct nan_datapath_host_event *event;
	struct osif_request *request;

	vdev_nan_obj = nan_get_vdev_priv_obj(vdev);
	if (!vdev_nan_obj) {
		osif_err("vdev_nan_obj is NULL");
		return;
	}

	request = osif_request_get(vdev_nan_obj->disable_context);
	if (!request) {
		osif_debug("Obsolete request");
		return;
	}

	event = osif_request_priv(request);
	qdf_mem_copy(event, evt, sizeof(*event));

	osif_request_complete(request);
	osif_request_put(request);
}

static void os_if_nan_datapath_event_handler(struct wlan_objmgr_psoc *psoc,
					     struct wlan_objmgr_vdev *vdev,
					     uint32_t type, void *msg)
{
	switch (type) {
	case NAN_DATAPATH_INF_CREATE_RSP:
		os_if_ndp_iface_create_rsp_handler(psoc, vdev, msg);
		break;
	case NAN_DATAPATH_INF_DELETE_RSP:
		os_if_ndp_iface_delete_rsp_handler(psoc, vdev, msg);
		break;
	case NDP_CONFIRM:
		os_if_ndp_confirm_ind_handler(vdev, msg);
		break;
	case NDP_INITIATOR_RSP:
		os_if_ndp_initiator_rsp_handler(vdev, msg);
		break;
	case NDP_INDICATION:
		os_if_ndp_indication_handler(vdev, msg);
		break;
	case NDP_NEW_PEER:
		os_if_new_peer_ind_handler(vdev, msg);
		break;
	case NDP_RESPONDER_RSP:
		os_if_ndp_responder_rsp_handler(vdev, msg);
		break;
	case NDP_END_RSP:
		os_if_ndp_end_rsp_handler(vdev, msg);
		break;
	case NDP_END_IND:
		os_if_ndp_end_ind_handler(vdev, msg);
		break;
	case NDP_PEER_DEPARTED:
		os_if_peer_departed_ind_handler(vdev, msg);
		break;
	case NDP_SCHEDULE_UPDATE:
		os_if_ndp_sch_update_ind_handler(vdev, msg);
		break;
	case NDP_HOST_UPDATE:
		os_if_ndp_host_update_handler(vdev, msg);
		break;
	default:
		break;
	}
}

int os_if_nan_register_lim_callbacks(struct wlan_objmgr_psoc *psoc,
				     struct nan_callbacks *cb_obj)
{
	return ucfg_nan_register_lim_callbacks(psoc, cb_obj);
}

void os_if_nan_post_ndi_create_rsp(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id, bool success)
{
	struct nan_datapath_inf_create_rsp rsp = {0};
	struct wlan_objmgr_vdev *vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
						psoc, vdev_id, WLAN_NAN_ID);

	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	if (success) {
		rsp.status = NAN_DATAPATH_RSP_STATUS_SUCCESS;
		rsp.reason = 0;
		os_if_nan_datapath_event_handler(psoc, vdev,
						 NAN_DATAPATH_INF_CREATE_RSP,
						 &rsp);
	} else {
		rsp.status = NAN_DATAPATH_RSP_STATUS_ERROR;
		rsp.reason = NAN_DATAPATH_NAN_DATA_IFACE_CREATE_FAILED;
		os_if_nan_datapath_event_handler(psoc, vdev,
						 NAN_DATAPATH_INF_CREATE_RSP,
						 &rsp);
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_NAN_ID);
}

void os_if_nan_post_ndi_delete_rsp(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id, bool success)
{
	struct nan_datapath_inf_delete_rsp rsp = {0};
	struct wlan_objmgr_vdev *vdev = wlan_objmgr_get_vdev_by_id_from_psoc(
						psoc, vdev_id, WLAN_NAN_ID);
	if (!vdev) {
		osif_err("vdev is null");
		return;
	}

	if (success) {
		rsp.status = NAN_DATAPATH_RSP_STATUS_SUCCESS;
		rsp.reason = 0;
		os_if_nan_datapath_event_handler(psoc, vdev,
						 NAN_DATAPATH_INF_DELETE_RSP,
						 &rsp);
	} else {
		rsp.status = NAN_DATAPATH_RSP_STATUS_ERROR;
		rsp.reason = NAN_DATAPATH_NAN_DATA_IFACE_DELETE_FAILED;
		os_if_nan_datapath_event_handler(psoc, vdev,
						 NAN_DATAPATH_INF_DELETE_RSP,
						 &rsp);
	}
	wlan_objmgr_vdev_release_ref(vdev, WLAN_NAN_ID);
}

static inline uint32_t osif_ndp_get_ndi_delete_rsp_len(void)
{
	uint32_t data_len = NLMSG_HDRLEN;

	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE].len);
	data_len += nla_total_size(vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE].len);

	return data_len;
}

void os_if_nan_ndi_session_end(struct wlan_objmgr_vdev *vdev)
{
	uint32_t data_len;
	struct sk_buff *vendor_event;
	struct wlan_objmgr_pdev *pdev = wlan_vdev_get_pdev(vdev);
	struct pdev_osif_priv *os_priv = wlan_pdev_get_ospriv(pdev);
	enum nan_datapath_state state;

	/*
	 * The virtual adapters are stopped and closed even during
	 * driver unload or stop, the service layer is not required
	 * to be informed in that case (response is not expected)
	 */
	state = ucfg_nan_get_ndi_state(vdev);
	if (state != NAN_DATA_NDI_DELETING_STATE &&
	    state != NAN_DATA_DISCONNECTED_STATE) {
		osif_err("NDI interface deleted: state: %u", state);
		return;
	}

	data_len = osif_ndp_get_ndi_delete_rsp_len();
	/* notify response to the upper layer */
	vendor_event = cfg80211_vendor_event_alloc(os_priv->wiphy, NULL,
			data_len, QCA_NL80211_VENDOR_SUBCMD_NDP_INDEX,
			GFP_KERNEL);

	if (!vendor_event) {
		osif_err("cfg80211_vendor_event_alloc failed");
		return;
	}

	/* Sub vendor command goes first */
	if (nla_put_u32(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_SUBCMD,
			QCA_WLAN_VENDOR_ATTR_NDP_INTERFACE_DELETE)) {
		osif_err("VENDOR_ATTR_NDP_SUBCMD put fail");
		goto failure;
	}

	/* Transaction id */
	if (nla_put_u16(vendor_event, QCA_WLAN_VENDOR_ATTR_NDP_TRANSACTION_ID,
			ucfg_nan_get_ndp_delete_transaction_id(vdev))) {
		osif_err("VENDOR_ATTR_NDP_TRANSACTION_ID put fail");
		goto failure;
	}

	/* Status code */
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RESPONSE_STATUS_TYPE,
			ucfg_nan_get_ndi_delete_rsp_status(vdev))) {
		osif_err("VENDOR_ATTR_NDP_DRV_RETURN_TYPE put fail");
		goto failure;
	}

	/* Status return value */
	if (nla_put_u32(vendor_event,
			QCA_WLAN_VENDOR_ATTR_NDP_DRV_RETURN_VALUE,
			ucfg_nan_get_ndi_delete_rsp_reason(vdev))) {
		osif_err("VENDOR_ATTR_NDP_DRV_RETURN_VALUE put fail");
		goto failure;
	}

	osif_debug("delete transaction id: %u, status code: %u reason: %u",
		   ucfg_nan_get_ndp_delete_transaction_id(vdev),
		   ucfg_nan_get_ndi_delete_rsp_status(vdev),
		   ucfg_nan_get_ndi_delete_rsp_reason(vdev));

	ucfg_nan_set_ndp_delete_transaction_id(vdev, 0);
	ucfg_nan_set_ndi_state(vdev, NAN_DATA_NDI_DELETED_STATE);
	ucfg_ndi_remove_entry_from_policy_mgr(vdev);
	cfg80211_vendor_event(vendor_event, GFP_KERNEL);

	return;
failure:
	kfree_skb(vendor_event);
}

/**
 * os_if_nan_discovery_event_handler() - NAN Discovery Interface event handler
 * @nan_evt: NAN Event parameters
 *
 * Module sends a NAN related vendor event to the upper layer
 *
 * Return: none
 */
static void os_if_nan_discovery_event_handler(struct nan_event_params *nan_evt)
{
	struct sk_buff *vendor_event;
	struct wlan_objmgr_pdev *pdev;
	struct pdev_osif_priv *os_priv;

	/*
	 * Since Partial Offload chipsets have only one pdev per psoc, the first
	 * pdev from the pdev list is used.
	 */
	pdev = wlan_objmgr_get_pdev_by_id(nan_evt->psoc, 0, WLAN_NAN_ID);
	if (!pdev) {
		osif_err("null pdev");
		return;
	}
	os_priv = wlan_pdev_get_ospriv(pdev);

	vendor_event =
		cfg80211_vendor_event_alloc(os_priv->wiphy, NULL,
					    nan_evt->buf_len + NLMSG_HDRLEN,
					    QCA_NL80211_VENDOR_SUBCMD_NAN_INDEX,
					    GFP_KERNEL);

	if (!vendor_event) {
		osif_err("cfg80211_vendor_event_alloc failed");
		goto fail;
	}

	if (nla_put(vendor_event, QCA_WLAN_VENDOR_ATTR_NAN, nan_evt->buf_len,
		    nan_evt->buf)) {
		osif_err("QCA_WLAN_VENDOR_ATTR_NAN put failed");
		goto fail;
	}

	cfg80211_vendor_event(vendor_event, GFP_KERNEL);
fail:
	wlan_objmgr_pdev_release_ref(pdev, WLAN_NAN_ID);
}

int os_if_nan_register_hdd_callbacks(struct wlan_objmgr_psoc *psoc,
				     struct nan_callbacks *cb_obj)
{
	cb_obj->os_if_ndp_event_handler = os_if_nan_datapath_event_handler;
	cb_obj->os_if_nan_event_handler = os_if_nan_discovery_event_handler;
	return ucfg_nan_register_hdd_callbacks(psoc, cb_obj);
}

static int os_if_nan_generic_req(struct wlan_objmgr_psoc *psoc,
				 struct nlattr **tb)
{
	struct nan_generic_req *nan_req;
	uint32_t buf_len;
	QDF_STATUS status;

	buf_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_NAN_CMD_DATA]);

	nan_req = qdf_mem_malloc(sizeof(*nan_req) +  buf_len);
	if (!nan_req)
		return -ENOMEM;

	nan_req->psoc = psoc;
	nan_req->params.request_data_len = buf_len;
	nla_memcpy(nan_req->params.request_data,
		   tb[QCA_WLAN_VENDOR_ATTR_NAN_CMD_DATA], buf_len);

	status = ucfg_nan_discovery_req(nan_req, NAN_GENERIC_REQ);

	if (QDF_IS_STATUS_SUCCESS(status))
		osif_debug("Successfully sent a NAN request");
	else
		osif_err("Unable to send a NAN request");

	qdf_mem_free(nan_req);
	return qdf_status_to_os_return(status);
}

static int os_if_process_nan_disable_req(struct wlan_objmgr_psoc *psoc,
					 struct nlattr **tb)
{
	uint8_t *data;
	uint32_t data_len;
	QDF_STATUS status;

	data = nla_data(tb[QCA_WLAN_VENDOR_ATTR_NAN_CMD_DATA]);
	data_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_NAN_CMD_DATA]);

	status = ucfg_disable_nan_discovery(psoc, data, data_len);

	return qdf_status_to_os_return(status);
}

static int os_if_process_nan_enable_req(struct wlan_objmgr_psoc *psoc,
					struct nlattr **tb)
{
	uint32_t chan_freq_2g, chan_freq_5g = 0;
	uint32_t buf_len;
	QDF_STATUS status;
	uint32_t fine_time_meas_cap;
	struct nan_enable_req *nan_req;

	if (!tb[QCA_WLAN_VENDOR_ATTR_NAN_DISC_24GHZ_BAND_FREQ]) {
		osif_err("NAN Social channel for 2.4Gz is unavailable!");
		return -EINVAL;
	}
	chan_freq_2g =
		nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_NAN_DISC_24GHZ_BAND_FREQ]);

	if (tb[QCA_WLAN_VENDOR_ATTR_NAN_DISC_5GHZ_BAND_FREQ])
		chan_freq_5g =
			nla_get_u32(tb[
				QCA_WLAN_VENDOR_ATTR_NAN_DISC_5GHZ_BAND_FREQ]);

	if (!ucfg_is_nan_enable_allowed(psoc, chan_freq_2g)) {
		osif_err("NAN Enable not allowed at this moment for channel %d",
			 chan_freq_2g);
		return -EINVAL;
	}

	buf_len = nla_len(tb[QCA_WLAN_VENDOR_ATTR_NAN_CMD_DATA]);

	nan_req = qdf_mem_malloc(sizeof(*nan_req) + buf_len);
	if (!nan_req)
		return -ENOMEM;

	nan_req->social_chan_2g_freq = chan_freq_2g;
	if (chan_freq_5g)
		nan_req->social_chan_5g_freq = chan_freq_5g;
	nan_req->psoc = psoc;
	nan_req->params.request_data_len = buf_len;

	ucfg_mlme_get_fine_time_meas_cap(psoc, &fine_time_meas_cap);
	nan_req->params.rtt_cap = fine_time_meas_cap;
	nan_req->params.disable_6g_nan = ucfg_get_disable_6g_nan(psoc);

	nla_memcpy(nan_req->params.request_data,
		   tb[QCA_WLAN_VENDOR_ATTR_NAN_CMD_DATA], buf_len);

	osif_debug("Sending NAN Enable Req. NAN Ch Freq: %d %d",
		   nan_req->social_chan_2g_freq, nan_req->social_chan_5g_freq);
	status = ucfg_nan_discovery_req(nan_req, NAN_ENABLE_REQ);

	if (QDF_IS_STATUS_SUCCESS(status))
		osif_debug("Successfully sent NAN Enable request");
	else
		osif_err("Unable to send NAN Enable request");

	qdf_mem_free(nan_req);
	return qdf_status_to_os_return(status);
}

int os_if_process_nan_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			  const void *data, int data_len)
{
	uint32_t nan_subcmd;
	struct nlattr *tb[QCA_WLAN_VENDOR_ATTR_NAN_PARAMS_MAX + 1];

	if (wlan_cfg80211_nla_parse(tb, QCA_WLAN_VENDOR_ATTR_NAN_PARAMS_MAX,
				    data, data_len, nan_attr_policy)) {
		osif_err("Invalid NAN vendor command attributes");
		return -EINVAL;
	}

	if (!tb[QCA_WLAN_VENDOR_ATTR_NAN_CMD_DATA]) {
		osif_err("NAN cmd data missing!");
		return -EINVAL;
	}

	/*
	 * If target does not support NAN DBS, send request with type GENERIC.
	 * These will be treated as passthrough by the driver. This is to make
	 * sure that HW mode is not set to DBS by NAN Enable request. NAN state
	 * machine will remain unaffected in this case.
	 */
	if (!NAN_CONCURRENCY_SUPPORTED(psoc)) {
		policy_mgr_check_and_stop_opportunistic_timer(psoc, vdev_id);
		return os_if_nan_generic_req(psoc, tb);
	}

	/*
	 * Send all requests other than Enable/Disable as type GENERIC.
	 * These will be treated as passthrough by the driver.
	 */
	if (!tb[QCA_WLAN_VENDOR_ATTR_NAN_SUBCMD_TYPE])
		return os_if_nan_generic_req(psoc, tb);

	nan_subcmd = nla_get_u32(tb[QCA_WLAN_VENDOR_ATTR_NAN_SUBCMD_TYPE]);

	switch (nan_subcmd) {
	case QCA_WLAN_NAN_EXT_SUBCMD_TYPE_ENABLE_REQ:
		return os_if_process_nan_enable_req(psoc, tb);
	case QCA_WLAN_NAN_EXT_SUBCMD_TYPE_DISABLE_REQ:
		return os_if_process_nan_disable_req(psoc, tb);
	default:
		osif_err("Unrecognized NAN subcmd type(%d)", nan_subcmd);
		return -EINVAL;
	}
}
