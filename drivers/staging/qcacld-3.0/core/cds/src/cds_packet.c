/*
 * Copyright (c) 2014-2016, 2018-2020 The Linux Foundation. All rights reserved.
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

/**=========================================================================

   \file        cds_packet.c

   \brief       Connectivity driver services (CDS) network Packet APIs

   Network Protocol packet/buffer support interfaces

   ========================================================================*/

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include <cds_packet.h>
#include <i_cds_packet.h>
#include <qdf_mc_timer.h>
#include <qdf_trace.h>
#include <wlan_hdd_main.h>
#include "qdf_nbuf.h"
#include "qdf_mem.h"
#include "cds_utils.h"

#define TX_PKT_MIN_HEADROOM          (64)

/**
 * cds_pkt_return_packet  Free the cds Packet
 * @ cds Packet
 */
QDF_STATUS cds_pkt_return_packet(cds_pkt_t *packet)
{
	/* Validate the input parameter pointer */
	if (unlikely(!packet)) {
		return QDF_STATUS_E_INVAL;
	}

	/* Free up the qdf nbuf */
	qdf_nbuf_free(packet->pkt_buf);

	packet->pkt_buf = NULL;

	/* Free up the Rx packet */
	qdf_mem_free(packet);

	return QDF_STATUS_SUCCESS;
}

/**--------------------------------------------------------------------------

   \brief cds_pkt_get_packet_length() - Get packet length for a cds Packet

   This API returns the total length of the data in a cds Packet.

   \param pPacket - the cds Packet to get the packet length from.

   \param pPacketSize - location to return the total size of the data contained
   in the cds Packet.
   \return

   \sa

   ---------------------------------------------------------------------------*/
QDF_STATUS
cds_pkt_get_packet_length(cds_pkt_t *pPacket, uint16_t *pPacketSize)
{
	/* Validate the parameter pointers */
	if (unlikely((!pPacket) || (!pPacketSize)) ||
	    (!pPacket->pkt_buf)) {
		cds_alert("NULL pointer");
		return QDF_STATUS_E_INVAL;
	}
	/* return the requested information */
	*pPacketSize = qdf_nbuf_len(pPacket->pkt_buf);
	return QDF_STATUS_SUCCESS;
}

#ifdef MEMORY_DEBUG
/*---------------------------------------------------------------------------
* @brief cds_packet_alloc_debug() -
      Allocate a network buffer for TX
   ---------------------------------------------------------------------------*/
QDF_STATUS cds_packet_alloc_debug(uint16_t size, void **data, void **ppPacket,
				  const char *func_name, uint32_t line_num)
{
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;
	qdf_nbuf_t nbuf;

	nbuf = qdf_nbuf_alloc_debug(NULL,
		roundup(size + TX_PKT_MIN_HEADROOM, 4),
		TX_PKT_MIN_HEADROOM, sizeof(uint32_t), false,
				     func_name, line_num);

	if (nbuf) {
		qdf_nbuf_put_tail(nbuf, size);
		qdf_nbuf_set_protocol(nbuf, ETH_P_CONTROL);
		*ppPacket = nbuf;
		*data = qdf_nbuf_data(nbuf);
		qdf_ret_status = QDF_STATUS_SUCCESS;
	}

	return qdf_ret_status;
}
#else
/*---------------------------------------------------------------------------
* @brief cds_packet_alloc() -
      Allocate a network buffer for TX
   ---------------------------------------------------------------------------*/
QDF_STATUS cds_packet_alloc(uint16_t size, void **data, void **ppPacket)
{
	QDF_STATUS qdf_ret_status = QDF_STATUS_E_FAILURE;
	qdf_nbuf_t nbuf;

	nbuf = qdf_nbuf_alloc(NULL, roundup(size + TX_PKT_MIN_HEADROOM, 4),
			      TX_PKT_MIN_HEADROOM, sizeof(uint32_t), false);

	if (nbuf) {
		qdf_nbuf_put_tail(nbuf, size);
		qdf_nbuf_set_protocol(nbuf, ETH_P_CONTROL);
		*ppPacket = nbuf;
		*data = qdf_nbuf_data(nbuf);
		qdf_ret_status = QDF_STATUS_SUCCESS;
	}

	return qdf_ret_status;
}

#endif
/*---------------------------------------------------------------------------
* @brief cds_packet_free() -
      Free input network buffer
   ---------------------------------------------------------------------------*/
void cds_packet_free(void *pPacket)
{
	qdf_nbuf_free((qdf_nbuf_t) pPacket);
}
