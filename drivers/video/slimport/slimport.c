/*
 * Copyright(c) 2012-2013, LGE Inc. All rights reserved.
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

#define pr_fmt(fmt)	"%s %s: " fmt, "anx7808", __func__

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
#include <linux/of_gpio.h>
#include <linux/slimport.h>
#include <linux/async.h>
#include <linux/of_platform.h>
#include <linux/clk.h>
#include <linux/clkdev.h>

#include "../msm/mdss/mdss_hdmi_util.h"

#include "slimport_private.h"
#include "slimport_tx_drv.h"

#define ANX7808_MAX_ACTIVE_H	1920

struct anx7808_data {
	struct i2c_client *client;
	struct anx7808_platform_data *pdata;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct mutex lock;
	struct wake_lock slimport_lock;
	int gpio_p_dwn;
	int gpio_reset;
	int gpio_int;
	int gpio_cbl_det;
	const char *vdd10_name;
	const char *avdd33_name;
	struct regulator *avdd_reg;
	struct regulator *vdd_reg;
	struct platform_device *hdmi_pdev;
	struct msm_hdmi_sp_ops *hdmi_sp_ops;
	bool update_chg_type;
	struct clk *usb_clk;
	atomic_t usb_clk_count;
};

struct anx7808_data *the_chip;

#ifdef HDCP_EN
static bool hdcp_enable = 1;
#else
static bool hdcp_enable;
#endif

struct completion init_aux_ch_completion;
static uint32_t sp_tx_chg_current_ma = NORMAL_CHG_I_MA;
static bool irq_enabled = 0;

/* Link integrity check is sometimes failed without external the power.
 * This is a workaround for this failure. Enabling the USB clk help
 * this issue.
 */
static void anx7808_get_usb_clk(struct i2c_client *client)
{
	struct anx7808_data *anx7808 = i2c_get_clientdata(client);
	struct device_node *dev_node = client->dev.of_node;
	struct device_node *node;
	struct platform_device *pdev;

	node = of_parse_phandle(dev_node, "analogix,usb", 0);
	if (!node) {
		pr_warn("can't find dwc-usb3-msm phandle\n");
		return;
	}

	pdev = of_find_device_by_node(node);
	if (!pdev) {
		pr_warn("can't find the device by node\n");
		return;
	}

	anx7808->usb_clk = clk_get(&pdev->dev, "ref_clk");
	if (IS_ERR(anx7808->usb_clk)) {
		pr_warn("can't get usb3 ref_clk\n");
		return;
	}

	atomic_set(&anx7808->usb_clk_count, 0);
	pr_info("clk: %d\n", (int)clk_get_rate(anx7808->usb_clk));
}

static void anx7808_put_usb_clk(struct i2c_client *client)
{
	struct anx7808_data *anx7808 = i2c_get_clientdata(client);

	if (anx7808->usb_clk) {
		clk_put(anx7808->usb_clk);
		anx7808->usb_clk = NULL;
	}
}

static void anx7808_vote_usb_clk(void *data)
{
	struct anx7808_data *anx7808 = data;

	if (!anx7808->usb_clk)
		return;

	if (1 == atomic_inc_return(&anx7808->usb_clk_count)) {
		pr_info("usb clk is voted\n");
		clk_prepare_enable(anx7808->usb_clk);
	}
}

static void anx7808_unvote_usb_clk(void *data)
{
	struct anx7808_data *anx7808 = data;

	if (!anx7808->usb_clk)
		return;

	if (1 == __atomic_add_unless(&anx7808->usb_clk_count, -1, 0)) {
		pr_info("usb clk is unvoted\n");
		clk_disable_unprepare(anx7808->usb_clk);
	}
}

static int anx7808_avdd_3p3_power(struct anx7808_data *chip, int on)
{
	static int on_state;
	int ret = 0;

	if (on_state == on) {
		pr_info("avdd 3.3V is already %s\n", on_state ? "on" : "off");
		goto out;
	}

	if (!chip->avdd_reg) {
		chip->avdd_reg = regulator_get(NULL, chip->avdd33_name);
		if (IS_ERR(chip->avdd_reg)) {
			ret = PTR_ERR(chip->avdd_reg);
			pr_err("regulator_get %s failed. rc = %d\n",
					chip->avdd33_name, ret);
			chip->avdd_reg = NULL;
			goto out;
		}
	}

	if (on) {
		ret = regulator_enable(chip->avdd_reg);
		if (ret) {
			pr_err("avdd_reg enable failed (%d)\n", ret);
			goto err_reg;
		}
	} else {
		ret = regulator_disable(chip->avdd_reg);
		if (ret) {
			pr_err("avdd_reg disable failed (%d)\n", ret);
			goto err_reg;
		}
	}

	on_state = on;
	return 0;

err_reg:
	regulator_put(chip->avdd_reg);
	chip->avdd_reg = NULL;
out:
	return ret;
}

static int anx7808_vdd_1p0_power(struct anx7808_data *chip, int on)
{
	static int on_state;
	int ret = 0;

	if (on_state == on) {
		pr_info("vdd 1.0V is already %s\n", on_state ? "on" : "off");
		goto out;
	}

	if (!chip->vdd_reg) {
		chip->vdd_reg = regulator_get(NULL, chip->vdd10_name);
		if (IS_ERR(chip->vdd_reg)) {
			ret = PTR_ERR(chip->vdd_reg);
			pr_err("regulator_get %s failed. ret = %d\n",
					chip->vdd10_name, ret);
			chip->vdd_reg = NULL;
			goto out;
		}
	}

	if (on) {
		ret = regulator_enable(chip->vdd_reg);
		if (ret) {
			pr_err("vdd_reg enable failed (%d)\n", ret);
			goto err_reg;
		}
	} else {
		ret = regulator_disable(chip->vdd_reg);
		if (ret) {
			pr_err("vdd_reg disable failed (%d)\n", ret);
			goto err_reg;
		}
	}

	on_state = on;
	return 0;

err_reg:
	regulator_put(chip->vdd_reg);
	chip->vdd_reg = NULL;
out:
	return ret;
}

int sp_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;

	if (!the_chip)
		return -EINVAL;

	if (sp_tx_pd_mode) {
		pr_debug("tried to access the sp reg after power down\n");
		return -EIO;
	}

	the_chip->client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(the_chip->client, offset);
	if (ret < 0) {
		pr_err("failed to read i2c addr=%x\n", slave_addr);
		return ret;
	}
	*buf = (uint8_t) ret;

	return 0;
}

int sp_write_reg(uint8_t slave_addr, uint8_t offset, uint8_t value)
{
	int ret = 0;

	if (!the_chip)
		return -EINVAL;

	if (sp_tx_pd_mode) {
		pr_debug("tried to access the sp reg after power down\n");
		return -EIO;
	}

	the_chip->client->addr = (slave_addr >> 1);
	ret = i2c_smbus_write_byte_data(the_chip->client, offset, value);
	if (ret < 0) {
		pr_err("failed to write i2c addr=%x\n", slave_addr);
	}
	return ret;
}

void sp_tx_hardware_poweron(void)
{
	if (!the_chip)
		return;

	gpio_set_value(the_chip->gpio_reset, 0);
	msleep(1);
	gpio_set_value(the_chip->gpio_p_dwn, 0);
	msleep(2);
	anx7808_vdd_1p0_power(the_chip, 1);
	msleep(5);
	gpio_set_value(the_chip->gpio_reset, 1);

	sp_tx_pd_mode = 0;

	pr_info("anx7808 power on\n");
}

void sp_tx_hardware_powerdown(void)
{
	int status = 0;

	if (!the_chip)
		return;

	sp_tx_pd_mode = 1;

	gpio_set_value(the_chip->gpio_reset, 0);
	msleep(1);
	anx7808_vdd_1p0_power(the_chip, 0);
	msleep(2);
	gpio_set_value(the_chip->gpio_p_dwn, 1);
	msleep(1);

	/* turn off hpd */
	if (the_chip->hdmi_sp_ops->set_upstream_hpd) {
		status = the_chip->hdmi_sp_ops->set_upstream_hpd(
				the_chip->hdmi_pdev, 0);
		if (status)
			pr_err("failed to turn off hpd\n");
	}
	pr_info("anx7808 power down\n");
}

static void slimport_cable_plug_proc(struct anx7808_data *anx7808)
{
	int status = 0;

	if (gpio_get_value_cansleep(anx7808->gpio_cbl_det)) {
		/* debounce time for avoiding glitch */
		msleep(50);
		if (gpio_get_value_cansleep(anx7808->gpio_cbl_det)) {
			if (sp_tx_pd_mode) {
				/* first turn on hpd */
				if (anx7808->hdmi_sp_ops->set_upstream_hpd) {
					status = anx7808->hdmi_sp_ops->set_upstream_hpd(
							anx7808->hdmi_pdev, 1);
					if (status)
						pr_err("failed to turn on hpd\n");
				}
				sp_tx_hardware_poweron();
				sp_tx_power_on(SP_TX_PWR_REG);
				sp_tx_power_on(SP_TX_PWR_TOTAL);
				hdmi_rx_initialization();
				sp_tx_initialization();
				sp_tx_vbus_poweron();
				if (!sp_tx_get_cable_type(1)) {
					pr_err("AUX ERR\n");
					sp_tx_power_down_and_init();
					return;
				}
				sp_tx_rx_type_backup = sp_tx_rx_type;
			}

			switch (sp_tx_rx_type) {
			case RX_HDMI:
				if (sp_tx_get_hdmi_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_DP:
				if (sp_tx_get_dp_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_VGA_GEN:
				if (sp_tx_get_vga_connection())
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				break;
			case RX_VGA_9832:
				if (sp_tx_get_vga_connection()) {
					sp_tx_send_message(MSG_CLEAR_IRQ);
					sp_tx_set_sys_state(STATE_PARSE_EDID);
				}
				break;
			case RX_NULL:
			default:
				break;
			}
		}
	} else if (sp_tx_pd_mode == 0) {
			sp_tx_power_down_and_init();
	}
}

static void slimport_edid_proc(void)
{
	sp_tx_aux_polling_enable(0);
	sp_tx_edid_read();

	if (bedid_break)
		pr_err("EDID corruption!\n");
	sp_tx_aux_polling_enable(1);
	hdmi_rx_set_hpd(1);
	hdmi_rx_set_termination(1);
	sp_tx_set_sys_state(STATE_LINK_TRAINING);
}

int slimport_read_edid_block(int block, uint8_t *edid_buf)
{
	if (block == 0) {
		memcpy(edid_buf, bedid_firstblock, sizeof(bedid_firstblock));
	} else if (block == 1) {
		memcpy(edid_buf, bedid_extblock, sizeof(bedid_extblock));
	} else {
		pr_err("%s: block number %d is invalid\n", __func__, block);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(slimport_read_edid_block);

unchar sp_get_link_bw(void)
{
	return slimport_link_bw;
}
EXPORT_SYMBOL(sp_get_link_bw);

void sp_set_link_bw(unchar link_bw)
{
	slimport_link_bw = link_bw;
}
EXPORT_SYMBOL(sp_set_link_bw);

enum RX_CBL_TYPE sp_get_ds_cable_type(void)
{
	return sp_tx_rx_type;
}
EXPORT_SYMBOL(sp_get_ds_cable_type);

bool slimport_is_connected(void)
{
	bool result = false;

	if (!the_chip)
		return false;

	if (gpio_get_value_cansleep(the_chip->gpio_cbl_det)) {
		mdelay(10);
		if (gpio_get_value_cansleep(the_chip->gpio_cbl_det)) {
			pr_info("slimport cable is detected\n");
			result = true;
		}
	}

	return result;
}
EXPORT_SYMBOL(slimport_is_connected);

bool is_slimport_dp(void)
{
	return (sp_tx_rx_type == RX_DP);
}
EXPORT_SYMBOL(is_slimport_dp);

uint32_t slimport_get_chg_current(void)
{
	int ret;

	INIT_COMPLETION(init_aux_ch_completion);
	the_chip->update_chg_type = true;

	ret = wait_for_completion_timeout(&init_aux_ch_completion,
					msecs_to_jiffies(2000));
	if (!ret) {
		pr_err("failed to access charger type\n");
		return NORMAL_CHG_I_MA;
	}

	return sp_tx_chg_current_ma;
}
EXPORT_SYMBOL(slimport_get_chg_current);

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
	return;
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

	if (anx7808->update_chg_type && !sp_tx_pd_mode) {
		sp_tx_chg_current_ma = sp_tx_get_chg_current();
		anx7808->update_chg_type = false;
		complete_all(&init_aux_ch_completion);
	}

	if (sp_tx_system_state == STATE_CONFIG_HDMI)
		sp_tx_config_hdmi_input();

	if (sp_tx_system_state == STATE_LINK_TRAINING) {
		if (!sp_tx_lt_pre_config())
			sp_tx_hw_link_training();
	}

	if (sp_tx_system_state == STATE_CONFIG_OUTPUT)
		slimport_config_output();

	if (sp_tx_system_state == STATE_HDCP_AUTH) {
		if (hdcp_enable) {
			sp_tx_hdcp_process();
		} else {
			sp_tx_power_down(SP_TX_PWR_HDCP);
			sp_tx_video_mute(0);
			hdmi_rx_show_video_info();
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
	gpio_free(anx7808->gpio_cbl_det);
	gpio_free(anx7808->gpio_int);
	gpio_free(anx7808->gpio_reset);
	gpio_free(anx7808->gpio_p_dwn);
}

static int anx7808_init_gpio(struct anx7808_data *anx7808)
{
	int ret = 0;

	ret = gpio_request_one(anx7808->gpio_p_dwn,
				GPIOF_OUT_INIT_HIGH, "anx_p_dwn_ctl");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7808->gpio_p_dwn);
		goto out;
	}

	ret = gpio_request_one(anx7808->gpio_reset,
				GPIOF_OUT_INIT_LOW, "anx7808_reset_n");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7808->gpio_reset);
		goto err0;
	}

	ret = gpio_request_one(anx7808->gpio_int,
				GPIOF_IN, "anx7808_int_n");

	if (ret) {
		pr_err("failed to request gpio %d\n", anx7808->gpio_int);
		goto err1;
	}

	ret = gpio_request_one(anx7808->gpio_cbl_det,
				GPIOF_IN, "anx7808_cbl_det");
	if (ret) {
		pr_err("failed to request gpio %d\n", anx7808->gpio_cbl_det);
		goto err2;
	}

	gpio_set_value(anx7808->gpio_reset, 0);
	gpio_set_value(anx7808->gpio_p_dwn, 1);

	goto out;

err2:
	gpio_free(anx7808->gpio_int);
err1:
	gpio_free(anx7808->gpio_reset);
err0:
	gpio_free(anx7808->gpio_p_dwn);
out:
	return ret;
}

static int anx7808_system_init(void)
{
	int ret = 0;

	ret = anx7808_chip_detect();
	if (ret == 0) {
		pr_err("failed to detect anx7808\n");
		return -ENODEV;
	}

	anx7808_chip_initial();
	return 0;
}

static irqreturn_t anx7808_cbl_det_isr(int irq, void *data)
{
	struct anx7808_data *anx7808 = data;
	int status;

	if (gpio_get_value(anx7808->gpio_cbl_det)) {
		wake_lock(&anx7808->slimport_lock);
		pr_info("detect cable insertion\n");
		anx7808_vote_usb_clk(anx7808);
		queue_delayed_work(anx7808->workqueue, &anx7808->work, 0);
	} else {
		pr_info("detect cable removal\n");
		status = cancel_delayed_work_sync(&anx7808->work);
		if (status == 0)
			flush_workqueue(anx7808->workqueue);
		wake_unlock(&anx7808->slimport_lock);
		wake_lock_timeout(&anx7808->slimport_lock, 2*HZ);
		anx7808_unvote_usb_clk(anx7808);
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

static int anx7808_enable_irq(const char *val, const struct kernel_param *kp)
{
	int ret;
	int old_val = irq_enabled;

	if (!the_chip)
		return -ENODEV;

	ret = param_set_bool(val, kp);
	if (ret) {
		pr_err("failed to enable slimpot\n");
		return ret;
	}

	if (irq_enabled) {
		if (old_val) {
			disable_irq(the_chip->client->irq);
			sp_tx_power_down_and_init();
			anx7808_unvote_usb_clk(the_chip);
		}
		anx7808_cbl_det_isr(the_chip->client->irq, the_chip);
		enable_irq(the_chip->client->irq);
	}

	return 0;
}

static struct kernel_param_ops enable_irq_ops = {
	.set = anx7808_enable_irq,
	.get = param_get_bool,
};
module_param_cb(enable_irq, &enable_irq_ops, &irq_enabled, 0644);

static int anx7808_parse_dt(struct device_node *node,
			   struct anx7808_data *anx7808)
{
	int ret = 0;
	struct platform_device *hdmi_pdev = NULL;
	struct device_node *hdmi_tx_node = NULL;

	anx7808->gpio_p_dwn =
		of_get_named_gpio(node, "analogix,p-dwn-gpio", 0);
	if (anx7808->gpio_p_dwn < 0) {
		pr_err("failed to get analogix,p-dwn-gpio.\n");
		ret = anx7808->gpio_p_dwn;
		goto out;
	}

	anx7808->gpio_reset =
		of_get_named_gpio(node, "analogix,reset-gpio", 0);
	if (anx7808->gpio_reset < 0) {
		pr_err("failed to get analogix,reset-gpio.\n");
		ret = anx7808->gpio_reset;
		goto out;
	}

	anx7808->gpio_int =
		of_get_named_gpio(node, "analogix,irq-gpio", 0);
	if (anx7808->gpio_int < 0) {
		pr_err("failed to get analogix,irq-gpio.\n");
		ret = anx7808->gpio_int;
		goto out;
	}

	anx7808->gpio_cbl_det =
		of_get_named_gpio(node, "analogix,cbl-det-gpio", 0);
	if (anx7808->gpio_cbl_det < 0) {
		pr_err("failed to get analogix,cbl-det-gpio.\n");
		ret = anx7808->gpio_cbl_det;
		goto out;
	}

	ret = of_property_read_string(node, "analogix,vdd10-name",
			&anx7808->vdd10_name);
	if (ret) {
		pr_err("failed to get vdd10-name.\n");
		goto out;
	}

	ret = of_property_read_string(node, "analogix,avdd33-name",
			&anx7808->avdd33_name);
	if (ret) {
		pr_err("failed to get avdd33-name.\n");
		goto out;
	}

	/* parse phandle for hdmi tx handle */
	hdmi_tx_node = of_parse_phandle(node, "analogix,hdmi-tx-map", 0);
	if (!hdmi_tx_node) {
		pr_err("can't find hdmi phandle\n");
		ret = -EINVAL;
		goto out;
	}

	hdmi_pdev = of_find_device_by_node(hdmi_tx_node);
	if (!hdmi_pdev) {
		pr_err("can't find the deivce by node\n");
		ret = -EINVAL;
		goto out;
	}
	anx7808->hdmi_pdev = hdmi_pdev;

out:
	return ret;
}

static void anx7808_setup_video_mode_lut(void)
{
	int i;
	struct msm_hdmi_mode_timing_info *temp_timing;

	for (i = 0; i < HDMI_VFRMT_MAX; i++) {
		temp_timing = (struct msm_hdmi_mode_timing_info *)
				hdmi_get_supported_mode(i);
		if (!temp_timing)
			continue;

		/* formats that exceed max active_h */
		if (temp_timing->active_h > ANX7808_MAX_ACTIVE_H) {
			pr_debug("disabled mode (%d)\n", temp_timing->active_h);
			hdmi_del_supported_mode(i);
		}
	}
}

static int anx7808_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{
	struct anx7808_data *anx7808;
	struct anx7808_platform_data *pdata;
	struct device_node *dev_node = client->dev.of_node;
	struct msm_hdmi_sp_ops *hdmi_sp_ops = NULL;
	int ret = 0;

	if (!i2c_check_functionality(client->adapter,
			I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("i2c bus does not support anx7808\n");
		ret = -ENODEV;
		goto exit;
	}

	anx7808 = kzalloc(sizeof(struct anx7808_data), GFP_KERNEL);
	if (!anx7808) {
		pr_err("failed to allocate driver data\n");
		ret = -ENOMEM;
		goto exit;
	}

	anx7808->client = client;
	i2c_set_clientdata(client, anx7808);

	if (dev_node) {
		ret = anx7808_parse_dt(dev_node, anx7808);
		if (ret) {
			pr_err("failed to parse dt\n");
			goto err0;
		}
	} else {
		pdata = client->dev.platform_data;
		if (pdata == NULL) {
			pr_err("no platform data.\n");
			goto err0;
		}

		anx7808->gpio_p_dwn = pdata->gpio_p_dwn;
		anx7808->gpio_reset = pdata->gpio_reset;
		anx7808->gpio_int = pdata->gpio_int;
		anx7808->gpio_cbl_det = pdata->gpio_cbl_det;
		anx7808->vdd10_name = pdata->vdd10_name;
		anx7808->vdd10_name = pdata->avdd33_name;
	}

	/* initialize hdmi_sp_ops */
	hdmi_sp_ops = devm_kzalloc(&client->dev,
				   sizeof(struct msm_hdmi_sp_ops),
				   GFP_KERNEL);
	if (!hdmi_sp_ops) {
		pr_err("alloc hdmi sp ops failed\n");
		goto err0;
	}

	if (anx7808->hdmi_pdev) {
		ret = msm_hdmi_register_sp(anx7808->hdmi_pdev,
					  hdmi_sp_ops);
		if (ret) {
			pr_err("register with hdmi_failed\n");
			goto err0;
		}
	}

	anx7808->hdmi_sp_ops = hdmi_sp_ops;

	the_chip = anx7808;

	mutex_init(&anx7808->lock);
	init_completion(&init_aux_ch_completion);
	ret = anx7808_init_gpio(anx7808);
	if (ret) {
		pr_err("failed to initialize gpio\n");
		goto err0;
	}

	INIT_DELAYED_WORK(&anx7808->work, anx7808_work_func);

	anx7808->workqueue = create_singlethread_workqueue("anx7808_work");
	if (!anx7808->workqueue) {
		pr_err("failed to create work queue\n");
		ret = -ENOMEM;
		goto err1;
	}

	ret = anx7808_avdd_3p3_power(anx7808, true);
	if (ret)
		goto err2;

	ret = anx7808_vdd_1p0_power(anx7808, false);
	if (ret)
		goto err3;

	ret = anx7808_system_init();
	if (ret) {
		pr_err("failed to initialize anx7808\n");
		goto err4;
	}

	client->irq = gpio_to_irq(anx7808->gpio_cbl_det);
	if (client->irq < 0) {
		pr_err("failed to get gpio irq\n");
		goto err4;
	}

	wake_lock_init(&anx7808->slimport_lock, WAKE_LOCK_SUSPEND,
				"slimport_wake_lock");

	ret = request_threaded_irq(client->irq, NULL, anx7808_cbl_det_isr,
					IRQF_TRIGGER_RISING
					| IRQF_TRIGGER_FALLING
					| IRQF_ONESHOT,
					"anx7808", anx7808);
	if (ret  < 0) {
		pr_err("failed to request irq\n");
		goto err5;
	}

	disable_irq(client->irq);
	ret = enable_irq_wake(client->irq);
	if (ret  < 0) {
		pr_err("interrupt wake enable fail\n");
		goto err6;
	}

	anx7808_setup_video_mode_lut();
	anx7808_get_usb_clk(client);

	return 0;

err6:
	free_irq(client->irq, anx7808);
err5:
	wake_lock_destroy(&anx7808->slimport_lock);
err4:
	if (!anx7808->vdd_reg)
		regulator_put(anx7808->vdd_reg);
err3:
	if (!anx7808->avdd_reg)
		regulator_put(anx7808->avdd_reg);
err2:
	destroy_workqueue(anx7808->workqueue);
err1:
	anx7808_free_gpio(anx7808);
err0:
	the_chip = NULL;
	kfree(anx7808);
exit:
	return ret;
}

static int anx7808_i2c_remove(struct i2c_client *client)
{
	struct anx7808_data *anx7808 = i2c_get_clientdata(client);

	anx7808_put_usb_clk(client);
	free_irq(client->irq, anx7808);
	wake_lock_destroy(&anx7808->slimport_lock);
	if (!anx7808->vdd_reg)
		regulator_put(anx7808->vdd_reg);
	if (!anx7808->avdd_reg)
		regulator_put(anx7808->avdd_reg);
	destroy_workqueue(anx7808->workqueue);
	anx7808_free_gpio(anx7808);
	the_chip = NULL;
	kfree(anx7808);
	return 0;
}

static const struct i2c_device_id anx7808_id[] = {
	{ "anx7808", 0 },
	{ }
};
MODULE_DEVICE_TABLE(i2c, anx7808_id);

static struct of_device_id anx_match_table[] = {
	{ .compatible = "analogix,anx7808",},
	{ },
};

static struct i2c_driver anx7808_driver = {
	.driver  = {
		.name  = "anx7808",
		.owner  = THIS_MODULE,
		.of_match_table = anx_match_table,
	},
	.probe  = anx7808_i2c_probe,
	.remove  = anx7808_i2c_remove,
	.id_table  = anx7808_id,
};

static void __init anx7808_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

	ret = i2c_add_driver(&anx7808_driver);
	if (ret)
		pr_err("%s: failed to register anx7808 driver\n", __func__);
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
MODULE_VERSION("0.4");
