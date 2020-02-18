/* Copyright (c) 2016, Motorola Mobility, LLC.
 *
 * This program is free software; you can redistribute it and/or modify
 * it under the terms of the GNU General Public License version 2 and
 * only version 2 as published by the Free Software Foundation.
 *
 * This program is distributed in the hope that it will be useful,
 * but WITHOUT ANY WARRANTY; without even the implied warranty of
 * MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
 * GNU General Public License for more details.
 *
 */

#ifndef __PANEL_NOTIFIER_H__
#define __PANEL_NOTIFIER_H__

#include <linux/notifier.h>

enum panel_event {
	PANEL_EVENT_PRE_DISPLAY_OFF,
	PANEL_EVENT_PRE_DISPLAY_ON,
	PANEL_EVENT_DISPLAY_OFF,
	PANEL_EVENT_DISPLAY_ON,
};

#ifdef CONFIG_PANEL_NOTIFICATIONS

#define MAX_PANEL_IDX 2
enum touch_panel_id {
	TOUCH_PANEL_IDX_PRIMARY = 0,
	TOUCH_PANEL_MAX_IDX,
};

int panel_register_notifier(struct notifier_block *nb);
int panel_unregister_notifier(struct notifier_block *nb);
int panel_notify(unsigned int event, void *data);
int touch_set_state(int state, int panel_idx);
int check_touch_state(int *state, int panel_idx);

#else

#include <linux/errno.h>
static inline int panel_register_notifier(struct notifier_block *nb)
{
	return -ENOSYS;
}
static inline int panel_unregister_notifier(struct notifier_block *nb)
{
	return -ENOSYS;
}
static inline int panel_notify(unsigned int event, void *data)
{
	return -ENOSYS;
}
static inline int touch_set_state(int state, int panel_idx);
{
	return -ENOSYS;
}
static inline int check_touch_state(int *state, int panel_idx);
{
	return -ENOSYS;
}


#endif
#endif
