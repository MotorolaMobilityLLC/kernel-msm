/*
 * Copyright (C) 2010-2013 Motorola, Inc.
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

#ifndef __STM401_H__
#define __STM401_H__

/** The following define the IOCTL command values via the ioctl macros */
#define STM401_IOCTL_BASE		77
#define STM401_IOCTL_BOOTLOADERMODE	\
		_IOW(STM401_IOCTL_BASE, 0, unsigned char)
#define STM401_IOCTL_NORMALMODE	\
		_IOW(STM401_IOCTL_BASE, 1, unsigned char)
#define STM401_IOCTL_MASSERASE	\
		_IOW(STM401_IOCTL_BASE, 2, unsigned char)
#define STM401_IOCTL_SETSTARTADDR	\
		_IOW(STM401_IOCTL_BASE, 3, unsigned int)
#define STM401_IOCTL_TEST_READ	\
		_IOR(STM401_IOCTL_BASE, 4, unsigned char)
#define STM401_IOCTL_TEST_WRITE	\
		_IOW(STM401_IOCTL_BASE, 5, unsigned char)
#define STM401_IOCTL_TEST_WRITE_READ	\
		_IOWR(STM401_IOCTL_BASE, 6, unsigned short)
#define STM401_IOCTL_SET_MAG_DELAY	\
		_IOW(STM401_IOCTL_BASE, 7, unsigned short)
#define STM401_IOCTL_TEST_BOOTMODE	\
		_IOW(STM401_IOCTL_BASE, 8, unsigned char)
#define STM401_IOCTL_SET_ACC_DELAY	\
		_IOW(STM401_IOCTL_BASE, 9,  unsigned short)
#define STM401_IOCTL_SET_MOTION_DELAY	\
		_IOW(STM401_IOCTL_BASE, 10, unsigned char)
#define STM401_IOCTL_SET_GYRO_DELAY	\
		_IOW(STM401_IOCTL_BASE, 11, unsigned short)
#define STM401_IOCTL_SET_DEBUG	\
		_IOW(STM401_IOCTL_BASE, 12, unsigned char)
#define STM401_IOCTL_SET_USER_PROFILE	\
		_IOW(STM401_IOCTL_BASE, 13, int)
#define STM401_IOCTL_SET_GPS_DATA	\
		_IOW(STM401_IOCTL_BASE, 14, int)
#define STM401_IOCTL_SET_PRES_DELAY	\
		_IOW(STM401_IOCTL_BASE, 15, unsigned short)
#define STM401_IOCTL_SET_REF_ALTITUDE	\
		_IOW(STM401_IOCTL_BASE, 16, int)
#define STM401_IOCTL_SET_ACTIVE_MODE	\
		_IOW(STM401_IOCTL_BASE, 17, unsigned char)
#define STM401_IOCTL_SET_PASSIVE_MODE	\
		_IOW(STM401_IOCTL_BASE, 18, unsigned char)
#define STM401_IOCTL_SET_FACTORY_MODE	\
		_IOW(STM401_IOCTL_BASE, 19, unsigned char)
#define STM401_IOCTL_GET_SENSORS	\
		_IOR(STM401_IOCTL_BASE, 20, unsigned char)
#define STM401_IOCTL_SET_SENSORS	\
		_IOW(STM401_IOCTL_BASE, 21, unsigned char)
#define STM401_IOCTL_GET_VERSION	\
		_IOR(STM401_IOCTL_BASE, 22, unsigned char)
#define STM401_IOCTL_SET_MONITOR_DELAY	\
		_IOW(STM401_IOCTL_BASE, 23, unsigned char)
#define STM401_IOCTL_GET_DOCK_STATUS	\
		_IOR(STM401_IOCTL_BASE, 24, unsigned char)
#define STM401_IOCTL_SET_ORIENTATION_DELAY	\
		_IOW(STM401_IOCTL_BASE, 25, unsigned char)
#define STM401_IOCTL_SET_EQUIPMENT_TYPE	\
		_IOW(STM401_IOCTL_BASE, 26, unsigned char)
#define STM401_IOCTL_SET_POWER_MODE	\
		_IOW(STM401_IOCTL_BASE, 27, unsigned char)
#define STM401_IOCTL_GET_ALGOS	\
		_IOR(STM401_IOCTL_BASE, 28, char*)
#define STM401_IOCTL_SET_ALGOS	\
		_IOW(STM401_IOCTL_BASE, 29, char*)
#define STM401_IOCTL_GET_MAG_CAL \
		_IOR(STM401_IOCTL_BASE, 30, unsigned char*)
#define STM401_IOCTL_SET_MAG_CAL \
		_IOW(STM401_IOCTL_BASE, 31, unsigned char*)
/* 32 unused */
#define STM401_IOCTL_SET_MOTION_DUR	\
		_IOW(STM401_IOCTL_BASE, 33, unsigned int)
/* 34 unused */
#define STM401_IOCTL_SET_ZRMOTION_DUR	\
		_IOW(STM401_IOCTL_BASE, 35, unsigned int)
#define STM401_IOCTL_GET_WAKESENSORS	\
		_IOR(STM401_IOCTL_BASE, 36, unsigned char)
#define STM401_IOCTL_SET_WAKESENSORS	\
		_IOW(STM401_IOCTL_BASE, 37, unsigned char)
#define STM401_IOCTL_GET_VERNAME	\
		_IOW(STM401_IOCTL_BASE, 38, char*)
#define STM401_IOCTL_SET_POSIX_TIME	\
		_IOW(STM401_IOCTL_BASE, 39, unsigned long)
/* 40-42 unused */
#define STM401_IOCTL_SET_ALGO_REQ \
		_IOR(STM401_IOCTL_BASE, 43, char*)
#define STM401_IOCTL_GET_ALGO_EVT \
		_IOR(STM401_IOCTL_BASE, 44, char*)
#define STM401_IOCTL_ENABLE_BREATHING \
		_IOW(STM401_IOCTL_BASE, 45, unsigned char)
#define STM401_IOCTL_WRITE_REG \
		_IOR(STM401_IOCTL_BASE, 46, char*)
#define STM401_IOCTL_READ_REG \
		_IOR(STM401_IOCTL_BASE, 47, char*)
#define STM401_IOCTL_SET_STEP_COUNTER_DELAY	\
		_IOW(STM401_IOCTL_BASE, 48,  unsigned short)
#define STM401_IOCTL_GET_IR_CONFIG \
		_IOWR(STM401_IOCTL_BASE, 49, char*)
#define STM401_IOCTL_SET_IR_CONFIG \
		_IOW(STM401_IOCTL_BASE, 50, char*)
#define STM401_IOCTL_SET_IR_GESTURE_DELAY	\
		_IOW(STM401_IOCTL_BASE, 51,  unsigned short)
#define STM401_IOCTL_SET_IR_RAW_DELAY	\
		_IOW(STM401_IOCTL_BASE, 52,  unsigned short)
#define STM401_IOCTL_GET_BOOTED \
		_IOR(STM401_IOCTL_BASE, 53, unsigned char)
#define STM401_IOCTL_SET_LOWPOWER_MODE \
		_IOW(STM401_IOCTL_BASE, 54, char)
#define STM401_IOCTL_SET_FLUSH \
		_IOW(STM401_IOCTL_BASE, 55, int)

#define FW_VERSION_SIZE 12
#define STM401_CONTROL_REG_SIZE 200
#define STM401_STATUS_REG_SIZE 8
#define STM401_TOUCH_REG_SIZE  8
#define STM401_MAG_CAL_SIZE 26
#define STM_AOD_INSTRUMENTATION_REG_SIZE 256
#define STM401_AS_DATA_BUFF_SIZE 20
#define STM401_MS_DATA_BUFF_SIZE 20

#define STM401_CAMERA_DATA 0x01

/* Mask values */

/* Non wakable sensors */
#define M_ACCEL			0x000001
#define M_GYRO			0x000002
#define M_PRESSURE		0x000004
#define M_ECOMPASS		0x000008
#define M_TEMPERATURE	0x000010
#define M_ALS			0x000020
#define M_STEP_DETECTOR	0x000040
#define M_STEP_COUNTER	0x000080

#define M_LIN_ACCEL		0x000100
#define M_QUATERNION	0x000200
#define M_GRAVITY		0x000400
#define M_DISP_ROTATE		0x000800
#define M_DISP_BRIGHTNESS	0x001000
#define M_IR_GESTURE        0x002000
#define M_IR_RAW            0x004000

#define M_UNCALIB_GYRO		0x008000
#define M_UNCALIB_MAG		0x010000
#define M_IR_OBJECT		0x020000

/* wake sensor status */
#define M_DOCK			0x000001
#define M_PROXIMITY		0x000002
#define M_TOUCH			0x000004
#define M_COVER			0x000008
#define M_QUICKPEEK		0x000010
#define M_HUB_RESET		0x000080

#define M_FLATUP		0x000100
#define M_FLATDOWN		0x000200
#define M_STOWED		0x000400
#define M_CAMERA_ACT		0x000800
#define M_NFC			0x001000
#define M_SIM			0x002000
#define M_CHOPCHOP		0x004000
#define M_LOG_MSG		0x008000

#define M_IR_WAKE_GESTURE	0x200000

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
#define STM401_IDX_MODALITY        0
#define STM401_IDX_ORIENTATION     1
#define STM401_IDX_STOWED          2
#define STM401_IDX_ACCUM_MODALITY  3
#define STM401_IDX_ACCUM_MVMT      4

#define STM401_NUM_ALGOS           5

#define STM401_EVT_SZ_TRANSITION   7
#define STM401_EVT_SZ_ACCUM_STATE  2
#define STM401_EVT_SZ_ACCUM_MVMT   4

struct stm401_android_sensor_data {
	int64_t timestamp;
	unsigned char type;
	unsigned char data[STM401_AS_DATA_BUFF_SIZE];
	int size;
	unsigned char status;
};

struct stm401_moto_sensor_data {
	int64_t timestamp;
	unsigned char type;
	unsigned char data[STM401_MS_DATA_BUFF_SIZE];
	int size;
};

enum STM401_data_types {
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

#ifdef __KERNEL__
#include <linux/fb_quickdraw.h>
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define NAME			     "stm401"

/* STM401 memory map */
#define ID                              0x00
#define REV_ID                          0x01
#define ERROR_STATUS                    0x02
#define LOWPOWER_REG                    0x03

#define STM401_PEEKDATA_REG             0x09
#define STM401_PEEKSTATUS_REG           0x0A
#define STM401_STATUS_REG               0x0B
#define STM401_TOUCH_REG                0x0C
#define STM401_CONTROL_REG              0x0D
#define STM_AOD_INSTRUMENTATION_REG     0x0E

#define AP_POSIX_TIME                   0x10

#define ACCEL_UPDATE_RATE               0x16
#define MAG_UPDATE_RATE                 0x17
#define PRESSURE_UPDATE_RATE            0x18
#define GYRO_UPDATE_RATE                0x19

#define NONWAKESENSOR_CONFIG            0x1A
#define WAKESENSOR_CONFIG               0x1B

#define IR_STATUS                       0x11
#define IR_GESTURE_RATE                 0x12
#define IR_RAW_RATE                     0x13
#define IR_GESTURE                      0x1C
#define IR_RAW                          0x1D
#define IR_CONFIG                       0x1E
#define IR_STATE                        0x1F

#define MOTION_DUR                      0x20
#define ZRMOTION_DUR                    0x22

#define BYPASS_MODE                     0x24
#define SLAVE_ADDRESS                   0x25

#define ALGO_CONFIG                     0x26
#define ALGO_INT_STATUS                 0x27
#define GENERIC_INT_STATUS              0x28

#define MOTION_DATA                     0x2D

#define PROX_SETTINGS                   0x33

#define LUX_TABLE_VALUES                0x34
#define BRIGHTNESS_TABLE_VALUES         0x35
#define STEP_COUNTER_UPDATE_RATE        0x36

#define INTERRUPT_MASK                  0x37
#define WAKESENSOR_STATUS               0x39
#define INTERRUPT_STATUS                0x3A

#define ACCEL_X                         0x3B
#define LIN_ACCEL_X                     0x3C
#define GRAVITY_X                       0x3D
#define STEP_COUNTER			0X3E

#define DOCK_DATA                       0x3F

#define COVER_DATA                      0x40

#define TEMPERATURE_DATA                0x41

#define GYRO_X                          0x43
#define UNCALIB_GYRO_X			0x45
#define UNCALIB_MAG_X			0x46

#define STEP_DETECTOR			0X47

#define MAG_CAL                         0x48
#define MAG_HX                          0x49

#define DISP_ROTATE_DATA                0x4A
#define FLAT_DATA                       0x4B
#define CAMERA                          0x4C
#define NFC                             0x4D
#define SIM                             0x4E
#define CHOPCHOP                        0x4F

#define ALGO_CFG_ACCUM_MODALITY         0x5D
#define ALGO_REQ_ACCUM_MODALITY         0x60
#define ALGO_EVT_ACCUM_MODALITY         0x63

#define CURRENT_PRESSURE                0x66

#define ALS_LUX                         0x6A

#define DISPLAY_BRIGHTNESS              0x6B

#define PROXIMITY                       0x6C

#define STOWED                          0x6D

#define ALGO_CFG_MODALITY               0x6E
#define ALGO_CFG_ORIENTATION            0x6F
#define ALGO_CFG_STOWED                 0x70
#define ALGO_CFG_ACCUM_MVMT             0x71

#define ALGO_REQ_MODALITY               0x72
#define ALGO_REQ_ORIENTATION            0x73
#define ALGO_REQ_STOWED                 0x74
#define ALGO_REQ_ACCUM_MVMT             0x75

#define ALGO_EVT_MODALITY               0x76
#define ALGO_EVT_ORIENTATION            0x77
#define ALGO_EVT_STOWED                 0x78
#define ALGO_EVT_ACCUM_MVMT             0x79

#define RESET                           0x7F
/* STM401 memory map end */

#define READ_CMDBUFF_SIZE 512

#define LIGHTING_TABLE_SIZE 32

#define STM401_AS_DATA_QUEUE_SIZE       0x20
#define STM401_AS_DATA_QUEUE_MASK       0x1F
#define STM401_MS_DATA_QUEUE_SIZE       0x08
#define STM401_MS_DATA_QUEUE_MASK       0x07

#define STM401_CLIENT_MASK		0xF0

#define STM401_BUSY_STATUS_MASK	0x80
#define STM401_BUSY_SLEEP_USEC	10000
#define STM401_BUSY_RESUME_COUNT	14
#define STM401_BUSY_SUSPEND_COUNT	6
#define STM401_LATE_SUSPEND_TIMEOUT	400

#define AOD_WAKEUP_REASON_ESD		4
#define AOD_WAKEUP_REASON_QP_PREPARE		5
#define AOD_WAKEUP_REASON_QP_DRAW		6
#define AOD_WAKEUP_REASON_QP_ERASE		7
#define AOD_WAKEUP_REASON_QP_COMPLETE		8

#define AOD_QP_ACK_BUFFER_ID_MASK	0x3F
#define AOD_QP_ACK_RCVD			0
#define AOD_QP_ACK_DONE			1
#define AOD_QP_ACK_INVALID		2
#define AOD_QP_ACK_ESD_RECOVERED	3

#define AOD_QP_DRAW_MAX_BUFFER_ID	63
#define AOD_QP_DRAW_NO_OVERRIDE		0xFFFF

#define AOD_QP_ENABLED_VOTE_KERN		0x01
#define AOD_QP_ENABLED_VOTE_USER		0x02
#define AOD_QP_ENABLED_VOTE_MASK		0x03

#define STM401_MAX_GENERIC_DATA		512

#define ESR_SIZE			128

#define STM401_RESET_DELAY		50

#define I2C_RESPONSE_LENGTH		8

#define STM401_MAXDATA_LENGTH		256

#define STM401_IR_GESTURE_CNT      8
#define STM401_IR_SZ_GESTURE       4
#define STM401_IR_SZ_RAW           20
#define STM401_IR_CONFIG_REG_SIZE  255

/* stm401_readbuff offsets. */
#define IRQ_WAKE_LO  0
#define IRQ_WAKE_MED 1
#define IRQ_WAKE_HI  2

#define IRQ_NOWAKE_LO   0
#define IRQ_NOWAKE_MED  1
#define IRQ_NOWAKE_HI   2

#define DOCK_STATE	0
#define PROX_DISTANCE	0
#define COVER_STATE	0
#define TOUCH_REASON	1
#define FLAT_UP		0
#define FLAT_DOWN	0
#define STOWED_STATUS	0
#define NFC_VALUE	0
#define ALGO_TYPE	7
#define COMPASS_STATUS	12
#define DISP_VALUE	0
#define ACCEL_RD_X	0
#define ACCEL_RD_Y	2
#define ACCEL_RD_Z	4
#define MAG_X		0
#define MAG_Y		2
#define MAG_Z		4
#define MAG_UNCALIB_X   6
#define MAG_UNCALIB_Y   8
#define MAG_UNCALIB_Z   10
#define ORIENT_X	6
#define ORIENT_Y	8
#define ORIENT_Z	10
#define GYRO_RD_X	0
#define GYRO_RD_Y	2
#define GYRO_RD_Z	4
#define GYRO_UNCALIB_X	6
#define GYRO_UNCALIB_Y	8
#define GYRO_UNCALIB_Z	10
#define ALS_VALUE	0
#define TEMP_VALUE	0
#define PRESSURE_VALUE	0
#define GRAV_X		0
#define GRAV_Y		2
#define GRAV_Z		4
#define CAMERA_VALUE	0
#define IR_GESTURE_EVENT    0
#define IR_GESTURE_ID       1
#define IR_STATE_STATE  0
#define STEP8_DATA	0
#define STEP16_DATA	2
#define STEP32_DATA	4
#define STEP64_DATA	6
#define SIM_DATA	0
#define STEP_DETECT	0
#define CHOPCHOP_DATA   0

/* The following macros are intended to be called with the stm IRQ handlers */
/* only and refer to local variables in those functions. */
#define STM16_TO_HOST(x) ((short) be16_to_cpu(*((u16 *) (stm401_readbuff+(x)))))
#define STM32_TO_HOST(x) ((short) be32_to_cpu(*((u32 *) (stm401_readbuff+(x)))))

struct stm401_quickpeek_message {
	u8 message;
	u8 panel_state;
	u8 buffer_id;
	s16 x1;
	s16 y1;
	s16 x2;
	s16 y2;
	struct list_head list;
};

struct stm401_aod_enabled_vote {
	struct mutex vote_lock;
	unsigned int vote;
	unsigned int resolved_vote;
};

struct stm401_platform_data {
	int (*init)(void);
	void (*exit)(void);
	int (*power_on)(void);
	int (*power_off)(void);
	int gpio_reset;
	int gpio_bslen;
	int gpio_wakeirq;
	int gpio_int;
	int gpio_sh_wake;
	int gpio_sh_wake_resp;
	unsigned int bslen_pin_active_value;
	u16 lux_table[LIGHTING_TABLE_SIZE];
	u8 brightness_table[LIGHTING_TABLE_SIZE];
	char fw_version[FW_VERSION_SIZE];
	int ct406_detect_threshold;
	int ct406_undetect_threshold;
	int ct406_recalibrate_threshold;
	int ct406_pulse_count;
};

struct stm401_data {
	struct i2c_client *client;
	struct stm401_platform_data *pdata;
	/* to avoid two i2c communications at the same time */
	struct mutex lock;
	struct work_struct irq_work;
	struct work_struct irq_wake_work;
	struct work_struct clear_interrupt_status_work;
	struct workqueue_struct *irq_work_queue;
	struct wake_lock wakelock;
	struct wake_lock reset_wakelock;
	struct input_dev *input_dev;

	struct mutex sh_wakeup_lock;
	int sh_wakeup_count;
	int sh_lowpower_enabled;

	int hw_initialized;

	atomic_t enabled;
	int irq;
	int irq_wake;
	unsigned int current_addr;
	enum stm_mode mode;
	unsigned char intp_mask;

	dev_t stm401_dev_num;
	struct class *stm401_class;
	struct cdev as_cdev;
	struct cdev ms_cdev;

	struct switch_dev dsdev; /* Standard Dock switch */
	struct switch_dev edsdev; /* Motorola Dock switch */

	struct stm401_android_sensor_data
		stm401_as_data_buffer[STM401_AS_DATA_QUEUE_SIZE];
	int stm401_as_data_buffer_head;
	int stm401_as_data_buffer_tail;
	wait_queue_head_t stm401_as_data_wq;

	struct stm401_moto_sensor_data
		stm401_ms_data_buffer[STM401_MS_DATA_QUEUE_SIZE];
	int stm401_ms_data_buffer_head;
	int stm401_ms_data_buffer_tail;
	wait_queue_head_t stm401_ms_data_wq;

	struct regulator *regulator_1;
	struct regulator *regulator_2;

	/* Quick peek data */
	struct workqueue_struct *quickpeek_work_queue;
	struct work_struct quickpeek_work;
	struct wake_lock quickpeek_wakelock;
	struct list_head quickpeek_command_list;
	wait_queue_head_t quickpeek_wait_queue;
	atomic_t qp_enabled;
	bool quickpeek_occurred;
	unsigned short qw_irq_status;
	struct stm401_aod_enabled_vote aod_enabled;
	bool qp_in_progress;
	bool qp_prepared;
	struct mutex qp_list_lock;

	bool in_reset_and_init;
	bool is_suspended;
	bool ignore_wakeable_interrupts;
	int ignored_interrupts;
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#endif
	bool pending_wake_work;
};

/* per algo config, request, and event registers */
struct stm401_algo_info_t {
	unsigned short config_bit;
	unsigned char cfg_register;
	unsigned char req_register;
	unsigned char evt_register;
	unsigned short evt_size;
};

#define ALGO_RQST_DATA_SIZE 28
struct stm401_algo_requst_t {
	char size;
	char data[ALGO_RQST_DATA_SIZE];
};

irqreturn_t stm401_isr(int irq, void *dev);
void stm401_irq_work_func(struct work_struct *work);

irqreturn_t stm401_wake_isr(int irq, void *dev);
void stm401_irq_wake_work_func(struct work_struct *work);
int stm401_process_ir_gesture(struct stm401_data *ps_stm401);

long stm401_misc_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg);

void stm401_reset(struct stm401_platform_data *pdata, unsigned char *cmdbuff);
int stm401_reset_and_init(void);

int stm401_as_data_buffer_write(struct stm401_data *ps_stm401,
	unsigned char type, unsigned char *data, int size,
	unsigned char status);
int stm401_as_data_buffer_read(struct stm401_data *ps_stm401,
	struct stm401_android_sensor_data *buff);
int stm401_ms_data_buffer_write(struct stm401_data *ps_stm401,
	unsigned char type, unsigned char *data, int size);
int stm401_ms_data_buffer_read(struct stm401_data *ps_stm401,
	struct stm401_moto_sensor_data *buff);

int stm401_i2c_write_read_no_reset(struct stm401_data *ps_stm401,
	u8 *buf, int writelen, int readlen);
int stm401_i2c_read_no_reset(struct stm401_data *ps_stm401,
	u8 *buf, int len);
int stm401_i2c_write_no_reset(struct stm401_data *ps_stm401,
	u8 *buf, int len);
int stm401_i2c_write_read(struct stm401_data *ps_stm401, u8 *buf,
	int writelen, int readlen);
int stm401_i2c_read(struct stm401_data *ps_stm401, u8 *buf, int len);
int stm401_i2c_write(struct stm401_data *ps_stm401, u8 *buf, int len);
int stm401_enable(struct stm401_data *ps_stm401);

void stm401_wake(struct stm401_data *ps_stm401);
void stm401_sleep(struct stm401_data *ps_stm401);
void stm401_detect_lowpower_mode(unsigned char *cmdbuff);

int stm401_load_brightness_table(struct stm401_data *ps_stm401,
	unsigned char *cmdbuff);

int stm401_irq_wake_work_func_display_locked(struct stm401_data *ps_stm401,
	unsigned short irq_status);
unsigned short stm401_get_interrupt_status(struct stm401_data *ps_stm401,
	unsigned char reg, int *err);
void stm401_quickpeek_work_func(struct work_struct *work);
void stm401_quickpeek_reset_locked(struct stm401_data *ps_stm401);
int stm401_quickpeek_disable_when_idle(struct stm401_data *ps_stm401);
void stm401_vote_aod_enabled_locked(struct stm401_data *ps_stm401, int voter,
	bool enable);
void stm401_store_vote_aod_enabled(struct stm401_data *ps_stm401, int voter,
	bool enable);
void stm401_store_vote_aod_enabled_locked(struct stm401_data *ps_stm401,
	int voter, bool enable);
int stm401_resolve_aod_enabled_locked(struct stm401_data *ps_stm401);
int stm401_display_handle_touch_locked(struct stm401_data *ps_stm401);
int stm401_display_handle_quickpeek_locked(struct stm401_data *ps_stm401,
	bool releaseWakelock);
void stm401_quickwakeup_init(struct stm401_data *ps_stm401);

int stm401_boot_flash_erase(void);
int stm401_get_version(struct stm401_data *ps_stm401);
int switch_stm401_mode(enum stm_mode mode);
int stm401_bootloadermode(struct stm401_data *ps_stm401);

extern struct stm401_data *stm401_misc_data;

extern unsigned short stm401_g_acc_delay;
extern unsigned short stm401_g_mag_delay;
extern unsigned short stm401_g_gyro_delay;
extern unsigned short stm401_g_baro_delay;
extern unsigned short stm401_g_ir_gesture_delay;
extern unsigned short stm401_g_ir_raw_delay;
extern unsigned short stm401_g_step_counter_delay;
extern unsigned long stm401_g_nonwake_sensor_state;
extern unsigned short stm401_g_algo_state;
extern unsigned char stm401_g_motion_dur;
extern unsigned char stm401_g_zmotion_dur;
extern unsigned char stm401_g_control_reg[STM401_CONTROL_REG_SIZE];
extern unsigned char stm401_g_mag_cal[STM401_MAG_CAL_SIZE];
extern unsigned short stm401_g_control_reg_restore;
extern unsigned char stm401_g_ir_config_reg[STM401_IR_CONFIG_REG_SIZE];
extern bool stm401_g_ir_config_reg_restore;
extern bool stm401_g_booted;

extern unsigned char stm401_cmdbuff[];
extern unsigned char stm401_readbuff[];

extern unsigned short stm401_i2c_retry_delay;

extern const struct stm401_algo_info_t stm401_algo_info[] ;

extern struct stm401_algo_requst_t stm401_g_algo_requst[];

extern long stm401_time_delta;

extern unsigned int stm401_irq_disable;

extern unsigned short stm401_g_wake_sensor_state;

extern unsigned char stat_string[];

extern const struct file_operations stm401_as_fops;
extern const struct file_operations stm401_ms_fops;
extern const struct file_operations stm401_misc_fops;
extern struct miscdevice stm401_misc_device;

#endif /* __KERNEL__ */

#endif  /* __STM401_H__ */

