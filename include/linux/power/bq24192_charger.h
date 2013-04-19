/*
* Copyright(c) 2013, LGE Inc. All rights reserved.
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

#ifndef __BQ24192_CHARGER_H__
#define __BQ24192_CHARGER_H__

#define BQ24192_NAME  "bq24192"

struct bq24192_platform_data {
	int stat_gpio;
	int chg_en_n_gpio;
	int int_gpio;
	int chg_current_ma;
	int term_current_ma;
	int wlc_support;

};

#endif
