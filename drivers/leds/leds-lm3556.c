/*
 * Copyright (C) 2009 - 2012 Motorola, Inc.
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

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/platform_device.h>
#include <linux/types.h>
#include <linux/gpio.h>
#include <linux/module.h>
#include <linux/leds-lm3556.h>
#include <linux/delay.h>

#define LM3556_MAX_RW_RETRIES	5
#define LM3556_I2C_RETRY_DELAY	10
#define LM3556_TORCH_STEP	32
#define LM3556_STROBE_STEP	16
#define LM3556_STANDBY_VAL     0x40
#define LM3556_ENABLE_STROBE_PIN_VAL 0x63


#define LM3556_SI_REV_AND_FILTER_TIME_REG	0x00
#define LM3556_IVFM_MODE_REG			0x01
#define LM3556_NTC_SET_REG			0x02
#define LM3556_IND_RAMP_TIME_REG		0x03
#define LM3556_IND_BLINK_REG			0x04
#define LM3556_IND_PERIOD_CNT_REG		0x05
#define LM3556_TORCH_RAMP_TIME_REG		0x06
#define LM3556_CONF_REG			0x07
#define LM3556_FLASH_FEAT_REG			0x08
#define LM3556_CURRENT_CTRL_REG		0x09
#define LM3556_ENABLE_REG			0x0A
#define LM3556_FLAGS_REG			0x0B

#define TX_EVENT_FAULT			0x80
#define THERMAL_SHUTDOWN_FAULT		0x02
#define THERMAL_MONITOR_FAULT		0x40
#define LED_FAULT			0x04
#define VOLTAGE_MONITOR_FAULT		0x80
#define UNDER_VOLTAGE_FAULT		0x10

#define LM3556_MIN_STROBE_POWER		1
#define LM3556_MIN_TORCH_POWER		1

#define STROBE_CURRENT_MASK	0x0F
#define TORCH_CURRENT_MASK	0x70

#define TORCH_CURRENT_SHIFT    4

struct lm3556_data {
	struct i2c_client *client;
	struct lm3556_platform_data *pdata;
	struct led_classdev led_dev;
	int camera_strobe_brightness;
	int flash_light_brightness;
	unsigned int max_strobe_power;
	unsigned int max_torch_power;
};

static struct lm3556_data *torch_data;
static bool lm3556_probe_success;

struct lm3556_reg {
	const char *name;
	uint8_t reg;
} lm3556_regs[] = {
	{ "SI_REV_AND_FILTER_TIME",	LM3556_SI_REV_AND_FILTER_TIME_REG},
	{ "NTC_SET_REG",		LM3556_NTC_SET_REG},
	{ "IVFM_MODE_REG",		LM3556_IVFM_MODE_REG},
	{ "ENABLE",			LM3556_ENABLE_REG},
	{ "MSG_IND_RAMP_TIME",		LM3556_IND_RAMP_TIME_REG},
	{ "MSG_IND_BLINK",		LM3556_IND_BLINK_REG},
	{ "MSG_IND_PERIOD_CNT",	LM3556_IND_PERIOD_CNT_REG},
	{ "TORCH_RAMP_TIME",		LM3556_TORCH_RAMP_TIME_REG},
	{ "FLASH_FEAT_REG",		LM3556_FLASH_FEAT_REG},
	{ "CURRENT_CTRL_REG",		LM3556_CURRENT_CTRL_REG},
	{ "FLAGS",			LM3556_FLAGS_REG},
	{ "CONFIG_REG",	LM3556_CONF_REG},
};

static int lm3556_read_reg(uint8_t reg, uint8_t *val)
{
	int err = -1;
	int i = 0;
	unsigned char value[1];
	if (!val) {
		pr_err("%s: invalid value pointer\n", __func__);
		return -EINVAL;
	}
	/* If I2C client doesn't exist */
	if (torch_data->client == NULL) {
		pr_err("%s: null i2c client\n", __func__);
		return -EUNATCH;
	}

	if (torch_data->client->adapter == NULL) {
		pr_err("%s: null adapter\n", __func__);
		return -EUNATCH;
	}

	value[0] = reg & 0xFF;

	do {
		err = i2c_master_send(torch_data->client, value, 1);
		if (err == 1)
			err = i2c_master_recv(torch_data->client, val, 1);
		if (err != 1)
			msleep_interruptible(LM3556_I2C_RETRY_DELAY);
	} while ((err != 1) && ((++i) < LM3556_MAX_RW_RETRIES));
	if (err != 1)
		return err;
	return 0;
}

static int lm3556_write_reg(uint8_t reg, uint8_t val)
{
	int err = -1;
	int i = 0;
	uint8_t buf[2] = { reg, val };

	/* If I2C client doesn't exist */
	if (torch_data->client == NULL) {
		pr_err("%s: null i2c client\n", __func__);
		return -EUNATCH;
	}

	if (torch_data->client->adapter == NULL) {
		pr_err("%s: null adapter\n", __func__);
		return -EUNATCH;
	}

	do {
		err = i2c_master_send(torch_data->client, buf, 2);

		if (err != 2)
			msleep_interruptible(LM3556_I2C_RETRY_DELAY);
	} while ((err != 2) && ((++i) < LM3556_MAX_RW_RETRIES));

	if (err != 2)
		return err;

	return 0;
}

static ssize_t ld_lm3556_registers_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	unsigned i, n, reg_count;
	uint8_t value;

	reg_count = sizeof(lm3556_regs) / sizeof(lm3556_regs[0]);
	for (i = 0, n = 0; i < reg_count; i++) {
		lm3556_read_reg(lm3556_regs[i].reg, &value);
		n += scnprintf(buf + n, PAGE_SIZE - n,
				"%-20s = 0x%02X\n",
				lm3556_regs[i].name,
				value);
	}
	return n;
}

static ssize_t ld_lm3556_registers_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	unsigned i, reg_count, value;
	uint8_t torch_val, strobe_val;
	uint8_t reg_addr;
	uint8_t temp_torch_val, temp_strobe_val;
	int error;
	char name[30];

	if (count >= 30) {
		pr_err("%s:input too long\n", __func__);
		return -EINVAL;
	}

	if (sscanf(buf, "%29s %x", name, &value) != 2) {
		pr_err("%s:unable to parse input\n", __func__);
		return -EINVAL;
	}
	name[sizeof(name)-1] = '\0';

	reg_count = sizeof(lm3556_regs) / sizeof(lm3556_regs[0]);
	for (i = 0; i < reg_count; i++) {
		if (!strcmp(name, lm3556_regs[i].name)) {
			reg_addr = lm3556_regs[i].reg;
			goto reg_found;
		}
	}

	pr_err("%s:no such register %s\n", __func__, name);
	return -EINVAL;

reg_found:
	switch (reg_addr) {
	case LM3556_CURRENT_CTRL_REG:
		torch_val = TORCH_CURRENT_MASK &
			torch_data->pdata->current_cntrl_reg_def;

		strobe_val = STROBE_CURRENT_MASK &
			torch_data->pdata->current_cntrl_reg_def;

		temp_torch_val = TORCH_CURRENT_MASK & value;

		temp_strobe_val = STROBE_CURRENT_MASK & value;

		/* Check for exceeding torch/strobe current and
		 * if yes then limit the current values to max
		 * values set from device tree
		 */

		if (temp_torch_val > torch_val)
			temp_torch_val = torch_val;
		if (temp_strobe_val > strobe_val)
			temp_strobe_val = strobe_val;

		value = (temp_torch_val | temp_strobe_val);
		break;

		/*
		 * In future, here we can do register specific settings
		 * in the value to be written to the corresponding register
		 */
	default:
		break;
	}

	error = lm3556_write_reg(reg_addr, value);
	if (error) {
		pr_err("%s:Failed to write register %s\n",
			__func__, name);
		return -EIO;
	}

	return count;
}

static DEVICE_ATTR(registers, 0644, ld_lm3556_registers_show,
		ld_lm3556_registers_store);

int lm3556_enable_strobe_mode(void)
{
	int err;

	if (!lm3556_probe_success)
		return -ENODEV;

	err = lm3556_write_reg(LM3556_ENABLE_REG,
				torch_data->pdata->flash_enable_val);
	if (err) {
		pr_err("%s: Writing to 0x%X failed %i\n",
				__func__, LM3556_ENABLE_REG,
				err);
		err = -EIO;
	}

	err = lm3556_write_reg(LM3556_FLASH_FEAT_REG,
				torch_data->pdata->flash_features_reg_def);

	if (err) {
		pr_err("%s: Writing to 0x%X failed %i\n",
				__func__,
				LM3556_FLASH_FEAT_REG,
				err);
		return -EIO;
	}

	return err;
}
EXPORT_SYMBOL(lm3556_enable_strobe_mode);

int lm3556_enable_torch_mode(void)
{
	int err;

	if (!lm3556_probe_success)
		return -ENODEV;

	err = lm3556_write_reg(LM3556_ENABLE_REG,
				torch_data->pdata->torch_enable_val);
	if (err) {
		pr_err("%s: Writing to 0x%X failed %i\n",
				__func__, LM3556_ENABLE_REG,
				err);
		err = -EIO;
	}

	return err;
}
EXPORT_SYMBOL(lm3556_enable_torch_mode);


int lm3556_init_default_mode(void)
{
	int err;

	if (!lm3556_probe_success)
		return -ENODEV;

	err = lm3556_write_reg(LM3556_ENABLE_REG,
				LM3556_ENABLE_STROBE_PIN_VAL);
	if (err) {
		pr_err("%s: Writing to 0x%X failed %i\n",
				__func__, LM3556_ENABLE_REG,
				err);
		err = -EIO;
	}

	return err;
}
EXPORT_SYMBOL(lm3556_init_default_mode);

int lm3556_disable_mode(void)
{
	int err;
	if (!lm3556_probe_success)
		return -ENODEV;

	err = lm3556_write_reg(LM3556_ENABLE_REG, LM3556_STANDBY_VAL);
	return err;
}
EXPORT_SYMBOL(lm3556_disable_mode);


static int lm3556_led_write(unsigned long flash_val, uint8_t mode)
{
	int err;
	uint8_t err_flags;
	uint8_t val = 0;
	uint8_t temp_val = 0;
	uint8_t prev_flash_val, torch_brightness = 0;
	uint8_t prev_torch_val, strobe_brightness = 0;

	switch (mode) {
	case LM3556_TORCH_MODE:
		if (flash_val) {
			if (flash_val > torch_data->max_torch_power)
				flash_val = torch_data->max_torch_power;

			temp_val = flash_val / LM3556_TORCH_STEP;

			val |= temp_val << TORCH_CURRENT_SHIFT;

			err = lm3556_read_reg(LM3556_CURRENT_CTRL_REG,
					&prev_flash_val);
			if (err) {
				pr_err("%s: Reading 0x%X failed %i\n",
						__func__,
						LM3556_CURRENT_CTRL_REG,
						err);
				return -EIO;
			}

			prev_flash_val &= STROBE_CURRENT_MASK;
			torch_brightness = (val | prev_flash_val);

			err = lm3556_write_reg(LM3556_CURRENT_CTRL_REG,
							torch_brightness);

			if (err) {
				pr_err("%s: Writing to 0x%X failed %i\n",
						__func__,
						LM3556_CURRENT_CTRL_REG,
						err);
				return -EIO;
			}

			err = lm3556_enable_torch_mode();
			if (err) {
				pr_err("Failed to enable torch");
				return err;
			}
		} else {
			err = lm3556_disable_mode();
			if (err) {
				pr_err("Failed to disable torch");
				return err;
			}
		}
		torch_data->flash_light_brightness = flash_val;
		break;

	case LM3556_STROBE_MODE:
		err = lm3556_read_reg(LM3556_FLAGS_REG, &err_flags);
		if (err) {
			pr_err("%s: Reading the status failed for %i\n",
					__func__, err);
			return -EIO;
		}

		if (torch_data->pdata->flags & LM3556_ERROR_CHECK) {
			if (err_flags & (THERMAL_SHUTDOWN_FAULT |
						THERMAL_MONITOR_FAULT |
						LED_FAULT |
						VOLTAGE_MONITOR_FAULT |
						UNDER_VOLTAGE_FAULT)) {
				pr_err("%s: Error indicated by chip 0x%X\n",
						__func__, err_flags);
				return err_flags;
			}
		}
		if (flash_val > torch_data->max_strobe_power)
			flash_val = torch_data->max_strobe_power;

		val = flash_val / LM3556_STROBE_STEP;

		err = lm3556_read_reg(LM3556_CURRENT_CTRL_REG,
				&prev_torch_val);
		if (err) {
			pr_err("%s: Reading 0x%X failed %i\n",
					__func__,
					LM3556_CURRENT_CTRL_REG,
					err);
			return -EIO;
		}

		prev_torch_val &= TORCH_CURRENT_MASK;
		val = (val & 0x0f);
		strobe_brightness = (val | prev_torch_val);

		err = lm3556_write_reg(LM3556_CURRENT_CTRL_REG,
				strobe_brightness);

		if (err) {
			pr_err("%s: Writing to 0x%X failed %i\n",
					__func__,
					LM3556_CURRENT_CTRL_REG,
					err);
			return -EIO;
		}

		torch_data->camera_strobe_brightness = flash_val;
		break;
	}
	return 0;
}

static ssize_t lm3556_torch_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
				torch_data->flash_light_brightness);
}

static ssize_t lm3556_torch_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long torch_val = LED_OFF;

	err = kstrtoul(buf, 10, &torch_val);
	if (err) {
		pr_err("%s: Invalid parameter sent\n", __func__);
		return -EINVAL;
	}
	err = lm3556_led_write(torch_val, LM3556_TORCH_MODE);

	if (err)
		return err;

	return count;
}

static DEVICE_ATTR(flash_light, 0644, lm3556_torch_show, lm3556_torch_store);

static ssize_t lm3556_strobe_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n",
				torch_data->camera_strobe_brightness);
}


static ssize_t lm3556_strobe_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int err;
	unsigned long strobe_val = 0;

	err = kstrtoul(buf, 10, &strobe_val);
	if (err) {
		pr_err("%s: Invalid parameter sent\n", __func__);
		return -EINVAL;
	}
	err = lm3556_led_write(strobe_val, LM3556_STROBE_MODE);

	if (err)
		return err;

	return count;
}

static DEVICE_ATTR(camera_strobe, 0644, lm3556_strobe_show,
		lm3556_strobe_store);


static ssize_t lm3556_strobe_err_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	int err;
	uint8_t err_flags;

	err = lm3556_read_reg(LM3556_FLAGS_REG, &err_flags);
	if (err) {
		pr_err("%s: Reading the status failed for %i\n",
				__func__, err);
		return -EIO;
	}

	return snprintf(buf, PAGE_SIZE, "%d\n", (err_flags & TX_EVENT_FAULT));
}

static DEVICE_ATTR(strobe_err, 0644, lm3556_strobe_err_show, NULL);

static ssize_t lm3556_max_strobe_power_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", torch_data->max_strobe_power);
}

static DEVICE_ATTR(max_strobe_power, 0644,
		lm3556_max_strobe_power_range_show, NULL);

static ssize_t lm3556_max_torch_power_range_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%u\n", torch_data->max_torch_power);
}

static DEVICE_ATTR(max_torch_power, 0644,
		lm3556_max_torch_power_range_show, NULL);

static int lm3556_init_registers(void)
{
	int err;
	uint8_t si_filter_time_val;

	err = lm3556_read_reg(LM3556_SI_REV_AND_FILTER_TIME_REG,
			&si_filter_time_val);
	if (err) {
		pr_err("%s: Reading 0x%X failed %i\n",
				__func__, LM3556_SI_REV_AND_FILTER_TIME_REG,
				err);
		return -EIO;
	}
	si_filter_time_val |=  torch_data->pdata->si_rev_filter_time_def;

	if (lm3556_write_reg(LM3556_SI_REV_AND_FILTER_TIME_REG,
				si_filter_time_val) ||
			lm3556_write_reg(LM3556_IVFM_MODE_REG,
				torch_data->pdata->ivfm_reg_def) ||
			lm3556_write_reg(LM3556_NTC_SET_REG,
				torch_data->pdata->ntc_reg_def) ||
			lm3556_write_reg(LM3556_IND_RAMP_TIME_REG,
				torch_data->pdata->ind_ramp_time_reg_def) ||
			lm3556_write_reg(LM3556_IND_BLINK_REG,
				torch_data->pdata->ind_blink_reg_def) ||
			lm3556_write_reg(LM3556_IND_PERIOD_CNT_REG,
				torch_data->pdata->ind_period_cnt_reg_def) ||
			lm3556_write_reg(LM3556_TORCH_RAMP_TIME_REG,
				torch_data->pdata->torch_ramp_time_reg_def) ||
			lm3556_write_reg(LM3556_CONF_REG,
				torch_data->pdata->config_reg_def) ||
			lm3556_write_reg(LM3556_FLASH_FEAT_REG,
				torch_data->pdata->flash_features_reg_def) ||
			lm3556_write_reg(LM3556_CURRENT_CTRL_REG,
				torch_data->pdata->current_cntrl_reg_def) ||
			lm3556_write_reg(LM3556_ENABLE_REG,
				torch_data->pdata->enable_reg_def) ||
			lm3556_write_reg(LM3556_FLAGS_REG,
				torch_data->pdata->flag_reg_def)) {
					pr_err("%s:Register init failed\n",
							__func__);
					return -EIO;
				}
	return err;
}

static void evaluate_max_power(void)
{
	uint8_t strobe_val;
	uint8_t torch_val;

	strobe_val = (torch_data->pdata->current_cntrl_reg_def &
				STROBE_CURRENT_MASK);

	torch_data->max_strobe_power = LM3556_STROBE_STEP * strobe_val;

	if (torch_data->max_strobe_power == 0)
		torch_data->max_strobe_power = LM3556_MIN_STROBE_POWER;

	torch_val = (torch_data->pdata->current_cntrl_reg_def &
				TORCH_CURRENT_MASK);

	torch_val >>= TORCH_CURRENT_SHIFT;

	torch_data->max_torch_power = LM3556_TORCH_STEP * torch_val;

	if (torch_data->max_torch_power == 0)
		torch_data->max_strobe_power = LM3556_MIN_TORCH_POWER;
}


static int lm3556_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct lm3556_platform_data *pdata = client->dev.platform_data;
	int err = -1;
	if (pdata == NULL) {
		err = -ENODEV;
		dev_err(&client->dev, "platform data is NULL. exiting.\n");
		goto error1;
	}

	if (!pdata->flags) {
		pr_err("%s: Device does not exist\n", __func__);
		err = ENODEV;
		goto error1;
	}

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		err = -ENODEV;
		dev_err(&client->dev, "client not i2c capable\n");
		goto error1;
	}

	err = gpio_request(pdata->hw_enable, "camflash_en");
	if (err < 0) {
		pr_err("%s: gpio_request(%d) failed\n",
				__func__, pdata->hw_enable);
		err = -ENODEV;
		goto error1;
	}
	err = gpio_direction_output(pdata->hw_enable, 1);
	if (err < 0) {
		gpio_free(pdata->hw_enable);
		pr_err("%s: gpio_direction_output(%d) failed\n",
				__func__, pdata->hw_enable);
		err = -ENODEV;
		goto error1;
	}

	torch_data = kzalloc(sizeof(struct lm3556_data), GFP_KERNEL);
	if (torch_data == NULL) {
		err = -ENOMEM;
		dev_err(&client->dev, "kzalloc failed\n");
		gpio_free(pdata->hw_enable);
		goto error1;
	}

	pr_debug("%s\n", __func__);

	torch_data->client = client;
	torch_data->pdata = pdata;

	i2c_set_clientdata(client, torch_data);

	err = lm3556_init_registers();
	if (err < 0)
		goto err_init_failed_release_gpio;

	evaluate_max_power();

	torch_data->led_dev.name = LM3556_LED_DEV;
	torch_data->led_dev.brightness_set = NULL;
	err = led_classdev_register((struct device *)
			&client->dev, &torch_data->led_dev);
	if (err < 0) {
		err = -ENODEV;
		pr_err("%s: Register led class failed: %d\n",
				__func__, err);
		goto err_init_failed_release_gpio;
	}

	if (torch_data->pdata->flags & LM3556_TORCH) {
		pr_debug("%s: Creating Torch\n", __func__);

		err = device_create_file(torch_data->led_dev.dev,
				&dev_attr_flash_light);
		if (err < 0) {
			err = -ENODEV;
			pr_err("%s:File device creation failed: %d\n",
					__func__, err);
			goto err_reg_torch_file_failed;
		}

		pr_debug("%s: Creating Max_Torch_Power File\n", __func__);

		err = device_create_file(torch_data->led_dev.dev,
				&dev_attr_max_torch_power);
		if (err < 0) {
			pr_err("%s:File device creation failed: %d\n",
					__func__, err);
			err = -ENODEV;
			goto err_reg_max_torch_power_file_failed;
		}

	}
	if (torch_data->pdata->flags & LM3556_FLASH) {
		pr_debug("%s: Creating Flash\n", __func__);

		err = device_create_file(torch_data->led_dev.dev,
				&dev_attr_camera_strobe);
		if (err < 0) {
			err = -ENODEV;
			pr_err("%s:File device creation failed: %d\n",
					__func__, err);
			goto err_reg_flash_file_failed;
		}

		pr_debug("%s: Creating strobe error\n", __func__);

		err = device_create_file(torch_data->led_dev.dev,
				&dev_attr_strobe_err);
		if (err < 0) {
			err = -ENODEV;
			pr_err("%s:File device creation failed: %d\n",
					__func__, err);
			goto err_reg_strobe_error_file_failed;
		}

		pr_debug("%s: Creating Max_Strobe_Power File\n", __func__);

		err = device_create_file(torch_data->led_dev.dev,
				&dev_attr_max_strobe_power);
		if (err < 0) {
			pr_err("%s:File device creation failed: %d\n",
					__func__, err);
			err = -ENODEV;
			goto err_reg_max_strobe_power_file_failed;
		}
	}


	pr_debug("%s: Creating Registers File\n", __func__);

	err = device_create_file(torch_data->led_dev.dev,
			&dev_attr_registers);
	if (err < 0) {
		pr_err("%s:File device creation failed: %d\n",
				__func__, err);
		err = -ENODEV;
		goto err_reg_register_file_failed;
	}

	lm3556_probe_success = true;

	pr_debug("LM3556 Initialised");
	return 0;

err_reg_register_file_failed:
	if (torch_data->pdata->flags & LM3556_FLASH)
		device_remove_file(torch_data->led_dev.dev,
				&dev_attr_max_strobe_power);

err_reg_max_strobe_power_file_failed:
	if (torch_data->pdata->flags & LM3556_FLASH)
		device_remove_file(torch_data->led_dev.dev,
				&dev_attr_strobe_err);

err_reg_strobe_error_file_failed:
	if (torch_data->pdata->flags & LM3556_FLASH)
		device_remove_file(torch_data->led_dev.dev,
				&dev_attr_camera_strobe);
err_reg_flash_file_failed:
	if (torch_data->pdata->flags & LM3556_TORCH)
		device_remove_file(torch_data->led_dev.dev,
				&dev_attr_max_torch_power);

err_reg_max_torch_power_file_failed:
	if (torch_data->pdata->flags & LM3556_TORCH)
		device_remove_file(torch_data->led_dev.dev,
				&dev_attr_flash_light);
err_reg_torch_file_failed:
	led_classdev_unregister(&torch_data->led_dev);

err_init_failed_release_gpio:
	gpio_free(pdata->hw_enable);
	kfree(torch_data);
error1:
	return err;
}


static int lm3556_remove(struct i2c_client *client)
{
	if (torch_data->pdata->flags & LM3556_FLASH) {
		device_remove_file(torch_data->led_dev.dev,
				&dev_attr_camera_strobe);
		device_remove_file(torch_data->led_dev.dev,
				&dev_attr_strobe_err);
		device_remove_file(torch_data->led_dev.dev,
				&dev_attr_max_strobe_power);
	}

	if (torch_data->pdata->flags & LM3556_TORCH) {
		device_remove_file(torch_data->led_dev.dev,
				&dev_attr_flash_light);
		device_remove_file(torch_data->led_dev.dev,
				&dev_attr_max_torch_power);
	}

	led_classdev_unregister(&torch_data->led_dev);

	device_remove_file(torch_data->led_dev.dev, &dev_attr_registers);

	gpio_free(torch_data->pdata->hw_enable);
	kfree(torch_data->pdata);
	kfree(torch_data);
	return 0;
}

static const struct i2c_device_id lm3556_id[] = {
	{LM3556_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, lm3556_id);

static struct i2c_driver lm3556_i2c_driver = {
	.probe = lm3556_probe,
	.remove = lm3556_remove,
	.id_table = lm3556_id,
	.driver = {
		.name = LM3556_NAME,
		.owner = THIS_MODULE,
	},
};

static int __init lm3556_init(void)
{
	pr_debug("LM3556 LED DRIVER IC INITIALISATION\n");
	return i2c_add_driver(&lm3556_i2c_driver);
}

static void __init lm3556_exit(void)
{
	pr_debug("LM3556 LED DRIVER IC EXIT\n");
	i2c_del_driver(&lm3556_i2c_driver);
}


module_init(lm3556_init);
module_exit(lm3556_exit);


/*****************************************************************************/

MODULE_DESCRIPTION("Lightning Driver for LM3556");
MODULE_AUTHOR("MOTOROLA");
MODULE_LICENSE("GPL");
