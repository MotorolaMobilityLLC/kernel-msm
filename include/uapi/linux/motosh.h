/*
 * Copyright (c) 2015, Motorola Mobility LLC
 * All rights reserved.
 *
 * Redistribution and use in source and binary forms, with or without
 * modification, are permitted provided that the following conditions are met:
 *     - Redistributions of source code must retain the above copyright
 *       notice, this list of conditions and the following disclaimer.
 *     - Redistributions in binary form must reproduce the above copyright
 *       notice, this list of conditions and the following disclaimer in the
 *       documentation and/or other materials provided with the distribution.
 *     - Neither the name of Motorola Mobility nor the
 *       names of its contributors may be used to endorse or promote products
 *       derived from this software without specific prior written permission.
 *
 * THIS SOFTWARE IS PROVIDED BY THE COPYRIGHT HOLDERS AND CONTRIBUTORS "AS IS"
 * AND ANY EXPRESS OR IMPLIED WARRANTIES, INCLUDING, BUT NOT LIMITED TO,
 * THE IMPLIED WARRANTIES OF MERCHANTABILITY AND FITNESS FOR A PARTICULAR
 * PURPOSE ARE DISCLAIMED. IN NO EVENT SHALL MOTOROLA MOBILITY LLC BE LIABLE
 * FOR ANY DIRECT, INDIRECT, INCIDENTAL, SPECIAL, EXEMPLARY, OR CONSEQUENTIAL
 * DAMAGES (INCLUDING, BUT NOT LIMITED TO, PROCUREMENT OF SUBSTITUTE GOODS
 * OR SERVICES; LOSS OF USE, DATA, OR PROFITS; OR BUSINESS INTERRUPTION)
 * HOWEVER CAUSED AND ON ANY THEORY OF LIABILITY, WHETHER IN CONTRACT, STRICT
 * LIABILITY, OR TORT (INCLUDING NEGLIGENCE OR OTHERWISE) ARISING IN ANY WAY
 * OUT OF THE USE OF THIS SOFTWARE, EVEN IF ADVISED OF THE POSSIBILITY OF SUCH
 * DAMAGE.
 */

#ifndef _UAPI_MOTOSH_H
#define _UAPI_MOTOSH_H

/* sensor handle definitions */
#define MIN_SENSOR_ID (0)
#define ID_A  (0)  /* Accelerometer */
#define ID_G  (1)  /* Gyroscope */
#define ID_PR (2)  /* Pressure */
#define ID_M  (3)  /* Magnetometer */
#define ID_O  (4)  /* Orientation */
#define ID_T  (5)  /* Temperature */
#define ID_L  (6)  /* Light */

#define ID_LA (7)  /* Linear Acceleration */
#define ID_Q  (8)  /* Quaternion */
#define ID_GRAVITY (9)  /* Gravity */
#define ID_DR (10) /* Display Rotate */
#define ID_DB (11) /* Display Brightness */

#define ID_D  (12) /* Dock */
#define ID_P  (13) /* Proximity */

#define ID_FU (14) /* Flat Up */
#define ID_FD (15) /* Flat Down */
#define ID_S  (16) /* Stowed */
#define ID_CA (17) /* Camera Activate */
#define ID_NFC (18) /* NFC Detect */
#define ID_IR_GESTURE (19) /* IR Gesture */
#define ID_IR_RAW     (20) /* IR Raw Data */
#define ID_SIM (21) /* Significant motion */
#define ID_STEP_DETECTOR (22) /* Step detector */
#define ID_STEP_COUNTER  (23) /* Step counter */
#define ID_UNCALIB_GYRO  (24) /* Uncalibrated Gyroscope */
#define ID_UNCALIB_MAG   (25) /* Uncalibrated Magenetometer */
#define ID_IR_OBJECT (26) /* IR Object Detect */
#define ID_CHOPCHOP_GESTURE (27) /* ChopChop Gesture */
#define ID_QUAT_6AXIS (28)
#define ID_QUAT_9AXIS (29)
#define ID_LIFT_GESTURE (30)
#define ID_GLANCE_GESTURE (31)
#define ID_RP (32) /* Moto Rear Proximity */
#define ID_MOTO_GLANCE_GESTURE (33)
#define ID_MOTO_MOD_CURRENT_DRAIN (34)
#define ID_GAME_RV (35)
#define ID_SENSOR_SYNC (36)
#define ID_ULTRASOUND_GESTURE (37)
#define ID_MOTION_DETECT (38)
#define ID_STATIONARY_DETECT (39)
#define MAX_SENSOR_ID (39)

/* structure to hold rate and timeout for sensor batching */
struct motosh_moto_sensor_batch_cfg {
	unsigned short delay;
	uint32_t timeout;
};

/** The following define the IOCTL command values via the ioctl macros */
#define MOTOSH_IOCTL_BASE		77
#define MOTOSH_IOCTL_BOOTLOADERMODE	\
		_IOW(MOTOSH_IOCTL_BASE, 0, unsigned char)
#define MOTOSH_IOCTL_NORMALMODE	\
		_IOW(MOTOSH_IOCTL_BASE, 1, unsigned char)
#define MOTOSH_IOCTL_MASSERASE	\
		_IOW(MOTOSH_IOCTL_BASE, 2, unsigned char)
#define MOTOSH_IOCTL_SETSTARTADDR	\
		_IOW(MOTOSH_IOCTL_BASE, 3, unsigned int)
#define MOTOSH_IOCTL_TEST_READ	\
		_IOR(MOTOSH_IOCTL_BASE, 4, unsigned char)
#define MOTOSH_IOCTL_TEST_WRITE	\
		_IOW(MOTOSH_IOCTL_BASE, 5, unsigned char)
#define MOTOSH_IOCTL_TEST_WRITE_READ	\
		_IOWR(MOTOSH_IOCTL_BASE, 6, unsigned short)
#define MOTOSH_IOCTL_SET_MAG_DELAY	\
		_IOW(MOTOSH_IOCTL_BASE, 7, unsigned short)
#define MOTOSH_IOCTL_TEST_BOOTMODE	\
		_IOW(MOTOSH_IOCTL_BASE, 8, unsigned char)
#define MOTOSH_IOCTL_SET_ACC_DELAY	\
		_IOW(MOTOSH_IOCTL_BASE, 9, struct motosh_moto_sensor_batch_cfg)
#define MOTOSH_IOCTL_SET_MOTION_DELAY	\
		_IOW(MOTOSH_IOCTL_BASE, 10, unsigned char)
#define MOTOSH_IOCTL_SET_GYRO_DELAY	\
		_IOW(MOTOSH_IOCTL_BASE, 11, unsigned short)
#define MOTOSH_IOCTL_SET_DEBUG	\
		_IOW(MOTOSH_IOCTL_BASE, 12, unsigned char)
#define MOTOSH_IOCTL_SET_USER_PROFILE	\
		_IOW(MOTOSH_IOCTL_BASE, 13, int)
#define MOTOSH_IOCTL_SET_GPS_DATA	\
		_IOW(MOTOSH_IOCTL_BASE, 14, int)
#define MOTOSH_IOCTL_SET_PRES_DELAY	\
		_IOW(MOTOSH_IOCTL_BASE, 15, unsigned short)
#define MOTOSH_IOCTL_SET_REF_ALTITUDE	\
		_IOW(MOTOSH_IOCTL_BASE, 16, int)
#define MOTOSH_IOCTL_SET_ACTIVE_MODE	\
		_IOW(MOTOSH_IOCTL_BASE, 17, unsigned char)
#define MOTOSH_IOCTL_SET_PASSIVE_MODE	\
		_IOW(MOTOSH_IOCTL_BASE, 18, unsigned char)
#define MOTOSH_IOCTL_SET_FACTORY_MODE	\
		_IOW(MOTOSH_IOCTL_BASE, 19, unsigned char)
#define MOTOSH_IOCTL_GET_SENSORS	\
		_IOR(MOTOSH_IOCTL_BASE, 20, unsigned char)
#define MOTOSH_IOCTL_SET_SENSORS	\
		_IOW(MOTOSH_IOCTL_BASE, 21, unsigned char)
#define MOTOSH_IOCTL_GET_VERSION	\
		_IOR(MOTOSH_IOCTL_BASE, 22, unsigned char)
#define MOTOSH_IOCTL_SET_MONITOR_DELAY	\
		_IOW(MOTOSH_IOCTL_BASE, 23, unsigned char)
#define MOTOSH_IOCTL_GET_DOCK_STATUS	\
		_IOR(MOTOSH_IOCTL_BASE, 24, unsigned char)
#define MOTOSH_IOCTL_SET_ORIENTATION_DELAY	\
		_IOW(MOTOSH_IOCTL_BASE, 25, unsigned char)
#define MOTOSH_IOCTL_SET_EQUIPMENT_TYPE	\
		_IOW(MOTOSH_IOCTL_BASE, 26, unsigned char)
#define MOTOSH_IOCTL_SET_POWER_MODE	\
		_IOW(MOTOSH_IOCTL_BASE, 27, unsigned char)
#define MOTOSH_IOCTL_GET_ALGOS	\
		_IOR(MOTOSH_IOCTL_BASE, 28, char[MOTOSH_ALGO_SIZE])
#define MOTOSH_IOCTL_SET_ALGOS	\
		_IOW(MOTOSH_IOCTL_BASE, 29, char[MOTOSH_ALGO_SIZE])
#define MOTOSH_IOCTL_GET_MAG_CAL \
		_IOR(MOTOSH_IOCTL_BASE, 30, char[MOTOSH_MAG_CAL_SIZE])
#define MOTOSH_IOCTL_SET_MAG_CAL \
		_IOW(MOTOSH_IOCTL_BASE, 31, char[MOTOSH_MAG_CAL_SIZE])
#define MOTOSH_IOCTL_GET_VERSION_STR \
		_IOW(MOTOSH_IOCTL_BASE, 32, char[FW_VERSION_STR_MAX_LEN])
#define MOTOSH_IOCTL_SET_MOTION_DUR	\
		_IOW(MOTOSH_IOCTL_BASE, 33, unsigned int)
#define MOTOSH_IOCTL_GET_FLASH_CRC \
		_IOW(MOTOSH_IOCTL_BASE, 34, unsigned int)
#define MOTOSH_IOCTL_SET_ZRMOTION_DUR	\
		_IOW(MOTOSH_IOCTL_BASE, 35, unsigned int)
#define MOTOSH_IOCTL_GET_WAKESENSORS	\
		_IOR(MOTOSH_IOCTL_BASE, 36, unsigned char)
#define MOTOSH_IOCTL_SET_WAKESENSORS	\
		_IOW(MOTOSH_IOCTL_BASE, 37, unsigned char)
#define MOTOSH_IOCTL_GET_VERNAME	\
		_IOW(MOTOSH_IOCTL_BASE, 38, char[FW_VERSION_SIZE])
#define MOTOSH_IOCTL_SET_POSIX_TIME	\
		_IOW(MOTOSH_IOCTL_BASE, 39, char[1])
#define MOTOSH_IOCTL_GET_GYRO_CAL \
		_IOR(MOTOSH_IOCTL_BASE, 40, char[MOTOSH_GYRO_CAL_SIZE])
#define MOTOSH_IOCTL_SET_GYRO_CAL \
		_IOR(MOTOSH_IOCTL_BASE, 41, char[MOTOSH_GYRO_CAL_SIZE])
#define MOTOSH_IOCTL_SET_ALS_DELAY \
		_IOW(MOTOSH_IOCTL_BASE, 42, unsigned short)
#define MOTOSH_IOCTL_SET_ALGO_REQ \
		_IOR(MOTOSH_IOCTL_BASE, 43, char[1])
#define MOTOSH_IOCTL_GET_ALGO_EVT \
		_IOR(MOTOSH_IOCTL_BASE, 44, char[1])

#ifdef CONFIG_SENSORS_MOTOSH_MOTODISP
#define MOTOSH_IOCTL_ENABLE_BREATHING \
		_IOW(MOTOSH_IOCTL_BASE, 45, unsigned char)
#endif /* CONFIG_SENSORS_MOTOSH_MOTODISP */

#define MOTOSH_IOCTL_WRITE_REG \
		_IOR(MOTOSH_IOCTL_BASE, 46, char[1])
#define MOTOSH_IOCTL_READ_REG \
		_IOR(MOTOSH_IOCTL_BASE, 47, char[1])
#define MOTOSH_IOCTL_SET_STEP_COUNTER_DELAY	\
		_IOW(MOTOSH_IOCTL_BASE, 48,  unsigned short)
#define MOTOSH_IOCTL_GET_IR_CONFIG \
		_IOWR(MOTOSH_IOCTL_BASE, 49, char[1])
#define MOTOSH_IOCTL_SET_IR_CONFIG \
		_IOW(MOTOSH_IOCTL_BASE, 50, char[1])
#define MOTOSH_IOCTL_SET_IR_GESTURE_DELAY	\
		_IOW(MOTOSH_IOCTL_BASE, 51,  unsigned short)
#define MOTOSH_IOCTL_SET_IR_RAW_DELAY	\
		_IOW(MOTOSH_IOCTL_BASE, 52,  unsigned short)
#define MOTOSH_IOCTL_GET_BOOTED \
		_IOR(MOTOSH_IOCTL_BASE, 53, unsigned char)
#define MOTOSH_IOCTL_SET_LOWPOWER_MODE \
		_IOW(MOTOSH_IOCTL_BASE, 54, char)
#define MOTOSH_IOCTL_SET_FLUSH \
		_IOW(MOTOSH_IOCTL_BASE, 55, int)
#define MOTOSH_IOCTL_SET_ANTCAP_ENABLE \
		_IOW(MOTOSH_IOCTL_BASE, 56, unsigned char)
#define MOTOSH_IOCTL_SET_ANTCAP_CFG \
		_IOW(MOTOSH_IOCTL_BASE, 57, unsigned char*)
#define MOTOSH_IOCTL_SET_ANTCAP_CAL \
		_IOW(MOTOSH_IOCTL_BASE, 58, unsigned char*)
#define MOTOSH_IOCTL_SET_HEADSET_STATE \
		_IOW(MOTOSH_IOCTL_BASE, 59, unsigned char)
#define MOTOSH_IOCTL_SET_USBCONN_STATE \
		_IOW(MOTOSH_IOCTL_BASE, 60, unsigned char)
#define MOTOSH_IOCTL_SET_AIRPLANE_MODE \
		_IOW(MOTOSH_IOCTL_BASE, 61, unsigned char)
#define MOTOSH_IOCTL_PASSTHROUGH \
		_IOR(MOTOSH_IOCTL_BASE, 62, char[1])
#define MOTOSH_IOCTL_GET_ACCEL_CAL \
		_IOR(MOTOSH_IOCTL_BASE, 63, char[MOTOSH_ACCEL_CAL_SIZE])
#define MOTOSH_IOCTL_SET_ACCEL_CAL \
		_IOR(MOTOSH_IOCTL_BASE, 64, char[MOTOSH_ACCEL_CAL_SIZE])
#define MOTOSH_IOCTL_SET_VR_MODE	\
		_IOW(MOTOSH_IOCTL_BASE, 65, unsigned char)


/* Used in HAL */
#define FW_VERSION_SIZE 12
#define FW_VERSION_STR_MAX_LEN 256

#define MOTOSH_MAXDATA_LENGTH		256
/* The SH register number/ID size. */
#define MOTOSH_REGISTER_LEN		1
/* Used in libsensorhub. Maximum that can be RX/TX with READ/WRITE_REG IOCTL */
#define MOTOSH_TX_PAYLOAD_LEN	(MOTOSH_MAXDATA_LENGTH - MOTOSH_REGISTER_LEN)
#define MOTOSH_RX_PAYLOAD_LEN	(MOTOSH_MAXDATA_LENGTH)

/* Not used in user space */
#define MOTOSH_CONTROL_REG_SIZE 200
/* Not used in user space */
#define MOTOSH_STATUS_REG_SIZE 8
/* Not used */
#define MOTOSH_TOUCH_REG_SIZE  8
/* Used in HAL */
#define MOTOSH_MAG_CAL_SIZE 32
#define MOTOSH_GYRO_CAL_SIZE 198 /* 33 entries - 6 bytes each */
#define MOTOSH_ACCEL_CAL_SIZE 6 /* 1 entry - 6 bytes */
/* Not used */
#define STM_AOD_INSTRUMENTATION_REG_SIZE 256
/* Not used in user space */
#define MOTOSH_AS_DATA_BUFF_SIZE 20
/* Not used in user space */
#define MOTOSH_MS_DATA_BUFF_SIZE 20
/* Used ONLY in HAL */
#define MOTOSH_CAMERA_DATA 0x01
/* Used only for ioctl def */
#define MOTOSH_ALGO_SIZE         2
/* Used in kernel and HAL */
#define MOTOSH_PASSTHROUGH_SIZE 16

#define MOTOSH_ANTCAP_CAL_BUFF_SIZE  64
#define MOTOSH_ANTCAP_CFG_BUFF_SIZE  64

/* Mask values */
/* Non wakable sensors */
#define M_ACCEL			0x000001
#define M_GYRO			0x000002
#define M_PRESSURE		0x000004
#define M_ECOMPASS		0x000008

#define M_ALS			0x000020
#define M_STEP_DETECTOR	0x000040
#define M_STEP_COUNTER	0x000080

#define M_LIN_ACCEL		0x000100
#define M_QUAT_6AXIS	0x000200
#define M_GRAVITY		0x000400
#define M_DISP_ROTATE		0x000800
#define M_DISP_BRIGHTNESS	0x001000
#define M_IR_GESTURE        0x002000
#define M_IR_RAW            0x004000

#define M_UNCALIB_GYRO		0x008000
#define M_UNCALIB_MAG		0x010000
#define M_IR_OBJECT		0x020000
#define M_QUAT_9AXIS		0x040000
#define M_MOTO_MOD_CURRENT_DRAIN		0x080000
#define M_GAME_RV		0x400000
#define M_SENSOR_SYNC		0x800000

/* wake sensor status */
#define M_DOCK			0x000001
#define M_PROXIMITY		0x000002
#define M_TOUCH			0x000004
#define M_COVER			0x000008
#define M_QUICKPEEK		0x000010
#define M_LIFT			0x000020
#define M_INIT_COMPLETE		0x000040
#define M_HUB_RESET		0x000080

#define M_FLATUP		0x000100
#define M_FLATDOWN		0x000200
#define M_STOWED		0x000400
#define M_CAMERA_ACT		0x000800
#define M_NFC			0x001000
#define M_SIM			0x002000
#define M_CHOPCHOP		0x004000
#define M_LOG_MSG		0x008000

/*#define M_UNUSED              0x010000*/
/*#define M_UNUSED              0x020000*/
#define M_GLANCE                0x040000
/*#define M_UNUSED              0x080000*/
#define M_MOTION_DETECT         0x100000
#define M_STATIONARY_DETECT     0x200000
/*#define M_UNUSED              0x400000*/
/*#define M_UNUSED              0x800000*/


/* algo config mask */
#define M_MMOVEME               0x000001
#define M_NOMMOVE               0x000002
#define M_ALGO_MODALITY         0x000008
#define M_ALGO_ORIENTATION      0x000010
#define M_ALGO_STOWED           0x000020
#define M_ALGO_ACCUM_MODALITY   0x000040
#define M_ALGO_ACCUM_MVMT       0x000080

#define M_IR_WAKE_GESTURE       0x200000

/* generic interrupt mask */
#define M_GENERIC_INTRPT        0x0080

/* algo index */
#define MOTOSH_IDX_MODALITY        0
#define MOTOSH_IDX_ORIENTATION     1
#define MOTOSH_IDX_STOWED          2
#define MOTOSH_IDX_ACCUM_MODALITY  3
#define MOTOSH_IDX_ACCUM_MVMT      4

#define MOTOSH_NUM_ALGOS           5

#define MOTOSH_EVT_SZ_TRANSITION   7
#define MOTOSH_EVT_SZ_ACCUM_STATE  2
#define MOTOSH_EVT_SZ_ACCUM_MVMT   4

/* VR defines */
#define VR_ACCEL 0x01
#define VR_GYRO  0x02
#define VR_MAG   0x04
#define VR_READY 0x80

struct motosh_android_sensor_data {
	int64_t timestamp;
	unsigned char type;
	unsigned char data[MOTOSH_AS_DATA_BUFF_SIZE];
	int size;
	unsigned char status;
};

struct motosh_moto_sensor_data {
	int64_t timestamp;
	unsigned char type;
	unsigned char data[MOTOSH_MS_DATA_BUFF_SIZE];
	int size;
};

enum MOTOSH_data_types {
	DT_ACCEL,
	DT_GYRO,
	DT_PRESSURE,
	DT_MAG,
	DT_ORIENT,
	DT_TEMP,
	DT_ALS,
	DT_LIN_ACCEL,
	DT_QUAT_6AXIS,
	DT_QUAT_9AXIS,
	DT_GRAVITY,
	DT_DISP_ROTATE,
	DT_DISP_BRIGHT,
	DT_DOCK,
	DT_PROX,
	DT_COVER,
	DT_FLAT_UP,
	DT_FLAT_DOWN,
	DT_STOWED,
	DT_MMMOVE,
	DT_NOMOVE,
	DT_CAMERA_ACT,
	DT_NFC,
	DT_ALGO_EVT,
	DT_ACCUM_MVMT,
	DT_IR_GESTURE,
	DT_IR_RAW,
	DT_IR_OBJECT,
	DT_SIM,
	DT_RESET,
	DT_GENERIC_INT,
	DT_STEP_COUNTER,
	DT_STEP_DETECTOR,
	DT_UNCALIB_GYRO,
	DT_UNCALIB_MAG,
	DT_CHOPCHOP,
	DT_FLUSH,
	DT_LIFT,
	DT_GYRO_CAL,
	DT_GLANCE,
	DT_ACCEL_CAL,
	DT_MOTO_MOD_CURRENT_DRAIN,
	DT_GAME_RV,
	DT_SENSOR_SYNC,
	DT_MOTION_DETECT,
	DT_STATIONARY_DETECT
};

enum {
	NO_DOCK,
	DESK_DOCK,
	CAR_DOCK
};

enum stm_mode {
	UNINITIALIZED,
	BOOTMODE,
	NORMALMODE,
	FACTORYMODE
};

enum reset_mode {
	START_RESET,
	COMPLETE_INIT
};

enum stm_commands {
	PASSWORD_RESET,
	MASS_ERASE,
	PROGRAM_CODE,
	END_FIRMWARE,
	PASSWORD_RESET_DEFAULT,
	CRC_CHECK
};

struct stm_response {
	/* 0x0080 */
	unsigned short header;
	unsigned char len_lsb;
	unsigned char len_msb;
	unsigned char cmd;
	unsigned char data;
	unsigned char crc_lsb;
	unsigned char crc_msb;
};

#endif /*_UAPI_MOTOSH_H*/
