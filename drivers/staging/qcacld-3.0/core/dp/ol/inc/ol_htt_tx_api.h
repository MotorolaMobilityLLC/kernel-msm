/*
 * Copyright (c) 2011-2019 The Linux Foundation. All rights reserved.
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

/**
 * @file ol_htt_tx_api.h
 * @brief Specify the tx HTT API functions called by the host data SW.
 * @details
 *  This file declares the HTT API functions that are specifically
 *  related to transmit processing.
 *  In particular, the methods of the abstract HTT tx descriptor are
 *  specified.
 */
#ifndef _OL_HTT_TX_API__H_
#define _OL_HTT_TX_API__H_

/* #include <osapi_linux.h>    / * uint16_t, etc. * / */
#include <osdep.h>              /* uint16_t, etc. */
#include <qdf_nbuf.h>           /* qdf_nbuf_t */
#include <ol_cfg.h>             /* wlan_frm_fmt */

#include <htt.h>                /* needed by inline functions */
#include <qdf_net_types.h>
#include <ol_htt_api.h>         /* htt_pdev_handle */
#include <htt_types.h>
#include <qdf_trace.h>
#include <cds_api.h>

#define HTT_INVALID_CHANNEL -1

/* Remove these macros when they get added to htt.h. */
#ifndef HTT_TX_DESC_EXTENSION_GET
#define HTT_TX_DESC_EXTENSION_OFFSET_DWORD 0
#define HTT_TX_DESC_EXTENSION_M        0x10000000
#define HTT_TX_DESC_EXTENSION_S        28

#define HTT_TX_DESC_EXTENSION_GET(_var) \
	(((_var) & HTT_TX_DESC_EXTENSION_M) >> HTT_TX_DESC_EXTENSION_S)
#define HTT_TX_DESC_EXTENSION_SET(_var, _val)				\
	do {								\
		HTT_CHECK_SET_VAL(HTT_TX_DESC_EXTENSION, _val);		\
		((_var) |= ((_val) << HTT_TX_DESC_EXTENSION_S));	\
	} while (0)
#endif

/*================ meta-info about tx MSDUs =================================*/

/*
 * For simplicity, use the IEEE 802.11 frame type values.
 */
enum htt_frm_type {
	htt_frm_type_mgmt = 0,
	htt_frm_type_ctrl = 1,
	htt_frm_type_data = 2
};

/*
 * For simplicity, use the IEEE 802.11 frame sub-type values.
 */
enum htt_frm_subtype {
	htt_frm_subtype_mgmt_assoc_req = 0,
	htt_frm_subtype_mgmt_assoc_resp = 1,
	htt_frm_subtype_mgmt_reassoc_req = 2,
	htt_frm_subtype_mgmt_reassoc_resp = 3,
	htt_frm_subtype_mgmt_probe_req = 4,
	htt_frm_subtype_mgmt_probe_resp = 5,
	htt_frm_subtype_mgmt_timing_adv = 6,
	htt_frm_subtype_mgmt_beacon = 8,
	htt_frm_subtype_mgmt_atim = 9,
	htt_frm_subtype_mgmt_disassoc = 10,
	htt_frm_subtype_mgmt_auth = 11,
	htt_frm_subtype_mgmt_deauth = 12,
	htt_frm_subtype_mgmt_action = 13,
	htt_frm_subtype_mgmt_action_no_ack = 14,

	htt_frm_subtype_data_data = 0,
	htt_frm_subtype_data_data_cf_ack = 1,
	htt_frm_subtype_data_data_cf_poll = 2,
	htt_frm_subtype_data_data_cf_ack_cf_poll = 3,
	htt_frm_subtype_data_null = 4,
	htt_frm_subtype_data_cf_ack = 5,
	htt_frm_subtype_data_cf_poll = 6,
	htt_frm_subtype_data_cf_ack_cf_poll = 7,
	htt_frm_subtype_data_QoS_data = 8,
	htt_frm_subtype_data_QoS_data_cf_ack = 9,
	htt_frm_subtype_data_QoS_data_cf_poll = 10,
	htt_frm_subtype_data_QoS_data_cf_ack_cf_poll = 11,
	htt_frm_subtype_data_QoS_null = 12,
	htt_frm_subtype_data_QoS_cf_poll = 14,
	htt_frm_subtype_data_QoS_cf_ack_cf_poll = 15,
};

enum htt_ofdm_datarate {		/* Value    MBPS    Modulation  Coding*/
	htt_ofdm_datarate_6_mbps = 0,	/* 0        6       BPSK        1/2   */
	htt_ofdm_datarate_9_mbps = 1,	/* 1        9       BPSK        3/4   */
	htt_ofdm_datarate_12_mbps = 2,	/* 2        12      QPSK        1/2   */
	htt_ofdm_datarate_18_mbps = 3,	/* 3        18      QPSK        3/4   */
	htt_ofdm_datarate_24_mbps = 4,	/* 4        24      16-QAM      1/2   */
	htt_ofdm_datarate_36_mbps = 5,	/* 5        36      16-QAM      3/4   */
	htt_ofdm_datarate_48_mbps = 6,	/* 6        48      64-QAM      1/2   */
	htt_ofdm_datarate_54_mbps = 7,	/* 7        54      64-QAM      3/4   */
	htt_ofdm_datarate_max = 7,
};

/**
 * struct ocb_tx_ctrl_hdr_t - TX control header
 * @version:		must be 1
 * @length:		length of this structure
 * @channel_freq:	channel on which to transmit the packet
 * @valid_pwr:		bit 0: if set, tx pwr spec is valid
 * @valid_datarate:	bit 1: if set, tx MCS mask spec is valid
 * @valid_retries:	bit 2: if set, tx retries spec is valid
 * @valid_chain_mask:	bit 3: if set, chain mask is valid
 * @valid_expire_tsf:	bit 4: if set, tx expire TSF spec is valid
 * @valid_tid:		bit 5: if set, TID is valid
 * @reserved0_15_6:	bits 15:6 - unused, set to 0x0
 * @all_flags:		union of all the flags
 * @expire_tsf_lo:	TX expiry time (TSF) LSBs
 * @expire_tsf_hi:	TX expiry time (TSF) MSBs
 * @pwr:		Specify what power the tx frame needs to be transmitted
 *			at. The power a signed (two's complement) value is in
 *			units of 0.5 dBm. The value needs to be appropriately
 *			sign-extended when extracting the value from the message
 *			and storing it in a variable that is larger than A_INT8.
 *			If the transmission uses multiple tx chains, this power
 *			spec is the total transmit power, assuming incoherent
 *			combination of per-chain power to produce the total
 *			power.
 * @datarate:		The desired modulation and coding scheme.
 *			VALUE    DATA RATE   MODULATION  CODING RATE
 *			@ 20 MHz
 *			(MBPS)
 *			0        6           BPSK        1/2
 *			1        9           BPSK        3/4
 *			2        12          QPSK        1/2
 *			3        18          QPSK        3/4
 *			4        24          16-QAM      1/2
 *			5        36          16-QAM      3/4
 *			6        48          64-QAM      1/2
 *			7        54          64-QAM      3/4
 * @retry_limit:	Specify the maximum number of transmissions, including
 *			the initial transmission, to attempt before giving up if
 *			no ack is received.
 *			If the tx rate is specified, then all retries shall use
 *			the same rate as the initial transmission.
 *			If no tx rate is specified, the target can choose
 *			whether to retain the original rate during the
 *			retransmissions, or to fall back to a more robust rate.
 * @chain_mask:		specify which chains to transmit from
 * @ext_tid:		Extended Traffic ID (0-15)
 * @reserved:		Ensure that the size of the structure is a multiple of
 *			4. Must be 0.
 *
 * When sending an OCB packet, the user application has
 * the option of including the following struct following an ethernet header
 * with the proto field set to 0x8151. This struct includes various TX
 * paramaters including the TX power and MCS.
 */
PREPACK struct ocb_tx_ctrl_hdr_t {
	uint16_t version;
	uint16_t length;
	uint16_t channel_freq;

	union {
		struct {
			uint16_t
			valid_pwr:1,
			valid_datarate:1,
			valid_retries:1,
			valid_chain_mask:1,
			valid_expire_tsf:1,
			valid_tid:1,
			reserved0_15_6:10;
		};
		uint16_t all_flags;
	};

	uint32_t expire_tsf_lo;
	uint32_t expire_tsf_hi;
	int8_t pwr;
	uint8_t datarate;
	uint8_t retry_limit;
	uint8_t chain_mask;
	uint8_t ext_tid;
	uint8_t reserved[3];
} POSTPACK;

/**
 * @brief tx MSDU meta-data that HTT may use to program the FW/HW tx descriptor
 */
struct htt_msdu_info_t {
	/* the info sub-struct specifies the characteristics of the MSDU */
	struct {
		uint16_t ethertype;
#define HTT_INVALID_PEER_ID 0xffff
		uint16_t peer_id;
		uint8_t vdev_id;
		uint8_t ext_tid;
		/*
		 * l2_hdr_type - L2 format (802.3, native WiFi 802.11,
		 * or raw 802.11)
		 * Based on attach-time configuration, the tx frames provided
		 * by the OS to the tx data SW are expected to be either
		 * 802.3 format or the "native WiFi" variant of 802.11 format.
		 * Internally, the driver may also inject tx frames into the tx
		 * datapath, and these frames may be either 802.3 format or
		 * 802.11 "raw" format, with no further 802.11 encapsulation
		 * needed.
		 * The tx frames are tagged with their frame format, so target
		 * FW/HW will know how to interpret the packet's encapsulation
		 * headers when doing tx classification, and what form of 802.11
		 * header encapsulation is needed, if any.
		 */
		uint8_t l2_hdr_type;    /* enum htt_pkt_type */
		/*
		 * frame_type - is the tx frame management or data?
		 * Just to avoid confusion, the enum values for this frame type
		 * field use the 802.11 frame type values, although it is
		 * unexpected for control frames to be sent through the host
		 * data path.
		 */
		uint8_t frame_type;     /* enum htt_frm_type */
		/*
		 * frame subtype - this field specifies the sub-type of
		 * management frames
		 * Just to avoid confusion, the enum values for this frame
		 * subtype field use the 802.11 management frame subtype values.
		 */
		uint8_t frame_subtype;  /* enum htt_frm_subtype */
		uint8_t is_unicast;

		/* dest_addr is not currently used.
		 * It could be used as an input to a Tx BD (Riva tx descriptor)
		 * signature computation.
		   uint8_t *dest_addr;
		 */

		uint8_t l3_hdr_offset;  /* wrt qdf_nbuf_data(msdu), in bytes */

		/* l4_hdr_offset is not currently used.
		 * It could be used to specify to a TCP/UDP checksum computation
		 * engine where the TCP/UDP header starts.
		 */
		/* uint8_t l4_hdr_offset; - wrt qdf_nbuf_data(msdu), in bytes */
	} info;
	/* the action sub-struct specifies how to process the MSDU */
	struct {
		/* mgmt frames: option to force 6 Mbps rate */
		uint8_t use_6mbps;
		uint8_t do_encrypt;
		uint8_t do_tx_complete;
		uint8_t tx_comp_req;

		/*
		 * cksum_offload - Specify whether checksum offload is
		 * enabled or not
		 * Target FW uses this flag to turn on HW checksumming
		 * 0x0 - No checksum offload
		 * 0x1 - L3 header checksum only
		 * 0x2 - L4 checksum only
		 * 0x3 - L3 header checksum + L4 checksum
		 */
		qdf_nbuf_tx_cksum_t cksum_offload;
	} action;
};

static inline void htt_msdu_info_dump(struct htt_msdu_info_t *msdu_info)
{
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "HTT MSDU info object (%pK)\n", msdu_info);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  ethertype: %#x\n", msdu_info->info.ethertype);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  peer_id: %d\n", msdu_info->info.peer_id);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  vdev_id: %d\n", msdu_info->info.vdev_id);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  ext_tid: %d\n", msdu_info->info.ext_tid);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  l2_hdr_type: %d\n", msdu_info->info.l2_hdr_type);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  frame_type: %d\n", msdu_info->info.frame_type);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  frame_subtype: %d\n", msdu_info->info.frame_subtype);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  is_unicast: %u\n", msdu_info->info.is_unicast);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  l3_hdr_offset: %u\n", msdu_info->info.l3_hdr_offset);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  use 6 Mbps: %d\n", msdu_info->action.use_6mbps);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  do_encrypt: %d\n", msdu_info->action.do_encrypt);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  do_tx_complete: %d\n", msdu_info->action.do_tx_complete);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  is_unicast: %u\n", msdu_info->info.is_unicast);
	QDF_TRACE(QDF_MODULE_ID_TXRX, QDF_TRACE_LEVEL_INFO_LOW,
		  "  is_unicast: %u\n", msdu_info->info.is_unicast);
}

/*================ tx completion message field access methods ===============*/

/**
 * @brief Look up the descriptor ID of the nth MSDU from a tx completion msg.
 * @details
 *  A tx completion message tells the host that the target is done
 *  transmitting a series of MSDUs.  The message uses a descriptor ID
 *  to identify each such MSDU.  This function/macro is used to
 *  find the ID of one such MSDU referenced by the tx completion message.
 *
 * @param iterator - tx completion message context provided by HTT to the
 *      tx completion message handler.  This abstract reference to the
 *      HTT tx completion message's payload allows the data SW's tx
 *      completion handler to not care about the format of the HTT
 *      tx completion message.
 * @param num - (zero-based) index to specify a single MSDU within the
 *      series of MSDUs referenced by the tx completion message
 * @return descriptor ID for the specified MSDU
 */
uint16_t htt_tx_compl_desc_id(void *iterator, int num);

/*========================= tx descriptor operations ========================*/

/**
 * @brief Allocate a HTT abstract tx descriptor.
 * @details
 *  Allocate a HTT abstract tx descriptor from a pool within "consistent"
 *  memory, which is accessible by HIF and/or MAC DMA as well as by the
 *  host CPU.
 *  It is expected that the tx datapath will allocate HTT tx descriptors
 *  and link them with datapath SW tx descriptors up front as the driver
 *  is loaded.  Thereafter, the link from datapath SW tx descriptor to
 *  HTT tx descriptor will be maintained until the driver is unloaded.
 *
 * @param htt_pdev - handle to the HTT instance making the allocation
 * @param[OUT] paddr_lo - physical address of the HTT descriptor
 * @return success -> descriptor handle, -OR- failure -> NULL
 */
void *htt_tx_desc_alloc(htt_pdev_handle pdev, qdf_dma_addr_t *paddr,
			uint16_t index);

/**
 * @brief Free a HTT abstract tx descriptor.
 *
 * @param htt_pdev - handle to the HTT instance that made the allocation
 * @param htt_tx_desc - the descriptor to free
 */
void htt_tx_desc_free(htt_pdev_handle htt_pdev, void *htt_tx_desc);

#if defined(HELIUMPLUS)
/**
 * @brief Allocate TX frag descriptor
 * @details
 *  Allocate TX frag descriptor
 *
 * @param pdev - handle to the HTT instance that made the allocation
 * @param index - tx descriptor index
 * @param frag_paddr_lo - fragment descriptor physical address lower 32bits
 * @param frag_ptr - fragment descriptor hlos pointe
 * @return success 0
 */
int htt_tx_frag_alloc(htt_pdev_handle pdev,
	u_int16_t index, qdf_dma_addr_t *frag_paddr, void **frag_ptr);
#else
static inline int htt_tx_frag_alloc(htt_pdev_handle pdev,
	u_int16_t index, qdf_dma_addr_t *frag_paddr, void **frag_ptr)
{
	*frag_ptr = NULL;
	return 0;
}
#endif /* defined(HELIUMPLUS) */

#if defined(CONFIG_HL_SUPPORT)

/**
 * @brief Discard all tx frames in the process of being downloaded.
 * @details
 * This function dicards any tx frames queued in HTT or the layers
 * under HTT.
 * The download completion callback is invoked on these frames.
 *
 * @param htt_pdev - handle to the HTT instance
 * @param[OUT] frag_paddr_lo - physical address of the fragment descriptor
 *                             (MSDU Link Extension Descriptor)
 */
static inline void htt_tx_pending_discard(htt_pdev_handle pdev)
{
}
#else

void htt_tx_pending_discard(htt_pdev_handle pdev);
#endif

/**
 * @brief Download a MSDU descriptor and (a portion of) the MSDU payload.
 * @details
 *  This function is used within LL systems to download a tx descriptor and
 *  the initial portion of the tx MSDU payload, and within HL systems to
 *  download the tx descriptor and the entire tx MSDU payload.
 *  The HTT layer determines internally how much of the tx descriptor
 *  actually needs to be downloaded. In particular, the HTT layer does not
 *  download the fragmentation descriptor, and only for the LL case downloads
 *  the physical address of the fragmentation descriptor.
 *  In HL systems, the tx descriptor and the entire frame are downloaded.
 *  In LL systems, only the tx descriptor and the header of the frame are
 *  downloaded.  To determine how much of the tx frame to download, this
 *  function assumes the tx frame is the default frame type, as specified
 *  by ol_cfg_frame_type.  "Raw" frames need to be transmitted through the
 *  alternate htt_tx_send_nonstd function.
 *  The tx descriptor has already been attached to the qdf_nbuf object during
 *  a preceding call to htt_tx_desc_init.
 *
 * @param htt_pdev - the handle of the physical device sending the tx data
 * @param msdu - the frame being transmitted
 * @param msdu_id - unique ID for the frame being transmitted
 * @return 0 -> success, -OR- 1 -> failure
 */
int
htt_tx_send_std(htt_pdev_handle htt_pdev, qdf_nbuf_t msdu, uint16_t msdu_id);

/**
 * @brief Download a Batch Of Tx MSDUs
 * @details
 *     Each MSDU already has the MSDU ID stored in the headroom of the
 *     netbuf data buffer, and has the HTT tx descriptor already attached
 *     as a prefix fragment to the netbuf.
 *
 * @param htt_pdev - the handle of the physical device sending the tx data
 * @param head_msdu - the MSDU Head for Tx batch being transmitted
 * @param num_msdus - The total Number of MSDU's provided for batch tx
 * @return null-terminated linked-list of unaccepted frames
 */
qdf_nbuf_t
htt_tx_send_batch(htt_pdev_handle htt_pdev,
		  qdf_nbuf_t head_msdu, int num_msdus);

/* The htt scheduler for queued packets in htt
 * htt when unable to send to HTC because of lack of resource
 * forms a nbuf queue which is flushed when tx completion event from
 * target is received
 */

void htt_tx_sched(htt_pdev_handle pdev);

/**
 * @brief Same as htt_tx_send_std, but can handle raw frames.
 */
int
htt_tx_send_nonstd(htt_pdev_handle htt_pdev,
		   qdf_nbuf_t msdu,
		   uint16_t msdu_id, enum htt_pkt_type pkt_type);

/**
 * htt_pkt_dl_len_get() Gets the HTT PKT download length.
 * @pdev: pointer to struct htt_pdev_t
 *
 * Return: size of HTT packet download length.
 */
int
htt_pkt_dl_len_get(struct htt_pdev_t *pdev);

/* Used to set classify bit in HTT desc.*/
#define HTT_TX_CLASSIFY_BIT_S	4

/**
 * enum htt_ce_tx_pkt_type - enum of packet types to be set in CE
 *			     descriptor
 * @tx_pkt_type_raw: Value set for RAW frames
 * @tx_pkt_type_native_wifi: Value set for NATIVE WIFI frames
 * @tx_pkt_type_eth2: Value set for Ethernet II frames (mostly default)
 * @tx_pkt_type_802_3: Value set for 802.3 / original ethernet frames
 * @tx_pkt_type_mgmt: Value set for MGMT frames over HTT
 *
 */
enum htt_ce_tx_pkt_type {
	tx_pkt_type_raw = 0,
	tx_pkt_type_native_wifi = 1,
	tx_pkt_type_eth2 = 2,
	tx_pkt_type_802_3 = 3,
	tx_pkt_type_mgmt = 4
};

/**
 * enum extension_header_type - extension header type
 * @EXT_HEADER_NOT_PRESENT: extension header not present
 * @OCB_MODE_EXT_HEADER: Extension header for OCB mode
 * @WISA_MODE_EXT_HEADER_6MBPS: WISA mode 6Mbps header
 * @WISA_MODE_EXT_HEADER_24MBPS: WISA mode 24Mbps header
 */
enum extension_header_type {
	EXT_HEADER_NOT_PRESENT,
	OCB_MODE_EXT_HEADER,
	WISA_MODE_EXT_HEADER_6MBPS,
	WISA_MODE_EXT_HEADER_24MBPS,
};

extern const uint32_t htt_to_ce_pkt_type[];

/**
 * Provide a constant to specify the offset of the HTT portion of the
 * HTT tx descriptor, to avoid having to export the descriptor definition.
 * The htt module checks internally that this exported offset is consistent
 * with the private tx descriptor definition.
 *
 * Similarly, export a definition of the HTT tx descriptor size, and then
 * check internally that this exported constant matches the private tx
 * descriptor definition.
 */
#define HTT_TX_DESC_VADDR_OFFSET 8

/**
 * htt_tx_desc_init() - Initialize the per packet HTT Tx descriptor
 * @pdev:		  The handle of the physical device sending the
 *			  tx data
 * @htt_tx_desc:	  Abstract handle to the tx descriptor
 * @htt_tx_desc_paddr_lo: Physical address of the HTT tx descriptor
 * @msdu_id:		  ID to tag the descriptor with.
 *			  The FW sends this ID back to host as a cookie
 *			  during Tx completion, which the host uses to
 *			  identify the MSDU.
 *			  This ID is an index into the OL Tx desc. array.
 * @msdu:		  The MSDU that is being prepared for transmission
 * @msdu_info:		  Tx MSDU meta-data
 * @tso_info:		  Storage for TSO meta-data
 * @ext_header_data:      extension header data
 * @type:                 extension header type
 *
 * This function initializes the HTT tx descriptor.
 * HTT Tx descriptor is a host-f/w interface structure, and meta-data
 * accompanying every packet downloaded to f/w via the HTT interface.
 *
 * Return QDF_STATUS_SUCCESS for success, otherwise error.
 */
QDF_STATUS
htt_tx_desc_init(htt_pdev_handle pdev,
		 void *htt_tx_desc,
		 qdf_dma_addr_t htt_tx_desc_paddr,
		 uint16_t msdu_id,
		 qdf_nbuf_t msdu, struct htt_msdu_info_t *msdu_info,
		 struct qdf_tso_info_t *tso_info,
		 void *ext_header_data,
		 enum extension_header_type type);

/**
 * @brief Set a flag to indicate that the MSDU in question was postponed.
 * @details
 *  In systems in which the host retains its tx frame until the target sends
 *  a tx completion, the target has the option of discarding it's copy of
 *  the tx descriptor (and frame, for HL) and sending a "postpone" message
 *  to the host, to inform the host that it must eventually download the
 *  tx descriptor (and frame, for HL).
 *  Before the host downloads the postponed tx desc/frame again, it will use
 *  this function to set a flag in the HTT tx descriptor indicating that this
 *  is a re-send of a postponed frame, rather than a new frame.  The target
 *  uses this flag to keep the correct order between re-sent and new tx frames.
 *  This function is relevant for LL systems.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param desc - abstract handle to the tx descriptor
 */
void htt_tx_desc_flag_postponed(htt_pdev_handle pdev, void *desc);

/**
 * @brief Set a flag to tell the target that more tx downloads are en route.
 * @details
 *  At times, particularly in response to a U-APSD trigger in a HL system, the
 *  host will download multiple tx descriptors (+ frames, in HL) in a batch.
 *  The host will use this function to set a "more" flag in the initial
 *  and interior frames of the batch, to tell the target that more tx frame
 *  downloads within the batch are imminent.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param desc - abstract handle to the tx descriptor
 */
void htt_tx_desc_flag_batch_more(htt_pdev_handle pdev, void *desc);

/**
 * @brief Specify the number of fragments in the fragmentation descriptor.
 * @details
 *  Specify the number of fragments within the MSDU, i.e. the number of
 *  elements within the fragmentation descriptor.
 *  For LL, this is used to terminate the list of fragments used by the
 *  HW's tx MAC DMA.
 *  For HL, this is used to terminate the list of fragments provided to
 *  HTC for download.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param desc - abstract handle to the tx descriptor
 * @param num_frags - the number of fragments comprising the MSDU
 */
static inline
void
htt_tx_desc_num_frags(htt_pdev_handle pdev, void *desc, uint32_t num_frags)
{
	/*
	 * Set the element after the valid frag elems to 0x0,
	 * to terminate the list of fragments.
	 */
#if defined(HELIUMPLUS)
	if (HTT_WIFI_IP(pdev, 2, 0)) {
		struct msdu_ext_frag_desc *fdesc;

		/** Skip TSO related 4 dwords WIFI2.0*/
		fdesc = (struct msdu_ext_frag_desc *)
			&(((struct msdu_ext_desc_t *)desc)->frags[0]);
		fdesc[num_frags].u.desc64 = 0;
	} else {
		/* This piece of code should never be executed on HELIUMPLUS */
		*((u_int32_t *)
		  (((char *) desc) + HTT_TX_DESC_LEN + num_frags * 8)) = 0;
	}
#else /* ! HELIUMPLUS */
	*((uint32_t *)
	  (((char *)desc) + HTT_TX_DESC_LEN + num_frags * 8)) = 0;
#endif /* HELIUMPLUS */
}

/* checksum offload flags for hw */
#define IPV4_CSUM_EN     0x00010000
#define UDP_IPV4_CSUM_EN 0x00020000
#define UDP_IPV6_CSUM_EN 0x00040000
#define TCP_IPV4_CSUM_EN 0x00080000
#define TCP_IPV6_CSUM_EN 0x00100000
#define PARTIAL_CSUM_EN  0x00200000

/**
 * @brief Specify the location and size of a fragment of a tx MSDU.
 * @details
 *  In LL systems, the tx MAC DMA needs to know how the MSDU is constructed
 *  from fragments.
 *  In LL and HL systems, the HIF's download DMA to the target (LL: tx desc
 *  + header of tx payload; HL: tx desc + entire tx payload) needs to know
 *  where to find the fragments to download.
 *  The tx data SW uses this function to specify the location and size of
 *  each of the MSDU's fragments.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param desc - abstract handle to the HTT tx descriptor
 * @param frag_num - which fragment is being specified (zero-based indexing)
 * @param frag_phys_addr - DMA/physical address of the fragment
 * @param frag_len - number of bytes within the fragment
 */
static inline
void
htt_tx_desc_frag(htt_pdev_handle pdev,
		 void *desc,
		 int frag_num, qdf_dma_addr_t frag_phys_addr, uint16_t frag_len)
{
	uint32_t *word32;
#if defined(HELIUMPLUS)
	uint64_t  *word64;

	if (HTT_WIFI_IP(pdev, 2, 0)) {
		word32 = (u_int32_t *)(desc);
		/* Initialize top 6 words of TSO flags per packet */
		*word32++ = 0;
		*word32++ = 0;
		*word32++ = 0;
		if (((struct txrx_pdev_cfg_t *)(pdev->ctrl_pdev))
		    ->ip_tcp_udp_checksum_offload)
			*word32 |= (IPV4_CSUM_EN | TCP_IPV4_CSUM_EN |
					TCP_IPV6_CSUM_EN | UDP_IPV4_CSUM_EN |
					UDP_IPV6_CSUM_EN);
		else
			*word32 = 0;
		word32++;
		*word32++ = 0;
		*word32++ = 0;

		qdf_assert_always(word32 == (uint32_t *)
				&(((struct msdu_ext_desc_t *)desc)->frags[0]));

		/* Each fragment consumes 2 DWORDS */
		word32 += (frag_num << 1);
		word64 = (uint64_t *)word32;
		*word64 = frag_phys_addr;
		/*
		 * The frag_phys address is 37 bits. So, the higher 16 bits will
		 * be for len
		 */
		word32++;
		*word32 &= 0x0000ffff;
		*word32 |= (frag_len << 16);
	} else {
		/* For Helium+, this block cannot exist */
		QDF_ASSERT(0);
	}
#else /* !defined(HELIUMPLUS) */
	{
		uint64_t u64  = (uint64_t)frag_phys_addr;
		uint32_t u32l = (u64 & 0xffffffff);
		uint32_t u32h = (uint32_t)((u64 >> 32) & 0x1f);
		uint64_t *word64;

		word32 = (uint32_t *) (((char *)desc) +
			 HTT_TX_DESC_LEN + frag_num * 8);
		word64 = (uint64_t *)word32;
		*word32 = u32l;
		word32++;
		*word32 = (u32h << 16) | frag_len;
	}
#endif /* defined(HELIUMPLUS) */
}

void htt_tx_desc_frags_table_set(htt_pdev_handle pdev,
				 void *desc,
				 qdf_dma_addr_t paddr,
				 qdf_dma_addr_t frag_desc_paddr,
				 int reset);

/**
 * @brief Specify the type and subtype of a tx frame.
 *
 * @param pdev - the handle of the physical device sending the tx data
 * @param type - format of the MSDU (802.3, native WiFi, raw, or mgmt)
 * @param sub_type - sub_type (relevant for raw frames)
 */
static inline
void
htt_tx_desc_type(htt_pdev_handle pdev,
		 void *htt_tx_desc, enum htt_pkt_type type, uint8_t sub_type)
{
	uint32_t *word0;

	word0 = (uint32_t *) htt_tx_desc;
	/* clear old values */
	*word0 &= ~(HTT_TX_DESC_PKT_TYPE_M | HTT_TX_DESC_PKT_SUBTYPE_M);
	/* write new values */
	HTT_TX_DESC_PKT_TYPE_SET(*word0, type);
	HTT_TX_DESC_PKT_SUBTYPE_SET(*word0, sub_type);
}

/***** TX MGMT DESC management APIs ****/

/* Number of mgmt descriptors in the pool */
#define HTT_MAX_NUM_MGMT_DESCS 32

/** htt_tx_mgmt_desc_pool_alloc
 * @description - allocates the memory for mgmt frame descriptors
 * @param  - htt pdev object
 * @param  - num of descriptors to be allocated in the pool
 */
void htt_tx_mgmt_desc_pool_alloc(struct htt_pdev_t *pdev, A_UINT32 num_elems);

/** htt_tx_mgmt_desc_alloc
 * @description - reserves a mgmt descriptor from the pool
 * @param  - htt pdev object
 * @param  - pointer to variable to hold the allocated desc id
 * @param  - pointer to the mamangement from UMAC
 * @return - pointer the allocated mgmt descriptor
 */
qdf_nbuf_t
htt_tx_mgmt_desc_alloc(struct htt_pdev_t *pdev, A_UINT32 *desc_id,
		       qdf_nbuf_t mgmt_frm);

/** htt_tx_mgmt_desc_free
 * @description - releases the management descriptor back to the pool
 * @param  - htt pdev object
 * @param  - descriptor ID
 */
void
htt_tx_mgmt_desc_free(struct htt_pdev_t *pdev, A_UINT8 desc_id,
		      A_UINT32 status);

/** htt_tx_mgmt_desc_pool_free
 * @description - releases all the resources allocated for mgmt desc pool
 * @param  - htt pdev object
 */
void htt_tx_mgmt_desc_pool_free(struct htt_pdev_t *pdev);

/**
 * @brief Provide a buffer to store a 802.11 header added by SW tx encap
 *
 * @param htt_tx_desc - which frame the 802.11 header is being added to
 * @param new_l2_hdr_size - how large the buffer needs to be
 */
#define htt_tx_desc_mpdu_header(htt_tx_desc, new_l2_hdr_size) /*NULL*/
/**
 * @brief How many tx credits would be consumed by the specified tx frame.
 *
 * @param msdu - the tx frame in question
 * @return number of credits used for this tx frame
 */
#define htt_tx_msdu_credit(msdu) 1      /* 1 credit per buffer */
#ifdef HTT_DBG
void htt_tx_desc_display(void *tx_desc);
#else
#define htt_tx_desc_display(tx_desc)
#endif

static inline void htt_tx_desc_set_peer_id(void *htt_tx_desc, uint16_t peer_id)
{
	uint16_t *peer_id_field_ptr;

	peer_id_field_ptr = (uint16_t *)
			    (htt_tx_desc +
			     HTT_TX_DESC_PEERID_DESC_PADDR_OFFSET_BYTES);

	*peer_id_field_ptr = peer_id;
}

static inline
void htt_tx_desc_set_chanfreq(void *htt_tx_desc, uint16_t chanfreq)
{
	uint16_t *chanfreq_field_ptr;

	/*
	 * The reason we dont use CHAN_FREQ_OFFSET_BYTES is because
	 * it uses DWORD as unit
	 *
	 * The reason we dont use the SET macro in htt.h is because
	 * htt_tx_desc is incomplete type
	 */
	chanfreq_field_ptr = (uint16_t *)
		(htt_tx_desc +
		 HTT_TX_DESC_PEERID_DESC_PADDR_OFFSET_BYTES
		 + sizeof(A_UINT16));

	*chanfreq_field_ptr = chanfreq;
}

#if defined(FEATURE_TSO) && defined(HELIUMPLUS)
void
htt_tx_desc_fill_tso_info(htt_pdev_handle pdev, void *desc,
	 struct qdf_tso_info_t *tso_info);
#else
#define htt_tx_desc_fill_tso_info(pdev, desc, tso_info)
#endif
#endif /* _OL_HTT_TX_API__H_ */
