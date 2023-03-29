/*
 * Copyright (c) 2019-2021 The Linux Foundation. All rights reserved.
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

#ifndef __WMA_TWT_H
#define __WMA_TWT_H

#include "wma.h"
#include "wmi_unified_twt_param.h"

/**
 * struct twt_enable_disable_conf - TWT enable/disable configuration/parameters
 * @congestion_timeout: Congestion timer value in ms for firmware controlled TWT
 * @bcast_en: If bcast TWT enabled
 * @ext_conf_present: If extended config is present
 * @role: The configuration is for WMI_TWT_ROLE
 * @oper: The configuration is for WMI_TWT_OPERATION
 */
struct twt_enable_disable_conf {
	uint32_t congestion_timeout;
	bool bcast_en;
	bool ext_conf_present;
	enum WMI_TWT_ROLE role;
	enum WMI_TWT_OPERATION oper;
};

/**
 * struct twt_add_dialog_complete_event - TWT add dialog complete event
 * @params: Fixed parameters for TWT add dialog complete event
 * @additional_params: additional parameters for TWT add dialog complete event
 *
 * Holds the fixed and additional parameters from add dialog
 * complete event
 */
struct twt_add_dialog_complete_event {
	struct wmi_twt_add_dialog_complete_event_param params;
	struct wmi_twt_add_dialog_additional_params additional_params;
};

#ifdef WLAN_SUPPORT_TWT
/**
 * wma_send_twt_enable_cmd() - Send TWT Enable command to firmware
 * @pdev_id: pdev id
 * @conf: Pointer to twt_enable_disable_conf
 *
 * Return: None
 */
void wma_send_twt_enable_cmd(uint32_t pdev_id,
			     struct twt_enable_disable_conf *conf);

/**
 * wma_set_twt_peer_caps() - Fill the peer TWT capabilities
 * @params: STA context params which will store the capabilities
 * @cmd: Command in which the capabilities should be populated
 *
 * Return: None
 */
void wma_set_twt_peer_caps(tpAddStaParams params,
			   struct peer_assoc_params *cmd);

/**
 * wma_send_twt_disable_cmd() - Send TWT disable command to firmware
 * @pdev_id: pdev id
 * @conf: Pointer to twt_enable_disable_conf
 *
 * Return: None
 */
void wma_send_twt_disable_cmd(uint32_t pdev_id,
			      struct twt_enable_disable_conf *conf);

/**
 * wma_twt_process_add_dialog() - Process twt add dialog command
 * @wma_handle: wma handle
 * @params: add dialog configuration parameters
 *
 * Return: QDF_STATUS_SUCCESS on success, other QDF_STATUS error code
 * on failure
 */

QDF_STATUS wma_twt_process_add_dialog(t_wma_handle *wma_handle,
				      struct wmi_twt_add_dialog_param *params);

/**
 * wma_twt_process_del_dialog() - Process del dialog command
 * @wma_handle: wma handle
 * @params: del dialog configuration parameters
 *
 * Return: QDF_STATUS_SUCCESS on success, other QDF_STATUS error code
 * on failure
 */
QDF_STATUS wma_twt_process_del_dialog(t_wma_handle *wma_handle,
				      struct wmi_twt_del_dialog_param *params);

/**
 * wma_twt_process_pause_dialog() - Process pause dialog command
 * @wma_handle: wma handle
 * @params: pause dialog configuration parameters
 *
 * Return: QDF_STATUS_SUCCESS on success, other QDF_STATUS error code
 * on failure
 */
QDF_STATUS
wma_twt_process_pause_dialog(t_wma_handle *wma_handle,
			     struct wmi_twt_pause_dialog_cmd_param *params);

/**
 * wma_twt_process_nudge_dialog() - Process nudge dialog command
 * @wma_handle: wma handle
 * @params: nudge dialog configuration parameters
 *
 * Return: QDF_STATUS_SUCCESS on success, other QDF_STATUS error code
 * on failure
 */
QDF_STATUS
wma_twt_process_nudge_dialog(t_wma_handle *wma_handle,
			     struct wmi_twt_nudge_dialog_cmd_param *params);

/**
 * wma_twt_process_resume_dialog() - Process resume dialog command
 * @wma_handle: wma handle
 * @params: resume dialog configuration parameters
 *
 * Return: QDF_STATUS_SUCCESS on success, other QDF_STATUS error code
 * on failure
 */
QDF_STATUS
wma_twt_process_resume_dialog(t_wma_handle *wma_handle,
			      struct wmi_twt_resume_dialog_cmd_param *params);

/**
 * wma_update_bcast_twt_support() - update bcost twt support
 * @wh: wma handle
 * @tgt_cfg: target configuration to be updated
 *
 * Update braodcast twt support based on service bit.
 *
 * Return: None
 */
void wma_update_bcast_twt_support(tp_wma_handle wh,
				  struct wma_tgt_cfg *tgt_cfg);

/**
 * wma_update_twt_tgt_cap()- update the supported twt capabilities
 * @wh: wma handle
 * @tgt_cfg: target configuration to be updated
 *
 * Update support for twt capabilities based on service bit.
 *
 * Return: None
 */
void wma_update_twt_tgt_cap(tp_wma_handle wh, struct wma_tgt_cfg *tgt_cfg);

/**
 * wma_register_twt_events() - register for TWT wmi events
 * @wma_handle : wma handle
 *
 * Registers the wmi event handlers for TWT.
 *
 * Return: None
 */
void wma_register_twt_events(tp_wma_handle wma_handle);
#else
static inline void
wma_send_twt_enable_cmd(uint32_t pdev_id,
			struct twt_enable_disable_conf *conf)
{
	wma_debug("TWT not supported as WLAN_SUPPORT_TWT is disabled");
}

static inline void
wma_send_twt_disable_cmd(uint32_t pdev_id,
			 struct twt_enable_disable_conf *conf)
{
}

static inline void wma_set_twt_peer_caps(tpAddStaParams params,
					 struct peer_assoc_params *cmd)
{
}

static inline
QDF_STATUS wma_twt_process_add_dialog(t_wma_handle *wma_handle,
				      struct wmi_twt_add_dialog_param *params)
{
	wma_debug("TWT not supported as WLAN_SUPPORT_TWT is disabled");

	return QDF_STATUS_E_INVAL;
}

static inline
QDF_STATUS wma_twt_process_del_dialog(t_wma_handle *wma_handle,
				      struct wmi_twt_del_dialog_param *params)
{
	wma_debug("TWT not supported as WLAN_SUPPORT_TWT is disabled");

	return QDF_STATUS_E_INVAL;
}

static inline QDF_STATUS
wma_twt_process_pause_dialog(t_wma_handle *wma_handle,
			     struct wmi_twt_pause_dialog_cmd_param *params)
{
	wma_debug("TWT not supported as WLAN_SUPPORT_TWT is disabled");

	return QDF_STATUS_E_INVAL;
}

static inline QDF_STATUS
wma_twt_process_nudge_dialog(t_wma_handle *wma_handle,
			     struct wmi_twt_nudge_dialog_cmd_param *params)
{
	wma_debug("TWT not supported as WLAN_SUPPORT_TWT is disabled");

	return QDF_STATUS_E_INVAL;
}

static inline QDF_STATUS
wma_twt_process_resume_dialog(t_wma_handle *wma_handle,
			      struct wmi_twt_resume_dialog_cmd_param *params)
{
	wma_debug("TWT not supported as WLAN_SUPPORT_TWT is disabled");

	return QDF_STATUS_E_INVAL;
}

static inline void wma_update_bcast_twt_support(tp_wma_handle wh,
						struct wma_tgt_cfg *tgt_cfg)
{
}

static inline
void wma_update_twt_tgt_cap(tp_wma_handle wh, struct wma_tgt_cfg *tgt_cfg)
{
}
static inline void wma_register_twt_events(tp_wma_handle wma_handle)
{
}
#endif

#endif /* __WMA_HE_H */
