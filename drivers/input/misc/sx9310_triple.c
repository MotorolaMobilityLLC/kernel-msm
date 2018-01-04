/*
 * file sx9310_sar.c
 * brief SX9310 Driver for two channel SAP using
 *
 * Driver for the SX9310
 * Copyright (c) 2015-2016 Semtech Corp
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */
#define DEBUG
#define DRIVER_NAME "sx9310"
#define USE_SENSORS_CLASS

#define MAX_WRITE_ARRAY_SIZE 32
#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/device.h>
#include <linux/sensors.h>
#include <linux/interrupt.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/usb.h>
#include <linux/power_supply.h>

/* main struct, interrupt,init,pointers */
#include <linux/input/sx9310_triple.h>

#define IDLE 0
#define ACTIVE 1
#define S_PROX   1
#define S_BODY   2

#define SX9310_DEBUG 1
#define LOG_TAG "SX9310 "

#if SX9310_DEBUG
#define LOG_INFO(fmt, args...)    pr_info(LOG_TAG fmt, ##args)
#else
#define LOG_INFO(fmt, args...)
#endif

#define LOG_DBG(fmt, args...)	pr_debug(LOG_TAG fmt, ##args)
#define LOG_ERR(fmt, args...)	pr_err(LOG_TAG fmt, ##args)

static int sx9310_debug_enable;
module_param_named(
        debug_enable, sx9310_debug_enable, int, S_IRUSR | S_IWUSR
);

static int last_val;
static int mEnabled;
static int programming_done;
psx93XX_t sx9310_sar_ptr;

/**
 * struct sx9310
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct sx9310 {
	pbuttonInformation_t pbuttonInformation;
	psx9310_platform_data_t hw; /* specific platform data settings */
} sx9310_t, *psx9310_t;

static void ForcetoTouched(psx93XX_t this)
{
	psx9310_t pDevice = NULL;
	struct input_dev *input_top = NULL;
	struct input_dev *input_bottom = NULL;
	struct _buttonInfo *pCurrentButton  = NULL;

	pDevice = this->pDevice;
	if (this && pDevice) {
		LOG_INFO("ForcetoTouched()\n");

		pCurrentButton = pDevice->pbuttonInformation->buttons;
		input_top = pDevice->pbuttonInformation->input_top;
		input_bottom = pDevice->pbuttonInformation->input_bottom;
		pCurrentButton->state = ACTIVE;
		last_val = 1;
		if (mEnabled) {
			input_report_abs(input_top, ABS_DISTANCE, 1);
			input_sync(input_top);
			input_report_abs(input_bottom, ABS_DISTANCE, 1);
			input_sync(input_bottom);
		}
		LOG_INFO("Leaving ForcetoTouched()\n");
	}
}

/**
 * fn static int write_register(psx93XX_t this, u8 address, u8 value)
 * brief Sends a write register to the device
 * param this Pointer to main parent struct
 * param address 8-bit register address
 * param value   8-bit register value to write to address
 * return Value from i2c_master_send
 */
static int write_register(psx93XX_t this, u8 address, u8 value)
{
	struct i2c_client *i2c = 0;
	char buffer[2];
	int returnValue = 0;

	buffer[0] = address;
	buffer[1] = value;
	returnValue = -ENOMEM;
	if (this && this->bus) {
		i2c = this->bus;

		returnValue = i2c_master_send(i2c, buffer, 2);
		if (sx9310_debug_enable)
			LOG_DBG("write_register Addr: \
				0x%x Val: 0x%x Return: %d\n",
				address, value, returnValue);
	}
	if (returnValue < 0) {
		ForcetoTouched(this);
		LOG_INFO("Write_register-ForcetoTouched()\n");
	}
	return returnValue;
}

/**
 * fn static int read_register(psx93XX_t this, u8 address, u8 *value)
 * brief Reads a register's value from the device
 * param this Pointer to main parent struct
 * param address 8-Bit address to read from
 * param value Pointer to 8-bit value to save register value to
 * return Value from i2c_smbus_read_byte_data if < 0. else 0
 */
static int read_register(psx93XX_t this, u8 address, u8 *value)
{
	struct i2c_client *i2c = 0;
	s32 returnValue = 0;

	if (this && value && this->bus) {
		i2c = this->bus;
		returnValue = i2c_smbus_read_byte_data(i2c, address);
		if (sx9310_debug_enable)
			LOG_DBG("read_register Addr: 0x%x Return: 0x%x\n",
				address, returnValue);
		if (returnValue >= 0) {
			*value = returnValue;
			return 0;
		} else {
			return returnValue;
		}
	}
	ForcetoTouched(this);
	LOG_INFO("read_register-ForcetoTouched()\n");
	return -ENOMEM;
}

/**
 * detect if sx9310 exist or not
 * return 1 if chip exist else return 0
 */
static int sx9310_detect(struct i2c_client *client)
{
	s32 returnValue = 0, i;
	u8 address = SX9310_WHOAMI;
	u8 value = 0x01;

	if (client) {
		for (i = 0; i < 3; i++) {
			returnValue = i2c_smbus_read_byte_data(client, address);
			LOG_INFO("sx9310 read_register for %d time Addr:\
					0x%x Return: 0x%x\n",
					i, address, returnValue);
			if (returnValue >= 0) {
				if (value == returnValue) {
					LOG_INFO("sx9310 detect success!\n");
					return 1;
				}
			}
		}
	}
	LOG_ERR("sx9310 detect failed!!!\n");
	return 0;
}

/**
 * brief Perform a manual offset calibration
 * param this Pointer to main parent struct
 * return Value return value from the write register
 */
static int manual_offset_calibration(psx93XX_t this)
{
	s32 returnValue = 0;

	returnValue = write_register(this, SX9310_IRQSTAT_REG, 0xFF);
	return returnValue;
}

/**
 * brief sysfs show function for manual calibration which currently just
 * returns register value.
 */
static ssize_t manual_offset_calibration_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 reg_value = 0;
	psx93XX_t this = dev_get_drvdata(dev);

	LOG_INFO("Reading IRQSTAT_REG\n");
	read_register(this, SX9310_IRQSTAT_REG, &reg_value);
	return scnprintf(buf, PAGE_SIZE, "%d\n", reg_value);
}

/* brief sysfs store function for manual calibration */
static ssize_t manual_offset_calibration_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	psx93XX_t this = dev_get_drvdata(dev);
	unsigned long val;

	if (kstrtoul(buf, 0, &val))
		return -EINVAL;
	if (val) {
		LOG_INFO("Performing manual_offset_calibration()\n");
		manual_offset_calibration(this);
	}
	return count;
}

static DEVICE_ATTR(calibrate, 0644, manual_offset_calibration_show,
		manual_offset_calibration_store);
static struct attribute *sx9310_attributes[] = {
	&dev_attr_calibrate.attr,
	NULL,
};
static struct attribute_group sx9310_attr_group = {
	.attrs = sx9310_attributes,
};

/**
 * fn static int read_regStat(psx93XX_t this)
 * brief Shortcut to read what caused interrupt.
 * details This is to keep the drivers a unified
 * function that will read whatever register(s)
 * provide information on why the interrupt was caused.
 * param this Pointer to main parent struct
 * return If successful, Value of bit(s) that cause interrupt, else 0
 */
static int read_regStat(psx93XX_t this)
{
	u8 data = 0;

	if (this) {
		if (read_register(this, SX9310_IRQSTAT_REG, &data) == 0)
			return (data & 0x00FF);
	}
	return 0;
}

static void read_rawData(psx93XX_t this)
{
	u8 msb = 0, lsb = 0;
	unsigned int ii;

	if (this) {
		for (ii = 0; ii < USE_CHANNEL_NUM; ii++) {
			/* here to check the CSx */
			write_register(this, SX9310_CPSRD, ii);
			msleep(100);
			read_register(this, SX9310_USEMSB, &msb);
			read_register(this, SX9310_USELSB, &lsb);
			LOG_INFO("sx9310 cs%d USEFUL msb = 0x%x, lsb = 0x%x\n",
					ii, msb, lsb);
			read_register(this, SX9310_AVGMSB, &msb);
			read_register(this, SX9310_AVGLSB, &lsb);
			LOG_INFO("sx9310 cs%d AVERAGE msb = 0x%x, lsb = 0x%x\n",
					ii, msb, lsb);
			read_register(this, SX9310_DIFFMSB, &msb);
			read_register(this, SX9310_DIFFLSB, &lsb);
			LOG_INFO("sx9310 cs%d DIFF msb = 0x%x, lsb = 0x%x\n",
					ii, msb, lsb);
			read_register(this, SX9310_OFFSETMSB, &msb);
			read_register(this, SX9310_OFFSETLSB, &lsb);
			LOG_INFO("sx9310 cs%d OFFSET msb = 0x%x, lsb = 0x%x\n",
					ii, msb, lsb);
		}
	}
}

/**
 * brief  Initialize I2C config from platform data
 * param this Pointer to main parent struct
 *
 */
static void hw_init(psx93XX_t this)
{
	psx9310_t pDevice = 0;
	psx9310_platform_data_t pdata = 0;
	int i = 0;
	/* configure device */
	LOG_INFO("Going to Setup I2C Registers\n");
	pDevice = this->pDevice;
	pdata = pDevice->hw;
	if (this && pDevice && pdata) {
		while (i < pdata->i2c_reg_num) {
			/* Write all registers/values contained in i2c_reg */
			if (sx9310_debug_enable)
				LOG_DBG("Going to Write Reg: \
					0x%x Value: 0x%x\n",
					pdata->pi2c_reg[i].reg,
					pdata->pi2c_reg[i].val);
			write_register(this, pdata->pi2c_reg[i].reg,
					pdata->pi2c_reg[i].val);
			i++;
		}

		write_register(this, SX9310_CPS_CTRL0_REG,
				this->board->cust_prox_ctrl0);
		write_register(this, SX9310_CPSRD,
				this->board->cust_raw_data_channel);
	} else {
		LOG_ERR("ERROR! platform data 0x%p\n", pDevice->hw);
		/* Force to touched if error */
		ForcetoTouched(this);
		LOG_INFO("Hardware_init-ForcetoTouched()\n");
	}
}

/**
 * fn static int initialize(psx93XX_t this)
 * brief Performs all initialization needed to configure the device
 * param this Pointer to main parent struct
 * return Last used command's return value (negative if error)
 */
static int initialize(psx93XX_t this)
{
	int ret;

	if (this) {
		/* prepare reset by disabling any irq handling */
		this->irq_disabled = 1;
		disable_irq(this->irq);
		/* perform a reset */
		ret = write_register(this, SX9310_SOFTRESET_REG,
				SX9310_SOFTRESET);
		if (ret < 0)
			goto error_exit;
		/* wait until the reset has finished by monitoring NIRQ */
		LOG_INFO("Software Reset. Waiting device back to continue.\n");
		/* just sleep for awhile instead of using a loop
		 * with reading irq status */
		msleep(300);
		/*
		 * while(this->get_nirq_low && this->get_nirq_low())
		 * { read_regStat(this); }
		 */
		LOG_INFO("Device back from the reset, continuing. NIRQ = %d\n",
				this->get_nirq_low(this->board->irq_gpio));
		hw_init(this);
		msleep(100); /* make sure everything is running */
		ret = manual_offset_calibration(this);
		if (ret < 0)
			goto error_exit;
		/* re-enable interrupt handling */
		enable_irq(this->irq);
		this->irq_disabled = 0;

		/* make sure no interrupts are pending
		 * since enabling irq will only
		 * work on next falling edge */
		read_regStat(this);
		LOG_INFO("Exiting initialize(). NIRQ = %d\n",
				this->get_nirq_low(this->board->irq_gpio));
		programming_done = ACTIVE;
		return 0;
	}
	return -ENOMEM;

error_exit:
	programming_done = IDLE;
	return ret;
}

/**
 * brief Handle what to do when a touch occurs
 * param this Pointer to main parent struct
 */
static void touchProcess(psx93XX_t this)
{
	int counter = 0;
	u8 i = 0;
	int numberOfButtons = 0;
	psx9310_t pDevice = NULL;
	struct _buttonInfo *buttons = NULL;
	struct input_dev *input_top = NULL;
	struct input_dev *input_bottom = NULL;
	struct _buttonInfo *pCurrentButton  = NULL;
	struct sx9310_platform_data *board;

	pDevice = this->pDevice;
	board = this->board;
	if (this && pDevice) {
		if (sx9310_debug_enable)
			LOG_DBG("Inside touchProcess()\n");
		read_register(this, SX9310_STAT0_REG, &i);

		buttons = pDevice->pbuttonInformation->buttons;
		input_top = pDevice->pbuttonInformation->input_top;
		input_bottom = pDevice->pbuttonInformation->input_bottom;
		numberOfButtons = pDevice->pbuttonInformation->buttonSize;

		if (unlikely((buttons == NULL) || (input_top == NULL) ||
						 (input_bottom == NULL))) {
			LOG_ERR("ERROR!! buttons or input NULL!!!\n");
			return;
		}

		for (counter = 0; counter < numberOfButtons; counter++) {
			pCurrentButton = &buttons[counter];
			if (pCurrentButton == NULL) {
				LOG_ERR("ERR!current button index: %d NULL!\n",
						counter);
				return; /* ERRORR!!!! */
			}
			switch (pCurrentButton->state) {
			case IDLE: /* Button is being in far state! */
				if ((i & pCurrentButton->mask) ==
							 pCurrentButton->mask) {
					LOG_INFO("CS %d State=BODY.\n",
							counter);
					if (board->cap_channel_top == counter) {
						input_report_abs(input_top,
							ABS_DISTANCE, 2);
						input_sync(input_top);
					} else if (board->cap_channel_bottom ==
								 counter) {
						input_report_abs(input_bottom,
							 ABS_DISTANCE, 2);
						input_sync(input_bottom);
					}
					pCurrentButton->state = S_BODY;
					last_val = 2;
				} else if ((i & pCurrentButton->mask) ==
					   (pCurrentButton->mask & 0x0f)) {
					LOG_INFO("CS %d State=PROX.\n",
							counter);
					if (board->cap_channel_top == counter) {
						input_report_abs(input_top,
							ABS_DISTANCE, 1);
						input_sync(input_top);
					} else if (board->cap_channel_bottom ==
								 counter) {
						input_report_abs(input_bottom,
							 ABS_DISTANCE, 1);
						input_sync(input_bottom);
					}
					pCurrentButton->state = S_PROX;
					last_val = 0;
				} else {
					if (sx9310_debug_enable)
						LOG_DBG("CS %d still in"
							  "IDLE State.\n",
							  counter);
				}
				break;
			case S_PROX: /* Button is being in proximity! */
				if ((i & pCurrentButton->mask) ==
							 pCurrentButton->mask) {
					LOG_INFO("CS %d State=BODY.\n",
							counter);
					if (board->cap_channel_top == counter) {
						input_report_abs(input_top,
							 ABS_DISTANCE, 2);
						input_sync(input_top);
					} else if (board->cap_channel_bottom ==
								 counter) {
						input_report_abs(input_bottom,
							ABS_DISTANCE, 2);
						input_sync(input_bottom);
					}
					pCurrentButton->state = S_BODY;
					last_val = 2;
				} else if ((i & pCurrentButton->mask) ==
					   (pCurrentButton->mask & 0x0f)) {
					LOG_INFO("CS %d still in PROX State.\n",
							counter);
				} else{
					if (sx9310_debug_enable)
						LOG_DBG("CS %d State=IDLE.\n",
							counter);
					if (board->cap_channel_top == counter) {
						input_report_abs(input_top,
							ABS_DISTANCE, 0);
						input_sync(input_top);
					} else if (board->cap_channel_bottom ==
								 counter) {
						input_report_abs(input_bottom,
							 ABS_DISTANCE, 0);
						input_sync(input_bottom);
					}
					pCurrentButton->state = IDLE;
					last_val = 0;
				}
				break;
			case S_BODY: /* Button is being in 0mm! */
				if ((i & pCurrentButton->mask) ==
							 pCurrentButton->mask) {
					LOG_INFO("CS %d still in BODY State.\n",
							counter);
				} else if ((i & pCurrentButton->mask) ==
					   (pCurrentButton->mask & 0x0f)) {
					LOG_INFO("CS %d State=PROX.\n",
							counter);
					if (board->cap_channel_top == counter) {
						input_report_abs(input_top,
							ABS_DISTANCE, 1);
						input_sync(input_top);
					} else if (board->cap_channel_bottom
								 == counter) {
						input_report_abs(input_bottom,
							 ABS_DISTANCE, 1);
						input_sync(input_bottom);
					}
					pCurrentButton->state = S_PROX;
					last_val = 1;
				} else{
					if (sx9310_debug_enable)
						LOG_DBG("CS %d State=IDLE.\n",
							counter);
					if (board->cap_channel_top == counter) {
						input_report_abs(input_top,
							ABS_DISTANCE, 0);
						input_sync(input_top);
					} else if (board->cap_channel_bottom
								 == counter) {
						input_report_abs(input_bottom,
							ABS_DISTANCE, 0);
						input_sync(input_bottom);
					}
					pCurrentButton->state = IDLE;
					last_val = 0;
				}
				break;
			/* Shouldn't be here, device only
			 * allowed ACTIVE or IDLE */
			default:
				break;
			};
		}
		if (sx9310_debug_enable)
			LOG_DBG("Leaving touchProcess()\n");
	}
}

static int sx9310_get_nirq_state(unsigned irq_gpio)
{
	if (irq_gpio)
		return !gpio_get_value(irq_gpio);

	LOG_ERR("sx9310 irq_gpio is not set.");
	return -EINVAL;
}

static struct _totalButtonInformation smtcButtonInformation = {
	.buttons = psmtcButtons,
	.buttonSize = ARRAY_SIZE(psmtcButtons),
};

/**
 *fn static void sx9310_reg_setup_init(struct i2c_client *client)
 *brief read reg val form dts
 *      reg_array_len for regs needed change num
 *      data_array_val's format <reg val ...>
 */
static void sx9310_reg_setup_init(struct i2c_client *client)
{
	u32 data_array_len = 0;
	u32 *data_array;
	int ret, i, j;
	struct device_node *np = client->dev.of_node;

	ret = of_property_read_u32(np, "reg_array_len", &data_array_len);
	if (ret < 0) {
		LOG_ERR("data_array_len read error");
		return;
	}
	data_array = kmalloc(data_array_len * 2 * sizeof(u32), GFP_KERNEL);
	ret = of_property_read_u32_array(np, "reg_array_val",
			data_array,
			data_array_len*2);
	if (ret < 0) {
		LOG_ERR("data_array_val read error");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(sx9310_i2c_reg_setup); i++) {
		for (j = 0; j < data_array_len*2; j += 2) {
			if (data_array[j] == sx9310_i2c_reg_setup[i].reg) {
				sx9310_i2c_reg_setup[i].val = data_array[j+1];
				LOG_INFO("read dtsi 0x%02x:0x%02x set reg\n",
					data_array[j], data_array[j+1]);
			}
		}
	}
	kfree(data_array);
	/*for test*/
	/*for (i = 0; i < ARRAY_SIZE(sx9310_i2c_reg_setup); i++) {
	  dev_err(&client->dev, "%x:%x",
	  sx9310_i2c_reg_setup[i].reg,
	  sx9310_i2c_reg_setup[i].val);
	  }*/
}

static void sx9310_platform_data_of_init(struct i2c_client *client,
		psx9310_platform_data_t pplatData)
{
	struct device_node *np = client->dev.of_node;
	u32 scan_period, sensor_en, raw_data_channel;
	u32 cap_channel_top, cap_channel_bottom;
	int ret;

	client->irq = of_get_gpio(np, 0);
	pplatData->irq_gpio = client->irq;

	ret = of_property_read_u32(np, "cap,use_channel", &sensor_en);
	if (ret)
		sensor_en = DUMMY_USE_CHANNEL;

	ret = of_property_read_u32(np, "cap,use_channel_top", &cap_channel_top);
	if (ret)
		sensor_en = DUMMY_USE_CHANNEL;

	ret = of_property_read_u32(np, "cap,use_channel_bottom",
					&cap_channel_bottom);
	if (ret)
		sensor_en = DUMMY_USE_CHANNEL;

	ret = of_property_read_u32(np, "cap,scan_period", &scan_period);
	if (ret)
		scan_period = DUMMY_SCAN_PERIOD;

	ret = of_property_read_u32(np, "cap,raw_data_channel",
					&raw_data_channel);
	if (ret)
		raw_data_channel = DUMMY_RAW_DATA_CHANNEL;

	pplatData->cust_prox_ctrl0 = (scan_period << 4) | sensor_en;
	pplatData->cust_raw_data_channel = raw_data_channel;
	pplatData->cap_channel_top = (int)cap_channel_top;
	pplatData->cap_channel_bottom = (int)cap_channel_bottom;

	pplatData->get_is_nirq_low = sx9310_get_nirq_state;
	pplatData->init_platform_hw = NULL;
	/*  pointer to an exit function. Here in case needed in the future */
	/*
	 *.exit_platform_hw = sx9310_exit_ts,
	 */
	pplatData->exit_platform_hw = NULL;
	sx9310_reg_setup_init(client);
	pplatData->pi2c_reg = sx9310_i2c_reg_setup;
	pplatData->i2c_reg_num = ARRAY_SIZE(sx9310_i2c_reg_setup);

	pplatData->pbuttonInformation = &smtcButtonInformation;
}

static ssize_t capsense_reset_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, 8, "%d\n", programming_done);
}

static ssize_t capsense_reset_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	psx93XX_t this = sx9310_sar_ptr;
	psx9310_t pDevice = NULL;
	struct input_dev *input_top = NULL;
	struct input_dev *input_bottom = NULL;

	pDevice = this->pDevice;
	input_top = pDevice->pbuttonInformation->input_top;
	input_bottom = pDevice->pbuttonInformation->input_bottom;

	if (!count || (this == NULL))
		return -EINVAL;

	if (!strncmp(buf, "reset", 5) || !strncmp(buf, "1", 1))
		write_register(this, SX9310_IRQSTAT_REG, 0xFF);

	input_report_abs(input_top, ABS_DISTANCE, 0);
	input_sync(input_top);
	input_report_abs(input_bottom, ABS_DISTANCE, 0);
	input_sync(input_bottom);

	return count;
}

static CLASS_ATTR(reset, 0660, capsense_reset_show, capsense_reset_store);

static ssize_t capsense_enable_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, 8, "%d\n", mEnabled);
}

static ssize_t capsense_enable_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	psx93XX_t this = sx9310_sar_ptr;
	psx9310_t pDevice = NULL;
	struct input_dev *input_top = NULL;
	struct input_dev *input_bottom = NULL;

	pDevice = this->pDevice;
	input_top = pDevice->pbuttonInformation->input_top;
	input_bottom = pDevice->pbuttonInformation->input_bottom;

	if (!count || (this == NULL))
		return -EINVAL;

	if ((!strncmp(buf, "1", 1)) && (mEnabled == 0)) {
		LOG_INFO("enable cap sensor\n");
		initialize(this);

		input_report_abs(input_top, ABS_DISTANCE, 0);
		input_sync(input_top);
		input_report_abs(input_bottom, ABS_DISTANCE, 0);
		input_sync(input_bottom);
		mEnabled = 1;
	} else if ((!strncmp(buf, "0", 1)) && (mEnabled == 1)) {
		LOG_INFO("disable cap sensor\n");

		write_register(this, SX9310_CPS_CTRL0_REG, 0x10);
		write_register(this, SX9310_IRQ_ENABLE_REG, 0x00);

		input_report_abs(input_top, ABS_DISTANCE, -1);
		input_sync(input_top);
		input_report_abs(input_bottom, ABS_DISTANCE, -1);
		input_sync(input_bottom);
		mEnabled = 0;
	} else {
		LOG_ERR("unknown enable symbol\n");
	}

	return count;
}

#ifdef USE_SENSORS_CLASS
static int capsensor_set_enable(struct sensors_classdev *sensors_cdev,
				unsigned int enable)
{
	psx93XX_t this = sx9310_sar_ptr;
	psx9310_t pDevice = NULL;
	struct input_dev *input_top = NULL;
	struct input_dev *input_bottom = NULL;

	pDevice = this->pDevice;
	input_top = pDevice->pbuttonInformation->input_top;
	input_bottom = pDevice->pbuttonInformation->input_bottom;

	if ((enable == 1) && (mEnabled == 0)) {
		LOG_INFO("enable cap sensor\n");
		initialize(this);

		input_report_abs(input_top, ABS_DISTANCE, 0);
		input_sync(input_top);
		input_report_abs(input_bottom, ABS_DISTANCE, 0);
		input_sync(input_bottom);
		mEnabled = 1;
	} else if ((enable == 0) && (mEnabled == 1)) {
		LOG_INFO("disable cap sensor\n");

		write_register(this, SX9310_CPS_CTRL0_REG, 0x10);
		write_register(this, SX9310_IRQ_ENABLE_REG, 0x00);

		input_report_abs(input_top, ABS_DISTANCE, -1);
		input_sync(input_top);
		input_report_abs(input_bottom, ABS_DISTANCE, -1);
		input_sync(input_bottom);
		mEnabled = 0;
	} else {
		LOG_ERR("unknown enable symbol\n");
	}

	return 0;
}
#endif

static CLASS_ATTR(enable, 0660, capsense_enable_show, capsense_enable_store);

static ssize_t reg_dump_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	u8 reg_value = 0;
	u16 offset_value = 0;
	u16 diff_value = 0;
	psx93XX_t this = sx9310_sar_ptr;
	char *p = buf;

	if (this->read_flag) {
		this->read_flag = 0;
		read_register(this, this->read_reg, &reg_value);
		p += snprintf(p, PAGE_SIZE, "%02x\n", reg_value);
		return (p-buf);
	}

	read_register(this, SX9310_IRQ_ENABLE_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "ENABLE(0x%02x)=0x%02x\n",
			SX9310_IRQ_ENABLE_REG, reg_value);

	read_register(this, SX9310_IRQFUNC_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "IRQFUNC(0x%02x)=0x%02x\n",
			SX9310_IRQFUNC_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL0_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL0(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL0_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL1_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL1(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL1_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL2_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL2(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL2_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL3_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL3(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL3_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL4_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL4(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL4_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL5_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL5(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL5_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL6_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL6(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL6_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL7_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL7(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL7_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL8_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL8(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL8_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL9_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL9(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL9_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL10_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL10(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL10_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL11_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL11(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL11_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL12_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL12(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL12_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL13_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL13(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL13_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL14_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL14(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL14_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL15_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL15(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL15_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL16_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL16(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL16_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL17_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL17(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL17_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL18_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL18(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL18_REG, reg_value);

	read_register(this, SX9310_CPS_CTRL19_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CTRL19(0x%02x)=0x%02x\n",
			SX9310_CPS_CTRL19_REG, reg_value);

	read_register(this, SX9310_SAR_CTRL0_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "SCTRL0(0x%02x)=0x%02x\n",
			SX9310_SAR_CTRL0_REG, reg_value);

	read_register(this, SX9310_SAR_CTRL1_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "SCTRL1(0x%02x)=0x%02x\n",
			SX9310_SAR_CTRL1_REG, reg_value);

	read_register(this, SX9310_SAR_CTRL2_REG, &reg_value);
	p += snprintf(p, PAGE_SIZE, "SCTRL2(0x%02x)=0x%02x\n",
			SX9310_SAR_CTRL2_REG, reg_value);

	read_register(this, SX9310_CPSRD, &reg_value);
	p += snprintf(p, PAGE_SIZE, "CPSD(0x%02x)=0x%02x\n",
			SX9310_CPSRD, reg_value);

	read_register(this, SX9310_USEMSB, &reg_value);
	p += snprintf(p, PAGE_SIZE, "USEMSB(0x%02x)=0x%02x\n",
			SX9310_USEMSB, reg_value);

	read_register(this, SX9310_USELSB, &reg_value);
	p += snprintf(p, PAGE_SIZE, "USELSB(0x%02x)=0x%02x\n",
			SX9310_USELSB, reg_value);

	read_register(this, SX9310_AVGMSB, &reg_value);
	p += snprintf(p, PAGE_SIZE, "AVGMSB(0x%02x)=0x%02x\n",
			SX9310_AVGMSB, reg_value);

	read_register(this, SX9310_AVGLSB, &reg_value);
	p += snprintf(p, PAGE_SIZE, "AVGLSB(0x%02x)=0x%02x\n",
			SX9310_AVGLSB, reg_value);

	read_register(this, SX9310_DIFFMSB, &reg_value);
	p += snprintf(p, PAGE_SIZE, "DIFFMSB(0x%02x)=0x%02x\n",
			SX9310_DIFFMSB, reg_value);
	diff_value = reg_value;
	diff_value <<= 8;

	read_register(this, SX9310_DIFFLSB, &reg_value);
	p += snprintf(p, PAGE_SIZE, "DIFFLSB(0x%02x)=0x%02x\n",
			SX9310_DIFFLSB, reg_value);
	diff_value += reg_value;

	read_register(this, SX9310_OFFSETMSB, &reg_value);
	p += snprintf(p, PAGE_SIZE, "OFFSETMSB(0x%02x)=0x%02x\n",
			SX9310_OFFSETMSB, reg_value);
	offset_value = reg_value;
	offset_value <<= 8;

	read_register(this, SX9310_OFFSETLSB, &reg_value);
	p += snprintf(p, PAGE_SIZE, "OFFSETLSB(0x%02x)=0x%02x\n",
			SX9310_OFFSETLSB, reg_value);
	offset_value += reg_value;

	read_register(this, SX9310_SARMSB, &reg_value);
	p += snprintf(p, PAGE_SIZE, "SARMSB(0x%02x)=0x%02x\n",
			SX9310_SARMSB, reg_value);

	read_register(this, SX9310_SARLSB, &reg_value);
	p += snprintf(p, PAGE_SIZE, "SARLSB(0x%02x)=0x%02x\n",
			SX9310_SARLSB, reg_value);

	reg_value = gpio_get_value(this->board->irq_gpio);
	p += snprintf(p, PAGE_SIZE, "NIRQ=%d\n", reg_value);

	p += snprintf(p, PAGE_SIZE, "diff_value=0x%04x\n",
					diff_value);
	p += snprintf(p, PAGE_SIZE, "offset_value=0x%04x\n",
					offset_value);

	return (p-buf);
}

static ssize_t reg_dump_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	psx93XX_t this = sx9310_sar_ptr;
	unsigned int val, reg, opt;

	if (strcmp("select_ch0\n", buf) == 0) {
		write_register(this, SX9310_CPSRD, 0x00);
	} else if (strcmp("select_ch1\n", buf) == 0) {
		write_register(this, SX9310_CPSRD, 0x01);
	} else if (strcmp("select_ch2\n", buf) == 0) {
		write_register(this, SX9310_CPSRD, 0x02);
	} else if (strcmp("calibrate\n", buf) == 0) {
		write_register(this, SX9310_IRQSTAT_REG, 0xff);
	} else if (sscanf(buf, "%x,%x,%x", &reg, &val, &opt) == 3) {
		LOG_DBG("%s, read reg = 0x%02x\n", __func__, *(u8 *)&reg);
		this->read_reg = *((u8 *)&reg);
		this->read_flag = 1;
	} else if (sscanf(buf, "%x,%x", &reg, &val) == 2) {
		LOG_DBG("%s,reg = 0x%02x, val = 0x%02x\n",
				__func__, *(u8 *)&reg, *(u8 *)&val);
		write_register(this, *((u8 *)&reg), *((u8 *)&val));
	}

	return count;
}

static CLASS_ATTR(reg, 0660, reg_dump_show, reg_dump_store);

static void ps_notify_callback_work(struct work_struct *work)
{
	psx93XX_t this = container_of(work, sx93XX_t, ps_notify_work);
	psx9310_t pDevice = NULL;
	struct input_dev *input_top = NULL;
	struct input_dev *input_bottom = NULL;
	int ret = 0;

	pDevice = this->pDevice;
	input_top = pDevice->pbuttonInformation->input_top;
	input_bottom = pDevice->pbuttonInformation->input_bottom;

	LOG_INFO("Usb insert,going to force calibrate\n");
	ret = write_register(this, SX9310_IRQSTAT_REG, 0xff);
	if (ret < 0)
		LOG_ERR(" Usb insert,calibrate cap sensor failed\n");

	input_report_abs(input_top, ABS_DISTANCE, 0);
	input_sync(input_top);
	input_report_abs(input_bottom, ABS_DISTANCE, 0);
	input_sync(input_bottom);
}

static int ps_get_state(struct power_supply *psy, bool *present)
{
	union power_supply_propval pval = { 0 };
	int retval;

	retval = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
			&pval);
	if (retval) {
		LOG_ERR("%s psy get property failed\n", psy->name);
		return retval;
	}
	*present = (pval.intval) ? true : false;
	LOG_INFO("%s is %s\n", psy->name,
			(*present) ? "present" : "not present");
	return 0;
}

static int ps_notify_callback(struct notifier_block *self,
		unsigned long event, void *p)
{
	psx93XX_t this = container_of(self, sx93XX_t, ps_notif);
	struct power_supply *psy = p;
	bool present;
	int retval;

	if ((event == PSY_EVENT_PROP_ADDED || event == PSY_EVENT_PROP_CHANGED)
			&& psy && psy->get_property && psy->name &&
			!strncmp(psy->name, "usb", sizeof("usb"))) {
		LOG_INFO("ps notification: event = %lu\n", event);
		retval = ps_get_state(psy, &present);
		if (retval) {
			LOG_ERR("psy get property failed\n");
			return retval;
		}

		if (event == PSY_EVENT_PROP_CHANGED) {
			if (this->ps_is_present == present) {
				LOG_INFO("ps present state not change\n");
				return 0;
			}
		}
		this->ps_is_present = present;
		schedule_work(&this->ps_notify_work);
	}

	return 0;
}

static struct class capsense_class = {
	.name			= "capsense",
	.owner			= THIS_MODULE,
};

/**
 * fn static int sx9310_probe(struct i2c_client *client, const struct i2c_device_id *id)
 * brief Probe function
 * param client pointer to i2c_client
 * param id pointer to i2c_device_id
 * return Whether probe was successful
 */
static int sx9310_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	psx93XX_t this = 0;
	psx9310_t pDevice = 0;
	psx9310_platform_data_t pplatData = 0;
	int ret;
	struct input_dev *input_top = NULL;
	struct input_dev *input_bottom = NULL;
	struct power_supply *psy = NULL;

	LOG_INFO("sx9310_probe()\n");

	/* detect if sx9310 exist or not */
	if (sx9310_detect(client) == 0)
		return -ENODEV;

	pplatData = kzalloc(sizeof(pplatData), GFP_KERNEL);
	sx9310_platform_data_of_init(client, pplatData);
	client->dev.platform_data = pplatData;

	if (!pplatData) {
		LOG_ERR("platform data is required!\n");
		return -EINVAL;
	}

	if (!i2c_check_functionality(client->adapter,
				I2C_FUNC_SMBUS_READ_WORD_DATA))
		return -EIO;

	/* create memory for main struct */
	this = kzalloc(sizeof(sx93XX_t), GFP_KERNEL);
	LOG_INFO("\t Initialized Main Memory: 0x%p\n", this);

	if (this) {
		/* In case we need to reinitialize data
		 * (e.q. if suspend reset device) */
		this->init = initialize;
		/* shortcut to read status of interrupt */
		this->refreshStatus = read_regStat;
		/* pointer to function from platform data to get pendown
		 * (1->NIRQ=0, 0->NIRQ=1) */
		this->get_nirq_low = pplatData->get_is_nirq_low;
		/* save irq in case we need to reference it */
		this->irq = gpio_to_irq(client->irq);
		/* do we need to create an irq timer after interrupt ? */
		this->useIrqTimer = 0;
		this->board = pplatData;
		/* Setup function to call on corresponding reg irq source bit */
		if (MAX_NUM_STATUS_BITS >= 8) {
			this->statusFunc[0] = 0; /* TXEN_STAT */
			this->statusFunc[1] = 0; /* UNUSED */
			this->statusFunc[2] = 0; /* UNUSED */
			this->statusFunc[3] = read_rawData; /* CONV_STAT */
			this->statusFunc[4] = 0; /* COMP_STAT */
			this->statusFunc[5] = touchProcess; /* RELEASE_STAT */
			this->statusFunc[6] = touchProcess; /* TOUCH_STAT  */
			this->statusFunc[7] = 0; /* RESET_STAT */
		}

		/* setup i2c communication */
		this->bus = client;
		i2c_set_clientdata(client, this);

		/* record device struct */
		this->pdev = &client->dev;

		/* create memory for device specific struct */
		this->pDevice = pDevice = kzalloc(sizeof(sx9310_t), GFP_KERNEL);
		LOG_INFO("\t Initialized Device Specific Memory: 0x%p\n",
				pDevice);
		sx9310_sar_ptr = this;
		if (pDevice) {
			/* for accessing items in user data (e.g. calibrate) */
			ret = sysfs_create_group(&client->dev.kobj,
						 &sx9310_attr_group);


			/* Check if we hava a platform
			 * initialization function to call*/
			if (pplatData->init_platform_hw)
				pplatData->init_platform_hw();

			/* Add Pointer to main platform data struct */
			pDevice->hw = pplatData;

			/* Initialize the button information
			 * initialized with keycodes */
			pDevice->pbuttonInformation =
						 pplatData->pbuttonInformation;

			/* Create the input device */
			input_top = input_allocate_device();
			if (!input_top)
				return -ENOMEM;

			/* Set all the keycodes */
			__set_bit(EV_ABS, input_top->evbit);
			input_set_abs_params(input_top, ABS_DISTANCE,
								 -1, 100, 0, 0);
			/* save the input pointer and finish initialization */
			pDevice->pbuttonInformation->input_top =
								 input_top;
			input_top->name = "SX9310 Cap Touch top";
			if (input_register_device(input_top)) {
				LOG_ERR("add top cap touch unsuccess\n");
				return -ENOMEM;
			}
			/* Create the input device */
			input_bottom = input_allocate_device();
			if (!input_bottom)
				return -ENOMEM;
			/* Set all the keycodes */
			__set_bit(EV_ABS, input_bottom->evbit);
			input_set_abs_params(input_bottom, ABS_DISTANCE,
								 -1, 100, 0, 0);
			/* save the input pointer and finish initialization */
			pDevice->pbuttonInformation->input_bottom =
								 input_bottom;
			/* save the input pointer and finish initialization */
			input_bottom->name = "SX9310 Cap Touch bottom";
			if (input_register_device(input_bottom)) {
				LOG_ERR("add bottom cap touch unsuccess\n");
				return -ENOMEM;
			}
		}

		ret = class_register(&capsense_class);
		if (ret < 0) {
			LOG_ERR("Create fsys class failed (%d)\n", ret);
			return ret;
		}

		ret = class_create_file(&capsense_class, &class_attr_reset);
		if (ret < 0) {
			LOG_ERR("Create reset file failed (%d)\n", ret);
			return ret;
		}

		ret = class_create_file(&capsense_class, &class_attr_enable);
		if (ret < 0) {
			LOG_ERR("Create enable file failed (%d)\n", ret);
			return ret;
		}

		ret = class_create_file(&capsense_class, &class_attr_reg);
		if (ret < 0) {
			LOG_ERR("Create reg file failed (%d)\n", ret);
			return ret;
		}
#ifdef USE_SENSORS_CLASS
		sensors_capsensor_top_cdev.sensors_enable =
							capsensor_set_enable;
		sensors_capsensor_top_cdev.sensors_poll_delay = NULL;
		ret = sensors_classdev_register(&input_top->dev,
						&sensors_capsensor_top_cdev);
		if (ret < 0)
			LOG_ERR("create top cap sensor_class  file \
				 failed (%d)\n", ret);
		sensors_capsensor_bottom_cdev.sensors_enable =
							 capsensor_set_enable;
		sensors_capsensor_bottom_cdev.sensors_poll_delay = NULL;
		ret = sensors_classdev_register(&input_bottom->dev,
					&sensors_capsensor_bottom_cdev);
		if (ret < 0)
			LOG_ERR("create bottom cap sensor_class file \
				 failed (%d)\n", ret);
#endif

		pplatData->cap_vdd = regulator_get(&client->dev, "cap_vdd");
		if (IS_ERR(pplatData->cap_vdd)) {
			if (PTR_ERR(pplatData->cap_vdd) == -EPROBE_DEFER) {
				ret = PTR_ERR(pplatData->cap_vdd);
				goto err_vdd_defer;
			}
			LOG_ERR("%s: Failed to get regulator\n",
					__func__);
		} else {
			int error = regulator_enable(pplatData->cap_vdd);

			if (error) {
				regulator_put(pplatData->cap_vdd);
				LOG_ERR("%s: Error %d enable regulator\n",
						__func__, error);
				return error;
			}
			pplatData->cap_vdd_en = true;
			LOG_INFO("cap_vdd regulator is %s\n",
				regulator_is_enabled(pplatData->cap_vdd) ?
				"on" : "off");
		}

		pplatData->cap_svdd = regulator_get(&client->dev, "cap_svdd");
		if (!IS_ERR(pplatData->cap_svdd)) {
			ret = regulator_enable(pplatData->cap_svdd);
			if (ret) {
				regulator_put(pplatData->cap_svdd);
				LOG_ERR("Failed to enable cap_svdd\n");
				goto err_svdd_error;
			}
			pplatData->cap_svdd_en = true;
			LOG_INFO("cap_svdd regulator is %s\n",
				regulator_is_enabled(pplatData->cap_svdd) ?
				"on" : "off");
		} else {
			ret = PTR_ERR(pplatData->cap_vdd);
			if (ret == -EPROBE_DEFER)
				goto err_svdd_error;
		}
		sx93XX_sar_init(this);

		write_register(this, SX9310_CPS_CTRL0_REG, 0x10);
		write_register(this, SX9310_IRQ_ENABLE_REG, 0x00);
		mEnabled = 0;

		INIT_WORK(&this->ps_notify_work, ps_notify_callback_work);
		this->ps_notif.notifier_call = ps_notify_callback;
		ret = power_supply_reg_notifier(&this->ps_notif);
		if (ret) {
			LOG_ERR(
				"Unable to register ps_notifier: %d\n", ret);
			goto free_ps_notifier;
		}

		psy = power_supply_get_by_name("usb");
		if (psy) {
			ret = ps_get_state(psy, &this->ps_is_present);
			if (ret) {
				LOG_ERR(
					"psy get property failed rc=%d\n",
					ret);
				goto free_ps_notifier;
			}
		}

		return  0;
	}
	return -ENOMEM;

free_ps_notifier:
	power_supply_unreg_notifier(&this->ps_notif);

err_svdd_error:
	LOG_ERR("%s svdd defer.\n", __func__);
	regulator_disable(pplatData->cap_vdd);
	regulator_put(pplatData->cap_vdd);

err_vdd_defer:
	LOG_ERR("%s input free device.\n", __func__);
	input_free_device(input_top);
	input_free_device(input_bottom);

	return ret;
}

/**
 * fn static int sx9310_remove(struct i2c_client *client)
 * brief Called when device is to be removed
 * param client Pointer to i2c_client struct
 * return Value from sx93XX_sar_remove()
 */
static int sx9310_remove(struct i2c_client *client)
{
	psx9310_platform_data_t pplatData = 0;
	psx9310_t pDevice = 0;
	psx93XX_t this = i2c_get_clientdata(client);

	pDevice = this->pDevice;
	if (this && pDevice) {
#ifdef USE_SENSORS_CLASS
		sensors_classdev_unregister(&sensors_capsensor_top_cdev);
		sensors_classdev_unregister(&sensors_capsensor_bottom_cdev);
#endif
		input_unregister_device(pDevice->pbuttonInformation->input_top);
		input_unregister_device(
				pDevice->pbuttonInformation->input_bottom);

		if (this->board->cap_svdd_en) {
			regulator_disable(this->board->cap_svdd);
			regulator_put(this->board->cap_svdd);
		}

		if (this->board->cap_vdd_en) {
			regulator_disable(this->board->cap_vdd);
			regulator_put(this->board->cap_vdd);
		}
#ifdef USE_SENSORS_CLASS
		sensors_classdev_unregister(&sensors_capsensor_top_cdev);
		sensors_classdev_unregister(&sensors_capsensor_bottom_cdev);
#endif
		sysfs_remove_group(&client->dev.kobj, &sx9310_attr_group);
		pplatData = client->dev.platform_data;
		if (pplatData && pplatData->exit_platform_hw)
			pplatData->exit_platform_hw();
		kfree(this->pDevice);
	}
	return sx93XX_sar_remove(this);
}
#if defined(USE_KERNEL_SUSPEND)
/*====================================================*/
/***** Kernel Suspend *****/
static int sx9310_suspend(struct i2c_client *client, pm_message_t mesg)
{
	psx93XX_t this = i2c_get_clientdata(client);

	sx93XX_sar_suspend(this);
	return 0;
}
/***** Kernel Resume *****/
static int sx9310_resume(struct i2c_client *client)
{
	psx93XX_t this = i2c_get_clientdata(client);

	sx93XX_sar_resume(this);
	return 0;
}
/*====================================================*/
#endif

#ifdef CONFIG_OF
static const struct of_device_id synaptics_rmi4_match_tbl[] = {
	{ .compatible = "semtech," DRIVER_NAME },
	{ },
};
MODULE_DEVICE_TABLE(of, synaptics_rmi4_match_tbl);
#endif

static struct i2c_device_id sx9310_idtable[] = {
	{ DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sx9310_idtable);

static struct i2c_driver sx9310_driver = {
	.driver = {
		.owner  = THIS_MODULE,
		.name   = DRIVER_NAME
	},
	.id_table = sx9310_idtable,
	.probe	  = sx9310_probe,
	.remove	  = sx9310_remove,
#if defined(USE_KERNEL_SUSPEND)
	.suspend  = sx9310_suspend,
	.resume   = sx9310_resume,
#endif
};
static int __init sx9310_init(void)
{
	return i2c_add_driver(&sx9310_driver);
}
static void __exit sx9310_exit(void)
{
	i2c_del_driver(&sx9310_driver);
}

module_init(sx9310_init);
module_exit(sx9310_exit);

MODULE_AUTHOR("Semtech Corp. (http://www.semtech.com/)");
MODULE_DESCRIPTION("SX9310 Capacitive Touch Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("0.1");

#ifdef USE_THREADED_IRQ
static void sx93XX_process_interrupt(psx93XX_t this, u8 nirqlow)
{
	int status = 0;
	int counter = 0;

	if (!this) {
		LOG_ERR("sx93XX_worker_func, NULL sx93XX_t\n");
		return;
	}
	/* since we are not in an interrupt don't need to disable irq. */
	status = this->refreshStatus(this);
	counter = -1;
	if (sx9310_debug_enable)
		LOG_DBG("Worker - Refresh Status %d\n", status);

	while ((++counter) < MAX_NUM_STATUS_BITS) { /* counter start from MSB */
		if (((status >> counter) & 0x01)
			&& (this->statusFunc[counter])) {
			if (sx9310_debug_enable)
				LOG_DBG("Function %d Pointer Found. Calling\n"
					,counter);
			this->statusFunc[counter](this);
		}
	}
	if (unlikely(this->useIrqTimer && nirqlow)) {
		/* In case we need to send a timer for example on a touchscreen
		 * checking penup, perform this here
		 */
		cancel_delayed_work(&this->dworker);
		schedule_delayed_work(&this->dworker,
				msecs_to_jiffies(this->irqTimeout));
		LOG_INFO("Schedule Irq timer");
	}
}


static void sx93XX_worker_func(struct work_struct *work)
{
	psx93XX_t this = 0;

	if (work) {
		this = container_of(work, sx93XX_t, dworker.work);
		if (!this) {
			LOG_ERR("sx93XX_worker_func, NULL sx93XX_t\n");
			return;
		}
		if ((!this->get_nirq_low)
			|| (!this->get_nirq_low(this->board->irq_gpio))) {
			/* only run if nirq is high */
			sx93XX_process_interrupt(this, 0);
		}
	} else {
		LOG_ERR("sx93XX_worker_func, NULL work_struct\n");
	}
}
static irqreturn_t sx93XX_interrupt_thread(int irq, void *data)
{
	psx93XX_t this = 0;

	this = data;

	mutex_lock(&this->mutex);
	if (sx9310_debug_enable)
		LOG_DBG("sx93XX_irq\n");
	if ((!this->get_nirq_low) || this->get_nirq_low(this->board->irq_gpio))
		sx93XX_process_interrupt(this, 1);
	else {
		if (sx9310_debug_enable)
			LOG_DBG("sx93XX_irq - nirq read high\n");
	}
	mutex_unlock(&this->mutex);
	return IRQ_HANDLED;
}
#else
static void sx93XX_schedule_work(psx93XX_t this, unsigned long delay)
{
	unsigned long flags;

	if (this) {
		LOG_INFO("sx93XX_schedule_work()\n");
		spin_lock_irqsave(&this->lock, flags);
		/* Stop any pending penup queues */
		cancel_delayed_work(&this->dworker);
		/*
		 * after waiting for a delay, this put the job
		 * in the kernel-global workqueue.
		 * so no need to create new thread in work queue.
		 */
		schedule_delayed_work(&this->dworker, delay);
		spin_unlock_irqrestore(&this->lock, flags);
	} else
		LOG_ERR("sx93XX_schedule_work, NULL psx93XX_t\n");
}

static irqreturn_t sx93XX_irq(int irq, void *pvoid)
{
	psx93XX_t this = 0;

	if (pvoid) {
		this = (psx93XX_t)pvoid;
		if (sx9310_debug_enable)
			LOG_DBG("sx93XX_irq\n");
		if ((!this->get_nirq_low)
			|| this->get_nirq_low(this->board->irq_gpio)) {
			if (sx9310_debug_enable)
				LOG_DBG("sx93XX_irq - Schedule Work\n");
			sx93XX_schedule_work(this, 0);
		} else {
			if (sx9310_debug_enable)
				LOG_DBG("sx93XX_irq - nirq read high\n");
		}
	} else
		LOG_ERR("sx93XX_irq, NULL pvoid\n");
	return IRQ_HANDLED;
}

static void sx93XX_worker_func(struct work_struct *work)
{
	psx93XX_t this = 0;
	int status = 0;
	int counter = 0;
	u8 nirqLow = 0;

	if (work) {
		this = container_of(work, sx93XX_t, dworker.work);

		if (!this) {
			LOG_ERR("sx93XX_worker_func, NULL sx93XX_t\n");
			return;
		}
		if (unlikely(this->useIrqTimer)) {
			if ((!this->get_nirq_low)
				|| this->get_nirq_low(this->board->irq_gpio))
				nirqLow = 1;
		}
		/* since we are not in an interrupt
		 * don't need to disable irq. */
		status = this->refreshStatus(this);
		counter = -1;
		if (sx9310_debug_enable)
			LOG_DBG("Worker - Refresh Status %d\n", status);
		/* counter start from MSB */
		while ((++counter) < MAX_NUM_STATUS_BITS) {
			if (((status >> counter) & 0x01)
				&& (this->statusFunc[counter])) {
				LOG_INFO("Function %d Pointer Found. Calling\n"
					 ,counter);
				this->statusFunc[counter](this);
			}
		}
		if (unlikely(this->useIrqTimer && nirqLow)) {
			/* Early models and if RATE=0 for newer models
			 * require a penup timer;
			 * Queue up the function again for checking on penup */
			sx93XX_schedule_work(this,
					msecs_to_jiffies(this->irqTimeout));
		}
	} else {
		LOG_ERR("sx93XX_worker_func, NULL work_struct\n");
	}
}
#endif

void sx93XX_sar_suspend(psx93XX_t this)
{
	if (this) {
		disable_irq(this->irq);
		write_register(this, SX9310_CPS_CTRL0_REG, 0x10);
		write_register(this, SX9310_IRQ_ENABLE_REG, 0x00);
	}
}
void sx93XX_sar_resume(psx93XX_t this)
{
	if (this) {
#ifdef USE_THREADED_IRQ
		mutex_lock(&this->mutex);
		/* Just in case need to reset any uncaught interrupts */
		sx93XX_process_interrupt(this, 0);
		mutex_unlock(&this->mutex);
#else
		sx93XX_schedule_work(this, 0);
#endif
		if (this->init)
			this->init(this);
		enable_irq(this->irq);
	}
}

int sx93XX_sar_init(psx93XX_t this)
{
	int err = 0;

	if (this && this->pDevice) {
#ifdef USE_THREADED_IRQ

		/* initialize worker function */
		INIT_DELAYED_WORK(&this->dworker, sx93XX_worker_func);


		/* initialize mutex */
		mutex_init(&this->mutex);
		/* initailize interrupt reporting */
		this->irq_disabled = 0;
		err = request_threaded_irq(this->irq, NULL,
				sx93XX_interrupt_thread,
				IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
				this->pdev->driver->name,
				this);
#else
		/* initialize spin lock */
		spin_lock_init(&this->lock);

		/* initialize worker function */
		INIT_DELAYED_WORK(&this->dworker, sx93XX_worker_func);

		/* initailize interrupt reporting */
		this->irq_disabled = 0;
		err = request_irq(this->irq, sx93XX_irq, IRQF_TRIGGER_FALLING,
				this->pdev->driver->name, this);
#endif
		if (err) {
			LOG_ERR("irq %d busy?\n", this->irq);
			return err;
		}
#ifdef USE_THREADED_IRQ
		LOG_INFO("registered with threaded irq (%d)\n", this->irq);
#else
		LOG_INFO("registered with irq (%d)\n", this->irq);
#endif
		/* call init function pointer:
		 * this should initialize all registers */
		if (this->init)
			return this->init(this);
		LOG_ERR("No init function!!!!\n");
	}
	return -ENOMEM;
}

int sx93XX_sar_remove(psx93XX_t this)
{
	if (this) {
		/* Cancel the Worker Func */
		cancel_delayed_work_sync(&this->dworker);
		free_irq(this->irq, this);
		kfree(this);
		return 0;
	}
	return -ENOMEM;
}
