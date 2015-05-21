/*
********************* (C) COPYRIGHT 2012 STMicroelectronics ********************
*
* File Name          : lis3dsh.h
* Authors            : MH - C&I BU - Application Team
*		     : Matteo Dameno (matteo.dameno@st.com)
*		     : Denis Ciocca (denis.ciocca@st.com)
* Version            : V.1.2.2
* Date               : 2012/Dec/15
*
********************************************************************************
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
* THE PRESENT SOFTWARE IS PROVIDED ON AN "AS IS" BASIS, WITHOUT WARRANTIES
* OR CONDITIONS OF ANY KIND, EITHER EXPRESS OR IMPLIED, FOR THE SOLE
* PURPOSE TO SUPPORT YOUR APPLICATION DEVELOPMENT.
* AS A RESULT, STMICROELECTRONICS SHALL NOT BE HELD LIABLE FOR ANY DIRECT,
* INDIRECT OR CONSEQUENTIAL DAMAGES WITH RESPECT TO ANY CLAIMS ARISING FROM THE
* CONTENT OF SUCH SOFTWARE AND/OR THE USE MADE BY CUSTOMERS OF THE CODING
* INFORMATION CONTAINED HEREIN IN CONNECTION WITH THEIR PRODUCTS.
*
********************************************************************************

********************************************************************************
Version History.
	V 1.0.0		First Release
	V 1.0.2		I2C address bugfix
	V 1.2.0		Registers names compliant to correct datasheet
	V.1.2.1		Removed enable_interrupt_output sysfs file, manage int1
			and int2, implements int1 isr.
	V.1.2.2		Modified state program loadiing defines, removed
			state machine program.
********************************************************************************
SYSFS interface
- range: set full scale
	-> accelerometer: 	2,4,6,8,16 				[g]
- poll_period_ms: set 1/ODR
	-> accelerometer:	LIS3DH_ACC_MIN_POLL_PERIOD_MS < t	[ms]
- enable_device: enable/disable sensor					[1/0]


INPUT subsystem: NOTE-> output data INCLUDE the sensitivity in accelerometer.
- accelerometer:	abs_x, abs_y, abs_z		[ug]
*******************************************************************************/

#ifndef __LIS3DSH_H__
#define __LIS3DSH_H__


#define LIS3DSH_ACC_DEV_NAME			"lis3dsh_acc"


/* Poll Interval */
#define LIS3DSH_ACC_MIN_POLL_PERIOD_MS		1


#ifdef __KERNEL__

#define LIS3DSH_ACC_SAD0L		(0x02)
#define LIS3DSH_ACC_SAD0H		(0x01)
#define LIS3DSH_ACC_I2C_SADROOT		(0x07)
#define LIS3DSH_ACC_I2C_SAD_L		((LIS3DSH_ACC_I2C_SADROOT<<2) | \
							LIS3DSH_ACC_SAD0L)
#define LIS3DSH_ACC_I2C_SAD_H		((LIS3DSH_ACC_I2C_SADROOT<<2) | \
							LIS3DSH_ACC_SAD0H)

/* Interrupt */
#define LIS3DSH_ACC_DEFAULT_INT1_GPIO		(33)
#define LIS3DSH_ACC_DEFAULT_INT2_GPIO		(37)


/* Accelerometer Sensor Full Scale */
#define LIS3DSH_ACC_G_2G			(0x00)
#define LIS3DSH_ACC_G_4G			(0x08)
#define LIS3DSH_ACC_G_6G			(0x10)
#define LIS3DSH_ACC_G_8G			(0x18)
#define LIS3DSH_ACC_G_16G			(0x20)


struct lis3dsh_acc_platform_data {
	unsigned int poll_interval;
	unsigned int min_interval;

	u8 fs_range;

	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;

	u8 negate_x;
	u8 negate_y;
	u8 negate_z;

	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);

	/* set gpio_int either to the choosen gpio pin number or to -EINVAL
	 * if leaved unconnected
	 */
	int gpio_int1;
	int gpio_int2;
};

#endif /* __KERNEL__ */

#endif /* __LIS3DSH_H__ */
