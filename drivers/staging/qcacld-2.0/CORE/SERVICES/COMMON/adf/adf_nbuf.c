/*
 * Copyright (c) 2013-2015 The Linux Foundation. All rights reserved.
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


#include <linux/kernel.h>
#include <linux/version.h>
#include <linux/skbuff.h>
#include <linux/module.h>
#include <adf_os_types.h>
#include <adf_nbuf.h>
#include <adf_os_io.h>

#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
#include <net/cnss_prealloc.h>
#endif

adf_nbuf_trace_update_t  trace_update_cb = NULL;

#if defined(CONFIG_WCNSS_MEM_PRE_ALLOC) && defined(WITH_BACKPORTS)
struct sk_buff *__adf_nbuf_pre_alloc(adf_os_device_t osdev, size_t size)
{
	struct sk_buff *skb = NULL;

	if (size >= WCNSS_PRE_SKB_ALLOC_GET_THRESHOLD)
		skb = wcnss_skb_prealloc_get(size);

	return skb;
}

int __adf_nbuf_pre_alloc_free(struct sk_buff *skb)
{
	return wcnss_skb_prealloc_put(skb);
}
#else
struct sk_buff *__adf_nbuf_pre_alloc(adf_os_device_t osdev, size_t size)
{
	return NULL;
}

int __adf_nbuf_pre_alloc_free(struct sk_buff *skb)
{
	return 0;
}
#endif

/*
 * @brief This allocates an nbuf aligns if needed and reserves
 *        some space in the front, since the reserve is done
 *        after alignment the reserve value if being unaligned
 *        will result in an unaligned address.
 *
 * @param hdl
 * @param size
 * @param reserve
 * @param align
 *
 * @return nbuf or NULL if no memory
 */
struct sk_buff *
__adf_nbuf_alloc(adf_os_device_t osdev, size_t size, int reserve, int align, int prio)
{
    struct sk_buff *skb;
    unsigned long offset;

    if(align)
        size += (align - 1);

    skb = __adf_nbuf_pre_alloc(osdev, size);

    if (skb)
       goto skb_cb;

    skb = dev_alloc_skb(size);

    if (!skb) {
        printk("ERROR:NBUF alloc failed\n");
        return NULL;
    }

skb_cb:
    memset(skb->cb, 0x0, sizeof(skb->cb));

    /*
     * The default is for netbuf fragments to be interpreted
     * as wordstreams rather than bytestreams.
     * Set the CVG_NBUF_MAX_EXTRA_FRAGS+1 wordstream_flags bits,
     * to provide this default.
     */
    NBUF_EXTRA_FRAG_WORDSTREAM_FLAGS(skb) =
        (1 << (CVG_NBUF_MAX_EXTRA_FRAGS + 1)) - 1;

    /**
     * XXX:how about we reserve first then align
     */

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
}

#ifdef QCA_ARP_SPOOFING_WAR
/*
 * __adf_rx_nbuf_alloc() Rx buffer allocation function *
 * @hdl:
 * @size:
 * @reserve:
 * @align:
 *
 * Use existing buffer allocation API and overwrite
 * priv_data field of skb->cb for registering callback
 * as it is not used for Rx case.
 *
 * Return: nbuf or NULL if no memory
 */
struct sk_buff *
__adf_rx_nbuf_alloc(adf_os_device_t osdev, size_t size, int reserve, int align, int prio)
{
    struct sk_buff *skb;

    skb = __adf_nbuf_alloc(osdev, size, reserve,align, prio);
    if (skb) {
        NBUF_CB_PTR(skb) = osdev->filter_cb;
    }
    return skb;
}
#endif
/*
 * @brief free the nbuf its interrupt safe
 * @param skb
 */
void
__adf_nbuf_free(struct sk_buff *skb)
{
#ifdef QCA_MDM_DEVICE
#if defined(IPA_OFFLOAD) && (!defined(IPA_UC_OFFLOAD) ||\
   (defined(IPA_UC_OFFLOAD) && defined(IPA_UC_STA_OFFLOAD)))
    if( (NBUF_OWNER_ID(skb) == IPA_NBUF_OWNER_ID) && NBUF_CALLBACK_FN(skb) )
        NBUF_CALLBACK_FN_EXEC(skb);
    else
#endif
#endif /* QCA_MDM_DEVICE */
    {
       if (__adf_nbuf_pre_alloc_free(skb))
           return;
       dev_kfree_skb_any(skb);
    }
}


/*
 * @brief Reference the nbuf so it can get held until the last free.
 * @param skb
 */

void
__adf_nbuf_ref(struct sk_buff *skb)
{
    skb_get(skb);
}

/**
 *  @brief Check whether the buffer is shared
 *  @param skb: buffer to check
 *
 *  Returns true if more than one person has a reference to this
 *  buffer.
 */
int
__adf_nbuf_shared(struct sk_buff *skb)
{
    return skb_shared(skb);
}
/**
 * @brief create a nbuf map
 * @param osdev
 * @param dmap
 *
 * @return a_status_t
 */
a_status_t
__adf_nbuf_dmamap_create(adf_os_device_t osdev, __adf_os_dma_map_t *dmap)
{
    a_status_t error = A_STATUS_OK;
    /**
     * XXX: driver can tell its SG capablity, it must be handled.
     * XXX: Bounce buffers if they are there
     */
    (*dmap) = kzalloc(sizeof(struct __adf_os_dma_map), GFP_KERNEL);
    if(!(*dmap))
        error = A_STATUS_ENOMEM;

    return error;
}

/**
 * @brief free the nbuf map
 *
 * @param osdev
 * @param dmap
 */
void
__adf_nbuf_dmamap_destroy(adf_os_device_t osdev, __adf_os_dma_map_t dmap)
{
    kfree(dmap);
}

/**
 * @brief get the dma map of the nbuf
 *
 * @param osdev
 * @param bmap
 * @param skb
 * @param dir
 *
 * @return a_status_t
 */
a_status_t
__adf_nbuf_map(
    adf_os_device_t osdev,
    struct sk_buff *skb,
    adf_os_dma_dir_t dir)
{
#ifdef ADF_OS_DEBUG
    struct skb_shared_info  *sh = skb_shinfo(skb);
#endif
    adf_os_assert(
        (dir == ADF_OS_DMA_TO_DEVICE) || (dir == ADF_OS_DMA_FROM_DEVICE));

    /*
     * Assume there's only a single fragment.
     * To support multiple fragments, it would be necessary to change
     * adf_nbuf_t to be a separate object that stores meta-info
     * (including the bus address for each fragment) and a pointer
     * to the underlying sk_buff.
     */
    adf_os_assert(sh->nr_frags == 0);

    return __adf_nbuf_map_single(osdev, skb, dir);

    return A_STATUS_OK;
}

/**
 * @brief adf_nbuf_unmap() - to unmap a previously mapped buf
 */
void
__adf_nbuf_unmap(
    adf_os_device_t osdev,
    struct sk_buff *skb,
    adf_os_dma_dir_t dir)
{
    adf_os_assert(
        (dir == ADF_OS_DMA_TO_DEVICE) || (dir == ADF_OS_DMA_FROM_DEVICE));

    adf_os_assert(((dir == ADF_OS_DMA_TO_DEVICE) || (dir == ADF_OS_DMA_FROM_DEVICE)));
    /*
     * Assume there's a single fragment.
     * If this is not true, the assertion in __adf_nbuf_map will catch it.
     */
    __adf_nbuf_unmap_single(osdev, skb, dir);
}

a_status_t
__adf_nbuf_map_single(
    adf_os_device_t osdev, adf_nbuf_t buf, adf_os_dma_dir_t dir)
{
    u_int32_t paddr_lo;

/* tempory hack for simulation */
#ifdef A_SIMOS_DEVHOST
    NBUF_MAPPED_PADDR_LO(buf) = paddr_lo = (u_int32_t) buf->data;
    return A_STATUS_OK;
#else
    /* assume that the OS only provides a single fragment */
    NBUF_MAPPED_PADDR_LO(buf) = paddr_lo =
        dma_map_single(osdev->dev, buf->data,
                       skb_end_pointer(buf) - buf->data, dir);
    return dma_mapping_error(osdev->dev, paddr_lo) ?
        A_STATUS_FAILED : A_STATUS_OK;
#endif	/* #ifdef A_SIMOS_DEVHOST */
}

void
__adf_nbuf_unmap_single(
    adf_os_device_t osdev, adf_nbuf_t buf, adf_os_dma_dir_t dir)
{
#if !defined(A_SIMOS_DEVHOST)
    dma_unmap_single(osdev->dev, NBUF_MAPPED_PADDR_LO(buf),
                     skb_end_pointer(buf) - buf->data, dir);
#endif	/* #if !defined(A_SIMOS_DEVHOST) */
}

/**
 * @brief return the dma map info
 *
 * @param[in]  bmap
 * @param[out] sg (map_info ptr)
 */
void
__adf_nbuf_dmamap_info(__adf_os_dma_map_t bmap, adf_os_dmamap_info_t *sg)
{
    adf_os_assert(bmap->mapped);
    adf_os_assert(bmap->nsegs <= ADF_OS_MAX_SCATTER);

    memcpy(sg->dma_segs, bmap->seg, bmap->nsegs *
           sizeof(struct __adf_os_segment));
    sg->nsegs = bmap->nsegs;
}
/**
 * @brief return the frag data & len, where frag no. is
 *        specified by the index
 *
 * @param[in] buf
 * @param[out] sg (scatter/gather list of all the frags)
 *
 */
void
__adf_nbuf_frag_info(struct sk_buff *skb, adf_os_sglist_t  *sg)
{
#if defined(ADF_OS_DEBUG) || defined(__ADF_SUPPORT_FRAG_MEM)
    struct skb_shared_info  *sh = skb_shinfo(skb);
#endif
    adf_os_assert(skb != NULL);
    sg->sg_segs[0].vaddr = skb->data;
    sg->sg_segs[0].len   = skb->len;
    sg->nsegs            = 1;

#ifndef __ADF_SUPPORT_FRAG_MEM
    adf_os_assert(sh->nr_frags == 0);
#else
    for(int i = 1; i <= sh->nr_frags; i++){
        skb_frag_t    *f        = &sh->frags[i - 1];
        sg->sg_segs[i].vaddr    = (uint8_t *)(page_address(f->page) +
                                  f->page_offset);
        sg->sg_segs[i].len      = f->size;

        adf_os_assert(i < ADF_OS_MAX_SGLIST);
    }
    sg->nsegs += i;
#endif
}

a_status_t
__adf_nbuf_set_rx_cksum(struct sk_buff *skb, adf_nbuf_rx_cksum_t *cksum)
{
    switch (cksum->l4_result) {
    case ADF_NBUF_RX_CKSUM_NONE:
        skb->ip_summed = CHECKSUM_NONE;
        break;
    case ADF_NBUF_RX_CKSUM_TCP_UDP_UNNECESSARY:
        skb->ip_summed = CHECKSUM_UNNECESSARY;
        break;
    case ADF_NBUF_RX_CKSUM_TCP_UDP_HW:
        skb->ip_summed = CHECKSUM_PARTIAL;
        skb->csum      = cksum->val;
        break;
    default:
        printk("ADF_NET:Unknown checksum type\n");
        adf_os_assert(0);
	return A_STATUS_ENOTSUPP;
    }
    return A_STATUS_OK;
}

adf_nbuf_tx_cksum_t
__adf_nbuf_get_tx_cksum(struct sk_buff *skb)
{
    switch (skb->ip_summed) {
    case CHECKSUM_NONE:
        return ADF_NBUF_TX_CKSUM_NONE;
    case CHECKSUM_PARTIAL:
        /* XXX ADF and Linux checksum don't map with 1-to-1. This is not 100%
         * correct. */
        return ADF_NBUF_TX_CKSUM_TCP_UDP;
    case CHECKSUM_COMPLETE:
        return ADF_NBUF_TX_CKSUM_TCP_UDP_IP;
    default:
        return ADF_NBUF_TX_CKSUM_NONE;
    }
}

a_status_t
__adf_nbuf_get_vlan_info(adf_net_handle_t hdl, struct sk_buff *skb,
                         adf_net_vlanhdr_t *vlan)
{
     return A_STATUS_OK;
}

a_uint8_t
__adf_nbuf_get_tid(struct sk_buff *skb)
{
    return skb->priority;
}

void
__adf_nbuf_set_tid(struct sk_buff *skb, a_uint8_t tid)
{
        skb->priority = tid;
}

a_uint8_t
__adf_nbuf_get_exemption_type(struct sk_buff *skb)
{
    return ADF_NBUF_EXEMPT_NO_EXEMPTION;
}

void
__adf_nbuf_dmamap_set_cb(__adf_os_dma_map_t dmap, void *cb, void *arg)
{
    return;
}

void
__adf_nbuf_reg_trace_cb(adf_nbuf_trace_update_t cb_func_ptr)
{
   trace_update_cb = cb_func_ptr;
   return;
}

a_status_t
__adf_nbuf_is_dhcp_pkt(struct sk_buff *skb)
{
   a_uint16_t    SPort;
   a_uint16_t    DPort;

    SPort = (a_uint16_t)(*(a_uint16_t *)(skb->data + ADF_NBUF_TRAC_IPV4_OFFSET +
                                     ADF_NBUF_TRAC_IPV4_HEADER_SIZE));
    DPort = (a_uint16_t)(*(a_uint16_t *)(skb->data + ADF_NBUF_TRAC_IPV4_OFFSET +
                                     ADF_NBUF_TRAC_IPV4_HEADER_SIZE + sizeof(a_uint16_t)));

    if (((ADF_NBUF_TRAC_DHCP_SRV_PORT == adf_os_cpu_to_be16(SPort)) &&
       (ADF_NBUF_TRAC_DHCP_CLI_PORT == adf_os_cpu_to_be16(DPort))) ||
       ((ADF_NBUF_TRAC_DHCP_CLI_PORT == adf_os_cpu_to_be16(SPort)) &&
       (ADF_NBUF_TRAC_DHCP_SRV_PORT == adf_os_cpu_to_be16(DPort))))
    {
        return A_STATUS_OK;
    }
    else
    {
        return A_STATUS_FAILED;
    }
}

a_status_t
__adf_nbuf_is_eapol_pkt(struct sk_buff *skb)
{
    a_uint16_t    ether_type;

    ether_type = (a_uint16_t)(*(a_uint16_t *)(skb->data + ADF_NBUF_TRAC_ETH_TYPE_OFFSET));
    if (ADF_NBUF_TRAC_EAPOL_ETH_TYPE == adf_os_cpu_to_be16(ether_type))
    {
        return A_STATUS_OK;
    }
    else
    {
        return A_STATUS_FAILED;
    }
}

#ifdef QCA_PKT_PROTO_TRACE
void
__adf_nbuf_trace_update(struct sk_buff *buf, char *event_string)
{
   char string_buf[NBUF_PKT_TRAC_MAX_STRING];

   if ((!trace_update_cb) || (!event_string)) {
      return;
   }

   if (!adf_nbuf_trace_get_proto_type(buf)) {
      return;
   }

   /* Buffer over flow */
   if (NBUF_PKT_TRAC_MAX_STRING <=
       (adf_os_str_len(event_string) + NBUF_PKT_TRAC_PROTO_STRING)) {
      return;
   }

   adf_os_mem_zero(string_buf,
                   NBUF_PKT_TRAC_MAX_STRING);
   adf_os_mem_copy(string_buf,
                   event_string, adf_os_str_len(event_string));
   if (NBUF_PKT_TRAC_TYPE_EAPOL &
       adf_nbuf_trace_get_proto_type(buf)) {
      adf_os_mem_copy(string_buf + adf_os_str_len(event_string),
                      "EPL",
                      NBUF_PKT_TRAC_PROTO_STRING);
   }
   else if (NBUF_PKT_TRAC_TYPE_DHCP &
            adf_nbuf_trace_get_proto_type(buf)) {
      adf_os_mem_copy(string_buf + adf_os_str_len(event_string),
                      "DHC",
                      NBUF_PKT_TRAC_PROTO_STRING);
   } else if (NBUF_PKT_TRAC_TYPE_MGMT_ACTION &
              adf_nbuf_trace_get_proto_type(buf)) {
      adf_os_mem_copy(string_buf + adf_os_str_len(event_string),
                      "MACT",
                      NBUF_PKT_TRAC_PROTO_STRING);
   }

   trace_update_cb(string_buf);
   return;
}
#endif /* QCA_PKT_PROTO_TRACE */

EXPORT_SYMBOL(__adf_nbuf_alloc);
#ifdef QCA_ARP_SPOOFING_WAR
EXPORT_SYMBOL(__adf_rx_nbuf_alloc);
#endif
EXPORT_SYMBOL(__adf_nbuf_free);
EXPORT_SYMBOL(__adf_nbuf_ref);
EXPORT_SYMBOL(__adf_nbuf_shared);
EXPORT_SYMBOL(__adf_nbuf_frag_info);
EXPORT_SYMBOL(__adf_nbuf_dmamap_create);
EXPORT_SYMBOL(__adf_nbuf_dmamap_destroy);
EXPORT_SYMBOL(__adf_nbuf_map);
EXPORT_SYMBOL(__adf_nbuf_unmap);
EXPORT_SYMBOL(__adf_nbuf_map_single);
EXPORT_SYMBOL(__adf_nbuf_unmap_single);
EXPORT_SYMBOL(__adf_nbuf_dmamap_info);
EXPORT_SYMBOL(__adf_nbuf_set_rx_cksum);
EXPORT_SYMBOL(__adf_nbuf_get_tx_cksum);
EXPORT_SYMBOL(__adf_nbuf_get_vlan_info);
EXPORT_SYMBOL(__adf_nbuf_get_tid);
EXPORT_SYMBOL(__adf_nbuf_set_tid);
EXPORT_SYMBOL(__adf_nbuf_get_exemption_type);
EXPORT_SYMBOL(__adf_nbuf_dmamap_set_cb);
EXPORT_SYMBOL(__adf_nbuf_is_dhcp_pkt);
EXPORT_SYMBOL(__adf_nbuf_is_eapol_pkt);
