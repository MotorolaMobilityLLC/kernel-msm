/*
 * Copyright (c) 2014-2016, 2019 The Linux Foundation. All rights reserved.
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

#if !defined(__I_CDS_PACKET_H)
#define __I_CDS_PACKET_H

/**=========================================================================

   \file        i_cds_packet.h

   \brief       Connectivity driver services network packet APIs

   Network Protocol packet/buffer internal include file

   ========================================================================*/

/*--------------------------------------------------------------------------
   Include Files
   ------------------------------------------------------------------------*/
#include "qdf_types.h"
/**
 * Rx Packet Struct
 * Buffer for the packet received from WMA has pointers to 802.11
 * frame fields and additional information based on the type of frame.
 * @frequency: Frequency
 * @snr: Signal to noise ratio
 * @rssi: Received signal strength indicator, normalized to -96 dBm as
 *        normal noise floor by adding -96 to snr. All the configured
 *        thresholds in the driver assume that noise floor is -96 dBm.
 * @timestamp: System timestamp when frame was received. Set to jiffies.
 * @mpdu_hdr_ptr: Pointer to beginning of 802.11 MPDU
 * @mpdu_data_ptr: Pointer to beginning of payload
 * @mpdu_len: Length of 802.11 MPDU
 * @mpdu_hdr_len: Length of 802.11 MPDU header
 * @mpdu_data_len: Length of 802.11 MPDU payload
 * @offloadScanLearn: Bit set to 1 for beacons received during roaming scan
 * @roamCandidateInd: Bit set to 1 when roaming candidate is found by fw
 * @scan_src: Source of scan
 * @dpuFeedback: DPU feedback for frame
 * @session_id: PE session
 * @tsf_delta: Delta between tsf in frame and local value of tsf
 * @rssi_raw: rssi based on actual noise floor in hardware.
 */
typedef struct {
	uint32_t frequency;
	uint8_t snr;
	uint32_t rssi;
	uint32_t timestamp;
	uint8_t *mpdu_hdr_ptr;
	uint8_t *mpdu_data_ptr;
	uint32_t mpdu_len;
	uint32_t mpdu_hdr_len;
	uint32_t mpdu_data_len;
	uint8_t offloadScanLearn:1;
	uint8_t roamCandidateInd:1;
	uint8_t scan_src;
	uint8_t dpuFeedback;
	uint8_t session_id;
	uint32_t tsf_delta;
	uint32_t rssi_raw;
} t_packetmeta, *tp_packetmeta;

/* implementation specific cds packet type */
struct cds_pkt_t {
	/* Packet Meta Information */
	t_packetmeta pkt_meta;

	/* Pointer to Packet */
	void *pkt_buf;
};

#endif /* !defined( __I_CDS_PACKET_H ) */
