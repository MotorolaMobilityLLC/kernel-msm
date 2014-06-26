/*
 * platform_bq24192.h: bq24192 platform data header file
 *
 * (C) Copyright 2008 Intel Corporation
 * Author:
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; version 2
 * of the License.
 */
#ifndef _PLATFORM_BQ24192_H_
#define _PLATFORM_BQ24192_H_

#define CHGR_INT_N	93
#define BQ24192_CHRG_OTG_GPIO	36

#define BQ24192_CHRG_CUR_NOLIMIT	1500 /* in mA */
#define BQ24192_CHRG_CUR_HIGH		900
#define BQ24192_CHRG_CUR_MEDIUM		500
#define BQ24192_CHRG_CUR_LOW		100

/* ADC Channel Numbers */
#define BATT_NUM_GPADC_SENSORS	1
#define GPADC_BPTHERM_CHNUM	0x9
#define GPADC_BPTHERM_SAMPLE_COUNT	1

#define BATT_VMIN_THRESHOLD_DEF	3400	/* 3400mV */
#define BATT_TEMP_MAX_DEF	60	/* 60 degrees */
#define BATT_TEMP_MIN_DEF	0
#define BATT_CRIT_CUTOFF_VOLT_DEF	3600	/* 3600 mV */

#define BPTHERM_CURVE_MAX_SAMPLES	23
#define BPTHERM_CURVE_MAX_VALUES	4

/*CLT battery temperature  attributes*/
#define BPTHERM_ADC_MIN	107
#define BPTHERM_ADC_MAX	977

/* SMIP related definitions */
/* sram base address for smip accessing*/
#define SMIP_SRAM_OFFSET_ADDR	0x44d
#define SMIP_SRAM_BATT_PROP_OFFSET_ADDR	0x460
#define TEMP_MON_RANGES	4

/* Signature comparision of SRAM data for supportted Battery Char's */
#define SBCT_REV	0x16
#define RSYS_MOHMS	0xAA
/* Master Charge control register */
#define MSIC_CHRCRTL	0x188
#define MSIC_CHRGENBL	0x40
extern void *bq24192_platform_data(void *info) __attribute__((weak));
#ifdef CONFIG_CHARGER_BQ24192
extern int platform_get_battery_pack_temp(int *temp);
#else
static int platform_get_battery_pack_temp(int *temp)
{
	return 0;
}
#endif
#endif
