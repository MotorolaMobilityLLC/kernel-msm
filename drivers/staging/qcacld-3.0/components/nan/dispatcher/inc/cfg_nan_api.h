/*
 * Copyright (c) 2018-2020 The Linux Foundation. All rights reserved.
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

#if !defined(__NAN_CFG_API_H__)
#define __NAN_CFG_API_H__

/**
 *
 * DOC: nan_cfg_api.h
 *
 * NAN feature INI configuration parameters get/set APIs
 */
#include "qdf_types.h"

struct wlan_objmgr_psoc;

#ifdef WLAN_FEATURE_NAN
/**
 * cfg_nan_get_enable() - get NAN support enable status
 * @psoc: pointer to psoc object
 *
 * This function returns NAN enable status
 */
bool cfg_nan_get_enable(struct wlan_objmgr_psoc *psoc);

/**
 * cfg_nan_get_datapath_enable() - get NAN Datapath support enable status
 * @psoc: pointer to psoc object
 *
 * This function returns NAN Datapath enable status
 */
bool cfg_nan_get_datapath_enable(struct wlan_objmgr_psoc *psoc);

/**
 * cfg_nan_get_ndi_mac_randomize() - get NDI MAC randomize enable status
 * @psoc: pointer to psoc object
 *
 * This function returns NAN Datapath Interface MAC randomization status
 */
bool cfg_nan_get_ndi_mac_randomize(struct wlan_objmgr_psoc *psoc);

/**
 * cfg_nan_get_ndp_inactivity_timeout() - get NDP inactivity timeout value
 * @psoc: pointer to psoc object
 * @val: pointer to the value where inactivity timeout has to be copied to
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cfg_nan_get_ndp_inactivity_timeout(struct wlan_objmgr_psoc *psoc,
					      uint16_t *val);

/**
 * cfg_nan_get_ndp_keepalive_period() - get NDP keepalive period
 * @psoc: pointer to psoc object
 * @val: pointer to the value where keepalive period has to be copied to
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cfg_nan_get_ndp_keepalive_period(struct wlan_objmgr_psoc *psoc,
					    uint16_t *val);

/**
 * cfg_nan_get_ndp_max_sessions() - get NDP max sessions host supports
 * @psoc: pointer to psoc object
 * @val: pointer to hold max ndp sessions
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cfg_nan_get_ndp_max_sessions(struct wlan_objmgr_psoc *psoc,
					uint32_t *val);

/**
 * cfg_nan_get_max_ndi() - get max number of ndi host supports
 * @psoc: pointer to psoc object
 * @val: pointer to hold max number of ndi
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cfg_nan_get_max_ndi(struct wlan_objmgr_psoc *psoc, uint32_t *val);

/**
 * cfg_nan_get_support_mp0_discovery() - get value of config support mp0
 * discovery
 * @psoc: pointer to psoc object
 *
 * Return: Value of config join clustur with mp
 */
bool cfg_nan_get_support_mp0_discovery(struct wlan_objmgr_psoc *psoc);

/**
 * cfg_nan_is_roam_config_disabled() - get value of nan config roam disable
 * discovery
 * @psoc: pointer to psoc object
 *
 * Return: true on sta roam disable by nan else false
 */
bool cfg_nan_is_roam_config_disabled(struct wlan_objmgr_psoc *psoc);
#else
static inline
bool cfg_nan_is_roam_config_disabled(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline bool cfg_nan_get_enable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline bool cfg_nan_get_datapath_enable(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline bool cfg_nan_get_ndi_mac_randomize(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
QDF_STATUS cfg_nan_get_ndp_inactivity_timeout(struct wlan_objmgr_psoc *psoc,
					      uint16_t *val)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS cfg_nan_get_ndp_keepalive_period(struct wlan_objmgr_psoc *psoc,
					    uint16_t *val)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS cfg_nan_get_ndp_max_sessions(struct wlan_objmgr_psoc *psoc,
					uint32_t *val)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS cfg_nan_get_max_ndi(struct wlan_objmgr_psoc *psoc, uint32_t *val)
{
	return QDF_STATUS_SUCCESS;
}

static inline bool cfg_nan_get_support_mp0_discovery(
						struct wlan_objmgr_psoc *psoc)
{
	return false;
}

#endif

#endif

