/**
 * \file
 *
 * \authors   Evan Lojewski <evan.lojewski@emmicro-us.com>
 *
 * \brief     Standard datatypes
 *
 * \copyright (C) 2013-2014 EM Microelectronic – US, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef TYPES_H
#define TYPES_H

#include "em718x_portable.h"

#if !defined(__KERNEL__)

#include <stdint.h>
typedef int8_t     s8;                                       /**< Signed 8 bit integer. */
typedef uint8_t    u8;                                       /**< Unsigned 8 bit integer. */
typedef int16_t    s16;                                      /**< Signed 16 bit integer. */
typedef uint16_t   u16;                                      /**< Unsigned 16 bit integer. */
typedef int32_t    s32;                                      /**< Signed 32 bit integer. */
typedef uint32_t   u32;                                      /**< Unsigned 32 bit integer. */
typedef int64_t    s64;                                      /**< Signed 64 bit integer. */
typedef uint64_t   u64;                                      /**< Unsigned 64 bit integer. */

#if !defined(__cplusplus)
typedef char       bool;                                     /**< Boolean. */
#endif

#else
#include <linux/types.h>
#endif

#ifndef TRUE
#define TRUE         1     /**< True */
#endif /* !TRUE */

#ifndef FALSE
#define FALSE        0     /**< False */
#endif /* !FALSE */

#ifndef NULL
#define NULL         ((void*)0)  /**< Null pointer */
#endif /* NULL */




#endif /* TYPES_H */
