/*
 * arch/arm/mach-msm/switch-max77836.c
 *
 * c source file supporting MAX77836 MUIC common platform device register
 *
 * Copyright (C) 2010 Samsung Electronics
 * Seung-Jin Hahn <sjin.hahn@samsung.com>
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
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307 USA
 *
 */

#include <linux/platform_device.h>
#include <linux/device.h>
#include <linux/kernel.h>
#include <linux/err.h>
#include <linux/power_supply.h>
#include <linux/module.h>

#include <linux/max77836-muic.h>
#include <linux/muic.h>
#include <linux/platform_data/android_battery.h>

/* switch device header */
#ifdef CONFIG_SWITCH
#include <linux/switch.h>
#endif

#include <linux/usb/ch9.h>
#include <linux/usb/gadget.h>
#include <linux/regulator/consumer.h>

#ifdef CONFIG_SWITCH
static struct switch_dev switch_dock = {
	.name = "dock",
};

static struct switch_dev switch_usb = {
	.name = "usb_cable",
};
#endif

extern void sec_otg_set_vbus_state(int);

extern struct class *sec_class;

struct device *switch_device;

/* charger cable state */
extern bool is_cable_attached;
bool is_jig_attached;

static void muic_init_cb(void)
{
#ifdef CONFIG_SWITCH
	int ret;

	/* for CarDock, DeskDock */
	ret = switch_dev_register(&switch_dock);
	if (ret < 0)
		pr_err("%s:%s Failed to register dock switch(%d)\n",
			MUIC_DEV_NAME, __func__, ret);

	/* for usb client mode */
	ret = switch_dev_register(&switch_usb);
	if (ret < 0)
		pr_err("%s:%s Failed to register usb switch(%d)\n",
			MUIC_DEV_NAME, __func__, ret);
	else
		pr_info("%s:%s: switch_usb switch_dev registered\n",
			MUIC_DEV_NAME, __func__);
#endif
}

/* usb cable call back function */
static void muic_usb_cb(u8 usb_mode)
{
	pr_info("%s:%s MUIC usb_cb:%d\n", MUIC_DEV_NAME, __func__, usb_mode);

	switch (usb_mode) {
	case USB_CABLE_DETACHED:
		pr_info("usb: muic: USB_CABLE_DETACHED(%d)\n", usb_mode);
		sec_otg_set_vbus_state(0);

		#ifdef CONFIG_SWITCH
		switch_set_state(&switch_usb, usb_mode);
		#endif
		break;
	case USB_CABLE_ATTACHED:
		pr_info("usb: muic: USB_CABLE_ATTACHED(%d)\n", usb_mode);
		#ifdef CONFIG_SWITCH
		switch_set_state(&switch_usb, usb_mode);
		#endif

		sec_otg_set_vbus_state(1);
		break;
	default:
		pr_info("usb: muic: invalid mode%d\n", usb_mode);
	}
}
EXPORT_SYMBOL(muic_usb_cb);

int current_cable_type = CHARGE_SOURCE_NONE;
static int muic_charger_cb(enum muic_attached_dev cable_type)
{
	struct power_supply *psy = power_supply_get_by_name("battery");
	union power_supply_propval value;

	pr_info("%s:%s %d\n", MUIC_DEV_NAME, __func__, cable_type);

	switch (cable_type) {
	case ATTACHED_DEV_NONE_MUIC:
	case ATTACHED_DEV_OTG_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_MUIC:
		current_cable_type = CHARGE_SOURCE_NONE;
		break;
	case ATTACHED_DEV_USB_MUIC:
	case ATTACHED_DEV_CDP_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_POGO_USB)
	case ATTACHED_DEV_POGO_USB_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_USB */
	case ATTACHED_DEV_JIG_USB_OFF_MUIC:
	case ATTACHED_DEV_JIG_USB_ON_MUIC:
		current_cable_type = CHARGE_SOURCE_USB;
		break;
	case ATTACHED_DEV_TA_MUIC:
#if defined(CONFIG_MUIC_SUPPORT_POGO_TA)
	case ATTACHED_DEV_POGO_TA_MUIC:
#endif /* CONFIG_MUIC_SUPPORT_POGO_TA */
#if defined(CONFIG_MUIC_ADC_ADD_PLATFORM_DEVICE)
	case ATTACHED_DEV_2A_TA_MUIC:
	case ATTACHED_DEV_UNKNOWN_TA_MUIC:
#endif /* CONFIG_MUIC_ADC_ADD_PLATFORM_DEVICE */
	case ATTACHED_DEV_DESKDOCK_MUIC:
	case ATTACHED_DEV_CARDOCK_MUIC:
	case ATTACHED_DEV_JIG_UART_OFF_VB_MUIC:
		current_cable_type = CHARGE_SOURCE_AC;
		break;
	default:
		pr_err("%s:%s invalid type:%d\n", MUIC_DEV_NAME, __func__, cable_type);
		return -EINVAL;
	}

#if defined(CONFIG_MACH_WATCH)
	tsp_charger_infom(current_cable_type != POWER_SUPPLY_TYPE_BATTERY ? true : false );
#endif

	pr_info("%s: current cable type :%d\n", __func__, current_cable_type);
	if (!psy || !psy->set_property)
		pr_err("%s: fail to get ac psy\n", __func__);
	else {
		value.intval = current_cable_type;
		psy->set_property(psy, POWER_SUPPLY_PROP_ONLINE, &value);
	}
	return 0;
}

static void muic_dock_cb(int type)
{
	pr_info("%s:%s MUIC dock type=%d\n", MUIC_DEV_NAME, __func__, type);
#ifdef CONFIG_SWITCH
	switch_set_state(&switch_dock, type);
#endif
}

int muic_get_jig_state(void)
{
	return is_jig_attached;
}

static void muic_set_jig_state(bool jig_attached)
{
	pr_info("%s:%s jig attach: %d\n", MUIC_DEV_NAME, __func__, jig_attached);
	is_jig_attached = jig_attached;
}

struct sec_switch_data sec_switch_data = {
	.init_cb = muic_init_cb,
	.usb_cb = muic_usb_cb,
	.charger_cb = muic_charger_cb,
	.dock_cb = muic_dock_cb,
	.set_jig_state_cb = muic_set_jig_state,
};

static ssize_t switch_show_vbus(struct device *dev,
				      struct device_attribute *attr, char *buf)
{
	int i;
	struct regulator *regulator;

	regulator = regulator_get(NULL, "safeout1");
	if (IS_ERR(regulator)) {
		pr_warn("%s: fail to get regulator\n", __func__);
		return sprintf(buf, "UNKNOWN\n");
	}
	if (regulator_is_enabled(regulator))
		i = sprintf(buf, "VBUS is enabled\n");
	else
		i = sprintf(buf, "VBUS is disabled\n");
	regulator_put(regulator);

	return i;
}

static ssize_t switch_store_vbus(struct device *dev,
				       struct device_attribute *attr,
				       const char *buf, size_t count)
{
	int enable;
	int ret;
	struct regulator *regulator;

	if (!strncmp(buf, "0", 1))
		enable = 0;
	else if (!strncmp(buf, "1", 1))
		enable = 1;
	else {
		pr_warn("%s: Wrong command\n", __func__);
		return count;
	}

//	sec_otg_set_vbus_state(enable);
	pr_info("%s: enable=%d\n", __func__, enable);

	regulator = regulator_get(NULL, "safeout1");
	if (IS_ERR(regulator)) {
		pr_warn("%s: fail to get regulator\n", __func__);
		return count;
	}

	if (enable) {
		if (!regulator_is_enabled(regulator))
			ret=regulator_enable(regulator);
	} else {
		if (regulator_is_enabled(regulator))
			ret=regulator_force_disable(regulator);
	}
	regulator_put(regulator);

	return count;
}

DEVICE_ATTR(ctl_vbus, 0664, switch_show_vbus,
	    switch_store_vbus);

static int __init sec_switch_init(void)
{
	int ret = 0;
	switch_device = device_create(sec_class, NULL, 0, NULL, "switch");
	if (IS_ERR(switch_device)) {
		pr_err("%s:%s Failed to create device(switch)!\n",
				MUIC_DEV_NAME, __func__);
		return -ENODEV;
	}

	ret = device_create_file(switch_device, &dev_attr_ctl_vbus);
	if (ret)
		pr_err("%s:%s= Failed to create device file(ctl_vbus)!\n",
				__FILE__, __func__);

	return ret;
};

device_initcall(sec_switch_init);
