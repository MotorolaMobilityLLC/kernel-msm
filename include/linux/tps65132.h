/*
 * Copyright (C) 2010-2012 Motorola, Inc.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 as
 * published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 * You should have received a copy of the GNU General Public License
 * along with this program; if not, write to the Free Software
 * Foundation, Inc., 59 Temple Place, Suite 330, Boston, MA
 * 02111-1307, USA
 */

#ifndef __TPS65132_H__
#define __TPS65132_H__

#ifdef __KERNEL__
#define TPS65132_NAME "tps65132"
struct tps65132_platform_data {
	int disp_v1_en;
	int disp_v3_en;
};
#endif /* __KERNEL__ */


#endif  /* __TPS65132_H__ */

