/*
 * Copyright (c) 2015, The Linux Foundation. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */
#include <linux/module.h>
#include <linux/kobject.h>
#include <linux/sysfs.h>
#include <linux/firmware.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/irq.h>
#include <linux/interrupt.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/input/mt.h>
#include <linux/cdev.h>
#include <linux/fs.h>
#include <linux/completion.h>
#include <linux/init.h>
#include <asm/uaccess.h>
#include <asm/unaligned.h>
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#else
#include <linux/earlysuspend.h>
#endif
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/leds.h>
#include <linux/kthread.h>

#define VTG_MIN_UV		2850000
#define VTG_MAX_UV		2850000
#define I2C_VTG_MIN_UV	1800000
#define I2C_VTG_MAX_UV	1800000

#define SN_REG_RESET		0x2f
#define SN_REG_POWER		0x00
#define SN_REG_SET_MODE		0x02
#define SN_REG_CURRENT		0x03
#define SN_REG_PWM_VAL		0x04
#define SN_REG_PWM_GREEN		0x05
#define SN_REG_PWM_BLUE		0x06

#define SN_REG_SET_DATA		0x07
#define SN_REG_SELF_T0		0x0a
#define SN_REG_SELF_T1_T2	0x10
#define SN_REG_SELF_T3_T4	0x16
#define SN_REG_TIME_UPDATE	0x1c
#define SN_REG_SET_OUT		0x1D

#define SN_CURRENT_5mA		0x08
#define SN_CURRENT_10mA		0x04
#define SN_CURRENT_17mA		0x10
#define SN_CURRENT_30mA		0x0c
#define SN_CURRENT_42mA		0x00

#define RGB_LED_MIN_MS		50
#define RGB_LED_MAX_MS		10000

struct sn3193_breath_ctrl {
	u8 t0;			/* start */
	u8 t1;			/* rise */
	u8 t2;			/* hold high */
	u8 t3;			/* sink */
	u8 t4;			/* stop */
};

enum led_colors {
	RED,
	GREEN,
	BLUE,
};

enum led_bits {
	SN3193_OFF,
	SN3193_BLINK,
	SN3193_ON,
};

struct sn3193_led_pdata {
	struct i2c_client *client;
	struct led_classdev led_cdev_r;
	struct led_classdev led_cdev_g;
	struct led_classdev led_cdev_b;

	struct mutex led_lock;
	u32 led_r_brightness;
	u32 led_g_brightness;
	u32 led_b_brightness;

	enum led_bits state_ledr;
	enum led_bits state_ledg;
	enum led_bits state_ledb;
	u32 led_current;
	struct sn3193_breath_ctrl ctrl;
	int sdb_gpio;
	u32 sdb_gpio_flags;
	bool i2c_pull_up;

	struct regulator *vcc_i2c;
};

static int sn3193_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret = i2c_smbus_write_byte_data(client, reg, val);

	if (ret >= 0)
		return 0;

	dev_err(&client->dev, "%s: reg 0x%x, val 0x%x, err %d\n",
		__func__, reg, val, ret);

	return ret;
}

static void sn3193_led_reset_off(struct sn3193_led_pdata *data)
{
	struct sn3193_led_pdata *pdata = data;

	pr_debug("%s\n", __func__);

	mutex_lock(&pdata->led_lock);
	gpio_set_value(pdata->sdb_gpio, 1);
	sn3193_write_reg(pdata->client, SN_REG_RESET, 0xff);	/* reset */
	sn3193_write_reg(pdata->client, SN_REG_POWER, 0x00);	/* power off */
	gpio_set_value(pdata->sdb_gpio, 0);
	pdata->state_ledr = SN3193_OFF;
	pdata->state_ledg = SN3193_OFF;
	pdata->state_ledb = SN3193_OFF;
	mutex_unlock(&pdata->led_lock);
}

static void sn3193_led_pwm_ctrl(struct sn3193_led_pdata *data,
				bool power, enum led_colors color,
				u32 brightness)
{
	struct sn3193_led_pdata *pdata = data;
	bool on_off = power;

	pr_debug("%s, on=%d, led_current=%d, led_brightness=%d color %d\n",
		 __func__, on_off, pdata->led_current, brightness, color);

	if (on_off) {
		mutex_lock(&pdata->led_lock);

		gpio_set_value(pdata->sdb_gpio, 1);
		/* PWM mode */
		sn3193_write_reg(pdata->client, SN_REG_SET_MODE, 0x00);
		/* power on */
		sn3193_write_reg(pdata->client, SN_REG_POWER, 0x20);
		/* set current */
		sn3193_write_reg(pdata->client, SN_REG_CURRENT,
				 pdata->led_current);
		sn3193_write_reg(pdata->client, SN_REG_PWM_VAL + color,
				 brightness);
		/* update register */
		sn3193_write_reg(pdata->client, SN_REG_SET_DATA, 0xff);
		mutex_unlock(&pdata->led_lock);
	} else {
		if ((SN3193_OFF == pdata->state_ledr)
		    && (SN3193_OFF == pdata->state_ledg)
		    && (SN3193_OFF == pdata->state_ledb)) {
			sn3193_led_reset_off(pdata);
		} else {
			mutex_lock(&pdata->led_lock);
			gpio_set_value(pdata->sdb_gpio, 1);
			sn3193_write_reg(pdata->client, SN_REG_SET_MODE, 0x00);
			sn3193_write_reg(pdata->client, SN_REG_POWER, 0x20);
			sn3193_write_reg(pdata->client, SN_REG_CURRENT, 0x00);
			sn3193_write_reg(pdata->client, SN_REG_PWM_VAL + color,
					 0x00);
			sn3193_write_reg(pdata->client, SN_REG_SET_DATA, 0xff);
			mutex_unlock(&pdata->led_lock);
		}
	}

}

static void sn3193_led_self_breath_ctrl(struct sn3193_led_pdata *data,
					bool power, enum led_colors color)
{
	struct sn3193_led_pdata *pdata = data;
	bool on_off = power;

	pr_debug("%s, on=%d, t0 %d t1 %d t2 %d t3 %d t4 %d\n",
		__func__, on_off, pdata->ctrl.t0, pdata->ctrl.t1,
		pdata->ctrl.t2, pdata->ctrl.t3, pdata->ctrl.t4);
	if (on_off) {
		gpio_set_value(pdata->sdb_gpio, 1);

		mutex_lock(&pdata->led_lock);
		sn3193_write_reg(pdata->client, SN_REG_SET_MODE, 0x20);
		sn3193_write_reg(pdata->client, SN_REG_POWER, 0x20);
		sn3193_write_reg(pdata->client, SN_REG_CURRENT,
						pdata->led_current);

		sn3193_write_reg(pdata->client, SN_REG_SELF_T0 + color,
							pdata->ctrl.t0);
		sn3193_write_reg(pdata->client, SN_REG_SELF_T1_T2 + color,
					pdata->ctrl.t1 | pdata->ctrl.t2);
		sn3193_write_reg(pdata->client, SN_REG_SELF_T3_T4 + color,
					pdata->ctrl.t3 | pdata->ctrl.t4);
		sn3193_write_reg(pdata->client, SN_REG_TIME_UPDATE, 0x00);
		sn3193_write_reg(pdata->client, SN_REG_PWM_VAL + color, 0xff);
		sn3193_write_reg(pdata->client, SN_REG_SET_DATA, 0xff);
		mutex_unlock(&pdata->led_lock);
	} else {
		if ((SN3193_OFF == pdata->state_ledr)
		    && (SN3193_OFF == pdata->state_ledg)
		    && (SN3193_OFF == pdata->state_ledb)) {
			sn3193_led_reset_off(pdata);
		} else {
			mutex_lock(&pdata->led_lock);
			sn3193_write_reg(pdata->client,
						SN_REG_PWM_VAL + color, 0x00);
			sn3193_write_reg(pdata->client,
						SN_REG_SET_DATA, 0xff);
			mutex_unlock(&pdata->led_lock);
		}
	}

}

static void sn3193_led_set_r_brightness(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	struct sn3193_led_pdata *pdata =
	    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_r);

	pr_debug("%s\n", __func__);
	pdata->led_r_brightness = led_cdev->brightness = value;
	if (pdata->led_r_brightness) {
		if (SN3193_BLINK == pdata->state_ledr
		    || SN3193_BLINK == pdata->state_ledg
		    || SN3193_BLINK == pdata->state_ledb) {
			sn3193_led_reset_off(pdata);
		}
		pdata->state_ledr = SN3193_ON;
		sn3193_led_pwm_ctrl(pdata, true, RED, pdata->led_r_brightness);
	} else {
		pdata->state_ledr = SN3193_OFF;
		sn3193_led_pwm_ctrl(pdata, false, RED, pdata->led_r_brightness);
	}
}

static enum led_brightness sn3193_led_get_r_brightness(struct led_classdev
						       *led_cdev)
{
	struct sn3193_led_pdata *pdata =
	    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_r);

	pr_debug("%s, brightness=%d\n", __func__, pdata->led_r_brightness);

	return pdata->led_r_brightness;
}

static void sn3193_led_set_g_brightness(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	struct sn3193_led_pdata *pdata =
	    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_g);

	pr_debug("%s\n", __func__);
	pdata->led_g_brightness = led_cdev->brightness = value;
	if (pdata->led_g_brightness) {
		if (SN3193_BLINK == pdata->state_ledr
		    || SN3193_BLINK == pdata->state_ledg
		    || SN3193_BLINK == pdata->state_ledb) {
			sn3193_led_reset_off(pdata);
		}
		pdata->state_ledg = SN3193_ON;
		sn3193_led_pwm_ctrl(pdata, true, GREEN,
				    pdata->led_g_brightness);
	} else {
		pdata->state_ledg = SN3193_OFF;
		sn3193_led_pwm_ctrl(pdata, false, GREEN,
				    pdata->led_g_brightness);
	}
}

static enum led_brightness sn3193_led_get_g_brightness(struct led_classdev
						       *led_cdev)
{
	struct sn3193_led_pdata *pdata =
	    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_g);

	pr_debug("%s, brightness=%d\n", __func__, pdata->led_g_brightness);

	return pdata->led_g_brightness;
}

static void sn3193_led_set_b_brightness(struct led_classdev *led_cdev,
					enum led_brightness value)
{
	struct sn3193_led_pdata *pdata =
	    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_b);

	pr_debug("%s\n", __func__);
	pdata->led_b_brightness = led_cdev->brightness = value;

	if (pdata->led_b_brightness) {
		if (SN3193_BLINK == pdata->state_ledr
		    || SN3193_BLINK == pdata->state_ledg
		    || SN3193_BLINK == pdata->state_ledb) {
			sn3193_led_reset_off(pdata);
		}
		pdata->state_ledb = SN3193_ON;
		sn3193_led_pwm_ctrl(pdata, true, BLUE, pdata->led_b_brightness);
	} else {
		pdata->state_ledb = SN3193_OFF;
		sn3193_led_pwm_ctrl(pdata, false, BLUE,
				    pdata->led_b_brightness);
	}
}

static enum led_brightness sn3193_led_get_b_brightness(struct led_classdev
						       *led_cdev)
{
	struct sn3193_led_pdata *pdata =
	    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_b);

	pr_debug("%s, brightness=%d\n", __func__, pdata->led_b_brightness);

	return pdata->led_b_brightness;
}

static int sn3193_led_power_on(struct i2c_client *client, bool on)
{
	struct sn3193_led_pdata *pdata = i2c_get_clientdata(client);
	int rc = 0;

	pr_debug("%s\n", __func__);

	if (!on)
		goto power_off;

	if (pdata->i2c_pull_up) {
		rc = regulator_enable(pdata->vcc_i2c);
		if (rc) {
			dev_err(&client->dev,
				"Regulator vcc_i2c enable failed rc=%d\n", rc);
		}
	}
	return rc;

power_off:

	rc = regulator_disable(pdata->vcc_i2c);
	if (rc) {
		dev_err(&client->dev,
			"Regulator vcc_i2c disable failed rc=%d\n", rc);
	}

	return rc;
}

static int sn3193_led_power_init(struct i2c_client *client, bool on)
{
	struct sn3193_led_pdata *pdata = i2c_get_clientdata(client);
	int rc = 0;

	pr_debug("%s\n", __func__);
	if (!on)
		goto pwr_deinit;

	if (pdata->i2c_pull_up) {
		pdata->vcc_i2c = regulator_get(&client->dev, "vcc_i2c");
		if (IS_ERR(pdata->vcc_i2c)) {
			rc = PTR_ERR(pdata->vcc_i2c);
			dev_err(&client->dev,
				"Regulator get failed vcc_i2c rc=%d\n", rc);
			goto reg_vdd_set_vtg;
		}

		if (regulator_count_voltages(pdata->vcc_i2c) > 0) {
			rc = regulator_set_voltage(pdata->vcc_i2c,
						   I2C_VTG_MIN_UV,
						   I2C_VTG_MAX_UV);
			if (rc) {
				dev_err(&client->dev,
					"Regulator set_vtg failed vcc_i2c rc=%d\n",
					rc);
				goto reg_vcc_i2c_put;
			}
		}
	}
	return 0;

reg_vcc_i2c_put:
	if (pdata->i2c_pull_up)
		regulator_put(pdata->vcc_i2c);
reg_vdd_set_vtg:
	return rc;
pwr_deinit:

	if (pdata->i2c_pull_up) {
		if (regulator_count_voltages(pdata->vcc_i2c) > 0)
			regulator_set_voltage(pdata->vcc_i2c, 0,
					      I2C_VTG_MAX_UV);
		regulator_put(pdata->vcc_i2c);
	}
	return 0;
}

static int sn3193_led_set_gpio(struct sn3193_led_pdata *data)
{
	struct sn3193_led_pdata *pdata = data;
	int ret = 0;

	pr_debug("%s\n", __func__);
	if (gpio_is_valid(pdata->sdb_gpio)) {
		ret =
		    gpio_request_one(pdata->sdb_gpio,
				     GPIOF_DIR_OUT | GPIOF_INIT_LOW,
				     "sn3193_led_sdb");
		if (ret != 0) {
			pr_err("Failed to request sn3193_led_sdb: %d\n", ret);
			return -EINVAL;
		}
		gpio_set_value(pdata->sdb_gpio, 0);
	}

	return 0;
}

static int sn3193_led_parse_dt(struct device *dev,
			       struct sn3193_led_pdata *data)
{
	struct device_node *np = dev->of_node;
	struct sn3193_led_pdata *pdata = data;

	pr_debug("%s\n", __func__);

	/* gpio info */
	pdata->sdb_gpio =
	    of_get_named_gpio_flags(np, "si-en,sdb-gpio", 0,
				    &pdata->sdb_gpio_flags);
	if (pdata->sdb_gpio < 0)
		pr_err("%s, sdb_gpio not exist!\n", __func__);
	else
		pr_debug("%s, sdb_gpio=%d\n", __func__, pdata->sdb_gpio);

	pdata->i2c_pull_up = of_property_read_bool(np, "si-en,i2c-pull-up");

	return 0;
}

static void sn3193_set_led_blink(struct sn3193_led_pdata *led,
				 enum led_colors color)
{
	pr_debug("%s  color %d\n", __func__, color);
	if (led->ctrl.t2 > 0)
		sn3193_led_self_breath_ctrl(led, true, color);
	else
		sn3193_led_self_breath_ctrl(led, false, color);
}

static ssize_t sn3193_blink_store(struct device *dev,
				  struct device_attribute *attr,
				  const char *buf, size_t count)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct sn3193_led_pdata *led;
	unsigned long blinking = 0;
	unsigned int hold_time = 0;
	ssize_t ret = -EINVAL;

	pr_debug("%s\n", __func__);
	ret = kstrtoul(buf, 10, &blinking);
	if (ret)
		return ret;

	if (blinking > 0)
		hold_time = blinking + 1;
	else
		hold_time = 0;
	pr_debug("%s led_cdev->name %s blinking %ld ret %d\n", __func__,
			led_cdev->name, blinking, ret);

	pr_debug("%s  hold_time %d\n", __func__, hold_time);
	if (!strcmp(led_cdev->name, "red")) {
		led =
		    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_r);
		led->ctrl.t1 = hold_time << 5;
		led->ctrl.t2 = hold_time << 1;
		led->ctrl.t3 = hold_time << 5;

		if (SN3193_ON == led->state_ledr || SN3193_ON == led->state_ledg
		    || SN3193_ON == led->state_ledb) {
			sn3193_led_reset_off(led);
		}
		led->state_ledr = SN3193_BLINK;
		sn3193_set_led_blink(led, RED);
	} else if (!strcmp(led_cdev->name, "green")) {
		led =
		    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_g);
		led->ctrl.t1 = hold_time << 5;
		led->ctrl.t2 = hold_time << 1;
		led->ctrl.t3 = hold_time << 5;
		if (SN3193_ON == led->state_ledr || SN3193_ON == led->state_ledg
		    || SN3193_ON == led->state_ledb) {
			sn3193_led_reset_off(led);
		}
		led->state_ledg = SN3193_BLINK;
		sn3193_set_led_blink(led, GREEN);
	} else if (!strcmp(led_cdev->name, "blue")) {
		led =
		    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_b);
		led->ctrl.t1 = hold_time << 5;
		led->ctrl.t2 = hold_time << 1;
		led->ctrl.t3 = hold_time << 5;
		if (SN3193_ON == led->state_ledr || SN3193_ON == led->state_ledg
		    || SN3193_ON == led->state_ledb) {
			sn3193_led_reset_off(led);
		}
		led->state_ledb = SN3193_BLINK;
		sn3193_set_led_blink(led, BLUE);
	} else {
		pr_err("%s invalid led color!\n", __func__);
		return -EINVAL;
	}

	return count;
}

static ssize_t rgb_on_off_ms_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct sn3193_led_pdata *led;
	int t2_setting_to_ms[8] = {0, 130, 260, 520, 1040, 2080, 4160, 8320};
	int t4_setting_to_ms[11] = {0, 130, 260, 520, 1040, 2080, 4160, 8320, 16640,
				    32280, 66560};
	unsigned int on_time;
	unsigned int off_time;

	if (!strcmp(led_cdev->name, "red")) {
		led =
		    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_r);
	} else if (!strcmp(led_cdev->name, "green")) {
		led =
		    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_g);
	} else if (!strcmp(led_cdev->name, "blue")) {
		led =
		    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_b);
	} else {
		pr_err("%s invalid led color!\n", __func__);
		return -EINVAL;
	}

	on_time = led->ctrl.t2 >> 1;
	off_time = led->ctrl.t4 >> 1;

	if (on_time > sizeof(t2_setting_to_ms)) {
		on_time = sizeof(t2_setting_to_ms)-1;
	}
	if (off_time > sizeof(t4_setting_to_ms)) {
		off_time = sizeof(t4_setting_to_ms)-1;
	}
	return sprintf(buf, "%d %d\n",
			t2_setting_to_ms[on_time], t4_setting_to_ms[off_time]);
}

static ssize_t rgb_on_off_ms_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct sn3193_led_pdata *led;
	int on_ms;
	int off_ms;
	int ret;
	unsigned int on_time;
	unsigned int off_time;
	int t2_ms_to_setting[7] = {195, 390, 780, 1560, 3120, 6240, 8320};
	int t4_ms_to_setting[10] = {195, 390, 780, 1560, 3120, 6240, 12480, 24960,
				    48420, 66560};

	ret = sscanf(buf, "%d %d", &on_ms, &off_ms);
	if (ret <= 1)
		return -EINVAL;

	if (on_ms < RGB_LED_MIN_MS)
		on_ms = RGB_LED_MIN_MS;

	if (on_ms > RGB_LED_MAX_MS)
		on_ms = RGB_LED_MAX_MS;

	if (off_ms > RGB_LED_MAX_MS)
		off_ms = RGB_LED_MAX_MS;

	if (on_ms == 0) {
		on_time = 0;
	} else {
		for (on_time = 1;on_time<sizeof(t2_ms_to_setting);on_time++) {
			if (on_ms <= t2_ms_to_setting[on_time-1]) {
				break;
			}
		}
	}

	if (off_ms == 0) {
		off_time = 0;
	} else {
		for (off_time = 1;off_time<sizeof(t4_ms_to_setting);off_time++) {
			if (off_ms <= t4_ms_to_setting[off_time-1]) {
				break;
			}
		}
	}

	if (!strcmp(led_cdev->name, "red")) {
		led =
		    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_r);
		led->ctrl.t0 = 0;
		led->ctrl.t1 = 1 << 5;
		led->ctrl.t2 = on_time << 1;
		led->ctrl.t3 = 1 << 5;
		led->ctrl.t4 = off_time << 1;
		if (SN3193_ON == led->state_ledr || SN3193_ON == led->state_ledg
		    || SN3193_ON == led->state_ledb) {
			sn3193_led_reset_off(led);
		}
		led->state_ledr = SN3193_BLINK;
		sn3193_set_led_blink(led, RED);
	} else if (!strcmp(led_cdev->name, "green")) {
		led =
		    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_g);
		led->ctrl.t0 = 0;
		led->ctrl.t1 = 1 << 5;
		led->ctrl.t2 = on_time << 1;
		led->ctrl.t3 = 1 << 5;
		led->ctrl.t4 = off_time << 1;
	if (SN3193_ON == led->state_ledr || SN3193_ON == led->state_ledg
		    || SN3193_ON == led->state_ledb) {
			sn3193_led_reset_off(led);
		}
		led->state_ledg = SN3193_BLINK;
		sn3193_set_led_blink(led, GREEN);
	} else if (!strcmp(led_cdev->name, "blue")) {
		led =
		    container_of(led_cdev, struct sn3193_led_pdata, led_cdev_b);
		led->ctrl.t0 = 0;
		led->ctrl.t1 = 1 << 5;
		led->ctrl.t2 = on_time << 1;
		led->ctrl.t3 = 1 << 5;
		led->ctrl.t4 = off_time << 1;
		if (SN3193_ON == led->state_ledr || SN3193_ON == led->state_ledg
		    || SN3193_ON == led->state_ledb) {
			sn3193_led_reset_off(led);
		}
		led->state_ledb = SN3193_BLINK;
		sn3193_set_led_blink(led, BLUE);
	} else {
		pr_err("%s invalid led color!\n", __func__);
		return -EINVAL;
	}

	return size;
}

static DEVICE_ATTR(blink, 0664, NULL, sn3193_blink_store);
static DEVICE_ATTR(on_off_ms, 0660, rgb_on_off_ms_show, rgb_on_off_ms_store);

static struct attribute *blink_attrs[] = {
	&dev_attr_blink.attr,
	&dev_attr_on_off_ms.attr,
	NULL
};

static const struct attribute_group blink_attr_group = {
	.attrs = blink_attrs,
};

static int sn3193_led_probe(struct i2c_client *client,
			    const struct i2c_device_id *id)
{
	struct i2c_adapter *adapter = to_i2c_adapter(client->dev.parent);
	struct sn3193_led_pdata *pdata;
	int ret = 0;

	if (!i2c_check_functionality(adapter, I2C_FUNC_I2C))
		return -EIO;

	if (client->dev.of_node) {
		pdata =
		    devm_kzalloc(&client->dev, sizeof(struct sn3193_led_pdata),
				 GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev,
				"Failed to allocated sn3193_led_pdata\n");
			return -ENOMEM;
		}
		ret = sn3193_led_parse_dt(&client->dev, pdata);
		if (ret) {
			dev_err(&client->dev, "DT parsing failed\n");
			goto Err_parse_dt;
		}
	}

	pdata->client = client;
	i2c_set_clientdata(client, pdata);
	dev_set_drvdata(&client->dev, pdata);

	mutex_init(&pdata->led_lock);

	sn3193_led_set_gpio(pdata);
	msleep(10);

	ret = sn3193_led_power_init(client, true);
	if (ret) {
		dev_err(&client->dev, "power init failed");
		goto Err_power_init;
	}

	ret = sn3193_led_power_on(client, true);
	if (ret) {
		dev_err(&client->dev, "power on failed");
		goto Err_power_on;
	}

	sn3193_led_reset_off(pdata);

	/* Power up IC */
	pdata->led_current = SN_CURRENT_17mA;
	pdata->led_r_brightness = 0x00;
	pdata->led_g_brightness = 0x00;
	pdata->led_b_brightness = 0x00;
	pdata->ctrl.t0 = 0x00;
	pdata->ctrl.t1 = 0x00;
	pdata->ctrl.t2 = 0x00;
	pdata->ctrl.t3 = 0x00;
	pdata->ctrl.t4 = 0x00;
	pdata->state_ledr = SN3193_OFF;
	pdata->state_ledg = SN3193_OFF;
	pdata->state_ledb = SN3193_OFF;

	pdata->led_cdev_r.name = "red";
	pdata->led_cdev_r.brightness = LED_OFF;
	pdata->led_cdev_r.max_brightness = LED_FULL;
	pdata->led_cdev_r.brightness_set = sn3193_led_set_r_brightness;
	pdata->led_cdev_r.brightness_get = sn3193_led_get_r_brightness;

	ret = led_classdev_register(&pdata->client->dev, &pdata->led_cdev_r);
	if (ret < 0) {
		dev_err(&client->dev, "couldn't register LED %s\n",
			pdata->led_cdev_r.name);
		goto Err_led_classdev_r;
	}
	ret =
	    sysfs_create_group(&pdata->led_cdev_r.dev->kobj, &blink_attr_group);
	if (ret)
		goto Err_led_r_create_group;

	pdata->led_cdev_g.name = "green";
	pdata->led_cdev_g.brightness = LED_OFF;
	pdata->led_cdev_g.max_brightness = LED_FULL;
	pdata->led_cdev_g.brightness_set = sn3193_led_set_g_brightness;
	pdata->led_cdev_g.brightness_get = sn3193_led_get_g_brightness;

	ret = led_classdev_register(&pdata->client->dev, &pdata->led_cdev_g);
	if (ret < 0) {
		dev_err(&client->dev, "couldn't register LED %s\n",
			pdata->led_cdev_g.name);
		goto Err_led_classdev_g;
	}
	ret =
	    sysfs_create_group(&pdata->led_cdev_g.dev->kobj, &blink_attr_group);
	if (ret)
		goto Err_led_g_create_group;

	pdata->led_cdev_b.name = "blue";
	pdata->led_cdev_b.brightness = LED_OFF;
	pdata->led_cdev_b.max_brightness = LED_FULL;
	pdata->led_cdev_b.brightness_set = sn3193_led_set_b_brightness;
	pdata->led_cdev_b.brightness_get = sn3193_led_get_b_brightness;

	ret = led_classdev_register(&pdata->client->dev, &pdata->led_cdev_b);
	if (ret < 0) {
		dev_err(&client->dev, "couldn't register LED %s\n",
			pdata->led_cdev_b.name);
		goto Err_led_classdev_b;
	}
	ret =
	    sysfs_create_group(&pdata->led_cdev_b.dev->kobj, &blink_attr_group);
	if (ret)
		goto Err_led_b_create_group;

	pr_debug("%s done\n", __func__);
	return 0;
Err_led_b_create_group:
	led_classdev_unregister(&pdata->led_cdev_b);
Err_led_classdev_b:
Err_led_g_create_group:
	led_classdev_unregister(&pdata->led_cdev_g);
Err_led_classdev_g:
Err_led_r_create_group:
	led_classdev_unregister(&pdata->led_cdev_r);
Err_led_classdev_r:
	sn3193_led_power_on(client, false);
Err_power_on:
	sn3193_led_power_init(client, false);
Err_power_init:
Err_parse_dt:
	devm_kfree(&client->dev, pdata);
	dev_err(&client->dev, "%s err, ret=%d\n", __func__, ret);
	return ret;
}

static int sn3193_led_remove(struct i2c_client *client)
{
	struct sn3193_led_pdata *pdata = i2c_get_clientdata(client);

	led_classdev_unregister(&pdata->led_cdev_r);
	led_classdev_unregister(&pdata->led_cdev_g);
	led_classdev_unregister(&pdata->led_cdev_b);

	sn3193_led_power_on(client, false);
	sn3193_led_power_init(client, false);

	mutex_destroy(&pdata->led_lock);
	i2c_set_clientdata(client, NULL);
	devm_kfree(&client->dev, pdata);

	return 0;
}

static const struct i2c_device_id sn3193_led_id[] = {
	{"sn3193_led", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, sn3193_led_id);

#ifdef CONFIG_OF
static struct of_device_id sn3193_led_match_table[] = {
	{.compatible = "si-en,sn3193",},
	{},
};
#else
#define mms_match_table NULL
#endif

static struct i2c_driver sn3193_led_driver = {
	.probe = sn3193_led_probe,
	.remove = sn3193_led_remove,
	.driver = {
		   .name = "sn3193_led",
		   .owner = THIS_MODULE,
		   .of_match_table = sn3193_led_match_table,
		   },
	.id_table = sn3193_led_id,
};

static int __init sn3193_led_init(void)
{
	pr_debug("%s\n", __func__);
	return i2c_add_driver(&sn3193_led_driver);
}

static void __exit sn3193_led_exit(void)
{
	return i2c_del_driver(&sn3193_led_driver);
}

module_init(sn3193_led_init);
module_exit(sn3193_led_exit);

MODULE_AUTHOR("Kun pang <pangkun@longcheer.net>");
MODULE_VERSION("2015.12.14");
MODULE_DESCRIPTION("SI-EN Breath LED Driver, designed for L9300.");
MODULE_LICENSE("GPL");
