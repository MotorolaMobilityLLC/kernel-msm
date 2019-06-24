/* Copyright (c) 2018-2019, The Linux Foundation. All rights reserved.
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

#include <linux/bitops.h>
#include <linux/device.h>
#include <linux/err.h>
#include <linux/init.h>
#include <linux/kernel.h>
#include <linux/leds.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/nvmem-consumer.h>
#include <linux/of.h>
#include <linux/of_address.h>
#include <linux/platform_device.h>
#include <linux/pwm.h>
#include <linux/regmap.h>
#include <linux/types.h>
#include <linux/slab.h>

#define TRILED_REG_TYPE			0x04
#define TRILED_REG_SUBTYPE		0x05
#define TRILED_REG_EN_CTL		0x46

/* TRILED_REG_EN_CTL */
#define TRILED_EN_CTL_MASK		GENMASK(7, 5)
#define TRILED_EN_CTL_MAX_BIT		7

#define TRILED_TYPE			0x19
#define TRILED_SUBTYPE_LED3H0L12	0x02
#define TRILED_SUBTYPE_LED2H0L12	0x03
#define TRILED_SUBTYPE_LED1H2L12	0x04

#define TRILED_NUM_MAX			3

#define PWM_PERIOD_DEFAULT_NS		1000000
#define RAMP_STEP_DEFAULT_MS		50

struct pwm_setting {
	u64	pre_period_ns;
	u64	period_ns;
	u64	duty_ns;
	u32	ramp_step_ms;
	bool	changed;
	int	max_pattern_length;
	u64	*duty_pattern;
	struct pwm_output_pattern *output_pattern;
};

struct led_setting {
	u64			on_ms;
	u64			off_ms;
	enum led_brightness	brightness;
	bool			blink;
	bool			breath;
	u64			breath_rate_ms;
};

struct qpnp_led_dev {
	struct led_classdev	cdev;
	struct pwm_device	*pwm_dev;
	struct pwm_setting	pwm_setting;
	struct led_setting	led_setting;
	struct qpnp_tri_led_chip	*chip;
	struct mutex		lock;
	const char		*label;
	const char		*default_trigger;
	u8			id;
	bool			blinking;
	bool			breathing;
};

struct qpnp_tri_led_chip {
	struct device		*dev;
	struct regmap		*regmap;
	struct qpnp_led_dev	*leds;
	struct nvmem_device	*pbs_nvmem;
	struct mutex		bus_lock;
	int			num_leds;
	u16			reg_base;
	u8			subtype;
	u8			bitmap;
	u32			*lut_patterns; /* patterns in percentage */
	int			lut_pattern_length;
};

static int qpnp_tri_led_read(struct qpnp_tri_led_chip *chip, u16 addr, u8 *val)
{
	int rc;
	unsigned int tmp;

	mutex_lock(&chip->bus_lock);
	rc = regmap_read(chip->regmap, chip->reg_base + addr, &tmp);
	if (rc < 0)
		dev_err(chip->dev, "Read addr 0x%x failed, rc=%d\n", addr, rc);
	else
		*val = (u8)tmp;
	mutex_unlock(&chip->bus_lock);

	return rc;
}

static int qpnp_tri_led_masked_write(struct qpnp_tri_led_chip *chip,
				u16 addr, u8 mask, u8 val)
{
	int rc;

	mutex_lock(&chip->bus_lock);
	rc = regmap_update_bits(chip->regmap, chip->reg_base + addr, mask, val);
	if (rc < 0)
		dev_err(chip->dev, "Update addr 0x%x to val 0x%x with mask 0x%x failed, rc=%d\n",
					addr, val, mask, rc);
	mutex_unlock(&chip->bus_lock);

	return rc;
}

static void qpnp_tri_led_update_pwm_pattern(struct qpnp_led_dev *led,
					    struct pwm_setting *pwm)
{
	int i;
	u64 percent;
	u64 duty_ns;
	struct pwm_output_pattern *pattern = NULL;

	if (!pwm->duty_pattern || pwm->max_pattern_length <= 0)
		goto updated;

	pattern = kzalloc(sizeof(*pattern), GFP_KERNEL);
	if (!pattern)
		goto updated;

	for (i = 0; i < pwm->max_pattern_length; i++) {
		percent = led->chip->lut_patterns[i];
		duty_ns = percent * pwm->period_ns;
		do_div(duty_ns, 100);
		pwm->duty_pattern[i] = min(duty_ns, pwm->period_ns);
	}

	pattern->duty_pattern = pwm->duty_pattern;
	pattern->num_entries = pwm->max_pattern_length;
	pattern->cycles_per_duty = (u64)pwm->ramp_step_ms * NSEC_PER_MSEC;
	pattern->cycles_per_duty /= pwm->period_ns;

	if (pwm->output_pattern)
		kfree(pwm->output_pattern);
	pwm->output_pattern = pattern;

updated:
	pwm->changed = false;
}

static int __tri_led_config_pwm(struct qpnp_led_dev *led,
				struct pwm_setting *pwm)
{
	struct pwm_state pstate;
	int rc;

	pwm_get_state(led->pwm_dev, &pstate);
	pstate.enabled = !!(pwm->duty_ns != 0);
	pstate.period = pwm->period_ns;
	pstate.duty_cycle = pwm->duty_ns;
	pstate.output_type = led->led_setting.breath ?
		PWM_OUTPUT_MODULATED : PWM_OUTPUT_FIXED;
	if (pwm->changed && led->led_setting.breath)
		qpnp_tri_led_update_pwm_pattern(led, pwm);
	pstate.output_pattern = pwm->output_pattern;
	rc = pwm_apply_state(led->pwm_dev, &pstate);
	if (rc < 0)
		dev_err(led->chip->dev, "Apply PWM state for %s led failed, rc=%d\n",
					led->cdev.name, rc);

	return rc;
}

#define PBS_ENABLE	1
#define PBS_DISABLE	2
#define PBS_ARG		0x42
#define PBS_TRIG_CLR	0xE6
#define PBS_TRIG_SET	0xE5
static int __tri_led_set(struct qpnp_led_dev *led)
{
	int rc = 0;
	u8 val = 0, mask = 0, pbs_val;
	u8 prev_bitmap;

	rc = __tri_led_config_pwm(led, &led->pwm_setting);
	if (rc < 0) {
		dev_err(led->chip->dev, "Configure PWM for %s led failed, rc=%d\n",
					led->cdev.name, rc);
		return rc;
	}

	mask |= 1 << (TRILED_EN_CTL_MAX_BIT - led->id);

	if (led->pwm_setting.duty_ns == 0)
		val = 0;
	else
		val = mask;

	if (led->chip->subtype == TRILED_SUBTYPE_LED2H0L12 &&
		led->chip->pbs_nvmem) {
		/*
		 * Control BOB_CONFIG_EXT_CTRL2_FORCE_EN for HR_LED through
		 * PBS trigger. PBS trigger for enable happens if any one of
		 * LEDs are turned on. PBS trigger for disable happens only
		 * if both LEDs are turned off.
		 */

		prev_bitmap = led->chip->bitmap;
		if (val)
			led->chip->bitmap |= (1 << led->id);
		else
			led->chip->bitmap &= ~(1 << led->id);

		if (!(led->chip->bitmap & prev_bitmap)) {
			pbs_val = led->chip->bitmap ? PBS_ENABLE : PBS_DISABLE;
			rc = nvmem_device_write(led->chip->pbs_nvmem, PBS_ARG,
				1, &pbs_val);
			if (rc < 0) {
				dev_err(led->chip->dev, "Couldn't set PBS_ARG, rc=%d\n",
					rc);
				return rc;
			}

			pbs_val = 1;
			rc = nvmem_device_write(led->chip->pbs_nvmem,
				PBS_TRIG_CLR, 1, &pbs_val);
			if (rc < 0) {
				dev_err(led->chip->dev, "Couldn't set PBS_TRIG_CLR, rc=%d\n",
					rc);
				return rc;
			}

			pbs_val = 1;
			rc = nvmem_device_write(led->chip->pbs_nvmem,
				PBS_TRIG_SET, 1, &pbs_val);
			if (rc < 0) {
				dev_err(led->chip->dev, "Couldn't set PBS_TRIG_SET, rc=%d\n",
					rc);
				return rc;
			}
		}
	}

	rc = qpnp_tri_led_masked_write(led->chip, TRILED_REG_EN_CTL,
							mask, val);
	if (rc < 0)
		dev_err(led->chip->dev, "Update addr 0x%x failed, rc=%d\n",
					TRILED_REG_EN_CTL, rc);

	return rc;
}

static int qpnp_tri_led_set(struct qpnp_led_dev *led)
{
	u64 on_ms, off_ms, period_ns, duty_ns;
	enum led_brightness brightness = led->led_setting.brightness;
	int rc = 0;

	if (led->led_setting.blink) {
		on_ms = led->led_setting.on_ms;
		off_ms = led->led_setting.off_ms;

		duty_ns = on_ms * NSEC_PER_MSEC;
		period_ns = (on_ms + off_ms) * NSEC_PER_MSEC;

		if (period_ns < duty_ns && duty_ns != 0)
			period_ns = duty_ns + 1;
	} else {
		/* Use initial period if no blinking is required */
		period_ns = led->pwm_setting.pre_period_ns;

		if (brightness == LED_OFF)
			duty_ns = 0;

		duty_ns = period_ns * brightness;
		do_div(duty_ns, LED_FULL);

		if (period_ns < duty_ns && duty_ns != 0)
			period_ns = duty_ns + 1;
	}
	dev_dbg(led->chip->dev, "PWM settings for %s led: period = %lluns, duty = %lluns\n",
				led->cdev.name, period_ns, duty_ns);

	led->pwm_setting.duty_ns = duty_ns;
	led->pwm_setting.period_ns = period_ns;

	rc = __tri_led_set(led);
	if (rc < 0) {
		dev_err(led->chip->dev, "__tri_led_set %s failed, rc=%d\n",
				led->cdev.name, rc);
		return rc;
	}

	if (led->led_setting.blink) {
		led->cdev.brightness = LED_FULL;
		led->blinking = true;
		led->breathing = false;
	} else if (led->led_setting.breath) {
		led->cdev.brightness = LED_FULL;
		led->blinking = false;
		led->breathing = true;
	} else {
		led->cdev.brightness = led->led_setting.brightness;
		led->blinking = false;
		led->breathing = false;
	}

	return rc;
}

static int qpnp_tri_led_set_brightness(struct led_classdev *led_cdev,
		enum led_brightness brightness)
{
	struct qpnp_led_dev *led =
		container_of(led_cdev, struct qpnp_led_dev, cdev);
	int rc = 0;

	mutex_lock(&led->lock);
	if (brightness > LED_FULL)
		brightness = LED_FULL;

	if (brightness == led->led_setting.brightness &&
			!led->blinking && !led->breathing) {
		mutex_unlock(&led->lock);
		return 0;
	}

	led->led_setting.brightness = brightness;
	if (!!brightness)
		led->led_setting.off_ms = 0;
	else
		led->led_setting.on_ms = 0;
	led->led_setting.blink = false;
	led->led_setting.breath = false;

	rc = qpnp_tri_led_set(led);
	if (rc)
		dev_err(led->chip->dev, "Set led failed for %s, rc=%d\n",
				led->label, rc);

	mutex_unlock(&led->lock);

	return rc;
}

static enum led_brightness qpnp_tri_led_get_brightness(
			struct led_classdev *led_cdev)
{
	return led_cdev->brightness;
}

static int qpnp_tri_led_set_blink(struct led_classdev *led_cdev,
		unsigned long *on_ms, unsigned long *off_ms)
{
	struct qpnp_led_dev *led =
		container_of(led_cdev, struct qpnp_led_dev, cdev);
	int rc = 0;

	mutex_lock(&led->lock);
	if (led->blinking && *on_ms == led->led_setting.on_ms &&
			*off_ms == led->led_setting.off_ms) {
		dev_dbg(led_cdev->dev, "Ignore, on/off setting is not changed: on %lums, off %lums\n",
						*on_ms, *off_ms);
		mutex_unlock(&led->lock);
		return 0;
	}

	if (*on_ms == 0) {
		led->led_setting.blink = false;
		led->led_setting.breath = false;
		led->led_setting.brightness = LED_OFF;
	} else if (*off_ms == 0) {
		led->led_setting.blink = false;
		led->led_setting.breath = false;
		led->led_setting.brightness = led->cdev.brightness;
	} else {
		led->led_setting.on_ms = *on_ms;
		led->led_setting.off_ms = *off_ms;
		led->led_setting.blink = true;
		led->led_setting.breath = false;
	}

	rc = qpnp_tri_led_set(led);
	if (rc)
		dev_err(led->chip->dev, "Set led failed for %s, rc=%d\n",
				led->label, rc);

	mutex_unlock(&led->lock);
	return rc;
}

static ssize_t breath_rate_ms_show(struct device *dev,
					struct device_attribute *attr,
					char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct qpnp_led_dev *led =
		container_of(led_cdev, struct qpnp_led_dev, cdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", led->led_setting.breath_rate_ms);
}

static ssize_t breath_rate_ms_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int rc;
	u32 breath_rate_ms;
	u32 ramp_step_ms;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct qpnp_led_dev *led =
		container_of(led_cdev, struct qpnp_led_dev, cdev);

	rc = kstrtou32(buf, 10, &breath_rate_ms);
	if (rc < 0)
		return rc;

	cancel_work_sync(&led_cdev->set_brightness_work);

	mutex_lock(&led->lock);
	if (led->led_setting.breath_rate_ms == breath_rate_ms)
		goto unlock;

	led->led_setting.breath_rate_ms = breath_rate_ms;
	ramp_step_ms = led->pwm_setting.ramp_step_ms;
	if (led->pwm_setting.max_pattern_length > 0)
		ramp_step_ms =
			breath_rate_ms / led->pwm_setting.max_pattern_length;

	if (led->pwm_setting.ramp_step_ms == ramp_step_ms)
		goto unlock;

	led->pwm_setting.ramp_step_ms = ramp_step_ms;
	led->pwm_setting.changed = true;
	if (!led->led_setting.breath)
		goto unlock;

	led->led_setting.breath = true;
	led->led_setting.brightness = LED_FULL;
	rc = qpnp_tri_led_set(led);
	if (rc < 0)
		dev_err(led->chip->dev, "Set led failed for %s, rc=%d\n",
				led->label, rc);

unlock:
	mutex_unlock(&led->lock);
	return (rc < 0) ? rc : count;
}
static DEVICE_ATTR(breath_rate_ms, 0644, breath_rate_ms_show,
					breath_rate_ms_store);

static ssize_t breath_show(struct device *dev, struct device_attribute *attr,
							char *buf)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct qpnp_led_dev *led =
		container_of(led_cdev, struct qpnp_led_dev, cdev);

	return snprintf(buf, PAGE_SIZE, "%d\n", led->led_setting.breath);
}

static ssize_t breath_store(struct device *dev, struct device_attribute *attr,
						const char *buf, size_t count)
{
	int rc;
	bool breath;
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct qpnp_led_dev *led =
		container_of(led_cdev, struct qpnp_led_dev, cdev);

	rc = kstrtobool(buf, &breath);
	if (rc < 0)
		return rc;

	cancel_work_sync(&led_cdev->set_brightness_work);

	mutex_lock(&led->lock);
	if (led->breathing == breath)
		goto unlock;

	led->led_setting.blink = false;
	led->led_setting.breath = breath;
	led->led_setting.brightness = breath ? LED_FULL : LED_OFF;
	rc = qpnp_tri_led_set(led);
	if (rc < 0)
		dev_err(led->chip->dev, "Set led failed for %s, rc=%d\n",
				led->label, rc);

unlock:
	mutex_unlock(&led->lock);
	return (rc < 0) ? rc : count;
}

static DEVICE_ATTR(breath, 0644, breath_show, breath_store);
static const struct attribute *breath_attrs[] = {
	&dev_attr_breath.attr,
	&dev_attr_breath_rate_ms.attr,
	NULL
};

static int qpnp_tri_led_register(struct qpnp_tri_led_chip *chip)
{
	struct qpnp_led_dev *led;
	int rc, i, j;

	for (i = 0; i < chip->num_leds; i++) {
		led = &chip->leds[i];
		mutex_init(&led->lock);
		led->cdev.name = led->label;
		led->cdev.max_brightness = LED_FULL;
		led->cdev.brightness_set_blocking = qpnp_tri_led_set_brightness;
		led->cdev.brightness_get = qpnp_tri_led_get_brightness;
		led->cdev.blink_set = qpnp_tri_led_set_blink;
		led->cdev.default_trigger = led->default_trigger;
		led->cdev.brightness = LED_OFF;
		led->cdev.flags |= LED_KEEP_TRIGGER;

		rc = devm_led_classdev_register(chip->dev, &led->cdev);
		if (rc < 0) {
			dev_err(chip->dev, "%s led class device registering failed, rc=%d\n",
							led->label, rc);
			goto err_out;
		}

		if (pwm_get_output_type_supported(led->pwm_dev)
				& PWM_OUTPUT_MODULATED) {
			rc = sysfs_create_files(&led->cdev.dev->kobj,
					breath_attrs);
			if (rc < 0) {
				dev_err(chip->dev, "Create breath file for %s led failed, rc=%d\n",
						led->label, rc);
				goto err_out;
			}
		}
	}

	return 0;

err_out:
	for (j = 0; j <= i; j++) {
		if (j < i)
			sysfs_remove_files(&chip->leds[j].cdev.dev->kobj,
					breath_attrs);
		mutex_destroy(&chip->leds[j].lock);
	}
	return rc;
}

static int qpnp_tri_led_hw_init(struct qpnp_tri_led_chip *chip)
{
	int rc = 0;
	u8 val;

	rc = qpnp_tri_led_read(chip, TRILED_REG_TYPE, &val);
	if (rc < 0) {
		dev_err(chip->dev, "Read REG_TYPE failed, rc=%d\n", rc);
		return rc;
	}

	if (val != TRILED_TYPE) {
		dev_err(chip->dev, "invalid subtype(%d)\n", val);
		return -ENODEV;
	}

	rc = qpnp_tri_led_read(chip, TRILED_REG_SUBTYPE, &val);
	if (rc < 0) {
		dev_err(chip->dev, "Read REG_SUBTYPE failed, rc=%d\n", rc);
		return rc;
	}

	chip->subtype = val;

	return 0;
}

static int qpnp_tri_led_parse_pwm_dt(struct qpnp_led_dev *led, struct device_node *np)
{
	struct of_phandle_args args;
	struct device_node *pwm_dev_node = NULL;
	struct pwm_setting *pwm = &led->pwm_setting;
	struct qpnp_tri_led_chip *chip = led->chip;
	int index;
	int cur_index;
	int rc;
	bool found;
	int low_idx, high_idx;

	pwm->max_pattern_length = 0;
	pwm->ramp_step_ms = RAMP_STEP_DEFAULT_MS;
	rc = of_property_read_u32(np, "ramp-step-ms",
					&pwm->ramp_step_ms);
	if (rc) {
		dev_err(chip->dev, "Get ramp-step-ms failed, rc=%d\n", rc);
	}

	rc = of_parse_phandle_with_args(np, "pwms", "#pwm-cells", 0,
					 &args);
	if (rc) {
		dev_err(chip->dev, "Can't parse \"pwms\" property\n");
		return rc;
	}

	/* Find the pattern lut from pwm dt */
	if (chip->lut_pattern_length <= 0) {
		rc = of_property_count_elems_of_size(args.np,
						"qcom,lut-patterns",
						sizeof(u32));
		if (rc < 0) {
			dev_warn(chip->dev,
				"Read lut-patterns size failed, rc=%d\n", rc);
			rc = 0;
		}
		chip->lut_pattern_length = rc;

		if (chip->lut_pattern_length > 0) {
			chip->lut_patterns = devm_kcalloc(chip->dev,
						chip->lut_pattern_length,
						sizeof(*chip->lut_patterns),
						GFP_KERNEL);
			if (!chip->lut_patterns) {
				chip->lut_pattern_length = 0;
			} else {
				rc = of_property_read_u32_array(
						args.np,
						"qcom,lut-patterns",
						chip->lut_patterns,
						chip->lut_pattern_length);
				if (rc < 0) {
					dev_err(chip->dev,
						"Readout lut-patterns failed,"
						" rc=%d\n", rc);
					devm_kfree(chip->dev,
						chip->lut_patterns);
					chip->lut_patterns = NULL;
					chip->lut_pattern_length = 0;
					rc = 0;
				}
			}
		} else {
			chip->lut_patterns = NULL;
			chip->lut_pattern_length = 0;
		}
	}

	found = false;
	cur_index = 0;
	index = args.args[0];
	for_each_available_child_of_node(args.np, pwm_dev_node) {
		if (index != cur_index)
			cur_index++;
		else {
			found = true;
			break;
		}
	}
	if (!found) {
		dev_err(chip->dev, "Can't find pwm%d entry\n", index);
		rc = -ENODEV;
		goto put;
	}

	rc = of_property_read_u32(pwm_dev_node, "qcom,ramp-low-index",
					&low_idx);
	if (rc < 0) {
		dev_err(chip->dev,
			"Get ramp-low-index failed for pwm%d, rc=%d\n",
			index, rc);
		goto put;
	}

	rc = of_property_read_u32(pwm_dev_node, "qcom,ramp-high-index",
					&high_idx);
	if (rc < 0) {
		dev_err(chip->dev,
			"Get ramp-high-index failed for pwm%d, rc=%d\n",
			index, rc);
		goto put;
	}

	if (low_idx < 0 || low_idx >= high_idx) {
		dev_err(chip->dev,
			"Invalid pwm%d low_idx(%d), high_idx(%d)\n",
			index, low_idx, high_idx);
		rc = -EINVAL;
		goto put;
	}

	pwm->max_pattern_length = high_idx - low_idx + 1;
	pwm->max_pattern_length =
		min(pwm->max_pattern_length, chip->lut_pattern_length);
	dev_info(chip->dev, "Get pwm%d pattern:low_idx(%d),high_idx(%d),"
			"length=%d\n", index, low_idx, high_idx,
			pwm->max_pattern_length);

	pwm->output_pattern = NULL;
	pwm->duty_pattern = NULL;
	if (pwm->max_pattern_length > 0) {
		pwm->duty_pattern = devm_kcalloc(chip->dev,
					pwm->max_pattern_length,
					sizeof(*pwm->duty_pattern),
					GFP_KERNEL);
		if (!pwm->duty_pattern)
			pwm->max_pattern_length = 0;
	}

	led->led_setting.breath_rate_ms =
			pwm->ramp_step_ms * pwm->max_pattern_length;
	pwm->changed = true;

put:
	of_node_put(args.np);

	return rc;
}

static int qpnp_tri_led_parse_dt(struct qpnp_tri_led_chip *chip)
{
	struct device_node *node = chip->dev->of_node, *child_node;
	struct qpnp_led_dev *led;
	struct pwm_args pargs;
	const __be32 *addr;
	int rc = 0, id, i = 0;

	addr = of_get_address(chip->dev->of_node, 0, NULL, NULL);
	if (!addr) {
		dev_err(chip->dev, "Getting address failed\n");
		return -EINVAL;
	}
	chip->reg_base = be32_to_cpu(addr[0]);

	chip->num_leds = of_get_available_child_count(node);
	if (chip->num_leds == 0) {
		dev_err(chip->dev, "No led child node defined\n");
		return -ENODEV;
	}

	if (chip->num_leds > TRILED_NUM_MAX) {
		dev_err(chip->dev, "can't support %d leds(max %d)\n",
				chip->num_leds, TRILED_NUM_MAX);
		return -EINVAL;
	}

	if (of_find_property(chip->dev->of_node, "nvmem", NULL)) {
		chip->pbs_nvmem = devm_nvmem_device_get(chip->dev, "pbs_sdam");
		if (IS_ERR_OR_NULL(chip->pbs_nvmem)) {
			rc = PTR_ERR(chip->pbs_nvmem);
			if (rc != -EPROBE_DEFER) {
				dev_err(chip->dev, "Couldn't get nvmem device, rc=%d\n",
					rc);
				return -ENODEV;
			}
			chip->pbs_nvmem = NULL;
			return rc;
		}
	}

	rc = of_property_count_elems_of_size(chip->dev->of_node,
			"qcom,lut-patterns", sizeof(u32));
	if (rc < 0) {
		dev_warn(chip->dev, "Read lut-patterns failed, rc=%d\n", rc);
		rc = 0;
	}
	chip->lut_pattern_length = rc;

	if (chip->lut_pattern_length > 0) {
		chip->lut_patterns = devm_kcalloc(chip->dev,
						chip->lut_pattern_length,
						sizeof(*chip->lut_patterns),
						GFP_KERNEL);
		if (!chip->lut_patterns)
			chip->lut_pattern_length = 0;
		else {
			rc = of_property_read_u32_array(chip->dev->of_node,
						"qcom,lut-patterns",
						chip->lut_patterns,
						chip->lut_pattern_length);
			if (rc < 0) {
				rc = 0;
				devm_kfree(chip->dev, chip->lut_patterns);
				chip->lut_patterns = NULL;
				chip->lut_pattern_length = 0;
			}
		}
	} else {
		chip->lut_patterns = NULL;
		chip->lut_pattern_length = 0;
	}

	chip->leds = devm_kcalloc(chip->dev, chip->num_leds,
			sizeof(struct qpnp_led_dev), GFP_KERNEL);
	if (!chip->leds)
		return -ENOMEM;

	for_each_available_child_of_node(node, child_node) {
		rc = of_property_read_u32(child_node, "led-sources", &id);
		if (rc) {
			dev_err(chip->dev, "Get led-sources failed, rc=%d\n",
							rc);
			return rc;
		}

		if (id >= TRILED_NUM_MAX) {
			dev_err(chip->dev, "only support 0~%d current source\n",
					TRILED_NUM_MAX - 1);
			return -EINVAL;
		}

		led = &chip->leds[i++];
		led->chip = chip;
		led->id = id;
		led->label =
			of_get_property(child_node, "label", NULL) ? :
							child_node->name;

		led->pwm_dev =
			devm_of_pwm_get(chip->dev, child_node, NULL);
		if (IS_ERR(led->pwm_dev)) {
			rc = PTR_ERR(led->pwm_dev);
			if (rc != -EPROBE_DEFER)
				dev_err(chip->dev, "Get pwm device for %s led failed, rc=%d\n",
							led->label, rc);
			return rc;
		}

		pwm_get_args(led->pwm_dev, &pargs);
		if (pargs.period == 0)
			led->pwm_setting.pre_period_ns = PWM_PERIOD_DEFAULT_NS;
		else
			led->pwm_setting.pre_period_ns = pargs.period;

		led->default_trigger = of_get_property(child_node,
				"linux,default-trigger", NULL);

		qpnp_tri_led_parse_pwm_dt(led, child_node);
	}

	return rc;
}

static int qpnp_tri_led_probe(struct platform_device *pdev)
{
	struct qpnp_tri_led_chip *chip;
	int rc = 0;

	chip = devm_kzalloc(&pdev->dev, sizeof(*chip), GFP_KERNEL);
	if (!chip)
		return -ENOMEM;

	chip->dev = &pdev->dev;
	chip->regmap = dev_get_regmap(chip->dev->parent, NULL);
	if (!chip->regmap) {
		dev_err(chip->dev, "Getting regmap failed\n");
		return -EINVAL;
	}

	rc = qpnp_tri_led_parse_dt(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Devicetree properties parsing failed, rc=%d\n",
								rc);
		return rc;
	}

	mutex_init(&chip->bus_lock);

	rc = qpnp_tri_led_hw_init(chip);
	if (rc) {
		dev_err(chip->dev, "HW initialization failed, rc=%d\n", rc);
		goto destroy;
	}

	dev_set_drvdata(chip->dev, chip);
	rc = qpnp_tri_led_register(chip);
	if (rc < 0) {
		dev_err(chip->dev, "Registering LED class devices failed, rc=%d\n",
								rc);
		goto destroy;
	}

	dev_dbg(chip->dev, "Tri-led module with subtype 0x%x is detected\n",
					chip->subtype);
	return 0;
destroy:
	mutex_destroy(&chip->bus_lock);
	dev_set_drvdata(chip->dev, NULL);

	return rc;
}

static int qpnp_tri_led_remove(struct platform_device *pdev)
{
	int i;
	struct qpnp_tri_led_chip *chip = dev_get_drvdata(&pdev->dev);

	mutex_destroy(&chip->bus_lock);
	for (i = 0; i < chip->num_leds; i++) {
		sysfs_remove_files(&chip->leds[i].cdev.dev->kobj, breath_attrs);
		mutex_destroy(&chip->leds[i].lock);
	}
	dev_set_drvdata(chip->dev, NULL);
	return 0;
}

static const struct of_device_id qpnp_tri_led_of_match[] = {
	{ .compatible = "qcom,tri-led",},
	{ },
};

static struct platform_driver qpnp_tri_led_driver = {
	.driver		= {
		.name		= "qcom,tri-led",
		.of_match_table	= qpnp_tri_led_of_match,
	},
	.probe		= qpnp_tri_led_probe,
	.remove		= qpnp_tri_led_remove,
};
module_platform_driver(qpnp_tri_led_driver);

MODULE_DESCRIPTION("QTI TRI_LED driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:qpnp-tri-led");
