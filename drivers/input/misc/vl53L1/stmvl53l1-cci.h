/*
 *  stmvl53l0-cci.h - Linux kernel modules for STM VL53L0 FlightSense TOF sensor
 *
 *  Copyright (C) 2016 STMicroelectronics Imaging Division
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License as published by
 *  the Free Software Foundation; either version 2 of the License, or
 *  (at your option) any later version.
 *
 *  This program is distributed in the hope that it will be useful,
 *  but WITHOUT ANY WARRANTY; without even the implied warranty of
 *  MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 *  GNU General Public License for more details.
 */
/*
 * Defines
 */
#ifndef STMVL53L0_CCI_H
#define STMVL53L0_CCI_H
#include <linux/types.h>
#include "stmvl53l1.h"


#ifdef CAMERA_CCI
struct tof_ctrl_t {
	struct platform_device *pdev;
	enum msm_camera_device_type_t device_type;	
	enum cci_device_num cci_num;
	enum cci_i2c_master_t cci_master;
	struct camera_io_master io_master_info;	
	struct cam_subdev v4l2_dev_str;
	struct stmvl53l1_data *vl53l1_data;
	struct msm_pinctrl_info pinctrl_info;
	uint8_t cam_pinctrl_status;
	char device_name[20];

	/* reference counter */
	struct kref ref;
	/*!< if null no regulator use for power ctrl */
	struct regulator *power_supply;

	struct regulator *cci_supply;

	/*!< power enable gpio number
	 *
	 * if -1 no gpio if vdd not avl pwr is not controllable
	*/
	int pwren_gpio;

	/*!< xsdn reset (low active) gpio number to device
	 *
	 *  -1  mean none assume no "resetable"
	*/
	int xsdn_gpio;

	/*!< intr gpio number to device
	 *
	 *  intr is active/low negative edge by default
	 *
	 *  -1  mean none assume use polling
	 *  @warning if the dev tree and intr gpio is require please adapt code
	*/
	int intr_gpio;

	/*!< device boot i2c register address
	 *
	 * boot_reg is the value of device i2c address after it is bring out
	 * of reset.
	 */
	int boot_reg;

	struct delayed_work	dwork;

	/*!< is set if above irq gpio got acquired */
	struct hw_data_flags_t {
		unsigned pwr_owned    : 1; /*!< set if pwren gpio is owned*/
		unsigned xsdn_owned   : 1; /*!< set if sxdn  gpio is owned*/
		unsigned intr_owned   : 1; /*!< set if intr  gpio is owned*/
		unsigned intr_started : 1; /*!< set if irq is hanlde  */
	} io_flag;

	/** the irq vectore assigned to gpio
	 * -1 if no irq hanled
	*/
	int irq;

};

int stmvl53l1_init_cci(void);
void __exit stmvl53l1_exit_cci(void *);
int stmvl53l1_power_up_cci(void *);
int stmvl53l1_power_down_cci(void *);
int stmvl53l1_reset_release_cci(void *);
int stmvl53l1_reset_hold_cci(void *);
void stmvl53l1_clean_up_cci(void);
int stmvl53l1_start_intr_cci(void *object, int *poll_mode);
void *stmvl53l1_get_cci(void *);
void stmvl53l1_put_cci(void *);

#endif /* CAMERA_CCI */
#endif /* STMVL53L0_CCI_H */
