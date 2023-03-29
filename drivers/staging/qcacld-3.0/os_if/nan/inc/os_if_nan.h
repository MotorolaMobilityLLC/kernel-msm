/*
 * Copyright (c) 2012-2021 The Linux Foundation. All rights reserved.
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
 * DOC: declares nan component os interface APIs
 */

#ifndef _OS_IF_NAN_H_
#define _OS_IF_NAN_H_

#include "qdf_types.h"
#ifdef WLAN_FEATURE_NAN
#include "nan_public_structs.h"
#include "nan_ucfg_api.h"
#include "qca_vendor.h"

/* QCA_NL80211_VENDOR_SUBCMD_NAN_EXT policy */
extern const struct nla_policy nan_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NAN_PARAMS_MAX + 1];

/* QCA_NL80211_VENDOR_SUBCMD_NDP policy */
extern const struct nla_policy vendor_attr_policy[
			QCA_WLAN_VENDOR_ATTR_NDP_PARAMS_MAX + 1];

/**
 * struct ndi_find_vdev_filter - find vdev filter object. this can be extended
 * @ifname:           interface name of vdev
 * @found_vdev:       found vdev object matching one or more of above params
 */
struct ndi_find_vdev_filter {
	char *ifname;
	struct wlan_objmgr_vdev *found_vdev;
};

/**
 * os_if_nan_process_ndp_cmd: os_if api to handle nan request message
 * @psoc: pointer to psoc object
 * @data: request data. contains vendor cmd tlvs
 * @data_len: length of data
 * @is_ndp_allowed: Indicates whether to allow NDP creation.
 *		    NDI creation is always allowed.
 *
 * Return: status of operation
 */
int os_if_nan_process_ndp_cmd(struct wlan_objmgr_psoc *psoc,
			      const void *data, int data_len,
			      bool is_ndp_allowed);

/**
 * os_if_nan_register_hdd_callbacks: os_if api to register hdd callbacks
 * @psoc: pointer to psoc object
 * @cb_obj: struct pointer containing callbacks
 *
 * Return: status of operation
 */
int os_if_nan_register_hdd_callbacks(struct wlan_objmgr_psoc *psoc,
				     struct nan_callbacks *cb_obj);

/**
 * os_if_nan_register_lim_callbacks: os_if api to register lim callbacks
 * @psoc: pointer to psoc object
 * @cb_obj: struct pointer containing callbacks
 *
 * Return: status of operation
 */
int os_if_nan_register_lim_callbacks(struct wlan_objmgr_psoc *psoc,
				     struct nan_callbacks *cb_obj);

/**
 * os_if_nan_post_ndi_create_rsp: os_if api to pos ndi create rsp to umac nan
 * component
 * @psoc: pointer to psoc object
 * @vdev_id: vdev id of ndi
 * @success: if create was success or failure
 *
 * Return: None
 */
void os_if_nan_post_ndi_create_rsp(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id, bool success);

/**
 * os_if_nan_post_ndi_delete_rsp: os_if api to pos ndi delete rsp to umac nan
 * component
 * @psoc: pointer to psoc object
 * @vdev_id: vdev id of ndi
 * @success: if delete was success or failure
 *
 * Return: None
 */
void os_if_nan_post_ndi_delete_rsp(struct wlan_objmgr_psoc *psoc,
				   uint8_t vdev_id, bool success);

/**
 * os_if_nan_ndi_session_end: os_if api to process ndi session end
 * component
 * @vdev: pointer to vdev deleted
 *
 * Return: None
 */
void os_if_nan_ndi_session_end(struct wlan_objmgr_vdev *vdev);

/**
 * os_if_nan_set_ndi_state: os_if api set NDI state
 * @vdev: pointer to vdev deleted
 * @state: value to set
 *
 * Return: status of operation
 */
static inline QDF_STATUS os_if_nan_set_ndi_state(struct wlan_objmgr_vdev *vdev,
						 uint32_t state)
{
	return ucfg_nan_set_ndi_state(vdev, state);
}

/**
 * os_if_nan_set_ndp_create_transaction_id: set ndp create transaction id
 * @vdev: pointer to vdev object
 * @val: value to set
 *
 * Return: status of operation
 */
static inline QDF_STATUS os_if_nan_set_ndp_create_transaction_id(
						struct wlan_objmgr_vdev *vdev,
						uint16_t val)
{
	return ucfg_nan_set_ndp_create_transaction_id(vdev, val);
}

/**
 * os_if_nan_set_ndp_delete_transaction_id: set ndp delete transaction id
 * @vdev: pointer to vdev object
 * @val: value to set
 *
 * Return: status of operation
 */
static inline QDF_STATUS os_if_nan_set_ndp_delete_transaction_id(
						struct wlan_objmgr_vdev *vdev,
						uint16_t val)
{
	return ucfg_nan_set_ndp_delete_transaction_id(vdev, val);
}

/**
 * os_if_process_nan_req: os_if api to handle NAN requests attached to the
 * vendor command QCA_NL80211_VENDOR_SUBCMD_NAN_EXT
 * @psoc: pointer to psoc object
 * @vdev_id: NAN vdev id
 * @data: request data. contains vendor cmd tlvs
 * @data_len: length of data
 *
 * Return: status of operation
 */
int os_if_process_nan_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
			  const void *data, int data_len);
#else

static inline void os_if_nan_post_ndi_create_rsp(struct wlan_objmgr_psoc *psoc,
						 uint8_t vdev_id, bool success)
{
}

static inline void os_if_nan_post_ndi_delete_rsp(struct wlan_objmgr_psoc *psoc,
						 uint8_t vdev_id, bool success)
{
}

#endif /* WLAN_FEATURE_NAN */

#endif
