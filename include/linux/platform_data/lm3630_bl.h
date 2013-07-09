/*
 * Copyright (c) 2013 LGE Inc. All rights reserved.
 *
 * This software is licensed under the terms of the GNU General Public
 * License version 2, as published by the Free Software Foundation, and
 * may be copied, distributed, and modified under those terms.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __LM3630_BL_H
#define __LM3630_BL_H

void lm3630_lcd_backlight_set_level(int level);

struct lm3630_platform_data {
	int en_gpio;
	int boost_ctrl_reg;
	int bank_sel;
	int linear_map;
	int max_current;
	int min_brightness;
	int max_brightness;
	int default_brightness;
	int pwm_enable;
	int blmap_size;
	char *blmap;
};
#endif
