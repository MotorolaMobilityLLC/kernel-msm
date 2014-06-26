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
/** @file: dx_dev_defs.h
 * Device specific definitions for the CC device driver */

#ifndef _DX_DEV_DEFS_H_
#define _DX_DEV_DEFS_H_

#define DRIVER_NAME MODULE_NAME

/* OpenFirmware matches */
#define DX_DEV_OF_MATCH  {                        \
	{.name = "dx_cc54" },                     \
	{.compatible = "xlnx,plbv46-cc-1.00.c",}, \
	{}                                        \
}

/* Firmware images file names (for request_firmware) */
#define RESIDENT_IMAGE_NAME DRIVER_NAME "-resident.bin"
#define CACHE_IMAGE_NAME DRIVER_NAME "-cache.bin"
#define VRL_IMAGE_NAME DRIVER_NAME "-Primary_VRL.bin"

/* OTP index of verification hash for key in VRL */
#define VRL_KEY_INDEX 0

#define ICACHE_SIZE_LOG2_DEFAULT 20	/* 1MB */
#define DCACHE_SIZE_LOG2_DEFAULT 20	/* 1MB */

#define EXPECTED_FW_VER 0x01000000

/* The known SeP clock frequency in MHz (30 MHz on Virtex-5 FPGA) */
/* Comment this line if SeP frequency is already initialized in CC_INIT ext. */
/*#define SEP_FREQ_MHZ 30*/

/* Number of SEP descriptor queues */
#define SEP_MAX_NUM_OF_DESC_Q  2

/* Maximum number of registered memory buffers per user context */
#define MAX_REG_MEMREF_PER_CLIENT_CTX 16

/* Maximum number of SeP Applets session per client context */
#define MAX_SEPAPP_SESSION_PER_CLIENT_CTX 16

#endif				/* _DX_DEV_DEFS_H_ */
