/*
 *  Copyright (C) 2011, Samsung Electronics Co. Ltd. All Rights Reserved.
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
 */

#ifndef __SSP_PRJ_H__
#define __SSP_PRJ_H__

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/uaccess.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/workqueue.h>
#include <linux/gpio.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/wakelock.h>
#include <linux/miscdevice.h>
#include <linux/delay.h>
#include <linux/firmware.h>
#include <linux/timer.h>
#include <linux/list.h>
#include <linux/rtc.h>
#include <linux/regulator/consumer.h>
#include <linux/ssp_platformdata.h>
#ifdef CONFIG_SENSORS_SSP_STM
#include <linux/spi/spi.h>
#endif
#ifdef CONFIG_SENSORS_SSP_SENSORHUB
#include "ssp_sensorhub.h"
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
#undef CONFIG_HAS_EARLYSUSPEND
#endif

#define SSP_DBG		1

#define SUCCESS		1
#define FAIL		0
#define ERROR		-1

#if SSP_DBG
#define SSP_FUNC_DBG 1
#define SSP_DATA_DBG 0

#define ssp_dbg(format, ...) pr_info(format, ##__VA_ARGS__);
#else
#define ssp_dbg(format, ...)
#endif

#if SSP_FUNC_DBG
#define func_dbg() pr_info("[SSP]: %s\n", __func__);
#else
#define func_dbg()
#endif

#if SSP_DATA_DBG
#define data_dbg(format, ...) pr_info(format, ##__VA_ARGS__);
#else
#define data_dbg(format, ...)
#endif

/* ssp mcu device ID */
#define DEVICE_ID		0x55

#define FACTORY_DATA_MAX	99

#define SSP_SW_RESET_TIME	3000
#define DEFUALT_POLLING_DELAY	(200 * NSEC_PER_MSEC)
#define DATA_PACKET_SIZE	960

/* SSP Binary Type */
enum {
	KERNEL_BINARY = 0,
	KERNEL_CRASHED_BINARY,
	UMS_BINARY,
};

/*
 * SENSOR_DELAY_SET_STATE
 * Check delay set to avoid sending ADD instruction twice
 */
enum {
	INITIALIZATION_STATE = 0,
	NO_SENSOR_STATE,
	ADD_SENSOR_STATE,
	RUNNING_SENSOR_STATE,
};

/* Firmware download STATE */
enum {
	FW_DL_STATE_FAIL = -1,
	FW_DL_STATE_NONE = 0,
	FW_DL_STATE_NEED_TO_SCHEDULE,
	FW_DL_STATE_SCHEDULED,
	FW_DL_STATE_DOWNLOADING,
	FW_DL_STATE_SYNC,
	FW_DL_STATE_DONE,
};

enum {
	SENSORS_BATCH_DRY_RUN               = 0x00000001,
	SENSORS_BATCH_WAKE_UPON_FIFO_FULL   = 0x00000002
};

enum {
	META_DATA_FLUSH_COMPLETE = 1,
};

#define SSP_INVALID_REVISION		99999
#define SSP_INVALID_REVISION2		0xFFFFFF

/* Gyroscope DPS */
#define GYROSCOPE_DPS250		250
#define GYROSCOPE_DPS500		500
#define GYROSCOPE_DPS2000		2000

/* Gesture Sensor Current */
#define DEFUALT_IR_CURRENT		100

/* kernel -> ssp manager cmd*/
#define SSP_LIBRARY_SLEEP_CMD		(1 << 5)
#define SSP_LIBRARY_LARGE_DATA_CMD	(1 << 6)
#define SSP_LIBRARY_WAKEUP_CMD		(1 << 7)

/* AP -> SSP Instruction */
#define MSG2SSP_INST_BYPASS_SENSOR_ADD		0xA1
#define MSG2SSP_INST_BYPASS_SENSOR_REMOVE	0xA2
#define MSG2SSP_INST_REMOVE_ALL			0xA3
#define MSG2SSP_INST_CHANGE_DELAY		0xA4
#define MSG2SSP_INST_LIBRARY_ADD		0xB1
#define MSG2SSP_INST_LIBRARY_REMOVE		0xB2
#define MSG2SSP_INST_LIB_NOTI			0xB4
#define MSG2SSP_INST_LIB_DATA			0xC1

#define MSG2SSP_AP_MCU_SET_GYRO_CAL		0xCD
#define MSG2SSP_AP_MCU_SET_ACCEL_CAL		0xCE
#define MSG2SSP_AP_STATUS_SHUTDOWN		0xD0
#define MSG2SSP_AP_STATUS_WAKEUP		0xD1
#define MSG2SSP_AP_STATUS_SLEEP			0xD2
#define MSG2SSP_AP_STATUS_RESUME		0xD3
#define MSG2SSP_AP_STATUS_SUSPEND		0xD4
#define MSG2SSP_AP_STATUS_RESET			0xD5
#define MSG2SSP_AP_STATUS_POW_CONNECTED	0xD6
#define MSG2SSP_AP_STATUS_POW_DISCONNECTED	0xD7
#define MSG2SSP_AP_TEMPHUMIDITY_CAL_DONE	0xDA
#define MSG2SSP_AP_MCU_SET_DUMPMODE		0xDB
#define MSG2SSP_AP_MCU_DUMP_CHECK		0xDC
#define MSG2SSP_AP_MCU_BATCH_FLUSH		0xDD
#define MSG2SSP_AP_MCU_BATCH_COUNT		0xDF

#define MSG2SSP_AP_WHOAMI			0x0F
#define MSG2SSP_AP_FIRMWARE_REV			0xF0
#define MSG2SSP_AP_SENSOR_FORMATION		0xF1
#define MSG2SSP_AP_SENSOR_PROXTHRESHOLD		0xF2
#define MSG2SSP_AP_SENSOR_BARCODE_EMUL		0xF3
#define MSG2SSP_AP_SENSOR_SCANNING		0xF4
#define MSG2SSP_AP_SET_MAGNETIC_HWOFFSET	0xF5
#define MSG2SSP_AP_GET_MAGNETIC_HWOFFSET	0xF6
#define MSG2SSP_AP_SENSOR_GESTURE_CURRENT	0xF7
#define MSG2SSP_AP_GET_THERM			0xF8
#define MSG2SSP_AP_GET_BIG_DATA			0xF9
#define MSG2SSP_AP_SET_BIG_DATA			0xFA
#define MSG2SSP_AP_START_BIG_DATA		0xFB
#define MSG2SSP_AP_SET_MAGNETIC_STATIC_MATRIX	0xFD
#define MSG2SSP_AP_SENSOR_TILT			0xEA
#define MSG2SSP_AP_MCU_SET_TIME			0xFE
#define MSG2SSP_AP_MCU_GET_TIME			0xFF
#define MSG2SSP_AP_MOBEAM_DATA_SET		0x31
#define MSG2SSP_AP_MOBEAM_REGISTER_SET		0x32
#define MSG2SSP_AP_MOBEAM_COUNT_SET		0x33
#define MSG2SSP_AP_MOBEAM_START			0x34
#define MSG2SSP_AP_MOBEAM_STOP			0x35

#define MSG2SSP_AP_FUSEROM			0X01

/* voice data */
#define TYPE_WAKE_UP_VOICE_SERVICE		0x01
#define TYPE_WAKE_UP_VOICE_SOUND_SOURCE_AM	0x01
#define TYPE_WAKE_UP_VOICE_SOUND_SOURCE_GRAMMER	0x02

/* Factory Test */
#define ACCELEROMETER_FACTORY		0x80
#define GYROSCOPE_FACTORY		0x81
#define GEOMAGNETIC_FACTORY		0x82
#define PRESSURE_FACTORY		0x85
#define GESTURE_FACTORY			0x86
#define TEMPHUMIDITY_CRC_FACTORY	0x88
#define GYROSCOPE_TEMP_FACTORY		0x8A
#define GYROSCOPE_DPS_FACTORY		0x8B
#define MCU_FACTORY			0x8C
#define MCU_SLEEP_FACTORY		0x8D

/* Factory data length */
#define ACCEL_FACTORY_DATA_LENGTH	1
#define GYRO_FACTORY_DATA_LENGTH	36
#define MAGNETIC_FACTORY_DATA_LENGTH	26
#define PRESSURE_FACTORY_DATA_LENGTH	1
#define MCU_FACTORY_DATA_LENGTH		5
#define GYRO_TEMP_FACTORY_DATA_LENGTH	2
#define GYRO_DPS_FACTORY_DATA_LENGTH	1
#define TEMPHUMIDITY_FACTORY_DATA_LENGTH	1
#define MCU_SLEEP_FACTORY_DATA_LENGTH	FACTORY_DATA_MAX
#define GESTURE_FACTORY_DATA_LENGTH		4

/* SSP -> AP ACK about write CMD */
#define MSG_ACK		0x80	/* ACK from SSP to AP */
#define MSG_NAK		0x70	/* NAK from SSP to AP */

/* Accelerometer sensor*/
#define MAX_ACCEL_1G	16384
#define MAX_ACCEL_2G	32767
#define MIN_ACCEL_2G	-32768
#define MAX_ACCEL_4G	65536

#define MAX_GYRO	32767
#define MIN_GYRO	-32768

#define MAX_COMP_BUFF	60

/* SSP_INSTRUCTION_CMD */
enum {
	REMOVE_SENSOR = 0,
	ADD_SENSOR,
	CHANGE_DELAY,
	GO_SLEEP,
	REMOVE_LIBRARY,
	ADD_LIBRARY,
};

/* SENSOR_TYPE */
enum {
	ACCELEROMETER_SENSOR = 0,
	GYROSCOPE_SENSOR,
	GEOMAGNETIC_UNCALIB_SENSOR,
	GEOMAGNETIC_RAW,
	GEOMAGNETIC_SENSOR,
	PRESSURE_SENSOR,
	GESTURE_SENSOR,
	PROXIMITY_SENSOR,
	TEMPERATURE_HUMIDITY_SENSOR,
	LIGHT_SENSOR,
	PROXIMITY_RAW,
	ORIENTATION_SENSOR,
	STEP_DETECTOR = 12,
	SIG_MOTION_SENSOR,
	GYRO_UNCALIB_SENSOR,
	GAME_ROTATION_VECTOR = 15,
	ROTATION_VECTOR,
	STEP_COUNTER,
	BIO_HRM_RAW,
	BIO_HRM_RAW_FAC,
	BIO_HRM_LIB = 20,
	TILT_TO_WAKE,
	SENSOR_MAX,
};

struct meta_data_event {
	s32 what;
	s32 sensor;
} __attribute__((__packed__));

struct sensor_value {
	union {
		struct {
			s16 x;
			s16 y;
			s16 z;
		};
		struct {		/*calibrated mag, gyro*/
			s16 cal_x;
			s16 cal_y;
			s16 cal_z;
			u8 accuracy;
		};
		struct {		/*uncalibrated mag, gyro*/
			s16 uncal_x;
			s16 uncal_y;
			s16 uncal_z;
			s16 offset_x;
			s16 offset_y;
			s16 offset_z;
		};
		struct {		/* rotation vector */
			s32 quat_a;
			s32 quat_b;
			s32 quat_c;
			s32 quat_d;
			s16 acc_x;
			s16 acc_y;
			s16 acc_z;
			u8 acc_rot;
		};
		struct {		/* game rotation vector */
			s32 quat_grv_a;
			s32 quat_grv_b;
			s32 quat_grv_c;
			s32 quat_grv_d;
			u8 acc_grv_rot;
		};
#ifdef CONFIG_SENSORS_SSP_ADPD142
		struct {
			s16 hr;
			s16 rri;
			s32 snr;
		};
#endif
		u8 step_det;
		u8 sig_motion;
		u32 step_diff;
		struct meta_data_event meta_data;
	};
	u64 timestamp;
} __attribute__((__packed__));

extern struct class *sensors_event_class;
extern int androidboot_mode_charger;

struct calibraion_data {
	s16 x;
	s16 y;
	s16 z;
};

/* ssp_msg options bit*/
#define SSP_SPI		0	/* read write mask */
#define SSP_RETURN	2	/* write and read option */
#define SSP_GYRO_DPS	3	/* gyro dps mask */
#define SSP_INDEX	3	/* data index mask */

#define SSP_SPI_MASK		(3 << SSP_SPI)
#define SSP_GYRO_DPS_MASK	(3 << SSP_GYRO_DPS)
#define SSP_INDEX_MASK		(8191 << SSP_INDEX)

struct ssp_msg {
	u8 cmd;
	u16 length;
	u16 options;
	u32 data;

	struct list_head list;
	struct completion *done;
	char *buffer;
	u8 free_buffer;
	bool *dead_hook;
	bool dead;
} __attribute__((__packed__));

enum {
	AP2HUB_READ = 0,
	AP2HUB_WRITE,
	HUB2AP_WRITE,
	AP2HUB_READY,
	AP2HUB_RETURN
};

enum {
	BIG_TYPE_TEMP = 0,
	BIG_TYPE_MAX,
};

struct ssp_data {
	struct iio_dev *accel_indio_dev;
	struct iio_dev *gyro_indio_dev;
	struct iio_dev *rot_indio_dev;
	struct iio_dev *game_rot_indio_dev;
	struct iio_dev *step_det_indio_dev;
	struct iio_trigger *accel_trig;
	struct iio_trigger *gyro_trig;
	struct iio_trigger *rot_trig;
	struct iio_trigger *game_rot_trig;
	struct iio_trigger *step_det_trig;

	struct input_dev *mag_input_dev;
	struct input_dev *uncal_mag_input_dev;
	struct input_dev *sig_motion_input_dev;
	struct input_dev *uncalib_gyro_input_dev;
	struct input_dev *step_cnt_input_dev;
	struct input_dev *meta_input_dev;
#ifdef CONFIG_SENSORS_SSP_ADPD142
	struct input_dev *hrm_lib_input_dev;
#endif
	struct input_dev *tilt_wake_input_dev;

#ifdef CONFIG_SENSORS_SSP_STM
	struct spi_device *spi;
#endif
	struct i2c_client *client;
	struct wake_lock ssp_wake_lock;
	struct timer_list debug_timer;
	struct workqueue_struct *debug_wq;
	struct work_struct work_debug;
	struct calibraion_data accelcal;
	struct calibraion_data gyrocal;
	struct sensor_value buf[SENSOR_MAX];
	struct device *sen_dev;
	struct device *mcu_device;
	struct device *acc_device;
	struct device *gyro_device;
	struct device *mag_device;
#ifdef CONFIG_SENSORS_SSP_ADPD142
	struct device *hrm_device;
#endif

	struct delayed_work work_firmware;
	struct delayed_work work_refresh;
	struct miscdevice batch_io_device;

	bool bSspShutdown;
	bool bAccelAlert;
	bool bGeomagneticRawEnabled;
	bool bBinaryChashed;
	bool bProbeIsDone;
	bool bDumping;
	bool bTimeSyncing;
	bool bHandlingIrq;
	bool bDebugmsg;

	unsigned int uIr_Current;
	unsigned char uFuseRomData[3];
	unsigned char uMagCntlRegData;
	char *pchLibraryBuf;
	char chLcdLdi[2];
	int iIrq;
	int iLibraryLength;
	int aiCheckStatus[SENSOR_MAX];

	unsigned int uComFailCnt;
	unsigned int uResetCnt;
	unsigned int uTimeOutCnt;
	unsigned int uIrqCnt;
	unsigned int uDumpCnt;

	unsigned int uGyroDps;
	unsigned int uSensorState;
	unsigned int uCurFirmRev;
	char uLastResumeState;
	char uLastAPState;
	u64 step_count_total;

	atomic_t aSensorEnable;
	int64_t adDelayBuf[SENSOR_MAX];
	s32 batchLatencyBuf[SENSOR_MAX];
	s8 batchOptBuf[SENSOR_MAX];

	void (*get_sensor_data[SENSOR_MAX])(char *, int *,
		struct sensor_value *);
	void (*report_sensor_data[SENSOR_MAX])(struct ssp_data *,
		struct sensor_value *);

#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspend;
#endif

#ifdef CONFIG_SENSORS_SSP_SENSORHUB
	struct ssp_sensorhub_data *hub_data;
#endif
	int ap_rev;
	int accel_position;
	int mag_position;
	int fw_dl_state;
	u8 mag_matrix_size;
	u8 *mag_matrix;

#ifdef CONFIG_SENSORS_SSP_STM
	struct mutex comm_mutex;
	struct mutex pending_mutex;
#endif

	int mcu_int1;
	int mcu_int2;
	int ap_int;
	int rst;

	struct list_head pending_list;
	void (*ssp_big_task[BIG_TYPE_MAX])(struct work_struct *);
	u64 timestamp;
	struct regulator *vdd_hub;
	struct regulator *vdd_acc;
	struct regulator *vdd_hrm;
};

struct ssp_big {
	struct ssp_data *data;
	struct work_struct work;
	u32 length;
	u32 addr;
};

int ssp_iio_configure_ring(struct iio_dev *);
void ssp_iio_unconfigure_ring(struct iio_dev *);
int ssp_iio_probe_trigger(struct ssp_data *,
		struct iio_dev *, struct iio_trigger *);
void ssp_iio_remove_trigger(struct iio_trigger *);

void ssp_enable(struct ssp_data *, bool);
int ssp_spi_async(struct ssp_data *, struct ssp_msg *);
int ssp_spi_sync(struct ssp_data *, struct ssp_msg *, int);
void clean_pending_list(struct ssp_data *);
void toggle_mcu_reset(struct ssp_data *);
int initialize_mcu(struct ssp_data *);
int initialize_input_dev(struct ssp_data *);
int initialize_sysfs(struct ssp_data *);
void initialize_function_pointer(struct ssp_data *);

void sensors_remove_symlink(struct input_dev *);
void destroy_sensor_class(void);
int initialize_event_symlink(struct ssp_data *);
int sensors_create_symlink(struct input_dev *);
int accel_open_calibration(struct ssp_data *);
int gyro_open_calibration(struct ssp_data *);
int check_fwbl(struct ssp_data *);
void remove_input_dev(struct ssp_data *);
void remove_sysfs(struct ssp_data *);
void remove_event_symlink(struct ssp_data *);
int ssp_send_cmd(struct ssp_data *, char, int);
int send_instruction(struct ssp_data *, u8, u8, u8 *, u8);
int send_instruction_sync(struct ssp_data *, u8, u8, u8 *, u8);
int flush(struct ssp_data *, u8);
int get_batch_count(struct ssp_data *, u8);
int select_irq_msg(struct ssp_data *);
int get_chipid(struct ssp_data *);
int get_fuserom_data(struct ssp_data *);
int set_big_data_start(struct ssp_data *, u8 , u32);
int set_gyro_cal(struct ssp_data *);
int set_accel_cal(struct ssp_data *);

int set_sensor_position(struct ssp_data *);
int set_magnetic_static_matrix(struct ssp_data *);
void sync_sensor_state(struct ssp_data *);
int get_msdelay(int64_t);
unsigned int get_sensor_scanning_info(struct ssp_data *);
unsigned int get_firmware_rev(struct ssp_data *);
int forced_to_download_binary(struct ssp_data *, int);
int parse_dataframe(struct ssp_data *, char *, int);
int print_mcu_debug(struct ssp_data *, char *, int *, int);
void enable_debug_timer(struct ssp_data *);
void disable_debug_timer(struct ssp_data *);
int initialize_debug_timer(struct ssp_data *);
void report_meta_data(struct ssp_data *, struct sensor_value *);
void report_acc_data(struct ssp_data *, struct sensor_value *);
void report_gyro_data(struct ssp_data *, struct sensor_value *);
void report_mag_data(struct ssp_data *, struct sensor_value *);
void report_mag_uncaldata(struct ssp_data *, struct sensor_value *);
void report_rot_data(struct ssp_data *, struct sensor_value *);
void report_game_rot_data(struct ssp_data *, struct sensor_value *);
void report_step_det_data(struct ssp_data *, struct sensor_value *);
void report_geomagnetic_raw_data(struct ssp_data *, struct sensor_value *);
void report_sig_motion_data(struct ssp_data *, struct sensor_value *);
void report_uncalib_gyro_data(struct ssp_data *, struct sensor_value *);
void report_step_cnt_data(struct ssp_data *, struct sensor_value *);
#ifdef CONFIG_SENSORS_SSP_ADPD142
void report_hrm_lib_data(struct ssp_data *, struct sensor_value *);
#endif
void report_tilt_wake_data(struct ssp_data *, struct sensor_value *);

unsigned int get_module_rev(struct ssp_data *data);
void reset_mcu(struct ssp_data *);
int queue_refresh_task(struct ssp_data *data, int delay);
int sensors_register(struct device *, void *,
	struct device_attribute*[], char *);
void sensors_unregister(struct device *,
	struct device_attribute*[]);
ssize_t mcu_reset_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_dump_show(struct device *, struct device_attribute *, char *);
ssize_t mcu_update_ums_bin_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_update_kernel_bin_show(struct device *,
	struct device_attribute *, char *);
ssize_t mcu_update_kernel_crashed_bin_show(struct device *,
	struct device_attribute *, char *);

#ifdef CONFIG_SENSORS_SSP_STM
void ssp_dump_task(struct work_struct *work);
void ssp_read_big_library_task(struct work_struct *work);
void ssp_send_big_library_task(struct work_struct *work);
void ssp_pcm_dump_task(struct work_struct *work);
#endif
int set_time(struct ssp_data *);
int get_time(struct ssp_data *);
#endif
