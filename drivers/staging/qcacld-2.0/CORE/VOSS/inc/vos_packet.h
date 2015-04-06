/*
 * Copyright (c) 2011-2012,2014 The Linux Foundation. All rights reserved.
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

#if !defined( __VOS_PKT_H )
#define __VOS_PKT_H

/**=========================================================================

  \file        vos_packet.h

  \brief       virtual Operating System Services (vOSS) network Packet APIs

   Network Protocol packet/buffer support interfaces

  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_types.h>
#include <vos_status.h>

/*--------------------------------------------------------------------------
  Preprocessor definitions and constants
  ------------------------------------------------------------------------*/

/*--------------------------------------------------------------------------
  Type declarations
  ------------------------------------------------------------------------*/
struct vos_pkt_t;
typedef struct vos_pkt_t vos_pkt_t;


#ifdef QCA_PKT_PROTO_TRACE
#include "adf_nbuf.h"

#define VOS_PKT_TRAC_TYPE_EAPOL   NBUF_PKT_TRAC_TYPE_EAPOL
#define VOS_PKT_TRAC_TYPE_DHCP    NBUF_PKT_TRAC_TYPE_DHCP
#define VOS_PKT_TRAC_TYPE_MGMT_ACTION    NBUF_PKT_TRAC_TYPE_MGMT_ACTION /* Managment action frame */

#define VOS_PKT_TRAC_DUMP_CMD     9999

/*---------------------------------------------------------------------------

  * brief vos_pkt_get_proto_type() -
      Find protoco type from packet contents

  * skb Packet Pointer
  * tracking_map packet type want to track
  * dot11_type, frame type when the frame is in dot11 format

---------------------------------------------------------------------------*/
v_U8_t vos_pkt_get_proto_type
(
   struct sk_buff *skb,
   v_U8_t tracking_map,
   v_BOOL_t dot11_type
);

/*---------------------------------------------------------------------------

  * brief vos_pkt_trace_buf_update() -
      Update storage buffer with interest event string

  * event_string Event String may packet type or outstanding event

---------------------------------------------------------------------------*/
void vos_pkt_trace_buf_update
(
   char    *event_string
);

/*---------------------------------------------------------------------------

  * brief vos_pkt_trace_buf_dump() -
      Dump stored information into kernel log

---------------------------------------------------------------------------*/
void vos_pkt_trace_buf_dump
(
   void
);

/*---------------------------------------------------------------------------

  * brief vos_pkt_proto_trace_init() -
      Initialize protocol trace functionality, allocate required resource

---------------------------------------------------------------------------*/
void vos_pkt_proto_trace_init
(
   void
);

/*---------------------------------------------------------------------------

  * brief vos_pkt_proto_trace_close() -
      Free required resource

---------------------------------------------------------------------------*/
void vos_pkt_proto_trace_close
(
   void
);
#endif /* QCA_PKT_PROTO_TRACE */

/**
 * vos_pkt_return_packet  Free the voss Packet
 * @ vos Packet
 */
VOS_STATUS vos_pkt_return_packet(vos_pkt_t *packet);

/**
 * vos_pkt_get_packet_length  Returns the packet length
 * @ vos Packet
 */
VOS_STATUS vos_pkt_get_packet_length( vos_pkt_t *pPacket,
                                      v_U16_t *pPacketSize );

/*
 * TODO: Remove later
 * All the below difinitions are not
 * required for Host Driver 2.0
 * once corresponding references are removed
 * from HDD and other layers
 * below code will be removed
 */
//The size of AMSDU frame per spec can be a max of 3839 bytes
// in BD/PDUs that means 30 (one BD = 128 bytes)
// we must add the size of the 802.11 header to that
#define VPKT_SIZE_BUFFER  ((30 * 128) + 32)

/// voss Packet Types
typedef enum
{
   /// voss Packet is used to transmit 802.11 Management frames.
   VOS_PKT_TYPE_TX_802_11_MGMT,

   /// voss Packet is used to transmit 802.11 Data frames.
   VOS_PKT_TYPE_TX_802_11_DATA,

   /// voss Packet is used to transmit 802.3 Data frames.
   VOS_PKT_TYPE_TX_802_3_DATA,

   /// voss Packet contains Received data of an unknown frame type
   VOS_PKT_TYPE_RX_RAW,

   /// Invalid sentinel value
   VOS_PKT_TYPE_MAXIMUM

} VOS_PKT_TYPE;

/// user IDs.   These IDs are needed on the vos_pkt_get/set_user_data_ptr()
/// to identify the user area in the voss Packet.
typedef enum
{
   VOS_PKT_USER_DATA_ID_TL =0,
   VOS_PKT_USER_DATA_ID_BAL,
   VOS_PKT_USER_DATA_ID_WDA,
   VOS_PKT_USER_DATA_ID_HDD,
   VOS_PKT_USER_DATA_ID_BAP,
   VOS_PKT_USER_DATA_ID_BSL,

   VOS_PKT_USER_DATA_ID_MAX

} VOS_PKT_USER_DATA_ID;

typedef VOS_STATUS ( *vos_pkt_get_packet_callback )( vos_pkt_t *pPacket,
                                                     v_VOID_t *userData );

VOS_STATUS vos_pkt_get_available_buffer_pool (VOS_PKT_TYPE  pktType,
                                              v_SIZE_t     *vosFreeBuffer);
VOS_STATUS vos_pkt_get_os_packet( vos_pkt_t *pPacket,
                                  v_VOID_t **ppOSPacket,
                                  v_BOOL_t clearOSPacket );
VOS_STATUS vos_pkt_wrap_data_packet( vos_pkt_t **ppPacket,
                                     VOS_PKT_TYPE pktType,
                                     v_VOID_t *pOSPacket,
                                     vos_pkt_get_packet_callback callback,
                                     v_VOID_t *userData );
VOS_STATUS vos_pkt_set_os_packet( vos_pkt_t *pPacket,
                                  v_VOID_t *pOSPacket );
VOS_STATUS vos_pkt_get_timestamp( vos_pkt_t *pPacket,
                                  v_TIME_t* pTstamp );
VOS_STATUS vos_pkt_walk_packet_chain( vos_pkt_t *pPacket,
                                      vos_pkt_t **ppChainedPacket,
                                      v_BOOL_t unchainPacket );
VOS_STATUS vos_pkt_peek_data( vos_pkt_t *pPacket,
                              v_SIZE_t pktOffset,
                              v_VOID_t **ppPacketData,
                              v_SIZE_t numBytes );
VOS_STATUS vos_pkt_get_packet( vos_pkt_t **ppPacket,
                               VOS_PKT_TYPE pktType,
                               v_SIZE_t dataSize,
                               v_SIZE_t numPackets,
                               v_BOOL_t zeroBuffer,
                               vos_pkt_get_packet_callback callback,
                               v_VOID_t *userData );
VOS_STATUS vos_pkt_reserve_head( vos_pkt_t *pPacket,
                                 v_VOID_t **ppData,
                                 v_SIZE_t dataSize );
VOS_STATUS vos_pkt_pop_head( vos_pkt_t *pPacket,
                             v_VOID_t *pData,
                             v_SIZE_t dataSize );
VOS_STATUS vos_pkt_push_head( vos_pkt_t *pPacket,
                              v_VOID_t *pData,
                              v_SIZE_t dataSize );
v_VOID_t vos_pkt_get_user_data_ptr( vos_pkt_t *pPacket,
                                    VOS_PKT_USER_DATA_ID userID,
                                    v_VOID_t **ppUserData );
v_VOID_t vos_pkt_set_user_data_ptr( vos_pkt_t *pPacket,
                                    VOS_PKT_USER_DATA_ID userID,
                                    v_VOID_t *pUserData );
VOS_STATUS vos_pkt_extract_data( vos_pkt_t *pPacket,
                                 v_SIZE_t pktOffset,
                                 v_VOID_t *pOutputBuffer,
                                 v_SIZE_t *pOutputBufferSize );
#endif  // !defined( __VOS_PKT_H )
