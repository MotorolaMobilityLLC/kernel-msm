/*
 * Copyright (c) 2013-2014 The Linux Foundation. All rights reserved.
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

/*
 * fw_one_bin.h
 */

#ifndef FW_ONE_BIN_H_
#define FW_ONE_BIN_H_

/* Firmware Single Binary Magic Number */
#define ONE_BIN_MAGIC_NUM     ('Q' | 'C' << 8 | 'A' << 16 | 'W' << 24)

/* Format version */
#define ONE_BIN_FORMAT_MAJOR  0x1
#define ONE_BIN_FORMAT_MINOR  0
#define ONE_BIN_FORMAT_VER    (ONE_BIN_FORMAT_MAJOR << 16 | ONE_BIN_FORMAT_MINOR)

/* CHIP ID */
#define AR6320_1_0_CHIP_ID      0x5000000
#define AR6320_1_1_CHIP_ID      0x5000001
#define AR6320_1_3_CHIP_ID      0x5000003
#define AR6320_2_0_CHIP_ID      0x5010000

/* WLAN BINARY_ID */
#define WLAN_SETUP_BIN_ID     0x01
#define WLAN_OTP_BIN_ID       0x02
#define WLAN_ATHWLAN_BIN_ID   0x0F

/* UTF BINARY_ID */
#define UTF_SETUP_BIN_ID      0x10
#define UTF_OTP_BIN_ID        0x20
#define UTF_UTF_BIN_ID        0xF0

/* BINARY_GROUP_ID */
#define WLAN_GROUP_ID         0x80000000
#define UTF_GROUP_ID          0x40000000

/* ACTION */
#define ACTION_PARSE_SIG      0x80000000   /* parse signature		  */
#define ACTION_DOWNLOAD       0x1	   /* download only		  */
#define ACTION_DOWNLOAD_EXEC  0x3	   /* download binary and execute */

/* Binary Meta Header */
typedef struct {
       unsigned int magic_num;       /* = ONE_BIN_MAGIC_NUM                  */
       unsigned int total_len;       /* single binary file length            */
       unsigned int format_ver;      /* single binary format version         */
       unsigned int fst_tag_off;     /* offset of the first binary's CHIP_ID */
} FW_ONE_BIN_META_T;

/* Binary description header */
typedef struct {
       unsigned int chip_id;        /* CHIP_ID                               */
       unsigned int binary_id;      /* BINARY_ID                             */
       unsigned int bin_group_id;   /* BINARY_GROUP_ID                       */
       unsigned int next_tag_off;   /* Offset of the next binary's CHIP_ID   */
       unsigned int binary_off;     /* Offset of binary                      */
       unsigned int binary_len;     /* Length of binary                      */
       unsigned int action;         /* Action of binary operation            */
       unsigned int reserved;       /* 0, reserved                           */
} FW_BIN_HEADER_T;

#endif /* FW_ONE_BIN_H_ */
