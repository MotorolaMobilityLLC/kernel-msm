/*
 * Copyright (c) 2011-2020 The Linux Foundation. All rights reserved.
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
 * This file lim_scan_result_utils.cc contains the utility functions
 * LIM uses for maintaining and accessing scan results on STA.
 * Author:        Chandra Modumudi
 * Date:          02/13/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */

#include "lim_types.h"
#include "lim_utils.h"
#include "lim_ser_des_utils.h"
#include "lim_api.h"
#include "lim_ft_defs.h"
#include "lim_session.h"
#include "rrm_api.h"
#include "cds_utils.h"

/**
 * lim_collect_bss_description()
 *
 ***FUNCTION:
 * This function is called during scan upon receiving
 * Beacon/Probe Response frame to check if the received
 * frame matches scan criteria, collect BSS description
 * and add it to cached scan results.
 *
 ***LOGIC:
 *
 ***ASSUMPTIONS:
 * NA
 *
 ***NOTE:
 * NA
 *
 * @param  mac - Pointer to Global MAC structure
 * @param  pBPR - Pointer to parsed Beacon/Probe Response structure
 * @param  pRxPacketInfo  - Pointer to Received frame's BD
 * @param  fScanning - flag to indicate if it is during scan.
 * ---------------------------------------------
 *
 * @return None
 */
void
lim_collect_bss_description(struct mac_context *mac,
			    struct bss_description *pBssDescr,
			    tpSirProbeRespBeacon pBPR,
			    uint8_t *pRxPacketInfo, uint8_t fScanning)
{
	uint8_t *pBody;
	uint32_t ieLen = 0;
	tpSirMacMgmtHdr pHdr;
	uint32_t chan_freq;
	uint8_t rfBand = 0;

	pHdr = WMA_GET_RX_MAC_HEADER(pRxPacketInfo);

	if (SIR_MAC_B_PR_SSID_OFFSET > WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo)) {
		QDF_ASSERT(WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo) >=
			   SIR_MAC_B_PR_SSID_OFFSET);
		return;
	}
	ieLen =
		WMA_GET_RX_PAYLOAD_LEN(pRxPacketInfo) - SIR_MAC_B_PR_SSID_OFFSET;
	pBody = WMA_GET_RX_MPDU_DATA(pRxPacketInfo);
	rfBand = WMA_GET_RX_RFBAND(pRxPacketInfo);

	/**
	 * Length of BSS desription is without length of
	 * length itself and length of pointer that holds ieFields.
	 *
	 * struct bss_description
	 * +--------+---------------------------------+---------------+
	 * | length | other fields                    | pointer to IEs|
	 * +--------+---------------------------------+---------------+
	 *                                            ^
	 *                                            ieFields
	 */
	pBssDescr->length =
		(uint16_t)(offsetof(struct bss_description, ieFields[0]) -
			   sizeof(pBssDescr->length) + ieLen);

	/* Copy BSS Id */
	qdf_mem_copy((uint8_t *) &pBssDescr->bssId,
		     (uint8_t *) pHdr->bssId, sizeof(tSirMacAddr));

	/* Copy Timestamp, Beacon Interval and Capability Info */
	pBssDescr->scansystimensec = qdf_get_bootbased_boottime_ns();

	pBssDescr->timeStamp[0] = pBPR->timeStamp[0];
	pBssDescr->timeStamp[1] = pBPR->timeStamp[1];
	pBssDescr->beaconInterval = pBPR->beaconInterval;
	pBssDescr->capabilityInfo =
		lim_get_u16((uint8_t *) &pBPR->capabilityInfo);

	if (!pBssDescr->beaconInterval) {
		pe_warn("Beacon Interval is ZERO, making it to default 100 "
			   QDF_MAC_ADDR_FMT, QDF_MAC_ADDR_REF(pHdr->bssId));
		pBssDescr->beaconInterval = 100;
	}
	/*
	 * There is a narrow window after Channel Switch msg is sent to HAL and before the AGC is shut
	 * down and beacons/Probe Rsps can trickle in and we may report the incorrect channel in 5Ghz
	 * band, so not relying on the 'last Scanned Channel' stored in LIM.
	 * Instead use the value returned by RXP in BD. This the the same value which HAL programs into
	 * RXP before every channel switch.
	 * Right now there is a problem in 5Ghz, where we are receiving beacons from a channel different from
	 * the currently scanned channel. so incorrect channel is reported to CSR and association does not happen.
	 * So for now we keep on looking for the channel info in the beacon (DSParamSet IE OR HT Info IE), and only if it
	 * is not present in the beacon, we go for the channel info present in RXP.
	 * This fix will work for 5Ghz 11n devices, but for 11a devices, we have to rely on RXP routing flag to get the correct channel.
	 * So The problem of incorrect channel reporting in 5Ghz will still remain for 11a devices.
	 */
	chan_freq = lim_get_channel_from_beacon(mac, pBPR);
	pBssDescr->chan_freq = chan_freq;

	/* set the network type in bss description */
	pBssDescr->nwType =
		lim_get_nw_type(mac, chan_freq, SIR_MAC_MGMT_FRAME, pBPR);

	/* Copy RSSI & SINR from BD */
	pBssDescr->rssi = (int8_t) WMA_GET_RX_RSSI_NORMALIZED(pRxPacketInfo);
	pBssDescr->rssi_raw = (int8_t) WMA_GET_RX_RSSI_RAW(pRxPacketInfo);

	/* SINR no longer reported by HW */
	pBssDescr->sinr = 0;
	pe_debug(QDF_MAC_ADDR_FMT " rssi: normalized: %d, absolute: %d",
		QDF_MAC_ADDR_REF(pHdr->bssId), pBssDescr->rssi,
		pBssDescr->rssi_raw);

	pBssDescr->received_time = (uint64_t)qdf_mc_timer_get_system_time();
	pBssDescr->tsf_delta = WMA_GET_RX_TSF_DELTA(pRxPacketInfo);
	pBssDescr->seq_ctrl = pHdr->seqControl;

	pe_debug("Received %s from BSSID: "QDF_MAC_ADDR_FMT" tsf_delta = %u Seq Num: %x ssid:%.*s, rssi: %d",
		 pBssDescr->fProbeRsp ? "Probe Rsp" : "Beacon",
		 QDF_MAC_ADDR_REF(pHdr->bssId),
		 pBssDescr->tsf_delta, ((pHdr->seqControl.seqNumHi <<
		 HIGH_SEQ_NUM_OFFSET) | pHdr->seqControl.seqNumLo),
		 pBPR->ssId.length, pBPR->ssId.ssId, pBssDescr->rssi_raw);

	if (fScanning) {
		rrm_get_start_tsf(mac, pBssDescr->startTSF);
		pBssDescr->parentTSF = WMA_GET_RX_TIMESTAMP(pRxPacketInfo);
	}

	/* MobilityDomain */
	pBssDescr->mdie[0] = 0;
	pBssDescr->mdie[1] = 0;
	pBssDescr->mdie[2] = 0;
	pBssDescr->mdiePresent = false;
	/* If mdie is present in the probe resp we */
	/* fill it in the bss description */
	if (pBPR->mdiePresent) {
		pBssDescr->mdiePresent = true;
		pBssDescr->mdie[0] = pBPR->mdie[0];
		pBssDescr->mdie[1] = pBPR->mdie[1];
		pBssDescr->mdie[2] = pBPR->mdie[2];
	}

#ifdef FEATURE_WLAN_ESE
	pBssDescr->QBSSLoad_present = false;
	pBssDescr->QBSSLoad_avail = 0;
	if (pBPR->QBSSLoad.present) {
		pBssDescr->QBSSLoad_present = true;
		pBssDescr->QBSSLoad_avail = pBPR->QBSSLoad.avail;
	}
#endif
	/* Copy IE fields */
	qdf_mem_copy((uint8_t *) &pBssDescr->ieFields,
		     pBody + SIR_MAC_B_PR_SSID_OFFSET, ieLen);

	/*set channel number in beacon in case it is not present */
	pBPR->chan_freq = chan_freq;
	mac->lim.beacon_probe_rsp_cnt_per_scan++;

	return;
} /*** end lim_collect_bss_description() ***/
