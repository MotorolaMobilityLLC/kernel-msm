/*
 * leds-msm-pmic.c - MSM PMIC LEDs driver.
 *
 * Copyright (c) 2009, Code Aurora Forum. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 */

#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/qpnp/pwm.h>
#include <linux/time.h>
#include <linux/of.h>
#include <linux/slab.h>
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/device.h>
#include <linux/leds.h>
#include <linux/err.h>
#include "leds.h"
#include "../video/msm/mdss/mdss_asus_debug.h"		//ASUS_BSP +++ Maggie_Lee "Support ambient mode"

/* Debug levels */
#define NO_DEBUG       0
#define DEBUG_POWER     1
#define DEBUG_INFO  2
#define DEBUG_VERBOSE 5
#define DEBUG_RAW      8
#define DEBUG_TRACE   10

static int debug = DEBUG_INFO;

module_param(debug, int, 0644);

MODULE_PARM_DESC(debug, "Activate debugging output");

#define backlight_debug(level, ...) \
		if (debug >= (level)) \
			pr_info(__VA_ARGS__);

#define MAX_BACKLIGHT_BRIGHTNESS 4095
#define MIN_BACKLIGHT_BRIGHTNESS 0
#define BL_GAIN	10000
#define BL_SHIFT	108

void asus_set_bl_brightness(struct led_classdev *, int);

struct backlight_parameter {
	int min_index;
	int max_index;
	int default_index;
	int ambient_index;
};

struct asus_backlight_data {
	int max_value;
	int min_value;
	struct led_classdev *led_cdev;
	struct backlight_parameter phone;
}asus_backlight_pdata;

struct platform_device asus_led_device = {
	.name           = "asus-backlight",
	.id             = 0,
	.dev            = {
		.platform_data = &asus_backlight_pdata,
	},
};

int asus_set_backlight(int value)
{
	struct asus_backlight_data *pdata = asus_led_device.dev.platform_data;
	int index;
	int div = ((pdata->max_value-pdata->min_value)*BL_GAIN/(pdata->phone.max_index-pdata->phone.min_index));

	if (value == 0)
		index = is_ambient_on() ? pdata->phone.ambient_index : 0;
	else if (value >= pdata->max_value)
        	index = pdata->phone.max_index;
	else if (value > 0 && value <= pdata->min_value) 
		index = pdata->phone.min_index;
	else if (value > pdata->min_value && value < pdata->max_value) 
		index = value*BL_GAIN/div+BL_SHIFT;
	else {
		printk("[BL] value (%d) not within spec, set to default brightness\n", value);
		index = pdata->phone.default_index;//value not in spec, do set default value
	}

	printk("[BL] %s: backlight value(%d), final brightness(%d) \n", __func__, value, index);
	__led_set_brightness(pdata->led_cdev, index);

	return 0;
}

void asus_set_bl_brightness(struct led_classdev *led_cdev, int value)
{
	struct asus_backlight_data *pdata = asus_led_device.dev.platform_data;
	pdata->led_cdev = led_cdev;

	backlight_debug(DEBUG_INFO,"[BL] (%s): phone_set_backlight %d \n", __func__, value);
	asus_set_backlight(value);
}

static int asus_bl_parse_dt(struct platform_device *pdev,struct asus_backlight_data *pdata)
{
	struct device_node *np = pdev->dev.of_node;
	u32 res[6];
	int rc = 0;

	//parse max/min/ambient BL value
	rc = of_property_read_u32_array(np, "value", res, 2);
	if (rc) {
		pr_err("%s:%d, value not specified\n",__func__, __LINE__);
		return -EINVAL;
	}
	pdata->min_value = (!rc ? res[0] : 17);
	pdata->max_value = (!rc ? res[1] : 255);
	printk("[BL]pdata->min_value  = %d\n",pdata->min_value);
	printk("[BL]pdata->max_value  = %d\n",pdata->max_value);

	//parse phone BL index setting
	rc = of_property_read_u32_array(np, "phone-bl-param", res, 4);
	if (rc) {
		pr_err("%s:%d, bl-tout-defau-level not specified\n",__func__, __LINE__);
		return -EINVAL;
	}
	pdata->phone.min_index = (!rc ? res[0] : 113);
	pdata->phone.max_index = (!rc ? res[1] : 255);
	pdata->phone.default_index= (!rc ? res[2] : 155);
	pdata->phone.ambient_index= (!rc ? res[3] : 113);
	printk("[BL]pdata->phone.min_index = %d\n",pdata->phone.min_index);
	printk("[BL]pdata->phone.max_index = %d\n",pdata->phone.max_index);
	printk("[BL]pdata->phone.default_index = %d\n",pdata->phone.default_index);
	printk("[BL]pdata->phone.ambient_index = %d\n",pdata->phone.ambient_index);

	return rc;
}

static int asus_backlight_probe(struct platform_device *pdev)
{
	struct asus_backlight_data *pdata;
	int ret =0;
	int rc =0;

	printk( "[BL]asus_backlight_probe+++\n");
	
	pdata = kzalloc(sizeof(struct asus_backlight_data), GFP_KERNEL);
	if(!pdata){
		printk("%s: failed to allocate device data\n", __func__);
		ret = -EINVAL;
		goto exit;
	}        

	if (pdata == NULL) {
        	pr_err("%s.invalid platform data.\n", __func__);
        	ret = -ENODEV;
		goto exit;
	}

	rc = asus_bl_parse_dt(pdev, pdata);
	if (rc)
		return rc;

	memcpy(&asus_led_device.dev.platform_data, &pdata, sizeof(pdata));

	device_rename(&pdev->dev, "asus_backlight");

	printk( "[BL]asus_backlight_probe---\n");

	return 0;

exit:
	printk("[BL]asus_backlight_probe----failed\n");
	return ret;
}

static const struct of_device_id BL_dt_match[] = {
	{ .compatible = "qcom, asus-backlight",},
	{}
};

static struct platform_driver this_driver = {
    .probe  = asus_backlight_probe,
    .driver = {
    	.name   = "backlight",
	.of_match_table = BL_dt_match,
    },
};

static int __init msm_pmic_led_init(void)
{
	return platform_driver_register(&this_driver);
}

static void __exit msm_pmic_led_exit(void)
{
    platform_driver_unregister(&this_driver);
}

module_init(msm_pmic_led_init);
module_exit(msm_pmic_led_exit);

MODULE_DESCRIPTION("MSM PMIC8226 ASUS backlight driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:board-8226");
