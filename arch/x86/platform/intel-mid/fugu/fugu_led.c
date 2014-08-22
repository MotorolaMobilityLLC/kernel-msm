/* Copyright (c) 2014, ASUSTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/rpmsg.h>
#include <asm/intel_scu_pmic.h>
#include <asm/intel_mid_rpmsg.h>

#include <linux/leds.h>

/* Charger LED Control Registr Definition */
#define CHRLEDCTRL_REG 0x53
/* Charger LED State Maching */
#define CHRLEDFSM_REG 0x54
/* Charger LED PWM Register Definiition */
#define CHRLEDPWM_REG 0x55

static struct rpmsg_instance *pmic_instance;

static int fugu_led_level(enum led_brightness value)
{
	if ((value >= 0) && (value < 50))
		return 1;
	else if ((value >= 50) && (value < 150))
		return 2;
	else if ((value >= 150) && (value < 200))
		return 3;
	else
		return 0;
}

static u8 is_enabled(void)
{
	u8 data;

	intel_scu_ipc_ioread8(CHRLEDCTRL_REG, &data);

	data &= BIT(1);

	return !!data;
}

static void fugu_led_enable(bool enable)
{
	u8 data;

	intel_scu_ipc_ioread8(CHRLEDCTRL_REG, &data);

	if (enable)
		data |= BIT(1);
	else
		data &= ~BIT(1);

	intel_scu_ipc_iowrite8(CHRLEDCTRL_REG, data);
}

static void fugu_led_brightness_set(struct led_classdev *led_cdev,
			     enum led_brightness value)
{
	s32 level;
	u8 data;

	level = fugu_led_level(value);

	intel_scu_ipc_ioread8(CHRLEDCTRL_REG, &data);
	data &= ~(BIT(3) | BIT(2));
	data |= (level << 2);
	intel_scu_ipc_iowrite8(CHRLEDCTRL_REG, data);
	led_cdev->brightness = value;
}

static enum led_brightness
fugu_led_brightness_get(struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static int fugu_led_freq_set(u8 freq)
{
	u8 data;

	if (freq > 3)
		return -EINVAL;

	intel_scu_ipc_ioread8(CHRLEDCTRL_REG, &data);
	data &= ~(BIT(5) | BIT(4));
	data |= (freq << 4);
	intel_scu_ipc_iowrite8(CHRLEDCTRL_REG, data);

	return 0;
}

static u8 fugu_led_freq_get(void)
{
	u8 data;

	intel_scu_ipc_ioread8(CHRLEDCTRL_REG, &data);
	data = data | (BIT(5) | BIT(4));
	data >>= 4;

	return data;
}

static int fugu_led_set_lighting_effect(u8 lighting_effect)
{
	u8 data;
	if (lighting_effect > 3)
		return -EINVAL;

	intel_scu_ipc_ioread8(CHRLEDFSM_REG, &data);
	data &= ~(BIT(2) | BIT(1));
	data |= (lighting_effect << 1);
	intel_scu_ipc_iowrite8(CHRLEDFSM_REG, data);

	return 0;
}

static u8 fugu_led_get_lighting_effect(void)
{
	u8 data;

	intel_scu_ipc_ioread8(CHRLEDFSM_REG, &data);
	data = data | (BIT(2) | BIT(1));
	data >>= 1;

	return data;
}

static ssize_t fugu_led_freq_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 freq;

	freq = fugu_led_freq_get();

	return snprintf(buf, PAGE_SIZE, "%d\n", freq);
}

static ssize_t fugu_led_freq_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	ret = fugu_led_freq_set(val);
	if (ret)
		dev_err(dev, "fail to set freq\n");
	 else
		ret = size;

	return ret;
}

static ssize_t fugu_led_enable_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%s\n", is_enabled() ? "enabled" : "disabled");
}

static ssize_t fugu_led_enable_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	fugu_led_enable(!!val);

	return size;
}

static ssize_t fugu_led_lighting_effect_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	u8 lighting_effect;

	lighting_effect = fugu_led_get_lighting_effect();

	return snprintf(buf, PAGE_SIZE, "%d\n", lighting_effect);
}

static ssize_t fugu_led_lighting_effect_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t size)
{
	unsigned long val;
	int ret;

	ret = kstrtoul(buf, 10, &val);
	if (ret)
		return ret;

	ret = fugu_led_set_lighting_effect(val);
	if (ret)
		dev_err(dev, "fail to set lighting effect\n");
	else
		ret = size;

	return ret;
}

static DEVICE_ATTR(led_freq, 0644, fugu_led_freq_show, fugu_led_freq_store);
static DEVICE_ATTR(led_enable, 0644,
	fugu_led_enable_show, fugu_led_enable_store);
static DEVICE_ATTR(led_lighting_effect, 0644,
	fugu_led_lighting_effect_show, fugu_led_lighting_effect_store);

static struct led_classdev fugu_white_led = {
	.name			= "white",
	.brightness		= LED_OFF,
	.max_brightness	= LED_FULL,
	.brightness_set		= fugu_led_brightness_set,
	.brightness_get		= fugu_led_brightness_get,
};

void led_init(void)
{
	/* LED is software controll and enabled */
	intel_scu_ipc_iowrite8(CHRLEDCTRL_REG, 3);

	/* LED lighting effect: Breathing varies Duty Cycle */
	intel_scu_ipc_iowrite8(CHRLEDFSM_REG, 6);

	/* LED PWM: 256/256. There are a total of 256 duty cycle steps */
	intel_scu_ipc_iowrite8(CHRLEDPWM_REG, 0xFF);

	fugu_led_brightness_set(&fugu_white_led, 0xFF);
}

static int fugu_led_probe(struct rpmsg_channel *rpdev)
{
	int ret = 0;

	if (rpdev == NULL) {
		pr_err("rpmsg channel not created\n");
		ret = -ENODEV;
		goto out;
	}

	dev_info(&rpdev->dev, "Probed pmic rpmsg device\n");

	/* Allocate rpmsg instance for pmic*/
	ret = alloc_rpmsg_instance(rpdev, &pmic_instance);
	if (!pmic_instance) {
		dev_err(&rpdev->dev, "kzalloc pmic instance failed\n");
		goto out;
	}
	/* Initialize rpmsg instance */
	init_rpmsg_instance(pmic_instance);

	ret = led_classdev_register(&rpdev->dev, &fugu_white_led);
	if (ret < 0) {
		dev_err(&rpdev->dev, "fail to register led class.");
		goto out;
	}

	ret = device_create_file(&rpdev->dev, &dev_attr_led_freq);
	if (ret)
		dev_err(&rpdev->dev, "failed device_create_file(led_freq)\n");

	ret = device_create_file(&rpdev->dev, &dev_attr_led_enable);
	if (ret)
		dev_err(&rpdev->dev, "failed device_create_fiaild(led_enable)\n");

	ret = device_create_file(&rpdev->dev, &dev_attr_led_lighting_effect);
	if (ret)
		dev_err(&rpdev->dev, "failed device_create_file(led_lighting_effect)\n");

	/* Initialize led */
	led_init();
out:
	return ret;
}

static void fugu_led_remove(struct rpmsg_channel *rpdev)
{
	free_rpmsg_instance(rpdev, &pmic_instance);
	dev_info(&rpdev->dev, "Removed pmic rpmsg device\n");
}

static void fugu_led_cb(struct rpmsg_channel *rpdev, void *data,
					int len, void *priv, u32 src)
{
	dev_warn(&rpdev->dev, "unexpected, message\n");

	print_hex_dump(KERN_DEBUG, __func__, DUMP_PREFIX_NONE, 16, 1,
		       data, len,  true);
}

static struct rpmsg_device_id fugu_led_id_table[] = {
	{ .name	= "rpmsg_fugu_led" },
	{ },
};
MODULE_DEVICE_TABLE(rpmsg, fugu_led_id_table);

static struct rpmsg_driver fugu_led = {
	.drv.name	= KBUILD_MODNAME,
	.drv.owner	= THIS_MODULE,
	.id_table	= fugu_led_id_table,
	.probe		= fugu_led_probe,
	.callback	= fugu_led_cb,
	.remove		= fugu_led_remove,
};

static int __init fugu_led_init(void)
{
	return register_rpmsg_driver(&fugu_led);
}

module_init(fugu_led_init);

static void __exit fugu_led_exit(void)
{
	return unregister_rpmsg_driver(&fugu_led);
}
module_exit(fugu_led_exit);

MODULE_AUTHOR("Alan Lu<alan_lu@asus.com>");
MODULE_DESCRIPTION("Fugu Led Driver");
MODULE_LICENSE("GPL v2");
