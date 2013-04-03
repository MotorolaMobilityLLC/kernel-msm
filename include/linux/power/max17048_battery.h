/*
 * Copyright (C) 2013 LGE Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 */

#ifndef __MAX17048_BATTERY_H__
#define __MAX17048_BATTERY_H__

struct max17048_platform_data {
	int rcomp;
	int alert_threshold;
	int alert_gpio;
};
#endif
