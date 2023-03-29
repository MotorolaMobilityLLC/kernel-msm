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

#ifndef _OL_TXRX__H_
#define _OL_TXRX__H_

#include <qdf_nbuf.h>           /* qdf_nbuf_t */
#include <cdp_txrx_cmn.h>       /* ol_txrx_vdev_t, etc. */
#include "cds_sched.h"
#include <cdp_txrx_handle.h>
#include <ol_txrx_types.h>
#include <ol_txrx_internal.h>
#include <qdf_hrtimer.h>

/*
 * Pool of tx descriptors reserved for
 * high-priority traffic, such as ARP/EAPOL etc
 * only for forwarding path.
 */
#define OL_TX_NON_FWD_RESERVE	100

/**
 * enum ol_txrx_fc_limit_id - Flow control identifier for
 * vdev limits based on band, channel bw and number of spatial streams
 * @TXRX_FC_5GH_80M_2x2: Limit for 5GHz, 80MHz BW, 2x2 NSS
 * @TXRX_FC_5GH_40M_2x2:
 * @TXRX_FC_5GH_20M_2x2:
 * @TXRX_FC_5GH_80M_1x1:
 * @TXRX_FC_5GH_40M_1x1:
 * @TXRX_FC_5GH_20M_1x1:
 * @TXRX_FC_2GH_40M_2x2:
 * @TXRX_FC_2GH_20M_2x2:
 * @TXRX_FC_2GH_40M_1x1:
 * @TXRX_FC_2GH_20M_1x1:
 */
enum ol_txrx_fc_limit_id {
	TXRX_FC_5GH_80M_2x2,
	TXRX_FC_5GH_40M_2x2,
	TXRX_FC_5GH_20M_2x2,
	TXRX_FC_5GH_80M_1x1,
	TXRX_FC_5GH_40M_1x1,
	TXRX_FC_5GH_20M_1x1,
	TXRX_FC_2GH_40M_2x2,
	TXRX_FC_2GH_20M_2x2,
	TXRX_FC_2GH_40M_1x1,
	TXRX_FC_2GH_20M_1x1,
	TXRX_FC_MAX
};

#define TXRX_RFS_ENABLE_PEER_ID_UNMAP_COUNT    3
#define TXRX_RFS_DISABLE_PEER_ID_UNMAP_COUNT   1

ol_txrx_peer_handle ol_txrx_peer_get_ref_by_addr(ol_txrx_pdev_handle pdev,
						 u8 *peer_addr,
						 enum peer_debug_id_type
						 dbg_id);

int  ol_txrx_peer_release_ref(ol_txrx_peer_handle peer,
			      enum peer_debug_id_type dbg_id);

/**
 * ol_txrx_soc_attach() - initialize the soc
 * @scn_handle: Opaque SOC handle from control plane
 * @dp_ol_if_ops: Offload Operations
 *
 * Return: SOC handle on success, NULL on failure
 */
ol_txrx_soc_handle ol_txrx_soc_attach(void *scn_handle,
				      struct ol_if_ops *dp_ol_if_ops);

/**
 * ol_tx_desc_pool_size_hl() - allocate tx descriptor pool size for HL systems
 * @ctrl_pdev: the control pdev handle
 *
 * Return: allocated pool size
 */
u_int16_t
ol_tx_desc_pool_size_hl(struct cdp_cfg *ctrl_pdev);

#ifndef OL_TX_AVG_FRM_BYTES
#define OL_TX_AVG_FRM_BYTES 1000
#endif

#ifndef OL_TX_DESC_POOL_SIZE_MIN_HL
#define OL_TX_DESC_POOL_SIZE_MIN_HL 500
#endif

#ifndef OL_TX_DESC_POOL_SIZE_MAX_HL
#define OL_TX_DESC_POOL_SIZE_MAX_HL 5000
#endif

#ifndef FW_STATS_DESC_POOL_SIZE
#define FW_STATS_DESC_POOL_SIZE 10
#endif

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
#define TXRX_HL_TX_FLOW_CTRL_VDEV_LOW_WATER_MARK 400
#define TXRX_HL_TX_FLOW_CTRL_MGMT_RESERVED 100
#endif

#define TXRX_HL_TX_DESC_HI_PRIO_RESERVED 20
#define TXRX_HL_TX_DESC_QUEUE_RESTART_TH \
		(TXRX_HL_TX_DESC_HI_PRIO_RESERVED + 100)

struct peer_hang_data {
	uint16_t tlv_header;
	uint8_t peer_mac_addr[QDF_MAC_ADDR_SIZE];
	uint16_t peer_timeout_bitmask;
} qdf_packed;

#if defined(CONFIG_HL_SUPPORT) && defined(FEATURE_WLAN_TDLS)

void
ol_txrx_hl_tdls_flag_reset(struct cdp_soc_t *soc_hdl,
			   uint8_t vdev_id, bool flag);
#else

static inline void
ol_txrx_hl_tdls_flag_reset(struct cdp_soc_t *soc_hdl,
			   uint8_t vdev_id, bool flag)
{
}
#endif

#ifdef WDI_EVENT_ENABLE
void *ol_get_pldev(struct cdp_soc_t *soc, uint8_t pdev_id);
#else
static inline
void *ol_get_pldev(struct cdp_soc_t *soc, uint8_t pdev_id)
{
	return NULL;
}
#endif

#ifdef QCA_SUPPORT_TXRX_LOCAL_PEER_ID
ol_txrx_peer_handle
ol_txrx_peer_get_ref_by_local_id(struct cdp_pdev *ppdev,
				 uint8_t local_peer_id,
				 enum peer_debug_id_type dbg_id);
#endif /* QCA_SUPPORT_TXRX_LOCAL_PEER_ID */

/**
 * ol_txrx_get_pdev_from_pdev_id() - Returns pdev object given the pdev id
 * @soc: core DP soc context
 * @pdev_id: pdev id from pdev object can be retrieved
 *
 * Return: Pointer to DP pdev object
 */

static inline struct ol_txrx_pdev_t *
ol_txrx_get_pdev_from_pdev_id(struct ol_txrx_soc_t *soc,
			      uint8_t pdev_id)
{
	return soc->pdev_list[pdev_id];
}

/*
 * @nbuf: buffer which contains data to be displayed
 * @nbuf_paddr: physical address of the buffer
 * @len: defines the size of the data to be displayed
 *
 * Return: None
 */
void
ol_txrx_dump_pkt(qdf_nbuf_t nbuf, uint32_t nbuf_paddr, int len);

/**
 * ol_txrx_get_vdev_from_vdev_id() - get vdev from vdev_id
 * @vdev_id: vdev_id
 *
 * Return: vdev handle
 *            NULL if not found.
 */
struct cdp_vdev *ol_txrx_get_vdev_from_vdev_id(uint8_t vdev_id);

/**
 * ol_txrx_get_vdev_from_soc_vdev_id() - get vdev from soc and vdev_id
 * @soc: datapath soc handle
 * @vdev_id: vdev_id
 *
 * Return: vdev handle
 *            NULL if not found.
 */
struct ol_txrx_vdev_t *ol_txrx_get_vdev_from_soc_vdev_id(
				struct ol_txrx_soc_t *soc, uint8_t vdev_id);

/**
 * ol_txrx_get_mon_vdev_from_pdev() - get monitor mode vdev from pdev
 * @soc: datapath soc handle
 * @pdev_id: the physical device id the virtual device belongs to
 *
 * Return: vdev id
 *         error if not found.
 */
uint8_t ol_txrx_get_mon_vdev_from_pdev(struct cdp_soc_t *soc,
				       uint8_t pdev_id);

/**
 * ol_txrx_get_vdev_by_peer_addr() - Get vdev handle by peer mac address
 * @ppdev - data path device instance
 * @peer_addr - peer mac address
 *
 * Get virtual interface handle by local peer mac address
 *
 * Return: Virtual interface instance handle
 *         NULL in case cannot find
 */
ol_txrx_vdev_handle
ol_txrx_get_vdev_by_peer_addr(struct cdp_pdev *ppdev,
			      struct qdf_mac_addr peer_addr);

void *ol_txrx_find_peer_by_addr(struct cdp_pdev *pdev,
				uint8_t *peer_addr);

/**
 * @brief specify the peer's authentication state
 * @details
 *  Specify the peer's authentication state (none, connected, authenticated)
 *  to allow the data SW to determine whether to filter out invalid data frames.
 *  (In the "connected" state, where security is enabled, but authentication
 *  has not completed, tx and rx data frames other than EAPOL or WAPI should
 *  be discarded.)
 *  This function is only relevant for systems in which the tx and rx filtering
 *  are done in the host rather than in the target.
 *
 * @param soc - datapath soc handle
 * @param peer_mac - mac address of which peer has changed its state
 * @param state - the new state of the peer
 *
 * Return: QDF Status
 */
QDF_STATUS ol_txrx_peer_state_update(struct cdp_soc_t *soc_hdl,
				     uint8_t *peer_mac,
				     enum ol_txrx_peer_state state);

void htt_pkt_log_init(struct cdp_soc_t *soc_hdl, uint8_t pdev_id, void *scn);
void peer_unmap_timer_handler(void *data);

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
/**
 * ol_txrx_register_tx_flow_control() - register tx flow control callback
 * @soc_hdl: soc handle
 * @vdev_id: vdev_id
 * @flowControl: flow control callback
 * @osif_fc_ctx: callback context
 * @flow_control_is_pause: is vdev paused by flow control
 *
 * Return: 0 for success or error code
 */
int ol_txrx_register_tx_flow_control(struct cdp_soc_t *soc_hdl,
				     uint8_t vdev_id,
				     ol_txrx_tx_flow_control_fp flow_control,
				     void *osif_fc_ctx,
				     ol_txrx_tx_flow_control_is_pause_fp
				     flow_control_is_pause);

/**
 * ol_txrx_de_register_tx_flow_control_cb() - deregister tx flow control
 *                                            callback
 * @soc_hdl: soc handle
 * @vdev_id: vdev_id
 *
 * Return: 0 for success or error code
 */
int ol_txrx_deregister_tx_flow_control_cb(struct cdp_soc_t *soc_hdl,
					  uint8_t vdev_id);

bool ol_txrx_get_tx_resource(struct cdp_soc_t *soc_hdl, uint8_t pdev_id,
			     struct qdf_mac_addr peer_addr,
			     unsigned int low_watermark,
			     unsigned int high_watermark_offset);

/**
 * ol_txrx_ll_set_tx_pause_q_depth() - set pause queue depth
 * @soc_hdl: soc handle
 * @vdev_id: vdev id
 * @pause_q_depth: pause queue depth
 *
 * Return: 0 for success or error code
 */
int ol_txrx_ll_set_tx_pause_q_depth(struct cdp_soc_t *soc_hdl,
				    uint8_t vdev_id, int pause_q_depth);
#endif

void ol_tx_init_pdev(ol_txrx_pdev_handle pdev);

#ifdef CONFIG_HL_SUPPORT
void ol_txrx_vdev_txqs_init(struct ol_txrx_vdev_t *vdev);
void ol_txrx_vdev_tx_queue_free(struct ol_txrx_vdev_t *vdev);
void ol_txrx_peer_txqs_init(struct ol_txrx_pdev_t *pdev,
			    struct ol_txrx_peer_t *peer);
void ol_txrx_peer_tx_queue_free(struct ol_txrx_pdev_t *pdev,
				struct ol_txrx_peer_t *peer);
#else
static inline void
ol_txrx_vdev_txqs_init(struct ol_txrx_vdev_t *vdev) {}

static inline void
ol_txrx_vdev_tx_queue_free(struct ol_txrx_vdev_t *vdev) {}

static inline void
ol_txrx_peer_txqs_init(struct ol_txrx_pdev_t *pdev,
		       struct ol_txrx_peer_t *peer) {}

static inline void
ol_txrx_peer_tx_queue_free(struct ol_txrx_pdev_t *pdev,
			   struct ol_txrx_peer_t *peer) {}
#endif

#if defined(CONFIG_HL_SUPPORT) && defined(DEBUG_HL_LOGGING)
void ol_txrx_pdev_txq_log_init(struct ol_txrx_pdev_t *pdev);
void ol_txrx_pdev_txq_log_destroy(struct ol_txrx_pdev_t *pdev);
void ol_txrx_pdev_grp_stats_init(struct ol_txrx_pdev_t *pdev);
void ol_txrx_pdev_grp_stat_destroy(struct ol_txrx_pdev_t *pdev);
#else
static inline void
ol_txrx_pdev_txq_log_init(struct ol_txrx_pdev_t *pdev) {}

static inline void
ol_txrx_pdev_txq_log_destroy(struct ol_txrx_pdev_t *pdev) {}

static inline void
ol_txrx_pdev_grp_stats_init(struct ol_txrx_pdev_t *pdev) {}

static inline void
ol_txrx_pdev_grp_stat_destroy(struct ol_txrx_pdev_t *pdev) {}
#endif

#if defined(CONFIG_HL_SUPPORT) && defined(FEATURE_WLAN_TDLS)
/**
 * ol_txrx_copy_mac_addr_raw() - copy raw mac addr
 * @soc_hdl: datapath soc handle
 * @vdev_id: the data virtual device id
 * @bss_addr: bss address
 *
 * Return: None
 */
void ol_txrx_copy_mac_addr_raw(struct cdp_soc_t *soc, uint8_t vdev_id,
			       uint8_t *bss_addr);

/**
 * ol_txrx_add_last_real_peer() - add last peer
 * @soc_hdl: datapath soc handle
 * @pdev_id: the data physical device id
 * @vdev_id: virtual device id
 *
 * Return: None
 */
void ol_txrx_add_last_real_peer(struct cdp_soc_t *soc, uint8_t pdev_id,
				uint8_t vdev_id);

/**
 * is_vdev_restore_last_peer() - check for vdev last peer
 * @soc: datapath soc handle
 * vdev_id: vdev id
 * @peer_mac: peer mac address
 *
 * Return: true if last peer is not null
 */
bool is_vdev_restore_last_peer(struct cdp_soc_t *soc, uint8_t vdev_id,
			       uint8_t *peer_mac);

/**
 * ol_txrx_update_last_real_peer() - check for vdev last peer
 * @soc: datapath soc handle
 * @pdev_id: the data physical device id
 * @vdev_id: vdev_id
 * @restore_last_peer: restore last peer flag
 *
 * Return: None
 */
void ol_txrx_update_last_real_peer(struct cdp_soc_t *soc, uint8_t pdev_id,
				   uint8_t vdev_id,
				   bool restore_last_peer);

/**
 * ol_txrx_set_peer_as_tdls_peer() - mark peer as tdls peer
 * @soc: pointer to SOC handle
 * @vdev_id: virtual interface id
 * @peer_mac: peer mac address
 * @value: false/true
 *
 * Return: None
 */
void ol_txrx_set_peer_as_tdls_peer(struct cdp_soc_t *soc, uint8_t vdev_id,
				   uint8_t *peer_mac, bool val);

/**
 * ol_txrx_set_tdls_offchan_enabled() - set tdls offchan enabled
 * @soc: pointer to SOC handle
 * @vdev_id: virtual interface id
 * @peer_mac: peer mac address
 * @value: false/true
 *
 * Return: None
 */
void ol_txrx_set_tdls_offchan_enabled(struct cdp_soc_t *soc, uint8_t vdev_id,
				      uint8_t *peer_mac, bool val);
#endif

#if defined(FEATURE_TSO) && defined(FEATURE_TSO_DEBUG)
void ol_txrx_stats_display_tso(ol_txrx_pdev_handle pdev);
void ol_txrx_tso_stats_init(ol_txrx_pdev_handle pdev);
void ol_txrx_tso_stats_deinit(ol_txrx_pdev_handle pdev);
void ol_txrx_tso_stats_clear(ol_txrx_pdev_handle pdev);
#else
static inline
void ol_txrx_stats_display_tso(ol_txrx_pdev_handle pdev)
{
	ol_txrx_err("TSO is not supported\n");
}

static inline
void ol_txrx_tso_stats_init(ol_txrx_pdev_handle pdev) {}

static inline
void ol_txrx_tso_stats_deinit(ol_txrx_pdev_handle pdev) {}

static inline
void ol_txrx_tso_stats_clear(ol_txrx_pdev_handle pdev) {}
#endif

struct ol_tx_desc_t *
ol_txrx_mgmt_tx_desc_alloc(struct ol_txrx_pdev_t *pdev,
			   struct ol_txrx_vdev_t *vdev,
			   qdf_nbuf_t tx_mgmt_frm,
			   struct ol_txrx_msdu_info_t *tx_msdu_info);

int ol_txrx_mgmt_send_frame(struct ol_txrx_vdev_t *vdev,
			    struct ol_tx_desc_t *tx_desc,
			    qdf_nbuf_t tx_mgmt_frm,
			    struct ol_txrx_msdu_info_t *tx_msdu_info,
			    uint16_t chanfreq);

#ifdef CONFIG_HL_SUPPORT
static inline
uint32_t ol_tx_get_desc_global_pool_size(struct ol_txrx_pdev_t *pdev)
{
	return ol_tx_desc_pool_size_hl(pdev->ctrl_pdev);
}
#else
#ifdef QCA_LL_TX_FLOW_CONTROL_V2
static inline
uint32_t ol_tx_get_desc_global_pool_size(struct ol_txrx_pdev_t *pdev)
{
	return pdev->num_msdu_desc;
}
#else
static inline
uint32_t ol_tx_get_desc_global_pool_size(struct ol_txrx_pdev_t *pdev)
{
	return ol_cfg_target_tx_credit(pdev->ctrl_pdev);
}
#endif
#endif

/**
 * cdp_soc_t_to_ol_txrx_soc_t() - typecast cdp_soc_t to ol_txrx_soc_t
 * @soc: OL soc handle
 *
 * Return: struct ol_txrx_soc_t pointer
 */
static inline
struct ol_txrx_soc_t *cdp_soc_t_to_ol_txrx_soc_t(ol_txrx_soc_handle soc)
{
	return (struct ol_txrx_soc_t *)soc;
}

/**
 * ol_txrx_soc_t_to_cdp_soc_t() - typecast ol_txrx_soc_t to cdp_soc
 * @soc: Opaque soc handle
 *
 * Return: struct cdp_soc_t pointer
 */
static inline
ol_txrx_soc_handle ol_txrx_soc_t_to_cdp_soc_t(struct ol_txrx_soc_t *soc)
{
	return (struct cdp_soc_t *)soc;
}

/**
 * cdp_pdev_to_ol_txrx_pdev_t() - typecast cdp_pdev to ol_txrx_pdev_t
 * @pdev: OL pdev handle
 *
 * Return: struct ol_txrx_pdev_t pointer
 */
static inline
struct ol_txrx_pdev_t *cdp_pdev_to_ol_txrx_pdev_t(struct cdp_pdev *pdev)
{
	return (struct ol_txrx_pdev_t *)pdev;
}

/**
 * ol_txrx_pdev_t_to_cdp_pdev() - typecast ol_txrx_pdev_t to cdp_pdev
 * @pdev: Opaque pdev handle
 *
 * Return: struct cdp_pdev pointer
 */
static inline
struct cdp_pdev *ol_txrx_pdev_t_to_cdp_pdev(struct ol_txrx_pdev_t *pdev)
{
	return (struct cdp_pdev *)pdev;
}

/**
 * cdp_vdev_to_ol_txrx_vdev_t() - typecast cdp_vdev to ol_txrx_vdev_t
 * @vdev: OL vdev handle
 *
 * Return: struct ol_txrx_vdev_t pointer
 */
static inline
struct ol_txrx_vdev_t *cdp_vdev_to_ol_txrx_vdev_t(struct cdp_vdev *vdev)
{
	return (struct ol_txrx_vdev_t *)vdev;
}

/**
 * ol_txrx_vdev_t_to_cdp_vdev() - typecast ol_txrx_vdev_t to cdp_vdev
 * @vdev: Opaque vdev handle
 *
 * Return: struct cdp_vdev pointer
 */
static inline
struct cdp_vdev *ol_txrx_vdev_t_to_cdp_vdev(struct ol_txrx_vdev_t *vdev)
{
	return (struct cdp_vdev *)vdev;
}

#ifdef QCA_LL_TX_FLOW_CONTROL_V2
void ol_tx_set_desc_global_pool_size(uint32_t num_msdu_desc);
uint32_t ol_tx_get_total_free_desc(struct ol_txrx_pdev_t *pdev);
/**
 * ol_txrx_fwd_desc_thresh_check() - check to forward packet to tx path
 * @vdev: which virtual device the frames were addressed to
 *
 * This API is to check whether enough descriptors are available or not
 * to forward packet to tx path. If not enough descriptors left,
 * start dropping tx-path packets.
 * Do not pause netif queues as still a pool of descriptors is reserved
 * for high-priority traffic such as EAPOL/ARP etc.
 * In case of intra-bss forwarding, it could be possible that tx-path can
 * consume all the tx descriptors and pause netif queues. Due to this,
 * there would be some left for stack triggered packets such as ARP packets
 * which could lead to disconnection of device. To avoid this, reserved
 * a pool of descriptors for high-priority packets, i.e., reduce the
 * threshold of drop in the intra-bss forwarding path.
 *
 * Return: true ; forward the packet, i.e., below threshold
 *         false; not enough descriptors, drop the packet
 */
bool ol_txrx_fwd_desc_thresh_check(struct ol_txrx_vdev_t *txrx_vdev);

/**
 * ol_tx_desc_thresh_reached() - is tx desc threshold reached
 * @soc_hdl: Datapath soc handle
 * @vdev_id: id of vdev
 *
 * Return: true if tx desc available reached threshold or false otherwise
 */
static inline bool ol_tx_desc_thresh_reached(struct cdp_soc_t *soc_hdl,
					     uint8_t vdev_id)
{
	struct ol_txrx_vdev_t *vdev;

	vdev = (struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);
	if (!vdev) {
		dp_err("vdev is NULL");
		return false;
	}

	return !(ol_txrx_fwd_desc_thresh_check(vdev));
}

#else
/**
 * ol_tx_get_total_free_desc() - get total free descriptors
 * @pdev: pdev handle
 *
 * Return: total free descriptors
 */
static inline
uint32_t ol_tx_get_total_free_desc(struct ol_txrx_pdev_t *pdev)
{
	return pdev->tx_desc.num_free;
}

static inline
bool ol_txrx_fwd_desc_thresh_check(struct ol_txrx_vdev_t *txrx_vdev)
{
	return true;
}

#endif

#if defined(FEATURE_HL_GROUP_CREDIT_FLOW_CONTROL) && \
	defined(FEATURE_HL_DBS_GROUP_CREDIT_SHARING)
static inline void
ol_txrx_init_txq_group_limit_lend(struct ol_txrx_pdev_t *pdev)
{
	BUILD_BUG_ON(OL_TX_MAX_GROUPS_PER_QUEUE > 1);
	BUILD_BUG_ON(OL_TX_MAX_TXQ_GROUPS > 2);
	pdev->limit_lend = 0;
	pdev->min_reserve = 0;
}
#else
static inline void
ol_txrx_init_txq_group_limit_lend(struct ol_txrx_pdev_t *pdev)
{}
#endif

int ol_txrx_fw_stats_desc_pool_init(struct ol_txrx_pdev_t *pdev,
				    uint8_t pool_size);
void ol_txrx_fw_stats_desc_pool_deinit(struct ol_txrx_pdev_t *pdev);
struct ol_txrx_fw_stats_desc_t
	*ol_txrx_fw_stats_desc_alloc(struct ol_txrx_pdev_t *pdev);
struct ol_txrx_stats_req_internal
	*ol_txrx_fw_stats_desc_get_req(struct ol_txrx_pdev_t *pdev,
				       uint8_t desc_id);

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
/**
 * ol_txrx_register_hl_flow_control() -register hl netdev flow control callback
 * @soc_hdl: soc handle
 * @pdev_id: datapath pdev identifier
 * @flowControl: flow control callback
 *
 * Return: 0 for success or error code
 */
int ol_txrx_register_hl_flow_control(struct cdp_soc_t *soc_hdl,
				     uint8_t pdev_id,
				     tx_pause_callback flowcontrol);

/**
 * ol_txrx_set_vdev_os_queue_status() - Set OS queue status for a vdev
 * @soc_hdl: soc handle
 * @vdev_id: vdev id for the vdev under consideration.
 * @action: action to be done on queue for vdev
 *
 * Return: 0 on success, -EINVAL on failure
 */
int ol_txrx_set_vdev_os_queue_status(struct cdp_soc_t *soc_hdl, u8 vdev_id,
				     enum netif_action_type action);

/**
 * ol_txrx_set_vdev_tx_desc_limit() - Set TX descriptor limits for a vdev
 * @soc_hdl: soc handle
 * @vdev_id: vdev id for the vdev under consideration.
 * @chan_freq: channel frequency on which the vdev has been started.
 *
 * Return: 0 on success, -EINVAL on failure
 */
int ol_txrx_set_vdev_tx_desc_limit(struct cdp_soc_t *soc_hdl, u8 vdev_id,
				   u32 chan_freq);
#endif

/**
 * ol_txrx_get_new_htt_msg_format() - check htt h2t msg feature
 * @pdev - datapath device instance
 *
 * Check if h2t message length includes htc header length
 *
 * return if new htt h2t msg feature enabled
 */
bool ol_txrx_get_new_htt_msg_format(struct ol_txrx_pdev_t *pdev);

/**
 * ol_txrx_set_new_htt_msg_format() - set htt h2t msg feature
 * @val - enable or disable new htt h2t msg feature
 *
 * Set if h2t message length includes htc header length
 *
 * return NONE
 */
void ol_txrx_set_new_htt_msg_format(uint8_t val);

/**
 * ol_txrx_set_peer_unmap_conf_support() - set peer unmap conf feature
 * @val - enable or disable peer unmap conf feature
 *
 * Set if peer unamp conf feature is supported by both FW and in INI
 *
 * return NONE
 */
void ol_txrx_set_peer_unmap_conf_support(bool val);

/**
 * ol_txrx_get_peer_unmap_conf_support() - check peer unmap conf feature
 *
 * Check if peer unmap conf feature is enabled
 *
 * return true is peer unmap conf feature is enabled else false
 */
bool ol_txrx_get_peer_unmap_conf_support(void);

/**
 * ol_txrx_get_tx_compl_tsf64() - check tx compl tsf64 feature
 *
 * Check if tx compl tsf64 feature is enabled
 *
 * return true is tx compl tsf64 feature is enabled else false
 */
bool ol_txrx_get_tx_compl_tsf64(void);

/**
 * ol_txrx_set_tx_compl_tsf64() - set tx compl tsf64 feature
 * @val - enable or disable tx compl tsf64 feature
 *
 * Set if tx compl tsf64 feature is supported FW
 *
 * return NONE
 */
void ol_txrx_set_tx_compl_tsf64(bool val);

#ifdef QCA_SUPPORT_TXRX_DRIVER_TCP_DEL_ACK

/**
 * ol_txrx_vdev_init_tcp_del_ack() - initialize tcp delayed ack structure
 * @vdev: vdev handle
 *
 * Return: none
 */
void ol_txrx_vdev_init_tcp_del_ack(struct ol_txrx_vdev_t *vdev);

/**
 * ol_txrx_vdev_deinit_tcp_del_ack() - deinitialize tcp delayed ack structure
 * @vdev: vdev handle
 *
 * Return: none
 */
void ol_txrx_vdev_deinit_tcp_del_ack(struct ol_txrx_vdev_t *vdev);

/**
 * ol_txrx_vdev_free_tcp_node() - add tcp node in free list
 * @vdev: vdev handle
 * @node: tcp stream node
 *
 * Return: none
 */
void ol_txrx_vdev_free_tcp_node(struct ol_txrx_vdev_t *vdev,
				struct tcp_stream_node *node);

/**
 * ol_txrx_vdev_alloc_tcp_node() - allocate tcp node
 * @vdev: vdev handle
 *
 * Return: tcp stream node
 */
struct tcp_stream_node *
ol_txrx_vdev_alloc_tcp_node(struct ol_txrx_vdev_t *vdev);

/**
 * ol_tx_pdev_reset_driver_del_ack() - reset driver delayed ack enabled flag
 * @ppdev: the data physical device
 *
 * Return: none
 */
void
ol_tx_pdev_reset_driver_del_ack(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);

/**
 * ol_tx_vdev_set_driver_del_ack_enable() - set driver delayed ack enabled flag
 * @soc_hdl: datapath soc handle
 * @vdev_id: vdev id
 * @rx_packets: number of rx packets
 * @time_in_ms: time in ms
 * @high_th: high threshold
 * @low_th: low threshold
 *
 * Return: none
 */
void
ol_tx_vdev_set_driver_del_ack_enable(struct cdp_soc_t *soc_hdl,
				     uint8_t vdev_id,
				     unsigned long rx_packets,
				     uint32_t time_in_ms,
				     uint32_t high_th,
				     uint32_t low_th);

/**
 * ol_tx_hl_send_all_tcp_ack() - send all queued tcp ack packets
 * @vdev: vdev handle
 *
 * Return: none
 */
void ol_tx_hl_send_all_tcp_ack(struct ol_txrx_vdev_t *vdev);

/**
 * tcp_del_ack_tasklet() - tasklet function to send ack packets
 * @data: vdev handle
 *
 * Return: none
 */
void tcp_del_ack_tasklet(void *data);

/**
 * ol_tx_get_stream_id() - get stream_id from packet info
 * @info: packet info
 *
 * Return: stream_id
 */
uint16_t ol_tx_get_stream_id(struct packet_info *info);

/**
 * ol_tx_get_packet_info() - update packet info for passed msdu
 * @msdu: packet
 * @info: packet info
 *
 * Return: none
 */
void ol_tx_get_packet_info(qdf_nbuf_t msdu, struct packet_info *info);

/**
 * ol_tx_hl_find_and_send_tcp_stream() - find and send tcp stream for passed
 *                                       stream info
 * @vdev: vdev handle
 * @info: packet info
 *
 * Return: none
 */
void ol_tx_hl_find_and_send_tcp_stream(struct ol_txrx_vdev_t *vdev,
				       struct packet_info *info);

/**
 * ol_tx_hl_find_and_replace_tcp_ack() - find and replace tcp ack packet for
 *                                       passed packet info
 * @vdev: vdev handle
 * @msdu: packet
 * @info: packet info
 *
 * Return: none
 */
void ol_tx_hl_find_and_replace_tcp_ack(struct ol_txrx_vdev_t *vdev,
				       qdf_nbuf_t msdu,
				       struct packet_info *info);

/**
 * ol_tx_hl_vdev_tcp_del_ack_timer() - delayed ack timer function
 * @timer: timer handle
 *
 * Return: enum
 */
enum qdf_hrtimer_restart_status
ol_tx_hl_vdev_tcp_del_ack_timer(qdf_hrtimer_data_t *timer);

/**
 * ol_tx_hl_del_ack_queue_flush_all() - drop all queued packets
 * @vdev: vdev handle
 *
 * Return: none
 */
void ol_tx_hl_del_ack_queue_flush_all(struct ol_txrx_vdev_t *vdev);

#else

static inline
void ol_txrx_vdev_init_tcp_del_ack(struct ol_txrx_vdev_t *vdev)
{
}

static inline
void ol_txrx_vdev_deinit_tcp_del_ack(struct ol_txrx_vdev_t *vdev)
{
}

static inline
void ol_tx_pdev_reset_driver_del_ack(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
}

static inline
void ol_tx_vdev_set_driver_del_ack_enable(struct cdp_soc_t *soc_hdl,
					  uint8_t vdev_id,
					  unsigned long rx_packets,
					  uint32_t time_in_ms,
					  uint32_t high_th,
					  uint32_t low_th)
{
}

#endif

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
void ol_tx_vdev_set_bundle_require(uint8_t vdev_id, unsigned long tx_bytes,
				   uint32_t time_in_ms, uint32_t high_th,
				   uint32_t low_th);

void ol_tx_pdev_reset_bundle_require(struct cdp_soc_t *soc_hdl, uint8_t pdev_id);

#else

static inline
void ol_tx_vdev_set_bundle_require(uint8_t vdev_id, unsigned long tx_bytes,
				   uint32_t time_in_ms, uint32_t high_th,
				   uint32_t low_th)
{
}

static inline
void ol_tx_pdev_reset_bundle_require(struct cdp_soc_t *soc_hdl, uint8_t pdev_id)
{
}
#endif

#endif /* _OL_TXRX__H_ */
