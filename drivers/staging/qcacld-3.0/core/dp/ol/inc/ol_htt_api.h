/*
 * Copyright (c) 2011, 2014-2019-2020 The Linux Foundation. All rights reserved.
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
 * @file ol_htt_api.h
 * @brief Specify the general HTT API functions called by the host data SW.
 * @details
 *  This file declares the HTT API functions that are not specific to
 *  either tx nor rx.
 */
#ifndef _OL_HTT_API__H_
#define _OL_HTT_API__H_

#include <qdf_types.h>          /* qdf_device_t */
#include <qdf_nbuf.h>           /* qdf_nbuf_t */
#include <athdefs.h>            /* A_STATUS */
#include <htc_api.h>            /* HTC_HANDLE */
#include "htt.h"                /* htt_dbg_stats_type, etc. */
#include <cdp_txrx_cmn.h>       /* ol_pdev_handle */
#include <ol_defines.h>
#include <cdp_txrx_handle.h>

struct htt_pdev_t;
typedef struct htt_pdev_t *htt_pdev_handle;

htt_pdev_handle
htt_pdev_alloc(ol_txrx_pdev_handle txrx_pdev,
	struct cdp_cfg *ctrl_pdev,
	HTC_HANDLE htc_pdev, qdf_device_t osdev);

/**
 * @brief Allocate and initialize a HTT instance.
 * @details
 *  This function allocates and initializes an HTT instance.
 *  This involves allocating a pool of HTT tx descriptors in
 *  consistent memory, allocating and filling a rx ring (LL only),
 *  and connecting the HTC's HTT_DATA_MSG service.
 *  The HTC service connect call will block, so this function
 *  needs to be called in passive context.
 *  Because HTC setup has not been completed at the time this function
 *  is called, this function cannot send any HTC messages to the target.
 *  Messages to configure the target are instead sent in the
 *  htc_attach_target function.
 *
 * @param pdev - data SW's physical device handle
 *      (used as context pointer during HTT -> txrx calls)
 * @param desc_pool_size - number of HTT descriptors to (pre)allocate
 * @return success -> HTT pdev handle; failure -> NULL
 */
int
htt_attach(struct htt_pdev_t *pdev, int desc_pool_size);

/**
 * @brief Send HTT configuration messages to the target.
 * @details
 *  For LL only, this function sends a rx ring configuration message to the
 *  target.  For HL, this function is a no-op.
 *
 * @param htt_pdev - handle to the HTT instance being initialized
 */
QDF_STATUS htt_attach_target(htt_pdev_handle htt_pdev);

/**
 * enum htt_op_mode - Virtual device operation mode
 *
 * @htt_op_mode_unknown: Unknown mode
 * @htt_op_mode_ap: AP mode
 * @htt_op_mode_ibss: IBSS mode
 * @htt_op_mode_sta: STA (client) mode
 * @htt_op_mode_monitor: Monitor mode
 * @htt_op_mode_ocb: OCB mode
 */
enum htt_op_mode {
	htt_op_mode_unknown,
	htt_op_mode_ap,
	htt_op_mode_ibss,
	htt_op_mode_sta,
	htt_op_mode_monitor,
	htt_op_mode_ocb,
};

/* no-ops */
#define htt_vdev_attach(htt_pdev, vdev_id, op_mode)
#define htt_vdev_detach(htt_pdev, vdev_id)
#define htt_peer_qos_update(htt_pdev, peer_id, qos_capable)
#define htt_peer_uapsdmask_update(htt_pdev, peer_id, uapsd_mask)

void htt_pdev_free(htt_pdev_handle pdev);

/**
 * @brief Deallocate a HTT instance.
 *
 * @param htt_pdev - handle to the HTT instance being torn down
 */
void htt_detach(htt_pdev_handle htt_pdev);

/**
 * @brief Stop the communication between HTT and target
 * @details
 *  For ISOC solution, this function stop the communication between HTT and
 *  target.
 *  For Peregrine/Rome, it's already stopped by ol_ath_disconnect_htc
 *  before ol_txrx_pdev_detach called in ol_ath_detach. So this function is
 *  a no-op.
 *  Peregrine/Rome HTT layer is on top of HTC while ISOC solution HTT layer is
 *  on top of DXE layer.
 *
 * @param htt_pdev - handle to the HTT instance being initialized
 */
void htt_detach_target(htt_pdev_handle htt_pdev);

/*
 * @brief Tell the target side of HTT to suspend H2T processing until synced
 * @param htt_pdev - the host HTT object
 * @param sync_cnt - what sync count value the target HTT FW should wait for
 *      before resuming H2T processing
 */
A_STATUS htt_h2t_sync_msg(htt_pdev_handle htt_pdev, uint8_t sync_cnt);

int
htt_h2t_aggr_cfg_msg(htt_pdev_handle htt_pdev,
		     int max_subfrms_ampdu, int max_subfrms_amsdu);

/**
 * @brief Get the FW status
 * @details
 *  Trigger FW HTT to retrieve FW status.
 *  A separate HTT message will come back with the statistics we want.
 *
 * @param pdev - handle to the HTT instance
 * @param stats_type_upload_mask - bitmask identifying which stats to upload
 * @param stats_type_reset_mask - bitmask identifying which stats to reset
 * @param cookie - unique value to distinguish and identify stats requests
 * @return 0 - succeed to send the request to FW; otherwise, failed to do so.
 */
int
htt_h2t_dbg_stats_get(struct htt_pdev_t *pdev,
		      uint32_t stats_type_upload_mask,
		      uint32_t stats_type_reset_mask,
		      uint8_t cfg_stats_type,
		      uint32_t cfg_val, uint8_t cookie);

/**
 * @brief Get the fields from HTT T2H stats upload message's stats info header
 * @details
 *  Parse the a HTT T2H message's stats info tag-length-value header,
 *  to obtain the stats type, status, data length, and data address.
 *
 * @param stats_info_list - address of stats record's header
 * @param[out] type - which type of FW stats are contained in the record
 * @param[out] status - whether the stats are (fully) present in the record
 * @param[out] length - how large the data portion of the stats record is
 * @param[out] stats_data - where the data portion of the stats record is
 */
void
htt_t2h_dbg_stats_hdr_parse(uint8_t *stats_info_list,
			    enum htt_dbg_stats_type *type,
			    enum htt_dbg_stats_status *status,
			    int *length, uint8_t **stats_data);

/**
 * @brief Display a stats record from the HTT T2H STATS_CONF message.
 * @details
 *  Parse the stats type and status, and invoke a type-specified printout
 *  to display the stats values.
 *
 *  @param stats_data - buffer holding the stats record from the STATS_CONF msg
 *  @param concise - whether to do a verbose or concise printout
 */
void htt_t2h_stats_print(uint8_t *stats_data, int concise);

/**
 * htt_log_rx_ring_info() - log htt rx ring info during FW_RX_REFILL failure
 * @pdev: handle to the HTT instance
 *
 * Return: None
 */
void htt_log_rx_ring_info(htt_pdev_handle pdev);

/**
 * htt_rx_refill_failure() - During refill failure check if debt is zero
 * @pdev: handle to the HTT instance
 *
 * Return: None
 */
void htt_rx_refill_failure(htt_pdev_handle pdev);

#ifndef HTT_DEBUG_LEVEL
#if defined(DEBUG)
#define HTT_DEBUG_LEVEL 10
#else
#define HTT_DEBUG_LEVEL 0
#endif
#endif

#if HTT_DEBUG_LEVEL > 5
void htt_display(htt_pdev_handle pdev, int indent);
#else
#define htt_display(pdev, indent)
#endif

#define HTT_DXE_RX_LOG 0
#define htt_rx_reorder_log_print(pdev)

#ifdef IPA_OFFLOAD
int htt_h2t_ipa_uc_rsc_cfg_msg(struct htt_pdev_t *pdev);

/**
 * htt_ipa_uc_get_resource() - Get uc resource from htt and lower layer
 * @pdev - handle to the HTT instance
 * @ce_sr - CE source ring DMA mapping info
 * @tx_comp_ring - tx completion ring DMA mapping info
 * @rx_rdy_ring - rx Ready ring DMA mapping info
 * @rx2_rdy_ring - rx2 Ready ring DMA mapping info
 * @rx_proc_done_idx - rx process done index
 * @rx2_proc_done_idx - rx2 process done index
 * @ce_sr_ring_size: copyengine source ring size
 * @ce_reg_paddr - CE Register address
 * @tx_num_alloc_buffer - Number of TX allocated buffers
 *
 * Return: 0 success
 */
int
htt_ipa_uc_get_resource(htt_pdev_handle pdev,
			qdf_shared_mem_t **ce_sr,
			qdf_shared_mem_t **tx_comp_ring,
			qdf_shared_mem_t **rx_rdy_ring,
			qdf_shared_mem_t **rx2_rdy_ring,
			qdf_shared_mem_t **rx_proc_done_idx,
			qdf_shared_mem_t **rx2_proc_done_idx,
			uint32_t *ce_sr_ring_size,
			qdf_dma_addr_t *ce_reg_paddr,
			uint32_t *tx_num_alloc_buffer);

int
htt_ipa_uc_set_doorbell_paddr(htt_pdev_handle pdev,
			      qdf_dma_addr_t ipa_uc_tx_doorbell_paddr,
			      qdf_dma_addr_t ipa_uc_rx_doorbell_paddr);

int
htt_h2t_ipa_uc_set_active(struct htt_pdev_t *pdev, bool uc_active, bool is_tx);

int htt_h2t_ipa_uc_get_stats(struct htt_pdev_t *pdev);

int htt_h2t_ipa_uc_get_share_stats(struct htt_pdev_t *pdev,
				   uint8_t reset_stats);

int htt_h2t_ipa_uc_set_quota(struct htt_pdev_t *pdev, uint64_t quota_bytes);

int htt_ipa_uc_attach(struct htt_pdev_t *pdev);

void htt_ipa_uc_detach(struct htt_pdev_t *pdev);
#else
/**
 * htt_h2t_ipa_uc_rsc_cfg_msg() - Send WDI IPA config message to firmware
 * @pdev: handle to the HTT instance
 *
 * Return: 0 success
 */
static inline int htt_h2t_ipa_uc_rsc_cfg_msg(struct htt_pdev_t *pdev)
{
	return 0;
}

/**
 * htt_ipa_uc_set_doorbell_paddr() - Propagate IPA doorbell address
 * @pdev: handle to the HTT instance
 * @ipa_uc_tx_doorbell_paddr: TX doorbell base physical address
 * @ipa_uc_rx_doorbell_paddr: RX doorbell base physical address
 *
 * Return: 0 success
 */
static inline int
htt_ipa_uc_set_doorbell_paddr(htt_pdev_handle pdev,
			      uint32_t ipa_uc_tx_doorbell_paddr,
			      uint32_t ipa_uc_rx_doorbell_paddr)
{
	return 0;
}

/**
 * htt_h2t_ipa_uc_set_active() - Propagate WDI path enable/disable to firmware
 * @pdev: handle to the HTT instance
 * @uc_active: WDI UC path enable or not
 * @is_tx: TX path or RX path
 *
 * Return: 0 success
 */
static inline int
htt_h2t_ipa_uc_set_active(struct htt_pdev_t *pdev, bool uc_active,
	bool is_tx)
{
	return 0;
}

/**
 * htt_h2t_ipa_uc_get_stats() - WDI UC state query request to firmware
 * @pdev: handle to the HTT instance
 *
 * Return: 0 success
 */
static inline int htt_h2t_ipa_uc_get_stats(struct htt_pdev_t *pdev)
{
	return 0;
}

/**
 * htt_h2t_ipa_uc_get_share_stats() - WDI UC wifi sharing state request to FW
 * @pdev: handle to the HTT instance
 *
 * Return: 0 success
 */
static inline int htt_h2t_ipa_uc_get_share_stats(struct htt_pdev_t *pdev,
						uint8_t reset_stats)
{
	return 0;
}

/**
 * htt_h2t_ipa_uc_set_quota() - WDI UC set quota request to firmware
 * @pdev: handle to the HTT instance
 *
 * Return: 0 success
 */
static inline int htt_h2t_ipa_uc_set_quota(struct htt_pdev_t *pdev,
					   uint64_t quota_bytes)
{
	return 0;
}

/**
 * htt_ipa_uc_attach() - Allocate UC data path resources
 * @pdev: handle to the HTT instance
 *
 * Return: 0 success
 */
static inline int htt_ipa_uc_attach(struct htt_pdev_t *pdev)
{
	return 0;
}

/**
 * htt_ipa_uc_attach() - Remove UC data path resources
 * @pdev: handle to the HTT instance
 *
 * Return: 0 success
 */
static inline void htt_ipa_uc_detach(struct htt_pdev_t *pdev)
{
}
#endif /* IPA_OFFLOAD */

#ifdef FEATURE_MONITOR_MODE_SUPPORT
void htt_rx_mon_note_capture_channel(htt_pdev_handle pdev, int mon_ch);

void ol_htt_mon_note_chan(struct cdp_pdev *ppdev, int mon_ch);
#else
static inline
void htt_rx_mon_note_capture_channel(htt_pdev_handle pdev, int mon_ch) {}

static inline
void ol_htt_mon_note_chan(struct cdp_pdev *ppdev, int mon_ch) {}
#endif

#if defined(DEBUG_HL_LOGGING) && defined(CONFIG_HL_SUPPORT)

void htt_dump_bundle_stats(struct htt_pdev_t *pdev);
void htt_clear_bundle_stats(struct htt_pdev_t *pdev);
#else

static inline void htt_dump_bundle_stats(struct htt_pdev_t *pdev)
{
}

static inline void htt_clear_bundle_stats(struct htt_pdev_t *pdev)
{
}
#endif

void htt_mark_first_wakeup_packet(htt_pdev_handle pdev, uint8_t value);

typedef void (*tp_rx_pkt_dump_cb)(qdf_nbuf_t msdu, uint8_t peer_id,
			uint8_t status);
#ifdef REMOVE_PKT_LOG
static inline
void htt_register_rx_pkt_dump_callback(struct htt_pdev_t *pdev,
				       tp_rx_pkt_dump_cb ol_rx_pkt_dump_call)
{
}

static inline
void htt_deregister_rx_pkt_dump_callback(struct htt_pdev_t *pdev)
{
}

static inline
void ol_rx_pkt_dump_call(qdf_nbuf_t msdu, uint8_t peer_id, uint8_t status)
{
}
#else
void htt_register_rx_pkt_dump_callback(struct htt_pdev_t *pdev,
				       tp_rx_pkt_dump_cb ol_rx_pkt_dump_call);
void htt_deregister_rx_pkt_dump_callback(struct htt_pdev_t *pdev);
void ol_rx_pkt_dump_call(qdf_nbuf_t msdu, uint8_t peer_id, uint8_t status);
#endif

#endif /* _OL_HTT_API__H_ */
