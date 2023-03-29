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

/*
 * DOC: wlan_cm_roam_offload.h
 *
 * Implementation for the common roaming offload api interfaces.
 */

#ifndef _WLAN_CM_ROAM_OFFLOAD_H_
#define _WLAN_CM_ROAM_OFFLOAD_H_

#include "qdf_str.h"
#include "wlan_cm_roam_public_struct.h"

#if defined(WLAN_FEATURE_HOST_ROAM) || defined(WLAN_FEATURE_ROAM_OFFLOAD)

/**
 * cm_roam_state_change() - Post roam state change to roam state machine
 * @pdev: pdev pointer
 * @vdev_id: vdev id
 * @requested_state: roam state to be set
 * @reason: reason for changing roam state for the requested vdev id
 *
 * This function posts roam state change to roam state machine handling
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_roam_state_change(struct wlan_objmgr_pdev *pdev,
		     uint8_t vdev_id,
		     enum roam_offload_state requested_state,
		     uint8_t reason);

/**
 * cm_roam_send_rso_cmd() - send rso command
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @rso_command: roam command to send
 * @reason: reason for changing roam state for the requested vdev id
 *
 * similar to csr_roam_offload_scan, will be used from many legacy
 * process directly, generate a new function wlan_cm_roam_send_rso_cmd
 * for external usage.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_roam_send_rso_cmd(struct wlan_objmgr_psoc *psoc,
				uint8_t vdev_id, uint8_t rso_command,
				uint8_t reason);

/**
 * cm_rso_set_roam_trigger() - Send roam trigger bitmap firmware
 * @pdev: Pointer to pdev
 * @vdev_id: vdev id
 * @triggers: Carries pointer of the object containing vdev id and
 *  roam_trigger_bitmap.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS cm_rso_set_roam_trigger(struct wlan_objmgr_pdev *pdev,
				   uint8_t vdev_id,
				   struct wlan_roam_triggers *trigger);

/**
 * cm_roam_stop_req() - roam stop request handling
 * @psoc: psoc pointer
 * @vdev_id: vdev id
 * @reason: reason for changing roam state for the requested vdev id
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_roam_stop_req(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
		 uint8_t reason);

/**
 * cm_roam_fill_rssi_change_params() - Fill roam scan rssi change parameters
 * @psoc: PSOC pointer
 * @vdev_id: vdev_id
 * @params: RSSI change parameters
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_roam_fill_rssi_change_params(struct wlan_objmgr_psoc *psoc, uint8_t vdev_id,
				struct wlan_roam_rssi_change_params *params);
#endif

#ifdef WLAN_FEATURE_ROAM_OFFLOAD
/**
 * cm_roam_send_rt_stats_config() - Send roam event stats cfg value to FW
 * @psoc: PSOC pointer
 * @vdev_id: vdev id
 * @param_value: roam stats enable/disable cfg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_roam_send_rt_stats_config(struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id, uint8_t param_value);
#else
static inline QDF_STATUS
cm_roam_send_rt_stats_config(struct wlan_objmgr_psoc *psoc,
			     uint8_t vdev_id, uint8_t param_value)
{
	return QDF_STATUS_E_NOSUPPORT;
}
#endif

/**
 * cm_roam_send_disable_config() - Send roam module enable/disable cfg to fw
 * @psoc: PSOC pointer
 * @vdev_id: vdev id
 * @cfg: roaming enable/disable cfg
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
cm_roam_send_disable_config(struct wlan_objmgr_psoc *psoc,
			    uint8_t vdev_id, uint8_t cfg);

/**
 * cm_crypto_authmode_to_wmi_authmode  - Get WMI authmode
 * @authmodeset: Connection auth mode
 * @akm: connection AKM
 * @ucastcipherset: Unicast cipherset
 *
 * Return: WMI auth mode
 */
uint32_t cm_crypto_authmode_to_wmi_authmode(int32_t authmodeset, int32_t akm,
					    int32_t ucastcipherset);
/**
 * cm_crypto_cipher_wmi_cipher()  - Convert crypto cipher to WMI cipher
 * @cipherset: Crypto cipher to convert
 *
 * Return: Cipherset stored in crypto param
 */
uint32_t cm_crypto_cipher_wmi_cipher(int32_t cipherset);

#if defined(WLAN_FEATURE_ROAM_OFFLOAD) && defined(WLAN_FEATURE_FILS_SK)
QDF_STATUS cm_roam_scan_offload_add_fils_params(
		struct wlan_objmgr_psoc *psoc,
		struct wlan_roam_scan_offload_params *rso_cfg,
		uint8_t vdev_id);
#else
static inline
QDF_STATUS cm_roam_scan_offload_add_fils_params(
		struct wlan_objmgr_psoc *psoc,
		struct wlan_roam_scan_offload_params *rso_cfg,
		uint8_t vdev_id)
{
	return QDF_STATUS_SUCCESS;
}
#endif /* FEATURE_ROAM_OFFLOAD && WLAN_FEATURE_FILS_SK */
#endif /* _WLAN_CM_ROAM_OFFLOAD_H_ */
