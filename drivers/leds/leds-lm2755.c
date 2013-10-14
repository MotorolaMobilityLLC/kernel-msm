/*
 * LM2755 LED data driver.
 *
 * Copyright (C) 2012 Motorola Mobility, Inc.
 *
 * Contact: Alina Yakovleva <qvdh43@motorola.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	 See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin St, Fifth Floor, Boston, MA
 * 02110-1301 USA
 */
#define pr_fmt(fmt)     "%s: " fmt, __func__

#include <linux/module.h>
#include <linux/init.h>
#include <linux/i2c.h>
#include <linux/mutex.h>
#include <linux/gpio.h>
#include <linux/interrupt.h>
#include <linux/delay.h>
#include <linux/ctype.h>
#include <linux/spinlock.h>
#include <linux/wait.h>
#include <linux/leds.h>
#include <linux/leds-lm2755.h>
#include <linux/workqueue.h>
#include <linux/slab.h>

#define DRIVER_NAME "lm2755"

#define LM2755_ENABLE_BIT 0x01
#define LM2755_ENABLE_BLINK_BIT 0x08
#define LM2755_CLOCK_BIT 0x40
#define LM2755_CHARGE_PUMP_BIT 0x80
#define LM2755_NTSTEP 8
#define LM2755_MIN_TSTEP_INT 50 /* 50 us internal step */
#define LM2755_D1_BASE 0xA0
#define LM2755_D2_BASE 0xB0
#define LM2755_D3_BASE 0xC0
#define LM2755_D1_MASK 0xFF0000
#define LM2755_D2_MASK 0x00FF00
#define LM2755_D3_MASK 0x0000FF

#define LM2755_REGISTERS \
	ENTRY(GENERAL,           0x10) \
	ENTRY(TIME_STEP,         0x20) \
	ENTRY(D1_DELAY,          0xA1) \
	ENTRY(D1_LOW,            0xA8) \
	ENTRY(D1_HIGH,           0xA9) \
	ENTRY(D1_TIME_LOW,       0xA2) \
	ENTRY(D1_TIME_HIGH,      0xA3) \
	ENTRY(D1_RAMP_DOWN_STEP, 0xA4) \
	ENTRY(D1_RAMP_UP_STEP,   0xA5) \
	ENTRY(D2_DELAY,          0xB1) \
	ENTRY(D2_LOW,            0xB8) \
	ENTRY(D2_HIGH,           0xB9) \
	ENTRY(D2_TIME_LOW,       0xB2) \
	ENTRY(D2_TIME_HIGH,      0xB3) \
	ENTRY(D2_RAMP_DOWN_STEP, 0xB4) \
	ENTRY(D2_RAMP_UP_STEP,   0xB5) \
	ENTRY(D3_DELAY,          0xC1) \
	ENTRY(D3_LOW,            0xC8) \
	ENTRY(D3_HIGH,           0xC9) \
	ENTRY(D3_TIME_LOW,       0xC2) \
	ENTRY(D3_TIME_HIGH,      0xC3) \
	ENTRY(D3_RAMP_DOWN_STEP, 0xC4) \
	ENTRY(D3_RAMP_UP_STEP,   0xC5)

#define LM2755_REG_DELAY            0x01
#define LM2755_REG_TIME_LOW         0x02
#define LM2755_REG_TIME_HIGH        0x03
#define LM2755_REG_RAMP_DOWN_STEP   0x04
#define LM2755_REG_RAMP_UP_STEP     0x05
#define LM2755_REG_LOW              0x08
#define LM2755_REG_HIGH             0x09

#define ENTRY(a, b) {#a, b},
static struct lm2755_reg {
	const char *name;
	int reg;
} lm2755_regs[] = {
	LM2755_REGISTERS
};
#undef ENTRY

#define ENTRY(a, b) LM2755_REG_##a = b,
enum {
	LM2755_REGISTERS
};
#undef ENTRY

#define LM2755_NUM_REGS ARRAY_SIZE(lm2755_regs)

#define DEFAULT_LM2755_CLASS_NAME "rgb"

static const char * const names[] = {
	"d1-led-name",
	"d2-led-name",
	"d3-led-name"
};

/* Trace register writes */
static unsigned trace_write;
module_param(trace_write, uint, 0664);
#define pr_write(fmt, args...) \
				{ \
				if (trace_write) \
					pr_info(fmt, ##args);\
				}

static unsigned trace_access = 1;
module_param(trace_access, uint, 0664);
#define pr_access(fmt, args...) {\
			if (trace_access)\
				pr_info(fmt, ##args);\
			}

static unsigned trace_verbose;
module_param(trace_verbose, uint, 0664);

struct lm2755_led {
	struct led_classdev led_cdev;
	struct work_struct work;
	uint8_t level;
	int id;
};

struct lm2755_data {
	struct lm2755_led leds[LM2755_NUM_LEDS];
	uint32_t tstep[LM2755_NTSTEP];
	uint32_t min_tstep;
	uint8_t config;
	uint32_t max_time;
	uint8_t max_level;
	struct mutex lock;
	struct led_classdev	led_cdev;
	struct work_struct rgb_work;
	struct i2c_client *client;
	/* D1, D2, D3 levels */
	uint8_t rgb[LM2755_NUM_LEDS];
	uint8_t rgb_group[LM2755_NUM_LEDS];
	uint32_t rgb_mask[LM2755_NUM_LEDS];
	unsigned ms_on;    /* Blink rate in ms */
	unsigned ms_off;
	unsigned ramp_up;   /* Ramp rate in percent */
	unsigned ramp_down;
	void (*enable)(bool state);
};

static inline uint8_t lm2755_scale(int brightness, uint8_t max)
{
	if (brightness == 0)
		return 0;
	brightness = brightness * max / LED_FULL;
	if (brightness == 0)
		brightness = 1;
	return (uint8_t)(brightness);
}

static inline struct lm2755_led *cdev_to_led(struct led_classdev *cdev)
{
	return container_of(cdev, struct lm2755_led, led_cdev);
}

static inline struct lm2755_data *led_to_lm2755(struct lm2755_led *led)
{
	return container_of(led, struct lm2755_data, leds[led->id]);
}

static inline int lm2755_write(struct i2c_client *client,
	uint8_t reg, uint8_t value)
{
	int ret;

	pr_write("reg 0x%02X, value 0x%02X\n", reg, value);
	ret = i2c_smbus_write_byte_data(client, reg, value);
	if (ret < 0)
		pr_err("unable to write reg 0x%02X, value 0x%02X: %d\n",
			reg, value, ret);
	return ret;
}

static int lm2755_read(struct i2c_client *client, uint8_t reg, uint8_t *buf)
{
	s32 ret;

	ret = i2c_smbus_read_byte_data(client, reg);
	if (ret < 0) {
		pr_err("unable to read reg 0x%02X: %d\n", reg, ret);
		return ret;
	}

	*buf = ret;
	return 0;
}

static int lm2755_enable_led(struct lm2755_data *data, uint8_t group)
{
	uint8_t val;
	int ret;

	/* Read general register */
	ret = lm2755_read(data->client, LM2755_REG_GENERAL, &val);
	if (ret < 0)
		return ret;
	/* Clear charge pump and clock bits */
	val &= ~(LM2755_CHARGE_PUMP_BIT | LM2755_CLOCK_BIT);
	val |= data->config;

	/* Enable requested group */
	val |= LM2755_ENABLE_BIT << group;
	val &= ~(LM2755_ENABLE_BLINK_BIT << group);

	ret = lm2755_write(data->client, LM2755_REG_GENERAL, val);
	return ret;
}

static int lm2755_disable_led(struct lm2755_data *data, uint8_t group)
{
	uint8_t val;
	int ret;
	uint8_t disable = LM2755_ENABLE_BIT | LM2755_ENABLE_BLINK_BIT;

	/* Read general register */
	ret = lm2755_read(data->client, LM2755_REG_GENERAL, &val);
	if (ret < 0)
		return ret;
	/* Clear charge pump and clock bits */
	val &= ~(LM2755_CHARGE_PUMP_BIT | LM2755_CLOCK_BIT);
	val |= data->config;

	/* Disable requested group */
	disable = ~(disable << group);
	val &= disable;

	ret = lm2755_write(data->client, LM2755_REG_GENERAL, val);
	return ret;
}

/* Set brightness of an RGB LED */
static void lm2755_rgb_brightness_set(struct led_classdev *cdev,
			     enum led_brightness brightness)
{
	struct lm2755_data *data =
		container_of(cdev, struct lm2755_data, led_cdev);
	int i;

	pr_access("%s, value = %d\n", cdev->name, brightness);
	mutex_lock(&data->lock);
	brightness = lm2755_scale(brightness, data->max_level);
	/* from this interface brightness is the same for all components */
	/* interface not utilized by the HAL at this moment*/
	for (i = 0; i < LM2755_NUM_LEDS; i++) {
		if (data->leds[i].id < 0)
			continue;
		data->rgb[i] = brightness;
	}
	data->ms_on = 0;
	data->ms_off = 0;
	data->ramp_up = 0;
	data->ramp_down = 0;
	mutex_unlock(&data->lock);

	schedule_work(&data->rgb_work);
}

static enum led_brightness lm2755_rgb_brightness_get(struct led_classdev *cdev)
{
	struct lm2755_data *data =
		container_of(cdev, struct lm2755_data, led_cdev);

	return data->rgb[0];
}

/* Calculate n for high/low given time and n of step */
static unsigned lm2755_calc_nhighlow(struct lm2755_data *data,
	uint32_t nstep, uint32_t time)
{
	uint32_t n;
	uint32_t time_actual;
	uint32_t tstep = data->tstep[nstep]/1000;

	if (time < tstep)
		return 0;
	n = time/tstep - 1;
	if (n >= 0xFF)
		return 0xFF;
	time_actual = tstep * (n+1);
	if (time_actual < time)
		return n+1;
	else
		return n;
}

/* Calculate ramping time given n of step, brighntess level and n or ramp */
static inline uint32_t lm2755_calc_tramp(struct lm2755_data *data,
	uint32_t nstep, uint32_t level, uint32_t nramp)
{
	if (nramp <= 1)
		return (data->tstep[0]/1000) * (level - 1) * 4;
	else
		return (data->tstep[nstep]/1000) * (level - 1) * (nramp - 1);
}

/* Calculate n or ramp given brightness level, n or step, and ramping time */
static uint32_t lm2755_calc_nramp(struct lm2755_data *data,
	uint32_t nstep, uint32_t level, uint32_t time)
{
	uint32_t nramp;
	uint32_t time_actual;
	uint32_t time_next;
	uint32_t tstep = data->tstep[nstep]/1000;

	if (level == 1)
		level++;
	nramp = time/(tstep * (level-1)) + 1;
	time_actual = lm2755_calc_tramp(data, nstep, level, nramp);
	if (time > time_actual) {
		/* Try higher index */
		if (nramp >= 0xFF)
			return 0xFF;
		time_next = lm2755_calc_tramp(data, nstep, level, nramp+1);
		/* Return the closest one */
		if (time - time_actual > time_next - time)
			nramp++;
	} else {
		/* Try lower index */
		if (nramp <= 1)
			return nramp;
		time_next = lm2755_calc_tramp(data, nstep, level, nramp-1);
		if (time_actual - time > time - time_next)
			nramp--;
	}

	return nramp;
}

/* Work to enable RGB LED */
static void lm2755_rgb_work(struct work_struct *work)
{
	struct lm2755_data *data =
		container_of(work, struct lm2755_data, rgb_work);
	uint8_t val = data->config;
	int i;
	uint8_t nstep;
	uint8_t nrise, nfall, nlow, nhigh;
	uint32_t time_rise, time_fall, time_high;
	uint32_t time_rise_act, time_fall_act;
	uint32_t us_off_low, us_off_high, us_off, us_on;

	mutex_lock(&data->lock);
	pr_debug("0x%02X%02X%02X, on/off=%u/%u ms, ramp=%d%%/%d%%\n",
		data->rgb[LM2755_D1], data->rgb[LM2755_D2],
		data->rgb[LM2755_D3], data->ms_on, data->ms_off,
		data->ramp_up, data->ramp_down);
	/* Turn it off first */
	lm2755_write(data->client, LM2755_REG_GENERAL, val);
	/* Enable those LEDs that are on */
	for (i = 0; i < LM2755_NUM_LEDS; i++) {
		if (data->rgb[i] == 0)
			continue;
		/* Save position of enabled LEDs in the general reg. mask*/
		val |= LM2755_ENABLE_BIT << i;
		/* Low brightness level is 0 */
		lm2755_write(data->client,
			data->rgb_group[i] | LM2755_REG_LOW, 0);
		/* High brightness level */
		lm2755_write(data->client,
			data->rgb_group[i] | LM2755_REG_HIGH, data->rgb[i]);
	}
	if (data->ms_on != 0) { /* Blinking */
		us_on = data->ms_on * 1000;
		us_off = data->ms_off * 1000;
		/* Find tstep for ms_off first, times are in microseconds */
		for (nstep = 0; nstep < LM2755_NTSTEP; nstep++) {
			us_off_low = data->tstep[nstep]/1000;
			us_off_high = us_off_low * 256;
			if (us_off_low < us_off && us_off < us_off_high)
				break;
		}
		if (nstep == LM2755_NTSTEP)
			nstep--;
		pr_debug("nstep = %d\n", nstep);
		lm2755_write(data->client, LM2755_REG_TIME_STEP, nstep);

		/* Ramp up and down in microseconds */
		time_rise = us_on * data->ramp_up / 100;
		time_fall = us_on * data->ramp_down / 100;

		/* Calculate N for time off, rise, and fall */
		nlow = lm2755_calc_nhighlow(data, nstep, us_off);
		pr_debug("nlow = %d, time_off = %u ms\n",
			nlow, (data->tstep[nstep] * (nlow+1))/1000000);

		for (i = 0; i < LM2755_NUM_LEDS; i++) {
			if (data->rgb[i] == 0)
				continue;
			if (data->ramp_up) {
				nrise = lm2755_calc_nramp(data, nstep,
					data->rgb[i], time_rise);
				time_rise_act = lm2755_calc_tramp(data, nstep,
					data->rgb[i], nrise);
			} else {
				nrise = 0;
				time_rise_act = 0;
			}
			if (data->ramp_down) {
				nfall = lm2755_calc_nramp(data, nstep,
					data->rgb[i], time_fall);
				time_fall_act = lm2755_calc_tramp(data, nstep,
					data->rgb[i], nfall);
			} else {
				nfall = 0;
				time_fall_act = 0;
			}

			if (us_on <= (time_rise_act + time_fall_act))
				time_high = 0;
			else
				time_high =
					us_on - time_rise_act - time_fall_act;
			nhigh = lm2755_calc_nhighlow(data, nstep, time_high);

			pr_debug("LED D%d: nrise = %u (0x%x), time = %u ms\n",
				(i+1), nrise, nrise, time_rise_act/1000);
			pr_debug("LED D%d: nfall = %u (0x%x), time = %u ms\n",
				(i+1), nfall, nfall, time_fall_act/1000);
			pr_debug("LED D%d: nhigh = %u (0x%x), time = %u ms\n",
				(i+1), nhigh, nhigh, time_high/1000);

			/* N for low */
			lm2755_write(data->client, data->rgb_group[i]
				| LM2755_REG_TIME_LOW, nlow);
			/* N for high */
			lm2755_write(data->client, data->rgb_group[i]
				| LM2755_REG_TIME_HIGH, nhigh);
			/* N for ramp up */
			lm2755_write(data->client, data->rgb_group[i]
				| LM2755_REG_RAMP_UP_STEP, nrise);
			/* N for ramp down */
			lm2755_write(data->client,
				data->rgb_group[i]
				| LM2755_REG_RAMP_DOWN_STEP, nfall);
			/* Add any leds enabled by this command */
			val |= LM2755_ENABLE_BLINK_BIT << i;
		}
	}
	lm2755_write(data->client, LM2755_REG_GENERAL, val);
	mutex_unlock(&data->lock);
}

/* Set brightness of a single LED */
static void lm2755_brightness_set(struct led_classdev *cdev,
			     enum led_brightness brightness)
{
	struct lm2755_led *led = cdev_to_led(cdev);
	struct lm2755_data *data = led_to_lm2755(led);

	pr_access("%s, value = %d\n", cdev->name, brightness);

	mutex_lock(&data->lock);
	led->level = lm2755_scale(brightness, data->max_level);
	mutex_unlock(&data->lock);
	schedule_work(&led->work);
}

static enum led_brightness lm2755_brightness_get(struct led_classdev *cdev)
{
	struct lm2755_led *led = cdev_to_led(cdev);

	return led->level;
}

/* Work to enable single LED */
static void lm2755_led_work(struct work_struct *work)
{
	struct lm2755_led *led =
		container_of(work, struct lm2755_led, work);
	struct lm2755_data *data = led_to_lm2755(led);
	uint8_t group = data->rgb_group[led->id];

	mutex_lock(&data->lock);
	/* Write High Level */
	pr_debug("%s, group %#x led id %#x level = %#x\n",
		led->led_cdev.name, group, led->id, led->level);
	lm2755_write(data->client, group | LM2755_REG_HIGH, led->level);
	/* If LEDs were set via rgb control clear them */
	if (data->rgb[LM2755_D1] ||
		data->rgb[LM2755_D2] ||
		data->rgb[LM2755_D3]) {
		int i;

		data->ms_on = 0;
		data->ms_off = 0;
		data->ramp_up = 0;
		data->ramp_down = 0;
		for (i = 0; i < LM2755_NUM_LEDS; i++)
			data->rgb[i] = 0;
		lm2755_write(data->client, LM2755_REG_GENERAL, data->config);
	}
	if (led->level)
		lm2755_enable_led(data, led->id);
	else
		lm2755_disable_led(data, led->id);
	mutex_unlock(&data->lock);
}

static ssize_t lm2755_show_control(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct lm2755_data *data =
		container_of(led, struct lm2755_data, led_cdev);

	scnprintf(buf, PAGE_SIZE,
		"RGB=0x%02X%02X%02X, on/off=%d/%d ms, ramp=%d%%/%d%%\n",
		data->rgb[LM2755_D1], data->rgb[LM2755_D2],
		data->rgb[LM2755_D3], data->ms_on, data->ms_off,
		data->ramp_up, data->ramp_down);

	return strlen(buf);
}

static ssize_t lm2755_store_control(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct lm2755_data *data =
		container_of(led, struct lm2755_data, led_cdev);
	unsigned rgb = 0, ms_on = 0, ms_off = 0, rup = 0, rdown = 0;
	int i;

	if (len == 0)
		return 0;

	sscanf(buf, "%x %u %u %u %u", &rgb, &ms_on, &ms_off, &rup, &rdown);
	pr_access("0x%X, on/off=%u/%u ms, ramp=%u%%/%u%%\n",
		rgb, ms_on, ms_off, rup, rdown);

	/* LED that is off is not blinking */
	if (rgb == 0) {
		ms_on = 0;
		ms_off = 0;
	}
	/* Total ramp time cannot be more than 100% */
	if (rup + rdown > 100) {
		rup = 50;
		rdown = 50;
	}
	/* If ms_on is not 0 but ms_off is 0 we won't blink */
	if (ms_off == 0)
		ms_on = 0;
	/* Check ms_on and ms_off ranges */
	if (ms_on > data->max_time)
		ms_on = data->max_time;
	if (ms_off > data->max_time)
		ms_off = data->max_time;
	mutex_lock(&data->lock);
	for (i = 0; i < LM2755_NUM_LEDS; i++) {
		if (data->leds[i].id < 0)
			continue;
		data->rgb[i] =
			(uint8_t)((rgb & data->rgb_mask[i]) >> (16 - i*8));
	}
	/* If blinking equalize to max, otherwise they will blink out of sync */
	if (rgb != 0 && ms_on != 0) {
		uint8_t max = data->rgb[0];
		for (i = 1; i < LM2755_NUM_LEDS; i++) {
			if (data->rgb[i] > max)
				max = data->rgb[i];
		}
		for (i = 0; i < LM2755_NUM_LEDS; i++) {
			if (data->rgb[i] != 0)
				data->rgb[i] = max;
		}
	}
	/* Scale by max_level */
	for (i = 0; i < LM2755_NUM_LEDS; i++)
		data->rgb[i] = lm2755_scale(data->rgb[i], data->max_level);

	data->ms_on = ms_on;
	data->ms_off = ms_off;
	data->ramp_up = rup;
	data->ramp_down = rdown;
	mutex_unlock(&data->lock);

	schedule_work(&data->rgb_work);

	return len;
}

static ssize_t lm2755_store_registers(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct lm2755_data *data =
		container_of(led, struct lm2755_data, led_cdev);
	uint8_t reg;
	uint8_t value;

	sscanf(buf, "%hhx %hhx", &reg, &value);
	pr_debug("writing reg 0x%x = 0x%x\n", (unsigned)reg, (unsigned)value);
	lm2755_write(data->client, reg, value);

	return len;
}

static ssize_t lm2755_show_registers(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct lm2755_data *data =
		container_of(led, struct lm2755_data, led_cdev);
	int i, n = 0;
	uint8_t value = 0;

	pr_debug("%s: reading registers\n", __func__);
	for (i = 0, n = 0; i < LM2755_NUM_REGS; i++) {
		lm2755_read(data->client, lm2755_regs[i].reg, &value);
		n += scnprintf(buf + n, PAGE_SIZE - n,
			"%-20s (0x%X) = 0x%02X\n",
			lm2755_regs[i].name, (unsigned)lm2755_regs[i].reg,
			(unsigned)value);
	}
	return n;
}

static void lm2755_set_tsteps(struct lm2755_data *data, uint32_t min_tstep)
{
	int i;

	/* Populate tstep array */
	for (i = 0; i < LM2755_NTSTEP; i++)
		data->tstep[i] = min_tstep * (1 << i);
	data->max_time = (0xFF * data->tstep[LM2755_NTSTEP-1])/1000;
}

static ssize_t lm2755_store_clock(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct lm2755_data *data =
		container_of(led, struct lm2755_data, led_cdev);
	uint8_t clock_bit = LM2755_CLOCK_BIT;

	if (buf[0] == 'i') {
		data->config &= ~clock_bit;
		lm2755_set_tsteps(data, LM2755_MIN_TSTEP_INT * 1000);
		pr_info("changing to internal clock, %u us\n",
			data->tstep[0]/1000);
	} else {
		/* Can only do that if external clock was configured before */
		if (data->min_tstep == 0) {
			pr_err("unable to change to external clock, not configured\n");
		} else {
			lm2755_set_tsteps(data, data->min_tstep);
			data->config |= clock_bit;
			pr_info("changing external clock, %u us\n",
				data->tstep[0]/1000);
		}
	}
	return len;
}

static ssize_t lm2755_show_clock(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct lm2755_data *data =
		container_of(led, struct lm2755_data, led_cdev);

	scnprintf(buf, PAGE_SIZE, "%s, min tstep = %u us\n",
		data->config & LM2755_CLOCK_BIT ? "external" : "internal",
		data->tstep[0]/1000);
	return strlen(buf);
}

static ssize_t lm2755_store_max_level(struct device *dev,
			     struct device_attribute *attr,
			     const char *buf, size_t len)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct lm2755_data *data =
		container_of(led, struct lm2755_data, led_cdev);
	int level = 0;
	int ret;

	ret = kstrtoint(buf, 16, &level);
	if (ret < 0) {
		pr_err("unable to convert \'%s\' to int\n", buf);
		return -EINVAL;
	}
	if (level == 0 || level > LM2755_MAX_LEVEL) {
		pr_err("invalid level 0x%X\n", level);
		return -EINVAL;
	}
	mutex_lock(&data->lock);
	data->max_level = level;
	mutex_unlock(&data->lock);
	return len;
}

static ssize_t lm2755_show_max_level(struct device *dev,
			    struct device_attribute *attr,
			    char *buf)
{
	struct led_classdev *led = dev_get_drvdata(dev);
	struct lm2755_data *data =
		container_of(led, struct lm2755_data, led_cdev);

	snprintf(buf, PAGE_SIZE, "0x%02X\n", data->max_level);
	return strlen(buf);
}

/* LED class device attributes */
static DEVICE_ATTR(control, S_IRUGO | S_IWUSR,
	lm2755_show_control, lm2755_store_control);
static DEVICE_ATTR(registers, S_IRUGO | S_IWUSR,
	lm2755_show_registers, lm2755_store_registers);
static DEVICE_ATTR(clock, S_IRUGO | S_IWUSR,
	lm2755_show_clock, lm2755_store_clock);
static DEVICE_ATTR(max_level, S_IRUGO | S_IWUSR,
	lm2755_show_max_level, lm2755_store_max_level);

static struct attribute *lm2755_led_attributes[] = {
	&dev_attr_control.attr,
	&dev_attr_registers.attr,
	&dev_attr_clock.attr,
	&dev_attr_max_level.attr,
	NULL,
};

static struct attribute_group lm2755_attribute_group = {
	.attrs = lm2755_led_attributes
};

static void lm2755_unregister_leds(struct lm2755_data *data)
{
	int i;

	for (i = 0; i < LM2755_NUM_LEDS; i++) {
		if (data->leds[i].id != -1) {
			led_classdev_unregister(&data->leds[i].led_cdev);
			cancel_work_sync(&data->leds[i].work);
			data->leds[i].id = -1;
		}
	}
}

static int lm2755_register_leds(struct lm2755_data *data, const char **names)
{
	int i;
	int ret;

	for (i = 0; i < LM2755_NUM_LEDS; i++)
		data->leds[i].id = -1;

	for (i = 0; i < LM2755_NUM_LEDS; i++) {
		if (names[i] == NULL)
			continue;
		data->leds[i].led_cdev.brightness_set = lm2755_brightness_set;
		data->leds[i].led_cdev.brightness_get = lm2755_brightness_get;
		data->leds[i].led_cdev.name = names[i];
		ret = led_classdev_register(&data->client->dev,
			&data->leds[i].led_cdev);
		if (ret < 0) {
			pr_err("couldn't register \'%s\' LED clas\n", names[i]);
			return ret;
		}
		pr_info("registered \'%s\' LED class\n", names[i]);
		data->leds[i].id = i;
		INIT_WORK(&data->leds[i].work, lm2755_led_work);
	}

	return 0;
}

#ifdef CONFIG_OF
static int lm2755_parse_dt(struct device *dev,
		struct lm2755_platform_data *pdata)
{
	int rc, i;
	u32 temp_val;
	struct device_node *np = dev->of_node;

	/* read the led class name */
	pdata->rgb_name = DEFAULT_LM2755_CLASS_NAME;
	rc = of_property_read_string(np, "rgb-class-name",
		&pdata->rgb_name);
	if (rc && (rc != -EINVAL))
		dev_err(dev, "Unable to read rgb-name\n");

	/* read each channel presence and settings */
	for (i = 0; i < LM2755_NUM_LEDS; i++) {
		pdata->led_names[i] = NULL;
		rc = of_property_read_string(np, names[i],
			&pdata->led_names[i]);
		if (rc && (rc != -EINVAL))
			dev_err(dev, "Unable to read %s\n", names[i]);
	}

	rc = of_property_read_u32(np, "clock-mode", &temp_val);
	if (rc && (rc != -EINVAL))
		dev_err(dev, "Unable to read clock-mode\n");
	else if (rc != -EINVAL)
		pdata->clock_mode = (u8) temp_val;

	rc = of_property_read_u32(np, "charge-pump-mode", &temp_val);
	if (rc && (rc != -EINVAL))
		dev_err(dev, "Unable to read charge-pump-mode\n");
	else if (rc != -EINVAL)
		pdata->charge_pump_mode = (u8) temp_val;

	rc = of_property_read_u32(np, "min-tstep", &temp_val);
	if (rc && (rc != -EINVAL))
		dev_err(dev, "Unable to read min-tstep\n");
	else if (rc != -EINVAL)
		pdata->min_tstep = temp_val;

	rc = of_property_read_u32(np, "max-level", &temp_val);
	if (rc && (rc != -EINVAL))
		dev_err(dev, "Unable to read max-level\n");
	else if (rc != -EINVAL)
		pdata->max_level = (u8) temp_val;

	return 0;
}
#else
static int lm2755_parse_dt(struct device *dev,
			struct lm2755_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int __devinit lm2755_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	int ret;
	int error;
	struct lm2755_data *data;
	struct lm2755_platform_data *pdata = NULL;
	uint32_t min_tstep;

#ifdef CONFIG_OF
	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
			sizeof(struct lm2755_platform_data), GFP_KERNEL);
		if (!pdata) {
			dev_err(&client->dev, "Failed to allocate memory\n");
			return -ENOMEM;
		} else {
			data = devm_kzalloc(&client->dev,
				sizeof(*data), GFP_KERNEL);
			if (!data) {
				pr_err("unable to kzalloc lm2755_data\n");
				return -ENOMEM;
			}
		}

		error = lm2755_parse_dt(&client->dev, pdata);
		if (error)
			return error;
	}
#else
	/* We need platform data */
	if (!client->dev.platform_data) {
		pr_err("no platform data\n");
		return -EINVAL;
	}

	pdata = client->dev.platform_data;

#endif

	/* Check I2C functionality */
	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_BYTE_DATA)) {

		pr_err("I2C_FUNC_SMBUS_BYTE_DATA not supported\n");
		return -ENOTSUPP;
	}

	if (pdata->clock_mode == LM2755_CLOCK_EXT) {
		if (pdata->min_tstep == 0) {
			pr_err("invalid min_tstep 0 for external clock\n");
			return -EINVAL;
		} else {
			min_tstep = pdata->min_tstep;
		}
	} else {
		min_tstep = LM2755_MIN_TSTEP_INT * 1000;
	}

	/* Allocate our internal data structure */
	data = devm_kzalloc(&client->dev, sizeof(*data), GFP_KERNEL);
	if (!data) {
		pr_err("unable to kzalloc lm2755_data\n");
		return -ENOMEM;
	}

	i2c_set_clientdata(client, data);
	data->client = client;
	data->config = pdata->clock_mode | pdata->charge_pump_mode;
	/* Save min tstep for external clock */
	if (pdata->clock_mode == LM2755_CLOCK_EXT)
		data->min_tstep = min_tstep;

	lm2755_set_tsteps(data, min_tstep);
	pr_info("max_time = %d\n", data->max_time);

	data->max_level = pdata->max_level;
	if (data->max_level == 0 || data->max_level > LM2755_MAX_LEVEL)
		data->max_level = LM2755_MAX_LEVEL;

	/* Populate groups */
	data->rgb_group[LM2755_D1] = LM2755_D1_BASE;
	data->rgb_group[LM2755_D2] = LM2755_D2_BASE;
	data->rgb_group[LM2755_D3] = LM2755_D3_BASE;

	data->rgb_mask[LM2755_D1] = LM2755_D1_MASK;
	data->rgb_mask[LM2755_D2] = LM2755_D2_MASK;
	data->rgb_mask[LM2755_D3] = LM2755_D3_MASK;

	mutex_init(&data->lock);

	data->enable = pdata->enable;
	if (data->enable)
		data->enable(1);
	ret = lm2755_write(data->client, LM2755_REG_GENERAL, data->config);
	if (ret < 0) {
		pr_err("unable to write to the chip\n");
		goto fail1;
	}

	/* HIGH part of the brightness has power on value 1110b */
	/* LOW  part of the brightness has power on value 0000b */
	lm2755_write(data->client, LM2755_REG_D1_HIGH, 0);
	lm2755_write(data->client, LM2755_REG_D2_HIGH, 0);
	lm2755_write(data->client, LM2755_REG_D3_HIGH, 0);

	/* Register named LEDs */
	ret = lm2755_register_leds(data, pdata->led_names);
	if (ret < 0) {
		pr_err("unable to register leds\n");
		goto fail1;
	}

	/* Register RGB LED class if requested */
	if (pdata->rgb_name) {
		data->led_cdev.brightness_set = lm2755_rgb_brightness_set;
		data->led_cdev.brightness_get = lm2755_rgb_brightness_get;
		data->led_cdev.name = pdata->rgb_name;
		ret = led_classdev_register(&client->dev, &data->led_cdev);
		if (ret < 0) {
			pr_err("couldn't register \'%s\' LED class\n",
					pdata->rgb_name);
			goto fail2;
		}
		pr_info("registered \'%s\' LED class\n", pdata->rgb_name);

		INIT_WORK(&data->rgb_work, lm2755_rgb_work);

		ret = sysfs_create_group(&data->led_cdev.dev->kobj,
				&lm2755_attribute_group);
		if (ret < 0) {
			pr_err("couldn't register attribute sysfs group\n");
			goto fail3;
		}
	}

	pr_info("finished successfully\n");

	return 0;

fail3:
	led_classdev_unregister(&data->led_cdev);
fail2:
	lm2755_unregister_leds(data);
fail1:
	if (data->enable)
		data->enable(0);
	return ret;
}

static int lm2755_remove(struct i2c_client *client)
{
	struct lm2755_data *data = i2c_get_clientdata(client);
	int i;

	sysfs_remove_group(&data->led_cdev.dev->kobj,
			&lm2755_attribute_group);

	for (i = 0; i < LM2755_NUM_LEDS; i++) {
		if (data->leds[i].id < 0)
			continue;
		led_classdev_unregister(&data->leds[i].led_cdev);
		cancel_work_sync(&data->leds[i].work);
	}
	led_classdev_unregister(&data->led_cdev);
	cancel_work_sync(&data->rgb_work);

	if (data->enable)
		data->enable(0);
	return 0;
}

static const struct i2c_device_id lm2755_id[] = {
	{DRIVER_NAME, 0},
	{ }
};
MODULE_DEVICE_TABLE(i2c, lm2755_id);

#ifdef CONFIG_OF
static const struct of_device_id lm2755_match_table[] = {
	{ .compatible = "lm,lm2755" },
	{ },
	};
#endif

static struct i2c_driver lm2755_driver = {
	.driver = {
		.name	= "lm2755",
		.owner	= THIS_MODULE,
		.of_match_table = of_match_ptr(lm2755_match_table),
	},
	.probe		= lm2755_probe,
	.remove		= lm2755_remove,
	.id_table	= lm2755_id,
};

static int __init lm2755_init(void)
{
	int ret;

	ret = i2c_add_driver(&lm2755_driver);

	if (ret < 0)
		pr_err("failed to add lm2755 driver\n");

	return ret;
}

static void __exit lm2755_exit(void)
{
	i2c_del_driver(&lm2755_driver);
}

module_init(lm2755_init);
module_exit(lm2755_exit);

MODULE_AUTHOR("Alina Yakovleva");
MODULE_DESCRIPTION("LM2755 LED engine");
MODULE_LICENSE("GPL v2");
