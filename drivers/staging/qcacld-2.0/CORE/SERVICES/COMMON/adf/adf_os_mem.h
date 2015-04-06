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
 * @file adf_os_mem.h
 * This file abstracts memory operations.
 */

#ifndef _ADF_OS_MEM_H
#define _ADF_OS_MEM_H

#include <adf_os_types.h>
#include <adf_os_mem_pvt.h>
#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
#ifdef CONFIG_CNSS
#include <net/cnss.h>
#else
#include <wcnss_api.h>
#endif
#endif

/**
 * @brief Allocate a memory buffer. Note this call can block.
 *
 * @param[in] size    buffer size
 *
 * @return Buffer pointer or NULL if there's not enough memory.
 */
static inline void *
adf_os_mem_alloc(adf_os_device_t osdev, adf_os_size_t size)
{
#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
    void *p_mem;
#endif

#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
   if (size > WCNSS_PRE_ALLOC_GET_THRESHOLD)
   {
       p_mem = wcnss_prealloc_get(size);
       if (NULL != p_mem)
           return p_mem;
   }
#endif

    return __adf_os_mem_alloc(osdev, size);
}

void *
adf_os_mem_alloc_outline(adf_os_device_t osdev, adf_os_size_t size);

/**
 * @brief Free malloc'ed buffer
 *
 * @param[in] buf     buffer pointer allocated by @ref adf_os_mem_alloc
 */
static inline void
adf_os_mem_free(void *buf)
{
#ifdef CONFIG_WCNSS_MEM_PRE_ALLOC
    if (wcnss_prealloc_put(buf))
    {
        return;
    }
#endif

    __adf_os_mem_free(buf);
}

void
adf_os_mem_free_outline(void *buf);

static inline  void *
adf_os_mem_alloc_consistent(
    adf_os_device_t osdev, adf_os_size_t size, adf_os_dma_addr_t *paddr, adf_os_dma_context_t mctx)
{
    return __adf_os_mem_alloc_consistent(osdev, size, paddr, mctx);
}

static inline void
adf_os_mem_free_consistent(
    adf_os_device_t osdev,
    adf_os_size_t size,
    void *vaddr,
    adf_os_dma_addr_t paddr,
    adf_os_dma_context_t memctx)
{
    __adf_os_mem_free_consistent(osdev, size, vaddr, paddr, memctx);
}

/**
 * @brief Move a memory buffer. Overlapping regions are not allowed.
 *
 * @param[in] dst     destination address
 * @param[in] src     source address
 * @param[in] size    buffer size
 */
static inline __ahdecl void
adf_os_mem_copy(void *dst, const void *src, adf_os_size_t size)
{
    __adf_os_mem_copy(dst, src, size);
}

/**
 * @brief Does a non-destructive copy of memory buffer
 *
 * @param[in] dst     destination address
 * @param[in] src     source address
 * @param[in] size    buffer size
 */
static inline void
adf_os_mem_move(void *dst, void *src, adf_os_size_t size)
{
	__adf_os_mem_move(dst,src,size);
}


/**
 * @brief Fill a memory buffer
 *
 * @param[in] buf   buffer to be filled
 * @param[in] b     byte to fill
 * @param[in] size  buffer size
 */
static inline void
adf_os_mem_set(void *buf, a_uint8_t b, adf_os_size_t size)
{
    __adf_os_mem_set(buf, b, size);
}


/**
 * @brief Zero a memory buffer
 *
 * @param[in] buf   buffer to be zeroed
 * @param[in] size  buffer size
 */
static inline __ahdecl void
adf_os_mem_zero(void *buf, adf_os_size_t size)
{
    __adf_os_mem_zero(buf, size);
}

void
adf_os_mem_zero_outline(void *buf, adf_os_size_t size);

/**
 * @brief Compare two memory buffers
 *
 * @param[in] buf1  first buffer
 * @param[in] buf2  second buffer
 * @param[in] size  buffer size
 *
 * @retval    0     equal
 * @retval    1     not equal
 */
static inline int
adf_os_mem_cmp(const void *buf1, const void *buf2, adf_os_size_t size)
{
    return __adf_os_mem_cmp(buf1, buf2, size);
}

/**
 * @brief Compare two strings
 *
 * @param[in] str1  First string
 * @param[in] str2  Second string
 *
 * @retval    0     equal
 * @retval    >0    not equal, if  str1  sorts lexicographically after str2
 * @retval    <0    not equal, if  str1  sorts lexicographically before str2
 */
static inline a_int32_t
adf_os_str_cmp(const char *str1, const char *str2)
{
    return __adf_os_str_cmp(str1, str2);
}

/**
 * @brief Returns the length of a string
 *
 * @param[in] str   input string
 *
 * @return    length of string
 */
static inline a_int32_t
adf_os_str_len(const char *str)
{
    return (a_int32_t)__adf_os_str_len(str);
}


#endif
