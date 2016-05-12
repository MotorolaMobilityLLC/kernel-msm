/*
 * Copyright (c) 2016, Sharp. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#ifndef __SHTPS_FTS_CFG_FT8607_H__
#define __SHTPS_FTS_CFG_FT8607_H__

/* ===================================================================================
 * [ Firmware update ]
 */
#include "shtps_fts_fw_ft8607.h"


/* ===================================================================================
 * [ Model specifications ]
 */
#define CONFIG_SHTPS_LCD_SIZE_X						720
#define CONFIG_SHTPS_LCD_SIZE_Y						1280

#define SHTPS_QOS_LATENCY_DEF_VALUE	 				34

#define SHTPS_VAL_FINGER_WIDTH_MAXSIZE				15
#define SHTPS_REGVAL_STATUS_CRC_ERROR				0xED

/* ===================================================================================
 * [ Hardware specifications ]
 */
#define	SHTPS_I2C_BLOCKREAD_BUFSIZE					(SHTPS_TM_TXNUM_MAX * 4)
#define SHTPS_I2C_BLOCKWRITE_BUFSIZE				0x10

#define SHTPS_I2C_RETRY_COUNT						5
#define SHTPS_I2C_RETRY_WAIT						5

#define SHTPS_HWRESET_TIME_MS						5
#define SHTPS_HWRESET_AFTER_TIME_MS					300

/* -----------------------------------------------------------------------------------
 */
#endif /* __SHTPS_FTS_CFG_FT8607_H__ */
