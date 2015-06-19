/*
 * Copyright (c) "2012,2014" The Linux Foundation. All rights reserved.
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
 *
 * $ATH_LICENSE_HOSTSDK0_C$
 */

#include <sys/types.h>
#include <sys/file.h>
#include <sys/ioctl.h>
#include <sys/socket.h>
#include <linux/types.h>
#include <linux/if.h>
#include <linux/wireless.h>
#include <linux/version.h>

#include <stdio.h>
#include <stdlib.h>
#include <string.h>
#include <getopt.h>
#include <sys/types.h>
#include <sys/stat.h>
#include <fcntl.h>
#include <unistd.h>
#include <errno.h>
#include <sys/time.h>

#include <a_osapi.h>
#include <athdefs.h>
#include <a_types.h>

#include "apb_athr_wlan_map.h"
#include "rtc_soc_reg.h"
#include "efuse_reg.h"

#ifndef PATH_MAX
#define PATH_MAX 1024
#endif

/*
 * This is a user-level agent which provides diagnostic read/write
 * access to Target space.  This may be used
 *   to collect information for analysis
 *   to read/write Target registers
 *   etc.
 */

#define DIAG_READ_TARGET      1
#define DIAG_WRITE_TARGET     2
#define DIAG_READ_WORD        3
#define DIAG_WRITE_WORD       4
#define DIAG_DUMP_TARGET      5


#define ADDRESS_FLAG                    0x0001
#define LENGTH_FLAG                     0x0002
#define PARAM_FLAG                      0x0004
#define FILE_FLAG                       0x0008
#define UNUSED0x010                     0x0010
#define AND_OP_FLAG                     0x0020
#define BITWISE_OP_FLAG                 0x0040
#define QUIET_FLAG                      0x0080
#define OTP_FLAG                        0x0100
#define HEX_FLAG                        0x0200
/* dump file mode,x: hex mode; other binary mode. */
#define UNUSED0x400                     0x0400
#define DEVICE_FLAG                     0x0800
#define TARGET_FLAG                     0x1000
#define PATH_FLAG                       0x2000

/* Limit malloc size when reading/writing file */
#define MAX_BUF                         (8*1024)

#define DUMP_DRAM_START_ADDR 0x400000
#define DUMP_DRAM_LEN        0x50000

#define PEREGRINE_REG_PART1_START_ADDR 0x4000
#define PEREGRINE_REG_PART1_LEN        0x2000
#define PEREGRINE_REG_PART2_START_ADDR 0x8000
#define PEREGRINE_REG_PART2_LEN        0x58000

#define AR6320V1_REG_PART1_START_ADDR 0x0                 /*RTC_SOC_BASE_ADDRESS*/
#define AR6320V1_REG_PART1_LEN        (0x800 - 0x0)       /*WLAN_MBOX_BASE_ADDRESS - RTC_SOC_BASE_ADDRESS*/
#define AR6320V1_REG_PART2_START_ADDR 0x27000             /*STEREO_BASE_ADDRESS*/
#define AR6320V1_REG_PART2_LEN        (0x60000 - 0x27000) /*USB_BASE_ADDRESS - STEREO_BASE_ADDRESS*/

/* For Rome version 2.x */

#define AR6320V2_DRAM_START_ADDR 0x400000   // dram start
#define AR6320V2_DUMP_DRAM_LEN   0x70000    // dram length
#define AR6320V2_IRAM_START_ADDR 0x980000   // iram start
#define AR6320V2_IRAM_LEN        0x38000    // iram length
#define AR6320V2_AXI_START_ADDR  0xa0000    // axi start
#define AR6320V2_AXI_LEN         0x18000    // axi length

/* For Rome version 3.x */

#define AR6320V3_DRAM_START_ADDR 0x400000 // dram start
#define AR6320V3_DUMP_DRAM_LEN   0xa8000  // dram length
#define AR6320V3_IRAM_START_ADDR 0x980000 // iram start
#define AR6320V3_IRAM_LEN        0x38000  // iram length
#define AR6320V3_AXI_START_ADDR  0xa0000  // axi start
#define AR6320V3_AXI_LEN         0x18000  // axi length

struct ath_target_reg_info {
    A_UINT32 reg_start;
    A_UINT32 reg_len;
    const char *reg_info;
    const char *save_file;
};

static const struct ath_target_reg_info reg_ar9888_v2[] = {
    {DUMP_DRAM_START_ADDR,           DUMP_DRAM_LEN,           "DRAM",      "fwdump_prgr_v2_dram"},
    {PEREGRINE_REG_PART1_START_ADDR, PEREGRINE_REG_PART1_LEN, "REG_PART1", "fwdump_prgr_v2_reg1"},
    {PEREGRINE_REG_PART2_START_ADDR, PEREGRINE_REG_PART2_LEN, "REG_PART2", "fwdump_prgr_v2_reg2"},
    {0, 0, 0, 0}
};

static const struct ath_target_reg_info reg_ar6320_v1[] = {
    {DUMP_DRAM_START_ADDR,          DUMP_DRAM_LEN,          "DRAM",      "fwdump_rome_v1_dram"},
    {AR6320V1_REG_PART1_START_ADDR, AR6320V1_REG_PART1_LEN, "REG_PART1", "fwdump_rome_v1_reg1"},
    {AR6320V1_REG_PART2_START_ADDR, AR6320V1_REG_PART2_LEN, "REG_PART2", "fwdump_rome_v1_reg2"},
    {0, 0, 0, 0}
};

static const struct ath_target_reg_info reg_ar6320_v2[] = {
    {AR6320V2_DRAM_START_ADDR,      AR6320V2_DUMP_DRAM_LEN, "DRAM",      "fwdump_rome_v2_dram"},
    {AR6320V2_IRAM_START_ADDR,      AR6320V2_IRAM_LEN,      "IRAM",      "fwdump_rome_v2_iram"},
    {AR6320V2_AXI_START_ADDR,       AR6320V2_AXI_LEN,       "AXI",       "fwdump_rome_v2_axi" },
    {0, 0, 0, 0}
};

static const struct ath_target_reg_info reg_ar6320_v3[] = {
    {AR6320V3_DRAM_START_ADDR,      AR6320V3_DUMP_DRAM_LEN, "DRAM",      "fwdump_rome_v3_dram"},
    {AR6320V3_IRAM_START_ADDR,      AR6320V3_IRAM_LEN,      "IRAM",      "fwdump_rome_v3_iram"},
    {AR6320V3_AXI_START_ADDR,       AR6320V3_AXI_LEN,       "AXI" ,      "fwdump_rome_v3_axi" },
    {0x800,    0x10,   "REG",    "reg800"},
    {0x820,    0xC,    "REG",    "reg820"},
    {0x830,    0xC4,   "REG",    "reg830"},
    {0x90C,    0x10,   "REG",    "reg90C"},
    {0xA14,    0x4,    "REG",    "regA14"},
    {0xA84,    0x10,   "REG",    "regA84"},
    {0xAA8,    0x2C,   "REG",    "regAA8"},
    {0xADC,    0x64,   "REG",    "regADC"},
    {0x1000,   0xA4,   "REG",   "reg1000"},
    {0x10BC,   0x60,   "REG",   "reg10BC"},
    {0x1134,   0x4,    "REG",   "reg1134"},
    {0x1144,   0x8,    "REG",   "reg1144"},
    {0x1150,   0xC,    "REG",   "reg1150"},
    {0x1160,   0x18,   "REG",   "reg1160"},
    {0x1240,   0x20,   "REG",   "reg1240"},
    {0x2000,   0x7C,   "REG",   "reg2000"},
    {0x3000,   0x14,   "REG",   "reg3000"},
    {0x4000,   0x14,   "REG",   "reg4000"},
    {0x5000,   0x124,  "REG",   "reg5000"},
    {0x6000,   0x40,   "REG",   "reg6000"},
    {0x6080,   0x4C,   "REG",   "reg6080"},
    {0x6100,   0x1C,   "REG",   "reg6100"},
    {0x6140,   0x98,   "REG",   "reg6140"},
    {0x6200,   0x38,   "REG",   "reg6200"},
    {0x6240,   0x4C,   "REG",   "reg6240"},
    {0x62C0,   0x2C,   "REG",   "reg62C0"},
    {0x6380,   0x68,   "REG",   "reg6380"},
    {0x6400,   0x40,   "REG",   "reg6400"},
    {0x6480,   0x4C,   "REG",   "reg6480"},
    {0x6500,   0x1C,   "REG",   "reg6500"},
    {0x6540,   0x40,   "REG",   "reg6540"},
    {0x6600,   0x38,   "REG",   "reg6600"},
    {0x6640,   0x4C,   "REG",   "reg6640"},
    {0x66C0,   0x2C,   "REG",   "reg66C0"},
    {0x6780,   0x68,   "REG",   "reg6780"},
    {0x7080,   0xC,    "REG",   "reg7080"},
    {0x70C0,   0x8,    "REG",   "reg70C0"},
    {0x7400,   0x1C,   "REG",   "reg7400"},
    {0x7440,   0x14,   "REG",   "reg7440"},
    {0x7800,   0x18,   "REG",   "reg7800"},
    {0x8000,   0x4,    "REG",   "reg8000"},
    {0x8010,   0x54,   "REG",   "reg8010"},
    {0x8080,   0x4,    "REG",   "reg8080"},
    {0x80A0,   0x4,    "REG",   "reg80A0"},
    {0x80C0,   0x4,    "REG",   "reg80C0"},
    {0x80E0,   0x14,   "REG",   "reg80E0"},
    {0x8100,   0x4,    "REG",   "reg8100"},
    {0x8110,   0x1C,   "REG",   "reg8110"},
    {0x9000,   0x4,    "REG",   "reg9000"},
    {0x9800,   0x2C,   "REG",   "reg9800"},
    {0x9830,   0x8,    "REG",   "reg9830"},
    {0x9840,   0x2C,   "REG",   "reg9840"},
    {0x9870,   0x28,   "REG",   "reg9870"},
    {0x9A00,   0x200,  "REG",   "reg9A00"},
    {0xD580,   0x1C,   "REG",   "regD580"},
    {0xF000,   0xE0,   "REG",   "regF000"},
    {0xF140,   0x50,   "REG",   "regF140"},
    {0xF250,   0xC,    "REG",   "regF250"},
    {0xF260,   0x8,    "REG",   "regF260"},
    {0xF26C,   0x3C,   "REG",   "regF26C"},
    {0x10008,  0x4,    "REG",  "reg10008"},
    {0x10014,  0x4,    "REG",  "reg10014"},
    {0x1001C,  0x4,    "REG",  "reg1001C"},
    {0x10024,  0x4,    "REG",  "reg10024"},
    {0x10030,  0x4,    "REG",  "reg10030"},
    {0x10040,  0x14,   "REG",  "reg10040"},
    {0x10058,  0x24,   "REG",  "reg10058"},
    {0x10080,  0x44,   "REG",  "reg10080"},
    {0x100C8,  0x4C,   "REG",  "reg100C8"},
    {0x1012C,  0x4,    "REG",  "reg1012C"},
    {0x10138,  0xC,    "REG",  "reg10138"},
    {0x10200,  0x20,   "REG",  "reg10200"},
    {0x10230,  0x20,   "REG",  "reg10230"},
    {0x10260,  0x20,   "REG",  "reg10260"},
    {0x10290,  0x20,   "REG",  "reg10290"},
    {0x102C0,  0x1C,   "REG",  "reg102C0"},
    {0x102E0,  0x14,   "REG",  "reg102E0"},
    {0x102FC,  0x80,   "REG",  "reg102FC"},
    {0x10380,  0x10,   "REG",  "reg10380"},
    {0x10800,  0x28,   "REG",  "reg10800"},
    {0x10840,  0x4,    "REG",  "reg10840"},
    {0x10880,  0x4,    "REG",  "reg10880"},
    {0x108C0,  0x28,   "REG",  "reg108C0"},
    {0x10900,  0x28,   "REG",  "reg10900"},
    {0x10940,  0x4,    "REG",  "reg10940"},
    {0x10980,  0x4,    "REG",  "reg10980"},
    {0x109C0,  0x28,   "REG",  "reg109C0"},
    {0x10A00,  0x28,   "REG",  "reg10A00"},
    {0x10A40,  0x10,   "REG",  "reg10A40"},
    {0x11000,  0x28,   "REG",  "reg11000"},
    {0x11030,  0x4,    "REG",  "reg11030"},
    {0x11038,  0x30,   "REG",  "reg11038"},
    {0x11070,  0x4,    "REG",  "reg11070"},
    {0x11078,  0x30,   "REG",  "reg11078"},
    {0x110B0,  0x4,    "REG",  "reg110B0"},
    {0x110B8,  0x30,   "REG",  "reg110B8"},
    {0x110F0,  0x4,    "REG",  "reg110F0"},
    {0x110F8,  0x30,   "REG",  "reg110F8"},
    {0x11138,  0xC,    "REG",  "reg11138"},
    {0x11178,  0x8,    "REG",  "reg11178"},
    {0x111B8,  0x8,    "REG",  "reg111B8"},
    {0x111F8,  0x8,    "REG",  "reg111F8"},
    {0x11238,  0x4,    "REG",  "reg11238"},
    {0x11270,  0x4,    "REG",  "reg11270"},
    {0x11278,  0x4,    "REG",  "reg11278"},
    {0x112B0,  0x4,    "REG",  "reg112B0"},
    {0x112B8,  0x4,    "REG",  "reg112B8"},
    {0x112F0,  0x4,    "REG",  "reg112F0"},
    {0x112F8,  0x4,    "REG",  "reg112F8"},
    {0x11338,  0x4,    "REG",  "reg11338"},
    {0x11378,  0x4,    "REG",  "reg11378"},
    {0x113B8,  0x4,    "REG",  "reg113B8"},
    {0x113F8,  0x4,    "REG",  "reg113F8"},
    {0x11438,  0x8,    "REG",  "reg11438"},
    {0x11478,  0x8,    "REG",  "reg11478"},
    {0x114B8,  0x4,    "REG",  "reg114B8"},
    {0x114F8,  0x4,    "REG",  "reg114F8"},
    {0x11538,  0x4,    "REG",  "reg11538"},
    {0x11578,  0x4,    "REG",  "reg11578"},
    {0x115B8,  0x4,    "REG",  "reg115B8"},
    {0x115F8,  0x4,    "REG",  "reg115F8"},
    {0x11638,  0x4,    "REG",  "reg11638"},
    {0x11678,  0x4,    "REG",  "reg11678"},
    {0x116B8,  0x4,    "REG",  "reg116B8"},
    {0x116F8,  0x4,    "REG",  "reg116F8"},
    {0x11738,  0x4,    "REG",  "reg11738"},
    {0x11778,  0x4,    "REG",  "reg11778"},
    {0x117B8,  0x4,    "REG",  "reg117B8"},
    {0x117F8,  0x4,    "REG",  "reg117F8"},
    {0x17000,  0x1C,   "REG",  "reg17000"},
    {0x17020,  0x8C,   "REG",  "reg17020"},
    {0x18000,  0x50,   "REG",  "reg18000"},
    {0x18054,  0x20,   "REG",  "reg18054"},
    {0x18080,  0x54,   "REG",  "reg18080"},
    {0x180DC,  0x28,   "REG",  "reg180DC"},
    {0x18108,  0x34,   "REG",  "reg18108"},
    {0x18144,  0x4,    "REG",  "reg18144"},
    {0x18168,  0xC,    "REG",  "reg18168"},
    {0x18178,  0x8,    "REG",  "reg18178"},
    {0x181C8,  0x18,   "REG",  "reg181C8"},
    {0x181E4,  0x4,    "REG",  "reg181E4"},
    {0x181EC,  0x20,   "REG",  "reg181EC"},
    {0x1825C,  0x24,   "REG",  "reg1825C"},
    {0x18284,  0xC,    "REG",  "reg18284"},
    {0x18294,  0xC,    "REG",  "reg18294"},
    {0x18300,  0x4,    "REG",  "reg18300"},
    {0x18314,  0xC,    "REG",  "reg18314"},
    {0x18328,  0x28,   "REG",  "reg18328"},
    {0x1835C,  0x10,   "REG",  "reg1835C"},
    {0x18370,  0x20,   "REG",  "reg18370"},
    {0x18398,  0x14,   "REG",  "reg18398"},
    {0x183BC,  0x1C,   "REG",  "reg183BC"},
    {0x183DC,  0x18,   "REG",  "reg183DC"},
    {0x18400,  0x2F4,  "REG",  "reg18400"},
    {0x186F8,  0x24,   "REG",  "reg186F8"},
    {0x18720,  0x70,   "REG",  "reg18720"},
    {0x19800,  0x30,   "REG",  "reg19800"},
    {0x19834,  0xC,    "REG",  "reg19834"},
    {0x19880,  0x1C,   "REG",  "reg19880"},
    {0x198A4,  0xC,    "REG",  "reg198A4"},
    {0x198BC,  0x44,   "REG",  "reg198BC"},
    {0x19C00,  0x88,   "REG",  "reg19C00"},
    {0x19D00,  0x20,   "REG",  "reg19D00"},
    {0x19E00,  0x7C,   "REG",  "reg19E00"},
    {0x19E80,  0x14,   "REG",  "reg19E80"},
    {0x19E98,  0x14,   "REG",  "reg19E98"},
    {0x19EB0,  0xC,    "REG",  "reg19EB0"},
    {0x19F70,  0x4,    "REG",  "reg19F70"},
    {0x19F80,  0xC,    "REG",  "reg19F80"},
    {0x19FA0,  0x14,   "REG",  "reg19FA0"},
    {0x19FC0,  0x18,   "REG",  "reg19FC0"},
    {0x1A000,  0x200,  "REG",  "reg1A000"},
    {0x1A204,  0xC,    "REG",  "reg1A204"},
    {0x1A228,  0x4,    "REG",  "reg1A228"},
    {0x1A230,  0x18,   "REG",  "reg1A230"},
    {0x1A250,  0x20,   "REG",  "reg1A250"},
    {0x1A280,  0x10,   "REG",  "reg1A280"},
    {0x1A2A0,  0x4,    "REG",  "reg1A2A0"},
    {0x1A2C0,  0x2C,   "REG",  "reg1A2C0"},
    {0x1A300,  0xBC,   "REG",  "reg1A300"},
    {0x1A3F0,  0x4,    "REG",  "reg1A3F0"},
    {0x1A3F8,  0x3C,   "REG",  "reg1A3F8"},
    {0x1A438,  0xC,    "REG",  "reg1A438"},
    {0x1A448,  0x20,   "REG",  "reg1A448"},
    {0x1A580,  0xC,    "REG",  "reg1A580"},
    {0x1A644,  0x10,   "REG",  "reg1A644"},
    {0x1A670,  0x28,   "REG",  "reg1A670"},
    {0x1A6AC,  0x4,    "REG",  "reg1A6AC"},
    {0x1A6D0,  0x4,    "REG",  "reg1A6D0"},
    {0x1A6EC,  0x20,   "REG",  "reg1A6EC"},
    {0x1A710,  0x28,   "REG",  "reg1A710"},
    {0x1A7C0,  0x10,   "REG",  "reg1A7C0"},
    {0x1A7D4,  0x4,    "REG",  "reg1A7D4"},
    {0x1A7DC,  0x8,    "REG",  "reg1A7DC"},
    {0x1A7F0,  0x8,    "REG",  "reg1A7F0"},
    {0x1A888,  0x14,   "REG",  "reg1A888"},
    {0x1A8A8,  0x4,    "REG",  "reg1A8A8"},
    {0x1A8C0,  0x1C,   "REG",  "reg1A8C0"},
    {0x1A8F0,  0xC,    "REG",  "reg1A8F0"},
    {0x1AE04,  0x4,    "REG",  "reg1AE04"},
    {0x1AE18,  0xC,    "REG",  "reg1AE18"},
    {0x1AF80,  0xC,    "REG",  "reg1AF80"},
    {0x1AFA0,  0x14,   "REG",  "reg1AFA0"},
    {0x1B000,  0x200,  "REG",  "reg1B000"},
    {0x1B284,  0x4,    "REG",  "reg1B284"},
    {0x1B2D0,  0x8,    "REG",  "reg1B2D0"},
    {0x1B2DC,  0x10,   "REG",  "reg1B2DC"},
    {0x1B300,  0x40,   "REG",  "reg1B300"},
    {0x1B374,  0x4,    "REG",  "reg1B374"},
    {0x1B380,  0x4,    "REG",  "reg1B380"},
    {0x1B388,  0x4,    "REG",  "reg1B388"},
    {0x1B404,  0x4,    "REG",  "reg1B404"},
    {0x1B420,  0x8,    "REG",  "reg1B420"},
    {0x1B440,  0x4,    "REG",  "reg1B440"},
    {0x1B448,  0x4,    "REG",  "reg1B448"},
    {0x1B450,  0x8,    "REG",  "reg1B450"},
    {0x1B45C,  0xC,    "REG",  "reg1B45C"},
    {0x1B584,  0x8,    "REG",  "reg1B584"},
    {0x1B68C,  0x4,    "REG",  "reg1B68C"},
    {0x1B6AC,  0x4,    "REG",  "reg1B6AC"},
    {0x1B7F0,  0x8,    "REG",  "reg1B7F0"},
    {0x1C800,  0x400,  "REG",  "reg1C800"},
    {0x1CE00,  0x4,    "REG",  "reg1CE00"},
    {0x1CF80,  0x4,    "REG",  "reg1CF80"},
    {0x1D200,  0x600,  "REG",  "reg1D200"},
    {0x1E000,  0x2014, "REG",  "reg1E000"},
    {0x20100,  0x24,   "REG",  "reg20100"},
    {0x21400,  0x3A8,  "REG",  "reg21400"},
    {0x21800,  0x3A8,  "REG",  "reg21800"},
    {0x21C00,  0x3A8,  "REG",  "reg21C00"},
    {0x22000,  0x3A8,  "REG",  "reg22000"},
    {0x22400,  0x3A8,  "REG",  "reg22400"},
    {0x22800,  0x3A8,  "REG",  "reg22800"},
    {0x22C00,  0x3A8,  "REG",  "reg22C00"},
    {0x23000,  0x3A8,  "REG",  "reg23000"},
    {0x24000,  0x34,   "REG",  "reg24000"},
    {0x26000,  0x64,   "REG",  "reg26000"},
    {0x27000,  0x24,   "REG",  "reg27000"},
    {0x34000,  0xC,    "REG",  "reg34000"},
    {0x34400,  0x5C,   "REG",  "reg34400"},
    {0x34800,  0x5C,   "REG",  "reg34800"},
    {0x34C00,  0x5C,   "REG",  "reg34C00"},
    {0x35000,  0x5C,   "REG",  "reg35000"},
    {0x35400,  0x5C,   "REG",  "reg35400"},
    {0x35800,  0x5C,   "REG",  "reg35800"},
    {0x35C00,  0x5C,   "REG",  "reg35C00"},
    {0x36000,  0x5C,   "REG",  "reg36000"},
    {0x38000,  0x64,   "REG",  "reg38000"},
    {0x38070,  0x70,   "REG",  "reg38070"},
    {0x3A000,  0x74,   "REG",  "reg3A000"},
    {0x40000,  0xA4,   "REG",  "reg40000"},
    {0x80000,  0xC,    "REG",  "reg80000"},
    {0x80010,  0x10,   "REG",  "reg80010"},
    {0, 0, 0, 0}
};

#define INVALID_TARGET_INDEX    0xffff
#define MIN_TARGET_INDEX        0
#define MAX_TARGET_INDEX        4

struct ath_target_info {
    const char *name;
    const struct ath_target_reg_info *reg_info;
};

static const struct ath_target_info target_info[] = {
    {"AR9888_v2", reg_ar9888_v2},
    {"AR6320_v1", reg_ar6320_v1},
    {"AR6320_v2", reg_ar6320_v2},
    {"AR6320_v3", reg_ar6320_v3},
};



unsigned int flag;
const char *progname;
const char commands[] =
"commands and options:\n\
--get --address=<target word address>\n\
--set --address=<target word address> --[value|param]=<value>\n\
                                      --or=<OR-ing value>\n\
                                      --and=<AND-ing value>\n\
--read --address=<target address> --length=<bytes> --file=<filename>\n\
--write --address=<target address> --file=<filename>\n\
                                   --[value|param]=<value>\n\
--otp --read --address=<otp offset> --length=<bytes> --file=<filename>\n\
--otp --write --address=<otp offset> --file=<filename>\n\
--dump --target=<target name> [--hex] [--path=<pathname>]\n\
--quiet\n\
--device=<device name> (if not default)\n\
The options can also be given in the abbreviated form --option=x or -o x.\n\
The options can be given in any order.";

#define A_ROUND_UP(x, y)             ((((x) + ((y) - 1)) / (y)) * (y))

#define quiet() (flag & QUIET_FLAG)
#define nqprintf(args...) if (!quiet()) {printf(args);}
#define min(x,y) ((x) < (y) ? (x) : (y))

void ReadTargetRange(int dev, A_UINT32 address, A_UINT8 *buffer,
                     A_UINT32 length);
void ReadTargetWord(int dev, A_UINT32 address, A_UINT32 *buffer);
void WriteTargetRange(int dev, A_UINT32 address, A_UINT8 *buffer,
                      A_UINT32 length);
void WriteTargetWord(int dev, A_UINT32 address, A_UINT32 value);
int ValidWriteOTP(int dev, A_UINT32 address, A_UINT8 *buffer, A_UINT32 length);

static inline void *
MALLOC(int nbytes)
{
    void *p= malloc(nbytes);

    if (!p)
    {
        fprintf(stderr, "err -Cannot allocate memory\n");
    }

    return p;
}

void
usage(void)
{
    fprintf(stderr, "usage:\n%s ", progname);
    fprintf(stderr, "%s\n", commands);
    exit(-1);
}

void
list_supported_target_names()
{
    int i, target_num = sizeof(target_info)/sizeof(target_info[0]);

    fprintf(stderr, "supported target parameter as follow:\n");
    for (i = 0; i < target_num; i++) {
        fprintf(stderr, "\t--target=%s\n", target_info[i].name);
    }
}

void
ReadTargetRange(int dev, A_UINT32 address, A_UINT8 *buffer, A_UINT32 length)
{
    int nbyte;
    unsigned int remaining;

    (void)lseek(dev, address, SEEK_SET);

    remaining = length;
    while (remaining) {
        nbyte = read(dev, buffer, (size_t)remaining);
        if (nbyte <= 0) {
            fprintf(stderr, "err %s failed (nbyte=%d, address=0x%x"
                    " remaining=%d).\n",
                    __FUNCTION__, nbyte, address, remaining);
            exit(1);
        }

        remaining -= nbyte;
        buffer += nbyte;
        address += nbyte;
    }
}

void
ReadTargetWord(int dev, A_UINT32 address, A_UINT32 *buffer)
{
    ReadTargetRange(dev, address, (A_UINT8 *)buffer, sizeof(*buffer));
}

void
ReadTargetOTP(int dev, A_UINT32 offset, A_UINT8 *buffer, A_UINT32 length)
{
    A_UINT32 status_mask;
    A_UINT32 otp_status, i;

    /* Enable OTP reads */
    WriteTargetWord(dev, RTC_SOC_BASE_ADDRESS+OTP_OFFSET, OTP_VDD12_EN_SET(1));
    status_mask = OTP_STATUS_VDD12_EN_READY_SET(1);
    do {
        ReadTargetWord(dev, RTC_SOC_BASE_ADDRESS+OTP_STATUS_OFFSET,
                       &otp_status);
    } while ((otp_status & OTP_STATUS_VDD12_EN_READY_MASK) != status_mask);

    /* Conservatively set OTP read timing */
    WriteTargetWord(dev, EFUSE_BASE_ADDRESS+RD_STROBE_PW_REG_OFFSET, 6);

    /* Read data from OTP */
    for (i=0; i<length; i++, offset++) {
        A_UINT32 efuse_word;

        ReadTargetWord(dev, EFUSE_BASE_ADDRESS+EFUSE_INTF0_OFFSET+(offset<<2),
                       &efuse_word);
        buffer[i] = (A_UINT8)efuse_word;
    }

    /* Disable OTP */
    WriteTargetWord(dev, RTC_SOC_BASE_ADDRESS+OTP_OFFSET, 0);
}

void
WriteTargetRange(int dev, A_UINT32 address, A_UINT8 *buffer, A_UINT32 length)
{
    int nbyte;
    unsigned int remaining;

    (void)lseek(dev, address, SEEK_SET);

    remaining = length;
    while (remaining) {
        nbyte = write(dev, buffer, (size_t)remaining);
        if (nbyte <= 0) {
            fprintf(stderr, "err %s failed (nbyte=%d, address=0x%x"
                    " remaining=%d).\n",
                    __FUNCTION__, nbyte, address, remaining);
            exit(1);
        }

        remaining -= nbyte;
        buffer += nbyte;
        address += nbyte;
    }
}

void
WriteTargetWord(int dev, A_UINT32 address, A_UINT32 value)
{
    A_UINT32 param = value;

    WriteTargetRange(dev, address, (A_UINT8 *)&param, sizeof(param));
}

#define BAD_OTP_WRITE(have, want) ((((have) ^ (want)) & (have)) != 0)

/*
 * Check if the current contents of OTP and the desired
 * contents specified by buffer/length are compatible.
 * If we're trying to CLEAR an OTP bit, then this request
 * is invalid.
 * returns: 0-->INvalid; 1-->valid
 */
int
ValidWriteOTP(int dev, A_UINT32 offset, A_UINT8 *buffer, A_UINT32 length)
{
    A_UINT32 i;
    A_UINT8 *otp_contents;

    otp_contents = MALLOC(length);
    if (otp_contents == NULL)
        return 0;
    ReadTargetOTP(dev, offset, otp_contents, length);

    for (i=0; i<length; i++) {
        if (BAD_OTP_WRITE(otp_contents[i], buffer[i])) {
            fprintf(stderr, "Abort. Cannot change offset %d from 0x%02x"
                    " to 0x%02x\n",
                    offset+i, otp_contents[i], buffer[i]);
            free(otp_contents);
            return 0;
        }
    }
    free(otp_contents);
    return 1;
}

/*
 * This is NOT the ideal way to write OTP since it does not handle
 * media errors.  It's much better to use the otpstream_* API.
 * This capability is here to help salvage parts that have previously
 * had OTP written.
 */
void
WriteTargetOTP(int dev, A_UINT32 offset, A_UINT8 *buffer, A_UINT32 length)
{
    A_UINT32 status_mask;
    A_UINT32 otp_status, i;

    /* Enable OTP read/write power */
    WriteTargetWord(dev, RTC_SOC_BASE_ADDRESS+OTP_OFFSET,
                    OTP_VDD12_EN_SET(1) | OTP_LDO25_EN_SET(1));
    status_mask = OTP_STATUS_VDD12_EN_READY_SET(1) |
                  OTP_STATUS_LDO25_EN_READY_SET(1);
    do {
        ReadTargetWord(dev, RTC_SOC_BASE_ADDRESS+OTP_STATUS_OFFSET,
                       &otp_status);
    } while ((otp_status & (OTP_STATUS_VDD12_EN_READY_MASK|
              OTP_STATUS_LDO25_EN_READY_MASK)) != status_mask);

    /* Conservatively set OTP read/write timing for 110MHz core clock */
    WriteTargetWord(dev, EFUSE_BASE_ADDRESS+VDDQ_SETTLE_TIME_REG_OFFSET, 2200);
    WriteTargetWord(dev, EFUSE_BASE_ADDRESS+PG_STROBE_PW_REG_OFFSET, 605);
    WriteTargetWord(dev, EFUSE_BASE_ADDRESS+RD_STROBE_PW_REG_OFFSET, 6);

    /* Enable eFuse for write */
    WriteTargetWord(dev, EFUSE_BASE_ADDRESS+EFUSE_WR_ENABLE_REG_OFFSET,
                    EFUSE_WR_ENABLE_REG_V_SET(1));
    WriteTargetWord(dev, EFUSE_BASE_ADDRESS+BITMASK_WR_REG_OFFSET, 0x00);

    /* Write data to OTP */
    for (i=0; i<length; i++, offset++) {
        A_UINT32 efuse_word;
        A_UINT32 readback;
        int attempt;

#define EFUSE_WRITE_COUNT 3
        efuse_word = (A_UINT32)buffer[i];
        for (attempt=1; attempt<=EFUSE_WRITE_COUNT; attempt++) {
            WriteTargetWord(dev,
                            EFUSE_BASE_ADDRESS+EFUSE_INTF0_OFFSET+(offset<<2),
                            efuse_word);
        }

        /* verify */
        ReadTargetWord(dev, EFUSE_BASE_ADDRESS+EFUSE_INTF0_OFFSET+(offset<<2),
                       &readback);
        if (efuse_word != readback) {
            fprintf(stderr, "OTP write failed. Offset=%d, Value=0x%x,"
                    " Readback=0x%x\n", offset, efuse_word, readback);
            break;
        }
    }

    /* Disable OTP */
    WriteTargetWord(dev, RTC_SOC_BASE_ADDRESS+OTP_OFFSET, 0);
}

void
DumpTargetMem(int dev, unsigned int target_idx, char *pathname)
{
    const struct ath_target_reg_info *reg_info;
    FILE * dump_fd;
    char filename[PATH_MAX], tempfn[PATH_MAX];
    A_UINT8 *buffer;
    unsigned int i, address, length, remaining;

    if (target_idx >= MAX_TARGET_INDEX)
        return;

    buffer = (A_UINT8 *)MALLOC(MAX_BUF);
    if (buffer == NULL)
        return;

    reg_info = target_info[target_idx].reg_info;
    while ((reg_info->reg_start != 0) || (reg_info->reg_len != 0)) {
        memset(filename, 0, sizeof(filename));
        snprintf(filename, sizeof(filename), "%s%s", pathname,
                 reg_info->save_file);
        snprintf(tempfn, sizeof(tempfn), "%s", filename);
        if(flag & HEX_FLAG) {
            snprintf(filename, sizeof(filename), "%s.txt", tempfn);
        } else {
            snprintf(filename, sizeof(filename), "%s.bin", tempfn);
        }

        if ((dump_fd = fopen(filename, "wb+")) == NULL) {
            fprintf(stderr, "err %s cannot create/open output file (%s)\n",
                        __FUNCTION__, filename);
            reg_info++;
            continue;
        }

        remaining = length = reg_info->reg_len;
        address   = reg_info->reg_start;
        if(flag & HEX_FLAG) {
            fprintf(dump_fd,"target mem dump area[0x%08x - 0x%08x]",address,
                    address+length);
        }

        nqprintf("DIAG Read Target (address: 0x%x, length: 0x%x, filename: %s)\n",
                    address, length, filename);

        while (remaining) {
            length = (remaining > MAX_BUF) ? MAX_BUF : remaining;
            ReadTargetRange(dev, address, buffer, length);
            if(flag & HEX_FLAG) {
                for(i=0; i<length; i+=4) {
                    if(i%16 == 0)
                        fprintf(dump_fd,"\n0x%08x:\t",address+i);
                    fprintf(dump_fd,"0x%08x\t",*(A_UINT32*)(buffer+i));
                }
            } else {
                fwrite(buffer,1 , length, dump_fd);
            }
            remaining -= length;
            address += length;
        }

        fclose(dump_fd);
        reg_info++;
    }
    free(buffer);
}



unsigned int
parse_address(char *optarg)
{
    unsigned int address;

    /* may want to add support for symbolic addresses here */

    address = strtoul(optarg, NULL, 0);

    return address;
}

unsigned int
parse_target_index(char *optarg)
{
    unsigned int i, index = INVALID_TARGET_INDEX;

    for (i = 0; i < sizeof(target_info)/sizeof(target_info[0]); i++) {
        if (strncmp(optarg, target_info[i].name, strlen(target_info[i].name)) == 0) {
            /* found */
            index = i;
            break;
        }
    }

    return index;
}

int
main (int argc, char **argv) {
    int c, fd, dev;
    int i;
    FILE * dump_fd;
    unsigned int address = 0, target_idx = 0, length = 0;
    A_UINT32 param = 0;
    char filename[PATH_MAX], tempfn[PATH_MAX];
    char pathname[PATH_MAX];
    char devicename[PATH_MAX];
    unsigned int cmd = 0;
    A_UINT8 *buffer;
    unsigned int bitwise_mask = 0;

    progname = argv[0];

    if (argc == 1) usage();

    flag = 0;
    memset(filename, '\0', sizeof(filename));
    memset(devicename, '\0', sizeof(devicename));
    memset(tempfn, '\0', sizeof(tempfn));

    while (1) {
        int option_index = 0;
        static struct option long_options[] = {
            {"address", 1, NULL, 'a'},
            {"and", 1, NULL, 'n'},
            {"device", 1, NULL, 'D'},
            {"dump", 0, NULL, 'd'},
            {"get", 0, NULL, 'g'},
            {"file", 1, NULL, 'f'},
            {"hex", 0, NULL, 'x'},
            {"length", 1, NULL, 'l'},
            {"or", 1, NULL, 'o'},
            {"otp", 0, NULL, 'O'},
            {"param", 1, NULL, 'p'},
            {"path", 1, NULL, 'P'},
            {"quiet", 0, NULL, 'q'},
            {"read", 0, NULL, 'r'},
            {"set", 0, NULL, 's'},
            {"target", 1, NULL, 't'},
            {"value", 1, NULL, 'p'},
            {"write", 0, NULL, 'w'},
            {0, 0, 0, 0}
        };

        c = getopt_long (argc, argv, "xrwgsqdOf:l:a:p:c:n:o:D:t:P:",
                         long_options, &option_index);
        if (c == -1)
            break;

        switch (c) {
        case 'r':
            cmd = DIAG_READ_TARGET;
            break;

        case 'w':
            cmd = DIAG_WRITE_TARGET;
            break;

        case 'g':
            cmd = DIAG_READ_WORD;
            break;

        case 's':
            cmd = DIAG_WRITE_WORD;
            break;

        case 'd':
            cmd = DIAG_DUMP_TARGET;
            break;

        case 'f':
            memset(filename, '\0', sizeof(filename));
            snprintf(filename, sizeof(filename), "%s", optarg);
            flag |= FILE_FLAG;
            break;

        case 'l':
            length = parse_address(optarg);
            flag |= LENGTH_FLAG;
            break;

        case 'a':
            address = parse_address(optarg);
            flag |= ADDRESS_FLAG;
            break;

        case 'p':
            param = strtoul(optarg, NULL, 0);
            flag |= PARAM_FLAG;
            break;

        case 'n':
            flag |= PARAM_FLAG | AND_OP_FLAG | BITWISE_OP_FLAG;
            bitwise_mask = strtoul(optarg, NULL, 0);
            break;

        case 'o':
            flag |= PARAM_FLAG | BITWISE_OP_FLAG;
            bitwise_mask = strtoul(optarg, NULL, 0);
            break;

        case 'O':
            flag |= OTP_FLAG;
            break;

        case 'q':
            flag |= QUIET_FLAG;
            break;

        case 't':
            target_idx = parse_target_index(optarg);
            if (target_idx != INVALID_TARGET_INDEX) {
                flag |= TARGET_FLAG;
            }
            break;

        case 'P':
            memset(pathname, '\0', sizeof(pathname));
            snprintf(pathname, sizeof(pathname), "%s", optarg);
            if (pathname[strlen(pathname)-1] != '/') {
                snprintf(tempfn, sizeof(tempfn),"%s", pathname);
                snprintf(pathname, sizeof(pathname),"%s/", tempfn);
            }
            flag |= PATH_FLAG;
            break;

        case 'D':
            snprintf(devicename, sizeof(devicename), "%s%s", optarg,
                     "/athdiagpfs");
            flag |= DEVICE_FLAG;
            break;

        case 'x':
            flag |= HEX_FLAG;
            break;

        default:
            fprintf(stderr, "Cannot understand '%s'\n", argv[option_index]);
            usage();
        }
    }

    for (;;) {
        /* DIAG uses a sysfs special file which may be auto-detected */
        if (!(flag & DEVICE_FLAG)) {
            FILE *find_dev;
            size_t nbytes;

            /*
             * Convenience: if no device was specified on the command
             * line, try to figure it out.  Typically there's only a
             * single device anyway.
             */
            find_dev = popen("echo /proc/cld/athdiagpfs", "r");
            if (find_dev) {
                nbytes=fread(devicename, 1, sizeof(devicename), find_dev);
                pclose(find_dev);

                if (nbytes > 15) {
                    /* auto-detect possibly successful */
                    devicename[nbytes-1]='\0'; /* replace \n with 0 */
                } else {
                    snprintf(devicename, sizeof(devicename), "%s",
                             "unknown_DIAG_device");
                }
            }
        }

        dev = open(devicename, O_RDWR);
        if (dev >= 0) {
            break; /* successfully opened diag special file */
        } else {
            fprintf(stderr, "err %s failed (%d) to open DIAG file (%s)\n",
                __FUNCTION__, errno, devicename);
            exit(1);
        }
    }

    switch(cmd)
    {
    case DIAG_READ_TARGET:
        if ((flag & (ADDRESS_FLAG | LENGTH_FLAG | FILE_FLAG)) ==
                (ADDRESS_FLAG | LENGTH_FLAG | FILE_FLAG))
        {
            if ((dump_fd = fopen(filename, "wb+")) == NULL)
            {
                fprintf(stderr, "err %s cannot create/open output file (%s)\n",
                        __FUNCTION__, filename);
                exit(1);
            }

            buffer = (A_UINT8 *)MALLOC(MAX_BUF);
            if (buffer == NULL) {
                fclose(dump_fd);
                close(dev);
                exit(1);
            }
            nqprintf(
                    "DIAG Read Target (address: 0x%x, length: %d,"
                    " filename: %s)\n", address, length, filename);
            {
                unsigned int remaining = length;

                if(flag & HEX_FLAG)
                {
                    if (flag & OTP_FLAG) {
                        fprintf(dump_fd,"target otp dump area"
                                " [0x%08x - 0x%08x]",address,address+length);
                    } else {
                        fprintf(dump_fd,"target mem dump area"
                                " [0x%08x - 0x%08x]",address,address+length);
                    }
                }
                while (remaining)
                {
                    length = (remaining > MAX_BUF) ? MAX_BUF : remaining;
                    if (flag & OTP_FLAG) {
                        ReadTargetOTP(dev, address, buffer, length);
                    } else {
                        ReadTargetRange(dev, address, buffer, length);
                    }
                    if(flag & HEX_FLAG)
                    {
                        for(i=0;i<(int)length;i+=4)
                        {
                            if(i%16 == 0)
                                fprintf(dump_fd,"\n0x%08x:\t",address+i);
                            fprintf(dump_fd,"0x%08x\t",*(A_UINT32*)(buffer+i));
                        }
                    }
                    else
                    {
                        fwrite(buffer,1 , length, dump_fd);
                    }
                    remaining -= length;
                    address += length;
                }
            }
            fclose(dump_fd);
            free(buffer);
        } else {
            usage();
        }
        break;

    case DIAG_WRITE_TARGET:
        if (!(flag & ADDRESS_FLAG))
        {
            usage(); /* no address specified */
        }
        if (!(flag & (FILE_FLAG | PARAM_FLAG)))
        {
            usage(); /* no data specified */
        }
        if ((flag & FILE_FLAG) && (flag & PARAM_FLAG))
        {
            usage(); /* too much data specified */
        }

        if (flag & FILE_FLAG)
        {
            struct stat filestat;
            unsigned int file_length;

            if ((fd = open(filename, O_RDONLY)) < 0)
            {
                fprintf(stderr, "err %s Could not open file"
                        " (%s)\n", __FUNCTION__, filename);
                exit(1);
            }
            memset(&filestat, '\0', sizeof(struct stat));
            buffer = (A_UINT8 *)MALLOC(MAX_BUF);
            if (buffer == NULL) {
                close(fd);
                exit(1);
            }
            fstat(fd, &filestat);
            file_length = filestat.st_size;
            if (file_length == 0) {
                fprintf(stderr, "err %s Zero length input file"
                        " (%s)\n", __FUNCTION__, filename);
                exit(1);
            }

            if (flag & LENGTH_FLAG) {
                if (length > file_length) {
                    fprintf(stderr, "err %s file %s: length (%d)"
                            " too short (%d)\n", __FUNCTION__,
                        filename, file_length, length);
                    exit(1);
                }
            } else {
                length = file_length;
            }

            nqprintf(
                 "DIAG Write Target (address: 0x%x, filename: %s,"
                 " length: %d)\n", address, filename, length);

        }
        else
        { /* PARAM_FLAG */
            nqprintf(
                 "DIAG Write Word (address: 0x%x, value: 0x%x)\n",
                  address, param);
            length = sizeof(param);
            buffer = (A_UINT8 *)&param;
            fd = -1;
        }

        /*
         * Write length bytes of data to memory/OTP.
         * Data is either present in buffer OR
         * needs to be read from fd in MAX_BUF chunks.
         *
         * Within the kernel, the implementation of
         * DIAG_WRITE_TARGET further limits the size
         * of each transfer over the interconnect.
         */
        {
            unsigned int remaining;
            unsigned int otp_check_address = address;

            if (flag & OTP_FLAG) {
                /* Validate OTP write before committing anything */
                remaining = length;
                while (remaining)
                {
                    int nbyte;

                    length = (remaining > MAX_BUF) ? MAX_BUF : remaining;
                    if (fd > 0)
                    {
                        nbyte = read(fd, buffer, length);
                        if (nbyte != (int)length) {
                            fprintf(stderr, "err %s read from file failed"
                                    " (%d)\n", __FUNCTION__, nbyte);
                            exit(1);
                        }
                    }

                    if ((flag & OTP_FLAG) && !ValidWriteOTP(dev,
                                otp_check_address, buffer, length))
                    {
                            exit(1);
                    }

                    remaining -= length;
                    otp_check_address += length;
                }
                (void)lseek(fd, 0, SEEK_SET);
            }

            remaining = length;
            while (remaining)
            {
                int nbyte;

                length = (remaining > MAX_BUF) ? MAX_BUF : remaining;
                if (fd > 0)
                {
                    nbyte = read(fd, buffer, length);
                    if (nbyte != (int)length) {
                        fprintf(stderr, "err %s read from file failed"
                                " (%d)\n", __FUNCTION__, nbyte);
                        exit(1);
                    }
                }

                if (flag & OTP_FLAG) {
                    WriteTargetOTP(dev, address, buffer, length);
                } else {
                    WriteTargetRange(dev, address, buffer, length);
                }

                remaining -= length;
                address += length;
            }
        }

        if (flag & FILE_FLAG) {
            free(buffer);
            close(fd);
        }

        break;

    case DIAG_READ_WORD:
        if ((flag & (ADDRESS_FLAG)) == (ADDRESS_FLAG))
        {
            nqprintf("DIAG Read Word (address: 0x%x)\n", address);
            ReadTargetWord(dev, address, &param);

            if (quiet()) {
                printf("0x%x\n", param);
            } else {
                printf("Value in target at 0x%x: 0x%x (%d)\n",
                        address, param, param);
            }
        }
        else usage();
        break;

    case DIAG_WRITE_WORD:
        if ((flag & (ADDRESS_FLAG | PARAM_FLAG)) == (ADDRESS_FLAG | PARAM_FLAG))
        {
            A_UINT32 origvalue = 0;

            if (flag & BITWISE_OP_FLAG) {
                /* first read */
                ReadTargetWord(dev, address, &origvalue);
                param = origvalue;

                /* now modify */
                if (flag & AND_OP_FLAG) {
                    param &= bitwise_mask;
                } else {
                    param |= bitwise_mask;
                }
            /* fall through to write out the parameter */
            }

            if (flag & BITWISE_OP_FLAG) {
                if (quiet()) {
                    printf("0x%x\n", origvalue);
                } else {
                    printf("DIAG Bit-Wise (%s) modify Word (address: 0x%x,"
                           " orig:0x%x, new: 0x%x,  mask:0x%X)\n",
                           (flag & AND_OP_FLAG) ? "AND" : "OR", address,
                           origvalue, param, bitwise_mask );
                }
            } else{
                nqprintf("DIAG Write Word (address: 0x%x, param:"
                         " 0x%x)\n", address, param);
            }

            WriteTargetWord(dev, address, param);
        }
        else usage();
        break;

    case DIAG_DUMP_TARGET:
        if (!(flag & TARGET_FLAG)) {
            list_supported_target_names();
            usage(); /* no target specified */
        }

        if (!(flag & PATH_FLAG)) {
            memset(pathname, '\0', sizeof(filename));
            if (getcwd(pathname, sizeof(pathname)-1) != NULL)
                printf("%s\n",pathname);
            snprintf(tempfn, sizeof(tempfn), "%s", pathname);
            snprintf(pathname, sizeof(pathname), "%s/", tempfn);
        }

        DumpTargetMem(dev, target_idx, pathname);
        break;

    default:
        usage();
    }

    exit (0);
}
