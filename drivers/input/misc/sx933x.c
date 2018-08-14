/*! \file sx933x.c
 * \brief  SX933x Driver
 *
 * Driver for the SX933x
 * Copyright (c) 2018 Semtech Corp
 *
 *  This program is free software; you can redistribute it and/or modify
 *  it under the terms of the GNU General Public License version 2 as
 *  published by the Free Software Foundation.
 */
#define DRIVER_NAME "sx933x"

#define MAX_WRITE_ARRAY_SIZE 32

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/init.h>
#include <linux/delay.h>
#include <linux/input.h>
#include <linux/fs.h>
#include <linux/device.h>
#include <linux/interrupt.h>
#include <linux/syscalls.h>
//#include <linux/wakelock.h>
#include <linux/uaccess.h>
#include <linux/sort.h>
#include <linux/gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/of_gpio.h>
#include <linux/notifier.h>
#include <linux/usb.h>
#include <linux/power_supply.h>
#include <linux/sensors.h>
#include <linux/input/sx933x.h> 	/* main struct, interrupt,init,pointers */


#define SX933x_DEBUG 0
#define LOG_TAG "[sar SX933x]: "

#if SX933x_DEBUG
#define LOG_INFO(fmt, args...)    pr_info(LOG_TAG fmt, ##args)
#else
#define LOG_INFO(fmt, args...)
#endif
#define LOG_DBG(fmt, args...)   pr_info(LOG_TAG fmt, ##args) 

#define USE_DTS_REG

#define SX933x_I2C_M_WR                 0 /* for i2c Write */
#define SX933x_I2C_M_RD                 1 /* for i2c Read */

#define IDLE			    0
#define PROXACTIVE			  1
#define BODYACTIVE			  2

#define MAIN_SENSOR		1 //CS1

/* Failer Index */
#define SX933x_ID_ERROR 	1
#define SX933x_NIRQ_ERROR	2
#define SX933x_CONN_ERROR	3
#define SX933x_I2C_ERROR	4

/*! \struct sx933x
 * Specialized struct containing input event data, platform data, and
 * last cap state read if needed.
 */
typedef struct sx933x
{
	pbuttonInformation_t pbuttonInformation;
	psx933x_platform_data_t hw;		/* specific platform data settings */
} sx933x_t, *psx933x_t;

static int irq_gpio_num;
static psx93XX_t global_sx933x;

static int sx933x_get_nirq_state(void)
{
	return  !gpio_get_value(irq_gpio_num);
}


/*! \fn static int sx933x_i2c_write_16bit(psx93XX_t this, u8 address, u8 value)
 * \brief Sends a write register to the device
 * \param this Pointer to main parent struct
 * \param address 8-bit register address
 * \param value   8-bit register value to write to address
 * \return Value from i2c_master_send
 */

static int sx933x_i2c_write_16bit(psx93XX_t this, u16 reg_addr, u32 buf)
{
	int ret =  -ENOMEM;
	struct i2c_client *i2c = 0;
	struct i2c_msg msg;
	unsigned char w_buf[6];

	if (this && this->bus)
	{
		i2c = this->bus;
		w_buf[0] = (u8)(reg_addr>>8);
		w_buf[1] = (u8)(reg_addr);
		w_buf[2] = (u8)(buf>>24);
		w_buf[3] = (u8)(buf>>16);
		w_buf[4] = (u8)(buf>>8);
		w_buf[5] = (u8)(buf);

		msg.addr = i2c->addr;
		msg.flags = SX933x_I2C_M_WR;
		msg.len = 6; //2bytes regaddr + 4bytes data
		msg.buf = (u8 *)w_buf;

		ret = i2c_transfer(i2c->adapter, &msg, 1);
		if (ret < 0)
			LOG_DBG("%s - i2c write error %d\n", __func__, ret);

	}
	return ret;
}



/*! \fn static int sx933x_i2c_read_16bit(psx93XX_t this, u8 address, u8 *value)
 * \brief Reads a register's value from the device
 * \param this Pointer to main parent struct
 * \param address 8-Bit address to read from
 * \param value Pointer to 8-bit value to save register value to
 * \return Value from i2c_smbus_read_byte_data if < 0. else 0
 */
static int sx933x_i2c_read_16bit(psx93XX_t this, u16 reg_addr, u32 *data32)
{
	int ret =  -ENOMEM;
	struct i2c_client *i2c = 0;
	struct i2c_msg msg[2];
	u8 w_buf[2];
	u8 buf[4];

	if (this && this->bus)
	{
		i2c = this->bus;

		w_buf[0] = (u8)(reg_addr>>8);
		w_buf[1] = (u8)(reg_addr);

		msg[0].addr = i2c->addr;
		msg[0].flags = SX933x_I2C_M_WR;
		msg[0].len = 2;
		msg[0].buf = (u8 *)w_buf;

		msg[1].addr = i2c->addr;;
		msg[1].flags = SX933x_I2C_M_RD;
		msg[1].len = 4;
		msg[1].buf = (u8 *)buf;

		ret = i2c_transfer(i2c->adapter, msg, 2);
		if (ret < 0)
			LOG_DBG("%s - i2c read error %d\n", __func__, ret);

		data32[0] = ((u32)buf[0]<<24) | ((u32)buf[1]<<16) | ((u32)buf[2]<<8) | ((u32)buf[3]);

	}
	return ret;
}




//static int sx933x_set_mode(psx93XX_t this, unsigned char mode);

/*! \fn static int read_regStat(psx93XX_t this)
 * \brief Shortcut to read what caused interrupt.
 * \details This is to keep the drivers a unified
 * function that will read whatever register(s)
 * provide information on why the interrupt was caused.
 * \param this Pointer to main parent struct
 * \return If successful, Value of bit(s) that cause interrupt, else 0
 */
static int read_regStat(psx93XX_t this)
{
	u32 data = 0;
	if (this)
	{
		if (sx933x_i2c_read_16bit(this,SX933X_HOSTIRQSRC_REG,&data) > 0) //bug
			return (data & 0x00FF);
	}
	return 0;
}

static int sx933x_Hardware_Check(psx93XX_t this)
{
	int ret;
	u32 idCode;
	u8 loop = 0;
	this->failStatusCode = 0;

	//Check th IRQ Status
	while(this->get_nirq_low && this->get_nirq_low())
	{
		read_regStat(this);
		msleep(100);
		if(++loop >10)
		{
			this->failStatusCode = SX933x_NIRQ_ERROR;
			break;
		}
	}

	//Check I2C Connection
	ret = sx933x_i2c_read_16bit(this, SX933X_INFO_REG, &idCode);
	if(ret < 0)
	{
		this->failStatusCode = SX933x_I2C_ERROR;
	}

	if(idCode!= SX933X_WHOAMI_VALUE)
	{
		this->failStatusCode = SX933x_ID_ERROR;
	}

	LOG_DBG("sx933x idcode = 0x%x, failcode = 0x%x\n", idCode, this->failStatusCode);
	return (int)this->failStatusCode;
}

static int sx933x_global_variable_init(psx93XX_t this)
{
	this->irq_disabled = 0;
	this->failStatusCode = 0;
	this->reg_in_dts = true;
	return 0;
}

/*! \brief Perform a manual offset calibration
 * \param this Pointer to main parent struct
 * \return Value return value from the write register
 */
static int manual_offset_calibration(psx93XX_t this)
{
	int ret = 0;
	ret = sx933x_i2c_write_16bit(this, SX933X_CMD_REG, I2C_REGCMD_COMPEN);
	return ret;

}

/****************************************************************************/
/*! \brief sysfs show function for manual calibration which currently just
 * returns register value.
 */
static void read_rawData(psx93XX_t this)
{
	u8 csx, index;
	s32 useful;
	s32 average;
	s32 diff;
	u32 uData;
	u16 offset;
	//s32 state = 0;

	if(this)
	{
		for(csx =0; csx<5; csx++)
		{
			index = csx*4;
			sx933x_i2c_read_16bit(this, SX933X_USEPH0_REG + index, &uData);
			useful = (s32)uData>>10;
			sx933x_i2c_read_16bit(this, SX933X_AVGPH0_REG + index, &uData);
			average = (s32)uData>>10;
			sx933x_i2c_read_16bit(this, SX933X_DIFFPH0_REG + index, &uData);
			diff = (s32)uData>>10;
			sx933x_i2c_read_16bit(this, SX933X_OFFSETPH0_REG + index*2, &uData);
			offset = (u16)(uData & 0x7FFF);
			//state = psmtcButtons[csx].state;
			LOG_DBG("[PH: %d] Useful = %d Average = %d, DIFF = %d Offset = %d \n",
					csx,useful,average,diff,offset);
		}
	}
}

static ssize_t capsense_reset_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	u32 temp = 0;
	sx933x_i2c_read_16bit(global_sx933x, SX933X_GNRLCTRL2_REG, &temp);
	if (!count)
		return -EINVAL;

	if (!strncmp(buf, "reset", 5) || !strncmp(buf, "1", 1)) {
		if (temp & 0x0000001F) {
			LOG_DBG("Going to refresh baseline\n");
			manual_offset_calibration(global_sx933x);
		}
	}

	return count;
}

static ssize_t capsense_raw_data_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	char *p = buf;
	int csx;
	s32 useful, average, diff;
	u32 uData;
	u16 offset;

	psx93XX_t this = global_sx933x;
	if(this) {
		for(csx =0; csx<5; csx++) {
			sx933x_i2c_read_16bit(this, SX933X_USEPH0_REG + csx*4, &uData);
			useful = (s32)uData>>10;
			sx933x_i2c_read_16bit(this, SX933X_AVGPH0_REG + csx*4, &uData);
			average = (s32)uData>>10;
			sx933x_i2c_read_16bit(this, SX933X_DIFFPH0_REG + csx*4, &uData);
			diff = (s32)uData>>10;
			sx933x_i2c_read_16bit(this, SX933X_OFFSETPH0_REG + csx*8, &uData);
			offset = (u16)(uData & 0x7FFF);
			p += snprintf(p, PAGE_SIZE, "[PH: %d] Useful = %d, Average = %d, DIFF = %d Offset = %d \n",
					csx,useful,average,diff,offset);
		}
	}
	return (p-buf);
}

static ssize_t sx933x_register_write_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	u32 reg_address = 0, val = 0;
	psx93XX_t this = global_sx933x;

	if (sscanf(buf, "%x,%x", &reg_address, &val) != 2)
	{
		LOG_DBG("%s - The number of data are wrong\n",__func__);
		return -EINVAL;
	}

	sx933x_i2c_write_16bit(this, reg_address, val);

	LOG_DBG("%s - Register(0x%x) data(0x%x)\n",__func__, reg_address, val);
	return count;
}

static ssize_t sx933x_register_read_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	u32 val=0;
	int regist = 0;
	int nirq_state = 0;
	psx93XX_t this = global_sx933x;

	if (sscanf(buf, "%x", &regist) != 1)
	{
		LOG_DBG("%s - The number of data are wrong\n",__func__);
		return -EINVAL;
	}

	sx933x_i2c_read_16bit(this, regist, &val);
	nirq_state = sx933x_get_nirq_state();

	LOG_DBG("%s - Register(0x%2x) data(0x%4x) nirq_state(%d)\n",__func__, regist, val, nirq_state);
	return count;
}

static ssize_t manual_offset_calibration_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	u32 reg_value = 0;
	psx93XX_t this = global_sx933x;

	LOG_DBG("Reading IRQSTAT_REG\n");
	sx933x_i2c_read_16bit(this,SX933X_HOSTIRQSRC_REG,&reg_value);
	return sprintf(buf, "%d\n", reg_value);
}


static ssize_t manual_offset_calibration_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	unsigned long val;
	psx93XX_t this = global_sx933x;

	if (kstrtoul(buf, 10, &val))                //(strict_strtoul(buf, 10, &val)) {
	{
		LOG_DBG(" %s - Invalid Argument\n", __func__);
		return -EINVAL;
	}

	if (val)
		manual_offset_calibration(this);

	return count;
}
static struct class_attribute capsense_class_attributes[] = {
	__ATTR(reset, 0660, NULL, capsense_reset_store),
	__ATTR(raw_data, 0660, capsense_raw_data_show, NULL),
	__ATTR(register_write,  0660, NULL,sx933x_register_write_store),
	__ATTR(register_read,0660, NULL,sx933x_register_read_store),
	__ATTR(manual_calibrate, 0660, manual_offset_calibration_show,manual_offset_calibration_store),
	__ATTR_NULL,
};

struct class capsense_class = {
	.name                   = "capsense",
	.owner                  = THIS_MODULE,
	.class_attrs		= capsense_class_attributes,
};

/**************************************/

/*! \brief  Initialize I2C config from platform data
 * \param this Pointer to main parent struct
 */
static void sx933x_reg_init(psx93XX_t this)
{
	psx933x_t pDevice = 0;
	psx933x_platform_data_t pdata = 0;
	int i = 0;
	uint32_t tmpvalue;
	/* configure device */
	if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw))
	{
		/*******************************************************************************/
		// try to initialize from device tree!
		/*******************************************************************************/
		while ( i < ARRAY_SIZE(sx933x_i2c_reg_setup))
		{
			/* Write all registers/values contained in i2c_reg */
			LOG_DBG("Going to Write Reg: 0x%x Value: 0x%x\n",
					sx933x_i2c_reg_setup[i].reg,sx933x_i2c_reg_setup[i].val);
			tmpvalue = sx933x_i2c_reg_setup[i].val;
			if (sx933x_i2c_reg_setup[i].reg == SX933X_GNRLCTRL2_REG)
			{
				if((sx933x_i2c_reg_setup[i].val & 0x3F) == 0)
				{
					tmpvalue = (sx933x_i2c_reg_setup[i].val|0x3F);
				}
			}
			sx933x_i2c_write_16bit(this, sx933x_i2c_reg_setup[i].reg, tmpvalue);
			i++;
		}
#ifdef USE_DTS_REG
		if (this->reg_in_dts == true)
		{
			i = 0;
			while ( i < pdata->i2c_reg_num)
			{
				/* Write all registers/values contained in i2c_reg */
				LOG_DBG("Going to Write Reg from dts: 0x%x Value: 0x%x\n",
						pdata->pi2c_reg[i].reg,pdata->pi2c_reg[i].val);
				sx933x_i2c_write_16bit(this, pdata->pi2c_reg[i].reg,pdata->pi2c_reg[i].val);
				i++;
			}
		}
#endif
		/*******************************************************************************/
		sx933x_i2c_write_16bit(this, SX933X_CMD_REG,SX933X_PHASE_CONTROL);  //enable phase control
	}
	else {
		LOG_DBG("ERROR! platform data 0x%p\n",pDevice->hw);
	}

}


/*! \fn static int initialize(psx93XX_t this)
 * \brief Performs all initialization needed to configure the device
 * \param this Pointer to main parent struct
 * \return Last used command's return value (negative if error)
 */
static int initialize(psx93XX_t this)
{
	int ret;
	if (this)
	{
		LOG_DBG("SX933x income initialize\n");
		/* prepare reset by disabling any irq handling */
		this->irq_disabled = 1;
		disable_irq(this->irq);
		/* perform a reset */
		sx933x_i2c_write_16bit(this, SX933X_RESET_REG, I2C_SOFTRESET_VALUE);
		/* wait until the reset has finished by monitoring NIRQ */
		LOG_DBG("Sent Software Reset. Waiting until device is back from reset to continue.\n");
		/* just sleep for awhile instead of using a loop with reading irq status */
		msleep(100);
		ret = sx933x_global_variable_init(this);
		sx933x_reg_init(this);
		msleep(100); /* make sure everything is running */
		manual_offset_calibration(this);

		/* re-enable interrupt handling */
		enable_irq(this->irq);

		/* make sure no interrupts are pending since enabling irq will only
		 * work on next falling edge */
		read_regStat(this);
		return 0;
	}
	return -ENOMEM;
}

/*!
 * \brief Handle what to do when a touch occurs
 * \param this Pointer to main parent struct
 */
static void touchProcess(psx93XX_t this)
{
	int counter = 0;
	u32 i = 0;
	u32 touchFlag = 0;
	int numberOfButtons = 0;
	psx933x_t pDevice = NULL;
	struct _buttonInfo *buttons = NULL;
	struct input_dev *input = NULL;

	struct _buttonInfo *pCurrentButton  = NULL;

	if (this && (pDevice = this->pDevice))
	{
		sx933x_i2c_read_16bit(this, SX933X_STAT0_REG, &i);
		LOG_DBG("touchProcess STAT0_REG:0x%08x\n", i);

		buttons = pDevice->pbuttonInformation->buttons;
		numberOfButtons = pDevice->pbuttonInformation->buttonSize;

		if (unlikely( buttons==NULL ))
		{
			LOG_DBG(":ERROR!! buttons NULL!!!\n");
			return;
		}

		for (counter = 0; counter < numberOfButtons; counter++)
		{
			if (buttons[counter].enabled == false) {
				LOG_INFO("touchProcess %s disabled, ignor this\n", buttons[counter].name);
				continue;
			}
			if (buttons[counter].used== false) {
				LOG_INFO("touchProcess %s unused, ignor this\n", buttons[counter].name);
				continue;
			}
			pCurrentButton = &buttons[counter];			
			if (pCurrentButton==NULL)
			{
				LOG_DBG("ERROR!! current button at index: %d NULL!!!\n", counter);
				return; // ERRORR!!!!
			}
			input = pCurrentButton->input_dev;
			if (input==NULL)
			{
				LOG_DBG("ERROR!! current button input at index: %d NULL!!!\n", counter);
				return; // ERRORR!!!!
			}

			touchFlag = i & (pCurrentButton->ProxMask | pCurrentButton->BodyMask);
			if (touchFlag == (pCurrentButton->ProxMask | pCurrentButton->BodyMask)) {
				if (pCurrentButton->state == BODYACTIVE)
					LOG_DBG(" %s already BODYACTIVE\n", pCurrentButton->name);
				else {
					input_report_abs(input, ABS_DISTANCE, 2);
					input_sync(input);
					pCurrentButton->state = BODYACTIVE;
					LOG_DBG(" %s report 5mm BODYACTIVE\n", pCurrentButton->name);
				}
			} else if (touchFlag == pCurrentButton->ProxMask) {
				if (pCurrentButton->state == PROXACTIVE)
					LOG_DBG(" %s already PROXACTIVE\n", pCurrentButton->name);
				else {
					input_report_abs(input, ABS_DISTANCE, 1);
					input_sync(input);
					pCurrentButton->state = PROXACTIVE;
					LOG_DBG(" %s report 15mm PROXACTIVE\n", pCurrentButton->name);
				}
			}else if (touchFlag == 0) {
				if (pCurrentButton->state == IDLE)
					LOG_DBG("%s already released.\n", pCurrentButton->name);
				else {
					input_report_abs(input, ABS_DISTANCE, 0);
					input_sync(input);
					pCurrentButton->state = IDLE;
					LOG_DBG("%s report  released.\n", pCurrentButton->name);
				}
			}
		}
		LOG_INFO("Leaving touchProcess()\n");
	}
}

static int sx933x_parse_dt(struct sx933x_platform_data *pdata, struct device *dev)
{
	struct device_node *dNode = dev->of_node;
	enum of_gpio_flags flags;

	if (dNode == NULL)
		return -ENODEV;

	pdata->irq_gpio= of_get_named_gpio_flags(dNode,
			"Semtech,nirq-gpio", 0, &flags);
	irq_gpio_num = pdata->irq_gpio;
	if (pdata->irq_gpio < 0)
	{
		LOG_DBG("%s - get irq_gpio error\n", __func__);
		return -ENODEV;
	}

	pdata->button_used_flag = 0;
	of_property_read_u32(dNode,"Semtech,button-flag",&pdata->button_used_flag);
	LOG_INFO("%s -  used button 0x%x \n", __func__, pdata->button_used_flag);

#ifdef USE_DTS_REG
	// load in registers from device tree
	of_property_read_u32(dNode,"Semtech,reg-num",&pdata->i2c_reg_num);
	// layout is register, value, register, value....
	// if an extra item is after just ignore it. reading the array in will cause it to fail anyway
	LOG_INFO("%s -  size of elements %d \n", __func__,pdata->i2c_reg_num);
	if (pdata->i2c_reg_num > 0)
	{
		// initialize platform reg data array
		pdata->pi2c_reg = devm_kzalloc(dev,sizeof(struct smtc_reg_data)*pdata->i2c_reg_num, GFP_KERNEL);
		if (unlikely(pdata->pi2c_reg == NULL))
		{
			LOG_DBG("%s -  size of elements %d alloc error\n", __func__,pdata->i2c_reg_num);
			return -ENOMEM;
		}

		// initialize the array
		if (of_property_read_u32_array(dNode,"Semtech,reg-init",(u32*)&(pdata->pi2c_reg[0]),sizeof(struct smtc_reg_data)*pdata->i2c_reg_num/sizeof(u32)))
			return -ENOMEM;
	}
#endif
	LOG_INFO("%s -[%d] parse_dt complete\n", __func__,pdata->irq_gpio);
	return 0;
}

/* get the NIRQ state (1->NIRQ-low, 0->NIRQ-high) */
static int sx933x_init_platform_hw(struct i2c_client *client)
{
	psx93XX_t this = i2c_get_clientdata(client);
	struct sx933x *pDevice = NULL;
	struct sx933x_platform_data *pdata = NULL;

	int rc = 0;

	LOG_INFO("%s init_platform_hw start!",__func__);

	if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw))
	{
		if (gpio_is_valid(pdata->irq_gpio))
		{
			rc = gpio_request(pdata->irq_gpio, "sx933x_irq_gpio");
			if (rc < 0)
			{
				LOG_DBG("SX933x Request gpio. Fail![%d]\n", rc);
				return rc;
			}
			rc = gpio_direction_input(pdata->irq_gpio);
			if (rc < 0)
			{
				LOG_DBG("SX933x Set gpio direction. Fail![%d]\n", rc);
				return rc;
			}
			this->irq = client->irq = gpio_to_irq(pdata->irq_gpio);
		}
		else
		{
			LOG_DBG("SX933x Invalid irq gpio num.(init)\n");
		}
	}
	else
	{
		LOG_DBG("%s - Do not init platform HW", __func__);
	}
	return rc;
}

static void sx933x_exit_platform_hw(struct i2c_client *client)
{
	psx93XX_t this = i2c_get_clientdata(client);
	struct sx933x *pDevice = NULL;
	struct sx933x_platform_data *pdata = NULL;

	if (this && (pDevice = this->pDevice) && (pdata = pDevice->hw))
	{
		if (gpio_is_valid(pdata->irq_gpio))
		{
			gpio_free(pdata->irq_gpio);
		}
		else
		{
			LOG_DBG("Invalid irq gpio num.(exit)\n");
		}
	}
	return;
}

static int capsensor_set_enable(struct sensors_classdev *sensors_cdev,
		unsigned int enable)
{
	bool disableFlag = true;
	int i = 0;
	u32 temp = 0x0;

	for (i = 0; i < ARRAY_SIZE(psmtcButtons); i++) {
		if (strcmp(sensors_cdev->name, psmtcButtons[i].name) == 0) {
			if (enable == 1) {
				LOG_DBG("enable cap sensor : %s\n", sensors_cdev->name);
				sx933x_i2c_read_16bit(global_sx933x, SX933X_GNRLCTRL2_REG, &temp);
				temp = temp | 0x0000001F;
				LOG_INFO("set reg 0x%x val 0x%x\n", SX933X_GNRLCTRL2_REG, temp);
				sx933x_i2c_write_16bit(global_sx933x, SX933X_GNRLCTRL2_REG, temp);
				psmtcButtons[i].enabled = true;
				input_report_abs(psmtcButtons[i].input_dev, ABS_DISTANCE, 0);
				input_sync(psmtcButtons[i].input_dev);

				manual_offset_calibration(global_sx933x);
			} else if (enable == 0) {
				LOG_DBG("disable cap sensor : %s\n", sensors_cdev->name);
				psmtcButtons[i].enabled = false;
				input_report_abs(psmtcButtons[i].input_dev, ABS_DISTANCE, -1);
				input_sync(psmtcButtons[i].input_dev);
			} else {
				LOG_DBG("unknown enable symbol\n");
			}
		}
	}

	//if all chs disabled, then disable all
	for (i = 0; i < ARRAY_SIZE(psmtcButtons); i++) {
		if (psmtcButtons[i].enabled) {
			disableFlag = false;
			break;
		}
	}
	if (disableFlag) {
		LOG_DBG("disable all chs\n");
		sx933x_i2c_read_16bit(global_sx933x, SX933X_GNRLCTRL2_REG, &temp);
		LOG_INFO("read reg 0x%x val 0x%x\n", SX933X_GNRLCTRL2_REG, temp);
		temp = temp & 0xFFFFFFE0;
		LOG_INFO("set reg 0x%x val 0x%x\n", SX933X_GNRLCTRL2_REG, temp);
		sx933x_i2c_write_16bit(global_sx933x, SX933X_GNRLCTRL2_REG, temp);
	}
	return 0;
}

static void ps_notify_callback_work(struct work_struct *work)
{
	u32 temp = 0;
	sx933x_i2c_read_16bit(global_sx933x, SX933X_GNRLCTRL2_REG, &temp);
	if (temp & 0x0000001F) {
		LOG_DBG("USB state change, Going to force calibrate\n");
		manual_offset_calibration(global_sx933x);
	}
}

static int ps_get_state(struct power_supply *psy, bool *present)
{
	union power_supply_propval pval = {0};
	int retval;

	retval = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
			&pval);
	if (retval) {
		LOG_DBG("%s psy get property failed\n", psy->desc->name);
		return retval;
	}
	*present = (pval.intval) ? true : false;
	LOG_INFO("%s is %s\n", psy->desc->name,
			(*present) ? "present" : "not present");
	return 0;
}

static int ps_notify_callback(struct notifier_block *self,
		unsigned long event, void *p)
{
	struct sx933x_platform_data *data =
		container_of(self, struct sx933x_platform_data, ps_notif);
	struct power_supply *psy = p;
	bool present;
	int retval;

	if ((event == PSY_EVENT_PROP_ADDED || event == PSY_EVENT_PROP_CHANGED)
			&& psy && psy->desc->get_property && psy->desc->name &&
			!strncmp(psy->desc->name, "usb", sizeof("usb")) && data) {
		LOG_INFO("ps notification: event = %lu\n", event);
		retval = ps_get_state(psy, &present);
		if (retval) {
			return retval;
		}

		if (event == PSY_EVENT_PROP_CHANGED) {
			if (data->ps_is_present == present) {
				LOG_INFO("ps present state not change\n");
				return 0;
			}
		}
		data->ps_is_present = present;
		schedule_work(&data->ps_notify_work);
	}

	return 0;
}

/*! \fn static int sx933x_probe(struct i2c_client *client, const struct i2c_device_id *id)
 * \brief Probe function
 * \param client pointer to i2c_client
 * \param id pointer to i2c_device_id
 * \return Whether probe was successful
 */
static int sx933x_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int i = 0;
	int err = 0;

	psx93XX_t this = 0;
	psx933x_t pDevice = 0;
	psx933x_platform_data_t pplatData = 0;
	struct power_supply *psy = NULL;
	struct totalButtonInformation *pButtonInformationData = NULL;
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);

	LOG_DBG("sx933x_probe()\n");

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_READ_WORD_DATA))
	{
		LOG_DBG("Check i2c functionality.Fail!\n");
		err = -EIO;
		return err;
	}

	this = devm_kzalloc(&client->dev,sizeof(sx93XX_t), GFP_KERNEL); /* create memory for main struct */
	LOG_DBG("Initialized Main Memory: 0x%p\n",this);

	pButtonInformationData = devm_kzalloc(&client->dev , sizeof(struct totalButtonInformation), GFP_KERNEL);
	if (!pButtonInformationData)
	{
		LOG_DBG("Failed to allocate memory(totalButtonInformation)\n");
		err = -ENOMEM;
		return err;
	}

	pButtonInformationData->buttonSize = ARRAY_SIZE(psmtcButtons);
	pButtonInformationData->buttons =  psmtcButtons;
	pplatData = devm_kzalloc(&client->dev,sizeof(struct sx933x_platform_data), GFP_KERNEL);
	if (!pplatData)
	{
		LOG_DBG("platform data is required!\n");
		return -EINVAL;
	}
	pplatData->get_is_nirq_low = sx933x_get_nirq_state;
	pplatData->pbuttonInformation = pButtonInformationData;

	client->dev.platform_data = pplatData;
	err = sx933x_parse_dt(pplatData, &client->dev);
	if (err)
	{
		LOG_DBG("could not setup pin\n");
		return ENODEV;
	}

	pplatData->init_platform_hw = sx933x_init_platform_hw;
	LOG_INFO("SX933x init_platform_hw done!\n");

	if (this)
	{
		LOG_INFO("SX933x initialize start!!");
		/* In case we need to reinitialize data
		 * (e.q. if suspend reset device) */
		this->init = initialize;
		/* shortcut to read status of interrupt */
		this->refreshStatus = read_regStat;
		/* pointer to function from platform data to get pendown
		 * (1->NIRQ=0, 0->NIRQ=1) */
		this->get_nirq_low = pplatData->get_is_nirq_low;
		/* save irq in case we need to reference it */
		this->irq = client->irq;
		/* do we need to create an irq timer after interrupt ? */
		this->useIrqTimer = 0;

		/* Setup function to call on corresponding reg irq source bit */
		if (MAX_NUM_STATUS_BITS>= 8)
		{
			this->statusFunc[0] = 0; /* TXEN_STAT */
			this->statusFunc[1] = 0; /* UNUSED */
			this->statusFunc[2] = touchProcess; /* body&table */
			this->statusFunc[3] = read_rawData; /* CONV_STAT */
			this->statusFunc[4] = touchProcess; /* COMP_STAT */
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
		this->pDevice = pDevice = devm_kzalloc(&client->dev,sizeof(sx933x_t), GFP_KERNEL);
		LOG_INFO("initialized Device Specific Memory: 0x%p\n",pDevice);

		if (pDevice)
		{
			/* for accessing items in user data (e.g. calibrate) */
			err = class_register(&capsense_class);
			if (err < 0) {
				LOG_DBG("Create fsys class failed (%d)\n", err);
				return err;
			}

			/* Add Pointer to main platform data struct */
			pDevice->hw = pplatData;

			/* Check if we hava a platform initialization function to call*/
			if (pplatData->init_platform_hw)
				pplatData->init_platform_hw(client);

			/* Initialize the button information initialized with keycodes */
			pDevice->pbuttonInformation = pplatData->pbuttonInformation;

			for (i = 0; i < pButtonInformationData->buttonSize; i++)
			{
				if (pplatData->button_used_flag>>i & 0x01) {
					pButtonInformationData->buttons[i].used = true;
					pButtonInformationData->buttons[i].state = IDLE;
					pButtonInformationData->buttons[i].input_dev = input_allocate_device();
					if (!pButtonInformationData->buttons[i].input_dev)
						return -ENOMEM;
					pButtonInformationData->buttons[i].input_dev->name = pButtonInformationData->buttons[i].name;
					/* Set all the keycodes */
					__set_bit(EV_ABS, pButtonInformationData->buttons[i].input_dev->evbit);
					input_set_abs_params(pButtonInformationData->buttons[i].input_dev, ABS_DISTANCE, -1, 100, 0, 0);

					err = input_register_device(pButtonInformationData->buttons[i].input_dev);
					/* report a unused val, then first val will report after enable */
					input_report_abs(pButtonInformationData->buttons[i].input_dev, ABS_DISTANCE, -1);
					input_sync(pButtonInformationData->buttons[i].input_dev);

					pButtonInformationData->buttons[i].sensors_capsensor_cdev.sensors_enable = capsensor_set_enable;
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.sensors_poll_delay = NULL;
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.name = pButtonInformationData->buttons[i].name;
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.vendor = "semtech";
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.version = 1;
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.type = SENSOR_TYPE_MOTO_CAPSENSE;
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.max_range = "5";
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.resolution = "5.0";
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.sensor_power = "3";
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.min_delay = 0;
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.fifo_reserved_event_count = 0;
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.fifo_max_event_count = 0;
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.delay_msec = 100;
					pButtonInformationData->buttons[i].sensors_capsensor_cdev.enabled = 0;
					pButtonInformationData->buttons[i].enabled = false;

					err = sensors_classdev_register(&pButtonInformationData->buttons[i].input_dev->dev, &pButtonInformationData->buttons[i].sensors_capsensor_cdev);
					if (err < 0)
						LOG_DBG("create %d cap sensor_class  file failed (%d)\n", i, err);
				}
			}
		}

		pplatData->cap_vdd = regulator_get(&client->dev, "cap_vdd");
		if (IS_ERR(pplatData->cap_vdd)) {
			if (PTR_ERR(pplatData->cap_vdd) == -EPROBE_DEFER) {
				err = PTR_ERR(pplatData->cap_vdd);
				return err;
			}    
			LOG_INFO("%s: Failed to get regulator\n",
					__func__);
		} else {
			err = regulator_enable(pplatData->cap_vdd);
			if (err) {
				regulator_put(pplatData->cap_vdd);
				LOG_DBG("%s: Error %d enable regulator\n",
						__func__, err);
				return err;
			}
			pplatData->cap_vdd_en = true;
			LOG_INFO("cap_vdd regulator is %s\n",
					regulator_is_enabled(pplatData->cap_vdd) ?
					"on" : "off");
		}

		/*notify usb state*/
		INIT_WORK(&pplatData->ps_notify_work, ps_notify_callback_work);
		pplatData->ps_notif.notifier_call = ps_notify_callback;
		err = power_supply_reg_notifier(&pplatData->ps_notif);
		if (err)
			LOG_DBG("Unable to register ps_notifier: %d\n", err);

		psy = power_supply_get_by_name("usb");
		if (psy) {
			err = ps_get_state(psy, &pplatData->ps_is_present);
			if (err) {
				LOG_DBG("psy get property failed rc=%d\n", err);
				power_supply_unreg_notifier(&pplatData->ps_notif);
			}
		}

		sx93XX_IRQ_init(this);
		/* call init function pointer (this should initialize all registers */
		if (this->init) {
			this->init(this);
		}
		else {
			LOG_DBG("No init function!!!!\n");
			return -ENOMEM;
		}
	}	else	{
		return -1;
	}

	pplatData->exit_platform_hw = sx933x_exit_platform_hw;

	if (sx933x_Hardware_Check(this) != 0) {
		LOG_DBG("sx933x_Hardware_CheckFail!\n");
		//return -1;
	}

	global_sx933x = this;
	LOG_INFO("sx933x_probe() Done\n");
	return 0;
}

/*! \fn static int sx933x_remove(struct i2c_client *client)
 * \brief Called when device is to be removed
 * \param client Pointer to i2c_client struct
 * \return Value 0
 */
static int sx933x_remove(struct i2c_client *client)
{
	psx933x_platform_data_t pplatData =0;
	psx933x_t pDevice = 0; 
	struct _buttonInfo *pCurrentbutton;
	int i = 0;
	psx93XX_t this = i2c_get_clientdata(client);
	LOG_DBG("sx933x_remove");
	if (this && (pDevice = this->pDevice))
	{    
		free_irq(this->irq, this);
		cancel_delayed_work_sync(&(this->dworker));	

		class_unregister(&capsense_class);
		power_supply_unreg_notifier(&pplatData->ps_notif);
		pplatData = client->dev.platform_data;
		if (pplatData && pplatData->exit_platform_hw)
			pplatData->exit_platform_hw(client);

		if (pplatData->cap_vdd_en) {
			regulator_disable(pplatData->cap_vdd); 
			regulator_put(pplatData->cap_vdd);
		}
		for (i = 0; i <pDevice->pbuttonInformation->buttonSize; i++) {
			pCurrentbutton = &(pDevice->pbuttonInformation->buttons[i]);
			sensors_classdev_unregister(&(pCurrentbutton->sensors_capsensor_cdev));
			input_unregister_device(pCurrentbutton->input_dev);
		}
	}
	return 0;
}


#if 1//def CONFIG_PM
/*====================================================*/
/***** Kernel Suspend *****/
static int sx933x_suspend(struct device *dev)
{
	psx93XX_t this = dev_get_drvdata(dev);
	if (this) {
		disable_irq(this->irq);
		//sx933x_i2c_write_16bit(this,SX933X_CPS_CTRL0_REG,0x20);//make sx933x in Sleep mode
	}
	return 0;
}
/***** Kernel Resume *****/
static int sx933x_resume(struct device *dev)
{
	psx93XX_t this = dev_get_drvdata(dev);
	if (this) {
		enable_irq(this->irq);
		//sx933x_i2c_write_16bit(this,SX933X_CPS_CTRL0_REG,0x27);//Exit from Sleep mode
	}
	return 0;
}
/*====================================================*/
#else
#define sx933x_suspend		NULL
#define sx933x_resume		NULL
#endif /* CONFIG_PM */

static struct i2c_device_id sx933x_idtable[] =
{
	{ DRIVER_NAME, 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, sx933x_idtable);
#ifdef CONFIG_OF
static struct of_device_id sx933x_match_table[] =
{
	{ .compatible = "Semtech,sx933x",},
	{ },
};
#else
#define sx933x_match_table NULL
#endif
static const struct dev_pm_ops sx933x_pm_ops =
{
	.suspend = sx933x_suspend,
	.resume = sx933x_resume,
};
static struct i2c_driver sx933x_driver =
{
	.driver = {
		.owner			= THIS_MODULE,
		.name			= DRIVER_NAME,
		.of_match_table	= sx933x_match_table,
		.pm				= &sx933x_pm_ops,
	},
	.id_table		= sx933x_idtable,
	.probe			= sx933x_probe,
	.remove			= sx933x_remove,
};
static int __init sx933x_I2C_init(void)
{
	return i2c_add_driver(&sx933x_driver);
}
static void __exit sx933x_I2C_exit(void)
{
	i2c_del_driver(&sx933x_driver);
}

module_init(sx933x_I2C_init);
module_exit(sx933x_I2C_exit);

MODULE_AUTHOR("Semtech Corp. (http://www.semtech.com/)");
MODULE_DESCRIPTION("SX933x Capacitive Proximity Controller Driver");
MODULE_LICENSE("GPL");
MODULE_VERSION("1");

static void sx93XX_schedule_work(psx93XX_t this, unsigned long delay)
{
	unsigned long flags;
	if (this)
	{
		LOG_INFO("sx93XX_schedule_work()\n");
		spin_lock_irqsave(&this->lock,flags);
		/* Stop any pending penup queues */
		cancel_delayed_work(&this->dworker);
		//after waiting for a delay, this put the job in the kernel-global workqueue. so no need to create new thread in work queue.
		schedule_delayed_work(&this->dworker,delay);
		spin_unlock_irqrestore(&this->lock,flags);
	}
	else
		LOG_DBG("sx93XX_schedule_work, NULL psx93XX_t\n");
}

static irqreturn_t sx93XX_irq(int irq, void *pvoid)
{
	psx93XX_t this = 0;
	if (pvoid)
	{
		this = (psx93XX_t)pvoid;
		if ((!this->get_nirq_low) || this->get_nirq_low())
		{
			LOG_INFO("sx93XX_irq - call sx93XX_schedule_work\n");
			sx93XX_schedule_work(this,0);
		}
		else
		{
			LOG_DBG("sx93XX_irq - nirq read high\n");
		}
	}
	else
	{
		LOG_DBG("sx93XX_irq, NULL pvoid\n");
	}
	return IRQ_HANDLED;
}

static void sx93XX_worker_func(struct work_struct *work)
{
	psx93XX_t this = 0;
	int status = 0;
	int counter = 0;
	u8 nirqLow = 0;
	if (work)
	{
		this = container_of(work,sx93XX_t,dworker.work);

		if (!this)
		{
			LOG_DBG("sx93XX_worker_func, NULL sx93XX_t\n");
			return;
		}
		if (unlikely(this->useIrqTimer))
		{
			if ((!this->get_nirq_low) || this->get_nirq_low())
			{
				nirqLow = 1;
			}
		}
		/* since we are not in an interrupt don't need to disable irq. */
		status = this->refreshStatus(this);
		counter = -1;
		LOG_INFO("Worker_func - Refresh Status %d, use_timer_in_irq:%d\n", status, this->useIrqTimer);

		while((++counter) < MAX_NUM_STATUS_BITS)   /* counter start from MSB */
		{
			if (((status>>counter) & 0x01) && (this->statusFunc[counter]))
			{
				LOG_INFO("SX933x Function Pointer Found. Calling\n");
				this->statusFunc[counter](this);
			}
		}
		if (unlikely(this->useIrqTimer && nirqLow))
		{
			/* Early models and if RATE=0 for newer models require a penup timer */
			/* Queue up the function again for checking on penup */
			sx93XX_schedule_work(this,msecs_to_jiffies(this->irqTimeout));
		}
	}
	else
	{
		LOG_DBG("sx93XX_worker_func, NULL work_struct\n");
	}
}

int sx93XX_IRQ_init(psx93XX_t this)
{
	int err = 0;
	if (this && this->pDevice)
	{
		/* initialize spin lock */
		spin_lock_init(&this->lock);
		/* initialize worker function */
		INIT_DELAYED_WORK(&this->dworker, sx93XX_worker_func);
		/* initailize interrupt reporting */
		this->irq_disabled = 0;
		err = request_irq(this->irq, sx93XX_irq, IRQF_TRIGGER_FALLING,
				this->pdev->driver->name, this);
		if (err)
		{
			LOG_DBG("irq %d busy?\n", this->irq);
			return err;
		}
		LOG_DBG("registered with irq (%d)\n", this->irq);
	}
	return -ENOMEM;
}
