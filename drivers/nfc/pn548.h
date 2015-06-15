/*
 * Copyright (C) 2010 Trusted Logic S.A.
 * modifications copyright (C) 2015 NXP B.V.
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

#ifndef _PN548_H_
#define _PN548_H_
#define PN548_MAGIC 0xE9

/*
 * PN548 power control via ioctl
 * PN548_SET_PWR(0): power off
 * PN548_SET_PWR(1): power on
 * PN548_SET_PWR(2): reset and power on with firmware download enabled
 */
#define PN548_SET_PWR _IOW(PN548_MAGIC, 0x01, unsigned int)
#define PN548_CLK_REQ _IOW(PN548_MAGIC, 0x02, unsigned int)

struct pn548_i2c_platform_data {
	int irq_gpio;
	int fwdl_en_gpio;
	int clk_req_gpio;
	int ven_gpio;
};
#endif
