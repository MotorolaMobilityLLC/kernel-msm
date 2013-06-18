
/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#ifndef __LGIT_OIS_H
#define __LGIT_OIS_H

#define I2C_NUM_BYTE            (6) /* QCT maximum size is 6 */

#define LGIT_OIS_BINARY_VER_2_DATA1   "bu24205_LGIT_VER_2_DATA1.bin"
#define LGIT_OIS_BINARY_VER_2_DATA2   "bu24205_LGIT_VER_2_DATA2.bin"
#define LGIT_OIS_BINARY_VER_2_DATA3   "bu24205_LGIT_VER_2_DATA3.bin"

#define LGIT_OIS_BINARY_VER_3_DATA1   "bu24205_LGIT_VER_3_DATA1.bin"
#define LGIT_OIS_BINARY_VER_3_DATA2   "bu24205_LGIT_VER_3_DATA2.bin"
#define LGIT_OIS_BINARY_VER_3_DATA3   "bu24205_LGIT_VER_3_DATA3.bin"
#define LGIT_OIS_BINARY_CAL_DATA      "bu24205_LGIT_cal.bin"

#define E2P_DATA_BYTE           (28)
#define E2P_FIRST_ADDR          (0x0900)

#define BIN_SRT_ADDR_FOR_DL1    (0x0000)
#define BIN_END_ADDR_FOR_DL1    (0x0893)
#define BIN_SRT_ADDR_FOR_DL2    (0x0000)
#define BIN_END_ADDR_FOR_DL2    (0x00D3)
#define BIN_SRT_ADDR_FOR_DL3    (0x0000)
#define BIN_END_ADDR_FOR_DL3    (0x0097)

#define CTR_STR_ADDR_FOR_DL1    (0x0000)
#define CTR_END_ADDR_FOR_DL1    (0x0893)
#define CTR_STR_ADDR_FOR_DL2    (0x5400)
#define CTR_END_ADDR_FOR_DL2    (0x54D3)
#define CTR_STR_ADDR_FOR_DL3    (0x1188)
#define CTR_END_ADDR_FOR_DL3    (0x121F)

#define E2P_STR_ADDR_FOR_E2P_DL (0)
#define E2P_END_ADDR_FOR_E2P_DL (27)
#define CTL_END_ADDR_FOR_E2P_DL (0x13A8)

#define OIS_START_DL_ADDR       (0xF010)
#define OIS_COMPLETE_DL_ADDR    (0xF006)
#define OIS_READ_STATUS_ADDR    (0x6024)

#define BIN_SUM_VALUE_2         (0x0002F737)
#define BIN_SUM_VALUE_3         (0x0002F836)
#define LIMIT_STATUS_POLLING    (10)
#define LIMIT_OIS_ON_RETRY      (5)

#endif
