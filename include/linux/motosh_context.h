/*
 * Copyright (C) 2016 Motorola, Inc.
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
 */
#ifndef __MOTOSH_CONTEXT_H__
#define __MOTOSH_CONTEXT_H__

#define MOTOSH_CONTEXT_TT_NOHOLD (-1)

extern uint8_t motosh_tabletop_mode(void);
extern uint8_t motosh_tabletop_mode_hold(int millis);

#endif  /* __MOTOSH_CONTEXT_H__ */
