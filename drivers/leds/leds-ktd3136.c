/*
 * KTD3136_BL Driver
 *
 * SiliconMitus KTD3136 Backlight driver chip
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
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
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif



#define KTD3136_LED_DEV "ktd3136-BL"
#define KTD3136_NAME "ktd3136-bl"

#define MAX_BRIGHTNESS			(2047)
#define BANK_NONE				0x00
#define REG_DEV_ID				0x00
#define REG_SW_RESET				0x01
#define	REG_MODE				0x02
#define REG_CONTROL				0x03
#define REG_RATIO_LSB				0x04
#define REG_RATIO_MSB			0x05
#define REG_PWM					0x06
#define REG_RAMP_ON				0x07
#define REG_TRANS_RAMP			0x08
#define REG_FLASH_SETTING			0x09
#define REG_STATUS				0x0A
#define BIT_CH3_FAULT			BIT(7)
#define BIT_CH2_FAULT			BIT(6)
#define BIT_CH1_FAULT			BIT(5)
#define BIT_FLASH_TIMEOUT		BIT(4)
#define BIT_OVP					BIT(3)
#define BIT_UVLO				BIT(2)
#define BIT_OCP					BIT(1)
#define BIT_THERMAL_SHUTDOWN	BIT(0)

#define RESET_CONDITION_BITS	 (BIT_CH3_FAULT | BIT_CH2_FAULT | BIT_CH1_FAULT | BIT_OVP | BIT_OCP)


int ktd3136_brightness_table_reg4[256] = {0x01,0x02,0x04,0x04,0x07,0x02,0x00,0x06,0x04,0x02,0x03,0x04,0x05,0x06,0x02,0x06,0x02,
			0x06,0x02,0x06,0x02,0x06,0x02,0x04,0x05,0x06,0x05,0x03,0x00,0x05,0x02,0x06,0x02,0x06,0x02,0x06,0x02,0x06,0x02,0x06,
			0x02,0x06,0x02,0x06,0x02,0x06,0x01,0x04,0x07,0x02,0x05,0x00,0x03,0x06,0x01,0x04,0x07,0x02,0x05,0x00,0x03,0x05,0x07,
			0x01,0x03,0x05,0x07,0x01,0x03,0x05,0x07,0x01,0x03,0x05,0x07,0x01,0x02,0x03,0x04,0x05,0x06,0x07,0x00,0x01,0x02,0x03,
			0x04,0x05,0x06,0x07,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x00,0x07,0x06,0x05,
			0x04,0x03,0x02,0x01,0x00,0x07,0x06,0x05,0x04,0x03,0x02,0x01,0x07,0x05,0x03,0x01,0x07,0x05,0x03,0x01,0x07,0x05,0x03,
			0x01,0x07,0x05,0x03,0x00,0x05,0x02,0x07,0x04,0x01,0x06,0x03,0x00,0x05,0x02,0x07,0x04,0x01,0x06,0x03,0x00,0x05,0x02,
			0x07,0x03,0x07,0x03,0x07,0x03,0x07,0x03,0x07,0x03,0x07,0x03,0x07,0x03,0x07,0x03,0x07,0x03,0x07,0x03,0x07,0x03,0x07,
			0x03,0x07,0x03,0x07,0x03,0x06,0x01,0x04,0x07,0x02,0x05,0x00,0x03,0x06,0x01,0x04,0x07,0x02,0x05,0x00,0x03,0x06,0x01,
			0x04,0x07,0x02,0x05,0x07,0x01,0x03,0x05,0x07,0x01,0x03,0x05,0x07,0x01,0x03,0x05,0x07,0x01,0x03,0x05,0x07,0x01,0x03,
			0x05,0x07,0x01,0x03,0x05,0x07,0x01,0x03,0x05,0x07,0x01,0x03,0x05,0x07,0x01,0x03,0x05,0x07,0x01,0x03,0x05,0x07,0x01,
			0x03,0x05,0x07,0x01,0x03,0x04,0x05,0x06,0x07};
int ktd3136_brightness_table_reg5[256] = {0x00,0x06,0x0C,0x11,0x15,0x1A,0x1E,0x21,0x25,0x29,0x2C,0x2F,0x32,0x35,0x38,0x3A,0x3D,
			0x3F,0x42,0x44,0x47,0x49,0x4C,0x4E,0x50,0x52,0x54,0x56,0x58,0x59,0x5B,0x5C,0x5E,0x5F,0x61,0x62,0x64,0x65,0x67,0x68,
			0x6A,0x6B,0x6D,0x6E,0x70,0x71,0x73,0x74,0x75,0x77,0x78,0x7A,0x7B,0x7C,0x7E,0x7F,0x80,0x82,0x83,0x85,0x86,0x87,0x88,
			0x8A,0x8B,0x8C,0x8D,0x8F,0x90,0x91,0x92,0x94,0x95,0x96,0x97,0x99,0x9A,0x9B,0x9C,0x9D,0x9E,0x9F,0xA1,0xA2,0xA3,0xA4,
			0xA5,0xA6,0xA7,0xA8,0xAA,0xAB,0xAC,0xAD,0xAE,0xAF,0xB0,0xB1,0xB2,0xB3,0xB4,0xB5,0xB6,0xB7,0xB8,0xB9,0xB9,0xBA,0xBB,
			0xBC,0xBD,0xBE,0xBF,0xC0,0xC0,0xC1,0xC2,0xC3,0xC4,0xC5,0xC6,0xC6,0xC7,0xC8,0xC9,0xC9,0xCA,0xCB,0xCC,0xCC,0xCD,0xCE,
			0xCF,0xCF,0xD0,0xD1,0xD2,0xD2,0xD3,0xD3,0xD4,0xD5,0xD5,0xD6,0xD7,0xD7,0xD8,0xD8,0xD9,0xDA,0xDA,0xDB,0xDC,0xDC,0xDD,
			0xDD,0xDE,0xDE,0xDF,0xDF,0xE0,0xE0,0xE1,0xE1,0xE2,0xE2,0xE3,0xE3,0xE4,0xE4,0xE5,0xE5,0xE6,0xE6,0xE7,0xE7,0xE8,0xE8,
			0xE9,0xE9,0xEA,0xEA,0xEB,0xEB,0xEC,0xEC,0xEC,0xED,0xED,0xEE,0xEE,0xEE,0xEF,0xEF,0xEF,0xF0,0xF0,0xF1,0xF1,0xF1,0xF2,
			0xF2,0xF2,0xF3,0xF3,0xF3,0xF4,0xF4,0xF4,0xF4,0xF5,0xF5,0xF5,0xF5,0xF6,0xF6,0xF6,0xF6,0xF7,0xF7,0xF7,0xF7,0xF8,0xF8,
			0xF8,0xF8,0xF9,0xF9,0xF9,0xF9,0xFA,0xFA,0xFA,0xFA,0xFB,0xFB,0xFB,0xFB,0xFC,0xFC,0xFC,0xFC,0xFD,0xFD,0xFD,0xFD,0xFE,
			0xFE,0xFE,0xFE,0xFF,0xFF,0xFF,0xFF,0xFF,0xFF};

struct ktd3136_data {
	struct led_classdev led_dev;
	struct i2c_client *client;
	struct i2c_adapter *adapter;
	unsigned short addr;
	struct mutex		lock;
	struct work_struct	work;
	enum led_brightness brightness;
#ifdef CONFIG_FB
	struct notifier_block fb_notif;
#endif
	bool enable;
	u8 pwm_cfg;
	u8 boost_ctl;
	u8 full_scale_current;
	bool brt_code_enable;
	u16 *brt_code_table;
	int hwen_gpio;
	bool pwm_mode;
	bool using_lsb;
	unsigned int pwm_period;
	unsigned int full_scale_led;
	unsigned int ramp_on_time;
	unsigned int ramp_off_time;
	unsigned int pwm_trans_dim;
	unsigned int i2c_trans_dim;
	unsigned int channel;
	unsigned int ovp_level;
	unsigned int frequency;
	unsigned int default_brightness;
	unsigned int max_brightness;
	unsigned int induct_current;
	unsigned int flash_current;
	unsigned int flash_timeout;

};

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

static int ktd3136_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
	return platform_read_i2c_block(client, &addr, 1, val, 1);
}

static int platform_write_i2c_block(struct i2c_client *client, char *writebuf, int writelen)
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

static int ktd3136_write_reg(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;
	return platform_write_i2c_block(client, buf, sizeof(buf));
}
static void ktd3136_hwen_pin_ctrl(struct ktd3136_data *drvdata, int en)
{
	if (gpio_is_valid(drvdata->hwen_gpio)) {
		if (en) {
			pr_debug("hwen pin is going to be high!---<%d>\n", en);
			gpio_set_value(drvdata->hwen_gpio, true);
		} else {
			pr_debug("hwen pin is going to be low!---<%d>\n", en);
			gpio_set_value(drvdata->hwen_gpio, false);
		}
		msleep(1);
	}
}



static int ktd3136_gpio_init(struct ktd3136_data *drvdata)
{

	int ret;

	if (gpio_is_valid(drvdata->hwen_gpio)) {
		ret = gpio_request(drvdata->hwen_gpio, "ktd_hwen_gpio");
		if (ret<0) {
			pr_err("failed to request gpio\n");
			return -1;
		}
		ret = gpio_direction_output(drvdata->hwen_gpio, 0);
		pr_debug(" request gpio init\n");
		if (ret<0) {
			pr_err("failed to set output");
			gpio_free(drvdata->hwen_gpio);
			return ret;
		}
		pr_debug("gpio is valid!\n");
		ktd3136_hwen_pin_ctrl(drvdata, 1);
	}

	return 0;
}
static int ktd3136_masked_write(struct i2c_client *client, int reg, u8 mask, u8 val)
{
	int rc;
	u8 temp;

	rc = ktd3136_read_reg(client, reg, &temp);
	if (rc < 0) {
		pr_err("failed to read reg=0x%x, rc=%d\n", reg, rc);
	} else {
		temp &= ~mask;
		temp |= val & mask;
		rc = ktd3136_write_reg(client, reg, temp);
		if (rc<0) {
			pr_err( "failed to write masked data. reg=%03x, rc=%d\n", reg, rc);
		}
	}

	ktd3136_read_reg(client, reg, &temp);
	return rc;
}

static int ktd3136_bl_enable_channel(struct ktd3136_data *drvdata)
{
	int ret;

	if (drvdata->channel == 0) {
		//default value for mode Register, all channel disabled.
		pr_debug("all channels are going to be disabled\n");
		ret = ktd3136_write_reg(drvdata->client, REG_PWM, 0x18);//b0001 1000
	} else if (drvdata->channel == 3) {
		pr_debug("turn all channel on!\n");
		ret = ktd3136_masked_write(drvdata->client, REG_PWM, 0x07, 0x07);
	} else if (drvdata->channel == 2) {
		ret = ktd3136_masked_write(drvdata->client, REG_PWM, 0x07, 0x03);
	}

	return ret;
}
static void ktd3136_pwm_mode_enable(struct ktd3136_data *drvdata, bool en)
{
	u8 value;

	if (en) {
		if (drvdata->pwm_mode) {
			pr_debug("already activated!\n");
		} else {
			drvdata->pwm_mode = en;
		}
		ktd3136_masked_write(drvdata->client, REG_PWM, 0x80, 0x80);
	} else {
		if (drvdata->pwm_mode) {
			drvdata->pwm_mode = en;
		}
		ktd3136_masked_write(drvdata->client, REG_PWM, 0x80, 0x00);
	}

	ktd3136_read_reg(drvdata->client, REG_PWM, &value);
	pr_debug("current pwm_mode is --<%x>\n", value);
}
static int ktd_find_bit(int x)
{
	int i = 0;

	while ((x = x >> 1))
		i++;

	return i+1;
}

static void ktd3136_ramp_setting(struct ktd3136_data *drvdata)
{
	unsigned int max_time = 16384;
	int temp = 0;

	if (drvdata->ramp_on_time == 0) {//512us
		ktd3136_masked_write(drvdata->client, REG_RAMP_ON, 0xf0, 0x00);
		pr_debug("rampon time is 0 \n");
	} else if (drvdata->ramp_on_time > max_time) {
		ktd3136_masked_write(drvdata->client, REG_RAMP_ON, 0xf0, 0xf0);
		pr_debug("rampon time is max \n");
	} else {
		temp = ktd_find_bit(drvdata->ramp_on_time);
		ktd3136_masked_write(drvdata->client, REG_RAMP_ON, 0xf0, temp<<4);
		pr_debug("temp is %d\n", temp);
	}

	if (drvdata->ramp_off_time == 0) {//512us
		ktd3136_masked_write(drvdata->client, REG_RAMP_ON, 0x0f, 0x00);
		pr_debug("rampoff time is 0 \n");
	} else if (drvdata->ramp_off_time > max_time) {
		ktd3136_masked_write(drvdata->client, REG_RAMP_ON, 0x0f, 0x0f);
		pr_debug("rampoff time is max \n");
	} else {
		temp = ktd_find_bit(drvdata->ramp_off_time);
		ktd3136_masked_write(drvdata->client, REG_RAMP_ON, 0x0f, temp);
		pr_debug("temp is %d\n", temp);
	}

}
static void ktd3136_transition_ramp(struct ktd3136_data *drvdata)
{
	int reg_i2c, reg_pwm, temp;

	if (drvdata->i2c_trans_dim >= 1024) {
		reg_i2c = 0xf;
	} else if (drvdata->i2c_trans_dim < 128) {
		reg_i2c = 0x0;
	} else {
		temp =drvdata->i2c_trans_dim/64;
		reg_i2c = temp-1;
		pr_debug("reg_i2c is --<0x%x>\n", reg_i2c);
	}

	if(drvdata->pwm_trans_dim >= 256){
		reg_pwm = 0x7;
	}else if(drvdata->pwm_trans_dim < 4){
		reg_pwm = 0x0;
	}else{
		temp = ktd_find_bit(drvdata->pwm_trans_dim);
		reg_pwm = temp -2;
		pr_debug("temp is %d\n", temp);
	}

	ktd3136_masked_write(drvdata->client, REG_TRANS_RAMP, 0x70, reg_pwm);
	ktd3136_masked_write(drvdata->client, REG_TRANS_RAMP, 0x0f, reg_i2c);

}

static int ktd3136_backlight_init(struct ktd3136_data *drvdata)
{
	int err = 0;
	u8 value;
	u8 update_value;
	update_value = (drvdata->ovp_level == 32) ? 0x20 : 0x00;
	(drvdata->induct_current == 2600) ? update_value |=0x0E : update_value;
	(drvdata->frequency == 1000) ? update_value |=0x40: update_value;

	ktd3136_write_reg(drvdata->client, REG_CONTROL, update_value);
	ktd3136_bl_enable_channel(drvdata);
		if (drvdata->pwm_mode) {
			ktd3136_pwm_mode_enable(drvdata, true);
		} else {
			ktd3136_pwm_mode_enable(drvdata, false);
			}
	ktd3136_ramp_setting(drvdata);
	ktd3136_transition_ramp(drvdata);
	ktd3136_read_reg(drvdata->client, REG_CONTROL, &value);
	pr_debug("read control register -before--<0x%x> -after--<0x%x> \n",
					update_value, value);

	return err;
}

static int ktd3136_backlight_enable(struct ktd3136_data *drvdata)
{
	int err = 0;
	ktd3136_masked_write(drvdata->client, REG_MODE, 0xf8, drvdata->full_scale_led);
	drvdata->enable = true;

	return err;
}

static int ktd3136_backlight_reset(struct ktd3136_data *drvdata)
{
	int err = 0;
	ktd3136_masked_write(drvdata->client, REG_SW_RESET, 0x01, 0x01);
	return err;
}

static int ktd3136_check_id(struct ktd3136_data *drvdata)
{
	u8 value=0;
	int err = 0;
	ktd3136_read_reg(drvdata->client, 0x00, &value);
	if (value!=0x18) {
		pr_err("%s : ID check err\n", __func__);
		err = -EINVAL;
		return err;
	}

	return err;
}
static void ktd3136_check_status(struct ktd3136_data *drvdata)
{
	u8 value=0;

	ktd3136_read_reg(drvdata->client, REG_STATUS, &value);
	if (value) {
		pr_err("status bit has been change! <%x>", value);

		if (value & RESET_CONDITION_BITS) {
			ktd3136_backlight_reset(drvdata);
			ktd3136_backlight_init(drvdata);
			ktd3136_backlight_enable(drvdata);
		}

	}
	return ;
}

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				       unsigned long event, void *data)
{
	int *blank;
	struct fb_event *evdata = data;
	struct ktd3136_data *drvdata =
		container_of(self, struct ktd3136_data, fb_notif);

	/*
	 *  FB_EVENT_BLANK(0x09): A hardware display blank change occurred.
	 *  FB_EARLY_EVENT_BLANK(0x10): A hardware display blank early change
	 * occurred.
	 */
	if (evdata && evdata->data && (event == FB_EARLY_EVENT_BLANK)) {
		blank = evdata->data;
		if (*blank == FB_BLANK_POWERDOWN)
			drvdata->enable = false;
	}

	return NOTIFY_OK;
}
#endif

void ktd3136_set_brightness(struct ktd3136_data *drvdata, int brt_val)
{
	if (drvdata->enable == false)
		ktd3136_backlight_init(drvdata);

	pr_debug("brt_val is %d \n", brt_val);
	if (brt_val>0) {
		ktd3136_masked_write(drvdata->client, REG_MODE, 0x01, 0x01); //enalbe bl mode
	} else {
		ktd3136_masked_write(drvdata->client, REG_MODE, 0x01, 0x00); //disable bl mode
	}
	if (drvdata->using_lsb) {
		ktd3136_masked_write(drvdata->client, REG_RATIO_LSB, 0x07, brt_val);
		ktd3136_masked_write(drvdata->client, REG_RATIO_MSB, 0xff, brt_val>>3);
	} else {
		ktd3136_masked_write(drvdata->client, REG_RATIO_LSB, 0x07, ktd3136_brightness_table_reg4[brt_val]);
		ktd3136_masked_write(drvdata->client, REG_RATIO_MSB, 0xff, ktd3136_brightness_table_reg5[brt_val]);
	}

	if (drvdata->enable == false)
		ktd3136_backlight_enable(drvdata);

	drvdata->brightness = brt_val;

	if (drvdata->brightness == 0)
		drvdata->enable = false;

}

static void __ktd3136_work(struct ktd3136_data *led,
				enum led_brightness value)
{
	mutex_lock(&led->lock);
	ktd3136_set_brightness(led, value);
	mutex_unlock(&led->lock);
}

static void ktd3136_work(struct work_struct *work)
{
	struct ktd3136_data *drvdata = container_of(work,
					struct ktd3136_data, work);

	__ktd3136_work(drvdata, drvdata->led_dev.brightness);

	return;
}


static void ktd3136_brightness_set(struct led_classdev *led_cdev,
			enum led_brightness brt_val)
{
	struct ktd3136_data *drvdata;

	drvdata = container_of(led_cdev, struct ktd3136_data, led_dev);

	schedule_work(&drvdata->work);
}

static void ktd3136_get_dt_data(struct device *dev, struct ktd3136_data *drvdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	u32 bl_channel, temp;
	drvdata->hwen_gpio = of_get_named_gpio(np, "ktd,hwen-gpio", 0);
	pr_debug("hwen --<%d>\n", drvdata->hwen_gpio);

	drvdata->pwm_mode = of_property_read_bool(np,"ktd,pwm-mode");
	pr_debug("pwmmode --<%d> \n", drvdata->pwm_mode);

	drvdata->using_lsb = of_property_read_bool(np, "ktd,using-lsb");
	pr_debug("using_lsb --<%d>\n", drvdata->using_lsb);

	if (drvdata->using_lsb) {
		drvdata->default_brightness = 0x7ff;
		drvdata->max_brightness = 2047;
	} else {
		drvdata->default_brightness = 0xff;
		drvdata->max_brightness = 255;
	}
	rc = of_property_read_u32(np, "ktd,pwm-frequency", &temp);
	if (rc) {
		pr_err("Invalid pwm-frequency!\n");
	} else {
		drvdata->pwm_period = temp;
		pr_debug("pwm-frequency --<%d> \n", drvdata->pwm_period);
	}

	rc = of_property_read_u32(np, "ktd,bl-fscal-led", &temp);
	if (rc) {
		pr_err("Invalid backlight full-scale led current!\n");
	} else {
		drvdata->full_scale_led = temp;
		pr_debug("full-scale led current --<%d mA> \n", drvdata->full_scale_led);
	}

	rc = of_property_read_u32(np, "ktd,turn-on-ramp", &temp);
	if (rc) {
		pr_err("Invalid ramp timing ,,turnon!\n");
	} else {
		drvdata->ramp_on_time = temp;
		pr_debug("ramp on time --<%d ms> \n", drvdata->ramp_on_time);
	}

	rc = of_property_read_u32(np, "ktd,turn-off-ramp", &temp);
	if (rc) {
		pr_err("Invalid ramp timing ,,turnoff!\n");
	} else {
		drvdata->ramp_off_time = temp;
		pr_debug("ramp off time --<%d ms> \n", drvdata->ramp_off_time);
	}

	rc = of_property_read_u32(np, "ktd,pwm-trans-dim", &temp);
	if (rc) {
		pr_err("Invalid pwm-tarns-dim value!\n");
	}
	else {
		drvdata->pwm_trans_dim = temp;
		pr_debug("pwm trnasition dimming	--<%d ms> \n", drvdata->pwm_trans_dim);
	}

	rc = of_property_read_u32(np, "ktd,i2c-trans-dim", &temp);
	if (rc) {
		pr_err("Invalid i2c-trans-dim value !\n");
	} else {
		drvdata->i2c_trans_dim = temp;
		pr_debug("i2c transition dimming --<%d ms>\n", drvdata->i2c_trans_dim);
	}

	rc = of_property_read_u32(np, "ktd,bl-channel", &bl_channel);
	if (rc) {
		pr_err("Invalid channel setup\n");
	} else {
		drvdata->channel = bl_channel;
		pr_debug("bl-channel --<%x> \n", drvdata->channel);
	}

	rc = of_property_read_u32(np, "ktd,ovp-level", &temp);
	if (!rc) {
		drvdata->ovp_level = temp;
		pr_debug("ovp-level --<%d> --temp <%d>\n", drvdata->ovp_level, temp);
	}else
		pr_err("Invalid OVP level!\n");

	rc = of_property_read_u32(np, "ktd,switching-frequency", &temp);
	if (!rc) {
		drvdata->frequency = temp;
		pr_debug("switching frequency --<%d> \n", drvdata->frequency);
	} else {
		pr_err("Invalid Frequency value!\n");
	}

	rc = of_property_read_u32(np, "ktd,inductor-current", &temp);
	if (!rc) {
		drvdata->induct_current = temp;
		pr_debug("inductor current limit --<%d> \n", drvdata->induct_current);
	} else
		pr_err("invalid induct_current limit\n");

	rc = of_property_read_u32(np, "ktd,flash-timeout", &temp);
	if (!rc) {
		drvdata->flash_timeout = temp;
		pr_debug("flash timeout --<%d> \n", drvdata->flash_timeout);
	} else {
		pr_err("invalid flash-time value!\n");
	}

	rc = of_property_read_u32(np, "ktd,flash-current", &temp);
	if (!rc) {
		drvdata->flash_current = temp;
		pr_debug("flash current --<0x%x> \n", drvdata->flash_current);
	} else {
		pr_err("invalid flash current value!\n");
	}
}

static int ktd3136_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct ktd3136_data *drvdata;
	int err = 0;

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

	drvdata = kzalloc(sizeof(struct ktd3136_data), GFP_KERNEL);
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
	drvdata->led_dev.name = KTD3136_LED_DEV;
	drvdata->led_dev.brightness_set = ktd3136_brightness_set;
	drvdata->led_dev.max_brightness = MAX_BRIGHTNESS;
	ktd3136_get_dt_data(&client->dev, drvdata);
	i2c_set_clientdata(client, drvdata);
	err =ktd3136_check_id(drvdata);
	if (err <0) {
		pr_err("%s : ID idenfy failed\n", __func__);
		goto err_init;
	}

	mutex_init(&drvdata->lock);
	INIT_WORK(&drvdata->work, ktd3136_work);
	err = led_classdev_register(&client->dev, &drvdata->led_dev);
	if (err < 0) {
		pr_err("%s : Register led class failed\n", __func__);
		err = -ENODEV;
		goto err_init;
	} else {
	pr_debug("%s: Register led class successful\n", __func__);

	}
	ktd3136_gpio_init(drvdata);
	ktd3136_backlight_init(drvdata);
	ktd3136_backlight_enable(drvdata);
	ktd3136_check_status(drvdata);

#if defined(CONFIG_FB)
	drvdata->fb_notif.notifier_call = fb_notifier_callback;
	err = fb_register_client(&drvdata->fb_notif);
	if (err)
		pr_err("%s : Unable to register fb_notifier: %d\n", __func__, err);
#endif

	return 0;

err_init:
	kfree(drvdata);
err_out:
	return err;
}

static int ktd3136_remove(struct i2c_client *client)
{
	struct ktd3136_data *drvdata = i2c_get_clientdata(client);

	led_classdev_unregister(&drvdata->led_dev);

	kfree(drvdata);
	return 0;
}

static const struct i2c_device_id ktd3136_id[] = {
	{KTD3136_NAME, 0},
	{}
};
static struct of_device_id match_table[] = {
		{.compatible = "ktd,ktd3136",}
};

MODULE_DEVICE_TABLE(i2c, ktd3136_id);

static struct i2c_driver ktd3136_i2c_driver = {
	.probe = ktd3136_probe,
	.remove = ktd3136_remove,
	.id_table = ktd3136_id,
	.driver = {
		.name = KTD3136_NAME,
		.owner = THIS_MODULE,
		.of_match_table = match_table,
	},
};

module_i2c_driver(ktd3136_i2c_driver);
MODULE_DESCRIPTION("Back Light driver for ktd3136");
MODULE_LICENSE("GPL v2");
