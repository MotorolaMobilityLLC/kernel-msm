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
#include "i2c.h"
#include "mp_test.h"
#include "finger_report.h"
#include "protocol.h"
#include "../platform.h"


struct core_i2c_data *core_i2c;

#ifdef I2C_DMA
static unsigned char *ilitek_dma_va;
static dma_addr_t ilitek_dma_pa;

#define DMA_VA_BUFFER	4096

static int dma_alloc(struct core_i2c_data *i2c)
{
	if (i2c->client != NULL) {
		i2c->client->dev.coherent_dma_mask = DMA_BIT_MASK(32);
		ilitek_dma_va = (u8 *) dma_alloc_coherent(&i2c->client->dev
		, DMA_VA_BUFFER, &ilitek_dma_pa, GFP_KERNEL);
		if (ERR_ALLOC_MEM(ilitek_dma_va)) {
			ipio_err("Allocate DMA I2C Buffer failed\n");
			return -ENOMEM;
		}

		memset(ilitek_dma_va, 0, DMA_VA_BUFFER);
		i2c->client->ext_flag |= I2C_DMA_FLAG;
		return 0;
	}

	ipio_err("i2c->client is NULL, return fail\n");
	return -ENODEV;
}

static void dma_free(void)
{
	if (ilitek_dma_va != NULL) {
		dma_free_coherent(&core_i2c->client->dev, DMA_VA_BUFFER
		, ilitek_dma_va, ilitek_dma_pa);

		ilitek_dma_va = NULL;
		ilitek_dma_pa = 0;

		ipio_info("Succeed to free DMA buffer\n");
	}
}
#endif /* I2C_DMA */

int core_i2c_write(uint8_t nSlaveId, uint8_t *pBuf, uint16_t nSize)
{
	int res = 0;
	uint8_t check_sum = 0;
	uint8_t *txbuf = NULL;

	struct i2c_msg msgs[] = {
		{
		 .addr = nSlaveId,
		 .flags = 0,	/* write flag. */
		 .len = nSize,
		 .buf = pBuf,
		 },
	};

#ifdef I2C_DMA
	ipio_debug(DEBUG_I2C, "DMA: size = %d\n", nSize);
	if (nSize > 8) {
		msgs[0].addr = (core_i2c->client->addr & I2C_MASK_FLAG);
		msgs[0].ext_flag =
		 (core_i2c->client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
		memcpy(ilitek_dma_va, pBuf, nSize);
		msgs[0].buf = (uint8_t *) ilitek_dma_pa;
	}
#endif /* I2C_DMA */

	/*
	 * NOTE: If TP driver is doing MP test and commanding 0xF1 to FW,
	 * we add a checksum to the last index and plus 1 with size.
	 */
	txbuf = kcalloc(nSize + 1, sizeof(uint8_t), GFP_KERNEL);
	if (ERR_ALLOC_MEM(txbuf)) {
		ipio_err("Failed to allocate txbuf mem\n");
		res = -ENOMEM;
		goto out;
	}
	if (protocol->major >= 5 && protocol->mid >= 4) {
		if (core_fr->actual_fw_mode == protocol->test_mode && pBuf[0] == 0xF1) {
			check_sum = core_fr_calc_checksum(pBuf, nSize);
			memcpy(txbuf, pBuf, nSize);
			txbuf[nSize] = check_sum;
			msgs[0].buf = txbuf;
			msgs[0].len = nSize + 1;
		}
	}

	if (i2c_transfer(core_i2c->client->adapter, msgs, 1) < 0) {
		if (core_config->do_ic_reset) {
			/* ignore i2c error if doing ic reset */
			res = 0;
		} else {
			res = -EIO;
			ipio_err("I2C Write Error, res = %d\n", res);
			goto out;
		}
	}

out:
	ipio_kfree((void **)&txbuf);
	return res;
}
EXPORT_SYMBOL(core_i2c_write);

int core_i2c_read(uint8_t nSlaveId, uint8_t *pBuf, uint16_t nSize)
{
	int res = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = nSlaveId,
		 .flags = I2C_M_RD, /* read flag */
		 .len = nSize,
		 .buf = pBuf,
		 },
	};

#ifdef I2C_DMA
	ipio_debug(DEBUG_I2C, "DMA: size = %d\n", nSize);
	if (nSize > 8) {
		msgs[0].addr = (core_i2c->client->addr & I2C_MASK_FLAG);
		msgs[0].ext_flag =
		 (core_i2c->client->ext_flag | I2C_ENEXT_FLAG | I2C_DMA_FLAG);
		msgs[0].buf = (uint8_t *) ilitek_dma_pa;
	} else {
		msgs[0].buf = pBuf;
	}
#endif /* I2C_DMA */

	if (i2c_transfer(core_i2c->client->adapter, msgs, 1) < 0) {
		res = -EIO;
		ipio_err("I2C Read Error, res = %d\n", res);
		goto out;
	}
#ifdef I2C_DMA
	if (nSize > 8)
		memcpy(pBuf, ilitek_dma_va, nSize);

#endif /* I2C_DMA */

out:
	return res;
}
EXPORT_SYMBOL(core_i2c_read);

int core_i2c_segmental_read(uint8_t nSlaveId, uint8_t *pBuf, uint16_t nSize)
{
	int res = 0;
	int offset = 0;

	struct i2c_msg msgs[] = {
		{
		 .addr = nSlaveId,
		 .flags = I2C_M_RD,
		 .len = nSize,
/* .buf = pBuf, */
		 },
	};

	while (nSize > 0) {
		msgs[0].buf = &pBuf[offset];

		if (nSize > core_i2c->seg_len) {
			msgs[0].len = core_i2c->seg_len;
			nSize -= core_i2c->seg_len;
			offset += msgs[0].len;
		} else {
			msgs[0].len = nSize;
			nSize = 0;
		}

		ipio_debug(DEBUG_I2C, "Length = %d\n", msgs[0].len);

		if (i2c_transfer(core_i2c->client->adapter, msgs, 1) < 0) {
			res = -EIO;
			ipio_err("I2C Read Error, res = %d\n", res);
			goto out;
		}
	}

out:
	return res;
}
EXPORT_SYMBOL(core_i2c_segmental_read);

int core_i2c_init(struct i2c_client *client)
{
	int i;

	core_i2c = kmalloc(sizeof(*core_i2c), GFP_KERNEL);
	if (ERR_ALLOC_MEM(core_i2c)) {
		ipio_err("Failed to alllocate core_i2c mem %ld\n",
		 PTR_ERR(core_i2c));
		core_i2c_remove();
		return -ENOMEM;
	}

	core_i2c->client = client;
	core_i2c->seg_len = 256;	/* length of segment */

#ifdef I2C_DMA
	if (dma_alloc(core_i2c->client) < 0) {
		ipio_err("Failed to alllocate DMA mem %ld\n",
		 PTR_ERR(core_i2c));
		return -ENOMEM;
	}
#endif /* I2C_DMA */

	for (i = 0 ; i < ARRAY_SIZE(ipio_chip_list); i++) {
		if (ipio_chip_list[i] == ipd->chip_id) {
			if (ipio_chip_list[i] == CHIP_TYPE_ILI7807) {
				if (core_config->chip_type == ILI7807_TYPE_F_AA
				&& core_config->chip_type == ILI7807_TYPE_F_AB)
					core_i2c->clk = 100000;
				else
					core_i2c->clk = 400000;

			} else if (ipio_chip_list[i] == CHIP_TYPE_ILI9881) {
				core_i2c->clk = 400000;
			}

			return 0;
		}
	}

	ipio_err("Can't find this chip in support list\n");
	return 0;
}
EXPORT_SYMBOL(core_i2c_init);

void core_i2c_remove(void)
{
	ipio_info("Remove core-i2c members\n");

#ifdef I2C_DMA
	dma_free();
#endif /* I2C_DMA */

	ipio_kfree((void **)&core_i2c);
}
EXPORT_SYMBOL(core_i2c_remove);
