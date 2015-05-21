/**
 * \file
 *
 * \authors   Joshua Gleaton <joshua.gleaton@treadlighttech.com>
 *
 * \brief     Linux kernel module input events listing.
 *
 *            Input events sent from sensor /dev/input/event<N> devices
 *            to userspace
 *
 * \copyright (C) 2013-2014 EM Microelectronic – US, Inc.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License as
 * published by the Free Software Foundation; either version 2 of
 * the License, or (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA 02111-1307 USA
 */

#ifndef __EM718X_INPUT_EVENTS_H__
#define __EM718X_INPUT_EVENTS_H__

#include <linux/input.h>

#define INPUT_EVENT_TYPE	EV_MSC

/** event code sent when a 32-bit axis value changes */
#define INPUT_EVENT_X		MSC_SERIAL
#define INPUT_EVENT_Y		MSC_PULSELED
#define INPUT_EVENT_Z		MSC_GESTURE
#define INPUT_EVENT_W		MSC_RAW

/** event code sent when a 16-bit packed axes value changes */
#define INPUT_EVENT_XY		MSC_SERIAL
#define INPUT_EVENT_ZW		MSC_PULSELED

/** event code sent when a timestamp changes */
#define INPUT_EVENT_TIME_MSB	MSC_SCAN
#define INPUT_EVENT_TIME_LSB	MSC_MAX


#endif
