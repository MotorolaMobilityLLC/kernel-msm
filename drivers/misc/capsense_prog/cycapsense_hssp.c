/*
 * Source file for:
 * Cypress CapSense(TM) programming driver.
 *
 * Copyright (c) 2012-2013 Motorola Mobility LLC
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

#include <linux/module.h>
#include <linux/platform_device.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/string.h>
#include <linux/firmware.h>
#include <linux/limits.h>
#include <linux/uaccess.h>
#include <linux/delay.h>
#include <linux/io.h>
#include <linux/hssp_programmer.h>
#include "cycapsense_hssp.h"
#include "ProgrammingSteps.h"

#define CYCAPSENSE_PROG_NAME	"cycapsense_hssp"

struct cycapsense_ctrl_data *ctrl_data;

static int programming_done;
static BLOCKING_NOTIFIER_HEAD(hssp_notifier_list);

/**
 * register_hssp_update_notify - register accessory notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * Calls the notifier callback to when CapSense goes to/from programming mode,
 * indicated by a boolean passed to the callback's action parameter.
 */
void register_hssp_update_notify(struct notifier_block *nb)
{
	if (!nb)
		return;

	/* inform new client of current state */
	if (nb->notifier_call)
		nb->notifier_call(nb, programming_done, NULL);

	blocking_notifier_chain_register(&hssp_notifier_list, nb);
}
EXPORT_SYMBOL(register_hssp_update_notify);

/**
 * unregister_hssp_update_notify - unregister a notifier callback
 * @nb: pointer to the notifier block for the callback events.
 *
 * register_hssp_update_notify() must have been previously called for this
 * function to work properly.
 */
void unregister_hssp_update_notify(struct notifier_block *nb)
{
	blocking_notifier_chain_unregister(&hssp_notifier_list, nb);
}
EXPORT_SYMBOL(unregister_hssp_update_notify);

static void cycapsense_hssp_notify(int status)
{
	programming_done = status;
	blocking_notifier_call_chain(&hssp_notifier_list,
						programming_done, NULL);
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

	if (inf->fw_name[0] ||
		cycapsense_hssp_verify_swrev(&data->hssp_d) != SUCCESS) {
		/* force update regardless check sum, if user requested */
		dev_info(data->dev, "Flashing firmware %s\n", fw_name);
		error = cycapsense_hssp_dnld(&data->hssp_d);
		if (!error)
			dev_info(data->dev, "%s flashed successful\n",
						fw_name);
	} else {
		data->hssp_d.chip_cs = data->hssp_d.inf.cs;
		dev_info(data->dev,
				"SW rev is matching. No firmware upgrade.\n");
	}

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

int cycapsense_fw_update(void)
{
	if (ctrl_data == NULL || ctrl_data->dev == NULL) {
		pr_err("%s: Ctrl data not initialized\n", __func__);
		return -ENODEV;
	}

	ctrl_data->cmd = HSSP_CMD_FW_UPDATE;
	schedule_work(&ctrl_data->work);
	return 0;
}
EXPORT_SYMBOL(cycapsense_fw_update);

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
EXPORT_SYMBOL(cycapsense_reset);

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

static ssize_t cycapsense_fw_show(struct class *class,
					struct class_attribute *attr,
					char *buf)
{
	return snprintf(buf, 16, "0x%x\n",
				ctrl_data && ctrl_data->hssp_d.chip_cs ?
				ctrl_data->hssp_d.sw_rev : 0);
}

static ssize_t cycapsense_reset_show(struct class *class,
					struct class_attribute *attr,
					char *buf)
{
	return snprintf(buf, 8, "%d\n", programming_done);
}

static ssize_t cycapsense_reset_store(struct class *class,
					struct class_attribute *attr,
					const char *buf, size_t count)
{
	if (!count)
		return -EINVAL;

	if (!strncmp(buf, "reset", 5) || !strncmp(buf, "1", 1))
		cycapsense_reset();

	return count;
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

static CLASS_ATTR(fw_update, 0660, cycapsense_fw_show, cycapsense_fw_store);
static CLASS_ATTR(reset, 0660, cycapsense_reset_show, cycapsense_reset_store);

static struct class capsense_class = {
	.name			= "capsense",
	.owner			= THIS_MODULE,
};

static int cycapsense_prog_probe(struct platform_device *pdev)
{
	struct device_node *np = pdev->dev.of_node;
	int error;
	unsigned int addr = 0, size = 0;
	unsigned int gpios[9];

	ctrl_data = devm_kzalloc(&pdev->dev,
		sizeof(struct cycapsense_ctrl_data), GFP_KERNEL);
	if (ctrl_data == NULL) {
		dev_err(&pdev->dev, "Failed to allocate memory\n");
		return -ENOMEM;
	}

	ctrl_data->hssp_d.rst_gpio = of_get_gpio(np, 0);
	error = cycapsense_validate_gpio(&pdev->dev,
		ctrl_data->hssp_d.rst_gpio, "capsense_reset_gpio", 1, 0);
	if (error)
		return error;

	gpio_export(ctrl_data->hssp_d.rst_gpio, false);

	ctrl_data->hssp_d.c_gpio = of_get_gpio(np, 1);
	/*request only, direction == 2. Will be set by firmware loader*/
	error = cycapsense_validate_gpio(&pdev->dev,
		ctrl_data->hssp_d.c_gpio, "capsense_sclk_gpio", 2, 0);
	if (error)
		return error;

	ctrl_data->hssp_d.d_gpio = of_get_gpio(np, 2);
	/*request only, direction == 2. Will be set by firmware loader*/
	error = cycapsense_validate_gpio(&pdev->dev,
		ctrl_data->hssp_d.d_gpio, "capsense_sdata_gpio", 2, 0);
	if (error)
		return error;

	error = of_property_read_string(np, "label", &ctrl_data->hssp_d.name);
	if (error) {
		dev_err(&pdev->dev, "Error reading node label\n");
		return error;
	}
	error = of_property_read_u32(np, "silicon_id",
		&ctrl_data->hssp_d.silicon_id);
	if (error) {
		dev_err(&pdev->dev, "Error reading node silicon id\n");
		return error;
	}
	error = of_property_read_u32(np, "block_size",
		&ctrl_data->hssp_d.block_size);
	if (error) {
		dev_err(&pdev->dev, "Error reading node block size\n");
		return error;
	}
	error = of_property_read_u32(np, "num_of_blocks",
		&ctrl_data->hssp_d.num_of_blocks);
	if (error) {
		dev_err(&pdev->dev, "Error reading node num_of_blocks\n");
		return error;
	}
	error = of_property_read_u32(np, "secure_bytes",
		&ctrl_data->hssp_d.secure_bytes);
	if (error) {
		dev_err(&pdev->dev, "Error reading node secure_bytes\n");
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
					devm_ioremap(&pdev->dev, addr, size);
			ctrl_data->hssp_d.c_offset = gpios[CLK_GPIO];
			ctrl_data->hssp_d.d_offset = gpios[DAT_GPIO];
			dev_info(&pdev->dev, "Raw GPIO (%d:%d) RW enabled\n",
						ctrl_data->hssp_d.c_offset,
						ctrl_data->hssp_d.d_offset);
		} else {
			ctrl_data->hssp_d.raw_mode = 0;
			dev_info(&pdev->dev, "Raw GPIO RW enabled but regs are not defined.Switch to normal mode\n");
		}
	}


	error = class_register(&capsense_class);
	if (error < 0) {
		dev_err(&pdev->dev,
			"Create fsys class failed (%d)\n", error);
		return error;
	}

	error = class_create_file(&capsense_class, &class_attr_fw_update);
	if (error < 0) {
		dev_err(&pdev->dev,
			"Create fw_update file failed (%d)\n", error);
		return error;
	}

	error = class_create_file(&capsense_class, &class_attr_reset);
	if (error < 0) {
		dev_err(&pdev->dev,
			"Create reset file failed (%d)\n", error);
		return error;
	}
	ctrl_data->dev = &pdev->dev;
	platform_set_drvdata(pdev, ctrl_data);

	INIT_WORK(&ctrl_data->work, capsense_update_work);

	dev_dbg(&pdev->dev, "Cypress CapSense probe complete\n");

	return 0;
}

static int cycapsense_prog_remove(struct platform_device *pdev)
{
	class_remove_file(&capsense_class, &class_attr_fw_update);
	class_remove_file(&capsense_class, &class_attr_reset);
	class_unregister(&capsense_class);
	return 0;
}

static const struct of_device_id cycapsense_dt_match[] = {
	{.compatible = "cypress,cycapsense_hssp",},
	{}
};
MODULE_DEVICE_TABLE(of, cycapsense_dt_match);

static struct platform_driver cycapsense_prog_driver = {
	.driver = {
		.name = CYCAPSENSE_PROG_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(cycapsense_dt_match),
	},
	.probe = cycapsense_prog_probe,
	.remove = cycapsense_prog_remove,
};


module_platform_driver(cycapsense_prog_driver);

MODULE_ALIAS("prog:cycapsense_hssp");
MODULE_LICENSE("GPL v2");
MODULE_DESCRIPTION("Cypress CapSense(R) HSSP programming driver");
MODULE_AUTHOR("Motorola Mobility LLC");
