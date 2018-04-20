/*
 * Copyright (C) 2013 Cypress, Inc.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/input.h>
#include <linux/delay.h>
#include <linux/module.h>
#include <linux/interrupt.h>
#include <linux/debugfs.h>
#include <linux/gpio.h>
#include <linux/of_gpio.h>
#include <linux/slab.h>
#include "cyttsp_sar.h"
#include <linux/firmware.h>
#include <linux/regulator/consumer.h>
#include <linux/notifier.h>
#include <linux/kernel.h>
#include <linux/sensors.h>
#include <linux/hssp_programmer.h>
#include "cycapsense_hssp.h"
#include "ProgrammingSteps.h"
#include <linux/notifier.h>
#include <linux/usb.h>
#include <linux/power_supply.h>

#define CYTTSP_DEBUG 0
#define LOG_TAG "cyttsp "
#if CYTTSP_DEBUG
#define LOG_INFO(fmt, args...)    pr_info(LOG_TAG fmt, ##args)
#else
#define LOG_INFO(fmt, args...)
#endif
#define LOG_ERR(fmt, args...)    pr_info(LOG_TAG fmt, ##args)


struct cycapsense_ctrl_data *ctrl_data;
static int programming_done;
static int fw_dl_status;
static BLOCKING_NOTIFIER_HEAD(hssp_notifier_list);

pcyttsp_data_t pcyttsp_sar_ptr;
struct sensors_classdev sensors_capsensor_cdev[4];

static int cyttsp_i2c_read_block(struct device *dev, u8 addr,
		u8 len, void *data)
{
	struct i2c_client *client = to_i2c_client(dev);
	int ret;

	ret = i2c_master_send(client, &addr, 1);
	if (ret != 1) {
		dev_err(dev, "Failed to read block!\n");
		return -EIO;
	}

	ret = i2c_master_recv(client, data, len);

	return (ret < 0) ? ret : ret != len ? -EIO : 0;
}

static u8 cyttsp_read_reg(struct cyttsp_sar_data *data, u8 reg)
{
	int ret;
	u8 val;
	u8 buffer[2];
	struct device *dev = &data->client->dev;
	struct i2c_client *client = to_i2c_client(dev);
	struct cyttsp_sar_platform_data *pdata = data->pdata;

	mutex_lock(&pdata->i2c_mutex);
	ret = cyttsp_i2c_read_block(&data->client->dev,
			reg, 1, &val);
	if (ret < 0) {
		dev_err(&data->client->dev, "Failed to read reg!\n");
		disable_irq_nosync(gpio_to_irq(pdata->irq_gpio));
		dev_err(&data->client->dev, "irq disable before reset chip!\n");
		gpio_direction_output(ctrl_data->hssp_d.rst_gpio, 0);
		dev_err(&data->client->dev, "direction out, value = 0\n");
		msleep(1);
		gpio_set_value(ctrl_data->hssp_d.rst_gpio, 1);
		dev_err(&data->client->dev, "direction out, value = 1\n");
		msleep(100);
		if (data->enable == true) {
			buffer[0] = CYTTSP_NORMAL_MODE;
			buffer[1] = 0x00;
			ret = i2c_master_send(client, buffer, sizeof(buffer));
			if (ret != sizeof(buffer))
				dev_err(&data->client->dev, "enable channel after reset chip failed\n");
			buffer[0] = CYTTSP_SAR_CHANNEL_ENABLE;
			buffer[1] = 0x0f;
			ret = i2c_master_send(client, buffer, sizeof(buffer));
			if (ret != sizeof(buffer))
				dev_err(&data->client->dev, "enable channel after reset chip failed\n");
		}
		enable_irq(gpio_to_irq(pdata->irq_gpio));
		dev_err(&data->client->dev, "irq enable after reset chip!\n");
		mutex_unlock(&pdata->i2c_mutex);
		dev_err(&data->client->dev, "mutex unlock after reset chip!\n");
		return ret;
	}
	mutex_unlock(&pdata->i2c_mutex);
	return val;
}

static int cyttsp_write_reg(struct cyttsp_sar_data *data,
		u8 reg, u8 val)
{
	int ret;
	u8 buffer[2];
	struct device *dev = &data->client->dev;
	struct i2c_client *client = to_i2c_client(dev);
	struct cyttsp_sar_platform_data *pdata = data->pdata;

	buffer[0] = reg;
	buffer[1] = val;

	mutex_lock(&pdata->i2c_mutex);
	ret = i2c_master_send(client, buffer, sizeof(buffer));
	if (ret != sizeof(buffer)) {
		dev_err(&data->client->dev, "Failed to write %x reg!\n", buffer[0]);
		disable_irq_nosync(gpio_to_irq(pdata->irq_gpio));
		dev_err(&data->client->dev, "irq disable before reset chip!\n");
		gpio_direction_output(ctrl_data->hssp_d.rst_gpio, 0);
		dev_err(&data->client->dev, "direction out, value = 0\n");
		msleep(1);
		gpio_set_value(ctrl_data->hssp_d.rst_gpio, 1);
		dev_err(&data->client->dev, "direction out, value = 1\n");
		msleep(100);
		if (data->enable == true) {
			buffer[0] = CYTTSP_NORMAL_MODE;
			buffer[1] = 0x00;
			ret = i2c_master_send(client, buffer, sizeof(buffer));
			if (ret != sizeof(buffer))
				dev_err(&data->client->dev, "enable channel after reset chip failed\n");
			buffer[0] = CYTTSP_SAR_CHANNEL_ENABLE;
			buffer[1] = 0x0f;
			ret = i2c_master_send(client, buffer, sizeof(buffer));
			if (ret != sizeof(buffer))
				dev_err(&data->client->dev, "enable channel after reset chip failed\n");
		}
		enable_irq(gpio_to_irq(pdata->irq_gpio));
		dev_err(&data->client->dev, "irq enable after reset chip!\n");
		mutex_unlock(&pdata->i2c_mutex);
		dev_err(&data->client->dev, "mutex unlock after reset chip!\n");
		return ret;
	}
	mutex_unlock(&pdata->i2c_mutex);

	return 0;
}

static int cycapsense_validate_gpio(struct device *dev,
		int gpio, char *name, int dir_out, int out_val)
{
	int error;
	error = devm_gpio_request(dev, gpio, name);
	if (error) {
		dev_err(dev, "unable to request gpio [%d]\n", gpio);
		goto f_end;
	}
	if (dir_out == 2)
		goto f_end;

	if (dir_out == 1)
		error = gpio_direction_output(gpio, out_val);
	else
		error = gpio_direction_input(gpio);
	if (error) {
		dev_err(dev, "unable to set direction for gpio [%d]\n", gpio);
		goto f_end;
	}
f_end:
	return error;
}

#ifdef CONFIG_OF
static void cyttsp_sar_dump_value(struct device *dev,
		struct cyttsp_sar_platform_data *pdata)
{

	dev_info(dev, "irq gpio = %d\n", pdata->irq_gpio);
	dev_info(dev, "irqflags = %d\n", (int)pdata->irqflags);
	dev_info(dev, "input name = %s\n", pdata->input_name);
	dev_info(dev, "sar number = %d\n", pdata->nsars);
	dev_info(dev, "sar_status_reg = 0x%x\n", pdata->sar_status_reg);
	dev_info(dev, "standby_reg = 0x%x\n", pdata->standby_reg);
	dev_info(dev, "default_config = %d\n", pdata->default_config);
}

static int cyttsp_sar_parse_dt(struct device *dev,
		struct cyttsp_sar_platform_data *pdata)
{
	int ret, error;
	struct device_node *np = dev->of_node;
	u32 temp_val;
	unsigned int addr = 0, size = 0;
	unsigned int gpios[12];

	/* swd reset */
	dev_err(dev, "cycapsense_prog_probe reqeust reset gpio\n");
	ctrl_data->hssp_d.rst_gpio = of_get_gpio(np, 0);
	error = cycapsense_validate_gpio(dev,
			ctrl_data->hssp_d.rst_gpio, "capsense_reset_gpio", 1, 0);
	if (error)
		return error;

	gpio_export(ctrl_data->hssp_d.rst_gpio, false);

	dev_err(dev, "cycapsense_prog_probe clk gpio\n");
	ctrl_data->hssp_d.c_gpio = of_get_gpio(np, 1);
	/*request only, direction == 2. Will be set by firmware loader*/
	error = cycapsense_validate_gpio(dev,
			ctrl_data->hssp_d.c_gpio, "capsense_sclk_gpio", 2, 0);
	if (error)
		return error;

	dev_err(dev, "cycapsense_prog_probe data gpio\n");
	ctrl_data->hssp_d.d_gpio = of_get_gpio(np, 2);
	/*request only, direction == 2. Will be set by firmware loader*/
	error = cycapsense_validate_gpio(dev,
			ctrl_data->hssp_d.d_gpio, "capsense_sdata_gpio", 2, 0);
	if (error)
		return error;

	/*irq*/
	pdata->irq_gpio = of_get_gpio(np, 3);

	error = of_property_read_string(np, "label", &ctrl_data->hssp_d.name);
	if (error) {
		dev_err(dev, "Error reading node label\n");
		return error;
	}
	error = of_property_read_u32(np, "silicon_id",
			&ctrl_data->hssp_d.silicon_id);
	if (error) {
		dev_err(dev, "Error reading node silicon id\n");
		return error;
	}
	error = of_property_read_u32(np, "block_size",
			&ctrl_data->hssp_d.block_size);
	if (error) {
		dev_err(dev, "Error reading node block size\n");
		return error;
	}
	error = of_property_read_u32(np, "num_of_blocks",
			&ctrl_data->hssp_d.num_of_blocks);
	if (error) {
		dev_err(dev, "Error reading node num_of_blocks\n");
		return error;
	}
	error = of_property_read_u32(np, "secure_bytes",
			&ctrl_data->hssp_d.secure_bytes);
	if (error) {
		dev_err(dev, "Error reading node secure_bytes\n");
		return error;
	}
	of_property_read_u32(np, "use-raw-gpio-rw",
			&ctrl_data->hssp_d.raw_mode);
	if (ctrl_data->hssp_d.raw_mode) {

		of_property_read_u32(np, "gpio_base_addr", &addr);
		of_property_read_u32(np, "gpio_base_size", &size);
		memset(gpios, 0, sizeof(gpios));
		of_property_read_u32_array(np, "gpios", gpios,
				ARRAY_SIZE(gpios));
		if (addr && size && gpios[CLK_GPIO] && gpios[DAT_GPIO]) {
			ctrl_data->hssp_d.gpio_base =
				devm_ioremap(dev, addr, size);
			dev_info(dev, "gpio base addr = %p\n", ctrl_data->hssp_d.gpio_base);
			ctrl_data->hssp_d.c_offset = gpios[CLK_GPIO];
			ctrl_data->hssp_d.d_offset = gpios[DAT_GPIO];
			dev_info(dev, "Raw GPIO (%d:%d) RW enabled\n",
					ctrl_data->hssp_d.c_offset,
					ctrl_data->hssp_d.d_offset);
			error = of_property_read_u32(np, "gpio_clk_group", &ctrl_data->hssp_d.c_group_offset);
			if (error)
				ctrl_data->hssp_d.c_group_offset = 0x00;
			error = of_property_read_u32(np, "gpio_dat_group", &ctrl_data->hssp_d.d_group_offset);
			if (error)
				ctrl_data->hssp_d.d_group_offset = 0x00;
			dev_info(dev, "GPIO offset (0x%x, 0x%x)  enabled\n",
					ctrl_data->hssp_d.c_group_offset,
					ctrl_data->hssp_d.d_group_offset);
		} else {
			ctrl_data->hssp_d.raw_mode = 0;
			dev_info(dev, "Raw GPIO RW enabled but regs are not defined.Switch to normal mode\n");
		}
	}


	ret = of_property_read_u32(np, "cyttsp,sar-status-reg",
			&temp_val);
	if (ret) {
		dev_err(dev, "Unable to read fw sar-status-reg\n");
		return ret;
	} else
		pdata->sar_status_reg = (u8)temp_val;

	pdata->cut_off_power = 0;

	ret = of_property_read_u32(np, "cyttsp,standby-reg",
			&temp_val);
	if (ret) {
		dev_err(dev, "Unable to read standby reg\n");
		return ret;
	} else
		pdata->standby_reg = (u8)temp_val;

	ret = of_property_read_u32(np, "cyttsp,bootloader-addr",
			&temp_val);
	if (ret)
		dev_err(dev, "Unable to read bootloader address\n");
	else
		pdata->bootloader_addr = (u8)temp_val;

	ret = of_property_read_u32(np, "cyttsp,key-num", &temp_val);
	if (ret) {
		dev_err(dev, "Unable to read key num\n");
		return ret;
	} else
		pdata->nsars = temp_val;

	ret = of_property_read_u32(np, "cyttsp,default-config",
			&pdata->default_config);
	if (ret) {
		dev_err(dev, "Unable to get default config\n");
		pdata->default_config = -1;
	}

	cyttsp_sar_dump_value(dev, pdata);

	return 0;
}
#else
static int cyttsp_sar_parse_dt(struct device *dev,
		struct cyttsp_sar_platform_data *pdata)
{
	return -ENODEV;
}
#endif


static irqreturn_t cyttsp_sar_eint_func(int irq, void *dev_id)
{
	struct cyttsp_sar_data *data = dev_id;
	struct cyttsp_sar_platform_data *pdata = data->pdata;

	disable_irq_nosync(gpio_to_irq(pdata->irq_gpio));
	wake_lock_timeout(&data->cap_lock, msecs_to_jiffies(100));
	schedule_delayed_work(&data->eint_work, 0);

	return IRQ_HANDLED;
}


static void cyttsp_sar_eint_work(struct work_struct *work)
{
	struct cyttsp_sar_data *data = pcyttsp_sar_ptr;
	struct cyttsp_sar_platform_data *pdata = data->pdata;
	u8 temp[3];
	u32 sar_state = 0;
	u32 sensor_status = 0;
	int ret ;
	int i;
	u32 channel_status_mask[CYTTSP_SAR_CHANNEL_MAX] = {
		0x00000003,
		0x0000000C,
		0x00000030,
		0x000000C0,
		0x00000300,
		0x00000C00,
		0x00003000,
		0x0000C000  };

	LOG_INFO("%s\n", __func__);

	if (data->enable) {
		ret = cyttsp_i2c_read_block(&data->client->dev, CYTTSP_REG_INTERRUPT_PEDNING, 3, &temp[0]);
		if (ret < 0) {
			dev_err(&data->client->dev, "Failed to read interrupt pending regsiter!\n");
			enable_irq(gpio_to_irq(pdata->irq_gpio));
			dev_err(&data->client->dev, "enable irq in interrupt\n");
			return;
		}

		sensor_status = ((unsigned long)temp[1] +  (unsigned long)(temp[2] << 8));
		for (i = 0; i < pdata->nsars; i++) {

			/* Report event only if sensor status for a specific channel has changed
			   and it matches the channel position in interrupt pending register */
			if (((sensor_status ^ data->sensorStatus) & channel_status_mask[i])) {
				/* Find the sar state for the channel */
				sar_state = (sensor_status & channel_status_mask[i]) >> (i * 2);

				if (sar_state < CYTTSP_SAR_STATE_ERROR) {
					LOG_INFO("update channel%d status : %x\n", i, sar_state);
					input_report_abs(data->input_dev[i], ABS_DISTANCE, sar_state);
					input_sync(data->input_dev[i]);
				} else {
					dev_err(&data->client->dev, "sar sensor error detected, Need reset!\n");
				}
			}
		}

	}

	data->sensorStatus = sensor_status;

	enable_irq(gpio_to_irq(pdata->irq_gpio));

}

/**
 * brief  Initialize I2C config from platform data
 * param this Pointer to main parent struct
 *
 */
static void hw_init(void)
{
	struct cyttsp_sar_data *data = pcyttsp_sar_ptr;

	struct cyttsp_sar_platform_data *pdata = data->pdata;
	int i = 0;
	int ret;
	/* configure device */
	LOG_INFO("Going to Setup I2C Registers\n");
	while (i < pdata->i2c_reg_num) {
		/* Write all registers/values contained in i2c_reg */
		LOG_INFO("Going to Write Reg: 0x%x Value: 0x%x\n",
				pdata->pi2c_reg[i].reg, pdata->pi2c_reg[i].val);
		ret = cyttsp_write_reg(data, pdata->pi2c_reg[i].reg, pdata->pi2c_reg[i].val);
		if (ret < 0)
			LOG_ERR("reg write failed\n");
		i++;
	}

	LOG_INFO("i2c reg set done\n");
}

static void cycapsense_hssp_notify(int status)
{
	programming_done = status;
	blocking_notifier_call_chain(&hssp_notifier_list,
			programming_done, NULL);
}

int cycapsense_hssp_dnld(struct hssp_data *d)
{
	cycapsense_hssp_notify(HSSP_START);

	if (d->inf.data == NULL || d->inf.s_data == NULL) {
		pr_err("%s: Invalid input arguments\n", __func__);
		return -EINVAL;
	}

	if (DeviceAcquire() == FAILURE) {
		pr_err("%s: Device Acquire failed\n", __func__);
		return -EIO;
	}

	if (VerifySiliconId(d) == FAILURE) {
		pr_err("%s: Verify Selicon ID failed\n", __func__);
		return -EIO;
	}

	if (EraseAllFlash(d) == FAILURE) {
		pr_err("%s: Flash Erase failed\n", __func__);
		return -EIO;
	}

	if (ChecksumPrivileged(d) == FAILURE) {
		pr_err("%s: Checksum Privileged failed\n", __func__);
		return -EIO;
	}

	if (ProgramFlash(d) == FAILURE) {
		pr_err("%s: Program Flash failed\n", __func__);
		return -EIO;
	}

	if (VerifyFlash(d) == FAILURE) {
		pr_err("%s: Verify Flash failed\n", __func__);
		return -EIO;
	}

	if (ProgramProtectionSettings(d) == FAILURE) {
		pr_err("%s: Program Protection failed\n", __func__);
		return -EIO;
	}

	if (VerifyProtectionSettings(d) == FAILURE) {
		pr_err("%s: Verify Protection failed\n", __func__);
		return -EIO;
	}

	if (VerifyChecksum(d) == FAILURE) {
		pr_err("%s: Verify Checksum failed\n", __func__);
		return -EIO;
	}

	ExitProgrammingMode();
	msleep(1000);
	cycapsense_hssp_notify(HSSP_STOP);
	return SUCCESS;
}

const unsigned char ascii2bin[] = {
	0x00, 0x01, 0x02, 0x03, 0x04, 0x05, 0x06, 0x07,
	0x08, 0x09, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00, 0x00,
	0x00, 0x0A, 0x0B, 0x0C, 0x0D, 0x0E, 0x0F
};
#define a_2_b(a)	(ascii2bin[((a) & 0xff) - 0x30])
#define a_a_2_byte(a1, a2)	((a_2_b(a1) << 4) | a_2_b(a2))

/* this simplifyied version of Intel HEX, Cypress PSoC specific*/
int cycapsense_hssp_parse_hex(struct hssp_data *d, const u8 *src, int len)
{
	enum r_type { REC_DATA_T = 0, REC_EOF_T = 1, REC_EXT_L_ADDR_T = 4 };
	struct { int len; enum r_type type; int e_addr; } rec = { 0, 0, 0 };
	enum parser_state { REC_START, REC_LEN, REC_ADDR, REC_TYPE, REC_DATA,
		REC_CS } state = REC_START;
	int i = 0, j = 0, cnt;
	u16 cs = 0, f_cs;
	u8 *dst;
	u8 rec_cs = 0, val;

	if (d == NULL) {
		pr_err("%s: ID is not defined.\n", __func__);
		return -EINVAL;
	}

	cnt = d->block_size * d->num_of_blocks;
	d->inf.data = kzalloc(cnt, GFP_KERNEL);
	if (d->inf.data == NULL) {
		pr_err("%s: Error kzalloc (%d).\n", __func__, cnt);
		return -ENOMEM;
	}
	d->inf.s_data = kzalloc(d->secure_bytes, GFP_KERNEL);
	if (d->inf.s_data == NULL) {
		pr_err("%s: Error kzalloc (%d).\n", __func__, d->secure_bytes);
		kfree(d->inf.data);
		return -ENOMEM;
	}
	d->inf.m_data = kzalloc(P4000_METADATA_SIZE, GFP_KERNEL);
	if (d->inf.m_data == NULL) {
		pr_err("%s: Error kzalloc (%d).\n", __func__,
				P4000_METADATA_SIZE);
		kfree(d->inf.data);
		return -ENOMEM;
	}
	dst = d->inf.data; /*data always first in PSoC HEX*/
	while (i < len) {
		switch (state) {
		case REC_START:
			if (src[i] == '\r' || src[i] == '\n')
				i++;
			else if (src[i++] != ':')
				goto parse_hex_end_err;
			else
				state = REC_LEN;
			break;
		case REC_LEN:
			if (len - i < 2)
				goto parse_hex_end_err;
			rec.len = a_a_2_byte(src[i], src[i+1]);
			i += 2;
			rec_cs += rec.len;
			state = REC_ADDR;
			break;
		case REC_ADDR:
			if (len - i < 4)
				goto parse_hex_end_err;
			/* addr is not used by PSoC, only for check sum */
			val = a_a_2_byte(src[i], src[i+1]);
			rec_cs += val;
			val = a_a_2_byte(src[i+2], src[i+3]);
			rec_cs += val;
			i += 4;
			state = REC_TYPE;
			break;
		case REC_TYPE:
			if (len - i < 2)
				goto parse_hex_end_err;
			rec.type = a_a_2_byte(src[i], src[i+1]);
			i += 2;
			rec_cs += rec.type;
			switch (rec.type) {
			case REC_DATA_T:
				state = REC_DATA;
				break;
			case REC_EXT_L_ADDR_T:
				if (len - i < 4)
					goto parse_hex_end_err;
				val = a_a_2_byte(src[i], src[i+1]);
				rec_cs += val;
				rec.e_addr = val << 8;
				val = a_a_2_byte(src[i+2], src[i+3]);
				i += 4;
				rec_cs += val;
				rec.e_addr |= val;
				if (rec.e_addr == 0x9030) {
					/* Checksum */
					dst = (u8 *)&f_cs;
					d->inf.cs = cs;
					j = 0;
					cnt = 2;
				} else if (rec.e_addr == 0x9040) {
					/* Flash protection */
					if (j > cnt) {
						pr_err("%s: Too big for %s.\n",
								__func__, d->name);
						goto parse_hex_end_err;
					}
					dst = d->inf.s_data;
					cnt = d->secure_bytes;
					j = 0;
				} else if (rec.e_addr == 0x9050) {
					/* Metadata */
					j = 0;
					dst = d->inf.m_data;
					cnt = P4000_METADATA_SIZE;
				} else if (rec.e_addr == 0x9060) {
					/* Chip level protection */
					dst = (u8 *)&d->inf.cl_protection;
					j = 0;
					cnt = 1;
				} else {
					pr_err("%s: Unsupported ext addr 0x%x\n",
							__func__, rec.e_addr);
					goto parse_hex_end_err;
				}
				state = REC_CS;
				break;
			case REC_EOF_T:
				if (be16_to_cpu(f_cs) != d->inf.cs) {
					pr_err("%s: Data checksum failed 0x%x - 0x%x\n",
							__func__, be16_to_cpu(f_cs),
							d->inf.cs);
					goto parse_hex_end_err;
				}
				state = REC_CS;
				break;
			default: /* Cypress supports only types above */
				pr_err("%s: Unsupported record type\n",
						__func__);
				goto parse_hex_end_err;
			}
			break;
		case REC_DATA:
			if (len - i < rec.len * 2)
				goto parse_hex_end_err;
			for (; rec.len; rec.len--, i += 2, dst++, j++) {
				val = a_a_2_byte(src[i], src[i+1]);
				if (j < cnt) {
					*dst = val;
					cs += val;
				}
				rec_cs += val;
			}
			state = REC_CS;
			break;
		case REC_CS:
			if (len - i < 2)
				goto parse_hex_end_err;
			val = a_a_2_byte(src[i], src[i+1]);
			i += 2;
			rec_cs += val;
			if (rec_cs != 0) {
				pr_err("%s: Record check sum failed\n",
						__func__);
				goto parse_hex_end_err;
			}
			state = REC_START;
			break;
		}
	}
	return 0;
parse_hex_end_err:
	pr_err("%s: HEX format error.\n", __func__);
	kfree(d->inf.data);
	d->inf.data = NULL;
	kfree(d->inf.s_data);
	d->inf.s_data = NULL;
	kfree(d->inf.m_data);
	d->inf.m_data = NULL;
	return -EINVAL;
}


int __cycapsense_reset(struct cycapsense_ctrl_data *data)
{
	int status = programming_done;

	if (status == HSSP_STOP)
		cycapsense_hssp_notify(HSSP_START);

	dev_info(data->dev, "Reset requested\n");
	device_lock(data->dev);
	ExitProgrammingMode();
	msleep(1000);

	if (status == HSSP_STOP)
		cycapsense_hssp_notify(HSSP_STOP);

	device_unlock(data->dev);
	return 0;
}

static int cycapsense_hssp_erase(struct hssp_data *d)
{
	cycapsense_hssp_notify(HSSP_START);

	device_lock(ctrl_data->dev);
	if (DeviceAcquire() == FAILURE) {
		pr_err("%s: Device Acquire failed\n", __func__);
		return -EIO;
	}

	if (EraseAllFlash(d) == FAILURE) {
		pr_err("%s: Flash Erase failed\n", __func__);
		return -EIO;
	}
	device_unlock(ctrl_data->dev);
	d->chip_cs = 0;
	pr_err("%s: Flash Erase successful\n", __func__);
	return SUCCESS;
}


static int cycapsense_hssp_verify_swrev(struct hssp_data *d)
{
	int ret;

	cycapsense_hssp_notify(HSSP_START);

	if (DeviceAcquire() == FAILURE) {
		pr_err("%s: Device Acquire failed\n", __func__);
		ret = -EIO;
		goto end;
	}

	if (VerifySiliconId(d) == FAILURE) {
		pr_err("%s: Verify Selicon ID failed\n", __func__);
		ret = -EIO;
		goto end;
	}

	ret = VerifySwRevision(d);

end:
	ExitProgrammingMode();
	msleep(1000);
	cycapsense_hssp_notify(HSSP_STOP);
	return ret;
}

int __cycapsense_fw_update(struct cycapsense_ctrl_data *data)
{
	const struct firmware *fw = NULL;
	char *fw_name = NULL;
	struct hex_info *inf;
	int error = 0;

	inf = &data->hssp_d.inf;

	device_lock(data->dev);

	fw_name = kzalloc(NAME_MAX, GFP_KERNEL);
	if (fw_name == NULL) {
		dev_err(data->dev, "No memory for FW name\n");
		error = -ENOMEM;
		goto fw_upd_end;
	}
	if (inf->fw_name[0] == 0) {
		strlcpy(fw_name, data->hssp_d.name, NAME_MAX);
		strlcat(fw_name, ".hex", NAME_MAX);
	} else {
		strlcpy(fw_name, inf->fw_name, NAME_MAX);
	}
	if (request_firmware(&fw, fw_name, data->dev) < 0 || !fw
			|| !fw->data || !fw->size) {
		dev_err(data->dev, "Unable to get firmware %s\n", fw_name);
		error = -ENOENT;
		goto fw_upd_end;
	}
	error = cycapsense_hssp_parse_hex(&data->hssp_d, fw->data,
			fw->size);
	if (error)
		goto fw_upd_end;

	if (cycapsense_hssp_verify_swrev(&data->hssp_d) != SUCCESS) {

		/* force update regardless check sum, if user requested */
		dev_info(data->dev, "Flashing firmware %s\n", fw_name);
		fw_dl_status = 2;
		error = cycapsense_hssp_dnld(&data->hssp_d);
		if (!error) {
			dev_info(data->dev, "%s flashed successful\n",
					fw_name);
			fw_dl_status = 0;
		} else
			fw_dl_status = 1;
	} else {
		data->hssp_d.chip_cs = data->hssp_d.inf.cs;
		dev_info(data->dev,
				"SW rev is matching. No firmware upgrade.\n");
		fw_dl_status = 0;
	}

	error = cyttsp_write_reg(pcyttsp_sar_ptr, CYTTSP_SAR_OP_MODE, 0x01);
	if (error < 0)
		LOG_ERR("reg write failed\n");
	pcyttsp_sar_ptr->enable = false;

fw_upd_end:
	if (inf->data != NULL) {
		kfree(inf->data);
		inf->data = NULL;
	}
	if (inf->s_data != NULL) {
		kfree(inf->s_data);
		inf->s_data = NULL;
	}
	if (fw != NULL)
		release_firmware(fw);
	if (fw_name != NULL)
		kfree(fw_name);

	device_unlock(data->dev);
	return error;
}

static int capsensor_set_enable(struct sensors_classdev *sensors_cdev, unsigned int enable)
{
	struct cyttsp_sar_data *data = pcyttsp_sar_ptr;
	struct cyttsp_sar_platform_data *pdata = data->pdata;
	struct input_dev *input;
	int i, ret;

	if (enable == 1) {
		LOG_INFO("enable cap sensor\n");
		hw_init();
		for (i = 0; i < pdata->nsars; i++) {
			input = data->input_dev[i];
			input_report_abs(input, ABS_DISTANCE, 0);
			input_sync(input);
		}
		data->enable = true;
	} else if (enable == 0) {
		LOG_INFO("disable cap sensor\n");
		ret = cyttsp_write_reg(data, CYTTSP_SAR_OP_MODE, 0x01);
		if (ret < 0)
			LOG_ERR("reg write failed\n");

		for (i = 0; i < pdata->nsars; i++) {
			input = data->input_dev[i];
			input_report_abs(input, ABS_DISTANCE, -1);
			input_sync(input);
		}
		data->enable = false;
	} else {
		LOG_ERR("unknown enable symbol\n");
	}

	return 0;
}


/**
 *fn static void cyttsp_reg_setup_init(struct i2c_client *client)
 *brief read reg val form dts
 *      reg_array_len for regs needed change num
 *      data_array_val's format <reg val ...>
 */
static void cyttsp_reg_setup_init(struct i2c_client *client)
{
	u32 data_array_len = 0;
	u32 *data_array;
	int ret, i, j;
	struct device_node *np = client->dev.of_node;

	ret = of_property_read_u32(np, "reg_array_len", &data_array_len);
	if (ret < 0) {
		LOG_ERR("data_array_len read error");
		return;
	}
	data_array = kmalloc(data_array_len * 2 * sizeof(u32), GFP_KERNEL);
	ret = of_property_read_u32_array(np, "reg_array_val",
			data_array,
			data_array_len*2);
	if (ret < 0) {
		LOG_ERR("data_array_val read error");
		return;
	}
	for (i = 0; i < ARRAY_SIZE(cyttsp_i2c_reg_setup); i++) {
		for (j = 0; j < data_array_len*2; j += 2) {
			if (data_array[j] == cyttsp_i2c_reg_setup[i].reg)
				cyttsp_i2c_reg_setup[i].val = data_array[j+1];
		}
	}
	kfree(data_array);
}

static int cyttsp_initialize_regulator(struct cyttsp_sar_data *data)
{
	int ret = -1;
	struct i2c_client *client = data->client;

	data->regulator_vdd = devm_regulator_get(&client->dev, "vdd");
	if (IS_ERR(data->regulator_vdd)) {
		dev_err(&client->dev,
				"%s: regulator_get failed: %ld\n",
				__func__, PTR_ERR(data->regulator_vdd));
		goto err_null_regulator;
	}

	ret = regulator_enable(data->regulator_vdd);
	if (ret < 0) {
		dev_err(&client->dev,
				"%s: regulator_enable failed: %d\n",
				__func__, ret);
		goto err_null_regulator;
	}

	return 0;

err_null_regulator:
	data->regulator_vdd = NULL;
	return ret;
}

static void capsense_update_work(struct work_struct *w)
{
	struct cycapsense_ctrl_data *data =
		container_of(w, struct cycapsense_ctrl_data, work);

	switch (data->cmd) {
	case HSSP_CMD_RESET:
		__cycapsense_reset(data);
		break;
	case HSSP_CMD_ERASE:
		cycapsense_hssp_erase(&data->hssp_d);
		break;
	case HSSP_CMD_FW_UPDATE:
		__cycapsense_fw_update(data);
		break;
	}
	data->cmd = HSSP_CMD_NONE;
}

int cycapsense_reset(void)
{
	if (ctrl_data == NULL || ctrl_data->dev == NULL) {
		pr_err("%s: Ctrl data not initialized\n", __func__);
		return -ENODEV;
	}

	ctrl_data->cmd = HSSP_CMD_RESET;
	schedule_work(&ctrl_data->work);

	return 0;
}

static ssize_t cycapsense_fw_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, 16, "0x%x\n",
			ctrl_data && ctrl_data->hssp_d.chip_cs ?
			ctrl_data->hssp_d.sw_rev : 0);
}

static ssize_t cycapsense_fw_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	char *cp;

	if (!count)
		return -EINVAL;

	cp = memchr(buf, '\n', count);
	if (cp)
		*(char *)cp = 0;

	if (!strcmp(buf, "reset"))
		ctrl_data->cmd = HSSP_CMD_RESET;
	else if (!strcmp(buf, "erase"))
		ctrl_data->cmd = HSSP_CMD_ERASE;
	else {
		ctrl_data->cmd = HSSP_CMD_FW_UPDATE;
		if (!strcmp(buf, "1"))
			ctrl_data->hssp_d.inf.fw_name[0] = 0;
		else
			strlcpy(ctrl_data->hssp_d.inf.fw_name, buf, NAME_MAX);
	}

	schedule_work(&ctrl_data->work);

	return count;
}

static ssize_t cycapsense_reset_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	struct cyttsp_sar_data *data = pcyttsp_sar_ptr;
	struct cyttsp_sar_platform_data *pdata = data->pdata;
	struct input_dev *input;
	int i, ret;

	if (!count)
		return -EINVAL;

	if (!strncmp(buf, "reset", 5) || !strncmp(buf, "1", 1)) {
		LOG_INFO("Going to refresh baseline\n");
		ret = cyttsp_write_reg(data, CYTTSP_SAR_REFRESH_BASELINE, 0x01);
		if (ret < 0)
			LOG_ERR("reg write failed\n");

		for (i = 0; i < pdata->nsars; i++) {
			input = data->input_dev[i];
			input_report_abs(input, ABS_DISTANCE, 0);
			input_sync(input);
		}
	}

	return count;
}

static ssize_t cycapsense_set_threshold_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	struct cyttsp_sar_data *data = pcyttsp_sar_ptr;
	int i, ret;
	u32 threshold_array_len = 0;
	u32 *threshold_array_data;
	struct device_node *np = data->client->dev.of_node;

	if (!count)
		return -EINVAL;

	LOG_INFO("start set threshold, sku = %s\n", buf);

	ret = of_property_read_u32(np, "threshold_reg_array_len",
					&threshold_array_len);
	if (ret < 0) {
		LOG_ERR("data_array_len read error");
	}

	threshold_array_data = kmalloc(threshold_array_len * 2 * sizeof(u32),
					GFP_KERNEL);

	if (!strncmp(buf, "NA", 2)) {
		ret = of_property_read_u32_array(np, "na_threshold_array_val",
				threshold_array_data,
				threshold_array_len * 2);
		if (ret < 0)
			LOG_ERR("data_array_val read error");
	} else if (!strncmp(buf, "EMEA", 4)) {
		ret = of_property_read_u32_array(np, "emea_threshold_array_val",
				threshold_array_data,
				threshold_array_len * 2);
		if (ret < 0)
			LOG_ERR("data_array_val read error");
	} else if (!strncmp(buf, "APAC", 4)) {
		ret = of_property_read_u32_array(np, "apac_threshold_array_val",
				threshold_array_data,
				threshold_array_len * 2);
		if (ret < 0)
			LOG_ERR("data_array_val read error");
	} else if (!strncmp(buf, "LATAM", 5)) {
		ret = of_property_read_u32_array(np,
				"latam_threshold_array_val",
				threshold_array_data,
				threshold_array_len * 2);
		if (ret < 0)
			LOG_ERR("data_array_val read error");
	} else if (!strncmp(buf, "PRC", 3)) {
		ret = of_property_read_u32_array(np, "prc_threshold_array_val",
				threshold_array_data,
				threshold_array_len * 2);
		if (ret < 0) {
			LOG_ERR("data_array_val read error");
		}
	} else if (!strncmp(buf, "BRAZIL", 6)) {
		ret = of_property_read_u32_array(np, "brazil_threshold_array_val",
				threshold_array_data,
				threshold_array_len * 2);
		if (ret < 0) {
			LOG_ERR("data_array_val read error");
		}
	} else {
		ret = of_property_read_u32_array(np, "default_threshold_array_val",
				threshold_array_data,
				threshold_array_len * 2);
		if (ret < 0) {
			LOG_ERR("data_array_val read error");
		}
		LOG_ERR("radio is not expected, radio = %s", buf);
	}

	for (i = 0; i < threshold_array_len; i++) {
		LOG_INFO("Going to Write Reg: 0x%x Value: 0x%x\n",
				threshold_array_data[i*2],
				threshold_array_data[i*2 + 1]);

		ret = cyttsp_write_reg(data, threshold_array_data[i*2],
					threshold_array_data[i*2 + 1]);
		if (ret < 0)
			LOG_ERR("reg write failed");
	}


	return count;
}

static ssize_t cycapsense_enable_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct cyttsp_sar_data *data = pcyttsp_sar_ptr;

	return snprintf(buf, 8, "%d\n", (int)data->enable);

}

static ssize_t cycapsense_enable_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	struct cyttsp_sar_data *data = pcyttsp_sar_ptr;
	struct cyttsp_sar_platform_data *pdata = data->pdata;
	struct input_dev *input;
	int i, ret;

	if (!strncmp(buf, "1", 1)) {
		LOG_INFO("enable cap sensor\n");
		hw_init();
		for (i = 0; i < pdata->nsars; i++) {
			input = data->input_dev[i];
			input_report_abs(input, ABS_DISTANCE, 0);
			input_sync(input);
		}
		data->enable = true;
	} else if (!strncmp(buf, "0", 1)) {
		LOG_INFO("disable cap sensor\n");
		ret = cyttsp_write_reg(data, CYTTSP_SAR_OP_MODE, 0x01);
		if (ret < 0)
			LOG_ERR("reg write failed\n");

		for (i = 0; i < pdata->nsars; i++) {
			input = data->input_dev[i];
			input_report_abs(input, ABS_DISTANCE, -1);
			input_sync(input);
		}
		data->enable = false;
	} else {
		LOG_ERR("unknown enable symbol\n");
	}

	return count;
}

static ssize_t cycapsense_reg_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	struct cyttsp_sar_data *data = pcyttsp_sar_ptr;
	u8 reg_value, reg = 0x00;
	char *p = buf;

	while (reg <= 0x8a) {
		reg_value = cyttsp_read_reg(data, reg);
		p += snprintf(p, PAGE_SIZE, "ADDR(0x%02x)=0x%02x,\n", reg, reg_value);
		reg++;
	}
	return (p-buf);
}

static ssize_t cycapsense_reg_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	struct cyttsp_sar_data *data = pcyttsp_sar_ptr;
	unsigned int val, reg;
	int ret;

	if (sscanf(buf, "%x,%x", &reg, &val) == 2) {
		ret = cyttsp_write_reg(data, *((u8 *)&reg), *((u8 *)&val));
		if (ret < 0)
			LOG_ERR("reg write failed\n");
	}

	return count;
}

static ssize_t cycapsense_debug_store(struct class *class,
		struct class_attribute *attr,
		const char *buf, size_t count)
{
	struct cyttsp_sar_data *data = pcyttsp_sar_ptr;
	u8 val1, val2, reg = 0x00;
	u16 value1, value2;
	int i, flag = 0;

	if (!strncmp(buf, "0", 1)) {
		flag = 1;
		LOG_INFO("read channel 0 data\n");
		for (i = 0; i < 200; i++) {
			if (flag == 0)
				break;
			reg = 0x5b;
			val1 = cyttsp_read_reg(data, reg);
			reg = 0x5c;
			val2 = cyttsp_read_reg(data, reg);
			value1 = val2 << 8 | val1;
			reg = 0x7b;
			val1 = cyttsp_read_reg(data, reg);
			reg = 0x7c;
			val2 = cyttsp_read_reg(data, reg);
			value2 = val2 << 8 | val1;
			LOG_INFO("  raw = 0x%x,      signal = 0x%x\n", value1, value2);
			msleep(100);

		}
	} else if (!strncmp(buf, "1", 1)) {
		flag = 1;
		LOG_INFO("read channel 1 data\n");
		for (i = 0; i < 200; i++) {
			if (flag == 0)
				break;
			reg = 0x5d;
			val1 = cyttsp_read_reg(data, reg);
			reg = 0x5e;
			val2 = cyttsp_read_reg(data, reg);
			value1 = val2 << 8 | val1;
			reg = 0x7d;
			val1 = cyttsp_read_reg(data, reg);
			reg = 0x7e;
			val2 = cyttsp_read_reg(data, reg);
			value2 = val2 << 8 | val1;
			LOG_INFO("  raw = 0x%x,      signal = 0x%x\n", value1, value2);
			msleep(100);

		}
	} else if (!strncmp(buf, "2", 1)) {
		flag = 1;
		LOG_INFO("read channel 2 data\n");
		for (i = 0; i < 200; i++) {
			if (flag == 0)
				break;
			reg = 0x5f;
			val1 = cyttsp_read_reg(data, reg);
			reg = 0x60;
			val2 = cyttsp_read_reg(data, reg);
			value1 = val2 << 8 | val1;
			reg = 0x7f;
			val1 = cyttsp_read_reg(data, reg);
			reg = 0x80;
			val2 = cyttsp_read_reg(data, reg);
			value2 = val2 << 8 | val1;
			LOG_INFO("  raw = 0x%x,      signal = 0x%x\n", value1, value2);
			msleep(100);

		}
	} else if (!strncmp(buf, "3", 1)) {
		flag = 1;
		LOG_INFO("read channel 3 data\n");
		for (i = 0; i < 200; i++) {
			if (flag == 0)
				break;
			reg = 0x61;
			val1 = cyttsp_read_reg(data, reg);
			reg = 0x62;
			val2 = cyttsp_read_reg(data, reg);
			value1 = val2 << 8 | val1;
			reg = 0x81;
			val1 = cyttsp_read_reg(data, reg);
			reg = 0x82;
			val2 = cyttsp_read_reg(data, reg);
			value2 = val2 << 8 | val1;
			LOG_INFO("  raw = 0x%x,      signal = 0x%x\n", value1, value2);
			msleep(100);
		}
	} else if (!strncmp(buf, "4", 1)) {
		flag = 0;
	}

	return count;
}

static ssize_t cycapsense_fw_download_status_show(struct class *class,
		struct class_attribute *attr,
		char *buf)
{
	return snprintf(buf, 8, "%d", fw_dl_status);
}

static CLASS_ATTR(fw_update, 0660, cycapsense_fw_show, cycapsense_fw_store);
static CLASS_ATTR(set_threshold, 0660, NULL, cycapsense_set_threshold_store);
static CLASS_ATTR(reset, 0660, NULL, cycapsense_reset_store);
static CLASS_ATTR(enable, 0660, cycapsense_enable_show, cycapsense_enable_store);
static CLASS_ATTR(reg, 0660, cycapsense_reg_show, cycapsense_reg_store);
static CLASS_ATTR(debug, 0660, NULL, cycapsense_debug_store);
static CLASS_ATTR(fw_download_status, 0660, cycapsense_fw_download_status_show, NULL);

static void ps_notify_callback_work(struct work_struct *work)
{
	struct cyttsp_sar_data *data =
		container_of(work, struct cyttsp_sar_data, ps_notify_work);
	struct cyttsp_sar_platform_data *pdata = data->pdata;
	struct input_dev *input;
	int i, ret;

	LOG_INFO("Going to force calibrate\n");
	ret = cyttsp_write_reg(data, CYTTSP_SAR_REFRESH_BASELINE, 0x0f);
	if (ret < 0)
		LOG_ERR("reg write failed\n");

	for (i = 0; i < pdata->nsars; i++) {
		input = data->input_dev[i];
		input_report_abs(input, ABS_DISTANCE, 0);
		input_sync(input);
	}
}


static int ps_get_state(struct cyttsp_sar_data *data,
		struct power_supply *psy, bool *present)
{
	struct device *dev = &data->client->dev;
	union power_supply_propval pval = {0};
	int retval;

	retval = power_supply_get_property(psy, POWER_SUPPLY_PROP_PRESENT,
			&pval);
	if (retval) {
		dev_err(dev, "%s psy get property failed\n", psy->desc->name);
		return retval;
	}
	*present = (pval.intval) ? true : false;
	LOG_INFO("%s is %s\n", psy->name,
			(*present) ? "present" : "not present");
	return 0;
}

static int ps_notify_callback(struct notifier_block *self,
		unsigned long event, void *p)
{
	struct cyttsp_sar_data *data =
		container_of(self, struct cyttsp_sar_data, ps_notif);
	struct power_supply *psy = p;
	struct device *dev = NULL;
	bool present;
	int retval;

	if ((event == PSY_EVENT_PROP_CHANGED)
			&& psy && psy->desc->get_property && psy->desc->name &&
			!strncmp(psy->desc->name, "usb", sizeof("usb")) && data) {
		dev = &data->client->dev;
		LOG_INFO("ps notification: event = %lu\n", event);
		retval = ps_get_state(data, psy, &present);
		if (retval) {
			dev_err(dev, "psy get property failed\n");
			return retval;
		}

		if (event == PSY_EVENT_PROP_CHANGED) {
			if (data->ps_is_present == present) {
				LOG_INFO("ps present state not change\n");
				return 0;
			}
		}
		data->ps_is_present = present;
		schedule_work(&data->ps_notify_work);
	}

	return 0;
}

static struct class capsense_class = {
	.name                   = "capsense",
	.owner                  = THIS_MODULE,
};

static int cyttsp_sar_probe(struct i2c_client *client,
		const struct i2c_device_id *id)
{
	struct cyttsp_sar_platform_data *pdata;
	struct cyttsp_sar_data *data;
	struct power_supply *psy = NULL;
	int error, i;
	char *name;
	char *buffer;

	dev_info(&client->dev, "cycapsense_probe enter\n");
	pdata = devm_kzalloc(&client->dev,
			sizeof(struct cyttsp_sar_platform_data), GFP_KERNEL);
	if (!pdata) {
		dev_err(&client->dev, "Failed to allocate memroy for pdata\n");
		return -ENOMEM;
	}

	ctrl_data = devm_kzalloc(&client->dev,
			sizeof(struct cycapsense_ctrl_data), GFP_KERNEL);
	if (ctrl_data == NULL) {
		dev_err(&client->dev, "Failed to allocate memory for ctrl_data\n");
		return -ENOMEM;
	}

	error = cyttsp_sar_parse_dt(&client->dev, pdata);
	if (error)
		return error;
	client->irq = pdata->irq_gpio;
	mutex_init(&pdata->i2c_mutex);

	cyttsp_reg_setup_init(client);
	pdata->pi2c_reg = cyttsp_i2c_reg_setup;
	pdata->i2c_reg_num = ARRAY_SIZE(cyttsp_i2c_reg_setup);
	client->dev.platform_data = pdata;

	data = kzalloc(sizeof(struct cyttsp_sar_data), GFP_KERNEL);
	if (!data) {
		dev_err(&client->dev, "Failed to allocate memory for data!\n");
		return -ENOMEM;
	}

	data->client = client;
	data->pdata = pdata;
	pcyttsp_sar_ptr = data;
	ctrl_data->dev = &client->dev;

	error = cyttsp_initialize_regulator(data);
	if (error < 0) {
		dev_err(&client->dev, "Failed to initialize regulator\n");
		goto err_free_data;
	}
	i2c_set_clientdata(data->client, data);

	for (i = 0; i < pdata->nsars; i++) {
		/* Initialize input device */
		data->input_dev[i] = input_allocate_device();
		if (!data->input_dev[i]) {
			dev_err(&client->dev, "Failed to allocate input device\n");
			return -ENOMEM;
		}
		name = kasprintf(GFP_KERNEL, "cyttsp_sar_ch%d", i);

		data->input_dev[i]->name = name;

		/* Set all the keycodes */
		__set_bit(EV_ABS, data->input_dev[i]->evbit);
		input_set_abs_params(data->input_dev[i], ABS_DISTANCE, -1, 100, 0, 0);

		error = input_register_device(data->input_dev[i]);
		if (error) {
			dev_err(&client->dev, "Unable to register input %d device, error: %d\n", i, error);
			input_free_device(data->input_dev[i]);
		}
		buffer = kasprintf(GFP_KERNEL, "Moto CapSense Ch%d", i);
		sensors_capsensor_cdev[i].sensors_enable = capsensor_set_enable;
		sensors_capsensor_cdev[i].sensors_poll_delay = NULL;
		sensors_capsensor_cdev[i].name = buffer;
		sensors_capsensor_cdev[i].vendor = "cypress";
		sensors_capsensor_cdev[i].version = 1;
		sensors_capsensor_cdev[i].type = SENSOR_TYPE_MOTO_CAPSENSE;
		sensors_capsensor_cdev[i].max_range = "5";
		sensors_capsensor_cdev[i].resolution = "5.0";
		sensors_capsensor_cdev[i].sensor_power = "3";
		sensors_capsensor_cdev[i].min_delay = 0;
		sensors_capsensor_cdev[i].fifo_reserved_event_count = 0;
		sensors_capsensor_cdev[i].fifo_max_event_count = 0;
		sensors_capsensor_cdev[i].delay_msec = 100;
		sensors_capsensor_cdev[i].enabled = 0;

		error = sensors_classdev_register(&data->input_dev[i]->dev, &sensors_capsensor_cdev[i]);
		if (error < 0)
			LOG_ERR("create %d cap sensor_class  file failed (%d)\n", i, error);

	}

	error = request_irq(gpio_to_irq(client->irq), cyttsp_sar_eint_func,
			IRQF_TRIGGER_FALLING, client->dev.driver->name, data);

	dev_info(&client->dev, "registering irq %d\n", gpio_to_irq(client->irq));
	if (error) {
		dev_err(&client->dev, "Error %d registering irq %d\n", error, gpio_to_irq(client->irq));
		goto err_irq_gpio_req;
	}

	INIT_DELAYED_WORK(&data->eint_work, cyttsp_sar_eint_work);

	wake_lock_init(&data->cap_lock, WAKE_LOCK_SUSPEND, "capsense wakelock");

	error = class_register(&capsense_class);
	if (error < 0) {
		dev_err(&client->dev,
				"Create fsys class failed (%d)\n", error);
		return error;
	}

	error = class_create_file(&capsense_class, &class_attr_fw_update);
	if (error < 0) {
		dev_err(&client->dev,
				"Create fw_update file failed (%d)\n", error);
		return error;
	}

	error = class_create_file(&capsense_class, &class_attr_reset);
	if (error < 0) {
		dev_err(&client->dev,
				"Create reset file failed (%d)\n", error);
		return error;
	}

	error = class_create_file(&capsense_class, &class_attr_reg);
	if (error < 0) {
		dev_err(&client->dev,
				"Create reg file failed (%d)\n", error);
		return error;
	}

	error = class_create_file(&capsense_class, &class_attr_set_threshold);
	if (error < 0) {
		dev_err(&client->dev,
				"Create set_threshold file failed (%d)\n", error);
		return error;
	}

	error = class_create_file(&capsense_class, &class_attr_enable);
	if (error < 0) {
		dev_err(&client->dev,
				"Create enable file failed (%d)\n", error);
		return error;
	}

	error = class_create_file(&capsense_class, &class_attr_debug);
	if (error < 0) {
		dev_err(&client->dev,
				"Create debug file failed (%d)\n", error);
		return error;
	}

	error = class_create_file(&capsense_class, &class_attr_fw_download_status);
	if (error < 0) {
		dev_err(&client->dev,
				"Create fw dl status file failed (%d)\n", error);
		return error;
	}

	INIT_WORK(&data->ps_notify_work, ps_notify_callback_work);
	data->ps_notif.notifier_call = ps_notify_callback;
	error = power_supply_reg_notifier(&data->ps_notif);
	if (error) {
		dev_err(&client->dev,
				"Unable to register ps_notifier: %d\n", error);
		goto free_ps_notifier;
	}

	psy = power_supply_get_by_name("usb");
	if (psy) {
		error = ps_get_state(data, psy, &data->ps_is_present);
		if (error) {
			dev_err(&client->dev,
					"psy get property failed rc=%d\n",
					error);
			goto free_ps_notifier;
		}
	}


	fw_dl_status = 1;
	ctrl_data->dev = &client->dev;
	INIT_WORK(&ctrl_data->work, capsense_update_work);
	gpio_direction_output(ctrl_data->hssp_d.rst_gpio, 0);
	gpio_set_value(ctrl_data->hssp_d.rst_gpio, 1);
	dev_info(&client->dev, "Cypress CapSense probe complete\n");

	ctrl_data->cmd = HSSP_CMD_FW_UPDATE;
	ctrl_data->hssp_d.inf.fw_name[0] = 0;

	schedule_work(&ctrl_data->work);

	dev_info(&client->dev, "Cypress CapSense probe success\n");
	return 0;

free_ps_notifier:
	power_supply_unreg_notifier(&data->ps_notif);
err_irq_gpio_req:
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
err_free_data:
	kfree(data);
	return error;
}

static int cyttsp_sar_remove(struct i2c_client *client)
{
	struct cyttsp_sar_data *data = i2c_get_clientdata(client);
	const struct cyttsp_sar_platform_data *pdata = data->pdata;
	int i;

	free_irq(client->irq, data);
	wake_lock_destroy(&data->cap_lock);
	for (i = 0; i < 4; i++) {
		input_unregister_device(data->input_dev[i]);
	}
	if (gpio_is_valid(pdata->irq_gpio))
		gpio_free(pdata->irq_gpio);
	if (data->regulator_vdd)
		regulator_disable(data->regulator_vdd);
	if (data->regulator_avdd)
		regulator_disable(data->regulator_avdd);
	if (data->regulator_vddio)
		regulator_disable(data->regulator_vddio);

	kfree(data);
	data = NULL;

	return 0;
}

static void cyttsp_sar_shutdown(struct i2c_client *client)
{
	disable_irq(client->irq);
}

static int cypress_i2c_suspend(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cyttsp_sar_data *data = i2c_get_clientdata(client);
	const struct cyttsp_sar_platform_data *pdata = data->pdata;

	disable_irq(gpio_to_irq(pdata->irq_gpio));
	dev_dbg(&client->dev, "cypress i2c suspend\n");

	return 0;
}

static int cypress_i2c_resume(struct device *dev)
{
	struct i2c_client *client = to_i2c_client(dev);
	struct cyttsp_sar_data *data = i2c_get_clientdata(client);
	const struct cyttsp_sar_platform_data *pdata = data->pdata;

	enable_irq(gpio_to_irq(pdata->irq_gpio));
	dev_dbg(&client->dev, "cypress i2c resume\n");

	return 0;
}

static const struct dev_pm_ops cypress_i2c_pm_ops = {
	.suspend	= cypress_i2c_suspend,
	.resume		= cypress_i2c_resume,
};

static const struct i2c_device_id cyt_id[] = {
	{"cyttsp_streetfighter", 0},
	{ },
};

#ifdef CONFIG_OF
static struct of_device_id cyttsp_match_table[] = {
	{ .compatible = "cypress,sf3155",},
	{ },
};
#else
#define cyttsp_match_table NULL
#endif

static struct i2c_driver cyttsp_sar_driver = {
	.driver = {
		.name	= "cyttsp_streetfighter",
		.owner	= THIS_MODULE,
		.pm		= &cypress_i2c_pm_ops,
		.of_match_table = cyttsp_match_table,
	},
	.probe		= cyttsp_sar_probe,
	.remove		= cyttsp_sar_remove,
	.shutdown	= cyttsp_sar_shutdown,
	.id_table	= cyt_id,
};

static int __init cyttsp_sar_init(void)
{
	return i2c_add_driver(&cyttsp_sar_driver);
}

static void __exit cyttsp_sar_exit(void)
{
	i2c_del_driver(&cyttsp_sar_driver);
}

late_initcall(cyttsp_sar_init);
module_exit(cyttsp_sar_exit);

/* Module information */
MODULE_AUTHOR("XXX <XXX@cypress.com>");
MODULE_DESCRIPTION("Cypress Capsense SAR Driver");
MODULE_LICENSE("GPL");

