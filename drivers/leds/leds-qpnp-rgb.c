/* Copyright (c) 2012-2013, The Linux Foundation. All rights reserved.
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

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/leds.h>
#include <linux/err.h>
#include <linux/spinlock.h>
#include <linux/of_platform.h>
#include <linux/of_device.h>
#include <linux/spmi.h>
#include <linux/qpnp/pwm.h>
#include <linux/workqueue.h>

#define LED_TRIGGER_DEFAULT		"none"

#define RGB_LED_SRC_SEL(base)		(base + 0x45)
#define RGB_LED_EN_CTL(base)		(base + 0x46)
#define RGB_LED_ATC_CTL(base)		(base + 0x47)
#define LUT_RAMP_CONTROL		0xB0C8
#define LUT_RAMP_SID			1

#define RGB_MAX_LEVEL			LED_FULL
#define RGB_LED_ENABLE_RED		0x80
#define RGB_LED_ENABLE_GREEN		0x40
#define RGB_LED_ENABLE_BLUE		0x20
#define RGB_LED_SOURCE_VPH_PWR		0x01
#define RGB_LED_ENABLE_MASK		0xE0
#define RGB_LED_SRC_MASK		0x03
#define QPNP_LED_PWM_FLAGS	(PM_PWM_LUT_LOOP | PM_PWM_LUT_RAMP_UP \
				 | PM_PWM_LUT_PAUSE_HI_EN)
#define QPNP_LUT_RAMP_STEP_DEFAULT	255
#define	PWM_LUT_MAX_SIZE		63
#define RGB_LED_DISABLE			0x00

#define MPP_MAX_LEVEL			LED_FULL
#define LED_MPP_MODE_CTRL(base)		(base + 0x40)
#define LED_MPP_VIN_CTRL(base)		(base + 0x41)
#define LED_MPP_EN_CTRL(base)		(base + 0x46)
#define LED_MPP_SINK_CTRL(base)		(base + 0x4C)

#define LED_MPP_CURRENT_DEFAULT		5
#define LED_MPP_CURRENT_PER_SETTING	5
#define LED_MPP_SOURCE_SEL_DEFAULT	LED_MPP_MODE_ENABLE

#define LED_MPP_SINK_MASK		0x07
#define LED_MPP_MODE_MASK		0x7F
#define LED_MPP_EN_MASK			0x80
#define LED_MPP_SRC_MASK		0x0F
#define LED_MPP_MODE_CTRL_MASK		0x70

#define LED_MPP_MODE_SINK		(0x06 << 4)
#define LED_MPP_MODE_ENABLE		0x01
#define LED_MPP_MODE_OUTPUT		0x10
#define LED_MPP_MODE_DISABLE		0x00
#define LED_MPP_EN_ENABLE		0x80
#define LED_MPP_EN_DISABLE		0x00

#define MPP_SOURCE_DTEST1		0x08

/**
 * enum qpnp_leds - QPNP supported led ids
 * @QPNP_ID_WLED - White led backlight
 */
enum qpnp_leds {
	QPNP_ID_WLED = 0,
	QPNP_ID_FLASH1_LED0,
	QPNP_ID_FLASH1_LED1,
	QPNP_ID_RGB_RED,
	QPNP_ID_RGB_GREEN,
	QPNP_ID_RGB_BLUE,
	QPNP_ID_LED_MPP,
	QPNP_ID_KPDBL,
	QPNP_ID_ATC,
	QPNP_ID_COMBO,
	QPNP_ID_MAX,
};

enum led_mode {
	PWM_MODE = 0,
	LPG_MODE,
	MANUAL_MODE,
};

static u8 rgb_pwm_debug_regs[] = {
	0x45, 0x46, 0x47,
};

static u8 mpp_debug_regs[] = {
	0x40, 0x41, 0x42, 0x45, 0x46, 0x4c,
};

/**
 *  pwm_config_data - pwm configuration data
 *  @lut_params - lut parameters to be used by pwm driver
 *  @pwm_device - pwm device
 *  @pwm_channel - pwm channel to be configured for led
 *  @pwm_period_us - period for pwm, in us
 *  @mode - mode the led operates in
 */
struct pwm_config_data {
	struct lut_params	lut_params;
	struct pwm_device	*pwm_dev;
	int			pwm_channel;
	u32			pwm_period_us;
	struct pwm_duty_cycles	*duty_cycles;
	u8	mode;
	u8	enable;
	u8	blinking;
};

/**
 *  mpp_config_data - mpp configuration data
 *  @pwm_cfg - device pwm configuration
 *  @current_setting - current setting, 5ma-40ma in 5ma increments
 *  @source_sel - source selection
 *  @mode_ctrl - mode control
 *  @pwm_mode - pwm mode in use
 */
struct mpp_config_data {
	struct pwm_config_data	*pwm_cfg;
	u8	current_setting;
	u8	source_sel;
	u8	mode_ctrl;
	u8 pwm_mode;
};

/**
 *  rgb_config_data - rgb configuration data
 *  @pwm_cfg - device pwm configuration
 *  @enable - bits to enable led
 */
struct rgb_config_data {
	struct pwm_config_data	*pwm_cfg;
	u8	enable;
};

/**
 * struct qpnp_led_data - internal led data structure
 * @led_classdev - led class device
 * @delayed_work - delayed work for turning off the LED
 * @id - led index
 * @base_reg - base register given in device tree
 * @lock - to protect the transactions
 * @reg - cached value of led register
 * @num_leds - number of leds in the module
 * @max_current - maximum current supported by LED
 * @default_on - true: default state max, false, default state 0
 * @turn_off_delay_ms - number of msec before turning off the LED
 */
struct qpnp_led_data {
	struct led_classdev	cdev;
	struct spmi_device	*spmi_dev;
	struct delayed_work	dwork;
	int			id;
	u16			base;
	u8			reg;
	u8			num_leds;
	spinlock_t		lock;
	struct wled_config_data *wled_cfg;
	struct flash_config_data	*flash_cfg;
	struct kpdbl_config_data	*kpdbl_cfg;
	struct rgb_config_data	*rgb_cfg;
	struct mpp_config_data	*mpp_cfg;
	int			max_current;
	bool			default_on;
	int			turn_off_delay_ms;
};

#define BLINK_LEDS_ARRAY_SIZE 3

#define WHITE_LED_MASK	0xFF0000
#define RED_LED_MASK	0xFF0000
#define GREEN_LED_MASK	0x00FF00
#define BLUE_LED_MASK	0x0000FF

/**
 *  blink_led_data - blink configuration data
 */

struct blink_led_data {
	struct qpnp_led_data  *led;
	unsigned	mask;
};


static int qpnp_mpp_set(struct qpnp_led_data *);
static int qpnp_rgb_set(struct qpnp_led_data *);
static int qpnp_led_masked_write(struct qpnp_led_data *, u16, u8, u8);

static int flags = QPNP_LED_PWM_FLAGS;
static int duty_ms = 14;
static int period;
static int pause_hi;
static int start_idx;
static int duty_pcts_ramp[PWM_LUT_MAX_SIZE] = {0};
static int num_idx = ARRAY_SIZE(duty_pcts_ramp);
static int blinking_leds;
static int ramp_up, ramp_down;
static int delay_on, delay_off;
static unsigned rgb;
static struct blink_led_data blink_array[BLINK_LEDS_ARRAY_SIZE];
static int blink_cnt;

module_param(flags, int, 0644);
module_param(duty_ms, int, 0644);
module_param(period, int, 0644);
module_param(pause_hi, int, 0644);
module_param(num_idx, int, 0644);
module_param(start_idx, int, 0644);
module_param(blinking_leds, int, 0644);
module_param(blink_cnt, int, 0644);
module_param_array_named(pcts, duty_pcts_ramp, uint, NULL, 0644);

static void qpnp_blink_enable_leds(void)
{
	int i, ret;
	struct pwm_config_data *pwm;
	struct qpnp_led_data *led;
	u8 lpg_map = 0;
	struct spmi_device *spmi_dev = 0;

	for (i = 0; i < blink_cnt; i++) {
		led = blink_array[i].led;
		if (!led->cdev.brightness)
			continue;
		pwm = (led->id == QPNP_ID_LED_MPP) ?
			led->mpp_cfg->pwm_cfg : led->rgb_cfg->pwm_cfg;
		ret = pwm_enable_lut_no_ramp(pwm->pwm_dev);
		if (ret < 0)
			dev_err(&led->spmi_dev->dev,
				"%s lut config err (%d)\n", __func__, ret);
		if (led->id == QPNP_ID_LED_MPP)
			ret = qpnp_led_masked_write(led,
				LED_MPP_EN_CTRL(led->base), LED_MPP_EN_MASK,
				LED_MPP_EN_ENABLE);
		else
			ret = qpnp_led_masked_write(led,
				RGB_LED_EN_CTL(led->base),
				led->rgb_cfg->enable, led->rgb_cfg->enable);
		if (ret)
			dev_err(&led->spmi_dev->dev,
				"%s Enable led error (%d)\n", __func__, ret);
		spmi_dev = led->spmi_dev;
		pwm->blinking = 1;
		lpg_map |= 1 << pwm->pwm_channel;
		blinking_leds++;
	}

	if (lpg_map) {
		/* Start all ramps with one write command */
		ret = spmi_ext_register_writel(spmi_dev->ctrl, LUT_RAMP_SID,
			LUT_RAMP_CONTROL, &lpg_map, 1);
		if (ret)
			dev_err(&spmi_dev->dev,
				"Unable set lut enable rc(%d)\n", ret);
	}
}

static void qpnp_blink_config_luts(void)
{
	int i, ret;
	struct pwm_config_data *pwm;
	struct qpnp_led_data *led;

	for (i = 0; i < blink_cnt; i++) {
		led = blink_array[i].led;
		if (!led->cdev.brightness)
			continue;
		pwm = (led->id == QPNP_ID_LED_MPP) ?
			led->mpp_cfg->pwm_cfg : led->rgb_cfg->pwm_cfg;
		pwm_disable(pwm->pwm_dev);
		pwm->lut_params.start_idx = start_idx;
		pwm->duty_cycles->start_idx = start_idx;
		pwm->lut_params.idx_len = num_idx;
		pwm->lut_params.lut_pause_hi = pause_hi;
		pwm->lut_params.lut_pause_lo = 0;
		pwm->lut_params.ramp_step_ms = duty_ms;
		pwm->lut_params.flags = flags;
		pwm->duty_cycles->duty_pcts = duty_pcts_ramp;

		ret = pwm_lut_config(pwm->pwm_dev,
			period, /* ignored by hardware */
			pwm->duty_cycles->duty_pcts,
			pwm->lut_params);
		if (ret)
			dev_err(&led->spmi_dev->dev,
				"%s LUT config error (%d)\n", __func__, ret);
	}
}

static int qpnp_blink_set(unsigned on)
{
	/* num_up and num_down are percentages of times for ramping */
	static int num_up = ((PWM_LUT_MAX_SIZE-1) * 1)/2;
	int num_down;
	/* num_top is percentage of time to stay up solid */
	int num_top;
	int i;
	int ret = 0;
	int init_max, max = 100;
	int max_level;
	int step;
	int (*array_ptr)[];
	struct pwm_config_data *pwm;
	struct qpnp_led_data *led;

	if (!on) {
		pr_debug("%s: Turning blinking off\n", __func__);
		for (i = 0; i < blink_cnt; i++) {
			led = blink_array[i].led;
			pwm = (led->id == QPNP_ID_LED_MPP) ?
				led->mpp_cfg->pwm_cfg : led->rgb_cfg->pwm_cfg;
			ret = pwm_change_mode(pwm->pwm_dev, PWM_MODE);
			if (ret < 0)
				dev_err(&led->spmi_dev->dev,
					"Change to PWM err (%d)\n", ret);
			pwm->blinking = 0;
			blinking_leds--;
			if (led->id == QPNP_ID_LED_MPP)
				ret = qpnp_mpp_set(led);
			else
				ret = qpnp_rgb_set(led);

			if (ret < 0)
				dev_err(&led->spmi_dev->dev,
					"MPP/RGB set failed (%d)\n", ret);
		}
		return ret;
	}
	if (!ramp_up && !ramp_down) {
		pr_debug("%s: No ramping\n", __func__);
		/* We need to configure all PWMs first and then enable
		them first so that it's as synchronous as possible.
		This is a non breathing case of blinking */

		for (i = 0; i < blink_cnt; i++) {
			led = blink_array[i].led;
			if (!led->cdev.brightness)
				continue;
			pwm = (led->id == QPNP_ID_LED_MPP) ?
				led->mpp_cfg->pwm_cfg : led->rgb_cfg->pwm_cfg;
			pwm_disable(pwm->pwm_dev);
			ret = pwm_change_mode(pwm->pwm_dev, PWM_MODE);
			if (ret < 0)
				dev_err(&led->spmi_dev->dev,
					"Change to PWM err (%d)\n", ret);
			ret = pwm_config(pwm->pwm_dev,
				delay_on * 1000 * NSEC_PER_USEC,
				(delay_on + delay_off) * 1000 * NSEC_PER_USEC);
			if (ret) {
				dev_err(&led->spmi_dev->dev,
					"PWM config error (%d)\n", ret);
				return ret;
			}
		}
		qpnp_blink_enable_leds();
	} else {
		pr_debug("%s: Ramping\n", __func__);
		num_down = num_up;
		num_top = PWM_LUT_MAX_SIZE - num_up - num_down - 1;
		/* Last index is 0 for delay off */
		array_ptr = &duty_pcts_ramp;
		duty_pcts_ramp[PWM_LUT_MAX_SIZE-1] = 0;
		num_idx = ARRAY_SIZE(duty_pcts_ramp);
		duty_ms = delay_on/(PWM_LUT_MAX_SIZE-1);
		if (delay_on - duty_ms * (PWM_LUT_MAX_SIZE-1) >
			(duty_ms+1)*(PWM_LUT_MAX_SIZE-1) - delay_on)
			duty_ms++;
		pause_hi = delay_off - duty_ms;
		period = duty_ms * PWM_LUT_MAX_SIZE + pause_hi;

		/* Equalize all LEDs' levels to max */
		max_level = blink_array[0].led->cdev.brightness;
		for (i = 1; i < blink_cnt; i++) {
			led = blink_array[i].led;
			if (led->cdev.brightness > max_level)
				max_level = led->cdev.brightness;
		}
		init_max = 100 * max_level/LED_FULL;
		/* Ramp up */
		max = init_max;
		step = max/(num_up - 1);
		if (ramp_up) {
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
		if (ramp_down) {
			for (i = num_down+num_top; i <= PWM_LUT_MAX_SIZE-2;
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
			for (i = num_down+num_top; i <= PWM_LUT_MAX_SIZE-2; i++)
				duty_pcts_ramp[i] = max;
		}
		/* We need to configure all LUT in PWMs first and then enable
		them first so that it's as synchronous as possible */
		qpnp_blink_config_luts();
		qpnp_blink_enable_leds();
	}

	return ret;
}

static ssize_t combo_show_control(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{

	return scnprintf(buf, PAGE_SIZE,
		"RGB=%#x, on/off=%d/%d ms, ramp=%d/%d\n",
		rgb, delay_on, delay_off,
		ramp_up, ramp_down);
}

static ssize_t combo_store_control(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led_cdev = dev_get_drvdata(dev);
	struct qpnp_led_data *led =
		container_of(led_cdev, struct qpnp_led_data, cdev);
	int i;

	if (len == 0)
		return 0;

	spin_lock(&led->lock);
	/* Clear pulse parameters in case of incomplete input */
	rgb = 0;
	delay_on = 0;
	delay_off = 0;
	ramp_up = 0;
	ramp_down = 0;

	sscanf(buf, "%x %u %u %u %u", &rgb, &delay_on, &delay_off,
		&ramp_up, &ramp_down);
	pr_debug("0x%X, on/off=%u/%u ms, ramp=%u%%/%u%%\n",
		rgb, delay_on, delay_off, ramp_up, ramp_down);

	/* Set to always ramping until non ramping mode is fixed */

	ramp_up = 1;
	ramp_down = 1;

	/* LED that is off is not blinking */
	if (rgb == 0) {
		delay_on = 0;
		delay_off = 0;
	}

	/* If delay_on is not 0 but delay_off is 0 we won't blink */
	if (delay_off == 0)
		delay_on = 0;

	for (i = 0; i < blink_cnt; i++) {
		blink_array[i].led->cdev.brightness =
			(rgb & blink_array[i].mask) >> (16 - i*8);
	}

	/* If we are already blinking - reset the pwm */
	if (delay_on && delay_off && blinking_leds)
		qpnp_blink_set(0);
	qpnp_blink_set(delay_on && delay_off);

	spin_unlock(&led->lock);

	return len;
}

/* LED class device attributes */
static DEVICE_ATTR(control, S_IRUGO | S_IWUSR,
	combo_show_control, combo_store_control);

static struct attribute *combo_led_attributes[] = {
	&dev_attr_control.attr,
	NULL,
};

static struct attribute_group combo_attribute_group = {
	.attrs = combo_led_attributes
};

static int
qpnp_led_masked_write(struct qpnp_led_data *led, u16 addr, u8 mask, u8 val)
{
	int rc;
	u8 reg;

	rc = spmi_ext_register_readl(led->spmi_dev->ctrl, led->spmi_dev->sid,
		addr, &reg, 1);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
			"Unable to read from addr=%x, rc(%d)\n", addr, rc);
	}

	reg &= ~mask;
	reg |= val;

	rc = spmi_ext_register_writel(led->spmi_dev->ctrl, led->spmi_dev->sid,
		addr, &reg, 1);
	if (rc)
		dev_err(&led->spmi_dev->dev,
			"Unable to write to addr=%x, rc(%d)\n", addr, rc);
	return rc;
}

static void qpnp_dump_regs(struct qpnp_led_data *led, u8 regs[], u8 array_size)
{
	int i;
	u8 val;

	pr_debug("===== %s LED register dump start =====\n", led->cdev.name);
	for (i = 0; i < array_size; i++) {
		spmi_ext_register_readl(led->spmi_dev->ctrl,
					led->spmi_dev->sid,
					led->base + regs[i],
					&val, sizeof(val));
		pr_debug("%s: 0x%x = 0x%x\n", led->cdev.name,
					led->base + regs[i], val);
	}
	pr_debug("===== %s LED register dump end =====\n", led->cdev.name);
}

static int qpnp_mpp_set(struct qpnp_led_data *led)
{
	int rc;
	int duty_us;

	if (led->cdev.brightness) {

		if (led->mpp_cfg->pwm_mode == PWM_MODE) {
			pwm_disable(led->mpp_cfg->pwm_cfg->pwm_dev);
			duty_us = (led->mpp_cfg->pwm_cfg->pwm_period_us *
					led->cdev.brightness) / LED_FULL;
			/*config pwm for brightness scaling*/
			pr_debug("%s duty cycle %dus period %dus\n", __func__,
				duty_us,
				led->mpp_cfg->pwm_cfg->pwm_period_us);
			rc = pwm_config(led->mpp_cfg->pwm_cfg->pwm_dev,
					duty_us * NSEC_PER_USEC,
					led->mpp_cfg->pwm_cfg->pwm_period_us *
					NSEC_PER_USEC);
			if (rc < 0) {
				dev_err(&led->spmi_dev->dev, "Failed to " \
					"configure pwm for new values\n");
				return rc;
			}
		}

		if (led->mpp_cfg->pwm_mode != MANUAL_MODE)
			pwm_enable(led->mpp_cfg->pwm_cfg->pwm_dev);


		rc = qpnp_led_masked_write(led,
				LED_MPP_EN_CTRL(led->base), LED_MPP_EN_MASK,
				LED_MPP_EN_ENABLE);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
					"Failed to write led enable " \
					"reg\n");
			return rc;
		}
	} else {

		if (led->mpp_cfg->pwm_mode != MANUAL_MODE) {
			pwm_disable(led->mpp_cfg->pwm_cfg->pwm_dev);
			pwm_change_mode(led->mpp_cfg->pwm_cfg->pwm_dev,
				PWM_MODE);
		}

		rc = qpnp_led_masked_write(led,
					LED_MPP_EN_CTRL(led->base),
					LED_MPP_EN_MASK,
					LED_MPP_EN_DISABLE);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
					"Failed to write led enable reg\n");
			return rc;
		}
	}

	qpnp_dump_regs(led, mpp_debug_regs, ARRAY_SIZE(mpp_debug_regs));

	return 0;
}

static int qpnp_rgb_set(struct qpnp_led_data *led)
{
	int duty_us;
	int rc;

	if (led->cdev.brightness) {
		if (led->rgb_cfg->pwm_cfg->mode == PWM_MODE) {
			duty_us = (led->rgb_cfg->pwm_cfg->pwm_period_us *
				led->cdev.brightness) / LED_FULL;
			rc = pwm_config(led->rgb_cfg->pwm_cfg->pwm_dev,
					duty_us * NSEC_PER_USEC,
					led->rgb_cfg->pwm_cfg->pwm_period_us *
						NSEC_PER_USEC);
			if (rc < 0) {
				dev_err(&led->spmi_dev->dev,
					"pwm config failed\n");
				return rc;
			}
		}
		rc = qpnp_led_masked_write(led,
			RGB_LED_EN_CTL(led->base),
			led->rgb_cfg->enable, led->rgb_cfg->enable);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Failed to write led enable reg\n");
			return rc;
		}

		rc = pwm_enable(led->rgb_cfg->pwm_cfg->pwm_dev);
		if (rc < 0) {
			dev_err(&led->spmi_dev->dev, "pwm enable failed\n");
			return rc;
		}
	} else {
		pwm_disable(led->rgb_cfg->pwm_cfg->pwm_dev);
		pwm_change_mode(led->rgb_cfg->pwm_cfg->pwm_dev,
			PWM_MODE);

		rc = qpnp_led_masked_write(led,
			RGB_LED_EN_CTL(led->base),
			led->rgb_cfg->enable, RGB_LED_DISABLE);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Failed to write led enable reg\n");
			return rc;
		}
	}

	qpnp_dump_regs(led, rgb_pwm_debug_regs, ARRAY_SIZE(rgb_pwm_debug_regs));

	return 0;
}

static void qpnp_led_set(struct led_classdev *led_cdev,
				enum led_brightness value)
{
	int rc;
	struct qpnp_led_data *led;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);
	if (value < LED_OFF || value > led->cdev.max_brightness) {
		dev_err(&led->spmi_dev->dev, "Invalid brightness value\n");
		return;
	}

	spin_lock(&led->lock);
	led->cdev.brightness = value;

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		rc = qpnp_rgb_set(led);
		if (rc < 0)
			dev_err(&led->spmi_dev->dev,
				"RGB set brightness failed (%d)\n", rc);
		break;
	case QPNP_ID_LED_MPP:
		rc = qpnp_mpp_set(led);
		if (rc < 0)
			dev_err(&led->spmi_dev->dev,
					"MPP set brightness failed (%d)\n", rc);
		break;
	case QPNP_ID_COMBO:
		break;
	default:
		dev_err(&led->spmi_dev->dev, "%s Invalid LED(%d)\n",
			__func__, led->id);
		break;
	}
	spin_unlock(&led->lock);
}

static int __devinit qpnp_led_set_max_brightness(struct qpnp_led_data *led)
{
	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		led->cdev.max_brightness = RGB_MAX_LEVEL;
		break;
	case QPNP_ID_LED_MPP:
		led->cdev.max_brightness = MPP_MAX_LEVEL;
		break;
	case QPNP_ID_COMBO:
		break;
	default:
		dev_err(&led->spmi_dev->dev, "%s Invalid LED(%d)\n",
			__func__, led->id);
		return -EINVAL;
	}

	return 0;
}

static enum led_brightness qpnp_led_get(struct led_classdev *led_cdev)
{
	struct qpnp_led_data *led;

	led = container_of(led_cdev, struct qpnp_led_data, cdev);

	return led->cdev.brightness;
}

static void qpnp_led_turn_off_delayed(struct work_struct *work)
{
	struct delayed_work *dwork = to_delayed_work(work);
	struct qpnp_led_data *led
		= container_of(dwork, struct qpnp_led_data, dwork);

	led->cdev.brightness = LED_OFF;
	qpnp_led_set(&led->cdev, led->cdev.brightness);
}

static void qpnp_led_turn_off(struct qpnp_led_data *led)
{
	INIT_DELAYED_WORK(&led->dwork, qpnp_led_turn_off_delayed);
	schedule_delayed_work(&led->dwork,
		msecs_to_jiffies(led->turn_off_delay_ms));
}

static int __devinit qpnp_pwm_init(struct pwm_config_data *pwm_cfg,
					struct spmi_device *spmi_dev,
					const char *name)
{
	int rc, start_idx, idx_len;

	if (pwm_cfg->pwm_channel != -1) {
		pwm_cfg->pwm_dev =
			pwm_request(pwm_cfg->pwm_channel, name);

		if (IS_ERR_OR_NULL(pwm_cfg->pwm_dev)) {
			dev_err(&spmi_dev->dev,
				"could not acquire PWM Channel %d, " \
				"error %ld\n",
				pwm_cfg->pwm_channel,
				PTR_ERR(pwm_cfg->pwm_dev));
			pwm_cfg->pwm_dev = NULL;
			return -ENODEV;
		}

		if (pwm_cfg->mode == LPG_MODE) {
			start_idx =
			pwm_cfg->duty_cycles->start_idx;
			idx_len =
			pwm_cfg->duty_cycles->num_duty_pcts;

			if (idx_len >= PWM_LUT_MAX_SIZE &&
					start_idx) {
				dev_err(&spmi_dev->dev,
					"Wrong LUT size or index\n");
				return -EINVAL;
			}
			if ((start_idx + idx_len) >
					PWM_LUT_MAX_SIZE) {
				dev_err(&spmi_dev->dev,
					"Exceed LUT limit\n");
				return -EINVAL;
			}
			rc = pwm_lut_config(pwm_cfg->pwm_dev,
				PM_PWM_PERIOD_MIN, /* ignored by hardware */
				pwm_cfg->duty_cycles->duty_pcts,
				pwm_cfg->lut_params);
			if (rc < 0) {
				dev_err(&spmi_dev->dev, "Failed to " \
					"configure pwm LUT\n");
				return rc;
			}
		}
	} else {
		dev_err(&spmi_dev->dev,
			"Invalid PWM channel\n");
		return -EINVAL;
	}

	return 0;
}

static int __devinit qpnp_rgb_init(struct qpnp_led_data *led)
{
	int rc;

	rc = qpnp_led_masked_write(led, RGB_LED_SRC_SEL(led->base),
		RGB_LED_SRC_MASK, RGB_LED_SOURCE_VPH_PWR);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
			"Failed to write led source select register\n");
		return rc;
	}

	rc = qpnp_pwm_init(led->rgb_cfg->pwm_cfg, led->spmi_dev,
				led->cdev.name);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
			"Failed to initialize pwm\n");
		return rc;
	}
	/* Initialize led for use in auto trickle charging mode */
	rc = qpnp_led_masked_write(led, RGB_LED_ATC_CTL(led->base),
		led->rgb_cfg->enable, led->rgb_cfg->enable);

	return 0;
}

static int __devinit qpnp_mpp_init(struct qpnp_led_data *led)
{
	int rc, val;

	val = (led->mpp_cfg->current_setting / LED_MPP_CURRENT_PER_SETTING) - 1;

	if (val < 0)
		val = 0;

	rc = qpnp_led_masked_write(led, LED_MPP_SINK_CTRL(led->base),
		LED_MPP_SINK_MASK, val);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
			"Failed to write led enable reg\n");
		return rc;
	}

	if (led->mpp_cfg->pwm_mode != MANUAL_MODE) {
		rc = qpnp_pwm_init(led->mpp_cfg->pwm_cfg, led->spmi_dev,
					led->cdev.name);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Failed to initialize pwm\n");
			return rc;
		}
	}

	val = (led->mpp_cfg->source_sel & LED_MPP_SRC_MASK) |
		(led->mpp_cfg->mode_ctrl & LED_MPP_MODE_CTRL_MASK);

	rc = qpnp_led_masked_write(led,
		LED_MPP_MODE_CTRL(led->base), LED_MPP_MODE_MASK,
		val);
	if (rc) {
		dev_err(&led->spmi_dev->dev,
				"Failed to write led mode reg\n");
		return rc;
	}

	return 0;
}

static int __devinit qpnp_led_initialize(struct qpnp_led_data *led)
{
	int rc = 0;

	switch (led->id) {
	case QPNP_ID_RGB_RED:
	case QPNP_ID_RGB_GREEN:
	case QPNP_ID_RGB_BLUE:
		rc = qpnp_rgb_init(led);
		if (rc)
			dev_err(&led->spmi_dev->dev,
				"RGB initialize failed(%d)\n", rc);
		break;
	case QPNP_ID_LED_MPP:
		rc = qpnp_mpp_init(led);
		if (rc)
			dev_err(&led->spmi_dev->dev,
				"MPP initialize failed(%d)\n", rc);
		break;
	case QPNP_ID_COMBO:
		break;
	default:
		dev_err(&led->spmi_dev->dev, "%s Invalid LED(%d)\n",
			__func__, led->id);
		return -EINVAL;
	}

	return rc;
}

static int __devinit qpnp_get_common_configs(struct qpnp_led_data *led,
				struct device_node *node)
{
	int rc;
	u32 val;
	const char *temp_string;

	led->cdev.default_trigger = LED_TRIGGER_DEFAULT;
	rc = of_property_read_string(node, "linux,default-trigger",
		&temp_string);
	if (!rc)
		led->cdev.default_trigger = temp_string;
	else if (rc != -EINVAL)
		return rc;

	led->default_on = false;
	rc = of_property_read_string(node, "qcom,default-state",
		&temp_string);
	if (!rc) {
		if (strncmp(temp_string, "on", sizeof("on")) == 0)
			led->default_on = true;
	} else if (rc != -EINVAL)
		return rc;

	led->turn_off_delay_ms = 0;
	rc = of_property_read_u32(node, "qcom,turn-off-delay-ms", &val);
	if (!rc)
		led->turn_off_delay_ms = val;
	else if (rc != -EINVAL)
		return rc;

	return 0;
}

/*
 * Handlers for alternative sources of platform_data
 */

static int __devinit qpnp_get_config_pwm(struct pwm_config_data *pwm_cfg,
				struct spmi_device *spmi_dev,
				struct device_node *node)
{
	int rc;
	u32 val;

	rc = of_property_read_u32(node, "qcom,pwm-channel", &val);
	if (!rc)
		pwm_cfg->pwm_channel = val;
	else
		return rc;

	rc = of_property_read_u32(node, "qcom,pwm-us", &val);
	if (!rc)
		pwm_cfg->pwm_period_us = val;
	else
			return rc;

	pwm_cfg->blinking = 0;

	pwm_cfg->duty_cycles = devm_kzalloc(&spmi_dev->dev,
		sizeof(struct pwm_duty_cycles), GFP_KERNEL);
	if (!pwm_cfg->duty_cycles) {
		dev_err(&spmi_dev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	pwm_cfg->duty_cycles->num_duty_pcts = PWM_LUT_MAX_SIZE;
	pwm_cfg->duty_cycles->duty_pcts = devm_kzalloc(&spmi_dev->dev,
		sizeof(int) * pwm_cfg->duty_cycles->num_duty_pcts, GFP_KERNEL);
	if (!pwm_cfg->duty_cycles->duty_pcts) {
		dev_err(&spmi_dev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	return 0;
};

static int qpnp_led_get_mode(const char *mode)
{
	if (strncmp(mode, "manual", strlen(mode)) == 0)
		return MANUAL_MODE;
	else if (strncmp(mode, "pwm", strlen(mode)) == 0)
		return PWM_MODE;
	else if (strncmp(mode, "lpg", strlen(mode)) == 0)
		return LPG_MODE;
	else
		return -EINVAL;
};

static int qpnp_led_arrange(const char *label, struct qpnp_led_data *led)
{
	if (blink_cnt < BLINK_LEDS_ARRAY_SIZE) {
		if (strncmp(label, "white", strlen(label)) == 0) {
			blink_array[0].led = led;
			blink_array[0].mask = WHITE_LED_MASK;
		} else if (strncmp(label, "red", strlen(label)) == 0) {
			blink_array[0].led = led;
			blink_array[0].mask = RED_LED_MASK;
		} else if (strncmp(label, "green", strlen(label)) == 0) {
			blink_array[1].led = led;
			blink_array[1].mask = GREEN_LED_MASK;
		} else if (strncmp(label, "blue", strlen(label)) == 0) {
			blink_array[2].led = led;
			blink_array[2].mask = BLUE_LED_MASK;
		} else {
			pr_err("%s Unexpected label %s\n", __func__, label);
			return -EINVAL;
		}
	} else {
		dev_err(&led->spmi_dev->dev, "Too many blink leds\n");
			return -EINVAL;
	}
	blink_cnt++;
	return 0;
};

static int __devinit qpnp_get_config_rgb(struct qpnp_led_data *led,
				struct device_node *node)
{
	int rc;
	u8 led_mode;
	const char *mode;

	led->rgb_cfg = devm_kzalloc(&led->spmi_dev->dev,
				sizeof(struct rgb_config_data), GFP_KERNEL);
	if (!led->rgb_cfg) {
		dev_err(&led->spmi_dev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	if (led->id == QPNP_ID_RGB_RED)
		led->rgb_cfg->enable = RGB_LED_ENABLE_RED;
	else if (led->id == QPNP_ID_RGB_GREEN)
		led->rgb_cfg->enable = RGB_LED_ENABLE_GREEN;
	else if (led->id == QPNP_ID_RGB_BLUE)
		led->rgb_cfg->enable = RGB_LED_ENABLE_BLUE;
	else
		return -EINVAL;

	rc = of_property_read_string(node, "qcom,mode", &mode);
	if (!rc) {
		led_mode = qpnp_led_get_mode(mode);
		if ((led_mode == MANUAL_MODE) || (led_mode == -EINVAL)) {
			dev_err(&led->spmi_dev->dev, "Selected mode not " \
				"supported for rgb.\n");
			return -EINVAL;
		}
		led->rgb_cfg->pwm_cfg = devm_kzalloc(&led->spmi_dev->dev,
					sizeof(struct pwm_config_data),
					GFP_KERNEL);
		if (!led->rgb_cfg->pwm_cfg) {
			dev_err(&led->spmi_dev->dev,
				"Unable to allocate memory\n");
			return -ENOMEM;
		}
		led->rgb_cfg->pwm_cfg->mode = led_mode;
	} else
		return rc;

	rc = qpnp_get_config_pwm(led->rgb_cfg->pwm_cfg, led->spmi_dev, node);
	if (rc < 0)
		return rc;

	return 0;
}

static int __devinit qpnp_get_config_mpp(struct qpnp_led_data *led,
		struct device_node *node)
{
	int rc;
	u32 val;
	u8 led_mode;
	const char *mode;

	led->mpp_cfg = devm_kzalloc(&led->spmi_dev->dev,
			sizeof(struct mpp_config_data), GFP_KERNEL);
	if (!led->mpp_cfg) {
		dev_err(&led->spmi_dev->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	led->mpp_cfg->current_setting = LED_MPP_CURRENT_DEFAULT;
	rc = of_property_read_u32(node, "qcom,current-setting", &val);
	if (!rc)
		led->mpp_cfg->current_setting = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->mpp_cfg->source_sel = LED_MPP_SOURCE_SEL_DEFAULT;
	rc = of_property_read_u32(node, "qcom,source-sel", &val);
	if (!rc)
		led->mpp_cfg->source_sel = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	led->mpp_cfg->mode_ctrl = LED_MPP_MODE_SINK;
	rc = of_property_read_u32(node, "qcom,mode-ctrl", &val);
	if (!rc)
		led->mpp_cfg->mode_ctrl = (u8) val;
	else if (rc != -EINVAL)
		return rc;

	rc = of_property_read_string(node, "qcom,mode", &mode);
	if (!rc) {
		led_mode = qpnp_led_get_mode(mode);
		led->mpp_cfg->pwm_mode = led_mode;
		if (led_mode == MANUAL_MODE)
			return MANUAL_MODE;
		else if (led_mode == -EINVAL) {
			dev_err(&led->spmi_dev->dev, "Selected mode not " \
				"supported for mpp.\n");
			return -EINVAL;
		}
		led->mpp_cfg->pwm_cfg = devm_kzalloc(&led->spmi_dev->dev,
					sizeof(struct pwm_config_data),
					GFP_KERNEL);
		if (!led->mpp_cfg->pwm_cfg) {
			dev_err(&led->spmi_dev->dev,
				"Unable to allocate memory\n");
			return -ENOMEM;
		}
		led->mpp_cfg->pwm_cfg->mode = led_mode;
	} else
		return rc;

	rc = qpnp_get_config_pwm(led->mpp_cfg->pwm_cfg, led->spmi_dev, node);
	if (rc < 0)
		return rc;

	return 0;
}

static int __devinit qpnp_leds_rgb_probe(struct spmi_device *spmi)
{
	struct qpnp_led_data *led, *led_array;
	struct resource *led_resource;
	struct device_node *node, *temp;
	int rc, i, num_leds = 0, parsed_leds = 0;
	const char *led_label;

	node = spmi->dev.of_node;
	if (node == NULL)
		return -ENODEV;

	temp = NULL;
	while ((temp = of_get_next_child(node, temp)))
		num_leds++;

	if (!num_leds)
		return -ECHILD;

	led_array = devm_kzalloc(&spmi->dev,
		(sizeof(struct qpnp_led_data) * num_leds), GFP_KERNEL);
	if (!led_array) {
		dev_err(&spmi->dev, "Unable to allocate memory\n");
		return -ENOMEM;
	}

	for_each_child_of_node(node, temp) {
		led = &led_array[parsed_leds];
		led->num_leds = num_leds;
		led->spmi_dev = spmi;

		led_resource = spmi_get_resource(spmi, NULL, IORESOURCE_MEM, 0);
		if (!led_resource) {
			dev_err(&spmi->dev, "Unable to get LED base address\n");
			rc = -ENXIO;
			goto fail_id_check;
		}
		led->base = led_resource->start;

		rc = of_property_read_string(temp, "label", &led_label);
		if (rc < 0) {
			dev_err(&led->spmi_dev->dev,
				"Failure reading label, rc = %d\n", rc);
			goto fail_id_check;
		}

		rc = of_property_read_string(temp, "linux,name",
			&led->cdev.name);
		if (rc < 0) {
			dev_err(&led->spmi_dev->dev,
				"Failure reading led name, rc = %d\n", rc);
			goto fail_id_check;
		}

		rc = of_property_read_u32(temp, "qcom,max-current",
			&led->max_current);
		if (rc < 0) {
			dev_err(&led->spmi_dev->dev,
				"Failure reading max_current, rc =  %d\n", rc);
			goto fail_id_check;
		}

		rc = of_property_read_u32(temp, "qcom,id", &led->id);
		if (rc < 0) {
			dev_err(&led->spmi_dev->dev,
				"Failure reading led id, rc =  %d\n", rc);
			goto fail_id_check;
		}

		rc = qpnp_get_common_configs(led, temp);
		if (rc) {
			dev_err(&led->spmi_dev->dev,
				"Failure reading common led configuration," \
				" rc = %d\n", rc);
			goto fail_id_check;
		}

		led->cdev.brightness_set    = qpnp_led_set;
		led->cdev.brightness_get    = qpnp_led_get;

		if (strncmp(led_label, "rgb", sizeof("rgb")) == 0) {
			rc = qpnp_get_config_rgb(led, temp);
			if (rc < 0) {
				dev_err(&led->spmi_dev->dev,
					"Unable to read rgb config data\n");
				goto fail_id_check;
			}
		} else if (strncmp(led_label, "mpp", sizeof("mpp")) == 0) {
			rc = qpnp_get_config_mpp(led, temp);
			if (rc < 0) {
				dev_err(&led->spmi_dev->dev,
						"Unable to read mpp config data\n");
				goto fail_id_check;
			}
		} else if (strncmp(led_label, "combo", sizeof("combo")) == 0) {
			/* Do nothing. Needed to have valid id check */
		} else {
			dev_err(&led->spmi_dev->dev, "No LED matching label\n");
			rc = -EINVAL;
			goto fail_id_check;
		}

		spin_lock_init(&led->lock);

		rc =  qpnp_led_initialize(led);
		if (rc < 0)
			goto fail_id_check;

		rc = qpnp_led_set_max_brightness(led);
		if (rc < 0)
			goto fail_id_check;

		rc = led_classdev_register(&spmi->dev, &led->cdev);
		if (rc) {
			dev_err(&spmi->dev, "unable to register led %d,rc=%d\n",
						 led->id, rc);
			goto fail_id_check;
		}

		if (QPNP_ID_COMBO == led->id) {

			rc = sysfs_create_group(&led->cdev.dev->kobj,
				&combo_attribute_group);
			if (rc < 0) {
				pr_err("couldn't register attribute sysfs group\n");
				goto fail_id_check;
			}

		} else {
			/* Because of the RGB mask now we care about
				place of a LED in the array */
			if (qpnp_led_arrange(led->cdev.name, led))
				goto fail_id_check;
		}

		/* configure default state */
		if (led->default_on) {
			led->cdev.brightness = led->cdev.max_brightness;
			if (led->turn_off_delay_ms > 0)
				qpnp_led_turn_off(led);
		} else
			led->cdev.brightness = LED_OFF;

		qpnp_led_set(&led->cdev, led->cdev.brightness);

		parsed_leds++;
	}
	dev_set_drvdata(&spmi->dev, led_array);
	return 0;

fail_id_check:
	for (i = 0; i < parsed_leds; i++)
		led_classdev_unregister(&led_array[i].cdev);
	return rc;
}

static int __devexit qpnp_leds_rgb_remove(struct spmi_device *spmi)
{
	struct qpnp_led_data *led_array  = dev_get_drvdata(&spmi->dev);
	int i, parsed_leds = led_array->num_leds;

	for (i = 0; i < parsed_leds; i++) {
		led_classdev_unregister(&led_array[i].cdev);
		switch (led_array[i].id) {
		case QPNP_ID_RGB_RED:
		case QPNP_ID_RGB_GREEN:
		case QPNP_ID_RGB_BLUE:
		default:
			dev_err(&led_array[i].spmi_dev->dev,
					"Invalid LED(%d)\n",
					led_array[i].id);
			return -EINVAL;
		}
	}

	return 0;
}
static struct of_device_id spmi_match_table[] = {
	{	.compatible = "qcom,leds-qpnp-rgb",
	}
};

static struct spmi_driver qpnp_leds_rgb_driver = {
	.driver		= {
		.name	= "qcom,leds-qpnp-rgb",
		.of_match_table = spmi_match_table,
	},
	.probe		= qpnp_leds_rgb_probe,
	.remove		= __devexit_p(qpnp_leds_rgb_remove),
};

static int __init qpnp_leds_rgb_init(void)
{
	return spmi_driver_register(&qpnp_leds_rgb_driver);
}
module_init(qpnp_leds_rgb_init);

static void __exit qpnp_leds_rgb_exit(void)
{
	spmi_driver_unregister(&qpnp_leds_rgb_driver);
}
module_exit(qpnp_leds_rgb_exit);

MODULE_DESCRIPTION("QPNP LEDs RGB driver");
MODULE_LICENSE("GPL v2");
MODULE_ALIAS("leds:leds-qpnp-rgb");

