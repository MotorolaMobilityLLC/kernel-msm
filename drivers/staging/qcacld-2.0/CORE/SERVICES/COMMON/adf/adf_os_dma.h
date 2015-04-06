/*
 * Copyright (c) 2011,2013 The Linux Foundation. All rights reserved.
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
 * @ingroup adf_os_public
 * @file adf_os_dma.h
 * This file abstracts DMA operations.
 */

#ifndef _ADF_OS_DMA_H
#define _ADF_OS_DMA_H

#include <adf_os_types.h>
#include <adf_os_dma_pvt.h>

/*
 * @brief a dma address representation of a platform
 */

/**
 * @brief Allocate a DMA buffer and map it to local bus address space
 *
 * @param[in]  osdev     platform device instance
 * @param[in]  size      DMA buffer size
 * @param[in]  coherent  0 => cached.
 * @param[out] dmap      opaque coherent memory handle
 *
 * @return     returns the virtual address of the memory
 */
static inline void *
adf_os_dmamem_alloc(adf_os_device_t     osdev,
                    adf_os_size_t       size,
                    a_bool_t            coherent,
                    adf_os_dma_map_t   *dmap)
{
    return __adf_os_dmamem_alloc(osdev, size, coherent, dmap);
}

/**
 * @brief Free a previously mapped DMA buffer
 *
 * @param[in] osdev     platform device instance
 * @param[in] size      DMA buffer size
 * @param[in] coherent  0 => cached.
 * @param[in] vaddr     virtual address of DMA buffer
 * @param[in] dmap      memory handle
 */
static inline void
adf_os_dmamem_free(adf_os_device_t    osdev,
                   adf_os_size_t      size,
                   a_bool_t           coherent,
                   void              *vaddr,
                   adf_os_dma_map_t   dmap)
{
    __adf_os_dmamem_free(osdev, size, coherent, vaddr, dmap);
}

/**
 * @brief given a dmamem map, returns the (bus) address
 *
 * @param[in] dmap      memory handle
 *
 * @return the (bus) address
 */
static inline adf_os_dma_addr_t
adf_os_dmamem_map2addr(adf_os_dma_map_t dmap)
{
    return(__adf_os_dmamem_map2addr(dmap));
}

/**
 * @brief Flush and invalidate cache for a given dmamem map
 *
 * @param[in] osdev     platform device instance
 * @param[in] dmap		mem handle
 * @param[in] op        op code for sync type, (see @ref adf_os_types.h)
 */
static inline void
adf_os_dmamem_cache_sync(adf_os_device_t      osdev,
                         adf_os_dma_map_t     dmap,
                         adf_os_cache_sync_t  op)
{
    __adf_os_dmamem_cache_sync(osdev, dmap, op);
}

/**
 * @brief Get the cpu cache line size
 *
 * @return The CPU cache line size in bytes.
 */
static inline adf_os_size_t
adf_os_cache_line_size(void)
{
    return __adf_os_cache_line_size();
}

/**
 * @brief invalidate the virtual address range specified by
 *        start and end addresses.
 * Note: This does not write back the cache entries.
 *
 * @return void
 */
static inline void
adf_os_invalidate_range(void * start, void * end)
{
    return __adf_os_invalidate_range(start, end);
}

#endif
