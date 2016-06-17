/*
 * Copyright (C) 2014 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
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

int stml0xx_spi_sensorhub_ready(void)
{
	int timeout = SPI_SENSORHUB_TIMEOUT;

	/* wait for the sensorhub to signal it's ready */
	while (!gpio_get_value
	       (stml0xx_misc_data->pdata->gpio_spi_ready_for_receive)
	       && --timeout) {
		udelay(1);
	}

	if (timeout <= 0)
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"data ready timeout: %d", timeout);
	else
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"got data ready! %d", timeout);

	return timeout;
}

int stml0xx_spi_sensorhub_ack(void)
{
	return gpio_get_value(stml0xx_misc_data->pdata->gpio_spi_data_ack);
}

/* Transfer data over SPI */
int stml0xx_spi_transfer(unsigned char *tx_buf, unsigned char *rx_buf, int len)
{
	struct spi_message msg;
	struct spi_transfer transfer;
	int rc;

	if ((!tx_buf && !rx_buf) || len == 0)
		return -EFAULT;

	mutex_lock(&stml0xx_misc_data->spi_lock);

	if (!rx_buf)
		rx_buf = stml0xx_misc_data->spi_rx_buf;

	memset(rx_buf, 0, len);
	memset(&transfer, 0, sizeof(transfer));
	spi_message_init(&msg);
	transfer.tx_buf = tx_buf;
	transfer.rx_buf = rx_buf;
	transfer.len = len;
	transfer.bits_per_word = 8;
	transfer.delay_usecs = 0;

	if (stml0xx_misc_data->mode == BOOTMODE) {
		/* boot mode clock speed */
		transfer.speed_hz = SPI_FLASH_CLK_SPD_HZ;
	} else {
		transfer.speed_hz = SPI_NORMAL_CLK_SPD_HZ;
	}

	spi_message_add_tail(&transfer, &msg);

	if (stml0xx_misc_data->mode != BOOTMODE) {
		if (stml0xx_spi_sensorhub_ready() <= 0) {
			dev_dbg(&stml0xx_misc_data->spi->dev,
				"SPI error STM not ready");
			rc = -EIO;
			goto EXIT;
		}
	}

	rc = spi_sync(stml0xx_misc_data->spi, &msg);

	if (stml0xx_misc_data->mode != BOOTMODE) {
		if (rc >= 0) {
			/* wait for the hub to process the message
			   and ack/nack */
			stml0xx_spi_sensorhub_ready();

			/* check for sensorhub ack */
			if (!stml0xx_spi_sensorhub_ack()) {
				rc = -EIO;
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"SPI transfer error NACK");
			}
		}
	}

	if (rc >= 0) {
		/* Successful write */
#if ENABLE_VERBOSE_LOGGING
		int i;
		for (i = 0; i < len; i++) {
			if (tx_buf) {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"SPI Write 0x%02x, Read 0x%02x",
					tx_buf[i], rx_buf[i]);
			} else {
				dev_dbg(&stml0xx_misc_data->spi->dev,
					"SPI ----------, Read 0x%02x",
					rx_buf[i]);
			}
		}
#endif

		goto EXIT;
	}
EXIT:
	mutex_unlock(&stml0xx_misc_data->spi_lock);
	return rc;
}

/* Write then read over SPI */
static int stml0xx_spi_write_read_no_reset_no_retries(unsigned char *tx_buf,
					       int tx_len,
					       unsigned char *rx_buf,
					       int rx_len)
{
	int rc = stml0xx_spi_transfer(tx_buf, NULL, tx_len);
	if (rc >= 0)
		rc = stml0xx_spi_transfer(NULL, rx_buf, rx_len);
	return rc;
}

int stml0xx_spi_write_read_no_reset(unsigned char *tx_buf, int tx_len,
				    unsigned char *rx_buf, int rx_len)
{
	int tries = 0;
	int ret = -EIO;

	for (tries = 0; tries < SPI_RETRIES && ret < 0; tries++) {
		ret =
		    stml0xx_spi_write_read_no_reset_no_retries(tx_buf, tx_len,
							       rx_buf, rx_len);
		if (ret < 0 && stml0xx_misc_data->mode == BOOTMODE)
			msleep(SPI_RETRY_DELAY);
	}

	if (tries == SPI_RETRIES) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"SPI write_read_no_reset error [%d]", ret);
	}

	return ret;
}

/* Read bytes from SPI */
static int stml0xx_spi_read_no_reset_no_retries(unsigned char *buf, int len)
{
	return stml0xx_spi_transfer(NULL, buf, len);
}

int stml0xx_spi_read_no_reset(unsigned char *buf, int len)
{
	int tries;
	int ret = -EIO;

	for (tries = 0; tries < SPI_RETRIES && ret < 0; tries++) {
		ret = stml0xx_spi_read_no_reset_no_retries(buf, len);
		if (ret < 0 && stml0xx_misc_data->mode == BOOTMODE)
			msleep(SPI_RETRY_DELAY);
	}

	if (tries == SPI_RETRIES) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"SPI read_no_reset error [%d]", ret);
	}

	return ret;
}

/* Write bytes over SPI */
static int stml0xx_spi_write_no_reset_no_retries(unsigned char *buf, int len)
{
	return stml0xx_spi_transfer(buf, NULL, len);
}

int stml0xx_spi_write_no_reset(unsigned char *buf, int len)
{
	int tries = 0;
	int ret = -EIO;

	for (tries = 0; tries < SPI_RETRIES && ret < 0; tries++) {
		ret = stml0xx_spi_write_no_reset_no_retries(buf, len);
		if (ret < 0 && stml0xx_misc_data->mode == BOOTMODE)
			msleep(SPI_RETRY_DELAY);
	}

	if (tries == SPI_RETRIES) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"SPI write_no_reset error [%d]", ret);
	}

	return ret;
}

static int stml0xx_spi_write_read_no_retries(unsigned char *tx_buf, int tx_len,
				      unsigned char *rx_buf, int rx_len)
{
	int err = 0;

	if (stml0xx_misc_data->mode == BOOTMODE)
		return -EFAULT;

	if (tx_buf == NULL || rx_buf == NULL || tx_len == 0 || rx_len == 0)
		return -EFAULT;

	err = stml0xx_spi_write_read_no_reset_no_retries(tx_buf, tx_len, rx_buf,
							 rx_len);

	if (err < 0) {
		dev_dbg(&stml0xx_misc_data->spi->dev, "write_read error");
		err = -EIO;
	} else {
		dev_dbg(&stml0xx_misc_data->spi->dev, "write_read successful:");
	}

	return err;
}

int stml0xx_spi_write_read(unsigned char *tx_buf, int tx_len,
			   unsigned char *rx_buf, int rx_len)
{
	int tries;
	int ret = -EIO;

	if (stml0xx_misc_data->mode == BOOTMODE)
		return -EFAULT;

	if (tx_buf == NULL || rx_buf == NULL || tx_len == 0 || rx_len == 0)
		return -EFAULT;

	for (tries = 0; tries < SPI_RETRIES && ret < 0; tries++) {
		ret = stml0xx_spi_write_read_no_retries(tx_buf, tx_len,
							rx_buf, rx_len);
	}

	if (ret < 0) {
		stml0xx_reset(stml0xx_misc_data->pdata);
		dev_err(&stml0xx_misc_data->spi->dev,
			"SPI write_read error [%d]", ret);
	}

	return ret;
}

static int stml0xx_spi_read_no_retries(unsigned char *buf, int len)
{
	int err = 0;

	if (buf == NULL || len == 0)
		return -EFAULT;

	if (stml0xx_misc_data->mode == BOOTMODE)
		return -EFAULT;

	err = stml0xx_spi_read_no_reset_no_retries(buf, len);

	if (err < 0) {
		dev_dbg(&stml0xx_misc_data->spi->dev, "read error");
		err = -EIO;
	} else {
		dev_dbg(&stml0xx_misc_data->spi->dev, "read successful:");
	}
	return err;
}

int stml0xx_spi_read(unsigned char *buf, int len)
{
	int tries;
	int ret = -EIO;

	if (buf == NULL || len == 0)
		return -EFAULT;

	if (stml0xx_misc_data->mode == BOOTMODE)
		return -EFAULT;

	for (tries = 0; tries < SPI_RETRIES && ret < 0; tries++)
		ret = stml0xx_spi_read_no_retries(buf, len);

	if (ret < 0) {
		stml0xx_reset(stml0xx_misc_data->pdata);
		dev_err(&stml0xx_misc_data->spi->dev,
			"SPI read error [%d]", ret);
	}

	return ret;
}

static int stml0xx_spi_write_no_retries(unsigned char *buf, int len)
{
	int err = 0;

	if (stml0xx_misc_data->mode == BOOTMODE)
		return -EFAULT;

	err = stml0xx_spi_write_no_reset_no_retries(buf, len);

	if (err < 0) {
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"write error - %x", buf[0]);
		err = -EIO;
	} else {
		dev_dbg(&stml0xx_misc_data->spi->dev, "write successful");
	}
	return err;
}

int stml0xx_spi_write(unsigned char *buf, int len)
{
	int tries;
	int ret = -EIO;

	if (stml0xx_misc_data->mode == BOOTMODE)
		return -EFAULT;

	for (tries = 0; tries < SPI_RETRIES && ret < 0; tries++)
		ret = stml0xx_spi_write_no_retries(buf, len);

	if (ret < 0) {
		stml0xx_reset(stml0xx_misc_data->pdata);
		dev_err(&stml0xx_misc_data->spi->dev,
			"SPI write error [%d]", ret);
	}

	return ret;
}

static int stml0xx_spi_send_write_reg_no_retries(unsigned char reg_type,
					  unsigned char *reg_data,
					  int reg_size)
{

	int data_offset = 0, data_size;
	int remaining_data_size = reg_size;
	int ret = 0;
	unsigned char cmd_buff[SPI_MSG_SIZE];

	if (!reg_data || reg_size <= 0 ||
	    reg_size > STML0XX_MAX_WRITE_REG_LEN) {
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Invalid buffer or buffer size");
		ret = -EFAULT;
		goto EXIT;
	}

	while (remaining_data_size > 0) {
		/* Clear buffer */
		memset(cmd_buff, 0, SPI_MSG_SIZE);

		/* Populate header */
		cmd_buff[0] = SPI_BARKER_1;
		cmd_buff[1] = SPI_BARKER_2;
		cmd_buff[2] = SPI_MSG_TYPE_WRITE_REG;
		cmd_buff[3] = reg_type;

		/* Payload data offset */
		cmd_buff[4] = data_offset;
		data_size = (remaining_data_size < SPI_TX_PAYLOAD_LEN) ?
		    remaining_data_size : SPI_TX_PAYLOAD_LEN;
		cmd_buff[5] = data_size;

		/* Copy data and update data parameters */
		memcpy(&cmd_buff[SPI_TX_HDR_SIZE],
		       reg_data + data_offset, data_size);
		remaining_data_size -= data_size;
		data_offset += data_size;

		/* Swap all bytes */
		stml0xx_spi_swap_bytes(cmd_buff,
				       SPI_MSG_SIZE - SPI_CRC_SIZE);

		/* Append 2-byte CRC (unswapped) */
		stml0xx_spi_append_crc(cmd_buff,
				       SPI_MSG_SIZE - SPI_CRC_SIZE);

		/* Write write request to SPI */
		ret =
		    stml0xx_spi_write_no_reset_no_retries(cmd_buff,
							  SPI_MSG_SIZE);
		if (ret < 0)
			goto EXIT;
	}

EXIT:
	return ret;
}

int stml0xx_spi_send_write_reg(unsigned char reg_type,
			       unsigned char *reg_data, int reg_size)
{
	return stml0xx_spi_send_write_reg_reset(reg_type, reg_data,
	    reg_size, RESET_ALLOWED);
}

int stml0xx_spi_send_write_reg_reset(unsigned char reg_type,
			       unsigned char *reg_data, int reg_size,
			       uint8_t reset_allowed)
{
	int tries;
	int ret = -EIO;

	if (!reg_data || reg_size <= 0 ||
	    reg_size > STML0XX_MAX_WRITE_REG_LEN) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Invalid buffer or buffer size");
		ret = -EFAULT;
		return ret;
	}

	for (tries = 0; tries < SPI_RETRIES && ret < 0; tries++) {
		ret = stml0xx_spi_send_write_reg_no_retries(reg_type,
			reg_data, reg_size);
	}

	if (ret < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"SPI write_reg_reset error [%d]", ret);
		if (reset_allowed)
			stml0xx_reset(stml0xx_misc_data->pdata);
	}

	return ret;
}

static int stml0xx_spi_send_read_reg_no_retries(enum sh_spi_msg spi_msg_type,
				unsigned char reg_type,
				unsigned char *reg_data,
				int reg_size)
{
	int data_offset = 0, data_size;
	int remaining_data_size = reg_size;
	unsigned short recv_crc, calc_crc;
	int ret = 0;
	unsigned char cmd_buff[SPI_MSG_SIZE];
	unsigned char read_buff[SPI_MSG_SIZE];

	if (!reg_data || reg_size <= 0 ||
	    reg_size > SPI_RX_PAYLOAD_LEN) {
		dev_dbg(&stml0xx_misc_data->spi->dev,
			"Invalid buffer or buffer size");
		ret = -EFAULT;
		goto EXIT;
	}

	while (remaining_data_size > 0) {
		/* Clear buffer */
		memset(cmd_buff, 0, SPI_MSG_SIZE);

		/* Populate header */
		cmd_buff[0] = SPI_BARKER_1;
		cmd_buff[1] = SPI_BARKER_2;
		cmd_buff[2] = spi_msg_type;
		cmd_buff[3] = reg_type;

		/* Payload data offset */
		cmd_buff[4] = data_offset;

		/* Hub doesn't send the header when transmitting
		 * so we can read more than we can write */
		data_size = (remaining_data_size < SPI_RX_PAYLOAD_LEN) ?
		    remaining_data_size : SPI_RX_PAYLOAD_LEN;
		cmd_buff[5] = data_size;

		/* Swap all bytes */
		stml0xx_spi_swap_bytes(cmd_buff,
			SPI_MSG_SIZE - SPI_CRC_SIZE);

		/* Append 2-byte CRC (unswapped) */
		stml0xx_spi_append_crc(cmd_buff,
			SPI_MSG_SIZE - SPI_CRC_SIZE);

		/* Write read request to SPI */
		ret = stml0xx_spi_write_no_reset_no_retries(cmd_buff,
							  SPI_MSG_SIZE);
		if (ret < 0)
			goto EXIT;

		ret = stml0xx_spi_read_no_reset_no_retries(read_buff,
							 SPI_MSG_SIZE);

		if (ret < 0)
			goto EXIT;

		/* Validate CRC */
		recv_crc = (read_buff[SPI_MSG_SIZE - 2] << 8) |
				read_buff[SPI_MSG_SIZE - 1];
		calc_crc = stml0xx_spi_calculate_crc(read_buff,
						     SPI_MSG_SIZE -
						     SPI_CRC_SIZE);
		if (recv_crc != calc_crc) {
			dev_dbg(&stml0xx_misc_data->spi->dev,
				"CRC mismatch [0x%02x/0x%02x]. Discarding data.",
				recv_crc, calc_crc);
			ret = -EIO;
			goto EXIT;
		}

		/* Swap bytes before extracting data */
		stml0xx_spi_swap_bytes(read_buff,
				       SPI_MSG_SIZE - SPI_CRC_SIZE);

		/* Extract payload */
		memcpy(reg_data + data_offset, read_buff, data_size);
		remaining_data_size -= data_size;
		data_offset += data_size;
	}
EXIT:
	return ret;
}

int stml0xx_spi_send_read_reg(unsigned char reg_type,
			      unsigned char *reg_data, int reg_size)
{
	return stml0xx_spi_send_read_reg_reset(reg_type, reg_data,
	    reg_size, RESET_ALLOWED);
}

int stml0xx_spi_send_read_reg_reset(unsigned char reg_type,
			      unsigned char *reg_data, int reg_size,
			      uint8_t reset_allowed)
{
	int tries;
	int ret = -EIO;

	if (!reg_data || reg_size <= 0 ||
	    reg_size > SPI_RX_PAYLOAD_LEN) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Invalid buffer or buffer size");
		ret = -EFAULT;
		return ret;
	}

	for (tries = 0; tries < SPI_RETRIES && ret < 0; tries++) {
		ret = stml0xx_spi_send_read_reg_no_retries(
			SPI_MSG_TYPE_READ_REG, reg_type, reg_data, reg_size);
	}

	if (ret < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"SPI read_reg_reset error [%d]", ret);
		if (reset_allowed)
			stml0xx_reset(stml0xx_misc_data->pdata);
	}

	return ret;
}

int stml0xx_spi_read_msg_data(enum sh_spi_msg spi_msg,
				unsigned char *data_buffer,
				int buffer_size,
				enum reset_option reset_allowed) {
	int tries;
	int ret = -EIO;

	if (!data_buffer || buffer_size <= 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"Invalid buffer or buffer size");
		ret = -EFAULT;
		return ret;
	}

	for (tries = 0; tries < SPI_RETRIES && ret < 0; tries++) {
		ret = stml0xx_spi_send_read_reg_no_retries(spi_msg,
				0, data_buffer, buffer_size);
	}

	if (ret < 0) {
		dev_err(&stml0xx_misc_data->spi->dev,
			"SPI read_msg_data error [%d]", ret);
		if (reset_allowed)
			stml0xx_reset(stml0xx_misc_data->pdata);
	}

	return ret;
}

void stml0xx_spi_swap_bytes(unsigned char *data, int size)
{
	unsigned short *temp = (unsigned short *)data;
	int len = size / 2;
	int i;

	for (i = 0; i < len; i++)
		temp[i] = (temp[i] >> 8) | (temp[i] << 8);
}
