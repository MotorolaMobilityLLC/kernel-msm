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

#include <linux/panel_notifier.h>

static BLOCKING_NOTIFIER_HEAD(panel_notifier_list);

int panel_notify(unsigned int event, void *data)
{
	return blocking_notifier_call_chain(&panel_notifier_list, event, data);
}
EXPORT_SYMBOL_GPL(panel_notify);

int panel_register_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_register(&panel_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(panel_register_notifier);

int panel_unregister_notifier(struct notifier_block *nb)
{
	return blocking_notifier_chain_unregister(&panel_notifier_list, nb);
}
EXPORT_SYMBOL_GPL(panel_unregister_notifier);

static int touch_state[TOUCH_PANEL_MAX_IDX] = {0};
int touch_set_state(int state, int panel_idx)
{
	int rc = 0;

	if (panel_idx >= TOUCH_PANEL_MAX_IDX)
		rc = -EINVAL;
	else
		touch_state[panel_idx] = state;

	return rc;
}
EXPORT_SYMBOL_GPL(touch_set_state);

int check_touch_state(int *state, int panel_idx)
{
	int rc = 0;

	if (panel_idx >= TOUCH_PANEL_MAX_IDX) rc = -EINVAL;
	else *state = touch_state[panel_idx];

	return rc;
}
EXPORT_SYMBOL_GPL(check_touch_state);
