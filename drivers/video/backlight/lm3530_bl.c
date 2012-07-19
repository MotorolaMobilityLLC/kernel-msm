/*
 *  LM3530 backlight device driver
 *
 *  Copyright (C) 2011-2012, LG Eletronics,Inc. All rights reservced.
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
#include <linux/platform_data/lm35xx_bl.h>
#include <linux/kernel.h>
#include <linux/spinlock.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/mutex.h>
#include <mach/board.h>

#define MAX_LEVEL               0x71
#define MIN_LEVEL               0x01
#define DEFAULT_LEVEL           0x2A

#define DEFAULT_FTM_BRIGHTNESS  0x03

#define I2C_BL_NAME             "lm3530"

#define BL_ON                   1
#define BL_OFF                  0

static DEFINE_MUTEX(backlight_mtx);

static struct i2c_client *lm3530_i2c_client;

struct lm3530_device {
	struct i2c_client *client;
	struct backlight_device *bl_dev;
	int gpio;
	int max_current;
	int min_brightness;
	int max_brightness;
	int factory_brightness;
};

static const struct i2c_device_id lm3530_bl_id[] = {
	{ I2C_BL_NAME, 0 },
	{ },
};

static int lm3530_write_reg(struct i2c_client *client,
		unsigned char reg, unsigned char val);

static int cur_main_lcd_level;
static int saved_main_lcd_level;
static int backlight_status = BL_ON;

static void lm3530_hw_reset(struct i2c_client *client)
{
	struct lm3530_device *dev = i2c_get_clientdata(client);
	int gpio = dev->gpio;

	if (gpio_is_valid(gpio)) {
		gpio_direction_output(gpio, 1);
		gpio_set_value_cansleep(gpio, 1);
		mdelay(1);
	}
}

static int lm3530_write_reg(struct i2c_client *client,
		unsigned char reg, unsigned char val)
{
	u8 buf[2];
	struct i2c_msg msg = {
		client->addr, 0, 2, buf
	};

	buf[0] = reg;
	buf[1] = val;

	if (i2c_transfer(client->adapter, &msg, 1) < 0)
		dev_err(&client->dev, "i2c write error\n");

	return 0;
}

static void lm3530_set_main_current_level(struct i2c_client *client, int level)
{
	struct lm3530_device *dev = i2c_get_clientdata(client);
	int cal_value = 0;
	int min_brightness = dev->min_brightness;
	int max_brightness = dev->max_brightness;

	dev->bl_dev->props.brightness = cur_main_lcd_level = level;

	if (level != 0) {
		if (level > 0 && level <= MIN_LEVEL)
			cal_value = min_brightness;
		else if (level > MIN_LEVEL && level <= MAX_LEVEL)
			cal_value = (max_brightness - min_brightness)*level
				/(MAX_LEVEL - MIN_LEVEL)-
				((max_brightness - min_brightness)*MIN_LEVEL
				/(MAX_LEVEL - MIN_LEVEL) - min_brightness);
		else if (level > MAX_LEVEL)
			cal_value = max_brightness;

		lm3530_write_reg(client, 0xA0, cal_value);
	} else
		lm3530_write_reg(client, 0x10, 0x00);

	mdelay(1);
}

static void lm3530_backlight_on(struct i2c_client *client, int level)
{
	struct lm3530_device *dev = i2c_get_clientdata(client);

	mutex_lock(&backlight_mtx);
	if (backlight_status == BL_OFF) {
		pr_info("%s, ++ lm3530_backlight_on  \n",__func__);
		lm3530_hw_reset(client);

		lm3530_write_reg(dev->client, 0xA0, 0x00);
		lm3530_write_reg(dev->client, 0x10, dev->max_current);
		lm3530_write_reg(dev->client, 0x30, 0x25);
	}

	lm3530_set_main_current_level(dev->client, level);
	backlight_status = BL_ON;
	mutex_unlock(&backlight_mtx);
}

static void lm3530_backlight_off(struct i2c_client *client)
{
	struct lm3530_device *dev = i2c_get_clientdata(client);
	int gpio = dev->gpio;

	pr_info("%s, on: %d\n", __func__, backlight_status);

	mutex_lock(&backlight_mtx);
	if (backlight_status == BL_OFF) {
		mutex_unlock(&backlight_mtx);
		return;
	}

	saved_main_lcd_level = cur_main_lcd_level;
	lm3530_set_main_current_level(dev->client, 0);
	backlight_status = BL_OFF;

	gpio_tlmm_config(GPIO_CFG(gpio, 0, GPIO_CFG_OUTPUT, GPIO_CFG_NO_PULL,
			GPIO_CFG_2MA), GPIO_CFG_ENABLE);
	gpio_direction_output(gpio, 0);
	msleep(6);
	mutex_unlock(&backlight_mtx);
}

void lm3530_lcd_backlight_set_level(int level)
{
	struct i2c_client *client = lm3530_i2c_client;

	if (!client) {
		pr_warn("%s: not yet enabled\n", __func__);
		return;
	}

	if (level > MAX_LEVEL)
		level = MAX_LEVEL;

	pr_debug("%s: level %d\n", __func__, level);
	if (level)
		lm3530_backlight_on(client, level);
	else
		lm3530_backlight_off(client);
}
EXPORT_SYMBOL(lm3530_lcd_backlight_set_level);

void lm3530_lcd_backlight_pwm_disable(void)
{
	struct i2c_client *client = lm3530_i2c_client;
	struct lm3530_device *dev = i2c_get_clientdata(client);

	if (backlight_status == BL_OFF)
		return;

	lm3530_write_reg(client, 0x10, dev->max_current & 0x1F);
}
EXPORT_SYMBOL(lm3530_lcd_backlight_pwm_disable);

static int bl_set_intensity(struct backlight_device *bd)
{
	lm3530_lcd_backlight_set_level(bd->props.brightness);
	return 0;
}

static int bl_get_intensity(struct backlight_device *bd)
{
	return cur_main_lcd_level;
}

static ssize_t lcd_backlight_show_level(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n",
			cur_main_lcd_level);
}

static ssize_t lcd_backlight_store_level(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int level;

	if (!count)
		return -EINVAL;

	level = simple_strtoul(buf, NULL, 10);
	lm3530_lcd_backlight_set_level(level);

	return count;
}

static int lm3530_bl_resume(struct i2c_client *client)
{
	lm3530_backlight_on(client, saved_main_lcd_level);
	return 0;
}

static int lm3530_bl_suspend(struct i2c_client *client, pm_message_t state)
{
	pr_info("%s: new state: %d\n", __func__, state.event);

	lm3530_backlight_off(client);

	return 0;
}

static ssize_t lcd_backlight_show_on_off(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	pr_info("%s received (prev backlight_status: %s)\n", __func__,
			backlight_status ? "ON" : "OFF");
	return 0;
}

static ssize_t lcd_backlight_store_on_off(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int on_off;
	struct i2c_client *client = to_i2c_client(dev);

	if (!count)
		return -EINVAL;

	pr_info("%s received (prev backlight_status: %s)\n", __func__,
			backlight_status ? "ON" : "OFF");

	on_off = simple_strtoul(buf, NULL, 10);

	pr_info("%d", on_off);

	if (on_off == 1)
		lm3530_bl_resume(client);
	else if (on_off == 0)
		lm3530_bl_suspend(client, PMSG_SUSPEND);

	return count;

}
DEVICE_ATTR(lm3530_level, 0644, lcd_backlight_show_level,
		lcd_backlight_store_level);
DEVICE_ATTR(lm3530_backlight_on_off, 0644, lcd_backlight_show_on_off,
		lcd_backlight_store_on_off);

static struct backlight_ops lm3530_bl_ops = {
	.update_status = bl_set_intensity,
	.get_brightness = bl_get_intensity,
};

static int __devinit lm3530_probe(struct i2c_client *i2c_dev,
		const struct i2c_device_id *id)
{
	struct backlight_platform_data *pdata;
	struct lm3530_device *dev;
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	int err = 0;

	pdata = i2c_dev->dev.platform_data;
	if (!pdata)
		return -ENODEV;

	dev = kzalloc(sizeof(struct lm3530_device), GFP_KERNEL);
	if (!dev) {
		dev_err(&i2c_dev->dev, "out of memory\n");
		return -ENOMEM;
	}

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = MAX_LEVEL;

	bl_dev = backlight_device_register(I2C_BL_NAME, &i2c_dev->dev, NULL,
			&lm3530_bl_ops, &props);
	if (IS_ERR(bl_dev)) {
		dev_err(&i2c_dev->dev, "failed to register backlight\n");
		err = PTR_ERR(bl_dev);
		goto err_backlight_device_register;
	}
	bl_dev->props.max_brightness = MAX_LEVEL;
	bl_dev->props.brightness = DEFAULT_LEVEL;
	bl_dev->props.power = FB_BLANK_UNBLANK;

	dev->bl_dev = bl_dev;
	dev->client = i2c_dev;
	dev->gpio = pdata->gpio;
	dev->max_current = pdata->max_current;
	dev->min_brightness = pdata->min_brightness;
	dev->max_brightness = pdata->max_brightness;
	i2c_set_clientdata(i2c_dev, dev);

	dev->factory_brightness = DEFAULT_FTM_BRIGHTNESS;

	if (gpio_is_valid(dev->gpio)) {
		err = gpio_request(dev->gpio, "lm3530 reset");
		if (err < 0) {
			dev_err(&i2c_dev->dev, "failed to request gpio\n");
			goto err_gpio_request;
		}
	}

	err = device_create_file(&i2c_dev->dev, &dev_attr_lm3530_level);
	if (err < 0) {
		dev_err(&i2c_dev->dev, "failed to create 1st sysfs\n");
		goto err_device_create_file_1;
	}
	err = device_create_file(&i2c_dev->dev,
			&dev_attr_lm3530_backlight_on_off);
	if (err < 0) {
		dev_err(&i2c_dev->dev, "failed to create 2nd sysfs\n");
		goto err_device_create_file_2;
	}

	lm3530_i2c_client = i2c_dev;

	pr_info("lm3530 probed\n");
	return 0;

err_device_create_file_2:
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3530_level);
err_device_create_file_1:
	if (gpio_is_valid(dev->gpio))
		gpio_free(dev->gpio);
err_gpio_request:
	backlight_device_unregister(bl_dev);
err_backlight_device_register:
	kfree(dev);

	return err;
}

static int __devexit lm3530_remove(struct i2c_client *i2c_dev)
{
	struct lm3530_device *dev = i2c_get_clientdata(i2c_dev);

	lm3530_i2c_client = NULL;
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3530_level);
	device_remove_file(&i2c_dev->dev, &dev_attr_lm3530_backlight_on_off);
	i2c_set_clientdata(i2c_dev, NULL);

	if (gpio_is_valid(dev->gpio))
		gpio_free(dev->gpio);

	backlight_device_unregister(dev->bl_dev);
	kfree(dev);

	return 0;
}

static struct i2c_driver main_lm3530_driver = {
	.probe = lm3530_probe,
	.remove = lm3530_remove,
	.id_table = lm3530_bl_id,
	.driver = {
		.name = I2C_BL_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init lcd_backlight_init(void)
{
	return i2c_add_driver(&main_lm3530_driver);
}

module_init(lcd_backlight_init);

MODULE_DESCRIPTION("LM3530 Backlight Control");
MODULE_AUTHOR("Jaeseong Gim <jaeseong.gim@lge.com>");
MODULE_LICENSE("GPL");
