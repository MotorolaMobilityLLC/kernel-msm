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
 * mac_init_api.c - This file has all the mac level init functions
 *                   for all the defined threads at system level.
 * Author:    Dinesh Upadhyay
 * Date:      04/23/2007
 * History:-
 * Date: 04/08/2008       Modified by: Santosh Mandiganal
 * Modification Information: Code to allocate and free the  memory for DumpTable entry.
 * --------------------------------------------------------------------------
 *
 */
/* Standard include files */
#include "lim_api.h"             /* lim_cleanup */
#include "sir_types.h"
#include "sys_entry_func.h"
#include "mac_init_api.h"
#include "wlan_mlme_main.h"
#include "wlan_psoc_mlme_api.h"

#ifdef TRACE_RECORD
#include "mac_trace.h"
#endif

#ifdef WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY
static struct mac_context *global_mac_context;

static inline struct mac_context *mac_allocate_context_buffer(void)
{
	global_mac_context = qdf_mem_malloc(sizeof(*global_mac_context));

	return global_mac_context;
}

static inline void mac_free_context_buffer(void)
{
	qdf_mem_free(global_mac_context);
	global_mac_context = NULL;
}
#else /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */
static struct mac_context global_mac_context;

static inline struct mac_context *mac_allocate_context_buffer(void)
{
	return &global_mac_context;
}

static inline void mac_free_context_buffer(void)
{
}
#endif /* WLAN_ALLOCATE_GLOBAL_BUFFERS_DYNAMICALLY */

QDF_STATUS mac_start(mac_handle_t mac_handle,
		     struct mac_start_params *params)
{
	QDF_STATUS status = QDF_STATUS_SUCCESS;
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac || !params) {
		QDF_ASSERT(0);
		status = QDF_STATUS_E_FAILURE;
		return status;
	}

	mac->gDriverType = params->driver_type;

	if (ANI_DRIVER_TYPE(mac) != QDF_DRIVER_TYPE_MFG)
		status = pe_start(mac);

	return status;
}

QDF_STATUS mac_stop(mac_handle_t mac_handle)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	pe_stop(mac);

	return QDF_STATUS_SUCCESS;
}

QDF_STATUS mac_open(struct wlan_objmgr_psoc *psoc, mac_handle_t *mac_handle,
		    hdd_handle_t hdd_handle, struct cds_config_info *cds_cfg)
{
	struct mac_context *mac;
	QDF_STATUS status;
	struct wlan_mlme_psoc_ext_obj *mlme_ext_obj;

	QDF_BUG(mac_handle);
	if (!mac_handle)
		return QDF_STATUS_E_FAILURE;

	mac = mac_allocate_context_buffer();
	if (!mac)
		return QDF_STATUS_E_NOMEM;

	/*
	 * Set various global fields of mac here
	 * (Could be platform dependent as some variables in mac are platform
	 * dependent)
	 */
	mac->hdd_handle = hdd_handle;

	status = wlan_objmgr_psoc_try_get_ref(psoc, WLAN_LEGACY_MAC_ID);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("PSOC get ref failure");
		goto free_mac_context;
	}

	mac->psoc = psoc;
	mlme_ext_obj = wlan_psoc_mlme_get_ext_hdl(psoc);
	if (!mlme_ext_obj) {
		pe_err("Failed to get MLME Obj");
		status = QDF_STATUS_E_FAILURE;
		goto release_psoc_ref;
	}
	mac->mlme_cfg = &mlme_ext_obj->cfg;

	*mac_handle = MAC_HANDLE(mac);

	/* For Non-FTM cases this value will be reset during mac_start */
	if (cds_cfg->driver_type)
		mac->gDriverType = QDF_DRIVER_TYPE_MFG;

	sys_init_globals(mac);

	/* FW: 0 to 2047 and Host: 2048 to 4095 */
	mac->mgmtSeqNum = WLAN_HOST_SEQ_NUM_MIN - 1;
	mac->he_sgi_ltf_cfg_bit_mask = DEF_HE_AUTO_SGI_LTF;
	mac->is_usr_cfg_amsdu_enabled = true;

	status = pe_open(mac, cds_cfg);
	if (QDF_IS_STATUS_ERROR(status)) {
		pe_err("failed to open PE; status:%u", status);
		goto release_psoc_ref;
	}

	return QDF_STATUS_SUCCESS;

release_psoc_ref:
	wlan_objmgr_psoc_release_ref(psoc, WLAN_LEGACY_MAC_ID);

free_mac_context:
	mac_free_context_buffer();

	return status;
}

QDF_STATUS mac_close(mac_handle_t mac_handle)
{

	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	if (!mac)
		return QDF_STATUS_E_FAILURE;

	pe_close(mac);

	if (mac->pdev) {
		wlan_objmgr_pdev_release_ref(mac->pdev, WLAN_LEGACY_MAC_ID);
		mac->pdev = NULL;
	}
	wlan_objmgr_psoc_release_ref(mac->psoc, WLAN_LEGACY_MAC_ID);
	mac->mlme_cfg = NULL;
	mac->psoc = NULL;
	qdf_mem_zero(mac, sizeof(*mac));
	mac_free_context_buffer();

	return QDF_STATUS_SUCCESS;
}

void mac_register_sesssion_open_close_cb(mac_handle_t mac_handle,
					 csr_session_close_cb close_session,
					 csr_roam_complete_cb callback)
{
	struct mac_context *mac = MAC_CONTEXT(mac_handle);

	mac->session_close_cb = close_session;
	mac->session_roam_complete_cb = callback;
}

#ifdef WLAN_BCN_RECV_FEATURE
void mac_register_bcn_report_send_cb(struct mac_context *mac,
				     beacon_report_cb cb)
{
	if (!mac) {
		pe_err("Invalid MAC");
		return;
	}

	mac->lim.sme_bcn_rcv_callback = cb;
}
#endif

