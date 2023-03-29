/*
 * Copyright (c) 2020-2021 The Linux Foundation. All rights reserved.
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
 * DOC: Declare public API related to the pkt_capture called by north bound
 * HDD/OSIF/LIM
 */

#ifndef _WLAN_PKT_CAPTURE_UCFG_API_H_
#define _WLAN_PKT_CAPTURE_UCFG_API_H_

#include <qdf_status.h>
#include <qdf_types.h>
#include "wlan_pkt_capture_objmgr.h"
#include "wlan_pkt_capture_public_structs.h"
#include "wlan_pkt_capture_mon_thread.h"
#include <htt_types.h>
#include "wlan_pkt_capture_data_txrx.h"
#include <ol_htt_api.h>

#ifdef WLAN_FEATURE_PKT_CAPTURE
/**
 * ucfg_pkt_capture_init() - Packet capture component initialization.
 *
 * This function gets called when packet capture initializing.
 *
 * Return: QDF_STATUS
 */
QDF_STATUS ucfg_pkt_capture_init(void);

/**
 * ucfg_pkt_capture_deinit() - Packet capture component de-init.
 *
 * This function gets called when packet capture de-init.
 *
 * Return: None
 */
void ucfg_pkt_capture_deinit(void);

/**
 * ucfg_pkt_capture_get_mode() - get packet capture mode
 * @psoc: objmgr psoc handle
 *
 * Return: enum pkt_capture_mode
 */
enum pkt_capture_mode
ucfg_pkt_capture_get_mode(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pkt_capture_suspend_mon_thread() - suspend packet capture mon thread
 * vdev: pointer to vdev object manager
 *
 * Return: 0 on success, -EINVAL on failure
 */
int ucfg_pkt_capture_suspend_mon_thread(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pkt_capture_resume_mon_thread() - resume packet capture mon thread
 * vdev: pointer to vdev object manager
 *
 * Resume packet capture MON thread by completing RX thread resume event
 *
 * Return: None
 */
void ucfg_pkt_capture_resume_mon_thread(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pkt_capture_register_callbacks - Register packet capture callbacks
 * @vdev: pointer to wlan vdev object manager
 * mon_cb: callback to call
 * context: callback context
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_pkt_capture_register_callbacks(struct wlan_objmgr_vdev *vdev,
				    QDF_STATUS (*mon_cb)(void *, qdf_nbuf_t),
				    void *context);

/**
 * ucfg_pkt_capture_deregister_callbacks - De-register packet capture callbacks
 * @vdev: pointer to wlan vdev object manager
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_pkt_capture_deregister_callbacks(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pkt_capturee_set_pktcap_mode - Set packet capture mode
 * @psoc: pointer to psoc object
 * @mode: mode to be set
 *
 * Return: None
 */
void ucfg_pkt_capture_set_pktcap_mode(struct wlan_objmgr_psoc *psoc,
				      enum pkt_capture_mode val);

/**
 * ucfg_pkt_capture_get_pktcap_mode - Get packet capture mode
 * @psoc: pointer to psoc object
 *
 * Return: enum pkt_capture_mode
 */
enum pkt_capture_mode
ucfg_pkt_capture_get_pktcap_mode(struct wlan_objmgr_psoc *psoc);

/**
 * ucfg_pkt_capturee_set_pktcap_config - Set packet capture config
 * @vdev: pointer to vdev object
 * @config: config to be set
 *
 * Return: None
 */
void ucfg_pkt_capture_set_pktcap_config(struct wlan_objmgr_vdev *vdev,
					enum pkt_capture_config config);

/**
 * ucfg_pkt_capture_get_pktcap_config - Get packet capture config
 * @vdev: pointer to vdev object
 *
 * Return: config value
 */
enum pkt_capture_config
ucfg_pkt_capture_get_pktcap_config(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pkt_capture_process_mgmt_tx_data() - process management tx packets
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
				      uint8_t status);

/**
 * ucfg_pkt_capture_mgmt_tx() - process mgmt tx completion
 * for pkt capture mode
 * @pdev: pointer to pdev object
 * @nbuf: netbuf
 * @chan_freq: channel freq
 * @preamble_type: preamble_type
 *
 * Return: none
 */
void
ucfg_pkt_capture_mgmt_tx(struct wlan_objmgr_pdev *pdev,
			 qdf_nbuf_t nbuf,
			 uint16_t chan_freq,
			 uint8_t preamble_type);

/**
 * ucfg_pkt_capture_mgmt_tx_completion() - process mgmt tx completion
 * for pkt capture mode
 * @pdev: pointer to pdev object
 * @desc_id: desc_id
 * @status: status
 * @params: management offload event params
 *
 * Return: none
 */
void
ucfg_pkt_capture_mgmt_tx_completion(
				struct wlan_objmgr_pdev *pdev,
				uint32_t desc_id,
				uint32_t status,
				struct mgmt_offload_event_params *params);

/**
 * ucfg_pkt_capture_rx_msdu_process() -  process data rx pkts
 * @bssid: bssid
 * @head_msdu: pointer to head msdu
 * @vdev_id: vdev_id
 * @pdev: pdev handle
 *
 * Return: none
 */
void ucfg_pkt_capture_rx_msdu_process(
				uint8_t *bssid,
				qdf_nbuf_t head_msdu,
				uint8_t vdev_id, htt_pdev_handle pdev);

/**
 * ucfg_pkt_capture_rx_offloaded_pkt() - check offloaded data pkt or not
 * @rx_ind_msg: rx_ind_msg
 *
 * Return: 0 not an offload pkt
 *         1 offload pkt
 */
bool ucfg_pkt_capture_rx_offloaded_pkt(qdf_nbuf_t rx_ind_msg);

/**
 * ucfg_pkt_capture_rx_drop_offload_pkt() - drop offload packets
 * @head_msdu: pointer to head msdu
 *
 * Return: none
 */
void ucfg_pkt_capture_rx_drop_offload_pkt(qdf_nbuf_t head_msdu);

/**
 * ucfg_pkt_capture_offload_deliver_indication_handler() - Handle offload
 * data pkts
 * @msg: offload netbuf msg
 * @vdev_id: vdev id
 * @bssid: bssid
 * @pdev: pdev handle
 *
 * Return: none
 */
void ucfg_pkt_capture_offload_deliver_indication_handler(
					void *msg, uint8_t vdev_id,
					uint8_t *bssid, htt_pdev_handle pdev);

/**
 * ucfg_pkt_capture_tx_get_txcomplete_data_hdr() - extract Tx data hdr from Tx
 * completion for pkt capture mode
 * @msg_word: Tx completion htt msg
 * @num_msdus: number of msdus
 *
 * Return: tx data hdr information
 */
struct htt_tx_data_hdr_information *ucfg_pkt_capture_tx_get_txcomplete_data_hdr(
		uint32_t *msg_word,
		int num_msdus);

/**
 * ucfg_pkt_capture_tx_completion_process() - process data tx packets
 * @vdev_id: vdev id for which packet is captured
 * @mon_buf_list: netbuf list
 * @type: data process type
 * @tid:  tid number
 * @status: Tx status
 * @pktformat: Frame format
 * @bssid: bssid
 * @pdev: pdev handle
 * @tx_retry_cnt: tx retry count
 *
 * Return: none
 */
void ucfg_pkt_capture_tx_completion_process(
			uint8_t vdev_id,
			qdf_nbuf_t mon_buf_list,
			enum pkt_capture_data_process_type type,
			uint8_t tid, uint8_t status, bool pkt_format,
			uint8_t *bssid, htt_pdev_handle pdev,
			uint8_t tx_retry_cnt);

/**
 * ucfg_pkt_capture_record_channel() - Update Channel Information
 * for packet capture mode
 * @vdev: pointer to vdev
 *
 * Return: None
 */
void ucfg_pkt_capture_record_channel(struct wlan_objmgr_vdev *vdev);

/**
 * ucfg_pkt_capture_register_callbacks - ucfg API to register WMA callbacks
 * @psoc: pointer to psoc object
 * @cb_obj: Pointer to packet capture callback structure
 *
 * Return: status of operation
 */
int
ucfg_pkt_capture_register_wma_callbacks(struct wlan_objmgr_psoc *psoc,
					struct pkt_capture_callbacks *cb_obj);

/**
 * ucfg_pkt_capture_set_filter ucfg API to set frame filter
 * @frame_filter: pkt capture frame filter data
 * @vdev: pointer to vdev
 *
 * Return: QDF_STATUS
 */
QDF_STATUS
ucfg_pkt_capture_set_filter(struct pkt_capture_frame_filter frame_filter,
			    struct wlan_objmgr_vdev *vdev);

#else
static inline
QDF_STATUS ucfg_pkt_capture_init(void)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_pkt_capture_deinit(void)
{
}

static inline
enum pkt_capture_mode ucfg_pkt_capture_get_mode(struct wlan_objmgr_psoc *psoc)
{
	return PACKET_CAPTURE_MODE_DISABLE;
}

static inline
void ucfg_pkt_capture_resume_mon_thread(struct wlan_objmgr_vdev *vdev)
{
}

static inline
int ucfg_pkt_capture_suspend_mon_thread(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

static inline QDF_STATUS
ucfg_pkt_capture_register_callbacks(struct wlan_objmgr_vdev *vdev,
				    QDF_STATUS (*mon_cb)(void *, qdf_nbuf_t),
				    void *context)
{
	return QDF_STATUS_SUCCESS;
}

static inline
QDF_STATUS ucfg_pkt_capture_deregister_callbacks(struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

static inline
void ucfg_pkt_capture_set_pktcap_mode(struct wlan_objmgr_psoc *psoc,
				      uint8_t val)
{
}

static inline enum pkt_capture_mode
ucfg_pkt_capture_get_pktcap_mode(struct wlan_objmgr_psoc *psoc)
{
	return PACKET_CAPTURE_MODE_DISABLE;
}

static inline
void ucfg_pkt_capture_set_pktcap_config(struct wlan_objmgr_vdev *vdev,
					enum pkt_capture_config config)
{
}

static inline enum pkt_capture_config
ucfg_pkt_capture_get_pktcap_config(struct wlan_objmgr_vdev *vdev)
{
	return 0;
}

static inline QDF_STATUS
ucfg_pkt_capture_process_mgmt_tx_data(
				struct mgmt_offload_event_params *params,
				qdf_nbuf_t nbuf,
				uint8_t status)
{
	return 0;
}

static inline void
ucfg_pkt_capture_mgmt_tx(struct wlan_objmgr_pdev *pdev,
			 qdf_nbuf_t nbuf,
			 uint16_t chan_freq,
			 uint8_t preamble_type)
{
}

static inline void
ucfg_pkt_capture_mgmt_tx_completion(struct wlan_objmgr_pdev *pdev,
				    uint32_t desc_id,
				    uint32_t status,
				    struct mgmt_offload_event_params *params)
{
}

static inline void
ucfg_pkt_capture_offload_deliver_indication_handler(
					void *msg, uint8_t vdev_id,
					uint8_t *bssid, htt_pdev_handle pdev)
{
}

static inline
struct htt_tx_data_hdr_information *ucfg_pkt_capture_tx_get_txcomplete_data_hdr(
		uint32_t *msg_word,
		int num_msdus)
{
	return NULL;
}

static inline void
ucfg_pkt_capture_rx_msdu_process(
				uint8_t *bssid,
				qdf_nbuf_t head_msdu,
				uint8_t vdev_id, htt_pdev_handle pdev)
{
}

static inline bool
ucfg_pkt_capture_rx_offloaded_pkt(qdf_nbuf_t rx_ind_msg)
{
	return false;
}

static inline void
ucfg_pkt_capture_rx_drop_offload_pkt(qdf_nbuf_t head_msdu)
{
}

static inline void
ucfg_pkt_capture_tx_completion_process(
			uint8_t vdev_id,
			qdf_nbuf_t mon_buf_list,
			enum pkt_capture_data_process_type type,
			uint8_t tid, uint8_t status, bool pkt_format,
			uint8_t *bssid, htt_pdev_handle pdev,
			uint8_t tx_retry_cnt)
{
}

static inline void
ucfg_pkt_capture_record_channel(struct wlan_objmgr_vdev *vdev)
{
}

static inline QDF_STATUS
ucfg_pkt_capture_set_filter(struct pkt_capture_frame_filter frame_filter,
			    struct wlan_objmgr_vdev *vdev)
{
	return QDF_STATUS_SUCCESS;
}

#endif /* WLAN_FEATURE_PKT_CAPTURE */
#endif /* _WLAN_PKT_CAPTURE_UCFG_API_H_ */
