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
#include <linux/platform_data/slimport_device.h>
#include <linux/slab.h>
#include <linux/types.h>
#include <linux/workqueue.h>
#include <linux/wakelock.h>
#include <linux/regulator/consumer.h>
#include <linux/err.h>
#include <linux/async.h>
#include <linux/slimport.h>
#include <linux/ratelimit.h>
#include <linux/clk.h>

#include "slimport7816_tx_reg.h"
#include "slimport7816_tx_drv.h"
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
#include "../../msm/mdss/mdss_hdmi_slimport.h"
#endif

#include <linux/of_gpio.h>
#include <linux/of_platform.h>
#include <linux/pinctrl/consumer.h>

#include <linux/mod_display.h>
#include <linux/mod_display_ops.h>

/* Enable or Disable HDCP by default */
/* hdcp_enable = 1: Enable,  0: Disable */
int hdcp_disable;

/* HDCP switch for external block*/
/* external_block_en = 1: enable, 0: disable*/
int external_block_en = 0;
static bool irq_enable;

/* to access global platform data */
static struct anx7816_platform_data *g_pdata;
static struct anx7816_data *g_data;

/* #define USE_HDMI_SWITCH */
#define TRUE 1
#define FALSE 0
#ifdef USE_HDMI_SWITCH
static int hdmi_switch_gpio = 64;
#endif

static int slimport7816_avdd_power(unsigned int onoff);

struct i2c_client *anx7816_client;

struct anx7816_data {
	struct anx7816_platform_data    *pdata;
	struct delayed_work    work;
	struct workqueue_struct    *workqueue;
	struct mutex    lock;
	struct wake_lock slimport_lock;
/*	struct delayed_work dwc3_ref_clk_work; */
	bool slimport_connected;
};

static int anx7816_pinctrl_rst_set_state(bool active)
{
	struct anx7816_platform_data *pdata = g_pdata;
	struct pinctrl_state *pin_state;
	int rc = -EFAULT;

	if (IS_ERR_OR_NULL(pdata->pin_res.pinctrl))
		return PTR_ERR(pdata->pin_res.pinctrl);

	pin_state = active ? pdata->pin_res.gpio_state_active
				: pdata->pin_res.gpio_state_suspend;

	if (!IS_ERR_OR_NULL(pin_state)) {
		rc = pinctrl_select_state(pdata->pin_res.pinctrl, pin_state);
		if (rc)
			pr_err("%s %s: can not set %s pin\n", LOG_TAG, __func__,
					active ? ANX_PINCTRL_STATE_DEFAULT
					: ANX_PINCTRL_STATE_SLEEP);
	} else
		pr_err("%s %s: invalid '%s' pinstate\n", LOG_TAG, __func__,
				active ? ANX_PINCTRL_STATE_DEFAULT
				: ANX_PINCTRL_STATE_SLEEP);

	return rc;
}

static int anx7816_pinctrl_init(struct device *dev,
					struct anx7816_platform_data *pdata)
{
	pdata->pin_res.pinctrl = devm_pinctrl_get(dev);
	if (IS_ERR_OR_NULL(pdata->pin_res.pinctrl)) {
		pr_err("%s %s: failed to get pinctrl for reset\n",
						LOG_TAG, __func__);
		return PTR_ERR(pdata->pin_res.pinctrl);
	}

	pdata->pin_res.gpio_state_active
			= pinctrl_lookup_state(pdata->pin_res.pinctrl,
					ANX_PINCTRL_STATE_DEFAULT);
	if (IS_ERR_OR_NULL(pdata->pin_res.gpio_state_active))
		pr_warn("%s %s: can not get default pinstate\n",
						LOG_TAG, __func__);

	pdata->pin_res.gpio_state_suspend
			= pinctrl_lookup_state(pdata->pin_res.pinctrl,
					ANX_PINCTRL_STATE_SLEEP);
	if (IS_ERR_OR_NULL(pdata->pin_res.gpio_state_suspend))
		pr_warn("%s %s: can not get sleep pinstate\n",
						LOG_TAG, __func__);

	return 0;
}

/*static unsigned int cable_smem_size; */

struct msm_hdmi_slimport_ops *hdmi_slimport_ops;

#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
void slimport_set_hdmi_hpd(int on)
{
	int rc = 0;
	static int hdmi_hpd_flag;

	pr_info("%s %s:+\n", LOG_TAG, __func__);

	if (on && hdmi_hpd_flag != 1) {
		hdmi_hpd_flag = 1;
		rc = hdmi_slimport_ops->set_upstream_hpd(g_pdata->hdmi_pdev, 1);
		pr_debug("%s %s: hpd on = %s\n", LOG_TAG, __func__,
				rc ? "failed" : "passed");
		if (rc) {
			msleep(2000);
			rc = hdmi_slimport_ops->set_upstream_hpd(
							g_pdata->hdmi_pdev, 1);
		}
	} else if (!on && hdmi_hpd_flag != 0) {
		hdmi_hpd_flag = 0;
		rc = hdmi_slimport_ops->set_upstream_hpd(g_pdata->hdmi_pdev, 0);
		pr_debug("%s %s: hpd off = %s\n", LOG_TAG, __func__,
				rc ? "failed" : "passed");

	} else {
		pr_debug("%s %s: hpd status is stupid.\n", LOG_TAG, __func__);
	}

	pr_debug("%s %s:-\n", LOG_TAG, __func__);
}
#endif

bool slimport_is_connected(void)
{
	struct anx7816_platform_data *pdata = NULL;
	bool result = false;
	int i = 0;

	/* TODO. we have no detection for now, then hack for now */
	pr_info("%s: no cable detection is supported. Forced connected(HACK)\n",
							__func__);
	return true;

	if (!anx7816_client)
		return false;

#ifdef CONFIG_OF
	pdata = g_pdata;
#else
	pdata = anx7816_client->dev.platform_data;
#endif

	if (!pdata)
		return false;

	/* spin_lock(&pdata->lock); */

	while (i < 30) {
		if (gpio_get_value_cansleep(pdata->gpio_cbl_det)) {
			pr_err("%s %s : Slimport Dongle is detected\n",
					LOG_TAG, __func__);
			result = true;
			break;
		}
		msleep(10);
		i++;
	}
	/* spin_unlock(&pdata->lock); */

	return result;
}
EXPORT_SYMBOL(slimport_is_connected);

bool slimport_is_check(void)
{
	struct anx7816_platform_data *pdata = NULL;

	if (!anx7816_client)
		return false;

	pdata = anx7816_client->dev.platform_data;

	if (!pdata)
		return false;

	return pdata->check_slimport_connection;
}
EXPORT_SYMBOL(slimport_is_check);

static int slimport7816_avdd_power(unsigned int onoff)
{
#ifdef CONFIG_OF
	struct anx7816_platform_data *pdata = g_pdata;
#else
	struct anx7816_platform_data *pdata = anx7816_client->dev.platform_data;
#endif
	int rc = 0;

	if (onoff) {
		pr_debug("%s %s: avdd power on\n", LOG_TAG, __func__);
		rc = regulator_enable(pdata->avdd_33);
		if (rc < 0) {
			pr_err("%s %s: failed to enable avdd regulator rc=%d\n",
						LOG_TAG, __func__, rc);
			goto err;
		}

		rc = regulator_enable(pdata->vdd_18);
		if (rc < 0) {
			pr_err("%s %s: failed to enable vdd_18 reg. rc=%d\n",
						LOG_TAG, __func__, rc);
			goto err_vdd_18;
		}
	} else {
		int tmp = 0;

		pr_debug("%s %s: avdd power off\n", LOG_TAG, __func__);

		rc = regulator_disable(pdata->vdd_18);
		if (rc < 0) {
			pr_err("%s %s: failed to disable vdd_18 reg. rc=%d\n",
						LOG_TAG, __func__, rc);
			tmp = rc;
		}

		rc = regulator_disable(pdata->avdd_33);
		if (rc < 0) {
			pr_err("%s %s: failed to disable avdd reg. rc=%d\n",
						LOG_TAG, __func__, rc);
			tmp = rc;
		}

		rc = tmp;
	}

	return rc;
err_vdd_18:
	regulator_disable(pdata->avdd_33);
err:
	return rc;
}

static ssize_t
slimport7816_rev_check_store(
	struct device *dev, struct device_attribute *attr,
	 const char *buf, size_t count)
{
	int cmd;

	sscanf(buf, "%d", &cmd);
	switch (cmd) {
	case 1:
		/* sp_tx_chip_located(); */
		break;
	}
	return count;
}
/*sysfs interface : Enable or Disable HDCP by default*/
static ssize_t sp_hdcp_feature_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hdcp_disable);
}

static ssize_t sp_hdcp_feature_store(struct device *dev,
			struct device_attribute *attr,
			const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	hdcp_disable = val;
	pr_debug(" hdcp_disable = %d\n", hdcp_disable);
	return count;
}

/*sysfs  interface : HDCP switch for VGA dongle*/
static ssize_t sp_external_block_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", external_block_en);
}

static ssize_t sp_external_block_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	external_block_en = val;
	return count;
}

/*sysfs  interface : i2c_master_read_reg, i2c_master_write_reg
anx7730 addr id:: DP_rx(0x50:0, 0x8c:1) HDMI_tx(0x72:5, 0x7a:6, 0x70:7)
ex:read ) 05df   = read:0  id:5 reg:0xdf
ex:write) 15df5f = write:1 id:5 reg:0xdf val:0x5f
*/
static ssize_t anx7730_write_reg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	char op, i;
	char r[3];
	char v[3];
	unchar tmp;
	int id, reg, val = 0;

	if (sp_tx_cur_states() != STATE_PLAY_BACK) {
		pr_err("%s: error!, Not STATE_PLAY_BACK\n", LOG_TAG);
		return -EINVAL;
	}

	if (sp_tx_cur_cable_type() != DWN_STRM_IS_HDMI) {
		pr_err("%s: error!, rx is not anx7730\n", LOG_TAG);
		return -EINVAL;
	}

	if (count != 7 && count != 5) {
		pr_err("%s: cnt:%d, invalid input!\n", LOG_TAG, (int)count-1);
		pr_err("%s: ex) 05df  -> op:0(read)  id:5 reg:0xdf\n",
							LOG_TAG);
		pr_err("%s: ex) 15df5f-> op:1(write) id:5 reg:0xdf val:0x5f\n",
							LOG_TAG);
		return -EINVAL;
	}

	ret = snprintf(&op, 2, buf);
	ret = snprintf(&i, 2, buf+1);
	ret = snprintf(r, 3, buf+2);

	ret = kstrtoul(&i, 10, (unsigned long *)&id);
	ret += kstrtoul(r, 16, (unsigned long *)&reg);
	if (ret)
		return -EINVAL;

	if ((id != 0 && id != 1 && id != 5 && id != 6 && id != 7)) {
		pr_err("%s: invalid addr id! (id:0,1,5,6,7)\n", LOG_TAG);
		return -EINVAL;
	}

	switch (op) {
	case 0x30: /* "0" -> read */
		i2c_master_read_reg(id, reg, &tmp);
		pr_debug("%s: anx7730 read(%d,0x%x)= 0x%x\n",
						LOG_TAG, id, reg, tmp);
		break;

	case 0x31: /* "1" -> write */
		ret = snprintf(v, 3, buf+4);
		ret = kstrtoul(v, 16, (unsigned long *)&val);
		if (ret)
			return -EINVAL;

		i2c_master_write_reg(id, reg, val);
		i2c_master_read_reg(id, reg, &tmp);
		pr_debug("%s: anx7730 write(%d,0x%x,0x%x)\n",
						LOG_TAG, id, reg, tmp);
		break;

	default:
		pr_err("%s: invalid operation code! (0:read, 1:write)\n",
						LOG_TAG);
		return -EINVAL;
	}

	return count;
}

/*sysfs  interface : sp_read_reg, sp_write_reg
anx7816 addr id:: HDMI_rx(0x7e:0, 0x80:1) DP_tx(0x72:5, 0x7a:6, 0x78:7)
ex:read ) 05df   = read:0  id:5 reg:0xdf
ex:write) 15df5f = write:1 id:5 reg:0xdf val:0x5f
*/
static int anx7816_id_change(int id)
{
	int chg_id = 0;

	switch (id) {
	case 0:
		chg_id = 0x7e; /* RX_P0 */
		break;
	case 1:
		chg_id = 0x80; /* RX_P1 */
		break;
	case 5:
		chg_id = 0x72; /* TX_P2 */
		break;
	case 6:
		chg_id = 0x7a; /* TX_P1 */
		break;
	case 7:
		chg_id = 0x78; /* TX_P0 */
		break;
	}
	return chg_id;
}

static ssize_t anx7816_write_reg_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret = 0;
	char op, i;
	char r[3];
	char v[3];
	unchar tmp;
	int id, reg, val = 0;

	if (sp_tx_cur_states() != STATE_PLAY_BACK) {
		pr_err("%s: error!, Not STATE_PLAY_BACK\n", LOG_TAG);
		return -EINVAL;
	}

	if (count != 7 && count != 5) {
		pr_err("%s: cnt:%d, invalid input!\n", LOG_TAG, (int)count-1);
		pr_err("%s: ex) 05df  -> op:0(read)  id:5 reg:0xdf\n",
							LOG_TAG);
		pr_err("%s: ex) 15df5f-> op:1(write) id:5 reg:0xdf val:0x5f\n",
							LOG_TAG);
		return -EINVAL;
	}

	ret = snprintf(&op, 2, buf);
	ret = snprintf(&i, 2, buf+1);
	ret = snprintf(r, 3, buf+2);

	ret = kstrtoul(&i, 10, (unsigned long *)&id);
	ret += kstrtoul(r, 16, (unsigned long *)&reg);
	if (ret)
		return -EINVAL;

	if ((id != 0 && id != 1 && id != 5 && id != 6 && id != 7)) {
		pr_err("%s: invalid addr id! (id:0,1,5,6,7)\n", LOG_TAG);
		return -EINVAL;
	}

	id = anx7816_id_change(id); /* ex) 5 -> 0x72 */

	switch (op) {
	case 0x30: /* "0" -> read */
		sp_read_reg(id, reg, &tmp);
		pr_debug("%s: anx7816 read(0x%x,0x%x)= 0x%x\n",
						LOG_TAG, id, reg, tmp);
		break;

	case 0x31: /* "1" -> write */
		ret = snprintf(v, 3, buf+4);
		val = simple_strtoul(v, NULL, 16);

		sp_write_reg(id, reg, val);
		sp_read_reg(id, reg, &tmp);
		pr_debug("%s: anx7816 write(0x%x,0x%x,0x%x)\n",
						LOG_TAG, id, reg, tmp);
		break;

	default:
		pr_err("%s: invalid operation code! (0:read, 1:write)\n",
						LOG_TAG);
		return -EINVAL;
	}

	return count;
}

static ssize_t slimport_sysfs_rda_hdmi_vga(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	int ret;
	ret = is_slimport_vga();

	return snprintf(buf, PAGE_SIZE, "%d", ret);
}

#ifdef SP_REGISTER_SET_TEST /* Slimport test */
/*sysfs read interface*/
static ssize_t ctrl_reg0_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG0, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg0_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG0 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg1_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG1, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg1_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG1 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg2_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG2, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg2_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG2 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg3_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG3, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg3_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG3 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg4_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG4, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg4_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG4 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg5_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG5, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg5_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG5 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg6_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG6, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg6_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG6 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg7_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG7, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg7_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG7 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg8_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG8, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg8_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG8 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg9_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG9, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg9_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG9 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg10_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG10, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg10_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG10 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg11_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG11, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg11_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG11 = val;
	return count;
}


/*sysfs read interface*/
static ssize_t ctrl_reg12_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG12, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg12_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG12 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg13_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG13, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg13_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG13 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg14_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG14, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg14_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG14 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg15_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG15, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg15_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG15 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg16_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG16, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg16_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG16 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg17_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG17, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg17_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG17 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg18_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG18, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg18_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG18 = val;
	return count;
}

/*sysfs read interface*/
static ssize_t ctrl_reg19_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	unchar c;
	sp_read_reg(TX_P1, SP_TX_LT_CTRL_REG19, &c);

	return snprintf(buf, PAGE_SIZE, "0x%x\n", c);
}

/*sysfs write interface*/
static ssize_t ctrl_reg19_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG19 = val;
	return count;
}
#endif

static ssize_t anx7816_enable_irq_show(struct device *dev,
		struct device_attribute *attr,
		char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", irq_enable);
}

static ssize_t anx7816_enable_irq_store(struct device *dev,
		struct device_attribute *attr,
		const char *buf, size_t count)
{
	int ret;
	long val;

	if (!anx7816_client)
		return -ENODEV;
	ret = kstrtol(buf, 10, &val);

	if (ret)
		return ret;

	irq_enable = val;
	return count;
}

static struct device_attribute slimport_device_attrs[] = {
	__ATTR(rev_check, S_IWUSR, NULL, slimport7816_rev_check_store),
	__ATTR(hdcp_disable, S_IRUGO | S_IWUSR, sp_hdcp_feature_show,
					sp_hdcp_feature_store),
	__ATTR(hdcp_switch, S_IRUGO | S_IWUSR, sp_external_block_show,
					sp_external_block_store),
	__ATTR(hdmi_vga, S_IRUGO, slimport_sysfs_rda_hdmi_vga, NULL),
	__ATTR(anx7730, S_IWUSR, NULL, anx7730_write_reg_store),
	__ATTR(anx7816, S_IWUSR, NULL, anx7816_write_reg_store),
	__ATTR(enable_irq, S_IRUGO | S_IWUSR, anx7816_enable_irq_show,
			anx7816_enable_irq_store),
#ifdef SP_REGISTER_SET_TEST /*slimport test */
	__ATTR(ctrl_reg0, S_IRUGO | S_IWUSR, ctrl_reg0_show, ctrl_reg0_store),
	__ATTR(ctrl_reg1, S_IRUGO | S_IWUSR, ctrl_reg1_show, ctrl_reg1_store),
	__ATTR(ctrl_reg2, S_IRUGO | S_IWUSR, ctrl_reg2_show, ctrl_reg2_store),
	__ATTR(ctrl_reg3, S_IRUGO | S_IWUSR, ctrl_reg3_show, ctrl_reg3_store),
	__ATTR(ctrl_reg4, S_IRUGO | S_IWUSR, ctrl_reg4_show, ctrl_reg4_store),
	__ATTR(ctrl_reg5, S_IRUGO | S_IWUSR, ctrl_reg5_show, ctrl_reg5_store),
	__ATTR(ctrl_reg6, S_IRUGO | S_IWUSR, ctrl_reg6_show, ctrl_reg6_store),
	__ATTR(ctrl_reg7, S_IRUGO | S_IWUSR, ctrl_reg7_show, ctrl_reg7_store),
	__ATTR(ctrl_reg8, S_IRUGO | S_IWUSR, ctrl_reg8_show, ctrl_reg8_store),
	__ATTR(ctrl_reg9, S_IRUGO | S_IWUSR, ctrl_reg9_show, ctrl_reg9_store),
	__ATTR(ctrl_reg10, S_IRUGO | S_IWUSR, ctrl_reg10_show,
							ctrl_reg10_store),
	__ATTR(ctrl_reg11, S_IRUGO | S_IWUSR, ctrl_reg11_show,
							ctrl_reg11_store),
	__ATTR(ctrl_reg12, S_IRUGO | S_IWUSR, ctrl_reg12_show,
							ctrl_reg12_store),
	__ATTR(ctrl_reg13, S_IRUGO | S_IWUSR, ctrl_reg13_show,
							ctrl_reg13_store),
	__ATTR(ctrl_reg14, S_IRUGO | S_IWUSR, ctrl_reg14_show,
							ctrl_reg14_store),
	__ATTR(ctrl_reg15, S_IRUGO | S_IWUSR, ctrl_reg15_show,
							ctrl_reg15_store),
	__ATTR(ctrl_reg16, S_IRUGO | S_IWUSR, ctrl_reg16_show,
							ctrl_reg16_store),
	__ATTR(ctrl_reg17, S_IRUGO | S_IWUSR, ctrl_reg17_show,
							ctrl_reg17_store),
	__ATTR(ctrl_reg18, S_IRUGO | S_IWUSR, ctrl_reg18_show,
							ctrl_reg18_store),
	__ATTR(ctrl_reg19, S_IRUGO | S_IWUSR, ctrl_reg19_show,
							ctrl_reg19_store),
#endif
};

static int create_sysfs_interfaces(struct device *dev)
{
	int i;

	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++)
		if (device_create_file(dev, &slimport_device_attrs[i]))
			goto error;
	return 0;
error:
	for ( ; i >= 0; i--)
		device_remove_file(dev, &slimport_device_attrs[i]);
	pr_err("%s %s: Unable to create interface", LOG_TAG, __func__);
	return -EINVAL;
}

int sp_read_reg_byte(uint8_t slave_addr, uint8_t offset)
{
	int ret = 0;

	anx7816_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(anx7816_client, offset);
	if (ret < 0) {
		pr_err("%s %s: failed to read i2c addr=%x\n", LOG_TAG,
			__func__, slave_addr);
		return ret;
	}
	return 0;
}

int sp_read_reg(uint8_t slave_addr, uint8_t offset, uint8_t *buf)
{
	int ret = 0;

	anx7816_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_read_byte_data(anx7816_client, offset);
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

	anx7816_client->addr = (slave_addr >> 1);
	ret = i2c_smbus_write_byte_data(anx7816_client, offset, value);
	if (ret < 0) {
		pr_err("%s %s: failed to write i2c addr=%x\n", LOG_TAG,
			__func__, slave_addr);
	}
	return ret;
}

int anx7816_clk_start(struct anx7816_platform_data *pdata);
void anx7816_clk_stop(struct anx7816_platform_data *pdata);

void sp_tx_hardware_poweron(void)
{
	int rc = 0;
#ifdef CONFIG_OF
	struct anx7816_platform_data *pdata = g_pdata;
#else
	struct anx7816_platform_data *pdata = anx7816_client->dev.platform_data;
#endif
	pr_debug("%s %s: anx7816 power on\n", LOG_TAG, __func__);

	rc = anx7816_pinctrl_rst_set_state(true);
	if (rc < 0) {
		pr_err("%s %s: fail to set_state for reset. ret=%d\n",
						LOG_TAG, __func__, rc);
		return;
	}

	if (gpio_is_valid(pdata->gpio_reset))
		gpio_set_value(pdata->gpio_reset, 0);
	udelay(1000);

	if (gpio_is_valid(pdata->gpio_p_dwn))
		gpio_set_value(pdata->gpio_p_dwn, 1);
	udelay(2000);

	rc = regulator_enable(pdata->dvdd_10);
	if (rc < 0) {
		pr_err("%s %s: failed to enable dvdd_10 regulator rc=%d\n",
						LOG_TAG, __func__, rc);
		goto err;
	}
	usleep_range(4500, 5000);

	if (gpio_is_valid(pdata->gpio_p_dwn))
		gpio_set_value(pdata->gpio_p_dwn, 0);
	udelay(2000);

	rc = anx7816_clk_start(pdata);
	if (rc < 0) {
		pr_err("%s %s: fail to start the dp_brdge_mclk. ret=%d\n",
						LOG_TAG, __func__, rc);
		goto err_vdd_10;
	}
	usleep_range(9000, 10000);

	pdata->check_slimport_connection = true;

	if (gpio_is_valid(pdata->gpio_reset))
		gpio_set_value(pdata->gpio_reset, 1);

	usleep_range(9000, 10000);

	return;

err_vdd_10:
	rc = regulator_disable(pdata->dvdd_10);
	if (rc < 0)
		pr_err("%s %s: fail to disable dvdd_10 regulator. rc=%d\n",
						LOG_TAG, __func__, rc);
err:
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_set_value(pdata->gpio_reset, 0);

	if (gpio_is_valid(pdata->gpio_p_dwn))
		gpio_set_value(pdata->gpio_p_dwn, 1);

	anx7816_pinctrl_rst_set_state(false);
}

void sp_tx_hardware_powerdown(void)
{
	int rc = -EFAULT;
#ifdef CONFIG_OF
	struct anx7816_platform_data *pdata = g_pdata;
#else
	struct anx7816_platform_data *pdata = anx7816_client->dev.platform_data;
#endif

	pr_debug("%s %s: anx7816 power down\n", LOG_TAG, __func__);
	if (gpio_is_valid(pdata->gpio_reset))
		gpio_set_value(pdata->gpio_reset, 0);

	udelay(1000);

	anx7816_clk_stop(pdata);
	udelay(2000);

	pdata->check_slimport_connection = false;
	if (gpio_is_valid(pdata->gpio_p_dwn))
		gpio_set_value(pdata->gpio_p_dwn, 1);

	regulator_disable(pdata->dvdd_10);
	rc = anx7816_pinctrl_rst_set_state(false);
	if (rc < 0)
		pr_err("%s %s: fail to set_state for reset. ret=%d\n",
							LOG_TAG, __func__, rc);
	udelay(1000);
}

int slimport_read_edid_block(int block, uint8_t *edid_buf)
{
	pr_debug("%s is called\n", __func__);

	if (block == 0) {
		memcpy(edid_buf, edid_blocks, 128 * sizeof(char));
	} else if (block == 1) {
		memcpy(edid_buf, (edid_blocks + 128), 128 * sizeof(char));
	} else {
		pr_err("%s %s: block number %d is invalid\n",
			   LOG_TAG, __func__, block);
		return -EINVAL;
	}

	return 0;
}
EXPORT_SYMBOL(slimport_read_edid_block);

static void anx7816_free_gpio(struct anx7816_data *anx7816)
{
/*	gpio_free(anx7816->pdata->gpio_int); */

	if (gpio_is_valid(anx7816->pdata->gpio_reset))
		gpio_free(anx7816->pdata->gpio_reset);
	if (gpio_is_valid(anx7816->pdata->gpio_p_dwn))
		gpio_free(anx7816->pdata->gpio_p_dwn);

}
static int anx7816_init_gpio(struct anx7816_data *anx7816)
{
	int ret = 0;

	pr_debug("anx7816 init gpio\n");

	if (gpio_is_valid(anx7816->pdata->gpio_p_dwn)) {
		ret = gpio_request(anx7816->pdata->gpio_p_dwn, "anx_p_dwn_ctl");
		if (ret) {
			pr_err("%s : failed to request gpio %d\n", __func__,
						anx7816->pdata->gpio_p_dwn);
			goto out;
		}
		gpio_direction_output(anx7816->pdata->gpio_p_dwn, 1);
		gpio_set_value(anx7816->pdata->gpio_p_dwn, 1);
	}

	if (gpio_is_valid(anx7816->pdata->gpio_reset)) {
		ret = gpio_request(anx7816->pdata->gpio_reset,
							"anx7816_reset_n");
		if (ret) {
			pr_err("%s:failed to request gpio_reset %d\n",
				__func__, anx7816->pdata->gpio_reset);
			goto err0;
		}
		gpio_direction_output(anx7816->pdata->gpio_reset, 1);
		gpio_set_value(anx7816->pdata->gpio_reset, 0);
	}

#if 0
	ret = gpio_request(anx7816->pdata->gpio_int, "anx7816_int_n");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
				anx7816->pdata->gpio_int);
		goto err1;
	}
	gpio_direction_input(anx7816->pdata->gpio_int);

	gpio_set_value(anx7816->pdata->gpio_reset, 0);
	gpio_set_value(anx7816->pdata->gpio_p_dwn, 1);

#endif


#ifdef USE_HDMI_SWITCH
	ret = gpio_request(hdmi_switch_gpio, "anx7816_hdmi_switch_gpio");
	if (ret) {
		pr_err("%s : failed to request gpio %d\n", __func__,
				hdmi_switch_gpio);
		goto err1;
	}
	gpio_direction_output(hdmi_switch_gpio, 0);
	udelay(1000);
	gpio_set_value(hdmi_switch_gpio, 1);
#endif

	goto out;

#ifdef USE_HDMI_SWITCH
err1:
	if (gpio_is_valid(anx7816->pdata->gpio_reset)
		gpio_free(anx7816->pdata->gpio_reset);
#endif

err0:
	if (gpio_is_valid(anx7816->pdata->gpio_p_dwn))
		gpio_free(anx7816->pdata->gpio_p_dwn);

out:
	return ret;
}

static int anx7816_system_init(void)
{
	int ret = 0;

	ret = slimport_chip_detect();
	if (ret == 0) {
		pr_err("%s : failed to detect anx7816\n", __func__);
		return -ENODEV;
	}

	slimport_chip_initial();
	return 0;
}

/* extern void dwc3_ref_clk_set(bool); */

/*static void dwc3_ref_clk_work_func(struct work_struct *work)
{
	struct anx7816_data *td = container_of(work, struct anx7816_data,
						dwc3_ref_clk_work.work);
	if(td->slimport_connected)
		dwc3_ref_clk_set(true);
	else
		dwc3_ref_clk_set(false);
}*/

void anx7816_hpd_cb(bool connected)
{
	struct anx7816_data *anx7816;

	if (!g_data) {
		pr_err("%s: g_data is not set \n", __func__);
		return;
	} else
		anx7816 = g_data;

	if (connected) {
		if (!anx7816->slimport_connected) {
			wake_lock(&anx7816->slimport_lock);
			anx7816->slimport_connected = true;
			pr_info("%s %s : detect cable insertion\n",
						LOG_TAG, __func__);
			queue_delayed_work(anx7816->workqueue,
						&anx7816->work, 0);
		} else
			pr_debug("%s: anx7816 is already ON\n", __func__);
	} else {
		if (anx7816->slimport_connected) {
			anx7816->slimport_connected = false;
			pr_info("%s %s : detect cable removal\n",
						LOG_TAG, __func__);
			cancel_delayed_work_sync(&anx7816->work);
			/* TODO: hack for P0 HDMI HPD detection */
			hdmi_hpd_hack(0);
			wake_unlock(&anx7816->slimport_lock);
			wake_lock_timeout(&anx7816->slimport_lock, 2*HZ);
		} else
			pr_debug("%s: anx7816 is already OFF\n", __func__);
	}
}

static void anx7816_work_func(struct work_struct *work)
{
	struct anx7816_data *td = container_of(work, struct anx7816_data,
								work.work);
	int workqueu_timer = 0;

	if (sp_tx_cur_states() >= STATE_PLAY_BACK)
		workqueu_timer = 500;
	else
		workqueu_timer = 100;
	mutex_lock(&td->lock);
	slimport_main_process();
	mutex_unlock(&td->lock);
	queue_delayed_work(td->workqueue, &td->work,
			   msecs_to_jiffies(workqueu_timer));
}

#ifdef CONFIG_OF
int anx7816_clk_get(struct device *dev, struct anx7816_platform_data *pdata)
{
	int rc = 0;

	pdata->mclk = devm_clk_get(dev, "dp_bridge_mclk");
	if (IS_ERR(pdata->mclk)) {
		pr_err("%s: failed to get dp_brdge_mclk. ret =%d\n",
							__func__, rc);
		return PTR_ERR(pdata->mclk);
	}

	return rc;
}

int anx7816_clk_start(struct anx7816_platform_data *pdata)
{
	int rc = 0;

	rc = clk_prepare_enable(pdata->mclk);
	if (rc)
		pr_err("%s:failed to start dp_brdge_mclk. ret=%d\n",
							__func__, rc);
	return rc;
}

void anx7816_clk_stop(struct anx7816_platform_data *pdata)
{
	clk_disable_unprepare(pdata->mclk);
}


int anx7816_regulator_configure(
	struct device *dev, struct anx7816_platform_data *pdata)
{
	int rc = 0;

	pr_debug("%s+\n", __func__);

	pdata->avdd_33 = regulator_get(dev, "analogix,vdd_ana");

	if (IS_ERR(pdata->avdd_33)) {
		rc = PTR_ERR(pdata->avdd_33);
		pr_err("%s : Regulator get failed avdd_33 rc=%d\n",
							__func__, rc);
		return rc;
	}

	pdata->vdd_18 = regulator_get(dev, "analogix,vdd_18");
	if (IS_ERR(pdata->vdd_18)) {
		rc = PTR_ERR(pdata->vdd_18);
		pr_err("%s : Regulator get failed vdd_18 rc=%d\n",
							__func__, rc);
		goto error_set_vtg_avdd_33;
	}

	pdata->dvdd_10 = regulator_get(dev, "analogix,vdd_10");
	if (IS_ERR(pdata->dvdd_10)) {
		rc = PTR_ERR(pdata->dvdd_10);
		pr_err("%s : Regulator get failed dvdd_10 rc=%d\n",
			   __func__, rc);
		goto error_set_vtg_vdd_18;
	}

	return 0;

error_set_vtg_vdd_18:
	regulator_put(pdata->vdd_18);
error_set_vtg_avdd_33:
	regulator_put(pdata->avdd_33);

	return rc;
}

static int anx7816_parse_dt(
	struct device *dev, struct anx7816_platform_data *pdata)
{
	int rc = 0;
	struct device_node *np = dev->of_node;
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
	struct platform_device *sp_pdev = NULL;
	struct device_node *sp_tx_node = NULL;
#endif

	pdata->gpio_p_dwn = of_get_named_gpio_flags(np,
					"analogix,p-dwn-gpio", 0, NULL);
	if (!gpio_is_valid(pdata->gpio_p_dwn))
		pr_err("%s: Invalid p-dwn-gpio = %d\n",
					__func__, pdata->gpio_p_dwn);

	pdata->gpio_reset = of_get_named_gpio_flags(np,
					"analogix,reset-gpio", 0, NULL);
	if (!gpio_is_valid(pdata->gpio_reset))
		pr_err("%s: Invalid gpio_reset = %d\n",
					__func__, pdata->gpio_reset);


/*	pdata->gpio_int = of_get_named_gpio_flags(
		np, "analogix,irq-gpio", 0, NULL);

*/
	pr_debug("%s gpio p_dwn : %d, reset : %d, irq : %d, gpio_cbl_det %d\n",
			LOG_TAG, pdata->gpio_p_dwn,
			pdata->gpio_reset,
			pdata->gpio_int,
			pdata->gpio_cbl_det);

#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
	/* parse phandle for hdmi tx */
	sp_tx_node = of_parse_phandle(np, "qcom,hdmi-tx-map", 0);
	if (!sp_tx_node) {
		pr_err("%s %s: can't find hdmi phandle\n", LOG_TAG, __func__);
		return -ENODEV;
	}

	sp_pdev = of_find_device_by_node(sp_tx_node);
	if (!sp_pdev) {
		pr_err("%s %s: can't find the device by node\n",
				LOG_TAG, __func__);
		return -ENODEV;
	}
	pdata->hdmi_pdev = sp_pdev;
#endif
	if (anx7816_regulator_configure(dev, pdata) < 0) {
		pr_err("%s %s: parsing dt for anx7816 is failed.\n",
				LOG_TAG, __func__);
		return rc;
	}

	if (anx7816_clk_get(dev, pdata) < 0) {
		pr_err("%s %s: failed to get clks\n", LOG_TAG, __func__);
		return rc;
	}

	/* connects function nodes which are not provided with dts */
	pdata->avdd_power = slimport7816_avdd_power;

#ifdef USE_HDMI_SWITCH
	hdmi_switch_gpio = of_get_named_gpio_flags(np,
					"analogix,hdmi-switch-gpio", 0, NULL);
	pr_debug("%s hdmi_switch_gpio : %d\n", LOG_TAG, hdmi_switch_gpio);
#endif
	return 0;
}
#else
static int anx7816_parse_dt(
	struct device *dev, struct anx7816_platform_data *pdata)
{
	return -ENODEV;
}
#endif


static int slimport_mod_display_handle_available(void *data)
{
	struct anx7816_data *anx7816;

	pr_debug("%s+\n", __func__);

	anx7816 = (struct anx7816_data *)data;

	pr_debug("%s-\n", __func__);

	return 0;
}

static int slimport_mod_display_handle_unavailable(void *data)
{
	struct anx7816_data *anx7816;

	pr_debug("%s+\n", __func__);

	anx7816 = (struct anx7816_data *)data;

	pr_debug("%s-\n", __func__);

	return 0;
}

static int slimport_mod_display_handle_connect(void *data)
{
	struct anx7816_data *anx7816;

	pr_debug("%s+\n", __func__);

	anx7816 = (struct anx7816_data *)data;

	pr_debug("%s-\n", __func__);

	return 0;
}

static int slimport_mod_display_handle_disconnect(void *data)
{
	struct anx7816_data *anx7816;

	pr_debug("%s+\n", __func__);

	anx7816 = (struct anx7816_data *)data;

	pr_debug("%s-\n", __func__);

	return 0;
}

static struct mod_display_ops slimport_mod_display_ops = {
	.handle_available = slimport_mod_display_handle_available,
	.handle_unavailable = slimport_mod_display_handle_unavailable,
	.handle_connect = slimport_mod_display_handle_connect,
	.handle_disconnect = slimport_mod_display_handle_disconnect,
	.data = NULL,
};

static struct mod_display_impl_data slimport_mod_display_impl = {
	.mod_display_type = MOD_DISPLAY_TYPE_DP,
	.ops = &slimport_mod_display_ops,
};

/*
int anx7816_get_sbl_cable_type(void)
{
	int cable_type = 0;
	unsigned int *p_cable_type = (unsigned int *)
		(smem_get_entry(SMEM_ID_VENDOR1, &cable_smem_size));

	if (p_cable_type)
		cable_type = *p_cable_type;
	else
		cable_type = 0;

	return cable_type;
}
*/
static int anx7816_i2c_probe(struct i2c_client *client,
			const struct i2c_device_id *id)
{

	struct anx7816_data *anx7816;
	struct anx7816_platform_data *pdata;
	int ret = 0;
	/* int sbl_cable_type = 0; */

	pr_debug("%s %s start\n", LOG_TAG, __func__);

#ifdef SP_REGISTER_SET_TEST
	val_SP_TX_LT_CTRL_REG0 = 0x01;
	val_SP_TX_LT_CTRL_REG1 = 0x03;
	val_SP_TX_LT_CTRL_REG2 = 0x57;
	val_SP_TX_LT_CTRL_REG3 = 0x7f;
	val_SP_TX_LT_CTRL_REG4 = 0x71;
	val_SP_TX_LT_CTRL_REG5 = 0x6b;
	val_SP_TX_LT_CTRL_REG6 = 0x7f;
	val_SP_TX_LT_CTRL_REG7 = 0x73;
	val_SP_TX_LT_CTRL_REG8 = 0x7f;
	val_SP_TX_LT_CTRL_REG9 = 0x7f;
	val_SP_TX_LT_CTRL_REG10 = 0x00;
	val_SP_TX_LT_CTRL_REG11 = 0x00;
	val_SP_TX_LT_CTRL_REG12 = 0x02;
	val_SP_TX_LT_CTRL_REG13 = 0x00;
	val_SP_TX_LT_CTRL_REG14 = 0x0c;
	val_SP_TX_LT_CTRL_REG15 = 0x42;
	val_SP_TX_LT_CTRL_REG16 = 0x2f;
	val_SP_TX_LT_CTRL_REG17 = 0x3e;
	val_SP_TX_LT_CTRL_REG18 = 0x77;
	val_SP_TX_LT_CTRL_REG19 = 0x7e;
#endif
	if (!i2c_check_functionality(client->adapter,
		I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("%s: i2c bus does not support the anx7816\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	anx7816 = kzalloc(sizeof(struct anx7816_data), GFP_KERNEL);
	if (!anx7816) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto exit;
	}

	if (client->dev.of_node) {
		pdata = devm_kzalloc(&client->dev,
				 sizeof(struct anx7816_platform_data),
				 GFP_KERNEL);
		if (!pdata) {
			pr_err("%s: Failed to allocate memory\n", __func__);
			return -ENOMEM;
		}
		client->dev.platform_data = pdata;
	/* device tree parsing function call */
		ret = anx7816_parse_dt(&client->dev, pdata);
		if (ret != 0) /* if occurs error */
			goto err0;

		anx7816->pdata = pdata;
	} else {
		anx7816->pdata = client->dev.platform_data;
	}

	/* to access global platform data */
	g_pdata = anx7816->pdata;
	g_data = anx7816;

	anx7816_client = client;

	mutex_init(&anx7816->lock);

	if (!anx7816->pdata) {
		ret = -EINVAL;
		goto err0;
	}

	ret = anx7816_pinctrl_init(&client->dev, pdata);
	if (ret) {
		pr_err("%s : failed to call pinctrl_init. ret = %d\n",
							__func__, ret);
		goto err0;
	}

	ret = anx7816_init_gpio(anx7816);
	if (ret) {
		pr_err("%s: failed to initialize gpio\n", __func__);
		goto err0;
	}

	INIT_DELAYED_WORK(&anx7816->work, anx7816_work_func);
/*	INIT_DELAYED_WORK(&anx7816->dwc3_ref_clk_work, dwc3_ref_clk_work_func);
*/

	anx7816->workqueue = create_singlethread_workqueue("anx7816_work");
	if (anx7816->workqueue == NULL) {
		pr_err("%s: failed to create work queue\n", __func__);
		ret = -ENOMEM;
		goto err1;
	}

	anx7816->pdata->avdd_power(1);

	ret = anx7816_system_init();
	if (ret) {
		pr_err("%s: failed to initialize anx7816\n", __func__);
		goto err2;
	}
	wake_lock_init(&anx7816->slimport_lock, WAKE_LOCK_SUSPEND,
				"slimport_wake_lock");

	ret = create_sysfs_interfaces(&client->dev);
	if (ret < 0) {
		pr_err("%s : sysfs register failed", __func__);
		goto err3;
	}

#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
	hdmi_slimport_ops = devm_kzalloc(&client->dev,
				    sizeof(struct msm_hdmi_slimport_ops),
				    GFP_KERNEL);
	if (!hdmi_slimport_ops) {
		pr_err("%s: alloc hdmi slimport ops failed\n", __func__);
		ret = -ENOMEM;
		goto err3;
	}

	if (anx7816->pdata->hdmi_pdev) {
		ret = msm_hdmi_register_slimport(anx7816->pdata->hdmi_pdev,
					   hdmi_slimport_ops, anx7816);
		if (ret) {
			pr_err("%s: register with hdmi failed\n", __func__);
			ret = -EPROBE_DEFER;
			goto err3;
		}
	}
#endif

	slimport_mod_display_ops.data = (void *)anx7816;
	mod_display_register_impl(&slimport_mod_display_impl);

	pr_debug("%s %s end\n", LOG_TAG, __func__);
	goto exit;

err3:
	free_irq(client->irq, anx7816);
err2:
	destroy_workqueue(anx7816->workqueue);
err1:
	anx7816_free_gpio(anx7816);
err0:
	anx7816_client = NULL;
	kfree(anx7816);
exit:
	return ret;
}

static int anx7816_i2c_remove(struct i2c_client *client)
{
	struct anx7816_data *anx7816 = i2c_get_clientdata(client);
	int i = 0;

	for (i = 0; i < ARRAY_SIZE(slimport_device_attrs); i++)
		device_remove_file(&client->dev, &slimport_device_attrs[i]);
	pr_warn("anx7816_i2c_remove\n");
	sp_tx_clean_state_machine();
	destroy_workqueue(anx7816->workqueue);
	sp_tx_hardware_powerdown();
	free_irq(client->irq, anx7816);
	anx7816_free_gpio(anx7816);
	wake_lock_destroy(&anx7816->slimport_lock);
	kfree(anx7816);
	return 0;
}

bool is_slimport_vga(void)
{
	return ((sp_tx_cur_cable_type() == DWN_STRM_IS_VGA_9832)
		|| (sp_tx_cur_cable_type() == DWN_STRM_IS_ANALOG)) ? 1 : 0;
}
/* 0x01: hdmi device is attached
    0x02: DP device is attached
    0x03: Old VGA device is attached // RX_VGA_9832
    0x04: new combo VGA device is attached // RX_VGA_GEN
    0x00: unknown device            */
EXPORT_SYMBOL(is_slimport_vga);

bool is_slimport_dp(void)
{
	return (sp_tx_cur_cable_type() == DWN_STRM_IS_DIGITAL) ? TRUE : FALSE;
}
EXPORT_SYMBOL(is_slimport_dp);

unchar sp_get_link_bw(void)
{
	return sp_tx_cur_bw();
}
EXPORT_SYMBOL(sp_get_link_bw);

void sp_set_link_bw(unchar link_bw)
{
	sp_tx_set_bw(link_bw);
}

unchar sp_get_rx_bw(void)
{
	return sp_rx_cur_bw();
}
EXPORT_SYMBOL(sp_get_rx_bw);

static int anx7816_i2c_suspend(struct i2c_client *client, pm_message_t state)
{
	return 0;
}

static int anx7816_i2c_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id anx7816_id[] = {
	{ "anx7816", 0 },
	{ }
};

MODULE_DEVICE_TABLE(i2c, anx7816_id);

#ifdef CONFIG_OF
static struct of_device_id anx_match_table[] = {
	{ .compatible = "analogix,anx7816",},
	{ },
};
#endif

static struct i2c_driver anx7816_driver = {
	.driver  = {
		.name  = "anx7816",
		.owner  = THIS_MODULE,
#ifdef CONFIG_OF
		.of_match_table = anx_match_table,
#endif
	},
	.probe  = anx7816_i2c_probe,
	.remove  = anx7816_i2c_remove,
	.suspend = anx7816_i2c_suspend,
	.resume = anx7816_i2c_resume,
	.id_table  = anx7816_id,
};

static void __init anx7816_init_async(void *data, async_cookie_t cookie)
{
	int ret = 0;

	ret = i2c_add_driver(&anx7816_driver);
	if (ret < 0)
		pr_err("%s: failed to register anx7816 i2c drivern", __func__);
}

static int __init anx7816_init(void)
{
	async_schedule(anx7816_init_async, NULL);
	return 0;
}

static void __exit anx7816_exit(void)
{
	i2c_del_driver(&anx7816_driver);
}

module_init(anx7816_init);
module_exit(anx7816_exit);

MODULE_DESCRIPTION("Slimport  transmitter ANX7816 driver");
MODULE_AUTHOR("Choonmin Lee <choonmin.lee@lge.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.5");
