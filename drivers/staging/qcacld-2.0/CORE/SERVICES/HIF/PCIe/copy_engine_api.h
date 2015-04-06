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

#ifndef __COPY_ENGINE_API_H__
#define __COPY_ENGINE_API_H__

#if defined(CONFIG_COPY_ENGINE_SUPPORT)

/* TBDXXX: Use int return values for consistency with Target */

/* TBDXXX: Perhaps merge Host/Target-->common */

/*
 * Copy Engine support: low-level Target-side Copy Engine API.
 * This is a hardware access layer used by code that understands
 * how to use copy engines.
 */

/*
 * A "struct CE_handle *" serves as an opaque pointer-sized
 * handle to a specific copy engine.
 */
struct CE_handle;

/*
 * "Send Completion" callback type for Send Completion Notification.
 *
 * If a Send Completion callback is registered and one or more sends
 * have completed, the callback is invoked.
 *
 * per_CE_send_context is a context supplied by the calling layer
 * (via CE_send_cb_register). It is associated with a copy engine.
 *
 * per_transfer_send_context is context supplied by the calling layer
 * (via the "send" call).  It may be different for each invocation
 * of send.
 *
 * The buffer parameter is the first byte sent of the first buffer
 * sent (if more than one buffer).
 *
 * nbytes is the number of bytes of that buffer that were sent.
 *
 * transfer_id matches the value used when the buffer or
 * buf_list was sent.
 *
 * Implementation note: Pops 1 completed send buffer from Source ring
 */
typedef void (* CE_send_cb)(struct CE_handle *copyeng,
                            void *per_CE_send_context,
                            void *per_transfer_send_context,
                            CE_addr_t buffer,
                            unsigned int nbytes,
                            unsigned int transfer_id,
                            unsigned int sw_index,
                            unsigned int hw_index);

/*
 * "Buffer Received" callback type for Buffer Received Notification.
 *
 * Implementation note: Pops 1 completed recv buffer from Dest ring
 */
typedef void (* CE_recv_cb)(struct CE_handle *copyeng,
                            void *per_CE_recv_context,
                            void *per_transfer_recv_context,
                            CE_addr_t buffer,
                            unsigned int nbytes,
                            unsigned int transfer_id,
                            unsigned int flags);

/*
 * Copy Engine Watermark callback type.
 *
 * Allows upper layers to be notified when watermarks are reached:
 *   space is available and/or running short in a source ring
 *   buffers are exhausted and/or abundant in a destination ring
 *
 * The flags parameter indicates which condition triggered this
 * callback.  See CE_WM_FLAG_*.
 *
 * Watermark APIs are provided to allow upper layers "batch"
 * descriptor processing and to allow upper layers to
 * throttle/unthrottle.
 */
typedef void (* CE_watermark_cb)(struct CE_handle *copyeng,
                                 void *per_CE_wm_context,
                                 unsigned int flags);

#define CE_WM_FLAG_SEND_HIGH   1
#define CE_WM_FLAG_SEND_LOW    2
#define CE_WM_FLAG_RECV_HIGH   4
#define CE_WM_FLAG_RECV_LOW    8

/* A list of buffers to be gathered and sent */
struct CE_sendlist;

/* Copy Engine settable attributes */
struct CE_attr;

/*==================Send======================================================*/

             /* CE_send flags */
/* disable ring's byte swap, even if the default policy is to swap */
#define CE_SEND_FLAG_SWAP_DISABLE        1

/*
 * Queue a source buffer to be sent to an anonymous destination buffer.
 *   copyeng         - which copy engine to use
 *   buffer          - address of buffer
 *   nbytes          - number of bytes to send
 *   transfer_id     - arbitrary ID; reflected to destination
 *   flags           - CE_SEND_FLAG_* values
 * Returns 0 on success; otherwise an error status.
 *
 * Note: If no flags are specified, use CE's default data swap mode.
 *
 * Implementation note: pushes 1 buffer to Source ring
 */
int CE_send(struct CE_handle *copyeng,
                 void *per_transfer_send_context,
                 CE_addr_t buffer,
                 unsigned int nbytes,
                 unsigned int transfer_id, /* 14 bits */
                 unsigned int flags);


/*
 * Register a Send Callback function.
 * This function is called as soon as the contents of a Send
 * have reached the destination, unless disable_interrupts is
 * requested.  In this case, the callback is invoked when the
 * send status is polled, shortly after the send completes.
 */
void CE_send_cb_register(struct CE_handle *copyeng,
                         CE_send_cb fn_ptr,
                         void *per_CE_send_context,
                         int disable_interrupts);

/*
 * Return the size of a SendList. This allows the caller to allocate
 * a SendList while the SendList structure remains opaque.
 */
unsigned int CE_sendlist_sizeof(void);

/* Initialize a sendlist */
void CE_sendlist_init(struct CE_sendlist *sendlist);

/* Append a simple buffer (address/length) to a sendlist. */
int CE_sendlist_buf_add(struct CE_sendlist *sendlist,
                         CE_addr_t buffer,
                         unsigned int nbytes,
                         u_int32_t flags /* OR-ed with internal flags */);

/*
 * Queue a "sendlist" of buffers to be sent using gather to a single
 * anonymous destination buffer
 *   copyeng         - which copy engine to use
 *   sendlist        - list of simple buffers to send using gather
 *   transfer_id     - arbitrary ID; reflected to destination
 * Returns 0 on success; otherwise an error status.
 *
 * Implemenation note: Pushes multiple buffers with Gather to Source ring.
 */
int CE_sendlist_send(struct CE_handle *copyeng,
                          void *per_transfer_send_context,
                          struct CE_sendlist *sendlist,
                          unsigned int transfer_id); /* 14 bits */

/*==================Recv======================================================*/

/*
 * Make a buffer available to receive. The buffer must be at least of a
 * minimal size appropriate for this copy engine (src_sz_max attribute).
 *   copyeng                    - which copy engine to use
 *   per_transfer_recv_context  - context passed back to caller's recv_cb
 *   buffer                     - address of buffer in CE space
 * Returns 0 on success; otherwise an error status.
 *
 * Implemenation note: Pushes a buffer to Dest ring.
 */
int CE_recv_buf_enqueue(struct CE_handle *copyeng,
                             void *per_transfer_recv_context,
                             CE_addr_t buffer);

/*
 * Register a Receive Callback function.
 * This function is called as soon as data is received
 * from the source.
 */
void CE_recv_cb_register(struct CE_handle *copyeng,
                         CE_recv_cb fn_ptr,
                         void *per_CE_recv_context,
                         int disable_interrupts);

/*==================CE Watermark==============================================*/

/*
 * Register a Watermark Callback function.
 * This function is called as soon as a watermark level
 * is crossed.  A Watermark Callback function is free to
 * handle received data "en masse"; but then some coordination
 * is required with a registered Receive Callback function.
 * [Suggestion: Either handle Receives in a Receive Callback
 * or en masse in a Watermark Callback; but not both.]
 */
void CE_watermark_cb_register(struct CE_handle *copyeng,
                              CE_watermark_cb fn_ptr,
                              void *per_CE_wm_context);

/*
 * Set low/high watermarks for the send/source side of a copy engine.
 *
 * Typically, the destination side CPU manages watermarks for
 * the receive side and the source side CPU manages watermarks
 * for the send side.
 *
 * A low watermark of 0 is never hit (so the watermark function
 * will never be called for a Low Watermark condition).
 *
 * A high watermark equal to nentries is never hit (so the
 * watermark function will never be called for a High Watermark
 * condition).
 */
void CE_send_watermarks_set(struct CE_handle *copyeng,
                            unsigned int low_alert_nentries,
                            unsigned int high_alert_nentries);

/* Set low/high watermarks for the receive/destination side of a copy engine. */
void CE_recv_watermarks_set(struct CE_handle *copyeng,
                            unsigned int low_alert_nentries,
                            unsigned int high_alert_nentries);

/*
 * Return the number of entries that can be queued
 * to a ring at an instant in time.
 *
 * For source ring, does not imply that destination-side
 * buffers are available; merely indicates descriptor space
 * in the source ring.
 *
 * For destination ring, does not imply that previously
 * received buffers have been processed; merely indicates
 * descriptor space in destination ring.
 *
 * Mainly for use with CE Watermark callback.
 */
unsigned int CE_send_entries_avail(struct CE_handle *copyeng);
unsigned int CE_recv_entries_avail(struct CE_handle *copyeng);

/*
 * Return the number of entries in the ring that are ready
 * to be processed by software.
 *
 * For source ring, the number of descriptors that have
 * been completed and can now be overwritten with new send
 * descriptors.
 *
 * For destination ring, the number of descriptors that
 * are available to be processed (newly received buffers).
 */
unsigned int CE_send_entries_done(struct CE_handle *copyeng);
unsigned int CE_recv_entries_done(struct CE_handle *copyeng);

             /* recv flags */
/* Data is byte-swapped */
#define CE_RECV_FLAG_SWAPPED            1

/*
 * Supply data for the next completed unprocessed receive descriptor.
 *
 * For use
 *    with CE Watermark callback,
 *    in a recv_cb function when processing buf_lists
 *    in a recv_cb function in order to mitigate recv_cb's.
 *
 * Implemenation note: Pops buffer from Dest ring.
 */
int CE_completed_recv_next(struct CE_handle *copyeng,
                                void **per_CE_contextp,
                                void **per_transfer_contextp,
                                CE_addr_t *bufferp,
                                unsigned int *nbytesp,
                                unsigned int *transfer_idp,
                                unsigned int *flagsp);

/*
 * Supply data for the next completed unprocessed send descriptor.
 *
 * For use
 *    with CE Watermark callback
 *    in a send_cb function in order to mitigate send_cb's.
 *
 * Implementation note: Pops 1 completed send buffer from Source ring
 */
int CE_completed_send_next(struct CE_handle *copyeng,
                                void **per_CE_contextp,
                                void **per_transfer_contextp,
                                CE_addr_t *bufferp,
                                unsigned int *nbytesp,
                                unsigned int *transfer_idp,
                                unsigned int *sw_idx,
                                unsigned int *hw_idx);


/*==================CE Engine Initialization==================================*/

/* Initialize an instance of a CE */
struct CE_handle *CE_init(struct hif_pci_softc *sc,
                          unsigned int CE_id,
                          struct CE_attr *attr);


/*==================CE Engine Shutdown========================================*/
/*
 * Support clean shutdown by allowing the caller to revoke
 * receive buffers.  Target DMA must be stopped before using
 * this API.
 */
A_STATUS
CE_revoke_recv_next(struct CE_handle *copyeng,
                    void **per_CE_contextp,
                    void **per_transfer_contextp,
                    CE_addr_t *bufferp);

/*
 * Support clean shutdown by allowing the caller to cancel
 * pending sends.  Target DMA must be stopped before using
 * this API.
 */
A_STATUS
CE_cancel_send_next(struct CE_handle *copyeng,
                    void **per_CE_contextp,
                    void **per_transfer_contextp,
                    CE_addr_t *bufferp,
                    unsigned int *nbytesp,
                    unsigned int *transfer_idp);

void CE_fini(struct CE_handle *copyeng);

/*==================CE Interrupt Handlers=====================================*/
void CE_per_engine_service_any(int irq, void *arg);
void CE_per_engine_service(struct hif_pci_softc *sc, unsigned int CE_id);
void CE_per_engine_servicereap(struct hif_pci_softc *sc, unsigned int CE_id);

/*===================CE cmpl interrupt Enable/Disable ============================*/
void CE_disable_any_copy_compl_intr(struct hif_pci_softc *sc);
void CE_enable_any_copy_compl_intr(struct hif_pci_softc *sc);
void CE_disable_any_copy_compl_intr_nolock(struct hif_pci_softc *sc);
void CE_enable_any_copy_compl_intr_nolock(struct hif_pci_softc *sc);

/* API to check if any of the copy engine pipes has pending frames for prcoessing */
bool CE_get_rx_pending(struct hif_pci_softc *sc);

/* CE_attr.flags values */
#define CE_ATTR_NO_SNOOP                0x01  /* Use NonSnooping PCIe accesses? */
#define CE_ATTR_BYTE_SWAP_DATA          0x02  /* Byte swap data words */
#define CE_ATTR_SWIZZLE_DESCRIPTORS     0x04  /* Swizzle descriptors? */
#define CE_ATTR_DISABLE_INTR            0x08  /* no interrupt on copy completion */
#define CE_ATTR_ENABLE_POLL             0x10  /* poll for residue descriptors */

/* Attributes of an instance of a Copy Engine */
struct CE_attr {
    unsigned int flags;                 /* CE_ATTR_* values */

    unsigned int priority;              /* TBD */

    unsigned int src_nentries;          /* #entries in source ring -
                                           Must be a power of 2 */

    unsigned int src_sz_max;            /* Max source send size for this CE.
                                           This is also the minimum size of
                                           a destination buffer. */

    unsigned int dest_nentries;         /* #entries in destination ring -
                                           Must be a power of 2 */

    void *reserved;                     /* Future use */
};

/*
 * When using sendlist_send to transfer multiple buffer fragments, the
 * transfer context of each fragment, except last one, will be filled
 * with CE_SENDLIST_ITEM_CTXT. CE_completed_send will return success for
 * each fragment done with send and the transfer context would be
 * CE_SENDLIST_ITEM_CTXT. Upper layer could use this to identify the
 * status of a send completion.
 */
#define CE_SENDLIST_ITEM_CTXT   ((void *)0xcecebeef)

/*
 * This is an opaque type that is at least large enough to hold
 * a sendlist. A sendlist can only be accessed through CE APIs,
 * but this allows a sendlist to be allocated on the run-time
 * stack.  TBDXXX: un-opaque would be simpler...
 */
struct CE_sendlist {
    unsigned int word[62];
};

#define ATH_ISR_NOSCHED         0x0000  /* Do not schedule bottom half/DPC                */
#define ATH_ISR_SCHED           0x0001  /* Schedule the bottom half for execution        */
#define ATH_ISR_NOTMINE         0x0002  /* This was not my interrupt - for shared IRQ's    */

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
            a_uint32_t *ce_reg_paddr);
#endif /* IPA_UC_OFFLOAD */

#endif /* CONFIG_COPY_ENGINE_SUPPORT */
#endif /* __COPY_ENGINE_API_H__ */
