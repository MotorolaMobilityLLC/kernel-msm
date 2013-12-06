/* Copyright (c) 2013, Motorola Mobility, Inc.
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

#include "mipi_mot.h"

int mmi_panel_register_notifier(struct notifier_block *nb)
{
	struct mipi_mot_panel *mot_panel = mipi_mot_get_mot_panel();
	return srcu_notifier_chain_register(
				&mot_panel->panel_notifier_list, nb);
}

int mmi_panel_unregister_notifier(struct notifier_block *nb)
{
	struct mipi_mot_panel *mot_panel = mipi_mot_get_mot_panel();
	return srcu_notifier_chain_unregister(
				&mot_panel->panel_notifier_list, nb);
}

void mmi_panel_notify(unsigned int state, void *data)
{
	struct mipi_mot_panel *mot_panel = mipi_mot_get_mot_panel();
	pr_debug("%s (%d) is called\n", __func__, state);
	if (!mot_panel->mfd->quickdraw_in_progress)
		srcu_notifier_call_chain(&mot_panel->panel_notifier_list, state,
			data);
	else
		pr_debug("%s: skip notifications!\n", __func__);
}
