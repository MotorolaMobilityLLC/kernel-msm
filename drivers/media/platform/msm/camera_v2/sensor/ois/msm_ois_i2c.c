/* Copyright (c) 2011-2013, The Linux Foundation. All rights reserved.
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

#include "msm_ois.h"
#include "msm_ois_i2c.h"

int32_t RamWriteA(uint16_t RamAddr, uint16_t RamData)
{
	return ois_i2c_write(RamAddr, RamData, 2);
}

int32_t RamReadA(uint16_t RamAddr, uint16_t *ReadData)
{
	return ois_i2c_read(RamAddr, ReadData, 2);
}

int32_t RamWrite32A(uint16_t RamAddr, uint32_t RamData)
{
	int32_t ret = 0;
	uint8_t data[4];

	data[0] = (RamData >> 24) & 0xFF;
	data[1] = (RamData >> 16) & 0xFF;
	data[2] = (RamData >> 8)  & 0xFF;
	data[3] = (RamData) & 0xFF;

	ret = ois_i2c_write_seq(RamAddr, &data[0], 4);
	return ret;
}

int32_t RamRead32A(uint16_t RamAddr, uint32_t *ReadData)
{
	int32_t ret = 0;
	uint8_t buf[4];

	ret = ois_i2c_read_seq(RamAddr, buf, 4);
	*ReadData = (buf[0] << 24) | (buf[1] << 16) | (buf[2] << 8) | buf[3];
	return ret;
}

int32_t RegWriteA(uint16_t RegAddr, uint8_t RegData)
{
	int32_t ret = 0;
	uint16_t data = (uint16_t)RegData;

	ret = ois_i2c_write(RegAddr, data, 1);
	return ret;
}

int32_t RegReadA(uint16_t RegAddr, uint8_t *RegData)
{
	int32_t ret = 0;
	uint16_t data = 0;

	ret = ois_i2c_read(RegAddr, &data, 1);
	*RegData = (uint8_t)data;
	return ret;
}

int32_t E2PRegWriteA(uint16_t RegAddr, uint8_t RegData)
{
	int32_t ret = 0;
	uint16_t data = (uint16_t)RegData;

	ret = ois_i2c_e2p_write(RegAddr, data, 1);
	return ret;
}

int32_t E2PRegReadA(uint16_t RegAddr, uint8_t *RegData)
{
	int32_t ret = 0;
	uint16_t data = 0;

	ret = ois_i2c_e2p_read(RegAddr, &data, 1);
	*RegData = (uint8_t)data;
	return ret;
}

int32_t E2pRed(uint16_t UsAdr, uint8_t UcLen, uint8_t *UcPtr)
{
	uint16_t UcCnt;

	if (UcLen > 0) {
		UcPtr += UcLen - 1;
		for (UcCnt = 0; UcCnt < UcLen;  UcCnt++, UcPtr--)
			E2PRegReadA(UsAdr + UcCnt, UcPtr);
	}
	return 0;
}

int32_t E2pWrt(uint16_t UsAdr, uint8_t UcLen, uint8_t *UcPtr)
{
	uint8_t UcCnt;

	for (UcCnt = 0; UcCnt < UcLen;  UcCnt++)
		E2PRegWriteA(UsAdr + UcCnt, (uint8_t)UcPtr[abs((UcLen - 1) - UcCnt)]);

	return 0;
}
