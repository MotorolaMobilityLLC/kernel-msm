/*
 * ILITEK Touch IC driver
 *
 * Copyright (C) 2011 ILI Technology Corporation.
 *
 * Author: Dicky Chiang <dicky_chiang@ilitek.com>
 * Based on TDD v7.0 implemented by Mstar & ILITEK
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 *
 */

#include "../common.h"
#include "config.h"
#include "spi.h"
#include "finger_report.h"

struct core_spi_data *core_spi;

int core_Rx_check(uint16_t check)
{
	int size = 0, i, count = 100;
	uint8_t txbuf[5] = { 0 }, rxbuf[4] = {0};
	uint16_t status = 0;

	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x94;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;

	for (i = 0 ; i < count; i++) {

		txbuf[0] = SPI_WRITE;
		if (spi_write_then_read(core_spi->spi, txbuf, 5, txbuf, 0) < 0) {
			size = -EIO;
			ipio_err("spi Write Error, res = %d\n", size);
		}

		txbuf[0] = SPI_READ;
		if (spi_write_then_read(core_spi->spi, txbuf, 1, rxbuf, 4) < 0) {
			size = -EIO;
			ipio_err("spi Read Error, res = %d\n", size);
		}

		status = (rxbuf[2] << 8) + rxbuf[3];
		size = (rxbuf[0] << 8) + rxbuf[1];

		if (status == check)
			return size;

		mdelay(1);
	}

	size = -EIO;
	ipio_err("Check lock error\n");
	return size;
}

int core_Tx_unlock_check(void)
{
	int res = 0, i, count = 100;
	uint8_t txbuf[5] = { 0 }, rxbuf[4] = {0};
	uint16_t unlock = 0;

	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x0;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;

	for (i = 0; i < count; i++) {
		txbuf[0] = SPI_WRITE;
		if (spi_write_then_read(core_spi->spi, txbuf, 5, txbuf, 0) < 0) {
			res = -EIO;
			ipio_err("spi Write Error, res = %d\n", res);
		}

		txbuf[0] = SPI_READ;
		if (spi_write_then_read(core_spi->spi, txbuf, 1, rxbuf, 4) < 0) {
			res = -EIO;
			ipio_err("spi Read Error, res = %d\n", res);
		}

		unlock = (rxbuf[2] << 8) + rxbuf[3];

		if (unlock == 0x9881)
			return res;

		mdelay(1);
	}

	ipio_err("Check unlock error\n");
	return res;
}

int core_ice_mode_read_9881H11(uint8_t *data, uint32_t size)
{
	int res = 0;
	uint8_t txbuf[64] = { 0 };

	/* set read address */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x98;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;
	if (spi_write_then_read(core_spi->spi, txbuf, 5, txbuf, 0) < 0) {
		res = -EIO;
		return res;
	}

	/* read data */
	txbuf[0] = SPI_READ;
	if (spi_write_then_read(core_spi->spi, txbuf, 1, data, size) < 0) {
		res = -EIO;
		return res;
	}

	/* write data unlock */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x94;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;
	txbuf[5] = (size & 0xFF00) >> 8;
	txbuf[6] = size & 0xFF;
	txbuf[7] = (char)0x98;
	txbuf[8] = (char)0x81;
	if (spi_write_then_read(core_spi->spi, txbuf, 9, txbuf, 0) < 0) {
		res = -EIO;
		ipio_err("spi Write data unlock error, res = %d\n", res);
	}

	return res;
}

int core_ice_mode_write_9881H11(uint8_t *data, uint32_t size)
{
	int res = 0;
	uint8_t check_sum = 0, wsize = 0;
	uint8_t *txbuf = NULL;

    txbuf = kcalloc(size + 9, sizeof(uint8_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(txbuf)) {
		ipio_err("Failed to allocate txbuf\n");
		res = -ENOMEM;
		goto out;
	}

	/* Write data */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x4;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;

	/* Calcuate checsum and fill it in the last byte */
	check_sum = core_fr_calc_checksum(data, size);
	memcpy(txbuf + 5, data, size);
	txbuf[5 + size] = check_sum;
	size++;
	wsize = size;
	if (wsize % 4 != 0)
		wsize += 4 - (wsize % 4);

	if (spi_write_then_read(core_spi->spi, txbuf, wsize + 5, txbuf, 0) < 0) {
		res = -EIO;
		ipio_err("spi Write Error, res = %d\n", res);
		goto out;
	}

	/* write data lock */
	txbuf[0] = SPI_WRITE;
	txbuf[1] = 0x25;
	txbuf[2] = 0x0;
	txbuf[3] = 0x0;
	txbuf[4] = 0x2;
	txbuf[5] = (size & 0xFF00) >> 8;
	txbuf[6] = size & 0xFF;
	txbuf[7] = (char)0x5A;
	txbuf[8] = (char)0xA5;
	if (spi_write_then_read(core_spi->spi, txbuf, 9, txbuf, 0) < 0) {
		res = -EIO;
		ipio_err("spi Write data lock Error, res = %d\n", res);
	}

out:
	ipio_kfree((void **)&txbuf);
	return res;
}

int core_ice_mode_disable_9881H11(void)
{
	int res = 0;
	uint8_t txbuf[5] = {0};

	txbuf[0] = 0x82;
	txbuf[1] = 0x1B;
	txbuf[2] = 0x62;
	txbuf[3] = 0x10;
	txbuf[4] = 0x18;

	if (spi_write_then_read(core_spi->spi, txbuf, 5, txbuf, 0) < 0) {
		res = -EIO;
		ipio_err("spi_write_then_read Error, res = %d\n", res);
	}

	return res;
}

int core_ice_mode_enable_9881H11(void)
{
	int res = 0;
	uint8_t txbuf[5] = {0}, rxbuf[2] = {0};

	txbuf[0] = 0x82;
	txbuf[1] = 0x1F;
	txbuf[2] = 0x62;
	txbuf[3] = 0x10;
	txbuf[4] = 0x18;

	if (spi_write_then_read(core_spi->spi, txbuf, 1, rxbuf, 1) < 0) {
		res = -EIO;
		ipio_err("spi Write Error, res = %d\n", res);
		return res;
	}

	/* check recover data */
	if (rxbuf[0] == 0x82) {
		ipio_info("rxbuf:0x%x\n", rxbuf[0]);
		return CHECK_RECOVER;
	}

	if (spi_write_then_read(core_spi->spi, txbuf, 5, rxbuf, 0) < 0) {
		res = -EIO;
		ipio_err("spi Write Error, res = %d\n", res);
	}

	return res;
}

int core_spi_read_9881H11(uint8_t *pBuf, uint16_t nSize)
{
	int res = 0, size = 0;

	res = core_ice_mode_enable_9881H11();
	if (res < 0) {
		goto out;
	}

	size = core_Rx_check(0x5AA5);
	if (size < 0) {
		res = -EIO;
		goto out;
	}

	if (core_ice_mode_read_9881H11(pBuf, size) < 0) {
		res = -EIO;
		goto out;
	}

	if (core_ice_mode_disable_9881H11() < 0) {
		res = -EIO;
		goto out;
	}

out:
	return res;
}

int core_spi_write_9881H11(uint8_t *pBuf, uint16_t nSize)
{
	int res = 0;
	uint8_t *txbuf = NULL;

    txbuf = kcalloc(nSize + 5, sizeof(uint8_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(txbuf)) {
		ipio_err("Failed to allocate txbuf\n");
		return -ENOMEM;
		goto out;
	}

	res = core_ice_mode_enable_9881H11();
	if (res < 0)
		return res;

	if (core_ice_mode_write_9881H11(pBuf, nSize) < 0) {
		res = -EIO;
		goto out;
	}

	if (core_Tx_unlock_check() < 0) {
		res = -ETXTBSY;
	}

out:
	ipio_kfree((void **)&txbuf);
	return res;
}

int core_spi_write(uint8_t *pBuf, uint16_t nSize)
{
	int res = 0;
	uint8_t *txbuf = NULL;

    txbuf = kcalloc(nSize + 1, sizeof(uint8_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(txbuf)) {
		ipio_err("Failed to allocate txbuf\n");
		return -ENOMEM;
		goto out;
	}

	if (core_config->icemodeenable == false) {
		res = core_spi_write_9881H11(pBuf, nSize);
		core_ice_mode_disable_9881H11();
		ipio_kfree((void **)&txbuf);
		return res;
	}

	txbuf[0] = SPI_WRITE;
    memcpy(txbuf+1, pBuf, nSize);

	if (spi_write_then_read(core_spi->spi, txbuf, nSize+1, txbuf, 0) < 0) {
		if (core_config->do_ic_reset) {
			/* ignore spi error if doing ic reset */
			res = 0;
		} else {
			res = -EIO;
			ipio_err("spi Write Error, res = %d\n", res);
			goto out;
		}
	}

out:
	ipio_kfree((void **)&txbuf);
	return res;
}
EXPORT_SYMBOL(core_spi_write);

int core_spi_read(uint8_t *pBuf, uint16_t nSize)
{
	int res = 0;
	uint8_t txbuf[1] = {0};

	txbuf[0] = SPI_READ;

	if (core_config->icemodeenable == false)
		return core_spi_read_9881H11(pBuf, nSize);

	if (spi_write_then_read(core_spi->spi, txbuf, 1, pBuf, nSize) < 0) {
		if (core_config->do_ic_reset) {
			/* ignore spi error if doing ic reset */
			res = 0;
		} else {
			res = -EIO;
			ipio_err("spi Read Error, res = %d\n", res);
			goto out;
		}
	}

out:
	return res;
}
EXPORT_SYMBOL(core_spi_read);

int core_spi_init(struct spi_device *spi)
{
	int ret;

	core_spi = kmalloc(sizeof(*core_spi), GFP_KERNEL);
	if (ERR_ALLOC_MEM(core_spi)) {
		ipio_err("Failed to alllocate core_i2c mem %ld\n", PTR_ERR(core_spi));
		core_spi_remove();
		return -ENOMEM;
	}

	core_spi->spi = spi;
	spi->mode = SPI_MODE_0;
	spi->bits_per_word = 8;

	ret = spi_setup(spi);
	if (ret < 0) {
		ipio_err("ERR: fail to setup spi\n");
		return -ENODEV;
	}

	ipio_info("name = %s, bus_num = %d,cs = %d, mode = %d, speed = %d\n", spi->modalias,
	 spi->master->bus_num, spi->chip_select, spi->mode, spi->max_speed_hz);

	return 0;
}
EXPORT_SYMBOL(core_spi_init);

void core_spi_remove(void)
{
	ipio_info("Remove core-spi members\n");
	ipio_kfree((void **)&core_spi);
}
EXPORT_SYMBOL(core_spi_remove);
