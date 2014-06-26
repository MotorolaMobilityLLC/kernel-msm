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

#ifndef SEP_INIT_CC_ERROR_H
#define SEP_INIT_CC_ERROR_H

#include "sep_error.h"

	/*! \file lcs.h
	 * \brief This file containes lcs definitions
	 */
#define DX_INIT_CC_OK DX_SEP_OK
/* DX_INIT_CC_MODULE_ERROR_BASE - 0xE0004000 */
#define DX_CC_INIT_MSG_CS_ERROR	\
	(DX_INIT_CC_MODULE_ERROR_BASE + 0x1)
#define DX_CC_INIT_MSG_WRONG_TOKEN_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0x2)
#define DX_CC_INIT_MSG_WRONG_OP_CODE_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0x3)
#define DX_CC_INIT_MSG_WRONG_RESIDENT_ADDR_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0x4)
#define DX_CC_INIT_MSG_WRONG_I_CACHE_ADDR_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0x5)
#define DX_CC_INIT_MSG_WRONG_I_CACHE_DEST_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0x6)
#define DX_CC_INIT_MSG_WRONG_D_CACHE_ADDR_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0x7)
#define DX_CC_INIT_MSG_WRONG_D_CACHE_SIZE_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0x8)
#define DX_CC_INIT_MSG_WRONG_INIT_EXT_ADDR_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0x9)
#define DX_CC_INIT_MSG_WRONG_VRL_ADDR_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0xA)
#define DX_CC_INIT_MSG_WRONG_MAGIC_NUM_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0xB)
#define DX_CC_INIT_MSG_WRONG_OUTPUT_BUFF_ADDR_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0xC)
#define DX_CC_INIT_MSG_WRONG_OUTPUT_BUFF_SIZE_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0xD)
#define DX_RESERVED_0_ERROR \
	(DX_INIT_CC_MODULE_ERROR_BASE + 0xE)

/* DX_INIT_CC_EXT_MODULE_ERROR_BASE - 0xE0005000 */
#define DX_CC_INIT_EXT_FIRST_PARAM_ERRR \
	(DX_INIT_CC_EXT_MODULE_ERROR_BASE + 0x1)
#define DX_CC_INIT_EXT_WRONG_LAST_PARAM_LENGTH_ERRR \
	(DX_INIT_CC_EXT_MODULE_ERROR_BASE + 0x2)
#define DX_CC_INIT_EXT_WRONG_CHECKSUM_VALUE_ERRR \
	(DX_INIT_CC_EXT_MODULE_ERROR_BASE + 0x3)
#define DX_CC_INIT_EXT_WRONG_DISABLE_MODULE_LENGTH_ERRR \
	(DX_INIT_CC_EXT_MODULE_ERROR_BASE + 0x4)
#define DX_CC_INIT_EXT_WRONG_AXI_CONFIG_LENGTH_ERRR \
	(DX_INIT_CC_EXT_MODULE_ERROR_BASE + 0x5)
#define DX_CC_INIT_EXT_WRONG_PARAM_ERRR \
	(DX_INIT_CC_EXT_MODULE_ERROR_BASE + 0x6)
#define DX_CC_INIT_EXT_EXCEED_MAX_PARAM_PARAM_ERRR \
	(DX_INIT_CC_EXT_MODULE_ERROR_BASE + 0x7)
#define DX_CC_INIT_EXT_WRONG_SEP_FREQ_LENGTH_ERRR \
	(DX_INIT_CC_EXT_MODULE_ERROR_BASE + 0x8)

#endif
