/*
 * Copyright (C) 2014-2015 Motorola, Inc.
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
#include <linux/clk.h>
#include <linux/clk/msm-clk.h>
#include <linux/err.h>
#include <linux/errno.h>
#include <linux/fs.h>
#include <linux/gpio.h>
#include <linux/kernel.h>
#include <linux/miscdevice.h>
#include <linux/module.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/platform_device.h>
#include <linux/slab.h>
#include <linux/spi/spi.h>
#include <linux/ssv_gps.h>
#include <linux/string.h>
#include <linux/regulator/consumer.h>

struct ssv_control *global_control_data;
char ssv_gps_debug = SSV_GPS_ERROR;
EXPORT_SYMBOL_GPL(ssv_gps_debug);

/* ------------------- Regulator Control ----------------------- */
static ssize_t ssv_gps_regulator_enable_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	struct ssv_control *ctrl_data = dev_get_drvdata(dev);

	return sprintf(buf, "%u\n",
			ctrl_data->regulator_enabled);
}

static ssize_t ssv_gps_regulator_enable_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	struct ssv_control *ctrl_data = dev_get_drvdata(dev);
	unsigned int value;
	int result;

	if (sscanf(buf, "%d", &value) == 1) {
		if (value) {
			result = regulator_enable(ctrl_data->supply);
			if (!result)
				ctrl_data->regulator_enabled = 1;
		} else {
			result = regulator_disable(ctrl_data->supply);
			if (!result)
				ctrl_data->regulator_enabled = 0;
		}
		return count;
	}
	return -EINVAL;
}

static DEVICE_ATTR(enable_regulator, S_IRUGO | S_IWUSR,
			ssv_gps_regulator_enable_show,
			ssv_gps_regulator_enable_store);

/* ------------------- End of Regulator Control ----------------------- */

static long ssv_gps_ioctl(struct file *filp, unsigned int cmd,
				unsigned long arg)
{
	int ret = 0;
	struct ssv_control *control_data;
	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "%s:\n", __func__);

	control_data = filp->private_data;

	switch (cmd) {
	default:
		KDEBUG(SSV_GPS_ERROR, "%s Invalid ioctl cmd %d\n",
			__func__, cmd);
		ret = -EINVAL;
		break;
	}
	return ret;
}

static int ssv_gps_open(struct inode *inode, struct file *file)
{
	int ret = 0;
	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "%s:\n", __func__);
	if (global_control_data) {
		ret = nonseekable_open(inode, file);
		if (ret >= 0) {
			file->private_data = global_control_data;
			ret = 0;
		}
	}
	return ret;
}

static int ssv_gps_close(struct inode *inode, struct file *file)
{
	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "%s:\n", __func__);
	file->private_data = NULL;
	return 0;
}

static const struct file_operations ssv_gps_fops = {
	.owner = THIS_MODULE,
	.unlocked_ioctl = ssv_gps_ioctl,
	.open  = ssv_gps_open,
	.release = ssv_gps_close,
};

static struct miscdevice ssv_gps_misc_device = {
	.minor = MISC_DYNAMIC_MINOR,
	.name = SSV_GPS_DRIVER_NAME,
	.fops = &ssv_gps_fops,
};

static int ssv_gps_hw_deinit(struct ssv_control *control_data)
{
	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "%s:\n", __func__);
	if (!control_data)
		return -ENODATA;

	if (gpio_is_valid(control_data->boot_select_gpio)) {
		gpio_free(control_data->boot_select_gpio);
		control_data->boot_select_gpio = -1;
	}

	if (gpio_is_valid(control_data->irq_gpio)) {
		gpio_free(control_data->irq_gpio);
		control_data->irq_gpio = -1;
	}

	if (gpio_is_valid(control_data->on_off_gpio)) {
		gpio_free(control_data->on_off_gpio);
		control_data->on_off_gpio = -1;
	}

	if (gpio_is_valid(control_data->reset_gpio)) {
		gpio_free(control_data->reset_gpio);
		control_data->reset_gpio = -1;
	}

	if (gpio_is_valid(control_data->host_wakeup_gpio)) {
		gpio_free(control_data->host_wakeup_gpio);
		control_data->host_wakeup_gpio = -1;
	}

	return 0;
}

static int ssv_gps_hw_init(struct platform_device *pdev,
			struct ssv_control *control_data,
			struct device_node *node)
{
	int gpio;
	const int *spi_bus, *spi_freq, *chip_sel;
	int len;
	int err = -EINVAL;

	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "%s:\n", __func__);
	if (!control_data)
		return -ENODATA;
	else if (!node)
		return -ENODATA;

	/* Set up GPIO */
	/* GPS_BOOT_SELECT */
	gpio = of_get_named_gpio_flags(node, "mot,boot-select-gpio", 0, NULL);
	err = (gpio < 0) ? -ENODEV : gpio_request(gpio, "GPS BOOT_SELECT");
	if (err) {
		KDEBUG(SSV_GPS_ERROR,
		       "Failed acquiring SSV GPS BOOT SELECT GPIO - %d(%d)\n",
		       gpio, err);
		goto error_boot_select;
	}
	gpio_direction_output(gpio, 0);
	gpio_export(gpio, false);
	gpio_export_link(&pdev->dev, "gps_boot_select", gpio);
	control_data->boot_select_gpio = gpio;
	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "GPS BOOT SELECT GPIO %d.\n",
	       control_data->boot_select_gpio);

	/* GPS_IRQ */
	gpio = of_get_named_gpio_flags(node, "mot,irq-gpio", 0, NULL);
	err = (gpio < 0) ? -ENODEV : gpio_request(gpio, "GPS IRQ");
	if (err) {
		KDEBUG(SSV_GPS_ERROR,
		       "Failed acquiring SSV GPS IRQ GPIO - %d(%d)\n",
		       gpio, err);
		goto error_irq;
	}
	gpio_direction_input(gpio);
	gpio_export(gpio, false);
	gpio_export_link(&pdev->dev, "gps_irq", gpio);
	control_data->irq_gpio = gpio;
	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "GPS IRQ GPIO %d.\n",
	       control_data->irq_gpio);

	/* GPS_ON_OFF */
	gpio = of_get_named_gpio_flags(node, "mot,on-off-gpio", 0, NULL);
	err = (gpio < 0) ? -ENODEV : gpio_request(gpio, "GPS ON OFF");
	if (err) {
		KDEBUG(SSV_GPS_ERROR,
		       "Failed acquiring SSV GPS ON_OFF GPIO - %d(%d)\n",
		       gpio, err);
		goto error_on_off;
	}
	gpio_direction_output(gpio, 0);
	gpio_export(gpio, false);
	gpio_export_link(&pdev->dev, "gps_on_off", gpio);
	control_data->on_off_gpio = gpio;
	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "GPS ON_OFF GPIO %d.\n",
	       control_data->on_off_gpio);

	/* GPS_RESET */
	gpio = of_get_named_gpio_flags(node, "mot,reset-gpio", 0, NULL);
	err = (gpio < 0) ? -ENODEV : gpio_request(gpio, "GPS RESET");
	if (err) {
		KDEBUG(SSV_GPS_ERROR,
		       "Failed acquiring SSV GPS RESET GPIO - %d(%d)\n",
		       gpio, err);
		goto error_reset;
	}
	gpio_direction_output(gpio, 1);
	gpio_export(gpio, false);
	gpio_export_link(&pdev->dev, "gps_reset", gpio);
	control_data->reset_gpio = gpio;
	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "GPS RESET GPIO %d.\n",
	       control_data->reset_gpio);

	/* GPS_HOST_WAKEUP */
	gpio = of_get_named_gpio_flags(node, "mot,host-wakeup-gpio", 0, NULL);
	err = (gpio < 0) ? -ENODEV : gpio_request(gpio, "GPS HOST WAKEUP");
	if (err) {
		KDEBUG(SSV_GPS_ERROR,
		       "Failed acquiring SSV GPS HOST WAKEUP GPIO - %d(%d)\n",
		       gpio, err);
		goto error_wakeup;
	}
	gpio_direction_input(gpio);
	gpio_export(gpio, false);
	gpio_export_link(&pdev->dev, "gps_host_wakeup", gpio);
	control_data->host_wakeup_gpio = gpio;
	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "GPS HOST WAKEUP GPIO %d.\n",
	       control_data->host_wakeup_gpio);

	/* Set up regulator */
	control_data->supply = regulator_get(&pdev->dev, "gps");
	if (IS_ERR(control_data->supply)) {
		err = PTR_ERR(control_data->supply);
		KDEBUG(SSV_GPS_ERROR, "Regulator get failed(%d)\n", err);
		goto error_get_reg;
	}

	/* Set up SPI */
	spi_bus = of_get_property(node, "mot,spi-bus", &len);
	if (!spi_bus || len < sizeof(*spi_bus)) {
		KDEBUG(SSV_GPS_ERROR, "Failed to read spi bus number.\n");
		goto error_spi_bus;
	}
	control_data->spi_bus = be32_to_cpup(spi_bus);

	spi_freq = of_get_property(node, "mot,gps-spi-max-frequency", &len);
	if (!spi_freq || len < sizeof(*spi_freq)) {
		KDEBUG(SSV_GPS_ERROR,
		       "Failed to read GPS spi max frequency.\n");
		goto error_spi_freq;
	}
	control_data->spi_freq = be32_to_cpup(spi_freq);

	chip_sel = of_get_property(node, "mot,chip-select", &len);
	if (!chip_sel || len < sizeof(*chip_sel)) {
		KDEBUG(SSV_GPS_ERROR, "Failed to read GPS chip select.\n");
		goto error_chip_sel;
	}
	control_data->chip_sel = be32_to_cpup(chip_sel);

	{
	struct spi_board_info ssv_gps_spi_info[] = {
		{
			.modalias = "spidev",
			.bus_num = control_data->spi_bus,
			.chip_select = control_data->chip_sel,
			.max_speed_hz = control_data->spi_freq,
		},
	};
	KDEBUG(SSV_GPS_VERBOSE_DEBUG,
		"Registering GPS SPIDEV bus %d,max freq %d.\n",
		control_data->spi_bus, control_data->spi_freq);

	err = spi_register_board_info(ssv_gps_spi_info,
					ARRAY_SIZE(ssv_gps_spi_info));

	if (err) {
		KDEBUG(SSV_GPS_ERROR,
		       "Failed to register GPS spidev device (%d).\n", err);
		goto error_reg_spidev;
	}
	}

	return 0;

error_reg_spidev:
error_spi_freq:
error_chip_sel:
error_spi_bus:
	regulator_put(control_data->supply);
error_get_reg:
	gpio_free(control_data->host_wakeup_gpio);
	control_data->host_wakeup_gpio = -1;
error_wakeup:
	gpio_free(control_data->reset_gpio);
	control_data->reset_gpio = -1;
error_reset:
	gpio_free(control_data->on_off_gpio);
	control_data->on_off_gpio = -1;
error_on_off:
	gpio_free(control_data->irq_gpio);
	control_data->irq_gpio = -1;
error_irq:
	gpio_free(control_data->boot_select_gpio);
	control_data->boot_select_gpio = -1;
error_boot_select:
	return err;
}


static int ssv_gps_create_sysfs_files(struct platform_device *pdev)
{
	int err;
	err = device_create_file(&pdev->dev,
			&dev_attr_enable_regulator);

	return err;
}

static void ssv_gps_remove_sysfs_files(struct platform_device *pdev)
{
	device_remove_file(&pdev->dev,
			&dev_attr_enable_regulator);
}

static int ssv_gps_probe(struct platform_device *pdev)
{
	int err = -EINVAL;
	struct ssv_control *control_data;
	struct device_node *node = pdev->dev.of_node;

	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "%s:\n", __func__);
	control_data = kzalloc(sizeof(*control_data), GFP_KERNEL);
	if (!control_data)
		return -ENOMEM;

	platform_set_drvdata(pdev, control_data);
	global_control_data = control_data;

	err = ssv_gps_hw_init(pdev, control_data, node);
	if (err < 0) {
		KDEBUG(SSV_GPS_ERROR, "%s: hw_init failed!", __func__);
		goto free_mem;
	}

	err = ssv_gps_create_sysfs_files(pdev);
	if (err < 0) {
		KDEBUG(SSV_GPS_ERROR, "%s: sysfs_init failed!", __func__);
		goto free_hw;
	}

	err = misc_register(&ssv_gps_misc_device);
	if (err < 0) {
		KDEBUG(SSV_GPS_ERROR, "Error registering %s driver\n",
		       SSV_GPS_DRIVER_NAME);
		goto sysfs_deinit;
	}
	return 0;

sysfs_deinit:
	ssv_gps_remove_sysfs_files(pdev);
free_hw:
	ssv_gps_hw_deinit(control_data);
free_mem:
	global_control_data = NULL;
	platform_set_drvdata(pdev, NULL);
	kfree(control_data);
	return err;
}

static int __exit ssv_gps_remove(struct platform_device *pdev)
{
	struct ssv_control *control_data = platform_get_drvdata(pdev);

	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "%s:\n", __func__);
	if (regulator_is_enabled(control_data->supply))
		regulator_disable(control_data->supply);
	regulator_put(control_data->supply);
	ssv_gps_hw_deinit(control_data);
	misc_deregister(&ssv_gps_misc_device);
	ssv_gps_remove_sysfs_files(pdev);
	kfree(control_data);
	platform_set_drvdata(pdev, NULL);
	global_control_data = NULL;
	return 0;
}

static struct of_device_id ssv_gps_match_tbl[] = {
	{ .compatible = "mot,ssv_gps" },
	{},
};

static struct platform_driver ssv_gps_driver = {
	.probe = ssv_gps_probe,
	.remove = __exit_p(ssv_gps_remove),
	.shutdown = NULL,
	.suspend = NULL,
	.resume = NULL,
	.driver = {
		.name = SSV_GPS_DRIVER_NAME,
		.owner = THIS_MODULE,
		.of_match_table = of_match_ptr(ssv_gps_match_tbl),
	},
};

static int __init ssv_gps_init(void)
{
	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "%s:\n", __func__);
	return platform_driver_register(&ssv_gps_driver);
}

static void __exit ssv_gps_exit(void)
{
	KDEBUG(SSV_GPS_VERBOSE_DEBUG, "%s:\n", __func__);
	platform_driver_unregister(&ssv_gps_driver);
}

module_init(ssv_gps_init);
module_exit(ssv_gps_exit);

MODULE_ALIAS("platform:ssv_gps");
MODULE_DESCRIPTION("SiRF Star V GPS driver");
MODULE_AUTHOR("Motorola");
MODULE_LICENSE("GPL");
