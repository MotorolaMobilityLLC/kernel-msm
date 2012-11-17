/*
 * Copyright (C) 2012 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 *
 */

#ifndef __LINUX_POWER_BQ5101XB_CHARGER_H__
#define __LINUX_POWER_BQ5101XB_CHARGER_H__

#define BQ5101XB_DRV_NAME "bq5101xb-charger"


enum bq5101xb_charger_priority {
	BQ5101XB_WIRELESS = 0,
	BQ5101XB_WIRED,
};

/**
 * struct bq5101xb_charger_platform_data -
 *
 * @chg_b_pin:		GPIO Pin Input
 *			Indicates Power is Supplied
 *			Active 0 Pin
 *
 * @ts_ctrl_term_b_pin:	GPIO Pin Output
 *			Controls Power Termination
 *			Active 0 Pin
 *
 * @ts_ctrl_fault_pin:	GPIO Pin Output
 *			Controls Power Fault
 *
 * @en1_pin:		GPIO Pin Output
 *			Controls Charge Complete
 *
 * @en2_pin:		GPIO Pin Output
 *			Controls Charge Complete
 *
 * @hot_temp:		Hot Temperature Threshold for Power Termination
 *			Units of Degrees Celsius
 *
 * @cold_temp:		Cold Temperature Threshold for Power Termination
 *			Units of Degrees Celsius
 *
 * @resume_soc:		State of Charge for Resuming from Charge Complete
 *			Units of Whole Percentage
 *
 * @resume_vbatt:	Battery Voltage for Resuming from Charge Complete
 *			Units of milliVolts
 *
 * @supply_list:	List of Supplies hooked to the BQ5101XB
 *
 * @num_supplies:	Number of Supplies hooked to the BQ5101XB
 *
 * @priority:		Charger priority Wired or Wireless
 *
 * @check_powered:	System Function for Checking Current Power State
 *
 * @check_wired:	System Function for Checking Current Wired State
 *
 */
struct bq5101xb_charger_platform_data {
	/* Inputs */
	int chrg_b_pin;

	/* Outputs */
	int ts_ctrl_term_b_pin;
	int ts_ctrl_fault_pin;
	int en1_pin;
	int en2_pin;

	/* Thresholds */
	int hot_temp;
	int cold_temp;
	int resume_soc;
	int resume_vbatt;

	char **supply_list;
	int num_supplies;
	enum bq5101xb_charger_priority priority;

	int (*check_powered)(void);
	int (*check_wired)(void);
};

#endif
