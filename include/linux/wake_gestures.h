/*
 * include/linux/wake_gestures.h
 *
 * Copyright (c) 2013-15, Aaron Segaert <asegaert@gmail.com>
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA  02110-1301, USA.
 */

#ifndef _LINUX_WAKE_GESTURES_H
#define _LINUX_WAKE_GESTURES_H

#include <linux/input.h>

extern int s2w_switch;
extern int dt2w_switch;
extern bool gestures_enabled;
bool scr_suspended(void);
void set_vibrate(int value);

#endif	/* _LINUX_WAKE_GESTURES_H */

