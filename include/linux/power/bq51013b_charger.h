/*
* Copyright(c) 2013, LG Electronics Inc. All rights reserved.
*
* This software is licensed under the terms of the GNU General Public
* License version 2, as published by the Free Software Foundation, and
* may be copied, distributed, and modified under those terms.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE. See the
* GNU General Public License for more details.
*
*/

#ifndef __BQ51013B_CHARGER_H__
#define __BQ51013B_CHARGER_H__

struct bq51013b_platform_data {
	unsigned int chg_ctrl_gpio;
	unsigned int wlc_int_gpio;
	int current_ma;
};
#endif
