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

//==============================================================================
// This file contains the definitions of the basic atheros data types.
// It is used to map the data types in atheros files to a platform specific
// type.
//
// Author(s): ="Atheros"
//==============================================================================
#ifndef _A_OSAPI_H_
#define _A_OSAPI_H_

#if defined(__linux__) && !defined(LINUX_EMULATION)
#include "osapi_linux.h"
#endif

/*=== the following primitives have the same definition for all platforms ===*/

#define A_COMPILE_TIME_ASSERT(assertion_name, predicate) \
    typedef char assertion_name[(predicate) ? 1 : -1]

/*
 * If N is a power of 2, then N and N-1 are orthogonal
 * (N-1 has all the least-significant bits set which are zero in N)
 * so  N ^ (N-1) = (N << 1) - 1
 */
#define A_COMPILE_TIME_ASSERT_IS_PWR2(assertion_name, value) \
    A_COMPILE_TIME_ASSERT(assertion_name,                    \
        (((value) ^ ((value)-1)) == ((value) << 1) - 1))

#ifndef __ubicom32__
#define HIF_MALLOC_DIAGMEM(osdev, size, pa, context, retry) \
    OS_MALLOC_CONSISTENT(osdev, size, pa, context, retry)
#define HIF_FREE_DIAGMEM(osdev, size, vaddr, pa, context) \
    OS_FREE_CONSISTENT(osdev, size, vaddr, pa, context)
#define HIF_DIAGMEM_SYNC(osdev, pa, size, dir, context)
#else
#define HIF_MALLOC_DIAGMEM(osdev, size, pa, context, retry) \
    OS_MALLOC_NONCONSISTENT(osdev, size, pa, context, retry)
#define HIF_FREE_DIAGMEM(osdev, size, vaddr, pa, context) \
    OS_FREE_NONCONSISTENT(osdev, size, vaddr, pa, context)
#define HIF_DIAGMEM_SYNC(osdev, pa, size, dir, context) \
    OS_SYNC_SINGLE(osdev, pa, size, dir, context)
#endif /* ubicom32 */

#endif /* _OSAPI_H_ */
