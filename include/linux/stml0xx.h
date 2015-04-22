/*
 * Copyright (C) 2010-2014 Motorola, Inc.
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

#ifndef __STML0XX_H__
#define __STML0XX_H__

/*The user-space-visible defintions */
#include <uapi/linux/stml0xx.h>

#include <linux/cdev.h>
#include <linux/irq.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/switch.h>
#include <linux/wakelock.h>

/* Log macros */
#define ENABLE_VERBOSE_LOGGING 0

/* SPI */
#define SPI_FLASH_CLK_SPD_HZ    4000000
#define SPI_NORMAL_CLK_SPD_HZ   4000000
#define SPI_BUFF_SIZE           1152
#define SPI_RETRIES             5
#define SPI_RETRY_DELAY         20
#define SPI_SENSORHUB_TIMEOUT   5000
#define SPI_BARKER_1            0xF9
#define SPI_BARKER_2            0xAE
#define SPI_HEADER_SIZE         6
#define SPI_CRC_SIZE            2
#define SPI_WRITE_REG_HDR_SIZE      6
#define SPI_READ_REG_HDR_SIZE       6
#define SPI_CRC_LEN                 2
#define SPI_READ_SENSORS_HDR_SIZE   3
#define SPI_TX_PAYLOAD_LEN         88
#define SPI_MSG_SIZE	\
	(SPI_HEADER_SIZE+SPI_TX_PAYLOAD_LEN+SPI_CRC_SIZE)
#define SPI_RX_PAYLOAD_LEN	\
	(SPI_MSG_SIZE - SPI_CRC_SIZE)

enum sh_spi_msg {
	SPI_MSG_TYPE_READ_REG = 1,
	SPI_MSG_TYPE_WRITE_REG,
	SPI_MSG_TYPE_READ_IRQ_DATA,
	SPI_MSG_TYPE_READ_WAKE_IRQ_DATA
};

#include <linux/spi/spi.h>

/* STML0XX memory map */
#define ID                              0x00
#define REV_ID                          0x01
#define LOG_MSG_STATUS                  0x02
#define LOWPOWER_REG                    0x03
#define INIT_COMPLETE_REG               0x04
#define ACCEL_ORIENTATION		0x06
#define ACCEL_SWAP                      0x07

#define STML0XX_PEEKDATA_REG             0x09
#define STML0XX_PEEKSTATUS_REG           0x0A
#define STML0XX_STATUS_REG               0x0B
#define STML0XX_TOUCH_REG                0x0C
#define STML0XX_CONTROL_REG              0x0D

#define AP_POSIX_TIME                   0x10

#define LED_NOTIF_CONTROL               0X11

#define ACCEL2_UPDATE_RATE		0x13

#define ACCEL_UPDATE_RATE               0x16
#define MAG_UPDATE_RATE                 0x17
#define PRESSURE_UPDATE_RATE            0x18
#define GYRO_UPDATE_RATE                0x19

#define NONWAKESENSOR_CONFIG            0x1A
#define WAKESENSOR_CONFIG               0x1B

#define MOTION_DUR                      0x20
#define ZRMOTION_DUR                    0x22

#define BYPASS_MODE                     0x24
#define SLAVE_ADDRESS                   0x25

#define ALGO_CONFIG                     0x26
#define ALGO_INT_STATUS                 0x27
#define GENERIC_INT_STATUS              0x28

#define HEADSET_HW_VER                  0x2B
#define HEADSET_CONTROL                 0x2C

#define MOTION_DATA                     0x2D

#define HEADSET_DATA                    0x2F

#define HEADSET_SETTINGS                0x2E

#define PROX_SETTINGS                   0x33

#define LUX_TABLE_VALUES                0x34
#define BRIGHTNESS_TABLE_VALUES         0x35

#define INTERRUPT_MASK                  0x37
#define WAKESENSOR_STATUS               0x39
#define INTERRUPT_STATUS                0x3A

#define ACCEL_X                         0x3B
#define LIN_ACCEL_X                     0x3C
#define GRAVITY_X                       0x3D
#define ACCEL2_X			0x3E

#define DOCK_DATA                       0x3F

#define COVER_DATA                      0x40

#define TEMPERATURE_DATA                0x41

#define GYRO_X                          0x43
#define UNCALIB_GYRO_X			0x45
#define UNCALIB_MAG_X			0x46

#define MAG_CAL                         0x48
#define MAG_HX                          0x49

#define DISP_ROTATE_DATA                0x4A
#define FLAT_DATA                       0x4B
#define CAMERA                          0x4C
#define NFC                             0x4D
#define SIM                             0x4E
#define CHOPCHOP                        0x4F
#define LIFT                            0x51

#define SH_LOG_LEVEL_REG                0x55

#define DSP_CONTROL                     0x58

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
/* STML0XX memory map end */

#define LIGHTING_TABLE_SIZE 32

#define STML0XX_AS_DATA_QUEUE_SIZE       0x20
#define STML0XX_AS_DATA_QUEUE_MASK       0x1F
#define STML0XX_MS_DATA_QUEUE_SIZE       0x08
#define STML0XX_MS_DATA_QUEUE_MASK       0x07

#define STML0XX_CLIENT_MASK		0xF0

#define STML0XX_BUSY_STATUS_MASK	0x80
#define STML0XX_BUSY_SLEEP_USEC	10000
#define STML0XX_BUSY_RESUME_COUNT	14
#define STML0XX_BUSY_SUSPEND_COUNT	6

#define STML0XX_MAX_REG_LEN         255

#define LOG_MSG_SIZE			24

#define STML0XX_RESET_DELAY		50

#define I2C_RESPONSE_LENGTH		8

#define STML0XX_MAXDATA_LENGTH		256

/* stml0xx IRQ SPI buffer indexes */
#define IRQ_IDX_STATUS_LO         0
#define IRQ_IDX_STATUS_MED        1
#define IRQ_IDX_STATUS_HI         2
#define IRQ_IDX_ACCEL1            3
#define IRQ_IDX_ACCEL2           36
#define IRQ_IDX_ALS              42
#define IRQ_IDX_DISP_ROTATE      44
#define IRQ_IDX_DISP_BRIGHTNESS  45

/* stml0xx WAKE IRQ SPI buffer indexes */
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

/* stml0xx_readbuff offsets. */
#define IRQ_WAKE_LO  0
#define IRQ_WAKE_MED 1
#define IRQ_WAKE_HI  2

#define IRQ_NOWAKE_LO   0
#define IRQ_NOWAKE_MED  1
#define IRQ_NOWAKE_HI   2

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
#define CHOP_VALUE	0
#define SIM_DATA	0
#define LIFT_DISTANCE	0
#define LIFT_ROTATION	4
#define LIFT_GRAV_DIFF	8

#define STML0XX_LED_MAX_DELAY 0xFFFF
#define STML0XX_LED_MAX_BRIGHTNESS 0x00FFFFFF
#define STML0XX_LED_HALF_BRIGHTNESS 0x007F7F7F
#define STML0XX_LED_OFF 0x00000000

/* The following macros are intended to be called with the stm IRQ handlers */
/* only and refer to local variables in those functions. */
#define STM16_TO_HOST(x, buf) ((short) be16_to_cpu( \
		*((u16 *) (buf+(x)))))
#define STM32_TO_HOST(x, buf) ((short) be32_to_cpu( \
		*((u32 *) (buf+(x)))))

#define STML0XX_HALL_SOUTH 1
#define STML0XX_HALL_NORTH 2

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
};

struct stml0xx_data {
	struct stml0xx_platform_data *pdata;
	struct mutex lock;
	struct work_struct clear_interrupt_status_work;
	struct work_struct initialize_work;
	struct workqueue_struct *irq_work_queue;
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
	unsigned int current_addr;
	enum stm_mode mode;
	unsigned char intp_mask;

	dev_t stml0xx_dev_num;
	struct class *stml0xx_class;
	struct cdev as_cdev;
	struct cdev ms_cdev;

	struct switch_dev dsdev;	/* Standard Dock switch */
	struct switch_dev edsdev;	/* Motorola Dock switch */

	struct stml0xx_android_sensor_data
	 stml0xx_as_data_buffer[STML0XX_AS_DATA_QUEUE_SIZE];
	int stml0xx_as_data_buffer_head;
	int stml0xx_as_data_buffer_tail;
	wait_queue_head_t stml0xx_as_data_wq;

	struct stml0xx_moto_sensor_data
	 stml0xx_ms_data_buffer[STML0XX_MS_DATA_QUEUE_SIZE];
	int stml0xx_ms_data_buffer_head;
	int stml0xx_ms_data_buffer_tail;
	wait_queue_head_t stml0xx_ms_data_wq;

	struct regulator *regulator_1;
	struct regulator *regulator_2;
	struct regulator *regulator_3;

	bool is_suspended;
	bool pending_wake_work;
};

#ifndef ts_to_ns
# define ts_to_ns(ts) ((ts).tv_sec*1000000000LL + (ts).tv_nsec)
#endif
struct stml0xx_work_struct {
	/* Base struct */
	struct work_struct ws;
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
struct stml0xx_algo_requst_t {
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

extern struct stml0xx_data *stml0xx_misc_data;

extern unsigned short stml0xx_g_acc_delay;
extern unsigned short stml0xx_g_acc2_delay;
extern unsigned short stml0xx_g_mag_delay;
extern unsigned short stml0xx_g_gyro_delay;
extern unsigned short stml0xx_g_baro_delay;
extern unsigned long stml0xx_g_nonwake_sensor_state;
extern unsigned short stml0xx_g_algo_state;
extern unsigned char stml0xx_g_motion_dur;
extern unsigned char stml0xx_g_zmotion_dur;
extern unsigned char stml0xx_g_control_reg[STML0XX_CONTROL_REG_SIZE];
extern unsigned char stml0xx_g_mag_cal[STML0XX_MAG_CAL_SIZE];
extern unsigned short stml0xx_g_control_reg_restore;
extern bool stml0xx_g_booted;

/* global buffers used exclusively in bootloader mode */
extern unsigned char *stml0xx_boot_cmdbuff;
extern unsigned char *stml0xx_boot_readbuff;

extern unsigned short stml0xx_spi_retry_delay;

extern const struct stml0xx_algo_info_t stml0xx_algo_info[];

extern struct stml0xx_algo_requst_t stml0xx_g_algo_requst[];

extern long stml0xx_time_delta;

extern unsigned int stml0xx_irq_disable;

extern unsigned long stml0xx_g_wake_sensor_state;

extern unsigned char stat_string[];

extern const struct file_operations stml0xx_as_fops;
extern const struct file_operations stml0xx_ms_fops;
extern const struct file_operations stml0xx_misc_fops;
extern struct miscdevice stml0xx_misc_device;

#endif /* __STML0XX_H__ */
