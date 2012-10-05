/*
 * LEDs driver for GPIOs with blinking done via Qualcomm LPGs
 *
 * Copyright (C) 2011 Motorola Mobility, Inc
 * Alina Yakovleva <qvdh43@motorola.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 */
#include <linux/module.h>
#include <linux/kernel.h>
#include <linux/init.h>
#include <linux/platform_device.h>
#include <linux/leds.h>
#include <linux/pwm.h>
#include <linux/leds-pwm-gpio.h>
#include <linux/slab.h>
#include <linux/gpio.h>

#include <linux/mfd/pm8xxx/core.h>
#include <linux/mfd/pm8xxx/pwm.h>
#include <linux/leds-pm8xxx.h>

struct pm8xxx_rgb_led_drv_data;

struct pm8xxx_rgb_led_data {
	struct led_classdev cdev;
	struct pwm_device *pwm;
	unsigned gpio;
	int pwm_id;
	unsigned value;
	u8 can_sleep;
	u8 active_low;
	u8 blinking;
	struct pm8xxx_rgb_led_drv_data *drv_data;
};

struct pm8xxx_rgb_led_drv_data {
	struct pm8xxx_rgb_led_data *leds;
	unsigned num_leds;
	unsigned long delay_on;
	unsigned long delay_off;
	unsigned long ramp_up;
	unsigned long ramp_down;
	unsigned blinking;
	int max_brightness;
};

#define PM8XXX_RGB_PWM_FLAGS	(PM_PWM_LUT_LOOP | PM_PWM_LUT_RAMP_UP |\
	PM_PWM_LUT_PAUSE_HI_EN | PM_PWM_LUT_PAUSE_LO_EN)

static int flags = PM8XXX_RGB_PWM_FLAGS;
static int duty_ms = 50;
static int period;
static int pause_lo, pause_hi;
static int start_idx;
static int duty_pcts_ramp[PM_PWM_LUT_SIZE] = {0};
static int num_idx = ARRAY_SIZE(duty_pcts_ramp);

module_param(flags, int, 0644);
module_param(duty_ms, int, 0644);
module_param(period, int, 0644);
module_param(pause_lo, int, 0644);
module_param(pause_hi, int, 0644);
module_param(num_idx, int, 0644);
module_param(start_idx, int, 0644);
module_param_array_named(pcts, duty_pcts_ramp, uint, NULL, 0644);

static int pm8xxx_rgb_blink_set(struct pm8xxx_rgb_led_drv_data *drv_data,
	unsigned on)
{
	/* num_up and num_down are percentages of times for ramping */
	static int num_up = ((PM_PWM_LUT_SIZE-1) * 1)/2;
	int num_down;
	/* num_top is percentage of time to stay up solid */
	int num_top;
	int i;
	int ret = 0;
	int init_max, max = 100;
	int max_level;
	int step;
	int (*array_ptr)[];

	drv_data->blinking = 0;
	if (!on) {
		pr_debug("%s: turning blinking off\n", __func__);
		for (i = 0; i < drv_data->num_leds; i++) {
			pwm_disable(drv_data->leds[i].pwm);
			pm8xxx_pwm_lut_enable(drv_data->leds[i].pwm, 0);
			/* If brightness is not 0 turn it on */
			drv_data->leds[i].blinking = 0;
			if (drv_data->leds[i].cdev.brightness) {
				ret = pwm_config(drv_data->leds[i].pwm,
					PM8XXX_PWM_PERIOD_MAX,
					PM8XXX_PWM_PERIOD_MAX);
				if (ret)
					pr_err("%s: pwm_config err %d\n",
						__func__, ret);
				ret = pwm_enable(drv_data->leds[i].pwm);
				if (ret)
					pr_err("%s: pwm_enable err %d\n",
						__func__, ret);
			}
		}
		return ret;
	}
	if (!drv_data->ramp_up && !drv_data->ramp_down) {
		/* We need to configure all PWMs first and then enable
		them first so that it's as synchronous as possible */
		for (i = 0; i < drv_data->num_leds; i++) {
			pwm_disable(drv_data->leds[i].pwm);
			pm8xxx_pwm_lut_enable(drv_data->leds[i].pwm, 0);
			if (!drv_data->leds[i].cdev.brightness)
				continue;
			ret = pwm_config(drv_data->leds[i].pwm,
				drv_data->delay_on * 1000,
				(drv_data->delay_on + drv_data->delay_off)
				*1000);
			if (ret) {
				pr_err("%s: pwm_config error %d\n",
					__func__, ret);
				return ret;
			}
		}
		for (i = 0; i < drv_data->num_leds; i++) {
			if (!drv_data->leds[i].cdev.brightness)
				continue;
			ret = pwm_enable(drv_data->leds[i].pwm);
			if (ret) {
				pr_err("%s: pwm_enable error %d\n",
					__func__, ret);
				return ret;
			}
			drv_data->leds[i].blinking = 1;
			drv_data->blinking++;
		}
	} else {
		num_down = num_up;
		num_top = PM_PWM_LUT_SIZE - num_up - num_down - 1;
		/* Last index is 0 for delay off */
		array_ptr = &duty_pcts_ramp;
		duty_pcts_ramp[PM_PWM_LUT_SIZE-1] = 0;
		num_idx = ARRAY_SIZE(duty_pcts_ramp);
		duty_ms = drv_data->delay_on/(PM_PWM_LUT_SIZE-1);
		if (drv_data->delay_on - duty_ms * (PM_PWM_LUT_SIZE-1) >
			(duty_ms+1)*(PM_PWM_LUT_SIZE-1) - drv_data->delay_on)
			duty_ms++;
		pause_hi = drv_data->delay_off - duty_ms;
		pause_lo = 0;
		period = duty_ms * PM_PWM_LUT_SIZE + pause_hi;

		/* Equalize all LEDs' levels to max */
		max_level = drv_data->leds[0].value;
		for (i = 1; i < drv_data->num_leds; i++) {
			if (drv_data->leds[i].value > max_level)
				max_level = drv_data->leds[i].value;
		}
		init_max = 100 * max_level/LED_FULL;
		/* Ramp up */
		max = init_max;
		step = max/(num_up - 1);
		if (drv_data->ramp_up) {
			for (i = num_up-1; i >= 0; i--) {
				duty_pcts_ramp[i] = max;
				if (!step) {
					if (i % 2)
						max -= 1;
				} else
					max -= step;
				if (max < 1)
					max = 1;
			}
		} else {
			for (i = num_up-1; i >= 0; i--)
				duty_pcts_ramp[i] = max;
		}
		/* Top */
		max = init_max;
		for (i = num_up; i < num_up+num_top; i++)
			duty_pcts_ramp[i] = max;
		/* Ramp down */
		max = init_max;
		if (drv_data->ramp_down) {
			for (i = num_down+num_top; i <= PM_PWM_LUT_SIZE-2;
				i++) {
				duty_pcts_ramp[i] = max;
				if (!step) {
					if (i % 2)
						max -= 1;
				} else
					max -= step;
				if (max < 1)
					max = 1;
			}
		} else {
			for (i = num_down+num_top; i <= PM_PWM_LUT_SIZE-2; i++)
				duty_pcts_ramp[i] = max;
		}
		/* We need to configure all PWMs first and then enable
		them first so that it's as synchronous as possible */
		for (i = 0; i < drv_data->num_leds; i++) {
			pwm_disable(drv_data->leds[i].pwm);
			pm8xxx_pwm_lut_enable(drv_data->leds[i].pwm, 0);
			if (!drv_data->leds[i].cdev.brightness)
				continue;
			ret = pm8xxx_pwm_lut_config(drv_data->leds[i].pwm,
				period,
				(int *)array_ptr, duty_ms, start_idx,
				num_idx, pause_lo, pause_hi, flags);
			if (ret)
				pr_err("%s: pm8xxx_pwm_lut_config error %d\n",
					__func__, ret);
		}
		for (i = 0; i < drv_data->num_leds; i++) {
			if (!drv_data->leds[i].cdev.brightness)
				continue;
			ret = pm8xxx_pwm_lut_enable(drv_data->leds[i].pwm, 1);
			if (ret)
				pr_err("%s: pm8xxx_pwm_lut_enable error %d\n",
					__func__, ret);
			drv_data->leds[i].blinking = 1;
			drv_data->blinking++;
		}
	}

	return ret;
}

static void pm8xxx_rgb_led_set(struct led_classdev *led_cdev,
	enum led_brightness value)
{
	struct pm8xxx_rgb_led_data *led =
		container_of(led_cdev, struct pm8xxx_rgb_led_data, cdev);
	struct pm8xxx_rgb_led_drv_data *drv_data = led->drv_data;
	int ret;

	pr_debug("%s: %s, %d\n", __func__, led->cdev.name, value);

	pwm_disable(led->pwm);
	if (!value) {
		if (led->blinking) {
			led->blinking = 0;
			drv_data->blinking--;
		}
		return;
	}
	/* Scale by max_brightness */
	led->value = value * drv_data->max_brightness/LED_FULL;

	/* If the whole thing is blinking we need to blink this one as well;
	but we need to stop and restart blinking so that it's synchronous */
	if (drv_data->blinking) {
		pm8xxx_rgb_blink_set(drv_data, 1);
	} else {
		ret = pwm_config(led->pwm, 1000 * led->value / LED_FULL, 1000);
		if (ret)
			pr_err("%s: pwm_config error %d\n", __func__, ret);
		pwm_enable(led->pwm);
		if (ret)
			pr_err("%s: pwm_enable error %d\n", __func__, ret);
	}
}

static ssize_t pm8xxx_rgb_blink_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pm8xxx_rgb_led_data *led_dat = dev_get_drvdata(dev);
	struct pm8xxx_rgb_led_drv_data *drv_data = led_dat->drv_data;

	snprintf(buf, 100,
		"on/off=%ld/%ld ms, rup/rdown=%ld/%ld, blinking=%d\n",
		drv_data->delay_on, drv_data->delay_off,
		drv_data->ramp_up, drv_data->ramp_down, drv_data->blinking);
	return strnlen(buf, 100);
}

static ssize_t pm8xxx_rgb_blink_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct pm8xxx_rgb_led_data *led_dat = dev_get_drvdata(dev);
	struct pm8xxx_rgb_led_drv_data *drv_data = led_dat->drv_data;
	/* Parameters are: led on, led off, ramp up, ramp down */
	unsigned long params[] = {0, 0, 0, 0};
	char *ptr = (char *)buf;
	char *rem_ptr;
	int i, ret = 0;

	/* Parse 4 space separated parameters */
	for (i = 0; i < ARRAY_SIZE(params); i++) {
		if (ptr) {
			while (*ptr == ' ' || *ptr == '\t')
				ptr++;
		} else
			break;
		params[i] = simple_strtoul(ptr, &rem_ptr, 10);
		ptr = rem_ptr;
	}
	pr_debug("%s: on/off=%d/%d, rup=%d, rdown=%d\n", __func__,
		(unsigned)params[0], (unsigned)params[1],
		(unsigned)params[2], (unsigned)params[3]);
	drv_data->delay_on = params[0];
	drv_data->delay_off = params[1];
	drv_data->ramp_up = params[2];
	drv_data->ramp_down = params[3];

	/* If either params[0] or params[1] is 0 blinking is turned off */
	if (params[0] && params[1] && drv_data->blinking)
		pm8xxx_rgb_blink_set(drv_data, 0);
	ret = pm8xxx_rgb_blink_set(drv_data, params[0] && params[1]);
	return strlen(buf);
}

static ssize_t pm8xxx_rgb_max_store(struct device *dev,
	struct device_attribute *attr, const char *buf, size_t count)
{
	struct pm8xxx_rgb_led_data *led_dat = dev_get_drvdata(dev);
	struct pm8xxx_rgb_led_drv_data *drv_data = led_dat->drv_data;
	int max_brightness = LED_FULL;
	int ret;

	ret = kstrtoint(buf, 10, &max_brightness);
	if (ret != 0)
		return -EINVAL;
	if (max_brightness > LED_FULL || max_brightness == 0)
		max_brightness = LED_FULL;
	drv_data->max_brightness = max_brightness;

	return count;
}

static ssize_t pm8xxx_rgb_max_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	struct pm8xxx_rgb_led_data *led_dat = dev_get_drvdata(dev);
	struct pm8xxx_rgb_led_drv_data *drv_data = led_dat->drv_data;

	snprintf(buf, 5, "%d\n", drv_data->max_brightness);
	return strnlen(buf, 5);
}

static DEVICE_ATTR(blink, 0664, pm8xxx_rgb_blink_show, pm8xxx_rgb_blink_store);
static DEVICE_ATTR(max, 0664, pm8xxx_rgb_max_show, pm8xxx_rgb_max_store);

static struct attribute *pm8xxx_rgb_led_attributes[] = {
	&dev_attr_blink.attr,
	&dev_attr_max.attr,
	NULL,
};

static struct attribute_group pm8xxx_rgb_attribute_group = {
	.attrs = pm8xxx_rgb_led_attributes
};

static int __devinit create_pm8xxx_rgb_led(const struct led_pwm_gpio *led,
	struct pm8xxx_rgb_led_data *led_dat, struct device *parent)
{
	int ret, state;

	led_dat->gpio = -1;
	led_dat->pwm_id = -1;

	/* skip leds that aren't available */
	if (!gpio_is_valid(led->gpio)) {
		pr_err("%s: skipping unavailable LED gpio %d (%s)\n",
			__func__, led->gpio, led->name);
		return 0;
	}
	if (led->pwm_id < 0) {
		pr_err("%s: no PWM ID for \'%s\'\n", __func__, led->name);
		return -EINVAL;
	}

	ret = gpio_request(led->gpio, led->name);
	if (ret < 0) {
		pr_err("%s: gpio_request(%d, %s) failed\n",
			__func__, led->gpio, led->name);
		return ret;
	}

	led_dat->cdev.name = led->name;
	led_dat->cdev.default_trigger = led->default_trigger;
	led_dat->gpio = led->gpio;
	led_dat->pwm_id = led->pwm_id;
	led_dat->active_low = led->active_low;
	led_dat->can_sleep = gpio_cansleep(led->gpio);
	led_dat->cdev.brightness_set = pm8xxx_rgb_led_set;
	if (led->default_state == LEDS_GPIO_DEFSTATE_KEEP)
		state = !!gpio_get_value(led_dat->gpio) ^ led_dat->active_low;
	else
		state = (led->default_state == LEDS_GPIO_DEFSTATE_ON);
	led_dat->cdev.brightness = state ? LED_FULL : LED_OFF;
	if (!led->retain_state_suspended)
		led_dat->cdev.flags |= LED_CORE_SUSPENDRESUME;

	ret = gpio_direction_output(led_dat->gpio, led_dat->active_low ^ state);
	if (ret < 0) {
		pr_err("%s: gpio_direction_output(%d, %d) failed\n",
			__func__, led_dat->gpio, led_dat->active_low ^ state);
		goto err;
	}

	ret = led_classdev_register(parent, &led_dat->cdev);
	if (ret < 0) {
		pr_err("%s: unable to register LED class for %s\n",
			__func__, led_dat->cdev.name);
		goto err;
	}
	pr_info("%s: LED class %s, gpio %d, can sleep = %d\n",
		__func__, led->name, led->gpio, led_dat->can_sleep);

	if (!strcmp(led_dat->cdev.name, "red")) {
		ret = sysfs_create_group(&led_dat->cdev.dev->kobj,
			&pm8xxx_rgb_attribute_group);
		if (ret) {
			pr_err("%s: unable to create attribute group for %s\n",
				__func__, led_dat->cdev.name);
			goto err_attr;
		}
	}

	led_dat->pwm = pwm_request(led_dat->pwm_id, led_dat->cdev.name);
	if (!led_dat->pwm) {
		pr_err("%s: pwm_request error for %s, pwm_id %d\n",
			__func__, led_dat->cdev.name, led_dat->pwm_id);
		goto err_pwm_request;
	}
	pr_info("%s: requested pwm for %s, pwm_id=%d\n",
		__func__, led_dat->cdev.name, led_dat->pwm_id);

	return 0;

err_pwm_request:
	sysfs_remove_group(&led_dat->cdev.dev->kobj,
		&pm8xxx_rgb_attribute_group);
err_attr:
	led_classdev_unregister(&led_dat->cdev);
err:
	gpio_free(led_dat->gpio);
	return ret;
}

static void delete_pm8xxx_rgb_led(struct pm8xxx_rgb_led_data *led)
{
	if (!gpio_is_valid(led->gpio))
		return;
	sysfs_remove_group(&led->cdev.dev->kobj,
		&pm8xxx_rgb_attribute_group);
	led_classdev_unregister(&led->cdev);
	gpio_free(led->gpio);
	pwm_free(led->pwm);
}

static int __devinit pm8xxx_rgb_led_probe(struct platform_device *pdev)
{
	struct led_pwm_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct pm8xxx_rgb_led_drv_data *drv_data;
	struct pm8xxx_rgb_led_data *leds_data;
	int i, ret;

	if (!pdata) {
		pr_err("%s: no platform data supplied\n", __func__);
		return -EINVAL;
	}
	if (pdata->num_leds <= 0) {
		pr_err("%s: invalid number of LEDs %d\n", __func__,
			pdata->num_leds);
		return -EINVAL;
	}

	drv_data = kzalloc(sizeof(struct pm8xxx_rgb_led_drv_data), GFP_KERNEL);
	if (!drv_data) {
		pr_err("%s: kzalloc error allocating drv_data\n", __func__);
		return -ENOMEM;
	}

	leds_data = kzalloc(
		sizeof(struct pm8xxx_rgb_led_data) * pdata->num_leds,
		GFP_KERNEL);
	if (!leds_data) {
		pr_err("%s: kzalloc error allocating leds_data\n", __func__);
		kfree(drv_data);
		return -ENOMEM;
	}
	drv_data->leds = leds_data;
	drv_data->num_leds = pdata->num_leds;
	drv_data->blinking = 0;
	drv_data->max_brightness = pdata->max_brightness;
	if (drv_data->max_brightness == 0)
		drv_data->max_brightness = LED_FULL;

	pr_info("%s: num_leds is %d\n", __func__, pdata->num_leds);
	for (i = 0; i < pdata->num_leds; i++) {
		ret = create_pm8xxx_rgb_led(&pdata->leds[i], &leds_data[i],
			&pdev->dev);
		leds_data[i].drv_data = drv_data;
		if (ret < 0)
			goto err;
	}

	platform_set_drvdata(pdev, leds_data);

	return 0;

err:
	for (i = i - 1; i >= 0; i--)
		delete_pm8xxx_rgb_led(&leds_data[i]);

	kfree(leds_data);
	kfree(drv_data);

	return ret;
}

static int __devexit pm8xxx_rgb_led_remove(struct platform_device *pdev)
{
	int i;
	struct led_pwm_gpio_platform_data *pdata = pdev->dev.platform_data;
	struct pm8xxx_rgb_led_data *leds_data;
	struct pm8xxx_rgb_led_drv_data *drv_data;

	leds_data = platform_get_drvdata(pdev);

	drv_data = leds_data[0].drv_data;
	for (i = 0; i < pdata->num_leds; i++)
		delete_pm8xxx_rgb_led(&leds_data[i]);

	kfree(leds_data);
	kfree(drv_data);

	return 0;
}

static struct platform_driver pm8xxx_rgb_led_driver = {
	.probe		= pm8xxx_rgb_led_probe,
	.remove		= __devexit_p(pm8xxx_rgb_led_remove),
	.driver		= {
		.name	= "pm8xxx_rgb_leds",
		.owner	= THIS_MODULE,
	},
};

static int __init pm8xxx_rgb_led_init(void)
{
	int ret;

	ret = platform_driver_register(&pm8xxx_rgb_led_driver);
	if (ret)
		pr_err("%s: unable to register platform driver\n", __func__);
	return ret;
}

static void __exit pm8xxx_rgb_led_exit(void)
{
	platform_driver_unregister(&pm8xxx_rgb_led_driver);
}

module_init(pm8xxx_rgb_led_init);
module_exit(pm8xxx_rgb_led_exit);

MODULE_AUTHOR("Motorola Mobility");
MODULE_DESCRIPTION("PM8xxx RGB GPIO/PWM LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("platform:pm8xxx-rgb-leds");
