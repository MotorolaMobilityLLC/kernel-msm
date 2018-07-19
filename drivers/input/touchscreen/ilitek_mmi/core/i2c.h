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

#ifndef __I2C_H
#define __I2C_H

struct core_i2c_data {
	struct i2c_client *client;
	int clk;
	int seg_len;
};

extern struct core_i2c_data *core_i2c;

extern int core_i2c_write(uint8_t nSlaveId, uint8_t *pBuf, uint16_t nSize);
extern int core_i2c_read(uint8_t nSlaveId, uint8_t *pBuf, uint16_t nSize);

extern int core_i2c_segmental_read(uint8_t nSlaveId, uint8_t *pBuf
, uint16_t nSize);

extern int core_i2c_init(struct i2c_client *client);
extern void core_i2c_remove(void);

#endif
