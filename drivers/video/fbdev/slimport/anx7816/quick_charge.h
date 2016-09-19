/*
 * Copyright(c) 2014, Analogix Semiconductor. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __QUICK_CHARGE_H__
#define __QUICK_CHARGE_H__


/*registers define*/

#define DPCD_HIGH_ADDRESS 0
#define DPCD_MID_ADDRESS 5
#define DPCD_POWER_DELIVERY 0x32
#define DPCD_GOOD_BATTERY_CAP 0x34
#define DPCD_CHARGER_TYPE 0xf7
#define CHARGER_PLUG_STATUS 0xFF /*1, unlpug, 0 plugged*/
#define DPCD_DONGLE_CAP 0x20

#ifdef QUALCOMM_SMB1357
#define PMIC_ADDRESS 0x38
#define PMIC_BQ_CONFIG_OFFSET 0x40
#define PMIC_OTG_OFFSET 0x42
#define PMIC_USBIN_CONFIG_OFFSET 0x0c
#define PMIC_AICL_OFFSET 0x0d
#define PMIC_MAX_INPUT_VOL 9
#endif

#define RETURN_OK 2
#define RETURN_ERR 1

void quick_charge_main_process(void);
void reset_process(void);
int enable_otg(void);
int disable_charge(void);
int disable_otg(void);
int pmic_recovery(void);
void set_charger_status(int status);
int get_input_charger_type(void);
int get_dongle_capability(void);
int get_current_voltage(void);
void set_pmic_voltage(int vol);
int set_request_voltage(int voltage);
int is_sink_charge_ack_back(void);

#endif
