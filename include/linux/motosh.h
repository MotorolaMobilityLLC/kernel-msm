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
 */

#ifndef __MOTOSH_H__
#define __MOTOSH_H__

/* The user-space-visible definitions. */
#include <uapi/linux/motosh.h>

#include <linux/spinlock.h>

#ifdef CONFIG_MMI_HALL_NOTIFICATIONS
#include <mach/mmi_hall_notifier.h>
#endif

#include <linux/fb_quickdraw.h>
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define NAME			     "motosh"

/* MOTOSH memory map */
#define ID                              0x00
#define REV_ID                          0x01
#define ERROR_STATUS                    0x02
#define LOWPOWER_REG                    0x03
#define MOTOSH_ELAPSED_RT               0x06

#define MOTOSH_PEEKDATA_REG             0x09
#define MOTOSH_PEEKSTATUS_REG           0x0A
#define MOTOSH_STATUS_REG               0x0B
#define MOTOSH_TOUCH_REG                0x0C
#define MOTOSH_CONTROL_REG              0x0D
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
#define QUAT_6AXIS_UPDATE_RATE          0x21
#define ZRMOTION_DUR                    0x22
#define QUAT_9AXIS_UPDATE_RATE          0x23

#define BYPASS_MODE                     0x24
#define SLAVE_ADDRESS                   0x25

#define ALGO_CONFIG                     0x26
#define ALGO_INT_STATUS                 0x27
#define GENERIC_INT_STATUS              0x28

#define SENSOR_ORIENTATIONS             0x2A

#define FW_VERSION_LEN_REG              0x2B
#define FW_VERSION_STR_REG              0x2C

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
#define QUATERNION_DATA                 0x44
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
#define LIFT                            0x51

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
/* MOTOSH memory map end */

#define READ_CMDBUFF_SIZE 512

#define LIGHTING_TABLE_SIZE 32

#define MOTOSH_MS_DATA_QUEUE_SIZE       0x08
#define MOTOSH_MS_DATA_QUEUE_MASK       0x07

#define MOTOSH_CLIENT_MASK		0xF0

#define MOTOSH_BUSY_STATUS_MASK	0x80
#define MOTOSH_BUSY_SLEEP_USEC	10000
#define MOTOSH_BUSY_RESUME_COUNT	14
#define MOTOSH_BUSY_SUSPEND_COUNT	6
#define MOTOSH_LATE_SUSPEND_TIMEOUT	400

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

#define MOTOSH_MAX_GENERIC_DATA		512

#define ESR_SIZE			128

#define I2C_RESPONSE_LENGTH		8

#define MOTOSH_MAXDATA_LENGTH		256

#define MOTOSH_IR_GESTURE_CNT      8
#define MOTOSH_IR_SZ_GESTURE       4
#define MOTOSH_IR_SZ_RAW           20
#define MOTOSH_IR_CONFIG_REG_SIZE  255

/* motosh_readbuff offsets. */
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
#define QUAT_6AXIS_A	0
#define QUAT_6AXIS_B	2
#define QUAT_6AXIS_C	4
#define QUAT_6AXIS_W	6
#define QUAT_9AXIS_A	8
#define QUAT_9AXIS_B	10
#define QUAT_9AXIS_C	12
#define QUAT_9AXIS_W	14
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
#define LIFT_DISTANCE	0
#define LIFT_ROTATION	4
#define LIFT_GRAV_DIFF	8

/* The following macros are intended to be called with the stm IRQ handlers */
/* only and refer to local variables in those functions. */
#define STM16_TO_HOST(x) ((short) be16_to_cpu(*((u16 *) (motosh_readbuff+(x)))))
#define STM32_TO_HOST(x) ((short) be32_to_cpu(*((u32 *) (motosh_readbuff+(x)))))

#define MOTOSH_HALL_SOUTH 1
#define MOTOSH_HALL_NORTH 2

struct motosh_quickpeek_message {
	u8 message;
	u8 panel_state;
	u8 buffer_id;
	u16 x1;
	u16 y1;
	u16 x2;
	u16 y2;
	struct list_head list;
};

struct motosh_aod_enabled_vote {
	struct mutex vote_lock;
	unsigned int vote;
	unsigned int resolved_vote;
};

struct motosh_platform_data {
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
	char fw_version_str[FW_VERSION_STR_MAX_LEN];
	int ct406_detect_threshold;
	int ct406_undetect_threshold;
	int ct406_recalibrate_threshold;
	int ct406_pulse_count;
	int ct406_prox_gain;
	int accel_orient;
	int gyro_orient;
	int mag_orient;
};

/**
 * as_node - Android sensor event node type
 * @list: underlying linux list_head structure
 * @data: data from the sensor event
 *
 * The as event queue is a list of these nodes
 */
struct as_node {
	struct list_head list;
	struct motosh_android_sensor_data data;
};

struct motosh_data {
	struct i2c_client *client;
	struct motosh_platform_data *pdata;
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

	dev_t motosh_dev_num;
	struct class *motosh_class;
	struct device *motosh_class_dev;
	struct cdev as_cdev;
	struct cdev ms_cdev;

	struct switch_dev dsdev; /* Standard Dock switch */
	struct switch_dev edsdev; /* Motorola Dock switch */

	/* Android sensor event queue */
	struct as_node as_queue;
	/* Lock for modifying as_queue */
	spinlock_t as_queue_lock;

	wait_queue_head_t motosh_as_data_wq;

	struct motosh_moto_sensor_data
		motosh_ms_data_buffer[MOTOSH_MS_DATA_QUEUE_SIZE];
	int motosh_ms_data_buffer_head;
	int motosh_ms_data_buffer_tail;
	wait_queue_head_t motosh_ms_data_wq;

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
	struct motosh_aod_enabled_vote aod_enabled;
	bool ignore_wakeable_interrupts;
	int ignored_interrupts;
	bool qp_in_progress;
	bool qp_prepared;
	struct mutex qp_list_lock;

	bool in_reset_and_init;
	bool is_suspended;
	bool pending_wake_work;
#if defined(CONFIG_FB)
	struct notifier_block fb_notif;
#endif
	struct mmi_hall_data *hall_data;
};

/* per algo config, request, and event registers */
struct motosh_algo_info_t {
	unsigned short config_bit;
	unsigned char cfg_register;
	unsigned char req_register;
	unsigned char evt_register;
	unsigned short evt_size;
};

#define ALGO_RQST_DATA_SIZE 28
struct motosh_algo_requst_t {
	char size;
	char data[ALGO_RQST_DATA_SIZE];
};

int64_t motosh_timestamp_ns(void);
int motosh_set_rv_6axis_update_rate(
	struct motosh_data *ps_motosh,
	const uint8_t newDelay);
int motosh_set_rv_9axis_update_rate(
	struct motosh_data *ps_motosh,
	const uint8_t newDelay);

irqreturn_t motosh_isr(int irq, void *dev);
void motosh_irq_work_func(struct work_struct *work);

irqreturn_t motosh_wake_isr(int irq, void *dev);
void motosh_irq_wake_work_func(struct work_struct *work);
int motosh_process_ir_gesture(struct motosh_data *ps_motosh);

long motosh_misc_ioctl(struct file *file, unsigned int cmd,
	unsigned long arg);

void motosh_reset(struct motosh_platform_data *pdata, unsigned char *cmdbuff);
int motosh_reset_and_init(enum reset_mode mode);

int motosh_as_data_buffer_write(struct motosh_data *ps_motosh,
	unsigned char type, unsigned char *data, int size,
	unsigned char status);
int motosh_as_data_buffer_read(struct motosh_data *ps_motosh,
	struct motosh_android_sensor_data *buff);
int motosh_ms_data_buffer_write(struct motosh_data *ps_motosh,
	unsigned char type, unsigned char *data, int size);
int motosh_ms_data_buffer_read(struct motosh_data *ps_motosh,
	struct motosh_moto_sensor_data *buff);

int motosh_i2c_write_read_no_reset(struct motosh_data *ps_motosh,
	u8 *buf, int writelen, int readlen);
int motosh_i2c_read_no_reset(struct motosh_data *ps_motosh,
	u8 *buf, int len);
int motosh_i2c_write_no_reset(struct motosh_data *ps_motosh,
	u8 *buf, int len);
int motosh_i2c_write_read(struct motosh_data *ps_motosh, u8 *buf,
	int writelen, int readlen);
int motosh_i2c_read(struct motosh_data *ps_motosh, u8 *buf, int len);
int motosh_i2c_write(struct motosh_data *ps_motosh, u8 *buf, int len);
int motosh_enable(struct motosh_data *ps_motosh);

void motosh_wake(struct motosh_data *ps_motosh);
void motosh_sleep(struct motosh_data *ps_motosh);
void motosh_detect_lowpower_mode(unsigned char *cmdbuff);

int motosh_load_brightness_table(struct motosh_data *ps_motosh,
	unsigned char *cmdbuff);

int motosh_irq_wake_work_func_display_locked(struct motosh_data *ps_motosh,
	unsigned short irq_status);
unsigned short motosh_get_interrupt_status(struct motosh_data *ps_motosh,
	unsigned char reg, int *err);
void motosh_quickpeek_work_func(struct work_struct *work);
void motosh_quickpeek_reset_locked(struct motosh_data *ps_motosh);
int motosh_quickpeek_disable_when_idle(struct motosh_data *ps_motosh);
void motosh_vote_aod_enabled_locked(struct motosh_data *ps_motosh, int voter,
	bool enable);
void motosh_store_vote_aod_enabled(struct motosh_data *ps_motosh, int voter,
	bool enable);
void motosh_store_vote_aod_enabled_locked(struct motosh_data *ps_motosh,
	int voter, bool enable);
int motosh_resolve_aod_enabled_locked(struct motosh_data *ps_motosh);
int motosh_display_handle_touch_locked(struct motosh_data *ps_motosh);
int motosh_display_handle_quickpeek_locked(struct motosh_data *ps_motosh,
	bool releaseWakelock);
void motosh_quickwakeup_init(struct motosh_data *ps_motosh);

int motosh_boot_flash_erase(void);
int motosh_get_version(struct motosh_data *ps_motosh);
int motosh_get_version_str(struct motosh_data *ps_motosh);
int switch_motosh_mode(enum stm_mode mode);
int motosh_bootloadermode(struct motosh_data *ps_motosh);

void motosh_time_sync(void);
int64_t motosh_time_recover(int32_t hubshort, int64_t cur_time);
void motosh_time_compare(void);

extern struct motosh_data *motosh_misc_data;

extern unsigned short motosh_g_acc_delay;
extern unsigned short motosh_g_mag_delay;
extern unsigned short motosh_g_gyro_delay;
extern uint8_t motosh_g_rv_6axis_delay;
extern uint8_t motosh_g_rv_9axis_delay;
extern unsigned short motosh_g_baro_delay;
extern unsigned short motosh_g_ir_gesture_delay;
extern unsigned short motosh_g_ir_raw_delay;
extern unsigned short motosh_g_step_counter_delay;
extern unsigned long motosh_g_nonwake_sensor_state;
extern unsigned short motosh_g_algo_state;
extern unsigned char motosh_g_motion_dur;
extern unsigned char motosh_g_zmotion_dur;
extern unsigned char motosh_g_control_reg[MOTOSH_CONTROL_REG_SIZE];
extern unsigned char motosh_g_mag_cal[MOTOSH_MAG_CAL_SIZE];
extern unsigned short motosh_g_control_reg_restore;
extern unsigned char motosh_g_ir_config_reg[MOTOSH_IR_CONFIG_REG_SIZE];
extern bool motosh_g_ir_config_reg_restore;
extern bool motosh_g_booted;

extern unsigned char motosh_cmdbuff[];
extern unsigned char motosh_readbuff[];

extern unsigned short motosh_i2c_retry_delay;

extern const struct motosh_algo_info_t motosh_algo_info[];

extern struct motosh_algo_requst_t motosh_g_algo_requst[];

extern long motosh_time_delta;

extern unsigned int motosh_irq_disable;

extern unsigned short motosh_g_wake_sensor_state;

extern unsigned char stat_string[];

extern const struct file_operations motosh_as_fops;
extern const struct file_operations motosh_ms_fops;
extern const struct file_operations motosh_misc_fops;
extern struct miscdevice motosh_misc_device;

#endif  /* __MOTOSH_H__ */
