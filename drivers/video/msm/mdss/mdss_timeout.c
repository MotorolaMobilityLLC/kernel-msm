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

static void (*mdss_mdp_dump_func)(void *data);
static void *mdss_mdp_dump_data;

void mdss_timeout_dropbox_trigger_callback(void *data)
{
	mdss_timeout_dump(__func__);
}

void mdss_timeout_init(void (*dump_func)(void *data), void *data)
{
	if (!initialized) {
		mdss_mdp_dump_func = dump_func;
		mdss_mdp_dump_data = data;
		mdss_timeout_data = vzalloc(MDSS_DUMP_SIZE);
		if (mdss_timeout_data) {
			mdss_timeout_data_pos = 0;

			dropbox_register_trigger_callback("mdss_diag_timeout",
				&mdss_timeout_dropbox_trigger_callback, NULL);
			initialized = 1;
		} else
			pr_err("%s: Unable to allocate space for mdss_timeout_data!",
				__func__);
	}
}
EXPORT_SYMBOL(mdss_timeout_init);

void mdss_timeout_dump(const char *timeout_type)
{
	if (!initialized)
		return;

	mdss_timeout_data_pos = 0;

	MDSS_TIMEOUT_DUMP("%s", timeout_type);

	if (mdss_mdp_dump_func)
		mdss_mdp_dump_func(mdss_mdp_dump_data);

	dump_stack();

	dropbox_queue_event_text("mdss_diag_timeout", mdss_timeout_data,
		mdss_timeout_data_pos);
}
