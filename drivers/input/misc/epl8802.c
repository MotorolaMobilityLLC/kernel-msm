/* drivers/i2c/chips/epl8802.c - light and proxmity sensors driver
 * Copyright (C) 2014 ELAN Corporation.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */


#include <linux/hrtimer.h>
#include <linux/timer.h>
#include <linux/delay.h>
#if defined(CONFIG_HAS_EARLYSUSPEND)
#include <linux/earlysuspend.h>
#endif
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/irq.h>
#include <linux/errno.h>
#include <linux/err.h>
#include <linux/gpio.h>
#include <linux/miscdevice.h>
#include <linux/slab.h>
#include <linux/uaccess.h>
#include <asm/mach-types.h>
#include <asm/setup.h>
#include <linux/wakelock.h>
#include <linux/jiffies.h>
#include <linux/epl8802.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_device.h>


/******************************************************************************
 * driver info
*******************************************************************************/
#define EPL_DEV_NAME		"EPL8802"
#define DRIVER_VERSION		"3.0.0_Lenovo"

/******************************************************************************
* paltform select
*******************************************************************************/
/*platform select*/
#define S5PV210     0
#define SPREAD      0
#define QCOM        1
#define LEADCORE    0
#define MARVELL     0
#define SENSOR_CLASS 0

#if QCOM
#include <linux/of_gpio.h>
#endif

#if SENSOR_CLASS
#include <linux/sensors.h>
#endif

/******************************************************************************
 *  ALS / PS function define
 ******************************************************************************/
#define PS_DYN_K_ONE	1	/*PS DYN K ONE*/
#define ALS_DYN_INTT    1       /* ALS Auto INTT*/
#define PS_FIRST_REPORT 1       /* enable ps, report Far or NEAR*/
#define ALS_DYN_INTT_REPORT    1
#define ALSPS_FACTORY_CALI 0

#define ATTR_RANGE_PATH 1

#define ALSPS_DEBUG 0
/******************************************************************************
* ALS / PS define
*******************************************************************************/
#define ALS_POLLING_MODE    1	/* 1 is polling mode, 0 is interrupt mode*/
#define PS_POLLING_MODE      0	/* 1 is polling mode, 0 is interrupt mode*/

#define ALS_LOW_THRESHOLD		1000
#define ALS_HIGH_THRESHOLD		3000

#define PS_LOW_THRESHOLD		2073 /*PS low threshold*/
#define PS_HIGH_THRESHOLD		7525 /*PS high threshold*/

#define LUX_PER_COUNT			400 /*ALS lux per count, 400/1000=0.4*/
#define ALS_LEVEL    16

static int als_level[] = {20, 45, 70, 90, 150, 300, 500, 700, 1150,
			  2250, 4500, 8000, 15000, 30000, 50000
			 };
static int als_value[] = {10, 30, 60, 80, 100, 200, 400, 600, 800,
			  1500, 3000, 6000, 10000, 20000, 40000, 60000
			 };

static int polling_time = 200;  /* report rate for polling mode*/
#define ELAN_INT_PIN 129    /*Interrupt pin setting*/

/*ALS interrupt table*/ /*for ALS interrupt mode*/
unsigned long als_lux_intr_level[] = {15, 39, 63, 316, 639, 4008,
				      5748, 10772, 14517, 65535
				     };
unsigned long als_adc_intr_level[10] = {0};

int als_frame_time = 0;
int ps_frame_time = 0;

/******************************************************************************
 *  factory setting
 ******************************************************************************/
 #if ALSPS_FACTORY_CALI
/*ps calibration file location*/
static const char ps_cal_file[] =
	"/data/data/com.eminent.ps.calibration/ps.dat";
/*als calibration file location*/
static const char als_cal_file[] =
	"/data/data/com.eminent.ps.calibration/als.dat";

static int PS_h_offset = 2000;
static int PS_l_offset = 1000;
static int PS_MAX_XTALK = 30000;
#endif

/******************************************************************************
 *I2C function define
*******************************************************************************/
#define TXBYTES                             2
#define RXBYTES                             2
#define PACKAGE_SIZE                   8
#define I2C_RETRY_COUNT          10
int i2c_max_count = 8;

/******************************************************************************
 *  configuration
 ******************************************************************************/
#if S5PV210
static const char ElanPsensorName[] = "proximity";
static const char ElanALsensorName[] = "lightsensor-level";
#elif SPREAD
static const char ElanPsensorName[] = "light sensor";
#elif QCOM || LEADCORE
static const char ElanPsensorName[] = "Rear proximity sensor";
static const char ElanALsensorName[] = "light";
#elif MARVELL
static const char ElanPsensorName[] = "proximity_sensor";
static const char ElanALsensorName[] = "light_sensor";
#endif

#define LOG_TAG "alsps_epl8802"
#if ALSPS_DEBUG
#define LOG_FUN(f)             pr_info(LOG_TAG " %s\n", __func__)
#define LOG_INFO(fmt, args...)    pr_info(LOG_TAG fmt, ##args)
#else
#define LOG_FUN(f)
#define LOG_INFO(fmt, args...)
#endif
#define LOG_DBG(fmt, args...)	pr_info(LOG_TAG fmt, ##args)
#define LOG_ERR(fmt, args...)	pr_err(LOG_TAG "%s %d : "fmt, __func__,\
							__LINE__, ##args)

enum CMC_ALS_REPORT_TYPE {
	CMC_BIT_RAW			= 0x0,
	CMC_BIT_PRE_COUNT      = 0x1,
	CMC_BIT_DYN_INT		= 0x2,
	CMC_BIT_TABLE                 = 0x3,
	CMC_BIT_INTR_LEVEL      = 0x4,
};

struct epl_raw_data {
	u8 raw_bytes[PACKAGE_SIZE];
	u16 renvo;
};

struct epl_sensor_priv {
	struct i2c_client *client;
	struct input_dev *als_input_dev;
	struct input_dev *ps_input_dev;
	struct delayed_work  eint_work;
	struct delayed_work  polling_work;
#if defined(CONFIG_HAS_EARLYSUSPEND)
	struct early_suspend early_suspend;
#endif
	int intr_pin;
	int (*power)(int on);
	int ps_opened;
	int als_opened;
	int als_suspend;
	int ps_suspend;
	int lux_per_count;
	int enable_pflag;
	int enable_lflag;
	int read_flag;
	int irq;
#if SENSOR_CLASS
	struct sensors_classdev	als_cdev;
	struct sensors_classdev	ps_cdev;
	int			flush_count;
#endif
	/*data*/
	u16		als_level_num;
	u16		als_value_num;
	u32		als_level[ALS_LEVEL - 1];
	u32		als_value[ALS_LEVEL];
	/*als interrupt*/
	int als_intr_level;
	int als_intr_lux;

	/* pin ctl */
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};
#if ATTR_RANGE_PATH
static struct kobject *kernel_kobj_dev;
#endif
static struct platform_device *sensor_dev;
struct epl_sensor_priv *epl_sensor_obj;
static epl_optical_sensor epl_sensor;
static struct epl_raw_data	gRawData;
static struct wake_lock ps_lock;
static struct mutex sensor_mutex;

static int epl_sensor_setup_interrupt(struct epl_sensor_priv *epld);
static int ps_sensing_time(int intt, int adc, int cycle);
static int als_sensing_time(int intt, int adc, int cycle);
static void epl_sensor_eint_work(struct work_struct *work);
static void epl_sensor_polling_work(struct work_struct *work);
static int als_intr_update_table(struct epl_sensor_priv *epld);
#if ALSPS_FACTORY_CALI
static int factory_ps_data(void);
static int factory_ps_data(void);
#endif
#if PS_DYN_K_ONE
static void epl_sensor_do_ps_auto_k_one(void);
#endif
#if ALS_DYN_INTT_REPORT
static int epl_sensor_als_dyn_report(bool report_flag);
#endif
/******************************************************************************
 *  PS DYN K ONE
 ******************************************************************************/
#if PS_DYN_K_ONE
static bool ps_dyn_flag;
#define PS_MAX_CT	7600
#define PS_MAX_IR	50000
#define PS_DYN_H_OFFSET 6943
#define PS_DYN_L_OFFSET 2024
u32 dynk_thd_low = 0;
u32 dynk_thd_high = 0;
#endif
/******************************************************************************
 *  PS FIRST REPORT
 ******************************************************************************/
#if PS_FIRST_REPORT
static bool ps_frist_flag = true;
#endif
/******************************************************************************
 *  ALS DYN INTT
 ******************************************************************************/
#if ALS_DYN_INTT
/*
 *Dynamic INTT
 */
int dynamic_intt_idx;
int dynamic_intt_init_idx = 1;	/*initial dynamic_intt_idx */
int c_gain;
int dynamic_intt_lux = 0;

uint16_t dynamic_intt_high_thr;
uint16_t dynamic_intt_low_thr;
uint32_t dynamic_intt_max_lux = 17000;
uint32_t dynamic_intt_min_lux = 0;
uint32_t dynamic_intt_min_unit = 1000;

static int als_dynamic_intt_intt[] = {EPL_ALS_INTT_1024, EPL_ALS_INTT_1024};
static int als_dynamic_intt_value[] = {1024, 1024};
static int als_dynamic_intt_gain[] = {EPL_GAIN_MID, EPL_GAIN_LOW};
static int als_dynamic_intt_high_thr[] = {60000, 60000};
static int als_dynamic_intt_low_thr[] = {200, 200};
static int als_dynamic_intt_intt_num =  sizeof(als_dynamic_intt_value) / sizeof(
		int);
#endif
/******************************************************************************
 *  ALS_DYN_INTT_REPORT
 ******************************************************************************/
#if ALS_DYN_INTT_REPORT
u16 factory_dyn_intt_raw = 0;
#endif
/******************************************************************************
 *  Sensor calss
 ******************************************************************************/
#if SENSOR_CLASS
#define PS_MIN_POLLING_RATE    200
#define ALS_MIN_POLLING_RATE    200

static struct sensors_classdev als_cdev = {
	.name = "epl8802-light",
	.vendor = "Eminent Technology Corp",
	.version = 1,
	.handle = SENSORS_LIGHT_HANDLE,
	.type = SENSOR_TYPE_LIGHT,
	.max_range = "65536",
	.resolution = "1.0",
	.sensor_power = "0.25",
	.min_delay = 50000,
	.max_delay = 2000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.flags = 2,
	.enabled = 0,
	.delay_msec = 50,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};

static struct sensors_classdev ps_cdev = {
	.name = "epl8802-proximity",
	.vendor = "Eminent Technology Corp",
	.version = 1,
	.handle = SENSORS_PROXIMITY_HANDLE,
	.type = SENSOR_TYPE_PROXIMITY,
	.max_range = "5",
	.resolution = "5.0",
	.sensor_power = "0.25",
	.min_delay = 10000,
	.max_delay = 2000,
	.fifo_reserved_event_count = 0,
	.fifo_max_event_count = 0,
	.flags = 3,
	.enabled = 0,
	.delay_msec = 50,
	.sensors_enable = NULL,
	.sensors_poll_delay = NULL,
};
#endif

/*
//====================I2C write operation===============//
*/
static int epl_sensor_I2C_Write_Block(struct i2c_client *client, u8 addr,
				      u8 *data,
				      u8 len)
{
	/*because address also occupies one byte,
		the maximum length for write is 7 bytes*/
	int err, idx, num;
	char buf[8];

	err = 0;

	if (!client) {
		return -EINVAL;
	} else if (len >= 8) {
		LOG_ERR(" length %d exceeds %d\n", len, 8);
		return -EINVAL;
	}

	num = 0;
	buf[num++] = addr;
	for (idx = 0; idx < len; idx++)
		buf[num++] = data[idx];

	err = i2c_master_send(client, buf, num);
	if (err < 0) {
		LOG_ERR("send command error!!\n");

		return -EFAULT;
	}

	return err;
}

static int epl_sensor_I2C_Write_Cmd(struct i2c_client *client, uint8_t regaddr,
				    uint8_t data, uint8_t txbyte)
{
	uint8_t buffer[2];
	int ret = 0;
	int retry;

	buffer[0] = regaddr;
	buffer[1] = data;

	for (retry = 0; retry < I2C_RETRY_COUNT; retry++) {
		ret = i2c_master_send(client, buffer, txbyte);

		if (ret == txbyte)
			break;

		LOG_ERR("i2c write error,TXBYTES %d\n", ret);
		mdelay(10);
	}

	if (retry >= I2C_RETRY_COUNT) {
		LOG_ERR("i2c write retry over %d\n", I2C_RETRY_COUNT);
		return -EINVAL;
	}

	return ret;
}

static int epl_sensor_I2C_Write(struct i2c_client *client, uint8_t regaddr,
				uint8_t data)
{
	int ret = 0;

	ret = epl_sensor_I2C_Write_Cmd(client, regaddr, data, 0x02);
	return ret;
	return 0;
}

static int epl_sensor_I2C_Read(struct i2c_client *client, uint8_t regaddr,
			       uint8_t bytecount)
{
	int ret = 0;

	int retry;
	int read_count = 0, rx_count = 0;

	while (bytecount > 0) {
		epl_sensor_I2C_Write_Cmd(client, regaddr + read_count,
				0x00, 0x01);

		for (retry = 0; retry < I2C_RETRY_COUNT; retry++) {
			rx_count = bytecount > i2c_max_count ?
					i2c_max_count : bytecount;
			ret = i2c_master_recv(client,
				&gRawData.raw_bytes[read_count], rx_count);

			if (ret == rx_count)
				break;

			LOG_ERR("i2c read error,RXBYTES %d\r\n", ret);
			mdelay(10);
		}

		if (retry >= I2C_RETRY_COUNT) {
			LOG_ERR("i2c read retry over %d\n", I2C_RETRY_COUNT);
			return -EINVAL;
		}
		bytecount -= rx_count;
		read_count += rx_count;
	}

	return ret;
}

static void epl_sensor_restart_polling(void)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	cancel_delayed_work(&epld->polling_work);
	schedule_delayed_work(&epld->polling_work,
			msecs_to_jiffies(polling_time));
}

static int set_psensor_intr_threshold(uint16_t low_thd, uint16_t high_thd)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	struct i2c_client *client = epld->client;
	uint8_t high_msb , high_lsb, low_msb, low_lsb;
	u8 buf[4];

	buf[3] = high_msb = (uint8_t)(high_thd >> 8);
	buf[2] = high_lsb = (uint8_t)(high_thd & 0x00ff);
	buf[1] = low_msb  = (uint8_t)(low_thd >> 8);
	buf[0] = low_lsb  = (uint8_t)(low_thd & 0x00ff);

	mutex_lock(&sensor_mutex);
	epl_sensor_I2C_Write_Block(client, 0x0c, buf, 4);
	mutex_unlock(&sensor_mutex);

	LOG_DBG("%s: low_thd = %d, high_thd = %d\n", __func__,
			low_thd, high_thd);

	return 0;
}

static int set_lsensor_intr_threshold(uint16_t low_thd, uint16_t high_thd)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	struct i2c_client *client = epld->client;
	uint8_t high_msb , high_lsb, low_msb, low_lsb;
	u8 buf[4];

	buf[3] = high_msb = (uint8_t)(high_thd >> 8);
	buf[2] = high_lsb = (uint8_t)(high_thd & 0x00ff);
	buf[1] = low_msb  = (uint8_t)(low_thd >> 8);
	buf[0] = low_lsb  = (uint8_t)(low_thd & 0x00ff);

	mutex_lock(&sensor_mutex);
	epl_sensor_I2C_Write_Block(client, 0x08, buf, 4);
	mutex_unlock(&sensor_mutex);
	LOG_INFO("%s: low_thd = %d, high_thd = %d\n", __func__,
			low_thd, high_thd);

	return 0;
}

static void epl_sensor_report_lux(int report_lux)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
		LOG_INFO("ALS raw = %d, lux = %d\n\n",
			epl_sensor.als.dyn_intt_raw, report_lux);
	} else {
		LOG_INFO("ALS raw = %d, lux = %d\n\n",
			epl_sensor.als.data.channels[1], report_lux);
	}

#if SPREAD
	input_report_abs(epld->ps_input_dev, ABS_MISC, report_lux);
	input_sync(epld->ps_input_dev);
#else
	input_report_abs(epld->als_input_dev, ABS_MISC, report_lux);
	input_sync(epld->als_input_dev);
#endif
}

static void epl_sensor_report_ps_status(void)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	ktime_t	timestamp;
	int distance;

	timestamp = ktime_get();

	LOG_DBG("epl_sensor.ps.data.data=%d, value=%d\n",
		epl_sensor.ps.data.data, epl_sensor.ps.compare_low >> 3);

	if (epl_sensor.ps.compare_low >> 3 == 0)
		distance = 1;
	else if (epl_sensor.ps.compare_low >> 3 == 1)
		distance = 10;
	else
		distance = -1;

	input_report_abs(epld->ps_input_dev, ABS_DISTANCE, distance);
	input_event(epld->ps_input_dev, EV_SYN, SYN_TIME_SEC,
		    ktime_to_timespec(timestamp).tv_sec);
	input_event(epld->ps_input_dev, EV_SYN, SYN_TIME_NSEC,
		    ktime_to_timespec(timestamp).tv_nsec);
	input_sync(epld->ps_input_dev);
}

static void set_als_ps_intr_type(struct i2c_client *client, bool ps_polling,
				 bool als_polling)
{
	/*
	 *set als / ps interrupt control mode and trigger type
	 */
	switch ((ps_polling << 1) | als_polling) {
	case 0: /* ps and als interrupt */
		epl_sensor.interrupt_control = EPL_INT_CTRL_ALS_OR_PS;
		epl_sensor.als.interrupt_type = EPL_INTTY_ACTIVE;
		epl_sensor.ps.interrupt_type = EPL_INTTY_ACTIVE;
		break;

	case 1: /*ps interrupt and als polling */
		epl_sensor.interrupt_control = EPL_INT_CTRL_PS;
		epl_sensor.als.interrupt_type = EPL_INTTY_DISABLE;
		epl_sensor.ps.interrupt_type = EPL_INTTY_ACTIVE;
		break;

	case 2: /* ps polling and als interrupt */
		epl_sensor.interrupt_control = EPL_INT_CTRL_ALS;
		epl_sensor.als.interrupt_type = EPL_INTTY_ACTIVE;
		epl_sensor.ps.interrupt_type = EPL_INTTY_DISABLE;
		break;

	case 3: /*ps and als polling */
		epl_sensor.interrupt_control = EPL_INT_CTRL_ALS_OR_PS;
		epl_sensor.als.interrupt_type = EPL_INTTY_DISABLE;
		epl_sensor.ps.interrupt_type = EPL_INTTY_DISABLE;
		break;
	}
}

static void write_global_variable(struct i2c_client *client)
{
	u8 buf;

	epl_sensor_I2C_Write(client, 0x11, EPL_POWER_OFF | EPL_RESETN_RESET);
	/*
	 *wake up chip
	 *buf = epl_sensor.reset | epl_sensor.power;
	 *epl_sensor_I2C_Write(client, 0x11, buf);
	 */

	/* read revno*/
	epl_sensor_I2C_Read(client, 0x20, 2);
	epl_sensor.revno = gRawData.raw_bytes[0] | gRawData.raw_bytes[1] << 8;

	/*chip refrash*/
	epl_sensor_I2C_Write(client, 0xfd, 0x8e);
	epl_sensor_I2C_Write(client, 0xfe, 0x22);
	epl_sensor_I2C_Write(client, 0xfe, 0x02);
	epl_sensor_I2C_Write(client, 0xfd, 0x00);

	epl_sensor_I2C_Write(client, 0xfc, EPL_A_D | EPL_NORMAL |
		EPL_GFIN_ENABLE | EPL_VOS_ENABLE | EPL_DOC_ON);

	/*
	 *set als / ps interrupt control mode and trigger type
	 */
	set_als_ps_intr_type(client, epl_sensor.ps.polling_mode,
			     epl_sensor.als.polling_mode);

	/*ps setting*/
	buf = epl_sensor.ps.integration_time | epl_sensor.ps.gain;
	epl_sensor_I2C_Write(client, 0x03, buf);

	buf = epl_sensor.ps.adc | epl_sensor.ps.cycle;
	epl_sensor_I2C_Write(client, 0x04, buf);

	buf = epl_sensor.ps.ir_on_control | epl_sensor.ps.ir_mode |
	      epl_sensor.ps.ir_drive;
	epl_sensor_I2C_Write(client, 0x05, buf);

	buf = epl_sensor.interrupt_control | epl_sensor.ps.persist |
	      epl_sensor.ps.interrupt_type;
	epl_sensor_I2C_Write(client, 0x06, buf);

	buf = epl_sensor.ps.compare_reset | epl_sensor.ps.lock;
	epl_sensor_I2C_Write(client, 0x1b, buf);

	epl_sensor_I2C_Write(client, 0x22,
			(u8)(epl_sensor.ps.cancelation & 0xff));
	epl_sensor_I2C_Write(client, 0x23,
			     (u8)((epl_sensor.ps.cancelation & 0xff00) >> 8));
	set_psensor_intr_threshold(epl_sensor.ps.low_threshold,
				   epl_sensor.ps.high_threshold);

	/*als setting*/
	buf = epl_sensor.als.integration_time | epl_sensor.als.gain;
	epl_sensor_I2C_Write(client, 0x01, buf);

	buf = epl_sensor.als.adc | epl_sensor.als.cycle;
	epl_sensor_I2C_Write(client, 0x02, buf);

	buf = epl_sensor.als.interrupt_channel_select | epl_sensor.als.persist |
	      epl_sensor.als.interrupt_type;
	epl_sensor_I2C_Write(client, 0x07, buf);

	buf = epl_sensor.als.compare_reset | epl_sensor.als.lock;
	epl_sensor_I2C_Write(client, 0x12, buf);

	set_lsensor_intr_threshold(epl_sensor.als.low_threshold,
				   epl_sensor.als.high_threshold);


	/*
	 *set mode and wait
	 */
	buf = epl_sensor.wait | epl_sensor.mode;
	epl_sensor_I2C_Write(client, 0x00, buf);
}

/*====================initial global variable===============/*/
static void initial_global_variable(struct i2c_client *client,
				    struct epl_sensor_priv *obj)
{
	/*
	 *general setting
	 */
	epl_sensor.power = EPL_POWER_ON;
	epl_sensor.reset = EPL_RESETN_RUN;
	epl_sensor.mode = EPL_MODE_IDLE;
	epl_sensor.wait = EPL_WAIT_400_MS;
	epl_sensor.osc_sel = EPL_OSC_SEL_1MHZ;

	/*
	 *als setting
	 */
	epl_sensor.als.polling_mode = ALS_POLLING_MODE;
	epl_sensor.als.integration_time = EPL_ALS_INTT_1024;
	epl_sensor.als.gain = EPL_GAIN_LOW;
	epl_sensor.als.adc = EPL_PSALS_ADC_11;
	epl_sensor.als.cycle = EPL_CYCLE_16;
	epl_sensor.als.interrupt_channel_select = EPL_ALS_INT_CHSEL_1;
	epl_sensor.als.persist = EPL_PERIST_1;
	epl_sensor.als.compare_reset = EPL_CMP_RUN;
	epl_sensor.als.lock = EPL_UN_LOCK;
	epl_sensor.als.report_type =
		CMC_BIT_DYN_INT; /*CMC_BIT_RAW; //CMC_BIT_DYN_INT;  */
	epl_sensor.als.high_threshold = ALS_HIGH_THRESHOLD;
	epl_sensor.als.low_threshold = ALS_LOW_THRESHOLD;
	/*
	 *als factory
	 */
	epl_sensor.als.factory.calibration_enable =  false;
	epl_sensor.als.factory.calibrated = false;
	epl_sensor.als.factory.lux_per_count = LUX_PER_COUNT;
	/*
	 *update als intr table
	 */
	if (epl_sensor.als.polling_mode == 0)
		als_intr_update_table(obj);

#if ALS_DYN_INTT
	if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
		dynamic_intt_idx = dynamic_intt_init_idx;
		epl_sensor.als.integration_time =
				als_dynamic_intt_intt[dynamic_intt_idx];
		epl_sensor.als.gain =
				als_dynamic_intt_gain[dynamic_intt_idx];
		dynamic_intt_high_thr =
				als_dynamic_intt_high_thr[dynamic_intt_idx];
		dynamic_intt_low_thr =
				als_dynamic_intt_low_thr[dynamic_intt_idx];
	}
	c_gain = 300; /* 300/1000=0.3 //Lux per count */
#endif
	/*
	 *ps setting
	 */
	epl_sensor.ps.polling_mode = PS_POLLING_MODE;
	epl_sensor.ps.integration_time = EPL_PS_INTT_272;
	epl_sensor.ps.gain = EPL_GAIN_LOW;
	epl_sensor.ps.adc = EPL_PSALS_ADC_11;
	epl_sensor.ps.cycle = EPL_CYCLE_8;
	epl_sensor.ps.persist = EPL_PERIST_1;
	epl_sensor.ps.ir_on_control = EPL_IR_ON_CTRL_ON;
	epl_sensor.ps.ir_mode = EPL_IR_MODE_CURRENT;
	epl_sensor.ps.ir_drive = EPL_IR_DRIVE_100;
	epl_sensor.ps.compare_reset = EPL_CMP_RUN;
	epl_sensor.ps.lock = EPL_UN_LOCK;
	epl_sensor.ps.high_threshold = PS_HIGH_THRESHOLD;
	epl_sensor.ps.low_threshold = PS_LOW_THRESHOLD;
	/*
	 *ps factory
	 */
	epl_sensor.ps.factory.calibration_enable =  false;
	epl_sensor.ps.factory.calibrated = false;
	epl_sensor.ps.factory.cancelation = 0;

	/*
	 *write setting to sensor
	 */
	write_global_variable(client);
}

#if ALSPS_FACTORY_CALI
/*----------------referce code for factory --------------------*/
static int write_factory_calibration(struct epl_sensor_priv *epl_data,
				     char *ps_data,
				     int ps_cal_len)
{
	struct file *fp_cal;
	struct filename name = {.name = ps_cal_file};
	mm_segment_t fs;
	loff_t pos;

	LOG_FUN();
	pos = 0;

	fp_cal = file_open_name(name, O_CREAT | O_RDWR
			| O_TRUNC, 0755/*S_IRWXU*/);
	if (IS_ERR(fp_cal)) {
		LOG_ERR("[ELAN]create file_h error\n");
		return -EIO;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	vfs_write(fp_cal, ps_data, ps_cal_len, &pos);

	filp_close(fp_cal, NULL);

	set_fs(fs);

	return 0;
}

static bool read_factory_calibration(void)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	int ret;
	struct filename name;
	char buffer[100] = {0};

	if (epl_sensor.ps.factory.calibration_enable
	    && !epl_sensor.ps.factory.calibrated) {
		name.name = ps_cal_file;
		fp = file_open_name(name, O_RDWR, S_IRUSR);

		if (IS_ERR(fp)) {
			LOG_ERR("NO PS calibration file(%d)\n",
					(int)IS_ERR(fp));
			epl_sensor.ps.factory.calibration_enable =  false;
		} else {
			int ps_cancelation = 0, ps_hthr = 0, ps_lthr = 0;

			pos = 0;
			fs = get_fs();
			set_fs(KERNEL_DS);
			vfs_read(fp, buffer, sizeof(buffer), &pos);
			filp_close(fp, NULL);

			ret = sscanf(buffer, "%d,%d,%d", &ps_cancelation,
					&ps_hthr, &ps_lthr);
			epl_sensor.ps.factory.cancelation = ps_cancelation;
			epl_sensor.ps.factory.high_threshold = ps_hthr;
			epl_sensor.ps.factory.low_threshold = ps_lthr;
			set_fs(fs);

			epl_sensor.ps.high_threshold =
					epl_sensor.ps.factory.high_threshold;
			epl_sensor.ps.low_threshold =
					epl_sensor.ps.factory.low_threshold;
			epl_sensor.ps.cancelation =
					epl_sensor.ps.factory.cancelation;
		}

		epl_sensor_I2C_Write(epld->client, 0x22,
			     (u8)(epl_sensor.ps.cancelation & 0xff));
		epl_sensor_I2C_Write(epld->client, 0x23,
			     (u8)((epl_sensor.ps.cancelation & 0xff00) >> 8));
		set_psensor_intr_threshold(epl_sensor.ps.low_threshold,
				   epl_sensor.ps.high_threshold);

		epl_sensor.ps.factory.calibrated = true;
	}

	if (epl_sensor.als.factory.calibration_enable
	    && !epl_sensor.als.factory.calibrated) {
		name.name = als_cal_file;
		fp = file_open_name(name, O_RDONLY, S_IRUSR);
		if (IS_ERR(fp)) {
			LOG_ERR("NO ALS calibration file(%d)\n",
				(int)IS_ERR(fp));
			epl_sensor.als.factory.calibration_enable =  false;
		} else {
			int als_lux_per_count = 0;

			pos = 0;
			fs = get_fs();
			set_fs(KERNEL_DS);
			vfs_read(fp, buffer, sizeof(buffer), &pos);
			filp_close(fp, NULL);

			/*ret = sscanf(buffer, "%d", &als_lux_per_count);*/
			ret = kstrtoint(buffer, 10, &als_lux_per_count);
			if (ret)
				return ret;
			epl_sensor.als.factory.lux_per_count =
					als_lux_per_count;
			set_fs(fs);
		}
		epl_sensor.als.factory.calibrated = true;
	}
	return true;
}

static int epl_run_ps_calibration(struct epl_sensor_priv *epl_data)
{
	struct epl_sensor_priv *epld = epl_data;
	u16 ch1 = 0;
	int ps_hthr = 0, ps_lthr = 0, ps_cancelation = 0, ps_cal_len = 0;
	char ps_calibration[20];

	if (PS_MAX_XTALK < 0) {
		LOG_ERR("[%s]:Failed: PS_MAX_XTALK < 0 \r\n", __func__);
		return -EINVAL;
	}

	switch (epl_sensor.mode) {
	case EPL_MODE_PS:
	case EPL_MODE_ALS_PS:
		ch1 = factory_ps_data();
		break;
	}

	if (ch1 > PS_MAX_XTALK) {
		LOG_ERR("[%s]:Failed: ch1 > max_xtalk(%d)\r\n", __func__, ch1);
		return -EINVAL;
	} else if (ch1 < 0) {
		LOG_ERR("[%s]:Failed: ch1 = 0\r\n", __func__);
		return -EINVAL;
	}

	ps_hthr = ch1 + PS_h_offset;
	ps_lthr = ch1 + PS_l_offset;

	ps_cal_len = snprintf(ps_calibration, PAGE_SIZE,
			"%d,%d,%d", ps_cancelation, ps_hthr, ps_lthr);

	if (write_factory_calibration(epld, ps_calibration, ps_cal_len) < 0) {
		LOG_ERR("[%s] create file error\n", __func__);
		return -EINVAL;
	}

	epl_sensor.ps.low_threshold = ps_lthr;
	epl_sensor.ps.high_threshold = ps_hthr;
	set_psensor_intr_threshold(epl_sensor.ps.low_threshold,
				   epl_sensor.ps.high_threshold);

	LOG_INFO("[%s]: ch1 = %d\n", __func__, ch1);

	return ch1;
}
#endif
/*----------------------------------------------------------------------------*/

#if ALS_DYN_INTT
long raw_convert_to_lux(u16 raw_data)
{
	long lux = 0;
	long dyn_intt_raw = 0;
	int gain_value = 0;

	if (epl_sensor.als.gain == EPL_GAIN_MID)
		gain_value = 8;
	else if (epl_sensor.als.gain == EPL_GAIN_LOW)
		gain_value = 1;

	dyn_intt_raw = (raw_data * 10) / (10 * gain_value *
			als_dynamic_intt_value[dynamic_intt_idx] /
			als_dynamic_intt_value[1]);

	LOG_INFO("[%s]: dyn_intt_raw=%ld \r\n", __func__, dyn_intt_raw);

	if (dyn_intt_raw > 0xffff)
		epl_sensor.als.dyn_intt_raw = 0xffff;
	else
		epl_sensor.als.dyn_intt_raw = dyn_intt_raw;

	lux = c_gain * epl_sensor.als.dyn_intt_raw;

	LOG_INFO(
		"[%s]:raw_data=%d, epl_sensor.als.dyn_intt_raw=%d,lux=%ld \r\n",
		__func__, raw_data,
		epl_sensor.als.dyn_intt_raw, lux);

	if (lux >= (dynamic_intt_max_lux * dynamic_intt_min_unit)) {
		LOG_INFO(
			"[%s]:raw_convert_to_lux:change max lux\r\n",
			__func__);
		lux = dynamic_intt_max_lux * dynamic_intt_min_unit;
	} else if (lux <= (dynamic_intt_min_lux * dynamic_intt_min_unit)) {
		LOG_INFO(
			"[%s]:raw_convert_to_lux:change min lux\r\n", __func__);
		lux = dynamic_intt_min_lux * dynamic_intt_min_unit;
	}

	return lux;
}
#endif

static int epl_sensor_get_als_value(struct epl_sensor_priv *obj, u16 als)
{
	int idx;
	int invalid = 0;
	int level = 0;
	long lux;
#if ALS_DYN_INTT
	long now_lux = 0, lux_tmp = 0;
	bool change_flag = false;
#endif

	switch (epl_sensor.als.report_type) {
	case CMC_BIT_RAW:
		return als;

	case CMC_BIT_PRE_COUNT:
		return (als * epl_sensor.als.factory.lux_per_count) / 1000;

	case CMC_BIT_TABLE:
		for (idx = 0; idx < obj->als_level_num; idx++) {
			if (als < als_level[idx])
				break;
		}

		if (idx >= obj->als_value_num) {
			LOG_ERR("exceed range\n");
			idx = obj->als_value_num - 1;
		}

		if (!invalid) {
			LOG_INFO("ALS: %05d => %05d\n", als, als_value[idx]);
			als = als_value[idx];
		} else
			LOG_ERR("ALS: %05d => %05d (-1)\n",
				als, als_value[idx]);

		return als;

#if ALS_DYN_INTT
	case CMC_BIT_DYN_INT:
		LOG_INFO(
			"[%s]:intt_idx=%d,intt_value=%d,intt_gain=%d,als=%d\r\n",
			__func__, dynamic_intt_idx,
			als_dynamic_intt_value[dynamic_intt_idx],
			als_dynamic_intt_gain[dynamic_intt_idx], als);

		if (als > dynamic_intt_high_thr) {
			if (dynamic_intt_idx ==
				(als_dynamic_intt_intt_num - 1)) {
				als = dynamic_intt_high_thr;
				lux_tmp = raw_convert_to_lux(als);

				LOG_INFO(">>>>>>>>>>>>>>>>>>>INTT_MAX_LUX\r\n");
			} else {
				change_flag = true;
				als  = dynamic_intt_high_thr;
				lux_tmp = raw_convert_to_lux(als);
				dynamic_intt_idx++;

				LOG_INFO(">>>>>change INTT high:%d,raw:%d\r\n",
					dynamic_intt_idx, als);
			}
		} else if (als < dynamic_intt_low_thr) {
			if (dynamic_intt_idx == 0) {
				/*
				 *als = dynamic_intt_low_thr;
				 */
				lux_tmp = raw_convert_to_lux(als);

				LOG_INFO(">>>>>>>>>>INTT_MIN_LUX\r\n");
			} else {
				change_flag = true;
				als  = dynamic_intt_low_thr;
				lux_tmp = raw_convert_to_lux(als);
				dynamic_intt_idx--;

				LOG_INFO(">>>change INTT low:%d,raw:%d\r\n",
					dynamic_intt_idx, als);
			}
		} else {
			lux_tmp = raw_convert_to_lux(als);
		}

		now_lux = lux_tmp;
		dynamic_intt_lux = now_lux / dynamic_intt_min_unit;
		if (change_flag == true) {
			LOG_INFO("[%s]: ALS_DYN_INTT:Chang Setting \r\n",
					__func__);
			epl_sensor.als.integration_time =
				als_dynamic_intt_intt[dynamic_intt_idx];
			epl_sensor.als.gain =
				als_dynamic_intt_gain[dynamic_intt_idx];
			dynamic_intt_high_thr =
				als_dynamic_intt_high_thr[dynamic_intt_idx];
			dynamic_intt_low_thr =
				als_dynamic_intt_low_thr[dynamic_intt_idx];
			epl_sensor_fast_update(obj->client);

			mutex_lock(&sensor_mutex);
			/*
			 *epl_sensor_I2C_Write(obj->client, 0x11,
			 *EPL_POWER_OFF | EPL_RESETN_RESET);
			 *epl_sensor_I2C_Write(obj->client, 0x01,
			 *epl_sensor.als.integration_time |
			 *epl_sensor.als.gain);
			 */
			epl_sensor_I2C_Write(obj->client, 0x00,
					epl_sensor.wait | epl_sensor.mode);
			epl_sensor_I2C_Write(obj->client, 0x11,
					EPL_POWER_ON | EPL_RESETN_RUN);
			mutex_unlock(&sensor_mutex);
#if ALS_DYN_INTT_REPORT
			dynamic_intt_lux = -1;
#endif
		}
#if ALS_DYN_INTT_REPORT
		else
			factory_dyn_intt_raw = epl_sensor.als.dyn_intt_raw;
#endif

		if (epl_sensor.als.polling_mode == 0) {
			lux = dynamic_intt_lux;
			if (lux > 0xFFFF)
				lux = 0xFFFF;

			for (idx = 0; idx < 10; idx++) {
				if (lux <= (*(als_lux_intr_level + idx))) {
					level = idx;
					if (*(als_lux_intr_level + idx))
						break;
				}
				if (idx == 9) {
					level = idx;
					break;
				}
			}
			obj->als_intr_level = level;
			obj->als_intr_lux = lux;
			return level;
		} else {
			return dynamic_intt_lux;
		}

		break;
#endif
	case CMC_BIT_INTR_LEVEL:
		lux = (als * epl_sensor.als.factory.lux_per_count) / 1000;
		if (lux > 0xFFFF)
			lux = 0xFFFF;

		for (idx = 0; idx < 10; idx++) {
			if (lux <= (*(als_lux_intr_level + idx))) {
				level = idx;
				if (*(als_lux_intr_level + idx))
					break;
			}
			if (idx == 9) {
				level = idx;
				break;
			}
		}
		obj->als_intr_level = level;
		obj->als_intr_lux = lux;
		return level;
	}
	return 0;
}

/*----------------------------------------------------------------------------*/
int epl_sensor_read_als(struct i2c_client *client)
{
	struct epl_sensor_priv *obj = i2c_get_clientdata(client);
	u8 buf[5];

	if (client == NULL) {
		LOG_ERR("CLIENT CANN'T EQUL NULL\n");
		return -EINVAL;
	}

	mutex_lock(&sensor_mutex);
	epl_sensor_I2C_Read(obj->client, 0x12, 5);
	buf[0] = gRawData.raw_bytes[0];
	buf[1] = gRawData.raw_bytes[1];
	buf[2] = gRawData.raw_bytes[2];
	buf[3] = gRawData.raw_bytes[3];
	buf[4] = gRawData.raw_bytes[4];
	mutex_unlock(&sensor_mutex);

	epl_sensor.als.saturation = (buf[0] & 0x20);
	epl_sensor.als.compare_high = (buf[0] & 0x10);
	epl_sensor.als.compare_low = (buf[0] & 0x08);
	epl_sensor.als.interrupt_flag = (buf[0] & 0x04);
	epl_sensor.als.compare_reset = (buf[0] & 0x02);
	epl_sensor.als.lock = (buf[0] & 0x01);
	epl_sensor.als.data.channels[0] = (buf[2] << 8) | buf[1];
	epl_sensor.als.data.channels[1] = (buf[4] << 8) | buf[3];

	LOG_INFO("als: ~~~~ ALS ~~~~~\n");
	LOG_INFO("als: buf = 0x%x\n", buf[0]);
	LOG_INFO("als: sat = 0x%x\n", epl_sensor.als.saturation);
	LOG_INFO("als: cmp h = 0x%x, l = %d\n", epl_sensor.als.compare_high,
		 epl_sensor.als.compare_low);
	LOG_INFO("als: int_flag = 0x%x\n", epl_sensor.als.interrupt_flag);
	LOG_INFO("als: cmp_rstn = 0x%x, lock = 0x%0x\n",
		epl_sensor.als.compare_reset, epl_sensor.als.lock);
	LOG_INFO("read als channel 0 = %d\n", epl_sensor.als.data.channels[0]);
	LOG_INFO("read als channel 1 = %d\n", epl_sensor.als.data.channels[1]);

	return 0;
}

/*----------------------------------------------------------------------------*/
int epl_sensor_read_ps(struct i2c_client *client)
{
	u8 buf[5];

	if (client == NULL) {
		LOG_ERR("CLIENT CANN'T EQUL NULL\n");
		return -EINVAL;
	}

	mutex_lock(&sensor_mutex);
	epl_sensor_I2C_Read(client, 0x1b, 5);
	buf[0] = gRawData.raw_bytes[0];
	buf[1] = gRawData.raw_bytes[1];
	buf[2] = gRawData.raw_bytes[2];
	buf[3] = gRawData.raw_bytes[3];
	buf[4] = gRawData.raw_bytes[4];
	mutex_unlock(&sensor_mutex);

	epl_sensor.ps.saturation = (buf[0] & 0x20);
	epl_sensor.ps.compare_high = (buf[0] & 0x10);
	epl_sensor.ps.compare_low = (buf[0] & 0x08);
	epl_sensor.ps.interrupt_flag = (buf[0] & 0x04);
	epl_sensor.ps.compare_reset = (buf[0] & 0x02);
	epl_sensor.ps.lock = (buf[0] & 0x01);
	epl_sensor.ps.data.ir_data = (buf[2] << 8) | buf[1];
	epl_sensor.ps.data.data = (buf[4] << 8) | buf[3];

	LOG_INFO("ps: ~~~~ PS ~~~~~\n");
	LOG_INFO("ps: buf = 0x%x\n", buf[0]);
	LOG_INFO("ps: sat = 0x%x\n", epl_sensor.ps.saturation);
	LOG_INFO("ps: cmp h = 0x%x, l = 0x%x\n", epl_sensor.ps.compare_high,
		 epl_sensor.ps.compare_low);
	LOG_INFO("ps: int_flag = 0x%x\n", epl_sensor.ps.interrupt_flag);
	LOG_INFO("ps: cmp_rstn = 0x%x, lock = %x\n",
		epl_sensor.ps.compare_reset, epl_sensor.ps.lock);
	LOG_INFO("[%s]: data = %d\n", __func__, epl_sensor.ps.data.data);
	LOG_INFO("[%s]: ir data = %d\n", __func__, epl_sensor.ps.data.ir_data);

	return 0;
}

/*----------------------------------------------------------------------------*/
#if ALSPS_FACTORY_CALI
static int factory_ps_data(void)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	bool enable_ps = epld->enable_pflag == 1 && epld->ps_suspend == 0;

	if (enable_ps == 1 && epl_sensor.ps.polling_mode == 0)
		epl_sensor_read_ps(epld->client);
	else if (enable_ps == 0)
		LOG_INFO("[%s]: ps is disabled \r\n", __func__);

	LOG_INFO("[%s]: enable_ps=%d, ps_raw=%d \r\n", __func__, enable_ps,
		 epl_sensor.ps.data.data);

	return epl_sensor.ps.data.data;
}

static int factory_als_data(void)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	u16 als_raw = 0;
	int als_lux = 0;
	bool enable_als = epld->enable_lflag == 1 && epld->als_suspend == 0;

	if (enable_als == 1 && epl_sensor.als.polling_mode == 0) {
		epl_sensor_read_als(epld->client);

		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT)
			als_lux = epl_sensor_get_als_value(epld,
				epl_sensor.als.data.channels[1]);
	} else if (enable_als == 0)
		LOG_INFO("[%s]: als is disabled \r\n", __func__);

	if (epl_sensor.als.report_type == CMC_BIT_DYN_INT)	{
#if ALS_DYN_INTT_REPORT
		als_raw = factory_dyn_intt_raw;
#else
		als_raw = epl_sensor.als.dyn_intt_raw;
#endif
		LOG_INFO("[%s]: ALS_DYN_INTT: als_raw=%d \r\n",
			__func__, als_raw);
	} else {
		als_raw = epl_sensor.als.data.channels[1];
		LOG_INFO("[%s]: als_raw=%d \r\n", __func__, als_raw);
	}

	return als_raw;
}
#endif

void epl_sensor_enable_ps(int enable)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_DBG("[%s]: ps enable=%d \r\n", __func__, enable);

	if (epld->enable_pflag != enable) {
		epld->enable_pflag = enable;
		if (enable) {
			/*wake_lock(&ps_lock);*/
#if PS_FIRST_REPORT
			ps_frist_flag = true;
			set_psensor_intr_threshold(epl_sensor.ps.high_threshold,
					epl_sensor.ps.high_threshold + 1);
#endif
#if PS_DYN_K_ONE
			ps_dyn_flag = true;
#endif
		} else {
			/*wake_unlock(&ps_lock);*/
		}
		epl_sensor_fast_update(epld->client);
		epl_sensor_update_mode(epld->client);
#if PS_DYN_K_ONE
		epl_sensor_do_ps_auto_k_one();
#endif
	}
}

void epl_sensor_enable_als(int enable)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_DBG("[%s]: enable=%d \r\n", __func__, enable);

	if (epld->enable_lflag != enable) {
#if ALS_DYN_INTT
		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
			dynamic_intt_idx = dynamic_intt_init_idx;
			epl_sensor.als.integration_time =
				als_dynamic_intt_intt[dynamic_intt_idx];
			epl_sensor.als.gain =
				als_dynamic_intt_gain[dynamic_intt_idx];
			dynamic_intt_high_thr =
				als_dynamic_intt_high_thr[dynamic_intt_idx];
			dynamic_intt_low_thr =
				als_dynamic_intt_low_thr[dynamic_intt_idx];
		}
#endif
		epld->enable_lflag = enable;
		if (epld->enable_lflag == 1)
			epl_sensor_fast_update(epld->client);
		epl_sensor_update_mode(epld->client);
#if ALS_DYN_INTT_REPORT
		if (epld->enable_lflag == 1)	 {
			if (epl_sensor.als.report_type == CMC_BIT_DYN_INT)
				epl_sensor_als_dyn_report(true);
		}
#endif
	}
}

/************************************************************************/
/*
 *for 3637
 */
static int als_sensing_time(int intt, int adc, int cycle)
{
	long sensing_us_time;
	int sensing_ms_time;
	int als_intt, als_adc, als_cycle;

	als_intt = als_intt_value[intt >> 2];
	als_adc = adc_value[adc >> 3];
	als_cycle = cycle_value[cycle];

	LOG_INFO("ALS: INTT=%d, ADC=%d, Cycle=%d \r\n",
		als_intt, als_adc, als_cycle);

	sensing_us_time = (als_intt + als_adc * 2 * 2) * 2 * als_cycle;
	sensing_ms_time = sensing_us_time / 1000;

	LOG_INFO("[%s]: sensing=%d ms \r\n", __func__, sensing_ms_time);

	return (sensing_ms_time + 5);
}

static int ps_sensing_time(int intt, int adc, int cycle)
{
	long sensing_us_time;
	int sensing_ms_time;
	int ps_intt, ps_adc, ps_cycle;

	ps_intt = ps_intt_value[intt >> 2];
	ps_adc = adc_value[adc >> 3];
	ps_cycle = cycle_value[cycle];

	LOG_INFO("PS: INTT=%d, ADC=%d, Cycle=%d \r\n",
		ps_intt, ps_adc, ps_cycle);

	sensing_us_time = (ps_intt * 3 + ps_adc * 2 * 3) * ps_cycle;
	sensing_ms_time = sensing_us_time / 1000;

	LOG_INFO("[%s]: sensing=%d ms\r\n", __func__, sensing_ms_time);

	return (sensing_ms_time + 5);
}

#if SENSOR_CLASS
static int epld_sensor_cdev_enable_als(struct sensors_classdev *sensors_cdev,
				       unsigned int enable)
{
	LOG_INFO("[%s]: enable=%d \r\n", __func__, enable);

	epl_sensor_enable_als(enable);

	return 0;
}

static int epld_sensor_cdev_enable_ps(struct sensors_classdev *sensors_cdev,
				      unsigned int enable)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_INFO("[%s]: enable=%d \r\n", __func__, enable);

	epl_sensor_enable_ps(enable);

	return 0;
}

static ssize_t epl_snesor_cdev_set_ps_delay(struct sensors_classdev
		*sensors_cdev,
		unsigned int delay_msec)
{
	/*
	 *struct elan_epl_data *epld = container_of
	 *(sensors_cdev, struct elan_epl_data, als_cdev);
	 */
	if (delay_msec < PS_MIN_POLLING_RATE)	/*at least 200 ms */
		delay_msec = PS_MIN_POLLING_RATE;

	polling_time = delay_msec;	/* convert us => ms */

	return 0;
}

static ssize_t epl_sensor_cdev_set_als_delay(struct sensors_classdev
		*sensors_cdev,
		unsigned int delay_msec)
{
	/*
	 *struct elan_epl_data *epld = container_of
	 *(sensors_cdev, struct elan_epl_data, als_cdev);
	 */

	if (delay_msec < ALS_MIN_POLLING_RATE)	/*at least 200 ms */
		delay_msec = ALS_MIN_POLLING_RATE;

	polling_time = delay_msec;	/* convert us => ms */

	return 0;
}
#endif

#if ALS_DYN_INTT_REPORT
static int epl_sensor_als_dyn_report(bool report_flag)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int als_value = -1, i = 0;

	do {
		epl_sensor_read_als(epld->client);
		als_value = epl_sensor_get_als_value(epld,
				epl_sensor.als.data.channels[1]);

		LOG_INFO("[%s]: als_value=%d \r\n", __func__, als_value);

		if (als_value != -1 && report_flag == true) {
			LOG_INFO("[%s]: als_dyn_report(%d) \r\n",
				__func__, als_value);
			epl_sensor_report_lux(als_value);
		}

		i++;
	} while (-1 == als_value);

	return als_value;
}
#endif

#if PS_DYN_K_ONE
static void epl_sensor_do_ps_auto_k_one(void)
{
#if !PS_FIRST_REPORT
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int ps_time = ps_sensing_time(epl_sensor.ps.integration_time,
		epl_sensor.ps.adc, epl_sensor.ps.cycle);
#endif

	if (ps_dyn_flag == true) {
#if !PS_FIRST_REPORT
		msleep(ps_time);
		LOG_INFO("[%s]: msleep(%d)\r\n", __func__, ps_time);
		epl_sensor_read_ps(epld->client);
#endif
		if (epl_sensor.ps.data.data < PS_MAX_CT
			&& (epl_sensor.ps.saturation == 0)
			&& (epl_sensor.ps.data.ir_data < PS_MAX_IR)) {
			LOG_INFO("[%s]: epl_sensor.ps.data.data=%d \r\n",
				__func__, epl_sensor.ps.data.data);
			dynk_thd_low = epl_sensor.ps.data.data
					+ PS_DYN_L_OFFSET;
			dynk_thd_high = epl_sensor.ps.data.data
					+ PS_DYN_H_OFFSET;
			set_psensor_intr_threshold(dynk_thd_low, dynk_thd_high);
		} else {
			LOG_INFO("[%s]:threshold err;ps.data.data=%d\r\n",
				__func__, epl_sensor.ps.data.data);
			epl_sensor.ps.high_threshold = PS_HIGH_THRESHOLD;
			epl_sensor.ps.low_threshold = PS_LOW_THRESHOLD;
			set_psensor_intr_threshold(
				epl_sensor.ps.low_threshold,
				epl_sensor.ps.high_threshold);
			dynk_thd_low = epl_sensor.ps.low_threshold;
			dynk_thd_high = epl_sensor.ps.high_threshold;
		}

		ps_dyn_flag = false;
		LOG_INFO("[%s]: reset ps_dyn_flag ........... \r\n", __func__);
	}
}
#endif

void epl_sensor_fast_update(struct i2c_client *client)
{
	int als_fast_time = 0;

	LOG_FUN();
	mutex_lock(&sensor_mutex);
	als_fast_time = als_sensing_time(epl_sensor.als.integration_time,
					 epl_sensor.als.adc,
					 EPL_CYCLE_1);

	epl_sensor_I2C_Write(client, 0x11, EPL_POWER_OFF | EPL_RESETN_RESET);
	epl_sensor_I2C_Write(client, 0x02, epl_sensor.als.adc | EPL_CYCLE_1);
	epl_sensor_I2C_Write(client, 0x01,
			     epl_sensor.als.integration_time |
			     epl_sensor.als.gain);
	epl_sensor_I2C_Write(client, 0x00, epl_sensor.wait | EPL_MODE_ALS);
	epl_sensor_I2C_Write(client, 0x11, EPL_POWER_ON | EPL_RESETN_RUN);
	mutex_unlock(&sensor_mutex);

	msleep(als_fast_time);
	LOG_INFO("[%s]: msleep(%d)\r\n", __func__, als_fast_time);

	mutex_lock(&sensor_mutex);
	if (epl_sensor.als.polling_mode == 0) {
		/*
		 *fast_mode is already ran one frame, so must to
		 *reset CMP bit for als intr mode
		 *IDLE mode and CMMP reset
		 */
		epl_sensor_I2C_Write(client, 0x00, epl_sensor.wait |
			EPL_MODE_IDLE);
		epl_sensor_I2C_Write(client, 0x12, EPL_CMP_RESET |
			EPL_UN_LOCK);
		epl_sensor_I2C_Write(client, 0x12, EPL_CMP_RUN |
			EPL_UN_LOCK);
	}

	epl_sensor_I2C_Write(client, 0x11, EPL_POWER_OFF | EPL_RESETN_RESET);
	epl_sensor_I2C_Write(client, 0x02, epl_sensor.als.adc |
		epl_sensor.als.cycle);
	/*
	 *epl_sensor_I2C_Write(client, 0x11, EPL_POWER_ON | EPL_RESETN_RUN);
	 */
	mutex_unlock(&sensor_mutex);
}

void epl_sensor_update_mode(struct i2c_client *client)
{
	struct epl_sensor_priv *obj = epl_sensor_obj;
	int als_time = 0, ps_time = 0;
	bool enable_ps = obj->enable_pflag == 1 && obj->ps_suspend == 0;
	bool enable_als = obj->enable_lflag == 1 && obj->als_suspend == 0;

	als_frame_time = als_time = als_sensing_time(
		epl_sensor.als.integration_time,
		epl_sensor.als.adc, epl_sensor.als.cycle);
	ps_frame_time = ps_time = ps_sensing_time(
		epl_sensor.ps.integration_time,
		epl_sensor.ps.adc, epl_sensor.ps.cycle);

	LOG_INFO("mode selection =0x%x\n", enable_ps | (enable_als << 1));

	switch ((enable_als << 1) | enable_ps) {
	case 0: /*disable all */
		epl_sensor.mode = EPL_MODE_IDLE;
		break;
	case 1: /*als = 0, ps = 1 */
		epl_sensor.mode = EPL_MODE_PS;
		break;
	case 2: /*als = 1, ps = 0 */
		epl_sensor.mode = EPL_MODE_ALS;
		break;
	case 3: /*als = 1, ps = 1 */
		epl_sensor.mode = EPL_MODE_ALS_PS;
		break;
	}

	/*
	 * initial factory calibration variable
	 */
#if ALSPS_FACTORY_CALI
	read_factory_calibration();
#endif

	mutex_lock(&sensor_mutex);
	/*
	 ***** write setting ****
	 * step 1. set sensor at idle mode
	 * step 2. uplock and reset als / ps status
	 * step 3. set sensor at operation mode
	 * step 4. delay sensing time
	 * step 5. unlock and run als / ps status
	 */
	epl_sensor_I2C_Write(client, 0x11, EPL_POWER_OFF | EPL_RESETN_RESET);

	epl_sensor_I2C_Write(obj->client, 0x00, epl_sensor.wait |
			epl_sensor.mode);

	/* if mode isnt IDLE, PWR_ON and RUN */
	if (epl_sensor.mode != EPL_MODE_IDLE)
		epl_sensor_I2C_Write(client, 0x11, EPL_POWER_ON |
			EPL_RESETN_RUN);

	/*
	 ***** check setting ****
	 */
	if (enable_ps == 1) {
		LOG_INFO("[%s] PS:low_thd = %d, high_thd = %d\n", __func__,
			 epl_sensor.ps.low_threshold,
			 epl_sensor.ps.high_threshold);
	}
	if (enable_als == 1 && epl_sensor.als.polling_mode == 0) {
		LOG_INFO("[%s] ALS:low_thd = %d, high_thd = %d\n", __func__,
			 epl_sensor.als.low_threshold,
			 epl_sensor.als.high_threshold);
	}

	LOG_INFO("[%s] reg0x00= 0x%x\n", __func__,
			epl_sensor.wait | epl_sensor.mode);
	LOG_INFO("[%s] reg0x07= 0x%x\n", __func__,
		 epl_sensor.als.interrupt_channel_select |
		 epl_sensor.als.persist | epl_sensor.als.interrupt_type);
	LOG_INFO("[%s] reg0x06= 0x%x\n", __func__,
		 epl_sensor.interrupt_control | epl_sensor.ps.persist |
		 epl_sensor.ps.interrupt_type);
	LOG_INFO("[%s] reg0x11= 0x%x\n", __func__,
		epl_sensor.power | epl_sensor.reset);
	LOG_INFO("[%s] reg0x12= 0x%x\n", __func__,
		 epl_sensor.als.compare_reset | epl_sensor.als.lock);
	LOG_INFO("[%s] reg0x1b= 0x%x\n", __func__,
		 epl_sensor.ps.compare_reset | epl_sensor.ps.lock);
	mutex_unlock(&sensor_mutex);

#if PS_FIRST_REPORT
	if (ps_frist_flag == true) {
		ps_frist_flag = false;
		LOG_INFO("[%s]: PS CMP RESET/RUN \r\n", __func__);
		mutex_lock(&sensor_mutex);
		epl_sensor_I2C_Write(obj->client, 0x1b, EPL_CMP_RESET
				| EPL_UN_LOCK);
		epl_sensor_I2C_Write(obj->client, 0x1b, EPL_CMP_RUN
				| EPL_UN_LOCK);
		mutex_unlock(&sensor_mutex);

		msleep(ps_time);
		LOG_INFO("[%s] PS msleep(%dms)\r\n", __func__, ps_time);
	}
#endif

	if ((enable_als == 1 && epl_sensor.als.polling_mode == 1) ||
	    (enable_ps == 1 && epl_sensor.ps.polling_mode == 1)) {
		epl_sensor_restart_polling();
	}
}

/*----------------------------------------------------------------------------*/
static void epl_sensor_polling_work(struct work_struct *work)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	struct i2c_client *client = epld->client;
	int report_lux = 0;
	bool enable_ps = epld->enable_pflag == 1 && epld->ps_suspend == 0;
	bool enable_als = epld->enable_lflag == 1 && epld->als_suspend == 0;

	LOG_INFO("enable_pflag = %d, enable_lflag = %d\n",
		enable_ps, enable_als);
	cancel_delayed_work(&epld->polling_work);

	if ((enable_als &&  epl_sensor.als.polling_mode == 1) || (enable_ps
			&&  epl_sensor.ps.polling_mode == 1)) {
		schedule_delayed_work(&epld->polling_work,
			msecs_to_jiffies(polling_time));
	}

	if (enable_als &&  epl_sensor.als.polling_mode == 1) {
		epl_sensor_read_als(client);

#if ALS_DYN_INTT_REPORT
		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT)	{
			report_lux = epl_sensor_als_dyn_report(true);
		} else
#endif
		{
			report_lux = epl_sensor_get_als_value(epld,
				epl_sensor.als.data.channels[1]);
			epl_sensor_report_lux(report_lux);
		}
	}

	if (enable_ps && epl_sensor.ps.polling_mode == 1) {
		epl_sensor_read_ps(client);
		epl_sensor_report_ps_status();
	}

	if (enable_als == false && enable_ps == false) {
		cancel_delayed_work(&epld->polling_work);
		LOG_INFO("disable sensor\n");
	}
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static irqreturn_t epl_sensor_eint_func(int irqNo, void *handle)
{
	struct epl_sensor_priv *epld = (struct epl_sensor_priv *)handle;

	disable_irq_nosync(epld->irq);
	schedule_delayed_work(&epld->eint_work, 0);

	return IRQ_HANDLED;
}
/*----------------------------------------------------------------------------*/
static void epl_sensor_intr_als_report_lux(void)
{
	struct epl_sensor_priv *obj = epl_sensor_obj;
	u16 als;

	/*
	 *#if ALS_DYN_INTT
	 *    int last_dynamic_intt_idx = dynamic_intt_idx;
	 *#endif
	 */
	LOG_INFO("[%s]: IDEL MODE \r\n", __func__);
	mutex_lock(&sensor_mutex);
	epl_sensor_I2C_Write(obj->client, 0x00, epl_sensor.wait |
			EPL_MODE_IDLE);
	mutex_unlock(&sensor_mutex);

	als = epl_sensor_get_als_value(obj, epl_sensor.als.data.channels[1]);

	mutex_lock(&sensor_mutex);
	epl_sensor_I2C_Write(obj->client, 0x12, EPL_CMP_RESET |
			EPL_UN_LOCK);
	  /*After runing CMP_RESET, dont clean interrupt_flag */
	epl_sensor_I2C_Write(obj->client, 0x11,
			     EPL_POWER_OFF |
			     EPL_RESETN_RESET);
	mutex_unlock(&sensor_mutex);

	/*
	 *#if ALS_DYN_INTT
	 *    if(dynamic_intt_idx == last_dynamic_intt_idx)
	 *#endif
	 */
	{
		LOG_INFO("[%s]: report als = %d \r\n", __func__, als);

		epl_sensor_report_lux(als);
	}

	if (epl_sensor.als.report_type == CMC_BIT_INTR_LEVEL
	    || epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
#if ALS_DYN_INTT
		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT)	{
			if (dynamic_intt_idx == 0) {
				int gain_value = 0;
				int normal_value = 0;
				long low_thd = 0, high_thd = 0;

				if (epl_sensor.als.gain == EPL_GAIN_MID)
					gain_value = 8;
				else if (epl_sensor.als.gain == EPL_GAIN_LOW)
					gain_value = 1;

				normal_value = gain_value *
				  als_dynamic_intt_value[dynamic_intt_idx] /
				       als_dynamic_intt_value[1];

				if ((als == 0)) /*Note: als =0 is nothing */
					low_thd = 0;
				else
					low_thd = (*(als_adc_intr_level +
						(als - 1)) + 1) * normal_value;
				high_thd = (*(als_adc_intr_level + als)) *
						normal_value;

				if (low_thd > 0xfffe)
					low_thd = 65533;
				if (high_thd >= 0xffff)
					high_thd = 65534;

				epl_sensor.als.low_threshold = low_thd;
				epl_sensor.als.high_threshold = high_thd;
			} else {
				epl_sensor.als.low_threshold =
					*(als_adc_intr_level + (als - 1)) + 1;
				epl_sensor.als.high_threshold =
					*(als_adc_intr_level + als);

				if (epl_sensor.als.low_threshold == 0)
					epl_sensor.als.low_threshold = 1;
			}
			LOG_INFO("[%s]:dynamic_intt_idx=%d, thd(%d/%d) \r\n",
				__func__, dynamic_intt_idx,
				 epl_sensor.als.low_threshold,
				 epl_sensor.als.high_threshold);
		} else
#endif
		{
			epl_sensor.als.low_threshold =
				*(als_adc_intr_level + (als - 1)) + 1;
			epl_sensor.als.high_threshold =
				*(als_adc_intr_level + als);
			if ((als == 0) ||
				(epl_sensor.als.data.channels[1] == 0)) {
				epl_sensor.als.low_threshold = 0;
			}
		}
	}

	/*
	 *write new threshold
	 */
	set_lsensor_intr_threshold(epl_sensor.als.low_threshold,
				   epl_sensor.als.high_threshold);
	mutex_lock(&sensor_mutex);
	epl_sensor_I2C_Write(obj->client, 0x00,
			epl_sensor.wait | epl_sensor.mode);
	epl_sensor_I2C_Write(obj->client, 0x11,
			EPL_POWER_ON | EPL_RESETN_RUN);
	mutex_unlock(&sensor_mutex);
	LOG_INFO("[%s]: MODE=0x%x \r\n", __func__, epl_sensor.mode);
}

/*----------------------------------------------------------------------------*/
static void epl_sensor_eint_work(struct work_struct *work)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	bool enable_ps = epld->enable_pflag == 1
			&& epld->ps_suspend == 0;

	LOG_INFO("xxxxxxxxxxx\n\n");

	epl_sensor_read_ps(epld->client);
	epl_sensor_read_als(epld->client);
	if (epl_sensor.ps.interrupt_flag == EPL_INT_TRIGGER) {
		if (enable_ps) {
			wake_lock_timeout(&ps_lock, 2 * HZ);
			epl_sensor_report_ps_status();
		}
		/*
		 *PS unlock interrupt pin and restart chip
		 */
		mutex_lock(&sensor_mutex);
		epl_sensor_I2C_Write(epld->client, 0x1b,
				EPL_CMP_RUN | EPL_UN_LOCK);
		mutex_unlock(&sensor_mutex);
	}

	if (epl_sensor.als.interrupt_flag == EPL_INT_TRIGGER) {
		epl_sensor_intr_als_report_lux();
		/*
		 *ALS unlock interrupt pin and restart chip
		 */
		mutex_lock(&sensor_mutex);
		epl_sensor_I2C_Write(epld->client, 0x12,
				EPL_CMP_RUN | EPL_UN_LOCK);
		mutex_unlock(&sensor_mutex);
	}

	enable_irq(epld->irq);
}
/*----------------------------------------------------------------------------*/
static int epl_sensor_setup_interrupt(struct epl_sensor_priv *epld)
{
	struct i2c_client *client = epld->client;
	int err = 0;
#if QCOM || MARVELL
	unsigned int irq_gpio;
	unsigned int irq_gpio_flags;
	struct device_node *np = client->dev.of_node;
#endif

	usleep_range(5000, 10000);
#if S5PV210
	epld->intr_pin = S5PV210_GPH0(1);
	err = gpio_request(S5PV210_GPH0(1), "Elan EPL IRQ");
	if (err) {
		LOG_ERR("gpio pin request fail (%d)\n", err);
		goto initial_fail;
	} else {
		LOG_INFO("----- Samsung gpio config success -----\n");
		s3c_gpio_cfgpin(S5PV210_GPH0(1),
			S3C_GPIO_SFN(0x0F)/*(S5PV210_GPH0_1_EXT_INT30_1) */);
		s3c_gpio_setpull(S5PV210_GPH0(1), S3C_GPIO_PULL_UP);
	}
	epld->irq = gpio_to_irq(epld->intr_pin);
#elif SPREAD
	epld->intr_pin = ELAN_INT_PIN; /*need setting*/
	err = gpio_request(epld->intr_pin, "Elan EPL IRQ");
	if (err) {
		LOG_ERR("gpio pin request fail (%d)\n", err);
		goto initial_fail;
	} else {
		gpio_direction_input(epld->intr_pin);

		/*get irq*/
		client->irq = gpio_to_irq(epld->intr_pin);
		epld->irq = client->irq;

		LOG_INFO("IRQ number is %d\n", client->irq);
	}
	epld->irq = gpio_to_irq(epld->intr_pin);
#elif QCOM
	irq_gpio = of_get_named_gpio_flags(np, "epl,irq-gpio",
		0, &irq_gpio_flags);
	/*
	 *irq_gpio = ELAN_INT_PIN;
	 */
	epld->intr_pin = irq_gpio;
	if (epld->intr_pin < 0)
		goto initial_fail;

	if (gpio_is_valid(epld->intr_pin)) {
		err = gpio_request(epld->intr_pin, "epl_irq_gpio");
		if (err) {
			LOG_ERR("irq gpio request failed");
			goto initial_fail;
		}

		err = gpio_direction_input(epld->intr_pin);
		if (err) {
			LOG_ERR("set_direction for irq gpio failed\n");
			goto initial_fail;
		}
	}
	epld->irq = gpio_to_irq(epld->intr_pin);

	LOG_INFO("request gpio %d, irq %d\n", irq_gpio, epld->irq);

	epld->pinctrl = devm_pinctrl_get(&client->dev);
	if (IS_ERR_OR_NULL(epld->pinctrl)) {
		LOG_ERR("%s:Getting pinctrl handle failed \r\n", __func__);
		return -EINVAL;
	}

	epld->gpio_state_active = pinctrl_lookup_state(epld->pinctrl,
				  EPL_SENSOR_PINCTRL_STATE_DEFAULT);

	epld->gpio_state_suspend = pinctrl_lookup_state(epld->pinctrl,
				   EPL_SENSOR_PINCTRL_STATE_SLEEP);

	if (epld->pinctrl && epld->gpio_state_active) {
		err = pinctrl_select_state(epld->pinctrl,
			epld->gpio_state_active);
		if (err) {
			LOG_ERR("%s:pinctrl adopt active state, err = %d\r\n",
				__func__, err);
			return -EINVAL;
		}
		LOG_INFO("%s:pinctrl adopt active state\r\n", __func__);
	}

#elif LEADCORE
	/*
	*epld->intr_pin = ELAN_INT_PIN; //need setting
	*/
	epld->intr_pin = irq_to_gpio(client->irq); /*need confirm*/

	err = gpio_request(epld->intr_pin, "epl irq");
	/*
	 *err = gpio_request(epld->intr_pin, "epl irq");
	 */
	if (err < 0) {
		LOG_ERR("%s:Gpio request failed! \r\n", __func__);
		goto initial_fail;
	}
	gpio_direction_input(epld->intr_pin);

	epld->irq = gpio_to_irq(epld->intr_pin);
#elif MARVELL

	/*
	 *epld->intr_pin = ELAN_INT_PIN; //need setting
	*/
	epld->intr_pin = of_get_named_gpio_flags(np, "epl,irq-gpio", 0,
			 &irq_gpio_flags);
	if (client->irq <= 0) {
		LOG_ERR("client->irq(%d) Failed \r\n", client->irq);
		goto initial_fail;
	}
	gpio_request(epld->intr_pin, "epl irq");
	gpio_direction_input(epld->intr_pin);
	epld->irq = gpio_to_irq(epld->intr_pin);
#endif
	err = request_irq(epld->irq, epl_sensor_eint_func,
			  IRQF_TRIGGER_LOW, /*IRQF_TRIGGER_FALLING*/
			  client->dev.driver->name, epld);
	if (err < 0) {
		LOG_ERR("request irq pin %d fail for gpio\n", err);
		goto fail_free_intr_pin;
	}

	return err;

initial_fail:
fail_free_intr_pin:
	gpio_free(epld->intr_pin);
	/*
	 *    free_irq(epld->irq, epld);
	 */

	return err;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/

static ssize_t epl_sensor_show_reg(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{

	ssize_t len = 0;
	struct i2c_client *client = epl_sensor_obj->client;

	if (!epl_sensor_obj) {
		LOG_ERR("epl_obj is null!!\n");
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x00 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x00));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x01 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x01));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x02 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x02));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x03 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x03));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x04 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x04));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x05 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x05));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x06 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x06));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x07 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x07));
	if (epl_sensor.als.polling_mode == 0) {
		len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x08 value = 0x%x\n",
				i2c_smbus_read_byte_data(client, 0x08));
		len += snprintf(buf + len, PAGE_SIZE - len,
				"chip id REG 0x09 value = 0x%x\n",
				i2c_smbus_read_byte_data(client, 0x09));
		len += snprintf(buf + len, PAGE_SIZE - len,
				"chip id REG 0x0A value = 0x%x\n",
				i2c_smbus_read_byte_data(client, 0x0A));
		len += snprintf(buf + len, PAGE_SIZE - len,
				"chip id REG 0x0B value = 0x%x\n",
				i2c_smbus_read_byte_data(client, 0x0B));
	}
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x0C value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x0C));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x0D value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x0D));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x0E value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x0E));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x0F value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x0F));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x11 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x11));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x12 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x12));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x13 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x13));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x14 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x14));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x15 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x15));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x16 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x16));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x1B value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x1B));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x1C value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x1C));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x1D value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x1D));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x1E value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x1E));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x1F value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x1F));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x22 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x22));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x23 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x23));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x24 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x24));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0x25 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x25));
	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip id REG 0xFC value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0xFC));

	return len;

}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_show_status(struct device *dev,
				      struct device_attribute *attr,
				      char *buf)
{
	ssize_t len = 0;
	struct epl_sensor_priv *epld = epl_sensor_obj;
	bool enable_ps = epld->enable_pflag == 1 && epld->ps_suspend == 0;
	bool enable_als = epld->enable_lflag == 1 && epld->als_suspend == 0;

	if (!epl_sensor_obj) {
		LOG_ERR("epl_sensor_obj is null!!\n");
		return 0;
	}

	len += snprintf(buf + len, PAGE_SIZE - len,
			"chip is %s, ver is %s\n",
			EPL_DEV_NAME,	DRIVER_VERSION);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"als/ps polling is %d-%d\n",
			epl_sensor.als.polling_mode,
			epl_sensor.ps.polling_mode);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"wait = %d, mode = %d\n",
			epl_sensor.wait >> 4, epl_sensor.mode);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"interrupt control = %d\n",
			epl_sensor.interrupt_control >> 4);
	len += snprintf(buf + len, PAGE_SIZE - len,
			"frame time ps=%dms, als=%dms\n",
			ps_frame_time, als_frame_time);
	if (enable_ps) {
		len += snprintf(buf + len, PAGE_SIZE - len,
				"PS:\n");
		len += snprintf(buf + len, PAGE_SIZE - len,
				"INTEG = %d, gain = %d\n",
				epl_sensor.ps.integration_time >> 2,
				epl_sensor.ps.gain);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"ADC = %d, cycle = %d, ir drive = %d\n",
				epl_sensor.ps.adc >> 3, epl_sensor.ps.cycle,
				epl_sensor.ps.ir_drive);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"saturation = %d, int flag = %d\n",
				epl_sensor.ps.saturation >> 5,
				epl_sensor.ps.interrupt_flag >> 2);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"Thr(L/H) = (%d/%d)\n",
				epl_sensor.ps.low_threshold,
				epl_sensor.ps.high_threshold);
#if PS_DYN_K_ONE
		len += snprintf(buf + len, PAGE_SIZE - len,
				"Dyn thr(L/H) = (%ld/%ld)\n",
				(long)dynk_thd_low, (long)dynk_thd_high);
#endif
		len += snprintf(buf + len, PAGE_SIZE - len,
				"pals data = %d, data = %d\n",
				epl_sensor.ps.data.ir_data,
				epl_sensor.ps.data.data);
	}

	if (enable_als) {
		len += snprintf(buf + len, PAGE_SIZE - len, "ALS:\n");
		len += snprintf(buf + len, PAGE_SIZE - len,
				"INTEG = %d, gain = %d\n",
				epl_sensor.als.integration_time >> 2,
				epl_sensor.als.gain);
		len += snprintf(buf + len, PAGE_SIZE - len,
				"ADC = %d, cycle = %d\n",
				epl_sensor.als.adc >> 3, epl_sensor.als.cycle);
#if ALS_DYN_INTT
		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
			len += snprintf(buf + len, PAGE_SIZE - len,
					"c_gain = %d\n", c_gain);
			len += snprintf(buf + len, PAGE_SIZE - len,
				"dyn_intt_raw = %d, dynamic_intt_lux = %d\n",
				epl_sensor.als.dyn_intt_raw, dynamic_intt_lux);
		}
#endif
		if (epl_sensor.als.polling_mode == 0)
			len += snprintf(buf + len, PAGE_SIZE - len,
					"Thr(L/H) = (%d/%d)\n",
					epl_sensor.als.low_threshold,
					epl_sensor.als.high_threshold);

		len += snprintf(buf + len, PAGE_SIZE - len,
				"ch0 = %d, ch1 = %d\n",
				epl_sensor.als.data.channels[0],
				epl_sensor.als.data.channels[1]);
	}

	return len;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_als_enable(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	uint16_t mode = 0;
	int ret;

	LOG_FUN();

	/*ret = sscanf(buf, "%hu", &mode);*/
	ret = kstrtou16(buf, 10, &mode);
	if (ret)
		return ret;

	epl_sensor_enable_als(mode);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_ps_enable(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	uint16_t mode = 0;
	int ret;

	LOG_FUN();

	/*ret = sscanf(buf, "%hu", &mode);*/
	ret = kstrtou16(buf, 10, &mode);
	if (ret)
		return ret;

	epl_sensor_enable_ps(mode);

	return count;
}

/*----------------------------------------------------------------------------*/
#if ALSPS_FACTORY_CALI
static ssize_t epl_sensor_show_cal_raw(struct device *dev,
				       struct device_attribute *attr,
				       char *buf)
{
	ssize_t len = 0;
	u16 ch1 = 0;

	if (!epl_sensor_obj) {
		LOG_ERR("epl_sensor_obj is null!!\n");
		return 0;
	}

	switch (epl_sensor.mode) {
	case EPL_MODE_PS:
		ch1 = factory_ps_data();
		break;

	case EPL_MODE_ALS:
		ch1 = factory_als_data();
		break;
	}

	LOG_INFO("cal_raw = %d \r\n" , ch1);

	len += snprintf(buf + len, PAGE_SIZE - len, "%d \r\n", ch1);

	return  len;
}
#endif

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_threshold(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int hthr = 0, lthr = 0;
	int ret;

	if (!epld)	{
		LOG_ERR("epl_sensor_obj is null!!\n");
		return 0;
	}

	switch (epl_sensor.mode) {
	case EPL_MODE_PS:
		ret = sscanf(buf, "%d,%d", &lthr, &hthr);
		epl_sensor.ps.low_threshold = lthr;
		epl_sensor.ps.high_threshold = hthr;
		set_psensor_intr_threshold(epl_sensor.ps.low_threshold,
					   epl_sensor.ps.high_threshold);
		break;

	case EPL_MODE_ALS:
		ret = sscanf(buf, "%d,%d", &lthr, &hthr);
		epl_sensor.als.low_threshold = lthr;
		epl_sensor.als.high_threshold = hthr;
		set_lsensor_intr_threshold(epl_sensor.als.low_threshold,
					   epl_sensor.als.high_threshold);
		break;
	}

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_wait_time(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val;
	int ret;

	/*ret = sscanf(buf, "%d", &val);*/
	ret = kstrtoint(buf, 10, &val);
	if (ret)
		return ret;

	epl_sensor.wait = (val & 0xf) << 4;

	return count;
}
/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_gain(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int value = 0;
	int ret;

	LOG_FUN();

	/*ret = sscanf(buf, "%d", &value);*/
	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	value = value & 0x03;

	switch (epl_sensor.mode) {
	case EPL_MODE_PS: /*ps */
		epl_sensor.ps.gain = value;
		epl_sensor_I2C_Write(epld->client, 0x03,
		     epl_sensor.ps.integration_time | epl_sensor.ps.gain);
		break;

	case EPL_MODE_ALS: /*als */
		epl_sensor.als.gain = value;
		epl_sensor_I2C_Write(epld->client, 0x01,
		     epl_sensor.als.integration_time | epl_sensor.als.gain);
		break;
	}

	epl_sensor_update_mode(epld->client);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_mode(struct device *dev,
				     struct device_attribute *attr,
				     const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int value = 0;
	int ret;

	LOG_FUN();

	epld->enable_pflag = 0;
	epld->enable_lflag = 0;

	/*ret = sscanf(buf, "%d", &value);*/
	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	switch (value) {
	case 0:
		epl_sensor.mode = EPL_MODE_IDLE;
		break;

	case 1:
		epld->enable_lflag = 1;
		epl_sensor.mode = EPL_MODE_ALS;
		break;

	case 2:
		epld->enable_pflag = 1;
		epl_sensor.mode = EPL_MODE_PS;
		break;

	case 3:
		epld->enable_lflag = 1;
		epld->enable_pflag = 1;
		epl_sensor.mode = EPL_MODE_ALS_PS;
		break;
	}

	epl_sensor_update_mode(epld->client);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_ir_mode(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int value = 0;
	int ret;
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	/*ret = sscanf(buf, "%d", &value);*/
	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	switch (epl_sensor.mode) {
	case EPL_MODE_PS: /*ps */
		switch (value) {
		case 0:
			epl_sensor.ps.ir_mode = EPL_IR_MODE_CURRENT;
			break;

		case 1:
			epl_sensor.ps.ir_mode = EPL_IR_MODE_VOLTAGE;
			break;
		}

		epl_sensor_I2C_Write(epld->client, 0x05,
			epl_sensor.ps.ir_on_control |
			epl_sensor.ps.ir_mode | epl_sensor.ps.ir_drive);
		break;
	}

	epl_sensor_I2C_Write(epld->client, 0x00,
			epl_sensor.wait | epl_sensor.mode);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_ir_contrl(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	uint8_t  data;
	int ret;

	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	/*ret = sscanf(buf, "%d", &value);*/
	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	switch (epl_sensor.mode) {
	case EPL_MODE_PS: /*ps */
		switch (value) {
		case 0:
			epl_sensor.ps.ir_on_control = EPL_IR_ON_CTRL_OFF;
			break;

		case 1:
			epl_sensor.ps.ir_on_control = EPL_IR_ON_CTRL_ON;
			break;
		}

		data = epl_sensor.ps.ir_on_control | epl_sensor.ps.ir_mode |
		       epl_sensor.ps.ir_drive;
		LOG_INFO("[%s]: 0x05 = 0x%x\n", __func__, data);

		epl_sensor_I2C_Write(epld->client, 0x05,
			     epl_sensor.ps.ir_on_control |
			     epl_sensor.ps.ir_mode | epl_sensor.ps.ir_drive);
		break;
	}

	epl_sensor_I2C_Write(epld->client, 0x00,
			epl_sensor.wait | epl_sensor.mode);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_ir_drive(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	int ret;
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	/*ret = sscanf(buf, "%d", &value);*/
	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	switch (epl_sensor.mode) {
	case EPL_MODE_PS:
		epl_sensor.ps.ir_drive = (value & 0x03);
		epl_sensor_I2C_Write(epld->client, 0x05,
			     epl_sensor.ps.ir_on_control |
			     epl_sensor.ps.ir_mode | epl_sensor.ps.ir_drive);
		break;
	}

	epl_sensor_I2C_Write(epld->client, 0x00,
			epl_sensor.wait | epl_sensor.mode);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_interrupt_type(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	int ret;

	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	/*ret = sscanf(buf, "%d", &value);*/
	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	switch (epl_sensor.mode) {
	case EPL_MODE_PS: /*ps */
		if (!epl_sensor.ps.polling_mode) {
			epl_sensor.ps.interrupt_type = value & 0x03;
			epl_sensor_I2C_Write(epld->client, 0x06,
				epl_sensor.interrupt_control |
				epl_sensor.ps.persist |
				epl_sensor.ps.interrupt_type);
			LOG_INFO("[%s]: 0x06 = 0x%x\n", __func__,
				 epl_sensor.interrupt_control |
				 epl_sensor.ps.persist |
				 epl_sensor.ps.interrupt_type);
		}
		break;

	case EPL_MODE_ALS: /*als */
		if (!epl_sensor.als.polling_mode) {
			epl_sensor.als.interrupt_type = value & 0x03;
			epl_sensor_I2C_Write(epld->client, 0x07,
				epl_sensor.als.interrupt_channel_select |
				epl_sensor.als.persist |
				epl_sensor.als.interrupt_type);
			LOG_INFO("[%s]: 0x07 = 0x%x\n", __func__,
				epl_sensor.als.interrupt_channel_select |
				epl_sensor.als.persist |
				epl_sensor.als.interrupt_type);
		}
		break;
	}

	return count;
}

/*----------------------------------------------------------------------------*/

static ssize_t epl_sensor_store_ps_polling_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int polling_mode = 0;
	int ret;

	/*ret = sscanf(buf, "%d", &polling_mode);*/
	ret = kstrtoint(buf, 10, &polling_mode);
	if (ret)
		return ret;

	epl_sensor.ps.polling_mode = polling_mode;

	set_als_ps_intr_type(epld->client, epl_sensor.ps.polling_mode,
			epl_sensor.als.polling_mode);
	epl_sensor_I2C_Write(epld->client, 0x06,
			epl_sensor.interrupt_control | epl_sensor.ps.persist |
			epl_sensor.ps.interrupt_type);
	epl_sensor_I2C_Write(epld->client, 0x07,
			epl_sensor.als.interrupt_channel_select |
			epl_sensor.als.persist |
			epl_sensor.als.interrupt_type);

	epl_sensor_fast_update(epld->client);
	epl_sensor_update_mode(epld->client);

	return count;
}

static ssize_t epl_sensor_store_als_polling_mode(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int polling_mode = 0;
	int ret;

	/*ret = sscanf(buf, "%d", &polling_mode);*/
	ret = kstrtoint(buf, 10, &polling_mode);
	if (ret)
		return ret;
	epl_sensor.als.polling_mode = polling_mode;

	set_als_ps_intr_type(epld->client, epl_sensor.ps.polling_mode,
		     epl_sensor.als.polling_mode);
	epl_sensor_I2C_Write(epld->client, 0x06,
		     epl_sensor.interrupt_control | epl_sensor.ps.persist |
		     epl_sensor.ps.interrupt_type);
	epl_sensor_I2C_Write(epld->client, 0x07,
		     epl_sensor.als.interrupt_channel_select |
		     epl_sensor.als.persist |
		     epl_sensor.als.interrupt_type);

	/*
	 *update als intr table
	 */
	if (epl_sensor.als.polling_mode == 0)
		als_intr_update_table(epld);

	epl_sensor_fast_update(epld->client);
	epl_sensor_update_mode(epld->client);

	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_integration(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	int ret;
#if ALS_DYN_INTT
	int value1 = 0;
#endif
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	/*
	 *sscanf(buf, "%d",&value);
	 */

	switch (epl_sensor.mode) {
	case EPL_MODE_PS: /*ps */
		/*ret = sscanf(buf, "%d", &value);*/
		ret = kstrtoint(buf, 10, &value);
		if (ret)
			return ret;

		epl_sensor.ps.integration_time = (value & 0xf) << 2;
		epl_sensor_I2C_Write(epld->client, 0x03,
			epl_sensor.ps.integration_time | epl_sensor.ps.gain);
		epl_sensor_I2C_Read(epld->client, 0x03, 1);
		LOG_INFO("[%s]: 0x03 = 0x%x (0x%x)\n", __func__,
			epl_sensor.ps.integration_time | epl_sensor.ps.gain,
			gRawData.raw_bytes[0]);
		break;

	case EPL_MODE_ALS: /*als */
#if ALS_DYN_INTT
		if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
			ret = sscanf(buf, "%d,%d", &value, &value1);

			als_dynamic_intt_intt[0] = (value & 0xf) << 2;
			als_dynamic_intt_value[0] = als_intt_value[value];

			als_dynamic_intt_intt[1] = (value1 & 0xf) << 2;
			als_dynamic_intt_value[1] = als_intt_value[value1];
			LOG_INFO("[%s]: als_dynamic_intt_value=%d,%d \r\n",
				__func__, als_dynamic_intt_value[0],
				 als_dynamic_intt_value[1]);

			dynamic_intt_idx = dynamic_intt_init_idx;
			epl_sensor.als.integration_time =
				als_dynamic_intt_intt[dynamic_intt_idx];
			epl_sensor.als.gain =
				als_dynamic_intt_gain[dynamic_intt_idx];
			dynamic_intt_high_thr =
				als_dynamic_intt_high_thr[dynamic_intt_idx];
			dynamic_intt_low_thr =
				als_dynamic_intt_low_thr[dynamic_intt_idx];
		} else
#endif
		{
			/*ret = sscanf(buf, "%d", &value);*/
			ret = kstrtoint(buf, 10, &value);
			if (ret)
				return ret;
			epl_sensor.als.integration_time = (value & 0xf) << 2;
			epl_sensor_I2C_Write(epld->client, 0x01,
				epl_sensor.als.integration_time |
				epl_sensor.als.gain);
			epl_sensor_I2C_Read(epld->client, 0x01, 1);
			LOG_INFO("[%s]: 0x01 = 0x%x (0x%x)\n", __func__,
				 epl_sensor.als.integration_time |
				 epl_sensor.als.gain, gRawData.raw_bytes[0]);
		}
		break;
	}

	epl_sensor_update_mode(epld->client);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_adc(struct device *dev,
				    struct device_attribute *attr,
				    const char *buf, size_t count)
{
	int value = 0;
	int ret;
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	/*ret = sscanf(buf, "%d", &value);*/
	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	switch (epl_sensor.mode) {
	case EPL_MODE_PS: /*ps */
		epl_sensor.ps.adc = (value & 0x3) << 3;
		epl_sensor_I2C_Write(epld->client, 0x04,
				     epl_sensor.ps.adc | epl_sensor.ps.cycle);
		epl_sensor_I2C_Read(epld->client, 0x04, 1);
		LOG_INFO("[%s]:0x04 = 0x%x (0x%x)\n", __func__,
			 epl_sensor.ps.adc | epl_sensor.ps.cycle,
			 gRawData.raw_bytes[0]);
		break;

	case EPL_MODE_ALS: /*als */
		epl_sensor.als.adc = (value & 0x3) << 3;
		epl_sensor_I2C_Write(epld->client, 0x02,
				     epl_sensor.als.adc | epl_sensor.als.cycle);
		epl_sensor_I2C_Read(epld->client, 0x02, 1);
		LOG_INFO("[%s]:0x02 = 0x%x (0x%x)\n", __func__,
			 epl_sensor.als.adc | epl_sensor.als.cycle,
			 gRawData.raw_bytes[0]);
		break;
	}

	epl_sensor_update_mode(epld->client);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_cycle(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t count)
{
	int value = 0;
	int ret;

	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	/*ret = sscanf(buf, "%d", &value);*/
	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	switch (epl_sensor.mode) {
	case EPL_MODE_PS: /*ps */
		epl_sensor.ps.cycle = (value & 0x7);
		epl_sensor_I2C_Write(epld->client, 0x04,
				     epl_sensor.ps.adc | epl_sensor.ps.cycle);
		LOG_INFO("[%s]:0x04 = 0x%x (0x%x)\n", __func__,
			 epl_sensor.ps.adc | epl_sensor.ps.cycle,
			 gRawData.raw_bytes[0]);
		break;

	case EPL_MODE_ALS: /*als */
		epl_sensor.als.cycle = (value & 0x7);
		epl_sensor_I2C_Write(epld->client, 0x02,
				     epl_sensor.als.adc | epl_sensor.als.cycle);
		LOG_INFO("[%s]:0x02 = 0x%x (0x%x)\n", __func__,
			 epl_sensor.als.adc | epl_sensor.als.cycle,
			 gRawData.raw_bytes[0]);
		break;
	}

	epl_sensor_update_mode(epld->client);
	return count;
}

/*----------------------------------------------------------------------------*/
static ssize_t epl_sensor_store_als_report_type(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int value = 0;
	int ret;

	LOG_FUN();

	/*ret = sscanf(buf, "%d", &value);*/
	ret = kstrtoint(buf, 10, &value);
	if (ret)
		return ret;

	epl_sensor.als.report_type = value & 0xf;

	return count;
}
/*----------------------------------------------------------------------------*/
#if ALSPS_FACTORY_CALI
static ssize_t epl_sensor_store_ps_w_calfile(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int ps_hthr = 0, ps_lthr = 0, ps_cancelation = 0;
	int ps_cal_len = 0;
	int ret;
	char ps_calibration[20];

	LOG_FUN();

	if (!epl_sensor_obj) {
		LOG_ERR("epl_obj is null!!\n");
		return 0;
	}
	ret = sscanf(buf, "%d,%d,%d", &ps_cancelation, &ps_hthr, &ps_lthr);

	ps_cal_len = snprintf(ps_calibration, PAGE_SIZE,
		"%d,%d,%d",  ps_cancelation, ps_hthr, ps_lthr);

	write_factory_calibration(epld, ps_calibration, ps_cal_len);
	return count;
}
#endif
/*----------------------------------------------------------------------------*/

static ssize_t epl_sensor_store_reg_write(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int reg;
	int data;
	int ret;

	LOG_FUN();

	ret = sscanf(buf, "%x,%x", &reg, &data);

	LOG_INFO("[%s]: reg=0x%x, data=0x%x", __func__, reg, data);
	epl_sensor_I2C_Write(epld->client, reg, data);

	return count;
}

static ssize_t epl_sensor_store_unlock(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int mode;
	int ret;

	LOG_FUN();

	/*ret = sscanf(buf, "%d", &mode);*/
	ret = kstrtoint(buf, 10, &mode);
	if (ret)
		return ret;

	LOG_INFO("mode = %d \r\n", mode);

	switch (mode) {
	case 0: /*ps */
		/*
		 *PS unlock and run
		 */
		epl_sensor.ps.compare_reset = EPL_CMP_RUN;
		epl_sensor.ps.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x1b,
			epl_sensor.ps.compare_reset | epl_sensor.ps.lock);
		break;

	case 1: /*als */
		/*
		 *ALS unlock and run
		 */
		epl_sensor.als.compare_reset = EPL_CMP_RUN;
		epl_sensor.als.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x12,
			epl_sensor.als.compare_reset | epl_sensor.als.lock);
		break;

	case 2: /*als unlock and reset */
		epl_sensor.als.compare_reset = EPL_CMP_RESET;
		epl_sensor.als.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x12,
			epl_sensor.als.compare_reset | epl_sensor.als.lock);
		break;

	case 3: /*ps+als */
		/*
		 *PS unlock and run
		 */
		epl_sensor.ps.compare_reset = EPL_CMP_RUN;
		epl_sensor.ps.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x1b,
			epl_sensor.ps.compare_reset | epl_sensor.ps.lock);

		/*
		 *ALS unlock and run
		 */
		epl_sensor.als.compare_reset = EPL_CMP_RUN;
		epl_sensor.als.lock = EPL_UN_LOCK;
		epl_sensor_I2C_Write(epld->client, 0x12,
			epl_sensor.als.compare_reset | epl_sensor.als.lock);
		break;
	}
	/*double check PS or ALS lock*/

	return count;
}

static ssize_t epl_sensor_store_als_ch_sel(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int ch_sel;
	int ret;

	LOG_FUN();

	/*ret = sscanf(buf, "%d", &ch_sel);*/
	ret = kstrtoint(buf, 10, &ch_sel);
	if (ret)
		return ret;

	LOG_INFO("channel selection = %d \r\n", ch_sel);
	switch (ch_sel) {
	case 0: /*ch0 */
		epl_sensor.als.interrupt_channel_select = EPL_ALS_INT_CHSEL_0;
		break;

	case 1: /*ch1 */
		epl_sensor.als.interrupt_channel_select = EPL_ALS_INT_CHSEL_1;
		break;
	}
	epl_sensor_I2C_Write(epld->client, 0x07,
		     epl_sensor.als.interrupt_channel_select |
		     epl_sensor.als.persist |
		     epl_sensor.als.interrupt_type);

	epl_sensor_update_mode(epld->client);

	return count;
}

static ssize_t epl_sensor_store_ps_cancelation(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int cancelation;
	int ret;

	LOG_FUN();

	/*ret = sscanf(buf, "%d", &cancelation);	*/
	ret = kstrtoint(buf, 10, &cancelation);
	if (ret)
		return ret;

	epl_sensor.ps.cancelation = cancelation;

	LOG_INFO("epl_sensor.ps.cancelation = %d \r\n",
			epl_sensor.ps.cancelation);

	epl_sensor_I2C_Write(epld->client, 0x22,
			     (u8)(epl_sensor.ps.cancelation & 0xff));
	epl_sensor_I2C_Write(epld->client, 0x23,
			     (u8)((epl_sensor.ps.cancelation & 0xff00) >> 8));

	return count;
}

static ssize_t epl_sensor_show_ps_polling(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u16 *tmp = (u16 *)buf;

	tmp[0] = epl_sensor.ps.polling_mode;
	return 2;
}

static ssize_t epl_sensor_show_als_polling(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u16 *tmp = (u16 *)buf;

	tmp[0] = epl_sensor.als.polling_mode;
	return 2;
}

#if ALSPS_FACTORY_CALI
static ssize_t epl_sensor_show_ps_run_cali(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	ssize_t len = 0;
	int ret;

	LOG_FUN();

	ret = epl_run_ps_calibration(epld);

	len += snprintf(buf + len, PAGE_SIZE - len, "ret = %d\r\n", ret);

	return len;
}

static ssize_t epl_sensor_show_pdata(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	ssize_t len = 0;
	int ps_raw = 0;

	ps_raw = factory_ps_data();
	LOG_INFO("[%s]: ps_raw=%d \r\n", __func__, ps_raw);

	len += snprintf(buf + len, PAGE_SIZE - len, "%d", ps_raw);

	return len;
}

static ssize_t epl_sensor_show_als_data(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t len = 0;
	u16 als_raw = 0;

	als_raw = factory_als_data();
	LOG_INFO("[%s]: als_raw=%d \r\n", __func__, als_raw);

	len += snprintf(buf + len, PAGE_SIZE - len, "%d", als_raw);
	return len;
}
#endif

static ssize_t epl_sensor_show_renvo(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	ssize_t len = 0;

	LOG_FUN();
	LOG_INFO("gRawData.renvo=0x%x \r\n", epl_sensor.revno);

	len += snprintf(buf + len, PAGE_SIZE - len, "%x", epl_sensor.revno);

	return len;
}

static ssize_t epl_sensor_show_delay_ms(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	ssize_t len = 0;

	LOG_FUN();

	return len;
}

static ssize_t epl_sensor_store_delay_ms(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	LOG_FUN();

	return count;
}

#if ALS_DYN_INTT
static ssize_t epl_sensor_store_c_gain(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int als_c_gian;
	int ret;

	/*ret = sscanf(buf, "%d", &als_c_gian);*/
	ret = kstrtoint(buf, 10, &als_c_gian);
	if (ret)
		return ret;

	c_gain = als_c_gian;
	LOG_INFO("c_gain = %d \r\n", c_gain);

	return count;
}
#endif

/*----------------------------------------------------------------------------*/
/*CTS --> S_IWUSR | S_IRUGO*/
static DEVICE_ATTR(elan_status, S_IRUSR | S_IRUGO, epl_sensor_show_status,
		   NULL);
static DEVICE_ATTR(elan_reg, S_IRUSR | S_IRUGO, epl_sensor_show_reg, NULL);
static DEVICE_ATTR(mode, S_IWUSR | S_IRUGO, NULL, epl_sensor_store_mode);
static DEVICE_ATTR(wait_time, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_wait_time);
static DEVICE_ATTR(set_threshold, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_threshold);
static DEVICE_ATTR(als_enable, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_als_enable);
static DEVICE_ATTR(als_report_type, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_als_report_type);
static DEVICE_ATTR(enable_sar, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_ps_enable);
static DEVICE_ATTR(ps_polling_mode, S_IWUSR | S_IRUGO,
		   epl_sensor_show_ps_polling,
		   epl_sensor_store_ps_polling_mode);
static DEVICE_ATTR(als_polling_mode, S_IWUSR | S_IRUGO,
		   epl_sensor_show_als_polling,
		   epl_sensor_store_als_polling_mode);
static DEVICE_ATTR(gain, S_IWUSR | S_IRUGO, NULL, epl_sensor_store_gain);
static DEVICE_ATTR(ir_mode, S_IWUSR | S_IRUGO, NULL, epl_sensor_store_ir_mode);
static DEVICE_ATTR(ir_drive, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_ir_drive);
static DEVICE_ATTR(ir_on, S_IWUSR | S_IRUGO, NULL, epl_sensor_store_ir_contrl);
static DEVICE_ATTR(interrupt_type, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_interrupt_type);
static DEVICE_ATTR(integration, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_integration);
static DEVICE_ATTR(adc, S_IWUSR | S_IRUGO, NULL, epl_sensor_store_adc);
static DEVICE_ATTR(cycle, S_IWUSR | S_IRUGO, NULL, epl_sensor_store_cycle);
static DEVICE_ATTR(i2c_w, S_IWUSR | S_IRUGO, NULL, epl_sensor_store_reg_write);
static DEVICE_ATTR(unlock, S_IWUSR | S_IRUGO, NULL, epl_sensor_store_unlock);
static DEVICE_ATTR(als_ch, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_als_ch_sel);
static DEVICE_ATTR(ps_cancel, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_ps_cancelation);
#if ALSPS_FACTORY_CALI
static DEVICE_ATTR(cal_raw, S_IRUSR | S_IRUGO, epl_sensor_show_cal_raw, NULL);
static DEVICE_ATTR(ps_w_calfile, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_ps_w_calfile);
static DEVICE_ATTR(run_ps_cali, S_IRUSR | S_IRUGO, epl_sensor_show_ps_run_cali,
		   NULL);
static DEVICE_ATTR(pdata, S_IRUSR | S_IRUGO, epl_sensor_show_pdata, NULL);
static DEVICE_ATTR(als_data, S_IRUSR | S_IRUGO, epl_sensor_show_als_data, NULL);
#endif
static DEVICE_ATTR(elan_renvo, S_IRUSR | S_IRUGO, epl_sensor_show_renvo, NULL);
static DEVICE_ATTR(set_delay_ms, S_IWUSR | S_IRUGO, epl_sensor_show_delay_ms,
		   epl_sensor_store_delay_ms);
#if ALS_DYN_INTT
static DEVICE_ATTR(als_dyn_c_gain, S_IWUSR | S_IRUGO, NULL,
		   epl_sensor_store_c_gain);
#endif
/*----------------------------------------------------------------------------*/
static struct attribute *epl_sensor_attr_list[] = {
	&dev_attr_elan_status.attr,
	&dev_attr_elan_reg.attr,
	&dev_attr_als_enable.attr,
	&dev_attr_enable_sar.attr,
	&dev_attr_set_threshold.attr,
	&dev_attr_wait_time.attr,
	&dev_attr_gain.attr,
	&dev_attr_mode.attr,
	&dev_attr_ir_mode.attr,
	&dev_attr_ir_drive.attr,
	&dev_attr_ir_on.attr,
	&dev_attr_interrupt_type.attr,
	&dev_attr_integration.attr,
	&dev_attr_adc.attr,
	&dev_attr_cycle.attr,
	&dev_attr_als_report_type.attr,
	&dev_attr_ps_polling_mode.attr,
	&dev_attr_als_polling_mode.attr,
	&dev_attr_i2c_w.attr,
	&dev_attr_unlock.attr,
	&dev_attr_als_ch.attr,
	&dev_attr_ps_cancel.attr,
#if ALSPS_FACTORY_CALI
	&dev_attr_cal_raw.attr,
	&dev_attr_ps_w_calfile.attr,
	&dev_attr_run_ps_cali.attr,
	&dev_attr_pdata.attr,
	&dev_attr_als_data.attr,
#endif
	&dev_attr_elan_renvo.attr,
	&dev_attr_set_delay_ms.attr,
#if ALS_DYN_INTT
	&dev_attr_als_dyn_c_gain.attr,
#endif
	NULL,
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct attribute_group epl_sensor_attr_group = {
	.attrs = epl_sensor_attr_list,
};
/*----------------------------------------------------------------------------*/
#if !SPREAD/*SPREAD start.....*/
/*----------------------------------------------------------------------------*/
static int epl_sensor_als_open(struct inode *inode, struct file *file)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	if (epld->als_opened)
		return -EBUSY;

	epld->als_opened = 1;

	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int epl_sensor_als_read(struct file *file, char __user *buffer,
			       size_t count,
			       loff_t *ppos)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;
	int buf[1];

	if (epld->read_flag == 1) {
		buf[0] = epl_sensor.als.data.channels[1];
		if (copy_to_user(buffer, &buf , sizeof(buf)))
			return 0;
		epld->read_flag = 0;
		return 12;
	} else {
		return 0;
	}
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int epl_sensor_als_release(struct inode *inode, struct file *file)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	epld->als_opened = 0;

	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static long epl_sensor_als_ioctl(struct file *file, unsigned int cmd,
				 unsigned long arg)
{
	int flag;
#if ALSPS_FACTORY_CALI
	unsigned long buf[1];
#endif
	struct epl_sensor_priv *epld = epl_sensor_obj;
	void __user *argp = (void __user *)arg;

	LOG_INFO("als io ctrl cmd %d\n", _IOC_NR(cmd));

	switch (cmd) {
	case ELAN_EPL8800_IOCTL_GET_LFLAG:
		LOG_INFO("elan ambient-light IOCTL Sensor get lflag\n");
		flag = epld->enable_lflag;
		if (copy_to_user(argp, &flag, sizeof(flag)))
			return -EFAULT;

		LOG_INFO("elan ambient-light Sensor get lflag %d\n", flag);
		break;

	case ELAN_EPL8800_IOCTL_ENABLE_LFLAG:
#if LEADCORE
	case LIGHT_SET_ENALBE:
#endif
		LOG_INFO("elan ambient-light IOCTL Sensor set lflag\n");
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;

		epl_sensor_enable_als(flag);

		LOG_INFO("elan ambient-light Sensor set lflag %d\n", flag);
		break;

#if ALSPS_FACTORY_CALI
	case ELAN_EPL8800_IOCTL_GETDATA:
		buf[0] = factory_als_data();
		if (copy_to_user(argp, &buf , sizeof(buf)))
			return -EFAULT;
		break;
#endif

#if LEADCORE
	case LIGHT_SET_DELAY:
		if (arg > LIGHT_MAX_DELAY)
			arg = LIGHT_MAX_DELAY;
		else if (arg < LIGHT_MIN_DELAY)
			arg = LIGHT_MIN_DELAY;
		LOG_INFO("LIGHT_SET_DELAY--%d\r\n", (int)arg);
		polling_time = arg;
		break;
#endif
	default:
		LOG_ERR("invalid cmd %d\n", _IOC_NR(cmd));
		return -EINVAL;
	}

	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static const struct file_operations epl_sensor_als_fops = {
	.owner = THIS_MODULE,
	.open = epl_sensor_als_open,
	.read = epl_sensor_als_read,
	.release = epl_sensor_als_release,
	.unlocked_ioctl = epl_sensor_als_ioctl
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct miscdevice epl_sensor_als_device = {
	.minor = MISC_DYNAMIC_MINOR,
#if LEADCORE
	.name = "light",
#else
	.name = "elan_als",
#endif
	.fops = &epl_sensor_als_fops
};
/*----------------------------------------------------------------------------*/
#endif /*SPREAD end.........*/

/*----------------------------------------------------------------------------*/
static int epl_sensor_ps_open(struct inode *inode, struct file *file)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	if (epld->ps_opened)
		return -EBUSY;

	epld->ps_opened = 1;

	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int epl_sensor_ps_release(struct inode *inode, struct file *file)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	epld->ps_opened = 0;

	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static long epl_sensor_ps_ioctl(struct file *file, unsigned int cmd,
				unsigned long arg)
{
#if ALSPS_FACTORY_CALI
	int value;
#endif
	int flag;
	struct epl_sensor_priv *epld = epl_sensor_obj;
	void __user *argp = (void __user *)arg;

	LOG_INFO("ps io ctrl cmd %d\n", _IOC_NR(cmd));

	/*
	 *ioctl message handle must define
	 *by android sensor library (case by case)
	 */
	switch (cmd) {
	case ELAN_EPL8800_IOCTL_GET_PFLAG:
	case LTR_IOCTL_GET_PFLAG:
		LOG_INFO("elan Proximity Sensor IOCTL get pflag\n");
		flag = epld->enable_pflag;
		if (copy_to_user(argp, &flag, sizeof(flag)))
			return -EFAULT;

		LOG_INFO("elan Proximity Sensor get pflag %d\n", flag);
		break;

	case ELAN_EPL8800_IOCTL_ENABLE_PFLAG:
#if LEADCORE
	case PROXIMITY_SET_ENALBE:
#endif
	case LTR_IOCTL_SET_PFLAG:
		LOG_INFO("elan Proximity IOCTL Sensor set pflag\n");
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;

		epl_sensor_enable_ps(flag);

		LOG_INFO("elan Proximity Sensor set pflag %d\n", flag);
		break;

#if ALSPS_FACTORY_CALI
	case ELAN_EPL8800_IOCTL_GETDATA:
		value = factory_ps_data();

		LOG_INFO("elan proximity Sensor get data (%d)\n", value);

		if (copy_to_user(argp, &value , sizeof(value)))
			return -EFAULT;
		break;
#endif

#if LEADCORE
	case PROXIMITY_SET_DELAY:
		if (arg > PROXIMITY_MAX_DELAY)
			arg = PROXIMITY_MAX_DELAY;
		else if (arg < PROXIMITY_MIN_DELAY)
			arg = PROXIMITY_MIN_DELAY;
		LOG_INFO("PROXIMITY_SET_DELAY--%d\r\n", (int)arg);
		polling_time = arg;
		break;
#endif

#if SPREAD  /*SPREAD start.....*/
	case ELAN_EPL8800_IOCTL_GET_LFLAG:
		LOG_INFO("elan ambient-light IOCTL Sensor get lflag\n");
		flag = epld->enable_lflag;
		if (copy_to_user(argp, &flag, sizeof(flag)))
			return -EFAULT;

		LOG_INFO("elan ambient-light Sensor get lflag %d\n", flag);
		break;

	case ELAN_EPL8800_IOCTL_ENABLE_LFLAG:
		LOG_INFO("elan ambient-light IOCTL Sensor set lflag\n");
		if (copy_from_user(&flag, argp, sizeof(flag)))
			return -EFAULT;
		if (flag < 0 || flag > 1)
			return -EINVAL;

		epl_sensor_enable_als(flag);

		LOG_INFO("elan ambient-light Sensor set lflag %d\n", flag);
		break;

#endif  /*SPREAD end......*/
	default:
		LOG_ERR("invalid cmd %d\n", _IOC_NR(cmd));
		return -EINVAL;
	}

	return 0;

}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static const struct file_operations epl_sensor_ps_fops = {
	.owner = THIS_MODULE,
	.open = epl_sensor_ps_open,
	.release = epl_sensor_ps_release,
	.unlocked_ioctl = epl_sensor_ps_ioctl
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct miscdevice epl_sensor_ps_device = {
	.minor = MISC_DYNAMIC_MINOR,
#if LEADCORE
	.name = "proximity",
#else
	.name = "elan_ps",
#endif
	.fops = &epl_sensor_ps_fops
};

#if !SPREAD /*SPREAD start.....*/
/*----------------------------------------------------------------------------*/
static ssize_t light_enable_show(struct device *dev,
				 struct device_attribute *attr,
				 char *buf)
{
	struct epl_sensor_priv *epld  = epl_sensor_obj;

	LOG_INFO("%s: ALS_status=%d\n", __func__, epld->enable_lflag);
	return snprintf(buf, PAGE_SIZE, "%d\n", epld->enable_lflag);
}

static ssize_t light_enable_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t size)
{
	uint16_t als_enable = 0;
	int ret;

	LOG_INFO("light_enable_store: enable=%s\n", buf);
	/*t = sscanf(buf, "%hu", &als_enable);*/
	ret = kstrtou16(buf, 10, &als_enable);
	if (ret)
		return ret;
	epl_sensor_enable_als(als_enable);

	return size;
}
/*----------------------------------------------------------------------------*/
#if MARVELL
static struct device_attribute dev_attr_light_enable =
	__ATTR(enable1, S_IRWXUGO,
	       light_enable_show, light_enable_store);
#else
static struct device_attribute dev_attr_light_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR,
	       light_enable_show, light_enable_store);
#endif
static struct attribute *light_sysfs_attrs[] = {
	&dev_attr_light_enable.attr,
	NULL
};

static struct attribute_group light_attribute_group = {
	.attrs = light_sysfs_attrs,
};
/*----------------------------------------------------------------------------*/
static int epl_sensor_setup_lsensor(struct epl_sensor_priv *epld)
{
	int err = 0;

	LOG_INFO("epl_sensor_setup_lsensor enter.\n");

	epld->als_input_dev = input_allocate_device();
	if (!epld->als_input_dev) {
		LOG_ERR("could not allocate ls input device\n");
		return -ENOMEM;
	}
	epld->als_input_dev->name = ElanALsensorName;
	set_bit(EV_ABS, epld->als_input_dev->evbit);
	input_set_abs_params(epld->als_input_dev, ABS_MISC, 0, 9, 0, 0);

	err = input_register_device(epld->als_input_dev);
	if (err < 0) {
		LOG_ERR("can not register ls input device\n");
		goto err_free_ls_input_device;
	}

	err = misc_register(&epl_sensor_als_device);
	if (err < 0) {
		LOG_ERR("can not register ls misc device\n");
		goto err_unregister_ls_input_device;
	}

	err = sysfs_create_group(&epld->als_input_dev->dev.kobj,
				 &light_attribute_group);

	if (err) {
		pr_err("%s: could not create sysfs group\n", __func__);
		goto err_free_ls_input_device;
	}

	return err;

err_unregister_ls_input_device:
	input_unregister_device(epld->als_input_dev);
err_free_ls_input_device:
	input_free_device(epld->als_input_dev);

	return err;
}
#endif /*SPREAD end.....*/
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static ssize_t proximity_enable_show(struct device *dev,
				     struct device_attribute *attr,
				     char *buf)
{
	struct epl_sensor_priv *epld  = epl_sensor_obj;

	LOG_INFO("%s: PS status=%d\n", __func__, epld->enable_pflag);
	return snprintf(buf, PAGE_SIZE, "%d\n", epld->enable_pflag);
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static ssize_t proximity_enable_store(struct device *dev,
				      struct device_attribute *attr,
				      const char *buf, size_t size)
{
	uint16_t ps_enable = 0;
	int ret;

	LOG_INFO("proximity_enable_store: enable=%s\n", buf);

	/*ret = sscanf(buf, "%hu", &ps_enable);*/
	ret = kstrtou16(buf, 10, &ps_enable);
	if (ret)
		return ret;

	epl_sensor_enable_ps(ps_enable);

	return size;
}
/*----------------------------------------------------------------------------*/
#if MARVELL
static struct device_attribute dev_attr_psensor_enable =
	__ATTR(enable1, S_IRWXUGO,
	       proximity_enable_show, proximity_enable_store);
#else
static struct device_attribute dev_attr_psensor_enable =
	__ATTR(enable, S_IRUGO | S_IWUSR,
	       proximity_enable_show, proximity_enable_store);
#endif
static struct attribute *proximity_sysfs_attrs[] = {
	&dev_attr_psensor_enable.attr,
	NULL
};

static struct attribute_group proximity_attribute_group = {
	.attrs = proximity_sysfs_attrs,
};
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int epl_sensor_setup_psensor(struct epl_sensor_priv *epld)
{
	int err = 0;

	LOG_INFO("epl_sensor_setup_psensor enter.\n");

	epld->ps_input_dev = input_allocate_device();
	if (!epld->ps_input_dev) {
		LOG_ERR("could not allocate ps input device\n");
		return -ENOMEM;
	}
	epld->ps_input_dev->name = ElanPsensorName;

	set_bit(EV_ABS, epld->ps_input_dev->evbit);
	input_set_abs_params(epld->ps_input_dev, ABS_DISTANCE, 0, 10, 0, 0);
#if SPREAD
	set_bit(EV_ABS, epld->ps_input_dev->evbit);
	input_set_abs_params(epld->ps_input_dev, ABS_MISC, 0, 9, 0, 0);
#endif
	err = input_register_device(epld->ps_input_dev);
	if (err < 0) {
		LOG_ERR("could not register ps input device\n");
		goto err_free_ps_input_device;
	}

	err = misc_register(&epl_sensor_ps_device);
	if (err < 0) {
		LOG_ERR("could not register ps misc device\n");
		goto err_unregister_ps_input_device;
	}

	err = sysfs_create_group(&epld->ps_input_dev->dev.kobj,
				 &proximity_attribute_group);

	if (err) {
		pr_err("%s: PS could not create sysfs group\n", __func__);
		goto err_free_ps_input_device;
	}

	return err;

err_unregister_ps_input_device:
	input_unregister_device(epld->ps_input_dev);
err_free_ps_input_device:
	input_free_device(epld->ps_input_dev);

	return err;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
#ifdef CONFIG_SUSPEND
static int epl_sensor_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();
	if (!epld) {
		LOG_ERR("null pointer!!\n");
		return -EINVAL;
	}

#if !defined(CONFIG_HAS_EARLYSUSPEND)
	epld->als_suspend = 1;

	LOG_DBG("[%s]: enable_pflag=%d, enable_lflag=%d \r\n", __func__,
		epld->enable_pflag,
		epld->enable_lflag);

	if (epld->enable_pflag == 0) {
		if (epld->enable_lflag == 1 &&
			epl_sensor.als.polling_mode == 0) {
			LOG_INFO("[%s]: check ALS interrupt_flag.....\r\n",
				__func__);
			epl_sensor_read_als(epld->client);
			if (epl_sensor.als.interrupt_flag == EPL_INT_TRIGGER) {
				LOG_INFO("[%s]: als.interrupt_flag=%d\r\n",
				__func__, epl_sensor.als.interrupt_flag);
				/*
				 *ALS unlock interrupt pin and restart chip
				 */
				epl_sensor_I2C_Write(epld->client, 0x12,
					EPL_CMP_RESET | EPL_UN_LOCK);
			}
		}
		/*
		 *atomic_set(&obj->ps_suspend, 1);
		 */
		LOG_INFO("[%s]: ps disable \r\n", __func__);
		epl_sensor_update_mode(epld->client);
	}
#endif /*CONFIG_HAS_EARLYSUSPEND*/
	return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND)
static void epl_sensor_early_suspend(struct early_suspend *h)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	if (!epld) {
		LOG_ERR("null pointer!!\n");
		return;
	}

	epld->als_suspend = 1;

	LOG_DBG("[%s]: enable_pflag=%d, enable_lflag=%d \r\n", __func__,
		epld->enable_pflag,
		epld->enable_lflag);

	if (epld->enable_pflag == 0) {
		if (epld->enable_lflag == 1 &&
			epl_sensor.als.polling_mode == 0) {
				LOG_INFO(
				"[%s]: check ALS interrupt_flag.......\r\n",
				__func__);
			epl_sensor_read_als(epld->client);
			if (epl_sensor.als.interrupt_flag == EPL_INT_TRIGGER) {
				LOG_INFO(
				"[%s]: epl_sensor.als.interrupt_flag = %d\r\n",
				__func__, epl_sensor.als.interrupt_flag);
				/*
				 *ALS unlock interrupt pin and restart chip
				 */
				epl_sensor_I2C_Write(epld->client, 0x12,
					EPL_CMP_RESET | EPL_UN_LOCK);
			}
		}
		/*
		 *atomic_set(&obj->ps_suspend, 1);
		 */
		LOG_INFO("[%s]: ps disable \r\n", __func__);
		epl_sensor_update_mode(epld->client);
	}

}
#endif

static int epl_sensor_resume(struct i2c_client *client)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	if (!epld) {
		LOG_ERR("null pointer!!\n");
		return -EINVAL;
	}

#if !defined(CONFIG_HAS_EARLYSUSPEND)
	epld->als_suspend = 0;
	epld->ps_suspend = 0;

	LOG_DBG("[%s]: enable_pflag=%d, enable_lflag=%d \r\n", __func__,
		epld->enable_pflag,
		epld->enable_lflag);

	if (epld->enable_pflag == 0) {
		LOG_INFO("[%s]: ps is disabled \r\n", __func__);
		epl_sensor_fast_update(epld->client);
		epl_sensor_update_mode(epld->client);
	} else if (epld->enable_lflag == 1 &&
		epl_sensor.als.polling_mode == 1) {
		LOG_INFO("[%s]: restart polling_work \r\n", __func__);
		epl_sensor_restart_polling();
	}
#endif /*CONFIG_HAS_EARLYSUSPEND*/

	return 0;
}
/*----------------------------------------------------------------------------*/

#if defined(CONFIG_HAS_EARLYSUSPEND)
/*----------------------------------------------------------------------------*/
static void epl_sensor_late_resume(struct early_suspend *h)
{
	struct epl_sensor_priv *epld = epl_sensor_obj;

	LOG_FUN();

	if (!epld)	{
		LOG_ERR("null pointer!!\n");
		return;
	}

	epld->als_suspend = 0;
	epld->ps_suspend = 0;

	LOG_DBG("[%s]: enable_pflag=%d, enable_lflag=%d \r\n", __func__,
		epld->enable_pflag, epld->enable_lflag);

	if (epld->enable_pflag == 0) {
		LOG_INFO("[%s]: ps is disabled \r\n", __func__);
		epl_sensor_fast_update(epld->client);
		epl_sensor_update_mode(epld->client);
	} else if (epld->enable_lflag == 1 &&
		epl_sensor.als.polling_mode == 1) {
			LOG_INFO("[%s]: restart polling_work \r\n", __func__);
			epl_sensor_restart_polling();
	}
}
#endif

#endif
/*----------------------------------------------------------------------------*/
static int als_intr_update_table(struct epl_sensor_priv *epld)
{
	int i;

	for (i = 0; i < 10; i++) {
		if (als_lux_intr_level[i] < 0xFFFF) {
#if ALS_DYN_INTT
			if (epl_sensor.als.report_type == CMC_BIT_DYN_INT) {
				als_adc_intr_level[i] = als_lux_intr_level[i]
					* 1000 / c_gain;
			} else
#endif
			{
				als_adc_intr_level[i] = als_lux_intr_level[i]
					* 1000 /
					epl_sensor.als.factory.lux_per_count;
			}

			if (i != 0 && als_adc_intr_level[i] <=
				als_adc_intr_level[i - 1])
					als_adc_intr_level[i] = 0xffff;
		} else {
			als_adc_intr_level[i] = als_lux_intr_level[i];
		}
		LOG_INFO(" [%s]:als_adc_intr_level: [%d], %ld \r\n",
			__func__, i, als_adc_intr_level[i]);
	}

	return 0;
}
/*----------------------------------------------------------------------------*/
static int epl_sensor_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	int err = 0;
	struct epl_sensor_priv *epld;
	int renvo = 0;

	LOG_INFO("elan sensor probe enter.\n");

	epld = kzalloc(sizeof(struct epl_sensor_priv), GFP_KERNEL);
	if (!epld)
		return -ENOMEM;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "No i2c func what we need?!!\n");
		err = -ENOTSUPP;
		goto i2c_fail;
	}

	LOG_INFO("chip id REG 0x00 value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x00));
	LOG_INFO("chip id REG 0x01 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x01));
	LOG_INFO("chip id REG 0x02 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x02));
	LOG_INFO("chip id REG 0x03 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x03));
	LOG_INFO("chip id REG 0x04 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x04));
	LOG_INFO("chip id REG 0x05 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x05));
	LOG_INFO("chip id REG 0x06 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x06));
	LOG_INFO("chip id REG 0x07 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x07));
	LOG_INFO("chip id REG 0x11 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x11));
	LOG_INFO("chip id REG 0x12 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x12));
	LOG_INFO("chip id REG 0x1B value = 0x%x\n",
			i2c_smbus_read_byte_data(client, 0x1B));
	LOG_INFO("chip id REG 0x20 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x20));
	LOG_INFO("chip id REG 0x21 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x21));
	LOG_INFO("chip id REG 0x24 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x24));
	LOG_INFO("chip id REG 0x25 value = 0x%x\n",
			i2c_smbus_read_byte_data(client,	0x25));

	renvo = i2c_smbus_read_byte_data(client, 0x21);
	if (renvo != 0x81 && renvo != 0x91) {
		LOG_INFO("elan ALS/PS sensor is failed(0x%x).\n", renvo);
		err = -1;
		goto i2c_fail;
	}

	epld->als_level_num = sizeof(epld->als_level) /
			sizeof(epld->als_level[0]);
	epld->als_value_num = sizeof(epld->als_value) /
			sizeof(epld->als_value[0]);
	BUG_ON(sizeof(epld->als_level) != sizeof(als_level));
	memcpy(epld->als_level, als_level, sizeof(epld->als_level));
	BUG_ON(sizeof(epld->als_value) != sizeof(als_value));
	memcpy(epld->als_value, als_value, sizeof(epld->als_value));

	epld->client = client;
	epld->irq = client->irq;
	epld->pinctrl = NULL;
	epld->gpio_state_active = NULL;
	epld->gpio_state_suspend = NULL;
	i2c_set_clientdata(client, epld);
	epl_sensor_obj = epld;

	INIT_DELAYED_WORK(&epld->eint_work, epl_sensor_eint_work);
	INIT_DELAYED_WORK(&epld->polling_work, epl_sensor_polling_work);
	mutex_init(&sensor_mutex);

	/*
	 *initial global variable and write to senosr
	 */
	initial_global_variable(client, epld);
#if !SPREAD
	err = epl_sensor_setup_lsensor(epld);
	if (err < 0) {
		LOG_ERR("epl_sensor_setup_lsensor error!!\n");
		goto err_lightsensor_setup;
	}
#endif

	err = epl_sensor_setup_psensor(epld);
	if (err < 0) {
		LOG_ERR("epl_sensor_setup_psensor error!!\n");
		goto err_psensor_setup;
	}


	if (epl_sensor.als.polling_mode == 0 ||
			epl_sensor.ps.polling_mode == 0) {
		err = epl_sensor_setup_interrupt(epld);
		if (err < 0) {
			LOG_ERR("setup error!\n");
			goto err_sensor_setup;
		}
	}

#if SENSOR_CLASS
	epld->als_cdev = als_cdev;
	epld->als_cdev.sensors_enable = epld_sensor_cdev_enable_als;
	epld->als_cdev.sensors_poll_delay = epl_sensor_cdev_set_als_delay;
	/*epld->als_cdev.sensors_flush = epl_snesor_cdev_als_flush;
		//dont support */
	err = sensors_classdev_register(&client->dev, &epld->als_cdev);
	if (err) {
		LOG_ERR("sensors class register failed.\n");
		goto err_register_als_cdev;
	}

	epld->ps_cdev = ps_cdev;
	epld->ps_cdev.sensors_enable = epld_sensor_cdev_enable_ps;
	epld->ps_cdev.sensors_poll_delay = epl_snesor_cdev_set_ps_delay;
	/*epld->ps_cdev.sensors_flush = epl_snesor_cdev_ps_flush;
		//dont support */
	/*epld->ps_cdev.sensors_calibrate = epl_snesor_cdev_ps_calibrate;
		//dont support */
	/*epld->ps_cdev.sensors_write_cal_params = epl_snesor_cdev_ps_write_cal;
		//dont support */
	/*epld->ps_cdev.params = epld->calibrate_buf;   //dont support */
	err = sensors_classdev_register(&client->dev, &epld->ps_cdev);
	if (err) {
		LOG_ERR("sensors class register failed.\n");
		goto err_register_ps_cdev;
	}
#endif

#ifdef CONFIG_SUSPEND

#if defined(CONFIG_HAS_EARLYSUSPEND)
	epld->early_suspend.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 1;
	epld->early_suspend.suspend = epl_sensor_early_suspend;
	epld->early_suspend.resume = epl_sensor_late_resume;
	register_early_suspend(&epld->early_suspend);
#endif

#endif
	wake_lock_init(&ps_lock, WAKE_LOCK_SUSPEND, "ps wakelock");
#if ATTR_RANGE_PATH
	kernel_kobj_dev = kobject_create_and_add("range", kernel_kobj);

	if (IS_ERR(kernel_kobj_dev)) {
		pr_err("kernel_kobj_dev init: error\n");
		goto err_fail;
	}

	err = sysfs_create_group(kernel_kobj_dev, &epl_sensor_attr_group);
	if (err != 0) {
		dev_err(&client->dev, "%s:create sysfs group error", __func__);
		goto err_fail;
	}
#else
	sensor_dev = platform_device_register_simple("elan_alsps", -1, NULL, 0);
	if (IS_ERR(sensor_dev)) {
		pr_err("sensor_dev_init: error\n");
		goto err_fail;
	}


	err = sysfs_create_group(&sensor_dev->dev.kobj, &epl_sensor_attr_group);
	if (err != 0) {
		dev_err(&client->dev, "%s:create sysfs group error", __func__);
		goto err_fail;
	}
#endif
	LOG_INFO("sensor probe success.\n");

	return err;

#if SENSOR_CLASS
err_register_ps_cdev:
	sensors_classdev_unregister(&epld->ps_cdev);
err_register_als_cdev:
	sensors_classdev_unregister(&epld->als_cdev);
#endif
err_fail:
	input_unregister_device(epld->als_input_dev);
	input_unregister_device(epld->ps_input_dev);
	input_free_device(epld->als_input_dev);
	input_free_device(epld->ps_input_dev);
#if !SPREAD
err_lightsensor_setup:
#endif
err_psensor_setup:
err_sensor_setup:
	misc_deregister(&epl_sensor_ps_device);
#if !SPREAD
	misc_deregister(&epl_sensor_als_device);
#endif
i2c_fail:
	kfree(epld);

	return err;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static int epl_sensor_remove(struct i2c_client *client)
{
	struct epl_sensor_priv *epld = i2c_get_clientdata(client);

	dev_dbg(&client->dev, "%s: enter.\n", __func__);
#if defined(CONFIG_HAS_EARLYSUSPEND)
	unregister_early_suspend(&epld->early_suspend);
#endif
	sysfs_remove_group(&sensor_dev->dev.kobj, &epl_sensor_attr_group);
	platform_device_unregister(sensor_dev);
	input_unregister_device(epld->als_input_dev);
	input_unregister_device(epld->ps_input_dev);
#if !SPREAD
	input_free_device(epld->als_input_dev);
#endif
	input_free_device(epld->ps_input_dev);
	misc_deregister(&epl_sensor_ps_device);
#if !SPREAD
	misc_deregister(&epl_sensor_als_device);
#endif

	free_irq(epld->irq, epld);

	if (epld->pinctrl && epld->gpio_state_suspend)
		pinctrl_select_state(epld->pinctrl, epld->gpio_state_suspend);

	kfree(epld);

	return 0;
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static const struct i2c_device_id epl_sensor_id[] = {
	{ EPL_DEV_NAME, 0 },
	{ }
};

#ifdef CONFIG_OF
static struct of_device_id epl_match_table[] = {
	{ .compatible = "epl,epl8802",},
	{ },
};
MODULE_DEVICE_TABLE(of, epl_match_table);
#endif

/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
static struct i2c_driver epl_sensor_driver = {
	.probe	= epl_sensor_probe,
	.remove	= epl_sensor_remove,
	.id_table	= epl_sensor_id,
	.driver	= {
		.name = EPL_DEV_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = epl_match_table,
#endif
	},
#ifdef CONFIG_SUSPEND
	.suspend = epl_sensor_suspend,
	.resume = epl_sensor_resume,
#endif
};

static int __init epl_sensor_init(void)
{
	return i2c_add_driver(&epl_sensor_driver);
}

static void __exit epl_sensor_exit(void)
{
	i2c_del_driver(&epl_sensor_driver);
}
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
module_init(epl_sensor_init);
module_exit(epl_sensor_exit);
/*----------------------------------------------------------------------------*/

/*----------------------------------------------------------------------------*/
MODULE_AUTHOR("Renato Pan <renato.pan@eminent-tek.com>");
MODULE_DESCRIPTION("ELAN epl8802 driver");
MODULE_LICENSE("GPL");
