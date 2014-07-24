/*
* Simple driver for Texas Instruments LM3630A Backlight driver chip
* Copyright (C) 2012 Texas Instruments
* Copyright (C) 2014 Motorola Mobility LLC.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 as
* published by the Free Software Foundation.
*
*/

#define pr_fmt(fmt)	"%s: " fmt, __func__

#include <linux/module.h>
#include <linux/slab.h>
#include <linux/i2c.h>
#include <linux/backlight.h>
#include <linux/err.h>
#include <linux/delay.h>
#include <linux/uaccess.h>
#include <linux/interrupt.h>
#include <linux/regmap.h>
#include <linux/pwm.h>
#include <linux/leds.h>
#include <linux/of.h>
#include <linux/of_gpio.h>
#include <linux/regulator/consumer.h>
#include <linux/platform_data/lm3630a_bl.h>

#define REG_CTRL	0x00
#define REG_BOOST	0x02
#define REG_CONFIG	0x01
#define REG_BRT_A	0x03
#define REG_BRT_B	0x04
#define REG_I_A		0x05
#define REG_I_B		0x06
#define REG_INT_STATUS	0x09
#define REG_INT_EN	0x0A
#define REG_FAULT	0x0B
#define REG_PWM_OUTLOW	0x12
#define REG_PWM_OUTHIGH	0x13
#define REG_MAX		0x1F

#define INT_DEBOUNCE_MSEC	10
#define LM3630A_VDDIO_VOLT	1800000

#define LM3630A_MAX_BRIGHTNESS		0xFF
#define LM3630A_HBM_OFF_BRIGHTNESS	(LM3630A_MAX_BRIGHTNESS + 1)
#define LM3630A_HBM_ON_BRIGHTNESS	(LM3630A_HBM_OFF_BRIGHTNESS + 1)
#define LM3630A_MAX_LOGIC_BRIGHTNESS	LM3630A_HBM_ON_BRIGHTNESS
#define LM3630A_MAX_CURRENT		0x1F
#define LM3630A_BOOST_CTRL_DEFAULT	0x38
#define LM3630A_FLT_STRENGTH_DEFAULT	0x03
#define LM3630A_CONFIG_DEFAULT		0x00

struct lm3630a_chip {
	struct device *dev;
	struct delayed_work work;

	int irq;
	struct workqueue_struct *irqthread;
	struct lm3630a_platform_data *pdata;
	struct backlight_device *bleda;
	struct backlight_device *bledb;
	struct regmap *regmap;
	struct pwm_device *pwmd;
	struct regulator *vddio;
	struct led_classdev ledcdev;
	struct workqueue_struct *ledwq;
	struct work_struct ledwork;
	int ledval;
};

#ifdef CONFIG_FB_BACKLIGHT
static bool is_fb_backlight = true;
#else
static bool is_fb_backlight;
#endif
/* i2c access */
static int lm3630a_read(struct lm3630a_chip *pchip, unsigned int reg)
{
	int rval;
	unsigned int reg_val;

	rval = regmap_read(pchip->regmap, reg, &reg_val);
	if (rval < 0)
		return rval;
	return reg_val & 0xFF;
}

static int lm3630a_write(struct lm3630a_chip *pchip,
			 unsigned int reg, unsigned int data)
{
	return regmap_write(pchip->regmap, reg, data);
}

static int lm3630a_update(struct lm3630a_chip *pchip,
			  unsigned int reg, unsigned int mask,
			  unsigned int data)
{
	return regmap_update_bits(pchip->regmap, reg, mask, data);
}

static int lm3630a_chip_enable(struct lm3630a_chip *pchip)
{
	struct lm3630a_platform_data *pdata = pchip->pdata;
	int ret;

	if (!IS_ERR(pchip->vddio)) {
		if (regulator_set_voltage(pchip->vddio,
				LM3630A_VDDIO_VOLT, LM3630A_VDDIO_VOLT)) {
			dev_err(pchip->dev, "Fail to set regulator voltage.\n");
			ret = -EBUSY;
			goto err;
		}
		if (regulator_enable(pchip->vddio)) {
			dev_err(pchip->dev, "Fail to enable regulator.\n");
			ret = -EIO;
			goto err;
		}
	}

	if (gpio_is_valid(pdata->hwen_gpio))
		gpio_direction_output(pdata->hwen_gpio, 1);

err:
	return ret;
}

static int lm3630a_chip_disable(struct lm3630a_chip *pchip)
{
	struct lm3630a_platform_data *pdata = pchip->pdata;
	int ret;

	if (!IS_ERR(pchip->vddio)) {
		if (regulator_disable(pchip->vddio)) {
			dev_err(pchip->dev, "Fail to disable regulator.\n");
			ret = -EIO;
			goto err;
		}
	}

	if (gpio_is_valid(pdata->hwen_gpio))
		gpio_direction_output(pdata->hwen_gpio, 0);

err:
	return ret;
}

/* Configure chip registers */
static int lm3630a_chip_config(struct lm3630a_chip *pchip)
{
	int rval = 0;
	struct lm3630a_platform_data *pdata = pchip->pdata;

	dev_dbg(pchip->dev, "Configure registers\n");
	/* exit sleep mode */
	rval |= lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	/* set Filter Strength Register */
	rval = lm3630a_write(pchip, 0x50, pdata->flt_str);
	/* set Cofig. register */
	rval |= lm3630a_update(pchip, REG_CONFIG, 0x1f, pdata->config);
	/* set boost control */
	rval |= lm3630a_write(pchip, REG_BOOST, pdata->boost_ctrl);
	/* set current A */
	rval |= lm3630a_update(pchip, REG_I_A, 0x1F, pdata->leda_max_cur);
	/* set current B */
	rval |= lm3630a_write(pchip, REG_I_B, pdata->ledb_max_cur);
	/* set control */
	rval |= lm3630a_update(pchip, REG_CTRL, 0x14, pdata->leda_ctrl);
	rval |= lm3630a_update(pchip, REG_CTRL, 0x0B, pdata->ledb_ctrl);
	/* wait for a while to make sure configuration effective */
	usleep_range(1000, 2000);
	if (rval < 0)
		dev_err(pchip->dev, "Failed to configure registers\n");
	return rval;

}

/* initialize chip */
static int lm3630a_chip_init(struct lm3630a_chip *pchip)
{
	int rval = 0;
	struct lm3630a_platform_data *pdata = pchip->pdata;

	/* enable vddio power supply */
	if (pdata->vddio_name[0] != 0) {
		pchip->vddio = devm_regulator_get(pchip->dev,
						pdata->vddio_name);
		if (IS_ERR(pchip->vddio)) {
			dev_err(pchip->dev, "Fail to get regulator rc = %ld\n",
				PTR_ERR(pchip->vddio));
			rval = -ENXIO;
			goto err;
		}
	}

	/* enable chip */
	if (gpio_is_valid(pdata->hwen_gpio)) {
		if (devm_gpio_request(pchip->dev, pdata->hwen_gpio,
				LM3630A_NAME)) {
			dev_err(pchip->dev, "gpio %d unavailable\n",
					pdata->hwen_gpio);
			rval = -EOPNOTSUPP;
			goto err;
		}
	}

	lm3630a_chip_enable(pchip);

	if (pdata->skip_init_config) {
		dev_info(pchip->dev, "Skip init configuration.");
		goto end;
	}

	usleep_range(1000, 2000);
	rval = lm3630a_chip_config(pchip);
	/* set brightness A and B */
	rval |= lm3630a_write(pchip, REG_BRT_A, pdata->leda_init_brt);
	rval |= lm3630a_write(pchip, REG_BRT_B, pdata->ledb_init_brt);
	if (rval < 0)
		dev_err(pchip->dev, "i2c failed to access register\n");

end:
	return rval;
err:
	if (!IS_ERR(pchip->vddio))
		devm_regulator_put(pchip->vddio);
	return rval;
}

/* interrupt handling */
static void lm3630a_delayed_func(struct work_struct *work)
{
	int rval;
	struct lm3630a_chip *pchip;

	pchip = container_of(work, struct lm3630a_chip, work.work);

	rval = lm3630a_read(pchip, REG_INT_STATUS);
	if (rval < 0) {
		dev_err(pchip->dev,
			"i2c failed to access REG_INT_STATUS Register\n");
		return;
	}

	dev_info(pchip->dev, "REG_INT_STATUS Register is 0x%x\n", rval);
}

static irqreturn_t lm3630a_isr_func(int irq, void *chip)
{
	int rval;
	struct lm3630a_chip *pchip = chip;
	unsigned long delay = msecs_to_jiffies(INT_DEBOUNCE_MSEC);

	queue_delayed_work(pchip->irqthread, &pchip->work, delay);

	rval = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (rval < 0) {
		dev_err(pchip->dev, "i2c failed to access register\n");
		return IRQ_NONE;
	}
	return IRQ_HANDLED;
}

static int lm3630a_intr_config(struct lm3630a_chip *pchip)
{
	int rval;

	rval = lm3630a_write(pchip, REG_INT_EN, 0x87);
	if (rval < 0)
		return rval;

	INIT_DELAYED_WORK(&pchip->work, lm3630a_delayed_func);
	pchip->irqthread = create_singlethread_workqueue("lm3630a-irqthd");
	if (!pchip->irqthread) {
		dev_err(pchip->dev, "create irq thread fail\n");
		return -ENOMEM;
	}
	if (request_threaded_irq
	    (pchip->irq, NULL, lm3630a_isr_func,
	     IRQF_TRIGGER_FALLING | IRQF_ONESHOT, "lm3630a_irq", pchip)) {
		dev_err(pchip->dev, "request threaded irq fail\n");
		destroy_workqueue(pchip->irqthread);
		return -ENOMEM;
	}
	return rval;
}

static void lm3630a_pwm_ctrl(struct lm3630a_chip *pchip, int br, int br_max)
{
	unsigned int period = pchip->pdata->pwm_period;
	unsigned int duty = br * period / br_max;

	pwm_config(pchip->pwmd, duty, period);
	if (duty)
		pwm_enable(pchip->pwmd);
	else
		pwm_disable(pchip->pwmd);
}

/* update and get brightness */
static int lm3630a_bank_a_update_status(struct backlight_device *bl)
{
	int ret;
	struct lm3630a_chip *pchip = bl_get_data(bl);
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

	/* pwm control */
	if ((pwm_ctrl & LM3630A_PWM_BANK_A) != 0) {
		lm3630a_pwm_ctrl(pchip, bl->props.brightness,
				 bl->props.max_brightness);
		return bl->props.brightness;
	}

	/* disable sleep */
	ret = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (ret < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
	/* minimum brightness is 0x04 */
	ret = lm3630a_write(pchip, REG_BRT_A, bl->props.brightness);
	if (bl->props.brightness < 0x4)
		ret |= lm3630a_update(pchip, REG_CTRL, LM3630A_LEDA_ENABLE, 0);
	else
		ret |= lm3630a_update(pchip, REG_CTRL,
				      LM3630A_LEDA_ENABLE, LM3630A_LEDA_ENABLE);
	if (ret < 0)
		goto out_i2c_err;
	return bl->props.brightness;

out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access\n");
	return bl->props.brightness;
}

static int lm3630a_bank_a_get_brightness(struct backlight_device *bl)
{
	int brightness, rval;
	struct lm3630a_chip *pchip = bl_get_data(bl);
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

	if ((pwm_ctrl & LM3630A_PWM_BANK_A) != 0) {
		rval = lm3630a_read(pchip, REG_PWM_OUTHIGH);
		if (rval < 0)
			goto out_i2c_err;
		brightness = (rval & 0x01) << 8;
		rval = lm3630a_read(pchip, REG_PWM_OUTLOW);
		if (rval < 0)
			goto out_i2c_err;
		brightness |= rval;
		goto out;
	}

	/* disable sleep */
	rval = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (rval < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
	rval = lm3630a_read(pchip, REG_BRT_A);
	if (rval < 0)
		goto out_i2c_err;
	brightness = rval;

out:
	bl->props.brightness = brightness;
	return bl->props.brightness;
out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access register\n");
	return 0;
}

static const struct backlight_ops lm3630a_bank_a_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3630a_bank_a_update_status,
	.get_brightness = lm3630a_bank_a_get_brightness,
};

/* update and get brightness */
static int lm3630a_bank_b_update_status(struct backlight_device *bl)
{
	int ret;
	struct lm3630a_chip *pchip = bl_get_data(bl);
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

	/* pwm control */
	if ((pwm_ctrl & LM3630A_PWM_BANK_B) != 0) {
		lm3630a_pwm_ctrl(pchip, bl->props.brightness,
				 bl->props.max_brightness);
		return bl->props.brightness;
	}

	/* disable sleep */
	ret = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (ret < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
	/* minimum brightness is 0x04 */
	ret = lm3630a_write(pchip, REG_BRT_B, bl->props.brightness);
	if (bl->props.brightness < 0x4)
		ret |= lm3630a_update(pchip, REG_CTRL, LM3630A_LEDB_ENABLE, 0);
	else
		ret |= lm3630a_update(pchip, REG_CTRL,
				      LM3630A_LEDB_ENABLE, LM3630A_LEDB_ENABLE);
	if (ret < 0)
		goto out_i2c_err;
	return bl->props.brightness;

out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access REG_CTRL\n");
	return bl->props.brightness;
}

static int lm3630a_bank_b_get_brightness(struct backlight_device *bl)
{
	int brightness, rval;
	struct lm3630a_chip *pchip = bl_get_data(bl);
	enum lm3630a_pwm_ctrl pwm_ctrl = pchip->pdata->pwm_ctrl;

	if ((pwm_ctrl & LM3630A_PWM_BANK_B) != 0) {
		rval = lm3630a_read(pchip, REG_PWM_OUTHIGH);
		if (rval < 0)
			goto out_i2c_err;
		brightness = (rval & 0x01) << 8;
		rval = lm3630a_read(pchip, REG_PWM_OUTLOW);
		if (rval < 0)
			goto out_i2c_err;
		brightness |= rval;
		goto out;
	}

	/* disable sleep */
	rval = lm3630a_update(pchip, REG_CTRL, 0x80, 0x00);
	if (rval < 0)
		goto out_i2c_err;
	usleep_range(1000, 2000);
	rval = lm3630a_read(pchip, REG_BRT_B);
	if (rval < 0)
		goto out_i2c_err;
	brightness = rval;

out:
	bl->props.brightness = brightness;
	return bl->props.brightness;
out_i2c_err:
	dev_err(pchip->dev, "i2c failed to access register\n");
	return 0;
}

static const struct backlight_ops lm3630a_bank_b_ops = {
	.options = BL_CORE_SUSPENDRESUME,
	.update_status = lm3630a_bank_b_update_status,
	.get_brightness = lm3630a_bank_b_get_brightness,
};

static int lm3630a_backlight_register(struct lm3630a_chip *pchip)
{
	struct backlight_properties props;
	struct lm3630a_platform_data *pdata = pchip->pdata;

	props.type = BACKLIGHT_RAW;
	if (pdata->leda_ctrl != LM3630A_LEDA_DISABLE) {
		props.brightness = pdata->leda_init_brt;
		props.max_brightness = pdata->leda_max_brt;
		pchip->bleda = backlight_device_register("lm3630a_leda",
					pchip->dev, pchip,
					&lm3630a_bank_a_ops, &props);
		if (IS_ERR(pchip->bleda))
			return PTR_ERR(pchip->bleda);
	}

	if ((pdata->ledb_ctrl != LM3630A_LEDB_DISABLE) &&
	    (pdata->ledb_ctrl != LM3630A_LEDB_ON_A)) {
		props.brightness = pdata->ledb_init_brt;
		props.max_brightness = pdata->ledb_max_brt;
		pchip->bledb = backlight_device_register("lm3630a_ledb",
					pchip->dev, pchip,
					&lm3630a_bank_b_ops, &props);
		if (IS_ERR(pchip->bledb))
			return PTR_ERR(pchip->bledb);
	}
	return 0;
}

static const struct regmap_config lm3630a_regmap = {
	.reg_bits = 8,
	.val_bits = 8,
	.max_register = REG_MAX,
};

static void lm3630a_led_set_func(struct work_struct *work)
{
	struct lm3630a_chip *pchip;
	struct lm3630a_platform_data *pdata;
	int ret = 0, brt, ledval;
	bool new_hbm = false;
	static bool cur_hbm;
	static int cur_brt = LM3630A_MAX_BRIGHTNESS;

	pchip = container_of(work, struct lm3630a_chip, ledwork);
	ledval = pchip->ledval;
	pdata = pchip->pdata;

	dev_dbg(pchip->dev, "led value = %d\n", ledval);
	if (ledval == LM3630A_HBM_ON_BRIGHTNESS ||
	    ledval == LM3630A_HBM_OFF_BRIGHTNESS) {
		new_hbm = ledval == LM3630A_HBM_ON_BRIGHTNESS ? true : false;
		if (new_hbm == cur_hbm) {
			dev_warn(pchip->dev, "HBM state is %s already\n",
					new_hbm ? "ON" : "OFF");
			goto out;
		}
	} else {
		cur_brt = ledval;
		/* In HBM mode, brightness setting is not effective */
		if (cur_hbm)
			goto out;
	}

	if (lm3630a_read(pchip, REG_CTRL) & LM3630A_SLEEP_STATUS) {
		dev_info(pchip->dev, "wake up and re-init chip\n");
		ret = lm3630a_chip_config(pchip);
		if (ret < 0)
			goto out;
	}

	if (new_hbm != cur_hbm) {
		cur_hbm = new_hbm;
		dev_info(pchip->dev, "HBM state: %s\n", new_hbm ?
					"ON" : "OFF");
		ledval = new_hbm ?
			max(pdata->leda_max_brt, pdata->ledb_max_brt)
			: cur_brt;
		ret |= lm3630a_update(pchip, REG_I_A, 0x1F, new_hbm ?
			pdata->leda_max_hbm_cur : pdata->leda_max_cur);
		ret |= lm3630a_write(pchip, REG_I_B, new_hbm ?
			pdata->ledb_max_hbm_cur : pdata->ledb_max_cur);
	}

	if (pdata->leda_ctrl != LM3630A_LEDA_DISABLE) {
		/* pwm control */
		if ((pdata->pwm_ctrl & LM3630A_PWM_BANK_A) != 0)
			lm3630a_pwm_ctrl(pchip, ledval, pdata->leda_max_brt);
		else {
			brt = ledval > pdata->leda_max_brt ?
				pdata->leda_max_brt : ledval;
			if (!brt)
				ret = lm3630a_update(pchip, REG_CTRL,
						LM3630A_LEDA_ENABLE, 0);
			else {
				ret = lm3630a_update(pchip, REG_CTRL,
						LM3630A_LEDA_ENABLE,
						LM3630A_LEDA_ENABLE);
				ret |= lm3630a_write(pchip, REG_BRT_A, brt);
			}
			if (ret < 0)
				goto out;
		}
	}

	if ((pdata->ledb_ctrl != LM3630A_LEDB_DISABLE) &&
	    (pdata->ledb_ctrl != LM3630A_LEDB_ON_A)) {
		/* pwm control */
		if ((pdata->pwm_ctrl & LM3630A_PWM_BANK_B) != 0)
			lm3630a_pwm_ctrl(pchip, ledval, pdata->ledb_max_brt);
		else {
			brt = ledval > pdata->ledb_max_brt ?
				pdata->ledb_max_brt : ledval;
			if (!brt)
				ret = lm3630a_update(pchip, REG_CTRL,
						LM3630A_LEDB_ENABLE, 0);
			else {
				ret = lm3630a_update(pchip, REG_CTRL,
						LM3630A_LEDB_ENABLE,
						LM3630A_LEDB_ENABLE);
				ret |= lm3630a_write(pchip, REG_BRT_B, brt);
			}
			if (ret < 0)
				goto out;
		}
	}

	if (!ledval)
		ret = lm3630a_update(pchip, REG_CTRL, LM3630A_SLEEP_ENABLE,
				LM3630A_SLEEP_ENABLE);
out:
	if (ret < 0)
		dev_err(pchip->dev, "fail to set brightness\n");
	return;
}

static void lm3630a_led_set(struct led_classdev *cdev,
				enum led_brightness value)
{
	struct lm3630a_chip *pchip;

	pchip = container_of(cdev, struct lm3630a_chip, ledcdev);

	if (value > LM3630A_MAX_LOGIC_BRIGHTNESS) {
		value = LM3630A_MAX_BRIGHTNESS;
		dev_warn(pchip->dev,
			"Force brightness to %d if it's invalid\n", value);
	}

	pchip->ledval = value;
	queue_work(pchip->ledwq, &pchip->ledwork);
}

static struct led_classdev lm3630a_led_cdev = {
	.name = "lm3630a-backlight",
	.default_trigger = "bkl-trigger",
	.brightness_set = lm3630a_led_set,
	.max_brightness = LM3630A_MAX_LOGIC_BRIGHTNESS + 1,
};

static int lm3630a_parse_dt(struct device_node *np,
		struct lm3630a_chip *pchip)
{
	struct lm3630a_platform_data *pdata = pchip->pdata;
	int rc;
	u32 tmp;
	const char *st = NULL;

	rc = of_get_named_gpio(np, "ti,irq-gpio", 0);
	if (!gpio_is_valid(rc))
		dev_warn(pchip->dev, "irq gpio not specified\n");
	else
		pchip->irq = gpio_to_irq(rc);

	pdata->hwen_gpio = of_get_named_gpio(np, "ti,hwen-gpio", 0);
	if (!gpio_is_valid(pdata->hwen_gpio))
		dev_warn(pchip->dev, "hwen gpio not specified\n");

	pdata->pwm_gpio = of_get_named_gpio(np, "ti,pwm-gpio", 0);
	if (!gpio_is_valid(pdata->pwm_gpio))
		dev_warn(pchip->dev, "pwm gpio not specified\n");

	rc = of_property_read_string(np, "ti,vddio-name", &st);
	if (rc)
		dev_warn(pchip->dev, "vddio name not specified\n");
	else
		snprintf(pdata->vddio_name,
			ARRAY_SIZE(pdata->vddio_name), "%s", st);

	rc = of_property_read_u32(np, "ti,leda-ctrl", &tmp);
	if (rc) {
		dev_err(pchip->dev, "leda-ctrl not found in devtree\n");
		return -EINVAL;
	}
	pdata->leda_ctrl = tmp;

	rc = of_property_read_u32(np, "ti,ledb-ctrl", &tmp);
	if (rc) {
		dev_err(pchip->dev, "ledb-ctrl not found in devtree\n");
		return -EINVAL;
	}
	pdata->ledb_ctrl = tmp;

	rc = of_property_read_u32(np, "ti,leda-max-brt", &tmp);
	if (rc)
		dev_warn(pchip->dev, "leda-max-brt not found in devtree\n");
	pdata->leda_max_brt = rc ? LM3630A_MAX_BRIGHTNESS : tmp;

	rc = of_property_read_u32(np, "ti,ledb-max-brt", &tmp);
	if (rc)
		dev_warn(pchip->dev, "ledb-max-brt not found in devtree\n");
	pdata->ledb_max_brt = rc ? LM3630A_MAX_BRIGHTNESS : tmp;

	rc = of_property_read_u32(np, "ti,leda-init-brt", &tmp);
	if (rc)
		dev_warn(pchip->dev, "leda-init-brt not found in devtree\n");
	pdata->leda_init_brt = rc ? LM3630A_MAX_BRIGHTNESS : tmp;

	rc = of_property_read_u32(np, "ti,ledb-init-brt", &tmp);
	if (rc)
		dev_warn(pchip->dev, "ledb-init-brt not found in devtree\n");
	pdata->ledb_init_brt = rc ? LM3630A_MAX_BRIGHTNESS : tmp;

	rc = of_property_read_u32(np, "ti,leda-max-cur", &tmp);
	if (rc)
		dev_warn(pchip->dev, "leda-max-cur not found in devtree\n");
	pdata->leda_max_cur = rc ? LM3630A_MAX_CURRENT : tmp;

	rc = of_property_read_u32(np, "ti,ledb-max-cur", &tmp);
	if (rc)
		dev_warn(pchip->dev, "ledb-max-cur not found in devtree\n");
	pdata->ledb_max_cur = rc ? LM3630A_MAX_CURRENT : tmp;

	rc = of_property_read_u32(np, "ti,leda-max-hbm-cur", &tmp);
	if (rc)
		dev_warn(pchip->dev, "leda-max-hbm-cur not found in devtree\n");
	pdata->leda_max_hbm_cur = rc ? LM3630A_MAX_CURRENT : tmp;

	rc = of_property_read_u32(np, "ti,ledb-max-hbm-cur", &tmp);
	if (rc)
		dev_warn(pchip->dev, "ledb-max-hbm-cur not found in devtree\n");
	pdata->ledb_max_hbm_cur = rc ? LM3630A_MAX_CURRENT : tmp;

	rc = of_property_read_u32(np, "ti,boost-ctrl", &tmp);
	if (rc)
		dev_warn(pchip->dev, "boost-ctrl not found in devtree\n");
	pdata->boost_ctrl = rc ? LM3630A_BOOST_CTRL_DEFAULT : tmp;

	rc = of_property_read_u32(np, "ti,flt-str", &tmp);
	if (rc)
		dev_warn(pchip->dev, "flt-str not found in devtree\n");
	pdata->flt_str = rc ? LM3630A_FLT_STRENGTH_DEFAULT : tmp;

	rc = of_property_read_u32(np, "ti,config", &tmp);
	if (rc)
		dev_warn(pchip->dev, "config not found in devtree\n");
	pdata->config = rc ? LM3630A_CONFIG_DEFAULT : tmp;
	pdata->pwm_ctrl = gpio_is_valid(pdata->pwm_gpio) ?
			pdata->config & LM3630A_PWM_BANK_ALL : 0x00;

	pdata->skip_init_config = of_property_read_bool(np,
					"ti,skip-init-config");

	dev_info(pchip->dev, "loading configuration done.\n");
	return 0;
}

static int lm3630a_probe(struct i2c_client *client,
			 const struct i2c_device_id *id)
{
	struct lm3630a_platform_data *pdata = dev_get_platdata(&client->dev);
	struct lm3630a_chip *pchip;
	struct device_node *np = client->dev.of_node;
	int rval;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		dev_err(&client->dev, "fail : i2c functionality check\n");
		return -EOPNOTSUPP;
	}

	pchip = devm_kzalloc(&client->dev, sizeof(struct lm3630a_chip),
			     GFP_KERNEL);
	if (!pchip)
		return -ENOMEM;
	pchip->dev = &client->dev;

	pchip->regmap = devm_regmap_init_i2c(client, &lm3630a_regmap);
	if (IS_ERR(pchip->regmap)) {
		rval = PTR_ERR(pchip->regmap);
		dev_err(&client->dev, "fail : allocate reg. map: %d\n", rval);
		return rval;
	}

	i2c_set_clientdata(client, pchip);
	if (pdata == NULL) {
		pdata = devm_kzalloc(pchip->dev,
				     sizeof(struct lm3630a_platform_data),
				     GFP_KERNEL);
		if (pdata == NULL)
			return -ENOMEM;
		pchip->pdata = pdata;
		if (lm3630a_parse_dt(np, pchip))
			return -EINVAL;
	} else
		pchip->pdata = pdata;

	/* chip initialize */
	rval = lm3630a_chip_init(pchip);
	if (rval < 0) {
		dev_err(&client->dev, "fail : init chip\n");
		return rval;
	}

	/* backlight register */
	if (is_fb_backlight) {
		rval = lm3630a_backlight_register(pchip);
		if (rval < 0) {
			dev_err(&client->dev, "fail : backlight register.\n");
			goto err1;
		}
	} else {
		pchip->ledcdev = lm3630a_led_cdev;
		INIT_WORK(&pchip->ledwork, lm3630a_led_set_func);
		rval = led_classdev_register(pchip->dev, &pchip->ledcdev);
		if (rval) {
			dev_err(pchip->dev, "unable to register %s,rc=%d\n",
				lm3630a_led_cdev.name, rval);
			return rval;
		}
		pchip->ledwq = create_singlethread_workqueue("lm3630a-led-wq");
		if (!pchip->ledwq) {
			dev_err(pchip->dev, "fail to create led thread\n");
			rval = -ENOMEM;
			goto err1;
		}
	}

	/* pwm */
	if (pdata->pwm_ctrl != LM3630A_PWM_DISABLE) {
		pchip->pwmd = pwm_request(pdata->pwm_gpio, "lm3630a-pwm");
		if (IS_ERR(pchip->pwmd)) {
			dev_err(&client->dev, "fail : get pwm device\n");
			rval = PTR_ERR(pchip->pwmd);
			goto err1;
		}
	}

	/* interrupt enable  : irq 0 is not allowed */
	if (pchip->irq) {
		rval = lm3630a_intr_config(pchip);
		if (rval < 0)
			goto err2;
	}

	dev_info(&client->dev, "LM3630A backlight register OK.\n");
	return 0;

err2:
	if (!IS_ERR_OR_NULL(pchip->pwmd))
		pwm_free(pchip->pwmd);
	if (pchip->irq)
		free_irq(pchip->irq, pchip);
	if (pchip->irqthread) {
		flush_workqueue(pchip->irqthread);
		destroy_workqueue(pchip->irqthread);
	}
err1:
	if (is_fb_backlight) {
		if (!IS_ERR_OR_NULL(pchip->bleda))
			backlight_device_unregister(pchip->bleda);
		if (!IS_ERR_OR_NULL(pchip->bledb))
			backlight_device_unregister(pchip->bledb);
	} else
		led_classdev_unregister(&pchip->ledcdev);

	return rval;
}

static int lm3630a_remove(struct i2c_client *client)
{
	int rval;
	struct lm3630a_chip *pchip = i2c_get_clientdata(client);

	rval = lm3630a_write(pchip, REG_BRT_A, 0);
	if (rval < 0)
		dev_err(pchip->dev, "i2c failed to access register\n");

	rval = lm3630a_write(pchip, REG_BRT_B, 0);
	if (rval < 0)
		dev_err(pchip->dev, "i2c failed to access register\n");

	if (pchip->irq) {
		free_irq(pchip->irq, pchip);
		flush_workqueue(pchip->irqthread);
		destroy_workqueue(pchip->irqthread);
	}

	if (!IS_ERR_OR_NULL(pchip->pwmd))
		pwm_free(pchip->pwmd);

	lm3630a_chip_disable(pchip);

	if (is_fb_backlight) {
		if (!IS_ERR_OR_NULL(pchip->bleda))
			backlight_device_unregister(pchip->bleda);
		if (!IS_ERR_OR_NULL(pchip->bledb))
			backlight_device_unregister(pchip->bledb);
	} else {
		destroy_workqueue(pchip->ledwq);
		led_classdev_unregister(&pchip->ledcdev);
	}

	return 0;
}

static const struct i2c_device_id lm3630a_id[] = {
	{LM3630A_NAME, 0},
	{}
};

static struct of_device_id lm3630a_match_table[] = {
	{ .compatible = "ti,lm3630a",},
	{ },
};

MODULE_DEVICE_TABLE(i2c, lm3630a_id);

static struct i2c_driver lm3630a_i2c_driver = {
	.driver = {
		.name = LM3630A_NAME,
		.of_match_table = lm3630a_match_table,
	},
	.probe = lm3630a_probe,
	.remove = lm3630a_remove,
	.id_table = lm3630a_id,
};

module_i2c_driver(lm3630a_i2c_driver);

MODULE_DESCRIPTION("Texas Instruments Backlight driver for LM3630A");
MODULE_AUTHOR("Daniel Jeong <gshark.jeong@gmail.com>");
MODULE_AUTHOR("LDD MLP <ldd-mlp@list.ti.com>");
MODULE_LICENSE("GPL v2");
