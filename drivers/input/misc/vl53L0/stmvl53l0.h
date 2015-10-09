/*
 *  stmvl53l0.h - Linux kernel modules for STM VL53L0 FlightSense TOF sensor
 *
 *  Copyright (C) 2015 STMicroelectronics Imaging Division
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
 *
 *  You should have received a copy of the GNU General Public License
 *  along with this program; if not, write to the Free Software
 *  Foundation, Inc., 675 Mass Ave, Cambridge, MA 02139, USA.
 */
/*
 * Defines
 */
#ifndef STMVL53L0_H
#define STMVL53L0_H

#include <linux/types.h>
#include <linux/mutex.h>
#include <linux/workqueue.h>
#include <linux/miscdevice.h>

#define STMVL53L0_DRV_NAME	"stmvl53l0"
#define STMVL53L0_SLAVE_ADDR	(0x52>>1)

#define DRIVER_VERSION		"0.0.3"
#define I2C_M_WR			0x00
/* #define INT_POLLING_DELAY	20 */

/* if don't want to have output from vl6180_dbgmsg, comment out #DEBUG macro */
#define DEBUG
/* #define vl6180_dbgmsg(str, args...) pr_debug("%s: " str, __func__, ##args) */
#define vl53l0_dbgmsg(str, args...)	\
	printk(KERN_INFO "%s: " str, __func__, ##args)
/* #define vl6180_errmsg(str, args...) pr_err("%s: " str, __func__, ##args) */
#define vl53l0_errmsg(str, args...) \
	printk(KERN_ERR "%s: " str, __func__, ##args)

#define VL53L0_VDD_MIN      2600000
#define VL53L0_VDD_MAX      3000000

typedef enum {
	NORMAL_MODE = 0,
	OFFSETCALIB_MODE = 1,
	XTALKCALIB_MODE = 2,
} init_mode_e;

typedef enum {
	OFFSET_PAR = 0,
	XTALKRATE_PAR = 1,
	XTALKENABLE_PAR = 2,
} parameter_name_e;

/*
 *  IOCTL register data structs
 */
struct stmvl53l0_register {
	uint32_t is_read; /*1: read 0: write*/
	uint32_t reg_index;
	uint32_t reg_bytes;
	uint32_t reg_data;
	int32_t status;
};

/*
 *  IOCTL parameter structs
 */
struct stmvl53l0_parameter {
	uint32_t is_read; /*1: Get 0: Set*/
	parameter_name_e name;
	int32_t value;
	int32_t status;
};

/*
 *  driver data structs
 */
struct stmvl53l0_data {

	VL53L0_DevData_t Data;	/* !<embed ST VL53L0 Dev data as
								"dev_data" */
	uint8_t   I2cDevAddr;	/*!< i2c device address user specific field
							*/
	uint8_t   comms_type;	/*!< Type of comms : VL53L0_COMMS_I2C
							or VL53L0_COMMS_SPI */
	uint16_t  comms_speed_khz;	/*!< Comms speed [kHz] :
						typically 400kHz for I2C */
#ifdef CAMERA_CCI
	struct cci_data client_object;
#else
	struct i2c_data client_object;
#endif
	struct mutex update_lock;
	struct delayed_work	dwork;		/* for PS  work handler */
	struct input_dev *input_dev_ps;
	struct kobject *range_kobj;

	const char *dev_name;
	/* function pointer */

	/* misc device */
	struct miscdevice miscdev;

	int irq;
	unsigned int reset;

	/* control flag from HAL */
	unsigned int enable_ps_sensor;

	/* PS parameters */
	unsigned int ps_is_singleshot;
	unsigned int ps_is_started;
	unsigned int ps_data;			/* to store PS data */

	/* Range Data */
	VL53L0_RangingMeasurementData_t rangeData;


	/* delay time in miniseconds*/
	uint8_t delay_ms;

	struct mutex work_mutex;

	/* Debug */
	unsigned int enableDebug;
	uint8_t interrupt_received;
};

/*
 *  funtion pointer structs
 */
struct stmvl53l0_module_fn_t {
	int (*init)(void);
	void (*deinit)(void *);
	int (*power_up)(void *, unsigned int *);
	int (*power_down)(void *);
};

int stmvl53l0_setup(struct stmvl53l0_data *data);

#endif /* STMVL53L0_H */
