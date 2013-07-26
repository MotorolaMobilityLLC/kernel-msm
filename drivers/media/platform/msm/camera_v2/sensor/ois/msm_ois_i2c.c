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

#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/file.h>

#ifdef CONFIG_OIS_CONTROLLER
#include "msm_ois.h"
#include "msm_ois_i2c.h"
#endif

int32_t ram_write_addr(uint16_t RamAddr, uint16_t RamData)
{
	return ois_i2c_write(RamAddr, RamData, 2);
}

int32_t ram_read_addr(uint16_t RamAddr, uint16_t *ReadData)
{
	return ois_i2c_read(RamAddr, ReadData, 2);
}

int32_t ram_write_32addr(uint16_t RamAddr, uint32_t RamData)
{
	int32_t ret = 0;
	uint8_t data[4];

	data[0] = (RamData >> 24) & 0xFF;
	data[1] = (RamData >> 16) & 0xFF;
	data[2] = (RamData >> 8)  & 0xFF;
	data[3] = (RamData) & 0xFF;

	ret = ois_i2c_write_seq(RamAddr, &data[0], 4);
	return ret;
}

int32_t ram_read_32addr(uint16_t RamAddr, uint32_t *ReadData)
{
	int32_t ret = 0;
	uint8_t buf[4];

	ret = ois_i2c_read_seq(RamAddr, buf, 4);
	*ReadData = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	return ret;
}

int32_t reg_write_addr(uint16_t RegAddr, uint8_t RegData)
{
	int32_t ret = 0;
	uint16_t data = (uint16_t)RegData;

	ret = ois_i2c_write(RegAddr, data, 1);
	return ret;
}

int32_t reg_read_addr(uint16_t RegAddr, uint8_t *RegData)
{
	int32_t ret = 0;
	uint16_t data = 0;

	ret = ois_i2c_read(RegAddr, &data, 1);
	*RegData = (uint8_t)data;
	return ret;
}

int32_t e2p_reg_write_addr(uint16_t RegAddr, uint8_t RegData)
{
	int32_t ret = 0;
	uint16_t data = (uint16_t)RegData;

	ret = ois_i2c_e2p_write(RegAddr, data, 1);
	return ret;
}

int32_t e2p_reg_read_addr(uint16_t RegAddr, uint8_t *RegData)
{
	int32_t ret = 0;
	uint16_t data = 0;

	ret = ois_i2c_e2p_read(RegAddr, &data, 1);
	*RegData = (uint8_t)data;
	return ret;
}

int32_t e2p_read(uint16_t UsAdr, uint8_t UcLen, uint8_t *UcPtr)
{
	uint16_t UcCnt;

	if (UcLen > 0) {
		UcPtr += UcLen - 1;
		for (UcCnt = 0; UcCnt < UcLen;  UcCnt++, UcPtr--)
			e2p_reg_read_addr(UsAdr + UcCnt, UcPtr);
	}
	return 0;
}

int32_t e2p_write(uint16_t UsAdr, uint8_t UcLen, uint8_t *UcPtr)
{
	uint8_t UcCnt;

	for (UcCnt = 0; UcCnt < UcLen;  UcCnt++)
		e2p_reg_write_addr(UsAdr + UcCnt, (uint8_t)UcPtr[abs((UcLen - 1) - UcCnt)]);

	return 0;
}

#ifdef CONFIG_OIS_CONTROLLER

#define I2C_NUM_BYTE		 (6)  //QCT maximum size is 6!

static int32_t load_bin(uint8_t *ois_bin, uint32_t filesize, char *filename)
{
	int fd1;
	uint8_t fs_name_buf1[256];
	uint32_t cur_fd_pos;
	int32_t rc = OIS_SUCCESS;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);
	sprintf(fs_name_buf1, "/system/vendor/firmware/%s", filename);
	fd1 = sys_open(fs_name_buf1, O_RDONLY, 0);
	if (fd1 < 0) {
		pr_err("%s: File not exist \n", __func__);
		rc = OIS_INIT_LOAD_BIN_ERROR;
		goto END;
	}
	if ((unsigned)sys_lseek(fd1, (off_t)0, 2) != filesize) {
		pr_err("%s: File size error \n", __func__);
		rc = OIS_INIT_LOAD_BIN_ERROR;
		goto END;
	}
	cur_fd_pos = (unsigned)sys_lseek(fd1, (off_t)0, 0);
	memset(ois_bin, 0x00, filesize);
	if ((unsigned)sys_read(fd1, (char __user *)ois_bin, filesize) != filesize) {
		pr_err("%s: File read error \n", __func__);
		rc = OIS_INIT_LOAD_BIN_ERROR;
	}

END:
	sys_close(fd1);
	set_fs(old_fs);
	return rc;
}

static int32_t ois_i2c_bin_seq_write(int src_saddr,
	 int src_eaddr, int dst_addr, uint8_t *ois_bin)
{
	int i, j;
	int total_byte, remaining_byte;
	struct msm_camera_i2c_seq_reg_setting conf_array;
	struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;
	int32_t rc = 0;

	total_byte = src_eaddr - src_saddr + 1;
	conf_array.size = total_byte / I2C_NUM_BYTE;
	remaining_byte = total_byte - conf_array.size * I2C_NUM_BYTE;

	CDBG("%s, write seq array size = %d, remaining_byte = %d\n",
		 __func__, conf_array.size, remaining_byte);

	if (remaining_byte < 0) {
		CDBG("%s, remaining_byte = %d\n", __func__, remaining_byte);
		conf_array.size = conf_array.size - 1;
		remaining_byte = total_byte - conf_array.size * I2C_NUM_BYTE;
	}

	reg_setting = kzalloc(conf_array.size *
		(sizeof(struct msm_camera_i2c_seq_reg_array)), GFP_KERNEL);
	if (!reg_setting) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		return -ENOMEM;
	}

	for (i = 0; i < conf_array.size; i++) {
		reg_setting[i].reg_addr = dst_addr;
		reg_setting[i].reg_data_size = I2C_NUM_BYTE;
		for (j = 0; j < I2C_NUM_BYTE; j++)
			reg_setting[i].reg_data[j] = ois_bin[src_saddr++];
		dst_addr = dst_addr + I2C_NUM_BYTE;
	}

	conf_array.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	conf_array.delay = 0;
	conf_array.reg_setting = reg_setting;
	rc = ois_i2c_write_seq_table(&conf_array);
	if (rc < 0) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		goto END;
	}
	CDBG("%s, write array seq finished!, src_saddr %d, dst_addr %d \n",
		 __func__, src_saddr, dst_addr);

	if (remaining_byte > 0) {
		rc = ois_i2c_write_seq(dst_addr, &ois_bin[src_saddr], remaining_byte);
		CDBG("%s, remaining bytes write!, dst_addr %d, src_saddr %d, remaining_byte %d\n",
			 __func__, dst_addr, src_saddr, remaining_byte);
	}

END:
	kfree(reg_setting);
	return rc;
}

static int32_t ois_i2c_load_and_write_bin(struct ois_i2c_bin_entry bin_entry)
{
	int i;
	int32_t rc = OIS_SUCCESS;
	uint8_t *ois_bin_data = NULL;
	struct ois_i2c_bin_addr addr;
	CDBG("%s\n", __func__);

	ois_bin_data = kmalloc(bin_entry.filesize, GFP_KERNEL);
	if (!ois_bin_data) {
		pr_err("%s: Can not alloc bin data\n", __func__);
		rc = OIS_INIT_NOMEM;
		goto END2;
	}

	rc = load_bin(ois_bin_data, bin_entry.filesize, bin_entry.filename);
	if (rc < 0)
		goto END1;

	for (i = 0; i < bin_entry.blocks; i++) {
		addr = bin_entry.addrs[i];
		rc = ois_i2c_bin_seq_write(addr.bin_str_addr,
			addr.bin_end_addr, addr.reg_str_addr, ois_bin_data);
		if (rc < 0) {
			pr_err("%s: program %d down error\n", __func__, i);
			rc = OIS_INIT_I2C_ERROR;
			goto END1;
		}
	}

END1:
	kfree(ois_bin_data);
END2:
	return rc;
}

int32_t ois_i2c_load_and_write_bin_list(struct ois_i2c_bin_list bin_list)
{
	int i;
	int length = bin_list.files;
	int32_t rc = OIS_SUCCESS;

	for (i = 0; i < length; i++) {
		rc = ois_i2c_load_and_write_bin(bin_list.entries[i]);
		if (rc < 0)
			goto END;
	}

END:
	return rc;
}

int32_t ois_i2c_load_and_write_e2prom_data(uint16_t e2p_str_addr,
		uint16_t e2p_data_length, uint16_t reg_str_addr)
{
	int32_t rc = OIS_SUCCESS;
	uint8_t *e2p_cal_data = NULL;
	int i;
	uint16_t data;

	/* Read E2P data!*/
	e2p_cal_data = kmalloc(e2p_data_length, GFP_KERNEL);
	if (!e2p_cal_data) {
		pr_err("%s: Can not alloc e2p data\n", __func__);
		rc = OIS_INIT_NOMEM;
		goto END2;
	}
	memset(e2p_cal_data, 0x00, e2p_data_length);

	/* start reading from e2p rom*/
	for (i = 0; i < e2p_data_length; i++) {
		ois_i2c_e2p_read(e2p_str_addr + i, &data, 1);
		e2p_cal_data[i] = data & 0xFF;
		CDBG("%s: eeprom %x, %x\n", __func__, e2p_str_addr + i, e2p_cal_data[i]);
	}

	rc = ois_i2c_bin_seq_write(0, e2p_data_length - 1, reg_str_addr, e2p_cal_data);
	if (rc < 0) {
		pr_err("%s: e2p down error\n", __func__);
		goto END1;
	}

END1:
	kfree(e2p_cal_data);
END2:
	return rc;
}
#endif
