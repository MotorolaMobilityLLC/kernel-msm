/*
 * Copyright (c) 2011 The Linux Foundation. All rights reserved.
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
 * @file adf_os_time.h
 * This file abstracts time related functionality.
 */
#ifndef _ADF_OS_TIME_H
#define _ADF_OS_TIME_H

#include <adf_os_time_pvt.h>
#ifdef CONFIG_CNSS
#include <net/cnss.h>
#endif

typedef __adf_time_t   adf_os_time_t;

/**
 * @brief count the number of ticks elapsed from the time when
 *        the system booted
 *
 * @return ticks
 */
static inline unsigned long
adf_os_ticks(void)
{
	return __adf_os_ticks();
}

/**
 * @brief convert ticks to milliseconds
 *
 * @param[in] ticks number of ticks
 * @return time in milliseconds
 */
static inline a_uint32_t
adf_os_ticks_to_msecs(unsigned long clock_ticks)
{
	return (__adf_os_ticks_to_msecs(clock_ticks));
}

/**
 * @brief convert milliseconds to ticks
 *
 * @param[in] time in milliseconds
 * @return number of ticks
 */
static inline unsigned long
adf_os_msecs_to_ticks(a_uint32_t msecs)
{
	return (__adf_os_msecs_to_ticks(msecs));
}

/**
 * @brief Return a monotonically increasing time. This increments once per HZ ticks
 */
static inline unsigned long
adf_os_getuptime(void)
{
    return (__adf_os_getuptime());
}

/**
 * @brief Return current timestamp.
 */
static inline unsigned long
adf_os_gettimestamp(void)
{
    return (__adf_os_gettimestamp());
}

/**
 * @brief Delay in microseconds
 *
 * @param[in] microseconds to delay
 */
static inline void
adf_os_udelay(int usecs)
{
    __adf_os_udelay(usecs);
}

/**
 * @brief Delay in milliseconds.
 *
 * @param[in] milliseconds to delay
 */
static inline void
adf_os_mdelay(int msecs)
{
    __adf_os_mdelay(msecs);
}

/**
 * @brief Check if _a is later than _b.
 */
#define adf_os_time_after(_a, _b)       __adf_os_time_after(_a, _b)

/**
 * @brief Check if _a is prior to _b.
 */
#define adf_os_time_before(_a, _b)      __adf_os_time_before(_a, _b)

/**
 * @brief Check if _a atleast as recent as _b, if not later.
 */
#define adf_os_time_after_eq(_a, _b)    __adf_os_time_after_eq(_a, _b)

/**
 * @brief Get kernel boot time.
 *
 * @return Time in microseconds
 */
static inline a_uint64_t adf_get_boottime(void)
{
#ifdef CONFIG_CNSS
   struct timespec ts;

   cnss_get_boottime(&ts);

   return (((a_uint64_t)ts.tv_sec * 1000000) + (ts.tv_nsec / 1000));
#else
   return adf_os_ticks_to_msecs(adf_os_ticks()) * 1000;
#endif /* CONFIG_CNSS */
}
#endif
