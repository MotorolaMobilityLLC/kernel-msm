/*
 * Copyright (c) 2010 The Linux Foundation. All rights reserved.
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


#ifndef ADF_OS_ATOMIC_PVT_H
#define ADF_OS_ATOMIC_PVT_H

#include <adf_os_types.h> /* a_status_t */

#include <asm/atomic.h>

typedef atomic_t __adf_os_atomic_t;

static inline a_status_t
__adf_os_atomic_init(__adf_os_atomic_t *v)
{
    atomic_set(v, 0);

    return A_STATUS_OK;
}

static inline a_uint32_t
__adf_os_atomic_read(__adf_os_atomic_t *v)
{
    return (atomic_read(v));
}
static inline void
__adf_os_atomic_inc(__adf_os_atomic_t *v)
{
    atomic_inc(v);
}

static inline void
__adf_os_atomic_dec(__adf_os_atomic_t *v)
{
    atomic_dec(v);
}

static inline void
__adf_os_atomic_add(int i, __adf_os_atomic_t *v)
{
    atomic_add(i, v);
}

static inline a_uint32_t
 __adf_os_atomic_dec_and_test(__adf_os_atomic_t *v)
{
    return(atomic_dec_and_test(v));
}

static inline void
 __adf_os_atomic_set(__adf_os_atomic_t *v, int i)
{
    atomic_set(v, i);
}
#endif
