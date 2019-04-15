/* Copyright (c) 2018, The Linux Foundation. All rights reserved.
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

#include <linux/module.h>
#include <linux/firmware.h>
#include <cam_sensor_cmn_header.h>
#include "cam_ois_core.h"
#include "cam_ois_soc.h"
#include "cam_sensor_util.h"
#include "cam_debug_util.h"
#include "cam_res_mgr_api.h"
#include "cam_common_util.h"

static int cam_ois_bm24218_start_dl(struct camera_io_master *io_master_info)
{
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	struct cam_sensor_i2c_reg_array    i2c_reg_array;
	CAM_DBG(CAM_OIS, "cam_ois_bm24218_start_dl, 0xf010=0x00");
	i2c_reg_array.reg_addr = 0xf010;
	i2c_reg_array.reg_data = 0x00;
	i2c_reg_array.data_mask = 0;
	i2c_reg_array.delay = 0;
	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.reg_setting = &i2c_reg_array;
	i2c_reg_setting.size = 1;
	i2c_reg_setting.delay = 0;
	return camera_io_dev_write(io_master_info, &i2c_reg_setting);
}

static int cam_ois_bm24218_stop_dl(struct camera_io_master *io_master_info)
{
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	struct cam_sensor_i2c_reg_array    i2c_reg_array;
	CAM_DBG(CAM_OIS, "cam_ois_bm24218_stop_dl, 0xf006=0x00");
	i2c_reg_array.reg_addr = 0xf006;
	i2c_reg_array.reg_data = 0x00;
	i2c_reg_array.data_mask = 0;
	i2c_reg_array.delay = 0;
	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.reg_setting = &i2c_reg_array;
	i2c_reg_setting.size = 1;
	i2c_reg_setting.delay = 0;
	return camera_io_dev_write(io_master_info, &i2c_reg_setting);
}

static int cam_ois_bm24218_read_checksum(struct camera_io_master *io_master_info)
{
	uint32_t checksum = 0;
	int32_t rc = -EINVAL;
	rc = camera_io_dev_read(io_master_info, 0xf008, &checksum, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_DWORD);
	CAM_ERR(CAM_OIS, "OIS FW CHECKSUM: %x", checksum);
	return rc;
}

static int cam_ois_bm24218_read_reg6024(struct camera_io_master *io_master_info, uint32_t *val)
{
	uint32_t status = 0;
	int32_t rc = -EINVAL;
	rc = camera_io_dev_read(io_master_info, 0x6024, &status, CAMERA_SENSOR_I2C_TYPE_WORD, CAMERA_SENSOR_I2C_TYPE_BYTE);
	CAM_DBG(CAM_OIS, "OIS STATUS: %x", status);
	if (val) *val = status;
	return rc;
}

static uint32_t cam_ois_bm24218_poll_status(struct camera_io_master *io_master_info)
{
	uint32_t cnt = 0;
	uint32_t val = 0;

	do {
		cam_ois_bm24218_read_reg6024(io_master_info, &val);
		if (val) break;
		usleep_range(5000, 6000);
	} while (!val && cnt++ <100);

	return val;
}

static int cam_ois_bm24218_enable_servo_gyro(struct camera_io_master *io_master_info)
{
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	struct cam_sensor_i2c_reg_array    i2c_reg_array[] = {
		{
			.reg_addr = 0x6020,
			.reg_data = 0x01,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x614F,
			.reg_data = 0x01,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x6023,
			.reg_data = 0x02,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x76,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x02,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x44,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x02,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x45,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x58,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x6023,
			.reg_data = 0x00,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x6021,
			.reg_data = 0x7B,
			.data_mask = 0,
			.delay = 0
		},
	};

	int32_t ret = 0, i;
	uint32_t status = 0;

	CAM_DBG(CAM_OIS, "Enable SERV&GYRO");
	for (i=0; i<11; i++) {
		i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		i2c_reg_setting.reg_setting = &i2c_reg_array[i];
		i2c_reg_setting.size = 1;
		i2c_reg_setting.delay = 0;
		CAM_DBG(CAM_OIS, "SERV&GYRO: reg[%04x], %02x", i2c_reg_array[i].reg_addr, i2c_reg_array[i].reg_data);
		if ((i2c_reg_array[i].reg_addr == 0x614F) ||
		    (i2c_reg_array[i].reg_addr == 0x6020 && i2c_reg_array[i].reg_data == 0x01)) {
			CAM_ERR(CAM_OIS, "Poll OIS status before 0x%04x", i2c_reg_array[i].reg_addr);
			status = cam_ois_bm24218_poll_status(io_master_info);
			if (0 == status) {
				CAM_ERR(CAM_OIS, "Poll OIS timeout");
				break;
			}
		}
		ret = camera_io_dev_write(io_master_info, &i2c_reg_setting);
		if (i2c_reg_array[i].reg_addr == 0x602d && i2c_reg_array[i].reg_data == 0x58) {
			usleep_range(25*1000, 30*1000);
			CAM_DBG(CAM_OIS, "Delay for 25ms");
		}
	}

	cam_ois_bm24218_poll_status(io_master_info);

	CAM_ERR(CAM_OIS, "Enable SERV&GYRO Done");
	return ret;
}
static int cam_ois_bm24218tele_enable_servo_gyro(struct camera_io_master *io_master_info)
{
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	struct cam_sensor_i2c_reg_array    i2c_reg_array[] = {
		{
			.reg_addr = 0x6020,
			.reg_data = 0x01,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x6023,
			.reg_data = 0x02,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x11,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x01,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x76,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x01,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x7a,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x02,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x7b,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x02,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x76,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x00,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x4c,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x03,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x13,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x05,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x14,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x1b,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x64,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x60,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x65,
			.data_mask = 0,
			.delay = 0
		},
{
			.reg_addr = 0x602d,
			.reg_data = 0x08,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x4f,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x63,
			.data_mask = 0,
			.delay = 0,
		},
		{
			.reg_addr = 0x602c,
			.reg_data = 0x4e,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x602d,
			.reg_data = 0x0c,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x6023,
			.reg_data = 0x00,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x6021,
			.reg_data = 0x7b,
			.data_mask = 0,
			.delay = 0
		},
		{
			.reg_addr = 0x6020,
			.reg_data = 0x02,
			.data_mask = 0,
			.delay = 0
		},
	};

	int32_t ret = 0, i;
	uint32_t status = 0;

	CAM_DBG(CAM_OIS, "Enable SERV&GYRO");
	for (i=0; i<29; i++) {
		i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
		i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
		i2c_reg_setting.reg_setting = &i2c_reg_array[i];
		i2c_reg_setting.size = 1;
		i2c_reg_setting.delay = 0;
		CAM_DBG(CAM_OIS, "SERV&GYRO: reg[%04x], %02x", i2c_reg_array[i].reg_addr, i2c_reg_array[i].reg_data);
		if ((i2c_reg_array[i].reg_addr == 0x6021 && i2c_reg_array[i].reg_data == 0x7b) ||
		    (i2c_reg_array[i].reg_addr == 0x6020 && i2c_reg_array[i].reg_data == 0x01)) {
			CAM_ERR(CAM_OIS, "Poll OIS status before 0x%04x", i2c_reg_array[i].reg_addr);
			status = cam_ois_bm24218_poll_status(io_master_info);
			if (0 == status) {
				CAM_ERR(CAM_OIS, "Poll OIS timeout");
				break;
			}
		}
		ret = camera_io_dev_write(io_master_info, &i2c_reg_setting);
		if (i2c_reg_array[i].reg_addr == 0x602d && i2c_reg_array[i].reg_data == 0x01) {
			usleep_range(100*1000, 100*1000);
			CAM_DBG(CAM_OIS, "Delay for 100ms");
		}
		if (i2c_reg_array[i].reg_addr == 0x602d && i2c_reg_array[i].reg_data == 0x0c) {
			usleep_range(25*1000, 30*1000);
			CAM_DBG(CAM_OIS, "Delay for 25ms");
		}
	}

	cam_ois_bm24218_poll_status(io_master_info);

	CAM_ERR(CAM_OIS, "Enable SERV&GYRO Done");
	return ret;
}

int cam_ois_bm24218_fw_download(struct cam_ois_ctrl_t *o_ctrl)
{
	uint16_t                           total_bytes = 0;
	uint8_t                           *ptr = NULL;
	int32_t                            rc = 0, cnt;
	uint32_t                           fw_size;
	const struct firmware             *fw = NULL;
	const char                        *fw_name_prog = NULL;
	const char                        *fw_name_coeff = NULL;
	char                               name_prog[32] = {0};
	char                               name_coeff[32] = {0};
	struct device                     *dev = &(o_ctrl->pdev->dev);
	struct cam_sensor_i2c_reg_setting  i2c_reg_setting;
	struct page                       *page = NULL;
	int writing_bytes = 0;

	if (!o_ctrl) {
		CAM_ERR(CAM_OIS, "Invalid Args");
		return -EINVAL;
	}

	snprintf(name_coeff, 32, "%s.coeff", o_ctrl->ois_name);

	snprintf(name_prog, 32, "%s.prog", o_ctrl->ois_name);

	/* cast pointer as const pointer*/
	fw_name_prog = name_prog;
	fw_name_coeff = name_coeff;

	/* Load FW */
	rc = request_firmware(&fw, fw_name_prog, dev);
	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name_prog);
		return rc;
	}

	cam_ois_bm24218_start_dl(&(o_ctrl->io_master_info));
	usleep_range(200, 210);

	total_bytes = fw->size;
	CAM_DBG(CAM_OIS, "OIS:opcode.prog:%x, len:%d", o_ctrl->opcode.prog, total_bytes);
	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.size = total_bytes;
	i2c_reg_setting.delay = 1;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
		total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),
		fw_size, 0, GFP_KERNEL);
	if (!page) {
		CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
		release_firmware(fw);
		return -ENOMEM;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
		page_address(page));

	cnt = 0;
	ptr = (uint8_t *)fw->data;
	do {
		writing_bytes = 0;
		for (; cnt < total_bytes;
			cnt++, ptr++) {
			i2c_reg_setting.reg_setting[writing_bytes].reg_addr =
				o_ctrl->opcode.prog+cnt;
			i2c_reg_setting.reg_setting[writing_bytes].reg_data = *ptr;
			i2c_reg_setting.reg_setting[writing_bytes].delay = 0;
			i2c_reg_setting.reg_setting[writing_bytes].data_mask = 0;
			if (writing_bytes && writing_bytes %200 == 0) {
				break;
			}
			writing_bytes ++;
		}
		i2c_reg_setting.size = writing_bytes;
		CAM_DBG(CAM_OIS, "OIS Writing:%d bytes, cnt:%d", writing_bytes, cnt);
		rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
			&i2c_reg_setting, 1);
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "OIS FW PROG download failed %d", rc);
			goto release_firmware;
		}
	} while (cnt < total_bytes);

	cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),
		page, fw_size);
	page = NULL;
	fw_size = 0;
	release_firmware(fw);

	rc = request_firmware(&fw, fw_name_coeff, dev);
	if (rc) {
		CAM_ERR(CAM_OIS, "Failed to locate %s", fw_name_coeff);
		return rc;
	}

	total_bytes = fw->size;
	CAM_DBG(CAM_OIS, "OIS: opcode.coeff:%x, len:%d", o_ctrl->opcode.coeff, total_bytes);
	i2c_reg_setting.addr_type = CAMERA_SENSOR_I2C_TYPE_WORD;
	i2c_reg_setting.data_type = CAMERA_SENSOR_I2C_TYPE_BYTE;
	i2c_reg_setting.size = total_bytes;
	i2c_reg_setting.delay = 1;
	fw_size = PAGE_ALIGN(sizeof(struct cam_sensor_i2c_reg_array) *
		total_bytes) >> PAGE_SHIFT;
	page = cma_alloc(dev_get_cma_area((o_ctrl->soc_info.dev)),
		fw_size, 0, GFP_KERNEL);
	if (!page) {
		CAM_ERR(CAM_OIS, "Failed in allocating i2c_array");
		release_firmware(fw);
		return -ENOMEM;
	}

	i2c_reg_setting.reg_setting = (struct cam_sensor_i2c_reg_array *) (
		page_address(page));

	cnt = 0;
	ptr = (uint8_t *)fw->data;
	do {
		writing_bytes = 0;
		for (; cnt < total_bytes;
			cnt++, ptr++) {
			i2c_reg_setting.reg_setting[writing_bytes].reg_addr =
				o_ctrl->opcode.coeff+cnt;
			i2c_reg_setting.reg_setting[writing_bytes].reg_data = *ptr;
			i2c_reg_setting.reg_setting[writing_bytes].delay = 0;
			i2c_reg_setting.reg_setting[writing_bytes].data_mask = 0;
			if (writing_bytes && writing_bytes %200 == 0) {
				break;
			}
			writing_bytes ++;
		}
		i2c_reg_setting.size = writing_bytes;
		CAM_DBG(CAM_OIS, "OIS Writing:%d bytes, cnt:%d", writing_bytes, cnt);
		rc = camera_io_dev_write_continuous(&(o_ctrl->io_master_info),
			&i2c_reg_setting, 1);
		if (rc < 0)
			CAM_ERR(CAM_OIS, "OIS FW COEF download failed %d", rc);
	} while (cnt < total_bytes);

	cam_ois_bm24218_read_checksum(&(o_ctrl->io_master_info));

release_firmware:
	cma_release(dev_get_cma_area((o_ctrl->soc_info.dev)),
		page, fw_size);
	release_firmware(fw);

	return rc;
}

int cam_ois_bm24218_after_cal_data_dl(struct cam_ois_ctrl_t *o_ctrl)
{
	int rc = 0;
	rc = cam_ois_bm24218_stop_dl(&(o_ctrl->io_master_info));
	if (rc < 0) {
		CAM_ERR(CAM_OIS, "OIS cam_ois_bm24218_stop_dl failed: %d", rc);
	}
	cam_ois_bm24218_poll_status(&(o_ctrl->io_master_info));
	if (o_ctrl->is_ois_calib &&  !strcmp(o_ctrl->ois_name, "mot_bm24218_tele")) {
		rc = cam_ois_bm24218tele_enable_servo_gyro(&(o_ctrl->io_master_info));
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "OIS cam_ois_bm24218tele_enable_servo_gyro failed: %d", rc);
		}
	} else if (o_ctrl->is_ois_calib &&  !strcmp(o_ctrl->ois_name, "mot_bm24218")) {
		rc = cam_ois_bm24218_enable_servo_gyro(&(o_ctrl->io_master_info));
		if (rc < 0) {
			CAM_ERR(CAM_OIS, "OIS cam_ois_bm24218_enable_servo_gyro failed: %d", rc);
		}
	}

	return rc;
}
