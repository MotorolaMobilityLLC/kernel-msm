/*
 * Copyright (c) 2015, Motorola, Inc. All Rights Reserved.
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


#ifndef __M4SENSORHUB_ADS_H__
#define __M4SENSORHUB_ADS_H__

#include <linux/types.h>

enum {
	MODE_0,
	MODE_1,
	MODE_2,
	MODE_3,
	MODE_4,
	MODE_MAX
};

struct m4sensorhub_ads_data {
	uint32_t seq;
	int ch1_data;
	int ch2_data;
	long long timestamp;
};

#endif /*__M4SENSORHUB_ADS_H__ */
