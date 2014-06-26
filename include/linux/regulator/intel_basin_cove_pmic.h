/*
 * intel_basin_cove_pmic.h - Support for Basin Cove pmic VR
 * Copyright (c) 2012, Intel Corporation.
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 *
 */
#ifndef __INTEL_BASIN_COVE_PMIC_H_
#define __INTEL_BASIN_COVE_PMIC_H_

struct regulator_init_data;

enum intel_regulator_id {
	VPROG1,
	VPROG2,
	VPROG3,
};

/* Voltage tables for Regulators */
static const u16 VPROG1_VSEL_table[] = {
	1500, 1800, 2500, 2800,
};

static const u16 VPROG2_VSEL_table[] = {
	1500, 1800, 2500, 2850,
};

static const u16 VPROG3_VSEL_table[] = {
	1050, 1800, 2500, 2800,
};

/* Slave Address for all regulators */
#define VPROG1CNT_ADDR	0x0ac
#define VPROG2CNT_ADDR	0x0ad
#define VPROG3CNT_ADDR	0x0ae
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

#endif /* __INTEL_BASIN_COVE_PMIC_H_ */
