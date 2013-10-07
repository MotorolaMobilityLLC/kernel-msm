/*
  * drivers/power/Smb345-charger.c
  *
  * Charger driver for Summit SMB345
  *
  * Copyright (c) 2012, ASUSTek Inc.
  *
  * This program is free software; you can redistribute it and/or modify
  * it under the terms of the GNU General Public License as published by
  * the Free Software Foundation; either version 2 of the License, or
  * (at your option) any later version.
  *
  * This program is distributed in the hope that it will be useful, but WITHOUT
  * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
  * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
  * more details.
  */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/mutex.h>
#include <linux/err.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <linux/power_supply.h>
#include <linux/platform_device.h>
#include <linux/regulator/driver.h>
#include <linux/regulator/machine.h>
#include <linux/smb345-charger.h>
#include <linux/slab.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/usb/otg.h>
#include <linux/workqueue.h>
#include <linux/fs.h>
#include <linux/miscdevice.h>
#include <linux/ioctl.h>
#include <asm/uaccess.h>
#include <asm/mach-types.h>

#define smb345_CHARGE		0x00
#define smb345_CHRG_CRNTS	0x01
#define smb345_VRS_FUNC		0x02
#define smb345_FLOAT_VLTG	0x03
#define smb345_CHRG_CTRL	0x04
#define smb345_STAT_TIME_CTRL	0x05
#define smb345_PIN_CTRL		0x06
#define smb345_THERM_CTRL	0x07
#define smb345_SYSOK_USB3	0x08
#define smb345_CTRL_REG		0x09

#define smb345_OTG_TLIM_REG	0x0A
#define smb345_HRD_SFT_TEMP	0x0B
#define smb345_FAULT_INTR	0x0C
#define smb345_STS_INTR_1	0x0D
#define smb345_I2C_ADDR	0x0E
#define smb345_IN_CLTG_DET	0x10
#define smb345_STS_INTR_2	0x11

/* Command registers */
#define smb345_CMD_REG		0x30
#define smb345_CMD_REG_B	0x31
#define smb345_CMD_REG_c	0x33

/* Interrupt Status registers */
#define smb345_INTR_STS_A	0x35
#define smb345_INTR_STS_B	0x36
#define smb345_INTR_STS_C	0x37
#define smb345_INTR_STS_D	0x38
#define smb345_INTR_STS_E	0x39
#define smb345_INTR_STS_F	0x3A

/* Status registers */
#define smb345_STS_REG_A	0x3B
#define smb345_STS_REG_B	0x3C
#define smb345_STS_REG_C	0x3D
#define smb345_STS_REG_D	0x3E
#define smb345_STS_REG_E	0x3F

/* APQ8064 GPIO pin definition */
#define APQ_AP_CHAR	22
#define APQ_AP_ACOK	23

#define smb345_ENABLE_WRITE	1
#define smb345_DISABLE_WRITE	0
#define ENABLE_WRT_ACCESS	0x80
#define ENABLE_APSD		0x04
#define PIN_CTRL		0x10
#define PIN_ACT_LOW	0x20
#define ENABLE_CHARGE		0x02
#define USBIN		0x80
#define APSD_OK		0x08
#define APSD_RESULT		0x07
#define FLOAT_VOLT_MASK	0x3F
#define ENABLE_PIN_CTRL_MASK 0x60
#define HOT_LIMIT_MASK 0x33
#define BAT_OVER_VOLT_MASK 0x40
#define STAT_OUTPUT_EN		0x20
#define GPIO_AC_OK		APQ_AP_ACOK
#define WPC_DEBOUNCE_INTERVAL	(1 * HZ)
#define WPC_SET_CURT_INTERVAL	(2 * HZ)
#define WPC_INIT_DET_INTERVAL	(22 * HZ)
#define WPC_SET_CURT_LIMIT_CNT	6
#define BAT_Cold_Limit 0
#define BAT_Hot_Limit 45
#define BAT_Mid_Temp_Wired 45
#define BAT_Mid_Temp_Wireless 40
#define FLOAT_VOLT 0x2A
#define FLOAT_VOLT_LOW 0x1E
#define FLOAT_VOLT_43V 0x28
#define FLOAT_VOLT_LOW_DECIMAL 4110000
#define THERMAL_RULE1 1
#define THERMAL_RULE2 2

/* Functions declaration */
extern int  bq27541_battery_callback(unsigned usb_cable_state);
extern int  bq27541_wireless_callback(unsigned wireless_state);
extern void touch_callback(unsigned cable_status);
static ssize_t smb345_reg_show(struct device *dev, struct device_attribute *attr, char *buf);

/* Global variables */
static struct smb345_charger *charger;
static struct workqueue_struct *smb345_wq;
struct wake_lock charger_wakelock;
static int charge_en_flag = 1;
struct wake_lock wireless_wakelock;
bool wireless_on;
bool otg_on;
extern int ac_on;
extern int usb_on;
static bool wpc_en;
static bool disable_DCIN;

/* Sysfs interface */
static DEVICE_ATTR(reg_status, S_IWUSR | S_IRUGO, smb345_reg_show, NULL);

static struct attribute *smb345_attributes[] = {
	&dev_attr_reg_status.attr,
NULL
};

static const struct attribute_group smb345_group = {
	.attrs = smb345_attributes,
};

static int smb345_read(struct i2c_client *client, int reg)
{
	int ret;

	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int smb345_write(struct i2c_client *client, int reg, u8 value)
{
	int ret;

	ret = i2c_smbus_write_byte_data(client, reg, value);

	if (ret < 0)
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);

	return ret;
}

static int smb345_update_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret, retval;

	retval = smb345_read(client, reg);
	if (retval < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, retval);
		return retval;
	}

	ret = smb345_write(client, reg, retval | value);
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	return ret;
}

static int smb345_clear_reg(struct i2c_client *client, int reg, u8 value)
{
	int ret, retval;

	retval = smb345_read(client, reg);
	if (retval < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, retval);
		return retval;
	}

	ret = smb345_write(client, reg, retval & (~value));
	if (ret < 0) {
		dev_err(&client->dev, "%s: err %d\n", __func__, ret);
		return ret;
	}

	return ret;
}

int smb345_volatile_writes(struct i2c_client *client, uint8_t value)
{
	int ret = 0;

	if (value == smb345_ENABLE_WRITE) {
		/* Enable volatile write to config registers */
		ret = smb345_update_reg(client, smb345_CMD_REG,
						ENABLE_WRT_ACCESS);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed in writing"
				"register 0x%02x\n", __func__, smb345_CMD_REG);
			return ret;
		}
	} else {
		ret = smb345_read(client, smb345_CMD_REG);
		if (ret < 0) {
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
			return ret;
		}

		ret = smb345_write(client, smb345_CMD_REG, ret & (~(1<<7)));
		if (ret < 0) {
			dev_err(&client->dev, "%s: err %d\n", __func__, ret);
			return ret;
		}
	}
	return ret;
}

int wireless_is_plugged(void)
{
	if (charger->wpc_pok_gpio != -1)
		return !(gpio_get_value(charger->wpc_pok_gpio));
	else
		return 0;
}

static int smb345_pin_control(bool state)
{
	struct i2c_client *client = charger->client;
	u8 ret = 0;

	mutex_lock(&charger->pinctrl_lock);

	if (state) {
		/*Pin Controls -active low */
		ret = smb345_update_reg(client, smb345_PIN_CTRL, PIN_ACT_LOW);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed to"
						"enable charger\n", __func__);
		}
	} else {
		/*Pin Controls -active high */
		ret = smb345_clear_reg(client, smb345_PIN_CTRL, PIN_ACT_LOW);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed to"
						"disable charger\n", __func__);
		}
	}

	mutex_unlock(&charger->pinctrl_lock);
	return ret;
}

int smb345_charger_enable(bool state)
{
	struct i2c_client *client = charger->client;
	u8 ret = 0;

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in configuring charger..\n",
								__func__);
		goto error;
	}
	charge_en_flag = state;
	smb345_pin_control(state);

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in configuring charger..\n",
								__func__);
		goto error;
	}

error:
	return ret;
}
EXPORT_SYMBOL_GPL(smb345_charger_enable);

int smb345_vflt_setting(void)
{
	struct i2c_client *client = charger->client;
	u8 ret = 0, setting;

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in smb345 volatile writes \n", __func__);
		goto error;
	}

	ret = smb345_read(client, smb345_FLOAT_VLTG);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in smb345 read \n", __func__);
		goto error;
	}

	setting = ret & FLOAT_VOLT_MASK;
	if (setting != FLOAT_VOLT_43V) {
		setting = ret & (~FLOAT_VOLT_MASK);
		setting |= FLOAT_VOLT_43V;
		SMB_NOTICE("Set Float Volt, retval=%x setting=%x\n", ret, setting);
		ret = smb345_write(client, smb345_FLOAT_VLTG, setting);
		if (ret < 0) {
			dev_err(&client->dev, "%s() error in smb345 write \n", __func__);
			goto error;
		}
	} else
		SMB_NOTICE("Bypass set Float Volt=%x\n", ret);

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in smb345 volatile writes \n", __func__);
		goto error;
	}

error:
	return ret;
}

int
smb345_set_InputCurrentlimit(struct i2c_client *client, u32 current_setting)
{
	int ret = 0, retval;
	u8 setting = 0;

	wake_lock(&charger_wakelock);

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in configuring charger..\n",
								__func__);
		goto error;
	}

	if (charge_en_flag)
		smb345_pin_control(0);

	retval = smb345_read(client, smb345_VRS_FUNC);
	if (retval < 0) {
		dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
				__func__, smb345_VRS_FUNC);
		goto error;
	}

	setting = retval & (~(BIT(4)));
	SMB_NOTICE("Disable AICL, retval=%x setting=%x\n", retval, setting);
	ret = smb345_write(client, smb345_VRS_FUNC, setting);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
			"0x%02x\n", __func__, setting, smb345_VRS_FUNC);
		goto error;
	}

	retval = smb345_read(client, smb345_CHRG_CRNTS);
	if (retval < 0) {
		dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
			__func__, smb345_CHRG_CRNTS);
		goto error;
	}
	setting = retval & 0xF0;
	if(current_setting == 2000)
		setting |= 0x07;
	else if(current_setting == 1800)
		setting |= 0x06;
	else if (current_setting == 1200)
		setting |= 0x04;
	else if(current_setting == 900)
		setting |= 0x03;
	else if(current_setting == 500)
		setting |= 0x01;
	else
		setting |= 0x07;

	SMB_NOTICE("Set ICL=%u retval =%x setting=%x\n", current_setting, retval, setting);

	ret = smb345_write(client, smb345_CHRG_CRNTS, setting);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
			"0x%02x\n", __func__, setting, smb345_CHRG_CRNTS);
		goto error;
	}

	if(current_setting == 2000)
		charger->curr_limit = 2000;
	else if(current_setting == 1800)
		charger->curr_limit = 1800;
	else if (current_setting == 1200)
		charger->curr_limit = 1200;
	else if(current_setting == 900)
		charger->curr_limit = 900;
	else if(current_setting == 500)
		charger->curr_limit = 500;
	else
		charger->curr_limit = 2000;

	if (current_setting > 900) {
		charger->time_of_1800mA_limit = jiffies;
	} else{
		charger->time_of_1800mA_limit = 0;
	}

	retval = smb345_read(client, smb345_VRS_FUNC);
	if (retval < 0) {
		dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
				__func__, smb345_VRS_FUNC);
		goto error;
	}

	setting = retval | BIT(4);
	SMB_NOTICE("Re-enable AICL, setting=%x\n", setting);
	msleep(20);
	ret = smb345_write(client, smb345_VRS_FUNC, setting);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
			"0x%02x\n", __func__, setting, smb345_VRS_FUNC);
			goto error;
	}

	if (charge_en_flag)
		smb345_pin_control(1);

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in configuring charger..\n",
								__func__);
		goto error;
	}

error:
	wake_unlock(&charger_wakelock);
	return ret;
}

static irqreturn_t smb345_inok_isr(int irq, void *dev_id)
{
	SMB_NOTICE("VBUS_DET = %s\n", gpio_get_value(GPIO_AC_OK) ? "H" : "L");

	return IRQ_HANDLED;
}

static irqreturn_t smb345_wireless_isr(int irq, void *dev_id)
{
	struct smb345_charger *smb = dev_id;

	queue_delayed_work(smb345_wq, &smb->wireless_isr_work, WPC_DEBOUNCE_INTERVAL);

	return IRQ_HANDLED;
}

static int smb345_wireless_irq(struct smb345_charger *smb)
{
	int err = 0 ;
	unsigned gpio = charger->wpc_pok_gpio;
	unsigned irq_num = gpio_to_irq(gpio);

	err = gpio_request(gpio, "wpc_pok");
	if (err)
		SMB_ERR("gpio %d request failed\n", gpio);

	err = gpio_direction_input(gpio);
	if (err)
		SMB_ERR("gpio %d unavaliable for input\n", gpio);

	err = request_irq(irq_num, smb345_wireless_isr,
		IRQF_TRIGGER_FALLING | IRQF_TRIGGER_RISING | IRQF_SHARED,
		"wpc_pok", smb);
	if (err < 0) {
		SMB_ERR("%s irq %d request failed\n", "wpc_pok", irq_num);
		goto fault ;
	}
	enable_irq_wake(irq_num);
	SMB_NOTICE("GPIO pin irq %d requested ok, wpc_pok = %s\n", irq_num,
		gpio_get_value(gpio) ? "H" : "L");
	return 0;

fault:
	gpio_free(gpio);
	return err;
}

int
smb345_set_WCInputCurrentlimit(struct i2c_client *client, u32 current_setting)
{
	int ret = 0, retval;
	u8 setting;

	charger->wpc_curr_limit_count++;

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in configuring charger..\n",
								__func__);
		goto error;
	}

	if (current_setting != 0) {
		retval = smb345_read(client, smb345_CHRG_CRNTS);
		if (retval < 0) {
			dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
				__func__, smb345_CHRG_CRNTS);
			goto error;
		}

		setting = retval & 0x0F;
		if (current_setting == 2000)
			setting |= 0x70;
		else if (current_setting == 1800)
			setting |= 0x60;
		else if (current_setting == 1200)
			setting |= 0x40;
		else if (current_setting == 900)
			setting |= 0x30;
		else if (current_setting == 700)
			setting |= 0x20;
		else if (current_setting == 500)
			setting |= 0x10;
		else if (current_setting == 300)
			setting |= 0x00;
		else
			setting |= 0x20;

		SMB_NOTICE("Set ICL=%u retval =%x setting=%x\n",
			current_setting, retval, setting);

		ret = smb345_write(client, smb345_CHRG_CRNTS, setting);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
				"0x%02x\n", __func__, setting, smb345_CHRG_CRNTS);
			goto error;
		}

		charger->wpc_curr_limit = current_setting;
	}

	if (current_setting == 300) {
		retval = smb345_read(client, smb345_VRS_FUNC);
		if (retval < 0) {
			dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
					__func__, smb345_VRS_FUNC);
			goto error;
		}

		setting = retval & (~(BIT(4)));
		SMB_NOTICE("Disable AICL, retval=%x setting=%x\n", retval, setting);
		ret = smb345_write(client, smb345_VRS_FUNC, setting);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
				"0x%02x\n", __func__, setting, smb345_VRS_FUNC);
			goto error;
		}
	}

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() error in configuring charger..\n",
								__func__);
		goto error;
	}

error:
	return ret;
}

static void wireless_set(void)
{
	wireless_on = true;
	if (delayed_work_pending(&charger->wireless_set_current_work))
		cancel_delayed_work(&charger->wireless_set_current_work);
	wake_lock(&wireless_wakelock);
	charger->wpc_curr_limit = 300;
	charger->wpc_curr_limit_count = 0;
	smb345_set_WCInputCurrentlimit(charger->client, 300);
	smb345_vflt_setting();
	bq27541_wireless_callback(wireless_on);
	queue_delayed_work(smb345_wq, &charger->wireless_set_current_work, WPC_SET_CURT_INTERVAL);
}

static void wireless_reset(void)
{
	wireless_on = false;
	if (delayed_work_pending(&charger->wireless_set_current_work))
		cancel_delayed_work(&charger->wireless_set_current_work);
	charger->wpc_curr_limit = 300;
	charger->wpc_curr_limit_count = 0;
	if (ac_on) {
		smb345_set_InputCurrentlimit(charger->client, 1200);
		smb345_vflt_setting();
	}
	bq27541_wireless_callback(wireless_on);
	wake_unlock(&wireless_wakelock);
}

static void wireless_isr_work_function(struct work_struct *dat)
{
	if (delayed_work_pending(&charger->wireless_isr_work))
		cancel_delayed_work(&charger->wireless_isr_work);

	SMB_NOTICE("wireless state = %d\n", wireless_is_plugged());

	if (otg_on) {
		SMB_NOTICE("bypass wireless isr due to otg_on\n");
		return;
	}

	if (wireless_is_plugged())
		wireless_set();
	else
		wireless_reset();
}

static void wireless_det_work_function(struct work_struct *dat)
{
	if (otg_on) {
		SMB_NOTICE("bypass wireless isr due to otg_on\n");
		return;
	}
	if (wireless_is_plugged())
		wireless_set();
}

static void wireless_set_current_function(struct work_struct *dat)
{
	if (delayed_work_pending(&charger->wireless_set_current_work))
		cancel_delayed_work(&charger->wireless_set_current_work);

	if (charger->wpc_curr_limit == 700 || charger->wpc_curr_limit_count >= WPC_SET_CURT_LIMIT_CNT)
		return;
	else if (charger->wpc_curr_limit == 300)
		smb345_set_WCInputCurrentlimit(charger->client, 500);
	else if (charger->wpc_curr_limit == 500)
		smb345_set_WCInputCurrentlimit(charger->client, 700);

	queue_delayed_work(smb345_wq, &charger->wireless_set_current_work, WPC_SET_CURT_INTERVAL);
}

void reconfig_AICL(void)
{
	struct i2c_client *client = charger->client;

	if (ac_on && !gpio_get_value(GPIO_AC_OK)) {
		int retval;
		retval = smb345_read(client, smb345_STS_REG_E);
		if (retval < 0)
			dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
			__func__, smb345_STS_REG_E);
		else {
			SMB_NOTICE("Status Reg E=0x%02x\n", retval);

			if ((retval & 0xF) <= 0x1) {
				SMB_NOTICE("reconfig input current limit\n");
				smb345_set_InputCurrentlimit(client, 1200);
			}
		}
	}
}
EXPORT_SYMBOL(reconfig_AICL);

static int smb345_inok_irq(struct smb345_charger *smb)
{
	int err = 0 ;
	unsigned gpio = GPIO_AC_OK;
	unsigned irq_num = gpio_to_irq(gpio);

	err = gpio_request(gpio, "smb345_inok");
	if (err) {
		SMB_ERR("gpio %d request failed \n", gpio);
	}

	err = gpio_direction_input(gpio);
	if (err) {
		SMB_ERR("gpio %d unavaliable for input \n", gpio);
	}

	err = request_irq(irq_num, smb345_inok_isr, IRQF_TRIGGER_FALLING |IRQF_TRIGGER_RISING|IRQF_SHARED,
		"smb345_inok", smb);
	if (err < 0) {
		SMB_ERR("%s irq %d request failed \n","smb345_inok", irq_num);
		goto fault ;
	}
	enable_irq_wake(irq_num);
	SMB_NOTICE("GPIO pin irq %d requested ok, smb345_INOK = %s\n", irq_num, gpio_get_value(gpio)? "H":"L");
	return 0;

fault:
	gpio_free(gpio);
	return err;
}

static int smb345_configure_otg(struct i2c_client *client)
{
	int ret = 0;

	/*Enable volatile writes to registers and Allow fast charge */
	ret = smb345_write(client, smb345_CMD_REG, 0xc0);
       if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_CMD_REG);
		goto error;
       }

	/* Change "OTG output current limit" to 250mA */
      ret = smb345_write(client, smb345_OTG_TLIM_REG, 0x34);
       if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_OTG_TLIM_REG);
		goto error;
       }

	/* Enable OTG */
       ret = smb345_update_reg(client, smb345_CMD_REG, 0x10);
       if (ret < 0) {
	       dev_err(&client->dev, "%s: Failed in writing register"
			"0x%02x\n", __func__, smb345_CMD_REG);
		goto error;
       }

	/* Change "OTG output current limit" from 250mA to 750mA */
	ret = smb345_update_reg(client, smb345_OTG_TLIM_REG, 0x08);
       if (ret < 0) {
	       dev_err(&client->dev, "%s: Failed in writing register"
			"0x%02x\n", __func__, smb345_OTG_TLIM_REG);
		goto error;
       }

	/* Change OTG to Pin control */
       ret = smb345_write(client, smb345_CTRL_REG, 0x65);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_CTRL_REG);
		goto error;
       }

	/* Disable volatile writes to registers */
	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s error in configuring OTG..\n",
								__func__);
	       goto error;
	}
	return 0;
error:
	return ret;
}

void smb345_otg_status(bool on)
{
	struct i2c_client *client = charger->client;
	int ret;

	SMB_NOTICE("otg function: %s\n", on ? "on" : "off");

	if (on) {
		otg_on = true;
		/* ENABLE OTG */
		ret = smb345_configure_otg(client);
		if (ret < 0) {
			dev_err(&client->dev, "%s() error in configuring"
				"otg..\n", __func__);
			return;
		}
		if (wireless_is_plugged())
			wireless_reset();
		return;
	} else
		otg_on = false;

	if (wireless_is_plugged())
		wireless_set();
}
EXPORT_SYMBOL_GPL(smb345_otg_status);

int smb345_float_volt_set(unsigned int val)
{
	struct i2c_client *client = charger->client;
	int ret = 0, retval;

	if (val > 4500 || val < 3500) {
		SMB_ERR("%s(): val=%d is out of range !\n",__func__, val);
	}

	printk("%s(): val=%d\n",__func__, val);

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
		goto fault;
	}

	retval = smb345_read(client, smb345_FLOAT_VLTG);
	if (retval < 0) {
		dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
				__func__, smb345_FLOAT_VLTG);
		goto fault;
	}
	retval = retval & (~FLOAT_VOLT_MASK);
	val = clamp_val(val, 3500, 4500) - 3500;
	val /= 20;
	retval |= val;
	ret = smb345_write(client, smb345_FLOAT_VLTG, retval);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_FLOAT_VLTG);
		goto fault;
       }

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger disable write error..\n", __func__);
	}
	return 0;
fault:
	return ret;
}
EXPORT_SYMBOL_GPL(smb345_float_volt_set);

int usb_cable_type_detect(unsigned int chgr_type)
{
	struct i2c_client *client = charger->client;
	int  success = 0;

	mutex_lock(&charger->usb_lock);

	if (chgr_type == CHARGER_NONE) {
		SMB_NOTICE("INOK=H\n");
		if (wpc_en) {
			if (disable_DCIN) {
				SMB_NOTICE("enable wpc_pok, enable DCIN\n");
				disable_DCIN = false;
				enable_irq_wake(gpio_to_irq(charger->wpc_pok_gpio));
				enable_irq(gpio_to_irq(charger->wpc_pok_gpio));
				gpio_set_value(charger->wpc_en1, 0);
				gpio_set_value(charger->wpc_en2, 0);
			}
		}
		success =  bq27541_battery_callback(non_cable);
		touch_callback(non_cable);
	} else {
		SMB_NOTICE("INOK=L\n");

		if (chgr_type == CHARGER_SDP) {
			SMB_NOTICE("Cable: SDP\n");
			smb345_vflt_setting();
			success =  bq27541_battery_callback(usb_cable);
			touch_callback(usb_cable);
		} else {
			if (chgr_type == CHARGER_CDP) {
				SMB_NOTICE("Cable: CDP\n");
			} else if (chgr_type == CHARGER_DCP) {
				SMB_NOTICE("Cable: DCP\n");
			} else if (chgr_type == CHARGER_OTHER) {
				SMB_NOTICE("Cable: OTHER\n");
			} else if (chgr_type == CHARGER_ACA) {
				SMB_NOTICE("Cable: ACA\n");
			} else {
				SMB_NOTICE("Cable: TBD\n");
				smb345_vflt_setting();
				success =  bq27541_battery_callback(usb_cable);
				touch_callback(usb_cable);
				goto done;
			}
			smb345_set_InputCurrentlimit(client, 1200);
			smb345_vflt_setting();
			success =  bq27541_battery_callback(ac_cable);
			touch_callback(ac_cable);
			if (wpc_en) {
				if (delayed_work_pending(&charger->wireless_set_current_work))
					cancel_delayed_work(&charger->wireless_set_current_work);
				if (!disable_DCIN) {
					SMB_NOTICE("AC cable detect, disable wpc_pok, disable DCIN");
					disable_DCIN = true;
					disable_irq(gpio_to_irq(charger->wpc_pok_gpio));
					disable_irq_wake(gpio_to_irq(charger->wpc_pok_gpio));
					gpio_set_value(charger->wpc_en1, 1);
					gpio_set_value(charger->wpc_en2, 1);
					mdelay(200);
					if (wireless_on)
						wireless_reset();
				}
			}
		}
	}
done:
	mutex_unlock(&charger->usb_lock);
	return success;
}
EXPORT_SYMBOL_GPL(usb_cable_type_detect);

/* Sysfs function */
static ssize_t smb345_reg_show(struct device *dev, struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = charger->client;
	uint8_t config_reg[15], cmd_reg[2], status_reg[11];
	char tmp_buf[64];
	int i, cfg_ret, cmd_ret, sts_ret = 0;

	cfg_ret = i2c_smbus_read_i2c_block_data(client, smb345_CHARGE, 15, config_reg);
	cmd_ret = i2c_smbus_read_i2c_block_data(client, smb345_CMD_REG, 2, cmd_reg);
	sts_ret = i2c_smbus_read_i2c_block_data(client, smb345_INTR_STS_A, 11, status_reg);

	sprintf(tmp_buf, "SMB34x Configuration Registers Detail\n"
					"==================\n");
	strcpy(buf, tmp_buf);

	if (cfg_ret > 0) {
		for(i=0;i<=14;i++) {
			sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", i, config_reg[i]);
			strcat(buf, tmp_buf);
		}
	}
	if (cmd_ret > 0) {
		for(i=0;i<=1;i++) {
			sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", 48+i, cmd_reg[i]);
			strcat(buf, tmp_buf);
		}
	}
	if (sts_ret > 0) {
		for(i=0;i<=10;i++) {
			sprintf(tmp_buf, "Reg%02xh:\t0x%02x\n", 53+i, status_reg[i]);
			strcat(buf, tmp_buf);
		}
	}
	return strlen(buf);
}

static int smb345_otg_setting(struct i2c_client *client)
{
	int ret;

	/*Enable volatile writes to registers and Allow fast charge */
	ret = smb345_update_reg(client, smb345_CMD_REG, 0xc0);
       if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_CMD_REG);
		goto error;
       }

	/* Change OTG to I2C control */
	ret = smb345_update_reg(client, smb345_CTRL_REG, 0x25);
	if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_CTRL_REG);
		goto error;
       }

	/*Enable volatile writes to registers and Allow fast charge */
	ret = smb345_update_reg(client, smb345_CMD_REG, 0xc0);
       if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_CMD_REG);
		goto error;
       }

	/* Change "OTG output current limit" to 250mA */
	ret = smb345_update_reg(client, smb345_OTG_TLIM_REG, 0x34);
       if (ret < 0) {
		dev_err(&client->dev, "%s(): Failed in writing"
			"register 0x%02x\n", __func__, smb345_OTG_TLIM_REG);
		goto error;
       }

	/* Disable volatile writes to registers */
	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s error in configuring OTG..\n",
								__func__);
	       goto error;
	}
	return 0;
error:
	return ret;
}

static int smb345_wireless_en_config(struct smb345_charger *smb)
{
	int err = 0 ;

	err = gpio_request(charger->wpc_en1, "WPC_EN1");
	if (err)
		return -ENODEV;
	err = gpio_request(charger->wpc_en2, "WPC_EN2");
	if (err)
		goto fault;

	gpio_set_value(charger->wpc_en1, 0);
	gpio_set_value(charger->wpc_en2, 0);

	SMB_NOTICE("GPIO pin %d, %d requested ok, wpc_en1 = %s, wpc_en2 = %s\n",
		charger->wpc_en1, charger->wpc_en2, gpio_get_value(charger->wpc_en1) ? "H" : "L",
		gpio_get_value(charger->wpc_en2) ? "H" : "L");

	return 0;
fault:
	gpio_free(charger->wpc_en1);
	return err;
}

int smb345_config_thermal_suspend(void)
{
	struct i2c_client *client = charger->client;
	int ret = 0, retval, setting = 0;

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
		goto error;
	}

	retval = smb345_read(client, smb345_HRD_SFT_TEMP);
	if (retval < 0) {
		dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
				__func__, smb345_HRD_SFT_TEMP);
		goto error;
	}

	setting = retval & HOT_LIMIT_MASK;
	if (setting != 0x01) {
		setting = retval & (~HOT_LIMIT_MASK);
		setting |= 0x01;
		SMB_NOTICE("Set HRD SFT limit, retval=%x setting=%x\n", retval, setting);
		ret = smb345_write(client, smb345_HRD_SFT_TEMP, setting);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
				"0x%02x\n", __func__, setting, smb345_HRD_SFT_TEMP);
			goto error;
		}
	} else
		SMB_NOTICE("Bypass set HRD SFT limit=%x\n", retval);

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
		goto error;
	}
error:
	return ret;
}

int smb345_config_thermal_limit(void)
{
	struct i2c_client *client = charger->client;
	int ret = 0, retval, setting = 0;

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
		goto error;
	}

	retval = smb345_read(client, smb345_HRD_SFT_TEMP);
	if (retval < 0) {
		dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
				__func__, smb345_HRD_SFT_TEMP);
		goto error;
	}

	setting = retval & HOT_LIMIT_MASK;
	if (setting != 0x33) {
		setting = retval & (~HOT_LIMIT_MASK);
		setting |= 0x33;
		SMB_NOTICE("Set HRD SFT limit, retval=%x setting=%x\n", retval, setting);
		ret = smb345_write(client, smb345_HRD_SFT_TEMP, setting);
		if (ret < 0) {
			dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
				"0x%02x\n", __func__, setting, smb345_HRD_SFT_TEMP);
			goto error;
		}
	} else
		SMB_NOTICE("Bypass set HRD SFT limit=%x\n", retval);

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
		goto error;
	}
error:
	return ret;
}

int smb345_config_thermal_charging(int temp, int volt, int rule)
{
	struct i2c_client *client = charger->client;
	int ret = 0, retval, setting = 0;
	int BAT_Mid_Temp = BAT_Mid_Temp_Wired;

	if (rule == THERMAL_RULE1)
		BAT_Mid_Temp = BAT_Mid_Temp_Wired;
	else if (rule == THERMAL_RULE2)
		BAT_Mid_Temp = BAT_Mid_Temp_Wireless;

	mdelay(100);
	smb345_config_thermal_limit();

	SMB_NOTICE("temp=%d, volt=%d\n", temp, volt);

	ret = smb345_volatile_writes(client, smb345_ENABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
		goto error;
	}

	/*control float voltage*/
	retval = smb345_read(client, smb345_FLOAT_VLTG);
	if (retval < 0) {
		dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
				__func__, smb345_FLOAT_VLTG);
		goto error;
	}

	setting = retval & FLOAT_VOLT_MASK;
	if (temp <= BAT_Mid_Temp
		|| (temp > BAT_Mid_Temp && volt > FLOAT_VOLT_LOW_DECIMAL)
		|| temp > BAT_Hot_Limit) {
		if (setting != FLOAT_VOLT_43V) {
			setting = retval & (~FLOAT_VOLT_MASK);
			setting |= FLOAT_VOLT_43V;
			SMB_NOTICE("Set Float Volt, retval=%x setting=%x\n", retval, setting);
			ret = smb345_write(client, smb345_FLOAT_VLTG, setting);
			if (ret < 0) {
				dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
					"0x%02x\n", __func__, setting, smb345_FLOAT_VLTG);
				goto error;
			}
		} else
			SMB_NOTICE("Bypass set Float Volt=%x\n", retval);
	} else {
		if (setting != FLOAT_VOLT_LOW) {
			setting = retval & (~FLOAT_VOLT_MASK);
			setting |= FLOAT_VOLT_LOW;
			SMB_NOTICE("Set Float Volt, retval=%x setting=%x\n", retval, setting);
			ret = smb345_write(client, smb345_FLOAT_VLTG, setting);
			if (ret < 0) {
				dev_err(&client->dev, "%s(): Failed in writing 0x%02x to register"
					"0x%02x\n", __func__, setting, smb345_FLOAT_VLTG);
				goto error;
			}
		} else
			SMB_NOTICE("Bypass set Float Volt=%x\n", retval);
	}

	/*charger enable/disable*/
	retval = smb345_read(client, smb345_PIN_CTRL);
	if (retval < 0) {
		dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
				__func__, smb345_PIN_CTRL);
		goto error;
	}

	setting = retval & ENABLE_PIN_CTRL_MASK;
	if (temp < BAT_Cold_Limit || temp > BAT_Hot_Limit ||
		(temp > BAT_Mid_Temp && volt > FLOAT_VOLT_LOW_DECIMAL)) {
		if (setting != 0x40) {
			SMB_NOTICE("Charger disable\n");
			smb345_charger_enable(false);
		} else
			SMB_NOTICE("Bypass charger disable\n");
	} else {
		if (setting != 0x60) {
			SMB_NOTICE("Charger enable\n");
			smb345_charger_enable(true);
		} else {
			/*interrupt status*/
			retval = smb345_read(client, smb345_INTR_STS_B);
			if (retval < 0) {
				dev_err(&client->dev, "%s(): Failed in reading 0x%02x",
					__func__, smb345_INTR_STS_B);
				goto error;
			}
			if ((retval & BAT_OVER_VOLT_MASK) == 0x40) {
				SMB_NOTICE("disable then enable charger to recover bat over-volt\n");
				smb345_charger_enable(false);
				smb345_charger_enable(true);
			} else
				SMB_NOTICE("Bypass charger enable\n");
		}
	}

	ret = smb345_volatile_writes(client, smb345_DISABLE_WRITE);
	if (ret < 0) {
		dev_err(&client->dev, "%s() charger enable write error..\n", __func__);
		goto error;
	}
error:
	return ret;
}
EXPORT_SYMBOL(smb345_config_thermal_charging);

static int __devinit smb345_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct smb345_platform_data *platform_data;
	int ret;

	if (!i2c_check_functionality(adapter, I2C_FUNC_SMBUS_BYTE))
		return -EIO;

	charger = kzalloc(sizeof(*charger), GFP_KERNEL);
	if (!charger)
		return -ENOMEM;

	printk("smb345_probe+\n");
	charger->client = client;
	charger->dev = &client->dev;
	i2c_set_clientdata(client, charger);

	smb345_otg_setting(charger->client);

	ret = sysfs_create_group(&client->dev.kobj, &smb345_group);
	if (ret) {
		SMB_ERR("Unable to create the sysfs\n");
		goto error;
	}

	mutex_init(&charger->apsd_lock);
	mutex_init(&charger->usb_lock);
	mutex_init(&charger->pinctrl_lock);

	smb345_wq = create_singlethread_workqueue("smb345_wq");

	INIT_DELAYED_WORK_DEFERRABLE(&charger->wireless_isr_work,
		wireless_isr_work_function);
	INIT_DELAYED_WORK_DEFERRABLE(&charger->wireless_det_work,
		wireless_det_work_function);
	INIT_DELAYED_WORK_DEFERRABLE(&charger->wireless_set_current_work,
		wireless_set_current_function);

	wake_lock_init(&charger_wakelock, WAKE_LOCK_SUSPEND,
			"charger_configuration");
	wake_lock_init(&wireless_wakelock, WAKE_LOCK_SUSPEND,
			"wireless_charger");
	charger->curr_limit = UINT_MAX;
	charger->wpc_curr_limit = 300;
	charger->wpc_curr_limit_count = 0;
	charger->cur_cable_type = non_cable;
	charger->old_cable_type = non_cable;
	charger->wpc_pok_gpio = -1;
	charger->wpc_en1 = -1;
	charger->wpc_en2 = -1;
	wireless_on = false;
	otg_on = false;
	wpc_en = false;
	disable_DCIN = false;

	ret = smb345_inok_irq(charger);
	if (ret) {
		SMB_ERR("Failed in requesting ACOK# pin isr\n");
		goto error;
	}

	if (client->dev.platform_data) {
		platform_data = client->dev.platform_data;
		if (platform_data->wpc_pok_gpio != -1) {
			charger->wpc_pok_gpio = platform_data->wpc_pok_gpio;
			ret = smb345_wireless_irq(charger);
			if (ret) {
				SMB_ERR("Failed in requesting WPC_POK# pin isr\n");
				goto error;
			}

		}
		if (platform_data->wpc_en1 != -1 && platform_data->wpc_en2 != -1) {
			charger->wpc_en1 = platform_data->wpc_en1;
			charger->wpc_en2 = platform_data->wpc_en2;
			wpc_en = true;
			ret = smb345_wireless_en_config(charger);
			if (ret) {
				SMB_ERR("Failed in config WPC en gpio\n");
				goto error;
			}
		}
		queue_delayed_work(smb345_wq, &charger->wireless_det_work, WPC_INIT_DET_INTERVAL);
	}

	printk("smb345_probe-\n");

	return 0;
error:
	kfree(charger);
	return ret;
}

static int __devexit smb345_remove(struct i2c_client *client)
{
	struct smb345_charger *charger = i2c_get_clientdata(client);
	kfree(charger);
	return 0;
}

static int smb345_suspend(struct i2c_client *client, pm_message_t mesg)
{
	printk("smb345_suspend+\n");
	flush_workqueue(smb345_wq);
	if (ac_on || wireless_on || usb_on)
		smb345_config_thermal_suspend();
	printk("smb345_suspend-\n");
	return 0;
}

static int smb345_resume(struct i2c_client *client)
{
	printk("smb345_resume+\n");
	if (wireless_on != !(gpio_get_value(charger->wpc_pok_gpio)))
		wake_lock_timeout(&charger_wakelock, 2*HZ);
	printk("smb345_resume-\n");
	return 0;
}

void smb345_shutdown(struct i2c_client *client)
{
	printk("smb345_shutdown+\n");
	if (ac_on || wireless_on || usb_on)
		smb345_config_thermal_suspend();
	printk("smb345_shutdown-\n");
}

static const struct i2c_device_id smb345_id[] = {
	{ "smb345", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, smb345_id);

static struct i2c_driver smb345_i2c_driver = {
	.driver	= {
		.name	= "smb345",
	},
	.probe		= smb345_probe,
	.remove		= __devexit_p(smb345_remove),
	.shutdown	= smb345_shutdown,
	.suspend 		= smb345_suspend,
	.resume 		= smb345_resume,
	.id_table	= smb345_id,
};

static int __init smb345_init(void)
{
	return i2c_add_driver(&smb345_i2c_driver);
}
module_init(smb345_init);

static void __exit smb345_exit(void)
{
	i2c_del_driver(&smb345_i2c_driver);
}
module_exit(smb345_exit);

MODULE_DESCRIPTION("smb345 Battery-Charger");
MODULE_LICENSE("GPL");
