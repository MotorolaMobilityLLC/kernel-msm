/*
 * Copyright (C) 2014 Motorola Mobility LLC.
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

#ifndef __FSA8500_H__
#define __FSA8500_H__

struct fsa8500_regs {
	u8 reg;
	u8 value;
};
struct fsa8500_platform_data {
	int irq_gpio;
	int alwayson_micbias;
	int init_regs_num;
	struct fsa8500_regs *init_regs;
};

#endif  /* __FSA8500_H__ */
