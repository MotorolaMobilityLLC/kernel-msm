/*
 * Copyright (c) 2017-2019 The Linux Foundation. All rights reserved.
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
 * DOC: Defines off channel tx API & structures
 */

#ifndef _WLAN_P2P_OFF_CHAN_TX_H_
#define _WLAN_P2P_OFF_CHAN_TX_H_

#include <qdf_types.h>
#include <qdf_mc_timer.h>
#include <qdf_list.h>

#define P2P_EID_VENDOR                          0xdd
#define P2P_ACTION_VENDOR_SPECIFIC_CATEGORY     0x7F
#define P2P_PUBLIC_ACTION_FRAME                 0x4
#define P2P_MAC_MGMT_ACTION                     0xD
#define P2P_PUBLIC_ACTION_VENDOR_SPECIFIC       0x9
#define P2P_NOA_ATTR                            0xC

#define P2P_MAX_NOA_ATTR_LEN                    31
#define P2P_IE_HEADER_LEN                       6
#define P2P_ACTION_OFFSET                       24
#define P2P_PUBLIC_ACTION_FRAME_TYPE_OFFSET     30
#define P2P_ACTION_FRAME_TYPE_OFFSET            29
#define PROBE_RSP_IE_OFFSET                     36

#define P2P_TX_PKT_MIN_HEADROOM                (64)

#define P2P_OUI                                 "\x50\x6f\x9a\x09"
#define P2P_OUI_SIZE                            4

#define P2P_ACTION_FRAME_RSP_WAIT               500
#define P2P_ACTION_FRAME_ACK_WAIT               300
#define P2P_ACTION_FRAME_TX_TIMEOUT             2000

#define DST_MAC_ADDR_OFFSET  4
#define SRC_MAC_ADDR_OFFSET  (DST_MAC_ADDR_OFFSET + QDF_MAC_ADDR_SIZE)

#define P2P_NOA_STREAM_ARR_SIZE (P2P_MAX_NOA_ATTR_LEN + (2 * P2P_IE_HEADER_LEN))

#define P2P_GET_TYPE_FRM_FC(__fc__)         (((__fc__) & 0x0F) >> 2)
#define P2P_GET_SUBTYPE_FRM_FC(__fc__)      (((__fc__) & 0xF0) >> 4)

#define WLAN_WAIT_TIME_SET_RND 100

struct p2p_soc_priv_obj;
struct cancel_roc_context;
struct p2p_tx_conf_event;
struct p2p_rx_mgmt_event;

/**
 * enum p2p_frame_type - frame type
 * @P2P_FRAME_MGMT:         mgmt frame
 * @P2P_FRAME_NOT_SUPPORT:  not support frame type
 */
enum p2p_frame_type {
	P2P_FRAME_MGMT = 0,
	P2P_FRAME_NOT_SUPPORT,
};

/**
 * enum p2p_frame_sub_type - frame sub type
 * @P2P_MGMT_PROBE_REQ:       probe request frame
 * @P2P_MGMT_PROBE_RSP:       probe response frame
 * @P2P_MGMT_DISASSOC:        disassociation frame
 * @P2P_MGMT_AUTH:            authentication frame
 * @P2P_MGMT_DEAUTH:          deauthentication frame
 * @P2P_MGMT_ACTION:          action frame
 * @P2P_MGMT_NOT_SUPPORT:     not support sub frame type
 */
enum p2p_frame_sub_type {
	P2P_MGMT_PROBE_REQ = 4,
	P2P_MGMT_PROBE_RSP,
	P2P_MGMT_DISASSOC = 10,
	P2P_MGMT_AUTH,
	P2P_MGMT_DEAUTH,
	P2P_MGMT_ACTION,
	P2P_MGMT_NOT_SUPPORT,
};

/**
 * enum p2p_public_action_type - public action frame type
 * @P2P_PUBLIC_ACTION_NEG_REQ:       go negotiation request frame
 * @P2P_PUBLIC_ACTION_NEG_RSP:       go negotiation response frame
 * @P2P_PUBLIC_ACTION_NEG_CNF:       go negotiation confirm frame
 * @P2P_PUBLIC_ACTION_INVIT_REQ:     p2p invitation request frame
 * @P2P_PUBLIC_ACTION_INVIT_RSP:     p2p invitation response frame
 * @P2P_PUBLIC_ACTION_DEV_DIS_REQ:   device discoverability request
 * @P2P_PUBLIC_ACTION_DEV_DIS_RSP:   device discoverability response
 * @P2P_PUBLIC_ACTION_PROV_DIS_REQ:  provision discovery request
 * @P2P_PUBLIC_ACTION_PROV_DIS_RSP:  provision discovery response
 * @P2P_PUBLIC_ACTION_GAS_INIT_REQ:  gas initial request,
 * @P2P_PUBLIC_ACTION_GAS_INIT_RSP:  gas initial response
 * @P2P_PUBLIC_ACTION_GAS_COMB_REQ:  gas comeback request
 * @P2P_PUBLIC_ACTION_GAS_COMB_RSP:  gas comeback response
 * @P2P_PUBLIC_ACTION_NOT_SUPPORT:   not support p2p public action frame
 */
enum p2p_public_action_type {
	P2P_PUBLIC_ACTION_NEG_REQ = 0,
	P2P_PUBLIC_ACTION_NEG_RSP,
	P2P_PUBLIC_ACTION_NEG_CNF,
	P2P_PUBLIC_ACTION_INVIT_REQ,
	P2P_PUBLIC_ACTION_INVIT_RSP,
	P2P_PUBLIC_ACTION_DEV_DIS_REQ,
	P2P_PUBLIC_ACTION_DEV_DIS_RSP,
	P2P_PUBLIC_ACTION_PROV_DIS_REQ,
	P2P_PUBLIC_ACTION_PROV_DIS_RSP,
	P2P_PUBLIC_ACTION_GAS_INIT_REQ = 10,
	P2P_PUBLIC_ACTION_GAS_INIT_RSP,
	P2P_PUBLIC_ACTION_GAS_COMB_REQ,
	P2P_PUBLIC_ACTION_GAS_COMB_RSP,
	P2P_PUBLIC_ACTION_NOT_SUPPORT,
};

/**
 * enum p2p_action_type - p2p action frame type
 * @P2P_ACTION_PRESENCE_REQ:    presence request frame
 * @P2P_ACTION_PRESENCE_RSP:    presence response frame
 * @P2P_ACTION_NOT_SUPPORT:     not support action frame type
 */
enum p2p_action_type {
	P2P_ACTION_PRESENCE_REQ = 1,
	P2P_ACTION_PRESENCE_RSP = 2,
	P2P_ACTION_NOT_SUPPORT,
};

struct p2p_frame_info {
	enum p2p_frame_type type;
	enum p2p_frame_sub_type sub_type;
	enum p2p_public_action_type public_action_type;
	enum p2p_action_type action_type;
};

/**
 * struct tx_action_context - tx action frame context
 * @node:           Node for next element in the list
 * @p2p_soc_obj:    Pointer to SoC global p2p private object
 * @vdev_id:        Vdev id on which this request has come
 * @scan_id:        Scan id given by scan component for this roc req
 * @roc_cookie:     Cookie for remain on channel request
 * @id:             Identifier of this tx context
 * @chan:           Chan for which this tx has been requested
 * @buf:            tx buffer
 * @buf_len:        Length of tx buffer
 * @off_chan:       Is this off channel tx
 * @no_cck:         Required cck or not
 * @no_ack:         Required ack or not
 * @duration:       Duration for the RoC
 * @tx_timer:       RoC timer
 * @frame_info:     Frame type information
 */
struct tx_action_context {
	qdf_list_node_t node;
	struct p2p_soc_priv_obj *p2p_soc_obj;
	int vdev_id;
	int scan_id;
	uint64_t roc_cookie;
	int32_t id;
	uint8_t chan;
	uint8_t *buf;
	int buf_len;
	bool off_chan;
	bool no_cck;
	bool no_ack;
	bool rand_mac_tx;
	uint32_t duration;
	qdf_mc_timer_t tx_timer;
	struct p2p_frame_info frame_info;
	qdf_nbuf_t nbuf;
};

/**
 * p2p_rand_mac_tx_done() - process random mac mgmt tx done
 * @soc: soc
 * @tx_ctx: tx context
 *
 * This function will remove the random mac addr filter reference.
 *
 * Return: void
 */
void
p2p_rand_mac_tx_done(struct wlan_objmgr_psoc *soc,
		     struct tx_action_context *tx_ctx);

/**
 * p2p_clear_mac_filter() - send clear mac addr filter cmd
 * @soc: soc
 * @vdev_id: vdev id
 * @mac: mac addr
 * @freq: freq
 *
 * This function send clear random mac addr filter command to p2p component
 * msg core
 *
 * Return: QDF_STATUS_SUCCESS - if sent successfully.
 *         otherwise: failed.
 */
QDF_STATUS
p2p_clear_mac_filter(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
		     uint8_t *mac, uint32_t freq);

/**
 * p2p_is_vdev_support_rand_mac() - check vdev type support random mac mgmt
 *    tx or not
 * @vdev: vdev object
 *
 * Return: true: support random mac mgmt tx
 *         false: not support random mac mgmt tx.
 */
bool
p2p_is_vdev_support_rand_mac(struct wlan_objmgr_vdev *vdev);

/**
 * p2p_dump_tx_queue() - dump tx queue
 * @p2p_soc_obj: p2p soc private object
 *
 * This function dumps tx queue and output details about tx context in
 * queue.
 *
 * Return: None
 */
void p2p_dump_tx_queue(struct p2p_soc_priv_obj *p2p_soc_obj);

/**
 * p2p_ready_to_tx_frame() - dump tx queue
 * @p2p_soc_obj: p2p soc private object
 * @cookie: cookie is pointer to roc
 *
 * This function find out the tx context in wait for roc queue and tx
 * this frame.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_ready_to_tx_frame(struct p2p_soc_priv_obj *p2p_soc_obj,
	uint64_t cookie);

/**
 * p2p_cleanup_tx_sync() - Cleanup tx queue
 * @p2p_soc_obj: p2p psoc private object
 * @vdev:        vdev object
 *
 * This function cleanup tx context in queue until cancellation done.
 * To avoid deadlock, don't call from scheduler thread.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_cleanup_tx_sync(
	struct p2p_soc_priv_obj *p2p_soc_obj,
	struct wlan_objmgr_vdev *vdev);

/**
 * p2p_process_cleanup_tx_queue() - process the message to cleanup tx
 * @param: pointer to cleanup parameters
 *
 * This function cleanup wait for roc queue and wait for ack queue.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_process_cleanup_tx_queue(
	struct p2p_cleanup_param *param);

/**
 * p2p_process_mgmt_tx() - Process mgmt frame tx request
 * @tx_ctx: tx context
 *
 * This function handles mgmt frame tx request. It will call API from
 * mgmt txrx component.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_process_mgmt_tx(struct tx_action_context *tx_ctx);

/**
 * p2p_process_mgmt_tx_cancel() - Process cancel mgmt frame tx request
 * @cancel_tx: cancel tx context
 *
 * This function cancel mgmt frame tx request by cookie.
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_process_mgmt_tx_cancel(
	struct cancel_roc_context *cancel_tx);

/**
 * p2p_process_mgmt_tx_ack_cnf() - Process tx ack event
 * @tx_cnf_event: tx confirmation event information
 *
 * This function mgmt frame tx confirmation. It will deliver this
 * event to up layer
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_process_mgmt_tx_ack_cnf(
	struct p2p_tx_conf_event *tx_cnf_event);

/**
 * p2p_process_rx_mgmt() - Process rx mgmt frame event
 * @rx_mgmt_event: rx mgmt frame event information
 *
 * This function mgmt frame rx mgmt frame event. It will deliver this
 * event to up layer
 *
 * Return: QDF_STATUS_SUCCESS - in case of success
 */
QDF_STATUS p2p_process_rx_mgmt(
	struct p2p_rx_mgmt_event *rx_mgmt_event);

/**
 * p2p_find_tx_ctx_by_nbuf() - find tx context by nbuf
 * @p2p_soc_obj:        p2p soc object
 * @nbuf:               pointer to nbuf
 *
 * This function finds out tx context by nbuf.
 *
 * Return: pointer to tx context
 */
struct tx_action_context *p2p_find_tx_ctx_by_nbuf(
	struct p2p_soc_priv_obj *p2p_soc_obj, void *nbuf);

#define P2P_80211_FRM_SA_OFFSET 10

/**
 * p2p_del_random_mac() - del mac fitler from given vdev rand mac list
 * @soc: soc object
 * @vdev_id: vdev id
 * @rnd_cookie: random mac mgmt tx cookie
 *
 * This function will del the mac addr filter from vdev random mac addr list.
 * If there is no reference to mac addr, it will set a clear timer to flush it
 * in target finally.
 *
 * Return: QDF_STATUS_SUCCESS - del successfully.
 *             other : failed to del the mac address entry.
 */
QDF_STATUS
p2p_del_random_mac(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
		   uint64_t rnd_cookie);

/**
 * p2p_random_mac_handle_tx_done() - del mac filter from given vdev rand mac
 * list when mgmt tx done
 * @soc: soc object
 * @vdev_id: vdev id
 * @rnd_cookie: random mac mgmt tx cookie
 * @duration: timeout value to flush the addr in target.
 *
 * This function will del the mac addr filter from vdev random mac addr list
 * and also remove the filter from firmware if duration is zero else start
 * the timer for that duration.
 *
 * Return: QDF_STATUS_SUCCESS - del successfully.
 *		other : failed to del the mac address entry.
 */
QDF_STATUS
p2p_random_mac_handle_tx_done(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
			      uint64_t rnd_cookie, uint32_t duration);

/**
 * p2p_check_random_mac() - check random mac addr or not
 * @soc: soc context
 * @vdev_id: vdev id
 * @random_mac_addr: mac addr to be checked
 *
 * This function check the input addr is random mac addr or not for vdev.
 *
 * Return: true if addr is random mac address else false.
 */
bool p2p_check_random_mac(struct wlan_objmgr_psoc *soc, uint32_t vdev_id,
			  uint8_t *random_mac_addr);

/**
 * p2p_process_set_rand_mac() - process the set random mac command
 * @set_filter_req: request data
 *
 * This function will process the set mac addr filter command.
 *
 * Return: QDF_STATUS_SUCCESS: if process successfully
 *             other: failed.
 */
QDF_STATUS p2p_process_set_rand_mac(
		struct p2p_set_mac_filter_req *set_filter_req);

/**
 * p2p_process_set_rand_mac_rsp() - process the set random mac response
 * @resp: response date
 *
 * This function will process the set mac addr filter event.
 *
 * Return: QDF_STATUS_SUCCESS: if process successfully
 *             other: failed.
 */
QDF_STATUS p2p_process_set_rand_mac_rsp(struct p2p_mac_filter_rsp *resp);

/**
 * p2p_del_all_rand_mac_vdev() - del all random mac filter in vdev
 * @vdev: vdev object
 *
 * This function will del all random mac filter in vdev
 *
 * Return: void
 */
void p2p_del_all_rand_mac_vdev(struct wlan_objmgr_vdev *vdev);

/**
 * p2p_del_all_rand_mac_soc() - del all random mac filter in soc
 * @soc: soc object
 *
 * This function will del all random mac filter in all vdev of soc
 *
 * Return: void
 */
void p2p_del_all_rand_mac_soc(struct wlan_objmgr_psoc *soc);

/**
 * p2p_rand_mac_tx() - handle random mac mgmt tx
 * @tx_action: tx action context
 *
 * This function will check whether need to set random mac tx filter for a
 * given mgmt tx request and do the mac addr filter process as needed.
 *
 * Return: void
 */
void p2p_rand_mac_tx(struct  tx_action_context *tx_action);

/**
 * p2p_init_random_mac_vdev() - Init random mac data for vdev
 * @p2p_vdev_obj: p2p vdev private object
 *
 * This function will init the per vdev random mac data structure.
 *
 * Return: void
 */
void p2p_init_random_mac_vdev(struct p2p_vdev_priv_obj *p2p_vdev_obj);

/**
 * p2p_deinit_random_mac_vdev() - Init random mac data for vdev
 * @p2p_vdev_obj: p2p vdev private object
 *
 * This function will deinit the per vdev random mac data structure.
 *
 * Return: void
 */
void p2p_deinit_random_mac_vdev(struct p2p_vdev_priv_obj *p2p_vdev_obj);

/**
 * p2p_get_p2pie_ptr() - get the pointer to p2p ie
 * @ie:      source ie
 * @ie_len:  source ie length
 *
 * This function finds out p2p ie by p2p oui and return the pointer.
 *
 * Return: pointer to p2p ie
 */
const uint8_t *p2p_get_p2pie_ptr(const uint8_t *ie, uint16_t ie_len);

#endif /* _WLAN_P2P_OFF_CHAN_TX_H_ */
