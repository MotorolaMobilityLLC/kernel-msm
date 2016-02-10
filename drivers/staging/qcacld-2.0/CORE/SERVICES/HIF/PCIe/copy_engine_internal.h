/*
 * Copyright (c) 2013 The Linux Foundation. All rights reserved.
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

#include <hif.h> /* A_TARGET_WRITE */

/* Copy Engine operational state */
enum CE_op_state {
    CE_UNUSED,
    CE_PAUSED,
    CE_RUNNING,
};

enum ol_ath_hif_ce_ecodes {
    CE_RING_DELTA_FAIL=0
};

struct CE_src_desc;

/* Copy Engine Ring internal state */
struct CE_ring_state {

    /* Number of entries in this ring; must be power of 2 */
    unsigned int nentries;
    unsigned int nentries_mask;

    /*
     * For dest ring, this is the next index to be processed
     * by software after it was/is received into.
     *
     * For src ring, this is the last descriptor that was sent
     * and completion processed by software.
     *
     * Regardless of src or dest ring, this is an invariant
     * (modulo ring size):
     *     write index >= read index >= sw_index
     */
    unsigned int sw_index;
    unsigned int write_index; /* cached copy */
    /*
     * For src ring, this is the next index not yet processed by HW.
     * This is a cached copy of the real HW index (read index), used
     * for avoiding reading the HW index register more often than
     * necessary.
     * This extends the invariant:
     *     write index >= read index >= hw_index >= sw_index
     *
     * For dest ring, this is currently unused.
     */
    unsigned int hw_index;    /* cached copy */

    /* Start of DMA-coherent area reserved for descriptors */
    void *base_addr_owner_space_unaligned; /* Host address space */
    CE_addr_t base_addr_CE_space_unaligned;  /* CE address space */

    /*
     * Actual start of descriptors.
     * Aligned to descriptor-size boundary.
     * Points into reserved DMA-coherent area, above.
     */
    void *base_addr_owner_space; /* Host address space */
    CE_addr_t base_addr_CE_space; /* CE address space */
    /*
     * Start of shadow copy of descriptors, within regular memory.
     * Aligned to descriptor-size boundary.
     */
    char *shadow_base_unaligned;
    struct CE_src_desc *shadow_base;

    unsigned int low_water_mark_nentries;
    unsigned int high_water_mark_nentries;
    void **per_transfer_context;
    OS_DMA_MEM_CONTEXT(ce_dmacontext)   // OS Specific DMA context
};

/* Copy Engine internal state */
struct CE_state {
    struct hif_pci_softc *sc; /* back pointer to device's sc */
    unsigned int id;
    unsigned int attr_flags; /* CE_ATTR_* */
    u_int32_t ctrl_addr; /* relative to BAR */
    enum CE_op_state state;

    CE_send_cb send_cb;
    void *send_context;

    CE_recv_cb recv_cb;
    void *recv_context;

    /* misc_cbs - are any callbacks besides send and recv enabled? */
    u_int8_t misc_cbs;

    CE_watermark_cb watermark_cb;
    void *wm_context;

    /*Record the state of the copy compl interrupt*/
    int disable_copy_compl_intr;

    unsigned int src_sz_max;
    struct CE_ring_state *src_ring;
    struct CE_ring_state *dest_ring;
    atomic_t rx_pending;

    /* epping */
    bool timer_inited;
    adf_os_timer_t poll_timer;
};

/* Descriptor rings must be aligned to this boundary */
#define CE_DESC_RING_ALIGN 8

struct CE_src_desc {
    CE_addr_t src_ptr;
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int32_t  meta_data:14,
            byte_swap:1,
            gather:1,
            nbytes:16;
#else

    u_int32_t nbytes:16,
	      gather:1,
	      byte_swap:1,
	      meta_data:14;
#endif
};

struct dest_desc_info {
#if _BYTE_ORDER == _BIG_ENDIAN
    u_int32_t  meta_data:14,
               byte_swap:1,
               gather:1,
               nbytes:16;
#else
    u_int32_t nbytes:16,
              gather:1,
              byte_swap:1,
              meta_data:14;
#endif
};

struct CE_dest_desc {
    CE_addr_t dest_ptr;
    struct dest_desc_info info;
};

/**
 * typdef CE_desc - unified data type for ce descriptors
 *
 * Both src and destination descriptors follow the same format.
 * They use different data structures for different access symantics.
 * Here we provice a unifying data type.
 */
typedef union {
	struct CE_src_desc src_desc;
	struct CE_dest_desc dest_desc;
} CE_desc;

#define CE_SENDLIST_ITEMS_MAX 12

enum CE_sendlist_type_e {
    CE_SIMPLE_BUFFER_TYPE,
    /* TBDXXX: CE_RX_DESC_LIST, */
};

/*
 * There's a public "CE_sendlist" and a private "CE_sendlist_s".
 * The former is an opaque structure with sufficient space
 * to hold the latter.  The latter is the actual structure
 * definition and it is only used internally.  The opaque version
 * of the structure allows callers to allocate an instance on the
 * run-time stack without knowing any of the details of the
 * structure layout.
 */
struct CE_sendlist_s {
    unsigned int num_items;
    struct CE_sendlist_item {
        enum CE_sendlist_type_e send_type;
        dma_addr_t data; /* e.g. buffer or desc list */
        union {
            unsigned int nbytes;  /* simple buffer */
            unsigned int ndesc;   /* Rx descriptor list */
        } u;
        /* flags: externally-specified flags; OR-ed with internal flags */
        u_int32_t flags;
    } item[CE_SENDLIST_ITEMS_MAX];
};


/* which ring of a CE? */
#define CE_RING_SRC  0
#define CE_RING_DEST 1

#define CDC_WAR_MAGIC_STR   0xceef0000
#define CDC_WAR_DATA_CE     4

/* Additional internal-only CE_send flags */
#define CE_SEND_FLAG_GATHER             0x00010000         /* Use Gather */

#define CE_SRC_RING_WRITE_IDX_SET(targid, CE_ctrl_addr, n) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+SR_WR_INDEX_ADDRESS, (n))

#define CE_SRC_RING_WRITE_IDX_GET(targid, CE_ctrl_addr) \
        A_TARGET_READ((targid), (CE_ctrl_addr)+SR_WR_INDEX_ADDRESS)

#define CE_SRC_RING_READ_IDX_GET(targid, CE_ctrl_addr) \
        A_TARGET_READ((targid), (CE_ctrl_addr)+CURRENT_SRRI_ADDRESS)

#define CE_SRC_RING_BASE_ADDR_SET(targid, CE_ctrl_addr, addr) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+SR_BA_ADDRESS, (addr))

#define CE_SRC_RING_SZ_SET(targid, CE_ctrl_addr, n) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+SR_SIZE_ADDRESS, (n))

#define CE_SRC_RING_DMAX_SET(targid, CE_ctrl_addr, n) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+CE_CTRL1_ADDRESS, \
                 (A_TARGET_READ((targid), (CE_ctrl_addr)+CE_CTRL1_ADDRESS) & ~CE_CTRL1_DMAX_LENGTH_MASK) | \
                                CE_CTRL1_DMAX_LENGTH_SET(n))
#define CE_SRC_RING_BYTE_SWAP_SET(targid, CE_ctrl_addr, n) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+CE_CTRL1_ADDRESS, \
                    (A_TARGET_READ((targid), (CE_ctrl_addr)+CE_CTRL1_ADDRESS) & ~CE_CTRL1_SRC_RING_BYTE_SWAP_EN_MASK ) | \
                                CE_CTRL1_SRC_RING_BYTE_SWAP_EN_SET(n))

#define CE_DEST_RING_BYTE_SWAP_SET(targid, CE_ctrl_addr, n) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+CE_CTRL1_ADDRESS, \
                    (A_TARGET_READ((targid), (CE_ctrl_addr)+CE_CTRL1_ADDRESS) & ~CE_CTRL1_DST_RING_BYTE_SWAP_EN_MASK ) | \
                                CE_CTRL1_DST_RING_BYTE_SWAP_EN_SET(n))

#define CE_DEST_RING_WRITE_IDX_SET(targid, CE_ctrl_addr, n) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+DST_WR_INDEX_ADDRESS, (n))

#define CE_DEST_RING_WRITE_IDX_GET(targid, CE_ctrl_addr) \
        A_TARGET_READ((targid), (CE_ctrl_addr)+DST_WR_INDEX_ADDRESS)

#define CE_DEST_RING_READ_IDX_GET(targid, CE_ctrl_addr) \
        A_TARGET_READ((targid), (CE_ctrl_addr)+CURRENT_DRRI_ADDRESS)

#define CE_DEST_RING_BASE_ADDR_SET(targid, CE_ctrl_addr, addr) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+DR_BA_ADDRESS, (addr))

#define CE_DEST_RING_SZ_SET(targid, CE_ctrl_addr, n) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+DR_SIZE_ADDRESS, (n))

#define CE_SRC_RING_HIGHMARK_SET(targid, CE_ctrl_addr, n) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+SRC_WATERMARK_ADDRESS, \
                 (A_TARGET_READ((targid), (CE_ctrl_addr)+SRC_WATERMARK_ADDRESS) & ~SRC_WATERMARK_HIGH_MASK) | \
                                SRC_WATERMARK_HIGH_SET(n))

#define CE_SRC_RING_LOWMARK_SET(targid, CE_ctrl_addr, n) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+SRC_WATERMARK_ADDRESS, \
                 (A_TARGET_READ((targid), (CE_ctrl_addr)+SRC_WATERMARK_ADDRESS) & ~SRC_WATERMARK_LOW_MASK) | \
                                SRC_WATERMARK_LOW_SET(n))

#define CE_DEST_RING_HIGHMARK_SET(targid, CE_ctrl_addr, n) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+DST_WATERMARK_ADDRESS, \
                 (A_TARGET_READ((targid), (CE_ctrl_addr)+DST_WATERMARK_ADDRESS) & ~DST_WATERMARK_HIGH_MASK) | \
                                DST_WATERMARK_HIGH_SET(n))

#define CE_DEST_RING_LOWMARK_SET(targid, CE_ctrl_addr, n) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+DST_WATERMARK_ADDRESS, \
                 (A_TARGET_READ((targid), (CE_ctrl_addr)+DST_WATERMARK_ADDRESS) & ~DST_WATERMARK_LOW_MASK) | \
                                DST_WATERMARK_LOW_SET(n))

#define CE_COPY_COMPLETE_INTR_ENABLE(targid, CE_ctrl_addr) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+HOST_IE_ADDRESS, \
		A_TARGET_READ((targid), (CE_ctrl_addr)+HOST_IE_ADDRESS) | HOST_IE_COPY_COMPLETE_MASK)

#define CE_COPY_COMPLETE_INTR_DISABLE(targid, CE_ctrl_addr) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+HOST_IE_ADDRESS, \
		A_TARGET_READ((targid), (CE_ctrl_addr)+HOST_IE_ADDRESS) & ~HOST_IE_COPY_COMPLETE_MASK)

#define CE_BASE_ADDRESS(CE_id) \
	CE0_BASE_ADDRESS + ((CE1_BASE_ADDRESS-CE0_BASE_ADDRESS)*(CE_id))

#define CE_WATERMARK_INTR_ENABLE(targid, CE_ctrl_addr) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+HOST_IE_ADDRESS, \
		A_TARGET_READ((targid), (CE_ctrl_addr)+HOST_IE_ADDRESS) | CE_WATERMARK_MASK)

#define CE_WATERMARK_INTR_DISABLE(targid, CE_ctrl_addr) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+HOST_IE_ADDRESS, \
		A_TARGET_READ((targid), (CE_ctrl_addr)+HOST_IE_ADDRESS) & ~CE_WATERMARK_MASK)

#define CE_ERROR_INTR_ENABLE(targid, CE_ctrl_addr) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+MISC_IE_ADDRESS, \
		A_TARGET_READ((targid), (CE_ctrl_addr)+MISC_IE_ADDRESS) | CE_ERROR_MASK)

#define CE_MISC_INT_STATUS_GET(targid, CE_ctrl_addr) \
        A_TARGET_READ((targid), (CE_ctrl_addr)+MISC_IS_ADDRESS)

#define CE_ENGINE_INT_STATUS_GET(targid, CE_ctrl_addr) \
        A_TARGET_READ((targid), (CE_ctrl_addr)+HOST_IS_ADDRESS)

#define CE_ENGINE_INT_STATUS_CLEAR(targid, CE_ctrl_addr, mask) \
        A_TARGET_WRITE((targid), (CE_ctrl_addr)+HOST_IS_ADDRESS, (mask))

#define CE_WATERMARK_MASK (HOST_IS_SRC_RING_LOW_WATERMARK_MASK  | \
                           HOST_IS_SRC_RING_HIGH_WATERMARK_MASK | \
                           HOST_IS_DST_RING_LOW_WATERMARK_MASK  | \
                           HOST_IS_DST_RING_HIGH_WATERMARK_MASK)

#define CE_ERROR_MASK     (MISC_IS_AXI_ERR_MASK           | \
                           MISC_IS_DST_ADDR_ERR_MASK      | \
                           MISC_IS_SRC_LEN_ERR_MASK       | \
                           MISC_IS_DST_MAX_LEN_VIO_MASK   | \
                           MISC_IS_DST_RING_OVERFLOW_MASK | \
                           MISC_IS_SRC_RING_OVERFLOW_MASK)

#define CE_SRC_RING_TO_DESC(baddr, idx)                 &(((struct CE_src_desc *)baddr)[idx])
#define CE_DEST_RING_TO_DESC(baddr, idx)                &(((struct CE_dest_desc *)baddr)[idx])

/* Ring arithmetic (modulus number of entries in ring, which is a pwr of 2).  */
#define CE_RING_DELTA(nentries_mask, fromidx, toidx) \
        (((int)(toidx)-(int)(fromidx)) & (nentries_mask))

#define CE_RING_IDX_INCR(nentries_mask, idx) \
        (((idx) + 1) & (nentries_mask))

#define CE_INTERRUPT_SUMMARY(targid) \
		CE_WRAPPER_INTERRUPT_SUMMARY_HOST_MSI_GET( \
		A_TARGET_READ((targid), CE_WRAPPER_BASE_ADDRESS+CE_WRAPPER_INTERRUPT_SUMMARY_ADDRESS))

/*Macro to increment CE packet errors*/
#define OL_ATH_CE_PKT_ERROR_COUNT_INCR(_sc,_ce_ecode);\
{\
   if(_ce_ecode==CE_RING_DELTA_FAIL)(_sc->ol_sc->pkt_stats.ce_ring_delta_fail_count)+=1;\
}


/* Given a Copy Engine's ID, determine the interrupt number for that copy engine's interrupts. */
#define CE_ID_TO_INUM(id)                               (A_INUM_CE0_COPY_COMP_BASE + (id))
#define CE_INUM_TO_ID(inum)                             ((inum) - A_INUM_CE0_COPY_COMP_BASE)
