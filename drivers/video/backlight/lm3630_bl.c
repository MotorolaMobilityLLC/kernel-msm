/*
 * Copyright (C) 2013 LGE, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/module.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <mach/board.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <mach/board_lge.h>

#define I2C_BL_NAME                              "lm3630"
#define MAX_BRIGHTNESS_LM3630                    0xFF
#define MIN_BRIGHTNESS_LM3630                    0x0F
#define DEFAULT_BRIGHTNESS                       0xFF
#define DEFAULT_FTM_BRIGHTNESS                   0x0F

#define BL_ON        1
#define BL_OFF       0

static int exp_min_value = 150;
static int cal_value;
static struct i2c_client *lm3630_i2c_client;
static int store_level_used = 0;

struct backlight_platform_data {
	void (*platform_init)(void);
	int gpio;
	unsigned int mode;
	int max_current;
	int init_on_boot;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int factory_brightness;
	int blmap_size;
	char *blmap;
};

struct lm3630_device {
	struct i2c_client *client;
	struct backlight_device *bl_dev;
	int gpio;
	int max_current;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int factory_brightness;
	struct mutex bl_mutex;
	int blmap_size;
	char *blmap;
};

static const struct i2c_device_id lm3630_bl_id[] = {
	{ I2C_BL_NAME, 0 },
	{ },
};

static int lm3630_read_reg(struct i2c_client *client, u8 reg, u8 *buf);
static int lm3630_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val);
static int cur_main_lcd_level = DEFAULT_BRIGHTNESS;
static int saved_main_lcd_level = DEFAULT_BRIGHTNESS;
static int backlight_status = BL_OFF;
static int lm3630_pwm_enable;
static struct lm3630_device *main_lm3630_dev;

static void lm3630_hw_reset(void)
{
	int gpio = main_lm3630_dev->gpio;

	/* Fix GPIO Setting Warning */
	if (gpio_is_valid(gpio)) {
		gpio_direction_output(gpio, 1);
		gpio_set_value_cansleep(gpio, 1);
		mdelay(10);
	} else {
		pr_err("%s: gpio is not valid !!\n", __func__);
	}
}

static int lm3630_read_reg(struct i2c_client *client, u8 reg, u8 *buf)
{
	int ret;

	pr_debug("%s: reg=0x%x\n", __func__, reg);
	ret = i2c_smbus_read_byte_data(client, reg);

	if (ret < 0)
		pr_err("%s: i2c error\n", __func__);
	else
		*buf = ret;

	return 0;

}

static int lm3630_write_reg(struct i2c_client *client, unsigned char reg, unsigned char val)
{
	int err;
	u8 buf[2];
	struct i2c_msg msg = {
		client->addr, 0, 2, buf
	};

	buf[0] = reg;
	buf[1] = val;

	err = i2c_transfer(client->adapter, &msg, 1);
	if (err < 0)
		pr_err("%s: i2c write error\n", __func__);

	return 0;
}

static void lm3630_set_main_current_level(struct i2c_client *client, int level)
{
	struct lm3630_device *dev = i2c_get_clientdata(client);
	int min_brightness = dev->min_brightness;
	int max_brightness = dev->max_brightness;

	if (level == -1)
		level = dev->default_brightness;
	cur_main_lcd_level = level;
	dev->bl_dev->props.brightness = cur_main_lcd_level;
	store_level_used = 0;
	mutex_lock(&dev->bl_mutex);
	if (level != 0) {
		if (level > 0 && level <= min_brightness)
			level = min_brightness;
		else if (level > max_brightness)
			level = max_brightness;

		if (dev->blmap) {
			if (level < dev->blmap_size) {
				cal_value = dev->blmap[level];
				lm3630_write_reg(client, 0x03, cal_value);
			} else {
				pr_warn("%s: invalid index %d:%d\n", __func__,
						dev->blmap_size, level);
			}
		} else {
			cal_value = level;
			lm3630_write_reg(client, 0x03, cal_value);
		}
	} else {
		lm3630_write_reg(client, 0x00, 0x00);
	}
	mutex_unlock(&dev->bl_mutex);
	pr_debug("%s: level=%d, cal_value=%d\n", __func__, level, cal_value);
}

static void lm3630_set_main_current_level_no_mapping(struct i2c_client *client, int level)
{
	struct lm3630_device *dev = i2c_get_clientdata(client);

	if (level > 255)
		level = 255;
	else if (level < 0)
		level = 0;

	cur_main_lcd_level = level;
	dev->bl_dev->props.brightness = cur_main_lcd_level;
	store_level_used = 1;
	mutex_lock(&main_lm3630_dev->bl_mutex);
	if (level != 0)
		lm3630_write_reg(client, 0x03, level);
	else
		lm3630_write_reg(client, 0x00, 0x00);
	mutex_unlock(&main_lm3630_dev->bl_mutex);
}

void lm3630_backlight_on(int level)
{
	if (backlight_status == BL_OFF) {
		lm3630_hw_reset();
		/*  OVP(24V),OCP(1.0A) , Boost Frequency(500khz) */
		lm3630_write_reg(main_lm3630_dev->client, 0x02, 0x30);
		if (lm3630_pwm_enable)
			/* eble Feedback , disable  PWM for BANK A,B */
			lm3630_write_reg(main_lm3630_dev->client, 0x01, 0x09);
		else
			/* eble Feedback , disable  PWM for BANK A,B */
			lm3630_write_reg(main_lm3630_dev->client, 0x01, 0x08);
		/* Full-Scale Current (23.4mA) of BANK A */
		lm3630_write_reg(main_lm3630_dev->client, 0x05, 0x17);
		/* Enable LED A to Exponential, LED2 is connected to BANK_A */
		lm3630_write_reg(main_lm3630_dev->client, 0x00, 0x15);
	}
	mdelay(1);
	lm3630_set_main_current_level(main_lm3630_dev->client, level);
	backlight_status = BL_ON;
	return;
}

void lm3630_backlight_off(void)
{
	if (backlight_status == BL_OFF)
		return;

	saved_main_lcd_level = cur_main_lcd_level;
	lm3630_set_main_current_level(main_lm3630_dev->client, 0);
	backlight_status = BL_OFF;
	gpio_direction_output(main_lm3630_dev->gpio, 0);
	msleep(6);
	return;
}

void lm3630_lcd_backlight_set_level(int level)
{
	if (!lm3630_i2c_client) {
		pr_warn("%s: No device\n", __func__);
		return;
	}

	if (level > MAX_BRIGHTNESS_LM3630) /* TODO:  dev->max_brightness */
		level = MAX_BRIGHTNESS_LM3630;

	pr_debug("%s: level=%d\n", __func__, level);

	if (level)
		lm3630_backlight_on(level);
	else
		lm3630_backlight_off();
}
EXPORT_SYMBOL(lm3630_lcd_backlight_set_level);

static int bl_set_intensity(struct backlight_device *bd)
{
	struct i2c_client *client = to_i2c_client(bd->dev.parent);

	/* If it's trying to set same backlight value, skip it. */
	if (bd->props.brightness == cur_main_lcd_level) {
		pr_debug("%s: level set already. Skip it.\n", __func__);
		return 0;
	}
	lm3630_set_main_current_level(client, bd->props.brightness);
	cur_main_lcd_level = bd->props.brightness;
	return 0;
}

static int bl_get_intensity(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static ssize_t lcd_backlight_show_level(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r = 0;

	if (store_level_used == 0)
		r = snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n",
				cal_value);
	else if (store_level_used == 1)
		r = snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n",
				cur_main_lcd_level);
	return r;
}

static ssize_t lcd_backlight_store_level(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int level;
	struct i2c_client *client = to_i2c_client(dev);

	if (!count)
		return -EINVAL;
	level = simple_strtoul(buf, NULL, 10);
	lm3630_set_main_current_level_no_mapping(client, level);
	pr_info("%s: level=%d\n", __func__, level);
	return count;
}

static int lm3630_bl_resume(struct i2c_client *client)
{
	lm3630_lcd_backlight_set_level(saved_main_lcd_level);
	return 0;
}

static int lm3630_bl_suspend(struct i2c_client *client, pm_message_t state)
{
	pr_info("%s: state.event=%d\n", __func__, state.event);
	lm3630_lcd_backlight_set_level(saved_main_lcd_level);
	return 0;
}

static ssize_t lcd_backlight_show_on_off(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r = 0;

	pr_info("%s: received (prev backlight_status: %s)\n",
		__func__, backlight_status ? "ON" : "OFF");
	return r;
}

static ssize_t lcd_backlight_store_on_off(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int on_off;
	struct i2c_client *client = to_i2c_client(dev);

	if (!count)
		return -EINVAL;
	pr_info("%s: received (prev backlight_status: %s)\n",
		__func__, backlight_status ? "ON" : "OFF");
	on_off = simple_strtoul(buf, NULL, 10);
	pr_info("%s: on_off=%d", __func__, on_off);
	if (on_off == 1)
		lm3630_bl_resume(client);
	else if (on_off == 0)
		lm3630_bl_suspend(client, PMSG_SUSPEND);
	return count;

}
static ssize_t lcd_backlight_show_exp_min_value(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;

	r = snprintf(buf, PAGE_SIZE, "LCD Backlight  : %d\n", exp_min_value);
	return r;
}

static ssize_t lcd_backlight_store_exp_min_value(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int value;

	if (!count)
		return -EINVAL;
	value = simple_strtoul(buf, NULL, 10);
	exp_min_value = value;
	return count;
}

static ssize_t lcd_backlight_show_pwm(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int r;
	u8 level, pwm_low, pwm_high, config;

	mutex_lock(&main_lm3630_dev->bl_mutex);
	lm3630_read_reg(main_lm3630_dev->client, 0x01, &config);
	mdelay(3);
	lm3630_read_reg(main_lm3630_dev->client, 0x03, &level);
	mdelay(3);
	lm3630_read_reg(main_lm3630_dev->client, 0x12, &pwm_low);
	mdelay(3);
	lm3630_read_reg(main_lm3630_dev->client, 0x13, &pwm_high);
	mdelay(3);
	mutex_unlock(&main_lm3630_dev->bl_mutex);
	r = snprintf(buf, PAGE_SIZE, "Show PWM level: %d pwm_low: %d "
			"pwm_high: %d config: %d\n", level, pwm_low,
			pwm_high, config);
	return r;
}

static ssize_t lcd_backlight_store_pwm(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	/* TODO: need? */
	return count;
}

DEVICE_ATTR(lm3630_level, 0644, lcd_backlight_show_level,
		lcd_backlight_store_level);
DEVICE_ATTR(lm3630_backlight_on_off, 0644, lcd_backlight_show_on_off,
		lcd_backlight_store_on_off);
DEVICE_ATTR(lm3630_exp_min_value, 0644, lcd_backlight_show_exp_min_value,
		lcd_backlight_store_exp_min_value);
DEVICE_ATTR(lm3630_pwm, 0644, lcd_backlight_show_pwm, lcd_backlight_store_pwm);

#ifdef CONFIG_OF
static int lm3630_parse_dt(struct device *dev,
		struct backlight_platform_data *pdata)
{
	int rc = 0;
	int i;
	u32 *array;
	struct device_node *np = dev->of_node;

	pdata->gpio = of_get_named_gpio_flags(np,
			"lm3630,lcd_bl_en", 0, NULL);
	rc = of_property_read_u32(np, "lm3630,max_current",
			&pdata->max_current);
	rc = of_property_read_u32(np, "lm3630,min_brightness",
			&pdata->min_brightness);
	rc = of_property_read_u32(np, "lm3630,default_brightness",
			&pdata->default_brightness);
	rc = of_property_read_u32(np, "lm3630,max_brightness",
			&pdata->max_brightness);
	rc = of_property_read_u32(np, "lm3630,enable_pwm",
			&lm3630_pwm_enable);
	if (rc == -EINVAL)
		lm3630_pwm_enable = 1;
	rc = of_property_read_u32(np, "lm3630,blmap_size",
			&pdata->blmap_size);
	if (pdata->blmap_size) {
		array = kzalloc(sizeof(u32) * pdata->blmap_size, GFP_KERNEL);
		if (!array)
			return -ENOMEM;
		rc = of_property_read_u32_array(np, "lm3630,blmap", array, pdata->blmap_size);
		if (rc) {
			pr_err("%s: Uable to read backlight map\n", __func__);
			return -EINVAL;
		}
		pdata->blmap = kzalloc(sizeof(char) * pdata->blmap_size, GFP_KERNEL);
		if (!pdata->blmap)
			return -ENOMEM;
		for (i = 0; i < pdata->blmap_size; i++)
			pdata->blmap[i] = (char)array[i];
		if (array)
			kfree(array);
	} else {
		pdata->blmap = NULL;
	}
	pr_debug("%s: gpio=%d, max_current=%d, min=%d, default=%d, max=%d, pwm=%d , blmap_size=%d\n",
			__func__, pdata->gpio,
			pdata->max_current,
			pdata->min_brightness,
			pdata->default_brightness,
			pdata->max_brightness,
			lm3630_pwm_enable,
			pdata->blmap_size);

	return rc;
}
#endif

static struct backlight_ops lm3630_bl_ops = {
	.update_status = bl_set_intensity,
	.get_brightness = bl_get_intensity,
};

static int lm3630_probe(struct i2c_client *i2c_dev, const struct i2c_device_id *id)
{
	struct backlight_platform_data *pdata;
	struct lm3630_device *dev;
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	int err;

	pr_info("%s: i2c probe start\n", __func__);
#ifdef CONFIG_OF
	if (&i2c_dev->dev.of_node) {
		pdata = devm_kzalloc(&i2c_dev->dev,
				     sizeof(struct backlight_platform_data),
				     GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		err = lm3630_parse_dt(&i2c_dev->dev, pdata);
		if (err != 0)
			return err;
	} else {
		pdata = i2c_dev->dev.platform_data;
	}
#else
	pdata = i2c_dev->dev.platform_data;
#endif

	dev = kzalloc(sizeof(struct lm3630_device), GFP_KERNEL);
	if (dev == NULL) {
		pr_err("%s: fail alloc for lm3630_device\n", __func__);
		return -ENOMEM;
	}
	main_lm3630_dev = dev;
	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = MAX_BRIGHTNESS_LM3630;
	bl_dev = backlight_device_register(I2C_BL_NAME, &i2c_dev->dev,
			NULL, &lm3630_bl_ops, &props);
	bl_dev->props.max_brightness = MAX_BRIGHTNESS_LM3630; /* TODO */
	bl_dev->props.brightness = DEFAULT_BRIGHTNESS; /* TODO */
	bl_dev->props.power = FB_BLANK_UNBLANK;
	dev->bl_dev = bl_dev;
	dev->client = i2c_dev;
	dev->gpio = pdata->gpio;
	dev->max_current = pdata->max_current;
	dev->min_brightness = pdata->min_brightness;
	dev->default_brightness = pdata->default_brightness;
	dev->max_brightness = pdata->max_brightness;
	dev->blmap_size = pdata->blmap_size;
	if (dev->blmap_size) {
		dev->blmap = kzalloc(sizeof(char) * dev->blmap_size, GFP_KERNEL);
		if (!dev->blmap) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		memcpy(dev->blmap, pdata->blmap, dev->blmap_size);
	} else {
		dev->blmap = NULL;
	}
	i2c_set_clientdata(i2c_dev, dev);

	/* reference the reset procedure from lm3530 */
	if (gpio_is_valid(dev->gpio)) {
		err = gpio_request(dev->gpio, "lm3630 reset");
		if (err < 0) {
			pr_err("%s: failed to request gpio\n", __func__);
			goto err_gpio_request;
		}
	}

	mutex_init(&dev->bl_mutex);
	err = device_create_file(&i2c_dev->dev, &dev_attr_lm3630_level);
	if (err < 0) {
		pr_err("%s: failed to create sysfs level\n", __func__);
		goto err_create_sysfs_level;
	}
	err = device_create_file(&i2c_dev->dev, &dev_attr_lm3630_backlight_on_off);
	if (err < 0) {
		pr_err("%s: failed to create sysfs bl_on_off\n", __func__);
		goto err_create_sysfs_bl_on_off;
	}
	err = device_create_file(&i2c_dev->dev, &dev_attr_lm3630_exp_min_value);
	if (err < 0) {
		pr_err("%s: failed to create sysfs exp_min_value\n", __func__);
		goto err_create_sysfs_exp_min_value;
	}
	err = device_create_file(&i2c_dev->dev, &dev_attr_lm3630_pwm);
	if (err < 0) {
		pr_err("%s: failed to create sysfs pwm\n", __func__);
		goto err_create_sysfs_pwm;
	}
	lm3630_i2c_client = i2c_dev;
	pr_info("%s: lm3630 probed\n", __func__);
	return 0;

/* error handling */
err_create_sysfs_pwm:
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3630_exp_min_value);
err_create_sysfs_exp_min_value:
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3630_backlight_on_off);
err_create_sysfs_bl_on_off:
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3630_level);
err_create_sysfs_level:
	/* unregister gpio */
	if (gpio_is_valid(dev->gpio))
		gpio_free(dev->gpio);
err_gpio_request:
	backlight_device_unregister(dev->bl_dev);
	kfree(dev);
	return err;
}

static int lm3630_remove(struct i2c_client *i2c_dev)
{
	struct lm3630_device *dev = (struct lm3630_device *)i2c_get_clientdata(i2c_dev);
	int gpio = dev->gpio; /* main_lm3630_dev->gpio; */

	lm3630_i2c_client = NULL;
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3630_level);
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3630_backlight_on_off);
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3630_exp_min_value);
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3630_pwm);
	i2c_set_clientdata(i2c_dev, NULL);
	if (gpio_is_valid(gpio))
		gpio_free(gpio);
	backlight_device_unregister(dev->bl_dev);
	kfree(dev);
	return 0;
}

#ifdef CONFIG_OF
static struct of_device_id lm3630_match_table[] = {
	{ .compatible = "backlight,lm3630",},
	{ },
};
#endif

static struct i2c_driver main_lm3630_driver = {
	.probe = lm3630_probe,
	.remove = lm3630_remove,
	.suspend = NULL,
	.resume = NULL,
	.id_table = lm3630_bl_id,
	.driver = {
		.name = I2C_BL_NAME,
		.owner = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = lm3630_match_table,
#endif
	},
};

static int __init lcd_backlight_init(void)
{
	return i2c_add_driver(&main_lm3630_driver);
}

module_init(lcd_backlight_init);

MODULE_DESCRIPTION("LM3630 Backlight Control");
MODULE_AUTHOR("daewoo kwak");
MODULE_LICENSE("GPL");
