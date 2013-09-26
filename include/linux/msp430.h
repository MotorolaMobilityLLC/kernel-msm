/*
 * Copyright (C) 2010-2012 Motorola, Inc.
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
 */

#ifndef __MSP430_H__
#define __MSP430_H__

/** The following define the IOCTL command values via the ioctl macros */
#define MSP430_IOCTL_BASE		77
#define MSP430_IOCTL_BOOTLOADERMODE	\
		_IOW(MSP430_IOCTL_BASE, 0, unsigned char)
#define MSP430_IOCTL_NORMALMODE	\
		_IOW(MSP430_IOCTL_BASE, 1, unsigned char)
#define MSP430_IOCTL_MASSERASE	\
		_IOW(MSP430_IOCTL_BASE, 2, unsigned char)
#define MSP430_IOCTL_SETSTARTADDR	\
		_IOW(MSP430_IOCTL_BASE, 3, unsigned int)
#define MSP430_IOCTL_TEST_READ	\
		_IOR(MSP430_IOCTL_BASE, 4, unsigned char)
#define MSP430_IOCTL_TEST_WRITE	\
		_IOW(MSP430_IOCTL_BASE, 5, unsigned char)
#define MSP430_IOCTL_TEST_WRITE_READ	\
		_IOWR(MSP430_IOCTL_BASE, 6, unsigned short)
#define MSP430_IOCTL_SET_MAG_DELAY	\
		_IOW(MSP430_IOCTL_BASE, 7, unsigned short)
#define MSP430_IOCTL_TEST_BOOTMODE	\
		_IOW(MSP430_IOCTL_BASE, 8, unsigned char)
#define MSP430_IOCTL_SET_ACC_DELAY	\
		_IOW(MSP430_IOCTL_BASE, 9,  unsigned short)
#define MSP430_IOCTL_SET_MOTION_DELAY	\
		_IOW(MSP430_IOCTL_BASE, 10, unsigned char)
#define MSP430_IOCTL_SET_GYRO_DELAY	\
		_IOW(MSP430_IOCTL_BASE, 11, unsigned short)
#define MSP430_IOCTL_SET_DEBUG	\
		_IOW(MSP430_IOCTL_BASE, 12, unsigned char)
#define MSP430_IOCTL_SET_USER_PROFILE	\
		_IOW(MSP430_IOCTL_BASE, 13, int)
#define MSP430_IOCTL_SET_GPS_DATA	\
		_IOW(MSP430_IOCTL_BASE, 14, int)
#define MSP430_IOCTL_SET_PRES_DELAY	\
		_IOW(MSP430_IOCTL_BASE, 15, unsigned short)
#define MSP430_IOCTL_SET_REF_ALTITUDE	\
		_IOW(MSP430_IOCTL_BASE, 16, int)
#define MSP430_IOCTL_SET_ACTIVE_MODE	\
		_IOW(MSP430_IOCTL_BASE, 17, unsigned char)
#define MSP430_IOCTL_SET_PASSIVE_MODE	\
		_IOW(MSP430_IOCTL_BASE, 18, unsigned char)
#define MSP430_IOCTL_SET_FACTORY_MODE	\
		_IOW(MSP430_IOCTL_BASE, 19, unsigned char)
#define MSP430_IOCTL_GET_SENSORS	\
		_IOR(MSP430_IOCTL_BASE, 20, unsigned char)
#define MSP430_IOCTL_SET_SENSORS	\
		_IOW(MSP430_IOCTL_BASE, 21, unsigned char)
#define MSP430_IOCTL_GET_VERSION	\
		_IOR(MSP430_IOCTL_BASE, 22, unsigned char)
#define MSP430_IOCTL_SET_MONITOR_DELAY	\
		_IOW(MSP430_IOCTL_BASE, 23, unsigned char)
#define MSP430_IOCTL_GET_DOCK_STATUS	\
		_IOR(MSP430_IOCTL_BASE, 24, unsigned char)
#define MSP430_IOCTL_SET_ORIENTATION_DELAY	\
		_IOW(MSP430_IOCTL_BASE, 25, unsigned char)
#define MSP430_IOCTL_SET_EQUIPMENT_TYPE	\
		_IOW(MSP430_IOCTL_BASE, 26, unsigned char)
#define MSP430_IOCTL_SET_POWER_MODE	\
		_IOW(MSP430_IOCTL_BASE, 27, unsigned char)
#define MSP430_IOCTL_GET_ALGOS	\
		_IOR(MSP430_IOCTL_BASE, 28, char*)
#define MSP430_IOCTL_SET_ALGOS	\
		_IOW(MSP430_IOCTL_BASE, 29, char*)
#define MSP430_IOCTL_GET_MAG_CAL \
		_IOR(MSP430_IOCTL_BASE, 30, unsigned char*)
#define MSP430_IOCTL_SET_MAG_CAL \
		_IOW(MSP430_IOCTL_BASE, 31, unsigned char*)
/* 32 unused */
#define MSP430_IOCTL_SET_MOTION_DUR	\
		_IOW(MSP430_IOCTL_BASE, 33, unsigned int)
/* 34 unused */
#define MSP430_IOCTL_SET_ZRMOTION_DUR	\
		_IOW(MSP430_IOCTL_BASE, 35, unsigned int)
#define MSP430_IOCTL_GET_WAKESENSORS	\
		_IOR(MSP430_IOCTL_BASE, 36, unsigned char)
#define MSP430_IOCTL_SET_WAKESENSORS	\
		_IOW(MSP430_IOCTL_BASE, 37, unsigned char)
#define MSP430_IOCTL_GET_VERNAME	\
		_IOW(MSP430_IOCTL_BASE, 38, char*)
#define MSP430_IOCTL_SET_POSIX_TIME	\
		_IOW(MSP430_IOCTL_BASE, 39, unsigned long)
#define MSP430_IOCTL_SET_CONTROL_REG	\
		_IOW(MSP430_IOCTL_BASE, 40, char*)
#define MSP430_IOCTL_GET_STATUS_REG	\
		_IOR(MSP430_IOCTL_BASE, 41, char*)
#define MSP430_IOCTL_GET_TOUCH_REG	\
		_IOR(MSP430_IOCTL_BASE, 42, char*)
#define MSP430_IOCTL_SET_ALGO_REQ \
		_IOR(MSP430_IOCTL_BASE, 43, char*)
#define MSP430_IOCTL_GET_ALGO_EVT \
		_IOR(MSP430_IOCTL_BASE, 44, char*)
#define MSP430_IOCTL_GET_AOD_INSTRUMENTATION_REG	\
		_IOR(MSP430_IOCTL_BASE, 45, char*)

#define FW_VERSION_SIZE 12
#define MSP_CONTROL_REG_SIZE 200
#define MSP_STATUS_REG_SIZE 8
#define MSP_TOUCH_REG_SIZE  8
#define MSP_MAG_CAL_SIZE 26
#define MSP_AOD_INSTRUMENTATION_REG_SIZE 256

#ifdef __KERNEL__
#define LIGHTING_TABLE_SIZE 32

struct msp430_platform_data {
	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);
	int gpio_reset;
	int gpio_bslen;
	int gpio_wakeirq;
	int gpio_int;
	int gpio_mipi_req;
	int gpio_mipi_busy;
	unsigned int bslen_pin_active_value;
	u16 lux_table[LIGHTING_TABLE_SIZE];
	u8 brightness_table[LIGHTING_TABLE_SIZE];
	char fw_version[FW_VERSION_SIZE];
	int ct406_detect_threshold;
	int ct406_undetect_threshold;
	int ct406_recalibrate_threshold;
	int ct406_pulse_count;
};
#endif /* __KERNEL__ */

/* Mask values */

/* Non wakable sensors */
#define M_ACCEL			0x0001
#define M_GYRO			0x0002
#define M_PRESSURE		0x0004
#define M_ECOMPASS		0x0008
#define M_TEMPERATURE		0x0010
#define M_ALS			0x0020

#define M_LIN_ACCEL		0x0100
#define M_QUATERNION		0x0200
#define M_GRAVITY		0x0400
#define M_DISP_ROTATE		0x0800
#define M_DISP_BRIGHTNESS	0x1000

/* wake sensor status */
#define M_DOCK			0x0001
#define M_PROXIMITY		0x0002
#define M_TOUCH			0x0004
#define M_HUB_RESET		0x0080

#define M_FLATUP		0x0100
#define M_FLATDOWN		0x0200
#define M_STOWED		0x0400
#define M_CAMERA_ACT	0x0800
#define M_NFC			0x1000
#define M_SIM			0x2000
#define M_LOG_MSG		0x8000

/* algo config mask */
#define M_MMOVEME               0x0001
#define M_NOMMOVE               0x0002
#define M_ALGO_MODALITY         0x0008
#define M_ALGO_ORIENTATION      0x0010
#define M_ALGO_STOWED           0x0020
#define M_ALGO_ACCUM_MODALITY   0x0040
#define M_ALGO_ACCUM_MVMT       0x0080

/* algo index */
#define MSP_IDX_MODALITY        0
#define MSP_IDX_ORIENTATION     1
#define MSP_IDX_STOWED          2
#define MSP_IDX_ACCUM_MODALITY  3
#define MSP_IDX_ACCUM_MVMT      4

#define MSP_NUM_ALGOS           5

#define MSP_EVT_SZ_TRANSITION   7
#define MSP_EVT_SZ_ACCUM_STATE  2
#define MSP_EVT_SZ_ACCUM_MVMT   4

struct msp430_android_sensor_data {
	int64_t timestamp;
	signed short data1;
	signed short data2;
	signed short data3;
	signed short data4;
	signed short data5;
	signed short data6;
	unsigned char type;
	unsigned char status;
};

struct msp430_moto_sensor_data {
	int64_t timestamp;
	signed short data1;
	signed short data2;
	signed short data3;
	signed short data4;
	unsigned char type;
};

enum MSP430_data_types {
	DT_ACCEL,
	DT_GYRO,
	DT_PRESSURE,
	DT_MAG,
	DT_ORIENT,
	DT_TEMP,
	DT_ALS,
	DT_LIN_ACCEL,
	DT_QUATERNION,
	DT_GRAVITY,
	DT_DISP_ROTATE,
	DT_DISP_BRIGHT,
	DT_DOCK,
	DT_PROX,
	DT_FLAT_UP,
	DT_FLAT_DOWN,
	DT_STOWED,
	DT_MMMOVE,
	DT_NOMOVE,
	DT_CAMERA_ACT,
	DT_NFC,
	DT_ALGO_EVT,
	DT_ACCUM_MVMT,
	DT_SIM,
	DT_RESET
};

enum {
	NO_DOCK,
	DESK_DOCK,
	CAR_DOCK
};

#endif  /* __MSP430_H__ */

