/*
 * Copyright (C) 2010-2013 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/motosh.h>


#define OLD_BOOT_VER 0x10

#define I2C_RETRIES 5
#define I2C_RETRY_DELAY 20
#define COMMAND_RETRIES 5
#define COMMAND_DELAY 2000

#define ACK_BYTE 0x79
#define BUSY_BYTE 0x76
#define NACK_BYTE 0x1F

#define ERASE_DELAY 200
#define ERASE_TIMEOUT 80

#define RESTART_DELAY 1000
#define WRITE_DELAY 20
#define WRITE_TIMEOUT 20

enum stm_command {
	GET_VERSION = 0x01,
	GET_ID = 0x02,
	READ_MEMORY = 0x11,
	GO = 0x21,
	WRITE_MEMORY = 0x31,
	NO_WAIT_WRITE_MEMORY = 0x32,
	ERASE = 0x44,
	NO_WAIT_ERASE = 0x45,
	WRITE_UNPROTECT = 0x73,
};


static unsigned char motosh_bootloader_ver;


static int motosh_boot_i2c_write(struct motosh_data *ps_motosh,
	u8 *buf, int len)
{
	int ret;
	int tries = I2C_RETRIES;
	struct i2c_msg msgs[] = {
		{
			.addr = ps_motosh->client->addr,
			.flags = ps_motosh->client->flags,
			.len = len,
			.buf = buf,
		}
	};

	if (buf == NULL || len == 0)
		return -EFAULT;

	while (tries) {
		ret = i2c_transfer(ps_motosh->client->adapter, msgs, 1);
		if (ret >= 0)
			break;

		dev_err(&motosh_misc_data->client->dev,
			"Boot mode I2C write error %d\n", ret);
		tries--;
		msleep(I2C_RETRY_DELAY);
	}

	if (ret < 0)
		dev_err(&motosh_misc_data->client->dev,
			"Boot mode I2C write fail %d\n", ret);

	return  ret;
}

static int motosh_boot_i2c_read(struct motosh_data *ps_motosh, u8 *buf, int len)
{
	int ret;
	int tries = I2C_RETRIES;
	struct i2c_msg msgs[] = {
		{
			.addr = ps_motosh->client->addr,
			.flags = ps_motosh->client->flags | I2C_M_RD,
			.len = len,
			.buf = buf,
		}
	};

	if (buf == NULL || len == 0)
		return -EFAULT;

	while (tries) {
		ret = i2c_transfer(ps_motosh->client->adapter, msgs, 1);
		if (ret >= 0)
			break;

		dev_err(&motosh_misc_data->client->dev,
			"Boot mode I2C read error %d\n", ret);
		tries--;
		msleep(I2C_RETRY_DELAY);
	}

	if (ret < 0)
		dev_err(&motosh_misc_data->client->dev,
			"Boot mode I2C read fail %d\n", ret);

	return  ret;
}

static int motosh_boot_cmd_write(struct motosh_data *ps_motosh,
	enum stm_command command)
{
	int index = 0;

	motosh_cmdbuff[index++] = command;
	motosh_cmdbuff[index++] = ~command;

	return motosh_boot_i2c_write(ps_motosh, motosh_cmdbuff, index);
}

static int motosh_boot_checksum_write(struct motosh_data *ps_motosh,
	u8 *buf, int len)
{
	int i;
	u8 checksum = 0;

	for (i = 0; i < len; i++)
		checksum ^= buf[i];
	buf[i++] = checksum;

	return motosh_boot_i2c_write(ps_motosh, buf, i);
}

static int motosh_get_boot_ver(void)
{
	int tries = COMMAND_RETRIES;
	int err = 0;

	while (tries) {
		err = motosh_boot_cmd_write(motosh_misc_data, GET_VERSION);
		if (err < 0)
			goto RETRY_VER;

		err = motosh_boot_i2c_read(motosh_misc_data,
			motosh_readbuff, 1);
		if (err < 0)
			goto RETRY_VER;
		if (motosh_readbuff[0] != ACK_BYTE) {
			dev_err(&motosh_misc_data->client->dev,
				"Error sending GET_VERSION command 0x%02x\n",
				motosh_readbuff[0]);
			goto RETRY_VER;
		}

		err = motosh_boot_i2c_read(motosh_misc_data,
			motosh_readbuff, 1);
		if (err < 0)
			goto RETRY_VER;
		motosh_bootloader_ver = motosh_readbuff[0];
		dev_err(&motosh_misc_data->client->dev,
			"Bootloader version 0x%02x\n", motosh_bootloader_ver);

		err = motosh_boot_i2c_read(motosh_misc_data,
			motosh_readbuff, 1);
		if (err < 0)
			goto RETRY_VER;
		if (motosh_readbuff[0] == ACK_BYTE)
			break;
		dev_err(&motosh_misc_data->client->dev,
			"Error reading GET_VERSION data 0x%02x\n",
			motosh_readbuff[0]);
RETRY_VER:
		tries--;
		msleep(COMMAND_DELAY);
	}

	if (err < 0)
		return -EIO;

	return motosh_bootloader_ver;
}

int motosh_boot_flash_erase(void)
{
	int tries = COMMAND_RETRIES;
	int index = 0;
	int count = 0;
	int err = 0;

	if (motosh_bootloader_ver == 0) {
		if (motosh_get_boot_ver() <= 0) {
			err = -EIO;
			goto EXIT;
		}
	}

	dev_dbg(&motosh_misc_data->client->dev,
		"Starting flash erase\n");

	if (motosh_bootloader_ver > OLD_BOOT_VER) {
		/* Use new bootloader erase command */
		while (tries) {
			err = motosh_boot_cmd_write(motosh_misc_data,
				NO_WAIT_ERASE);
			if (err < 0)
				goto RETRY_ERASE;

			err = motosh_boot_i2c_read(motosh_misc_data,
				motosh_readbuff, 1);
			if (err < 0)
				goto RETRY_ERASE;
			if (motosh_readbuff[0] != ACK_BYTE) {
				dev_err(&motosh_misc_data->client->dev,
					"Error sending ERASE command 0x%02x\n",
					motosh_readbuff[0]);
				goto RETRY_ERASE;
			}

			motosh_cmdbuff[index++] = 0xFF;
			motosh_cmdbuff[index++] = 0xFF;
			err = motosh_boot_checksum_write(motosh_misc_data,
				motosh_cmdbuff, index);
			if (err < 0)
				goto RETRY_ERASE;

			motosh_readbuff[0] = 0;
			do {
				err = motosh_boot_i2c_read(motosh_misc_data,
					motosh_readbuff, 1);
				if ((err >= 0) && (motosh_readbuff[0] ==
						NACK_BYTE))
					break;
				msleep(ERASE_DELAY);
				count++;
				if (count == ERASE_TIMEOUT)
					break;
			} while (motosh_readbuff[0] != ACK_BYTE);
			if (motosh_readbuff[0] == ACK_BYTE) {
				err = 0;
				dev_dbg(&motosh_misc_data->client->dev,
					"Flash erase successful\n");
				break;
			}
			dev_err(&motosh_misc_data->client->dev,
				"Error waiting for ERASE complete 0x%02x\n",
				motosh_readbuff[0]);
RETRY_ERASE:
			err = -EIO;
			tries--;
			msleep(COMMAND_DELAY);
		}
	} else {
		/* Use old bootloader erase command */
		err = motosh_boot_cmd_write(motosh_misc_data, ERASE);
		if (err < 0)
			goto EXIT;

		err = motosh_boot_i2c_read(motosh_misc_data,
			motosh_readbuff, 1);
		if (err < 0)
			goto EXIT;
		if (motosh_readbuff[0] != ACK_BYTE) {
			dev_err(&motosh_misc_data->client->dev,
				"Error sending ERASE command 0x%02x\n",
				motosh_readbuff[0]);
			err = -EIO;
			goto EXIT;
		}

		motosh_cmdbuff[index++] = 0xFF;
		motosh_cmdbuff[index++] = 0xFF;
		err = motosh_boot_checksum_write(motosh_misc_data,
			motosh_cmdbuff, index);
		if (err < 0)
			goto EXIT;

		/* We should be checking for an ACK here, but waiting
		   for the erase to complete takes too long and the I2C
		   driver will time out and fail.
		   Instead we just wait and hope the erase was succesful.
		*/
		msleep(16000);
	}

EXIT:
	return err;
}

int motosh_get_version(struct motosh_data *ps_motosh)
{
	int err = 0;
	if (ps_motosh->mode == BOOTMODE) {
		dev_err(&ps_motosh->client->dev,
			"Tried to read version in boot mode\n");
		err = -EIO;
		goto EXIT;
	}

	motosh_wake(ps_motosh);

	motosh_cmdbuff[0] = REV_ID;
	err = motosh_i2c_write_read_no_reset(ps_motosh, motosh_cmdbuff, 1, 1);
	if (err >= 0) {
		err = (int)motosh_readbuff[0];
		dev_err(&ps_motosh->client->dev, "MOTOSH version %02x",
			motosh_readbuff[0]);
		motosh_g_booted = 1;
	}
	motosh_sleep(ps_motosh);

EXIT:
	return err;
}

int switch_motosh_mode(enum stm_mode mode)
{
	struct motosh_platform_data *pdata;
	unsigned int bslen_pin_active_value =
		motosh_misc_data->pdata->bslen_pin_active_value;
	int tries = COMMAND_RETRIES;
	int err = 0;

	pdata = motosh_misc_data->pdata;

	/* bootloader mode */
	if (mode == BOOTMODE) {
		motosh_misc_data->mode = mode;
		gpio_set_value(pdata->gpio_bslen,
			       (bslen_pin_active_value));
		dev_dbg(&motosh_misc_data->client->dev,
			"Switching to boot mode\n");
		msleep(motosh_i2c_retry_delay);
		gpio_set_value(pdata->gpio_reset, 0);
		msleep(motosh_i2c_retry_delay);
		gpio_set_value(pdata->gpio_reset, 1);

		/* TODO: wleh01, reduce this delay if possible and
		   update MOTOSH_RESET_DELAY */
		msleep(600);

		while (tries) {
			err = motosh_boot_cmd_write(motosh_misc_data, GET_ID);
			if (err < 0)
				goto RETRY_ID;

			err = motosh_boot_i2c_read(motosh_misc_data,
						   motosh_readbuff, 1);
			if (err < 0)
				goto RETRY_ID;

			if (motosh_readbuff[0] != ACK_BYTE) {
				dev_err(&motosh_misc_data->client->dev,
					"Error sending GET_ID command 0x%02x\n",
					motosh_readbuff[0]);
				goto RETRY_ID;
			}

			err = motosh_boot_i2c_read(motosh_misc_data,
						   motosh_readbuff, 3);
			if (err < 0)
				goto RETRY_ID;

			dev_err(&motosh_misc_data->client->dev,
				"Part ID 0x%02x 0x%02x 0x%02x\n",
				motosh_readbuff[0], motosh_readbuff[1],
				motosh_readbuff[2]);

			err = motosh_boot_i2c_read(motosh_misc_data,
						   motosh_readbuff, 1);
			if (err < 0)
				goto RETRY_ID;

			if (motosh_readbuff[0] == ACK_BYTE)
				break;
			dev_err(&motosh_misc_data->client->dev,
				"Error reading GETID data 0x%02x\n",
				motosh_readbuff[0]);
RETRY_ID:
			err = -EIO;
			tries--;
			msleep(COMMAND_DELAY);
		}
	} else if (mode > BOOTMODE) {

		/* normal/factory mode */
		gpio_set_value(pdata->gpio_bslen,
			       !(bslen_pin_active_value));
		dev_dbg(&motosh_misc_data->client->dev,
			"Switching to normal mode\n");
		/* init only if not in the factory
		   - motosh_irq_disable indicates factory test ongoing */
		if (!motosh_irq_disable) {
			err = motosh_reset_and_init();
			if (!err)
				motosh_misc_data->mode = mode;
			else
				motosh_misc_data->mode = UNINITIALIZED;
		} else
			motosh_reset(pdata, motosh_cmdbuff);
	} else
		motosh_misc_data->mode = mode;

	dev_info(&motosh_misc_data->client->dev,
		 "motosh mode is: %d\n", motosh_misc_data->mode);

	return err;
}

static int motosh_misc_open(struct inode *inode, struct file *file)
{
	int err = 0;
	dev_dbg(&motosh_misc_data->client->dev, "motosh_misc_open\n");

	err = nonseekable_open(inode, file);
	if (err < 0)
		return err;
	file->private_data = motosh_misc_data;

	err = motosh_enable(motosh_misc_data);

	return err;
}

ssize_t motosh_misc_write(struct file *file, const char __user *buff,
				 size_t count, loff_t *ppos)
{
	int tries = COMMAND_RETRIES;
	int index = 0;
	int wait_count = 0;
	int err = 0;

	if (count > MOTOSH_MAXDATA_LENGTH || count == 0) {
		dev_err(&motosh_misc_data->client->dev,
			"Invalid packet size %d\n", (int)count);
		return -EINVAL;
	}

	mutex_lock(&motosh_misc_data->lock);

	if (motosh_bootloader_ver == 0) {
		if (motosh_get_boot_ver() <= 0) {
			err = -EIO;
			goto EXIT;
		}
	}

	if (motosh_misc_data->mode == BOOTMODE) {
		dev_dbg(&motosh_misc_data->client->dev,
			"Starting flash write, %d bytes to address 0x%08x\n",
			(int)count, motosh_misc_data->current_addr);

		if (motosh_bootloader_ver > OLD_BOOT_VER) {
			while (tries) {
				/* Use new bootloader write command */
				err = motosh_boot_cmd_write(motosh_misc_data,
					NO_WAIT_WRITE_MEMORY);
				if (err < 0)
					goto RETRY_WRITE;

				err = motosh_boot_i2c_read(motosh_misc_data,
					motosh_readbuff, 1);
				if (err < 0)
					goto RETRY_WRITE;
				if (motosh_readbuff[0] != ACK_BYTE) {
					dev_err(&motosh_misc_data->client->dev,
							"Error sending WRITE_MEMORY command 0x%02x\n",
							motosh_readbuff[0]);
					goto RETRY_WRITE;
				}

				motosh_cmdbuff[index++]
					= (motosh_misc_data->current_addr >> 24)
					& 0xFF;
				motosh_cmdbuff[index++]
					= (motosh_misc_data->current_addr >> 16)
					& 0xFF;
				motosh_cmdbuff[index++]
					= (motosh_misc_data->current_addr >> 8)
					& 0xFF;
				motosh_cmdbuff[index++]
					= motosh_misc_data->current_addr & 0xFF;
				err = motosh_boot_checksum_write(
					motosh_misc_data, motosh_cmdbuff,
					index);
				if (err < 0)
					goto RETRY_WRITE;
				err = motosh_boot_i2c_read(motosh_misc_data,
					motosh_readbuff, 1);
				if (err < 0)
					goto RETRY_WRITE;
				if (motosh_readbuff[0] != ACK_BYTE) {
					dev_err(&motosh_misc_data->client->dev,
						"Error sending MEMORY_WRITE address 0x%02x\n",
						motosh_readbuff[0]);
					goto RETRY_WRITE;
				}

				motosh_cmdbuff[0] = count - 1;
				if (copy_from_user(&motosh_cmdbuff[1], buff,
						count)) {
					dev_err(&motosh_misc_data->client->dev,
						"Copy from user returned error\n");
					err = -EINVAL;
					goto EXIT;
				}
				if (count & 0x1) {
					motosh_cmdbuff[count + 1] = 0xFF;
					count++;
				}
				err = motosh_boot_checksum_write(
					motosh_misc_data, motosh_cmdbuff,
					count + 1);
				if (err < 0) {
					dev_err(&motosh_misc_data->client->dev,
						"Error sending MEMORY_WRITE data 0x%02x\n",
						motosh_readbuff[0]);
					goto RETRY_WRITE;
				}

				motosh_readbuff[0] = 0;
				do {
					err = motosh_boot_i2c_read(
						motosh_misc_data,
						motosh_readbuff, 1);
					if ((err >= 0) && (motosh_readbuff[0]
							== NACK_BYTE))
						break;
					msleep(WRITE_DELAY);
					wait_count++;
					if (wait_count == WRITE_TIMEOUT)
						break;
				} while (motosh_readbuff[0] != ACK_BYTE);
				if (motosh_readbuff[0] == ACK_BYTE) {
					dev_dbg(&motosh_misc_data->client->dev,
						"MEMORY_WRITE successful\n");
					err = 0;
					break;
				}
				dev_err(&motosh_misc_data->client->dev,
					"Error writing MEMORY_WRITE data 0x%02x\n",
					motosh_readbuff[0]);
RETRY_WRITE:
				dev_dbg(&motosh_misc_data->client->dev,
					"Retry MEMORY_WRITE\n");
				err = -EIO;
				tries--;
				msleep(COMMAND_DELAY);
			}
			if (err < 0)
				goto EXIT;
		} else {
			/* Use old bootloader write command */
			err = motosh_boot_cmd_write(motosh_misc_data,
				WRITE_MEMORY);
			if (err < 0)
				goto EXIT;
			err = motosh_boot_i2c_read(motosh_misc_data,
				motosh_readbuff, 1);
			if (err < 0)
				goto EXIT;
			if (motosh_readbuff[0] != ACK_BYTE) {
				dev_err(&motosh_misc_data->client->dev,
				 "Error sending WRITE_MEMORY command 0x%02x\n",
					motosh_readbuff[0]);
				err = -EIO;
				goto EXIT;
			}

			motosh_cmdbuff[index++]
				= (motosh_misc_data->current_addr >> 24) & 0xFF;
			motosh_cmdbuff[index++]
				= (motosh_misc_data->current_addr >> 16) & 0xFF;
			motosh_cmdbuff[index++]
				= (motosh_misc_data->current_addr >> 8) & 0xFF;
			motosh_cmdbuff[index++]
				= motosh_misc_data->current_addr & 0xFF;
			err = motosh_boot_checksum_write(motosh_misc_data,
				motosh_cmdbuff, index);
			if (err < 0)
				goto EXIT;
			err = motosh_boot_i2c_read(motosh_misc_data,
				motosh_readbuff, 1);
			if (err < 0)
				goto EXIT;
			if (motosh_readbuff[0] != ACK_BYTE) {
				dev_err(&motosh_misc_data->client->dev,
					"Error sending write memory address 0x%02x\n",
					motosh_readbuff[0]);
				err = -EIO;
				goto EXIT;
			}

			motosh_cmdbuff[0] = count - 1;
			if (copy_from_user(&motosh_cmdbuff[1], buff, count)) {
				dev_err(&motosh_misc_data->client->dev,
					"Copy from user returned error\n");
				err = -EINVAL;
				goto EXIT;
			}
			if (count & 0x1) {
				motosh_cmdbuff[count + 1] = 0xFF;
				count++;
			}
			err = motosh_boot_checksum_write(motosh_misc_data,
				motosh_cmdbuff, count + 1);
			if (err < 0)
				goto EXIT;
			err = motosh_boot_i2c_read(motosh_misc_data,
				motosh_readbuff, 1);
			if (err < 0)
				goto EXIT;
			if (motosh_readbuff[0] != ACK_BYTE) {
				dev_err(&motosh_misc_data->client->dev,
					"Error sending flash data 0x%02x\n",
					motosh_readbuff[0]);
				err = -EIO;
				goto EXIT;
			}
		}

		dev_dbg(&motosh_misc_data->client->dev,
			"Flash write completed\n");

		tries = COMMAND_RETRIES;
		while (tries) {
			err = motosh_boot_cmd_write(motosh_misc_data,
				READ_MEMORY);
			if (err < 0)
				goto RETRY_READ;

			err = motosh_boot_i2c_read(motosh_misc_data,
				motosh_readbuff, 1);
			if (err < 0)
				goto RETRY_READ;
			if (motosh_readbuff[0] != ACK_BYTE) {
				dev_err(&motosh_misc_data->client->dev,
				 "Error sending READ_MEMORY command 0x%02x\n",
					motosh_readbuff[0]);
				goto RETRY_READ;
			}

			index = 0;
			motosh_cmdbuff[index++]
				= (motosh_misc_data->current_addr >> 24) & 0xFF;
			motosh_cmdbuff[index++]
				= (motosh_misc_data->current_addr >> 16) & 0xFF;
			motosh_cmdbuff[index++]
				= (motosh_misc_data->current_addr >> 8) & 0xFF;
			motosh_cmdbuff[index++]
				= motosh_misc_data->current_addr & 0xFF;
			err = motosh_boot_checksum_write(motosh_misc_data,
				motosh_cmdbuff, index);
			if (err < 0)
				goto RETRY_READ;
			err = motosh_boot_i2c_read(motosh_misc_data,
				motosh_readbuff, 1);
			if (err < 0)
				goto RETRY_READ;
			if (motosh_readbuff[0] != ACK_BYTE) {
				dev_err(&motosh_misc_data->client->dev,
					"Error sending READ_MEMORY address 0x%02x\n",
					motosh_readbuff[0]);
				goto RETRY_READ;
			}

			err = motosh_boot_cmd_write(motosh_misc_data,
				(count - 1));
			if (err < 0)
				goto RETRY_READ;
			err = motosh_boot_i2c_read(motosh_misc_data,
				motosh_readbuff, 1);
			if (err < 0)
				goto RETRY_READ;
			if (motosh_readbuff[0] != ACK_BYTE) {
				dev_err(&motosh_misc_data->client->dev,
					"Error sending READ_MEMORY count 0x%02x\n",
					motosh_readbuff[0]);
				goto RETRY_READ;
			}

			err = motosh_boot_i2c_read(motosh_misc_data,
				motosh_readbuff, count);
			if (err < 0) {
				dev_err(&motosh_misc_data->client->dev,
					"Error reading READ_MEMORY data 0x%02x\n",
					motosh_readbuff[0]);
				goto RETRY_READ;
			}

			for (index = 0; index < count; index++) {
				if (motosh_readbuff[index] != buff[index]) {
					dev_err(&motosh_misc_data->client->dev,
						"Error verifying write 0x%08x 0x%02x 0x%02x 0x%02x\n",
						motosh_misc_data->current_addr,
						index,
						motosh_readbuff[index],
						buff[index]);
					goto RETRY_READ;
				}
			}
			err = 0;
			dev_dbg(&motosh_misc_data->client->dev,
				"Data compare successful\n");
			break;
RETRY_READ:
			dev_dbg(&motosh_misc_data->client->dev,
				"Retry READ_MEMORY\n");
			err = -EIO;
			tries--;
			msleep(COMMAND_DELAY);
		}
		motosh_misc_data->current_addr += count;
	} else {
		dev_dbg(&motosh_misc_data->client->dev,
			"Normal mode write started\n");
		if (copy_from_user(motosh_cmdbuff, buff, count)) {
			dev_err(&motosh_misc_data->client->dev,
				"Copy from user returned error\n");
			err = -EINVAL;
		}

		motosh_wake(motosh_misc_data);

		if (err == 0)
			err = motosh_i2c_write_no_reset(motosh_misc_data,
				motosh_cmdbuff, count);

		motosh_sleep(motosh_misc_data);

		if (err == 0)
			err = count;
	}

EXIT:
	mutex_unlock(&motosh_misc_data->lock);
	return err;
}


const struct file_operations motosh_misc_fops = {
	.owner = THIS_MODULE,
	.open = motosh_misc_open,
	.unlocked_ioctl = motosh_misc_ioctl,
	.write = motosh_misc_write,
};
