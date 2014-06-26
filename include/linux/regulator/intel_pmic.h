/*
*Support for intel pmic
*Copyright (c) 2012, Intel Corporation.
*This program is free software; you can redistribute it and/or modify
*it under the terms of the GNU General Public License version 2 as
*published by the Free Software Foundation.
*
*/

struct regulator_init_data;

enum intel_regulator_id {
	VPROG1,
	VPROG2,
	VEMMC1,
	VEMMC2,
};

/* Voltage tables for Regulators */
static const u16 VPROG1_VSEL_table[] = {
	1200, 1800, 2500, 2800,
};

static const u16 VPROG2_VSEL_table[] = {
	1200, 1800, 2500, 2800,
};

static const u16 VEMMC1_VSEL_table[] = {
	2850,
};
static const u16 VEMMC2_VSEL_table[] = {
	2850,
};

static const u16 V180AON_VSEL_table[] = {
	1800, 1817, 1836, 1854,
};

/* Slave Address for all regulators */
#define VPROG1CNT_ADDR	0x0D6
#define VPROG2CNT_ADDR	0x0D7
#define VEMMC1CNT_ADDR	0x0D9
#define VEMMC2CNT_ADDR	0x0DA
/**
 * intel_pmic_info - platform data for intel pmic
 * @pmic_reg: pmic register that is to be used for this VR
 */
struct intel_pmic_info {
	struct regulator_init_data *init_data;
	struct regulator_dev *intel_pmic_rdev;
	const u16 *table;
	u16 pmic_reg;
	u8 table_len;
};
