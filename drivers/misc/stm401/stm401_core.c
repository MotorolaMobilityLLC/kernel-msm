/*
 * Copyright (C) 2010-2013 Motorola Mobility LLC
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

#include <linux/cdev.h>
#include <linux/delay.h>
#ifdef CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/export.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/input-polldev.h>
#include <linux/interrupt.h>
#include <linux/irq.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/poll.h>
#include <linux/regulator/consumer.h>
#include <linux/slab.h>
#include <linux/switch.h>
#include <linux/time.h>
#include <linux/uaccess.h>
#include <linux/wakelock.h>
#include <linux/workqueue.h>

#include <linux/stm401.h>


#define NAME			     "stm401"

#define I2C_RETRIES			5
#define RESET_RETRIES			2
#define STM401_DELAY_USEC		10
#define G_MAX				0x7FFF

#define STM401_BUSY_SLEEP_USEC    10000
#define STM401_BUSY_RESUME_COUNT  14
#define STM401_BUSY_SUSPEND_COUNT 6
#define STM401_I2C_FAIL_LIMIT     10

long stm401_time_delta;
unsigned int stm401_irq_disable;
module_param_named(irq_disable, stm401_irq_disable, uint, 0644);

unsigned short stm401_i2c_retry_delay = 10;

/* Remember the sensor state */
unsigned short stm401_g_acc_delay;
unsigned short stm401_g_mag_delay;
unsigned short stm401_g_gyro_delay;
unsigned short stm401_g_baro_delay;
unsigned short stm401_g_nonwake_sensor_state;
unsigned short stm401_g_wake_sensor_state;
unsigned short stm401_g_algo_state;
unsigned char stm401_g_motion_dur;
unsigned char stm401_g_zmotion_dur;
unsigned char stm401_g_control_reg[STM401_CONTROL_REG_SIZE];
unsigned char stm401_g_mag_cal[STM401_MAG_CAL_SIZE];

/* Store error message */
unsigned char stat_string[ESR_SIZE+1];

struct stm401_algo_requst_t stm401_g_algo_requst[STM401_NUM_ALGOS];

unsigned char stm401_cmdbuff[512];
unsigned char stm401_readbuff[512];

/* per algo config, request, and event registers */
const struct stm401_algo_info_t stm401_algo_info[STM401_NUM_ALGOS] = {
	{ M_ALGO_MODALITY, ALGO_CFG_MODALITY, ALGO_REQ_MODALITY,
	  ALGO_EVT_MODALITY, STM401_EVT_SZ_TRANSITION },
	{ M_ALGO_ORIENTATION, ALGO_CFG_ORIENTATION, ALGO_REQ_ORIENTATION,
	  ALGO_EVT_ORIENTATION, STM401_EVT_SZ_TRANSITION },
	{ M_ALGO_STOWED, ALGO_CFG_STOWED, ALGO_REQ_STOWED,
	  ALGO_EVT_STOWED, STM401_EVT_SZ_TRANSITION },
	{ M_ALGO_ACCUM_MODALITY, ALGO_CFG_ACCUM_MODALITY,
	   ALGO_REQ_ACCUM_MODALITY, ALGO_EVT_ACCUM_MODALITY,
	   STM401_EVT_SZ_ACCUM_STATE },
	{ M_ALGO_ACCUM_MVMT, ALGO_CFG_ACCUM_MVMT, ALGO_REQ_ACCUM_MVMT,
	  ALGO_EVT_ACCUM_MVMT, STM401_EVT_SZ_ACCUM_MVMT }
};

struct stm401_data *stm401_misc_data;

#ifdef CONFIG_HAS_EARLYSUSPEND
static void stm401_early_suspend(struct early_suspend *handler);
static void stm401_late_resume(struct early_suspend *handler);
#endif

int stm401_i2c_write_read_no_reset(struct stm401_data *ps_stm401,
			u8 *buf, int writelen, int readlen)
{
	int tries, err = 0;
	struct i2c_msg msgs[] = {
		{
			.addr = ps_stm401->client->addr,
			.flags = ps_stm401->client->flags,
			.len = writelen,
			.buf = buf,
		},
		{
			.addr = ps_stm401->client->addr,
			.flags = ps_stm401->client->flags | I2C_M_RD,
			.len = readlen,
			.buf = stm401_readbuff,
		},
	};
	if (buf == NULL || writelen == 0 || readlen == 0)
		return -EFAULT;

	tries = 0;
	do {
		err = i2c_transfer(ps_stm401->client->adapter, msgs, 2);
		if (err != 2)
			msleep_interruptible(stm401_i2c_retry_delay);
	} while ((err != 2) && (++tries < I2C_RETRIES));
	if (err != 2) {
		dev_err(&ps_stm401->client->dev, "Read transfer error\n");
		err = -EIO;
		ps_stm401->stm401_i2c_fail_count++;
		if (ps_stm401->stm401_i2c_fail_count > STM401_I2C_FAIL_LIMIT)
			ps_stm401->stm401_hub_fail = 1;
	} else {
		err = 0;
		dev_dbg(&ps_stm401->client->dev, "Read from STM401: ");
		for (tries = 0; tries < readlen; tries++)
			dev_dbg(&ps_stm401->client->dev, "%02x",
				stm401_readbuff[tries]);
		ps_stm401->stm401_i2c_fail_count = 0;
	}

	return err;
}

int stm401_i2c_read_no_reset(struct stm401_data *ps_stm401,
			u8 *buf, int len)
{
	int tries, err = 0;

	if (buf == NULL || len == 0)
		return -EFAULT;
	tries = 0;
	do {
		err = i2c_master_recv(ps_stm401->client, buf, len);
		if (err < 0)
			msleep_interruptible(stm401_i2c_retry_delay);
	} while ((err < 0) && (++tries < I2C_RETRIES));
	if (err < 0) {
		dev_err(&ps_stm401->client->dev, "i2c read transfer error\n");
		err = -EIO;
		ps_stm401->stm401_i2c_fail_count++;
		if (ps_stm401->stm401_i2c_fail_count > STM401_I2C_FAIL_LIMIT)
			ps_stm401->stm401_hub_fail = 1;
	} else {
		dev_dbg(&ps_stm401->client->dev, "i2c read was successsful:\n");
		for (tries = 0; tries < err; tries++)
			dev_dbg(&ps_stm401->client->dev, "%02x", buf[tries]);
		ps_stm401->stm401_i2c_fail_count = 0;
	}

	return err;
}

int stm401_i2c_write_no_reset(struct stm401_data *ps_stm401,
			u8 *buf, int len)
{
	int err = 0;
	int tries = 0;

	tries = 0;
	do {
		err = i2c_master_send(ps_stm401->client, buf, len);
		if (err < 0)
			msleep_interruptible(stm401_i2c_retry_delay);
	} while ((err < 0) && (++tries < I2C_RETRIES));

	if (err < 0) {
		dev_err(&ps_stm401->client->dev, "i2c write error\n");
		err = -EIO;
		ps_stm401->stm401_i2c_fail_count++;
		if (ps_stm401->stm401_i2c_fail_count > STM401_I2C_FAIL_LIMIT)
			ps_stm401->stm401_hub_fail = 1;
	} else {
		dev_dbg(&ps_stm401->client->dev,
			"i2c write successful\n");
		err = 0;
		ps_stm401->stm401_i2c_fail_count = 0;
	}

	return err;
}

static int stm401_hw_init(struct stm401_data *ps_stm401)
{
	int err = 0;
	dev_dbg(&ps_stm401->client->dev, "stm401_hw_init\n");
	ps_stm401->hw_initialized = 1;
	return err;
}

static void stm401_device_power_off(struct stm401_data *ps_stm401)
{
	dev_dbg(&ps_stm401->client->dev,
		"stm401_device_power_off\n");
	if (ps_stm401->hw_initialized == 1) {
		if (ps_stm401->pdata->power_off)
			ps_stm401->pdata->power_off();
		ps_stm401->hw_initialized = 0;
	}
}

static int stm401_device_power_on(struct stm401_data *ps_stm401)
{
	int err = 0;
	dev_dbg(&ps_stm401->client->dev, "stm401_device_power_on\n");
	if (ps_stm401->pdata->power_on) {
		err = ps_stm401->pdata->power_on();
		if (err < 0) {
			dev_err(&ps_stm401->client->dev,
				"power_on failed: %d\n", err);
			return err;
		}
	}
	if (!ps_stm401->hw_initialized) {
		err = stm401_hw_init(ps_stm401);
		if (err < 0) {
			stm401_device_power_off(ps_stm401);
			return err;
		}
	}
	return err;
}

int stm401_i2c_write_read(struct stm401_data *ps_stm401, u8 *buf,
			int writelen, int readlen)
{
	int tries, err = 0;

	if (buf == NULL || writelen == 0 || readlen == 0)
		return -EFAULT;

	tries = 0;
	do {
		err = stm401_i2c_write_read_no_reset(ps_stm401,
			buf, writelen, readlen);
		if (err < 0)
			stm401_reset_and_init();
	} while ((err < 0) && (++tries < RESET_RETRIES));

	if (err < 0) {
		dev_err(&ps_stm401->client->dev, "write_read error\n");
		err = -EIO;
	} else {
		dev_dbg(&ps_stm401->client->dev, "write_read successsful:\n");
	}

	return err;
}

int stm401_i2c_read(struct stm401_data *ps_stm401, u8 *buf, int len)
{
	int tries, err = 0;

	if (buf == NULL || len == 0)
		return -EFAULT;
	tries = 0;
	do {
		err = stm401_i2c_read_no_reset(ps_stm401, buf, len);
		if (err < 0)
			stm401_reset_and_init();
	} while ((err < 0) && (++tries < RESET_RETRIES));
	if (err < 0) {
		dev_err(&ps_stm401->client->dev, "read error\n");
		err = -EIO;
	} else {
		dev_dbg(&ps_stm401->client->dev, "read successsful:\n");
	}
	return err;
}

int stm401_i2c_write(struct stm401_data *ps_stm401, u8 *buf, int len)
{
	int err = 0;
	int tries = 0;

	tries = 0;
	do {
		err = stm401_i2c_write_no_reset(ps_stm401, buf, len);
		if (err < 0)
			stm401_reset_and_init();
	} while ((err < 0) && (++tries < RESET_RETRIES));

	if (err < 0) {
		dev_err(&ps_stm401->client->dev, "write error - %x\n", buf[0]);
		err = -EIO;
	} else {
		dev_dbg(&ps_stm401->client->dev,
			"write successful\n");
		err = 0;
	}
	return err;
}

static ssize_t dock_print_name(struct switch_dev *switch_dev, char *buf)
{
	switch (switch_get_state(switch_dev)) {
	case NO_DOCK:
		return snprintf(buf, 5, "None\n");
	case DESK_DOCK:
		return snprintf(buf, 5, "DESK\n");
	case CAR_DOCK:
		return snprintf(buf, 4, "CAR\n");
	}

	return -EINVAL;
}

int stm401_enable(struct stm401_data *ps_stm401)
{
	int err = 0;

	dev_dbg(&ps_stm401->client->dev, "stm401_enable\n");
	if (!atomic_cmpxchg(&ps_stm401->enabled, 0, 1)) {
		err = stm401_device_power_on(ps_stm401);
		if (err < 0) {
			atomic_set(&ps_stm401->enabled, 0);
			dev_err(&ps_stm401->client->dev,
				"stm401_enable returned with %d\n", err);
			return err;
		}
	}

	return err;
}

struct miscdevice stm401_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = NAME,
	.fops = &stm401_misc_fops,
};

#ifdef CONFIG_OF
static struct stm401_platform_data *
stm401_of_init(struct i2c_client *client)
{
	int len;
	int lsize, bsize, index;
	struct stm401_platform_data *pdata;
	struct device_node *np = client->dev.of_node;
	unsigned int lux_table[LIGHTING_TABLE_SIZE];
	unsigned int brightness_table[LIGHTING_TABLE_SIZE];
	const char *name;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);
	if (!pdata) {
		dev_err(&stm401_misc_data->client->dev,
			"pdata allocation failure\n");
		return NULL;
	}

	pdata->gpio_int = of_get_gpio(np, 0);
	pdata->gpio_reset = of_get_gpio(np, 1);
	pdata->gpio_bslen = of_get_gpio(np, 2);
	pdata->gpio_wakeirq = of_get_gpio(np, 3);
#if 0
	pdata->gpio_mipi_req = of_get_gpio(np, 4);
	pdata->gpio_mipi_busy = of_get_gpio(np, 5);
#endif
	if (of_get_property(np, "lux_table", &len) == NULL) {
		dev_err(&stm401_misc_data->client->dev,
			"lux_table len access failure\n");
		return NULL;
	}
	lsize = len / sizeof(u32);
	if ((lsize != 0) && (lsize < (LIGHTING_TABLE_SIZE - 1)) &&
		(!of_property_read_u32_array(np, "lux_table",
					(u32 *)(lux_table),
					lsize))) {
		for (index = 0; index < lsize; index++)
			pdata->lux_table[index] = ((u32 *)lux_table)[index];
	} else {
		dev_err(&stm401_misc_data->client->dev,
			"Lux table is missing\n");
		return NULL;
	}
	pdata->lux_table[lsize] = 0xFFFF;

	if (of_get_property(np, "brightness_table", &len) == NULL) {
		dev_err(&stm401_misc_data->client->dev,
			"Brightness_table len access failure\n");
		return NULL;
	}
	bsize = len / sizeof(u32);
	if ((bsize != 0) && (bsize < (LIGHTING_TABLE_SIZE)) &&
		!of_property_read_u32_array(np,
					"brightness_table",
					(u32 *)(brightness_table),
					bsize)) {

		for (index = 0; index < bsize; index++) {
			pdata->brightness_table[index]
				= ((u32 *)brightness_table)[index];
		}
	} else {
		dev_err(&stm401_misc_data->client->dev,
			"Brightness table is missing\n");
		return NULL;
	}

	if ((lsize + 1) != bsize) {
		dev_err(&stm401_misc_data->client->dev,
			"Lux and Brightness table sizes don't match\n");
		return NULL;
	}

	of_property_read_u32(np, "bslen_pin_active_value",
				&pdata->bslen_pin_active_value);

	of_get_property(np, "stm401_fw_version", &len);
	if (!of_property_read_string(np, "stm401_fw_version", &name))
		strlcpy(pdata->fw_version, name, FW_VERSION_SIZE);
	else
		dev_dbg(&stm401_misc_data->client->dev,
			"Not useing stm401_fw_version override\n");

	pdata->ct406_detect_threshold = 0x006E;
	pdata->ct406_undetect_threshold = 0x0050;
	pdata->ct406_recalibrate_threshold = 0x0064;
	pdata->ct406_pulse_count = 0x04;
	of_property_read_u32(np, "ct406_detect_threshold",
				&pdata->ct406_detect_threshold);
	of_property_read_u32(np, "ct406_undetect_threshold",
				&pdata->ct406_undetect_threshold);
	of_property_read_u32(np, "ct406_recalibrate_threshold",
				&pdata->ct406_recalibrate_threshold);
	of_property_read_u32(np, "ct406_pulse_count",
				&pdata->ct406_pulse_count);

	return pdata;
}
#else
static inline struct stm401_platform_data *
stm401_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static int stm401_gpio_init(struct stm401_platform_data *pdata,
				struct i2c_client *pdev)
{
	int err;

	err = gpio_request(pdata->gpio_int, "stm401 int");
	if (err) {
		dev_err(&stm401_misc_data->client->dev,
			"stm401 int gpio_request failed: %d\n", err);
		return err;
	}
	gpio_direction_input(pdata->gpio_int);
	err = gpio_export(pdata->gpio_int, 0);
	if (err) {
		dev_err(&stm401_misc_data->client->dev,
			"gpio_int gpio_export failed: %d\n", err);
		goto free_int;
	}
	err = gpio_export_link(&pdev->dev, "gpio_irq", pdata->gpio_int);
	if (err) {
		dev_err(&stm401_misc_data->client->dev,
			"gpio_irq gpio_export_link failed: %d\n", err);
		goto free_int;
	}

	err = gpio_request(pdata->gpio_reset, "stm401 reset");
	if (err) {
		dev_err(&stm401_misc_data->client->dev,
			"stm401 reset gpio_request failed: %d\n", err);
		goto free_int;
	}
	gpio_direction_output(pdata->gpio_reset, 1);
	gpio_set_value(pdata->gpio_reset, 1);
	err = gpio_export(pdata->gpio_reset, 0);
	if (err) {
		dev_err(&stm401_misc_data->client->dev,
			"reset gpio_export failed: %d\n", err);
		goto free_reset;
	}

	err = gpio_request(pdata->gpio_bslen, "stm401 bslen");
	if (err) {
		dev_err(&stm401_misc_data->client->dev,
			"bslen gpio_request failed: %d\n", err);
		goto free_reset;
	}
	gpio_direction_output(pdata->gpio_bslen, 0);
	gpio_set_value(pdata->gpio_bslen, 0);
	err = gpio_export(pdata->gpio_bslen, 0);
	if (err) {
		dev_err(&stm401_misc_data->client->dev,
			"bslen gpio_export failed: %d\n", err);
		goto free_bslen;
	}

	if (gpio_is_valid(pdata->gpio_wakeirq)) {
		err = gpio_request(pdata->gpio_wakeirq, "stm401 wakeirq");
		if (err) {
			dev_err(&stm401_misc_data->client->dev,
				"wakeirq gpio_request failed: %d\n", err);
			goto free_bslen;
		}
		gpio_direction_input(pdata->gpio_wakeirq);
		err = gpio_export(pdata->gpio_wakeirq, 0);
		if (err) {
			dev_err(&stm401_misc_data->client->dev,
				"wakeirq gpio_export failed: %d\n", err);
			goto free_wakeirq;
		}

		err = gpio_export_link(&pdev->dev, "wakeirq",
						pdata->gpio_wakeirq);
		if (err) {
			dev_err(&stm401_misc_data->client->dev,
				"wakeirq link failed: %d\n", err);
			goto free_wakeirq;
		}
	} else {
		pr_warn("%s: gpio wake irq not specified\n", __func__);
	}

#if 0
	if (gpio_is_valid(pdata->gpio_mipi_req)) {
		err = gpio_request(pdata->gpio_mipi_req, "mipi_d0_req");
		if (err) {
			dev_err(&stm401_misc_data->client->dev,
				"mipi_req_gpio gpio_request failed: %d\n", err);
			goto free_wakeirq;
		}
		gpio_direction_output(pdata->gpio_mipi_req, 0);
		err = gpio_export(pdata->gpio_mipi_req, 0);
		if (err) {
			dev_err(&stm401_misc_data->client->dev,
				"mipi_req_gpio gpio_export failed: %d\n", err);
			goto free_mipi_req;
		}
		stm401_misc_data->ap_stm401_handoff_ctrl = true;
	} else {
		stm401_misc_data->ap_stm401_handoff_ctrl = false;
		pr_warn("%s: gpio mipi req not specified\n", __func__);
	}

	if (gpio_is_valid(pdata->gpio_mipi_busy)) {
		err = gpio_request(pdata->gpio_mipi_busy, "mipi_d0_busy");
		if (err) {
			dev_err(&stm401_misc_data->client->dev,
				"mipi_d0_busy gpio_request failed: %d\n", err);
			goto free_mipi_req;
		}
		gpio_direction_input(pdata->gpio_mipi_busy);
		err = gpio_export(pdata->gpio_mipi_busy, 0);
		if (err) {
			dev_err(&stm401_misc_data->client->dev,
				"mipi_d0_busy gpio_export failed: %d\n", err);
			goto free_mipi_busy;
		}
	} else {
		stm401_misc_data->ap_stm401_handoff_ctrl = false;
		pr_warn("%s: gpio mipi busy not specified\n", __func__);
	}
#endif

	return 0;

#if 0
free_mipi_busy:
	gpio_free(pdata->gpio_mipi_busy);
free_mipi_req:
	gpio_free(pdata->gpio_mipi_req);
#endif
free_wakeirq:
	gpio_free(pdata->gpio_wakeirq);
free_bslen:
	gpio_free(pdata->gpio_bslen);
free_reset:
	gpio_free(pdata->gpio_reset);
free_int:
	gpio_free(pdata->gpio_int);
	return err;
}

static void stm401_gpio_free(struct stm401_platform_data *pdata)
{
	gpio_free(pdata->gpio_int);
	gpio_free(pdata->gpio_reset);
	gpio_free(pdata->gpio_bslen);
	gpio_free(pdata->gpio_wakeirq);
#if 0
	gpio_free(pdata->gpio_mipi_req);
	gpio_free(pdata->gpio_mipi_busy);
#endif
}

static int stm401_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct stm401_platform_data *pdata;
	struct stm401_data *ps_stm401;
	int err = -1;
	dev_info(&client->dev, "probe begun\n");

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "client not i2c capable\n");
		return -ENODEV;
	}

	ps_stm401 = kzalloc(sizeof(*ps_stm401), GFP_KERNEL);
	if (ps_stm401 == NULL) {
		dev_err(&client->dev,
			"failed to allocate memory for module data: %d\n", err);
		return -ENOMEM;
	}
	ps_stm401->client = client;
	stm401_misc_data = ps_stm401;

	if (client->dev.of_node)
		pdata = stm401_of_init(client);
	else
		pdata = client->dev.platform_data;

	if (pdata == NULL) {
		dev_err(&client->dev, "platform data is NULL, exiting\n");
		err = -ENODEV;
		goto err_pdata;
	}

	/* initialize regulators */
	ps_stm401->regulator_1 = regulator_get(&client->dev, "sensor1");
	if (IS_ERR(ps_stm401->regulator_1)) {
		dev_err(&client->dev, "Failed to get VIO regulator\n");
		goto err_regulator;
	}

	ps_stm401->regulator_2 = regulator_get(&client->dev, "sensor2");
	if (IS_ERR(ps_stm401->regulator_2)) {
		dev_err(&client->dev, "Failed to get VCC regulator\n");
		regulator_put(ps_stm401->regulator_1);
		goto err_regulator;
	}

	if (regulator_enable(ps_stm401->regulator_1)) {
		dev_err(&client->dev, "Failed to enable Sensor 1 regulator\n");
		regulator_put(ps_stm401->regulator_2);
		regulator_put(ps_stm401->regulator_1);
		goto err_regulator;
	}

	if (regulator_enable(ps_stm401->regulator_2)) {
		dev_err(&client->dev, "Failed to enable Sensor 2 regulator\n");
		regulator_disable(ps_stm401->regulator_1);
		regulator_put(ps_stm401->regulator_2);
		regulator_put(ps_stm401->regulator_1);
		goto err_regulator;
	}

	err = stm401_gpio_init(pdata, client);
	if (err) {
		dev_err(&client->dev, "stm401 gpio init failed\n");
		goto err_gpio_init;
	}

	mutex_init(&ps_stm401->lock);
	mutex_lock(&ps_stm401->lock);
	wake_lock_init(&ps_stm401->wakelock, WAKE_LOCK_SUSPEND, "stm401");

	ps_stm401->ap_stm401_handoff_enable = false;

	/* Set to passive mode by default */
	stm401_g_nonwake_sensor_state = 0;
	stm401_g_wake_sensor_state = 0;
	/* clear the interrupt mask */
	ps_stm401->intp_mask = 0x00;

	INIT_WORK(&ps_stm401->irq_work, stm401_irq_work_func);
	INIT_WORK(&ps_stm401->irq_wake_work, stm401_irq_wake_work_func);

	ps_stm401->irq_work_queue = create_singlethread_workqueue("stm401_wq");
	if (!ps_stm401->irq_work_queue) {
		err = -ENOMEM;
		dev_err(&client->dev, "cannot create work queue: %d\n", err);
		goto err1;
	}
	ps_stm401->pdata = pdata;
	i2c_set_clientdata(client, ps_stm401);
	ps_stm401->client->flags &= 0x00;

	if (ps_stm401->pdata->init) {
		err = ps_stm401->pdata->init();
		if (err < 0) {
			dev_err(&client->dev, "init failed: %d\n", err);
			goto err2;
		}
	}

	/*configure interrupt*/
	ps_stm401->irq = gpio_to_irq(ps_stm401->pdata->gpio_int);
	if (gpio_is_valid(ps_stm401->pdata->gpio_wakeirq))
		ps_stm401->irq_wake
			= gpio_to_irq(ps_stm401->pdata->gpio_wakeirq);
	else
		ps_stm401->irq_wake = -1;

	err = stm401_device_power_on(ps_stm401);
	if (err < 0) {
		dev_err(&client->dev, "power on failed: %d\n", err);
		goto err4;
	}

	if (ps_stm401->irq_wake != -1)
		enable_irq_wake(ps_stm401->irq_wake);
	atomic_set(&ps_stm401->enabled, 1);

	err = misc_register(&stm401_misc_device);
	if (err < 0) {
		dev_err(&client->dev, "misc register failed: %d\n", err);
		goto err6;
	}

	if (alloc_chrdev_region(&ps_stm401->stm401_dev_num, 0, 2, "stm401")
		< 0)
		dev_err(&client->dev,
			"alloc_chrdev_region failed\n");
	ps_stm401->stm401_class = class_create(THIS_MODULE, "stm401");

	cdev_init(&ps_stm401->as_cdev, &stm401_as_fops);
	ps_stm401->as_cdev.owner = THIS_MODULE;
	err = cdev_add(&ps_stm401->as_cdev, ps_stm401->stm401_dev_num, 1);
	if (err)
		dev_err(&client->dev,
			"cdev_add as failed: %d\n", err);

	device_create(ps_stm401->stm401_class, NULL,
		MKDEV(MAJOR(ps_stm401->stm401_dev_num), 0),
		ps_stm401, "stm401_as");

	cdev_init(&ps_stm401->ms_cdev, &stm401_ms_fops);
	ps_stm401->ms_cdev.owner = THIS_MODULE;
	err = cdev_add(&ps_stm401->ms_cdev, ps_stm401->stm401_dev_num + 1, 1);
	if (err)
		dev_err(&client->dev,
			"cdev_add ms failed: %d\n", err);

	device_create(ps_stm401->stm401_class, NULL,
		MKDEV(MAJOR(ps_stm401->stm401_dev_num), 1),
		ps_stm401, "stm401_ms");

	stm401_device_power_off(ps_stm401);

	atomic_set(&ps_stm401->enabled, 0);

	err = request_irq(ps_stm401->irq, stm401_isr,
				IRQF_TRIGGER_RISING,
				NAME, ps_stm401);
	if (err < 0) {
		dev_err(&client->dev, "request irq failed: %d\n", err);
		goto err7;
	}

	if (ps_stm401->irq_wake != -1) {
		err = request_irq(ps_stm401->irq_wake, stm401_wake_isr,
				IRQF_TRIGGER_RISING,
				NAME, ps_stm401);
		if (err < 0) {
			dev_err(&client->dev, "request wake irq failed: %d\n",
				err);
			goto err8;
		}
	}

#ifdef CONFIG_HAS_EARLYSUSPEND
	ps_stm401->early_suspend.level = EARLY_SUSPEND_LEVEL_DISABLE_FB + 1;
	ps_stm401->early_suspend.suspend = stm401_early_suspend;
	ps_stm401->early_suspend.resume = stm401_late_resume;
	register_early_suspend(&ps_stm401->early_suspend);
#endif

	init_waitqueue_head(&ps_stm401->stm401_as_data_wq);
	init_waitqueue_head(&ps_stm401->stm401_ms_data_wq);

	ps_stm401->dsdev.name = "dock";
	ps_stm401->dsdev.print_name = dock_print_name;
	err = switch_dev_register(&ps_stm401->dsdev);
	if (err) {
		dev_err(&client->dev,
			"Couldn't register switch (%s) rc=%d\n",
			ps_stm401->dsdev.name, err);
		ps_stm401->dsdev.dev = NULL;
	}

	ps_stm401->edsdev.name = "extdock";
	ps_stm401->edsdev.print_name = dock_print_name;
	err = switch_dev_register(&ps_stm401->edsdev);
	if (err) {
		dev_err(&client->dev,
			"Couldn't register switch (%s) rc=%d\n",
			ps_stm401->edsdev.name, err);
		ps_stm401->edsdev.dev = NULL;
	}

	ps_stm401->input_dev = input_allocate_device();
	if (!ps_stm401->input_dev) {
		err = -ENOMEM;
		dev_err(&client->dev,
			"input device allocate failed: %d\n", err);
		goto err8;
	}
	input_set_drvdata(ps_stm401->input_dev, ps_stm401);
	input_set_capability(ps_stm401->input_dev, EV_KEY, KEY_POWER);
	input_set_capability(ps_stm401->input_dev, EV_KEY, KEY_CAMERA);
	ps_stm401->input_dev->name = "sensorprocessor";

	err = input_register_device(ps_stm401->input_dev);
	if (err) {
		dev_err(&client->dev,
			"unable to register input polled device %s: %d\n",
			ps_stm401->input_dev->name, err);
		goto err9;
	}

	switch_stm401_mode(NORMALMODE);

	mutex_unlock(&ps_stm401->lock);

	dev_info(&client->dev, "probed finished\n");

	return 0;

err9:
	input_free_device(ps_stm401->input_dev);
err8:
	free_irq(ps_stm401->irq, ps_stm401);
err7:
	misc_deregister(&stm401_misc_device);
err6:
	stm401_device_power_off(ps_stm401);
err4:
	if (ps_stm401->pdata->exit)
		ps_stm401->pdata->exit();
err2:
	destroy_workqueue(ps_stm401->irq_work_queue);
err1:
	mutex_unlock(&ps_stm401->lock);
	mutex_destroy(&ps_stm401->lock);
	wake_unlock(&ps_stm401->wakelock);
	wake_lock_destroy(&ps_stm401->wakelock);
	stm401_gpio_free(pdata);
err_gpio_init:
	regulator_disable(ps_stm401->regulator_2);
	regulator_disable(ps_stm401->regulator_1);
	regulator_put(ps_stm401->regulator_2);
	regulator_put(ps_stm401->regulator_1);
err_regulator:
err_pdata:
	kfree(ps_stm401);
	return err;
}

static int stm401_remove(struct i2c_client *client)
{
	struct stm401_data *ps_stm401 = i2c_get_clientdata(client);
	dev_err(&ps_stm401->client->dev, "stm401_remove\n");
	switch_dev_unregister(&ps_stm401->dsdev);
	switch_dev_unregister(&ps_stm401->edsdev);
	if (ps_stm401->irq_wake != -1)
		free_irq(ps_stm401->irq_wake, ps_stm401);
	free_irq(ps_stm401->irq, ps_stm401);
	misc_deregister(&stm401_misc_device);
	input_unregister_device(ps_stm401->input_dev);
	input_free_device(ps_stm401->input_dev);
	stm401_device_power_off(ps_stm401);
	if (ps_stm401->pdata->exit)
		ps_stm401->pdata->exit();
	stm401_gpio_free(ps_stm401->pdata);
	destroy_workqueue(ps_stm401->irq_work_queue);
	mutex_destroy(&ps_stm401->lock);
	wake_unlock(&ps_stm401->wakelock);
	wake_lock_destroy(&ps_stm401->wakelock);
	disable_irq_wake(ps_stm401->irq);
#ifdef CONFIG_HAS_EARLYSUSPEND
	unregister_early_suspend(&ps_stm401->early_suspend);
#endif
	regulator_disable(ps_stm401->regulator_2);
	regulator_disable(ps_stm401->regulator_1);
	regulator_put(ps_stm401->regulator_2);
	regulator_put(ps_stm401->regulator_1);
	kfree(ps_stm401);

	return 0;
}

static int stm401_resume(struct i2c_client *client)
{
	struct stm401_data *ps_stm401 = i2c_get_clientdata(client);
	int count = 0, level = 0;
	int stm401_req = ps_stm401->pdata->gpio_mipi_req;
	int stm401_busy = ps_stm401->pdata->gpio_mipi_busy;
	dev_dbg(&stm401_misc_data->client->dev, "stm401_resume\n");
	mutex_lock(&ps_stm401->lock);

	if ((ps_stm401->ap_stm401_handoff_enable)
		&& (ps_stm401->ap_stm401_handoff_ctrl)) {
		gpio_set_value(stm401_req, 0);
		dev_dbg(&ps_stm401->client->dev, "STM401 REQ is set %d\n",
			 gpio_get_value(stm401_req));
	}

	/* read interrupt mask register to clear
		any interrupt during suspend state */
	stm401_cmdbuff[0] = INTERRUPT_STATUS;
	stm401_i2c_write_read(ps_stm401, stm401_cmdbuff, 1, 2);

	if ((ps_stm401->ap_stm401_handoff_enable)
		&& (ps_stm401->ap_stm401_handoff_ctrl)) {
		do {
			usleep_range(STM401_BUSY_SLEEP_USEC,
					 STM401_BUSY_SLEEP_USEC);
			level = gpio_get_value(stm401_busy);
			count++;
		} while ((level) && (count < STM401_BUSY_RESUME_COUNT));

		if (count == STM401_BUSY_RESUME_COUNT)
			dev_err(&ps_stm401->client->dev,
				"timedout while waiting for STM401 BUSY LOW\n");
	}
	ps_stm401->ap_stm401_handoff_enable = false;

	mutex_unlock(&ps_stm401->lock);
	return 0;
}

static int stm401_suspend(struct i2c_client *client, pm_message_t mesg)
{
	struct stm401_data *ps_stm401 = i2c_get_clientdata(client);
	int count = 0, level = 0;
	int stm401_req = ps_stm401->pdata->gpio_mipi_req;
	int stm401_busy = ps_stm401->pdata->gpio_mipi_busy;
	dev_dbg(&ps_stm401->client->dev, "stm401_suspend\n");
	mutex_lock(&ps_stm401->lock);

	if ((ps_stm401->ap_stm401_handoff_enable)
		 && (ps_stm401->ap_stm401_handoff_ctrl)) {

		gpio_set_value(stm401_req, 1);
		dev_dbg(&ps_stm401->client->dev, "STM401 REQ is set %d\n",
			 gpio_get_value(stm401_req));

		do {
			usleep_range(STM401_BUSY_SLEEP_USEC,
				STM401_BUSY_SLEEP_USEC);
			level = gpio_get_value(stm401_busy);
			count++;
		} while ((!level) && (count < STM401_BUSY_SUSPEND_COUNT));

		if (count == STM401_BUSY_SUSPEND_COUNT)
			dev_err(&ps_stm401->client->dev,
				"timedout while waiting for STM401 BUSY HIGH\n");
	}

	mutex_unlock(&ps_stm401->lock);
	return 0;
}

#ifdef CONFIG_HAS_EARLYSUSPEND
static void stm401_early_suspend(struct early_suspend *handler)
{
	struct stm401_data *ps_stm401;

	ps_stm401 = container_of(handler, struct stm401_data, early_suspend);
	stm401_suspend(ps_stm401->client, PMSG_SUSPEND);
}

static void stm401_late_resume(struct early_suspend *handler)
{
	struct stm401_data *ps_stm401;

	ps_stm401 = container_of(handler, struct stm401_data, early_suspend);
	stm401_resume(ps_stm401->client);
}
#endif

static const struct i2c_device_id stm401_id[] = {
	{NAME, 0},
	{},
};

MODULE_DEVICE_TABLE(i2c, stm401_id);

#ifdef CONFIG_OF
static struct of_device_id stm401_match_tbl[] = {
	{ .compatible = "stm,stm401" },
	{ },
};
MODULE_DEVICE_TABLE(of, stm401_match_tbl);
#endif

static struct i2c_driver stm401_driver = {
	.driver = {
		   .name = NAME,
		   .owner = THIS_MODULE,
		   .of_match_table = of_match_ptr(stm401_match_tbl),
		   },
	.probe = stm401_probe,
	.remove = stm401_remove,
#ifndef CONFIG_HAS_EARLYSUSPEND
	.resume = stm401_resume,
	.suspend = stm401_suspend,
#endif
	.id_table = stm401_id,
};

static int __init stm401_init(void)
{
	return i2c_add_driver(&stm401_driver);
}

static void __exit stm401_exit(void)
{
	i2c_del_driver(&stm401_driver);
	return;
}

module_init(stm401_init);
module_exit(stm401_exit);

MODULE_DESCRIPTION("STM401 sensor processor");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
