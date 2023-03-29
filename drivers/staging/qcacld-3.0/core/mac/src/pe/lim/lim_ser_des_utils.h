/*
 * Copyright (c) 2011-2019 The Linux Foundation. All rights reserved.
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
 *
 * This file lim_ser_des_utils.h contains the utility definitions
 * LIM uses while processing messages from upper layer software
 * modules
 * Author:        Chandra Modumudi
 * Date:          10/20/02
 * History:-
 * Date           Modified by    Modification Information
 * --------------------------------------------------------------------
 */
#ifndef __LIM_SERDES_UTILS_H
#define __LIM_SERDES_UTILS_H

#include "sir_api.h"
#include "ani_system_defs.h"
#include "sir_mac_prot_def.h"
#include "utils_api.h"
#include "lim_types.h"
#include "lim_prop_exts_utils.h"

/* Byte String <--> uint16_t/uint32_t copy functions */
static inline void lim_copy_u16(uint8_t *ptr, uint16_t u16Val)
{
#if ((defined(ANI_OS_TYPE_QNX) && defined(ANI_LITTLE_BYTE_ENDIAN)) ||	\
	(defined(ANI_OS_TYPE_ANDROID) && defined(ANI_LITTLE_BYTE_ENDIAN)))
	*ptr++ = (uint8_t) (u16Val & 0xff);
	*ptr = (uint8_t) ((u16Val >> 8) & 0xff);
#else
#error "Unknown combination of OS Type and endianness"
#endif
}

static inline uint16_t lim_get_u16(uint8_t *ptr)
{
#if ((defined(ANI_OS_TYPE_QNX) && defined(ANI_LITTLE_BYTE_ENDIAN)) ||	\
	(defined(ANI_OS_TYPE_ANDROID) && defined(ANI_LITTLE_BYTE_ENDIAN)))
	return ((uint16_t) (*(ptr + 1) << 8)) | ((uint16_t) (*ptr));
#else
#error "Unknown combination of OS Type and endianness"
#endif
}

static inline void lim_copy_u32(uint8_t *ptr, uint32_t u32Val)
{
#if ((defined(ANI_OS_TYPE_QNX) && defined(ANI_LITTLE_BYTE_ENDIAN)) ||	\
	(defined(ANI_OS_TYPE_ANDROID) && defined(ANI_LITTLE_BYTE_ENDIAN)))
	*ptr++ = (uint8_t) (u32Val & 0xff);
	*ptr++ = (uint8_t) ((u32Val >> 8) & 0xff);
	*ptr++ = (uint8_t) ((u32Val >> 16) & 0xff);
	*ptr = (uint8_t) ((u32Val >> 24) & 0xff);
#else
#error "Unknown combination of OS Type and endianness"
#endif
}

static inline uint32_t lim_get_u32(uint8_t *ptr)
{
#if ((defined(ANI_OS_TYPE_QNX) && defined(ANI_LITTLE_BYTE_ENDIAN)) ||	\
	(defined(ANI_OS_TYPE_ANDROID) && defined(ANI_LITTLE_BYTE_ENDIAN)))
	return ((*(ptr + 3) << 24) |
		(*(ptr + 2) << 16) | (*(ptr + 1) << 8) | (*(ptr)));
#else
#error "Unknown combination of OS Type and endianness"
#endif
}

/**
 * lim_copy_u16_be()- This API copies a u16 value in buffer
 * to network byte order
 * @ptr: pointer to buffer
 * @u16_val: value needs to be copied
 *
 * Return: None
 */
static inline void lim_copy_u16_be(uint8_t *ptr, uint16_t u16_val)
{
	ptr[0] = u16_val >> 8;
	ptr[1] = u16_val & 0xff;
}

/**
 * lim_copy_u16_be()- This API reads u16 value from network byte order buffer
 * @ptr: pointer to buffer
 *
 * Return: 16bit value
 */
static inline uint16_t lim_get_u16_be(uint8_t *buf)
{
	return (buf[0] << 8) | buf[1];
}
#endif /* __LIM_SERDES_UTILS_H */
