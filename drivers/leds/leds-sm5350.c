/*
 * SM5350 BL Driver
 *
 * SiliconMitus SM5350 Backlight driver chip
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#include <linux/i2c.h>
#include <linux/leds.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#if defined(CONFIG_FB)
#include <linux/notifier.h>
#include <linux/fb.h>
#endif

#define SM5350_LED_DEV "SM5350-BL"
#define SM5350_NAME "sm5350-bl"

#define MAX_BRIGHTNESS			(2047)
#define BANK_NONE				0x00
#define BANK_A					0x01
#define BANK_B					0x02

#define SM5350_REVISION_REG						0x00
#define SM5350_SW_RESET_REG						0x01
#define SM5350_HVLED_CURR_SINK_OUT_CFG_REG		0x10
#define SM5350_CTL_A_RAMP_TIME_REG				0x11
#define SM5350_CTL_B_RAMP_TIME_REG				0x12
#define SM5350_CTL_RUNTIME_RAMP_TIME_REG		0x13
#define SM5350_CTL_RUNTIME_RAMP_CFG_REG			0x14
#define SM5350_BRIGHTNESS_CFG_REG				0x16
#define SM5350_CTL_A_FULL_SCALE_CURR_REG		0x17
#define SM5350_CTL_B_FULL_SCALE_CURR_REG		0x18
#define SM5350_HVLED_CURR_SINK_FEEDBACK_REG		0x19
#define SM5350_BOOST_CTL_REG					0x1A
#define SM5350_AUTO_FREQ_THRESHOLD_REG			0x1B
#define SM5350_PWM_CFG_REG						0x1C
#define SM5350_CTL_A_BRIGHTNESS_LSB_REG			0x20
#define SM5350_CTL_A_BRIGHTNESS_MSB_REG			0x21
#define SM5350_CTL_B_BRIGHTNESS_LSB_REG			0x22
#define SM5350_CTL_B_BRIGHTNESS_MSB_REG			0x23
#define SM5350_CTL_BANK_EN_REG				0x24
#define SM5350_HVLED_OPEN_FAULTS_REG			0xB0
#define SM5350_HVLED_SHORT_FAULTS_REG			0xB2
#define SM5350_LED_FAULT_ENABLES_REG			0xB4

struct sm5350_data {
	struct led_classdev led_dev;
	struct i2c_client *client;
	struct i2c_adapter *adapter;
	unsigned short addr;
	struct mutex		lock;
	struct work_struct	work;
	enum led_brightness brightness;
#ifdef CONFIG_FB
	struct notifier_block fb_notif;
#endif
	bool enable;
	bool bank_A;
	bool bank_B;
	u8 ctl_bank_en;
	u8 map_mode;
	u8 pwm_cfg;
	u8 boost_ctl;
	u8 full_scale_current;
	bool brt_code_enable;
	u16 *brt_code_table;
};

static int platform_read_i2c_block(struct i2c_client *client, char *writebuf,
			   int writelen, char *readbuf, int readlen)
{
	int ret;

	if (writelen > 0) {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = 0,
				 .len = writelen,
				 .buf = writebuf,
			 },
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 2);
		if (ret < 0)
			dev_err(&client->dev, "%s: i2c read error.\n",
				__func__);
	} else {
		struct i2c_msg msgs[] = {
			{
				 .addr = client->addr,
				 .flags = I2C_M_RD,
				 .len = readlen,
				 .buf = readbuf,
			 },
		};
		ret = i2c_transfer(client->adapter, msgs, 1);
		if (ret < 0)
			dev_err(&client->dev, "%s:i2c read error.\n", __func__);
	}
	return ret;
}

static int sm5350_read_reg(struct i2c_client *client, u8 addr, u8 *val)
{
	return platform_read_i2c_block(client, &addr, 1, val, 1);
}

static int platform_write_i2c_block(struct i2c_client *client, char *writebuf, int writelen)
{
	int ret;

	struct i2c_msg msgs[] = {
		{
			 .addr = client->addr,
			 .flags = 0,
			 .len = writelen,
			 .buf = writebuf,
		 },
	};
	ret = i2c_transfer(client->adapter, msgs, 1);
	if (ret < 0)
		dev_err(&client->dev, "%s: i2c write error.\n", __func__);

	return ret;
}

static int sm5350_write_reg(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;
	return platform_write_i2c_block(client, buf, sizeof(buf));
}

static int sm5350_init_registers(struct sm5350_data *drvdata)
{
	int err = 0;
	int current_sink_out_cfg = 7;

	sm5350_write_reg(drvdata->client, SM5350_BOOST_CTL_REG, drvdata->boost_ctl);
	sm5350_write_reg(drvdata->client, SM5350_PWM_CFG_REG, drvdata->pwm_cfg);
	sm5350_write_reg(drvdata->client, SM5350_CTL_BANK_EN_REG, drvdata->ctl_bank_en);
	if(drvdata->bank_B) {
		sm5350_write_reg(drvdata->client, SM5350_CTL_B_FULL_SCALE_CURR_REG, drvdata->full_scale_current);
		current_sink_out_cfg = 7;
	}else {
		sm5350_write_reg(drvdata->client, SM5350_CTL_A_FULL_SCALE_CURR_REG, drvdata->full_scale_current);
		current_sink_out_cfg = 0;
	}
	sm5350_write_reg(drvdata->client,  SM5350_HVLED_CURR_SINK_OUT_CFG_REG, current_sink_out_cfg);
	sm5350_write_reg(drvdata->client, SM5350_BRIGHTNESS_CFG_REG, drvdata->map_mode);

	drvdata->enable = true;

	return err;
}

#if defined(CONFIG_FB)
static int fb_notifier_callback(struct notifier_block *self,
				       unsigned long event, void *data)
{
	int *blank;
	struct fb_event *evdata = data;
	struct sm5350_data *drvdata =
		container_of(self, struct sm5350_data, fb_notif);

	/*
	 *  FB_EVENT_BLANK(0x09): A hardware display blank change occurred.
	 *  FB_EARLY_EVENT_BLANK(0x10): A hardware display blank early change
	 * occurred.
	 */
	if (evdata && evdata->data && (event == FB_EARLY_EVENT_BLANK)) {
		blank = evdata->data;
		if (*blank == FB_BLANK_POWERDOWN)
			drvdata->enable = false;
	}

	return NOTIFY_OK;
}
#endif

void sm5350_set_brightness(struct sm5350_data *drvdata, int brt_val)
{

	u8 brt_LSB = 0;
	u8 brt_MSB = 0;
	int index = 0, remainder;
	int code, code1, code2;

	if (drvdata->enable == false)
		sm5350_init_registers(drvdata);

	if (drvdata->brt_code_enable) {
		index = brt_val / 10;
		remainder = brt_val % 10;

		code1 = drvdata->brt_code_table[index];
		code2 = drvdata->brt_code_table[index+1];

		code = (code2 - code1) * remainder / 10 + code1;

		brt_LSB = code & 0x7;
		brt_MSB = (code >> 3) & 0xFF;
	} else {
		brt_LSB = brt_val & 0x7;
		brt_MSB = (brt_val >> 3) & 0xFF;
	}

	if (drvdata->bank_B) {
		sm5350_write_reg(drvdata->client, SM5350_CTL_B_BRIGHTNESS_LSB_REG, brt_LSB);
		sm5350_write_reg(drvdata->client, SM5350_CTL_B_BRIGHTNESS_MSB_REG, brt_MSB);
	}else {
		sm5350_write_reg(drvdata->client, SM5350_CTL_A_BRIGHTNESS_LSB_REG, brt_LSB);
		sm5350_write_reg(drvdata->client, SM5350_CTL_A_BRIGHTNESS_MSB_REG, brt_MSB);
	}

	drvdata->brightness = brt_val;

	if (drvdata->brightness == 0)
		drvdata->enable = false;

}

static int sm5350_check_id(struct sm5350_data *drvdata)
{
	u8 value = 0;
	int err = 0;
	sm5350_read_reg(drvdata->client, 0x00, &value);
	if (value != 0x00) {
		pr_err("%s : ID check err\n", __func__);
		err = -EINVAL;
		return err;
	}

	return err;
}

static void dump_sm5350_regs(struct sm5350_data *drvdata)
{
	u8 brt_LSB = 0;
	u8 brt_MSB = 0;
	u8 boost_ctl, pwm_cfg, ctl_bank_en, full_scale_current, sink_out_cfg;

	sm5350_read_reg(drvdata->client, SM5350_HVLED_CURR_SINK_OUT_CFG_REG, &sink_out_cfg);
	sm5350_read_reg(drvdata->client, SM5350_BOOST_CTL_REG, &boost_ctl);
	sm5350_read_reg(drvdata->client, SM5350_PWM_CFG_REG, &pwm_cfg);
	sm5350_read_reg(drvdata->client, SM5350_CTL_BANK_EN_REG, &ctl_bank_en);
	if(drvdata->bank_B) {
		sm5350_read_reg(drvdata->client, SM5350_CTL_B_FULL_SCALE_CURR_REG, &full_scale_current);
		sm5350_read_reg(drvdata->client, SM5350_CTL_B_BRIGHTNESS_LSB_REG, &brt_LSB);
		sm5350_read_reg(drvdata->client, SM5350_CTL_B_BRIGHTNESS_MSB_REG, &brt_MSB);
	}else {
		sm5350_read_reg(drvdata->client, SM5350_CTL_A_FULL_SCALE_CURR_REG, &full_scale_current);
		sm5350_read_reg(drvdata->client, SM5350_CTL_A_BRIGHTNESS_LSB_REG, &brt_LSB);
		sm5350_read_reg(drvdata->client, SM5350_CTL_A_BRIGHTNESS_MSB_REG, &brt_MSB);
	}

	pr_err(">>-- boost_ctl(0x%x), pwm_cfg(0x%x), ctl_bank_en(0x%x), full_scale_current(0x%x), brt_LSB(0x%x), brt_MSB(0x%x), sink_out_cfg(0x%x).\n",
		boost_ctl, pwm_cfg, ctl_bank_en, full_scale_current, brt_LSB, brt_MSB, sink_out_cfg);
}

static void __sm5350_work(struct sm5350_data *led,
				enum led_brightness value)
{
	mutex_lock(&led->lock);
	sm5350_set_brightness(led, value);
	mutex_unlock(&led->lock);
}

static void sm5350_work(struct work_struct *work)
{
	struct sm5350_data *drvdata = container_of(work,
					struct sm5350_data, work);

	__sm5350_work(drvdata, drvdata->led_dev.brightness);

	return;
}


static void sm5350_brightness_set(struct led_classdev *led_cdev,
				     enum led_brightness brt_val)
{
	struct sm5350_data *drvdata;

	drvdata = container_of(led_cdev, struct sm5350_data, led_dev);

	schedule_work(&drvdata->work);
}

static int sm5350_get_dt_data(struct device *dev, struct sm5350_data *drvdata)
{
	int rc;
	u32 tmp;
	struct device_node *of_node = NULL;
	int len;
	const char *data;
	u32 *buf;
	int i = 0;

	of_node = dev->of_node;

	rc = of_property_read_u32(of_node, "boost-ctl", &tmp);
	if (rc) {
		pr_err("%s:%d, dt not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	drvdata->boost_ctl = (!rc ? tmp : 0);
	pr_debug("%s : boost_ctl=0x%x\n",__func__, drvdata->boost_ctl);

	rc = of_property_read_u32(of_node, "pwm-cfg", &tmp);
	if (rc) {
		pr_err("%s:%d, dt not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	drvdata->pwm_cfg = (!rc ? tmp : 0);
	pr_debug("%s : pwm_cfg=0x%x\n",__func__, drvdata->pwm_cfg);

	rc = of_property_read_u32(of_node, "ctl-bank-en", &tmp);
	if (rc) {
		pr_err("%s:%d, dt not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	drvdata->ctl_bank_en = (!rc ? tmp : 0);
	pr_debug("%s : ctl_bank_en=0x%x\n",__func__, drvdata->ctl_bank_en);

	if (drvdata->ctl_bank_en & 0x01)
		drvdata->bank_A = true;
	if (drvdata->ctl_bank_en & 0x02)
		drvdata->bank_B = true;

	pr_debug("%s : bank_A=%d bank_B=%d\n",__func__, drvdata->bank_A, drvdata->bank_B);

	rc = of_property_read_u32(of_node, "map-mode", &tmp);
	if (rc) {
		pr_err("%s:%d, dt not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	drvdata->map_mode = (!rc ? tmp : 0);
	pr_debug("%s : map_mode=0x%x\n",__func__, drvdata->map_mode);

	rc = of_property_read_u32(of_node, "full-scale-current", &tmp);
	if (rc) {
		pr_err("%s:%d, dt not specified\n",
						__func__, __LINE__);
		return -EINVAL;
	}
	drvdata->full_scale_current = (!rc ? tmp : 0);

	pr_info("bank_A = %d, bank_B = %d, pwm_cfg = 0x%x, full_scale_current = 0x%x, boost_ctl = 0x%x.\n",
		drvdata->bank_A, drvdata->bank_B, drvdata->pwm_cfg, drvdata->full_scale_current, drvdata->boost_ctl);

	drvdata->brt_code_enable = of_property_read_bool(of_node, "brt-code-enable");

	if (drvdata->brt_code_enable == false) {
		pr_info("%s : brt_code_enable = %d, rc = %d.\n",__func__, drvdata->brt_code_enable, rc);
		return rc;
	}

	data = of_get_property(of_node, "brt-code-table", &len);
	if (!data) {
		pr_err("%s: read brt-code-table failed\n", __func__);
		return -ENOMEM;
	}

	len /= sizeof(u32);

	drvdata->led_dev.max_brightness = 10 * (len - 1);
	pr_debug("%s : max_brightness=%d\n",__func__, drvdata->led_dev.max_brightness);

	buf = kzalloc(len * sizeof(u32), GFP_KERNEL);
	if (!buf)
		return -ENOMEM;

	rc = of_property_read_u32_array(of_node, "brt-code-table", buf, len);
	if (rc) {
		pr_err("%s:%d, dt not specified\n",__func__, __LINE__);
		rc = -EINVAL;
		goto end;
	}

	drvdata->brt_code_table = kzalloc(len * sizeof(u16), GFP_KERNEL);
	if (!drvdata->brt_code_table) {
		pr_err("%s:%d, allocate memory failed\n",__func__, __LINE__);
		rc = -ENOMEM;
		goto end;
	}

	for (i=0; i < len; i++) {
		drvdata->brt_code_table[i] = (u16) buf[i];
		pr_debug("%s : buf=%d i=%d\n",__func__, buf[i], i);
	}
end:
	kfree(buf);
	return rc;
}

static int sm5350_probe(struct i2c_client *client,
			   const struct i2c_device_id *id)
{
	struct sm5350_data *drvdata;
	int err = 0;

	if (!i2c_check_functionality(client->adapter, I2C_FUNC_I2C)) {
		pr_err("%s : I2C_FUNC_I2C not supported\n", __func__);
		err = -EIO;
		goto err_out;
	}

	if (!client->dev.of_node) {
		pr_err("%s : no device node\n", __func__);
		err = -ENOMEM;
		goto err_out;
	}

	drvdata = kzalloc(sizeof(struct sm5350_data), GFP_KERNEL);
	if (drvdata == NULL) {
		pr_err("%s : kzalloc failed\n", __func__);
		err = -ENOMEM;
		goto err_out;
	}

	drvdata->client = client;
	drvdata->adapter = client->adapter;
	client->addr = 0x36;
	drvdata->addr = client->addr;
	drvdata->brightness = LED_OFF;
	drvdata->enable = true;
	drvdata->led_dev.default_trigger = "bkl-trigger";
	drvdata->led_dev.name = SM5350_LED_DEV;
	drvdata->led_dev.brightness_set = sm5350_brightness_set;
	drvdata->led_dev.max_brightness = MAX_BRIGHTNESS;

	err = sm5350_get_dt_data(&client->dev, drvdata);
	if(err < 0) {
		pr_err("%s : get dt failed\n", __func__);
		err = -ENOMEM;
		goto err_init;
	}

	i2c_set_clientdata(client, drvdata);

	err = sm5350_check_id(drvdata);
	if (err < 0) {
		pr_err("%s : ID idenfy failed\n", __func__);
		goto err_init;
	}

	mutex_init(&drvdata->lock);
	INIT_WORK(&drvdata->work, sm5350_work);

	err = led_classdev_register(&client->dev, &drvdata->led_dev);
	if (err < 0) {
		pr_err("%s : Register led class failed\n", __func__);
		err = -ENODEV;
		goto err_init;
	}
	sm5350_init_registers(drvdata);
	dump_sm5350_regs(drvdata);

#if defined(CONFIG_FB)
	drvdata->fb_notif.notifier_call = fb_notifier_callback;
	err = fb_register_client(&drvdata->fb_notif);
	if (err)
		pr_err("%s : Unable to register fb_notifier: %d\n", __func__, err);
#endif

	return 0;

err_init:
	kfree(drvdata);
err_out:
	return err;
}

static int sm5350_remove(struct i2c_client *client)
{
	struct sm5350_data *drvdata = i2c_get_clientdata(client);

	led_classdev_unregister(&drvdata->led_dev);

	kfree(drvdata);
	return 0;
}

static const struct i2c_device_id sm5350_id[] = {
	{SM5350_NAME, 0},
	{}
};
static struct of_device_id match_table[] = {
       {.compatible = "sm-sm5350",}
};

MODULE_DEVICE_TABLE(i2c, sm5350_id);

static struct i2c_driver sm5350_i2c_driver = {
	.probe = sm5350_probe,
	.remove = sm5350_remove,
	.id_table = sm5350_id,
	.driver = {
		.name = SM5350_NAME,
		.owner = THIS_MODULE,
		.of_match_table = match_table,
	},
};

module_i2c_driver(sm5350_i2c_driver);
MODULE_DESCRIPTION("Back Light driver for SM5350");
MODULE_LICENSE("GPL v2");
