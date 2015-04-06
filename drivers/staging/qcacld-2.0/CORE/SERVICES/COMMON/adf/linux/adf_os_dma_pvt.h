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


#ifndef _ADF_CMN_OS_DMA_PVT_H
#define _ADF_CMN_OS_DMA_PVT_H

#include <adf_os_types.h>
#include <adf_os_mem.h>
#include <linux/types.h>
#include <linux/slab.h>
#include <linux/device.h>
#include <linux/dma-mapping.h>
#include <linux/cache.h>
#include <asm/io.h>
#include <asm/cacheflush.h>

#include <adf_os_types.h>
#include <adf_os_util.h>

/**
 * XXX:error handling
 *
 * @brief allocate a DMA buffer mapped to local bus Direction
 *        doesnt matter, since this API is called at init time.
 *
 * @param size
 * @param coherentSMP_CACHE_BYTES
 * @param dmap
 *
 * @return void*
 */
static inline void *
__adf_os_dmamem_alloc(adf_os_device_t     osdev,
                      size_t       size,
                      a_bool_t            coherent,
                      __adf_os_dma_map_t   *dmap)
{
    void               *vaddr;
    __adf_os_dma_map_t  lmap;

   lmap = kzalloc(sizeof(struct __adf_os_dma_map), GFP_KERNEL);

   adf_os_assert(lmap);

   lmap->nsegs = 1;
   lmap->coherent = coherent;

   if(coherent)
       vaddr = dma_alloc_coherent(osdev->dev, size, &lmap->seg[0].daddr,
                                  GFP_ATOMIC);
   else
       vaddr = dma_alloc_noncoherent(osdev->dev, size, &lmap->seg[0].daddr,
                                     GFP_ATOMIC);

   adf_os_assert(vaddr);

   lmap->seg[0].len = size;
   lmap->mapped = 1;

   (*dmap) = lmap;

   return vaddr;
}

/*
 * Free a previously mapped DMA buffer
 * Direction doesnt matter, since this API is called at closing time.
 */
static inline void
__adf_os_dmamem_free(adf_os_device_t    osdev, __adf_os_size_t size,
                     a_bool_t coherent, void *vaddr, __adf_os_dma_map_t dmap)
{
    adf_os_assert(dmap->mapped);

    if(coherent)
        dma_free_coherent(osdev->dev, size, vaddr, dmap->seg[0].daddr);
    else
        dma_free_noncoherent(osdev->dev, size, vaddr, dmap->seg[0].daddr);

    kfree(dmap);
}


#define __adf_os_dmamem_map2addr(_dmap)    ((_dmap)->seg[0].daddr)

static inline void
__adf_os_dmamem_cache_sync(__adf_os_device_t osdev, __adf_os_dma_map_t dmap,
                           adf_os_cache_sync_t sync)
{
    if(!dmap->coherent){
        dma_sync_single_for_cpu(osdev->dev, dmap->seg[0].daddr, dmap->seg[0].len,
                        DMA_BIDIRECTIONAL);
    }
}
static inline adf_os_size_t
__adf_os_cache_line_size(void)
{
    return SMP_CACHE_BYTES;
}

static inline void
__adf_os_invalidate_range(void * start, void * end)
{
#ifdef MSM_PLATFORM
    dmac_inv_range(start, end);
#else
    //TODO figure out how to invalidate cache on x86 and other non-MSM platform
    __adf_os_print("Cache Invalidate not yet implemented for non-MSM platform\n");
    return;
#endif
}


#endif /*_ADF_CMN_OS_DMA_PVT_H*/
