/*
 * Copyright (c) 2011, 2014 The Linux Foundation. All rights reserved.
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

#ifndef _HTT_TYPES__H_
#define _HTT_TYPES__H_

#include <osdep.h>        /* u_int16_t, dma_addr_t */
#include <adf_os_types.h> /* adf_os_device_t */
#include <adf_os_lock.h>  /* adf_os_spinlock_t */
#include <adf_os_timer.h> /* adf_os_timer_t */
#include <adf_os_atomic.h>/* adf_os_atomic_inc */
#include <adf_nbuf.h>     /* adf_nbuf_t */
#include <htc_api.h>      /* HTC_PACKET */

#include <ol_ctrl_api.h>  /* ol_pdev_handle */
#include <ol_txrx_api.h>  /* ol_txrx_pdev_handle */

#define DEBUG_DMA_DONE

#define HTT_TX_MUTEX_TYPE adf_os_spinlock_t

#ifdef QCA_TX_HTT2_SUPPORT
#ifndef HTC_TX_HTT2_MAX_SIZE
/* Should sync to the target's implementation. */
#define HTC_TX_HTT2_MAX_SIZE    (120)
#endif
#endif /* QCA_TX_HTT2_SUPPORT */

#define HTT_INVALID_PEER    0xffff
#define HTT_INVALID_VDEV    0xff
#define HTT_NON_QOS_TID     16
#define HTT_INVALID_TID     31

#define HTT_TX_EXT_TID_DEFAULT              0
#define HTT_TX_EXT_TID_NON_QOS_MCAST_BCAST 16
#define HTT_TX_EXT_TID_MGMT                17
#define HTT_TX_EXT_TID_INVALID             31
#define HTT_TX_EXT_TID_NONPAUSE            19

#ifdef DEBUG_RX_RING_BUFFER
#define HTT_RX_RING_BUFF_DBG_LIST          1024
#endif

#define HTT_HTC_PKT_MISCLIST_SIZE           6
/**
 * @brief General specification of the tx frame contents
 *
 * @details
 * for efficiency, the HTT packet type values correspond
 * to the bit positions of the WAL packet type values, so the
 * translation is a simple shift operation.
 */
enum htt_pkt_type {
    htt_pkt_type_raw = 0,
    htt_pkt_type_native_wifi = 1,
    htt_pkt_type_ethernet = 2,
    htt_pkt_type_mgmt = 3,

    /* keep this last */
    htt_pkt_num_types
};

struct htt_pdev_t;

struct htt_htc_pkt {
    void *pdev_ctxt;
    dma_addr_t nbuf_paddr;
    HTC_PACKET htc_pkt;
    u_int16_t  msdu_id;
};

struct htt_htc_pkt_union {
    union {
        struct htt_htc_pkt pkt;
        struct htt_htc_pkt_union *next;
    } u;
};

/*
 * HTT host descriptor:
 * Include the htt_tx_msdu_desc that gets downloaded to the target,
 * but also include the HTC_FRAME_HDR and alignment padding that
 * precede the htt_tx_msdu_desc.
 * HTCSendDataPkt expects this header space at the front of the
 * initial fragment (i.e. tx descriptor) that is downloaded.
 */
struct htt_host_tx_desc_t {
    u_int8_t htc_header[HTC_HEADER_LEN];
    /* force the tx_desc field to begin on a 4-byte boundary */
    union {
        u_int32_t dummy_force_align;
        struct htt_tx_msdu_desc_t tx_desc;
    } align32;
};

struct htt_tx_mgmt_desc_buf {
    adf_nbuf_t   msg_buf;
    A_BOOL       is_inuse;
    adf_nbuf_t   mgmt_frm;
};

struct htt_tx_mgmt_desc_ctxt {
    struct htt_tx_mgmt_desc_buf *pool;
    A_UINT32    pending_cnt;
};

typedef struct _htt_list_node {
    struct _htt_list_node * prev;
    struct _htt_list_node * next;
} htt_list_node;

typedef htt_list_node htt_list_head;

struct htt_rx_hash_entry {
    A_UINT32    paddr;
    adf_nbuf_t  netbuf;
    A_UINT8     fromlist;
    htt_list_node listnode;
#ifdef RX_HASH_DEBUG
    A_UINT32    cookie;
#endif
};

struct htt_rx_hash_bucket {
    htt_list_head listhead;
    struct htt_rx_hash_entry * entries;
    htt_list_head freepool;
#ifdef RX_HASH_DEBUG
    A_UINT32 count;
#endif
};

#ifdef IPA_UC_OFFLOAD

/* IPA micro controller
   wlan host driver
   firmware shared memory structure */
struct uc_shared_mem_t
{
   u_int32_t *vaddr;
   adf_os_dma_addr_t paddr;
   adf_os_dma_mem_context(memctx);
};

/* Micro controller datapath offload
 * WLAN TX resources */
struct htt_ipa_uc_tx_resource_t
{
   struct uc_shared_mem_t tx_ce_idx;
   struct uc_shared_mem_t tx_comp_base;

   u_int32_t           tx_comp_idx_paddr;
   adf_nbuf_t         *tx_buf_pool_vaddr_strg;
   u_int32_t           alloc_tx_buf_cnt;
};

/* Micro controller datapath offload
 * WLAN RX resources */
struct htt_ipa_uc_rx_resource_t
{
   adf_os_dma_addr_t  rx_rdy_idx_paddr;
   struct uc_shared_mem_t rx_ind_ring_base;
   struct uc_shared_mem_t rx_ipa_prc_done_idx;
   u_int32_t          rx_ind_ring_size;
};

struct ipa_uc_rx_ring_elem_t
{
   u_int32_t  rx_packet_paddr;
   u_int16_t  vdev_id;
   u_int16_t  rx_packet_leng;
};
#endif /* IPA_UC_OFFLOAD */

struct htt_tx_credit_t
{
    adf_os_atomic_t bus_delta;
    adf_os_atomic_t target_delta;
};

#ifdef DEBUG_RX_RING_BUFFER
struct rx_buf_debug {
        uint32_t               paddr;
        void *                 vaddr;
        bool                   in_use;
};
#endif

struct htt_tx_desc_page_t
{
	char* page_v_addr_start;
	char* page_v_addr_end;
	adf_os_dma_addr_t page_p_addr;
};

struct htt_pdev_t {
    ol_pdev_handle ctrl_pdev;
    ol_txrx_pdev_handle txrx_pdev;
    HTC_HANDLE htc_pdev;
    adf_os_device_t osdev;

    HTC_ENDPOINT_ID htc_endpoint;

#ifdef QCA_TX_HTT2_SUPPORT
    HTC_ENDPOINT_ID htc_tx_htt2_endpoint;
    u_int16_t htc_tx_htt2_max_size;
#endif /* QCA_TX_HTT2_SUPPORT */

#ifdef ATH_11AC_TXCOMPACT
    HTT_TX_MUTEX_TYPE		txnbufq_mutex;
    adf_nbuf_queue_t		txnbufq;
    struct htt_htc_pkt_union *htt_htc_pkt_misclist;
#endif

    struct htt_htc_pkt_union *htt_htc_pkt_freelist;
    struct {
        int is_high_latency;
        int is_full_reorder_offload;
        int default_tx_comp_req;
    } cfg;
    struct {
        u_int8_t major;
        u_int8_t minor;
    } tgt_ver;
    struct {
        struct {
           /*
            * Ring of network buffer objects -
            * This ring is used exclusively by the host SW.
            * This ring mirrors the dev_addrs_ring that is shared
            * between the host SW and the MAC HW.
            * The host SW uses this netbufs ring to locate the network
            * buffer objects whose data buffers the HW has filled.
            */
           adf_nbuf_t *netbufs_ring;
           /*
            * Ring of buffer addresses -
            * This ring holds the "physical" device address of the
            * rx buffers the host SW provides for the MAC HW to fill.
            */
           u_int32_t *paddrs_ring;
           adf_os_dma_mem_context(memctx);
        } buf;
        /*
         * Base address of ring, as a "physical" device address rather than a
         * CPU address.
         */
        u_int32_t base_paddr;
        int size;           /* how many elems in the ring (power of 2) */
        unsigned size_mask; /* size - 1 */

        int fill_level; /* how many rx buffers to keep in the ring */
        int fill_cnt;   /* how many rx buffers (full+empty) are in the ring */

        /*
         * target_idx -
         * Without reorder offload:
         * not used
         * With reorder offload:
         * points to the location in the rx ring from which rx buffers are
         * available to copy into the MAC DMA ring
         */
        struct {
            u_int32_t *vaddr;
            u_int32_t paddr;
            adf_os_dma_mem_context(memctx);
        } target_idx;

        /*
         * alloc_idx/host_idx -
         * Without reorder offload:
         * where HTT SW has deposited empty buffers
         * This is allocated in consistent mem, so that the FW can read
         * this variable, and program the HW's FW_IDX reg with the value
         * of this shadow register
         * With reorder offload:
         * points to the end of the available free rx buffers
         */
        struct {
            u_int32_t *vaddr;
            u_int32_t paddr;
            adf_os_dma_mem_context(memctx);
        } alloc_idx;

        /* sw_rd_idx - where HTT SW has processed bufs filled by rx MAC DMA */
        struct {
            unsigned msdu_desc;
            unsigned msdu_payld;
        } sw_rd_idx;

        /*
         * refill_retry_timer - timer triggered when the ring is not
         * refilled to the level expected
         */
        adf_os_timer_t refill_retry_timer;

        /*
         * refill_ref_cnt - ref cnt for Rx buffer replenishment - this
         * variable is used to guarantee that only one thread tries
         * to replenish Rx ring.
         */
        adf_os_atomic_t refill_ref_cnt;
#ifdef DEBUG_DMA_DONE
        u_int32_t dbg_initial_msdu_payld;
        u_int32_t dbg_mpdu_range;
        u_int32_t dbg_mpdu_count;
        u_int32_t dbg_ring_idx;
        u_int32_t dbg_refill_cnt;
        u_int32_t dbg_sync_success;
#endif
#ifdef HTT_RX_RESTORE
        int rx_reset;
        u_int8_t htt_rx_restore;
#endif
        struct htt_rx_hash_bucket **hash_table;
        u_int32_t listnode_offset;
    } rx_ring;
    int rx_desc_size_hl;
    long rx_fw_desc_offset;
    int rx_mpdu_range_offset_words;
    int rx_ind_msdu_byte_idx;

    struct {
        int size; /* of each HTT tx desc */
        int pool_elems;
        int alloc_cnt;
        char *pool_vaddr;
        u_int32_t pool_paddr;
        u_int32_t *freelist;
        adf_os_dma_mem_context(memctx);
    } tx_descs;
    int download_len;
    void (*tx_send_complete_part2)(
        void *pdev, A_STATUS status, adf_nbuf_t msdu, u_int16_t msdu_id);

    HTT_TX_MUTEX_TYPE htt_tx_mutex;

    struct {
        int htc_err_cnt;
    } stats;

    int cur_seq_num_hl;
    struct htt_tx_mgmt_desc_ctxt tx_mgmt_desc_ctxt;
    struct targetdef_s *targetdef;

#ifdef IPA_UC_OFFLOAD
    struct htt_ipa_uc_tx_resource_t ipa_uc_tx_rsc;
    struct htt_ipa_uc_rx_resource_t ipa_uc_rx_rsc;
#endif /* IPA_UC_OFFLOAD */

    struct htt_tx_credit_t htt_tx_credit;

#ifdef DEBUG_RX_RING_BUFFER
    struct rx_buf_debug *rx_buff_list;
    int rx_buff_index;
#endif

    int num_pages;
    int num_desc_per_page;
    struct htt_tx_desc_page_t *desc_pages;
};

#endif /* _HTT_TYPES__H_ */
