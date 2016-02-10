/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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

/**
 * @file ol_htt_rx_api.h
 * @brief Specify the rx HTT API functions called by the host data SW.
 * @details
 *  This file declares the HTT API functions that are specifically
 *  related to receive processing.
 *  In particular, this file specifies methods of the abstract HTT rx
 *  descriptor, and functions to iterate though a series of rx descriptors
 *  and rx MSDU buffers.
 */
#ifndef _OL_HTT_RX_API__H_
#define _OL_HTT_RX_API__H_

//#include <osapi_linux.h>     /* u_int16_t, etc. */
#include <osdep.h>        /* u_int16_t, etc. */
#include <adf_nbuf.h>     /* adf_nbuf_t */
#include <adf_os_types.h> /* a_bool_t */

#include <htt.h>         /* HTT_RX_IND_MPDU_STATUS */
#include <ol_htt_api.h>  /* htt_pdev_handle */

#include <ieee80211_defines.h> /* ieee80211_rx_status */
#include <ol_vowext_dbg_defs.h>


/*================ constants and types used in the rx API ===================*/

#define HTT_RSSI_INVALID 0x7fff

/**
 * @brief RX stats header
 * @details
 *  When receiving an OCB packet, the RX stats is sent to the user application
 *  so that the user application can do processing based on the RX stats.
 *  This structure will be preceded by an ethernet header with
 *  the proto field set to 0x8152. This struct includes various RX
 *  paramaters including RSSI, data rate, and center frequency.
 */
PREPACK struct ocb_rx_stats_hdr_t {
    /* version - The version must be 1. */
    A_UINT16 version;
    /* length - The length of this structure */
    A_UINT16 length;
    /* channel_freq - The center frequency for the packet */
    A_UINT16 channel_freq;
    /* rssi_cmb - combined RSSI from all chains */
    A_INT16 rssi_cmb;
    /* rssi - rssi for chains 0 through 3 (for 20 MHz bandwidth) */
    A_INT16 rssi[4];
    /* tsf32 - timestamp in TSF units */
    A_UINT32 tsf32;
    /* timestamp_microsec - timestamp in microseconds */
    A_UINT32 timestamp_microsec;
    /* datarate - MCS index */
    A_UINT8 datarate;
    /* timestamp_submicrosec - submicrosecond portion of the timestamp */
    A_UINT8 timestamp_submicrosec;
    /* ext_tid - Extended TID */
    A_UINT8 ext_tid;
    /* Ensure the size of the structure is a multiple of 4 */
    A_UINT8 reserved;
};

/*================ rx indication message field access methods ===============*/

/**
 * @brief Check if a rx indication message has a rx reorder flush command.
 * @details
 *  Space is reserved in each rx indication message for a rx reorder flush
 *  command, to release specified MPDUs from the rx reorder holding array
 *  before processing the new MPDUs referenced by the rx indication message.
 *  This rx reorder flush command contains a flag to show whether the command
 *  is valid within a given rx indication message.
 *  This function checks the validity flag from the rx indication
 *  flush command IE within the rx indication message.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @return
 *      1 - the message's rx flush command is valid and should be processed
 *          before processing new rx MPDUs,
 *      -OR-
 *      0 - the message's rx flush command is invalid and should be ignored
 */
int
htt_rx_ind_flush(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg);

/**
 * @brief Return the sequence number starting the range of MPDUs to flush.
 * @details
 *  Read the fields of the rx indication message that identify the start
 *  and end of the range of MPDUs to flush from the rx reorder holding array
 *  and send on to subsequent stages of rx processing.
 *  These sequence numbers are the 6 LSBs of the 12-bit 802.11 sequence
 *  number.  These sequence numbers are masked with the block ack window size,
 *  rounded up to a power of two (minus one, to create a bitmask) to obtain
 *  the corresponding index into the rx reorder holding array.
 *  The series of MPDUs to flush includes the one specified by the start
 *  sequence number.
 *  The series of MPDUs to flush excludes the one specified by the end
 *  sequence number; the MPDUs up to but not including the end sequence number
 *  are to be flushed.
 *  These start and end seq num fields are only valid if the "flush valid"
 *  flag is set.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @param seq_num_start - (call-by-reference output) sequence number
 *      for the start of the range of MPDUs to flush
 * @param seq_num_end - (call-by-reference output) sequence number
 *      for the end of the range of MPDUs to flush
 */
void
htt_rx_ind_flush_seq_num_range(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    unsigned *seq_num_start,
    unsigned *seq_num_end);

/**
 * @brief Check if a rx indication message has a rx reorder release command.
 * @details
 *  Space is reserved in each rx indication message for a rx reorder release
 *  command, to release specified MPDUs from the rx reorder holding array
 *  after processing the new MPDUs referenced by the rx indication message.
 *  This rx reorder release command contains a flag to show whether the command
 *  is valid within a given rx indication message.
 *  This function checks the validity flag from the rx indication
 *  release command IE within the rx indication message.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @return
 *      1 - the message's rx release command is valid and should be processed
 *          after processing new rx MPDUs,
 *      -OR-
 *      0 - the message's rx release command is invalid and should be ignored
 */
int
htt_rx_ind_release(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg);

/**
 * @brief Return the sequence number starting the range of MPDUs to release.
 * @details
 *  Read the fields of the rx indication message that identify the start
 *  and end of the range of MPDUs to release from the rx reorder holding
 *  array and send on to subsequent stages of rx processing.
 *  These sequence numbers are the 6 LSBs of the 12-bit 802.11 sequence
 *  number.  These sequence numbers are masked with the block ack window size,
 *  rounded up to a power of two (minus one, to create a bitmask) to obtain
 *  the corresponding index into the rx reorder holding array.
 *  The series of MPDUs to release includes the one specified by the start
 *  sequence number.
 *  The series of MPDUs to release excludes the one specified by the end
 *  sequence number; the MPDUs up to but not including the end sequence number
 *  are to be released.
 *  These start and end seq num fields are only valid if the "release valid"
 *  flag is set.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @param seq_num_start - (call-by-reference output) sequence number
 *        for the start of the range of MPDUs to release
 * @param seq_num_end - (call-by-reference output) sequence number
 *        for the end of the range of MPDUs to release
 */
void
htt_rx_ind_release_seq_num_range(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    unsigned *seq_num_start,
    unsigned *seq_num_end);

/*
 * For now, the host HTT -> host data rx status enum
 * exactly matches the target HTT -> host HTT rx status enum;
 * no translation is required.
 * However, the host data SW should only use the htt_rx_status,
 * so that in the future a translation from target HTT rx status
 * to host HTT rx status can be added, if the need ever arises.
 */
enum htt_rx_status {
    htt_rx_status_unknown = HTT_RX_IND_MPDU_STATUS_UNKNOWN,
    htt_rx_status_ok = HTT_RX_IND_MPDU_STATUS_OK,
    htt_rx_status_err_fcs = HTT_RX_IND_MPDU_STATUS_ERR_FCS,
    htt_rx_status_err_dup = HTT_RX_IND_MPDU_STATUS_ERR_DUP,
    htt_rx_status_err_replay = HTT_RX_IND_MPDU_STATUS_ERR_REPLAY,
    htt_rx_status_err_inv_peer = HTT_RX_IND_MPDU_STATUS_ERR_INV_PEER,
    htt_rx_status_ctrl_mgmt_null = HTT_RX_IND_MPDU_STATUS_MGMT_CTRL,
    htt_rx_status_tkip_mic_err = HTT_RX_IND_MPDU_STATUS_TKIP_MIC_ERR,

    htt_rx_status_err_misc = HTT_RX_IND_MPDU_STATUS_ERR_MISC
};

/**
 * @brief Check the status MPDU range referenced by a rx indication message.
 * @details
 *  Check the status of a range of MPDUs referenced by a rx indication message.
 *  This status determines whether the MPDUs should be processed or discarded.
 *  If the status is OK, then the MPDUs within the range should be processed
 *  as usual.
 *  Otherwise (FCS error, duplicate error, replay error, unknown sender error,
 *  etc.) the MPDUs within the range should be discarded.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @param mpdu_range_num - which MPDU range within the rx ind msg to check,
 *        starting from 0
 * @param status - (call-by-reference output) MPDU status
 * @param mpdu_count - (call-by-reference output) count of MPDUs comprising
 *        the specified MPDU range
 */
void
htt_rx_ind_mpdu_range_info(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    int mpdu_range_num,
    enum htt_rx_status *status,
    int *mpdu_count);

int16_t
htt_rx_ind_rssi_dbm(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg);

int16_t
htt_rx_ind_rssi_dbm_chain(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg,
                          int8_t chain);

void
htt_rx_ind_legacy_rate(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg,
    uint8_t *legacy_rate, uint8_t *legacy_rate_sel);


void
htt_rx_ind_timestamp(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg,
    uint32_t *timestamp_microsec, uint8_t *timestamp_submicrosec);

uint32_t
htt_rx_ind_tsf32(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg);

uint8_t
htt_rx_ind_ext_tid(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg);


/*==================== rx MPDU descriptor access methods ====================*/

extern
bool (*htt_rx_mpdu_desc_retry)(
		htt_pdev_handle pdev, void *mpdu_desc);
/**
 * @brief Return a rx MPDU's sequence number.
 * @details
 *  This function returns the LSBs of the 802.11 sequence number for the
 *  provided rx MPDU descriptor.
 *  Depending on the system, 6-12 LSBs from the 802.11 sequence number are
 *  returned.  (Typically, either the 8 or 12 LSBs are returned.)
 *  This sequence number is masked with the block ack window size,
 *  rounded up to a power of two (minus one, to create a bitmask) to obtain
 *  the corresponding index into the rx reorder holding array.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return the LSBs of the sequence number for the MPDU
 */
extern a_uint16_t
(*htt_rx_mpdu_desc_seq_num)(htt_pdev_handle pdev, void *mpdu_desc);

/**
 * @brief Return a rx MPDU's rx reorder array index, based on sequence number.
 * @details
 *  This function returns a sequence-number based index into the rx
 *  reorder array for the specified MPDU.
 *  In some systems, this rx reorder array is simply the LSBs of the
 *  sequence number, or possibly even the full sequence number.
 *  To support such systems, the returned index has to be masked with
 *  the power-of-two array size before using the value to index the
 *  rx reorder array.
 *  In other systems, this rx reorder array index is
 *      (sequence number) % (block ack window size)
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return the rx reorder array index the MPDU goes into
 */
/* use sequence number (or LSBs thereof) as rx reorder array index */
#define htt_rx_mpdu_desc_reorder_idx htt_rx_mpdu_desc_seq_num

union htt_rx_pn_t {
    /* WEP: 24-bit PN */
    u_int32_t pn24;

    /* TKIP or CCMP: 48-bit PN */
    u_int64_t pn48;

    /* WAPI: 128-bit PN */
    u_int64_t pn128[2];
};

/**
 * @brief Find the packet number (PN) for a MPDU.
 * @details
 *  This function only applies when the rx PN check is configured to be
 *  performed in the host rather than the target, and on peers using a
 *  security type for which a PN check applies.
 *  The pn_len_bits argument is used to determine which element of the
 *  htt_rx_pn_t union to deposit the PN value read from the MPDU descriptor
 *  into.
 *  A 24-bit PN is deposited into pn->pn24.
 *  A 48-bit PN is deposited into pn->pn48.
 *  A 128-bit PN is deposited in little-endian order into pn->pn128.
 *  Specifically, bits 63:0 of the PN are copied into pn->pn128[0], while
 *  bits 127:64 of the PN are copied into pn->pn128[1].
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @param pn - the location to copy the packet number into
 * @param pn_len_bits - the PN size, in bits
 */
extern void (*htt_rx_mpdu_desc_pn)(
    htt_pdev_handle pdev,
    void *mpdu_desc,
    union htt_rx_pn_t *pn,
    int pn_len_bits);

extern
uint8_t (*htt_rx_mpdu_desc_tid)(
			htt_pdev_handle pdev, void *mpdu_desc);

/**
 * @brief Return the TSF timestamp indicating when a MPDU was received.
 * @details
 *  This function provides the timestamp indicating when the PPDU that
 *  the specified MPDU belongs to was received.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return 32 LSBs of TSF time at which the MPDU's PPDU was received
 */
u_int32_t
htt_rx_mpdu_desc_tsf32(
    htt_pdev_handle pdev,
    void *mpdu_desc);

/**
 * @brief Return the 802.11 header of the MPDU
 * @details
 *  This function provides a pointer to the start of the 802.11 header
 *  of the Rx MPDU
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return pointer to 802.11 header of the received MPDU
 */
char *
htt_rx_mpdu_wifi_hdr_retrieve(
    htt_pdev_handle pdev,
    void *mpdu_desc);

/**
 * @brief Return the RSSI provided in a rx descriptor.
 * @details
 *  Return the RSSI from a rx descriptor, converted to dBm units.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return RSSI in dBm, or HTT_INVALID_RSSI
 */
int16_t
htt_rx_mpdu_desc_rssi_dbm(htt_pdev_handle pdev, void *mpdu_desc);


/*==================== rx MSDU descriptor access methods ====================*/

/**
 * @brief Check if a MSDU completes a MPDU.
 * @details
 *  When A-MSDU aggregation is used, a single MPDU will consist of
 *  multiple MSDUs.  This function checks a MSDU's rx descriptor to
 *  see whether the MSDU is the final MSDU within a MPDU.
 *
 * @param pdev - the handle of the physical device the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - there are subsequent MSDUs within the A-MSDU / MPDU
 *      -OR-
 *      1 - this is the last MSDU within its MPDU
 */
extern a_bool_t (*htt_rx_msdu_desc_completes_mpdu)(
        htt_pdev_handle pdev, void *msdu_desc);

/**
 * @brief Check if a MSDU is first msdu of MPDU.
 * @details
 *  When A-MSDU aggregation is used, a single MPDU will consist of
 *  multiple MSDUs.  This function checks a MSDU's rx descriptor to
 *  see whether the MSDU is the first MSDU within a MPDU.
 *
 * @param pdev - the handle of the physical device the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - this is interior MSDU in the A-MSDU / MPDU
 *      -OR-
 *      1 - this is the first MSDU within its MPDU
 */
extern a_bool_t (*htt_rx_msdu_first_msdu_flag)(
        htt_pdev_handle pdev, void *msdu_desc);

/**
 * @brief Retrieve encrypt bit from a mpdu desc.
 * @details
 *  Fw will pass all the frame  to the host whether encrypted or not, and will
 *  indicate the encrypt flag in the desc, this function is to get the info and used
 *  to make a judge whether should make pn check, because non-encrypted frames
 *  always get the same pn number 0.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param mpdu_desc - the abstract descriptor for the MPDU in question
 * @return 0 - the frame was not encrypted
 *         1 - the frame was encrypted
 */
extern a_bool_t
(*htt_rx_mpdu_is_encrypted)(htt_pdev_handle pdev, void *mpdu_desc);

/**
 * @brief Indicate whether a rx desc has a WLAN unicast vs. mcast/bcast flag.
 * @details
 *  A flag indicating whether a MPDU was delivered over WLAN as unicast or
 *  multicast/broadcast may be only valid once per MPDU (LL), or within each
 *  rx descriptor for the MSDUs within the MPDU (HL).  (In practice, it is
 *  unlikely that A-MSDU aggregation will be used in HL, so typically HL will
 *  only have one MSDU per MPDU anyway.)
 *  This function indicates whether the specified rx descriptor contains
 *  a WLAN ucast vs. mcast/bcast flag.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - The rx descriptor does not contain a WLAN ucast vs. mcast flag.
 *      -OR-
 *      1 - The rx descriptor has a valid WLAN ucast vs. mcast flag.
 */
extern int (*htt_rx_msdu_has_wlan_mcast_flag)(
        htt_pdev_handle pdev, void *msdu_desc);

/**
 * @brief Indicate whether a MSDU was received as unicast or mcast/bcast
 * @details
 *  Indicate whether the MPDU that the specified MSDU belonged to was
 *  delivered over the WLAN as unicast, or as multicast/broadcast.
 *  This query can only be performed on rx descriptors for which
 *  htt_rx_msdu_has_wlan_mcast_flag is true.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - The MSDU was delivered over the WLAN as unicast.
 *      -OR-
 *      1 - The MSDU was delivered over the WLAN as broadcast or multicast.
 */
extern a_bool_t (*htt_rx_msdu_is_wlan_mcast)(
        htt_pdev_handle pdev, void *msdu_desc);

/**
 * @brief Indicate whether a MSDU was received as a fragmented frame
 * @details
 *  This query can only be performed on LL system.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - The MSDU was a non-fragmented frame.
 *      -OR-
 *      1 - The MSDU was fragmented frame.
 */
extern int (*htt_rx_msdu_is_frag)(
        htt_pdev_handle pdev, void *msdu_desc);

/**
 * @brief Indicate if a MSDU should be delivered to the OS shim or discarded.
 * @details
 *  Indicate whether a MSDU should be discarded or delivered to the OS shim.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - The MSDU should be delivered to the OS
 *      -OR-
 *      non-zero - The MSDU should not be delivered to the OS.
 *          If the "forward" flag is set, it should be forwarded to tx.
 *          Else, it should be discarded.
 */
int
htt_rx_msdu_discard(htt_pdev_handle pdev, void *msdu_desc);

/**
 * @brief Indicate whether a MSDU should be forwarded to tx.
 * @details
 *  Indicate whether a MSDU should be forwarded to tx, e.g. for intra-BSS
 *  STA-to-STA forwarding in an AP, or for multicast echo in an AP.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - The MSDU should not be forwarded
 *      -OR-
 *      non-zero - The MSDU should be forwarded.
 *          If the "discard" flag is set, then the original MSDU can be
 *          directly forwarded into the tx path.
 *          Else, a copy (clone?) of the rx MSDU needs to be created to
 *          send to the tx path.
 */
int
htt_rx_msdu_forward(htt_pdev_handle pdev, void *msdu_desc);

/**
 * @brief Indicate whether a MSDU's contents need to be inspected.
 * @details
 *  Indicate whether the host data SW needs to examine the contents of the
 *  received MSDU, and based on the packet type infer what special handling
 *  to provide for the MSDU.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @return
 *      0 - No inspection + special handling is required.
 *      -OR-
 *      non-zero - Inspect the MSDU contents to infer what special handling
 *          to apply to the MSDU.
 */
int
htt_rx_msdu_inspect(htt_pdev_handle pdev, void *msdu_desc);

/**
 * @brief Provide all action specifications for a rx MSDU
 * @details
 *  Provide all action specifications together.  This provides the same
 *  information in a single function call as would be provided by calling
 *  the functions htt_rx_msdu_discard, htt_rx_msdu_forward, and
 *  htt_rx_msdu_inspect.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @param[out] discard - 1: discard the MSDU, 0: deliver the MSDU to the OS
 * @param[out] forward - 1: forward the rx MSDU to tx, 0: no rx->tx forward
 * @param[out] inspect - 1: process according to MSDU contents, 0: no inspect
 */
void
htt_rx_msdu_actions(
    htt_pdev_handle pdev,
    void *msdu_desc,
    int *discard,
    int *forward,
    int *inspect);

/**
 * @brief Get the key id sent in IV of the frame
 * @details
 *  Provide the key index octet which is taken from IV.
 *  This is valid only for the first MSDU.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the abstract descriptor for the MSDU in question
 * @key_id - Key id octet
 * @return indication of whether key id access is successful
 *   A_TRUE - Success
 *   A_FALSE - if this is not first msdu
 */
extern a_bool_t
(*htt_rx_msdu_desc_key_id)(
    htt_pdev_handle pdev,
    void *mpdu_desc,
    u_int8_t *key_id);

extern a_bool_t
(*htt_rx_msdu_chan_info_present)(
	htt_pdev_handle pdev,
	void *mpdu_desc);

extern a_bool_t
(*htt_rx_msdu_center_freq)(
	htt_pdev_handle pdev,
	struct ol_txrx_peer_t *peer,
	void *mpdu_desc,
	uint16_t *primary_chan_center_freq_mhz,
	uint16_t *contig_chan1_center_freq_mhz,
	uint16_t *contig_chan2_center_freq_mhz,
	uint8_t *phy_mode);

/*====================== rx MSDU + descriptor delivery ======================*/

/**
 * @brief Return a linked-list of network buffer holding the next rx A-MSDU.
 * @details
 *  In some systems, the rx MSDUs are uploaded along with the rx
 *  indication message, while in other systems the rx MSDUs are uploaded
 *  out of band, via MAC DMA.
 *  This function provides an abstract way to obtain a linked-list of the
 *  next MSDUs, regardless of whether the MSDU was delivered in-band with
 *  the rx indication message, or out of band through MAC DMA.
 *  In a LL system, this function returns a linked list of the one or more
 *  MSDUs that together comprise an A-MSDU.
 *  In a HL system, this function returns a degenerate linked list consisting
 *  of a single MSDU (head_msdu == tail_msdu).
 *  This function also makes sure each MSDU's rx descriptor can be found
 *  through the MSDU's network buffer.
 *  In most systems, this is trivial - a single network buffer stores both
 *  the MSDU rx descriptor and the MSDU payload.
 *  In systems where the rx descriptor is in a separate buffer from the
 *  network buffer holding the MSDU payload, a pointer to the rx descriptor
 *  has to be stored in the network buffer.
 *  After this function call, the descriptor for a given MSDU can be
 *  obtained via the htt_rx_msdu_desc_retrieve function.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @param head_msdu - call-by-reference network buffer handle, which gets set
 *      in this function to point to the head MSDU of the A-MSDU
 * @param tail_msdu - call-by-reference network buffer handle, which gets set
 *      in this function to point to the tail MSDU of the A-MSDU, or the
 *      same MSDU that the head_msdu points to if only a single MSDU is
 *      delivered at a time.
 * @return indication of whether any MSDUs in the AMSDU use chaining:
 * 0 - no buffer chaining
 * 1 - buffers are chained
 */
extern int
(*htt_rx_amsdu_pop)(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    adf_nbuf_t *head_msdu,
    adf_nbuf_t *tail_msdu);

extern int
(*htt_rx_frag_pop)(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_ind_msg,
    adf_nbuf_t *head_msdu,
    adf_nbuf_t *tail_msdu);

/**
 * @brief Return a linked list of buffers holding one MSDU
 *  In some systems the buffers are delivered along with offload delivery
 *  indication message itself, while in other systems the buffers are uploaded
 *  out of band, via MAC DMA.
 * @details
 *  This function provides an abstract way to obtain a linked-list of the
 *  buffers corresponding to an msdu, regardless of whether the MSDU was
 *  delivered in-band with the rx indication message, or out of band through
 *  MAC DMA.
 *  In a LL system, this function returns a linked list of one or more
 *  buffers corresponding to an MSDU
 *  In a HL system , TODO
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param offload_deliver_msg - the nebuf containing the offload deliver message
 * @param head_msdu - call-by-reference network buffer handle, which gets set in this
 *      function to the head buffer of this MSDU
 * @param tail_msdu - call-by-reference network buffer handle, which gets set in this
 *      function to the tail buffer of this MSDU
 */
extern int
(*htt_rx_offload_msdu_pop)(
    htt_pdev_handle pdev,
    adf_nbuf_t offload_deliver_msg,
    int *vdev_id,
    int *peer_id,
    int *tid,
    u_int8_t *fw_desc,
    adf_nbuf_t *head_buf,
    adf_nbuf_t *tail_buf);

/**
 * @brief Return the rx descriptor for the next rx MPDU.
 * @details
 *  The rx MSDU descriptors may be uploaded as part of the rx indication
 *  message, or delivered separately out of band.
 *  This function provides an abstract way to obtain the next MPDU descriptor,
 *  regardless of whether the MPDU descriptors are delivered in-band with
 *  the rx indication message, or out of band.
 *  This is used to iterate through the series of MPDU descriptors referenced
 *  by a rx indication message.
 *  The htt_rx_amsdu_pop function should be called before this function
 *  (or at least before using the returned rx descriptor handle), so that
 *  the cache location for the rx descriptor will be flushed before the
 *  rx descriptor gets used.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param rx_ind_msg - the netbuf containing the rx indication message
 * @return next abstract rx descriptor from the series of MPDUs referenced
 *      by an rx ind msg
 */
extern void *
(*htt_rx_mpdu_desc_list_next)(htt_pdev_handle pdev, adf_nbuf_t rx_ind_msg);

/**
 * @brief Retrieve a previously-stored rx descriptor from a MSDU buffer.
 * @details
 *  The data SW will call the htt_rx_msdu_desc_link macro/function to
 *  link a MSDU's rx descriptor with the buffer holding the MSDU payload.
 *  This function retrieves the rx MSDU descriptor.
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu - the buffer containing the MSDU payload
 * @return the corresponding abstract rx MSDU descriptor
 */
extern void *
(*htt_rx_msdu_desc_retrieve)(htt_pdev_handle pdev, adf_nbuf_t msdu);

/**
 * @brief Free both an rx MSDU descriptor and the associated MSDU buffer.
 * @details
 *  Usually the WLAN driver does not free rx MSDU buffers, but needs to
 *  do so when an invalid frame (e.g. FCS error) was deposited into the
 *  queue of rx buffers.
 *  This function frees both the rx descriptor and the rx frame.
 *  On some systems, the rx descriptor and rx frame are stored in the
 *  same buffer, and thus one free suffices for both objects.
 *  On other systems, the rx descriptor and rx frame are stored
 *  separately, so distinct frees are internally needed.
 *  However, in either case, the rx descriptor has been associated with
 *  the MSDU buffer, and can be retrieved by htt_rx_msdu_desc_retrieve.
 *  Hence, it is only necessary to provide the MSDU buffer; the HTT SW
 *  internally finds the corresponding MSDU rx descriptor.
 *
 * @param htt_pdev - the HTT instance the rx data was received on
 * @param rx_msdu_desc - rx descriptor for the MSDU being freed
 * @param msdu - rx frame buffer for the MSDU being freed
 */
void
htt_rx_desc_frame_free(
    htt_pdev_handle htt_pdev,
    adf_nbuf_t msdu);

/**
 * @brief Look up and free the rx descriptor for a MSDU.
 * @details
 *  When the driver delivers rx frames to the OS, it first needs
 *  to free the associated rx descriptors.
 *  In some systems the rx descriptors are allocated in the same
 *  buffer as the rx frames, so this operation is a no-op.
 *  In other systems, the rx descriptors are stored separately
 *  from the rx frames, so the rx descriptor has to be freed.
 *  The descriptor is located from the MSDU buffer with the
 *  htt_rx_desc_frame_free macro/function.
 *
 * @param htt_pdev - the HTT instance the rx data was received on
 * @param msdu - rx frame buffer for the rx MSDU descriptor being freed
 */
void
htt_rx_msdu_desc_free(htt_pdev_handle htt_pdev, adf_nbuf_t msdu);

/**
 * @brief Add new MSDU buffers for the target to fill.
 * @details
 *  In some systems, the underlying upload mechanism (HIF) allocates new rx
 *  buffers itself.  In other systems, the underlying upload mechanism
 *  (MAC DMA) needs to be provided with new rx buffers.
 *  This function is used as an abstract method to indicate to the underlying
 *  data upload mechanism when it is an appropriate time to allocate new rx
 *  buffers.
 *  If the allocation is automatically handled, a la HIF, then this function
 *  call is ignored.
 *  If the allocation has to be done explicitly, a la MAC DMA, then this
 *  function provides the context and timing for such replenishment
 *  allocations.
 *
 * @param pdev - the HTT instance the rx data will be received on
 */
void
htt_rx_msdu_buff_replenish(htt_pdev_handle pdev);

/**
 * @brief Links list of MSDUs into an single MPDU. Updates RX stats
 * @details
 *  When HW MSDU splitting is turned on each MSDU in an AMSDU MPDU occupies
 *  a separate wbuf for delivery to the network stack. For delivery to the
 *  monitor mode interface they need to be restitched into an MPDU. This
 *  function does this. Also updates the RX status if the MPDU starts
 *  a new PPDU
 *
 * @param pdev - the HTT instance the rx data was received on
 * @param head_msdu - network buffer handle, which points to the first MSDU
 *      in the list. This is a NULL terminated list
 * @param rx_staus - pointer to the status associated with this MPDU.
 *      Updated only if there is a new PPDU and new status associated with it
 * @param clone_not_reqd - If set the MPDU linking destroys the passed in
 *      list, else operates on a cloned nbuf
 * @return network buffer handle to the MPDU
 */
adf_nbuf_t
htt_rx_restitch_mpdu_from_msdus(
    htt_pdev_handle pdev,
    adf_nbuf_t head_msdu,
    struct ieee80211_rx_status *rx_status,
    unsigned clone_not_reqd);

/**
 * @brief Return the sequence number of MPDUs to flush.
 * @param pdev - the HTT instance the rx data was received on
 * @param rx_frag_ind_msg - the netbuf containing the rx fragment indication message
 * @param seq_num_start - (call-by-reference output) sequence number
 *      for the start of the range of MPDUs to flush
 * @param seq_num_end - (call-by-reference output) sequence number
 *      for the end of the range of MPDUs to flush
 */
void
htt_rx_frag_ind_flush_seq_num_range(
    htt_pdev_handle pdev,
    adf_nbuf_t rx_frag_ind_msg,
    int *seq_num_start,
    int *seq_num_end);
/**
 * @brief Return the HL rx desc size
 * @param pdev - the HTT instance the rx data was received on
 * @param msdu_desc - the hl rx desc pointer
 *
 */
u_int16_t
htt_rx_msdu_rx_desc_size_hl(
        htt_pdev_handle pdev,
        void *msdu_desc);

/**
 * @brief populates vowext stats by processing RX desc.
 * @param msdu - network buffer handle
 * @param vowstats - handle to vow ext stats.
 */
void htt_rx_get_vowext_stats(adf_nbuf_t msdu,struct vow_extstats *vowstats);

/**
 * @brief parses the offload message passed by the target.
 * @param pdev - pdev handle
 * @param paddr - physical address of the rx buffer
 * @param vdev_id - reference to vdev id to be filled
 * @param peer_id - reference to the peer id to be filled
 * @param tid - reference to the tid to be filled
 * @param fw_desc - reference to the fw descriptor to be filled
 * @param peer_id - reference to the peer id to be filled
 * @param head_buf - reference to the head buffer
 * @param tail_buf - reference to the tail buffer
 */
int
htt_rx_offload_paddr_msdu_pop_ll(
    htt_pdev_handle pdev,
    u_int32_t * msg_word,
    int msdu_iter,
    int *vdev_id,
    int *peer_id,
    int *tid,
    u_int8_t *fw_desc,
    adf_nbuf_t *head_buf,
    adf_nbuf_t *tail_buf);
#endif /* _OL_HTT_RX_API__H_ */
