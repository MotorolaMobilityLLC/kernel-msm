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

#ifndef __UTILSAPI_H
#define __UTILSAPI_H

#include <stdarg.h>
#include <sir_common.h>
#include "ani_global.h"
#include "sys_wrapper.h"
#include "wlan_vdev_mlme_main.h"
#include "wlan_vdev_mlme_api.h"

/**
 * sir_swap_u16()
 *
 * FUNCTION:
 * This function is called to swap two U8s of an uint16_t value
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  val    uint16_t value to be uint8_t swapped
 * @return        Swapped uint16_t value
 */

static inline uint16_t sir_swap_u16(uint16_t val)
{
	return ((val & 0x00FF) << 8) | ((val & 0xFF00) >> 8);
} /*** end sir_swap_u16() ***/

/**
 * sir_swap_u16if_needed()
 *
 * FUNCTION:
 * This function is called to swap two U8s of an uint16_t value depending
 * on endiannes of the target processor/compiler the software is
 * running on
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  val    uint16_t value to be uint8_t swapped
 * @return        Swapped uint16_t value
 */

static inline uint16_t sir_swap_u16if_needed(uint16_t val)
{
#ifndef ANI_LITTLE_BYTE_ENDIAN
	return sir_swap_u16(val);
#else
	return val;
#endif
} /*** end sir_swap_u16if_needed() ***/

/**
 * sir_swap_u32()
 *
 * FUNCTION:
 * This function is called to swap four U8s of an uint32_t value
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  val    uint32_t value to be uint8_t swapped
 * @return        Swapped uint32_t value
 */

static inline uint32_t sir_swap_u32(uint32_t val)
{
	return (val << 24) |
		(val >> 24) |
		((val & 0x0000FF00) << 8) | ((val & 0x00FF0000) >> 8);
} /*** end sir_swap_u32() ***/

/**
 * sir_swap_u32if_needed()
 *
 * FUNCTION:
 * This function is called to swap U8s of an uint32_t value depending
 * on endiannes of the target processor/compiler the software is
 * running on
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  val    uint32_t value to be uint8_t swapped
 * @return        Swapped uint32_t value
 */

static inline uint32_t sir_swap_u32if_needed(uint32_t val)
{
#ifndef ANI_LITTLE_BYTE_ENDIAN
	return sir_swap_u32(val);
#else
	return val;
#endif
} /*** end sir_swap_u32if_needed() ***/

/**
 * sir_swap_u32_buf
 *
 * FUNCTION:
 * It swaps N dwords into the same buffer
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of uint32_t array
 * @return void
 *
 */

/**
 * sir_read_u32_n
 *
 * FUNCTION:
 * It reads a 32 bit number from the byte array in network byte order
 * i.e. the least significant byte first
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of  byte array
 * @return 32 bit value
 */

static inline uint32_t sir_read_u32_n(uint8_t *ptr)
{
	return (*(ptr) << 24) |
		(*(ptr + 1) << 16) | (*(ptr + 2) << 8) | (*(ptr + 3));
}

/**
 * sir_read_u16
 *
 * FUNCTION:
 * It reads a 16 bit number from the byte array in NON-network byte order
 * i.e. the least significant byte first
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of  byte array
 * @return 16 bit value
 */

static inline uint16_t sir_read_u16(uint8_t *ptr)
{
	return (*ptr) | (*(ptr + 1) << 8);
}

/**
 * sir_read_u32
 *
 * FUNCTION:
 * It reads a 32 bit number from the byte array in NON-network byte order
 * i.e. the least significant byte first
 *
 * LOGIC:
 *
 * ASSUMPTIONS:
 * None.
 *
 * NOTE:
 *
 * @param  ptr address of  byte array
 * @return 32 bit value
 */

static inline uint32_t sir_read_u32(uint8_t *ptr)
{
	return (*(ptr)) |
		(*(ptr + 1) << 8) | (*(ptr + 2) << 16) | (*(ptr + 3) << 24);
}

/* / Copy a MAC address from 'from' to 'to' */
static inline void sir_copy_mac_addr(uint8_t to[], uint8_t from[])
{
#if defined(_X86_)
	uint32_t align = (0x3 & ((uint32_t) to | (uint32_t) from));

	if (align == 0) {
		*((uint16_t *) &(to[4])) = *((uint16_t *) &(from[4]));
		*((uint32_t *) to) = *((uint32_t *) from);
	} else if (align == 2) {
		*((uint16_t *) &to[4]) = *((uint16_t *) &from[4]);
		*((uint16_t *) &to[2]) = *((uint16_t *) &from[2]);
		*((uint16_t *) &to[0]) = *((uint16_t *) &from[0]);
	} else {
		to[5] = from[5];
		to[4] = from[4];
		to[3] = from[3];
		to[2] = from[2];
		to[1] = from[1];
		to[0] = from[0];
	}
#else
	to[0] = from[0];
	to[1] = from[1];
	to[2] = from[2];
	to[3] = from[3];
	to[4] = from[4];
	to[5] = from[5];
#endif
}

static inline uint8_t sir_compare_mac_addr(uint8_t addr1[], uint8_t addr2[])
{
#if defined(_X86_)
	uint32_t align = (0x3 & ((uint32_t) addr1 | (uint32_t) addr2));

	if (align == 0) {
		return (*((uint16_t *) &(addr1[4])) ==
			 *((uint16_t *) &(addr2[4])))
			&& (*((uint32_t *) addr1) == *((uint32_t *) addr2));
	} else if (align == 2) {
		return (*((uint16_t *) &addr1[4]) ==
			 *((uint16_t *) &addr2[4]))
			&& (*((uint16_t *) &addr1[2]) ==
			    *((uint16_t *) &addr2[2]))
			&& (*((uint16_t *) &addr1[0]) ==
			    *((uint16_t *) &addr2[0]));
	} else {
		return (addr1[5] == addr2[5]) &&
			(addr1[4] == addr2[4]) &&
			(addr1[3] == addr2[3]) &&
			(addr1[2] == addr2[2]) &&
			(addr1[1] == addr2[1]) && (addr1[0] == addr2[0]);
	}
#else
	return (addr1[0] == addr2[0]) &&
		(addr1[1] == addr2[1]) &&
		(addr1[2] == addr2[2]) &&
		(addr1[3] == addr2[3]) &&
		(addr1[4] == addr2[4]) && (addr1[5] == addr2[5]);
#endif
}

/*
 * converts uint16_t CW value to 4 bit value to be inserted in IE
 */
static inline uint8_t convert_cw(uint16_t cw)
{
	uint8_t val = 0;

	while (cw > 0) {
		val++;
		cw >>= 1;
	}
	if (val > 15)
		return 0xF;
	return val;
}

/* The user priority to AC mapping is such:
 *   UP(1, 2) ---> AC_BK(1)
 *   UP(0, 3) ---> AC_BE(0)
 *   UP(4, 5) ---> AC_VI(2)
 *   UP(6, 7) ---> AC_VO(3)
 */
#define WLAN_UP_TO_AC_MAP            0x33220110
#define upToAc(up)                ((WLAN_UP_TO_AC_MAP >> ((up) << 2)) & 0x03)

#endif /* __UTILSAPI_H */
