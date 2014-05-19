/*
 * max77836_charger.h
 * Samsung MAX77836 Charger Header
 *
 * Copyright (C) 2012 Samsung Electronics, Inc.
 *
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

#ifndef __MAX77836_CHARGER_H
#define __MAX77836_CHARGER_H __FILE__

#include <linux/module.h>
#include <linux/mfd/core.h>
#include <linux/mfd/max77836.h>
#include <linux/mfd/max77836-private.h>
#include <linux/regulator/machine.h>
#include <linux/power_supply.h>

/* MAX77836_CHG_REG_CHG_INT */
#define MAX77836_BYP_I                  (1 << 0)
#define MAX77836_BATP_I			(1 << 2)
#define MAX77836_BAT_I                  (1 << 3)
#define MAX77836_CHG_I                  (1 << 4)
#define MAX77836_WCIN_I			(1 << 5)
#define MAX77836_CHGIN_I                (1 << 6)

/* MAX77836_CHG_REG_CHG_INT_MASK */
#define MAX77836_BYP_IM                 (1 << 0)
#define MAX77836_THM_IM                 (1 << 2)
#define MAX77836_BAT_IM                 (1 << 3)
#define MAX77836_CHG_IM                 (1 << 4)
#define MAX77836_WCIN_IM		(1 << 5)
#define MAX77836_CHGIN_IM               (1 << 6)

/* MAX77836_CHG_REG_CHG_INT_OK */
#define MAX77836_BYP_OK                 0x01
#define MAX77836_BYP_OK_SHIFT           0
#define MAX77836_BATP_OK		0x04
#define MAX77836_BATP_OK_SHIFT		2
#define MAX77836_BAT_OK                 0x08
#define MAX77836_BAT_OK_SHIFT           3
#define MAX77836_CHG_OK                 0x10
#define MAX77836_CHG_OK_SHIFT           4
#define MAX77836_WCIN_OK		0x20
#define MAX77836_WCIN_OK_SHIFT		5
#define MAX77836_CHGIN_OK               0x40
#define MAX77836_CHGIN_OK_SHIFT         6
#define MAX77836_DETBAT                 0x04
#define MAX77836_DETBAT_SHIFT           2

/* MAX77836_CHG_REG_CHG_DTLS_00 */
#define MAX77836_BATP_DTLS		0x01
#define MAX77836_BATP_DTLS_SHIFT	0
#define MAX77836_WCIN_DTLS		0x18
#define MAX77836_WCIN_DTLS_SHIFT	3
#define MAX77836_CHGIN_DTLS             0x60
#define MAX77836_CHGIN_DTLS_SHIFT       5

/* MAX77836_CHG_REG_CHG_DTLS_01 */
#define MAX77836_CHG_DTLS               0x0F
#define MAX77836_CHG_DTLS_SHIFT         0
#define MAX77836_BAT_DTLS               0x70
#define MAX77836_BAT_DTLS_SHIFT         4

/* MAX77836_CHG_REG_CHG_DTLS_02 */
#define MAX77836_BYP_DTLS               0x0F
#define MAX77836_BYP_DTLS_SHIFT         0
#define MAX77836_BYP_DTLS0      0x1
#define MAX77836_BYP_DTLS1      0x2
#define MAX77836_BYP_DTLS2      0x4
#define MAX77836_BYP_DTLS3      0x8

/* MAX77836_CHG_REG_CHG_CNFG_00 */
#define CHG_CNFG_00_MODE_SHIFT		        0
#define CHG_CNFG_00_CHG_SHIFT		        0
#define CHG_CNFG_00_OTG_SHIFT		        1
#define CHG_CNFG_00_BUCK_SHIFT		        2
#define CHG_CNFG_00_BOOST_SHIFT		        3
#define CHG_CNFG_00_DIS_MUIC_CTRL_SHIFT	        5
#define CHG_CNFG_00_MODE_MASK		        (0xf << CHG_CNFG_00_MODE_SHIFT)
#define CHG_CNFG_00_CHG_MASK		        (1 << CHG_CNFG_00_CHG_SHIFT)
#define CHG_CNFG_00_OTG_MASK		        (1 << CHG_CNFG_00_OTG_SHIFT)
#define CHG_CNFG_00_BUCK_MASK		        (1 << CHG_CNFG_00_BUCK_SHIFT)
#define CHG_CNFG_00_BOOST_MASK		        (1 << CHG_CNFG_00_BOOST_SHIFT)
#define CHG_CNFG_00_DIS_MUIC_CTRL_MASK	        (1 << CHG_CNFG_00_DIS_MUIC_CTRL_SHIFT)
#define MAX77836_MODE_DEFAULT                   0x04
#define MAX77836_MODE_CHGR                      0x01
#define MAX77836_MODE_OTG                       0x02
#define MAX77836_MODE_BUCK                      0x04
#define MAX77836_MODE_BOOST		        0x08

/* MAX77836_CHG_REG_CHG_CNFG_02 */
#define MAX77836_CHG_CC                         0x3F

/* MAX77836_CHG_REG_CHG_CNFG_03 */
#define MAX77836_CHG_TO_ITH		        0x07
#define MAX77836_CHG_TO_TIME		        (0x07 << 3)

/* MAX77836_CHG_REG_CHG_CNFG_04 */
#define MAX77836_CHG_MINVSYS_MASK               0xE0
#define MAX77836_CHG_MINVSYS_SHIFT              5
#define MAX77836_CHG_PRM_MASK                   0x1F
#define MAX77836_CHG_PRM_SHIFT                  0

#define CHG_CNFG_04_CHG_CV_PRM_SHIFT            0
#define CHG_CNFG_04_CHG_CV_PRM_MASK             (0x3F << CHG_CNFG_04_CHG_CV_PRM_SHIFT)

/* MAX77836_CHG_REG_CHG_CNFG_09 */
#define MAX77836_CHG_CHGIN_LIM                  0x7F

/* MAX77836_CHG_REG_CHG_CNFG_12 */
#define MAX77836_CHG_WCINSEL		        0x40

/* MAX77836_CHG_REG_CHG_DTLS_00 */
#define MAX77836_BATP_DTLS		        0x01
#define MAX77836_BATP_DTLS_SHIFT	        0
#define MAX77836_WCIN_DTLS		        0x18
#define MAX77836_WCIN_DTLS_SHIFT	        3
#define MAX77836_CHGIN_DTLS                     0x60
#define MAX77836_CHGIN_DTLS_SHIFT               5

/* MAX77836_CHG_DETAILS_01 */
#define MAX77836_CHG_DTLS                       0x0F
#define MAX77836_CHG_DTLS_SHIFT                 0
#define MAX77836_BAT_DTLS                       0x70
#define MAX77836_BAT_DTLS_SHIFT                 4

/* MAX77836_CHG_CTRL_06 */
#define MAX77836_CHG_CTRL6_AUTOSTOP_SHIFT	5
#define MAX77836_CHG_CTRL6_AUTOSTOP_MASK        (0x1 << MAX77836_CHG_CTRL6_AUTOSTOP_SHIFT)


struct max77836_chg_charging_current {
	int input_current_limit;
	int fast_charging_current;
	int full_check_current;
};
#define max77836_chg_charging_current_t \
	struct max77836_chg_charging_current

struct max77836_chg_platform_data {
	max77836_chg_charging_current_t *charging_current;
	/* float voltage (mV) */
	int chg_float_voltage;
};
#define max77836_chg_platform_data_t \
	struct max77836_chg_platform_data

struct max77836_chg_data {
	struct i2c_client		*client;
	max77836_chg_platform_data_t *pdata;
	struct power_supply		psy_chg;
	struct delayed_work isr_work;

	int cable_type;
	int status;
	bool is_charging;

	/* charging current : + charging, - OTG */
	int charging_current;

	/* register programming */
	int reg_addr;
	int reg_data;
	int irq_base;

	int irq_eoc;
	int chg_float_voltage;

	int bootdone;
};

#endif /* __MAX77836_CHARGER_H */
