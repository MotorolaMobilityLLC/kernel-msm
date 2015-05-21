/**
 * \file
 *
 * \authors   Pete Skeggs <pete.skeggs@emmicro-us.com>
 *
 * \brief     portability macros
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


#ifndef _PORTABLE_H_
#define _PORTABLE_H_

#if defined(__KERNEL__)
#define PREPACK
#define MIDPACK __attribute__((__packed__))
#define POSTPACK
#define VOLATILE __volatile__
#define ATOMIC_INT int
#include <linux/swab.h> // for swab32 (swap bytes within a 32 bit word)

#elif defined(_MSC_VER)
#include "windows.h"
#define PREPACK __pragma(pack(push, 1))
#define MIDPACK
#define POSTPACK __pragma(pack(pop))
#define VOLATILE volatile
#define ATOMIC_INT int
#define swab32(x) ((u32)(				\
	(((u32)(x) & (u32)0x000000ffUL) << 24) |		\
	(((u32)(x) & (u32)0x0000ff00UL) <<  8) |		\
	(((u32)(x) & (u32)0x00ff0000UL) >>  8) |		\
	(((u32)(x) & (u32)0xff000000UL) >> 24)))

#elif defined(__GNUC__)
#define PREPACK
#define MIDPACK __attribute__((__packed__))
#define POSTPACK
#define VOLATILE __volatile__
#define ATOMIC_INT int
#if defined(__gnu_linux__) || defined(__CYGWIN__)
#include <linux/swab.h> // for swab32 (swap bytes within a 32 bit word)
#else
#define swab32(x) ((u32)(				\
	(((u32)(x) & (u32)0x000000ffUL) << 24) |		\
	(((u32)(x) & (u32)0x0000ff00UL) <<  8) |		\
	(((u32)(x) & (u32)0x00ff0000UL) >>  8) |		\
	(((u32)(x) & (u32)0xff000000UL) >> 24)))
#endif
#endif

#ifndef BIT
	#define BIT(x)	(1<<x)
#endif


#endif
