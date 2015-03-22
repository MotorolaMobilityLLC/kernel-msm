/*
 * cyttsp4_proximity.h
 * Cypress TrueTouch(TM) Standard Product V4 Proximity touch reports module.
 * For use with Cypress Txx4xx parts.
 * Supported parts include:
 * TMA4XX
 * TMA1036
 *
 * Copyright (C) 2012 Cypress Semiconductor
 * Copyright (C) 2011 Sony Ericsson Mobile Communications AB.
 *
 * This program is free software; you can redistribute it and/or
 * modify it under the terms of the GNU General Public License
 * version 2, and only version 2, as published by the
 * Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License along
 * with this program; if not, write to the Free Software Foundation, Inc.,
 * 51 Franklin Street, Fifth Floor, Boston, MA 02110-1301, USA.
 *
 * Contact Cypress Semiconductor at www.cypress.com <ttdrivers@cypress.com>
 *
 */

#ifndef _LINUX_CYTTSP4_PROXIMITY_H
#define _LINUX_CYTTSP4_PROXIMITY_H

#include <linux/cyttsp4_mt.h>

#define CYTTSP4_PROXIMITY_NAME "cyttsp4_proximity"

struct cyttsp4_proximity_platform_data {
	struct touch_framework *frmwrk;
	char const *inp_dev_name;
};

#endif /* _LINUX_CYTTSP4_PROXIMITY_H */
