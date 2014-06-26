
/*
 * intel_mdf_battery.h - Intel Medfield MSIC Internal charger and Battery Driver
 *
 * Copyright (C) 2011 Intel Corporation
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; version 2 of the License.
 *
 * This program is distributed in the hope that it will be useful, but
 * WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.	See the GNU
 * General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA.
 *
 * ~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~~
 * Author: Ramakrishna Pallala <ramakrishna.pallala@intel.com>
 */

#ifndef __INTEL_MDF_BATTERY_H_
#define __INTEL_MDF_BATTERY_H_

extern void  mfld_umip_read_termination_current(u32 *term_curr);
#ifdef CONFIG_BATTERY_INTEL_MDF
extern int intel_msic_is_current_sense_enabled(void);
extern int intel_msic_check_battery_present(void);
extern int intel_msic_check_battery_health(void);
extern int intel_msic_check_battery_status(void);
extern int intel_msic_get_battery_pack_temp(int *val);
extern int intel_msic_save_config_data(const char *name, void *data, int len);
extern int intel_msic_restore_config_data(const char *name, void *data,
					  int len);
extern bool intel_msic_is_capacity_shutdown_en(void);
extern bool intel_msic_is_volt_shutdown_en(void);
extern bool intel_msic_is_lowbatt_shutdown_en(void);
extern int intel_msic_get_vsys_min(void);
#else
static int intel_msic_is_current_sense_enabled(void)
{
	return 0;
}
static int intel_msic_check_battery_present(void)
{
	return 0;
}
static int intel_msic_check_battery_health(void)
{
	return 0;
}
static int intel_msic_check_battery_status(void)
{
	return 0;
}
static int intel_msic_get_battery_pack_temp(int *val)
{
	return 0;
}
static int intel_msic_save_config_data(const char *name, void *data, int len)
{
	return 0;
}
static int intel_msic_restore_config_data(const char *name, void *data,
					  int len)
{
	return 0;
}

static bool intel_msic_is_capacity_shutdown_en(void)
{
	return false;
}
static bool intel_msic_is_volt_shutdown_en(void)
{
	return false;
}
static bool intel_msic_is_lowbatt_shutdown_en(void)
{
	return false;
}
static int intel_msic_get_vsys_min(void)
{
	return 0;
}
#endif

#endif /* __INTEL_MDF_BATTERY_H_ */
