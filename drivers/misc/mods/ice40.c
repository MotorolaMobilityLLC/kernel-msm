/*
 * MTD SPI driver for ST ice40xx (and similar) serial flash chips
 *
 * Author: Mike Lavender, weiweij@motorola.com
 *
 * Copyright (c) 2005, motorola Inc.
 *
 * This code is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */

#include <linux/delay.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/module.h>
#include <linux/device.h>
#include <linux/regmap.h>
#include <linux/spi/spi.h>
#include <linux/interrupt.h>
#include <linux/of_gpio.h>
#include <linux/of_irq.h>
#include <linux/firmware.h>
#include <linux/fs.h>
#include <linux/uaccess.h>
#include <linux/regulator/consumer.h>

//#undef CONFIG_APBA_SPI_SWITCH

#define ICE40_DEVICE_NAME	"ICE40"

#define ICE40_FW_RD_FILE_NAME	"/data/ice40_r.bin"
#define ICE40_FW_WR_FILE_NAME "/data/ice40_w.bin"

//#define FW_FLASH_DELAY
#define FW_BOOT_WITHOUT_FLASH
#define FW_FLASH_DELAY_MS	(20 * 1000)

#define ICE40_FIRMWARE_NAME_LEN (64)
#define ICE40_FW_DUMMY_DATA_NUM (500)
#define ICE40_DEFAULT_MID 1
#define ICE40_DEFAULT_SID 0
#define ICE40_FW_CDONE_TIMEOUT	(HZ * 1000)

#define ICE40_REG_RAM_SIZE (71342)
#define ICE40_REG_MAX (ICE40_REG_RAM_SIZE + ICE40_FW_DUMMY_DATA_NUM)

#ifndef __func__
#define __func__ ICE40_DEVICE_NAME
#endif

//static int gpio_sel;

struct ice40_data {
	struct spi_device	*spi;
	struct device		*dev;
	struct regmap	*regmap;
	struct regulator 	*vreg_1p2;

	char firmware_name[ICE40_FIRMWARE_NAME_LEN];
	int fw_mid;
	int fw_sid;
	struct completion fw_callback;
	struct delayed_work fw_flash_work;

	int irq_hpd;

	int gpio_hpd;
	int gpio_cdone;
	int gpio_reset;
	int gpio_cs;
	int gpio_spi_sel;
	int gpio_ldo_1p2_en;
	int gpio_ldo_1p8_en;
	int gpio_ldo_2p5_en;
	int gpio_ldo_spi_1p8_en;

	int sleep_flag;
};

static bool ice40_reg_writeable(struct device *dev, unsigned int reg);
static bool ice40_reg_volatile(struct device *dev, unsigned int reg);

static int ice40_flash_fw(struct ice40_data *idata, const struct firmware *fw);
static int ice40_ldo_en(struct ice40_data *idata, bool en);

static struct regmap_config ice40_regcfg = {
	.name = "ice40",
	.reg_bits = 8,
	.val_bits = 8,
	.cache_type = REGCACHE_RBTREE,

	.max_register = ICE40_REG_MAX,
	.writeable_reg = ice40_reg_writeable,
	.volatile_reg = ice40_reg_volatile,
};

static struct ice40_data *ice_data = NULL;
static int fw_dbg = 0;

module_param(fw_dbg, int, 0644);
MODULE_PARM_DESC(fw_dbg, "fw data Debug 0/1");

static bool ice40_reg_writeable(struct device *dev, unsigned int reg)
{
	switch (reg) {
		case 0:
		default:
			break;
	}

	return true;
}

static bool ice40_reg_volatile(struct device *dev, unsigned int reg)
{
	switch (reg) {
		case 0:
		default:
			break;
	}

	return false;
}

static int ice40_save_dbg_fw(struct ice40_data *idata,
						char* file,
						const char* data,
						int size)
{
	struct file *fp;
	mm_segment_t fs;
	loff_t pos;
	int ret = 0;

	fp = filp_open(file, O_RDWR | O_CREAT, 0644);
	pr_info("%s file ret %d\n", __func__, IS_ERR(fp));
	if (IS_ERR(fp)) {
		pr_err("%s create dbg file %s error %d\n", __func__, file, IS_ERR(file));
		return -1;
	}

	fs = get_fs();
	set_fs(KERNEL_DS);

	pos = 0;
	ret = vfs_write(fp, data, size, &pos);
	if (ret <= 0)
		pr_err("%s write file %s err %d, pos %d\n", __func__, file, ret, (int) pos);

	filp_close(fp, NULL);
	set_fs(fs);

	return ret;
}

static int ice40_read_back_fw(struct ice40_data *idata, int read_back_len)
{
	char* fw_data;
	unsigned int data= 0;
	int i, len, ret = 0;

	len = (read_back_len > 0) ? read_back_len : ICE40_REG_MAX;
	if (len > ICE40_REG_MAX)
		len = ICE40_REG_MAX;

	pr_info("%s read fw %d(org %d)\n", __func__, len, read_back_len);

	fw_data = kmalloc(ICE40_REG_MAX, GFP_KERNEL);

	gpio_set_value(idata->gpio_cs, 1);
	udelay(1000);
	gpio_set_value(idata->gpio_cs, 0);
	udelay(100);

	for(i=0;i<len;i++){
		ret = regmap_read(idata->regmap, i, &data);
		if (ret < 0) {
			pr_err("%s read %d data err, ret %d\n", __func__, i, ret);
			goto end;
		}
		fw_data[i] = (unsigned char) data;
	}

end:
	udelay(100);
	gpio_set_value(idata->gpio_cs, 1);

	ice40_save_dbg_fw(idata, ICE40_FW_RD_FILE_NAME, (const char*) fw_data, len);

	kfree(fw_data);

	return ret;
}

int fpga_request_fw_and_flash(struct ice40_data *idata, const char *name)
{
	const struct firmware *fw = NULL;
	struct device *dev;
	long count = ICE40_FW_CDONE_TIMEOUT;
	int ret;

	if (!idata) {
		pr_err("%s invalid ice_data\n", __func__);
		return -EINVAL;
	}

	dev = &idata->spi->dev;

	pr_info("%s: request firmware %s \n",
		__func__, name);

	ret = request_firmware(&fw, name, dev);
	if (ret < 0) {
		pr_debug("%s: request firmware failed for %s, err=%d\n",
			__func__, name, ret);
		return ret;
	}

	if (!fw || !fw->size) {
		pr_err("%s: firmware invalid for %s\n",
			__func__, name);
		return -EINVAL;
	}

	/* Support loading fw for ES2 into es2_apba partition */
	ret = ice40_flash_fw(idata, fw);
	if (ret < 0)
		pr_err("%s: flashing failed for %s, err=%d\n",
			__func__, name, ret);

	release_firmware(fw);

	do {
		ret = gpio_get_value(idata->gpio_cdone);
		count--;
	} while ((!ret) && (count > 0));

	if ((!ret) && (count <= 0))
		pr_err("%s fw flash wait cdone failed\n", __func__);
	else
		pr_info("%s:fw flash complete\n", __func__);

	if (fw_dbg)
		ice40_read_back_fw(idata, fw->size);

	return ret;
}

int fpga_flash(void)
{
	int ret;

	if (!ice_data) {
		pr_err("%s invalid ice_data\n", __func__);
		return -EINVAL;
	}

	ret = fpga_request_fw_and_flash(ice_data, ice_data->firmware_name);

	return ret;
}
EXPORT_SYMBOL(fpga_flash);

static int ice40_flash_fw(struct ice40_data *idata, const struct firmware *fw)
{
	int i = 0, ret = 0;
	const char* fw_data = fw->data;
	int fw_size = fw->size;;

	ice40_ldo_en(idata, 1);

	if (fw_dbg) {
		pr_info("%s debug fw data. Size %d\n", __func__, fw_size);
		ice40_save_dbg_fw(idata, ICE40_FW_WR_FILE_NAME, fw_data, fw_size);
	}

	if (gpio_is_valid(idata->gpio_spi_sel)) {
		gpio_set_value(idata->gpio_spi_sel, 1);
		//gpio_set_value(gpio_sel, 0);
	}

	udelay(1000);

	gpio_set_value(idata->gpio_cs, 0);
	gpio_set_value(idata->gpio_reset, 0);
	udelay(10);
	gpio_set_value(idata->gpio_reset, 1);
	udelay(800);
	gpio_set_value(idata->gpio_cs, 1);

	//send 8 dummy clocks
	for(i=0;i<8;i++) {
		ret = regmap_read(idata->regmap, 0, &ret);
		if (ret < 0) {
			pr_err("%s %d dummy clock, ret %d\n", __func__, i, ret);
			//goto end;
		}
	}

	gpio_set_value(idata->gpio_cs, 0);

	ret = regmap_bulk_write(idata->regmap, 0, fw_data, fw_size);
	if (ret < 0) {
		pr_err("%s tx data err, ret %d\n", __func__, ret);
		goto end;
	}

	for(i=0;i<ICE40_FW_DUMMY_DATA_NUM;i++){
		ret = regmap_write(idata->regmap, i + fw_size, 0);
		if (ret < 0) {
			pr_err("%s %d dummy clock err, ret %d\n", __func__, i, ret);
			goto end;
		}
	}

end:
	gpio_set_value(idata->gpio_cs, 1);

	udelay(1000);
	if (gpio_is_valid(idata->gpio_spi_sel)) {
		gpio_set_value(idata->gpio_spi_sel, 1);
	}

	return ret;
}

static void ice40_firmware_callback(const struct firmware *fw, void *context)
{
	struct ice40_data *idata = (struct ice40_data *)context;
	long count = ICE40_FW_CDONE_TIMEOUT;
	int size;
	int ret;

	if (!idata) {
		pr_err("%s: invalid ice data\n", __func__);
		release_firmware(fw);
		if (ice_data)
			complete(&ice_data->fw_callback);
		return;
	}

	if (!fw) {
		pr_err("%s: no firmware available\n", __func__);
	} else {
		pr_info("%s: size=%zu data=%p\n", __func__, fw->size,
			fw->data);
		size =  fw->size;

		ret = ice40_flash_fw(idata, fw);
		if (ret < 0) {
			pr_err("%s: flashing failed err=%d\n", __func__, ret);
			goto end;
		}

		/* TODO: notify system, in case of error */
		release_firmware(fw);

		do {
			ret = gpio_get_value(idata->gpio_cdone);
			count--;
		} while ((!ret) && (count > 0));

		if ((!ret) && (count <= 0))
			pr_err("%s fw flash wait cdone failed\n", __func__);
		else
			pr_info("%s:fw flash complete\n", __func__);

		if (fw_dbg)
			ice40_read_back_fw(idata, size);
	}

end:
	complete(&idata->fw_callback);
}

static void ice40_fw_flash_work(struct work_struct *work)
{
	struct ice40_data *idata = container_of(work,
								struct ice40_data,
								fw_flash_work.work);
	int ret;

	ret = request_firmware_nowait(THIS_MODULE, true, idata->firmware_name,
				      idata->dev, GFP_KERNEL, idata,
				      ice40_firmware_callback);
	if (ret) {
		pr_err("%s failed to request firmware.\n", __func__);
		complete(&idata->fw_callback);
		return;
	}
}

static irqreturn_t ice40_hpd_irq(int irq, void *idata)
{
	pr_info("%s\n", __func__);
	return IRQ_HANDLED;
}

static ssize_t flash_fw_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	if (!ice_data) {
		pr_err("%s invalid ice_data\n", __func__);
		return scnprintf(buf, PAGE_SIZE, "no data\n");
	}

	return scnprintf(buf, PAGE_SIZE,
			 "%s\n", ice_data->firmware_name);
}

static ssize_t flash_fw_store(struct device *dev, struct device_attribute *attr,
							const char *buf, size_t count)
{
	if (!ice_data) {
		pr_err("%s invalid ice_data\n", __func__);
		return -EINVAL;
	}

	if (strstr(buf, "ice40-") && strstr(buf, "bin"))
		strcpy(ice_data->firmware_name, buf);

	pr_info("%s use fw %s\n", __func__, ice_data->firmware_name);

	fpga_request_fw_and_flash(ice_data, ice_data->firmware_name);

	return count;
}

static DEVICE_ATTR(flash_fw, S_IWUSR | S_IRUGO, flash_fw_show, flash_fw_store);

static ssize_t sleep_show(struct device *dev,
			       struct device_attribute *attr, char *buf)
{
	if (!ice_data) {
		pr_err("%s invalid ice_data\n", __func__);
		return scnprintf(buf, PAGE_SIZE, "no data\n");
	}

	return scnprintf(buf, PAGE_SIZE,
			 "%s\n", ice_data->sleep_flag ? "true" : "false");
}

static ssize_t sleep_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	unsigned long val;

	if (!ice_data) {
		pr_err("%s invalid ice_data\n", __func__);
		return -EINVAL;
	}

	if (kstrtoul(buf, 10, &val) < 0)
		return -EINVAL;

	ice_data->sleep_flag = !!val;

	return count;
}

static DEVICE_ATTR(sleep, S_IWUSR | S_IRUGO, sleep_show, sleep_store);

static struct attribute *ice40_dbg_attrs[] = {
	&dev_attr_flash_fw.attr,
	&dev_attr_sleep.attr,
	NULL,
};

ATTRIBUTE_GROUPS(ice40_dbg);

static int ice40_ldo_en(struct ice40_data *idata, bool en)
{
	int ret;

	pr_info("%s ldo en %d\n", __func__, en);
	if (en) {
		if (idata->vreg_1p2) {
			ret = regulator_enable(idata->vreg_1p2);
		}

		gpio_set_value(idata->gpio_ldo_1p2_en, 1);
		udelay(5000);
		gpio_set_value(idata->gpio_ldo_1p8_en, 1);
		udelay(5000);
		gpio_set_value(idata->gpio_ldo_2p5_en, 1);
		if (gpio_is_valid(idata->gpio_ldo_spi_1p8_en))
			gpio_set_value(idata->gpio_ldo_spi_1p8_en, 1);
		udelay(5000);

		gpio_set_value(idata->gpio_reset, 1);
	} else {
		if (idata->vreg_1p2) {
			ret = regulator_disable(idata->vreg_1p2);
		}

		gpio_set_value(idata->gpio_ldo_1p2_en, 0);
		gpio_set_value(idata->gpio_ldo_1p8_en, 0);
		gpio_set_value(idata->gpio_ldo_2p5_en, 0);
		if (gpio_is_valid(idata->gpio_ldo_spi_1p8_en))
			gpio_set_value(idata->gpio_ldo_spi_1p8_en, 0);
	}

	return 0;
}

static int ice40_of_init(struct device *dev, struct ice40_data *idata)
{
	struct device_node *node = dev->of_node;
	struct property *prop;
	int rc = 0;

	if (!node) {
		dev_err(dev, "%s: of_node do not find\n", __func__);
		rc = -1;
		goto end;
	}

	//fw version and name
	idata->fw_mid = ICE40_DEFAULT_MID;
	idata->fw_sid = ICE40_DEFAULT_SID;

	if (of_property_read_u32(node, "ice40,fw-mid", &idata->fw_mid))
		dev_warn(dev, "no firmware mid. use default %d\n",
			idata->fw_mid);
	if (of_property_read_u32(node, "ice40,fw-sid", &idata->fw_sid))
		dev_warn(dev, "no firmware sid. use default %d\n",
			idata->fw_sid);

	snprintf(idata->firmware_name, sizeof(idata->firmware_name),
		 "ice40-%d-%d.bin", idata->fw_mid, idata->fw_sid);

	//cs
	idata->gpio_cs = of_get_named_gpio(node, "ice,gpio-cs", 0);
	if (idata->gpio_cs < 0) {
		pr_err("failed to get ice,cs-gpio.\n");
		rc = idata->gpio_cs;
		goto end;
	}

	if (gpio_is_valid(idata->gpio_cs)) {
		rc = gpio_request(idata->gpio_cs, ICE40_DEVICE_NAME "CS");
		if (rc == 0) {
			gpio_direction_output(idata->gpio_cs, 0);
		}
	}
	gpio_export(idata->gpio_cs, true);

	//ldo
	prop = of_find_property(node, "vdd_1p2-supply", NULL);
	if (prop) {
		idata->vreg_1p2 = devm_regulator_get(dev, "vdd_1p2");
		if (IS_ERR(idata->vreg_1p2)) {
			dev_err(dev, "%s get vreg 1p2 error %ld\n",
						__func__, PTR_ERR(idata->vreg_1p2));
			return PTR_ERR(idata->vreg_1p2);
		}

		rc  = regulator_set_voltage(idata->vreg_1p2, 1350000, 1350000);
		if (rc ) {
			dev_err(dev, "Failed to set reg 1p2 vol: %d\n", rc );
			return rc ;
		}

		rc = regulator_set_load(idata->vreg_1p2, 100000);
		if (rc ) {
			dev_err(dev, "Failed to set reg 1p2 cur: %d\n", rc );
		}
	} else {
		idata->vreg_1p2 = NULL;
		pr_info("%s do not have vdd_1p2 supply\n", __func__);
	}

	//gpio_sel just for test
/*	gpio_sel = of_get_named_gpio(node, "ice,gpio-sel1", 0);
	if (gpio_sel < 0) {
		pr_err("failed to get ice,ice,gpio-sel1.\n");
		rc = gpio_sel;
		goto end;
	}
	if (gpio_is_valid(gpio_sel)) {
		rc = gpio_request(gpio_sel, ICE40_DEVICE_NAME "gpio_sel1");
		if (rc == 0) {
			gpio_direction_output(gpio_sel, 0);
		}
	}
	gpio_export(gpio_sel, true);*/

	//ldo_1p2_en
	idata->gpio_ldo_1p2_en = of_get_named_gpio(node, "ice,gpio-ldo-1p2-en", 0);
	if (idata->gpio_ldo_1p2_en < 0) {
		pr_err("failed to get ice,ldo_1p2_en-gpio.\n");
		rc = idata->gpio_ldo_1p2_en;
		goto end;
	}

	if (gpio_is_valid(idata->gpio_ldo_1p2_en)) {
		rc = gpio_request(idata->gpio_ldo_1p2_en, ICE40_DEVICE_NAME "ldo_1p2_en");
		if (rc == 0) {
			gpio_direction_output(idata->gpio_ldo_1p2_en, 0);
			gpio_export(idata->gpio_ldo_1p2_en, true);
		}
	}

	//ldo_1p8_en
	idata->gpio_ldo_1p8_en = of_get_named_gpio(node, "ice,gpio-ldo-1p8-en", 0);
	if (idata->gpio_ldo_1p8_en < 0) {
		pr_err("failed to get ice,ldo_1p8_en-gpio.\n");
		rc = idata->gpio_ldo_1p8_en;
		goto end;
	}

	if (gpio_is_valid(idata->gpio_ldo_1p8_en)) {
		rc = gpio_request(idata->gpio_ldo_1p8_en, ICE40_DEVICE_NAME "ldo_1p8_en");
		if (rc == 0) {
			gpio_direction_output(idata->gpio_ldo_1p8_en, 0);
			gpio_export(idata->gpio_ldo_1p8_en, true);
		}
	}

	//ldo_2p5_en
	idata->gpio_ldo_2p5_en = of_get_named_gpio(node, "ice,gpio-ldo-2p5-en", 0);
	if (idata->gpio_ldo_2p5_en < 0) {
		pr_err("failed to get ice,ldo_2p5_en-gpio.\n");
		rc = idata->gpio_ldo_2p5_en;
		goto end;
	}

	if (gpio_is_valid(idata->gpio_ldo_2p5_en)) {
		rc = gpio_request(idata->gpio_ldo_2p5_en, ICE40_DEVICE_NAME "ldo_2p5_en");
		if (rc == 0) {
			gpio_direction_output(idata->gpio_ldo_2p5_en, 0);
			gpio_export(idata->gpio_ldo_2p5_en, true);
		}
	}

	//ldo_spi_1p8_en
	// spi 1p8 may use the same ldo as vccio
	// so this pin may not needed.
	idata->gpio_ldo_spi_1p8_en = of_get_named_gpio(node, "ice,gpio-ldo-spi-1p8-en", 0);
	if (idata->gpio_ldo_spi_1p8_en < 0) {
		pr_err("failed to get ice,ldo_spi_1p8_en-gpio.\n");
		rc = idata->gpio_ldo_spi_1p8_en;
	}

	if (gpio_is_valid(idata->gpio_ldo_spi_1p8_en)) {
		rc = gpio_request(idata->gpio_ldo_spi_1p8_en, ICE40_DEVICE_NAME "ldo_spi_1p8_en");
		if (rc == 0) {
			gpio_direction_output(idata->gpio_ldo_spi_1p8_en, 0);
			gpio_export(idata->gpio_ldo_spi_1p8_en, true);
		}
	}

	//reset
	idata->gpio_reset = of_get_named_gpio(node, "ice,gpio-reset", 0);
	if (idata->gpio_reset < 0) {
		pr_err("failed to get ice,reset-gpio.\n");
		rc = idata->gpio_reset;
		goto end;
	}

	if (gpio_is_valid(idata->gpio_reset)) {
		rc = gpio_request(idata->gpio_reset, ICE40_DEVICE_NAME "RST");
		if (rc == 0) {
			gpio_direction_output(idata->gpio_reset, 0);
			gpio_export(idata->gpio_reset, true);
		}
	}

	//cdone
	idata->gpio_cdone = of_get_named_gpio(node, "ice,gpio-cdone", 0);
	if (idata->gpio_cdone < 0) {
		pr_err("failed to get ice,cdone-gpio.\n");
		rc = idata->gpio_cdone;
		goto end;
	}

	if (gpio_is_valid(idata->gpio_cdone)) {
		rc = gpio_request(idata->gpio_cdone, ICE40_DEVICE_NAME "DONE");
		if (rc == 0) {
			gpio_direction_input(idata->gpio_cdone);
			gpio_export(idata->gpio_cdone, true);
		}
	}

	//spi-sel : valid high
	idata->gpio_spi_sel = of_get_named_gpio(node, "ice,gpio-spi-sel", 0);
	if (idata->gpio_spi_sel < 0) {
		pr_err("there is no ice,spi-sel-gpio.\n");
	} else {
		if (gpio_is_valid(idata->gpio_spi_sel)) {
			rc = gpio_request(idata->gpio_spi_sel, ICE40_DEVICE_NAME "SPI-SEL");
			if (rc == 0) {
				gpio_direction_output(idata->gpio_spi_sel, 0);
				gpio_export(idata->gpio_spi_sel, true);
			}
		}
	}

	//gpio-hpd
	idata->gpio_hpd = of_get_named_gpio(node, "ice,gpio-hpd", 0);
	if (idata->gpio_hpd < 0) {
		pr_err("there is no ice,hpd-gpio.\n");
	} else {
		if (gpio_is_valid(idata->gpio_hpd)) {
			rc = gpio_request(idata->gpio_hpd, ICE40_DEVICE_NAME "HPD");
			if (rc == 0) {
				idata->irq_hpd = gpio_to_irq(idata->gpio_hpd);
				if (idata->irq_hpd < 0) {
					pr_err("%s, failed to get gpio irq\n", __func__);
					return (idata->irq_hpd);
				}
				rc = request_threaded_irq(idata->irq_hpd, NULL, ice40_hpd_irq,
								           IRQF_TRIGGER_FALLING | IRQF_ONESHOT,
								           "ice40_hpd", idata);
				if (rc < 0) {
					pr_err("%s failed to request irq\n", __func__);
					goto end;
				}
			} else {
				pr_err("%s failed to request gpio hpd\n", __func__);
			}
		} else {
			pr_err("%s gpio hpd is invalid\n", __func__);
		}
	}

end:
	return rc;
}

static int ice40_probe(struct spi_device *spi)
{
	struct ice40_data *idata;
	int ret;

	idata = devm_kzalloc(&spi->dev, sizeof(struct ice40_data), GFP_KERNEL);
	if (!idata)
		return -ENOMEM;

	idata->regmap = devm_regmap_init_spi(spi, &ice40_regcfg);
	if (IS_ERR(idata->regmap)) {
		ret = PTR_ERR(idata->regmap);
		dev_err(&spi->dev,
			"Failed to allocate register map: %d\n", ret);
		return ret;
	}

	idata->spi = spi;
	idata->dev = &spi->dev;

	dev_set_drvdata(&spi->dev, idata);

	ret = ice40_of_init(&spi->dev, idata);
	if (ret) {
		dev_err(&spi->dev, "ice40_of_init failed %d\n", ret);
		return ret;
	}

	ice_data = idata;

	ret = sysfs_create_groups(&idata->dev->kobj, ice40_dbg_groups);
	if (ret) {
		dev_err(idata->dev, "%s Failed to create sysfs attributes %d\n",
			__func__, ret);
		return ret;
	}

	idata->sleep_flag = false;

	pr_info("%s successed. SPI speed %d, fw debug %s\n",
		__func__, spi->max_speed_hz, fw_dbg ? "true":"false");

	init_completion(&idata->fw_callback);

	INIT_DELAYED_WORK(&idata->fw_flash_work, ice40_fw_flash_work);

#ifdef FW_BOOT_WITHOUT_FLASH
	/*do not flash when prob. Fw will be flashed by other module*/
#else
#ifdef FW_FLASH_DELAY
	schedule_delayed_work(&idata->fw_flash_work,
					msecs_to_jiffies(FW_FLASH_DELAY_MS));
#else
	ret = request_firmware_nowait(THIS_MODULE, true, idata->firmware_name,
				      idata->dev, GFP_KERNEL, idata,
				      ice40_firmware_callback);
	if (ret) {
		pr_err("%s failed to request firmware.\n", __func__);
		complete(&idata->fw_callback);
		return -1;
	}
#endif
#endif

	return 0;
}


static int ice40_remove(struct spi_device *spi)
{
	struct ice40_data *idata = dev_get_drvdata(&spi->dev);

	wait_for_completion(&idata->fw_callback);
	cancel_delayed_work_sync(&idata->fw_flash_work);

	return 0;
}

static const struct spi_device_id ice40_ids[] = {
	{"ice40"},
	{ },
};
MODULE_DEVICE_TABLE(spi, ice40_ids);

static const struct of_device_id ice40_of_table[] = {
	{ .compatible = "lattice,ice40" },
	{}
};
MODULE_DEVICE_TABLE(of, ice40_of_table);

static struct spi_driver ice40_driver = {
	.driver = {
		.name	= "ice40",
		.of_match_table = ice40_of_table,
	},
	.id_table	= ice40_ids,
	.probe	= ice40_probe,
	.remove	= ice40_remove,
};

module_spi_driver(ice40_driver);

MODULE_LICENSE("GPL");
MODULE_DESCRIPTION("SPI driver for lattice ice40 chips");
