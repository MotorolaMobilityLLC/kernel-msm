/*
 * Copyright (c) 2011-2015 The Linux Foundation. All rights reserved.
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
 * @file adf_os_util.h
 * This file defines utility functions.
 */

#ifndef _ADF_OS_UTIL_H
#define _ADF_OS_UTIL_H

#include <adf_os_util_pvt.h>

/**
 * @brief Compiler-dependent macro denoting code likely to execute.
 */
#define adf_os_unlikely(_expr)     __adf_os_unlikely(_expr)

/**
 * @brief Compiler-dependent macro denoting code unlikely to execute.
 */
#define adf_os_likely(_expr)       __adf_os_likely(_expr)

/**
 * @brief read memory barrier.
 */
#define adf_os_wmb()                __adf_os_wmb()

/**
 * @brief write memory barrier.
 */
#define adf_os_rmb()                __adf_os_rmb()

/**
 * @brief read + write memory barrier.
 */
#define adf_os_mb()                 __adf_os_mb()

/**
 * @brief return the lesser of a, b
 */
#define adf_os_min(_a, _b)          __adf_os_min(_a, _b)

/**
 * @brief return the larger of a, b
 */
#define adf_os_max(_a, _b)          __adf_os_max(_a, _b)

/**
 * @brief assert "expr" evaluates to false.
 */
#ifdef ADF_OS_DEBUG
#define adf_os_assert(expr)         __adf_os_assert(expr)
#else
#define adf_os_assert(expr)
#endif /* ADF_OS_DEBUG */

/**
 * @brief alway assert "expr" evaluates to false.
 */
#define adf_os_assert_always(expr)  __adf_os_assert(expr)

/**
 * @brief warn & dump backtrace if expr evaluates true
 */
#define adf_os_warn(expr)           __adf_os_warn(expr)
/**
 * @brief supply pseudo-random numbers
 */
static inline void adf_os_get_rand(adf_os_handle_t  hdl,
                                   a_uint8_t       *ptr,
                                   a_uint32_t       len)
{
    __adf_os_get_rand(hdl, ptr, len);
}



/**
 * @brief return the absolute value of a
 */
#define adf_os_abs(_a)              __adf_os_abs(_a)

/**
 * @brief replace with the name of the current function
 */
#define adf_os_function             __adf_os_function



/**
 * @brief return square root
 */

/**
 * @brief  Math function for getting a square root
 *
 * @param[in] x     Number to compute the sqaure root
 *
 * @return  Sqaure root as integer
 */
static adf_os_inline a_uint32_t
adf_os_int_sqrt(a_uint32_t x)
{
	return __adf_os_int_sqrt(x);
}

/**
 * @brief initialize completion structure
 *
 * @param[in] ptr - completion structure
 */
static inline void
adf_os_init_completion(adf_os_comp_t *ptr)
{
    __adf_os_init_completion(ptr);
}

/**
 * @brief re-initialize completion structure
 *
 * @param[in] comp - completion structure
 */
static inline void
adf_os_re_init_completion(adf_os_comp_t comp)
{
	__adf_os_re_init_completion(comp);
}

/**
 * @brief wait for completion till timeout
 * @param[in] ptr - completion structure
 * @param[in] timeout - timeout value in jiffies
 *
 * @Return: 0 if timed out, and positive on completion
 */
static inline unsigned long
adf_os_wait_for_completion_timeout(adf_os_comp_t *ptr, unsigned long timeout)
{
    return  __adf_os_wait_for_completion_timeout(ptr, timeout);
}

/**
 * @brief wake up the thread waiting for this completion
 *
 * @param[in] ptr - completion structure
 */
static inline void
adf_os_complete(adf_os_comp_t *ptr)
{
    __adf_os_complete(ptr);
}
#endif /*_ADF_OS_UTIL_H*/
