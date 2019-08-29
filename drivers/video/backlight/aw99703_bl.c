/*
* aw99703.c   aw99703 backlight module
*
* Version: v1.0.2
*
* Copyright (c) 2019 AWINIC Technology CO., LTD
*
*  Author: Joseph <zhangzetao@awinic.com.cn>
*
* This program is free software; you can redistribute  it and/or modify it
* under  the terms of  the GNU General  Public License as published by the
* Free Software Foundation;  either version 2 of the  License, or (at your
* option) any later version.
*/

#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/platform_data/aw99703_bl.h>

#define AW99703_LED_DEV "aw99703-bl"
#define AW99703_NAME "aw99703-bl"

#define AW99703_VERSION "v1.0.2"

struct aw99703_data *g_aw99703_data;

static int platform_read_i2c_block(struct i2c_client *client, char *writebuf,
			   int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = 0,
				 .len = writelen,
				 .buf = writebuf,
			 },
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}

static int aw99703_i2c_read(struct i2c_client *client, u8 addr, u8 *val)
{
	return platform_read_i2c_block(client, &addr, 1, val, 1);
}

static int platform_write_i2c_block(struct i2c_client *client,
		char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s: i2c write error.\n", __func__);

	return ret;
}

static int aw99703_i2c_write(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;

	return platform_write_i2c_block(client, buf, sizeof(buf));
}
static void aw99703_hwen_pin_ctrl(struct aw99703_data *drvdata, int en)
{
	if (gpio_is_valid(drvdata->hwen_gpio)) {
		if (en) {
			pr_info("hwen pin is going to be high!---<%d>\n", en);
			gpio_set_value(drvdata->hwen_gpio, true);
			usleep_range(3500, 4000);
		} else {
			pr_info("hwen pin is going to be low!---<%d>\n", en);
			gpio_set_value(drvdata->hwen_gpio, false);
			usleep_range(1000, 2000);
		}
	}
}

static int aw99703_gpio_init(struct aw99703_data *drvdata)
{

	int ret;

	if (gpio_is_valid(drvdata->hwen_gpio)) {
		ret = gpio_request(drvdata->hwen_gpio, "hwen_gpio");
		if (ret < 0) {
			pr_err("failed to request gpio\n");
			return -1;
		}
		ret = gpio_direction_output(drvdata->hwen_gpio, 0);
		pr_info(" request gpio init\n");
		if (ret < 0) {
			pr_err("failed to set output");
			gpio_free(drvdata->hwen_gpio);
			return ret;
		}
		pr_info("gpio is valid!\n");
		aw99703_hwen_pin_ctrl(drvdata, 1);
	}

	return 0;
}

static int aw99703_i2c_write_bit(struct i2c_client *client,
	unsigned int reg_addr, unsigned int  mask, unsigned char reg_data)
{
	unsigned char reg_val = 0;

	aw99703_i2c_read(client, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= reg_data;
	aw99703_i2c_write(client, reg_addr, reg_val);

	return 0;
}

static int aw99703_bl_enable_channel(struct aw99703_data *drvdata)
{
	int ret = 0;

	if (drvdata->channel == 3) {
		pr_info("%s turn all channel on!\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH3_ENABLE |
						AW99703_LEDCUR_CH2_ENABLE |
						AW99703_LEDCUR_CH1_ENABLE);
	} else if (drvdata->channel == 2) {
		pr_info("%s turn two channel on!\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH2_ENABLE |
						AW99703_LEDCUR_CH1_ENABLE);
	} else if (drvdata->channel == 1) {
		pr_info("%s turn one channel on!\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH1_ENABLE);
	} else {
		pr_info("%s all channels are going to be disabled\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						0x98);
	}

	return ret;
}

static void aw99703_pwm_mode_enable(struct aw99703_data *drvdata)
{
	if (drvdata->pwm_mode) {
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_PDIS_MASK,
					AW99703_MODE_PDIS_ENABLE);
		pr_info("%s pwm_mode is enable\n", __func__);
	} else {
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_PDIS_MASK,
					AW99703_MODE_PDIS_DISABLE);
		pr_info("%s pwm_mode is disable\n", __func__);
	}
}

static void aw99703_ramp_setting(struct aw99703_data *drvdata)
{
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TURNCFG,
				AW99703_TURNCFG_ON_TIM_MASK,
				drvdata->ramp_on_time << 4);
	pr_info("%s drvdata->ramp_on_time is 0x%x\n",
		__func__, drvdata->ramp_on_time);

	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TURNCFG,
				AW99703_TURNCFG_OFF_TIM_MASK,
				drvdata->ramp_off_time);
	pr_info("%s drvdata->ramp_off_time is 0x%x\n",
		__func__, drvdata->ramp_off_time);

}
static void aw99703_transition_ramp(struct aw99703_data *drvdata)
{

	pr_info("%s enter\n", __func__);
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TRANCFG,
				AW99703_TRANCFG_PWM_TIM_MASK,
				drvdata->pwm_trans_dim);
	pr_info("%s drvdata->pwm_trans_dim is 0x%x\n", __func__,
		drvdata->pwm_trans_dim);

	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TRANCFG,
				AW99703_TRANCFG_I2C_TIM_MASK,
				drvdata->i2c_trans_dim);
	pr_info("%s drvdata->i2c_trans_dim is 0x%x\n",
		__func__, drvdata->i2c_trans_dim);

}

static int aw99703_backlight_init(struct aw99703_data *drvdata)
{
	pr_info("%s enter.\n", __func__);

	aw99703_pwm_mode_enable(drvdata);

	/*mode:map type*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_MODE,
				AW99703_MODE_MAP_MASK,
				AW99703_MODE_MAP_LINEAR);

	/*default OVPSEL 38V*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_OVPSEL_MASK,
				AW99703_BSTCTR1_OVPSEL_38V);

	/*switch frequency 1000kHz*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_SF_MASK,
				AW99703_BSTCTR1_SF_1000KHZ);

	/*OCP SELECT*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_OCPSEL_MASK,
				AW99703_BSTCTR1_OCPSEL_3P3A);

	/*BSTCRT2 IDCTSEL*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR2,
				AW99703_BSTCTR2_IDCTSEL_MASK,
				AW99703_BSTCTR2_IDCTSEL_10UH);

	/*Backlight current full scale*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_LEDCUR,
				AW99703_LEDCUR_BLFS_MASK,
				drvdata->full_scale_led << 3);

	aw99703_bl_enable_channel(drvdata);

	aw99703_ramp_setting(drvdata);
	aw99703_transition_ramp(drvdata);

	return 0;
}

static int aw99703_backlight_enable(struct aw99703_data *drvdata)
{
	pr_info("%s enter.\n", __func__);

	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_MODE,
				AW99703_MODE_WORKMODE_MASK,
				AW99703_MODE_WORKMODE_BACKLIGHT);

	drvdata->enable = true;

	return 0;
}

int  aw99703_set_brightness(struct aw99703_data *drvdata, int brt_val)
{
	pr_info("%s brt_val is %d\n", __func__, brt_val);
	if (drvdata->enable == false) {
		aw99703_backlight_init(drvdata);
		aw99703_backlight_enable(drvdata);
	}

	if (brt_val > 0) {
		/*enalbe bl mode*/
		/* set backlight brt_val */
		aw99703_i2c_write(drvdata->client,
				AW99703_REG_LEDLSB,
				brt_val&0x0007);
		aw99703_i2c_write(drvdata->client,
				AW99703_REG_LEDMSB,
				(brt_val >> 3)&0xff);

		/* backlight enable */
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_BACKLIGHT);
	} else {
		/* standby mode*/
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_STANDBY);
	}

	drvdata->brightness = brt_val;

	if (drvdata->brightness == 0)
		drvdata->enable = false;
	return 0;
}


static int aw99703_bl_get_brightness(struct backlight_device *bl_dev)
{
		return bl_dev->props.brightness;
}

static int aw99703_bl_update_status(struct backlight_device *bl_dev)
{
		struct aw99703_data *drvdata = bl_get_data(bl_dev);
		int brt;

		if (bl_dev->props.state & BL_CORE_SUSPENDED)
				bl_dev->props.brightness = 0;

		brt = bl_dev->props.brightness;
		/*
		 * Brightness register should always be written
		 * not only register based mode but also in PWM mode.
		 */
		return aw99703_set_brightness(drvdata, brt);
}

static const struct backlight_ops aw99703_bl_ops = {
		.update_status = aw99703_bl_update_status,
		.get_brightness = aw99703_bl_get_brightness,
};


static int aw99703_read_chipid(struct aw99703_data *drvdata)
{
	int ret = -1;
	u8 value = 0;
	unsigned char cnt = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw99703_i2c_read(drvdata->client, 0x00, &value);
		if (ret < 0) {
			pr_err("%s: failed to read reg AW99703_REG_ID: %d\n",
				__func__, ret);
		}
		switch (value) {
		case 0x03:
			pr_info("%s aw99703 detected\n", __func__);
			return 0;
		default:
			pr_info("%s unsupported device revision (0x%x)\n",
				__func__, value);
			break;
		}
		cnt++;

		msleep(AW_READ_CHIPID_RETRY_DELAY);
	}

	return -EINVAL;
}


static void __aw99703_work(struct aw99703_data *led,
				enum led_brightness value)
{
	mutex_lock(&led->lock);
	aw99703_set_brightness(led, value);
	mutex_unlock(&led->lock);
}

static void aw99703_work(struct work_struct *work)
{
	struct aw99703_data *drvdata = container_of(work,
					struct aw99703_data, work);

	__aw99703_work(drvdata, drvdata->led_dev.brightness);
}


static void aw99703_brightness_set(struct led_classdev *led_cdev,
			enum led_brightness brt_val)
{
	struct aw99703_data *drvdata;

	drvdata = container_of(led_cdev, struct aw99703_data, led_dev);
	schedule_work(&drvdata->work);
}

static void
aw99703_get_dt_data(struct device *dev, struct aw99703_data *drvdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	u32 bl_channel, temp;

	drvdata->hwen_gpio = of_get_named_gpio(np, "aw99703,hwen-gpio", 0);
	pr_info("%s drvdata->hwen_gpio --<%d>\n", __func__, drvdata->hwen_gpio);

	rc = of_property_read_u32(np, "aw99703,pwm-mode", &drvdata->pwm_mode);
	if (rc != 0)
		pr_err("%s pwm-mode not found\n", __func__);
	else
		pr_info("%s pwm_mode=%d\n", __func__, drvdata->pwm_mode);

	drvdata->using_lsb = of_property_read_bool(np, "aw99703,using-lsb");
	pr_info("%s using_lsb --<%d>\n", __func__, drvdata->using_lsb);

	if (drvdata->using_lsb) {
		drvdata->default_brightness = 0x7ff;
		drvdata->max_brightness = 2047;
	} else {
		drvdata->default_brightness = 0xff;
		drvdata->max_brightness = 255;
	}

	rc = of_property_read_u32(np, "aw99703,bl-fscal-led", &temp);
	if (rc) {
		pr_err("Invalid backlight full-scale led current!\n");
	} else {
		drvdata->full_scale_led = temp;
		pr_info("%s full-scale led current --<%d mA>\n",
			__func__, drvdata->full_scale_led);
	}

	rc = of_property_read_u32(np, "aw99703,turn-on-ramp", &temp);
	if (rc) {
		pr_err("Invalid ramp timing ,turnon!\n");
	} else {
		drvdata->ramp_on_time = temp;
		pr_info("%s ramp on time --<%d ms>\n",
			__func__, drvdata->ramp_on_time);
	}

	rc = of_property_read_u32(np, "aw99703,turn-off-ramp", &temp);
	if (rc) {
		pr_err("Invalid ramp timing ,,turnoff!\n");
	} else {
		drvdata->ramp_off_time = temp;
		pr_info("%s ramp off time --<%d ms>\n",
			__func__, drvdata->ramp_off_time);
	}

	rc = of_property_read_u32(np, "aw99703,pwm-trans-dim", &temp);
	if (rc) {
		pr_err("Invalid pwm-tarns-dim value!\n");
	} else {
		drvdata->pwm_trans_dim = temp;
		pr_info("%s pwm trnasition dimming	--<%d ms>\n",
			__func__, drvdata->pwm_trans_dim);
	}

	rc = of_property_read_u32(np, "aw99703,i2c-trans-dim", &temp);
	if (rc) {
		pr_err("Invalid i2c-trans-dim value !\n");
	} else {
		drvdata->i2c_trans_dim = temp;
		pr_info("%s i2c transition dimming --<%d ms>\n",
			__func__, drvdata->i2c_trans_dim);
	}

	rc = of_property_read_u32(np, "aw99703,bl-channel", &bl_channel);
	if (rc) {
		pr_err("Invalid channel setup\n");
	} else {
		drvdata->channel = bl_channel;
		pr_info("%s bl-channel --<%x>\n", __func__, drvdata->channel);
	}
}

/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw99703_i2c_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw99703_data *aw99703 = dev_get_drvdata(dev);

	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw99703_i2c_write(aw99703->client,
				(unsigned char)databuf[0],
				(unsigned char)databuf[1]);
	}

	return count;
}

static ssize_t aw99703_i2c_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct aw99703_data *aw99703 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW99703_REG_MAX; i++) {
		if (!(aw99703_reg_access[i]&REG_RD_ACCESS))
			continue;
		aw99703_i2c_read(aw99703->client, i, &reg_val);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n",
				i, reg_val);
	}

	return len;
}

static DEVICE_ATTR(reg, 0664, aw99703_i2c_reg_show, aw99703_i2c_reg_store);
static struct attribute *aw99703_attributes[] = {
	&dev_attr_reg.attr,
	NULL
};

static struct attribute_group aw99703_attribute_group = {
	.attrs = aw99703_attributes
};

static int aw99703_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct aw99703_data *drvdata;
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	int err = 0;

	pr_info("%s enter! driver version %s\n", __func__, AW99703_VERSION);

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : I2C_FUNC_I2C not supported\n", __func__);
		err = -EIO;
		goto err_out;
	}

	if (!client->dev.of_node) {
		pr_err("%s : no device node\n", __func__);
		err = -ENOMEM;
		goto err_out;
	}

	drvdata = kzalloc(sizeof(struct aw99703_data), GFP_KERNEL);
	if (drvdata == NULL) {
		pr_err("%s : kzalloc failed\n", __func__);
		err = -ENOMEM;
		goto err_out;
	}

	drvdata->client = client;
	drvdata->adapter = client->adapter;
	drvdata->addr = client->addr;
	drvdata->brightness = LED_OFF;
	drvdata->enable = true;
	drvdata->led_dev.default_trigger = "bkl-trigger";
	drvdata->led_dev.name = AW99703_LED_DEV;
	drvdata->led_dev.brightness_set = aw99703_brightness_set;
	drvdata->led_dev.max_brightness = MAX_BRIGHTNESS;
	mutex_init(&drvdata->lock);
	INIT_WORK(&drvdata->work, aw99703_work);
	aw99703_get_dt_data(&client->dev, drvdata);
	i2c_set_clientdata(client, drvdata);
	aw99703_gpio_init(drvdata);

	err = aw99703_read_chipid(drvdata);
	if (err < 0) {
		pr_err("%s : ID idenfy failed\n", __func__);
		goto err_init;
	}

	err = led_classdev_register(&client->dev, &drvdata->led_dev);
	if (err < 0) {
		pr_err("%s : Register led class failed\n", __func__);
		err = -ENODEV;
		goto err_init;
	} else {
	pr_debug("%s: Register led class successful\n", __func__);
	}

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.brightness = MAX_BRIGHTNESS;
	props.max_brightness = MAX_BRIGHTNESS;
	bl_dev = backlight_device_register(AW99703_NAME, &client->dev,
					drvdata, &aw99703_bl_ops, &props);

	g_aw99703_data = drvdata;
	aw99703_backlight_init(drvdata);
	aw99703_backlight_enable(drvdata);

	err = sysfs_create_group(&client->dev.kobj, &aw99703_attribute_group);
	if (err < 0) {
		dev_info(&client->dev, "%s error creating sysfs attr files\n",
			__func__);
		goto err_sysfs;
	}
	pr_info("%s exit\n", __func__);
	return 0;

err_sysfs:
err_init:
	kfree(drvdata);
err_out:
	return err;
}

static int aw99703_remove(struct i2c_client *client)
{
	struct aw99703_data *drvdata = i2c_get_clientdata(client);

	led_classdev_unregister(&drvdata->led_dev);

	kfree(drvdata);
	return 0;
}

static const struct i2c_device_id aw99703_id[] = {
	{AW99703_NAME, 0},
	{}
};
static struct of_device_id match_table[] = {
		{.compatible = "awinic,aw99703-bl",}
};

MODULE_DEVICE_TABLE(i2c, aw99703_id);

static struct i2c_driver aw99703_i2c_driver = {
	.probe = aw99703_probe,
	.remove = aw99703_remove,
	.id_table = aw99703_id,
	.driver = {
		.name = AW99703_NAME,
		.owner = THIS_MODULE,
		.of_match_table = match_table,
	},
};

module_i2c_driver(aw99703_i2c_driver);
MODULE_DESCRIPTION("Back Light driver for aw99703");
MODULE_LICENSE("GPL v2");
