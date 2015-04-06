/*
 * Copyright (c) 2012-2014 The Linux Foundation. All rights reserved.
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

/**=========================================================================

  \file        vos_packet.c

  \brief       virtual Operating System Services (vOSS) network Packet APIs

   Network Protocol packet/buffer support interfaces

  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include <vos_packet.h>
#include <i_vos_packet.h>
#include <vos_timer.h>
#include <vos_trace.h>
#include <wlan_hdd_main.h>
#include "adf_nbuf.h"
#include "vos_memory.h"
#include "adf_os_mem.h"

#ifdef QCA_PKT_PROTO_TRACE
/* Protocol specific packet tracking feature */
#define VOS_PKT_TRAC_ETH_TYPE_OFFSET 12
#define VOS_PKT_TRAC_IP_OFFSET       14
#define VOS_PKT_TRAC_IP_HEADER_SIZE  20
#define VOS_PKT_TRAC_DHCP_SRV_PORT   67
#define VOS_PKT_TRAC_DHCP_CLI_PORT   68
#define VOS_PKT_TRAC_EAPOL_ETH_TYPE  0x888E
#define VOS_PKT_TRAC_MAX_STRING_LEN  12
#define VOS_PKT_TRAC_MAX_TRACE_BUF   50
#define VOS_PKT_TRAC_MAX_STRING_BUF  64

/* protocol Storage Structure */
typedef struct
{
   v_U32_t  order;
   v_TIME_t event_time;
   char     event_string[VOS_PKT_TRAC_MAX_STRING_LEN];
} vos_pkt_proto_trace_t;

vos_pkt_proto_trace_t  *trace_buffer = NULL;
unsigned int            trace_buffer_order = 0;
vos_spin_lock_t         trace_buffer_lock;
#endif /* QCA_PKT_PROTO_TRACE */

/**
 * vos_pkt_return_packet  Free the voss Packet
 * @ vos Packet
 */
VOS_STATUS vos_pkt_return_packet(vos_pkt_t *packet)
{
   // Validate the input parameter pointer
   if (unlikely(packet == NULL)) {
      return VOS_STATUS_E_INVAL;
   }

   /* Free up the Adf nbuf */
   adf_nbuf_free(packet->pkt_buf);

   packet->pkt_buf = NULL;

   /* Free up the Rx packet */
   vos_mem_free(packet);

   return VOS_STATUS_SUCCESS;
}

/**--------------------------------------------------------------------------

  \brief vos_pkt_get_packet_length() - Get packet length for a voss Packet

  This API returns the total length of the data in a voss Packet.

  \param pPacket - the voss Packet to get the packet length from.

  \param pPacketSize - location to return the total size of the data contained
                       in the voss Packet.
  \return

  \sa

  ---------------------------------------------------------------------------*/
VOS_STATUS vos_pkt_get_packet_length( vos_pkt_t *pPacket,
                                      v_U16_t *pPacketSize )
{
   // Validate the parameter pointers
   if (unlikely((pPacket == NULL) || (pPacketSize == NULL)) ||
                   (pPacket->pkt_buf == NULL))
   {
      VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_FATAL,
                "VPKT [%d]: NULL pointer", __LINE__);
      return VOS_STATUS_E_INVAL;
   }

   // return the requested information
   *pPacketSize = adf_nbuf_len(pPacket->pkt_buf);
   return VOS_STATUS_SUCCESS;
}

/*
 * TODO: Remove below later since all the below
 * definitions are not required for Host
 * driver 2.0 (still references from HDD and
 * other layers are yet to be removed)
 */
VOS_STATUS vos_pkt_get_available_buffer_pool (VOS_PKT_TYPE  pktType,
                                              v_SIZE_t     *vosFreeBuffer)
{
    return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_pkt_get_os_packet( vos_pkt_t *pPacket,
                                  v_VOID_t **ppOSPacket,
                                  v_BOOL_t clearOSPacket )
{
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_pkt_wrap_data_packet( vos_pkt_t **ppPacket,
                                     VOS_PKT_TYPE pktType,
                                     v_VOID_t *pOSPacket,
                                     vos_pkt_get_packet_callback callback,
                                     v_VOID_t *userData )
{
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_pkt_set_os_packet( vos_pkt_t *pPacket,
                                  v_VOID_t *pOSPacket )
{
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_pkt_get_timestamp( vos_pkt_t *pPacket,
                                  v_TIME_t* pTstamp )
{
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_pkt_walk_packet_chain( vos_pkt_t *pPacket,
                                      vos_pkt_t **ppChainedPacket,
                                      v_BOOL_t unchainPacket )
{
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_pkt_peek_data( vos_pkt_t *pPacket,
                              v_SIZE_t pktOffset,
                              v_VOID_t **ppPacketData,
                              v_SIZE_t numBytes )
{
   return VOS_STATUS_SUCCESS;
}
VOS_STATUS vos_pkt_get_packet( vos_pkt_t **ppPacket,
                               VOS_PKT_TYPE pktType,
                               v_SIZE_t dataSize,
                               v_SIZE_t numPackets,
                               v_BOOL_t zeroBuffer,
                               vos_pkt_get_packet_callback callback,
                               v_VOID_t *userData )
{
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_pkt_reserve_head( vos_pkt_t *pPacket,
                                 v_VOID_t **ppData,
                                 v_SIZE_t dataSize )
{
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_pkt_pop_head( vos_pkt_t *pPacket,
                             v_VOID_t *pData,
                             v_SIZE_t dataSize )
{
   return VOS_STATUS_SUCCESS;
}

VOS_STATUS vos_pkt_push_head( vos_pkt_t *pPacket,
                              v_VOID_t *pData,
                              v_SIZE_t dataSize )
{
   return VOS_STATUS_SUCCESS;
}

v_VOID_t vos_pkt_set_user_data_ptr( vos_pkt_t *pPacket,
                                    VOS_PKT_USER_DATA_ID userID,
                                    v_VOID_t *pUserData )
{
   return;
}

v_VOID_t vos_pkt_get_user_data_ptr( vos_pkt_t *pPacket,
                                    VOS_PKT_USER_DATA_ID userID,
                                    v_VOID_t **ppUserData )
{
   return;
}

VOS_STATUS vos_pkt_extract_data( vos_pkt_t *pPacket,
                                 v_SIZE_t pktOffset,
                                 v_VOID_t *pOutputBuffer,
                                 v_SIZE_t *pOutputBufferSize )
{
   return VOS_STATUS_SUCCESS;
}

#ifdef QCA_PKT_PROTO_TRACE
/*---------------------------------------------------------------------------

  * brief vos_pkt_get_proto_type() -
      Find protoco type from packet contents

  * skb Packet Pointer
  * tracking_map packet type want to track
  * dot11_type, type of dot11 frame

---------------------------------------------------------------------------*/
v_U8_t vos_pkt_get_proto_type
(
   struct sk_buff *skb,
   v_U8_t tracking_map,
   v_U8_t dot11_type
)
{
   v_U8_t     pkt_proto_type = 0;
   v_U16_t    ether_type;
   v_U16_t    SPort;
   v_U16_t    DPort;

   if (dot11_type)
   {
      if (dot11_type == (VOS_PKT_TRAC_TYPE_MGMT_ACTION & tracking_map))
         pkt_proto_type |= VOS_PKT_TRAC_TYPE_MGMT_ACTION;

      /* Protocol type map */
      return pkt_proto_type;
   }

   /* EAPOL Tracking enabled */
   if (VOS_PKT_TRAC_TYPE_EAPOL & tracking_map)
   {
      ether_type = (v_U16_t)(*(v_U16_t *)(skb->data + VOS_PKT_TRAC_ETH_TYPE_OFFSET));
      if (VOS_PKT_TRAC_EAPOL_ETH_TYPE == VOS_SWAP_U16(ether_type))
      {
         pkt_proto_type |= VOS_PKT_TRAC_TYPE_EAPOL;
      }
   }

   /* DHCP Tracking enabled */
   if (VOS_PKT_TRAC_TYPE_DHCP & tracking_map)
   {
      SPort = (v_U16_t)(*(v_U16_t *)(skb->data + VOS_PKT_TRAC_IP_OFFSET +
                                     VOS_PKT_TRAC_IP_HEADER_SIZE));
      DPort = (v_U16_t)(*(v_U16_t *)(skb->data + VOS_PKT_TRAC_IP_OFFSET +
                                     VOS_PKT_TRAC_IP_HEADER_SIZE + sizeof(v_U16_t)));
      if (((VOS_PKT_TRAC_DHCP_SRV_PORT == VOS_SWAP_U16(SPort)) &&
           (VOS_PKT_TRAC_DHCP_CLI_PORT == VOS_SWAP_U16(DPort))) ||
          ((VOS_PKT_TRAC_DHCP_CLI_PORT == VOS_SWAP_U16(SPort)) &&
           (VOS_PKT_TRAC_DHCP_SRV_PORT == VOS_SWAP_U16(DPort))))
      {
         pkt_proto_type |= VOS_PKT_TRAC_TYPE_DHCP;
      }
   }

   /* Protocol type map */
   return pkt_proto_type;
}

/*---------------------------------------------------------------------------

  * brief vos_pkt_trace_buf_update() -
      Update storage buffer with interest event string

  * event_string Event String may packet type or outstanding event

---------------------------------------------------------------------------*/
void vos_pkt_trace_buf_update
(
   char    *event_string
)
{
   v_U32_t slot;

   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_INFO,
             "%s %d, %s", __func__, __LINE__, event_string);
   vos_spin_lock_acquire(&trace_buffer_lock);
   slot = trace_buffer_order % VOS_PKT_TRAC_MAX_TRACE_BUF;
   trace_buffer[slot].order = trace_buffer_order;
   trace_buffer[slot].event_time = vos_timer_get_system_time();
   vos_mem_zero(trace_buffer[slot].event_string,
                sizeof(trace_buffer[slot].event_string));
   vos_mem_copy(trace_buffer[slot].event_string,
                event_string,
                (VOS_PKT_TRAC_MAX_STRING_LEN < strlen(event_string))?
                VOS_PKT_TRAC_MAX_STRING_LEN:strlen(event_string));
   trace_buffer_order++;
   vos_spin_lock_release(&trace_buffer_lock);

   return;
}

/*---------------------------------------------------------------------------

  * brief vos_pkt_trace_buf_dump() -
      Dump stored information into kernel log

---------------------------------------------------------------------------*/
void vos_pkt_trace_buf_dump
(
   void
)
{
   v_U32_t slot, idx;

   vos_spin_lock_acquire(&trace_buffer_lock);
   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             "PACKET TRACE DUMP START Current Timestamp %u",
              (unsigned int)vos_timer_get_system_time());
   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             "ORDER :        TIME : EVT");
   if (VOS_PKT_TRAC_MAX_TRACE_BUF > trace_buffer_order)
   {
      for (slot = 0 ; slot < trace_buffer_order; slot++)
      {
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   "%5d :%12u : %s",
                   trace_buffer[slot].order,
                   (unsigned int)trace_buffer[slot].event_time,
                   trace_buffer[slot].event_string);
      }
   }
   else
   {
      for (idx = 0 ; idx < VOS_PKT_TRAC_MAX_TRACE_BUF; idx++)
      {
         slot = (trace_buffer_order + idx) % VOS_PKT_TRAC_MAX_TRACE_BUF;
         VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
                   "%5d :%12u : %s",
                   trace_buffer[slot].order,
                   (unsigned int)trace_buffer[slot].event_time,
                   trace_buffer[slot].event_string);
      }
   }

   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             "PACKET TRACE DUMP END");
   vos_spin_lock_release(&trace_buffer_lock);

   return;
}

/*---------------------------------------------------------------------------

  * brief vos_pkt_proto_trace_init() -
      Initialize protocol trace functionality, allocate required resource

---------------------------------------------------------------------------*/
void vos_pkt_proto_trace_init
(
   void
)
{
   /* Init spin lock to protect global memory */
   vos_spin_lock_init(&trace_buffer_lock);
   trace_buffer_order = 0;
   trace_buffer = vos_mem_malloc(
       VOS_PKT_TRAC_MAX_TRACE_BUF * sizeof(vos_pkt_proto_trace_t));
   vos_mem_zero((void *)trace_buffer,
       VOS_PKT_TRAC_MAX_TRACE_BUF * sizeof(vos_pkt_proto_trace_t));

   /* Register callback function to NBUF
    * Lower layer event also will be reported to here */
   adf_nbuf_reg_trace_cb(vos_pkt_trace_buf_update);
   return;
}

/*---------------------------------------------------------------------------

  * brief vos_pkt_proto_trace_close() -
      Free required resource

---------------------------------------------------------------------------*/
void vos_pkt_proto_trace_close
(
   void
)
{
   VOS_TRACE(VOS_MODULE_ID_VOSS, VOS_TRACE_LEVEL_ERROR,
             "%s %d", __func__, __LINE__);
   vos_mem_free(trace_buffer);
   vos_spin_lock_destroy(&trace_buffer_lock);

   return;
}
#endif /* QCA_PKT_PROTO_TRACE */
