/*
 * Copyright (c) 2011, 2014 The Linux Foundation. All rights reserved.
 *
 * Previously licensed under the ISC license by Qualcomm Atheros, Inc.
 *
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
 * This file was originally distributed by Qualcomm Atheros, Inc.
 * under proprietary terms before Copyright ownership was assigned
 * to the Linux Foundation.
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

#include <adf_os_types.h> /* adf_os_device_t */
#include <adf_nbuf.h>     /* adf_nbuf_t */
#include <athdefs.h>      /* A_STATUS */
#include <htc_api.h>      /* HTC_HANDLE */
#include <ol_ctrl_api.h>  /* ol_pdev_handle */
#include <ol_txrx_api.h>  /* ol_txrx_pdev_handle */
#include "htt.h"          /* htt_dbg_stats_type, etc. */

/* TID */
#define OL_HTT_TID_NON_QOS_UNICAST     16
#define OL_HTT_TID_NON_QOS_MCAST_BCAST 18


struct htt_pdev_t;
typedef struct htt_pdev_t *htt_pdev_handle;

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
 * @param txrx_pdev - data SW's physical device handle
 *      (used as context pointer during HTT -> txrx calls)
 * @param ctrl_pdev - control SW's physical device handle
 *      (used to query configuration functions)
 * @param osdev - abstract OS device handle
 *      (used for mem allocation)
 * @param desc_pool_size - number of HTT descriptors to (pre)allocate
 * @return success -> HTT pdev handle; failure -> NULL
 */
htt_pdev_handle
htt_attach(
    ol_txrx_pdev_handle txrx_pdev,
    ol_pdev_handle ctrl_pdev,
    HTC_HANDLE htc_pdev,
    adf_os_device_t osdev,
    int desc_pool_size);

/**
 * @brief Send HTT configuration messages to the target.
 * @details
 *  For LL only, this function sends a rx ring configuration message to the
 *  target.  For HL, this function is a no-op.
 *
 * @param htt_pdev - handle to the HTT instance being initialized
 */
A_STATUS
htt_attach_target(htt_pdev_handle htt_pdev);

/**
 * @brief modes that a virtual device can operate as
 * @details
 *  A virtual device can operate as an AP, an IBSS, or a STA (client).
 *  or in monitor mode
 */
enum htt_op_mode {
   htt_op_mode_unknown,
   htt_op_mode_ap,
   htt_op_mode_ibss,
   htt_op_mode_sta,
   htt_op_mode_monitor,
};

/* no-ops */
#define htt_vdev_attach(htt_pdev, vdev_id, op_mode)
#define htt_vdev_detach(htt_pdev, vdev_id)
#define htt_peer_qos_update(htt_pdev, peer_id, qos_capable)
#define htt_peer_uapsdmask_update(htt_pdev, peer_id, uapsd_mask)

/**
 * @brief Deallocate a HTT instance.
 *
 * @param htt_pdev - handle to the HTT instance being torn down
 */
void
htt_detach(htt_pdev_handle htt_pdev);

/**
 * @brief Stop the communication between HTT and target
 * @details
 *  For ISOC solution, this function stop the communication between HTT and target.
 *  For Peregrine/Rome, it's already stopped by ol_ath_disconnect_htc
 *  before ol_txrx_pdev_detach called in ol_ath_detach. So this function is a no-op.
 *  Peregrine/Rome HTT layer is on top of HTC while ISOC solution HTT layer is
 *  on top of DXE layer.
 *
 * @param htt_pdev - handle to the HTT instance being initialized
 */
void
htt_detach_target(htt_pdev_handle htt_pdev);

/*
 * @brief Tell the target side of HTT to suspend H2T processing until synced
 * @param htt_pdev - the host HTT object
 * @param sync_cnt - what sync count value the target HTT FW should wait for
 *      before resuming H2T processing
 */
A_STATUS
htt_h2t_sync_msg(htt_pdev_handle htt_pdev, u_int8_t sync_cnt);


int
htt_h2t_aggr_cfg_msg(htt_pdev_handle htt_pdev,
                     int max_subfrms_ampdu,
                     int max_subfrms_amsdu);

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
htt_h2t_dbg_stats_get(
    struct htt_pdev_t *pdev,
    u_int32_t stats_type_upload_mask,
    u_int32_t stats_type_reset_mask,
    u_int8_t cfg_stats_type,
    u_int32_t cfg_val,
    u_int64_t cookie);

/**
 * @brief Get the fields from HTT T2H stats upload message's stats info header
 * @details
 *  Parse the a HTT T2H message's stats info tag-length-value header,
 *  to obtain the stats type, status, data lenght, and data address.
 *
 * @param stats_info_list - address of stats record's header
 * @param[out] type - which type of FW stats are contained in the record
 * @param[out] status - whether the stats are (fully) present in the record
 * @param[out] length - how large the data portion of the stats record is
 * @param[out] stats_data - where the data portion of the stats record is
 */
void
htt_t2h_dbg_stats_hdr_parse(
    u_int8_t *stats_info_list,
    enum htt_dbg_stats_type *type,
    enum htt_dbg_stats_status *status,
    int *length,
    u_int8_t **stats_data);

/**
 * @brief Display a stats record from the HTT T2H STATS_CONF message.
 * @details
 *  Parse the stats type and status, and invoke a type-specified printout
 *  to display the stats values.
 *
 *  @param stats_data - buffer holding the stats record from the STATS_CONF msg
 *  @param concise - whether to do a verbose or concise printout
 */
void
htt_t2h_stats_print(u_int8_t *stats_data, int concise);

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

#ifdef IPA_UC_OFFLOAD
/**
 * @brief send IPA UC resource config message to firmware with HTT message
 * @details
 *  send IPA UC resource config message to firmware with HTT message
 *
 * @param pdev - handle to the HTT instance
 */
int
htt_h2t_ipa_uc_rsc_cfg_msg(struct htt_pdev_t *pdev);

/**
 * @brief Client request resource information
 * @details
 *  OL client will reuqest IPA UC related resource information
 *  Resource information will be distributted to IPA module
 *  All of the required resources should be pre-allocated
 *
 * @param pdev - handle to the HTT instance
 * @param ce_sr_base_paddr - copy engine source ring base physical address
 * @param ce_sr_ring_size - copy engine source ring size
 * @param ce_reg_paddr - copy engine register physical address
 * @param tx_comp_ring_base_paddr - tx comp ring base physical address
 * @param tx_comp_ring_size - tx comp ring size
 * @param tx_num_alloc_buffer - number of allocated tx buffer
 * @param rx_rdy_ring_base_paddr - rx ready ring base physical address
 * @param rx_rdy_ring_size - rx ready ring size
 * @param rx_proc_done_idx_paddr - rx process done index physical address
 */
int
htt_ipa_uc_get_resource(htt_pdev_handle pdev,
   a_uint32_t *ce_sr_base_paddr,
   a_uint32_t *ce_sr_ring_size,
   a_uint32_t *ce_reg_paddr,
   a_uint32_t *tx_comp_ring_base_paddr,
   a_uint32_t *tx_comp_ring_size,
   a_uint32_t *tx_num_alloc_buffer,
   a_uint32_t *rx_rdy_ring_base_paddr,
   a_uint32_t *rx_rdy_ring_size,
   a_uint32_t *rx_proc_done_idx_paddr);

/**
 * @brief Client set IPA UC doorbell register
 * @details
 *  IPA UC let know doorbell register physical address
 *  WLAN firmware will use this physical address to notify IPA UC
 *
 * @param pdev - handle to the HTT instance
 * @param ipa_uc_tx_doorbell_paddr - tx comp doorbell physical address
 * @param ipa_uc_rx_doorbell_paddr - rx ready doorbell physical address
 */
int
htt_ipa_uc_set_doorbell_paddr(htt_pdev_handle pdev,
           a_uint32_t ipa_uc_tx_doorbell_paddr,
           a_uint32_t ipa_uc_rx_doorbell_paddr);

/**
 * @brief Client notify IPA UC data path active or not
 *
 * @param pdev - handle to the HTT instance
 * @param uc_active - UC data path is active or not
 * @param is_tx - UC TX is active or not
 */
int
htt_h2t_ipa_uc_set_active(struct htt_pdev_t *pdev,
   a_bool_t uc_active,
   a_bool_t is_tx);

/**
 * @brief query uc data path stats
 *
 * @param pdev - handle to the HTT instance
 */
int
htt_h2t_ipa_uc_get_stats(struct htt_pdev_t *pdev);

/**
 * @brief Attach IPA UC data path
 *
 * @param pdev - handle to the HTT instance
 */
int
htt_ipa_uc_attach(struct htt_pdev_t *pdev);

/**
 * @brief detach IPA UC data path
 *
 * @param pdev - handle to the HTT instance
 */
void
htt_ipa_uc_detach(struct htt_pdev_t *pdev);
#endif /* IPA_UC_OFFLOAD */

#endif /* _OL_HTT_API__H_ */
