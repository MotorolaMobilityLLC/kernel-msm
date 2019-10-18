/*
* aw99703.c   aw99703 backlight module
*
* Version: v1.0.4
*
* Copyright (c) 2019 AWINIC Technology CO., LTD
*
*  Author: Joseph <zhangzetao@awinic.com.cn>
*
* This program is free software; you can redistribute  it and/or modify it
* under  the terms of  the GNU General  Public License as published by the
* Free Software Foundation;  either version 2 of the  License, or (at your
* option) any later version.
*/

#include <linux/i2c.h>
#include <linux/slab.h>
#include <linux/input.h>
#include <linux/types.h>
#include <linux/regulator/consumer.h>
#include <linux/module.h>
#include <linux/version.h>
#include <linux/of_gpio.h>
#include <linux/delay.h>
#include <linux/backlight.h>
#include <linux/platform_data/aw99703_bl.h>

#define AW99703_NAME "aw99703-bl"

#define AW99703_VERSION "v1.0.6"

struct aw99703_data *g_aw99703_data;

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

static int aw99703_i2c_read(struct i2c_client *client, u8 addr, u8 *val)
{
	return platform_read_i2c_block(client, &addr, 1, val, 1);
}

static int platform_write_i2c_block(struct i2c_client *client,
		char *writebuf, int writelen)
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

static int aw99703_i2c_write(struct i2c_client *client, u8 addr, const u8 val)
{
	u8 buf[2] = {0};

	buf[0] = addr;
	buf[1] = val;

	return platform_write_i2c_block(client, buf, sizeof(buf));
}

static int aw99703_gpio_init(struct aw99703_data *drvdata)
{

	int ret;

	if (gpio_is_valid(drvdata->hwen_gpio)) {
		ret = gpio_request(drvdata->hwen_gpio, "hwen_gpio");
		if (ret < 0) {
			pr_err("failed to request gpio\n");
			return -1;
		}
		ret = gpio_direction_output(drvdata->hwen_gpio, 1);
		pr_info(" request gpio init\n");
		if (ret < 0) {
			pr_err("failed to set output");
			gpio_free(drvdata->hwen_gpio);
			return ret;
		}
		pr_info("gpio is valid!\n");
	}

	return 0;
}

static int aw99703_i2c_write_bit(struct i2c_client *client,
	unsigned int reg_addr, unsigned int  mask, unsigned char reg_data)
{
	unsigned char reg_val = 0;

	aw99703_i2c_read(client, reg_addr, &reg_val);
	reg_val &= mask;
	reg_val |= reg_data;
	aw99703_i2c_write(client, reg_addr, reg_val);

	return 0;
}

static int aw99703_brightness_map(unsigned int level)
{
	/*MAX_LEVEL_256*/
	if (g_aw99703_data->bl_map == 1) {
		if (level == 255)
			return 2047;
		return level * 8;
	}

	/*MAX_LEVEL_1024*/
	if (g_aw99703_data->bl_map == 2)
		return level * 2;
	/*MAX_LEVEL_2048*/
	if (g_aw99703_data->bl_map == 3)
		return level;

	return  level;
}

static int aw99703_bl_enable_channel(struct aw99703_data *drvdata)
{
	int ret = 0;

	if (drvdata->channel == 3) {
		pr_info("%s turn all channel on!\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH3_ENABLE |
						AW99703_LEDCUR_CH2_ENABLE |
						AW99703_LEDCUR_CH1_ENABLE);
	} else if (drvdata->channel == 2) {
		pr_info("%s turn two channel on!\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH2_ENABLE |
						AW99703_LEDCUR_CH1_ENABLE);
	} else if (drvdata->channel == 1) {
		pr_info("%s turn one channel on!\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						AW99703_LEDCUR_CH1_ENABLE);
	} else {
		pr_info("%s all channels are going to be disabled\n", __func__);
		ret = aw99703_i2c_write_bit(drvdata->client,
						AW99703_REG_LEDCUR,
						AW99703_LEDCUR_CHANNEL_MASK,
						0x98);
	}

	return ret;
}

static void aw99703_pwm_mode_enable(struct aw99703_data *drvdata)
{
	if (drvdata->pwm_mode) {
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_PDIS_MASK,
					AW99703_MODE_PDIS_ENABLE);
		pr_info("%s pwm_mode is enable\n", __func__);
	} else {
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_PDIS_MASK,
					AW99703_MODE_PDIS_DISABLE);
		pr_info("%s pwm_mode is disable\n", __func__);
	}
}


static void aw99703_pwm_maptype_set(struct aw99703_data *drvdata)
{
	pr_info("%s enter\n", __func__);
	/*mode:map type*/
	if(drvdata->maptype == 1)
		aw99703_i2c_write_bit(drvdata->client,
				      AW99703_REG_MODE,
				      AW99703_MODE_MAP_MASK,
				      AW99703_MODE_MAP_LINEAR);
	else
		aw99703_i2c_write_bit(drvdata->client,
				      AW99703_REG_MODE,
				      AW99703_MODE_MAP_MASK,
				      AW99703_MODE_MAP_EXPONENTIAL);

}

static void aw99703_ramp_setting(struct aw99703_data *drvdata)
{
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TURNCFG,
				AW99703_TURNCFG_ON_TIM_MASK,
				drvdata->ramp_on_time << 4);
	pr_info("%s drvdata->ramp_on_time is 0x%x\n",
		__func__, drvdata->ramp_on_time);

	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TURNCFG,
				AW99703_TURNCFG_OFF_TIM_MASK,
				drvdata->ramp_off_time);
	pr_info("%s drvdata->ramp_off_time is 0x%x\n",
		__func__, drvdata->ramp_off_time);

}

static void aw99703_flash_setting(struct aw99703_data *drvdata)
{
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_FLASH,
				AW99703_FLASH_FLTO_TIM_MASK,
				drvdata->flto_tim << 4);
	pr_info("%s drvdata->flto_tim is 0x%x\n",
		__func__, drvdata->flto_tim);

	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_FLASH,
				AW99703_FLASH_FL_S_TIM_MASK,
				drvdata->fl_s);
	pr_info("%s drvdata->fl_s is 0x%x\n",
		__func__, drvdata->fl_s);

}

static void aw99703_transition_ramp(struct aw99703_data *drvdata)
{
	pr_info("%s enter\n", __func__);
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TRANCFG,
				AW99703_TRANCFG_PWM_TIM_MASK,
				drvdata->pwm_trans_dim << 4);
	pr_info("%s drvdata->pwm_trans_dim is 0x%x\n", __func__,
		drvdata->pwm_trans_dim);

	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_TRANCFG,
				AW99703_TRANCFG_I2C_TIM_MASK,
				drvdata->i2c_trans_dim);
	pr_info("%s drvdata->i2c_trans_dim is 0x%x\n",
		__func__, drvdata->i2c_trans_dim);
}

static void aw99703_bstctrl1_set(struct aw99703_data *drvdata)
{
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_SF_SFT_MASK,
				drvdata->sf_sft << 6);

	/*switch frequency*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_SF_MASK,
				drvdata->sf << 5);

	/*default OVPSEL*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_OVPSEL_MASK,
				drvdata->ovp_sel << 2);

	/*OCP SELECT*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR1,
				AW99703_BSTCTR1_OCPSEL_MASK,
				drvdata->ocp_sel << 0);
}


static void aw99703_pwm_config_set(struct aw99703_data *drvdata)
{
	/*Pwm sampler frequency*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_PWM,
				AW99703_PWM_P_SF_MASK,
				drvdata->p_sf << 6);

	/*Pwm hysteresis*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_PWM,
				AW99703_PWM_P_HYS_MASK,
				drvdata->p_hys << 2);

	/*Pwm filter*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_PWM,
				AW99703_PWM_P_FLT_MASK,
				drvdata->p_flt << 0);
}

static int aw99703_backlight_init(struct aw99703_data *drvdata)
{
	pr_info("%s enter.\n", __func__);

	aw99703_pwm_mode_enable(drvdata);

	aw99703_pwm_maptype_set(drvdata);

	aw99703_bstctrl1_set(drvdata);

	/*BSTCRT2 IDCTSEL*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR2,
				AW99703_BSTCTR2_IDCTSEL_MASK,
				drvdata->idctsel << 6);

	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_BSTCTR2,
				AW99703_BSTCTR2_EMISEL_MASK,
				drvdata->emisel << 3);

	aw99703_pwm_config_set(drvdata);

	/*Backlight current full scale*/
	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_LEDCUR,
				AW99703_LEDCUR_BLFS_MASK,
				drvdata->full_scale_led << 3);

	aw99703_bl_enable_channel(drvdata);

	aw99703_ramp_setting(drvdata);
	aw99703_transition_ramp(drvdata);
	aw99703_flash_setting(drvdata);

	aw99703_i2c_write(drvdata->client, AW99703_REG_WPRT1, (char)drvdata->wprt1);
	aw99703_i2c_write(drvdata->client, AW99703_REG_BSTCTR3, (char)drvdata->bstcrt3);
	aw99703_i2c_write(drvdata->client, AW99703_REG_BSTCTR4, (char)drvdata->bstcrt4);
	aw99703_i2c_write(drvdata->client, AW99703_REG_BSTCTR5, (char)drvdata->bstcrt5);

	return 0;
}

static int aw99703_backlight_enable(struct aw99703_data *drvdata)
{
	pr_info("%s enter.\n", __func__);

	aw99703_i2c_write_bit(drvdata->client,
				AW99703_REG_MODE,
				AW99703_MODE_WORKMODE_MASK,
				AW99703_MODE_WORKMODE_BACKLIGHT);

	drvdata->enable = true;

	return 0;
}

int  aw99703_set_brightness(struct aw99703_data *drvdata, int brt_val)
{
	pr_info("%s brt_val is %d\n", __func__, brt_val);

	if ((drvdata->enable == false) && (brt_val != 0)) {
		aw99703_backlight_init(drvdata);
		aw99703_backlight_enable(drvdata);
	}

	brt_val = aw99703_brightness_map(brt_val);

	if (brt_val > 0) {
		/*enalbe bl mode*/
		/* set backlight brt_val */
		aw99703_i2c_write(drvdata->client,
				AW99703_REG_LEDLSB,
				brt_val&0x0007);
		aw99703_i2c_write(drvdata->client,
				AW99703_REG_LEDMSB,
				(brt_val >> 3)&0xff);

		/* backlight enable */
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_BACKLIGHT);
	} else {
		/* standby mode*/
		aw99703_i2c_write_bit(drvdata->client,
					AW99703_REG_MODE,
					AW99703_MODE_WORKMODE_MASK,
					AW99703_MODE_WORKMODE_STANDBY);
	}

	drvdata->brightness = brt_val;

	if (drvdata->brightness == 0)
		drvdata->enable = false;
	return 0;
}

static int aw99703_bl_get_brightness(struct backlight_device *bl_dev)
{
		return bl_dev->props.brightness;
}

static int aw99703_bl_update_status(struct backlight_device *bl_dev)
{
		struct aw99703_data *drvdata = bl_get_data(bl_dev);
		int brt;

		if (bl_dev->props.state & BL_CORE_SUSPENDED)
				bl_dev->props.brightness = 0;

		brt = bl_dev->props.brightness;
		/*
		 * Brightness register should always be written
		 * not only register based mode but also in PWM mode.
		 */
		return aw99703_set_brightness(drvdata, brt);
}

static const struct backlight_ops aw99703_bl_ops = {
		.update_status = aw99703_bl_update_status,
		.get_brightness = aw99703_bl_get_brightness,
};

static int aw99703_read_chipid(struct aw99703_data *drvdata)
{
	int ret = -1;
	u8 value = 0;
	unsigned char cnt = 0;

	while (cnt < AW_READ_CHIPID_RETRIES) {
		ret = aw99703_i2c_read(drvdata->client, 0x00, &value);
		if (ret < 0) {
			pr_err("%s: failed to read reg AW99703_REG_ID: %d\n",
				__func__, ret);
		}
		switch (value) {
		case 0x03:
			pr_info("%s aw99703 detected\n", __func__);
			return 0;
		default:
			pr_info("%s unsupported device revision (0x%x)\n",
				__func__, value);
			break;
		}
		cnt++;

		msleep(AW_READ_CHIPID_RETRY_DELAY);
	}

	return -EINVAL;
}

static void
aw99703_get_dt_data(struct device *dev, struct aw99703_data *drvdata)
{
	int rc;
	struct device_node *np = dev->of_node;
	u32 bl_channel, temp;

	drvdata->hwen_gpio = of_get_named_gpio(np, "aw99703,hwen-gpio", 0);
	pr_info("%s drvdata->hwen_gpio --<%d>\n", __func__, drvdata->hwen_gpio);

	rc = of_property_read_u32(np, "aw99703,pwm-mode", &drvdata->pwm_mode);
	if (rc) {
		drvdata->pwm_mode = 0;
		pr_err("%s pwm-mode not found\n", __func__);
	} else {
		pr_info("%s pwm_mode=%d\n", __func__, drvdata->pwm_mode);
	}

	rc = of_property_read_u32(np, "aw99703,pwm-maptype", &drvdata->maptype);
	if (rc) {
		drvdata->maptype = 0;
		pr_err("%s pwm-maptype not found\n", __func__);
	} else {
		pr_info("%s pwm-maptype=%d\n", __func__, drvdata->maptype);
	}

	rc = of_property_read_u32(np, "aw99703,bl-fscal-led", &temp);
	if (rc) {
		drvdata->full_scale_led = 0x13;
		pr_err("Invalid backlight full-scale led current!\n");
	} else {
		drvdata->full_scale_led = temp;
		pr_info("%s full-scale led current --<0x%x>\n",
			__func__, drvdata->full_scale_led);
	}

	rc = of_property_read_u32(np, "aw99703,bl-channel", &bl_channel);
	if (rc) {
		drvdata->channel = 3;
		pr_err("Invalid channel setup\n");
	} else {
		drvdata->channel = bl_channel;
		pr_info("%s bl-channel --<%x>\n", __func__, drvdata->channel);
	}

	rc = of_property_read_u32(np, "aw99703,sf-sft", &drvdata->sf_sft);
	if (rc) {
		drvdata->sf_sft = 0;
		pr_err("%s sf-sft not found\n", __func__);
	} else {
		pr_info("%s sf-sft=%d\n", __func__, drvdata->sf_sft);
	}

	rc = of_property_read_u32(np, "aw99703,switching-frequency", &drvdata->sf);
	if (rc) {
		drvdata->sf = 1;
		pr_err("%s switching-frequency not found\n", __func__);
	} else {
		pr_info("%s switching-frequency=%d\n", __func__, drvdata->sf);
	}

	rc = of_property_read_u32(np, "aw99703,ovp-sel", &drvdata->ovp_sel);
	if (rc) {
		drvdata->ovp_sel = 3;
		pr_err("%s ovp-sel not found\n", __func__);
	} else {
		pr_info("%s ovp-sel = 0x%x\n", __func__, drvdata->ovp_sel);
	}

	rc = of_property_read_u32(np, "aw99703,ocp-sel", &drvdata->ocp_sel);
	if (rc) {
		drvdata->ocp_sel = 2;
		pr_err("%s ocp-sel not found\n", __func__);
	} else {
		pr_info("%s ocp-sel = 0x%x\n", __func__, drvdata->ocp_sel);
	}

	rc = of_property_read_u32(np, "aw99703,idctsel", &drvdata->idctsel);
	if (rc) {
		drvdata->idctsel = 0;
		pr_err("%s idctsel not found\n", __func__);
	} else {
		pr_info("%s idctsel = %d\n", __func__, drvdata->idctsel);
	}

	rc = of_property_read_u32(np, "aw99703,emisel", &drvdata->emisel);
	if (rc) {
		drvdata->emisel = 0;
		pr_err("%s emisel not found\n", __func__);
	} else {
		pr_info("%s emisel = %d\n", __func__, drvdata->emisel);
	}

	rc = of_property_read_u32(np, "aw99703,pwm-frequency", &drvdata->p_sf);
	if (rc) {
		drvdata->p_sf = 2;
		pr_err("%s pwm-frequency not found\n", __func__);
	} else {
		pr_info("%s pwm-frequency = %d\n", __func__, drvdata->p_sf);
	}

	rc = of_property_read_u32(np, "aw99703,pwm-hysteresis", &drvdata->p_hys);
	if (rc) {
		drvdata->p_hys = 4;
		pr_err("%s pwm-hysteresis not found\n", __func__);
	} else {
		pr_info("%s pwm-hysteresis = %d\n", __func__, drvdata->p_hys);
	}

	rc = of_property_read_u32(np, "aw99703,pwm-filter", &drvdata->p_flt);
	if (rc) {
		drvdata->p_flt = 3;
		pr_err("%s pwm-filter not found\n", __func__);
	} else {
		pr_info("%s pwm-filter = %d\n", __func__, drvdata->p_flt);
	}

	rc = of_property_read_u32(np, "aw99703,turn-on-ramp", &temp);
	if (rc) {
		drvdata->ramp_on_time = 4;
		pr_err("Invalid ramp timing ,turnon!\n");
	} else {
		drvdata->ramp_on_time = temp;
		pr_info("%s ramp on time --<0x%x>\n",
			__func__, drvdata->ramp_on_time);
	}

	rc = of_property_read_u32(np, "aw99703,turn-off-ramp", &temp);
	if (rc) {
		drvdata->ramp_off_time = 4;
		pr_err("Invalid ramp timing ,,turnoff!\n");
	} else {
		drvdata->ramp_off_time = temp;
		pr_info("%s ramp off time --<0x%x>\n",
			__func__, drvdata->ramp_off_time);
	}

	rc = of_property_read_u32(np, "aw99703,pwm-trans-dim", &temp);
	if (rc) {
		drvdata->pwm_trans_dim = 0;
		pr_err("Invalid pwm-tarns-dim value!\n");
	} else {
		drvdata->pwm_trans_dim = temp;
		pr_info("%s pwm trnasition dimming--<%d>\n",
			__func__, drvdata->pwm_trans_dim);
	}

	rc = of_property_read_u32(np, "aw99703,i2c-trans-dim", &temp);
	if (rc) {
		drvdata->i2c_trans_dim = 0;
		pr_err("Invalid i2c-trans-dim value !\n");
	} else {
		drvdata->i2c_trans_dim = temp;
		pr_info("%s i2c transition dimming --<%d>\n",
			__func__, drvdata->i2c_trans_dim);
	}

	rc = of_property_read_u32(np, "aw99703,flash-flto-dim", &drvdata->flto_tim);
	if (rc) {
		drvdata->flto_tim = 5;
		pr_err("%s flash-flto-dim not found\n", __func__);
	} else {
		pr_info("%s flash-flto-dim = %d\n", __func__, drvdata->flto_tim);
	}

	rc = of_property_read_u32(np, "aw99703,flash-fl-s", &drvdata->fl_s);
	if (rc) {
		drvdata->fl_s = 8;
		pr_err("%s flash-fl-s not found\n", __func__);
	} else {
		pr_info("%s flash-fl-s = %d\n", __func__, drvdata->fl_s);
	}

	rc = of_property_read_u32(np, "aw99703,bl-map", &drvdata->bl_map);
	if (rc) {
		drvdata->bl_map = 1;
		pr_err("%s bl_map not found\n", __func__);
	} else {
		pr_info("%s bl_map=%d\n", __func__, drvdata->bl_map);
	}

	rc = of_property_read_u32(np, "aw99703,wprt1", &drvdata->wprt1);
	if (rc) {
		drvdata->wprt1 = 0 ;
		pr_err("%s wprt1 not found\n", __func__);
	} else {
		pr_info("%s wprt1=0x%x\n", __func__, drvdata->wprt1);
	}

	rc = of_property_read_u32(np, "aw99703,bstcrt3", &drvdata->bstcrt3);
	if (rc) {
		drvdata->bstcrt3 = 0x81;
		pr_err("%s bstcrt3 not found\n", __func__);
	} else {
		pr_info("%s bstcrt3=0x%x\n", __func__, drvdata->bstcrt3);
	}

	rc = of_property_read_u32(np, "aw99703,bstcrt4", &drvdata->bstcrt4);
	if (rc) {
		drvdata->bstcrt4 = 0x48;
		pr_err("%s bstcrt4 not found\n", __func__);
	} else {
		pr_info("%s bstcrt4=0x%x\n", __func__, drvdata->bstcrt4);
	}

	rc = of_property_read_u32(np, "aw99703,bstcrt5", &drvdata->bstcrt5);
	if (rc) {
		drvdata->bstcrt5 = 0x69;
		pr_err("%s bstcrt5 not found\n", __func__);
	} else {
		pr_info("%s bstcrt5=0x%x\n", __func__, drvdata->bstcrt5);
	}
}

/******************************************************
 *
 * sys group attribute: reg
 *
 ******************************************************/
static ssize_t aw99703_i2c_reg_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	struct aw99703_data *aw99703 = dev_get_drvdata(dev);

	unsigned int databuf[2] = {0, 0};

	if (sscanf(buf, "%x %x", &databuf[0], &databuf[1]) == 2) {
		aw99703_i2c_write(aw99703->client,
				(unsigned char)databuf[0],
				(unsigned char)databuf[1]);
	}

	return count;
}

static ssize_t aw99703_i2c_reg_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	struct aw99703_data *aw99703 = dev_get_drvdata(dev);
	ssize_t len = 0;
	unsigned char i = 0;
	unsigned char reg_val = 0;

	for (i = 0; i < AW99703_REG_MAX; i++) {
		if (!(aw99703_reg_access[i]&REG_RD_ACCESS))
			continue;
		aw99703_i2c_read(aw99703->client, i, &reg_val);
		len += snprintf(buf+len, PAGE_SIZE-len, "reg:0x%02x=0x%02x\n",
				i, reg_val);
	}

	return len;
}

static DEVICE_ATTR(reg, 0664, aw99703_i2c_reg_show, aw99703_i2c_reg_store);
static struct attribute *aw99703_attributes[] = {
	&dev_attr_reg.attr,
	NULL
};

static struct attribute_group aw99703_attribute_group = {
	.attrs = aw99703_attributes
};

static int aw99703_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct aw99703_data *drvdata;
	struct backlight_device *bl_dev;
	struct backlight_properties props;
	int err = 0;

	pr_info("%s enter! driver version %s\n", __func__, AW99703_VERSION);

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

	drvdata = kzalloc(sizeof(struct aw99703_data), GFP_KERNEL);
	if (drvdata == NULL) {
		pr_err("%s : kzalloc failed\n", __func__);
		err = -ENOMEM;
		goto err_out;
	}

	drvdata->client = client;
	drvdata->adapter = client->adapter;
	drvdata->addr = client->addr;
	drvdata->brightness = MAX_BRIGHTNESS;
	drvdata->enable = true;

	aw99703_get_dt_data(&client->dev, drvdata);
	i2c_set_clientdata(client, drvdata);
	aw99703_gpio_init(drvdata);

	err = aw99703_read_chipid(drvdata);
	if (err < 0) {
		pr_err("%s : ID idenfy failed\n", __func__);
		goto err_init;
	}

	memset(&props, 0, sizeof(struct backlight_properties));
	props.type = BACKLIGHT_RAW;
	props.brightness = MAX_BRIGHTNESS;
	props.max_brightness = MAX_BRIGHTNESS;
	bl_dev = backlight_device_register(AW99703_NAME, &client->dev,
					drvdata, &aw99703_bl_ops, &props);
	if (err) {
		pr_err("%s : register backlight device failed\n", __func__);
		goto err_dev;
	}

	g_aw99703_data = drvdata;
	aw99703_backlight_init(drvdata);
	aw99703_backlight_enable(drvdata);

	err = sysfs_create_group(&client->dev.kobj, &aw99703_attribute_group);
	if (err < 0) {
		dev_info(&client->dev, "%s error creating sysfs attr files\n",
			__func__);
		goto err_sysfs;
	}
	pr_info("%s exit\n", __func__);
	return 0;

err_sysfs:
err_dev:
	backlight_device_unregister(bl_dev);
err_init:
	kfree(drvdata);
err_out:
	return err;
}

static int aw99703_remove(struct i2c_client *client)
{
	struct aw99703_data *drvdata = i2c_get_clientdata(client);

	backlight_device_unregister(drvdata->bl_dev);

	kfree(drvdata);
	return 0;
}

static const struct i2c_device_id aw99703_id[] = {
	{AW99703_NAME, 0},
	{}
};
static struct of_device_id match_table[] = {
		{.compatible = "awinic,aw99703-bl",}
};

MODULE_DEVICE_TABLE(i2c, aw99703_id);

static struct i2c_driver aw99703_i2c_driver = {
	.probe = aw99703_probe,
	.remove = aw99703_remove,
	.id_table = aw99703_id,
	.driver = {
		.name = AW99703_NAME,
		.owner = THIS_MODULE,
		.of_match_table = match_table,
	},
};

module_i2c_driver(aw99703_i2c_driver);
MODULE_DESCRIPTION("Back Light driver for aw99703");
MODULE_LICENSE("GPL v2");
