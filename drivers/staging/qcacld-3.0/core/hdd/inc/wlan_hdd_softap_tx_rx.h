/*
 * Copyright (c) 2014-2021 The Linux Foundation. All rights reserved.
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

#if !defined(WLAN_HDD_SOFTAP_TX_RX_H)
#define WLAN_HDD_SOFTAP_TX_RX_H

/**
 * DOC: wlan_hdd_softap_tx_rx.h
 *
 * Linux HDD SOFTAP Tx/Rx APIs
 */

#include <wlan_hdd_hostapd.h>
#include <cdp_txrx_peer_ops.h>

/**
 * hdd_softap_hard_start_xmit() - Transmit a frame
 * @skb: pointer to OS packet
 * @dev: pointer to net_device structure
 *
 * Function registered as a net_device .ndo_start_xmit() method for
 * master mode interfaces (SoftAP/P2P GO), called by the OS if any
 * packet needs to be transmitted.
 *
 * Return: Status of the transmission
 */
netdev_tx_t hdd_softap_hard_start_xmit(struct sk_buff *skb,
				       struct net_device *dev);

/**
 * hdd_softap_ipa_start_xmit() - Transmit a frame, request from IPA
 * @nbuf: pointer to buffer/packet
 * @dev: pointer to net_device structure
 *
 * Function registered as a xmit callback in SAP mode,
 * called by IPA if any packet needs to be transmitted.
 *
 * Return: Status of the transmission
 */
QDF_STATUS hdd_softap_ipa_start_xmit(qdf_nbuf_t nbuf, qdf_netdev_t dev);

/**
 * hdd_softap_tx_timeout() - TX timeout handler
 * @dev: pointer to network device
 * @txqueue: tx queue
 *
 * Function registered as a net_device .ndo_tx_timeout() method for
 * master mode interfaces (SoftAP/P2P GO), called by the OS if the
 * driver takes too long to transmit a frame.
 *
 * Return: None
 */
#if (LINUX_VERSION_CODE >= KERNEL_VERSION(5, 6, 0))
void hdd_softap_tx_timeout(struct net_device *dev, unsigned int txqueue);
#else
void hdd_softap_tx_timeout(struct net_device *dev);
#endif
/**
 * hdd_softap_init_tx_rx() - Initialize Tx/Rx module
 * @adapter: pointer to adapter context
 *
 * Return: None
 */
void hdd_softap_init_tx_rx(struct hdd_adapter *adapter);

/**
 * hdd_softap_deinit_tx_rx() - Deinitialize Tx/Rx module
 * @adapter: pointer to adapter context
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS hdd_softap_deinit_tx_rx(struct hdd_adapter *adapter);

/**
 * hdd_softap_init_tx_rx_sta() - Initialize Tx/Rx for a softap station
 * @adapter: pointer to adapter context
 * @sta_mac: pointer to the MAC address of the station
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS hdd_softap_init_tx_rx_sta(struct hdd_adapter *adapter,
				     struct qdf_mac_addr *sta_mac);

/**
 * hdd_softap_rx_packet_cbk() - Receive packet handler
 * @adapter_context: pointer to HDD adapter
 * @rx_buf: pointer to rx qdf_nbuf chain
 *
 * Receive callback registered with the Data Path.  The Data Path will
 * call this to notify the HDD when one or more packets were received
 * for a registered STA.
 *
 * Return: QDF_STATUS_E_FAILURE if any errors encountered,
 *	   QDF_STATUS_SUCCESS otherwise
 */
QDF_STATUS hdd_softap_rx_packet_cbk(void *adapter_context, qdf_nbuf_t rx_buf);

/**
 * hdd_softap_deregister_sta() - Deregister a STA with the Data Path
 * @adapter: pointer to adapter context
 * @sta_info: double pointer to HDD station info structure
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS hdd_softap_deregister_sta(struct hdd_adapter *adapter,
				     struct hdd_station_info **sta_info);

/**
 * hdd_softap_register_sta() - Register a SoftAP STA
 * @adapter: pointer to adapter context
 * @auth_required: is additional authentication required?
 * @privacy_required: should 802.11 privacy bit be set?
 * @sta_mac: station MAC address
 * @event: STA assoc complete event (Can be NULL)
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS
hdd_softap_register_sta(struct hdd_adapter *adapter,
			bool auth_required,
			bool privacy_required,
			struct qdf_mac_addr *sta_mac,
			tSap_StationAssocReassocCompleteEvent *event);

/**
 * hdd_softap_register_bc_sta() - Register the SoftAP broadcast STA
 * @adapter: pointer to adapter context
 * @privacy_required: should 802.11 privacy bit be set?
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS hdd_softap_register_bc_sta(struct hdd_adapter *adapter,
				      bool privacy_required);

/**
 * hdd_softap_stop_bss() - Stop the BSS
 * @adapter: pointer to adapter context
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS hdd_softap_stop_bss(struct hdd_adapter *adapter);

/**
 * hdd_softap_change_sta_state() - Change the state of a SoftAP station
 * @adapter: pointer to adapter context
 * @sta_mac: MAC address of the station
 * @state: new state of the station
 *
 * Return: QDF_STATUS_SUCCESS on success, QDF_STATUS_E_** on error
 */
QDF_STATUS hdd_softap_change_sta_state(struct hdd_adapter *adapter,
				       struct qdf_mac_addr *sta_mac,
				       enum ol_txrx_peer_state state);

#ifdef QCA_LL_LEGACY_TX_FLOW_CONTROL
/**
 * hdd_softap_tx_resume_timer_expired_handler() - TX Q resume timer handler
 * @adapter_context: pointer to vdev adapter
 *
 * TX Q resume timer handler for SAP and P2P GO interface.  If Blocked
 * OS Q is not resumed during timeout period, to prevent permanent
 * stall, resume OS Q forcefully for SAP and P2P GO interface.
 *
 * Return: None
 */
void hdd_softap_tx_resume_timer_expired_handler(void *adapter_context);

/**
 * hdd_softap_tx_resume_cb() - Resume OS TX Q.
 * @adapter_context: pointer to vdev apdapter
 * @tx_resume: TX Q resume trigger
 *
 * Q was stopped due to WLAN TX path low resource condition
 *
 * Return: None
 */
void hdd_softap_tx_resume_cb(void *adapter_context, bool tx_resume);
#else
static inline
void hdd_softap_tx_resume_timer_expired_handler(void *adapter_context)
{
}

static inline
void hdd_softap_tx_resume_cb(void *adapter_context, bool tx_resume)
{
}
#endif /* QCA_LL_LEGACY_TX_FLOW_CONTROL */

/**
 * hdd_ipa_update_rx_mcbc_stats() - Update broadcast multicast stats
 * @adapter: pointer to hdd adapter
 * @skb: pointer to netbuf
 *
 * Check if multicast or broadcast pkt was received and increment
 * the stats accordingly. This is required only if IPA is enabled
 * as in case of regular Rx path mcast/bcast stats are processed
 * in the dp layer.
 *
 * Return: None
 */
void hdd_ipa_update_rx_mcbc_stats(struct hdd_adapter *adapter,
				  struct sk_buff *skb);

#ifdef SAP_DHCP_FW_IND
/**
 * hdd_post_dhcp_ind() - Send DHCP START/STOP indication to FW
 * @adapter: pointer to hdd adapter
 * @mac_addr: mac address
 * @type: WMA message type
 *
 * Return: error number
 */
int hdd_post_dhcp_ind(struct hdd_adapter *adapter,
		      uint8_t *mac_addr, uint16_t type);

/**
 * hdd_softap_inspect_dhcp_packet() - Inspect DHCP packet
 * @adapter: pointer to hdd adapter
 * @skb: pointer to OS packet (sk_buff)
 * @dir: direction
 *
 * Inspect the Tx/Rx frame, and send DHCP START/STOP notification to the FW
 * through WMI message, during DHCP based IP address acquisition phase.
 *
 * - Send DHCP_START notification to FW when SAP gets DHCP Discovery
 * - Send DHCP_STOP notification to FW when SAP sends DHCP ACK/NAK
 *
 * DHCP subtypes are determined by a status octet in the DHCP Message type
 * option (option code 53 (0x35)).
 *
 * Each peer will be in one of 4 DHCP phases, starts from QDF_DHCP_PHASE_ACK,
 * and transitioned per DHCP message type as it arrives.
 *
 * - QDF_DHCP_PHASE_DISCOVER: upon receiving DHCP_DISCOVER message in ACK phase
 * - QDF_DHCP_PHASE_OFFER: upon receiving DHCP_OFFER message in DISCOVER phase
 * - QDF_DHCP_PHASE_REQUEST: upon receiving DHCP_REQUEST message in OFFER phase
 *	or ACK phase (Renewal process)
 * - QDF_DHCP_PHASE_ACK : upon receiving DHCP_ACK/NAK message in REQUEST phase
 *	or DHCP_DELINE message in OFFER phase
 *
 * Return: error number
 */
int hdd_softap_inspect_dhcp_packet(struct hdd_adapter *adapter,
				   struct sk_buff *skb,
				   enum qdf_proto_dir dir);
#else
static inline
int hdd_post_dhcp_ind(struct hdd_adapter *adapter,
		      uint8_t *mac_addr, uint16_t type)
{
	return 0;
}

static inline
int hdd_softap_inspect_dhcp_packet(struct hdd_adapter *adapter,
				   struct sk_buff *skb,
				   enum qdf_proto_dir dir)
{
	return 0;
}
#endif

/**
 * hdd_softap_check_wait_for_tx_eap_pkt() - Check and wait for eap failure
 * pkt completion event
 * @adapter: pointer to hdd adapter
 * @mac_addr: mac address of peer
 *
 * Check and wait for eap failure pkt tx completion.
 *
 * Return: void
 */
void hdd_softap_check_wait_for_tx_eap_pkt(struct hdd_adapter *adapter,
					  struct qdf_mac_addr *mac_addr);
#endif /* end #if !defined(WLAN_HDD_SOFTAP_TX_RX_H) */
