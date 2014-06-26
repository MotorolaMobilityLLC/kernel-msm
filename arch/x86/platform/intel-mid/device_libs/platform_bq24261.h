/*
 * platform_mrfl_bq24261.h: platform data for bq24261 driver
 *
 * (C) Copyright 2012 Intel Corporation
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_MRFL_BQ24261_H_
#define _PLATFORM_MRFL_BQ24261_H_

#define MRFL_CHRGR_DEV_NAME	"bq24261_charger"

#define PMIC_SRAM_INTR_MAP 0xFFFFF616
#define PMIC_EXT_INTR_MASK 0x01

#define BQ24261_CHRG_CUR_LOW		100	/* 100mA */
#define BQ24261_CHRG_CUR_MEDIUM		500	/* 500mA */
#define BQ24261_CHRG_CUR_HIGH		900	/* 900mA */
#define BQ24261_CHRG_CUR_NOLIMIT	1500	/* 1500mA */

extern void __init *bq24261_platform_data(
			void *info) __attribute__((weak));
#endif
