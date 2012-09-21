/*
 * Copyright (C) 2012 LGE, Inc.
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

#ifndef __EARJACK_DEBUGGER_H
#define __EARJACK_DEBUGGER_H

struct earjack_debugger_platform_data {
	int gpio_trigger;
	void (*set_uart_console)(int enable);
};

#endif
