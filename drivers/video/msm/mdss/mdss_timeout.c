/* Copyright (c) 2013, Motorola Mobility LLC
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
#include <linux/dropbox.h>
#include <linux/vmalloc.h>

#include "mdss_timeout.h"
#include "mdss_fb.h"
#include "mdss_mdp.h"
#include "mdss_panel.h"

char *mdss_timeout_data;
u32 mdss_timeout_data_pos;
static int initialized;

void mdss_interrupts_dump(void)
{
	u32 isr, mask;
	isr = MDSS_MDP_REG_READ(MDSS_MDP_REG_INTR_STATUS);
	mask = MDSS_MDP_REG_READ(MDSS_MDP_REG_INTR_EN);
	MDSS_TIMEOUT_LOG("MDSS_MDP_REG_INTR_STATUS: 0x%08X\n", isr);
	MDSS_TIMEOUT_LOG("MDSS_MDP_REG_INTR_EN: 0x%08X\n", mask);
	MDSS_TIMEOUT_LOG("global irqs disabled: %d\n", irqs_disabled());
}

void mdss_dump_ctl(struct mdss_mdp_ctl *ctl)
{
	MDSS_TIMEOUT_LOG("-------- MDP5 CTL DATA ---------\n");
	MDSS_TIMEOUT_LOG("play_cnt=%u\n", ctl->play_cnt);
	MDSS_TIMEOUT_LOG("vsync_cnt=%u\n", ctl->vsync_cnt);
	MDSS_TIMEOUT_LOG("underrun_cnt=%u\n", ctl->underrun_cnt);
	MDSS_TIMEOUT_LOG("------ MDP5 CTL DATA DONE ------\n");

	if (ctl->ctx_dump_fnc) {
		MDSS_TIMEOUT_LOG("-------- MDP5 CTX DATA ---------\n");
		ctl->ctx_dump_fnc(ctl);
		MDSS_TIMEOUT_LOG("------ MDP5 CTX DATA DONE ------\n");
	}
}

void mdss_timeout_dropbox_trigger_callback(void *data)
{
	mdss_timeout_dump((struct msm_fb_data_type *)data, __func__);
}

void mdss_timeout_init(struct msm_fb_data_type *mfd)
{
	if (!initialized) {
		mdss_timeout_data = vzalloc(MDSS_DUMP_SIZE);
		if (mdss_timeout_data) {
			mdss_timeout_data_pos = 0;

			dropbox_register_trigger_callback("mdss_diag_timeout",
				&mdss_timeout_dropbox_trigger_callback, mfd);
			initialized = 1;
		} else
			pr_err("%s: Unable to allocate space for mdss_timeout_data!",
				__func__);
	}
}
EXPORT_SYMBOL(mdss_timeout_init);

void mdss_timeout_dump(struct msm_fb_data_type *mfd, const char *timeout_type)
{
	struct mdss_overlay_private *mdp5_data = mfd_to_mdp5_data(mfd);
	struct mdss_mdp_ctl *ctl = mdp5_data->ctl;

	if (!initialized)
		return;

	mdss_timeout_data_pos = 0;

	MDSS_TIMEOUT_DUMP("%s", timeout_type);

	mdss_interrupts_dump();
	mdss_dump_ctl(ctl);

	dump_stack();

	dropbox_queue_event_text("mdss_diag_timeout", mdss_timeout_data,
		mdss_timeout_data_pos);
}
