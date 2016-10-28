/*
 * Copyright (C) 2010-2015 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#ifndef __STML0XX_H__
#define __STML0XX_H__

/*The user-space-visible defintions */
#include <uapi/linux/stml0xx.h>

#include <linux/cdev.h>
#include <linux/irq.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/spinlock.h>
#include <linux/switch.h>
#include <linux/wakelock.h>

#ifdef CONFIG_MMI_HALL_NOTIFICATIONS
#include <linux/mmi_hall_notifier.h>
#endif

#ifdef CONFIG_SENSORS_SH_AK09912
#include "linux/stml0xx_akm.h"
#endif

/* Log macros */
#define ENABLE_VERBOSE_LOGGING 0

/* SPI */
#define SPI_FLASH_CLK_SPD_HZ     960000
#define SPI_NORMAL_CLK_SPD_HZ   4800000
#define SPI_BUFF_SIZE           1152
#define SPI_RETRIES             5
#define SPI_RETRY_DELAY         20
#define SPI_SENSORHUB_TIMEOUT   5000
#define SPI_BARKER_1            0xF9
#define SPI_BARKER_2            0xAE

enum sh_spi_msg {
	SPI_MSG_TYPE_READ_REG = 1,
	SPI_MSG_TYPE_WRITE_REG,
	SPI_MSG_TYPE_READ_IRQ_DATA,
	SPI_MSG_TYPE_READ_WAKE_IRQ_DATA
};

#include <linux/spi/spi.h>

/* STML0XX memory map */
#define GYRO_CALIBRATION
#define VMM_ENTRY(reg, id, writable, addr, size) id,
#define DSP
enum vmm_ids {
#include <linux/stml0xx_vmm.h>
};
#undef VMM_ENTRY
#undef GYRO_CALIBRATION
/* STML0XX memory map end */

#define LIGHTING_TABLE_SIZE 32

#define STML0XX_MS_DATA_QUEUE_SIZE       0x08
#define STML0XX_MS_DATA_QUEUE_MASK       0x07

#define STML0XX_CLIENT_MASK              0xF0

#define STML0XX_BUSY_STATUS_MASK         0x80
#define STML0XX_BUSY_SLEEP_USEC          10000
#define STML0XX_BUSY_RESUME_COUNT        14
#define STML0XX_BUSY_SUSPEND_COUNT       6

#define STML0XX_RESET_DELAY              50

#define LOG_MSG_SIZE                     40

#define I2C_RESPONSE_LENGTH              8

/* Streaming sensors queue constants */
#define STREAM_SENSOR_TYPE_ACCEL1        1
#define STREAM_SENSOR_TYPE_ACCEL2        2
#define STREAM_SENSOR_TYPE_UNCAL_GYRO    3
#define STREAM_SENSOR_TYPE_UNCAL_MAG     4
#define STREAM_SENSOR_QUEUE_DEPTH        20
#define STREAM_SENSOR_QUEUE_ENTRY_SIZE   11
#define STREAM_SENSOR_QUEUE_SIZE \
	(STREAM_SENSOR_QUEUE_DEPTH * STREAM_SENSOR_QUEUE_ENTRY_SIZE + 5)
#define STREAM_SENSOR_QUEUE_INSERT_IDX \
	(STREAM_SENSOR_QUEUE_DEPTH * STREAM_SENSOR_QUEUE_ENTRY_SIZE)
#define STREAM_SENSOR_QUEUE_REMOVE_IDX \
	(STREAM_SENSOR_QUEUE_DEPTH * STREAM_SENSOR_QUEUE_ENTRY_SIZE + 4)

/* stml0xx non-wake IRQ SPI buffer indexes */
#define IRQ_IDX_STATUS_LO            0
#define IRQ_IDX_STATUS_MED           1
#define IRQ_IDX_STATUS_HI            2
#define IRQ_IDX_STREAM_SENSOR_QUEUE  3
#define IRQ_IDX_GYRO_CAL_X           (IRQ_IDX_STREAM_SENSOR_QUEUE \
				      + STREAM_SENSOR_QUEUE_SIZE)
#define IRQ_IDX_GYRO_CAL_Y           (IRQ_IDX_GYRO_CAL_X + 2)
#define IRQ_IDX_GYRO_CAL_Z           (IRQ_IDX_GYRO_CAL_Y + 2)
#define IRQ_IDX_ALS                  (IRQ_IDX_GYRO_CAL_Z + 2)
#define IRQ_IDX_DISP_ROTATE          (IRQ_IDX_ALS + 2)
#define IRQ_IDX_STEP_COUNTER         (IRQ_IDX_DISP_ROTATE + 1)

/* stml0xx wake IRQ SPI buffer indexes */
#define WAKE_IRQ_IDX_STATUS_LO              0
#define WAKE_IRQ_IDX_STATUS_MED             1
#define WAKE_IRQ_IDX_STATUS_HI              2
#define WAKE_IRQ_IDX_ALGO_STATUS_LO         3
#define WAKE_IRQ_IDX_ALGO_STATUS_MED        4
#define WAKE_IRQ_IDX_ALGO_STATUS_HI         5
#define WAKE_IRQ_IDX_PROX                   6
#define WAKE_IRQ_IDX_COVER                  7
#define WAKE_IRQ_IDX_HEADSET                8
#define WAKE_IRQ_IDX_FLAT                   9
#define WAKE_IRQ_IDX_STOWED                10
#define WAKE_IRQ_IDX_CAMERA                11
#define WAKE_IRQ_IDX_LIFT                  13
#define WAKE_IRQ_IDX_SIM                   25
#define WAKE_IRQ_IDX_MOTION                27
#define WAKE_IRQ_IDX_MODALITY              29
#define WAKE_IRQ_IDX_MODALITY_ORIENT       36
#define WAKE_IRQ_IDX_MODALITY_STOWED       43
#define WAKE_IRQ_IDX_MODALITY_ACCUM        50
#define WAKE_IRQ_IDX_MODALITY_ACCUM_MVMT   52
#define WAKE_IRQ_IDX_LOG_MSG               56
#define WAKE_IRQ_IDX_GLANCE               (WAKE_IRQ_IDX_LOG_MSG + LOG_MSG_SIZE)
#define WAKE_IRQ_IDX_PROX_ALS             (WAKE_IRQ_IDX_GLANCE + 2)
#define WAKE_IRQ_IDX_PROX_UNUSED          (WAKE_IRQ_IDX_PROX_ALS + 2)
#define WAKE_IRQ_IDX_PROX_RAW             (WAKE_IRQ_IDX_PROX_UNUSED + 1)
#define WAKE_IRQ_IDX_PROX_NOISE           (WAKE_IRQ_IDX_PROX_RAW + 2)
#define WAKE_IRQ_IDX_PROX_RECAL           (WAKE_IRQ_IDX_PROX_NOISE + 2)
#define WAKE_IRQ_IDX_PROX_LTHRESH         (WAKE_IRQ_IDX_PROX_RECAL + 2)
#define WAKE_IRQ_IDX_PROX_HTHRESH         (WAKE_IRQ_IDX_PROX_LTHRESH + 2)
#define WAKE_IRQ_IDX_CHOPCHOP         (WAKE_IRQ_IDX_PROX_HTHRESH + 2)

/* Non-wake IRQ work function flags */
#define IRQ_WORK_FLAG_NONE                   0x00

/* stml0xx_readbuff offsets. */
#define IRQ_WAKE_LO  0
#define IRQ_WAKE_MED 1
#define IRQ_WAKE_HI  2

#define IRQ_NOWAKE_LO   0
#define IRQ_NOWAKE_MED  1
#define IRQ_NOWAKE_HI   2

/* stream sensor data offsets */
#define SENSOR_TYPE_IDX    0
#define DELTA_TICKS_IDX    1
#define SENSOR_X_IDX  3
#define SENSOR_Y_IDX  5
#define SENSOR_Z_IDX  7

#define SENSOR_DATA_SIZE         6
#define RAW_MAG_DATA_SIZE        8
#define UNCALIB_SENSOR_DATA_SIZE 12

#define DOCK_STATE	0
#define PROX_DISTANCE	0
#define COVER_STATE	0
#define HEADSET_STATE   0
#define TOUCH_REASON	1
#define FLAT_UP		0
#define FLAT_DOWN	0
#define STOWED_STATUS	0
#define NFC_VALUE	0
#define ALGO_TYPE	7
#define COMPASS_STATUS	12
#define DISP_VALUE	0
#define TEMP_VALUE	0
#define PRESSURE_VALUE	0
#define LIFT_DISTANCE	0
#define LIFT_ROTATION	4
#define LIFT_GRAV_DIFF	8

#define STML0XX_LED_MAX_DELAY 0xFFFF
#define STML0XX_LED_MAX_BRIGHTNESS 0x00FFFFFF
#define STML0XX_LED_HALF_BRIGHTNESS 0x007F7F7F
#define STML0XX_LED_OFF 0x00000000

/* The following macros are intended to be called with the sensorhub IRQ
   handlers only and refer to local variables in those functions. */
#define SH_TO_H16(buf) (int16_t)(*(buf) << 8 | *((buf)+1))
#define SH_TO_UH16(buf) (uint16_t)(*(buf) << 8 | *((buf)+1))
#define SH_TO_H32(buf) (int32_t)(*(buf) << 24 | *((buf)+1) << 16 \
				| *((buf)+2) << 8 | *((buf)+3))

#define STML0XX_HALL_SOUTH 1
#define STML0XX_HALL_NORTH 2

/**
 * struct stml0xx_ioctl_work_struct - struct for deferred ioctl data
 * @ws base struct
 * @cmd ioctl number
 * @data ioctl data
 * @data_len length of @data
 * @algo_req_ndx index into @stml0xx_g_algo_requst
 */
struct stml0xx_ioctl_work_struct {
	struct work_struct ws;
	unsigned int cmd;
	union {
		unsigned char bytes[32];
		unsigned short delay;
	} data;
	unsigned char data_len;
	size_t algo_req_ndx;
};

struct stml0xx_platform_data {
	int (*init) (void);
	void (*exit) (void);
	int (*power_on) (void);
	int (*power_off) (void);
	int gpio_reset;
	int gpio_bslen;
	int gpio_wakeirq;
	int gpio_int;
	int gpio_sh_wake;
	int gpio_spi_ready_for_receive;
	int gpio_spi_data_ack;
	unsigned int bslen_pin_active_value;
	unsigned int reset_hw_type;
	u16 lux_table[LIGHTING_TABLE_SIZE];
	u8 brightness_table[LIGHTING_TABLE_SIZE];
	char fw_version[FW_VERSION_SIZE];
	int ct406_detect_threshold;
	int ct406_undetect_threshold;
	int ct406_recalibrate_threshold;
	int ct406_pulse_count;
	int ct406_prox_gain;
	int ct406_als_lux1_c0_mult;
	int ct406_als_lux1_c1_mult;
	int ct406_als_lux1_div;
	int ct406_als_lux2_c0_mult;
	int ct406_als_lux2_c1_mult;
	int ct406_als_lux2_div;
	int dsp_iface_enable;
	int headset_detect_enable;
	int headset_hw_version;
	int headset_insertion_debounce;
	int headset_removal_debounce;
	int headset_button_down_debounce;
	int headset_button_up_debounce;
	int headset_button_0_1_threshold;
	int headset_button_1_2_threshold;
	int headset_button_2_3_threshold;
	int headset_button_3_upper_threshold;
	int headset_button_1_keycode;
	int headset_button_2_keycode;
	int headset_button_3_keycode;
	int headset_button_4_keycode;
	int accel_orientation_1;
	int accel_orientation_2;
	int accel_swap;
	int cover_detect_polarity;
	int mag_layout;
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
	struct stml0xx_android_sensor_data data;
};

struct stml0xx_data {
	struct stml0xx_platform_data *pdata;
	struct mutex lock;
	struct work_struct initialize_work;
	struct workqueue_struct *irq_work_queue;
	struct workqueue_struct *ioctl_work_queue;
	struct wake_lock wakelock;
	struct wake_lock wake_sensor_wakelock;
	struct wake_lock reset_wakelock;
	struct input_dev *input_dev;

	struct mutex sh_wakeup_lock;
	int sh_wakeup_count;
	int sh_lowpower_enabled;

	int hw_initialized;

	/* SPI */
	struct spi_device *spi;
	struct mutex spi_lock;
	unsigned char *spi_tx_buf;
	unsigned char *spi_rx_buf;

	atomic_t enabled;
	int irq;
	int irq_wake;
	unsigned int irq_wake_work_delay;	/* in ms */
	bool irq_wake_work_pending;
	uint64_t pending_wake_irq_ts_ns;
	unsigned int current_addr;
	enum stm_mode mode;
	unsigned char intp_mask;

	dev_t stml0xx_dev_num;
	struct class *stml0xx_class;
	struct cdev as_cdev;
	struct cdev ms_cdev;

	struct switch_dev dsdev;	/* Standard Dock switch */
	struct switch_dev edsdev;	/* Motorola Dock switch */

	/* Android sensor event queue */
	struct as_node as_queue;
	/* Lock for modifying as_queue */
	spinlock_t as_queue_lock;

	wait_queue_head_t stml0xx_as_data_wq;

	struct stml0xx_moto_sensor_data
	 stml0xx_ms_data_buffer[STML0XX_MS_DATA_QUEUE_SIZE];
	int stml0xx_ms_data_buffer_head;
	int stml0xx_ms_data_buffer_tail;
	wait_queue_head_t stml0xx_ms_data_wq;

	struct regulator *regulator_1;
	struct regulator *regulator_2;
	struct regulator *regulator_3;

	bool discard_sensor_queue;
	bool is_suspended;
#ifdef CONFIG_MMI_HALL_NOTIFICATIONS
	struct mmi_hall_data *hall_data;
#endif

#ifdef CONFIG_SENSORS_SH_AK09912
	struct akm_data_queue_t akm_data_queue;
	uint8_t akm_sense_info[AKM_SENSOR_INFO_SIZE];
	uint8_t akm_sense_conf[AKM_SENSOR_CONF_SIZE];
	struct mutex akm_sensor_mutex;
	struct mutex akm_val_mutex;
	struct mutex akm_accel_mutex;
	atomic_t akm_active;
	atomic_t akm_drdy;
	/* Positive value means the device is working.
	 * 0 or negative value means the device is not woking,
	 * i.e. in power-down mode.
	 */
	int8_t akm_is_busy;
	uint32_t akm_enable_flag;
	wait_queue_head_t akm_drdy_wq;
	wait_queue_head_t akm_open_wq;
	char akm_layout;
	int16_t akm_accel_data[3];
#endif
};

#ifndef ts_to_ns
# define ts_to_ns(ts) ((ts).tv_sec*1000000000LL + (ts).tv_nsec)
#endif
struct stml0xx_work_struct {
	/* Base struct */
	struct work_struct ws;
	/* Timestamp in nanoseconds */
	uint64_t ts_ns;
	uint8_t flags;
};

struct stml0xx_delayed_work_struct {
	/* Base struct */
	struct delayed_work ws;
	/* Timestamp in nanoseconds */
	uint64_t ts_ns;
};

/* per algo config, request, and event registers */
struct stml0xx_algo_info_t {
	unsigned short config_bit;
	unsigned char cfg_register;
	unsigned char req_register;
	unsigned char evt_register;
	unsigned short evt_size;
};

#define ALGO_RQST_DATA_SIZE 28
struct stml0xx_algo_request_t {
	char size;
	char data[ALGO_RQST_DATA_SIZE];
};

#define STML0XX_LED_NAME "rgb"
extern struct attribute_group stml0xx_notification_attribute_group;

irqreturn_t stml0xx_isr(int irq, void *dev);
void stml0xx_irq_work_func(struct work_struct *work);

irqreturn_t stml0xx_wake_isr(int irq, void *dev);
void stml0xx_irq_wake_work_func(struct work_struct *work);

long stml0xx_misc_ioctl(struct file *file, unsigned int cmd, unsigned long arg);

void stml0xx_reset(struct stml0xx_platform_data *pdata);
void stml0xx_initialize_work_func(struct work_struct *work);


int stml0xx_as_data_buffer_write(struct stml0xx_data *ps_stml0xx,
				 unsigned char type, unsigned char *data,
				 int size, unsigned char status,
				 uint64_t timestamp_ns);
int stml0xx_as_data_buffer_read(struct stml0xx_data *ps_stml0xx,
				struct stml0xx_android_sensor_data *buff);
int stml0xx_ms_data_buffer_write(struct stml0xx_data *ps_stml0xx,
				 unsigned char type, unsigned char *data,
				 int size);
int stml0xx_ms_data_buffer_read(struct stml0xx_data *ps_stml0xx,
				struct stml0xx_moto_sensor_data *buff);

/* SPI-related functions */
int stml0xx_spi_transfer(unsigned char *tx_buf, unsigned char *rx_buf, int len);
int stml0xx_spi_write_read_no_reset(unsigned char *tx_buf, int tx_len,
				    unsigned char *rx_buf, int rx_len);
int stml0xx_spi_read_no_reset(unsigned char *buf, int len);
int stml0xx_spi_write_no_reset(unsigned char *buf, int len);
int stml0xx_spi_write_read(unsigned char *tx_buf, int tx_len,
			   unsigned char *rx_buf, int rx_len);
int stml0xx_spi_read(unsigned char *buf, int len);
int stml0xx_spi_write(unsigned char *buf, int len);
int stml0xx_spi_transfer(unsigned char *tx_buf, unsigned char *rx_buf, int len);
int stml0xx_spi_send_write_reg(unsigned char reg_type,
			       unsigned char *reg_data, int reg_data_size);
int stml0xx_spi_send_write_reg_reset(unsigned char reg_type,
			       unsigned char *reg_data, int reg_data_size,
			       uint8_t reset_allowed);
int stml0xx_spi_send_read_reg(unsigned char reg_type,
			      unsigned char *reg_data, int reg_data_size);
int stml0xx_spi_send_read_reg_reset(unsigned char reg_type,
			      unsigned char *reg_data, int reg_data_size,
			      uint8_t reset_allowed);
int stml0xx_spi_read_msg_data(enum sh_spi_msg spi_msg,
				unsigned char *data_buffer,
				int buffer_size,
				enum reset_option reset_allowed);
void stml0xx_spi_swap_bytes(unsigned char *data, int size);
unsigned short stml0xx_spi_calculate_crc(unsigned char *data, int len);
void stml0xx_spi_append_crc(unsigned char *data, int len);

int stml0xx_enable(void);
void stml0xx_wake(struct stml0xx_data *ps_stml0xx);
void stml0xx_sleep(struct stml0xx_data *ps_stml0xx);
void stml0xx_detect_lowpower_mode(void);

int stml0xx_load_brightness_table(void);

int stml0xx_irq_wake_work_func_display_locked(struct stml0xx_data *ps_stml0xx,
					      unsigned short irq_status);
unsigned short stml0xx_get_interrupt_status(struct stml0xx_data *ps_stml0xx,
					    unsigned char reg, int *err);

int stml0xx_boot_flash_erase(void);
int stml0xx_get_version(struct stml0xx_data *ps_stml0xx);
int switch_stml0xx_mode(enum stm_mode mode);
int stml0xx_bootloadermode(struct stml0xx_data *ps_stml0xx);

int stml0xx_led_set(struct led_classdev *led_cdev);
int stml0xx_led_set_reset(struct led_classdev *led_cdev, uint8_t allow_reset);
void stml0xx_brightness_set(struct led_classdev *led_cdev,
	enum led_brightness value);
enum led_brightness stml0xx_brightness_get(struct led_classdev *led_cdev);
int stml0xx_blink_set(struct led_classdev *led_cdev,
	unsigned long *delay_on, unsigned long *delay_off);
uint8_t stml0xx_set_lowpower_mode(enum lowpower_mode lp_type,
		enum reset_option reset);

#ifdef CONFIG_SENSORS_SH_AK09912
void stml0xx_akm_init(struct stml0xx_data *ps_stml0xx);
int akm09912_i2c_check_device(struct stml0xx_data *ps_stml0xx);
int akm09912_enable_mag(int enable, struct stml0xx_data *ps_stml0xx);
int stml0xx_akm_data_queue_insert(struct stml0xx_data *ps_stml0xx,
					uint8_t *data);
uint8_t *stml0xx_akm_data_queue_remove(struct stml0xx_data *ps_stml0xx);
#endif

extern struct stml0xx_data *stml0xx_misc_data;

extern unsigned short stml0xx_g_acc_delay;
extern unsigned short stml0xx_g_acc2_delay;
extern unsigned short stml0xx_g_mag_delay;
extern unsigned short stml0xx_g_gyro_delay;
extern unsigned short stml0xx_g_baro_delay;
extern unsigned short stml0xx_g_als_delay;
extern unsigned short stml0xx_g_step_counter_delay;
extern unsigned long stml0xx_g_nonwake_sensor_state;
extern unsigned short stml0xx_g_algo_state;
extern unsigned char stml0xx_g_motion_dur;
extern unsigned char stml0xx_g_zmotion_dur;
extern unsigned char stml0xx_g_control_reg[STML0XX_CONTROL_REG_SIZE];
extern unsigned char stml0xx_g_mag_cal[STML0XX_MAG_CAL_SIZE];
extern unsigned char stml0xx_g_gyro_cal[STML0XX_GYRO_CAL_SIZE];
extern unsigned char stml0xx_g_accel_cal[STML0XX_ACCEL_CAL_SIZE];
extern unsigned short stml0xx_g_control_reg_restore;
extern bool stml0xx_g_booted;

/* global buffers used exclusively in bootloader mode */
extern unsigned char *stml0xx_boot_cmdbuff;
extern unsigned char *stml0xx_boot_readbuff;

extern unsigned short stml0xx_spi_retry_delay;

extern const struct stml0xx_algo_info_t stml0xx_algo_info[];

extern struct stml0xx_algo_request_t stml0xx_g_algo_request[];

extern long stml0xx_time_delta;

extern unsigned int stml0xx_irq_disable;

extern unsigned long stml0xx_g_wake_sensor_state;

extern unsigned char stat_string[];

extern const struct file_operations stml0xx_as_fops;
extern const struct file_operations stml0xx_ms_fops;
extern const struct file_operations stml0xx_misc_fops;
extern struct miscdevice stml0xx_misc_device;

#endif /* __STML0XX_H__ */
