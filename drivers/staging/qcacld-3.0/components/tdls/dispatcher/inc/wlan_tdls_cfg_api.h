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

/**
 * DOC: Contains p2p configures interface definitions
 */

#ifndef _WLAN_TDLS_CFG_API_H_
#define _WLAN_TDLS_CFG_API_H_

#include <qdf_types.h>

struct wlan_objmgr_psoc;

#ifdef FEATURE_WLAN_TDLS
/**
 * cfg_tdls_get_support_enable() - get tdls support enable
 * @psoc:        pointer to psoc object
 * @val:         pointer to tdls support enable/disable
 *
 * This function gets tdls support enable
 */
QDF_STATUS
cfg_tdls_get_support_enable(struct wlan_objmgr_psoc *psoc,
			    bool *val);

/**
 * cfg_tdls_set_support_enable() - set tdls support enable
 * @psoc:        pointer to psoc object
 * @val:         set tdls support enable/disable
 *
 * This function sets tdls support enable
 */
QDF_STATUS
cfg_tdls_set_support_enable(struct wlan_objmgr_psoc *psoc,
			    bool val);

/**
 * cfg_tdls_get_external_control() - get tdls external control
 * @psoc:        pointer to psoc object
 * @val:         pointer to tdls external control enable/disable
 *
 * This function gets tdls external control
 */
QDF_STATUS
cfg_tdls_get_external_control(struct wlan_objmgr_psoc *psoc,
			      bool *val);

/**
 * cfg_tdls_get_uapsd_mask() - get tdls uapsd mask
 * @psoc:        pointer to psoc object
 * @val:         pointer to tdls uapsd mask
 *
 * This function gets tdls uapsd mask
 */
QDF_STATUS
cfg_tdls_get_uapsd_mask(struct wlan_objmgr_psoc *psoc,
			uint32_t *val);

/**
 * cfg_tdls_get_buffer_sta_enable() - get tdls buffer sta enable
 * @psoc:        pointer to psoc object
 * @val:         pointer to tdls buffer sta enable
 *
 * This function gets tdls buffer sta enable
 */
QDF_STATUS
cfg_tdls_get_buffer_sta_enable(struct wlan_objmgr_psoc *psoc,
			       bool *val);

/**
 * cfg_tdls_set_buffer_sta_enable() - set tdls buffer sta enable
 * @psoc:        pointer to psoc object
 * @val:         tdls buffer sta enable
 *
 * This function sets tdls buffer sta enable
 */
QDF_STATUS
cfg_tdls_set_buffer_sta_enable(struct wlan_objmgr_psoc *psoc,
			       bool val);

/**
 * cfg_tdls_get_uapsd_inactivity_time() - get tdls uapsd inactivity time
 * @psoc:        pointer to psoc object
 * @val:         pointer to tdls uapsd inactivity time
 *
 * This function gets tdls uapsd inactivity time
 */
QDF_STATUS
cfg_tdls_get_uapsd_inactivity_time(struct wlan_objmgr_psoc *psoc,
				   uint32_t *val);

/**
 * cfg_tdls_get_rx_pkt_threshold() - get tdls rx pkt threshold
 * @psoc:        pointer to psoc object
 * @val:         pointer to tdls tdls rx pkt threshold
 *
 * This function gets tdls rx pkt threshold
 */
QDF_STATUS
cfg_tdls_get_rx_pkt_threshold(struct wlan_objmgr_psoc *psoc,
			      uint32_t *val);

/**
 * cfg_tdls_get_off_channel_enable() - get tdls off channel enable
 * @psoc:        pointer to psoc object
 * @val:         pointer to tdls off channel enable
 *
 * This function gets tdls off channel enable
 */
QDF_STATUS
cfg_tdls_get_off_channel_enable(struct wlan_objmgr_psoc *psoc,
				bool *val);

/**
 * cfg_tdls_set_off_channel_enable() - set tdls off channel enable
 * @psoc:        pointer to psoc object
 * @val:         tdls off channel enable
 *
 * This function sets tdls off channel enable
 */
QDF_STATUS
cfg_tdls_set_off_channel_enable(struct wlan_objmgr_psoc *psoc,
				bool val);

/**
 * cfg_tdls_get_off_channel_enable_orig() - get tdls off channel enable orig
 * @psoc:        pointer to psoc object
 * @val:         pointer to tdls off channel enable
 *
 * This function gets tdls off channel enable orig
 */
QDF_STATUS
cfg_tdls_get_off_channel_enable_orig(struct wlan_objmgr_psoc *psoc,
				     bool *val);

/**
 * cfg_tdls_restore_off_channel_enable() - set tdls off channel enable to
 *                                         tdls_off_chan_enable_orig
 * @psoc:        pointer to psoc object
 *
 * Return: NULL
 */
void cfg_tdls_restore_off_channel_enable(struct wlan_objmgr_psoc *psoc);

/**
 * cfg_tdls_store_off_channel_enable() - save tdls off channel enable to
 *                                       tdls_off_chan_enable_orig
 * @psoc:        pointer to psoc object
 *
 * Return: NULL
 */
void cfg_tdls_store_off_channel_enable(struct wlan_objmgr_psoc *psoc);

/**
 * cfg_tdls_get_wmm_mode_enable() - get tdls wmm mode enable
 * @psoc:        pointer to psoc object
 * @val:         pointer to tdls wmm mode enable
 *
 * This function gets tdls wmm mode enable
 */
QDF_STATUS
cfg_tdls_get_wmm_mode_enable(struct wlan_objmgr_psoc *psoc,
			     bool *val);

/**
 * cfg_tdls_set_vdev_nss_2g() - set tdls vdev nss 2g
 * @psoc:        pointer to psoc object
 * @val:         tdls vdev nss 2g
 *
 * This function sets tdls vdev nss 2g
 */
QDF_STATUS
cfg_tdls_set_vdev_nss_2g(struct wlan_objmgr_psoc *psoc,
			 uint8_t val);

/**
 * cfg_tdls_set_vdev_nss_5g() - set tdls vdev nss 5g
 * @psoc:        pointer to psoc object
 * @val:         tdls vdev nss 5g
 *
 * This function sets tdls vdev nss 5g
 */
QDF_STATUS
cfg_tdls_set_vdev_nss_5g(struct wlan_objmgr_psoc *psoc,
			 uint8_t val);

/**
 * cfg_tdls_get_sleep_sta_enable() - get tdls sleep sta enable
 * @psoc:        pointer to psoc object
 * @val:         pointer to tdls sleep sta enable
 *
 * This function gets tdls sleep sta enable
 */
QDF_STATUS
cfg_tdls_get_sleep_sta_enable(struct wlan_objmgr_psoc *psoc,
			      bool *val);

/**
 * cfg_tdls_set_sleep_sta_enable() - set tdls sleep sta enable
 * @psoc:        pointer to psoc object
 * @val:         tdls sleep sta enable
 *
 * This function sets tdls sleep sta enable
 */
QDF_STATUS
cfg_tdls_set_sleep_sta_enable(struct wlan_objmgr_psoc *psoc,
			      bool val);

/**
 * cfg_tdls_get_scan_enable() - get tdls scan enable
 * @psoc:        pointer to psoc object
 * @val:         pointer to tdls scan enable
 *
 * This function gets tdls scan enable
 */
QDF_STATUS
cfg_tdls_get_scan_enable(struct wlan_objmgr_psoc *psoc,
			 bool *val);

/**
 * cfg_tdls_set_scan_enable() - set tdls scan enable
 * @psoc:        pointer to psoc object
 * @val:         tdls scan enable
 *
 * This function sets tdls scan enable
 */
QDF_STATUS
cfg_tdls_set_scan_enable(struct wlan_objmgr_psoc *psoc,
			 bool val);

/**
 * cfg_tdls_get_max_peer_count() - get tdls max peer count
 * @psoc:        pointer to psoc object
 *
 * This function gets tdls max peer count
 */
uint16_t cfg_tdls_get_max_peer_count(struct wlan_objmgr_psoc *psoc);
#else
static inline QDF_STATUS
cfg_tdls_get_support_enable(struct wlan_objmgr_psoc *psoc,
			    bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_set_support_enable(struct wlan_objmgr_psoc *psoc,
			    bool val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_get_external_control(struct wlan_objmgr_psoc *psoc,
			      bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_get_uapsd_mask(struct wlan_objmgr_psoc *psoc,
			uint32_t *val)
{
	*val = 0;

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_get_buffer_sta_enable(struct wlan_objmgr_psoc *psoc,
			       bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_set_buffer_sta_enable(struct wlan_objmgr_psoc *psoc,
			       bool val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_get_uapsd_inactivity_time(struct wlan_objmgr_psoc *psoc,
				   uint32_t *val)
{
	*val = 0;

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_get_rx_pkt_threshold(struct wlan_objmgr_psoc *psoc,
			      uint32_t *val)
{
	*val = 0;

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_get_off_channel_enable(struct wlan_objmgr_psoc *psoc,
				bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_set_off_channel_enable(struct wlan_objmgr_psoc *psoc,
				bool val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_get_off_channel_enable_orig(struct wlan_objmgr_psoc *psoc,
				     bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}

static inline void
cfg_tdls_restore_off_channel_enable(struct wlan_objmgr_psoc *psoc)
{
}

static inline void
cfg_tdls_store_off_channel_enable(struct wlan_objmgr_psoc *psoc)
{
}

static inline QDF_STATUS
cfg_tdls_get_wmm_mode_enable(struct wlan_objmgr_psoc *psoc,
			     bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_set_vdev_nss_2g(struct wlan_objmgr_psoc *psoc,
			 uint8_t val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_set_vdev_nss_5g(struct wlan_objmgr_psoc *psoc,
			 uint8_t val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_get_sleep_sta_enable(struct wlan_objmgr_psoc *psoc,
			      bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_set_sleep_sta_enable(struct wlan_objmgr_psoc *psoc,
			      bool val)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_get_scan_enable(struct wlan_objmgr_psoc *psoc,
			 bool *val)
{
	*val = false;

	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS
cfg_tdls_set_scan_enable(struct wlan_objmgr_psoc *psoc,
			 bool val)
{
	return QDF_STATUS_SUCCESS;
}

static inline uint16_t
cfg_tdls_get_max_peer_count(struct wlan_objmgr_psoc *psoc)
{
	return 0;
}
#endif /* FEATURE_WLAN_TDLS */
#endif /* _WLAN_TDLS_CFG_API_H_ */
