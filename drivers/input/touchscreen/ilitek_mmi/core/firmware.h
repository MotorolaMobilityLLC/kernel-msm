/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 * Based on TDD v7.0 implemented by Mstar & ILITEK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#ifndef __FIRMWARE_H
#define __FIRMWARE_H

#define CHECK_FW_FAIL  -1
#define NEED_UPDATE	1
#define NO_NEED_UPDATE  0

#define AP_STAR_ADDR    0x00000
#define AP_END_ADDR	0x0FFFF
#define DATA_STAR_ADDR  0x10000
#define DATA_END_ADDR	0x11FFF
#define MP_STAR_ADDR    0x13000
#define MP_END_ADDR	0x1BFFF

struct core_firmware_data {
uint8_t new_fw_ver[4];
uint8_t old_fw_ver[4];

uint32_t start_addr;
uint32_t end_addr;
uint32_t checksum;
uint32_t crc32;
uint32_t update_status;
uint32_t max_count;

int delay_after_upgrade;

bool isUpgrading;
bool force_upgrad;
bool isCRC;
bool isboot;
bool hasBlockInfo;

int (*upgrade_func)(bool isIRAM);
};

extern struct core_firmware_data *core_firmware;

#ifdef HOST_DOWNLOAD
extern int core_firmware_boot_host_download(void);
extern int tddi_host_download(bool isIRAM);
#endif
#ifdef BOOT_FW_UPGRADE
extern int core_firmware_boot_upgrade(void);
#endif
/* extern int core_firmware_iram_upgrade(const char* fpath); */
extern int core_firmware_upgrade(const char *pFilePath, bool isIRAM);
extern int core_firmware_init(void);
extern int tddi_check_fw_upgrade(void);
extern void core_firmware_remove(void);

#endif /* __FIRMWARE_H */
