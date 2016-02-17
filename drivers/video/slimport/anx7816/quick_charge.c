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
#ifdef QUICK_CHARGE_SUPPORT
#include "quick_charge.h"
#include "slimport_tx_drv.h"
#include <linux/notifier.h>
/*10*100ms*/
#define ACK_TIMEOUT 10

static unchar s_dongle_cap;
static unchar s_AP_cap;
static unchar s_request_vol;
static unchar s_voltage_cap[4] = {5, 9, 12, 20};
static unchar s_count = ACK_TIMEOUT;
static unchar s_pmic_0c;
static unchar s_pmic_11;
static unchar s_pmic_40;
static unchar s_pmic_0d;
static unchar s_charger_type;
static unchar s_pmic_voltage;
typedef int (*process_state)(void);

enum e_process_state {
	CHARGER_INITIALIZE = 0,
	/*IS_CHARGE_REQUEST_BACK,*/
	CHARGER_PLUG_PROCESS,
	SET_CHARGER_TO_TX,
	IS_SINK_CHARGE_ACK_BACK,
	CHARGE_FINISH_PROCESS,
	PROCESS_IDLE
};


static unchar s_pre_process = CHARGER_INITIALIZE;
static unchar s_current_process = CHARGER_INITIALIZE;
/*1 discharged, 0 charged*/
static unchar s_charger_connected = 1;
static unchar s_charger_status;

extern struct blocking_notifier_head *get_notifier_list_head(void);

int enable_otg(void)
{
#ifdef QUALCOMM_SMB1357
	char buf[16] = {0};
	/*sent otg enable command:0x1=enable 0x0=disable*/
	sp_read_reg(PMIC_ADDRESS, PMIC_OTG_OFFSET, buf);
	pr_err("enable_otg, buf=%x\n", (int)buf[0]);
	if (buf[0] & 0x01)
		return 0;
	buf[0] |= 0x1;
	sp_write_reg(PMIC_ADDRESS, PMIC_OTG_OFFSET, buf[0]);
#endif
	return 0;
}
int disable_otg(void)
{
#ifdef QUALCOMM_SMB1357
	char buf[16] = {0};
	/*sent otg enable command:0x1=enable 0x0=disable*/

	sp_read_reg(PMIC_ADDRESS, PMIC_OTG_OFFSET, buf);
	pr_err("disable_otg, buf=%x\n", (int)buf[0]);
	if ((buf[0] & 0x01) == 0)
		return 0;
	buf[0] &= 0x0;
	sp_write_reg(PMIC_ADDRESS, PMIC_OTG_OFFSET, buf[0]);
#endif
	return 0;
}

int enable_charge(void)
{
#ifdef QUALCOMM_SMB1357
	char buf[16] = {0};
	/*sent otg enable command:0x1=enable 0x0=disable*/

	sp_read_reg(PMIC_ADDRESS, PMIC_OTG_OFFSET, buf);
	pr_err("enable_charge, buf=%x\n", (int)buf[0]);
	if (buf[0] & 0x2)
		return 0;
	buf[0] |= 0x2;
	sp_write_reg(PMIC_ADDRESS, PMIC_OTG_OFFSET, buf[0]);
#endif
	return 0;
}
int disable_charge(void)
{
#ifdef QUALCOMM_SMB1357
	char buf[16] = {0};
	/*sent otg enable command:0x1=enable 0x0=disable*/

	sp_read_reg(PMIC_ADDRESS, PMIC_OTG_OFFSET, buf);
	pr_err("disable_charge, buf=%x\n", (int)buf[0]);
	buf[0] &= 0xfd;
	sp_write_reg(PMIC_ADDRESS, PMIC_OTG_OFFSET, buf[0]);
#endif
	return 0;
}

void pmic_BQ_config_access(void)
{
#ifdef QUALCOMM_SMB1357
	char buf[2] = {0};
	/*BQ config access*/
	sp_read_reg(PMIC_ADDRESS, PMIC_BQ_CONFIG_OFFSET, buf);
	pr_err("pmic_BQ_config_access, BQ config,buf=%x\n", (int)buf[0]);
	s_pmic_40 = buf[0];
	sp_write_reg(PMIC_ADDRESS, PMIC_BQ_CONFIG_OFFSET, buf[0] | 0x41);
#endif
}

/*int pmic_init(void)
{
#ifdef QUALCOMM_SMB1357
	char buf[2] = {0};
	unchar vol = 0;
	msleep(20);*/
	/*enable hvdcp*/
	/*sp_read_reg(PMIC_ADDRESS, PMIC_HVDCP_CONFIG_OFFSET, buf);
	s_pmic_0e = buf[0];
	pr_err("pmic_init, hvdcp buf=%x\n", (int)buf[0]);
	if ((buf[0] & 0x8) == 0) {
		buf[0] |= 0x8;
		pmic_BQ_config_access();
		sp_write_reg(PMIC_ADDRESS, PMIC_HVDCP_CONFIG_OFFSET, buf[0]);
	}
	for (vol = 0; vol < 4; vol++) {
		if (s_AP_cap == s_voltage_cap[vol])
			break;
	}
	sp_read_reg(PMIC_ADDRESS, PMIC_HVDCP_CONFIG_OFFSET, buf);
	pr_err("pmic_init, hvdcp buf2=%x\n", (int)buf[0]);
	pmic_BQ_config_access();
	sp_write_reg(PMIC_ADDRESS, PMIC_HVDCP_CONFIG_OFFSET,
				(buf[0] & 0xCF) | (vol << 4));
	msleep(20);
	sp_read_reg(PMIC_ADDRESS, PMIC_HVDCP_CONFIG_OFFSET, buf);
	pr_err("pmic_init, hvdcp 0e=%x\n", (int)buf[0]);*/
	/*AICL enable*/
	/*sp_read_reg(PMIC_ADDRESS, PMIC_AICL_OFFSET, buf);
	pr_err("pmic_init, AICL buf=%x\n", (int)buf[0]);
	s_pmic_0d = buf[0];
	pmic_BQ_config_access();
	sp_write_reg(PMIC_ADDRESS, PMIC_AICL_OFFSET, buf[0] | 4);*/

	/*Auto source detect disabled*/
	/*sp_read_reg(PMIC_ADDRESS, PMIC_AUTO_SOURCE_DETECT_OFFSET, buf);
	s_pmic_11 = buf[0];
	pr_err("pmic_init, Auto source detect buf=%x\n", (int)buf[0]);
	if (buf[0] & 0x01) {
		buf[0] &= 0xFE;
		pmic_BQ_config_access();
		sp_write_reg(PMIC_ADDRESS, PMIC_AUTO_SOURCE_DETECT_OFFSET,
				buf[0]);
	}
	msleep(20);*/
	/*SDP normal*/
	/*sp_read_reg(PMIC_ADDRESS, PMIC_AUTO_SOURCE_DETECT_OFFSET, buf);
	pr_err("pmic_init, SDP normal buf=%x\n", (int)buf[0]);
	if (buf[0] & 0x10) {
		buf[0] &= 0xEF;
		pmic_BQ_config_access();
		sp_write_reg(PMIC_ADDRESS, PMIC_AUTO_SOURCE_DETECT_OFFSET,
				buf[0]);
	}
	msleep(20);
#endif
	return 0;
}*/

int pmic_process(unchar vol)
{
#ifdef QUALCOMM_SMB1357
	char buf[2] = {0};
	unchar pmic_vol = 0;
	switch (vol) {
	case 5:
		pmic_vol = 0;
		break;
	case 9:
		pmic_vol = (3 << 5);
		break;
	case 12:
		break;
	case 20:
		break;
	default:
		break;
	}
	/*Set USBIN 9v or 12V*/
	sp_read_reg(PMIC_ADDRESS, PMIC_USBIN_CONFIG_OFFSET, buf);
	s_pmic_0c = buf[0];
	pr_err("pmic_init, USBIN=%x\n", (int)buf[0]);
	pmic_BQ_config_access();
	sp_write_reg(PMIC_ADDRESS, PMIC_USBIN_CONFIG_OFFSET,
				(buf[0] & 0x9F) | pmic_vol);
#endif
	return 0;
}



int pmic_recovery(void)
{
#ifdef QUALCOMM_SMB1357
	pmic_BQ_config_access();
	sp_write_reg(PMIC_ADDRESS, PMIC_USBIN_CONFIG_OFFSET, s_pmic_0c);
#endif
	return 0;
}

int read_AP_cap(void)
{
#ifdef QUALCOMM_SMB1357
	return PMIC_MAX_INPUT_VOL;
#endif
	return 9;
}
int charge_initialization(void)
{
	/*
	1. Set NEW_CHARGE_REQUEST(Set 0x72A0:2=1,0x72A0:3 = 1)
	2 .Set CHARGE_VOLTAGE_REQUEST (Set 0x72A0:1 = 1, 0x72A3 = 0x01, 5V)
	3. Set GOOD_BATTERY, PD_QC_CAP (Set 0x72A5 = 0x03, 9V for example)
	*/
	unchar val;
	s_request_vol = 0;
	s_pmic_0c = 0;
	s_pmic_11 = 0;
	s_pmic_40 = 0;
	s_pmic_0d = 0;
	s_charger_type = 0;
	s_count = ACK_TIMEOUT;
	s_charger_connected = 1;
	pr_info("%s ,charge_initialization\n", LOG_TAG);
	s_AP_cap = read_AP_cap();
	/*pmic_init();*/
	val = 0x01;
	s_current_process = PROCESS_IDLE;
	s_pre_process = PROCESS_IDLE;
	return 0;
}

int is_sink_charge_ack_back(void)
{
	unchar val;
	sp_tx_aux_dpcdread_bytes(DPCD_HIGH_ADDRESS, DPCD_MID_ADDRESS,
				DPCD_POWER_DELIVERY, 1, &val);
	pr_info("%s, is_sink_charge_ack_back val=%d\n", LOG_TAG, val);
	if (((val & 0x80) == 0) && s_count > 0) {
		s_count--;
	}
	if (val & 0x80) {
		s_count = ACK_TIMEOUT;
		return RETURN_OK;
	}
	if (s_count == 0) {
		s_count = ACK_TIMEOUT;
		return RETURN_ERR;
	}
	return 0;
}

int read_dongle_cap(void)
{
	/*
	Read DONGLE_QC_CAP ( 0x72A7, 0x0520)
	*/
	unchar val = 0;
	sp_tx_aux_dpcdread_bytes(DPCD_HIGH_ADDRESS, DPCD_MID_ADDRESS,
				DPCD_DONGLE_CAP, 1, &val);
	pr_info("%s, read_dongle_cap,val=%d\n", LOG_TAG, val);
	val = s_voltage_cap[val];
	s_dongle_cap = val;
	return val;
}

int has_charger_plugged(void)
{
	if (s_charger_status) {
		/*pr_info("%s, has_charger_plugged plugged\n", LOG_TAG);*/
		return 1;
	} else
		return 0;
	return 0;
}

int charger_plugged_process(void)
{
	unchar idx;
	pr_info("%s, charger_plugged_process\n", LOG_TAG);
	disable_otg();
	msleep(10);
	enable_charge();
	msleep(10);
	for (idx = 0; idx < 4; idx++) {
		if (s_AP_cap == s_voltage_cap[idx])
			break;
	}
	idx = (0x01|(idx << 1));
	sp_tx_aux_dpcdwrite_bytes(DPCD_HIGH_ADDRESS, DPCD_MID_ADDRESS,
				DPCD_GOOD_BATTERY_CAP, 1, &idx);
	return RETURN_OK;
}

int charger_plug_process(void)
{
	/*pr_info("%s, chager_plug_process\n", LOG_TAG);*/
	read_dongle_cap();
	if (has_charger_plugged()) {
		charger_plugged_process();
		s_pre_process++;
		s_current_process++;
		s_charger_connected = 0;
	} else {
		if (s_charger_connected == 0) {
			/*From charging to discharging*/
			s_charger_connected = 1;
			pmic_recovery();
			sp_tx_hardware_powerdown();
			reset_process();
		}
		s_current_process = PROCESS_IDLE;
		s_pre_process = PROCESS_IDLE;
	}
	return 0;
}

void set_charger_status(int status)
{
	s_charger_status = status;
	blocking_notifier_call_chain(get_notifier_list_head(), status, NULL);
	if (status > 0) {
		if (s_current_process == CHARGER_INITIALIZE)
			charge_initialization();
		s_current_process = CHARGER_PLUG_PROCESS;
		s_pre_process = CHARGER_PLUG_PROCESS;
		charger_plug_process();
	}
}

int read_charger_type(void)
{
	/*
	Read 0x72A6
	*/
	unchar val = 0;
	sp_tx_aux_dpcdread_bytes(DPCD_HIGH_ADDRESS, DPCD_MID_ADDRESS,
				DPCD_CHARGER_TYPE, 1, &val);
	pr_info("%s,read_charger_type,val=%d\n", LOG_TAG, val);
	return val;
}

int check_PMIC_charge_status(void)
{
#ifdef QUALCOMM_SMB1357
	unchar val;
	sp_read_reg(PMIC_ADDRESS, 0x55, &val);
	pr_info("%s, check_PMIC_charge_status, val=%d\n", LOG_TAG, val);
	if (val & 0x01) {
		/*power ok*/
		return 1;
	}
#endif
	return 0;
}

int get_input_charger_type(void)
{
	return s_charger_type;
}


int get_dongle_capability(void)
{
	return s_dongle_cap;
}

int get_current_voltage(void)
{
	return s_request_vol;
}

void set_pmic_voltage(int vol)
{
	s_pmic_voltage = vol;
}


int set_request_voltage(int voltage)
{
	unchar vol = 0;
	unchar val;
	pr_info("set_request_voltage,voltage=%d\n", voltage);
	if (voltage > s_AP_cap || voltage > s_dongle_cap)
		return RETURN_ERR;
	/*s_current_process = SET_CHARGER_TO_TX;
	s_pre_process = SET_CHARGER_TO_TX;*/
	/*if (s_request_vol == 5)
		return idx;
	for (vol = 1; vol < 3; vol++) {
		if (s_request_vol == s_voltage_cap[vol])
			s_request_vol = s_voltage_cap[vol-1];
	}
	return vol - 1;*/
	s_request_vol = voltage;
	for (vol = 0; vol < 4; vol++) {
		if (voltage == s_voltage_cap[vol])
			break;
	}
	sp_tx_aux_dpcdread_bytes(DPCD_HIGH_ADDRESS, DPCD_MID_ADDRESS,
				DPCD_POWER_DELIVERY, 1, &val);
	pr_info("set_chager_to_tx, 0x72A3, val=%x\n", (int)val);
	val = ((val & 0x9F) | (vol << 5) | 0x01);
	pr_info("set_chager_to_tx, 0x72A3, write val=%x\n", (int)val);
	sp_tx_aux_dpcdwrite_bytes(DPCD_HIGH_ADDRESS, DPCD_MID_ADDRESS,
				DPCD_POWER_DELIVERY, 1, &val);
	/*s_pre_process++;
	s_current_process++;*/
	return RETURN_OK;
}

int set_chager_to_tx(void)
{
	unchar vol = 0;
	unchar val;
	s_charger_type = read_charger_type();
	if (s_charger_type == 0) {
		s_pre_process = CHARGER_PLUG_PROCESS;
		s_current_process = CHARGER_PLUG_PROCESS;
		s_charger_connected = 1;
		return 0;
	}
	if (s_request_vol == 0) {
		s_request_vol = (s_AP_cap > s_dongle_cap) ?
				s_dongle_cap : s_AP_cap;
	}
	for (vol = 0; vol < 4; vol++) {
		if (s_request_vol == s_voltage_cap[vol])
			break;
	}
	pr_info("set_chager_to_tx,vol=%d, s_request_vol=%d,charger type =%d\n",
			vol, s_request_vol, s_charger_type);
	if ((s_charger_type == 0x09)) {
		sp_tx_aux_dpcdread_bytes(DPCD_HIGH_ADDRESS, DPCD_MID_ADDRESS,
					DPCD_POWER_DELIVERY, 1, &val);
		pr_info("set_chager_to_tx, 0x72A3, val=%x\n", (int)val);
		val = ((val & 0x9F) | (vol << 5) | 0x01);
		pr_info("set_chager_to_tx, 0x72A3, write val=%x\n", (int)val);
		sp_tx_aux_dpcdwrite_bytes(DPCD_HIGH_ADDRESS, DPCD_MID_ADDRESS,
					DPCD_POWER_DELIVERY, 1, &val);
		pmic_process(s_request_vol);
		s_pre_process++;
		s_current_process++;
	} else {
		pmic_process(5);
		s_current_process = PROCESS_IDLE;
		s_pre_process = PROCESS_IDLE;
	}
	return 0;
}

void set_new_voltage(void)
{
	switch (s_request_vol) {
	case 5:
		break;
	case 9:
		s_request_vol = 5;
		break;
	case 12:
		s_request_vol = 9;
		break;
	case 20:
		s_request_vol = 12;
		break;
	}
}

int charge_finish_process(void)
{
	pr_info("%s,\n", LOG_TAG);
	if (!check_PMIC_charge_status()) {
		s_pre_process = SET_CHARGER_TO_TX;
		s_current_process = SET_CHARGER_TO_TX;
		set_new_voltage();
		set_chager_to_tx();
	} else {
		s_current_process++;
		s_pre_process++;
	}
	return 0;
}

int idle_process(void)
{
	if (has_charger_plugged() == 0) {
		if (s_charger_connected == 0) {
			/*From charging to discharging*/
			s_charger_connected = 1;
			reset_process();
		}
	}
	return 0;
}

process_state state_functions[] = {
	charge_initialization,
	/*is_charge_request_back,*/
	charger_plug_process,
	/*is_charge_request_back,*/
	set_chager_to_tx,
	/*is_charge_request_back,*/
	is_sink_charge_ack_back,
	charge_finish_process,
	idle_process
};

void reset_process(void)
{
	s_current_process = CHARGER_INITIALIZE;
	s_pre_process = CHARGER_INITIALIZE;
}


void quick_charge_main_process(void)
{
	int ret = 0;
	/*pr_info("main_process,s_current_process=%d\n", s_current_process);*/
	ret = (*state_functions[s_current_process])();
	if (s_current_process == IS_SINK_CHARGE_ACK_BACK) {
		if (ret == RETURN_ERR) {
			sp_tx_hardware_powerdown();
			reset_process();
		} else if (ret == RETURN_OK) {
			/*pmic_process(s_request_vol);*/
			s_current_process++;
			s_pre_process++;
			(*state_functions[s_current_process])();
		}
	} else if (s_current_process == PROCESS_IDLE)
		return;

}
#endif /*#ifdef QUICK_CHARGE_SUPPORT*/
