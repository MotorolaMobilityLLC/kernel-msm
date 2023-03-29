/*
 * Copyright (c) 2012-2019 The Linux Foundation. All rights reserved.
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

#ifndef _PKTLOG_FMT_H_
#define _PKTLOG_FMT_H_

#ifndef REMOVE_PKT_LOG

#define CUR_PKTLOG_VER          10010   /* Packet log version */
#define PKTLOG_MAGIC_NUM        7735225

#ifdef __linux__
#ifdef MULTI_IF_NAME
#define PKTLOG_PROC_DIR "ath_pktlog" MULTI_IF_NAME
#define WLANDEV_BASENAME "cld" MULTI_IF_NAME
#else
#define PKTLOG_PROC_DIR "ath_pktlog"
#define WLANDEV_BASENAME "cld"
#endif
#endif
#define PKTLOG_PROC_SYSTEM "system"
#ifdef WIN32
#pragma pack(push, pktlog_fmt, 1)
#define __ATTRIB_PACK
#elif defined(__EFI__)
#define __ATTRIB_PACK
#else
#ifndef __ATTRIB_PACK
#define __ATTRIB_PACK __attribute__ ((packed))
#endif
#endif
#include <a_types.h>
/*
 * Each packet log entry consists of the following fixed length header
 * followed by variable length log information determined by log_type
 */

struct ath_pktlog_hdr {
	uint16_t flags;
	uint16_t missed_cnt;
#ifdef HELIUMPLUS
	uint8_t log_type;
	uint8_t macId;
#else
	uint16_t log_type;
#endif
	uint16_t size;
	uint32_t timestamp;
#ifdef PKTLOG_HAS_SPECIFIC_DATA
	uint32_t type_specific_data;
#endif
} __ATTRIB_PACK;

#define ATH_PKTLOG_HDR_FLAGS_MASK 0xffff
#define ATH_PKTLOG_HDR_FLAGS_SHIFT 0
#define ATH_PKTLOG_HDR_FLAGS_OFFSET 0
#define ATH_PKTLOG_HDR_MISSED_CNT_MASK 0xffff0000
#define ATH_PKTLOG_HDR_MISSED_CNT_SHIFT 16
#define ATH_PKTLOG_HDR_MISSED_CNT_OFFSET 0
#ifdef HELIUMPLUS
#define ATH_PKTLOG_HDR_LOG_TYPE_MASK 0x00ff
#define ATH_PKTLOG_HDR_LOG_TYPE_SHIFT 0
#define ATH_PKTLOG_HDR_LOG_TYPE_OFFSET 1
#define ATH_PKTLOG_HDR_MAC_ID_MASK 0xff00
#define ATH_PKTLOG_HDR_MAC_ID_SHIFT 8
#define ATH_PKTLOG_HDR_MAC_ID_OFFSET 1
#else
#define ATH_PKTLOG_HDR_LOG_TYPE_MASK 0xffff
#define ATH_PKTLOG_HDR_LOG_TYPE_SHIFT 0
#define ATH_PKTLOG_HDR_LOG_TYPE_OFFSET 1
#endif

#define ATH_PKTLOG_HDR_SIZE_MASK 0xffff0000
#define ATH_PKTLOG_HDR_SIZE_SHIFT 16
#define ATH_PKTLOG_HDR_SIZE_OFFSET 1
#define ATH_PKTLOG_HDR_TIMESTAMP_OFFSET 2
#define ATH_PKTLOG_HDR_TYPE_SPECIFIC_DATA_OFFSET 3

/**
 * enum - Pktlog flag field details
 * packet origin [1:0]
 * 00 - Local
 * 01 - Remote
 * 10 - Unknown/Not applicable
 * 11 - Reserved
 * reserved [15:2]
 */
enum {
	PKTLOG_FLG_FRM_TYPE_LOCAL_S = 0,
	PKTLOG_FLG_FRM_TYPE_REMOTE_S,
	PKTLOG_FLG_FRM_TYPE_CLONE_S,
	PKTLOG_FLG_FRM_TYPE_CBF_S,
	PKTLOG_FLG_FRM_TYPE_UNKNOWN_S
};

#define PHFLAGS_INTERRUPT_CONTEXT 0x80000000

/* Masks for setting pktlog events filters */
#define ATH_PKTLOG_TX       0x000000001
#define ATH_PKTLOG_RX       0x000000002
#define ATH_PKTLOG_RCFIND   0x000000004
#define ATH_PKTLOG_RCUPDATE 0x000000008
#define ATH_PKTLOG_ANI      0x000000010
#define ATH_PKTLOG_TEXT     0x000000020
#define ATH_PKTLOG_PHYERR   0x000000040
#define ATH_PKTLOG_PROMISC  0x000000080
#define ATH_PKTLOG_SW_EVENT 0x000000100

/* WIN defns */
#define ATH_PKTLOG_H_INFO   0x000000200
#define ATH_PKTLOG_STEERING 0x000000400
#define ATH_PKTLOG_REMOTE_LOGGING_ENABLE  0x000000800
#define ATH_PKTLOG_TX_CAPTURE_ENABLE  0x000001000
#define ATH_PKTLOG_LITE_T2H  0x000002000
#define ATH_PKTLOG_LITE_RX   0x000004000

/* Types of packet log events */
#define PKTLOG_TYPE_TX_CTRL      1
#define PKTLOG_TYPE_TX_STAT      2
#define PKTLOG_TYPE_TX_MSDU_ID   3
#define PKTLOG_TYPE_TX_FRM_HDR   4
#define PKTLOG_TYPE_RX_STAT      5
#define PKTLOG_TYPE_RC_FIND      6
#define PKTLOG_TYPE_RC_UPDATE    7
#define PKTLOG_TYPE_TX_VIRT_ADDR 8
#define PKTLOG_TYPE_SMART_ANTENNA 9
#define PKTLOG_TYPE_SW_EVENT     10
#define PKTLOG_TYPE_PKT_DUMP     11
/* Command to process Monitor status ring (Rx) buffers */
#define PKTLOG_TYPE_RX_STATBUF   22
/* Command to process PPDU Tx buffers from CE */
#define PKTLOG_TYPE_LITE_T2H     23
/* Command to process PPDU Rx buffers from Monitor status ring */
#define PKTLOG_TYPE_LITE_RX      24
#define PKTLOG_TYPE_MAX          25

#define PKTLOG_MAX_TXCTL_WORDS 57       /* +2 words for bitmap */
#define PKTLOG_MAX_TXSTATUS_WORDS 32
#define PKTLOG_MAX_PROTO_WORDS  16
#define PKTLOG_MAX_RXDESC_WORDS 62
#define PKTLOG_HDR_SIZE_16      0x8000

struct txctl_frm_hdr {
	uint16_t framectrl;     /* frame control field from header */
	uint16_t seqctrl;       /* frame control field from header */
	uint16_t bssid_tail;    /* last two octets of bssid */
	uint16_t sa_tail;       /* last two octets of SA */
	uint16_t da_tail;       /* last two octets of DA */
	uint16_t resvd;
};

#if defined(HELIUMPLUS)
/* Peregrine 11ac based */
#define MAX_PKT_INFO_MSDU_ID 1
#else
/* Peregrine 11ac based */
#define MAX_PKT_INFO_MSDU_ID 192
#endif /* defined(HELIUMPLUS) */

/*
 * msdu_id_info_t is defined for reference only
 */
struct msdu_id_info {
	uint32_t num_msdu;
	uint8_t bound_bmap[(MAX_PKT_INFO_MSDU_ID + 7)>>3];
	/* TODO:
	*  Convert the id's to uint32_t
	*  Reduces computation in the driver code
	*/
	uint16_t id[MAX_PKT_INFO_MSDU_ID];
} __ATTRIB_PACK;
#define MSDU_ID_INFO_NUM_MSDU_OFFSET 0 /* char offset */
#define MSDU_ID_INFO_BOUND_BM_OFFSET offsetof(struct msdu_id_info, bound_bmap)
#define MSDU_ID_INFO_ID_OFFSET  offsetof(struct msdu_id_info, id)


struct ath_pktlog_txctl {
	struct ath_pktlog_hdr pl_hdr;
	/* struct txctl_frm_hdr frm_hdr; */
	void *txdesc_hdr_ctl;   /* frm_hdr + Tx descriptor words */
	struct {
		struct txctl_frm_hdr frm_hdr;
		uint32_t txdesc_ctl[PKTLOG_MAX_TXCTL_WORDS];
		/* uint32_t *proto_hdr;   / * protocol header (variable length!) * / */
		/* uint32_t *misc; / * Can be used for HT specific or other misc info * / */
	} priv;
} __ATTRIB_PACK;

struct ath_pktlog_tx_status {
	struct ath_pktlog_hdr pl_hdr;
	void *ds_status;
	int32_t misc[0];        /* Can be used for HT specific or other misc info */
} __ATTRIB_PACK;

struct ath_pktlog_msdu_info {
	struct ath_pktlog_hdr pl_hdr;
	void *ath_msdu_info;
	A_UINT32 num_msdu;
	struct {
		/*
		 * Provision to add more information fields
		 */
		struct msdu_info_t {
			A_UINT32 num_msdu;
			A_UINT8 bound_bmap[MAX_PKT_INFO_MSDU_ID >> 3];
		} msdu_id_info;
		/*
		 * array of num_msdu
		 * Static implementation will consume unwanted memory
		 * Need to split the pktlog_get_buf to get the buffer pointer only
		 */
		uint16_t msdu_len[MAX_PKT_INFO_MSDU_ID];
	} priv;
	size_t priv_size;

} __ATTRIB_PACK;

struct ath_pktlog_rx_info {
	struct ath_pktlog_hdr pl_hdr;
	void *rx_desc;
} __ATTRIB_PACK;

struct ath_pktlog_rc_find {
	struct ath_pktlog_hdr pl_hdr;
	void *rcFind;
} __ATTRIB_PACK;

struct ath_pktlog_sw_event {
	struct ath_pktlog_hdr pl_hdr;
	void *sw_event;
} __ATTRIB_PACK;

struct ath_pktlog_rc_update {
	struct ath_pktlog_hdr pl_hdr;
	void *txRateCtrl;       /* rate control state proper */
} __ATTRIB_PACK;

#ifdef WIN32
#pragma pack(pop, pktlog_fmt)
#endif
#ifdef __ATTRIB_PACK
#undef __ATTRIB_PACK
#endif /* __ATTRIB_PACK */

/*
 * The following header is included in the beginning of the file,
 * followed by log entries when the log buffer is read through procfs
 */

struct ath_pktlog_bufhdr {
	uint32_t magic_num;     /* Used by post processing scripts */
	uint32_t version;       /* Set to CUR_PKTLOG_VER */
};

struct ath_pktlog_buf {
	struct ath_pktlog_bufhdr bufhdr;
	int32_t rd_offset;
	volatile int32_t wr_offset;
	/* Whenever this bytes written value croses 4K bytes,
	 * logging will be triggered
	 */
	int32_t bytes_written;
	/* Index of the messages sent to userspace */
	uint32_t msg_index;
	/* Offset for read */
	loff_t offset;
	char log_data[0];
};

#define PKTLOG_MOV_RD_IDX(_rd_offset, _log_buf, _log_size)  \
	do { \
		if ((_rd_offset + sizeof(struct ath_pktlog_hdr) + \
		    ((struct ath_pktlog_hdr *)((_log_buf)->log_data + \
					       (_rd_offset)))->size) <= _log_size) { \
			_rd_offset = ((_rd_offset) + sizeof(struct ath_pktlog_hdr) + \
				      ((struct ath_pktlog_hdr *)((_log_buf)->log_data +	\
								 (_rd_offset)))->size);	\
		} else { \
			_rd_offset = ((struct ath_pktlog_hdr *)((_log_buf)->log_data +	\
								(_rd_offset)))->size;  \
		} \
		(_rd_offset) = (((_log_size) - (_rd_offset)) >=	\
				sizeof(struct ath_pktlog_hdr)) ? _rd_offset : 0; \
	} while (0)

#endif /* REMOVE_PKT_LOG */

/**
 * enum pkt_type - packet type
 * @START_MONITOR: indicates parser to start packetdump parsing
 * @STOP_MONITOR: indicates parser to stop packetdump parsing
 * @TX_MGMT_PKT: TX management Packet
 * @TX_DATA_PKT: TX data Packet
 * @RX_MGMT_PKT: RX management Packet
 * @RX_DATA_PKT: RX data Packet
 *
 * This enum has packet types
 */
enum pkt_type {
	START_MONITOR = 1,
	STOP_MONITOR,
	TX_MGMT_PKT,
	TX_DATA_PKT,
	RX_MGMT_PKT,
	RX_DATA_PKT,
};

/**
 * enum tx_pkt_fate - tx packet fate
 * @TX_PKT_FATE_ACKED: Sent over air and ACKed
 * @TX_PKT_FATE_SENT: Sent over air but not ACKed.
 * @TX_PKT_FATE_FW_QUEUED: Queued within firmware,
 * but not yet sent over air
 * @TX_PKT_FATE_FW_DROP_INVALID: Dropped by firmware as invalid.
 * E.g. bad source address, bad checksum, or invalid for current state.
 * @TX_PKT_FATE_FW_DROP_NOBUFS: Dropped by firmware due
 * to lack of buffer space
 * @TX_PKT_FATE_FW_DROP_OTHER: Dropped by firmware for any other
 * reason. Includes frames that were sent by driver to firmware, but
 * unaccounted for by firmware.
 * @TX_PKT_FATE_DRV_QUEUED: Queued within driver, not yet sent to firmware.
 * @TX_PKT_FATE_DRV_DROP_INVALID: Dropped by driver as invalid.
 * E.g. bad source address, or invalid for current state.
 * @TX_PKT_FATE_DRV_DROP_NOBUFS: Dropped by driver due to lack of buffer space
 * @TX_PKT_FATE_DRV_DROP_OTHER: Dropped by driver for any other reason.
 * E.g. out of buffers.
 *
 * This enum has packet fate types
 */

enum tx_pkt_fate {
	TX_PKT_FATE_ACKED,
	TX_PKT_FATE_SENT,
	TX_PKT_FATE_FW_QUEUED,
	TX_PKT_FATE_FW_DROP_INVALID,
	TX_PKT_FATE_FW_DROP_NOBUFS,
	TX_PKT_FATE_FW_DROP_OTHER,
	TX_PKT_FATE_DRV_QUEUED,
	TX_PKT_FATE_DRV_DROP_INVALID,
	TX_PKT_FATE_DRV_DROP_NOBUFS,
	TX_PKT_FATE_DRV_DROP_OTHER,
};

/**
 * enum rx_pkt_fate - rx packet fate
 * @RX_PKT_FATE_SUCCESS: Valid and delivered to
 * network stack (e.g., netif_rx()).
 * @RX_PKT_FATE_FW_QUEUED: Queued within firmware,
 * but not yet sent to driver.
 * @RX_PKT_FATE_FW_DROP_FILTER: Dropped by firmware
 * due to host-programmable filters.
 * @RX_PKT_FATE_FW_DROP_INVALID: Dropped by firmware
 * as invalid. E.g. bad checksum, decrypt failed, or invalid for current state.
 * @RX_PKT_FATE_FW_DROP_NOBUFS: Dropped by firmware
 * due to lack of buffer space.
 * @RX_PKT_FATE_FW_DROP_OTHER: Dropped by firmware
 * for any other reason.
 * @RX_PKT_FATE_DRV_QUEUED: Queued within driver,
 * not yet delivered to network stack.
 * @RX_PKT_FATE_DRV_DROP_FILTER: Dropped by driver
 * due to filter rules.
 * @RX_PKT_FATE_DRV_DROP_INVALID: Dropped by driver as invalid.
 * E.g. not permitted in current state.
 * @RX_PKT_FATE_DRV_DROP_NOBUFS: Dropped by driver
 * due to lack of buffer space.
 * @RX_PKT_FATE_DRV_DROP_OTHER: Dropped by driver for any other reason.
 *
 * This enum has packet fate types
 */

enum rx_pkt_fate {
	RX_PKT_FATE_SUCCESS,
	RX_PKT_FATE_FW_QUEUED,
	RX_PKT_FATE_FW_DROP_FILTER,
	RX_PKT_FATE_FW_DROP_INVALID,
	RX_PKT_FATE_FW_DROP_NOBUFS,
	RX_PKT_FATE_FW_DROP_OTHER,
	RX_PKT_FATE_DRV_QUEUED,
	RX_PKT_FATE_DRV_DROP_FILTER,
	RX_PKT_FATE_DRV_DROP_INVALID,
	RX_PKT_FATE_DRV_DROP_NOBUFS,
	RX_PKT_FATE_DRV_DROP_OTHER,
};

#endif /* _PKTLOG_FMT_H_ */
