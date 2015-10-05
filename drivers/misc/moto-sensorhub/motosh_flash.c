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
#define WRITE_DELAY_US_MIN 2800
#define WRITE_DELAY_US_MAX 3000
#define WRITE_ACK_NACK_TRIES 10
#define RESET_PULSE 20
#define FLASHEN_I2C_DELAY 5

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
unsigned char motosh_flash_cmdbuff[MOTOSH_MAX_PACKET_LENGTH];
unsigned char motosh_flash_readbuff[MOTOSH_MAXDATA_LENGTH];

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

	motosh_flash_cmdbuff[index++] = command;
	motosh_flash_cmdbuff[index++] = ~command;

	return motosh_boot_i2c_write(ps_motosh, motosh_flash_cmdbuff, index);
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
			motosh_flash_readbuff, 1);
		if (err < 0)
			goto RETRY_VER;
		if (motosh_flash_readbuff[0] != ACK_BYTE) {
			dev_err(&motosh_misc_data->client->dev,
				"Error sending GET_VERSION command 0x%02x\n",
				motosh_flash_readbuff[0]);
			goto RETRY_VER;
		}

		err = motosh_boot_i2c_read(motosh_misc_data,
			motosh_flash_readbuff, 1);
		if (err < 0)
			goto RETRY_VER;
		motosh_bootloader_ver = motosh_flash_readbuff[0];
		dev_err(&motosh_misc_data->client->dev,
			"Bootloader version 0x%02x\n", motosh_bootloader_ver);

		err = motosh_boot_i2c_read(motosh_misc_data,
			motosh_flash_readbuff, 1);
		if (err < 0)
			goto RETRY_VER;
		if (motosh_flash_readbuff[0] == ACK_BYTE)
			break;
		dev_err(&motosh_misc_data->client->dev,
			"Error reading GET_VERSION data 0x%02x\n",
			motosh_flash_readbuff[0]);
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

	dev_info(&motosh_misc_data->client->dev,
		"Starting flash erase\n");

	if (motosh_bootloader_ver > OLD_BOOT_VER) {
		/* Use new bootloader erase command */
		while (tries) {
			err = motosh_boot_cmd_write(motosh_misc_data,
				NO_WAIT_ERASE);
			if (err < 0)
				goto RETRY_ERASE;

			err = motosh_boot_i2c_read(motosh_misc_data,
				motosh_flash_readbuff, 1);
			if (err < 0)
				goto RETRY_ERASE;
			if (motosh_flash_readbuff[0] != ACK_BYTE) {
				dev_err(&motosh_misc_data->client->dev,
					"Error sending ERASE command 0x%02x\n",
					motosh_flash_readbuff[0]);
				goto RETRY_ERASE;
			}

			motosh_flash_cmdbuff[index++] = 0xFF;
			motosh_flash_cmdbuff[index++] = 0xFF;
			err = motosh_boot_checksum_write(motosh_misc_data,
				motosh_flash_cmdbuff, index);
			if (err < 0) {
				dev_err(&motosh_misc_data->client->dev,
					"Error wiping checksum %d\n",
					tries);
				goto RETRY_ERASE;
			}
			motosh_flash_readbuff[0] = 0;

			do {
				msleep(ERASE_DELAY);
				err = motosh_boot_i2c_read(motosh_misc_data,
					motosh_flash_readbuff, 1);
				if ((err >= 0) &&
				    (motosh_flash_readbuff[0] == NACK_BYTE))
					break;

				count++;
				if (count == ERASE_TIMEOUT)
					break;

				dev_dbg(&motosh_misc_data->client->dev,
					"Waiting for mass erase, res: %d -- 0x%02X\n",
					err, motosh_flash_readbuff[0]);
			} while (motosh_flash_readbuff[0] != ACK_BYTE);

			if (motosh_flash_readbuff[0] == ACK_BYTE) {
				err = 0;
				dev_dbg(&motosh_misc_data->client->dev,
					"Flash erase successful\n");
				break;
			}
			dev_err(&motosh_misc_data->client->dev,
				"Error waiting for ERASE complete 0x%02x\n",
				motosh_flash_readbuff[0]);
RETRY_ERASE:
			err = -EIO;
			tries--;
			msleep(COMMAND_DELAY);
		}
	} else {
		dev_err(&motosh_misc_data->client->dev,
			"OLD BOOTLOADER NOT SUPPORTED\n");
		err = -EIO;
		goto EXIT;
	}

EXIT:
	return err;
}

int motosh_get_version(struct motosh_data *ps_motosh)
{
	int err = 0;
	char cmdbuff[1];
	char readbuff[1];

	if (ps_motosh->mode == BOOTMODE) {
		dev_err(&ps_motosh->client->dev,
			"Tried to read version in boot mode\n");
		err = -EIO;
		goto EXIT;
	}

	motosh_wake(ps_motosh);

	cmdbuff[0] = REV_ID;
	err = motosh_i2c_write_read_no_reset(ps_motosh, cmdbuff, readbuff,
		1, 1);
	if (err >= 0) {
		err = (int)readbuff[0];
		dev_err(&ps_motosh->client->dev, "MOTOSH version %02x",
			readbuff[0]);
		motosh_g_booted = 1;
	}
	motosh_sleep(ps_motosh);

EXIT:
	return err;
}

int motosh_get_version_str(struct motosh_data *ps_motosh)
{
	int err = 0;
	int len;
	char cmdbuff[1];
	char readbuff[MOTOSH_MAXDATA_LENGTH];

	if (ps_motosh->mode == BOOTMODE) {
		dev_err(&ps_motosh->client->dev,
			"Tried to read version string in boot mode\n");
		err = -EIO;
		goto EXIT;
	}

	motosh_wake(ps_motosh);

	cmdbuff[0] = FW_VERSION_LEN;
	err = motosh_i2c_write_read_no_reset(ps_motosh, cmdbuff, readbuff,
		1, 1);
	if (err >= 0) {
		len = (int)readbuff[0];
		if (len >= FW_VERSION_STR_MAX_LEN)
			len = FW_VERSION_STR_MAX_LEN - 1;
		if (len >= MOTOSH_MAXDATA_LENGTH)
			len = MOTOSH_MAXDATA_LENGTH - 1;

		dev_dbg(&ps_motosh->client->dev,
			"MOTOSH version len %03d", len);
		cmdbuff[0] = FW_VERSION_STR;
		err = motosh_i2c_write_read_no_reset(ps_motosh, cmdbuff,
			readbuff, 1, len);
		if (err >= 0) {
			readbuff[len] = '\0';
			strlcpy(ps_motosh->pdata->fw_version_str,
				readbuff, FW_VERSION_STR_MAX_LEN);
			dev_info(&ps_motosh->client->dev, "MOTOSH version str %s",
						readbuff);
			motosh_g_booted = 1;
		}
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

	motosh_misc_data->wake_work_delay = 0;

	/* bootloader mode */
	if (mode == BOOTMODE) {
		/* revert back to non-booted. This prevents TCMD from
		   trying to access over I2C or ioctl during desk testing
		   when a flash upgrade is being peformed. */
		motosh_g_booted = 0;

		motosh_misc_data->mode = mode;
		dev_dbg(&motosh_misc_data->client->dev,
			"Switching to boot mode\n");

		while (tries) {
			/* Assert reset and flash enable and release */
			gpio_set_value(pdata->gpio_reset, 0);
			msleep(RESET_PULSE);
			gpio_set_value(pdata->gpio_bslen,
				       (bslen_pin_active_value));
			msleep(RESET_PULSE);
			gpio_set_value(pdata->gpio_reset, 1);
			msleep(FLASHEN_I2C_DELAY);

			/* de-assert boot pin to reduce chance of collision
			   with modemm authentication on P2B Clark - hardware
			   mistakenly shared this GPIO
			*/
			gpio_set_value(pdata->gpio_bslen,
				       !(bslen_pin_active_value));

			err = motosh_boot_cmd_write(motosh_misc_data, GET_ID);
			if (err < 0)
				goto RETRY_ID;

			err = motosh_boot_i2c_read(motosh_misc_data,
						   motosh_flash_readbuff, 1);
			if (err < 0)
				goto RETRY_ID;

			if (motosh_flash_readbuff[0] != ACK_BYTE) {
				dev_err(&motosh_misc_data->client->dev,
					"Error sending GET_ID command 0x%02x\n",
					motosh_flash_readbuff[0]);
				goto RETRY_ID;
			}

			err = motosh_boot_i2c_read(motosh_misc_data,
						   motosh_flash_readbuff, 3);
			if (err < 0)
				goto RETRY_ID;

			dev_err(&motosh_misc_data->client->dev,
				"Part ID 0x%02x 0x%02x 0x%02x\n",
				motosh_flash_readbuff[0],
				motosh_flash_readbuff[1],
				motosh_flash_readbuff[2]);

			err = motosh_boot_i2c_read(motosh_misc_data,
						   motosh_flash_readbuff, 1);
			if (err < 0)
				goto RETRY_ID;

			if (motosh_flash_readbuff[0] == ACK_BYTE)
				break;
			dev_err(&motosh_misc_data->client->dev,
				"Error reading GETID data 0x%02x\n",
				motosh_flash_readbuff[0]);
RETRY_ID:
			err = -EIO;
			tries--;

			/* Exit flash mode so we can re-enter on a retry
			   Assert reset de-assert flash-enable and release */
			gpio_set_value(pdata->gpio_reset, 0);
			msleep(RESET_PULSE);
			gpio_set_value(pdata->gpio_reset, 1);

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
			motosh_reset_and_init(START_RESET);
		} else {
			motosh_misc_data->mode = mode;
			motosh_reset(pdata, motosh_flash_cmdbuff);
		}
	} else
		motosh_misc_data->mode = mode;

	dev_dbg(&motosh_misc_data->client->dev,
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
				if (err < 0) {
					dev_err(&motosh_misc_data->client->dev,
						"error NO_WAIT_WRITE_MEMORY\n");
					goto RETRY_WRITE;
				}

				err = motosh_boot_i2c_read(motosh_misc_data,
					motosh_flash_readbuff, 1);
				if (err < 0) {
					dev_err(&motosh_misc_data->client->dev,
						"error NO_WAIT_WRITE_MEMORY read\n");
					goto RETRY_WRITE;
				}

				if (motosh_flash_readbuff[0] != ACK_BYTE) {
					dev_err(&motosh_misc_data->client->dev,
						"Error sending WRITE_MEMORY command 0x%02x\n",
						motosh_flash_readbuff[0]);
					goto RETRY_WRITE;
				}

				motosh_flash_cmdbuff[index++]
					= (motosh_misc_data->current_addr >> 24)
					& 0xFF;
				motosh_flash_cmdbuff[index++]
					= (motosh_misc_data->current_addr >> 16)
					& 0xFF;
				motosh_flash_cmdbuff[index++]
					= (motosh_misc_data->current_addr >> 8)
					& 0xFF;
				motosh_flash_cmdbuff[index++]
					= motosh_misc_data->current_addr & 0xFF;
				err = motosh_boot_checksum_write(
					motosh_misc_data, motosh_flash_cmdbuff,
					index);
				if (err < 0)
					goto RETRY_WRITE;

				err = motosh_boot_i2c_read(motosh_misc_data,
					motosh_flash_readbuff, 1);
				if (err < 0)
					goto RETRY_WRITE;
				if (motosh_flash_readbuff[0] != ACK_BYTE) {
					dev_err(&motosh_misc_data->client->dev,
						"Error sending MEMORY_WRITE address 0x%02x\n",
						motosh_flash_readbuff[0]);
					goto RETRY_WRITE;
				}

				motosh_flash_cmdbuff[0] = count - 1;
				if (copy_from_user(&motosh_flash_cmdbuff[1],
						buff, count)) {
					dev_err(&motosh_misc_data->client->dev,
						"Copy from user returned error\n");
					err = -EINVAL;
					goto EXIT;
				}
				if (count & 0x1) {
					motosh_flash_cmdbuff[count + 1] = 0xFF;
					count++;
				}

				err = motosh_boot_checksum_write(
					motosh_misc_data, motosh_flash_cmdbuff,
					count + 1);
				if (err < 0) {
					dev_err(&motosh_misc_data->client->dev,
						"Error sending MEMORY_WRITE data 0x%02x\n",
						motosh_flash_readbuff[0]);
					goto RETRY_WRITE;
				}

				motosh_flash_readbuff[0] = 0;
				do {
					usleep_range(WRITE_DELAY_US_MIN,
						WRITE_DELAY_US_MAX);

					err = motosh_boot_i2c_read(
						motosh_misc_data,
						motosh_flash_readbuff, 1);

					/* successfully received a NACK */
					if ((err >= 0) &&
					    (motosh_flash_readbuff[0]
							== NACK_BYTE))
						break;

					wait_count++;
					if (wait_count == WRITE_ACK_NACK_TRIES)
						break;
				} while (motosh_flash_readbuff[0] != ACK_BYTE);

				if (motosh_flash_readbuff[0] == ACK_BYTE) {
					dev_dbg(&motosh_misc_data->client->dev,
						"MEMORY_WRITE successful\n");
					err = 0;
					break;
				}

				/* If I2C Error or NACK_BYTE */
				dev_err(&motosh_misc_data->client->dev,
					"Error writing MEMORY_WRITE i2c: %d data 0x%02x\n",
					err,
					motosh_flash_readbuff[0]);
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
			dev_err(&motosh_misc_data->client->dev,
				"OLD BOOTLOADER NOT SUPPORTED\n");
			err = -EIO;
			goto EXIT;
		}

		dev_dbg(&motosh_misc_data->client->dev,
			"Flash write completed\n");

		motosh_misc_data->current_addr += count;
	} else {
		dev_dbg(&motosh_misc_data->client->dev,
			"Normal mode write started\n");
		if (copy_from_user(motosh_flash_cmdbuff, buff, count)) {
			dev_err(&motosh_misc_data->client->dev,
				"Copy from user returned error\n");
			err = -EINVAL;
		}

		motosh_wake(motosh_misc_data);

		if (err == 0)
			err = motosh_i2c_write_no_reset(motosh_misc_data,
				motosh_flash_cmdbuff, count);

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
#ifdef CONFIG_COMPAT
	.compat_ioctl = motosh_misc_ioctl,
#endif
	.write = motosh_misc_write,
};
