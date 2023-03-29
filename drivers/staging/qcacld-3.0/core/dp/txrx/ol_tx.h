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

/**
 * @file ol_tx.h
 * @brief Internal definitions for the high-level tx module.
 */
#ifndef _OL_TX__H_
#define _OL_TX__H_

#include <qdf_nbuf.h>           /* qdf_nbuf_t */
#include <qdf_lock.h>
#include <cdp_txrx_cmn.h>       /* ol_txrx_vdev_t, etc. */
#include <cdp_txrx_misc.h>      /* ol_tx_spec */
#include <cdp_txrx_handle.h>
#include <ol_txrx_types.h>      /* ol_tx_desc_t, ol_txrx_msdu_info_t */
#include <ol_txrx.h>
#include <hif.h>

#ifdef IPA_OFFLOAD
/**
 * ol_tx_send_ipa_data_frame() - send IPA data frame
 * @soc_hdl: datapath soc handle
 * @vdev: virtual interface id
 * @skb: skb
 *
 * Return: skb/ NULL is for success
 */
qdf_nbuf_t ol_tx_send_ipa_data_frame(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
				     qdf_nbuf_t skb);
#endif

#ifdef CONFIG_LL_DP_SUPPORT
struct ol_tx_desc_t *
ol_tx_prepare_ll(ol_txrx_vdev_handle vdev,
		 qdf_nbuf_t msdu,
		 struct ol_txrx_msdu_info_t *msdu_info);
#endif

qdf_nbuf_t ol_tx_ll_wrapper(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list);
#ifdef WLAN_FEATURE_FASTPATH
qdf_nbuf_t ol_tx_ll_fast(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list);

void ol_tx_setup_fastpath_ce_handles(struct hif_opaque_softc *osc,
				     struct ol_txrx_pdev_t *pdev);
#else
static inline
void ol_tx_setup_fastpath_ce_handles(struct hif_opaque_softc *osc,
				     struct ol_txrx_pdev_t *pdev)
{ }

qdf_nbuf_t ol_tx_ll(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list);
#endif

qdf_nbuf_t ol_tx_ll_queue(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list);

#ifdef CONFIG_HL_SUPPORT
#define OL_TX_SEND ol_tx_hl
#else
#define OL_TX_SEND OL_TX_LL
#endif

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
#define OL_TX_LL ol_tx_ll_queue
#else
#define OL_TX_LL ol_tx_ll_wrapper
#endif

#ifdef WLAN_SUPPORT_TXRX_HL_BUNDLE
void ol_tx_hl_vdev_bundle_timer(void *context);

void ol_tx_hl_queue_flush_all(struct ol_txrx_vdev_t *vdev);
qdf_nbuf_t
ol_tx_hl_pdev_queue_send_all(struct ol_txrx_pdev_t *pdev);
#else
static inline
void ol_tx_hl_vdev_bundle_timer(void *context)
{
}

static inline
void ol_tx_hl_queue_flush_all(struct ol_txrx_vdev_t *vdev)
{
}

static inline
qdf_nbuf_t
ol_tx_hl_pdev_queue_send_all(struct ol_txrx_pdev_t *pdev)
{
	return NULL;
}
#endif

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
void ol_tx_vdev_ll_pause_queue_send(void *context);
void ol_tx_pdev_ll_pause_queue_send_all(struct ol_txrx_pdev_t *pdev);
#else
static inline void ol_tx_vdev_ll_pause_queue_send(void *context)
{
}
static inline
void ol_tx_pdev_ll_pause_queue_send_all(struct ol_txrx_pdev_t *pdev)
{
}
#endif

static inline
int ol_txrx_tx_is_raw(enum ol_tx_spec tx_spec)
{
	return	tx_spec &
		(OL_TX_SPEC_RAW | OL_TX_SPEC_NO_AGGR | OL_TX_SPEC_NO_ENCRYPT);
}

static inline
uint8_t ol_txrx_tx_raw_subtype(enum ol_tx_spec tx_spec)
{
	uint8_t sub_type = 0x1; /* 802.11 MAC header present */

	if (tx_spec & OL_TX_SPEC_NO_AGGR)
		sub_type |= 0x1 << HTT_TX_MSDU_DESC_RAW_SUBTYPE_NO_AGGR_S;
	if (tx_spec & OL_TX_SPEC_NO_ENCRYPT)
		sub_type |= 0x1 << HTT_TX_MSDU_DESC_RAW_SUBTYPE_NO_ENCRYPT_S;
	if (tx_spec & OL_TX_SPEC_NWIFI_NO_ENCRYPT)
		sub_type |= 0x1 << HTT_TX_MSDU_DESC_RAW_SUBTYPE_NO_ENCRYPT_S;
	return sub_type;
}

/**
 * ol_tx_hl() - transmit tx frames for a HL system.
 * @vdev: the virtual device transmit the data
 * @msdu_list: the tx frames to send
 *
 * Return: NULL if all MSDUs are accepted
 */
qdf_nbuf_t
ol_tx_hl(ol_txrx_vdev_handle vdev, qdf_nbuf_t msdu_list);

/**
 * ol_tx_non_std() - Allow the control-path SW to send data frames
 * @soc_hdl: Datapath soc handle
 * @vdev_id: id of vdev
 * @tx_spec: what non-standard handling to apply to the tx data frames
 * @msdu_list: NULL-terminated list of tx MSDUs
 *
 * Generally, all tx data frames come from the OS shim into the txrx layer.
 * However, there are rare cases such as TDLS messaging where the UMAC
 * control-path SW creates tx data frames.
 *  This UMAC SW can call this function to provide the tx data frames to
 *  the txrx layer.
 *  The UMAC SW can request a callback for these data frames after their
 *  transmission completes, by using the ol_txrx_data_tx_cb_set function
 *  to register a tx completion callback, and by specifying
 *  ol_tx_spec_no_free as the tx_spec arg when giving the frames to
 *  ol_tx_non_std.
 *  The MSDUs need to have the appropriate L2 header type (802.3 vs. 802.11),
 *  as specified by ol_cfg_frame_type().
 *
 *  Return: null - success, skb - failure
 */
#ifdef CONFIG_HL_SUPPORT
qdf_nbuf_t ol_tx_non_std_hl(struct ol_txrx_vdev_t *vdev,
			    enum ol_tx_spec tx_spec,
			    qdf_nbuf_t msdu_list);

static inline qdf_nbuf_t
ol_tx_non_std(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
	      enum ol_tx_spec tx_spec, qdf_nbuf_t msdu_list)
{
	struct ol_txrx_vdev_t *vdev;

	vdev = (struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);

	if (!vdev)
		return msdu_list;
	else
		return ol_tx_non_std_hl(vdev, tx_spec, msdu_list);
}
#else
qdf_nbuf_t ol_tx_non_std_ll(struct ol_txrx_vdev_t *vdev,
			    enum ol_tx_spec tx_spec, qdf_nbuf_t msdu_list);

static inline qdf_nbuf_t
ol_tx_non_std(struct cdp_soc_t *soc_hdl, uint8_t vdev_id,
	      enum ol_tx_spec tx_spec, qdf_nbuf_t msdu_list)
{
	struct ol_txrx_vdev_t *vdev;

	vdev = (struct ol_txrx_vdev_t *)ol_txrx_get_vdev_from_vdev_id(vdev_id);

	if (!vdev)
		return msdu_list;
	else
		return ol_tx_non_std_ll(vdev, tx_spec, msdu_list);
}
#endif

void ol_txrx_mgmt_tx_complete(void *ctxt, qdf_nbuf_t netbuf, int err);

/**
 * ol_txrx_mgmt_tx_cb_set() - Store a callback for delivery
 *	notifications for management frames.
 * @soc: Datapath soc handle
 * @pdev_id: Physical device instance id
 * @type: the type of mgmt frame the callback is used for
 * @download_cb: the callback for notification of delivery to the target
 * @ota_ack_cb: the callback for notification of delivery to the peer
 * @ctxt: context to use with the callback
 *
 * When the txrx SW receives notifications from the target that a tx frame
 * has been delivered to its recipient, it will check if the tx frame
 * is a management frame.  If so, the txrx SW will check the management
 * frame type specified when the frame was submitted for transmission.
 * If there is a callback function registered for the type of management
 * frame in question, the txrx code will invoke the callback to inform
 * the management + control SW that the mgmt frame was delivered.
 * This function is used by the control SW to store a callback pointer
 * for a given type of management frame.
 */
QDF_STATUS
ol_txrx_mgmt_tx_cb_set(struct cdp_soc_t *soc, uint8_t pdev_id, uint8_t type,
		       ol_txrx_mgmt_tx_cb download_cb,
		       ol_txrx_mgmt_tx_cb ota_ack_cb, void *ctxt);

/**
 * ol_txrx_mgmt_send_ext() - Transmit a management frame
 * @soc: Datapath soc handle
 * @vdev_id: virtual interface id
 * @tx_mgmt_frm: management frame to transmit
 * @type: the type of management frame (determines what callback to use)
 * @use_6mbps: specify whether management frame to transmit should
 *	use 6 Mbps rather than 1 Mbps min rate(for 5GHz band or P2P)
 * @chanfreq: channel to transmit the frame on
 *
 * Send the specified management frame from the specified virtual device.
 * The type is used for determining whether to invoke a callback to inform
 * the sender that the tx mgmt frame was delivered, and if so, which
 * callback to use.
 *
 * Return: 0 - the frame is accepted for transmission
 *         1 - the frame was not accepted
 */
int
ol_txrx_mgmt_send_ext(struct cdp_soc_t *soc, uint8_t vdev_id,
		      qdf_nbuf_t tx_mgmt_frm,
		      uint8_t type, uint8_t use_6mbps, uint16_t chanfreq);

qdf_nbuf_t
ol_tx_reinject(struct ol_txrx_vdev_t *vdev, qdf_nbuf_t msdu, uint16_t peer_id);

#if defined(FEATURE_TSO)
void ol_tso_seg_list_init(struct ol_txrx_pdev_t *pdev, uint32_t num_seg);
void ol_tso_seg_list_deinit(struct ol_txrx_pdev_t *pdev);
void ol_tso_num_seg_list_init(struct ol_txrx_pdev_t *pdev, uint32_t num_seg);
void ol_tso_num_seg_list_deinit(struct ol_txrx_pdev_t *pdev);
uint32_t ol_tx_tso_get_stats_idx(struct ol_txrx_pdev_t *pdev);
uint8_t ol_tx_prepare_tso(ol_txrx_vdev_handle vdev,
			  qdf_nbuf_t msdu,
			  struct ol_txrx_msdu_info_t *msdu_info);
void ol_tx_tso_update_stats(struct ol_txrx_pdev_t *pdev,
			    struct qdf_tso_info_t  *tso_info, qdf_nbuf_t msdu,
			    uint32_t tso_msdu_idx);
#else
static inline uint32_t ol_tx_tso_get_stats_idx(struct ol_txrx_pdev_t *pdev)
{
	return 0;
}

static inline void ol_tso_seg_list_init(struct ol_txrx_pdev_t *pdev,
	uint32_t num_seg)
{
}

static inline void ol_tso_seg_list_deinit(struct ol_txrx_pdev_t *pdev)
{
}

static inline void ol_tso_num_seg_list_init(struct ol_txrx_pdev_t *pdev,
	uint32_t num_seg)
{
}

static inline void ol_tso_num_seg_list_deinit(struct ol_txrx_pdev_t *pdev)
{
}

static inline uint8_t ol_tx_prepare_tso(ol_txrx_vdev_handle vdev,
					qdf_nbuf_t msdu,
					struct ol_txrx_msdu_info_t *msdu_info)
{
	return 0;
}

static inline void ol_tx_tso_update_stats(struct ol_txrx_pdev_t *pdev,
					  struct qdf_tso_info_t  *tso_info,
					  qdf_nbuf_t msdu,
					  uint32_t tso_msdu_idx)
{
}
#endif

#ifdef QCA_HL_NETDEV_FLOW_CONTROL
bool ol_tx_desc_is_high_prio(qdf_nbuf_t msdu);
#endif

#if defined(HELIUMPLUS)
void ol_txrx_dump_frag_desc(char *msg, struct ol_tx_desc_t *tx_desc);
#else
static inline
void ol_txrx_dump_frag_desc(char *msg, struct ol_tx_desc_t *tx_desc)
{
}
#endif

#endif /* _OL_TX__H_ */
