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
 * DOC: contains nan definitions exposed to other modules
 */

#ifndef _WLAN_NAN_API_H_
#define _WLAN_NAN_API_H_

#include "wlan_objmgr_peer_obj.h"
#include "wlan_policy_mgr_public_struct.h"
#include "qdf_status.h"

#ifdef WLAN_FEATURE_NAN

#include "../src/nan_main_i.h"

struct wlan_objmgr_psoc;

/**
 * nan_init: initializes NAN component, called by dispatcher init
 *
 * Return: status of operation
 */
QDF_STATUS nan_init(void);

/**
 * nan_deinit: de-initializes NAN component, called by dispatcher init
 *
 * Return: status of operation
 */
QDF_STATUS nan_deinit(void);

/**
 * nan_psoc_enable: psoc enable API for NANitioning component
 * @psoc: pointer to PSOC
 *
 * Return: status of operation
 */
QDF_STATUS nan_psoc_enable(struct wlan_objmgr_psoc *psoc);

/**
 * nan_psoc_disable: psoc disable API for NANitioning component
 * @psoc: pointer to PSOC
 *
 * Return: status of operation
 */
QDF_STATUS nan_psoc_disable(struct wlan_objmgr_psoc *psoc);

/**
 * nan_get_peer_priv_obj: get NAN priv object from peer object
 * @peer: pointer to peer object
 *
 * Return: pointer to NAN peer private object
 */
static inline
struct nan_peer_priv_obj *nan_get_peer_priv_obj(struct wlan_objmgr_peer *peer)
{
	struct nan_peer_priv_obj *obj;

	if (!peer) {
		nan_err("peer is null");
		return NULL;
	}
	obj = wlan_objmgr_peer_get_comp_private_obj(peer, WLAN_UMAC_COMP_NAN);

	return obj;
}

/**
 * nan_get_vdev_priv_obj: get NAN priv object from vdev object
 * @vdev: pointer to vdev object
 *
 * Return: pointer to NAN vdev private object
 */
static inline
struct nan_vdev_priv_obj *nan_get_vdev_priv_obj(struct wlan_objmgr_vdev *vdev)
{
	struct nan_vdev_priv_obj *obj;

	if (!vdev) {
		nan_err("vdev is null");
		return NULL;
	}
	obj = wlan_objmgr_vdev_get_comp_private_obj(vdev, WLAN_UMAC_COMP_NAN);

	return obj;
}

/**
 * nan_get_psoc_priv_obj: get NAN priv object from psoc object
 * @psoc: pointer to psoc object
 *
 * Return: pointer to NAN psoc private object
 */
static inline
struct nan_psoc_priv_obj *nan_get_psoc_priv_obj(struct wlan_objmgr_psoc *psoc)
{
	struct nan_psoc_priv_obj *obj;

	if (!psoc) {
		nan_err("psoc is null");
		return NULL;
	}
	obj = wlan_objmgr_psoc_get_comp_private_obj(psoc, WLAN_UMAC_COMP_NAN);

	return obj;
}

/**
 * nan_psoc_get_tx_ops: get TX ops from the NAN private object
 * @psoc: pointer to psoc object
 *
 * Return: pointer to TX op callback
 */
static inline
struct wlan_nan_tx_ops *nan_psoc_get_tx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct nan_psoc_priv_obj *nan_priv;

	if (!psoc) {
		nan_err("psoc is null");
		return NULL;
	}

	nan_priv = nan_get_psoc_priv_obj(psoc);
	if (!nan_priv) {
		nan_err("psoc private object is null");
		return NULL;
	}

	return &nan_priv->tx_ops;
}

/**
 * nan_psoc_get_rx_ops: get RX ops from the NAN private object
 * @psoc: pointer to psoc object
 *
 * Return: pointer to RX op callback
 */
static inline
struct wlan_nan_rx_ops *nan_psoc_get_rx_ops(struct wlan_objmgr_psoc *psoc)
{
	struct nan_psoc_priv_obj *nan_priv;

	if (!psoc) {
		nan_err("psoc is null");
		return NULL;
	}

	nan_priv = nan_get_psoc_priv_obj(psoc);
	if (!nan_priv) {
		nan_err("psoc private object is null");
		return NULL;
	}

	return &nan_priv->rx_ops;
}

/**
 * wlan_nan_get_connection_info: Get NAN related connection info
 * @psoc: pointer to psoc object
 * @conn_info: Coonection info structure pointer
 *
 * Return: status of operation
 */
QDF_STATUS
wlan_nan_get_connection_info(struct wlan_objmgr_psoc *psoc,
			     struct policy_mgr_vdev_entry_info *conn_info);

/**
 * wlan_nan_get_disc_5g_ch_freq: Get NAN Disc 5G channel frequency
 * @psoc: pointer to psoc object
 *
 * Return: NAN Disc 5G channel frequency
 */
uint32_t wlan_nan_get_disc_5g_ch_freq(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_nan_get_sap_conc_support: Get NAN+SAP conc support
 * @psoc: pointer to psoc object
 *
 * Return: True if NAN+SAP supported else false
 */
bool wlan_nan_get_sap_conc_support(struct wlan_objmgr_psoc *psoc);

/**
 * nan_disable_cleanup: Cleanup NAN state upon NAN disable
 * @psoc: pointer to psoc object
 *
 * Return: Cleanup NAN state upon NAN disable
 */
QDF_STATUS nan_disable_cleanup(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_nan_is_beamforming_supported- Get support for beamforing
 * @psoc: pointer to psoc object
 *
 * Return: True if beamforming is supported, false if not.
 */
bool wlan_nan_is_beamforming_supported(struct wlan_objmgr_psoc *psoc);

/**
 * wlan_is_nan_allowed_on_freq() - Check if NAN is allowed on given freq
 * @pdev: pdev context
 * @freq: Frequency to be checked
 *
 * Check if NAN/NDP can be enabled on given frequency.
 *
 * Return: True if NAN is allowed on the given frequency
 */
bool wlan_is_nan_allowed_on_freq(struct wlan_objmgr_pdev *pdev, uint32_t freq);

#else /* WLAN_FEATURE_NAN */
static inline QDF_STATUS nan_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS nan_deinit(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS nan_psoc_enable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS nan_psoc_disable(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
wlan_nan_get_connection_info(struct wlan_objmgr_psoc *psoc,
			     struct policy_mgr_vdev_entry_info *conn_info)
{
	return QDF_STATUS_E_FAILURE;
}

static inline uint32_t
wlan_nan_get_disc_5g_ch_freq(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}

static inline
bool wlan_nan_get_sap_conc_support(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
QDF_STATUS nan_disable_cleanup(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_E_FAILURE;
}

static inline
bool wlan_nan_is_beamforming_supported(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
bool wlan_is_nan_allowed_on_freq(struct wlan_objmgr_pdev *pdev, uint32_t freq)
{
	return false;
}
#endif /* WLAN_FEATURE_NAN */
#endif /* _WLAN_NAN_API_H_ */
