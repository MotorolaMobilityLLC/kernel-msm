/*
 * Copyright (c) 2017-2021 The Linux Foundation. All rights reserved.
 * Copyright (c) 2021 Qualcomm Innovation Center, Inc. All rights reserved.
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
 * DOC: contains interface prototypes for OS_IF layer
 */

#ifndef _NAN_UCFG_API_H_
#define _NAN_UCFG_API_H_

#include "wlan_objmgr_cmn.h"
#include "nan_public_structs.h"

#define NAN_CONCURRENCY_SUPPORTED(psoc) \
	(ucfg_is_nan_dbs_supported(psoc) || \
	 ucfg_is_nan_conc_control_supported(psoc))

#define NDI_CONCURRENCY_SUPPORTED(psoc) \
	(ucfg_is_ndi_dbs_supported(psoc) || \
	 ucfg_is_nan_conc_control_supported(psoc))

#ifdef WLAN_FEATURE_NAN
#define ucfg_nan_set_ndi_state(vdev, state) \
	__ucfg_nan_set_ndi_state(vdev, state, __func__)

/**
 * ucfg_nan_set_ndi_state: set ndi state
 * @vdev: pointer to vdev object
 * @state: value to set
 * @func: Caller of this API
 *
 * Return: status of operation
 */
QDF_STATUS __ucfg_nan_set_ndi_state(struct wlan_objmgr_vdev *vdev,
				    enum nan_datapath_state state,
				    const char *func);

/**
 * ucfg_nan_psoc_open: Setup NAN priv object params on PSOC open
 * @psoc: Pointer to PSOC object
 *
 * Return: QDF Status of operation
 */
QDF_STATUS ucfg_nan_psoc_open(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_nan_psoc_close: Clean up NAN priv data on PSOC close
 * @psoc: Pointer to PSOC object
 *
 * Return: None
 */
void ucfg_nan_psoc_close(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_nan_get_ndi_state: get ndi state from vdev obj
 * @vdev: pointer to vdev object
 *
 * Return: ndi state
 */
enum nan_datapath_state ucfg_nan_get_ndi_state(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_nan_set_active_peers: set active ndi peer
 * @vdev: pointer to vdev object
 * @val: value to set
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_nan_set_active_peers(struct wlan_objmgr_vdev *vdev,
				     uint32_t val);

/**
 * ucfg_nan_get_active_peers: get active ndi peer from vdev obj
 * @vdev: pointer to vdev object
 *
 * Return: active ndi peer
 */
uint32_t ucfg_nan_get_active_peers(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_nan_set_peer_mc_list: API to derive peer multicast address and add it
 *                            to the list
 * @vdev: pointer to vdev object
 * @peer_mac_addr: Peer MAC address
 *
 * Return: None
 */
void ucfg_nan_set_peer_mc_list(struct wlan_objmgr_vdev *vdev,
			       struct qdf_mac_addr peer_mac_addr);

/**
 * ucfg_nan_get_peer_mc_list: API to get peer multicast address list
 * @vdev: pointer to vdev object
 * @peer_mc_addr_list: Out pointer to the peer multicast address list
 *
 * Return: None
 */
void ucfg_nan_get_peer_mc_list(struct wlan_objmgr_vdev *vdev,
			       struct qdf_mac_addr **peer_mc_addr_list);

/**
 * ucfg_nan_clear_peer_mc_list: Clear peer multicast address list
 * @psoc: pointer to psoc object
 * @vdev: pointer to vdev object
 * @peer_mac_addr: Pointer to peer MAC address
 *
 * Return: None
 */
void ucfg_nan_clear_peer_mc_list(struct wlan_objmgr_psoc *psoc,
				 struct wlan_objmgr_vdev *vdev,
				 struct qdf_mac_addr *peer_mac_addr);

/**
 * ucfg_nan_set_ndp_create_transaction_id: set ndp create transaction id
 * @vdev: pointer to vdev object
 * @val: value to set
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_nan_set_ndp_create_transaction_id(struct wlan_objmgr_vdev *vdev,
						  uint16_t val);

/**
 * ucfg_nan_get_ndp_create_transaction_id: get ndp create transaction id
 * vdev obj
 * @vdev: pointer to vdev object
 *
 * Return: ndp create transaction_id
 */
uint16_t ucfg_nan_get_ndp_create_transaction_id(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_nan_set_ndp_delete_transaction_id: set ndp delete transaction id
 * @vdev: pointer to vdev object
 * @val: value to set
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_nan_set_ndp_delete_transaction_id(struct wlan_objmgr_vdev *vdev,
						  uint16_t val);

/**
 * ucfg_nan_get_ndp_delete_transaction_id: get ndp delete transaction id from
 * vdev obj
 * @vdev: pointer to vdev object
 *
 * Return: ndp delete transaction_id
 */
uint16_t ucfg_nan_get_ndp_delete_transaction_id(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_nan_set_ndi_delete_rsp_reason: set ndi delete response reason
 * @vdev: pointer to vdev object
 * @val: value to set
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_nan_set_ndi_delete_rsp_reason(struct wlan_objmgr_vdev *vdev,
					      uint32_t val);

/**
 * ucfg_nan_get_ndi_delete_rsp_reason: get ndi delete response reason from vdev
 * obj
 * @vdev: pointer to vdev object
 *
 * Return: ndi delete rsp reason
 */
uint32_t ucfg_nan_get_ndi_delete_rsp_reason(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_nan_set_ndi_delete_rsp_status: set ndi delete response reason
 * @vdev: pointer to vdev object
 * @val: value to set
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_nan_set_ndi_delete_rsp_status(struct wlan_objmgr_vdev *vdev,
					      uint32_t val);

/**
 * ucfg_nan_get_ndi_delete_rsp_status: get ndi delete response status from vdev
 * obj
 * @vdev: pointer to vdev object
 *
 * Return: ndi delete rsp status
 */
uint32_t ucfg_nan_get_ndi_delete_rsp_status(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_nan_get_callbacks: ucfg API to return callbacks
 * @psoc: pointer to psoc object
 * @cb_obj: callback struct to populate
 *
 * Return: callback struct on success, NULL otherwise
 */
QDF_STATUS ucfg_nan_get_callbacks(struct wlan_objmgr_psoc *psoc,
				  struct nan_callbacks *cb_obj);

/**
 * ucfg_nan_req_processor: ucfg API to be called from HDD/OS_IF to
 * process nan datapath initiator request from userspace
 * @vdev: nan vdev pointer
 * @in_req: NDP request
 * @psoc: pointer to psoc object
 * @req_type: type of request
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_nan_req_processor(struct wlan_objmgr_vdev *vdev,
				  void *in_req, uint32_t req_type);

/**
 * ucfg_nan_datapath_event_handler: ucfg API to be called from legacy code to
 * post events to os_if/hdd layer
 * @psoc: pointer to psoc object
 * @vdev: pointer to vdev object
 * @type: message type
 * @msg: msg buffer
 *
 * Return: None
 */
void ucfg_nan_datapath_event_handler(struct wlan_objmgr_psoc *psoc,
				     struct wlan_objmgr_vdev *vdev,
				     uint32_t type, void *msg);

/**
 * ucfg_nan_register_hdd_callbacks: ucfg API to set hdd callbacks
 * @psoc: pointer to psoc object
 * @cb_obj: structs containing callbacks
 * @os_if_event_handler: os if event handler callback
 *
 * Return: status of operation
 */
int ucfg_nan_register_hdd_callbacks(struct wlan_objmgr_psoc *psoc,
				    struct nan_callbacks *cb_obj);

/*
 * ucfg_nan_register_lim_callbacks: ucfg API to set lim callbacks
 * @psoc: pointer to psoc object
 * @cb_obj: structs containing callbacks
 *
 * Return: status of operation
 */
int ucfg_nan_register_lim_callbacks(struct wlan_objmgr_psoc *psoc,
				    struct nan_callbacks *cb_obj);

/**
 * ucfg_nan_get_callbacks: ucfg API to return callbacks
 * @psoc: pointer to psoc object
 * @cb_obj: callback struct to populate
 *
 * Return: callback struct on success, NULL otherwise
 */
QDF_STATUS ucfg_nan_get_callbacks(struct wlan_objmgr_psoc *psoc,
				  struct nan_callbacks *cb_obj);

/**
 * ucfg_nan_discovery_req: ucfg API for NAN Discovery related requests
 * @in_req: NAN request
 * @req_type: Request type
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_nan_discovery_req(void *in_req, uint32_t req_type);

/**
 * ucfg_is_nan_conc_control_supported() - is NAN concurrency controlled by host
 * @psoc: pointer to psoc object
 *
 * This function returns NAN concurrency support status
 *
 * Return: True if NAN concurrency is controlled by host, False otherwise
 */
bool ucfg_is_nan_conc_control_supported(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_is_nan_dbs_supported() - ucfg API to query NAN DBS support
 * @psoc: pointer to psoc object
 *
 * This function returns NAN DBS support status
 *
 * Return: True if NAN DBS is supported, False otherwise
 */
bool ucfg_is_nan_dbs_supported(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_is_ndi_dbs_supported() - ucfg API to query NAN Datapath DBS support
 * @psoc: pointer to psoc object
 *
 * This function returns NDI DBS support status
 *
 * Return: True if NDI DBS is supported, False otherwise
 */
bool ucfg_is_ndi_dbs_supported(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_is_nan_dbs_supported() - ucfg API to query NAN SAP support
 * @psoc: pointer to psoc object
 *
 * This function returns NAN SAP support status
 *
 * Return: True if NAN SAP is supported, False otherwise
 */
bool ucfg_is_nan_sap_supported(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_is_nan_enable_allowed() - ucfg API to query if NAN Discovery is
 * allowed
 * @psoc: pointer to psoc object
 * @nan_ch_freq: NAN Discovery primary social channel
 *
 * Return: True if NAN Discovery enable is allowed, False otherwise
 */
bool ucfg_is_nan_enable_allowed(struct wlan_objmgr_psoc *psoc,
				uint32_t nan_ch_freq);

/**
 * ucfg_is_nan_disc_active() - ucfg API to query if NAN Discovery is
 * active
 * @psoc: pointer to psoc object
 *
 * Return: True if NAN Discovery is active, False otherwise
 */
bool ucfg_is_nan_disc_active(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_nan_set_tgt_caps: ucfg API to set the NAN capabilities of the Target
 * @psoc: pointer to psoc object
 * @nan_caps: pointer to the structure of NAN capability bits
 *
 * Return: status of operation
 */
void ucfg_nan_set_tgt_caps(struct wlan_objmgr_psoc *psoc,
			   struct nan_tgt_caps *nan_caps);

/**
 * ucfg_nan_disable_concurrency: ucfg API to explicitly disable NAN Discovery
 * @psoc: pointer to psoc object
 *
 * Return: None
 */
void ucfg_nan_disable_concurrency(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_nan_register_wma_callbacks: ucfg API to register WMA callbacks
 * @psoc: pointer to psoc object
 * @cb_obj: Pointer to NAN callback structure
 *
 * Return: status of operation
 */
int ucfg_nan_register_wma_callbacks(struct wlan_objmgr_psoc *psoc,
				    struct nan_callbacks *cb_obj);
/**
 * ucfg_nan_check_and_disable_unsupported_ndi: ucfg API to check if NAN Datapath
 * is active on multiple NDI's and disable the unsupported concurrencies.
 * @psoc: pointer to psoc object
 * @force: When set forces NDI disable
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_nan_check_and_disable_unsupported_ndi(struct wlan_objmgr_psoc *psoc,
					   bool force);

/**
 * ucfg_ndi_remove_entry_from_policy_mgr() - API to remove NDI entry from
 *	policy manager.
 * @vdev: vdev pointer for NDI interface
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_ndi_remove_entry_from_policy_mgr(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_nan_is_enable_disable_in_progress() - Is NAN enable/disable in progress
 * @psoc: Pointer to PSOC object
 *
 * Return: True if NAN discovery enable/disable is in progress, false otherwise
 */
bool ucfg_nan_is_enable_disable_in_progress(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_nan_is_sta_ndp_concurrency_allowed() - Indicates if NDP is allowed
 * @psoc: pointer to psoc object
 * @vdev: pointer to vdev object
 *
 * If STA+NDI(NDPs) exist and another NDI tries to establish
 * NDP, then reject the second NDI(NDP).
 *
 * Return: true if allowed, false otherwise
 */
bool ucfg_nan_is_sta_ndp_concurrency_allowed(struct wlan_objmgr_psoc *psoc,
					     struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_nan_set_vdev_creation_supp_by_fw()- Set the NAN separate vdev psoc param
 * @psoc: pointer to psoc object
 * @set: True if firmware supports NAN separate vdev feature
 *
 * Cache the value of set in NAN psoc object param.
 *
 * Return: None
 */
void
ucfg_nan_set_vdev_creation_supp_by_fw(struct wlan_objmgr_psoc *psoc, bool set);

/**
 * ucfg_nan_is_vdev_creation_allowed()- Get support for NAN vdev creation
 * @psoc: pointer to psoc object
 *
 * Return: True if NAN vdev creation is allowed by host and firmware else false
 */
bool ucfg_nan_is_vdev_creation_allowed(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_nan_is_sta_nan_ndi_4_port_allowed- Get support for 4 port (STA +
 * NAN Disc + NDI + NDI)
 * @psoc: pointer to psoc object
 *
 * Return: True if 4 port concurrency allowed or not.
 */
bool ucfg_nan_is_sta_nan_ndi_4_port_allowed(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_nan_is_beamforming_supported- Get support for beamforing
 * @psoc: pointer to psoc object
 *
 * Return: True if beamforming is supported, false if not.
 */
bool ucfg_nan_is_beamforming_supported(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_disable_nan_discovery() - Disable NAN discovery
 * @psoc: pointer to psoc object
 * @data: Data to be sent to NAN discovery engine, which runs in firmware
 * @data_len: Length of the data
 *
 * Send NAN disable request to firmware by setting the mandatory
 * params(disable_2g_discovery, disable_5g_discovery) along
 * with the data, if provided.
 *
 * Return: status of operation
 */
QDF_STATUS ucfg_disable_nan_discovery(struct wlan_objmgr_psoc *psoc,
				      uint8_t *data, uint32_t data_len);

/**
 * ucfg_nan_disable_ndi() - Disable the NDI with given vdev_id
 * @psoc: pointer to psoc object
 * @ndi_vdev_id: vdev_id of the NDI to be disabled
 *
 * Disable all the NDPs present on the given NDI by sending NDP_END_ALL
 * to firmware. Firmwere sends an immediate response(NDP_HOST_UPDATE) with
 * ndp_disable param as 1 followed by NDP_END indication for all the NDPs.
 *
 * Return: status of operation
 */
QDF_STATUS
ucfg_nan_disable_ndi(struct wlan_objmgr_psoc *psoc, uint32_t ndi_vdev_id);

/**
 * ucfg_get_nan_feature_config() - Get NAN feature bitmap
 * @psoc: pointer to psoc object
 * @nan_feature_config: NAN feature config bitmap to be enabled in firmware
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_get_nan_feature_config(struct wlan_objmgr_psoc *psoc,
				       uint32_t *nan_feature_config);

/**
 * ucfg_is_nan_vdev() - Check if the current vdev supports NAN or not
 * @vdev: pointer to vdev object
 *
 * Return true
 * 1. If the VDEV type is NAN_DISC or
 * 2. If the VDEV type is STA and nan_separate_iface feature is not supported
 *
 * Return: Bool
 */
bool ucfg_is_nan_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_nan_disable_ind_to_userspace() - Send NAN disble ind to userspace
 * @psoc: pointer to psoc object
 *
 * Prepare NAN disable indication and send it to userspace
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_nan_disable_ind_to_userspace(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_is_nan_allowed_on_freq() - Check if NAN is allowed on given freq
 * @pdev: pdev context
 * @freq: Frequency to be checked
 *
 * Check if NAN/NDP can be enabled on given frequency.
 * Validate SRD channels based on the ini and reg domain. Assume rest of the
 * channels support NAN/NDP for now.
 *
 * Return: True if NAN is allowed on the given frequency
 */
bool ucfg_is_nan_allowed_on_freq(struct wlan_objmgr_pdev *pdev, uint32_t freq);

/**
 * ucfg_get_disable_6g_nan() - Get NAN feature configuration for 6GHz
 * @psoc: pointer to psoc object
 *
 * Return: Boolean flag indicating whether the NAN feature is disabled in
 *         6GHz or not
 */
bool ucfg_get_disable_6g_nan(struct wlan_objmgr_psoc *psoc);

#else /* WLAN_FEATURE_NAN */

static inline
void ucfg_nan_set_tgt_caps(struct wlan_objmgr_psoc *psoc,
			   struct nan_tgt_caps *nan_caps)
{
}

static inline void ucfg_nan_disable_concurrency(struct wlan_objmgr_psoc *psoc)
{
}

static inline QDF_STATUS
ucfg_nan_check_and_disable_unsupported_ndi(struct wlan_objmgr_psoc *psoc,
					   bool force)
{
	return QDF_STATUS_SUCCESS;
}

static inline QDF_STATUS ucfg_nan_psoc_open(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline void ucfg_nan_psoc_close(struct wlan_objmgr_psoc *psoc)
{
}

static inline bool ucfg_is_nan_disc_active(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
enum nan_datapath_state ucfg_nan_get_ndi_state(struct wlan_objmgr_vdev *vdev)
{
	return NAN_DATA_INVALID_STATE;
}

static inline
bool ucfg_nan_is_enable_disable_in_progress(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
bool ucfg_nan_is_sta_ndp_concurrency_allowed(struct wlan_objmgr_psoc *psoc,
					     struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline void
ucfg_nan_set_vdev_creation_supp_by_fw(struct wlan_objmgr_psoc *psoc, bool set)
{
}

static inline
bool ucfg_nan_is_vdev_creation_allowed(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
bool ucfg_nan_is_sta_nan_ndi_4_port_allowed(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
bool ucfg_nan_is_beamforming_supported(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
QDF_STATUS ucfg_disable_nan_discovery(struct wlan_objmgr_psoc *psoc,
				      uint8_t *data, uint32_t data_len)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS
ucfg_nan_disable_ndi(struct wlan_objmgr_psoc *psoc, uint32_t ndi_vdev_id)
{
	return QDF_STATUS_E_INVAL;
}

static inline
bool ucfg_is_nan_conc_control_supported(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
QDF_STATUS ucfg_get_nan_feature_config(struct wlan_objmgr_psoc *psoc,
				       uint32_t *nan_feature_config)
{
	return QDF_STATUS_SUCCESS;
}

static inline
bool ucfg_is_nan_vdev(struct wlan_objmgr_vdev *vdev)
{
	return false;
}

static inline
bool ucfg_is_nan_dbs_supported(struct wlan_objmgr_psoc *psoc)
{
	return false;
}

static inline
QDF_STATUS ucfg_nan_disable_ind_to_userspace(struct wlan_objmgr_psoc *psoc)
{
	return QDF_STATUS_SUCCESS;
}

static inline
bool ucfg_is_nan_allowed_on_freq(struct wlan_objmgr_pdev *pdev, uint32_t freq)
{
	return false;
}

static inline bool ucfg_get_disable_6g_nan(struct wlan_objmgr_psoc *psoc)
{
	return true;
}

static inline void
ucfg_nan_get_peer_mc_list(struct wlan_objmgr_vdev *vdev,
			  struct qdf_mac_addr **peer_mc_addr_list)
{
}
#endif /* WLAN_FEATURE_NAN */
#endif /* _NAN_UCFG_API_H_ */
