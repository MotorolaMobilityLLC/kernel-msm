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
 * DOC: This file contains TDLS configures interface definitions
 */

#include <wlan_objmgr_psoc_obj.h>
#include "wlan_tdls_cfg_api.h"
#include "../../core/src/wlan_tdls_main.h"

QDF_STATUS
cfg_tdls_get_support_enable(struct wlan_objmgr_psoc *psoc,
			    bool *val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		*val = false;
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	*val = soc_obj->tdls_configs.tdls_support_enable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_set_support_enable(struct wlan_objmgr_psoc *psoc,
			    bool val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	soc_obj->tdls_configs.tdls_support_enable = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_get_external_control(struct wlan_objmgr_psoc *psoc,
			      bool *val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		*val = false;
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	*val = soc_obj->tdls_configs.tdls_external_control;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_get_uapsd_mask(struct wlan_objmgr_psoc *psoc,
			uint32_t *val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		*val = 0;
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	*val = soc_obj->tdls_configs.tdls_uapsd_mask;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_get_buffer_sta_enable(struct wlan_objmgr_psoc *psoc,
			       bool *val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		*val = false;
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	*val = soc_obj->tdls_configs.tdls_buffer_sta_enable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_set_buffer_sta_enable(struct wlan_objmgr_psoc *psoc,
			       bool val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	soc_obj->tdls_configs.tdls_buffer_sta_enable = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_get_uapsd_inactivity_time(struct wlan_objmgr_psoc *psoc,
				   uint32_t *val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		*val = 0;
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	*val = soc_obj->tdls_configs.tdls_uapsd_inactivity_time;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_get_rx_pkt_threshold(struct wlan_objmgr_psoc *psoc,
			      uint32_t *val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		*val = 0;
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	*val = soc_obj->tdls_configs.tdls_rx_pkt_threshold;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_get_off_channel_enable(struct wlan_objmgr_psoc *psoc,
				bool *val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		*val = false;
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	*val = soc_obj->tdls_configs.tdls_off_chan_enable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_set_off_channel_enable(struct wlan_objmgr_psoc *psoc,
				bool val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	soc_obj->tdls_configs.tdls_off_chan_enable = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_get_off_channel_enable_orig(struct wlan_objmgr_psoc *psoc,
				     bool *val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		*val = false;
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	*val = soc_obj->tdls_configs.tdls_off_chan_enable_orig;

	return QDF_STATUS_SUCCESS;
}

void cfg_tdls_store_off_channel_enable(struct wlan_objmgr_psoc *psoc)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		tdls_err("tdls soc null");
		return;
	}

	soc_obj->tdls_configs.tdls_off_chan_enable_orig =
		soc_obj->tdls_configs.tdls_off_chan_enable;
}

void cfg_tdls_restore_off_channel_enable(struct wlan_objmgr_psoc *psoc)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		tdls_err("tdls soc null");
		return;
	}

	soc_obj->tdls_configs.tdls_off_chan_enable =
		soc_obj->tdls_configs.tdls_off_chan_enable_orig;
	soc_obj->tdls_configs.tdls_off_chan_enable_orig = false;
}

QDF_STATUS
cfg_tdls_get_wmm_mode_enable(struct wlan_objmgr_psoc *psoc,
			     bool *val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		*val = false;
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	*val = soc_obj->tdls_configs.tdls_wmm_mode_enable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_set_vdev_nss_2g(struct wlan_objmgr_psoc *psoc,
			 uint8_t val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	soc_obj->tdls_configs.tdls_vdev_nss_2g = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_set_vdev_nss_5g(struct wlan_objmgr_psoc *psoc,
			 uint8_t val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	soc_obj->tdls_configs.tdls_vdev_nss_5g = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_get_sleep_sta_enable(struct wlan_objmgr_psoc *psoc,
			      bool *val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		*val = false;
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	*val = soc_obj->tdls_configs.tdls_sleep_sta_enable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_set_sleep_sta_enable(struct wlan_objmgr_psoc *psoc,
			      bool val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	soc_obj->tdls_configs.tdls_sleep_sta_enable = val;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_get_scan_enable(struct wlan_objmgr_psoc *psoc,
			 bool *val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		*val = false;
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	*val = soc_obj->tdls_configs.tdls_scan_enable;

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS
cfg_tdls_set_scan_enable(struct wlan_objmgr_psoc *psoc,
			 bool val)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		tdls_err("tdls soc null");
		return QDF_STATUS_E_INVAL;
	}

	soc_obj->tdls_configs.tdls_scan_enable = val;

	return QDF_STATUS_SUCCESS;
}

uint16_t
cfg_tdls_get_max_peer_count(struct wlan_objmgr_psoc *psoc)
{
	struct tdls_soc_priv_obj *soc_obj;

	soc_obj = wlan_psoc_get_tdls_soc_obj(psoc);
	if (!soc_obj) {
		tdls_err("tdls soc null");
		return 1;
	}

	return soc_obj->max_num_tdls_sta;
}
