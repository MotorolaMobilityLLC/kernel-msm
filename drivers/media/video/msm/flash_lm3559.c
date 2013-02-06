/*
 * Flash LED driver (LM3559)
 *
 * Copyright (C) 2011 LGE, Inc.
 *
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/init.h>
#include <linux/leds.h>
#include <linux/errno.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/delay.h>
#include <linux/hrtimer.h>
#include <linux/types.h>
#include <linux/platform_data/flash_lm3559.h>
#include <mach/camera.h>


#define LM3559_I2C_NAME  			"lm3559"

#define LM3559_POWER_OFF			0
#define LM3559_POWER_ON				1

/* Register Descriptions */
#define LM3559_REG_ENABLE			0x10
#define LM3559_REG_GPIO				0x20
#define LM3559_REG_VLED_MONITOR			0x30
#define LM3559_REG_ADC_DELAY			0x31
#define LM3559_REG_VIN_MONITOR			0x80
#define LM3559_REG_LAST_FLASH			0x81
#define LM3559_REG_TORCH_BRIGHTNESS		0xA0
#define LM3559_REG_FLASH_BRIGHTNESS		0xB0
#define LM3559_REG_FLASH_DURATION		0xC0
#define LM3559_REG_FLAGS			0xD0
#define LM3559_REG_CONFIGURATION1		0xE0
#define LM3559_REG_CONFIGURATION2		0xF0
#define LM3559_REG_PRIVACY			0x11
#define LM3559_REG_MESSAGE_INDICATOR		0x12
#define LM3559_REG_INDICATOR_BLINKING		0x13
#define LM3559_REG_PRIVACY_PWM			0x14

enum led_status {
	LM3559_LED_OFF,
	LM3559_LED_LOW,
	LM3559_LED_HIGH,
	LM3559_LED_MAX
};

static int lm3559_onoff_state = LM3559_POWER_OFF;

static struct lm3559_flash_platform_data *lm3559_led_flash_pdata = NULL;
static struct i2c_client *lm3559_i2c_client = NULL;

static int lm3559_write_reg(struct i2c_client *client, unsigned char addr, unsigned char data)
{
	int err = 0;

	unsigned char buf[2] ={0,};

	struct i2c_msg msg[] = {
		{
			.addr  = client->addr,
			.flags = 0,
			.len   = 2,
			.buf   = buf,
		},
	};

	buf[0] = addr;
	buf[1] = data;

	if ((err = i2c_transfer(client->adapter, &msg[0], 1)) < 0) {
		dev_err(&client->dev, "i2c write error [%d]\n",err);
	}

	return err;

}

static int lm3559_read_reg(struct i2c_client *client, unsigned char addr, unsigned char *data)
{
	int err = 0;
	unsigned char buf[1] ={0};

	struct i2c_msg msgs[] = {
		{
			.addr  = client->addr,
			.flags = I2C_M_RD,
			.len   = 1,
			.buf   = buf,
		},
	};

	buf[0] = addr;

	if ((err = i2c_transfer(client->adapter, &msgs[0], 1)) < 0) {
		dev_err(&client->dev, "i2c read error [%d]\n",err);
	}

	*data = buf[0];

	return err;

}

#if 0
static void lm3559_led_shutdown(void)
{
	lm3559_write_reg(lm3559_i2c_client, LM3559_REG_ENABLE, 0x18);
}
#endif

/*	Torch Current
	 000 : 28.125 mA		100 : 140.625 mA
	 001 : 56.25 mA 		101 : 168.75 mA
	 010 : 84.375 mA 		110 : 196.875 mA
	 011 : 112.5mA  		111 : 225 mA
*/
static void lm3559_enable_torch_mode(enum led_status state)
{
	pr_info("%s: state = %d\n", __func__, state);

	if (state == LM3559_LED_LOW) {
		/* 011 011 : 112.5 mA */
		lm3559_write_reg(lm3559_i2c_client, LM3559_REG_TORCH_BRIGHTNESS, 0x1B);
	} else {
		/* 111 111 : 225 mA */
		lm3559_write_reg(lm3559_i2c_client, LM3559_REG_TORCH_BRIGHTNESS, 0x3F);
	}

	lm3559_write_reg(lm3559_i2c_client, LM3559_REG_ENABLE, 0x1A);
}

/*	 Flash Current
	 0000 : 56.25 mA		1000 : 506.25 mA
	 0001 : 112.5 mA 		1001 : 562.5 mA
	 0010 : 168.75 mA 		1010 : 618.75 mA
	 0011 : 225 mA  		1011 : 675 mA
	 0100 : 281.25 mA		1100 : 731.25 mA
	 0101 : 337.5 mA		1101 : 787.5 mA
	 0110 : 393.75 mA		1110 : 843.75 mA
	 0111 : 450 mA			1111 : 900 mA
*/
static void lm3559_enable_flash_mode(enum led_status state)
{
	unsigned char data = 0;

	pr_info("%s: state = %d\n", __func__, state);

	lm3559_read_reg(lm3559_i2c_client, LM3559_REG_FLASH_DURATION, &data);

	pr_debug("%s: Before - LM3559_REG_FLASH_DURATION[0x%x]\n",__func__,data);
	data = ((data & 0x1F) | 0x1F); /* 1.4A Peak Current & 1024ms Duration*/

	pr_debug("%s: After - LM3559_REG_FLASH_DURATION[0x%x]\n",__func__,data);
	lm3559_write_reg(lm3559_i2c_client, LM3559_REG_FLASH_DURATION, data);

	if (state == LM3559_LED_LOW) {
		/* 0001 0001 : 112.5 mA => 0100 0100: 281.25 mA*/
		CDBG("[LM3559_LED_LOW]LM3559_REG_FLASH_BRIGHTNESS \n");
		lm3559_write_reg(lm3559_i2c_client, LM3559_REG_FLASH_BRIGHTNESS, 0x44);
	}
	else {
		/*0011 0011 : 225 mA => 0110 0110 : 393.75 mA => 1010 1010: 618.75 mA*/
		CDBG("[LM3559_LED_HIGH]LM3559_REG_FLASH_BRIGHTNESS \n");
		lm3559_write_reg(lm3559_i2c_client, LM3559_REG_FLASH_BRIGHTNESS, 0xAA);
	}
	lm3559_write_reg(lm3559_i2c_client, LM3559_REG_ENABLE, 0x1B);
}

static int lm3559_config_gpio_on(void)
{
	int rc = 0;
	pr_debug("%s\n", __func__);

	rc = gpio_request(lm3559_led_flash_pdata->gpio_en, "cam_flash_en");
	if (rc < 0) {
		pr_warn("%s: gpio_request failed: %d\n", __func__, rc);
		return rc;
	}

	gpio_tlmm_config(GPIO_CFG(lm3559_led_flash_pdata->gpio_en, 0, GPIO_CFG_OUTPUT,
		GPIO_CFG_PULL_UP, GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_direction_output(lm3559_led_flash_pdata->gpio_en, 0);
	return rc;
}

static void lm3559_config_gpio_off(void)
{
	pr_info("%s\n", __func__);

	gpio_direction_input(lm3559_led_flash_pdata->gpio_en);
	gpio_free(lm3559_led_flash_pdata->gpio_en);
}

static void lm3559_led_enable(void)
{
	pr_info("%s\n", __func__);
	gpio_set_value_cansleep(lm3559_led_flash_pdata->gpio_en, 1);
	lm3559_onoff_state = LM3559_POWER_ON;
}

static void lm3559_led_disable(void)
{
	pr_info("%s\n", __func__);
	gpio_set_value_cansleep(lm3559_led_flash_pdata->gpio_en, 0);
	lm3559_onoff_state = LM3559_POWER_OFF;
}

int lm3559_flash_set_led_state(int led_state)
{
	int err = 0;

	pr_info("%s: led_state = %d\n", __func__, led_state);

	switch (led_state) {
	case MSM_CAMERA_LED_OFF:
		lm3559_led_disable();
		break;
	case MSM_CAMERA_LED_LOW:
		lm3559_led_enable();
		lm3559_enable_torch_mode(LM3559_LED_LOW);
		break;
	case MSM_CAMERA_LED_HIGH:
		lm3559_led_enable();
		lm3559_enable_flash_mode(LM3559_LED_HIGH);
		break;
	case MSM_CAMERA_LED_INIT:
		err = lm3559_config_gpio_on();
		break;
	case MSM_CAMERA_LED_RELEASE:
		lm3559_config_gpio_off();
		break;
	default:
		err = -EINVAL;
		break;
	}

	return err;
}

EXPORT_SYMBOL(lm3559_flash_set_led_state);

static void lm3559_flash_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	pr_info("%s: led_cdev->brightness[%d]\n", __func__, value);

	led_cdev->brightness = value;

	if (value)
		lm3559_enable_torch_mode(LM3559_LED_LOW);
	else
		lm3559_led_disable();
}

static struct led_classdev lm3559_flash_led = {
	.name = "spotlight",
	.brightness_set	= lm3559_flash_led_set,
};

static int lm3559_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	int err = 0;

	lm3559_i2c_client = client;
	lm3559_led_flash_pdata = client->dev.platform_data;

	if (lm3559_led_flash_pdata == NULL) {
	    pr_err("%s: platform_data is NULL\n", __func__);
	    return -ENODEV;
	}

	err = led_classdev_register(&client->dev, &lm3559_flash_led);
	if (err < 0) {
		pr_err("%s: failed to register\n", __func__);
		return err;
	}

	pr_debug("%s: done\n", __func__);

	return err;
}

static int lm3559_remove(struct i2c_client *client)
{
	led_classdev_unregister(&lm3559_flash_led);

	lm3559_i2c_client = NULL;
	lm3559_led_flash_pdata = NULL;

	return 0;
}

static const struct i2c_device_id lm3559_ids[] = {
	{ LM3559_I2C_NAME, 0 },	/* lm3559 */
	{ /* end of list */ },
};

static struct i2c_driver lm3559_driver = {
	.probe    = lm3559_probe,
	.remove   = lm3559_remove,
	.id_table = lm3559_ids,
	.driver   = {
		.name =  LM3559_I2C_NAME,
		.owner= THIS_MODULE,
	},
};
static int __init lm3559_init(void)
{
	pr_info("%s\n", __func__);
	return i2c_add_driver(&lm3559_driver);
}

static void __exit lm3559_exit(void)
{
	i2c_del_driver(&lm3559_driver);
}

module_init(lm3559_init);
module_exit(lm3559_exit);

MODULE_AUTHOR("LG Electronics");
MODULE_DESCRIPTION("LM3559 Flash Driver");
MODULE_LICENSE("GPL");

