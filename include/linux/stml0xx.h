
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

#ifdef __KERNEL__
#include <linux/cdev.h>
#include <linux/irq.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/switch.h>
#include <linux/wakelock.h>
#endif

/* Log macros */
#define ENABLE_VERBOSE_LOGGING 1

/* SPI */
#define SPI_DMA_ENABLED         true
#define SPI_FLASH_CLK_SPD_HZ    4000000
#define SPI_NORMAL_CLK_SPD_HZ    4000000
#define SPI_BUFF_SIZE           1152
#define SPI_RETRIES             5
#define SPI_RETRY_DELAY         20
#define SPI_SENSORHUB_TIMEOUT   5000
#define SPI_BARKER_1            0xF9
#define SPI_BARKER_2            0xAE
#define SPI_HEADER_SIZE         6
#define SPI_CRC_SIZE            2
#define SPI_MAX_PAYLOAD_LEN     24
#define SPI_MSG_TYPE_READ_REG       0x01
#define SPI_MSG_TYPE_WRITE_REG      0x02
#define SPI_MSG_TYPE_READ_SENSORS   0x03
#define SPI_WRITE_REG_HDR_SIZE      6
#define SPI_READ_REG_HDR_SIZE       6
#define SPI_MSG_SIZE                32
#define SPI_CRC_LEN                 2
#define SPI_READ_SENSORS_HDR_SIZE   3

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
		_IOR(STML0XX_IOCTL_BASE, 28, char*)
#define STML0XX_IOCTL_SET_ALGOS	\
		_IOW(STML0XX_IOCTL_BASE, 29, char*)
#define STML0XX_IOCTL_GET_MAG_CAL \
		_IOR(STML0XX_IOCTL_BASE, 30, unsigned char*)
#define STML0XX_IOCTL_SET_MAG_CAL \
		_IOW(STML0XX_IOCTL_BASE, 31, unsigned char*)
/* 32 unused */
#define STML0XX_IOCTL_SET_MOTION_DUR	\
		_IOW(STML0XX_IOCTL_BASE, 33, unsigned int)
/* 34 unused */
#define STML0XX_IOCTL_SET_ZRMOTION_DUR	\
		_IOW(STML0XX_IOCTL_BASE, 35, unsigned int)
#define STML0XX_IOCTL_GET_WAKESENSORS	\
		_IOR(STML0XX_IOCTL_BASE, 36, unsigned char)
#define STML0XX_IOCTL_SET_WAKESENSORS	\
		_IOW(STML0XX_IOCTL_BASE, 37, unsigned char)
#define STML0XX_IOCTL_GET_VERNAME	\
		_IOW(STML0XX_IOCTL_BASE, 38, char*)
#define STML0XX_IOCTL_SET_POSIX_TIME	\
		_IOW(STML0XX_IOCTL_BASE, 39, unsigned long)
/* 40-42 unused */
#define STML0XX_IOCTL_SET_ALGO_REQ \
		_IOR(STML0XX_IOCTL_BASE, 43, char*)
#define STML0XX_IOCTL_GET_ALGO_EVT \
		_IOR(STML0XX_IOCTL_BASE, 44, char*)
#define STML0XX_IOCTL_ENABLE_BREATHING \
		_IOW(STML0XX_IOCTL_BASE, 45, unsigned char)
#define STML0XX_IOCTL_WRITE_REG \
		_IOR(STML0XX_IOCTL_BASE, 46, char*)
#define STML0XX_IOCTL_READ_REG \
		_IOR(STML0XX_IOCTL_BASE, 47, char*)
/* 48-52 unused */
#define STML0XX_IOCTL_GET_BOOTED \
		_IOR(STML0XX_IOCTL_BASE, 53, unsigned char)
#define STML0XX_IOCTL_SET_LOWPOWER_MODE \
		_IOW(STML0XX_IOCTL_BASE, 54, char)
#define STML0XX_IOCTL_SET_ACC2_DELAY	\
		_IOW(STML0XX_IOCTL_BASE, 55,  unsigned short)

#define FW_VERSION_SIZE 12
#define STML0XX_CONTROL_REG_SIZE 200
#define STML0XX_STATUS_REG_SIZE 8
#define STML0XX_TOUCH_REG_SIZE  8
#define STML0XX_POWER_REG_SIZE  3
#define STML0XX_MAG_CAL_SIZE 26
#define STML0XX_AS_DATA_BUFF_SIZE 20
#define STML0XX_MS_DATA_BUFF_SIZE 20

#define STML0XX_CAMERA_DATA 0x01

/* Mask values */

/* Non wakable sensors */
#define M_ACCEL			0x000001
#define M_GYRO			0x000002
#define M_PRESSURE		0x000004
#define M_ECOMPASS		0x000008
#define M_TEMPERATURE	0x000010
#define M_ALS			0x000020

#define M_LIN_ACCEL		0x000100
#define M_QUATERNION	0x000200
#define M_GRAVITY		0x000400
#define M_DISP_ROTATE		0x000800
#define M_DISP_BRIGHTNESS	0x001000

#define M_UNCALIB_GYRO		0x008000
#define M_UNCALIB_MAG		0x010000
#define M_ACCEL2		0x020000

/* wake sensor status */
#define M_DOCK			0x000001
#define M_PROXIMITY		0x000002
#define M_TOUCH			0x000004
#define M_COVER			0x000008
#define M_HEADSET               0x000020
#define M_INIT_COMPLETE         0x000040
#define M_HUB_RESET		0x000080


#define M_FLATUP		0x000100
#define M_FLATDOWN		0x000200
#define M_STOWED		0x000400
#define M_CAMERA_ACT		0x000800
#define M_NFC			0x001000
#define M_SIM			0x002000
#define M_LOG_MSG		0x008000

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
	DT_ACCEL2
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
#include <linux/spi/spi.h>

/* STML0XX memory map */
#define ID                              0x00
#define REV_ID                          0x01
#define ERROR_STATUS                    0x02
#define LOWPOWER_REG                    0x03
#define INIT_COMPLETE_REG               0x04

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

#define HEADSET_CONTROL                 0x2C

#define MOTION_DATA                     0x2D

#define HEADSET_SETTINGS                0x2E

#define HEADSET_DATA                    0x2F

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

#define ESR_SIZE			24

#define STML0XX_RESET_DELAY		50

#define I2C_RESPONSE_LENGTH		8

#define STML0XX_MAXDATA_LENGTH		256

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
#define SIM_DATA	0

#define STML0XX_LED_MAX_DELAY 0xFFFF
#define STML0XX_LED_MAX_BRIGHTNESS 0x00FFFFFF
#define STML0XX_LED_HALF_BRIGHTNESS 0x007F7F7F
#define STML0XX_LED_OFF 0x00000000

/* The following macros are intended to be called with the stm IRQ handlers */
/* only and refer to local variables in those functions. */
#define STM16_TO_HOST(x) ((short) be16_to_cpu( \
		*((u16 *) (stml0xx_readbuff+(x)))))
#define STM32_TO_HOST(x) ((short) be32_to_cpu( \
		*((u32 *) (stml0xx_readbuff+(x)))))

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
	u16 lux_table[LIGHTING_TABLE_SIZE];
	u8 brightness_table[LIGHTING_TABLE_SIZE];
	char fw_version[FW_VERSION_SIZE];
	int ct406_detect_threshold;
	int ct406_undetect_threshold;
	int ct406_recalibrate_threshold;
	int ct406_pulse_count;
	int headset_insertion_debounce;
	int headset_removal_debounce;
	int headset_button_down_debounce;
	int headset_button_up_debounce;
	int headset_button_0_1_threshold;
	int headset_button_1_2_threshold;
	int headset_button_2_3_threshold;
	int headset_button_3_upper_threshold;
};

struct stml0xx_data {
	struct spi_device *spi;
	struct stml0xx_platform_data *pdata;
	/* to avoid two spi communications at the same time */
	struct mutex lock;
	struct work_struct irq_work;
	struct work_struct irq_wake_work;
	struct work_struct clear_interrupt_status_work;
	struct work_struct initialize_work;
	struct workqueue_struct *irq_work_queue;
	struct wake_lock wakelock;
	struct wake_lock reset_wakelock;
	struct input_dev *input_dev;

	struct mutex sh_wakeup_lock;
	int sh_wakeup_count;
	int sh_lowpower_enabled;

	int hw_initialized;

	/* SPI DMA */
	bool spi_dma_enabled;
	unsigned char *spi_tx_buf;
	unsigned char *spi_rx_buf;
	dma_addr_t spi_tx_dma;
	dma_addr_t spi_rx_dma;

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

	struct led_classdev led_cdev;
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

void stml0xx_reset(struct stml0xx_platform_data *pdata, unsigned char *cmdbuff);
void stml0xx_initialize_work_func(struct work_struct *work);


int stml0xx_as_data_buffer_write(struct stml0xx_data *ps_stml0xx,
				 unsigned char type, unsigned char *data,
				 int size, unsigned char status);
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

extern unsigned char *stml0xx_cmdbuff;
extern unsigned char *stml0xx_readbuff;

extern unsigned short stml0xx_spi_retry_delay;

extern const struct stml0xx_algo_info_t stml0xx_algo_info[];

extern struct stml0xx_algo_requst_t stml0xx_g_algo_requst[];

extern long stml0xx_time_delta;

extern unsigned int stml0xx_irq_disable;

extern unsigned short stml0xx_g_wake_sensor_state;

extern unsigned char stat_string[];

extern const struct file_operations stml0xx_as_fops;
extern const struct file_operations stml0xx_ms_fops;
extern const struct file_operations stml0xx_misc_fops;
extern struct miscdevice stml0xx_misc_device;

#endif /* __KERNEL__ */

#endif /* __STML0XX_H__ */
