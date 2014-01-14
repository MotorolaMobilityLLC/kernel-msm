/*
 * Synaptics DSX touchscreen driver
 *
 * Copyright (C) 2012 Synaptics Incorporated
 *
 * Copyright (C) 2012 Alexandra Chin <alexandra.chin@tw.synaptics.com>
 * Copyright (C) 2012 Scott Lin <scott.lin@tw.synaptics.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */
#define pr_fmt(fmt) "%s: " fmt, __func__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/firmware.h>
#include <linux/semaphore.h>
#include <linux/wakelock.h>
#include <linux/input/synaptics_rmi_dsx.h>
#include "synaptics_dsx_i2c.h"

#define CHECKSUM_OFFSET 0x00
#define BOOTLOADER_VERSION_OFFSET 0x07
#define IMAGE_SIZE_OFFSET 0x08
#define CONFIG_SIZE_OFFSET 0x0C
#define PRODUCT_ID_OFFSET 0x10
#define PRODUCT_INFO_OFFSET 0x1E
#define FW_IMAGE_OFFSET 0x100

#define UI_CONFIG_AREA 0x00
#define PERM_CONFIG_AREA 0x01
#define BL_CONFIG_AREA 0x02
#define DISP_CONFIG_AREA 0x03

enum flash_command {
	CMD_WRITE_FW_BLOCK		= 0x2,
	CMD_ERASE_ALL			= 0x3,
	CMD_READ_CONFIG_BLOCK	= 0x5,
	CMD_WRITE_CONFIG_BLOCK	= 0x6,
	CMD_ERASE_CONFIG		= 0x7,
	CMD_ERASE_BL_CONFIG		= 0x9,
	CMD_ERASE_DISP_CONFIG	= 0xA,
	CMD_ENABLE_FLASH_PROG	= 0xF,
};

#define SLEEP_MODE_NORMAL (0x00)
#define SLEEP_MODE_SENSOR_SLEEP (0x01)
#define SLEEP_MODE_RESERVED0 (0x02)
#define SLEEP_MODE_RESERVED1 (0x03)

#define ENABLE_WAIT_MS (1 * 1000)
#define WRITE_WAIT_MS (3 * 1000)
#define ERASE_WAIT_MS (5 * 1000)

#define MIN_SLEEP_TIME_US 500
#define MAX_SLEEP_TIME_US 1000

static ssize_t fwu_sysfs_show_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t fwu_sysfs_store_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count);

static ssize_t fwu_sysfs_do_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_write_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_read_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_config_area_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_force_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_image_size_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count);

static ssize_t fwu_sysfs_block_size_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_firmware_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_configuration_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_perm_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_bl_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static ssize_t fwu_sysfs_disp_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf);

static int fwu_wait_for_idle(int timeout_ms);

static int fwu_scan_pdt(void);

/* F34 packet register description */

static struct {
	unsigned char id_letter;
	unsigned char id_digit;
} f34_bootloader_id;

static struct {
	unsigned char bl_minor;
	unsigned char bl_major;
} f34_bootloader_mm;

static struct {
	unsigned char fw_id_0_7;
	unsigned char fw_id_8_15;
	unsigned char fw_id_16_23;
	unsigned char fw_id_24_31;
} f34_firmware_id;

static struct f34_properties flash_properties;

static struct {
	unsigned char blk_size_lsb;
	unsigned char blk_size_msb;
} f34_blk_size;

static struct f34_query3_0 {
	unsigned char fw_blk_count_lsb;
	unsigned char fw_blk_count_msb;
} f34_fw_blk_count;

static struct {
	unsigned char ui_cfg_blk_count_lsb;
	unsigned char ui_cfg_blk_count_msb;
} f34_ui_cfg_blk_count;

static struct {
	unsigned char perm_cfg_blk_count_lsb;
	unsigned char perm_cfg_blk_count_msb;
} f34_perm_cfg_blk_count;

static struct {
	unsigned char bl_cfg_blk_count_lsb;
	unsigned char bl_cfg_blk_count_msb;
} f34_bl_cfg_blk_count;

static struct {
	unsigned char disp_cfg_blk_count_lsb;
	unsigned char disp_cfg_blk_count_msb;
} f34_disp_cfg_blk_count;

static struct {
	unsigned char ui_cfg_blk_count_msb2;
} f34_query_8;

static struct synaptics_rmi4_subpkt f34_query_0_v1[] = {
	RMI4_SUBPKT_STATIC(f34_bootloader_id),
	RMI4_SUBPKT_STATIC(f34_bootloader_mm),
	RMI4_SUBPKT_STATIC(f34_firmware_id),
};

static struct synaptics_rmi4_subpkt f34_query_1_v1[] = {
	RMI4_SUBPKT_STATIC(flash_properties),
};

static struct synaptics_rmi4_subpkt f34_query_2_v1[] = {
	RMI4_SUBPKT_STATIC(f34_blk_size),
};

static struct synaptics_rmi4_subpkt f34_query_3_v1[] = {
	RMI4_SUBPKT_STATIC(f34_fw_blk_count),
	RMI4_SUBPKT_STATIC(f34_ui_cfg_blk_count),
	RMI4_SUBPKT_STATIC(f34_perm_cfg_blk_count),
	RMI4_SUBPKT_STATIC(f34_bl_cfg_blk_count),
	RMI4_SUBPKT_STATIC(f34_disp_cfg_blk_count),
};

static struct synaptics_rmi4_packet_reg f34_query_reg_array_v1[] = {
	RMI4_REG_STATIC(f34_query_0_v1, 0, 8),
	RMI4_REG_STATIC(f34_query_1_v1, 1, 1),
	RMI4_REG_STATIC(f34_query_2_v1, 2, 2),
	RMI4_REG_STATIC(f34_query_3_v1, 3, 10),
};

static struct synaptics_rmi4_func_packet_regs f34_query_regs_v1 = {
	.base_addr = 0,
	.nr_regs = ARRAY_SIZE(f34_query_reg_array_v1),
	.regs = f34_query_reg_array_v1,
};

static struct synaptics_rmi4_subpkt f34_query_0_v0[] = {
	RMI4_SUBPKT_STATIC(f34_bootloader_id),
};

static struct synaptics_rmi4_subpkt f34_query_2_v0[] = {
	RMI4_SUBPKT_STATIC(flash_properties),
};

static struct synaptics_rmi4_subpkt f34_query_3_v0[] = {
	RMI4_SUBPKT_STATIC(f34_blk_size),
};

static struct synaptics_rmi4_subpkt f34_query_5_v0[] = {
	RMI4_SUBPKT_STATIC(f34_fw_blk_count),
};

static struct synaptics_rmi4_subpkt f34_query_7_v0[] = {
	RMI4_SUBPKT_STATIC(f34_ui_cfg_blk_count),
	RMI4_SUBPKT_STATIC(f34_perm_cfg_blk_count),
	RMI4_SUBPKT_STATIC(f34_bl_cfg_blk_count),
	RMI4_SUBPKT_STATIC(f34_disp_cfg_blk_count),
};

static struct synaptics_rmi4_subpkt f34_query_8_v0[] = {
	RMI4_SUBPKT_STATIC(f34_query_8),
};

static struct synaptics_rmi4_packet_reg f34_query_reg_array_v0[] = {
	RMI4_REG_STATIC(f34_query_0_v0, 0, 2), /* bootloader id */
	RMI4_REG_STATIC(f34_query_2_v0, 2, 1), /* flash properties */
	RMI4_REG_STATIC(f34_query_3_v0, 3, 2), /* block size */
	RMI4_REG_STATIC(f34_query_5_v0, 5, 2), /* firmware block count */
	RMI4_REG_STATIC(f34_query_7_v0, 7, 8),
	RMI4_REG_STATIC(f34_query_8_v0, 8, 1),
};

static struct synaptics_rmi4_func_packet_regs f34_query_regs_v0 = {
	.base_addr = 0,
	.nr_regs = ARRAY_SIZE(f34_query_reg_array_v0),
	.regs = f34_query_reg_array_v0,
};

struct image_header {
	unsigned int checksum;
	unsigned int image_size;
	unsigned int config_size;
	unsigned char options;
	unsigned char bootloader_version;
	unsigned char product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE + 1];
	unsigned char product_info[SYNAPTICS_RMI4_PRODUCT_INFO_SIZE];
};

struct pdt_properties {
	union {
		struct {
			unsigned char reserved_1:6;
			unsigned char has_bsr:1;
			unsigned char reserved_2:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f01_device_status {
	union {
		struct {
			unsigned char status_code:4;
			unsigned char reserved:2;
			unsigned char flash_prog:1;
			unsigned char unconfigured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct f01_device_control {
	union {
		struct {
			unsigned char sleep_mode:2;
			unsigned char nosleep:1;
			unsigned char reserved:2;
			unsigned char charger_connected:1;
			unsigned char report_rate:1;
			unsigned char configured:1;
		} __packed;
		unsigned char data[1];
	};
};

struct synaptics_rmi4_fwu_handle {
	bool initialized;
	bool irq_enabled;
	char fw_filename[SYNAPTICS_RMI4_FILENAME_SIZE];
	unsigned int firmware_id;
	unsigned int config_id;
	unsigned int image_size;
	unsigned int data_pos;
	unsigned char intr_mask;
	unsigned char bootloader_id[2];
	unsigned char *ext_data_source;
	unsigned char *read_config_buf;
	const unsigned char *firmware_data;
	const unsigned char *config_data;
	unsigned short block_size;
	unsigned short fw_block_count;
	unsigned short config_block_count;
	unsigned short perm_config_block_count;
	unsigned short bl_config_block_count;
	unsigned short disp_config_block_count;
	unsigned short config_size;
	unsigned short config_area;
	unsigned short f34_blkdata_addr;
	unsigned short f34_flash_cmd_addr;
	unsigned short f34_flash_status_addr;
	struct synaptics_rmi4_fn_desc f01_fd;
	struct synaptics_rmi4_fn_desc f34_fd;
	struct synaptics_rmi4_exp_fn_ptr *fn_ptr;
	struct synaptics_rmi4_data *rmi4_data;
	struct semaphore sema;
	struct wake_lock flash_wake_lock;
};

static struct bin_attribute dev_attr_data = {
	.attr = {
		.name = "data",
		.mode = (S_IRUGO | S_IWUSR | S_IWGRP),
	},
	.size = 0,
	.read = fwu_sysfs_show_image,
	.write = fwu_sysfs_store_image,
};

static struct device_attribute attrs[] = {
	__ATTR(doreflash, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_do_reflash_store),
	__ATTR(writeconfig, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_write_config_store),
	__ATTR(readconfig, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_read_config_store),
	__ATTR(configarea, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_config_area_store),
	__ATTR(imagesize, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_image_size_store),
	__ATTR(blocksize, S_IRUSR | S_IRGRP,
			fwu_sysfs_block_size_show,
			synaptics_rmi4_store_error),
	__ATTR(forcereflash, S_IWUSR | S_IWGRP,
			synaptics_rmi4_show_error,
			fwu_sysfs_force_reflash_store),
	__ATTR(fwblockcount, S_IRUSR | S_IRGRP,
			fwu_sysfs_firmware_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(configblockcount, S_IRUSR | S_IRGRP,
			fwu_sysfs_configuration_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(permconfigblockcount, S_IRUSR | S_IRGRP,
			fwu_sysfs_perm_config_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(blconfigblockcount, S_IRUSR | S_IRGRP,
			fwu_sysfs_bl_config_block_count_show,
			synaptics_rmi4_store_error),
	__ATTR(dispconfigblockcount, S_IRUSR | S_IRGRP,
			fwu_sysfs_disp_config_block_count_show,
			synaptics_rmi4_store_error),
};

static struct synaptics_rmi4_fwu_handle *fwu;
static bool force_reflash;
static struct completion remove_complete;

static unsigned int extract_uint(const unsigned char *ptr)
{
	return (unsigned int)ptr[0] +
			(unsigned int)ptr[1] * 0x100 +
			(unsigned int)ptr[2] * 0x10000 +
			(unsigned int)ptr[3] * 0x1000000;
}

static void parse_header(struct image_header *header,
		const unsigned char *fw_image)
{
	header->checksum = extract_uint(&fw_image[CHECKSUM_OFFSET]);
	header->bootloader_version = fw_image[BOOTLOADER_VERSION_OFFSET];
	header->image_size = extract_uint(&fw_image[IMAGE_SIZE_OFFSET]);
	header->config_size = extract_uint(&fw_image[CONFIG_SIZE_OFFSET]);
	memcpy(header->product_id, &fw_image[PRODUCT_ID_OFFSET],
			SYNAPTICS_RMI4_PRODUCT_ID_SIZE);
	header->product_id[SYNAPTICS_RMI4_PRODUCT_ID_SIZE] = 0;
	memcpy(header->product_info, &fw_image[PRODUCT_INFO_OFFSET],
			SYNAPTICS_RMI4_PRODUCT_INFO_SIZE);

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
		"Firwmare size %d, config size %d\n",
		header->image_size,
		header->config_size);

	return;
}

static unsigned int fwu_firmware_id(void)
{
	unsigned int firmware_id;
	struct synaptics_rmi4_device_info *rmi;
	rmi = &(fwu->rmi4_data->rmi4_mod_info);
	batohui(&firmware_id, rmi->build_id, sizeof(rmi->build_id));
	return firmware_id;
}

static int fwu_read_f01_device_status(struct f01_device_status *status)
{
	return fwu->fn_ptr->read(fwu->rmi4_data,
			fwu->f01_fd.data_base_addr,
			status->data,
			sizeof(status->data));
}

static struct synaptics_rmi4_func_packet_regs
			*fwu_f34_packet_regs_addr(void)
{
	return fwu->bootloader_id[1] == '5' ?
		&f34_query_regs_v0 : &f34_query_regs_v1;
}

static int fwu_f34_read_query_regs(void)
{
	int retval = -ENODATA;
	struct synaptics_rmi4_func_packet_regs *f34_regs;

	f34_regs = fwu_f34_packet_regs_addr();
	f34_regs->base_addr = fwu->f34_fd.query_base_addr;
	retval = synaptics_rmi4_read_packet_regs(fwu->rmi4_data, f34_regs);
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to query F34 registers: rc=%d\n",
				__func__, retval);
		return retval;
	}

	return retval;
}

static int fwu_read_f34_queries(void)
{
	int retval;
	struct i2c_client *i2c_client = fwu->rmi4_data->i2c_client;

	/* read BL id to determine F34 version */
	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu->f34_fd.query_base_addr,
			fwu->bootloader_id,
			sizeof(fwu->bootloader_id));
	if (retval < 0) {
		dev_err(&i2c_client->dev,
				"%s: Failed to read bootloader ID\n",
				__func__);
		return retval;
	}

	retval = fwu_f34_read_query_regs();
	if (retval < 0) {
		dev_err(&i2c_client->dev,
				"%s: Failed to read query regs\n",
				__func__);
		return retval;
	}

	dev_dbg(&i2c_client->dev,
		"%s perm:%d, bl:%d, display:%d\n",
		__func__,
		flash_properties.has_perm_config,
		flash_properties.has_bl_config,
		flash_properties.has_display_config);

	batohs(&fwu->block_size, (unsigned char *)&f34_blk_size);
	batohs(&fwu->fw_block_count, (unsigned char *)&f34_fw_blk_count);
	batohs(&fwu->config_block_count,
				(unsigned char *)&f34_ui_cfg_blk_count);
	batohs(&fwu->perm_config_block_count,
				(unsigned char *)&f34_perm_cfg_blk_count);
	batohs(&fwu->bl_config_block_count,
				(unsigned char *)&f34_bl_cfg_blk_count);
	batohs(&fwu->disp_config_block_count,
				(unsigned char *)&f34_disp_cfg_blk_count);

	if (flash_properties.has_config_id) {
		struct synaptics_rmi4_device_info *rmi;
		rmi = &(fwu->rmi4_data->rmi4_mod_info);
		retval = fwu->fn_ptr->read(fwu->rmi4_data,
				fwu->f34_fd.ctrl_base_addr,
				rmi->config_id,
				SYNAPTICS_RMI4_CONFIG_ID_SIZE);
		if (retval < 0) {
			dev_err(&i2c_client->dev,
				"Failed to read config ID (code %d).\n",
				retval);
			return retval;
		}
	}

	/* fill in version dependent F34 data registers addresses */
	if (fwu->bootloader_id[1] == '5') {
		fwu->f34_blkdata_addr = fwu->f34_fd.data_base_addr + 2;
		fwu->f34_flash_cmd_addr = fwu->f34_fd.data_base_addr +
							fwu->block_size + 2;
		fwu->f34_flash_status_addr = fwu->f34_flash_cmd_addr;
	} else {
		fwu->f34_blkdata_addr = fwu->f34_fd.data_base_addr + 1;
		fwu->f34_flash_cmd_addr = fwu->f34_fd.data_base_addr + 2;
		fwu->f34_flash_status_addr = fwu->f34_fd.data_base_addr + 3;
	}

	return 0;
}

static int fwu_read_interrupt_status(void)
{
	int retval;
	unsigned char interrupt_status;
	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu->f01_fd.data_base_addr + 1,
			&interrupt_status,
			sizeof(interrupt_status));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to read intr status\n",
				__func__);
		return retval;
	}
	return interrupt_status;
}

static int fwu_read_f34_flash_status(unsigned char *status,
					bool program_enabled)
{
	int retval;
	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu->f34_flash_status_addr,
			status,
			sizeof(*status));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to read flash status\n",
				__func__);
		return retval;
	}
	if (program_enabled)
		*status &= 0x80;
	else
		*status &= 0x3F;
	return 0;
}

static int fwu_reset_device(void)
{
	int retval;

	retval = fwu->rmi4_data->reset_device(fwu->rmi4_data,
				&fwu->f01_fd.cmd_base_addr);
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to reset core driver after reflash\n",
				__func__);
		goto exit;
	}

	retval = fwu_scan_pdt();
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to scan PDT after reflash\n",
				__func__);
		goto exit;
	}

	retval = fwu_read_f34_queries();
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to query F34 after reflash\n",
				__func__);
		goto exit;
	}
exit:
	return retval;
}

static int fwu_write_f34_command(unsigned char cmd)
{
	int retval;

	retval = fwu->fn_ptr->write(fwu->rmi4_data,
			fwu->f34_flash_cmd_addr,
			&cmd,
			sizeof(cmd));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write command 0x%02x\n",
				__func__, cmd);
		return retval;
	}
	return 0;
}

static irqreturn_t fwu_irq(int irq, void *data)
{
	struct synaptics_rmi4_fwu_handle *fwu_ptr = data;
	up(&fwu_ptr->sema);
	return IRQ_HANDLED;
}

static int fwu_wait_for_idle(int timeout_ms)
{
	int retval;

	retval = down_timeout(&fwu->sema, msecs_to_jiffies(timeout_ms));
	if (retval) {
		retval = -ETIMEDOUT;
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"timed out waiting for cmd to complete\n");
	} else
		retval = fwu_read_interrupt_status();
	return retval;
}

static int fwu_scan_pdt(void)
{
	int retval;
	unsigned char ii;
	unsigned char intr_count = 0;
	unsigned char intr_off;
	unsigned char intr_src;
	unsigned short addr;
	bool f01found = false;
	bool f34found = false;
	struct synaptics_rmi4_fn_desc rmi_fd;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev, "Scan PDT\n");

	for (addr = PDT_START; addr > PDT_END; addr -= PDT_ENTRY_SIZE) {
		retval = fwu->fn_ptr->read(fwu->rmi4_data,
					addr,
					(unsigned char *)&rmi_fd,
					sizeof(rmi_fd));
		if (retval < 0)
			return retval;

		if (rmi_fd.fn_number) {
			dev_dbg(&fwu->rmi4_data->i2c_client->dev,
					"%s: Found F%02x\n",
					__func__, rmi_fd.fn_number);
			switch (rmi_fd.fn_number) {
			case SYNAPTICS_RMI4_F01:
				f01found = true;
				fwu->f01_fd = rmi_fd;
				break;
			case SYNAPTICS_RMI4_F34:
				f34found = true;
				fwu->f34_fd = rmi_fd;
				fwu->intr_mask = 0;
				intr_src = rmi_fd.intr_src_count;
				intr_off = intr_count % 8;
				for (ii = intr_off;
						ii < ((intr_src & MASK_3BIT) +
						intr_off);
						ii++)
					fwu->intr_mask |= 1 << ii;
				break;
			}
		} else
			break;

		if (rmi_fd.intr_src_count & 0x60)
			dev_dbg(&fwu->rmi4_data->i2c_client->dev,
				"%s: F%02x alternative version!!!\n",
				__func__,
				rmi_fd.fn_number);

		intr_count += (rmi_fd.intr_src_count & MASK_3BIT);
	}

	if (!f01found || !f34found) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to find both F01 and F34\n",
				__func__);
		return -EINVAL;
	}

	fwu_read_interrupt_status();
	return 0;
}

static int fwu_write_blocks(unsigned char *block_ptr, unsigned short block_cnt,
		unsigned char command)
{
	int retval;
	unsigned char block_offset[] = {0, 0};
	unsigned short block_num;
	unsigned int progress = (command == CMD_WRITE_CONFIG_BLOCK) ?
				10 : 100;
	block_offset[1] |= (UI_CONFIG_AREA << 5);
	retval = fwu->fn_ptr->write(fwu->rmi4_data,
			fwu->f34_fd.data_base_addr,
			block_offset,
			sizeof(block_offset));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write to block number registers\n",
				__func__);
		return retval;
	}

	for (block_num = 0; block_num < block_cnt; block_num++) {
		unsigned char flash_status = 0;
		if (block_num % progress == 0)
			dev_dbg(&fwu->rmi4_data->i2c_client->dev,
				"%s: update %s %3d / %3d\n",
				__func__,
				command == CMD_WRITE_CONFIG_BLOCK ?
				"config" : "firmware",
				block_num,
				block_cnt);

		retval = fwu->fn_ptr->write(fwu->rmi4_data,
			fwu->f34_blkdata_addr,
			block_ptr,
			fwu->block_size);
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write block data (block %d)\n",
				__func__, block_num);
			return retval;
		}

		retval = fwu_write_f34_command(command);
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
					"%s: Failed to write command for block %d\n",
					__func__, block_num);
			return retval;
		}

		retval = fwu_wait_for_idle(WRITE_WAIT_MS);
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to wait for idle status \
				(block %d)\n",
				__func__, block_num);
			return retval;
		}

		fwu_read_f34_flash_status(&flash_status, false);
		if (flash_status) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Flash block %d status %x\n",
				__func__,
				block_num,
				flash_status);
			return -EAGAIN;
		}
		block_ptr += fwu->block_size;
	}

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
		"%s: update %s %3d / %3d\n",
		__func__,
		command == CMD_WRITE_CONFIG_BLOCK ?
		"config" : "firmware",
		block_cnt,
		block_cnt);

	return 0;
}

static int fwu_write_firmware(void)
{
	return fwu_write_blocks((unsigned char *)fwu->firmware_data,
		fwu->fw_block_count, CMD_WRITE_FW_BLOCK);
}

static int fwu_write_configuration(void)
{
	return fwu_write_blocks((unsigned char *)fwu->config_data,
		fwu->config_block_count, CMD_WRITE_CONFIG_BLOCK);
}

static int fwu_write_bootloader_id(void)
{
	int retval;

	retval = fwu->fn_ptr->read(fwu->rmi4_data,
			fwu->f34_fd.query_base_addr,
			fwu->bootloader_id,
			sizeof(fwu->bootloader_id));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to read bootloader ID\n",
				__func__);
		return retval;
	}

	dev_dbg(&fwu->rmi4_data->i2c_client->dev, "Write bootloader ID: %c%c\n",
			fwu->bootloader_id[0], fwu->bootloader_id[1]);

	retval = fwu->fn_ptr->write(fwu->rmi4_data,
			fwu->f34_blkdata_addr,
			fwu->bootloader_id,
			sizeof(fwu->bootloader_id));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write bootloader ID\n",
				__func__);
		return retval;
	}

	return 0;
}

static void fwu_enable_irq(bool enable)
{
	int retval;

	if (enable) {
		if (fwu->irq_enabled)
			return;

		fwu_read_interrupt_status();
		sema_init(&fwu->sema, 0);

		retval = request_irq(fwu->rmi4_data->irq, fwu_irq,
				IRQF_TRIGGER_FALLING, "fwu", fwu);
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
					"%s: Failed to request irq: %d\n",
					__func__, retval);
		}

		dev_dbg(&fwu->rmi4_data->i2c_client->dev,
					"enabling F34 IRQ handler\n");
		fwu->irq_enabled = true;
	} else {
		if (!fwu->irq_enabled)
			return;

		dev_dbg(&fwu->rmi4_data->i2c_client->dev,
					"disabling F34 IRQ handler\n");
		disable_irq(fwu->rmi4_data->irq);
		free_irq(fwu->rmi4_data->irq, fwu);
		fwu->irq_enabled = false;
	}
}

static int fwu_enter_flash_prog(void)
{
	int retval;
	struct f01_device_status f01_device_status;
	unsigned char program_enabled = 0;

	retval = fwu_read_f01_device_status(&f01_device_status);
	if (retval < 0)
		return retval;

	if (f01_device_status.flash_prog) {
		dev_info(&fwu->rmi4_data->i2c_client->dev,
				"%s: Already in flash prog mode\n",
				__func__);
		return 0;
	} else {
		unsigned int config_id;
		struct synaptics_rmi4_device_info *rmi;

		rmi = &(fwu->rmi4_data->rmi4_mod_info);
		batohui(&config_id, rmi->config_id, sizeof(rmi->config_id));

		/* Firmware ID stops changing at some point, thus  */
		/* config ID is the only id that guaranteed to grow */
		if (fwu->config_id <= config_id) {
			/* do not allow downgrade firmware */
			/* unless specifically instructed to */
			if (!force_reflash)
				return -EEXIST;

			force_reflash = false;
			pr_notice("%s: Reflash enforced\n", __func__);
		}

		dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Firmware IDs: currently running-%x, in image-%x\n",
			__func__,
			fwu_firmware_id(), fwu->firmware_id);

		dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Config IDs: currently running-%x, in image-%x\n",
			__func__,
			config_id, fwu->config_id);
	}

	dev_dbg(&fwu->rmi4_data->i2c_client->dev, "Enter bootloader mode\n");

	retval = fwu_write_bootloader_id();
	if (retval < 0)
		return retval;

	retval = fwu_write_f34_command(CMD_ENABLE_FLASH_PROG);
	if (retval < 0)
		return retval;

	retval = fwu_wait_for_idle(ENABLE_WAIT_MS);

	if (retval < 0)
		return retval;

	fwu_read_f34_flash_status(&program_enabled, true);
	if (!program_enabled) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
			"%s: Program enabled bit not set\n",
			__func__);
		return -EINVAL;
	}

	retval = fwu_scan_pdt();
	if (retval < 0)
		return retval;

	retval = fwu_read_f34_queries();
	if (retval < 0)
		return retval;

	dev_info(&fwu->rmi4_data->i2c_client->dev,
				"Device firmware ID %x\n",
				fwu_firmware_id());

	retval = fwu_read_f01_device_status(&f01_device_status);
	if (retval < 0)
		return retval;

	if (!f01_device_status.flash_prog) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Not in flash prog mode\n",
				__func__);
		return -EINVAL;
	}

	return retval;
}

static int fwu_handle_reflash_error(void)
{
	int retval, count = 0;
	int timeout_count = 1000;
	const struct synaptics_dsx_platform_data *platform_data =
				fwu->rmi4_data->board;
	fwu_enable_irq(false);

	do {
		if (gpio_get_value(platform_data->irq_gpio))
			break;

		usleep_range(MIN_SLEEP_TIME_US, MAX_SLEEP_TIME_US);
		count++;
	} while (count < timeout_count);

	if (count == timeout_count) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
			"%s: Timed out waiting for idle status\n",
			__func__);
		return -ETIMEDOUT;
	}

	fwu_read_interrupt_status();

	retval = fwu_scan_pdt();
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
			"%s: Failed to scan PDT\n",
			__func__);
		return -EINVAL;
	}

	retval = fwu_read_f34_queries();
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
			"%s: Failed to query F34\n",
			__func__);
		return -EINVAL;
	}

	fwu_enable_irq(true);

	return 0;
}

static int fwu_do_reflash(void)
{
	int retval;
	unsigned char program_enabled = 0;

	fwu_enable_irq(true);

	retval = fwu_enter_flash_prog();
	if (retval < 0)
		goto reflash_failed;

retry:
	fwu->rmi4_data->set_state(fwu->rmi4_data, STATE_INIT);

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	retval = fwu_write_bootloader_id();
	if (retval < 0)
		goto reflash_failed;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Bootloader ID written\n",
			__func__);

	retval = fwu_write_f34_command(CMD_ERASE_ALL);
	if (retval < 0)
		goto reflash_failed;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Erase all command written\n",
			__func__);

	retval = fwu_wait_for_idle(ERASE_WAIT_MS);
	if (retval < 0)
		goto reflash_failed;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

	fwu_read_f34_flash_status(&program_enabled, true);
	if (!program_enabled) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
			"%s: Program enabled bit not set\n",
			__func__);
		retval = -EINVAL;
		goto reflash_failed;
	}

	fwu->rmi4_data->set_state(fwu->rmi4_data, STATE_FLASH);

	if (fwu->firmware_data) {
		retval = fwu_write_firmware();
		if (retval == -EAGAIN) {
			retval = fwu_handle_reflash_error();
			if (!retval) {
				dev_dbg(&fwu->rmi4_data->i2c_client->dev,
						"recovered from flash error\n");
				goto retry;
			}
		}
		if (retval < 0)
			goto reflash_failed;
		pr_notice("%s: Firmware programmed\n", __func__);
	}

	if (fwu->config_data) {
		retval = fwu_write_configuration();
		if (retval == -EAGAIN) {
			retval = fwu_handle_reflash_error();
			if (!retval) {
				dev_dbg(&fwu->rmi4_data->i2c_client->dev,
						"recovered from flash error\n");
				goto retry;
			}
		}
		if (retval < 0)
			goto reflash_failed;
		pr_notice("%s: Configuration programmed\n", __func__);
	}


reflash_failed:
	fwu_enable_irq(false);

	return retval;
}

static bool fwu_tdat_image_format(const unsigned char *fw_image)
{
	return fw_image[0] == 0x31;
}

static void fwu_tdat_config_set(const unsigned char *data, size_t size,
		const unsigned char **image, size_t *image_size)
{
	unsigned short id;
	size_t length, offset;

	for (offset = 0; offset < size; offset += length+5) {
		id = (data[offset+1] << 8) | data[offset];
		length = (data[offset+4] << 8) | data[offset+3];
		if (id == 0x0001) {
			*image = &data[offset+5];
			*image_size = length;
		}
	}
}

static void fwu_tdat_section_offset(
		const unsigned char **image, size_t *image_size)
{
	size_t offset;
	offset = (*image)[0] + 1;
	*image_size -= offset;
	*image = &(*image)[offset];
}

static int fwu_parse_tdat_image(struct image_header *header,
		const unsigned char *fw_image, size_t fw_size)
{
	int ii;
	unsigned int id;
	size_t length, offset;
	const unsigned char *section, *data = fw_image;

	pr_notice("%s: Start TDAT image processing\n", __func__);

	for (ii = 0, offset = 1; offset < fw_size; offset += length+4) {
		length = (data[offset+3] << 16) |
			 (data[offset+2] << 8) | data[offset+1];

		dev_dbg(&fwu->rmi4_data->i2c_client->dev,
				"Record[%d]: length %u, offset %u\n",
				ii++, length, offset);

		if ((offset+length+4) > fw_size) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
					"Data overflow at offset %u (%u)\n",
					offset, data[offset]);
			return -EINVAL;
		}
	}

	if (offset != fw_size) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"Data is misaligned\n");
		return -EINVAL;
	}

	for (offset = 1; offset < fw_size; offset += length+4) {
		id = data[offset];
		length = (data[offset+3] << 16) |
			 (data[offset+2] << 8) | data[offset+1];
		section = &data[offset+4];

		switch (id) {
		case 1: /* config */
			dev_dbg(&fwu->rmi4_data->i2c_client->dev,
				"%s: Config record %d, size %d\n",
				__func__, id, length);

			fwu_tdat_config_set(section, length,
					&fwu->config_data,
					&header->config_size);
			fwu_tdat_section_offset(&fwu->config_data,
						&header->config_size);
			batohui(&fwu->config_id,
					(unsigned char *)fwu->config_data,
					SYNAPTICS_RMI4_CONFIG_ID_SIZE);
			break;

		case 2: /* firmware */
			dev_dbg(&fwu->rmi4_data->i2c_client->dev,
				"%s: Firmware record %d, size %d\n",
				__func__,
				id,
				length);

			batohui(&fwu->firmware_id,
					(unsigned char *)&section[1],
					SYNAPTICS_RMI4_BUILD_ID_SIZE);

			dev_dbg(&fwu->rmi4_data->i2c_client->dev,
				"%s: Firmware build ID %x\n",
				__func__,
				fwu->firmware_id);

			fwu->firmware_data = section;
			header->image_size = length;
			fwu_tdat_section_offset(&fwu->firmware_data,
						&header->image_size);
			break;
		default:
			dev_dbg(&fwu->rmi4_data->i2c_client->dev,
				"%s: Don't care section id %d\n",
				__func__,
				id);
			break;
		}
	}


	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
		"%s: Firwmare size %d, config size %d\n",
		__func__,
		header->image_size,
		header->config_size);
	return 0;
}

static int fwu_start_reflash(void)
{
	int retval;
	size_t fw_size = 0;
	struct image_header header;
	const unsigned char *fw_image;
	const struct firmware *fw_entry = NULL;
	struct f01_device_status f01_device_status;
	struct i2c_client *i2c_client = fwu->rmi4_data->i2c_client;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(fwu->rmi4_data->rmi4_mod_info);

	pr_notice("%s: Start of reflash process\n", __func__);

	if (fwu->ext_data_source)
		fw_image = fwu->ext_data_source;
	else {
		dev_dbg(&i2c_client->dev,
				"%s: Requesting firmware image %s\n",
				__func__, fwu->fw_filename);

		retval = request_firmware(&fw_entry, fwu->fw_filename,
				&fwu->rmi4_data->i2c_client->dev);
		if (retval != 0) {
			dev_err(&i2c_client->dev,
					"%s: Firmware image %s not available\n",
					__func__, fwu->fw_filename);
			retval = -EINVAL;
			goto exit;
		}

		dev_dbg(&i2c_client->dev,
				"%s: Firmware image size = %d\n",
				__func__, fw_entry->size);

		fw_image = fw_entry->data;
		fw_size = fw_entry->size;
	}

	if (fwu_tdat_image_format(fw_image))
		fwu_parse_tdat_image(&header, fw_image, fw_size);
	else {
		parse_header(&header, fw_image);

		pr_notice("Firmware file has BL version %d\n",
					header.bootloader_version);
		if (header.image_size)
			fwu->firmware_data = fw_image + FW_IMAGE_OFFSET;
		if (header.config_size)
			fwu->config_data = fw_image + FW_IMAGE_OFFSET +
					header.image_size;
	}

	wake_lock(&fwu->flash_wake_lock);
	fwu->rmi4_data->irq_enable(fwu->rmi4_data, false);

	dev_dbg(&i2c_client->dev,
			"Device firmware ID %x\n",
			fwu_firmware_id());
	dev_dbg(&i2c_client->dev,
			"Device config ID 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
			rmi->config_id[0],
			rmi->config_id[1],
			rmi->config_id[2],
			rmi->config_id[3]);
	/* .img config id */
	dev_dbg(&i2c_client->dev,
			".img config ID 0x%02X, 0x%02X, 0x%02X, 0x%02X\n",
			fwu->config_data[0],
			fwu->config_data[1],
			fwu->config_data[2],
			fwu->config_data[3]);

	retval = fwu_do_reflash();
	switch (retval) {
	case 0:
		break;
	case -EEXIST:
		dev_info(&i2c_client->dev,
			"%s: Current firmware is up to date\n",
			__func__);
		retval = 0;
	default:
		if (retval < 0)
			dev_err(&i2c_client->dev,
				"%s: Failed to do reflash\n",
				__func__);
		fwu->rmi4_data->irq_enable(fwu->rmi4_data, true);
		goto unlock;
	}

	fwu->rmi4_data->set_state(fwu->rmi4_data, STATE_UNKNOWN);
	/* reset device */
	fwu_reset_device();
	fwu->rmi4_data->ready_state(fwu->rmi4_data, false);

unlock:
	/* check device status */
	retval = fwu_read_f01_device_status(&f01_device_status);
	if (!retval)
		dev_info(&i2c_client->dev,
			"Device is in %s mode\n",
			f01_device_status.flash_prog ? "bootloader" : "UI");

	release_firmware(fw_entry);
	wake_unlock(&fwu->flash_wake_lock);
exit:
	return retval;
}

static int fwu_do_write_config(void)
{
	int retval;

	retval = fwu_enter_flash_prog();
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	if (fwu->config_area == PERM_CONFIG_AREA) {
		fwu->config_block_count = fwu->perm_config_block_count;
		goto write_config;
	}

	retval = fwu_write_bootloader_id();
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Bootloader ID written\n",
			__func__);

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		retval = fwu_write_f34_command(CMD_ERASE_CONFIG);
		break;
	case BL_CONFIG_AREA:
		retval = fwu_write_f34_command(CMD_ERASE_BL_CONFIG);
		fwu->config_block_count = fwu->bl_config_block_count;
		break;
	case DISP_CONFIG_AREA:
		retval = fwu_write_f34_command(CMD_ERASE_DISP_CONFIG);
		fwu->config_block_count = fwu->disp_config_block_count;
		break;
	}
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Erase command written\n",
			__func__);

	retval = fwu_wait_for_idle(ERASE_WAIT_MS);
	if (retval < 0)
		return retval;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Idle status detected\n",
			__func__);

write_config:
	retval = fwu_write_configuration();
	if (retval < 0)
		return retval;

	pr_notice("%s: Config written\n", __func__);

	return retval;
}

static int fwu_start_write_config(void)
{
	int retval;
	struct image_header header;

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		break;
	case PERM_CONFIG_AREA:
		if (!flash_properties.has_perm_config)
			return -EINVAL;
		break;
	case BL_CONFIG_AREA:
		if (!flash_properties.has_bl_config)
			return -EINVAL;
		break;
	case DISP_CONFIG_AREA:
		if (!flash_properties.has_display_config)
			return -EINVAL;
		break;
	default:
		return -EINVAL;
	}

	if (fwu->ext_data_source)
		fwu->config_data = fwu->ext_data_source;
	else
		return -EINVAL;

	if (fwu->config_area == UI_CONFIG_AREA) {
		parse_header(&header, fwu->ext_data_source);

		if (header.config_size) {
			fwu->config_data = fwu->ext_data_source +
					FW_IMAGE_OFFSET +
					header.image_size;
		} else {
			return -EINVAL;
		}
	}

	pr_notice("%s: Start of write config process\n", __func__);

	retval = fwu_do_write_config();
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write config\n",
				__func__);
	}

	fwu->rmi4_data->reset_device(fwu->rmi4_data,
				&fwu->f01_fd.cmd_base_addr);

	pr_notice("%s: End of write config process\n", __func__);

	return retval;
}

static int fwu_do_read_config(void)
{
	int retval;
	unsigned char block_offset[] = {0, 0};
	unsigned short block_num;
	unsigned short block_count;
	unsigned short index = 0;

	retval = fwu_enter_flash_prog();
	if (retval < 0)
		goto exit;

	dev_dbg(&fwu->rmi4_data->i2c_client->dev,
			"%s: Entered flash prog mode\n",
			__func__);

	switch (fwu->config_area) {
	case UI_CONFIG_AREA:
		block_count = fwu->config_block_count;
		break;
	case PERM_CONFIG_AREA:
		if (!flash_properties.has_perm_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->perm_config_block_count;
		break;
	case BL_CONFIG_AREA:
		if (!flash_properties.has_bl_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->bl_config_block_count;
		break;
	case DISP_CONFIG_AREA:
		if (!flash_properties.has_display_config) {
			retval = -EINVAL;
			goto exit;
		}
		block_count = fwu->disp_config_block_count;
		break;
	default:
		retval = -EINVAL;
		goto exit;
	}

	fwu->config_size = fwu->block_size * block_count;

	kfree(fwu->read_config_buf);
	fwu->read_config_buf = kzalloc(fwu->config_size, GFP_KERNEL);

	block_offset[1] |= (fwu->config_area << 5);

	retval = fwu->fn_ptr->write(fwu->rmi4_data,
			fwu->f34_fd.data_base_addr,
			block_offset,
			sizeof(block_offset));
	if (retval < 0) {
		dev_err(&fwu->rmi4_data->i2c_client->dev,
				"%s: Failed to write to block number registers\n",
				__func__);
		goto exit;
	}

	for (block_num = 0; block_num < block_count; block_num++) {
		retval = fwu_write_f34_command(CMD_READ_CONFIG_BLOCK);
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
					"%s: Failed to write read config command\n",
					__func__);
			goto exit;
		}

		retval = fwu_wait_for_idle(WRITE_WAIT_MS);
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
					"%s: Failed to wait for idle status\n",
					__func__);
			goto exit;
		}

		retval = fwu->fn_ptr->read(fwu->rmi4_data,
				fwu->f34_blkdata_addr,
				&fwu->read_config_buf[index],
				fwu->block_size);
		if (retval < 0) {
			dev_err(&fwu->rmi4_data->i2c_client->dev,
					"%s: Failed to read block data (block %d)\n",
					__func__, block_num);
			goto exit;
		}

		index += fwu->block_size;
	}

exit:
	fwu->rmi4_data->reset_device(fwu->rmi4_data,
				&fwu->f01_fd.cmd_base_addr);
	return retval;
}

int synaptics_fw_updater(unsigned char *fw_data)
{
	int retval;

	if (!fwu)
		return -ENODEV;

	if (!fwu->initialized)
		return -ENODEV;

	fwu->ext_data_source = fw_data;
	fwu->config_area = UI_CONFIG_AREA;

	retval = fwu_start_reflash();

	return retval;
}
EXPORT_SYMBOL(synaptics_fw_updater);

static ssize_t fwu_sysfs_show_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (count < fwu->config_size) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Not enough space (%d bytes) in buffer\n",
				__func__, count);
		return -EINVAL;
	}

	memcpy(buf, fwu->read_config_buf, fwu->config_size);

	return fwu->config_size;
}

static ssize_t fwu_sysfs_store_image(struct file *data_file,
		struct kobject *kobj, struct bin_attribute *attributes,
		char *buf, loff_t pos, size_t count)
{
	memcpy((void *)(&fwu->ext_data_source[fwu->data_pos]),
			(const void *)buf,
			count);

	fwu->data_pos += count;

	return count;
}

static ssize_t fwu_sysfs_do_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	char template[SYNAPTICS_RMI4_FILENAME_SIZE];
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;
	struct synaptics_rmi4_device_info *rmi;

	rmi = &(rmi4_data->rmi4_mod_info);

	if (count > sizeof(fwu->fw_filename)) {
		dev_err(&rmi4_data->i2c_client->dev,
			"%s: FW filename is too long\n",
			__func__);
		retval = -EINVAL;
		goto exit;
	}

	if (!force_reflash) {
		snprintf(template, sizeof(template), "synaptics-%s-",
						rmi->product_id_string);
		if (strncmp(buf, template,
			strnlen(template, sizeof(template)))) {
			dev_err(&rmi4_data->i2c_client->dev,
				"%s: FW does not belong to %s\n",
				__func__,
				rmi->product_id_string);
			retval = -EINVAL;
			goto exit;
		}
	}

	strlcpy(fwu->fw_filename, buf, count);
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: FW filename: %s\n",
			__func__,
			fwu->fw_filename);

	retval = synaptics_fw_updater(fwu->ext_data_source);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to do reflash\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	return retval;
}

static ssize_t fwu_sysfs_write_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1) {
		retval = -EINVAL;
		goto exit;
	}

	if (input != 1) {
		retval = -EINVAL;
		goto exit;
	}

	retval = fwu_start_write_config();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to write config\n",
				__func__);
		goto exit;
	}

	retval = count;

exit:
	kfree(fwu->ext_data_source);
	fwu->ext_data_source = NULL;
	return retval;
}

static ssize_t fwu_sysfs_read_config_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned int input;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	retval = fwu_do_read_config();
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to read config\n",
				__func__);
		return retval;
	}

	return count;
}

static ssize_t fwu_sysfs_config_area_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long config_area;

	retval = sstrtoul(buf, 10, &config_area);
	if (retval)
		return retval;

	fwu->config_area = config_area;

	return count;
}

static ssize_t fwu_sysfs_image_size_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int retval;
	unsigned long size;
	struct synaptics_rmi4_data *rmi4_data = fwu->rmi4_data;

	retval = sstrtoul(buf, 10, &size);
	if (retval)
		return retval;

	fwu->image_size = size;
	fwu->data_pos = 0;

	kfree(fwu->ext_data_source);
	fwu->ext_data_source = kzalloc(fwu->image_size, GFP_KERNEL);
	if (!fwu->ext_data_source) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for image data\n",
				__func__);
		return -ENOMEM;
	}

	return count;
}

static ssize_t fwu_sysfs_force_reflash_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	unsigned int input;

	if (sscanf(buf, "%u", &input) != 1)
		return -EINVAL;

	if (input != 1)
		return -EINVAL;

	force_reflash = true;

	return count;
}

static ssize_t fwu_sysfs_block_size_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", fwu->block_size);
}

static ssize_t fwu_sysfs_firmware_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", fwu->fw_block_count);
}

static ssize_t fwu_sysfs_configuration_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", fwu->config_block_count);
}

static ssize_t fwu_sysfs_perm_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", fwu->perm_config_block_count);
}

static ssize_t fwu_sysfs_bl_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", fwu->bl_config_block_count);
}

static ssize_t fwu_sysfs_disp_config_block_count_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return scnprintf(buf, PAGE_SIZE, "%u\n", fwu->disp_config_block_count);
}

static void synaptics_rmi4_fwu_attn(struct synaptics_rmi4_data *rmi4_data,
		unsigned char intr_mask)
{
	unsigned char flash_status;
	if (fwu->intr_mask & intr_mask)
		fwu_read_f34_flash_status(&flash_status, false);

	return;
}

static int synaptics_rmi4_fwu_init(struct synaptics_rmi4_data *rmi4_data)
{
	int retval;
	unsigned char attr_count;
	struct pdt_properties pdt_props;
	struct synaptics_rmi4_device_info *rmi;

	fwu = kzalloc(sizeof(*fwu), GFP_KERNEL);
	if (!fwu) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fwu\n",
				__func__);
		goto exit;
	}

	fwu->fn_ptr = kzalloc(sizeof(*(fwu->fn_ptr)), GFP_KERNEL);
	if (!fwu->fn_ptr) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to alloc mem for fn_ptr\n",
				__func__);
		retval = -ENOMEM;
		goto exit_free_fwu;
	}

	fwu->rmi4_data = rmi4_data;
	rmi = &(rmi4_data->rmi4_mod_info);
	fwu->fn_ptr->read = rmi4_data->i2c_read;
	fwu->fn_ptr->write = rmi4_data->i2c_write;
	fwu->fn_ptr->enable = rmi4_data->irq_enable;

	retval = fwu->fn_ptr->read(rmi4_data,
			PDT_PROPS,
			pdt_props.data,
			sizeof(pdt_props.data));
	if (retval < 0) {
		dev_dbg(&rmi4_data->i2c_client->dev,
				"%s: Failed to read PDT properties, assuming 0x00\n",
				__func__);
	} else if (pdt_props.has_bsr) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Reflash for LTS not currently supported\n",
				__func__);
		goto exit_free_mem;
	}

	retval = fwu_scan_pdt();
	if (retval < 0)
		goto exit_free_mem;

	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: F01 product info: 0x%04x 0x%04x\n", __func__,
			rmi->product_info[0],
			rmi->product_info[1]);
	dev_dbg(&rmi4_data->i2c_client->dev,
			"%s: F01 product ID: %s\n", __func__,
			rmi->product_id_string);

	retval = fwu_read_f34_queries();
	if (retval < 0)
		goto exit_free_mem;

	pr_notice("F34 version %d\n", fwu->bootloader_id[1] - '5');

	wake_lock_init(&fwu->flash_wake_lock,
				WAKE_LOCK_SUSPEND, "synaptics_fw_flash");
	fwu->initialized = true;

	retval = sysfs_create_bin_file(&rmi4_data->i2c_client->dev.kobj,
			&dev_attr_data);
	if (retval < 0) {
		dev_err(&rmi4_data->i2c_client->dev,
				"%s: Failed to create sysfs bin file\n",
				__func__);
		goto exit_free_mem;
	}

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		retval = sysfs_create_file(&rmi4_data->i2c_client->dev.kobj,
				&attrs[attr_count].attr);
		if (retval < 0) {
			dev_err(&rmi4_data->i2c_client->dev,
					"%s: Failed to create sysfs attributes\n",
					__func__);
			retval = -ENODEV;
			goto exit_remove_attrs;
		}
	}

	return 0;

exit_remove_attrs:
for (attr_count--; attr_count >= 0; attr_count--) {
	sysfs_remove_file(&rmi4_data->i2c_client->dev.kobj,
			&attrs[attr_count].attr);
}

sysfs_remove_bin_file(&rmi4_data->i2c_client->dev.kobj, &dev_attr_data);

exit_free_mem:
	kfree(fwu->fn_ptr);

exit_free_fwu:
	kfree(fwu);

exit:
	return 0;
}

static void synaptics_rmi4_fwu_remove(struct synaptics_rmi4_data *rmi4_data)
{
	unsigned char attr_count;

	sysfs_remove_bin_file(&rmi4_data->i2c_client->dev.kobj, &dev_attr_data);

	for (attr_count = 0; attr_count < ARRAY_SIZE(attrs); attr_count++) {
		sysfs_remove_file(&rmi4_data->i2c_client->dev.kobj,
				&attrs[attr_count].attr);
	}

	kfree(fwu->fn_ptr);
	kfree(fwu);

	complete(&remove_complete);

	return;
}

static int __init rmi4_fw_update_module_init(void)
{
	synaptics_rmi4_new_function(RMI_FW_UPDATER, true,
			synaptics_rmi4_fwu_init,
			synaptics_rmi4_fwu_remove,
			synaptics_rmi4_fwu_attn,
			IC_MODE_ANY);
	return 0;
}

static void __exit rmi4_fw_update_module_exit(void)
{
	init_completion(&remove_complete);
	synaptics_rmi4_new_function(RMI_FW_UPDATER, false,
			synaptics_rmi4_fwu_init,
			synaptics_rmi4_fwu_remove,
			synaptics_rmi4_fwu_attn,
			IC_MODE_ANY);
	wait_for_completion(&remove_complete);
	return;
}

module_init(rmi4_fw_update_module_init);
module_exit(rmi4_fw_update_module_exit);

MODULE_AUTHOR("Synaptics, Inc.");
MODULE_DESCRIPTION("Synaptics DSX FW Update Module");
MODULE_LICENSE("GPL v2");
MODULE_VERSION(SYNAPTICS_DSX_DRIVER_VERSION);
