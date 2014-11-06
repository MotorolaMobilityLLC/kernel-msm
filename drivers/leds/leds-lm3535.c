/*
 * Copyright (C) 2013 Motorola Mobility, Inc.
 * Author: Alina Yakovleva <qvdh43@motorola.com>
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * as published by the Free Software Foundation; either version 2
 * of the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA
 * 02110-1301, USA.
 */

// Linux  driver for    LM3535 display backlight

#include <linux/module.h>
#include <linux/init.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/interrupt.h>
#include <linux/hrtimer.h>
#include <linux/platform_device.h>
#include <linux/miscdevice.h>
#include <linux/i2c.h>
#include <linux/delay.h>
#include <asm/io.h>
#include <linux/clk.h>
#include <linux/irq.h>
#include <linux/sysfs.h>
#include <linux/hwmon-sysfs.h>
#include <linux/leds.h>
#include <linux/mutex.h>
#include <linux/debugfs.h>
#include <linux/list.h>
#include <asm/gpio.h>
#include <asm/io.h>
#include <asm/uaccess.h>
#include "als.h"
#undef CONFIG_HAS_EARLYSUSPEND
#ifdef  CONFIG_HAS_EARLYSUSPEND
#include <linux/earlysuspend.h>
#endif
#ifdef CONFIG_LM3535_ESD_RECOVERY
#include <mot/esd_poll.h>
#endif /* CONFIG_LM3535_ESD_RECOVERY */
#ifdef CONFIG_WAKEUP_SOURCE_NOTIFY
#include <linux/notifier.h>
#include <linux/wakeup_source_notify.h>
#define MIN_DOCK_BVALUE		36
#include <linux/m4sensorhub.h>
#include <linux/m4sensorhub/MemMapUserSettings.h>
#endif

#define MODULE_NAME "leds_lm3535"

/******************************************************************************
 *  LM3535 registers
 ******************************************************************************/
#define LM3535_ENABLE_REG                0x10
#define LM3535_CONFIG_REG                0x20
#define LM3535_OPTIONS_REG               0x30
#define LM3535_ALS_REG                   0x40
#define LM3535_ALS_CTRL_REG              0x50
#define LM3535_ALS_RESISTOR_REG          0x51
#ifdef CONFIG_LM3535_BUTTON_BL
#define LM3535_ALS_SELECT_REG            0x52
#endif
#define LM3535_BRIGHTNESS_CTRL_REG_A     0xA0
#define LM3535_BRIGHTNESS_CTRL_REG_B     0xB0
#define LM3535_BRIGHTNESS_CTRL_REG_C     0xC0
#define LM3535_ALS_ZB0_REG               0X60
#define LM3535_ALS_ZB1_REG               0X61
#define LM3535_ALS_ZB2_REG               0X62
#define LM3535_ALS_ZB3_REG               0X63
#define LM3535_ALS_Z0T_REG               0X70
#define LM3535_ALS_Z1T_REG               0X71
#define LM3535_ALS_Z2T_REG               0X72
#define LM3535_ALS_Z3T_REG               0X73
#define LM3535_ALS_Z4T_REG               0X74
#define LM3535_TRIM_REG                  0xD0

#define ALS_FLAG_MASK 0x08
#define ALS_ZONE_MASK 0x07
#define ALS_NO_ZONE   5
#define LM3535_TRIM_VALUE 0x68

#define LM3535_LED_MAX 0x7F  // Max brightness value supported by LM3535

#define LM3535_HWEN_GPIO	35

/* Final revision CONFIG value */
#define CONFIG_VALUE 0x4C
#define CONFIG_VALUE_NO_ALS 0x0C

/* ALS Averaging time */
#define ALS_AVERAGING 0x50  // 1600ms, needs 2 ave. periods

#define MAX_AMBIENT_BACKLIGHT 36

/* Zone boundaries */
static unsigned als_zb[] = {0x04, 0x42, 0x96, 0xFF};
module_param_array(als_zb, uint, NULL, 0644);
static unsigned resistor_value = 0x30;
module_param(resistor_value, uint, 0644);
static unsigned pwm_value = 0x1;
module_param(pwm_value, uint, 0644);

static unsigned als_sleep = 350;
module_param(als_sleep, uint, 0644);
static unsigned ramp_time = 200000;
module_param(ramp_time, uint, 0644);
enum {
    TRACE_SUSPEND = 0x1,
    TRACE_ALS = 0x2,
    TRACE_BRIGHTNESS = 0x4,
    TRACE_WRITE = 0x8,
    TRACE_EVENT = 0x10,
};
unsigned do_trace; /* = TRACE_ALS | TRACE_SUSPEND | TRACE_BRIGHTNESS;*/
module_param(do_trace, uint, 0644);

#define printk_write(fmt,args...) if (do_trace & TRACE_WRITE) printk(KERN_INFO fmt, ##args)
#define printk_br(fmt,args...) if (do_trace & TRACE_BRIGHTNESS) printk(KERN_INFO fmt, ##args)
#define printk_als(fmt,args...) if (do_trace & TRACE_ALS) printk(KERN_INFO fmt, ##args)
#define printk_suspend(fmt,args...) if (do_trace & TRACE_SUSPEND) printk(KERN_INFO fmt, ##args)
#define printk_event(fmt,args...) if (do_trace & TRACE_EVENT) printk(KERN_INFO fmt, ##args)

/* ALS callbacks */
static DEFINE_MUTEX(als_cb_mutex);
static LIST_HEAD(als_callbacks);
struct als_callback {
    als_cb cb;
    uint32_t cookie;
    struct list_head entry;
};

struct lm3535_options_register_r1 {
    int rs      : 2;       // Ramp step
    int gt      : 2;       // Gain transition filter
    int rev     : 2;
};
/* Ramp times for Rev1 in microseconds */
static unsigned int lm3535_ramp_r1[] = {51, 13000, 26000, 52000};

struct lm3535_options_register_r2 {
    int rs_down : 2;
    int rs_up : 2;
    int gt : 2;
};
/* Ramp times for Rev2 in microseconds */
static unsigned int lm3535_ramp_r2[] = {6, 6000, 12000, 24000};

struct lm3535_options_register_r3 {
    int rs_down : 3;
    int rs_up : 3;
    int gt : 2;
};
/* Ramp times for Rev3 in microseconds */
static unsigned int lm3535_ramp_r3[] =
    {6, 770, 1500, 3000, 6000, 12000, 25000, 50000};

static void lm3535_send_als_event (int zone);
static char *reg_name (int reg);
static int lm3535_configure (void);
static void lm3535_call_als_callbacks (unsigned old_zone, unsigned zone);
static void lm3535_set_options_r1 (uint8_t *buf, unsigned ramp);
static void lm3535_set_options_r2 (uint8_t *buf, unsigned ramp);
static void lm3535_set_options_r3 (uint8_t *buf, unsigned ramp);
static int lm3535_write_reg (unsigned reg, uint8_t value, const char *caller);
static int lm3535_read_reg (unsigned reg, uint8_t *value);
static int lm3535_set_ramp (struct i2c_client *client,
    unsigned int on, unsigned int nsteps, unsigned int *rtime);
static int lm3535_enable(struct i2c_client *client, unsigned int on);
static int lm3535_probe(struct i2c_client *client,
    const struct i2c_device_id *id);
static int lm3535_setup (struct i2c_client *client);
static int lm3535_remove (struct i2c_client *client);
static void lm3535_work_func (struct work_struct *work);
static void lm3535_allow_als_work_func(struct work_struct *work);
static irqreturn_t lm3535_irq_handler (int irq, void *dev_id);
static void lm3535_brightness_set(struct led_classdev *led_cdev,
                enum led_brightness value);

#ifdef	CONFIG_HAS_AMBIENTMODE
static void lm3535_brightness_set_raw_als(struct led_classdev *led_cdev,
				unsigned int value);
#endif
/* static unsigned lm3535_read_als_zone (void); */
#ifdef CONFIG_PM
#ifdef CONFIG_HAS_EARLYSUSPEND
static void lm3535_early_suspend (struct early_suspend *h);
static void lm3535_late_resume (struct early_suspend *h);
static struct early_suspend early_suspend_data = {
    .level = EARLY_SUSPEND_LEVEL_BLANK_SCREEN + 5,
    .suspend = lm3535_early_suspend,
    .resume = lm3535_late_resume,
};
#endif
#if defined(CONFIG_HAS_EARLYSUSPEND) || !defined(CONFIG_HAS_AMBIENTMODE)
static int lm3535_suspend (struct i2c_client *client, pm_message_t mesg);
static int lm3535_resume (struct i2c_client *client);
#endif
#endif

static void (*lm3535_set_options_f)(uint8_t *buf, unsigned ramp) =
    lm3535_set_options_r1;
static unsigned int *lm3535_ramp = lm3535_ramp_r1;

/* LED class struct */
static struct led_classdev lm3535_led = {
    .name = "lcd-backlight",
    .brightness_set = lm3535_brightness_set,
#ifdef	CONFIG_HAS_AMBIENTMODE
	.brightness_set_raw_als = lm3535_brightness_set_raw_als,
#endif
};

/* LED class struct for no ramping */
static struct led_classdev lm3535_led_noramp = {
    .name = "lcd-nr-backlight",
    .brightness_set = lm3535_brightness_set,
#ifdef	CONFIG_HAS_AMBIENTMODE
	.brightness_set_raw_als = lm3535_brightness_set_raw_als,
#endif
};
#ifdef CONFIG_LM3535_BUTTON_BL
static struct led_classdev lm3535_led_button = {
    .name = "button-backlight",
    .brightness_set = lm3535_button_brightness_set,
#ifdef	CONFIG_HAS_AMBIENTMODE
	.brightness_set_raw_als = lm3535_brightness_set_raw_als,
#endif
};
#endif

static const struct of_device_id of_lm3535_match[] = {
	{ .compatible = "ti,lm3535", },
	{},
};

static const struct i2c_device_id lm3535_id[] = {
    { "lm3535", 0 },
    { }
};

#define LM3535_NUM_ZONES 6
struct lm3535 {
    uint16_t addr;
    struct i2c_client *client;
    struct input_dev *idev;
    unsigned initialized;
    unsigned enabled;
    int use_irq;
    int revision;
    int nramp;
    atomic_t als_zone;     // Current ALS zone
    atomic_t bright_zone;  // Current brightness zone, diff. from ALS
    atomic_t use_als;      // Whether to use ALS
    atomic_t in_suspend;   // Whether the driver is in TCMD SUSPEND mode
    atomic_t do_als_config; // Whether to configure ALS averaging
    unsigned bvalue;       // Current brightness register value
    unsigned saved_bvalue; // Brightness before TCMD SUSPEND
    //struct hrtimer timer;
    struct work_struct  work;
	struct delayed_work als_delayed_work;
	int prevent_als_read;	/* Whether to prevent als reads for a time */
#ifdef CONFIG_WAKEUP_SOURCE_NOTIFY
	atomic_t docked;
	struct notifier_block dock_nb;
#endif
};
static DEFINE_MUTEX(lm3535_mutex);

static struct lm3535 lm3535_data = {
    .nramp = 4,
    .bvalue = 0x79,
};

#if defined CONFIG_PM_SLEEP

static int lm3535_pm_suspend(struct device *dev)
{
	cancel_delayed_work_sync(&lm3535_data.als_delayed_work);
	lm3535_data.prevent_als_read = 0;

	return 0;
}

static SIMPLE_DEV_PM_OPS(lm3535_pm_ops, lm3535_pm_suspend, NULL);
#define LM3535_PM_OPS (&lm3535_pm_ops)

#else
#define LM3535_PM_OPS NULL
#endif

/* This is the I2C driver that will be inserted */
static struct i2c_driver lm3535_driver = {
	.driver = {
		.name   = "lm3535",
		.of_match_table = of_match_ptr(of_lm3535_match),
		.pm = LM3535_PM_OPS,
	},
	.id_table = lm3535_id,
	.probe = lm3535_probe,
	.remove  = lm3535_remove,
#if !defined(CONFIG_HAS_EARLYSUSPEND) && !defined(CONFIG_HAS_AMBIENTMODE)
	.suspend    = lm3535_suspend,
	.resume     = lm3535_resume,
#endif
};

#if 0
unsigned lm3535_read_als_zone (void)
{
    uint8_t reg;
    int ret;

    if (lm3535_data.revision <= 1) {
        printk (KERN_ERR "%s: early revison, setting zone to 5 (no ALS)\n",
           __FUNCTION__);
        atomic_set (lm3535_data.als_zone, ALS_NO_ZONE);
        return ALS_NO_ZONE;
    }
    lm3535_read_reg (LM3535_CONFIG_REG, &reg);
    if (reg & 0x50) {
        ret = lm3535_read_reg (LM3535_ALS_REG, &reg);
        lm3535_data.als_zone = reg & ALS_ZONE_MASK;
        return lm3535_data.als_zone;
    } else {
        printk (KERN_ERR "%s: ALS is not enabled, CONFIG=0x%x, zone=5\n",
            __FUNCTION__, reg);
        lm3535_data.als_zone = 5;
        return 5;
    }
}
#endif
int lm3535_register_als_callback(als_cb func, uint32_t cookie)
{
    struct als_callback *c;

    //printk (KERN_INFO "%s: enter\n", __FUNCTION__);
    c = kzalloc (sizeof (struct als_callback), GFP_KERNEL);
    if (c == NULL) {
        printk (KERN_ERR "%s: unable to register ALS callback: kzalloc\n",
            __FUNCTION__);
        return -ENOMEM;
    }
    c->cb = func;
    c->cookie = cookie;
    mutex_lock (&als_cb_mutex);
    list_add (&c->entry, &als_callbacks);
    mutex_unlock (&als_cb_mutex);
    return 0;
}
EXPORT_SYMBOL(lm3535_register_als_callback);

void lm3535_unregister_als_callback (als_cb func)
{
    struct als_callback *c;

    if (!lm3535_data.initialized) {
        printk (KERN_ERR "%s: not initialized\n", __FUNCTION__);
        return;
    }
    printk (KERN_INFO "%s: enter\n", __FUNCTION__);
    mutex_lock (&als_cb_mutex);
    list_for_each_entry(c, &als_callbacks, entry) {
        if (c->cb == func) {
            list_del (&c->entry);
            kfree(c);
            mutex_unlock (&als_cb_mutex);
            return;
        }
    }
    mutex_unlock (&als_cb_mutex);
    printk (KERN_ERR "%s: callback 0x%x not found\n",
        __FUNCTION__, (unsigned int)func);
}
EXPORT_SYMBOL(lm3535_unregister_als_callback);

unsigned lm3535_als_is_dark (void)
{
    unsigned zone;

    zone = atomic_read (&lm3535_data.als_zone);
    printk (KERN_ERR "%s: enter, zone = %d\n",
        __FUNCTION__, zone);
    if (zone == 0 || zone == 5)
        return 1;
    else
        return 0;
}
EXPORT_SYMBOL(lm3535_als_is_dark);

static int lm3535_read_reg (unsigned reg, uint8_t *value)
{
    struct i2c_client *client = lm3535_data.client;
    uint8_t buf[1];
    int ret = 0;

    if (!value)
        return -EINVAL;
    buf[0] = reg;
    ret = i2c_master_send (client, buf, 1);
    if (ret > 0) {
        msleep_interruptible (1);
        ret = i2c_master_recv (client, buf, 1);
        if (ret > 0)
            *value = buf[0];
    }
    return ret;
}

static int lm3535_write_reg (unsigned reg, uint8_t value, const char *caller)
{
    uint8_t buf[2] = {reg, value};
    int ret = 0;

    printk_write ("%s: writing 0x%X to reg 0x%X (%s) at addr 0x%X\n",
        caller, buf[1], buf[0], reg_name (reg), lm3535_data.client->addr);
    ret = i2c_master_send (lm3535_data.client, buf, 2);
    if (ret < 0)
        printk (KERN_ERR "%s: i2c_master_send error %d\n",
            caller, ret);
    return ret;
}

#if defined(CONFIG_WAKEUP_SOURCE_NOTIFY) || defined(CONFIG_HAS_AMBIENTMODE)
/* RAW ALS coefficients */
static int raw_als_z0[] = {-44773, 6893285, 23168221};
static int raw_als_z1[] = {-141, 358959, 274671182};
static int raw_als_z2[] = {-74, 529533, 37559974};
#endif

/* ALS Coefficients */
static long als_z0[] =          {24, -15295, 4123276, 634967215};
module_param_array(als_z0, long, NULL, 0644);

static long als_z1[] =          {24, -14596, 3987380, 659307205};
module_param_array(als_z1, long, NULL, 0644);

static long als_z2[] =          {0, -5700, 2553000, 953424200};
module_param_array(als_z2, long, NULL, 0644);

static long als_z3[] =          {0, -5700, 2553000, 953424200};
module_param_array(als_z3, long, NULL, 0644);

static long als_z4[] =          {0, -5700, 2553000, 953424200};
module_param_array(als_z4, long, NULL, 0644);

static unsigned long als_denom = 10000000;
module_param(als_denom, ulong, 0644);

static unsigned dim_values[] = {0x03, 0x30, 0x50, 0x50, 0x50};
module_param_array(dim_values, uint, NULL, 0644);

#if defined(CONFIG_WAKEUP_SOURCE_NOTIFY) || defined(CONFIG_HAS_AMBIENTMODE)
static int abs_als_to_backlight(unsigned int als_value)
{
	int backlight;
	int als = als_value;
	/*
	Clamp ALS to max 2330.
	for ALS 0 to 86
	Y=(-44772.9*X^2+6893284.5*X+23168220.5)

	for ALS 87 to 999
	Y=(-141.2*X^2+358959.2*X+274671182)

	for ALS 1000 to 2330
	Y=(-74.1*X^2+529533.4*X+37559974.7)

	For all ranges, Y = Y / 10000000;
	This is the final backlight value
	*/
	if (als < 86) {
		backlight = (int)(((raw_als_z0[0] * als * als) +
					(raw_als_z0[1] * als)
					+ raw_als_z0[2])/10000000);
	} else if (als > 87 && als < 999) {
		backlight = (int)(((raw_als_z1[0] * als * als) +
					(raw_als_z1[1] * als) +
					raw_als_z1[2])/10000000);
	} else {
		if (als > 2330)
			als = 2330;

		backlight = (int)(((raw_als_z2[0] * als * als) +
					(raw_als_z2[1] * als) +
					raw_als_z2[2])/10000000);
	}

	/* Cap ambient mode backlight to 36*/
	if (backlight > MAX_AMBIENT_BACKLIGHT)
		backlight = MAX_AMBIENT_BACKLIGHT;
	pr_info("ALS: %d, backlight: %d\n", als, backlight);
	return backlight;
}
#endif
/* Convert slider value into LM3535 register value */
static uint8_t lm3535_convert_value (unsigned value, unsigned zone)
{
    uint8_t reg;
    uint32_t res;
#ifdef CONFIG_WAKEUP_SOURCE_NOTIFY
	struct m4sensorhub_data *m4sensorhub;
	int size;
	static int ambient_als_backlight;
	uint16_t als;
	bool adjust_als = false;
#endif

    if (!value)
        return 0;

    if (atomic_read (&lm3535_data.in_suspend)) {
        printk_br ("%s: in TCMD SUSPEND, returning 0x%x\n",
            __FUNCTION__, value/2);
        return value/2;
    }
    switch (zone) {
        case 0:
            if (value == 1)
                res = dim_values[0];   // DIM value
            else
                res =  als_z0[0] * value * value * value
                      +als_z0[1] * value * value
                      +als_z0[2] * value
                      +als_z0[3];
            break;
        case 1:
            if (value == 1)
                res = dim_values[1];   // DIM value
            else
                res =  als_z1[0] * value * value * value
                      +als_z1[1] * value * value
                      +als_z1[2] * value
                      +als_z1[3];
            break;
        case 2:
            if (value == 1)
                res = dim_values[2];   // DIM value
            else
                res =  als_z2[0] * value * value * value
                      +als_z2[1] * value * value
                      +als_z2[2] * value
                      +als_z2[3];
            break;
        case 3:
            if (value == 1)
                res = dim_values[3];   // DIM value
            else
                res =  als_z3[0] * value * value * value
                      +als_z3[1] * value * value
                      +als_z3[2] * value
                      +als_z3[3];
            break;
        case 4:
        default:
            if (value == 1)
                res = dim_values[4];   // DIM value
            else
                res =  als_z4[0] * value * value * value
                      +als_z4[1] * value * value
                      +als_z4[2] * value
                      +als_z4[3];
            break;
    }
    if (value == 1)
        reg = res;
    else
        reg = res / als_denom;

#ifdef CONFIG_WAKEUP_SOURCE_NOTIFY
	if (!lm3535_data.prevent_als_read) {
		/* make sure this is atleast as high as corresponding ambient
		 * mode value for current ALS condition */
		m4sensorhub = m4sensorhub_client_get_drvdata();
		size = m4sensorhub_reg_getsize(m4sensorhub,
					M4SH_REG_LIGHTSENSOR_SIGNAL);
		if (size != sizeof(als)) {
			pr_err("can't get M4 reg size for ALS\n");
			ambient_als_backlight = 0;
		} else if (size != m4sensorhub_reg_read(m4sensorhub,
						M4SH_REG_LIGHTSENSOR_SIGNAL,
						(char *)&als)) {
			pr_err("error reading M4 ALS value\n");
			ambient_als_backlight = 0;
		} else {
			adjust_als = true;
			/* prevent als reads for next 500 ms */
			lm3535_data.prevent_als_read = 1;
			schedule_delayed_work(&lm3535_data.als_delayed_work,
					      msecs_to_jiffies(500));
		}
	} else if (ambient_als_backlight > reg) {
		/* If valid, use previously read als value */
		reg = ambient_als_backlight;
	}

	if (adjust_als) {
		ambient_als_backlight = abs_als_to_backlight(als);

		if (ambient_als_backlight > reg)
			reg = ambient_als_backlight;
	}
#endif

	printk_br(KERN_INFO "%s: v=%d, z=%d, res=0x%x, reg=0x%x\n",
		__func__, value, zone, res, reg);

    return reg;
}

#ifdef CONFIG_WAKEUP_SOURCE_NOTIFY
static int lm3535_dock_notifier(struct notifier_block *self,
				unsigned long action, void *dev)
{
	switch (action) {
	case DISPLAY_WAKE_EVENT_DOCKON:
		atomic_set(&lm3535_data.docked, 1);
		break;
	case DISPLAY_WAKE_EVENT_DOCKOFF:
		atomic_set(&lm3535_data.docked, 0);
		break;
	}

	return NOTIFY_OK;
}
#endif /* CONFIG_WAKEUP_SOURCE_NOTIFY */

#ifdef	CONFIG_HAS_AMBIENTMODE
struct led_classdev *led_get_default_dev(void)
{
	return &lm3535_led;
}
EXPORT_SYMBOL(led_get_default_dev);



static void lm3535_brightness_set_raw_als(struct led_classdev *led_cdev,
				unsigned int value)
{
	struct i2c_client *client = lm3535_data.client;
	int ret;
	unsigned breg = LM3535_BRIGHTNESS_CTRL_REG_A;

	int bvalue = abs_als_to_backlight(value);
	mutex_lock(&lm3535_mutex);

	if (!lm3535_data.enabled && value != 0)
		lm3535_enable(client, 1);

	ret = lm3535_write_reg(breg, bvalue, __func__);
	lm3535_data.bvalue = bvalue;
	led_cdev->brightness = bvalue;

	mutex_unlock(&lm3535_mutex);
}
#endif
static void lm3535_brightness_set (struct led_classdev *led_cdev,
                enum led_brightness value)
{
    struct i2c_client *client = lm3535_data.client;
    int ret, nsteps;
    unsigned int total_time = 0;
    unsigned breg = LM3535_BRIGHTNESS_CTRL_REG_A;
    unsigned bright_zone;
    unsigned bvalue;
    unsigned do_ramp = 1;

    printk_br ("%s: %s, 0x%x (%d)\n", __FUNCTION__,
        led_cdev->name, value, value);
    if (!lm3535_data.initialized) {
        printk (KERN_ERR "%s: not initialized\n", __FUNCTION__);
        return;
    }
    if (strstr (led_cdev->name, "nr"))
        do_ramp = 0;

    bright_zone = atomic_read (&lm3535_data.bright_zone);
    mutex_lock (&lm3535_mutex);
    if (value == -1)
        value = led_cdev->brightness; /* Special case for ALS adjustment */
    else if ((value > 0) && (value < 5))
        value = 0; /* Special case for turn off */
    else if ((value >= 5) && (value < 10))
        value = 1; /* Special case for dim */

    if ((value == 0) && (!lm3535_data.enabled)) {
        /* If LED already disabled, we don't need to do anything */
        mutex_unlock(&lm3535_mutex);
        return;
    }

#ifdef CONFIG_LM3535_ESD_RECOVERY
    if (value == LED_OFF && esd_polling)
    {
        esd_poll_stop(lm3535_check_esd);
        esd_polling = 0;
    }
#endif /* CONFIG_LM3535_ESD_RECOVERY */
    if (!lm3535_data.enabled && value != 0)
        lm3535_enable(client, 1);

    /* Calculate brightness value for each zone relative to its cap */
    bvalue = lm3535_convert_value (value, bright_zone);
#ifdef CONFIG_WAKEUP_SOURCE_NOTIFY
	if (atomic_read(&lm3535_data.docked) && (bvalue < MIN_DOCK_BVALUE))
		bvalue = MIN_DOCK_BVALUE; /* hard code for dock mode */
#endif /* CONFIG_WAKEUP_SOURCE_NOTIFY */

    /* Calculate number of steps for ramping */
    nsteps = bvalue - lm3535_data.bvalue;
    if (nsteps < 0)
        nsteps = nsteps * (-1);

    lm3535_set_ramp (client, do_ramp, nsteps, &total_time);

    printk_br ("%s: zone %d, 0x%x => 0x%x, %d steps, ramp time %dus\n",
        __FUNCTION__, bright_zone,
        lm3535_data.bvalue, bvalue, nsteps, total_time);

    /* Write to each zone brightness register so that when it jumps into
     * the next zone the value is adjusted automatically
     */
    ret = lm3535_write_reg (breg, bvalue, __FUNCTION__);
    lm3535_data.bvalue = bvalue;

    if (value == 0) {
        /* Disable everything */
        if (do_ramp) {
            /* Wait for ramping to finish */
            udelay (total_time);
        }
        lm3535_enable(client, 0);
    }

#ifdef CONFIG_LM3535_ESD_RECOVERY
    if ((value > 0) && (!esd_polling))
    {
        esd_poll_start(lm3535_check_esd, 0);
        esd_polling = 1;
    }
#endif /* CONFIG_LM3535_ESD_RECOVERY */


    mutex_unlock (&lm3535_mutex);
}

#ifdef CONFIG_LM3535_BUTTON_BL
static void lm3535_button_brightness_set (struct led_classdev *led_cdev,
                enum led_brightness value)
{
    int ret;
    unsigned breg = LM3535_BRIGHTNESS_CTRL_REG_C;
    struct i2c_client *client = lm3535_data.client;

    printk_br ("%s: %s, 0x%x (%d)\n", __FUNCTION__,
        led_cdev->name, value, value);

    mutex_lock (&lm3535_mutex);

    if (!lm3535_data.button_enabled && value != 0) {
        lm3535_enable (client, lm3535_data.enabled, 1);
    }

    ret = lm3535_write_reg (breg, 0xF8, __FUNCTION__); // Lowest setting

    if (value == 0)
        lm3535_enable(client, 0);

    mutex_unlock(&lm3535_mutex);
}
#endif

static int lm3535_als_open (struct inode *inode, struct file *file)
{
    if (!lm3535_data.initialized)
        return -ENODEV;

    return 0;
}

static int lm3535_als_release (struct inode *inode, struct file *file)
{
    return 0;
}
#define CMD_LEN 5
static ssize_t lm3535_als_write (struct file *fp, const char __user *buf,
    size_t count, loff_t *pos)
{
    unsigned char cmd[CMD_LEN];
    int len;
    uint8_t value;
    unsigned old_zone;

    if (count < 1)
        return 0;

    len = count > CMD_LEN-1 ? CMD_LEN-1 : count;

    if (copy_from_user (cmd, buf, len))
        return -EFAULT;

    if (lm3535_data.revision <= 1)
        return -EFAULT;

    cmd[len] = '\0';
    if (cmd[len-1] == '\n') {
        cmd[len-1] = '\0';
        len--;
    }
    if (!strcmp (cmd, "1")) {
        printk (KERN_INFO "%s: enabling ALS\n", __FUNCTION__);
        value = CONFIG_VALUE | 0x80;
        mutex_lock (&lm3535_mutex);
        atomic_set (&lm3535_data.use_als, 1);
        /* No need to change ALS zone; interrupt handler will do it */
        lm3535_write_reg (LM3535_CONFIG_REG, value, __FUNCTION__);
        mutex_unlock (&lm3535_mutex);
    } else if (!strcmp (cmd, "0")) {
        printk (KERN_INFO "%s: disabling ALS\n", __FUNCTION__);
        value = CONFIG_VALUE_NO_ALS;
        mutex_lock (&lm3535_mutex);
        old_zone = atomic_read (&lm3535_data.als_zone);
        lm3535_write_reg (LM3535_CONFIG_REG, value, __FUNCTION__);
        atomic_set (&lm3535_data.use_als, 0);
        atomic_set (&lm3535_data.als_zone, ALS_NO_ZONE);
        mutex_unlock (&lm3535_mutex);
        if (atomic_read (&lm3535_data.bright_zone) < 2) {
            atomic_set (&lm3535_data.bright_zone, ALS_NO_ZONE);
            printk_als ("%s: ALS canceled; changing brightness\n",
                __FUNCTION__);
            /* Adjust brightness */
            lm3535_brightness_set (&lm3535_led, -1);
        } else {
            atomic_set (&lm3535_data.bright_zone, ALS_NO_ZONE);
        }
        lm3535_call_als_callbacks (old_zone, 0);
        lm3535_send_als_event (0);
    } else {
        printk (KERN_ERR "%s: invalid command %s\n", __FUNCTION__, cmd);
        return -EFAULT;
    }

    return count;
}

static ssize_t lm3535_als_read (struct file *file, char __user *buf,
    size_t count, loff_t *ppos)
{
    char z[23];

    if (file->private_data)
        return 0;

    if (!atomic_read (&lm3535_data.use_als)) {
        sprintf (z, "%d\n", ALS_NO_ZONE);
    } else {
        sprintf (z, "%d %d\n",
            atomic_read (&lm3535_data.als_zone),
            atomic_read (&lm3535_data.bright_zone));
    }
    if (copy_to_user (buf, z, strlen (z)))
        return -EFAULT;

    file->private_data = (void *)1;
    return strlen (z);
}

static const struct file_operations als_fops = {
    .owner      = THIS_MODULE,
    .read       = lm3535_als_read,
    .write      = lm3535_als_write,
    .open       = lm3535_als_open,
    .release    = lm3535_als_release,
};

static struct miscdevice als_miscdev = {
    .minor      = MISC_DYNAMIC_MINOR,
    .name       = "als",
    .fops       = &als_fops,
};

static ssize_t lm3535_suspend_show (struct device *dev,
                      struct device_attribute *attr, char *buf)
{
    sprintf (buf, "%d\n", atomic_read (&lm3535_data.in_suspend));
    return strlen(buf)+1;
}

static ssize_t lm3535_suspend_store (struct device *dev,
                     struct device_attribute *attr,
                     const char *buf, size_t size)
{
    unsigned value = 0;

    if (!buf || size == 0) {
        printk (KERN_ERR "%s: invalid command\n", __FUNCTION__);
        return -EINVAL;
    }

    sscanf (buf, "%d", &value);
    if (value) {
        printk (KERN_INFO "%s: going into TCMD SUSPEND mode\n",
            __FUNCTION__);
        atomic_set (&lm3535_data.in_suspend, 1);
        lm3535_data.saved_bvalue = lm3535_led.brightness;
        lm3535_led.brightness = 255;
    } else {
        printk (KERN_INFO "%s: exiting TCMD SUSPEND mode\n",
            __FUNCTION__);
        atomic_set (&lm3535_data.in_suspend, 0);
        lm3535_led.brightness = lm3535_data.saved_bvalue;
    }
    /* Adjust brightness */
    lm3535_brightness_set (&lm3535_led, -1);
    return size;
}
static DEVICE_ATTR(suspend, 0644, lm3535_suspend_show, lm3535_suspend_store);

/* This function is called by i2c_probe */
static int lm3535_probe (struct i2c_client *client,
    const struct i2c_device_id *id)
{
    int ret = 0;
    unsigned long request_flags =  IRQF_TRIGGER_LOW;

    gpio_request(LM3535_HWEN_GPIO, "LM3535 HWEN");
    gpio_direction_output(LM3535_HWEN_GPIO, 1);
    msleep(1);

    /* We should be able to read and write byte data */
    if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
        printk (KERN_ERR "%s: I2C_FUNC_I2C not supported\n",
            __FUNCTION__);
        return -ENOTSUPP;
    }

    lm3535_data.client = client;
    i2c_set_clientdata (client, &lm3535_data);

    /* Initialize chip */
    lm3535_setup (lm3535_data.client);

    /* Initialize interrupts */
    if (lm3535_data.revision > 1) {
        INIT_WORK(&lm3535_data.work, lm3535_work_func);
        if (client->irq) {
            ret = request_irq (client->irq, lm3535_irq_handler, request_flags,
                "lm3535", &lm3535_data);

            if (ret == 0) {
                lm3535_data.use_irq = 1;
                ret = irq_set_irq_wake (client->irq, 1);
            } else {
                printk (KERN_ERR "request_irq %d for lm3535 failed: %d\n",
                    client->irq, ret);
                free_irq (client->irq, &lm3535_data);
                lm3535_data.use_irq = 0;
            }
        }
    }

    /* Register LED class */
    ret = led_classdev_register (&client->adapter->dev, &lm3535_led);
    if (ret) {
        printk (KERN_ERR "%s: led_classdev_register %s failed: %d\n",
           __FUNCTION__, lm3535_led.name, ret);
        return ret;
    }

    /* Register LED class for no ramping */
    ret = led_classdev_register (&client->adapter->dev, &lm3535_led_noramp);
    if (ret) {
        printk (KERN_ERR "%s: led_classdev_register %s failed: %d\n",
           __FUNCTION__, lm3535_led.name, ret);
    }
    if ((ret = misc_register (&als_miscdev))) {
        printk (KERN_ERR "%s: misc_register failed, error %d\n",
            __FUNCTION__, ret);
        led_classdev_unregister (&lm3535_led);
    led_classdev_unregister(&lm3535_led_noramp);
        return ret;
    }

    atomic_set (&lm3535_data.in_suspend, 0);
    ret = device_create_file (lm3535_led.dev, &dev_attr_suspend);
    if (ret) {
      printk (KERN_ERR "%s: unable to create suspend device file for %s: %d\n",
            __FUNCTION__, lm3535_led.name, ret);
        led_classdev_unregister (&lm3535_led);
        led_classdev_unregister(&lm3535_led_noramp);
        misc_deregister (&als_miscdev);
        return ret;
    }
    dev_set_drvdata (lm3535_led.dev, &lm3535_led);
#if 0
    lm3535_data.idev = input_allocate_device();
    if (lm3535_data.idev == NULL) {
      printk (KERN_ERR "%s: unable to allocate input device file for als\n",
            __FUNCTION__);
        led_classdev_unregister (&lm3535_led);
        led_classdev_unregister(&lm3535_led_noramp);
        misc_deregister (&als_miscdev);
        device_remove_file (lm3535_led.dev, &dev_attr_suspend);
        return -ENOMEM;
    }
	lm3535_data.idev->name = "als";
	input_set_capability(lm3535_data.idev, EV_MSC, MSC_RAW);
	input_set_capability(lm3535_data.idev, EV_LED, LED_MISC);
    ret = input_register_device (lm3535_data.idev);
    if (ret) {
      printk (KERN_ERR "%s: unable to register input device file for als: %d\n",
            __FUNCTION__, ret);
        led_classdev_unregister (&lm3535_led);
        led_classdev_unregister(&lm3535_led_noramp);
        misc_deregister (&als_miscdev);
        device_remove_file (lm3535_led.dev, &dev_attr_suspend);
        input_free_device (lm3535_data.idev);
        return ret;
    }
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
    register_early_suspend (&early_suspend_data);
#endif

	lm3535_led.brightness = 84;
	lm3535_led_noramp.brightness = 84;
	/* lm3535_brightness_set (&lm3535_led_noramp, 255); */
	lm3535_write_reg (LM3535_BRIGHTNESS_CTRL_REG_A, 87, __func__);
	lm3535_data.initialized = 1;
#ifdef CONFIG_WAKEUP_SOURCE_NOTIFY
	atomic_set(&lm3535_data.docked, 0);
	lm3535_data.dock_nb.notifier_call = lm3535_dock_notifier;
	wakeup_source_register_notify(&lm3535_data.dock_nb);
#endif /* CONFIG_WAKEUP_SOURCE_NOTIFY */

	INIT_DELAYED_WORK(&lm3535_data.als_delayed_work,
			  lm3535_allow_als_work_func);

    return 0;
}

static irqreturn_t lm3535_irq_handler (int irq, void *dev_id)
{
    struct lm3535 *data_ptr = (struct lm3535 *)dev_id;

    pr_debug ("%s: got an interrupt %d\n", __FUNCTION__, irq);

    disable_irq (irq);
    schedule_work (&data_ptr->work);

    return IRQ_HANDLED;
}

static void lm3535_send_als_event (int zone)
{
#ifdef CONFIG_ALS_UEVENT
    char event_string[20];
    char *envp[] = {event_string, NULL};
    int ret;

    sprintf (event_string, "ALS_ZONE=%d", zone);
    ret = kobject_uevent_env (&als_miscdev.this_device->kobj,
        KOBJ_CHANGE, envp);
    if (ret) {
        printk (KERN_ERR "%s: kobject_uevent_env failed: %d\n",
            __FUNCTION__, ret);
    } else {
        printk_event ("%s: kobject_uevent_env %s success\n",
            __FUNCTION__, event_string);
    }
#else
    //input_event (lm3535_data.idev, EV_MSC, MSC_RAW, light_value);
	input_event (lm3535_data.idev, EV_LED, LED_MISC, zone);
	input_sync (lm3535_data.idev);

#endif
}

static void lm3535_call_als_callbacks(unsigned old_zone, unsigned zone)
{
    struct als_callback *c;
    unsigned old, new;

    old = (old_zone == ALS_NO_ZONE) ? 0 : old_zone;
    new = (zone == ALS_NO_ZONE) ? 0 : zone;

    mutex_lock (&als_cb_mutex);
    list_for_each_entry(c, &als_callbacks, entry) {
    c->cb(old, new, c->cookie);
    }
    mutex_unlock (&als_cb_mutex);
}

static void lm3535_work_func (struct work_struct *work)
{
    int ret;
    uint8_t reg;
    unsigned zone, old_zone;

    pr_debug ("%s: work function called\n", __FUNCTION__);
    ret = lm3535_read_reg (LM3535_ALS_REG, &reg);
    if (ret) {
        if (reg & ALS_FLAG_MASK) {
            zone = reg & ALS_ZONE_MASK;
            if (zone > 4)
                zone = 4;
            old_zone = atomic_read (&lm3535_data.als_zone);
            printk_als ("%s: ALS zone changed: %d => %d, register = 0x%x\n",
                __FUNCTION__, old_zone, zone, reg);
            atomic_set (&lm3535_data.als_zone, zone);
            if (zone > atomic_read (&lm3535_data.bright_zone) ||
                atomic_read (&lm3535_data.bright_zone) == ALS_NO_ZONE) {
                atomic_set (&lm3535_data.bright_zone, zone);
                if (!atomic_read (&lm3535_data.in_suspend)) {
                    printk_als ("%s: ALS zone increased; changing brightness\n",
                        __FUNCTION__);
                    /* Adjust brightness */
                    lm3535_brightness_set (&lm3535_led, -1);
                } else {
                    printk_als ("%s: ALS zone increased; SUSPEND mode - not changing brightness\n",
                        __FUNCTION__);
                }
            }
            /* See if PWM needs to be changed */
            if (old_zone < 2 && zone >= 2) {
                lm3535_write_reg (LM3535_CONFIG_REG, CONFIG_VALUE | 0x80,
                    __FUNCTION__);
                printk_als ("%s: moved from dim/dark to bright; disable PWM\n",
                    __FUNCTION__);
            } else if (old_zone >= 2 && zone < 2) {
                lm3535_write_reg (LM3535_CONFIG_REG,
                    CONFIG_VALUE|0x80|pwm_value,
                    __FUNCTION__);
                printk_als ("%s: moved from bright to dim/dark; enable PWM\n",
                    __FUNCTION__);
            }
            if (!atomic_read (&lm3535_data.in_suspend)) {
                lm3535_call_als_callbacks (old_zone, zone);
            }
            lm3535_send_als_event (zone);
        } else {
            printk_als ("%s: got ALS interrupt but flag is not set: 0x%x\n",
                __FUNCTION__, reg);
        }
    }
    if (atomic_read (&lm3535_data.do_als_config)) {
        lm3535_write_reg (LM3535_ALS_CTRL_REG, ALS_AVERAGING, __FUNCTION__);
        atomic_set (&lm3535_data.do_als_config, 0);
        printk_als ("%s: configured ALS averaging 0x%x\n",
            __FUNCTION__, ALS_AVERAGING);
    }
    enable_irq (lm3535_data.client->irq);
}

static void lm3535_allow_als_work_func(struct work_struct *work)
{
	lm3535_data.prevent_als_read = 0;
}

#if 0
static enum hrtimer_restart lm3535_timer_func (struct hrtimer *timer)
{
    schedule_work(&lm3535_data.work);

    hrtimer_start(&lm3535_data.timer,
        ktime_set(1, 0), HRTIMER_MODE_REL);
    return HRTIMER_NORESTART;
}
#endif

static void lm3535_set_options_r1 (uint8_t *buf, unsigned ramp)
{
    struct lm3535_options_register_r1 *r =
        (struct lm3535_options_register_r1 *)buf;

    if (!r)
        return;
    *buf = 0;
    r->rs = ramp;
    r->gt = 0x2;
    r->rev = 0;
}

static void lm3535_set_options_r2 (uint8_t *buf, unsigned ramp)
{
    struct lm3535_options_register_r2 *r =
        (struct lm3535_options_register_r2 *)buf;

    if (!r)
        return;
    *buf = 0;
    r->rs_up = ramp;
    r->rs_down = ramp;
    r->gt = 0;
}

static void lm3535_set_options_r3 (uint8_t *buf, unsigned ramp)
{
    struct lm3535_options_register_r3 *r =
        (struct lm3535_options_register_r3 *)buf;

    if (!r)
        return;
    *buf = 0;
    r->rs_up = ramp;
    r->rs_down = ramp;
    r->gt = 0x02;
}

/* This function calculates ramp step time so that total ramp time is
 * equal to ramp_time defined currently at 200ms
 */
static int lm3535_set_ramp (struct i2c_client *client,
    unsigned int on, unsigned int nsteps, unsigned int *rtime)
{
    int ret, i = 0;
    uint8_t value = 0;
    unsigned int total_time = 0;

    if (on) {
        /* Calculate the closest possible ramp time */
        for (i = 0; i < lm3535_data.nramp; i++) {
            total_time = nsteps * lm3535_ramp[i];
            if (total_time >= ramp_time)
                break;
        }
        if (i > 0 && total_time > ramp_time) {
#if 0
            /* If previous value is closer */
            if (total_time - ramp_time >
                    ramp_time - nsteps * lm3535_ramp[i-1]) {
                i--;
                total_time = nsteps * lm3535_ramp[i];
            }
#endif
            i--;
            total_time = nsteps * lm3535_ramp[i];
        }
        lm3535_set_options_f (&value, i);
    }

#if 0
    printk (KERN_ERR "%s: ramp = %s, ramp step = %d us (total = %d us)\n",
        __FUNCTION__, on ? "on" : "off", lm3535_ramp[i], total_time);
#endif
    if (rtime)
        *rtime = total_time;
    ret = lm3535_write_reg (LM3535_OPTIONS_REG, value, __FUNCTION__);
#if 0
    printk_br ("%s: nsteps = %d, OPTIONS_REG = 0x%x, total ramp = %dus\n",
        __FUNCTION__, nsteps, value, lm3535_ramp[i] * nsteps);
#endif
    return ret;
}

static int lm3535_enable(struct i2c_client *client, unsigned int on)
{
    int ret;
    uint8_t value = 0x0F; // Enable A

    if (on) {
        gpio_set_value(LM3535_HWEN_GPIO, 1);
        msleep(1);
    } else
        value = 0;

    ret = lm3535_write_reg (LM3535_ENABLE_REG, value, __FUNCTION__);
    if (ret < 0)
        return ret;

    if (lm3535_data.revision == 2 || lm3535_data.revision == 3) {
        if (on) {
            ret = lm3535_write_reg (LM3535_TRIM_REG, LM3535_TRIM_VALUE,
                __FUNCTION__);
        }
    }
    if (!on)
        gpio_set_value(LM3535_HWEN_GPIO, 0);

    lm3535_data.enabled = on;
    return ret;
}

static int lm3535_configure (void)
{
    int ret = 0;

#if 1
    /* On G2, we are not using ALS interrupt so we want to leave the ALS
       interrupt disabled and ALS resistor in high impedance mode */
    /* Disable ALS interrupt */
    lm3535_write_reg(LM3535_CONFIG_REG, 0, __func__);
    /* Put ALS Resistor into high impedance mode to save current */
    lm3535_write_reg(LM3535_ALS_RESISTOR_REG, 0, __func__);

#else
    uint8_t value = 0x03;
    uint8_t reg = 0;
    unsigned old_zone = 0;
    unsigned new_zone = ALS_NO_ZONE;

    /* Config register bits:
     * Rev1: AVE2  AVE1    AVE0     ALS-SD   ALS-EN PWM-EN  53A    62A
     * Rev2: ALSF  ALS-SD  ALS-ENB  ALS-ENA  62A    53A     PWM-P  PWM-EN
     * Rev3: ALSF  ALS-EN  ALS-ENB  ALS-ENA  62A    53A     PWM-P  PWM-EN
     *
     * ALSF: sets E1B/INT pin to interrupt Pin; 0 = D1B, 1 = INT.
     *     Open Drain Interrupt.  Pulls low when change occurs.  Flag cleared
     *     once a I2C read command for register 0x40 occurs.
     * ALS-SD (rev2) and ALS-EN (final):  turn off ALS feature
     *     ALS-SD: 0 = Active, 1 = Shutdown. Rev2 cannot have ALS active without
     *         LEDs turned on (ENxx bits = 1).
     *     ALS-EN: 0 = Shutdown, 1 = Active. Final can have ALS active without
     *         LEDs turned on.  ALS-EN overrides the ENC bit.
     * ALS-ENB and ALS-ENA: 1 enables ALS control of diode current. BankA has
     *     full ALS control, BankB just has on/off ability.
     * 62A and 53A: 1 sets D62 and D53 to BankA (Required for 6 LEDs in BankA).
     *     0 sets them to BankB.
     * PWM-P: PWM Polarity, 0 = Active (Diodes on) High, 1 = Active (Diodes on)
     *     Low.
     * PWM-EN: Enables PWM Functionality. 1 = Active.
     */

#ifndef CONFIG_MAC_MOT
#ifdef CONFIG_LM3535_ESD_RECOVERY
    /* Configure lighting zone max brightness */
    lm3535_write_reg (LM3535_ALS_Z0T_REG, als_zone_max[0],
        __FUNCTION__);
    lm3535_write_reg (LM3535_ALS_Z1T_REG, als_zone_max[1],
        __FUNCTION__);
    lm3535_write_reg (LM3535_ALS_Z2T_REG, als_zone_max[2],
        __FUNCTION__);
    lm3535_write_reg (LM3535_ALS_Z3T_REG, als_zone_max[3],
        __FUNCTION__);
    lm3535_write_reg (LM3535_ALS_Z4T_REG, als_zone_max[4],
        __FUNCTION__);
#endif
#endif

    if (lm3535_data.revision > 1) {
        /* Configure internal ALS resistor register */
        ret = lm3535_write_reg (LM3535_ALS_RESISTOR_REG,
            (uint8_t)resistor_value, __FUNCTION__);

#ifndef CONFIG_MAC_MOT
#ifdef CONFIG_LM3535_BUTTON_BL
        ret = lm3535_write_reg (LM3535_ALS_SELECT_REG, 0x2,
            __FUNCTION__);
#endif
#endif
        /* Configure lighting zone boundaries */
        lm3535_write_reg (LM3535_ALS_ZB0_REG, als_zb[0], __FUNCTION__);
        lm3535_write_reg (LM3535_ALS_ZB1_REG, als_zb[1], __FUNCTION__);
        lm3535_write_reg (LM3535_ALS_ZB2_REG, als_zb[2], __FUNCTION__);
        lm3535_write_reg (LM3535_ALS_ZB3_REG, als_zb[3], __FUNCTION__);

        /* Configure ALS averaging to be very short the first time */
        lm3535_write_reg (LM3535_ALS_CTRL_REG, 0x80, __FUNCTION__);
    }

    if (lm3535_data.revision == 0) {
        value = 0x3;  // Just enable A, don't bother with ALS
        atomic_set (&lm3535_data.use_als, 0);
    } else if (lm3535_data.revision == 1) {
        value = 0x0C; // No ALS or PWM
        atomic_set (&lm3535_data.use_als, 0);
    } else {
        if (atomic_read (&lm3535_data.use_als))
            value = CONFIG_VALUE;
        else
            value = CONFIG_VALUE_NO_ALS;
    }
    pr_debug ("%s: use_als is %d, value = 0x%x\n", __FUNCTION__,
        atomic_read (&lm3535_data.use_als), value);
    ret = lm3535_write_reg (LM3535_CONFIG_REG, value, __FUNCTION__);
    // Nothing else to do for older revisions
    if (lm3535_data.revision <= 1) {
        return 0;
    }

    /* Has to be at least 300ms even with ALS averaging set to 0 */
    msleep_interruptible (als_sleep);  // Wait for ALS to kick in
    /* Read current ALS zone */
    old_zone = atomic_read (&lm3535_data.als_zone);
    if (atomic_read (&lm3535_data.use_als)) {
        ret = lm3535_read_reg (LM3535_ALS_REG, &reg);
        if (ret) {
            new_zone = reg & ALS_ZONE_MASK;
            if (new_zone > 4) {
                new_zone = 4;
            }
            atomic_set (&lm3535_data.als_zone, new_zone);
            printk_als ("%s: ALS Register: 0x%x, zone %d\n",
                __FUNCTION__, reg, atomic_read (&lm3535_data.als_zone));
        } else {
            atomic_set (&lm3535_data.als_zone, ALS_NO_ZONE);
            atomic_set (&lm3535_data.use_als, 0);
            printk (KERN_ERR "%s: unable to read ALS zone; disabling ALS\n",
                __FUNCTION__);
        }
    } else {
        atomic_set (&lm3535_data.als_zone, ALS_NO_ZONE);
    }
    /* Brightness zone is for now the same as ALS zone */
    atomic_set (&lm3535_data.bright_zone,
        atomic_read (&lm3535_data.als_zone));
    lm3535_call_als_callbacks (old_zone, new_zone);
    if (lm3535_data.initialized) {
        lm3535_send_als_event (new_zone);
    }
    if (atomic_read (&lm3535_data.use_als)) {
        /* Configure averaging */
        //lm3535_write_reg (LM3535_ALS_CTRL_REG, ALS_AVERAGING, __FUNCTION__);
        /* Enable interrupt and PWM for CABC */
        if (new_zone <= 1) {
            lm3535_write_reg (LM3535_CONFIG_REG, CONFIG_VALUE|0x80|pwm_value,
                __FUNCTION__);
        } else {
            lm3535_write_reg (LM3535_CONFIG_REG, CONFIG_VALUE | 0x80,
                __FUNCTION__);
        }
    } else {
        lm3535_write_reg (LM3535_CONFIG_REG, CONFIG_VALUE | pwm_value,
            __FUNCTION__);
    }
#endif

    return ret;
}

static int lm3535_setup (struct i2c_client *client)
{
    int ret;
    uint8_t value;

    /* Read revision number */
    ret = lm3535_read_reg (LM3535_ALS_CTRL_REG, &value);
    if (ret < 0) {
        printk (KERN_ERR "%s: unable to read from chip: %d\n",
            __FUNCTION__, ret);
        printk(KERN_ERR "lm3535 client->addr = 0x%x failed\n", client->addr);

        /* If the first I2C address doesn't work, try 0x36 */
        client->addr = 0x36;
        ret = lm3535_read_reg (LM3535_ALS_CTRL_REG, &value);
        if (ret < 0) {
          printk (KERN_ERR "%s: unable to read from chip: %d\n",
            __FUNCTION__, ret);
          printk(KERN_ERR "lm3535 client->addr = 0x%x failed\n", client->addr);
          return ret;
        }
    }

    switch (value) {
        case 0xFF: lm3535_data.revision = 0; break;
        case 0xF0: lm3535_data.revision = 1; break;
        case 0x02: lm3535_data.revision = 2; break;
        case 0x00: lm3535_data.revision = 3; break;
        case 0x01: lm3535_data.revision = 4; break;
        default: lm3535_data.revision = 4; break; // Assume final
    }
    /* revision is going to be an index to lm3535_ramp array */
    printk (KERN_INFO "%s: revision %d (0x%X)\n",
        __FUNCTION__, lm3535_data.revision+1, value);
    if (lm3535_data.revision == 0) {
        lm3535_ramp = lm3535_ramp_r1;
        lm3535_set_options_f = lm3535_set_options_r1;
    } else if (lm3535_data.revision == 1) {
        lm3535_ramp = lm3535_ramp_r2;
        lm3535_set_options_f = lm3535_set_options_r2;
    } else {
        lm3535_ramp = lm3535_ramp_r3;
        lm3535_set_options_f = lm3535_set_options_r3;
        lm3535_data.nramp = 8;
    }

    /* PWM */
    if (lm3535_data.revision < 4) {
        pwm_value = 0;
    }
    atomic_set (&lm3535_data.als_zone, ALS_NO_ZONE);
    atomic_set (&lm3535_data.use_als, 1);
    atomic_set (&lm3535_data.do_als_config, 1);
    ret = lm3535_configure ();
    if (ret < 0)
        return ret;
    ret = lm3535_enable(client, 1);
    if (ret < 0)
        return ret;

    //hrtimer_init (&lm3535_data.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
    //lm3535_data.timer.function = lm3535_timer_func;
    //hrtimer_start(&lm3535_data.timer, ktime_set(2, 0), HRTIMER_MODE_REL);

    return ret;
}

static int lm3535_remove (struct i2c_client *client)
{
    struct lm3535 *data_ptr = i2c_get_clientdata(client);

	cancel_delayed_work_sync(&lm3535_data.als_delayed_work);
	lm3535_data.prevent_als_read = 0;
    if (data_ptr->use_irq)
        free_irq (client->irq, data_ptr);
    led_classdev_unregister (&lm3535_led);
    led_classdev_unregister(&lm3535_led_noramp);
/*    led_classdev_unregister (&lm3535_led_noramp); */
    misc_deregister (&als_miscdev);
    device_remove_file (lm3535_led.dev, &dev_attr_suspend);
#if 0
    input_unregister_device (lm3535_data.idev);
    input_free_device (lm3535_data.idev);
#endif
    return 0;
}

#if defined(CONFIG_HAS_EARLYSUSPEND) || !defined(CONFIG_HAS_AMBIENTMODE)
static int lm3535_suspend (struct i2c_client *client, pm_message_t mesg)
{
    printk_suspend ("%s: called with pm message %d\n",
        __FUNCTION__, mesg.event);

    led_classdev_suspend (&lm3535_led);

#if 0
    /* Disable ALS interrupt */
    lm3535_write_reg (LM3535_CONFIG_REG, 0, __FUNCTION__);
    /* Put ALS Resistor into high impedance mode to save current */
    lm3535_write_reg (LM3535_ALS_RESISTOR_REG, 0, __FUNCTION__);

    /* Reset ALS averaging */
    lm3535_write_reg (LM3535_ALS_CTRL_REG, 0, __FUNCTION__);
    atomic_set (&lm3535_data.do_als_config, 1);
#endif

    gpio_set_value(LM3535_HWEN_GPIO, 0);

    return 0;
}

static int lm3535_resume (struct i2c_client *client)
{
    printk_suspend ("%s: resuming\n", __FUNCTION__);
    mutex_lock (&lm3535_mutex);

    /* If LED was disabled going into suspend, we don't want to enable yet
       until brightness is set to non-zero value */
    if (lm3535_data.enabled == 0) {
        gpio_set_value(LM3535_HWEN_GPIO, 1);
        udelay(10);

        lm3535_configure();
    }
    mutex_unlock (&lm3535_mutex);
    led_classdev_resume (&lm3535_led);
    printk_suspend ("%s: driver resumed\n", __FUNCTION__);

    return 0;
}
#endif

#ifdef CONFIG_HAS_EARLYSUSPEND
static void lm3535_early_suspend (struct early_suspend *h)
{
    lm3535_suspend (lm3535_data.client, PMSG_SUSPEND);
}

static void lm3535_late_resume (struct early_suspend *h)
{
    lm3535_resume (lm3535_data.client);
}
#endif


static int __init lm3535_init (void)
{
    int ret;

    printk (KERN_INFO "%s: enter\n", __FUNCTION__);
    ret = i2c_add_driver (&lm3535_driver);
    if (ret) {
        printk (KERN_ERR "%s: i2c_add_driver failed, error %d\n",
            __FUNCTION__, ret);
    }

    return ret;
}

static void __exit lm3535_exit(void)
{
    i2c_del_driver (&lm3535_driver);
}

static char *reg_name (int reg)
{
    switch (reg) {
        case (LM3535_ENABLE_REG): return "ENABLE"; break;
        case (LM3535_CONFIG_REG): return "CONFIG"; break;
        case (LM3535_OPTIONS_REG): return "OPTIONS"; break;
        case (LM3535_ALS_REG): return "ALS"; break;
        case (LM3535_ALS_RESISTOR_REG): return "ALS_RESISTOR"; break;
        case (LM3535_ALS_CTRL_REG): return "ALS CONTROL"; break;
        case (LM3535_BRIGHTNESS_CTRL_REG_A): return "BRIGHTNESS_CTRL_A"; break;
        case (LM3535_BRIGHTNESS_CTRL_REG_B): return "BRIGHTNESS_CTRL_B"; break;
        case (LM3535_BRIGHTNESS_CTRL_REG_C): return "BRIGHTNESS_CTRL_C"; break;
        case (LM3535_ALS_ZB0_REG): return "ALS_ZB0"; break;
        case (LM3535_ALS_ZB1_REG): return "ALS_ZB1"; break;
        case (LM3535_ALS_ZB2_REG): return "ALS_ZB2"; break;
        case (LM3535_ALS_ZB3_REG): return "ALS_ZB3"; break;
        case (LM3535_ALS_Z0T_REG): return "ALS_Z0T"; break;
        case (LM3535_ALS_Z1T_REG): return "ALS_Z1T"; break;
        case (LM3535_ALS_Z2T_REG): return "ALS_Z2T"; break;
        case (LM3535_ALS_Z3T_REG): return "ALS_Z3T"; break;
        case (LM3535_ALS_Z4T_REG): return "ALS_Z4T"; break;
        case (LM3535_TRIM_REG): return "TRIM"; break;
        default: return "UNKNOWN"; break;
    }
    return "UNKNOWN";
}

unsigned lmxxxx_detect_esd (void)
{
    uint8_t value = 0;

    if (!lm3535_data.initialized) {
        printk (KERN_ERR "%s: not initialized\n", __FUNCTION__);
        return 0;
    }
    //0 - no ESD, 1 - ESD
    lm3535_read_reg (LM3535_ALS_RESISTOR_REG, &value);
    if (value != (uint8_t)resistor_value)
        return 1;
    return 0;
}
EXPORT_SYMBOL(lmxxxx_detect_esd);

void lmxxxx_fix_esd (void)
{
    if (!lm3535_data.initialized) {
        printk (KERN_ERR "%s: not initialized\n", __FUNCTION__);
        return;
    }
    mutex_lock (&lm3535_mutex);
    lm3535_configure ();
    mutex_unlock (&lm3535_mutex);
    return;
}
EXPORT_SYMBOL(lmxxxx_fix_esd);

void lmxxxx_set_pwm (unsigned en_dis)
{
    if (en_dis) {
        pwm_value = 1;
    } else {
        pwm_value = 0;
    }
    printk (KERN_INFO "%s: setting PWM to %d\n",
        __FUNCTION__, pwm_value);
}
EXPORT_SYMBOL(lmxxxx_set_pwm);
module_init(lm3535_init);
module_exit(lm3535_exit);

MODULE_DESCRIPTION("LM3535 DISPLAY BACKLIGHT DRIVER");
MODULE_LICENSE("GPL v2");
