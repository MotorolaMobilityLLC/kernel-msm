/*
 * max77836-muic.c - MUIC driver for the Maxim 77836
 *
 *  Copyright (C) 2010 Samsung Electronics
 *  <ms925.kim@samsung.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#include <linux/kernel.h>
#include <linux/module.h>
#include <linux/i2c.h>
#include <linux/gpio.h>
#include <linux/slab.h>
#include <linux/interrupt.h>
#include <linux/err.h>
#include <linux/platform_device.h>
#include <linux/workqueue.h>
#include <linux/mfd/max77836.h>
#include <linux/mfd/max77836-private.h>

/* for Regmap */
#include <linux/regmap.h>

#include <linux/max77836-muic.h>
#include <linux/host_notify.h>

#include <linux/irq.h>

extern struct sec_switch_data sec_switch_data;
extern struct device *switch_device;

static const struct max77836_muic_vps_data muic_vps_table[] = {
#if defined(CONFIG_MUIC_SUPPORT_OTG_DONGLE)
	{
		.adcerr		= 0x00,
		.adclow		= (0x0 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_GND,
		.vbvolt		= 0x00,
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_NOTHING_ATTACH,
		.control1	= CTRL1_USB,
		.vps_name	= "OTG",
		.attached_dev	= ATTACHED_DEV_OTG_MUIC,
	},
#endif /* CONFIG_MUIC_SUPPORT_OTG_DONGLE */

#if defined(CONFIG_MUIC_SUPPORT_FACTORY_BUTTON)
	{
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_REMOTE_S11,
		.vbvolt		= 0x00,
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_NOTHING_ATTACH,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Factory Button",
		.attached_dev	= ATTACHED_DEV_FACTORY_BUTTON,
	},
#endif /* CONFIG_MUIC_SUPPORT_FACTORY_BUTTON */

#if defined(CONFIG_MUIC_SUPPORT_POGO_TA)
	{
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_SMARTDOCK,
		.vbvolt		= (0x1 << STATUS2_VBVOLT_SHIFT),
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "POGO + TA",
		.attached_dev	= ATTACHED_DEV_POGO_TA_MUIC,
	},
#endif /* CONFIG_MUIC_SUPPORT_POGO_TA */

#if defined(CONFIG_MUIC_SUPPORT_POGO_USB)
	{
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_SMARTDOCK,
		.vbvolt		= (0x1 << STATUS2_VBVOLT_SHIFT),
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_USB,
		.control1	= CTRL1_USB,
		.vps_name	= "POGO + USB",
		.attached_dev	= ATTACHED_DEV_POGO_USB_MUIC,
	},
#endif /* CONFIG_MUIC_SUPPORT_POGO_USB */

	{
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_JIG_USB_OFF, /* 255K */
		.vbvolt		= (0x1 << STATUS2_VBVOLT_SHIFT),
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_NOTHING_ATTACH,
		.control1	= CTRL1_USB,
		.vps_name	= "Jig USB Off",
		.attached_dev	= ATTACHED_DEV_JIG_USB_OFF_MUIC,
	},
	{
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_JIG_USB_ON, /* 301K */
		.vbvolt		= (0x1 << STATUS2_VBVOLT_SHIFT),
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_NOTHING_ATTACH,
		.control1	= CTRL1_USB,
		.vps_name	= "Jig USB On",
		.attached_dev	= ATTACHED_DEV_JIG_USB_ON_MUIC,
	},
	{
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_JIG_UART_OFF, /* 523K */
		.vbvolt		= 0x00,
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_NOTHING_ATTACH,
		.control1	= CTRL1_UART,
		.vps_name	= "Jig UART Off",
		.attached_dev	= ATTACHED_DEV_JIG_UART_OFF_MUIC,
	},
	{
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_JIG_UART_OFF, /* 523K + VB */
		.vbvolt		= (0x1 << STATUS2_VBVOLT_SHIFT),
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_NOTHING_ATTACH,
		.control1	= CTRL1_UART,
		.vps_name	= "Jig UART Off + VB",
		.attached_dev	= ATTACHED_DEV_JIG_UART_OFF_VB_MUIC,
	},
	{
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_JIG_UART_ON, /* 619K */
		.vbvolt		= 0x00,
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_NOTHING_ATTACH,
		.control1	= CTRL1_UART,
		.vps_name	= "Jig UART On",
		.attached_dev	= ATTACHED_DEV_JIG_UART_ON_MUIC,
	},
	{
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_OPEN,
		.vbvolt		= (0x1 << STATUS2_VBVOLT_SHIFT),
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_DEDICATED_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "TA",
		.attached_dev	= ATTACHED_DEV_TA_MUIC,
	},
	{
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_OPEN,
		.vbvolt		= (0x1 << STATUS2_VBVOLT_SHIFT),
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_USB,
		.control1	= CTRL1_USB,
		.vps_name	= "USB",
		.attached_dev	= ATTACHED_DEV_USB_MUIC,
	},
	{
		/* USB cable with specific id (219.3KOhm) */
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_CEA936ATYPE1_CHG, /* 200K */
		.vbvolt		= (0x1 << STATUS2_VBVOLT_SHIFT),
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_USB,
		.control1	= CTRL1_USB,
		.vps_name	= "USB (Specific)",
		.attached_dev	= ATTACHED_DEV_USB_MUIC,
	},
	{
		.adcerr		= 0x00,
		.adclow		= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_OPEN,
		.vbvolt		= (0x1 << STATUS2_VBVOLT_SHIFT),
		.chgdetrun	= 0x00,
		.chgtyp		= CHGTYP_CDP,
		.control1	= CTRL1_USB,
		.vps_name	= "CDP",
		.attached_dev	= ATTACHED_DEV_CDP_MUIC,
	},
	{
		.adcerr 	= 0x00,
		.adclow 	= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_OPEN,
		.vbvolt 	= (0x1 << STATUS2_VBVOLT_SHIFT),
		.chgdetrun	= 0x00,
		.chgtyp 	= CHGTYP_500MA_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Special 500mA TA",
		.attached_dev	= ATTACHED_DEV_TA_MUIC,
	},
	{
		.adcerr 	= 0x00,
		.adclow 	= (0x1 << STATUS1_ADCLOW_SHIFT),
		.adc		= ADC_OPEN,
		.vbvolt 	= (0x1 << STATUS2_VBVOLT_SHIFT),
		.chgdetrun	= 0x00,
		.chgtyp 	= CHGTYP_1A_CHARGER,
		.control1	= CTRL1_OPEN,
		.vps_name	= "Special 1A TA",
		.attached_dev	= ATTACHED_DEV_TA_MUIC,
	},
};

static int muic_lookup_vps_table(enum muic_attached_dev new_dev)
{
	const struct max77836_muic_vps_data *tmp_vps;
	int i;

	for (i = 0; i < ARRAY_SIZE(muic_vps_table); i++) {
		tmp_vps = &(muic_vps_table[i]);

		if (tmp_vps->attached_dev != new_dev)
			continue;

		pr_debug("%s:%s (%d) vps table match found at i(%d), %s\n",
				MUIC_DEV_NAME, __func__, new_dev, i,
				tmp_vps->vps_name);

		return i;
	}

	pr_debug("%s:%s can't find (%d) on vps table\n", MUIC_DEV_NAME,
			__func__, new_dev);

	return -1;
}

static int write_muic_ctrl_reg(struct max77836_muic_data *muic_data,
				const u8 reg, const u8 val)
{
	struct i2c_client *i2c = muic_data->i2c;
	int ret = 0;
	u8 reg_val = 0;

	ret = max77836_read_reg(i2c, reg, &reg_val);
	if (ret < 0)
		pr_err("%s:%s err read REG(0x%02x) [%d]\n", MUIC_DEV_NAME,
				__func__, reg, ret);

	if (reg_val ^ val) {
		pr_info("%s:%s overwrite REG(0x%02x) from [0x%x] to [0x%x]\n",
				MUIC_DEV_NAME, __func__, reg, reg_val, val);

		ret = max77836_write_reg(i2c, reg, val);
		if (ret < 0)
			pr_err("%s:%s err write REG(0x%02x)\n", MUIC_DEV_NAME,
					__func__, reg);
	} else {
		pr_debug("%s:%s REG(0x%02x) already [0x%x], just return\n",
				MUIC_DEV_NAME, __func__, reg, reg_val);
	}

	ret = max77836_read_reg(i2c, reg, &reg_val);
	if (ret < 0)
		pr_err("%s:%s err read REG(0x%02x) [%d]\n", MUIC_DEV_NAME,
				__func__, reg, ret);
	else
		pr_debug("%s:%s REG(0x%02x) after change [0x%x]\n", MUIC_DEV_NAME,
				__func__, reg, reg_val);

	return ret;
}

static int write_max77836_ctrl2_reg(struct max77836_muic_data *muic_data,
					const u8 mask, const u8 val)
{
	struct max77836_platform_data *mfd_pdata = muic_data->mfd_pdata;
	struct i2c_client *i2c = muic_data->i2c;
	u8 old_val, new_val;
	int ret = 0;

	if (!muic_data->i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		return -ENODEV;
	}

	ret = mfd_pdata->get_control2_reg(i2c, &old_val);
	if (ret) {
		pr_err("%s:%s fail to read reg(%d)\n", MUIC_DEV_NAME, __func__,
				ret);
		return ret;
	}

	old_val = old_val & 0xff;

	new_val = (val & mask) | (old_val & (~mask));
	ret = mfd_pdata->set_control2_reg(muic_data->i2c, new_val);
	if (ret)
		pr_err("%s: fail to update reg(%d)\n", __func__, ret);

	pr_debug("%s:%s CTRL2[before:0x%02x, set:0x%02x, after:0x%02x]\n",
			MUIC_DEV_NAME, __func__, old_val, val, new_val);

	return ret;
}

static int write_vps_regs(struct max77836_muic_data *muic_data,
			enum muic_attached_dev new_dev)
{
	const struct max77836_muic_vps_data *tmp_vps;
	int vps_index;
	int ret = 0;

	vps_index = muic_lookup_vps_table(new_dev);
	if (vps_index == -1)
		return -ENODEV;

	tmp_vps = &(muic_vps_table[vps_index]);

	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77836_MUIC_REG_CONTROL1,
			tmp_vps->control1);

	return ret;
}

static int com_to_open(struct max77836_muic_data *muic_data)
{
	max77836_reg_ctrl1_t reg_val;
	int ret = 0;

	reg_val = CTRL1_OPEN;
	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77836_MUIC_REG_CONTROL1,
			reg_val);
	if (ret)
		pr_err("%s:%s write_muic_ctrl_reg err\n", MUIC_DEV_NAME,
				__func__);

	return ret;
}

static int com_to_uart(struct max77836_muic_data *muic_data)
{

	max77836_reg_ctrl1_t reg_val;
	int ret = 0;

	reg_val = CTRL1_UART;
	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77836_MUIC_REG_CONTROL1,
			reg_val);
	if (ret)
		pr_err("%s:%s write_muic_ctrl_reg err\n", MUIC_DEV_NAME, __func__);
	else
		pr_info("%s:%s: muic path set to UART\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_usb(struct max77836_muic_data *muic_data)
{

	max77836_reg_ctrl1_t reg_val;
	int ret = 0;

	reg_val = CTRL1_USB;
	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77836_MUIC_REG_CONTROL1,
			reg_val);
	if (ret)
		pr_err("%s:%s write_muic_ctrl_reg err\n", MUIC_DEV_NAME, __func__);
	else
		pr_info("%s:%s: muic path set to USB\n", MUIC_DEV_NAME, __func__);

	return ret;
}

static int com_to_audio(struct max77836_muic_data *muic_data)
{

	max77836_reg_ctrl1_t reg_val;
	int ret = 0;

	reg_val = CTRL1_AUDIO;
	/* write control1 register */
	ret = write_muic_ctrl_reg(muic_data, MAX77836_MUIC_REG_CONTROL1,
			reg_val);
	if (ret)
		pr_err("%s:%s write_muic_ctrl_reg err\n", MUIC_DEV_NAME,
				__func__);

	return ret;
}




static u8 max77836_muic_get_adc_value(struct max77836_muic_data *muic_data)
{
	u8 status;
	u8 adc = ADC_ERROR;
	int ret = 0;

	ret = max77836_read_reg(muic_data->i2c, MAX77836_MUIC_REG_STATUS1,
			&status);
	if (ret)
		pr_err("%s:%s fail to read muic reg(%d)\n", MUIC_DEV_NAME,
				__func__, ret);
	else
		adc = status & STATUS1_ADC_MASK;

	return adc;
}

static ssize_t max77836_muic_show_dump_reg(struct device *dev,
						   struct device_attribute *attr, char *buf)
{
	struct max77836_muic_data *muic_data = dev_get_drvdata(dev);
	u8 stat[3] = {0};
	u8 ctrl[3] = {0};
	u8 cdet[2] = {0};

	max77836_bulk_read(muic_data->i2c, MAX77836_MUIC_REG_STATUS1, 3, stat);
	max77836_bulk_read(muic_data->i2c, MAX77836_MUIC_REG_CONTROL1, 3, ctrl);
	max77836_bulk_read(muic_data->i2c, MAX77836_REG_CDETCTRL1, 2, cdet);

	return sprintf(buf,
		"reg :   1  /   2  /   3  \n"
		"stat: 0x%02x / 0x%02x / 0x%02x\n"
		"ctrl: 0x%02x / 0x%02x / 0x%02x\n"
		"cdet: 0x%02x / 0x%02x /\n",
		stat[0], stat[1], stat[2], ctrl[0], ctrl[1], ctrl[2], cdet[0], cdet[1]);
}

static ssize_t max77836_muic_show_manual_sw(struct device *dev,
						   struct device_attribute *attr, char *buf)
{
	struct max77836_muic_data *muic_data = dev_get_drvdata(dev);
	u8 val = 0;
	ssize_t cnt = 0;

	max77836_read_reg(muic_data->i2c, MAX77836_MUIC_REG_CONTROL1, &val);

	switch (val) {
		case 0x9:
			cnt = sprintf(buf, "USB");
			break;
		case 0x12:
			cnt = sprintf(buf, "AUDIO");
			break;
		case 0x1b:
			cnt = sprintf(buf, "UART");
			break;
		default:
			cnt = sprintf(buf, "OPEN");
			break;
	}

	return cnt;
}

static ssize_t max77836_muic_store_manual_sw(struct device *dev,
					  struct device_attribute *attr,
					  const char *buf, size_t count)
{
	struct max77836_muic_data *muic_data = dev_get_drvdata(dev);

	if (!strncasecmp(buf, "OPEN", 4))		com_to_open(muic_data);
	else if (!strncasecmp(buf, "USB", 3))	com_to_usb(muic_data);
	else if (!strncasecmp(buf, "AUDIO", 5))	com_to_audio(muic_data);
	else if (!strncasecmp(buf, "UART", 4))	com_to_uart(muic_data);
	else
		pr_warn("%s:%s invalid value\n", MUIC_DEV_NAME, __func__);

	return count;
}

static ssize_t max77836_muic_show_adc(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77836_muic_data *muic_data = dev_get_drvdata(dev);
	u8 adc;

	adc = max77836_muic_get_adc_value(muic_data);
	pr_debug("%s:%s adc(0x%02x)\n", MUIC_DEV_NAME, __func__, adc);

	if (adc == ADC_ERROR) {
		pr_err("%s:%s fail to read adc value\n", MUIC_DEV_NAME,
				__func__);
		return sprintf(buf, "UNKNOWN\n");
	}

	return sprintf(buf, "%x\n", adc);
}

static ssize_t max77836_muic_show_usb_state(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77836_muic_data *muic_data = dev_get_drvdata(dev);

	pr_debug("%s:%s attached_dev(%d)\n", MUIC_DEV_NAME, __func__,
			muic_data->attached_dev);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_POGO_USB)
	case ATTACHED_DEV_POGO_USB_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_USB */
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		return sprintf(buf, "USB_STATE_CONFIGURED\n");
	default:
		break;
	}

	return sprintf(buf, "USB_STATE_NOTCONFIGURED\n");
}

static ssize_t max77836_muic_show_attached_dev(struct device *dev,
				   struct device_attribute *attr,
				   char *buf)
{
	struct max77836_muic_data *muic_data = dev_get_drvdata(dev);
	const struct max77836_muic_vps_data *tmp_vps;
	int vps_index;

	vps_index = muic_lookup_vps_table(muic_data->attached_dev);
	if (vps_index == -1)
		return sprintf(buf, "No Device\n");

	tmp_vps = &(muic_vps_table[vps_index]);

	return sprintf(buf, "%s\n", tmp_vps->vps_name);
}

static void max77836_muic_set_adcdbset(struct max77836_muic_data *muic_data,
					int value)
{
	int ret;
	u8 val;
	u8 cntl3_before, cntl3_after;

	if (value > 3) {
		pr_err("%s:%s invalid value(%d)\n", MUIC_DEV_NAME, __func__,
				value);
		return;
	}

	if (!muic_data->i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		return;
	}

	val = (value << CTRL3_ADCDBSET_SHIFT);
	max77836_read_reg(muic_data->i2c, MAX77836_MUIC_REG_CONTROL3,
			&cntl3_before);

	ret = max77836_write_reg(muic_data->i2c, MAX77836_MUIC_REG_CONTROL3,
			val);
	if (ret < 0)
		pr_err("%s: fail to write reg\n", __func__);

	max77836_read_reg(muic_data->i2c, MAX77836_MUIC_REG_CONTROL3,
			&cntl3_after);

	pr_debug("%s:%s CNTL3: before(0x%02x), value(0x%02x), after(0x%02x)\n",
			MUIC_DEV_NAME, __func__, cntl3_before, val, cntl3_after);
}

static void max77836_muic_set_dchktm(struct max77836_muic_data *muic_data,
				int value)
{
	struct max77836_platform_data *mfd_pdata = muic_data->mfd_pdata;
	int ret;
	u8 old_val, val, new_val;
	const u8 mask = DCHKTM_MASK;

	if (value > 1) {
		pr_err("%s:%s invalid value(%d)\n", MUIC_DEV_NAME, __func__,
				value);
		return;
	}

	if (!muic_data->i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		return;
	}

	ret = mfd_pdata->get_cdetctrl1_reg(muic_data->i2c, &old_val);
	if (ret) {
		pr_err("%s:%s fail to read reg\n", MUIC_DEV_NAME, __func__);
		return;
	}

	old_val = old_val & 0xff;

	val = (value << DCHKTM_SHIFT);
	pr_info("%s:%s DCHKTM(0x%02x)\n", MUIC_DEV_NAME, __func__, val);

	new_val = (val & mask) | (old_val & (~mask));
	ret = mfd_pdata->set_cdetctrl1_reg(muic_data->i2c, new_val);
	if (ret < 0)
		pr_err("%s: fail to update reg\n", __func__);
}

static DEVICE_ATTR(dump_reg, S_IRUGO, max77836_muic_show_dump_reg, NULL);
static DEVICE_ATTR(manual_sw, 0664, max77836_muic_show_manual_sw,
		max77836_muic_store_manual_sw);
static DEVICE_ATTR(adc, S_IRUGO, max77836_muic_show_adc, NULL);
static DEVICE_ATTR(usb_state, S_IRUGO, max77836_muic_show_usb_state, NULL);
static DEVICE_ATTR(attached_dev, S_IRUGO, max77836_muic_show_attached_dev, NULL);

static struct attribute *max77836_muic_attributes[] = {
	&dev_attr_dump_reg.attr,
	&dev_attr_manual_sw.attr,
	&dev_attr_adc.attr,
	&dev_attr_usb_state.attr,
	&dev_attr_attached_dev.attr,
	NULL
};

static const struct attribute_group max77836_muic_group = {
	.attrs = max77836_muic_attributes,
};

#if defined(CONFIG_MUIC_SUPPORT_FACTORY_BUTTON)

static void max77836_muic_attach_callback_dock(struct max77836_muic_data *muic_data,
						int dock_type)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (switch_data->dock_cb)
		switch_data->dock_cb(dock_type);
	else
		pr_info("%s:%s dock_cb pointer is NULL, just return\n",
				MUIC_DEV_NAME, __func__);
}

static void max77836_muic_detach_callback_dock(struct max77836_muic_data *muic_data)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (switch_data->dock_cb)
		switch_data->dock_cb(MUIC_DOCK_DETACHED);
	else
		pr_info("%s:%s dock_cb pointer is NULL, just return\n",
				MUIC_DEV_NAME, __func__);
}
#endif

static void max77836_muic_attach_callback_usb(struct max77836_muic_data *muic_data)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;

	pr_debug("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (muic_data->is_usb_ready == false) {
		pr_info("%s:%s USB is not ready, just return\n", MUIC_DEV_NAME,
				__func__);
		return;
	}

	if (switch_data->usb_cb)
		switch_data->usb_cb(USB_CABLE_ATTACHED);
	else
		pr_info("%s:%s usb_cb pointer is NULL, just return\n",
				MUIC_DEV_NAME, __func__);
}

static void max77836_muic_detach_callback_usb(struct max77836_muic_data *muic_data)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;

	pr_debug("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (muic_data->is_usb_ready == false) {
		pr_info("%s:%s USB is not ready, just return\n", MUIC_DEV_NAME,
				__func__);
		return;
	}

	if (switch_data->usb_cb)
		switch_data->usb_cb(USB_CABLE_DETACHED);
	else
		pr_info("%s:%s usb_cb pointer is NULL, just return\n",
				MUIC_DEV_NAME, __func__);
}

static void max77836_muic_attach_callback_chg(struct max77836_muic_data *muic_data)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;

	pr_debug("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (muic_data->is_muic_ready == false) {
		pr_info("%s:%s MUIC is not ready, just return\n", MUIC_DEV_NAME,
				__func__);
		return;
	}

	if (switch_data->charger_cb)
		switch_data->charger_cb(muic_data->attached_dev);
	else
		pr_info("%s:%s charger_cb pointer is NULL, just return\n",
				MUIC_DEV_NAME, __func__);
}

static void max77836_muic_detach_callback_chg(struct max77836_muic_data *muic_data)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;

	pr_debug("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (muic_data->is_muic_ready == false) {
		pr_info("%s:%s MUIC is not ready, just return\n", MUIC_DEV_NAME,
				__func__);
		return;
	}

	if (switch_data->charger_cb)
		switch_data->charger_cb(muic_data->attached_dev);
	else
		pr_info("%s:%s charger_cb pointer is NULL, just return\n",
				MUIC_DEV_NAME, __func__);
}

static void max77836_muic_attach_callback_jig(struct max77836_muic_data *muic_data)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;
	bool jig_attached = false;

	pr_debug("%s:%s\n", MUIC_DEV_NAME, __func__);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		jig_attached = true;
		break;
	default:
		pr_warn("%s:%s function called from wrong context\n",
				MUIC_DEV_NAME, __func__);
		break;
	}

	if (switch_data->set_jig_state_cb)
		switch_data->set_jig_state_cb(jig_attached);
	else
		pr_info("%s:%s set_jig_state_cb is NULL, just return\n",
				MUIC_DEV_NAME, __func__);
}

static void max77836_muic_detach_callback_jig(struct max77836_muic_data *muic_data)
{
	struct sec_switch_data *switch_data = muic_data->switch_data;

	pr_debug("%s:%s\n", MUIC_DEV_NAME, __func__);

	if (switch_data->set_jig_state_cb)
		switch_data->set_jig_state_cb(false);
	else
		pr_info("%s:%s set_jig_state_cb is NULL, just return\n",
				MUIC_DEV_NAME, __func__);
}

static void max77836_muic_force_detach(struct max77836_muic_data *muic_data)
{
	muic_lookup_vps_table(muic_data->attached_dev);

	max77836_muic_detach_callback_jig(muic_data);

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_TA_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_POGO_TA)
	case ATTACHED_DEV_POGO_TA_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_TA */
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		max77836_muic_detach_callback_chg(muic_data);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_POGO_USB)
	case ATTACHED_DEV_POGO_USB_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_USB */
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		max77836_muic_detach_callback_chg(muic_data);
		max77836_muic_detach_callback_usb(muic_data);
		break;
	case ATTACHED_DEV_DESKDOCK_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_FACTORY_BUTTON)
	case ATTACHED_DEV_FACTORY_BUTTON:
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		max77836_muic_detach_callback_dock(muic_data);
		break;
#endif /* CONFIG_MUIC_SUPPORT_FACTORY_BUTTON */
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
	case ATTACHED_DEV_UNKNOWN_MUIC:
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
	case ATTACHED_DEV_NONE_MUIC:
		break;
	default:
		pr_info("%s:%s invalid device type(%d)\n", MUIC_DEV_NAME,
				__func__, muic_data->attached_dev);
		muic_data->attached_dev = ATTACHED_DEV_NONE_MUIC;
		break;
	}

	return;
}

static int max77836_muic_logically_detach(struct max77836_muic_data *muic_data,
						enum muic_attached_dev new_dev)
{
	bool force_path_open = true;
	int ret = 0;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
	case ATTACHED_DEV_TA_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_FACTORY_BUTTON)
	case ATTACHED_DEV_FACTORY_BUTTON:
#endif /* CONFIG_MUIC_SUPPORT_FACTORY_BUTTON */
#if defined(CONFIG_MUIC_SUPPORT_POGO_TA)
	case ATTACHED_DEV_POGO_TA_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_TA */
#if defined(CONFIG_MUIC_SUPPORT_POGO_USB)
	case ATTACHED_DEV_POGO_USB_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_USB */
		break;
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_VB_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		if (new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC)
			force_path_open = false;
		break;
	case ATTACHED_DEV_UNKNOWN_MUIC:
		if ((new_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) || (new_dev == ATTACHED_DEV_JIG_UART_ON_MUIC))
			force_path_open = false;
		break;
	case ATTACHED_DEV_NONE_MUIC:
		goto out;
	default:
		pr_warn("%s:%s try to attach without logically detach\n",
				MUIC_DEV_NAME, __func__);
		goto out;
	}

	pr_debug("%s:%s attached(%d)!=new(%d), assume detach\n", MUIC_DEV_NAME,
			__func__, muic_data->attached_dev, new_dev);

	max77836_muic_force_detach(muic_data);

	if (force_path_open)
		ret = com_to_open(muic_data);

out:
	return ret;
}

static int max77836_muic_enable_accdet(struct max77836_muic_data *muic_data)
{
	int ret = 0;

	ret = write_max77836_ctrl2_reg(muic_data, CTRL2_ACCDET_MASK,
			(1 << CTRL2_ACCDET_SHIFT));
	if (ret)
		pr_err("%s: fail to update reg(%d)\n", __func__, ret);

	return ret;
}

static int max77836_muic_handle_detach(struct max77836_muic_data *muic_data)
{
	int ret = 0;

	/* Enable Factory Accessory Detection State Machine */
	if (muic_data->adcen)
		max77836_muic_enable_accdet(muic_data);

	max77836_muic_force_detach(muic_data);

	ret = com_to_open(muic_data);

	return ret;
}

static int max77836_muic_handle_attach(struct max77836_muic_data *muic_data,
					enum muic_attached_dev new_dev)
{
	int ret = 0;

	if (new_dev == muic_data->attached_dev) {
		pr_info("%s:%s Duplicated(%d), just ignore\n", MUIC_DEV_NAME,
				__func__, muic_data->attached_dev);
		return ret;
	}

	ret = max77836_muic_logically_detach(muic_data, new_dev);
	if (ret)
		pr_warn("%s:%s fail to logically detach(%d)\n", MUIC_DEV_NAME,
				__func__, ret);

	if (muic_data->mfd_pdata->set_gpio_pogo_cb) {
		/* VBATT <-> VBUS voltage switching, along to new_dev. */
		ret = muic_data->mfd_pdata->set_gpio_pogo_cb(new_dev);
		if (ret)
			pr_warn("%s:%s fail to VBATT <-> VBUS voltage switching(%d)\n",
					MUIC_DEV_NAME, __func__, ret);
	}

	switch (new_dev) {
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
	case ATTACHED_DEV_JIG_UART_ON_MUIC:
		ret = write_vps_regs(muic_data, new_dev);
		muic_data->attached_dev = new_dev;
		max77836_muic_attach_callback_jig(muic_data);
		break;
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
	case ATTACHED_DEV_TA_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_POGO_TA)
	case ATTACHED_DEV_POGO_TA_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_TA */
		ret = write_vps_regs(muic_data, new_dev);
		muic_data->attached_dev = new_dev;
		max77836_muic_attach_callback_jig(muic_data);
		max77836_muic_attach_callback_chg(muic_data);
		break;
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_POGO_USB)
	case ATTACHED_DEV_POGO_USB_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_USB */
		ret = write_vps_regs(muic_data, new_dev);
		muic_data->attached_dev = new_dev;
		max77836_muic_attach_callback_usb(muic_data);
		max77836_muic_attach_callback_chg(muic_data);
		break;
#if defined(CONFIG_MUIC_SUPPORT_FACTORY_BUTTON)
	case ATTACHED_DEV_FACTORY_BUTTON:
		ret = write_vps_regs(muic_data, new_dev);
		muic_data->attached_dev = new_dev;
		max77836_muic_attach_callback_dock(muic_data, MUIC_DOCK_FACTORY);
		break;
#endif /* CONFIG_MUIC_SUPPORT_FACTORY_BUTTON */
	default:
		pr_warn("%s:%s unsupported dev(%d)\n", MUIC_DEV_NAME, __func__,
				new_dev);
		break;
	}

	return ret;
}

static void max77836_muic_detect_dev(struct max77836_muic_data *muic_data, int irq)
{
	struct i2c_client *i2c = muic_data->i2c;
	const struct max77836_muic_vps_data *tmp_vps;
	enum muic_attached_dev new_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	int intr = MUIC_INTR_DETACH;
	u8 status[2], ctrl2, ctrl3;
	u8 adcerr, adclow, adc, vbvolt, chgdetrun, chgtyp, adcen;
	int ret;
	int i;

	ret = max77836_bulk_read(i2c, MAX77836_MUIC_REG_STATUS1, 2, status);
	if (ret) {
		pr_err("%s:%s fail to read STAT (%d)\n", MUIC_DEV_NAME,	__func__, ret);
		return;
	}

	ret = max77836_read_reg(muic_data->i2c, MAX77836_REG_CONTROL2, &ctrl2);
	if (ret) {
		pr_err("%s:%s fail to read CTRL2 (%d)\n", MUIC_DEV_NAME, __func__, ret);
		return;
	}

	ret = max77836_read_reg(muic_data->i2c, MAX77836_MUIC_REG_CONTROL3, &ctrl3);
	if (ret) {
		pr_err("%s:%s fail to read CTRL3 (%d)\n", MUIC_DEV_NAME, __func__, ret);
		return;
	}

	pr_info("%s:%s STATUS1:0x%02x, 2:0x%02x, ctrl2:0x%02x\n", MUIC_DEV_NAME,
		__func__, status[0], status[1], ctrl2);

	adcerr = status[0] & STATUS1_ADCERR_MASK;
	adclow = status[0] & STATUS1_ADCLOW_MASK;
	adc = status[0] & STATUS1_ADC_MASK;
	vbvolt = status[1] & STATUS2_VBVOLT_MASK;
	chgdetrun = status[1] & STATUS2_CHGDETRUN_MASK;
	chgtyp = status[1] & STATUS2_CHGTYP_MASK;
	adcen = ctrl2 & CTRL2_ADCEN_MASK;

	pr_debug("%s:%s adcerr:0x%x adclow:0x%x adc:0x%x vb:0x%x chgdetrun:0x%x"
			" chgtyp:0x%x adcen:0x%x\n", MUIC_DEV_NAME, __func__, adcerr,
			adclow, adc, vbvolt, chgdetrun, chgtyp, adcen);

	/* Workaround for Factory mode.
	 * Abandon adc interrupt of approximately +-100K range
	 * if previous cable status was JIG UART BOOT OFF.
	 */
	if (muic_data->attached_dev == ATTACHED_DEV_JIG_UART_OFF_MUIC) {
		if ((adc == (ADC_JIG_UART_OFF + 1)) ||
				(adc == (ADC_JIG_UART_OFF - 1))) {
			pr_warn("%s:%s abandon ADC\n", MUIC_DEV_NAME, __func__);
			return;
		}
	}

	for (i = 0; i < ARRAY_SIZE(muic_vps_table); i++) {
		tmp_vps = &(muic_vps_table[i]);

		if (tmp_vps->adcerr != adcerr)
			continue;

		if (tmp_vps->adclow != adclow)
			continue;

		if (tmp_vps->adc != adc)
			continue;

		if (tmp_vps->vbvolt != vbvolt)
			continue;

		if (tmp_vps->chgdetrun != chgdetrun)
			continue;

		if (tmp_vps->chgtyp != chgtyp)
			continue;

		pr_info("%s:%s vps table match found at i(%d), %s\n",
				MUIC_DEV_NAME, __func__, i, tmp_vps->vps_name);

		new_dev = tmp_vps->attached_dev;

		intr = MUIC_INTR_ATTACH;
		break;
	}

	if (intr == MUIC_INTR_ATTACH) {
		pr_info("%s:%s ATTACHED\n", MUIC_DEV_NAME, __func__);
		max77836_muic_handle_attach(muic_data, new_dev);
	} else {
		pr_info("%s:%s DETACHED\n", MUIC_DEV_NAME, __func__);
		max77836_muic_handle_detach(muic_data);
	}
	return;
}

static irqreturn_t max77836_muic_irq(int irq, void *data)
{
	struct max77836_muic_data *muic_data = data;
	pr_info("%s:%s irq:%d\n", MUIC_DEV_NAME, __func__, irq);

	mutex_lock(&muic_data->muic_mutex);
	if (muic_data->is_muic_ready == true)
		max77836_muic_detect_dev(muic_data, irq);
	else
		pr_info("%s:%s MUIC is not ready, just return\n", MUIC_DEV_NAME,
				__func__);
	mutex_unlock(&muic_data->muic_mutex);

	return IRQ_HANDLED;
}

#define REQUEST_IRQ(_irq, _name)					\
do {									\
	ret = request_threaded_irq(_irq, NULL, max77836_muic_irq,	\
				IRQF_NO_SUSPEND, _name, muic_data);	\
	if (ret < 0)							\
		pr_err("%s:%s Failed to request IRQ #%d: %d\n",		\
				MUIC_DEV_NAME, __func__, _irq, ret);	\
} while (0)

static int max77836_muic_irq_init(struct max77836_muic_data *muic_data)
{
	u8 intmask_val[3];
	int ret;

	REQUEST_IRQ(muic_data->irq_adcerr, "muic-adcerr");
	REQUEST_IRQ(muic_data->irq_adc, "muic-adc");
	REQUEST_IRQ(muic_data->irq_chgtyp, "muic-chgtyp");
	REQUEST_IRQ(muic_data->irq_vbvolt, "muic-vbvolt");

	pr_info("%s:%s irq_num: adc(%d), adcerr(%d), chgtyp(%d), vbvolt(%d)\n",
			MUIC_DEV_NAME, __func__,
			muic_data->irq_adc, muic_data->irq_adcerr,
			muic_data->irq_chgtyp, muic_data->irq_vbvolt);

	max77836_bulk_read(muic_data->i2c, MAX77836_REG_INTMASK1, 3, intmask_val);
	dev_info(muic_data->dev, "%s: intmask 1:0x%x, 2:0x%x, 3:0x%x\n",
		__func__, intmask_val[0], intmask_val[1], intmask_val[2]);

	return 0;
}

#define CHECK_GPIO(_gpio, _name)					\
do {									\
	if (!_gpio) {							\
		dev_err(&pdev->dev, _name " GPIO defined as 0 !\n");	\
		WARN_ON(!_gpio);					\
		ret = -EIO;						\
		goto err_kfree;						\
	}								\
} while (0)

static void max77836_muic_init_detect(struct work_struct *work)
{
	struct max77836_muic_data *muic_data = container_of(work,
			struct max77836_muic_data, init_work.work);

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	mutex_lock(&muic_data->muic_mutex);
	muic_data->is_muic_ready = true;

	max77836_muic_detect_dev(muic_data, -1);
	mutex_unlock(&muic_data->muic_mutex);
}

static void max77836_muic_usb_detect(struct work_struct *work)
{
	struct max77836_muic_data *muic_data = container_of(work,
			struct max77836_muic_data, usb_work.work);

	pr_info("%s: attached_dev : 0x%x %s\n", MUIC_DEV_NAME,muic_data->attached_dev,__func__);

	mutex_lock(&muic_data->muic_mutex);
	muic_data->is_usb_ready = true;

	switch (muic_data->attached_dev) {
	case ATTACHED_DEV_USB_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_POGO_USB)
	case ATTACHED_DEV_POGO_USB_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_USB */
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
		max77836_muic_attach_callback_usb(muic_data);
		break;
	default:
		break;
	}
	mutex_unlock(&muic_data->muic_mutex);
}

static int max77836_muic_probe(struct platform_device *pdev)
{
	struct max77836_dev *max77836 = dev_get_drvdata(pdev->dev.parent);
	struct max77836_platform_data *mfd_pdata = dev_get_platdata(max77836->dev);
	struct max77836_muic_data *muic_data;
	u8 ctrl2 = 0;
	int ret = 0;

	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);

	muic_data = kzalloc(sizeof(struct max77836_muic_data), GFP_KERNEL);
	if (!muic_data) {
		pr_err("%s: failed to allocate driver data\n", __func__);
		ret = -ENOMEM;
		goto err_return;
	}

	if (!mfd_pdata) {
		pr_err("%s: failed to get max77836 mfd platform data\n", __func__);
		ret = -ENOMEM;
		goto err_kfree;
	}

	muic_data->dev = &pdev->dev;
	mutex_init(&muic_data->muic_mutex);
	muic_data->i2c = max77836->i2c;
	muic_data->mfd_pdata = mfd_pdata;
	muic_data->irq_adcerr = mfd_pdata->irq_base + MAX77836_IRQ_INT1_ADCERR;
	muic_data->irq_adc = mfd_pdata->irq_base + MAX77836_IRQ_INT1_ADC;
	muic_data->irq_chgtyp = mfd_pdata->irq_base + MAX77836_IRQ_INT2_CHGTYP;
	muic_data->irq_vbvolt = mfd_pdata->irq_base + MAX77836_IRQ_INT2_VBVOLT;
	muic_data->switch_data = &sec_switch_data;
	muic_data->attached_dev = ATTACHED_DEV_UNKNOWN_MUIC;
	muic_data->is_usb_ready = false;
	muic_data->is_muic_ready = false;

	platform_set_drvdata(pdev, muic_data);

	/* Set ADC debounce time: 25ms */
	max77836_muic_set_adcdbset(muic_data, 2);

	/* Set Charger-Detection Check Time: 625ms */
	max77836_muic_set_dchktm(muic_data, 1);

	ret = max77836_read_reg(muic_data->i2c, MAX77836_REG_CONTROL2, &ctrl2);
	if (ret) {
		pr_err("%s:%s fail to read CTRL2 (%d)\n", MUIC_DEV_NAME, __func__, ret);
		goto fail;
	}

	pr_info("%s:%s: CTRL2:0x%x\n", MUIC_DEV_NAME, __func__, ctrl2);

	muic_data->adcen = (ctrl2 & CTRL2_ADCEN_MASK) >> CTRL2_ADCEN_SHIFT;
	/* If disable ADCEn from bootloader, disable AccDet also */
	if (!muic_data->adcen) {
		pr_info("%s:%s: Disable AccDet\n", MUIC_DEV_NAME, __func__);
		max77836_update_reg(muic_data->i2c, MAX77836_REG_CONTROL2, 0, CTRL2_ACCDET_MASK);
	}

	ret = max77836_read_reg(muic_data->i2c, MAX77836_REG_CONTROL2, &ctrl2);
	if (ret) {
		pr_err("%s:%s fail to read CTRL2 (%d)\n", MUIC_DEV_NAME, __func__, ret);
		goto fail;
	}

	pr_info("%s:%s: new CTRL2:0x%x\n", MUIC_DEV_NAME, __func__, ctrl2);

	/* create sysfs group */
	ret = sysfs_create_group(&switch_device->kobj, &max77836_muic_group);
	if (ret) {
		pr_err("%s: failed to create max77836 muic attribute group\n",
				__func__);
		goto fail;
	}
	dev_set_drvdata(switch_device, muic_data);

	if (muic_data->switch_data->init_cb)
		muic_data->switch_data->init_cb();

	ret = max77836_muic_irq_init(muic_data);
	if (ret < 0) {
		dev_err(&pdev->dev, "Failed to initialize MUIC irq:%d\n", ret);
		goto fail_init_irq;
	}

	/* initial cable detection */
	INIT_DELAYED_WORK(&muic_data->init_work, max77836_muic_init_detect);
	schedule_delayed_work(&muic_data->init_work,
			msecs_to_jiffies(3000));

	INIT_DELAYED_WORK(&muic_data->usb_work, max77836_muic_usb_detect);
	schedule_delayed_work(&muic_data->usb_work, msecs_to_jiffies(17000));

	return 0;

fail_init_irq:
	if (muic_data->irq_adcerr)
		free_irq(muic_data->irq_adcerr, NULL);
	if (muic_data->irq_adc)
		free_irq(muic_data->irq_adc, NULL);
	if (muic_data->irq_chgtyp)
		free_irq(muic_data->irq_chgtyp, NULL);
	if (muic_data->irq_vbvolt)
		free_irq(muic_data->irq_vbvolt, NULL);
fail:
	platform_set_drvdata(pdev, NULL);
	mutex_destroy(&muic_data->muic_mutex);
err_kfree:
	kfree(muic_data);
err_return:
	return ret;
}

static int max77836_muic_remove(struct platform_device *pdev)
{
	struct max77836_muic_data *muic_data = platform_get_drvdata(pdev);

	sysfs_remove_group(&switch_device->kobj, &max77836_muic_group);

	if (muic_data) {
		pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
		cancel_delayed_work(&muic_data->init_work);
		cancel_delayed_work(&muic_data->usb_work);

		free_irq(muic_data->irq_adc, muic_data);
		free_irq(muic_data->irq_chgtyp, muic_data);
		free_irq(muic_data->irq_vbvolt, muic_data);
		free_irq(muic_data->irq_adcerr, muic_data);

		mutex_destroy(&muic_data->muic_mutex);
		kfree(muic_data);
	}

	return 0;
}

void max77836_muic_shutdown(struct device *dev)
{
	struct max77836_muic_data *muic_data = dev_get_drvdata(dev);
	struct max77836_dev *max77836;
	int ret;
	u8 val;
	u8 ctrl2 = 0;

	pr_info("%s:%s +\n", MUIC_DEV_NAME, __func__);
	if (!muic_data->i2c) {
		pr_err("%s:%s no muic i2c client\n", MUIC_DEV_NAME, __func__);
		return;
	}

	max77836 = i2c_get_clientdata(muic_data->i2c);
	pr_info("%s:%s max77836->i2c_lock.count.counter=%d\n", MUIC_DEV_NAME,
		__func__, max77836->i2c_lock.count.counter);

	pr_info("%s:%s JIGSet: auto detection\n", MUIC_DEV_NAME, __func__);
	val = (0 << CTRL3_JIGSET_SHIFT) | (0 << CTRL3_BOOTSET_SHIFT);

	ret = max77836_update_reg(muic_data->i2c, MAX77836_MUIC_REG_CONTROL3,
			val, CTRL3_JIGSET_MASK | CTRL3_BOOTSET_MASK);
	if (ret < 0) {
		pr_err("%s:%s fail to update reg\n", MUIC_DEV_NAME, __func__);
		return;
	}

	ret = max77836_read_reg(muic_data->i2c, MAX77836_REG_CONTROL2, &ctrl2);
	if (ret) {
		pr_err("%s:%s fail to read cntl2 reg\n", MUIC_DEV_NAME, __func__);
		return;
	}
	pr_info("%s:%s: ctrl2:0x%x\n", MUIC_DEV_NAME, __func__, ctrl2);
	pr_info("%s:%s -\n", MUIC_DEV_NAME, __func__);
}

static struct platform_driver max77836_muic_driver = {
	.driver		= {
		.name	= MUIC_DEV_NAME,
		.owner	= THIS_MODULE,
		.shutdown = max77836_muic_shutdown,
	},
	.probe		= max77836_muic_probe,
	.remove		= max77836_muic_remove,
};

static int __init max77836_muic_init(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	return platform_driver_register(&max77836_muic_driver);
}
module_init(max77836_muic_init);

static void __exit max77836_muic_exit(void)
{
	pr_info("%s:%s\n", MUIC_DEV_NAME, __func__);
	platform_driver_unregister(&max77836_muic_driver);
}
module_exit(max77836_muic_exit);


MODULE_DESCRIPTION("Maxim MAX77836 MUIC driver");
MODULE_AUTHOR("<sjin.hahn@samsung.com>");
MODULE_LICENSE("GPL");
