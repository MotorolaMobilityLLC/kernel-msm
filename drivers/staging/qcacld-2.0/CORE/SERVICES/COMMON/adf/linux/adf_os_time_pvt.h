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


#ifndef _ADF_CMN_OS_TIME_PVT_H
#define _ADF_CMN_OS_TIME_PVT_H

#include <linux/jiffies.h>
#include <linux/delay.h>

typedef unsigned long __adf_time_t;

static inline __adf_time_t
__adf_os_ticks(void)
{
    return (jiffies);
}
static inline uint32_t
__adf_os_ticks_to_msecs(unsigned long ticks)
{
    return (jiffies_to_msecs(ticks));
}
static inline __adf_time_t
__adf_os_msecs_to_ticks(a_uint32_t msecs)
{
    return (msecs_to_jiffies(msecs));
}
static inline __adf_time_t
__adf_os_getuptime(void)
{
    return jiffies;
}

static inline __adf_time_t
__adf_os_gettimestamp(void)
{
    return ((jiffies / HZ) * 1000) + (jiffies % HZ) * (1000 / HZ);
}

static inline void
__adf_os_udelay(a_uint32_t usecs)
{
#ifdef CONFIG_ARM
    /*
    ** This is in support of XScale build.  They have a limit on the udelay
    ** value, so we have to make sure we don't approach the limit
    */

    a_uint32_t    mticks;
    a_uint32_t    leftover;
    int                i;

    /*
    ** slice into 1024 usec chunks (simplifies calculation)
    */

    mticks = usecs >> 10;
    leftover = usecs - (mticks << 10);

    for(i = 0;i < mticks;i++)
    {
        udelay(1024);
    }

    udelay(leftover);

#else
    /*
     * Normal Delay functions. Time specified in microseconds.
     */
    udelay(usecs);

#endif
}

static inline void
__adf_os_mdelay(a_uint32_t msecs)
{
    mdelay(msecs);
}

/**
 * @brief Check if _a is later than _b.
 */
static inline a_bool_t
__adf_os_time_after(__adf_time_t a, __adf_time_t b)
{
    return ((long)(b) - (long)(a) < 0);
}

/**
 * @brief Check if _a is prior to _b.
 */
static inline a_bool_t
__adf_os_time_before(__adf_time_t a, __adf_time_t b)
{
    return __adf_os_time_after(b,a);
}

/**
 * @brief Check if _a atleast as recent as _b, if not later.
 */
static inline a_bool_t
__adf_os_time_after_eq(__adf_time_t a, __adf_time_t b)
{
    return ((long)(a) - (long)(b) >= 0);
}

#endif
