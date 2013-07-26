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

#ifndef __OIS_I2C_COMMON_H
#define __OIS_I2C_COMMON_H

#include <linux/i2c.h>

int32_t ram_write_addr(uint16_t RamAddr, uint16_t RamData);
int32_t ram_read_addr(uint16_t RamAddr, uint16_t *ReadData);
int32_t ram_write_32addr(uint16_t RamAddr, uint32_t RamData);
int32_t ram_read_32addr(uint16_t RamAddr, uint32_t *ReadData);
int32_t reg_write_addr(uint16_t RegAddr, uint8_t RegData);
int32_t reg_read_addr(uint16_t RegAddr, uint8_t *RegData);
int32_t e2p_reg_write_addr(uint16_t RegAddr, uint8_t RegData);
int32_t e2p_reg_read_addr(uint16_t RegAddr, uint8_t *RegData);
int32_t e2p_read(uint16_t UsAdr, uint8_t UcLen, uint8_t *UcPtr);
int32_t e2p_write(uint16_t UsAdr, uint8_t UcLen, uint8_t *UcPtr);

#ifdef CONFIG_OIS_CONTROLLER
int32_t ois_i2c_write_table(struct msm_camera_i2c_reg_setting *write_setting);
int32_t ois_i2c_write_seq_table(struct msm_camera_i2c_seq_reg_setting *write_setting);
int32_t ois_i2c_write(uint16_t addr, uint16_t data, enum msm_camera_i2c_data_type data_type);
int32_t ois_i2c_read(uint16_t addr, uint16_t *data, enum msm_camera_i2c_data_type data_type);
int32_t ois_i2c_write_seq(uint16_t addr, uint8_t *data, uint16_t num_byte);
int32_t ois_i2c_read_seq(uint16_t addr, uint8_t *data, uint16_t num_byte);
int32_t ois_i2c_e2p_write(uint16_t addr, uint16_t data, enum msm_camera_i2c_data_type data_type);
int32_t ois_i2c_e2p_read(uint16_t addr, uint16_t *data, enum msm_camera_i2c_data_type data_type);
int32_t ois_i2c_act_write(uint8_t data1, uint8_t data2);

#define MAX_OIS_BIN_FILENAME   64
#define MAX_OIS_BIN_BLOCKS     4
#define MAX_OIS_BIN_FILES      3

struct ois_i2c_bin_addr {
	uint16_t bin_str_addr;
	uint16_t bin_end_addr;
	uint16_t reg_str_addr;
};

struct ois_i2c_bin_entry {
	char  filename[MAX_OIS_BIN_FILENAME];
	uint32_t filesize;
	uint16_t blocks;
	struct ois_i2c_bin_addr addrs[MAX_OIS_BIN_BLOCKS];
};

struct ois_i2c_bin_list {
	uint16_t files;
	struct ois_i2c_bin_entry entries[MAX_OIS_BIN_FILES];
	uint32_t checksum;
};

int32_t ois_i2c_load_and_write_e2prom_data(uint16_t e2p_str_addr,
		uint16_t e2p_data_length, uint16_t reg_str_addr);
int32_t ois_i2c_load_and_write_bin_list(struct ois_i2c_bin_list bin_list);
#endif //CONFIF_OIS_CONTROLLER
#endif
