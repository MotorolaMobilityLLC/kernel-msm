/*
 * tusb320.h, TUSB320 USB TYPE-C Controller device driver
 *
 * Copyright (C) 2015 Longcheer Co.Ltd
 * Author: Kun Pang <pangkun@longcheer.net>
 *
 *  This program is free software; you can redistribute  it and/or modify it
 *  under  the terms of  the GNU General  Public License as published by the
 *  Free Software Foundation;  either version 2 of the  License, or (at your
 *  option) any later version.
 *
 */

#ifndef __TUSB320_H__
#define __TUSB320_H__

#define TUSB320_ATTACH	1
#define TUSB320_DETACH	0

enum tusb320_type {
	TUSB320_TYPE_NONE = 0,
	TUSB320_TYPE_AUDIO,
	TUSB320_TYPE_POWER_ACC,
	TUSB320_TYPE_DEBUG_DFP,
	TUSB320_TYPE_DEBUG_UFP
};

enum tusb320_bc_lvl {
	TUSB320_BC_LVL_RA = 0,
	TUSB320_BC_LVL_USB,
	TUSB320_BC_LVL_1P5,
	TUSB320_BC_LVL_3A
};

#endif

