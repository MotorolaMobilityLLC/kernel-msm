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

#define DRIVER_VERSION		"1.1.14"
#define I2C_M_WR			0x00
/* #define INT_POLLING_DELAY	20 */

/* if don't want to have output from vl6180_dbgmsg, comment out #DEBUG macro */
/*#define DEBUG*/
/* #define vl6180_dbgmsg(str, args...) pr_debug("%s: " str, __func__, ##args) */
#define vl53l0_dbgmsg(str, args...)	\
	pr_debug("%s: " str, __func__, ##args)
/* #define vl6180_errmsg(str, args...) pr_err("%s: " str, __func__, ##args) */
#define vl53l0_errmsg(str, args...) \
	printk(KERN_ERR "%s: " str, __func__, ##args)

#define VL53L0_VDD_MIN      2850000
#define VL53L0_VDD_MAX      2850000
/*driver working mode*/
#define	OFF_MODE  0
#define	CAM_MODE  1
#define	SAR_MODE  2
#define	SUPER_MODE  3
#define	XTALKCAL_MODE  4
#define	OFFSETCAL_MODE  5
/*user actions*/
#define	CAM_ON  0
#define	CAM_OFF 1
#define	SAR_ON  2
#define	SAR_OFF 3
#define	XTALKCAL_ON 4
#define	OFFSETCAL_ON 5
#define	CAL_OFF 6
/*parameter types*/
#define	OFFSET_PAR 0
#define	XTALKRATE_PAR 1
#define	XTALKENABLE_PAR 2
#define	SIGMAVAL_PRA 3
#define	SIGMACTL_PRA 4
#define	WRAPAROUNDCTL_PRA 5
#define	INTERMEASUREMENTPERIOD_PAR 6
#define	MEASUREMENTTIMINGBUDGET_PAR 7
#define	SGLVAL_PRA 8
#define	SGLCTL_PRA 9
#define CUTV_PRA 10

#define	CCI_BUS  0
#define	I2C_BUS  1


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
	uint32_t name;
	int32_t value;
	int32_t status;
};

/*
 *  driver data structs
 */
struct stmvl53l0_data {
	/* embed ST VL53L0 Dev data as "dev_data" */
	VL53L0_DevData_t Data;
	/* i2c device address user specific field */
	uint8_t   I2cDevAddr;
	uint8_t   comms_type;
	uint16_t  comms_speed_khz;

	struct cci_data cci_client_object;
	struct i2c_data i2c_client_object;
	void *client_object;
	struct mutex update_lock;
	struct delayed_work	dwork;		/* for PS  work handler */
	struct delayed_work     initwork;
	struct input_dev *input_dev_ps;
	struct kobject *range_kobj;

	const char *dev_name;

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
	int offset;
	int xtalk;

	/* delay time in miniseconds*/
	unsigned int delay_ms;

	struct mutex work_mutex;
	/* Debug */
	unsigned int enableDebug;
	uint8_t interrupt_received;
	int d_mode;
	uint8_t w_mode;
	/*for SAR mode indicate low range interrupt*/
	uint8_t lowint;
	uint8_t bus_type;
	uint32_t lowv;
	uint32_t highv;
	wait_queue_head_t range_data_wait;
	uint8_t d_ready;
	uint8_t cut_v;
	unsigned int xtalkCalDistance;
	unsigned int offsetCalDistance;
	unsigned int xtalkcalval;
	VL53L0_GpioFunctionality gpio_function;
	uint8_t c_suspend;
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

struct stmvl53l0_data *stmvl53l0_getobject(void);
int stmvl53l0_setup(struct stmvl53l0_data *data, uint8_t type);
int stmvl53l0_checkmoduleid(struct stmvl53l0_data *data,
	void *client, uint8_t type);
void i2c_setclient(void *client, uint8_t type);

#endif /* STMVL53L0_H */
