/*
 * Copyright (C) 2013 Motorola Mobility, LLC
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

#include <mach/mmi_panel_notifier.h>

static BLOCKING_NOTIFIER_HEAD(mmi_panel_notifier_list);

static int mmi_panel_notifier_cb(struct notifier_block *nb,
		unsigned long event, void *ignore)
{
	struct mmi_notifier *mmi =
		container_of(nb, struct mmi_notifier, mmi_nb);

	switch (event) {
	case MMI_PANEL_EVENT_PRE_DISPLAY_OFF:
		if (mmi && mmi->pre_display_off)
			mmi->pre_display_off(mmi->dev);
		break;
	case MMI_PANEL_EVENT_PRE_DISPLAY_ON:
		if (mmi && mmi->pre_display_on)
			mmi->pre_display_on(mmi->dev);
		break;
	case MMI_PANEL_EVENT_DISPLAY_OFF:
		if (mmi && mmi->display_off)
			mmi->display_off(mmi->dev);
		break;
	case MMI_PANEL_EVENT_DISPLAY_ON:
		if (mmi && mmi->display_on)
			mmi->display_on(mmi->dev);
		break;
	default:
		return -EINVAL;
	}

	return 0;
}

int mmi_panel_register_notifier(struct mmi_notifier *mmi)
{
	struct notifier_block *nb = &(mmi->mmi_nb);
	nb->notifier_call = mmi_panel_notifier_cb;
	return blocking_notifier_chain_register(&mmi_panel_notifier_list, nb);
}

int mmi_panel_unregister_notifier(struct mmi_notifier *mmi)
{
	struct notifier_block *nb = &(mmi->mmi_nb);
	return blocking_notifier_chain_unregister(&mmi_panel_notifier_list, nb);
}

void mmi_panel_notify(unsigned int state, void *data)
{
	blocking_notifier_call_chain(&mmi_panel_notifier_list, state, data);
}
