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

#if !defined( __I_VOS_PACKET_H )
#define __I_VOS_PACKET_H

/**=========================================================================

  \file        i_vos_packet.h

  \brief       virtual Operating System Servies (vOSS)

   Network Protocol packet/buffer internal include file

  ========================================================================*/

/* $Header$ */

/*--------------------------------------------------------------------------
  Include Files
  ------------------------------------------------------------------------*/
#include "adf_os_types.h"
/*
 * Rx Packet Struct
 * rssi field is normalized to -96 dBm as normal noise floor by adding
 * -96 to snr. All the configured thresholds in the driver assume that
 * noise floor is -96 dBm.
 * rssi_raw field is signal strength of the received frame relative
 * to the noise floor recorded in hardware.
 */
typedef struct
{
   u_int8_t channel;
   u_int8_t snr;
   u_int32_t rssi;
   u_int32_t timestamp;
   u_int8_t *mpdu_hdr_ptr;
   u_int8_t *mpdu_data_ptr;
   u_int32_t mpdu_len;
   u_int32_t mpdu_hdr_len;
   u_int32_t mpdu_data_len;
   u_int8_t  offloadScanLearn:1;
   u_int8_t  roamCandidateInd:1;
   u_int8_t  scan:1;
   u_int8_t  dpuFeedback;
   u_int8_t  sessionId;
   u_int8_t  scan_src;
   u_int32_t rssi_raw;
   u_int32_t tsf_delta;
}t_packetmeta, *tp_packetmeta;

/* implementation specific vos packet type */
struct vos_pkt_t
{
   /* Packet Meta Information */
   t_packetmeta pkt_meta;

   /* Pointer to Packet */
   void *pkt_buf;
};

#endif  // !defined( __I_VOS_PACKET_H )
