/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 *
 * mac_init_api.c - Header file for mac level init functions
 * Author:    Dinesh Upadhyay
 * Date:      04/23/2007
 * History:-
 * Date       Modified by            Modification Information
 * --------------------------------------------------------------------------
 *
 */
#ifndef __MAC_INIT_API_H
#define __MAC_INIT_API_H

#include "ani_global.h"
#include "sir_types.h"

/**
 * struct mac_start_params - parameters needed when starting the MAC
 * @driver_type: Operating mode of the driver
 */
struct mac_start_params {
	enum qdf_driver_type driver_type;
};

/**
 * mac_start() - Start all MAC modules
 * @mac_handle: Opaque handle to the MAC context
 * @params: Parameters needed to start the MAC
 *
 * This function is called to start MAC. This function will start all
 * the mac modules.
 *
 * Return: QDF_STATUS_SUCCESS if the MAC was successfully started. Any
 *         other value means that there was an issue with starting the
 *         MAC and the MAC should not be considered operational.
 */
QDF_STATUS mac_start(mac_handle_t mac_handle,
		     struct mac_start_params *params);

/**
 * mac_stop() - Stop all MAC modules
 * @mac_handle: Opaque handle to the MAC context
 *
 * This function is called to stop MAC. This function will stop all
 * the mac modules.
 *
 * Return: QDF_STATUS_SUCCESS if the MAC was successfully stopped. Any
 *         other value means that there was an issue with stopping the
 *         MAC, but the caller should still consider the MAC to be
 *         stopped.
 */
QDF_STATUS mac_stop(mac_handle_t mac_handle);

/**
 * mac_open() - Open the MAC
 * @psoc: SOC global object
 * @mac_handle: Pointer to where the MAC handle is to be stored
 * @hdd_handle: Opaque handle to the HDD context
 * @cds_cfg: Initial configuration
 *
 * This function will be called during init. This function is suppose
 * to allocate all the memory with the global context will be
 * allocated here.
 *
 * Return: QDF_STATUS_SUCCESS if the MAC was successfully opened and a
 *         MAC handle was returned to the caller. Any other value
 *         means the MAC was not opened.
 */
QDF_STATUS mac_open(struct wlan_objmgr_psoc *psoc, mac_handle_t *mac_handle,
		    hdd_handle_t hdd_handle, struct cds_config_info *cds_cfg);

/**
 * mac_close() - close the MAC
 * @mac_handle: Opaque handle to the MAC context returned by mac_open()
 *
 * This function will be called in shutdown sequence from HDD. All the
 * allocated memory with global context will be freed here.
 *
 * Return: QDF_STATUS_SUCCESS if the MAC was successfully closed. Any
 *         other value means that there was an issue with closing the
 *         MAC, but the caller should still consider the MAC to be
 *         closed.
 */
QDF_STATUS mac_close(mac_handle_t mac_handle);

/**
 * mac_register_sesssion_open_close_cb() - register open/close session cb
 * @mac_handle: Opaque handle to the MAC context
 * @close_session: callback to be registered with SME for closing the session
 * @callback: Common callback to hdd for all modes
 */
void mac_register_sesssion_open_close_cb(mac_handle_t mac_handle,
					 csr_session_close_cb close_session,
					 csr_roam_complete_cb callback);

#ifdef WLAN_BCN_RECV_FEATURE
/**
 * mac_register_bcn_report_send_cb() - Register bcn receive start
 * indication handler callback
 * @mac: Pointer to Global MAC structure
 * @cb: A pointer to store the callback
 *
 * Once driver gets QCA_NL80211_VENDOR_SUBCMD_BEACON_REPORTING vendor
 * command with attribute for start only. MAC layer register a sme
 * callback through this function.
 *
 * Return: None.
 */
void mac_register_bcn_report_send_cb(struct mac_context *mac,
				     beacon_report_cb cb);
#endif /* WLAN_BCN_RECV_FEATURE */
#endif /* __MAC_INIT_API_H */
