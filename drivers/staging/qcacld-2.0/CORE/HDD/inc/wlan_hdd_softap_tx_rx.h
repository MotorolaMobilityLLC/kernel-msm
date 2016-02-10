/*
 * Copyright (c) 2014 The Linux Foundation. All rights reserved.
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


#if !defined( WLAN_HDD_SOFTAP_TX_RX_H )
#define WLAN_HDD_SOFTAP_TX_RX_H

/**===========================================================================

  \file  wlan_hdd_softap_tx_rx.h

  \brief Linux HDD SOFTAP Tx/RX APIs

  ==========================================================================*/

/*---------------------------------------------------------------------------
  Include files
  -------------------------------------------------------------------------*/
#include <wlan_hdd_hostapd.h>

/*---------------------------------------------------------------------------
  Preprocessor definitions and constants
  -------------------------------------------------------------------------*/
#define HDD_SOFTAP_TX_BK_QUEUE_MAX_LEN (82*2)
#define HDD_SOFTAP_TX_BE_QUEUE_MAX_LEN (78*2)
#define HDD_SOFTAP_TX_VI_QUEUE_MAX_LEN (74*2)
#define HDD_SOFTAP_TX_VO_QUEUE_MAX_LEN (70*2)

/*---------------------------------------------------------------------------
  Type declarations
  -------------------------------------------------------------------------*/

/*---------------------------------------------------------------------------
  Function declarations and documentation
  -------------------------------------------------------------------------*/

/**============================================================================
  @brief hdd_softap_hard_start_xmit() - Function registered with the Linux OS for
  transmitting packets

  @param skb      : [in]  pointer to OS packet (sk_buff)
  @param dev      : [in] pointer to Libra softap network device

  @return         : NET_XMIT_DROP if packets are dropped
                  : NET_XMIT_SUCCESS if packet is enqueued successfully
  ===========================================================================*/
extern int hdd_softap_hard_start_xmit(struct sk_buff *skb, struct net_device *dev);

/**============================================================================
  @brief hdd_softap_tx_timeout() - Function called by OS if there is any
  timeout during transmission. Since HDD simply enqueues packet
  and returns control to OS right away, this would never be invoked

  @param dev : [in] pointer to Libra network device
  @return    : None
  ===========================================================================*/
extern void hdd_softap_tx_timeout(struct net_device *dev);

/**============================================================================
  @brief hdd_softap_stats() - Function registered with the Linux OS for
  device TX/RX statistics

  @param dev      : [in] pointer to Libra network device

  @return         : pointer to net_device_stats structure
  ===========================================================================*/
extern struct net_device_stats* hdd_softap_stats(struct net_device *dev);

/**============================================================================
  @brief hdd_softap_init_tx_rx() - Init function to initialize Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_init_tx_rx( hdd_adapter_t *pAdapter );

/**============================================================================
  @brief hdd_softap_deinit_tx_rx() - Deinit function to clean up Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_deinit_tx_rx( hdd_adapter_t *pAdapter );

/**============================================================================
  @brief hdd_softap_init_tx_rx_sta() - Init function to initialize a station in Tx/RX
  modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @param STAId    : [in] Station ID to deinit
  @param pmacAddrSTA  : [in] pointer to the MAC address of the station
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_init_tx_rx_sta( hdd_adapter_t *pAdapter, v_U8_t STAId, v_MACADDR_t *pmacAddrSTA);

/**============================================================================
  @brief hdd_softap_deinit_tx_rx_sta() - Deinit function to clean up a station
                                         in Tx/RX modules in HDD

  @param pAdapter : [in] pointer to adapter context
  @param STAId    : [in] Station ID to deinit
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_deinit_tx_rx_sta ( hdd_adapter_t *pAdapter, v_U8_t STAId );

/**============================================================================
  @brief hdd_softap_rx_packet_cbk() - Receive callback registered with TL.
  TL will call this to notify the HDD when a packet was received
  for a registered STA.

  @param vosContext   : [in] pointer to VOS context
  @param rxBufChain   : [in] pointer to adf_nbuf rx chain
  @param staId        : [in] Station Id

  @return             : VOS_STATUS_E_FAILURE if any errors encountered,
                      : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_rx_packet_cbk(v_VOID_t *vosContext,
                                           adf_nbuf_t rxBufChain,
                                           v_U8_t staId);

/**============================================================================
  @brief hdd_softap_DeregisterSTA - Deregister a station from TL block

  @param pAdapter : [in] pointer to adapter context
  @param STAId    : [in] Station ID to deregister
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  ===========================================================================*/
extern VOS_STATUS hdd_softap_DeregisterSTA( hdd_adapter_t *pAdapter, tANI_U8 staId );

/**============================================================================
  @brief hdd_softap_RegisterSTA - Register a station into TL block

  @param pAdapter : [in] pointer to adapter context
  @param STAId    : [in] Station ID to deregister
  @param fAuthRequired: [in] Station requires further security negotiation or not
  @param fPrivacyBit  : [in] privacy bit needs to be set or not
  @param ucastSig  : [in] Unicast Signature send to TL
  @param bcastSig  : [in] Broadcast Signature send to TL
  @param pPeerMacAddress  : [in] station MAC address
  @param fWmmEnabled  : [in] Wmm enabled sta or not
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_RegisterSTA( hdd_adapter_t *pAdapter,
                                       v_BOOL_t fAuthRequired,
                                       v_BOOL_t fPrivacyBit,
                                       v_U8_t staId,
                                       v_U8_t ucastSig,
                                       v_U8_t bcastSig,
                                       v_MACADDR_t *pPeerMacAddress,
                                       v_BOOL_t fWmmEnabled);

/**============================================================================
  @brief hdd_softap_Register_BC_STA - Register a default broadcast station into TL block

  @param pAdapter : [in] pointer to adapter context
  @param fPrivacyBit : [in] privacy bit needs to be set or not
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_Register_BC_STA( hdd_adapter_t *pAdapter, v_BOOL_t fPrivacyBit);

/**============================================================================
  @brief hdd_softap_DeregisterSTA - DeRegister the default broadcast station into TL block

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_Deregister_BC_STA( hdd_adapter_t *pAdapter);

/**============================================================================
  @brief hdd_softap_stop_bss - Helper function to stop bss and do cleanup in HDD and TL

  @param pAdapter : [in] pointer to adapter context
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_stop_bss( hdd_adapter_t *pHostapdAdapter);


/**============================================================================
  @brief hdd_softap_change_STA_state - Helper function to change station state by MAC address

  @param pAdapter : [in] pointer to adapter context
  @param pDestMacAddress : [in] pointer to station MAC address
  @param state    : [in] new station state
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_change_STA_state( hdd_adapter_t *pAdapter, v_MACADDR_t *pDestMacAddress, WLANTL_STAStateType state);

/**============================================================================
  @brief hdd_softap_GetStaId - Helper function to get station Id from MAC address

  @param pAdapter : [in] pointer to adapter context
  @param pDestMacAddress : [in] pointer to station MAC address
  @param staId    : [out] station id
  @return         : VOS_STATUS_E_FAILURE if any errors encountered
                  : VOS_STATUS_SUCCESS otherwise
  =========================================================================== */
extern VOS_STATUS hdd_softap_GetStaId( hdd_adapter_t *pAdapter, v_MACADDR_t *pMacAddress, v_U8_t *staId);

#ifdef QCA_LL_TX_FLOW_CT
/**============================================================================
  @brief hdd_softap_tx_resume_timer_expired_handler() - Resume OS TX Q timer
      expired handler for SAP and P2P GO interface.
      If Blocked OS Q is not resumed during timeout period, to prevent
      permanent stall, resume OS Q forcefully for SAP and P2P GO interface.

  @param adapter_context : [in] pointer to vdev adapter

  @return         : NONE
  ===========================================================================*/
void hdd_softap_tx_resume_timer_expired_handler(void *adapter_context);

/**============================================================================
  @brief hdd_softap_tx_resume_cb() - Resume OS TX Q.
      Q was stopped due to WLAN TX path low resource condition

  @param adapter_context : [in] pointer to vdev adapter
  @param tx_resume       : [in] TX Q resume trigger

  @return         : NONE
  ===========================================================================*/
void hdd_softap_tx_resume_cb(void *adapter_context,
                        v_BOOL_t tx_resume);
#endif /* QCA_LL_TX_FLOW_CT */
#endif    // end #if !defined( WLAN_HDD_SOFTAP_TX_RX_H )
