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

/*
 * LGIT OIS firmware
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <mach/gpio.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/types.h>
#include <linux/syscalls.h>
#include <linux/fcntl.h>
#include <linux/file.h>
#include <mach/camera2.h>
#include <linux/poll.h>
#include "msm_ois.h"
#include "msm_ois_i2c.h"
#include "lgit_ois.h"

static int cal_i2c_array_byte(int s_addr, int e_addr)
{
	return (e_addr - s_addr + 1);
}

static int lgit_ois_program_seq_write(int src_saddr,
	int src_eaddr, int dst_addr, uint8_t *ois_bin)
{
	int cnt, cnt2;
	int total_byte, remaining_byte;
	struct msm_camera_i2c_seq_reg_setting conf_array;
	struct msm_camera_i2c_seq_reg_array *reg_setting = NULL;
	int rc = 0;

	total_byte = cal_i2c_array_byte(src_saddr, src_eaddr);
	conf_array.size = total_byte / I2C_NUM_BYTE;
	remaining_byte = total_byte - conf_array.size * I2C_NUM_BYTE;
	CDBG("%s, write seq array size = %d, remaining_byte = %d\n",
		__func__, conf_array.size, remaining_byte);

	reg_setting = kzalloc(conf_array.size *
		(sizeof(struct msm_camera_i2c_seq_reg_array)), GFP_KERNEL);
	if (!reg_setting) {
		pr_err("%s: out of memory\n", __func__);
		return -ENOMEM;
	}

	for (cnt = 0; cnt < conf_array.size; cnt++) {
		reg_setting[cnt].reg_addr = dst_addr;
		reg_setting[cnt].reg_data_size = I2C_NUM_BYTE;
		dst_addr += I2C_NUM_BYTE;
		for (cnt2 = 0; cnt2 < I2C_NUM_BYTE; cnt2++)
			reg_setting[cnt].reg_data[cnt2] = ois_bin[src_saddr++];
	}

	conf_array.addr_type = MSM_CAMERA_I2C_WORD_ADDR;
	conf_array.delay = 0;
	conf_array.reg_setting = reg_setting;
	rc = ois_i2c_write_seq_table(&conf_array);
	if (rc < 0) {
		pr_err("%s:%d failed\n", __func__, __LINE__);
		goto out;
	}
	CDBG("%s, write array seq finished!, src_saddr %d, dst_addr %d \n",
		__func__, src_saddr, dst_addr);

	if (remaining_byte > 0) {
		ois_i2c_write_seq(dst_addr, &ois_bin[src_saddr], remaining_byte);
		CDBG("%s, remaining bytes write! "
			"dst_addr %d, src_saddr %d, remaining_byte %d \n",
			__func__, dst_addr, src_saddr, remaining_byte);
	}

out:
	kfree(reg_setting);
	return rc;
}

static int lgit_ois_poll_ready(int limit)
{
	uint8_t ois_status;
	int read_byte = 0;

	RegReadA(0x6024, &ois_status); /* polling status ready */
	read_byte++;

	while ((ois_status != 0x01) && (read_byte < limit)) {
		usleep(2000); /* wait 2ms */
		RegReadA(0x6024, &ois_status); /* polling status ready */
		read_byte++;
	}

	CDBG("%s, 0x6024 read_byte = %d %d\n", __func__, read_byte, ois_status);
	return ois_status;
}

static int lgit_bin_download(void)
{
	int rc = 0;
	int cnt;
	int fd1 = -1;
	int fd2 = -1;
	int fd3 = -1;
	uint32_t file_size;
	uint32_t cur_fd_pos;
	int read_byte;
	int32_t read_value_32t;
	uint8_t *bin1 = NULL;
	uint8_t *bin2 = NULL;
	uint8_t *bin3 = NULL;
	uint8_t *e2p_cal_data = NULL;
	uint8_t buf1[100];
	uint8_t buf2[100];
	uint8_t buf3[100];
	uint16_t cal_ver = 0;
	int sum_value;
	uint16_t data;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	CDBG("%s\n", __func__);

	ois_i2c_e2p_read(E2P_FIRST_ADDR + 0x1C, &cal_ver, 2);
	++cal_ver;
	pr_info("%s %d.0 \n", __func__, cal_ver);

	sum_value = ((cal_ver==2) ? BIN_SUM_VALUE_2:BIN_SUM_VALUE_3);

	snprintf(buf1, sizeof(buf1),
		"/vendor/firmware/bu24205_LGIT_VER_%d_DATA1.bin", cal_ver);
	snprintf(buf2, sizeof(buf2),
		"/vendor/firmware/bu24205_LGIT_VER_%d_DATA2.bin", cal_ver);
	snprintf(buf3, sizeof(buf3),
		"/vendor/firmware/bu24205_LGIT_VER_%d_DATA3.bin", cal_ver);

	/* First Binary Read */
	fd1 = sys_open(buf1, O_RDONLY, 0);
	if (fd1 < 0) {
		pr_err("%s: Can not open %s[%d]\n", __func__, buf1, fd1);
		rc =  -ENOENT;
		goto file_open_error;
	}

	file_size = (unsigned)sys_lseek(fd1, (off_t)0, 2);
	if (file_size == 0) {
		pr_err("%s: File size is 0\n", __func__);
		rc =  -ENOENT;
		goto file_open_error;
	}

	cur_fd_pos = (unsigned)sys_lseek(fd1, (off_t)0, 0);
	CDBG("%s: firmware cur file pos %u\n", __func__, cur_fd_pos);
	CDBG("%s: firmware file size %d , %u\n", __func__, fd1, file_size);

	bin1 = kzalloc(file_size, GFP_KERNEL);
	if (!bin1) {
		pr_err("%s: Can not alloc bin data\n", __func__);
		rc = -ENOMEM;
		goto file_open_error;
	}

	read_value_32t = sys_read(fd1, (char __user *)bin1, file_size);
	if (read_value_32t != file_size) {
		pr_err("%s: can not read file\n", __func__);
		rc = -ENOMEM;
		goto file_open_error;
	}

	sys_close(fd1);
	fd1 = -1;

	/* Second Binary Read */
	fd2 = sys_open(buf2, O_RDONLY, 0);
	if (fd2 < 0) {
		pr_err("%s: Can not open %s[%d]\n", __func__, buf2, fd2);
		rc =  -ENOENT;
		goto file_open_error;
	}

	file_size = (unsigned)sys_lseek(fd2, (off_t)0, 2);
	if (file_size == 0) {
		pr_err("%s: File size is 0\n", __func__);
		rc =  -ENOENT;
		goto file_open_error;
	}

	cur_fd_pos = (unsigned)sys_lseek(fd2, (off_t)0, 0);

	CDBG("%s: firmware cur file pos %u\n", __func__, cur_fd_pos);
	CDBG("%s: firmware file size %d , %u\n", __func__, fd2, file_size);

	bin2 = kzalloc(file_size, GFP_KERNEL);
	if (!bin2) {
		pr_err("%s: Can not alloc bin data\n", __func__);
		rc = -ENOMEM;
		goto file_open_error;
	}

	read_value_32t = sys_read(fd2, (char __user *)bin2, file_size);
	if (read_value_32t != file_size) {
		pr_err("%s: can not read file\n", __func__);
		rc = -ENOMEM;
		goto file_open_error;
	}

	sys_close(fd2);
	fd2 = -1;

	/* Third Binary Read */
	fd3 = sys_open(buf3, O_RDONLY, 0);
	if (fd3 < 0) {
		pr_err("%s: Can not open %s[%d]\n", __func__, buf3, fd3);
		rc =  -ENOENT;
		goto file_open_error;
	}

	file_size = (unsigned)sys_lseek(fd3, (off_t)0, 2);
	if (file_size == 0) {
		pr_err("%s: File size is 0\n", __func__);
		rc =  -ENOENT;
		goto file_open_error;
	}

	cur_fd_pos = (unsigned)sys_lseek(fd3, (off_t)0, 0);
	CDBG("%s: firmware cur file pos %u\n", __func__, cur_fd_pos);
	CDBG("%s: firmware file size %d , %u\n", __func__, fd3, file_size);

	bin3 = kzalloc(file_size, GFP_KERNEL);
	if (!bin3) {
		pr_err("%s: Can not alloc bin data\n", __func__);
		rc = -ENOMEM;
		goto file_open_error;
	}

	read_value_32t = sys_read(fd3, (char __user *)bin3, file_size);
	if (read_value_32t != file_size) {
		pr_err("%s: can not read file\n", __func__);
		rc = -ENOMEM;
		goto file_open_error;
	}

	sys_close(fd3);
	fd3 = -1;
	set_fs(old_fs);

	/* OIS program downloading */

	/* Send command ois start dl */
	rc = RegWriteA(OIS_START_DL_ADDR, 0x00);
	cnt = 0;
	while (rc < 0 && cnt < LIMIT_STATUS_POLLING) {
		usleep(2000);
		rc = RegWriteA(OIS_START_DL_ADDR, 0x00);
		cnt++;
	}
	if (cnt == LIMIT_STATUS_POLLING)
		goto program_down_error;

	rc = lgit_ois_program_seq_write(BIN_SRT_ADDR_FOR_DL1,
		BIN_END_ADDR_FOR_DL1,
		CTR_STR_ADDR_FOR_DL1, bin1);
	if (rc < 0) {
		pr_err("%s: program 1 down error\n", __func__);
		goto program_down_error;
	}

	rc = lgit_ois_program_seq_write(BIN_SRT_ADDR_FOR_DL2,
		BIN_END_ADDR_FOR_DL2,
		CTR_STR_ADDR_FOR_DL2, bin2);
	if (rc < 0) {
		pr_err("%s: program 2 down error\n", __func__);
		goto program_down_error;
	}

	rc = lgit_ois_program_seq_write(BIN_SRT_ADDR_FOR_DL3,
		BIN_END_ADDR_FOR_DL3,
		CTR_STR_ADDR_FOR_DL3, bin3);
	if (rc < 0) {
		pr_err("%s: program 3 down error\n", __func__);
		goto program_down_error;
	}

	/* Check sum value!*/
	RamRead32A(0xF008, &read_value_32t);
	if (read_value_32t == sum_value) {
		CDBG("%s: ok to go for download! read_value_32t = 0x%x\n",
			__func__,	read_value_32t);
	} else {
		pr_err("%s: Failed! read_value_32t = 0x%x\n", __func__,
			read_value_32t);
		rc =  -ENOENT;
		goto program_down_error;
	}

	/* Read E2P data!*/
	e2p_cal_data = kzalloc(E2P_DATA_BYTE, GFP_KERNEL);
	if (!e2p_cal_data) {
		pr_err("%s: Can not alloc e2p data\n", __func__);
		rc = -ENOMEM;
		goto program_down_error;
	}

	/* start reading from e2p rom*/
	read_byte = 0;
	for (cnt = 0; cnt < E2P_DATA_BYTE; cnt++) {
		ois_i2c_e2p_read(E2P_FIRST_ADDR + cnt, &data, 1);
		e2p_cal_data[cnt] = data & 0xFF;
		read_byte++;
	}

	CDBG("%s: e2p read_byte = %d\n", __func__, read_byte);

	rc = lgit_ois_program_seq_write(E2P_STR_ADDR_FOR_E2P_DL,
		E2P_END_ADDR_FOR_E2P_DL, CTL_END_ADDR_FOR_E2P_DL, e2p_cal_data);
	if (rc < 0) {
		pr_err("%s: e2p down error\n", __func__);
		goto e2p_down_error;
	}

	/* Send command ois complete dl */
	RegWriteA(OIS_COMPLETE_DL_ADDR, 0x00);
	pr_info("%s, complete dl FINISHED! \n", __func__);

	/* Read ois status */
	lgit_ois_poll_ready(LIMIT_STATUS_POLLING);

	/* final*/
	kfree(bin1);
	kfree(bin2);
	kfree(bin3);
	kfree(e2p_cal_data);
	return rc;

file_open_error:
	if (fd1 >= 0)
		sys_close(fd1);
	if (fd2 >= 0)
		sys_close(fd2);
	if (fd3 >= 0)
		sys_close(fd3);
	set_fs(old_fs);
e2p_down_error:
	if(e2p_cal_data)
		kfree(e2p_cal_data);
program_down_error:
	if(bin1)
		kfree(bin1);
	if(bin2)
		kfree(bin2);
	if(bin3)
		kfree(bin3);
	return rc;
}

int lgit_cal_bin_download(void)
{
	int rc = 0;
	int cnt;
	int fd1 = -1;
	uint32_t file_size;
	uint32_t cur_fd_pos;
	int32_t read_value_32t;
	uint8_t *bin1 = NULL;
	uint8_t *e2p_cal_data = NULL;
	uint8_t buf1[100];
	uint16_t data;
	uint16_t cal_ver = 0;
	mm_segment_t old_fs = get_fs();

	set_fs(KERNEL_DS);

	CDBG("%s\n", __func__);

	ois_i2c_e2p_read(E2P_FIRST_ADDR + 0x1C, &cal_ver, 2);
	++cal_ver;
	pr_info("%s %d.0 \n", __func__, cal_ver);
	snprintf(buf1, sizeof(buf1),
		"/vendor/firmware/bu24205_LGIT_VER_%d_CAL.bin", cal_ver);

	/* First Binary Read */
	fd1 = sys_open(buf1, O_RDONLY, 0);
	if (fd1 < 0) {
		pr_err("%s: -> go to release binary mode \n", __func__);
		rc =  -ENOENT;
		goto file_open_error;
	}

	file_size = (unsigned)sys_lseek(fd1, (off_t)0, 2);
	if (file_size == 0) {
		pr_err("%s: File size is 0\n", __func__);
		rc =  -ENOENT;
		goto file_open_error;
	}

	cur_fd_pos = (unsigned)sys_lseek(fd1, (off_t)0, 0);
	CDBG("%s: firmware cur file pos %u\n", __func__, cur_fd_pos);
	CDBG("%s: firmware file size %d , %u\n", __func__, fd1, file_size);

	bin1 = kzalloc(file_size, GFP_KERNEL);
	if (!bin1) {
		pr_err("%s: Can not alloc bin data\n", __func__);
		rc = -ENOMEM;
		goto file_open_error;
	}

	read_value_32t = sys_read(fd1, (char __user *)bin1, file_size);
	if (read_value_32t != file_size) {
		pr_err("%s: can not read file\n", __func__);
		rc = -ENOMEM;
		goto file_open_error;
	}

	sys_close(fd1);
	set_fs(old_fs);

	/* OIS program downloading */

	/* Send command ois start dl */
	rc = RegWriteA(OIS_START_DL_ADDR, 0x00);
	cnt = 0;
	while (rc < 0 && cnt < LIMIT_STATUS_POLLING) {
		usleep(2000);
		rc = RegWriteA(OIS_START_DL_ADDR, 0x00);
		cnt ++;
	}
	if (cnt == LIMIT_STATUS_POLLING)
		goto program_down_error;

	rc = lgit_ois_program_seq_write(0x5400, 0x54D3, 0x5400, bin1);
	if (rc < 0) {
		pr_err("%s: program 1 down error\n", __func__);
		goto program_down_error;
	}

	rc = lgit_ois_program_seq_write(0x0000, 0x0F73, 0x0000, bin1);
	if (rc < 0) {
		pr_err("%s: program 2 down error\n", __func__);
		goto program_down_error;
	}

	rc = lgit_ois_program_seq_write(0x1188, 0x121F, 0x1188, bin1);
	if (rc < 0) {
		pr_err("%s: program 3 down error\n", __func__);
		goto program_down_error;
	}

	rc = lgit_ois_program_seq_write(0x1600, 0x1A43, 0x1600, bin1);
	if (rc < 0) {
		pr_err("%s: program 3 down error\n", __func__);
		goto program_down_error;
	}

	/* Check sum value!*/
	RamRead32A(0xF008, &read_value_32t);
	//if(read_value_32t == 0x608DB)
	{
		CDBG("%s: ok to go for download! read_value_32t = 0x%x\n", __func__,
			read_value_32t);
	}

	/* Read E2P data!*/
	CDBG("%s: %d %d\n", __func__, sizeof(uint8_t), E2P_DATA_BYTE);

	e2p_cal_data = kzalloc(E2P_DATA_BYTE, GFP_KERNEL);
	if (!e2p_cal_data) {
		pr_err("%s: Can not alloc e2p data\n", __func__);
		rc = -ENOMEM;
		goto program_down_error;
	}

	/* start reading from e2p rom*/
	for (cnt = 0; cnt < E2P_DATA_BYTE; cnt++) {
		ois_i2c_e2p_read(E2P_FIRST_ADDR + cnt, &data, 1);
		e2p_cal_data[cnt] = data & 0xFF;
		CDBG("%s: eeprom %x, %x\n", __func__, E2P_FIRST_ADDR + cnt,
			e2p_cal_data[cnt]);
	}

	rc = lgit_ois_program_seq_write(E2P_STR_ADDR_FOR_E2P_DL,
		E2P_END_ADDR_FOR_E2P_DL, CTL_END_ADDR_FOR_E2P_DL, e2p_cal_data);
	if (rc < 0) {
		pr_err("%s: e2p down error\n", __func__);
		goto e2p_down_error;
	}

	/* Send command ois complete dl */
	RegWriteA(OIS_COMPLETE_DL_ADDR, 0x00) ;
	pr_info("%s, complete dl FINISHED! \n", __func__);

	/* Read ois status */
	lgit_ois_poll_ready(LIMIT_STATUS_POLLING);

	/* final*/
	kfree(bin1);
	kfree(e2p_cal_data);
	return rc;

file_open_error:
	if (fd1 >= 0)
		sys_close(fd1);
	set_fs(old_fs);
e2p_down_error:
	if(e2p_cal_data)
		kfree(e2p_cal_data);
program_down_error:
	if(bin1)
		kfree(bin1);
	return rc;
}

static int lgit_ois_init_cmd(int limit)
{
	int trial = 0;

	do {
		RegWriteA(0x6020, 0x01);
		trial++;
	} while (!lgit_ois_poll_ready(LIMIT_STATUS_POLLING) && trial < limit);

	if (trial == limit)
		return 1;

	return 0;
}

static int lgit_ois_on_cmd(void)
{
	RegWriteA(0x6023, 0x04); /* gyro on */
	if (!lgit_ois_poll_ready(LIMIT_STATUS_POLLING))
		return 1;

	RegWriteA(0x6021, 0x11); /* movie mode, pan on */
	RegWriteA(0x6020, 0x02); /* ois on */
	return 0;
}

static int lgit_ois_mode_thread(void *data)
{
	CDBG("%s: start !\n", __func__);
	RegWriteA(0x6020, 0x01); /* update mode (servo on, ois off) */
	if (!lgit_ois_poll_ready(LIMIT_STATUS_POLLING))
		goto end;

	RegWriteA(0x6021, 0x00); /* still mode, pan-off */
	RegWriteA(0x6020, 0x02); /* ois on */

end:
	CDBG("%s: end !\n", __func__);
	complete_and_exit(0, 0);
}

static int lgit_ois_mode(enum ois_mode_t data)
{
	int mode = data;

	CDBG("%s:%d\n", __func__, data);

	switch (mode) {
	case OIS_MODE_PREVIEW_CAPTURE:
	case OIS_MODE_VIDEO:
		CDBG("%s:%d, %d preview capture \n", __func__, data, mode);
		RegWriteA(0x6020, 0x01);
		if (!lgit_ois_poll_ready(LIMIT_STATUS_POLLING))
			return 0;
		RegWriteA(0x6021, 0x11);
		RegWriteA(0x6020, 0x02);
		break;
	case OIS_MODE_CAPTURE:
		CDBG("%s:%d, %d capture \n", __func__, data, mode);
		kernel_thread(lgit_ois_mode_thread, 0, 0);
		break;
	case OIS_MODE_CENTERING_ONLY:
		CDBG("%s:%d, %d centering_only \n", __func__, data, mode);
		RegWriteA(0x6020, 0x01);
		if (!lgit_ois_poll_ready(LIMIT_STATUS_POLLING))
			return 0;
		break;
	case OIS_MODE_CENTERING_OFF:
		CDBG("%s:%d, %d centering_off \n", __func__, data, mode);
		RegWriteA(0x6020, 0x01);
		if (!lgit_ois_poll_ready(LIMIT_STATUS_POLLING))
			return 0;
		RegWriteA(0x6020, 0x00);
		if (!lgit_ois_poll_ready(LIMIT_STATUS_POLLING))
			return 0;
		break;
	}
	return 0;
}

static void lgit_ois_on(void)
{
	CDBG("%s\n", __func__);
	if (lgit_cal_bin_download()) {
		if (lgit_bin_download())
			return;
	}
	if (lgit_ois_init_cmd(LIMIT_OIS_ON_RETRY))
		return;
	if (lgit_ois_on_cmd())
		return;
	CDBG("%s: complete!\n", __func__);
}

static void lgit_ois_off(void)
{
	CDBG("%s\n", __func__);
	RegWriteA(0x6020, 0x01); /* OIS update mode(lens centering) */
	if (!lgit_ois_poll_ready(LIMIT_STATUS_POLLING))
		return;

	RegWriteA(0x6020, 0x00); /* lens centering off */
	if (!lgit_ois_poll_ready(LIMIT_STATUS_POLLING))
		return;
}

static int lgit_ois_stat(struct msm_sensor_ois_info_t *ois_stat)
{
	uint8_t buf[2];

	snprintf(ois_stat->ois_provider, ARRAY_SIZE(ois_stat->ois_provider),
		"LGIT_ROHM");

	RegWriteA(0x6094, 0x04); /* x target request */

	ois_i2c_read_seq(0x6044, buf, 2);
	CDBG("%s gyrox %x%x \n", __func__, buf[0], buf[1]);
	ois_stat->gyro[0] = (buf[0] << 8) | buf[1];

	ois_i2c_read_seq(0x6096, buf, 2); /* 0->X */
	CDBG("%s targetx %x%x \n", __func__, buf[0], buf[1]);
	/* scaled to match hallx */
	ois_stat->target[0] = 0xFFFF & ((int16_t)((buf[0] << 8) | buf[1]) * 41);

	ois_i2c_read_seq(0x6058, buf, 1);
	CDBG("%s hallx %x \n", __func__, buf[0]);
	/* signed 8bit -> signed 16bit */
	ois_stat->hall[0] = 0xFFFF & (((int8_t)buf[0]) * 256);

	RegWriteA(0x6094, 0x05);  /* y target request */

	ois_i2c_read_seq(0x6042, buf, 2);
	CDBG("%s gyroy %x%x \n", __func__, buf[0], buf[1]);
	ois_stat->gyro[1] = (buf[0] << 8) | buf[1];

	ois_i2c_read_seq(0x6096, buf, 2);
	CDBG("%s targety %x%x \n", __func__, buf[0], buf[1]);
	/* scaled to match hally */
	ois_stat->target[1] = 0xFFFF & ((int16_t)((buf[0] << 8) | buf[1]) * 41);

	ois_i2c_read_seq(0x6059, buf, 1);
	CDBG("%s hally %x \n", __func__, buf[0]);
	ois_stat->hall[1] = 0xFFFF & (((int8_t)buf[0]) * 256);

	ois_stat->is_stable = 1;
	return 0;
}

static struct msm_ois_fn_t lgit_ois_func_tbl = {
	.ois_on = lgit_ois_on,
	.ois_off = lgit_ois_off,
	.ois_mode = lgit_ois_mode,
	.ois_stat = lgit_ois_stat,
};

void lgit_ois_init(struct msm_ois_ctrl_t *msm_ois_t)
{
	msm_ois_t->sid_ois = 0x7C >> 1;
	msm_ois_t->ois_func_tbl = &lgit_ois_func_tbl;
}
