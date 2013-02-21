/*
 * Copyright (C) 2010-2011 Motorola Mobility, Inc.
 * Copyright (C) 2012-2013 Motorola Mobility LLC
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#include <linux/delay.h>
#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/backlight.h>
#include <linux/platform_device.h>
#include <linux/lp8556.h>
#include <linux/module.h>
#include <linux/moduleparam.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/earlysuspend.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/gpio.h>

#define LP8556_VDD_MIN_VOLTAGE 1620000
#define LP8556_VDD_MAX_VOLTAGE 3600000
#define LP8556_REGULATOR_NAME "vddio"
#define LP8556_I2C_ADDRESS	0x2C
#define LP8556_CHIP_ID		0xFC

#define LP8556_MAX_RW_RETRIES 5
#define LP8556_I2C_RETRY_DELAY 10

#define LP8556_BRT_CTRL_REG	0x00
#define LP8556_DEVICE_CTRL_REG	0x01
#define LP8556_STATUS_REG	0x02
#define LP8556_CHIP_ID_REG	0x03
#define LP8556_DIRECT_CTRL_REG	0x04
#define LP8556_TEMP_MSB_REG	0x05
#define LP8556_TEMP_LSB_REG	0x06

#define LP8556_DEVICE_CTRL_FAST_BIT	0x01

/* EEPROM Register address */
#define LP8556_EEPROM_CTRL	0x72
#define LP8556_EEPROM_98	0x98
#define LP8556_EEPROM_9E	0x9E
#define LP8556_EEPROM_A0	0xa0
#define LP8556_EEPROM_A1	0xa1
#define LP8556_EEPROM_A2	0xa2
#define LP8556_EEPROM_A3	0xa3
#define LP8556_EEPROM_A4	0xa4
#define LP8556_EEPROM_A5	0xa5
#define LP8556_EEPROM_A6	0xa6
#define LP8556_EEPROM_A7	0xa7
#define LP8556_EEPROM_A8	0xa8
#define LP8556_EEPROM_A9	0xa9
#define LP8556_EEPROM_AA	0xaA
#define LP8556_EEPROM_AB	0xaB
#define LP8556_EEPROM_AC	0xaC
#define LP8556_EEPROM_AD	0xaD
#define LP8556_EEPROM_AE	0xaE
#define LP8556_EEPROM_AF	0xaF

#define LP8556_MAX_BRIGHTNESS	255

#define LCD_BACKLIGHT "lcd-backlight"

static const unsigned short normal_i2c[] = { LP8556_I2C_ADDRESS,
					     I2C_CLIENT_END };

struct lp8556_data {
	struct led_classdev led_dev;
	struct backlight_device *bl_dev;
	struct i2c_client *client;
	struct work_struct work;
	struct lp8556_platform_data *pdata;
	int version;
	atomic_t enabled;
	bool suspended;
	struct regulator *regulator;
#ifdef CONFIG_HAS_EARLYSUSPEND
	struct early_suspend early_suspender;
#endif
};

#define DEBUG 1
#ifdef DEBUG
struct lp8556_reg {
	const char *name;
	uint8_t reg;
} lp8556_regs[] = {
	{ "BRT_CTRL",		LP8556_BRT_CTRL_REG },
	{ "DEV_CTRL",		LP8556_DEVICE_CTRL_REG },
	{ "STATUS",		LP8556_STATUS_REG },
	{ "CHIP_ID",		LP8556_CHIP_ID },
	{ "DIRECT_CTRL",	LP8556_DIRECT_CTRL_REG },
	{ "TEMP_MSB",		LP8556_TEMP_MSB_REG },
	{ "TEMP_LSB",		LP8556_TEMP_LSB_REG },
	{ "EEPROM_98",		LP8556_EEPROM_98 },
	{ "EEPROM_9E",		LP8556_EEPROM_9E },
	{ "EEPROM_A0",		LP8556_EEPROM_A0 },
	{ "EEPROM_A1",		LP8556_EEPROM_A1 },
	{ "EEPROM_A2",		LP8556_EEPROM_A2 },
	{ "EEPROM_A3",		LP8556_EEPROM_A3 },
	{ "EEPROM_A4",		LP8556_EEPROM_A4 },
	{ "EEPROM_A5",		LP8556_EEPROM_A5 },
	{ "EEPROM_A6",		LP8556_EEPROM_A6 },
	{ "EEPROM_A7",		LP8556_EEPROM_A7 },
	{ "EEPROM_A8",		LP8556_EEPROM_A8 },
	{ "EEPROM_A9",		LP8556_EEPROM_A9 },
	{ "EEPROM_AA",		LP8556_EEPROM_AA },
	{ "EEPROM_AB",		LP8556_EEPROM_AB },
	{ "EEPROM_AC",		LP8556_EEPROM_AC },
	{ "EEPROM_AD",		LP8556_EEPROM_AD },
	{ "EEPROM_AE",		LP8556_EEPROM_AE },
	{ "EEPROM_AF",		LP8556_EEPROM_AF }
};
#endif

static void lp8556_brightness_write(struct lp8556_data *led_data);

static void lp8556_flash_eeprom(struct lp8556_data *led_data)
{
	uint8_t i = 0;
	int error = 0;

	for (i = 0; i < led_data->pdata->eeprom_tbl_sz; i++) {
		if (led_data->pdata->eeprom_table[i].flag == 1) {
			error = i2c_smbus_write_byte_data(led_data->client,
				  led_data->pdata->eeprom_table[i].addr,
				  led_data->pdata->eeprom_table[i].data);

			if (error < 0)
				pr_err("%s:  Register-0x%02X"\
				       "init failed\n", __func__,
				       led_data->pdata->eeprom_table[i].addr);
		}
	}
}

static int __devinit lp8556_init_registers(struct lp8556_data *led_data)
{
	uint8_t value = 0;
	int error;

	error = i2c_smbus_read_i2c_block_data(led_data->client,
					      LP8556_CHIP_ID_REG, 1, &value);
	if (error < 0)
		return -ENODEV;

	led_data->version = value;
	dev_info(&led_data->client->dev, "%s: Found LP8556 ES%d device.\n",
		__func__, led_data->version);

	lp8556_flash_eeprom(led_data);

	if (i2c_smbus_write_byte_data(led_data->client,
				      LP8556_BRT_CTRL_REG,
				      led_data->pdata->power_up_brightness)) {
		pr_err("%s: Register %d initialization failed\n", __func__,
			LP8556_BRT_CTRL_REG);
		return -EINVAL;
	}

	if (i2c_smbus_write_byte_data(led_data->client,
				      LP8556_DEVICE_CTRL_REG,
				      led_data->pdata->dev_ctrl_config |
				      LP8556_DEVICE_CTRL_FAST_BIT)) {
		pr_err("%s: Register %d initialization failed\n", __func__,
			LP8556_DEVICE_CTRL_REG);
		return -EINVAL;
	}

	if (i2c_smbus_write_byte_data(led_data->client,
				      LP8556_DIRECT_CTRL_REG,
				      led_data->pdata->direct_ctrl)) {
		pr_err("%s: Register %d initialization failed\n", __func__,
			LP8556_DIRECT_CTRL_REG);
		return -EINVAL;
	}

	/* clear to get status */
	error = i2c_smbus_read_i2c_block_data(led_data->client,
					      LP8556_STATUS_REG, 1, &value);
	if ((error < 0) ||  (value != 0x30)) {
		pr_err("%s: LP8556 status error - 0x%X, 0x%X\n",
		       __func__, value, error);
		return -EINVAL;
	}

	return 0;
}

void lp8556_brightness_set(struct led_classdev *led_dev,
			   enum led_brightness brightness)
{
	struct lp8556_data *led_data =
		container_of(led_dev, struct lp8556_data, led_dev);

	pr_debug("%s:\n", __func__);
	if (brightness > LP8556_MAX_BRIGHTNESS)
		brightness = LP8556_MAX_BRIGHTNESS;

	led_data->led_dev.brightness = brightness;
	led_data->bl_dev->props.brightness = brightness;

	pr_debug("%s: %d\n", __func__, brightness);

	if (!led_data->suspended)
		queue_work(system_nrt_wq, &led_data->work);
}
EXPORT_SYMBOL(lp8556_brightness_set);

static int lp8556_update_brightness(struct backlight_device *bl_dev)
{
	struct lp8556_data *led_data = bl_get_data(bl_dev);

	pr_debug("%s: %d\n", __func__, bl_dev->props.brightness);

	led_data->led_dev.brightness = bl_dev->props.brightness;
	if (!led_data->suspended)
		queue_work(system_nrt_wq, &led_data->work);

	return 0;
}

static int lp8556_get_brightness(struct backlight_device *bl_dev)
{
	struct lp8556_data *led_data = bl_get_data(bl_dev);
	pr_debug("%s: %d\n", __func__, led_data->led_dev.brightness);
	return led_data->led_dev.brightness;
}

static const struct backlight_ops lp8556_bl_ops = {
	.update_status	= lp8556_update_brightness,
	.get_brightness	= lp8556_get_brightness,
};

static void lp8556_brightness_work(struct work_struct *work)
{
	struct lp8556_data *led_data =
		container_of(work, struct lp8556_data, work);

	lp8556_brightness_write(led_data);
}

static void lp8556_enable(struct lp8556_data *led_data, bool enable)
{
	if (gpio_is_valid(led_data->pdata->enable_gpio)) {
		gpio_set_value(led_data->pdata->enable_gpio, enable ? 1 : 0);
		if (enable)
			usleep_range(400, 500);
	}
}

static void lp8556_brightness_write(struct lp8556_data *led_data)
{
	int error = 0;
	int brightness = led_data->led_dev.brightness;

	pr_debug("%s: Setting brightness to %i\n", __func__, brightness);

	if (brightness == LED_OFF) {
		if (atomic_cmpxchg(&led_data->enabled, 1, 0)) {
			if (i2c_smbus_write_byte_data(led_data->client,
						      LP8556_BRT_CTRL_REG, 0))
				pr_err("%s: Failed to set brightness: 0\n",
					__func__);
			if (!IS_ERR(led_data->regulator)) {
				regulator_disable(led_data->regulator);
			        pr_info("%s regulator disabled\n", __func__);
			}
			lp8556_enable(led_data, false);
		}
	} else {
		if (!atomic_cmpxchg(&led_data->enabled, 0, 1)) {
			if (!IS_ERR(led_data->regulator)) {
				regulator_enable(led_data->regulator);
				pr_info("%s regulator enabled\n", __func__);
			}

			lp8556_enable(led_data, true);
			lp8556_flash_eeprom(led_data);

			if (i2c_smbus_write_byte_data(led_data->client,
						      LP8556_BRT_CTRL_REG,
						      brightness))
				pr_err("%s: Failed to set brightness:%d\n",
					__func__, error);

			if (i2c_smbus_write_byte_data(led_data->client,
					LP8556_DEVICE_CTRL_REG,
					led_data->pdata->dev_ctrl_config |
					LP8556_DEVICE_CTRL_FAST_BIT))
				pr_err("%s: Failed to setup config:%d\n",
					__func__, error);
		} else
			if (i2c_smbus_write_byte_data(led_data->client,
						      LP8556_BRT_CTRL_REG,
						      brightness))
				pr_err("%s: Failed to set brightness:%d\n",
					__func__, error);
	}
}

#ifdef DEBUG
static ssize_t lp8556_registers_show(struct device *dev,
			      struct device_attribute *attr, char *buf)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client,
						 dev);
	struct lp8556_data *led_data = i2c_get_clientdata(client);
	unsigned i, n, reg_count;
	uint8_t value = 0;
	int error;

	reg_count = sizeof(lp8556_regs) / sizeof(lp8556_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		error = i2c_smbus_read_i2c_block_data(led_data->client,
						      lp8556_regs[i].reg,
						      1, &value);
		if (error < 0) {
			pr_err("%s: i2c_smbus_read_i2c_block_data: %d\n",
				__func__, error);
			return -EINVAL;
		}
		n += scnprintf(buf + n, PAGE_SIZE - n,
			       "%-20s = 0x%02X\n",
			       lp8556_regs[i].name,
			       value);
	}

	return n;
}

static ssize_t lp8556_registers_store(struct device *dev,
			       struct device_attribute *attr,
			       const char *buf, size_t count)
{
	struct i2c_client *client = container_of(dev->parent, struct i2c_client,
						 dev);
	struct lp8556_data *led_data = i2c_get_clientdata(client);
	unsigned i, reg_count, value;
	int error;
	char name[30];

	if (count >= 30) {
		pr_err("%s: input too long\n", __func__);
		return -EINVAL;
	}

	if (sscanf(buf, "%s %x", name, &value) != 2) {
		pr_err("%s: unable to parse input\n", __func__);
		return -EINVAL;
	}

	reg_count = sizeof(lp8556_regs) / sizeof(lp8556_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strncmp(name, lp8556_regs[i].name, 30)) {
			error = i2c_smbus_write_byte_data(led_data->client,
							  lp8556_regs[i].reg,
							  value);
			if (error) {
				pr_err("%s: Failed to write register \"%s\"\n",
					__func__, name);
				return -EINVAL;
			}
			return count;
		}
	}

	pr_err("%s: no such register \"%s\"\n", __func__, name);
	return -EINVAL;
}
static DEVICE_ATTR(registers, 0644, lp8556_registers_show,
		lp8556_registers_store);
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lp8556_early_suspend(struct early_suspend *h)
{
	struct lp8556_data *led_data =
		container_of(h, struct lp8556_data, early_suspender);
	enum led_brightness cur_brightness = led_data->led_dev.brightness;

	led_data->suspended = true;

	cancel_work_sync(&led_data->work);
	led_data->led_dev.brightness = LED_OFF;
	lp8556_brightness_write(led_data);
	led_data->led_dev.brightness = cur_brightness;
}

static void lp8556_late_resume(struct early_suspend *h)
{
	struct lp8556_data *led_data =
		container_of(h, struct lp8556_data, early_suspender);
	led_data->suspended = false;
	queue_work(system_nrt_wq, &led_data->work);
}
#endif

static int lp8556_gpio_init(struct lp8556_platform_data *pdata)
{
	int ret;

	ret = gpio_request(pdata->enable_gpio, "lp8556_enable");
	if (ret) {
		pr_err("%s: error requesting enable_gpio\n", __func__);
		return ret;
	}

	gpio_direction_output(pdata->enable_gpio, 1);
	ret = gpio_export(pdata->enable_gpio, 0);
	if (ret) {
		pr_err("%s: failed to export enable_gpio\n", __func__);
		goto free_gpio;
	}

	return 0;

free_gpio:
	gpio_free(pdata->enable_gpio);

	return ret;
}

#ifdef CONFIG_OF
static struct lp8556_platform_data *
lp8556_of_init(struct i2c_client *client)
{
	struct lp8556_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	int *buf;
	int temp;
	int ret;
	int len;
	const void *prop;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "pdata alloc failure\n");
		return NULL;
	}

	ret = of_property_read_u32(np, "ti,lp8556-initial-brightness", &temp);
	if (!ret)
		pdata->power_up_brightness = (u8)temp;

	ret = of_property_read_u32(np, "ti,lp8556-dev-ctrl-config", &temp);
	if (!ret)
		pdata->dev_ctrl_config = (u8)temp;

	ret = of_property_read_u32(np, "ti,lp8556-direct-ctrl", &temp);
	if (!ret)
		pdata->direct_ctrl = (u8)temp;

	prop = of_get_property(np, "ti,lp8556-eeprom-settings", &len);
	if (prop) {
		int size;
		int i;

		/* Firmware should be pairs of register / value entries */
		if (!len || (len % (sizeof(int)*2))) {
			dev_err(&client->dev, "eeprom entry invalid\n");
			return NULL;
		}

		/* Populate the number of entries, and allocate memory to hold
		 * all of the 'lp8556_eeprom_data' entries.
		 */
		pdata->eeprom_tbl_sz = len / (sizeof(int)*2);
		size = pdata->eeprom_tbl_sz * sizeof(struct lp8556_eeprom_data);

		pdata->eeprom_table = devm_kzalloc(&client->dev, size,
							GFP_KERNEL);
		if (!pdata->eeprom_table) {
			dev_err(&client->dev, "eeprom table alloc failure\n");
			return NULL;
		}

		buf = kzalloc(len, GFP_KERNEL);
		if (!buf) {
			dev_err(&client->dev, "temp buffer alloc failure\n");
			return NULL;
		}

		ret = of_property_read_u32_array(np,
						"ti,lp8556-eeprom-settings",
						buf, pdata->eeprom_tbl_sz * 2);
		if (ret) {
			dev_err(&client->dev, "reading eeprom data failed\n");
			kfree(buf);
			return NULL;
		}

		for (i = 0; i < pdata->eeprom_tbl_sz; i++) {
			pdata->eeprom_table[i].flag = 1;
			pdata->eeprom_table[i].addr = (u8)buf[i*2];
			pdata->eeprom_table[i].data = (u8)buf[i*2 + 1];
			dev_dbg(&client->dev, "ADDR: %02X DATA: %02X\n",
				pdata->eeprom_table[i].addr,
				pdata->eeprom_table[i].data);
		}

		kfree(buf);
	}

	/* Grab the enable GPIO, if it exists */
	pdata->enable_gpio = of_get_gpio(np, 0);

	return pdata;
}
#else
static inline struct lp8556_platform_data *
lp8556_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static int __devinit lp8556_probe(struct i2c_client *client,
				  const struct i2c_device_id *id)
{
	struct lp8556_platform_data *pdata;
	struct lp8556_data *led_data;
	int error;

	pr_info("%s\n", __func__);

	if (client->dev.of_node)
		pdata = lp8556_of_init(client);
	else
		pdata = client->dev.platform_data;

	if (pdata == NULL) {
		pr_err("%s: platform data required\n", __func__);
		return -ENODEV;
	} else if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s: I2C_FUNC_I2C not supported\n", __func__);
		return -ENODEV;
	}

	led_data = kzalloc(sizeof(struct lp8556_data), GFP_KERNEL);
	if (led_data == NULL) {
		error = -ENOMEM;
		goto err_alloc_data_failed;
	}

	led_data->client = client;

	led_data->led_dev.name = LCD_BACKLIGHT;
	led_data->led_dev.brightness_set = lp8556_brightness_set;
	led_data->pdata = pdata;

	i2c_set_clientdata(client, led_data);

	INIT_WORK(&led_data->work, lp8556_brightness_work);

	led_data->regulator = regulator_get(&client->dev,
					    LP8556_REGULATOR_NAME);
	if (IS_ERR(led_data->regulator))
		pr_err("%s: regulator_get(\"%s\"): -1\n",
		       __func__, LP8556_REGULATOR_NAME);
	else {
		error = regulator_set_voltage(led_data->regulator,
				LP8556_VDD_MIN_VOLTAGE, LP8556_VDD_MAX_VOLTAGE);
		if (error != 0)
			pr_err("%s: regulator_set_voltage(%d:%d):%d\n",
				__func__, LP8556_VDD_MIN_VOLTAGE,
				LP8556_VDD_MAX_VOLTAGE, error);
		error = regulator_enable(led_data->regulator);
		if (error != 0)
			pr_err("%s: regulator_enable(\"%s\"):%d\n", __func__,
				LP8556_REGULATOR_NAME, error);
	}

	error = lp8556_gpio_init(pdata);
	if (error) {
		pr_err("%s: gpio_init failure\n", __func__);
		goto err_gpio_init;
	}

	error = gpio_export_link(&led_data->client->dev, "reset",
				 led_data->pdata->enable_gpio);
	if (error < 0) {
		pr_err("%s: failed to export link %s for gpio %d, err = %d\n",
			__func__, "reset", led_data->pdata->enable_gpio, error);
		goto err_gpio_export_link;
	}

	lp8556_enable(led_data, true);

	error = lp8556_init_registers(led_data);
	if (error < 0) {
		pr_err("%s: Register Initialization failed: %d\n",
			__func__, error);
		error = -ENODEV;
		goto err_reg_init_failed;
	}

	atomic_set(&led_data->enabled, 1);

	error = led_classdev_register((struct device *) &client->dev,
			&led_data->led_dev);
	if (error < 0) {
		pr_err("%s: Register led class failed: %d\n", __func__, error);
		error = -ENODEV;
		goto err_classdev_failed;
	}

	led_data->bl_dev = backlight_device_register(LCD_BACKLIGHT,
				&client->dev, led_data, &lp8556_bl_ops, NULL);

	if (IS_ERR(led_data->bl_dev)) {
		error = PTR_ERR(led_data->bl_dev);
		pr_err("%s: backlight_device_register: %d\n", __func__, error);
		goto err_bl_device_register;
	}

	led_data->bl_dev->props.max_brightness = LP8556_MAX_BRIGHTNESS;
	led_data->led_dev.brightness = led_data->pdata->power_up_brightness;
	led_data->bl_dev->props.brightness = led_data->led_dev.brightness;

#ifdef CONFIG_HAS_EARLYSUSPEND
	led_data->early_suspender.level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN;
	led_data->early_suspender.suspend = lp8556_early_suspend;
	led_data->early_suspender.resume = lp8556_late_resume,
	register_early_suspend(&led_data->early_suspender);
#endif

#ifdef DEBUG
	error = device_create_file(led_data->led_dev.dev, &dev_attr_registers);
	if (error < 0)
		pr_err("%s: File device creation failed: %d\n",
			__func__, error);
#endif
	pr_debug("Backlight driver initialized\n");

	return 0;

err_bl_device_register:
	led_classdev_unregister(&led_data->led_dev);
err_classdev_failed:
err_reg_init_failed:
	lp8556_enable(led_data, false);
err_gpio_export_link:
	gpio_free(led_data->pdata->enable_gpio);
err_gpio_init:
	if (!IS_ERR(led_data->regulator)) {
		regulator_disable(led_data->regulator);
		regulator_put(led_data->regulator);
	}
	kfree(led_data);
	i2c_set_clientdata(client, NULL);
err_alloc_data_failed:
	return error;
}

static int __devexit lp8556_remove(struct i2c_client *client)
{
	struct lp8556_data *led_data = i2c_get_clientdata(client);

#ifdef DEBUG
	device_remove_file(led_data->led_dev.dev, &dev_attr_registers);
#endif
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&led_data->early_suspender);
#endif
	led_data->led_dev.brightness = LED_OFF;
	lp8556_brightness_write(led_data);

	if (led_data->bl_dev)
		backlight_device_unregister(led_data->bl_dev);
	led_classdev_unregister(&led_data->led_dev);
	gpio_free(led_data->pdata->enable_gpio);
	if (!IS_ERR(led_data->regulator))
		regulator_put(led_data->regulator);
	kfree(led_data);
	i2c_set_clientdata(client, NULL);

	return 0;
}

static const struct i2c_device_id lp8556_id[] = {
	{"lp8556", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lp8556_id);

#ifdef CONFIG_OF
static struct of_device_id lp8556_match_tbl[] = {
	{ .compatible = "ti,lp8556" },
	{ },
};
MODULE_DEVICE_TABLE(of, lp8556_match_tbl);
#endif

static struct i2c_driver lp8556_i2c_driver = {
	.driver = {
		.name = "lp8556",
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(lp8556_match_tbl),
	},
	.probe = lp8556_probe,
	.remove = __devexit_p(lp8556_remove),
	.id_table = lp8556_id,
	.address_list = normal_i2c
};

static int __init lp8556_init(void)
{
	return i2c_add_driver(&lp8556_i2c_driver);
}

static void __exit lp8556_exit(void)
{
	i2c_del_driver(&lp8556_i2c_driver);
}

module_init(lp8556_init);
module_exit(lp8556_exit);

MODULE_DESCRIPTION("Lighting driver for LP8556");
MODULE_AUTHOR("Motorola Mobility");
MODULE_LICENSE("GPL");
