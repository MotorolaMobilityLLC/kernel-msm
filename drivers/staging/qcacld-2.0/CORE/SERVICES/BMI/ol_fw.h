/*
 * Copyright (c) 2013-2014,2016 The Linux Foundation. All rights reserved.
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

#ifndef _OL_FW_H_
#define _OL_FW_H_

#ifdef QCA_WIFI_FTM
#include "vos_types.h"
#endif

#define AR6004_VERSION_REV1_3        0x31c8088a

#define AR9888_REV1_VERSION          0x4000002c
#define AR9888_REV2_VERSION          0x4100016c
#define QCA_VERSION                  0x4100270f
#define AR6320_REV1_VERSION          0x5000000
#define AR6320_REV1_1_VERSION        0x5000001
#define AR6320_REV1_VERSION_1        AR6320_REV1_1_VERSION
#define AR6320_REV1_3_VERSION        0x5000003
#define AR6320_REV2_VERSION          AR6320_REV1_1_VERSION
#define AR6320_REV2_1_VERSION        0x5010000
#define AR6320_REV3_VERSION          0x5020000
#define QCA9377_REV1_1_VERSION       0x5020001
#define AR6320_REV3_2_VERSION        0x5030000
#define AR6320_REV4_VERSION          AR6320_REV2_1_VERSION
#define QCA9379_REV1_VERSION         0x5040000
#define AR6320_DEV_VERSION           0x1000000
#define QCA_FIRMWARE_FILE            "athwlan.bin"
#define QCA_UTF_FIRMWARE_FILE        "utf.bin"
#define QCA_BOARD_DATA_FILE          "fakeboar.bin"
#define QCA_OTP_FILE                 "otp.bin"
#define QCA_SETUP_FILE               "athsetup.bin"
#define AR61X4_SINGLE_FILE           "qca61x4.bin"
#define QCA_FIRMWARE_EPPING_FILE     "epping.bin"

/* Configuration for statistics pushed by firmware */
#define PDEV_DEFAULT_STATS_UPDATE_PERIOD    500
#define VDEV_DEFAULT_STATS_UPDATE_PERIOD    500
#define PEER_DEFAULT_STATS_UPDATE_PERIOD    500

/*
 * Note that not all the register locations are accessible.
 * A list of accessible target registers are specified with
 * their start and end addresses in a table for given target
 * version. We should NOT access other locations as either
 * they are invalid locations or host does not have read
 * access to it or the value of the particular register
 * read might change
 */
#define REGISTER_LOCATION       0x00000800

#ifdef TARGET_DUMP_FOR_NON_QC_PLATFORM
#define DRAM_LOCATION           0x00400000
#define DRAM_SIZE               0x00097FFC

#define IRAM_LOCATION           0x00980000
#define IRAM_SIZE               0x000BFFFC

#define AXI_LOCATION            0x000a0000
#define AXI_SIZE                0x0001FFFC
#else
#define DRAM_LOCATION           0x00400000
#define DRAM_SIZE               0x000a8000
#define DRAM_LOCAL_BASE_ADDRESS (0x100000)

#ifdef HIF_PCI
#define IRAM1_LOCATION          0x00980000
#define IRAM1_SIZE              0x00080000
#define IRAM2_LOCATION          0x00a00000
#define IRAM2_SIZE              0x00040000
#else
#define IRAM_LOCATION           0x00980000
#define IRAM_SIZE               0x00038000
#endif

#define AXI_LOCATION            0x000a0000
#ifdef HIF_PCI
#define AXI_SIZE                0x00018000
#else
#define AXI_SIZE                0x00020000
#endif /* #ifdef HIF_PCIE */
#endif

#define CE_OFFSET               0x00000400
#define CE_USEFUL_SIZE          0x00000058

#define TOTAL_DUMP_SIZE         0x00200000
#define PCIE_READ_LIMIT         0x00005000

#define SHA256_DIGEST_SIZE      32

struct hash_fw {
	u8 qwlan[SHA256_DIGEST_SIZE];
	u8 otp[SHA256_DIGEST_SIZE];
	u8 bdwlan[SHA256_DIGEST_SIZE];
	u8 utf[SHA256_DIGEST_SIZE];
};

int ol_target_coredump(void *instance, void* memoryBlock,
                        u_int32_t blockLength);
int ol_diag_read(struct ol_softc *scn, u_int8_t* buffer,
                 u_int32_t pos, size_t count);
void ol_schedule_ramdump_work(struct ol_softc *scn);
void ol_schedule_fw_indication_work(struct ol_softc *scn);
int ol_copy_ramdump(struct ol_softc *scn);
int dump_CE_register(struct ol_softc *scn);
int ol_download_firmware(struct ol_softc *scn);
int ol_configure_target(struct ol_softc *scn);
void ol_target_failure(void *instance, A_STATUS status);
u_int8_t ol_get_number_of_peers_supported(struct ol_softc *scn);

#ifdef REMOVE_PKT_LOG
static inline void ol_pktlog_init(void *)
{
}
#else
void ol_pktlog_init(void *);
#endif

#if defined(HIF_SDIO)
void ol_target_ready(struct ol_softc *scn, void *cfg_ctx);
#else
static inline void ol_target_ready(struct ol_softc *scn, void *cfg_ctx)
{

}
#endif

#endif /* _OL_FW_H_ */
