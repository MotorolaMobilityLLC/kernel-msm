/*******************************************************************
* (c) Copyright 2011-2012 Discretix Technologies Ltd.              *
* This software is protected by copyright, international           *
* treaties and patents, and distributed under multiple licenses.   *
* Any use of this Software as part of the Discretix CryptoCell or  *
* Packet Engine products requires a commercial license.            *
* Copies of this Software that are distributed with the Discretix  *
* CryptoCell or Packet Engine product drivers, may be used in      *
* accordance with a commercial license, or at the user's option,   *
* used and redistributed under the terms and conditions of the GNU *
* General Public License ("GPL") version 2, as published by the    *
* Free Software Foundation.                                        *
* This program is distributed in the hope that it will be useful,  *
* but WITHOUT ANY LIABILITY AND WARRANTY; without even the implied *
* warranty of MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. *
* See the GNU General Public License version 2 for more details.   *
* You should have received a copy of the GNU General Public        *
* License version 2 along with this Software; if not, please write *
* to the Free Software Foundation, Inc., 59 Temple Place - Suite   *
* 330, Boston, MA 02111-1307, USA.                                 *
* Any copy or reproduction of this Software, as permitted under    *
* the GNU General Public License version 2, must include this      *
* Copyright Notice as well as any other notices provided under     *
* the said license.                                                *
********************************************************************/

#ifndef _SEP_LLI_H_
#define _SEP_LLI_H_
#ifdef __KERNEL__
#include <linux/types.h>
#else
#include <stdint.h>
#endif
#include "dx_bitops.h"

#define SEP_LLI_GET(lli_p, lli_field) BITFIELD_GET(                            \
		((u32 *)(lli_p))[SEP_LLI_ ## lli_field ## _WORD_OFFSET],  \
		SEP_LLI_ ## lli_field ## _BIT_OFFSET,			       \
		SEP_LLI_ ## lli_field ## _BIT_SIZE)
#define SEP_LLI_SET(lli_p, lli_field, new_val) BITFIELD_SET(                   \
		((u32 *)(lli_p))[SEP_LLI_ ## lli_field ## _WORD_OFFSET],  \
		SEP_LLI_ ## lli_field ## _BIT_OFFSET,			       \
		SEP_LLI_ ## lli_field ## _BIT_SIZE,			       \
		new_val)

#define SEP_LLI_INIT(lli_p)  do { \
	((u32 *)(lli_p))[0] = 0; \
	((u32 *)(lli_p))[1] = 0; \
} while (0)

/* Copy local LLI scratchpad to SeP LLI buffer */
#define SEP_LLI_COPY_TO_SEP(sep_lli_p, host_lli_p) do {             \
	int i;                                                      \
	for (i = 0; i < SEP_LLI_ENTRY_WORD_SIZE; i++)               \
		((u32 *)(sep_lli_p))[i] =                      \
			cpu_to_le32(((u32 *)(host_lli_p))[i]); \
} while (0)
/* and vice-versa */
#define SEP_LLI_COPY_FROM_SEP(host_lli_p, sep_lli_p) do {                 \
	int i;                                                            \
		for (i = 0; i < SEP_LLI_ENTRY_WORD_SIZE; i++)             \
			((u32 *)(host_lli_p))[i] =                   \
				le32_to_cpu(((u32 *)(sep_lli_p))[i]);\
} while (0)

/* Size of entry */
#define SEP_LLI_ENTRY_WORD_SIZE 2
#define SEP_LLI_ENTRY_BYTE_SIZE (SEP_LLI_ENTRY_WORD_SIZE * sizeof(u32))

/* (DMA) Address: ADDR */
#define SEP_LLI_ADDR_WORD_OFFSET 0
#define SEP_LLI_ADDR_BIT_OFFSET 0
#define SEP_LLI_ADDR_BIT_SIZE 32
/* Size: SIZE */
#define SEP_LLI_SIZE_WORD_OFFSET 1
#define SEP_LLI_SIZE_BIT_OFFSET 0
#define SEP_LLI_SIZE_BIT_SIZE 30
/* First/Last LLI entries bit marks: FIRST, LAST */
#define SEP_LLI_FIRST_WORD_OFFSET 1
#define SEP_LLI_FIRST_BIT_OFFSET 30
#define SEP_LLI_FIRST_BIT_SIZE 1
#define SEP_LLI_LAST_WORD_OFFSET 1
#define SEP_LLI_LAST_BIT_OFFSET 31
#define SEP_LLI_LAST_BIT_SIZE 1

#endif /*_SEP_LLI_H_*/
