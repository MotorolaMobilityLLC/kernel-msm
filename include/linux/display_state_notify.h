/*
 * Copyright (C) 2014 Motorola Mobility LLC.
 *
 * This program is free software; you can redistribute it and/or modify it
 * under the terms of the GNU General Public License version 2 as published by
 * the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful, but WITHOUT
 * ANY WARRANTY; without even the implied warranty of MERCHANTABILITY or
 * FITNESS FOR A PARTICULAR PURPOSE.  See the GNU General Public License for
 * more details.
 *
 * You should have received a copy of the GNU General Public License along with
 * this program.  If not, see <http://www.gnu.org/licenses/>.
 */

#ifndef DISPLAY_STATE_NOTIFY_H
#define DISPLAY_STATE_NOTIFY_H

#ifdef __KERNEL__

enum display_state {		/* display states */
	DISPLAY_STATE_OFF,	/* display is off */
	DISPLAY_STATE_ON,	/* display is on with full power */
	DISPLAY_STATE_LP,	/* display is on with low power */
};

enum display_mode {		/* display modes, used when display is on */
	DISPLAY_MODE_TRANS,	/* display transmissive mode */
	DISPLAY_MODE_REFLT,	/* display reflective mode */
	DISPLAY_MODE_TRFLT,	/* display transflective mode */
};

union display_state_event {
	unsigned long event;
	struct {
		unsigned short state;	/* display state */
		unsigned short mode;	/* display mode */
	} disp;
};

#define GEN_DISP_EVENT(state, mode)	\
	(unsigned long)(((state) & 0xFFFF) | (((mode) & 0xFFFF)<<16))
#define DISP_EVENT_STATE(event)		((event) & 0xFFFF)
#define DISP_EVENT_MODE(event)		(((event) & 0xFFFF0000)>>16)
#define _DISPLAY_EVENT_(state, mode)	\
	GEN_DISP_EVENT(DISPLAY_STATE_##state, DISPLAY_MODE_##mode)
#define INVALID_DISPLAY_EVENT		GEN_DISP_EVENT(0xFFFF, 0xFFFF)

extern void display_state_register_notify(struct notifier_block *nb);
extern void display_state_unregister_notify(struct notifier_block *nb);
extern void display_state_notify_subscriber(union display_state_event disp_state);
#endif /* __KERNEL__ */

#endif /* DISPLAY_STATE_NOTIFY_H */
