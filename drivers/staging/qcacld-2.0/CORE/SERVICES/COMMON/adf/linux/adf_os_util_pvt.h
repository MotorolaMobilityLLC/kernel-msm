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


#ifndef _ADF_CMN_OS_UTIL_PVT_H
#define _ADF_CMN_OS_UTIL_PVT_H

#include <linux/compiler.h>
#include <linux/kernel.h>
#include <linux/types.h>

#include <linux/random.h>

//#include <asm/system.h>
#include <adf_os_types.h>
/*
 * Generic compiler-dependent macros if defined by the OS
 */

#define __adf_os_unlikely(_expr)   unlikely(_expr)
#define __adf_os_likely(_expr)     likely(_expr)

/**
 * @brief memory barriers.
 */
#define __adf_os_wmb()                wmb()
#define __adf_os_rmb()                rmb()
#define __adf_os_mb()                 mb()

#define __adf_os_min(_a, _b)         ((_a) < (_b) ? _a : _b)
#define __adf_os_max(_a, _b)         ((_a) > (_b) ? _a : _b)

#define __adf_os_abs(_a)             __builtin_abs(_a)

/**
 * @brief Assert
 */
#define __adf_os_assert(expr)  do {    \
    if(unlikely(!(expr))) {                                 \
        printk(KERN_ERR "Assertion failed! %s:%s %s:%d\n",   \
              #expr, __FUNCTION__, __FILE__, __LINE__);      \
        dump_stack();                                      \
        BUG_ON(1);          \
    }     \
}while(0)

/**
 * @brief Warning
 */
#define __adf_os_warn(cond) ({                      \
    int __ret_warn = !!(cond);              \
    if (unlikely(__ret_warn)) {                 \
        printk("WARNING: at %s:%d %s()\n", __FILE__,        \
            __LINE__, __FUNCTION__);            \
        dump_stack();                       \
    }                               \
    unlikely(__ret_warn);                   \
})

/**
 * @brief replace with the name of the function
 */
#define __adf_os_function   __FUNCTION__

static inline a_status_t
__adf_os_get_rand(adf_os_handle_t  hdl, uint8_t *ptr, uint32_t  len)
{
    get_random_bytes(ptr, len);

    return A_STATUS_OK;
}

/**
 * @brief return square root
 */
static __adf_os_inline a_uint32_t __adf_os_int_sqrt(a_uint32_t x)
{
	return int_sqrt(x);
}

/**
 * @brief completion structure initialization
 */
static __adf_os_inline void
__adf_os_init_completion(adf_os_comp_t *ptr)
{
	init_completion(ptr);
}


/**
 * @brief wait for completion till timeout
 *
 * @Return: 0 if timed out, and positive on completion
 */
static __adf_os_inline unsigned long
__adf_os_wait_for_completion_timeout(adf_os_comp_t *ptr, unsigned long timeout)
{
	return wait_for_completion_timeout(ptr, timeout);
}

static __adf_os_inline void
__adf_os_complete(adf_os_comp_t *ptr)
{
	complete(ptr);
}
#endif /*_ADF_CMN_OS_UTIL_PVT_H*/
