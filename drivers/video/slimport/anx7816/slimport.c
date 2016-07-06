/*
 * Copyright(c) 2014, Analogix Semiconductor. All rights reserved.
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
#include "slimport.h"
#include "slimport_tx_drv.h"
#include "slimport_tx_reg.h"
#include <linux/platform_data/slimport_device.h>
#include <linux/pinctrl/consumer.h>
#include <linux/clk.h>
#include <linux/completion.h>
#ifdef QUICK_CHARGE_SUPPORT
#include "quick_charge.h"
#endif

#define MML_DYNAMIC_IRQ_SUPPORT 1

#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
#include "../../msm/mdss/mdss_hdmi_slimport.h"
#endif

#include <linux/mod_display.h>
#include <linux/mod_display_ops.h>

/* Enable or Disable HDCP by default */
/* hdcp_enable = 1: Enable,  0: Disable */
static int hdcp_enable = 1;

/* HDCP switch for external block*/
/* external_block_en = 1: enable, 0: disable*/
int external_block_en = 1;

/* to access global platform data */
static struct anx7816_platform_data *g_pdata;
static struct anx7816_data *g_data;

/* Use device tree structure data when defined "CONFIG_OF"  */
/*#define CONFIG_OF
#define SP_REGISTER_SET_TEST*/

#ifdef SP_REGISTER_SET_TEST
/*//For Slimport swing&pre-emphasis test*/
unchar val_SP_TX_LT_CTRL_REG0;
unchar val_SP_TX_LT_CTRL_REG10;
unchar val_SP_TX_LT_CTRL_REG11;
unchar val_SP_TX_LT_CTRL_REG2;
unchar val_SP_TX_LT_CTRL_REG12;
unchar val_SP_TX_LT_CTRL_REG1;
unchar val_SP_TX_LT_CTRL_REG6;
unchar val_SP_TX_LT_CTRL_REG16;
unchar val_SP_TX_LT_CTRL_REG5;
unchar val_SP_TX_LT_CTRL_REG8;
unchar val_SP_TX_LT_CTRL_REG15;
unchar val_SP_TX_LT_CTRL_REG18;
#endif

#define TRUE 1
#define FALSE 0

static int slimport7816_avdd_power(unsigned int onoff);
static int anx7816_enable_irq(int enable);

struct i2c_client *anx7816_client;

#define ANX_PINCTRL_STATE_DEFAULT "anx_default"
#define ANX_PINCTRL_STATE_SLEEP  "anx_sleep"

struct anx7816_pinctrl_res {
	struct pinctrl *pinctrl;
	struct pinctrl_state *gpio_state_active;
	struct pinctrl_state *gpio_state_suspend;
};

struct anx7816_platform_data {
	int gpio_p_dwn;
	int gpio_reset;
	int gpio_cbl_det;
	int gpio_v10_ctrl;
	int gpio_v33_ctrl;
	int external_ldo_control;
	int (*avdd_power)(unsigned int onoff);
	int (*dvdd_power)(unsigned int onoff);
	struct regulator *avdd_33;
	struct regulator *dvdd_10;
	struct regulator *vdd_18;
	spinlock_t lock;
	struct clk *mclk;
	struct anx7816_pinctrl_res pin_res;
	struct pinctrl *pinctrl;
	struct pinctrl_state *hdmi_pinctrl_active;
	struct pinctrl_state *hdmi_pinctrl_suspend;
	struct platform_device *hdmi_pdev;
	bool cbl_det_irq_enabled;
	struct mutex sp_tx_power_lock;
	bool sp_tx_power_state;
};

struct anx7816_data {
	struct anx7816_platform_data *pdata;
	struct delayed_work work;
	struct workqueue_struct *workqueue;
	struct mutex lock;
	struct wake_lock slimport_lock;
	atomic_t slimport_connected;
	struct completion connect_wait;
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


#ifdef QUICK_CHARGE_SUPPORT
extern struct blocking_notifier_head *get_notifier_list_head(void);
#endif

bool slimport_dongle_is_connected(void)
{
	struct anx7816_platform_data *pdata = NULL;
	bool result = false;

	if (!anx7816_client)
		return false;

#ifdef CONFIG_OF
	pdata = g_pdata;
#else
	pdata = anx7816_client->dev.platform_data;
#endif

	if (!pdata)
		return false;

	if (gpio_get_value(pdata->gpio_cbl_det) == DONGLE_CABLE_INSERT
		/*gpio_get_value_cansleep(pdata->gpio_cbl_det)*/) {
		pr_debug("%s %s : Slimport Dongle is detected\n",
			LOG_TAG, __func__);
		result = true;
	}

	return result;
}

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

/*static int slimport7816_dvdd_power(unsigned int onoff)
{
#ifdef CONFIG_OF
	struct anx7816_platform_data *pdata = g_pdata;
#else
	struct anx7816_platform_data *pdata = anx7816_client->dev.platform_data;
#endif
	int rc = 0;
*/
/* To do : regulator control after H/W change */
/*	return rc;
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
}*/

ssize_t slimport7816_rev_check_store(struct device *dev,
					struct device_attribute *attr,
					const char *buf, size_t count)
{
	int cmd;
	int result;

	result = sscanf(buf, "%d", &cmd);
	switch (cmd) {
	case 1:
		/*//sp_tx_chip_located();*/
		break;
	}
	return count;
}

/*sysfs interface : Enable or Disable HDCP by default*/
ssize_t sp_hdcp_feature_show(struct device *dev,
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", hdcp_enable);
}

ssize_t sp_hdcp_feature_store(struct device *dev,
				 struct device_attribute *attr, const char *buf,
				 size_t count)
{
	int ret;
	long val;
	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	hdcp_enable = val;
	pr_debug(" hdcp_enable = %d\n", hdcp_enable);
	return count;
}

/*sysfs  interface : HDCP switch for VGA dongle*/
ssize_t sp_external_block_show(struct device *dev,
				  struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "%d\n", external_block_en);
}

ssize_t sp_external_block_store(struct device *dev,
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

#ifdef MML_DYNAMIC_IRQ_SUPPORT
/* sysfs interface : Dynamic IRQ Enable Manual Control */
ssize_t sp_irq_enable_show(struct device *dev,
			   struct device_attribute *attr, char *buf)
{
	struct anx7816_data *anx7816;

	if (!g_data) {
		pr_err("%s: g_data is not set \n", __func__);
		return -ENODEV;
	} else
		anx7816 = g_data;

	return snprintf(buf, PAGE_SIZE, "%d\n", anx7816->pdata->cbl_det_irq_enabled);
}

ssize_t sp_irq_enable_store(struct device *dev,
			    struct device_attribute *attr,
			    const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;

	if (val != !!val)
		pr_err("%s: Invalid input: %s (%ld)\n", __func__, buf, val);
	else
		anx7816_enable_irq(val);

	return count;
}
#endif

/*sysfs  interface : i2c_master_read_reg, i2c_master_write_reg
anx7730 addr id:: DP_rx(0x50:0, 0x8c:1) HDMI_tx(0x72:5, 0x7a:6, 0x70:7)
ex:read ) 05df   = read:0  id:5 reg:0xdf
ex:write) 15df5f = write:1 id:5 reg:0xdf val:0x5f
*/
ssize_t anx7730_write_reg_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	char op, i;
	char r[3];
	char v[3];
	unchar tmp;
	int id, reg, val = 0;

	/*if (sp_tx_cur_states() != STATE_PLAY_BACK) {
		pr_err("%s: error!, Not STATE_PLAY_BACK\n", LOG_TAG);
		return -EINVAL;
	}*/

	/*if (sp_tx_cur_cable_type() != DWN_STRM_IS_HDMI_7730) {
		pr_err("%s: error!, rx is not anx7730\n", LOG_TAG);
		return -EINVAL;
	}*/

	if (count != 7 && count != 5) {
		pr_err("%s: cnt:%d, invalid input!\n", LOG_TAG, (int)count - 1);
		pr_err("%s: ex) 05df   -> op:0(read)  id:5 reg:0xdf\n",
		       LOG_TAG);
		pr_err("%s: ex) 15df5f -> op:1(wirte) id:5 reg:0xdf val:0x5f\n",
		       LOG_TAG);
		return -EINVAL;
	}

	ret = snprintf(&op, 2, buf);
	ret = snprintf(&i, 2, buf + 1);
	ret = snprintf(r, 3, buf + 2);

	ret = kstrtoul(&i, 10, (unsigned long *)&id);
	ret += kstrtoul(r, 16, (unsigned long *)&reg);
	if (ret)
		return -EINVAL;

	if ((id != 0 && id != 1 && id != 5 && id != 6 && id != 7)) {
		pr_err("%s: invalid addr id! (id:0,1,5,6,7)\n", LOG_TAG);
		return -EINVAL;
	}

	switch (op) {
	case 0x30:		/* "0" -> read */
		i2c_master_read_reg(id, reg, &tmp);
		pr_debug("%s: anx7730 read(%d,0x%x)= 0x%x\n", LOG_TAG, id, reg,
			tmp);
		break;

	case 0x31:		/* "1" -> write */
		ret = snprintf(v, 3, buf + 4);
		ret = kstrtoul(v, 16, (unsigned long *)&val);

		i2c_master_write_reg(id, reg, val);
		i2c_master_read_reg(id, reg, &tmp);
		pr_debug("%s: anx7730 write(%d,0x%x,0x%x)\n", LOG_TAG, id, reg,
			tmp);
		break;

	default:
		pr_err("%s: invalid operation code! (0:read, 1:write)\n",
		       LOG_TAG);
		return -EINVAL;
	}

	return count;
}

/*sysfs  interface : sp_read_reg, sp_write_reg
anx7816 addr id:: HDMI_rx(0x7e:0, 0x80:1) DP_tx(0x72:5, 0x7a:6, 0x70:7)
ex:read ) 05df   = read:0  id:5 reg:0xdf
ex:write) 15df5f = write:1 id:5 reg:0xdf val:0x5f
*/
static int anx7816_id_change(int id)
{
	int chg_id = 0;

	switch (id) {
	case 0:
		chg_id = RX_P0;
		break;
	case 1:
		chg_id = RX_P1;
		break;
	case 5:
		chg_id = TX_P2;
		break;
	case 6:
		chg_id = TX_P1;
		break;
	case 7:
		chg_id = TX_P0;
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

	/*if (sp_tx_cur_states() != STATE_PLAY_BACK) {
		pr_err("%s: error!, Not STATE_PLAY_BACK\n", LOG_TAG);
		return -EINVAL;
	}*/

	if (count != 7 && count != 5) {
		pr_err("%s: cnt:%d, invalid input!\n", LOG_TAG, (int)count - 1);
		pr_err("%s: ex) 05df   -> op:0(read)  id:5 reg:0xdf\n",
		       LOG_TAG);
		pr_err("%s: ex) 15df5f -> op:1(wirte) id:5 reg:0xdf val:0x5f\n",
		       LOG_TAG);
		return -EINVAL;
	}

	ret = snprintf(&op, 2, buf);
	ret = snprintf(&i, 2, buf + 1);
	ret = snprintf(r, 3, buf + 2);

	ret = kstrtoul(&i, 10, (unsigned long *)&id);
	ret += kstrtoul(r, 16, (unsigned long *)&reg);
	if (ret)
		return -EINVAL;

	if ((id != 0 && id != 1 && id != 5 && id != 6 && id != 7)) {
		pr_err("%s: invalid addr id! (id:0,1,5,6,7)\n", LOG_TAG);
		return -EINVAL;
	}

	id = anx7816_id_change(id);	/*//ex) 5 -> 0x72*/

	switch (op) {
	case 0x30:		/* "0" -> read */
		sp_read_reg(id, reg, &tmp);
		pr_debug("%s: anx7816 read(0x%x,0x%x)= 0x%x\n", LOG_TAG, id,
			reg, tmp);
		break;

	case 0x31:		/* "1" -> write */
		ret = snprintf(v, 3, buf + 4);
		ret = kstrtoul(v, 16, (unsigned long *)&val);
		if (ret)
			return -EINVAL;

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

static ssize_t anx_dpcd_store(struct device *dev,
				   struct device_attribute *attr,
				   const char *buf, size_t count)
{
	int ret = 0;
	char op;
	char offset[3];
	char val[3];

	unchar tmp, write_val;
	int reg;
	ret = snprintf(&op, 1, buf);
	ret = snprintf(offset, 2, buf+1);
	ret = kstrtoul(offset, 16, (unsigned long *)&reg);
	if (ret)
		return -EINVAL;

	switch (op) {
	case 0x30:
		/*read*/
		sp_tx_aux_dpcdread_bytes(0, 5, reg, 1, &tmp);
		pr_debug("%s: anx7816 read(0x05,0x%x)= 0x%x\n", LOG_TAG,
			reg, tmp);
		break;
	case 0x31:
		/*write*/
		ret = snprintf(val, 2, buf+3);
		ret = kstrtoul(val, 16, (unsigned long *)&write_val);
		sp_tx_aux_dpcdwrite_bytes(0, 5, reg, 1, &write_val);
		sp_tx_aux_dpcdread_bytes(0, 5, reg, 1, &tmp);
		pr_debug("%s: anx7816 write(0x05,0x%x,0x%x)= 0x%x\n", LOG_TAG,
			reg, write_val,	tmp);
		break;
	}
	return ret;
}


#ifdef SP_REGISTER_SET_TEST	/*//Slimport test*/
/*sysfs read interface*/
static ssize_t ctrl_reg0_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG0);
}

/*sysfs write interface*/
static ssize_t ctrl_reg0_store(struct device *dev,
			struct device_attribute *attr, const char *buf, size_t count)
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
static ssize_t ctrl_reg10_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG10);
}

/*sysfs write interface*/
static ssize_t ctrl_reg10_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
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
				struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG11);
}

/*sysfs write interface*/
static ssize_t ctrl_reg11_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
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
static ssize_t ctrl_reg2_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG2);
}

/*sysfs write interface*/
static ssize_t ctrl_reg2_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
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
static ssize_t ctrl_reg12_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG12);
}

/*sysfs write interface*/
static ssize_t ctrl_reg12_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
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
static ssize_t ctrl_reg1_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG1);
}

/*sysfs write interface*/
static ssize_t ctrl_reg1_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
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
static ssize_t ctrl_reg6_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG6);
}

/*sysfs write interface*/
static ssize_t ctrl_reg6_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
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
static ssize_t ctrl_reg16_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG16);
}

/*sysfs write interface*/
static ssize_t ctrl_reg16_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
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
static ssize_t ctrl_reg5_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG5);
}

/*sysfs write interface*/
static ssize_t ctrl_reg5_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
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
static ssize_t ctrl_reg8_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG8);
}

/*sysfs write interface*/
static ssize_t ctrl_reg8_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
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
static ssize_t ctrl_reg15_show(struct device *dev,
		struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG15);
}

/*sysfs write interface*/
static ssize_t ctrl_reg15_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
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
static ssize_t ctrl_reg18_show(struct device *dev,
			struct device_attribute *attr, char *buf)
{
	return snprintf(buf, PAGE_SIZE, "0x%x\n", val_SP_TX_LT_CTRL_REG18);
}

/*sysfs write interface*/
static ssize_t ctrl_reg18_store(struct device *dev,
		struct device_attribute *attr, const char *buf, size_t count)
{
	int ret;
	long val;

	ret = kstrtol(buf, 10, &val);
	if (ret)
		return ret;
	val_SP_TX_LT_CTRL_REG18 = val;
	return count;
}
#endif
/* for debugging */
static struct device_attribute slimport_device_attrs[] = {
	__ATTR(rev_check, S_IRUGO | S_IWUSR, NULL,
	       slimport7816_rev_check_store),
	__ATTR(hdcp, S_IRUGO | S_IWUSR, sp_hdcp_feature_show,
	       sp_hdcp_feature_store),
	__ATTR(hdcp_switch, S_IRUGO | S_IWUSR, sp_external_block_show,
	       sp_external_block_store),
#ifdef MML_DYNAMIC_IRQ_SUPPORT
	__ATTR(irq_enable, S_IRUGO | S_IWUSR, sp_irq_enable_show,
	       sp_irq_enable_store),
#endif
	__ATTR(anx7730, S_IRUGO | S_IWUSR, NULL, anx7730_write_reg_store),
	__ATTR(anx7816, S_IRUGO | S_IWUSR, NULL, anx7816_write_reg_store),
	__ATTR(DPCD, S_IRUGO | S_IWUSR, NULL, anx_dpcd_store),
#ifdef SP_REGISTER_SET_TEST	/*//slimport test*/
	__ATTR(ctrl_reg0, S_IRUGO | S_IWUSR, ctrl_reg0_show, ctrl_reg0_store),
	__ATTR(ctrl_reg10, S_IRUGO | S_IWUSR, ctrl_reg10_show,
	       ctrl_reg10_store),
	__ATTR(ctrl_reg11, S_IRUGO | S_IWUSR, ctrl_reg11_show,
	       ctrl_reg11_store),
	__ATTR(ctrl_reg2, S_IRUGO | S_IWUSR, ctrl_reg2_show, ctrl_reg2_store),
	__ATTR(ctrl_reg12, S_IRUGO | S_IWUSR, ctrl_reg12_show,
	       ctrl_reg12_store),
	__ATTR(ctrl_reg1, S_IRUGO | S_IWUSR, ctrl_reg1_show, ctrl_reg1_store),
	__ATTR(ctrl_reg6, S_IRUGO | S_IWUSR, ctrl_reg6_show, ctrl_reg6_store),
	__ATTR(ctrl_reg16, S_IRUGO | S_IWUSR, ctrl_reg16_show,
	       ctrl_reg16_store),
	__ATTR(ctrl_reg5, S_IRUGO | S_IWUSR, ctrl_reg5_show, ctrl_reg5_store),
	__ATTR(ctrl_reg8, S_IRUGO | S_IWUSR, ctrl_reg8_show, ctrl_reg8_store),
	__ATTR(ctrl_reg15, S_IRUGO | S_IWUSR, ctrl_reg15_show,
	       ctrl_reg15_store),
	__ATTR(ctrl_reg18, S_IRUGO | S_IWUSR, ctrl_reg18_show,
	       ctrl_reg18_store),
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
	for (; i >= 0; i--)
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

	mutex_lock(&pdata->sp_tx_power_lock);

	if (pdata->sp_tx_power_state) {
		pr_warn("%s %s: already powered on!\n", LOG_TAG, __func__);
		goto exit;
	}

	rc = anx7816_pinctrl_rst_set_state(true);
	if (rc < 0) {
		pr_err("%s %s: fail to set_state for reset. ret=%d\n",
						LOG_TAG, __func__, rc);
		goto exit;
	}

	if (gpio_is_valid(pdata->gpio_reset))
		gpio_set_value(pdata->gpio_reset, 0);
	usleep_range(1000, 1000);

	if (gpio_is_valid(pdata->gpio_p_dwn))
		gpio_set_value(pdata->gpio_p_dwn, 1);
	usleep_range(2000, 2000);

	rc = regulator_enable(pdata->dvdd_10);
	if (rc < 0) {
		pr_err("%s %s: failed to enable dvdd_10 regulator rc=%d\n",
						LOG_TAG, __func__, rc);
		goto err;
	}
	usleep_range(4500, 5000);

	if (gpio_is_valid(pdata->gpio_p_dwn))
		gpio_set_value(pdata->gpio_p_dwn, 0);
	usleep_range(2000, 2000);

	rc = anx7816_clk_start(pdata);
	if (rc < 0) {
		pr_err("%s %s: fail to start the dp_brdge_mclk. ret=%d\n",
						LOG_TAG, __func__, rc);
		goto err_vdd_10;
	}
	usleep_range(9000, 10000);

/* pdata->check_slimport_connection = true; TODO.. where does this go? */

	if (gpio_is_valid(pdata->gpio_reset))
		gpio_set_value(pdata->gpio_reset, 1);

	usleep_range(9000, 10000);

	pdata->sp_tx_power_state = 1;

	mutex_unlock(&pdata->sp_tx_power_lock);

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

exit:
	mutex_unlock(&pdata->sp_tx_power_lock);
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

	mutex_lock(&pdata->sp_tx_power_lock);

	if (!pdata->sp_tx_power_state) {
		pr_warn("%s %s: already powered off!\n", LOG_TAG, __func__);
		goto exit;
	}

	if (gpio_is_valid(pdata->gpio_reset))
		gpio_set_value(pdata->gpio_reset, 0);

	usleep_range(1000, 1000);

	anx7816_clk_stop(pdata);
	usleep_range(2000, 2000);

	/* pdata->check_slimport_connection = false;
	TODO where does this go? */
	if (gpio_is_valid(pdata->gpio_p_dwn))
		gpio_set_value(pdata->gpio_p_dwn, 1);

	regulator_disable(pdata->dvdd_10);
	rc = anx7816_pinctrl_rst_set_state(false);
	if (rc < 0)
		pr_err("%s %s: fail to set_state for reset. ret=%d\n",
						LOG_TAG, __func__, rc);
	usleep_range(1000, 1000);

	pdata->sp_tx_power_state = 0;

exit:
	mutex_unlock(&pdata->sp_tx_power_lock);
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

static void anx7816_free_gpio(struct anx7816_data *anx7816)
{
/*      gpio_free(anx7816->pdata->gpio_int); */
	if (gpio_is_valid(anx7816->pdata->gpio_reset))
		gpio_free(anx7816->pdata->gpio_reset);
	if (gpio_is_valid(anx7816->pdata->gpio_p_dwn))
		gpio_free(anx7816->pdata->gpio_p_dwn);
	if (gpio_is_valid(anx7816->pdata->gpio_cbl_det))
		gpio_free(anx7816->pdata->gpio_cbl_det);
}

static int anx7816_init_gpio(struct anx7816_data *anx7816)
{
	int ret = 0;

	pr_debug("%s %s: anx7816 init gpio\n", LOG_TAG, __func__);
	/*  gpio for chip power down  */
	if (gpio_is_valid(anx7816->pdata->gpio_p_dwn)) {
		ret = gpio_request(anx7816->pdata->gpio_p_dwn,
						"anx7816_p_dwn_ctl");
		if (ret) {
			pr_err("%s : failed to request gpio %d\n", __func__,
						anx7816->pdata->gpio_p_dwn);
			goto out;
		}
		gpio_direction_output(anx7816->pdata->gpio_p_dwn, 1);
	} else
		pr_err("%s : Invalid gpio_p_dwn\n", __func__);

	if (gpio_is_valid(anx7816->pdata->gpio_reset)) {
		/*  gpio for chip reset  */
		ret = gpio_request(anx7816->pdata->gpio_reset,
						"anx7816_reset_n");
		if (ret) {
			pr_err("%s : failed to request gpio %d\n", __func__,
					       anx7816->pdata->gpio_reset);
			goto err0;
		}
		gpio_direction_output(anx7816->pdata->gpio_reset, 0);
	} else
		pr_err("%s : Invalid gpio_reset\n", __func__);

	/*  gpio for slimport cable detect  */
	if (gpio_is_valid(anx7816->pdata->gpio_cbl_det)) {
		ret = gpio_request(anx7816->pdata->gpio_cbl_det,
						"anx7816_cbl_det");
		if (ret) {
			pr_err("%s : failed to request gpio %d\n", __func__,
			       anx7816->pdata->gpio_cbl_det);
			goto err1;
		}
		gpio_direction_input(anx7816->pdata->gpio_cbl_det);
		gpio_export(anx7816->pdata->gpio_cbl_det, false);
	} else
		pr_err("%s : Invalid  gpio_cbl_det\n", __func__);

	goto out;

err1:
	if (gpio_is_valid(anx7816->pdata->gpio_reset))
		gpio_free(anx7816->pdata->gpio_reset);
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
		sp_tx_hardware_powerdown();
		pr_err("%s : failed to detect anx7816\n", __func__);
		return -ENODEV;
	}

	slimport_chip_initial();
	return 0;
}

void cable_disconnect(void *data)
{
	struct anx7816_data *anx7816 = data;

	cancel_delayed_work_sync(&anx7816->work);
	flush_workqueue(anx7816->workqueue);
#ifdef QUICK_CHARGE_SUPPORT
	disable_otg();
	pmic_recovery();
	reset_process();
#endif
#ifdef CONFIG_SLIMPORT_DYNAMIC_HPD
	slimport_set_hdmi_hpd(0);
#endif
	sp_tx_hardware_powerdown();
	sp_tx_clean_state_machine();
	wake_unlock(&anx7816->slimport_lock);
	wake_lock_timeout(&anx7816->slimport_lock, 2 * HZ);

}
void system_power_ctrl(unchar enable);

/*JIRA: CLD-110,
Software patch for cable det pin has glitch before stable "High"*/
/*#define CABLE_DET_PIN_HAS_GLITCH */
#define DEBOUNCING_DELAY_US 50000
static unsigned char confirmed_cable_det(void *data)
{
	struct anx7816_data *anx7816 = data;
	#ifdef CABLE_DET_PIN_HAS_GLITCH
	unsigned char count = 10;
	unsigned char cable_det_count = 0;

	do {
		if (gpio_get_value(anx7816->pdata->gpio_cbl_det)
				== DONGLE_CABLE_INSERT)
			cable_det_count++;
		usleep_range(5000, 5000);
	} while (count--);

	return (cable_det_count > 5) ? 0 : 1;
	#else
	usleep_range(DEBOUNCING_DELAY_US, DEBOUNCING_DELAY_US);
	return gpio_get_value(anx7816->pdata->gpio_cbl_det);
	#endif
}

static irqreturn_t anx7816_cbl_det_isr(int irq, void *data)
{
	struct anx7816_data *anx7816 = data;
	int cable_connected = 0;

	cable_connected = confirmed_cable_det(data);
	pr_debug("%s %s : detect cable insertion, cable_connected = %d\n",
					LOG_TAG, __func__, cable_connected);

	if (cable_connected == DONGLE_CABLE_INSERT) {
		if (!atomic_add_unless(&anx7816->slimport_connected, 1, 1))
			goto out;

		wake_lock(&anx7816->slimport_lock);
#ifdef QUICK_CHARGE_SUPPORT
		reset_process();
#endif
		pr_info("%s %s : detect cable insertion\n", LOG_TAG, __func__);
		complete(&anx7816->connect_wait);
		queue_delayed_work(anx7816->workqueue, &anx7816->work, 0);
	} else {
		if (!atomic_add_unless(&anx7816->slimport_connected, -1, 0))
			goto out;

		pr_info("%s %s : detect cable removal\n", LOG_TAG, __func__);
		complete(&anx7816->connect_wait);
		cable_disconnect(anx7816);
		/*msleep(1000);*/
	}

out:
	return IRQ_HANDLED;
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
#ifdef QUICK_CHARGE_SUPPORT
	if (sp_tx_cur_states() >= STATE_SINK_CONNECTION)
		quick_charge_main_process();
#endif
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

int anx7816_regulator_configure(struct device *dev,
				struct anx7816_platform_data *pdata)
{
	int rc = 0;

	pr_debug("%s+\n", __func__);

	pdata->avdd_33 = regulator_get(dev, "analogix,vdd_33");
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

static int anx7816_parse_dt(struct device *dev,
			    struct anx7816_platform_data *pdata)
{
	int rc = 0;
	struct device_node *np = dev->of_node;
	struct platform_device *sp_pdev = NULL;
	struct device_node *sp_tx_node = NULL;

	pdata->gpio_p_dwn =
	    of_get_named_gpio_flags(np, "analogix,p-dwn-gpio", 0, NULL);
	if (!gpio_is_valid(pdata->gpio_p_dwn))
		pr_err("%s: Invalid p-dwn-gpio = %d\n",
					__func__, pdata->gpio_p_dwn);

	pdata->gpio_reset =
	    of_get_named_gpio_flags(np, "analogix,reset-gpio", 0, NULL);
	if (!gpio_is_valid(pdata->gpio_reset))
		pr_err("%s: Invalid gpio_reset = %d\n",
					__func__, pdata->gpio_reset);

	pdata->gpio_cbl_det =
	    of_get_named_gpio_flags(np, "analogix,cbl-det-gpio", 0, NULL);

	pr_debug(
	       "%s gpio p_dwn : %d, reset : %d,  gpio_cbl_det %d\n",
	       LOG_TAG, pdata->gpio_p_dwn,
	       pdata->gpio_reset, pdata->gpio_cbl_det);

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

	return 0;
}
#else
static int anx7816_parse_dt(struct device *dev,
			    struct anx7816_platform_data *pdata)
{
	return -ENODEV;
}
#endif

int slimport_reset_standby(void)
{
	int ret = 0;
	struct anx7816_data *anx7816;

	if (!g_data) {
		pr_err("%s: g_data is not set\n", __func__);
		return -ENODEV;
	}

	anx7816 = g_data;

	if (atomic_read(&anx7816->slimport_connected)) {
		pr_err("%s: connected, not resetting!\n", __func__);
		ret = -EBUSY;
	} else if (gpio_get_value(anx7816->pdata->gpio_cbl_det) ==
			DONGLE_CABLE_INSERT) {
		pr_warn("%s: in cable detected state, resetting!\n", __func__);
		sp_tx_hardware_poweron();
		sp_tx_hardware_powerdown();

		usleep_range(250000, 250001);

		if (gpio_get_value(anx7816->pdata->gpio_cbl_det) ==
			DONGLE_CABLE_INSERT) {
			pr_err("%s: cable detected after reset!\n", __func__);
			ret = -EAGAIN;
		}
	} else {
		pr_debug("%s: already in standby, not resetting!\n", __func__);
	}

	return ret;
}

#ifdef MML_DYNAMIC_IRQ_SUPPORT
static int anx7816_enable_irq(int enable)
{
	struct anx7816_data *anx7816;
	int ret = 0;

	pr_debug("%s+ (enable: %d)\n", __func__, enable);

	if (!anx7816_client)
		return -ENODEV;

	if (!g_data) {
		pr_err("%s: g_data is not set \n", __func__);
		return -ENODEV;
	} else
		anx7816 = g_data;

	if (enable && !anx7816->pdata->cbl_det_irq_enabled) {
		ret = request_threaded_irq(anx7816_client->irq, NULL,
				anx7816_cbl_det_isr,
				IRQF_TRIGGER_RISING
				| IRQF_TRIGGER_FALLING
				| IRQF_ONESHOT, "anx7816", anx7816);
		if (ret < 0) {
			pr_err("%s : failed to request irq\n", __func__);
			goto exit;
		}

		anx7816->pdata->cbl_det_irq_enabled = true;
	} else if (!enable && anx7816->pdata->cbl_det_irq_enabled) {
		free_irq(anx7816_client->irq, anx7816);

		anx7816->pdata->cbl_det_irq_enabled = false;
	}

exit:
	return ret;
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
	int ret = 0;

	pr_debug("%s+\n", __func__);

	anx7816 = (struct anx7816_data *)data;

	if (atomic_read(&anx7816->slimport_connected)) {
		pr_err("%s: Slimport should not be connected!\n", __func__);
		ret = -EBUSY;
	}

	pr_debug("%s-\n", __func__);

	return ret;
}

static int slimport_mod_display_handle_connect(void *data)
{
	struct anx7816_data *anx7816;
	int retries = 2;
	int ret = 0;

	pr_debug("%s+\n", __func__);

	anx7816 = (struct anx7816_data *)data;

	reinit_completion(&anx7816->connect_wait);

#ifdef MML_DYNAMIC_IRQ_SUPPORT
	anx7816_enable_irq(1);
#endif

	mod_display_set_display_state(MOD_DISPLAY_ON);

	while (!wait_for_completion_timeout(&anx7816->connect_wait,
		msecs_to_jiffies(1000)) && retries) {
		pr_debug("%s: Slimport not connected... Retries left: %d\n",
			__func__, retries);
		retries--;

		/* Power cycle the chip here to work around cable detection
		   issues we've seen */
		sp_tx_hardware_poweron();
		sp_tx_hardware_powerdown();
	}

	if (!atomic_read(&anx7816->slimport_connected)) {
		pr_warn("%s: Slimport failed to connect...\n", __func__);
		ret = -ENODEV;
	}

	pr_debug("%s-\n", __func__);

	return ret;
}

static int slimport_mod_display_handle_disconnect(void *data)
{
	struct anx7816_data *anx7816;
	int retries = 2;

	pr_debug("%s+\n", __func__);

	anx7816 = (struct anx7816_data *)data;

	reinit_completion(&anx7816->connect_wait);

	mod_display_set_display_state(MOD_DISPLAY_OFF);

	while (atomic_read(&anx7816->slimport_connected) &&
	       !wait_for_completion_timeout(&anx7816->connect_wait,
	       msecs_to_jiffies(1000)) && retries) {
		pr_debug("%s: Slimport not disconnected... Retries left: %d\n",
			__func__, retries);
		retries--;
	}

#ifdef MML_DYNAMIC_IRQ_SUPPORT
	anx7816_enable_irq(0);
#endif

	/* This should never happen, but just in case... */
	if (atomic_add_unless(&anx7816->slimport_connected, -1, 0)) {
		pr_err("%s %s : Slimport failed to disconnect... Force cable removal\n",
			LOG_TAG, __func__);
		cable_disconnect(anx7816);
	}

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

static int anx7816_i2c_probe(struct i2c_client *client,
			     const struct i2c_device_id *id)
{

	struct anx7816_data *anx7816;
	struct anx7816_platform_data *pdata;
	int ret = 0;

	pr_debug("%s %s start\n", LOG_TAG, __func__);

#ifdef SP_REGISTER_SET_TEST
	val_SP_TX_LT_CTRL_REG0 = 0x19;
	val_SP_TX_LT_CTRL_REG10 = 0x00;
	val_SP_TX_LT_CTRL_REG11 = 0x00;
	val_SP_TX_LT_CTRL_REG2 = 0x36;
	val_SP_TX_LT_CTRL_REG12 = 0x00;
	val_SP_TX_LT_CTRL_REG1 = 0x26;
	val_SP_TX_LT_CTRL_REG6 = 0x3c;
	val_SP_TX_LT_CTRL_REG16 = 0x18;
	val_SP_TX_LT_CTRL_REG5 = 0x28;
	val_SP_TX_LT_CTRL_REG8 = 0x2F;
	val_SP_TX_LT_CTRL_REG15 = 0x10;
	val_SP_TX_LT_CTRL_REG18 = 0x1F;
#endif
	if (!i2c_check_functionality(client->adapter,
	I2C_FUNC_SMBUS_I2C_BLOCK)) {
		pr_err("%s: i2c bus does not support the anx7816\n", __func__);
		ret = -ENODEV;
		goto exit;
	}

	anx7816 = kzalloc(sizeof(struct anx7816_data), GFP_KERNEL);
	if (!anx7816) {
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
		if (ret != 0)	/* if occurs error */
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

	anx7816->workqueue = create_singlethread_workqueue("anx7816_work");
	if (anx7816->workqueue == NULL) {
		pr_err("%s: failed to create work queue\n", __func__);
		ret = -ENOMEM;
		goto err1;
	}
	anx7816->pdata->avdd_power(1);
/*	anx7816->pdata->dvdd_power(1);*/

	anx7816->pdata->sp_tx_power_state = 0;
	mutex_init(&anx7816->pdata->sp_tx_power_lock);
	atomic_set(&anx7816->slimport_connected, 0);

	ret = anx7816_system_init();
	if (ret) {
		pr_err("%s: failed to initialize anx7816\n", __func__);
		goto err2;
	}

	anx7816->pdata->cbl_det_irq_enabled = false;
	client->irq = gpio_to_irq(anx7816->pdata->gpio_cbl_det);
	if (client->irq < 0) {
		pr_err("%s : failed to get gpio irq\n", __func__);
		goto err2;
	}

	wake_lock_init(&anx7816->slimport_lock,
		       WAKE_LOCK_SUSPEND, "slimport_wake_lock");

#ifndef MML_DYNAMIC_IRQ_SUPPORT
	ret = request_threaded_irq(client->irq, NULL, anx7816_cbl_det_isr,
				   IRQF_TRIGGER_RISING
				   | IRQF_TRIGGER_FALLING
				   | IRQF_ONESHOT, "anx7816", anx7816);
	if (ret < 0) {
		pr_err("%s : failed to request irq\n", __func__);
		goto err2;
	}

	ret = irq_set_irq_wake(client->irq, 1);
	if (ret < 0) {
		pr_err("%s : Request irq for cable detect", __func__);
		pr_err("interrupt wake set fail\n");
		goto err3;
	}

	ret = enable_irq_wake(client->irq);
	if (ret < 0) {
		pr_err("%s : Enable irq for cable detect", __func__);
		pr_err("interrupt wake enable fail\n");
		goto err3;
	}
#endif

	ret = create_sysfs_interfaces(&client->dev);
	if (ret < 0) {
		pr_err("%s : sysfs register failed", __func__);
		goto err3;
	}

	/*QC2.0*/
#ifdef QUICK_CHARGE_SUPPORT
	BLOCKING_INIT_NOTIFIER_HEAD(get_notifier_list_head());
#endif

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
	init_completion(&anx7816->connect_wait);

	pr_debug("%s %s end\n", LOG_TAG, __func__);
	goto exit;

err3:
#ifndef MML_DYNAMIC_IRQ_SUPPORT
	free_irq(client->irq, anx7816);
#endif
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
	pr_debug("anx7816_i2c_remove\n");
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
bool is_slimport_dp(void)
{
	return (sp_tx_cur_cable_type() == DWN_STRM_IS_DIGITAL) ? TRUE : FALSE;
}

#define MHZ_TO_KHZ(freq) ((freq) * 1000)

u32 sp_get_link_bandwidth_khz(unchar link_bandwidth)
{
	u32 link_bandwidth_khz = 0;

	switch (link_bandwidth) {
	case LINK_1P62G:
		link_bandwidth_khz = MHZ_TO_KHZ(1620);
		break;
	case LINK_2P7G:
		link_bandwidth_khz = MHZ_TO_KHZ(2700);
		break;
	case LINK_5P4G:
		link_bandwidth_khz = MHZ_TO_KHZ(5400);
		break;
	case LINK_6P75G:
		link_bandwidth_khz = MHZ_TO_KHZ(6750);
		break;
	}
	return link_bandwidth_khz;
}

unchar sp_get_link_bandwidth_limit_from_khz(u32 link_bandwidth_limit_khz)
{
	if (link_bandwidth_limit_khz >= MHZ_TO_KHZ(6750))
		return LINK_6P75G;
	else if (link_bandwidth_limit_khz >= MHZ_TO_KHZ(5400))
		return LINK_5P4G;
	else if (link_bandwidth_limit_khz >= MHZ_TO_KHZ(2700))
		return LINK_2P7G;
	else if (link_bandwidth_limit_khz >= MHZ_TO_KHZ(1620))
		return LINK_1P62G;
	else
		return 0;
}

u32 sp_get_rx_bw_khz(void)
{
	return sp_get_link_bandwidth_khz(sp_rx_cur_bw());
}

static int anx7816_i2c_suspend(struct i2c_client *client, pm_message_t state)
{
	return 0;
}

static int anx7816_i2c_resume(struct i2c_client *client)
{
	return 0;
}

static const struct i2c_device_id anx7816_id[] = {
	{"anx7816", 0},
	{}
};

MODULE_DEVICE_TABLE(i2c, anx7816_id);

#ifdef CONFIG_OF
static struct of_device_id anx_match_table[] = {
	{.compatible = "analogix,anx7816",},
	{},
};
#endif

static struct i2c_driver anx7816_driver = {
	.driver = {
		   .name = "anx7816",
		   .owner = THIS_MODULE,
#ifdef CONFIG_OF
		   .of_match_table = anx_match_table,
#endif
		   },
	.probe = anx7816_i2c_probe,
	.remove = anx7816_i2c_remove,
	.suspend = anx7816_i2c_suspend,
	.resume = anx7816_i2c_resume,
	.id_table = anx7816_id,
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
MODULE_AUTHOR("Junhua Xia <jxia@analogixsemi.com>");
MODULE_LICENSE("GPL");
MODULE_VERSION("1.5");
