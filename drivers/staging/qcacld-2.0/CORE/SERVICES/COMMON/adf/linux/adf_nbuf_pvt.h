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


/*
 * Linux implemenation of skbuf
 */
#ifndef _ADF_CMN_NET_PVT_BUF_H
#define _ADF_CMN_NET_PVT_BUF_H

#include <linux/skbuff.h>
#include <linux/netdevice.h>
#include <linux/dma-mapping.h>
#include <asm/types.h>
#include <asm/scatterlist.h>
#include <adf_os_types.h>

#define __ADF_NBUF_NULL   NULL

/*
 * Use socket buffer as the underlying implentation as skbuf .
 * Linux use sk_buff to represent both packet and data,
 * so we use sk_buffer to represent both skbuf .
 */
typedef struct sk_buff *        __adf_nbuf_t;

typedef void (*__adf_nbuf_callback_fn) (struct sk_buff *skb);
#define OSDEP_EAPOL_TID 6  /* send it on VO queue */

/* CVG_NBUF_MAX_OS_FRAGS -
 * max tx fragments provided by the OS
 */
#define CVG_NBUF_MAX_OS_FRAGS 1

/* CVG_NBUF_MAX_EXTRA_FRAGS -
 * max tx fragments added by the driver
 * The driver will always add one tx fragment (the tx descriptor) and may
 * add a second tx fragment (e.g. a TSO segment's modified IP header).
 */
#define CVG_NBUF_MAX_EXTRA_FRAGS 2

typedef void (*adf_nbuf_trace_update_t)(char *);

struct cvg_nbuf_cb {
    /*
     * Store a pointer to a parent network buffer.
     * This is used during TSO to allow the network buffers used for
     * segments of the jumbo tx frame to reference the jumbo frame's
     * original network buffer, so as each TSO segment's network buffer
     * is freed, the original jumbo tx frame's reference count can be
     * decremented, and ultimately freed once all the segments have been
     * freed.
     */
    union {
        struct sk_buff *parent;
        void *ptr;
#ifdef DEBUG_RX_RING_BUFFER
        uint32_t map_index;
#endif
    } txrx_field;

    /*
     * Store the DMA mapping info for the network buffer fragments
     * provided by the OS.
     */
    u_int32_t mapped_paddr_lo[CVG_NBUF_MAX_OS_FRAGS];

    /*
     * place tx_desc_id after mapped_paddr_lo to avoid cb length overflow
     */
#ifdef CONFIG_HL_SUPPORT
    uint16_t tx_desc_id;
#endif
    /* store extra tx fragments provided by the driver */
    struct {
        /* vaddr -
         * CPU address (a.k.a. virtual address) of the tx fragments added
         * by the driver
         */
        unsigned char *vaddr[CVG_NBUF_MAX_EXTRA_FRAGS];
        /* paddr_lo -
         * bus address (a.k.a. physical address) of the tx fragments added
         * by the driver
         */
        u_int32_t paddr_lo[CVG_NBUF_MAX_EXTRA_FRAGS];
        u_int16_t len[CVG_NBUF_MAX_EXTRA_FRAGS];
        u_int8_t  num; /* how many extra frags has the driver added */
        u_int8_t
            /*
             * Store a wordstream vs. bytestream flag for each extra fragment,
             * plus one more flag for the original fragment(s) of the netbuf.
             */
            wordstream_flags : CVG_NBUF_MAX_EXTRA_FRAGS+1;
    } extra_frags;
#ifdef QCA_MDM_DEVICE
    uint32_t owner_id;
    __adf_nbuf_callback_fn adf_nbuf_callback_fn;
#ifdef IPA_OFFLOAD
    unsigned long priv_data;
#endif
#endif /* QCA_MDM_DEVICE */
#ifdef QCA_PKT_PROTO_TRACE
    unsigned char proto_type;
    unsigned char vdev_id;
#endif /* QCA_PKT_PROTO_TRACE */
#ifdef QCA_TX_HTT2_SUPPORT
    unsigned char tx_htt2_frm: 1;
    unsigned char tx_htt2_reserved: 7;
#endif /* QCA_TX_HTT2_SUPPORT */
};

#ifdef QCA_ARP_SPOOFING_WAR
#define NBUF_CB_PTR(skb) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->txrx_field.ptr)
#endif

#ifdef DEBUG_RX_RING_BUFFER
#define NBUF_MAP_ID(skb) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->txrx_field.map_index)
#endif

#ifdef QCA_MDM_DEVICE
#define NBUF_OWNER_ID(skb) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->owner_id)
#ifdef IPA_OFFLOAD
#define NBUF_OWNER_PRIV_DATA(skb) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->priv_data)
#endif
#define NBUF_CALLBACK_FN(skb) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->adf_nbuf_callback_fn)
#define NBUF_CALLBACK_FN_EXEC(skb) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->adf_nbuf_callback_fn)(skb)
#endif /* QCA_MDM_DEVICE */
#define NBUF_MAPPED_PADDR_LO(skb) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->mapped_paddr_lo[0])
#define NBUF_NUM_EXTRA_FRAGS(skb) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->extra_frags.num)
#define NBUF_EXTRA_FRAG_VADDR(skb, frag_num) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->extra_frags.vaddr[(frag_num)])
#define NBUF_EXTRA_FRAG_PADDR_LO(skb, frag_num) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->extra_frags.paddr_lo[(frag_num)])
#define NBUF_EXTRA_FRAG_LEN(skb, frag_num) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->extra_frags.len[(frag_num)])
#define NBUF_EXTRA_FRAG_WORDSTREAM_FLAGS(skb) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->extra_frags.wordstream_flags)

#ifdef QCA_PKT_PROTO_TRACE
#define NBUF_SET_PROTO_TYPE(skb, proto_type) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->proto_type = proto_type)
#define NBUF_GET_PROTO_TYPE(skb) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->proto_type)
#else
#define NBUF_SET_PROTO_TYPE(skb, proto_type);
#define NBUF_GET_PROTO_TYPE(skb) 0;
#endif /* QCA_PKT_PROTO_TRACE */

#ifdef QCA_TX_HTT2_SUPPORT
#define NBUF_SET_TX_HTT2_FRM(skb, candi) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->tx_htt2_frm = candi)
#define NBUF_GET_TX_HTT2_FRM(skb) \
    (((struct cvg_nbuf_cb *)((skb)->cb))->tx_htt2_frm)
#else
#define NBUF_SET_TX_HTT2_FRM(skb, candi)
#define NBUF_GET_TX_HTT2_FRM(skb) 0
#endif /* QCA_TX_HTT2_SUPPORT */

#ifdef CONFIG_HL_SUPPORT
#define NBUF_CB_ID(skb) \
    (&((struct cvg_nbuf_cb *)((skb)->cb))->tx_desc_id)
#endif

#define __adf_nbuf_get_num_frags(skb)              \
    /* assume the OS provides a single fragment */ \
    (NBUF_NUM_EXTRA_FRAGS(skb) + 1)


#define __adf_nbuf_frag_push_head( \
            skb, frag_len, frag_vaddr, frag_paddr_lo, frag_paddr_hi) \
    do { \
        int frag_num = NBUF_NUM_EXTRA_FRAGS(skb)++; \
        NBUF_EXTRA_FRAG_VADDR(skb, frag_num) = frag_vaddr; \
        NBUF_EXTRA_FRAG_PADDR_LO(skb, frag_num) = frag_paddr_lo; \
        NBUF_EXTRA_FRAG_LEN(skb, frag_num) = frag_len; \
    } while (0)

#define __adf_nbuf_get_frag_len(skb, frag_num)           \
    ((frag_num < NBUF_NUM_EXTRA_FRAGS(skb)) ?            \
        NBUF_EXTRA_FRAG_LEN(skb, frag_num) : (skb)->len)

#define __adf_nbuf_get_frag_vaddr(skb, frag_num)              \
    ((frag_num < NBUF_NUM_EXTRA_FRAGS(skb)) ?                 \
        NBUF_EXTRA_FRAG_VADDR(skb, frag_num) : ((skb)->data))

#define __adf_nbuf_get_frag_paddr_lo(skb, frag_num)              \
    ((frag_num < NBUF_NUM_EXTRA_FRAGS(skb)) ?                    \
        NBUF_EXTRA_FRAG_PADDR_LO(skb, frag_num) :                \
        /* assume that the OS only provides a single fragment */ \
        NBUF_MAPPED_PADDR_LO(skb))

#define __adf_nbuf_get_frag_is_wordstream(skb, frag_num) \
    ((frag_num < NBUF_NUM_EXTRA_FRAGS(skb)) ?            \
        (NBUF_EXTRA_FRAG_WORDSTREAM_FLAGS(skb) >>        \
            (frag_num)) & 0x1 :                          \
        (NBUF_EXTRA_FRAG_WORDSTREAM_FLAGS(skb) >>        \
             (CVG_NBUF_MAX_EXTRA_FRAGS)) & 0x1)

#define __adf_nbuf_set_frag_is_wordstream(skb, frag_num, is_wordstream) \
    do {                                                                \
        if (frag_num >= NBUF_NUM_EXTRA_FRAGS(skb)) {                    \
            frag_num = CVG_NBUF_MAX_EXTRA_FRAGS;                        \
        }                                                               \
        /* clear the old value */                                       \
        NBUF_EXTRA_FRAG_WORDSTREAM_FLAGS(skb) &= ~(1 << frag_num);      \
        /* set the new value */                                         \
        NBUF_EXTRA_FRAG_WORDSTREAM_FLAGS(skb) |=                        \
            ((is_wordstream) << frag_num);                              \
    } while (0)

#define __adf_nbuf_trace_set_proto_type(skb, proto_type) \
    NBUF_SET_PROTO_TYPE(skb, proto_type)
#define __adf_nbuf_trace_get_proto_type(skb) \
    NBUF_GET_PROTO_TYPE(skb);

typedef struct __adf_nbuf_qhead {
    struct sk_buff   *head;
    struct sk_buff   *tail;
    unsigned int     qlen;
}__adf_nbuf_queue_t;

// typedef struct sk_buff_head     __adf_nbuf_queue_t;

// struct anet_dma_info {
//     dma_addr_t  daddr;
//     uint32_t    len;
// };
//
// struct __adf_nbuf_map {
//     int                   nelem;
//     struct anet_dma_info  dma[1];
// };
//
// typedef struct __adf_nbuf_map     *__adf_nbuf_dmamap_t;


/*
 * Use sk_buff_head as the implementation of adf_nbuf_queue_t.
 * Because the queue head will most likely put in some structure,
 * we don't use pointer type as the definition.
 */


/*
 * prototypes. Implemented in adf_nbuf_pvt.c
 */
__adf_nbuf_t    __adf_nbuf_alloc(__adf_os_device_t osdev, size_t size, int reserve, int align, int prio);
#ifdef QCA_ARP_SPOOFING_WAR
__adf_nbuf_t    __adf_rx_nbuf_alloc(__adf_os_device_t osdev, size_t size,
        int reserve, int align, int prio);
#endif
void            __adf_nbuf_free (struct sk_buff *skb);
void            __adf_nbuf_ref (struct sk_buff *skb);
int             __adf_nbuf_shared (struct sk_buff *skb);
a_status_t      __adf_nbuf_dmamap_create(__adf_os_device_t osdev,
                                         __adf_os_dma_map_t *dmap);
void            __adf_nbuf_dmamap_destroy(__adf_os_device_t osdev,
                                          __adf_os_dma_map_t dmap);
a_status_t      __adf_nbuf_map(__adf_os_device_t osdev,
                    struct sk_buff *skb, adf_os_dma_dir_t dir);
void            __adf_nbuf_unmap(__adf_os_device_t osdev,
                    struct sk_buff *skb, adf_os_dma_dir_t dir);
a_status_t      __adf_nbuf_map_single(__adf_os_device_t osdev,
                    struct sk_buff *skb, adf_os_dma_dir_t dir);
void            __adf_nbuf_unmap_single(__adf_os_device_t osdev,
                    struct sk_buff *skb, adf_os_dma_dir_t dir);
void            __adf_nbuf_dmamap_info(__adf_os_dma_map_t bmap, adf_os_dmamap_info_t *sg);
void            __adf_nbuf_frag_info(struct sk_buff *skb, adf_os_sglist_t  *sg);
void            __adf_nbuf_dmamap_set_cb(__adf_os_dma_map_t dmap, void *cb, void *arg);
void            __adf_nbuf_reg_trace_cb(adf_nbuf_trace_update_t cb_func_ptr);
a_status_t      __adf_nbuf_is_dhcp_pkt(struct sk_buff *skb);
a_status_t      __adf_nbuf_is_eapol_pkt(struct sk_buff *skb);

#ifdef QCA_PKT_PROTO_TRACE
void
__adf_nbuf_trace_update(struct sk_buff *buf, char *event_string);
#else
#define __adf_nbuf_trace_update(skb, event_string)
#endif /* QCA_PKT_PROTO_TRACE */

static inline a_status_t
__adf_os_to_status(signed int error)
{
    switch(error){
    case 0:
        return A_STATUS_OK;
    case ENOMEM:
    case -ENOMEM:
        return A_STATUS_ENOMEM;
    default:
        return A_STATUS_ENOTSUPP;
    }
}



/**
 * @brief This keeps the skb shell intact expands the headroom
 *        in the data region. In case of failure the skb is
 *        released.
 *
 * @param skb
 * @param headroom
 *
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_realloc_headroom(struct sk_buff *skb, uint32_t headroom)
{
    if(pskb_expand_head(skb, headroom, 0, GFP_ATOMIC)){
        dev_kfree_skb_any(skb);
        skb = NULL;
    }
    return skb;
}
/**
 * @brief This keeps the skb shell intact exapnds the tailroom
 *        in data region. In case of failure it releases the
 *        skb.
 *
 * @param skb
 * @param tailroom
 *
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_realloc_tailroom(struct sk_buff *skb, uint32_t tailroom)
{
    if(likely(!pskb_expand_head(skb, 0, tailroom, GFP_ATOMIC)))
        return skb;
    /**
     * unlikely path
     */
    dev_kfree_skb_any(skb);
    return NULL;
}
/**
 * @brief return the amount of valid data in the skb, If there
 *        are frags then it returns total length.
 *
 * @param skb
 *
 * @return size_t
 */
static inline size_t
__adf_nbuf_len(struct sk_buff *skb)
{
    int i, extra_frag_len = 0;

    i = NBUF_NUM_EXTRA_FRAGS(skb);
    while (i-- > 0) {
        extra_frag_len += NBUF_EXTRA_FRAG_LEN(skb, i);
    }
    return (extra_frag_len + skb->len);
}


/**
 * @brief link two nbufs, the new buf is piggybacked into the
 *        older one. The older (src) skb is released.
 *
 * @param dst( buffer to piggyback into)
 * @param src (buffer to put)
 *
 * @return a_status_t (status of the call) if failed the src skb
 *         is released
 */
static inline a_status_t
__adf_nbuf_cat(struct sk_buff *dst, struct sk_buff *src)
{
    a_status_t error = 0;

    adf_os_assert(dst && src);

    /*
     * Since pskb_expand_head unconditionally reallocates the skb->head buffer,
     * first check whether the current buffer is already large enough.
     */
    if (skb_tailroom(dst) < src->len) {
        error = pskb_expand_head(dst, 0, src->len, GFP_ATOMIC);
        if (error) {
            return __adf_os_to_status(error);
        }
    }
    memcpy(skb_tail_pointer(dst), src->data, src->len);

    skb_put(dst, src->len);
    dev_kfree_skb_any(src);

    return __adf_os_to_status(error);
}


/**
 * @brief  create a version of the specified nbuf whose contents
 *         can be safely modified without affecting other
 *         users.If the nbuf is a clone then this function
 *         creates a new copy of the data. If the buffer is not
 *         a clone the original buffer is returned.
 *
 * @param skb (source nbuf to create a writable copy from)
 *
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_unshare(struct sk_buff *skb)
{
    return skb_unshare(skb, GFP_ATOMIC);
}
/**************************nbuf manipulation routines*****************/

static inline int
__adf_nbuf_headroom(struct sk_buff *skb)
{
    return skb_headroom(skb);
}
/**
 * @brief return the amount of tail space available
 *
 * @param buf
 *
 * @return amount of tail room
 */
static inline uint32_t
__adf_nbuf_tailroom(struct sk_buff *skb)
{
    return skb_tailroom(skb);
}

/**
 * @brief Push data in the front
 *
 * @param[in] buf      buf instance
 * @param[in] size     size to be pushed
 *
 * @return New data pointer of this buf after data has been pushed,
 *         or NULL if there is not enough room in this buf.
 */
static inline uint8_t *
__adf_nbuf_push_head(struct sk_buff *skb, size_t size)
{
    if(NBUF_MAPPED_PADDR_LO(skb)) {
       NBUF_MAPPED_PADDR_LO(skb) -= size;
    }
    return skb_push(skb, size);
}
/**
 * @brief Puts data in the end
 *
 * @param[in] buf      buf instance
 * @param[in] size     size to be pushed
 *
 * @return data pointer of this buf where new data has to be
 *         put, or NULL if there is not enough room in this buf.
 */
static inline uint8_t *
__adf_nbuf_put_tail(struct sk_buff *skb, size_t size)
{
   if (skb_tailroom(skb) < size) {
       if(unlikely(pskb_expand_head(skb, 0, size-skb_tailroom(skb), GFP_ATOMIC))) {
           dev_kfree_skb_any(skb);
           return NULL;
       }
   }
   return skb_put(skb, size);
}

/**
 * @brief pull data out from the front
 *
 * @param[in] buf   buf instance
 * @param[in] size  size to be popped
 *
 * @return New data pointer of this buf after data has been popped,
 *         or NULL if there is not sufficient data to pull.
 */
static inline uint8_t *
__adf_nbuf_pull_head(struct sk_buff *skb, size_t size)
{
    if(NBUF_MAPPED_PADDR_LO(skb)) {
       NBUF_MAPPED_PADDR_LO(skb) += size;
    }
    return skb_pull(skb, size);
}
/**
 *
 * @brief trim data out from the end
 *
 * @param[in] buf   buf instance
 * @param[in] size     size to be popped
 *
 * @return none
 */
static inline void
__adf_nbuf_trim_tail(struct sk_buff *skb, size_t size)
{
    return skb_trim(skb, skb->len - size);
}

/**
 * @brief test whether the nbuf is cloned or not
 *
 * @param buf
 *
 * @return a_bool_t (TRUE if it is cloned or else FALSE)
 */
static inline a_bool_t
__adf_nbuf_is_cloned(struct sk_buff *skb)
{
        return skb_cloned(skb);
}

/* TODO: Fix this */
static inline uint8_t *
__adf_nbuf_init(struct sk_buff *skb, size_t reserve, size_t align, size_t tail_size)
{
#if 0
    unsigned long offset;
     /**
     * Align & make sure that the tail & data are adjusted properly
     */
    if(align){
        offset = ((unsigned long) skb->data) % align;
        if(offset)
            skb_reserve(skb, align - offset);
    }

    /**
     * NOTE:alloc doesn't take responsibility if reserve unaligns the data
     * pointer
     */
    skb_reserve(skb, reserve);
    return skb;
#endif
    return NULL;
}

static inline void
__adf_nbuf_free_pool(struct sk_buff *skb)
{
    return;
}

/*********************nbuf private buffer routines*************/

/**
 * @brief get the priv pointer from the nbuf'f private space
 *
 * @param buf
 *
 * @return data pointer to typecast into your priv structure
 */
static inline uint8_t *
__adf_nbuf_get_priv(struct sk_buff *skb)
{
        return &skb->cb[8];
}

/**
 * @brief This will return the header's addr & m_len
 */
static inline void
__adf_nbuf_peek_header(struct sk_buff *skb, uint8_t   **addr,
                       uint32_t    *len)
{
    *addr = skb->data;
    *len  = skb->len;
}

 /* adf_nbuf_queue_init() - initialize a skbuf queue
 */
/* static inline void */
/* __adf_nbuf_queue_init(struct sk_buff_head *qhead) */
/* { */
/*     skb_queue_head_init(qhead); */
/* } */

/* /\* */
/*  * adf_nbuf_queue_add() - add a skbuf to the end of the skbuf queue */

/*  * We use the non-locked version because */
/*  * there's no need to use the irq safe version of spinlock. */
/*  * However, the caller has to do synchronization by itself. */
/*  *\/ */
/* static inline void */
/* __adf_nbuf_queue_add(struct sk_buff_head *qhead,  */
/*                      struct sk_buff *skb) */
/* { */
/*     __skb_queue_tail(qhead, skb); */
/*     adf_os_assert(qhead->next == qhead->prev); */
/* } */

/* /\* */
/*  * adf_nbuf_queue_remove() - remove a skbuf from the head of the skbuf queue */
/*  * */
/*  * We use the non-locked version because */
/*  * there's no need to use the irq safe version of spinlock. */
/*  * However, the caller has to do synchronization by itself. */
/*  *\/ */
/* static inline struct sk_buff * */
/* __adf_nbuf_queue_remove(struct sk_buff_head * qhead) */
/* { */
/*     adf_os_assert(qhead->next == qhead->prev); */
/*     return __skb_dequeue(qhead); */
/* } */

/* static inline uint32_t */
/* __adf_nbuf_queue_len(struct sk_buff_head * qhead) */
/* { */
/*         adf_os_assert(qhead->next == qhead->prev); */
/*         return qhead->qlen; */
/* } */
/* /\** */
/*  * @brief returns the first guy in the Q */
/*  * @param qhead */
/*  *  */
/*  * @return (NULL if the Q is empty) */
/*  *\/ */
/* static inline struct sk_buff *    */
/* __adf_nbuf_queue_first(struct sk_buff_head *qhead) */
/* { */
/*    adf_os_assert(qhead->next == qhead->prev); */
/*    return (skb_queue_empty(qhead) ? NULL : qhead->next); */
/* } */
/* /\** */
/*  * @brief return the next packet from packet chain */
/*  *  */
/*  * @param buf (packet) */
/*  *  */
/*  * @return (NULL if no packets are there) */
/*  *\/ */
/* static inline struct sk_buff *    */
/* __adf_nbuf_queue_next(struct sk_buff *skb) */
/* { */
/*   return NULL; // skb->next; */
/* } */
/* /\** */
/*  * adf_nbuf_queue_empty() - check if the skbuf queue is empty */
/*  *\/ */
/* static inline a_bool_t */
/* __adf_nbuf_is_queue_empty(struct sk_buff_head *qhead) */
/* { */
/*     adf_os_assert(qhead->next == qhead->prev); */
/*     return skb_queue_empty(qhead); */
/* } */

/******************Custom queue*************/



/**
 * @brief initiallize the queue head
 *
 * @param qhead
 */
static inline a_status_t
__adf_nbuf_queue_init(__adf_nbuf_queue_t *qhead)
{
    memset(qhead, 0, sizeof(struct __adf_nbuf_qhead));
    return A_STATUS_OK;
}


/**
 * @brief add an skb in the tail of the queue. This is a
 *        lockless version, driver must acquire locks if it
 *        needs to synchronize
 *
 * @param qhead
 * @param skb
 */
static inline void
__adf_nbuf_queue_add(__adf_nbuf_queue_t *qhead,
                     struct sk_buff *skb)
{
    skb->next = NULL;/*Nullify the next ptr*/

    if(!qhead->head)
        qhead->head = skb;
    else
        qhead->tail->next = skb;

    qhead->tail = skb;
    qhead->qlen++;
}

/**
 * @brief add an skb at  the head  of the queue. This is a
 * lockless version, driver must acquire locks if it
 * needs to synchronize
 *
 * @param qhead
 * @param skb
 */
static inline void
__adf_nbuf_queue_insert_head(__adf_nbuf_queue_t *qhead,
                             __adf_nbuf_t skb)
{
    if(!qhead->head){
        /*Empty queue Tail pointer Must be updated */
        qhead->tail = skb;
    }
    skb->next = qhead->head;
    qhead->head = skb;
    qhead->qlen++;
}

/**
 * @brief remove a skb from the head of the queue, this is a
 *        lockless version. Driver should take care of the locks
 *
 * @param qhead
 *
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_queue_remove(__adf_nbuf_queue_t * qhead)
{
    __adf_nbuf_t  tmp = NULL;

    if (qhead->head) {
        qhead->qlen--;
        tmp = qhead->head;
        if ( qhead->head == qhead->tail ) {
            qhead->head = NULL;
            qhead->tail = NULL;
        } else {
            qhead->head = tmp->next;
        }
        tmp->next = NULL;
    }
    return tmp;
}
/**
 * @brief return the queue length
 *
 * @param qhead
 *
 * @return uint32_t
 */
static inline uint32_t
__adf_nbuf_queue_len(__adf_nbuf_queue_t * qhead)
{
        return qhead->qlen;
}
/**
 * @brief returns the first skb in the queue
 *
 * @param qhead
 *
 * @return (NULL if the Q is empty)
 */
static inline struct sk_buff *
__adf_nbuf_queue_first(__adf_nbuf_queue_t *qhead)
{
    return qhead->head;
}
/**
 * @brief return the next skb from packet chain, remember the
 *        skb is still in the queue
 *
 * @param buf (packet)
 *
 * @return (NULL if no packets are there)
 */
static inline struct sk_buff *
__adf_nbuf_queue_next(struct sk_buff *skb)
{
    return skb->next;
}
/**
 * @brief check if the queue is empty or not
 *
 * @param qhead
 *
 * @return a_bool_t
 */
static inline a_bool_t
__adf_nbuf_is_queue_empty(__adf_nbuf_queue_t *qhead)
{
    return (qhead->qlen == 0);
}

/*
 * Use sk_buff_head as the implementation of adf_nbuf_queue_t.
 * Because the queue head will most likely put in some structure,
 * we don't use pointer type as the definition.
 */


/*
 * prototypes. Implemented in adf_nbuf_pvt.c
 */
adf_nbuf_tx_cksum_t __adf_nbuf_get_tx_cksum(struct sk_buff *skb);
a_status_t      __adf_nbuf_set_rx_cksum(struct sk_buff *skb,
                                        adf_nbuf_rx_cksum_t *cksum);
a_status_t      __adf_nbuf_get_vlan_info(adf_net_handle_t hdl,
                                         struct sk_buff *skb,
                                         adf_net_vlanhdr_t *vlan);
a_uint8_t __adf_nbuf_get_tid(struct sk_buff *skb);
void __adf_nbuf_set_tid(struct sk_buff *skb, a_uint8_t tid);
a_uint8_t __adf_nbuf_get_exemption_type(struct sk_buff *skb);
/*
 * adf_nbuf_pool_init() implementation - do nothing in Linux
 */
static inline a_status_t
__adf_nbuf_pool_init(adf_net_handle_t anet)
{
    return A_STATUS_OK;
}

/*
 * adf_nbuf_pool_delete() implementation - do nothing in linux
 */
#define __adf_nbuf_pool_delete(osdev)


/**
 * @brief Expand both tailroom & headroom. In case of failure
 *        release the skb.
 *
 * @param skb
 * @param headroom
 * @param tailroom
 *
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_expand(struct sk_buff *skb, uint32_t headroom, uint32_t tailroom)
{
    if(likely(!pskb_expand_head(skb, headroom, tailroom, GFP_ATOMIC)))
        return skb;

    dev_kfree_skb_any(skb);
    return NULL;
}


/**
 * @brief clone the nbuf (copy is readonly)
 *
 * @param src_nbuf (nbuf to clone from)
 * @param dst_nbuf (address of the cloned nbuf)
 *
 * @return status
 *
 * @note if GFP_ATOMIC is overkill then we can check whether its
 *       called from interrupt context and then do it or else in
 *       normal case use GFP_KERNEL
 * @example     use "in_irq() || irqs_disabled()"
 *
 *
 */
static inline struct sk_buff *
__adf_nbuf_clone(struct sk_buff *skb)
{
    return skb_clone(skb, GFP_ATOMIC);
}
/**
 * @brief returns a private copy of the skb, the skb returned is
 *        completely modifiable
 *
 * @param skb
 *
 * @return skb or NULL
 */
static inline struct sk_buff *
__adf_nbuf_copy(struct sk_buff *skb)
{
    return skb_copy(skb, GFP_ATOMIC);
}

#define __adf_nbuf_reserve      skb_reserve

/***********************XXX: misc api's************************/
static inline a_bool_t
__adf_nbuf_tx_cksum_info(struct sk_buff *skb, uint8_t **hdr_off,
                         uint8_t **where)
{
//     if (skb->ip_summed == CHECKSUM_NONE)
//         return A_FALSE;
//
//     if (skb->ip_summed == CHECKSUM_HW) {
//         *hdr_off = (uint8_t *)(skb->h.raw - skb->data);
//         *where   = *hdr_off + skb->csum;
//         return A_TRUE;
//     }

    adf_os_assert(0);
    return A_FALSE;
}


/*
 * XXX What about other unions in skb? Windows might not have it, but we
 * should penalize linux drivers for it.
 * Besides this function is not likely doint the correct thing.
 */
static inline a_status_t
__adf_nbuf_get_tso_info(struct sk_buff *skb, adf_nbuf_tso_t *tso)
{
    adf_os_assert(0);
    return A_STATUS_ENOTSUPP;
/*
    if (!skb_shinfo(skb)->tso_size) {
        tso->type = adf_net_TSO_NONE;
        return;
    }

    tso->mss      = skb_shinfo(skb)->tso_size;
*/
//     tso->hdr_off  = (uint8_t)(skb->h.raw - skb->data);
//
//     if (skb->protocol == ntohs(ETH_P_IP))
//         tso->type =  ADF_NET_TSO_IPV4;
//     else if (skb->protocol == ntohs(ETH_P_IPV6))
//         tso->type =  ADF_NET_TSO_ALL;
//     else
//         tso->type = ADF_NET_TSO_NONE;
}

/**
 * @brief return the pointer the skb's buffer
 *
 * @param skb
 *
 * @return uint8_t *
 */
static inline uint8_t *
__adf_nbuf_head(struct sk_buff *skb)
{
    return skb->head;
}

/**
 * @brief return the pointer to data header in the skb
 *
 * @param skb
 *
 * @return uint8_t *
 */
static inline uint8_t *
__adf_nbuf_data(struct sk_buff *skb)
{
    return skb->data;
}

/**
 * @brief return the priority value of the skb
 *
 * @param skb
 *
 * @return uint32_t
 */
static inline uint32_t
__adf_nbuf_get_priority(struct sk_buff *skb)
{
    return skb->priority;
}

/**
 * @brief sets the priority value of the skb
 *
 * @param skb, priority
 *
 * @return void
 */
static inline void
__adf_nbuf_set_priority(struct sk_buff *skb, uint32_t p)
{
    skb->priority = p;
}

/**
 * @brief sets the next skb pointer of the current skb
 *
 * @param skb and next_skb
 *
 * @return void
 */
static inline void
__adf_nbuf_set_next(struct sk_buff *skb, struct sk_buff *skb_next)
{
    skb->next = skb_next;
}

/**
 * @brief return the next skb pointer of the current skb
 *
 * @param skb - the current skb
 *
 * @return the next skb pointed to by the current skb
 */
static inline struct sk_buff *
__adf_nbuf_next(struct sk_buff *skb)
{
    return skb->next;
}

/**
 * @brief sets the next skb pointer of the current skb. This fn is used to
 * link up extensions to the head skb. Does not handle linking to the head
 *
 * @param skb and next_skb
 *
 * @return void
 */
static inline void
__adf_nbuf_set_next_ext(struct sk_buff *skb, struct sk_buff *skb_next)
{
    skb->next = skb_next;
}

/**
 * @brief return the next skb pointer of the current skb
 *
 * @param skb - the current skb
 *
 * @return the next skb pointed to by the current skb
 */
static inline struct sk_buff *
__adf_nbuf_next_ext(struct sk_buff *skb)
{
    return skb->next;
}

/**
 * @brief link list of packet extensions to the head segment
 * @details
 *  This function is used to link up a list of packet extensions (seg1, 2,
 *  ...) to the nbuf holding the head segment (seg0)
 * @param[in] head_buf nbuf holding head segment (single)
 * @param[in] ext_list nbuf list holding linked extensions to the head
 * @param[in] ext_len Total length of all buffers in the extension list
 */
static inline void
__adf_nbuf_append_ext_list(
        struct sk_buff *skb_head,
        struct sk_buff * ext_list,
        size_t ext_len)
{
        skb_shinfo(skb_head)->frag_list = ext_list;
        skb_head->data_len = ext_len;
        skb_head->len += skb_head->data_len;
}

static inline void
__adf_nbuf_tx_free(struct sk_buff *bufs, int tx_err)
{
    while (bufs) {
        struct sk_buff *next = __adf_nbuf_next(bufs);
        __adf_nbuf_free(bufs);
        bufs = next;
    }
}

/**
 * @brief return the checksum value of the skb
 *
 * @param skb
 *
 * @return uint32_t
 */
static inline uint32_t
__adf_nbuf_get_age(struct sk_buff *skb)
{
    return skb->csum;
}

/**
 * @brief sets the checksum value of the skb
 *
 * @param skb, value
 *
 * @return void
 */
static inline void
__adf_nbuf_set_age(struct sk_buff *skb, uint32_t v)
{
    skb->csum = v;
}

/**
 * @brief adjusts the checksum/age value of the skb
 *
 * @param skb, adj
 *
 * @return void
 */
static inline void
__adf_nbuf_adj_age(struct sk_buff *skb, uint32_t adj)
{
    skb->csum -= adj;
}

/**
 * @brief return the length of the copy bits for skb
 *
 * @param skb, offset, len, to
 *
 * @return int32_t
 */
static inline int32_t
__adf_nbuf_copy_bits(struct sk_buff *skb, int32_t offset, int32_t len, void *to)
{
    return skb_copy_bits(skb, offset, to, len);
}

/**
 * @brief sets the length of the skb and adjust the tail
 *
 * @param skb, length
 *
 * @return void
 */
static inline void
__adf_nbuf_set_pktlen(struct sk_buff *skb, uint32_t len)
{
    if (skb->len > len) {
        skb_trim(skb, len);
    }
    else {
        if (skb_tailroom(skb) < len - skb->len) {
            if(unlikely(pskb_expand_head(
                            skb, 0, len - skb->len -skb_tailroom(skb), GFP_ATOMIC)))
            {
                dev_kfree_skb_any(skb);
                //KASSERT(0, ("No enough tailroom for skb, failed to alloc"));
                adf_os_assert(0);
            }
        }
        skb_put(skb, (len - skb->len));
    }
}

/**
 * @brief sets the protocol value of the skb
 *
 * @param skb, protocol
 *
 * @return void
 */
static inline void
__adf_nbuf_set_protocol(struct sk_buff *skb, uint16_t protocol)
{
    skb->protocol = protocol;
}

/**
 * @brief test whether the nbuf is nonlinear or not
 *
 * @param buf
 *
 * @return a_bool_t (TRUE if it is nonlinear or else FALSE)
 */
static inline a_bool_t
__adf_nbuf_is_nonlinear(struct sk_buff *skb)
{
        return skb_is_nonlinear(skb);
}

/**
 * @brief zeros out cb
 *
 * @param nbuf
 *
 * @return void
 */
static inline void
__adf_nbuf_reset_ctxt(__adf_nbuf_t nbuf)
{
    adf_os_mem_zero(nbuf->cb, sizeof(nbuf->cb));
}

/**
 * @brief This function peeks data into the buffer at given offset
 *
 * @param[in] buf   buffer
 * @param[out] data  peeked output buffer
 * @param[in] off   offset
 * @param[in] len   length of buffer requested beyond offset
 *
 * @return status of operation
 */
static inline a_status_t
__adf_nbuf_peek_data(__adf_nbuf_t buf, void **data, a_uint32_t off,
		     a_uint32_t len)
{
	/* check if we can peek data of len bytes from the offset */
	if (buf->len < (off + len)) {
		*data = NULL;
		return A_STATUS_ENOMEM;
	}

	*data = (void*)&buf->data[off];

	return A_STATUS_OK;
}

#define __adf_nbuf_set_tx_htt2_frm(skb, candi) \
    NBUF_SET_TX_HTT2_FRM(skb, candi)
#define __adf_nbuf_get_tx_htt2_frm(skb) \
    NBUF_GET_TX_HTT2_FRM(skb)

#endif /*_adf_nbuf_PVT_H */
