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


#ifndef _UAPI_STML0XX_H
#define _UAPI_STML0XX_H

/** The following define the IOCTL command values via the ioctl macros */
#define STML0XX_IOCTL_BASE		77
#define STML0XX_IOCTL_BOOTLOADERMODE	\
		_IOW(STML0XX_IOCTL_BASE, 0, unsigned char)
#define STML0XX_IOCTL_NORMALMODE	\
		_IOW(STML0XX_IOCTL_BASE, 1, unsigned char)
#define STML0XX_IOCTL_MASSERASE	\
		_IOW(STML0XX_IOCTL_BASE, 2, unsigned char)
#define STML0XX_IOCTL_SETSTARTADDR	\
		_IOW(STML0XX_IOCTL_BASE, 3, unsigned int)
#define STML0XX_IOCTL_TEST_READ	\
		_IOR(STML0XX_IOCTL_BASE, 4, unsigned char)
#define STML0XX_IOCTL_TEST_WRITE	\
		_IOW(STML0XX_IOCTL_BASE, 5, unsigned char)
#define STML0XX_IOCTL_TEST_WRITE_READ	\
		_IOWR(STML0XX_IOCTL_BASE, 6, unsigned short)
#define STML0XX_IOCTL_SET_MAG_DELAY	\
		_IOW(STML0XX_IOCTL_BASE, 7, unsigned short)
#define STML0XX_IOCTL_TEST_BOOTMODE	\
		_IOW(STML0XX_IOCTL_BASE, 8, unsigned char)
#define STML0XX_IOCTL_SET_ACC_DELAY	\
		_IOW(STML0XX_IOCTL_BASE, 9,  unsigned short)
#define STML0XX_IOCTL_SET_MOTION_DELAY	\
		_IOW(STML0XX_IOCTL_BASE, 10, unsigned char)
#define STML0XX_IOCTL_SET_GYRO_DELAY	\
		_IOW(STML0XX_IOCTL_BASE, 11, unsigned short)
#define STML0XX_IOCTL_SET_DEBUG	\
		_IOW(STML0XX_IOCTL_BASE, 12, unsigned char)
#define STML0XX_IOCTL_SET_USER_PROFILE	\
		_IOW(STML0XX_IOCTL_BASE, 13, int)
#define STML0XX_IOCTL_SET_GPS_DATA	\
		_IOW(STML0XX_IOCTL_BASE, 14, int)
#define STML0XX_IOCTL_SET_PRES_DELAY	\
		_IOW(STML0XX_IOCTL_BASE, 15, unsigned short)
#define STML0XX_IOCTL_SET_REF_ALTITUDE	\
		_IOW(STML0XX_IOCTL_BASE, 16, int)
#define STML0XX_IOCTL_SET_ACTIVE_MODE	\
		_IOW(STML0XX_IOCTL_BASE, 17, unsigned char)
#define STML0XX_IOCTL_SET_PASSIVE_MODE	\
		_IOW(STML0XX_IOCTL_BASE, 18, unsigned char)
#define STML0XX_IOCTL_SET_FACTORY_MODE	\
		_IOW(STML0XX_IOCTL_BASE, 19, unsigned char)
#define STML0XX_IOCTL_GET_SENSORS	\
		_IOR(STML0XX_IOCTL_BASE, 20, unsigned char)
#define STML0XX_IOCTL_SET_SENSORS	\
		_IOW(STML0XX_IOCTL_BASE, 21, unsigned char)
#define STML0XX_IOCTL_GET_VERSION	\
		_IOR(STML0XX_IOCTL_BASE, 22, unsigned char)
#define STML0XX_IOCTL_SET_MONITOR_DELAY	\
		_IOW(STML0XX_IOCTL_BASE, 23, unsigned char)
#define STML0XX_IOCTL_GET_DOCK_STATUS	\
		_IOR(STML0XX_IOCTL_BASE, 24, unsigned char)
#define STML0XX_IOCTL_SET_ORIENTATION_DELAY	\
		_IOW(STML0XX_IOCTL_BASE, 25, unsigned char)
#define STML0XX_IOCTL_SET_EQUIPMENT_TYPE	\
		_IOW(STML0XX_IOCTL_BASE, 26, unsigned char)
#define STML0XX_IOCTL_SET_POWER_MODE	\
		_IOW(STML0XX_IOCTL_BASE, 27, unsigned char)
#define STML0XX_IOCTL_GET_ALGOS	\
		_IOR(STML0XX_IOCTL_BASE, 28, char[STML0XX_ALGO_SIZE])
#define STML0XX_IOCTL_SET_ALGOS	\
		_IOW(STML0XX_IOCTL_BASE, 29, char[STML0XX_ALGO_SIZE])
#define STML0XX_IOCTL_GET_MAG_CAL \
		_IOR(STML0XX_IOCTL_BASE, 30, char[STML0XX_MAG_CAL_SIZE])
#define STML0XX_IOCTL_SET_MAG_CAL \
		_IOW(STML0XX_IOCTL_BASE, 31, char[STML0XX_MAG_CAL_SIZE])
#define STML0XX_IOCTL_SET_ALS_DELAY \
		_IOW(STML0XX_IOCTL_BASE, 32,  unsigned short)
#define STML0XX_IOCTL_SET_MOTION_DUR	\
		_IOW(STML0XX_IOCTL_BASE, 33, unsigned int)
#define STML0XX_IOCTL_SET_STEP_COUNTER_DELAY	\
		_IOW(STML0XX_IOCTL_BASE, 34, unsigned short)
#define STML0XX_IOCTL_SET_ZRMOTION_DUR	\
		_IOW(STML0XX_IOCTL_BASE, 35, unsigned int)
#define STML0XX_IOCTL_GET_WAKESENSORS	\
		_IOR(STML0XX_IOCTL_BASE, 36, unsigned char)
#define STML0XX_IOCTL_SET_WAKESENSORS	\
		_IOW(STML0XX_IOCTL_BASE, 37, unsigned char)
#define STML0XX_IOCTL_GET_VERNAME	\
		_IOW(STML0XX_IOCTL_BASE, 38, char[FW_VERSION_SIZE])
#define STML0XX_IOCTL_SET_POSIX_TIME	\
		_IOW(STML0XX_IOCTL_BASE, 39, unsigned long)
#define STML0XX_IOCTL_GET_GYRO_CAL \
		_IOR(STML0XX_IOCTL_BASE, 40, char[STML0XX_GYRO_CAL_SIZE])
#define STML0XX_IOCTL_SET_GYRO_CAL \
		_IOR(STML0XX_IOCTL_BASE, 41, char[STML0XX_GYRO_CAL_SIZE])
/* 42 unused */
#define STML0XX_IOCTL_SET_ALGO_REQ \
		_IOR(STML0XX_IOCTL_BASE, 43, char[1])
#define STML0XX_IOCTL_GET_ALGO_EVT \
		_IOR(STML0XX_IOCTL_BASE, 44, char[1])
#define STML0XX_IOCTL_ENABLE_BREATHING \
		_IOW(STML0XX_IOCTL_BASE, 45, unsigned char)
#define STML0XX_IOCTL_WRITE_REG \
		_IOR(STML0XX_IOCTL_BASE, 46, char[1])
#define STML0XX_IOCTL_READ_REG \
		_IOR(STML0XX_IOCTL_BASE, 47, char[1])
#define STML0XX_IOCTL_PASSTHROUGH \
		_IOR(STML0XX_IOCTL_BASE, 48, char[1])
/* 49-52 unused */
#define STML0XX_IOCTL_GET_BOOTED \
		_IOR(STML0XX_IOCTL_BASE, 53, unsigned char)
#define STML0XX_IOCTL_SET_LOWPOWER_MODE \
		_IOW(STML0XX_IOCTL_BASE, 54, char)
#define STML0XX_IOCTL_SET_ACC2_DELAY	\
		_IOW(STML0XX_IOCTL_BASE, 55,  unsigned short)
#define STML0XX_IOCTL_SET_FLUSH \
		_IOW(STML0XX_IOCTL_BASE, 56, int)
#define STML0XX_IOCTL_GET_ACCEL_CAL \
		_IOR(STML0XX_IOCTL_BASE, 57, char[STML0XX_ACCEL_CAL_SIZE])
#define STML0XX_IOCTL_SET_ACCEL_CAL \
		_IOR(STML0XX_IOCTL_BASE, 58, char[STML0XX_ACCEL_CAL_SIZE])

/* SPI contants */
#define SPI_MSG_SIZE        256
#define SPI_TX_HDR_SIZE     6
#define SPI_CRC_SIZE        2
#define SPI_TX_PAYLOAD_LEN  (SPI_MSG_SIZE - SPI_TX_HDR_SIZE - SPI_CRC_SIZE)
#define SPI_RX_PAYLOAD_LEN  (SPI_MSG_SIZE - SPI_CRC_SIZE)

#define FW_VERSION_SIZE                  12
#define FW_VERSION_STR_MAX_LEN           256u
#define STML0XX_CONTROL_REG_SIZE         200
#define STML0XX_STATUS_REG_SIZE          8
#define STML0XX_TOUCH_REG_SIZE           8
#define STML0XX_POWER_REG_SIZE           3
#define STML0XX_MAG_CAL_SIZE             26
#define STML0XX_GYRO_CAL_FIRST           102 /* 17 entries - 6 bytes each */
#define STML0XX_GYRO_CAL_SECOND          96  /* 16 entries - 6 bytes each */
#define STML0XX_GYRO_CAL_SIZE            198 /* 33 entries - 6 bytes each */
#define STML0XX_ACCEL_CAL_SIZE           6   /* 1 entry    - 6 bytes      */
#define STML0XX_AS_DATA_BUFF_SIZE        20
#define STML0XX_MS_DATA_BUFF_SIZE        20
#define STML0XX_ALGO_SIZE                2
#define STML0XX_PASSTHROUGH_SIZE         16
#define STML0XX_MAX_WRITE_REG_LEN        255
#define STML0XX_MAX_READ_REG_LEN         SPI_RX_PAYLOAD_LEN
#define STML0XX_MAX_FLASH_PACKET_LENGTH  256

#define STML0XX_CAMERA_DATA 0x01

/* Mask values */

/* Non-wakeable sensors */
#define M_ACCEL                 0x000001
#define M_GYRO                  0x000002
#define M_PRESSURE              0x000004
#define M_ECOMPASS              0x000008
#define M_TEMPERATURE           0x000010
#define M_ALS                   0x000020
#define M_STEP_DETECTOR         0x000040
#define M_STEP_COUNTER          0x000080
#define M_LIN_ACCEL             0x000100
#define M_QUAT_6AXIS            0x000200
#define M_GRAVITY               0x000400
#define M_DISP_ROTATE           0x000800
#define M_DISP_BRIGHTNESS       0x001000
#define M_ALGO_IR_GESTURE       0x002000
#define M_ALGO_IR_RAW           0x004000
#define M_UNCALIB_GYRO          0x008000
#define M_UNCALIB_MAG           0x010000
#define M_ACCEL2                0x020000
#define M_QUAT_9AXIS            0x040000
#define M_QUEUE_OVERFLOW        0x080000
#define M_UPDATE_ACCEL_CAL      0x100000

/* wake sensor status */
#define M_DOCK                  0x000001
#define M_PROXIMITY             0x000002
#define M_DISPLAY_TOUCH         0x000004
#define M_COVER                 0x000008
#define M_DISPLAY_PEEK          0x000010
#define M_HEADSET               0x000020
#define M_INIT_COMPLETE         0x000040
#define M_HUB_RESET             0x000080
#define M_FLATUP                0x000100
#define M_FLATDOWN              0x000200
#define M_STOWED                0x000400
#define M_CAMERA_GESTURE        0x000800
#define M_NFC                   0x001000
#define M_SIM                   0x002000
#define M_LIFT                  0x004000
#define M_LOG_MSG               0x008000
#define M_CHOPCHOP              0x010000
#define M_UPDATE_GYRO_CAL       0x020000
#define M_GLANCE                0x040000
/*#define M_UNUSED              0x080000*/
#define M_MOTION_DETECT         0x100000
#define M_STATIONARY_DETECT     0x200000
/*#define M_UNUSED              0x400000*/
/*#define M_UNUSED              0x800000*/

/* algo config mask */
#define M_MMOVEME               0x0001
#define M_NOMMOVE               0x0002
#define M_ALGO_MODALITY         0x0008
#define M_ALGO_ORIENTATION      0x0010
#define M_ALGO_STOWED           0x0020
#define M_ALGO_ACCUM_MODALITY   0x0040
#define M_ALGO_ACCUM_MVMT       0x0080

/* generic interrupt mask */
#define M_GENERIC_INTRPT        0x0080

/* algo index */
#define STML0XX_IDX_MODALITY        0
#define STML0XX_IDX_ORIENTATION     1
#define STML0XX_IDX_STOWED          2
#define STML0XX_IDX_ACCUM_MODALITY  3
#define STML0XX_IDX_ACCUM_MVMT      4

#define STML0XX_NUM_ALGOS           5

#define STML0XX_EVT_SZ_TRANSITION   7
#define STML0XX_EVT_SZ_ACCUM_STATE  2
#define STML0XX_EVT_SZ_ACCUM_MVMT   4

#define SH_HEADSET_EMPTY          0x00
#define SH_HEADPHONE_DETECTED     0x01
#define SH_HEADSET_DETECTED       0x02
#define SH_HEADSET_BUTTON_1_DOWN  0x04
#define SH_HEADSET_BUTTON_2_DOWN  0x08
#define SH_HEADSET_BUTTON_3_DOWN  0x10
#define SH_HEADSET_BUTTON_4_DOWN  0x20

/* Sensor Hub failure error codes */
#define RESET_REASON_UNKNOWN              0
#define RESET_REASON_WATCHDOG             1
#define RESET_REASON_MODALITY_ENGINE      2
#define RESET_REASON_MODALITY_EVENT       3
#define RESET_REASON_MODALITY_ACCUM_EVENT 4
#define RESET_REASON_ALGO_ENGINE          5
#define RESET_REASON_IOEXPANDER           6
#define RESET_REASON_PANIC                7
#define RESET_REASON_EXIT                 8
#define RESET_REASON_HEAP_ACCESS          9
#define RESET_REASON_MAX_CODE             9 /* Must match highest value above */

struct stml0xx_android_sensor_data {
	int64_t timestamp;
	unsigned char type;
	unsigned char data[STML0XX_AS_DATA_BUFF_SIZE];
	int size;
	unsigned char status;
};

struct stml0xx_moto_sensor_data {
	int64_t timestamp;
	unsigned char type;
	unsigned char data[STML0XX_MS_DATA_BUFF_SIZE];
	int size;
};

enum STML0XX_data_types {
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
	DT_SIM,
	DT_RESET,
	DT_GENERIC_INT,
	DT_UNCALIB_GYRO,
	DT_UNCALIB_MAG,
	DT_ACCEL2,
	DT_FLUSH,
	DT_LIFT,
	DT_CHOPCHOP,
	DT_GYRO_CAL,
	DT_STEP_COUNTER,
	DT_STEP_DETECTOR,
	DT_GLANCE,
	DT_ACCEL_CAL,
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

enum stm_commands {
	PASSWORD_RESET,
	MASS_ERASE,
	PROGRAM_CODE,
	END_FIRMWARE,
	PASSWORD_RESET_DEFAULT,
	CRC_CHECK
};

enum reset_option {
	RESET_NOT_ALLOWED,
	RESET_ALLOWED
};

enum lowpower_mode {
	LOWPOWER_DISABLED,
	LOWPOWER_ENABLED
};

enum sh_log_level {
	SH_LOG_DISABLE,
	SH_LOG_ERROR,
	SH_LOG_WARN,
	SH_LOG_DEBUG,
	SH_LOG_VERBOSE
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

#endif /*_UAPI_STML0XX_H */

