// SPDX-License-Identifier: GPL-2.0-only
/*
 * Copyright (c) 2025 Qualcomm Innovation Center, Inc. All rights reserved.
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/led-class-flash.h>
#include <linux/module.h>
#include <linux/regmap.h>
#include <linux/slab.h>

#define LM36011_LED_IR          0x0

/* Registers */
#define LM36011_ENABLE_REG      0x01
#define LM36011_CFG_REG         0x02
#define LM36011_LED_FLASH_REG   0x03
#define LM36011_LED_TORCH_REG   0x04
#define LM36011_DEV_ID_REG      0x06

/* Enable Mode bits */
#define LM36011_MODE_STANDBY    0x00
#define LM36011_MODE_IR_DRV     BIT(0)
#define LM36011_MODE_TORCH      BIT(1)
#define LM36011_STRB_EN         BIT(2)
#define LM36011_IVFM_EN         BIT(4)

#define FLASH_TIME_DURATION_120 BIT(2)
#define FLASH_TIME_DURATION_360 BIT(4)

#define LED_FLASH_BRIGHT_11ma BIT(0)
#define LED_FLASH_BRIGHT_88ma BIT(3)
#define LED_FLASH_BRIGHT_140ma BIT(4)

#define LM36011_ENABLE_MASK     (LM36011_MODE_IR_DRV | LM36011_MODE_TORCH)

/**
 * struct LM36011_led -
 * @dev: Pointer to the device
 * @client: Pointer to the I2C client
 * @regmap: Devices register map
 * @lock: Lock for reading/writing the device
 * @cdev_flash flash device to set brightness
 * @torch_current_max: maximum current for the torch
 * @flash_current_max: maximum current for the flash
 * @max_flash_timeout: maximum timeout for the flash
 * @led_mode: The mode to IR mode
 * @brightness: The current brightness value
 * @name: The name of the node left or right
 */
struct lm36011_led {
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct mutex lock;
	struct led_classdev cdev_flash;

	u32 flash_volt_max;
	u32 flash_current_max;
	u32 max_flash_timeout;
	u32 led_mode;
	int brightness;
	const char *name;
};

static const struct regmap_config lm36011_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = LM36011_DEV_ID_REG,
	.use_single_read = true,
	.use_single_write = true,
};

static int lm36011_brightness_reg(struct  device *dev, int val)
{
	int ret = 0, data = 0;
	struct lm36011_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led36011 data error\n", __func__);
		return -EFAULT;
	}

	/*
	 * LED Flash Brightness Reg
	 *   Bits 7  6  5  4   3  2  1  0
	 *        0  0  0  1   1  0  0  1
	 * Bit 0-6 LED Flash Brightness level
	 * Bit 7 - Thermal Current scale-back
	 * 0x19 ->>> 0.300
	 */
	dev_dbg(dev, "%s writing LM36011_LED_FLASH_REG:%x to val:%x\n",
			__func__, LM36011_LED_FLASH_REG, val);
	ret = regmap_write(led->regmap, LM36011_LED_FLASH_REG, val);
	if (ret < 0) {
		dev_err(dev,
			"Failed to write LM36011_LED_FLASH_REG with val:%x %d\n", val, ret);
		return ret;
	}

	ret = regmap_read(led->regmap, LM36011_LED_FLASH_REG, &data);
	if (ret < 0)
		dev_err(dev, "%s failed to read LM36011_LED_FLASH_REG:%x\n",
				__func__, LM36011_LED_FLASH_REG);
	else {
		dev_dbg(dev, "%s success read LM36011_LED_FLASH_REG:%x data:%x\n",
			__func__, LM36011_LED_FLASH_REG, data);
		led->brightness = data;
	}

	return ret;
}

static int lm36011_config_reg(struct  device *dev, int val)
{
	int ret = 0, data = 0;
	struct lm36011_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led36011 data error\n", __func__);
		return -EFAULT;
	}

	/*
	 * Config Reg
	 *   Bits 7  6  5  4   3  2  1  0
	 *        0  0  0  1   0  1  0  0
	 * Bit 0 - Torch Ramp 0 -- no Ramp
	 * Bits 1-4 Flash TimeOut Duration, 1010 - 600ms(default)
	 * Bits 7 -5 IVFM Levels - 000 2.9V. This is disabled
	 */
	dev_dbg(dev, "%s writing LM36011_CFG_REG:%x to 0x14 val:%x\n",
					__func__, LM36011_CFG_REG, val);
	ret = regmap_write(led->regmap, LM36011_CFG_REG, val);
	if (ret < 0) {
		dev_err(dev,
			"Failed to write LM36011_CFG_REG with 0x14: %d\n", ret);
		return ret;
	}

	ret = regmap_read(led->regmap, LM36011_CFG_REG, &data);
	if (ret < 0)
		dev_err(dev, "%s failed to read LM36011_CFG_REG:%x\n", __func__, LM36011_CFG_REG);
	else
		dev_dbg(dev, "%s success read LM36011_CFG_REG:%x, data:%x\n", __func__,
				LM36011_CFG_REG, data);

	return ret;
}

static int lm36011_enable_reg(struct  device *dev, int val)
{
	int ret = 0, data = 0;
	struct lm36011_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s lm36011 data error\n", __func__);
		return -EFAULT;
	}

	/*
	 * Enable Register
	 *   Bits 7  6  5  4   3  2  1  0
	 *        *  *  1  0   0  1  0  1
	 * Bits 0 & 1 -- 01 - IR Drive
	 * Bit 2 0 Strobe Enable - 1
	 * Bit 3 0 - Level Triffered.
	 * Bit 4 0 - IVFM Disabled
	 */
	dev_dbg(dev, "%s, writing LM36011_ENABLE_REG:%x to value:%x 0x25\n",
			__func__, LM36011_ENABLE_REG, val);
	ret = regmap_write(led->regmap, LM36011_ENABLE_REG, val);

	if (ret < 0) {
		dev_err(dev, "Failed to write LM36011_ENABLE_REG with 0x25: %d\n", ret);
		return ret;
	}

	ret = regmap_read(led->regmap, LM36011_ENABLE_REG, &data);
	if (ret < 0)
		dev_err(dev, "%s failed to read LM36011_ENABLE_REG:%x\n",
			__func__, LM36011_ENABLE_REG);
	else
		dev_dbg(dev, "%s success to read LM36011_ENABLE_REG:%x data:%x\n",
			__func__, LM36011_ENABLE_REG, data);

	return ret;
}

static ssize_t brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lm36011_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led36011 data error\n", __func__);
		return -EFAULT;
	}

	dev_err(dev, "%s LM36011 brightness_val:%x\n", __func__, led->brightness);

	return scnprintf(buf, PAGE_SIZE, "%x\n", led->brightness);
}

static ssize_t brightness_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0, ret = 0;
	struct lm36011_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led36011 data error\n", __func__);
		return -EFAULT;
	}

	ret = sscanf(buf, "%d\n", &val);
	if (ret < 0) {
		dev_err(dev, "LM36011 write setbrightness error\n");
		return ret;
	}

	dev_dbg(dev, "lm36011 setbrightness store %s value:%d\n", __func__, val);

	mutex_lock(&led->lock);
	lm36011_brightness_reg(dev, val);
	mutex_unlock(&led->lock);

	return count;
}

static DEVICE_ATTR_RW(brightness);

static struct attribute *lm36011_attrs[] = {
	&dev_attr_brightness.attr,
	NULL,
};

static struct attribute_group lm36011_attribute_group = {
	.name = "lm36011",
	.attrs = lm36011_attrs,
};

/* flash */
static int lm36011_brightness_set(struct led_classdev *cdev,
					enum led_brightness brightness)
{
	struct lm36011_led *led = container_of(cdev, struct lm36011_led, cdev_flash);
	int ret;

	mutex_lock(&led->lock);
	ret = lm36011_brightness_reg(led->dev, brightness);
	mutex_unlock(&led->lock);
	return ret;
}


static int lm36011_parse_node(struct lm36011_led *led)
{
	int ret = 0;

	ret = of_property_read_u32(led->dev->of_node, "mode", &led->led_mode);
	if (ret) {
		dev_err(&led->client->dev, "mode DT property missing\n");
		goto out_err;
	}

	if (led->led_mode != LM36011_LED_IR) {
		dev_err(&led->client->dev, "Invalid led mode requested\n");
		ret = -EINVAL;
		goto out_err;
	}

	ret = of_property_read_u32(led->dev->of_node, "led-max-microamp",
			&led->flash_current_max);
	if (ret) {
		dev_err(&led->client->dev, "led-max-microamp DT property missing\n");
		goto out_err;
	}

	dev_dbg(&led->client->dev, "%s led-max-microamp:%x\n", __func__, led->flash_current_max);

	ret = of_property_read_u32(led->dev->of_node, "led-ivfm-max-microvolt",
			&led->flash_volt_max);
	if (ret) {
		dev_err(&led->client->dev, "led-ivfm-max-microvolt DT property missing\n");
		goto out_err;
	}

	dev_dbg(&led->client->dev, "%s led-ivfm-max-microvolt:%x\n", __func__, led->flash_volt_max);
	ret = of_property_read_u32(led->dev->of_node, "led-max-timeout-us",
			&led->max_flash_timeout);
	if (ret) {
		dev_err(&led->client->dev, "flash-max-timeout-us DT property missing\n");
		goto out_err;
	}

	dev_dbg(&led->client->dev, "%s led-max-timeout-us:%x\n", __func__, led->max_flash_timeout);

	led->name = of_get_property(led->dev->of_node, "label", NULL);
	if (!led->name) {
		dev_err(&led->client->dev, "%s name node not found..\n", __func__);
		return -ENODEV;
	}

out_err:
	return ret;
}

static int lm36011_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct lm36011_led *led;
	int ret;

	led = devm_kzalloc(&client->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	led->dev = dev;
	led->client = client;
	i2c_set_clientdata(client, led);
	mutex_init(&led->lock);

	ret = lm36011_parse_node(led);
	if (ret) {
		dev_err(led->dev, "LM36011_parse_node DTS failed\n");
		return ret;
	}

	led->regmap = devm_regmap_init_i2c(client, &lm36011_regmap);
	if (IS_ERR(led->regmap)) {
		ret = PTR_ERR(led->regmap);
		dev_err(&client->dev, "Failed to allocate register map: %d\n", ret);
		return ret;
	}

	if (lm36011_enable_reg(dev,
			((LM36011_IVFM_EN << 0)|LM36011_STRB_EN | LM36011_MODE_IR_DRV)) < 0) {
		dev_err(&client->dev, "Failed to write LM36011_EN_REG with 0x25: %d\n", ret);
		return ret;
	}

	if (lm36011_config_reg(dev,
			((FLASH_TIME_DURATION_360 << 0) | FLASH_TIME_DURATION_120)) < 0) {
		dev_err(&client->dev, "Failed to write LM36011_CONFIG_REG with 0x14: %d\n", ret);
		return ret;
	}

	if (lm36011_brightness_reg(dev,
			((LED_FLASH_BRIGHT_140ma << 0) | LED_FLASH_BRIGHT_88ma |
			LED_FLASH_BRIGHT_11ma)) < 0) {
		dev_err(&client->dev, "Failed to write LM36011_LED_REG with 0x19: %d\n", ret);
		return ret;
	}

	/* flash */
	led->cdev_flash.name = "leds-lm36011";
	led->cdev_flash.max_brightness = 255;
	led->cdev_flash.brightness_set_blocking = lm36011_brightness_set;
	led->cdev_flash.default_trigger = "flash";
	ret = led_classdev_register(&client->dev, &led->cdev_flash);
	if (ret < 0)
		return ret;


	if (devm_device_add_group(led->dev, &lm36011_attribute_group)) {
		dev_err(led->dev, "failed to create sysfs group\n");
		return -ENOMEM;
	}

	return 0;
}

static void lm36011_remove(struct i2c_client *client)
{
	struct lm36011_led *led = i2c_get_clientdata(client);
	int ret;

	if (!led)
		return;

	ret = regmap_update_bits(led->regmap, LM36011_ENABLE_REG,
		LM36011_ENABLE_MASK, LM36011_MODE_STANDBY);
	if (ret)
		dev_err(&client->dev,
			"Failed to put into standby (%pe)\n", ERR_PTR(ret));
}

static int lm36011_pm_suspend(struct device *dev)
{
	struct lm36011_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led nullptr\n", __func__);
		return -EINVAL;
	}

	lm36011_enable_reg(dev, 0x0);
	lm36011_brightness_reg(dev, 0x0);

	return 0;
}

static int lm36011_pm_resume(struct device *dev)
{
	struct lm36011_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led nullptr\n", __func__);
		return -EINVAL;
	}

	lm36011_enable_reg(dev, ((LM36011_IVFM_EN << 0) | LM36011_STRB_EN | LM36011_MODE_IR_DRV));
	lm36011_config_reg(dev, ((FLASH_TIME_DURATION_360 << 0) | FLASH_TIME_DURATION_120));
	lm36011_brightness_reg(dev,
		(LED_FLASH_BRIGHT_140ma << 0) | LED_FLASH_BRIGHT_88ma | LED_FLASH_BRIGHT_11ma);

	return 0;
}

static const struct dev_pm_ops lm36011_pm_ops = {
	.suspend = lm36011_pm_suspend,
	.resume = lm36011_pm_resume,
};

static const struct i2c_device_id lm36011_i2c_id[] = {
	{"LM36011"},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm36011_i2c_id);

static const struct of_device_id of_lm36011_leds_match[] = {
	{ .compatible = "ti,lm36011", },
	{ },
};
MODULE_DEVICE_TABLE(of, of_lm36011_leds_match);

static struct i2c_driver lm36011_i2c_driver = {
	.driver = {
		.name = "LM36011",
		.of_match_table = of_match_ptr(of_lm36011_leds_match),
		.pm = &lm36011_pm_ops,
	},
	.id_table = lm36011_i2c_id,
	.probe = lm36011_probe,
	.remove = lm36011_remove,
};
module_i2c_driver(lm36011_i2c_driver);

MODULE_DESCRIPTION("Texas Instruments Flash Lighting driver for LM36011");
MODULE_LICENSE("GPL");
