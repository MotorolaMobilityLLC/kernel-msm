/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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
 * DOC: Public API implementation of pkt_capture called by north bound HDD/OSIF.
 */

#include "wlan_pkt_capture_objmgr.h"
#include "wlan_pkt_capture_main.h"
#include "wlan_pkt_capture_ucfg_api.h"
#include "wlan_pkt_capture_mon_thread.h"
#include "wlan_pkt_capture_mgmt_txrx.h"
#include "target_if_pkt_capture.h"
#include "wlan_pkt_capture_data_txrx.h"
#include "wlan_pkt_capture_tgt_api.h"

enum pkt_capture_mode ucfg_pkt_capture_get_mode(struct wlan_objmgr_psoc *psoc)
{
	return pkt_capture_get_mode(psoc);
}

/**
 * ucfg_pkt_capture_register_callbacks - Register packet capture callbacks
 * @vdev: pointer to wlan vdev object manager
 * mon_cb: callback to call
 * context: callback context
 *
 * Return: 0 in case of success, invalid in case of failure.
 */
QDF_STATUS
ucfg_pkt_capture_register_callbacks(struct wlan_objmgr_vdev *vdev,
				    QDF_STATUS (*mon_cb)(void *, qdf_nbuf_t),
				    void *context)
{
	return pkt_capture_register_callbacks(vdev, mon_cb, context);
}

/**
 * ucfg_pkt_capture_deregister_callbacks - De-register packet capture callbacks
 * @vdev: pointer to wlan vdev object manager
 *
 * Return: 0 in case of success, invalid in case of failure.
 */
QDF_STATUS ucfg_pkt_capture_deregister_callbacks(struct wlan_objmgr_vdev *vdev)
{
	return pkt_capture_deregister_callbacks(vdev);
}

/**
 * ucfg_pkt_capture_set_pktcap_mode - Set packet capture mode
 * @psoc: pointer to psoc object
 * @mode: mode to be set
 *
 * Return: None
 */
void ucfg_pkt_capture_set_pktcap_mode(struct wlan_objmgr_psoc *psoc,
				      enum pkt_capture_mode mode)
{
	pkt_capture_set_pktcap_mode(psoc, mode);
}

/**
 * ucfg_pkt_capture_get_pktcap_mode - Get packet capture mode
 * @psoc: pointer to psoc object
 *
 * Return: enum pkt_capture_mode
 */
enum pkt_capture_mode
ucfg_pkt_capture_get_pktcap_mode(struct wlan_objmgr_psoc *psoc)
{
	return pkt_capture_get_pktcap_mode(psoc);
}

/**
 * ucfg_pkt_capture_set_pktcap_config - Set packet capture config
 * @vdev: pointer to vdev object
 * @config: config to be set
 *
 * Return: None
 */
void ucfg_pkt_capture_set_pktcap_config(struct wlan_objmgr_vdev *vdev,
					enum pkt_capture_config config)
{
	pkt_capture_set_pktcap_config(vdev, config);
}

/**
 * ucfg_pkt_capture_get_pktcap_config - Get packet capture config
 * @vdev: pointer to vdev object
 *
 * Return: config value
 */
enum pkt_capture_config
ucfg_pkt_capture_get_pktcap_config(struct wlan_objmgr_vdev *vdev)
{
	return pkt_capture_get_pktcap_config(vdev);
}

/**
 * ucfg_pkt_capture_init() - Packet capture component initialization.
 *
 * This function gets called when packet capture initializing.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success.
 */
QDF_STATUS ucfg_pkt_capture_init(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_register_psoc_create_handler(
				WLAN_UMAC_COMP_PKT_CAPTURE,
				pkt_capture_psoc_create_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to register psoc create handler");
		return status;
	}

	status = wlan_objmgr_register_psoc_destroy_handler(
				WLAN_UMAC_COMP_PKT_CAPTURE,
				pkt_capture_psoc_destroy_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to register psoc delete handler");
		goto fail_destroy_psoc;
	}

	status = wlan_objmgr_register_vdev_create_handler(
				WLAN_UMAC_COMP_PKT_CAPTURE,
				pkt_capture_vdev_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to register vdev create handler");
		goto fail_create_vdev;
	}

	status = wlan_objmgr_register_vdev_destroy_handler(
				WLAN_UMAC_COMP_PKT_CAPTURE,
				pkt_capture_vdev_destroy_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status)) {
		pkt_capture_err("Failed to register vdev destroy handler");
		goto fail_destroy_vdev;
	}
	return status;

fail_destroy_vdev:
	wlan_objmgr_unregister_vdev_create_handler(WLAN_UMAC_COMP_PKT_CAPTURE,
		pkt_capture_vdev_create_notification, NULL);

fail_create_vdev:
	wlan_objmgr_unregister_psoc_destroy_handler(WLAN_UMAC_COMP_PKT_CAPTURE,
		pkt_capture_psoc_destroy_notification, NULL);

fail_destroy_psoc:
	wlan_objmgr_unregister_psoc_create_handler(WLAN_UMAC_COMP_PKT_CAPTURE,
		pkt_capture_psoc_create_notification, NULL);

	return status;
}

void ucfg_pkt_capture_deinit(void)
{
	QDF_STATUS status;

	status = wlan_objmgr_unregister_vdev_destroy_handler(
				WLAN_UMAC_COMP_PKT_CAPTURE,
				pkt_capture_vdev_destroy_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		pkt_capture_err("Failed to unregister vdev destroy handler");

	status = wlan_objmgr_unregister_vdev_create_handler(
				WLAN_UMAC_COMP_PKT_CAPTURE,
				pkt_capture_vdev_create_notification, NULL);
	if (QDF_IS_STATUS_ERROR(status))
		pkt_capture_err("Failed to unregister vdev create handler");

	status = wlan_objmgr_unregister_psoc_destroy_handler(
				WLAN_UMAC_COMP_PKT_CAPTURE,
				pkt_capture_psoc_destroy_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		pkt_capture_err("Failed to unregister psoc destroy handler");

	status = wlan_objmgr_unregister_psoc_create_handler(
				WLAN_UMAC_COMP_PKT_CAPTURE,
				pkt_capture_psoc_create_notification,
				NULL);
	if (QDF_IS_STATUS_ERROR(status))
		pkt_capture_err("Failed to unregister psoc create handler");
}

int ucfg_pkt_capture_suspend_mon_thread(struct wlan_objmgr_vdev *vdev)
{
	return pkt_capture_suspend_mon_thread(vdev);
}

void ucfg_pkt_capture_resume_mon_thread(struct wlan_objmgr_vdev *vdev)
{
	pkt_capture_resume_mon_thread(vdev);
}

/**
 * ucfg_process_pktcapture_mgmt_tx_data() - process management tx packets
 * @pdev: pointer to pdev object
 * @params: management offload event params
 * @nbuf: netbuf
 * @status: status
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_pkt_capture_process_mgmt_tx_data(struct wlan_objmgr_pdev *pdev,
				      struct mgmt_offload_event_params *params,
				      qdf_nbuf_t nbuf,
				      uint8_t status)
{
	return pkt_capture_process_mgmt_tx_data(
					pdev, params, nbuf,
					pkt_capture_mgmt_status_map(status));

}

void
ucfg_pkt_capture_mgmt_tx(struct wlan_objmgr_pdev *pdev,
			 qdf_nbuf_t nbuf,
			 uint16_t chan_freq,
			 uint8_t preamble_type)
{
	pkt_capture_mgmt_tx(pdev, nbuf, chan_freq, preamble_type);
}

/**
 * ucfg_process_pktcapture_mgmt_tx_completion(): process mgmt tx completion
 * for pkt capture mode
 * @pdev: pointer to pdev object
 * @desc_id: desc_id
 * @status: status
 * @params: management offload event params
 *
 * Return: none
 */
void
ucfg_pkt_capture_mgmt_tx_completion(struct wlan_objmgr_pdev *pdev,
				    uint32_t desc_id,
				    uint32_t status,
				    struct mgmt_offload_event_params *params)
{
	pkt_capture_mgmt_tx_completion(pdev, desc_id, status, params);
}

void ucfg_pkt_capture_rx_msdu_process(
				uint8_t *bssid,
				qdf_nbuf_t head_msdu,
				uint8_t vdev_id, htt_pdev_handle pdev)
{
		pkt_capture_msdu_process_pkts(bssid, head_msdu,
					      vdev_id, pdev, 0);
}

bool ucfg_pkt_capture_rx_offloaded_pkt(qdf_nbuf_t rx_ind_msg)
{
	return pkt_capture_rx_in_order_offloaded_pkt(rx_ind_msg);
}

void ucfg_pkt_capture_rx_drop_offload_pkt(qdf_nbuf_t head_msdu)
{
	pkt_capture_rx_in_order_drop_offload_pkt(head_msdu);
}

void
ucfg_pkt_capture_offload_deliver_indication_handler(
					void *msg, uint8_t vdev_id,
					uint8_t *bssid, htt_pdev_handle pdev)
{
	pkt_capture_offload_deliver_indication_handler(msg, vdev_id,
						       bssid, pdev);
}

struct htt_tx_data_hdr_information *ucfg_pkt_capture_tx_get_txcomplete_data_hdr(
							uint32_t *msg_word,
							int num_msdus)
{
	return pkt_capture_tx_get_txcomplete_data_hdr(msg_word, num_msdus);
}

void
ucfg_pkt_capture_tx_completion_process(
			uint8_t vdev_id,
			qdf_nbuf_t mon_buf_list,
			enum pkt_capture_data_process_type type,
			uint8_t tid, uint8_t status, bool pkt_format,
			uint8_t *bssid, htt_pdev_handle pdev,
			uint8_t tx_retry_cnt)
{
	pkt_capture_datapkt_process(
				vdev_id,
				mon_buf_list, TXRX_PROCESS_TYPE_DATA_TX_COMPL,
				tid, status, pkt_format, bssid, pdev,
				tx_retry_cnt);
}

void ucfg_pkt_capture_record_channel(struct wlan_objmgr_vdev *vdev)
{
	pkt_capture_record_channel(vdev);
}

int
ucfg_pkt_capture_register_wma_callbacks(struct wlan_objmgr_psoc *psoc,
					struct pkt_capture_callbacks *cb_obj)
{
	struct pkt_psoc_priv *psoc_priv = pkt_capture_psoc_get_priv(psoc);

	if (!psoc_priv) {
		pkt_capture_err("psoc priv is NULL");
		return -EINVAL;
	}

	psoc_priv->cb_obj.get_rmf_status = cb_obj->get_rmf_status;

	return 0;
}

QDF_STATUS
ucfg_pkt_capture_set_filter(struct pkt_capture_frame_filter frame_filter,
			    struct wlan_objmgr_vdev *vdev)
{
	return pkt_capture_set_filter(frame_filter, vdev);
}
