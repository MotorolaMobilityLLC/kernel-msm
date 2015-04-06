/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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

#ifndef TXRX_TL_SHIM_H
#define TXRX_TL_SHIM_H

#include <ol_txrx_osif_api.h>
#include <adf_os_lock.h>
#include <adf_os_atomic.h>

#ifdef FEATURE_WLAN_ESE
typedef struct deferred_iapp_work {
    pVosContextType	pVosGCtx;
    adf_nbuf_t nbuf;
    struct ol_txrx_vdev_t *vdev;
    bool inUse;
    struct work_struct deferred_work;
} deferred_iapp_work;
#endif

struct tlshim_buf {
	struct list_head list;
	adf_nbuf_t buf;
};

#define TLSHIM_FLUSH_CACHE_IN_PROGRESS 0
struct tlshim_sta_info {
	bool registered;
	bool suspend_flush;
	WLANTL_STARxCBType data_rx;
	/* To protect stainfo data like registered and data_rx */
	adf_os_spinlock_t stainfo_lock;
	struct list_head cached_bufq;
	unsigned long flags;
	v_S7_t first_rssi;
	v_U8_t vdev_id;
};

#ifdef QCA_LL_TX_FLOW_CT
struct tlshim_session_flow_Control {
	WLANTL_TxFlowControlCBType flowControl;
	v_U8_t                     sessionId;
	void                      *adpaterCtxt;
	void                      *vdev;
	adf_os_spinlock_t          fc_lock;
};
#endif /* QCA_LL_TX_FLOW_CT */

#ifdef IPA_UC_OFFLOAD
typedef void(*ipa_uc_fw_op_cb)(v_U8_t *op_msg, void *usr_ctxt);
#endif /* IPA_UC_OFFLOAD */

struct txrx_tl_shim_ctx {
	void *cfg_ctx;
	ol_txrx_tx_fp tx;
	WLANTL_MgmtFrmRxCBType mgmt_rx;
	struct tlshim_sta_info sta_info[WLAN_MAX_STA_COUNT];
	adf_os_spinlock_t bufq_lock;
	adf_os_spinlock_t mgmt_lock;
	struct work_struct cache_flush_work;

#ifdef FEATURE_WLAN_ESE
    /*
     * work structures to defer IAPP processing to
     * non interrupt context
     */
struct deferred_iapp_work iapp_work;
#endif
	v_BOOL_t ip_checksum_offload;
	u_int8_t   *last_beacon_data;
	u_int32_t   last_beacon_len;
	u_int32_t delay_interval;
	v_BOOL_t enable_rxthread;
	adf_os_atomic_t *vdev_active;
#ifdef QCA_LL_TX_FLOW_CT
	struct tlshim_session_flow_Control *session_flow_control;
#endif /* QCA_LL_TX_FLOW_CT */

#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
	vos_event_t  *peer_authorized_events;
#endif
#ifdef IPA_UC_OFFLOAD
	ipa_uc_fw_op_cb fw_op_cb;
	void *usr_ctxt;
#endif /* IPA_UC_OFFLOAD */
};

/*
 * APIs used by CLD specific components, as of now these are used only
 * in WMA.
 */
void WLANTL_RegisterVdev(void *vos_ctx, void *vdev);
void WLANTL_UnRegisterVdev(void *vos_ctx, u_int8_t vdev_id);
VOS_STATUS tl_shim_get_vdevid(struct ol_txrx_peer_t *peer, u_int8_t *vdev_id);

/*
 * tlshim_mgmt_roam_event_ind() is called from WMA layer when
 * BETTER_AP_FOUND event is received from roam engine.
 */
int tlshim_mgmt_roam_event_ind(void *context, u_int32_t vdev_id);
void *tl_shim_get_vdev_by_addr(void *vos_context, uint8_t *mac_addr);
void *tl_shim_get_vdev_by_sta_id(void *vos_context, uint8_t sta_id);

#ifdef QCA_SUPPORT_TXRX_VDEV_PAUSE_LL
void tl_shim_set_peer_authorized_event(void *vos_ctx, v_U8_t session_id);
#else
static inline void tl_shim_set_peer_authorized_event(void *vos_ctx, v_U8_t session_id)
{
}
#endif
#endif
