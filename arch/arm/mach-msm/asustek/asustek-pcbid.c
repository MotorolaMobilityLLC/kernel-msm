/*
 * arch/arm/mach-msm/asustek/asustek-pcbid.c
 *
 * Copyright (C) 2012 ASUSTek Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/module.h>
#include <linux/bitops.h>
#include <linux/gpio.h>
#include <linux/string.h>
#include <mach/board_asustek.h>
#include <asm/mach-types.h>

#define PCBID_VALUE_INVALID 0x4E2F4100 /* N/A */

enum {
	DEBUG_STATE = 1U << 0,
	DEBUG_VERBOSE = 1U << 1,
};

static int debug_mask = DEBUG_STATE;
module_param_named(debug_mask, debug_mask, int, S_IRUGO | S_IWUSR | S_IWGRP);

static unsigned int asustek_pcbid = PCBID_VALUE_INVALID;
static const char *asustek_chipid;
static unsigned int tp_type_pcbid[] = {0, 1};
static unsigned int lcd_type_pcbid[] = {2, 3};
static unsigned int hw_rev_pcbid[] = {4, 5};
static unsigned int lcd_pwm_pcbid[] = {8};

struct pcbid_maps
{
	unsigned char name[16];
	unsigned int *pcbid;
	unsigned int pcbid_num;
} asustek_pcbid_maps[] = {
	{"TP_TYPE", tp_type_pcbid, ARRAY_SIZE(tp_type_pcbid)},
	{"LCD_TYPE", lcd_type_pcbid, ARRAY_SIZE(lcd_type_pcbid)},
	{"HW_REV", hw_rev_pcbid, ARRAY_SIZE(hw_rev_pcbid)},
	{"LCD_PWM_TYPE", lcd_pwm_pcbid, ARRAY_SIZE(lcd_pwm_pcbid)},
};

#define NUM_MAPS (sizeof asustek_pcbid_maps / sizeof asustek_pcbid_maps[0])

int get_pcbid_type(const char *func)
{
	int i = 0, ret = 0;
	struct pcbid_maps *map = NULL;

	if (asustek_pcbid == PCBID_VALUE_INVALID) {
		pr_err("ASUSTek PCBID was invalid\n");
		return -ENODEV;
	}

	for (i = 0; i < NUM_MAPS; i++) {
		if (!strcmp(func, asustek_pcbid_maps[i].name)) {
			if (debug_mask & DEBUG_VERBOSE)
				pr_info("%s was found\n", func);

			map = &asustek_pcbid_maps[i];
			break;
		}
	}

	if (map) {
		/* found */
		for (i = 0; i < map->pcbid_num; i++) {
			ret += asustek_pcbid & BIT(map->pcbid[i]);
		}
		ret = ret >> map->pcbid[0];
	}
	else
		ret = -ENODEV;

	return ret;
}

tp_type asustek_get_tp_type(void)
{
	tp_type ret = TP_TYPE_INVALID;

	ret = get_pcbid_type("TP_TYPE");

	if (debug_mask & DEBUG_VERBOSE)
		pr_info("%s: %d\n", __func__, ret);

	if ((ret == -ENODEV) || (ret >= TP_TYPE_MAX))
		ret = TP_TYPE_INVALID;

	return ret;
}
EXPORT_SYMBOL(asustek_get_tp_type);

lcd_type asustek_get_lcd_type(void)
{
	lcd_type ret = LCD_TYPE_INVALID;

	ret = get_pcbid_type("LCD_TYPE");

	if (debug_mask & DEBUG_VERBOSE)
		pr_info("%s: %d\n", __func__, ret);

	if ((ret == -ENODEV) || (ret >= LCD_TYPE_MAX))
		ret = LCD_TYPE_INVALID;

	return ret;
}
EXPORT_SYMBOL(asustek_get_lcd_type);

lcd_pwm_type asustek_get_lcd_pwm_type(void)
{
	lcd_pwm_type ret = LCD_PWM_TYPE_INVALID;

	ret = get_pcbid_type("LCD_PWM_TYPE");

	if (debug_mask & DEBUG_VERBOSE)
		pr_info("%s: %d\n", __func__, ret);

	if ((ret == -ENODEV) || (ret >= LCD_PWM_TYPE_MAX))
		ret = LCD_PWM_TYPE_INVALID;

	return ret;
}
EXPORT_SYMBOL(asustek_get_lcd_pwm_type);


/* for asustek board revision */
static hw_rev asustek_hw_rev = HW_REV_INVALID;

static int __init hw_rev_setup(char *hw_rev_info)
{
	/* CAUTION:
	 * 	These strings come from bootloader through kernel cmdline.
	 *	rev_a is deprecated, and rev_e is supported starting from
	 *	bootloader with version FLO-2.08
	 */
	char *hw_rev_str[] = {"rev_e", "rev_b", "rev_c", "rev_d"};
	unsigned int i;

	if (debug_mask & DEBUG_STATE)
	pr_info("HW Revision: ASUSTek input %s \n", hw_rev_info);

	for (i = 0; i < HW_REV_MAX; i++) {
		if (!strcmp(hw_rev_info, hw_rev_str[i])) {
			asustek_hw_rev = (hw_rev) i;
			break;
		}
	}

	if (i == HW_REV_MAX) {
		pr_info("HW Revision: ASUSTek mismatched\n");
		return 1;
	}

	if (debug_mask & DEBUG_STATE)
		pr_info("HW Revision: ASUSTek matched %s \n",
			hw_rev_str[asustek_hw_rev]);

	return 1;
}

__setup("asustek.hw_rev=", hw_rev_setup);

hw_rev asustek_get_hw_rev(void)
{
	hw_rev ret = asustek_hw_rev;

	/* if valid hardware rev was inputed by bootloader */
	if (ret != HW_REV_INVALID)
		return ret;

	ret = get_pcbid_type("HW_REV");

	if (debug_mask & DEBUG_VERBOSE)
		pr_info("%s: %d\n", __func__, ret);

	if ((ret == -ENODEV) || (ret >= HW_REV_MAX))
		ret = HW_REV_INVALID;

	return ret;
}
EXPORT_SYMBOL(asustek_get_hw_rev);

#define ASUSTEK_PCBID_ATTR(module) \
static struct kobj_attribute module##_attr = { \
	.attr = { \
		.name = __stringify(module), \
		.mode = 0444, \
	}, \
	.show = module##_show, \
}

static ssize_t asustek_pcbid_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	char *s = buf;

	s += sprintf(s, "%03x\n", asustek_pcbid);

	return s - buf;
}

static ssize_t asustek_projectid_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	char *s = buf;

	s += sprintf(s, "%02x\n", (asustek_pcbid >> 6) & 0x3);

	return s - buf;
}

static ssize_t asustek_chipid_show(struct kobject *kobj,
	struct kobj_attribute *attr, char *buf)
{
	char *s = buf;

	s += sprintf(s, "%s\n", asustek_chipid);

	return s - buf;
}

ASUSTEK_PCBID_ATTR(asustek_pcbid);
ASUSTEK_PCBID_ATTR(asustek_projectid);
ASUSTEK_PCBID_ATTR(asustek_chipid);

static struct attribute *attr_list[] = {
	&asustek_pcbid_attr.attr,
	&asustek_projectid_attr.attr,
	&asustek_chipid_attr.attr,
	NULL,
};

static struct attribute_group attr_group = {
	.attrs = attr_list,
};

static int __init pcbid_driver_probe(struct platform_device *pdev)
{
	int i, ret = 0;
	struct resource *res;
	struct asustek_pcbid_platform_data *pdata;

	if (!pdev)
		return -EINVAL;

	pdata = pdev->dev.platform_data;

	if (pdata)
		asustek_chipid = kstrdup(pdata->UUID, GFP_KERNEL);

	asustek_pcbid = 0;

	for (i = 0; i < pdev->num_resources; i++) {
		res = platform_get_resource(pdev, IORESOURCE_IO, i);
		if (!res)
			return -ENODEV;

		if (debug_mask & DEBUG_VERBOSE)
			pr_info("ASUSTek: Requesting gpio%d for %s\n",
					res->start, res->name);

		if (machine_is_apq8064_flo() || machine_is_apq8064_deb()) {
			if ((asustek_hw_rev == HW_REV_C) ||
				(asustek_hw_rev == HW_REV_D) ||
				(asustek_hw_rev == HW_REV_E)) {
				if (i == 3) {
					pr_info("ASUSTek: Bypassing PCB_ID3\n");
					continue;
				}
			}
		}

		ret = gpio_request(res->start, res->name);
		if (ret) {
			/* indicate invalid pcbid value when error happens */
			pr_err("ASUSTek: Failed to request gpio%d for %s\n",
					res->start, res->name);
			asustek_pcbid = PCBID_VALUE_INVALID;
			res = NULL;
			break;
		}

		gpio_direction_input(res->start);
		asustek_pcbid |= gpio_get_value(res->start) << i;
	}

	if (asustek_pcbid == PCBID_VALUE_INVALID) {
		/* error handler to free allocated gpio resources */
		while (i >= 0) {
			res = platform_get_resource(pdev, IORESOURCE_IO, i);
			if (!res)
				return -ENODEV;

			if (debug_mask & DEBUG_VERBOSE)
				pr_info("ASUSTek: Freeing gpio%d for %s\n",
						res->start, res->name);

			gpio_free(res->start);
			i--;
		}
	}
	else {
		/* report pcbid info to dmesg */
		if (debug_mask && DEBUG_STATE)
			pr_info("ASUSTek: PCBID=%03x\n", asustek_pcbid);

		/* create a sysfs interface */
		ret = sysfs_create_group(&pdev->dev.kobj, &attr_group);

		if (ret)
			pr_err("ASUSTek: Failed to create sysfs group\n");
	}

	return ret;
}

static int __devexit pcbid_driver_remove(struct platform_device *pdev)
{
	return 0;
}

static struct platform_driver asustek_pcbid_driver __refdata = {
	.probe = pcbid_driver_probe,
	.remove = __devexit_p(pcbid_driver_remove),
	.driver = {
		.name = "asustek_pcbid",
		.owner = THIS_MODULE,
	},
};

static int __devinit asustek_pcbid_init(void)
{
	return platform_driver_register(&asustek_pcbid_driver);
}

postcore_initcall(asustek_pcbid_init);

MODULE_DESCRIPTION("ASUSTek PCBID driver");
MODULE_AUTHOR("Paris Yeh <paris_yeh@asus.com>");
MODULE_LICENSE("GPL");
