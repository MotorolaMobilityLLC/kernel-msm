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


/* This file contains the definitions of the OTP data that the SEP copies
into the SRAM during the first boot process */


#ifndef _SEP_SRAM_MAP_
#define _SEP_SRAM_MAP_

#define DX_FIRST_OEM_KEY_OFFSET_IN_SRAM         0x0
#define DX_SECOND_OEM_KEY_OFFSET_IN_SRAM        0x4
#define DX_THIRD_OEM_KEY_OFFSET_IN_SRAM         0x8
#define DX_LCS_OFFSET_IN_SRAM                   0xC
#define DX_MISC_OFFSET_IN_SRAM                  0xD
#define DX_CC_INIT_MSG_OFFSET_IN_SRAM		0x100
#define DX_PKA_MEMORY_OFFSET_IN_SRAM		0x200

#endif /*_GEN_SRAM_MAP_*/
