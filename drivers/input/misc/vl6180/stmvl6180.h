/*
 *  stmvl6180.h - Linux kernel modules for STM VL6180 FlightSense Time-of-Flight sensor
 *
 *  Copyright (C) 2014 STMicroelectronics Imaging Division
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
#ifndef STMVL6180_H
#define STMVL6180_H

#define STMVL6180_DRV_NAME	"stmvl6180"


#define DRIVER_VERSION		"2.0.3"
#define I2C_M_WR			0x00
//#define INT_POLLING_DELAY     20
#define RESULT_REG_COUNT	56

//if don't want to have output from vl6180_dbgmsg, comment out #DEBUG macro
#define DEBUG
//#define vl6180_dbgmsg(str, args...) pr_debug("%s: " str, __func__, ##args)
#define vl6180_dbgmsg(str, args...) printk("%s: " str, __func__, ##args)
#define vl6180_errmsg(str, args...) printk(KERN_ERR "%s: " str, __func__, ##args)

/**
 * VL6180 register addresses
 */
/* Device Registers */
#define VL6180_MODEL_ID_REG			    0x0000
#define VL6180_MODEL_REV_MAJOR_REG		    0x0001
#define VL6180_MODEL_REV_MINOR_REG		    0x0002
#define VL6180_MODULE_REV_MAJOR_REG		    0x0003
#define VL6180_MODULE_REV_MINOR_REG		    0x0004

#define VL6180_REVISION_ID_REG			    0x0005
#define VL6180_REVISION_ID_REG_BYTES		1
#define VL6180_DATE_HI_REG				0x0006
#define VL6180_DATE_HI_REG_BYTES		    1
#define VL6180_DATE_LO_REG				0x0007
#define VL6180_DATE_LO_REG_BYTES		    1
#define VL6180_TIME_REG				    0x0008
#define VL6180_TIME_REG_BYTES			    2
#define VL6180_CODE_REG				    0x000a
#define VL6180_CODE_REG_BYTES			    1
#define VL6180_FIRMWARE_REVISION_ID_REG		    0x000b
#define VL6180_FIRMWARE_REVISION_ID_REG_BYTES	    1

/* Result Registers */
#define VL6180_RESULT__RANGE_RAW_REG                                0x0064
#define VL6180_RESULT__RANGE_RAW_REG_BYTES                          1
#define VL6180_RESULT__RANGE_RETURN_RATE_REG                        0x0066
#define VL6180_RESULT__RANGE_RETURN_RATE_REG_BYTES                  2
#define VL6180_RESULT__RANGE_REFERENCE_RATE_REG                     0x0068
#define VL6180_RESULT__RANGE_REFERENCE_RATE_REG_BYTES               2
#define VL6180_RESULT__RANGE_RETURN_VCSEL_COUNT_REG                 0x006c
#define VL6180_RESULT__RANGE_RETURN_VCSEL_COUNT_REG_BYTES           4
#define VL6180_RESULT__RANGE_REFERENCE_VCSEL_COUNT_REG              0x0070
#define VL6180_RESULT__RANGE_REFERENCE_VCSEL_COUNT_REG_BYTES        4
#define VL6180_RESULT__RANGE_RETURN_AMB_COUNT_REG                   0x0074
#define VL6180_RESULT__RANGE_RETURN_AMB_COUNT_REG_BYTES             4
#define VL6180_RESULT__RANGE_REFERENCE_AMB_COUNT_REG                0x0078
#define VL6180_RESULT__RANGE_REFERENCE_AMB_COUNT_REG_BYTES          4
#define VL6180_RESULT__RANGE_RETURN_CONV_TIME_REG                   0x007c
#define VL6180_RESULT__RANGE_RETURN_CONV_TIME_REG_BYTES             4
#define VL6180_RESULT__RANGE_REFERENCE_CONV_TIME_REG                0x0080
#define VL6180_RESULT__RANGE_REFERENCE_CONV_TIME_REG_BYTES          4

typedef enum {
	NORMAL_MODE = 0,
	OFFSETCALIB_MODE = 1,
	XTALKCALIB_MODE = 2,
} init_mode_e;

/*
 *  IOCTL register data structs
 */
struct stmvl6180_register {
	uint32_t is_read; //1: read 0: write
	uint32_t reg_index;
	uint32_t reg_bytes;
	uint32_t reg_data;
	int32_t status;
};
 
/*
 *  driver data structs
 */
struct stmvl6180_data {

#ifdef CAMERA_CCI
	struct cci_data client_object;
#else
	struct i2c_data client_object;
#endif
	struct mutex update_lock;
	struct delayed_work	dwork;		/* for PS  work handler */
	struct input_dev *input_dev_ps;
	struct kobject *range_kobj;

	/* function pointer */
	struct stmvl6180_module_fn_t *pmodule_func_tbl;

	int irq;
	unsigned int reset;

	/* control flag from HAL */
	unsigned int enable_ps_sensor;

	/* PS parameters */
	unsigned int ps_is_singleshot;
	unsigned int ps_data;			/* to store PS data */

	/* Range Data */
	VL6180x_RangeData_t rangeData;

	/* Range Result Register Data */
	uint8_t  ResultBuffer[RESULT_REG_COUNT];

	/* delay time */
	uint8_t delay_ms;	// work handler delay time in miniseconds

	struct mutex work_mutex;

	/* Debug */
	unsigned int enableDebug;
};

/*
 *  funtion pointer structs
 */
struct stmvl6180_module_fn_t {
	int (*init)(void);
	void (*deinit)(void *);
	int (*power_up)(void *, unsigned int *);
	int (*power_down)(void *);
};

struct stmvl6180_data *stmvl6180_getobject(void);

#endif /* STMVL6180_H */
