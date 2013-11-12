/*
********************* (C) COPYRIGHT 2012 STMicroelectronics ********************
*
* File Name          : lis3dsh.h
* Authors            : MH - C&I BU - Application Team
*		     : Matteo Dameno (matteo.dameno@st.com)
*		     : Denis Ciocca (denis.ciocca@st.com)
* Version            : V.1.0.0
* Date               : 2012/08/01
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
********************************************************************************
SYSFS interface
- range: set full scale
	-> accelerometer:	2,4,6,8,16				[g]
- pollrate_ms: set 1/ODR
	-> accelerometer:	LIS3DH_ACC_MIN_POLL_PERIOD_MS < t	[ms]
- enable_device: enable/disable sensor					[1/0]


INPUT subsystem: NOTE-> output data INCLUDE the sensitivity in accelerometer.
- accelerometer:	abs_x, abs_y, abs_z		[ug]
*******************************************************************************/

#ifndef	__LIS3DSH_H__
#define	__LIS3DSH_H__


#define	LIS3DSH_ACC_DEV_NAME			"lis3dsh_acc"


/* Poll Interval */
#define	LIS3DSH_ACC_MIN_POLL_PERIOD_MS		1


#ifdef	__KERNEL__

/* Interrupt */
#define LIS3DSH_ACC_DEFAULT_INT1_GPIO		(-EINVAL)
#define LIS3DSH_ACC_DEFAULT_INT2_GPIO		(-EINVAL)


/* Accelerometer Sensor Full Scale */
#define LIS3DSH_ACC_G_2G			(0x00)
#define LIS3DSH_ACC_G_4G			(0x08)
#define LIS3DSH_ACC_G_6G			(0x10)
#define LIS3DSH_ACC_G_8G			(0x18)
#define LIS3DSH_ACC_G_16G			(0x20)

/* STATE PROGRAMS ENABLE CONTROLS */
#define	LIS3DSH_SM1_DIS_SM2_DIS			0x00
#define	LIS3DSH_SM1_DIS_SM2_EN			0x01
#define	LIS3DSH_SM1_EN_SM2_DIS			0x02
#define	LIS3DSH_SM1_EN_SM2_EN			0x03
/* */

/* INTERRUPTS ENABLE CONTROLS */
#define	LIS3DSH_INT1_DIS_INT2_DIS		0x00
#define	LIS3DSH_INT1_DIS_INT2_EN		0x01
#define	LIS3DSH_INT1_EN_INT2_DIS		0x02
#define	LIS3DSH_INT1_EN_INT2_EN			0x03
/* */

struct lis3dsh_acc_platform_data {
	u32 poll_interval;
	u32 min_interval;

	u8 fs_range;

	u8 axis_map_x;
	u8 axis_map_y;
	u8 axis_map_z;

	bool negate_x;
	bool negate_y;
	bool negate_z;

	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);

	int gpio_int1;
	int gpio_int2;
};

#endif	/* __KERNEL__ */

#endif	/* __LIS3DSH_H__ */
