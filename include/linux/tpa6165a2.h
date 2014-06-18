/*
 * Copyright (C) 2012 Motorola Mobility, Inc.
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

#ifndef __TPA6165A2_H__
#define __TPA6165A2_H__

struct tpa6165a2_platform_data {
	int irq_gpio;
	int alwayson_micbias;
	int jack_detect_config;
	int jack_detect_config_tty;
};

#endif  /* __TPA6165A2_H__ */
