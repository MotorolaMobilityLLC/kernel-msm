/*
 * Copyright(c) 2012, LG Electronics Inc. All rights reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#include <linux/delay.h>
#include <linux/gpio.h>
#include <linux/i2c.h>
#include <linux/interrupt.h>
#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/mutex.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/async.h>
#include <linux/of_gpio.h>
#include <linux/slimport.h>

#include "slimport_private.h"
#include "slimport_tx_drv.h"

/* to access global platform data */
static struct anx7808_platform_data *g_pdata;

/*
 * to apply High voltage to HDMI_SWITCH_EN
 * which can select MHL or SlimPort on LGPS11
 * this feature should be enable only when board has hdmi switch chip.
 */

/* #define USE_HDMI_SWITCH */

#ifdef USE_HDMI_SWITCH
static int hdmi_switch_gpio = 64;
#endif

static int slimport_avdd_power(unsigned int onoff);
static int slimport_dvdd_power(unsigned int onoff);

struct i2c_client *anx7808_client;

struct anx7808_data {
	struct anx7808_platform_data *pdata;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct mutex lock;
	struct wake_lock slimport_lock;
};

static bool hdcp_enable = 1;

bool slimport_is_connected(void)
{
#ifdef CONFIG_OF
	struct anx7808_platform_data *pdata = g_pdata;
#else
	struct anx7808_platform_data *pdata = anx7808_client->dev.platform_data;
#endif
	bool result = false;

	if (!pdata)
		return false;

	spin_lock(&pdata->lock);

	if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
		mdelay(10);
		if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
			pr_info("%s %s : Slimport Dongle is detected\n",
					LOG_TAG, __func__);
			result = true;
		}
	}
	spin_unlock(&pdata->lock);

	return result;
}
EXPORT_SYMBOL(slimport_is_connected);

/*
 * power control
 */
static int slimport_avdd_power(unsigned int onoff)
{
	/* TODO: will be fixed */
	return 0;
#if 0
#ifdef CONFIG_OF
	struct anx7808_platform_data *pdata = g_pdata;
#else
	struct anx7808_platform_data *pdata = anx7808_client->dev.platform_data;
#endif
	int rc = 0;

	if (onoff) {
		pr_info("%s %s: avdd power on\n", LOG_TAG, __func__);
		rc = regulator_enable(pdata->avdd_10);
		if (rc < 0) {
			pr_err("%s %s: failed to enable avdd regulator rc=%d\n",
					LOG_TAG, __func__, rc);
		}
	} else {
		pr_info("%s %s: avdd power off\n", LOG_TAG, __func__);
		rc = regulator_disable(pdata->avdd_10);
	}

	return rc;
#endif
}

static int slimport_dvdd_power(unsigned int onoff)
{
	/* TODO: will be fixed */
	return 0;
#if 0
#ifdef CONFIG_OF
	struct anx7808_platform_data *pdata = g_pdata;
#else
	struct anx7808_platform_data *pdata = anx7808_client->dev.platform_data;
#endif
	int rc = 0;

	if (onoff) {
		pr_info("%s %s: dvdd power on\n", LOG_TAG, __func__);
		rc = regulator_enable(pdata->dvdd_10);
		if (rc < 0) {
			pr_err("%s %s: failed to enable dvdd regulator rc=%d\n",
					LOG_TAG, __func__, rc);
		}
	} else {
		pr_info("%s %s: dvdd power off\n", LOG_TAG, __func__);
		rc = regulator_disable(pdata->dvdd_10);
	}

	return rc;
#endif
}

static ssize_t
slimport_rev_check_store(
	struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	int cmd;

	sscanf(buf, "%d", &cmd);
	switch (cmd) {
	case 1:
		sp_tx_chip_located();
		break;
	}
	return count;
}

/* for debugging */
static struct device_attribute slimport_device_attrs[] = {
	__ATTR(rev_check, S_IRUGO | S_IWUSR, NULL, slimport_rev_check_store),
};

int sp_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;

	anx7808_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(anx7808_client, offset);
	if (ret < 0) {
		pr_err("%s %s: failed to read i2c addr=%x\n", LOG_TAG,
			__func__, slave_addr);
		return ret;
	}
	*buf = (uint8_t) ret;

	return 0;
}

int sp_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	int ret = 0;

	anx7808_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_write_byte_data(anx7808_client, offset, value);
	if (ret < 0) {
		pr_err("%s %s: failed to write i2c addr=%x\n", LOG_TAG,
			__func__, slave_addr);
	}
	return ret;
}

void sp_tx_hardware_poweron(void)
{
#ifdef CONFIG_OF
	struct anx7808_platform_data *pdata = g_pdata;
#else
	struct anx7808_platform_data *pdata = anx7808_client->dev.platform_data;
#endif

	gpio_set_value(pdata->gpio_reset, 0);
	msleep(20);
	gpio_set_value(pdata->gpio_p_dwn, 0);
	msleep(10);

	if (pdata->external_ldo_control) {
		/* Enable 1.0V LDO */
		gpio_set_value(pdata->gpio_v10_ctrl, 1);
		msleep(20);
		/* Enable 3.3V LDO - need to be check */
		gpio_set_value(pdata->gpio_v33_ctrl, 1);
		msleep(20);
	}

	pdata->dvdd_power(1);
	msleep(100);
	gpio_set_value(pdata->gpio_reset, 1);
	msleep(20);

	pr_info("%s %s: anx7808 power on\n", LOG_TAG, __func__);
}

void sp_tx_hardware_powerdown(void)
{
#ifdef CONFIG_OF
	struct anx7808_platform_data *pdata = g_pdata;
#else
	struct anx7808_platform_data *pdata = anx7808_client->dev.platform_data;
#endif

	gpio_set_value(pdata->gpio_reset, 0);
	msleep(10);
	pdata->dvdd_power(0);
	msleep(10);

	if (pdata->external_ldo_control) {
		gpio_set_value(pdata->gpio_v10_ctrl, 0);
		msleep(20);
	}

	gpio_set_value(pdata->gpio_p_dwn, 1);
	msleep(20);

	pr_info("%s %s: anx7808 power down\n", LOG_TAG, __func__);
}

int slimport_read_edid_block(int block, uint8_t *edid_buf)
{
	if (block == 0) {
		memcpy(edid_buf, bedid_firstblock, sizeof(bedid_firstblock));
	} else if (block == 1) {
		memcpy(edid_buf, bedid_extblock, sizeof(bedid_extblock));
	} else {
		pr_err("%s %s: block number %d is invalid\n",
			   LOG_TAG, __func__, block);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(slimport_read_edid_block);

static void slimport_cable_plug_proc(struct anx7808_data *anx7808)
{
	struct anx7808_platform_data *pdata = anx7808->pdata;

	if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {

		mdelay(50);

		if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
			if (sp_tx_pd_mode) {
				sp_tx_pd_mode = 0;
				sp_tx_hardware_poweron();
				sp_tx_power_on(SP_TX_PWR_REG);
				sp_tx_power_on(SP_TX_PWR_TOTAL);
				hdmi_rx_initialization();
				sp_tx_initialization();
				sp_tx_vbus_poweron();

				msleep(200);

				if (!sp_tx_get_cable_type()) {
					pr_err("%s %s:AUX ERR\n",
						   LOG_TAG, __func__);
					sp_tx_vbus_powerdown();
					sp_tx_power_down(SP_TX_PWR_REG);
					sp_tx_power_down(SP_TX_PWR_TOTAL);
					sp_tx_hardware_powerdown();
					sp_tx_pd_mode = 1;
					sp_tx_link_config_done = 0;
					sp_tx_hw_lt_enable = 0;
					sp_tx_hw_lt_done = 0;
					sp_tx_rx_anx7730 = 0;
					sp_tx_rx_mydp = 0;
					sp_tx_set_sys_state(STATE_CABLE_PLUG);
					return;
				}
			}

			if (sp_tx_rx_anx7730) {
				if (sp_tx_get_hdmi_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
			} else if (sp_tx_rx_mydp) {
				if (sp_tx_get_dp_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
			} else {
				if (sp_tx_get_vga_connection()) {
					sp_tx_send_message(MSG_CLEAR_IRQ);
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				}
			}
		}

	} else if (sp_tx_pd_mode == 0) {
		sp_tx_vbus_powerdown();
		sp_tx_power_down(SP_TX_PWR_REG);
		sp_tx_power_down(SP_TX_PWR_TOTAL);
		sp_tx_hardware_powerdown();
		sp_tx_pd_mode = 1;
		sp_tx_link_config_done = 0;
		sp_tx_hw_lt_enable = 0;
		sp_tx_hw_lt_done = 0;
		sp_tx_rx_anx7730 = 0;
		sp_tx_rx_mydp = 0;
		sp_tx_set_sys_state(STATE_CABLE_PLUG);
	}
}

static void slimport_edid_proc(void)
{
	sp_tx_edid_read();

	if (bedid_break)
		pr_err("%s %s: EDID corruption!\n", LOG_TAG, __func__);
	hdmi_rx_set_hpd(1);
	hdmi_rx_set_termination(1);
	sp_tx_set_sys_state(STATE_CONFIG_HDMI);
}

static void slimport_config_output(void)
{
	sp_tx_clean_hdcp();
	sp_tx_set_colorspace();
	sp_tx_avi_setup();
	sp_tx_config_packets(AVI_PACKETS);
	sp_tx_enable_video_input(1);
	sp_tx_set_sys_state(STATE_HDCP_AUTH);
}

static void slimport_playback_proc(void)
{
	if (!anx7808_ver_ba)
		sp_tx_set_3d_packets();
}

static void slimport_main_proc(struct anx7808_data *anx7808)
{
	mutex_lock(&anx7808->lock);

	if (!sp_tx_pd_mode) {
		sp_tx_int_irq_handler();
		hdmi_rx_int_irq_handler();
	}

	if (sp_tx_system_state == STATE_CABLE_PLUG)
		slimport_cable_plug_proc(anx7808);

	if (sp_tx_system_state == STATE_PARSE_EDID)
		slimport_edid_proc();

	if (sp_tx_system_state == STATE_CONFIG_HDMI)
		sp_tx_config_hdmi_input();

	if (sp_tx_system_state == STATE_LINK_TRAINING) {
		if (!sp_tx_lt_pre_config())
			sp_tx_hw_link_training();
	}

	if (sp_tx_system_state == STATE_CONFIG_OUTPUT)
		slimport_config_output();

	if (sp_tx_system_state == STATE_HDCP_AUTH) {
#ifdef HDCP_EN
		hdcp_enable = 1;
#else
		hdcp_enable = 0;
#endif
		if (hdcp_enable &&
				(sp_tx_rx_anx7730 || sp_tx_rx_mydp)) {
			sp_tx_hdcp_process();
		} else {
			sp_tx_power_down(SP_TX_PWR_HDCP);
			sp_tx_video_mute(0);
			sp_tx_show_infomation();
			sp_tx_set_sys_state(STATE_PLAY_BACK);
		}
	}

	if (sp_tx_system_state == STATE_PLAY_BACK)
		slimport_playback_proc();

	mutex_unlock(&anx7808->lock);
}

static uint8_t anx7808_chip_detect(void)
{
	return sp_tx_chip_located();
}

static void anx7808_chip_initial(void)
{
#ifdef EYE_TEST
	sp_tx_eye_diagram_test();
#else
	sp_tx_variable_init();
	sp_tx_vbus_powerdown();
	sp_tx_hardware_powerdown();
	sp_tx_set_sys_state(STATE_CABLE_PLUG);
#endif
}

static void anx7808_free_gpio(struct anx7808_data *anx7808)
{
	gpio_free(anx7808->pdata->gpio_cbl_det);
	gpio_free(anx7808->pdata->gpio_int);
	gpio_free(anx7808->pdata->gpio_reset);
	gpio_free(anx7808->pdata->gpio_p_dwn);

	if (anx7808->pdata->external_ldo_control) {
		gpio_free(anx7808->pdata->gpio_v10_ctrl);
		gpio_free(anx7808->pdata->gpio_v33_ctrl);
	}
}

static int anx7808_init_gpio(struct anx7808_data *anx7808)
{
	int ret = 0;

	pr_info("anx7808 init gpio\n");

	ret = gpio_request(anx7808->pdata->gpio_p_dwn, "anx_p_dwn_ctl");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
				anx7808->pdata->gpio_p_dwn);
		goto err0;
	}
	gpio_direction_output(anx7808->pdata->gpio_p_dwn, 1);

	ret = gpio_request(anx7808->pdata->gpio_reset, "anx7808_reset_n");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
				anx7808->pdata->gpio_reset);
		goto err1;
	}
	gpio_direction_output(anx7808->pdata->gpio_reset, 0);

	ret = gpio_request(anx7808->pdata->gpio_int, "anx7808_int_n");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
				anx7808->pdata->gpio_int);
		goto err2;
	}
	gpio_direction_input(anx7808->pdata->gpio_int);

	ret = gpio_request(anx7808->pdata->gpio_cbl_det, "anx7808_cbl_det");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
				anx7808->pdata->gpio_cbl_det);
		goto err3;
	}
	gpio_direction_input(anx7808->pdata->gpio_cbl_det);

	if (anx7808->pdata->external_ldo_control) {
		ret = gpio_request(anx7808->pdata->gpio_v10_ctrl,
							"anx7808_v10_ctrl");
		if (ret) {
			pr_err("%s : failed to request gpio %d\n", __func__,
						anx7808->pdata->gpio_v10_ctrl);
			goto err5;
		}
		gpio_direction_output(anx7808->pdata->gpio_v10_ctrl, 0);

		ret = gpio_request(anx7808->pdata->gpio_v33_ctrl,
							"anx7808_v33_ctrl");
		if (ret) {
			pr_err("%s : failed to request gpio %d\n", __func__,
						anx7808->pdata->gpio_v33_ctrl);
			goto err6;
		}
		gpio_direction_output(anx7808->pdata->gpio_v33_ctrl, 0);

		gpio_set_value(anx7808->pdata->gpio_v10_ctrl, 0);
		/* need to be check below */
		gpio_set_value(anx7808->pdata->gpio_v33_ctrl, 1);

	}
	gpio_set_value(anx7808->pdata->gpio_reset, 0);
	gpio_set_value(anx7808->pdata->gpio_p_dwn, 1);

#ifdef USE_HDMI_SWITCH
	ret = gpio_request(hdmi_switch_gpio, "anx7808_hdmi_switch_gpio");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
				hdmi_switch_gpio);
		goto err4;
	}
	gpio_direction_output(hdmi_switch_gpio, 0);
	msleep(1);
	gpio_set_value(hdmi_switch_gpio, 1);
#endif

	goto out;

err6:
	gpio_free(anx7808->pdata->gpio_v33_ctrl);
err5:
	gpio_free(anx7808->pdata->gpio_v10_ctrl);
#ifdef USE_HDMI_SWITCH
err4:
	gpio_free(hdmi_switch_gpio);
#endif
err3:
	gpio_free(anx7808->pdata->gpio_cbl_det);
err2:
	gpio_free(anx7808->pdata->gpio_int);
err1:
	gpio_free(anx7808->pdata->gpio_reset);
err0:
	gpio_free(anx7808->pdata->gpio_p_dwn);
out:
	return ret;
}

static int anx7808_system_init(void)
{
	int ret = 0;

	ret = anx7808_chip_detect();
	if (ret == 0) {
		pr_err("%s : failed to detect anx7808\n", __func__);
		return -ENODEV;
	}

	anx7808_chip_initial();
	return 0;
}

static irqreturn_t anx7808_cbl_det_isr(int irq, void *data)
{
	struct anx7808_data *anx7808 = data;

	if (gpio_get_value(anx7808->pdata->gpio_cbl_det)) {
		wake_lock(&anx7808->slimport_lock);
		pr_info("%s : detect cable insertion\n", __func__);
		queue_delayed_work(anx7808->workqueue, &anx7808->work, 0);
	} else {
		pr_info("%s : detect cable removal\n", __func__);
		cancel_delayed_work_sync(&anx7808->work);
		wake_unlock(&anx7808->slimport_lock);
		wake_lock_timeout(&anx7808->slimport_lock, 2*HZ);
	}
	return IRQ_HANDLED;
}

static void anx7808_work_func(struct work_struct *work)
{
#ifndef EYE_TEST
	struct anx7808_data *td = container_of(work, struct anx7808_data,
								work.work);

	slimport_main_proc(td);
	queue_delayed_work(td->workqueue, &td->work,
			msecs_to_jiffies(300));
#endif
}

/*
 * add device tree parsing functions
 */
#ifdef CONFIG_OF
int anx7808_regulator_configure(
	struct device *dev, struct anx7808_platform_data *pdata)
{
	/* TODO: will be fixed */
	return 0;
#if 0
	int rc = 0;

	pdata->avdd_10 = regulator_get(dev, "analogix,vdd_ana");
	if (IS_ERR(pdata->avdd_10)) {
		rc = PTR_ERR(pdata->avdd_10);
		pr_err("%s : Regulator get failed avdd_10 rc=%d\n",
			   __func__, rc);
		return rc;
	}

	if (regulator_count_voltages(pdata->avdd_10) > 0) {
		rc = regulator_set_voltage(pdata->avdd_10, 1000000,
							1000000);
		if (rc) {
			pr_err("%s : Regulator set_vtg failed rc=%d\n",
				   __func__, rc);
			goto error_set_vtg_avdd_10;
		}
	}

	pdata->dvdd_10 = regulator_get(dev, "analogix,vdd_dig");
	if (IS_ERR(pdata->dvdd_10)) {
		rc = PTR_ERR(pdata->dvdd_10);
		pr_err("%s : Regulator get failed dvdd_10 rc=%d\n",
			   __func__, rc);
		return rc;
	}

	if (regulator_count_voltages(pdata->dvdd_10) > 0) {
		rc = regulator_set_voltage(pdata->dvdd_10, 1000000,
							1000000);
		if (rc) {
			pr_err("%s : Regulator set_vtg failed rc=%d\n",
				   __func__, rc);
			goto error_set_vtg_dvdd_10;
		}
	}

	return 0;

error_set_vtg_dvdd_10:
	regulator_put(pdata->dvdd_10);

error_set_vtg_avdd_10:
	regulator_put(pdata->avdd_10);

	return rc;
#endif
}

static int anx7808_parse_dt(
		struct device *dev, struct anx7808_platform_data *pdata)
{
	int rc = 0;
	struct device_node *np = dev->of_node;

	pdata->gpio_p_dwn = of_get_named_gpio_flags(
		np, "analogix,p-dwn-gpio", 0, NULL);

	pdata->gpio_reset = of_get_named_gpio_flags(
		np, "analogix,reset-gpio", 0, NULL);

	pdata->gpio_int = of_get_named_gpio_flags(
		np, "analogix,irq-gpio", 0, NULL);

	pdata->gpio_cbl_det = of_get_named_gpio_flags(
		np, "analogix,cbl-det-gpio", 0, NULL);

	pr_info(
		"%s gpio p_dwn : %d, reset : %d, irq : %d, gpio_cbl_det %d\n",
		LOG_TAG, pdata->gpio_p_dwn,
		pdata->gpio_reset,
		pdata->gpio_int,
		pdata->gpio_cbl_det);
	/*
	 * if "lge,external-ldo-control" property is not exist, we
	 * assume that it is used in board.
	 * lgps11 don't use external ldo control,
	 * please use "lge,external-ldo-control=<0>" in dtsi
	 */
	rc = of_property_read_u32(np, "lge,external-ldo-control",
		&pdata->external_ldo_control);
	if (rc == -EINVAL)
		pdata->external_ldo_control = 1;

	if (pdata->external_ldo_control) {
		pdata->gpio_v10_ctrl = of_get_named_gpio_flags(
			np, "analogix,v10-ctrl-gpio", 0, NULL);

		pdata->gpio_v33_ctrl = of_get_named_gpio_flags(
			np, "analogix,v33-ctrl-gpio", 0, NULL);

		pr_info("%s gpio_v10_ctrl %d avdd33-en-gpio %d\n",
			LOG_TAG, pdata->gpio_v10_ctrl, pdata->gpio_v33_ctrl);
	}

	if (anx7808_regulator_configure(dev, pdata) < 0) {
		pr_err("%s: parsing dt for anx7808 is failed.\n", __func__);
		return rc;
	}

/* connects function nodes which are not provided with dts */
	pdata->avdd_power = slimport_avdd_power;
	pdata->dvdd_power = slimport_dvdd_power;

#ifdef USE_HDMI_SWITCH
	hdmi_switch_gpio = of_get_named_gpio_flags(
		np, "analogix,hdmi-switch-gpio", 0, NULL);
	pr_info("%s hdmi_switch_gpio : %d\n",
		   LOG_TAG, hdmi_switch_gpio);
#endif
	return 0;
}
#else
static int anx7808_parse_dt(
	struct device *dev, struct anx7808_platform_data *pdata)
{
	return -ENODEV;
}
#endif

static int anx7808_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	struct anx7808_data *anx7808;
	struct anx7808_platform_data *pdata;
	int ret = 0;
	int i = 0;

	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("%s: i2c bus does not support the anx7808\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	anx7808 = kzalloc(sizeof(struct anx7808_data), GFP_KERNEL);
	if (!anx7808) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
					sizeof(struct anx7808_platform_data),
					GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		client->dev.platform_data = pdata;
/* device tree parsing function call */
		ret = anx7808_parse_dt(&client->dev, pdata);
		if (ret != 0) /* if occurs error */
			goto err0;

		anx7808->pdata = pdata;
	} else {
		anx7808->pdata = client->dev.platform_data;
	}

	/* to access global platform data */
	g_pdata = anx7808->pdata;

	memcpy(&anx7808_client, &client, sizeof(client));

	mutex_init(&anx7808->lock);

	if (!anx7808->pdata) {
		ret = -EINVAL;
		goto err0;
	}

	ret = anx7808_init_gpio(anx7808);
	if (ret) {
		pr_err("%s: failed to initialize gpio\n", __func__);
		goto err1;
	}

	INIT_DELAYED_WORK(&anx7808->work, anx7808_work_func);

	anx7808->workqueue = create_singlethread_workqueue("anx7808_work");
	if (anx7808->workqueue == NULL) {
		pr_err("%s: failed to create work queue\n", __func__);
		ret = -ENOMEM;
		goto err2;
	}

	anx7808->pdata->avdd_power(1);
	anx7808->pdata->dvdd_power(1);

	ret = anx7808_system_init();
	if (ret) {
		pr_err("%s: failed to initialize anx7808\n", __func__);
		goto err2;
	}

	client->irq = gpio_to_irq(anx7808->pdata->gpio_cbl_det);
	if (client->irq < 0) {
		pr_err("%s : failed to get gpio irq\n", __func__);
		goto err3;
	}

	wake_lock_init(&anx7808->slimport_lock,
				   WAKE_LOCK_SUSPEND,
				   "slimport_wake_lock");

	ret = request_threaded_irq(client->irq, NULL, anx7808_cbl_det_isr,
					IRQF_TRIGGER_RISING
					| IRQF_TRIGGER_FALLING
					| IRQF_ONESHOT,
					"anx7808", anx7808);
	if (ret  < 0) {
		pr_err("%s : failed to request irq\n", __func__);
		goto err3;
	}

	ret = irq_set_irq_wake(client->irq, 1);
	if (ret  < 0) {
		pr_err("%s : Request irq for cable detect", __func__);
		pr_err("interrupt wake set fail\n");
		goto err3;
	}

	ret = enable_irq_wake(client->irq);
	if (ret  < 0) {
		pr_err("%s : Enable irq for cable detect", __func__);
		pr_err("interrupt wake enable fail\n");
		goto err3;
	}

	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++) {
		ret = device_create_file(
			&client->dev, &slimport_device_attrs[i]);
		if (ret) {
			pr_err("anx7808 sysfs register failed\n");
			goto err3;
		}
	}

	goto exit;

err3:
	free_irq(client->irq, anx7808);
err2:
	destroy_workqueue(anx7808->workqueue);
err1:
	anx7808_free_gpio(anx7808);
err0:
	kfree(anx7808);
exit:
	return ret;
}

static int anx7808_i2c_remove(struct i2c_client *client)
{
	struct anx7808_data *anx7808 = i2c_get_clientdata(client);

	free_irq(client->irq, anx7808);
	anx7808_free_gpio(anx7808);
	destroy_workqueue(anx7808->workqueue);
	wake_lock_destroy(&anx7808->slimport_lock);
	kfree(anx7808);
	return 0;
}

static int anx7808_i2c_suspend(struct i2c_client *client, pm_message_t state)
{
	return 0;
}

static int anx7808_i2c_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id anx7808_id[] = {
	{ "anx7808", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, anx7808_id);

#ifdef CONFIG_OF
static struct of_device_id anx_match_table[] = {
	{ .compatible = "analogix,anx7808",},
	{ },
};
#endif

static struct i2c_driver anx7808_driver = {
	.driver  = {
		.name  = "anx7808",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = anx_match_table,
#endif
	},
	.probe  = anx7808_i2c_probe,
	.remove  = anx7808_i2c_remove,
	.suspend = anx7808_i2c_suspend,
	.resume = anx7808_i2c_resume,
	.id_table  = anx7808_id,
};

static void __init anx7808_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

	ret = i2c_add_driver(&anx7808_driver);
	if (ret < 0)
		pr_err("%s: failed to register anx7808 i2c drivern", __func__);
}

static int __init anx7808_init(void)
{
	async_schedule(anx7808_init_async, NULL);
	return 0;
}

static void __exit anx7808_exit(void)
{
	i2c_del_driver(&anx7808_driver);
}

module_init(anx7808_init);
module_exit(anx7808_exit);

MODULE_DESCRIPTION("Slimport  transmitter ANX7808 driver");
MODULE_AUTHOR("ChoongRyeol Lee <choongryeol.lee@lge.com>");
MODULE_LICENSE("GPL");
