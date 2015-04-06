/*
 * Copyright (c) 2012 The Linux Foundation. All rights reserved.
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
* @file isoc_hw_desc.h
* @brief Define the Tx BD and Rx BD structs
*/
#ifndef _ISOC_HW_DESC__H_
#define _ISOC_HW_DESC__H_

#include <a_types.h>      /* A_UINT32 */

#include <adf_os_types.h> /* adf_os_print */
#include <adf_os_util.h>  /* adf_os_assert */


/***
    NOTE: For Pronto definition, only V1 is defined. For V2, it needs to add other fields
***/

/**
* @brief isoc_rx_bd_t - the format of the "RX BD" (rx buffer descriptor)
*/

typedef struct
{
    /* 0x00 */
#ifdef BIG_ENDIAN_HOST

    /** (Only used by the DPU)
    This routing flag indicates the WQ number to which the DPU will push the
    frame after it finished processing it. */
    A_UINT32 dpu_routing_flag:8;

    /** This is DPU sig inserted by RXP. Signature on RA's DPU descriptor */
    A_UINT32 dpu_signature:3;

    /** When set Sta is authenticated. SW needs to set bit
    addr2_auth_extract_enable in rxp_config2 register. Then RXP will use bit 3
    in DPU sig to say whether STA is authenticated or not. In this case only
    lower 2bits of DPU Sig is valid */
    A_UINT32 sta_authenticated:1;

    /** When set address2 is not valid */
    A_UINT32 addr2_invalid:1;

    /** When set it indicates TPE has sent the Beacon frame */
    A_UINT32 beacon_sent:1;

    /** This bit filled by rxp when set indicates if the current tsf is smaller
    than received tsf */
    A_UINT32 rx_tsf_later:1;

    /** These two fields are used by SW to carry the Rx Channel number and SCAN bit in RxBD*/
    A_UINT32 rx_channel:4;
    /** For WMI host, this bit means band 0 - 2.4Ghz 1 - 5GHz*/
    A_UINT32 band_5ghz:1;

    A_UINT32 reserved0:1;

    /** LLC Removed
    This bit is only used in Libra rsvd for Virgo1.0/Virgo2.0
    Filled by ADU when it is set LLC is removed from packet */
    A_UINT32 llc_removed:1;

    A_UINT32 uma_bypass:1;

    /** This bit is only available in Virgo2.0/libra it is reserved in Virgo1.0
    Robust Management frame. This bit indicates to DPU that the packet is a
    robust management frame which requires decryption(this bit is only valid for
    management unicast encrypted frames)
    1 - Needs decryption
    0 - No decryption required */
    A_UINT32 robust_mgmt:1;

    /**
    This bit is only in Virgo2.0/libra it is reserved in Virgo 1.0
    This 1-bit field indicates to DPU Unicast/BC/MC packet
    0 - Unicast packet
    1 - Broadcast/Multicast packet
    This bit is only valid when robust_mgmt bit is 1 */
    A_UINT32 not_unicast:1;

    /** This is the KEY ID extracted from WEP packets and is used for determine
    the RX Key Index to use in the DPU Descriptror.
    This field  is 2bits for virgo 1.0
    And 3 bits in virgo2.0 and Libra
    In virgo2.0/libra it is 3bits for the BC/MC packets */
    A_UINT32 rx_key_id:3;

    /**  (Only used by the DPU)
    No encryption/decryption
    0: No action
    1: DPU will not encrypt/decrypt the frame, and discard any encryption
    related settings in the PDU descriptor. */
    A_UINT32 dpu_no_encrypt:1;

    /**
    This is only available in libra/virgo2.0  it is reserved for virgo1.0
    This bit is filled by RXP and modified by ADU
    This bit indicates to ADU/UMA module that the packet requires 802.11n to
    802.3 frame translation. Once ADU/UMA is done with translation they
    overwrite it with 1'b0/1'b1 depending on how the translation resulted
    When used by ADU
    0 - No frame translation required
    1 - Frame Translation required
    When used by SW
    0 - Frame translation not done, MPDU header offset points to 802.11 header..
    1 - Frame translation done ;  hence MPDU header offset will point to a
    802.3 header */
    A_UINT32 frame_translate:1;

    /** (Only used by the DPU)
    BD Type
    00: 'Generic BD', as indicted above
    01: De-fragmentation format
    10-11: Reserved for future use. */
    A_UINT32 bd_type:2;

#else
    A_UINT32 bd_type:2;
    A_UINT32 frame_translate:1;
    A_UINT32 dpu_no_encrypt:1;
    A_UINT32 rx_key_id:3;
    A_UINT32 not_unicast:1;
    A_UINT32 robust_mgmt:1;
    A_UINT32 reserved1:1;
    A_UINT32 llc_removed:1;
    A_UINT32 reserved0:1;
    /** For WMI host, this bit means band 0 - 2.4Ghz 1 - 5GHz*/
    A_UINT32 band_5ghz:1;
    A_UINT32 rx_channel:4;
    A_UINT32 rx_tsf_later:1;
    A_UINT32 beacon_sent:1;
    A_UINT32 addr2_invalid:1;
    A_UINT32 sta_authenticated:1;
    A_UINT32 dpu_signature:3;
    A_UINT32 dpu_routing_flag:8;
#endif

    /* 0x04 */
#ifdef BIG_ENDIAN_HOST

    /** This is used for AMSDU this is the PDU index of the PDU which is the
    one before last PDU; for all non AMSDU frames, this field SHALL be 0.
    Used in ADU (for AMSDU deaggregation) */
    A_UINT32 amsdu_pdu_idx:16;

#ifdef QCA_ISOC_PRONTO
    A_UINT32 adu_feedback:7;
    //ToDO: Add meaning of this bit
    A_UINT32 dpu_magic_packet: 1;
#else
    A_UINT32 adu_feedback:8;
#endif //QCA_ISOC_PRONTO

    /** DPU feedback */
    A_UINT32 dpu_feedback:8;

#else
    A_UINT32 dpu_feedback:8;
#ifdef QCA_ISOC_PRONTO
    A_UINT32 dpu_magic_packet: 1;
    A_UINT32 adu_feedback:7;
#else
    A_UINT32 adu_feedback:8;
#endif //QCA_ISOC_PRONTO

    A_UINT32 amsdu_pdu_idx:16;
#endif

    /* 0x08 */
#ifdef BIG_ENDIAN_HOST

    /** In case PDUs are linked to the BD, this field indicates the index of
    the first PDU linked to the BD. When PDU count is zero, this field has an
    undefined value. */
    A_UINT32 head_pdu_idx:16;

    /** In case PDUs are linked to the BD, this field indicates the index of
    the last PDU. When PDU count is zero, this field has an undefined value.*/
    A_UINT32 tail_pdu_idx:16;

#else
    A_UINT32 tail_pdu_idx:16;
    A_UINT32 head_pdu_idx:16;
#endif

    /* 0x0c */
#ifdef BIG_ENDIAN_HOST

    /** The length (in number of bytes) of the MPDU header.
    Limitation: The MPDU header offset + MPDU header length can never go beyond
    the end of the first PDU */
    A_UINT32 mpdu_header_length:8;

    /** The start byte number of the MPDU header.
    The byte numbering is done in the BE format. Word 0x0, bits [31:24] has
    byte index 0. */
    A_UINT32 mpdu_header_offset:8;

    /** The start byte number of the MPDU data.
    The byte numbering is done in the BE format. Word 0x0, bits [31:24] has
    byte index 0. Note that this offset can point all the way into the first
    linked PDU.
    Limitation: MPDU DATA OFFSET can not point into the 2nd linked PDU */
    A_UINT32 mpdu_data_offset:9;

    /** The number of PDUs linked to the BD.
    This field should always indicate the correct amount. */
    A_UINT32 pdu_count:7;
#else

    A_UINT32 pdu_count:7;
    A_UINT32 mpdu_data_offset:9;
    A_UINT32 mpdu_header_offset:8;
    A_UINT32 mpdu_header_length:8;
#endif

    /* 0x10 */
#ifdef BIG_ENDIAN_HOST

    /** This is the length (in number of bytes) of the entire MPDU
    (header and data). Note that the length does not include FCS field. */
    A_UINT32 mpdu_length:16;

    A_UINT32 reserved3: 3;
    //ToDO: Add meaning of this bit
    A_UINT32 rx_dxe_priority_routing:1;

    /** Traffic Identifier
    Indicates the traffic class the frame belongs to. For non QoS frames,
    this field is set to zero. */
    A_UINT32 tid:4;

    /*
     * For the HW and FW, reserved4 is 6 bits long.
     * However, the host SW uses two of these bits as flags to remember what
     * to do with the rx frame.  Hence, for the SW, reserved4 is only 4 bits.
     */
    A_UINT32 sw_flag_forward:1; /* SW-only field, unused by FW+HW */
    A_UINT32 sw_flag_discard:1; /* SW-only field, unused by FW+HW */
    A_UINT32 reserved4:4;   /* SW perspective:    4 bits reserved */
    //A_UINT32 reserved4:6; /* FW+HW perspecitve: 6 bits reserved */
    A_UINT32 htt_t2h_msg:1;
    A_UINT32 flow_control:1;
#else
    A_UINT32 flow_control:1;
    A_UINT32 htt_t2h_msg:1;
    //A_UINT32 reserved4:6; /* FW+HW perspecitve: 6 bits reserved */
    A_UINT32 reserved4:4;   /* SW perspective:    4 bits reserved */
    A_UINT32 sw_flag_discard:1;
    A_UINT32 sw_flag_forward:1;
    A_UINT32 tid:4;
    A_UINT32 rx_dxe_priority_routing:1;
    A_UINT32 reserved3: 3;
    A_UINT32 mpdu_length:16;
#endif

    /* 0x14 */
#ifdef BIG_ENDIAN_HOST

    /** (Only used by the DPU)
    The DPU descriptor index is used to calculate where in memory the DPU can
    find the DPU descriptor related to this frame. The DPU calculates the
    address by multiplying this index with the DPU descriptor size and adding
    the DPU descriptors base address. The DPU descriptor contains information
    specifying the encryption and compression type and contains references to
    where encryption keys can be found. */
    A_UINT32 dpu_desc_idx:8;

    /** The result from the binary address search on the ADDR1 of the incoming
    frame. See chapter: RXP filter for encoding of this field. */
    A_UINT32 addr1_index:8;

    /** The result from the binary address search on the ADDR2 of the incoming
    frame. See chapter: RXP filter for encoding of this field. */
    A_UINT32 addr2_index:8;

    /** The result from the binary address search on the ADDR3 of the incoming
    frame. See chapter: RXP filter for encoding of this field. */
    A_UINT32 addr3_index:8;
#else
    A_UINT32 addr3_index:8;
    A_UINT32 addr2_index:8;
    A_UINT32 addr1_index:8;
    A_UINT32 dpu_desc_idx:8;
#endif

    /* 0x18 */
#ifdef BIG_ENDIAN_HOST

    /** Indicates Rate Index of packet received  */
    A_UINT32 rate_index:9;

    /** An overview of RXP status information related to receiving the frame.*/
    A_UINT32 rxp_flags_has_fcs_en:1;
    A_UINT32 rxp_flags_set_nav:1;
    A_UINT32 rxp_flags_clear_nav:1;
    A_UINT32 rxp_flags_rmf_keyid_vld:1;
    A_UINT32 rxp_flags_addr3_index_invalid:1;
    A_UINT32 rxp_flags_addr2_index_invalid:1;
    A_UINT32 rxp_flags_addr1_index_invalid:1;
    A_UINT32 rxp_flags_errored_rmf_frame:1;
    A_UINT32 rxp_flags_ampdu_flag:1;
    A_UINT32 rxp_flags_rmf_keyid:3;
    A_UINT32 rxp_flags_last_mpdu:1;
    A_UINT32 rxp_flags_first_mpdu:1;
    A_UINT32 rxp_flags_has_phy_cmd:1;
    A_UINT32 rxp_flags_has_phy_stats:1;
    A_UINT32 rxp_flags_has_dlm:1;
    A_UINT32 rxp_flags_byp_dlm_proc:1;
    A_UINT32 rxp_flags_byp_mpdu_proc:1;
    A_UINT32 rxp_flags_fail_filter:1;
    A_UINT32 rxp_flags_fail_max_ptklen:1;
    A_UINT32 rxp_flags_fcs_err:1;
    A_UINT32 rxp_flags_err:1;

#else

    A_UINT32 rxp_flags_err:1;
    A_UINT32 rxp_flags_fcs_err:1;
    A_UINT32 rxp_flags_fail_max_ptklen:1;
    A_UINT32 rxp_flags_fail_filter:1;
    A_UINT32 rxp_flags_byp_mpdu_proc:1;
    A_UINT32 rxp_flags_byp_dlm_proc:1;
    A_UINT32 rxp_flags_has_dlm:1;
    A_UINT32 rxp_flags_has_phy_stats:1;
    A_UINT32 rxp_flags_has_phy_cmd:1;
    A_UINT32 rxp_flags_first_mpdu:1;
    A_UINT32 rxp_flags_last_mpdu:1;
    A_UINT32 rxp_flags_rmf_keyid:3;
    A_UINT32 rxp_flags_ampdu_flag:1;
    A_UINT32 rxp_flags_errored_rmf_frame:1;
    A_UINT32 rxp_flags_addr1_index_invalid:1;
    A_UINT32 rxp_flags_addr2_index_invalid:1;
    A_UINT32 rxp_flags_addr3_index_invalid:1;
    A_UINT32 rxp_flags_rmf_keyid_vld:1;
    A_UINT32 rxp_flags_clear_nav:1;
    A_UINT32 rxp_flags_set_nav:1;
    A_UINT32 rxp_flags_has_fcs_en:1;

    A_UINT32 rate_index:9;

#endif
    /* 0x1c */
    /** The PHY can be programmed to put all the PHY STATS received from the
    PHY when receiving a frame in the BD.  */
#ifdef BIG_ENDIAN_HOST
    A_UINT32 rssi0:8;
    A_UINT32 rssi1:8;
    A_UINT32 rssi2:8;
    A_UINT32 rssi3:8;
#else
    A_UINT32 rssi3:8;
    A_UINT32 rssi2:8;
    A_UINT32 rssi1:8;
    A_UINT32 rssi0:8;
#endif

    /* 0x20 */
    A_UINT32 phy_stats1;                      /* PHY status word 1: snr */

    /* 0x24 */
    /** The value of the TSF[31:0] bits at the moment that the RXP start
    receiving a frame from the PHY RX. */
    A_UINT32 rx_timestamp;                /* Rx timestamp, microsecond based*/

    /* 0x28~0x38 */
    /** The bits from the PMI command as received from the PHY RX. */
    A_UINT32 pmi_cmd4to23[5];               /* PMI cmd rcvd from RxP */

    /* 0x3c */
    /** The bits from the PMI command as received from the PHY RX. */
#ifdef QCA_ISOC__PRONTO

#ifdef BIG_ENDIAN_HOST
    /** The bits from the PMI command as received from the PHY RX. */
    A_UINT32 pmi_cmd24to25:16;

    /* 16-bit CSU Checksum value for the fragmented receive frames */
    A_UINT32 csu_checksum:16;
#else
    A_UINT32 csu_checksum:16;
    A_UINT32 pmi_cmd24to25:16;
#endif

#else
    /** The bits from the PMI command as received from the PHY RX. */
#ifdef BIG_ENDIAN_HOST
    A_UINT32 pmi_cmd24to25:16;
    A_UINT32 pmi_cmd26to27:16;
#else
    A_UINT32 pmi_cmd26to27:16;
    A_UINT32 pmi_cmd24to25:16;
#endif

#endif // QCA_ISOC__PRONTO

    /* 0x40 */
#ifdef BIG_ENDIAN_HOST

    /** Gives commands to software upon which host will perform some commands.
    Please refer to following RPE document for description of all different
    values for this field. */
    A_UINT32 reorder_opcode:4; /* isoc_rx_opcode */

    A_UINT32 reserved6:12;

    /** Filled by RPE to Indicate to the host up to which slot the host needs
    to forward the packets to upper Mac layer. This field mostly used for AMDPU
    packets */
    A_UINT32 reorder_fwd_idx:6;

    /** Filled by RPE which indicates to the host which one of slots in the
    available 64 slots should the host Queue the packet. This field only
    applied to AMPDU packets. */
    A_UINT32 reorder_slot_idx:6;

    A_UINT32 reserved7: 2;
    //ToDo: Add meaning to the bits
    A_UINT32 out_of_order_forward: 1;
    A_UINT32 reorder_enable: 1;

#else

    A_UINT32 reorder_enable: 1;
    A_UINT32 out_of_order_forward: 1;
    A_UINT32 reserved7: 2;

    A_UINT32 reorder_slot_idx:6;
    A_UINT32 reorder_fwd_idx:6;
    A_UINT32 reserved6:12;
    A_UINT32 reorder_opcode:4;
#endif

    /* 0x44 */
#ifdef BIG_ENDIAN_HOST
    /** reserved8 from a hardware perspective.
    Used by SW to propogate frame type/subtype information */
    A_UINT32 frame_type_subtype:8;

    /** Filled RPE gives the current sequence number in bitmap */
    A_UINT32 current_pkt_seqno:12;

    /** Filled by RPE which gives the sequence number of next expected packet
    in bitmap */
    A_UINT32 expected_pkt_seqno:12;
#else
    A_UINT32 expected_pkt_seqno:12;
    A_UINT32 current_pkt_seqno:12;
    A_UINT32 frame_type_subtype:8;
#endif

    /* 0x48 */
#ifdef BIG_ENDIAN_HOST

    /** When set it is the AMSDU subframe */
    A_UINT32 amsdu:1;

    /** When set it is the First subframe of the AMSDU packet */
    A_UINT32 amsdu_first:1;

    /** When set it is the last subframe of the AMSDU packet */
    A_UINT32 amsdu_last:1;

    /** When set it indicates an Errored AMSDU packet */
    A_UINT32 amsdu_error:1;

    A_UINT32 reserved9:4;

    /** It gives the order in which the AMSDU packet is processed
    Basically this is a number which increments by one for every AMSDU frame
    received. Mainly for debugging purpose. */
    A_UINT32 process_order:4;

    /** It is the order of the subframe of AMSDU that is processed by ADU.
    This is reset to 0 when ADU deaggregates the first subframe from a new
    AMSDU and increments by 1 for every new subframe deaggregated within the
    AMSDU, after it reaches 4'hf it stops incrementing. That means host should
    not rely on this field as index for subframe queuing.  Theoretically there
    can be way more than 16 subframes in an AMSDU. This is only used for debug
    purpose, SW should use LSF and FSF bits to determine first and last
    subframes. */
    A_UINT32 amsdu_idx:4;

    /** Filled by ADU this is the total AMSDU size */
    A_UINT32 total_amsdu_size:16;
#else
    A_UINT32 total_amsdu_size:16;
    A_UINT32 amsdu_idx:4;
    A_UINT32 process_order:4;
    A_UINT32 reserved9:4;
    A_UINT32 amsdu_error:1;
    A_UINT32 amsdu_last:1;
    A_UINT32 amsdu_first:1;
    A_UINT32 amsdu:1;
#endif

} isoc_rx_bd_t;


/**
* @brief specify whether to process or defer rx MPDUs
* @details
*  The rx reorder opcode indicates which rx MPDUs should be deferred,
*  due to prior MPDUs that have not yet arrived, and which shoudl be
*  processed, due to having all prior MPDUs already received.
*  The possibilities are:
*    - There were no missing MPDUs, and a new in-order MPDU is received:
*      release the new MPDU
*    - A single missing MPDU is received:
*      release the queued old MPDUs that were waiting on this new MPDU,
*      and also release this new MPDU
*    - A MPDU that is not the initial missing MPDU is received:
*      store the new MPDU in the rx reordering queue until missing
*      prior MPDUs have been received
*    - A new MPDU shifts the block ack window, and one of the following...
*        - All old MPDUs are no longer covered by the shifted block ack
*          window.
*          Release all old MPDUs, and store the new MPDU.
*        - The missing MPDU that old MPDUs were waiting for is no longer
*          covered by the shifted block ack window.  Release all such
*          old MPDUs, and if the new MPDU is in-order, release it too.
*        - Some old MPDUs are still covered by the shifted block ack
*          window, and so is a missing MPDU preceding the old MPDUs.
*          Any old MPDUs that are no longer covered by the shifted block
*          ack window are released, as are any old MPDUs that don't have
*          any missing prior MPDU.  All remaing MPDUs, including the new
*          MPDU, are left queued in the rx reorder array.
*    - A block ack request control message causes the block ack window
*      to shift: release queued MPDUs that are no longer covered by the
*      shifted block ack window.
*/
typedef enum
{
    ISOC_RX_OPCODE_INVALID         = 0,

    /* QUEUECUR_FWDBUF
     * The new MPDU fills a hole (or is at the front of the block ack
     * window) - release any buffered MPDUs that were waiting for this
     * new MPDU (and then release this new MPDU too).
     */
    ISOC_RX_OPCODE_QUEUECUR_FWDBUF = 1,

    /* FWDBUF_FWDCUR
     * The new MPDU shifts the block ack window, causing all old MPDUs
     * to no longer be within the shifted block ack window.
     * Thus, all bufferend MPDUs are released.  Since the new MPDU is at
     * the front of the shifted block ack window, it too is released.
     */
    ISOC_RX_OPCODE_FWDBUF_FWDCUR   = 2,

    /* QUEUECUR
     * The new MPDU is waiting for missing prior MPDUs.
     */
    ISOC_RX_OPCODE_QUEUECUR        = 3,

    /* FWDBUF_QUEUECUR
     * The new MPDU results in the block ack window being shifted,
     * and the new MPDU's position within the new block ack window
     * potentially falls on top of an old MPDU's position within the
     * old block ack window.
     * The new MPDU is waiting for a missing prior MPDU that falls within
     * the new position of the block ack window.
     * Thus, first release all the MPDUs that are no longer covered by the
     * shifted block ack window, then store the new MPDU.
     * This is the same as FWDALL_QUEUECUR, except that in this case some
     * of the old MPDUs still are covered by the shifted block ack window
     * and have a missing MPDU that they are waiting for.
     */
    ISOC_RX_OPCODE_FWDBUF_QUEUECUR = 4,

    /* FWDBUF_DROPCUR
     * A block ack request control message shifts the block ack window,
     * causing some of the queued MPDUs to be no longer covered by the
     * shifted block ack window.
     * Release these MPDUs that are no longer within the block ack window,
     * and any MPDUs that are at the start of the new window, while retaining
     * MPDUs within the window that are preceded by a missing MPDU.
     */
    ISOC_RX_OPCODE_FWDBUF_DROPCUR  = 5,

    /* FWDALL_DROPCUR
     * A block ack request control message shifts the block ack window,
     * causing all of the queued MPDUs to be no longer covered by the
     * shifted block ack window.  Release all queued MPDUs.
     */
    ISOC_RX_OPCODE_FWDALL_DROPCUR  = 6,

    /* FWDALL_QUEUECUR
     * This is equivalent to FWDBUF_QUEUECUR, but all old MPDUs are released,
     * either becaues they all are no longer covered by the shifted block ack
     * window, or because the missing MPDU they were waiting for is no longer
     * covered by the shifted block ack window, so though (some of) the old
     * MPDUs are still covered by the shifted block ack window, they no longer
     * have a missing prior MPDU within the block ack window.
     */
    ISOC_RX_OPCODE_FWDALL_QUEUECUR = 7,

    ISOC_RX_OPCODE_TEARDOWN        = 8, /* not used? */

    ISOC_RX_OPCODE_DROPCUR         = 9, /* not used? */

    ISOC_RX_OPCODE_MAX
} isoc_rx_opcode;


/* bd_type defined */
enum {
    ISOC_BD_TYPE_GENERIC = 0,
    ISOC_BD_TYPE_DEFRAG = 1,
};

/* dpu_feedback defined */
enum {
    ISOC_DPU_FEEDBACK_MULTI_ERROR = 0,   /* DPU detected multiple errors. Should never occur. */
    ISOC_DPU_FEEDBACK_BAD_TAG,           /* Tag fields in the BD and associated DPU descriptor did not match. */
    ISOC_DPU_FEEDBACK_BAD_BD,            /*  At least one of the following conditions applied:
    * The BD type was not 0 (normal BD) in a TX packet.
    * The BD type was not either 0 or 1 (normal or defrag BD) in an RX packet.
    * The MPDU Length field was less than the MPDU Header Length field.
    * The MPDU header was not located entirely within the BD.
    * The MPDU Data Offset pointed past the end of the first PDU.
    */
    ISOC_DPU_FEEDBACK_BAD_TKIP_MIC,     /* The TKIP MIC of a received packet is incorrect. */
    ISOC_DPU_FEEDBACK_BAD_DECRYPT,      /* Decryption of an RX fragment has failed.This error occurs only if none of the following more specific conditions applied. */
    ISOC_DPU_FEEDBACK_ENVELOPE_ONLY,    /* The received protected fragment had exactly sufficient MPDU data for an empty cryptographic envelope of the selected encryption mode. */
    ISOC_DPU_FEEDBACK_ENVELOPE_PART,    /* The received protected fragment had less MPDU data than required for the cryptographic envelope of the selected encryption mode. */
    ISOC_DPU_FEEDBACK_ZERO_LENGTH,      /* The received fragment had no MPDU data at all. */
    ISOC_DPU_FEEDBACK_BAD_EXTIV,        /* The received AES or TKIP fragment did not have the EXTIV bit set in the IV field of the cryptographic envelope. */
    ISOC_DPU_FEEDBACK_BAD_KID,          /* The KID field extracted from the received fragment did not match that in the DPU descriptor (or the BD for WEP encryption modes). */
    ISOC_DPU_FEEDBACK_BAD_WEP_SEED,     /* The received TKIP fragment’s computed WEP Seed did not match that in the IV. */
    ISOC_DPU_FEEDBACK_UNPROTECTED,      /* The received packet was unprotected, but the associated DPU descriptor had encryption enabled. */
    ISOC_DPU_FEEDBACK_PROTECTED,        /* The received packet was protected, but the associated DPU descriptor did not have an encryption mode enabled. */
    ISOC_DPU_FEEDBACK_BAD_REPLAY,       /* The received packet failed replay count checking. */
    ISOC_DPU_FEEDBACK_DPU_STALL,        /* The DPU stalled with a watchdog timeout and the forced packet completion event occurred. */
    ISOC_DPU_FEEDBACK_WAPI_WAI_FRAME,   /* If the encryption mode is WAPI and the recived frame is WAI */
};

/**
* @brief isoc_tx_bd_t - the format of the "TX BD" (tx buffer descriptor)
*/
typedef struct
{
    /* byte offset 0x0 */
#ifdef BIG_ENDIAN_HOST
    /**
    * (Only used by the DPU) This routing flag indicates the WQ number to
    * which the DPU will push the frame after it finished processing it.
    */
    A_UINT32 dpu_routing_flag: 8;

    /**
    * DPU signature
    * The DPU signature is used by the Tx MAC HW for a sanity check
    * that the specified DPU index (i.e. security key ID) is valid.
    */
    A_UINT32 dpu_signature: 3;

    /** Reserved  */
    A_UINT32 reserved0:2;

    /** Set to '1' to terminate the current AMPDU session. Added based on the
    request for WiFi Display */
    A_UINT32 terminate_ampdu:1;

    /** Bssid index to indicate ADU to use which of the 4 default MAC address
    to use while 802.3 to 802.11 translation in case search in ADU UMA table
    fails. The default MAC address should be appropriately programmed in the
    uma_tx_default_wmacaddr_u(_1,_2,_3) and uma_tx_default_wmacaddr_l(_1,_2,_3)
     registers */
    A_UINT32 uma_bssid_idx:2;

    /** Set to 1 to enable uma filling the BD when FT is not enabled.
    Ignored when FT is enabled. */
    A_UINT32 uma_bd_enable:1;

    /** (Only used by the CSU)
    0: No action
    1: Host will indicate TCP/UPD header start location and provide pseudo header value in BD.
    */
    A_UINT32 csu_sw_mode:1;

    /** Enable/Disable CSU on TX direction.
    0: Disable Checksum Unit (CSU) for Transmit.
    1: Enable
    */
    A_UINT32 csu_tx_enable:1;

    /** Enable/Disable Transport layer Checksum in CSU
    0: Disable TCP UDP checksum generation for TX.
    1: Enable TCP UDP checksum generation for TX.
    */
    A_UINT32 csu_enable_tl_checksum:1;

    /** Enable/Disable IP layer Checksum in CSU
    0: Disable IPv4/IPv6 checksum generation for TX
    1: Enable  IPv4/IPv6 checksum generation for TX
    */
    A_UINT32 csu_enable_ip_checksum:1;

    /** Filled by CSU to indicate whether transport layer Checksum is generated by CSU or not
    0: TCP/UDP checksum is being generated for TX.
    1: TCP/UDP checksum is NOT being generated for TX.
     */
    A_UINT32 csu_tl_checksum_generated:1;

    /** Filled by CSU in error scenario
    1: No valid header found during parsing. Therefore no checksum was validated.
    0: Valid header found
    */
    A_UINT32 csu_no_valid_header:1;

    /**
    * Robust Management Frames
    * This bit indicates to DPU that the packet is a robust management
    * frame which requires encryption (this bit is only valid for
    * certain management frames)
    * 1 - Needs encryption
    * 0 - No encrytion required
    * It is only set when Privacy bit=1 AND type/subtype=Deauth, Action,
    * Disassoc. Otherwise it should always be 0.
    */
    A_UINT32 robust_mgmt: 1;

    /**
    * This 1-bit field indicates to DPU Unicast/BC/MC packet
    * 0 - Unicast packet
    * 1 - Broadcast/Multicast packet
    * This bit is valid only if RMF bit is set
    */
    A_UINT32 not_unicast: 1;

    A_UINT32 reserved1: 1;

    /**
    * This bit indicates TPE has to assert the TX complete interrupt.
    * 0 - no interrupt
    * 1 - generate interrupt */
    A_UINT32 tx_complete_intr: 1;

    A_UINT32 fw_tx_complete_intr: 1;

    /**
    * No encryption/decryption
    * 0: No action
    * 1: DPU will not encrypt/decrypt the frame, and discard any encryption
    * related settings in the PDU descriptor.
    * (Only used by the DPU)
    */
    A_UINT32 dpu_no_encrypt: 1;

    /**
    * This bit indicates to ADU/UMA module that the packet requires 802.11n
    * to 802.3 frame translation. When used by ADU
    * 0 - No frame translation required
    * 1 - Frame Translation required
    */
    A_UINT32 frame_translate: 1;

    /**
    * BD Type
    * 00: 'Generic BD', as indicted above
    * 01: De-fragmentation format
    * 10-11: Reserved for future use.
    */
    A_UINT32 bd_type: 2;
#else
    A_UINT32 bd_type:               2;
    A_UINT32 frame_translate:       1;
    A_UINT32 dpu_no_encrypt:        1;
    A_UINT32 fw_tx_complete_intr:   1;
    A_UINT32 tx_complete_intr:      1;
    A_UINT32 reserved1:             1;
    A_UINT32 not_unicast:           1;
    A_UINT32 robust_mgmt:           1;

    A_UINT32 csu_no_valid_header:1;
    A_UINT32 csu_tl_checksum_generated:1;
    A_UINT32 csu_enable_ip_checksum:1;
    A_UINT32 csu_enable_tl_checksum:1;
    A_UINT32 csu_tx_enable:1;
    A_UINT32 csu_sw_mode:1;
    A_UINT32 uma_bd_enable:1;
    A_UINT32 uma_bssid_idx:2;
    A_UINT32 terminate_ampdu:1;
    A_UINT32 reserved0:2;

    A_UINT32 dpu_signature:         3;
    A_UINT32 dpu_routing_flag:      8;
#endif

    /* byte offset 0x4 */
#ifdef BIG_ENDIAN_HOST
    A_UINT32 reserved2:    16; /* MUST BE 0 otherwise triggers BMU error*/
    A_UINT32 adu_feedback:  8;
    /* DPU feedback in Tx path.*/
    A_UINT32 dpu_feedback:  8;

#else
    A_UINT32 dpu_feedback:  8;
    A_UINT32 adu_feedback:  8;
    A_UINT32 reserved2:    16;
#endif

    /* byte offset 0x8 */
#ifdef BIG_ENDIAN_HOST
    /**
    * head PDU index
    * It is initially filled by DXE then if encryption is on,
    * then DPU will overwrite these fields.
    * In case PDUs are linked to the BD, this field indicates
    * the index of the first PDU linked to the BD.
    * When PDU count is zero, this field has an undefined value.
    */
    A_UINT32 head_pdu_idx: 16;

    /**
    * head PDU index
    * It is initially filled by DXE then if encryption is on,
    * then DPU will overwrite these fields.
    * In case PDUs are linked to the BD, this field indicates
    * the index of the last PDU.
    * When PDU count is zero, this field has an undefined value.
    */
    A_UINT32 tail_pdu_idx: 16;
#else
    A_UINT32 tail_pdu_idx: 16;
    A_UINT32 head_pdu_idx: 16;
#endif

    /* byte offset 0xc */
#ifdef BIG_ENDIAN_HOST
    /**
    * The length (in number of bytes) of the MPDU header.
    * Limitation: The MPDU header offset + MPDU header length
    * can never go beyond the end of the first PDU
    */
    A_UINT32 mpdu_header_length: 8;

    /**
    * The start byte number of the MPDU header.
    * The byte numbering is done in the BE format.
    * Word 0x0, bits [31:24] has byte index 0.
    */
    A_UINT32 mpdu_header_offset: 8;

    /**
    * The start byte number of the MPDU data.
    * The byte numbering is done in the BE format.
    * Word 0x0, bits [31:24] has byte index 0.
    * Note that this offset can point all the way into the
    * first linked PDU.
    * Limitation: MPDU DATA OFFSET can not point into the
    * 2nd linked PDU
    */
    A_UINT32 mpdu_data_offset: 9;

    /**
    * PDU count
    * It is initially filled by DXE then if encryption is on,
    * DPU will overwrite these fields.
    * The number of PDUs linked to the BD.
    * This field should always indicate the correct amount.
    */
    A_UINT32 pdu_count:          7;
#else
    A_UINT32 pdu_count:          7;
    A_UINT32 mpdu_data_offset:   9;
    A_UINT32 mpdu_header_offset: 8;
    A_UINT32 mpdu_header_length: 8;
#endif

    /* byte offset 0x10 */
#ifdef BIG_ENDIAN_HOST
    /**
    * This covers MPDU header length + MPDU data length.
    * This does not include FCS.
    * For single frame transmission, PSDU size is mpdu_length + 4.
    */
    A_UINT32 mpdu_length: 16;

    A_UINT32 reserved3: 2;

    /**
    * Sequence number insertion by DPU
    * 00: Leave sequence number as is, as filled by host
    * 01: DPU to insert non TID based sequence number
    *     (If not TID based, then how does DPU know what seq to fill?
    *     Is this the non-Qos/Mgmt sequence number?)
    * 10: DPU to insert a sequence number based on TID.
    * 11: Reserved
    */
    A_UINT32 bd_seq_num_src:2;

    /**
    * Traffic Identifier
    * Indicates the traffic class the frame belongs to.
    * For non QoS frames, this field is set to zero.
    */
    A_UINT32 tid: 4;

    A_UINT32 reserved4: 8;
#else
    A_UINT32 reserved4:       8;
    A_UINT32 tid:             4;
    A_UINT32 bd_seq_num_src:  2;
    A_UINT32 reserved3:       2;
    A_UINT32 mpdu_length:    16;
#endif

    /* byte offset 0x14 */
#ifdef BIG_ENDIAN_HOST
    /**
    * (Only used by the DPU)
    * The DPU descriptor index is used to calculate where in
    * memory the DPU can find the DPU descriptor related to this frame.
    * The DPU calculates the address by multiplying this index
    * with the DPU descriptor size and adding the DPU descriptor
    * array's base address.
    * The DPU descriptor contains information specifying the encryption
    * and compression type and contains references to where encryption
    * keys can be found.
    */
    A_UINT32 dpu_desc_idx: 8;

    /**
    * The STAid of the RA address, a.k.a. peer ID
    */
    A_UINT32 sta_index: 8;

    /**
    * A field passed on to TPE which influences the ACK policy
    * to be used for this frame
    * 00 - ack
    * 01,10,11 - No Ack
    */
    A_UINT32 ack_policy: 2;

    /**
    * Overwrite option for the transmit rate
    * 00: Use rate programmed in the TPE STA descriptor
    * 01: Use TPE BD rate 1
    * 10: Use TPE BD rate 2
    * 11: Delayed Use TPE BD rate 3
    */
    A_UINT32 bd_rate: 2;

    /**
    * Which HW tx queue the frame should go into
    */
    A_UINT32 queue_id: 5;

    A_UINT32 reserved5: 7;
#else
    A_UINT32 reserved5:    7;
    A_UINT32 queue_id:     5;
    A_UINT32 bd_rate:      2;
    A_UINT32 ack_policy:   2;
    A_UINT32 sta_index:    8;
    A_UINT32 dpu_desc_idx: 8;
#endif

    /* byte offset 0x18 */
    A_UINT32 tx_bd_signature;

    /* byte offset 0x1c */
    A_UINT32 reserved6;

    /* byte offset 0x20 */
    /* Timestamp filled by DXE. Timestamp for current transfer */
    A_UINT32 dxe_h2b_start_timestamp;

    /* byte offset 0x24 */
    /* Timestamp filled by DXE. Timestamp for previous transfer */
    A_UINT32 dxe_h2b_end_timestamp;

#ifdef QCA_ISOC_PRONTO

    /* byte offset 0x28 */
#ifdef BIG_ENDIAN_HOST
    /** 10 bit value to indicate the start of TCP UDP frame relative to
             * the first IP frame header */
    A_UINT32 csu_tcp_udp_start_offset:10;

    /** 16 bit pseudo header for TCP UDP used by CSU to generate TCP/UDP
     * frame checksum */
    A_UINT32 csu_pseudo_header_checksum:16;

    A_UINT32 reserved7:6;
#else
    A_UINT32 reserved7:6;
    A_UINT32 csu_pseudo_header_checksum:16;
    A_UINT32 csu_tcp_udp_start_offset:10;
#endif

#endif /*QCA_ISOC_PRONTO*/

} isoc_tx_bd_t;

/**
* @brief utility function to swap bytes within a series of u_int32_t words
* @details
*  This function swaps bytes 0 <-> 3 and bytes 1 <-> 2 within each 4-byte
*  word within a specified address range.
*  The address range is assumed to be aligned to a 4-byte boundary, and
*  to have a length that is a multiple of 4 bytes.
*  This function can be used for endianness correction - after swapping
*  the bytes within a 4-byte word, the native u_int32_t value will have
*  bitfields within the u_int32_t word in the expected positions.
*  For example, if a structure that is initially stored in big-endian
*  format needs to be interpreted in a little-endian processor:
*
*  original 32-bit word:
*                             bit number
*   31          24 23            16 15             8 7             0
*  |--------------+----------------+----------------+---------------|
*  |      C       |       B        |       A_hi     |      A_lo     |
*  |--------------+----------------+----------------+---------------|
*
*  stored in
*  big-endian format:
*  byte   contents
*       +---------+                    +---------+
*     0 |    C    |                    |   A_lo  |
*       +---------+                    +---------+
*     1 |    B    |    byte-swapped    |   A_hi  |
*       +---------+  ================> +---------+
*     2 |   A_hi  |                    |    B    |
*       +---------+                    +---------+
*     3 |   A_lo  |                    |    C    |
*       +---------+                    +---------+
*
* byte-swapped values read into a 32-bit word on a little-endian processor:
*                             bit number
*   31          24 23            16 15             8 7             0
*  |--------------+----------------+----------------+---------------|
*  |      C       |       B        |       A_hi     |      A_lo     |
*  |--------------+----------------+----------------+---------------|
*/
static inline void
    isoc_hw_bd_swap_bytes32(char *addr, int bytes)
{
    u_int32_t *p32 = (u_int32_t *) addr;
    int i, num_words32;

    /* confirm that the address range has the expected alignment */
    adf_os_assert((((unsigned) addr) & 0x3) == 0);
    /* confirm that the address range has the expected length quantum */
    adf_os_assert((bytes & 0x3) == 0);

    num_words32 = bytes >> 2;
    for (i = 0; i < num_words32; i++, p32++) {
        u_int32_t word = *p32;
        *p32 =
            ((word & 0x000000ff) << 24) | /* move byte 0 --> byte 3 */
            ((word & 0x0000ff00) <<  8) | /* move byte 1 --> byte 2 */
            ((word & 0x00ff0000) >>  8) | /* move byte 2 --> byte 1 */
            ((word & 0xff000000) >> 24);  /* move byte 3 --> byte 0 */
    }
}

static inline void
    isoc_tx_bd_dump(isoc_tx_bd_t *tx_bd)
{
    char *p;
    int i;

    adf_os_print("Tx BD (%p)\n", tx_bd);

    adf_os_print("structured view:\n");
    adf_os_print("  BD type: %d\n", tx_bd->bd_type);
    adf_os_print("  frame translate: %d\n", tx_bd->frame_translate);
    adf_os_print("  DPU no-encrypt: %d\n", tx_bd->dpu_no_encrypt);
    adf_os_print("  FW tx complete intr: %d\n", tx_bd->fw_tx_complete_intr);
    adf_os_print("  tx complete intr: %d\n", tx_bd->tx_complete_intr);
    adf_os_print("  not unicast: %d\n", tx_bd->not_unicast);
    adf_os_print("  robust mgmt: %d\n", tx_bd->robust_mgmt);
    adf_os_print("  DPU signature: %d\n", tx_bd->dpu_signature);
    adf_os_print("  DPU routing flag: %d\n", tx_bd->dpu_routing_flag);
    adf_os_print("  DPU feedback: %#x\n", tx_bd->dpu_feedback);
    adf_os_print("  ADU feedback: %#x\n", tx_bd->adu_feedback);
    adf_os_print("  tail PDU idx: %d\n", tx_bd->tail_pdu_idx);
    adf_os_print("  head PDU idx: %d\n", tx_bd->head_pdu_idx);
    adf_os_print("  PDU count: %d\n", tx_bd->pdu_count);
    adf_os_print("  MPDU data offset: %d\n", tx_bd->mpdu_data_offset);
    adf_os_print("  MPDU header offset: %d\n", tx_bd->mpdu_header_offset);
    adf_os_print("  MPDU header length: %d\n", tx_bd->mpdu_header_length);
    adf_os_print("  TID: %d\n", tx_bd->tid);
    adf_os_print("  BD seq num src: %d\n", tx_bd->bd_seq_num_src);
    adf_os_print("  MPDU length: %d\n", tx_bd->mpdu_length);
    adf_os_print("  queue ID: %d\n", tx_bd->queue_id);
    adf_os_print("  BD rate: %d\n", tx_bd->bd_rate);
    adf_os_print("  ack policy: %d\n", tx_bd->ack_policy);
    adf_os_print("  STA index: %d\n", tx_bd->sta_index);
    adf_os_print("  DPU desc idx: %d\n", tx_bd->dpu_desc_idx);
    adf_os_print("  Tx BD signature: %d\n", tx_bd->tx_bd_signature);
    adf_os_print("  DXE start timestamp: %d\n", tx_bd->dxe_h2b_start_timestamp);
    adf_os_print("  DXE end timestamp: %d\n", tx_bd->dxe_h2b_end_timestamp);

    adf_os_print("raw view:\n  ");
    p = (char *) tx_bd;
    for (i = 0; i < sizeof(*tx_bd); i++, p++) {
        adf_os_print("%#02x ", *p);
        if ((i+1) % 8 == 0) {
            adf_os_print("\n  ");
        }
    }
    adf_os_print("\n");
}

static inline void
    isoc_rx_bd_dump(isoc_rx_bd_t *rx_bd)
{
    char *p;
    int i;

    adf_os_print("Rx BD (%p)\n", rx_bd);

    adf_os_print("structured view:\n");
    adf_os_print("  BD type: %d\n", rx_bd->bd_type);
    adf_os_print("  frame translate: %d\n", rx_bd->frame_translate);
    adf_os_print("  DPU no-encrypt: %d\n", rx_bd->dpu_no_encrypt);
    adf_os_print("  not unicast: %d\n", rx_bd->not_unicast);
    adf_os_print("  robust mgmt: %d\n", rx_bd->robust_mgmt);
    adf_os_print("  LLC removed: %d\n", rx_bd->llc_removed);
    adf_os_print("  DPU signature: %d\n", rx_bd->dpu_signature);
    adf_os_print("  DPU routing flag: %d\n", rx_bd->dpu_routing_flag);
    adf_os_print("  addr1 index: %d\n", rx_bd->addr1_index);
    adf_os_print("  addr2 index invalid: %d\n", rx_bd->addr2_invalid);
    adf_os_print("  addr2 index: %d\n", rx_bd->addr2_index);
    adf_os_print("  addr3 index: %d\n", rx_bd->addr3_index);
    adf_os_print("  DPU feedback: %#x\n", rx_bd->dpu_feedback);
    adf_os_print("  ADU feedback: %#x\n", rx_bd->adu_feedback);
    adf_os_print("  tail PDU idx: %d\n", rx_bd->tail_pdu_idx);
    adf_os_print("  head PDU idx: %d\n", rx_bd->head_pdu_idx);
    adf_os_print("  PDU count: %d\n", rx_bd->pdu_count);
    adf_os_print("  MPDU data offset: %d\n", rx_bd->mpdu_data_offset);
    adf_os_print("  MPDU header offset: %d\n", rx_bd->mpdu_header_offset);
    adf_os_print("  MPDU header length: %d\n", rx_bd->mpdu_header_length);
    adf_os_print("  TID: %d\n", rx_bd->tid);
    adf_os_print("  MPDU length: %d\n", rx_bd->mpdu_length);
    adf_os_print("  DPU desc idx: %d\n", rx_bd->dpu_desc_idx);
    adf_os_print("  HTT T2H Msg: %d\n", rx_bd->htt_t2h_msg);
    adf_os_print("  Flow Control: %d\n", rx_bd->flow_control);
    adf_os_print("  Current Pkt Sequence No: %d\n", rx_bd->current_pkt_seqno);
    adf_os_print("  Expected Pkt Sequence No: %d\n", rx_bd->expected_pkt_seqno);
    adf_os_print("  frame subtype: %d\n", rx_bd->frame_type_subtype);
    adf_os_print("  RSSI0: %d\n", rx_bd->rssi0);
    adf_os_print("  reorder opcode: %d\n", rx_bd->reorder_opcode);
    adf_os_print("  reorder fwd index: %d\n", rx_bd->reorder_fwd_idx);
    adf_os_print("  reorder slot index: %d\n", rx_bd->reorder_slot_idx);
    adf_os_print("  AMSDU Size: %d\n", rx_bd->total_amsdu_size);
    adf_os_print("  AMSDU IDX: %d\n", rx_bd->amsdu_idx);
    adf_os_print("  AMSDU: %d\n", rx_bd->amsdu);
    adf_os_print("  AMSDU first subfrm: %d\n", rx_bd->amsdu_first);
    adf_os_print("  AMSDU last subfrm: %d\n", rx_bd->amsdu_last);
    adf_os_print("  AMSDU error: %d\n", rx_bd->amsdu_error);
    adf_os_print("  RX timestamp: %d\n", rx_bd->rx_timestamp);

    adf_os_print("raw view start:\n  ");
    p = (char *) rx_bd;
    for (i = 0; i < sizeof(*rx_bd); i++, p++) {
        adf_os_print("%#02x ", *p);
        if ((i+1) % 8 == 0) {
            adf_os_print("\n  ");
        }
    }
    adf_os_print("raw view end\n");
}

#endif /* _ISOC_HW_DESC__H_ */
