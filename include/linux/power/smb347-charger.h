/*
 * Summit Microelectronics SMB347 Battery Charger Driver
 *
 * Copyright (C) 2011, Intel Corporation
 *
 * Authors: Bruce E. Robertson <bruce.e.robertson@intel.com>
 *          Mika Westerberg <mika.westerberg@linux.intel.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef SMB347_CHARGER_H
#define SMB347_CHARGER_H

#include <linux/types.h>
#include <linux/power_supply.h>
#include <linux/power/battery_id.h>

enum {
	/* use the default compensation method */
	SMB347_SOFT_TEMP_COMPENSATE_DEFAULT = -1,

	SMB347_SOFT_TEMP_COMPENSATE_NONE,
	SMB347_SOFT_TEMP_COMPENSATE_CURRENT,
	SMB347_SOFT_TEMP_COMPENSATE_VOLTAGE,
};

/* Use default factory programmed value for hard/soft temperature limit */
#define SMB347_TEMP_USE_DEFAULT		-273

/*
 * Charging enable can be controlled by software (via i2c) by
 * smb347-charger driver or by EN pin (active low/high).
 */
enum smb347_chg_enable {
	SMB347_CHG_ENABLE_SW,
	SMB347_CHG_ENABLE_PIN_ACTIVE_LOW,
	SMB347_CHG_ENABLE_PIN_ACTIVE_HIGH,
};

/*
 * Driving VBUS can be controlled by software (via i2c) by pin or by
 * hardware ID pin autodetection. If set to %SMB347_OTG_CONTROL_DISABLED
 * the functionality is disabled.
 *
 * %SMB347_OTG_CONTROL_DISABLED - don't use OTG at all
 * %SMB347_OTG_CONTROL_PIN - use ID pin to detect when VBUS should be
 *			     driven and raise VBUS automatically
 * %SMB347_OTG_CONTROL_AUTO - Use auto-OTG and RID detection algorithm
 * %SMB347_OTG_CONTROL_SW - enable OTG VBUS via register when we receive an
 *			    OTG event from transceiver driver
 * %SMB347_OTG_CONTROL_SW_PIN - enable OTG VBUS by switching to pin control
 *				mode when OTG event is received
 * %SMB347_OTG_CONTROL_SW_AUTO - enable OTG VBUS by switching to auto-OTG
 *				 mode when OTG event is received
 */
enum smb347_otg_control {
	SMB347_OTG_CONTROL_DISABLED,
	SMB347_OTG_CONTROL_PIN,
	SMB347_OTG_CONTROL_AUTO,
	SMB347_OTG_CONTROL_SW,
	SMB347_OTG_CONTROL_SW_PIN,
	SMB347_OTG_CONTROL_SW_AUTO,
};

/**
 * struct smb347_charger_platform_data - platform data for SMB347 charger
 * @battery_info: Information about the battery
 * @termination_current: current (in uA) used to determine when the
 *			 charging cycle terminates
 * @use_mains: AC/DC input can be used
 * @use_usb: USB input can be used
 * @irq_gpio: GPIO number used for interrupts (%-1 if not used)
 * @enable_control: how charging enable/disable is controlled
 *		    (driver/pin controls)
 * @otg_control: how OTG VBUS is controlled
 *
 * @use_main, @use_usb, and @otg_control are means to enable/disable
 * hardware support for these. This is useful when we want to have for
 * example OTG charging controlled via OTG transceiver driver and not by
 * the SMB347 hardware.
 *
 * If zero value is given in any of the current and voltage values, the
 * factory programmed default will be used. For soft/hard temperature
 * values, pass in %SMB347_TEMP_USE_DEFAULT instead.
 */
#define  MAXSMB34x_CONFIG_REG		20
#define  MAXSMB347_CONFIG_DATA_SIZE	(MAXSMB34x_CONFIG_REG*2)

struct smb347_charger_platform_data {
	struct power_supply_info battery_info;
	bool		use_mains;
	bool		use_usb;
	bool		show_battery;
	bool		is_valid_battery;
	int		irq_gpio;
	unsigned int	termination_current;
	enum smb347_chg_enable enable_control;
	enum smb347_otg_control otg_control;
	/*
	 * One time initialized configuration
	 * register map[offset, value].
	 */
	u16	char_config_regs[MAXSMB347_CONFIG_DATA_SIZE];

	char **supplied_to;
	size_t num_supplicants;
	size_t num_throttle_states;
	unsigned long supported_cables;
	struct power_supply_throttle *throttle_states;
	struct ps_batt_chg_prof *chg_profile;
	bool	detect_chg;
	bool	use_regulator;
	int	gpio_mux;
};

#ifdef CONFIG_CHARGER_SMB347
extern int smb347_get_charging_status(void);
extern int smb347_enable_charger(void);
extern int smb347_disable_charger(void);
extern int smb34x_get_bat_health(void);
#else
static int smb347_get_charging_status(void)
{
	return 0;
}
static int smb347_enable_charger(void)
{
	return 0;
}
static int smb347_disable_charger(void)
{
	return 0;
}
int smb34x_get_bat_health(void)
{
	return 0;
}
#endif /* SMB347_CHARGER_H */
#endif
