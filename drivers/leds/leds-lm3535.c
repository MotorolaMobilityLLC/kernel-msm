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
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>

/* Registers */
#define LM3535_DIODE_ENABLE_REG      0x10
#define LM3535_CFG_REG               0x20
#define LM3535_OPTIONS_REG           0x30
#define LM3535_BR_CONTROL_REG        0xA0
#define LM3535_DEV_ID_REG            0xFF

/* Enable Mode bits */
#define LM3535_MODE_STANDBY    0x00
#define LM3535_MODE_IR_DRV     BIT(0)
#define LM3535_MODE_TORCH      BIT(1)
#define LM3535_MODE_STROBE     (BIT(0) | BIT(1))
#define LM3535_STRB_EN         BIT(2)
#define LM3535_STRB_EDGE_TRIG  BIT(3)
#define LM3535_IVFM_EN         BIT(4)

#define LM36010_BOOST_LIMIT_28  BIT(5)
#define LM36010_BOOST_FREQ_4MHZ BIT(6)
#define LM36010_BOOST_MODE_PASS BIT(7)

#define LM3535_BANKA_EN1A BIT(0)
#define LM3535_BANKA_EN2A BIT(1)
#define LM3535_BANKA_EN3A BIT(2)
#define LM3535_BANKA_EN4A BIT(3)

#define LM3535_CONFIG_PWM_EN BIT(0)
#define LM3535_BRIGHTNESS_DEFAULT 0xF2

/**
 * struct LM3535_led -
 * @dev: device pointer
 * @client: Pointer to the I2C client
 * @regmap: Devices register map
 * @lock: Lock for reading/writing the device
 * @cdev_flash flash device to set brightness
 * @en_gpio: Enable GPIO
 * @brightness: Current brightness
 * @reg_addr: Current Register address
 * @reg_val: Register data value
 * @name: Label for the left or right or hat
 * @gpio_valid: Flag to denote gpio acquired
 */
struct lm3535_led {
	struct device *dev;
	struct i2c_client *client;
	struct regmap *regmap;
	struct mutex lock;
	struct led_classdev cdev_flash;

	int en_gpio;
	int brightness;
	int reg_addr;
	int reg_value;
	const char *name;
	bool gpio_valid;
};

static const struct regmap_config lm3535_regmap = {
	.reg_bits = 8,
	.val_bits = 8,

	.max_register = LM3535_DEV_ID_REG,
	.use_single_read = true,
	.use_single_write = true,
};

static ssize_t set_diode_enable(struct device *dev, int val)
{
	int ret = 0;
	int data = 0;
	struct lm3535_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s lm3535 data error\n", __func__);
		return -EFAULT;
	}

	/*
	 * Enable Register
	 *   Bits 7  6  5  4   3  2  1  0
	 *        0  0  0  0   1  1  1  1
	 * Bits 0 -  3 -- 0x0F - D1A to D14 GroupA
	 *      Bits 4-6 -   -0x00 - GroupB
	 *      Bit 7                   Group C
	 */
	dev_dbg(dev, "%s Writing LM3535_DIODE_ENABLE_REG:%x to val:%x\n", __func__,
				LM3535_DIODE_ENABLE_REG, val);
	ret = regmap_write(led->regmap, LM3535_DIODE_ENABLE_REG, val);

	if (ret < 0) {
		dev_err(dev,
			"Failed to write LM3535_DIODE_ENABLE_REG with ret:%d val:0x%x\n", ret, val);
		return ret;
	}

	ret = regmap_read(led->regmap, LM3535_DIODE_ENABLE_REG, &data);
	if (ret < 0)
		dev_err(dev, "%s Failed to read LM3535_DIODE_ENABLE_REG addr:%x\n", __func__,
			LM3535_DIODE_ENABLE_REG);
	else
		dev_dbg(dev, "%s Success read regmap, LM3535_DIODE_ENABLE_REG addr:%x, data:%x\n",
			__func__, LM3535_DIODE_ENABLE_REG, data);
	return ret;
}

static ssize_t set_configuration(struct  device *dev, int val)
{
	int ret = 0;
	struct lm3535_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led3535 data error\n", __func__);
		return -EFAULT;
	}

	/*
	 * Config Reg
	 *   Bits 7  6  5  4   3  2  1    0
	 *        0  0  0  0   0  0  0    1
	 *			         PWM PWM_EN
	 *			    Polarity(High is 0)
	 */
	dev_dbg(dev, "%s writing LM3535_CFG_REG to val:%d\n", __func__, val);
	ret = regmap_write(led->regmap, LM3535_CFG_REG, val);
	if (ret < 0) {
		dev_err(dev,
			"Failed to write LM3535_CFG_REG with val:%d : %d\n", val, ret);
		return ret;
	}

	ret = regmap_read(led->regmap, LM3535_CFG_REG, &val);
	if (ret < 0)
		dev_err(dev, "%s Failed to read LM3535_CFG_REG:%x\n", __func__,  LM3535_CFG_REG);
	else
		dev_dbg(dev, "%s Success read regmap, LM3535_CFG_REG:%x, val:%x\n",
					__func__, LM3535_CFG_REG, val);

	return ret;
}

static ssize_t brightness_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct lm3535_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led3535 data error\n", __func__);
		return -EFAULT;
	}

	dev_dbg(dev, "%s LM3535 brightness_val:%x\n", __func__, led->brightness);

	return scnprintf(buf, PAGE_SIZE, "%x\n", led->brightness);
}

static ssize_t lm3535_set_brightness_val(struct device *dev, u8 val)
{
	int ret = 0, data = 0;
	struct lm3535_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led3535 data error\n", __func__);
		return -EFAULT;
	}

	/*
	 * LED Flash Brightness Reg
	 * Brightness table from 0-6 bits
	 * 7th bit always 1
	 * 51.673% = 0xF2
	 */

	led->brightness = val;
	dev_dbg(dev, "%s writing LM3535_BR_CONTROL_REG:%x to val:%x\n",
				__func__, LM3535_BR_CONTROL_REG, val);
	ret = regmap_write(led->regmap, LM3535_BR_CONTROL_REG, val);
	if (ret < 0) {
		dev_err(dev,
		"Failed to write LM3535_BR_CONTROL_REG with value:%x :%d\n", val, ret);
		return ret;
	}

	ret = regmap_read(led->regmap, LM3535_BR_CONTROL_REG, &data);
	if (ret < 0)
		dev_err(dev, "%s Failed to read LM3535_BR_CONTROL_REG:%x\n",
				__func__,  LM3535_BR_CONTROL_REG);
	else
		dev_dbg(dev, "%s Success read regmap, LM3535_BR_CONTROL_REG:%x, data:%x\n",
			__func__, LM3535_BR_CONTROL_REG, data);

	return ret;
}

static ssize_t brightness_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	int val = 0, ret = 0;
	struct lm3535_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led3535 data error\n", __func__);
		return -EFAULT;
	}

	ret = sscanf(buf, "%d\n", &val);
	if (ret < 0) {
		dev_err(dev, "LM3535 write setbrightness error\n");
		return ret;
	}

	dev_dbg(dev, "lm3535 setbrightness store %s value:%d\n", __func__, val);
	mutex_lock(&led->lock);

	lm3535_set_brightness_val(dev, val);
	mutex_unlock(&led->lock);

	return count;
}

static DEVICE_ATTR_RW(brightness);

static struct attribute *lm3535_attrs[] = {
	&dev_attr_brightness.attr,
	NULL,
};

static struct attribute_group lm3535_attribute_group = {
	.name = "lm3535",
	.attrs = lm3535_attrs,
};

/* flash */
static int lm3535_brightness_set(struct led_classdev *cdev,
						enum led_brightness brightness)
{
	struct lm3535_led *led =
			container_of(cdev, struct lm3535_led, cdev_flash);

	return lm3535_set_brightness_val(led->dev, brightness);
}


static int lm3535_parse_node(struct lm3535_led *led)
{
	int ret = -ENODEV;
	struct fwnode_handle *child = NULL;

	led->en_gpio = of_get_named_gpio(led->dev->of_node, "drv-en-gpio", 0);

	if (!gpio_is_valid(led->en_gpio)) {
		dev_err(&led->client->dev, "Fatal en_gpio is not valid\n");
		return ret;
	}

	ret = devm_gpio_request_one(&led->client->dev, led->en_gpio, GPIOF_OUT_INIT_HIGH,
				"lm3535-hwen");
	if (ret) {
		dev_warn(&led->client->dev,
			"drv-en-gpio not able to acquire, already acquire, ignore it\n");
		ret = 0;
		led->gpio_valid = false;
	} else {
		gpio_set_value_cansleep(led->en_gpio, 1);
		led->gpio_valid = true;
	}

	led->name = of_get_property(led->dev->of_node, "label", NULL);
	if (!led->name) {
		dev_err(&led->client->dev, "%s name node not found..\n", __func__);
		return -ENODEV;
	}

	child = device_get_next_child_node(&led->client->dev, child);
	if (!child) {
		dev_err(&led->client->dev, "No LED Child node\n");
		return ret;
	}

	ret = fwnode_property_read_string(child, "qcom,led-name",
		&led->cdev_flash.name);
	if (ret < 0) {
		dev_err(&led->client->dev, "Failed to read flash LED names\n");
		return ret;
	}

	ret = fwnode_property_read_u32(child, "qcom,max-current-ma",
				&led->cdev_flash.max_brightness);
	if (ret < 0) {
		dev_err(&led->client->dev, "Failed to read max current, rc=%d\n", ret);
		return ret;
	}

	ret = fwnode_property_read_string(child, "qcom,default-led-trigger",
		&led->cdev_flash.default_trigger);
	if (ret < 0) {
		dev_err(&led->client->dev, "Failed to read default_trigger, rc=%d\n", ret);
		return ret;
	}

	return ret;
}

static int lm3535_probe(struct i2c_client *client, const struct i2c_device_id *id)
{
	struct device *dev = &client->dev;
	struct lm3535_led *led;
	int ret;

	led = devm_kzalloc(&client->dev, sizeof(*led), GFP_KERNEL);
	if (!led)
		return -ENOMEM;

	led->dev = dev;
	led->client = client;
	i2c_set_clientdata(client, led);

	ret = lm3535_parse_node(led);
	if (ret) {
		dev_err(&client->dev, "LM3535_parse_node failed exiting %s ...\n", __func__);
		return -ENODEV;
	}

	led->regmap = devm_regmap_init_i2c(client, &lm3535_regmap);
	if (IS_ERR(led->regmap)) {
		ret = PTR_ERR(led->regmap);
		dev_err(&client->dev,
			"Failed to allocate register map: %d\n", ret);
		return ret;
	}

	/* flash */
	led->cdev_flash.brightness_set_blocking = lm3535_brightness_set;
	ret = led_classdev_register(&client->dev, &led->cdev_flash);
	if (ret < 0)
		return ret;

	if (devm_device_add_group(led->dev, &lm3535_attribute_group)) {
		dev_err(led->dev, "%s  failed to create sysfs group\n", __func__);
		led_classdev_unregister(&led->cdev_flash);
		return -ENOMEM;
	}

	set_diode_enable(dev,
		LM3535_BANKA_EN4A | LM3535_BANKA_EN3A | LM3535_BANKA_EN2A | LM3535_BANKA_EN1A);
	set_configuration(dev, LM3535_CONFIG_PWM_EN);
	lm3535_set_brightness_val(dev, 0);

	mutex_init(&led->lock);

	return 0;
}

static void lm3535_remove(struct i2c_client *client)
{
	struct lm3535_led *led = (struct lm3535_led *)i2c_get_clientdata(client);

	if (!led)
		return;

	if (led->gpio_valid) {
		gpio_set_value_cansleep(led->en_gpio, 0);
		gpio_free(led->en_gpio);
	}
	led_classdev_unregister(&led->cdev_flash);

	mutex_destroy(&led->lock);
}

static int lm3535_pm_suspend(struct device *dev)
{
	struct lm3535_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led nullptr error\n", __func__);
		return -EFAULT;
	}

	set_diode_enable(dev, 0x0);
	set_configuration(dev, LM3535_CONFIG_PWM_EN);
	lm3535_set_brightness_val(dev, 0x0);

	return 0;
}

static int lm3535_pm_resume(struct device *dev)
{
	struct lm3535_led *led = dev_get_drvdata(dev);

	if (!led) {
		dev_err(dev, "%s led nullptr error\n", __func__);
		return -EFAULT;
	}

	set_diode_enable(dev, LM3535_BANKA_EN4A | LM3535_BANKA_EN3A |
			LM3535_BANKA_EN2A | LM3535_BANKA_EN1A);
	set_configuration(dev, LM3535_CONFIG_PWM_EN);
	/* Not setting the brightness */

	return 0;
}

static const struct dev_pm_ops lm3535_pm_ops = {
	.suspend = lm3535_pm_suspend,
	.resume = lm3535_pm_resume,
};

static const struct i2c_device_id lm3535_i2c_id[] = {
	{"LM3535"},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm3535_i2c_id);

static const struct of_device_id of_lm3535_leds_match[] = {
	{ .compatible = "ti,lm3535" },
	{ },
};
MODULE_DEVICE_TABLE(of, of_lm3535_leds_match);

static struct i2c_driver lm3535_i2c_driver = {
	.driver = {
		.name = "LM3535",
		.of_match_table = of_match_ptr(of_lm3535_leds_match),
		.pm = &lm3535_pm_ops,
	},
	.id_table = lm3535_i2c_id,
	.probe    = lm3535_probe,
	.remove   = lm3535_remove,
};
module_i2c_driver(lm3535_i2c_driver);

MODULE_DESCRIPTION("Texas Instruments Flash Lighting driver for LM3535");
MODULE_LICENSE("GPL");
