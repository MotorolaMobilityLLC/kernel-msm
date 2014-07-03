/*
 * Copyright (c) 2012 Synaptics Incorporated
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
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */

#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/ihex.h>
#include <linux/kernel.h>
#include <linux/moduleparam.h>
#include <linux/time.h>
#include <linux/i2c.h>
#include <linux/stat.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/synaptics_i2c_rmi4.h>
#include <linux/interrupt.h>
#include "synaptics_i2c_rmi4.h"

#define HAS_BSR_MASK 0x20

#define CHECKSUM_OFFSET                 0
#define BOOTLOADER_VERSION_OFFSET       0x07
#define IMAGE_SIZE_OFFSET               0x08
#define CONFIG_SIZE_OFFSET              0x0C
#define PRODUCT_ID_OFFSET               0x10
#define PRODUCT_INFO_OFFSET             0x1E
#define PRODUCT_INFO_SIZE               2
#define PRODUCT_ID_SIZE                 10

/* F34 image file offsets. */
#define F34_FW_IMAGE_OFFSET         0x100

/* F34 register offsets. */
#define F34_BLOCK_DATA_OFFSET 2
#define F34_BLOCK_DATA_OFFSET_V1 1

/* F34 commands */
#define F34_WRITE_FW_BLOCK          0x2
#define F34_ERASE_ALL               0x3
#define F34_READ_CONFIG_BLOCK       0x5
#define F34_WRITE_CONFIG_BLOCK      0x6
#define F34_ERASE_CONFIG            0x7
#define F34_ENABLE_FLASH_PROG       0xf
#define F34_STATUS_IN_PROGRESS      0xff
#define F34_STATUS_IDLE             0x80
#define F34_IDLE_WAIT_MS            500
#define F34_ENABLE_WAIT_MS          300
#define F34_ERASE_WAIT_MS           (5 * 1000)

#define IS_IDLE(ctl_ptr)      ((!ctl_ptr->status) && (!ctl_ptr->command))
#define extract_u32(ptr)      (le32_to_cpu(*(__le32 *)(ptr)))

#define FIRMWARE_NAME_FORCE	"rmi4.img"
#define PID_S3202_GFF 0
#define PID_S3202_OGS "TM2178"
#define PID_S3408 "s3408_ver5"
#define PID_S3402 "s3402"

/** Image file V5, Option 0
 */
struct image_header {
	u32 checksum;
	unsigned int	image_size;
	unsigned int	config_size;
	unsigned char	options;
	unsigned char	bootloader_version;
	u8		product_id[PRODUCT_ID_SIZE + 1];
	unsigned char	product_info[PRODUCT_INFO_SIZE];
};

struct reflash_data {
	struct rmi4_data		*rmi4_dev;
	struct rmi4_fn_desc		*f01_pdt;
	union f01_basic_queries		f01_queries;
	union f01_device_control_0	f01_controls;
	char				product_id[PRODUCT_ID_SIZE + 1];
	struct rmi4_fn_desc		*f34_pdt;
	u8				bootloader_id[2];
	union f34_query_regs		f34_queries;
	union f34_control_status	f34_controls;
	const u8			*firmware_data;
	const u8			*config_data;
	unsigned int			int_count;
};

/* If this parameter is true, we will update the firmware regardless of
 * the versioning info.
 */
static bool force;
module_param(force, bool, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(param, "Force reflash of RMI4 devices");

/* If this parameter is not NULL, we'll use that name for the firmware image,
 * instead of getting it from the F01 queries.
 */
static char *img_name;
module_param(img_name, charp, S_IRUGO | S_IWUSR);
MODULE_PARM_DESC(param, "Name of the RMI4 firmware image");

#define RMI4_IMAGE_FILE_REV1_OFFSET	30
#define RMI4_IMAGE_FILE_REV2_OFFSET	31
#define IMAGE_FILE_CHECKSUM_SIZE	4
#define FIRMWARE_IMAGE_AREA_OFFSET	0x100

static void
extract_header(const u8 *data, int pos, struct image_header *header)
{
	header->checksum = extract_u32(&data[pos + CHECKSUM_OFFSET]);
	header->bootloader_version = data[pos + BOOTLOADER_VERSION_OFFSET];
	header->image_size = extract_u32(&data[pos + IMAGE_SIZE_OFFSET]);
	header->config_size = extract_u32(&data[pos + CONFIG_SIZE_OFFSET]);
	memcpy(header->product_id,
			&data[pos + PRODUCT_ID_OFFSET], PRODUCT_ID_SIZE);
	header->product_id[PRODUCT_ID_SIZE] = 0;
	memcpy(header->product_info,
			&data[pos + PRODUCT_INFO_OFFSET], PRODUCT_INFO_SIZE);
}

static int rescan_pdt(struct reflash_data *data)
{
	int i, retval;
	bool f01_found = false, f34_found = false;
	struct rmi4_fn_desc rmi4_fn_desc;
	struct rmi4_data *rmi4_dev = data->rmi4_dev;
	struct rmi4_fn_desc *f34_pdt = data->f34_pdt;
	struct rmi4_fn_desc *f01_pdt = data->f01_pdt;
	struct i2c_client *client = rmi4_dev->i2c_client;

	/* Per spec, once we're in reflash we only need to look at the first
	 * PDT page for potentially changed F01 and F34 information.
	 */
	for (i = PDT_START_SCAN_LOCATION; i >= PDT_END_SCAN_LOCATION;
			i -= sizeof(rmi4_fn_desc)) {
		retval = rmi4_i2c_block_read(rmi4_dev, i, (u8 *)&rmi4_fn_desc,
					sizeof(rmi4_fn_desc));
		if (retval != sizeof(rmi4_fn_desc)) {
			dev_err(&client->dev,
				"Read PDT entry at %#06x failed: %d.\n",
				i, retval);
			return retval;
		}

		if (RMI4_END_OF_PDT(rmi4_fn_desc.fn_number))
			break;

		if (rmi4_fn_desc.fn_number == 0x01) {
			memcpy(f01_pdt, &rmi4_fn_desc, sizeof(rmi4_fn_desc));
			f01_found = true;
		} else if (rmi4_fn_desc.fn_number == 0x34) {
			memcpy(f34_pdt, &rmi4_fn_desc, sizeof(rmi4_fn_desc));
			f34_found = true;
		}
	}

	if (!f01_found) {
		dev_err(&client->dev, "Failed to find F01 PDT entry.\n");
		retval = -ENODEV;
	} else if (!f34_found) {
		dev_err(&client->dev, "Failed to find F34 PDT entry.\n");
		retval = -ENODEV;
	} else {
		retval = 0;
	}

	return retval;
}

static int read_f34_controls(struct reflash_data *data)
{
	int retval;
	union f34_control_status_v1 f34ctrlsts1;
	if (data->bootloader_id[1] > '5') {
		retval = rmi4_i2c_block_read(data->rmi4_dev,
			data->f34_controls.address, f34ctrlsts1.regs,
			sizeof(f34ctrlsts1.regs));

		data->f34_controls.command = f34ctrlsts1.command;
		data->f34_controls.status = f34ctrlsts1.status;
		data->f34_controls.program_enabled =
			f34ctrlsts1.program_enabled;

		return retval;
	} else
		return rmi4_i2c_byte_read(data->rmi4_dev,
			data->f34_controls.address, data->f34_controls.regs);
}

static int read_f01_status(struct reflash_data *data,
			   union f01_device_status *device_status)
{
	return rmi4_i2c_byte_read(data->rmi4_dev,
			data->f01_pdt->data_base_addr, device_status->regs);
}

static int read_f01_controls(struct reflash_data *data)
{
	return rmi4_i2c_byte_read(data->rmi4_dev,
			data->f01_pdt->ctrl_base_addr,
			data->f01_controls.regs);
}

static int write_f01_controls(struct reflash_data *data)
{
	return rmi4_i2c_byte_write(data->rmi4_dev,
			data->f01_pdt->ctrl_base_addr,
			data->f01_controls.regs[0]);
}

#define SLEEP_TIME_MS 20
/* Wait until the status is idle and we're ready to continue */
static int wait_for_idle(struct reflash_data *data, int timeout_ms)
{
	int timeout_count = timeout_ms / SLEEP_TIME_MS + 1;
	int count = 0;
	union f34_control_status *controls = &data->f34_controls;
	int retval;
	struct i2c_client *client = data->rmi4_dev->i2c_client;

	do {
		if (count || timeout_count == 1)
			msleep(SLEEP_TIME_MS);
		count++;
		retval = read_f34_controls(data);
		if (retval < 0) {
			dev_warn(&client->dev,
				"Waiting for idle, check times: %d\n", count);
			continue;
		} else if (IS_IDLE(controls)) {
			if (!data->f34_controls.program_enabled) {
				/* TODO: Kill this whole if block once
				 * FW-39000 is resolved. */
				dev_warn(&client->dev,
					"Yikes!  We're not enabled!\n");
				msleep(1000);
				read_f34_controls(data);
			}
			return 0;
		}
	} while (count < timeout_count);

	dev_err(&client->dev,
		"Timeout waiting for idle status, last status: %#04x.\n",
		controls->regs[0]);
	dev_err(&client->dev, "Command: %#04x\n", controls->command);
	dev_err(&client->dev, "Status:  %#04x\n", controls->status);
	dev_err(&client->dev, "Enabled: %d\n", controls->program_enabled);
	dev_err(&client->dev, "Idle:    %d\n", IS_IDLE(controls));
	return -ETIMEDOUT;
}

#define INT_SLEEP_TIME_MS 1
static int wait_for_interrupt(struct reflash_data *data, int timeout_ms)
{
	int timeout_count = timeout_ms / INT_SLEEP_TIME_MS + 1;
	int count = 0;
	int int_count = data->int_count;
	struct i2c_client *client = data->rmi4_dev->i2c_client;

	do {
		if (count || timeout_count == 1)
			msleep(INT_SLEEP_TIME_MS);
		count++;
		if (data->int_count != int_count)
			return 0;
	} while (count < timeout_count);

	dev_err(&client->dev, "%s, interrupt not detected within %d ms.\n",
		__func__, timeout_ms);

	return -ETIMEDOUT;
}

static irqreturn_t f34_irq_thread(int irq, void *data)
{
	u8 intr_status;
	int retval;
	struct reflash_data *pdata = data;
	struct i2c_client *client = pdata->rmi4_dev->i2c_client;

	pdata->int_count++;

	/* Assuming we're in bootloader mode, we only read one interrupt
	   register */
	retval = rmi4_i2c_block_read(pdata->rmi4_dev,
		pdata->f01_pdt->data_base_addr + 1,
		&intr_status,
		1);

	if (retval != 1) {
		dev_err(&client->dev,
			"could not read interrupt status register\n");
		return IRQ_NONE;
	}

	return IRQ_HANDLED;
}

static int read_f01_queries(struct reflash_data *data)
{
	int retval;
	u16 addr = data->f01_pdt->query_base_addr;
	struct i2c_client *client = data->rmi4_dev->i2c_client;

	retval = rmi4_i2c_block_read(data->rmi4_dev,
					addr, data->f01_queries.regs,
					ARRAY_SIZE(data->f01_queries.regs));
	if (retval < 0) {
		dev_err(&client->dev,
			"Failed to read F34 queries (code %d).\n", retval);
		return retval;
	}
	addr += ARRAY_SIZE(data->f01_queries.regs);

	retval = rmi4_i2c_block_read(data->rmi4_dev,
				addr, data->product_id, PRODUCT_ID_SIZE);
	if (retval < 0) {
		dev_err(&client->dev,
			"Failed to read product ID (code %d).\n", retval);
		return retval;
	}
	data->product_id[PRODUCT_ID_SIZE] = 0;
	dev_info(&client->dev, "F01 Product id:   %s\n",
			data->product_id);
	dev_info(&client->dev, "F01 product info: %#04x %#04x\n",
				data->f01_queries.productinfo_1,
				data->f01_queries.productinfo_2);

	return 0;
}

static int read_f34_queries(struct reflash_data *data)
{
	int retval;
	u8 id_str[3];
	struct i2c_client *client = data->rmi4_dev->i2c_client;

	retval = rmi4_i2c_block_read(data->rmi4_dev,
				data->f34_pdt->query_base_addr,
				data->bootloader_id, 2);
	if (retval < 0) {
		dev_err(&client->dev,
			"Failed to read F34 bootloader_id (code %d).\n",
			retval);
		return retval;
	}

	if (data->bootloader_id[1] > '5')
		retval = rmi4_i2c_block_read(data->rmi4_dev,
			data->f34_pdt->query_base_addr+1,
			data->f34_queries.regs,
			ARRAY_SIZE(data->f34_queries.regs));
	else
		retval = rmi4_i2c_block_read(data->rmi4_dev,
			data->f34_pdt->query_base_addr+2,
			data->f34_queries.regs,
			ARRAY_SIZE(data->f34_queries.regs));
	if (retval < 0) {
		dev_err(&client->dev,
			"Failed to read F34 queries (code %d).\n", retval);
		return retval;
	}
	data->f34_queries.block_size =
			le16_to_cpu(data->f34_queries.block_size);
	data->f34_queries.fw_block_count =
			le16_to_cpu(data->f34_queries.fw_block_count);
	data->f34_queries.config_block_count =
			le16_to_cpu(data->f34_queries.config_block_count);
	id_str[0] = data->bootloader_id[0];
	id_str[1] = data->bootloader_id[1];
	id_str[2] = 0;

	dev_dbg(&client->dev, "Got F34 data->f34_queries.\n");
	dev_dbg(&client->dev, "F34 bootloader id: %s (%#04x %#04x)\n",
		id_str, data->bootloader_id[0], data->bootloader_id[1]);
	dev_dbg(&client->dev, "F34 has config id: %d\n",
			data->f34_queries.has_config_id);
	dev_dbg(&client->dev, "F34 unlocked:      %d\n",
			data->f34_queries.unlocked);
	dev_dbg(&client->dev, "F34 regMap:        %d\n",
			data->f34_queries.reg_map);
	dev_dbg(&client->dev, "F34 block size:    %d\n",
			data->f34_queries.block_size);
	dev_dbg(&client->dev, "F34 fw blocks:     %d\n",
			data->f34_queries.fw_block_count);
	dev_dbg(&client->dev, "F34 config blocks: %d\n",
			data->f34_queries.config_block_count);

	if (data->bootloader_id[1] > '5')
		data->f34_controls.address = data->f34_pdt->data_base_addr +
			F34_BLOCK_DATA_OFFSET_V1 + 1;
	else
		data->f34_controls.address = data->f34_pdt->data_base_addr +
			F34_BLOCK_DATA_OFFSET + data->f34_queries.block_size;

	return 0;
}

static int write_bootloader_id(struct reflash_data *data)
{
	int retval;
	struct rmi4_data *rmi4_dev = data->rmi4_dev;
	struct rmi4_fn_desc *f34_pdt = data->f34_pdt;
	struct i2c_client *client = data->rmi4_dev->i2c_client;

	if (data->bootloader_id[1] > '5') {
		retval = rmi4_i2c_block_write(rmi4_dev,
			f34_pdt->data_base_addr + F34_BLOCK_DATA_OFFSET_V1,
			data->bootloader_id, ARRAY_SIZE(data->bootloader_id));
	} else
		retval = rmi4_i2c_block_write(rmi4_dev,
			f34_pdt->data_base_addr + F34_BLOCK_DATA_OFFSET,
			data->bootloader_id, ARRAY_SIZE(data->bootloader_id));
	if (retval < 0) {
		dev_err(&client->dev,
			"Failed to write bootloader ID. Code: %d.\n", retval);
		return retval;
	}

	return 0;
}

static int write_f34_command(struct reflash_data *data, u8 command)
{
	int retval;
	struct rmi4_data *rmi4_dev = data->rmi4_dev;
	struct i2c_client *client = data->rmi4_dev->i2c_client;

	retval = rmi4_i2c_byte_write(rmi4_dev,
				data->f34_controls.address, command);
	if (retval < 0) {
		dev_err(&client->dev,
			"Failed to write F34 command %#04x. Code: %d.\n",
			command, retval);
		return retval;
	}

	return 0;
}

static int enter_flash_programming(struct reflash_data *data)
{
	int retval;
	union f01_device_status device_status;
	struct i2c_client *client = data->rmi4_dev->i2c_client;

	retval = write_bootloader_id(data);
	if (retval < 0)
		return retval;

	dev_info(&client->dev, "Enabling flash programming.\n");
	retval = write_f34_command(data, F34_ENABLE_FLASH_PROG);
	if (retval < 0)
		return retval;

	wait_for_interrupt(data, F34_ENABLE_WAIT_MS);

	retval = wait_for_idle(data, F34_ENABLE_WAIT_MS);
	if (retval < 0) {
		dev_err(&client->dev,
			"Did not reach idle state after %d ms. Code: %d.\n",
			F34_ENABLE_WAIT_MS, retval);
		return retval;
	}
	if (!data->f34_controls.program_enabled) {
		dev_err(&client->dev,
			"Reached idle, but programming not enabled, "
			" current status register: %#04x.\n",
			data->f34_controls.regs[0]);
		return -EINVAL;
	}
	dev_info(&client->dev, "HOORAY! Programming is enabled!\n");

	retval = rescan_pdt(data);
	if (retval < 0) {
		dev_err(&client->dev,
				"Failed to rescan pdt.  Code: %d.\n", retval);
		return retval;
	}

	retval = read_f01_status(data, &device_status);
	if (retval < 0) {
		dev_err(&client->dev,
			"Failed to read F01 status after enabling reflash."
			"Code: %d\n", retval);
		return retval;
	}
	if (!(device_status.flash_prog)) {
		dev_err(&client->dev,
			"Device reports as not in flash programming mode.\n");
		return -EINVAL;
	}

	retval = read_f34_queries(data);
	if (retval < 0) {
		dev_err(&client->dev, "F34 queries failed, code = %d.\n",
			retval);
		return retval;
	}

	retval = read_f01_controls(data);
	if (retval < 0) {
		dev_err(&client->dev,
			"F01 controls read failed, code = %d.\n", retval);
		return retval;
	}
	data->f01_controls.nosleep = true;
	data->f01_controls.sleep_mode = RMI4_SLEEP_MODE_NORMAL;

	retval = write_f01_controls(data);
	if (retval < 0) {
		dev_err(&client->dev,
			"F01 controls write failed, code = %d.\n", retval);
		return retval;
	}

	return 0;
}

static void reset_device(struct reflash_data *data)
{
	int retval;
	struct i2c_client *client = data->rmi4_dev->i2c_client;

	dev_info(&client->dev, "Resetting...\n");
	retval = rmi4_i2c_byte_write(data->rmi4_dev,
			data->f01_pdt->cmd_base_addr, RMI4_DEVICE_RESET_CMD);
	if (retval < 0)
		dev_warn(&client->dev,
			 "WARNING - post-flash reset failed, code: %d.\n",
			 retval);
	msleep(RMI4_RESET_DELAY);
	dev_info(&client->dev, "Reset completed.\n");
}

/*
 * Send data to the device one block at a time.
 */
static int write_blocks(struct reflash_data *data, u8 *block_ptr,
			u16 block_count, u8 cmd)
{
	int block_num;
	u8 zeros[] = {0, 0};
	int retval;
	u16 addr = data->f34_pdt->data_base_addr + F34_BLOCK_DATA_OFFSET;
	struct i2c_client *client = data->rmi4_dev->i2c_client;

	if (data->bootloader_id[1] > '5')
		addr = data->f34_pdt->data_base_addr + F34_BLOCK_DATA_OFFSET_V1;

	retval = rmi4_i2c_block_write(data->rmi4_dev,
					data->f34_pdt->data_base_addr,
					zeros, ARRAY_SIZE(zeros));
	if (retval < 0) {
		dev_err(&client->dev,
			"Failed to write initial zeros. Code=%d.\n", retval);
		return retval;
	}

	for (block_num = 0; block_num < block_count; ++block_num) {
		retval = rmi4_i2c_block_write(data->rmi4_dev, addr, block_ptr,
					 data->f34_queries.block_size);
		if (retval < 0) {
			dev_err(&client->dev,
				"Failed to write block %d. Code=%d.\n",
						block_num, retval);
			return retval;
		}

		retval = write_f34_command(data, cmd);
		if (retval < 0) {
			dev_err(&client->dev,
			"Failed to write command for block %d. Code=%d.\n",
							block_num, retval);
			return retval;
		}

		retval = wait_for_idle(data, F34_IDLE_WAIT_MS);
		if (retval < 0) {
			dev_err(&client->dev,
			"Failed to go idle after writing block %d. Code=%d.\n",
							block_num, retval);
			return retval;
		}

		block_ptr += data->f34_queries.block_size;
	}

	return 0;
}

static int write_firmware(struct reflash_data *data)
{
	return write_blocks(data, (u8 *) data->firmware_data,
		data->f34_queries.fw_block_count, F34_WRITE_FW_BLOCK);
}

static int write_configuration(struct reflash_data *data)
{
	return write_blocks(data, (u8 *)data->config_data,
		data->f34_queries.config_block_count, F34_WRITE_CONFIG_BLOCK);
}

static void reflash_firmware(struct reflash_data *data)
{
	struct timespec start;
	struct timespec end;
	s64 duration_ns;
	int retval = 0;
	struct i2c_client *client = data->rmi4_dev->i2c_client;

	retval = enter_flash_programming(data);
	if (retval < 0)
		return;

	retval = write_bootloader_id(data);
	if (retval < 0)
		return;

	dev_info(&client->dev, "Erasing FW...\n");
	getnstimeofday(&start);

	retval = write_f34_command(data, F34_ERASE_ALL);
	if (retval < 0)
		return;

	wait_for_interrupt(data, F34_ERASE_WAIT_MS);

	dev_info(&client->dev, "Waiting for idle...\n");
	retval = wait_for_idle(data, F34_ERASE_WAIT_MS);
	if (retval < 0) {
		dev_err(&client->dev,
			"Failed to reach idle state. Code: %d.\n", retval);
		return;
	}

	getnstimeofday(&end);
	duration_ns = timespec_to_ns(&end) - timespec_to_ns(&start);
	dev_info(&client->dev,
		 "Erase complete, time: %lld ns.\n", duration_ns);

	if (data->firmware_data) {
		dev_info(&client->dev, "Writing firmware...\n");
		getnstimeofday(&start);
		retval = write_firmware(data);
		if (retval < 0)
			return;
		getnstimeofday(&end);
		duration_ns = timespec_to_ns(&end) - timespec_to_ns(&start);
		dev_info(&client->dev,
			 "Done writing FW, time: %lld ns.\n", duration_ns);
	}

	if (data->config_data) {
		dev_info(&client->dev, "Writing configuration...\n");
		getnstimeofday(&start);
		retval = write_configuration(data);
		if (retval < 0)
			return;
		getnstimeofday(&end);
		duration_ns = timespec_to_ns(&end) - timespec_to_ns(&start);
		dev_info(&client->dev,
			 "Done writing config, time: %lld ns.\n", duration_ns);
	}
}

/* Returns false if the firmware should not be reflashed.
 */
static bool
go_nogo(struct reflash_data *data, const struct rmi4_touch_calib *calib)
{
	int retval;
	u8 customer_id[4] = { 0 };
	u32 id;
	struct i2c_client *client = data->rmi4_dev->i2c_client;

	retval = rmi4_i2c_block_read(data->rmi4_dev,
				data->f34_pdt->ctrl_base_addr,
				customer_id, sizeof(customer_id));
	if (retval < 0) {
		dev_err(&client->dev, "Failed to read customer congfig ID (code %d).\n",
								retval);
		return true;
	}
	dev_info(&client->dev, "Customer ID HEX: 0x%x %x %x %x\n",
				customer_id[0], customer_id[1],
				customer_id[2], customer_id[3]);

	id = *(u32 *)customer_id;
	dev_info(&client->dev, "Customer ID: %d\n", id);

	if (id != calib->customer_id)
		return true;
	return false || force;
}

static void
print_image_info(struct i2c_client *client, struct image_header *header,
					const struct firmware *fw_entry)
{
	dev_info(&client->dev, "Img checksum:           %#08X\n",
			header->checksum);
	dev_info(&client->dev, "Img image size:         %d\n",
			header->image_size);
	dev_info(&client->dev, "Img config size:        %d\n",
			header->config_size);
	dev_info(&client->dev, "Img bootloader version: %d\n",
			header->bootloader_version);
	dev_info(&client->dev, "Img product id:         %s\n",
			header->product_id);
	dev_info(&client->dev, "Img product info:       %#04x %#04x\n",
			header->product_info[0], header->product_info[1]);
	dev_info(&client->dev, "Got firmware, size: %d.\n", fw_entry->size);
}

int rmi4_fw_update(struct rmi4_data *pdata,
		struct rmi4_fn_desc *f01_pdt, struct rmi4_fn_desc *f34_pdt)
{
#ifdef DEBUG
	struct timespec start;
	struct timespec end;
	s64 duration_ns;
#endif
	int retval, touch_type = 0;
	char *firmware_name;
	const struct firmware *fw_entry = NULL;
	struct i2c_client *client = pdata->i2c_client;
	union pdt_properties pdt_props;
	struct image_header header = { 0 };
	struct reflash_data data = {
		.rmi4_dev = pdata,
		.f01_pdt = f01_pdt,
		.f34_pdt = f34_pdt,
	};
	const struct rmi4_touch_calib *calib = pdata->board->calib;
	const struct rmi4_platform_data *platformdata =
		client->dev.platform_data;

	dev_info(&client->dev, "Enter %s.\n", __func__);
#ifdef	DEBUG
	getnstimeofday(&start);
#endif

	retval = rmi4_i2c_byte_read(pdata,
				PDT_PROPERTIES_LOCATION, pdt_props.regs);
	if (retval < 0) {
		dev_warn(&client->dev,
			 "Failed to read PDT props at %#06x (code %d).\n",
			 PDT_PROPERTIES_LOCATION, retval);
	}
	if (pdt_props.has_bsr) {
		dev_warn(&client->dev,
			 "Firmware update for LTS not currently supported.\n");
		return -1;
	}

	retval = read_f01_queries(&data);
	if (retval) {
		dev_err(&client->dev, "F01 queries failed, code = %d.\n",
			retval);
		return -1;
	}

	if (data.product_id[0] == PID_S3202_GFF)
		touch_type = RMI4_S3202_GFF;
	else if (strcmp(data.product_id, PID_S3202_OGS) == 0)
		touch_type = RMI4_S3202_OGS;
	else if (strcmp(data.product_id, PID_S3402) == 0) {
		if (!strncmp(client->name, S3400_CGS_DEV_ID, I2C_NAME_SIZE))
			touch_type = RMI4_S3400_CGS;
		else
			touch_type = RMI4_S3400_IGZO;
	} else {
		dev_err(&client->dev, "Unsupported touch screen type, product ID: %s\n",
				data.product_id);
		if (!force) {
			dev_err(&client->dev, "Use S3202 OGS as default type\n");
			return RMI4_S3202_OGS;
		}
	}

	if (force)
		firmware_name = FIRMWARE_NAME_FORCE;
	else
		firmware_name = calib[touch_type].fw_name;
	dev_info(&client->dev,
			"Firmware name:%s, hardware type:%d, client name:%s\n",
			firmware_name, touch_type, client->name);

	retval = read_f34_queries(&data);
	if (retval) {
		dev_err(&client->dev, "F34 queries failed, code = %d.\n",
			retval);
		return touch_type;
	}
	if (!go_nogo(&data, &calib[touch_type])) {
		dev_info(&client->dev, "Don't need to reflash firmware.\n");
		return touch_type;
	}
	dev_info(&client->dev, "Requesting %s.\n", firmware_name);
	retval = request_firmware(&fw_entry, firmware_name, &client->dev);
	if (retval != 0 || !fw_entry) {
		dev_err(&client->dev,
				"Firmware %s not available, code = %d\n",
				firmware_name, retval);
		return touch_type;
	}

	extract_header(fw_entry->data, 0, &header);
	print_image_info(client, &header, fw_entry);

	if (header.image_size)
		data.firmware_data = fw_entry->data + F34_FW_IMAGE_OFFSET;
	if (header.config_size)
		data.config_data = fw_entry->data + F34_FW_IMAGE_OFFSET +
			header.image_size;

	retval = request_threaded_irq(pdata->irq, NULL,
		f34_irq_thread,
		platformdata->irq_type,
		"rmi4_f34", &data);
	if (retval < 0) {
		dev_err(&client->dev, "Unable to get attn irq %d\n",
		pdata->irq);
	}

	reflash_firmware(&data);
	reset_device(&data);
	release_firmware(fw_entry);

	free_irq(pdata->irq, &data);
#ifdef	DEBUG
	getnstimeofday(&end);
	duration_ns = timespec_to_ns(&end) - timespec_to_ns(&start);
	dev_info(&client->dev, "Time to reflash: %lld ns.\n", duration_ns);
#endif
	return touch_type;
}
