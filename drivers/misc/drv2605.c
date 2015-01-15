/*
** =============================================================================
** Copyright (c) 2012  Immersion Corporation.
**
** This program is free software; you can redistribute it and/or
** modify it under the terms of the GNU General Public License
** as published by the Free Software Foundation; either version 2
** of the License, or (at your option) any later version.
**
** This program is distributed in the hope that it will be useful,
** but WITHOUT ANY WARRANTY; without even the implied warranty of
** MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
** GNU General Public License for more details.
**
** You should have received a copy of the GNU General Public License
** along with this program; if not, write to the Free Software
** Foundation, Inc., 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
**
** File:
**     drv2605.c
**
** Description:
**     DRV2605 chip driver
**
** =============================================================================
*/

#include <linux/init.h>
#include <linux/module.h>
#include <linux/kernel.h>

#include <linux/slab.h>
#include <linux/types.h>

#include <linux/fs.h>
#include <linux/cdev.h>

#include <linux/i2c.h>
#include <linux/semaphore.h>
#include <linux/device.h>

#include <linux/syscalls.h>
#include <asm/uaccess.h>

#include <linux/gpio.h>

#include <linux/sched.h>

#include <linux/drv2605.h>

#include <linux/spinlock_types.h>
#include <linux/spinlock.h>
#include <linux/delay.h>
#include <linux/jiffies.h>
#include <linux/timer.h>

#include <linux/workqueue.h>
#include <../../../drivers/staging/android/timed_output.h>
#include <linux/hrtimer.h>
#include <linux/err.h>
#include <linux/mutex.h>
#include <linux/clk.h>
#include <linux/wakelock.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>

/* Address of our device */
#define DEVICE_ADDR 0x5A

/* i2c bus that it sits on */
#define DEVICE_BUS  10

/*
   DRV2605 built-in effect bank/library
 */
#define EFFECT_LIBRARY LIBRARY_A

/*
   GPIO port that enable power to the device
*/

#define GPIO_LEVEL_HIGH 1
#define GPIO_LEVEL_LOW  0

/*
    Rated Voltage:

    Calculated using the formula r = v * 255 / 5.6
    where r is what will be written to the register
    and v is the rated voltage of the actuator

    Overdrive Clamp Voltage:
    Calculated using the formula o = oc * 255 / 5.6
    where o is what will be written to the register
    and oc is the overdrive clamp voltage of the actuator
*/

#if (EFFECT_LIBRARY == LIBRARY_A)
#define ERM_RATED_VOLTAGE               0x3E
#define ERM_OVERDRIVE_CLAMP_VOLTAGE     0x90

#elif (EFFECT_LIBRARY == LIBRARY_B)
#define ERM_RATED_VOLTAGE               0x90
#define ERM_OVERDRIVE_CLAMP_VOLTAGE     0x90

#elif (EFFECT_LIBRARY == LIBRARY_C)
#define ERM_RATED_VOLTAGE               0x90
#define ERM_OVERDRIVE_CLAMP_VOLTAGE     0x90

#elif (EFFECT_LIBRARY == LIBRARY_D)
#define ERM_RATED_VOLTAGE               0x90
#define ERM_OVERDRIVE_CLAMP_VOLTAGE     0x90

#elif (EFFECT_LIBRARY == LIBRARY_E)
#define ERM_RATED_VOLTAGE               0x90
#define ERM_OVERDRIVE_CLAMP_VOLTAGE     0x90

#else
#define ERM_RATED_VOLTAGE               0x90
#define ERM_OVERDRIVE_CLAMP_VOLTAGE     0x90
#endif

#define LRA_SEMCO1036                   0
#define LRA_SEMCO0934                   1
#define LRA_SELECTION                   LRA_SEMCO1036

#if (LRA_SELECTION == LRA_SEMCO1036)
#define LRA_RATED_VOLTAGE               0x56
#define LRA_OVERDRIVE_CLAMP_VOLTAGE     0x90
/* 100% of rated voltage (closed loop) */
#define LRA_RTP_STRENGTH                0x7F

#elif (LRA_SELECTION == LRA_SEMCO0934)
#define LRA_RATED_VOLTAGE               0x51
#define LRA_OVERDRIVE_CLAMP_VOLTAGE     0x72
/* 100% of rated voltage (closed loop) */
#define LRA_RTP_STRENGTH                0x7F
#endif

#define SKIP_LRA_AUTOCAL        1
#define GO_BIT_POLL_INTERVAL    15
#define STANDBY_WAKE_DELAY      1

#if EFFECT_LIBRARY == LIBRARY_A
/* ~44% of overdrive voltage (open loop) */
#define REAL_TIME_PLAYBACK_STRENGTH 0x7F
#elif EFFECT_LIBRARY == LIBRARY_F
/* 100% of rated voltage (closed loop) */
#define REAL_TIME_PLAYBACK_STRENGTH 0x7F
#else
/* 100% of overdrive voltage (open loop) */
#define REAL_TIME_PLAYBACK_STRENGTH 0x7F
#endif

#define MAX_TIMEOUT 15000	/* 15s */

#define DEFAULT_EFFECT 14	/* Strong buzz 100% */

#define RTP_CLOSED_LOOP_ENABLE  /* Set closed loop mode for RTP */
#define RTP_ERM_OVERDRIVE_CLAMP_VOLTAGE     0xF0

#define I2C_RETRY_DELAY		20 /* ms */
#define I2C_RETRIES		5

static struct drv260x {
	struct class *class;
	struct device *device;
	dev_t version;
	struct i2c_client *client;
	struct semaphore sem;
	struct cdev cdev;
	int en_gpio;
	int external_trigger;
	int trigger_gpio;
	unsigned char default_sequence[4];
	unsigned char rated_voltage;
	unsigned char overdrive_voltage;
	struct regulator *vibrator_vdd;
} *drv260x;

static struct vibrator {
	struct wake_lock wklock;
	struct pwm_device *pwm_dev;
	struct hrtimer timer;
	struct mutex lock;
	struct work_struct work;
	struct work_struct work_play_eff;
	struct work_struct work_probe;
	unsigned char sequence[8];
	volatile int should_stop;
} vibdata;

static char g_effect_bank = EFFECT_LIBRARY;
static int device_id = -1;

static unsigned char ERM_autocal_sequence[] = {
	MODE_REG, AUTO_CALIBRATION,
	REAL_TIME_PLAYBACK_REG, REAL_TIME_PLAYBACK_STRENGTH,
	LIBRARY_SELECTION_REG, EFFECT_LIBRARY,
	WAVEFORM_SEQUENCER_REG, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG2, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG3, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG4, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG5, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG6, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG7, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG8, WAVEFORM_SEQUENCER_DEFAULT,
	OVERDRIVE_TIME_OFFSET_REG, 0x00,
	SUSTAIN_TIME_OFFSET_POS_REG, 0x00,
	SUSTAIN_TIME_OFFSET_NEG_REG, 0x00,
	BRAKE_TIME_OFFSET_REG, 0x00,
	AUDIO_HAPTICS_CONTROL_REG,
	AUDIO_HAPTICS_RECT_20MS | AUDIO_HAPTICS_FILTER_125HZ,
	AUDIO_HAPTICS_MIN_INPUT_REG, AUDIO_HAPTICS_MIN_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_INPUT_REG, AUDIO_HAPTICS_MAX_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MIN_OUTPUT_REG, AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_OUTPUT_REG, AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE,
	RATED_VOLTAGE_REG, ERM_RATED_VOLTAGE,
	OVERDRIVE_CLAMP_VOLTAGE_REG, ERM_OVERDRIVE_CLAMP_VOLTAGE,
	AUTO_CALI_RESULT_REG, DEFAULT_ERM_AUTOCAL_COMPENSATION,
	AUTO_CALI_BACK_EMF_RESULT_REG, DEFAULT_ERM_AUTOCAL_BACKEMF,
	FEEDBACK_CONTROL_REG,
	FB_BRAKE_FACTOR_3X | LOOP_RESPONSE_MEDIUM |
	    FEEDBACK_CONTROL_BEMF_ERM_GAIN2,
	Control1_REG, STARTUP_BOOST_ENABLED | DEFAULT_DRIVE_TIME,
	Control2_REG,
	BIDIRECT_INPUT | AUTO_RES_GAIN_MEDIUM | BLANKING_TIME_SHORT |
	    IDISS_TIME_SHORT,
	Control3_REG, ERM_OpenLoop_Enabled | NG_Thresh_2,
	AUTOCAL_MEM_INTERFACE_REG, AUTOCAL_TIME_500MS,
	GO_REG, GO,
};

static unsigned char LRA_autocal_sequence[] = {
	MODE_REG, AUTO_CALIBRATION,
	RATED_VOLTAGE_REG, LRA_RATED_VOLTAGE,
	OVERDRIVE_CLAMP_VOLTAGE_REG, LRA_OVERDRIVE_CLAMP_VOLTAGE,
	FEEDBACK_CONTROL_REG,
	FEEDBACK_CONTROL_MODE_LRA | FB_BRAKE_FACTOR_4X | LOOP_RESPONSE_FAST,
	Control3_REG, NG_Thresh_2,
	GO_REG, GO,
};

static unsigned char reinit_sequence[] = {
	RATED_VOLTAGE_REG, 0,
	AUTO_CALI_RESULT_REG, 0,
	AUTO_CALI_BACK_EMF_RESULT_REG, 0,
	FEEDBACK_CONTROL_REG, 0,
	Control1_REG, 0,
	Control2_REG, 0,
	LIBRARY_SELECTION_REG, 0,
};

#if SKIP_LRA_AUTOCAL == 1
static unsigned char LRA_init_sequence[] = {
	MODE_REG, MODE_INTERNAL_TRIGGER,
	REAL_TIME_PLAYBACK_REG, REAL_TIME_PLAYBACK_STRENGTH,
	LIBRARY_SELECTION_REG, LIBRARY_F,
	WAVEFORM_SEQUENCER_REG, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG2, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG3, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG4, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG5, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG6, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG7, WAVEFORM_SEQUENCER_DEFAULT,
	WAVEFORM_SEQUENCER_REG8, WAVEFORM_SEQUENCER_DEFAULT,
	GO_REG, STOP,
	OVERDRIVE_TIME_OFFSET_REG, 0x00,
	SUSTAIN_TIME_OFFSET_POS_REG, 0x00,
	SUSTAIN_TIME_OFFSET_NEG_REG, 0x00,
	BRAKE_TIME_OFFSET_REG, 0x00,
	AUDIO_HAPTICS_CONTROL_REG,
	AUDIO_HAPTICS_RECT_20MS | AUDIO_HAPTICS_FILTER_125HZ,
	AUDIO_HAPTICS_MIN_INPUT_REG, AUDIO_HAPTICS_MIN_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_INPUT_REG, AUDIO_HAPTICS_MAX_INPUT_VOLTAGE,
	AUDIO_HAPTICS_MIN_OUTPUT_REG, AUDIO_HAPTICS_MIN_OUTPUT_VOLTAGE,
	AUDIO_HAPTICS_MAX_OUTPUT_REG, AUDIO_HAPTICS_MAX_OUTPUT_VOLTAGE,
	RATED_VOLTAGE_REG, LRA_RATED_VOLTAGE,
	OVERDRIVE_CLAMP_VOLTAGE_REG, LRA_OVERDRIVE_CLAMP_VOLTAGE,
	AUTO_CALI_RESULT_REG, DEFAULT_LRA_AUTOCAL_COMPENSATION,
	AUTO_CALI_BACK_EMF_RESULT_REG, DEFAULT_LRA_AUTOCAL_BACKEMF,
	FEEDBACK_CONTROL_REG,
	FEEDBACK_CONTROL_MODE_LRA | FB_BRAKE_FACTOR_2X |
	    LOOP_RESPONSE_MEDIUM | FEEDBACK_CONTROL_BEMF_LRA_GAIN3,
	Control1_REG,
	STARTUP_BOOST_ENABLED | AC_COUPLE_ENABLED | AUDIOHAPTIC_DRIVE_TIME,
	Control2_REG,
	BIDIRECT_INPUT | AUTO_RES_GAIN_MEDIUM | BLANKING_TIME_MEDIUM |
	    IDISS_TIME_MEDIUM,
	Control3_REG, NG_Thresh_2 | INPUT_ANALOG,
	AUTOCAL_MEM_INTERFACE_REG, AUTOCAL_TIME_500MS,
};
#endif

#ifdef CONFIG_OF
static struct drv260x_platform_data *drv260x_of_init(struct i2c_client *client)
{
	struct drv260x_platform_data *pdata;
	struct device_node *np = client->dev.of_node;

	pdata = devm_kzalloc(&client->dev, sizeof(*pdata), GFP_KERNEL);

	if (!pdata) {
		dev_err(&client->dev, "pdata allocation failure\n");
		return NULL;
	}

	pdata->en_gpio = of_get_gpio(np, 0);
	pdata->trigger_gpio = of_get_gpio(np, 1);
	of_property_read_u32(np, "external_trigger", &pdata->external_trigger);
	of_property_read_u32(np, "default_effect", &pdata->default_effect);
	of_property_read_u32(np, "rated_voltage", &pdata->rated_voltage);
	of_property_read_u32(np, "overdrive_voltage",
				&pdata->overdrive_voltage);
	of_property_read_u32(np, "effects_library", &pdata->effects_library);
	pdata->vibrator_vdd = devm_regulator_get(&client->dev, "vibrator_vdd");

	return pdata;
}
#else
static inline struct drv260x_platform_data
				*drv260x_of_init(struct i2c_client *client)
{
	return NULL;
}
#endif

static void drv260x_write_reg_val(const unsigned char *data, unsigned int size)
{
	int i = 0;
	int tries;
	int ret;

	if (size % 2 != 0)
		return;

	while (i < size) {
		for (tries = 0; tries < I2C_RETRIES; tries++) {
			ret = i2c_smbus_write_byte_data(drv260x->client,
							data[i], data[i + 1]);
			if (!ret)
				break;
			dev_err(&drv260x->client->dev,
				"drv2605: i2c write retry %d\n", tries+1);
			msleep_interruptible(I2C_RETRY_DELAY);
		}
		i += 2;
	}
}

static void drv260x_read_reg_val(unsigned char *data, unsigned int size)
{
	int i = 0;

	if (size % 2 != 0)
		return;

	while (i < size) {
		data[i + 1] = i2c_smbus_read_byte_data(drv260x->client,
							data[i]);
		i += 2;
	}
}

static void drv260x_set_go_bit(char val)
{
	char go[] = {
		GO_REG, val
	};
	drv260x_write_reg_val(go, sizeof(go));
}

static unsigned char drv260x_read_reg(unsigned char reg)
{
	int tries;
	int ret;

	for (tries = 0; tries < I2C_RETRIES; tries++) {
		ret = i2c_smbus_read_byte_data(drv260x->client, reg);
		if (ret >= 0)
			break;
		dev_err(&drv260x->client->dev, "drv2605: i2c read retry %d\n",
						tries+1);
		msleep_interruptible(I2C_RETRY_DELAY);
	}
	return ret;
}

static void drv2605_poll_go_bit(void)
{
	while (drv260x_read_reg(GO_REG) == GO)
		schedule_timeout_interruptible(msecs_to_jiffies
					       (GO_BIT_POLL_INTERVAL));
}

static void drv2605_select_library(char lib)
{
	char library[] = {
		LIBRARY_SELECTION_REG, lib
	};
	drv260x_write_reg_val(library, sizeof(library));
}

static void drv260x_set_rtp_val(char value)
{
	char rtp_val[] = {
		REAL_TIME_PLAYBACK_REG, value
	};
	drv260x_write_reg_val(rtp_val, sizeof(rtp_val));
}

static void drv2605_set_waveform_sequence(unsigned char *seq,
						unsigned int size)
{
	unsigned char data[WAVEFORM_SEQUENCER_MAX + 1];
	int tries;
	int ret;

	if (size > WAVEFORM_SEQUENCER_MAX)
		return;

	memset(data, 0, sizeof(data));
	memcpy(&data[1], seq, size);
	data[0] = WAVEFORM_SEQUENCER_REG;

	for (tries = 0; tries < I2C_RETRIES; tries++) {
		ret = i2c_master_send(drv260x->client, data, sizeof(data));
		if (ret >= 0)
			break;
		dev_err(&drv260x->client->dev, "drv2605: i2c send retry %d\n",
						tries+1);
		msleep_interruptible(I2C_RETRY_DELAY);
	}
}

static void drv260x_vreg_control(bool vdd_supply_enable)
{
	if (!IS_ERR(drv260x->vibrator_vdd)) {
		if(vdd_supply_enable) {
			if (regulator_enable(drv260x->vibrator_vdd))
				dev_err(&drv260x->client->dev,
					"regulator_enable error\n");
		} else
			regulator_disable(drv260x->vibrator_vdd);
	}
}

static void drv260x_change_mode(char mode)
{
	unsigned char tmp[] = {
#ifdef RTP_CLOSED_LOOP_ENABLE
		Control3_REG, NG_Thresh_2,
		OVERDRIVE_CLAMP_VOLTAGE_REG, RTP_ERM_OVERDRIVE_CLAMP_VOLTAGE,
#endif
		MODE_REG, mode
	};
	drv260x_vreg_control(true);
	if (drv260x->external_trigger)
		gpio_set_value(drv260x->en_gpio, GPIO_LEVEL_HIGH);
	/* POR may occur after enable,add time to stabilize the part before
	   the I2C accesses */
	udelay(850);
	drv260x_write_reg_val(reinit_sequence, sizeof(reinit_sequence));

#ifdef RTP_CLOSED_LOOP_ENABLE
	if (mode != MODE_REAL_TIME_PLAYBACK) {
		tmp[1] |= ERM_OpenLoop_Enabled;
		tmp[3] = drv260x->overdrive_voltage;
	}
#endif
	drv260x_write_reg_val(tmp, sizeof(tmp));
}

static void drv260x_standby(void)
{
	unsigned char tmp[] = {
#ifdef RTP_CLOSED_LOOP_ENABLE
		Control3_REG, NG_Thresh_2|ERM_OpenLoop_Enabled,
		OVERDRIVE_CLAMP_VOLTAGE_REG, drv260x->overdrive_voltage,
#endif
	};
	unsigned char mode[] = {
		MODE_REG, MODE_EXTERNAL_TRIGGER_EDGE
	};
	if (drv260x->external_trigger) {
		drv2605_set_waveform_sequence(drv260x->default_sequence, 1);
		drv260x_write_reg_val(tmp, sizeof(tmp));
		if (drv260x->external_trigger == 2)
			mode[1] = MODE_STANDBY;
		drv260x_write_reg_val(mode, sizeof(mode));
		gpio_set_value(drv260x->en_gpio, GPIO_LEVEL_LOW);
	} else
		drv260x_change_mode(MODE_STANDBY);

	drv260x_vreg_control(false);
}

/* ------------------------------------------------------------------------- */
#define YES 1
#define NO  0

static void setAudioHapticsEnabled(int enable);
static int audio_haptics_enabled = NO;
static int vibrator_is_playing = NO;

static int vibrator_get_time(struct timed_output_dev *dev)
{
	if (hrtimer_active(&vibdata.timer)) {
		ktime_t r = hrtimer_get_remaining(&vibdata.timer);
		return ktime_to_ms(r);
	}

	return 0;
}

static void vibrator_off(void)
{
	if (vibrator_is_playing) {
		vibrator_is_playing = NO;
		if (audio_haptics_enabled) {
			if ((drv260x_read_reg(MODE_REG) & DRV260X_MODE_MASK) !=
			    MODE_AUDIOHAPTIC)
				setAudioHapticsEnabled(YES);
		} else {
			/* Write 0V to the RTP register, and then place the IC
			   in standby 20ms later. This delay is required to
			   give the active braking circuitry time to stop
			   the motor. */
			drv260x_set_rtp_val(0x0);
			schedule_timeout_interruptible(msecs_to_jiffies(20));
			drv260x_standby();
		}
	}

	wake_unlock(&vibdata.wklock);
}

static void vibrator_enable(struct timed_output_dev *dev, int value)
{
	char mode;
	if (drv260x->client == NULL)
		return;
	mutex_lock(&vibdata.lock);
	hrtimer_cancel(&vibdata.timer);
	cancel_work_sync(&vibdata.work);

	if (value) {
		wake_lock(&vibdata.wklock);

		mode = drv260x_read_reg(MODE_REG) & DRV260X_MODE_MASK;
		/* Only change the mode if not already in RTP mode;
		   RTP input already set at init */
		if (mode != MODE_REAL_TIME_PLAYBACK) {
			if (audio_haptics_enabled && mode == MODE_AUDIOHAPTIC)
				setAudioHapticsEnabled(NO);
			drv260x_set_rtp_val(REAL_TIME_PLAYBACK_STRENGTH);
			drv260x_change_mode(MODE_REAL_TIME_PLAYBACK);
			vibrator_is_playing = YES;
		}

		if (value > 0) {
			if (value > MAX_TIMEOUT)
				value = MAX_TIMEOUT;
			hrtimer_start(&vibdata.timer,
				      ns_to_ktime((u64) value * NSEC_PER_MSEC),
				      HRTIMER_MODE_REL);
		}
	} else
		vibrator_off();

	mutex_unlock(&vibdata.lock);
}

static enum hrtimer_restart vibrator_timer_func(struct hrtimer *timer)
{
	schedule_work(&vibdata.work);
	return HRTIMER_NORESTART;
}

static void vibrator_work(struct work_struct *work)
{
	vibrator_off();
}

/* ------------------------------------------------------------------------ */

static void play_effect(struct work_struct *work)
{
	if (audio_haptics_enabled &&
	    ((drv260x_read_reg(MODE_REG) & DRV260X_MODE_MASK) ==
	     MODE_AUDIOHAPTIC))
		setAudioHapticsEnabled(NO);

	drv260x_change_mode(MODE_INTERNAL_TRIGGER);
	drv2605_set_waveform_sequence(vibdata.sequence,
				      sizeof(vibdata.sequence));
	drv260x_set_go_bit(GO);

	while (drv260x_read_reg(GO_REG) == GO && !vibdata.should_stop)
		schedule_timeout_interruptible(msecs_to_jiffies
					       (GO_BIT_POLL_INTERVAL));

	wake_unlock(&vibdata.wklock);
	if (audio_haptics_enabled)
		setAudioHapticsEnabled(YES);
	else
		drv260x_standby();
}

static void setAudioHapticsEnabled(int enable)
{
	if (enable) {
		if (g_effect_bank != LIBRARY_F) {
			char audiohaptic_settings[] = {
				Control1_REG, STARTUP_BOOST_ENABLED |
				    AC_COUPLE_ENABLED | AUDIOHAPTIC_DRIVE_TIME,
				Control3_REG, NG_Thresh_2 | INPUT_ANALOG
			};
			/* Chip needs to be brought out of
			   standby to change the registers */
			drv260x_change_mode(MODE_INTERNAL_TRIGGER);
			schedule_timeout_interruptible(msecs_to_jiffies
						       (STANDBY_WAKE_DELAY));
			drv260x_write_reg_val(audiohaptic_settings,
					      sizeof(audiohaptic_settings));
		}
		drv260x_change_mode(MODE_AUDIOHAPTIC);
	} else {
		/* Disable audio-to-haptics */
		drv260x_change_mode(MODE_STANDBY);
		schedule_timeout_interruptible(msecs_to_jiffies
					       (STANDBY_WAKE_DELAY));
		/* Chip needs to be brought out of
		   standby to change the registers */
		drv260x_change_mode(MODE_INTERNAL_TRIGGER);
		if (g_effect_bank != LIBRARY_F) {
			char default_settings[] = {
				Control1_REG,
				    STARTUP_BOOST_ENABLED | DEFAULT_DRIVE_TIME,
				Control3_REG, NG_Thresh_2 | ERM_OpenLoop_Enabled
			};
			schedule_timeout_interruptible(msecs_to_jiffies
						       (STANDBY_WAKE_DELAY));
			drv260x_write_reg_val(default_settings,
					      sizeof(default_settings));
		}
	}
}

static struct timed_output_dev to_dev = {
	.name = "vibrator",
	.get_time = vibrator_get_time,
	.enable = vibrator_enable,
};
static void drv260x_update_init_sequence(unsigned char *seq, int size,
					unsigned char reg, unsigned char data)
{
	int i;
	for (i = 0; i < size; i += 2) {
		if (seq[i] == reg)
			seq[i + 1] = data;
	}
}

static void drv260x_exit(void);
static void probe_work(struct work_struct *work);

static int drv260x_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct drv260x_platform_data *pdata = NULL;

	if (client->dev.of_node)
		pdata = drv260x_of_init(client);
	else
		pdata = client->dev.platform_data;

	if (pdata == NULL) {
		dev_err(&client->dev, "platform data is NULL, exiting\n");
		return -ENODEV;
	}
	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		printk(KERN_ALERT "drv260x probe failed");
		return -ENODEV;
	}

	drv260x->en_gpio = pdata->en_gpio;
	drv260x->trigger_gpio = pdata->trigger_gpio;
	drv260x->external_trigger = pdata->external_trigger;
	drv260x->vibrator_vdd = pdata->vibrator_vdd;
	if (PTR_ERR(drv260x->vibrator_vdd) == -EPROBE_DEFER)
		return -EPROBE_DEFER;
	if(IS_ERR(drv260x->vibrator_vdd))
		printk(KERN_ALERT "drv260x->vibrator_vdd not initialized\n");

	if (gpio_request(drv260x->en_gpio, "vibrator-en") < 0) {
		printk(KERN_ALERT "drv260x: error requesting gpio\n");
		return -ENODEV;
	}
	drv260x->client = client;

	/* Enable power to the chip */
	drv260x_vreg_control(true);
	gpio_export(drv260x->en_gpio, 0);
	gpio_direction_output(drv260x->en_gpio, GPIO_LEVEL_HIGH);

	if (pdata->default_effect)
		drv260x->default_sequence[0] = pdata->default_effect;
	else
		drv260x->default_sequence[0] = DEFAULT_EFFECT;

	if (pdata->rated_voltage)
		drv260x->rated_voltage = pdata->rated_voltage;
	else
		drv260x->rated_voltage = ERM_RATED_VOLTAGE;

	if (pdata->overdrive_voltage)
		drv260x->overdrive_voltage = pdata->overdrive_voltage;
	else
		drv260x->overdrive_voltage = ERM_OVERDRIVE_CLAMP_VOLTAGE;

	if ((pdata->effects_library >= LIBRARY_A) &&
		(pdata->effects_library <= LIBRARY_F))
		g_effect_bank = pdata->effects_library;

	INIT_WORK(&vibdata.work_probe, probe_work);
	schedule_work(&vibdata.work_probe);
	return 0;
}

static void probe_work(struct work_struct *work)
{
	char status;

	drv260x_update_init_sequence(ERM_autocal_sequence,
					sizeof(ERM_autocal_sequence),
					RATED_VOLTAGE_REG,
					drv260x->rated_voltage);
	drv260x_update_init_sequence(LRA_autocal_sequence,
					sizeof(LRA_autocal_sequence),
					RATED_VOLTAGE_REG,
					drv260x->rated_voltage);
	drv260x_update_init_sequence(LRA_init_sequence,
					sizeof(LRA_init_sequence),
					RATED_VOLTAGE_REG,
					drv260x->rated_voltage);

	drv260x_update_init_sequence(ERM_autocal_sequence,
					sizeof(ERM_autocal_sequence),
					OVERDRIVE_CLAMP_VOLTAGE_REG,
					drv260x->overdrive_voltage);
	drv260x_update_init_sequence(LRA_autocal_sequence,
					sizeof(LRA_autocal_sequence),
					OVERDRIVE_CLAMP_VOLTAGE_REG,
					drv260x->overdrive_voltage);
	drv260x_update_init_sequence(LRA_init_sequence,
					sizeof(LRA_init_sequence),
					OVERDRIVE_CLAMP_VOLTAGE_REG,
					drv260x->overdrive_voltage);

	drv260x_update_init_sequence(ERM_autocal_sequence,
					sizeof(ERM_autocal_sequence),
					LIBRARY_SELECTION_REG,
					g_effect_bank);
	/* Wait 30 us */
	udelay(30);

#if SKIP_LRA_AUTOCAL == 1
	/* Run auto-calibration */
	if (g_effect_bank != LIBRARY_F)
		drv260x_write_reg_val(ERM_autocal_sequence,
				      sizeof(ERM_autocal_sequence));
	else
		drv260x_write_reg_val(LRA_init_sequence,
				      sizeof(LRA_init_sequence));
#else
	/* Run auto-calibration */
	if (g_effect_bank == LIBRARY_F)
		drv260x_write_reg_val(LRA_autocal_sequence,
				      sizeof(LRA_autocal_sequence));
	else
		drv260x_write_reg_val(ERM_autocal_sequence,
				      sizeof(ERM_autocal_sequence));
#endif

	/* Wait until the procedure is done */
	drv2605_poll_go_bit();

	/* Read status */
	status = drv260x_read_reg(STATUS_REG);

#if SKIP_LRA_AUTOCAL == 0
	/* Check result */
	if ((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED) {
		printk(KERN_ALERT "drv260x auto-cal failed.\n");
		if (g_effect_bank == LIBRARY_F)
			drv260x_write_reg_val(LRA_autocal_sequence,
					      sizeof(LRA_autocal_sequence));
		else
			drv260x_write_reg_val(ERM_autocal_sequence,
					      sizeof(ERM_autocal_sequence));
		drv2605_poll_go_bit();
		status = drv260x_read_reg(STATUS_REG);
		if ((status & DIAG_RESULT_MASK) == AUTO_CAL_FAILED) {
			printk(KERN_ALERT "drv260x auto-cal retry failed.\n");
			// return -ENODEV;
		}
	}
#endif

	/* Choose default effect library */
	drv2605_select_library(g_effect_bank);

	/* Read calibration results */
	drv260x_read_reg_val(reinit_sequence, sizeof(reinit_sequence));

	/* Read device ID */
	device_id = (status & DEV_ID_MASK);
	switch (device_id) {
	case DRV2605:
		printk(KERN_ALERT "drv260x driver found: drv2605.\n");
		break;
	case DRV2604:
		printk(KERN_ALERT "drv260x driver found: drv2604.\n");
		break;
	default:
		printk(KERN_ALERT "drv260x driver found: unknown.\n");
		break;
	}

	/* Put hardware in standby */
	drv260x_standby();

	if (timed_output_dev_register(&to_dev) < 0) {
		printk(KERN_ALERT "drv260x: fail to create timed output dev\n");
		drv260x_exit();
		/* return -ENODEV; */
		return;
	}

	printk(KERN_ALERT "drv260x probe work succeeded");
	return;
}

static int drv260x_remove(struct i2c_client *client)
{
	printk(KERN_ALERT "drv260x remove");
	return 0;
}

static struct i2c_device_id drv260x_id_table[] = {
	{DEVICE_NAME, 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, drv260x_id_table);
#ifdef CONFIG_OF
static struct of_device_id drv260x_match_tbl[] = {
		{ .compatible = "ti,drv2604" },
		{ .compatible = "ti,drv2605" },
		{ },
	};
MODULE_DEVICE_TABLE(of, drv260x_match_tbl);
#endif

static struct i2c_driver drv260x_driver = {
	.driver = {
		  .name = DEVICE_NAME,
		  .of_match_table = of_match_ptr(drv260x_match_tbl),
		  },
	.id_table = drv260x_id_table,
	.probe = drv260x_probe,
	.remove = drv260x_remove
};

static char read_val;

static ssize_t drv260x_read(struct file *filp, char *buff, size_t length,
			    loff_t * offset)
{
	buff[0] = read_val;
	return 1;
}

static ssize_t drv260x_write(struct file *filp, const char *buff, size_t len,
			     loff_t * off)
{
	mutex_lock(&vibdata.lock);
	hrtimer_cancel(&vibdata.timer);

	vibdata.should_stop = YES;
	cancel_work_sync(&vibdata.work_play_eff);
	cancel_work_sync(&vibdata.work);

	if (vibrator_is_playing) {
		vibrator_is_playing = NO;
		drv260x_standby();
	}

	switch (buff[0]) {
	case HAPTIC_CMDID_PLAY_SINGLE_EFFECT:
	case HAPTIC_CMDID_PLAY_EFFECT_SEQUENCE:
		{
			memset(&vibdata.sequence, 0, sizeof(vibdata.sequence));
			if (!copy_from_user
			    (&vibdata.sequence, &buff[1], len - 1)) {
				vibdata.should_stop = NO;
				wake_lock(&vibdata.wklock);
				schedule_work(&vibdata.work_play_eff);
			}
			break;
		}
	case HAPTIC_CMDID_PLAY_TIMED_EFFECT:
		{
			unsigned int value = 0;
			char mode;

			value = buff[2];
			value <<= 8;
			value |= buff[1];

			if (value) {
				wake_lock(&vibdata.wklock);

				mode =
				    drv260x_read_reg(MODE_REG) &
				    DRV260X_MODE_MASK;
				if (mode != MODE_REAL_TIME_PLAYBACK) {
					if (audio_haptics_enabled
					    && mode == MODE_AUDIOHAPTIC)
						setAudioHapticsEnabled(NO);
					drv260x_set_rtp_val
					    (REAL_TIME_PLAYBACK_STRENGTH);
					drv260x_change_mode
					    (MODE_REAL_TIME_PLAYBACK);
					vibrator_is_playing = YES;
				}

				if (value > 0) {
					if (value > MAX_TIMEOUT)
						value = MAX_TIMEOUT;
					hrtimer_start(&vibdata.timer,
						      ns_to_ktime((u64) value *
								  NSEC_PER_MSEC),
						      HRTIMER_MODE_REL);
				}
			}
			break;
		}
	case HAPTIC_CMDID_STOP:
		{
			if (vibrator_is_playing) {
				vibrator_is_playing = NO;
				if (audio_haptics_enabled)
					setAudioHapticsEnabled(YES);
				else
					drv260x_standby();
			}
			vibdata.should_stop = YES;
			break;
		}
	case HAPTIC_CMDID_GET_DEV_ID:
		{
			/* Dev ID includes 2 parts, upper word for device id,
				lower word for chip revision */
			int revision =
			    (drv260x_read_reg(SILICON_REVISION_REG) &
			     SILICON_REVISION_MASK);
			read_val = (device_id >> 1) | revision;
			break;
		}
	case HAPTIC_CMDID_RUN_DIAG:
		{
			char diag_seq[] = {
				MODE_REG, MODE_DIAGNOSTICS,
				GO_REG, GO
			};
			if (audio_haptics_enabled &&
			    ((drv260x_read_reg(MODE_REG) & DRV260X_MODE_MASK) ==
			     MODE_AUDIOHAPTIC))
				setAudioHapticsEnabled(NO);
			drv260x_write_reg_val(diag_seq, sizeof(diag_seq));
			drv2605_poll_go_bit();
			read_val =
			    (drv260x_read_reg(STATUS_REG) & DIAG_RESULT_MASK) >>
			    3;
			break;
		}
	case HAPTIC_CMDID_AUDIOHAPTIC_ENABLE:
		{
			if ((drv260x_read_reg(MODE_REG) & DRV260X_MODE_MASK) !=
			    MODE_AUDIOHAPTIC) {
				setAudioHapticsEnabled(YES);
				audio_haptics_enabled = YES;
			}
			break;
		}
	case HAPTIC_CMDID_AUDIOHAPTIC_DISABLE:
		{
			if (audio_haptics_enabled) {
				if ((drv260x_read_reg(MODE_REG) &
				     DRV260X_MODE_MASK) == MODE_AUDIOHAPTIC)
					setAudioHapticsEnabled(NO);
				audio_haptics_enabled = NO;
				drv260x_standby();
			}
			break;
		}
	case HAPTIC_CMDID_AUDIOHAPTIC_GETSTATUS:
		{
			if ((drv260x_read_reg(MODE_REG) & DRV260X_MODE_MASK) ==
			    MODE_AUDIOHAPTIC)
				read_val = 1;
			else
				read_val = 0;
			break;
		}
	default:
		break;
	}

	mutex_unlock(&vibdata.lock);

	return len;
}

static struct file_operations fops = {
	.read = drv260x_read,
	.write = drv260x_write
};

static int drv260x_init(void)
{
	int reval = -ENOMEM;

	drv260x = kmalloc(sizeof *drv260x, GFP_KERNEL);
	if (!drv260x) {
		printk(KERN_ALERT
		       "drv260x: cannot allocate memory for drv260x driver\n");
		goto fail0;
	}

	reval = i2c_add_driver(&drv260x_driver);
	if (reval) {
		printk(KERN_ALERT "drv260x driver initialization error \n");
		goto fail2;
	}

	drv260x->version = MKDEV(0, 0);
	reval = alloc_chrdev_region(&drv260x->version, 0, 1, DEVICE_NAME);
	if (reval < 0) {
		printk(KERN_ALERT "drv260x: error getting major number %d\n",
		       reval);
		goto fail3;
	}

	drv260x->class = class_create(THIS_MODULE, DEVICE_NAME);
	if (!drv260x->class) {
		printk(KERN_ALERT "drv260x: error creating class\n");
		goto fail4;
	}

	drv260x->device =
	    device_create(drv260x->class, NULL, drv260x->version, NULL,
			  DEVICE_NAME);
	if (!drv260x->device) {
		printk(KERN_ALERT "drv260x: error creating device 2605\n");
		goto fail5;
	}

	cdev_init(&drv260x->cdev, &fops);
	drv260x->cdev.owner = THIS_MODULE;
	drv260x->cdev.ops = &fops;
	reval = cdev_add(&drv260x->cdev, drv260x->version, 1);

	if (reval) {
		printk(KERN_ALERT "drv260x: fail to add cdev\n");
		goto fail6;
	}

	hrtimer_init(&vibdata.timer, CLOCK_MONOTONIC, HRTIMER_MODE_REL);
	vibdata.timer.function = vibrator_timer_func;
	INIT_WORK(&vibdata.work, vibrator_work);
	INIT_WORK(&vibdata.work_play_eff, play_effect);

	wake_lock_init(&vibdata.wklock, WAKE_LOCK_SUSPEND, "vibrator");
	mutex_init(&vibdata.lock);

	printk(KERN_ALERT "drv260x: initialized\n");
	return 0;

 fail6:
	device_destroy(drv260x->class, drv260x->version);
 fail5:
	class_destroy(drv260x->class);
 fail4:
 fail3:
	i2c_del_driver(&drv260x_driver);
 fail2:
	kfree(drv260x);
 fail0:
	return reval;
}

static void drv260x_exit(void)
{
	gpio_direction_output(drv260x->en_gpio, GPIO_LEVEL_LOW);
	gpio_free(drv260x->en_gpio);
	if (!IS_ERR(drv260x->vibrator_vdd))
		devm_regulator_put(drv260x->vibrator_vdd);
	device_destroy(drv260x->class, drv260x->version);
	class_destroy(drv260x->class);
	unregister_chrdev_region(drv260x->version, 1);
	i2c_unregister_device(drv260x->client);
	kfree(drv260x);
	i2c_del_driver(&drv260x_driver);

	printk(KERN_ALERT "drv260x: exit\n");
}

module_init(drv260x_init);
module_exit(drv260x_exit);

/*  Current code version: 182 */
MODULE_LICENSE("GPL");
MODULE_AUTHOR("Immersion Corp.");
MODULE_DESCRIPTION("Driver for " DEVICE_NAME);
