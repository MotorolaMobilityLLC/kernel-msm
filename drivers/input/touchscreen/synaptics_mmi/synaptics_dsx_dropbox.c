/* Copyright (c) 2014 - 2015, Motorola Mobility LLC. All rights reserved.
*
* This program is free software; you can redistribute it and/or modify
* it under the terms of the GNU General Public License version 2 and
* only version 2 as published by the Free Software Foundation.
*
* This program is distributed in the hope that it will be useful,
* but WITHOUT ANY WARRANTY; without even the implied warranty of
* MERCHANTABILITY or FITNESS FOR A PARTICULAR PURPOSE.  See the
* GNU General Public License for more details.
*/
#include <linux/dropbox.h>
#include <linux/string.h>

#include "synaptics_dsx_dropbox.h"

#define DROPBOX_TOUCH_ISSUE "touch_issue"

static DEFINE_RATELIMIT_STATE(synaptics_dropbox_global_rl, 3600 * HZ, 1);

void synaptics_dropbox_report_event(char *msg, int count)
{
	char dropbox_entry[256];

	snprintf(dropbox_entry, sizeof(dropbox_entry), "%s\ncount: %d",
		msg, count);
	pr_err("%s: touch issue detected[count: %d][%s], reporting dropbox\n",
		__func__, count, msg);
	dropbox_queue_event_text(DROPBOX_TOUCH_ISSUE,
				dropbox_entry, strlen(dropbox_entry));
}

void synaptics_dropbox_report_event_ratelimit(char *msg, int count)
{
	struct ratelimit_state *rs = &synaptics_dropbox_global_rl;

	if (!__ratelimit(rs))
		return;
	synaptics_dropbox_report_event(msg, count);
}
