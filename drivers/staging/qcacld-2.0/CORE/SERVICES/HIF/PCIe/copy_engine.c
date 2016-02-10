/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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


#include <osdep.h>
#include "a_types.h"
#include <athdefs.h>
#include "osapi_linux.h"
#include "hif_msg_based.h"
#include "if_pci.h"
#include "copy_engine_api.h"
#include "copy_engine_internal.h"
#include "adf_os_lock.h"
#include "hif_pci.h"
#include "regtable.h"
#include <vos_getBin.h>
#include "epping_main.h"

#define CE_POLL_TIMEOUT 10 /* ms */

static int war1_allow_sleep;
extern int hif_pci_war1;

/*
 * Support for Copy Engine hardware, which is mainly used for
 * communication between Host and Target over a PCIe interconnect.
 */

/**
 * enum hif_ce_event_type - HIF copy engine event type
 * @HIF_RX_DESC_POST: event recorded before updating write index of RX ring.
 * @HIF_RX_DESC_COMPLETION: event recorded before updating sw index of RX ring.
 * @HIF_TX_GATHER_DESC_POST: post gather desc. (no write index update)
 * @HIF_TX_DESC_POST: event recorded before updating write index of TX ring.
 * @HIF_TX_DESC_COMPLETION: event recorded before updating sw index of TX ring.
 */
enum hif_ce_event_type {
	HIF_RX_DESC_POST,
	HIF_RX_DESC_COMPLETION,
	HIF_TX_GATHER_DESC_POST,
	HIF_TX_DESC_POST,
	HIF_TX_DESC_COMPLETION,
};

#ifdef CONFIG_SLUB_DEBUG_ON

/**
 * struct hif_ce_event - structure for detailing a ce event
 * @type: what the event was
 * @time: when it happened
 * @descriptor: descriptor enqueued or dequeued
 * @memory: virtual address that was used
 * @index: location of the descriptor in the ce ring;
 */
struct hif_ce_desc_event {
	uint16_t index;
	enum hif_ce_event_type type;
	uint64_t time;
	CE_desc descriptor;
	void* memory;
};

/* max history to record per copy engine */
#define HIF_CE_HISTORY_MAX 512
adf_os_atomic_t hif_ce_desc_history_index[CE_COUNT_MAX];
struct hif_ce_desc_event hif_ce_desc_history[CE_COUNT_MAX][HIF_CE_HISTORY_MAX];
static void CE_init_ce_desc_event_log(int ce_id, int size);

/**
 * get_next_record_index() - get the next record index
 * @table_index: atomic index variable to increment
 * @array_size: array size of the circular buffer
 *
 * Increment the atomic index and reserve the value.
 * Takes care of buffer wrap.
 * Guaranteed to be thread safe as long as fewer than array_size contexts
 * try to access the array.  If there are more than array_size contexts
 * trying to access the array, full locking of the recording process would
 * be needed to have sane logging.
 */
static int get_next_record_index(adf_os_atomic_t *table_index, int array_size)
{
	int record_index = adf_os_atomic_inc_return(table_index);
	if (record_index == array_size)
		adf_os_atomic_sub(array_size, table_index);

	while (record_index >= array_size)
		record_index -= array_size;
	return record_index;
}

/**
 * hif_record_ce_desc_event() - record ce descriptor events
 * @ce_id: which ce is the event occuring on
 * @type: what happened
 * @descriptor: pointer to the descriptor posted/completed
 * @memory: virtual address of buffer related to the descriptor
 * @index: index that the descriptor was/will be at.
 */
static void hif_record_ce_desc_event(int ce_id, enum hif_ce_event_type type,
		CE_desc *descriptor, void *memory, int index)
{
	int record_index = get_next_record_index(
			&hif_ce_desc_history_index[ce_id], HIF_CE_HISTORY_MAX);

	struct hif_ce_desc_event *event =
		&hif_ce_desc_history[ce_id][record_index];
	event->type = type;
	event->time = adf_get_boottime();
	event->descriptor = *descriptor;
	event->memory = memory;
	event->index = index;
}

/**
 * CE_init_ce_desc_event_log() - initialize the ce event log
 * @ce_id: copy engine id for which we are initializing the log
 * @size: size of array to dedicate
 *
 * Currently the passed size is ignored in favor of a precompiled value.
 */
static void CE_init_ce_desc_event_log(int ce_id, int size)
{
	adf_os_atomic_init(&hif_ce_desc_history_index[ce_id]);
}
#else
static inline void hif_record_ce_desc_event(
		int ce_id, enum hif_ce_event_type type,
		CE_desc *descriptor, void* memory,
		int index)
{
}

static inline void CE_init_ce_desc_event_log(int ce_id, int size)
{
}
#endif

/*
 * A single CopyEngine (CE) comprises two "rings":
 *   a source ring
 *   a destination ring
 *
 * Each ring consists of a number of descriptors which specify
 * an address, length, and meta-data.
 *
 * Typically, one side of the PCIe interconnect (Host or Target)
 * controls one ring and the other side controls the other ring.
 * The source side chooses when to initiate a transfer and it
 * chooses what to send (buffer address, length). The destination
 * side keeps a supply of "anonymous receive buffers" available and
 * it handles incoming data as it arrives (when the destination
 * recieves an interrupt).
 *
 * The sender may send a simple buffer (address/length) or it may
 * send a small list of buffers.  When a small list is sent, hardware
 * "gathers" these and they end up in a single destination buffer
 * with a single interrupt.
 *
 * There are several "contexts" managed by this layer -- more, it
 * may seem -- than should be needed. These are provided mainly for
 * maximum flexibility and especially to facilitate a simpler HIF
 * implementation. There are per-CopyEngine recv, send, and watermark
 * contexts. These are supplied by the caller when a recv, send,
 * or watermark handler is established and they are echoed back to
 * the caller when the respective callbacks are invoked. There is
 * also a per-transfer context supplied by the caller when a buffer
 * (or sendlist) is sent and when a buffer is enqueued for recv.
 * These per-transfer contexts are echoed back to the caller when
 * the buffer is sent/received.
 */

/*
 * Guts of CE_send, used by both CE_send and CE_sendlist_send.
 * The caller takes responsibility for any needed locking.
 */
int
CE_completed_send_next_nolock(struct CE_state *CE_state,
                              void **per_CE_contextp,
                              void **per_transfer_contextp,
                              CE_addr_t *bufferp,
                              unsigned int *nbytesp,
                              unsigned int *transfer_idp,
                              unsigned int *sw_idx,
                              unsigned int *hw_idx);

void WAR_CE_SRC_RING_WRITE_IDX_SET(struct hif_pci_softc *sc,
                                   void __iomem *targid,
                                   u32 ctrl_addr,
                                   unsigned int write_index)
{
        if (hif_pci_war1) {
                void __iomem *indicator_addr;

                indicator_addr = targid + ctrl_addr + DST_WATERMARK_ADDRESS;

                if (!war1_allow_sleep && ctrl_addr == CE_BASE_ADDRESS(CDC_WAR_DATA_CE)) {
                        A_PCI_WRITE32(indicator_addr,
                                    (CDC_WAR_MAGIC_STR | write_index));
                } else {
                        unsigned long irq_flags;
                        local_irq_save(irq_flags);
                        A_PCI_WRITE32(indicator_addr, 1);

                        /*
                         * PCIE write waits for ACK in IPQ8K, there is no
                         * need to read back value.
                         */
                        (void)A_PCI_READ32(indicator_addr);
                        (void)A_PCI_READ32(indicator_addr); /* conservative */

                        CE_SRC_RING_WRITE_IDX_SET(targid,
                                                  ctrl_addr,
                                                  write_index);

                        A_PCI_WRITE32(indicator_addr, 0);
                        local_irq_restore(irq_flags);
                }
        } else
                CE_SRC_RING_WRITE_IDX_SET(targid, ctrl_addr, write_index);
}

int
CE_send_nolock(struct CE_handle *copyeng,
               void *per_transfer_context,
               CE_addr_t buffer,
               unsigned int nbytes,
               unsigned int transfer_id,
               unsigned int flags)
{
    int status;
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct CE_ring_state *src_ring = CE_state->src_ring;
    u_int32_t ctrl_addr = CE_state->ctrl_addr;
    struct hif_pci_softc *sc = CE_state->sc;
    A_target_id_t targid = TARGID(sc);
    unsigned int nentries_mask = src_ring->nentries_mask;
    unsigned int sw_index = src_ring->sw_index;
    unsigned int write_index = src_ring->write_index;

    A_TARGET_ACCESS_BEGIN_RET(targid);
    if (unlikely(CE_RING_DELTA(nentries_mask, write_index, sw_index-1) <= 0)) {
        OL_ATH_CE_PKT_ERROR_COUNT_INCR(sc,CE_RING_DELTA_FAIL);
        status = A_ERROR;
        A_TARGET_ACCESS_END_RET(targid);
        return status;
    }
    {
        enum hif_ce_event_type event_type = HIF_TX_GATHER_DESC_POST;
        struct CE_src_desc *src_ring_base = (struct CE_src_desc *)src_ring->base_addr_owner_space;
        struct CE_src_desc *shadow_base = (struct CE_src_desc *)src_ring->shadow_base;
        struct CE_src_desc *src_desc = CE_SRC_RING_TO_DESC(src_ring_base, write_index);
        struct CE_src_desc *shadow_src_desc = CE_SRC_RING_TO_DESC(shadow_base, write_index);

        /* Update source descriptor */
        shadow_src_desc->src_ptr   = buffer;
        shadow_src_desc->meta_data = transfer_id;

        /*
         * Set the swap bit if:
         *   typical sends on this CE are swapped (host is big-endian) and
         *   this send doesn't disable the swapping (data is not bytestream)
         */
        shadow_src_desc->byte_swap =
            (((CE_state->attr_flags & CE_ATTR_BYTE_SWAP_DATA) != 0) &
            ((flags & CE_SEND_FLAG_SWAP_DISABLE) == 0));
        shadow_src_desc->gather    = ((flags & CE_SEND_FLAG_GATHER) != 0);
        shadow_src_desc->nbytes    = nbytes;

        *src_desc = *shadow_src_desc;

        src_ring->per_transfer_context[write_index] = per_transfer_context;

        /* Update Source Ring Write Index */
        write_index = CE_RING_IDX_INCR(nentries_mask, write_index);


        /* WORKAROUND */
        if (!shadow_src_desc->gather) {
            event_type = HIF_TX_DESC_POST;
            WAR_CE_SRC_RING_WRITE_IDX_SET(sc, targid, ctrl_addr, write_index);
        }

        /* src_ring->write index hasn't been updated event though the register
         * has allready been written to.
         */
        hif_record_ce_desc_event(CE_state->id, event_type,
                        (CE_desc *) shadow_src_desc, per_transfer_context,
                        src_ring->write_index);

        src_ring->write_index = write_index;
        status = A_OK;
    }
    A_TARGET_ACCESS_END_RET(targid);

    return status;
}

int
CE_send(struct CE_handle *copyeng,
        void *per_transfer_context,
        CE_addr_t buffer,
        unsigned int nbytes,
        unsigned int transfer_id,
        unsigned int flags)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct hif_pci_softc *sc = CE_state->sc;
    int status;


    adf_os_spin_lock_bh(&sc->target_lock);
    status = CE_send_nolock(copyeng, per_transfer_context, buffer, nbytes, transfer_id, flags);
    adf_os_spin_unlock_bh(&sc->target_lock);

    return status;
}

unsigned int
CE_sendlist_sizeof(void)
{
   return sizeof(struct CE_sendlist);
}

void
CE_sendlist_init(struct CE_sendlist *sendlist)
{
    struct CE_sendlist_s *sl = (struct CE_sendlist_s *)sendlist;
    sl->num_items=0;
}

int
CE_sendlist_buf_add(struct CE_sendlist *sendlist,
                    CE_addr_t buffer,
                    unsigned int nbytes,
                    u_int32_t flags)
{
    struct CE_sendlist_s *sl = (struct CE_sendlist_s *)sendlist;
    unsigned int num_items = sl->num_items;
    struct CE_sendlist_item *item;

	if (num_items >= CE_SENDLIST_ITEMS_MAX) {
		A_ASSERT(num_items < CE_SENDLIST_ITEMS_MAX);
		return A_NO_RESOURCE;
	}

    item = &sl->item[num_items];
    item->send_type = CE_SIMPLE_BUFFER_TYPE;
    item->data = buffer;
    item->u.nbytes = nbytes;
    item->flags = flags;
    sl->num_items = num_items+1;
	return A_OK;
}

int
CE_sendlist_send(struct CE_handle *copyeng,
                 void *per_transfer_context,
                 struct CE_sendlist *sendlist,
                 unsigned int transfer_id)
{
    int status = -ENOMEM;
    struct CE_sendlist_s *sl = (struct CE_sendlist_s *)sendlist;
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct CE_ring_state *src_ring = CE_state->src_ring;
    struct hif_pci_softc *sc = CE_state->sc;
    unsigned int nentries_mask = src_ring->nentries_mask;
    unsigned int num_items = sl->num_items;
    unsigned int sw_index ;
    unsigned int write_index ;

    A_ASSERT((num_items > 0) && (num_items < src_ring->nentries));

    adf_os_spin_lock_bh(&sc->target_lock);
    sw_index = src_ring->sw_index;
    write_index = src_ring->write_index;

    if (CE_RING_DELTA(nentries_mask, write_index, sw_index-1) >= num_items) {
        struct CE_sendlist_item *item;
        int i;

        /* handle all but the last item uniformly */
        for (i = 0; i < num_items-1; i++) {
            item = &sl->item[i];
            /* TBDXXX: Support extensible sendlist_types? */
            A_ASSERT(item->send_type == CE_SIMPLE_BUFFER_TYPE);
            status = CE_send_nolock(copyeng, CE_SENDLIST_ITEM_CTXT,
                                    (CE_addr_t)item->data, item->u.nbytes,
                                    transfer_id,
                                    item->flags | CE_SEND_FLAG_GATHER);
            A_ASSERT(status == A_OK);
        }
        /* provide valid context pointer for final item */
        item = &sl->item[i];
        /* TBDXXX: Support extensible sendlist_types? */
        A_ASSERT(item->send_type == CE_SIMPLE_BUFFER_TYPE);
        status = CE_send_nolock(copyeng, per_transfer_context,
                                (CE_addr_t)item->data, item->u.nbytes,
                                transfer_id, item->flags);
        A_ASSERT(status == A_OK);
    } else {
        /*
         * Probably not worth the additional complexity to support
         * partial sends with continuation or notification.  We expect
         * to use large rings and small sendlists. If we can't handle
         * the entire request at once, punt it back to the caller.
         */
    }
    adf_os_spin_unlock_bh(&sc->target_lock);

    return status;
}

int
CE_recv_buf_enqueue(struct CE_handle *copyeng,
                    void *per_recv_context,
                    CE_addr_t buffer)
{
    int status;
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct CE_ring_state *dest_ring = CE_state->dest_ring;
    u_int32_t ctrl_addr = CE_state->ctrl_addr;
    struct hif_pci_softc *sc = CE_state->sc;
    A_target_id_t targid = TARGID(sc);
    unsigned int nentries_mask = dest_ring->nentries_mask;
    unsigned int write_index ;
    unsigned int sw_index;
    int val = 0;

    adf_os_spin_lock_bh(&sc->target_lock);
    write_index = dest_ring->write_index;
    sw_index = dest_ring->sw_index;

    A_TARGET_ACCESS_BEGIN_RET_EXT(targid, val);
    if (val == -1) {
        adf_os_spin_unlock_bh(&sc->target_lock);
        return val;
    }

    if (CE_RING_DELTA(nentries_mask, write_index, sw_index-1) > 0) {
        struct CE_dest_desc *dest_ring_base = (struct CE_dest_desc *)dest_ring->base_addr_owner_space;
        struct CE_dest_desc *dest_desc = CE_DEST_RING_TO_DESC(dest_ring_base, write_index);

        /* Update destination descriptor */
        dest_desc->dest_ptr = buffer;
        dest_desc->info.nbytes = 0; /* NB: Enable CE_completed_recv_next_nolock to
								protect against race between DRRI update and
								desc update */

        dest_ring->per_transfer_context[write_index] = per_recv_context;

        hif_record_ce_desc_event(CE_state->id, HIF_RX_DESC_POST,
                        (CE_desc *) dest_desc, per_recv_context,
                        write_index);

        /* Update Destination Ring Write Index */
        write_index = CE_RING_IDX_INCR(nentries_mask, write_index);
        CE_DEST_RING_WRITE_IDX_SET(targid, ctrl_addr, write_index);
        dest_ring->write_index = write_index;
        status = A_OK;
    } else {
        status = A_ERROR;
    }
    A_TARGET_ACCESS_END_RET_EXT(targid, val);
    if (val == -1) {
        adf_os_spin_unlock_bh(&sc->target_lock);
        return val;
    }

    adf_os_spin_unlock_bh(&sc->target_lock);

    return status;
}

void
CE_send_watermarks_set(struct CE_handle *copyeng,
                       unsigned int low_alert_nentries,
                       unsigned int high_alert_nentries)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    u_int32_t ctrl_addr = CE_state->ctrl_addr;
    struct hif_pci_softc *sc = CE_state->sc;
    A_target_id_t targid = TARGID(sc);

    adf_os_spin_lock(&sc->target_lock);
    CE_SRC_RING_LOWMARK_SET(targid, ctrl_addr, low_alert_nentries);
    CE_SRC_RING_HIGHMARK_SET(targid, ctrl_addr, high_alert_nentries);
    adf_os_spin_unlock(&sc->target_lock);
}

void
CE_recv_watermarks_set(struct CE_handle *copyeng,
                       unsigned int low_alert_nentries,
                       unsigned int high_alert_nentries)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    u_int32_t ctrl_addr = CE_state->ctrl_addr;
    struct hif_pci_softc *sc = CE_state->sc;
    A_target_id_t targid = TARGID(sc);

    adf_os_spin_lock(&sc->target_lock);
    CE_DEST_RING_LOWMARK_SET(targid, ctrl_addr, low_alert_nentries);
    CE_DEST_RING_HIGHMARK_SET(targid, ctrl_addr, high_alert_nentries);
    adf_os_spin_unlock(&sc->target_lock);
}

unsigned int
CE_send_entries_avail(struct CE_handle *copyeng)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct CE_ring_state *src_ring = CE_state->src_ring;
    struct hif_pci_softc *sc = CE_state->sc;
    unsigned int nentries_mask = src_ring->nentries_mask;
    unsigned int sw_index;
    unsigned int write_index;

    adf_os_spin_lock(&sc->target_lock);
    sw_index = src_ring->sw_index;
    write_index = src_ring->write_index;
    adf_os_spin_unlock(&sc->target_lock);

    return CE_RING_DELTA(nentries_mask, write_index, sw_index-1);
}

unsigned int
CE_recv_entries_avail(struct CE_handle *copyeng)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct CE_ring_state *dest_ring = CE_state->dest_ring;
    struct hif_pci_softc *sc = CE_state->sc;
    unsigned int nentries_mask = dest_ring->nentries_mask;
    unsigned int sw_index;
    unsigned int write_index;

    adf_os_spin_lock(&sc->target_lock);
    sw_index = dest_ring->sw_index;
    write_index = dest_ring->write_index;
    adf_os_spin_unlock(&sc->target_lock);

    return CE_RING_DELTA(nentries_mask, write_index, sw_index-1);
}

/*
 * Guts of CE_send_entries_done.
 * The caller takes responsibility for any necessary locking.
 */
unsigned int
CE_send_entries_done_nolock(struct hif_pci_softc *sc, struct CE_state *CE_state)
{
    struct CE_ring_state *src_ring = CE_state->src_ring;
    u_int32_t ctrl_addr = CE_state->ctrl_addr;
    A_target_id_t targid = TARGID(sc);
    unsigned int nentries_mask = src_ring->nentries_mask;
    unsigned int sw_index;
    unsigned int read_index;

    sw_index = src_ring->sw_index;
    read_index = CE_SRC_RING_READ_IDX_GET(targid, ctrl_addr);

    return CE_RING_DELTA(nentries_mask, sw_index, read_index);
}

unsigned int
CE_send_entries_done(struct CE_handle *copyeng)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct hif_pci_softc *sc = CE_state->sc;
    unsigned int nentries;

    adf_os_spin_lock(&sc->target_lock);
    nentries = CE_send_entries_done_nolock(sc, CE_state);
    adf_os_spin_unlock(&sc->target_lock);

    return nentries;
}

/*
 * Guts of CE_recv_entries_done.
 * The caller takes responsibility for any necessary locking.
 */
unsigned int
CE_recv_entries_done_nolock(struct hif_pci_softc *sc, struct CE_state *CE_state)
{
    struct CE_ring_state *dest_ring = CE_state->dest_ring;
    u_int32_t ctrl_addr = CE_state->ctrl_addr;
    A_target_id_t targid = TARGID(sc);
    unsigned int nentries_mask = dest_ring->nentries_mask;
    unsigned int sw_index;
    unsigned int read_index;

    sw_index = dest_ring->sw_index;
    read_index = CE_DEST_RING_READ_IDX_GET(targid, ctrl_addr);

    return CE_RING_DELTA(nentries_mask, sw_index, read_index);
}

unsigned int
CE_recv_entries_done(struct CE_handle *copyeng)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct hif_pci_softc *sc = CE_state->sc;
    unsigned int nentries;

    adf_os_spin_lock(&sc->target_lock);
    nentries = CE_recv_entries_done_nolock(sc, CE_state);
    adf_os_spin_unlock(&sc->target_lock);

    return nentries;
}

/* Debug support */
void *ce_debug_cmplrn_context; /* completed recv next context */
void *ce_debug_cnclsn_context; /* cancel send next context */
void *ce_debug_rvkrn_context;  /* revoke receive next context */
void *ce_debug_cmplsn_context; /* completed send next context */


/*
 * Guts of CE_completed_recv_next.
 * The caller takes responsibility for any necessary locking.
 */
int
CE_completed_recv_next_nolock(struct CE_state *CE_state,
                              void **per_CE_contextp,
                              void **per_transfer_contextp,
                              CE_addr_t *bufferp,
                              unsigned int *nbytesp,
                              unsigned int *transfer_idp,
                              unsigned int *flagsp)
{
    int status;
    struct CE_ring_state *dest_ring = CE_state->dest_ring;
    unsigned int nentries_mask = dest_ring->nentries_mask;
    unsigned int sw_index = dest_ring->sw_index;

    struct CE_dest_desc *dest_ring_base = (struct CE_dest_desc *)dest_ring->base_addr_owner_space;
    struct CE_dest_desc *dest_desc = CE_DEST_RING_TO_DESC(dest_ring_base, sw_index);
    int nbytes;
    struct dest_desc_info dest_desc_info;

    /*
     * By copying the dest_desc_info element to local memory, we could
     * avoid extra memory read from non-cachable memory.
     */
    dest_desc_info = dest_desc->info;
    nbytes = dest_desc_info.nbytes;
    if (nbytes == 0) {
        /*
         * This closes a relatively unusual race where the Host
         * sees the updated DRRI before the update to the
         * corresponding descriptor has completed. We treat this
         * as a descriptor that is not yet done.
         */
        status = A_ERROR;
        goto done;
    }

    hif_record_ce_desc_event(CE_state->id, HIF_RX_DESC_COMPLETION,
                        (CE_desc *) dest_desc,
                        dest_ring->per_transfer_context[sw_index],
                        sw_index);

    dest_desc->info.nbytes = 0;

    /* Return data from completed destination descriptor */
    *bufferp      = (CE_addr_t)(dest_desc->dest_ptr);
    *nbytesp      = nbytes;
    *transfer_idp = dest_desc_info.meta_data;
    *flagsp       = (dest_desc_info.byte_swap) ?  CE_RECV_FLAG_SWAPPED : 0;

    if (per_CE_contextp) {
        *per_CE_contextp = CE_state->recv_context;
    }

    ce_debug_cmplrn_context = dest_ring->per_transfer_context[sw_index];
    if (per_transfer_contextp) {
        *per_transfer_contextp = ce_debug_cmplrn_context;
    }
    dest_ring->per_transfer_context[sw_index] = 0; /* sanity */

    /* Update sw_index */
    sw_index = CE_RING_IDX_INCR(nentries_mask, sw_index);
    dest_ring->sw_index = sw_index;
    status = A_OK;

done:
    return status;
}

int
CE_completed_recv_next(struct CE_handle *copyeng,
                       void **per_CE_contextp,
                       void **per_transfer_contextp,
                       CE_addr_t *bufferp,
                       unsigned int *nbytesp,
                       unsigned int *transfer_idp,
                       unsigned int *flagsp)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct hif_pci_softc *sc = CE_state->sc;
    int status;


    adf_os_spin_lock_bh(&sc->target_lock);
    status = CE_completed_recv_next_nolock(CE_state, per_CE_contextp, per_transfer_contextp,
                                               bufferp, nbytesp, transfer_idp, flagsp);
    adf_os_spin_unlock_bh(&sc->target_lock);

    return status;
}

/* NB: Modeled after CE_completed_recv_next_nolock */
A_STATUS
CE_revoke_recv_next(struct CE_handle *copyeng,
                    void **per_CE_contextp,
                    void **per_transfer_contextp,
                    CE_addr_t *bufferp)
{
    struct CE_state *CE_state;
    struct CE_ring_state *dest_ring;
    unsigned int nentries_mask;
    unsigned int sw_index;
    unsigned int write_index;
    A_STATUS status;
    struct hif_pci_softc *sc;

    CE_state = (struct CE_state *)copyeng;
    dest_ring = CE_state->dest_ring;
    if (!dest_ring) {
        return A_ERROR;
    }

    sc = CE_state->sc;
    adf_os_spin_lock(&sc->target_lock);
    nentries_mask = dest_ring->nentries_mask;
    sw_index = dest_ring->sw_index;
    write_index = dest_ring->write_index;
    if (write_index != sw_index) {
        struct CE_dest_desc *dest_ring_base = (struct CE_dest_desc *)dest_ring->base_addr_owner_space;
        struct CE_dest_desc *dest_desc = CE_DEST_RING_TO_DESC(dest_ring_base, sw_index);

        /* Return data from completed destination descriptor */
        *bufferp     = (CE_addr_t)(dest_desc->dest_ptr);

        if (per_CE_contextp) {
            *per_CE_contextp = CE_state->recv_context;
        }

        ce_debug_rvkrn_context = dest_ring->per_transfer_context[sw_index];
        if (per_transfer_contextp) {
            *per_transfer_contextp = ce_debug_rvkrn_context;
        }
        dest_ring->per_transfer_context[sw_index] = 0; /* sanity */

        /* Update sw_index */
        sw_index = CE_RING_IDX_INCR(nentries_mask, sw_index);
        dest_ring->sw_index = sw_index;
        status = A_OK;
    } else {
        status = A_ERROR;
    }
    adf_os_spin_unlock(&sc->target_lock);

    return status;
}

/*
 * Guts of CE_completed_send_next.
 * The caller takes responsibility for any necessary locking.
 */
int
CE_completed_send_next_nolock(struct CE_state *CE_state,
                              void **per_CE_contextp,
                              void **per_transfer_contextp,
                              CE_addr_t *bufferp,
                              unsigned int *nbytesp,
                              unsigned int *transfer_idp,
                              unsigned int *sw_idx,
                              unsigned int *hw_idx)
{
    int status = A_ERROR;
    struct CE_ring_state *src_ring = CE_state->src_ring;
    u_int32_t ctrl_addr = CE_state->ctrl_addr;
    struct hif_pci_softc *sc = CE_state->sc;
    A_target_id_t targid = TARGID(sc);
    unsigned int nentries_mask = src_ring->nentries_mask;
    unsigned int sw_index = src_ring->sw_index;
    unsigned int read_index;


    if (src_ring->hw_index == sw_index) {
        /*
         * The SW completion index has caught up with the cached
         * version of the HW completion index.
         * Update the cached HW completion index to see whether
         * the SW has really caught up to the HW, or if the cached
         * value of the HW index has become stale.
         */
        A_TARGET_ACCESS_BEGIN_RET(targid);
        src_ring->hw_index = CE_SRC_RING_READ_IDX_GET(targid, ctrl_addr);
        A_TARGET_ACCESS_END_RET(targid);
    }
    read_index = src_ring->hw_index;

    if (sw_idx)
        *sw_idx = sw_index;

    if (hw_idx)
        *hw_idx = read_index;

    if ((read_index != sw_index) && (read_index != 0xffffffff)) {
        struct CE_src_desc *shadow_base = (struct CE_src_desc *)src_ring->shadow_base;
        struct CE_src_desc *shadow_src_desc = CE_SRC_RING_TO_DESC(shadow_base, sw_index);

        hif_record_ce_desc_event(CE_state->id, HIF_TX_DESC_COMPLETION,
                        (CE_desc *) shadow_src_desc,
                        src_ring->per_transfer_context[sw_index],
                        sw_index);

        /* Return data from completed source descriptor */
        *bufferp      = (CE_addr_t)(shadow_src_desc->src_ptr);
        *nbytesp      = shadow_src_desc->nbytes;
        *transfer_idp = shadow_src_desc->meta_data;

        if (per_CE_contextp) {
            *per_CE_contextp = CE_state->send_context;
        }

        ce_debug_cmplsn_context = src_ring->per_transfer_context[sw_index];
        if (per_transfer_contextp) {
            *per_transfer_contextp = ce_debug_cmplsn_context;
        }
        src_ring->per_transfer_context[sw_index] = 0; /* sanity */

        /* Update sw_index */
        sw_index = CE_RING_IDX_INCR(nentries_mask, sw_index);
        src_ring->sw_index = sw_index;
        status = A_OK;
    }

    return status;
}

/* NB: Modeled after CE_completed_send_next */
A_STATUS
CE_cancel_send_next(struct CE_handle *copyeng,
                    void **per_CE_contextp,
                    void **per_transfer_contextp,
                    CE_addr_t *bufferp,
                    unsigned int *nbytesp,
                    unsigned int *transfer_idp)
{
    struct CE_state *CE_state;
    struct CE_ring_state *src_ring;
    unsigned int nentries_mask;
    unsigned int sw_index;
    unsigned int write_index;
    A_STATUS status;
    struct hif_pci_softc *sc;

    CE_state = (struct CE_state *)copyeng;
    src_ring = CE_state->src_ring;
    if (!src_ring) {
        return A_ERROR;
    }

    sc = CE_state->sc;
    adf_os_spin_lock(&sc->target_lock);
    nentries_mask = src_ring->nentries_mask;
    sw_index = src_ring->sw_index;
    write_index = src_ring->write_index;

    if (write_index != sw_index) {
        struct CE_src_desc *src_ring_base = (struct CE_src_desc *)src_ring->base_addr_owner_space;
        struct CE_src_desc *src_desc = CE_SRC_RING_TO_DESC(src_ring_base, sw_index);

        /* Return data from completed source descriptor */
        *bufferp      = (CE_addr_t)(src_desc->src_ptr);
        *nbytesp      = src_desc->nbytes;
        *transfer_idp = src_desc->meta_data;

        if (per_CE_contextp) {
            *per_CE_contextp = CE_state->send_context;
        }

        ce_debug_cnclsn_context = src_ring->per_transfer_context[sw_index];
        if (per_transfer_contextp) {
            *per_transfer_contextp = ce_debug_cnclsn_context;
        }
        src_ring->per_transfer_context[sw_index] = 0; /* sanity */

        /* Update sw_index */
        sw_index = CE_RING_IDX_INCR(nentries_mask, sw_index);
        src_ring->sw_index = sw_index;
        status = A_OK;
    } else {
        status = A_ERROR;
    }
    adf_os_spin_unlock(&sc->target_lock);

    return status;
}

/* Shift bits to convert IS_*_RING_*_WATERMARK_MASK to CE_WM_FLAG_*_* */
#define CE_WM_SHFT 1


int
CE_completed_send_next(struct CE_handle *copyeng,
                       void **per_CE_contextp,
                       void **per_transfer_contextp,
                       CE_addr_t *bufferp,
                       unsigned int *nbytesp,
                       unsigned int *transfer_idp,
                       unsigned int *sw_idx,
                       unsigned int *hw_idx)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct hif_pci_softc *sc = CE_state->sc;
    int status;


    adf_os_spin_lock_bh(&sc->target_lock);
    status = CE_completed_send_next_nolock(CE_state, per_CE_contextp, per_transfer_contextp,
                                               bufferp, nbytesp, transfer_idp,
                                               sw_idx, hw_idx);
    adf_os_spin_unlock_bh(&sc->target_lock);

    return status;
}


#ifdef ATH_11AC_TXCOMPACT
/* CE engine descriptor reap
   Similar to CE_per_engine_service , Only difference is CE_per_engine_service
   does recieve and reaping of completed descriptor ,
   This function only handles reaping of Tx complete descriptor.
   The Function is called from threshold reap  poll routine HIFSendCompleteCheck
   So should not countain recieve functionality within it .
 */

void
CE_per_engine_servicereap(struct hif_pci_softc *sc, unsigned int CE_id)
{
    struct CE_state *CE_state = sc->CE_id_to_state[CE_id];
    A_target_id_t targid = TARGID(sc);
    void *CE_context;
    void *transfer_context;
    CE_addr_t buf;
    unsigned int nbytes;
    unsigned int id;
    unsigned int sw_idx, hw_idx;

    A_TARGET_ACCESS_BEGIN(targid);

    /* Since this function is called from both user context and
     * tasklet context the spinlock has to lock the bottom halves.
     * This fix assumes that ATH_11AC_TXCOMPACT flag is always
     * enabled in TX polling mode. If this is not the case, more
     * bottom halve spin lock changes are needed. Due to data path
     * performance concern, after internal discussion we've decided
     * to make minimum change, i.e., only address the issue occurred
     * in this function. The possible negative effect of this minimum
     * change is that, in the future, if some other function will also
     * be opened to let the user context to use, those cases need to be
     * addressed by change spin_lock to spin_lock_bh also. */

    adf_os_spin_lock_bh(&sc->target_lock);

    if (CE_state->send_cb) {
       {
            /* Pop completed send buffers and call the registered send callback for each */
            while (CE_completed_send_next_nolock(CE_state, &CE_context, &transfer_context,
                        &buf, &nbytes, &id, &sw_idx, &hw_idx) == A_OK)
            {
                if(CE_id != CE_HTT_H2T_MSG){
                    adf_os_spin_unlock_bh(&sc->target_lock);
                    CE_state->send_cb((struct CE_handle *)CE_state, CE_context, transfer_context, buf, nbytes, id,
                                      sw_idx, hw_idx);
                    adf_os_spin_lock_bh(&sc->target_lock);
                }else{
                     struct HIF_CE_pipe_info *pipe_info = (struct HIF_CE_pipe_info *)CE_context;

                     adf_os_spin_lock_bh(&pipe_info->completion_freeq_lock);
                     pipe_info->num_sends_allowed++;
                     adf_os_spin_unlock_bh(&pipe_info->completion_freeq_lock);
                }
            }
        }
    }

    adf_os_spin_unlock_bh(&sc->target_lock);
    A_TARGET_ACCESS_END(targid);
}

#endif /*ATH_11AC_TXCOMPACT*/

/*
 * Number of times to check for any pending tx/rx completion on
 * a copy engine, this count should be big enough. Once we hit
 * this threashold we'll not check for any Tx/Rx comlpetion in same
 * interrupt handling. Note that this threashold is only used for
 * Rx interrupt processing, this can be used tor Tx as well if we
 * suspect any infinite loop in checking for pending Tx completion.
 */
#define CE_TXRX_COMP_CHECK_THRESHOLD 20

/*
 * Guts of interrupt handler for per-engine interrupts on a particular CE.
 *
 * Invokes registered callbacks for recv_complete,
 * send_complete, and watermarks.
 */
void
CE_per_engine_service(struct hif_pci_softc *sc, unsigned int CE_id)
{
    struct CE_state *CE_state = sc->CE_id_to_state[CE_id];
    u_int32_t ctrl_addr = CE_state->ctrl_addr;
    A_target_id_t targid = TARGID(sc);
    void *CE_context;
    void *transfer_context;
    CE_addr_t buf;
    unsigned int nbytes;
    unsigned int id;
    unsigned int flags;
    u_int32_t CE_int_status;
    unsigned int more_comp_cnt = 0;
    unsigned int more_snd_comp_cnt = 0;
    unsigned int sw_idx, hw_idx;

    A_TARGET_ACCESS_BEGIN(targid);

    adf_os_spin_lock(&sc->target_lock);

    /* Clear force_break flag and re-initialize receive_count to 0 */
    sc->receive_count = 0;
    sc->force_break = 0;
more_completions:
    if (CE_state->recv_cb) {

        /* Pop completed recv buffers and call the registered recv callback for each */
        while (CE_completed_recv_next_nolock(CE_state, &CE_context, &transfer_context,
                    &buf, &nbytes, &id, &flags) == A_OK)
        {
                adf_os_spin_unlock(&sc->target_lock);
                CE_state->recv_cb((struct CE_handle *)CE_state, CE_context, transfer_context,
                                    buf, nbytes, id, flags);

                /*
                 * EV #112693 - [Peregrine][ES1][WB342][Win8x86][Performance] BSoD_0x133 occurred in VHT80 UDP_DL
                 * Break out DPC by force if number of loops in HIF_PCI_CE_recv_data reaches MAX_NUM_OF_RECEIVES to avoid spending too long time in DPC for each interrupt handling.
                 * Schedule another DPC to avoid data loss if we had taken force-break action before
                 * Apply to Windows OS only currently, Linux/MAC os can expand to their platform if necessary
                 */

                /* Break the receive processes by force if force_break set up */
                if (adf_os_unlikely(sc->force_break))
                {
                    adf_os_atomic_set(&CE_state->rx_pending, 1);
                    CE_ENGINE_INT_STATUS_CLEAR(targid, ctrl_addr, HOST_IS_COPY_COMPLETE_MASK);
                    A_TARGET_ACCESS_END(targid);
                    return;
                }
                adf_os_spin_lock(&sc->target_lock);
        }
    }

    /*
     * Attention: We may experience potential infinite loop for below While Loop during Sending Stress test
     * Resolve the same way as Receive Case (Refer to EV #112693)
     */

    if (CE_state->send_cb) {
        /* Pop completed send buffers and call the registered send callback for each */

#ifdef ATH_11AC_TXCOMPACT
        while (CE_completed_send_next_nolock(CE_state, &CE_context, &transfer_context,
                    &buf, &nbytes, &id, &sw_idx, &hw_idx) == A_OK){

            if(CE_id != CE_HTT_H2T_MSG ||
                    WLAN_IS_EPPING_ENABLED(vos_get_conparam())){
                adf_os_spin_unlock(&sc->target_lock);
                CE_state->send_cb((struct CE_handle *)CE_state, CE_context, transfer_context, buf, nbytes, id,
                                  sw_idx, hw_idx);
                adf_os_spin_lock(&sc->target_lock);
            }else{
                struct HIF_CE_pipe_info *pipe_info = (struct HIF_CE_pipe_info *)CE_context;

                adf_os_spin_lock(&pipe_info->completion_freeq_lock);
                pipe_info->num_sends_allowed++;
                adf_os_spin_unlock(&pipe_info->completion_freeq_lock);
            }
        }
#else  /*ATH_11AC_TXCOMPACT*/
        while (CE_completed_send_next_nolock(CE_state, &CE_context, &transfer_context,
                    &buf, &nbytes, &id, &sw_idx, &hw_idx) == A_OK){
            adf_os_spin_unlock(&sc->target_lock);
            CE_state->send_cb((struct CE_handle *)CE_state, CE_context, transfer_context, buf, nbytes, id,
                              sw_idx, hw_idx);
            adf_os_spin_lock(&sc->target_lock);
        }
#endif /*ATH_11AC_TXCOMPACT*/
    }

more_watermarks:
    if (CE_state->misc_cbs) {
        CE_int_status = CE_ENGINE_INT_STATUS_GET(targid, ctrl_addr);
        if (CE_int_status & CE_WATERMARK_MASK) {
            if (CE_state->watermark_cb) {

                adf_os_spin_unlock(&sc->target_lock);
                /* Convert HW IS bits to software flags */
                flags = (CE_int_status & CE_WATERMARK_MASK) >> CE_WM_SHFT;

                CE_state->watermark_cb((struct CE_handle *)CE_state, CE_state->wm_context, flags);
                adf_os_spin_lock(&sc->target_lock);
            }
        }
    }

    /*
     * Clear the misc interrupts (watermark) that were handled above,
     * and that will be checked again below.
     * Clear and check for copy-complete interrupts again, just in case
     * more copy completions happened while the misc interrupts were being
     * handled.
     */
    CE_ENGINE_INT_STATUS_CLEAR(targid, ctrl_addr, CE_WATERMARK_MASK | HOST_IS_COPY_COMPLETE_MASK);

    /*
     * Now that per-engine interrupts are cleared, verify that
     * no recv interrupts arrive while processing send interrupts,
     * and no recv or send interrupts happened while processing
     * misc interrupts.Go back and check again.Keep checking until
     * we find no more events to process.
     */
    if (CE_state->recv_cb && CE_recv_entries_done_nolock(sc, CE_state)) {
        if (WLAN_IS_EPPING_ENABLED(vos_get_conparam()) ||
            more_comp_cnt++ < CE_TXRX_COMP_CHECK_THRESHOLD) {
            goto more_completions;
        } else {
            adf_os_print("%s:Potential infinite loop detected during Rx processing"
                         "nentries_mask:0x%x sw read_idx:0x%x hw read_idx:0x%x\n",
                        __func__, CE_state->dest_ring->nentries_mask,
                        CE_state->dest_ring->sw_index,
                        CE_DEST_RING_READ_IDX_GET(targid, CE_state->ctrl_addr));
        }
    }

    if (CE_state->send_cb && CE_send_entries_done_nolock(sc, CE_state)) {
        if (WLAN_IS_EPPING_ENABLED(vos_get_conparam()) ||
            more_snd_comp_cnt++ < CE_TXRX_COMP_CHECK_THRESHOLD) {
            goto more_completions;
        } else {
            adf_os_print("%s:Potential infinite loop detected during send completion"
                         "nentries_mask:0x%x sw read_idx:0x%x hw read_idx:0x%x\n",
                         __func__, CE_state->src_ring->nentries_mask,
                         CE_state->src_ring->sw_index,
                         CE_SRC_RING_READ_IDX_GET(targid, CE_state->ctrl_addr));
        }
    }


    if (CE_state->misc_cbs) {
        CE_int_status = CE_ENGINE_INT_STATUS_GET(targid, ctrl_addr);
        if (CE_int_status & CE_WATERMARK_MASK) {
            if (CE_state->watermark_cb) {
                goto more_watermarks;
            }
        }
    }

    adf_os_spin_unlock(&sc->target_lock);
    adf_os_atomic_set(&CE_state->rx_pending, 0);
    A_TARGET_ACCESS_END(targid);
}

static void
CE_poll_timeout(void *arg)
{
    struct CE_state *CE_state = (struct CE_state *) arg;
    if (CE_state->timer_inited) {
        CE_per_engine_service(CE_state->sc, CE_state->id);
        adf_os_timer_mod(&CE_state->poll_timer, CE_POLL_TIMEOUT);
    }
}


/*
 * Handler for per-engine interrupts on ALL active CEs.
 * This is used in cases where the system is sharing a
 * single interrput for all CEs
 */

void
CE_per_engine_service_any(int irq, void *arg)
{
    struct hif_pci_softc *sc = arg;
    A_target_id_t targid = TARGID(sc);
    int CE_id;
    A_UINT32 intr_summary;

    A_TARGET_ACCESS_BEGIN(targid);
    if (!adf_os_atomic_read(&sc->tasklet_from_intr)) {
        for (CE_id=0; CE_id < sc->ce_count; CE_id++) {
             struct CE_state *CE_state = sc->CE_id_to_state[CE_id];
             if (adf_os_atomic_read(&CE_state->rx_pending)) {
                 adf_os_atomic_set(&CE_state->rx_pending, 0);
                 CE_per_engine_service(sc, CE_id);
             }
        }

        A_TARGET_ACCESS_END(targid);
        return;
    }

    intr_summary = CE_INTERRUPT_SUMMARY(targid);

    for (CE_id=0; intr_summary && (CE_id < sc->ce_count); CE_id++) {
        if (intr_summary & (1<<CE_id)) {
            intr_summary &= ~(1<<CE_id);
        } else {
            continue; /* no intr pending on this CE */
        }

        CE_per_engine_service(sc, CE_id);
    }

    A_TARGET_ACCESS_END(targid);
}

/*
 * Adjust interrupts for the copy complete handler.
 * If it's needed for either send or recv, then unmask
 * this interrupt; otherwise, mask it.
 *
 * Called with target_lock held.
 */
static void
CE_per_engine_handler_adjust(struct CE_state *CE_state,
                             int disable_copy_compl_intr)
{
    u_int32_t ctrl_addr = CE_state->ctrl_addr;
    struct hif_pci_softc *sc = CE_state->sc;
    A_target_id_t targid = TARGID(sc);
    CE_state->disable_copy_compl_intr = disable_copy_compl_intr;

    A_TARGET_ACCESS_BEGIN(targid);
    if ((!disable_copy_compl_intr) &&
        (CE_state->send_cb || CE_state->recv_cb))
    {
        CE_COPY_COMPLETE_INTR_ENABLE(targid, ctrl_addr);
    } else {
        CE_COPY_COMPLETE_INTR_DISABLE(targid, ctrl_addr);
    }

    if (CE_state->watermark_cb) {
        CE_WATERMARK_INTR_ENABLE(targid, ctrl_addr);
    } else {
        CE_WATERMARK_INTR_DISABLE(targid, ctrl_addr);
    }
    A_TARGET_ACCESS_END(targid);

}

/*Iterate the CE_state list and disable the compl interrupt if it has been registered already.*/
void CE_disable_any_copy_compl_intr_nolock(struct hif_pci_softc *sc)
{
    A_target_id_t targid = TARGID(sc);
    int CE_id;

    A_TARGET_ACCESS_BEGIN(targid);
    for (CE_id=0; CE_id < sc->ce_count; CE_id++) {
        struct CE_state *CE_state = sc->CE_id_to_state[CE_id];
        u_int32_t ctrl_addr = CE_state->ctrl_addr;

        /* if the interrupt is currently enabled, disable it */
        if (!CE_state->disable_copy_compl_intr && (CE_state->send_cb || CE_state->recv_cb)) {
            CE_COPY_COMPLETE_INTR_DISABLE(targid, ctrl_addr);
        }

        if (CE_state->watermark_cb) {
            CE_WATERMARK_INTR_DISABLE(targid, ctrl_addr);
        }
    }
    A_TARGET_ACCESS_END(targid);
}

void CE_enable_any_copy_compl_intr_nolock(struct hif_pci_softc *sc)
{
    A_target_id_t targid = TARGID(sc);
    int CE_id;

    A_TARGET_ACCESS_BEGIN(targid);
    for (CE_id=0; CE_id < sc->ce_count; CE_id++) {
        struct CE_state *CE_state = sc->CE_id_to_state[CE_id];
        u_int32_t ctrl_addr = CE_state->ctrl_addr;

        /*
        * If the CE is supposed to have copy complete interrupts enabled
        * (i.e. there a callback registered, and the "disable" flag is not set),
        * then re-enable the interrupt.
        */
        if (!CE_state->disable_copy_compl_intr && (CE_state->send_cb || CE_state->recv_cb)) {
            CE_COPY_COMPLETE_INTR_ENABLE(targid, ctrl_addr);
        }

        if (CE_state->watermark_cb) {
            CE_WATERMARK_INTR_ENABLE(targid, ctrl_addr);
        }
    }
    A_TARGET_ACCESS_END(targid);
}

void
CE_disable_any_copy_compl_intr(struct hif_pci_softc *sc)
{
	adf_os_spin_lock(&sc->target_lock);
	CE_disable_any_copy_compl_intr_nolock(sc);
	adf_os_spin_unlock(&sc->target_lock);
}

/*Re-enable the copy compl interrupt if it has not been disabled before.*/
void
CE_enable_any_copy_compl_intr(struct hif_pci_softc *sc)
{
	adf_os_spin_lock(&sc->target_lock);
	CE_enable_any_copy_compl_intr_nolock(sc);
	adf_os_spin_unlock(&sc->target_lock);
}

void
CE_send_cb_register(struct CE_handle *copyeng,
                    CE_send_cb fn_ptr,
                    void *CE_send_context,
                    int disable_interrupts)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct hif_pci_softc *sc = CE_state->sc;

    adf_os_spin_lock(&sc->target_lock);
    CE_state->send_cb = fn_ptr;
    CE_state->send_context = CE_send_context;
    CE_per_engine_handler_adjust(CE_state, disable_interrupts);
    adf_os_spin_unlock(&sc->target_lock);
}

void
CE_recv_cb_register(struct CE_handle *copyeng,
                    CE_recv_cb fn_ptr,
                    void *CE_recv_context,
                    int disable_interrupts)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct hif_pci_softc *sc = CE_state->sc;

    adf_os_spin_lock(&sc->target_lock);
    CE_state->recv_cb = fn_ptr;
    CE_state->recv_context = CE_recv_context;
    CE_per_engine_handler_adjust(CE_state, disable_interrupts);
    adf_os_spin_unlock(&sc->target_lock);
}

void
CE_watermark_cb_register(struct CE_handle *copyeng,
                         CE_watermark_cb fn_ptr,
                         void *CE_wm_context)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    struct hif_pci_softc *sc = CE_state->sc;

    adf_os_spin_lock(&sc->target_lock);
    CE_state->watermark_cb = fn_ptr;
    CE_state->wm_context = CE_wm_context;
    CE_per_engine_handler_adjust(CE_state, 0);
    if (fn_ptr) {
        CE_state->misc_cbs = 1;
    }
    adf_os_spin_unlock(&sc->target_lock);
}

static unsigned int
roundup_pwr2(unsigned int n)
{
    int i;
    unsigned int test_pwr2;

    if (!(n & (n-1))) {
        return n; /* already a power of 2 */
    }

    test_pwr2 = 4;
    for (i=0; i<29; i++) {
        if (test_pwr2 > n) {
            return test_pwr2;
        }
        test_pwr2 = test_pwr2 << 1;
    }

    A_ASSERT(0); /* n too large */
    return 0;
}

bool CE_get_rx_pending(struct hif_pci_softc *sc)
{
    int CE_id;

    for (CE_id=0; CE_id < sc->ce_count; CE_id++) {
         struct CE_state *CE_state = sc->CE_id_to_state[CE_id];
         if (adf_os_atomic_read(&CE_state->rx_pending))
             return true;
    }

    return false;
}
/*
 * Initialize a Copy Engine based on caller-supplied attributes.
 * This may be called once to initialize both source and destination
 * rings or it may be called twice for separate source and destination
 * initialization. It may be that only one side or the other is
 * initialized by software/firmware.
 */
struct CE_handle *
CE_init(struct hif_pci_softc *sc,
        unsigned int CE_id,
        struct CE_attr *attr)
{
    struct CE_state *CE_state;
    u_int32_t ctrl_addr;
    A_target_id_t targid;
    unsigned int nentries;
    adf_os_dma_addr_t base_addr;
    struct ol_softc *scn = sc->ol_sc;
    bool malloc_CE_state = false;
    bool malloc_src_ring = false;

    A_ASSERT(CE_id < sc->ce_count);
    ctrl_addr = CE_BASE_ADDRESS(CE_id);
    adf_os_spin_lock(&sc->target_lock);
    CE_state = sc->CE_id_to_state[CE_id];


    if (!CE_state) {
        adf_os_spin_unlock(&sc->target_lock);
        CE_state = (struct CE_state *)A_MALLOC(sizeof(*CE_state));
        if (!CE_state) {
            dev_err(&sc->pdev->dev, "ath ERROR: CE_state has no mem\n");
            return NULL;
        } else
            malloc_CE_state = true;
        A_MEMZERO(CE_state, sizeof(*CE_state));
        adf_os_spin_lock(&sc->target_lock);
        if (!sc->CE_id_to_state[CE_id]) { /* re-check under lock */
            sc->CE_id_to_state[CE_id] = CE_state;

            CE_state->sc = sc;
            CE_state->id = CE_id;
            CE_state->ctrl_addr = ctrl_addr;
            CE_state->state = CE_RUNNING;
            CE_state->attr_flags = attr->flags; /* Save attribute flags */
        } else {
            /*
             * We released target_lock in order to allocate CE state,
             * but someone else beat us to it.  Continue, using that
             * CE_state (and free the one we allocated).
             */
            A_FREE(CE_state);
            malloc_CE_state = false;
            CE_state = sc->CE_id_to_state[CE_id];
        }
    }
    adf_os_spin_unlock(&sc->target_lock);

    adf_os_atomic_init(&CE_state->rx_pending);
    if (attr == NULL) {
        /* Already initialized; caller wants the handle */
        return (struct CE_handle *)CE_state;
    }

    targid = TARGID(sc);

    if (CE_state->src_sz_max) {
        A_ASSERT(CE_state->src_sz_max == attr->src_sz_max);
    } else {
        CE_state->src_sz_max = attr->src_sz_max;
    }

    CE_init_ce_desc_event_log(CE_id,
               attr->src_nentries + attr->dest_nentries);

    /* source ring setup */
    nentries = attr->src_nentries;
    if (nentries) {
        struct CE_ring_state *src_ring;
        unsigned CE_nbytes;
        char *ptr;

        nentries = roundup_pwr2(nentries);
        if (CE_state->src_ring) {
            A_ASSERT(CE_state->src_ring->nentries == nentries);
        } else {
            CE_nbytes = sizeof(struct CE_ring_state)
                      + (nentries * sizeof(void *)); /* per-send context */
            ptr = A_MALLOC(CE_nbytes);
            if (!ptr) {
                /* cannot allocate src ring. If the CE_state is allocated
                 * locally free CE_State and return error. */
                dev_err(&sc->pdev->dev, "ath ERROR: src ring has no mem\n");
                if (malloc_CE_state) {
                    /* allocated CE_state locally */
                    adf_os_spin_lock(&sc->target_lock);
                    sc->CE_id_to_state[CE_id]= NULL;
                    adf_os_spin_unlock(&sc->target_lock);
                    A_FREE(CE_state);
                    malloc_CE_state = false;
                }
                return NULL;
            } else {
                /* we can allocate src ring.
                 * Mark that the src ring is allocated locally */
                malloc_src_ring = true;
            }
            A_MEMZERO(ptr, CE_nbytes);

            src_ring = CE_state->src_ring = (struct CE_ring_state *)ptr;
            ptr += sizeof(struct CE_ring_state);
            src_ring->nentries = nentries;
            src_ring->nentries_mask = nentries-1;
            A_TARGET_ACCESS_BEGIN_RET_PTR(targid);
            src_ring->hw_index =
                src_ring->sw_index = CE_SRC_RING_READ_IDX_GET(targid, ctrl_addr);
            src_ring->write_index = CE_SRC_RING_WRITE_IDX_GET(targid, ctrl_addr);
            A_TARGET_ACCESS_END_RET_PTR(targid);
            src_ring->low_water_mark_nentries = 0;
            src_ring->high_water_mark_nentries = nentries;
            src_ring->per_transfer_context = (void **)ptr;

            /* Legacy platforms that do not support cache coherent DMA are unsupported */
            src_ring->base_addr_owner_space_unaligned =
                pci_alloc_consistent(scn->sc_osdev->bdev,
                                    (nentries * sizeof(struct CE_src_desc) + CE_DESC_RING_ALIGN),
                                    &base_addr);
            if (src_ring->base_addr_owner_space_unaligned == NULL) {
                dev_err(&sc->pdev->dev, "ath ERROR: src ring has no DMA mem\n");
                goto error_no_dma_mem;
            }
            src_ring->base_addr_CE_space_unaligned = base_addr;


            if (src_ring->base_addr_CE_space_unaligned & (CE_DESC_RING_ALIGN-1)) {

                src_ring->base_addr_CE_space = (src_ring->base_addr_CE_space_unaligned +
                        CE_DESC_RING_ALIGN-1) & ~(CE_DESC_RING_ALIGN-1);

                src_ring->base_addr_owner_space = (void *)(((size_t)src_ring->base_addr_owner_space_unaligned +
                        CE_DESC_RING_ALIGN-1) & ~(CE_DESC_RING_ALIGN-1));
            } else {
                src_ring->base_addr_CE_space = src_ring->base_addr_CE_space_unaligned;
                src_ring->base_addr_owner_space = src_ring->base_addr_owner_space_unaligned;
            }
            /*
             * Also allocate a shadow src ring in regular mem to use for
             * faster access.
             */
            src_ring->shadow_base_unaligned = A_MALLOC(
                nentries * sizeof(struct CE_src_desc) + CE_DESC_RING_ALIGN);
            if (src_ring->shadow_base_unaligned == NULL) {
                dev_err(&sc->pdev->dev, "ath ERROR: src ring has no shadow_base mem\n");
                goto error_no_dma_mem;
            }
            src_ring->shadow_base = (struct CE_src_desc *)
                (((size_t) src_ring->shadow_base_unaligned +
                CE_DESC_RING_ALIGN-1) & ~(CE_DESC_RING_ALIGN-1));

            A_TARGET_ACCESS_BEGIN_RET_PTR(targid);
            CE_SRC_RING_BASE_ADDR_SET(targid, ctrl_addr, src_ring->base_addr_CE_space);
            CE_SRC_RING_SZ_SET(targid, ctrl_addr, nentries);
            CE_SRC_RING_DMAX_SET(targid, ctrl_addr, attr->src_sz_max);
#ifdef BIG_ENDIAN_HOST
            /* Enable source ring byte swap for big endian host */
            CE_SRC_RING_BYTE_SWAP_SET(targid, ctrl_addr, 1);
#endif
            CE_SRC_RING_LOWMARK_SET(targid, ctrl_addr, 0);
            CE_SRC_RING_HIGHMARK_SET(targid, ctrl_addr, nentries);
            A_TARGET_ACCESS_END_RET_PTR(targid);
        }
    }

    /* destination ring setup */
    nentries = attr->dest_nentries;
    if (nentries) {
        struct CE_ring_state *dest_ring;
        unsigned CE_nbytes;
        char *ptr;

        nentries = roundup_pwr2(nentries);
        if (CE_state->dest_ring) {
            A_ASSERT(CE_state->dest_ring->nentries == nentries);
        } else {
            CE_nbytes = sizeof(struct CE_ring_state)
                      + (nentries * sizeof(void *)); /* per-recv context */
            ptr = A_MALLOC(CE_nbytes);
            if (!ptr) {
                /* cannot allocate dst ring. If the CE_state or src ring is allocated
                 * locally free CE_State and src ring and return error. */
                dev_err(&sc->pdev->dev, "ath ERROR: dest ring has no mem\n");
                if (malloc_src_ring) {
                    A_FREE(CE_state->src_ring);
                    CE_state->src_ring= NULL;
                    malloc_src_ring = false;
                }
                if (malloc_CE_state) {
                    /* allocated CE_state locally */
                    adf_os_spin_lock(&sc->target_lock);
                    sc->CE_id_to_state[CE_id]= NULL;
                    adf_os_spin_unlock(&sc->target_lock);
                    A_FREE(CE_state);
                    malloc_CE_state = false;
                }
                return NULL;
            }
            A_MEMZERO(ptr, CE_nbytes);

            dest_ring = CE_state->dest_ring = (struct CE_ring_state *)ptr;
            ptr += sizeof(struct CE_ring_state);
            dest_ring->nentries = nentries;
            dest_ring->nentries_mask = nentries-1;
            A_TARGET_ACCESS_BEGIN_RET_PTR(targid);
            dest_ring->sw_index = CE_DEST_RING_READ_IDX_GET(targid, ctrl_addr);
            dest_ring->write_index = CE_DEST_RING_WRITE_IDX_GET(targid, ctrl_addr);
            A_TARGET_ACCESS_END_RET_PTR(targid);
            dest_ring->low_water_mark_nentries = 0;
            dest_ring->high_water_mark_nentries = nentries;
            dest_ring->per_transfer_context = (void **)ptr;

            /* Legacy platforms that do not support cache coherent DMA are unsupported */
            dest_ring->base_addr_owner_space_unaligned =
                pci_alloc_consistent(scn->sc_osdev->bdev,
                                    (nentries * sizeof(struct CE_dest_desc) + CE_DESC_RING_ALIGN),
                                    &base_addr);
            if (dest_ring->base_addr_owner_space_unaligned == NULL) {
                dev_err(&sc->pdev->dev, "ath ERROR: dest ring has no DMA mem\n");
                goto error_no_dma_mem;
            }
            dest_ring->base_addr_CE_space_unaligned = base_addr;

            /* Correctly initialize memory to 0 to prevent garbage data
             * crashing system when download firmware
             */
            A_MEMZERO(dest_ring->base_addr_owner_space_unaligned, nentries * sizeof(struct CE_dest_desc) + CE_DESC_RING_ALIGN);

            if (dest_ring->base_addr_CE_space_unaligned & (CE_DESC_RING_ALIGN-1)) {

                dest_ring->base_addr_CE_space = (dest_ring->base_addr_CE_space_unaligned +
                        CE_DESC_RING_ALIGN-1) & ~(CE_DESC_RING_ALIGN-1);

                dest_ring->base_addr_owner_space = (void *)(((size_t)dest_ring->base_addr_owner_space_unaligned +
                        CE_DESC_RING_ALIGN-1) & ~(CE_DESC_RING_ALIGN-1));
            } else {
                dest_ring->base_addr_CE_space = dest_ring->base_addr_CE_space_unaligned;
                dest_ring->base_addr_owner_space = dest_ring->base_addr_owner_space_unaligned;
            }

            A_TARGET_ACCESS_BEGIN_RET_PTR(targid);
            CE_DEST_RING_BASE_ADDR_SET(targid, ctrl_addr, dest_ring->base_addr_CE_space);
            CE_DEST_RING_SZ_SET(targid, ctrl_addr, nentries);
#ifdef BIG_ENDIAN_HOST
            /* Enable Destination ring byte swap for big endian host */
            CE_DEST_RING_BYTE_SWAP_SET(targid, ctrl_addr, 1);
#endif
            CE_DEST_RING_LOWMARK_SET(targid, ctrl_addr, 0);
            CE_DEST_RING_HIGHMARK_SET(targid, ctrl_addr, nentries);
            A_TARGET_ACCESS_END_RET_PTR(targid);

            /* epping */
            /* poll timer */
            if ((CE_state->attr_flags & CE_ATTR_ENABLE_POLL)) {
                adf_os_timer_init(scn->adf_dev, &CE_state->poll_timer,
                        CE_poll_timeout, CE_state, ADF_DEFERRABLE_TIMER);
                CE_state->timer_inited = true;
                adf_os_timer_mod(&CE_state->poll_timer, CE_POLL_TIMEOUT);
            }
        }
    }

    /* Enable CE error interrupts */
    A_TARGET_ACCESS_BEGIN_RET_PTR(targid);
    CE_ERROR_INTR_ENABLE(targid, ctrl_addr);
    A_TARGET_ACCESS_END_RET_PTR(targid);

    return (struct CE_handle *)CE_state;

error_no_dma_mem:
    CE_fini((struct CE_handle *)CE_state);
    return NULL;
}

void
CE_fini(struct CE_handle *copyeng)
{
    struct CE_state *CE_state = (struct CE_state *)copyeng;
    unsigned int CE_id = CE_state->id;
    struct hif_pci_softc *sc = CE_state->sc;
    struct ol_softc *scn = sc->ol_sc;

    CE_state->state = CE_UNUSED;
    CE_state->sc->CE_id_to_state[CE_id] = NULL;
    if (CE_state->src_ring) {
        if (CE_state->src_ring->shadow_base_unaligned)
            A_FREE(CE_state->src_ring->shadow_base_unaligned);
        if (CE_state->src_ring->base_addr_owner_space_unaligned)
            pci_free_consistent(scn->sc_osdev->bdev,
                   (CE_state->src_ring->nentries * sizeof(struct CE_src_desc) + CE_DESC_RING_ALIGN),
                   CE_state->src_ring->base_addr_owner_space_unaligned, CE_state->src_ring->base_addr_CE_space);
        A_FREE(CE_state->src_ring);
    }
    if (CE_state->dest_ring) {
        if (CE_state->dest_ring->base_addr_owner_space_unaligned)
            pci_free_consistent(scn->sc_osdev->bdev,
                   (CE_state->dest_ring->nentries * sizeof(struct CE_dest_desc) + CE_DESC_RING_ALIGN),
                   CE_state->dest_ring->base_addr_owner_space_unaligned, CE_state->dest_ring->base_addr_CE_space);
        A_FREE(CE_state->dest_ring);

        /* epping */
        if (CE_state->timer_inited) {
            CE_state->timer_inited = false;
            adf_os_timer_free(&CE_state->poll_timer);
        }
    }
    A_FREE(CE_state);
}

#ifdef IPA_UC_OFFLOAD
/*
 * Copy engine should release resource to micro controller
 * Micro controller needs
   - Copy engine source descriptor base address
   - Copy engine source descriptor size
   - PCI BAR address to access copy engine regiser
 */
void CE_ipaGetResource(struct CE_handle *ce,
            a_uint32_t *ce_sr_base_paddr,
            a_uint32_t *ce_sr_ring_size,
            a_uint32_t *ce_reg_paddr)
{
    struct CE_state *CE_state = (struct CE_state *)ce;
    a_uint32_t ring_loop;
    struct CE_src_desc *ce_desc;
    a_uint32_t bar_value;
    struct hif_pci_softc *sc = CE_state->sc;

    if (CE_RUNNING != CE_state->state)
    {
        *ce_sr_base_paddr = 0;
        *ce_sr_ring_size = 0;
        return;
    }

    /* Update default value for descriptor */
    for (ring_loop = 0; ring_loop < CE_state->src_ring->nentries; ring_loop++)
    {
        ce_desc = (struct CE_src_desc *)
                  ((char *)CE_state->src_ring->base_addr_owner_space +
                   ring_loop * (sizeof(struct CE_src_desc)));
        /* Source pointer and ID,
         * should be updated by uc dynamically
         * ce_desc->src_ptr   = buffer;
         * ce_desc->meta_data = transfer_id; */
        /* No Byte SWAP */
        ce_desc->byte_swap = 0;
        /* DL size
         * pdev->download_len =
         *   sizeof(struct htt_host_tx_desc_t) +
         *   HTT_TX_HDR_SIZE_OUTER_HDR_MAX +
         *   HTT_TX_HDR_SIZE_802_1Q +
         *   HTT_TX_HDR_SIZE_LLC_SNAP +
         *   ol_cfg_tx_download_size(pdev->ctrl_pdev); */
        ce_desc->nbytes = 60;
        /* Single fragment No gather */
        ce_desc->gather = 0;
    }

    /* Get BAR address */
    hif_read_bar(CE_state->sc, &bar_value);

    *ce_sr_base_paddr = (a_uint32_t)CE_state->src_ring->base_addr_CE_space;
    *ce_sr_ring_size = (a_uint32_t)CE_state->src_ring->nentries;
    *ce_reg_paddr = bar_value + CE_BASE_ADDRESS(CE_state->id) + SR_WR_INDEX_ADDRESS;
    return;
}
#endif /* IPA_UC_OFFLOAD */
