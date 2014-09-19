/*
 * Copyright (C) 2010-2014 Motorola Mobility LLC
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
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/cdev.h>
#include <linux/delay.h>
#include <linux/dma-mapping.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/gpio.h>
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

#include <linux/stml0xx.h>

#define GET_ACK_RETRIES 10
#define GET_ACK_DELAY   20
#define COMMAND_RETRIES 5
#define COMMAND_DELAY   2000

#define SOF_BYTE        0x5A	/* Start of frame byte */
#define ACK_BYTE        0x79
#define BUSY_BYTE       0x76
#define NACK_BYTE       0x1F

#define ERASE_DELAY     200
#define ERASE_TIMEOUT   80

#define RESTART_DELAY   1000
#define WRITE_DELAY     20
#define WRITE_TIMEOUT   20

#define NUM_PAGES       512	/* Number of pages of internal flash */

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
	SPI_SYNC = 0xFF
};

static unsigned char stml0xx_bootloader_ver;

/* Send ACK byte over SPI */
static int stml0xx_boot_spi_send_ack(void)
{
	int rc = 0;
	stml0xx_cmdbuff[0] = ACK_BYTE;
	rc = stml0xx_spi_write_no_reset(stml0xx_cmdbuff, 1);
	if (rc < 0)
		dev_err(&stml0xx_misc_data->spi->dev,
			"Failed to send SPI ACK [%d]", rc);
	return rc;
}

/* Receive ACK byte and respond back with ACK */
static int stml0xx_boot_spi_wait_for_ack(int num_polls, int poll_int_ms,
					 bool send_ack)
{
	/* Poll for ACK */
	int rc = 0;
	while (num_polls--) {
		rc = stml0xx_spi_read_no_reset(stml0xx_readbuff, 1);
		if (rc < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Failed to read from SPI [%d]", rc);
			return rc;
		}
		if (stml0xx_readbuff[0] == ACK_BYTE) {
			/* Received ACK */
			if (send_ack) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Received ACK, sending ACK.");
				return stml0xx_boot_spi_send_ack();
			} else {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Received ACK");
				return rc;
			}
		}
		msleep(poll_int_ms);
	}
	dev_err(&stml0xx_misc_data->spi->dev, "Failed to receive ACK");
	return -EIO;
}

/* Send boot command and receive ACK */
static int stml0xx_boot_cmd_write(enum stm_command command, bool send_ack)
{
	int rc = 0, index = 0;
	/* Send command code with SOF and XOR checksum */
	stml0xx_cmdbuff[index++] = SOF_BYTE;
	if (command == SPI_SYNC) {
		/* No command, SOF only */
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Sending SPI sync byte [0x%02x]", SOF_BYTE);
	} else {
		stml0xx_cmdbuff[index++] = command;
		stml0xx_cmdbuff[index++] = ~command;
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"STML0XX Sending command [0x%02x]", command);
	}

	rc = stml0xx_spi_write_no_reset(stml0xx_cmdbuff, index);
	if (rc < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Failed to send boot command [%d]", rc);
		return rc;
	}
	/* Wait for ACK */
	return stml0xx_boot_spi_wait_for_ack(GET_ACK_RETRIES, GET_ACK_DELAY,
					     send_ack);
}

/* Add checksum byte and write to SPI */
static int stml0xx_boot_checksum_write(unsigned char *buf, int len)
{
	int i;
	unsigned char checksum = 0;

	for (i = 0; i < len; i++)
		checksum ^= buf[i];
	buf[i++] = checksum;

	return stml0xx_spi_write_no_reset(buf, i);
}

/* Get bootloader version */
static int stml0xx_get_boot_ver(void)
{
	int tries = COMMAND_RETRIES;

	while (tries--) {
		/* Send GET_VERSION command */
		if (stml0xx_boot_cmd_write(GET_VERSION, true) < 0)
			goto RETRY_GET_VERSION;

		/* Read bootloader version */
		if ((stml0xx_spi_read_no_reset(stml0xx_readbuff, 3)) < 0)
			goto RETRY_GET_VERSION;

		/* Byte 1: ACK
		   Byte 2: Bootloader version
				( 0 < ver <= 255), eg. 0x10 = ver 1.0
		   Byte 3: ACK */
		if ((stml0xx_readbuff[0] == ACK_BYTE) &&
		    (stml0xx_readbuff[2] == ACK_BYTE)) {
			stml0xx_bootloader_ver = stml0xx_readbuff[1];
			dev_dbg(&stml0xx_misc_data->spi->dev,
				"Bootloader version 0x%02x",
				stml0xx_bootloader_ver);
			/* Send ACK */
			stml0xx_boot_spi_send_ack();
			return stml0xx_bootloader_ver;
		} else {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Error reading GET_VERSION data 0x%02x 0x%02x 0x%02x",
				stml0xx_readbuff[0],
				stml0xx_readbuff[1],
				stml0xx_readbuff[2]);
			goto RETRY_GET_VERSION;
		}
RETRY_GET_VERSION:
		dev_err(&stml0xx_misc_data->spi->dev,
			"Retry get version, retries left [%d]", tries);
		msleep(COMMAND_DELAY);
	}
	return -EIO;
}

/* Erase internal flash */
int stml0xx_boot_flash_erase(void)
{
	int tries = COMMAND_RETRIES;
	int i;

	if (stml0xx_bootloader_ver == 0) {
		if (stml0xx_get_boot_ver() <= 0)
			return -EIO;
	}

	dev_dbg(&stml0xx_misc_data->spi->dev, "Starting flash erase");

	while (tries--) {
		/* Send ERASE command */
		if (stml0xx_boot_cmd_write(ERASE, true) < 0)
			goto RETRY_ERASE;

		/* Send number of pages + checksum
		   (mass erase option unsupported on STM32L0XX) */
		stml0xx_cmdbuff[0] = ((NUM_PAGES - 1) >> 8) & 0xFF;
		stml0xx_cmdbuff[1] = (NUM_PAGES - 1) & 0xFF;
		if (stml0xx_boot_checksum_write(stml0xx_cmdbuff, 2) < 0)
			goto RETRY_ERASE;

		/* Wait for ACK */
		if (stml0xx_boot_spi_wait_for_ack(GET_ACK_RETRIES,
						  GET_ACK_DELAY, true) < 0)
			goto RETRY_ERASE;

		/* Populate 2-byte page numbers (MSB then LSB) */
		for (i = 0; i < NUM_PAGES; i++) {
			stml0xx_cmdbuff[i * 2] = (i >> 8) & 0xFF;
			stml0xx_cmdbuff[(i * 2) + 1] = i & 0xFF;
		}

		/* Add checksum byte */
		if (stml0xx_boot_checksum_write
		    (stml0xx_cmdbuff, (NUM_PAGES * 2))
		    < 0)
			goto RETRY_ERASE;

		/* Wait for ACK */
		if (stml0xx_boot_spi_wait_for_ack(ERASE_TIMEOUT,
						  ERASE_DELAY, true) < 0)
			goto RETRY_ERASE;

		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Flash erase successful");
		return 0;

RETRY_ERASE:
		dev_err(&stml0xx_misc_data->spi->dev,
			"Retry flash erase, retries left [%d]", tries);
		msleep(COMMAND_DELAY);
	}
	return -EIO;
}

/* Get firmware version */
int stml0xx_get_version(struct stml0xx_data *ps_stml0xx)
{
	int rc = 0;
	unsigned char fw_ver;
	if (ps_stml0xx->mode == BOOTMODE) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Tried to read version in boot mode");
		return -EIO;
	}

	stml0xx_wake(ps_stml0xx);
	dev_dbg(&stml0xx_misc_data->spi->dev, "stml0xx_get_version");
	rc = stml0xx_spi_send_read_reg(REV_ID, &fw_ver, 1);
	if (rc >= 0) {
		rc = (int)fw_ver;
		dev_err(&stml0xx_misc_data->spi->dev,
			"STML0XX version %02x", fw_ver);
	}
	stml0xx_sleep(ps_stml0xx);
	return rc;
}

/* Switch to boot or normal mode */
int switch_stml0xx_mode(enum stm_mode mode)
{
	struct stml0xx_platform_data *pdata = stml0xx_misc_data->pdata;
	unsigned int bslen_pin_active_value = pdata->bslen_pin_active_value;
	int tries = COMMAND_RETRIES;
	int rc = 0;

	stml0xx_misc_data->mode = mode;

	/* Set to bootloader mode */
	if (mode == BOOTMODE) {
		/* Set boot pin */
		gpio_set_value(pdata->gpio_bslen, (bslen_pin_active_value));
		stml0xx_g_booted = 0;
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Switching to boot mode");
		msleep(stml0xx_spi_retry_delay);

		/* Toggle reset */
		gpio_set_value(pdata->gpio_reset, 0);
		msleep(stml0xx_spi_retry_delay);
		gpio_set_value(pdata->gpio_reset, 1);
		msleep(STML0XX_RESET_DELAY);

		/* Send SPI bootloader synchronization byte */
		rc = stml0xx_boot_cmd_write(SPI_SYNC, true);
		if (rc < 0) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Failed to send SPI sync byte [%d]", rc);
			return rc;
		}
		dev_dbg(&stml0xx_misc_data->spi->dev, "SPI sync successful");

		/* Check part ID */
		while (tries--) {
			if (stml0xx_boot_cmd_write(GET_ID, true) < 0)
				goto RETRY_GET_ID;

			/* Read part ID
			   Byte 1: ACK
			   Byte 2: N = 1
			   Byte 3: PID (MSB)
			   Byte 4: PID (LSB)
			   Byte 5: ACK */
			if (stml0xx_spi_read_no_reset(stml0xx_readbuff, 5)
				< 0)
				goto RETRY_GET_ID;
			/* Part ID read successfully */
			dev_dbg(&stml0xx_misc_data->spi->dev,
				"Part ID 0x%02x 0x%02x",
				stml0xx_readbuff[2], stml0xx_readbuff[3]);
			/* Send ACK */
			return stml0xx_boot_spi_send_ack();

RETRY_GET_ID:
			dev_err(&stml0xx_misc_data->spi->dev,
				"Retry get part ID, retries left [%d]", tries);
			msleep(COMMAND_DELAY);
		}
		return -EIO;
	} else {
		/* Set to normal mode */
		gpio_set_value(pdata->gpio_bslen, !(bslen_pin_active_value));
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Switching to normal mode");

		stml0xx_reset(pdata, stml0xx_cmdbuff);
	}
	return rc;
}

static int stml0xx_misc_open(struct inode *inode, struct file *file)
{
	int rc = 0;
	dev_dbg(&stml0xx_misc_data->spi->dev, "stml0xx_misc_open");

	rc = nonseekable_open(inode, file);
	if (rc < 0)
		return rc;
	file->private_data = stml0xx_misc_data;

	return stml0xx_enable();
}

ssize_t stml0xx_misc_write(struct file *file, const char __user *buff,
			   size_t count, loff_t *ppos)
{
	int read_retries = COMMAND_RETRIES;
	int write_retries = COMMAND_RETRIES;
	int index = 0;
	int err = 0;
	int bad_byte_cnt = 0;
	int fillers = 0;

	if (count > STML0XX_MAXDATA_LENGTH || count == 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Invalid packet size %d", count);
		return -EINVAL;
	}

	mutex_lock(&stml0xx_misc_data->lock);

	if (stml0xx_bootloader_ver == 0) {
		if (stml0xx_get_boot_ver() <= 0) {
			err = -EIO;
			goto EXIT;
		}
	}

	if (stml0xx_misc_data->mode == BOOTMODE) {
		/* For boot mode */
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Starting flash write, %d bytes to address 0x%08x",
			count, stml0xx_misc_data->current_addr);

BEGIN_WRITE:
		while (write_retries--) {
			/* Send write memory command and receive ACK */
			if (stml0xx_boot_cmd_write(WRITE_MEMORY, true) < 0)
				goto RETRY_WRITE;

			/* Send start address */
			stml0xx_cmdbuff[0] =
			    (stml0xx_misc_data->current_addr >> 24) & 0xFF;
			stml0xx_cmdbuff[1] =
			    (stml0xx_misc_data->current_addr >> 16) & 0xFF;
			stml0xx_cmdbuff[2] =
			    (stml0xx_misc_data->current_addr >> 8) & 0xFF;
			stml0xx_cmdbuff[3] =
			    stml0xx_misc_data->current_addr & 0xFF;
			if (stml0xx_boot_checksum_write(stml0xx_cmdbuff, 4)
				< 0)
				goto RETRY_WRITE;

			/* Wait for ACK */
			if (stml0xx_boot_spi_wait_for_ack(GET_ACK_RETRIES,
							  GET_ACK_DELAY,
							  true) < 0)
				goto RETRY_WRITE;

			/* Number of bytes to write (0-based) */
			stml0xx_cmdbuff[0] = count - 1;

			/* Copy payload */
			if (copy_from_user(&stml0xx_cmdbuff[1], buff, count)) {
				dev_err(&stml0xx_misc_data->spi->dev,
					"Copy from user returned error");
				err = -EINVAL;
				goto EXIT;
			}
			/* Payload must be 32-bit aligned.
			   Add 0xFF fillers if needed. */
			fillers = (4 - (count % 4)) % 4;
			if (fillers) {
				stml0xx_cmdbuff[0] += fillers;
				memset(&stml0xx_cmdbuff[count + 1], 0xFF,
				       fillers);
			}
			/* Number of bytes + payload + checksum */
			err = stml0xx_boot_checksum_write(stml0xx_cmdbuff,
							count + 1 + fillers);
			if (err < 0) {
				dev_err(&stml0xx_misc_data->spi->dev,
				"Error sending MEMORY_WRITE data 0x%02x [%d]",
					stml0xx_readbuff[0], err);
				goto RETRY_WRITE;
			}
			/* Wait for ACK */
			if (stml0xx_boot_spi_wait_for_ack(WRITE_TIMEOUT,
							  WRITE_DELAY,
							  true) < 0)
				goto RETRY_WRITE;

			dev_dbg(&stml0xx_misc_data->spi->dev,
				"MEMORY_WRITE successful");
			err = 0;
			break;
RETRY_WRITE:
			dev_dbg(&stml0xx_misc_data->spi->dev,
				"Retry MEMORY_WRITE, retries left [%d]",
				write_retries);
			err = -EIO;
			msleep(COMMAND_DELAY);
		}
		if (err < 0)
			goto EXIT;

		dev_dbg(&stml0xx_misc_data->spi->dev, "Flash write completed");

		while (read_retries--) {
			/* Send read memory command */
			if (stml0xx_boot_cmd_write(READ_MEMORY, true) < 0)
				goto RETRY_READ;

			/* Send read start address */
			stml0xx_cmdbuff[0]
			    = (stml0xx_misc_data->current_addr >> 24) & 0xFF;
			stml0xx_cmdbuff[1]
			    = (stml0xx_misc_data->current_addr >> 16) & 0xFF;
			stml0xx_cmdbuff[2]
			    = (stml0xx_misc_data->current_addr >> 8) & 0xFF;
			stml0xx_cmdbuff[3]
			    = stml0xx_misc_data->current_addr & 0xFF;
			if (stml0xx_boot_checksum_write(stml0xx_cmdbuff, 4)
				< 0)
				goto RETRY_READ;

			/* Wait for ACK */
			if (stml0xx_boot_spi_wait_for_ack(GET_ACK_RETRIES,
							  GET_ACK_DELAY,
							  true) < 0)
				goto RETRY_READ;

			/* Number of bytes to read (0-based) */
			stml0xx_cmdbuff[0] = count - 1;
			/* XOR checksum */
			stml0xx_cmdbuff[1] = ~(count - 1);
			if (stml0xx_spi_write_no_reset(stml0xx_cmdbuff, 2) < 0)
				goto RETRY_READ;

			/* Wait for ACK */
			if (stml0xx_boot_spi_wait_for_ack(GET_ACK_RETRIES,
							  GET_ACK_DELAY,
							  true) < 0)
				goto RETRY_READ;

			/* Read memory (read 1 extra byte for
			   extra ACK in the beginning) */
			if (stml0xx_spi_read_no_reset
			    (stml0xx_readbuff, count + 1) < 0)
				goto RETRY_READ;

			/* Compare data */
			for (index = 0; index < count; index++) {
				if (stml0xx_readbuff[index + 1]
					!= buff[index]) {
					dev_dbg(&stml0xx_misc_data->spi->dev,
						"Error verifying write 0x%08x 0x%02x 0x%02x 0x%02x",
						stml0xx_misc_data->current_addr,
						index,
						stml0xx_readbuff[index + 1],
						buff[index]);
					bad_byte_cnt++;
				}
			}
			err = 0;
			if (!bad_byte_cnt)
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"Data compare successful");
			else
				dev_err(&stml0xx_misc_data->spi->dev,
					"Write error detected, %d/%d bytes",
					bad_byte_cnt, count);
			break;
RETRY_READ:
			dev_dbg(&stml0xx_misc_data->spi->dev,
				"Retry READ_MEMORY, retries left [%d]",
				read_retries);
			err = -EIO;
			msleep(COMMAND_DELAY);
		}
		if (bad_byte_cnt) {
			bad_byte_cnt = 0;
			dev_dbg(&stml0xx_misc_data->spi->dev,
				"Retry WRITE_MEMORY, retries left [%d]",
				write_retries);
			goto BEGIN_WRITE;
		}
		stml0xx_misc_data->current_addr += count;
	} else {
		/* For normal mode */
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Normal mode write started");
		if (copy_from_user(stml0xx_cmdbuff, buff, count)) {
			dev_err(&stml0xx_misc_data->spi->dev,
				"Copy from user returned error");
			err = -EINVAL;
		}

		if (err == 0) {
			stml0xx_wake(stml0xx_misc_data);
			err =
			    stml0xx_spi_write_no_reset(stml0xx_cmdbuff,
						       (int)count);
			stml0xx_sleep(stml0xx_misc_data);
			err = count;
		}
	}

EXIT:
	mutex_unlock(&stml0xx_misc_data->lock);
	return err;
}

const struct file_operations stml0xx_misc_fops = {
	.owner = THIS_MODULE,
	.open = stml0xx_misc_open,
	.unlocked_ioctl = stml0xx_misc_ioctl,
	.write = stml0xx_misc_write,
};
