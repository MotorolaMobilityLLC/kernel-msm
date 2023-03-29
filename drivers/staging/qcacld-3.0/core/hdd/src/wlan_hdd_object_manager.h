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

#if !defined(WLAN_HDD_OBJECT_MANAGER_H)
#define WLAN_HDD_OBJECT_MANAGER_H
/**
 * DOC: HDD object manager API file to create/destroy psoc, pdev, vdev
 * and peer objects by calling object manager APIs
 *
 * Common object model has 1 : N mapping between PSOC and PDEV but for MCL
 * PSOC and PDEV has 1 : 1 mapping.
 *
 * MCL object model view is:
 *
 *                          --------
 *                          | PSOC |
 *                          --------
 *                             |
 *                             |
 *                 --------------------------
 *                 |          PDEV          |
 *                 --------------------------
 *                 |                        |
 *                 |                        |
 *                 |                        |
 *             ----------             -------------
 *             | vdev 0 |             |   vdev n  |
 *             ----------             -------------
 *             |        |             |           |
 *        ----------   ----------    ----------  ----------
 *        | peer 1 |   | peer n |    | peer 1 |  | peer n |
 *        ----------   ----------    ----------  -----------
 *
 */
#include "wlan_hdd_main.h"
#include <wlan_objmgr_cmn.h>
#include <wlan_objmgr_global_obj.h>
#include <wlan_objmgr_psoc_obj.h>
#include <wlan_objmgr_pdev_obj.h>
#include <wlan_objmgr_vdev_obj.h>
#include <wlan_objmgr_peer_obj.h>

/**
 * hdd_objmgr_create_and_store_psoc() - Create psoc and store in hdd context
 * @hdd_ctx: Hdd context
 * @psoc_id: Psoc Id
 *
 * This API creates Psoc object with given @psoc_id and store the psoc reference
 * to hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_create_and_store_psoc(struct hdd_context *hdd_ctx,
				     uint8_t psoc_id);

/**
 * hdd_objmgr_release_and_destroy_psoc() - Deletes the psoc object
 * @hdd_ctx: Hdd context
 *
 * This API deletes psoc object and release its reference from hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_release_and_destroy_psoc(struct hdd_context *hdd_ctx);

/**
 * hdd_objmgr_update_tgt_max_vdev_psoc() - Update target max vdev number
 * @hdd_ctx: Hdd context
 *
 * This API update target max vdev number to psoc object
 *
 * Return: None
 */
void hdd_objmgr_update_tgt_max_vdev_psoc(struct hdd_context *hdd_ctx,
					 uint8_t max_vdev);

/**
 * hdd_objmgr_create_and_store_pdev() - Create pdev and store in hdd context
 * @hdd_ctx: Hdd context
 *
 * This API creates the pdev object and store the pdev reference to hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_create_and_store_pdev(struct hdd_context *hdd_ctx);

/**
 * hdd_objmgr_release_and_destroy_pdev() - Deletes the pdev object
 * @hdd_ctx: Hdd context
 *
 * This API deletes pdev object and release its reference from hdd context
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_release_and_destroy_pdev(struct hdd_context *hdd_ctx);

/**
 * hdd_objmgr_get_vdev() - Get reference of vdev from adapter
 * @adapter: hdd adapter
 *
 * This API gets vdev object reference from hdd adapter
 *
 * Return: pointer to vdev object for success, NULL for failure
 */
#define hdd_objmgr_get_vdev(adapter) \
	__hdd_objmgr_get_vdev(adapter, __func__)
struct wlan_objmgr_vdev *__hdd_objmgr_get_vdev(struct hdd_adapter *adapter,
					       const char *func);

/**
 * hdd_objmgr_put_vdev() - Release reference of vdev object
 * @vdev: pointer to vdev object
 *
 * This API releases vdev object reference which was acquired using
 * hdd_objmgr_get_vdev().
 *
 * Return: void
 */
#define hdd_objmgr_put_vdev(vdev) \
	__hdd_objmgr_put_vdev(vdev, __func__)
void __hdd_objmgr_put_vdev(struct wlan_objmgr_vdev *vdev, const char *func);

/**
 * hdd_objmgr_set_peer_mlme_auth_state() - set the peer mlme auth state
 * @vdev: vdev pointer
 * @is_authenticated: Peer mlme auth state true/false
 *
 * This API set the peer mlme auth state
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_set_peer_mlme_auth_state(struct wlan_objmgr_vdev *vdev,
					bool is_authenticated);

/**
 * hdd_objmgr_set_peer_mlme_state() - set the peer mlme state
 * @vdev: vdev pointer
 * @peer_state: Peer mlme state
 *
 * This API set the peer mlme state
 *
 * Return: 0 for success, negative error code for failure
 */
int hdd_objmgr_set_peer_mlme_state(struct wlan_objmgr_vdev *vdev,
				   enum wlan_peer_state peer_state);

#endif /* end #if !defined(WLAN_HDD_OBJECT_MANAGER_H) */
