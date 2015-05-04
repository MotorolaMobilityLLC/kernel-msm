/*
 * Copyright (C) 2013 NXP Semiconductors N.V.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License as published by
 * the Free Software Foundation; either version 2 of the License, or
 * (at your option) any later version.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA  02111-1307  USA
 */

#ifndef _PN547_H_
#define _PN547_H_
#define PN544_MAGIC 0xE9

/*
 * PN547 power control via ioctl
 * PN547_SET_PWR(0): power off
 * PN547_SET_PWR(1): power on
 * PN547_SET_PWR(>1): power on with firmware download enabled
 */
#define PN544_SET_PWR _IOW(PN544_MAGIC, 0x01, unsigned int)
#define PN544_DEVICE_NAME "pn544"

struct pn547_i2c_platform_data {
	int irq_gpio;
	int fwdl_en_gpio;
	int clk_req_gpio;
	int ven_gpio;
};
#endif
