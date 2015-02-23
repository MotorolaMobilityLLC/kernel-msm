/*
 * Copyright (c) 2012-2014, Motorola, Inc. All Rights Reserved.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program.  If not, see <http://www.gnu.org/licenses/>.
 *
 */

#ifndef __SSV_GPS_H__
#define __SSV_GPS_H__

#include <linux/platform_device.h>
#include <linux/spi/spi.h>

extern char ssv_gps_debug;

#define SSV_GPS_DRIVER_NAME "ssv_gps"

#define KDEBUG(i, format, s...)     \
	do {\
		if (ssv_gps_debug >= i) \
			pr_crit(format, ##s);   \
	} while (0)

enum ssv_gps_debug_level {
	SSV_GPS_NODEBUG = 0x0,
	SSV_GPS_CRITICAL,
	SSV_GPS_ERROR,
	SSV_GPS_WARNING,
	SSV_GPS_NOTICE,
	SSV_GPS_INFO,
	SSV_GPS_DEBUG,
	SSV_GPS_VERBOSE_DEBUG
};

enum ssv_gps_ioctl_command {
	SSV_GPS_ENABLE_REGULATOR = 0x0,
	SSV_GPS_DISABLE_REGULATOR
};

struct ssv_control
{
	int boot_select_gpio;
	int irq_gpio;
	int on_off_gpio;
	int reset_gpio;
	int host_wakeup_gpio;
	int spi_bus;
	int spi_freq;
	int chip_sel;

	struct regulator *supply;
	int regulator_enabled;
};

#endif
