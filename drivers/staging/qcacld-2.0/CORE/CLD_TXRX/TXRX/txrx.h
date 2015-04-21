/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
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

#ifndef TXRX_H
#define TXRX_H

#include "vos_api.h"
#include "adf_nbuf.h"
#include "csrApi.h"
#include "sapApi.h"
#include "adf_nbuf.h"
#include "ol_txrx_osif_api.h"
#include "wlan_qct_tl.h"

/* wait on peer deletion timeout value in milliseconds */
#define PEER_DELETION_TIMEOUT 500

enum txrx_wmm_ac {
	TXRX_WMM_AC_VO,
	TXRX_WMM_AC_VI,
	TXRX_WMM_AC_BK,
	TXRX_WMM_AC_BE,

	TXRX_NUM_WMM_AC
};

struct txrx_rx_metainfo {
	u8 up;
	u16 dest_staid;
};

enum bt_frame_type {
	/* BT-AMP packet of type data */
	TXRX_BT_AMP_TYPE_DATA = 0x0001,

	/* BT-AMP packet of type activity report */
	TXRX_BT_AMP_TYPE_AR = 0x0002,

	/* BT-AMP packet of type security frame */
	TXRX_BT_AMP_TYPE_SEC = 0x0003,

	/* BT-AMP packet of type Link Supervision request frame */
	TXRX_BT_AMP_TYPE_LS_REQ = 0x0004,

	/* BT-AMP packet of type Link Supervision reply frame */
	TXRX_BT_AMP_TYPE_LS_REP = 0x0005,

	/* Invalid Frame */
	TXRX_BAP_INVALID_FRAME

};

enum wlan_ts_direction {
	/* uplink */
	WLAN_TX_DIR = 0,

	/* downlink */
	WLAN_RX_DIR = 1,

	/*bidirectional*/
	WLAN_BI_DIR = 2,
};

enum wlan_sta_state {
	/* Transition in this state made upon creation*/
	WLAN_STA_INIT = 0,

	/* Transition happens after Assoc success if second level authentication
	   is needed*/
	WLAN_STA_CONNECTED,

	/* Transition happens when second level auth is successful and keys are
	   properly installed */
	WLAN_STA_AUTHENTICATED,

	/* Transition happens when connectivity is lost*/
	WLAN_STA_DISCONNECTED,

	WLAN_STA_MAX_STATE
};

struct wlan_txrx_stats {
	/* Define various txrx stats here*/
};

struct ol_txrx_vdev_t;

VOS_STATUS wlan_register_mgmt_client(void *pdev_txrx,
				     VOS_STATUS (*rx_mgmt)(void *g_vosctx,
							   void *buf));

typedef void (*ol_txrx_vdev_delete_cb)(void *context);

/**
 * @typedef ol_txrx_tx_fp
 * @brief top-level transmit function
 */
typedef adf_nbuf_t
(*ol_txrx_tx_fp)(struct ol_txrx_vdev_t *vdev, adf_nbuf_t msdu_list);

typedef void
(*ol_txrx_mgmt_tx_cb)(void *ctxt, adf_nbuf_t tx_mgmt_frm, int had_error);

/* If RSSI realm is changed, send notification to Clients, SME, HDD */
typedef VOS_STATUS (*wlan_txrx_rssi_cross_thresh) (void *adapter, u8 rssi,
					    void *usr_ctx, v_S7_t avg_rssi);

struct wlan_txrx_ind_req {
	u16 msgType;    // message type is same as the request type
	u16 msgLen;  // length of the entire request
	u8  sessionId; //sme Session Id
	u8  rssiNotification;
	u8  avgRssi;
	void *tlCallback;
	void *pAdapter;
	void *pUserCtxt;
};

struct wlan_txrx_config_param {
	u8 ucAcWeights[TXRX_NUM_WMM_AC];
	u32 uDelayedTriggerFrmInt;
	u8 uMinFramesProcThres;
};

/* Rx callback registered with txrx */
typedef int (*wlan_txrx_cb_type)( void *g_vosctx, adf_nbuf_t buf, u8 sta_id,
				 struct txrx_rx_metainfo *rx_meta_info);

static inline int wlan_txrx_get_rssi(void *g_vosctx, u8 sta_id, v_S7_t *rssi)
{
	return 0;
}

static inline int wlan_txrx_enable_uapsd_ac(void *g_vosctx, u8 sta_id,
				     enum txrx_wmm_ac ac, u8 tid, u8 up,
				     u32 srv_int, u32 suspend_int,
				     enum wlan_ts_direction ts_dir)
{
	return 0;
}

static inline int wlan_txrx_disable_uapsd_ac(void *g_vosctx, u8 sta_id,
					     enum txrx_wmm_ac ac)
{
	return 0;
}

static inline int wlan_change_sta_state(void *g_vosctx, u8 sta_id,
					enum wlan_sta_state state)
{
	return 0;
}

static inline int wlan_deregister_mgmt_client(void *g_vosctx)
{
	return 0;
}

static inline void wlan_assoc_failed(u8 staid)
{
}

static inline int wlan_get_ap_stats(void *g_vosctx, tSap_SoftapStats *buf,
				    bool reset)
{
	return 0;
}

static inline int wlan_get_txrx_stats(void *g_vosctx, struct wlan_txrx_stats *stats,
				      u8 sta_id)
{
	return 0;
}

static inline int wlan_txrx_update_rssi_bmps(void *g_vosctx, u8 sta_id,
					     v_S7_t rssi)
{
	return 0;
}

static inline int wlan_txrx_deregister_rssi_indcb(void *g_vosctx,
						  v_S7_t rssi_val,
						  u8 trigger_event,
						  wlan_txrx_rssi_cross_thresh cb,
						  int mod_id)
{
	return 0;
}

static inline int wlan_txrx_register_rssi_indcb(void *g_vosctx,
						  v_S7_t rssi_val,
						  u8 trigger_event,
						  wlan_txrx_rssi_cross_thresh cb,
						  int mod_id, void *usr_ctx)
{
	return 0;
}

/* FIXME: The following stubs will be removed eventually */
static inline int wlan_txrx_mc_process_msg(void *g_vosctx, vos_msg_t *msg)
{
	return 0;
}

static inline int wlan_txrx_tx_process_msg(void *g_vosctx, vos_msg_t *msg)
{
	return 0;
}

static inline void wlan_txrx_mc_free_msg(void *g_vosctx, vos_msg_t *msg)
{
}
static inline void wlan_txrx_tx_free_msg(void *g_vosctx, vos_msg_t *msg)
{
}
#endif
