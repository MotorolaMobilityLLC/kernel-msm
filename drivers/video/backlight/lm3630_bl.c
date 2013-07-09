/*
 * Copyright (c) 2013 LGE Inc. All rights reserved.
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
#include <linux/platform_data/lm3630_bl.h>
#include <linux/kernel.h>
#include <linux/slab.h>
#include <linux/backlight.h>
#include <linux/fb.h>
#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/of_gpio.h>
#include <linux/debugfs.h>

#ifdef CONFIG_MACH_LGE
/* HACK: disable fb notifier unless off-mode charge */
#include <mach/board_lge.h>
#endif

#define I2C_BL_NAME   "lm3630"

/* Register definitions */
#define CONTROL_REG        0x00
#define CONFIG_REG         0x01
#define BOOST_CTL_REG      0x02
#define BRIGHTNESS_A_REG   0x03
#define BRIGHTNESS_B_REG   0x04
#define CURRENT_A_REG      0x05
#define CURRENT_B_REG      0x06
#define ON_OFF_RAMP_REG    0x07
#define RUN_RAMP_REG       0x08
#define IRQ_STATUS_REG     0x09
#define IRQ_ENABLE_REG     0x0A
#define FAULT_STAT_REG     0x0B
#define SOFT_RESET_REG     0x0F
#define PWM_OUT_REG        0x12
#define REVISION_REG       0x1F

#define SLEEP_CMD_MASK     0x80
#define LINEAR_A_MASK      0x10
#define LINEAR_B_MASK      0x08
#define LED_A_EN_MASK      0x04
#define LED_B_EN_MASK      0x02
#define LED2_ON_A_MASK     0x01
#define FEEDBACK_B_MASK    0x10
#define FEEDBACK_A_MASK    0x08
#define PWM_EN_B_MASK      0x02
#define PWM_EN_A_MASK      0x01

#define DEFAULT_CTRL_REG   0xC0
#define DEFAULT_CFG_REG    0x18

#define BL_OFF 0x00

enum {
	LED_BANK_A,
	LED_BANK_B,
	LED_BANK_AB,
};

static DEFINE_MUTEX(backlight_mtx);

struct lm3630_device {
	struct i2c_client *client;
	struct backlight_device *bl_dev;
	struct dentry  *dent;
	int en_gpio;
	int boost_ctrl_reg;
	int ctrl_reg;
	int bank_sel;
	int cfg_reg;
	int linear_map;
	int max_current;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int pwm_enable;
	int blmap_size;
	char *blmap;
};

static const struct i2c_device_id lm3630_bl_id[] = {
	{ I2C_BL_NAME, 0 },
	{ },
};

static struct lm3630_device *lm3630_dev;

struct debug_reg {
	char  *name;
	u8  reg;
};

#define LM3630_DEBUG_REG(x) {#x, x##_REG}

static struct debug_reg lm3630_debug_regs[] = {
	LM3630_DEBUG_REG(CONTROL),
	LM3630_DEBUG_REG(CONFIG),
	LM3630_DEBUG_REG(BOOST_CTL),
	LM3630_DEBUG_REG(BRIGHTNESS_A),
	LM3630_DEBUG_REG(BRIGHTNESS_B),
	LM3630_DEBUG_REG(CURRENT_A),
	LM3630_DEBUG_REG(CURRENT_B),
	LM3630_DEBUG_REG(ON_OFF_RAMP),
	LM3630_DEBUG_REG(RUN_RAMP),
	LM3630_DEBUG_REG(IRQ_STATUS),
	LM3630_DEBUG_REG(IRQ_ENABLE),
	LM3630_DEBUG_REG(FAULT_STAT),
	LM3630_DEBUG_REG(SOFT_RESET),
	LM3630_DEBUG_REG(PWM_OUT),
	LM3630_DEBUG_REG(REVISION),
};

static void lm3630_hw_reset(struct lm3630_device *dev)
{
	if (gpio_is_valid(dev->en_gpio)) {
		gpio_set_value_cansleep(dev->en_gpio, 1);
		mdelay(1);
	} else {
		pr_err("%s: en_gpio is not valid !!\n", __func__);
	}
}

static int lm3630_read_reg(struct i2c_client *client, u8 reg, u8 *buf)
{
	int ret;

	pr_debug("reg=0x%x\n", reg);
	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		pr_err("%s: i2c error! addr = 0x%x\n", __func__, reg);
		return ret;
	}

	*buf = (u8) ret;
	return 0;
}

static int lm3630_write_reg(struct i2c_client *client, u8 reg, u8 val)
{
	int ret;

	pr_debug("%s: reg=0x%x\n", __func__, reg);
	ret = i2c_smbus_write_byte_data(client, reg, val);
	if (ret < 0)
		pr_err("%s: i2c error! addr = 0x%x\n", __func__, reg);

	return ret;
}

static int set_reg(void *data, u64 val)
{
	u32 addr = (u32) data;
	int ret;
	struct i2c_client *client = lm3630_dev->client;

	ret = lm3630_write_reg(client, addr, (u8) val);

	return ret;
}

static int get_reg(void *data, u64 *val)
{
	u32 addr = (u32) data;
	u8 temp;
	int ret;
	struct i2c_client *client = lm3630_dev->client;

	ret = lm3630_read_reg(client, addr, &temp);
	if (ret < 0)
		return ret;

	*val = temp;

	return 0;
}

DEFINE_SIMPLE_ATTRIBUTE(reg_fops, get_reg, set_reg, "0x%02llx\n");

static void lm3630_set_brightness_reg(struct lm3630_device *dev, int level)
{
	if (dev->bank_sel == LED_BANK_A) {
		lm3630_write_reg(dev->client, BRIGHTNESS_A_REG, level);
	} else if (dev->bank_sel == LED_BANK_B) {
		lm3630_write_reg(dev->client, BRIGHTNESS_B_REG, level);
	} else {
		lm3630_write_reg(dev->client, BRIGHTNESS_A_REG, level);
		lm3630_write_reg(dev->client, BRIGHTNESS_B_REG, level);
	}
}

static void lm3630_set_main_current_level(struct i2c_client *client, int level)
{
	struct lm3630_device *dev = i2c_get_clientdata(client);

	mutex_lock(&backlight_mtx);
	dev->bl_dev->props.brightness = level;
	if (level != 0) {
		if (level < dev->min_brightness)
			level = dev->min_brightness;
		else if (level > dev->max_brightness)
			level = dev->max_brightness;

		if (dev->blmap) {
			if (level < dev->blmap_size)
				lm3630_set_brightness_reg(dev, dev->blmap[level]);
			else
				pr_err("%s: invalid index %d:%d\n", __func__,
						dev->blmap_size, level);
		} else {
			lm3630_set_brightness_reg(dev, level);
		}
	} else {
		lm3630_write_reg(client, CONTROL_REG, BL_OFF);
	}
	mutex_unlock(&backlight_mtx);
	pr_debug("%s: level=%d\n", __func__, level);
}

static void lm3630_set_max_current_reg(struct lm3630_device *dev, int val)
{
	if (dev->bank_sel == LED_BANK_A) {
		lm3630_write_reg(dev->client, CURRENT_A_REG, val);
	} else if (dev->bank_sel == LED_BANK_B) {
		lm3630_write_reg(dev->client, CURRENT_B_REG, val);
	} else {
		lm3630_write_reg(dev->client, CURRENT_A_REG, val);
		lm3630_write_reg(dev->client, CURRENT_B_REG, val);
	}
}

static void lm3630_hw_init(struct lm3630_device *dev)
{
	lm3630_hw_reset(dev);
	lm3630_write_reg(dev->client, BOOST_CTL_REG, dev->boost_ctrl_reg);
	lm3630_write_reg(dev->client, CONFIG_REG, dev->cfg_reg);
	lm3630_set_max_current_reg(dev, dev->max_current);
	lm3630_write_reg(dev->client, CONTROL_REG, dev->ctrl_reg);
	mdelay(1);
}

static void lm3630_backlight_on(struct lm3630_device *dev, int level)
{
	if (dev->bl_dev->props.brightness == 0) {
		lm3630_hw_init(dev);
		pr_info("%s\n", __func__);
	}
	lm3630_set_main_current_level(dev->client, level);
}

static void lm3630_backlight_off(struct lm3630_device *dev)
{
	if (dev->bl_dev->props.brightness == 0)
		return;

	lm3630_set_main_current_level(dev->client, 0);
	gpio_set_value_cansleep(dev->en_gpio, 0);
	udelay(1);
	pr_info("%s\n", __func__);
}

void lm3630_lcd_backlight_set_level(int level)
{
	if (!lm3630_dev) {
		pr_warn("%s: No device\n", __func__);
		return;
	}

	pr_debug("%s: level=%d\n", __func__, level);

	if (level)
		lm3630_backlight_on(lm3630_dev, level);
	else
		lm3630_backlight_off(lm3630_dev);
}
EXPORT_SYMBOL(lm3630_lcd_backlight_set_level);

static int bl_set_intensity(struct backlight_device *bd)
{
	struct i2c_client *client = to_i2c_client(bd->dev.parent);
	struct lm3630_device *dev = i2c_get_clientdata(client);
	int brightness = bd->props.brightness;

	if ((bd->props.state & BL_CORE_FBBLANK) ||
			(bd->props.state & BL_CORE_SUSPENDED))
		brightness = 0;
	else if (brightness == 0)
		brightness = dev->default_brightness;

	if (brightness == bd->props.brightness) {
		pr_debug("%s: requsted level is already set!\n", __func__);
		return 0;
	}

	lm3630_lcd_backlight_set_level(brightness);
	return 0;
}

static int bl_get_intensity(struct backlight_device *bd)
{
	return bd->props.brightness;
}

static ssize_t lcd_backlight_show_level(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct lm3630_device *lm3630 = i2c_get_clientdata(client);
	int r = 0;

	r = snprintf(buf, PAGE_SIZE, "LCD Backlight Level is : %d\n",
				lm3630->bl_dev->props.brightness);
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
	lm3630_set_main_current_level(client, level);
	pr_debug("%s: level=%d\n", __func__, level);

	return count;
}

DEVICE_ATTR(lm3630_level, 0644, lcd_backlight_show_level,
		lcd_backlight_store_level);

static int lm3630_create_debugfs_entries(struct lm3630_device *chip)
{
	int i;

	chip->dent = debugfs_create_dir(I2C_BL_NAME, NULL);
	if (IS_ERR(chip->dent)) {
		pr_err("%s: lm3630 driver couldn't create debugfs dir\n",
				__func__);
		return -EFAULT;
	}

	for (i = 0 ; i < ARRAY_SIZE(lm3630_debug_regs) ; i++) {
		char *name = lm3630_debug_regs[i].name;
		u32 reg = lm3630_debug_regs[i].reg;
		struct dentry *file;

		file = debugfs_create_file(name, 0644, chip->dent,
					(void *) reg, &reg_fops);
		if (IS_ERR(file)) {
			pr_err("%s: debugfs_create_file %s failed.\n",
					__func__, name);
			return -EFAULT;
		}
	}

	return 0;
}

static void lm3630_set_init_values(struct lm3630_device *dev)
{
	if (dev->bank_sel == LED_BANK_A) {
		dev->ctrl_reg = (DEFAULT_CTRL_REG & ~SLEEP_CMD_MASK)
			| LED_A_EN_MASK | LED2_ON_A_MASK;

		if (dev->linear_map)
			dev->ctrl_reg = dev->ctrl_reg | LINEAR_A_MASK;

		dev->cfg_reg = (DEFAULT_CFG_REG & ~ FEEDBACK_B_MASK)
			| FEEDBACK_A_MASK;

		if (dev->pwm_enable)
			dev->cfg_reg = dev->cfg_reg | PWM_EN_A_MASK;
	} else if (dev->bank_sel == LED_BANK_B) {
		dev->ctrl_reg = (DEFAULT_CTRL_REG & ~SLEEP_CMD_MASK)
			| LINEAR_B_MASK | LED_B_EN_MASK;

		if (dev->linear_map)
			dev->ctrl_reg = dev->ctrl_reg | LINEAR_B_MASK;

		dev->cfg_reg = (DEFAULT_CFG_REG & ~FEEDBACK_A_MASK)
				| FEEDBACK_B_MASK;

		if (dev->pwm_enable)
			dev->cfg_reg = dev->cfg_reg | PWM_EN_B_MASK;
	} else {
		dev->ctrl_reg = (DEFAULT_CTRL_REG & ~SLEEP_CMD_MASK)
			| LINEAR_A_MASK | LINEAR_B_MASK
			| LED_A_EN_MASK | LED_B_EN_MASK;

		if (dev->linear_map)
			dev->ctrl_reg = dev->ctrl_reg
				| LINEAR_A_MASK | LINEAR_B_MASK;

		dev->cfg_reg = DEFAULT_CFG_REG
			| FEEDBACK_A_MASK | FEEDBACK_B_MASK;

		if (dev->pwm_enable)
			dev->cfg_reg = dev->cfg_reg
				| PWM_EN_A_MASK | PWM_EN_B_MASK;
	}
}

static int lm3630_parse_dt(struct device_node *node,
			struct lm3630_device *dev)
{
	int i = 0, rc = 0;
	u32 *array = NULL;

	dev->en_gpio = of_get_named_gpio(node, "lm3630,lcd_bl_en", 0);
	if (dev->en_gpio < 0) {
		pr_err("%s: failed to get lm3630,lcd_bl_en\n", __func__);
		rc = dev->en_gpio;
		goto error;
	}

	rc = of_property_read_u32(node, "lm3630,boost_ctrl_reg",
				&dev->boost_ctrl_reg);
	if (rc) {
		pr_err("%s: failed to get lm3630,boost_ctrl_reg\n", __func__);
		goto error;
	}

	rc = of_property_read_u32(node, "lm3630,bank_sel",
				&dev->bank_sel);
	if (rc) {
		pr_err("%s: failed to get lm3630,bank_sel\n", __func__);
		goto error;
	}

	rc = of_property_read_u32(node, "lm3630,linear_map",
				&dev->linear_map);
	if (rc) {
		pr_err("%s: failed to get lm3630,linear_map\n", __func__);
		goto error;
	}

	rc = of_property_read_u32(node, "lm3630,max_current",
				&dev->max_current);
	if (rc) {
		pr_err("%s: failed to get lm3630,max_current\n", __func__);
		goto error;
	}

	rc = of_property_read_u32(node, "lm3630,min_brightness",
				&dev->min_brightness);
	if (rc) {
		pr_err("%s: failed to get lm3630,min_brightness\n", __func__);
		goto error;
	}

	rc = of_property_read_u32(node, "lm3630,default_brightness",
				&dev->default_brightness);
	if (rc) {
		pr_err("%s: failed to get lm3630,default_brightness\n",
				__func__);
		goto error;
	}

	rc = of_property_read_u32(node, "lm3630,max_brightness",
				&dev->max_brightness);
	if (rc) {
		pr_err("%s: failed to get lm3630,max_brightness\n", __func__);
		goto error;
	}

	rc = of_property_read_u32(node, "lm3630,pwm_enable",
				&dev->pwm_enable);
	if (rc) {
		pr_err("%s: failed to get lm3630,pwm_enable\n", __func__);
		goto error;
	}

	rc = of_property_read_u32(node, "lm3630,blmap_size",
				&dev->blmap_size);
	if (rc) {
		pr_err("%s: failed to get lm3630,blmap_size\n", __func__);
		goto error;
	}

	if (dev->blmap_size) {
		array = kzalloc(sizeof(u32) * dev->blmap_size, GFP_KERNEL);
		if (!array)
			goto error;

		rc = of_property_read_u32_array(node, "lm3630,blmap", array,
						dev->blmap_size);
		if (rc) {
			pr_err("%s: failed to read backlight map\n", __func__);
			goto err_blmap;
		}

		dev->blmap = kzalloc(sizeof(char) * dev->blmap_size,
					GFP_KERNEL);
		if (!dev->blmap)
			goto err_blmap;

		for (i = 0; i < dev->blmap_size; i++)
			dev->blmap[i] = (char)array[i];

		if (array)
			kfree(array);
	}

	pr_info("%s: max_current =0x%X min_brightness = 0x%X "
		"max_brightness = 0x%X default_brightness = 0x%X \n",
					__func__,
					dev->max_current,
					dev->min_brightness,
					dev->max_brightness,
					dev->default_brightness);
	pr_info("%s: boost_ctrl_reg = 0x%X bank_sel = %d "
		"linear_map = %d pwm_enable = %d blmap_size = %d\n",
					__func__,
					dev->boost_ctrl_reg,
					dev->bank_sel,
					dev->linear_map,
					dev->pwm_enable,
					dev->blmap_size);

	return 0;

err_blmap:
	kfree(array);
error:
	return rc;
}

static struct backlight_ops lm3630_bl_ops = {
	.update_status = bl_set_intensity,
	.get_brightness = bl_get_intensity,
};

static int lm3630_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct lm3630_platform_data *pdata;
	struct lm3630_device *dev;
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	struct device_node *dev_node = client->dev.of_node;
	int ret = 0;

	dev = kzalloc(sizeof(struct lm3630_device), GFP_KERNEL);
	if (!dev) {
		pr_err("%s: failed to allocate lm3630_device\n", __func__);
		return -ENOMEM;
	}

	if (dev_node) {
		ret = lm3630_parse_dt(dev_node, dev);
		if (ret) {
			pr_err("%s: failed to parse dt\n", __func__);
			goto err_parse_dt;
		}
	} else {
		pdata = client->dev.platform_data;
		if (pdata == NULL) {
			pr_err("%s: no platform data\n", __func__);
			goto err_parse_dt;
		}

		dev->en_gpio = pdata->en_gpio;
		dev->boost_ctrl_reg = pdata->boost_ctrl_reg;
		dev->bank_sel = pdata->bank_sel;
		dev->linear_map = pdata->linear_map;
		dev->max_current = pdata->max_current;
		dev->min_brightness = pdata->min_brightness;
		dev->default_brightness = pdata->default_brightness;
		dev->max_brightness = pdata->max_brightness;
		dev->pwm_enable = pdata->pwm_enable;
		dev->blmap_size = pdata->blmap_size;
		if (dev->blmap_size)
			dev->blmap = pdata->blmap;
	}

	/* initialize register values for hw reset */
	lm3630_set_init_values(dev);

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.max_brightness = dev->max_brightness;

	bl_dev = backlight_device_register(I2C_BL_NAME, &client->dev,
			NULL, &lm3630_bl_ops, &props);
	if (IS_ERR(bl_dev)) {
		pr_err("%s: failed to register backlight\n", __func__);
		ret = PTR_ERR(bl_dev);
		goto err_bl_dev_reg;
	}

#ifdef CONFIG_MACH_LGE
	/* HACK: disable fb notifier unless off-mode charge */
	if (lge_get_boot_mode() != LGE_BOOT_MODE_CHARGER)
		fb_unregister_client(&bl_dev->fb_notif);
#endif

	bl_dev->props.max_brightness = dev->max_brightness;
	bl_dev->props.brightness = dev->default_brightness;
	bl_dev->props.power = FB_BLANK_UNBLANK;
	dev->bl_dev = bl_dev;
	dev->client = client;
	i2c_set_clientdata(client, dev);

	if (gpio_is_valid(dev->en_gpio)) {
		ret = gpio_request_one(dev->en_gpio, GPIOF_OUT_INIT_HIGH,
					"lm3630_en");
		if (ret) {
			pr_err("%s: failed to request en_gpio\n", __func__);
			goto err_gpio_request;
		}
	}

	ret = device_create_file(&client->dev,
			&dev_attr_lm3630_level);
	if (ret) {
		pr_err("%s: failed to create sysfs level\n", __func__);
		goto err_create_sysfs_level;
	}

	ret = lm3630_create_debugfs_entries(dev);
	if (ret) {
		pr_err("%s: lm3630_create_debugfs_entries failed\n", __func__);
		goto err_create_debugfs;
	}

	lm3630_dev = dev;

	pr_info("%s: lm3630 probed\n", __func__);
	return 0;

err_create_debugfs:
	device_remove_file(&client->dev, &dev_attr_lm3630_level);
err_create_sysfs_level:
	if (gpio_is_valid(dev->en_gpio))
		gpio_free(dev->en_gpio);
err_gpio_request:
	backlight_device_unregister(bl_dev);
err_bl_dev_reg:
	if (dev_node && dev->blmap)
		kfree(dev->blmap);
err_parse_dt:
	kfree(dev);
	return ret;
}

static int lm3630_remove(struct i2c_client *client)
{
	struct lm3630_device *dev = i2c_get_clientdata(client);

	lm3630_dev = NULL;
	if (dev->dent)
		debugfs_remove_recursive(dev->dent);
	device_remove_file(&client->dev, &dev_attr_lm3630_level);
	i2c_set_clientdata(client, NULL);
	if (gpio_is_valid(dev->en_gpio))
		gpio_free(dev->en_gpio);
	backlight_device_unregister(dev->bl_dev);
	if (client->dev.of_node && dev->blmap)
		kfree(dev->blmap);
	kfree(dev);
	return 0;
}

static struct of_device_id lm3630_match_table[] = {
	{ .compatible = "backlight,lm3630",},
	{ },
};

static struct i2c_driver main_lm3630_driver = {
	.probe = lm3630_probe,
	.remove = lm3630_remove,
	.id_table = lm3630_bl_id,
	.driver = {
		.name = I2C_BL_NAME,
		.owner = THIS_MODULE,
		.of_match_table = lm3630_match_table,
	},
};

static int __init lcd_backlight_init(void)
{
	return i2c_add_driver(&main_lm3630_driver);
}

module_init(lcd_backlight_init);

MODULE_DESCRIPTION("LM3630 Backlight Control");
MODULE_LICENSE("GPL");
